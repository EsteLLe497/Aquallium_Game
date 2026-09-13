#include "ExitPortalRenderer.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace {
Microsoft::WRL::ComPtr<ID3DBlob> Compile(const std::filesystem::path& path,
    const char* entry,const char* profile)
{
    Microsoft::WRL::ComPtr<ID3DBlob> code,errors;
    const UINT flags=D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT hr=D3DCompileFromFile(path.c_str(),nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry,profile,flags,0,code.GetAddressOf(),errors.GetAddressOf());
    if(FAILED(hr)){
        std::string message="Exit portal shader compilation failed: "+path.string();
        if(errors){
            message+='\n';
            message.append(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize());
        }
        throw std::runtime_error(message);
    }
    return code;
}
void Check(HRESULT hr,const char* operation){if(FAILED(hr))throw std::runtime_error(std::string(operation)+" failed");}
}

void ExitPortalRenderer::Initialize(ID3D11Device* device,const std::filesystem::path& shaderPath)
{
    const auto vs=Compile(shaderPath,"VSMain","vs_5_0");
    const auto ps=Compile(shaderPath,"PSMain","ps_5_0");
    Check(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,vertexShader_.GetAddressOf()),"Exit portal VS");
    Check(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,pixelShader_.GetAddressOf()),"Exit portal PS");
    D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(Constants);buffer.Usage=D3D11_USAGE_DYNAMIC;
    buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    Check(device->CreateBuffer(&buffer,nullptr,constantBuffer_.GetAddressOf()),"Exit portal constants");
    D3D11_BLEND_DESC blend{};blend.RenderTarget[0].BlendEnable=TRUE;
    blend.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    Check(device->CreateBlendState(&blend,blendState_.GetAddressOf()),"Exit portal blend");
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=FALSE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    Check(device->CreateDepthStencilState(&depth,depthDisabled_.GetAddressOf()),"Exit portal depth");
}

void ExitPortalRenderer::Render(ID3D11DeviceContext* context,ID3D11RenderTargetView* target,
    UINT width,UINT height,float progress,bool beachSide,float time)
{
    if(!context||!target||!width||!height)return;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(constantBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    *static_cast<Constants*>(mapped.pData)={std::clamp(progress,0.f,1.f),beachSide?1.f:0.f,time,float(width)/height};
    context->Unmap(constantBuffer_.Get(),0);
    const D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};
    context->RSSetViewports(1,&viewport);context->OMSetRenderTargets(1,&target,nullptr);
    context->OMSetDepthStencilState(depthDisabled_.Get(),0);
    const float blendFactor[4]{};context->OMSetBlendState(blendState_.Get(),blendFactor,0xffffffff);
    context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader_.Get(),nullptr,0);context->PSSetShader(pixelShader_.Get(),nullptr,0);
    ID3D11Buffer* constants=constantBuffer_.Get();context->VSSetConstantBuffers(0,1,&constants);context->PSSetConstantBuffers(0,1,&constants);
    context->Draw(3,0);
}
