// =========================================================
// ファイルの情報[EndingSequence.h]
//
// 制作者:Masatora Tanaka        日付：2026/09/13
// =========================================================
#pragma once
namespace story {

// =========================================================
// 分岐後の余韻演出とエンドカード
// =========================================================
class EndingSequence {
public:
    void reset();
    void startNormalEnd();
    void startTrueEnd();
    void update(float deltaTime,bool advance);
    void draw() const;
    [[nodiscard]] bool active() const noexcept;
    bool consumeReturnTitle();

private:
    enum class Phase {Inactive,NormalFade,NormalCard,TrueInterlude,TrueCard};
    void showCard(const char* category,const char* endingName) const;
    void showTrueInterlude() const;

    Phase phase_=Phase::Inactive;
    float clock_=0.f;
    bool returnTitle_=false;
};

}
