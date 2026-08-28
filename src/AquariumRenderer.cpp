/*==================================================================================================

   [AquariumRenderer.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   Shadow Map、HDR Scene、Froxel Volume、Temporal、Glass Compositeの描画パイプライン
===================================================================================================*/
#include "AquariumRenderer.h"

#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
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
        std::string message = "Shader compilation failed: ";
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

void ThrowIfFailed(HRESULT hr, const char* operation)
{
    if (FAILED(hr))
    {
        throw std::runtime_error(std::string(operation) + " failed.");
    }
}

DirectX::XMMATRIX BuildStageViewProjection(
    float yaw,
    float pitch,
    const DirectX::XMFLOAT3& cameraPosition,
    UINT width,
    UINT height)
{
    using namespace DirectX;
    const float pitchCos = std::cos(pitch);
    const XMVECTOR forward = XMVector3Normalize(XMVectorSet(
        std::sin(yaw) * pitchCos,
        std::sin(pitch),
        std::cos(yaw) * pitchCos,
        0.0f));
    const XMVECTOR right = XMVector3Normalize(XMVectorSet(
        std::cos(yaw),
        0.0f,
        -std::sin(yaw),
        0.0f));
    const XMVECTOR up = XMVector3Normalize(
        XMVector3Cross(forward, right));
    const XMMATRIX view = XMMatrixLookToLH(
        XMLoadFloat3(&cameraPosition),
        forward,
        up);
    const float aspect =
        static_cast<float>(width) /
        static_cast<float>(std::max(height, 1u));
    const float verticalFov =
        2.0f * std::atan(1.0f / 1.45f);
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        verticalFov,
        aspect,
        0.03f,
        120.0f);
    return view * projection;
}
}

void AquariumRenderer::Initialize(ID3D11Device* device, const std::filesystem::path& shaderPath)
{
    // アクアリウム用HLSLは起動時にコンパイルし、エラー内容を例外へ含める。
    const auto vertexBytecode = CompileShader(shaderPath, "VSMain", "vs_5_0");
    const auto shadowVertexBytecode = CompileShader(shaderPath, "VSShadow", "vs_5_0");
    const auto sceneBytecode = CompileShader(shaderPath, "PSScene", "ps_5_0");
    const auto volumeBytecode = CompileShader(shaderPath, "PSVolume", "ps_5_0");
    const auto temporalBytecode = CompileShader(shaderPath, "PSTemporal", "ps_5_0");
    const auto compositeBytecode = CompileShader(shaderPath, "PSComposite", "ps_5_0");

    ThrowIfFailed(
        device->CreateVertexShader(
            vertexBytecode->GetBufferPointer(),
            vertexBytecode->GetBufferSize(),
            nullptr,
            vertexShader_.GetAddressOf()),
        "CreateVertexShader");

    ThrowIfFailed(
        device->CreateVertexShader(
            shadowVertexBytecode->GetBufferPointer(),
            shadowVertexBytecode->GetBufferSize(),
            nullptr,
            shadowVertexShader_.GetAddressOf()),
        "CreateVertexShader (shadow)");

    const D3D11_INPUT_ELEMENT_DESC shadowInputElement{
        "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
        D3D11_INPUT_PER_VERTEX_DATA, 0
    };
    ThrowIfFailed(
        device->CreateInputLayout(
            &shadowInputElement,
            1,
            shadowVertexBytecode->GetBufferPointer(),
            shadowVertexBytecode->GetBufferSize(),
            shadowInputLayout_.GetAddressOf()),
        "CreateInputLayout (shadow)");

    ThrowIfFailed(
        device->CreatePixelShader(
            sceneBytecode->GetBufferPointer(),
            sceneBytecode->GetBufferSize(),
            nullptr,
            scenePixelShader_.GetAddressOf()),
        "CreatePixelShader (scene)");

    ThrowIfFailed(
        device->CreatePixelShader(
            volumeBytecode->GetBufferPointer(),
            volumeBytecode->GetBufferSize(),
            nullptr,
            volumePixelShader_.GetAddressOf()),
        "CreatePixelShader (volume)");

    ThrowIfFailed(
        device->CreatePixelShader(
            temporalBytecode->GetBufferPointer(),
            temporalBytecode->GetBufferSize(),
            nullptr,
            temporalPixelShader_.GetAddressOf()),
        "CreatePixelShader (temporal)");

    ThrowIfFailed(
        device->CreatePixelShader(
            compositeBytecode->GetBufferPointer(),
            compositeBytecode->GetBufferSize(),
            nullptr,
            compositePixelShader_.GetAddressOf()),
        "CreatePixelShader (composite)");

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(FrameConstants);
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ThrowIfFailed(
        device->CreateBuffer(&bufferDesc, nullptr, frameConstantBuffer_.GetAddressOf()),
        "CreateBuffer");

    bufferDesc.ByteWidth = sizeof(ShadowConstants);
    ThrowIfFailed(
        device->CreateBuffer(&bufferDesc, nullptr, shadowConstantBuffer_.GetAddressOf()),
        "CreateBuffer (shadow constants)");

    CreateStaticResources(device);

    const std::filesystem::path stageShaderPath =
        shaderPath.parent_path() / L"Stage.hlsl";
    const std::filesystem::path stageModelPath =
        shaderPath.parent_path().parent_path() /
        L"model" /
        L"map.glb";
    stageModel_.Initialize(
        device,
        stageModelPath,
        stageShaderPath);

    const std::filesystem::path aquariumGreyboxPath =
        shaderPath.parent_path().parent_path() /
        L"model" /
        L"aquarium_route_01_02.glb";
    StageModel::ImportOptions aquariumGreyboxImport;
    aquariumGreyboxImport.hideAuthoringSurfaces = false;
    aquariumGreyboxModel_.Initialize(
        device,
        aquariumGreyboxPath,
        stageShaderPath,
        aquariumGreyboxImport);

    const std::filesystem::path underwaterArchPath =
        shaderPath.parent_path().parent_path() /
        L"model" /
        L"aquarium_underwater_arch.glb";
    underwaterArchModel_.Initialize(
        device,
        underwaterArchPath,
        stageShaderPath,
        aquariumGreyboxImport);

    const std::filesystem::path watatsumiTankPath =
        shaderPath.parent_path().parent_path() /
        L"model" /
        L"aquarium_watatsumi_hall.glb";
    watatsumiTankModel_.Initialize(
        device,
        watatsumiTankPath,
        stageShaderPath,
        aquariumGreyboxImport);
    jellyfishRenderer_.Initialize(
        device,
        shaderPath.parent_path() / L"Jellyfish.hlsl");
}

