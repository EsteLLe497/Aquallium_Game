#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <filesystem>
#include <string>

namespace audio {
// A dedicated MCI device keeps music independent from one-shot/generated SE.
class BackgroundMusic {
public:
    ~BackgroundMusic() { mciSendStringW(L"close aquarium_bgm",nullptr,0,nullptr); }
    void Initialize(const std::filesystem::path& folder) {folder_=folder;}
    void Update(bool title,bool silent) {
        // Asset assignments are intentionally crossed: stage.wav is the
        // title theme, while title.wav plays during exploration.
        const std::wstring desired=silent?L"":title?L"stage":L"title";
        if(desired==current_)return;
        mciSendStringW(L"close aquarium_bgm",nullptr,0,nullptr);
        current_=desired;
        if(current_.empty())return;
        const auto path=folder_/(current_+L".wav");
        const auto command=L"open \""+path.wstring()+L"\" type mpegvideo alias aquarium_bgm";
        lastError_=mciSendStringW(command.c_str(),nullptr,0,nullptr);
        if(!lastError_)lastError_=mciSendStringW(L"play aquarium_bgm from 0 repeat",nullptr,0,nullptr);
        if(lastError_)OutputDebugStringW((L"BGM could not play: "+path.wstring()+L"\n").c_str());
    }
    MCIERROR LastError() const noexcept {return lastError_;}
    const std::wstring& CurrentTrack() const noexcept {return current_;}
private:
    std::filesystem::path folder_;
    std::wstring current_;
    MCIERROR lastError_=0;
};
}
