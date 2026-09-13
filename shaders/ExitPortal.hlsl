cbuffer ExitPortalConstants : register(b0)
{
    float gProgress;
    float gBeachSide;
    float gTime;
    float gAspect;
};

struct VSOutput {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
VSOutput VSMain(uint id:SV_VertexID)
{
    VSOutput o;float2 p=float2((id<<1)&2,id&2);
    o.position=float4(p*float2(2,-2)+float2(-1,1),0,1);o.uv=p;return o;
}

float Hash21(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float4 PSMain(VSOutput input):SV_TARGET
{
    // Once the beach has replaced the aquarium, hold full exposure and let it
    // recover smoothly. No aperture is visible on the destination side.
    if(gBeachSide>.5){
        float alpha=1-smoothstep(0,1,gProgress);
        return float4(1,.995,.98,alpha);
    }

    float2 p=input.uv*2-1;p.x*=gAspect;
    float open=gProgress*gProgress*(3-2*gProgress);
    // The first visible light is the seam between the two door leaves. The
    // opening widens non-linearly like a real motor-driven double door.
    float halfWidth=lerp(.0025,gAspect*.64,pow(open,1.55));
    // The first frame is a full-height seam. It then expands equally left and
    // right, matching the pair of sliding emergency-door leaves.
    float distance=abs(p.x)-halfWidth;
    float core=1-smoothstep(-.004,.010,distance);
    float outside=max(distance,0);
    float nearBloom=exp(-outside*lerp(42,7,open));
    float farBloom=exp(-outside*lerp(13,2.1,open));

    float rayNoise=Hash21(float2(floor((p.y+1)*13),17.3));
    float rayShape=pow(saturate(.55+.28*cos(p.y*19+rayNoise*12)+.17*cos(p.y*43-rayNoise*7)),7);
    float rays=rayShape*exp(-abs(p.x)*1.35)*(1-core)*smoothstep(.08,.62,open);
    rays*=.68+.32*sin(gTime*.7+rayNoise*12);

    // Simulate camera exposure climbing as the portal occupies the frame.
    float exposureWash=pow(saturate((open-.54)/.46),1.35);
    float alpha=saturate(core+nearBloom*.46+farBloom*.20+rays*.13+exposureWash);
    float3 edgeColor=lerp(float3(.72,.86,1.0),float3(1.0,.995,.965),
        saturate(core+nearBloom*.55+exposureWash));
    return float4(edgeColor,alpha);
}
