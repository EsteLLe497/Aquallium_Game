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
constexpr float kStageFloorOffset = -2.25f;
constexpr float kWatatsumiUpperFloorY = 12.28f;
constexpr float kWatatsumiRampRadius = 17.8f;
constexpr float kWatatsumiRampStraightLength = 12.0f;

float EvaluateWatatsumiRampHeight(float t)
{
    float travel = 0.0f;
    const float arcLength = DirectX::XM_PI * kWatatsumiRampRadius;
    if (t > 0.06f && t < 0.20f)
    {
        travel = kWatatsumiRampStraightLength *
            ((t - 0.06f) / 0.14f);
    }
    else if (t >= 0.20f && t < 0.80f)
    {
        travel = kWatatsumiRampStraightLength +
            arcLength * ((t - 0.20f) / 0.60f);
    }
    else if (t >= 0.80f && t < 0.97f)
    {
        travel = kWatatsumiRampStraightLength + arcLength +
            kWatatsumiRampStraightLength *
                ((t - 0.80f) / 0.17f);
    }
    else if (t >= 0.97f)
    {
        travel = kWatatsumiRampStraightLength * 2.0f + arcLength;
    }

    // Integrate a two-metre linear grade blend at each landing. Height and
    // first derivative both meet the flat floor, avoiding camera bobble when
    // CollisionWorld hands the capsule between a path and a walkable rect.
    constexpr float blendLength = 2.0f;
    const float totalLength =
        kWatatsumiRampStraightLength * 2.0f + arcLength;
    const float effectiveLength = totalLength - blendLength;
    float integrated = 0.0f;
    if (travel <= blendLength)
    {
        integrated = 0.5f * travel * travel / blendLength;
    }
    else if (travel < totalLength - blendLength)
    {
        integrated = travel - blendLength * 0.5f;
    }
    else
    {
        const float remaining = totalLength - travel;
        integrated = effectiveLength -
            0.5f * remaining * remaining / blendLength;
    }
    return kWatatsumiUpperFloorY * integrated / effectiveLength;
}

DirectX::XMFLOAT3 EvaluateWatatsumiRampPoint(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float y = kStageFloorOffset + EvaluateWatatsumiRampHeight(t);
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
        return line(-0.5f, -17.8f, 5.8f, -17.8f, t / 0.06f);
    }
    if (t < 0.20f)
    {
        return line(
            5.8f, -17.8f, 17.8f, -17.8f,
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
            std::sin(angle) * 17.8f};
    }
    if (t < 0.97f)
    {
        return line(
            17.8f, 17.8f, 5.8f, 17.8f,
            (t - 0.80f) / 0.17f);
    }
    return line(
        5.8f, 17.8f, 3.6f, 17.8f,
        (t - 0.97f) / 0.03f);
}

DirectX::XMFLOAT3 EvaluateUnderwaterArchPoint(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float smooth = t * t * (3.0f - 2.0f * t);
    return {48.0f * t, kStageFloorOffset - 4.70f * smooth, 0.0f};
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
    BuildStageGlassCollision();
    BuildRouteCollision();
    BuildUnderwaterArchCollision();
    BuildWatatsumiCollision();
    BuildContinuousCollision();

    // Keep automated performance captures reproducible without changing the
    // normal player-facing startup route. Example: AQUARIUM_START_VIEW=6.
    wchar_t startView[8]{};
    if (GetEnvironmentVariableW(
            L"AQUARIUM_START_VIEW", startView,
            ARRAYSIZE(startView)) > 0)
    {
        switch (startView[0])
        {
        case L'3': SelectAquariumGreyboxView(); break;
        case L'4': SelectUnderwaterArchView(); break;
        case L'5': SelectJellyfishReverseValidationView(); break;
        case L'6': SelectWatatsumiTankView(); break;
        case L'7': SelectContinuousAquariumView(); break;
        default: break;
        }
    }
    ResetPlayer();
}

