// =========================================================
// ファイルの情報[PortraitLibrary.h]
//
// 制作者:Masatora Tanaka        日付：2026/09/13
// =========================================================
#pragma once

#include "StoryTexture.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace story {

// 立ち絵フォルダーを自動検出し、必要になった表情だけGPUへ読み込む。
class PortraitLibrary {
public:
    static std::shared_ptr<PortraitLibrary> load(
        ID3D11Device* device,const std::filesystem::path& folder);

    [[nodiscard]] const StoryTexture* find(const std::string& expression) const;
    [[nodiscard]] const StoryTexture* underlay(const std::string& expression) const;
    [[nodiscard]] const std::vector<std::string>& expressions() const noexcept {
        return expressions_;
    }

private:
    struct Entry {
        std::string expression;
        std::filesystem::path path;
        mutable StoryTexture texture;
        mutable bool loadAttempted=false;
    };

    PortraitLibrary(ID3D11Device* device,const std::filesystem::path& folder);
    [[nodiscard]] const StoryTexture* loadEntry(const std::string& expression) const;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    std::vector<std::string> expressions_;
    mutable std::vector<Entry> entries_;
};

}
