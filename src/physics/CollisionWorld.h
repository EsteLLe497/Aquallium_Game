#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>

namespace physics
{
enum class ColliderTag : std::uint8_t
{
    Untagged,
    Walkable,
    Solid,
    Ramp,
    Rail,
    Glass,
    Water,
    Trigger
};

enum class CollisionLayer : std::uint32_t
{
    None = 0,
    Player = 1u << 0,
    World = 1u << 1,
    Water = 1u << 2,
    Trigger = 1u << 3
};

[[nodiscard]] constexpr std::uint32_t LayerMask(CollisionLayer layer)
{
    return static_cast<std::uint32_t>(layer);
}

struct CharacterCapsule
{
    // Tall 195 cm adult proportions. Eye height is measured from the floor so
    // camera and collision remain one physical character.
    float radius = 0.32f;
    float height = 1.95f;
    // A high but still anatomically plausible eye line for a 195 cm adult.
    float eyeHeight = 1.89f;
    float stepHeight = 0.32f;
};

struct CharacterState
{
    DirectX::XMFLOAT3 eyePosition{};
    int activePath = -1;
    std::size_t activeSegment = 0;
};

struct BoxCollider
{
    std::wstring name;
    DirectX::XMFLOAT3 minimum{};
    DirectX::XMFLOAT3 maximum{};
    ColliderTag tag = ColliderTag::Solid;
    std::uint32_t layer = LayerMask(CollisionLayer::World);
};

struct WalkableRect
{
    std::wstring name;
    float minimumX = 0.0f;
    float maximumX = 0.0f;
    float minimumZ = 0.0f;
    float maximumZ = 0.0f;
    float floorY = 0.0f;
    ColliderTag tag = ColliderTag::Walkable;
    std::uint32_t layer = LayerMask(CollisionLayer::World);
};

struct PathSurface
{
    std::wstring name;
    std::vector<DirectX::XMFLOAT3> centerLine;
    float halfWidth = 1.0f;
    ColliderTag tag = ColliderTag::Ramp;
    std::uint32_t layer = LayerMask(CollisionLayer::World);
};

class CollisionWorld
{
public:
    void Clear();
    void AddBox(BoxCollider collider);
    void AddWalkableRect(WalkableRect surface);
    void AddPathSurface(PathSurface surface);

    void MoveCharacter(
        CharacterState& character,
        const DirectX::XMFLOAT3& displacement,
        const CharacterCapsule& capsule) const;

    [[nodiscard]] const std::vector<BoxCollider>& Boxes() const noexcept;
    [[nodiscard]] const std::vector<WalkableRect>& WalkableRects() const noexcept;
    [[nodiscard]] const std::vector<PathSurface>& Paths() const noexcept;
    [[nodiscard]] bool HasTag(ColliderTag tag) const noexcept;

private:
    struct PathQuery
    {
        float distanceSquared = 1.0e30f;
        DirectX::XMFLOAT3 point{};
        std::size_t segment = 0;
    };

    [[nodiscard]] bool TryFlatPosition(
        const DirectX::XMFLOAT3& desiredEye,
        float currentFootY,
        const CharacterCapsule& capsule,
        DirectX::XMFLOAT3& resolvedEye) const;
    [[nodiscard]] bool IsBlocked(
        const DirectX::XMFLOAT3& eyePosition,
        const CharacterCapsule& capsule) const;
    [[nodiscard]] PathQuery QueryPath(
        const PathSurface& path,
        float x,
        float z,
        std::size_t firstSegment,
        std::size_t lastSegment) const;
    void MoveCharacterStep(
        CharacterState& character,
        const DirectX::XMFLOAT3& displacement,
        const CharacterCapsule& capsule) const;

    std::vector<BoxCollider> boxes_;
    std::vector<WalkableRect> walkableRects_;
    std::vector<PathSurface> paths_;
};
}
