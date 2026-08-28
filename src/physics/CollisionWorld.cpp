#include "CollisionWorld.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace physics
{
namespace
{
constexpr std::uint32_t kPlayerWorldMask =
    LayerMask(CollisionLayer::World);

float LengthSquared2D(float x, float z)
{
    return x * x + z * z;
}
}

void CollisionWorld::Clear()
{
    boxes_.clear();
    walkableRects_.clear();
    paths_.clear();
}

void CollisionWorld::AddBox(BoxCollider collider)
{
    boxes_.push_back(std::move(collider));
}

void CollisionWorld::AddWalkableRect(WalkableRect surface)
{
    walkableRects_.push_back(std::move(surface));
}

void CollisionWorld::AddPathSurface(PathSurface surface)
{
    if (surface.centerLine.size() >= 2)
    {
        paths_.push_back(std::move(surface));
    }
}

const std::vector<BoxCollider>& CollisionWorld::Boxes() const noexcept
{
    return boxes_;
}

const std::vector<WalkableRect>& CollisionWorld::WalkableRects() const noexcept
{
    return walkableRects_;
}

const std::vector<PathSurface>& CollisionWorld::Paths() const noexcept
{
    return paths_;
}

bool CollisionWorld::HasTag(ColliderTag tag) const noexcept
{
    return std::ranges::any_of(boxes_, [tag](const BoxCollider& collider)
    {
        return collider.tag == tag;
    }) || std::ranges::any_of(
        walkableRects_, [tag](const WalkableRect& collider)
    {
        return collider.tag == tag;
    }) || std::ranges::any_of(paths_, [tag](const PathSurface& collider)
    {
        return collider.tag == tag;
    });
}

bool CollisionWorld::IsBlocked(
    const DirectX::XMFLOAT3& eyePosition,
    const CharacterCapsule& capsule) const
{
    const float feet = eyePosition.y - capsule.eyeHeight;
    const float head = feet + capsule.height;
    for (const BoxCollider& box : boxes_)
    {
        if ((box.layer & kPlayerWorldMask) == 0 ||
            box.tag == ColliderTag::Trigger ||
            box.tag == ColliderTag::Water)
        {
            continue;
        }
        if (head <= box.minimum.y || feet >= box.maximum.y)
        {
            continue;
        }
        const float nearestX = std::clamp(
            eyePosition.x, box.minimum.x, box.maximum.x);
        const float nearestZ = std::clamp(
            eyePosition.z, box.minimum.z, box.maximum.z);
        if (LengthSquared2D(
                eyePosition.x - nearestX,
                eyePosition.z - nearestZ) <
            capsule.radius * capsule.radius)
        {
            return true;
        }
    }
    return false;
}

bool CollisionWorld::TryFlatPosition(
    const DirectX::XMFLOAT3& desiredEye,
    float currentFootY,
    const CharacterCapsule& capsule,
    DirectX::XMFLOAT3& resolvedEye) const
{
    const WalkableRect* bestSurface = nullptr;
    float bestHeightDifference = 1.0e30f;
    for (const WalkableRect& surface : walkableRects_)
    {
        if ((surface.layer & kPlayerWorldMask) == 0)
        {
            continue;
        }
        // Walkable rectangles describe authored floor coverage, not walls.
        // Do not inset every rectangle by the capsule radius: adjacent rooms
        // would otherwise acquire an invisible gap at each doorway seam.
        // Boundary clearance is supplied by the tagged wall/rail colliders.
        const float minimumX = surface.minimumX;
        const float maximumX = surface.maximumX;
        const float minimumZ = surface.minimumZ;
        const float maximumZ = surface.maximumZ;
        if (desiredEye.x < minimumX || desiredEye.x > maximumX ||
            desiredEye.z < minimumZ || desiredEye.z > maximumZ)
        {
            continue;
        }
        const float heightDifference = std::abs(
            surface.floorY - currentFootY);
        if (heightDifference <= capsule.stepHeight &&
            heightDifference < bestHeightDifference)
        {
            bestSurface = &surface;
            bestHeightDifference = heightDifference;
        }
    }
    if (bestSurface == nullptr)
    {
        return false;
    }

    resolvedEye = desiredEye;
    resolvedEye.y = bestSurface->floorY + capsule.eyeHeight;
    return !IsBlocked(resolvedEye, capsule);
}

CollisionWorld::PathQuery CollisionWorld::QueryPath(
    const PathSurface& path,
    float x,
    float z,
    std::size_t firstSegment,
    std::size_t lastSegment) const
{
    PathQuery result;
    if (path.centerLine.size() < 2)
    {
        return result;
    }
    const std::size_t maximumSegment = path.centerLine.size() - 2;
    firstSegment = std::min(firstSegment, maximumSegment);
    lastSegment = std::min(lastSegment, maximumSegment);
    for (std::size_t index = firstSegment; index <= lastSegment; ++index)
    {
        const DirectX::XMFLOAT3& first = path.centerLine[index];
        const DirectX::XMFLOAT3& second = path.centerLine[index + 1];
        const float segmentX = second.x - first.x;
        const float segmentZ = second.z - first.z;
        const float segmentLengthSquared = std::max(
            LengthSquared2D(segmentX, segmentZ), 1.0e-8f);
        const float alpha = std::clamp(
            ((x - first.x) * segmentX + (z - first.z) * segmentZ) /
                segmentLengthSquared,
            0.0f,
            1.0f);
        const DirectX::XMFLOAT3 point{
            first.x + segmentX * alpha,
            first.y + (second.y - first.y) * alpha,
            first.z + segmentZ * alpha};
        const float distanceSquared = LengthSquared2D(
            x - point.x,
            z - point.z);
        if (distanceSquared < result.distanceSquared)
        {
            result.distanceSquared = distanceSquared;
            result.point = point;
            result.segment = index;
        }
    }
    return result;
}

void CollisionWorld::MoveCharacterStep(
    CharacterState& character,
    const DirectX::XMFLOAT3& displacement,
    const CharacterCapsule& capsule) const
{
    const DirectX::XMFLOAT3 previous = character.eyePosition;
    const DirectX::XMFLOAT3 desired{
        previous.x + displacement.x,
        previous.y + displacement.y,
        previous.z + displacement.z};
    const float currentFootY = previous.y - capsule.eyeHeight;

    if (character.activePath >= 0 &&
        character.activePath < static_cast<int>(paths_.size()))
    {
        const PathSurface& path = paths_[character.activePath];
        const std::size_t maximumSegment = path.centerLine.size() - 2;
        const std::size_t firstSegment = character.activeSegment > 8
            ? character.activeSegment - 8
            : 0;
        const std::size_t lastSegment = std::min(
            character.activeSegment + 8, maximumSegment);
        PathQuery query = QueryPath(
            path, desired.x, desired.z, firstSegment, lastSegment);

        // Hand off only after crossing an endpoint plane. Merely being near a
        // landing is insufficient because the lower floor overlaps the first
        // metres of the rising ramp in plan view.
        const DirectX::XMFLOAT3& pathStart = path.centerLine.front();
        const DirectX::XMFLOAT3& pathStartNext = path.centerLine[1];
        const DirectX::XMFLOAT3& pathEnd = path.centerLine.back();
        const DirectX::XMFLOAT3& pathEndPrevious =
            path.centerLine[path.centerLine.size() - 2];
        const float startPlane =
            (desired.x - pathStart.x) *
                (pathStartNext.x - pathStart.x) +
            (desired.z - pathStart.z) *
                (pathStartNext.z - pathStart.z);
        const float endPlane =
            (desired.x - pathEnd.x) *
                (pathEnd.x - pathEndPrevious.x) +
            (desired.z - pathEnd.z) *
                (pathEnd.z - pathEndPrevious.z);
        const bool crossedFirstLanding =
            query.segment <= 1 && startPlane < 0.0f;
        const bool crossedLastLanding =
            query.segment + 1 >= maximumSegment && endPlane > 0.0f;
        DirectX::XMFLOAT3 flatPosition{};
        if ((crossedFirstLanding || crossedLastLanding) &&
            TryFlatPosition(
                desired,
                query.point.y,
                capsule,
                flatPosition))
        {
            character.eyePosition = flatPosition;
            character.activePath = -1;
            character.activeSegment = 0;
            return;
        }

        const float offsetX = desired.x - query.point.x;
        const float offsetZ = desired.z - query.point.z;
        const float offsetLength = std::sqrt(
            LengthSquared2D(offsetX, offsetZ));
        const float allowedOffset = std::max(
            path.halfWidth - capsule.radius, 0.05f);
        const float scale = offsetLength > allowedOffset
            ? allowedOffset / std::max(offsetLength, 1.0e-6f)
            : 1.0f;
        character.eyePosition = {
            query.point.x + offsetX * scale,
            query.point.y + capsule.eyeHeight,
            query.point.z + offsetZ * scale};
        character.activeSegment = query.segment;
        return;
    }

    // Enter a path only through one of its endpoint neighbourhoods. A global
    // nearest-point query would confuse vertically overlapping helix turns.
    for (std::size_t pathIndex = 0; pathIndex < paths_.size(); ++pathIndex)
    {
        const PathSurface& path = paths_[pathIndex];
        const std::size_t maximumSegment = path.centerLine.size() - 2;
        constexpr std::size_t landingSegmentCount = 12;
        PathQuery first = QueryPath(
            path,
            desired.x,
            desired.z,
            0,
            std::min(landingSegmentCount, maximumSegment));
        PathQuery last = QueryPath(
            path,
            desired.x,
            desired.z,
            maximumSegment > landingSegmentCount
                ? maximumSegment - landingSegmentCount
                : 0,
            maximumSegment);
        PathQuery query = first.distanceSquared <= last.distanceSquared
            ? first
            : last;
        const float allowedOffset = path.halfWidth - capsule.radius;
        if (query.distanceSquared <= allowedOffset * allowedOffset &&
            std::abs(query.point.y - currentFootY) <= capsule.stepHeight)
        {
            character.activePath = static_cast<int>(pathIndex);
            character.activeSegment = query.segment;
            character.eyePosition = {
                desired.x,
                query.point.y + capsule.eyeHeight,
                desired.z};
            return;
        }
    }

    DirectX::XMFLOAT3 resolved{};
    if (TryFlatPosition(desired, currentFootY, capsule, resolved))
    {
        character.eyePosition = resolved;
        return;
    }

    // Axis-separated retries provide stable wall sliding for a capsule while
    // preserving the previous valid position if both directions are blocked.
    const DirectX::XMFLOAT3 xOnly{
        desired.x, previous.y, previous.z};
    if (TryFlatPosition(xOnly, currentFootY, capsule, resolved))
    {
        character.eyePosition = resolved;
    }
    const DirectX::XMFLOAT3 zOnly{
        character.eyePosition.x,
        character.eyePosition.y,
        desired.z};
    const float updatedFootY = character.eyePosition.y - capsule.eyeHeight;
    if (TryFlatPosition(zOnly, updatedFootY, capsule, resolved))
    {
        character.eyePosition = resolved;
    }
}

void CollisionWorld::MoveCharacter(
    CharacterState& character,
    const DirectX::XMFLOAT3& displacement,
    const CharacterCapsule& capsule) const
{
    const float horizontalDistance = std::sqrt(
        LengthSquared2D(displacement.x, displacement.z));
    const int stepCount = std::clamp(
        static_cast<int>(std::ceil(
            horizontalDistance / std::max(capsule.radius * 0.45f, 0.05f))),
        1,
        16);
    const DirectX::XMFLOAT3 step{
        displacement.x / stepCount,
        displacement.y / stepCount,
        displacement.z / stepCount};
    for (int index = 0; index < stepCount; ++index)
    {
        MoveCharacterStep(character, step, capsule);
    }
}
}
