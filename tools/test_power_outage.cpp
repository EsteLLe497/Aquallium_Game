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

    // 実際の登り経路: 下階入口で足音開始、書き文字で停止、その先で手形。
    settings.cameraPositionX=10.6f;settings.cameraPositionY=-.36f;settings.cameraPositionZ=3.2f;
    outage.Update(.016f,true,false,settings,settings);
    assert(settings.flashlightOn==1);
    assert(outage.ConsumeRequest()==PowerOutage::Request::FootstepsStart);
    settings.cameraPositionX=18.f;settings.cameraPositionZ=9.2f;
    outage.Update(.016f,true,false,settings,settings);
    assert(outage.WritingSeen());
    assert(outage.ConsumeRequest()==PowerOutage::Request::Bang);
    assert(settings.blackoutWriting>0);
    outage.Update(.016f,true,false,settings,settings);
    assert(outage.ConsumeRequest()==PowerOutage::Request::FootstepsStop);
    // 水槽横ではまだ手形もSEも発火しない。
    settings.cameraPositionY=-.1f;settings.cameraPositionZ=14.5f;
    outage.Update(.016f,true,false,settings,settings);
    assert(outage.ConsumeRequest()==PowerOutage::Request::None);
    assert(settings.blackoutHands<0.f);
    // 曲がり角後の奥スロープに入った瞬間、固定手形とSE要求を発火する。
    settings.cameraPositionX=10.8f;settings.cameraPositionY=1.1f;
    settings.cameraPositionZ=20.3f;
    outage.Update(.016f,true,false,settings,settings);
    assert(outage.ConsumeRequest()==PowerOutage::Request::None);
    assert(settings.blackoutHands>=0.f);
    int handSounds=0;
    for(int i=0;i<50;++i) {
        outage.Update(.02f,true,false,settings,settings);
        handSounds+=outage.ConsumeRequest()==PowerOutage::Request::Hands;
    }
    assert(handSounds==8);

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
    // 手形は短時間で消えず、停電中は壁面に残り続ける。
    assert(settings.blackoutHands>.99f);

    outage.ConfirmRestore();assert(!outage.BlocksPlayer());
    for(int i=0;i<140;++i)outage.Update(.016f,true,false,settings,settings);
    assert(outage.Restored());
    outage.Update(.016f,true,false,settings,settings);
    assert(settings.blackoutHands<0);
    assert(settings.blackoutWriting==0&&settings.powerOutage==0);
    assert(outage.ConsumeRequest()==PowerOutage::Request::PowerRestored);
    story::OpeningFlow flow;
    flow.BeginPowerMission();flow.SetPowerRestored();
    assert(flow.PowerRestored()&&flow.MissionIndex()==3);
    flow.RestoreProgress(true,false,true,true,true);assert(flow.MissionIndex()==3);
    PowerOutage levelCorridor;
    levelCorridor.RestoreProgress(true,false,false,false,false);
    settings.cameraPositionX=18;settings.cameraPositionY=-.36f;settings.cameraPositionZ=7.2f;
    levelCorridor.Update(.016f,true,false,settings,settings);
    assert(levelCorridor.ConsumeRequest()==PowerOutage::Request::FootstepsStart);
    assert(levelCorridor.ConsumeRequest()==PowerOutage::Request::Bang);
    assert(settings.blackoutWriting>0);
    std::cout<<"Power outage flow tests passed\n";
}
