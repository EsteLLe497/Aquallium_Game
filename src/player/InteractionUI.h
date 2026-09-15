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
    draw->AddRectFilledMultiColor(p,{p.x+width,p.y+44},IM_COL32(4,17,28,225),
        IM_COL32(9,32,46,218),IM_COL32(4,16,27,225),IM_COL32(3,13,23,225));
    draw->AddRect(p,{p.x+width,p.y+44},IM_COL32(105,205,235,145),5,0,1.f);
    draw->AddRectFilled({p.x,p.y+5},{p.x+3,p.y+39},IM_COL32(92,215,250,230),2);
    draw->AddText({p.x+14,p.y+8},IM_COL32(232,244,250,245),target.label);
    if(target.actionable){
        const ImVec2 keyMinimum{p.x+width-164,p.y+8};
        const ImVec2 keyMaximum{p.x+width-14,p.y+36};
        draw->AddRectFilled(keyMinimum,keyMaximum,IM_COL32(23,72,91,205),3);
        draw->AddRect(keyMinimum,keyMaximum,IM_COL32(104,203,232,135),3);
        draw->AddText(ImGui::GetFont(),15,{keyMinimum.x+12,keyMinimum.y+5},
            IM_COL32(190,227,240,235),"F / 左クリック");
    }
}
}
