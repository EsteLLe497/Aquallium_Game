#include "StoryFlowEditor.h"
#include "DialogueTextCodec.h"
#include "PortraitPresentation.h"

#include "../../third_party/imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace story {
namespace {
std::vector<std::string> Split(const std::string& value,char separator='|') {
    std::vector<std::string> out;std::stringstream stream(value);std::string item;
    while(std::getline(stream,item,separator))out.push_back(item);
    if(!value.empty()&&value.back()==separator)out.emplace_back();
    return out;
}
template<size_t N> void Copy(std::array<char,N>& target,const std::string& value) {
    std::snprintf(target.data(),target.size(),"%s",value.c_str());
}

struct DialogueTextEditContext {
    ImFont* gameFont=nullptr;
    float gameFontSize=0.f;
    float gameTextWidth=0.f;
    bool* insertLineBreak=nullptr;
};

// インゲームの会話ウィンドウと同じ横幅で、保存対象の改行を挿入する。
void WrapDialogueTextForGame(ImGuiInputTextCallbackData* data,const DialogueTextEditContext& context) {
    if(!data||!context.gameFont||context.gameFontSize<=0.f||context.gameTextWidth<=0.f)return;
    int lineStart=0;
    while(lineStart<data->BufTextLen){
        int lineEnd=lineStart;
        while(lineEnd<data->BufTextLen&&data->Buf[lineEnd]!='\n')++lineEnd;
        const char* begin=data->Buf+lineStart;
        const char* end=data->Buf+lineEnd;
        const char* wrap=context.gameFont->CalcWordWrapPosition(
            context.gameFontSize,begin,end,context.gameTextWidth);
        if(wrap<end){
            if(data->BufTextLen+1>=data->BufSize)return;
            const int wrapPosition=int(wrap-data->Buf);
            data->InsertChars(wrapPosition,"\n");
            lineStart=wrapPosition+1;
        }else lineStart=lineEnd+1;
    }
}

// 自動改行と「改行」ボタンを入力欄のカーソル状態へ反映する。
int EditDialogueText(ImGuiInputTextCallbackData* data) {
    auto* context=static_cast<DialogueTextEditContext*>(data->UserData);
    if(context->insertLineBreak&&*context->insertLineBreak){
        if(data->BufTextLen+1<data->BufSize)data->InsertChars(data->CursorPos,"\n");
        *context->insertLineBreak=false;
    }
    if(data->EventFlag==ImGuiInputTextFlags_CallbackEdit)
        WrapDialogueTextForGame(data,*context);
    return 0;
}

const char* Kinds="Dialogue\0Event\0Flag\0Condition\0Transition\0";
}

void StoryFlowEditor::Initialize(ID3D11Device* device,const std::filesystem::path& storyFolder,
                                 const std::filesystem::path& textureFolder) {
    device_=device;
    for(const auto& fontPath:{std::filesystem::path("C:/Windows/Fonts/YuGothR.ttc"),
                             std::filesystem::path("C:/Windows/Fonts/meiryo.ttc")}){
        if(std::filesystem::exists(fontPath)){
            editorFont_=ImGui::GetIO().Fonts->AddFontFromFileTTF(
                fontPath.string().c_str(),18.f,nullptr,ImGui::GetIO().Fonts->GetGlyphRangesJapanese());
            if(editorFont_)break;
        }
    }
    storyFolder_=storyFolder;flowPath_=storyFolder_/"story_flow.cfg";
    portraits_=PortraitLibrary::load(device,textureFolder/"girl");
    previewDialogue_.Initialize(device,textureFolder);
    LoadFlow();
}

void StoryFlowEditor::Update(float deltaTime) {
    if(!visible_||!previewPlaying_)return;
    previewDialogue_.UpdatePreview(std::max(0.f,deltaTime));
    playbackLine_=int(previewDialogue_.CurrentLine());
    if(!previewDialogue_.Active()){previewPlaying_=false;status_="Preview finished";RefreshDialoguePreview();}
}

