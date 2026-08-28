# Real Aquarium Reference Notes

Research date: 2026-07-29

This note records public, official aquarium references used to refine
`AQUARIUM_LAYOUT_V2.md`. The references are architectural and presentation
inspiration, not plans to copy a specific facility.

## Reference findings

### Osaka Aquarium Kaiyukan

Official map:
https://www.kaiyukan.com/info/area/map/

- Visitors enter through the Aqua Gate, rise to the upper level, and then move
  down through themed floors.
- The Pacific Ocean exhibit is encountered from several floors, so one major
  tank remains a repeated spatial landmark.
- Supporting exhibits change around the central tank instead of requiring a
  new hero tank in every zone.

Application to V2:

- Keep the Main Tank visible from more than one room.
- Add two or three differently framed portals to the same tank.
- Reuse one expensive water simulation rather than rendering a unique large
  aquarium for every room.

### Okinawa Churaumi Aquarium

Official area guide:
https://churaumi.okinawa/area/

- The visitor story progresses from the fourth-floor invitation to the ocean,
  through coral reefs and the Kuroshio, and finally to the first-floor deep
  sea.
- Spatial progression and ecological depth are aligned.

Application to V2:

- Use a deliberate light gradient: bright coastal water, saturated main-tank
  blue, then a dark deep-sea gallery.
- The lighting change should communicate progress before a sign or UI prompt
  is read.

### Shinagawa Aquarium

Official floor map:
https://www.aquarium.gr.jp/floor_map

- The official guide describes the tunnel aquarium as a 22 m "walk through the
  sea" beneath a 500-ton tank.
- The tunnel is treated as its own attraction between other exhibits.

Application to V2:

- The proposed 34 m arch is plausible as a hero section, but it needs three
  visual beats so it does not feel like one repeated tube.
- Use an entry silhouette, a bright central shaft, and a darker exit bend.

### Sumida Aquarium

Official floor guide:
https://www.sumida-aquarium.com/about/floor/

- One large Ogasawara tank is viewed from both the sixth and fifth floors.
- The Aqua Scope adds small round windows behind the same tank.
- A kaleidoscope tunnel uses mirrors and lighting to turn circulation into an
  attraction.
- Different exhibits use distinct presentation languages: open coral viewing,
  a fantasy jellyfish area, and Japanese-styled goldfish decoration.

Application to V2:

- Reframe the Main Tank through one panoramic opening and smaller side windows.
- Treat the transition corridor as authored content, not empty connective
  space.
- Give each gallery a clearly different silhouette and lighting language.

### Maxell Aqua Park Shinagawa

Official guides:
https://www.aqua-park.jp/aqua/guide/
https://www.aqua-park.jp/aqua/guide/ground.html
https://www.aqua-park.jp/aqua/guide/upperfloor.html

- The entrance combines fish, projected imagery, and art to build expectation.
- Coral Cafe Bar uses black light and luminous coral.
- Jellyfish Rumble is a 9 m by 35 m space whose sound and light change with
  time and season.
- Wonder Tube is an approximately 20 m tunnel using daylight from skylights;
  its lighting changes after evening.

Application to V2:

- Allow authored lighting presets to change by room and by event state.
- Use emissive surfaces for the Light Gallery instead of making every effect a
  volumetric light.
- Provide a day, night, and emergency preset for the underwater arch.

## Proposed room lighting

The values below are preliminary normalized tuning values for the current
prototype, not physical photometric units.

| Zone | Main palette | Caustics | Volume | Anisotropy | Exposure | Purpose |
|---|---|---:|---:|---:|---:|---|
| Entrance | warm neutral + faint cyan | 0.05 | 0.08 | 0.30 | 1.00 | Eye adaptation and orientation |
| Coastal Gallery | turquoise + green | 0.70 | 0.35 | 0.45 | 1.00 | Bright, shallow-water introduction |
| Main Tank Hall | deep blue + cyan key | 1.20 | 1.15 | 0.65 | 0.95 | Primary visual landmark |
| Light Gallery | violet + magenta emissive | 0.25 | 0.55 | 0.50 | 0.90 | Stylized contrast and puzzle room |
| Underwater Arch | aqua overhead shafts | 1.35 | 0.85 | 0.70 | 0.92 | Surrounded-by-water spectacle |
| Deep Sea Gallery | navy + sparse emissive | 0.05 | 0.50 | 0.75 | 0.70 | Quiet tension before the climax |
| Final Dome | blue, then emergency red | 0.80 | 1.00 | 0.60 | 0.85 | Break/chase event and escape |

## Runtime design

Do not create one volumetric renderer per room. That would multiply the most
expensive pass.

Use:

```text
LightingZone
  id
  oriented bounds
  preset id
  transition width
  priority

LightingPreset
  ambient and key colors
  exposure
  caustics strength
  volume strength
  anisotropy
  fog absorption/scattering
  active light mask
```

`LightingZoneManager` should find the active zone from the camera position and
blend to the next preset with `smoothstep` across a 1.5-2.0 m doorway band.
The existing aquarium renderer remains one shared pipeline; only constants and
visible tank/light masks change.

Performance rules:

- One shared froxel/volume pass per frame.
- One active room preset plus one transition preset.
- Cull whole room meshes at closed portals.
- Update shadow maps only for visible or dirty lights.
- Use emissive materials and bloom for decorative strips.
- Reserve ray-marched volume for the Main Tank, Arch, and Final Dome.
- Use inexpensive height fog or no volume in the Entrance and Coastal Gallery.

## Recommended V2 adjustment

Keep the current seven-zone plan, but divide the 34 m arch visually into:

1. Dark entry with a strong silhouette.
2. Bright central skylight and strongest caustics.
3. Blue-violet exit bend leading into the Deep Sea Gallery.

This preserves a single tunnel mesh and render zone while creating the
perceived variety of three spaces.
