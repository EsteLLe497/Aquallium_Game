#include "BeachPreviewRenderer.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {
Microsoft::WRL::ComPtr<ID3DBlob> Compile(const std::filesystem::path& path,
                                         const char* entry,const char* profile)
{
    // This is a full-screen shader, so keep it optimized in Debug builds too.
    UINT flags=D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3;
    Microsoft::WRL::ComPtr<ID3DBlob> code,errors;
    const HRESULT hr=D3DCompileFromFile(path.c_str(),nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry,profile,flags,0,code.GetAddressOf(),errors.GetAddressOf());
    if(FAILED(hr)){
        std::string message="Beach shader compilation failed: "+path.string();
        if(errors)message+='\n'+std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize());
        throw std::runtime_error(message);
    }
    return code;
}
void Check(HRESULT hr,const char* operation){if(FAILED(hr))throw std::runtime_error(std::string(operation)+" failed");}
}

void BeachPreviewRenderer::Initialize(ID3D11Device* device,const std::filesystem::path& shaderPath)
{
    const auto vs=Compile(shaderPath,"VSMain","vs_5_0");
    const auto ps=Compile(shaderPath,"PSMain","ps_5_0");
    const auto upscale=Compile(shaderPath,"PSUpscale","ps_5_0");
    Check(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,vertexShader_.GetAddressOf()),"Beach VS");
    Check(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,pixelShader_.GetAddressOf()),"Beach PS");
    Check(device->CreatePixelShader(upscale->GetBufferPointer(),upscale->GetBufferSize(),nullptr,upscalePixelShader_.GetAddressOf()),"Beach upscale PS");
    D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(Constants);buffer.Usage=D3D11_USAGE_DYNAMIC;
    buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    Check(device->CreateBuffer(&buffer,nullptr,constantBuffer_.GetAddressOf()),"Beach constants");
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=FALSE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    Check(device->CreateDepthStencilState(&depth,depthDisabled_.GetAddressOf()),"Beach depth state");
    depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;depth.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    Check(device->CreateDepthStencilState(&depth,depthEnabled_.GetAddressOf()),"Beach tree depth state");
    D3D11_BLEND_DESC blend{};blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    Check(device->CreateBlendState(&blend,opaqueBlend_.GetAddressOf()),"Beach blend state");
    D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD=D3D11_FLOAT32_MAX;
    Check(device->CreateSamplerState(&sampler,linearSampler_.GetAddressOf()),"Beach sampler");
    D3D11_BUFFER_DESC flashlight{};flashlight.ByteWidth=sizeof(float)*12;
    flashlight.Usage=D3D11_USAGE_DEFAULT;flashlight.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    const float noFlashlight[12]{};D3D11_SUBRESOURCE_DATA flashlightData{noFlashlight};
    Check(device->CreateBuffer(&flashlight,&flashlightData,noFlashlightBuffer_.GetAddressOf()),"Beach flashlight constants");

    const auto root=shaderPath.parent_path().parent_path();
    auto treePath=std::filesystem::current_path()/"asset"/"model"/"tree.glb";
    if(!std::filesystem::exists(treePath))treePath=root/"asset"/"model"/"tree.glb";
    StageModel::ImportOptions treeImport{};treeImport.loadBaseColorTextures=true;
    treeImport.translation={0,2.25f,0};
    treeModel_.Initialize(device,treePath,shaderPath.parent_path()/"Stage.hlsl",treeImport);
}

void BeachPreviewRenderer::EnsureSize(ID3D11Device* device,UINT outputWidth,UINT outputHeight)
{
    // Dense alpha-masked foliage is the worst beach angle. Keep that view
    // above 100 FPS, then recover edge definition in PSUpscale.
    const UINT width=std::max(1u,UINT(outputWidth*.39f));
    const UINT height=std::max(1u,UINT(outputHeight*.39f));
    if(width==sceneWidth_&&height==sceneHeight_)return;
    sceneTexture_.Reset();sceneTarget_.Reset();sceneView_.Reset();sceneDepth_.Reset();sceneDepthView_.Reset();sceneWidth_=width;sceneHeight_=height;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    Check(device->CreateTexture2D(&desc,nullptr,sceneTexture_.GetAddressOf()),"Beach scene texture");
    Check(device->CreateRenderTargetView(sceneTexture_.Get(),nullptr,sceneTarget_.GetAddressOf()),"Beach scene RTV");
    Check(device->CreateShaderResourceView(sceneTexture_.Get(),nullptr,sceneView_.GetAddressOf()),"Beach scene SRV");
    D3D11_TEXTURE2D_DESC depthDesc=desc;depthDesc.Format=DXGI_FORMAT_D32_FLOAT;
    depthDesc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    Check(device->CreateTexture2D(&depthDesc,nullptr,sceneDepth_.GetAddressOf()),"Beach tree depth texture");
    Check(device->CreateDepthStencilView(sceneDepth_.Get(),nullptr,sceneDepthView_.GetAddressOf()),"Beach tree depth view");
}

