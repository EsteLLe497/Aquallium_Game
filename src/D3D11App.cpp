/*==================================================================================================

   [D3D11App.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   メッセージループ、フレーム時間、Resize、Presentを管理するDirectX 11ホスト
===================================================================================================*/
#include "D3D11App.h"
#include "scenes/AquariumScene.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>

namespace
{
constexpr wchar_t WindowClassName[] = L"AquariumLightingPrototypeWindow";

void ThrowIfFailed(HRESULT hr, const char* operation)
{
    if (FAILED(hr))
    {
        throw std::runtime_error(std::string(operation) + " failed.");
    }
}

}

D3D11App::D3D11App(HINSTANCE instance)
    : instance_(instance)
{
    CreateMainWindow();
    CreateDeviceResources();
    renderer_.Init(device_.Get(), context_.Get());
    lightingEditor_.Initialize(window_, device_.Get(), context_.Get());
    input_.SetWindow(window_);
    sceneManager_.SetInitialScene(
        std::make_unique<AquariumScene>(device_.Get(), FindShaderPath()));
}

D3D11App::~D3D11App()
{
    lightingEditor_.Shutdown();
}

int D3D11App::Run()
{
    using clock = std::chrono::steady_clock;
    auto previousTime = clock::now();

    while (running_)
    {
        MSG message{};
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                running_ = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        const auto now = clock::now();
        const float deltaTime = std::min(
            std::chrono::duration<float>(now - previousTime).count(),
            0.1f);
        previousTime = now;

        if (!running_)
        {
            break;
        }

        if (minimized_)
        {
            Sleep(16);
            continue;
        }

        Update(deltaTime);
        Render(deltaTime);
    }

    return 0;
}

LRESULT CALLBACK D3D11App::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    D3D11App* app = nullptr;

    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCT*>(lParam);
        app = static_cast<D3D11App*>(create->lpCreateParams);
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->window_ = window;
    }
    else
    {
        app = reinterpret_cast<D3D11App*>(GetWindowLongPtr(window, GWLP_USERDATA));
    }

    return app ? app->HandleMessage(message, wParam, lParam)
               : DefWindowProc(window, message, wParam, lParam);
}

LRESULT D3D11App::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (lightingEditor_.HandleMessage(window_, message, wParam, lParam))
    {
        return 1;
    }
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        running_ = false;
        return 0;

    case WM_SIZE:
    {
        const UINT newWidth = LOWORD(lParam);
        const UINT newHeight = HIWORD(lParam);
        minimized_ = wParam == SIZE_MINIMIZED || newWidth == 0 || newHeight == 0;
        if (!minimized_ && swapChain_)
        {
            Resize(newWidth, newHeight);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(window_);
        }
        return 0;
    }

    return DefWindowProc(window_, message, wParam, lParam);
}

void D3D11App::CreateMainWindow()
{
    WNDCLASSEX windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = WindowClassName;

    if (!RegisterClassEx(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        throw std::runtime_error("RegisterClassEx failed.");
    }

    RECT windowRect{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
    ThrowIfFailed(
        HRESULT_FROM_WIN32(AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE) ? ERROR_SUCCESS : GetLastError()),
        "AdjustWindowRect");

    window_ = CreateWindowEx(
        0,
        WindowClassName,
        L"Aquarium Lighting Prototype",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!window_)
    {
        throw std::runtime_error("CreateWindowEx failed.");
    }

    ShowWindow(window_, SW_SHOW);
    UpdateWindow(window_);
}

void D3D11App::CreateDeviceResources()
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferDesc.Width = width_;
    swapChainDesc.BufferDesc.Height = height_;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.OutputWindow = window_;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL requestedLevels[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL createdLevel{};
    UINT flags = 0;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        requestedLevels,
        ARRAYSIZE(requestedLevels),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        swapChain_.GetAddressOf(),
        device_.GetAddressOf(),
        &createdLevel,
        context_.GetAddressOf());

#if defined(_DEBUG)
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            requestedLevels,
            ARRAYSIZE(requestedLevels),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            swapChain_.GetAddressOf(),
            device_.GetAddressOf(),
            &createdLevel,
            context_.GetAddressOf());
    }
