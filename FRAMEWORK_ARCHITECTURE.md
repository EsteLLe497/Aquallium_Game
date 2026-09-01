# Aquarium Prototype Framework Architecture

## Goal

This refactor keeps the existing DirectX 11 aquarium rendering path intact while
separating reusable application responsibilities. It was informed by the
`DM31_Game` sample's Manager, Scene, GameObject, Component, Input, and Renderer
boundaries, but uses explicit ownership and deferred state changes.

## Runtime ownership

```text
D3D11App
  |- Win32 window and message pump
  |- D3D11 device, context, swap chain, back buffer
  |- InputSystem
  `- SceneManager
       `- AquariumScene
            |- AquariumRenderer
            |- PlayerManager
            `- AquariumSettings
```

The platform host does not know aquarium camera rules or lighting controls.
`AquariumScene` does not own the window, swap chain, or presentation policy.
`AquariumRenderer` remains responsible only for GPU resources and render passes.

## Framework modules

## DM31_Game name compatibility

The reusable base files intentionally retain the sample's original names,
capitalization, header banners, author, dates, and Japanese lifecycle comments:

| DM31_Game name | Runtime role |
|---|---|
| `main.h/.cpp` | Win32 entry-point boundary |
| `manager.h/.cpp` / `Manager` | Safe scene ownership and deferred transition |
| `scene.h` / `Scene` | Scene lifecycle interface |
| `gameObject.h/.cpp` / `GameObject` | Extensible stage object |
| `component.h` / `Component` | Reusable object behavior |
| `input.h/.cpp` / `Input` | Press/trigger/release keyboard state |
| `renderer.h/.cpp` / `Renderer` | D3D11 frame context boundary |

`[compornent.h]` is kept verbatim in the banner because it exists in the
reference source. The implementation does not reproduce `delete this`, raw COM
ownership, or double-deletion behavior; those are corrected behind the same
architectural surface.

### FrameContext

Carries clamped delta time, monotonic total time, and frame index. New systems
should receive this context instead of reading a global timer.

### InputSystem

Tracks current and previous keyboard states plus relative mouse movement and exposes:

- `IsDown`: continuous movement and tuning.
- `WasPressed`: one-shot view, pause, reset, and VSync actions.
- `WasReleased`: future interaction or UI actions.
- `MouseDeltaX/Y`: frame-relative first-person camera input.

Input is cleared when the game window loses focus, preventing stuck movement.
Relative mouse capture is released while the F2 lighting editor is visible.

### PlayerManager

Owns the physical player capsule, eye position, yaw/pitch and gameplay controls.
It converts WASD into camera-relative movement, applies a Shift sprint multiplier,
uses mouse delta for first-person look, and emits a world-space selection ray on
the left-click edge. `AquariumScene` only chooses the active collision world and
copies the resolved player pose into the render settings. This leaves interaction
raycasts extensible without returning input code to the scene.

### Scene and SceneManager

Scenes own game-specific state and rendering. `RequestSceneChange` is deferred
until a safe update boundary, so a scene can request a transition without
destroying itself midway through its own callback. Scenes are exclusively owned
by `std::unique_ptr`.

A future entrance, main aquarium, loading screen, or lighting laboratory can be
a separate `Scene` without modifying the Win32/D3D host.

### GameObject, Component, and ObjectWorld

These are an optional lightweight object model for authored stage objects,
interactive doors, exhibit signs, cameras, fish schools, and reusable behaviors.

- `ObjectWorld` exclusively owns `GameObject` instances.
- `GameObject` exclusively owns its `Component` instances.
- `Component::Owner()` is non-owning and cannot delete the object.
- `Destroy()` only marks an object; `ObjectWorld` removes it after update.
- Pending spawns are activated at the next update boundary.
- Empty worlds add no render passes and no GPU cost.

This deliberately avoids the sample's global manager, raw ownership, and
self-deletion patterns.

## Existing behavior preserved

- `1`: underwater view.
- `2`: imported stage plus visitor-side glass view.
- `3`: generated aquarium greybox entrance view.
- `4`: original stage authoring view.
- `WASD`: camera-relative movement.
- `Shift`: sprint while moving.
- `Mouse`: first-person view rotation.
- `Left click`: request selection along the current view ray.
- `QE`: vertical movement in free underwater preview mode.
- Arrow keys: keyboard look fallback.
- `J/L`, `I/K`, `U/O`, `N/M`: existing lighting tuning.
- `Space`: pause.
- `R`: reset aquarium settings.
- `V`: VSync toggle, still owned by the platform host.
- `Escape`: close the native window.

The shaders, froxel volume, temporal accumulation, caustics, glass composite,
stage model loader, and render-target allocation were not rewritten.

## Adding a scene

1. Derive a class from `framework::Scene`.
2. Own its renderers, state, and optional `ObjectWorld` inside that class.
3. Implement `Update`, `Render`, and `GetDiagnostics`.
4. Construct it with all required resources.
5. Call `SceneManager::RequestSceneChange(std::make_unique<NewScene>(...))`.

Do not reach into `D3D11App` from a scene. Add a narrow service or command
interface when a platform action is genuinely required.

## Adding reusable gameplay behavior

Derive from `framework::Component`. Component constructors receive their owner:

```cpp
class RotateComponent final : public framework::Component
{
public:
    RotateComponent(framework::GameObject& owner, float speed)
        : Component(owner), speed_(speed) {}

    void Update(const framework::FrameContext& frame) override
    {
        Owner().GetTransform().rotation.y += speed_ * frame.deltaTime;
    }

private:
    float speed_;
};
```

Then create it through an `ObjectWorld` owned by a scene:

```cpp
auto& sign = world.Spawn<>(L"EntranceSign");
sign.AddComponent<RotateComponent>(0.2f);
```

## Recommended next layers

Add these only when a real feature needs them:

1. `AssetRegistry`: cache GLB, textures, and shaders by stable asset ID.
2. `RenderGraph` or explicit pass scheduler: order shadow, opaque, water,
   volumetric, glass, and post-processing resources.
3. `AquariumZone`: data describing water bounds, optical coefficients, lights,
   and quality tier per tank.
4. `CollisionAsset`: serialize the existing tagged `CollisionWorld` proxies
   from DCC-authored scene metadata instead of duplicating dimensions in C++.
5. Data serialization: scene and exhibit definitions outside C++.

Keeping those additions demand-driven prevents a speculative framework from
becoming heavier than the aquarium itself.

## Portfolio summary

- Refactored a monolithic DirectX 11 prototype into platform, input, scene,
  object/component, and rendering responsibilities.
- Implemented edge-triggered input and focus-safe keyboard state.
- Implemented deferred scene transitions with RAII ownership.
- Implemented deferred GameObject destruction and pending object activation.
- Preserved the production render path and its temporal history behavior during
  the architecture migration.
