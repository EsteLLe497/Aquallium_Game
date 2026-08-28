/*==================================================================================================

   [Stage.hlsl]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   ステージモデルのHDR Color、Linear Depth、Motion Vector出力
===================================================================================================*/
cbuffer AquariumLightConstants : register(b1)
{
    row_major float4x4 gStageLightViewProjection[4];
    float4 gStageLightSurfaceOrigin[4];
    float4 gStageLightRefractedAxis[4];
    float4 gStageLightColorStrength[4];
    float gStageCurrentShadowLight;
    float gStageActiveLightCount;
    float2 gStageLightPadding;
};

#include "lighting/LocalLighting.hlsli"

cbuffer StageConstants : register(b2)
{
    row_major float4x4 gStageViewProjection;
    row_major float4x4 gStagePreviousViewProjection;
    float4 gStageBaseColor;
    float4 gStageCameraPosition;
    // x: 0 solid, 1 hero water, 2 jelly cylinder volume, 3 display box,
    //    4 cyan emitter, 5 warm emitter, 6 jelly blue emitter, 7 jelly glass,
    //    8 underwater-arch water, 9 underwater-arch glass,
    //    10 real tank surface, 11 arch floor, 12 arch rock,
    //    13 arch seam, 14 arch rail, 15 arch trim,
    //    16 Watatsumi water, 17 acrylic, 18 architecture, 19 ramp,
    //    20 tank rock, 21 waterline emitter, 22 upper water surface
    // y: simulation time
    // z: material alpha
    float4 gStageSurfaceParameters;
};

Texture2D<float4> gStageRefractionScene : register(t8);
SamplerState gStageRefractionSampler : register(s3);

struct StageVertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct StageVertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 currentClip : TEXCOORD3;
    float4 previousClip : TEXCOORD4;
};

StageVertexOutput VSStage(StageVertexInput input)
{
    StageVertexOutput output;
    float3 worldPosition = input.position;
    float3 worldNormal = input.normal;
    if ((gStageSurfaceParameters.x > 9.5 &&
         gStageSurfaceParameters.x < 10.5) ||
        (gStageSurfaceParameters.x > 21.5 &&
         gStageSurfaceParameters.x < 22.5))
    {
        const float phaseA =
            worldPosition.x * 0.48 +
            worldPosition.z * 0.31 +
            gStageSurfaceParameters.y * 0.55;
        const float phaseB =
            worldPosition.x * -0.27 +
            worldPosition.z * 0.63 -
            gStageSurfaceParameters.y * 0.42;
        const float phaseC =
            worldPosition.x * 1.08 +
            worldPosition.z * -0.86 +
            gStageSurfaceParameters.y * 0.73;
        const float phaseD =
            worldPosition.x * 2.15 +
            worldPosition.z * 1.72 -
            gStageSurfaceParameters.y * 1.08;
        worldPosition.y +=
            sin(phaseA) * 0.140 +
            sin(phaseB) * 0.090 +
            sin(phaseC) * 0.035 +
            sin(phaseD) * 0.018;
        const float derivativeX =
            cos(phaseA) * 0.140 * 0.48 +
            cos(phaseB) * 0.090 * -0.27 +
            cos(phaseC) * 0.035 * 1.08 +
            cos(phaseD) * 0.018 * 2.15;
        const float derivativeZ =
            cos(phaseA) * 0.140 * 0.31 +
            cos(phaseB) * 0.090 * 0.63 +
            cos(phaseC) * 0.035 * -0.86 +
            cos(phaseD) * 0.018 * 1.72;
        worldNormal = normalize(float3(
            derivativeX,
            -1.0,
            derivativeZ));
    }
    output.worldPosition = worldPosition;
    output.normal = worldNormal;
    output.uv = input.uv;
    output.currentClip = mul(
        float4(worldPosition, 1.0),
        gStageViewProjection);
    output.previousClip = mul(
        float4(worldPosition, 1.0),
        gStagePreviousViewProjection);
    output.position = output.currentClip;
    return output;
}

struct StagePixelOutput
{
    float4 color : SV_TARGET0;
    float depth : SV_TARGET1;
    float2 motion : SV_TARGET2;
};

float2 ClipToUv(float4 clipPosition)
{
    const float2 ndc =
        clipPosition.xy / max(clipPosition.w, 0.0001);
    return float2(
        ndc.x * 0.5 + 0.5,
        -ndc.y * 0.5 + 0.5);
}