void StoryFlowEditor::CaptureScene(ID3D11DeviceContext* context,ID3D11RenderTargetView* source,
                                   unsigned width,unsigned height) {
    if(!visible_||!device_||!context||!source||width==0||height==0)return;
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sourceTexture;
    source->GetResource(resource.GetAddressOf());
    if(!resource||FAILED(resource.As(&sourceTexture)))return;
    D3D11_TEXTURE2D_DESC sourceDesc{};sourceTexture->GetDesc(&sourceDesc);
    if(!scenePreviewTexture_||scenePreviewWidth_!=sourceDesc.Width||scenePreviewHeight_!=sourceDesc.Height){
        scenePreviewTexture_.Reset();scenePreviewView_.Reset();
        D3D11_TEXTURE2D_DESC desc=sourceDesc;
        desc.MipLevels=1;desc.ArraySize=1;desc.SampleDesc={1,0};
        desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags=0;desc.MiscFlags=0;
        if(FAILED(device_->CreateTexture2D(&desc,nullptr,scenePreviewTexture_.GetAddressOf())))return;
        if(FAILED(device_->CreateShaderResourceView(scenePreviewTexture_.Get(),nullptr,scenePreviewView_.GetAddressOf()))){
            scenePreviewTexture_.Reset();return;
        }
        scenePreviewWidth_=desc.Width;scenePreviewHeight_=desc.Height;
    }
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> previousTarget;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> previousDepth;
    context->OMGetRenderTargets(1,previousTarget.GetAddressOf(),previousDepth.GetAddressOf());
    context->OMSetRenderTargets(0,nullptr,nullptr);
    context->CopyResource(scenePreviewTexture_.Get(),sourceTexture.Get());
    ID3D11RenderTargetView* target=previousTarget.Get();
    context->OMSetRenderTargets(1,&target,previousDepth.Get());
}

void StoryFlowEditor::LoadFlow() {
    nodes_.clear();std::ifstream file(flowPath_);std::string value;
    while(std::getline(file,value)){
        if(!value.empty()&&value.back()=='\r')value.pop_back();
        if(value.empty()||value[0]=='#')continue;
        auto f=Split(value);if(f.size()<9)continue;
        Node n{f[0],f[1],f[2],f[3],f[4],f[5],f[6]};
        try{n.x=std::stof(f[7]);n.y=std::stof(f[8]);}catch(...){n.x=n.y=0;}
        nodes_.push_back(std::move(n));
    }
    selectedNode_=nodes_.empty()?-1:0;
    for(int i=0;i<int(nodes_.size());++i)if(nodes_[i].id=="terrace.hint_after"){selectedNode_=i;break;}
    const float maximumY=nodes_.empty()?0.f:std::max_element(nodes_.begin(),nodes_.end(),
        [](const Node& a,const Node& b){return a.y<b.y;})->y;
    if(nodes_.size()>8&&maximumY<900.f)AutoArrangeVertical(false);
    flowDirty_=false;status_="Flow loaded";
    SyncNodeBuffers();LoadDialogue();Validate();
}

bool StoryFlowEditor::SaveFlow() {
    std::ofstream out(flowPath_,std::ios::trunc);
    out<<"# id|kind|title|dialogue|condition|setFlag|next|x|y\n";
    for(const auto& n:nodes_)
        out<<n.id<<'|'<<n.kind<<'|'<<n.title<<'|'<<n.script<<'|'<<n.condition<<'|'
           <<n.setFlag<<'|'<<n.next<<'|'<<n.x<<'|'<<n.y<<'\n';
    flowDirty_=!out;status_=out?"Flow saved":"Flow save failed";return bool(out);
}

void StoryFlowEditor::LoadDialogue() {
    lines_.clear();dialogueComments_.clear();selectedLine_=-1;
    if(selectedNode_<0||selectedNode_>=int(nodes_.size())||nodes_[selectedNode_].script.empty())return;
    std::ifstream file(storyFolder_/nodes_[selectedNode_].script);std::string value;
    while(std::getline(file,value)){
        if(!value.empty()&&value.back()=='\r')value.pop_back();
        if(value.empty())continue;
        if(value[0]=='#'){dialogueComments_.push_back(value);continue;}
        auto f=Split(value);DialogueLine line;
        if(f.size()==1){line.expression="hide";line.text=f[0];line.plain=true;}
        else if(f.size()>=5&&(f[0]=="still"||f[0]=="portrait")){
            line.layer=f[0];line.speaker=f[1];line.expression=f[2];line.event=f[3];line.text=f[4];
            for(size_t i=5;i<f.size();++i)line.text+='|'+f[i];
        }else if(f.size()>=3){
            line.speaker=f[0];line.expression=f[1];line.text=f[2];
            for(size_t i=3;i<f.size();++i)line.text+='|'+f[i];
        }else continue;
        line.text=dialogueTextCodec::quoteHeroineSpeech(
            line.speaker,dialogueTextCodec::decode(line.text));
        const auto tag=line.expression.find('@');
        if(tag!=std::string::npos){
            const auto tags=line.expression.substr(tag+1);line.expression.resize(tag);
            line.fast=tags.find("fast")!=std::string::npos;
            line.automatic=tags.find("auto")!=std::string::npos;
            line.smallText=tags.find("small")!=std::string::npos;
        }
        lines_.push_back(std::move(line));
    }
    selectedLine_=lines_.empty()?-1:0;dialogueDirty_=false;SyncLineBuffers();RefreshDialoguePreview();
}

