#pragma once
#include "PlayerManager.h"
#include "../../third_party/imgui/imgui.h"
#include <algorithm>
#include <cmath>

namespace player {
// 新しい対象は Target の配列へ追加。bounds は選択範囲、anchor は表示の基準点。
// 画面中央の視線で判定し、遮蔽物より奥や3.2m以上離れた対象には表示しない。
// ワールド位置を画面へ投影してから文字を描くため、UI は常にプレイヤーを向く。
struct InteractionTarget {
    int id;
    const wchar_t* collider;
    DirectX::XMFLOAT3 minimum,maximum,anchor;
    const char* label;
    bool actionable=true; // 状態だけを伝える対象には入力キーを表示しない。
};
inline float RayBox(const SelectionRay& ray,DirectX::XMFLOAT3 low,DirectX::XMFLOAT3 high) {
    float enterDistance=0,exitDistance=3.2f;
    const float origin[]={ray.origin.x,ray.origin.y,ray.origin.z};
    const float dir[]={ray.direction.x,ray.direction.y,ray.direction.z};
    const float lo[]={low.x,low.y,low.z},hi[]={high.x,high.y,high.z};
    for(int a=0;a<3;++a) {
        if(std::abs(dir[a])<.00001f) {if(origin[a]<lo[a] || origin[a]>hi[a])return -1;}
        else {
            float t0=(lo[a]-origin[a])/dir[a],t1=(hi[a]-origin[a])/dir[a];
            if(t0>t1)std::swap(t0,t1);
            enterDistance=std::max(enterDistance,t0);exitDistance=std::min(exitDistance,t1);
            if(enterDistance>exitDistance)return -1;
        }
    }
    return enterDistance;
}
inline int FindInteraction(const SelectionRay& ray,const physics::CollisionWorld& world,
                           const InteractionTarget* targets,int count) {
    float nearest=3.2f;int selected=-1;
    for(int i=0;i<count;++i) {
        const auto& target=targets[i];
        const float t=RayBox(ray,target.minimum,target.maximum);
        if(t<0 || t>=nearest)continue;
        bool occluded=false;
        for(const auto& box:world.Boxes()) {
            if(box.name==target.collider || box.tag==physics::ColliderTag::Trigger || box.tag==physics::ColliderTag::Water)continue;
            const float hit=RayBox(ray,box.minimum,box.maximum);
            if(hit>=0 && hit+.12f<t) {occluded=true;break;}
        }
        if(!occluded) {nearest=t;selected=i;}
    }
    return selected;
}
inline void DrawInteraction(const InteractionTarget& target,const SelectionRay& ray,float yaw) {
    using namespace DirectX;
    const ImVec2 size=ImGui::GetIO().DisplaySize;
    const XMVECTOR forward=XMLoadFloat3(&ray.direction);
    const XMVECTOR right=XMVectorSet(std::cos(yaw),0,-std::sin(yaw),0);
    const XMVECTOR up=XMVector3Cross(forward,right);
    const XMVECTOR delta=XMVectorSubtract(XMLoadFloat3(&target.anchor),XMLoadFloat3(&ray.origin));
    const float z=XMVectorGetX(XMVector3Dot(delta,forward));
    if(z<=.03f)return;
    // Same vertical projection scale (1.45) as AquariumRenderer. HUD uses full resolution.
    const float x=size.x*.5f+XMVectorGetX(XMVector3Dot(delta,right))*size.y*.725f/z;
    const float y=size.y*.5f-XMVectorGetX(XMVector3Dot(delta,up))*size.y*.725f/z;
    const float width=ImGui::CalcTextSize(target.label).x+(target.actionable?190:24);
    // Keep the prompt above the bottom tutorial even at arm's-length distance.
    ImVec2 p{std::clamp(x+35.f,12.f,std::max(12.f,size.x-width-12)),std::clamp(y-22.f,12.f,std::max(12.f,size.y-130))};
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddCircle({size.x*.5f,size.y*.5f},3,IM_COL32(220,235,245,200),12,1.5f);
    draw->AddRectFilled(p,{p.x+width,p.y+44},IM_COL32(8,20,32,205),6);
    draw->AddRect(p,{p.x+width,p.y+44},IM_COL32(170,210,235,130),6);
    draw->AddText({p.x+12,p.y+8},IM_COL32(230,240,250,240),target.label);
    if(target.actionable)
        draw->AddText(ImGui::GetFont(),17,{p.x+width-165,p.y+12},IM_COL32(160,200,225,210),"F / 左クリック");
}
}
