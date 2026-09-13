#pragma once

#include <d3d11.h>
#include <filesystem>
#include <wrl/client.h>
#include "StageModel.h"

class BeachPreviewRenderer
{
public:
    void Initialize(ID3D11Device* device,const std::filesystem::path& shaderPath);
    void Render(ID3D11DeviceContext* context,ID3D11RenderTargetView* target,
                UINT width,UINT height,float time,float cameraX,float cameraY,
                float cameraZ,float yaw,float pitch,float dayBlend=0,
                float wakeBlur=0,float wakeBlink=0);

private:
    struct alignas(16) Constants
    {
        float time,width,height,yaw;
        float pitch,cameraX,cameraY,cameraZ;
        float dayBlend,wakeBlur,wakeBlink,padding;
    };
    void EnsureSize(ID3D11Device* device,UINT outputWidth,UINT outputHeight);
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> upscalePixelShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthDisabled_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthEnabled_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> opaqueBlend_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSampler_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneTarget_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneView_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneDepth_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> sceneDepthView_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> noFlashlightBuffer_;
    StageModel treeModel_;
    UINT sceneWidth_=0,sceneHeight_=0;
};
