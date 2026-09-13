#include "HeroineEncounter.h"
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
        lines_.push_back({fields[0]=="still",fields[1],fields[2],fields[3],std::move(text)});
    }
    still_.Load(device,textureFolder/"stile"/"HeroineEncounter_placeholder.png");
    const char* names[]={"normal","ase","yan","hiki","nihi"};
    for(size_t i=0;i<portraits_.size();++i)portraits_[i].Load(device,textureFolder/"girl"/(std::string("Heroin_")+names[i]+".PNG"));
}
void HeroineEncounter::Reset(){phase_=Phase::Dormant;line_=0;letters_=fade_=0;emergencyFailed_=false;expression_="normal";}
void HeroineEncounter::Update(float dt,bool advance,float x,float floorY,float z,bool canTrigger) {
    if(phase_==Phase::Dormant) {
        // Crossing the first curved-gallery threshold starts once per game flow.
        if(canTrigger && floorY< -3.5f && z>87.35f && z<99.5f && x>-21.0f && x<1.0f) {
            phase_=Phase::StillIn;fade_=0;line_=0;letters_=0;
        }
        return;
    }
    if(phase_==Phase::Complete)return;
    const float fadeSpeed=1.6f;
    if(phase_==Phase::StillIn) {fade_=std::min(1.f,fade_+dt*fadeSpeed);if(fade_>=1){phase_=Phase::StillTalk;BeginLine(0);}return;}
    if(phase_==Phase::StillOut) {fade_=std::max(0.f,fade_-dt*fadeSpeed);if(fade_<=0){phase_=Phase::PortraitIn;while(line_<lines_.size()&&lines_[line_].still)++line_;}return;}
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
    if(phase_==Phase::StillTalk) {
        if(line_>=lines_.size()||!lines_[line_].still)phase_=Phase::StillOut;
        else BeginLine(line_);
    } else {
        if(line_>=lines_.size())phase_=Phase::PortraitOut;else BeginLine(line_);
    }
}
const StoryTexture* HeroineEncounter::Portrait() const {
    const char* names[]={"normal","ase","yan","hiki","nihi"};
    for(size_t i=0;i<portraits_.size();++i)if(expression_==names[i])return &portraits_[i];
    return &portraits_[0];
}
void HeroineEncounter::Draw() {
    if(phase_==Phase::Dormant||phase_==Phase::Complete)return;
    const ImVec2 size=ImGui::GetIO().DisplaySize;auto* background=ImGui::GetBackgroundDrawList();
    bool stillPhase=phase_==Phase::StillIn||phase_==Phase::StillTalk||phase_==Phase::StillOut;
    if(stillPhase&&still_.view) {
        float imageAspect=float(still_.width)/still_.height,screenAspect=size.x/size.y;
        ImVec2 uv0{0,0},uv1{1,1};
        if(imageAspect>screenAspect){float visible=screenAspect/imageAspect;uv0.x=(1-visible)*.5f;uv1.x=1-uv0.x;}
        else {float visible=imageAspect/screenAspect;uv0.y=(1-visible)*.5f;uv1.y=1-uv0.y;}
        background->AddImage(ImTextureRef(still_.view.Get()),{0,0},size,uv0,uv1,IM_COL32(255,255,255,int(255*fade_)));
        background->AddRectFilled({0,0},size,IM_COL32(2,8,18,int(55*fade_)));
    } else if(const StoryTexture* portrait=Portrait();portrait&&portrait->view) {
        float h=size.y*.92f,w=h*portrait->width/portrait->height;
        background->AddImage(ImTextureRef(portrait->view.Get()),{size.x-w-18,size.y-h+8},{size.x-18,size.y+8},{0,0},{1,1},IM_COL32(255,255,255,int(255*fade_)));
    }
    if(!DialogueVisible()||line_>=lines_.size())return;
    const float windowHeight=std::clamp(size.y*.27f,190.f,290.f);
    ImGui::SetNextWindowPos({size.x*.07f,size.y-windowHeight-24});ImGui::SetNextWindowSize({size.x*.86f,windowHeight});
    ImGui::SetNextWindowBgAlpha(.82f);ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.012f,.025f,.055f,.86f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{24,18});
    ImGui::Begin("##heroine_dialogue",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
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
