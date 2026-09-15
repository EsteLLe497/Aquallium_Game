#include "HeroineEncounter.h"
#include "DialogueTextCodec.h"
#include "PortraitPresentation.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

namespace story {
namespace {
std::vector<std::string> Split(const std::string& value) {
    std::vector<std::string> out;std::stringstream stream(value);std::string item;
    while(std::getline(stream,item,'|'))out.push_back(item);
    return out;
}
}
void HeroineEncounter::Initialize(ID3D11Device* device,const std::filesystem::path& storyFolder,const std::filesystem::path& textureFolder) {
    storyFolder_=storyFolder;lines_.clear();std::ifstream file(storyFolder/"heroine_encounter.dialogue");std::string value;
    while(std::getline(file,value)) {
        if(!value.empty()&&value.back()=='\r')value.pop_back();
        if(value.empty()||value[0]=='#')continue;
        auto fields=Split(value);if(fields.size()<5)continue;
        std::string text=fields[4];for(size_t i=5;i<fields.size();++i)text+='|'+fields[i];
        text=dialogueTextCodec::quoteHeroineSpeech(fields[1],dialogueTextCodec::decode(text));
        lines_.push_back({fields[1],fields[2],fields[3],std::move(text)});
    }
    portraits_=PortraitLibrary::load(device,textureFolder/"girl");
}
void HeroineEncounter::Reset(){phase_=Phase::Dormant;line_=0;letters_=fade_=0;emergencyFailed_=false;expression_="normal";}
void HeroineEncounter::Update(float dt,bool advance,float x,float floorY,float z,bool canTrigger) {
    if(phase_==Phase::Dormant) {
        // Crossing the first curved-gallery threshold starts once per game flow.
        if(canTrigger && floorY< -3.5f && z>87.35f && z<99.5f && x>-21.0f && x<1.0f) {
            // 初遭遇から通常会話と同じ上半身立ち絵をフェード表示する。
            phase_=Phase::PortraitIn;fade_=0;line_=0;letters_=0;
        }
        return;
    }
    if(phase_==Phase::Complete)return;
    const float fadeSpeed=1.6f;
    if(phase_==Phase::PortraitIn) {fade_=std::min(1.f,fade_+dt*fadeSpeed);if(fade_>=1){phase_=Phase::PortraitTalk;BeginLine(line_);}return;}
    if(phase_==Phase::PortraitOut) {fade_=std::max(0.f,fade_-dt*fadeSpeed);if(fade_<=0)phase_=Phase::Complete;return;}
    if(line_>=lines_.size()){phase_=Phase::PortraitOut;return;}
    letters_+=dt*22.f;
    if(advance) {
        const size_t count=Count(lines_[line_].text);
        if(letters_<float(count))letters_=float(count);else Advance();
    }
}
void HeroineEncounter::BeginLine(size_t line) {
    if(line>=lines_.size())return;line_=line;letters_=0;
    if(lines_[line].expression!="none")expression_=lines_[line].expression;
    if(lines_[line].event=="fail_emergency")emergencyFailed_=true;
}
void HeroineEncounter::Advance() {
    ++line_;
    if(line_>=lines_.size())phase_=Phase::PortraitOut;else BeginLine(line_);
}
const StoryTexture* HeroineEncounter::Portrait() const {
    return portraits_?portraits_->find(expression_):nullptr;
}
void HeroineEncounter::Draw() {
    if(phase_==Phase::Dormant||phase_==Phase::Complete)return;
    const ImVec2 size=ImGui::GetIO().DisplaySize;auto* background=ImGui::GetBackgroundDrawList();
    if(const StoryTexture* portrait=Portrait();portrait&&portrait->view) {
        const auto layout=portraitPresentation::upperBodyLayout(
            *portrait,{size.x-18,size.y+8},size.y*1.08f,size.x*.46f);
        if(const StoryTexture* underlay=portraits_->underlay(expression_);underlay&&underlay->view)
            background->AddImage(ImTextureRef(underlay->view.Get()),layout.minimum,layout.maximum,
                layout.uvMinimum,layout.uvMaximum,IM_COL32(255,255,255,int(255*fade_)));
        background->AddImage(ImTextureRef(portrait->view.Get()),layout.minimum,layout.maximum,
            layout.uvMinimum,layout.uvMaximum,IM_COL32(255,255,255,int(255*fade_)));
    }
    if(!DialogueVisible()||line_>=lines_.size())return;
    const float windowHeight=std::clamp(size.y*.27f,190.f,290.f);
    ImGui::SetNextWindowPos({size.x*.07f,size.y-windowHeight-24});ImGui::SetNextWindowSize({size.x*.86f,windowHeight});
    ImGui::SetNextWindowBgAlpha(.82f);ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.012f,.025f,.055f,.86f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{24,18});
    ImGui::Begin("##heroine_dialogue",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
    const ImVec2 panelMinimum=ImGui::GetWindowPos();
    auto* panelDraw=ImGui::GetWindowDrawList();
    panelDraw->AddRect(panelMinimum,{panelMinimum.x+ImGui::GetWindowWidth(),panelMinimum.y+windowHeight},
        IM_COL32(92,196,230,125),5.f,0,1.f);
    panelDraw->AddRectFilled({panelMinimum.x+1,panelMinimum.y+5},
        {panelMinimum.x+4,panelMinimum.y+windowHeight-5},IM_COL32(84,211,248,220),2.f);
    if(!lines_[line_].speaker.empty()) {
        ImGui::TextColored({.60f,.84f,1.f,1.f},"%s",lines_[line_].speaker.c_str());
        ImGui::Separator();
    }
    std::string shown=Prefix(lines_[line_].text,size_t(letters_));ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x-8);
    ImGui::TextUnformatted(shown.c_str());ImGui::PopTextWrapPos();
    if(size_t(letters_)>=Count(lines_[line_].text)){ImGui::SetCursorPos({24,windowHeight-43});ImGui::TextDisabled("クリック / F で次へ  ▽");}
    ImGui::End();ImGui::PopStyleVar();ImGui::PopStyleColor();
}
size_t HeroineEncounter::Count(const std::string& s){size_t n=0;for(unsigned char c:s)if((c&0xc0)!=0x80)++n;return n;}
std::string HeroineEncounter::Prefix(const std::string& s,size_t n){size_t i=0,c=0;for(;i<s.size();++i)if((static_cast<unsigned char>(s[i])&0xc0)!=0x80&&c++>=n)break;return s.substr(0,i);}
}
