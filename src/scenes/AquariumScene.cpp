/*==================================================================================================

   [AquariumScene.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   水中ビュー、ガラス越しビュー、ステージ確認ビューの更新と描画
===================================================================================================*/
#include "AquariumScene.h"

#include "../framework/input.h"
#include "../player/InteractionUI.h"
#include "../ui/AquariumUi.h"
#include "../generated/GameLayoutV3Generated.h"
#include "../generated/ReceptionLobbyGenerated.h"
#include "../generated/JellyBasementGenerated.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
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
    beachRenderer_.Initialize(device,shaderPath.parent_path()/"BeachPreview.hlsl");
    exitPortalRenderer_.Initialize(device,shaderPath.parent_path()/"ExitPortal.hlsl");
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
    BuildGameLayoutV3Collision();
    BuildReceptionLobbyCollision();
    BuildBeachCollision();
    miniMap_.Build(receptionLobbyCollision_);
    storyFolder_=std::filesystem::exists(std::filesystem::current_path()/"asset"/"story"/"minimap.goals")
        ? std::filesystem::current_path()/"asset"/"story"
        : shaderPath.parent_path().parent_path()/"asset"/"story";
    const auto textureFolder=std::filesystem::exists(std::filesystem::current_path()/"asset"/"texture"/"girl"/"normal.PNG")
        ? std::filesystem::current_path()/"asset"/"texture"
        : shaderPath.parent_path().parent_path()/"asset"/"texture";
    miniMap_.LoadGoals(storyFolder_);
    facilityPasswordLock_.Configure("2001","非常口 制御システム","4桁の起動パスワードを入力してください");
    settings_.heroTankLighting.Load(storyFolder_/"tank_lighting.cfg");
    heroineEncounter_.Initialize(device,storyFolder_,textureFolder);
    eventDialogue_.Initialize(device,textureFolder);
    inWaterStill_.Load(device,textureFolder/"stile"/"inWater.jpeg");
    storyFlowEditor_.Initialize(device,storyFolder_,textureFolder);
    gameMenu_.Initialize(device,textureFolder);
    const auto soundFolder=std::filesystem::exists(std::filesystem::current_path()/"asset"/"sound"/"se"/"select.mp3")
        ? std::filesystem::current_path()/"asset"/"sound"/"se"
        : shaderPath.parent_path().parent_path()/"asset"/"sound"/"se";
    soundEffects_.Initialize(soundFolder);
    beachSoundPath_=soundFolder/"Beach.mp3";
    beachMorningSoundPath_=soundFolder/"beachMorning.mp3";
    backgroundMusic_.initialize(soundFolder.parent_path()/"BGM");
    opening_.Load(shaderPath.parent_path().parent_path()/"asset"/"story");
    if(std::filesystem::exists(std::filesystem::current_path()/"asset"/"story"/"opening.camera"))
        opening_.Load(std::filesystem::current_path()/"asset"/"story");
    SelectReceptionLobbyView();applyTitlePresentation();
    gameMenu_.SetSlots(saveSystem_.Inspect());gameMenu_.ShowTitle();
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

void AquariumScene::BuildGameLayoutV3Collision()
{
    using physics::ColliderTag;
    using physics::CollisionLayer;
    using physics::LayerMask;

    gameLayoutV3Collision_.Clear();

    // The render mesh and these colliders are emitted by the same generator.
    // Keeping one source of truth prevents the invisible-wall and floor-seam
    // drift that occurred when three independently-authored maps overlapped.
    for (const game_layout_v3::BoxSpec& spec : game_layout_v3::kBoxes)
    {
        gameLayoutV3Collision_.AddBox({
            spec.name,
            {spec.minX, spec.minY, spec.minZ},
            {spec.maxX, spec.maxY, spec.maxZ},
            static_cast<ColliderTag>(spec.tag),
            LayerMask(CollisionLayer::World)});
    }
    for (const game_layout_v3::FloorSpec& spec : game_layout_v3::kFloors)
    {
        gameLayoutV3Collision_.AddWalkableRect({
            spec.name,
            spec.minX, spec.maxX, spec.minZ, spec.maxZ, spec.floorY,
            ColliderTag::Walkable,
            LayerMask(CollisionLayer::World)});
    }
    for (const game_layout_v3::PathSpec& spec : game_layout_v3::kPaths)
    {
        physics::PathSurface path;
        path.name = spec.name;
        path.centerLine.reserve(spec.count);
        for (std::size_t index = 0; index < spec.count; ++index)
        {
            const game_layout_v3::PathPoint& point = spec.points[index];
            path.centerLine.push_back({point.x, point.y, point.z});
        }
        path.halfWidth = spec.halfWidth;
        path.tag = ColliderTag::Ramp;
        path.layer = LayerMask(CollisionLayer::World);
        gameLayoutV3Collision_.AddPathSurface(std::move(path));
    }
}

void AquariumScene::BuildReceptionLobbyCollision()
{
    using physics::ColliderTag;
    using physics::CollisionLayer;
    using physics::LayerMask;

    receptionLobbyCollision_.Clear();
    for (const reception_lobby::BoxSpec& spec : reception_lobby::kBoxes)
    {
        receptionLobbyCollision_.AddBox({
            spec.name,
            {spec.minX, spec.minY, spec.minZ},
            {spec.maxX, spec.maxY, spec.maxZ},
            static_cast<ColliderTag>(spec.tag),
            LayerMask(CollisionLayer::World)});
    }
    for (const reception_lobby::FloorSpec& spec : reception_lobby::kFloors)
    {
        receptionLobbyCollision_.AddWalkableRect({
            spec.name,
            spec.minX, spec.maxX, spec.minZ, spec.maxZ, spec.floorY,
            ColliderTag::Walkable,
            LayerMask(CollisionLayer::World)});
    }
    for (const reception_lobby::PathSpec& spec : reception_lobby::kPaths)
    {
        physics::PathSurface path;
        path.name = spec.name;
        path.centerLine.reserve(spec.count);
        for (std::size_t index = 0; index < spec.count; ++index)
        {
            const reception_lobby::PathPoint& point = spec.points[index];
            path.centerLine.push_back({point.x, point.y, point.z});
        }
        path.halfWidth = spec.halfWidth;
        path.tag = ColliderTag::Ramp;
        path.layer = LayerMask(CollisionLayer::World);
        receptionLobbyCollision_.AddPathSurface(std::move(path));
    }
    for (const jelly_basement::BoxSpec& spec : jelly_basement::kBoxes)
    {
        receptionLobbyCollision_.AddBox({
            spec.name,
            {spec.minX, spec.minY, spec.minZ},
            {spec.maxX, spec.maxY, spec.maxZ},
            static_cast<ColliderTag>(spec.tag),
            LayerMask(CollisionLayer::World)});
    }
    for (const jelly_basement::CircleSpec& spec : jelly_basement::kCircles)
    {
        receptionLobbyCollision_.AddCircle({
            spec.name,
            {spec.x, spec.z},
            spec.minY, spec.maxY, spec.radius,
            static_cast<ColliderTag>(spec.tag),
            LayerMask(CollisionLayer::World)});
    }
    for (const jelly_basement::FloorSpec& spec : jelly_basement::kFloors)
    {
        receptionLobbyCollision_.AddWalkableRect({
            spec.name,
            spec.minX, spec.maxX, spec.minZ, spec.maxZ, spec.floorY,
            ColliderTag::Walkable,
            LayerMask(CollisionLayer::World)});
    }
}

