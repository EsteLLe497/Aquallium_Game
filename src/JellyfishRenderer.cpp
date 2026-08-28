#include "JellyfishRenderer.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace
{
using Microsoft::WRL::ComPtr;

ComPtr<ID3DBlob> CompileShader(
    const std::filesystem::path& path,
    const char* entryPoint,
    const char* profile)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompileFromFile(
        path.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint,
        profile,
        flags,
        0,
        bytecode.GetAddressOf(),
        errors.GetAddressOf());
    if (FAILED(result))
    {
        std::string message = "Jellyfish shader compilation failed.";
        if (errors)
        {
            message.append(
                static_cast<const char*>(errors->GetBufferPointer()),
                errors->GetBufferSize());
        }
        throw std::runtime_error(message);
    }
    return bytecode;
}

void ThrowIfFailed(HRESULT result, const char* operation)
{
    if (FAILED(result))
    {
        throw std::runtime_error(std::string(operation) + " failed.");
    }
}

template <typename T>
ComPtr<ID3D11Buffer> CreateImmutableBuffer(
    ID3D11Device* device,
    const std::vector<T>& values,
    UINT bindFlags)
{
    D3D11_BUFFER_DESC description{};
    description.ByteWidth = static_cast<UINT>(values.size() * sizeof(T));
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = bindFlags;
    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = values.data();
    ComPtr<ID3D11Buffer> buffer;
    ThrowIfFailed(
        device->CreateBuffer(
            &description,
            &initialData,
            buffer.GetAddressOf()),
        "CreateBuffer (jellyfish)");
    return buffer;
}
}

void JellyfishRenderer::Initialize(
    ID3D11Device* device,
    const std::filesystem::path& shaderPath)
{
    const auto vertexBytecode =
        CompileShader(shaderPath, "VSJellyfish", "vs_5_0");
    const auto pixelBytecode =
        CompileShader(shaderPath, "PSJellyfish", "ps_5_0");
    ThrowIfFailed(
        device->CreateVertexShader(
            vertexBytecode->GetBufferPointer(),
            vertexBytecode->GetBufferSize(),
            nullptr,
            vertexShader_.GetAddressOf()),
        "CreateVertexShader (jellyfish)");
    ThrowIfFailed(
        device->CreatePixelShader(
            pixelBytecode->GetBufferPointer(),
            pixelBytecode->GetBufferSize(),
            nullptr,
            pixelShader_.GetAddressOf()),
        "CreatePixelShader (jellyfish)");

    const D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
            D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
            D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 32,
            D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"INSTANCE_POSITION_SCALE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,
            D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE_TINT_PHASE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16,
            D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE_MOTION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32,
            D3D11_INPUT_PER_INSTANCE_DATA, 1}
    };
    ThrowIfFailed(
        device->CreateInputLayout(
            elements,
            static_cast<UINT>(std::size(elements)),
            vertexBytecode->GetBufferPointer(),
            vertexBytecode->GetBufferSize(),
            inputLayout_.GetAddressOf()),
        "CreateInputLayout (jellyfish)");

    D3D11_BUFFER_DESC constants{};
    constants.ByteWidth = sizeof(Constants);
    constants.Usage = D3D11_USAGE_DYNAMIC;
    constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ThrowIfFailed(
        device->CreateBuffer(
            &constants,
            nullptr,
            constantBuffer_.GetAddressOf()),
        "CreateBuffer (jellyfish constants)");

    D3D11_BLEND_DESC blend{};
    blend.IndependentBlendEnable = TRUE;
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    // Transparent biology contributes only to HDR color. Blending its
    // overlapping layers into linear depth and motion corrupts temporal
    // reprojection and produced black patches from the reverse viewpoint.
    blend.RenderTarget[1].RenderTargetWriteMask = 0;
    blend.RenderTarget[2].RenderTargetWriteMask = 0;
    ThrowIfFailed(
        device->CreateBlendState(&blend, blendState_.GetAddressOf()),
        "CreateBlendState (jellyfish)");

    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ThrowIfFailed(
        device->CreateDepthStencilState(&depth, depthState_.GetAddressOf()),
        "CreateDepthStencilState (jellyfish)");

    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    ThrowIfFailed(
        device->CreateRasterizerState(
            &rasterizer,
            rasterizerState_.GetAddressOf()),
        "CreateRasterizerState (jellyfish)");

    CreateGeometry(device);
    CreateInstances(device);
}

