#include "StoryTexture.h"
#include <wincodec.h>
#include <vector>

namespace story {
bool StoryTexture::Load(ID3D11Device* device,const std::filesystem::path& path) {
    view.Reset();width=height=0;alphaCoverage=0.f;
    if(!device)return false;
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return false;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if(FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)))return false;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if(FAILED(decoder->GetFrame(0,&frame)) || FAILED(frame->GetSize(&width,&height)) || !width || !height)return false;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateFormatConverter(&converter)) ||
       FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
    std::vector<unsigned char> pixels(size_t(width)*height*4);
    if(FAILED(converter->CopyPixels(nullptr,width*4,UINT(pixels.size()),pixels.data())))return false;
    size_t visiblePixels=0;
    for(size_t i=3;i<pixels.size();i+=4)if(pixels[i]>8)++visiblePixels;
    alphaCoverage=float(visiblePixels)/float(size_t(width)*height);
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    return SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)) && SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&view));
}
}
