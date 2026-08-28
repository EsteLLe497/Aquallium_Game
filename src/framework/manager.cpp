/*==================================================================================================

   [manager.cpp]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/05/12
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#include "manager.h"

#include <stdexcept>

namespace framework
{
SceneManager::~SceneManager()
{
    if (currentScene_)
    {
        currentScene_->OnExit();
    }
}

void SceneManager::SetInitialScene(std::unique_ptr<Scene> scene)
{
    if (currentScene_ || pendingScene_)
    {
        throw std::logic_error("The initial scene has already been set.");
    }
    if (!scene)
    {
        throw std::invalid_argument("The initial scene cannot be null.");
    }

    pendingScene_ = std::move(scene);
    ApplyPendingScene();
}

void SceneManager::RequestSceneChange(std::unique_ptr<Scene> scene)
{
    if (!scene)
    {
        throw std::invalid_argument("The requested scene cannot be null.");
    }
    pendingScene_ = std::move(scene);
}

void SceneManager::Update(const FrameContext& frame, const InputSystem& input)
{
    //シーン切り替え
    ApplyPendingScene();
    if (currentScene_)
    {
        currentScene_->Update(frame, input);
    }
    ApplyPendingScene();
}

void SceneManager::Render(const RenderContext& context)
{
    //描画
    if (currentScene_)
    {
        currentScene_->Render(context);
    }
}

SceneDiagnostics SceneManager::GetDiagnostics() const
{
    return currentScene_ ? currentScene_->GetDiagnostics() : SceneDiagnostics{};
}

bool SceneManager::HasScene() const
{
    return currentScene_ != nullptr;
}

void SceneManager::ApplyPendingScene()
{
    if (!pendingScene_)
    {
        return;
    }

    if (currentScene_)
    {
        currentScene_->OnExit();
    }
    currentScene_ = std::move(pendingScene_);
    currentScene_->OnEnter();
}
}
