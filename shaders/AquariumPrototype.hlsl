/*==================================================================================================

   [AquariumPrototype.hlsl]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   水面、コースティクス、水中散乱、体積光、Temporal、ガラス合成
===================================================================================================*/
cbuffer FrameConstants : register(b0)
{
    float gTime;
    float gDeltaTime;
    float2 gResolution;

    float gCameraYaw;
    float gCameraPitch;
    float gCausticsStrength;
    float gVolumeStrength;

    float gExposure;
    float gWaterClarity;
    float gAnisotropy;
    float gHistoryWeight;

    float gPreviousCameraYaw;
    float gPreviousCameraPitch;
    float gHistoryValid;
    float gFrameIndex;

    float gViewMode;
    float gPreviousViewMode;
    float gGlassDistortion;
    float gStagePreviewMode;

    float3 gCameraPosition;
    float gUnderwaterArchMode;

    float3 gPreviousCameraPosition;
    float gPreviousCameraPositionPadding;
};

cbuffer ShadowConstants : register(b1)
{
    row_major float4x4 gLightViewProjection[4];
    float4 gLightSurfaceOrigin[4];
    float4 gLightRefractedAxis[4];
    float4 gLightColorStrength[4];
    float gCurrentShadowLight;
    float gActiveLightCount;
    float2 gShadowPadding;
};

Texture3D<float> gNoiseTexture : register(t0);
Texture2D<float4> gSceneColorTexture : register(t1);
Texture2D<float> gSceneDepthTexture : register(t2);
Texture2D<float4> gVolumeTexture : register(t3);
Texture2D<float4> gHistoryTexture : register(t4);
Texture2DArray<float> gShadowMap : register(t5);
Texture2D<float2> gMotionTexture : register(t6);
SamplerState gLinearWrapSampler : register(s0);
SamplerState gLinearClampSampler : register(s1);
SamplerComparisonState gShadowSampler : register(s2);

static const float PI = 3.14159265359;
static const float FAR_DISTANCE = 30.0;

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
    VSOutput output;
    const float2 position = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(position * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = position;
    return output;
}

float3 GetCasterCenter(uint casterId, float time)
{
    if (casterId == 0)
    {
        return float3(-2.52, 2.28, 1.57);
    }
    if (casterId == 1)
    {
        return float3(
            0.30 + sin(time * 0.55) * 0.10,
            2.22 + sin(time * 0.83) * 0.06,
            2.61 + cos(time * 0.42) * 0.05);
    }
    return float3(3.38, 2.30, 3.82);
}

float3 GetCasterScale(uint casterId)
{
    if (casterId == 0)
    {
        return float3(0.15, 0.075, 0.14);
    }
    if (casterId == 1)
    {
        return float3(0.10, 0.060, 0.085);
    }
    return float3(0.14, 0.070, 0.15);
}

float4 VSShadow(float3 position : POSITION, uint instanceId : SV_InstanceID) : SV_POSITION
{
    const float3 worldPosition =
        position * GetCasterScale(instanceId) +
        GetCasterCenter(instanceId, gTime);
    const uint lightIndex = (uint)gCurrentShadowLight;
    return mul(float4(worldPosition, 1.0), gLightViewProjection[lightIndex]);
}

float Hash11(float n)
{
    return frac(sin(n) * 43758.5453123);
}

