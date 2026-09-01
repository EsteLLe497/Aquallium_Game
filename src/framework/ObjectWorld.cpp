#include "ObjectWorld.h"

#include <algorithm>

namespace framework
{
ObjectWorld::~ObjectWorld()
{
    Clear();
}

GameObject* ObjectWorld::FindByName(const std::wstring& name) const
{
    const auto active = std::find_if(
        objects_.begin(),
        objects_.end(),
        [&name](const auto& object)
        {
            return object->Name() == name && !object->IsDestroyRequested();
        });
    if (active != objects_.end())
    {
        return active->get();
    }

    const auto pending = std::find_if(
        pendingObjects_.begin(),
        pendingObjects_.end(),
        [&name](const auto& object)
        {
            return object->Name() == name && !object->IsDestroyRequested();
        });
    return pending != pendingObjects_.end() ? pending->get() : nullptr;
}

GameObject* ObjectWorld::FindFirstByTag(const std::wstring& tag) const
{
    const std::vector<GameObject*> matches = FindAllByTag(tag);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<GameObject*> ObjectWorld::FindAllByTag(
    const std::wstring& tag) const
{
    std::vector<GameObject*> matches;
    const auto appendMatches = [&matches, &tag](const auto& source)
    {
        for (const auto& object : source)
        {
            if (!object->IsDestroyRequested() && object->HasTag(tag))
            {
                matches.push_back(object.get());
            }
        }
    };
    appendMatches(objects_);
    appendMatches(pendingObjects_);
    return matches;
}

void ObjectWorld::Update(const FrameContext& frame)
{
    ActivatePendingObjects();
    for (const auto& object : objects_)
    {
        object->Update(frame);
    }
    RemoveDestroyedObjects();
}

void ObjectWorld::Render(const RenderContext& context)
{
    for (const auto& object : objects_)
    {
        object->Render(context);
    }
}

void ObjectWorld::Clear()
{
    pendingObjects_.clear();
    for (const auto& object : objects_)
    {
        object->Shutdown();
    }
    objects_.clear();
}

void ObjectWorld::ActivatePendingObjects()
{
    if (pendingObjects_.empty())
    {
        return;
    }

    for (auto& object : pendingObjects_)
    {
        object->Initialize();
        objects_.push_back(std::move(object));
    }
    pendingObjects_.clear();
}

void ObjectWorld::RemoveDestroyedObjects()
{
    std::erase_if(
        objects_,
        [](const auto& object)
        {
            if (!object->IsDestroyRequested())
            {
                return false;
            }
            object->Shutdown();
            return true;
        });
}
}
