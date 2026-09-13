cbuffer BeachConstants : register(b0)
{
    float gTime;
    float gWidth;
    float gHeight;
    float gYaw;
    float gPitch;
    float3 gCamera;
    float gDayBlend;
    float gWakeBlur;
    float gWakeBlink;
    float gEnvironmentPadding;
};

struct VSOutput { float4 position:SV_POSITION; float2 uv:TEXCOORD0; };
VSOutput VSMain(uint id:SV_VertexID)
{
    VSOutput o;float2 p=float2((id<<1)&2,id&2);
    o.position=float4(p*float2(2,-2)+float2(-1,1),0,1);o.uv=p;return o;
}
Texture2D<float4> gBeachScene:register(t0);
SamplerState gLinearSampler:register(s0);

static const float PI=3.14159265359;
float Hash11(float p){return frac(sin(p*127.1)*43758.5453);}
float Hash21(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
float Noise(float2 p)
{
    float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);
    return lerp(lerp(Hash21(i),Hash21(i+float2(1,0)),f.x),
                lerp(Hash21(i+float2(0,1)),Hash21(i+1),f.x),f.y);
}
float Fbm(float2 p)
{
    float n=0,a=.5;[unroll]for(int i=0;i<4;++i){n+=Noise(p)*a;p=mul(float2x2(1.62,1.18,-1.18,1.62),p);a*=.48;}return n;
}
float3 MoonDirection(){return normalize(float3(-.64,.43,.62));}
// Late-morning/noon sun: high enough for a clear daytime read while remaining
// inside the beach camera's vertical field of view.
float3 SunDirection(){return normalize(float3(-.55,.50,.67));}

float3 NightSky(float3 rd)
{
    float horizon=exp(-abs(rd.y)*7.5);
    float zenith=saturate(rd.y*.78+.18);
    float3 color=lerp(float3(.0008,.0025,.010),float3(.0025,.008,.032),pow(zenith,.55));
    color+=float3(.006,.012,.032)*horizon;
    float3 moonDir=MoonDirection();float moonDot=dot(rd,moonDir);
    float2 starUv=float2(atan2(rd.z,rd.x)/(2*PI)+.5,asin(clamp(rd.y,-1,1))/PI+.5);
    float2 starGrid=starUv*float2(520,260),starCell=floor(starGrid);
    float starSeed=Hash21(starCell);
    float2 starOffset=float2(Hash21(starCell+17.2),Hash21(starCell+53.7));
    float starPoint=1-smoothstep(.020,.125,length(frac(starGrid)-starOffset));
    float moonStarClear=1-smoothstep(.988,.997,moonDot);
    float2 starGridB=(starUv+float2(.137,.071))*float2(337,173);
    float2 starCellB=floor(starGridB);
    float starSeedB=Hash21(starCellB+29.4);
    float2 starOffsetB=float2(Hash21(starCellB+71.1),Hash21(starCellB+113.8));
    float starPointB=1-smoothstep(.025,.14,length(frac(starGridB)-starOffsetB));
    float star=(step(.9805,starSeed)*starPoint+
        step(.986,starSeedB)*starPointB*.68)*moonStarClear;
    float twinkle=.78+.22*sin(gTime*(1.1+Hash21(starCell+91.3)*1.7)+starSeed*19);
    color+=star*twinkle*float3(.82,.91,1.18)*smoothstep(-.02,.14,rd.y);

    float3 moonRight=normalize(cross(float3(0,1,0),moonDir));
    float3 moonUp=cross(moonDir,moonRight);
    float2 moonUv=float2(dot(rd,moonRight),dot(rd,moonUp))/max(moonDot,.001);
    float moonRadius=.055,moonDistance=length(moonUv);
    float disk=1-smoothstep(moonRadius*.94,moonRadius,moonDistance);
    [branch]if(moonDistance<moonRadius){
        float crater=Fbm(moonUv*165+float2(7.2,13.7));
        float limb=sqrt(saturate(1-moonDistance*moonDistance/(moonRadius*moonRadius)));
        float2 lunar=moonUv/moonRadius;
        float craterA=1-smoothstep(.13,.18,length(lunar-float2(-.28,.18)));
        float craterB=1-smoothstep(.075,.12,length(lunar-float2(.31,-.24)));
        float craterC=1-smoothstep(.05,.085,length(lunar-float2(.10,.38)));
        float craterShadow=saturate(craterA*.34+craterB*.26+craterC*.22+(1-smoothstep(.38,.62,crater))*.13);
        float3 lunarColor=lerp(float3(.58,.67,.82),float3(1.42,1.55,1.68),limb);
        color+=disk*lunarColor*(1-craterShadow);
    }
    color+=float3(.23,.36,.88)*exp(-moonDistance*moonDistance*190)*.90;

    float cloudDomain=max(rd.y+.28,.08);
    float2 cloudUv=rd.xz/cloudDomain*1.7+float2(gTime*.006,-gTime*.003);
    float clouds=smoothstep(.55,.76,Fbm(cloudUv));
    clouds*=smoothstep(-.03,.18,rd.y)*smoothstep(.72,.10,rd.y)*(1-disk);
    color=lerp(color,color+float3(.022,.035,.090),clouds*.45);
    return color;
}

