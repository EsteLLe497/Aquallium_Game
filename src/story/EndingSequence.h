// =========================================================
// ファイルの情報[EndingSequence.h]
//
// 制作者:Masatora Tanaka        日付：2026/09/13
// =========================================================
#pragma once
#include <filesystem>

namespace story {

// =========================================================
// 分岐後のスタッフロールとエンドカード
// =========================================================
class EndingSequence {
public:
    ~EndingSequence();
    void initialize(const std::filesystem::path& videoPath);
    void reset();
    void startNormalEnd();
    void startTrueEnd();
    void update(float deltaTime,bool advance);
    void draw() const;
    [[nodiscard]] bool active() const noexcept;
    bool consumeReturnTitle();

private:
    enum class Phase {Inactive,NormalFade,NormalCard,StaffRoll,TrueCard};
    bool startVideo();
    bool videoFinished();
    void stopVideo();
    void showCard(const char* category,const char* pendingName) const;

    std::filesystem::path videoPath_;
    Phase phase_=Phase::Inactive;
    float clock_=0.f;
    float videoPollClock_=0.f;
    bool videoPlaying_=false;
    bool returnTitle_=false;
};

}
