# Collision and navigation foundation

The aquarium uses a lightweight collision representation that is independent
from its render meshes. This keeps player motion deterministic and prevents
decorative or transparent geometry from becoming accidental floor polygons.

## Runtime roles

- `CharacterCapsule`: player radius, height, eye height, and step height.
- `WalkableRect`: flat public floors and upper walkways.
- `PathSurface`: sampled centre line plus usable half-width for ramps.
- `BoxCollider`: glass, walls, rails, props, and other solid blockers.
- `ColliderTag`: semantic role (`Walkable`, `Ramp`, `Glass`, `Rail`, etc.).
- `CollisionLayer`: query mask (`Player`, `World`, `Water`, `Trigger`).

`CollisionWorld::MoveCharacter` subdivides large motion to avoid tunnelling,
slides along flat boundaries, and projects ramp movement onto nearby polyline
segments. The active segment is retained while using a helical ramp, so two
vertically separated turns cannot be confused merely because their XZ
coordinates overlap.

## Why this is not a NavMesh yet

A navigation mesh answers where AI agents may walk; it does not replace a
player character controller or stop the camera capsule passing through walls.
The tagged `WalkableRect` and `PathSurface` records are deliberately suitable
as input for a later NavMesh bake while the current player uses direct capsule
collision.

## Adding an exhibit

1. Register its public floor as `Walkable`.
2. Register walls, glass, rails, and props as tagged blockers.
3. Register sloped or curved routes as `PathSurface` centre lines.
4. Keep render-only water and effects on non-blocking layers.
5. Query gameplay objects through `GameObject::Tag`,
   `ObjectWorld::FindFirstByTag`, or `ObjectWorld::FindAllByTag`.
