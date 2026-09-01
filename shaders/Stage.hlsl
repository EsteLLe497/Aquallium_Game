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
    //    20 tank rock, 21 waterline emitter, 22 upper water surface,
    //    23 arch bubble, 24 Watatsumi bubble
    // y: simulation time
    // z: material alpha
    float4 gStageSurfaceParameters;
};

Texture2D<float4> gStageRefractionScene : register(t8);
SamplerState gStageRefractionSampler : register(s3);

float StageArchBubbleSurfaceWave(
    float2 surfacePosition,
    float time,
    out float2 gradient)
{
    // Match the three authored diffuser banks at X=8/24/40 m. The closest
    // left/right plume becomes a physical-looking radial wave source.
    const float bankIndex = clamp(
        round((surfacePosition.x - 8.0) / 16.0),
        0.0,
        2.0);
    const float bankX = 8.0 + bankIndex * 16.0;
    const float plumeMagnitude = 4.85 + bankIndex * 0.12;
    const float plumeZ = surfacePosition.y < 0.0
        ? -plumeMagnitude
        : plumeMagnitude;
    const float2 delta =
        surfacePosition - float2(bankX, plumeZ);
    // Distance-squared rings avoid sqrt/exp in the receiver pixel shader.
    // A squared polynomial envelope retains a smooth finite disturbance zone.
    const float radiusSquared = dot(delta, delta);
    const float falloff = saturate(1.0 - radiusSquared * 0.034);
    const float envelope = falloff * falloff;
    const float phase =
        radiusSquared * 0.58 - time * 1.55 + bankIndex * 0.83;
    const float amplitude = 0.082;
    const float wave = sin(phase) * amplitude * envelope;
    const float envelopeDerivative = falloff > 0.0
        ? -0.068 * falloff
        : 0.0;
    const float derivativeByRadiusSquared = amplitude * (
        cos(phase) * 0.58 * envelope +
        sin(phase) * envelopeDerivative);
    gradient = 2.0 * delta * derivativeByRadiusSquared;
    return wave;
}

