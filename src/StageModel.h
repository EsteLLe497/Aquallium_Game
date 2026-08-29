/*==================================================================================================

   [StageModel.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   glTF/GLBステージモデルの読み込みとDirectX 11描画
===================================================================================================*/
#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <wrl/client.h>

#include "lighting/LocalLight.h"

class StageModel
{
public:
    enum class TransparentLayer
    {
        All,
        Medium,
        Glass
    };

    struct ImportOptions
    {
        bool hideAuthoringSurfaces = false;
        float yawRadians = 0.0f;
        DirectX::XMFLOAT3 translation{0.0f, 0.0f, 0.0f};
    };

    void Initialize(
        ID3D11Device* device,
        const std::filesystem::path& modelPath,
        const std::filesystem::path& shaderPath,
        const ImportOptions& options = {});

    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& currentViewProjection,
        const DirectX::XMMATRIX& previousViewProjection,
        const DirectX::XMFLOAT3& cameraPosition,
        float time,
        float aquariumOpeningMask = 1.0f);
    void RenderOpaque(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& currentViewProjection,
        const DirectX::XMMATRIX& previousViewProjection,
        const DirectX::XMFLOAT3& cameraPosition,
        float time,
        float aquariumOpeningMask = 1.0f,
        const lighting::LocalLightingRig* localLighting = nullptr);
    void RenderTransparent(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& currentViewProjection,
        const DirectX::XMMATRIX& previousViewProjection,
        const DirectX::XMFLOAT3& cameraPosition,
        float time,
        float aquariumOpeningMask = 1.0f,
        ID3D11ShaderResourceView* refractionSceneView = nullptr,
        TransparentLayer layer = TransparentLayer::All,
        const lighting::LocalLightingRig* localLighting = nullptr);

    [[nodiscard]] bool IsLoaded() const noexcept
    {
        return indexCount_ > 0;
    }

    [[nodiscard]] std::uint32_t MeshCount() const noexcept
    {
        return meshCount_;
    }

private:
    void RenderPass(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& currentViewProjection,
        const DirectX::XMMATRIX& previousViewProjection,
        const DirectX::XMFLOAT3& cameraPosition,
        float time,
        float aquariumOpeningMask,
        bool transparentPass,
        ID3D11ShaderResourceView* refractionSceneView,
        TransparentLayer layer = TransparentLayer::All,
        const lighting::LocalLightingRig* localLighting = nullptr);

    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
    };

    struct alignas(16) Constants
    {
        DirectX::XMFLOAT4X4 currentViewProjection;
        DirectX::XMFLOAT4X4 previousViewProjection;
        DirectX::XMFLOAT4 baseColor;
        DirectX::XMFLOAT4 cameraPosition;
        DirectX::XMFLOAT4 surfaceParameters;
    };

    struct DrawBatch
    {
        std::uint32_t indexStart = 0;
        std::uint32_t indexCount = 0;
        DirectX::XMFLOAT4 baseColor{0.8f, 0.8f, 0.8f, 1.0f};
        float surfaceType = 0.0f;
        bool transparent = false;
    };

    struct alignas(16) LocalLightingConstants
    {
        std::array<DirectX::XMFLOAT4, lighting::kMaximumLocalLights> positionRange{};
        std::array<DirectX::XMFLOAT4, lighting::kMaximumLocalLights> directionType{};
        std::array<DirectX::XMFLOAT4, lighting::kMaximumLocalLights> colorIntensity{};
        std::array<DirectX::XMFLOAT4, lighting::kMaximumLocalLights> coneEnabled{};
        DirectX::XMFLOAT4 lightControl{};
        DirectX::XMFLOAT4 ambientColorStrength{};
        DirectX::XMFLOAT4 tankBounceCenterRange{};
        DirectX::XMFLOAT4 tankBounceNormalHalfWidth{};
        DirectX::XMFLOAT4 tankBounceColorIntensity{};
        DirectX::XMFLOAT4 atmosphereColorDensity{};
        DirectX::XMFLOAT4 hybridControl{};
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> localLightingBuffer_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> transparentBlendState_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> transparentDepthState_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> refractionSampler_;

    std::vector<DrawBatch> drawBatches_;
    std::uint32_t indexCount_ = 0;
    std::uint32_t meshCount_ = 0;
};
