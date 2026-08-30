#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include <cstdint>
#include <filesystem>
#include <vector>
#include <wrl/client.h>

// Ambient fish are simulated as compact data, not individual GameObjects.
// Only scripted/chasing hero animals should pay the per-object framework cost.
class FishRenderer
{
public:
    enum class Habitat : std::uint32_t
    {
        None,
        UnderwaterArch,
        WatatsumiTank
    };

    void Initialize(
        ID3D11Device* device,
        const std::filesystem::path& shaderPath);

    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& viewProjection,
        const DirectX::XMFLOAT3& cameraPosition,
        float totalTime,
        float deltaTime,
        Habitat habitat);

    [[nodiscard]] std::uint32_t FishCount() const noexcept
    {
        return static_cast<std::uint32_t>(agents_.size());
    }

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
        float bendWeight;
    };

    struct Instance
    {
        DirectX::XMFLOAT4 positionScale;
        DirectX::XMFLOAT4 forwardPhase;
        DirectX::XMFLOAT4 tintSwim;
    };

    struct Agent
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT3 velocity{1.0f, 0.0f, 0.0f};
        float phase = 0.0f;
        float scale = 1.0f;
        float tint = 0.0f;
        std::uint32_t school = 0;
    };

    struct alignas(16) Constants
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4 cameraTime;
        DirectX::XMFLOAT4 waterParameters;
    };

    void CreateGeometry(ID3D11Device* device);
    void CreatePipeline(ID3D11Device* device, const std::filesystem::path& shaderPath);
    void ResetHabitat(Habitat habitat);
    void Simulate(float stepSeconds, float totalTime);
    DirectX::XMFLOAT3 SchoolTarget(std::uint32_t school, float time) const;
    void ApplyHabitatSteering(
        const Agent& agent,
        DirectX::XMFLOAT3& steering) const;
    void ConstrainToHabitat(Agent& agent) const;
    bool IsVisible(
        const Agent& agent,
        const DirectX::XMMATRIX& viewProjection,
        const DirectX::XMFLOAT3& cameraPosition) const;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;
    std::vector<Agent> agents_;
    std::vector<Instance> visibleInstances_;
    std::uint32_t indexCount_ = 0;
    std::uint32_t instanceCapacity_ = 256;
    Habitat habitat_ = Habitat::None;
    float simulationAccumulator_ = 0.0f;
};
