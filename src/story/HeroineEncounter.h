#pragma once
#include "StoryTexture.h"
#include "../../third_party/imgui/imgui.h"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace story {
class HeroineEncounter {
public:
    enum class Phase { Dormant,StillIn,StillTalk,StillOut,PortraitIn,PortraitTalk,PortraitOut,Complete };
    void Initialize(ID3D11Device* device,const std::filesystem::path& storyFolder,const std::filesystem::path& textureFolder);
    void Reset();
    void MarkComplete(){phase_=Phase::Complete;emergencyFailed_=true;}
    void Update(float dt,bool advance,float x,float floorY,float z,bool canTrigger);
    void Draw();
    bool BlocksPlayer() const {return phase_!=Phase::Dormant && phase_!=Phase::Complete;}
    bool DialogueVisible() const {return phase_==Phase::StillTalk||phase_==Phase::PortraitTalk;}
    bool EmergencyFailed() const {return emergencyFailed_;}
    bool Complete() const {return phase_==Phase::Complete;}
private:
    struct Line {bool still=false;std::string speaker,expression,event,text;};
    static size_t Count(const std::string& text);
    static std::string Prefix(const std::string& text,size_t count);
    void BeginLine(size_t line);
    void Advance();
    const StoryTexture* Portrait() const;
    std::vector<Line> lines_;std::filesystem::path storyFolder_;
    StoryTexture still_;std::array<StoryTexture,5> portraits_;
    Phase phase_=Phase::Dormant;size_t line_=0;float letters_=0,fade_=0;
    bool emergencyFailed_=false;std::string expression_="normal";
};
}
