// Offscreen regression capture for the post-chase blackout presentation.
#include "../src/rendering/AquariumRenderer.h"
#include <fstream>
#include <iostream>
using Microsoft::WRL::ComPtr;

int main(){try{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,&level,&context)))return 2;
    constexpr UINT w=960,h=540;D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;
    d.ArraySize=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target;device->CreateTexture2D(&d,nullptr,&target);
    ComPtr<ID3D11RenderTargetView> view;device->CreateRenderTargetView(target.Get(),nullptr,&view);
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;device->CreateTexture2D(&d,nullptr,&staging);
    AquariumRenderer renderer;renderer.Initialize(device.Get(),std::filesystem::absolute("shaders/AquariumPrototype.hlsl"));
    AquariumSettings s;s.stageMode=s.greyboxMode=s.receptionLobbyMode=true;s.adaptiveResolution=false;s.exposure=1.05f;
    s.localLighting.lightCount=2;
    s.localLighting.lights[0]={{0,7.55f,12.2f},14,{0,-1,0},8,{.1f,.48f,1},25,48,lighting::LocalLightType::Spot,true};
    s.localLighting.lights[1]={{18,5.5f,9.8f},10,{0,-1,0},4,{.1f,.3f,.7f},28,54,lighting::LocalLightType::Spot,true};
    std::filesystem::create_directories("artifacts");
    D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP_DISJOINT,0};
    ComPtr<ID3D11Query> interval,begin,end;
    if(FAILED(device->CreateQuery(&q,&interval)))return 4;
    q.Query=D3D11_QUERY_TIMESTAMP;
    if(FAILED(device->CreateQuery(&q,&begin))||FAILED(device->CreateQuery(&q,&end)))return 4;
    struct Pose{const char* name;float x,y,z,yaw,outage,visibility,approach;float light=1,writing=0;int palette=0;};
    for(const auto p:{
        Pose{"blackout-normal",0,-.36f,5,0,0,0,0},
        Pose{"blackout-dark",0,-.36f,5,0,1,0,0,0},
        Pose{"blackout-flashlight",0,-.36f,5,0,1,0,0},
        Pose{"blackout-corridor",0,4.9f,4,1.57f,1,0,0},
        Pose{"blackout-writing",18,-.36f,10.4f,1.5708f,1,0,0,1,1},
        Pose{"blackout-writing-angle",17.5f,-.36f,8.5f,1.08f,1,0,0,1,1},
        Pose{"blackout-writing-occluded",21.5f,-.36f,10.4f,-1.5708f,1,0,0,1,1},
        Pose{"blackout-impact",0,4.9f,6.2f,0,1,1,1},
        Pose{"blackout-impact-side",6.5f,4.9f,6.2f,-.65f,1,1,1},
        Pose{"tank-password-blue",0,.15f,2.35f,0,0,0,0,1,0,0},
        Pose{"tank-password-white",0,.15f,2.35f,0,0,0,0,1,0,2}}){
        s.heroTankLighting.alternateEnabled=p.palette!=0;
        s.heroTankLighting.alternateColor=s.heroTankLighting.whiteColor;
        s.cameraPositionX=p.x;s.cameraPositionY=p.y;s.cameraPositionZ=p.z;s.cameraYaw=p.yaw;
        s.cameraPitch=0;s.powerOutage=p.outage;s.blackoutPredatorVisibility=p.visibility;
        s.blackoutPredatorApproach=p.approach;
        s.flashlightOn=p.light;
        s.blackoutWriting=p.writing;
        for(int frame=0;frame<3;++frame)renderer.Render(context.Get(),view.Get(),w,h,8.f+frame*.016f,.016f,s);
        context->Begin(interval.Get());context->End(begin.Get());
        constexpr int samples=24;
        for(int frame=0;frame<samples;++frame)
            renderer.Render(context.Get(),view.Get(),w,h,9.f+frame*.016f,.016f,s);
        context->End(end.Get());context->End(interval.Get());
        context->CopyResource(staging.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return 3;
        BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=54;file.bfSize=54+w*h*4;
        BITMAPINFOHEADER info{};info.biSize=40;info.biWidth=w;info.biHeight=-int(h);info.biPlanes=1;info.biBitCount=32;
        std::ofstream out(std::string("artifacts/")+p.name+".bmp",std::ios::binary);
        out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));
        int redPixels=0;
        for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){
            auto* rgba=static_cast<unsigned char*>(mapped.pData)+mapped.RowPitch*y+x*4;
            if(rgba[0]>100 && rgba[0]>rgba[1]*3 && rgba[0]>rgba[2]*3)++redPixels;
            if(p.outage==1 && p.light==0 && (rgba[0]||rgba[1]||rgba[2])) {
                std::cerr<<"Light leaked into a fully dark frame\n";return 5;
            }
            const unsigned char bgra[]{rgba[2],rgba[1],rgba[0],255};out.write(reinterpret_cast<const char*>(bgra),4);
        }
        context->Unmap(staging.Get(),0);
        if(p.writing>0 && ((std::string(p.name)=="blackout-writing-occluded") ? redPixels!=0 : redPixels<100)) {
            std::cerr<<"Tank writing visibility/occlusion failed: "<<p.name<<'\n';return 6;
        }
        std::cout<<p.name<<" rendered";
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timing{};UINT64 first=0,last=0;
        if(context->GetData(interval.Get(),&timing,sizeof(timing),0)==S_OK && !timing.Disjoint &&
            context->GetData(begin.Get(),&first,sizeof(first),0)==S_OK &&
            context->GetData(end.Get(),&last,sizeof(last),0)==S_OK)
            std::cout<<" GPU ms/frame="<<double(last-first)*1000/double(timing.Frequency)/samples;
        std::cout<<'\n';
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
