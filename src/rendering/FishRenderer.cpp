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

struct FishShaderBytecode
{
    ComPtr<ID3DBlob> vertex;
    ComPtr<ID3DBlob> pixel;
};

const FishShaderBytecode& GetFishShaderBytecode(const std::filesystem::path& path)
{
    static std::filesystem::path cachedPath;
    static FishShaderBytecode cached;
    if(!cached.vertex||cachedPath!=path)
    {
        cachedPath=path;
        cached.vertex=CompileShader(path,"VSFish","vs_5_0");
        cached.pixel=CompileShader(path,"PSFish","ps_5_0");
    }
    return cached;
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

XMFLOAT3 LimitLength(const XMFLOAT3& value, float maximum)
{
    const float length = Length(value);
    return length > maximum && length > 0.00001f
        ? Scale(value, maximum / length)
        : value;
}

XMFLOAT3 TransformDirection(const XMFLOAT3& value,float yaw)
{
    const float c=std::cos(yaw),s=std::sin(yaw);
    return {value.x*c+value.z*s,value.y,-value.x*s+value.z*c};
}

XMFLOAT3 TransformPosition(const XMFLOAT3& value,const FishRenderer::Presentation& p)
{
    auto result=Add(TransformDirection(value,p.yawRadians),p.translation);
    result.z-=p.archExtension*std::clamp((57.f-result.z)/12.f,0.f,1.f);
    return result;
}

XMFLOAT3 InverseTransformPosition(const XMFLOAT3& value,const FishRenderer::Presentation& p)
{
    return TransformDirection(Subtract(value,p.translation),-p.yawRadians);
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
    CreateLowDetailGeometry(device);
    CreateRayGeometry(device);
    CreatePipeline(device, shaderPath);
}

void FishRenderer::CreateLowDetailGeometry(ID3D11Device* device)
{
    // Ten-triangle octahedral silhouette for distant schoolers. It preserves
    // thickness from head-on and top-down views unlike a camera-facing card,
    // while using less than eight percent of the hero fish triangles.
    const std::vector<Vertex> vertices{
        {{ 0.68f,  0.00f,  0.00f}, { 1, 0, 0}, {1.0f, 0.5f}, 0.00f,0.0f},
        {{-0.58f,  0.00f,  0.00f}, {-1, 0, 0}, {0.2f, 0.5f}, 0.72f,0.0f},
        {{ 0.00f,  0.22f,  0.00f}, { 0, 1, 0}, {0.6f, 0.0f}, 0.20f,0.0f},
        {{ 0.00f, -0.22f,  0.00f}, { 0,-1, 0}, {0.6f, 1.0f}, 0.20f,0.0f},
        {{ 0.00f,  0.00f,  0.12f}, { 0, 0, 1}, {0.6f, 0.5f}, 0.20f,0.0f},
        {{ 0.00f,  0.00f, -0.12f}, { 0, 0,-1}, {0.6f, 0.5f}, 0.20f,0.0f},
        {{-1.08f,  0.31f,  0.00f}, { 0, 0, 1}, {0.0f, 0.0f}, 1.00f,1.0f},
        {{-1.08f, -0.31f,  0.00f}, { 0, 0, 1}, {0.0f, 1.0f}, 1.00f,1.0f}
    };
    const std::vector<std::uint32_t> indices{
        0, 2, 4, 0, 4, 3, 0, 3, 5, 0, 5, 2,
        1, 4, 2, 1, 3, 4, 1, 5, 3, 1, 2, 5,
        1, 6, 7, 1, 7, 6
    };
    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    D3D11_SUBRESOURCE_DATA initialData{vertices.data(), 0, 0};
    ThrowIfFailed(device->CreateBuffer(
        &bufferDesc, &initialData, lowDetailVertexBuffer_.GetAddressOf()),
        "CreateBuffer (low-detail fish vertices)");
    bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bufferDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
    initialData.pSysMem = indices.data();
    ThrowIfFailed(device->CreateBuffer(
        &bufferDesc, &initialData, lowDetailIndexBuffer_.GetAddressOf()),
        "CreateBuffer (low-detail fish indices)");
    lowDetailIndexCount_ = static_cast<std::uint32_t>(indices.size());
}

void FishRenderer::CreateGeometry(ID3D11Device* device)
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    // 輪郭を滑らかにしつつ、群泳時の頂点負荷を抑えられる分割数に留める。
    constexpr int longitudinalSegments = 12;
    constexpr int radialSegments = 10;
    for (int longitudinal = 0; longitudinal <= longitudinalSegments; ++longitudinal)
    {
        const float u = static_cast<float>(longitudinal) /
            static_cast<float>(longitudinalSegments);
        const float x = -0.70f + u * 1.40f;
        const float profile = std::max(
            0.025f,
            std::pow(std::sin(u * kPi), 0.64f));
        for (int radial = 0; radial <= radialSegments; ++radial)
        {
            const float v = static_cast<float>(radial) /
                static_cast<float>(radialSegments);
            const float angle = v * kPi * 2.0f;
            const float y = std::cos(angle) * 0.235f * profile;
            const float z = std::sin(angle) * 0.125f * profile;
            const XMFLOAT3 normal = Normalize(
                {x / (0.74f * 0.74f),
                 y / (0.235f * 0.235f),
                 z / (0.125f * 0.125f)},
                {0.0f, 1.0f, 0.0f});
            const float tailWeight = (1.0f - u) * (1.0f - u);
            vertices.push_back({{x, y, z}, normal, {u, v}, tailWeight, 0.0f});
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

    // 中央に切れ込みを持つ二股尾。旧来の菱形より停止画でも魚に見える。
    const std::uint32_t tailBase = static_cast<std::uint32_t>(vertices.size());
    vertices.insert(vertices.end(), {
        {{-0.58f, 0.0f, 0.0f}, {0, 0, 1}, {0.0f, 0.5f}, 0.82f, 1.0f},
        {{-1.12f, 0.40f, 0.0f}, {0, 0, 1}, {1.0f, 0.0f}, 1.00f, 1.0f},
        {{-0.92f, 0.0f, 0.0f}, {0, 0, 1}, {0.66f, 0.5f}, 1.00f, 1.0f},
        {{-1.12f,-0.40f, 0.0f}, {0, 0, 1}, {1.0f, 1.0f}, 1.00f, 1.0f}
    });
    indices.insert(indices.end(), {
        tailBase, tailBase + 1, tailBase + 2,
        tailBase, tailBase + 2, tailBase + 3
    });

    // 背びれと左右の胸びれ。種別ごとの大きさは頂点シェーダーで変える。
    const std::uint32_t dorsalBase = static_cast<std::uint32_t>(vertices.size());
    vertices.insert(vertices.end(), {
        {{ 0.24f,0.18f,0.0f},{0,0,1},{0.0f,1.0f},0.18f,2.0f},
        {{-0.28f,0.20f,0.0f},{0,0,1},{1.0f,1.0f},0.42f,2.0f},
        {{-0.12f,0.52f,0.0f},{0,0,1},{0.7f,0.0f},0.34f,2.0f},
        {{ 0.18f,-0.02f, 0.09f},{0,1,0},{0.0f,0.0f},0.14f,3.0f},
        {{-0.22f,-0.06f, 0.10f},{0,1,0},{0.5f,0.0f},0.42f,3.0f},
        {{-0.34f,-0.16f, 0.43f},{0,1,0},{1.0f,1.0f},0.55f,3.0f},
        {{ 0.18f,-0.02f,-0.09f},{0,1,0},{0.0f,0.0f},0.14f,3.0f},
        {{-0.34f,-0.16f,-0.43f},{0,1,0},{1.0f,1.0f},0.55f,3.0f},
        {{-0.22f,-0.06f,-0.10f},{0,1,0},{0.5f,0.0f},0.42f,3.0f}
    });
    indices.insert(indices.end(), {
        dorsalBase,dorsalBase+1,dorsalBase+2,
        dorsalBase+3,dorsalBase+4,dorsalBase+5,
        dorsalBase+6,dorsalBase+7,dorsalBase+8
    });

    // 左右へ独立した目を置き、近距離でも頭の向きを瞬時に読めるようにする。
    const std::uint32_t eyeBase = static_cast<std::uint32_t>(vertices.size());
    vertices.insert(vertices.end(), {
        {{0.52f,0.105f, 0.082f},{0,0, 1},{0.5f,0.0f},0.0f,4.0f},
        {{0.57f,0.070f, 0.082f},{0,0, 1},{1.0f,0.5f},0.0f,4.0f},
        {{0.52f,0.035f, 0.082f},{0,0, 1},{0.5f,1.0f},0.0f,4.0f},
        {{0.47f,0.070f, 0.082f},{0,0, 1},{0.0f,0.5f},0.0f,4.0f},
        {{0.52f,0.105f,-0.082f},{0,0,-1},{0.5f,0.0f},0.0f,4.0f},
        {{0.47f,0.070f,-0.082f},{0,0,-1},{0.0f,0.5f},0.0f,4.0f},
        {{0.52f,0.035f,-0.082f},{0,0,-1},{0.5f,1.0f},0.0f,4.0f},
        {{0.57f,0.070f,-0.082f},{0,0,-1},{1.0f,0.5f},0.0f,4.0f}
    });
    indices.insert(indices.end(), {
        eyeBase,eyeBase+1,eyeBase+2, eyeBase,eyeBase+2,eyeBase+3,
        eyeBase+4,eyeBase+5,eyeBase+6, eyeBase+4,eyeBase+6,eyeBase+7
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
    // 厚みのある中央胴、後退翼、頭鰭、細い尾でエイ固有の輪郭を作る。
    // 負のbendWeightはFish.hlslの翼羽ばたき経路を選択する。
    std::vector<Vertex> vertices{
        {{ 0.08f, 0.28f, 0.00f},{0,1,0},{0.54f,0.50f},-0.06f,0.0f}, // 0 上面中央
        {{ 1.16f, 0.03f, 0.00f},{0,1,0},{1.00f,0.50f},-0.04f,0.0f}, // 1 吻端
        {{ 0.48f, 0.02f, 0.70f},{0,1,0},{0.76f,0.74f},-0.48f,0.0f}, // 2 右翼前縁
        {{-0.08f,-0.02f, 1.46f},{0,1,0},{0.48f,1.00f},-1.00f,0.0f}, // 3 右翼端
        {{-0.72f, 0.01f, 0.58f},{0,1,0},{0.18f,0.70f},-0.44f,0.0f}, // 4 右翼後縁
        {{-0.84f, 0.10f, 0.00f},{0,1,0},{0.12f,0.50f},-0.05f,0.0f}, // 5 尾根元
        {{-0.72f, 0.01f,-0.58f},{0,1,0},{0.18f,0.30f},-0.44f,0.0f}, // 6 左翼後縁
        {{-0.08f,-0.02f,-1.46f},{0,1,0},{0.48f,0.00f},-1.00f,0.0f}, // 7 左翼端
        {{ 0.48f, 0.02f,-0.70f},{0,1,0},{0.76f,0.26f},-0.48f,0.0f}, // 8 左翼前縁
        {{ 0.08f,-0.22f, 0.00f},{0,-1,0},{0.54f,0.50f},-0.06f,0.0f}, // 9 下面中央
        {{ 0.92f,-0.02f, 0.27f},{0,-1,0},{0.91f,0.60f},-0.18f,0.0f}, // 10 右頭鰭
        {{ 1.28f,-0.01f, 0.18f},{0,-1,0},{1.00f,0.57f},-0.16f,0.0f}, // 11
        {{ 0.92f,-0.02f,-0.27f},{0,-1,0},{0.91f,0.40f},-0.18f,0.0f}, // 12 左頭鰭
        {{ 1.28f,-0.01f,-0.18f},{0,-1,0},{1.00f,0.43f},-0.16f,0.0f}, // 13
        {{-0.76f, 0.08f, 0.045f},{0,1,0},{0.10f,0.52f},-0.03f,0.0f}, // 14 尾上
        {{-2.85f, 0.02f, 0.018f},{0,1,0},{0.00f,0.51f},-0.02f,0.0f}, // 15 尾先
        {{-0.76f, 0.02f,-0.045f},{0,1,0},{0.10f,0.48f},-0.03f,0.0f}  // 16 尾下
    };
    // 翼外周にも下面を持たせ、横から見ても紙のように消えない厚みを作る。
    constexpr std::uint32_t perimeter[]{1,2,3,4,5,6,7,8};
    const std::uint32_t lowerRimBase=static_cast<std::uint32_t>(vertices.size());
    for(const std::uint32_t vertexIndex:perimeter)
    {
        Vertex lower=vertices[vertexIndex];
        lower.position.y-=(vertexIndex==3||vertexIndex==7)?.16f:.26f;
        lower.normal={0,-1,0};
        vertices.push_back(lower);
    }
    std::vector<std::uint32_t> indices{
        0,1,2, 0,2,3, 0,3,4, 0,4,5,
        0,5,6, 0,6,7, 0,7,8, 0,8,1,
        10,11,1, 12,1,13,
        14,15,16
    };
    for(std::uint32_t index=0;index<8;++index)
    {
        const std::uint32_t next=(index+1)%8;
        const std::uint32_t topA=perimeter[index],topB=perimeter[next];
        const std::uint32_t bottomA=lowerRimBase+index,bottomB=lowerRimBase+next;
        indices.insert(indices.end(),{
            9,bottomB,bottomA,
            topA,bottomA,bottomB,topA,bottomB,topB});
    }
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
    const FishShaderBytecode& bytecode=GetFishShaderBytecode(shaderPath);
    ID3DBlob* vertexBytecode=bytecode.vertex.Get();
    ID3DBlob* pixelBytecode=bytecode.pixel.Get();
    ThrowIfFailed(device->CreateVertexShader(
        vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(),
        nullptr, vertexShader_.GetAddressOf()), "CreateVertexShader (fish)");
    ThrowIfFailed(device->CreatePixelShader(
        pixelBytecode->GetBufferPointer(), pixelBytecode->GetBufferSize(),
        nullptr, pixelShader_.GetAddressOf()), "CreatePixelShader (fish)");

    const std::array<D3D11_INPUT_ELEMENT_DESC, 9> layout{{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 32,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 0, 36,
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
    ThrowIfFailed(device->CreateBuffer(
        &bufferDesc, nullptr, lowDetailInstanceBuffer_.GetAddressOf()),
        "CreateBuffer (low-detail fish instances)");
    ThrowIfFailed(device->CreateBuffer(
        &bufferDesc, nullptr, floorShadowInstanceBuffer_.GetAddressOf()),
        "CreateBuffer (floor shadow instances)");
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

    // 魚影は砂面の奥行きを壊さず、既存の岩には正しく遮蔽される。
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    ThrowIfFailed(device->CreateDepthStencilState(
        &depthDesc, shadowDepthState_.GetAddressOf()),
        "CreateDepthStencilState (fish floor shadow)");

    D3D11_BLEND_DESC shadowBlendDesc{};
    auto& shadowTarget = shadowBlendDesc.RenderTarget[0];
    shadowTarget.BlendEnable = TRUE;
    shadowTarget.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    shadowTarget.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    shadowTarget.BlendOp = D3D11_BLEND_OP_ADD;
    shadowTarget.SrcBlendAlpha = D3D11_BLEND_ONE;
    shadowTarget.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    shadowTarget.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    shadowTarget.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ThrowIfFailed(device->CreateBlendState(
        &shadowBlendDesc, shadowBlendState_.GetAddressOf()),
        "CreateBlendState (fish floor shadow)");

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
        ? 0xA1734C21u
        : (habitat == Habitat::ReceptionHeroTank
            ? 0x9E3779B9u : 0x57A7C0DEu);
    auto random01 = [&randomState]()
    {
        randomState = randomState * 1664525u + 1013904223u;
        return static_cast<float>((randomState >> 8u) & 0x00ffffffu) /
            static_cast<float>(0x01000000u);
    };
    const std::uint32_t smallFishPerSchool = habitat == Habitat::UnderwaterArch
        ? 15u : (habitat == Habitat::ReceptionHeroTank ? 144u : 200u);
    // 大水槽は一塊に増量せず、色と遊泳層の異なる三群で密度を出す。
    // 遠距離では既存の軽量メッシュへ落ちるため、描画回数は増えない。
    const std::uint32_t smallSchoolCount = habitat == Habitat::UnderwaterArch
        ? 3u : (habitat == Habitat::ReceptionHeroTank ? 3u : 1u);
    const std::uint32_t entranceFishCount =
        habitat == Habitat::UnderwaterArch ? 9u : 0u;
    const std::uint32_t mediumFishCount =
        habitat == Habitat::UnderwaterArch
            ? 9u : (habitat == Habitat::ReceptionHeroTank ? 8u : 12u);
    agents_.reserve(
        smallFishPerSchool * smallSchoolCount + entranceFishCount + mediumFishCount);
    const auto spawnSchool = [&](std::uint32_t school,
                                 std::uint32_t count,
                                 std::uint32_t species)
    {
        const XMFLOAT3 target = SchoolTarget(school, school * 4.7f);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            const float angle = random01() * kPi * 2.0f;
            const bool denseSchool = species == 0u && habitat != Habitat::UnderwaterArch;
            const float radius = species == 0u
                ? (denseSchool ? 2.1f * std::sqrt(random01())
                               : 0.60f + random01() * 1.65f)
                : 1.30f + random01() * 2.10f;
            Agent agent;
            agent.position = {
                target.x + std::cos(angle) * radius,
                target.y + (random01() - 0.5f) * (denseSchool ? 2.0f : 1.35f),
                target.z + std::sin(angle) * radius * 0.72f};
            agent.velocity = Normalize(
                {0.45f + random01(),
                 (random01() - 0.5f) * 0.16f,
                 (random01() - 0.5f) * 0.42f},
                {1.0f, 0.0f, 0.0f});
            agent.phase = random01() * kPi * 2.0f;
            agent.scale = species == 0u
                ? (denseSchool ? 0.13f + random01() * 0.07f
                               : 0.22f + random01() * 0.12f)
                : 0.48f + random01() * 0.18f;
            agent.tint = random01();
            agent.school = school;
            agent.species = species;
            ConstrainToHabitat(agent);
            agents_.push_back(agent);
        }
    };
    for (std::uint32_t school = 0; school < smallSchoolCount; ++school)
    {
        spawnSchool(school, smallFishPerSchool, 0u);
    }
    // Medium fusilier-like fish use a sparse authored route. Boids still
    // provide local spacing, but their wider initial radius avoids a second
    // dense bait-ball silhouette.
    spawnSchool(3u, mediumFishCount, 1u);
    if (entranceFishCount > 0u)
    {
        // A small persistent welcome school keeps the first metres alive while
        // the other schools traverse the full 48 m exhibit.
        spawnSchool(4u, entranceFishCount, 0u);
    }
}

XMFLOAT3 FishRenderer::SchoolTarget(std::uint32_t school, float time) const
{
    const float schoolPhase = static_cast<float>(school) * 2.0943951f;
    if (habitat_ == Habitat::UnderwaterArch)
    {
        // Aquarium fish cruise slowly during exploration. The chase applies a
        // separate common acceleration, so calm movement need not be sped up.
        const float routeSpeed = school == 3u
            ? 0.030f : (school == 4u ? 0.035f : 0.046f);
        const float route = time * routeSpeed + schoolPhase;
        if (school == 4u)
        {
            const float x = 7.0f + std::sin(route) * 5.6f;
            return {
                x,
                ArchFloor(x) + 2.35f + std::sin(route * 0.81f) * 0.34f,
                4.85f + std::cos(route) * 0.68f};
        }
        const float x = school == 0u
            ? 15.5f + std::sin(route) * 15.0f
            : (school == 1u
                ? 30.0f + std::sin(route) * 17.0f
                : (school == 2u
                    ? 24.0f + std::sin(route) * 20.0f
                    : 24.0f + std::sin(route) * 21.5f));
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
    if (habitat_ == Habitat::ReceptionHeroTank)
    {
        if (school == 0u)
        {
            // One shared, continuous target keeps the school together. Slow
            // incommensurate waves vary its depth and occasional turns without
            // random per-fish impulses that would scatter the formation.
            const float turn = std::pow(std::max(0.0f, std::sin(time * 0.14f)), 6.0f);
            const float route = time * 0.135f + std::sin(time * 0.053f) * 0.65f;
            return {std::cos(route) * 5.5f + std::sin(time * 0.83f) * turn * 0.8f,
                4.1f + std::sin(time * 0.117f) * 1.35f,
                11.8f + std::sin(route * 0.79f) * 1.85f};
        }
        const float speed = school == 0u ? 0.17f
            : (school == 1u ? 0.135f : (school == 2u ? 0.105f : 0.082f));
        const float route = time * speed + schoolPhase;
        const float layerY = school == 0u ? 5.95f
            : (school == 1u ? 2.15f : (school == 2u ? -0.25f : 3.65f));
        return {
            std::cos(route) * (school == 3u ? 5.3f : 4.6f),
            layerY + std::sin(route * 0.63f + schoolPhase) * 0.62f,
            12.0f + std::sin(route) * (school == 3u ? 2.6f : 3.2f)};
    }
    // Ogasawara composition: three small-fish layers orbit at distinct depth
    // and speed, while the sparse medium group crosses the open blue channel.
    // Separate authored targets keep one giant Boids blob from filling the
    // whole tank without adding simulation agents or draw calls.
    const float routeSpeed = school == 0u
        ? 0.18f
        : (school == 1u ? 0.14f : (school == 2u ? 0.108f : 0.078f));
    const float route = time * routeSpeed + schoolPhase;
    if (school == 0u)
    {
        return {13.1f + std::cos(route) * 4.7f,
            7.15f + std::sin(route * 0.71f) * 0.82f,
            std::sin(route) * 8.6f};
    }
    if (school == 1u)
    {
        return {14.6f + std::cos(route) * 5.2f,
            4.65f + std::sin(route * 0.59f + 0.8f) * 1.05f,
            std::sin(route) * 10.0f};
    }
    if (school == 2u)
    {
        return {13.8f + std::cos(route) * 4.1f,
            2.15f + std::sin(route * 0.53f + 1.7f) * 0.72f,
            std::sin(route) * 7.1f};
    }
    return {14.7f + std::cos(route) * 5.5f,
        4.05f + std::sin(route * 0.47f) * 1.55f,
        std::sin(route) * 8.2f};
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
    else if (habitat_ == Habitat::ReceptionHeroTank)
    {
        minimum = {-8.05f, -1.45f, 8.35f};
        maximum = {8.05f, 7.55f, 15.60f};
        // U字型岩礁の高い両翼だけを避け、低い中央には魚を通す。
        const float reef = SmoothStep(10.7f,12.3f,agent.position.z);
        const float side = SmoothStep(3.0f,6.5f,std::abs(agent.position.x));
        minimum.y=-1.42f+reef*(.62f+side*5.05f);
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
    else if (habitat_ == Habitat::ReceptionHeroTank)
    {
        agent.position.x = std::clamp(agent.position.x, -8.05f, 8.05f);
        agent.position.z = std::clamp(agent.position.z, 8.35f, 15.60f);
        const float reef=SmoothStep(10.7f,12.3f,agent.position.z);
        const float side=SmoothStep(3.0f,6.5f,std::abs(agent.position.x));
        const float reefFloor=-1.42f+reef*(.62f+side*5.05f);
        agent.position.y = std::clamp(agent.position.y, reefFloor, 7.55f);
        if (agent.position.y <= reefFloor && agent.velocity.y < 0.0f)
        {
            agent.velocity.y = 0.0f;
        }
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
    constexpr float cellSize = 2.65f;
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
                    // Bound dense-school work to twelve candidates per cell.
                    // A stable agent-dependent offset avoids selecting the
                    // same leaders for every fish, without frame-to-frame noise.
                    const auto& cell = found->second;
                    const std::size_t candidateCount = agents_.size() <= 256u
                        ? cell.size() : std::min(cell.size(), std::size_t{12});
                    const std::size_t start = (index * 2654435761u) % cell.size();
                    for (std::size_t candidate = 0; candidate < candidateCount; ++candidate)
                    {
                        const std::size_t otherIndex = cell[
                            (start + candidate * cell.size() / candidateCount) % cell.size()];
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
                        const float spacing = agent.species == 0u &&
                            habitat_ != Habitat::UnderwaterArch
                                ? 0.24f * 0.24f : separationRadiusSquared;
                        if (distanceSquared < spacing)
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
        if(habitat_==Habitat::UnderwaterArch&&fleeToEntrance_>.001f)
        {
            // Combined route entrance is local -X. Every school keeps its
            // spacing while a strong common acceleration sells a real escape.
            steering.x-=8.6f*fleeToEntrance_;
            steering.y+=std::sin(totalTime*4.f+agent.phase)*.22f*fleeToEntrance_;
        }
        ApplyHabitatSteering(agent, steering);
        // 極端に近い個体の反発を上限処理し、1フレームの方向反転を防ぐ。
        const float steeringLimit=habitat_==Habitat::UnderwaterArch&&
            fleeToEntrance_>.001f?9.0f:2.25f;
        steering=LimitLength(steering,steeringLimit);

        XMFLOAT3 targetVelocity = Add(agent.velocity, Scale(steering, stepSeconds));
        const float speciesSpeedScale = agent.species == 0u ? 1.0f : 0.88f;
        const float minimumSpeed = (habitat_ == Habitat::UnderwaterArch
            ? 0.18f : 0.48f) * speciesSpeedScale;
        const float maximumSpeed = (habitat_ == Habitat::UnderwaterArch
            ? 0.46f+4.00f*fleeToEntrance_ : 1.18f) * speciesSpeedScale;
        const float speed = Length(targetVelocity);
        if (speed < minimumSpeed)
        {
            targetVelocity = Scale(
                Normalize(targetVelocity, agent.velocity), minimumSpeed);
        }
        else if (speed > maximumSpeed)
        {
            targetVelocity = Scale(targetVelocity, maximumSpeed / speed);
        }
        // 目標速度への指数補間で群れの細かなびくつきを吸収する。
        const float response=1-std::exp(-stepSeconds*(
            fleeToEntrance_>.001f?7.0f:2.6f));
        const XMFLOAT3 velocity=Add(
            Scale(agent.velocity,1-response),Scale(targetVelocity,response));
        nextVelocities[index] = velocity;
    }

    for (std::size_t index = 0; index < agents_.size(); ++index)
    {
        Agent& agent = agents_[index];
        agent.velocity = nextVelocities[index];
        agent.position = Add(agent.position, Scale(agent.velocity, stepSeconds));
        ConstrainToHabitat(agent);
    }
    if(habitat_==Habitat::UnderwaterArch&&fleeToEntrance_>.55f)
    {
        // 入口まで逃げ切った個体は境界へ貼り付けず、展示から退場させる。
        std::erase_if(agents_,[](const Agent& agent){return agent.position.x<=.205f;});
        // 逃走演出が完了した時点で、遠方に残った個体も展示外へ抜けた扱いにする。
        if(fleeToEntrance_>.995f)agents_.clear();
    }
}

bool FishRenderer::IsVisible(
    const XMFLOAT3& position,
    const DirectX::XMMATRIX& viewProjection,
    const XMFLOAT3& cameraPosition) const
{
    using namespace DirectX;
    const XMFLOAT3 cameraDelta = Subtract(position, cameraPosition);
    if (LengthSquared(cameraDelta) > 92.0f * 92.0f)
    {
        return false;
    }
    const XMVECTOR clip = XMVector4Transform(
        XMVectorSet(position.x, position.y, position.z, 1.0f),
        viewProjection);
    const float x = XMVectorGetX(clip);
    const float y = XMVectorGetY(clip);
    const float z = XMVectorGetZ(clip);
    const float w = XMVectorGetW(clip);
    const float margin = std::max(w * 0.10f, 0.08f);
    return w > 0.0f && x >= -w - margin && x <= w + margin &&
        y >= -w - margin && y <= w + margin && z >= -margin && z <= w + margin;
}

bool FishRenderer::IsHabitatVisible(
    const DirectX::XMMATRIX& viewProjection,
    const Presentation& presentation) const
{
    using namespace DirectX;
    XMFLOAT3 minimum{};
    XMFLOAT3 maximum{};
    if (habitat_ == Habitat::UnderwaterArch)
    {
        minimum = {-0.4f, -6.0f, -7.8f};
        maximum = {48.4f, 3.7f, 7.8f};
    }
    else if (habitat_ == Habitat::ReceptionHeroTank)
    {
        minimum = {-8.5f, -1.9f, 7.9f};
        maximum = {8.5f, 8.0f, 16.1f};
    }
    else
    {
        minimum = {7.3f, -1.8f, -13.2f};
        maximum = {21.3f, 9.9f, 13.2f};
    }

    unsigned outsideLeft = 0;
    unsigned outsideRight = 0;
    unsigned outsideBottom = 0;
    unsigned outsideTop = 0;
    unsigned outsideNear = 0;
    unsigned outsideFar = 0;
    for (unsigned corner = 0; corner < 8; ++corner)
    {
        const XMFLOAT3 world=TransformPosition({
            (corner & 1u) != 0 ? maximum.x : minimum.x,
            (corner & 2u) != 0 ? maximum.y : minimum.y,
            (corner & 4u) != 0 ? maximum.z : minimum.z},presentation);
        const XMVECTOR clip = XMVector4Transform(
            XMVectorSet(world.x,world.y,world.z,1.0f),
            viewProjection);
        const float x = XMVectorGetX(clip);
        const float y = XMVectorGetY(clip);
        const float z = XMVectorGetZ(clip);
        const float w = XMVectorGetW(clip);
        outsideLeft += x < -w;
        outsideRight += x > w;
        outsideBottom += y < -w;
        outsideTop += y > w;
        outsideNear += z < 0.0f;
        outsideFar += z > w;
    }
    return outsideLeft != 8u && outsideRight != 8u &&
        outsideBottom != 8u && outsideTop != 8u &&
        outsideNear != 8u && outsideFar != 8u;
}

void FishRenderer::BuildRayInstances(
    const DirectX::XMMATRIX& viewProjection,
    const XMFLOAT3& cameraPosition,
    float totalTime,
    const Presentation& presentation)
{
    visibleRayInstances_.clear();
    // Rays belong to the hero tank composition. Keeping them out of the arch
    // also avoids misleading the player before the numeric clue is found.
    const std::uint32_t rayCount = habitat_ == Habitat::UnderwaterArch
        ? 0u : (habitat_ == Habitat::ReceptionHeroTank ? 2u : 3u);
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
        if (habitat_ == Habitat::ReceptionHeroTank)
        {
            const float route = time * (0.052f + index * 0.008f) + phase;
            return XMFLOAT3{
                std::cos(route) * (4.8f + index * 0.4f),
                2.55f + std::sin(route * 0.61f) * 1.05f,
                12.0f + std::sin(route) * (2.65f + index * 0.35f)};
        }
        const float route = time * (0.058f + index * 0.006f) + phase;
        return XMFLOAT3{
            14.4f + std::cos(route) * (4.6f + index * 0.35f),
            3.8f + std::sin(route * 0.67f + phase) * 1.35f,
            std::sin(route) * (7.2f + index * 0.55f)};
    };
    for (std::uint32_t index = 0; index < rayCount; ++index)
    {
        const XMFLOAT3 localPosition = rayPosition(index, totalTime);
        const XMFLOAT3 position=TransformPosition(localPosition,presentation);
        if (!IsVisible(position, viewProjection, cameraPosition))
        {
            continue;
        }
        const XMFLOAT3 nextPosition = TransformPosition(
            rayPosition(index, totalTime + 0.08f),presentation);
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
            {0.055f, 0.145f, 0.235f, 1.28f + index * 0.09f},
            {2.0f, 1.0f, 1.0f, 0.72f}
        });
    }
}

// =========================================================
// 中央ライトを横切った魚を、実座標から砂面へ投影
// =========================================================
void FishRenderer::BuildFloorShadowInstances(
    const DirectX::XMMATRIX& viewProjection,
    const XMFLOAT3& cameraPosition,
    const Presentation& presentation,
    const lighting::HeroTankLightingRig* heroTankLighting)
{
    visibleFloorShadowInstances_.clear();
    if (habitat_ != Habitat::ReceptionHeroTank || heroTankLighting == nullptr)
    {
        return;
    }

    constexpr float tankCenterZ = 12.0f;
    constexpr float waterSurfaceY = 7.95f;
    constexpr float sandSurfaceY = -2.19f;
    constexpr float tankHalfWidth = 8.25f;
    constexpr float tankMinZ = 8.2f;
    constexpr float tankMaxZ = 15.8f;

    const XMFLOAT3 localLightPosition{
        heroTankLighting->keyOffset.x,
        waterSurfaceY + heroTankLighting->keyOffset.y,
        tankCenterZ + heroTankLighting->keyOffset.z};
    const XMFLOAT3 lightPosition =
        TransformPosition(localLightPosition, presentation);
    const XMFLOAT3 lightDirection = Normalize(
        TransformDirection(
            heroTankLighting->keyDirection, presentation.yawRadians),
        {0.0f, -1.0f, 0.0f});
    const float coneCosine = std::cos(
        heroTankLighting->keyConeDegrees * 0.5f * kPi / 180.0f);
    const float lightToFloorHeight =
        std::max(lightPosition.y - sandSurfaceY, 0.1f);
    visibleFloorShadowInstances_.reserve(agents_.size() / 3u);

    for (const Agent& agent : agents_)
    {
        if (visibleFloorShadowInstances_.size() >= instanceCapacity_)
        {
            break;
        }

        const XMFLOAT3 fishPosition =
            TransformPosition(agent.position, presentation);
        const XMFLOAT3 lightToFish = Subtract(fishPosition, lightPosition);
        const float distanceToLight = Length(lightToFish);
        if (distanceToLight <= 0.01f || lightToFish.y >= -0.01f ||
            fishPosition.y <= sandSurfaceY)
        {
            continue;
        }

        const XMFLOAT3 lightRay = Scale(lightToFish, 1.0f / distanceToLight);
        const float coneAlignment =
            lightRay.x * lightDirection.x +
            lightRay.y * lightDirection.y +
            lightRay.z * lightDirection.z;
        if (coneAlignment < coneCosine)
        {
            continue;
        }

        const float projectionDistance =
            (sandSurfaceY - lightPosition.y) / lightToFish.y;
        const XMFLOAT3 shadowPosition = Add(
            lightPosition, Scale(lightToFish, projectionDistance));
        const XMFLOAT3 localShadowPosition =
            InverseTransformPosition(shadowPosition, presentation);
        if (std::abs(localShadowPosition.x) > tankHalfWidth ||
            localShadowPosition.z < tankMinZ ||
            localShadowPosition.z > tankMaxZ ||
            !IsVisible(shadowPosition, viewProjection, cameraPosition))
        {
            continue;
        }

        const float heightRatio = std::clamp(
            (fishPosition.y - sandSurfaceY) / lightToFloorHeight,
            0.0f, 1.0f);
        const float coneEdge = std::clamp(
            (coneAlignment - coneCosine) /
                std::max(1.0f - coneCosine, 0.001f),
            0.0f, 1.0f);
        const float shadowOpacity =
            (0.30f - heightRatio * 0.20f) *
            SmoothStep(0.0f, 0.28f, coneEdge);
        if (shadowOpacity <= 0.012f)
        {
            continue;
        }

        XMFLOAT3 forward = TransformDirection(
            {agent.velocity.x, 0.0f, agent.velocity.z},
            presentation.yawRadians);
        forward = Normalize(forward, {1.0f, 0.0f, 0.0f});
        const bool mediumSpecies = agent.species == 1u;
        const float projectedScale = agent.scale *
            std::clamp(projectionDistance, 1.0f, 2.35f);
        visibleFloorShadowInstances_.push_back({
            {shadowPosition.x, shadowPosition.y, shadowPosition.z, projectedScale},
            {forward.x, forward.y, forward.z, agent.phase},
            {shadowOpacity, 0.0f, 0.0f, 0.0f},
            {static_cast<float>(agent.species),
             mediumSpecies ? 1.24f : 1.0f,
             mediumSpecies ? 1.12f : 1.0f,
             -1.0f}
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
    Habitat habitat,
    const lighting::HeroTankLightingRig* heroTankLighting,
    Presentation presentation)
{
    if (habitat != habitat_)
    {
        ResetHabitat(habitat);
    }
    if (habitat == Habitat::None || agents_.empty())
    {
        return;
    }

    fleeToEntrance_=std::clamp(presentation.fleeToEntrance,0.f,1.f);
    const bool habitatVisible = IsHabitatVisible(viewProjection,presentation);
    simulationAccumulator_ += std::clamp(deltaTime, 0.0f, 0.05f);
    const float simulationStep = habitatVisible ? 1.0f / 30.0f : 1.0f / 8.0f;
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
    if (!habitatVisible)
    {
        return;
    }

    visibleInstances_.clear();
    visibleInstances_.reserve(agents_.size());
    visibleLowDetailInstances_.clear();
    visibleLowDetailInstances_.reserve(agents_.size());
    visibleFloorShadowInstances_.clear();
    for (const Agent& agent : agents_)
    {
        const XMFLOAT3 worldPosition=TransformPosition(agent.position,presentation);
        if (!IsVisible(worldPosition, viewProjection, cameraPosition))
        {
            continue;
        }
        const XMFLOAT3 forward = Normalize(
            TransformDirection(agent.velocity,presentation.yawRadians), {1.0f, 0.0f, 0.0f});
        const bool mediumSpecies = agent.species == 1u;
        const float silver = mediumSpecies
            ? 0.39f + agent.tint * 0.10f
            : 0.58f + agent.tint * 0.14f;
        const bool overheadSilhouette =
            habitat_ == Habitat::UnderwaterArch && agent.school == 2u;
        const bool heroTankHabitat =
            habitat_ == Habitat::WatatsumiTank ||
            habitat_ == Habitat::ReceptionHeroTank;
        const bool boninYellowSchool =
            heroTankHabitat &&
            agent.species == 0u && agent.school == 1u;
        const bool boninDeepSchool =
            heroTankHabitat &&
            agent.species == 0u && agent.school == 2u;
        const XMFLOAT3 bodyTint = boninYellowSchool
            ? XMFLOAT3{
                0.36f + agent.tint * 0.10f,
                0.40f + agent.tint * 0.10f,
                0.16f + agent.tint * 0.05f}
            : (boninDeepSchool
                ? XMFLOAT3{
                    0.12f + agent.tint * 0.06f,
                    0.34f + agent.tint * 0.10f,
                    0.48f + agent.tint * 0.12f}
                : XMFLOAT3{
                    mediumSpecies
                        ? 0.20f + agent.tint * 0.08f
                        : 0.18f + agent.tint * 0.10f,
                    mediumSpecies
                        ? 0.34f + agent.tint * 0.10f
                        : 0.42f + agent.tint * 0.13f,
                    silver});
        const float cameraDistanceSquared = LengthSquared(
            Subtract(worldPosition, cameraPosition));
        // Dense schoolers occupy only a few pixels beyond eight metres.
        // Keep the animated silhouette while avoiding the full body mesh.
        const float detailDistance = heroTankHabitat ? 8.0f : 22.0f;
        const bool useLowDetail =
            agent.species == 0u && cameraDistanceSquared > detailDistance * detailDistance;
        auto& destination = useLowDetail
            ? visibleLowDetailInstances_
            : visibleInstances_;
        destination.push_back({
            {worldPosition.x, worldPosition.y, worldPosition.z, agent.scale},
            {forward.x, forward.y, forward.z, agent.phase},
            {bodyTint.x,
             bodyTint.y,
             bodyTint.z,
             (overheadSilhouette ? -1.0f : 1.0f) *
                 (mediumSpecies ? 2.15f : 2.85f + agent.tint * .75f)},
            {static_cast<float>(agent.species),
             mediumSpecies ? 1.24f : 1.0f,
             mediumSpecies ? 1.12f : 1.0f,
             mediumSpecies ? 0.58f : 0.36f}
        });
    }
    BuildFloorShadowInstances(
        viewProjection, cameraPosition, presentation, heroTankLighting);
    BuildRayInstances(viewProjection, cameraPosition, totalTime,presentation);
    if (visibleInstances_.empty() &&
        visibleLowDetailInstances_.empty() &&
        visibleFloorShadowInstances_.empty() &&
        visibleRayInstances_.empty())
    {
        return;
    }

    UploadInstances(context, instanceBuffer_.Get(), visibleInstances_);
    UploadInstances(
        context, lowDetailInstanceBuffer_.Get(), visibleLowDetailInstances_);
    UploadInstances(
        context, floorShadowInstanceBuffer_.Get(), visibleFloorShadowInstances_);
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
        : (habitat == Habitat::ReceptionHeroTank
            ? DirectX::XMFLOAT4{5.75f, 0.066f, 0.030f, 0.016f}
            : DirectX::XMFLOAT4{10.20f, 0.060f, 0.027f, 0.014f});
    if ((habitat == Habitat::WatatsumiTank ||
         habitat == Habitat::ReceptionHeroTank) &&
        heroTankLighting != nullptr)
    {
        const DirectX::XMFLOAT3 selectedColor =
            heroTankLighting->alternateEnabled
                ? heroTankLighting->alternateColor
                : heroTankLighting->defaultColor;
        constants->keyLightDirectionIntensity = {
            0.0f,
            1.0f,
            0.0f,
            heroTankLighting->overheadKeyIntensity *
                heroTankLighting->intensity};
        constants->keyLightColor = {
            heroTankLighting->overheadKeyColor.x,
            heroTankLighting->overheadKeyColor.y,
            heroTankLighting->overheadKeyColor.z,
            1.0f};
        if(habitat==Habitat::ReceptionHeroTank) {
            constants->keyLightColor={selectedColor.x,selectedColor.y,selectedColor.z,1.f};
            const auto& direction=heroTankLighting->keyDirection;
            constants->keyLightDirectionIntensity.x=-direction.x;
            constants->keyLightDirectionIntensity.y=-direction.y;
            constants->keyLightDirectionIntensity.z=-direction.z;
        }
        constants->sideLightColorIntensity = {
            selectedColor.x,
            selectedColor.y,
            selectedColor.z,
            heroTankLighting->sideLightIntensity *
                heroTankLighting->intensity};
    }
    else
    {
        constants->keyLightDirectionIntensity = {
            -0.249f, 0.955f, -0.187f, 1.0f};
        constants->keyLightColor = {1.0f, 1.0f, 1.0f, 1.0f};
        constants->sideLightColorIntensity = {};
    }
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

    if (!visibleFloorShadowInstances_.empty())
    {
        const float blendFactor[4] = {};
        context->OMSetBlendState(
            shadowBlendState_.Get(), blendFactor, 0xffffffffu);
        context->OMSetDepthStencilState(shadowDepthState_.Get(), 0);
        drawInstances(
            lowDetailVertexBuffer_.Get(), lowDetailIndexBuffer_.Get(),
            floorShadowInstanceBuffer_.Get(), lowDetailIndexCount_,
            static_cast<UINT>(visibleFloorShadowInstances_.size()));
        context->OMSetBlendState(nullptr, blendFactor, 0xffffffffu);
        context->OMSetDepthStencilState(depthState_.Get(), 0);
    }

    drawInstances(
        vertexBuffer_.Get(), indexBuffer_.Get(), instanceBuffer_.Get(),
        indexCount_, static_cast<UINT>(visibleInstances_.size()));
    drawInstances(
        lowDetailVertexBuffer_.Get(), lowDetailIndexBuffer_.Get(),
        lowDetailInstanceBuffer_.Get(), lowDetailIndexCount_,
        static_cast<UINT>(visibleLowDetailInstances_.size()));
    drawInstances(
        rayVertexBuffer_.Get(), rayIndexBuffer_.Get(), rayInstanceBuffer_.Get(),
        rayIndexCount_, static_cast<UINT>(visibleRayInstances_.size()));

    context->OMSetDepthStencilState(nullptr, 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffffu);
    context->RSSetState(nullptr);
}