float Hash31(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float Caustics(float2 uv, float time)
{
    float2 p = uv * 1.45;
    float accumulated = 0.0;

    [unroll]
    for (int layer = 0; layer < 5; ++layer)
    {
        const float fi = (float)layer + 1.0;
        const float phase = time * (0.22 + fi * 0.037);
        const float2 warped = p + float2(
            sin(p.y * (1.7 + fi * 0.11) + phase * 1.3),
            cos(p.x * (1.5 + fi * 0.09) - phase)) * 0.55;
        const float lines = abs(sin(warped.x * 2.1 + sin(warped.y * 1.6 + phase)));
        accumulated += 0.22 / max(0.055, lines);
        p = mul(float2x2(0.82, -0.57, 0.57, 0.82), p) * 1.22 + 0.41;
    }

    accumulated /= 5.0;

    // Keep the same five-layer pattern, but narrow its bright ridges instead
    // of adding more samples. This produces cleaner projected caustic lines
    // without increasing the shader's texture or ray-march cost.
    // Art-directed aquarium caustics: broaden the bright ridges and raise
    // their peak without adding another procedural layer.
    const float ridge = saturate(accumulated * 0.260 - 0.100);
    return pow(ridge, 3.45) * 10.5;
}

float WaterSurfaceHeight(float2 xz)
{
    return 2.65 +
        sin(xz.x * 1.4 + gTime * 0.63) * 0.055 +
        sin(dot(xz, float2(0.7, 1.1)) * 2.2 - gTime * 0.48) * 0.028 +
        sin(xz.y * 1.7 - gTime * 0.54) * 0.045 +
        sin(dot(xz, float2(-1.2, 0.6)) * 2.5 + gTime * 0.39) * 0.022;
}

float3 WaterSurfaceNormal(float3 position)
{
    const float2 xz = position.xz;
    const float waveA = xz.x * 1.4 + gTime * 0.63;
    const float waveB =
        dot(xz, float2(0.7, 1.1)) * 2.2 - gTime * 0.48;
    const float waveC = xz.y * 1.7 - gTime * 0.54;
    const float waveD =
        dot(xz, float2(-1.2, 0.6)) * 2.5 + gTime * 0.39;
    const float derivativeX =
        cos(waveA) * 0.055 * 1.4 +
        cos(waveB) * 0.028 * 0.7 * 2.2 +
        cos(waveD) * 0.022 * -1.2 * 2.5;
    const float derivativeZ =
        cos(waveB) * 0.028 * 1.1 * 2.2 +
        cos(waveC) * 0.045 * 1.7 +
        cos(waveD) * 0.022 * 0.6 * 2.5;
    return normalize(float3(-derivativeX, 1.0, -derivativeZ));
}

float3 GlassMicroNormal(float3 position)
{
    const float lowFrequency =
        sin(position.x * 1.73 + position.y * 0.61) *
        cos(position.y * 2.11 - position.x * 0.37);
    const float fineFrequency =
        sin(position.x * 8.3 + position.y * 6.7) *
        cos(position.y * 7.1 - position.x * 5.9);
    const float strength = 0.0055 * saturate(gGlassDistortion);
    return normalize(float3(
        (lowFrequency * 0.72 + fineFrequency * 0.28) * strength,
        (lowFrequency * -0.48 + fineFrequency * 0.34) * strength,
        -1.0));
}

bool RefractThroughAquariumGlass(
    float3 airOrigin,
    float3 airDirection,
    out float3 waterOrigin,
    out float3 waterDirection,
    out float3 glassTransmission,
    out float glassReflection)
{
    static const float GLASS_FRONT_Z = -5.10;
    static const float GLASS_BACK_Z = -4.94;
    static const float AIR_IOR = 1.0;
    static const float GLASS_IOR = 1.52;
    static const float WATER_IOR = 1.333;

    waterOrigin = airOrigin;
    waterDirection = airDirection;
    glassTransmission = 1.0;
    glassReflection = 0.0;

    if (airDirection.z <= 0.0001)
    {
        return false;
    }

    const float frontDistance =
        (GLASS_FRONT_Z - airOrigin.z) / airDirection.z;
    if (frontDistance <= 0.001)
    {
        return false;
    }

    const float3 frontPosition =
        airOrigin + airDirection * frontDistance;
    if (abs(frontPosition.x) > 5.45 ||
        frontPosition.y < -2.08 ||
        frontPosition.y > 2.84)
    {
        return false;
    }

    const float3 frontNormal = GlassMicroNormal(frontPosition);
    const float3 directionInGlass = refract(
        airDirection,
        frontNormal,
        AIR_IOR / GLASS_IOR);
    if (dot(directionInGlass, directionInGlass) < 0.25 ||
        directionInGlass.z <= 0.0001)
    {
        return false;
    }

    const float glassDistance =
        (GLASS_BACK_Z - frontPosition.z) / directionInGlass.z;
    const float3 backPosition =
        frontPosition + directionInGlass * glassDistance;
    // Both faces are locally parallel. Reusing the front micro-normal avoids
    // a second set of trigonometric evaluations in both scene and volume
    // passes while retaining the pane-thickness parallax.
    const float3 backNormal = frontNormal;
    waterDirection = refract(
        directionInGlass,
        backNormal,
        GLASS_IOR / WATER_IOR);
    if (dot(waterDirection, waterDirection) < 0.25)
    {
        return false;
    }
    waterDirection = normalize(waterDirection);
    waterOrigin = backPosition + waterDirection * 0.004;

    const float airGlassF0 = pow(
        (GLASS_IOR - AIR_IOR) / (GLASS_IOR + AIR_IOR),
        2.0);
    const float glassWaterF0 = pow(
        (GLASS_IOR - WATER_IOR) / (GLASS_IOR + WATER_IOR),
        2.0);
    const float airCosine = saturate(dot(-airDirection, frontNormal));
    const float waterCosine = saturate(dot(-directionInGlass, backNormal));
    const float airGlassFresnel =
        airGlassF0 +
        (1.0 - airGlassF0) * pow(1.0 - airCosine, 5.0);
    const float glassWaterFresnel =
        glassWaterF0 +
        (1.0 - glassWaterF0) * pow(1.0 - waterCosine, 5.0);

    const float3 glassAbsorption = float3(0.095, 0.030, 0.012);
    const float3 thicknessTransmission =
        exp(-glassAbsorption * glassDistance);
    glassTransmission =
        thicknessTransmission *
        (1.0 - airGlassFresnel) *
        (1.0 - glassWaterFresnel);
    glassReflection =
        airGlassFresnel +
        (1.0 - airGlassFresnel) * glassWaterFresnel;
    return true;
}

float IntersectPlane(float3 rayOrigin, float3 rayDirection, float3 planePoint, float3 planeNormal)
{
    const float denominator = dot(rayDirection, planeNormal);
    if (abs(denominator) < 0.0001)
    {
        return FAR_DISTANCE;
    }

    const float distance = dot(planePoint - rayOrigin, planeNormal) / denominator;
    return distance > 0.001 ? distance : FAR_DISTANCE;
}

float IntersectWaterSurface(float3 rayOrigin, float3 rayDirection)
{
    if (rayDirection.y <= 0.0001)
    {
        return FAR_DISTANCE;
    }

    float distance = (2.65 - rayOrigin.y) / rayDirection.y;
    // The displacement is small relative to the tank height, so one
    // predictor/corrector evaluation is visually equivalent to iteration.
    const float3 position = rayOrigin + rayDirection * distance;
    distance =
        (WaterSurfaceHeight(position.xz) - rayOrigin.y) /
        rayDirection.y;
    return distance > 0.001 ? distance : FAR_DISTANCE;
}

struct SceneHit
{
    float distance;
    float3 normal;
    float material;
};

float IntersectEllipsoid(
    float3 rayOrigin,
    float3 rayDirection,
    float3 center,
    float3 radius,
    out float3 normal)
{
    const float3 localOrigin = (rayOrigin - center) / radius;
    const float3 localDirection = rayDirection / radius;
    const float a = dot(localDirection, localDirection);
    const float b = 2.0 * dot(localOrigin, localDirection);
    const float c = dot(localOrigin, localOrigin) - 1.0;
    const float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0)
    {
        normal = 0.0;
        return FAR_DISTANCE;
    }

    const float root = sqrt(discriminant);
    float distance = (-b - root) / (2.0 * a);
    if (distance <= 0.001)
    {
        distance = (-b + root) / (2.0 * a);
    }
    if (distance <= 0.001)
    {
        normal = 0.0;
        return FAR_DISTANCE;
    }

    const float3 localHit = localOrigin + localDirection * distance;
    normal = normalize(localHit / radius);
    return distance;
}

SceneHit TraceRoom(float3 rayOrigin, float3 rayDirection)
{
    SceneHit hit;
    hit.distance = FAR_DISTANCE;
    hit.normal = float3(0, 0, 0);
    hit.material = 0.0;

    float distance = IntersectPlane(rayOrigin, rayDirection, float3(0, -2.25, 0), float3(0, 1, 0));
    float3 position = rayOrigin + rayDirection * distance;
    if (distance < hit.distance && abs(position.x) <= 5.8 && position.z >= -5.0 && position.z <= 7.5)
    {
        hit.distance = distance;
        hit.normal = float3(0, 1, 0);
        hit.material = 1.0;
    }

    distance = IntersectPlane(rayOrigin, rayDirection, float3(0, 0, 7.5), float3(0, 0, -1));
    position = rayOrigin + rayDirection * distance;
    if (distance < hit.distance && abs(position.x) <= 5.8 && position.y >= -2.25 && position.y <= 3.0)
    {
        hit.distance = distance;
        hit.normal = float3(0, 0, -1);
        hit.material = 2.0;
    }

    distance = IntersectPlane(rayOrigin, rayDirection, float3(-5.8, 0, 0), float3(1, 0, 0));
    position = rayOrigin + rayDirection * distance;
    if (distance < hit.distance && position.z >= -5.0 && position.z <= 7.5 && position.y >= -2.25 && position.y <= 3.0)
    {
        hit.distance = distance;
        hit.normal = float3(1, 0, 0);
        hit.material = 2.0;
    }

    distance = IntersectPlane(rayOrigin, rayDirection, float3(5.8, 0, 0), float3(-1, 0, 0));
    position = rayOrigin + rayDirection * distance;
    if (distance < hit.distance && position.z >= -5.0 && position.z <= 7.5 && position.y >= -2.25 && position.y <= 3.0)
    {
        hit.distance = distance;
        hit.normal = float3(-1, 0, 0);
        hit.material = 2.0;
    }

    return hit;
}

