#pragma once

#include "../lighting/LocalLight.h"
#include "../lighting/HeroTankLighting.h"

#include <d3d11.h>
#include <windows.h>

namespace imgui
{
class LightingEditor
{
public:
    void Initialize(HWND window, ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();
    bool HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void BeginFrame();
    void Draw(
        lighting::LocalLightingRig& rig,
        lighting::HeroTankLightingRig& heroTankRig);
    void Render(ID3D11DeviceContext* context);
    [[nodiscard]] bool IsVisible() const noexcept { return visible_; }
    [[nodiscard]] bool WantsInput() const noexcept;

private:
    bool initialized_ = false;
    // Editing is opt-in. When hidden, no ImGui frame or draw data is built.
    bool visible_ = false;
    int selectedLight_ = 0;
};
}
