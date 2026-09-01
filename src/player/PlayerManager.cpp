/*==================================================================================================

   [PlayerManager.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/09/01
----------------------------------------------------------------------------------------------------
   プレイヤーの移動、視点操作、選択入力を一元管理する
===================================================================================================*/
#include "PlayerManager.h"

#include "../framework/input.h"

#include <algorithm>
#include <cmath>
#include <windows.h>

namespace player
{
namespace
{
constexpr float kWalkSpeed = 2.35f;
constexpr float kSprintMultiplier = 1.85f;
constexpr float kVerticalSpeedScale = 0.65f;
constexpr float kMouseSensitivity = 0.0022f;
constexpr float kKeyboardLookSpeed = 0.75f;
constexpr float kPitchLimit = 1.35f;
constexpr float kTwoPi = 6.28318531f;
}

void PlayerManager::Reset(
    const DirectX::XMFLOAT3& eyePosition,
    float yaw,
    float pitch)
{
    character_.eyePosition = eyePosition;
    character_.activePath = -1;
    character_.activeSegment = 0;
    yaw_ = yaw;
    pitch_ = std::clamp(pitch, -kPitchLimit, kPitchLimit);
    selectionRequested_ = false;
}

void PlayerManager::Update(
    float deltaTime,
    const framework::InputSystem& input,
    const physics::CollisionWorld* collisionWorld,
    bool allowVerticalMovement)
{
    UpdateLook(deltaTime, input);
    selectionRequested_ = input.WasPressed(VK_LBUTTON);

    const DirectX::XMFLOAT3 movement = CalculateMovement(
        deltaTime, input, allowVerticalMovement);
    if (collisionWorld != nullptr)
    {
        collisionWorld->MoveCharacter(character_, movement, capsule_);
    }
    else
    {
        character_.eyePosition.x += movement.x;
        character_.eyePosition.y += movement.y;
        character_.eyePosition.z += movement.z;
    }
}

const DirectX::XMFLOAT3& PlayerManager::EyePosition() const noexcept
{
    return character_.eyePosition;
}

float PlayerManager::Yaw() const noexcept
{
    return yaw_;
}

float PlayerManager::Pitch() const noexcept
{
    return pitch_;
}

const physics::CharacterCapsule& PlayerManager::Capsule() const noexcept
{
    return capsule_;
}

bool PlayerManager::SelectionRequested() const noexcept
{
    return selectionRequested_;
}

SelectionRay PlayerManager::GetSelectionRay() const noexcept
{
    const float horizontal = std::cos(pitch_);
    return {
        character_.eyePosition,
        {
            std::sin(yaw_) * horizontal,
            std::sin(pitch_),
            std::cos(yaw_) * horizontal
        }
    };
}

void PlayerManager::UpdateLook(
    float deltaTime,
    const framework::InputSystem& input)
{
    yaw_ += input.MouseDeltaX() * kMouseSensitivity;
    pitch_ -= input.MouseDeltaY() * kMouseSensitivity;

    if (input.IsDown(VK_LEFT))
    {
        yaw_ -= kKeyboardLookSpeed * deltaTime;
    }
    if (input.IsDown(VK_RIGHT))
    {
        yaw_ += kKeyboardLookSpeed * deltaTime;
    }
    if (input.IsDown(VK_UP))
    {
        pitch_ += kKeyboardLookSpeed * deltaTime;
    }
    if (input.IsDown(VK_DOWN))
    {
        pitch_ -= kKeyboardLookSpeed * deltaTime;
    }

    yaw_ = std::remainder(yaw_, kTwoPi);
    pitch_ = std::clamp(pitch_, -kPitchLimit, kPitchLimit);
}

DirectX::XMFLOAT3 PlayerManager::CalculateMovement(
    float deltaTime,
    const framework::InputSystem& input,
    bool allowVerticalMovement) const
{
    float forwardInput = 0.0f;
    float rightInput = 0.0f;
    forwardInput += input.IsDown('W') ? 1.0f : 0.0f;
    forwardInput -= input.IsDown('S') ? 1.0f : 0.0f;
    rightInput += input.IsDown('D') ? 1.0f : 0.0f;
    rightInput -= input.IsDown('A') ? 1.0f : 0.0f;

    const float inputLength = std::sqrt(
        forwardInput * forwardInput + rightInput * rightInput);
    if (inputLength > 1.0f)
    {
        forwardInput /= inputLength;
        rightInput /= inputLength;
    }

    const float speed = kWalkSpeed *
        (input.IsDown(VK_SHIFT) ? kSprintMultiplier : 1.0f);
    const float forwardX = std::sin(yaw_);
    const float forwardZ = std::cos(yaw_);
    const float rightX = std::cos(yaw_);
    const float rightZ = -std::sin(yaw_);

    DirectX::XMFLOAT3 movement{
        (forwardX * forwardInput + rightX * rightInput) *
            speed * deltaTime,
        0.0f,
        (forwardZ * forwardInput + rightZ * rightInput) *
            speed * deltaTime};
    if (allowVerticalMovement)
    {
        movement.y += input.IsDown('E')
            ? speed * kVerticalSpeedScale * deltaTime : 0.0f;
        movement.y -= input.IsDown('Q')
            ? speed * kVerticalSpeedScale * deltaTime : 0.0f;
    }
    return movement;
}
}
