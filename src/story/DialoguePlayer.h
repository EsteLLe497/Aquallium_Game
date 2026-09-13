#pragma once
#include "StoryTexture.h"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

struct ImDrawList;

namespace story {
// 会話データは speaker|expression|text。speaker が空なら名前欄を出さない。
// expression は normal/ase/yan/hiki/nihi/close/smile、none は直前の表情を維持する。
// 主人公（speaker が空）の行は同じ会話内の直前表情を維持する。別会話へは持ち越さない。
// @fast/@auto/@small を後置すると、倍速・自動送り・小声表示になる。
class DialoguePlayer {
public:
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
    bool BlocksPlayer() const {return phase_!=Phase::Dormant&&phase_!=Phase::Complete;}
    bool Active() const {return phase_!=Phase::Dormant&&phase_!=Phase::Complete;}
private:
    enum class Phase {Dormant,FadeIn,Talk,FadeOut,Complete};
    struct Line {
        std::string speaker,expression,text;
        float speed=22.f,autoDelay=-1.f,textScale=1.f;
    };
    static size_t Count(const std::string& value);
    static std::string Prefix(const std::string& value,size_t count);
    const StoryTexture* Portrait() const;
    void BeginLine(size_t index);
    void DrawOverlay(ImDrawList* draw,float x,float y,float width,float height) const;
    std::vector<Line> lines_;
    std::array<StoryTexture,7> portraits_;
    StoryTexture clueImage_;
    Phase phase_=Phase::Dormant;
    size_t line_=0;
    float letters_=0,fade_=0,autoClock_=0;
    bool showClueImage_=false;
    std::string expression_="normal";
};
}