void AquariumScene::Update(
    const framework::FrameContext& frame,
    const framework::InputSystem& input)
{
    updateAudioSettings();
    // すべてのBGM変更要求を一つの時間軸でフェード処理する。
    updateBackgroundMusic();
    backgroundMusic_.advance(frame.deltaTime);
    ProcessMenuRequest();
#if defined(_DEBUG)
    if(!endingSequence_.active()&&input.WasPressed(VK_F3))storyFlowEditor_.Toggle();
#endif
    if(storyFlowEditor_.Visible()){
        storyFlowEditor_.Update(frame.deltaTime);
        storyFlowEditor_.Draw();
        if(storyFlowEditor_.PreviewPlaying()&&!settings_.paused)simulationTime_+=frame.deltaTime;
        return;
    }
    if(UpdateSceneTransition(frame.deltaTime)) {
        updateBackgroundMusic();
        if(inWaterIntro_){DrawInWaterStill();eventDialogue_.Draw();}
        // 水中から館内へ切り替えた直後も、起床演出の閉じたまぶたを維持する。
        // 共通フェードより先に館内だけが露出する一フレームを作らない。
        else if(opening_.ControlsCamera())opening_.Draw(editorOpen_,false);
        if(gameMenu_.IsTitle())gameMenu_.SetSlots(saveSystem_.Inspect());
        gameMenu_.Draw();
        DrawSceneFade();
        return;
    }
    if(gameMenu_.IsTitle()){
        updateTitlePresentation();
        updateBackgroundMusic();
        gameMenu_.SetSlots(saveSystem_.Inspect());gameMenu_.Draw();
        if(!settings_.paused)simulationTime_+=frame.deltaTime;
        return;
    }
    if(endingSequence_.active()){
        const bool advance=input.WasPressed(VK_LBUTTON)||input.WasPressed('F');
        endingSequence_.update(frame.deltaTime,advance);
        updateBackgroundMusic();
        endingSequence_.draw();
        if(endingSequence_.consumeReturnTitle())
            BeginSceneTransition({player::GameMenu::RequestType::ReturnTitle,-1});
        DrawSceneFade();
        if(!settings_.paused)simulationTime_+=frame.deltaTime;
        return;
    }
    if(inWaterIntro_){
        const bool advance=input.WasPressed(VK_LBUTTON)||input.WasPressed('F');
        eventDialogue_.Update(frame.deltaTime,advance);
        if(inWaterIntroDialoguePending_&&!eventDialogue_.Active()){
            inWaterIntroDialoguePending_=false;
            inWaterIntroTransition_=transitionFade_.BeginOut();
        }
        updateBackgroundMusic();
        DrawInWaterStill();eventDialogue_.Draw();gameMenu_.Draw();DrawSceneFade();
        if(!settings_.paused)simulationTime_+=frame.deltaTime;
        return;
    }
    if(UpdateEmergencyExitTransition(frame.deltaTime))return;
    if(UpdateMorningBeachTransition(frame.deltaTime))return;
    if(beachPreview_){
        const bool beachAdvance=input.WasPressed(VK_LBUTTON)||input.WasPressed('F');
        UpdateMorningAmbience(frame.deltaTime);
        if(beachArrivalLook_){
            beachArrivalLookTime_+=std::clamp(frame.deltaTime,0.f,.1f);
            constexpr float startYaw=-.68f;
            const auto eased=[](float value){
                value=std::clamp(value,0.f,1.f);
                return value*value*(3.f-2.f*value);
            };
            float yaw=startYaw;
            if(beachArrivalLookTime_<1.10f)
                yaw=startYaw-.82f*eased(beachArrivalLookTime_/1.10f);
            else if(beachArrivalLookTime_<1.55f)yaw=startYaw-.82f;
            else if(beachArrivalLookTime_<3.00f)
                yaw=startYaw-.82f+2.07f*eased((beachArrivalLookTime_-1.55f)/1.45f);
            else if(beachArrivalLookTime_<3.45f)yaw=startYaw+1.25f;
            else
                yaw=startYaw+1.25f*(1.f-eased((beachArrivalLookTime_-3.45f)/.90f));
            const float pitch=-.035f-.025f*std::sin(std::clamp(beachArrivalLookTime_/4.35f,0.f,1.f)*DirectX::XM_PI);
            beachPlayer_.SetControlledPose(beachPlayer_.EyePosition(),yaw,pitch);
            if(beachArrivalLookTime_>=4.35f){
                beachArrivalLook_=false;
                beachArrivalDialoguePendingSit_=
                    eventDialogue_.Start(storyFolder_/"beach_arrival.dialogue",false);
            }
        } else {
            eventDialogue_.Update(frame.deltaTime,beachAdvance);
            if(beachSeatedDialoguePendingChoice_&&!actuallyMusicStarted_&&
               eventDialogue_.currentCue()==story::DialoguePlayer::Cue::ActuallyBgm){
                // 「凪沙……なのか？」へ到達した瞬間から正体判明曲へ切り替える。
                actuallyMusicStarted_=true;
                updateBackgroundMusic();
            }
            if(morningWakeDialoguePendingStand_&&!eventDialogue_.Active()){
                morningWakeDialoguePendingStand_=false;
                morningStandTransition_=true;morningStandTime_=0.f;
            }
            if(beachStayDialoguePendingAftermath_&&!eventDialogue_.Active()){
                beachStayDialoguePendingAftermath_=false;
                actuallyMusicStarted_=false;
                // どちらの結末も同じ生還演出から始め、記憶の残り方で差を描く。
                BeginMorningBeachTransition();
            }
            if(beachStayAftermathPendingEnding_&&!eventDialogue_.Active()){
                beachStayAftermathPendingEnding_=false;
                endingSequence_.startNormalEnd();
            }
            if(trueEpiloguePendingEnding_&&!eventDialogue_.Active()){
                trueEpiloguePendingEnding_=false;
                endingSequence_.startTrueEnd();
            }
            if(beachLeaveDialoguePendingTransition_&&!eventDialogue_.Active()){
                beachLeaveDialoguePendingTransition_=false;
                BeginMorningBeachTransition();
            }
            if(beachSeatedDialoguePendingChoice_&&!eventDialogue_.Active()){
                beachSeatedDialoguePendingChoice_=false;
                beachChoiceActive_=true;
            }
            if(beachArrivalDialoguePendingSit_&&!eventDialogue_.Active()){
                beachArrivalDialoguePendingSit_=false;
                beachSitTransition_=true;beachSitTransitionTime_=0.f;
                beachSitStartEye_=beachPlayer_.EyePosition();
                beachSitStartYaw_=beachPlayer_.Yaw();
                beachSitStartPitch_=beachPlayer_.Pitch();
            }
            if(beachSitTransition_){
                beachSitTransitionTime_+=std::clamp(frame.deltaTime,0.f,.1f);
                float t=std::clamp(beachSitTransitionTime_/2.20f,0.f,1.f);
                t=t*t*(3.f-2.f*t);
                auto eye=beachSitStartEye_;
                eye.y=beachSitStartEye_.y+(.86f-beachSitStartEye_.y)*t;
                eye.z=beachSitStartEye_.z+.12f*t;
                const float yaw=beachSitStartYaw_+(-.74f-beachSitStartYaw_)*t;
                const float pitch=beachSitStartPitch_+(-.075f-beachSitStartPitch_)*t-
                    std::sin(t*DirectX::XM_PI)*.025f;
                beachPlayer_.SetControlledPose(eye,yaw,pitch);
                if(beachSitTransitionTime_>=2.20f){
                    beachSitTransition_=false;beachSeated_=true;
                    beachSeatedDialoguePendingChoice_=
                        eventDialogue_.Start(storyFolder_/"beach_seated.dialogue",false);
                }
            }
            else if(morningWake_)UpdateMorningWake(frame.deltaTime);
            else if(morningStandTransition_)UpdateMorningStand(frame.deltaTime);
            else if(!beachSeated_&&!eventDialogue_.BlocksPlayer())
                beachPlayer_.Update(frame.deltaTime,input,&beachCollision_,false);
        }
        const auto eye=beachPlayer_.EyePosition();
        settings_.cameraPositionX=eye.x;settings_.cameraPositionY=eye.y;settings_.cameraPositionZ=eye.z;
        settings_.cameraYaw=beachPlayer_.Yaw();settings_.cameraPitch=beachPlayer_.Pitch();
        updateBackgroundMusic();
        if(!settings_.paused)simulationTime_+=frame.deltaTime;
        if(!beachStoryMode_){
            auto* draw=ImGui::GetForegroundDrawList();
            draw->AddRectFilled({18,18},{420,58},IM_COL32(4,12,28,150),7);
            draw->AddText({32,29},IM_COL32(220,235,255,220),beachMorning_
                ? "O  水族館へ戻る    P  夜へ切替"
                : "P  水族館へ戻る    O  昼へ切替");
        }
        eventDialogue_.Draw();
        if(beachChoiceActive_&&!gameMenu_.IsOpen())DrawBeachChoice();
        gameMenu_.Draw();
        return;
    }
    // 1フレームだけ反応する操作
    // 数字キーは通常時だけQA用ビュー切替に使う。パスワード入力中まで
    // ルート切替へ流すと、124を打っただけで別シーンへ飛んでしまう。
    // プレイヤー入力とライティング調整を各専用クラスへ委譲する。
    if(!settings_.receptionLobbyMode) { opening_.Stop();settings_.wakeBlur=0; }
    const bool passwordWasActive=passwordLock_.Active()||facilityPasswordLock_.Active();
    passwordLock_.Update(input);
    switch(passwordLock_.ConsumeFeedback()) {
    case story::PasswordLock::Feedback::Select:soundEffects_.Play("select");break;
    case story::PasswordLock::Feedback::Open:
        settings_.managementDoorUnlocked=true;soundEffects_.Play("open");break;
    case story::PasswordLock::Feedback::Miss:
        soundEffects_.Play("miss");eventDialogue_.Start(storyFolder_/"password_wrong.dialogue",false);break;
    default:break;
    }
    facilityPasswordLock_.Update(input);
    switch(facilityPasswordLock_.ConsumeFeedback()) {
    case story::PasswordLock::Feedback::Select:soundEffects_.Play("select");break;
    case story::PasswordLock::Feedback::Open:
        soundEffects_.PlayBoot();
        eventDialogue_.Start(storyFolder_/"facility_started.dialogue",false);
        facilityBootDialoguePending_=true;break;
    case story::PasswordLock::Feedback::Miss:
        soundEffects_.Play("miss");eventDialogue_.Start(storyFolder_/"password_wrong.dialogue",false);break;
    default:break;
    }
    if(input.WasPressed(VK_ESCAPE)&&!passwordWasActive&&!heroineEncounter_.BlocksPlayer()&&
       !eventDialogue_.BlocksPlayer()&&!opening_.BlocksPlayer()&&
       !terraceConversation_.BlocksPlayer()&&!archChase_.Active()&&!powerOutage_.BlocksPlayer()&&
       !tankLightingConsole_.Active())gameMenu_.Toggle();
    const bool advance=!gameMenu_.IsOpen()&&!passwordLock_.Active()&&!facilityPasswordLock_.Active()&&!powerOutage_.BlocksPlayer()&&
        (input.WasPressed(VK_LBUTTON)||input.WasPressed('F'));
    settings_.cluePaperHighlighted=false;
    settings_.manualPapersHighlighted=false;
    settings_.manualPapersVisible=opening_.RestCompleted()&&!opening_.ManualCollected();
    const bool chaseRun=archChase_.PlayerCanRun();
    const bool controlled=(!chaseRun&&(opening_.BlocksPlayer()||heroineEncounter_.BlocksPlayer()||
        eventDialogue_.BlocksPlayer()||gameMenu_.IsOpen()||passwordLock_.Active()||facilityPasswordLock_.Active()||
        terraceConversation_.BlocksPlayer()||powerOutage_.BlocksPlayer()||
        tankLightingConsole_.Active()))||archChase_.BlocksPlayer();
    opening_.Tick(frame.deltaTime,advance,editorOpen_,settings_);
    if(controlled || (!chaseRun && (opening_.BlocksPlayer()||heroineEncounter_.BlocksPlayer()||eventDialogue_.BlocksPlayer()))) {
        if(archChase_.ConstrainsPlayer())
            playerManager_.SetControlledPose(
                {settings_.cameraPositionX,settings_.cameraPositionY,settings_.cameraPositionZ},
                settings_.cameraYaw,settings_.cameraPitch);
        else ResetPlayer();
    }
    else if(!editorOpen_) UpdatePlayer(frame.deltaTime,input);
    heroineEncounter_.Update(frame.deltaTime,advance,settings_.cameraPositionX,
        settings_.cameraPositionY-playerManager_.Capsule().eyeHeight,settings_.cameraPositionZ,
        settings_.receptionLobbyMode&&opening_.FindingEmergency()&&!editorOpen_);
    if(heroineEncounter_.EmergencyFailed())opening_.MarkEmergencyFailed();
    if(heroineEncounter_.Complete()&&!heroineJoined_){
        heroineJoined_=true;opening_.BeginPowerMission();terraceConversation_.UnlockTutorial();
    }
    eventDialogue_.Update(frame.deltaTime,advance);
    if(facilityBootDialoguePending_&&!eventDialogue_.Active()){
        opening_.SetFacilityPasswordCollected();
        facilityBootDialoguePending_=false;
    }
    const bool terraceWasBlocking=terraceConversation_.BlocksPlayer();
    terraceConversation_.Update(frame.deltaTime,eventDialogue_.Active(),settings_);
    switch(terraceConversation_.ConsumeRequest()){
    case story::TerraceConversation::Request::Gossip:
        eventDialogue_.Start(storyFolder_/(opening_.FacilityPasswordCollected()?
            "terrace_silent.dialogue":"terrace_gossip.dialogue"),false);break;
    case story::TerraceConversation::Request::Hint:
        eventDialogue_.Start(storyFolder_/(opening_.FacilityPasswordCollected()?
            "terrace_silent.dialogue":(!clueCollected_?"terrace_hint_none.dialogue":
            (archChase_.Complete()?"terrace_hint_after.dialogue":"terrace_hint_before.dialogue"))),false);break;
    case story::TerraceConversation::Request::RestDialogue:
        eventDialogue_.Start(storyFolder_/"terrace_rest.dialogue",false);break;
    case story::TerraceConversation::Request::RestCompleted:
        opening_.CompleteTerraceRest();break;
    default:break;
    }
    const bool archWasConstraining=archChase_.ConstrainsPlayer();
    float archZIntent=0.f;
    if(input.IsDown('W'))archZIntent+=std::cos(playerManager_.Yaw());
    if(input.IsDown('S'))archZIntent-=std::cos(playerManager_.Yaw());
    if(input.IsDown('D'))archZIntent-=std::sin(playerManager_.Yaw());
    if(input.IsDown('A'))archZIntent+=std::sin(playerManager_.Yaw());
    archChase_.Update(frame.deltaTime,heroineJoined_&&clueCollected_,archZIntent<-.2f,
        eventDialogue_.Active(),settings_);
    // Arch depth is shaded per material in Stage.hlsl, so viewing the
    // basement through its doorway never changes that room's exposure.
    switch(archChase_.ConsumeRequest()){
    case story::ArchChase::Request::StagnationDialogue:
        eventDialogue_.Start(storyFolder_/"arch_stagnation.dialogue",false);break;
    case story::ArchChase::Request::Creak:soundEffects_.PlayCreak();break;
    case story::ArchChase::Request::CreakDialogue:
        eventDialogue_.Start(storyFolder_/"arch_creak.dialogue",false);break;
    case story::ArchChase::Request::PredatorAppear:soundEffects_.Play("kaigyo");break;
    case story::ArchChase::Request::EscapeDialogue:
        eventDialogue_.Start(storyFolder_/"arch_escape.dialogue",false);break;
    case story::ArchChase::Request::AftermathDialogue:
        eventDialogue_.Start(storyFolder_/"arch_aftermath.dialogue",false);break;
    case story::ArchChase::Request::Impact:soundEffects_.PlayBurst();break;
    default:break;
    }
    powerOutage_.Update(frame.deltaTime,archChase_.Complete()&&heroineJoined_&&clueCollected_,
        eventDialogue_.Active(),settings_,settings_);
    switch(powerOutage_.ConsumeRequest()) {
    case story::PowerOutage::Request::BlackoutDialogue:
        soundEffects_.PlayBang();eventDialogue_.Start(storyFolder_/"blackout.dialogue",false);break;
    case story::PowerOutage::Request::FootstepsStart:
        soundEffects_.PlayFootsteps(settings_.cameraPositionX,settings_.cameraPositionZ,
            settings_.cameraYaw);break;
    case story::PowerOutage::Request::FootstepsStop:soundEffects_.StopGenerated();break;
    case story::PowerOutage::Request::Bang:soundEffects_.Play("glass");break;
    case story::PowerOutage::Request::Hands:
        // hand.mp3 が未配置の開発環境でも演出自体を無音にしない。
        // 8枚が短時間に続くため、独立チャンネルで最後まで重ねて鳴らす。
        if(soundEffects_.Has("hand"))soundEffects_.PlayPolyphonic("hand",8);
        else soundEffects_.PlayBang();
        break;
    case story::PowerOutage::Request::FishImpact:soundEffects_.PlayBurst();break;
    case story::PowerOutage::Request::PowerRestored:
        soundEffects_.StopGenerated();
        eventDialogue_.Start(storyFolder_/"power_restored.dialogue",false);break;
    default:break;
    }
    soundEffects_.UpdateFootsteps(settings_.paused?0.f:frame.deltaTime,
        settings_.cameraPositionX,settings_.cameraPositionZ,settings_.cameraYaw);
    if(archChase_.GameOver()&&advance){
        BeginSceneTransition({player::GameMenu::RequestType::ReturnTitle,-1});
    }
    updateBackgroundMusic();
    if(powerOutage_.Restored()&&!eventDialogue_.Active())opening_.SetPowerRestored();
    const bool consoleWasActive=tankLightingConsole_.Active();
    tankLightingConsole_.Update(frame.deltaTime,settings_);
    switch(tankLightingConsole_.ConsumeFeedback()){
    case story::TankLightingConsole::Feedback::Select:soundEffects_.Play("select");break;
    case story::TankLightingConsole::Feedback::Save:
        ApplyHeroTankLightColor(tankLightingConsole_.AppliedColor());soundEffects_.Play("open");break;
    default:break;
    }
    if(terraceWasBlocking||terraceConversation_.BlocksPlayer())ResetPlayer();
    else if(consoleWasActive||tankLightingConsole_.Active())ResetPlayer();
    else if(archWasConstraining||archChase_.ConstrainsPlayer())
        playerManager_.SetControlledPose(
            {settings_.cameraPositionX,settings_.cameraPositionY,settings_.cameraPositionZ},
            settings_.cameraYaw,settings_.cameraPitch);
    const float managementTarget=managementDoorTargetOpen_?DirectX::XM_PIDIV2:0.f;
    const float managementStep=std::min(std::abs(managementTarget-managementDoorAngle_),frame.deltaTime*.92f);
    managementDoorAngle_+=managementTarget>managementDoorAngle_?managementStep:-managementStep;
    settings_.managementDoorAngle=managementDoorAngle_;
    settings_.managementDoorOpen=managementDoorTargetOpen_;
    if(managementDoorAngle_>DirectX::XMConvertToRadians(64.f)&&!managementCollisionOpened_) {
        receptionLobbyCollision_.SetBoxBounds(L"ManagementLockedDoor",{1000,1000,1000},{1001,1001,1001});
        managementCollisionOpened_=true;
    }
    if(settings_.managementDoorOpen&&settings_.cameraPositionX>21.15f&&
       settings_.cameraPositionZ>1.f&&settings_.cameraPositionZ<8.f)opening_.SetManagementEntered();
    const float staffTarget=staffDoorTargetOpen_?DirectX::XM_PIDIV2:0.f;
    const float staffStep=std::min(std::abs(staffTarget-staffDoorAngle_),frame.deltaTime*1.15f);
    staffDoorAngle_+=staffTarget>staffDoorAngle_?staffStep:-staffStep;
    settings_.staffDoorAngle=staffDoorAngle_;
    if(staffDoorAngle_>DirectX::XMConvertToRadians(64.f)&&!staffDoorCollisionOpened_){
        receptionLobbyCollision_.SetBoxBounds(L"Reception_StaffDoor",{1000,1000,1000},{1001,1001,1001});
        staffDoorCollisionOpened_=true;
    } else if(staffDoorAngle_<DirectX::XMConvertToRadians(8.f)&&staffDoorCollisionOpened_){
        receptionLobbyCollision_.SetBoxBounds(L"Reception_StaffDoor",{6.18f,-2.25f,-.07f},{7.32f,.70f,.03f});
        staffDoorCollisionOpened_=false;
    }
    if(!editorOpen_) UpdateLightingTuning(frame.deltaTime, input);
    if(editorOpen_) opening_.Editor(settings_);
    if(settings_.receptionLobbyMode && !editorOpen_)
        miniMap_.Draw(frame.deltaTime,settings_.cameraPositionX,settings_.cameraPositionY,
            settings_.cameraPositionZ,settings_.cameraYaw,opening_.MissionIndex(),
            !opening_.BlocksPlayer()&&!heroineEncounter_.BlocksPlayer()&&
                !eventDialogue_.BlocksPlayer()&&!gameMenu_.IsOpen()&&!passwordLock_.Active()&&!facilityPasswordLock_.Active()&&
                !terraceConversation_.BlocksPlayer()&&!archChase_.Active()&&
                !powerOutage_.BlackedOut()&&!powerOutage_.BlocksPlayer()&&
                !tankLightingConsole_.Active(),
            opening_.PowerMission()&&!opening_.ClueCollected());
    heroineEncounter_.Draw();
    archChase_.Draw();
    eventDialogue_.Draw();
    // 会話中に消すのはマップだけ。目的欄は失敗イベントの×を
    // セリフと同時に伝えるため、ヒロイン会話中も表示する。
    opening_.Draw(editorOpen_);
    passwordLock_.Draw();
    facilityPasswordLock_.Draw();
    tankLightingConsole_.Draw();
    gameMenu_.Draw();
    if(!settings_.paused&&!gameMenu_.IsOpen()&&!opening_.BlocksPlayer()&&
       !heroineEncounter_.BlocksPlayer()&&!eventDialogue_.BlocksPlayer())playTimeSeconds_+=frame.deltaTime;
    terraceConversation_.Draw(clueCollected_);
    DrawSceneFade();

    if (!settings_.paused)
    {
        simulationTime_ += frame.deltaTime;
    }
}

