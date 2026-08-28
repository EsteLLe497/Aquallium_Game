# Aquarium Layout V4 - Linear Descent Promenade

V4 replaces the Hero Tank Loop with a nearly one-way visitor route. It uses the
reference research in `REAL_AQUARIUM_REFERENCE_NOTES_V2.md`.

## Core decisions

- The visitor route is a continuous S-shaped promenade with no optional room
  branches.
- The player starts in Stage 2, makes one short entrance check in Stage 1, then
  proceeds through Stages 2-9 without returning to an earlier room.
- The route adds scenic stages but does not add puzzles or enemies.
- The aquarium never shows the original world through water, glass, portals, or
  reflections.
- The Grand Ocean Hall is a narrow, vertically oriented approach.
- Its entire right wall is a tall hero-tank window, so the water remains in the
  player's peripheral view while they walk toward Stage 6.
- The large tank and underwater arch remain separate architectural chunks, but
  reuse the same water, caustics, and distant-school rendering assets.

## Route

1. **Entrance Lobby** - warm orientation light and the locked exit.
2. **Jellyfish Theater** - Kamo-inspired dark gallery and circular focal tank.
3. **Coastal Gradient** - shore, river mouth, rock pool, and shallow sea.
4. **Sunlit Reef Gallery** - bright turquoise breathing space.
5. **Grand Ocean Hall** - Churaumi-inspired vertical approach with the hero
   tank covering the right wall.
6. **Luminous Gallery** - compact violet display room and light-memory puzzle.
7. **Underwater Arch** - Marinepia-inspired continuous tank tunnel and chase.
8. **Deep Sea Descent** - narrow navy gallery and pulse-guided walk.
9. **Deep Sea Panorama** - final silhouette, emergency release, and physical
   exit.

Conceptual footprint: approximately 90 m by 38 m.

Conceptual visitor path: approximately 170 m, excluding pause time.

## Gameplay density

The additional rooms are visual pacing, not feature growth.

| Stage | Gameplay |
|---|---|
| 1 | Inspect locked exit |
| 2 | Observation only |
| 3 | Coastal three-symbol puzzle |
| 4 | Observation only |
| 5 | Observation and story contradiction only |
| 6 | Three-color light-memory puzzle |
| 7 | The only full chase |
| 8 | One-button pulse-guided walk and exit lever |
| 9 | Scripted ending sequence |

The first playable still has three puzzles, one chase, and one ending.

## Lighting progression

1. Warm neutral lobby.
2. Black space with pale cyan jellyfish rims.
3. Shallow cyan with moving caustics.
4. Sunlit turquoise and broad soft shafts.
5. Saturated ocean blue with a high water-surface key light.
6. Violet and blue emissive strips.
7. Cyan arch that loses lights from back to front during the chase.
8. Low-exposure navy with discrete pulse illumination.
9. Almost black blue, changing to controlled emergency red only at the end.

Room transitions use 2-3 m bends, dark vestibules, or ceiling compression so
lighting presets can blend without exposing unloaded rooms.

## Performance boundary

- Divide the route into nine room chunks and one right-side hero-tank chunk.
- Keep only the current room, the next room, and the shared tank visible.
- Use portal/doorway visibility rather than rendering the whole promenade.
- Bake static architectural lighting; reserve dynamic caustics for visible tank
  surfaces.
- Use volumetric lighting only in Stages 5 and 7, with the existing
  resolution/quality scaling.
- Reuse water materials, caustics atlases, and distant fish-school simulation
  between the Grand Ocean Hall and Underwater Arch; their geometry and
  visibility chunks remain separate.
- Stage 2's many tanks share material instances and one animation atlas.
- Scenic rooms must not allocate new shadow maps or new froxel volumes.

## Modeling boundary

Build the architecture in Blender or another model tool as separate modules:

```text
stage_01_entrance
stage_02_jellyfish
stage_03_coastal
stage_04_reef
stage_05_grand_ocean
stage_06_luminous
stage_07_arch
stage_08_deep_sea
stage_09_deep_sea_panorama
shared_hero_tank
```

Use programmatic geometry only for collision proxies, visibility portals,
trigger volumes, and debug visualization.

This plan does not overwrite `model/aquarium_greybox.glb`.

Prop placement, walking lines, light interactions, and presentation triggers are
specified in `AREA_FLOW_DETAIL_V1.md`.
