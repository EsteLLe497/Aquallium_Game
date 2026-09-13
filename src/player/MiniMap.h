#pragma once
#include "../physics/CollisionWorld.h"
#include "../../third_party/imgui/imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <queue>
#include <sstream>
#include <vector>

namespace player {
// Collision-derived navigation grid. Built once, never renders the 3D world again.
// Each goal is a floor-position (NOT eye height). See asset/story/minimap.goals.
class MiniMap {
public:
    struct Goal { bool enabled=false; float x=0,y=0,z=0,radius=2; };
    static int Floor(float x,float y,float z) {
        // The display landing and the arch mouth are still at 1F height.
        // Position disambiguates these overlaps; other locations use floor height.
        // Leave headroom for the seated wake-up camera; it is not a lower floor.
        if(y< -3.5f || (x< -6 && x> -15 && z>=25 && y<0)) return 0;
        if(y>0 || (x>15 && z>=10.4f && z<23) || (z>18 && y> -2.1f)) return 2;
        return 1;
    }
    void LoadGoals(const std::filesystem::path& folder) {
        goals_={}; std::ifstream file(folder/"minimap.goals");std::string line;
        while(std::getline(file,line)) {
            if(line.empty() || line[0]=='#')continue;
            std::istringstream in(line);int mission;Goal g;
            if(in>>mission>>g.x>>g.y>>g.z>>g.radius && mission>=0 && mission<int(goals_.size())) {
                g.enabled=true;goals_[mission]=g;
            }
        }
        mission_=-1;
    }
    void Build(const physics::CollisionWorld& world) {
        nodes_.clear();cells_.assign(NX*NZ,{});
        // Rasterize walkable rectangles and graded paths, preserving stacked floors.
        for(int iz=0;iz<NZ;++iz)for(int ix=0;ix<NX;++ix) {
            float x=X0+ix*Step,z=Z0+iz*Step;
            std::vector<float> heights;
            for(const auto& f:world.WalkableRects())
                if(x>=f.minimumX && x<=f.maximumX && z>=f.minimumZ && z<=f.maximumZ) heights.push_back(f.floorY);
            for(const auto& path:world.Paths()) {
                float best=1e9f,height=0;
                for(size_t i=1;i<path.centerLine.size();++i) {
                    const auto a=path.centerLine[i-1],b=path.centerLine[i];
                    float dx=b.x-a.x,dz=b.z-a.z;
                    float t=std::clamp(((x-a.x)*dx+(z-a.z)*dz)/std::max(.0001f,dx*dx+dz*dz),0.f,1.f);
                    float d=(x-a.x-dx*t)*(x-a.x-dx*t)+(z-a.z-dz*t)*(z-a.z-dz*t);
                    if(d<best){best=d;height=a.y+(b.y-a.y)*t;}
                }
                if(best<(path.halfWidth-.34f)*(path.halfWidth-.34f))heights.push_back(height);
            }
            std::sort(heights.begin(),heights.end());
            float last=-999;
            for(float y:heights) {
                if(std::abs(y-last)<.25f)continue;
                last=y;
                if(Blocked(world,x,y,z))continue;
                cells_[iz*NX+ix].push_back(int(nodes_.size()));
                nodes_.push_back({x,y,z,Floor(x,y,z),{}});
            }
        }
        const int offsets[][2]={{1,0},{-1,0},{0,1},{0,-1}};
        for(int iz=0;iz<NZ;++iz)for(int ix=0;ix<NX;++ix)
            for(int a:cells_[iz*NX+ix])for(auto& offset:offsets) {
                int xx=ix+offset[0],zz=iz+offset[1];if(xx<0||xx>=NX||zz<0||zz>=NZ)continue;
                for(int b:cells_[zz*NX+xx]) {
                    const auto& p=nodes_[a];const auto& q=nodes_[b];
                    if(std::abs(p.y-q.y)<.25f && !Blocked(world,(p.x+q.x)*.5f,(p.y+q.y)*.5f,(p.z+q.z)*.5f))nodes_[a].edges.push_back(b);
                }
            }
        mission_=-1;
    }
    size_t NodeCount() const {return nodes_.size();}
    bool HasRoute(float x,float y,float z,int mission) {SetMission(mission);int n=Closest(x,y,z);return n>=0 && !next_.empty() && next_[n]>=0;}
    void Draw(float dt,float x,float eyeY,float z,float yaw,int mission,bool visible=true,bool showOptionalExhibit=false) {
        if(nodes_.empty())return;
        mapAlpha_+=(float(visible)-mapAlpha_)*(1-std::exp(-std::min(dt,.1f)*5.f));
        if(mapAlpha_<.01f)return;
        auto C=[&](int r,int g,int b,int a){return IM_COL32(r,g,b,int(std::clamp(a*mapAlpha_,0.f,255.f)));};
        mission=std::clamp(mission,0,int(goals_.size())-1);
        SetMission(mission);
        float y=eyeY-1.89f;
        const int desiredFloor=Floor(x,y,z);
        if(desiredFloor!=candidate_){candidate_=desiredFloor;candidateTime_=0;}
        else candidateTime_+=dt;
        if(candidateTime_>.15f)displayFloor_=candidate_;
        const int floor=displayFloor_;
        float blend=1-std::exp(-std::min(dt,.1f)*7);
        for(int i=0;i<3;++i)weights_[i]+=(float(i==floor)-weights_[i])*blend;
        auto* d=ImGui::GetBackgroundDrawList(); // Dialogue, eyelids and editor remain above it.
        const auto viewport=ImGui::GetIO().DisplaySize;
        float radius=std::min(108.f,viewport.y*.18f);
        ImVec2 center{radius+22,radius+22};float scale=radius/26.f;
        auto project=[&](float px,float pz){return ImVec2{center.x+(px-x)*scale,center.y-(pz-z)*scale};};
        auto inside=[&](ImVec2 p,float margin=0.f){float dx=p.x-center.x,dz=p.y-center.y;return dx*dx+dz*dz<(radius-margin)*(radius-margin);};
        d->AddCircleFilled(center,radius,C(4,13,24,210),64);
        for(const auto& n:nodes_) {
            float alpha=weights_[n.floor];if(alpha<.02f)continue;
            auto p=project(n.x,n.z);float half=Step*scale*.5f;
            if(!inside(p,half*1.5f+2))continue;
            d->AddRectFilled({p.x-half,p.y-half},{p.x+half,p.y+half},C(55,95,116,int(150*alpha)));
        }
        // A subtle hero tank silhouette helps orient overlapping 1F/2F decks.
        for(float tx=-8.4f;tx<8.5f;tx+=Step)for(float tz=8.4f;tz<16;tz+=Step) {
            auto p=project(tx,tz);float half=Step*scale*.5f;
            if(inside(p,half*1.5f+2))d->AddRectFilled({p.x-half,p.y-half},{p.x+half,p.y+half},C(15,119,162,int(110*(weights_[1]+weights_[2]))));
        }
        int node=Closest(x,y,z),guard=0;
        // Stable world-space dashed cadence. Cached breadth-first route never cuts across walls.
        while(node>=0 && node<int(next_.size()) && next_[node]>=0 && next_[node]!=node && guard++<int(nodes_.size())) {
            int b=next_[node];const auto& a=nodes_[node];const auto& q=nodes_[b];
            if((guard%4)<2 && weights_[a.floor]>.02f) {
                auto p=project(a.x,a.z),r=project(q.x,q.z);
                if(inside(p,3)&&inside(r,3))d->AddLine(p,r,C(195,221,203,int(115*weights_[a.floor])),1.8f);
            }
            node=b;
        }
        // Optional clue area: route remains on the required management-room
        // objective, while the exhibition room is communicated as a broad
        // blue search region rather than a competing navigation route.
        if(showOptionalExhibit) {
            auto p=project(26.f,13.2f);float dx=p.x-center.x,dz=p.y-center.y;
            float length=std::sqrt(dx*dx+dz*dz),rr=std::clamp(4.2f*scale,12.f,24.f);
            if(length>radius-rr-3){p.x=center.x+dx/length*(radius-14);p.y=center.y+dz/length*(radius-14);rr=8;}
            d->AddCircleFilled(p,rr,C(40,145,255,42),32);
            d->AddCircle(p,rr,C(80,185,255,215),32,1.7f);
            if(floor!=2)d->AddText(ImGui::GetFont(),13,{p.x+9,p.y-9},C(145,215,255,230),"2F");
        }
        const auto& goal=goals_[mission];
        if(goal.enabled) {
            auto p=project(goal.x,goal.z);float dx=p.x-center.x,dz=p.y-center.y;
            float length=std::sqrt(dx*dx+dz*dz);
            float goalRadius=std::clamp(goal.radius*scale,9.f,20.f);
            bool offscreen=length>radius-goalRadius-3;
            if(offscreen){p.x=center.x+dx/length*(radius-14);p.y=center.y+dz/length*(radius-14);}
            const int gf=Floor(goal.x,goal.y,goal.z);
            float rr=offscreen?7.f:goalRadius;
            d->AddCircleFilled(p,rr,C(238,195,95,32),32);
            d->AddCircle(p,rr,C(244,210,120,200),32,1.5f);
            if(gf!=floor)d->AddText(ImGui::GetFont(),13,{p.x+8,p.y-9},C(245,222,165,220),gf==0?"B1":gf==1?"1F":"2F");
        }
        ImVec2 forward{std::sin(yaw),-std::cos(yaw)},right{std::cos(yaw),std::sin(yaw)};
        d->AddTriangleFilled({center.x+forward.x*9,center.y+forward.y*9},{center.x-forward.x*6+right.x*5,center.y-forward.y*6+right.y*5},{center.x-forward.x*6-right.x*5,center.y-forward.y*6-right.y*5},C(222,246,255,255));
        d->AddCircle(center,radius,C(127,187,213,165),64,1.5f);
        d->AddText(ImGui::GetFont(),18,{center.x-12,center.y-radius+8},C(220,237,245,230),floor==0?"B1F":floor==1?"1F":"2F");
        d->AddText(ImGui::GetFont(),16,{center.x-radius+8,center.y+radius+7},C(185,209,224,210),goal.enabled?"破線:経路  ○:目的地":"探索中:目的地未確定");
    }
private:
    struct Node {float x,y,z;int floor;std::vector<int> edges;};
    static constexpr float Step=.6f,X0=-27,Z0=-10.2f;
    static constexpr int NX=103,NZ=193;
    static bool Blocked(const physics::CollisionWorld& world,float x,float y,float z) {
        for(const auto& b:world.Boxes()) {
            if(b.tag==physics::ColliderTag::Trigger||b.tag==physics::ColliderTag::Water)continue;
            // Operable terrace leaves are a valid route after player interaction.
            // Locked management/exterior doors remain obstacles. Physics is unchanged.
            if(b.name==L"TerraceDoorLeft" || b.name==L"TerraceDoorRight" ||
               b.name==L"Reception_StaffDoor")continue;
            if(y+1.85f<=b.minimum.y || y+.33f>=b.maximum.y)continue;
            if(x>b.minimum.x-.33f&&x<b.maximum.x+.33f&&z>b.minimum.z-.33f&&z<b.maximum.z+.33f)return true;
        }
        for(const auto& c:world.Circles()) {
            if(y+1.85f<=c.minimumY || y+.33f>=c.maximumY)continue;
            float dx=x-c.center.x,dz=z-c.center.y;
            if(dx*dx+dz*dz<(c.radius+.33f)*(c.radius+.33f))return true;
        }
        return false;
    }
    int Closest(float x,float y,float z) const {
        int result=-1;float best=1e9f;
        for(int i=0;i<int(nodes_.size());++i){const auto& n=nodes_[i];float dy=(n.y-y)*5;
            float d=(n.x-x)*(n.x-x)+(n.z-z)*(n.z-z)+dy*dy;if(d<best){best=d;result=i;}}
        return result;
    }
    void SetMission(int mission) {
        mission=std::clamp(mission,0,int(goals_.size())-1);if(mission_==mission)return;mission_=mission;
        next_.assign(nodes_.size(),-1);const auto& goal=goals_[mission];if(!goal.enabled)return;
        int end=Closest(goal.x,goal.y,goal.z);if(end<0)return;
        std::queue<int> q;q.push(end);next_[end]=end;
        while(!q.empty()){int n=q.front();q.pop();for(int b:nodes_[n].edges)if(next_[b]<0){next_[b]=n;q.push(b);}}
    }
    std::vector<Node> nodes_;std::vector<std::vector<int>> cells_;std::vector<int> next_;
    std::array<Goal,5> goals_{};std::array<float,3> weights_{0,1,0};int mission_=-1;float mapAlpha_=1;
    int displayFloor_=1,candidate_=1;float candidateTime_=0;
};
}
