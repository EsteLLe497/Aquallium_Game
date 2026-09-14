#include "GameMenu.h"
#include "../../third_party/imgui/imgui.h"
#include <algorithm>
#include <cstdio>
namespace player {
namespace {
std::string Clock(float s) {
  int t = std::max(0, int(s));
  char out[32]{};
  std::snprintf(out, sizeof(out), "%02d:%02d:%02d", t / 3600, (t / 60) % 60,
                t % 60);
  return out;
}
} // namespace
void GameMenu::Initialize(ID3D11Device *device,
                          const std::filesystem::path &root) {
  clue_.Load(device, root / "other" / "nozo1.png");
}
void GameMenu::DrawSlots(bool saving) {
  ImGui::TextUnformatted(saving ? "セーブデータ" : "ロードデータ");
  ImGui::Separator();
  ImGui::Dummy({0, 8});
  for (int i = 0; i < SaveSystem::kSlotCount; ++i) {
    const auto &s = slots_[i];
    ImGui::PushID(i);
    ImGui::BeginChild("slot", {0, 76}, true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored({.50f, .82f, 1.f, 1.f}, "FILE %d", i + 1);
    ImGui::SameLine(112);
    if (s.occupied) {
      ImGui::TextUnformatted(s.location.c_str());
      ImGui::SameLine(650);
      ImGui::Text("PLAY %s", Clock(s.playSeconds).c_str());
      ImGui::TextDisabled("%s", s.timestamp.c_str());
    } else
      ImGui::TextDisabled("データがありません");
    ImGui::SetCursorPos({ImGui::GetWindowWidth() - 115, 22});
    ImGui::BeginDisabled(!saving && !s.occupied);
    if (ImGui::Button(saving ? "保存" : "ロード", {96, 34}))
      request_ = {saving ? RequestType::Save : RequestType::Load, i};
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::PopID();
    ImGui::Dummy({0, 5});
  }
}

// =========================================================
// BGM・SE音量設定
// =========================================================
void GameMenu::drawVolumeSettings() {
  ImGui::TextUnformatted("音量設定");
  ImGui::Separator();
  ImGui::Dummy({0, 18});
  ImGui::TextUnformatted("BGM音量");
  ImGui::SetNextItemWidth(-1);
  ImGui::SliderInt("##bgm_volume", &bgmVolumePercent_, 0, 100, "%d%%");
  ImGui::Dummy({0, 14});
  ImGui::TextUnformatted("SE音量");
  ImGui::SetNextItemWidth(-1);
  ImGui::SliderInt("##se_volume", &seVolumePercent_, 0, 100, "%d%%");
}

void GameMenu::Draw() {
  if (!open_)
    return;
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::GetBackgroundDrawList()->AddRectFilled(
      {0, 0}, screen, IM_COL32(1, 5, 12, title_ ? 235 : 205));
  if (title_ && page_ == Page::Title) {
    ImGui::SetNextWindowPos({screen.x * .5f, screen.y * .48f}, ImGuiCond_Always,
                            {.5f, .5f});
    ImGui::SetNextWindowSize({520, 500});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {.004f, .015f, .032f, .95f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {58, 48});
    ImGui::Begin("##title", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(1.75f);
    ImGui::TextColored({.58f, .86f, 1.f, 1.f}, "AQUALLIUM");
    ImGui::SetWindowFontScale(1.f);
    ImGui::TextDisabled("閉館後の水族館");
    ImGui::Dummy({0, 62});
    const ImVec2 b{ImGui::GetContentRegionAvail().x, 46};
    if (ImGui::Button("スタート", b))
      request_ = {RequestType::NewGame, -1};
    ImGui::Dummy({0, 8});
    if (ImGui::Button("ロード", b))
      page_ = Page::Load;
    ImGui::Dummy({0, 8});
    if (ImGui::Button("設定", b))
      page_ = Page::Settings;
    ImGui::Dummy({0, 8});
    if (ImGui::Button("終了", b))
      request_ = {RequestType::Quit, -1};
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    return;
  }
  if (title_ && page_ == Page::Settings) {
    ImGui::SetNextWindowPos({screen.x * .5f, screen.y * .48f}, ImGuiCond_Always,
                            {.5f, .5f});
    ImGui::SetNextWindowSize({520, 330});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {48, 38});
    ImGui::Begin("##title_settings", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoSavedSettings);
    if (ImGui::Button("← タイトル", {160, 36}))
      page_ = Page::Title;
    ImGui::Dummy({0, 18});
    drawVolumeSettings();
    ImGui::End();
    ImGui::PopStyleVar();
    return;
  }
  ImGui::SetNextWindowPos({screen.x * .10f, screen.y * .07f});
  ImGui::SetNextWindowSize({screen.x * .80f, screen.y * .86f});
  ImGui::SetNextWindowBgAlpha(.96f);
  ImGui::Begin(title_ ? "ロード" : "メニュー", nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove);
  if (title_) {
    if (ImGui::Button("← タイトル"))
      page_ = Page::Title;
    ImGui::SameLine();
    ImGui::TextDisabled("続きから始める");
    ImGui::Separator();
    DrawSlots(false);
    ImGui::End();
    return;
  }
  ImGui::BeginChild("tabs", {190, 0}, true);
  if (ImGui::Selectable("アイテム", page_ == Page::Items))
    page_ = Page::Items;
  if (ImGui::Selectable("セーブ", page_ == Page::Save))
    page_ = Page::Save;
  if (ImGui::Selectable("ロード", page_ == Page::Load))
    page_ = Page::Load;
  if (ImGui::Selectable("設定", page_ == Page::Settings))
    page_ = Page::Settings;
  if (ImGui::Selectable("タイトルへ", false))
    confirmTitle_ = true;
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("content", {0, 0}, true);
  if (page_ == Page::Save)
    DrawSlots(true);
  else if (page_ == Page::Load)
    DrawSlots(false);
  else if (page_ == Page::Settings)
    drawVolumeSettings();
  else {
    ImGui::TextUnformatted("アイテム");
    ImGui::Separator();
    if (!clueOwned_)
      ImGui::TextDisabled("所持しているアイテムはありません");
    else {
      ImGui::BeginChild("item_list", {245, 0}, true);
      if (ImGui::Selectable("とある暗号", inspect_))
        inspect_ = true;
      ImGui::EndChild();
      ImGui::SameLine();
      ImGui::BeginChild("preview", {0, 0}, true);
      if (!inspect_)
        ImGui::TextDisabled("アイテムを選択するとプレビューを表示します");
      else {
        ImGui::TextUnformatted("クラゲとエイが描かれた紙");
        ImGui::Separator();
        ImGui::TextWrapped("円柱水槽のクラゲ×4、エイ×2　と描かれている。");
        if (clue_.view) {
          const auto a = ImGui::GetContentRegionAvail();
          const float sc = std::max(
              .01f, std::min(a.x / clue_.width, (a.y - 8) / clue_.height));
          ImGui::Image(ImTextureRef(clue_.view.Get()),
                       {clue_.width * sc, clue_.height * sc});
        }
      }
      ImGui::EndChild();
    }
  }
  ImGui::EndChild();
  if (confirmTitle_) {
    ImGui::OpenPopup("タイトルに戻りますか？");
    confirmTitle_ = false;
  }
  ImGui::SetNextWindowPos({screen.x * .5f, screen.y * .5f}, ImGuiCond_Appearing,
                          {.5f, .5f});
  if (ImGui::BeginPopupModal("タイトルに戻りますか？", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("セーブしていない進行は失われます。");
    if (ImGui::Button("はい", {120, 36})) {
      request_ = {RequestType::ReturnTitle, -1};
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("いいえ", {120, 36}))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
  ImGui::End();
}
} // namespace player
