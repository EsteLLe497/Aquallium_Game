/*==================================================================================================

   [input.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/05/19
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#pragma once

#include <array>
#include <cstdint>
#include <windows.h>

namespace framework
{
class InputSystem
{
public:
    explicit InputSystem(HWND window = nullptr);

    void SetWindow(HWND window);
    void Update();

    [[nodiscard]] bool IsDown(int virtualKey) const;
    [[nodiscard]] bool WasPressed(int virtualKey) const;
    [[nodiscard]] bool WasReleased(int virtualKey) const;

    // 原本の呼び出し名を保った互換API。
    [[nodiscard]] bool GetKeyPress(BYTE keyCode) const { return IsDown(keyCode); }
    [[nodiscard]] bool GetKeyTrigger(BYTE keyCode) const { return WasPressed(keyCode); }

private:
    [[nodiscard]] static bool IsValidKey(int virtualKey);

    HWND window_ = nullptr;
    std::array<std::uint8_t, 256> current_{};
    std::array<std::uint8_t, 256> previous_{};
};

// DM31_Game互換名
using Input = InputSystem;
}

// DM31_Gameのグローバル型名との互換
using Input = framework::Input;
