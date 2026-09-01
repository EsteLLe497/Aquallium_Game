cbuffer FishConstants : register(b4)
{
    row_major float4x4 gViewProjection;
    float4 gCameraTime;
    // x: surface height, yzw: RGB absorption per metre.
    float4 gWaterParameters;
    // xyz: direction from receiver toward the broad front key, w: intensity.
    float4 gKeyLightDirectionIntensity;
    float4 gKeyLightColor;
    // rgb: puzzle palette, w: paired front-corner light intensity.
    float4 gSideLightColorIntensity;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float bendWeight : TEXCOORD1;
    float4 instancePositionScale : INSTANCE_POSITION_SCALE;
    float4 instanceForwardPhase : INSTANCE_FORWARD_PHASE;
    float4 instanceTintSwim : INSTANCE_TINT_SWIM;
    // x: species (0 schooler, 1 medium, 2 ray), yz: body shape.
    float4 instanceSpeciesShape : INSTANCE_SPECIES_SHAPE;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float3 tint : TEXCOORD2;
    float depthBelowSurface : TEXCOORD3;
    float tailMask : TEXCOORD4;
    float overheadSchool : TEXCOORD5;
    float species : TEXCOORD6;
};

float3 SafeNormalize(float3 value, float3 fallback)
{
    const float lengthSquared = dot(value, value);
    return lengthSquared > 1.0e-8
        ? value * rsqrt(lengthSquared)
        : fallback;
}

VertexOutput VSFish(VertexInput input)
{
    VertexOutput output;
    const float scale = input.instancePositionScale.w;
    const float phase = input.instanceForwardPhase.w;
    const float swimSpeed = abs(input.instanceTintSwim.w);
    const float wave = sin(
        gCameraTime.w * swimSpeed + phase - input.position.x * 5.2);
    float3 localPosition = input.position;
    const bool raySpecies = input.instanceSpeciesShape.x > 1.5;
    if (raySpecies)
    {
        const float wingMask = saturate(-input.bendWeight);
        localPosition.y += sin(
            gCameraTime.w * swimSpeed + phase +
            abs(input.position.z) * 0.85) * wingMask * 0.135;
    }
    else
    {
        localPosition.z += wave * 0.105 * saturate(input.bendWeight);
    }
    localPosition.x *= input.instanceSpeciesShape.y;
    localPosition.yz *= input.instanceSpeciesShape.z;

    const float3 forward = SafeNormalize(
        input.instanceForwardPhase.xyz,
        float3(1.0, 0.0, 0.0));
    const float3 referenceUp = abs(forward.y) < 0.92
        ? float3(0.0, 1.0, 0.0)
        : float3(0.0, 0.0, 1.0);
    const float3 right = SafeNormalize(
        cross(referenceUp, forward),
        float3(0.0, 0.0, 1.0));
    const float3 up = SafeNormalize(cross(forward, right), referenceUp);
    const float3 worldPosition =
        input.instancePositionScale.xyz +
        (forward * localPosition.x +
         up * localPosition.y +
         right * localPosition.z) * scale;
    output.worldPosition = worldPosition;
    output.worldNormal = SafeNormalize(
        forward * input.normal.x +
        up * input.normal.y +
        right * input.normal.z,
        up);
    output.tint = input.instanceTintSwim.rgb;
    output.depthBelowSurface = max(gWaterParameters.x - worldPosition.y, 0.0);
    output.tailMask = saturate(input.bendWeight);
    output.overheadSchool = input.instanceTintSwim.w < 0.0 ? 1.0 : 0.0;
    output.species = input.instanceSpeciesShape.x;
    output.position = mul(float4(worldPosition, 1.0), gViewProjection);
    return output;
}

