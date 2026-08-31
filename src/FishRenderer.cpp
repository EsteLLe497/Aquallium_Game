#include "FishRenderer.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace
{
using DirectX::XMFLOAT3;
using Microsoft::WRL::ComPtr;

constexpr float kStageFloorOffset = -2.25f;
constexpr float kPi = 3.14159265358979323846f;

void ThrowIfFailed(HRESULT hr, const char* operation)
{
    if (FAILED(hr))
    {
        throw std::runtime_error(std::string(operation) + " failed.");
    }
}

ComPtr<ID3DBlob> CompileShader(
    const std::filesystem::path& path,
    const char* entryPoint,
    const char* profile)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompileFromFile(
        path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint, profile, flags, 0,
        bytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        std::string message = "Fish shader compilation failed: " + path.string();
        if (errors)
        {
            message += "\n";
            message.append(
                static_cast<const char*>(errors->GetBufferPointer()),
                errors->GetBufferSize());
        }
        throw std::runtime_error(message);
    }
    return bytecode;
}

XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

XMFLOAT3 Subtract(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

XMFLOAT3 Scale(const XMFLOAT3& value, float scale)
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

float LengthSquared(const XMFLOAT3& value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

float Length(const XMFLOAT3& value)
{
    return std::sqrt(LengthSquared(value));
}

XMFLOAT3 Normalize(const XMFLOAT3& value, const XMFLOAT3& fallback)
{
    const float length = Length(value);
    return length > 0.00001f ? Scale(value, 1.0f / length) : fallback;
}

float SmoothStep(float minimum, float maximum, float value)
{
    const float t = std::clamp(
        (value - minimum) / std::max(maximum - minimum, 0.00001f),
        0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float ArchFloor(float x)
{
    const float t = std::clamp(x / 48.0f, 0.0f, 1.0f);
    const float smooth = t * t * (3.0f - 2.0f * t);
    return kStageFloorOffset - 4.7f * smooth;
}

float ArchCanopy(float x, float z)
{
    constexpr float springHeight = 1.20f;
    constexpr float glassRadius = 3.42f;
    constexpr float glassHeight = 3.72f;
    const float normalizedZ = std::clamp(z / glassRadius, -0.98f, 0.98f);
    return ArchFloor(x) + springHeight +
        glassHeight * std::sqrt(1.0f - normalizedZ * normalizedZ);
}

std::int64_t CellKey(int x, int y, int z)
{
    const std::int64_t hx = static_cast<std::int64_t>(x) * 73856093LL;
    const std::int64_t hy = static_cast<std::int64_t>(y) * 19349663LL;
    const std::int64_t hz = static_cast<std::int64_t>(z) * 83492791LL;
    return hx ^ hy ^ hz;
}
}

void FishRenderer::Initialize(
    ID3D11Device* device,
    const std::filesystem::path& shaderPath)
{
    CreateGeometry(device);
    CreateRayGeometry(device);
    CreatePipeline(device, shaderPath);
}

void FishRenderer::CreateGeometry(ID3D11Device* device)
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    constexpr int longitudinalSegments = 8;
    constexpr int radialSegments = 8;
    for (int longitudinal = 0; longitudinal <= longitudinalSegments; ++longitudinal)
    {
        const float u = static_cast<float>(longitudinal) /
            static_cast<float>(longitudinalSegments);
        const float x = -0.66f + u * 1.32f;
        const float profile = std::max(
            0.045f,
            std::pow(std::sin(u * kPi), 0.58f));
        for (int radial = 0; radial <= radialSegments; ++radial)
        {
            const float v = static_cast<float>(radial) /
                static_cast<float>(radialSegments);
            const float angle = v * kPi * 2.0f;
            const float y = std::cos(angle) * 0.22f * profile;
            const float z = std::sin(angle) * 0.115f * profile;
            const XMFLOAT3 normal = Normalize(
                {x / (0.70f * 0.70f),
                 y / (0.22f * 0.22f),
                 z / (0.115f * 0.115f)},
                {0.0f, 1.0f, 0.0f});
            const float tailWeight = (1.0f - u) * (1.0f - u);
            vertices.push_back({{x, y, z}, normal, {u, v}, tailWeight});
        }
    }
    const std::uint32_t row = radialSegments + 1;
    for (int longitudinal = 0; longitudinal < longitudinalSegments; ++longitudinal)
    {
        for (int radial = 0; radial < radialSegments; ++radial)
        {
            const std::uint32_t a = longitudinal * row + radial;
            const std::uint32_t b = a + row;
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }

    const std::uint32_t tailBase = static_cast<std::uint32_t>(vertices.size());
    vertices.insert(vertices.end(), {
        {{-0.58f, 0.0f, 0.0f}, {0, 0, 1}, {0.0f, 0.5f}, 0.86f},
        {{-0.98f, 0.34f, 0.0f}, {0, 0, 1}, {0.6f, 0.0f}, 1.0f},
        {{-1.13f, 0.0f, 0.0f}, {0, 0, 1}, {1.0f, 0.5f}, 1.0f},
        {{-0.98f, -0.34f, 0.0f}, {0, 0, 1}, {0.6f, 1.0f}, 1.0f}
    });
    indices.insert(indices.end(), {
        tailBase, tailBase + 1, tailBase + 2,
        tailBase, tailBase + 2, tailBase + 3
    });

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    D3D11_SUBRESOURCE_DATA initialData{vertices.data(), 0, 0};
    ThrowIfFailed(
        device->CreateBuffer(&bufferDesc, &initialData, vertexBuffer_.GetAddressOf()),
        "CreateBuffer (fish vertices)");

    bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bufferDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
    initialData.pSysMem = indices.data();
    ThrowIfFailed(
        device->CreateBuffer(&bufferDesc, &initialData, indexBuffer_.GetAddressOf()),
        "CreateBuffer (fish indices)");
    indexCount_ = static_cast<std::uint32_t>(indices.size());
}

void FishRenderer::CreateRayGeometry(ID3D11Device* device)
{
    // A compact manta silhouette. Negative bend weights select the wing-flap
    // path in Fish.hlsl, allowing the same pipeline to animate both meshes.
    const std::vector<Vertex> vertices{
        {{ 0.05f, 0.0f,  0.00f}, {0, 1, 0}, {0.50f, 0.50f}, -0.08f},
        {{ 1.08f, 0.0f,  0.00f}, {0, 1, 0}, {1.00f, 0.50f}, -0.05f},
        {{ 0.46f, 0.0f,  0.58f}, {0, 1, 0}, {0.72f, 0.73f}, -0.46f},
        {{-0.12f, 0.0f,  1.28f}, {0, 1, 0}, {0.45f, 1.00f}, -1.00f},
        {{-0.76f, 0.0f,  0.38f}, {0, 1, 0}, {0.14f, 0.65f}, -0.32f},
        {{-0.86f, 0.0f,  0.00f}, {0, 1, 0}, {0.10f, 0.50f}, -0.05f},
        {{-0.76f, 0.0f, -0.38f}, {0, 1, 0}, {0.14f, 0.35f}, -0.32f},
        {{-0.12f, 0.0f, -1.28f}, {0, 1, 0}, {0.45f, 0.00f}, -1.00f},
        {{ 0.46f, 0.0f, -0.58f}, {0, 1, 0}, {0.72f, 0.27f}, -0.46f},
        {{-0.82f, 0.0f,  0.00f}, {0, 1, 0}, {0.08f, 0.50f}, -0.04f},
        {{-2.16f, 0.0f,  0.00f}, {0, 1, 0}, {0.00f, 0.50f}, -0.03f},
        {{-1.45f, 0.0f,  0.055f}, {0, 1, 0}, {0.03f, 0.53f}, -0.04f}
    };
    const std::vector<std::uint32_t> indices{
        0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5,
        0, 5, 6, 0, 6, 7, 0, 7, 8, 0, 8, 1,
        9, 10, 11
    };
    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    D3D11_SUBRESOURCE_DATA initialData{vertices.data(), 0, 0};
    ThrowIfFailed(device->CreateBuffer(
        &bufferDesc, &initialData, rayVertexBuffer_.GetAddressOf()),
        "CreateBuffer (ray vertices)");
    bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bufferDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
    initialData.pSysMem = indices.data();
    ThrowIfFailed(device->CreateBuffer(
        &bufferDesc, &initialData, rayIndexBuffer_.GetAddressOf()),
        "CreateBuffer (ray indices)");
    rayIndexCount_ = static_cast<std::uint32_t>(indices.size());
}

void FishRenderer::CreatePipeline(
    ID3D11Device* device,
    const std::filesystem::path& shaderPath)
{
    const ComPtr<ID3DBlob> vertexBytecode =
        CompileShader(shaderPath, "VSFish", "vs_5_0");
    const ComPtr<ID3DBlob> pixelBytecode =
        CompileShader(shaderPath, "PSFish", "ps_5_0");
    ThrowIfFailed(device->CreateVertexShader(
        vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(),
        nullptr, vertexShader_.GetAddressOf()), "CreateVertexShader (fish)");
    ThrowIfFailed(device->CreatePixelShader(
        pixelBytecode->GetBufferPointer(), pixelBytecode->GetBufferSize(),
        nullptr, pixelShader_.GetAddressOf()), "CreatePixelShader (fish)");

    const std::array<D3D11_INPUT_ELEMENT_DESC, 8> layout{{
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
        {"INSTANCE_FORWARD_PHASE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16,
         D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE_TINT_SWIM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32,
         D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCE_SPECIES_SHAPE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48,
         D3D11_INPUT_PER_INSTANCE_DATA, 1}
    }};
    ThrowIfFailed(device->CreateInputLayout(
        layout.data(), static_cast<UINT>(layout.size()),
        vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(),
        inputLayout_.GetAddressOf()), "CreateInputLayout (fish)");

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = instanceCapacity_ * sizeof(Instance);
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ThrowIfFailed(device->CreateBuffer(
        &bufferDesc, nullptr, instanceBuffer_.GetAddressOf()),
        "CreateBuffer (fish instances)");
    bufferDesc.ByteWidth = 8u * sizeof(Instance);
    ThrowIfFailed(device->CreateBuffer(
        &bufferDesc, nullptr, rayInstanceBuffer_.GetAddressOf()),
        "CreateBuffer (ray instances)");
    bufferDesc.ByteWidth = sizeof(Constants);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ThrowIfFailed(device->CreateBuffer(
        &bufferDesc, nullptr, constantBuffer_.GetAddressOf()),
        "CreateBuffer (fish constants)");

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ThrowIfFailed(device->CreateDepthStencilState(
        &depthDesc, depthState_.GetAddressOf()),
        "CreateDepthStencilState (fish)");

    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    ThrowIfFailed(device->CreateRasterizerState(
        &rasterizerDesc, rasterizerState_.GetAddressOf()),
        "CreateRasterizerState (fish)");
}

void FishRenderer::ResetHabitat(Habitat habitat)
{
    habitat_ = habitat;
    agents_.clear();
    simulationAccumulator_ = 0.0f;
    if (habitat == Habitat::None)
    {
        return;
    }

    std::uint32_t randomState = habitat == Habitat::UnderwaterArch
        ? 0xA1734C21u : 0x57A7C0DEu;
    auto random01 = [&randomState]()
    {
        randomState = randomState * 1664525u + 1013904223u;
        return static_cast<float>((randomState >> 8u) & 0x00ffffffu) /
            static_cast<float>(0x01000000u);
    };
    const std::uint32_t smallFishPerSchool =
        habitat == Habitat::UnderwaterArch ? 18u : 36u;
    const std::uint32_t mediumFishCount =
        habitat == Habitat::UnderwaterArch ? 9u : 12u;
    agents_.reserve(smallFishPerSchool * 3u + mediumFishCount);
    const auto spawnSchool = [&](std::uint32_t school,
                                 std::uint32_t count,
                                 std::uint32_t species)
    {
        const XMFLOAT3 target = SchoolTarget(school, school * 4.7f);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            const float angle = random01() * kPi * 2.0f;
            const float radius = species == 0u
                ? 0.60f + random01() * 1.65f
                : 1.30f + random01() * 2.10f;
            Agent agent;
            agent.position = {
                target.x + std::cos(angle) * radius,
                target.y + (random01() - 0.5f) * 1.35f,
                target.z + std::sin(angle) * radius * 0.72f};
            agent.velocity = Normalize(
                {0.45f + random01(),
                 (random01() - 0.5f) * 0.16f,
                 (random01() - 0.5f) * 0.42f},
                {1.0f, 0.0f, 0.0f});
            agent.phase = random01() * kPi * 2.0f;
            agent.scale = species == 0u
                ? 0.22f + random01() * 0.12f
                : 0.48f + random01() * 0.18f;
            agent.tint = random01();
            agent.school = school;
            agent.species = species;
            ConstrainToHabitat(agent);
            agents_.push_back(agent);
        }
    };
    for (std::uint32_t school = 0; school < 3u; ++school)
    {
        spawnSchool(school, smallFishPerSchool, 0u);
    }
    // Medium fusilier-like fish use a sparse authored route. Boids still
    // provide local spacing, but their wider initial radius avoids a second
    // dense bait-ball silhouette.
    spawnSchool(3u, mediumFishCount, 1u);
}

XMFLOAT3 FishRenderer::SchoolTarget(std::uint32_t school, float time) const
{
    const float schoolPhase = static_cast<float>(school) * 2.0943951f;
    if (habitat_ == Habitat::UnderwaterArch)
    {
        const float route = time * (school == 3u ? 0.14f : 0.23f) + schoolPhase;
        const float x = 24.0f + std::sin(route) *
            (school == 2u ? 16.0f : 18.5f);
        if (school == 2u)
        {
            const float z = std::sin(route * 0.71f) * 1.55f;
            const float canopy = ArchCanopy(x, z);
            return {
                x,
                canopy + (3.28f - canopy) * 0.58f,
                z};
        }
        const float side = (school == 0u || school == 3u) ? -1.0f : 1.0f;
        return {
            x,
            ArchFloor(x) +
                (school == 3u ? 3.35f : 2.55f) +
                std::sin(route * 0.73f) * 0.42f,
            side * ((school == 3u ? 4.75f : 5.25f) +
                std::cos(route) * 0.78f)};
    }
    const float route = time * 0.19f + schoolPhase;
    return {
        14.2f + std::cos(route) * 5.0f,
        3.35f + std::sin(route * 0.63f + schoolPhase) * 1.65f,
        std::sin(route) * (7.5f + static_cast<float>(school) * 0.75f)};
}

void FishRenderer::ApplyHabitatSteering(
    const Agent& agent,
    XMFLOAT3& steering) const
{
    XMFLOAT3 minimum{};
    XMFLOAT3 maximum{};
    if (habitat_ == Habitat::UnderwaterArch)
    {
        if (agent.school == 2u)
        {
            minimum = {
                3.0f,
                ArchCanopy(agent.position.x, agent.position.z) + 0.22f,
                -2.35f};
            maximum = {45.0f, 3.28f, 2.35f};
        }
        else
        {
        const bool negativeSide = agent.school == 0u || agent.school == 3u;
        minimum = {0.2f, ArchFloor(agent.position.x) + 0.72f,
            negativeSide ? -7.3f : 3.75f};
        maximum = {47.8f, 3.20f,
            negativeSide ? -3.75f : 7.3f};
        }
    }
    else
    {
        minimum = {7.75f, -1.35f, -12.7f};
        maximum = {20.8f, 9.45f, 12.7f};
    }
    constexpr float margin = 1.35f;
    const auto axisPush = [&](float value, float low, float high, float& output)
    {
        if (value < low + margin)
        {
            output += 2.8f * (1.0f - SmoothStep(low, low + margin, value));
        }
        if (value > high - margin)
        {
            output -= 2.8f * SmoothStep(high - margin, high, value);
        }
    };
    axisPush(agent.position.x, minimum.x, maximum.x, steering.x);
    axisPush(agent.position.y, minimum.y, maximum.y, steering.y);
    axisPush(agent.position.z, minimum.z, maximum.z, steering.z);
}

void FishRenderer::ConstrainToHabitat(Agent& agent) const
{
    if (habitat_ == Habitat::UnderwaterArch)
    {
        if (agent.school == 2u)
        {
            agent.position.x = std::clamp(agent.position.x, 3.0f, 45.0f);
            agent.position.z = std::clamp(agent.position.z, -2.35f, 2.35f);
            agent.position.y = std::clamp(
                agent.position.y,
                ArchCanopy(agent.position.x, agent.position.z) + 0.22f,
                3.28f);
            return;
        }
        const bool negativeSide = agent.school == 0u || agent.school == 3u;
        agent.position.x = std::clamp(agent.position.x, 0.2f, 47.8f);
        agent.position.y = std::clamp(
            agent.position.y,
            ArchFloor(agent.position.x) + 0.72f,
            3.20f);
        agent.position.z = negativeSide
            ? std::clamp(agent.position.z, -7.3f, -3.75f)
            : std::clamp(agent.position.z, 3.75f, 7.3f);
    }
    else
    {
        agent.position.x = std::clamp(agent.position.x, 7.75f, 20.8f);
        agent.position.y = std::clamp(agent.position.y, -1.35f, 9.45f);
        agent.position.z = std::clamp(agent.position.z, -12.7f, 12.7f);
    }
}

void FishRenderer::Simulate(float stepSeconds, float totalTime)
{
    constexpr float cellSize = 2.4f;
    constexpr float neighbourRadiusSquared = 2.65f * 2.65f;
    constexpr float separationRadiusSquared = 0.72f * 0.72f;
    std::unordered_map<std::int64_t, std::vector<std::size_t>> grid;
    grid.reserve(agents_.size() * 2u);
    const auto cellCoordinate = [](float value)
    {
        return static_cast<int>(std::floor(value / cellSize));
    };
    for (std::size_t index = 0; index < agents_.size(); ++index)
    {
        const Agent& agent = agents_[index];
        grid[CellKey(
            cellCoordinate(agent.position.x),
            cellCoordinate(agent.position.y),
            cellCoordinate(agent.position.z))].push_back(index);
    }

    std::vector<XMFLOAT3> nextVelocities(agents_.size());
    for (std::size_t index = 0; index < agents_.size(); ++index)
    {
        const Agent& agent = agents_[index];
        const int cellX = cellCoordinate(agent.position.x);
        const int cellY = cellCoordinate(agent.position.y);
        const int cellZ = cellCoordinate(agent.position.z);
        XMFLOAT3 alignment{};
        XMFLOAT3 cohesion{};
        XMFLOAT3 separation{};
        std::uint32_t neighbours = 0;
        for (int z = -1; z <= 1; ++z)
        {
            for (int y = -1; y <= 1; ++y)
            {
                for (int x = -1; x <= 1; ++x)
                {
                    const auto found = grid.find(CellKey(
                        cellX + x, cellY + y, cellZ + z));
                    if (found == grid.end())
                    {
                        continue;
                    }
                    for (const std::size_t otherIndex : found->second)
                    {
                        if (otherIndex == index ||
                            agents_[otherIndex].school != agent.school)
                        {
                            continue;
                        }
                        const Agent& other = agents_[otherIndex];
                        const XMFLOAT3 difference =
                            Subtract(other.position, agent.position);
                        const float distanceSquared = LengthSquared(difference);
                        if (distanceSquared >= neighbourRadiusSquared ||
                            distanceSquared <= 0.00001f)
                        {
                            continue;
                        }
                        alignment = Add(alignment, other.velocity);
                        cohesion = Add(cohesion, other.position);
                        if (distanceSquared < separationRadiusSquared)
                        {
                            separation = Add(
                                separation,
                                Scale(difference, -1.0f / distanceSquared));
                        }
                        ++neighbours;
                    }
                }
            }
        }

        XMFLOAT3 steering{};
        if (neighbours > 0u)
        {
            const float inverseCount = 1.0f / static_cast<float>(neighbours);
            const XMFLOAT3 averageVelocity = Scale(alignment, inverseCount);
            const XMFLOAT3 averagePosition = Scale(cohesion, inverseCount);
            steering = Add(steering,
                Scale(Subtract(averageVelocity, agent.velocity),
                    agent.species == 0u ? 0.72f : 0.44f));
            steering = Add(steering,
                Scale(Normalize(Subtract(averagePosition, agent.position), {}),
                    agent.species == 0u ? 0.27f : 0.12f));
            steering = Add(steering, Scale(
                separation,
                agent.species == 0u ? 1.78f : 1.12f));
        }
        const XMFLOAT3 routeDirection = Normalize(
            Subtract(SchoolTarget(agent.school, totalTime), agent.position),
            agent.velocity);
        steering = Add(steering, Scale(
            routeDirection,
            agent.species == 0u ? 0.78f : 0.92f));
        ApplyHabitatSteering(agent, steering);

        XMFLOAT3 velocity = Add(agent.velocity, Scale(steering, stepSeconds));
        const float speciesSpeedScale = agent.species == 0u ? 1.0f : 0.88f;
        const float minimumSpeed = (habitat_ == Habitat::UnderwaterArch
            ? 0.72f : 0.82f) * speciesSpeedScale;
        const float maximumSpeed = (habitat_ == Habitat::UnderwaterArch
            ? 1.52f : 1.78f) * speciesSpeedScale;
        const float speed = Length(velocity);
        if (speed < minimumSpeed)
        {
            velocity = Scale(Normalize(velocity, agent.velocity), minimumSpeed);
        }
        else if (speed > maximumSpeed)
        {
            velocity = Scale(velocity, maximumSpeed / speed);
        }
        nextVelocities[index] = velocity;
    }

    for (std::size_t index = 0; index < agents_.size(); ++index)
    {
        Agent& agent = agents_[index];
        agent.velocity = nextVelocities[index];
        agent.position = Add(agent.position, Scale(agent.velocity, stepSeconds));
        ConstrainToHabitat(agent);
    }
}

bool FishRenderer::IsVisible(
    const Agent& agent,
    const DirectX::XMMATRIX& viewProjection,
    const XMFLOAT3& cameraPosition) const
{
    using namespace DirectX;
    const XMFLOAT3 cameraDelta = Subtract(agent.position, cameraPosition);
    if (LengthSquared(cameraDelta) > 92.0f * 92.0f)
    {
        return false;
    }
    const XMVECTOR clip = XMVector4Transform(
        XMVectorSet(agent.position.x, agent.position.y, agent.position.z, 1.0f),
        viewProjection);
    const float x = XMVectorGetX(clip);
    const float y = XMVectorGetY(clip);
    const float z = XMVectorGetZ(clip);
    const float w = XMVectorGetW(clip);
    const float margin = std::max(w * 0.10f, 0.08f);
    return w > 0.0f && x >= -w - margin && x <= w + margin &&
        y >= -w - margin && y <= w + margin && z >= -margin && z <= w + margin;
}

void FishRenderer::BuildRayInstances(
    const DirectX::XMMATRIX& viewProjection,
    const XMFLOAT3& cameraPosition,
    float totalTime)
{
    visibleRayInstances_.clear();
    const std::uint32_t rayCount =
        habitat_ == Habitat::UnderwaterArch ? 2u : 3u;
    const auto rayPosition = [&](std::uint32_t index, float time)
    {
        const float phase = static_cast<float>(index) * 2.37f;
        if (habitat_ == Habitat::UnderwaterArch)
        {
            const float route = time * (index == 0u ? 0.115f : 0.092f) + phase;
            // Keep the hero ray on a compact loop over the first half of the
            // tunnel so the species reads from the route's establishing view.
            const float x = index == 0u
                ? 13.0f + std::sin(route) * 8.5f
                : 24.0f + std::sin(route) * 17.0f;
            if (index == 0u)
            {
                const float z = std::cos(route * 0.73f) * 1.35f;
                const float canopy = ArchCanopy(x, z);
                return XMFLOAT3{
                    x,
                    canopy + (3.30f - canopy) * 0.70f,
                    z};
            }
            return XMFLOAT3{
                x,
                std::min(ArchFloor(x) + 3.25f, 3.02f),
                4.85f + std::cos(route * 0.81f) * 0.72f};
        }
        const float route = time * (0.082f + index * 0.009f) + phase;
        return XMFLOAT3{
            14.4f + std::cos(route) * (4.6f + index * 0.35f),
            3.8f + std::sin(route * 0.67f + phase) * 1.35f,
            std::sin(route) * (7.2f + index * 0.55f)};
    };
    for (std::uint32_t index = 0; index < rayCount; ++index)
    {
        const XMFLOAT3 position = rayPosition(index, totalTime);
        Agent visibilityProxy;
        visibilityProxy.position = position;
        if (!IsVisible(visibilityProxy, viewProjection, cameraPosition))
        {
            continue;
        }
        const XMFLOAT3 nextPosition = rayPosition(index, totalTime + 0.08f);
        const XMFLOAT3 forward = Normalize(
            Subtract(nextPosition, position),
            {1.0f, 0.0f, 0.0f});
        const float scale = habitat_ == Habitat::UnderwaterArch
            ? (index == 0u ? 1.18f : 0.78f)
            : (0.92f + index * 0.10f);
        visibleRayInstances_.push_back({
            {position.x, position.y, position.z, scale},
            {forward.x, forward.y, forward.z,
             0.73f + static_cast<float>(index) * 1.91f},
            {0.035f, 0.115f, 0.205f, 2.05f + index * 0.13f},
            {2.0f, 1.0f, 1.0f, 0.72f}
        });
    }
}

void FishRenderer::UploadInstances(
    ID3D11DeviceContext* context,
    ID3D11Buffer* buffer,
    const std::vector<Instance>& instances) const
{
    if (instances.empty())
    {
        return;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    ThrowIfFailed(context->Map(
        buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
        "Map (biology instances)");
    std::memcpy(
        mapped.pData,
        instances.data(),
        instances.size() * sizeof(Instance));
    context->Unmap(buffer, 0);
}

void FishRenderer::Render(
    ID3D11DeviceContext* context,
    const DirectX::XMMATRIX& viewProjection,
    const XMFLOAT3& cameraPosition,
    float totalTime,
    float deltaTime,
    Habitat habitat)
{
    if (habitat != habitat_)
    {
        ResetHabitat(habitat);
    }
    if (habitat == Habitat::None || agents_.empty())
    {
        return;
    }

    simulationAccumulator_ += std::clamp(deltaTime, 0.0f, 0.05f);
    constexpr float simulationStep = 1.0f / 30.0f;
    int stepCount = 0;
    while (simulationAccumulator_ >= simulationStep && stepCount < 2)
    {
        Simulate(simulationStep, totalTime);
        simulationAccumulator_ -= simulationStep;
        ++stepCount;
    }
    if (stepCount == 2)
    {
        simulationAccumulator_ = std::min(simulationAccumulator_, simulationStep);
    }

    visibleInstances_.clear();
    visibleInstances_.reserve(agents_.size());
    for (const Agent& agent : agents_)
    {
        if (!IsVisible(agent, viewProjection, cameraPosition))
        {
            continue;
        }
        const XMFLOAT3 forward = Normalize(agent.velocity, {1.0f, 0.0f, 0.0f});
        const bool mediumSpecies = agent.species == 1u;
        const float silver = mediumSpecies
            ? 0.39f + agent.tint * 0.10f
            : 0.58f + agent.tint * 0.14f;
        const bool overheadSilhouette =
            habitat_ == Habitat::UnderwaterArch && agent.school == 2u;
        visibleInstances_.push_back({
            {agent.position.x, agent.position.y, agent.position.z, agent.scale},
            {forward.x, forward.y, forward.z, agent.phase},
            {mediumSpecies
                 ? 0.20f + agent.tint * 0.08f
                 : 0.18f + agent.tint * 0.10f,
             mediumSpecies
                 ? 0.34f + agent.tint * 0.10f
                 : 0.42f + agent.tint * 0.13f,
             silver,
             (overheadSilhouette ? -1.0f : 1.0f) *
                 (mediumSpecies ? 3.15f : 4.3f + agent.tint * 1.5f)},
            {static_cast<float>(agent.species),
             mediumSpecies ? 1.24f : 1.0f,
             mediumSpecies ? 1.12f : 1.0f,
             mediumSpecies ? 0.58f : 0.36f}
        });
    }
    BuildRayInstances(viewProjection, cameraPosition, totalTime);
    if (visibleInstances_.empty() && visibleRayInstances_.empty())
    {
        return;
    }

    UploadInstances(context, instanceBuffer_.Get(), visibleInstances_);
    UploadInstances(context, rayInstanceBuffer_.Get(), visibleRayInstances_);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    ThrowIfFailed(context->Map(
        constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
        "Map (fish constants)");
    auto* constants = static_cast<Constants*>(mapped.pData);
    DirectX::XMStoreFloat4x4(&constants->viewProjection, viewProjection);
    constants->cameraTime = {
        cameraPosition.x, cameraPosition.y, cameraPosition.z, totalTime};
    constants->waterParameters = habitat == Habitat::UnderwaterArch
        ? DirectX::XMFLOAT4{3.55f, 0.080f, 0.038f, 0.020f}
        : DirectX::XMFLOAT4{10.20f, 0.060f, 0.027f, 0.014f};
    context->Unmap(constantBuffer_.Get(), 0);

    context->IASetInputLayout(inputLayout_.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    ID3D11Buffer* constantBuffer = constantBuffer_.Get();
    context->VSSetConstantBuffers(4, 1, &constantBuffer);
    context->PSSetShader(pixelShader_.Get(), nullptr, 0);
    context->PSSetConstantBuffers(4, 1, &constantBuffer);
    context->RSSetState(rasterizerState_.Get());
    context->OMSetBlendState(nullptr, nullptr, 0xffffffffu);
    context->OMSetDepthStencilState(depthState_.Get(), 0);
    const auto drawInstances = [&](ID3D11Buffer* vertices,
                                   ID3D11Buffer* indices,
                                   ID3D11Buffer* instances,
                                   UINT indexCount,
                                   UINT instanceCount)
    {
        if (instanceCount == 0u)
        {
            return;
        }
        ID3D11Buffer* buffers[] = {vertices, instances};
        const UINT strides[] = {sizeof(Vertex), sizeof(Instance)};
        const UINT offsets[] = {0, 0};
        context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        context->IASetIndexBuffer(indices, DXGI_FORMAT_R32_UINT, 0);
        context->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
    };
    drawInstances(
        vertexBuffer_.Get(), indexBuffer_.Get(), instanceBuffer_.Get(),
        indexCount_, static_cast<UINT>(visibleInstances_.size()));
    drawInstances(
        rayVertexBuffer_.Get(), rayIndexBuffer_.Get(), rayInstanceBuffer_.Get(),
        rayIndexCount_, static_cast<UINT>(visibleRayInstances_.size()));

    context->OMSetDepthStencilState(nullptr, 0);
    context->RSSetState(nullptr);
}