bool StoryFlowEditor::SaveDialogue() {
    if(selectedNode_<0||nodes_[selectedNode_].script.empty())return false;
    std::ofstream out(storyFolder_/nodes_[selectedNode_].script,std::ios::trunc);
    const bool eventFormat=std::any_of(lines_.begin(),lines_.end(),[](const DialogueLine& l){return !l.layer.empty();});
    const bool plainFormat=!lines_.empty()&&std::all_of(lines_.begin(),lines_.end(),[](const DialogueLine& l){return l.plain;});
    for(const auto& comment:dialogueComments_)out<<comment<<'\n';
    if(dialogueComments_.empty()&&!plainFormat)
        out<<(eventFormat?"# layer|speaker|expression|event|text\n":"# speaker|expression|text\n");
    for(const auto& line:lines_){
        std::string expression=line.expression;
        if(line.fast)expression+="@fast";
        if(line.automatic)expression+="@auto";
        if(line.smallText)expression+="@small";
        const std::string text=dialogueTextCodec::encode(line.text);
        if(line.plain)out<<text<<'\n';
        else if(!line.layer.empty())out<<line.layer<<'|'<<line.speaker<<'|'<<expression<<'|'<<line.event<<'|'<<text<<'\n';
        else out<<line.speaker<<'|'<<expression<<'|'<<text<<'\n';
    }
    dialogueDirty_=!out;status_=out?"Dialogue saved":"Dialogue save failed";
    if(out)RefreshDialoguePreview();return bool(out);
}

void StoryFlowEditor::SelectNode(int index) {
    if(index<0||index>=int(nodes_.size())||index==selectedNode_)return;
    if(dialogueDirty_)SaveDialogue();
    selectedNode_=index;previewPlaying_=false;playbackLine_=0;playbackTimer_=0;focusSelected_=true;
    SyncNodeBuffers();LoadDialogue();
}
void StoryFlowEditor::SelectLine(int index) {
    if(index<0||index>=int(lines_.size()))return;
    selectedLine_=index;SyncLineBuffers();if(!previewPlaying_)RefreshDialoguePreview();
}

void StoryFlowEditor::RefreshDialoguePreview() {
    previewDialogue_.Reset();
    if(selectedNode_<0||selectedNode_>=int(nodes_.size())||selectedLine_<0||nodes_[selectedNode_].script.empty())return;
    previewDialogue_.ShowStatic(storyFolder_/nodes_[selectedNode_].script,size_t(selectedLine_));
}
void StoryFlowEditor::SyncNodeBuffers() {
    if(selectedNode_<0||selectedNode_>=int(nodes_.size()))return;
    const auto& n=nodes_[selectedNode_];Copy(idBuffer_,n.id);Copy(kindBuffer_,n.kind);
    Copy(titleBuffer_,n.title);Copy(scriptBuffer_,n.script);Copy(conditionBuffer_,n.condition);
    Copy(flagBuffer_,n.setFlag);Copy(nextBuffer_,n.next);
}
void StoryFlowEditor::SyncLineBuffers() {
    if(selectedLine_<0||selectedLine_>=int(lines_.size()))return;
    const auto& l=lines_[selectedLine_];Copy(layerBuffer_,l.layer);Copy(eventBuffer_,l.event);Copy(speakerBuffer_,l.speaker);
    Copy(expressionBuffer_,l.expression);Copy(textBuffer_,l.text);
}

void StoryFlowEditor::DrawToolbar() {
    ImGui::TextUnformatted("F3  STORY FLOW");ImGui::SameLine();
    if(ImGui::Button("Reload"))LoadFlow();ImGui::SameLine();
    if(ImGui::Button("Save all")){SaveFlow();SaveDialogue();}ImGui::SameLine();
    if(ImGui::Button("Validate"))Validate();ImGui::SameLine();
    if(ImGui::Button("+ Node")){
        const int i=int(nodes_.size());nodes_.push_back({"new_node_"+std::to_string(i),"Dialogue","New node","","","","",80.f+i*24.f,80.f+i*18.f});
        selectedNode_=i;flowDirty_=true;SyncNodeBuffers();LoadDialogue();
    }
    ImGui::SameLine();
    ImGui::TextColored(flowDirty_||dialogueDirty_?ImVec4(1,.72f,.3f,1):ImVec4(.5f,.85f,.72f,1),
        "%s%s",status_.c_str(),flowDirty_||dialogueDirty_?"  * unsaved":"");
}

void StoryFlowEditor::DrawNodeList() {
    ImGui::TextUnformatted("STORY NODES");ImGui::Separator();
    for(int i=0;i<int(nodes_.size());++i){
        const std::string label=nodes_[i].title+"##node_list_"+std::to_string(i);
        if(ImGui::Selectable(label.c_str(),selectedNode_==i))SelectNode(i);
        ImGui::SameLine(155);ImGui::TextDisabled("%s",nodes_[i].kind.c_str());
    }
}

