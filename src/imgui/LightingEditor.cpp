#include "LightingEditor.h"

#include "../../third_party/imgui/imgui.h"
#include "../../third_party/imgui/imgui_impl_dx11.h"
#include "../../third_party/imgui/imgui_impl_win32.h"

#include <algorithm>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace imgui
{
void LightingEditor::Initialize(
    HWND window, ID3D11Device* device, ID3D11DeviceContext* context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 6.0f;
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(device, context);
    initialized_ = true;
}

void LightingEditor::Shutdown()
{
    if (!initialized_)
    {
        return;
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

bool LightingEditor::HandleMessage(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_KEYDOWN && wParam == VK_F2)
    {
        visible_ = !visible_;
        return true;
    }
    return initialized_ && visible_ &&
        ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
}

void LightingEditor::BeginFrame()
{
    if (!initialized_ || !visible_)
    {
        return;
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void LightingEditor::Draw(
    lighting::LocalLightingRig& rig,
    lighting::HeroTankLightingRig& heroTankRig)
{
    if (!initialized_ || !visible_)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(390.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Aquarium Local Lighting", &visible_))
    {
        ImGui::TextUnformatted("F2: show / hide editor");
        ImGui::Checkbox("Enable local lighting", &rig.enabled);
        ImGui::SeparatorText("Ambient visibility");
        ImGui::ColorEdit3("Ambient color", &rig.ambientColor.x,
            ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Ambient strength", &rig.ambientStrength,
            0.0f, 0.20f, "%.3f");

        ImGui::SeparatorText("Tank bounce");
        ImGui::Checkbox("Enable tank bounce", &rig.tankBounceEnabled);
        ImGui::DragFloat3("Tank center", &rig.tankBounceCenter.x, 0.05f);
        ImGui::DragFloat3("Tank normal", &rig.tankBounceNormal.x,
            0.02f, -1.0f, 1.0f);
        ImGui::ColorEdit3("Tank bounce color", &rig.tankBounceColor.x,
            ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Tank bounce intensity",
            &rig.tankBounceIntensity, 0.0f, 2.0f);
        ImGui::SliderFloat("Tank bounce range", &rig.tankBounceRange,
            1.0f, 40.0f, "%.1f m");
        ImGui::SliderFloat("Tank half width", &rig.tankBounceHalfWidth,
            0.5f, 20.0f, "%.2f m");
        ImGui::SliderFloat("Tank half height", &rig.tankBounceHalfHeight,
            0.5f, 10.0f, "%.2f m");

        ImGui::SeparatorText("Atmosphere");
        ImGui::Checkbox("Enable atmosphere", &rig.atmosphereEnabled);
        ImGui::ColorEdit3("Atmosphere color", &rig.atmosphereColor.x,
            ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Atmosphere density", &rig.atmosphereDensity,
            0.0f, 0.10f, "%.3f");
        ImGui::SliderFloat("Atmosphere start", &rig.atmosphereStart,
            0.0f, 30.0f, "%.1f m");
        ImGui::SliderFloat("Atmosphere maximum", &rig.atmosphereMaximum,
            0.0f, 0.75f, "%.2f");

        int lightCount = static_cast<int>(rig.lightCount);
        if (ImGui::SliderInt("Light count", &lightCount, 0,
            static_cast<int>(lighting::kMaximumLocalLights)))
        {
            rig.lightCount = static_cast<std::uint32_t>(lightCount);
        }
        if (rig.lightCount > 0)
        {
            selectedLight_ = std::clamp(
                selectedLight_, 0, static_cast<int>(rig.lightCount) - 1);
            ImGui::SliderInt("Selected", &selectedLight_, 0,
                static_cast<int>(rig.lightCount) - 1);
            auto& light = rig.lights[static_cast<std::size_t>(selectedLight_)];
            ImGui::SeparatorText("Light");
            ImGui::Checkbox("Enabled", &light.enabled);
            int type = static_cast<int>(light.type);
            if (ImGui::Combo("Type", &type, "Point\0Spot\0"))
            {
                light.type = static_cast<lighting::LocalLightType>(type);
            }
            ImGui::DragFloat3("Position", &light.position.x, 0.05f);
            ImGui::DragFloat3("Direction", &light.direction.x, 0.02f, -1.0f, 1.0f);
            ImGui::ColorEdit3("Color", &light.color.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Range", &light.range, 0.25f, 30.0f, "%.2f m");
            ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 20.0f);
            if (light.type == lighting::LocalLightType::Spot)
            {
                ImGui::SliderFloat("Inner cone", &light.innerConeDegrees, 1.0f, 85.0f);
                ImGui::SliderFloat("Outer cone", &light.outerConeDegrees,
                    light.innerConeDegrees + 1.0f, 89.0f);
            }
        }
        ImGui::SeparatorText("Accent lights");
        ImGui::Text("GPU cap: %u local lights", lighting::kMaximumLocalLights);
        ImGui::TextUnformatted("Water lighting remains a separate shader path.");

        ImGui::SeparatorText("Hero tank puzzle lighting");
        ImGui::TextUnformatted(
            "Game switches can toggle the same alternateEnabled flag.");
        if (ImGui::Button(
            heroTankRig.alternateEnabled
                ? "Restore default blue"
                : "Activate alternate colour"))
        {
            heroTankRig.alternateEnabled =
                !heroTankRig.alternateEnabled;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(
            heroTankRig.alternateEnabled ? "ALT" : "DEFAULT");
        ImGui::ColorEdit3(
            "Default tank colour",
            &heroTankRig.defaultColor.x,
            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        ImGui::ColorEdit3(
            "Switch tank colour",
            &heroTankRig.alternateColor.x,
            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        ImGui::ColorEdit3(
            "Overhead key colour",
            &heroTankRig.overheadKeyColor.x,
            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        ImGui::SliderFloat(
            "Tank light intensity",
            &heroTankRig.intensity,
            0.0f,
            2.5f,
            "%.2f");
        ImGui::SliderFloat(
            "Overhead key intensity",
            &heroTankRig.overheadKeyIntensity,
            0.0f,
            2.5f,
            "%.2f");
        ImGui::SliderFloat(
            "Side lights intensity",
            &heroTankRig.sideLightIntensity,
            0.0f,
            2.0f,
            "%.2f");
    }
    ImGui::End();
}

void LightingEditor::Render(ID3D11DeviceContext*)
{
    if (!initialized_ || !visible_)
    {
        return;
    }
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool LightingEditor::WantsInput() const noexcept
{
    if (!initialized_ || !visible_)
    {
        return false;
    }
    const ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard || io.WantCaptureMouse;
}
}
