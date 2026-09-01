#ifndef AQUARIUM_LOCAL_LIGHTING_HLSLI
#define AQUARIUM_LOCAL_LIGHTING_HLSLI

cbuffer LocalLightingConstants : register(b3)
{
    float4 gLocalLightPositionRange[8];
    float4 gLocalLightDirectionType[8];
    float4 gLocalLightColorIntensity[8];
    float4 gLocalLightConeEnabled[8];
    // x: active count, y: local-light system enabled
    float4 gLocalLightControl;
    float4 gAmbientColorStrength;
    float4 gTankBounceCenterRange;
    float4 gTankBounceNormalHalfWidth;
    float4 gTankBounceColorIntensity;
    float4 gAtmosphereColorDensity;
    // x: tank half-height, y: bounce enabled, z: atmosphere start,
    // w: maximum atmosphere blend
    float4 gHybridLightingControl;
};

float3 EvaluateLocalLighting(float3 worldPosition, float3 normal)
{
    const uint lightCount = min((uint)gLocalLightControl.x, 8u);
    if (gLocalLightControl.y < 0.5 || lightCount == 0u)
    {
        return 0.0;
    }

    float3 result = 0.0;
    [loop]
    for (uint index = 0; index < lightCount; ++index)
    {
        const float3 toLight =
            gLocalLightPositionRange[index].xyz - worldPosition;
        const float distanceSquared = dot(toLight, toLight);
        const float range = gLocalLightPositionRange[index].w;
        const float rangeSquared = range * range;
        if (distanceSquared >= rangeSquared)
        {
            continue;
        }
        const float inverseDistance = rsqrt(max(distanceSquared, 0.0025));
        const float3 lightDirection = toLight * inverseDistance;
        const float normalizedDistanceSquared = distanceSquared / rangeSquared;
        const float rangeFade = saturate(1.0 - normalizedDistanceSquared);
        const float attenuation = rangeFade * rangeFade /
            max(distanceSquared, 0.25);
        float cone = 1.0;
        if (gLocalLightDirectionType[index].w > 0.5)
        {
            const float3 spotAxis = normalize(
                gLocalLightDirectionType[index].xyz);
            const float cosine = dot(-lightDirection, spotAxis);
            cone = smoothstep(
                gLocalLightConeEnabled[index].y,
                gLocalLightConeEnabled[index].x,
                cosine);
        }
        const float ndotl = saturate(dot(normal, lightDirection));
        const float wrapDiffuse = saturate((ndotl + 0.12) / 1.12);
        result += gLocalLightColorIntensity[index].rgb *
            gLocalLightColorIntensity[index].w *
            attenuation * cone * wrapDiffuse;
    }
    return result;
}

float3 EvaluateAmbientLighting(float3 normal)
{
    const float hemisphere = lerp(0.36, 1.0, saturate(normal.y * 0.5 + 0.5));
    return gAmbientColorStrength.rgb *
        gAmbientColorStrength.w * hemisphere;
}

float3 EvaluateTankBounce(float3 worldPosition, float3 normal)
{
    if (gHybridLightingControl.y < 0.5)
    {
        return 0.0;
    }
    const float3 sourceNormal = normalize(gTankBounceNormalHalfWidth.xyz);
    const float3 worldUp = float3(0.0, 1.0, 0.0);
    float3 horizontalAxis = cross(worldUp, sourceNormal);
    horizontalAxis *= rsqrt(max(dot(horizontalAxis, horizontalAxis), 0.0001));
    const float3 sourceCenter = gTankBounceCenterRange.xyz;
    const float3 relative = worldPosition - sourceCenter;
    const float horizontal = clamp(
        dot(relative, horizontalAxis),
        -gTankBounceNormalHalfWidth.w,
        gTankBounceNormalHalfWidth.w);
    const float vertical = clamp(
        dot(relative, worldUp),
        -gHybridLightingControl.x,
        gHybridLightingControl.x);
    const float3 closestSourcePoint =
        sourceCenter + horizontalAxis * horizontal + worldUp * vertical;
    const float3 fromSource = worldPosition - closestSourcePoint;
    const float distanceSquared = dot(fromSource, fromSource);
    const float range = max(gTankBounceCenterRange.w, 0.01);
    if (distanceSquared >= range * range)
    {
        return 0.0;
    }
    const float inverseDistance = rsqrt(max(distanceSquared, 0.04));
    const float3 sourceToReceiver = fromSource * inverseDistance;
    const float sourceFacing = saturate(dot(sourceNormal, sourceToReceiver));
    const float receiverFacing = saturate(dot(normal, -sourceToReceiver) * 0.72 + 0.28);
    const float rangeFade = saturate(1.0 - distanceSquared / (range * range));
    // Broad aquarium windows behave closer to an area source than a point;
    // this softened falloff avoids a fake circular pool on the floor.
    const float attenuation = rangeFade * rangeFade /
        (1.0 + distanceSquared * 0.055);
    return gTankBounceColorIntensity.rgb *
        gTankBounceColorIntensity.w * attenuation *
        sourceFacing * receiverFacing;
}

float3 ApplyDryAtmosphere(float3 color, float cameraDistance)
{
    if (gAtmosphereColorDensity.w <= 0.0)
    {
        return color;
    }
    const float fogDistance = max(
        cameraDistance - gHybridLightingControl.z, 0.0);
    const float fog = min(
        1.0 - exp(-fogDistance * gAtmosphereColorDensity.w),
        gHybridLightingControl.w);
    return lerp(color, gAtmosphereColorDensity.rgb, fog);
}

#endif
