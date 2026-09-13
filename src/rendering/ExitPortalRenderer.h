#pragma once

#include <d3d11.h>
#include <filesystem>
#include <wrl/client.h>

class ExitPortalRenderer
{
public:
    void Initialize(ID3D11Device* device,const std::filesystem::path& shaderPath);
    void Render(ID3D11DeviceContext* context,ID3D11RenderTargetView* target,
        UINT width,UINT height,float progress,bool beachSide,float time);

private:
    struct alignas(16) Constants
    {
        float progress,beachSide,time,aspect;
    };
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthDisabled_;
};