void AquariumScene::StartNewGame(){
    endingSequence_.reset();
    actuallyMusicStarted_=false;
    beachPreview_=beachMorning_=beachStoryMode_=false;
    beachArrivalLook_=false;beachArrivalLookTime_=0.f;
    beachArrivalDialoguePendingSit_=beachSitTransition_=beachSeated_=false;
    beachSeatedDialoguePendingChoice_=beachChoiceActive_=false;
    beachBranch_=BeachBranch::Undecided;
    beachStayDialoguePendingAftermath_=beachStayAftermathPendingEnding_=false;
    beachLeaveDialoguePendingTransition_=false;
    morningBeachTransition_=morningBeachEntered_=morningWake_=false;
    morningBeachTransitionTime_=morningWakeTime_=morningWakeBlur_=morningWakeBlink_=0.f;
    morningWakeDialoguePendingStand_=trueEpiloguePendingEnding_=false;
    morningStandTransition_=false;morningStandTime_=0.f;
    beachSitTransitionTime_=0.f;
    emergencyExitTransition_=emergencyExitBeachEntered_=false;emergencyExitTransitionTime_=0;
    settings_.emergencyExitDoorOpen=0.f;
    soundEffects_.Stop("beach");soundEffects_.Stop("transition");
    soundEffects_.Stop("inwater");StopMorningAmbience();
    SelectReceptionLobbyView();clueCollected_=managementCollisionOpened_=heroineJoined_=false;
    managementDoorTargetOpen_=false;managementDoorAngle_=0;playTimeSeconds_=0;
    heroineEncounter_.Reset();eventDialogue_.Reset();passwordLock_.Reset();facilityPasswordLock_.Reset();terraceConversation_.Reset();archChase_.Reset();powerOutage_.Reset();
    facilityBootDialoguePending_=false;
    tankLightingConsole_.Reset();ApplyHeroTankLightColor(story::TankLightingConsole::Color::Blue);
    gameMenu_.SetClueOwned(false);gameMenu_.HideTitle();opening_.Stop();ResetPlayer();
    inWaterIntro_=true;inWaterIntroTransition_=false;
    eventDialogue_.Start(storyFolder_/"inwater_intro.dialogue",false);
    inWaterIntroDialoguePending_=true;
    soundEffects_.Play("inwater",true);
}

