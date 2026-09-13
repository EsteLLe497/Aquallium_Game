#pragma once
#include "../../third_party/imgui/imgui.h"
#include "../framework/input.h"
#include <string>
#include <utility>

namespace story {
class PasswordLock {
public:
    enum class Feedback {None,Select,Open,Miss};
    void Configure(std::string expected,std::string title,std::string prompt){
        expected_=std::move(expected);title_=std::move(title);prompt_=std::move(prompt);
        Reset();
    }
    void Open(){if(!unlocked_){active_=true;digits_.clear();wrong_=false;}}
    void Reset(){active_=unlocked_=wrong_=false;feedback_=Feedback::None;digits_.clear();}
    void ForceUnlocked(){active_=wrong_=false;unlocked_=true;digits_.clear();}
    void Update(const framework::InputSystem& input);
    void Draw();
    bool Active() const{return active_;}
    bool Unlocked() const{return unlocked_;}
    Feedback ConsumeFeedback(){const auto value=feedback_;feedback_=Feedback::None;return value;}
private:
    void Append(char digit);
    void Submit();
    bool active_=false,unlocked_=false,wrong_=false;
    Feedback feedback_=Feedback::None;
    std::string digits_;
    std::string expected_="124";
    std::string title_="管理室 セキュリティ";
    std::string prompt_="3桁のパスワードを入力してください";
};
}