void AquariumScene::BuildStageGlassCollision()
{
    using physics::ColliderTag;
    using physics::CollisionLayer;
    using physics::LayerMask;

    stageGlassCollision_.Clear();
    stageGlassCollision_.AddWalkableRect({
        L"StageGlass_PublicFloor",
        -4.75f, 4.75f, -10.0f, -5.35f, -2.25f,
        ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});
    stageGlassCollision_.AddBox({
        L"StageGlass_AquariumPane",
        {-5.10f, -2.25f, -5.35f},
        {5.10f, 3.0f, -5.15f},
        ColliderTag::Glass,
        LayerMask(CollisionLayer::World)});
    stageGlassCollision_.AddBox({
        L"StageGlass_RearWall",
        {-5.10f, -2.25f, -10.20f},
        {5.10f, 3.0f, -10.0f},
        ColliderTag::Solid,
        LayerMask(CollisionLayer::World)});
    for (const float x : {-4.85f, 4.85f})
    {
        stageGlassCollision_.AddBox({
            L"StageGlass_SideWall",
            {x - 0.10f, -2.25f, -10.0f},
            {x + 0.10f, 3.0f, -5.35f},
            ColliderTag::Solid,
            LayerMask(CollisionLayer::World)});
    }
}

void AquariumScene::BuildRouteCollision()
{
    using physics::ColliderTag;
    using physics::CollisionLayer;
    using physics::LayerMask;

    routeCollision_.Clear();
    const auto addBox = [this](
        const wchar_t* name,
        float centerX, float centerY, float centerZ,
        float sizeX, float sizeY, float sizeZ,
        ColliderTag tag = ColliderTag::Solid)
    {
        routeCollision_.AddBox({
            name,
            {centerX - sizeX * 0.5f,
             centerY + kStageFloorOffset - sizeY * 0.5f,
             centerZ - sizeZ * 0.5f},
            {centerX + sizeX * 0.5f,
             centerY + kStageFloorOffset + sizeY * 0.5f,
             centerZ + sizeZ * 0.5f},
            tag,
            LayerMask(CollisionLayer::World)});
    };
    const auto wallX = [&addBox](
        const wchar_t* name, float x0, float x1, float z, float height)
    {
        addBox(name, (x0 + x1) * 0.5f, height * 0.5f, z,
            x1 - x0, height, 0.28f);
    };
    const auto wallZ = [&addBox](
        const wchar_t* name, float x, float z0, float z1, float height)
    {
        addBox(name, x, height * 0.5f, (z0 + z1) * 0.5f,
            0.28f, height, z1 - z0);
    };

    // The three rectangles meet at authored door openings. CollisionWorld
    // treats them as floor coverage, while the boxes below are the walls.
    routeCollision_.AddWalkableRect({
        L"Route01_EntranceFloor", -18.0f, -6.0f, -4.5f, 4.5f,
        kStageFloorOffset,
        ColliderTag::Walkable, LayerMask(CollisionLayer::World)});
    routeCollision_.AddWalkableRect({
        L"Route01_VestibuleFloor", -6.0f, -3.0f, -2.0f, 2.0f,
        kStageFloorOffset,
        ColliderTag::Walkable, LayerMask(CollisionLayer::World)});
    routeCollision_.AddWalkableRect({
        L"Route02_JellyfishFloor", -3.0f, 15.0f, -7.5f, 7.5f,
        kStageFloorOffset,
        ColliderTag::Walkable, LayerMask(CollisionLayer::World)});

    wallX(L"Entrance_NorthWall", -18.0f, -6.0f, 4.5f, 4.0f);
    wallX(L"Entrance_SouthWall", -18.0f, -6.0f, -4.5f, 4.0f);
    wallZ(L"Entrance_WestWall_North", -18.0f, 2.0f, 4.5f, 4.0f);
    wallZ(L"Entrance_WestWall_South", -18.0f, -4.5f, -2.0f, 4.0f);
    wallZ(L"Entrance_EastWall_North", -6.0f, 2.0f, 4.5f, 4.0f);
    wallZ(L"Entrance_EastWall_South", -6.0f, -4.5f, -2.0f, 4.0f);
    wallX(L"Vestibule_NorthWall", -6.0f, -3.0f, 2.0f, 3.2f);
    wallX(L"Vestibule_SouthWall", -6.0f, -3.0f, -2.0f, 3.2f);
    wallX(L"Jellyfish_NorthWall", -3.0f, 15.0f, 7.5f, 5.2f);
    wallX(L"Jellyfish_SouthWall", -3.0f, 15.0f, -7.5f, 5.2f);
    wallZ(L"Jellyfish_WestWall_North", -3.0f, 2.0f, 7.5f, 5.2f);
    wallZ(L"Jellyfish_WestWall_South", -3.0f, -7.5f, -2.0f, 5.2f);
    wallZ(L"Jellyfish_EastWall_North", 15.0f, 1.6f, 7.5f, 5.2f);
    wallZ(L"Jellyfish_EastWall_South", 15.0f, -7.5f, -1.6f, 5.2f);

    // Event doors, furniture and tanks use distinct tags so interaction and a
    // future navmesh bake can filter them without geometry-name heuristics.
    addBox(L"Entrance_AutomaticDoor", -17.86f, 1.40f, 0.0f,
        0.12f, 2.80f, 4.0f);
    addBox(L"Entrance_InfoCounter", -13.6f, 0.62f, 3.65f,
        4.8f, 1.24f, 1.15f);
    addBox(L"Entrance_InfoBackPanel", -13.6f, 2.25f, 4.30f,
        5.4f, 2.45f, 0.22f);
    addBox(L"Entrance_MapPedestal", -10.2f, 0.72f, -0.6f,
        1.55f, 1.44f, 1.05f);
    for (const float z : {-2.8f, 2.8f})
    {
        addBox(L"Entrance_Bench", -8.0f, 0.48f, z,
            2.8f, 0.96f, 0.72f);
    }

    struct JellyColumn
    {
        float x;
        float z;
        float radius;
        float height;
    };
    constexpr JellyColumn columns[] = {
        {-0.2f, 3.55f, 0.72f, 3.85f},
        {2.0f, -3.45f, 0.64f, 3.35f},
        {4.3f, 3.20f, 0.70f, 4.05f},
        {6.6f, -3.65f, 0.74f, 3.75f},
        {8.8f, 3.45f, 0.62f, 3.40f},
        {11.0f, -3.25f, 0.70f, 4.00f},
        {13.0f, 3.55f, 0.66f, 3.55f}};
    for (const JellyColumn& column : columns)
    {
        addBox(L"Jellyfish_DisplayGlass", column.x,
            (column.height + 0.58f) * 0.5f, column.z,
            column.radius * 2.0f, column.height + 0.58f,
            column.radius * 2.0f, ColliderTag::Glass);
    }
    addBox(L"Jellyfish_StartBench", -0.3f, 0.48f, -5.65f,
        3.2f, 0.96f, 0.78f);
}