void StoryFlowEditor::AutoArrangeVertical(bool markDirty) {
    if(nodes_.empty())return;
    std::vector<int> depth(nodes_.size(),-1),indegree(nodes_.size(),0);
    const auto indexOf=[&](const std::string& id){
        const auto it=std::find_if(nodes_.begin(),nodes_.end(),[&](const Node& n){return n.id==id;});
        return it==nodes_.end()?-1:int(std::distance(nodes_.begin(),it));
    };
    for(const auto& node:nodes_)for(const auto& next:Split(node.next,','))
        if(const int target=indexOf(next);target>=0)++indegree[target];
    for(int i=0;i<int(nodes_.size());++i)if(indegree[i]==0)depth[i]=0;
    if(std::none_of(depth.begin(),depth.end(),[](int d){return d==0;}))depth[0]=0;
    for(size_t pass=0;pass<nodes_.size();++pass){
        bool changed=false;
        for(int i=0;i<int(nodes_.size());++i){
            if(depth[i]<0)continue;
            for(const auto& next:Split(nodes_[i].next,','))if(const int target=indexOf(next);target>=0&&depth[target]<depth[i]+1){
                depth[target]=depth[i]+1;changed=true;
            }
        }
        if(!changed)break;
    }
    int maximumDepth=*std::max_element(depth.begin(),depth.end());
    for(auto& d:depth)if(d<0)d=++maximumDepth;
    for(int level=0;level<=*std::max_element(depth.begin(),depth.end());++level){
        std::vector<int> row;
        for(int i=0;i<int(depth.size());++i)if(depth[i]==level)row.push_back(i);
        const float rowWidth=float(row.size())*220.f+float(std::max(0,int(row.size())-1))*72.f;
        const float startX=std::max(80.f,540.f-rowWidth*.5f);
        for(int lane=0;lane<int(row.size());++lane){
            nodes_[row[lane]].x=startX+lane*292.f;
            nodes_[row[lane]].y=70.f+level*148.f;
        }
    }
    if(markDirty){flowDirty_=true;status_="Flow arranged vertically";focusSelected_=true;}
}

void StoryFlowEditor::DrawCanvas(float width,float height) {
    constexpr float nodeWidth=220.f,nodeHeight=78.f;
    float maximumX=width/canvasZoom_,maximumY=height/canvasZoom_;
    for(const auto& node:nodes_){maximumX=std::max(maximumX,node.x+nodeWidth+260.f);maximumY=std::max(maximumY,node.y+nodeHeight+260.f);}
    const float contentWidth=std::max(width-20.f,maximumX*canvasZoom_);
    const float contentHeight=std::max(height-20.f,maximumY*canvasZoom_);
    ImGui::SetNextWindowContentSize({contentWidth,contentHeight});
    ImGui::BeginChild("##flow_canvas",{width,height},true,ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 origin=ImGui::GetCursorScreenPos();auto* draw=ImGui::GetWindowDrawList();
    if(focusSelected_&&selectedNode_>=0&&selectedNode_<int(nodes_.size())){
        const auto& node=nodes_[selectedNode_];
        ImGui::SetScrollX(std::clamp((node.x+nodeWidth*.5f)*canvasZoom_-width*.5f,0.f,ImGui::GetScrollMaxX()));
        ImGui::SetScrollY(std::clamp((node.y+nodeHeight*.5f)*canvasZoom_-height*.42f,0.f,ImGui::GetScrollMaxY()));
        focusSelected_=false;
    }
    if(ImGui::IsWindowHovered()&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle)){
        const ImVec2 delta=ImGui::GetIO().MouseDelta;
        ImGui::SetScrollX(ImGui::GetScrollX()-delta.x);ImGui::SetScrollY(ImGui::GetScrollY()-delta.y);
    }
    const ImU32 grid=IM_COL32(70,105,125,32);
    const float gridStep=32.f*canvasZoom_;
    for(float x=0;x<contentWidth;x+=gridStep)draw->AddLine({origin.x+x,origin.y},{origin.x+x,origin.y+contentHeight},grid);
    for(float y=0;y<contentHeight;y+=gridStep)draw->AddLine({origin.x,origin.y+y},{origin.x+contentWidth,origin.y+y},grid);
    for(const auto& n:nodes_){
        for(const auto& next:Split(n.next,',')){
            auto it=std::find_if(nodes_.begin(),nodes_.end(),[&](const Node& target){return target.id==next;});
            if(it==nodes_.end())continue;
            const ImVec2 from{origin.x+(n.x+nodeWidth*.5f)*canvasZoom_,origin.y+(n.y+nodeHeight)*canvasZoom_};
            const ImVec2 to{origin.x+(it->x+nodeWidth*.5f)*canvasZoom_,origin.y+it->y*canvasZoom_};
            const float bend=std::max(42.f,(to.y-from.y)*.45f);
            draw->AddBezierCubic(from,{from.x,from.y+bend},{to.x,to.y-bend},to,IM_COL32(94,183,214,205),2.2f);
            draw->AddTriangleFilled({to.x,to.y},{to.x-5.f,to.y-9.f},{to.x+5.f,to.y-9.f},IM_COL32(94,183,214,220));
        }
    }
    for(int i=0;i<int(nodes_.size());++i){
        auto& n=nodes_[i];ImGui::SetCursorScreenPos({origin.x+n.x*canvasZoom_,origin.y+n.y*canvasZoom_});
        ImGui::PushStyleColor(ImGuiCol_Button,selectedNode_==i?ImVec4(.12f,.34f,.44f,.98f):ImVec4(.06f,.14f,.21f,.96f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.12f,.30f,.39f,1));
        const std::string label=n.kind+"\n"+n.title+"##canvas_"+std::to_string(i);
        if(ImGui::Button(label.c_str(),{nodeWidth*canvasZoom_,nodeHeight*canvasZoom_})){SelectNode(i);requestedTab_=0;}
        if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Right))draggingNode_=i;
        if(draggingNode_==i&&ImGui::IsMouseDown(ImGuiMouseButton_Right)){
            const auto delta=ImGui::GetIO().MouseDelta;n.x=std::max(0.f,n.x+delta.x/canvasZoom_);n.y=std::max(0.f,n.y+delta.y/canvasZoom_);flowDirty_=true;
        }
        ImGui::PopStyleColor(2);
    }
    if(!ImGui::IsMouseDown(ImGuiMouseButton_Right))draggingNode_=-1;
    ImGui::EndChild();
}

