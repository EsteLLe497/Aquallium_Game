#include "../src/story/ArchChase.h"
#include "../src/story/ArchGeometry.h"
#include "../src/player/PlayerManager.h"
#include "../src/framework/input.h"
#include <cassert>
#include <cmath>
#include <iostream>
// Deterministic input backend: exercises real PlayerManager without OS focus.
static bool forward=false,sprint=false,right=false;
namespace framework {
InputSystem::InputSystem(HWND){} InputSystem::~InputSystem(){}
bool InputSystem::IsDown(int k)const{return (k=='W'&&forward)||(k==VK_SHIFT&&sprint)||(k=='D'&&right);}
bool InputSystem::WasPressed(int)const{return false;}
}
int main(){
    using namespace story;
    constexpr float dt=1.f/120, speed=2.35f*1.85f;
    AquariumSettings s; s.cameraPositionX=-10.05f;s.cameraPositionZ=arch::loopTrigger+.1f;
    s.cameraYaw=3.14159265f;
    ArchChase chase;framework::InputSystem input;player::PlayerManager player;
    s.cameraPositionY=arch::Floor(s.cameraPositionZ)+player.Capsule().eyeHeight;
    player.Reset({s.cameraPositionX,s.cameraPositionY,s.cameraPositionZ},s.cameraYaw,0);
    chase.Update(dt,true,true,false,s);
    assert(!chase.UsesExtendedPath());
    s.cameraPositionZ=arch::loopTrigger-.1f;
    s.cameraPositionY=arch::Floor(s.cameraPositionZ)+player.Capsule().eyeHeight;
    player.SetControlledPose({s.cameraPositionX,s.cameraPositionY,s.cameraPositionZ},s.cameraYaw,0);
    chase.Update(dt,true,true,false,s);
    assert(s.cameraPositionZ>arch::seam); // entered before passing the old seam
    // Regression: scene must select extended movement BEFORE any extension
    // exists; waiting for extension>0 leaves the first step in old collision.
    assert(s.archExtension==0.f && chase.UsesExtendedPath());
    float start=s.cameraPositionZ, previous=start, originalDistance=start-arch::entrance;
    forward=true;
    for(int i=0;i<2000&&!chase.BlocksPlayer();++i){
        assert(chase.UsesExtendedPath());
        player.UpdateExtendedArch(dt,input,s.archExtension);
        s.cameraPositionZ=player.EyePosition().z;s.cameraPositionY=player.EyePosition().y;
        chase.Update(dt,true,true,false,s);
        assert(s.cameraPositionZ<=previous+.0001f); // no teleport / position wrap
        assert(std::abs(s.cameraPositionZ-(arch::entrance-s.archExtension)-originalDistance)<.002f);
        previous=s.cameraPositionZ;
    }
    assert(chase.BlocksPlayer()&&start-s.cameraPositionZ>=30.f);
    assert(chase.ConsumeRequest()==ArchChase::Request::StagnationDialogue);
    bool dialogue=false;
    for(int i=0;i<2000&&!chase.PlayerCanRun();++i){
        chase.Update(dt,true,false,dialogue,s);dialogue=false;
        const auto r=chase.ConsumeRequest();
        if(r==ArchChase::Request::CreakDialogue||r==ArchChase::Request::EscapeDialogue)dialogue=true;
        // The initial wait already has a dialogue request consumed above.
        if(i==0)dialogue=true;
        player.SetControlledPose({s.cameraPositionX,s.cameraPositionY,s.cameraPositionZ},s.cameraYaw,s.cameraPitch);
    }
    assert(chase.PlayerCanRun()&&!chase.BlocksPlayer());
    auto failed=chase;auto failureSettings=s;
    bool impacted=false;
    for(int i=0;i<2400&&!failed.GameOver();++i){
        const bool wasRunning=failed.PlayerCanRun();
        failed.Update(dt,true,false,false,failureSettings);
        const auto request=failed.ConsumeRequest();
        if(wasRunning)assert(request!=ArchChase::Request::Creak);
        impacted|=request==ArchChase::Request::Impact;
    }
    assert(impacted&&failed.GameOver());
    const float budget=arch::EscapeSeconds(s.cameraPositionZ,s.archExtension,speed);
    float running=0;sprint=true;
    while(chase.PlayerCanRun()&&running<budget+1){
        player.UpdateExtendedArch(dt,input,s.archExtension);
        s.cameraPositionZ=player.EyePosition().z;s.cameraPositionY=player.EyePosition().y;
        chase.Update(dt,true,true,false,s);running+=dt;
    }
    assert(!chase.GameOver()&&budget-running>1.4f&&budget-running<1.6f);
    ArchChase::Request aftermath=ArchChase::Request::None;
    for(int i=0;i<300&&aftermath==ArchChase::Request::None;++i){
        chase.Update(dt,true,false,false,s);aftermath=chase.ConsumeRequest();
    }
    assert(aftermath==ArchChase::Request::AftermathDialogue&&chase.BlocksPlayer());
    chase.Update(dt,true,false,true,s);
    chase.Update(dt,true,false,false,s);
    assert(chase.Complete());
    // Both joins of the repeated span are vertically continuous.
    for(float e:{.1f,12.f,30.f}){
        assert(std::abs(arch::Floor(arch::seam-e-.0001f,e)-arch::Floor(arch::seam-e+.0001f,e))<.001f);
        assert(std::abs(arch::Floor(arch::seam-.0001f,e)-arch::Floor(arch::seam+.0001f,e))<.001f);
    }
    std::cout<<"PASS: 30m continuous movement, dialogue release, sprint escape, timeout game-over, seamless floor joins. Budget="<<budget<<" sprint="<<running<<" margin="<<budget-running<<"\n";
}
