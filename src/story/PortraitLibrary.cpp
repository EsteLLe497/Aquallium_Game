// =========================================================
// ファイルの情報[PortraitLibrary.cpp]
//
// 制作者:Masatora Tanaka        日付：2026/09/13
// =========================================================
#include "PortraitLibrary.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <unordered_map>

namespace story {
namespace {

std::string cacheKey(ID3D11Device* device,const std::filesystem::path& folder) {
    return folder.lexically_normal().string()+"#"+
        std::to_string(reinterpret_cast<std::uintptr_t>(device));
}

bool isPng(const std::filesystem::path& path) {
    std::string extension=path.extension().string();
    std::transform(extension.begin(),extension.end(),extension.begin(),
        [](unsigned char character){return char(std::tolower(character));});
    return extension==".png";
}

}

std::shared_ptr<PortraitLibrary> PortraitLibrary::load(
    ID3D11Device* device,const std::filesystem::path& folder) {
    static std::unordered_map<std::string,std::weak_ptr<PortraitLibrary>> cache;
    const std::string key=cacheKey(device,folder);
    if(const auto found=cache.find(key);found!=cache.end())
        if(auto library=found->second.lock())return library;
    auto library=std::shared_ptr<PortraitLibrary>(new PortraitLibrary(device,folder));
    cache[key]=library;
    return library;
}

PortraitLibrary::PortraitLibrary(ID3D11Device* device,const std::filesystem::path& folder)
    :device_(device) {
    std::error_code error;
    if(!std::filesystem::is_directory(folder,error))return;
    for(std::filesystem::directory_iterator file(folder,error),end;file!=end&&!error;file.increment(error)){
        if(!file->is_regular_file(error)||!isPng(file->path()))continue;
        entries_.push_back({file->path().stem().string(),file->path()});
    }
    std::sort(entries_.begin(),entries_.end(),[](const Entry& left,const Entry& right){
        const bool leftNormal=left.expression=="normal";
        const bool rightNormal=right.expression=="normal";
        if(leftNormal!=rightNormal)return leftNormal;
        return left.expression<right.expression;
    });
    expressions_.reserve(entries_.size());
    for(const auto& entry:entries_)expressions_.push_back(entry.expression);
}

const StoryTexture* PortraitLibrary::loadEntry(const std::string& expression) const {
    const auto found=std::find_if(entries_.begin(),entries_.end(),[&](const Entry& entry){
        return entry.expression==expression;
    });
    if(found==entries_.end())return nullptr;
    if(!found->loadAttempted){
        found->loadAttempted=true;
        found->texture.Load(device_.Get(),found->path);
    }
    return found->texture.view?&found->texture:nullptr;
}

const StoryTexture* PortraitLibrary::find(const std::string& expression) const {
    if(expression=="hide"||expression=="none")return nullptr;
    if(const StoryTexture* portrait=loadEntry(expression))return portrait;
    return expression=="normal"?nullptr:loadEntry("normal");
}

const StoryTexture* PortraitLibrary::underlay(const std::string& expression) const {
    const StoryTexture* portrait=loadEntry(expression);
    // 涙など顔だけの差分画像は、通常立ち絵へ重ねて全身を復元する。
    return portrait&&portrait->alphaCoverage<.08f?loadEntry("normal"):nullptr;
}

}
