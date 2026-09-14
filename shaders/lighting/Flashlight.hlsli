// One camera-mounted spotlight; no shadow map or additional rendering pass.
// Shared by stage, fish and jellyfish so no emissive aquarium light survives.
cbuffer FlashlightConstants : register(b5)
{
    float4 gFlashlightPosition; // xyz origin, w switched on
    float4 gFlashlightDirection; // xyz unit direction, w power outage blend
    // x: inscription, y: fixed handprints
    float4 gBlackoutWriting;
};

float3 FlashlightColor(float3 position, float3 normal, float3 albedo)
{
    float3 ray = position - gFlashlightPosition.xyz;
    float distanceSquared = max(dot(ray, ray), .0001);
    float3 direction = ray * rsqrt(distanceSquared);
    float angle = dot(direction, gFlashlightDirection.xyz);
    float cone = smoothstep(.88, .945, angle);
    float hotspot = smoothstep(.965, .998, angle);
    float range = 1 - smoothstep(14, 24, sqrt(distanceSquared));
    float diffuse = saturate(dot(normal, -direction));
    return albedo * float3(1, .97, .88) * diffuse * cone *
        (1.5 + hotspot * .8) * range / (1 + distanceSquared * .025) *
        gFlashlightPosition.w;
}
