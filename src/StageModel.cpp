/*==================================================================================================

   [StageModel.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   cgltfを利用したGLB解析、頂点展開、GPU Buffer生成、ステージ描画
===================================================================================================*/
#define _CRT_SECURE_NO_WARNINGS

#include "StageModel.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <bit>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#define CGLTF_IMPLEMENTATION
#include "../third_party/cgltf/cgltf.h"

namespace
{
constexpr float kStageFloorOffset = -2.25f;

void ThrowIfFailed(HRESULT hr, const char* operation)
{
    if (FAILED(hr))
    {
        throw std::runtime_error(std::string(operation) + " failed.");
    }
}

Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
    const std::filesystem::path& path,
    const char* entryPoint,
    const char* profile)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    // Runtime shader iteration still keeps debug information, but the local
    // light loop must be optimized. SKIP_OPTIMIZATION roughly doubled the
    // Watatsumi GPU cost after spot lighting was introduced.
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompileFromFile(
        path.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint,
        profile,
        flags,
        0,
        bytecode.GetAddressOf(),
        errors.GetAddressOf());

    if (FAILED(hr))
    {
        std::string message = "Stage shader compilation failed: ";
        message += path.string();
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

const cgltf_accessor* FindAttribute(
    const cgltf_primitive& primitive,
    cgltf_attribute_type type,
    cgltf_int index = 0)
{
    for (cgltf_size attributeIndex = 0;
         attributeIndex < primitive.attributes_count;
         ++attributeIndex)
    {
        const cgltf_attribute& attribute =
            primitive.attributes[attributeIndex];
        if (attribute.type == type && attribute.index == index)
        {
            return attribute.data;
        }
    }
    return nullptr;
}

DirectX::XMMATRIX LoadNodeWorldMatrix(const cgltf_node& node)
{
    float matrix[16]{};
    cgltf_node_transform_world(&node, matrix);

    // cgltf exposes glTF's column-major matrix. Listing its values as rows
    // creates the transposed representation expected by DirectXMath's
    // row-vector transform convention.
    return DirectX::XMMATRIX(
        matrix[0], matrix[1], matrix[2], matrix[3],
        matrix[4], matrix[5], matrix[6], matrix[7],
        matrix[8], matrix[9], matrix[10], matrix[11],
        matrix[12], matrix[13], matrix[14], matrix[15]);
}
}

