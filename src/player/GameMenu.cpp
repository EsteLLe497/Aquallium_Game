// =========================================================
// ファイルの情報[GameMenu.cpp]
//
// 制作者:Masatora Tanaka        日付：2026/09/15
// =========================================================
#include "GameMenu.h"
#include "../ui/AquariumUi.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace player {
namespace {

std::string clockText(float seconds){
    const int total=std::max(0,static_cast<int>(seconds));
    char output[32]{};
    std::snprintf(output,sizeof(output),"%02d:%02d:%02d",total/3600,(total/60)%60,total%60);
    return output;
}

}

void GameMenu::Initialize(ID3D11Device* device,const std::filesystem::path& root){
    clue_.Load(device,root/"other"/"nozo1.png");
    titleLogo_.Load(device,root/"other"/"title.png");
}

// =========================================================
// セーブ・ロード共通のデータカード
// =========================================================
void GameMenu::DrawSlots(bool saving){
    aquariumUi::heading("",saving?"セーブデータ":"ロードデータ");
    for(int i=0;i<SaveSystem::kSlotCount;++i){
        const auto& slot=slots_[i];
        ImGui::PushID(i);
        ImGui::BeginChild("slot",{0,92},true,ImGuiWindowFlags_NoScrollbar);
        auto* draw=ImGui::GetWindowDrawList();
        const ImVec2 origin=ImGui::GetWindowPos();
        draw->AddRectFilled({origin.x,origin.y+10},{origin.x+3,origin.y+82},
            slot.occupied?IM_COL32(86,208,245,220):IM_COL32(74,94,105,130),2.f);

        ImGui::SetCursorPos({18,14});
        ImGui::TextColored(aquariumUi::accentColor,"FILE %d",i+1);
        ImGui::SameLine(110);
        if(slot.occupied){
            ImGui::TextUnformatted(slot.location.c_str());
            ImGui::SetCursorPos({110,47});
            ImGui::TextDisabled("%s   /   PLAY %s",slot.timestamp.c_str(),
                clockText(slot.playSeconds).c_str());
        }else{
            ImGui::TextDisabled("データがありません");
        }
        ImGui::SetCursorPos({ImGui::GetWindowWidth()-126,27});
        ImGui::BeginDisabled(!saving&&!slot.occupied);
        if(aquariumUi::button(saving?"保存":"ロード",{106,38}))
            request_={saving?RequestType::Save:RequestType::Load,i};
        ImGui::EndDisabled();
        ImGui::EndChild();ImGui::PopID();ImGui::Dummy({0,7});
    }
}

// =========================================================
// BGM・SE音量設定
// =========================================================
void GameMenu::drawVolumeSettings(){
    aquariumUi::heading("","音量設定");
    ImGui::TextUnformatted("BGM音量");
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderInt("##bgm_volume",&bgmVolumePercent_,0,100,"%d%%");
    ImGui::Dummy({0,20});
    ImGui::TextUnformatted("SE音量");
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderInt("##se_volume",&seVolumePercent_,0,100,"%d%%");
}

