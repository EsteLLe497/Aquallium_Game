# Continuous Aquarium Route 07

## Confirmed route

`Entrance -> Hero Tank Hall -> Tank-left side gallery -> Descending Underwater Arch -> Jelly Column Room -> Curved Panorama Jelly Room`

The 2F helix remains an optional branch from the hero hall. Its H walkway has
facades for Management, Dolphin, a future exhibit, and an outdoor terrace. The
terrace entrance sits beyond the H crossing rather than directly beside the
ramp landing.

## Coordinates and scale

- One authored unit is one metre; `StageModel` applies the common `-2.25 m`
  stage-floor offset and glTF Z conversion.
- Hero tank face: `X=7.0`, width `29.0 m`, water height `12.1 m`.
- Hero tank depth: reduced from `14.7 m` to `10.5 m`.
- Entrance: `X=-42..-28`, `Z=-6..6`.
- Side gallery: runtime `X=7..22`, `Z=14.7..20.9`.
- Underwater arch: translated to runtime origin `(22, 0, 17.8)` and descends
  `4.7 m` over `48 m`.
- Basement jelly rooms: runtime `X=70..110`, centred at `Z=17.8`.
- 2F floor: authored `Y=12.28` (`10.03 m` after stage offset).

## Runtime architecture

The route is intentionally split into three render chunks:

1. `aquarium_watatsumi_hall.glb` — hero tank, hall, helix, H walkway.
2. `aquarium_underwater_arch.glb` — reused with an import translation.
3. `aquarium_continuous_shell.glb` — entrance, connectors, basement rooms,
   curved jelly display, and 2F destination facades.

Key `7` renders all three chunks but uses one `CollisionWorld`. This keeps the
current prototype continuous while preserving future frustum/portal culling,
asynchronous chunk loading, and per-zone lighting control.

## Portfolio-ready techniques

- Modular GLB stage streaming boundary with a continuous physical route.
- One tagged capsule collision world assembled from reusable zone colliders.
- Shared hero tank observed through front and curved side acrylic.
- Reused descending path surface with coordinate translation and C1 elevation.
- Large-pane glass keeps its cheap Fresnel path; only curved acrylic consumes
  the extra refraction copy.
- Water, glass, architecture, furniture, and event doors remain separate
  material/tag families for later light puzzles and interactions.

## Current deliberate placeholders

- The four 2F destinations are closed facade modules, ready for separate room
  GLBs; their final interiors are not duplicated inside the hall chunk.
- The panorama jelly water uses a low-segment faceted curve to minimize draw
  cost. Art replacement can preserve its material and collision contract.
- Zone-based culling is the next optimization step after visual walk-through
  confirms the final doorway positions.