#endif

    ThrowIfFailed(hr, "D3D11CreateDeviceAndSwapChain");
    if (createdLevel != D3D_FEATURE_LEVEL_11_0)
    {
        throw std::runtime_error("Direct3D feature level 11_0 is required.");
    }

    CreateBackBuffer();
}

void D3D11App::CreateBackBuffer()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(
        swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())),
        "IDXGISwapChain::GetBuffer");
    ThrowIfFailed(
        device_->CreateRenderTargetView(backBuffer.Get(), nullptr, backBufferView_.GetAddressOf()),
        "CreateRenderTargetView");
}

void D3D11App::Resize(UINT width, UINT height)
{
    if (!swapChain_ || width == 0 || height == 0)
    {
        return;
    }

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    backBufferView_.Reset();

    ThrowIfFailed(
        swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0),
        "ResizeBuffers");

    width_ = width;
    height_ = height;
    CreateBackBuffer();
}

void D3D11App::Update(float deltaTime)
{
    // F2 opens the lighting editor. Gameplay owns the cursor only while that
    // editor is hidden, so mouse-look and ImGui never fight over input.
    input_.SetRelativeMouseMode(!lightingEditor_.IsVisible());
    input_.Update();
    lightingEditor_.BeginFrame();
    if (auto* aquariumScene = dynamic_cast<AquariumScene*>(
        sceneManager_.GetCurrentScene()))
    {
        lightingEditor_.Draw(
            aquariumScene->GetLocalLighting(),
            aquariumScene->GetHeroTankLighting());
    }
    if (input_.WasPressed('V'))
    {
        vsyncEnabled_ = !vsyncEnabled_;
    }

    totalTime_ += deltaTime;
    sceneManager_.Update(
        framework::FrameContext{deltaTime, totalTime_, frameIndex_++},
        input_);
    UpdateWindowTitle(deltaTime);
}

void D3D11App::Render(float deltaTime)
{
    sceneManager_.Render(
        renderer_.Begin(backBufferView_.Get(), width_, height_, deltaTime));
    renderer_.End();
    lightingEditor_.Render(context_.Get());

    ThrowIfFailed(
        swapChain_->Present(vsyncEnabled_ ? 1 : 0, 0),
        "Present");
}

void D3D11App::UpdateWindowTitle(float deltaTime)
{
    titleTimer_ += deltaTime;
    titleFrameTime_ += deltaTime;
    ++titleFrameCount_;

    if (titleTimer_ < 0.5f)
    {
        return;
    }

    const float fps = titleFrameTime_ > 0.0f
        ? static_cast<float>(titleFrameCount_) / titleFrameTime_
        : 0.0f;

    const framework::SceneDiagnostics diagnostics =
        sceneManager_.GetDiagnostics();
    const std::wstring title = std::format(
        L"Aquarium Lighting Prototype | {:.0f} FPS | {} | {} | Scale {:.0f}% / {:.1f} ms | Caustics {:.2f} | Volume {:.2f} | g {:.2f} | Exposure {:.2f}{}",
        fps,
        diagnostics.viewLabel,
        vsyncEnabled_ ? L"VSYNC" : L"UNLOCKED",
        diagnostics.renderScale * 100.0f,
        diagnostics.smoothedFrameMilliseconds,
        diagnostics.causticsStrength,
        diagnostics.volumeStrength,
        diagnostics.anisotropy,
        diagnostics.exposure,
        diagnostics.paused ? L" | PAUSED" : L"");
    SetWindowText(window_, title.c_str());

    titleTimer_ = 0.0f;
    titleFrameTime_ = 0.0f;
    titleFrameCount_ = 0;
}

std::filesystem::path D3D11App::FindShaderPath() const
{
    wchar_t executablePath[MAX_PATH]{};
    const DWORD length = GetModuleFileName(nullptr, executablePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        throw std::runtime_error("GetModuleFileName failed.");
    }

    const std::filesystem::path shaderPath =
        std::filesystem::path(executablePath).parent_path() /
        L"shaders" /
        L"AquariumPrototype.hlsl";

    if (!std::filesystem::exists(shaderPath))
    {
        throw std::runtime_error("Shader file was not found: " + shaderPath.string());
    }
    return shaderPath;
}
