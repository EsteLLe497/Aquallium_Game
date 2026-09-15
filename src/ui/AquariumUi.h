// =========================================================
// ファイルの情報[AquariumUi.h]
//
// 制作者:Masatora Tanaka        日付：2026/09/15
// =========================================================
#pragma once

#include "../../third_party/imgui/imgui.h"

namespace aquariumUi {

constexpr ImVec4 panelColor{.012f,.030f,.055f,.96f};
constexpr ImVec4 cardColor{.018f,.055f,.082f,.92f};
constexpr ImVec4 accentColor{.26f,.78f,.96f,1.f};
constexpr ImVec4 textColor{.88f,.94f,.98f,1.f};
constexpr ImVec4 mutedColor{.44f,.58f,.66f,1.f};

// 水族館UI共通のパネルスタイル。
class PanelStyle {
public:
    PanelStyle(){
        ImGui::PushStyleColor(ImGuiCol_WindowBg,panelColor);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,cardColor);
        ImGui::PushStyleColor(ImGuiCol_Border,{.19f,.53f,.66f,.34f});
        ImGui::PushStyleColor(ImGuiCol_Separator,{.16f,.52f,.68f,.36f});
        ImGui::PushStyleColor(ImGuiCol_Text,textColor);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled,mutedColor);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab,accentColor);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,{.62f,.92f,1.f,1.f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,5.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,4.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,3.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,1.f);
    }
    ~PanelStyle(){ImGui::PopStyleVar(5);ImGui::PopStyleColor(8);}
    PanelStyle(const PanelStyle&)=delete;
    PanelStyle& operator=(const PanelStyle&)=delete;
};

// 背景を沈め、操作対象だけを浮かせる。
inline void drawBackdrop(const ImVec2& screen,int alpha=205){
    auto* draw=ImGui::GetBackgroundDrawList();
    draw->AddRectFilledMultiColor({0,0},screen,IM_COL32(0,4,11,alpha-28),
        IM_COL32(1,12,22,alpha-18),IM_COL32(0,3,9,alpha),IM_COL32(0,3,9,alpha));
}

// パネル上端へ水面を思わせる細い発光線を描く。
inline void drawPanelAccent(){
    const ImVec2 minimum=ImGui::GetWindowPos();
    const ImVec2 maximum{minimum.x+ImGui::GetWindowWidth(),minimum.y+2.f};
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(minimum,maximum,
        IM_COL32(60,190,235,35),IM_COL32(100,225,255,210),
        IM_COL32(100,225,255,210),IM_COL32(60,190,235,35));
}

// 画面ごとの見出しを同じ文字階層で表示する。
inline void heading(const char* category,const char* title,const char* description=nullptr){
    if(category&&category[0])ImGui::TextColored(accentColor,"%s",category);
    if(title&&title[0]){
        ImGui::SetWindowFontScale(1.32f);ImGui::TextUnformatted(title);
        ImGui::SetWindowFontScale(1.f);
    }
    if(description&&description[0])ImGui::TextDisabled("%s",description);
    ImGui::Dummy({0,8});ImGui::Separator();ImGui::Dummy({0,10});
}

// 選択状態を発光枠と左端のラインで示す共通ボタン。
inline bool button(const char* label,const ImVec2& size,bool selected=false,
                   ImVec2 textAlign={.5f,.5f}){
    ImGui::PushStyleColor(ImGuiCol_Button,selected?ImVec4(.055f,.20f,.27f,.96f):cardColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.07f,.25f,.33f,.98f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,{.10f,.34f,.43f,1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign,textAlign);
    const bool pressed=ImGui::Button(label,size);
    ImGui::PopStyleVar();ImGui::PopStyleColor(3);
    if(selected||ImGui::IsItemHovered()||ImGui::IsItemFocused()){
        auto* draw=ImGui::GetWindowDrawList();
        const ImVec2 minimum=ImGui::GetItemRectMin(),maximum=ImGui::GetItemRectMax();
        draw->AddRect(minimum,maximum,IM_COL32(120,225,255,selected?225:175),3.f,0,1.2f);
        draw->AddRectFilled({minimum.x,minimum.y+5},{minimum.x+3,maximum.y-5},
            IM_COL32(105,220,255,235),2.f);
    }
    return pressed;
}

}