player::SaveData AquariumScene::CaptureSave() const{
    player::SaveData d;d.position={settings_.cameraPositionX,settings_.cameraPositionY,settings_.cameraPositionZ};
    d.yaw=settings_.cameraYaw;d.pitch=settings_.cameraPitch;d.playSeconds=playTimeSeconds_;
    d.heroineJoined=heroineJoined_;d.clueCollected=clueCollected_;d.managementUnlocked=passwordLock_.Unlocked();d.archComplete=archChase_.Complete();
    d.blackoutStarted=powerOutage_.Started();d.blackoutWritingSeen=powerOutage_.WritingSeen();
    d.blackoutHandsSeen=powerOutage_.HandsSeen();
    d.blackoutFishSeen=powerOutage_.FishSeen();d.powerRestored=powerOutage_.Restored();
    d.terraceRestCompleted=opening_.RestCompleted();d.manualCollected=opening_.ManualCollected();
    d.facilityPasswordCollected=opening_.FacilityPasswordCollected();
    d.staffDoorOpen=staffDoorTargetOpen_;
    d.beachDecisionPoint=beachStoryMode_&&beachChoiceActive_;
    d.heroTankLightColor=static_cast<int>(tankLightingConsole_.AppliedColor());
    d.findingEmergency=opening_.FindingEmergency();d.managementEntered=opening_.ManagementEntered();
    d.location=d.beachDecisionPoint?"夜の浜辺・分岐直前":
        (settings_.cameraPositionY<-.2f?"B1F  水中アーチ / クラゲルーム":
        (settings_.cameraPositionY>3.f?"2F  上階展示フロア":"1F  大水槽フロア"));
    return d;
}

void AquariumScene::ApplySave(const player::SaveData& d){
    endingSequence_.reset();
    actuallyMusicStarted_=false;
    beachPreview_=beachMorning_=beachStoryMode_=false;
    beachArrivalLook_=false;beachArrivalLookTime_=0.f;
    beachArrivalDialoguePendingSit_=beachSitTransition_=beachSeated_=false;
    beachSeatedDialoguePendingChoice_=beachChoiceActive_=false;
    beachBranch_=BeachBranch::Undecided;
    beachStayDialoguePendingAftermath_=beachStayAftermathPendingEnding_=false;
    beachLeaveDialoguePendingTransition_=false;
    morningBeachTransition_=morningBeachEntered_=morningWake_=false;
    morningBeachTransitionTime_=morningWakeTime_=morningWakeBlur_=morningWakeBlink_=0.f;
    morningWakeDialoguePendingStand_=trueEpiloguePendingEnding_=false;
    morningStandTransition_=false;morningStandTime_=0.f;
    beachSitTransitionTime_=0.f;
    emergencyExitTransition_=emergencyExitBeachEntered_=false;emergencyExitTransitionTime_=0;
    settings_.emergencyExitDoorOpen=0.f;
    soundEffects_.Stop("beach");soundEffects_.Stop("transition");
    soundEffects_.Stop("inwater");StopMorningAmbience();
    inWaterIntro_=inWaterIntroDialoguePending_=inWaterIntroTransition_=false;
    SelectReceptionLobbyView();opening_.RestoreProgress(d.heroineJoined,d.findingEmergency,d.clueCollected,d.managementEntered,d.powerRestored,
        d.terraceRestCompleted,d.manualCollected,d.facilityPasswordCollected);
    heroineEncounter_.Reset();eventDialogue_.Reset();terraceConversation_.Reset();archChase_.Reset();passwordLock_.Reset();facilityPasswordLock_.Reset();
    facilityBootDialoguePending_=false;
    heroineJoined_=d.heroineJoined;clueCollected_=d.clueCollected;playTimeSeconds_=d.playSeconds;
    if(heroineJoined_){heroineEncounter_.MarkComplete();terraceConversation_.RestoreAfterEncounter();}
    if(d.archComplete)archChase_.MarkComplete();if(d.managementUnlocked)passwordLock_.ForceUnlocked();
    if(d.facilityPasswordCollected)facilityPasswordLock_.ForceUnlocked();
    powerOutage_.RestoreProgress(d.blackoutStarted,d.blackoutWritingSeen,
        d.blackoutHandsSeen,d.blackoutFishSeen,d.powerRestored);
    const auto savedLight=static_cast<story::TankLightingConsole::Color>(std::clamp(d.heroTankLightColor,0,2));
    tankLightingConsole_.Reset(savedLight);ApplyHeroTankLightColor(savedLight);
    staffDoorTargetOpen_=d.staffDoorOpen&&d.powerRestored;
    staffDoorAngle_=staffDoorTargetOpen_?DirectX::XM_PIDIV2:0.f;
    settings_.staffDoorAngle=staffDoorAngle_;
    if(staffDoorTargetOpen_){
        receptionLobbyCollision_.SetBoxBounds(L"Reception_StaffDoor",{1000,1000,1000},{1001,1001,1001});
        staffDoorCollisionOpened_=true;
    }
    settings_.managementDoorUnlocked=d.managementUnlocked;settings_.cluePaperVisible=!d.clueCollected;
    settings_.manualPapersVisible=d.terraceRestCompleted&&!d.manualCollected;
    if(d.beachDecisionPoint){
        beachPreview_=true;beachMorning_=false;beachStoryMode_=true;
        beachSeated_=true;beachChoiceActive_=true;
        // 分岐直前なら「凪沙……なのか？」のBGMキューは通過済み。
        // 保存済みの旧データにも追加項目なしでactually.mp3を復元できる。
        actuallyMusicStarted_=true;
        playBeachAmbience(false);
        beachPlayer_.Reset(d.position,d.yaw,d.pitch);
        settings_.cameraPositionX=d.position.x;settings_.cameraPositionY=d.position.y;settings_.cameraPositionZ=d.position.z;
        settings_.cameraYaw=d.yaw;settings_.cameraPitch=d.pitch;
        gameMenu_.SetClueOwned(d.clueCollected);gameMenu_.HideTitle();
        return;
    }
    gameMenu_.SetClueOwned(d.clueCollected);settings_.cameraPositionX=d.position.x;settings_.cameraPositionY=d.position.y;settings_.cameraPositionZ=d.position.z;
    settings_.cameraYaw=d.yaw;settings_.cameraPitch=d.pitch;gameMenu_.HideTitle();ResetPlayer();
}

