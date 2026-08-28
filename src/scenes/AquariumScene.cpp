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
#include <windows.h>

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
    watatsumiRampTracking_ = false;
    watatsumiRampT_ = 0.0f;
    watatsumiRampMaximumT_ = 0.0f;
}

void AquariumScene::SelectUnderwaterView()
{
    watatsumiRampTracking_ = false;
    settings_.viewMode = 0.0f;
    settings_.stageMode = false;
    settings_.greyboxMode = false;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
}

void AquariumScene::SelectStageGlassView()
{
    watatsumiRampTracking_ = false;
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
    watatsumiRampTracking_ = false;
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
    watatsumiRampTracking_ = false;
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
    watatsumiRampTracking_ = false;
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
    watatsumiRampTracking_ = false;
    watatsumiRampT_ = 0.0f;
    watatsumiRampMaximumT_ = 0.0f;
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
        if (moveLength > 0.0f)
        {
            moveForward /= moveLength;
            moveRight /= moveLength;
            settings_.cameraPositionX +=
                (forwardX * moveForward + rightX * moveRight) *
                movementSpeed * deltaTime;
            settings_.cameraPositionZ +=
                (forwardZ * moveForward + rightZ * moveRight) *
                movementSpeed * deltaTime;
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
            settings_.cameraPositionX =
                std::clamp(settings_.cameraPositionX, -27.0f, 37.0f);
            settings_.cameraPositionZ =
                std::clamp(settings_.cameraPositionZ, -23.2f, 23.2f);

            // The Shikoku Aquarium reference enters on the tank's right,
            // disappears inside the wall, follows the rear perimeter and
            // re-enters the atrium at the upper viewing level. Keep camera
            // height on the same authored centre line used by the GLB.
            auto evaluateRampPoint = [](float t)
            {
                t = std::clamp(t, 0.0f, 1.0f);
                const float y = 0.18f + 12.22f * t;
                auto line = [y](
                    float ax, float az,
                    float bx, float bz,
                    float u)
                {
                    return DirectX::XMFLOAT3{
                        ax + (bx - ax) * u,
                        y,
                        az + (bz - az) * u
                    };
                };
                if (t < 0.06f)
                {
                    return line(-0.5f, -17.8f, 5.8f, -17.8f,
                        t / 0.06f);
                }
                if (t < 0.20f)
                {
                    return line(5.8f, -17.8f, 17.8f, -17.8f,
                        (t - 0.06f) / 0.14f);
                }
                if (t < 0.80f)
                {
                    const float u = (t - 0.20f) / 0.60f;
                    const float angle =
                        -DirectX::XM_PIDIV2 + DirectX::XM_PI * u;
                    return DirectX::XMFLOAT3{
                        17.8f + std::cos(angle) * 17.8f,
                        y,
                        std::sin(angle) * 17.8f
                    };
                }
                if (t < 0.97f)
                {
                    return line(17.8f, 17.8f, 5.8f, 17.8f,
                        (t - 0.80f) / 0.17f);
                }
                return line(5.8f, 17.8f, 3.6f, 17.8f,
                    (t - 0.97f) / 0.03f);
            };

            constexpr float eyeHeight = 1.62f;
            constexpr float rampHalfWidth = 2.10f;
            constexpr float playerRadius = 0.34f;
            constexpr float allowedCenterOffset =
                rampHalfWidth - playerRadius;

            struct RampQuery
            {
                float score = 100000.0f;
                float distanceSquared = 100000.0f;
                float t = 0.0f;
                DirectX::XMFLOAT3 point{};
            };
            auto queryRamp = [&evaluateRampPoint](
                float positionX,
                float positionZ,
                float centerT,
                float halfRange,
                int sampleCount)
            {
                RampQuery result;
                const float minimumT = std::clamp(
                    centerT - halfRange, 0.0f, 1.0f);
                const float maximumT = std::clamp(
                    centerT + halfRange, 0.0f, 1.0f);
                for (int sampleIndex = 0;
                     sampleIndex <= sampleCount;
                     ++sampleIndex)
                {
                    const float sampleAlpha =
                        sampleIndex / static_cast<float>(sampleCount);
                    const float t = std::lerp(
                        minimumT, maximumT, sampleAlpha);
                    const DirectX::XMFLOAT3 point = evaluateRampPoint(t);
                    const float dx = positionX - point.x;
                    const float dz = positionZ - point.z;
                    const float distanceSquared = dx * dx + dz * dz;
                    if (distanceSquared < result.score)
                    {
                        result.score = distanceSquared;
                        result.distanceSquared = distanceSquared;
                        result.t = t;
                        result.point = point;
                    }
                }
                return result;
            };

            // Stateful swept-corridor collision. Once the player enters, the
            // previous route parameter disambiguates overlapping helix turns.
            RampQuery proposedRamp;
            if (watatsumiRampTracking_)
            {
                proposedRamp = queryRamp(
                    settings_.cameraPositionX,
                    settings_.cameraPositionZ,
                    watatsumiRampT_,
                    0.035f,
                    72);
            }
            else
            {
                proposedRamp = queryRamp(
                    settings_.cameraPositionX,
                    settings_.cameraPositionZ,
                    0.5f,
                    0.5f,
                    384);
                const float currentFloor =
                    settings_.cameraPositionY - eyeHeight;
                const bool lowerEntry =
                    proposedRamp.t < 0.075f &&
                    currentFloor < 0.85f;
                const bool upperEntry =
                    proposedRamp.t > 0.955f &&
                    currentFloor > 11.55f;
                watatsumiRampTracking_ =
                    (lowerEntry || upperEntry) &&
                    proposedRamp.distanceSquared < 2.15f * 2.15f;
                if (watatsumiRampTracking_)
                {
                    watatsumiRampT_ = proposedRamp.t;
                    watatsumiRampMaximumT_ = proposedRamp.t;
                }
            }

            if (watatsumiRampTracking_)
            {
                // Preserve route continuity even where the helix overlaps in
                // XZ. Small backwards movement remains possible, but a noisy
                // nearest-point switch can never jump to a distant turn.
                proposedRamp.t = std::clamp(
                    proposedRamp.t,
                    watatsumiRampT_ - 0.025f,
                    watatsumiRampT_ + 0.025f);
                watatsumiRampT_ = proposedRamp.t;
                watatsumiRampMaximumT_ = std::max(
                    watatsumiRampMaximumT_, watatsumiRampT_);
                proposedRamp.point = evaluateRampPoint(watatsumiRampT_);

                const float offsetX =
                    settings_.cameraPositionX - proposedRamp.point.x;
                const float offsetZ =
                    settings_.cameraPositionZ - proposedRamp.point.z;
                const float offsetLength = std::sqrt(
                    offsetX * offsetX + offsetZ * offsetZ);
                if (offsetLength > allowedCenterOffset)
                {
                    const float correctionScale =
                        allowedCenterOffset /
                        std::max(offsetLength, 0.0001f);
                    settings_.cameraPositionX =
                        proposedRamp.point.x + offsetX * correctionScale;
                    settings_.cameraPositionZ =
                        proposedRamp.point.z + offsetZ * correctionScale;
                    proposedRamp.distanceSquared =
                        allowedCenterOffset * allowedCenterOffset;
                }
                else
                {
                    proposedRamp.distanceSquared =
                        offsetLength * offsetLength;
                }

                const bool exitedAtLowerEnd =
                    watatsumiRampT_ < 0.012f &&
                    watatsumiRampMaximumT_ > 0.04f;
                const bool exitedAtUpperEnd =
                    watatsumiRampT_ > 0.988f;
                if (exitedAtLowerEnd || exitedAtUpperEnd)
                {
                    watatsumiRampTracking_ = false;
                }
            }

            const float nearestRampT = proposedRamp.t;
            float targetCameraY = 1.62f;
            if (watatsumiRampTracking_)
            {
                const DirectX::XMFLOAT3 rampPoint =
                    evaluateRampPoint(nearestRampT);
                targetCameraY = rampPoint.y + eyeHeight;
            }
            else if (settings_.cameraPositionY > 9.0f &&
                     ((settings_.cameraPositionX > -23.0f &&
                       settings_.cameraPositionX < 4.5f &&
                       std::abs(settings_.cameraPositionZ) > 15.3f &&
                       std::abs(settings_.cameraPositionZ) < 20.4f) ||
                      (settings_.cameraPositionX > -23.5f &&
                       settings_.cameraPositionX < -18.5f &&
                       std::abs(settings_.cameraPositionZ) < 20.4f)))
            {
                targetCameraY = 13.90f;
            }

            // Critically damped frame-rate-independent convergence removes
            // the stair-step caused by a fixed per-frame vertical clamp.
            const float verticalBlend =
                1.0f - std::exp(-10.0f * deltaTime);
            settings_.cameraPositionY = std::lerp(
                settings_.cameraPositionY,
                targetCameraY,
                verticalBlend);
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