void JellyfishRenderer::CreateGeometry(ID3D11Device* device)
{
    constexpr int radialSegments = 20;
    constexpr int verticalSegments = 7;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    for (int y = 0; y <= verticalSegments; ++y)
    {
        const float t = static_cast<float>(y) / verticalSegments;
        const float angleY = t * DirectX::XM_PIDIV2;
        const float radius = std::sin(angleY) * 0.36f;
        const float height = std::cos(angleY) * 0.29f;
        for (int x = 0; x <= radialSegments; ++x)
        {
            const float angle = DirectX::XM_2PI * x / radialSegments;
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            vertices.push_back({
                {radius * cosine, height, radius * sine},
                {cosine * std::sin(angleY), std::cos(angleY),
                    sine * std::sin(angleY)},
                {static_cast<float>(x) / radialSegments, t},
                0.0f
            });
        }
    }
    for (int y = 0; y < verticalSegments; ++y)
    {
        for (int x = 0; x < radialSegments; ++x)
        {
            const std::uint32_t a = y * (radialSegments + 1) + x;
            const std::uint32_t b = a + radialSegments + 1;
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }

    // Close the underside with a shallow translucent membrane. The original
    // hemisphere was open, so views from the bench side exposed the black room
    // through a jellyfish-shaped hole.
    const std::uint32_t undersideCenter =
        static_cast<std::uint32_t>(vertices.size());
    vertices.push_back({
        {0.0f, -0.055f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.5f, 1.0f},
        0.0f
    });
    const std::uint32_t rimStart =
        verticalSegments * (radialSegments + 1);
    for (int x = 0; x < radialSegments; ++x)
    {
        const std::uint32_t current = rimStart + x;
        const std::uint32_t following = current + 1;
        indices.insert(
            indices.end(),
            {undersideCenter, following, current});
    }

    constexpr int tentacleCount = 8;
    constexpr int tentacleSegments = 9;
    for (int strand = 0; strand < tentacleCount; ++strand)
    {
        const float angle = DirectX::XM_2PI * strand / tentacleCount;
        const std::uint32_t base =
            static_cast<std::uint32_t>(vertices.size());
        for (int segment = 0; segment <= tentacleSegments; ++segment)
        {
            const float t = static_cast<float>(segment) / tentacleSegments;
            const float radius = 0.08f + (strand % 3) * 0.018f;
            const DirectX::XMFLOAT3 center{
                std::cos(angle) * radius,
                -0.02f - t * (0.62f + (strand % 2) * 0.18f),
                std::sin(angle) * radius
            };
            for (int side = 0; side < 2; ++side)
            {
                vertices.push_back({
                    center,
                    {0.0f, 0.0f, -1.0f},
                    {static_cast<float>(side), t},
                    1.0f
                });
            }
        }
        for (int segment = 0; segment < tentacleSegments; ++segment)
        {
            const std::uint32_t a = base + segment * 2;
            indices.insert(indices.end(), {a, a + 2, a + 1, a + 1, a + 2, a + 3});
        }
    }
    jellyVertexBuffer_ = CreateImmutableBuffer(
        device, vertices, D3D11_BIND_VERTEX_BUFFER);
    jellyIndexBuffer_ = CreateImmutableBuffer(
        device, indices, D3D11_BIND_INDEX_BUFFER);
    jellyIndexCount_ = static_cast<std::uint32_t>(indices.size());

    const std::vector<Vertex> particleVertices{
        {{-0.5f, -0.5f, 0.0f}, {0, 0, -1}, {0, 1}, 2.0f},
        {{-0.5f,  0.5f, 0.0f}, {0, 0, -1}, {0, 0}, 2.0f},
        {{ 0.5f,  0.5f, 0.0f}, {0, 0, -1}, {1, 0}, 2.0f},
        {{ 0.5f, -0.5f, 0.0f}, {0, 0, -1}, {1, 1}, 2.0f}
    };
    const std::vector<std::uint32_t> particleIndices{0, 1, 2, 0, 2, 3};
    particleVertexBuffer_ = CreateImmutableBuffer(
        device, particleVertices, D3D11_BIND_VERTEX_BUFFER);
    particleIndexBuffer_ = CreateImmutableBuffer(
        device, particleIndices, D3D11_BIND_INDEX_BUFFER);
    particleIndexCount_ = 6;
}

void JellyfishRenderer::CreateInstances(ID3D11Device* device)
{
    struct Tank
    {
        float x;
        float z;
        float height;
    };
    const std::array<Tank, 7> tanks{{
        {-0.2f, -3.55f, 3.85f}, {2.0f, 3.45f, 3.35f},
        {4.3f, -3.20f, 4.05f}, {6.6f, 3.65f, 3.75f},
        {8.8f, -3.45f, 3.40f}, {11.0f, 3.25f, 4.00f},
        {13.0f, -3.55f, 3.55f}
    }};
    std::vector<Instance> jellyInstances;
    std::vector<Instance> particleInstances;
    for (std::size_t tankIndex = 0; tankIndex < tanks.size(); ++tankIndex)
    {
        const Tank& tank = tanks[tankIndex];
        constexpr int jellyfishPerTank = 7;
        for (int index = 0; index < jellyfishPerTank; ++index)
        {
            const float phase =
                static_cast<float>(tankIndex) * 1.73f + index * 2.11f;
            const float distributionAngle =
                index * 2.39996323f +
                static_cast<float>(tankIndex) * 0.43f;
            const float distributionRadius =
                0.07f + 0.045f * static_cast<float>(index % 4);
            const float y = -1.34f +
                (index + 0.52f) *
                (tank.height - 0.52f) /
                static_cast<float>(jellyfishPerTank);
            const float tintMix = static_cast<float>((index + tankIndex) % 3);
            const float scale = index == 0
                ? 0.58f
                : 0.40f + 0.045f * static_cast<float>(index % 4);
            jellyInstances.push_back({
                {tank.x + std::cos(distributionAngle) * distributionRadius,
                    y,
                    tank.z + std::sin(distributionAngle) * distributionRadius,
                    scale},
                {0.35f + tintMix * 0.07f, 0.68f - tintMix * 0.04f,
                    1.0f, phase},
                {0.18f + 0.018f * static_cast<float>(index % 4),
                    0.09f + 0.014f * static_cast<float>(tankIndex),
                    0.14f + 0.016f * static_cast<float>(index % 3),
                    0.82f + 0.055f * static_cast<float>(index)}
            });
        }
        for (int index = 0; index < 12; ++index)
        {
            const float phase = tankIndex * 4.1f + index * 1.37f;
            const float fraction = (index + 0.5f) / 12.0f;
            particleInstances.push_back({
                {tank.x + std::sin(phase * 2.3f) * 0.34f,
                    -1.52f + fraction * (tank.height - 0.18f),
                    tank.z + std::cos(phase * 1.7f) * 0.34f,
                    0.018f + 0.008f * (index % 3)},
                {0.18f, 0.62f, 1.0f, phase},
                {0.035f, 0.08f, 0.03f, 0.45f + 0.04f * index}
            });
        }
    }
    jellyInstanceBuffer_ = CreateImmutableBuffer(
        device, jellyInstances, D3D11_BIND_VERTEX_BUFFER);
    particleInstanceBuffer_ = CreateImmutableBuffer(
        device, particleInstances, D3D11_BIND_VERTEX_BUFFER);
    jellyInstanceCount_ = static_cast<std::uint32_t>(jellyInstances.size());
    particleInstanceCount_ =
        static_cast<std::uint32_t>(particleInstances.size());
}

void JellyfishRenderer::Render(
    ID3D11DeviceContext* context,
    const DirectX::XMMATRIX& currentViewProjection,
    const DirectX::XMMATRIX& previousViewProjection,
    const DirectX::XMFLOAT3& cameraPosition,
    float time)
{
    D3D11_MAPPED_SUBRESOURCE mapped{};
    ThrowIfFailed(
        context->Map(
            constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
        "Map (jellyfish constants)");
    auto* constants = static_cast<Constants*>(mapped.pData);
    DirectX::XMStoreFloat4x4(
        &constants->currentViewProjection, currentViewProjection);
    DirectX::XMStoreFloat4x4(
        &constants->previousViewProjection, previousViewProjection);
    constants->cameraTime = {
        cameraPosition.x, cameraPosition.y, cameraPosition.z, time
    };
    context->Unmap(constantBuffer_.Get(), 0);

    context->IASetInputLayout(inputLayout_.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context->PSSetShader(pixelShader_.Get(), nullptr, 0);
    ID3D11Buffer* constantBuffer = constantBuffer_.Get();
    context->VSSetConstantBuffers(3, 1, &constantBuffer);
    context->PSSetConstantBuffers(3, 1, &constantBuffer);
    context->RSSetState(rasterizerState_.Get());
    const float blendFactor[4]{};
    context->OMSetBlendState(blendState_.Get(), blendFactor, 0xffffffffu);
    context->OMSetDepthStencilState(depthState_.Get(), 0);

    auto draw = [&](ID3D11Buffer* vertices,
                    ID3D11Buffer* indices,
                    ID3D11Buffer* instances,
                    UINT indexCount,
                    UINT instanceCount)
    {
        ID3D11Buffer* buffers[] = {vertices, instances};
        const UINT strides[] = {sizeof(Vertex), sizeof(Instance)};
        const UINT offsets[] = {0, 0};
        context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        context->IASetIndexBuffer(indices, DXGI_FORMAT_R32_UINT, 0);
        context->DrawIndexedInstanced(
            indexCount, instanceCount, 0, 0, 0);
    };
    draw(
        jellyVertexBuffer_.Get(), jellyIndexBuffer_.Get(),
        jellyInstanceBuffer_.Get(), jellyIndexCount_, jellyInstanceCount_);
    draw(
        particleVertexBuffer_.Get(), particleIndexBuffer_.Get(),
        particleInstanceBuffer_.Get(), particleIndexCount_,
        particleInstanceCount_);

    context->OMSetBlendState(nullptr, blendFactor, 0xffffffffu);
    context->OMSetDepthStencilState(nullptr, 0);
    context->RSSetState(nullptr);
}