void AquariumScene::ProcessMenuRequest(){
    const auto r=gameMenu_.ConsumeRequest();using T=player::GameMenu::RequestType;
    if(r.type==T::Save&&!transitionFade_.Active()) {
        saveSystem_.Save(r.slot,CaptureSave());gameMenu_.SetSlots(saveSystem_.Inspect());
    } else if(r.type==T::NewGame||r.type==T::Load||r.type==T::ReturnTitle||r.type==T::Quit) {
        BeginSceneTransition(r);
    }
}

void AquariumScene::BeginSceneTransition(player::GameMenu::Request request) {
    if(pendingTransition_.type!=player::GameMenu::RequestType::None)return;
    if(transitionFade_.BeginOut())pendingTransition_=request;
}

bool AquariumScene::UpdateSceneTransition(float deltaTime) {
    return transitionFade_.Update(deltaTime,[this]{
        if(inWaterIntroTransition_)FinishInWaterIntro();
        else ApplySceneTransition();
    });
}

void AquariumScene::ApplySceneTransition() {
    const auto request=pendingTransition_;
    pendingTransition_={};
    using T=player::GameMenu::RequestType;
    if(request.type==T::NewGame)StartNewGame();
    else if(request.type==T::Load) {
        player::SaveData data;
        if(saveSystem_.Load(request.slot,data))ApplySave(data);
    } else if(request.type==T::ReturnTitle) {
        endingSequence_.reset();
        actuallyMusicStarted_=false;
        opening_.Stop();archChase_.Reset();soundEffects_.StopGenerated();
        soundEffects_.Stop("beach");soundEffects_.Stop("transition");soundEffects_.Stop("inwater");
        StopMorningAmbience();inWaterIntro_=inWaterIntroDialoguePending_=inWaterIntroTransition_=false;
        applyTitlePresentation();
        gameMenu_.ShowTitle();
    } else if(request.type==T::Quit) {
        soundEffects_.StopGenerated();soundEffects_.Stop("beach");
        soundEffects_.Stop("transition");soundEffects_.Stop("inwater");
        StopMorningAmbience();PostQuitMessage(0);
    }
}

void AquariumScene::DrawSceneFade() const {
    const float alpha=transitionFade_.Alpha();
    if(alpha<=.001f)return;
    const ImVec2 size=ImGui::GetIO().DisplaySize;
    ImGui::GetForegroundDrawList()->AddRectFilled(
        {0,0},size,IM_COL32(0,0,0,int(std::clamp(alpha,0.f,1.f)*255.f)));
}

void AquariumScene::DrawInWaterStill() const {
    if(!inWaterStill_.view||!inWaterStill_.width||!inWaterStill_.height)return;
    const ImVec2 screen=ImGui::GetIO().DisplaySize;
    const float screenAspect=screen.x/std::max(screen.y,1.f);
    const float imageAspect=float(inWaterStill_.width)/inWaterStill_.height;
    ImVec2 uv0{0,0},uv1{1,1};
    if(imageAspect>screenAspect){
        const float shown=screenAspect/imageAspect;
        uv0.x=(1.f-shown)*.5f;uv1.x=1.f-uv0.x;
    } else {
        const float shown=imageAspect/screenAspect;
        uv0.y=(1.f-shown)*.5f;uv1.y=1.f-uv0.y;
    }
    ImGui::GetBackgroundDrawList()->AddImage(ImTextureRef(inWaterStill_.view.Get()),
        {0,0},screen,uv0,uv1,IM_COL32_WHITE);
}

void AquariumScene::FinishInWaterIntro() {
    inWaterIntro_=inWaterIntroDialoguePending_=inWaterIntroTransition_=false;
    soundEffects_.Stop("inwater");eventDialogue_.Reset();
    opening_.Start();
    // 暗転の頂点で起床カメラと閉じたまぶたを適用してから館内へ切り替える。
    opening_.Tick(0.f,false,false,settings_);
    ResetPlayer();
}

void AquariumScene::BuildBeachCollision()
{
    using physics::ColliderTag;using physics::CollisionLayer;using physics::LayerMask;
    beachCollision_.Clear();
    // The visible shoreline meanders between x=-1.5 and x=1.8. Keeping the
    // playable strip inside x=2.1 prevents even the capsule edge entering water.
    beachCollision_.AddWalkableRect({L"BeachWhiteSand",2.1f,6.65f,-5.f,5.f,.06f,
        ColliderTag::Walkable,LayerMask(CollisionLayer::World)});
}

