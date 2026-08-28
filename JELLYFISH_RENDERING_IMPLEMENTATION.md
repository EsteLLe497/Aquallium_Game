# Jellyfish Exhibit Rendering

## Implemented

- Dedicated `JellyfishRenderer`, separate from imported stage ownership.
- One procedurally generated bell and eight ribbon tentacles per jellyfish.
- A shallow translucent underside membrane keeps the bell valid from either
  side of the exhibit.
- GPU instancing for 49 jellyfish across seven cylindrical tanks.
- Each tank contains one medium focal jellyfish and six smaller individuals.
- Per-instance position, scale, phase, tint, drift, and pulse speed.
- Vertex-shader bell contraction, delayed-looking vertical drift, and
  independent tentacle waves.
- 84 camera-facing suspended-particle instances.
- Transparent cylinder glass with a Fresnel edge and subtle vertical streak.
- Local blue floor bounce around each exhibit.
- Explicit pass order:
  1. Opaque architecture and tank bases.
  2. Jellyfish and suspended particles.
  3. Volumetric water and outer glass.
  4. Existing volume composite and tone mapping.
- Independent MRT blending writes transparent biology, water, and glass only
  to HDR color. Linear depth and motion remain owned by opaque geometry,
  preventing temporal-history corruption from overlapping transparent layers.
- Key `5` selects a fixed reverse exhibit view used to regression-test the
  former bench-side black-patch failure.

## Runtime validation

- Debug x64 build: 0 warnings, 0 errors.
- Runtime HLSL compilation succeeded.
- 1280 x 720 close exhibit view with 49 jellyfish: 179 FPS observed.
- The application remained alive throughout the automated route-view probe.

The FPS value is a captured observation and not a hardware-independent
guarantee.

## Current limitations

- Instance placement is prototype data inside `JellyfishRenderer`; it should
  move to tank-authored data when the stage format is finalized.
- Jellyfish are procedurally animated rather than bone-skinned.
- Transparent jellyfish are drawn as one instanced batch. Per-instance sorting
  can be added if future overlapping hero creatures reveal ordering artifacts.
- Screen-space background refraction is not yet sampled by the cylinder glass;
  the current glass uses a lightweight Fresnel approximation.
- The project has no audio service. Pump ambience and room reverb are deferred
  until an `IAudioService` or equivalent subsystem exists, rather than binding
  platform audio calls directly into rendering code.

## Reverse-view black-patch fix

The bench-side view previously blended transparent jellyfish, water, and glass
into all three MRT outputs. Alpha-blended linear depth and motion vectors
poisoned temporal volume reprojection where many transparent layers overlapped,
which appeared as black jellyfish fragments and occasionally black tank-sized
regions. Both transparent blend states now enable independent blending:

- RT0 HDR color: alpha blending enabled.
- RT1 linear depth: writes disabled.
- RT2 motion vectors: writes disabled.

Safe billboard basis construction and finite-value rejection remain as
additional protection for cameras that approach an instance center line.

## Portfolio wording

Implemented a DirectX 11 aquarium exhibit pipeline using GPU-instanced,
procedurally animated translucent jellyfish, vertex-shader ribbon tentacles,
shape-bounded volumetric water, suspended particles, Fresnel glass, and
localized exhibit bounce lighting. The stage renderer was split into opaque
and transparent passes so biology is composited inside the water rather than
drawn as a foreground overlay.
