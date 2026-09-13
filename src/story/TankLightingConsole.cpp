#include "TankLightingConsole.h"
#include "../../third_party/imgui/imgui.h"
#include <algorithm>
#include <cmath>

namespace story {
namespace {
constexpr DirectX::XMFLOAT3 kPreviewEye{0.f,.15f,2.35f};
constexpr float kPreviewYaw=0.f;
constexpr float kPreviewPitch=-.035f;
constexpr float kHoldDuration=2.6f;
constexpr float kFadeOutDuration=.34f;
constexpr float kFadeInDuration=.46f;
}

void TankLightingConsole::Reset(Color color){
    if(color!=Color::White)color=Color::Blue; // Retired red saves fall back to blue.
    phase_=Phase::Closed;selected_=applied_=color;feedback_=Feedback::None;clock_=fadeAlpha_=0;
}

void TankLightingConsole::Open(const AquariumSettings& settings){
    if(phase_!=Phase::Closed)return;
    savedEye_={settings.cameraPositionX,settings.cameraPositionY,settings.cameraPositionZ};
    savedYaw_=settings.cameraYaw;savedPitch_=settings.cameraPitch;
    selected_=applied_;phase_=Phase::Menu;clock_=0;
}

float TankLightingConsole::Smooth(float value){
    const float t=std::clamp(value,0.f,1.f);return t*t*t*(t*(t*6.f-15.f)+10.f);
}

void TankLightingConsole::SetPreviewCamera(AquariumSettings& settings,float dolly) const{
    settings.cameraPositionX=kPreviewEye.x;settings.cameraPositionY=kPreviewEye.y;
    settings.cameraPositionZ=kPreviewEye.z+dolly;settings.cameraYaw=kPreviewYaw;
    settings.cameraPitch=kPreviewPitch;
}

void TankLightingConsole::Update(float dt,AquariumSettings& settings){
    if(phase_==Phase::FadeToPreview){
        clock_+=dt;fadeAlpha_=std::clamp(clock_/kFadeOutDuration,0.f,1.f);
        if(clock_>=kFadeOutDuration){SetPreviewCamera(settings);phase_=Phase::PreviewIn;clock_=0;}
    } else if(phase_==Phase::PreviewIn){
        SetPreviewCamera(settings);clock_+=dt;
        fadeAlpha_=1.f-std::clamp(clock_/kFadeInDuration,0.f,1.f);
        if(clock_>=kFadeInDuration){phase_=Phase::Hold;clock_=fadeAlpha_=0;}
    } else if(phase_==Phase::Hold){
        clock_+=dt;SetPreviewCamera(settings,.42f*Smooth(clock_/kHoldDuration));
        if(clock_>=kHoldDuration){phase_=Phase::FadeToReturn;clock_=0;}
    } else if(phase_==Phase::FadeToReturn){
        SetPreviewCamera(settings,.42f);clock_+=dt;
        fadeAlpha_=std::clamp(clock_/kFadeOutDuration,0.f,1.f);
        if(clock_>=kFadeOutDuration){
            settings.cameraPositionX=savedEye_.x;settings.cameraPositionY=savedEye_.y;
            settings.cameraPositionZ=savedEye_.z;settings.cameraYaw=savedYaw_;settings.cameraPitch=savedPitch_;
            phase_=Phase::ReturnIn;clock_=0;
        }
    } else if(phase_==Phase::ReturnIn){
        settings.cameraPositionX=savedEye_.x;settings.cameraPositionY=savedEye_.y;
        settings.cameraPositionZ=savedEye_.z;settings.cameraYaw=savedYaw_;settings.cameraPitch=savedPitch_;
        clock_+=dt;fadeAlpha_=1.f-std::clamp(clock_/kFadeInDuration,0.f,1.f);
        if(clock_>=kFadeInDuration){phase_=Phase::Closed;clock_=fadeAlpha_=0;}
    }
}

void TankLightingConsole::Draw(){
    const ImVec2 screen=ImGui::GetIO().DisplaySize;
    if(phase_!=Phase::Menu){
        if(fadeAlpha_>.001f)ImGui::GetForegroundDrawList()->AddRectFilled(
            {0,0},screen,IM_COL32(0,0,0,int(std::clamp(fadeAlpha_,0.f,1.f)*255.f)));
        return;
    }
    ImGui::SetNextWindowPos({screen.x*.5f,screen.y*.5f},ImGuiCond_Always,{.5f,.5f});
    ImGui::SetNextWindowSize({470,0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,{.008f,.020f,.040f,.97f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{26,22});
    ImGui::Begin("##tank_lighting_console",nullptr,ImGuiWindowFlags_NoDecoration|
        ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored({.52f,.84f,1.f,1.f},"大水槽ライティング制御");
    ImGui::TextDisabled("照明色を選択して保存してください");
    ImGui::Separator();ImGui::Spacing();
    auto choice=[&](const char* label,Color color,const ImVec4& tint){
        const bool active=selected_==color;
        ImGui::PushStyleColor(ImGuiCol_Button,active?tint:ImVec4(tint.x*.35f,tint.y*.35f,tint.z*.35f,.85f));
        if(ImGui::Button(label,{128,44})&&selected_!=color){selected_=color;feedback_=Feedback::Select;}
        ImGui::PopStyleColor();
    };
    choice("青",Color::Blue,{.04f,.28f,.75f,1});ImGui::SameLine();
    choice("白",Color::White,{.72f,.72f,.68f,1});
    ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
    if(ImGui::Button("キャンセル",{198,38}))phase_=Phase::Closed;
    ImGui::SameLine();
    if(ImGui::Button("保存",{198,38})){
        applied_=selected_;feedback_=Feedback::Save;phase_=Phase::FadeToPreview;clock_=fadeAlpha_=0;
    }
    ImGui::End();ImGui::PopStyleVar();ImGui::PopStyleColor();
}

TankLightingConsole::Feedback TankLightingConsole::ConsumeFeedback(){
    const auto value=feedback_;feedback_=Feedback::None;return value;
}
}
