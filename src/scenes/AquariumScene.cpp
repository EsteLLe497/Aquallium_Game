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
        default: break;
        }
    }
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
        L"Watatsumi_UpperPortalLowerWall",
        {6.38f, kStageFloorOffset, 14.70f},
        {7.50f, kStageFloorOffset + 11.70f, 20.90f},
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
    watatsumiCollision_.AddBox({
        L"Watatsumi_RearWall",
        {-28.10f, kStageFloorOffset - 0.20f, -24.0f},
        {-27.55f, kStageFloorOffset + 19.80f, 24.0f},
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
    playerCharacter_.activePath = -1;
    playerCharacter_.activeSegment = 0;
}

void AquariumScene::ResetPlayerCharacter()
{
    playerCharacter_.eyePosition = {
        settings_.cameraPositionX,
        settings_.cameraPositionY,
        settings_.cameraPositionZ};
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
    settings_.cameraPositionY = -2.25f + playerCapsule_.eyeHeight;
    settings_.cameraPositionZ = -6.65f;
    settings_.cameraYaw = 0.0f;
    settings_.cameraPitch = -0.03f;
    ResetPlayerCharacter();
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
    settings_.cameraPositionY = kStageFloorOffset + playerCapsule_.eyeHeight;
    settings_.cameraPositionZ = 0.0f;
    settings_.cameraYaw = 1.57079633f;
    settings_.cameraPitch = -0.04f;
    ResetPlayerCharacter();
}

void AquariumScene::SelectUnderwaterArchView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = true;
    settings_.watatsumiTankMode = false;
    settings_.cameraPositionX = 0.85f;
    settings_.cameraPositionY = kStageFloorOffset + playerCapsule_.eyeHeight;
    settings_.cameraPositionZ = 0.0f;
    settings_.cameraYaw = 1.57079633f;
    settings_.cameraPitch = -0.055f;
    ResetPlayerCharacter();
    playerCharacter_.activePath = 0;
}

void AquariumScene::SelectJellyfishReverseValidationView()
{
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
    settings_.cameraPositionX = 6.0f;
    settings_.cameraPositionY = kStageFloorOffset + playerCapsule_.eyeHeight;
    settings_.cameraPositionZ = -6.15f;
    settings_.cameraYaw = 0.0f;
    settings_.cameraPitch = -0.03f;
    ResetPlayerCharacter();
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
    settings_.cameraPositionY = kStageFloorOffset + playerCapsule_.eyeHeight;
    settings_.cameraPositionZ = -3.0f;
    settings_.cameraYaw = 1.505f;
    settings_.cameraPitch = -0.065f;
    ResetPlayerCharacter();
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
            if (!settings_.stageMode)
            {
                settings_.cameraPositionX += requestedMoveX;
                settings_.cameraPositionZ += requestedMoveZ;
            }
        }
        if (!settings_.stageMode && input.IsDown('E'))
        {
            settings_.cameraPositionY += movementSpeed * 0.65f * deltaTime;
        }
        if (!settings_.stageMode && input.IsDown('Q'))
        {
            settings_.cameraPositionY -= movementSpeed * 0.65f * deltaTime;
        }

        if (settings_.underwaterArchMode)
        {
            underwaterArchCollision_.MoveCharacter(
                playerCharacter_,
                {requestedMoveX, 0.0f, requestedMoveZ},
                playerCapsule_);
            settings_.cameraPositionX = playerCharacter_.eyePosition.x;
            settings_.cameraPositionY = playerCharacter_.eyePosition.y;
            settings_.cameraPositionZ = playerCharacter_.eyePosition.z;
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
        else if (settings_.greyboxMode)
        {
            routeCollision_.MoveCharacter(
                playerCharacter_,
                {requestedMoveX, 0.0f, requestedMoveZ},
                playerCapsule_);
            settings_.cameraPositionX = playerCharacter_.eyePosition.x;
            settings_.cameraPositionY = playerCharacter_.eyePosition.y;
            settings_.cameraPositionZ = playerCharacter_.eyePosition.z;
        }
        else if (settings_.stageMode)
        {
            stageGlassCollision_.MoveCharacter(
                playerCharacter_,
                {requestedMoveX, 0.0f, requestedMoveZ},
                playerCapsule_);
            settings_.cameraPositionX = playerCharacter_.eyePosition.x;
            settings_.cameraPositionY = playerCharacter_.eyePosition.y;
            settings_.cameraPositionZ = playerCharacter_.eyePosition.z;
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
