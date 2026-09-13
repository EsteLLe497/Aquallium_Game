#pragma once
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <vector>
#include <stdexcept>

namespace rendering {
inline Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> UploadRgba(
    ID3D11Device* device,const void* pixels,UINT width,UINT height,bool srgb=false) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels,width*4,0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    if(FAILED(device->CreateTexture2D(&desc,&data,&texture))||
        FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&view)))
        throw std::runtime_error("Texture upload failed");
    return view;
}

inline Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> DecodeTexture(
    ID3D11Device* device,const unsigned char* bytes,UINT length) {
    using Microsoft::WRL::ComPtr;
    // S_FALSE also owns a COM reference; RPC_E_CHANGED_MODE owns none.
    const HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    struct Apartment {HRESULT result;~Apartment(){if(SUCCEEDED(result))CoUninitialize();}} scope{apartment};
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    auto check=[](HRESULT hr){if(FAILED(hr))throw std::runtime_error("Embedded material image decode failed");};
    check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    check(factory->CreateStream(&stream));
    check(stream->InitializeFromMemory(const_cast<BYTE*>(bytes),length));
    check(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder));
    check(decoder->GetFrame(0,&frame));check(factory->CreateFormatConverter(&converter));
    check(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,
        nullptr,0,WICBitmapPaletteTypeCustom));
    UINT width=0,height=0;check(frame->GetSize(&width,&height));
    if(!width||!height||width>16384||height>16384)throw std::runtime_error("Invalid image dimensions");
    std::vector<unsigned char> pixels(size_t(width)*height*4);
    check(converter->CopyPixels(nullptr,width*4,UINT(pixels.size()),pixels.data()));
    return UploadRgba(device,pixels.data(),width,height);
}

// Rasterize the supplied private font once. Never install it system-wide.
inline Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateHorrorWriting(
    ID3D11Device* device,const std::filesystem::path& fontPath) {
    if(!AddFontResourceExW(fontPath.c_str(),FR_PRIVATE,nullptr))
        throw std::runtime_error("onryou.TTF could not be loaded");
    struct FontResource {std::filesystem::path path;~FontResource(){RemoveFontResourceExW(path.c_str(),FR_PRIVATE,nullptr);}} resource{fontPath};
    constexpr int width=1024,height=256;
    struct Canvas {
        HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=nullptr;HFONT font=nullptr;
        HGDIOBJ oldBitmap=nullptr,oldFont=nullptr;
        ~Canvas(){if(oldFont)SelectObject(dc,oldFont);if(oldBitmap)SelectObject(dc,oldBitmap);
            if(font)DeleteObject(font);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    } canvas;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bits=nullptr;
    canvas.bitmap=CreateDIBSection(canvas.dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    canvas.font=CreateFontW(-180,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"怨霊");
    if(!canvas.dc||!canvas.bitmap||!canvas.font)throw std::runtime_error("Writing canvas creation failed");
    canvas.oldBitmap=SelectObject(canvas.dc,canvas.bitmap);canvas.oldFont=SelectObject(canvas.dc,canvas.font);
    PatBlt(canvas.dc,0,0,width,height,BLACKNESS);
    SetTextColor(canvas.dc,RGB(255,255,255));SetBkMode(canvas.dc,TRANSPARENT);
    RECT rect{0,0,width,height};DrawTextW(canvas.dc,L"ニガサナイ",-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    GdiFlush();
    return UploadRgba(device,bits,width,height);
}
}
