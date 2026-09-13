#pragma once
#include "../rendering/AquariumRenderer.h"
#include "../../third_party/imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace story {
// 使い方: Load(asset/story) → Start() → 毎フレーム Tick / Draw。
// セリフ追加は opening.dialogue、カメラ追加は opening.camera を編集。
// F2 の Camera/Story ツールからキーを変更・保存・スクラブできる。
// 出口を調べた事実は ExitChecked()、会話完了後は FindingEmergency()。
class OpeningFlow {
public:
    struct Key { float t,x,y,z,yaw,pitch,blur,blink; };
    enum class Phase { Off, Wake, Dialogue, Stand, Free, ExitDialogue, FindEmergency };
    void Load(const std::filesystem::path& folder) {
        folder_=folder;
        std::vector<Key> keys; std::vector<std::string> lines;
        std::ifstream camera(folder/"opening.camera"); std::string s;
        while(std::getline(camera,s)) {
            if(s.empty() || s[0]=='#') continue;
            std::replace(s.begin(),s.end(),',',' '); std::istringstream in(s); Key k{};
            if(in>>k.t>>k.x>>k.y>>k.z>>k.yaw>>k.pitch>>k.blur>>k.blink) keys.push_back(k);
        }
        std::ifstream dialogue(folder/"opening.dialogue");
        while(std::getline(dialogue,s)) {
            if(!s.empty() && s.back()=='\r') s.pop_back();
            if(!s.empty() && s[0]!='#') lines.push_back(s);
        }
        if(keys.size()<2 || lines.empty()) { error_="Camera/dialogue file is missing or invalid"; return; }
        std::stable_sort(keys.begin(),keys.end(),[](auto a,auto b){return a.t<b.t;});
        if(keys.front().t!=0 || keys.back().t<=5) {error_="Camera must start at 0 and end after 5 seconds";return;}
        for(size_t i=0;i<keys.size();++i) {
            const auto& k=keys[i];
            if(!std::isfinite(k.t)||!std::isfinite(k.x)||!std::isfinite(k.y)||!std::isfinite(k.z)||
               !std::isfinite(k.yaw)||!std::isfinite(k.pitch)||!std::isfinite(k.blur)||!std::isfinite(k.blink)||
               (i && k.t<=keys[i-1].t)) {error_="Camera keys must be finite with unique increasing times";return;}
        }
        keys_=std::move(keys); lines_=std::move(lines); error_.clear();
        if(phase_!=Phase::ExitDialogue) line_=std::min(line_,lines_.size()-1);
        std::ifstream exitFile(folder/"exit.dialogue");
        std::vector<std::string> exitLines;
        while(std::getline(exitFile,s)) {
            if(!s.empty() && s.back()=='\r') s.pop_back();
            if(!s.empty() && s[0]!='#') exitLines.push_back(s);
        }
        if(!exitLines.empty()) exitLines_=std::move(exitLines);
        if(phase_==Phase::ExitDialogue) line_=std::min(line_,exitLines_.size()-1);
    }
    void Start() { if(keys_.empty()) return; phase_=Phase::Wake; clock_=0; line_=0; letters_=0; preview_=false;emergencyFailed_=powerMission_=clueCollected_=managementEntered_=powerRestored_=restCompleted_=manualCollected_=facilityPasswordCollected_=false; }
    void Stop() { phase_=Phase::Off; preview_=false; }
    bool ControlsCamera() const { return preview_ || phase_==Phase::Wake || phase_==Phase::Dialogue || phase_==Phase::Stand; }
    bool BlocksPlayer() const { return ControlsCamera() || phase_==Phase::ExitDialogue; }
    bool ExitChecked() const { return phase_==Phase::ExitDialogue || phase_==Phase::FindEmergency; }
    bool FindingEmergency() const { return phase_==Phase::FindEmergency && !powerMission_; }
    int MissionIndex() const {return facilityPasswordCollected_?1:restCompleted_?4:powerRestored_?3:powerMission_?2:FindingEmergency()?1:0;}
    bool PowerMission() const{return powerMission_;}
    bool ClueCollected() const{return clueCollected_;}
    bool ManagementEntered() const{return managementEntered_;}
    bool PowerRestored() const{return powerRestored_;}
    bool RestCompleted() const{return restCompleted_;}
    bool ManualCollected() const{return manualCollected_;}
    bool FacilityPasswordCollected() const{return facilityPasswordCollected_;}
    void MarkEmergencyFailed(){emergencyFailed_=true;}
    void BeginPowerMission(){emergencyFailed_=true;powerMission_=true;}
    void SetClueCollected(){clueCollected_=true;}
    void SetManagementEntered(){managementEntered_=true;}
    void SetPowerRestored(){powerRestored_=true;managementEntered_=true;}
    void CompleteTerraceRest(){if(powerRestored_)restCompleted_=true;}
    void SetManualCollected(){if(restCompleted_)manualCollected_=true;}
    void SetFacilityPasswordCollected(){if(restCompleted_)facilityPasswordCollected_=true;}
    void RestoreProgress(bool heroineJoined,bool findingEmergency,bool clue,bool managementEntered,bool powerRestored=false,
                         bool restCompleted=false,bool manualCollected=false,bool facilityPasswordCollected=false){
        phase_=findingEmergency&&!heroineJoined?Phase::FindEmergency:Phase::Free;clock_=letters_=0;preview_=false;
        emergencyFailed_=heroineJoined;powerMission_=heroineJoined;
        clueCollected_=clue;managementEntered_=managementEntered;powerRestored_=powerRestored;
        restCompleted_=restCompleted;manualCollected_=manualCollected;
        facilityPasswordCollected_=facilityPasswordCollected;
    }
    // AQUARIUM_START_ZONE=ENCOUNTER 専用。通常のゲーム進行では呼ばない。
    void BeginEmergencySearchForQa(){phase_=Phase::FindEmergency;line_=0;letters_=0;preview_=false;emergencyFailed_=powerMission_=clueCollected_=managementEntered_=powerRestored_=restCompleted_=manualCollected_=facilityPasswordCollected_=false;}
    // 距離だけでは達成しない。視線のヒットと選択入力を確認した呼び出し元だけが実行する。
    void InspectExit() {
        if(phase_!=Phase::Free) return;
        phase_=Phase::ExitDialogue;line_=0;letters_=0;
    }
    void Tick(float dt,bool click,bool editor,AquariumSettings& settings) {
        if(!editor && !preview_) {
            if(phase_==Phase::Wake) {clock_=std::min(5.f,clock_+dt); if(clock_>=5) phase_=Phase::Dialogue;}
            else if(phase_==Phase::Dialogue || phase_==Phase::ExitDialogue) {
                const auto& dialogue=phase_==Phase::ExitDialogue ? exitLines_ : lines_;
                letters_+=dt*charactersPerSecond_;
                const size_t count=Count(dialogue[line_]);
                if(click) {
                    if(letters_<float(count)) letters_=float(count);
                    else if(++line_<dialogue.size()) letters_=0;
                    else if(phase_==Phase::ExitDialogue) phase_=Phase::FindEmergency;
                    else {phase_=Phase::Stand;clock_=5;}
                }
            } else if(phase_==Phase::Stand) {
                clock_=std::min(keys_.back().t,clock_+dt);
                if(clock_>=keys_.back().t) { Apply(settings); phase_=Phase::Free; }
            }
        }
        if(ControlsCamera()) Apply(settings); else {settings.wakeBlur=0; blink_=0;}
    }
    void Draw(bool editor=false,bool hudVisible=true) {
        if(phase_==Phase::Off && !preview_) return;
        const ImVec2 size=ImGui::GetIO().DisplaySize;
        if((phase_==Phase::Dialogue || phase_==Phase::ExitDialogue) && !preview_) {
            const auto& dialogue=phase_==Phase::ExitDialogue ? exitLines_ : lines_;
            ImGui::SetNextWindowPos({size.x*.10f,size.y*.76f});
            ImGui::SetNextWindowSize({size.x*.8f,size.y*.20f});
            ImGui::SetNextWindowBgAlpha(.76f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.018f,.035f,.07f,.8f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(24,18));
            ImGui::Begin("##dialogue",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
            const std::string visible=Prefix(dialogue[line_],size_t(letters_));
            ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(visible.c_str()); ImGui::PopTextWrapPos();
            if(size_t(letters_)>=Count(dialogue[line_])) {
                ImGui::SetCursorPosY(size.y*.20f-45); ImGui::TextDisabled("クリック / F で次へ  ▽");
            }
            ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor();
        }
        if(hudVisible && (phase_==Phase::Free || ExitChecked())) {
            auto* d=ImGui::GetForegroundDrawList();
            const float bottom=facilityPasswordCollected_?78.f:
                (((powerMission_&&!powerRestored_)||restCompleted_)?132.f:78.f);
            d->AddRectFilled({size.x-330,25},{size.x-20,bottom},IM_COL32(8,18,30,120),8);
            auto mission=[&](float y,const char* text,int mark){
                d->AddRect({size.x-310,y+5},{size.x-292,y+23},IM_COL32(210,225,240,150));
                if(mark==1){d->AddLine({size.x-308,y+14},{size.x-302,y+20},IM_COL32(160,225,210,220),2);d->AddLine({size.x-302,y+20},{size.x-294,y+8},IM_COL32(160,225,210,220),2);}
                if(mark==2){d->AddLine({size.x-307,y+8},{size.x-295,y+20},IM_COL32(230,115,125,225),2);d->AddLine({size.x-295,y+8},{size.x-307,y+20},IM_COL32(230,115,125,225),2);}
                d->AddText({size.x-277,y},IM_COL32(220,230,240,210),text);
            };
            if(facilityPasswordCollected_)mission(35,"非常口に向かう",0);
            else if(restCompleted_){
                mission(35,"マニュアルを入手する",manualCollected_?1:0);
                mission(65,"パスワードを入力する",facilityPasswordCollected_?1:0);
            }
            else if(powerRestored_)mission(35,"テラスのベンチで休む",0);
            else if(powerMission_){
                mission(35,"館内の電力を復旧する",powerRestored_?1:0);
                mission(65,"管理室に入る",managementEntered_?1:0);
                mission(95,"任意  2F展示室を調べる",clueCollected_?1:0);
            }
            else if(FindingEmergency())mission(35,"非常口を探す",emergencyFailed_?2:0);
            else mission(35,"出口に向かう",phase_==Phase::ExitDialogue?1:0);
            if(phase_==Phase::Free) {
            d->AddRectFilled({20,size.y-55},{size.x-20,size.y-12},IM_COL32(8,18,30,95),5);
            d->AddText({32,size.y-49},IM_COL32(210,224,236,185),"WASD 移動   マウス 視点操作   F / 左クリック 選択   Shift ダッシュ");
            }
        }
        // 湾曲した上下のまぶた。時間キーで閉じ率を編集できる。
        if(blink_>.001f && !editor) {
            auto* d=ImGui::GetForegroundDrawList();
            for(int i=0;i<64;++i) {
                float x=float(i)/64, edge=std::abs(x*2-1);
                float h=size.y*.5f*std::clamp(blink_+edge*edge*.20f*blink_,0.f,1.f);
                d->AddRectFilled({x*size.x,0},{(x+1.f/64)*size.x,h},IM_COL32(0,0,0,255));
                d->AddRectFilled({x*size.x,size.y-h},{(x+1.f/64)*size.x,size.y},IM_COL32(0,0,0,255));
            }
        }
    }
    void Editor(AquariumSettings& settings) {
        ImGui::SetNextWindowSize({520,500},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({std::max(10.f,ImGui::GetIO().DisplaySize.x-540),85},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(.96f);
        ImGui::Begin("Story / Camera preview");
        ImGui::SetWindowFontScale(.72f);
        ImGui::TextUnformatted("F2: close tools / mouse returns to game");
        ImGui::TextWrapped("Data: %s",folder_.u8string().c_str());
        if(ImGui::Button("Reload files")) Load(folder_);
        ImGui::SameLine(); if(ImGui::Button("Restart opening")) Start();
        ImGui::Checkbox("Scrub camera (locks gameplay)",&preview_);
        if(ImGui::Button("Resume story from preview")) {
            preview_=false;
            phase_=clock_<5 ? Phase::Wake : Phase::Stand;
        }
        if(!keys_.empty()) {
            ImGui::SliderFloat("Time",&clock_,0,keys_.back().t,"%.2f sec");
            selected_=std::clamp(selected_,0,int(keys_.size())-1);
            ImGui::SliderInt("Key",&selected_,0,int(keys_.size())-1);
            Key& k=keys_[selected_];
            ImGui::DragFloat3("Position",&k.x,.02f); ImGui::DragFloat2("Yaw / Pitch (degrees)",&k.yaw,.2f);
            ImGui::SliderFloat("Blur",&k.blur,0,1); ImGui::SliderFloat("Blink",&k.blink,0,1);
            if(ImGui::Button("Preview selected key")) {clock_=k.t;preview_=true;}
            if(ImGui::Button("Append current camera (+1 sec)")) keys_.push_back({keys_.back().t+1,settings.cameraPositionX,settings.cameraPositionY,settings.cameraPositionZ,settings.cameraYaw*57.29578f,settings.cameraPitch*57.29578f,0,0});
            if(ImGui::Button("Save camera keys")) {
                std::ofstream out(folder_/"opening.camera");
                out<<"# seconds,x,y,z,yawDegrees,pitchDegrees,blur,blink\n";
                for(auto a:keys_) out<<a.t<<','<<a.x<<','<<a.y<<','<<a.z<<','<<a.yaw<<','<<a.pitch<<','<<a.blur<<','<<a.blink<<'\n';
                if(!out) error_="Cannot save camera file";
            }
        }
        ImGui::SliderFloat("Characters / sec",&charactersPerSecond_,8,50);
        ImGui::TextWrapped("Dialogue: asset/story/opening.dialogue (UTF-8). One line per message. Reload after editing.");
        if(!error_.empty()) ImGui::TextWrapped("%s",error_.c_str());
        ImGui::End();
    }
private:
    static size_t Count(const std::string& s) {size_t n=0;for(unsigned char c:s) if((c&0xc0)!=0x80) ++n;return n;}
    static std::string Prefix(const std::string& s,size_t n) {size_t i=0,c=0;for(;i<s.size();++i) if((static_cast<unsigned char>(s[i])&0xc0)!=0x80 && c++>=n) break;return s.substr(0,i);}
    void Apply(AquariumSettings& s) {
        if(keys_.empty())return;
        size_t i=0;while(i+1<keys_.size() && keys_[i+1].t<clock_)++i;
        auto a=keys_[i],b=keys_[std::min(i+1,keys_.size()-1)];
        float t=std::clamp((clock_-a.t)/std::max(.001f,b.t-a.t),0.f,1.f);t=t*t*(3-2*t);
        auto mix=[t](float x,float y){return x+(y-x)*t;};
        s.cameraPositionX=mix(a.x,b.x);s.cameraPositionY=mix(a.y,b.y);s.cameraPositionZ=mix(a.z,b.z);
        s.cameraYaw=mix(a.yaw,b.yaw)*.0174532925f;s.cameraPitch=mix(a.pitch,b.pitch)*.0174532925f;
        s.wakeBlur=mix(a.blur,b.blur);blink_=mix(a.blink,b.blink);
    }
    std::filesystem::path folder_; std::vector<Key> keys_; std::vector<std::string> lines_;
    std::vector<std::string> exitLines_{"開かない・・・","非常口とかあるだろうし、探してみるか"};
    Phase phase_=Phase::Off;float clock_=0,letters_=0,charactersPerSecond_=22,blink_=0;
    size_t line_=0;int selected_=0;bool preview_=false,emergencyFailed_=false,powerMission_=false;
    bool clueCollected_=false,managementEntered_=false,powerRestored_=false;
    bool restCompleted_=false,manualCollected_=false,facilityPasswordCollected_=false;
    std::string error_;
};
}
