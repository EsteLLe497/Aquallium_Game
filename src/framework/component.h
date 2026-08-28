/*==================================================================================================

   [compornent.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/05/19
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#pragma once

#include "FrameContext.h"
#include "RenderContext.h"

namespace framework
{
class GameObject;

class Component
{
public:
    Component() = delete;
    explicit Component(GameObject& owner)
        : owner_(owner)
    {
    }
    virtual ~Component() = default;

    [[nodiscard]] GameObject& Owner() const
    {
        return owner_;
    }

    [[nodiscard]] bool IsEnabled() const
    {
        return enabled_;
    }

    void SetEnabled(bool enabled)
    {
        enabled_ = enabled;
    }

    // The original Init/Uninit/Update/Draw lifecycle is preserved as the
    // public extension surface. Context overloads keep the aquarium runtime
    // frame-rate independent without global timing state.
    virtual void Init() {}
    virtual void Uninit() {}
    virtual void OnAttach() { Init(); }
    virtual void OnDetach() { Uninit(); }
    virtual void Update(const FrameContext&) {}
    virtual void Render(const RenderContext&) {}

private:
    GameObject& owner_;
    bool enabled_ = true;
};
}

// DM31_Gameのグローバル型名との互換
using Component = framework::Component;