void StageModel::Initialize(
    ID3D11Device* device,
    const std::filesystem::path& modelPath,
    const std::filesystem::path& shaderPath,
    const ImportOptions& options)
{
    // GLBのメッシュを読み込み、ステージ用Vertex/Index Bufferへ変換する。
    if (!std::filesystem::exists(modelPath))
    {
        throw std::runtime_error(
            "Stage model was not found: " + modelPath.string());
    }

    cgltf_options cgltfOptions{};
    cgltf_data* rawData = nullptr;
    const std::string modelPathUtf8 = modelPath.string();
    cgltf_result result =
        cgltf_parse_file(&cgltfOptions, modelPathUtf8.c_str(), &rawData);
    if (result != cgltf_result_success)
    {
        throw std::runtime_error("cgltf_parse_file failed.");
    }

    struct DataGuard
    {
        cgltf_data* data;
        ~DataGuard()
        {
            cgltf_free(data);
        }
    } guard{rawData};

    result = cgltf_load_buffers(
        &cgltfOptions,
        rawData,
        modelPathUtf8.c_str());
    if (result != cgltf_result_success)
    {
        throw std::runtime_error("cgltf_load_buffers failed.");
    }
    result = cgltf_validate(rawData);
    if (result != cgltf_result_success)
    {
        throw std::runtime_error("The stage GLB failed cgltf validation.");
    }

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    drawBatches_.clear();
    meshCount_ = 0;
    std::unordered_set<std::uint64_t> geometrySignatures;

    using namespace DirectX;
    for (cgltf_size nodeIndex = 0;
         nodeIndex < rawData->nodes_count;
         ++nodeIndex)
    {
        const cgltf_node& node = rawData->nodes[nodeIndex];
        if (!node.mesh)
        {
            continue;
        }

        const XMMATRIX world = LoadNodeWorldMatrix(node);
        const XMMATRIX normalMatrix =
            XMMatrixTranspose(XMMatrixInverse(nullptr, world));

        for (cgltf_size primitiveIndex = 0;
             primitiveIndex < node.mesh->primitives_count;
             ++primitiveIndex)
        {
            const cgltf_primitive& primitive =
                node.mesh->primitives[primitiveIndex];
            if (primitive.type != cgltf_primitive_type_triangles)
            {
                continue;
            }
            if (options.hideAuthoringSurfaces &&
                primitive.material &&
                primitive.material->name &&
                (std::strcmp(primitive.material->name, "Glass") == 0 ||
                 std::strcmp(primitive.material->name, "Route") == 0))
            {
                // The aquarium pass supplies the live water/glass image.
                // Route is only a Blender greybox guide.
                continue;
            }

            const cgltf_accessor* positions =
                FindAttribute(primitive, cgltf_attribute_type_position);
            const cgltf_accessor* normals =
                FindAttribute(primitive, cgltf_attribute_type_normal);
            const cgltf_accessor* texcoords =
                FindAttribute(primitive, cgltf_attribute_type_texcoord);
            if (!positions)
            {
                continue;
            }

            const std::uint32_t baseVertex =
                static_cast<std::uint32_t>(vertices.size());
            vertices.reserve(vertices.size() + positions->count);

            for (cgltf_size vertexIndex = 0;
                 vertexIndex < positions->count;
                 ++vertexIndex)
            {
                float positionValues[3]{};
                float normalValues[3]{0.0f, 1.0f, 0.0f};
                float uvValues[2]{};
                cgltf_accessor_read_float(
                    positions,
                    vertexIndex,
                    positionValues,
                    3);
                if (normals)
                {
                    cgltf_accessor_read_float(
                        normals,
                        vertexIndex,
                        normalValues,
                        3);
                }
                if (texcoords)
                {
                    cgltf_accessor_read_float(
                        texcoords,
                        vertexIndex,
                        uvValues,
                        2);
                }

                XMVECTOR position = XMVector3TransformCoord(
                    XMLoadFloat3(
                        reinterpret_cast<const XMFLOAT3*>(
                            positionValues)),
                    world);
                XMVECTOR normal = XMVector3Normalize(
                    XMVector3TransformNormal(
                        XMLoadFloat3(
                            reinterpret_cast<const XMFLOAT3*>(
                                normalValues)),
                        normalMatrix));

                XMFLOAT3 bakedPosition;
                XMFLOAT3 bakedNormal;
                XMStoreFloat3(&bakedPosition, position);
                XMStoreFloat3(&bakedNormal, normal);

                // Convert glTF's right-handed Y-up space to the renderer's
                // left-handed Y-up space. Align Blender's floor at the
                // prototype's existing -2.25 m floor height.
                bakedPosition.y += kStageFloorOffset;
                bakedPosition.z = -bakedPosition.z;
                bakedNormal.z = -bakedNormal.z;

                const XMMATRIX importTransform =
                    XMMatrixRotationY(options.yawRadians) *
                    XMMatrixTranslation(
                        options.translation.x,
                        options.translation.y,
                        options.translation.z);
                const XMVECTOR importedPosition =
                    XMVector3TransformCoord(
                        XMLoadFloat3(&bakedPosition),
                        importTransform);
                const XMVECTOR importedNormal =
                    XMVector3Normalize(
                        XMVector3TransformNormal(
                            XMLoadFloat3(&bakedNormal),
                            XMMatrixRotationY(options.yawRadians)));
                XMStoreFloat3(&bakedPosition, importedPosition);
                XMStoreFloat3(&bakedNormal, importedNormal);

                vertices.push_back({
                    bakedPosition,
                    bakedNormal,
                    XMFLOAT2(uvValues[0], uvValues[1])
                });
            }

            std::vector<std::uint32_t> primitiveIndices;
            if (primitive.indices)
            {
                primitiveIndices.reserve(primitive.indices->count);
                for (cgltf_size index = 0;
                     index < primitive.indices->count;
                     ++index)
                {
                    primitiveIndices.push_back(
                        baseVertex +
                        static_cast<std::uint32_t>(
                            cgltf_accessor_read_index(
                                primitive.indices,
                                index)));
                }
            }
            else
            {
                primitiveIndices.reserve(positions->count);
                for (cgltf_size index = 0;
                     index < positions->count;
                     ++index)
                {
                    primitiveIndices.push_back(
                        baseVertex +
                        static_cast<std::uint32_t>(index));
                }
            }

            // Blender exports can contain several differently named nodes
            // with byte-identical transformed geometry. Rendering all of them
            // causes z-fighting and makes a single wall look doubled. Hash the
            // complete baked primitive and remove exact duplicates only;
            // nearby, coplanar, or differently materialed pieces remain.
            constexpr std::uint64_t fnvOffset = 14695981039346656037ull;
            constexpr std::uint64_t fnvPrime = 1099511628211ull;
            std::uint64_t geometrySignature = fnvOffset;
            const auto hashValue = [&geometrySignature](std::uint32_t value)
            {
                geometrySignature ^= value;
                geometrySignature *= fnvPrime;
            };
            for (std::size_t vertexIndex = baseVertex;
                 vertexIndex < vertices.size(); ++vertexIndex)
            {
                const Vertex& vertex = vertices[vertexIndex];
                for (const float value : {
                    vertex.position.x, vertex.position.y, vertex.position.z,
                    vertex.normal.x, vertex.normal.y, vertex.normal.z,
                    vertex.uv.x, vertex.uv.y})
                {
                    hashValue(std::bit_cast<std::uint32_t>(value));
                }
            }
            for (const std::uint32_t index : primitiveIndices)
            {
                hashValue(index - baseVertex);
            }
            if (primitive.material)
            {
                hashValue(static_cast<std::uint32_t>(
                    primitive.material - rawData->materials));
            }
            if (!geometrySignatures.insert(geometrySignature).second)
            {
                vertices.resize(baseVertex);
                continue;
            }

            // Flipping Z changes handedness, so reverse triangle winding.
            const std::uint32_t batchIndexStart =
                static_cast<std::uint32_t>(indices.size());
            for (std::size_t index = 0;
                 index + 2 < primitiveIndices.size();
                 index += 3)
            {
                indices.push_back(primitiveIndices[index]);
                indices.push_back(primitiveIndices[index + 2]);
                indices.push_back(primitiveIndices[index + 1]);
            }

            DirectX::XMFLOAT4 primitiveBaseColor{
                0.8f, 0.8f, 0.8f, 1.0f
            };
            float primitiveSurfaceType = 0.0f;
            bool primitiveTransparent = false;
            if (primitive.material &&
                primitive.material->has_pbr_metallic_roughness)
            {
                const cgltf_float* factor =
                    primitive.material
                        ->pbr_metallic_roughness
                        .base_color_factor;
                primitiveBaseColor = {
                    factor[0],
                    factor[1],
                    factor[2],
                    factor[3]
                };
            }
            if (primitive.material && primitive.material->name)
            {
                const char* materialName = primitive.material->name;
                if (std::strcmp(materialName, "TankWaterLarge") == 0)
                {
                    primitiveSurfaceType = 1.0f;
                }
                else if (std::strcmp(
                    materialName,
                    "TankWaterJellyCylinder") == 0)
                {
                    primitiveSurfaceType = 2.0f;
                }
                else if (std::strcmp(
                    materialName,
                    "TankWaterDisplayBox") == 0)
                {
                    primitiveSurfaceType = 3.0f;
                }
                else if (std::strcmp(materialName, "EmissiveCyan") == 0)
                {
                    primitiveSurfaceType = 4.0f;
                }
                else if (std::strcmp(materialName, "EmissiveWarm") == 0)
                {
                    primitiveSurfaceType = 5.0f;
                }
                else if (std::strcmp(
                    materialName,
                    "EmissiveJellyBlue") == 0)
                {
                    primitiveSurfaceType = 6.0f;
                }
                else if (std::strcmp(
                    materialName,
                    "TankGlassJellyCylinder") == 0)
                {
                    primitiveSurfaceType = 7.0f;
                }
                else if (std::strcmp(
                    materialName,
                    "TankWaterArch") == 0)
                {
                    primitiveSurfaceType = 8.0f;
                }
                else if (std::strcmp(
                    materialName,
                    "TankGlassArch") == 0)
                {
                    primitiveSurfaceType = 9.0f;
                }
                else if (std::strcmp(
                    materialName,
                    "ArchWaterSurface") == 0)
                {
                    primitiveSurfaceType = 10.0f;
                }
                else if (std::strcmp(materialName, "ArchFloor") == 0)
                {
                    primitiveSurfaceType = 11.0f;
                }
                else if (std::strcmp(materialName, "ArchRock") == 0)
                {
                    primitiveSurfaceType = 12.0f;
                }
                else if (std::strcmp(materialName, "ArchSeam") == 0)
                {
                    primitiveSurfaceType = 13.0f;
                }
                else if (std::strcmp(materialName, "ArchRail") == 0)
                {
                    primitiveSurfaceType = 14.0f;
                }
                else if (std::strcmp(materialName, "ArchTrim") == 0)
                {
                    primitiveSurfaceType = 15.0f;
                }
                else if (std::strcmp(materialName, "WatatsumiWater") == 0)
                {
                    primitiveSurfaceType = 16.0f;
                }
                else if (std::strcmp(materialName, "WatatsumiGlass") == 0)
                {
                    primitiveSurfaceType = 17.0f;
                }
                else if (std::strcmp(materialName, "WatatsumiArchitecture") == 0)
                {
                    primitiveSurfaceType = 18.0f;
                }
                else if (std::strcmp(materialName, "WatatsumiRamp") == 0)
                {
                    primitiveSurfaceType = 19.0f;
                }
                else if (std::strcmp(materialName, "WatatsumiRock") == 0)
                {
                    primitiveSurfaceType = 20.0f;
                }
                else if (std::strcmp(materialName, "WatatsumiEmitter") == 0)
                {
                    primitiveSurfaceType = 21.0f;
                }
                else if (std::strcmp(materialName, "WatatsumiWaterSurface") == 0)
                {
                    primitiveSurfaceType = 22.0f;
                }
                primitiveTransparent =
                    primitive.material->alpha_mode ==
                    cgltf_alpha_mode_blend;
            }

            DirectX::XMFLOAT3 boundsMinimum{
                vertices[baseVertex].position};
            DirectX::XMFLOAT3 boundsMaximum{
                vertices[baseVertex].position};
            for (std::size_t vertexIndex = baseVertex + 1;
                 vertexIndex < vertices.size(); ++vertexIndex)
            {
                const auto& position = vertices[vertexIndex].position;
                boundsMinimum.x = std::min(boundsMinimum.x, position.x);
                boundsMinimum.y = std::min(boundsMinimum.y, position.y);
                boundsMinimum.z = std::min(boundsMinimum.z, position.z);
                boundsMaximum.x = std::max(boundsMaximum.x, position.x);
                boundsMaximum.y = std::max(boundsMaximum.y, position.y);
                boundsMaximum.z = std::max(boundsMaximum.z, position.z);
            }
            drawBatches_.push_back({
                batchIndexStart,
                static_cast<std::uint32_t>(
                    indices.size() - batchIndexStart),
                primitiveBaseColor,
                boundsMinimum,
                boundsMaximum,
                primitiveSurfaceType,
                primitiveTransparent
            });
            ++meshCount_;
        }
    }

    if (vertices.empty() || indices.empty())
    {
        throw std::runtime_error(
            "The stage GLB contains no triangle primitives.");
    }

    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth =
        static_cast<UINT>(vertices.size() * sizeof(Vertex));
    vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    const D3D11_SUBRESOURCE_DATA vertexData{vertices.data()};
    ThrowIfFailed(
        device->CreateBuffer(
            &vertexBufferDesc,
            &vertexData,
            vertexBuffer_.GetAddressOf()),
        "CreateBuffer (stage vertices)");

    D3D11_BUFFER_DESC indexBufferDesc{};
    indexBufferDesc.ByteWidth =
        static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
    indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    const D3D11_SUBRESOURCE_DATA indexData{indices.data()};
    ThrowIfFailed(
        device->CreateBuffer(
            &indexBufferDesc,
            &indexData,
            indexBuffer_.GetAddressOf()),
        "CreateBuffer (stage indices)");
    indexCount_ = static_cast<std::uint32_t>(indices.size());

    const auto vertexBytecode =
        CompileShader(shaderPath, "VSStage", "vs_5_0");
    const auto pixelBytecode =
        CompileShader(shaderPath, "PSStage", "ps_5_0");
    ThrowIfFailed(
        device->CreateVertexShader(
            vertexBytecode->GetBufferPointer(),
            vertexBytecode->GetBufferSize(),
            nullptr,
            vertexShader_.GetAddressOf()),
        "CreateVertexShader (stage)");
    ThrowIfFailed(
        device->CreatePixelShader(
            pixelBytecode->GetBufferPointer(),
            pixelBytecode->GetBufferSize(),
            nullptr,
            pixelShader_.GetAddressOf()),
        "CreatePixelShader (stage)");

    const D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            static_cast<UINT>(offsetof(Vertex, position)),
            D3D11_INPUT_PER_VERTEX_DATA, 0
        },
        {
            "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            static_cast<UINT>(offsetof(Vertex, normal)),
            D3D11_INPUT_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            static_cast<UINT>(offsetof(Vertex, uv)),
            D3D11_INPUT_PER_VERTEX_DATA, 0
        }
    };
    ThrowIfFailed(
        device->CreateInputLayout(
            inputElements,
            static_cast<UINT>(std::size(inputElements)),
            vertexBytecode->GetBufferPointer(),
            vertexBytecode->GetBufferSize(),
            inputLayout_.GetAddressOf()),
        "CreateInputLayout (stage)");

    D3D11_BUFFER_DESC constantBufferDesc{};
    constantBufferDesc.ByteWidth = sizeof(Constants);
    constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ThrowIfFailed(
        device->CreateBuffer(
            &constantBufferDesc,
            nullptr,
            constantBuffer_.GetAddressOf()),
        "CreateBuffer (stage constants)");

    constantBufferDesc.ByteWidth = sizeof(LocalLightingConstants);
    ThrowIfFailed(
        device->CreateBuffer(
            &constantBufferDesc,
            nullptr,
            localLightingBuffer_.GetAddressOf()),
        "CreateBuffer (local lighting constants)");

    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    ThrowIfFailed(
        device->CreateRasterizerState(
            &rasterizerDesc,
            rasterizerState_.GetAddressOf()),
        "CreateRasterizerState (stage)");

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.IndependentBlendEnable = TRUE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    // Transparent water and glass do not own linear depth or motion. Keeping
    // RT1/RT2 untouched prevents overlapping cylinders from poisoning the
    // temporal volume history when viewed from the opposite side.
    blendDesc.RenderTarget[1].RenderTargetWriteMask = 0;
    blendDesc.RenderTarget[2].RenderTargetWriteMask = 0;
    ThrowIfFailed(
        device->CreateBlendState(
            &blendDesc,
            transparentBlendState_.GetAddressOf()),
        "CreateBlendState (stage transparent)");

    D3D11_DEPTH_STENCIL_DESC transparentDepthDesc{};
    transparentDepthDesc.DepthEnable = TRUE;
    transparentDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    transparentDepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ThrowIfFailed(
        device->CreateDepthStencilState(
            &transparentDepthDesc,
            transparentDepthState_.GetAddressOf()),
        "CreateDepthStencilState (stage transparent)");

    D3D11_SAMPLER_DESC refractionSamplerDesc{};
    refractionSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    refractionSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    refractionSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    refractionSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    refractionSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ThrowIfFailed(
        device->CreateSamplerState(
            &refractionSamplerDesc,
            refractionSampler_.GetAddressOf()),
        "CreateSamplerState (stage refraction)");
}

