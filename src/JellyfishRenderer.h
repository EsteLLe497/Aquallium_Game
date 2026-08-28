#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <wrl/client.h>

class JellyfishRenderer
{
public:
    void Initialize(
        ID3D11Device* device,
        const std::filesystem::path& shaderPath);

    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& currentViewProjection,
        const DirectX::XMMATRIX& previousViewProjection,
        const DirectX::XMFLOAT3& cameraPosition,
        float time);

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
        float part;
    };

    struct Instance
    {
        DirectX::XMFLOAT4 positionScale;
        DirectX::XMFLOAT4 tintPhase;
        DirectX::XMFLOAT4 motion;
    };

    struct alignas(16) Constants
    {
        DirectX::XMFLOAT4X4 currentViewProjection;
        DirectX::XMFLOAT4X4 previousViewProjection;
        DirectX::XMFLOAT4 cameraTime;
    };

    void CreateGeometry(ID3D11Device* device);
    void CreateInstances(ID3D11Device* device);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> jellyVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> jellyIndexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> jellyInstanceBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> particleVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> particleIndexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> particleInstanceBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;
    std::uint32_t jellyIndexCount_ = 0;
    std::uint32_t jellyInstanceCount_ = 0;
    std::uint32_t particleIndexCount_ = 0;
    std::uint32_t particleInstanceCount_ = 0;
};
