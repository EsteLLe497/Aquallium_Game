#pragma once

#include <d3d11.h>

namespace framework
{
struct RenderContext
{
    ID3D11DeviceContext* deviceContext = nullptr;
    ID3D11RenderTargetView* backBuffer = nullptr;
    UINT width = 0;
    UINT height = 0;
    float deltaTime = 0.0f;
};
}
