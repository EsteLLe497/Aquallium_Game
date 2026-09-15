#include "DialoguePlayer.h"
#include "DialogueTextCodec.h"
#include "PortraitPresentation.h"
#include "../../third_party/imgui/imgui.h"
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
void DialoguePlayer::Initialize(ID3D11Device* device,const std::filesystem::path& root) {
    portraits_=PortraitLibrary::load(device,root/"girl");
    clueImage_.Load(device,root/"other"/"nozo1.png");
}
bool DialoguePlayer::Start(const std::filesystem::path& script,bool showClueImage,size_t initialLine) {
    std::ifstream file(script);std::string value;lines_.clear();
    while(std::getline(file,value)) {
        if(!value.empty()&&value.back()=='\r')value.pop_back();
        if(value.empty()||value[0]=='#')continue;
        auto fields=Split(value);Line line;
        if(fields.size()==1)line={"","hide",fields[0]};
        else if(fields.size()>=5&&(fields[0]=="still"||fields[0]=="portrait")){
            line={fields[1],fields[2],fields[4]};for(size_t i=5;i<fields.size();++i)line.text+='|'+fields[i];
        }else if(fields.size()>=3){
            line={fields[0],fields[1],fields[2]};for(size_t i=3;i<fields.size();++i)line.text+='|'+fields[i];
        }else continue;
        line.text=dialogueTextCodec::quoteHeroineSpeech(
            line.speaker,dialogueTextCodec::decode(line.text));
        const auto tagAt=line.expression.find('@');
        if(tagAt!=std::string::npos) {
            const std::string tags=line.expression.substr(tagAt+1);
            line.expression.resize(tagAt);
            if(tags.find("fast")!=std::string::npos)line.speed=44.f;
            if(tags.find("auto")!=std::string::npos)line.autoDelay=.28f;
            if(tags.find("small")!=std::string::npos)line.textScale=.72f;
            if(tags.find("bgmActually")!=std::string::npos)line.cue=Cue::ActuallyBgm;
        }
        lines_.push_back(std::move(line));
    }
    if(lines_.empty())return false;
    line_=std::min(initialLine,lines_.size()-1);letters_=fade_=0;showClueImage_=showClueImage;
    // A new script is a new conversation part. Do not leak the previous
    // part's portrait into an opening protagonist line; BeginLine still keeps
    // the latest heroine expression for protagonist lines within this script.
    expression_="hide";
    for(size_t i=0;i<=line_;++i)
        if(!lines_[i].speaker.empty()&&lines_[i].expression!="none")expression_=lines_[i].expression;
    BeginLine(line_);phase_=Phase::FadeIn;
    return true;
}
bool DialoguePlayer::ShowStatic(const std::filesystem::path& script,size_t line) {
    if(!Start(script,false,line))return false;
    phase_=Phase::Talk;fade_=1.f;letters_=float(Count(lines_[line_].text));return true;
}
void DialoguePlayer::Update(float dt,bool advance) {
    if(phase_==Phase::Dormant||phase_==Phase::Complete)return;
    if(phase_==Phase::FadeIn){fade_=std::min(1.f,fade_+dt*2.2f);if(fade_>=1)phase_=Phase::Talk;return;}
    if(phase_==Phase::FadeOut){fade_=std::max(0.f,fade_-dt*2.2f);if(fade_<=0)phase_=Phase::Complete;return;}
    const size_t count=Count(lines_[line_].text);
    letters_+=dt*lines_[line_].speed;
    if(letters_>=float(count)&&lines_[line_].autoDelay>=0) {
        autoClock_+=dt;
        if(autoClock_>=lines_[line_].autoDelay) {
            if(++line_>=lines_.size()){phase_=Phase::FadeOut;return;}
            BeginLine(line_);
        }
        return;
    }
    if(!advance)return;
    if(letters_<float(count)){letters_=float(count);return;}
    if(++line_>=lines_.size()){phase_=Phase::FadeOut;return;}
    BeginLine(line_);
}
void DialoguePlayer::UpdatePreview(float dt) {
    Update(dt,false);
    if(phase_!=Phase::Talk||line_>=lines_.size())return;
    if(letters_>=float(Count(lines_[line_].text))&&lines_[line_].autoDelay<0){
        autoClock_+=dt;
        if(autoClock_>=.85f)Update(0.f,true);
    }
}
void DialoguePlayer::BeginLine(size_t index) {
    line_=index;letters_=autoClock_=0;
    if(!lines_[line_].speaker.empty() && lines_[line_].expression!="none")
        expression_=lines_[line_].expression;
}
float DialoguePlayer::textFontSize(float viewportHeight,float textScale) {
    return std::clamp(viewportHeight*.032f,13.f,24.f)*textScale;
}
float DialoguePlayer::textWrapWidth(float viewportWidth) {
    return viewportWidth*.86f-36.f;
}
const StoryTexture* DialoguePlayer::Portrait() const {
    return portraits_?portraits_->find(expression_):nullptr;
}
void DialoguePlayer::DrawOverlay(ImDrawList* background,float x,float y,float width,float height) const {
    if(!Active()||!background||width<=0||height<=0)return;
    const ImVec2 origin{x,y},screen{width,height};
    background->AddRectFilled(origin,{x+width,y+height},IM_COL32(2,7,16,int(105*fade_)));
    if(showClueImage_&&clueImage_.view) {
        const float h=screen.y*.48f,w=h*clueImage_.width/clueImage_.height;
        const ImVec2 p0{x+screen.x*.42f-w*.5f,y+screen.y*.08f};
        background->AddImage(ImTextureRef(clueImage_.view.Get()),p0,{p0.x+w,p0.y+h},{0,0},{1,1},IM_COL32(255,255,255,int(255*fade_)));
    }
    if(const auto* portrait=Portrait();portrait&&portrait->view) {
        const auto layout=portraitPresentation::upperBodyLayout(
            *portrait,{x+screen.x-16,y+screen.y+8},screen.y*1.08f,screen.x*.46f);
        if(const StoryTexture* underlay=portraits_->underlay(expression_);underlay&&underlay->view)
            background->AddImage(ImTextureRef(underlay->view.Get()),layout.minimum,layout.maximum,
                layout.uvMinimum,layout.uvMaximum,IM_COL32(255,255,255,int(255*fade_)));
        background->AddImage(ImTextureRef(portrait->view.Get()),layout.minimum,layout.maximum,
            layout.uvMinimum,layout.uvMaximum,IM_COL32(255,255,255,int(255*fade_)));
    }
    if(phase_!=Phase::Talk||line_>=lines_.size())return;
    const float windowHeight=std::clamp(screen.y*.27f,72.f,290.f);
    const ImVec2 p0{x+screen.x*.07f,y+screen.y-windowHeight-24},p1{x+screen.x*.93f,y+screen.y-24};
    background->AddRectFilledMultiColor(p0,p1,IM_COL32(3,14,26,int(238*fade_)),
        IM_COL32(7,29,43,int(232*fade_)),IM_COL32(2,8,17,int(242*fade_)),
        IM_COL32(2,8,17,int(242*fade_)));
    background->AddRect(p0,p1,IM_COL32(92,196,230,int(125*fade_)),5.f,0,1.f);
    background->AddRectFilled({p0.x+1,p0.y+1},{p0.x+4,p1.y-1},
        IM_COL32(84,211,248,int(220*fade_)),3.f);
    const float fontSize=textFontSize(screen.y);
    const ImGuiIO& io=ImGui::GetIO();
    ImFont* dialogueFont=io.FontDefault?io.FontDefault:(io.Fonts->Fonts.empty()?ImGui::GetFont():io.Fonts->Fonts[0]);
    float textY=p0.y+std::max(10.f,windowHeight*.09f);
    if(!lines_[line_].speaker.empty()) {
        background->AddText(dialogueFont,fontSize,{p0.x+18,textY},IM_COL32(153,214,255,int(255*fade_)),lines_[line_].speaker.c_str());
        textY+=fontSize+7;background->AddLine({p0.x+18,textY},{p1.x-18,textY},
            IM_COL32(82,174,207,int(125*fade_)));textY+=8;
    }
    const std::string shown=Prefix(lines_[line_].text,size_t(letters_));
    background->AddText(dialogueFont,fontSize*lines_[line_].textScale,{p0.x+18,textY},IM_COL32(255,255,255,int(255*fade_)),shown.c_str(),nullptr,textWrapWidth(screen.x));
    if(lines_[line_].autoDelay<0&&size_t(letters_)>=Count(lines_[line_].text)) {
        background->AddText(dialogueFont,fontSize*.72f,{p0.x+18,p1.y-fontSize-8},IM_COL32(150,160,175,int(255*fade_)),"クリック / F で次へ  ▽");
    }
}
void DialoguePlayer::Draw() const {const ImVec2 s=ImGui::GetIO().DisplaySize;DrawOverlay(ImGui::GetBackgroundDrawList(),0,0,s.x,s.y);}
void DialoguePlayer::DrawPreview(float x,float y,float width,float height) const {
    auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect({x,y},{x+width,y+height},true);
    DrawOverlay(draw,x,y,width,height);draw->PopClipRect();
}
void DialoguePlayer::Reset(){phase_=Phase::Dormant;line_=0;letters_=fade_=autoClock_=0;showClueImage_=false;expression_="normal";}
size_t DialoguePlayer::Count(const std::string& s){size_t n=0;for(unsigned char c:s)if((c&0xc0)!=0x80)++n;return n;}
std::string DialoguePlayer::Prefix(const std::string& s,size_t n){size_t i=0,c=0;for(;i<s.size();++i)if((static_cast<unsigned char>(s[i])&0xc0)!=0x80&&c++>=n)break;return s.substr(0,i);}
}