float StageWatatsumiAerationWave(
    float2 surfacePosition,
    float time,
    out float2 gradient)
{
    // Three real geometry bubble columns and the surface use the same source
    // coordinates. This keeps the broad disturbance attached to aeration
    // instead of sliding as an unrelated normal-map animation.
    const float2 sources[3] = {
        float2(11.2, -8.4),
        float2(14.9, 0.7),
        float2(12.0, 8.1)
    };
    float wave = 0.0;
    gradient = 0.0;
    [unroll]
    for (int sourceIndex = 0; sourceIndex < 3; ++sourceIndex)
    {
        const float2 delta = surfacePosition - sources[sourceIndex];
        const float radiusSquared = dot(delta, delta);
        const float falloff = saturate(1.0 - radiusSquared * 0.020);
        const float envelope = falloff * falloff;
        const float phase = radiusSquared * 0.36 - time * 1.18 +
            sourceIndex * 1.73;
        const float amplitude = 0.055;
        wave += sin(phase) * amplitude * envelope;
        const float envelopeDerivative = falloff > 0.0
            ? -0.040 * falloff
            : 0.0;
        const float derivativeByRadiusSquared = amplitude * (
            cos(phase) * 0.36 * envelope +
            sin(phase) * envelopeDerivative);
        gradient += 2.0 * delta * derivativeByRadiusSquared;
    }
    return wave;
}

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
        const bool archWaterSurface =
            gStageSurfaceParameters.x < 10.5;
        const float waveScale = archWaterSurface ? 1.34 : 1.18;
        float waterHeight = (
            sin(phaseA) * 0.140 +
            sin(phaseB) * 0.090 +
            sin(phaseC) * 0.035 +
            sin(phaseD) * 0.018) * waveScale;
        float derivativeX = (
            cos(phaseA) * 0.140 * 0.48 +
            cos(phaseB) * 0.090 * -0.27 +
            cos(phaseC) * 0.035 * 1.08 +
            cos(phaseD) * 0.018 * 2.15) * waveScale;
        float derivativeZ = (
            cos(phaseA) * 0.140 * 0.31 +
            cos(phaseB) * 0.090 * 0.63 +
            cos(phaseC) * 0.035 * -0.86 +
            cos(phaseD) * 0.018 * 1.72) * waveScale;
        if (archWaterSurface)
        {
            float2 bubbleGradient;
            waterHeight += StageArchBubbleSurfaceWave(
                worldPosition.xz,
                gStageSurfaceParameters.y,
                bubbleGradient);
            derivativeX += bubbleGradient.x;
            derivativeZ += bubbleGradient.y;

            const float capillaryPhase =
                worldPosition.x * 3.60 +
                worldPosition.z * 2.90 +
                gStageSurfaceParameters.y * 1.35;
            waterHeight += sin(capillaryPhase) * 0.026;
            derivativeX += cos(capillaryPhase) * 0.026 * 3.60;
            derivativeZ += cos(capillaryPhase) * 0.026 * 2.90;
        }
        else
        {
            float2 aerationGradient;
            waterHeight += StageWatatsumiAerationWave(
                worldPosition.xz,
                gStageSurfaceParameters.y,
                aerationGradient);
            derivativeX += aerationGradient.x;
            derivativeZ += aerationGradient.y;
        }
        worldPosition.y += waterHeight;
        worldNormal = normalize(float3(
            derivativeX,
            -1.0,
            derivativeZ));
    }
    else if (gStageSurfaceParameters.x > 22.5 &&
             gStageSurfaceParameters.x < 24.5)
    {
        // The low-poly plume remains one batch; a coherent wobble makes the
        // bubbles visibly feed the moving surface without CPU particle work.
        const bool heroTankBubble = gStageSurfaceParameters.x > 23.5;
        const float bubblePhase =
            worldPosition.y * (heroTankBubble ? 2.35 : 3.15) +
            worldPosition.x * 0.91 +
            gStageSurfaceParameters.y * (heroTankBubble ? 1.42 : 1.18);
        const float wobbleScale = heroTankBubble ? 1.45 : 1.0;
        worldPosition.x += sin(bubblePhase) * 0.032 * wobbleScale;
        worldPosition.z += cos(bubblePhase * 0.83) * 0.028 * wobbleScale;
        worldPosition.y += sin(bubblePhase * 0.47) * 0.045 * wobbleScale;
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

float3 StageArchitecturalAlbedo(
    float3 worldPosition,
    float3 normal,
    float3 authoredColor,
    bool metallicTrim)
{
    // A texture-free Art-Deco aquarium palette inspired by the reference:
    // dark terrazzo floor, navy diamond wainscot and pale upper plaster.
    // Everything is analytic, so this adds no textures or draw passes.
    const float floorWeight = smoothstep(0.58, 0.82, normal.y);
    const float ceilingWeight = smoothstep(0.58, 0.82, -normal.y);

    const float floorStoneWave =
        0.965 +
        0.022 * sin(worldPosition.x * 0.47 + worldPosition.z * 0.29) +
        0.013 * sin(worldPosition.x * -0.21 + worldPosition.z * 0.73);
    const float floorAggregate = smoothstep(
        0.91,
        0.985,
        StageHash21(floor(worldPosition.xz * 3.2) + 19.7));
    float3 floorColor =
        float3(0.018, 0.030, 0.047) * floorStoneWave +
        float3(0.010, 0.030, 0.041) * floorAggregate * 0.22;

    const float wallU = abs(normal.x) > abs(normal.z)
        ? worldPosition.z
        : worldPosition.x;
    const float storyY = worldPosition.y < 8.0
        ? max(worldPosition.y, 0.0)
        : max(worldPosition.y - 12.28, 0.0);
    const float wallCloud =
        0.965 +
        0.020 * sin(wallU * 0.31 + worldPosition.y * 0.17) +
        0.010 * sin(wallU * 0.79 - worldPosition.y * 0.29);

    // Rotating the coordinate axes by 45 degrees gives the reference's navy
    // diamond upholstery/tile without relying on a repeating image asset.
    const float diamondScale = 1.48;
    const float2 diamondCoordinates = float2(
        (wallU + storyY) * diamondScale,
        (wallU - storyY) * diamondScale);
    const float2 diamondRepeated = frac(diamondCoordinates);
    const float2 diamondDistance = min(
        diamondRepeated,
        1.0 - diamondRepeated);
    const float diamondInterior = smoothstep(
        0.028,
        0.075,
        min(diamondDistance.x, diamondDistance.y));
    const float diamondVariation = lerp(
        0.92,
        1.06,
        StageHash21(floor(diamondCoordinates) + 31.4));
    const float3 diamondField = lerp(
        float3(0.030, 0.090, 0.125),
        float3(0.018, 0.062, 0.118) * diamondVariation,
        diamondInterior);

    const float3 upperPlaster =
        float3(0.105, 0.118, 0.122) * wallCloud;
    const float lowerWallMask =
        smoothstep(0.16, 0.30, storyY) *
        (1.0 - smoothstep(3.08, 3.28, storyY));
    const float trimMask =
        smoothstep(3.16, 3.22, storyY) *
        (1.0 - smoothstep(3.38, 3.46, storyY));
    float3 wallColor = lerp(upperPlaster, diamondField, lowerWallMask);
    wallColor = lerp(
        wallColor,
        float3(0.030, 0.145, 0.165),
        trimMask * 0.72);

    const float authoredLuminance = dot(
        authoredColor,
        float3(0.2126, 0.7152, 0.0722));
    const float3 authoredTint = authoredLuminance > 0.025
        ? min(authoredColor * 0.34, float3(0.12, 0.16, 0.19))
        : wallColor;
    wallColor = lerp(wallColor, max(wallColor, authoredTint), 0.24);
    floorColor = lerp(floorColor, max(floorColor, authoredTint), 0.22);

    const float3 ceilingColor = float3(0.010, 0.017, 0.025);
    float3 materialColor = lerp(wallColor, floorColor, floorWeight);
    materialColor = lerp(materialColor, ceilingColor, ceilingWeight);
    if (metallicTrim)
    {
        const float brushed = 0.92 + 0.08 * sin(
            worldPosition.x * 5.1 + worldPosition.z * 4.3);
        materialColor = lerp(
            materialColor,
            float3(0.024, 0.058, 0.078) * brushed,
            0.58);
    }
    return materialColor;
}

float3 StageShadeDryArchitecture(
    float3 worldPosition,
    float3 normal,
    float3 viewDirection,
    float3 authoredColor,
    bool metallicTrim)
{
    const float3 materialColor = StageArchitecturalAlbedo(
        worldPosition,
        normal,
        authoredColor,
        metallicTrim);
    const float3 localLight = EvaluateLocalLighting(worldPosition, normal);
    const float3 ambient = EvaluateAmbientLighting(normal);
    const float3 tankBounce = EvaluateTankBounce(worldPosition, normal);
    const float grazing = pow(
        1.0 - saturate(abs(dot(normal, -viewDirection))), 3.0);

    // Stable low-frequency visibility replaces global fill light. The room
    // stays dark, while panel seams, floor modules and silhouettes survive.
    return materialColor * 0.145 +
        ambient * (0.64 + materialColor * 3.2) +
        localLight * (0.46 + materialColor * 2.8) +
        tankBounce * (0.54 + materialColor * 2.4) +
        float3(0.0025, 0.0080, 0.0135) * grazing;
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
            float2(6.20, 2.55);
        const float lightPool =
            exp(-dot(delta, delta) * 1.82) *
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

// The hero pane can cover most of the screen. Re-evaluating the generic
// spotlight projection (normalization + exp) for every water/glass pixel was
// needlessly expensive, so the fixed three-bank hero rig uses a polynomial
// footprint. The light data still comes from the CPU rig; only its falloff is
// approximated here. This keeps palette switching and moving-source support.
float StageHeroTankLightData(
    float3 worldPosition,
    out float3 dominantDirection,
    out float3 dominantColor,
    out float2 dominantSurfacePosition)
{
    float3 lightDirection0 = normalize(gStageLightRefractedAxis[0].xyz);
    float3 lightDirection1 = normalize(gStageLightRefractedAxis[1].xyz);
    float3 lightDirection2 = normalize(gStageLightRefractedAxis[2].xyz);
    const float waterDepth0 = max(
        gStageLightSurfaceOrigin[0].y - worldPosition.y, 0.0);
    const float waterDepth1 = max(
        gStageLightSurfaceOrigin[1].y - worldPosition.y, 0.0);
    const float waterDepth2 = max(
        gStageLightSurfaceOrigin[2].y - worldPosition.y, 0.0);
    const float2 surfacePosition0 = worldPosition.xz -
        lightDirection0.xz * (waterDepth0 / max(-lightDirection0.y, 0.001));
    const float2 surfacePosition1 = worldPosition.xz -
        lightDirection1.xz * (waterDepth1 / max(-lightDirection1.y, 0.001));
    const float2 surfacePosition2 = worldPosition.xz -
        lightDirection2.xz * (waterDepth2 / max(-lightDirection2.y, 0.001));

    const float2 delta0 = (surfacePosition0 -
        gStageLightSurfaceOrigin[0].xz) / float2(12.0, 8.5);
    const float2 delta1 = (surfacePosition1 -
        gStageLightSurfaceOrigin[1].xz) / float2(6.0, 5.0);
    const float2 delta2 = (surfacePosition2 -
        gStageLightSurfaceOrigin[2].xz) / float2(6.0, 5.0);
    float pool0 = saturate(1.0 - dot(delta0, delta0));
    float pool1 = saturate(1.0 - dot(delta1, delta1));
    float pool2 = saturate(1.0 - dot(delta2, delta2));
    pool0 *= pool0 * gStageLightColorStrength[0].w;
    pool1 *= pool1 * gStageLightColorStrength[1].w;
    pool2 *= pool2 * gStageLightColorStrength[2].w;

    dominantDirection = lightDirection0;
    dominantColor = gStageLightColorStrength[0].rgb;
    dominantSurfacePosition = surfacePosition0;
    float illumination = pool0;
    if (pool1 > illumination)
    {
        illumination = pool1;
        dominantDirection = lightDirection1;
        dominantColor = gStageLightColorStrength[1].rgb;
        dominantSurfacePosition = surfacePosition1;
    }
    if (pool2 > illumination)
    {
        illumination = pool2;
        dominantDirection = lightDirection2;
        dominantColor = gStageLightColorStrength[2].rgb;
        dominantSurfacePosition = surfacePosition2;
    }
    return saturate(illumination);
}

float StageHeroTankBroadCaustics(float2 position, float time)
{
    // Two broad travelling ridges are enough through a distant viewing pane.
    // The detailed three-layer pattern is retained on the tunnel receivers.
    const float waveA = abs(sin(
        position.x * 0.58 + position.y * 0.34 + time * 0.22));
    const float waveB = abs(sin(
        position.x * -0.31 + position.y * 0.51 - time * 0.17 +
        sin(position.x * 0.12 + time * 0.11) * 0.48));
    const float ridge = min(waveA, waveB);
    const float broad = saturate(1.0 - ridge / 0.42);
    return broad * broad;
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
        const float viewWaterDistance = min(
            length(cameraToStage) * 0.052,
            1.65);
        const float waterDistance = min(
            (0.24 + depthBelowSurface * 0.16) / facing,
            4.20) + viewWaterDistance;
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
            float3(0.008, 0.125, 0.340),
            float3(0.004, 0.052, 0.205),
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
            float3(0.006, 0.105, 0.410) *
                canopyHeight * surfaceTransmission *
                (0.18 + slowWaterA * 0.075) +
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
        const float2 greenUv = clamp(
            safeUv + refractionOffset,
            float2(0.002, 0.002),
            float2(0.998, 0.998));
        const float3 refractedSample = gStageRefractionScene.Sample(
            gStageRefractionSampler, greenUv).rgb;
        // The old three-fetch RGB split moved the UV by only six percent.
        // Preserve that restrained edge coloration analytically and spend one
        // texture fetch instead of three for every visible glass pixel.
        const float dispersion = saturate(
            length(refractionOffset) * 42.0) * (1.0 - facing);
        const float3 refractedColor = refractedSample *
            (1.0 + float3(0.032, 0.0, -0.032) * dispersion);

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
            float3(0.040, 0.315, 0.980) *
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
        // A long, broken strip above the tunnel is the perceived primary
        // source. It is evaluated on the existing water surface, so no extra
        // emitter geometry or draw pass is required.
        const float sourceSpine = exp(
            -input.worldPosition.z * input.worldPosition.z * 0.115) *
            smoothstep(-2.0, 1.5, input.worldPosition.x) *
            (1.0 - smoothstep(47.0, 50.0, input.worldPosition.x));
        const float spineBreakup = lerp(
            0.68,
            1.0,
            0.5 + 0.5 * sin(
                input.worldPosition.x * 0.19 -
                gStageSurfaceParameters.y * 0.21 +
                sin(input.worldPosition.z * 0.41) * 0.8));
        const float3 transmittedEntryLight =
            surfaceLightColor * entryTransmission *
            (entryCore * 3.15 + entryHalo * 0.34) *
            (0.88 + slowSurfacePulse * 0.12) +
            float3(0.19, 0.72, 1.34) *
                sourceSpine * spineBreakup *
                (0.48 + overheadBank * 0.52) * entryTransmission;
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
            projectedSurfacePosition * 0.20,
            gStageSurfaceParameters.y * 0.96,
            0.38);
        const float receiverStrength = archRock ? 0.165 : 0.410;
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
            lightColor * 1.04 *
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
        // The authored 12.45 m surface is rendered at 10.20 m after the common
        // -2.25 m stage offset. Use that actual world height; the old value
        // made every pixel optically 2.25 m too deep and unnecessarily opaque.
        const float waterDepth = max(10.20 - input.worldPosition.y, 0.0);
        const float3 interfaceNormal =
            dot(normal, -viewDirection) >= 0.0 ? normal : -normal;
        const float facing = saturate(
            dot(-viewDirection, interfaceNormal));
        const float fresnel =
            0.020 + 0.980 * pow(1.0 - facing, 5.0);
        const float2 currentUv = ClipToUv(input.currentClip);
        float3 waterRay = refract(
            viewDirection, interfaceNormal, 1.0 / 1.333);
        if (dot(waterRay, waterRay) < 0.25)
        {
            waterRay = viewDirection;
        }
        const float4 refractedClip = mul(
            float4(input.worldPosition + normalize(waterRay) * 0.42, 1.0),
            gStageViewProjection);
        float2 waterOffset = ClipToUv(refractedClip) - currentUv;
        waterOffset += float2(
            sin(input.worldPosition.y * 1.8 +
                input.worldPosition.z * 0.28 +
                gStageSurfaceParameters.y * 0.22),
            cos(input.worldPosition.y * 2.7 -
                input.worldPosition.z * 0.19 -
                gStageSurfaceParameters.y * 0.17)) * 0.00042;
        waterOffset = clamp(waterOffset, -0.016, 0.016);
        const float3 backgroundColor = gStageSurfaceParameters.w > 0.5
            ? gStageRefractionScene.Sample(
                gStageRefractionSampler,
                clamp(currentUv + waterOffset, 0.002, 0.998)).rgb
            : float3(0.003, 0.040, 0.095);
        const float opticalDistance = min(
            1.25 + waterDepth * 0.30 + (1.0 - facing) * 4.2,
            9.0);
        const float3 transmittance = exp(
            -float3(0.108, 0.038, 0.011) * opticalDistance);
        float3 tankLightDirection;
        float3 tankLightColor;
        float2 tankSurfacePosition;
        const float lightBank = StageHeroTankLightData(
            input.worldPosition,
            tankLightDirection,
            tankLightColor,
            tankSurfacePosition);
        const float broadCaustics = StageHeroTankBroadCaustics(
            tankSurfacePosition,
            gStageSurfaceParameters.y);
        const float3 deepColor = lerp(
            float3(0.002, 0.024, 0.070),
            float3(0.007, 0.135, 0.260),
            saturate(1.0 - waterDepth / 8.2));
        const float upperFade = saturate(
            (input.worldPosition.y - 0.25) / 6.2);
        float shaftCenter = saturate(1.0 - abs(
            input.worldPosition.z / 7.2));
        float shaftLeft = saturate(1.0 - abs(
            (input.worldPosition.z + 8.6) / 3.2));
        float shaftRight = saturate(1.0 - abs(
            (input.worldPosition.z - 8.6) / 3.2));
        shaftCenter *= shaftCenter * upperFade;
        shaftLeft *= shaftLeft * upperFade;
        shaftRight *= shaftRight * upperFade;
        const float slowShaftRipple = 0.84 + 0.16 * sin(
            input.worldPosition.y * 0.46 +
            input.worldPosition.z * 0.31 +
            gStageSurfaceParameters.y * 0.24);
        const float3 inScattering =
            deepColor * (0.52 + lightBank * 0.62) *
                (1.0 - transmittance) +
            tankLightColor * broadCaustics * lightBank *
                exp(-waterDepth * 0.16) * 0.24 +
            float3(0.010, 0.105, 0.245) * fresnel +
            tankLightColor * float3(0.44, 0.75, 1.00) *
                (shaftCenter * 0.52 +
                 shaftLeft * 0.24 +
                 shaftRight * 0.24) * slowShaftRipple;
        finalColor = backgroundColor * transmittance + inScattering;
        // The shader already sampled the scene through the water, so this is
        // a near-replacement blend rather than an opaque blue overlay.
        finalOpacity = saturate(0.70 + fresnel * 0.10);
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 16.5 && surfaceType < 17.5)
    {
        // This huge pane is almost flat. Water already refracted the opaque
        // scene, so a second screen-space refraction only doubled distortion
        // and required another full-resolution GPU copy. Retain the physically
        // important grazing Fresnel, edge thickness and soft source reflection.
        const float3 interfaceNormal =
            dot(normal, -viewDirection) >= 0.0 ? normal : -normal;
        const float facing = saturate(dot(-viewDirection, interfaceNormal));
        const float fresnel = 0.040 + 0.960 * pow(1.0 - facing, 5.0);
        const float verticalEdge = pow(
            saturate(abs(input.worldPosition.z) / 14.62), 10.0);
        float3 glassLightDirection;
        float3 glassLightColor;
        float2 glassSurfacePosition;
        const float glassLightBank = StageHeroTankLightData(
            input.worldPosition,
            glassLightDirection,
            glassLightColor,
            glassSurfacePosition);
        const float lightRipple = 0.82 + 0.18 * sin(
            glassSurfacePosition.x * 0.24 +
            glassSurfacePosition.y * 0.17 +
            gStageSurfaceParameters.y * 0.19);
        const float cleaningVariation = 0.5 + 0.5 * sin(
            input.worldPosition.y * 0.37 +
            input.worldPosition.z * 0.21);
        const float3 reflection =
            float3(0.018, 0.080, 0.145) *
                (fresnel * 0.58 + verticalEdge * 0.22) +
            glassLightColor * glassLightBank *
                (0.020 + fresnel * 0.075) * lightRipple;
        finalColor = reflection * (0.94 + cleaningVariation * 0.06);
        finalOpacity = saturate(
            0.030 + fresnel * 0.24 +
            verticalEdge * 0.050 + glassLightBank * 0.018);
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 17.5 && surfaceType < 21.5)
    {
        const bool isRamp = surfaceType > 18.5 && surfaceType < 19.5;
        const bool isRock = surfaceType > 19.5 && surfaceType < 20.5;
        const bool isEmitter = surfaceType > 20.5;
        if (isEmitter)
        {
            finalColor = gStageLightColorStrength[0].rgb *
                (0.64 + 0.04 * sin(gStageSurfaceParameters.y * 0.42));
        }
        else if (isRock)
        {
            const float broadMottle = 0.5 + 0.5 * sin(
                input.worldPosition.x * 1.1 +
                input.worldPosition.y * 1.7 +
                input.worldPosition.z * 0.8);
            const float fineMottle = 0.5 + 0.5 * sin(
                input.worldPosition.x * 3.7 -
                input.worldPosition.y * 2.9 +
                input.worldPosition.z * 2.3);
            const float mottling = lerp(0.62, 1.30,
                broadMottle * 0.68 + fineMottle * 0.32);
            const float waterDepth = max(10.20 - input.worldPosition.y, 0.0);
            float3 rockLightDirection;
            float3 rockLightColor;
            float2 rockSurfacePosition;
            const float rockLight = StageHeroTankLightData(
                input.worldPosition, rockLightDirection,
                rockLightColor, rockSurfacePosition);
            // The service shell follows the exact rear semi-ellipse. Detect it
            // analytically so it can share the rock batch but avoid the tiled
            // caustic/mottle response that looked like a patterned wallpaper.
            const float2 tankFootprint = float2(
                (input.worldPosition.x - 7.0) / 14.7,
                input.worldPosition.z / 14.5);
            const float rearShell = smoothstep(
                0.94, 0.995, dot(tankFootprint, tankFootprint)) *
                (1.0 - smoothstep(0.28, 0.58, abs(normal.y)));
            if (rearShell > 0.5)
            {
                const float broadNaturalVariation =
                    0.88 +
                    sin(input.worldPosition.z * 0.115 +
                        input.worldPosition.y * 0.075) * 0.075 +
                    sin(input.worldPosition.x * 0.19 -
                        input.worldPosition.y * 0.13) * 0.045;
                const float verticalBlue = saturate(
                    (input.worldPosition.y - 0.35) / 11.85);
                const float3 backdropColor = lerp(
                    float3(0.004, 0.021, 0.050),
                    float3(0.010, 0.072, 0.125),
                    verticalBlue) * broadNaturalVariation;
                finalColor = backdropColor +
                    rockLightColor * rockLight *
                    exp(-waterDepth * 0.19) * 0.045;
            }
            else
            {
                const float caustics = StageHeroTankBroadCaustics(
                    rockSurfacePosition,
                    gStageSurfaceParameters.y);
                const float upwardWetFace = saturate(normal.y * 0.5 + 0.5);
                const float3 boninRock = float3(0.050, 0.122, 0.150) *
                    mottling;
                finalColor = boninRock *
                    (0.22 + diffuse * 0.21 + upwardWetFace * 0.075) +
                    float3(0.003, 0.022, 0.048) *
                        exp(-waterDepth * 0.12) +
                    rockLightColor * caustics * rockLight *
                    exp(-waterDepth * 0.17) * 0.30;
            }
        }
        else
        {
            finalColor = StageShadeDryArchitecture(
                input.worldPosition,
                normal,
                viewDirection,
                gStageBaseColor.rgb,
                isRamp);
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
        float3 surfaceLightDirection;
        float3 surfaceLightColor;
        float2 surfaceLightPosition;
        const float surfaceLightBank = StageHeroTankLightData(
            input.worldPosition,
            surfaceLightDirection,
            surfaceLightColor,
            surfaceLightPosition);
        finalColor = lerp(
            background * float3(0.92, 0.985, 1.02),
            float3(0.025, 0.28, 0.68),
            saturate(0.12 + fresnel * 0.58)) +
            float3(0.28, 0.72, 1.10) * sparkle * 0.40 +
            surfaceLightColor * surfaceLightBank *
                (0.13 + fresnel * 0.17);
        finalOpacity = saturate(0.24 + fresnel * 0.26);
    }
    else if (!preserveAnalyticAquarium &&
        surfaceType > 22.5 && surfaceType < 24.5)
    {
        // Fine air bubbles are one combined low-poly batch. Their transparent
        // interiors stay nearly invisible; only the Fresnel rim and a small
        // overhead glint survive, which reads as a real air/water interface.
        const float3 bubbleNormal =
            dot(normal, -viewDirection) >= 0.0 ? normal : -normal;
        const float facing = saturate(dot(-viewDirection, bubbleNormal));
        const float rim = pow(1.0 - facing, 2.35);
        const float3 lightDirection = normalize(float3(0.08, -1.0, 0.06));
        const float glint = pow(saturate(dot(
            reflect(-lightDirection, bubbleNormal),
            -viewDirection)), 48.0);
        const float film = 0.5 + 0.5 * sin(
            input.worldPosition.y * 13.0 +
            input.worldPosition.x * 5.3 +
            gStageSurfaceParameters.y * 0.55);
        const bool heroTankBubble = surfaceType > 23.5;
        const float3 bubbleLightColor = heroTankBubble
            ? gStageLightColorStrength[0].rgb
            : float3(0.66, 0.92, 1.20);
        finalColor =
            lerp(
                float3(0.045, 0.24, 0.52),
                bubbleLightColor,
                film) * rim * 0.72 +
            bubbleLightColor * glint * 0.88;
        finalOpacity = saturate(
            (heroTankBubble ? 0.008 : 0.012) +
            rim * (heroTankBubble ? 0.16 : 0.20) + glint * 0.30);
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
        const float floorMask =
            (!preserveAnalyticAquarium &&
             normal.y > 0.75 &&
             input.worldPosition.x > -3.2 &&
             input.worldPosition.x < 15.2)
            ? 1.0
            : 0.0;
        finalColor = StageShadeDryArchitecture(
                input.worldPosition,
                normal,
                viewDirection,
                gStageBaseColor.rgb,
                false) +
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