float3 MorningSky(float3 rd)
{
    float height=saturate(rd.y),horizon=exp(-abs(rd.y)*5.0);
    float3 color=lerp(float3(.20,.43,.68),float3(.025,.18,.53),pow(height,.48));
    color+=float3(.16,.28,.34)*horizon;
    float sunDot=dot(rd,SunDirection());
    float sunDisk=smoothstep(.9991,.99955,sunDot);
    float sunHalo=pow(saturate(sunDot),82);
    color+=float3(1.20,1.10,.88)*(sunDisk*2.35+sunHalo*.38);
    float domain=max(rd.y+.24,.07);
    float2 uv=rd.xz/domain*1.5+float2(gTime*.004,-gTime*.002);
    float clouds=smoothstep(.53,.76,Fbm(uv))*smoothstep(-.02,.18,rd.y)*smoothstep(.78,.16,rd.y);
    float litSide=saturate(dot(normalize(rd.xz),normalize(SunDirection().xz))*.5+.5);
    color=lerp(color,lerp(float3(.40,.58,.78),float3(.93,.92,.86),litSide),clouds*.46);
    return color;
}

float3 EnvironmentSky(float3 rd){return lerp(NightSky(rd),MorningSky(rd),gDayBlend);}

float Shore(float z){return .25+sin(z*.15)*.65+sin(z*.043+1.3)*1.05;}
float OceanHeight(float2 p)
{
    float shoreDistance=p.x-Shore(p.y);float depth=saturate(-shoreDistance/4.5);
    float swell=sin(p.x*.42+p.y*.67+gTime*.72)*.075+sin(p.x*-.83+p.y*.31-gTime*.93)*.045;
    float chop=sin(p.x*1.75+p.y*1.21+gTime*1.42)*.018;
    float breakerPhase=p.x*1.42+p.y*.16-gTime*1.55;
    float breaker=pow(saturate(sin(breakerPhase)*.5+.5),7)*.28*exp(-pow((shoreDistance+.72)*.72,2));
    return .015+(swell+chop)*(.20+.80*depth)+breaker;
}
float SandHeight(float2 p)
{
    float dune=smoothstep(5.15,8.2,p.x);float ripple=sin(p.y*2.4+p.x*.7)+sin(p.y*5.3-p.x*1.6)*.45;
    return .06+dune*dune*.68+ripple*.008;
}
bool TraceHeight(float3 ro,float3 rd,bool water,out float3 hit,out float distance)
{
    distance=(.06-ro.y)/rd.y;if(distance<=0||distance>180)return false;
    [unroll]for(int i=0;i<3;++i){hit=ro+rd*distance;float h=water?OceanHeight(hit.xz):SandHeight(hit.xz);distance+=(h-hit.y)/rd.y;}
    hit=ro+rd*distance;return distance>0&&distance<180;
}
float3 SurfaceNormal(float2 p,bool water)
{
    const float e=.035;float h=water?OceanHeight(p):SandHeight(p);
    float hx=water?OceanHeight(p+float2(e,0)):SandHeight(p+float2(e,0));
    float hz=water?OceanHeight(p+float2(0,e)):SandHeight(p+float2(0,e));
    return normalize(float3(h-hx,e,h-hz));
}
float3 ReflectedSky(float3 rd)
{
    float zenith=saturate(rd.y*.8+.2);float3 color=lerp(float3(.006,.018,.05),float3(.012,.042,.12),zenith);
    float moon=pow(saturate(dot(rd,MoonDirection())),360);float halo=pow(saturate(dot(rd,MoonDirection())),36);
    float3 night=color+float3(.75,.90,1.15)*(moon*2.2+halo*.12);
    float sun=pow(saturate(dot(rd,SunDirection())),420),sunHalo=pow(saturate(dot(rd,SunDirection())),34);
    float3 morning=lerp(float3(.07,.25,.38),float3(.32,.62,.82),zenith)+
        float3(1.18,1.06,.82)*(sun*3.1+sunHalo*.18);
    return lerp(night,morning,gDayBlend);
}

