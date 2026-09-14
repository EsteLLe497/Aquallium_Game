#include "SoundEffects.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace audio {
namespace {
std::wstring Alias(std::string_view name) {
    return L"aquarium_se_"+std::wstring(name.begin(),name.end());
}
}
SoundEffects::~SoundEffects() {
    PlaySoundW(nullptr,nullptr,0);
    for(const auto name:{"select","open","miss","glass","hand","kaigyo","footsound","beach",
                         "gaya","siren","transition","inwater"})
        mciSendStringW((L"close "+Alias(name)).c_str(),nullptr,0,nullptr);
    for(const auto& [name,voiceCount]:polyphonicVoiceCounts_)
        for(unsigned voice=0;voice<voiceCount;++voice)
            mciSendStringW((L"close "+Alias(
                name+"_voice_"+std::to_string(voice))).c_str(),nullptr,0,nullptr);
}
void SoundEffects::Initialize(const std::filesystem::path& folder){
    folder_=folder;BuildCreakWave();BuildBurstWave();BuildBangWave();BuildBootWave();
}
void SoundEffects::Play(std::string_view name,bool loop,float initialVolume) {
    const auto path=folder_/(std::string(name)+".mp3");
    PlayFile(name,path,loop,initialVolume);
}
void SoundEffects::PlayFile(std::string_view name,const std::filesystem::path& path,
                            bool loop,float initialVolume) {
    if(!std::filesystem::exists(path))return;
    const auto alias=Alias(name);
    mciSendStringW((L"close "+alias).c_str(),nullptr,0,nullptr);
    const std::wstring open=L"open \""+path.wstring()+L"\" type mpegvideo alias "+alias;
    if(mciSendStringW(open.c_str(),nullptr,0,nullptr)==0) {
        logicalVolumes_[std::string(name)]=std::clamp(initialVolume,0.f,1.f);
        SetVolume(name,initialVolume);
        mciSendStringW((L"play "+alias+L" from 0"+(loop?L" repeat":L"")).c_str(),nullptr,0,nullptr);
    }
}
bool SoundEffects::SetVolume(std::string_view name,float normalizedVolume){
    logicalVolumes_[std::string(name)]=std::clamp(normalizedVolume,0.f,1.f);
    return applyVolume(name,normalizedVolume);
}
// =========================================================
// 同一SEのポリフォニック再生
// =========================================================
void SoundEffects::PlayPolyphonic(
    std::string_view name,unsigned voiceCount,float initialVolume) {
    const std::string key(name);
    voiceCount=std::clamp(voiceCount,1u,16u);
    polyphonicVoiceCounts_[key]=std::max(polyphonicVoiceCounts_[key],voiceCount);
    unsigned& cursor=polyphonicVoiceCursors_[key];
    const std::string voiceName=key+"_voice_"+std::to_string(cursor%voiceCount);
    ++cursor;
    PlayFile(voiceName,folder_/(key+".mp3"),false,initialVolume);
}
bool SoundEffects::applyVolume(std::string_view name,float normalizedVolume){
    const int volume=static_cast<int>(std::lround(
        std::clamp(normalizedVolume,0.f,1.f)*masterVolume_*1000.f));
    return mciSendStringW((L"setaudio "+Alias(name)+L" volume to "+
        std::to_wstring(volume)).c_str(),nullptr,0,nullptr)==0;
}
// =========================================================
// 全SEの基準音量変更
// =========================================================
void SoundEffects::setMasterVolume(float normalizedVolume){
    const float next=std::clamp(normalizedVolume,0.f,1.f);
    if(std::abs(next-masterVolume_)<.001f)return;
    masterVolume_=next;
    for(const auto& [name,volume]:logicalVolumes_)applyVolume(name,volume);
    // メモリ生成SEは次回再生から新音量を使う。再構築前に非同期再生を止める。
    PlaySoundW(nullptr,nullptr,0);
    BuildCreakWave();BuildBurstWave();BuildBangWave();BuildBootWave();
}
void SoundEffects::Stop(std::string_view name){
    mciSendStringW((L"close "+Alias(name)).c_str(),nullptr,0,nullptr);
    logicalVolumes_.erase(std::string(name));
}
bool SoundEffects::Has(std::string_view name) const {
    return std::filesystem::exists(folder_/(std::string(name)+".mp3"));
}
void SoundEffects::BuildCreakWave(){
    constexpr std::uint32_t rate=22050,seconds=4,samples=rate*seconds,dataBytes=samples*2;
    creakWave_.assign(44+dataBytes,0);
    auto put16=[&](size_t at,std::uint16_t v){std::memcpy(creakWave_.data()+at,&v,2);};
    auto put32=[&](size_t at,std::uint32_t v){std::memcpy(creakWave_.data()+at,&v,4);};
    std::memcpy(creakWave_.data(),"RIFF",4);put32(4,36+dataBytes);
    std::memcpy(creakWave_.data()+8,"WAVEfmt ",8);put32(16,16);put16(20,1);put16(22,1);
    put32(24,rate);put32(28,rate*2);put16(32,2);put16(34,16);
    std::memcpy(creakWave_.data()+36,"data",4);put32(40,dataBytes);
    std::uint32_t random=0x7235a91u;float filteredNoise=0;
    auto* pcm=reinterpret_cast<std::int16_t*>(creakWave_.data()+44);
    for(std::uint32_t i=0;i<samples;++i){
        const float t=float(i)/rate,progress=t/seconds;
        random=random*1664525u+1013904223u;
        const float noise=float((random>>9)&0x7fffff)/float(0x3fffff)-1.f;
        filteredNoise=filteredNoise*.985f+noise*.015f;
        const float groan=std::sin(t*(54.f+std::sin(t*1.7f)*13.f))*
            std::sin(t*6.7f+std::sin(t*.9f));
        const float cracks=std::pow(std::max(0.f,std::sin(t*17.3f+std::sin(t*2.1f)*3.f)),18.f);
        const float envelope=(.10f+.72f*progress)*std::min(t*2.f,1.f)*std::min((seconds-t)*3.f,1.f);
        const float sample=std::clamp((groan*.48f+filteredNoise*1.7f+cracks*.22f)*envelope,-.92f,.92f);
        pcm[i]=static_cast<std::int16_t>(sample*masterVolume_*32767.f);
    }
}
void SoundEffects::PlayCreak(){
    if(!creakWave_.empty())PlaySoundW(reinterpret_cast<LPCWSTR>(creakWave_.data()),nullptr,
        SND_MEMORY|SND_ASYNC|SND_NODEFAULT);
}
void SoundEffects::BuildBurstWave(){
    constexpr std::uint32_t rate=22050,samples=rate,dataBytes=samples*2;
    burstWave_.assign(44+dataBytes,0);
    auto put16=[&](size_t at,std::uint16_t v){std::memcpy(burstWave_.data()+at,&v,2);};
    auto put32=[&](size_t at,std::uint32_t v){std::memcpy(burstWave_.data()+at,&v,4);};
    std::memcpy(burstWave_.data(),"RIFF",4);put32(4,36+dataBytes);
    std::memcpy(burstWave_.data()+8,"WAVEfmt ",8);put32(16,16);put16(20,1);put16(22,1);
    put32(24,rate);put32(28,rate*2);put16(32,2);put16(34,16);
    std::memcpy(burstWave_.data()+36,"data",4);put32(40,dataBytes);
    std::uint32_t random=0x51f15e5u;float low=0;
    auto* pcm=reinterpret_cast<std::int16_t*>(burstWave_.data()+44);
    for(std::uint32_t i=0;i<samples;++i){
        const float t=float(i)/rate,env=std::exp(-t*5.4f);
        random=random*1664525u+1013904223u;
        const float noise=float((random>>9)&0x7fffff)/float(0x3fffff)-1.f;
        low=low*.92f+noise*.08f;
        const float crack=noise*std::exp(-t*31.f);
        pcm[i]=static_cast<std::int16_t>(std::clamp((low*1.8f+crack*1.2f)*env,-.96f,.96f)*masterVolume_*32767.f);
    }
}
void SoundEffects::PlayBurst(){
    if(!burstWave_.empty())PlaySoundW(reinterpret_cast<LPCWSTR>(burstWave_.data()),nullptr,
        SND_MEMORY|SND_ASYNC|SND_NODEFAULT);
}
void SoundEffects::BuildBangWave(){
    constexpr std::uint32_t rate=22050,samples=rate/2,dataBytes=samples*2;
    bangWave_.assign(44+dataBytes,0);
    auto put16=[&](size_t at,std::uint16_t v){std::memcpy(bangWave_.data()+at,&v,2);};
    auto put32=[&](size_t at,std::uint32_t v){std::memcpy(bangWave_.data()+at,&v,4);};
    std::memcpy(bangWave_.data(),"RIFF",4);put32(4,36+dataBytes);
    std::memcpy(bangWave_.data()+8,"WAVEfmt ",8);put32(16,16);put16(20,1);put16(22,1);
    put32(24,rate);put32(28,rate*2);put16(32,2);put16(34,16);
    std::memcpy(bangWave_.data()+36,"data",4);put32(40,dataBytes);
    auto* pcm=reinterpret_cast<std::int16_t*>(bangWave_.data()+44);
    std::uint32_t random=0x91a733u;float low=0;
    for(std::uint32_t i=0;i<samples;++i){
        const float t=float(i)/rate,env=std::exp(-t*12.f);
        random=random*1664525u+1013904223u;
        const float noise=float((random>>9)&0x7fffff)/float(0x3fffff)-1.f;
        low=low*.965f+noise*.035f;
        const float body=std::sin(t*2.f*3.14159265f*48.f)*std::exp(-t*7.f);
        pcm[i]=static_cast<std::int16_t>(std::clamp((body*.78f+low*2.1f)*env,-.95f,.95f)*masterVolume_*32767.f);
    }
}
void SoundEffects::PlayBang(){
    if(!bangWave_.empty())PlaySoundW(reinterpret_cast<LPCWSTR>(bangWave_.data()),nullptr,
        SND_MEMORY|SND_ASYNC|SND_NODEFAULT);
}
void SoundEffects::BuildBootWave(){
    constexpr std::uint32_t rate=44100,samples=rate*8/5,dataBytes=samples*2;
    bootWave_.assign(44+dataBytes,0);
    auto put16=[&](size_t at,std::uint16_t v){std::memcpy(bootWave_.data()+at,&v,2);};
    auto put32=[&](size_t at,std::uint32_t v){std::memcpy(bootWave_.data()+at,&v,4);};
    std::memcpy(bootWave_.data(),"RIFF",4);put32(4,36+dataBytes);
    std::memcpy(bootWave_.data()+8,"WAVEfmt ",8);put32(16,16);put16(20,1);put16(22,1);
    put32(24,rate);put32(28,rate*2);put16(32,2);put16(34,16);
    std::memcpy(bootWave_.data()+36,"data",4);put32(40,dataBytes);
    auto* pcm=reinterpret_cast<std::int16_t*>(bootWave_.data()+44);
    float phase=0;std::uint32_t random=0x2001u;float motorNoise=0;
    for(std::uint32_t i=0;i<samples;++i){
        const float t=float(i)/rate,progress=t/1.6f;
        const float frequency=72.f+185.f*std::min(progress/.72f,1.f);
        phase+=6.2831853f*frequency/rate;
        random=random*1664525u+1013904223u;
        const float noise=float((random>>9)&0x7fffff)/float(0x3fffff)-1.f;
        motorNoise=motorNoise*.94f+noise*.06f;
        const float attack=std::min(t*9.f,1.f),release=std::min((1.6f-t)*4.f,1.f);
        const float envelope=attack*std::max(release,0.f);
        const float whirr=std::sin(phase)*.58f+std::sin(phase*2.01f)*.17f+motorNoise*.12f;
        pcm[i]=static_cast<std::int16_t>(std::clamp(whirr*envelope,-.9f,.9f)*masterVolume_*32767.f);
    }
}
bool SoundEffects::PlayBoot(){
    return !bootWave_.empty()&&PlaySoundW(reinterpret_cast<LPCWSTR>(bootWave_.data()),nullptr,
        SND_MEMORY|SND_ASYNC|SND_NODEFAULT)!=FALSE;
}
void SoundEffects::PlayFootsteps(float listenerX,float listenerZ,float listenerYaw){
    const float forwardX=std::sin(listenerYaw),forwardZ=std::cos(listenerYaw);
    // 遠すぎる初期値ではマスター音量50%時にほぼ無音になるため、
    // 姿は見えないが確実に気付ける距離から追従を開始する。
    footstepX_=listenerX-forwardX*5.5f;
    footstepZ_=listenerZ-forwardZ*5.5f;
    footstepUpdateClock_=0;
    Play("footsound",true);
    footstepsActive_=mciGetDeviceIDW(L"aquarium_se_footsound")!=0;
    ApplyFootstepSpatial(listenerX,listenerZ,listenerYaw);
}
void SoundEffects::UpdateFootsteps(float dt,float listenerX,float listenerZ,float listenerYaw){
    if(!footstepsActive_||dt<=0)return;
    // Chase the listener's recent trail. Exponential pursuit keeps the source
    // moving smoothly even through corners and never teleports with the camera.
    const float forwardX=std::sin(listenerYaw),forwardZ=std::cos(listenerYaw);
    const float targetX=listenerX-forwardX*1.25f,targetZ=listenerZ-forwardZ*1.25f;
    const float follow=1-std::exp(-std::min(dt,.1f)*.34f);
    footstepX_+=(targetX-footstepX_)*follow;
    footstepZ_+=(targetZ-footstepZ_)*follow;
    footstepUpdateClock_+=dt;
    if(footstepUpdateClock_>=.05f){
        footstepUpdateClock_=0;ApplyFootstepSpatial(listenerX,listenerZ,listenerYaw);
    }
}
void SoundEffects::ApplyFootstepSpatial(float listenerX,float listenerZ,float listenerYaw){
    const float dx=footstepX_-listenerX,dz=footstepZ_-listenerZ;
    footstepDistance_=std::sqrt(dx*dx+dz*dz);
    const float inverseDistance=1/std::max(footstepDistance_,.001f);
    const float rightX=std::cos(listenerYaw),rightZ=-std::sin(listenerYaw);
    const float pan=std::clamp((dx*rightX+dz*rightZ)*inverseDistance,-1.f,1.f);
    // Audible but distant at spawn; approaches full level as the pursuer closes.
    const float gain=std::clamp((8.f-footstepDistance_)/7.f,.18f,1.f);
    const int left=int(1000*masterVolume_*gain*(1-.72f*std::max(pan,0.f)));
    const int right=int(1000*masterVolume_*gain*(1+.72f*std::min(pan,0.f)));
    const auto alias=Alias("footsound");
    footstepSpatialError_=mciSendStringW(
        (L"setaudio "+alias+L" left volume to "+std::to_wstring(left)).c_str(),nullptr,0,nullptr);
    const MCIERROR rightError=mciSendStringW(
        (L"setaudio "+alias+L" right volume to "+std::to_wstring(right)).c_str(),nullptr,0,nullptr);
    if(!footstepSpatialError_)footstepSpatialError_=rightError;
}
void SoundEffects::StopGenerated(){
    PlaySoundW(nullptr,nullptr,0);
    mciSendStringW(L"close aquarium_se_footsound",nullptr,0,nullptr);
    logicalVolumes_.erase("footsound");
    footstepsActive_=false;footstepDistance_=footstepUpdateClock_=0;
}
}
