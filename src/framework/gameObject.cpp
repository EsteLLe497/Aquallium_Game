/*==================================================================================================

   [gameObject.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/05/12
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#include "gameObject.h"

namespace framework
{
GameObject::GameObject(std::wstring name)
    : name_(std::move(name))
{
}

GameObject::~GameObject()
{
    Shutdown();
}

void GameObject::Destroy()
{
    destroyRequested_ = true;
}

bool GameObject::IsDestroyRequested() const
{
    return destroyRequested_;
}

const std::wstring& GameObject::Name() const
{
    return name_;
}

Transform& GameObject::GetTransform()
{
    return transform_;
}

const Transform& GameObject::GetTransform() const
{
    return transform_;
}

void GameObject::Initialize()
{
    if (initialized_)
    {
        return;
    }

    initialized_ = true;
    OnInitialize();
    for (const auto& component : components_)
    {
        component->OnAttach();
    }
}

void GameObject::Shutdown()
{
    if (!initialized_)
    {
        return;
    }

    for (auto iterator = components_.rbegin(); iterator != components_.rend(); ++iterator)
    {
        (*iterator)->OnDetach();
    }
    OnShutdown();
    initialized_ = false;
}

void GameObject::Update(const FrameContext& frame)
{
    if (destroyRequested_)
    {
        return;
    }

    OnUpdate(frame);
    for (const auto& component : components_)
    {
        if (component->IsEnabled())
        {
            component->Update(frame);
        }
    }
}

void GameObject::Render(const RenderContext& context)
{
    if (destroyRequested_)
    {
        return;
    }

    OnRender(context);
    for (const auto& component : components_)
    {
        if (component->IsEnabled())
        {
            component->Render(context);
        }
    }
}
}
