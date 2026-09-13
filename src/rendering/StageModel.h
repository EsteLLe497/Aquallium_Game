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
#include <algorithm>
#include <wrl/client.h>

#include "../lighting/LocalLight.h"

class StageModel
{
public:
    void SetDoorAngle(float angle) noexcept {
        previousDoorAngle_=doorAngle_;doorAngle_=angle;
    }
    void SetStoryState(bool cluePaperVisible,bool cluePaperHighlighted,
                       bool managementDoorUnlocked,float managementDoorAngle,
                       bool manualPapersVisible,bool manualPapersHighlighted,
                       float staffDoorAngle) noexcept {
        cluePaperVisible_=cluePaperVisible;cluePaperHighlighted_=cluePaperHighlighted;
        managementDoorUnlocked_=managementDoorUnlocked;
        previousManagementDoorAngle_=managementDoorAngle_;managementDoorAngle_=managementDoorAngle;
        manualPapersVisible_=manualPapersVisible;
        manualPapersHighlighted_=manualPapersHighlighted;
        previousStaffDoorAngle_=staffDoorAngle_;staffDoorAngle_=staffDoorAngle;
    }
    void SetEmergencyExitDoorOpen(float openness) noexcept {
        emergencyExitDoorOpen_=openness;
    }
    void SetRuntimeTransform(const DirectX::XMMATRIX& world,float reveal) noexcept;
    void SetArchPresentation(float extension,int kind) noexcept {
        archExtension_=extension;archKind_=kind;
    }
    enum class TransparentLayer
    {
        All,
        Medium,
        Glass
    };

    struct ImportOptions
    {
        bool hideAuthoringSurfaces = false;
        // Opt-in so existing aquarium materials retain their established
        // analytic shading. Props can preserve embedded albedo/opacity.
        bool loadBaseColorTextures = false;
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
    // Imported bounds include node transforms, matching the runtime mesh.
    [[nodiscard]] float MinimumZ() const noexcept {
        float result=0.f;
        for(const auto& batch:drawBatches_)
            if(batch.boundsMinimum.z<result)result=batch.boundsMinimum.z;
        return result;
    }
    void Bounds(DirectX::XMFLOAT3& minimum, DirectX::XMFLOAT3& maximum) const noexcept {
        minimum=maximum={};
        if(drawBatches_.empty())return;
        minimum=drawBatches_.front().boundsMinimum;
        maximum=drawBatches_.front().boundsMaximum;
        for(const auto& batch:drawBatches_) {
            minimum={std::min(minimum.x,batch.boundsMinimum.x),
                std::min(minimum.y,batch.boundsMinimum.y),std::min(minimum.z,batch.boundsMinimum.z)};
            maximum={std::max(maximum.x,batch.boundsMaximum.x),
                std::max(maximum.y,batch.boundsMaximum.y),std::max(maximum.z,batch.boundsMaximum.z)};
        }
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
        DirectX::XMFLOAT4 interactionParameters;
        DirectX::XMFLOAT4 interactionParameters2;
        DirectX::XMFLOAT4 interactionParameters3;
        DirectX::XMFLOAT4X4 runtimeWorld;
        DirectX::XMFLOAT4X4 previousRuntimeWorld;
        DirectX::XMFLOAT4 runtimeControl;
        DirectX::XMFLOAT4 archControl;
        DirectX::XMFLOAT4 specularGlossiness;
        DirectX::XMFLOAT4 emissiveColor;
        DirectX::XMFLOAT4 materialControl;
    };

    struct DrawBatch
    {
        std::uint32_t indexStart = 0;
        std::uint32_t indexCount = 0;
        DirectX::XMFLOAT4 baseColor{0.8f, 0.8f, 0.8f, 1.0f};
        DirectX::XMFLOAT3 boundsMinimum{};
        DirectX::XMFLOAT3 boundsMaximum{};
        float surfaceType = 0.0f;
        bool transparent = false;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalMap;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseColorMap;
        DirectX::XMFLOAT4 specularGlossiness{0,0,0,0};
        DirectX::XMFLOAT4 emissiveColor{};
        float normalScale=1;
        float alphaCutoff=0;
        bool importedMaterial=false;
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
    Microsoft::WRL::ComPtr<ID3D11PixelShader> materialPixelShader_;
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
    float doorAngle_=0.0f,previousDoorAngle_=0.0f;
    float managementDoorAngle_=0.0f,previousManagementDoorAngle_=0.0f;
    float staffDoorAngle_=0.0f,previousStaffDoorAngle_=0.0f;
    float emergencyExitDoorOpen_=0.0f;
    bool cluePaperVisible_=true,cluePaperHighlighted_=false,managementDoorUnlocked_=false;
    bool manualPapersVisible_=false,manualPapersHighlighted_=false;
    DirectX::XMFLOAT4X4 runtimeWorld_{},previousRuntimeWorld_{};
    float runtimeReveal_=1.f;
    float archExtension_=0.f;
    int archKind_=0;
    bool runtimeTransformEnabled_=false,runtimeTransformInitialized_=false;
};