const StoryTexture* StoryFlowEditor::SelectedPortrait() const {
    if(selectedLine_<0||selectedLine_>=int(lines_.size()))return nullptr;
    return portraits_?portraits_->find(lines_[selectedLine_].expression):nullptr;
}

void StoryFlowEditor::DrawLineProperties() {
    if(selectedLine_<0||selectedLine_>=int(lines_.size())){
        ImGui::TextDisabled("Select a dialogue line to edit it.");return;
    }
    auto& line=lines_[selectedLine_];
    if(const StoryTexture* portrait=SelectedPortrait();portrait&&portrait->view){
        const float h=168.f;
        const ImVec2 portraitPosition=ImGui::GetCursorScreenPos();
        const auto layout=portraitPresentation::upperBodyLayout(
            *portrait,{0.f,h},h,220.f);
        const ImVec2 portraitSize{layout.maximum.x-layout.minimum.x,h};
        if(const StoryTexture* underlay=portraits_->underlay(line.expression);underlay&&underlay->view)
            ImGui::GetWindowDrawList()->AddImage(ImTextureRef(underlay->view.Get()),portraitPosition,
                {portraitPosition.x+portraitSize.x,portraitPosition.y+h},layout.uvMinimum,layout.uvMaximum);
        ImGui::Image(ImTextureRef(portrait->view.Get()),portraitSize,
            layout.uvMinimum,layout.uvMaximum);ImGui::SameLine();
        const bool portraitHovered=ImGui::IsItemHovered();
        ImGui::BeginGroup();ImGui::TextDisabled("Portrait preview");
        ImGui::Text("%s",line.expression.c_str());ImGui::TextDisabled("Line %d / %d",selectedLine_+1,int(lines_.size()));
        ImGui::EndGroup();
        if(portraitHovered){
            ImGui::BeginTooltip();
            const float previewHeight=420.f;
            const ImVec2 tooltipPosition=ImGui::GetCursorScreenPos();
            const auto tooltipLayout=portraitPresentation::upperBodyLayout(
                *portrait,{0.f,previewHeight},previewHeight,420.f);
            const ImVec2 tooltipSize{tooltipLayout.maximum.x-tooltipLayout.minimum.x,previewHeight};
            if(const StoryTexture* underlay=portraits_->underlay(line.expression);underlay&&underlay->view)
                ImGui::GetWindowDrawList()->AddImage(ImTextureRef(underlay->view.Get()),tooltipPosition,
                    {tooltipPosition.x+tooltipSize.x,tooltipPosition.y+previewHeight},
                    tooltipLayout.uvMinimum,tooltipLayout.uvMaximum);
            ImGui::Image(ImTextureRef(portrait->view.Get()),tooltipSize,
                tooltipLayout.uvMinimum,tooltipLayout.uvMaximum);
            ImGui::EndTooltip();
        }
    }else ImGui::TextDisabled("Portrait: hidden / keep previous");
    if(line.plain)ImGui::TextDisabled("Plain opening line: text only");
    if(!line.layer.empty()){
        if(ImGui::InputText("Layer",layerBuffer_.data(),layerBuffer_.size())){line.layer=layerBuffer_.data();dialogueDirty_=true;}
        if(ImGui::InputText("Event",eventBuffer_.data(),eventBuffer_.size())){line.event=eventBuffer_.data();dialogueDirty_=true;}
    }
    ImGui::BeginDisabled(line.plain);
    if(ImGui::InputText("Speaker",speakerBuffer_.data(),speakerBuffer_.size())){line.speaker=speakerBuffer_.data();dialogueDirty_=true;}
    if(ImGui::BeginCombo("Portrait / expression",line.expression.c_str())){
        if(portraits_)for(const auto& expression:portraits_->expressions()){
            const bool selected=line.expression==expression;
            if(ImGui::Selectable(expression.c_str(),selected)){
                line.expression=expression;Copy(expressionBuffer_,line.expression);dialogueDirty_=true;
            }
            if(selected)ImGui::SetItemDefaultFocus();
        }
        for(const char* expression:{"hide","none"}){
            const bool selected=line.expression==expression;
            if(ImGui::Selectable(expression,selected)){
                line.expression=expression;Copy(expressionBuffer_,line.expression);dialogueDirty_=true;
            }
            if(selected)ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if(insertTextLineBreak_)ImGui::SetKeyboardFocusHere();
    const ImGuiIO& io=ImGui::GetIO();
    ImFont* gameFont=io.FontDefault?io.FontDefault:(io.Fonts->Fonts.empty()?ImGui::GetFont():io.Fonts->Fonts[0]);
    const float gameTextScale=line.smallText?.72f:1.f;
    DialogueTextEditContext textContext{
        gameFont,DialoguePlayer::textFontSize(io.DisplaySize.y,gameTextScale),
        DialoguePlayer::textWrapWidth(io.DisplaySize.x),&insertTextLineBreak_};
    constexpr ImGuiInputTextFlags textFlags=ImGuiInputTextFlags_WordWrap|
        ImGuiInputTextFlags_CallbackEdit|ImGuiInputTextFlags_CallbackAlways;
    if(ImGui::InputTextMultiline("Text",textBuffer_.data(),textBuffer_.size(),{0,92},textFlags,EditDialogueText,&textContext)){
        line.text=textBuffer_.data();dialogueDirty_=true;
    }
    if(ImGui::Button("改行"))insertTextLineBreak_=true;
    ImGui::SameLine();ImGui::TextDisabled("インゲームの会話幅で自動改行");
    ImGui::BeginDisabled(line.plain);
    dialogueDirty_|=ImGui::Checkbox("Fast",&line.fast);ImGui::SameLine();
    dialogueDirty_|=ImGui::Checkbox("Auto",&line.automatic);ImGui::SameLine();
    dialogueDirty_|=ImGui::Checkbox("Small",&line.smallText);ImGui::EndDisabled();
}

void StoryFlowEditor::DrawDialogueFlow() {
    if(selectedNode_<0)return;
    ImGui::TextUnformatted("DIALOGUE FLOW");ImGui::Separator();
    if(ImGui::InputText("Dialogue file",scriptBuffer_.data(),scriptBuffer_.size())){
        nodes_[selectedNode_].script=scriptBuffer_.data();flowDirty_=true;
    }
    if(ImGui::Button("Reload"))LoadDialogue();ImGui::SameLine();
    if(ImGui::Button("Save dialogue"))SaveDialogue();
    const float listHeight=std::clamp(ImGui::GetContentRegionAvail().y*.42f,170.f,330.f);
    ImGui::BeginChild("##dialogue_lines",{0,listHeight},true);
    for(int i=0;i<int(lines_.size());++i){
        ImGui::PushID(i);
        const bool active=(previewPlaying_?playbackLine_:selectedLine_)==i;
        if(active)ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.08f,.24f,.30f,.88f));
        ImGui::BeginChild("line_card",{0,64},true);
        ImGui::TextColored(ImVec4(.46f,.82f,.92f,1),"%02d  %s  [%s]",i+1,lines_[i].speaker.c_str(),lines_[i].expression.c_str());
        ImGui::TextWrapped("%s",lines_[i].text.c_str());
        if(ImGui::IsWindowHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left))SelectLine(i);
        ImGui::EndChild();
        if(active)ImGui::PopStyleColor();
        ImGui::PopID();
        ImGui::Spacing();
    }
    ImGui::EndChild();
    if(ImGui::Button("+ Line")){
        DialogueLine added;added.speaker="少女";added.expression="normal";added.text="新しいセリフ";
        if(!lines_.empty()){added.plain=lines_[0].plain;added.layer=lines_[0].layer.empty()?"": "portrait";added.event=added.layer.empty()?"":"none";}
        lines_.push_back(std::move(added));SelectLine(int(lines_.size())-1);dialogueDirty_=true;
    }
    ImGui::SameLine();
    if(ImGui::Button("Delete")&&selectedLine_>=0){
        lines_.erase(lines_.begin()+selectedLine_);selectedLine_=std::min(selectedLine_,int(lines_.size())-1);SyncLineBuffers();dialogueDirty_=true;
    }
    ImGui::SameLine();
    if(ImGui::Button("Up")&&selectedLine_>0){std::swap(lines_[selectedLine_],lines_[selectedLine_-1]);--selectedLine_;dialogueDirty_=true;}
    ImGui::SameLine();
    if(ImGui::Button("Down")&&selectedLine_>=0&&selectedLine_+1<int(lines_.size())){std::swap(lines_[selectedLine_],lines_[selectedLine_+1]);++selectedLine_;dialogueDirty_=true;}
    ImGui::SeparatorText("Selected line");DrawLineProperties();
}

