#pragma once

#include "DialoguePlayer.h"
#include "StoryTexture.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include <wrl/client.h>

struct ImFont;

namespace story {

class StoryFlowEditor {
public:
    void Initialize(ID3D11Device* device,const std::filesystem::path& storyFolder,
                    const std::filesystem::path& textureFolder);
    void Toggle() noexcept {visible_=!visible_;}
    [[nodiscard]] bool Visible() const noexcept {return visible_;}
    [[nodiscard]] bool PreviewPlaying() const noexcept {return previewPlaying_;}
    void Update(float deltaTime);
    void CaptureScene(ID3D11DeviceContext* context,ID3D11RenderTargetView* source,
                      unsigned width,unsigned height);
    void Draw();

private:
    struct Node {
        std::string id,kind,title,script,condition,setFlag,next;
        float x=0,y=0;
    };
    struct DialogueLine {
        std::string layer,speaker,expression,event,text;
        bool fast=false,automatic=false,smallText=false,plain=false;
    };

    void LoadFlow();
    bool SaveFlow();
    void LoadDialogue();
    bool SaveDialogue();
    void SelectNode(int index);
    void SelectLine(int index);
    void SyncNodeBuffers();
    void SyncLineBuffers();
    void DrawToolbar();
    void DrawNodeList();
    void DrawCanvas(float width,float height);
    void DrawNodeProperties();
    void DrawDialogueFlow();
    void DrawLineProperties();
    void DrawSceneEditor();
    void DrawFlowChart();
    void DrawScenePreview();
    void AutoArrangeVertical(bool markDirty=true);
    void Validate();
    const StoryTexture* SelectedPortrait() const;
    int PreviewLineIndex() const;
    void RefreshDialoguePreview();

    std::filesystem::path storyFolder_,flowPath_;
    std::vector<Node> nodes_;
    std::vector<DialogueLine> lines_;
    std::vector<std::string> dialogueComments_;
    std::array<StoryTexture,7> portraits_;
    DialoguePlayer previewDialogue_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> scenePreviewTexture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scenePreviewView_;
    ImFont* editorFont_=nullptr;
    unsigned scenePreviewWidth_=0,scenePreviewHeight_=0;
    int selectedNode_=-1,selectedLine_=-1;
    int activeTab_=0,requestedTab_=-1,playbackLine_=0,draggingNode_=-1;
    float playbackTimer_=0,canvasZoom_=1.f;
    bool visible_=false,flowDirty_=false,dialogueDirty_=false,previewPlaying_=false,focusSelected_=false;
    std::string status_;
    std::array<char,96> idBuffer_{},kindBuffer_{},titleBuffer_{},scriptBuffer_{};
    std::array<char,256> conditionBuffer_{},flagBuffer_{},nextBuffer_{};
    std::array<char,96> speakerBuffer_{},expressionBuffer_{};
    std::array<char,96> layerBuffer_{},eventBuffer_{};
    std::array<char,2048> textBuffer_{};
};

}
