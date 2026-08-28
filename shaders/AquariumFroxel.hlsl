/*==================================================================================================

   [AquariumFroxel.hlsl]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   3D Froxelへの光散乱注入、奥行き積分、Temporal Reprojection、最終合成
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
};

cbuffer ShadowConstants : register(b1)
{
    row_major float4x4 gLightViewProjection[3];
    float4 gLightSurfaceOrigin[3];
    float4 gLightRefractedAxis[3];
    float gCurrentShadowLight;
    float3 gShadowPadding;
};

#if defined(AQUARIUM_FROXEL_STAGE)
cbuffer FroxelConstants : register(b2)
{
    uint3 gFroxelDimensions;
    float gFroxelNearDistance;

    float gFroxelFarDistance;
    float gFroxelLogarithmicDepthRatio;
    float2 gFroxelPadding;
};
#endif

Texture3D<float> gNoiseTexture : register(t0);
Texture2D<float4> gSceneColorTexture : register(t1);
Texture2D<float> gSceneDepthTexture : register(t2);
Texture2D<float4> gVolumeTexture : register(t3);
Texture2D<float4> gHistoryTexture : register(t4);
Texture2DArray<float> gShadowMap : register(t5);
Texture2D<float2> gMotionTexture : register(t6);
#if defined(AQUARIUM_FROXEL_STAGE)
Texture3D<float4> gFroxelInjectionTexture : register(t7);
Texture3D<float4> gFroxelIntegratedTexture : register(t8);
#endif
#if defined(AQUARIUM_FROXEL_INJECTION)
RWTexture3D<float4> gFroxelInjectionUav : register(u0);
#elif defined(AQUARIUM_FROXEL_INTEGRATION)
RWTexture3D<float4> gFroxelIntegratedUav : register(u0);
#endif
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
    return pow(saturate(accumulated * 0.22 - 0.12), 3.3) * 5.0;
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

    return gShadowMap.SampleCmpLevelZero(
        gShadowSampler,
        float3(shadowUv, lightIndex),
        lightNdc.z - 0.0015);
}

float3 EvaluateSpotLight(
    float3 position,
    float3 rayDirection,
    float3 lightOrigin,
    float3 lightAxis,
    float coneScale,
    float intensity,
    float3 color,
    uint lightIndex)
{
    const float3 fromLight = position - lightOrigin;
    const float lightDepth = dot(fromLight, lightAxis);
    const float beamMask = BeamMask(position, lightOrigin, lightAxis, coneScale);
    if (beamMask <= 0.001)
    {
        return 0.0;
    }
    const float distanceAttenuation = rcp(1.0 + lightDepth * lightDepth * 0.018);
    const float phase = PhaseHenyeyGreenstein(dot(rayDirection, lightAxis), gAnisotropy) * (4.0 * PI);
    const float shadow = SampleVolumetricShadow(position, lightIndex);
    return color * beamMask * shadow * distanceAttenuation * phase * intensity;
}

#if defined(AQUARIUM_FROXEL_STAGE)
float3 EvaluateAllSpotLights(float3 position, float3 rayDirection)
{
    return
        EvaluateSpotLight(
            position,
            rayDirection,
            gLightSurfaceOrigin[0].xyz,
            normalize(gLightRefractedAxis[0].xyz),
            0.155 * lerp(0.88, 1.16, gLightRefractedAxis[0].w),
            26.0,
            float3(0.075, 0.42, 1.12),
            0) +
        EvaluateSpotLight(
            position,
            rayDirection,
            gLightSurfaceOrigin[1].xyz,
            normalize(gLightRefractedAxis[1].xyz),
            0.175 * lerp(0.88, 1.16, gLightRefractedAxis[1].w),
            31.0,
            float3(0.095, 0.48, 1.24),
            1) +
        EvaluateSpotLight(
            position,
            rayDirection,
            gLightSurfaceOrigin[2].xyz,
            normalize(gLightRefractedAxis[2].xyz),
            0.15 * lerp(0.88, 1.16, gLightRefractedAxis[2].w),
            24.0,
            float3(0.09, 0.36, 0.98),
            2);
}
#endif

float3 IntegrateVolume(
    float3 rayOrigin,
    float3 rayDirection,
    float maxDistance,
    float2 pixelUv)
{
    const int stepCount = 12;
    const float travelDistance = min(maxDistance, 15.0);
    const float stepLength = travelDistance / stepCount;
    const float2 halfResolutionPixel = floor(
        pixelUv * gResolution * 0.5);
    const float pixelSeed = Hash31(float3(halfResolutionPixel, 17.0));
    const float jitter = frac(pixelSeed + gFrameIndex * 0.61803398875);

    float3 accumulated = 0.0;
    float3 transmittance = 1.0;

    [loop]
    for (int step = 0; step < stepCount; ++step)
    {
        const float distance = (step + 0.35 + jitter * 0.55) * stepLength;
        const float3 position = rayOrigin + rayDirection * distance;

        const float3 noiseOffset = float3(0.0, gTime * 0.006, gTime * 0.002);
        const float coarseNoise = gNoiseTexture.SampleLevel(
            gLinearWrapSampler,
            position * 0.035 + noiseOffset,
            0).r;
        const float detailNoise = gNoiseTexture.SampleLevel(
            gLinearWrapSampler,
            position * 0.11 - noiseOffset * 1.7,
            0).r;
        const float mediumNoise = lerp(0.72, 1.28, coarseNoise * 0.72 + detailNoise * 0.28);

        const float shaftBreakup = smoothstep(0.18, 0.88, detailNoise);
        const float surfaceModulation =
            lerp(0.42, 1.12, shaftBreakup) *
            lerp(0.82, 1.08, coarseNoise);
#if defined(AQUARIUM_FROXEL_STAGE)
        float3 incidentLight = EvaluateAllSpotLights(position, rayDirection);
#else
        float3 incidentLight =
            EvaluateSpotLight(
                position,
                rayDirection,
                gLightSurfaceOrigin[0].xyz,
                normalize(gLightRefractedAxis[0].xyz),
                0.155 * lerp(0.88, 1.16, gLightRefractedAxis[0].w),
                26.0,
                float3(0.075, 0.42, 1.12),
                0) +
            EvaluateSpotLight(
                position,
                rayDirection,
                gLightSurfaceOrigin[1].xyz,
                normalize(gLightRefractedAxis[1].xyz),
                0.175 * lerp(0.88, 1.16, gLightRefractedAxis[1].w),
                31.0,
                float3(0.095, 0.48, 1.24),
                1) +
            EvaluateSpotLight(
                position,
                rayDirection,
                gLightSurfaceOrigin[2].xyz,
                normalize(gLightRefractedAxis[2].xyz),
                0.15 * lerp(0.88, 1.16, gLightRefractedAxis[2].w),
                24.0,
                float3(0.09, 0.36, 0.98),
                2);
#endif
        incidentLight *= surfaceModulation * mediumNoise * gVolumeStrength;

        const float3 particleCoordinate =
            position * float3(1.8, 2.8, 1.8) +
            float3(0, gTime * 0.22, 0);
        const float3 particleCell = floor(particleCoordinate);
        float3 particleLocal = frac(particleCoordinate) - 0.5;
        particleLocal += float3(
            Hash31(particleCell + 13.1),
            Hash31(particleCell + 29.7),
            Hash31(particleCell + 47.3)) * 0.34 - 0.17;
        const float particleSeed = Hash31(particleCell);
        const float particle =
            smoothstep(0.978, 1.0, particleSeed) *
            exp(-dot(particleLocal, particleLocal) * 135.0) *
            1.15;

        const float mediumDensity = (0.72 + mediumNoise * 0.38) * gWaterClarity;
        const float3 sigmaS = float3(0.007, 0.034, 0.086) * mediumDensity;
        const float3 sigmaA = float3(0.042, 0.016, 0.005) * mediumDensity;
        const float3 sigmaT = sigmaS + sigmaA;
        const float3 segmentTransmittance = exp(-sigmaT * stepLength);
        const float3 particleLight = float3(0.03, 0.16, 0.48) * particle * 0.13;
        const float3 source = incidentLight * sigmaS + particleLight;
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

float3 ShadeRoom(float3 position, float3 normal, float3 rayDirection, float material)
{
    if (material >= 10.0)
    {
        const float casterId = material - 10.0;
        const float3 rockColor = casterId > 0.5 && casterId < 1.5
            ? float3(0.025, 0.16, 0.19)
            : float3(0.025, 0.07, 0.075);
        const float diffuse = 0.18 + 0.82 * saturate(
            dot(normal, normalize(float3(-0.25, 0.9, -0.3))));
        const float caustics = Caustics(position.xz * 1.2, gTime) *
            gCausticsStrength * 0.24;
        const float rim = pow(
            1.0 - saturate(dot(-rayDirection, normal)),
            3.0);
        return rockColor * diffuse +
            float3(0.04, 0.38, 0.68) * caustics +
            float3(0.025, 0.16, 0.22) * rim;
    }

    float3 baseColor = material < 1.5
        ? float3(0.003, 0.014, 0.030)
        : float3(0.002, 0.009, 0.023);

    const float2 causticsUv = abs(normal.y) > 0.5 ? position.xz : position.xy;
    const float caustics = Caustics(causticsUv * 0.72, gTime) * gCausticsStrength;
    const float facing = saturate(dot(normal, normalize(float3(-0.15, 0.95, -0.25))));
    const float causticsVisibility = 0.18 + facing * 0.82;

    float3 color = baseColor;
    color += float3(0.012, 0.16, 0.62) * caustics * causticsVisibility;

    if (normal.z < -0.5)
    {
        const float heightGlow = pow(
            saturate((position.y + 2.25) / 5.25),
            3.2);
        const float centerGlow = exp(-position.x * position.x * 0.055);
        color += float3(0.002, 0.025, 0.13) *
            heightGlow * (0.32 + centerGlow * 0.68);
    }

    const float groutX = smoothstep(0.94, 1.0, abs(sin(position.x * PI * 0.5)));
    const float groutZ = smoothstep(0.94, 1.0, abs(sin(position.z * PI * 0.5)));
    color *= 1.0 - max(groutX, groutZ) * 0.18;

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
        float3(0.002, 0.012, 0.045),
        float3(0.018, 0.11, 0.32),
        skyGradient);

    const float sparkle =
        pow(saturate(dot(reflect(-normalize(float3(-0.2, -1.0, 0.1)), normal), -rayDirection)), 120.0) * 8.0;
    const float caustics = Caustics(position.xz * 0.95, gTime) * 0.13;
    const float ceilingBreakup =
        0.58 + 0.42 * gNoiseTexture.SampleLevel(
            gLinearWrapSampler,
            position * 0.045 + float3(0, gTime * 0.004, 0),
            0).r;
    float3 lightEntryGlow = 0.0;
    [unroll]
    for (uint lightIndex = 0; lightIndex < 3; ++lightIndex)
    {
        const float2 entryOffset =
            position.xz - gLightSurfaceOrigin[lightIndex].xz;
        const float entryHighlight =
            exp(-dot(entryOffset, entryOffset) * 5.5);
        const float slopeFocus =
            lerp(0.72, 1.42, gLightRefractedAxis[lightIndex].w);
        lightEntryGlow +=
            float3(0.035, 0.24, 1.15) *
            entryHighlight * slopeFocus;
    }

    return reflected * (0.20 + fresnel * 0.82) * ceilingBreakup +
        lightEntryGlow +
        float3(0.055, 0.36, 1.35) * (sparkle * 0.62 + caustics);
}

float3 ApplyUnderwaterAbsorption(float3 color, float distance)
{
    const float3 absorption = float3(0.24, 0.074, 0.022) * gWaterClarity;
    const float3 transmission = exp(-absorption * distance);
    const float3 inScattering = float3(0.0015, 0.018, 0.082) * (1.0 - transmission);
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

    rayOrigin = float3(0.0, 0.05, -4.25);
    rayDirection = normalize(forward * 1.45 + right * screen.x + up * screen.y);
}

#if defined(AQUARIUM_FROXEL_COMPUTE)
float FroxelBoundaryDistance(uint boundaryIndex)
{
    const float normalizedDepth =
        (float)boundaryIndex / (float)gFroxelDimensions.z;
    return gFroxelNearDistance * exp(
        gFroxelLogarithmicDepthRatio * normalizedDepth);
}

#if defined(AQUARIUM_FROXEL_INJECTION)
[numthreads(4, 4, 4)]
void CSInjectFroxel(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId >= gFroxelDimensions))
    {
        return;
    }

    const float2 uv =
        ((float2)dispatchThreadId.xy + 0.5) /
        (float2)gFroxelDimensions.xy;
    float3 rayOrigin;
    float3 rayDirection;
    BuildCameraRay(uv, rayOrigin, rayDirection);

    const float nearBoundary = FroxelBoundaryDistance(dispatchThreadId.z);
    const float farBoundary = FroxelBoundaryDistance(dispatchThreadId.z + 1);
    const float sampleDistance = sqrt(nearBoundary * farBoundary);
    const float3 position = rayOrigin + rayDirection * sampleDistance;

    const float waveNoise =
        sin(dot(position, float3(1.73, 2.31, 1.19)) + gTime * 0.42) *
        sin(dot(position, float3(-2.11, 0.83, 1.57)) - gTime * 0.31);
    const float mediumNoise = 0.82 + waveNoise * 0.18;

    const float mediumDensity =
        mediumNoise * gWaterClarity;
    const float3 sigmaS =
        float3(0.007, 0.034, 0.086) * mediumDensity;
    const float3 sigmaA =
        float3(0.042, 0.016, 0.005) * mediumDensity;
    const float3 sigmaT = sigmaS + sigmaA;
    const float scalarExtinction = dot(
        sigmaT,
        float3(0.2126, 0.7152, 0.0722));

    float3 incidentLight = 0.0;
    [unroll]
    for (uint lightIndex = 0; lightIndex < 3; ++lightIndex)
    {
        const float3 axis = normalize(gLightRefractedAxis[lightIndex].xyz);
        const float3 delta = position - gLightSurfaceOrigin[lightIndex].xyz;
        const float axialDistance = dot(delta, axis);
        const float3 radialVector = delta - axis * axialDistance;
        const float radialDistance = length(radialVector);
        const float beamRadius =
            0.13 + max(axialDistance, 0.0) *
            (0.145 + gLightRefractedAxis[lightIndex].w * 0.025);
        float beam = saturate(1.0 - radialDistance / max(beamRadius, 0.02));
        beam = beam * beam * (3.0 - 2.0 * beam);
        beam *= axialDistance > 0.0 ? exp(-axialDistance * 0.075) : 0.0;
        const float phase = pow(
            saturate(dot(-rayDirection, axis) * 0.5 + 0.5),
            3.0);
        const float3 lightColor =
            lightIndex == 0 ? float3(0.08, 0.46, 1.18) :
            lightIndex == 1 ? float3(0.10, 0.54, 1.34) :
                              float3(0.09, 0.40, 1.08);
        incidentLight += lightColor * beam * lerp(0.35, 1.0, phase);
    }
    incidentLight *= mediumNoise * gVolumeStrength * 22.0;
    float3 source = max(incidentLight * sigmaS, 0.0);
    if (any(isnan(source)) || any(isinf(source)))
    {
        source = 0.0;
    }

    gFroxelInjectionUav[dispatchThreadId] = float4(
        source,
        clamp(scalarExtinction, 0.0001, 1.0));
}
#endif

#if defined(AQUARIUM_FROXEL_INTEGRATION)
[numthreads(8, 8, 1)]
void CSIntegrateFroxel(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId.xy >= gFroxelDimensions.xy))
    {
        return;
    }

    float3 accumulated = 0.0;
    float transmittance = 1.0;

    [loop]
    for (uint slice = 0; slice < gFroxelDimensions.z; ++slice)
    {
        const float nearBoundary = FroxelBoundaryDistance(slice);
        const float farBoundary = FroxelBoundaryDistance(slice + 1);
        const float segmentLength = farBoundary - nearBoundary;
        const uint3 coordinate = uint3(dispatchThreadId.xy, slice);
        float4 medium = gFroxelInjectionTexture.Load(
            int4(coordinate, 0));
        if (any(isnan(medium)) || any(isinf(medium)))
        {
            medium = float4(0.0, 0.0, 0.0, 0.0001);
        }
        medium.rgb = max(medium.rgb, 0.0);
        medium.a = clamp(medium.a, 0.0001, 1.0);
        const float opticalDepth = saturate(medium.a * segmentLength);
        const float segmentTransmittance = 1.0 - opticalDepth;
        accumulated +=
            transmittance * medium.rgb * segmentLength;
        accumulated = clamp(accumulated, 0.0, 32.0);
        transmittance *= segmentTransmittance;
        gFroxelIntegratedUav[coordinate] = float4(
            accumulated,
            transmittance);
    }
}
#endif
#endif

float2 ProjectToCamera(
    float3 worldPosition,
    float yaw,
    float pitch,
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

    const float3 cameraOrigin = float3(0.0, 0.05, -4.25);
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
    BuildCameraRay(input.uv, rayOrigin, rayDirection);

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

#if defined(AQUARIUM_FROXEL_STAGE)
float4 PSVolume(VSOutput input) : SV_TARGET
{
    const float sceneDistance = gSceneDepthTexture.SampleLevel(
        gLinearClampSampler,
        input.uv,
        0);
    const float clampedDistance = clamp(
        sceneDistance,
        gFroxelNearDistance,
        gFroxelFarDistance);
    const float logarithmicDepth = log(
        clampedDistance / gFroxelNearDistance) /
        gFroxelLogarithmicDepthRatio;
    const float sliceCoordinate = saturate(logarithmicDepth);
    float3 volume = gFroxelIntegratedTexture.SampleLevel(
        gLinearClampSampler,
        float3(input.uv, sliceCoordinate),
        0).rgb;
    if (any(isnan(volume)) || any(isinf(volume)))
    {
        volume = 0.0;
    }
    volume = clamp(volume, 0.0, 32.0);
    return float4(volume, sceneDistance);
}
#endif

float4 PSTemporal(VSOutput input) : SV_TARGET
{
    const float2 texelSize = 2.0 / gResolution;
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
    const float2 texelSize = 2.0 / gResolution;
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
#if DEBUG_SCENE_VIEW
    return float4(pow(saturate(sceneColor * gExposure), 1.0 / 2.2), 1.0);
#endif
    const float sceneDepth = gSceneDepthTexture.SampleLevel(
        gLinearClampSampler,
        input.uv,
        0);
    const float3 volume = UpsampleVolume(input.uv, sceneDepth);
    sceneColor += volume;
    sceneColor += HighlightBloom(input.uv);

    // Exhibition-grade deep-blue color separation: keep cyan highlights while
    // pushing unlit water and architecture close to black.
    const float luminance = dot(
        sceneColor,
        float3(0.2126, 0.7152, 0.0722));
    sceneColor = lerp(luminance.xxx, sceneColor, 1.22);
    sceneColor *= float3(0.56, 0.82, 1.24);
    sceneColor -= float3(0.002, 0.004, 0.001);

    const float2 normalizedScreen = input.uv * 2.0 - 1.0;
    const float vignette = 1.0 - smoothstep(0.45, 1.35, dot(normalizedScreen, normalizedScreen));
    sceneColor *= lerp(0.42, 1.0, vignette);

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
    sceneColor += float3(0.01, 0.055, 0.16) * glassEdge;

    const float plinth = smoothstep(0.94, 0.985, input.uv.y);
    sceneColor = lerp(sceneColor, frameColor, plinth * 0.82);
    return float4(sceneColor, 1.0);
}
