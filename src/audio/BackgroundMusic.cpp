// =========================================================
// ファイルの情報[BackgroundMusic.cpp]
//
// 制作者:Masatora Tanaka        日付：2026/09/14
// =========================================================
#include "BackgroundMusic.h"
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>

namespace audio {
namespace {
constexpr wchar_t bgmAlias[]=L"aquarium_bgm";
constexpr float fadeOutSeconds=.65f;
constexpr float fadeInSeconds=.85f;
}

BackgroundMusic::~BackgroundMusic(){closeCurrent();}

// =========================================================
// BGMフォルダ初期化
// =========================================================
void BackgroundMusic::initialize(const std::filesystem::path& folder){folder_=folder;}

// =========================================================
// BGM種別に対応するファイル取得
// =========================================================
std::filesystem::path BackgroundMusic::fileFor(Track track,const std::filesystem::path& folder){
    switch(track){
    case Track::Title:return folder/"title.mp3";
    case Track::Aquarium:return folder/"stage.wav";
    case Track::Chase:{
        // 旧指定名も許容しつつ、現在配置済みの chase.mp3 を標準にする。
        const auto requested=folder/"chasee.mp3";
        return std::filesystem::exists(requested)?requested:folder/"chase.mp3";
    }
    case Track::Actually:return folder/"actually.mp3";
    default:return {};
    }
}

// =========================================================
// シーンBGM切替
// =========================================================
void BackgroundMusic::update(Track track){
    target_=track;
}

// =========================================================
// BGMフェード更新
// =========================================================
void BackgroundMusic::advance(float deltaTime){
    const float step=std::clamp(deltaTime,0.f,.1f);
    if(target_!=current_){
        if(current_!=Track::Silent&&deviceOpen_){
            fadeGain_=std::max(0.f,fadeGain_-step/fadeOutSeconds);
            applyVolume();
            if(fadeGain_>0.f)return;
        }
        closeCurrent();
        if(target_!=Track::Silent)openTrack(target_);
        return;
    }
    if(current_==Track::Silent||!deviceOpen_||fadeGain_>=1.f)return;
    fadeGain_=std::min(1.f,fadeGain_+step/fadeInSeconds);
    applyVolume();
}

// =========================================================
// 新しいBGMを無音から再生開始
// =========================================================
bool BackgroundMusic::openTrack(Track track){
    current_=track;
    const auto path=fileFor(track,folder_);
    fadeGain_=0.f;deviceOpen_=false;
    if(path.empty()||!std::filesystem::exists(path)){
        fadeGain_=1.f;
        return false;
    }
    const std::wstring command=L"open \""+path.wstring()+L"\" type mpegvideo alias "+
        std::wstring(bgmAlias);
    lastError_=mciSendStringW(command.c_str(),nullptr,0,nullptr);
    if(!lastError_){
        deviceOpen_=true;
        applyVolume();
        lastError_=mciSendStringW(L"play aquarium_bgm from 0 repeat",nullptr,0,nullptr);
    }
    if(lastError_)OutputDebugStringW((L"BGM could not play: "+path.wstring()+L"\n").c_str());
    if(lastError_){
        mciSendStringW(L"close aquarium_bgm",nullptr,0,nullptr);
        deviceOpen_=false;fadeGain_=1.f;
    }
    return lastError_==0;
}

// =========================================================
// 現在のMCIデバイスを閉じる
// =========================================================
void BackgroundMusic::closeCurrent(){
    if(deviceOpen_)mciSendStringW(L"close aquarium_bgm",nullptr,0,nullptr);
    deviceOpen_=false;current_=Track::Silent;fadeGain_=0.f;
}

// =========================================================
// BGM音量変更
// =========================================================
void BackgroundMusic::setVolume(float normalizedVolume){
    const float next=std::clamp(normalizedVolume,0.f,1.f);
    if(std::abs(next-volume_)<.001f)return;
    volume_=next;applyVolume();
}

// =========================================================
// 再生中BGMへ音量適用
// =========================================================
void BackgroundMusic::applyVolume(){
    if(!deviceOpen_)return;
    const int volume=static_cast<int>(std::lround(volume_*fadeGain_*1000.f));
    mciSendStringW((L"setaudio "+std::wstring(bgmAlias)+L" volume to "+
        std::to_wstring(volume)).c_str(),nullptr,0,nullptr);
}

}
