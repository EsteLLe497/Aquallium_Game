// =========================================================
// ファイルの情報[EndingSequence.cpp]
//
// 制作者:Masatora Tanaka        日付：2026/09/13
// =========================================================
#include "EndingSequence.h"
#include "../../third_party/imgui/imgui.h"
#include <algorithm>
#include <cwchar>
#include <windows.h>
#include <mmsystem.h>

namespace story {
namespace {
constexpr wchar_t videoAlias[]=L"aquarium_true_end_video";
constexpr float cardInputDelay=1.15f;
constexpr float normalFadeSeconds=2.20f;
constexpr float fallbackStaffRollSeconds=8.f;
}

EndingSequence::~EndingSequence(){stopVideo();}

// =========================================================
// 初期化
// =========================================================
void EndingSequence::initialize(const std::filesystem::path& videoPath){
    videoPath_=videoPath;
}

// =========================================================
// 状態初期化
// =========================================================
void EndingSequence::reset(){
    stopVideo();phase_=Phase::Inactive;clock_=videoPollClock_=0.f;returnTitle_=false;
}

// =========================================================
// ノーマルエンド開始
// =========================================================
void EndingSequence::startNormalEnd(){
    reset();phase_=Phase::NormalFade;
}

// =========================================================
// トゥルーエンド開始
// =========================================================
void EndingSequence::startTrueEnd(){
    reset();phase_=Phase::StaffRoll;videoPlaying_=startVideo();
}

// =========================================================
// スタッフロール動画開始
// =========================================================
bool EndingSequence::startVideo(){
    if(videoPath_.empty()||!std::filesystem::exists(videoPath_))return false;
    stopVideo();
    const std::wstring alias=videoAlias;
    const std::wstring open=L"open \""+videoPath_.wstring()+L"\" type mpegvideo alias "+alias;
    if(mciSendStringW(open.c_str(),nullptr,0,nullptr)!=0)return false;
    mciSendStringW((L"set "+alias+L" time format milliseconds").c_str(),nullptr,0,nullptr);
    if(mciSendStringW((L"play "+alias+L" fullscreen").c_str(),nullptr,0,nullptr)!=0){
        stopVideo();return false;
    }
    return true;
}

// =========================================================
// 動画終了判定
// =========================================================
bool EndingSequence::videoFinished(){
    wchar_t mode[32]{};
    const std::wstring command=L"status "+std::wstring(videoAlias)+L" mode";
    if(mciSendStringW(command.c_str(),mode,32,nullptr)!=0)return true;
    return std::wcscmp(mode,L"stopped")==0;
}

// =========================================================
// 動画停止
// =========================================================
void EndingSequence::stopVideo(){
    mciSendStringW((L"close "+std::wstring(videoAlias)).c_str(),nullptr,0,nullptr);
    videoPlaying_=false;
}

// =========================================================
// 更新
// =========================================================
void EndingSequence::update(float deltaTime,bool advance){
    if(phase_==Phase::Inactive)return;
    clock_+=std::clamp(deltaTime,0.f,.1f);
    if(phase_==Phase::StaffRoll){
        if(videoPlaying_){
            videoPollClock_+=deltaTime;
            if(videoPollClock_>=.25f){
                videoPollClock_=0.f;
                if(videoFinished()){stopVideo();phase_=Phase::TrueCard;clock_=0.f;}
            }
        }else if(clock_>=fallbackStaffRollSeconds||(advance&&clock_>=cardInputDelay)){
            phase_=Phase::TrueCard;clock_=0.f;
        }
        return;
    }
    if(phase_==Phase::NormalFade){
        if(clock_>=normalFadeSeconds){phase_=Phase::NormalCard;clock_=0.f;}
        return;
    }
    if(advance&&clock_>=cardInputDelay)returnTitle_=true;
}

// =========================================================
// エンドカード描画
// =========================================================
void EndingSequence::showCard(const char* category,const char* pendingName) const{
    const ImVec2 screen=ImGui::GetIO().DisplaySize;
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled({0,0},screen,IM_COL32_BLACK);
    const ImVec2 textSize=ImGui::CalcTextSize(category);
    draw->AddText({(screen.x-textSize.x)*.5f,screen.y*.43f},IM_COL32(220,235,245,255),category);
    const ImVec2 titleSize=ImGui::CalcTextSize(pendingName);
    draw->AddText({(screen.x-titleSize.x)*.5f,screen.y*.50f},IM_COL32(255,255,255,255),pendingName);
    if(clock_>=cardInputDelay){
        constexpr const char* guide="クリック / F でタイトルへ";
        const ImVec2 guideSize=ImGui::CalcTextSize(guide);
        draw->AddText({(screen.x-guideSize.x)*.5f,screen.y*.78f},IM_COL32(145,160,175,220),guide);
    }
}

// =========================================================
// 描画
// =========================================================
void EndingSequence::draw() const{
    if(phase_==Phase::Inactive)return;
    if(phase_==Phase::NormalFade){
        const ImVec2 screen=ImGui::GetIO().DisplaySize;
        const float t=std::clamp(clock_/normalFadeSeconds,0.f,1.f);
        const float eased=t*t*(3.f-2.f*t);
        ImGui::GetForegroundDrawList()->AddRectFilled(
            {0,0},screen,IM_COL32(0,0,0,static_cast<int>(255.f*eased)));
        return;
    }
    if(phase_==Phase::NormalCard){showCard("Normal End","「エンド名未定」");return;}
    if(phase_==Phase::TrueCard){showCard("True End","「タイトル名未定」");return;}
    const ImVec2 screen=ImGui::GetIO().DisplaySize;
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled({0,0},screen,IM_COL32_BLACK);
    if(videoPlaying_)return;
    constexpr const char* heading="STAFF ROLL（仮）";
    constexpr const char* notice="asset/video/true_end.mp4 を配置すると動画を再生します";
    const ImVec2 headingSize=ImGui::CalcTextSize(heading);
    const ImVec2 noticeSize=ImGui::CalcTextSize(notice);
    draw->AddText({(screen.x-headingSize.x)*.5f,screen.y*.42f},IM_COL32(230,240,250,255),heading);
    draw->AddText({(screen.x-noticeSize.x)*.5f,screen.y*.52f},IM_COL32(145,160,175,230),notice);
}

bool EndingSequence::active() const noexcept{return phase_!=Phase::Inactive;}

bool EndingSequence::consumeReturnTitle(){
    const bool requested=returnTitle_;returnTitle_=false;return requested;
}

}