float3 Water(float3 p,float3 rd)
{
    float t=gTime;float3 n=SurfaceNormal(p.xz,true);float3 reflected=reflect(rd,n);
    float fresnel=.035+.965*pow(1-saturate(dot(-rd,n)),5);
    float3 sky=ReflectedSky(reflected);
    float distance=length(p.xz-gCamera.xz);
    float3 nightDeep=lerp(float3(.006,.035,.085),float3(.018,.095,.19),exp(-distance*.025));
    float3 morningDeep=lerp(float3(.025,.21,.30),float3(.075,.39,.48),exp(-distance*.025));
    float3 deep=lerp(nightDeep,morningDeep,gDayBlend);
    float3 color=lerp(deep,sky,fresnel*.82+.10);
    float moonSpec=pow(saturate(dot(reflected,MoonDirection())),420);
    float moonPath=pow(saturate(dot(reflected,MoonDirection())),42);
    float breakup=smoothstep(.30,.92,Noise(p.xz*3.7+float2(t*.42,-t*.18)));
    color+=float3(.52,.72,1.15)*(moonSpec*3.5+moonPath*.32)*(.25+breakup*1.45)*(1-gDayBlend);
    float sunSpec=pow(saturate(dot(reflected,SunDirection())),460);
    float sunPath=pow(saturate(dot(reflected,SunDirection())),34);
    color+=float3(1.18,1.03,.74)*(sunSpec*4.0+sunPath*.42)*(.20+breakup*1.35)*gDayBlend;
    float shoreDistance=p.x-Shore(p.z);
    float crest=saturate((OceanHeight(p.xz)-.10)*5.8);
    float waveBand=sin(shoreDistance*5.2-p.z*.16+t*2.1)+sin(shoreDistance*9.1+t*2.8)*.35;
    float foam=smoothstep(.58,.92,waveBand)*smoothstep(-3.2,-.15,shoreDistance)*smoothstep(.35,-.25,shoreDistance);
    float foamBreakup=.28+.72*smoothstep(.30,.73,Noise(float2(p.z*1.15-t*.35,shoreDistance*5.8)));
    foam*=foamBreakup;
    foam+=smoothstep(.18,.015,abs(shoreDistance+sin(p.z*.34+t)*.12))*foamBreakup*.65+crest*crest*.72;
    return lerp(color,lerp(float3(.70,.85,1),float3(.96,.98,1),gDayBlend),saturate(foam));
}

