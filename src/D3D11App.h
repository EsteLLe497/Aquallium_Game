/*==================================================================================================

   [D3D11App.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   Win32 Window、D3D11 Device、SwapChainを所有するプラットフォーム層
===================================================================================================*/
#pragma once

#include "framework/input.h"
#include "framework/manager.h"
#include "framework/renderer.h"
#include "imgui/LightingEditor.h"

#include <d3d11.h>
#include <dxgi.h>
#include <filesystem>
#include <wrl/client.h>
#include <windows.h>

class D3D11App
{
public:
    explicit D3D11App(HINSTANCE instance);
    ~D3D11App();
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void CreateMainWindow();
    void CreateDeviceResources();
    void CreateBackBuffer();
    void Resize(UINT width, UINT height);
    void Update(float deltaTime);
    void Render(float deltaTime);
    void UpdateWindowTitle(float deltaTime);
    std::filesystem::path FindShaderPath() const;

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    UINT width_ = 1280;
    UINT height_ = 720;
    bool minimized_ = false;
    bool running_ = true;
    bool vsyncEnabled_ = false;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferView_;

    framework::Input input_;
    framework::Manager sceneManager_;
    framework::Renderer renderer_;
    imgui::LightingEditor lightingEditor_;

    double totalTime_ = 0.0;
    std::uint64_t frameIndex_ = 0;
    float titleTimer_ = 0.0f;
    float titleFrameTime_ = 0.0f;
    UINT titleFrameCount_ = 0;
};
