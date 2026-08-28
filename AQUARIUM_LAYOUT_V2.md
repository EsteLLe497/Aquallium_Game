# Aquarium Layout V2

This proposal removes the control room, pump room, and rear maintenance route.
It is a public-facing, single-floor route designed for a short escape game.

## Route

1. Entrance Lobby
2. Coastal Gallery
3. Main Tank Hall
4. Light Gallery
5. 34 m Underwater Arch Tunnel
6. Deep Sea Gallery
7. Final Dome and escape exit

The yellow route in `concepts/aquarium-floorplan-v2.png` is the intended
playthrough order. The room boundaries remain explicit so each doorway can be
used as a puzzle gate, occlusion boundary, and streaming boundary.

## Design intent

- The main tank remains visible early and acts as the spatial landmark.
- The underwater arch is a long, narrow spectacle section surrounded by water.
- Galleries use distinct rectangular footprints so art production can proceed
  room by room.
- The final dome is separated from the entrance and can host the glass-break or
  chase climax.
- No staff-only area is required for the first playable version.

## Performance intent

- Use one static GLB per room plus one tunnel shell GLB.
- Keep doors, breakable glass, puzzle props, and animated objects separate.
- Use door portals for room visibility and skip whole-room draw submission.
- Share wall, floor, frame, and glass materials across every room.
- Use the expensive water/glass composite only for visible tank portals.

The current `model/aquarium_greybox.glb` is deliberately unchanged until this
plan is approved.

## Lighting direction

Lighting changes by room rather than applying one blue preset to the whole
building. The reference research and proposed values are recorded in
`REAL_AQUARIUM_REFERENCE_NOTES.md`.

- Entrance: warm-neutral orientation space.
- Coastal Gallery: bright turquoise shallow-water light.
- Main Tank Hall: saturated blue, strong caustics, and restrained volume.
- Light Gallery: violet/magenta emissive accents.
- Underwater Arch: three lighting beats inside one continuous tunnel.
- Deep Sea Gallery: low exposure with sparse local emissive sources.
- Final Dome: calm blue that can transition to an emergency-red event state.
