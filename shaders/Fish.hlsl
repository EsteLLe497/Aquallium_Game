cbuffer FishConstants : register(b4)
{
    row_major float4x4 gViewProjection;
    float4 gCameraTime;
    // x: surface height, yzw: RGB absorption per metre.
    float4 gWaterParameters;
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
    localPosition.z += wave * 0.105 * input.bendWeight;

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
    output.tailMask = input.bendWeight;
    output.overheadSchool = input.instanceTintSwim.w < 0.0 ? 1.0 : 0.0;
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
        float3(-0.24, 0.92, -0.18),
        float3(0.0, 1.0, 0.0));
    const float diffuse = saturate(dot(normal, lightDirection));
    const float rim = pow(1.0 - saturate(dot(normal, viewDirection)), 2.1);
    const float flankFlash = pow(
        saturate(dot(reflect(-lightDirection, normal), viewDirection)),
        28.0);
    const float3 baseColor = input.tint * (0.22 + diffuse * 0.52);
    const float3 silver = float3(0.24, 0.48, 0.72) * flankFlash * 0.42;
    const float3 rimColor = float3(0.04, 0.22, 0.46) * rim * 0.42;
    const float3 transmission = exp(
        -gWaterParameters.yzw * input.depthBelowSurface);
    const float tailDarkening = lerp(1.0, 0.82, input.tailMask);
    float3 color = (baseColor + silver + rimColor) * transmission * tailDarkening;
    // Fish above the acrylic are intentionally read from below as silhouettes,
    // providing a cheap fish-shadow cue without a second shadow-map pass.
    const float viewedFromBelow = input.overheadSchool * smoothstep(
        0.35,
        1.8,
        input.worldPosition.y - gCameraTime.y);
    const float3 silhouette =
        float3(0.006, 0.025, 0.055) +
        float3(0.025, 0.12, 0.25) * rim;
    color = lerp(color, silhouette, viewedFromBelow * 0.88);
    return float4(color, 1.0);
}
