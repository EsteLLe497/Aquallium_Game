/*==================================================================================================

   [PlayerManager.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/09/01
----------------------------------------------------------------------------------------------------
   プレイヤーの移動、視点操作、選択入力を一元管理する
===================================================================================================*/
#pragma once

#include "../physics/CollisionWorld.h"

#include <DirectXMath.h>

namespace framework
{
class InputSystem;
}

namespace player
{
struct SelectionRay
{
    DirectX::XMFLOAT3 origin{};
    DirectX::XMFLOAT3 direction{0.0f, 0.0f, 1.0f};
};

class PlayerManager
{
public:
    void Reset(
        const DirectX::XMFLOAT3& eyePosition,
        float yaw,
        float pitch);

    void Update(
        float deltaTime,
        const framework::InputSystem& input,
        const physics::CollisionWorld* collisionWorld,
        bool allowVerticalMovement);

    [[nodiscard]] const DirectX::XMFLOAT3& EyePosition() const noexcept;
    [[nodiscard]] float Yaw() const noexcept;
    [[nodiscard]] float Pitch() const noexcept;
    [[nodiscard]] const physics::CharacterCapsule& Capsule() const noexcept;
    [[nodiscard]] bool SelectionRequested() const noexcept;
    [[nodiscard]] SelectionRay GetSelectionRay() const noexcept;

private:
    void UpdateLook(
        float deltaTime,
        const framework::InputSystem& input);
    [[nodiscard]] DirectX::XMFLOAT3 CalculateMovement(
        float deltaTime,
        const framework::InputSystem& input,
        bool allowVerticalMovement) const;

    physics::CharacterCapsule capsule_{};
    physics::CharacterState character_{};
    float yaw_ = 0.0f;
    float pitch_ = -0.03f;
    bool selectionRequested_ = false;
};
}