float BeamMask(float3 position, float3 lightOrigin, float3 axis, float coneScale)
{
    const float3 fromLight = position - lightOrigin;
    const float depth = dot(fromLight, axis);
    if (depth < 0.0)
    {
        return 0.0;
    }

    const float radialDistance = length(fromLight - axis * depth);
    const float radius = 0.10 + depth * coneScale;
    const float normalizedRadius = radialDistance / max(radius, 0.001);
    const float gaussianCore = exp(-normalizedRadius * normalizedRadius * 2.45);
    const float softBoundary = 1.0 - smoothstep(0.78, 1.18, normalizedRadius);
    return gaussianCore * softBoundary;
}

float PhaseHenyeyGreenstein(float cosTheta, float anisotropy)
{
    const float g = clamp(anisotropy, -0.92, 0.92);
    const float g2 = g * g;
    const float denominator = max(1.0 + g2 - 2.0 * g * cosTheta, 0.0001);
    return (1.0 - g2) / (4.0 * PI * pow(denominator, 1.5));
}

float SampleVolumetricShadow(float3 worldPosition, uint lightIndex)
{
    const float4 lightClip = mul(
        float4(worldPosition, 1.0),
        gLightViewProjection[lightIndex]);
    if (lightClip.w <= 0.0)
    {
        return 1.0;
    }

    const float3 lightNdc = lightClip.xyz / lightClip.w;
    const float2 shadowUv = float2(
        lightNdc.x * 0.5 + 0.5,
        -lightNdc.y * 0.5 + 0.5);
    if (any(shadowUv < 0.0) || any(shadowUv > 1.0) ||
        lightNdc.z <= 0.0 || lightNdc.z >= 1.0)
    {
        return 1.0;
    }

    const float2 shadowTexel = 1.25 / 512.0;
    float visibility = 0.0;
    visibility += gShadowMap.SampleCmpLevelZero(
        gShadowSampler,
        float3(shadowUv + float2(-shadowTexel.x, -shadowTexel.y), lightIndex),
        lightNdc.z - 0.0015);
    visibility += gShadowMap.SampleCmpLevelZero(
        gShadowSampler,
        float3(shadowUv + float2( shadowTexel.x, -shadowTexel.y), lightIndex),
        lightNdc.z - 0.0015);
    visibility += gShadowMap.SampleCmpLevelZero(
        gShadowSampler,
        float3(shadowUv + float2(-shadowTexel.x,  shadowTexel.y), lightIndex),
        lightNdc.z - 0.0015);
    visibility += gShadowMap.SampleCmpLevelZero(
        gShadowSampler,
        float3(shadowUv + float2( shadowTexel.x,  shadowTexel.y), lightIndex),
        lightNdc.z - 0.0015);
    return visibility * 0.25;
}

float3 EvaluateRefractedAreaLight(
    float3 position,
    float3 rayDirection,
    uint lightIndex)
{
    const float3 surfaceCenter = gLightSurfaceOrigin[lightIndex].xyz;
    const float3 centralAxis =
        gLightRefractedAxis[lightIndex].xyz;
    const float3 fromSurface =
        position - surfaceCenter;
    const float lightDepth =
        dot(fromSurface, centralAxis);
    if (lightDepth <= 0.0)
    {
        return 0.0;
    }

    // Measure from the refracted axis rather than from a vertical column.
    // This makes the visible shaft follow the water-bent light direction.
    const float3 axisCenter =
        surfaceCenter + centralAxis * lightDepth;
    const float3 lateral =
        position - axisCenter;

    // Water diffusion and surface curvature quickly erase the fixture's
    // literal panel silhouette. Use a soft elliptical Gaussian that widens
    // with depth so the volume reads as a light shaft, not a projected box.
    const float halfWidth = gUnderwaterArchMode > 0.5
        ? 1.34 + lightDepth * 0.15
        : 1.28 + lightDepth * 0.18;
    const float halfDepth = gUnderwaterArchMode > 0.5
        ? 1.02 + lightDepth * 0.11
        : 0.88 + lightDepth * 0.13;
    const float normalizedX =
        lateral.x / max(halfWidth, 0.001);
    const float normalizedZ =
        lateral.z / max(halfDepth, 0.001);
    const float radialDistance =
        normalizedX * normalizedX +
        normalizedZ * normalizedZ;
    const float shaft = exp(
        -radialDistance *
        (gUnderwaterArchMode > 0.5 ? 1.45 : 1.65));
    if (shaft <= 0.002)
    {
        return 0.0;
    }

    const float edgeFocus =
        saturate(1.0 - radialDistance * 0.38);
    const float focusing =
        lerp(0.78, 1.34, gLightRefractedAxis[lightIndex].w) *
        lerp(0.90, 1.08, edgeFocus);

    const float distanceAttenuation =
        rcp(1.0 + lightDepth * lightDepth * 0.010);
    const float phase = min(
        PhaseHenyeyGreenstein(
            // Incoming radiance travels along centralAxis, while the
            // scattering direction toward the eye is -rayDirection.
            dot(-rayDirection, centralAxis),
            gAnisotropy) * (4.0 * PI),
        6.0);
    // Instanced fish and rays do not yet write the legacy prototype shadow
    // buffer. Its diagnostic ellipsoids would create ghost occluders, so keep
    // this path unshadowed until biology is submitted to the shadow atlas.
    const float shadow = 1.0;
    const float3 lightColor =
        gLightColorStrength[lightIndex].rgb;
    const float intensity =
        (gUnderwaterArchMode > 0.5 ? 7.2 : 6.8) *
        gLightColorStrength[lightIndex].w;
    return lightColor *
        shaft *
        focusing *
        shadow *
        distanceAttenuation *
        phase *
        intensity;
}

