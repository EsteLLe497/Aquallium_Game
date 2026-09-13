#include "../src/rendering/BeachPreviewRenderer.h"
#include <windows.h>
#include <d3d11.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
int main(int argc,char** argv)
{
    try{
        const float cameraYaw=argc>1?std::strtof(argv[1],nullptr):-.68f;
        const float dayBlend=argc>2?std::strtof(argv[2],nullptr):0.f;
        ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
        HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,
            D3D11_SDK_VERSION,&device,&level,&context);
        if(FAILED(hr))hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
            D3D11_SDK_VERSION,&device,&level,&context);
        if(FAILED(hr))return 2;
        constexpr UINT width=1920,height=1080;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target;device->CreateTexture2D(&desc,nullptr,&target);
        ComPtr<ID3D11RenderTargetView> view;device->CreateRenderTargetView(target.Get(),nullptr,&view);
        BeachPreviewRenderer renderer;renderer.Initialize(device.Get(),std::filesystem::absolute("shaders/BeachPreview.hlsl"));
        for(int frame=0;frame<12;++frame)
            renderer.Render(context.Get(),view.Get(),width,height,18.f+frame/60.f,3.8f,1.95f,0.f,cameraYaw,-.035f,dayBlend);
        D3D11_QUERY_DESC queryDesc{D3D11_QUERY_TIMESTAMP_DISJOINT,0};
        ComPtr<ID3D11Query> disjoint,start,end;device->CreateQuery(&queryDesc,&disjoint);
        queryDesc.Query=D3D11_QUERY_TIMESTAMP;device->CreateQuery(&queryDesc,&start);device->CreateQuery(&queryDesc,&end);
        context->Begin(disjoint.Get());context->End(start.Get());
        constexpr int timedFrames=120;
        for(int frame=0;frame<timedFrames;++frame)
            renderer.Render(context.Get(),view.Get(),width,height,19.f+frame/60.f,3.8f,1.95f,0.f,cameraYaw,-.035f,dayBlend);
        context->End(end.Get());context->End(disjoint.Get());context->Flush();
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timing{};UINT64 beginTick=0,endTick=0;
        while(context->GetData(disjoint.Get(),&timing,sizeof(timing),0)==S_FALSE)Sleep(0);
        while(context->GetData(start.Get(),&beginTick,sizeof(beginTick),0)==S_FALSE)Sleep(0);
        while(context->GetData(end.Get(),&endTick,sizeof(endTick),0)==S_FALSE)Sleep(0);
        const double milliseconds=timing.Disjoint?0.0:
            double(endTick-beginTick)*1000.0/double(timing.Frequency)/timedFrames;
        std::cout<<"Beach GPU "<<milliseconds<<" ms/frame, "<<(milliseconds>0?1000.0/milliseconds:0)<<" FPS at 1920x1080\n";
        desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;device->CreateTexture2D(&desc,nullptr,&staging);
        context->CopyResource(staging.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return 3;
        std::vector<unsigned char> pixels(width*height*4);
        for(UINT y=0;y<height;++y){
            const auto* source=static_cast<const unsigned char*>(mapped.pData)+(height-1-y)*mapped.RowPitch;
            auto* destination=pixels.data()+y*width*4;
            for(UINT x=0;x<width;++x){destination[x*4]=source[x*4+2];destination[x*4+1]=source[x*4+1];destination[x*4+2]=source[x*4];destination[x*4+3]=255;}
        }
        context->Unmap(staging.Get(),0);std::filesystem::create_directories("artifacts");
        BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=width;
        info.biHeight=height;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=DWORD(pixels.size());
        file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+info.biSizeImage;
        std::ofstream output("artifacts/beach-preview.bmp",std::ios::binary);
        output.write(reinterpret_cast<const char*>(&file),sizeof(file));output.write(reinterpret_cast<const char*>(&info),sizeof(info));
        output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
        std::cout<<"Rendered artifacts/beach-preview.bmp\n";return output?0:4;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 5;}
}