void StoryFlowEditor::DrawNodeProperties() {
    if(selectedNode_<0||selectedNode_>=int(nodes_.size()))return;
    auto& n=nodes_[selectedNode_];
    ImGui::TextUnformatted("SCENE PROPERTIES");ImGui::Separator();
    int kind=0;const char* kinds[]={"Dialogue","Event","Flag","Condition","Transition"};
    for(int i=0;i<5;++i)if(n.kind==kinds[i])kind=i;
    if(ImGui::InputText("ID",idBuffer_.data(),idBuffer_.size())){n.id=idBuffer_.data();flowDirty_=true;}
    if(ImGui::InputText("Title",titleBuffer_.data(),titleBuffer_.size())){n.title=titleBuffer_.data();flowDirty_=true;}
    if(ImGui::Combo("Kind",&kind,Kinds)){n.kind=kinds[kind];Copy(kindBuffer_,n.kind);flowDirty_=true;}
    if(ImGui::InputText("Condition",conditionBuffer_.data(),conditionBuffer_.size())){n.condition=conditionBuffer_.data();flowDirty_=true;}
    if(ImGui::InputText("Set flag",flagBuffer_.data(),flagBuffer_.size())){n.setFlag=flagBuffer_.data();flowDirty_=true;}
    if(ImGui::InputText("Next node",nextBuffer_.data(),nextBuffer_.size())){n.next=nextBuffer_.data();flowDirty_=true;}
    if(ImGui::Button("Delete node")){
        nodes_.erase(nodes_.begin()+selectedNode_);selectedNode_=std::min(selectedNode_,int(nodes_.size())-1);
        flowDirty_=true;SyncNodeBuffers();LoadDialogue();
    }
}