float3 IntegrateVolume(
    float3 rayOrigin,
    float3 rayDirection,
    float maxDistance,
    float2 pixelUv)
{
    // Temporal reprojection hides the lower spatial sample count while saving
    // two shadowed steps and one 3D-noise lookup per remaining step.
    const int stepCount = 6;
    const float travelDistance = min(maxDistance, 15.0);
    const float stepLength = travelDistance / stepCount;
    float3 accumulated = 0.0;
    float3 transmittance = 1.0;

    [loop]
    for (int step = 0; step < stepCount; ++step)
    {
        // Centered intervals avoid the sparkling produced by per-pixel,
        // per-frame ray-start jitter at one-third resolution. The animated
        // water field and density still provide natural temporal motion.
        const float distance = (step + 0.5) * stepLength;
        const float3 position = rayOrigin + rayDirection * distance;

        const float3 noiseOffset = float3(0.0, gTime * 0.006, gTime * 0.002);
        const float coarseNoise = gNoiseTexture.SampleLevel(
            gLinearWrapSampler,
            position * 0.035 + noiseOffset,
            0).r;
        const float mediumNoise =
            lerp(0.98, 1.02, coarseNoise);
        const float surfaceModulation =
            lerp(0.78, 1.14, coarseNoise);
        // Sum the three analytic shafts. Nearest-only selection exposed its
        // Voronoi boundaries as vertical rectangular bands on the back wall.
        // Shadow sampling is currently disabled, so this remains inexpensive
        // at the one-third-resolution, six-step volume buffer.
        float3 incidentLight = 0.0;
        [unroll]
        for (uint lightIndex = 0;
             lightIndex < (uint)gActiveLightCount;
             ++lightIndex)
        {
            incidentLight += EvaluateRefractedAreaLight(
                position,
                rayDirection,
                lightIndex);
        }
        incidentLight *= surfaceModulation * mediumNoise * gVolumeStrength;

        const float mediumDensity = (0.72 + mediumNoise * 0.38) * gWaterClarity;
        const float3 sigmaS = float3(0.007, 0.034, 0.086) * mediumDensity;
        const float3 sigmaA = float3(0.042, 0.016, 0.005) * mediumDensity;
        const float3 sigmaT = sigmaS + sigmaA;
        const float3 segmentTransmittance = exp(-sigmaT * stepLength);
        const float depthHaze =
            saturate((2.65 - position.y) / 5.5);
        const float3 ambientHaze =
            float3(0.0010, 0.0055, 0.0085) *
            lerp(0.62, 1.0, depthHaze) *
            mediumNoise *
            gVolumeStrength;
        const float3 source =
            incidentLight * sigmaS +
            ambientHaze;
        const float3 integratedSource =
            source * (1.0 - segmentTransmittance) / max(sigmaT, 0.0001);

        accumulated += transmittance * integratedSource;
        transmittance *= segmentTransmittance;
        if (max(transmittance.r, max(transmittance.g, transmittance.b)) < 0.025)
        {
            break;
        }
    }

    return accumulated;
}

float3 IntegrateArchVolume(
    float3 rayOrigin,
    float3 rayDirection,
    float maxDistance)
{
    // This is the production shaft path: no transparent light-card geometry.
    // Three centered samples at one-third resolution retain enough depth
    // variation for a moving viewpoint; temporal reprojection and bilateral
    // upsampling recover continuity at a fraction of a full-resolution march.
    const int stepCount = 3;
    const float travelDistance = min(maxDistance, 14.0);
    const float stepLength = travelDistance / stepCount;
    float3 accumulated = 0.0;
    float3 transmittance = 1.0;

    [unroll]
    for (int step = 0; step < stepCount; ++step)
    {
        const float distance = (step + 0.5) * stepLength;
        const float3 position = rayOrigin + rayDirection * distance;
        if (position.x < 0.0 || position.x > 48.0)
        {
            continue;
        }

        const float slowDensity =
            0.92 + 0.08 * sin(
                position.x * 0.17 +
                position.y * 0.31 +
                gTime * 0.16);
        float3 incidentLight = 0.0;
        // The three authored banks are ordered along +X. A single coherent
        // route split selects the relevant adjacent pair without per-pixel
        // ranking or divergent dynamic loops.
        const uint nearestLight = position.x < 32.0 ? 0u : 1u;
        const uint secondLight = nearestLight + 1u;
        incidentLight += EvaluateRefractedAreaLight(
            position, rayDirection, nearestLight);
        incidentLight += EvaluateRefractedAreaLight(
            position, rayDirection, secondLight);
        incidentLight *= slowDensity * gVolumeStrength * 1.08;

        const float mediumDensity = 0.62 * gWaterClarity;
        const float3 sigmaS =
            float3(0.0065, 0.030, 0.082) * mediumDensity;
        const float3 sigmaA =
            float3(0.050, 0.018, 0.006) * mediumDensity;
        const float3 sigmaT = sigmaS + sigmaA;
        const float3 segmentTransmittance = exp(-sigmaT * stepLength);
        const float3 integratedSource =
            incidentLight * sigmaS *
            (1.0 - segmentTransmittance) / max(sigmaT, 0.0001);
        accumulated += transmittance * integratedSource;
        transmittance *= segmentTransmittance;
    }
    return accumulated;
}

float3 ShadeRoom(float3 position, float3 normal, float3 rayDirection, float material)
{
    float nearestAreaLightDistance = 1e8;
    [unroll]
    for (uint coverageLightIndex = 0;
         coverageLightIndex < (uint)gActiveLightCount;
         ++coverageLightIndex)
    {
        const float2 lightOffset =
            position.xz -
            gLightSurfaceOrigin[coverageLightIndex].xz;
        nearestAreaLightDistance = min(
            nearestAreaLightDistance,
            dot(lightOffset, lightOffset));
    }
    const float areaLightCoverage = rcp(
        1.0 + nearestAreaLightDistance * 0.32);

    if (material >= 10.0)
    {
        const float casterId = material - 10.0;
        const float3 rockColor = casterId > 0.5 && casterId < 1.5
            ? float3(0.045, 0.22, 0.22)
            : float3(0.035, 0.105, 0.11);
        const float diffuse = 0.18 + 0.82 * saturate(
            dot(normal, normalize(float3(-0.25, 0.9, -0.3))));
        const float caustics = Caustics(position.xz * 1.2, gTime) *
            gCausticsStrength * 0.24 *
            lerp(0.32, 1.0, areaLightCoverage);
        const float rim = pow(
            1.0 - saturate(dot(-rayDirection, normal)),
            3.0);
        return rockColor * diffuse +
            float3(0.04, 0.38, 0.68) * caustics +
            float3(0.025, 0.16, 0.22) * rim;
    }

    float3 baseColor = material < 1.5
        ? float3(0.010, 0.038, 0.075)
        : float3(0.008, 0.030, 0.063);

    const float2 causticsUv = abs(normal.y) > 0.5 ? position.xz : position.xy;
    const float caustics =
        Caustics(causticsUv * 0.58, gTime) *
        gCausticsStrength *
        lerp(0.28, 1.0, areaLightCoverage);
    const float facing = saturate(dot(normal, normalize(float3(-0.15, 0.95, -0.25))));
    const float horizontalSurface = smoothstep(0.35, 0.75, abs(normal.y));
    const float causticsVisibility =
        lerp(0.0, 0.16 + facing * 0.84, horizontalSurface);

    float3 color = baseColor;
    color += float3(0.020, 0.255, 0.90) *
        caustics * causticsVisibility;

    if (normal.z < -0.5)
    {
        const float heightGlow = pow(
            saturate((position.y + 2.25) / 5.25),
            3.2);
        const float centerGlow = exp(-position.x * position.x * 0.055);
        color += float3(0.002, 0.025, 0.13) *
            heightGlow * (0.32 + centerGlow * 0.68);
    }

    const float rim = pow(1.0 - saturate(dot(-rayDirection, normal)), 3.0);
    color += float3(0.004, 0.035, 0.10) * rim;
    return color;
}

