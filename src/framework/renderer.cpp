/*==================================================================================================

   [renderer.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/05/12
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#include "renderer.h"

#include <stdexcept>

namespace framework
{
void Renderer::Init(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
    if (device == nullptr || deviceContext == nullptr)
    {
        throw std::invalid_argument("Renderer requires a valid D3D11 device and context.");
    }

    m_Device = device;
    m_DeviceContext = deviceContext;

    // デバイス、スワップチェーン作成
    // レンダーターゲットビュー作成
    // デプスステンシルバッファ作成
    // デプスステンシルビュー作成
    // ビューポート設定
    // ラスタライザステート設定
    // ブレンドステート設定
    // デプスステンシルステート設定
    //depthStencilDesc.DepthEnable = FALSE;
    // サンプラーステート設定
    // 定数バッファ生成
    // ライト初期化
    // マテリアル初期化
    //
    // The platform host and AquariumRenderer currently implement the operations
    // above. Keeping this boundary lets them move behind Renderer incrementally
    // without rewriting the established aquarium render path.
}

void Renderer::Uninit()
{
    m_DeviceContext = nullptr;
    m_Device = nullptr;
}

RenderContext Renderer::Begin(
    ID3D11RenderTargetView* renderTargetView,
    UINT width,
    UINT height,
    float deltaTime) const
{
    if (m_DeviceContext == nullptr)
    {
        throw std::logic_error("Renderer::Init must be called before Begin.");
    }

    return RenderContext{
        m_DeviceContext,
        renderTargetView,
        width,
        height,
        deltaTime};
}

void Renderer::End() const
{
    // Present remains in D3D11App because VSync is a platform policy.
}
}