void BeachPreviewRenderer::Render(ID3D11DeviceContext* context,ID3D11RenderTargetView* target,
    UINT width,UINT height,float time,float cameraX,float cameraY,float cameraZ,float yaw,float pitch,
    float dayBlend,float wakeBlur,float wakeBlink)
{
    if(!context||!target||!width||!height)return;
    Microsoft::WRL::ComPtr<ID3D11Device> device;context->GetDevice(device.GetAddressOf());
    EnsureSize(device.Get(),width,height);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(constantBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    *static_cast<Constants*>(mapped.pData)={time,float(sceneWidth_),float(sceneHeight_),yaw,pitch,cameraX,cameraY,cameraZ,
        std::clamp(dayBlend,0.f,1.f),std::clamp(wakeBlur,0.f,1.f),std::clamp(wakeBlink,0.f,1.f),0};
    context->Unmap(constantBuffer_.Get(),0);
    const D3D11_VIEWPORT sceneViewport{0,0,float(sceneWidth_),float(sceneHeight_),0,1};
    ID3D11RenderTargetView* sceneTarget=sceneTarget_.Get();
    context->RSSetViewports(1,&sceneViewport);context->OMSetRenderTargets(1,&sceneTarget,nullptr);
    context->OMSetDepthStencilState(depthDisabled_.Get(),0);
    const float blendFactor[4]{};context->OMSetBlendState(opaqueBlend_.Get(),blendFactor,0xffffffff);
    context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader_.Get(),nullptr,0);context->PSSetShader(pixelShader_.Get(),nullptr,0);
    ID3D11Buffer* constants=constantBuffer_.Get();context->VSSetConstantBuffers(0,1,&constants);context->PSSetConstantBuffers(0,1,&constants);
    context->Draw(3,0);
    context->ClearDepthStencilView(sceneDepthView_.Get(),D3D11_CLEAR_DEPTH,1,0);
    context->OMSetRenderTargets(1,&sceneTarget,sceneDepthView_.Get());
    context->OMSetDepthStencilState(depthEnabled_.Get(),0);
    ID3D11Buffer* noFlashlight=noFlashlightBuffer_.Get();context->PSSetConstantBuffers(5,1,&noFlashlight);
    using namespace DirectX;
    const float cp=std::cos(pitch);
    const XMFLOAT3 camera{cameraX,cameraY,cameraZ};
    const XMVECTOR forward=XMVector3Normalize(XMVectorSet(std::sin(yaw)*cp,std::sin(pitch),std::cos(yaw)*cp,0));
    const XMVECTOR right=XMVector3Normalize(XMVectorSet(std::cos(yaw),0,-std::sin(yaw),0));
    const XMVECTOR up=XMVector3Normalize(XMVector3Cross(forward,right));
    const XMMATRIX view=XMMatrixLookToLH(XMLoadFloat3(&camera),forward,up);
    const XMMATRIX projection=XMMatrixPerspectiveFovLH(2*std::atan(1.f/1.45f),float(sceneWidth_)/sceneHeight_,.03f,120.f);
    const XMMATRIX viewProjection=view*projection;
    struct TreePlacement{float x,z,scale,yaw;};
    static constexpr std::array<TreePlacement,4> trees{{
        {9.35f,-4.65f,.78f,-.20f},{9.70f,-1.55f,.88f,.45f},
        {9.30f,1.55f,.80f,-.72f},{9.65f,4.65f,.90f,.16f}}};
    for(const auto& tree:trees){
        const XMMATRIX world=XMMatrixScaling(tree.scale,tree.scale,tree.scale)*
            XMMatrixRotationY(tree.yaw)*XMMatrixTranslation(tree.x,.74f,tree.z);
        treeModel_.SetRuntimeTransform(world,1);
        treeModel_.RenderOpaque(context,viewProjection,viewProjection,camera,time,dayBlend,nullptr);
    }
    context->OMSetRenderTargets(0,nullptr,nullptr);
    const D3D11_VIEWPORT outputViewport{0,0,float(width),float(height),0,1};
    context->RSSetViewports(1,&outputViewport);context->OMSetRenderTargets(1,&target,nullptr);
    context->OMSetDepthStencilState(depthDisabled_.Get(),0);
    context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader_.Get(),nullptr,0);context->VSSetConstantBuffers(0,1,&constants);
    context->PSSetShader(upscalePixelShader_.Get(),nullptr,0);
    // Tree rendering binds its own b0. Restore the beach constants before the
    // upscale/wake pass reads resolution, blur and eyelid animation values.
    context->PSSetConstantBuffers(0,1,&constants);
    ID3D11ShaderResourceView* source=sceneView_.Get();context->PSSetShaderResources(0,1,&source);
    ID3D11SamplerState* sampler=linearSampler_.Get();context->PSSetSamplers(0,1,&sampler);
    context->Draw(3,0);
    ID3D11ShaderResourceView* nullView=nullptr;context->PSSetShaderResources(0,1,&nullView);
}