void AquariumScene::DrawBeachChoice()
{
    const ImVec2 screen=ImGui::GetIO().DisplaySize;
    aquariumUi::drawBackdrop(screen,128);
    aquariumUi::PanelStyle style;
    ImGui::SetNextWindowPos({screen.x*.5f,screen.y*.54f},ImGuiCond_Always,{.5f,.5f});
    ImGui::SetNextWindowSize({600,278});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{38,30});
    ImGui::Begin("##beach_branch",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
    aquariumUi::drawPanelAccent();
    ImGui::TextDisabled("選択してください");ImGui::Dummy({0,12});
    const ImVec2 buttonSize{ImGui::GetContentRegionAvail().x,50};
    if(aquariumUi::button("凪沙の手を取る",buttonSize,false,{.08f,.5f})){
        beachChoiceActive_=false;beachBranch_=BeachBranch::Stay;
        soundEffects_.Play("select");
        beachStayDialoguePendingAftermath_=
            eventDialogue_.Start(storyFolder_/"beach_stay.dialogue",false);
    }
    ImGui::Dummy({0,9});
    if(aquariumUi::button("凪沙に別れを告げる",buttonSize,false,{.08f,.5f})){
        beachChoiceActive_=false;beachBranch_=BeachBranch::Leave;
        soundEffects_.Play("select");
        beachLeaveDialoguePendingTransition_=
            eventDialogue_.Start(storyFolder_/"beach_leave.dialogue",false);
    }
    ImGui::End();ImGui::PopStyleVar();

    ImGui::SetNextWindowPos({screen.x-280,screen.y-82},ImGuiCond_Always);
    ImGui::SetNextWindowSize({260,56});
    ImGui::Begin("##beach_save_load",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
    if(aquariumUi::button("セーブ",{112,36})){
        gameMenu_.SetSlots(saveSystem_.Inspect());gameMenu_.OpenSavePage();
    }
    ImGui::SameLine();
    if(aquariumUi::button("ロード",{112,36})){
        gameMenu_.SetSlots(saveSystem_.Inspect());gameMenu_.OpenLoadPage();
    }
    ImGui::End();
}

void AquariumScene::BeginEmergencyExitTransition()
{
    if(!opening_.FacilityPasswordCollected()||emergencyExitTransition_||beachStoryMode_)return;
    emergencyExitTransition_=true;emergencyExitBeachEntered_=false;
    emergencyExitTransitionTime_=0;
    settings_.emergencyExitDoorOpen=0.f;
    soundEffects_.Play("open");
    soundEffects_.Play("transition");
}

void AquariumScene::EnterStoryBeach()
{
    endingSequence_.reset();
    actuallyMusicStarted_=false;
    beachPreview_=true;beachMorning_=false;beachStoryMode_=true;
    beachArrivalLook_=true;beachArrivalLookTime_=0.f;
    beachArrivalDialoguePendingSit_=beachSitTransition_=beachSeated_=false;
    beachSeatedDialoguePendingChoice_=beachChoiceActive_=false;
    beachBranch_=BeachBranch::Undecided;
    beachStayDialoguePendingAftermath_=beachStayAftermathPendingEnding_=false;
    beachLeaveDialoguePendingTransition_=false;
    morningBeachTransition_=morningBeachEntered_=morningWake_=false;
    morningBeachTransitionTime_=morningWakeTime_=morningWakeBlur_=morningWakeBlink_=0.f;
    morningWakeDialoguePendingStand_=trueEpiloguePendingEnding_=false;
    morningStandTransition_=false;morningStandTime_=0.f;
    beachSitTransitionTime_=0.f;
    StopMorningAmbience();
    playBeachAmbience(false);
    beachPlayer_.Reset({3.8f,.06f+beachPlayer_.Capsule().eyeHeight,0.f},-.68f,-.035f);
    const auto eye=beachPlayer_.EyePosition();
    settings_.cameraPositionX=eye.x;settings_.cameraPositionY=eye.y;settings_.cameraPositionZ=eye.z;
    settings_.cameraYaw=beachPlayer_.Yaw();settings_.cameraPitch=beachPlayer_.Pitch();
}

bool AquariumScene::UpdateEmergencyExitTransition(float deltaTime)
{
    if(!emergencyExitTransition_)return false;
    emergencyExitTransitionTime_+=std::clamp(deltaTime,0.f,.1f);
    const float doorPhase=std::clamp(emergencyExitTransitionTime_/.52f,0.f,1.f);
    settings_.emergencyExitDoorOpen=doorPhase*doorPhase*(3.f-2.f*doorPhase);
    if(!settings_.paused)simulationTime_+=deltaTime;
    if(emergencyExitTransitionTime_>=1.10f&&!emergencyExitBeachEntered_){
        emergencyExitBeachEntered_=true;EnterStoryBeach();
    }
    if(emergencyExitBeachEntered_&&emergencyExitTransitionTime_>=2.90f){
        emergencyExitTransition_=false;
        soundEffects_.Stop("transition");
        return false;
    }
    updateBackgroundMusic();
    return true;
}

void AquariumScene::BeginMorningBeachTransition()
{
    if(morningBeachTransition_||morningWake_)return;
    morningBeachTransition_=true;morningBeachEntered_=false;
    morningBeachTransitionTime_=0.f;
    morningWakeBlur_=morningWakeBlink_=0.f;
    morningWakeDialoguePendingStand_=trueEpiloguePendingEnding_=false;
    morningStandTransition_=false;morningStandTime_=0.f;
    actuallyMusicStarted_=false;
    StopMorningAmbience();
    soundEffects_.Play("transition");
}

bool AquariumScene::UpdateMorningBeachTransition(float deltaTime)
{
    if(!morningBeachTransition_)return false;
    morningBeachTransitionTime_+=std::clamp(deltaTime,0.f,.1f);
    if(!settings_.paused)simulationTime_+=deltaTime;
    if(morningBeachTransitionTime_>=1.05f&&!morningBeachEntered_){
        morningBeachEntered_=true;beachMorning_=true;
        playBeachAmbience(true);
        beachPlayer_.SetControlledPose({2.12f,.34f,.28f},-.78f,1.54f);
        // Update returns early while the destination white is fading. Sync the
        // render camera at peak white so the first revealed frame is already
        // lying on the beach, never the previous seated/standing viewpoint.
        const auto eye=beachPlayer_.EyePosition();
        settings_.cameraPositionX=eye.x;settings_.cameraPositionY=eye.y;
        settings_.cameraPositionZ=eye.z;settings_.cameraYaw=beachPlayer_.Yaw();
        settings_.cameraPitch=beachPlayer_.Pitch();
    }
    if(morningBeachEntered_&&morningBeachTransitionTime_>=2.85f){
        morningBeachTransition_=false;morningWake_=true;morningWakeTime_=0.f;
        morningWakeBlur_=1.f;morningWakeBlink_=1.f;
        soundEffects_.Stop("transition");
        StartMorningAmbience();
        return false;
    }
    updateBackgroundMusic();
    return true;
}

void AquariumScene::UpdateMorningWake(float deltaTime)
{
    morningWakeTime_+=std::clamp(deltaTime,0.f,.1f);
    const auto smooth=[](float value){
        value=std::clamp(value,0.f,1.f);return value*value*(3.f-2.f*value);
    };
    const auto pulse=[&](float begin,float closed,float end){
        if(morningWakeTime_<begin||morningWakeTime_>=end)return 0.f;
        if(morningWakeTime_<closed)return smooth((morningWakeTime_-begin)/(closed-begin));
        return 1.f-smooth((morningWakeTime_-closed)/(end-closed));
    };
    // Starts shut, then two unsteady blinks before the eyes stay open.
    if(morningWakeTime_<.62f)morningWakeBlink_=1.f-smooth(morningWakeTime_/.62f);
    else morningWakeBlink_=std::max(pulse(.78f,.98f,1.28f),pulse(1.48f,1.74f,2.12f));
    morningWakeBlur_=std::clamp((1.f-smooth((morningWakeTime_-.30f)/3.10f))*.92f+
        morningWakeBlink_*.22f,0.f,1.f);
    const float rise=smooth((morningWakeTime_-2.05f)/2.65f);
    const DirectX::XMFLOAT3 eye{2.12f+.48f*rise,.34f+.82f*rise,.28f+.10f*rise};
    beachPlayer_.SetControlledPose(eye,-.78f+.10f*rise,1.54f-1.575f*rise);
    if(morningWakeTime_>=5.05f){
        morningWake_=false;morningWakeBlur_=morningWakeBlink_=0.f;
        if(beachBranch_==BeachBranch::Stay){
            // ノーマルも同じ浜辺で目覚めるが、その後の記憶だけが朧げになる。
            beachStayAftermathPendingEnding_=
                eventDialogue_.Start(storyFolder_/"beach_stay_aftermath.dialogue",false);
        }else{
            morningWakeDialoguePendingStand_=true;
            eventDialogue_.Start(storyFolder_/"beach_morning_wake.dialogue",false);
        }
    }
}

void AquariumScene::UpdateMorningStand(float deltaTime)
{
    morningStandTime_+=std::clamp(deltaTime,0.f,.1f);
    float t=std::clamp(morningStandTime_/2.20f,0.f,1.f);
    t=t*t*(3.f-2.f*t);
    const float standingY=.06f+beachPlayer_.Capsule().eyeHeight;
    const DirectX::XMFLOAT3 eye{2.60f,1.16f+(standingY-1.16f)*t,.38f};
    const float pitch=-.035f-std::sin(t*DirectX::XM_PI)*.085f;
    beachPlayer_.SetControlledPose(eye,-.68f,pitch);
    if(morningStandTime_>=2.20f){
        morningStandTransition_=false;morningStandTime_=0.f;beachSeated_=false;
        beachPlayer_.Reset({2.60f,standingY,.38f},-.68f,-.035f);
        if(beachStoryMode_&&beachBranch_==BeachBranch::Leave){
            // 朝の波音を残したまま、退院後の再訪へつなぐ。
            trueEpiloguePendingEnding_=
                eventDialogue_.Start(storyFolder_/"beach_true_epilogue.dialogue",false);
        }
    }
}

// =========================================================
// メニュー設定を各音声プレイヤーへ反映
// =========================================================
void AquariumScene::updateAudioSettings()
{
    backgroundMusic_.setVolume(gameMenu_.bgmVolume());
    soundEffects_.setMasterVolume(gameMenu_.seVolume());
}

// =========================================================
// 現在のゲーム状態に対応するBGMへ更新
// =========================================================
void AquariumScene::updateBackgroundMusic()
{
    using Track=audio::BackgroundMusic::Track;
    Track track=Track::Aquarium;
    if(gameMenu_.IsTitle())track=Track::Title;
    else if(actuallyMusicStarted_)track=Track::Actually;
    else if(archChase_.PlayerCanRun())track=Track::Chase;
    else if(inWaterIntro_||beachPreview_||archChase_.Active()||archChase_.GameOver()||
            powerOutage_.BlackedOut()||emergencyExitTransition_)track=Track::Silent;
    backgroundMusic_.update(track);
}

// =========================================================
// 夜・昼の浜辺環境音切替
// =========================================================
void AquariumScene::playBeachAmbience(bool morning)
{
    soundEffects_.Stop("beach");
    soundEffects_.PlayFile("beach",morning?beachMorningSoundPath_:beachSoundPath_,true);
}

// =========================================================
// タイトル専用の水槽内カメラへ切替
// =========================================================
void AquariumScene::applyTitlePresentation()
{
    beachPreview_=beachMorning_=beachStoryMode_=false;
    // 大水槽の砂面近くから、水面の三灯と泳ぐ魚影を見上げる。
    settings_.cameraPositionX=-.65f;
    settings_.cameraPositionY=-.55f;
    settings_.cameraPositionZ=12.65f;
    settings_.cameraYaw=-.16f;
    settings_.cameraPitch=.78f;
}

// =========================================================
// タイトル背景の緩やかな視線移動
// =========================================================
void AquariumScene::updateTitlePresentation()
{
    settings_.cameraYaw=-.16f+std::sin(simulationTime_*.075f)*.055f;
    settings_.cameraPitch=.78f+std::sin(simulationTime_*.052f+1.2f)*.025f;
}

void AquariumScene::StartMorningAmbience()
{
    soundEffects_.Stop("gaya");soundEffects_.Stop("siren");
    soundEffects_.Play("gaya",false,0.f);
    soundEffects_.Play("siren",false,0.f);
    morningAmbienceFading_=true;morningAmbienceFadingOut_=false;
    morningAmbienceFadeTime_=morningAmbienceFadeOutTime_=0.f;
    morningAmbienceAppliedVolume_=0.f;
}

void AquariumScene::UpdateMorningAmbience(float deltaTime)
{
    if(morningAmbienceFadingOut_){
        morningAmbienceFadeOutTime_+=std::clamp(deltaTime,0.f,.1f);
        float t=std::clamp(morningAmbienceFadeOutTime_/3.f,0.f,1.f);
        t=t*t*(3.f-2.f*t);
        const float volume=morningAmbienceFadeOutStartVolume_*(1.f-t);
        if(morningAmbienceAppliedVolume_-volume>=.025f||t>=1.f){
            soundEffects_.SetVolume("gaya",volume);soundEffects_.SetVolume("siren",volume);
            morningAmbienceAppliedVolume_=volume;
        }
        if(t>=1.f){
            soundEffects_.Stop("gaya");soundEffects_.Stop("siren");
            morningAmbienceFadingOut_=false;
        }
        return;
    }
    if(!morningAmbienceFading_)return;
    morningAmbienceFadeTime_+=std::clamp(deltaTime,0.f,.1f);
    float t=std::clamp(morningAmbienceFadeTime_/7.5f,0.f,1.f);
    const float volume=t*t*(3.f-2.f*t);
    // MCI string commands are intentionally throttled so the slow fade does
    // not become a per-frame CPU/driver cost.
    if(volume-morningAmbienceAppliedVolume_>=.025f||t>=1.f){
        soundEffects_.SetVolume("gaya",volume);
        soundEffects_.SetVolume("siren",volume);
        morningAmbienceAppliedVolume_=volume;
    }
    if(t>=1.f)morningAmbienceFading_=false;
}

void AquariumScene::BeginMorningAmbienceFadeOut()
{
    morningAmbienceFading_=false;morningAmbienceFadingOut_=true;
    morningAmbienceFadeOutTime_=0.f;
    morningAmbienceFadeOutStartVolume_=std::max(morningAmbienceAppliedVolume_,0.f);
}

void AquariumScene::StopMorningAmbience()
{
    soundEffects_.Stop("gaya");soundEffects_.Stop("siren");
    morningAmbienceFading_=morningAmbienceFadingOut_=false;
    morningAmbienceFadeTime_=morningAmbienceFadeOutTime_=0.f;
    morningAmbienceAppliedVolume_=-1.f;
}

void AquariumScene::Render(const framework::RenderContext& context)
{
    if(beachPreview_){
        beachRenderer_.Render(context.deviceContext,context.backBuffer,context.width,context.height,
            simulationTime_,settings_.cameraPositionX,settings_.cameraPositionY,settings_.cameraPositionZ,
            settings_.cameraYaw,settings_.cameraPitch,beachMorning_?1.f:0.f,
            morningWakeBlur_,morningWakeBlink_);
        if(emergencyExitTransition_){
            const float progress=std::clamp((emergencyExitTransitionTime_-1.10f)/1.80f,0.f,1.f);
            exitPortalRenderer_.Render(context.deviceContext,context.backBuffer,context.width,context.height,
                progress,true,simulationTime_);
        }
        if(morningBeachTransition_){
            const float progress=morningBeachEntered_
                ?std::clamp((morningBeachTransitionTime_-1.05f)/1.80f,0.f,1.f)
                :std::clamp(morningBeachTransitionTime_/1.05f,0.f,1.f);
            exitPortalRenderer_.Render(context.deviceContext,context.backBuffer,context.width,context.height,
                progress,morningBeachEntered_,simulationTime_);
        }
        storyFlowEditor_.CaptureScene(context.deviceContext,context.backBuffer,context.width,context.height);
        return;
    }
    renderer_.Render(
        context.deviceContext,
        context.backBuffer,
        context.width,
        context.height,
        simulationTime_,
        context.deltaTime,
        settings_);
    if(emergencyExitTransition_){
        const float progress=std::clamp((emergencyExitTransitionTime_-.08f)/1.02f,0.f,1.f);
        exitPortalRenderer_.Render(context.deviceContext,context.backBuffer,context.width,context.height,
            progress,false,simulationTime_);
    }
    storyFlowEditor_.CaptureScene(context.deviceContext,context.backBuffer,context.width,context.height);
}

framework::SceneDiagnostics AquariumScene::GetDiagnostics() const
{
    framework::SceneDiagnostics diagnostics;
    diagnostics.viewLabel = beachPreview_?(beachStoryMode_?(beachMorning_?L"DAY BEACH":L"NIGHT BEACH"):
        (beachMorning_?L"DAY BEACH PREVIEW":L"NIGHT BEACH PREVIEW")):(settings_.receptionLobbyMode
        ? L"ROUTE 09: RECEPTION + HERO TANK V3"
        : (settings_.gameLayoutV3Mode
        ? L"ROUTE 08: UNIFIED AQUARIUM V3"
        : (settings_.continuousMapMode
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
                : L"UNDERWATER VIEW"))))))));
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

