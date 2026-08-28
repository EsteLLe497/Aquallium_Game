/*==================================================================================================

   [AquariumScene.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   水中ビュー、ガラス越しビュー、ステージ確認ビューの更新と描画
===================================================================================================*/
#include "AquariumScene.h"

#include "../framework/input.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <windows.h>

namespace
{
DirectX::XMFLOAT3 EvaluateWatatsumiRampPoint(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float y = 0.18f + 12.22f * t;
    const auto line = [y](
        float ax, float az,
        float bx, float bz,
        float u)
    {
        return DirectX::XMFLOAT3{
            ax + (bx - ax) * u,
            y,
            az + (bz - az) * u};
    };
    if (t < 0.06f)
    {
        return line(-0.5f, 17.8f, 5.8f, 17.8f, t / 0.06f);
    }
    if (t < 0.20f)
    {
        return line(
            5.8f, 17.8f, 17.8f, 17.8f,
            (t - 0.06f) / 0.14f);
    }
    if (t < 0.80f)
    {
        const float u = (t - 0.20f) / 0.60f;
        const float angle =
            DirectX::XM_PIDIV2 - DirectX::XM_PI * u;
        return DirectX::XMFLOAT3{
            17.8f + std::cos(angle) * 17.8f,
            y,
            std::sin(angle) * 17.8f};
    }
    if (t < 0.97f)
    {
        return line(
            17.8f, -17.8f, 5.8f, -17.8f,
            (t - 0.80f) / 0.17f);
    }
    return line(
        5.8f, -17.8f, 3.6f, -17.8f,
        (t - 0.97f) / 0.03f);
}
}

AquariumScene::AquariumScene(
    ID3D11Device* device,
    const std::filesystem::path& shaderPath)
{
    renderer_.Initialize(device, shaderPath);
    settings_.localLighting.lightCount = 4;
    settings_.localLighting.lights[0] = {
        {-5.0f, 4.4f, -4.5f}, 7.0f,
        {0.18f, -1.0f, 0.15f}, 4.2f,
        {0.18f, 0.48f, 0.90f}, 24.0f, 42.0f,
        lighting::LocalLightType::Spot, true};
    settings_.localLighting.lights[1] = {
        {2.2f, 5.2f, 4.0f}, 8.5f,
        {-0.12f, -1.0f, -0.08f}, 4.8f,
        {0.08f, 0.34f, 0.86f}, 26.0f, 46.0f,
        lighting::LocalLightType::Spot, true};
    settings_.localLighting.lights[2] = {
        {6.4f, 13.2f, -17.8f}, 8.0f,
        {-0.45f, -0.72f, 0.0f}, 3.8f,
        {0.12f, 0.46f, 1.0f}, 20.0f, 38.0f,
        lighting::LocalLightType::Spot, true};
    settings_.localLighting.lights[3] = {
        {-13.5f, 7.1f, 0.0f}, 5.5f,
        {0.0f, -1.0f, 0.0f}, 2.4f,
        {0.16f, 0.38f, 0.72f}, 30.0f, 54.0f,
        lighting::LocalLightType::Spot, true};
    BuildWatatsumiCollision();
}

