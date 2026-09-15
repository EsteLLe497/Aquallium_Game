// =========================================================
// ファイルの情報[EndingSequence.cpp]
//
// 制作者:Masatora Tanaka        日付：2026/09/13
// =========================================================
#include "EndingSequence.h"
#include "../../third_party/imgui/imgui.h"
#include <algorithm>
#include <cfloat>

namespace story {
namespace {
constexpr float cardInputDelay=1.15f;
constexpr float normalFadeSeconds=2.20f;
constexpr float trueInterludeSeconds=6.8f;

// 短い余韻テキストを柔らかくフェードさせる。
float textAlpha(float localTime,float duration){
    constexpr float fadeSeconds=.75f;
    const float fadeIn=std::clamp(localTime/fadeSeconds,0.f,1.f);
    const float fadeOut=std::clamp((duration-localTime)/fadeSeconds,0.f,1.f);
    return std::min(fadeIn,fadeOut);
}
}

// =========================================================
// 状態初期化
// =========================================================
void EndingSequence::reset(){
    phase_=Phase::Inactive;clock_=0.f;returnTitle_=false;
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
    reset();phase_=Phase::TrueInterlude;
}

// =========================================================
// 更新
// =========================================================
void EndingSequence::update(float deltaTime,bool advance){
    if(phase_==Phase::Inactive)return;
    clock_+=std::clamp(deltaTime,0.f,.1f);
    if(phase_==Phase::TrueInterlude){
        if(clock_>=trueInterludeSeconds||(advance&&clock_>=cardInputDelay)){
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
void EndingSequence::showCard(const char* category,const char* endingName) const{
    const ImVec2 screen=ImGui::GetIO().DisplaySize;
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilledMultiColor({0,0},screen,IM_COL32(0,2,7,255),IM_COL32(1,10,18,255),
        IM_COL32(0,3,8,255),IM_COL32(0,2,6,255));
    ImFont* font=ImGui::GetFont();
    const float categorySize=16.f,titleFontSize=28.f;
    const ImVec2 categoryMeasure=font->CalcTextSizeA(categorySize,FLT_MAX,0,category);
    draw->AddText(font,categorySize,{(screen.x-categoryMeasure.x)*.5f,screen.y*.40f},
        IM_COL32(105,204,236,220),category);
    draw->AddLine({screen.x*.39f,screen.y*.46f},{screen.x*.61f,screen.y*.46f},
        IM_COL32(89,202,237,125),1.f);
    const ImVec2 titleMeasure=font->CalcTextSizeA(titleFontSize,FLT_MAX,0,endingName);
    draw->AddText(font,titleFontSize,{(screen.x-titleMeasure.x)*.5f,screen.y*.50f},
        IM_COL32(241,248,252,255),endingName);
    if(clock_>=cardInputDelay){
        constexpr const char* guide="クリック / F でタイトルへ";
        const ImVec2 guideSize=ImGui::CalcTextSize(guide);
        draw->AddText({(screen.x-guideSize.x)*.5f,screen.y*.78f},IM_COL32(145,160,175,220),guide);
    }
}

// =========================================================
// スタッフロールに代わる、砂へ残した言葉の余韻
// =========================================================
void EndingSequence::showTrueInterlude() const{
    const ImVec2 screen=ImGui::GetIO().DisplaySize;
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilledMultiColor({0,0},screen,IM_COL32(0,2,7,255),IM_COL32(1,10,18,255),
        IM_COL32(0,3,8,255),IM_COL32(0,2,6,255));
    constexpr float firstDuration=3.1f;
    const bool firstLine=clock_<firstDuration;
    const float localTime=firstLine?clock_:clock_-firstDuration;
    const float duration=firstLine?firstDuration:trueInterludeSeconds-firstDuration;
    const char* text=firstLine?"波が文字をさらっても、":"約束は、もう消えない。";
    const int alpha=static_cast<int>(255.f*textAlpha(localTime,duration));
    const ImVec2 textSize=ImGui::CalcTextSize(text);
    draw->AddText({(screen.x-textSize.x)*.5f,screen.y*.48f},IM_COL32(225,238,248,alpha),text);
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
    if(phase_==Phase::NormalCard){showCard("Normal End","「約束は朧げに」");return;}
    if(phase_==Phase::TrueCard){showCard("True End","「約束は暁の海で」");return;}
    showTrueInterlude();
}

bool EndingSequence::active() const noexcept{return phase_!=Phase::Inactive;}

bool EndingSequence::consumeReturnTitle(){
    const bool requested=returnTitle_;returnTitle_=false;return requested;
}

}
