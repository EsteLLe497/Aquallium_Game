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

InputSystem::~InputSystem()
{
    ReleaseRelativeMouse();
}

void InputSystem::SetWindow(HWND window)
{
    ReleaseRelativeMouse();
    window_ = window;
}

void InputSystem::SetRelativeMouseMode(bool enabled)
{
    relativeMouseRequested_ = enabled;
    if (!enabled)
    {
        ReleaseRelativeMouse();
    }
}

void InputSystem::Update()
{
    previous_ = current_;
    mouseDeltaX_ = 0.0f;
    mouseDeltaY_ = 0.0f;
    if (window_ == nullptr || GetForegroundWindow() != window_)
    {
        current_.fill(0);
        ReleaseRelativeMouse();
        return;
    }

    for (int virtualKey = 0; virtualKey < static_cast<int>(current_.size()); ++virtualKey)
    {
        current_[virtualKey] =
            (GetAsyncKeyState(virtualKey) & 0x8000) != 0 ? 1 : 0;
    }

    if (relativeMouseRequested_)
    {
        AcquireRelativeMouse();
        RECT clientRect{};
        GetClientRect(window_, &clientRect);
        POINT center{
            (clientRect.left + clientRect.right) / 2,
            (clientRect.top + clientRect.bottom) / 2};
        ClientToScreen(window_, &center);

        POINT cursor{};
        GetCursorPos(&cursor);
        mouseDeltaX_ = static_cast<float>(cursor.x - center.x);
        mouseDeltaY_ = static_cast<float>(cursor.y - center.y);
        SetCursorPos(center.x, center.y);
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

void InputSystem::AcquireRelativeMouse()
{
    if (relativeMouseAcquired_)
    {
        return;
    }

    RECT clip{};
    GetClientRect(window_, &clip);
    POINT topLeft{clip.left, clip.top};
    POINT bottomRight{clip.right, clip.bottom};
    ClientToScreen(window_, &topLeft);
    ClientToScreen(window_, &bottomRight);
    clip = {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
    ClipCursor(&clip);
    SetCapture(window_);
    ShowCursor(FALSE);

    const int centerX = (topLeft.x + bottomRight.x) / 2;
    const int centerY = (topLeft.y + bottomRight.y) / 2;
    SetCursorPos(centerX, centerY);
    relativeMouseAcquired_ = true;
}

void InputSystem::ReleaseRelativeMouse()
{
    if (!relativeMouseAcquired_)
    {
        return;
    }
    ClipCursor(nullptr);
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
    ShowCursor(TRUE);
    relativeMouseAcquired_ = false;
}
}
