# Aquarium Layout V5 - Continuous Bathymetric Promenade

V5 removes the feeling of clearing a sequence of independent rooms. The public
side is one continuous visitor promenade. Technical room chunks still exist for
culling and streaming, but doors are used only where the story requires them.

## Core experience

- Follow one obvious aquarium route without choosing branches.
- Walk beside water almost continuously after the Jellyfish Theater.
- Descend physically as the exhibits change from coast to open ocean and deep
  sea.
- Make the Grand Ocean Hall the visual peak.
- Enter the descending Underwater Arch immediately after the Grand Ocean Hall.
- Let lighting, water color, ceiling height, and sound communicate depth instead
  of room title cards.
- Keep interaction rare. Observation and movement are the main gameplay.

## New public route

1. **Entrance** — one short check of the locked public exit.
2. **Jellyfish Theater** — wake-up point and the only short backtrack.
3. **Coastal Promenade** — a continuous shallow-water exhibit wall.
4. **Reef Slope** — the same promenade broadens and begins to descend.
5. **Grand Ocean Hall** — a long gallery with the hero tank on the entire right.
6. **Descending Underwater Arch** — begins directly at the far end of the hero
   tank and drops through the water column.
7. **Twilight Promenade** — the arch opens into a low, dark observation path.
8. **Deep Sea Panorama** — the final large window and physical emergency exit.

Stages 3, 4, and 7 are presentation zones on a shared path, not rooms that need
to be unlocked. The old Luminous Gallery and Deep Sea Descent puzzles are
removed.

## Elevation profile

| Route zone | Approx. length | Floor elevation | Character |
|---|---:|---:|---|
| Entrance / Jellyfish | 30 m | ±0.0 m | Building level |
| Coastal Promenade | 24 m | 0.0 to -0.5 m | Shallow coast |
| Reef Slope | 28 m | -0.5 to -1.5 m | Bright reef |
| Grand Ocean Hall | 24 m | -1.5 to -2.0 m | Open ocean |
| Descending Arch | 48 m | -2.0 to -4.7 m | Midwater descent |
| Twilight Promenade | 24 m | -4.7 to -5.2 m | Deep sea |
| Deep Sea Panorama | 18 m | -5.2 m | Final basin |

The arch slope is approximately 1:18. This is visibly downhill while remaining
comfortable for normal first-person movement. The route should use long ramps,
not frequent stairs. Short two-step level changes may appear beside the visitor
path as architectural detail, but never block the main route.

## Plan composition

The route bends in a long S shape so adjacent lighting zones cannot be seen at
the same time. Stage 5 and Stage 6 share a straight visual axis:

```text
Grand Ocean Hall
  hero tank on the right
  water surface visible high above
        |
        v
Descending Underwater Arch
  same tank water, now wrapping over the player
  floor and ceiling descend together
        |
        v
Twilight Promenade -> Deep Sea Panorama
```

The transition from Stage 5 to Stage 6 has no puzzle room between them. The
large acrylic wall curves overhead and becomes the arch while the visitor floor
starts descending.

## Continuous lighting progression

Lighting is sampled from route distance and elevation, not switched abruptly by
room number.

| Zone | Walkway lighting | Water appearance |
|---|---|---|
| Coastal | soft cyan fill, high exposure | pale cyan, sharp caustics |
| Reef | turquoise key, broad soft shafts | saturated aqua, active caustics |
| Grand Ocean | ocean-blue key, dark visitor side | blue absorption, tall shafts |
| Upper arch | blue edge lights remain visible | surface caustics begin fading |
| Lower arch | sparse navy guide lights | stronger absorption and scattering |
| Twilight | low blue floor guidance | near-black distance, isolated bioluminescence |
| Panorama | one controlled overhead shaft | deep blue-black silhouette field |

Important realism rule: corridor fog and tank water are separate media. The air
side receives subtle haze only. Strong absorption and scattering belong inside
the water volume behind the glass.

## Compatibility with V4 names

| V4 | V5 |
|---|---|
| Coastal Gradient | Coastal Promenade |
| Sunlit Reef Gallery | Reef Slope |
| Grand Ocean Hall | Grand Ocean Hall |
| Luminous Gallery | Removed as a room; luminous accents move into the lower arch |
| Underwater Arch | Descending Underwater Arch |
| Deep Sea Descent | Replaced by Twilight Promenade |
| Deep Sea Panorama | Deep Sea Panorama |

During implementation, old numeric IDs can remain as aliases while authored
content migrates to stable string IDs. Do not make save data depend on display
numbers.

## Rendering and streaming boundary

The public route is continuous, but rendering remains modular:

```text
route_coastal
route_reef_slope
route_grand_ocean
route_arch_upper
route_arch_lower
route_twilight
route_panorama
shared_ocean_water
```

- Keep current, previous, and next route chunks resident.
- Use bends, tank rock masses, and ceiling ribs as occluders.
- Share one ocean-water simulation between Grand Ocean Hall and the arch.
- Reuse caustics and volumetric resources; only their weights change.
- Disable surface caustics progressively below the upper third of the arch.
- Keep one froxel volume alive and change its profile instead of allocating one
  per zone.
- Cull the hero tank rear geometry and fish layers using the visitor route
  direction.

This V5 plan remains design-only. It does not overwrite either current GLB.