float StageHash21(float2 value)
{
    value = frac(value * float2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return frac(value.x * value.y);
}

float StageHash31(float3 value)
{
    value = frac(value * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return frac((value.x + value.y) * value.z);
}

float StageArchSuspendedParticle(float3 samplePosition, float layer)
{
    // A sparse analytic particle field avoids transparent particle cards and
    // draw calls. Sampling it at several points along the camera ray gives
    // visible parallax, so the motes read as suspended in water instead of
    // being painted onto the acrylic canopy.
    const float cellSize = 0.42 + layer * 0.10;
    const float3 cellPosition = samplePosition / cellSize;
    const float3 cell = floor(cellPosition);
    const float density = StageHash31(cell + layer * 17.0);
    const float3 localPosition = frac(cellPosition) - 0.5;
    const float distanceToMote = length(localPosition);
    const float mote = smoothstep(0.105, 0.018, distanceToMote);
    const float slowDrift = 0.70 + 0.30 * sin(
        gStageSurfaceParameters.y * (0.34 + layer * 0.07) +
        density * 29.0);
    return mote * step(0.965, density) * slowDrift;
}

float StageTankCaustics(
    float2 position,
    float time,
    float ridgeWidth)
{
    float2 p = position * 3.10;
    float pattern = 0.0;
    [unroll]
    for (int layer = 0; layer < 3; ++layer)
    {
        const float phase = time * (0.34 + layer * 0.07);
        p += float2(
            sin(p.y * 1.7 + phase),
            cos(p.x * 1.45 - phase * 0.83)) * 0.24;
        const float ridge = abs(
            sin(p.x * 2.05 + sin(p.y * 1.55 + phase)));
        pattern += smoothstep(
            ridgeWidth,
            ridgeWidth * 0.12,
            ridge);
        p = mul(float2x2(0.82, -0.57, 0.57, 0.82), p) * 1.16;
    }
    return pattern / 3.0;
}

float StageArchOverheadLightData(
    float3 worldPosition,
    out float3 dominantDirection,
    out float3 dominantColor,
    out float2 dominantSurfacePosition)
{
    float illumination = 0.0;
    dominantDirection = float3(0.0, -1.0, 0.0);
    dominantColor = float3(0.035, 0.245, 0.940);
    dominantSurfacePosition = worldPosition.xz;
    [loop]
    for (uint lightIndex = 0;
         lightIndex < (uint)gStageActiveLightCount;
         ++lightIndex)
    {
        const float3 lightDirection = normalize(
            gStageLightRefractedAxis[lightIndex].xyz);
        const float waterDepth = max(
            gStageLightSurfaceOrigin[lightIndex].y - worldPosition.y,
            0.0);
        const float2 surfacePosition =
            worldPosition.xz -
            lightDirection.xz *
                (waterDepth / max(-lightDirection.y, 0.001));
        const float2 delta =
            (surfacePosition -
             gStageLightSurfaceOrigin[lightIndex].xz) /
            float2(4.35, 2.20);
        const float lightPool =
            exp(-dot(delta, delta) * 2.2) *
            gStageLightColorStrength[lightIndex].w;
        if (lightPool > illumination)
        {
            illumination = lightPool;
            dominantDirection = lightDirection;
            dominantColor =
                gStageLightColorStrength[lightIndex].rgb;
            dominantSurfacePosition = surfacePosition;
        }
    }
    return saturate(illumination);
}

float3 ShadeAuthoredTank(
    float3 worldPosition,
    float3 normal,
    float3 viewDirection)
{
    const float2 causticsUv = abs(normal.y) > 0.55
        ? worldPosition.xz
        : worldPosition.xy;
    const float caustics = StageTankCaustics(
        causticsUv,
        gStageSurfaceParameters.y,
        0.22);
    const float vertical = saturate(
        (worldPosition.y + 2.25) / 5.2);
    const float fresnel = pow(
        1.0 - saturate(dot(-viewDirection, normal)),
        3.5);
    const float slowPulse =
        0.91 + sin(gStageSurfaceParameters.y * 0.42 +
            worldPosition.x * 0.18) * 0.09;
    const float sparkleCell = StageHash21(
        floor(worldPosition.xy * 5.0));
    const float sparkle = step(0.985, sparkleCell) *
        pow(saturate(sin(
            gStageSurfaceParameters.y * 1.4 +
            sparkleCell * 31.0)), 18.0);

    float3 color = lerp(
        float3(0.003, 0.052, 0.125),
        float3(0.010, 0.155, 0.285),
        vertical);
    color *= slowPulse;
    color += float3(0.012, 0.235, 0.58) *
        caustics * (0.22 + vertical * 0.78);
    color += float3(0.004, 0.095, 0.23) * fresnel;
    color += float3(0.06, 0.42, 0.82) * sparkle;
    return color;
}

float3 ShadeSimpleDisplayWater(
    float3 worldPosition,
    float3 normal,
    float3 viewDirection,
    float2 uv,
    float surfaceType,
    out float opacity)
{
    // Small tanks deliberately avoid projected caustics. A slow low-amplitude
    // density variation prevents the water from looking completely static.
    const float fresnel = pow(
        1.0 - saturate(dot(-viewDirection, normal)),
        surfaceType < 2.5 ? 3.2 : 4.5);
    const float vertical = saturate(
        (worldPosition.y + 2.25) / 5.0);
    const float slowVariation =
        sin(worldPosition.y * 1.35 +
            worldPosition.x * 0.22 +
            gStageSurfaceParameters.y * 0.22) * 0.5 + 0.5;

    if (surfaceType < 2.5)
    {
        // The cylinder surface is used as a cheap entry proxy. Four samples
        // integrate scattering and Beer-Lambert extinction along the
        // approximate chord through the water. Cost is proportional only to
        // the visible tank pixels, never to the full screen.
        const float2 horizontalRay = viewDirection.xz;
        const float horizontalLengthSquared = max(
            dot(horizontalRay, horizontalRay),
            0.04);
        const float entryCosine = max(
            -dot(normal.xz, horizontalRay),
            0.0);
        const float approximateRadius = 0.68;
        const float chordLength = min(
            2.0 * approximateRadius *
                entryCosine / horizontalLengthSquared,
            1.55);
        // Six midpoint samples are still cheap on these small screen-space
        // cylinders, but hide the obvious integration bands of four samples.
        const float stepLength = chordLength / 6.0;

        float transmittance = 1.0;
        float3 integratedLight = 0.0;
        [unroll]
        for (int sampleIndex = 0; sampleIndex < 6; ++sampleIndex)
        {
            const float sampleDistance =
                (sampleIndex + 0.5) * stepLength;
            const float3 samplePosition =
                worldPosition + viewDirection * sampleDistance;
            const float sampleHeight = saturate(
                uv.y +
                viewDirection.y * sampleDistance / 3.70);

            // All density terms remain continuous in world space. The former
            // floor/hash cells became visible as rectangular bands at close
            // range, especially after alpha blending.
            const float waveA = sin(
                samplePosition.x * 2.15 +
                samplePosition.y * 2.70 +
                samplePosition.z * 1.35 -
                gStageSurfaceParameters.y * 0.17);
            const float waveB = sin(
                samplePosition.x * -3.40 +
                samplePosition.y * 1.65 +
                samplePosition.z * 3.85 +
                gStageSurfaceParameters.y * 0.11);
            const float waveC = sin(
                (samplePosition.x + samplePosition.z) * 5.10 -
                samplePosition.y * 2.25 -
                gStageSurfaceParameters.y * 0.08);
            const float density = saturate(
                0.52 +
                waveA * 0.095 +
                waveB * 0.055 +
                waveC * 0.025);

            const float bottomLight =
                exp(-sampleHeight * 2.35);
            const float topLight =
                exp(-(1.0 - sampleHeight) * 3.10);
            const float centerFill =
                0.12 +
                0.16 * (1.0 - abs(sampleHeight * 2.0 - 1.0));
            const float3 incidentLight =
                float3(0.012, 0.155, 0.58) *
                    (bottomLight * 1.18 + centerFill) +
                float3(0.025, 0.245, 0.82) *
                    topLight * 0.82;

            integratedLight +=
                transmittance *
                incidentLight *
                density *
                stepLength;
            transmittance *= exp(
                -density * stepLength * 0.46);
        }

        const float opticalOpacity = 1.0 - transmittance;
        opacity = saturate(
            0.035 +
            opticalOpacity * 0.76 +
            fresnel * 0.16);
        return integratedLight * 1.48 +
            lerp(
                float3(0.005, 0.030, 0.060),
                float3(0.012, 0.095, 0.185),
                vertical * 0.55 + slowVariation * 0.08) +
            float3(0.010, 0.100, 0.235) * fresnel;
    }

    opacity = saturate(
        gStageSurfaceParameters.z * 0.42 +
        fresnel * 0.10);
    return lerp(
        float3(0.006, 0.040, 0.065),
        float3(0.018, 0.155, 0.225),
        vertical * 0.52 + slowVariation * 0.06) +
        float3(0.008, 0.070, 0.110) * fresnel;
}

float3 JellyfishFloorBounce(float3 worldPosition)
{
    const float2 centers[7] = {
        float2(-0.2, -3.55), float2(2.0, 3.45),
        float2(4.3, -3.20), float2(6.6, 3.65),
        float2(8.8, -3.45), float2(11.0, 3.25),
        float2(13.0, -3.55)
    };
    float illumination = 0.0;
    [unroll]
    for (int index = 0; index < 7; ++index)
    {
        const float distanceSquared =
            dot(worldPosition.xz - centers[index],
                worldPosition.xz - centers[index]);
        illumination += exp(-distanceSquared * 0.72);
    }
    return float3(0.004, 0.055, 0.14) *
        min(illumination, 1.35);
}

StagePixelOutput PSStage(StageVertexOutput input)
{
    // Temporary authored-stage bridge: preserve the existing analytic
    // aquarium throughout its viewing opening while allowing imported
    // corridor geometry around it. A named _Glass proxy will replace these
    // constants.
    const float3 cameraToStage =
        input.worldPosition - gStageCameraPosition.xyz;
    const float glassIntersectionDistance =
        (-5.10 - gStageCameraPosition.z) /
        (abs(cameraToStage.z) > 0.0001
            ? cameraToStage.z
            : 0.0001);
    const float3 glassIntersection =
        gStageCameraPosition.xyz +
        cameraToStage * glassIntersectionDistance;
    const bool insideAquariumOpening =
        glassIntersectionDistance > 0.0 &&
        abs(glassIntersection.x) < 5.80 &&
        glassIntersection.y > -2.26 &&
        glassIntersection.y < 3.00;
    const bool preserveAnalyticAquarium =
        gStageCameraPosition.w > 0.5;
    if (preserveAnalyticAquarium && insideAquariumOpening)
    {
        discard;
    }

    const float3 normal = normalize(input.normal);
    const float3 keyDirection =
        normalize(float3(-0.24, 0.88, -0.40));
    const float diffuse =
        0.18 + 0.82 * saturate(dot(normal, keyDirection));
    const float upward =
        saturate(normal.y * 0.5 + 0.5);

    const float surfaceType = gStageSurfaceParameters.x;
    const float3 viewDirection = normalize(cameraToStage);
    float3 finalColor;
    float finalOpacity = 1.0;
    if (!preserveAnalyticAquarium && surfaceType > 0.5 && surfaceType < 1.5)
    {
        finalColor = ShadeAuthoredTank(
            input.worldPosition,
            normal,
            viewDirection);
    }
    else if (!preserveAnalyticAquarium && surfaceType > 1.5 && surfaceType < 3.5)
    {
        // Back faces would integrate the same cylinder chord a second time.
        // Keep one entry surface so opacity and cost stay stable.
        if (surfaceType < 2.5 &&
            dot(normal, viewDirection) > -0.001)
        {
            discard;
        }
        finalColor = ShadeSimpleDisplayWater(
            input.worldPosition,
            normal,
            viewDirection,
            input.uv,
            surfaceType,
            finalOpacity);
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 7.5 && surfaceType < 8.5)
    {
        // Approximate the water column between the tunnel and the exhibits.
        // Unlike the old flat-blue overlay, the copied opaque scene is
        // refracted, spectrally attenuated and combined with in-scattering.
        const float3 interfaceNormalBase =
            dot(normal, -viewDirection) >= 0.0 ? normal : -normal;
        // Small capillary ripples live in the pixel shader so the surface can
        // shimmer without requiring an expensive high-density water mesh.
        const float rippleA =
            input.worldPosition.x * 2.45 +
            input.worldPosition.z * 1.65 +
            gStageSurfaceParameters.y * 1.22;
        const float rippleB =
            input.worldPosition.x * -1.72 +
            input.worldPosition.z * 2.85 -
            gStageSurfaceParameters.y * 0.96;
        const float3 interfaceNormal = normalize(
            interfaceNormalBase + float3(
                sin(rippleA) * 0.105 + sin(rippleB) * 0.055,
                0.0,
                cos(rippleA) * 0.105 + cos(rippleB) * 0.055));
        const float facing = max(
            dot(-viewDirection, interfaceNormal),
            0.16);
        const float fresnel = pow(1.0 - saturate(facing), 3.4);
        const float depthProgress = saturate(input.worldPosition.x / 48.0);
        const float slowWaterA =
            sin(input.worldPosition.x * 0.23 +
                input.worldPosition.z * 0.72 -
                gStageSurfaceParameters.y * 0.17) * 0.5 + 0.5;
        const float slowWaterB = sin(
            input.worldPosition.x * -0.37 +
            input.worldPosition.y * 0.81 +
            input.worldPosition.z * 0.29 +
            gStageSurfaceParameters.y * 0.11) * 0.5 + 0.5;

        const float depthBelowSurface = max(
            5.8 - input.worldPosition.y,
            0.0);
        const float waterDistance = min(
            (0.24 + depthBelowSurface * 0.16) / facing,
            4.20);
        const float3 absorptionCoefficient = lerp(
            float3(0.20, 0.070, 0.022),
            float3(0.31, 0.110, 0.035),
            depthProgress);
        const float3 transmittance = exp(
            -absorptionCoefficient * waterDistance);

        const float2 currentUv = ClipToUv(input.currentClip);
        const float distortionWave =
            (slowWaterA - 0.5) * 0.0017 +
            (slowWaterB - 0.5) * 0.0009;
        const float2 distortionDirection = normalize(
            interfaceNormal.zy + float2(0.0001, 0.0001));
        const float2 waterUv = clamp(
            currentUv + distortionDirection * distortionWave,
            float2(0.002, 0.002),
            float2(0.998, 0.998));
        const float3 backgroundColor =
            gStageSurfaceParameters.w > 0.5
            ? gStageRefractionScene.Sample(
                gStageRefractionSampler,
                waterUv).rgb
            : float3(0.002, 0.018, 0.045);

        const float3 waterScatterColor = lerp(
            float3(0.006, 0.092, 0.270),
            float3(0.003, 0.036, 0.165),
            depthProgress);
        const float3 overheadDirection =
            normalize(float3(-0.20, -0.93, 0.29));
        const float forwardScatter = pow(
            saturate(dot(viewDirection, overheadDirection) * 0.5 + 0.5),
            7.0);
        const float routeT = saturate(input.worldPosition.x / 48.0);
        const float localFloor =
            -4.7 * routeT * routeT * (3.0 - 2.0 * routeT);
        const float canopyHeight = saturate(
            (input.worldPosition.y - localFloor - 1.20) / 3.88);
        const float surfaceTransmission = exp(
            -depthBelowSurface * 0.12);
        float3 moonlightDirection;
        float3 moonlightColor;
        float2 moonlightSurfacePosition;
        const float moonlightBank = StageArchOverheadLightData(
            input.worldPosition,
            moonlightDirection,
            moonlightColor,
            moonlightSurfacePosition);
        // Kaiyukan's night presentation is built around occasional broad
        // moonlit areas, not an evenly bright blue tunnel. Keep the pattern
        // deliberately large and slow so it reads as illumination through a
        // moving surface instead of dense caustics stuck to the canopy.
        const float broadMoonWave =
            sin(moonlightSurfacePosition.x * 0.17 +
                moonlightSurfacePosition.y * 0.29 +
                gStageSurfaceParameters.y * 0.16) * 0.5 + 0.5;
        const float crossingMoonWave =
            sin(moonlightSurfacePosition.x * -0.11 +
                moonlightSurfacePosition.y * 0.41 -
                gStageSurfaceParameters.y * 0.12) * 0.5 + 0.5;
        const float moonlightPatch =
            smoothstep(0.30, 0.82, broadMoonWave * crossingMoonWave) *
            moonlightBank *
            (0.22 + canopyHeight * 0.78) *
            surfaceTransmission;
        // Carry a restrained part of each surface pool down the curved shell.
        // The bright origin remains overhead, while the lower arc catches a
        // softer continuation that suggests light travelling through water.
        const float fallingMoonlight =
            moonlightBank * surfaceTransmission *
            (0.18 + canopyHeight * 0.82) *
            (0.42 + broadMoonWave * 0.58);

        const float3 cameraPosition = gStageCameraPosition.xyz;
        const float3 cameraRay = input.worldPosition - cameraPosition;
        const float rayLength = length(cameraRay);
        const float3 rayDirection = cameraRay / max(rayLength, 0.001);
        const float particleNear = StageArchSuspendedParticle(
            cameraPosition + rayDirection * min(rayLength * 0.31, 3.0),
            0.0);
        const float particleMiddle = StageArchSuspendedParticle(
            cameraPosition + rayDirection * min(rayLength * 0.58, 5.5),
            1.0);
        const float particleFar = StageArchSuspendedParticle(
            cameraPosition + rayDirection * min(rayLength * 0.82, 8.0),
            2.0);
        // Motes are only revealed by the broad overhead light. This preserves
        // dark water and prevents a snow-globe look in unlit tunnel sections.
        const float suspendedParticles =
            (particleNear * 0.50 +
             particleMiddle * 0.34 +
             particleFar * 0.20) *
            moonlightBank * (0.18 + moonlightPatch * 0.82);
        const float3 inScattering =
            (1.0 - transmittance) * waterScatterColor *
                (0.56 + forwardScatter * 0.28) +
            float3(0.005, 0.075, 0.330) *
                canopyHeight * surfaceTransmission *
                (0.14 + slowWaterA * 0.065) +
            moonlightColor *
                (moonlightPatch * 0.145 + fallingMoonlight * 0.060);

        finalColor =
            backgroundColor * transmittance +
            inScattering +
            float3(0.004, 0.032, 0.140) * fresnel +
            float3(0.075, 0.310, 0.720) * suspendedParticles;
        // The shader already performs transmission against the copied scene,
        // so use near-replacement blending instead of overlaying blue twice.
        finalOpacity = saturate(0.72 + fresnel * 0.12);
    }
    else if (!preserveAnalyticAquarium &&
        ((surfaceType > 6.5 && surfaceType < 7.5) ||
         (surfaceType > 8.5 && surfaceType < 9.5)))
    {
        const bool archGlass = surfaceType > 8.5;
        // Orient the interface toward the camera because the open cylinder is
        // intentionally double-sided. This keeps refraction stable on both
        // the near and far glass surfaces.
        const float3 interfaceNormal =
            dot(normal, -viewDirection) >= 0.0 ? normal : -normal;
        const float facing =
            saturate(dot(-viewDirection, interfaceNormal));
        const float fresnel =
            0.04 + 0.96 * pow(1.0 - facing, 5.0);

        const float2 currentUv = ClipToUv(input.currentClip);
        const float3 glassRay = refract(
            viewDirection,
            interfaceNormal,
            1.0 / 1.52);
        const float4 refractedClip = mul(
            float4(
                input.worldPosition +
                    glassRay * (archGlass ? 0.16 : 0.24),
                1.0),
            gStageViewProjection);
        float2 refractionOffset =
            ClipToUv(refractedClip) - currentUv;

        // Very small manufacturing/water-pressure waviness breaks the perfect
        // CG cylinder without turning the tank into a heat-haze effect.
        const float microWave =
            sin(input.worldPosition.y * 10.5 +
                input.uv.x * 31.0 +
                gStageSurfaceParameters.y * 0.16) * 0.00055 +
            sin(input.worldPosition.y * 23.0 -
                input.uv.x * 17.0) * 0.00022;
        const float2 distortionDirection = archGlass
            ? normalize(interfaceNormal.zy + float2(0.0001, 0.0001))
            : normalize(interfaceNormal.xz + float2(0.0001, 0.0001));
        refractionOffset += distortionDirection * microWave;
        refractionOffset = clamp(
            refractionOffset,
            archGlass ? float2(-0.014, -0.014) : float2(-0.022, -0.022),
            archGlass ? float2(0.014, 0.014) : float2(0.022, 0.022));

        const float2 safeUv = clamp(
            currentUv,
            float2(0.025, 0.025),
            float2(0.975, 0.975));
        const float2 redUv = clamp(
            safeUv + refractionOffset * 1.06,
            float2(0.002, 0.002),
            float2(0.998, 0.998));
        const float2 greenUv = clamp(
            safeUv + refractionOffset,
            float2(0.002, 0.002),
            float2(0.998, 0.998));
        const float2 blueUv = clamp(
            safeUv + refractionOffset * 0.94,
            float2(0.002, 0.002),
            float2(0.998, 0.998));
        const float3 refractedColor = float3(
            gStageRefractionScene.Sample(
                gStageRefractionSampler, redUv).r,
            gStageRefractionScene.Sample(
                gStageRefractionSampler, greenUv).g,
            gStageRefractionScene.Sample(
                gStageRefractionSampler, blueUv).b);

        const float3 radialNormal = normalize(
            (archGlass
                ? float3(0.0, interfaceNormal.y, interfaceNormal.z)
                : float3(interfaceNormal.x, 0.0, interfaceNormal.z)) +
            float3(0.0001, 0.0001, 0.0001));
        const float reflectionBand = pow(
            saturate(abs(dot(
                radialNormal,
                archGlass
                    ? normalize(float3(0.0, 0.76, 0.65))
                    : normalize(float3(-0.74, 0.0, 0.67))))),
            archGlass ? 28.0 : 54.0);
        const float rimLight =
            pow(1.0 - facing, 3.2);
        const float edgeInHeight = archGlass
            ? pow(saturate(interfaceNormal.y), 7.0) * 0.32
            : pow(saturate(abs(input.uv.y - 0.5) * 2.0), 7.0);
        float3 reflectedLight =
            (archGlass
                ? float3(0.025, 0.22, 0.76)
                : float3(0.08, 0.38, 0.72)) *
                (reflectionBand * 1.10 +
                 rimLight * 0.34 +
                 edgeInHeight * 0.16);
        if (archGlass)
        {
            float3 glassLightDirection;
            float3 glassLightColor;
            float2 glassSurfacePosition;
            const float glassLightBank = StageArchOverheadLightData(
                input.worldPosition,
                glassLightDirection,
                glassLightColor,
                glassSurfacePosition);
            const float overheadFacing = pow(
                saturate(abs(interfaceNormal.y)),
                1.7);
            const float glassLightRipple =
                sin(glassSurfacePosition.x * 0.18 +
                    glassSurfacePosition.y * 0.33 +
                    gStageSurfaceParameters.y * 0.17) * 0.5 + 0.5;
            reflectedLight += glassLightColor * glassLightBank *
                (0.026 + overheadFacing * 0.115) *
                (0.72 + glassLightRipple * 0.28);
        }
        const float3 absorption =
            float3(0.945, 0.978, 0.992);

        finalColor = gStageSurfaceParameters.w > 0.5
            ? lerp(
                refractedColor * absorption,
                reflectedLight,
                saturate(fresnel * 0.68))
            : reflectedLight;
        // The copied scene already supplies the transmitted image. Moderate
        // opacity lets it replace the unwarped background while retaining the
        // transparent water drawn immediately before the glass.
        finalOpacity = archGlass
            ? saturate(
                0.075 + fresnel * 0.30 +
                reflectionBand * 0.070 +
                edgeInHeight * 0.030)
            : saturate(
                0.42 + fresnel * 0.30 +
                reflectionBand * 0.16 +
                edgeInHeight * 0.05);
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 9.5 && surfaceType < 10.5)
    {
        // A real, independently displaced tank surface sits above the tunnel.
        // It contributes refraction and a soft area-light reflection, while
        // caustics are projected onto solid receivers below rather than glued
        // to the acrylic canopy.
        const float3 interfaceNormal =
            dot(normal, -viewDirection) >= 0.0 ? normal : -normal;
        const float facing = saturate(
            dot(-viewDirection, interfaceNormal));
        const float fresnel =
            0.020 + 0.980 * pow(1.0 - facing, 5.0);
        const float2 currentUv = ClipToUv(input.currentClip);
        const float2 surfaceOffset =
            interfaceNormal.xz *
            (0.0022 + (1.0 - facing) * 0.0020);
        const float2 surfaceUv = clamp(
            currentUv + surfaceOffset,
            float2(0.002, 0.002),
            float2(0.998, 0.998));
        const float3 transmittedScene =
            gStageSurfaceParameters.w > 0.5
            ? gStageRefractionScene.Sample(
                gStageRefractionSampler,
                surfaceUv).rgb
            : float3(0.010, 0.080, 0.150);
        float3 surfaceLightDirection;
        float3 surfaceLightColor;
        float2 surfaceProjection;
        const float overheadBank = StageArchOverheadLightData(
            input.worldPosition,
            surfaceLightDirection,
            surfaceLightColor,
            surfaceProjection);
        const float surfaceSpecular = pow(
            saturate(dot(
                reflect(-surfaceLightDirection, interfaceNormal),
                -viewDirection)),
            42.0);
        const float slowSurfacePulse =
            0.90 + sin(
                input.worldPosition.x * 0.34 +
                input.worldPosition.z * 0.27 +
                gStageSurfaceParameters.y * 0.38) * 0.10;
        const float3 reflectedAreaLight =
            float3(0.035, 0.270, 0.860) *
            (0.18 + fresnel * 0.72) * slowSurfacePulse +
            float3(0.30, 0.78, 1.10) * surfaceSpecular * 0.42 +
            surfaceLightColor *
                overheadBank * (0.12 + fresnel * 0.16);
        // Direct light from the fixture above the tank is visible through the
        // underside of the moving surface inside the Snell window. This entry
        // hotspot shares the exact surface point, color and intensity used to
        // launch the refracted volumetric shaft and receiver caustics.
        const float entryCore = pow(overheadBank, 4.20);
        const float entryHalo = pow(overheadBank, 0.82);
        const float entryTransmission = 1.0 - fresnel;
        const float3 transmittedEntryLight =
            surfaceLightColor * entryTransmission *
            (entryCore * 2.30 + entryHalo * 0.22) *
            (0.88 + slowSurfacePulse * 0.12);
        finalColor = lerp(
            transmittedScene * float3(0.90, 0.975, 1.02),
            reflectedAreaLight,
            saturate(0.18 + fresnel * 0.62)) +
            transmittedEntryLight;
        finalOpacity = saturate(0.30 + fresnel * 0.24);
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 10.5 && surfaceType < 12.5)
    {
        const bool archRock = surfaceType > 11.5;
        const float rockWaveX = sin(
            input.worldPosition.x * 1.73 +
            input.worldPosition.y * 2.31 +
            input.worldPosition.z * 0.91);
        const float rockWaveZ = cos(
            input.worldPosition.x * 0.83 -
            input.worldPosition.y * 1.47 +
            input.worldPosition.z * 2.17);
        const float3 receiverNormal = archRock
            ? normalize(normal + float3(
                rockWaveX * 0.16,
                (rockWaveX + rockWaveZ) * 0.045,
                rockWaveZ * 0.16))
            : normal;
        const float waterDepth = max(
            5.8 - input.worldPosition.y,
            0.0);
        const float lightTransmission = exp(-waterDepth * 0.16);
        const float receiverUp = archRock
            ? saturate(receiverNormal.y * 0.65 + 0.35)
            : saturate(receiverNormal.y);
        float3 lightDirection;
        float3 lightColor;
        float2 projectedSurfacePosition;
        const float overheadBank = StageArchOverheadLightData(
            input.worldPosition,
            lightDirection,
            lightColor,
            projectedSurfacePosition);
        const float lightFacing = saturate(
            dot(receiverNormal, -lightDirection));
        // Reuse the hero-tank ridge profile, but enlarge its world-space
        // cells for the long tunnel. Evaluate the pattern at the traced water
        // surface position so it follows the same angled refracted trajectory
        // as the corresponding overhead bank.
        const float caustics = StageTankCaustics(
            projectedSurfacePosition * 0.42,
            gStageSurfaceParameters.y * 0.72,
            0.22);
        const float receiverStrength = archRock ? 0.090 : 0.225;
        const float checker = abs(fmod(
            floor(input.worldPosition.x * 1.35) +
            floor(input.worldPosition.z * 1.35),
            2.0));
        const float rockMottle = saturate(
            0.52 + rockWaveX * 0.24 + rockWaveZ * 0.18);
        const float3 receiverBaseColor = archRock
            ? gStageBaseColor.rgb *
                lerp(0.58, 1.24, rockMottle) *
                lerp(
                    float3(0.70, 0.84, 0.90),
                    float3(0.88, 1.02, 0.96),
                    receiverUp)
            : gStageBaseColor.rgb * lerp(0.62, 1.42, checker);
        const float3 baseLighting = receiverBaseColor *
            (0.055 + diffuse * (archRock ? 0.085 : 0.045));
        finalColor =
            baseLighting +
            float3(0.008, 0.095, 0.390) *
                lightTransmission * receiverUp * 0.34 +
            lightColor * 0.88 *
                caustics * lightTransmission *
                lightFacing * overheadBank * receiverStrength;
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 12.5 && surfaceType < 15.5)
    {
        const bool archRail =
            surfaceType > 13.5 && surfaceType < 14.5;
        const bool archTrim = surfaceType > 14.5;
        const float upwardLight = saturate(normal.y * 0.5 + 0.5);
        const float grazing = pow(
            1.0 - saturate(abs(dot(normal, -viewDirection))),
            3.2);
        const float waterReflection =
            0.5 + 0.5 * sin(
                input.worldPosition.x * 0.42 +
                gStageSurfaceParameters.y * 0.48);
        if (archTrim)
        {
            finalColor =
                float3(0.018, 0.070, 0.115) *
                    lerp(0.72, 1.32, upwardLight) +
                float3(0.025, 0.145, 0.260) *
                    (grazing * 0.46 + waterReflection * 0.035);
        }
        else if (archRail)
        {
            finalColor =
                float3(0.007, 0.024, 0.042) *
                    lerp(0.68, 1.18, upwardLight) +
                float3(0.010, 0.085, 0.175) *
                    (grazing * 0.28 + waterReflection * 0.022);
        }
        else
        {
            finalColor = float3(0.006, 0.055, 0.115) *
                lerp(0.62, 1.20, upwardLight);
        }
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 15.5 && surfaceType < 16.5)
    {
        // One material is shared by the opaque tank backing and transparent
        // front interface.  Water depth is measured from the 6.45 m surface,
        // so upper and lower observations retain the same physical gradient.
        const float waterDepth = max(6.45 - input.worldPosition.y, 0.0);
        const float viewGrazing = pow(
            1.0 - saturate(abs(dot(normal, -viewDirection))), 3.2);
        const float3 transmittance = exp(
            -float3(0.19, 0.064, 0.018) *
            min(1.2 + waterDepth * 0.72, 7.5));
        float3 tankLightDirection;
        float3 tankLightColor;
        float2 tankSurfacePosition;
        const float lightBank = StageArchOverheadLightData(
            input.worldPosition,
            tankLightDirection,
            tankLightColor,
            tankSurfacePosition);
        const float broadCaustics = StageTankCaustics(
            tankSurfacePosition * 0.31,
            gStageSurfaceParameters.y * 0.48,
            0.24);
        const float3 deepColor = lerp(
            float3(0.002, 0.030, 0.085),
            float3(0.008, 0.145, 0.275),
            saturate(1.0 - waterDepth / 6.6));
        const float upperFade = saturate(
            (input.worldPosition.y - 0.25) / 6.2);
        const float shaftA = exp(-pow(
            (input.worldPosition.z + 3.4) / 2.35, 2.0)) *
            upperFade;
        const float shaftB = exp(-pow(
            (input.worldPosition.z - 3.1) / 2.85, 2.0)) *
            upperFade;
        const float slowShaftRipple = 0.84 + 0.16 * sin(
            input.worldPosition.y * 0.46 +
            input.worldPosition.z * 0.31 +
            gStageSurfaceParameters.y * 0.24);
        const float moteCell = StageHash21(floor(
            input.worldPosition.yz * float2(4.0, 3.2)));
        const float revealedMotes = step(0.976, moteCell) *
            (shaftA + shaftB) * upperFade;
        finalColor = deepColor * (1.16 + lightBank * 0.78) +
            tankLightColor * broadCaustics * lightBank *
                exp(-waterDepth * 0.20) * 0.22 +
            float3(0.010, 0.115, 0.255) * viewGrazing +
            float3(0.012, 0.135, 0.34) *
                (shaftA * 0.62 + shaftB * 0.48) * slowShaftRipple +
            float3(0.12, 0.48, 0.92) * revealedMotes * 0.25;
        finalColor = lerp(finalColor, finalColor * transmittance +
            float3(0.004, 0.042, 0.105) * (1.0 - transmittance), 0.62);
        finalOpacity = 0.86;
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 16.5 && surfaceType < 17.5)
    {
        // Thick aquarium acrylic: small Snell refraction at normal incidence,
        // strong Fresnel only toward the edge, and restrained RGB separation.
        const float3 interfaceNormal =
            dot(normal, -viewDirection) >= 0.0 ? normal : -normal;
        const float facing = saturate(dot(-viewDirection, interfaceNormal));
        const float fresnel = 0.040 + 0.960 * pow(1.0 - facing, 5.0);
        const float2 currentUv = ClipToUv(input.currentClip);
        const float3 acrylicRay = refract(
            viewDirection, interfaceNormal, 1.0 / 1.49);
        const float4 refractedClip = mul(
            float4(input.worldPosition + acrylicRay * 0.22, 1.0),
            gStageViewProjection);
        float2 offset = ClipToUv(refractedClip) - currentUv;
        const float pressureWave =
            sin(input.worldPosition.y * 2.1 + input.worldPosition.z * 0.31) *
            0.00032;
        offset += normalize(interfaceNormal.zy + 0.0001) * pressureWave;
        offset = clamp(offset, -0.010, 0.010);
        const float2 safeUv = clamp(currentUv, 0.003, 0.997);
        const float3 refracted = gStageSurfaceParameters.w > 0.5
            ? float3(
                gStageRefractionScene.Sample(
                    gStageRefractionSampler, safeUv + offset * 1.035).r,
                gStageRefractionScene.Sample(
                    gStageRefractionSampler, safeUv + offset).g,
                gStageRefractionScene.Sample(
                    gStageRefractionSampler, safeUv + offset * 0.965).b)
            : float3(0.003, 0.065, 0.145);
        const float verticalEdge = pow(
            saturate(abs(input.worldPosition.z) / 7.35), 10.0);
        const float3 reflection = float3(0.04, 0.28, 0.62) *
            (fresnel * 0.48 + verticalEdge * 0.20);
        finalColor = lerp(
            refracted * float3(0.945, 0.982, 0.995),
            reflection,
            saturate(fresnel * 0.62));
        finalOpacity = saturate(0.10 + fresnel * 0.30 + verticalEdge * 0.04);
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 17.5 && surfaceType < 21.5)
    {
        const bool isRamp = surfaceType > 18.5 && surfaceType < 19.5;
        const bool isRock = surfaceType > 19.5 && surfaceType < 20.5;
        const bool isEmitter = surfaceType > 20.5;
        if (isEmitter)
        {
            finalColor = float3(0.035, 0.48, 1.04) *
                (0.64 + 0.04 * sin(gStageSurfaceParameters.y * 0.42));
        }
        else if (isRock)
        {
            const float mottling = 0.72 + 0.28 * sin(
                input.worldPosition.x * 1.1 +
                input.worldPosition.y * 1.7 +
                input.worldPosition.z * 0.8);
            const float waterDepth = max(6.45 - input.worldPosition.y, 0.0);
            float3 rockLightDirection;
            float3 rockLightColor;
            float2 rockSurfacePosition;
            const float rockLight = StageArchOverheadLightData(
                input.worldPosition, rockLightDirection,
                rockLightColor, rockSurfacePosition);
            const float caustics = StageTankCaustics(
                rockSurfacePosition * 0.30,
                gStageSurfaceParameters.y * 0.48,
                0.24);
            finalColor = gStageBaseColor.rgb * mottling *
                (0.065 + diffuse * 0.075) +
                rockLightColor * caustics * rockLight *
                exp(-waterDepth * 0.19) * 0.18;
        }
        else
        {
            // Dry architecture is nearly black unless an authored local light
            // reaches it. This preserves depth while water remains on its
            // dedicated absorption/caustics/refraction shader branch.
            const float edge = pow(
                1.0 - saturate(abs(dot(normal, -viewDirection))), 3.0);
            const float3 localLight = EvaluateLocalLighting(
                input.worldPosition, normal);
            const float3 ambient = EvaluateAmbientLighting(normal);
            const float3 tankBounce = EvaluateTankBounce(
                input.worldPosition, normal);
            finalColor = gStageBaseColor.rgb * 0.016 +
                ambient + localLight + tankBounce +
                localLight * edge * (isRamp ? 0.055 : 0.025);
            finalColor = ApplyDryAtmosphere(
                finalColor,
                length(input.worldPosition - gStageCameraPosition.xyz));
        }
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 21.5 && surfaceType < 22.5)
    {
        const float3 interfaceNormal =
            dot(normal, -viewDirection) >= 0.0 ? normal : -normal;
        const float facing = saturate(dot(-viewDirection, interfaceNormal));
        const float fresnel = 0.020 + 0.980 * pow(1.0 - facing, 5.0);
        const float2 currentUv = ClipToUv(input.currentClip);
        const float2 offset = interfaceNormal.xz *
            (0.0018 + (1.0 - facing) * 0.0022);
        const float3 background = gStageSurfaceParameters.w > 0.5
            ? gStageRefractionScene.Sample(
                gStageRefractionSampler,
                clamp(currentUv + offset, 0.002, 0.998)).rgb
            : float3(0.006, 0.105, 0.190);
        const float sparkle = pow(saturate(dot(
            reflect(normalize(float3(0.12, -1.0, 0.08)), interfaceNormal),
            -viewDirection)), 54.0);
        finalColor = lerp(
            background * float3(0.92, 0.985, 1.02),
            float3(0.025, 0.28, 0.68),
            saturate(0.12 + fresnel * 0.58)) +
            float3(0.28, 0.72, 1.10) * sparkle * 0.40;
        finalOpacity = saturate(0.24 + fresnel * 0.26);
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 3.5 && surfaceType < 6.5)
    {
        // These strips represent practical light emitted by the exhibits. No
        // general ceiling fixtures are used in the route preview.
        const float emitterPulse = 0.92 +
            sin(gStageSurfaceParameters.y * 0.55 +
                input.worldPosition.x * 0.25) * 0.08;
        const float emitterGain = surfaceType < 4.5
            ? 1.35
            : (surfaceType < 5.5 ? 0.95 : 1.18);
        finalColor = gStageBaseColor.rgb *
            emitterGain *
            emitterPulse;
    }
    else
    {
        const float3 localLight = EvaluateLocalLighting(
            input.worldPosition, normal);
        const float3 ambient = EvaluateAmbientLighting(normal);
        const float3 tankBounce = EvaluateTankBounce(
            input.worldPosition, normal);
        const float floorMask =
            (!preserveAnalyticAquarium &&
             normal.y > 0.75 &&
             input.worldPosition.x > -3.2 &&
             input.worldPosition.x < 15.2)
            ? 1.0
            : 0.0;
        finalColor = gStageBaseColor.rgb * 0.012 +
            ambient + localLight + tankBounce +
            JellyfishFloorBounce(input.worldPosition) * floorMask;
        finalColor = ApplyDryAtmosphere(
            finalColor,
            length(input.worldPosition - gStageCameraPosition.xyz));
    }

    StagePixelOutput output;
    output.color = float4(finalColor, finalOpacity);
    output.depth =
        length(input.worldPosition - gStageCameraPosition.xyz);

    const float2 currentUv = ClipToUv(input.currentClip);
    const float2 previousUv = ClipToUv(input.previousClip);
    const bool previousValid =
        input.previousClip.w > 0.0001 &&
        all(previousUv >= 0.0) &&
        all(previousUv <= 1.0);
    output.motion = previousValid
        ? previousUv - currentUv
        : 0.0;
    return output;
}
