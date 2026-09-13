#include "../src/audio/BackgroundMusic.h"
#include "../src/audio/SoundEffects.h"
#include <iostream>
#include <string>
#include <objbase.h>

bool Playing(const wchar_t* alias) {
    wchar_t mode[64]{};
    const auto error=mciSendStringW((std::wstring(L"status ")+alias+L" mode").c_str(),mode,64,nullptr);
    std::wcout<<alias<<L" mode="<<mode<<L" error="<<error<<L'\n';
    return error==0 && std::wstring(mode)==L"playing";
}
int Volume(const wchar_t* alias) {
    wchar_t value[32]{};
    const auto error=mciSendStringW((std::wstring(L"status ")+alias+
        L" volume").c_str(),value,32,nullptr);
    return error==0?_wtoi(value):-1;
}
int main() {
    // Match wWinMain: testing without explicit COM initialization missed the
    // MTA incompatibility of the mpegvideo driver used by the actual game.
    const HRESULT comResult=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(comResult)){std::cerr<<"COM STA initialization failed\n";return 10;}
    struct ComScope {~ComScope(){CoUninitialize();}} comScope;
    APTTYPE apartment;APTTYPEQUALIFIER qualifier;
    if(FAILED(CoGetApartmentType(&apartment,&qualifier)) ||
        (apartment!=APTTYPE_STA && apartment!=APTTYPE_MAINSTA))return 11;
    audio::BackgroundMusic music;
    music.Initialize(std::filesystem::absolute("asset/Sound/BGM"));
    music.Update(true,false);std::cout<<"Music error="<<music.LastError()<<'\n';
    if(music.CurrentTrack()!=L"stage")return 12;
    if(!Playing(L"aquarium_bgm"))return 1;
    music.Update(false,false);if(music.CurrentTrack()!=L"title"||!Playing(L"aquarium_bgm"))return 2;
    music.Update(false,true);if(mciGetDeviceIDW(L"aquarium_bgm"))return 3;
    music.Update(false,false);if(music.CurrentTrack()!=L"title"||!Playing(L"aquarium_bgm"))return 4;
    audio::SoundEffects se;
    se.Initialize(std::filesystem::absolute("asset/Sound/SE"));
    se.PlayFootsteps(0,0,0);if(!Playing(L"aquarium_se_footsound"))return 7;
    const float initialDistance=se.FootstepDistance();
    for(int i=0;i<180;++i)se.UpdateFootsteps(1.f/60,0,0,0);
    if(!se.FootstepsActive()||se.FootstepSpatialError()!=0||
        !(se.FootstepDistance()<initialDistance-2.f))return 13;
    se.Play("glass");if(!Playing(L"aquarium_se_glass")||!Playing(L"aquarium_bgm"))return 5;
    if(!Playing(L"aquarium_se_footsound"))return 8;
    se.StopGenerated();if(mciGetDeviceIDW(L"aquarium_se_footsound")||!Playing(L"aquarium_bgm"))return 9;
    if(!se.PlayBoot())return 14;
    se.PlayFile("beach",std::filesystem::absolute("asset/Sound/BGM/Beach.mp3"),true);
    if(!Playing(L"aquarium_se_beach"))return 15;
    se.Stop("beach");if(mciGetDeviceIDW(L"aquarium_se_beach"))return 16;
    se.Play("transition");if(!Playing(L"aquarium_se_transition"))return 22;
    se.Stop("transition");if(mciGetDeviceIDW(L"aquarium_se_transition"))return 23;
    se.Play("inwater",true);if(!Playing(L"aquarium_se_inwater"))return 24;
    se.Stop("inwater");if(mciGetDeviceIDW(L"aquarium_se_inwater"))return 25;
    se.Play("gaya",true,0.f);se.Play("siren",true,0.f);
    if(!Playing(L"aquarium_se_gaya")||!Playing(L"aquarium_se_siren"))return 17;
    if(Volume(L"aquarium_se_gaya")!=0||Volume(L"aquarium_se_siren")!=0)return 20;
    if(!se.SetVolume("gaya",.5f)||!se.SetVolume("siren",.5f))return 18;
    const int gayaVolume=Volume(L"aquarium_se_gaya");
    const int sirenVolume=Volume(L"aquarium_se_siren");
    std::cout<<"Morning ambience volume gaya="<<gayaVolume
        <<" siren="<<sirenVolume<<'\n';
    // Some MCI mpegvideo drivers quantize the requested 0..1000 volume.
    if(std::abs(gayaVolume-500)>10||std::abs(sirenVolume-500)>10)return 21;
    se.Stop("gaya");se.Stop("siren");
    if(mciGetDeviceIDW(L"aquarium_se_gaya")||mciGetDeviceIDW(L"aquarium_se_siren"))return 19;
    music.Update(true,false);if(music.CurrentTrack()!=L"stage"||!Playing(L"aquarium_bgm"))return 6;
    std::cout<<"Swapped title / gameplay / silence / resume / independent glass SE passed\n";
}
