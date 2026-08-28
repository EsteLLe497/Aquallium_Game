# Aquarium Greybox Modeling Guide

## Generated assets

- `model/aquarium_greybox.glb`: modular single-floor aquarium greybox.
- `model/aquarium_route_01_02.glb`: V5 Entrance and Jellyfish Theater trial
- `model/aquarium_underwater_arch.glb`: 48 m descending underwater-arch route
  module. This is the model currently shown with key `3`.
- `concepts/aquarium-greybox-plan.png`: exact generated layout preview.
- `concepts/aquarium-greybox-isometric.png`: software-rendered GLB geometry preview.
- `tools/generate_aquarium_greybox.py`: source of truth for dimensions.
- `tools/generate_route_01_02.py`: source of truth for the V5 route 01-02
- `tools/generate_underwater_arch.py`: source of truth for the key-4 arch preview

## Descending underwater arch

Key `4` loads a dedicated 48 m greybox rather than the authoring-map view.
The route descends from 0.0 m to -4.7 m and automatically keeps the visitor
camera 1.58 m above the authored floor profile. `WASD` and arrow-key look remain
available, while lateral movement is constrained to the 5.44 m clear path.

### August 32 presentation pass

The key-4 module was re-authored from publicly released `Aquarium at August
32nd` trailer and gameplay stills as a composition reference, without copying
its assets. The route now uses a 6.4 m floor slab with approximately 5.44 m of
player movement, a 1.20 m arch spring, a 3.42 m horizontal by 3.72 m vertical
acrylic ellipse, and thin blue seams at 3 m intervals. A blue procedural checker
floor, opaque navy waist rails, and a dark open exit replace the former
continuous cyan guide strips and heavy black cage silhouette. The floor has no
local practical lights; illumination comes from the aquarium and exit outline.
The waist rails use an eight-sided sloped profile instead of stacked boxes,
with a separate six-sided top trim and grazing-angle blue reflections.

The presentation deliberately avoids an arch-local full-screen volume. Water
surface reflection, caustics, and receiver lighting still share the common
`AquariumLight` data, while the dry route stays visually clean and readable.

The first-pass module contains:

- a continuous semicircular water shell and a separate refractive glass shell
- 1.90 m vertical side walls below the arch spring, keeping the curve above
  the 1.58 m visitor eye line and raising the crown to approximately 5.0 m
- continuous structural ribs at 6 m intervals for distance and chase
  readability
- two floor-integrated cyan guide strips instead of ceiling lighting
- an unobstructed exit silhouette for clean route readability
- a bright end portal that stays readable from the entrance

The GLB uses nine material batches, 12,149 vertices and 8,392 triangles. Water
and glass have dedicated Stage shader surface types, allowing the module to be
replaced by an authored Blender GLB without changing the runtime view switch.

The arch water path samples the preserved HDR scene once and applies
screen-space micro-refraction, distance-dependent RGB Beer-Lambert absorption,
forward in-scattering and restrained high-frequency canopy caustics. Structural
ribs sit inside the glass radius and include floor-height legs, preventing the
glass pass from visually erasing sections of the black frame.

A denser tessellated water surface is defined independently at world
`Y = +5.8 m`. The entrance crown remains approximately 0.68 m below its mean
height and descends farther away, so
Beer-Lambert distance, surface transmission and the blue/deep lighting shift
come from actual authored depth rather than a route-progress color gradient.
Four animated wave bands displace the surface by up to approximately 0.28 m
in the vertex shader, with inexpensive pixel-normal ripples adding small-scale
reflection and refraction movement. Caustics are no
longer evaluated on the acrylic/water canopy proxy; only the authored arch
floor receives the projected pattern.

The arch receiver pass deliberately uses a lower-frequency caustic field than
the water surface. Its world scale is doubled and floor irradiance is raised
while the canopy remains caustic-free, producing bold readable pools of light
without returning to a pattern pasted onto the acrylic ceiling.

The existing `model/map.glb` is not overwritten.

Press `3` while the prototype is running to enter the generated Entrance and
Jellyfish Theater module. Press `2` to return to the existing imported stage
and glass view, or press `4` to enter the descending underwater arch.

## Route 01-02 module

