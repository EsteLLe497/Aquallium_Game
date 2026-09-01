/*==================================================================================================

   [AquariumRenderer.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   水面、コースティクス、体積光、ガラス合成を管理するアクアリウム専用レンダラー
===================================================================================================*/
#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <array>
#include <filesystem>
#include <wrl/client.h>

#include "StageModel.h"
#include "JellyfishRenderer.h"
#include "FishRenderer.h"
#include "lighting/HeroTankLighting.h"
#include "lighting/LocalLight.h"
#include "rendering/AdaptiveResolution.h"

static constexpr UINT kMaxAquariumLights = 4;

// One authored light drives its surface highlight, refracted underwater axis,
// volumetric shaft and receiver caustics. Keeping this data on the CPU prevents
// the individual effects from drifting apart while a route is tuned.
struct AquariumLight
{
    DirectX::XMFLOAT3 position{};
    float intensity = 0.0f;
    DirectX::XMFLOAT3 direction{0.0f, -1.0f, 0.0f};
    float coneAngleDegrees = 34.0f;
    DirectX::XMFLOAT3 color{0.088f, 0.42f, 1.04f};
    float surfaceHeight = 2.65f;
};

struct AquariumSettings
{
    // カメラ
    float cameraYaw = 0.0f;
    float cameraPitch = -0.03f;
    // 水中ライティング
    float causticsStrength = 1.10f;
    float volumeStrength = 1.32f;
    float exposure = 1.02f;
    float waterClarity = 1.18f;
    float anisotropy = 0.64f;
    float historyWeight = 0.94f;
    // 水中ビュー / ガラス越しビュー
    float viewMode = 0.0f;
    float glassDistortion = 1.0f;
    float cameraPositionX = 0.0f;
    float cameraPositionY = 0.10f;
    float cameraPositionZ = -6.65f;
    bool paused = false;
    bool stageMode = false;
    bool greyboxMode = false;
    bool underwaterArchMode = false;
    bool watatsumiTankMode = false;
    bool continuousMapMode = false;
    bool adaptiveResolution = true;
    float targetFrameRate = 100.0f;
    lighting::LocalLightingRig localLighting{};
    lighting::HeroTankLightingRig heroTankLighting{};
};

class AquariumRenderer
{
public:
    // シェーダーと解像度非依存GPUリソースを生成
    void Initialize(ID3D11Device* device, const std::filesystem::path& shaderPath);
    void Render(
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* target,
        UINT width,
        UINT height,
        float time,
        float deltaTime,
        const AquariumSettings& settings);

    [[nodiscard]] float RenderScale() const noexcept
    {
        return adaptiveResolution_.Scale();
    }

    [[nodiscard]] float SmoothedFrameMilliseconds() const noexcept
    {
        return adaptiveResolution_.SmoothedFrameMilliseconds();
    }

private:
    // 起動時に一度だけ必要なシェーダー、ステート、ノイズ、Shadow Mapを生成
    void CreateStaticResources(ID3D11Device* device);
    // ウィンドウサイズに依存するHDR、深度、Motion Vector、履歴Bufferを再生成
    void EnsureSizeResources(ID3D11Device* device, UINT width, UINT height);

    struct alignas(16) FrameConstants
    {
        float time;
        float deltaTime;
        float resolutionX;
        float resolutionY;

        float cameraYaw;
        float cameraPitch;
        float causticsStrength;
        float volumeStrength;

        float exposure;
        float waterClarity;
        float anisotropy;
        float historyWeight;

        float previousCameraYaw;
        float previousCameraPitch;
        float historyValid;
        float frameIndex;

        float viewMode;
        float previousViewMode;
        float glassDistortion;
        float padding;

        float cameraPositionX;
        float cameraPositionY;
        float cameraPositionZ;
        float underwaterArchMode;

        float previousCameraPositionX;
        float previousCameraPositionY;
        float previousCameraPositionZ;
        float previousCameraPositionPadding;
    };

    struct alignas(16) ShadowConstants
    {
        std::array<DirectX::XMFLOAT4X4, kMaxAquariumLights> lightViewProjection;
        std::array<DirectX::XMFLOAT4, kMaxAquariumLights> lightSurfaceOrigin;
        std::array<DirectX::XMFLOAT4, kMaxAquariumLights> lightRefractedAxis;
        std::array<DirectX::XMFLOAT4, kMaxAquariumLights> lightColorStrength;
        float currentLightIndex;
        float activeLightCount;
        float padding[2];
    };

    struct alignas(16) FroxelConstants
    {
        UINT width;
        UINT height;
        UINT depth;
        float nearDistance;

        float farDistance;
        float logarithmicDepthRatio;
        float padding[2];
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> shadowVertexShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> shadowInputLayout_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> scenePixelShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> volumePixelShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> temporalPixelShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> compositePixelShader_;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> froxelInjectionComputeShader_;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> froxelIntegrationComputeShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> frameConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> shadowConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> froxelConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> shadowVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> shadowIndexBuffer_;
    UINT shadowIndexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture3D> noiseTexture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTextureView_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearWrapSampler_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearClampSampler_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> shadowComparisonSampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> shadowRasterizerState_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> shadowTexture_;
    std::array<Microsoft::WRL::ComPtr<ID3D11DepthStencilView>, 3> shadowDepthViews_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadowTextureView_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColorTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneColorTarget_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneColorView_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> refractionCopyTexture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> refractionCopyView_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneDepthTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneDepthTarget_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneDepthView_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> motionTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> motionTarget_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> motionView_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stageDepthTexture_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> stageDepthView_;

    Microsoft::WRL::ComPtr<ID3D11Texture3D> froxelInjectionTexture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> froxelInjectionView_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> froxelInjectionUav_;
    Microsoft::WRL::ComPtr<ID3D11Texture3D> froxelIntegratedTexture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> froxelIntegratedView_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> froxelIntegratedUav_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> volumeTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> volumeTarget_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> volumeView_;
    std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>, 2> historyTextures_;
    std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, 2> historyTargets_;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2> historyViews_;

    UINT resourceWidth_ = 0;
    UINT resourceHeight_ = 0;
    UINT froxelWidth_ = 0;
    UINT froxelHeight_ = 0;
    static constexpr UINT kFroxelDepth = 64;
    UINT historyReadIndex_ = 0;
    UINT frameIndex_ = 0;
    float previousCameraYaw_ = 0.0f;
    float previousCameraPitch_ = -0.03f;
    float previousViewMode_ = 0.0f;
    DirectX::XMFLOAT3 previousCameraPosition_{0.0f, 0.10f, -6.65f};
    bool previousStageMode_ = false;
    bool previousGreyboxMode_ = false;
    bool previousUnderwaterArchMode_ = false;
    bool previousWatatsumiTankMode_ = false;
    bool previousContinuousMapMode_ = false;
    bool historyValid_ = false;
    StageModel stageModel_;
    StageModel aquariumGreyboxModel_;
    StageModel underwaterArchModel_;
    StageModel watatsumiTankModel_;
    StageModel continuousShellModel_;
    StageModel continuousArchModel_;
    JellyfishRenderer jellyfishRenderer_;
    FishRenderer fishRenderer_;
    rendering::AdaptiveResolution adaptiveResolution_;
};
