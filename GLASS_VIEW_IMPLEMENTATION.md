# Aquarium Glass View

## Purpose

The prototype now separates two optical camera paths without replacing the
existing HDR, shadow, volumetric-lighting, temporal-reprojection, or composite
passes.

- `1`: underwater camera (the original rendering path)
- `2`: visitor-side camera looking through aquarium glass

Visitor-side controls:

- `W / S`: walk forward / backward
- `A / D`: strafe left / right
- `Q / E`: move down / up for prototype inspection
- Arrow keys: look around
- `V`: toggle VSync (default is unlocked for GPU-performance measurement)

The camera is constrained to the dry corridor volume and cannot cross the
outer glass face. Current and previous visitor-camera positions are included
in the frame constants so temporal reprojection follows movement instead of
smearing the volume history.

## Glass optical model

`RefractThroughAquariumGlass` models a 16 cm prototype pane as two explicit
interfaces:

1. air to glass (`IOR 1.0 -> 1.52`)
2. glass to water (`IOR 1.52 -> 1.333`)

Each interface uses Snell refraction through HLSL `refract` and Schlick Fresnel.
The ray is displaced while travelling through the pane, which preserves the
parallax caused by glass thickness instead of treating the window as a
screen-space color filter.

The glass contribution also includes:

- RGB Beer-Lambert absorption through the pane thickness
- angle-dependent reflection from both interfaces
- low-amplitude procedural micro-normal distortion
- world-space corridor ceiling-panel reflections and edge tint

The old screen-space emitter and reflection bands were removed. Reflections
are now found by reflecting the view direction at the front glass plane,
intersecting that ray with the corridor ceiling, and testing three rectangular
fixtures in world space. They therefore slide and change angle correctly while
the visitor camera moves.

## Pipeline integration

`PSScene` uses the full two-interface optical path. `PSVolume` uses the
mathematically equivalent net air-to-water angle for parallel interfaces.
Because volume lighting is half-resolution, soft, and temporally accumulated,
the pane's small lateral displacement is not visually useful there. This
removes a second refraction, Fresnel evaluation, and exponential per volume
pixel while preserving the full model for opaque scene edges. Therefore:

- underwater absorption starts at the glass-water boundary
- volumetric lighting is integrated only inside the water
- the dry visitor area does not receive underwater fog
- glass transmission attenuates both opaque scene lighting and volume lighting

No new render target, texture, or fullscreen pass was added. The extra cost is
analytic math in the existing scene and half-resolution volume shaders.

## Refracted area lighting

The three fixed fog cones were replaced with refracted area-light samples.
Each volume step projects back to the animated water height field, evaluates
the local surface normal, refracts an air-side light sample with Snell's law,
and uses the resulting bundle direction for phase scattering and shadowing.
Water-surface curvature modulates focusing, while opaque caustics are weighted
by the same world-space light-entry regions.

The physical fixtures remain rectangular, but their underwater scattering
lobes use depth-widened elliptical Gaussian falloff measured from each
refracted axis. All three inexpensive analytic lobes are accumulated at each
volume step. This avoids both projected panel silhouettes and the Voronoi-like
vertical bands produced by nearest-light-only selection. The march remains at
six steps in the one-third-resolution volume buffer; authored shadow sampling
can be restored later when actual tank geometry is available. The animated
water normal and Snell-refraction axis are evaluated once per fixture on the
CPU each frame instead of at every volume step.

The volume/history buffers run at one-third linear resolution. Depth-aware
five-tap upsampling and motion-vector temporal reprojection retain stable
edges, while the lower pixel count provides the largest GPU-time reduction.

## Validation snapshot

- Release x64: 0 warnings, 0 errors
- Runtime HLSL compilation: passed
- Glass view, 1280 x 720, unlocked presentation: approximately 100-145 FPS
- Final stationary verification after VRC-style cleanup: approximately 136-151 FPS

These are prototype-machine observations rather than portable performance
guarantees. Production profiling should use D3D11 timestamp queries or PIX GPU
captures per pass; the window-title counter includes CPU and presentation time.

## VRC-inspired art direction pass

The final empty-tank look uses a stylized-realistic balance:

- lifted cyan/teal ambient water instead of near-black deep blue
- broad, low-frequency refracted area-light bundles
- wave-normal-warped elliptical surface irradiance instead of literal
  rectangular fixture imprints
- art-directed broad, high-energy analytic caustic ridges without increasing
  the five-layer evaluation count; floor projection scale, ridge remap and
  surface response are tuned independently
- continuous tank-floor material with the artificial square grout pattern
  removed
- restrained wall caustics with stronger floor response
- no radial vignette or spherical diagnostic particles
- four-tap PCF support retained for future authored shadow casters
- diagnostic ellipsoid shadows disabled until real inhabitants or rocks exist