void AquariumScene::BuildUnderwaterArchCollision()
{
    using physics::ColliderTag;
    using physics::CollisionLayer;
    using physics::LayerMask;

    underwaterArchCollision_.Clear();
    physics::PathSurface route;
    route.name = L"DescendingUnderwaterArch_Walkway";
    // The visible rail begins at 3.06 m. Subtracting the capsule radius in
    // CollisionWorld leaves the player's body tangent to that rail.
    route.halfWidth = 3.06f;
    route.tag = ColliderTag::Ramp;
    route.layer = LayerMask(CollisionLayer::World);
    constexpr int routeSegments = 192;
    route.centerLine.reserve(routeSegments + 1);
    for (int index = 0; index <= routeSegments; ++index)
    {
        route.centerLine.push_back(EvaluateUnderwaterArchPoint(
            index / static_cast<float>(routeSegments)));
    }
    underwaterArchCollision_.AddPathSurface(std::move(route));
}

void AquariumScene::BuildWatatsumiCollision()
{
    using physics::ColliderTag;
    using physics::CollisionLayer;
    using physics::LayerMask;

    watatsumiCollision_.Clear();
    watatsumiCollision_.AddWalkableRect({
        L"Watatsumi_1F_PublicFloor",
        -27.55f, 6.90f, -23.55f, 23.55f, kStageFloorOffset,
        ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddWalkableRect({
        L"Watatsumi_2F_NorthWalkway",
        -21.0f, 4.0f, 15.10f, 20.50f,
        kStageFloorOffset + 12.28f,
        ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddWalkableRect({
        L"Watatsumi_2F_SouthWalkway",
        -21.0f, 4.0f, -20.50f, -15.10f,
        kStageFloorOffset + 12.28f,
        ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddWalkableRect({
        L"Watatsumi_2F_CentreCrossWalkway",
        -11.20f, -5.80f, -20.50f, 20.50f,
        kStageFloorOffset + 12.28f,
        ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});

    physics::PathSurface ramp;
    ramp.name = L"Watatsumi_RightHelixRamp";
    ramp.halfWidth = 2.70f;
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
        {6.38f, kStageFloorOffset, -14.75f},
        {7.50f, kStageFloorOffset + 19.60f, 14.75f},
        ColliderTag::Glass,
        LayerMask(CollisionLayer::World)});
    // StageModel flips glTF Z while converting to the renderer's left-handed
    // coordinates. These portal colliders therefore use the rendered signs:
    // the lower entrance is at -17.8 m and the upper landing at +17.8 m.
    watatsumiCollision_.AddBox({
        L"Watatsumi_LowerPortalHeader",
        {6.38f, kStageFloorOffset + 7.35f, -20.90f},
        {7.50f, kStageFloorOffset + 19.60f, -14.70f},
        ColliderTag::Solid,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddBox({
        L"Watatsumi_UpperPortalStructuralBand",
        {6.38f, kStageFloorOffset + 7.35f, 14.70f},
        {7.50f, kStageFloorOffset + kWatatsumiUpperFloorY - 0.22f, 20.90f},
        ColliderTag::Solid,
        LayerMask(CollisionLayer::World)});
    for (const float portalZ : {-17.80f, 17.80f})
    {
        watatsumiCollision_.AddBox({
            portalZ < 0.0f
                ? L"Watatsumi_LowerPortalLeftShoulder"
                : L"Watatsumi_UpperPortalLeftShoulder",
            {6.38f, kStageFloorOffset, portalZ - 3.10f},
            {7.50f, kStageFloorOffset + 19.60f, portalZ - 2.70f},
            ColliderTag::Solid,
            LayerMask(CollisionLayer::World)});
        watatsumiCollision_.AddBox({
            portalZ < 0.0f
                ? L"Watatsumi_LowerPortalRightShoulder"
                : L"Watatsumi_UpperPortalRightShoulder",
            {6.38f, kStageFloorOffset, portalZ + 2.70f},
            {7.50f, kStageFloorOffset + 19.60f, portalZ + 3.10f},
            ColliderTag::Solid,
            LayerMask(CollisionLayer::World)});
    }
    // Exact facade closures match the generator: tank jamb -> portal trim,
    // then portal outer edge -> exterior wall. Unlike the old patch blocks,
    // these volumes neither overlap the doorway nor leave a hidden rear gap.
    for (const float side : {-1.0f, 1.0f})
    {
        watatsumiCollision_.AddBox({
            side < 0.0f
                ? L"Watatsumi_TankToSouthPortalClosure"
                : L"Watatsumi_TankToNorthPortalClosure",
            {6.38f, kStageFloorOffset,
             side < 0.0f ? -14.70f : 14.65f},
            {7.50f, kStageFloorOffset + 19.60f,
             side < 0.0f ? -14.65f : 14.70f},
            ColliderTag::Solid,
            LayerMask(CollisionLayer::World)});
        watatsumiCollision_.AddBox({
            side < 0.0f
                ? L"Watatsumi_SouthServiceVoid"
                : L"Watatsumi_NorthServiceVoid",
            {-27.70f, kStageFloorOffset,
             side < 0.0f ? -23.70f : 20.90f},
            {6.45f, kStageFloorOffset + 19.60f,
             side < 0.0f ? -20.90f : 23.70f},
            ColliderTag::Solid,
            LayerMask(CollisionLayer::World)});
    }
    for (const float side : {-1.0f, 1.0f})
    {
        watatsumiCollision_.AddBox({
            side < 0.0f
                ? L"Watatsumi_RearWallSouth"
                : L"Watatsumi_RearWallNorth",
            {-28.10f, kStageFloorOffset - 0.20f,
             side < 0.0f ? -24.0f : 4.20f},
            {-27.55f, kStageFloorOffset + 19.80f,
             side < 0.0f ? -4.20f : 24.0f},
            ColliderTag::Solid,
            LayerMask(CollisionLayer::World)});
    }
    watatsumiCollision_.AddBox({
        L"Watatsumi_RearEntranceHeader",
        {-28.10f, kStageFloorOffset + 5.20f, -4.20f},
        {-27.55f, kStageFloorOffset + 19.80f, 4.20f},
        ColliderTag::Solid,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddBox({
        L"Watatsumi_NorthWall",
        {-28.0f, kStageFloorOffset - 0.20f, 23.55f},
        {38.0f, kStageFloorOffset + 19.80f, 24.10f},
        ColliderTag::Solid,
        LayerMask(CollisionLayer::World)});
    watatsumiCollision_.AddBox({
        L"Watatsumi_SouthWall",
        {-28.0f, kStageFloorOffset - 0.20f, -24.10f},
        {38.0f, kStageFloorOffset + 19.80f, -23.55f},
        ColliderTag::Solid,
        LayerMask(CollisionLayer::World)});

    // Match the generated H exactly. Inner arm rails stop at the cross-passage
    // instead of piercing its walking surface, and each dead end is capped.
    const auto horizontalRail = [this](
        const wchar_t* name, float x0, float x1, float z)
    {
        watatsumiCollision_.AddBox({
            name,
            {x0, kStageFloorOffset + 12.28f, z - 0.06f},
            {x1, kStageFloorOffset + 13.44f, z + 0.06f},
            ColliderTag::Rail,
            LayerMask(CollisionLayer::World)});
    };
    const auto verticalRail = [this](
        const wchar_t* name, float x, float z0, float z1)
    {
        watatsumiCollision_.AddBox({
            name,
            {x - 0.06f, kStageFloorOffset + 12.28f, z0},
            {x + 0.06f, kStageFloorOffset + 13.44f, z1},
            ColliderTag::Rail,
            LayerMask(CollisionLayer::World)});
    };
    horizontalRail(L"Watatsumi_2F_SouthOuterRail", -21.0f, 4.0f, -20.50f);
    horizontalRail(L"Watatsumi_2F_NorthOuterRail", -21.0f, 4.0f, 20.50f);
    for (const float z : {-15.10f, 15.10f})
    {
        horizontalRail(L"Watatsumi_2F_InnerRailWest", -21.0f, -11.20f, z);
        horizontalRail(L"Watatsumi_2F_InnerRailEast", -5.80f, 4.0f, z);
    }
    verticalRail(L"Watatsumi_2F_CrossRailWest", -11.20f, -15.10f, 15.10f);
    verticalRail(L"Watatsumi_2F_CrossRailEast", -5.80f, -15.10f, 15.10f);
    verticalRail(L"Watatsumi_2F_SouthWestEndRail", -21.0f, -20.50f, -15.10f);
    verticalRail(L"Watatsumi_2F_NorthWestEndRail", -21.0f, 15.10f, 20.50f);
    // glTF Z is negated by StageModel: the visual south-east cap appears on
    // rendered -Z, while rendered +Z remains open to the upper ramp landing.
    verticalRail(L"Watatsumi_2F_SouthEastEndRail", 4.0f, -20.50f, -15.10f);

}

void AquariumScene::BuildContinuousCollision()
{
    using physics::ColliderTag;
    using physics::CollisionLayer;
    using physics::LayerMask;

    continuousCollision_.Clear();
    for (const physics::BoxCollider& box : watatsumiCollision_.Boxes())
    {
        continuousCollision_.AddBox(box);
    }
    for (const physics::WalkableRect& rect : watatsumiCollision_.WalkableRects())
    {
        continuousCollision_.AddWalkableRect(rect);
    }
    for (const physics::PathSurface& path : watatsumiCollision_.Paths())
    {
        continuousCollision_.AddPathSurface(path);
    }

    const auto addWall = [this](
        const wchar_t* name,
        float minX, float maxX,
        float minZ, float maxZ,
        float floorY, float height,
        ColliderTag tag = ColliderTag::Solid)
    {
        continuousCollision_.AddBox({
            name,
            {minX, floorY, minZ},
            {maxX, floorY + height, maxZ},
            tag,
            LayerMask(CollisionLayer::World)});
    };

    // Entrance and the 1F side gallery overlap the hall floor at their seams,
    // preventing a capsule-height snap when crossing between GLB chunks.
    continuousCollision_.AddWalkableRect({
        L"Continuous_EntranceFloor", -42.0f, -27.40f, -6.0f, 6.0f,
        kStageFloorOffset, ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});
    addWall(L"Continuous_EntranceNorthWall", -42.1f, -27.4f, 5.85f, 6.15f,
        kStageFloorOffset, 5.4f);
    addWall(L"Continuous_EntranceSouthWall", -42.1f, -27.4f, -6.15f, -5.85f,
        kStageFloorOffset, 5.4f);
    addWall(L"Continuous_EntranceExitDoor", -42.15f, -41.75f, -2.0f, 2.0f,
        kStageFloorOffset, 2.9f, ColliderTag::Trigger);

    continuousCollision_.AddWalkableRect({
        L"Continuous_HeroSideGallery", 6.45f, 22.50f, 14.70f, 20.70f,
        kStageFloorOffset, ColliderTag::Walkable,
        LayerMask(CollisionLayer::World)});
    // Keep the capsule centre 0.04 m inside the arch endpoint's legal radius.
    // Without this funnel a player holding strafe against the outer wall could
    // miss PathSurface hand-off by roughly one centimetre.
    addWall(L"Continuous_SideGalleryOuterWall", 6.45f, 22.7f, 20.70f, 21.05f,
        kStageFloorOffset, 7.2f);
    addWall(L"Continuous_SideTankGlass", 7.1f, 17.2f, 14.55f, 14.78f,
        kStageFloorOffset, 12.6f, ColliderTag::Glass);
    addWall(L"Continuous_SideGalleryInnerClosure", 17.2f, 22.7f, 14.55f, 14.85f,
        kStageFloorOffset, 7.2f);

    physics::PathSurface arch;
    arch.name = L"Continuous_DescendingUnderwaterArch";
    arch.halfWidth = 3.06f;
    arch.tag = ColliderTag::Ramp;
    arch.layer = LayerMask(CollisionLayer::World);
    constexpr int archSegments = 192;
    arch.centerLine.reserve(archSegments + 1);
    for (int index = 0; index <= archSegments; ++index)
    {
        DirectX::XMFLOAT3 point = EvaluateUnderwaterArchPoint(
            index / static_cast<float>(archSegments));
        point.x += 22.0f;
        point.z += 17.80f;
        arch.centerLine.push_back(point);
    }
    continuousCollision_.AddPathSurface(std::move(arch));

    const float basementY = kStageFloorOffset - 4.70f;
    continuousCollision_.AddWalkableRect({
        L"Continuous_JellyColumnRoom", 69.6f, 88.0f, 10.3f, 25.3f,
        basementY, ColliderTag::Walkable, LayerMask(CollisionLayer::World)});
    continuousCollision_.AddWalkableRect({
        L"Continuous_PanoramaJellyRoom", 88.0f, 110.0f, 8.8f, 26.8f,
        basementY, ColliderTag::Walkable, LayerMask(CollisionLayer::World)});
    addWall(L"Continuous_BasementSouthWall", 69.6f, 110.2f, 8.65f, 8.95f,
        basementY, 6.2f);
    addWall(L"Continuous_BasementNorthWall", 69.6f, 110.2f, 26.65f, 26.95f,
        basementY, 6.2f);
    addWall(L"Continuous_BasementEndWall", 109.85f, 110.15f, 8.8f, 26.8f,
        basementY, 6.2f);

    struct JellyColumn { float x; float z; };
    constexpr JellyColumn columns[] = {
        {73.0f, 21.3f}, {76.0f, 14.2f}, {80.0f, 21.5f},
        {83.0f, 14.4f}, {86.0f, 21.2f}};
    for (const JellyColumn& column : columns)
    {
        addWall(L"Continuous_JellyColumnGlass",
            column.x - 0.76f, column.x + 0.76f,
            column.z - 0.76f, column.z + 0.76f,
            basementY, 4.7f, ColliderTag::Glass);
    }
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
    if (input.WasPressed('7'))
    {
        SelectContinuousAquariumView();
    }

    // プレイヤー入力とライティング調整を各専用クラスへ委譲する。
    UpdatePlayer(frame.deltaTime, input);
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
    diagnostics.viewLabel = settings_.continuousMapMode
        ? L"ROUTE 07: CONTINUOUS AQUARIUM"
        : (settings_.watatsumiTankMode
        ? L"ROUTE 05: WATATSUMI HERO TANK"
        : (settings_.underwaterArchMode
        ? L"ROUTE 06: DESCENDING UNDERWATER ARCH"
        : (settings_.greyboxMode
        ? L"ROUTE 01-02: ENTRANCE + JELLYFISH"
        : (settings_.stageMode
            ? L"STAGE + GLASS VIEW"
            : (settings_.viewMode > 0.5f
                ? L"GLASS VIEW"
                : L"UNDERWATER VIEW")))));
    diagnostics.causticsStrength = settings_.causticsStrength;
    diagnostics.volumeStrength = settings_.volumeStrength;
    diagnostics.anisotropy = settings_.anisotropy;
    diagnostics.exposure = settings_.exposure;
    diagnostics.renderScale = renderer_.RenderScale();
    diagnostics.smoothedFrameMilliseconds =
        renderer_.SmoothedFrameMilliseconds();
    diagnostics.paused = settings_.paused;
    return diagnostics;
}

void AquariumScene::ResetSettings()
{
    const lighting::LocalLightingRig localLighting = settings_.localLighting;
    const lighting::HeroTankLightingRig heroTankLighting =
        settings_.heroTankLighting;
    settings_ = {};
    settings_.localLighting = localLighting;
    settings_.heroTankLighting = heroTankLighting;
    ResetPlayer();
}

void AquariumScene::ResetPlayer()
{
    playerManager_.Reset(
        {
            settings_.cameraPositionX,
            settings_.cameraPositionY,
            settings_.cameraPositionZ
        },
        settings_.cameraYaw,
        settings_.cameraPitch);
}

void AquariumScene::SelectUnderwaterView()
{
    settings_.viewMode = 0.0f;
    settings_.stageMode = false;
    settings_.greyboxMode = false;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
    settings_.continuousMapMode = false;
}

void AquariumScene::SelectStageGlassView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = false;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
    settings_.continuousMapMode = false;
    settings_.cameraPositionX = 0.0f;
    settings_.cameraPositionY = -2.25f + playerManager_.Capsule().eyeHeight;
    settings_.cameraPositionZ = -6.65f;
    settings_.cameraYaw = 0.0f;
    settings_.cameraPitch = -0.03f;
    ResetPlayer();
}

void AquariumScene::SelectAquariumGreyboxView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
    settings_.continuousMapMode = false;
    // Enter from the public doors and look through the lobby toward the
    // Jellyfish Theater. The generated route runs along +X.
    settings_.cameraPositionX = -16.2f;
    settings_.cameraPositionY = kStageFloorOffset + playerManager_.Capsule().eyeHeight;
    settings_.cameraPositionZ = 0.0f;
    settings_.cameraYaw = 1.57079633f;
    settings_.cameraPitch = -0.04f;
    ResetPlayer();
}

void AquariumScene::SelectUnderwaterArchView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = true;
    settings_.watatsumiTankMode = false;
    settings_.continuousMapMode = false;
    settings_.cameraPositionX = 0.85f;
    settings_.cameraPositionY = kStageFloorOffset + playerManager_.Capsule().eyeHeight;
    settings_.cameraPositionZ = 0.0f;
    settings_.cameraYaw = 1.57079633f;
    settings_.cameraPitch = -0.055f;
    ResetPlayer();
}

void AquariumScene::SelectJellyfishReverseValidationView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
    settings_.continuousMapMode = false;
    settings_.cameraPositionX = 6.0f;
    settings_.cameraPositionY = kStageFloorOffset + playerManager_.Capsule().eyeHeight;
    settings_.cameraPositionZ = -6.15f;
    settings_.cameraYaw = 0.0f;
    settings_.cameraPitch = -0.03f;
    ResetPlayer();
}

void AquariumScene::SelectWatatsumiTankView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = true;
    settings_.continuousMapMode = false;
    // Offset toward the ramp side so the tank remains the hero while the
    // lower-right wall portal is readable in the establishing shot.
    settings_.cameraPositionX = -9.5f;
    settings_.cameraPositionY = kStageFloorOffset + playerManager_.Capsule().eyeHeight;
    settings_.cameraPositionZ = -3.0f;
    settings_.cameraYaw = 1.505f;
    settings_.cameraPitch = -0.065f;
    ResetPlayer();
}

void AquariumScene::SelectContinuousAquariumView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
    settings_.continuousMapMode = true;
    settings_.cameraPositionX = -36.5f;
    settings_.cameraPositionY =
        kStageFloorOffset + playerManager_.Capsule().eyeHeight;
    settings_.cameraPositionZ = 0.0f;
    settings_.cameraYaw = 1.57079633f;
    settings_.cameraPitch = -0.035f;
    ResetPlayer();
}

void AquariumScene::UpdatePlayer(
    float deltaTime,
    const framework::InputSystem& input)
{
    const physics::CollisionWorld* collisionWorld = nullptr;
    if (settings_.underwaterArchMode)
    {
        collisionWorld = &underwaterArchCollision_;
    }
    else if (settings_.continuousMapMode)
    {
        collisionWorld = &continuousCollision_;
    }
    else if (settings_.watatsumiTankMode)
    {
        collisionWorld = &watatsumiCollision_;
    }
    else if (settings_.greyboxMode)
    {
        collisionWorld = &routeCollision_;
    }
    else if (settings_.stageMode)
    {
        collisionWorld = &stageGlassCollision_;
    }

    playerManager_.Update(
        deltaTime, input, collisionWorld, collisionWorld == nullptr);
    const DirectX::XMFLOAT3& eye = playerManager_.EyePosition();
    settings_.cameraPositionX = eye.x;
    settings_.cameraPositionY = eye.y;
    settings_.cameraPositionZ = eye.z;
    settings_.cameraYaw = playerManager_.Yaw();
    settings_.cameraPitch = playerManager_.Pitch();

    // The manager already emits a world-space ray on left click. A later
    // interaction registry can raycast here without changing input code.
    if (playerManager_.SelectionRequested())
    {
        [[maybe_unused]] const player::SelectionRay selectionRay =
            playerManager_.GetSelectionRay();
    }
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
