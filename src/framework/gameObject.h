/*==================================================================================================

   [gameObject.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/05/12
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#pragma once

#include "component.h"

#include <DirectXMath.h>
#include <cstdint>
#include <concepts>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace framework
{
//前方宣言
class Component;
class ObjectWorld;

struct Transform
{
    DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 rotation{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 scale{1.0f, 1.0f, 1.0f};
};

class GameObject
{
public:
    explicit GameObject(std::wstring name = L"GameObject");
    virtual ~GameObject();

    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;

    template<typename T, typename... Args>
        requires std::derived_from<T, Component>
    T& AddComponent(Args&&... args)
    {
        auto component = std::make_unique<T>(
            *this,
            std::forward<Args>(args)...);
        T& result = *component;
        components_.push_back(std::move(component));
        if (initialized_)
        {
            result.OnAttach();
        }
        return result;
    }

    template<typename T>
        requires std::derived_from<T, Component>
    [[nodiscard]] T* GetComponent() const
    {
        for (const auto& component : components_)
        {
            if (auto* result = dynamic_cast<T*>(component.get()))
            {
                return result;
            }
        }
        return nullptr;
    }

    void Destroy();
    [[nodiscard]] bool IsDestroyRequested() const;
    [[nodiscard]] const std::wstring& Name() const;
    void SetTag(std::wstring tag) { tag_ = std::move(tag); }
    [[nodiscard]] const std::wstring& Tag() const noexcept { return tag_; }
    [[nodiscard]] bool HasTag(const std::wstring& tag) const noexcept
    {
        return tag_ == tag;
    }
    void SetCollisionLayer(std::uint32_t layer) noexcept
    {
        collisionLayer_ = layer;
    }
    [[nodiscard]] std::uint32_t CollisionLayer() const noexcept
    {
        return collisionLayer_;
    }
    [[nodiscard]] Transform& GetTransform();
    [[nodiscard]] const Transform& GetTransform() const;

    // 原本の命名を保った互換API。削除自体はObjectWorldの更新境界まで遅延する。
    void setDestroy() { Destroy(); }

protected:
    virtual void OnInitialize() {}
    virtual void OnShutdown() {}
    virtual void OnUpdate(const FrameContext&) {}
    virtual void OnRender(const RenderContext&) {}

private:
    friend class ObjectWorld;

    void Initialize();
    void Shutdown();
    void Update(const FrameContext& frame);
    void Render(const RenderContext& context);

    std::wstring name_;
    std::wstring tag_ = L"Untagged";
    std::uint32_t collisionLayer_ = 1u;
    Transform transform_;
    std::vector<std::unique_ptr<Component>> components_;
    bool initialized_ = false;
    bool destroyRequested_ = false;
};
}

// DM31_Gameのグローバル型名との互換
using GameObject = framework::GameObject;