This avoids showing optimization artifacts as fog detail. The lighting signal
is deliberately smooth enough for one-third-resolution temporal volume
rendering, while water-surface refraction, glass Fresnel, RGB absorption and
world-space reflections remain physically motivated.

The water-surface entry glow reuses the surface normal and the noise value
already evaluated by the water shader. It therefore softens and animates the
three fixture contributions without another texture fetch, render target, or
caustics evaluation. This keeps the surface cue connected to the refracted
volume lighting while avoiding both visible panel-shaped masks and measurable
GPU cost in the prototype.

The six volume intervals use deterministic centered samples rather than
per-pixel temporal start jitter. This trades imperceptible depth integration
bias for a substantially cleaner anime-real presentation during visitor
movement.

Switching view modes invalidates temporal history for one frame to prevent the
underwater camera history from ghosting into the visitor-side view.

## Portfolio-ready technical summary

Implemented a dual-medium aquarium camera path in DirectX 11/HLSL 5.0. The
visitor view traces an analytic ray through air-glass and glass-water
interfaces using Snell refraction, Schlick Fresnel, pane-thickness parallax,
and Beer-Lambert absorption. The refracted ray is shared by opaque scene and
half-resolution volumetric-lighting passes, while view transitions explicitly
invalidate temporal history to avoid cross-medium reprojection artifacts.

## Authored-stage integration

Added a glTF 2.0 Binary stage path using the MIT-licensed cgltf parser. Blender
node transforms are baked into a static DirectX 11 vertex/index batch at load
time, with right-handed-to-left-handed coordinate conversion, normal
transformation and winding correction. The current 16-part entrance greybox
renders in one draw call.

The stage raster pass writes HDR color, linear camera distance and motion
vectors into the same MRT layout as the analytic prototype. Imported geometry
therefore participates in hardware depth testing and feeds the existing
depth-aware volume and temporal pipeline without creating a separate
post-processing path.

Validation of the initial untextured wall/floor greybox:

- Release x64: 0 warnings, 0 errors
- runtime GLB parse and validation: passed
- runtime `Stage.hlsl` compilation: passed
- 1280 x 720 stage view, unlocked presentation: approximately 208 FPS

View mode `2` now combines the imported stage with the established visitor-side
glass view and resets to the original camera pose. A camera-ray/glass-plane
intersection mask preserves the aquarium color and volume lighting through the
viewing opening while retaining imported corridor geometry away from that
line of sight. The integrated view measures approximately 121 FPS at
1280 x 720 on the prototype machine.

The bolder caustic art-direction pass widens the procedural ridge response,
increases cyan floor irradiance and lowers its spatial frequency while keeping
the existing five-layer evaluation. A separate water-surface multiplier avoids
highlight clipping. Stable unlocked measurement after this change is
approximately 132 FPS at 1280 x 720.

## Production extension points

For integration into a mesh-based game renderer, replace the analytic front
and back glass planes with:

- front/back face depth or a thickness buffer
- a glass/stencil mask
- the real scene depth and normals
- reflection probes or SSR for corridor reflections

The water renderer can remain independent. Only the ray-entry data and medium
mask need to be supplied by the host framework.

## Cylindrical exhibit glass (2026-07-30)

The jellyfish cylinders now use a localized screen-space refraction path
instead of a flat transparent-blue overlay.

Render sequence:

1. Render opaque architecture and instanced jellyfish into the HDR scene.
2. Copy the completed HDR color target once to an SRV-only texture.
3. Render simple exhibit water.
4. Render cylindrical glass while sampling the preserved HDR scene.

The glass shader uses:

- a face-forward cylindrical interface normal
- Snell refraction with an acrylic/glass-like IOR of 1.52
- projected screen-space displacement clamped to 2.2 percent of the viewport
- three RGB samples with slightly different offsets for restrained dispersion
- Schlick Fresnel with a 0.04 normal-incidence reflectance
- angle-dependent vertical reflection bands and top/bottom edge highlights
- two low-amplitude surface-wave terms to avoid a mathematically perfect tube

The scene copy avoids the DirectX 11 read/write hazard that would occur if the
active HDR render target were sampled directly. The transparent MRT blend
continues to write color only; linear depth and motion history stay untouched.

Validation:

- Debug x64 build: 0 warnings, 0 errors
- runtime HLSL compilation and reverse-side view: passed
- 1296 x 759 reverse-side validation capture: approximately 107 FPS
- black transparent-object regression: not reproduced

The implementation costs one full-resolution HDR copy per frame and three
texture samples only on glass-covered pixels. It does not add a full-screen
ray-march or reflection pass.
