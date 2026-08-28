# Jellyfish Cylinder Volume

## Goal

Add visible depth and illuminated water density to the jellyfish cylinders
without re-enabling the full-screen aquarium volume pass.

## Implementation

- The existing cylinder shell acts as the ray entry proxy.
- Back faces are discarded to avoid integrating the same water twice.
- An approximate view-dependent chord is calculated through each cylinder.
- Six fixed midpoint samples integrate local density and incident blue light.
- Beer-Lambert extinction accumulates transmittance along the chord.
- Top and bottom exhibit lights use separate attenuation curves.
- Animated continuous multi-wave density variation is generated procedurally,
  with no cell quantization or additional texture fetches.
- Standard alpha blending keeps the room and future creatures visible through
  the water.
- Forty-eight-sided display geometry keeps the bright circular caps and water
  silhouette smooth at close range.

The work is limited to pixels covered by `TankWaterJellyCylinder`. It does not
ray march the corridor, floor, or the complete screen, so it cannot create the
old rectangular fog artifacts.

## Performance

Validation capture:

- Debug x64
- 1280 x 720
- Seven visible 48-sided cylindrical tanks
- 211 FPS observed after the six-sample and 48-sided smoothing update.
- Six volume samples per covered cylinder pixel

This is a captured observation, not a fixed hardware guarantee.

## Portfolio wording

Implemented a lightweight shape-bounded volumetric water shader in DirectX 11.
The cylinder surface is used as a ray-entry proxy, followed by six-sample
single scattering and Beer-Lambert extinction along an approximate analytic
chord. The effect preserves transparent exhibit contents while avoiding a
full-screen volumetric pass.

## Future extension

Move radius, absorption, scattering color, and density into per-instance
parameters when cylinder placement becomes data driven. The current radius
approximation is intentionally shared because all seven prototype tanks have
similar diameters.
