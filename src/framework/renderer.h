/*==================================================================================================

   [renderer.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/05/12
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#pragma once

#include "RenderContext.h"

#include <d3d11.h>

namespace framework
{
class Renderer
{
public:
    void Init(ID3D11Device* device, ID3D11DeviceContext* deviceContext);
    void Uninit();

    [[nodiscard]] RenderContext Begin(
        ID3D11RenderTargetView* renderTargetView,
        UINT width,
        UINT height,
        float deltaTime) const;
    void End() const;

    [[nodiscard]] ID3D11Device* GetDevice() const { return m_Device; }
    [[nodiscard]] ID3D11DeviceContext* GetDeviceContext() const { return m_DeviceContext; }

private:
    // D3D11App owns these resources. Renderer only exposes the frame boundary
    // expected by scenes and never releases non-owned COM objects.
    ID3D11Device* m_Device = nullptr;
    ID3D11DeviceContext* m_DeviceContext = nullptr;
};
}

// DM31_Gameのグローバル型名との互換
using Renderer = framework::Renderer;
