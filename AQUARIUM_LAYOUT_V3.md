# Aquarium Layout V3

V3 applies the real-aquarium research in
`REAL_AQUARIUM_REFERENCE_NOTES.md` without copying one facility.

## Core decisions

- One 32 m by 11 m hero tank is the spatial core.
- The same tank is presented through a panoramic window, an angled tunnel-side
  bay, and small deep-sea portholes.
- The visitor route remains single-floor and mostly one-way.
- Lighting progresses from warm orientation light to shallow cyan, saturated
  aquarium blue, violet emissive light, and finally low-exposure deep-sea navy.
- The 34 m underwater arch contains three lighting beats inside one continuous
  mesh and render zone.

## Route

1. Entrance Lobby
2. Coastal Gallery
3. Main Tank Hall, panoramic View A
4. Light Gallery power puzzle, then return to Room 3
5. Underwater Arch entered directly from Room 3, angled View B
6. Deep Sea Gallery, porthole View C
7. Final Dome and escape exit

## Why one shared tank

Repeated views give the building a memorable landmark while keeping the
expensive water, caustics, shadow, and volumetric-light simulation shared.
Only the visible architectural portal and room lighting preset change.

## Lighting progression

The plan's bottom legend is the intended visual order. Doorway portals provide
1.5-2.0 m transition bands for smooth preset blending.

The Main Tank Hall is 48 m by 10 m. Room 4 is a required side-puzzle loop, but
the main route and the underwater-arch gate remain inside Room 3.

The final dome begins with the normal blue preset and changes to emergency red
only after the climax event. This state is data-driven rather than a separate
room renderer.

The current 20-30 minute narrative sequence is documented in
`EVENT_FLOW_V4.md`. It starts in Room 4, sends the player to check the Entrance,
and later returns to Room 4 for the light-memory puzzle.

## Implementation boundary

This plan does not overwrite `model/aquarium_greybox.glb`. After approval, the
next greybox should be exported as room modules:

- Entrance
- Coastal Gallery
- Main Tank Hall and shared tank shell
- Light Gallery
- Underwater Arch
- Deep Sea Gallery
- Final Dome

Doors, porthole frames, breakable glass, and puzzle props stay separate from
the static room meshes.