void AquariumScene::BuildWatatsumiCollision()
{
    using physics::ColliderTag;
    using physics::CollisionLayer;
    using physics::LayerMask;

    watatsumiCollision_.Clear();
    watatsumiCollision_.AddWalkableRect({
        L"Watatsumi_1F_PublicFloor",
        -27.55f, 6.20f, -23.55f, 23.55f, 0.0f,
        ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddWalkableRect({
        L"Watatsumi_2F_NorthWalkway",
        -21.0f, 4.0f, 15.70f, 19.90f, 12.28f,
        ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddWalkableRect({
        L"Watatsumi_2F_SouthWalkway",
        -21.0f, 4.0f, -19.90f, -15.70f, 12.28f,
        ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddWalkableRect({
        L"Watatsumi_2F_RearWalkway",
        -23.10f, -18.90f, -19.90f, 19.90f, 12.28f,
        ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});

    physics::PathSurface ramp;
    ramp.name = L"Watatsumi_RightHelixRamp";
    ramp.halfWidth = 2.10f;
    ramp.tag = ColliderTag::Ramp;
    ramp.layer = LayerMask(CollisionLayer::World);
    constexpr int rampSegments = 144;
    ramp.centerLine.reserve(rampSegments + 1);
    for (int index = 0; index <= rampSegments; ++index)
    {
        ramp.centerLine.push_back(EvaluateWatatsumiRampPoint(
            index / static_cast<float>(rampSegments)));
    }
    watatsumiCollision_.AddPathSurface(std::move(ramp));

    // Named and tagged blockers are kept separate from visual meshes. This
    // makes interaction queries deterministic and lets a future navmesh build
    // consume Walkable/Ramp surfaces without treating glass or rails as floor.
    watatsumiCollision_.AddBox({
        L"Watatsumi_HeroTankGlass",
        {6.38f, 0.0f, -14.75f},
        {7.50f, 18.40f, 14.75f},
        ColliderTag::Glass,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddBox({
        L"Watatsumi_RearWall",
        {-28.10f, -0.2f, -24.0f},
        {-27.55f, 18.6f, 24.0f},
        ColliderTag::Solid,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddBox({
        L"Watatsumi_NorthWall",
        {-28.0f, -0.2f, 23.55f},
        {38.0f, 18.6f, 24.10f},
        ColliderTag::Solid,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddBox({
        L"Watatsumi_SouthWall",
        {-28.0f, -0.2f, -24.10f},
        {38.0f, 18.6f, -23.55f},
        ColliderTag::Solid,
        LayerMask(CollisionLayer::World)});

}

void AquariumScene::Update(
    const framework::FrameContext& frame,
    const framework::InputSystem& input)
{
    // 1フレームだけ反応する操作
    if (input.WasPressed(VK_SPACE))
    {
        settings_.paused = !settings_.paused;
    }
    if (input.WasPressed('R'))
    {
        ResetSettings();
    }
    if (input.WasPressed('1'))
    {
        SelectUnderwaterView();
    }
    if (input.WasPressed('2'))
    {
        SelectStageGlassView();
    }
    if (input.WasPressed('3'))
    {
        SelectAquariumGreyboxView();
    }
    if (input.WasPressed('4'))
    {
        SelectUnderwaterArchView();
    }
    if (input.WasPressed('5'))
    {
        SelectJellyfishReverseValidationView();
    }
    if (input.WasPressed('6'))
    {
        SelectWatatsumiTankView();
    }

    // 押下中に連続して反映するカメラ移動とライティング調整
    UpdateCamera(frame.deltaTime, input);
    UpdateLightingTuning(frame.deltaTime, input);

    if (!settings_.paused)
    {
        simulationTime_ += frame.deltaTime;
    }
}

void AquariumScene::Render(const framework::RenderContext& context)
{
    renderer_.Render(
        context.deviceContext,
        context.backBuffer,
        context.width,
        context.height,
        simulationTime_,
        context.deltaTime,
        settings_);
}

framework::SceneDiagnostics AquariumScene::GetDiagnostics() const
{
    framework::SceneDiagnostics diagnostics;
    diagnostics.viewLabel = settings_.watatsumiTankMode
        ? L"ROUTE 05: WATATSUMI HERO TANK"
        : (settings_.underwaterArchMode
        ? L"ROUTE 06: DESCENDING UNDERWATER ARCH"
        : (settings_.greyboxMode
        ? L"ROUTE 01-02: ENTRANCE + JELLYFISH"
        : (settings_.stageMode
            ? L"STAGE + GLASS VIEW"
            : (settings_.viewMode > 0.5f
                ? L"GLASS VIEW"
                : L"UNDERWATER VIEW"))));
    diagnostics.causticsStrength = settings_.causticsStrength;
    diagnostics.volumeStrength = settings_.volumeStrength;
    diagnostics.anisotropy = settings_.anisotropy;
    diagnostics.exposure = settings_.exposure;
    diagnostics.paused = settings_.paused;
    return diagnostics;
}

void AquariumScene::ResetSettings()
{
    const lighting::LocalLightingRig localLighting = settings_.localLighting;
    settings_ = {};
    settings_.localLighting = localLighting;
    playerCharacter_.activePath = -1;
    playerCharacter_.activeSegment = 0;
}

void AquariumScene::SelectUnderwaterView()
{
    settings_.viewMode = 0.0f;
    settings_.stageMode = false;
    settings_.greyboxMode = false;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
}

void AquariumScene::SelectStageGlassView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = false;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
    settings_.cameraPositionX = 0.0f;
    settings_.cameraPositionY = 0.10f;
    settings_.cameraPositionZ = -6.65f;
    settings_.cameraYaw = 0.0f;
    settings_.cameraPitch = -0.03f;
}

void AquariumScene::SelectAquariumGreyboxView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
    // Enter from the public doors and look through the lobby toward the
    // Jellyfish Theater. The generated route runs along +X.
    settings_.cameraPositionX = -16.2f;
    settings_.cameraPositionY = 0.10f;
    settings_.cameraPositionZ = 0.0f;
    settings_.cameraYaw = 1.57079633f;
    settings_.cameraPitch = -0.04f;
}

void AquariumScene::SelectUnderwaterArchView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = true;
    settings_.watatsumiTankMode = false;
    settings_.cameraPositionX = 0.85f;
    settings_.cameraPositionY = 1.58f;
    settings_.cameraPositionZ = 0.0f;
    settings_.cameraYaw = 1.57079633f;
    settings_.cameraPitch = -0.055f;
}

void AquariumScene::SelectJellyfishReverseValidationView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
    settings_.cameraPositionX = 6.0f;
    settings_.cameraPositionY = 0.10f;
    settings_.cameraPositionZ = -6.15f;
    settings_.cameraYaw = 0.0f;
    settings_.cameraPitch = -0.03f;
}

void AquariumScene::SelectWatatsumiTankView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = true;
    // Offset toward the ramp side so the tank remains the hero while the
    // lower-right wall portal is readable in the establishing shot.
    settings_.cameraPositionX = -9.5f;
    settings_.cameraPositionY = 1.62f;
    settings_.cameraPositionZ = -3.0f;
    settings_.cameraYaw = 1.505f;
    settings_.cameraPitch = -0.065f;
    playerCharacter_.eyePosition = {
        settings_.cameraPositionX,
        settings_.cameraPositionY,
        settings_.cameraPositionZ};
    playerCharacter_.activePath = -1;
    playerCharacter_.activeSegment = 0;
}

void AquariumScene::UpdateCamera(
    float deltaTime,
    const framework::InputSystem& input)
{
    constexpr float cameraSpeed = 0.75f;
    constexpr float movementSpeed = 2.35f;

    if (settings_.viewMode > 0.5f)
    {
        const float forwardX = std::sin(settings_.cameraYaw);
        const float forwardZ = std::cos(settings_.cameraYaw);
        const float rightX = std::cos(settings_.cameraYaw);
        const float rightZ = -std::sin(settings_.cameraYaw);
        float moveForward = 0.0f;
        float moveRight = 0.0f;
        if (input.IsDown('W'))
        {
            moveForward += 1.0f;
        }
        if (input.IsDown('S'))
        {
            moveForward -= 1.0f;
        }
        if (input.IsDown('D'))
        {
            moveRight += 1.0f;
        }
        if (input.IsDown('A'))
        {
            moveRight -= 1.0f;
        }

        const float moveLength = std::sqrt(
            moveForward * moveForward + moveRight * moveRight);
        float requestedMoveX = 0.0f;
        float requestedMoveZ = 0.0f;
        if (moveLength > 0.0f)
        {
            moveForward /= moveLength;
            moveRight /= moveLength;
            requestedMoveX =
                (forwardX * moveForward + rightX * moveRight) *
                movementSpeed * deltaTime;
            requestedMoveZ =
                (forwardZ * moveForward + rightZ * moveRight) *
                movementSpeed * deltaTime;
            if (!settings_.watatsumiTankMode)
            {
                settings_.cameraPositionX += requestedMoveX;
                settings_.cameraPositionZ += requestedMoveZ;
            }
        }
        if (!settings_.underwaterArchMode &&
            !settings_.watatsumiTankMode && input.IsDown('E'))
        {
            settings_.cameraPositionY += movementSpeed * 0.65f * deltaTime;
        }
        if (!settings_.underwaterArchMode &&
            !settings_.watatsumiTankMode && input.IsDown('Q'))
        {
            settings_.cameraPositionY -= movementSpeed * 0.65f * deltaTime;
        }

        if (settings_.underwaterArchMode)
        {
            settings_.cameraPositionX =
                std::clamp(settings_.cameraPositionX, 0.55f, 47.45f);
            settings_.cameraPositionZ =
                std::clamp(settings_.cameraPositionZ, -2.72f, 2.72f);
            const float routeT = std::clamp(
                settings_.cameraPositionX / 48.0f,
                0.0f,
                1.0f);
            const float smoothDescent =
                routeT * routeT * (3.0f - 2.0f * routeT);
            settings_.cameraPositionY =
                1.58f - 4.7f * smoothDescent;
        }
        else if (settings_.watatsumiTankMode)
        {
            watatsumiCollision_.MoveCharacter(
                playerCharacter_,
                {requestedMoveX, 0.0f, requestedMoveZ},
                playerCapsule_);
            settings_.cameraPositionX =
                playerCharacter_.eyePosition.x;
            settings_.cameraPositionY =
                playerCharacter_.eyePosition.y;
            settings_.cameraPositionZ =
                playerCharacter_.eyePosition.z;
        }
        else if (settings_.stageMode)
        {
            settings_.cameraPositionX =
                std::clamp(settings_.cameraPositionX, -25.0f, 65.0f);
            settings_.cameraPositionY =
                std::clamp(settings_.cameraPositionY, -1.75f, 6.0f);
            settings_.cameraPositionZ =
                std::clamp(settings_.cameraPositionZ, -25.0f, 85.0f);
        }
        else
        {
            settings_.cameraPositionX =
                std::clamp(settings_.cameraPositionX, -4.75f, 4.75f);
            settings_.cameraPositionY =
                std::clamp(settings_.cameraPositionY, -1.75f, 2.25f);
            settings_.cameraPositionZ =
                std::clamp(settings_.cameraPositionZ, -10.0f, -5.35f);
        }

        if (input.IsDown(VK_LEFT))
        {
            settings_.cameraYaw -= cameraSpeed * deltaTime;
        }
        if (input.IsDown(VK_RIGHT))
        {
            settings_.cameraYaw += cameraSpeed * deltaTime;
        }
        if (input.IsDown(VK_UP))
        {
            settings_.cameraPitch += cameraSpeed * deltaTime;
        }
        if (input.IsDown(VK_DOWN))
        {
            settings_.cameraPitch -= cameraSpeed * deltaTime;
        }
    }
    else
    {
        if (input.IsDown('A'))
        {
            settings_.cameraYaw -= cameraSpeed * deltaTime;
        }
        if (input.IsDown('D'))
        {
            settings_.cameraYaw += cameraSpeed * deltaTime;
        }
        if (input.IsDown('W'))
        {
            settings_.cameraPitch += cameraSpeed * deltaTime;
        }
        if (input.IsDown('S'))
        {
            settings_.cameraPitch -= cameraSpeed * deltaTime;
        }
    }

    settings_.cameraPitch = std::clamp(settings_.cameraPitch, -0.45f, 0.45f);
    settings_.cameraYaw = settings_.stageMode
        ? std::remainder(settings_.cameraYaw, 6.28318531f)
        : std::clamp(settings_.cameraYaw, -0.7f, 0.7f);
}

void AquariumScene::UpdateLightingTuning(
    float deltaTime,
    const framework::InputSystem& input)
{
    constexpr float tuningSpeed = 0.85f;

    if (input.IsDown('J'))
    {
        settings_.causticsStrength =
            std::max(0.0f, settings_.causticsStrength - tuningSpeed * deltaTime);
    }
    if (input.IsDown('L'))
    {
        settings_.causticsStrength =
            std::min(3.0f, settings_.causticsStrength + tuningSpeed * deltaTime);
    }
    if (input.IsDown('I'))
    {
        settings_.volumeStrength =
            std::min(3.0f, settings_.volumeStrength + tuningSpeed * deltaTime);
    }
    if (input.IsDown('K'))
    {
        settings_.volumeStrength =
            std::max(0.0f, settings_.volumeStrength - tuningSpeed * deltaTime);
    }
    if (input.IsDown('U'))
    {
        settings_.exposure =
            std::max(0.35f, settings_.exposure - tuningSpeed * deltaTime);
    }
    if (input.IsDown('O'))
    {
        settings_.exposure =
            std::min(3.0f, settings_.exposure + tuningSpeed * deltaTime);
    }
    if (input.IsDown('N'))
    {
        settings_.anisotropy =
            std::max(-0.2f, settings_.anisotropy - tuningSpeed * 0.45f * deltaTime);
    }
    if (input.IsDown('M'))
    {
        settings_.anisotropy =
            std::min(0.9f, settings_.anisotropy + tuningSpeed * 0.45f * deltaTime);
    }
}
