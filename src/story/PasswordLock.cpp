#include "PasswordLock.h"
#include "../ui/AquariumUi.h"
#include <windows.h>

namespace story {
void PasswordLock::Append(char digit){if(digits_.size()<expected_.size()){digits_+=digit;wrong_=false;feedback_=Feedback::Select;}}
void PasswordLock::Submit(){
    if(digits_==expected_){unlocked_=true;active_=false;feedback_=Feedback::Open;}
    else{wrong_=true;active_=false;digits_.clear();feedback_=Feedback::Miss;}
}
void PasswordLock::Update(const framework::InputSystem& input) {
    if(!active_)return;
    for(int i=0;i<=9;++i)if(input.WasPressed('0'+i)||input.WasPressed(VK_NUMPAD0+i))Append(char('0'+i));
    if(input.WasPressed(VK_BACK)&&!digits_.empty())digits_.pop_back();
    if(input.WasPressed(VK_RETURN))Submit();
    if(input.WasPressed(VK_ESCAPE))active_=false;
}
void PasswordLock::Draw() {
    if(!active_)return;
    const ImVec2 screen=ImGui::GetIO().DisplaySize;
    aquariumUi::drawBackdrop(screen,185);
    aquariumUi::PanelStyle style;
    constexpr float width=500.f,height=500.f;
    ImGui::SetNextWindowPos({screen.x*.5f-width*.5f,screen.y*.5f-height*.5f});
    ImGui::SetNextWindowSize({width,height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{34,28});
    ImGui::Begin("##password_lock",nullptr,ImGuiWindowFlags_NoDecoration|
        ImGuiWindowFlags_NoSavedSettings);
    aquariumUi::drawPanelAccent();
    aquariumUi::heading("",title_.c_str());
    ImGui::TextWrapped("%s",prompt_.c_str());ImGui::Spacing();
    std::string display=digits_;while(display.size()<expected_.size())display+="＿";
    const ImVec2 displaySize=ImGui::CalcTextSize(display.c_str());
    ImGui::SetCursorPosX((width-displaySize.x*1.65f)*.5f);
    ImGui::SetWindowFontScale(1.65f);ImGui::TextColored(aquariumUi::accentColor,"%s",display.c_str());
    ImGui::SetWindowFontScale(1.f);ImGui::Dummy({0,14});
    const char* keys[] = {"1","2","3","4","5","6","7","8","9","消去","0","決定"};
    for(int i=0;i<12;++i){
        if(i%3)ImGui::SameLine();
        if(aquariumUi::button(keys[i],{138,52},i==11)){
            if(i<9)Append(char('1'+i));else if(i==9){digits_.clear();wrong_=false;}else if(i==10)Append('0');else Submit();
        }
    }
    ImGui::Dummy({0,8});ImGui::TextDisabled("数字キー / Enterでも操作できます");
    ImGui::End();ImGui::PopStyleVar();
}
}
