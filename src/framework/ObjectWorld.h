#pragma once

#include "gameObject.h"

#include <concepts>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace framework
{
class ObjectWorld
{
public:
    ~ObjectWorld();

    ObjectWorld(const ObjectWorld&) = delete;
    ObjectWorld& operator=(const ObjectWorld&) = delete;
    ObjectWorld() = default;

    template<typename T = GameObject, typename... Args>
        requires std::derived_from<T, GameObject>
    T& Spawn(Args&&... args)
    {
        auto object = std::make_unique<T>(std::forward<Args>(args)...);
        T& result = *object;
        pendingObjects_.push_back(std::move(object));
        return result;
    }

    [[nodiscard]] GameObject* FindByName(const std::wstring& name) const;
    void Update(const FrameContext& frame);
    void Render(const RenderContext& context);
    void Clear();

private:
    void ActivatePendingObjects();
    void RemoveDestroyedObjects();

    std::vector<std::unique_ptr<GameObject>> objects_;
    std::vector<std::unique_ptr<GameObject>> pendingObjects_;
};
}