int StoryFlowEditor::PreviewLineIndex() const {
    if(lines_.empty())return -1;
    return std::clamp(previewPlaying_?int(previewDialogue_.CurrentLine()):selectedLine_,0,int(lines_.size())-1);
}

void StoryFlowEditor::DrawScenePreview() {
    const char* title=selectedNode_>=0?nodes_[selectedNode_].title.c_str():"No scene selected";
    ImGui::Text("SCENE PREVIEW  /  %s",title);ImGui::Separator();
    ImVec2 available=ImGui::GetContentRegionAvail();
    const float aspect=scenePreviewHeight_?float(scenePreviewWidth_)/float(scenePreviewHeight_):16.f/9.f;
    float previewWidth=available.x,previewHeight=previewWidth/aspect;
    const float maxHeight=std::max(180.f,available.y-86.f);
    if(previewHeight>maxHeight){previewHeight=maxHeight;previewWidth=previewHeight*aspect;}
    const float indent=std::max(0.f,(available.x-previewWidth)*.5f);ImGui::SetCursorPosX(ImGui::GetCursorPosX()+indent);
    const ImVec2 topLeft=ImGui::GetCursorScreenPos();
    if(scenePreviewView_)ImGui::Image(ImTextureRef(scenePreviewView_.Get()),{previewWidth,previewHeight});
    else{
        ImGui::Dummy({previewWidth,previewHeight});
        auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(topLeft,{topLeft.x+previewWidth,topLeft.y+previewHeight},IM_COL32(10,18,25,255));
        draw->AddText({topLeft.x+24,topLeft.y+24},IM_COL32(150,170,180,255),"Scene preview is preparing...");
    }
    const int lineIndex=PreviewLineIndex();
    previewDialogue_.DrawPreview(topLeft.x,topLeft.y,previewWidth,previewHeight);
    ImGui::Spacing();
    const float controlsWidth=270.f;ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(available.x-controlsWidth)*.5f));
    if(ImGui::Button("|<",{42,30})&&selectedLine_>0){SelectLine(selectedLine_-1);playbackLine_=selectedLine_;}
    ImGui::SameLine();
    if(ImGui::Button("Play",{76,30})){
        if(lines_.empty())status_="No dialogue to preview";
        else{
            playbackLine_=std::max(0,selectedLine_);playbackTimer_=0;
            previewPlaying_=previewDialogue_.Start(storyFolder_/nodes_[selectedNode_].script,false,size_t(playbackLine_));
            status_=previewPlaying_?"Preview playing":"Preview unavailable";
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Stop",{76,30})){previewPlaying_=false;playbackTimer_=0;RefreshDialoguePreview();status_="Preview stopped";}
    ImGui::SameLine();
    if(ImGui::Button(">|",{42,30})&&selectedLine_+1<int(lines_.size())){SelectLine(selectedLine_+1);playbackLine_=selectedLine_;}
    ImGui::TextDisabled("%s  |  %s",previewPlaying_?"PLAYING":"STOPPED",lineIndex>=0?(std::to_string(lineIndex+1)+" / "+std::to_string(lines_.size())).c_str():"No dialogue");
}

void StoryFlowEditor::DrawSceneEditor() {
    const ImVec2 available=ImGui::GetContentRegionAvail();
    const float leftWidth=255.f,rightWidth=385.f;
    ImGui::BeginChild("##scene_left",{leftWidth,available.y},true);DrawNodeList();ImGui::Spacing();DrawNodeProperties();ImGui::EndChild();
    ImGui::SameLine();
    const float centerWidth=std::max(340.f,ImGui::GetContentRegionAvail().x-rightWidth-8.f);
    ImGui::BeginChild("##scene_preview",{centerWidth,available.y},true);DrawScenePreview();ImGui::EndChild();
    ImGui::SameLine();ImGui::BeginChild("##dialogue_flow",{0,available.y},true);DrawDialogueFlow();ImGui::EndChild();
}

void StoryFlowEditor::DrawFlowChart() {
    if(ImGui::Button("Auto arrange vertically"))AutoArrangeVertical();ImGui::SameLine();
    if(ImGui::Button("Center selected"))focusSelected_=true;ImGui::SameLine();
    ImGui::SetNextItemWidth(150.f);ImGui::SliderFloat("Zoom",&canvasZoom_,.72f,1.35f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();ImGui::TextDisabled("Wheel: vertical  |  Shift+wheel: horizontal  |  Middle drag: pan  |  Right drag node: move");
    const ImVec2 available=ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##flow_nodes",{225,available.y},true);DrawNodeList();ImGui::Separator();
    ImGui::TextWrapped("Click a node to open its Scene Editor. Select a name here and use Center selected to locate it.");ImGui::EndChild();ImGui::SameLine();
    DrawCanvas(ImGui::GetContentRegionAvail().x,available.y);
}

void StoryFlowEditor::Validate() {
    std::unordered_set<std::string> ids;int errors=0,warnings=0;
    for(const auto& n:nodes_){
        if(n.id.empty()||!ids.insert(n.id).second)++errors;
        for(const auto& next:Split(n.next,','))
            if(!next.empty()&&std::none_of(nodes_.begin(),nodes_.end(),[&](const Node& other){return other.id==next;}))++errors;
        if(!n.script.empty()&&!std::filesystem::exists(storyFolder_/n.script))++warnings;
    }
    status_="Validation: "+std::to_string(errors)+" errors, "+std::to_string(warnings)+" warnings";
}

void StoryFlowEditor::Draw() {
    if(!visible_)return;
    if(editorFont_)ImGui::PushFont(editorFont_,18.f);
    const ImVec2 display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({10,10},ImGuiCond_Always);
    ImGui::SetNextWindowSize({display.x-20,display.y-20},ImGuiCond_Always);
    if(!ImGui::Begin("Story Flow Editor",&visible_,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove)){
        ImGui::End();if(editorFont_)ImGui::PopFont();return;
    }
    DrawToolbar();ImGui::Separator();
    const int requestedAtFrameStart=requestedTab_;
    if(ImGui::BeginTabBar("##story_editor_tabs")){
        const ImGuiTabItemFlags sceneFlags=requestedTab_==0?ImGuiTabItemFlags_SetSelected:0;
        if(ImGui::BeginTabItem("Scene Editor",nullptr,sceneFlags)){
            activeTab_=0;DrawSceneEditor();ImGui::EndTabItem();
        }
        const ImGuiTabItemFlags flowFlags=requestedTab_==1?ImGuiTabItemFlags_SetSelected:0;
        if(ImGui::BeginTabItem("Story Flow Chart",nullptr,flowFlags)){
            activeTab_=1;DrawFlowChart();ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        if(requestedTab_==requestedAtFrameStart)requestedTab_=-1;
    }
    ImGui::End();
    if(editorFont_)ImGui::PopFont();
}
}
