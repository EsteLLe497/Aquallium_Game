#pragma once
#include "PortraitLibrary.h"
#include "StoryTexture.h"
#include <filesystem>
#include <string>
#include <vector>

struct ImDrawList;

namespace story {
// 会話データは speaker|expression|text。speaker が空なら地の文として名前欄を出さない。
// expression は girlフォルダーのPNGファイル名。none は直前の表情を維持する。
// 渚生の行はexpressionをnoneにして、同じ会話内の凪沙の表情を維持する。
// @fast/@auto/@small を後置すると、倍速・自動送り・小声表示になる。
class DialoguePlayer {
public:
    enum class Cue {None,ActuallyBgm};
    void Initialize(ID3D11Device* device,const std::filesystem::path& textureRoot);
    bool Start(const std::filesystem::path& script,bool showClueImage=false,size_t initialLine=0);
    void Update(float dt,bool advance);
    void UpdatePreview(float dt);
    void Draw() const;
    void DrawPreview(float x,float y,float width,float height) const;
    bool ShowStatic(const std::filesystem::path& script,size_t line);
    void Reset();
    const std::string& Expression() const noexcept {return expression_;}
    size_t CurrentLine() const noexcept {return line_;}
    Cue currentCue() const noexcept {
        return line_<lines_.size()?lines_[line_].cue:Cue::None;
    }
    bool BlocksPlayer() const {return phase_!=Phase::Dormant&&phase_!=Phase::Complete;}
    bool Active() const {return phase_!=Phase::Dormant&&phase_!=Phase::Complete;}
    // F3エディタも同じ値を使い、インゲーム表示と改行位置を一致させる。
    static float textFontSize(float viewportHeight,float textScale=1.f);
    static float textWrapWidth(float viewportWidth);
private:
    enum class Phase {Dormant,FadeIn,Talk,FadeOut,Complete};
    struct Line {
        std::string speaker,expression,text;
        float speed=22.f,autoDelay=-1.f,textScale=1.f;
        Cue cue=Cue::None;
    };
    static size_t Count(const std::string& value);
    static std::string Prefix(const std::string& value,size_t count);
    const StoryTexture* Portrait() const;
    void BeginLine(size_t index);
    void DrawOverlay(ImDrawList* draw,float x,float y,float width,float height) const;
    std::vector<Line> lines_;
    std::shared_ptr<PortraitLibrary> portraits_;
    StoryTexture clueImage_;
    Phase phase_=Phase::Dormant;
    size_t line_=0;
    float letters_=0,fade_=0,autoClock_=0;
    bool showClueImage_=false;
    std::string expression_="normal";
};
}
