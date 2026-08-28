/*==================================================================================================

   [input.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/05/19
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#include "input.h"

#include <algorithm>

namespace framework
{
InputSystem::InputSystem(HWND window)
    : window_(window)
{
}

void InputSystem::SetWindow(HWND window)
{
    window_ = window;
}

void InputSystem::Update()
{
    previous_ = current_;

    if (window_ == nullptr || GetForegroundWindow() != window_)
    {
        current_.fill(0);
        return;
    }

    for (int virtualKey = 0; virtualKey < static_cast<int>(current_.size()); ++virtualKey)
    {
        current_[virtualKey] =
            (GetAsyncKeyState(virtualKey) & 0x8000) != 0 ? 1 : 0;
    }
}

bool InputSystem::IsDown(int virtualKey) const
{
    return IsValidKey(virtualKey) && current_[virtualKey] != 0;
}

bool InputSystem::WasPressed(int virtualKey) const
{
    return IsValidKey(virtualKey) &&
        current_[virtualKey] != 0 &&
        previous_[virtualKey] == 0;
}

bool InputSystem::WasReleased(int virtualKey) const
{
    return IsValidKey(virtualKey) &&
        current_[virtualKey] == 0 &&
        previous_[virtualKey] != 0;
}

bool InputSystem::IsValidKey(int virtualKey)
{
    return virtualKey >= 0 && virtualKey < 256;
}
}
