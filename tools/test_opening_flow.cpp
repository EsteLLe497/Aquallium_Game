// Standalone: cl /std:c++17 /EHsc /utf-8 /I src tools/test_opening_flow.cpp
// Run from the project directory. Does not create a renderer or GPU device.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../src/story/OpeningFlow.h"
#include "../src/player/InteractionUI.h"
#include <cassert>
#include <iostream>
int main() {
    const player::SelectionRay ray{{0,0,0},{0,0,-1}};
    assert(player::RayBox(ray,{-1,-1,-3},{1,1,-2})==2);
    assert(player::RayBox(ray,{-1,-1,-5},{1,1,-4})<0); // too far
    assert(player::RayBox(ray,{-1,-1,2},{1,1,3})<0); // behind
    assert(player::RayBox(ray,{2,-1,-3},{3,1,-2})<0); // not looking at it
    physics::CollisionWorld screenedWorld;
    screenedWorld.AddBox({L"Reception_AcrylicScreen_1",{-.5f,-1.f,-1.6f},
        {.5f,1.f,-1.4f},physics::ColliderTag::Glass,
        physics::LayerMask(physics::CollisionLayer::World)});
    const player::InteractionTarget protectedManual{6,L"Reception_ManualSheet_A",
        {-.5f,-1.f,-2.6f},{.5f,1.f,-2.4f},{0,0,-2.5f},"拾う",true};
    assert(player::FindInteraction(ray,screenedWorld,&protectedManual,1)<0);
    physics::CollisionWorld beachWorld;
    beachWorld.AddWalkableRect({L"BeachWhiteSand",2.1f,6.65f,-5.f,5.f,.06f,
        physics::ColliderTag::Walkable,physics::LayerMask(physics::CollisionLayer::World)});
    physics::CharacterCapsule beachCapsule;
    physics::CharacterState beachWalker{{2.2f,.06f+beachCapsule.eyeHeight,0}};
    beachWorld.MoveCharacter(beachWalker,{-2.f,0,0},beachCapsule);
    assert(beachWalker.eyePosition.x>=2.1f); // Ocean side is never walkable.
    beachWalker.eyePosition={6.55f,.06f+beachCapsule.eyeHeight,0};
    beachWorld.MoveCharacter(beachWalker,{2.f,0,0},beachCapsule);
    assert(beachWalker.eyePosition.x<=6.65f); // Inland dune stays scenery-only.
    beachWalker.eyePosition={3.8f,.06f+beachCapsule.eyeHeight,4.9f};
    beachWorld.MoveCharacter(beachWalker,{0,0,2.f},beachCapsule);
    assert(beachWalker.eyePosition.z<=5.f); // The playable shoreline is 10 m long.
    beachWalker.eyePosition={3.8f,.06f+beachCapsule.eyeHeight,-4.9f};
    beachWorld.MoveCharacter(beachWalker,{0,0,-2.f},beachCapsule);
    assert(beachWalker.eyePosition.z>=-5.f);
    story::OpeningFlow flow;
    AquariumSettings settings;
    flow.Load("asset/story"); flow.Start();
    assert(flow.ControlsCamera());
    flow.Tick(0,false,false,settings);
    assert(settings.wakeBlur==1);
    flow.Tick(6,false,false,settings); // dialogue holds at 5 seconds
    const float seatedY=settings.cameraPositionY;
    flow.Tick(100,false,true,settings); // editor must pause progression
    assert(settings.cameraPositionY==seatedY && flow.ControlsCamera());
    // Follow the authored script, which can grow without changing this test.
    std::ifstream openingScript("asset/story/opening.dialogue");
    std::string authoredLine;int messageCount=0;
    while(std::getline(openingScript,authoredLine))
        if(!authoredLine.empty()&&authoredLine[0]!='#')++messageCount;
    for(int message=0;message<messageCount;++message) {
        flow.Tick(0,true,false,settings);
        assert(flow.ControlsCamera());
        flow.Tick(0,true,false,settings);
    }
    flow.Tick(10,false,false,settings);
    assert(!flow.ControlsCamera());
    assert(settings.cameraPositionY>seatedY && settings.wakeBlur==0);
    assert(!flow.ExitChecked());
    settings.cameraPositionX=0;settings.cameraPositionZ=-7.4f;
    settings.cameraPositionY=3.25f;
    flow.Tick(0,false,false,settings);assert(!flow.ExitChecked());
    settings.cameraPositionY=-.36f;
    flow.Tick(0,false,false,settings);assert(!flow.ExitChecked()); // proximity alone is insufficient
    flow.InspectExit(); assert(flow.ExitChecked() && flow.BlocksPlayer());
    const auto exitPosition=settings.cameraPositionZ;
    flow.Tick(0,false,false,settings);assert(settings.cameraPositionZ==exitPosition);
    for(int message=0;message<2;++message) {
        flow.Tick(0,true,false,settings);flow.Tick(0,true,false,settings);
    }
    assert(flow.FindingEmergency() && !flow.BlocksPlayer());
    flow.MarkEmergencyFailed();assert(flow.MissionIndex()==1);
    flow.BeginPowerMission();assert(flow.MissionIndex()==2 && !flow.FindingEmergency());
    assert(flow.PowerMission() && !flow.ClueCollected());
    flow.SetClueCollected();assert(flow.ClueCollected());
    flow.SetPowerRestored();assert(flow.MissionIndex()==3 && !flow.RestCompleted());
    flow.CompleteTerraceRest();assert(flow.MissionIndex()==4 && flow.RestCompleted());
    assert(!flow.ManualCollected() && !flow.FacilityPasswordCollected());
    flow.SetManualCollected();assert(flow.ManualCollected());
    flow.SetFacilityPasswordCollected();
    assert(flow.FacilityPasswordCollected()&&flow.MissionIndex()==1);
    flow.RestoreProgress(true,false,true,true,true,true,true,false);
    assert(flow.MissionIndex()==4 && flow.ManualCollected() &&
           !flow.FacilityPasswordCollected());
    flow.Start();assert(!flow.ExitChecked() && flow.ControlsCamera());
    assert(!flow.PowerMission() && !flow.ClueCollected());
    flow.Stop();assert(!flow.ControlsCamera());
    std::cout<<"Opening flow checks passed\n";
}
