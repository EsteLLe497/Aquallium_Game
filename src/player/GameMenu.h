#pragma once
#include "../story/StoryTexture.h"
#include "SaveSystem.h"
#include <array>
#include <filesystem>
namespace player {
class GameMenu {
public:
  enum class RequestType { None, NewGame, Save, Load, ReturnTitle, Quit };
  struct Request {
    RequestType type = RequestType::None;
    int slot = -1;
  };
  void Initialize(ID3D11Device *device,
                  const std::filesystem::path &textureRoot);
  void Toggle() {
    if (!title_) {
      open_ = !open_;
      if (open_)
        page_ = Page::Items;
    }
  }
  void Close() { open_ = false; }
  void ShowTitle() {
    title_ = true;
    open_ = true;
    page_ = Page::Title;
  }
  void HideTitle() {
    title_ = false;
    open_ = false;
  }
  void OpenSavePage() { title_ = false; open_ = true; page_ = Page::Save; }
  void OpenLoadPage() { title_ = false; open_ = true; page_ = Page::Load; }
  bool IsOpen() const { return open_; }
  bool IsTitle() const { return title_; }
  void SetClueOwned(bool value) { clueOwned_ = value; }
  void SetSlots(const std::array<SaveSlotInfo, SaveSystem::kSlotCount> &value) {
    slots_ = value;
  }
  Request ConsumeRequest() {
    const auto value = request_;
    request_ = {};
    return value;
  }
  void Draw();

private:
  enum class Page { Title, Items, Save, Load };
  void DrawSlots(bool saving);
  story::StoryTexture clue_;
  std::array<SaveSlotInfo, SaveSystem::kSlotCount> slots_{};
  Request request_{};
  Page page_ = Page::Items;
  bool open_ = false, title_ = false, clueOwned_ = false, inspect_ = false,
       confirmTitle_ = false;
};
} // namespace player