void AquariumScene::ApplyHeroTankLightColor(story::TankLightingConsole::Color color)
{
    using Color=story::TankLightingConsole::Color;
    auto& rig=settings_.heroTankLighting;
    rig.alternateEnabled=color==Color::White;
    if(color==Color::White)rig.alternateColor=rig.whiteColor;
    else rig.alternateColor=rig.defaultColor;
}

void AquariumScene::SelectReceptionLobbyView()
{
    heroineEncounter_.Reset();
    terraceConversation_.Reset();archChase_.Reset();powerOutage_.Reset();heroineJoined_=false;
    eventDialogue_.Reset();passwordLock_.Reset();facilityPasswordLock_.Reset();gameMenu_.Close();gameMenu_.SetClueOwned(false);
    facilityBootDialoguePending_=false;
    clueCollected_=managementCollisionOpened_=managementDoorTargetOpen_=false;
    staffDoorTargetOpen_=staffDoorCollisionOpened_=false;staffDoorAngle_=0.f;
    managementDoorAngle_=0.f;
    settings_.cluePaperVisible=true;settings_.cluePaperHighlighted=false;
    settings_.managementDoorUnlocked=false;settings_.managementDoorOpen=false;settings_.managementDoorAngle=0.f;
    settings_.manualPapersVisible=false;settings_.manualPapersHighlighted=false;settings_.staffDoorAngle=0.f;
    settings_.emergencyExitDoorOpen=0.f;
    settings_.powerOutage=settings_.blackoutPredatorVisibility=settings_.blackoutPredatorApproach=0.f;
    receptionLobbyCollision_.SetBoxBounds(L"ManagementLockedDoor",{20.89f,3.25f,4.07f},{21.11f,6.45f,6.93f});
    receptionLobbyCollision_.SetBoxBounds(L"Reception_StaffDoor",{6.18f,-2.25f,-.07f},{7.32f,.70f,.03f});
    terraceDoor_.SetOpen(false,receptionLobbyCollision_);
    settings_.terraceDoorOpen=false;settings_.terraceDoorAngle=0.f;
    settings_.viewMode = 1.0f;
    settings_.stageMode = true;
    settings_.greyboxMode = true;
    settings_.underwaterArchMode = false;
    settings_.watatsumiTankMode = false;
    settings_.continuousMapMode = false;
    settings_.gameLayoutV3Mode = false;
    settings_.receptionLobbyMode = true;

    // Enter just inside the locked exterior doors and face the three public
    // inspection desks. StageModel mirrors authored Z at import time.
    settings_.cameraPositionX = 0.0f;
    settings_.cameraPositionY =
        kStageFloorOffset + playerManager_.Capsule().eyeHeight;
    settings_.cameraPositionZ = -7.20f;
    settings_.cameraYaw = 0.0f;
    settings_.cameraPitch = -0.025f;

    ApplyReceptionHallLighting();

    opening_.Start();
    ResetPlayer();
}

void AquariumScene::ApplyReceptionHallLighting()
{
    // Route 09 uses one persistent rig.  Camera-position lighting swaps made
    // otherwise static architecture flash at the ramp/arch boundaries.  The
    // eight fixed practicals below remain resident for the whole route; their
    // finite ranges keep the closed aquarium dark without zone transitions.
    settings_.localLighting.lightCount = 8;
    settings_.localLighting.lights[0] = {
        {6.75f, 1.02f, -0.30f}, 5.2f,
        {-0.55f, -0.68f, -0.48f}, 0.82f,
        {0.12f, 0.72f, 0.34f}, 32.0f, 58.0f,
        lighting::LocalLightType::Point, true};
    settings_.localLighting.lights[1] = {
        {0.0f, 7.55f, 12.2f}, 14.0f,
        {0.0f, -1.0f, 0.0f}, 8.0f,
        {0.10f, 0.48f, 1.00f}, 25.0f, 48.0f,
        lighting::LocalLightType::Spot, true};
    settings_.localLighting.lights[2] = {
        {-5.8f, 5.7f, 13.4f}, 9.5f,
        {0.45f, -0.62f, -0.18f}, 2.8f,
        {0.06f, 0.32f, 0.92f}, 30.0f, 58.0f,
        lighting::LocalLightType::Spot, true};
    settings_.localLighting.lights[3] = {
        {5.9f, 5.5f, 13.8f}, 9.5f,
        {-0.45f, -0.58f, -0.18f}, 2.8f,
        {0.08f, 0.36f, 0.96f}, 30.0f, 58.0f,
        lighting::LocalLightType::Spot, true};
    settings_.localLighting.lights[4] = {
        {19.65f, -0.50f, 10.4f}, 8.5f,
        {-1.0f, -0.16f, 0.0f}, 2.6f,
        {0.06f, 0.34f, 1.00f}, 31.0f, 58.0f,
        lighting::LocalLightType::Spot, true};
    settings_.localLighting.lights[5] = {
        {0.0f, 6.85f, 5.45f}, 13.0f,
        {0.0f, -1.0f, 0.0f}, 2.1f,
        {0.08f, 0.30f, 0.72f}, 38.0f, 66.0f,
        lighting::LocalLightType::Spot, true};
    settings_.localLighting.lights[6] = {
        {-10.05f, 3.25f, 23.0f}, 11.0f,
        {0.0f, -0.82f, 0.22f}, 4.8f,
        {0.12f, 0.48f, 1.00f}, 26.0f, 58.0f,
        lighting::LocalLightType::Spot, true};
    settings_.localLighting.lights[7] = {
        {-10.05f, -3.8f, 88.0f}, 17.0f,
        {0.0f, -1.0f, 0.0f}, 1.65f,
        {0.09f, 0.25f, 0.92f}, 36.0f, 68.0f,
        lighting::LocalLightType::Point, true};
    settings_.localLighting.ambientColor = {0.018f, 0.050f, 0.078f};
    settings_.localLighting.ambientStrength = 0.060f;
    settings_.localLighting.tankBounceEnabled = true;
    settings_.localLighting.tankBounceCenter = {0.0f, 2.85f, 8.0f};
    settings_.localLighting.tankBounceNormal = {0.0f, 0.0f, -1.0f};
    settings_.localLighting.tankBounceRange = 18.0f;
    settings_.localLighting.tankBounceHalfWidth = 8.5f;
    settings_.localLighting.tankBounceHalfHeight = 5.1f;
    // Keep the tank glow on nearby walls, but do not wash the entire dry floor
    // into one saturated blue plane. The water shader carries the strong hue;
    // dry architecture only receives a softer, greener reflected component.
    settings_.localLighting.tankBounceColor = {0.022f, 0.115f, 0.235f};
    settings_.localLighting.tankBounceIntensity = 0.34f;
    settings_.localLighting.atmosphereEnabled = true;
    settings_.localLighting.atmosphereColor = {0.004f, 0.018f, 0.030f};
    settings_.localLighting.atmosphereDensity = 0.016f;
    settings_.localLighting.atmosphereStart = 5.5f;
    settings_.localLighting.atmosphereMaximum = 0.20f;
    settings_.localLighting.spatialCullingEnabled = true;
    settings_.exposure = 1.05f;
}