void StageModel::Render(
    ID3D11DeviceContext* context,
    const DirectX::XMMATRIX& currentViewProjection,
    const DirectX::XMMATRIX& previousViewProjection,
    const DirectX::XMFLOAT3& cameraPosition,
    float time,
    float aquariumOpeningMask)
{
    RenderPass(
        context, currentViewProjection, previousViewProjection,
        cameraPosition, time, aquariumOpeningMask, false, nullptr,
        TransparentLayer::All);
    RenderPass(
        context, currentViewProjection, previousViewProjection,
        cameraPosition, time, aquariumOpeningMask, true, nullptr,
        TransparentLayer::All);
}

void StageModel::RenderOpaque(
    ID3D11DeviceContext* context,
    const DirectX::XMMATRIX& currentViewProjection,
    const DirectX::XMMATRIX& previousViewProjection,
    const DirectX::XMFLOAT3& cameraPosition,
    float time,
    float aquariumOpeningMask,
    const lighting::LocalLightingRig* localLighting)
{
    RenderPass(
        context, currentViewProjection, previousViewProjection,
        cameraPosition, time, aquariumOpeningMask, false, nullptr,
        TransparentLayer::All, localLighting);
}

void StageModel::RenderTransparent(
    ID3D11DeviceContext* context,
    const DirectX::XMMATRIX& currentViewProjection,
    const DirectX::XMMATRIX& previousViewProjection,
    const DirectX::XMFLOAT3& cameraPosition,
    float time,
    float aquariumOpeningMask,
    ID3D11ShaderResourceView* refractionSceneView,
    TransparentLayer layer,
    const lighting::LocalLightingRig* localLighting)
{
    RenderPass(
        context, currentViewProjection, previousViewProjection,
        cameraPosition, time, aquariumOpeningMask, true,
        refractionSceneView, layer, localLighting);
}