float3 Sand(float3 p,float3 rd)
{
    float shoreDistance=p.x-Shore(p.z);float3 sandNormal=SurfaceNormal(p.xz,false);
    float ripple=sin(p.z*2.4+p.x*.7)*.5+sin(p.z*5.3-p.x*1.6)*.25;
    float grain=Noise(p.xz*7.5)*.5+Noise(p.xz*22)*.5;
    float3 dry=float3(.69,.72,.73)*(1+ripple*.025+grain*.035);
    float wet=smoothstep(2.5,.15,shoreDistance);
    float3 color=lerp(dry,float3(.20,.27,.34),wet*.72);
    float3 moonDir=MoonDirection();float3 nightColor=color*(.48+.52*saturate(dot(sandNormal,moonDir)));
    float3 morningColor=lerp(float3(.94,.92,.84),float3(.38,.44,.45),wet*.55)*
        (.72+.46*saturate(dot(sandNormal,SunDirection())));
    color=lerp(nightColor,morningColor,gDayBlend);
    float3 beachLightDirection=normalize(lerp(moonDir,SunDirection(),gDayBlend));
    float wetGlint=pow(saturate(dot(reflect(rd,sandNormal),beachLightDirection)),90)*wet;
    color+=lerp(float3(.28,.42,.72),float3(1.0,.94,.76),gDayBlend)*wetGlint;
    float foamNoise=.25+.75*smoothstep(.32,.72,Noise(float2(p.z*1.3-gTime*.45,shoreDistance*8)));
    float foamLine=smoothstep(.20,.025,abs(shoreDistance+sin(p.z*.32+gTime*1.1)*.10))*foamNoise;
    color=lerp(color,float3(.78,.86,.92),foamLine*.58);
    float inlandEdge=4.95+(Noise(float2(p.z*.34,p.x*.19))-.5)*.85;
    float inlandDark=smoothstep(inlandEdge,inlandEdge+2.45,p.x);
    color=lerp(color,lerp(float3(.003,.008,.011),float3(.018,.038,.024),gDayBlend),inlandDark*.92);
    return color;
}

float3 ShoreSpray(float3 ro,float3 rd,float maximum)
{
    float3 spray=0;[unroll]for(int i=0;i<6;++i){
        float z=-11+i*4.4;float seed=Hash11(i*7.13);float phase=frac(gTime*(.38+seed*.12)+seed);
        float3 centre=float3(Shore(z)-.32+sin(phase*6.28)*.12,
            .12+sin(phase*PI)*(.32+seed*.30),z+(Hash11(i+3.4)-.5)*1.3);
        float along=dot(centre-ro,rd);float3 nearest=ro+rd*along;
        float radius=lerp(.075,.018,phase);float particle=1-smoothstep(radius*.35,radius,length(nearest-centre));
        particle*=step(0,along)*step(along,maximum)*smoothstep(1,.78,phase);
        spray+=particle*float3(.58,.78,1.0)*(1.2-phase*.5);
    }
    return spray;
}

