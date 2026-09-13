#pragma once
#include "PlayerManager.h"
#include <cmath>

// Route09 hinged leaves. Closed and open poses share rendering/collision state.
namespace player {
class TerraceDoor {
public:
    bool IsOpen() const noexcept { return targetOpen_; }
    float Angle() const noexcept { return angle_; }
    void Apply(physics::CollisionWorld& world,bool collisionOpen) const {
        for (int side : {-1,1}) {
            const float x=side*(collisionOpen ? 1.5f : .75f);
            const float hx=collisionOpen ? .08f : .75f;
            world.SetBoxBounds(side<0 ? L"TerraceDoorLeft" : L"TerraceDoorRight",
                {x-hx,3.25f,collisionOpen ? -1.5f : -.08f},
                {x+hx,6.45f,collisionOpen ? 0.0f : .08f});
        }
    }
    bool Select(const SelectionRay& ray) {
        if (std::abs(ray.direction.z)<.001f) return false;
        const float t=-ray.origin.z/ray.direction.z;
        const float x=ray.origin.x+t*ray.direction.x;
        const float y=ray.origin.y+t*ray.direction.y;
        if (t<0 || t>3.2f || std::abs(x)>1.5f || y<3.25f || y>6.45f)
            return false;
        // Do not swing through the player or close around their capsule.
        if (std::abs(ray.origin.x)<1.9f && ray.origin.z> -1.9f && ray.origin.z<.45f)
            return false;
        targetOpen_=!targetOpen_; return true;
    }
    void Update(float dt,physics::CollisionWorld& world) {
        const float target=targetOpen_?DirectX::XM_PIDIV2:0.f;
        const float step=std::min(std::abs(target-angle_),dt*.92f);
        angle_+=target>angle_?step:-step;
        const bool collisionOpen=angle_>DirectX::XMConvertToRadians(64.f);
        if(collisionOpen!=collisionOpen_){collisionOpen_=collisionOpen;Apply(world,collisionOpen_);}
    }
    void SetOpen(bool value,physics::CollisionWorld& world) {
        targetOpen_=value;angle_=value?DirectX::XM_PIDIV2:0.f;collisionOpen_=value;Apply(world,collisionOpen_);
    }
private:
    bool targetOpen_=false,collisionOpen_=false;
    float angle_=0.f;
};
}
