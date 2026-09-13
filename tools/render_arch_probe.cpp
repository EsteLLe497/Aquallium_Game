// Offscreen art regression capture. Uses the production D3D11 renderer;
// no input injection, windows, or desktop screenshot dependency.
#include "../src/rendering/AquariumRenderer.h"
#include "../src/story/ArchGeometry.h"
#include <fstream>
#include <iostream>
#include <cstring>
using Microsoft::WRL::ComPtr;
int main(){try{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
    if(FAILED(hr))return 2;
    constexpr UINT w=960,h=540;
    D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target;device->CreateTexture2D(&d,nullptr,&target);
    ComPtr<ID3D11RenderTargetView> view;device->CreateRenderTargetView(target.Get(),nullptr,&view);
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;device->CreateTexture2D(&d,nullptr,&staging);
    AquariumRenderer renderer;renderer.Initialize(device.Get(),std::filesystem::absolute("shaders/AquariumPrototype.hlsl"));
    AquariumSettings s;s.stageMode=s.greyboxMode=s.receptionLobbyMode=true;
    s.adaptiveResolution=false;s.cameraPositionX=-10.05f;s.exposure=1.05f;
    struct Pose{const char* name;float z,extension,yaw,pitch,predator;float approach=0;};
    for(const auto p:{Pose{"arch-entry",30,0,0,0,0},Pose{"arch-basement",70,0,0,0,0},
        Pose{"arch-repeat-front",26.9f,30,3.14159265f,0,0},
        Pose{"arch-repeat-back",26.9f,30,0,0,0},
        Pose{"arch-face",26.9f,30,1.57079633f,0,1},
        Pose{"arch-contact",26.9f,30,1.57079633f,0,1,1},
        Pose{"basement-lookback",75,0,3.14159265f,0,0}}){
        s.cameraPositionZ=p.z;s.archExtension=p.extension;s.cameraYaw=p.yaw;s.cameraPitch=p.pitch;
        s.cameraPositionY=story::arch::Floor(p.z,p.extension)+1.89f;s.archPredatorVisibility=p.predator;
        s.archPredatorApproach=p.approach;
        for(int frame=0;frame<3;++frame)renderer.Render(context.Get(),view.Get(),w,h,12.f+frame*.016f,.016f,s);
        context->CopyResource(staging.Get(),target.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return 3;
        BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=54;file.bfSize=54+w*h*4;
        BITMAPINFOHEADER info{};info.biSize=40;info.biWidth=w;info.biHeight=-int(h);info.biPlanes=1;info.biBitCount=32;
        std::ofstream out(std::string("artifacts/")+p.name+".bmp",std::ios::binary);
        out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));
        for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){
            auto* rgba=static_cast<unsigned char*>(mapped.pData)+mapped.RowPitch*y+x*4;
            unsigned char bgra[]{rgba[2],rgba[1],rgba[0],255};out.write(reinterpret_cast<char*>(bgra),4);
        }
        context->Unmap(staging.Get(),0);std::cout<<p.name<<" rendered\n";
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
