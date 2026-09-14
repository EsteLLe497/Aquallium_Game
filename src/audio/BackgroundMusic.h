#pragma once
#include <filesystem>
#include <string>

namespace audio {
// シーンごとのBGMを一つのMCIデバイスで切り替える軽量プレイヤー。
class BackgroundMusic {
public:
    enum class Track {Silent,Title,Aquarium,Chase,Actually,Ending};
    ~BackgroundMusic();
    void initialize(const std::filesystem::path& folder);
    // 希望曲を予約し、advanceで停止・切替・再生開始を滑らかにつなぐ。
    void update(Track track);
    void advance(float deltaTime);
    void setVolume(float normalizedVolume);
    [[nodiscard]] unsigned long lastError() const noexcept {return lastError_;}
    [[nodiscard]] Track currentTrack() const noexcept {return current_;}
private:
    static std::filesystem::path fileFor(Track track,const std::filesystem::path& folder);
    bool openTrack(Track track);
    void closeCurrent();
    void applyVolume();
    std::filesystem::path folder_;
    Track current_=Track::Silent;
    Track target_=Track::Silent;
    float volume_=.5f;
    float fadeGain_=0.f;
    bool deviceOpen_=false;
    unsigned long lastError_=0;
};
}