- Entrance: 12 m x 9 m, 4.0 m ceiling.
- Dark vestibule: 3 m x 4 m, 3.2 m ceiling.
- Jellyfish Theater: 18 m x 15 m, 5.2 m ceiling.
- Seven slender cylindrical jellyfish tanks, 1.24-1.48 m in diameter.
- Alternating 3.35-4.05 m water heights form a column forest.
- No rectangular tanks in the Jellyfish Theater.
- A clear central walking band remains at least 3 m wide.
- Stable separate objects for the automatic doors, emergency release, starting
  bench, guide lights, and the Stage 3 exit.

The runtime loader now preserves base color per glTF primitive. This keeps
multi-material authored modules readable instead of applying the last material
to the entire stage.

### Water material families

Do not scale one hero-tank water material across every display. The loader and
stage shader currently recognize three independent families:

- `TankWaterLarge`: reserved for large ocean tanks. It may use caustics,
  absorption, scattering, and the heavier hero presentation.
- `TankWaterJellyCylinder`: transparent circular jellyfish water with a soft
  Fresnel edge, six-sample cylinder-local volumetric integration, animated
  low-frequency density, and no caustics.
- `TankWaterDisplayBox`: thinner rectangular display water with lower opacity
  and no caustics.

The underwater-arch floor reuses the hero-tank caustic ridge profile,
but at roughly 2.4 times its world-space cell size. This keeps individual light
pools bold without the three animated layers becoming a dense blue net. The
acrylic canopy remains excluded so the pattern reads as transmitted light on
solid receivers instead of a texture pasted onto the ceiling.

The route has a dedicated blue-biased lighting grade across water scattering,
surface reflection, glass highlights, receiver fill and caustics. Red and green
transmission are still retained so the result keeps depth variation instead of
becoming a flat monochrome overlay.

Four analytical service-light banks are positioned above the water surface at
12 m intervals along the left side of the route. Their rays are refracted into
the water and traced to each receiver, so caustic pools shift across the tunnel
with water depth instead of appearing uniformly beneath an invisible overhead
light. The same bank positions add restrained highlights to the authored water
surface.

Both small-display materials use alpha blending after opaque architecture and
read depth without writing it. This preserves the wall and props behind the
water. Tank rims remain independent emissive geometry; general ceiling lights
are not required.

## Coordinate and scale

- glTF right-handed, Y-up.
- 1 unit equals 1 meter.
- Building footprint: 38 m x 32 m.
- Typical public ceiling: 4.2 m.
- Main tank: 30 m wide, 9 m deep, 8 m high.
- Main viewing corridor: approximately 7 m deep.
- Standard gameplay door: 1.8 m wide x 2.5 m high.

The current `StageModel` loader converts glTF to the renderer's left-handed
coordinates and applies its existing floor offset.

## Named mesh groups

The GLB contains seven top-level meshes:

- `Floor`
- `Concrete`
- `TankShell`
- `Glass`
- `Door`
- `Pipe`
- `Route`

Important event-ready objects are authored separately in the generator:

- `MainTank_ViewingGlass`
- `BreakEvent_Glass_Intact`
- `Emergency_Exit`
- `PumpRoom_Door`
- `ControlRoom_Door`

The generator currently merges objects by material in the exported GLB to keep
draw and import overhead low. If individual runtime interaction is required,
export interactive doors and breakable glass as separate GLBs.

## Gameplay zones

1. Entrance lobby and locked emergency exit.
2. Main-tank viewing corridor.
3. Small exhibit and clue room.
4. Control room.
5. Rear maintenance corridor.
6. Pump and valve room.
7. Glass-break chase area.

The route forms a simple loop rather than a maze. The player sees the main tank
early, visits the back-of-house area, and returns to the public side for the
final chase.

## Regeneration

Run with the bundled Codex Python or any Python 3 installation containing
Pillow:

```powershell
python tools/generate_aquarium_greybox.py
```

Edit `build_layout()` to change dimensions. The script writes a self-contained
binary GLB with positions, normals, UVs, indices, materials, and named nodes,
then validates chunk and buffer ranges.

## Blender handoff

1. Import `model/aquarium_greybox.glb`.
2. Keep the scene unit scale at meters.
3. Delete the `Route` guide before final export.
4. Replace boxes with modular wall, floor, frame, and pipe kits.
5. Keep glass and animated doors separate.
6. Apply transforms and export as glTF Binary (`.glb`).
7. Keep `POSITION`, `NORMAL`, and `TEXCOORD_0`.

Do not add detailed props until the full route can be completed in-game.
