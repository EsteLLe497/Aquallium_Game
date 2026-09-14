// =========================================================
// ファイルの情報[PortraitPresentation.h]
//
// 制作者:Masatora Tanaka        日付：2026/09/13
// =========================================================
#pragma once
#include "StoryTexture.h"
#include "../../third_party/imgui/imgui.h"
#include <algorithm>

namespace story::portraitPresentation {

// =========================================================
// 立ち絵表示領域
// =========================================================
struct Layout {
    ImVec2 minimum{};
    ImVec2 maximum{};
    ImVec2 uvMinimum{0.f,0.f};
    ImVec2 uvMaximum{1.f,.50f};
};

// =========================================================
// 上半身表示用レイアウトの計算
// =========================================================
inline Layout upperBodyLayout(const StoryTexture& texture,const ImVec2& bottomRight,
    float preferredHeight,float maximumWidth)
{
    // 新素材の頭からスカート付近までを使い、会話中の表情を読みやすくする。
    constexpr float visibleHeight=.50f;
    const float sourceHeight=std::max(1.f,float(texture.height)*visibleHeight);
    const float aspect=float(texture.width)/sourceHeight;
    float height=preferredHeight;
    float width=height*aspect;
    if(width>maximumWidth){width=maximumWidth;height=width/aspect;}
    return {{bottomRight.x-width,bottomRight.y-height},bottomRight,{0.f,0.f},{1.f,visibleHeight}};
}

}