// =========================================================
// タイトルロゴとメインメニュー
// =========================================================
void GameMenu::drawTitlePage(float screenWidth,float screenHeight){
    const ImVec2 screen{screenWidth,screenHeight};
    auto* background=ImGui::GetBackgroundDrawList();
    background->AddRectFilledMultiColor({0,0},screen,IM_COL32(0,4,13,58),
        IM_COL32(0,9,20,48),IM_COL32(0,3,10,230),IM_COL32(0,3,10,230));
    if(titleLogo_.view&&titleLogo_.width&&titleLogo_.height){
        const float logoWidth=std::min(screen.x*.68f,900.f);
        const float scale=logoWidth/static_cast<float>(titleLogo_.width);
        const ImVec2 logoSize{logoWidth,titleLogo_.height*scale};
        const ImVec2 position{std::max(24.f,screen.x*.035f),std::max(24.f,screen.y*.075f)};
        background->AddImage(ImTextureRef(titleLogo_.view.Get()),position,
            {position.x+logoSize.x,position.y+logoSize.y});
    }

    const float width=std::clamp(screen.x*.27f,320.f,390.f);
    ImGui::SetNextWindowPos({screen.x-width-std::max(42.f,screen.x*.07f),screen.y*.53f});
    ImGui::SetNextWindowSize({width,282});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("##title",nullptr,ImGuiWindowFlags_NoDecoration|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
    ImGui::TextDisabled("閉館後の水族館");ImGui::Dummy({0,5});
    const ImVec2 buttonSize{ImGui::GetContentRegionAvail().x,44};
    if(aquariumUi::button("スタート",buttonSize,false,{.07f,.5f}))
        request_={RequestType::NewGame,-1};
    ImGui::Dummy({0,8});
    if(aquariumUi::button("ロード",buttonSize,false,{.07f,.5f}))page_=Page::Load;
    ImGui::Dummy({0,8});
    if(aquariumUi::button("設定",buttonSize,false,{.07f,.5f}))page_=Page::Settings;
    ImGui::Dummy({0,8});
    if(aquariumUi::button("終了",buttonSize,false,{.07f,.5f}))
        request_={RequestType::Quit,-1};
    ImGui::End();ImGui::PopStyleVar();
}

// =========================================================
// メニュー全体描画
// =========================================================
void GameMenu::Draw(){
    if(!open_)return;
    aquariumUi::PanelStyle style;
    const ImVec2 screen=ImGui::GetIO().DisplaySize;
    if(title_&&page_==Page::Title){drawTitlePage(screen.x,screen.y);return;}
    aquariumUi::drawBackdrop(screen,title_?178:218);

    if(title_&&page_==Page::Settings){
        ImGui::SetNextWindowPos({screen.x*.5f,screen.y*.48f},ImGuiCond_Always,{.5f,.5f});
        ImGui::SetNextWindowSize({560,360});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{46,34});
        ImGui::Begin("##title_settings",nullptr,ImGuiWindowFlags_NoDecoration|
            ImGuiWindowFlags_NoSavedSettings);
        aquariumUi::drawPanelAccent();
        if(aquariumUi::button("← タイトル",{170,36},false,{.08f,.5f}))page_=Page::Title;
        ImGui::Dummy({0,18});drawVolumeSettings();
        ImGui::End();ImGui::PopStyleVar();return;
    }

    ImGui::SetNextWindowPos({screen.x*.08f,screen.y*.065f});
    ImGui::SetNextWindowSize({screen.x*.84f,screen.y*.87f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{28,26});
    ImGui::Begin("##game_menu",nullptr,ImGuiWindowFlags_NoDecoration|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
    aquariumUi::drawPanelAccent();
    if(title_){
        if(aquariumUi::button("← タイトル",{170,36},false,{.08f,.5f}))page_=Page::Title;
        ImGui::SameLine();ImGui::TextDisabled("続きから始める");
        ImGui::Dummy({0,16});DrawSlots(false);
        ImGui::End();ImGui::PopStyleVar();return;
    }

    ImGui::BeginChild("navigation",{206,0},false);
    ImGui::Dummy({0,4});
    const ImVec2 navigationSize{ImGui::GetContentRegionAvail().x,43};
    if(aquariumUi::button("アイテム",navigationSize,page_==Page::Items,{.10f,.5f}))page_=Page::Items;
    ImGui::Dummy({0,7});
    if(aquariumUi::button("セーブ",navigationSize,page_==Page::Save,{.10f,.5f}))page_=Page::Save;
    ImGui::Dummy({0,7});
    if(aquariumUi::button("ロード",navigationSize,page_==Page::Load,{.10f,.5f}))page_=Page::Load;
    ImGui::Dummy({0,7});
    if(aquariumUi::button("設定",navigationSize,page_==Page::Settings,{.10f,.5f}))page_=Page::Settings;
    ImGui::SetCursorPosY(ImGui::GetWindowHeight()-53);
    if(aquariumUi::button("タイトルへ",navigationSize,false,{.10f,.5f}))confirmTitle_=true;
    ImGui::EndChild();ImGui::SameLine(0,24);

    ImGui::BeginChild("content",{0,0},false);
    if(page_==Page::Save)DrawSlots(true);
    else if(page_==Page::Load)DrawSlots(false);
    else if(page_==Page::Settings)drawVolumeSettings();
    else{
        aquariumUi::heading("","アイテム");
        if(!clueOwned_)ImGui::TextDisabled("所持しているアイテムはありません");
        else{
            ImGui::BeginChild("item_list",{255,0},true);
            if(aquariumUi::button("とある暗号",{ImGui::GetContentRegionAvail().x,44},inspect_,{.08f,.5f}))inspect_=true;
            ImGui::EndChild();ImGui::SameLine(0,18);
            ImGui::BeginChild("preview",{0,0},true);
            if(!inspect_)ImGui::TextDisabled("アイテムを選択するとプレビューを表示します");
            else{
                ImGui::TextUnformatted("クラゲとエイが描かれた紙");ImGui::Separator();
                ImGui::TextWrapped("円柱水槽のクラゲ×4、エイ×2　と描かれている。");
                if(clue_.view){
                    const auto available=ImGui::GetContentRegionAvail();
                    const float scale=std::max(.01f,std::min(available.x/clue_.width,
                        (available.y-8)/clue_.height));
                    ImGui::Image(ImTextureRef(clue_.view.Get()),{clue_.width*scale,clue_.height*scale});
                }
            }
            ImGui::EndChild();
        }
    }
    ImGui::EndChild();

    if(confirmTitle_){ImGui::OpenPopup("##return_title");confirmTitle_=false;}
    ImGui::SetNextWindowPos({screen.x*.5f,screen.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
    if(ImGui::BeginPopupModal("##return_title",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        aquariumUi::heading("","タイトルに戻りますか？");
        ImGui::TextUnformatted("セーブしていない進行は失われます。");
        if(aquariumUi::button("はい",{130,38},true)){
            request_={RequestType::ReturnTitle,-1};ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(aquariumUi::button("いいえ",{130,38}))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();ImGui::PopStyleVar();
}

}
