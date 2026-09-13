#include "PasswordLock.h"
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
    constexpr float width=460.f,height=460.f;
    ImGui::SetNextWindowPos({screen.x*.5f-width*.5f,screen.y*.5f-height*.5f});
    ImGui::SetNextWindowSize({width,height});ImGui::SetNextWindowBgAlpha(.97f);
    ImGui::Begin(title_.c_str(),nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse);
    ImGui::TextWrapped("%s",prompt_.c_str());ImGui::Spacing();
    std::string display=digits_;while(display.size()<expected_.size())display+="＿";
    ImGui::SetWindowFontScale(1.65f);ImGui::Text("   %s",display.c_str());ImGui::SetWindowFontScale(1.f);
    ImGui::Spacing();
    const char* keys[] = {"1","2","3","4","5","6","7","8","9","消去","0","決定"};
    for(int i=0;i<12;++i){
        if(i%3)ImGui::SameLine();
        if(ImGui::Button(keys[i],{126,55})){
            if(i<9)Append(char('1'+i));else if(i==9){digits_.clear();wrong_=false;}else if(i==10)Append('0');else Submit();
        }
    }
    ImGui::TextDisabled("数字キー / Enterでも操作できます");
    ImGui::End();
}
}
