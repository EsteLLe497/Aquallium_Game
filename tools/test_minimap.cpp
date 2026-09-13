#define NOMINMAX
#include "../src/player/MiniMap.h"
#include "../src/generated/ReceptionLobbyGenerated.h"
#include "../src/generated/JellyBasementGenerated.h"
#include <cassert>
#include <iostream>
int main(){
    physics::CollisionWorld world;
    auto box=[&](auto b){world.AddBox({b.name,{b.minX,b.minY,b.minZ},{b.maxX,b.maxY,b.maxZ},physics::ColliderTag(b.tag)});};
    auto floor=[&](auto f){world.AddWalkableRect({f.name,f.minX,f.maxX,f.minZ,f.maxZ,f.floorY});};
    for(auto b:reception_lobby::kBoxes)box(b);
    for(auto b:jelly_basement::kBoxes)box(b);
    for(auto f:reception_lobby::kFloors)floor(f);
    for(auto f:jelly_basement::kFloors)floor(f);
    for(auto c:jelly_basement::kCircles)world.AddCircle({c.name,{c.x,c.z},c.minY,c.maxY,c.radius,physics::ColliderTag(c.tag)});
    for(auto p:reception_lobby::kPaths){physics::PathSurface s;s.name=p.name;s.halfWidth=p.halfWidth;for(size_t i=0;i<p.count;++i)s.centerLine.push_back({p.points[i].x,p.points[i].y,p.points[i].z});world.AddPathSurface(s);}
    player::MiniMap map;map.Build(world);map.LoadGoals("asset/story");
    assert(map.NodeCount()>100);
    for(auto p: {DirectX::XMFLOAT3{7,-2.25f,3.25f},{-10,-2.25f,15},{18,-2.25f,10.4f},{-11.85f,3.25f,17},{11,3.25f,10},{0,3.25f,-4},{-10,-6.95f,80}}){
        bool route=map.HasRoute(p.x,p.y,p.z,0);
        std::cout<<p.x<<","<<p.y<<","<<p.z<<" route="<<route<<"\n";
        assert(route);
    }
    // 湾曲クラゲ室の非常口と、会話後に案内する2F管理室。
    assert(map.HasRoute(-10,-6.95f,80,1));
    assert(map.HasRoute(-10,-6.95f,90,2));
    assert(player::MiniMap::Floor(0,-2.25f,3)==1);
    assert(player::MiniMap::Floor(7,-2.69f,2.25f)==1); // seated opening
    assert(player::MiniMap::Floor(18,-2.25f,11)==2);
    assert(player::MiniMap::Floor(-10,-2.25f,25.1f)==0);
    assert(player::MiniMap::Floor(0,3.25f,3)==2);
    std::cout<<"Minimap tests passed; nodes="<<map.NodeCount()<<"\n";
}