float4 PSFish(VertexOutput input, bool frontFace : SV_IsFrontFace) : SV_TARGET0
{
    float3 normal = SafeNormalize(input.worldNormal, float3(0.0, 1.0, 0.0));
    if (!frontFace)
    {
        normal = -normal;
    }
    const float3 viewDirection = SafeNormalize(
        gCameraTime.xyz - input.worldPosition,
        float3(-1.0, 0.0, 0.0));
    const float3 lightDirection = SafeNormalize(
        gKeyLightDirectionIntensity.xyz,
        float3(0.0, 1.0, 0.0));
    const bool heroTankLighting = gSideLightColorIntensity.w > 0.0001;
    // The neutral overhead key falls off toward the rear wall. This deliberate
    // value separation makes overlapping schools readable as depth layers.
    const float frontDepth = saturate(
        1.0 - (input.worldPosition.x - 7.5) / 14.0);
    const float keyCoverage = heroTankLighting
        ? lerp(0.34, 1.0, frontDepth)
        : 1.0;
    const float diffuse = saturate(dot(normal, lightDirection)) *
        gKeyLightDirectionIntensity.w * keyCoverage;
    const float rim = pow(1.0 - saturate(dot(normal, viewDirection)), 2.1);
    const float flankFlash = pow(
        saturate(dot(reflect(-lightDirection, normal), viewDirection)),
        28.0);
    const bool raySpecies = input.species > 1.5;
    const float3 keyTint = lerp(
        float3(1.0, 1.0, 1.0),
        gKeyLightColor.rgb,
        heroTankLighting ? 0.72 : 0.0);
    const float3 baseColor = input.tint * keyTint *
        (raySpecies ? (0.15 + diffuse * 0.34) : (0.22 + diffuse * 0.52));
    const float3 silver = lerp(
        float3(0.24, 0.48, 0.72),
        gKeyLightColor.rgb,
        heroTankLighting ? 0.46 : 0.0) * flankFlash *
        (raySpecies ? 0.20 : 0.42);
    const float3 rimColor = float3(0.04, 0.22, 0.46) * rim * 0.42;
    // Two tiny sources at the front corners point toward the centre. Squared
    // footprints avoid sqrt and add no draw call or shadow map.
    const float2 leftDelta =
        (input.worldPosition.xz - float2(8.8, -10.5)) /
        float2(14.0, 11.0);
    const float2 rightDelta =
        (input.worldPosition.xz - float2(8.8, 10.5)) /
        float2(14.0, 11.0);
    float leftPool = saturate(1.0 - dot(leftDelta, leftDelta));
    float rightPool = saturate(1.0 - dot(rightDelta, rightDelta));
    leftPool *= leftPool;
    rightPool *= rightPool;
    // Fixed inward directions closely approximate the corner sources across
    // the distant fish silhouettes and avoid two normalizations per pixel.
    const float3 leftDirection = float3(-0.158, 0.573, -0.804);
    const float3 rightDirection = float3(-0.158, 0.573, 0.804);
    const float sideDiffuse =
        (saturate(dot(normal, leftDirection)) * leftPool +
         saturate(dot(normal, rightDirection)) * rightPool) *
        gSideLightColorIntensity.w;
    const float3 sideFill = input.tint *
        gSideLightColorIntensity.rgb * sideDiffuse * 0.36;
    const float3 transmission = exp(
        -gWaterParameters.yzw * input.depthBelowSurface);
    const float tailDarkening = lerp(1.0, 0.82, input.tailMask);
    const float rearDepthCue = heroTankLighting
        ? lerp(0.62, 1.0, frontDepth)
        : 1.0;
    float3 color =
        (baseColor + silver + rimColor + sideFill) *
        transmission * tailDarkening * rearDepthCue;
    // Fish above the acrylic are intentionally read from below as silhouettes,
    // providing a cheap fish-shadow cue without a second shadow-map pass.
    const float viewedFromBelow =
        saturate(input.overheadSchool + (raySpecies ? 0.72 : 0.0)) * smoothstep(
        0.35,
        1.8,
        input.worldPosition.y - gCameraTime.y);
    const float3 silhouette =
        float3(0.006, 0.025, 0.055) +
        float3(0.025, 0.12, 0.25) * rim;
    color = lerp(
        color,
        raySpecies ? silhouette * float3(0.18, 0.27, 0.38) : silhouette,
        viewedFromBelow * (raySpecies ? 0.94 : 0.88));
    return float4(color, 1.0);
}
