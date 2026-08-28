/*==================================================================================================

   [manager.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/05/12
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#pragma once

#include "scene.h"

#include <memory>
#include <utility>

namespace framework
{
//前方宣言
class GameObject;
class Scene;

class SceneManager
{
public:
    ~SceneManager();

    void SetInitialScene(std::unique_ptr<Scene> scene);
    void RequestSceneChange(std::unique_ptr<Scene> scene);

    template<typename T, typename... Args>
    void ChangeScene(Args&&... args)
    {
        RequestSceneChange(
            std::make_unique<T>(std::forward<Args>(args)...));
    }
    void Update(const FrameContext& frame, const InputSystem& input);
    void Render(const RenderContext& context);

    [[nodiscard]] SceneDiagnostics GetDiagnostics() const;
    [[nodiscard]] Scene* GetCurrentScene() const { return currentScene_.get(); }
    [[nodiscard]] bool HasScene() const;

private:
    void ApplyPendingScene();

    std::unique_ptr<Scene> currentScene_;
    std::unique_ptr<Scene> pendingScene_;
};

// DM31_Game互換名
using Manager = SceneManager;
}

// DM31_Gameのグローバル型名との互換
using Manager = framework::Manager;