float3 ShadeWaterSurface(float3 position, float3 rayDirection)
{
    const float3 normal = WaterSurfaceNormal(position);
    const float fresnel = 0.02 + 0.98 * pow(1.0 - saturate(dot(-rayDirection, normal)), 5.0);

    const float skyGradient = saturate(rayDirection.y * 0.5 + 0.5);
    const float3 reflected = lerp(
        float3(0.018, 0.13, 0.22),
        float3(0.060, 0.48, 0.56),
        skyGradient);

    const float sparkle =
        pow(saturate(dot(reflect(-normalize(float3(-0.2, -1.0, 0.1)), normal), -rayDirection)), 120.0) * 8.0;
    const float caustics = Caustics(position.xz * 0.82, gTime) * 0.085;
    const float ceilingBreakup =
        0.58 + 0.42 * gNoiseTexture.SampleLevel(
            gLinearWrapSampler,
            position * 0.045 + float3(0, gTime * 0.004, 0),
            0).r;
    float3 lightEntryGlow = 0.0;
    [unroll]
    for (uint lightIndex = 0;
         lightIndex < (uint)gActiveLightCount;
         ++lightIndex)
    {
        // Treat the fixtures as broad surface irradiance instead of drawing
        // their rectangular emitter shape directly onto the water.
        const float2 entryOffset =
            position.xz - gLightSurfaceOrigin[lightIndex].xz +
            normal.xz * 0.82;
        const float2 ellipticalOffset =
            entryOffset / float2(2.35, 1.55);
        const float radialFalloff =
            saturate(1.0 - dot(ellipticalOffset, ellipticalOffset));
        const float entryHighlight =
            radialFalloff * radialFalloff *
            lerp(0.88, 1.08, ceilingBreakup);
        const float slopeFocus =
            lerp(0.78, 1.18, gLightRefractedAxis[lightIndex].w);
        lightEntryGlow +=
            gLightColorStrength[lightIndex].rgb * 0.25 *
            entryHighlight * slopeFocus * (1.0 - fresnel * 0.58);
    }

    return reflected * (0.20 + fresnel * 0.82) * ceilingBreakup +
        lightEntryGlow +
        float3(0.055, 0.36, 1.35) * (sparkle * 0.62 + caustics);
}

