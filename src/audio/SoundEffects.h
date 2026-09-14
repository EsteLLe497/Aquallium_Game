#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <filesystem>
#include <cstdint>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

namespace audio {
// 短いMP3効果音をWindows標準のMCIで非同期再生する軽量プレイヤー。
// select/open/miss は asset/sound/se の同名MP3へ対応する。
class SoundEffects {
public:
    ~SoundEffects();
    void Initialize(const std::filesystem::path& folder);
    void Play(std::string_view name,bool loop=false,float initialVolume=1.0f);
    // 短い同一SEを独立した複数チャンネルで重ね、途中で上書きしない。
    void PlayPolyphonic(std::string_view name,unsigned voiceCount=8,
                        float initialVolume=1.0f);
    void PlayFile(std::string_view name,const std::filesystem::path& path,
                  bool loop=false,float initialVolume=1.0f);
    bool SetVolume(std::string_view name,float normalizedVolume);
    void setMasterVolume(float normalizedVolume);
    void Stop(std::string_view name);
    [[nodiscard]] bool Has(std::string_view name) const;
    // 外部アセットを増やさず、徐々に強くなる水槽の軋みをメモリ上で再生する。
    void PlayCreak();
    void PlayBurst();
    void PlayBang();
    bool PlayBoot();
    void PlayFootsteps(float listenerX,float listenerZ,float listenerYaw);
    void UpdateFootsteps(float deltaTime,float listenerX,float listenerZ,float listenerYaw);
    void StopGenerated();
    [[nodiscard]] bool FootstepsActive() const noexcept {return footstepsActive_;}
    [[nodiscard]] float FootstepDistance() const noexcept {return footstepDistance_;}
    [[nodiscard]] MCIERROR FootstepSpatialError() const noexcept {return footstepSpatialError_;}
private:
    void BuildCreakWave();
    void BuildBurstWave();
    void BuildBangWave();
    void BuildBootWave();
    void ApplyFootstepSpatial(float listenerX,float listenerZ,float listenerYaw);
    bool applyVolume(std::string_view name,float normalizedVolume);
    std::filesystem::path folder_;
    std::vector<std::uint8_t> creakWave_;
    std::vector<std::uint8_t> burstWave_;
    std::vector<std::uint8_t> bangWave_;
    std::vector<std::uint8_t> bootWave_;
    float footstepX_=0,footstepZ_=0,footstepDistance_=0,footstepUpdateClock_=0;
    bool footstepsActive_=false;
    MCIERROR footstepSpatialError_=0;
    float masterVolume_=.5f;
    std::unordered_map<std::string,float> logicalVolumes_;
    std::unordered_map<std::string,unsigned> polyphonicVoiceCursors_;
    std::unordered_map<std::string,unsigned> polyphonicVoiceCounts_;
};
}
