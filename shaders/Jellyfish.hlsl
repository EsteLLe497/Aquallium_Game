cbuffer JellyfishConstants : register(b3)
{
    row_major float4x4 gCurrentViewProjection;
    row_major float4x4 gPreviousViewProjection;
    float4 gCameraTime;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float part : TEXCOORD1;
    float4 instancePositionScale : INSTANCE_POSITION_SCALE;
    float4 instanceTintPhase : INSTANCE_TINT_PHASE;
    float4 instanceMotion : INSTANCE_MOTION;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 tint : TEXCOORD3;
    float part : TEXCOORD4;
    float4 currentClip : TEXCOORD5;
    float4 previousClip : TEXCOORD6;
};

float3 AnimatedCenter(VertexInput input, float time)
{
    const float phase = input.instanceTintPhase.w;
    return input.instancePositionScale.xyz + float3(
        sin(time * input.instanceMotion.x + phase) * 0.105,
        sin(time * input.instanceMotion.y + phase * 1.31) * 0.14,
        cos(time * input.instanceMotion.z + phase * 0.87) * 0.095);
}

float3 SafeDirection(float3 value, float3 fallback)
{
    const float lengthSquared = dot(value, value);
    return lengthSquared > 1.0e-8
        ? value * rsqrt(lengthSquared)
        : fallback;
}

float3 BuildHorizontalBillboardRight(float3 center)
{
    const float2 toCamera = gCameraTime.xz - center.xz;
    const float lengthSquared = dot(toCamera, toCamera);
    if (lengthSquared <= 1.0e-8)
    {
        return float3(1.0, 0.0, 0.0);
    }
    const float2 direction = toCamera * rsqrt(lengthSquared);
    return float3(direction.y, 0.0, -direction.x);
}

VertexOutput VSJellyfish(VertexInput input)
{
    VertexOutput output;
    const float time = gCameraTime.w;
    const float phase = input.instanceTintPhase.w;
    const float scale = input.instancePositionScale.w;
    const float pulse =
        sin(time * input.instanceMotion.w + phase) * 0.5 + 0.5;
    const float contraction = smoothstep(0.58, 0.94, pulse);
    float3 localPosition = input.position;
    float3 localNormal = input.normal;

    if (input.part < 0.5)
    {
        localPosition.xz *= 1.0 - contraction * 0.13;
        localPosition.y *= 1.0 + contraction * 0.12;
    }
    else if (input.part < 1.5)
    {
        const float strandWave =
            sin(time * 0.72 + phase + input.uv.y * 7.5);
        const float crossWave =
            sin(time * 0.47 + phase * 1.7 + input.uv.y * 4.8);
        localPosition.x += strandWave * 0.055 * input.uv.y;
        localPosition.z += crossWave * 0.040 * input.uv.y;
        const float3 center = AnimatedCenter(input, time);
        const float3 toCamera = SafeDirection(
            gCameraTime.xyz - center,
            float3(0.0, 0.0, -1.0));
        const float3 ribbonRight =
            BuildHorizontalBillboardRight(center);
        localPosition += ribbonRight *
            (input.uv.x * 2.0 - 1.0) *
            lerp(0.025, 0.010, input.uv.y);
        localNormal = -toCamera;
    }
    else
    {
        const float3 center = AnimatedCenter(input, time);
        const float3 toCamera = SafeDirection(
            gCameraTime.xyz - center,
            float3(0.0, 0.0, -1.0));
        const float3 right = BuildHorizontalBillboardRight(center);
        const float3 up = SafeDirection(
            cross(toCamera, right),
            float3(0.0, 1.0, 0.0));
        localPosition =
            right * input.position.x +
            up * input.position.y;
        localNormal = -toCamera;
    }

    const float3 currentCenter = AnimatedCenter(input, time);
    const float3 previousCenter = AnimatedCenter(input, time - 1.0 / 60.0);
    const float3 worldPosition = currentCenter + localPosition * scale;
    const float3 previousPosition = previousCenter + localPosition * scale;
    output.worldPosition = worldPosition;
    output.normal = SafeDirection(
        localNormal,
        float3(0.0, 1.0, 0.0));
    output.uv = input.uv;
    output.tint = input.instanceTintPhase.rgb;
    output.part = input.part;
    output.currentClip = mul(
        float4(worldPosition, 1.0), gCurrentViewProjection);
    output.previousClip = mul(
        float4(previousPosition, 1.0), gPreviousViewProjection);
    output.position = output.currentClip;
    return output;
}

struct PixelOutput
{
    float4 color : SV_TARGET0;
    float depth : SV_TARGET1;
    float2 motion : SV_TARGET2;
};

float2 ClipToUv(float4 clipPosition)
{
    const float2 ndc = clipPosition.xy / max(clipPosition.w, 0.0001);
    return float2(ndc.x * 0.5 + 0.5, -ndc.y * 0.5 + 0.5);
}

PixelOutput PSJellyfish(VertexOutput input)
{
    const float3 viewDirection = SafeDirection(
        input.worldPosition - gCameraTime.xyz,
        float3(0.0, 0.0, 1.0));
    const float3 surfaceNormal = SafeDirection(
        input.normal,
        float3(0.0, 1.0, 0.0));
    const float facing = saturate(dot(-viewDirection, surfaceNormal));
    float alpha;
    float3 color;
    if (input.part < 0.5)
    {
        const float fresnel = pow(1.0 - facing, 3.2);
        const float centerGlow =
            pow(saturate(1.0 - input.uv.y), 1.7);
        color =
            input.tint * (0.13 + centerGlow * 0.36) +
            float3(0.10, 0.34, 0.78) * fresnel;
        alpha = 0.16 + centerGlow * 0.13 + fresnel * 0.22;
    }
    else if (input.part < 1.5)
    {
        const float ribbon = 1.0 - abs(input.uv.x * 2.0 - 1.0);
        const float taper = pow(1.0 - input.uv.y, 0.55);
        alpha = ribbon * taper * 0.25;
        color = input.tint * (0.14 + taper * 0.17);
    }
    else
    {
        const float2 centered = input.uv * 2.0 - 1.0;
        const float circle = saturate(1.0 - dot(centered, centered));
        alpha = circle * circle * 0.28;
        color = input.tint * 0.38;
    }
    if (any(isnan(color)) || any(isinf(color)) ||
        isnan(alpha) || isinf(alpha))
    {
        discard;
    }

    PixelOutput output;
    output.color = float4(color, alpha);
    output.depth = length(input.worldPosition - gCameraTime.xyz);
    const float2 currentUv = ClipToUv(input.currentClip);
    const float2 previousUv = ClipToUv(input.previousClip);
    output.motion = previousUv - currentUv;
    return output;
}