float3 ApplyUnderwaterAbsorption(float3 color, float distance)
{
    const float3 absorption =
        float3(0.14, 0.045, 0.018) * gWaterClarity;
    const float3 transmission = exp(-absorption * distance);
    const float3 inScattering =
        float3(0.007, 0.038, 0.072) *
        (1.0 - transmission);
    return color * transmission + inScattering;
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float SignedDistanceRoundedBox(float2 position, float2 halfSize, float radius)
{
    const float2 q = abs(position) - halfSize + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float3 HighlightBloom(float2 uv)
{
    const float2 texel = 1.0 / gResolution;
    const float2 offsets[4] =
    {
        float2(-3.5, -3.5),
        float2( 3.5, -3.5),
        float2(-3.5,  3.5),
        float2( 3.5,  3.5)
    };

    float3 bloom = 0.0;
    [unroll]
    for (int index = 0; index < 4; ++index)
    {
        const float3 sampleColor = gSceneColorTexture.SampleLevel(
            gLinearClampSampler,
            uv + offsets[index] * texel,
            0).rgb;
        const float brightness = max(
            sampleColor.r,
            max(sampleColor.g, sampleColor.b));
        bloom += sampleColor * smoothstep(0.32, 1.1, brightness);
    }
    return bloom * 0.105;
}

float3 StageHighlightBloom(float2 uv)
{
    // Route views contain compact practical emitters rather than the full
    // analytic tank. Two symmetric bilinear taps preserve their soft halo at
    // half the scene-color bandwidth of the generic four-corner filter.
    const float2 texel = 1.0 / gResolution;
    const float3 sampleA = gSceneColorTexture.SampleLevel(
        gLinearClampSampler,
        uv + float2(-3.25, -2.75) * texel,
        0).rgb;
    const float3 sampleB = gSceneColorTexture.SampleLevel(
        gLinearClampSampler,
        uv + float2(3.25, 2.75) * texel,
        0).rgb;
    const float brightnessA = max(sampleA.r, max(sampleA.g, sampleA.b));
    const float brightnessB = max(sampleB.r, max(sampleB.g, sampleB.b));
    return (
        sampleA * smoothstep(0.32, 1.1, brightnessA) +
        sampleB * smoothstep(0.32, 1.1, brightnessB)) * 0.185;
}

void BuildCameraBasis(
    float yaw,
    float pitch,
    out float3 forward,
    out float3 right,
    out float3 up)
{
    const float yawCos = cos(yaw);
    const float yawSin = sin(yaw);
    const float pitchCos = cos(pitch);
    const float pitchSin = sin(pitch);

    forward = normalize(float3(yawSin * pitchCos, pitchSin, yawCos * pitchCos));
    right = normalize(float3(yawCos, 0.0, -yawSin));
    up = normalize(cross(forward, right));
}

void BuildCameraRay(float2 uv, out float3 rayOrigin, out float3 rayDirection)
{
    float2 screen = uv * 2.0 - 1.0;
    screen.y *= -1.0;
    screen.x *= gResolution.x / max(gResolution.y, 1.0);

    float3 forward;
    float3 right;
    float3 up;
    BuildCameraBasis(gCameraYaw, gCameraPitch, forward, right, up);

    rayOrigin = gViewMode > 0.5
        ? gCameraPosition
        : float3(0.0, 0.05, -4.25);
    rayDirection = normalize(forward * 1.45 + right * screen.x + up * screen.y);
}

bool BuildAquariumRay(
    float2 uv,
    out float3 rayOrigin,
    out float3 rayDirection,
    out float3 glassTransmission,
    out float glassReflection,
    out float3 glassPosition)
{
    BuildCameraRay(uv, rayOrigin, rayDirection);
    glassTransmission = 1.0;
    glassReflection = 0.0;
    glassPosition = 0.0;

    if (gViewMode < 0.5)
    {
        return true;
    }

    float3 waterOrigin;
    float3 waterDirection;
    const float3 airOrigin = rayOrigin;
    const float3 airDirection = rayDirection;
    const bool valid = RefractThroughAquariumGlass(
        rayOrigin,
        rayDirection,
        waterOrigin,
        waterDirection,
        glassTransmission,
        glassReflection);
    if (valid)
    {
        const float frontDistance =
            (-5.10 - airOrigin.z) / airDirection.z;
        glassPosition =
            airOrigin + airDirection * frontDistance;
    }
    rayOrigin = waterOrigin;
    rayDirection = waterDirection;
    return valid;
}

float3 EvaluateCorridorGlassReflection(float3 glassPosition)
{
    const float3 glassNormal = float3(0.0, 0.0, -1.0);
    const float3 toViewer =
        normalize(gCameraPosition - glassPosition);
    const float3 reflectedLookupDirection =
        reflect(-toViewer, glassNormal);
    if (reflectedLookupDirection.y <= 0.001)
    {
        return float3(0.008, 0.012, 0.018);
    }

    static const float CORRIDOR_CEILING_Y = 2.72;
    const float ceilingDistance =
        (CORRIDOR_CEILING_Y - glassPosition.y) /
        reflectedLookupDirection.y;
    if (ceilingDistance <= 0.0)
    {
        return float3(0.008, 0.012, 0.018);
    }

    const float3 ceilingPosition =
        glassPosition +
        reflectedLookupDirection * ceilingDistance;
    const float3 fixtureCenters[3] =
    {
        float3(-3.0, CORRIDOR_CEILING_Y, -6.70),
        float3( 0.0, CORRIDOR_CEILING_Y, -6.70),
        float3( 3.0, CORRIDOR_CEILING_Y, -6.70)
    };

    float fixtureReflection = 0.0;
    [unroll]
    for (uint fixtureIndex = 0;
         fixtureIndex < 3;
         ++fixtureIndex)
    {
        const float2 panelOffset =
            ceilingPosition.xz -
            fixtureCenters[fixtureIndex].xz;
        const float panel =
            (1.0 - smoothstep(0.48, 0.72, abs(panelOffset.x))) *
            (1.0 - smoothstep(0.20, 0.36, abs(panelOffset.y)));
        fixtureReflection += panel;
    }

    const float worldStreak =
        0.5 + 0.5 * sin(
            glassPosition.x * 37.0 +
            glassPosition.y * 19.0);
    return
        float3(0.85, 1.05, 1.22) * fixtureReflection +
        float3(0.012, 0.020, 0.030) *
            (0.35 + worldStreak * 0.65);
}

bool BuildAquariumVolumeRay(
    float2 uv,
    out float3 rayOrigin,
    out float3 rayDirection,
    out float3 glassTransmission)
{
    BuildCameraRay(uv, rayOrigin, rayDirection);
    glassTransmission = 1.0;
    if (gUnderwaterArchMode > 0.5)
    {
        // Intersect the exact elliptical cross-section used by the authored
        // glass proxy. The camera starts inside the dry tunnel; volume begins
        // only after the ray exits through the upper/side acrylic shell.
        const float routeT = saturate(rayOrigin.x / 48.0);
        const float localFloor =
            -4.7 * routeT * routeT * (3.0 - 2.0 * routeT);
        const float centerY = localFloor + 1.20;
        const float normalizedY =
            (rayOrigin.y - centerY) / 3.72;
        const float normalizedZ = rayOrigin.z / 3.42;
        const float directionY = rayDirection.y / 3.72;
        const float directionZ = rayDirection.z / 3.42;
        const float quadraticA =
            directionY * directionY + directionZ * directionZ;
        const float quadraticB = 2.0 *
            (normalizedY * directionY + normalizedZ * directionZ);
        const float quadraticC =
            normalizedY * normalizedY +
            normalizedZ * normalizedZ - 1.0;
        const float discriminant =
            quadraticB * quadraticB -
            4.0 * quadraticA * quadraticC;
        if (quadraticA < 0.000001 || discriminant <= 0.0)
        {
            return false;
        }

        const float entryDistance =
            (-quadraticB + sqrt(discriminant)) /
            (2.0 * quadraticA);
        if (entryDistance <= 0.001 || entryDistance > 10.0)
        {
            return false;
        }
        const float3 entryPosition =
            rayOrigin + rayDirection * entryDistance;
        const float entryRouteT = saturate(entryPosition.x / 48.0);
        const float entryFloor =
            -4.7 * entryRouteT * entryRouteT *
            (3.0 - 2.0 * entryRouteT);
        if (entryPosition.x < 0.0 || entryPosition.x > 48.0 ||
            entryPosition.y < entryFloor + 1.15)
        {
            return false;
        }

        rayOrigin = entryPosition + rayDirection * 0.025;
        glassTransmission = float3(0.94, 0.975, 0.995);
        return true;
    }
    if (gViewMode < 0.5)
    {
        return true;
    }

    static const float GLASS_FRONT_Z = -5.10;
    static const float GLASS_BACK_Z = -4.94;
    if (rayDirection.z <= 0.0001)
    {
        return false;
    }

    const float frontDistance =
        (GLASS_FRONT_Z - rayOrigin.z) / rayDirection.z;
    if (frontDistance <= 0.001)
    {
        return false;
    }

    const float3 frontPosition =
        rayOrigin + rayDirection * frontDistance;
    if (abs(frontPosition.x) > 5.45 ||
        frontPosition.y < -2.08 ||
        frontPosition.y > 2.84)
    {
        return false;
    }

    const float3 glassNormal = GlassMicroNormal(frontPosition);
    const float3 waterDirection = refract(
        rayDirection,
        glassNormal,
        1.0 / 1.333);
    if (dot(waterDirection, waterDirection) < 0.25)
    {
        return false;
    }

    // Parallel glass interfaces have the same net angular refraction as a
    // direct air-to-water transition. Volume is soft and half resolution, so
    // retaining only that equivalent angle avoids the second refraction,
    // Fresnel and exponential work without a visible lighting mismatch.
    const float backDistance =
        (GLASS_BACK_Z - rayOrigin.z) / rayDirection.z;
    rayOrigin =
        rayOrigin + rayDirection * backDistance +
        normalize(waterDirection) * 0.004;
    rayDirection = normalize(waterDirection);
    glassTransmission = float3(0.935, 0.955, 0.968);
    return true;
}

float2 ProjectToCamera(
    float3 worldPosition,
    float yaw,
    float pitch,
    float viewMode,
    float3 glassCameraPosition,
    out float valid)
{
    float3 forward;
    float3 right;
    float3 up;
    BuildCameraBasis(
        yaw,
        pitch,
        forward,
        right,
        up);

    const float3 cameraOrigin = viewMode > 0.5
        ? glassCameraPosition
        : float3(0.0, 0.05, -4.25);
    const float3 toPosition = worldPosition - cameraOrigin;
    const float forwardDistance = dot(toPosition, forward);
    valid = forwardDistance > 0.001 ? 1.0 : 0.0;

    const float safeForwardDistance = max(forwardDistance, 0.001);
    float2 screen;
    screen.x = 1.45 * dot(toPosition, right) / safeForwardDistance;
    screen.y = 1.45 * dot(toPosition, up) / safeForwardDistance;

    const float aspect = gResolution.x / max(gResolution.y, 1.0);
    const float2 ndc = float2(screen.x / aspect, -screen.y);
    const float2 previousUv = ndc * 0.5 + 0.5;
    valid *= all(previousUv >= 0.0) && all(previousUv <= 1.0) ? 1.0 : 0.0;
    return previousUv;
}

float2 ProjectToPreviousFrame(float3 worldPosition, out float valid)
{
    return ProjectToCamera(
        worldPosition,
        gPreviousCameraYaw,
        gPreviousCameraPitch,
        gPreviousViewMode,
        gPreviousCameraPosition,
        valid);
}

float GetSceneDistance(float3 rayOrigin, float3 rayDirection, out SceneHit roomHit)
{
    roomHit = TraceRoom(rayOrigin, rayDirection);
    const float surfaceDistance = IntersectWaterSurface(
        rayOrigin,
        rayDirection);
    return min(roomHit.distance, surfaceDistance);
}

struct SceneOutput
{
    float4 color : SV_TARGET0;
    float depth : SV_TARGET1;
    float2 motion : SV_TARGET2;
};

SceneOutput PSScene(VSOutput input)
{
    float3 rayOrigin;
    float3 rayDirection;
    float3 glassTransmission;
    float glassReflection;
    float3 glassPosition;
    const bool aquariumRayValid = BuildAquariumRay(
        input.uv,
        rayOrigin,
        rayDirection,
        glassTransmission,
        glassReflection,
        glassPosition);

    if (!aquariumRayValid)
    {
        SceneOutput blockedOutput;
        // Rays outside the aquarium window represent an unlit corridor wall.
        // The previous screen-space sine modulation produced unexplained
        // horizontal stripes in otherwise empty distance. Keep this region
        // uniform; glass micro-reflections are handled separately where a ray
        // actually intersects the viewing panel.
        blockedOutput.color = float4(
            float3(0.003, 0.005, 0.009),
            1.0);
        blockedOutput.depth = FAR_DISTANCE;
        blockedOutput.motion = 0.0;
        return blockedOutput;
    }

    SceneHit roomHit;
    const float surfaceDistance = IntersectWaterSurface(
        rayOrigin,
        rayDirection);

    roomHit = TraceRoom(rayOrigin, rayDirection);
    const float sceneDistance = min(roomHit.distance, surfaceDistance);
    float3 sceneColor;

    if (surfaceDistance < roomHit.distance)
    {
        const float3 surfacePosition = rayOrigin + rayDirection * surfaceDistance;
        sceneColor = ShadeWaterSurface(surfacePosition, rayDirection);
    }
    else
    {
        const float3 hitPosition = rayOrigin + rayDirection * roomHit.distance;
        sceneColor = ShadeRoom(hitPosition, roomHit.normal, rayDirection, roomHit.material);
    }

    sceneColor = ApplyUnderwaterAbsorption(sceneColor, sceneDistance);

    if (gViewMode > 0.5)
    {
        const float3 corridorReflection =
            EvaluateCorridorGlassReflection(glassPosition);
        sceneColor =
            sceneColor * glassTransmission +
            corridorReflection * glassReflection;
    }

    SceneOutput output;
    output.color = float4(sceneColor, 1.0);
    output.depth = sceneDistance;
    const float3 currentWorldPosition = rayOrigin + rayDirection * sceneDistance;
    float3 previousWorldPosition = currentWorldPosition;
    if (roomHit.material >= 11.0 && roomHit.material < 12.0 &&
        roomHit.distance <= surfaceDistance)
    {
        const float3 currentCenter = GetCasterCenter(1, gTime);
        const float3 previousCenter = GetCasterCenter(1, gTime - gDeltaTime);
        previousWorldPosition += previousCenter - currentCenter;
    }
    float motionValid;
    const float2 previousUv = ProjectToPreviousFrame(
        previousWorldPosition,
        motionValid);
    output.motion = motionValid > 0.5
        ? previousUv - input.uv
        : 0.0;
    return output;
}

float4 PSVolume(VSOutput input) : SV_TARGET
{
    float3 rayOrigin;
    float3 rayDirection;
    float3 glassTransmission;
    const bool aquariumRayValid = BuildAquariumVolumeRay(
        input.uv,
        rayOrigin,
        rayDirection,
        glassTransmission);

    const float sceneDistance = gSceneDepthTexture.SampleLevel(
        gLinearClampSampler,
        input.uv,
        0);
    if (!aquariumRayValid)
    {
        return float4(0.0, 0.0, 0.0, sceneDistance);
    }
    const float archEntryDistance = gUnderwaterArchMode > 0.5
        ? length(rayOrigin - gCameraPosition)
        : 0.0;
    float volumeDistance = max(
        sceneDistance - archEntryDistance,
        0.0);
    if (gUnderwaterArchMode > 0.5 && rayDirection.y > 0.0001)
    {
        // The authored water surface is transparent and therefore absent from
        // the opaque depth buffer. Explicitly stop integration at Y=5.8 so a
        // shaft never continues into the air above the tank.
        const float surfaceExitDistance = max(
            (5.8 - rayOrigin.y) / rayDirection.y,
            0.0);
        volumeDistance = min(volumeDistance, surfaceExitDistance);
    }
    const float3 volume = gUnderwaterArchMode > 0.5
        ? IntegrateArchVolume(
            rayOrigin,
            rayDirection,
            volumeDistance)
        : IntegrateVolume(
            rayOrigin,
            rayDirection,
            volumeDistance,
            input.uv);
    return float4(volume * glassTransmission, sceneDistance);
}

float4 PSTemporal(VSOutput input) : SV_TARGET
{
    const float2 texelSize = 3.0 / gResolution;
    const float4 currentSample = gVolumeTexture.SampleLevel(
        gLinearClampSampler,
        input.uv,
        0);

    if (gHistoryValid < 0.5)
    {
        return currentSample;
    }

    const float2 motion = gMotionTexture.SampleLevel(
        gLinearClampSampler,
        input.uv,
        0);
    const float2 previousUv = input.uv + motion;
    const float projectionValid =
        all(previousUv >= 0.0) && all(previousUv <= 1.0)
        ? 1.0
        : 0.0;
    float4 historySample = gHistoryTexture.SampleLevel(
        gLinearClampSampler,
        previousUv,
        0);

    float3 neighborhoodMinimum = currentSample.rgb;
    float3 neighborhoodMaximum = currentSample.rgb;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            if (abs(x) + abs(y) > 1)
            {
                continue;
            }

            const float3 neighbor = gVolumeTexture.SampleLevel(
                gLinearClampSampler,
                input.uv + float2(x, y) * texelSize,
                0).rgb;
            neighborhoodMinimum = min(neighborhoodMinimum, neighbor);
            neighborhoodMaximum = max(neighborhoodMaximum, neighbor);
        }
    }

    const float3 neighborhoodRange = max(
        neighborhoodMaximum - neighborhoodMinimum,
        0.004);
    historySample.rgb = clamp(
        historySample.rgb,
        neighborhoodMinimum - neighborhoodRange * 0.18,
        neighborhoodMaximum + neighborhoodRange * 0.18);

    const float depthThreshold = max(0.08, currentSample.a * 0.018);
    const float depthValid = abs(historySample.a - currentSample.a) < depthThreshold ? 1.0 : 0.0;
    const float luminanceCurrent = dot(currentSample.rgb, float3(0.2126, 0.7152, 0.0722));
    const float luminanceHistory = dot(historySample.rgb, float3(0.2126, 0.7152, 0.0722));
    const float luminanceAgreement =
        exp(-abs(luminanceCurrent - luminanceHistory) * 3.0);

    const float historyBlend =
        saturate(gHistoryWeight) *
        gHistoryValid *
        projectionValid *
        depthValid *
        lerp(0.45, 1.0, luminanceAgreement);

    return float4(
        lerp(currentSample.rgb, historySample.rgb, historyBlend),
        currentSample.a);
}

