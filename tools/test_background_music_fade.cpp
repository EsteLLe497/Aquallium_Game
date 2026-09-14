// =========================================================
// ファイルの情報[test_background_music_fade.cpp]
//
// 制作者:Masatora Tanaka        日付：2026/09/14
// =========================================================
#include "../src/audio/BackgroundMusic.h"

#include <windows.h>
#include <mmsystem.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
int ReadVolume()
{
    wchar_t value[32]{};
    const MCIERROR error=mciSendStringW(
        L"status aquarium_bgm volume",value,32,nullptr);
    return error==0?_wtoi(value):-1;
}

void Advance(audio::BackgroundMusic& music,float seconds)
{
    constexpr float step=.05f;
    for(float elapsed=0.f;elapsed<seconds;elapsed+=step)music.advance(step);
}
}

// =========================================================
// フェードイン・曲変更・フェードアウト検証
// =========================================================
int main()
{
    const HRESULT comResult=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(comResult))return 10;
    struct ComScope{~ComScope(){CoUninitialize();}} comScope;

    using Track=audio::BackgroundMusic::Track;
    audio::BackgroundMusic music;
    music.initialize(std::filesystem::absolute("asset/sound/BGM"));
    music.setVolume(1.f);

    music.update(Track::Title);music.advance(.016f);
    if(music.currentTrack()!=Track::Title||ReadVolume()>10)return 1;
    Advance(music,1.f);
    if(std::abs(ReadVolume()-1000)>12)return 2;

    music.update(Track::Aquarium);music.advance(.25f);
    const int fadingOutVolume=ReadVolume();
    std::cout<<"Fade-out volume="<<fadingOutVolume<<'\n';
    // MCIドライバーは指定値を対数音量へ量子化するため、途中値は単調減少だけを見る。
    if(fadingOutVolume<=10||fadingOutVolume>=990)return 3;
    Advance(music,1.7f);
    if(music.currentTrack()!=Track::Aquarium||
       std::abs(ReadVolume()-1000)>12)return 4;

    music.update(Track::Silent);music.advance(.25f);
    const int stoppingVolume=ReadVolume();
    if(stoppingVolume<=10||stoppingVolume>=990)return 5;
    Advance(music,.6f);
    if(mciGetDeviceIDW(L"aquarium_bgm")!=0)return 6;

    std::cout<<"PASS: BGM fade-in, track change, and fade-out\n";
    return 0;
}