void StageModel::RenderPass(
    ID3D11DeviceContext* context,
    const DirectX::XMMATRIX& currentViewProjection,
    const DirectX::XMMATRIX& previousViewProjection,
    const DirectX::XMFLOAT3& cameraPosition,
    float time,
    float aquariumOpeningMask,
    bool transparentPass,
    ID3D11ShaderResourceView* refractionSceneView,
    TransparentLayer layer,
    const lighting::LocalLightingRig* localLighting)
{
    // Current/Previous ViewProjectionを渡し、Temporal用Motion Vectorも出力する。
    if (!IsLoaded())
    {
        return;
    }

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    ID3D11Buffer* vertexBuffer = vertexBuffer_.Get();
    ID3D11Buffer* constantBuffer = constantBuffer_.Get();
    context->IASetInputLayout(inputLayout_.Get());
    context->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(
        0,
        1,
        &vertexBuffer,
        &stride,
        &offset);
    context->IASetIndexBuffer(
        indexBuffer_.Get(),
        DXGI_FORMAT_R32_UINT,
        0);
    context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context->VSSetConstantBuffers(2, 1, &constantBuffer);
    context->PSSetShader(pixelShader_.Get(), nullptr, 0);
    context->PSSetConstantBuffers(2, 1, &constantBuffer);
    // Transparent water/glass use dedicated lighting branches and never call
    // EvaluateLocalLighting. Avoid rebuilding and mapping b3 for those passes.
    if (!transparentPass)
    {
        LocalLightingConstants localConstants{};
        if (localLighting != nullptr && localLighting->enabled)
        {
            const std::uint32_t authoredCount = std::min(
                localLighting->lightCount,
                lighting::kMaximumLocalLights);
            std::uint32_t activeCount = 0;
            for (std::uint32_t index = 0; index < authoredCount; ++index)
            {
                const auto& light = localLighting->lights[index];
                if (!light.enabled || light.intensity <= 0.0f ||
                    light.range <= 0.01f)
                {
                    continue;
                }
                const DirectX::XMVECTOR direction = DirectX::XMVector3Normalize(
                    DirectX::XMLoadFloat3(&light.direction));
                DirectX::XMFLOAT3 normalizedDirection{};
                DirectX::XMStoreFloat3(&normalizedDirection, direction);
                localConstants.positionRange[activeCount] = {
                    light.position.x, light.position.y, light.position.z,
                    std::max(light.range, 0.01f)};
                localConstants.directionType[activeCount] = {
                    normalizedDirection.x, normalizedDirection.y,
                    normalizedDirection.z, static_cast<float>(light.type)};
                localConstants.colorIntensity[activeCount] = {
                    light.color.x, light.color.y, light.color.z,
                    std::max(light.intensity, 0.0f)};
                localConstants.coneEnabled[activeCount] = {
                    std::cos(DirectX::XMConvertToRadians(light.innerConeDegrees)),
                    std::cos(DirectX::XMConvertToRadians(light.outerConeDegrees)),
                    1.0f, 0.0f};
                ++activeCount;
            }
            localConstants.lightControl = {
                static_cast<float>(activeCount), 1.0f, 0.0f, 0.0f};
            localConstants.ambientColorStrength = {
                localLighting->ambientColor.x, localLighting->ambientColor.y,
                localLighting->ambientColor.z, localLighting->ambientStrength};
            localConstants.tankBounceCenterRange = {
                localLighting->tankBounceCenter.x,
                localLighting->tankBounceCenter.y,
                localLighting->tankBounceCenter.z,
                localLighting->tankBounceRange};
            localConstants.tankBounceNormalHalfWidth = {
                localLighting->tankBounceNormal.x,
                localLighting->tankBounceNormal.y,
                localLighting->tankBounceNormal.z,
                localLighting->tankBounceHalfWidth};
            localConstants.tankBounceColorIntensity = {
                localLighting->tankBounceColor.x,
                localLighting->tankBounceColor.y,
                localLighting->tankBounceColor.z,
                localLighting->tankBounceIntensity};
            localConstants.atmosphereColorDensity = {
                localLighting->atmosphereColor.x,
                localLighting->atmosphereColor.y,
                localLighting->atmosphereColor.z,
                localLighting->atmosphereEnabled
                    ? localLighting->atmosphereDensity : 0.0f};
            localConstants.hybridControl = {
                localLighting->tankBounceHalfHeight,
                localLighting->tankBounceEnabled ? 1.0f : 0.0f,
                localLighting->atmosphereStart,
                localLighting->atmosphereMaximum};
        }
        D3D11_MAPPED_SUBRESOURCE lightMapped{};
        ThrowIfFailed(context->Map(localLightingBuffer_.Get(), 0,
            D3D11_MAP_WRITE_DISCARD, 0, &lightMapped),
            "Map (local lighting constants)");
        *static_cast<LocalLightingConstants*>(lightMapped.pData) = localConstants;
        context->Unmap(localLightingBuffer_.Get(), 0);
        ID3D11Buffer* localLightingBuffer = localLightingBuffer_.Get();
        context->PSSetConstantBuffers(3, 1, &localLightingBuffer);
    }
    context->RSSetState(rasterizerState_.Get());

    const auto isVisible = [&currentViewProjection](const DrawBatch& batch)
    {
        using namespace DirectX;
        const XMFLOAT3& minimum = batch.boundsMinimum;
        const XMFLOAT3& maximum = batch.boundsMaximum;
        unsigned outsideLeft = 0;
        unsigned outsideRight = 0;
        unsigned outsideBottom = 0;
        unsigned outsideTop = 0;
        unsigned outsideNear = 0;
        unsigned outsideFar = 0;
        for (unsigned corner = 0; corner < 8; ++corner)
        {
            const XMVECTOR position = XMVectorSet(
                ((corner & 1u) != 0 ? maximum.x : minimum.x) +
                    ((corner & 1u) != 0 ? 0.35f : -0.35f),
                ((corner & 2u) != 0 ? maximum.y : minimum.y) +
                    ((corner & 2u) != 0 ? 0.35f : -0.35f),
                ((corner & 4u) != 0 ? maximum.z : minimum.z) +
                    ((corner & 4u) != 0 ? 0.35f : -0.35f),
                1.0f);
            const XMVECTOR clip = XMVector4Transform(
                position, currentViewProjection);
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
    };

    auto drawBatch = [&](const DrawBatch& batch)
    {
        if (!isVisible(batch))
        {
            return;
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        ThrowIfFailed(
            context->Map(
                constantBuffer_.Get(),
                0,
                D3D11_MAP_WRITE_DISCARD,
                0,
                &mapped),
            "Map (stage constants)");
        auto* constants = static_cast<Constants*>(mapped.pData);
        DirectX::XMStoreFloat4x4(
            &constants->currentViewProjection,
            currentViewProjection);
        DirectX::XMStoreFloat4x4(
            &constants->previousViewProjection,
            previousViewProjection);
        constants->baseColor = batch.baseColor;
        constants->cameraPosition = {
            cameraPosition.x,
            cameraPosition.y,
            cameraPosition.z,
            aquariumOpeningMask
        };
        constants->surfaceParameters = {
            batch.surfaceType,
            time,
            batch.baseColor.w,
            refractionSceneView != nullptr ? 1.0f : 0.0f
        };
        context->Unmap(constantBuffer_.Get(), 0);
        context->DrawIndexed(batch.indexCount, batch.indexStart, 0);
    };

    if (!transparentPass)
    {
        // Opaque architecture establishes depth before creatures and water.
        for (const DrawBatch& batch : drawBatches_)
        {
            if (!batch.transparent)
            {
                drawBatch(batch);
            }
        }
        context->RSSetState(nullptr);
        return;
    }

    // Water and glass read opaque depth but never write it. The biology pass
    // is inserted before this pass so it is naturally filtered by the water.
    const float blendFactor[4] = {};
    context->OMSetBlendState(
        transparentBlendState_.Get(),
        blendFactor,
        0xffffffffu);
    context->OMSetDepthStencilState(
        transparentDepthState_.Get(),
        0);
    context->PSSetShaderResources(
        8,
        1,
        &refractionSceneView);
    ID3D11SamplerState* refractionSampler = refractionSampler_.Get();
    context->PSSetSamplers(3, 1, &refractionSampler);
    for (const DrawBatch& batch : drawBatches_)
    {
        const bool isRefractiveGlass =
            (batch.surfaceType > 8.5f && batch.surfaceType < 9.5f) ||
            (batch.surfaceType > 16.5f && batch.surfaceType < 17.5f);
        const bool matchesLayer =
            layer == TransparentLayer::All ||
            (layer == TransparentLayer::Medium && !isRefractiveGlass) ||
            (layer == TransparentLayer::Glass && isRefractiveGlass);
        if (batch.transparent && matchesLayer)
        {
            drawBatch(batch);
        }
    }
    context->OMSetBlendState(nullptr, blendFactor, 0xffffffffu);
    context->OMSetDepthStencilState(nullptr, 0);
    ID3D11ShaderResourceView* nullRefractionView = nullptr;
    context->PSSetShaderResources(8, 1, &nullRefractionView);
    context->RSSetState(nullptr);
}