float3 UpsampleVolume(float2 uv, float fullResolutionDepth)
{
    const float2 texelSize = 3.0 / gResolution;
    float3 accumulated = 0.0;
    float totalWeight = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            if (abs(x) + abs(y) > 1)
            {
                continue;
            }

            const float2 offset = float2(x, y) * texelSize;
            const float4 sampleValue = gVolumeTexture.SampleLevel(
                gLinearClampSampler,
                uv + offset,
                0);
            const float spatialWeight = (x == 0 && y == 0) ? 1.0 : 0.68;
            const float depthWeight = exp(-abs(sampleValue.a - fullResolutionDepth) * 2.2);
            const float weight = spatialWeight * depthWeight + 0.0001;
            accumulated += sampleValue.rgb * weight;
            totalWeight += weight;
        }
    }

    return accumulated / totalWeight;
}

float4 PSComposite(VSOutput input) : SV_TARGET
{
    float3 sceneColor = gSceneColorTexture.SampleLevel(
        gLinearClampSampler,
        input.uv,
        0).rgb;

    if (gStagePreviewMode > 0.5)
    {
        // Fast authored-route path: volume is disabled for these rooms, so a
        // depth fetch and the generic four-tap bloom would be pure bandwidth.
        sceneColor += StageHighlightBloom(input.uv);
        sceneColor = ACESFilm(sceneColor * gExposure);
        sceneColor = pow(sceneColor, 1.0 / 2.2);
        return float4(sceneColor, 1.0);
    }

    const float sceneDepth = gSceneDepthTexture.SampleLevel(
        gLinearClampSampler,
        input.uv,
        0);
    // Watatsumi and plain greybox modes intentionally set volume strength to
    // zero. Keep that a true fast path instead of issuing five null SRV taps.
    const float3 volume = gVolumeStrength > 0.0001
        ? UpsampleVolume(input.uv, sceneDepth)
        : 0.0;
    sceneColor += volume;
    sceneColor += HighlightBloom(input.uv);

    // VRC-aquarium grade: lifted blue-green shadows, restrained contrast and
    // a slightly stylized cyan separation without flattening HDR highlights.
    const float luminance = dot(
        sceneColor,
        float3(0.2126, 0.7152, 0.0722));
    sceneColor = lerp(luminance.xxx, sceneColor, 1.08);
    sceneColor *= float3(0.72, 1.00, 1.08);
    sceneColor += float3(0.002, 0.010, 0.014);

    const float2 normalizedScreen = input.uv * 2.0 - 1.0;
    const float rectangularEdge =
        smoothstep(
            0.78,
            1.02,
            max(abs(normalizedScreen.x), abs(normalizedScreen.y)));
    sceneColor *= lerp(1.0, 0.82, rectangularEdge);

    const float topGlow = 1.0 - smoothstep(0.02, 0.36, input.uv.y);
    sceneColor += volume * topGlow * float3(0.08, 0.18, 0.40);

    sceneColor = ACESFilm(sceneColor * gExposure);
    sceneColor = pow(sceneColor, 1.0 / 2.2);

    // A dark rounded viewing aperture makes the tank read as an exhibit rather
    // than an evenly lit underwater room. The interior remains independent.
    const float windowDistance = SignedDistanceRoundedBox(
        normalizedScreen,
        float2(0.965, 0.925),
        0.075);
    const float outsideWindow = smoothstep(-0.004, 0.012, windowDistance);
    const float innerFrame = smoothstep(-0.052, -0.018, windowDistance) *
        (1.0 - outsideWindow);
    const float3 frameColor = float3(0.0015, 0.003, 0.010);
    sceneColor = lerp(sceneColor, frameColor, outsideWindow);
    sceneColor *= 1.0 - innerFrame * 0.62;

    const float glassEdge =
        exp(-abs(windowDistance + 0.020) * 260.0) *
        (0.28 + 0.72 * saturate(1.0 - input.uv.y));
    const float3 glassEdgeColor = lerp(
        float3(0.01, 0.055, 0.16),
        float3(0.025, 0.095, 0.22),
        step(0.5, gViewMode));
    sceneColor += glassEdgeColor * glassEdge;

    const float plinth = smoothstep(0.94, 0.985, input.uv.y);
    sceneColor = lerp(sceneColor, frameColor, plinth * 0.82);
    return float4(sceneColor, 1.0);
}