float4 PSMain(VSOutput input):SV_TARGET
{
    float2 q=input.position.xy/float2(gWidth,gHeight)*2-1;q.y=-q.y;
    float cp=cos(gPitch),sp=sin(gPitch),cy=cos(gYaw),sy=sin(gYaw);
    float3 forward=float3(sy*cp,sp,cy*cp),right=float3(cy,0,-sy),up=normalize(cross(forward,right));
    float3 rd=normalize(forward+right*q.x*(gWidth/gHeight)*.69+up*q.y*.69);
    float3 color=0;
    float groundT=rd.y<-.0005?(.06-gCamera.y)/rd.y:1e4;float sceneDistance=1e4;
    [branch]if(groundT>0&&groundT<180){
        float3 planePoint=gCamera+rd*groundT;bool water=planePoint.x<Shore(planePoint.z);float3 p;
        if(TraceHeight(gCamera,rd,water,p,sceneDistance)){
            // A low, grazing wake-up camera can cross the curved shoreline
            // between the flat-plane guess and the displaced surface hit.
            // Re-trace only mismatched pixels so wet sand cannot stretch into
            // the large triangular bands seen from the washed-ashore pose.
            bool hitWater=p.x<Shore(p.z);
            [branch]if(hitWater!=water){
                water=hitWater;
                TraceHeight(gCamera,rd,water,p,sceneDistance);
                water=p.x<Shore(p.z);
            }
            color=water?Water(p,rd):Sand(p,rd);
            float haze=saturate((sceneDistance-65)/100);color=lerp(color,EnvironmentSky(normalize(float3(rd.x,.015,rd.z))),haze*.7);
        }else color=EnvironmentSky(rd);
    } else {
        color=EnvironmentSky(rd);
        float oceanFacing=saturate(-rd.x*.8+.2);float ridgeHeight=.015+.035*Fbm(float2(atan2(rd.z,rd.x)*5,2));
        float ridge=(1-smoothstep(ridgeHeight,ridgeHeight+.008,rd.y))*smoothstep(-.04,.015,rd.y)*oceanFacing;
        color=lerp(color,lerp(float3(.005,.014,.026),float3(.055,.105,.105),gDayBlend),ridge);
        float inlandFacing=smoothstep(.16,.68,rd.x);
        float canopyHeight=.075+.065*Fbm(float2(atan2(rd.z,rd.x)*7.5,4.1));
        float canopy=(1-smoothstep(canopyHeight,canopyHeight+.018,rd.y))*
            smoothstep(-.045,.012,rd.y)*inlandFacing;
        color=lerp(color,lerp(float3(.0005,.0025,.0035),float3(.008,.026,.014),gDayBlend),canopy*.96);
        color*=1-inlandFacing*exp(-max(rd.y,0)*7.5)*lerp(.42,.20,gDayBlend);
    }
    color+=ShoreSpray(gCamera,rd,sceneDistance);
    float vignette=1-.16*dot(q,q);color*=saturate(vignette);
    color=1-exp(-color*1.35);color=pow(saturate(color),1/2.2);
    return float4(color,1);
}

float4 PSUpscale(VSOutput input):SV_TARGET
{
    float2 texel=1/float2(gWidth,gHeight);float3 center=gBeachScene.SampleLevel(gLinearSampler,input.uv,0).rgb;
    float3 neighbours=gBeachScene.SampleLevel(gLinearSampler,input.uv+float2(texel.x,0),0).rgb+
        gBeachScene.SampleLevel(gLinearSampler,input.uv-float2(texel.x,0),0).rgb+
        gBeachScene.SampleLevel(gLinearSampler,input.uv+float2(0,texel.y),0).rgb+
        gBeachScene.SampleLevel(gLinearSampler,input.uv-float2(0,texel.y),0).rgb;
    float3 color=saturate(center*1.30-neighbours*.075);
    [branch]if(gWakeBlur>.001){
        float2 spread=texel*lerp(1.5,9.0,gWakeBlur);
        float3 blurred=center*2;
        blurred+=gBeachScene.SampleLevel(gLinearSampler,input.uv+float2(spread.x,0),0).rgb;
        blurred+=gBeachScene.SampleLevel(gLinearSampler,input.uv-float2(spread.x,0),0).rgb;
        blurred+=gBeachScene.SampleLevel(gLinearSampler,input.uv+float2(0,spread.y),0).rgb;
        blurred+=gBeachScene.SampleLevel(gLinearSampler,input.uv-float2(0,spread.y),0).rgb;
        blurred+=gBeachScene.SampleLevel(gLinearSampler,input.uv+spread,0).rgb;
        blurred+=gBeachScene.SampleLevel(gLinearSampler,input.uv-spread,0).rgb;
        blurred+=gBeachScene.SampleLevel(gLinearSampler,input.uv+float2(spread.x,-spread.y),0).rgb;
        blurred+=gBeachScene.SampleLevel(gLinearSampler,input.uv+float2(-spread.x,spread.y),0).rgb;
        color=lerp(color,blurred*.1,gWakeBlur);
    }
    // Two eyelids close toward the centre instead of using a flat black fade.
    float vertical=abs(input.uv.y-.5)*2;
    float aperture=lerp(1.12,.015,saturate(gWakeBlink));
    float eyelid=smoothstep(aperture-.075,aperture+.025,vertical);
    color*=1-eyelid;
    return float4(color,1);
}
