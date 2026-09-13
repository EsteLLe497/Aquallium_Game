#include "../src/story/PowerOutage.h"
#include "../src/story/OpeningFlow.h"
#include <cassert>
#include <iostream>

using story::PowerOutage;

int main() {
    PowerOutage outage;
    AquariumSettings settings{};
    settings.cameraPositionX=0;settings.cameraPositionY=-.6f;settings.cameraPositionZ=5;
    outage.Update(.016f,true,false,settings,settings);
    assert(outage.BlackedOut());
    assert(outage.ConsumeRequest()==PowerOutage::Request::BlackoutDialogue);

    outage.Update(.1f,true,true,settings,settings);
    assert(settings.flashlightOn==0);
    outage.Update(.1f,true,false,settings,settings);
    assert(outage.BlackedOut());

    settings.cameraPositionX=12;settings.cameraPositionY=.4f;settings.cameraPositionZ=4;
    outage.Update(.016f,true,false,settings,settings);
    assert(settings.flashlightOn==1);
    assert(outage.ConsumeRequest()==PowerOutage::Request::None);
    settings.cameraPositionX=17.6f;settings.cameraPositionZ=9.2f;
    outage.Update(.016f,true,false,settings,settings);
    assert(outage.WritingSeen());
    assert(outage.ConsumeRequest()==PowerOutage::Request::Bang);
    assert(settings.blackoutWriting>0);
    for(int i=0;i<140;++i)outage.Update(.016f,true,false,settings,settings);
    assert(outage.ConsumeRequest()==PowerOutage::Request::FootstepsStart);

    settings.cameraPositionX=0;settings.cameraPositionY=4.9f;settings.cameraPositionZ=6.2f;
    outage.Update(.016f,true,false,settings,settings);
    assert(outage.FishSeen());
    outage.ConsumeRequest(); // ramp footsteps stop
    int impacts=0;
    for(int i=0;i<260;++i) {
        outage.Update(.016f,true,false,settings,settings);
        impacts+=outage.ConsumeRequest()==PowerOutage::Request::FishImpact;
    }
    assert(impacts==3);

    outage.ConfirmRestore();assert(!outage.BlocksPlayer());
    for(int i=0;i<140;++i)outage.Update(.016f,true,false,settings,settings);
    assert(outage.Restored());
    assert(settings.blackoutWriting==0&&settings.powerOutage==0);
    assert(outage.ConsumeRequest()==PowerOutage::Request::PowerRestored);
    story::OpeningFlow flow;
    flow.BeginPowerMission();flow.SetPowerRestored();
    assert(flow.PowerRestored()&&flow.MissionIndex()==3);
    flow.RestoreProgress(true,false,true,true,true);assert(flow.MissionIndex()==3);
    PowerOutage levelCorridor;
    levelCorridor.RestoreProgress(true,false,false,false);
    settings.cameraPositionX=18;settings.cameraPositionY=-.36f;settings.cameraPositionZ=7.2f;
    levelCorridor.Update(.016f,true,false,settings,settings);
    assert(levelCorridor.ConsumeRequest()==PowerOutage::Request::Bang);
    assert(settings.blackoutWriting>0);
    std::cout<<"Power outage flow tests passed\n";
}
