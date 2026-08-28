# Stage GLB import

## Current asset

The entrance greybox is loaded from `model/map.glb`.

The checked-in asset is a valid Blender-authored glTF 2.0 binary containing
16 mesh nodes, 16 triangle primitives and one neutral material.

Builds copy it to `build/<Configuration>/model/map.glb`. Replace the source
GLB and rebuild to test a new export.

## Runtime controls

- `1`: original underwater prototype
- `2`: imported corridor plus the original visitor/glass aquarium
- `3`: imported stage authoring view facing into the greybox
- `W/A/S/D`: move in stage view
- arrow keys: look
- `Q/E`: move vertically during greybox inspection

Stage view currently uses broad authoring bounds rather than collision. These
bounds deliberately allow inspection of the entire exported map.

## Import implementation

`StageModel` uses the MIT-licensed single-file `cgltf` parser. At load time it:

1. parses and validates the GLB;
2. resolves node world transforms;
3. reads indexed triangle positions, normals and UV0;
4. converts right-handed glTF Y-up coordinates to left-handed DirectX Y-up;
5. reverses triangle winding after the handedness conversion;
6. aligns Blender floor height 0 with the prototype floor height -2.25 m;
7. combines all static primitives into one immutable vertex/index batch.

The current 16-part greybox is therefore rendered with one stage draw call.
Node transforms are baked only for the static stage; future dynamic props
should retain separate transforms or use instancing.

## Rendering integration

The imported stage has its own vertex/pixel shader and hardware depth buffer.
It writes to the same three full-resolution MRTs as the analytic prototype:

- HDR scene color;
- linear camera distance;
- motion vectors.

This lets the existing depth-aware volumetric upsample and temporal
reprojection consume imported geometry without a duplicate composition path.
The analytic room remains underneath as a fallback while the authored stage is
still a greybox. For each imported-stage pixel, the shader intersects the
camera-to-surface ray with the prototype glass plane. Imported geometry is
discarded when that ray passes through the aquarium opening, preserving the
original visitor-side water color and lighting while allowing the opening to
move correctly with the camera. A named `_Glass` proxy should replace this
temporary analytic plane when the authored map is separated by role.

## Export expectations

Use Blender glTF 2.0 Binary (`.glb`) with:

- metres as the working scale;
- rotation and scale applied;
- normals and UVs exported;
- triangle meshes;
- embedded material data;
- no Draco compression for the current loader path.

Names are not required for the current wall/floor preview. Before aquarium
volumes are authored, use these suffixes:

- `_Opaque`
- `_Glass`
- `_WaterSurface`
- `_VolumeProxy`
- `_CollisionProxy`
- `_Light_00`, `_Light_01`, and so on

## Current limitations

- only static triangle geometry is rendered;
- only the first base-color factor is used;
- textures, skins and animations are not rendered yet;
- all current nodes are combined into one material batch;
- collision and aquarium-volume proxies are not extracted yet;
- the stage is enabled only in view mode `3`.

The next production step is material batching plus name-based extraction of
collision and aquarium-volume proxy nodes.