void AquariumRenderer::Render(
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* target,
    UINT width,
    UINT height,
    float time,
    float deltaTime,
    const AquariumSettings& settings)
{
    // 描画順:
    // 1. 水面で屈折した3灯のShadow Map
    // 2. HDR水槽シーン、Linear Depth、Motion Vector
    // 3. 低解像度Froxelへの散乱注入と視線方向積分
    // 4. Motion Vectorを使ったTemporal Reprojection
    // 5. ガラス屈折、色収差、Bloom、Tone Mappingを含む最終合成
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    EnsureSizeResources(device.Get(), width, height);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    ThrowIfFailed(
        context->Map(frameConstantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
        "Map");

    const bool viewHistoryValid =
        historyValid_ &&
        std::abs(previousViewMode_ - settings.viewMode) < 0.5f &&
        previousStageMode_ == settings.stageMode &&
        previousGreyboxMode_ == settings.greyboxMode &&
        previousUnderwaterArchMode_ == settings.underwaterArchMode &&
        previousWatatsumiTankMode_ == settings.watatsumiTankMode;
    auto* constants = static_cast<FrameConstants*>(mapped.pData);
    *constants = {
        time,
        deltaTime,
        static_cast<float>(width),
        static_cast<float>(height),
        settings.cameraYaw,
        settings.cameraPitch,
        settings.causticsStrength,
        (settings.greyboxMode && !settings.underwaterArchMode &&
         !settings.watatsumiTankMode)
            ? 0.0f
            : settings.volumeStrength *
                (settings.underwaterArchMode
                    ? 0.20f
                    : (settings.watatsumiTankMode ? 0.0f : 1.0f)),
        settings.exposure,
        settings.waterClarity,
        settings.anisotropy,
        settings.historyWeight,
        previousCameraYaw_,
        previousCameraPitch_,
        viewHistoryValid ? 1.0f : 0.0f,
        static_cast<float>(frameIndex_),
        settings.viewMode,
        previousViewMode_,
        settings.glassDistortion,
        settings.greyboxMode ? 1.0f : 0.0f,
        settings.cameraPositionX,
        settings.cameraPositionY,
        settings.cameraPositionZ,
        settings.underwaterArchMode ? 1.0f : 0.0f,
        previousCameraPosition_.x,
        previousCameraPosition_.y,
        previousCameraPosition_.z,
        0.0f
    };
    context->Unmap(frameConstantBuffer_.Get(), 0);

    using namespace DirectX;
    std::array<AquariumLight, kMaxAquariumLights> lights{};
    UINT activeLightCount = 3;
    if (settings.underwaterArchMode)
    {
        activeLightCount = 3;
        constexpr std::array<float, 4> routePositions{
            8.0f, 24.0f, 40.0f, 40.0f
        };
        constexpr std::array<float, 4> lateralPositions{
            -2.10f, 2.00f, -1.65f, -1.65f
        };
        constexpr std::array<XMFLOAT3, 4> lightDirections{
            XMFLOAT3{0.08f, -1.0f, 0.10f},
            XMFLOAT3{-0.06f, -1.0f, -0.08f},
            XMFLOAT3{0.09f, -1.0f, 0.07f},
            XMFLOAT3{0.09f, -1.0f, 0.07f}
        };
        constexpr std::array<float, 4> intensity{
            1.00f, 0.84f, 0.72f, 0.72f
        };
        for (UINT lightIndex = 0; lightIndex < activeLightCount; ++lightIndex)
        {
            lights[lightIndex] = {
                {routePositions[lightIndex], 7.40f,
                 lateralPositions[lightIndex]},
                intensity[lightIndex],
                lightDirections[lightIndex],
                31.0f,
                {0.340f, 0.780f, 1.080f},
                5.8f
            };
        }
    }
    else if (settings.watatsumiTankMode)
    {
        activeLightCount = 2;
        // Broad banks sit above the inferred 650 m3 water surface.  They are
        // deliberately unequal so the tank reads as the hall's only source.
        lights[0] = {
            {10.0f, 9.2f, -3.6f}, 0.90f,
            {0.12f, -1.0f, 0.08f}, 42.0f,
            {0.045f, 0.44f, 1.10f}, 6.45f
        };
        lights[1] = {
            {14.5f, 9.2f, 3.8f}, 0.72f,
            {-0.10f, -1.0f, -0.06f}, 45.0f,
            {0.020f, 0.25f, 0.82f}, 6.45f
        };
    }
    else
    {
        lights[0] = {
            {-2.9f, 3.5f, 1.4f}, 1.0f,
            {0.14f, -1.0f, 0.10f}, 34.0f,
            {0.088f, 0.42f, 1.04f}, 2.65f
        };
        lights[1] = {
            {0.2f, 3.5f, 2.6f}, 1.0f,
            {-0.05f, -1.0f, 0.02f}, 34.0f,
            {0.088f, 0.42f, 1.04f}, 2.65f
        };
        lights[2] = {
            {3.35f, 3.5f, 3.9f}, 1.0f,
            {-0.16f, -1.0f, -0.08f}, 34.0f,
            {0.088f, 0.42f, 1.04f}, 2.65f
        };
    }

    ShadowConstants shadowConstants{};
    shadowConstants.activeLightCount =
        static_cast<float>(activeLightCount);
    for (UINT lightIndex = 0;
         lightIndex < activeLightCount;
         ++lightIndex)
    {
        const XMVECTOR lightOrigin =
            XMLoadFloat3(&lights[lightIndex].position);
        const XMVECTOR lightAxis = XMVector3Normalize(
            XMLoadFloat3(&lights[lightIndex].direction));
        const float axisY = XMVectorGetY(lightAxis);
        float surfaceDistance =
            (lights[lightIndex].surfaceHeight -
             XMVectorGetY(lightOrigin)) / axisY;
        XMVECTOR surfacePoint =
            lightOrigin + lightAxis * surfaceDistance;

        const float surfaceX = XMVectorGetX(surfacePoint);
        const float surfaceZ = XMVectorGetZ(surfacePoint);
        const float waveA = surfaceX * 1.4f + time * 0.63f;
        const float waveB =
            (surfaceX * 0.7f + surfaceZ * 1.1f) * 2.2f -
            time * 0.48f;
        const float waveC = surfaceZ * 1.7f - time * 0.54f;
        const float waveD =
            (-surfaceX * 1.2f + surfaceZ * 0.6f) * 2.5f +
            time * 0.39f;
        const float surfaceHeight =
            lights[lightIndex].surfaceHeight +
            std::sin(waveA) * 0.055f +
            std::sin(waveB) * 0.028f +
            std::sin(waveC) * 0.045f +
            std::sin(waveD) * 0.022f;
        surfaceDistance =
            (surfaceHeight - XMVectorGetY(lightOrigin)) / axisY;
        surfacePoint =
            lightOrigin + lightAxis * surfaceDistance;

        const float derivativeX =
            std::cos(waveA) * 0.055f * 1.4f +
            std::cos(waveB) * 0.028f * 0.7f * 2.2f +
            std::cos(waveD) * 0.022f * -1.2f * 2.5f;
        const float derivativeZ =
            std::cos(waveB) * 0.028f * 1.1f * 2.2f +
            std::cos(waveC) * 0.045f * 1.7f +
            std::cos(waveD) * 0.022f * 0.6f * 2.5f;
        const XMVECTOR surfaceNormal = XMVector3Normalize(
            XMVectorSet(-derivativeX, 1.0f, -derivativeZ, 0.0f));
        const XMVECTOR refractedAxis = XMVector3Normalize(
            XMVector3Refract(
                lightAxis,
                surfaceNormal,
                1.0f / 1.333f));
        const float focus =
            std::clamp(
                (1.0f - XMVectorGetY(surfaceNormal)) * 9.0f,
                0.0f,
                1.0f);

        XMStoreFloat4(
            &shadowConstants.lightSurfaceOrigin[lightIndex],
            XMVectorSetW(surfacePoint, 1.0f));
        XMStoreFloat4(
            &shadowConstants.lightRefractedAxis[lightIndex],
            XMVectorSetW(refractedAxis, focus));
        shadowConstants.lightColorStrength[lightIndex] = {
            lights[lightIndex].color.x,
            lights[lightIndex].color.y,
            lights[lightIndex].color.z,
            lights[lightIndex].intensity
        };

        const XMMATRIX view = XMMatrixLookToLH(
            surfacePoint,
            refractedAxis,
            XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
        const XMMATRIX projection = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(lights[lightIndex].coneAngleDegrees),
            1.0f,
            0.03f,
            settings.underwaterArchMode ? 22.0f : 15.0f);
        XMStoreFloat4x4(
            &shadowConstants.lightViewProjection[lightIndex],
            view * projection);
    }

    ThrowIfFailed(
        context->Map(
            shadowConstantBuffer_.Get(),
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mapped),
        "Map (light constants)");
    *static_cast<ShadowConstants*>(mapped.pData) = shadowConstants;
    context->Unmap(shadowConstantBuffer_.Get(), 0);

    // Pass 0: render actual occluder meshes into one shadow-map slice per light.
    ID3D11ShaderResourceView* shadowNullView = nullptr;
    context->PSSetShaderResources(5, 1, &shadowNullView);
    const D3D11_VIEWPORT shadowViewport{
        0.0f, 0.0f, 512.0f, 512.0f, 0.0f, 1.0f
    };
    context->RSSetViewports(1, &shadowViewport);
    context->RSSetState(shadowRasterizerState_.Get());
    context->IASetInputLayout(shadowInputLayout_.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT shadowStride = sizeof(DirectX::XMFLOAT3);
    const UINT shadowOffset = 0;
    ID3D11Buffer* shadowVertexBuffer = shadowVertexBuffer_.Get();
    context->IASetVertexBuffers(0, 1, &shadowVertexBuffer, &shadowStride, &shadowOffset);
    context->IASetIndexBuffer(shadowIndexBuffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShader(shadowVertexShader_.Get(), nullptr, 0);
    ID3D11Buffer* frameBuffer = frameConstantBuffer_.Get();
    ID3D11Buffer* shadowBuffer = shadowConstantBuffer_.Get();
    context->VSSetConstantBuffers(0, 1, &frameBuffer);
    context->VSSetConstantBuffers(1, 1, &shadowBuffer);
    context->PSSetShader(nullptr, nullptr, 0);

    const UINT shadowLightCount =
        settings.underwaterArchMode ? 0u : 3u;
    for (UINT lightIndex = 0;
         lightIndex < shadowLightCount;
         ++lightIndex)
    {
        shadowConstants.currentLightIndex = static_cast<float>(lightIndex);
        ThrowIfFailed(
            context->Map(shadowConstantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
            "Map (shadow constants)");
        *static_cast<ShadowConstants*>(mapped.pData) = shadowConstants;
        context->Unmap(shadowConstantBuffer_.Get(), 0);
        context->ClearDepthStencilView(
            shadowDepthViews_[lightIndex].Get(),
            D3D11_CLEAR_DEPTH,
            1.0f,
            0);
        context->OMSetRenderTargets(0, nullptr, shadowDepthViews_[lightIndex].Get());
        context->DrawIndexedInstanced(shadowIndexCount_, 3, 0, 0, 0);
    }
    context->OMSetRenderTargets(0, nullptr, nullptr);
    context->RSSetState(nullptr);

    const D3D11_VIEWPORT fullViewport{
        0.0f,
        0.0f,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0f,
        1.0f
    };

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader_.Get(), nullptr, 0);

    ID3D11Buffer* constantBuffer = frameConstantBuffer_.Get();
    context->PSSetConstantBuffers(0, 1, &constantBuffer);
    context->PSSetConstantBuffers(1, 1, &shadowBuffer);

    // Pass 1: full-resolution HDR scene and analytic linear depth.
    context->RSSetViewports(1, &fullViewport);
    ID3D11RenderTargetView* sceneTargets[] = {
        sceneColorTarget_.Get(),
        sceneDepthTarget_.Get(),
        motionTarget_.Get()
    };
    context->OMSetRenderTargets(3, sceneTargets, nullptr);
    context->PSSetShader(scenePixelShader_.Get(), nullptr, 0);
    context->Draw(3, 0);

    // Route preview owns the complete frame. Remove the old analytic room so
    // the authored tanks, rather than a full-screen aquarium, light the hall.
    if (settings.greyboxMode)
    {
        const float routeDarkHall[4] = {
            0.00015f, 0.00035f, 0.00065f, 1.0f
        };
        const float watatsumiBlueBlack[4] = {
            0.000015f, 0.000040f, 0.000100f, 1.0f
        };
        const float* darkHall = settings.watatsumiTankMode
            ? watatsumiBlueBlack
            : routeDarkHall;
        const float farDepth[4] = {30.0f, 30.0f, 30.0f, 30.0f};
        const float zeroMotion[4] = {};
        context->ClearRenderTargetView(
            sceneColorTarget_.Get(),
            darkHall);
        context->ClearRenderTargetView(
            sceneDepthTarget_.Get(),
            farDepth);
        context->ClearRenderTargetView(
            motionTarget_.Get(),
            zeroMotion);
    }

    // Draw the imported stage over either the analytic tank or the dark route.
    StageModel* activeStageModel = settings.watatsumiTankMode
        ? &watatsumiTankModel_
        : (settings.underwaterArchMode
        ? &underwaterArchModel_
        : (settings.greyboxMode
            ? &aquariumGreyboxModel_
            : &stageModel_));
    if (settings.stageMode && activeStageModel->IsLoaded())
    {
        context->ClearDepthStencilView(
            stageDepthView_.Get(),
            D3D11_CLEAR_DEPTH,
            1.0f,
            0);
        context->OMSetRenderTargets(
            3,
            sceneTargets,
            stageDepthView_.Get());

        const DirectX::XMFLOAT3 currentCameraPosition{
            settings.cameraPositionX,
            settings.cameraPositionY,
            settings.cameraPositionZ
        };
        const DirectX::XMMATRIX currentViewProjection =
            BuildStageViewProjection(
                settings.cameraYaw,
                settings.cameraPitch,
                currentCameraPosition,
                width,
                height);
        const DirectX::XMMATRIX previousViewProjection =
            BuildStageViewProjection(
                previousCameraYaw_,
                previousCameraPitch_,
                previousCameraPosition_,
                width,
                height);
        const float openingMask =
            settings.greyboxMode ? 0.0f : 1.0f;
        activeStageModel->RenderOpaque(
            context,
            currentViewProjection,
            previousViewProjection,
            currentCameraPosition,
            time,
            openingMask,
            &settings.localLighting);
        if (settings.greyboxMode && !settings.underwaterArchMode &&
            !settings.watatsumiTankMode)
        {
            jellyfishRenderer_.Render(
                context,
                currentViewProjection,
                previousViewProjection,
                currentCameraPosition,
                time);
        }

        // Glass cannot sample the HDR target while that same texture is bound
        // for rendering. Preserve the opaque/biology result once, then let only
        // glass pixels read this copy for screen-space refraction.
        ID3D11RenderTargetView* unboundTargets[] = {
            nullptr, nullptr, nullptr
        };
        context->OMSetRenderTargets(3, unboundTargets, nullptr);
        context->CopyResource(
            refractionCopyTexture_.Get(),
            sceneColorTexture_.Get());
        context->OMSetRenderTargets(
            3,
            sceneTargets,
            stageDepthView_.Get());

        if (settings.underwaterArchMode)
        {
            // First refract the opaque scene through the water medium. Glass
            // must then sample a second copy containing that water result;
            // otherwise it replaces the underwater lighting with the older
            // opaque-only image and makes the overhead banks disappear.
            activeStageModel->RenderTransparent(
                context,
                currentViewProjection,
                previousViewProjection,
                currentCameraPosition,
                time,
                openingMask,
                refractionCopyView_.Get(),
                StageModel::TransparentLayer::ArchMedium,
                nullptr);

            context->OMSetRenderTargets(3, unboundTargets, nullptr);
            context->CopyResource(
                refractionCopyTexture_.Get(),
                sceneColorTexture_.Get());
            context->OMSetRenderTargets(
                3,
                sceneTargets,
                stageDepthView_.Get());

            activeStageModel->RenderTransparent(
                context,
                currentViewProjection,
                previousViewProjection,
                currentCameraPosition,
                time,
                openingMask,
                refractionCopyView_.Get(),
                StageModel::TransparentLayer::ArchGlass,
                nullptr);
        }
        else
        {
            activeStageModel->RenderTransparent(
                context,
                currentViewProjection,
                previousViewProjection,
                currentCameraPosition,
                time,
                openingMask,
                refractionCopyView_.Get(),
                StageModel::TransparentLayer::All,
                nullptr);
        }
    }

    // Pass 2: half-resolution volumetric lighting.
    ID3D11RenderTargetView* nullTargets[] = {nullptr, nullptr, nullptr};
    context->OMSetRenderTargets(3, nullTargets, nullptr);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader_.Get(), nullptr, 0);

    const UINT volumeWidth = (width + 2) / 3;
    const UINT volumeHeight = (height + 2) / 3;
    const D3D11_VIEWPORT volumeViewport{
        0.0f,
        0.0f,
        static_cast<float>(volumeWidth),
        static_cast<float>(volumeHeight),
        0.0f,
        1.0f
    };
    context->RSSetViewports(1, &volumeViewport);
    ID3D11RenderTargetView* volumeTarget = volumeTarget_.Get();
    context->OMSetRenderTargets(1, &volumeTarget, nullptr);
    context->PSSetShader(volumePixelShader_.Get(), nullptr, 0);

    ID3D11ShaderResourceView* volumeInputs[] = {
        noiseTextureView_.Get(),
        nullptr,
        sceneDepthView_.Get(),
        nullptr,
        nullptr,
        shadowTextureView_.Get()
    };
    context->PSSetShaderResources(0, 6, volumeInputs);
    ID3D11SamplerState* samplers[] = {
        linearWrapSampler_.Get(),
        linearClampSampler_.Get(),
        shadowComparisonSampler_.Get()
    };
    context->PSSetSamplers(0, 3, samplers);
    context->Draw(3, 0);

    // Pass 3: temporal reprojection into a ping-pong history buffer.
    ID3D11ShaderResourceView* nullViews[] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    context->PSSetShaderResources(0, 7, nullViews);
    context->OMSetRenderTargets(0, nullptr, nullptr);

    const UINT historyWriteIndex = 1u - historyReadIndex_;
    ID3D11RenderTargetView* historyTarget = historyTargets_[historyWriteIndex].Get();
    context->OMSetRenderTargets(1, &historyTarget, nullptr);
    context->PSSetShader(temporalPixelShader_.Get(), nullptr, 0);
    ID3D11ShaderResourceView* temporalInputs[] = {
        nullptr,
        nullptr,
        sceneDepthView_.Get(),
        volumeView_.Get(),
        historyViews_[historyReadIndex_].Get(),
        nullptr,
        motionView_.Get()
    };
    context->PSSetShaderResources(0, 7, temporalInputs);
    context->Draw(3, 0);

    // Pass 4: depth-aware volume upsample, HDR composite and tone mapping.
    context->PSSetShaderResources(0, 7, nullViews);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    context->RSSetViewports(1, &fullViewport);
    context->OMSetRenderTargets(1, &target, nullptr);
    context->PSSetShader(compositePixelShader_.Get(), nullptr, 0);
    ID3D11ShaderResourceView* compositeInputs[] = {
        nullptr,
        sceneColorView_.Get(),
        sceneDepthView_.Get(),
        historyViews_[historyWriteIndex].Get()
    };
    context->PSSetShaderResources(0, 4, compositeInputs);
    context->PSSetSamplers(0, 3, samplers);
    context->Draw(3, 0);

    context->PSSetShaderResources(0, 7, nullViews);

    historyReadIndex_ = historyWriteIndex;
    historyValid_ = true;
    previousCameraYaw_ = settings.cameraYaw;
    previousCameraPitch_ = settings.cameraPitch;
    previousViewMode_ = settings.viewMode;
    previousStageMode_ = settings.stageMode;
    previousGreyboxMode_ = settings.greyboxMode;
    previousUnderwaterArchMode_ = settings.underwaterArchMode;
    previousWatatsumiTankMode_ = settings.watatsumiTankMode;
    previousCameraPosition_ = {
        settings.cameraPositionX,
        settings.cameraPositionY,
        settings.cameraPositionZ
    };
    ++frameIndex_;
}

void AquariumRenderer::CreateStaticResources(ID3D11Device* device)
{
    // 解像度変更では作り直さない共有GPUリソース。
    constexpr UINT latitudeSegments = 18;
    constexpr UINT longitudeSegments = 24;
    std::vector<DirectX::XMFLOAT3> sphereVertices;
    std::vector<std::uint32_t> sphereIndices;
    sphereVertices.reserve((latitudeSegments + 1) * (longitudeSegments + 1));
    for (UINT latitude = 0; latitude <= latitudeSegments; ++latitude)
    {
        const float theta = DirectX::XM_PI * static_cast<float>(latitude) / latitudeSegments;
        for (UINT longitude = 0; longitude <= longitudeSegments; ++longitude)
        {
            const float phi = DirectX::XM_2PI * static_cast<float>(longitude) / longitudeSegments;
            sphereVertices.emplace_back(
                std::sin(theta) * std::cos(phi),
                std::cos(theta),
                std::sin(theta) * std::sin(phi));
        }
    }
    for (UINT latitude = 0; latitude < latitudeSegments; ++latitude)
    {
        for (UINT longitude = 0; longitude < longitudeSegments; ++longitude)
        {
            const UINT row = longitudeSegments + 1;
            const UINT a = latitude * row + longitude;
            const UINT b = a + row;
            sphereIndices.insert(
                sphereIndices.end(),
                {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    shadowIndexCount_ = static_cast<UINT>(sphereIndices.size());

    D3D11_BUFFER_DESC meshBufferDesc{};
    meshBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    meshBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    meshBufferDesc.ByteWidth = static_cast<UINT>(
        sphereVertices.size() * sizeof(DirectX::XMFLOAT3));
    D3D11_SUBRESOURCE_DATA meshInitialData{sphereVertices.data()};
    ThrowIfFailed(
        device->CreateBuffer(&meshBufferDesc, &meshInitialData, shadowVertexBuffer_.GetAddressOf()),
        "CreateBuffer (shadow vertices)");
    meshBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    meshBufferDesc.ByteWidth = static_cast<UINT>(
        sphereIndices.size() * sizeof(std::uint32_t));
    meshInitialData.pSysMem = sphereIndices.data();
    ThrowIfFailed(
        device->CreateBuffer(&meshBufferDesc, &meshInitialData, shadowIndexBuffer_.GetAddressOf()),
        "CreateBuffer (shadow indices)");

    constexpr UINT noiseSize = 64;
    std::vector<std::uint8_t> noiseData(noiseSize * noiseSize * noiseSize);

    auto hash = [](std::uint32_t value)
    {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return value;
    };

    for (UINT z = 0; z < noiseSize; ++z)
    {
        for (UINT y = 0; y < noiseSize; ++y)
        {
            for (UINT x = 0; x < noiseSize; ++x)
            {
                const std::uint32_t seed =
                    x + y * noiseSize + z * noiseSize * noiseSize;
                noiseData[seed] = static_cast<std::uint8_t>(hash(seed) & 0xffu);
            }
        }
    }

    D3D11_TEXTURE3D_DESC noiseDesc{};
    noiseDesc.Width = noiseSize;
    noiseDesc.Height = noiseSize;
    noiseDesc.Depth = noiseSize;
    noiseDesc.MipLevels = 1;
    noiseDesc.Format = DXGI_FORMAT_R8_UNORM;
    noiseDesc.Usage = D3D11_USAGE_IMMUTABLE;
    noiseDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA noiseInitialData{};
    noiseInitialData.pSysMem = noiseData.data();
    noiseInitialData.SysMemPitch = noiseSize;
    noiseInitialData.SysMemSlicePitch = noiseSize * noiseSize;

    ThrowIfFailed(
        device->CreateTexture3D(&noiseDesc, &noiseInitialData, noiseTexture_.GetAddressOf()),
        "CreateTexture3D (noise)");
    ThrowIfFailed(
        device->CreateShaderResourceView(noiseTexture_.Get(), nullptr, noiseTextureView_.GetAddressOf()),
        "CreateShaderResourceView (noise)");

    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ThrowIfFailed(
        device->CreateSamplerState(&samplerDesc, linearWrapSampler_.GetAddressOf()),
        "CreateSamplerState (wrap)");

    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    ThrowIfFailed(
        device->CreateSamplerState(&samplerDesc, linearClampSampler_.GetAddressOf()),
        "CreateSamplerState (clamp)");

    D3D11_SAMPLER_DESC shadowSamplerDesc{};
    shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    shadowSamplerDesc.BorderColor[0] = 1.0f;
    shadowSamplerDesc.BorderColor[1] = 1.0f;
    shadowSamplerDesc.BorderColor[2] = 1.0f;
    shadowSamplerDesc.BorderColor[3] = 1.0f;
    shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ThrowIfFailed(
        device->CreateSamplerState(
            &shadowSamplerDesc,
            shadowComparisonSampler_.GetAddressOf()),
        "CreateSamplerState (shadow comparison)");

    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.DepthBias = 180;
    rasterizerDesc.SlopeScaledDepthBias = 1.5f;
    ThrowIfFailed(
        device->CreateRasterizerState(&rasterizerDesc, shadowRasterizerState_.GetAddressOf()),
        "CreateRasterizerState (shadow)");

    D3D11_TEXTURE2D_DESC shadowTextureDesc{};
    shadowTextureDesc.Width = 512;
    shadowTextureDesc.Height = 512;
    shadowTextureDesc.MipLevels = 1;
    shadowTextureDesc.ArraySize = 3;
    shadowTextureDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    shadowTextureDesc.SampleDesc.Count = 1;
    shadowTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    shadowTextureDesc.BindFlags =
        D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    ThrowIfFailed(
        device->CreateTexture2D(&shadowTextureDesc, nullptr, shadowTexture_.GetAddressOf()),
        "CreateTexture2D (shadow array)");

    for (UINT lightIndex = 0; lightIndex < 3; ++lightIndex)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc{};
        depthViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        depthViewDesc.Texture2DArray.MipSlice = 0;
        depthViewDesc.Texture2DArray.FirstArraySlice = lightIndex;
        depthViewDesc.Texture2DArray.ArraySize = 1;
        ThrowIfFailed(
            device->CreateDepthStencilView(
                shadowTexture_.Get(),
                &depthViewDesc,
                shadowDepthViews_[lightIndex].GetAddressOf()),
            "CreateDepthStencilView (shadow slice)");
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC shadowViewDesc{};
    shadowViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
    shadowViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    shadowViewDesc.Texture2DArray.MostDetailedMip = 0;
    shadowViewDesc.Texture2DArray.MipLevels = 1;
    shadowViewDesc.Texture2DArray.FirstArraySlice = 0;
    shadowViewDesc.Texture2DArray.ArraySize = 3;
    ThrowIfFailed(
        device->CreateShaderResourceView(
            shadowTexture_.Get(),
            &shadowViewDesc,
            shadowTextureView_.GetAddressOf()),
        "CreateShaderResourceView (shadow array)");
}

void AquariumRenderer::EnsureSizeResources(ID3D11Device* device, UINT width, UINT height)
{
    // Resize時のみ再確保し、通常フレームでのGPUリソース生成を避ける。
    if (resourceWidth_ == width && resourceHeight_ == height)
    {
        return;
    }

    sceneColorTexture_.Reset();
    sceneColorTarget_.Reset();
    sceneColorView_.Reset();
    refractionCopyTexture_.Reset();
    refractionCopyView_.Reset();
    sceneDepthTexture_.Reset();
    sceneDepthTarget_.Reset();
    sceneDepthView_.Reset();
    motionTexture_.Reset();
    motionTarget_.Reset();
    motionView_.Reset();
    stageDepthTexture_.Reset();
    stageDepthView_.Reset();
    volumeTexture_.Reset();
    volumeTarget_.Reset();
    volumeView_.Reset();
    for (UINT index = 0; index < 2; ++index)
    {
        historyTextures_[index].Reset();
        historyTargets_[index].Reset();
        historyViews_[index].Reset();
    }

    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    ThrowIfFailed(
        device->CreateTexture2D(&textureDesc, nullptr, sceneColorTexture_.GetAddressOf()),
        "CreateTexture2D (scene color)");
    ThrowIfFailed(
        device->CreateRenderTargetView(sceneColorTexture_.Get(), nullptr, sceneColorTarget_.GetAddressOf()),
        "CreateRenderTargetView (scene color)");
    ThrowIfFailed(
        device->CreateShaderResourceView(sceneColorTexture_.Get(), nullptr, sceneColorView_.GetAddressOf()),
        "CreateShaderResourceView (scene color)");
    D3D11_TEXTURE2D_DESC refractionCopyDesc = textureDesc;
    refractionCopyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ThrowIfFailed(
        device->CreateTexture2D(
            &refractionCopyDesc,
            nullptr,
            refractionCopyTexture_.GetAddressOf()),
        "CreateTexture2D (refraction copy)");
    ThrowIfFailed(
        device->CreateShaderResourceView(
            refractionCopyTexture_.Get(),
            nullptr,
            refractionCopyView_.GetAddressOf()),
        "CreateShaderResourceView (refraction copy)");

    textureDesc.Format = DXGI_FORMAT_R32_FLOAT;
    ThrowIfFailed(
        device->CreateTexture2D(&textureDesc, nullptr, sceneDepthTexture_.GetAddressOf()),
        "CreateTexture2D (scene depth)");
    ThrowIfFailed(
        device->CreateRenderTargetView(sceneDepthTexture_.Get(), nullptr, sceneDepthTarget_.GetAddressOf()),
        "CreateRenderTargetView (scene depth)");
    ThrowIfFailed(
        device->CreateShaderResourceView(sceneDepthTexture_.Get(), nullptr, sceneDepthView_.GetAddressOf()),
        "CreateShaderResourceView (scene depth)");

    textureDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
    ThrowIfFailed(
        device->CreateTexture2D(&textureDesc, nullptr, motionTexture_.GetAddressOf()),
        "CreateTexture2D (motion vectors)");
    ThrowIfFailed(
        device->CreateRenderTargetView(motionTexture_.Get(), nullptr, motionTarget_.GetAddressOf()),
        "CreateRenderTargetView (motion vectors)");
    ThrowIfFailed(
        device->CreateShaderResourceView(motionTexture_.Get(), nullptr, motionView_.GetAddressOf()),
        "CreateShaderResourceView (motion vectors)");

    D3D11_TEXTURE2D_DESC stageDepthDesc{};
    stageDepthDesc.Width = width;
    stageDepthDesc.Height = height;
    stageDepthDesc.MipLevels = 1;
    stageDepthDesc.ArraySize = 1;
    stageDepthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    stageDepthDesc.SampleDesc.Count = 1;
    stageDepthDesc.Usage = D3D11_USAGE_DEFAULT;
    stageDepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ThrowIfFailed(
        device->CreateTexture2D(
            &stageDepthDesc,
            nullptr,
            stageDepthTexture_.GetAddressOf()),
        "CreateTexture2D (stage depth)");
    ThrowIfFailed(
        device->CreateDepthStencilView(
            stageDepthTexture_.Get(),
            nullptr,
            stageDepthView_.GetAddressOf()),
        "CreateDepthStencilView (stage depth)");

    textureDesc.Width = (width + 2) / 3;
    textureDesc.Height = (height + 2) / 3;
    textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    ThrowIfFailed(
        device->CreateTexture2D(&textureDesc, nullptr, volumeTexture_.GetAddressOf()),
        "CreateTexture2D (volume)");
    ThrowIfFailed(
        device->CreateRenderTargetView(volumeTexture_.Get(), nullptr, volumeTarget_.GetAddressOf()),
        "CreateRenderTargetView (volume)");
    ThrowIfFailed(
        device->CreateShaderResourceView(volumeTexture_.Get(), nullptr, volumeView_.GetAddressOf()),
        "CreateShaderResourceView (volume)");

    for (UINT index = 0; index < 2; ++index)
    {
        ThrowIfFailed(
            device->CreateTexture2D(&textureDesc, nullptr, historyTextures_[index].GetAddressOf()),
            "CreateTexture2D (volume history)");
        ThrowIfFailed(
            device->CreateRenderTargetView(historyTextures_[index].Get(), nullptr, historyTargets_[index].GetAddressOf()),
            "CreateRenderTargetView (volume history)");
        ThrowIfFailed(
            device->CreateShaderResourceView(historyTextures_[index].Get(), nullptr, historyViews_[index].GetAddressOf()),
            "CreateShaderResourceView (volume history)");
    }

    resourceWidth_ = width;
    resourceHeight_ = height;
    historyReadIndex_ = 0;
    frameIndex_ = 0;
    historyValid_ = false;
}