void AquariumScene::UpdatePlayer(
    float deltaTime,
    const framework::InputSystem& input)
{
    const physics::CollisionWorld* collisionWorld = nullptr;
    if (settings_.receptionLobbyMode)
    {
        collisionWorld = &receptionLobbyCollision_;
    }
    else if (settings_.gameLayoutV3Mode)
    {
        collisionWorld = &gameLayoutV3Collision_;
    }
    else if (settings_.underwaterArchMode)
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

    if(archChase_.UsesExtendedPath())
        playerManager_.UpdateExtendedArch(deltaTime,input,settings_.archExtension);
    else playerManager_.Update(
        deltaTime, input, collisionWorld, collisionWorld == nullptr);
    const DirectX::XMFLOAT3& eye = playerManager_.EyePosition();
    settings_.cameraPositionX = eye.x;
    settings_.cameraPositionY = eye.y;
    settings_.cameraPositionZ = eye.z;
    settings_.cameraYaw = playerManager_.Yaw();
    settings_.cameraPitch = playerManager_.Pitch();

    if(archChase_.UsesExtendedPath()) {
        if(archChase_.Escaped()&&settings_.cameraPositionZ+settings_.archExtension<25.5f){
            // Floating-origin handoff: camera and the entire visible 1F wing
            // move together, so their relative position never jumps.
            settings_.cameraPositionZ+=settings_.archExtension;
            settings_.archExtension=0;archChase_.CollapseExtension();ResetPlayer();
        }
        return;
    }

    if (settings_.receptionLobbyMode)
    {
        const auto ray=playerManager_.GetSelectionRay();
        // Broad handle region on the exterior double door. Runtime Y = authored Y - 2.25.
        player::InteractionTarget targets[10]{};
        int targetCount=0;
        const bool emergencyReady=opening_.FacilityPasswordCollected();
        const bool pcLockedForRest=powerOutage_.Restored()&&!opening_.RestCompleted();
        targets[targetCount++]={0,L"Reception_ExteriorGlassDoor",{-.85f,-1.65f,-8.60f},{.85f,-.20f,-8.20f},{.25f,-.90f,-8.18f},
            opening_.ExitChecked()?"開かない":"調べる",!opening_.ExitChecked()};
        targets[targetCount++]={9,L"JellyPanorama_FutureExit",{.63f,-6.80f,89.12f},{.94f,-3.72f,91.68f},{.62f,-5.35f,90.40f},
            emergencyReady?"非常口を開ける":"開かない",emergencyReady};
        targets[targetCount++]={1,L"TerraceDoorLeft",{-1.5f,3.25f,-.08f},{1.5f,6.45f,.08f},{.18f,4.55f,0},terraceDoor_.IsOpen()?"閉じる":"開く"};
        targets[targetCount++]={2,L"ManagementLockedDoor",{20.65f,3.65f,4.0f},{21.15f,5.8f,6.9f},{20.65f,4.6f,4.45f},!passwordLock_.Unlocked()?"パスワードを入力":(managementDoorTargetOpen_?"開いている":"開く"),!managementDoorTargetOpen_};
        targets[targetCount++]={4,L"V4_Monitor0",{22.45f,4.05f,6.55f},{23.55f,4.85f,7.05f},{23.f,4.62f,6.72f},
            powerOutage_.Restored()?"電力 復旧済み":(powerOutage_.BlackedOut()?"電力を復旧":"待機中"),
            passwordLock_.Unlocked()&&powerOutage_.BlackedOut()};
        targets[targetCount++]={5,L"Reception_StaffDoor",{6.05f,-2.25f,-.22f},{7.45f,.85f,.22f},{6.34f,-.95f,-.24f},
            !powerOutage_.Restored()?"停電でロックされている":(staffDoorTargetOpen_?"閉じる":"開く"),true};
        if(opening_.PowerMission()&&!clueCollected_)
            targets[targetCount++]={3,L"V4_ReefCluePaper",{25.62f,3.72f,9.70f},{26.38f,4.08f,10.30f},{26.0f,4.08f,10.0f},"拾う",true};
        if(opening_.RestCompleted()&&!opening_.ManualCollected())
            targets[targetCount++]={6,L"Reception_ManualSheet_A",{5.18f,-2.30f,-2.48f},{6.46f,-2.08f,-1.18f},{5.78f,-2.12f,-1.82f},"マニュアルを拾う",true};
        if(powerOutage_.Restored())
            targets[targetCount++]={7,L"V4_Monitor2",{28.45f,4.00f,6.50f},{29.55f,4.90f,7.10f},{29.f,4.62f,6.72f},
                pcLockedForRest?"ひとまず休もう":"大水槽照明を調整",!pcLockedForRest};
        if(powerOutage_.Restored())
            targets[targetCount++]={8,L"V4_Monitor1",{25.45f,4.00f,6.50f},{26.55f,4.90f,7.10f},{26.f,4.62f,6.72f},
                pcLockedForRest?"ひとまず休もう":opening_.FacilityPasswordCollected()?"非常口 起動済み":
                    (opening_.ManualCollected()?"起動パスワードを入力":"マニュアルが必要"),
                !pcLockedForRest&&opening_.ManualCollected()&&!opening_.FacilityPasswordCollected()};
        const int selected=player::FindInteraction(ray,receptionLobbyCollision_,targets,targetCount);
        if(selected>=0) {
            if(targets[selected].id==3)settings_.cluePaperHighlighted=true;
            if(targets[selected].id==6)settings_.manualPapersHighlighted=true;
            player::DrawInteraction(targets[selected],ray,settings_.cameraYaw);
            if(playerManager_.SelectionRequested() && targets[selected].actionable) {
                if(targets[selected].id==0) {
                    opening_.InspectExit();
                }
                else if(targets[selected].id==9)BeginEmergencyExitTransition();
                else if(targets[selected].id==1) terraceDoor_.Select(ray);
                else if(targets[selected].id==2) {
                    if(passwordLock_.Unlocked())managementDoorTargetOpen_=true;
                    else passwordLock_.Open();
                }
                else if(targets[selected].id==3) {
                    clueCollected_=true;settings_.cluePaperVisible=false;
                    opening_.SetClueCollected();gameMenu_.SetClueOwned(true);
                    eventDialogue_.Start(storyFolder_/"clue.dialogue",true);
                }
                else if(targets[selected].id==4)powerOutage_.ConfirmRestore();
                else if(targets[selected].id==5){
                    if(powerOutage_.Restored()){
                        staffDoorTargetOpen_=!staffDoorTargetOpen_;soundEffects_.Play("open");
                    } else soundEffects_.Play("miss");
                }
                else if(targets[selected].id==6){
                    opening_.SetManualCollected();settings_.manualPapersVisible=false;
                    soundEffects_.Play("select");
                }
                else if(targets[selected].id==7)tankLightingConsole_.Open(settings_);
                else if(targets[selected].id==8)facilityPasswordLock_.Open();
            }
        }
        if(selected<0&&heroineJoined_&&terraceConversation_.CanSit()){
            const player::InteractionTarget bench{
                4,L"V4_TerraceBench",{-4.30f,3.45f,-5.55f},{-1.70f,4.15f,-4.45f},
                {-3.0f,4.05f,-5.0f},"座る",true};
            if(player::FindInteraction(ray,receptionLobbyCollision_,&bench,1)==0){
                player::DrawInteraction(bench,ray,settings_.cameraYaw);
                if(playerManager_.SelectionRequested())terraceConversation_.StartSitting(
                    playerManager_.EyePosition(),playerManager_.Yaw(),playerManager_.Pitch(),
                    powerOutage_.Restored()&&!opening_.RestCompleted());
            }
        }
    }
    terraceDoor_.Update(deltaTime,receptionLobbyCollision_);
    settings_.terraceDoorOpen=terraceDoor_.IsOpen();
    settings_.terraceDoorAngle=terraceDoor_.Angle();
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
