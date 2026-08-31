# Aquarium Lighting Prototype — 技術記録

## プロジェクト概要

DirectX 11 / C++20 / HLSL 5.0 で、水族館の大型水槽を想定したリアルタイムライティングを試作する。生物や背景アセットに依存せず、水面・光・水中媒質だけで展示水槽らしい画を成立させることが目的。

## 現在の安定実装

- 動的 Height Field 水面
- 空気から水への Snell の法則による光軸屈折
- 水面法線と同期して動く3本のスポットライト
- 3スライス `Texture2DArray` Shadow Map
- RGB別の吸収・散乱係数
- Henyey–Greenstein 位相関数
- 低解像度ボリューム＋Motion Vector Temporal Reprojection
- Neighborhood Clamp、深度棄却、履歴Ping-Pong
- HDR合成、簡易Bloom、ACES Filmic Tone Mapping、展示水槽向けColor Grading
- Raster系とCompute系HLSLのコンパイル単位分離
- Debug Layerの警告・エラー収集

## Froxel Volumetric Lighting

### 実装済みの基盤

- 画面を8×8ピクセルのタイル、奥行きを対数分割64スライスにしたカメラ追従3Dグリッド
- `R16G16B16A16_FLOAT` 3D TextureをInjection/Integratedの2面で確保
- Compute Shaderによる媒質・光源Injection
- 列単位のFront-to-Back Integration
- 水面屈折後の光軸をFroxel Injectionへ共有
- 半解像度抽出、Temporal Reprojection、深度対応Upsampleへ接続
- NaN/Inf検出とクランプ

### 現在の扱い

対象GPU/ドライバでは3D UAV Integrationを有効にした際に最終出力が黒化する再現性のある問題が残っている。そのため `enableExperimentalFroxel = false` とし、実行時は実績のある低解像度Ray Marchへフォールバックする。黒画面のまま「完成」とは扱わない。

次の調査項目:

1. PIX/RenderDocでInjection・Integrated 3D Textureをスライスごとに確認
2. `CheckFormatSupport`でTyped UAV Store対応を実行時検証
3. 3D Textureを2D Texture Arrayへ置換した比較
4. GPU timestamp queryで各Compute Passを個別計測
5. Shadow比較サンプリングをInjectionから分離

## 検証記録

| 項目 | 結果 |
|---|---|
| Release x64 build | 0 warnings / 0 errors |
| 安定パイプライン | 1280×720で動作 |
| 観測FPS | 約72 FPS（画面キャプチャ実行時） |
| Froxel定数Injection | 正常 |
| Froxel解析的Injection | 単独動作を確認 |
| Froxel Integration | 対象環境で黒化、既定無効 |

## 2026-07-23 広域上部体積光

参考画像の「上部面光源から水中全体へ柔らかく広がる光」を、安定している半解像度Ray Marchへ追加した。

- 水面屈折後の中央ライト軸を共有する大型ソフトコーン
- Shadow Mapを追加せず、解析的な半径・Gaussian減衰で広域光を評価
- 主光源のみ強い前方散乱を使用し、既存ライトとは異方性を分離
- 深度依存の低濃度Ambient Haze
- HDR上で解析的に生成する白色コア、青色Glow、広域Haze
- 追加Render Pass・追加Texture Sampleなし

同時に次の最適化を行った。

- Volume Ray Marchを12ステップから10ステップへ削減
- 1ステップ2回だった3D Noise Sampleを1回へ削減
- 微粒子ハッシュ評価を交互ステップだけに実行
- Temporal Reprojectionで削減した空間サンプルを補完

1280×720 Release版では、画面キャプチャを行った状態で約107 FPSを観測した。計測値はGPUやキャプチャ負荷に依存するため、ポートフォリオでは計測環境と併記する。

## ポートフォリオ記載例

「DirectX 11上で水中向け体積光を設計。RGB吸収・散乱、Henyey–Greenstein位相関数、動的水面のSnell屈折、Shadow Map、Motion Vectorを用いたTemporal Reprojectionを実装した。さらに対数深度64スライスのFroxel Injection/Integration基盤を構築し、GPU互換性問題に対して安定Ray Marchへのフォールバックとシェーダーコンパイル単位の分離を行った。」

注: Froxelを完全な製品実績として記載するのは、Integrationの互換性問題を解消し、GPUキャプチャと複数環境で検証した後にする。

## 2026-08-07 水中アーチ共通ライト

水中アーチの照明を、個別シェーダーへ直書きした座標からCPU側の`AquariumLight`配列へ移行した。

- 最大4灯の位置、方向、色、強度、照射角、水面高を共通データ化
- 動的水面法線で空気から水へのSnell屈折方向をCPU側で算出
- 同じ屈折軸を水面反射、コースティクス、受光方向で共有
- 水面上の灯体位置と床上の光だまりを水深に応じて空間的に接続

アーチ専用の局所体積光も試作したが、解析円筒と実メッシュの媒体境界が一致せず画面上に不要な帯を生んだため採用しなかった。ポートフォリオでは完成技術として扱わない。

水面を`Y=+7.0m`から`Y=+5.8m`へ下げ、48×24分割へ細分化した。4帯域の頂点波とピクセル法線の細波を合成し、最大変位約0.28mとしながら入口側アーチ頂部との最小クリアランス約0.4mを維持した。構造リブは床から天井まで連続させている。

Debug x64、1300×760の画面キャプチャ時に正面約133 FPS、上向きの水面表示で約124 FPSを観測した。計測値はGPUとキャプチャ負荷に依存するため、ポートフォリオ掲載時は環境情報と併記する。

## 2026-08-11 公開映像を用いた水中アーチ再設計

『8月32日の水族館』の公開トレイラーとゲーム画面を、アセットではなく空間構成のリファレンスとして分析した。広い歩行帯、低い腰壁から立ち上がる大断面アクリル、青い市松床、暗い出口という視線誘導を、既存の生成GLBへ再構成した。

- 歩行可能幅を約3.44mから約5.44mへ拡張
- アーチ起点を1.90mから1.20mへ下げ、横半径3.42m・縦半径3.72mの楕円断面化
- 3m間隔の非発光アクリル目地と専用濃紺レール材を追加
- 腰壁を8角形の傾斜断面、天端を6角形の別トリム材として押し出し、角度依存反射を追加
- 全長シアンガイド、暖色足元灯、発光出口枠を廃止し、水槽上部からの光だけで誘導
- 市松模様を追加テクスチャなしのワールド座標シェーダーで生成
- 出口脇の仮岩を撤去し、非発光トリムと暗い開口で出口を形成
- アーチ専用局所Volumeは不採用のまま維持し、乾いた通路の可読性を優先

生成モデルは7マテリアル、11,609頂点、7,496三角形。Debug x64、約1300×760の画面キャプチャ中に約147～148 FPSを確認した。

## 2026-08-11 実在館を参考にした水中の空気感

海遊館の「夜の海遊館」における、暗い観覧側と時折差し込む月明かり風の照明、およびGeorgia Aquarium Ocean Voyagerの長い全周アクリル観覧トンネルを展示設計の参考にした。

- 高密度コースティクスではなく、大きくゆっくり移動する月光域をワールド座標で生成
- 水深と高さに応じたRGB別吸収、青色の前方散乱、上層の明度差を統合
- カメラから水殻までの視線上を解析的にサンプルし、視差を持つ浮遊微粒子を生成
- 微粒子は光の届く領域だけに出し、均一な雪・霧に見える状態を回避
- 月光の方向・色・範囲には既存の共通屈折ライト情報を再利用
- 2灯を左右交互かつ不等間隔に配置し、出口へ一直線に見える対称照明を回避
- 光源間を暗く保ち、アーチ上部から腰壁・床へ落ちる明暗の島を形成
- 粒子テクスチャ、板ポリゴン、Render Target、Draw Callは追加していない
- 水媒体とアクリルを別透明パスへ分離し、水描画後のHDRを再コピーしてガラス屈折へ入力
- ガラス正面は低反射・高透過、斜角はFresnel反射を維持し、水中照明の上書きを回避
- 既存の低解像度Volumeへアーチ専用2灯×2サンプルの軽量積分を追加
- authored GLBと同じ楕円断面でガラス交点を求め、水面Y=5.8mで積分を明示的に終了
- 水面上Y=7.4mの灯具から動的水面との交点を求め、同一点のFresnel透過ホットスポット、Snell屈折軸、体積光、床コースティクスを単一のライトデータで連動
- 48mの水中アーチへ左右交互の上部スポット3灯を配置。視線レイと屈折光軸の解析積分も試作したが、斜角で有限区間が四角い幕に見えたため製品プレビューでは不採用とし、低解像度散乱を広い楕円スポットへ調整
- 水媒体のRGB吸収と散乱を弱め、近距離は透明、下降して水深が増すほど青へ遷移する距離依存透過へ再調整
- 生成GLBへ左右の水槽底と不規則変形した低ポリ岩礁12群を追加。遠景壁も試作したが屈折境界が巨大な面として見えたため撤去し、岩礁だけを奥行き基準として採用

通常の散乱Ray Marchは2灯×2サンプルに維持し、3灯目は水面ハイライトと受光コースティクスだけへ利用する構成とした。最終版のDebug x64、約1300×760入口視点で105 FPSを確認した。

旧アーチVolumeの固定円筒境界は廃止した。新実装は現在の楕円アーチと水面に一致するレイ区間だけを積分し、乾いた床と水面上の空気へ光が漏れない。Debug x64、約1300×760で通常視点105 FPS、上向き高負荷視点101 FPSを確認した。

参考:

- https://www.kaiyukan.com/program/night/
- https://www.kaiyukan.com/about/exhibition/
- https://www.georgiaaquarium.org/gallery/ocean-voyager/

## 2026-08-12 Watatsumi-inspired 650 t hero-tank hall

- Added an isolated key-6 preview module instead of modifying routes 1-5.
- Based the spatial composition on public Shikoku Aquarium material: a 650 t
  hero tank wrapped by a ramp and observed from several heights.
- Cross-checked the official first- and second-floor maps with the architect's
  hall photograph. Rebuilt the tank as a flat-front, semi-elliptical rear plan
  and placed the public circulation on the tank's right-hand perimeter.
- Because construction drawings are not public, inferred a 9.8 x 6.1 x 14.5 m
  half-ellipse water body (about 681 m3 gross, before rock displacement) and
  recorded the values in glTF extras rather than claiming measured dimensions.
- Replaced the exposed helix with a 2.8 m-wide staged route: lower-right
  approach, wall portal, enclosed rear-perimeter rise, upper atrium re-entry,
  balcony viewing leg, and a future-room connector placeholder.
- Added dedicated HLSL material families for deep water, thick acrylic,
  displaced upper water surface, tank rock, dark hall architecture and ramp.
- Restricted lighting to the exhibit: the route-wide volumetric overlay is
  disabled in this view, while broad underwater shafts and sparse suspended
  motes are evaluated only on visible tank pixels.
- Camera height follows the same piecewise centre line authored into the GLB,
  including the concealed corridor, so first-person traversal stays attached
  to the rising floor without collision-specific hard-coded steps.
- Generated model: 7 material batches, 3,868 vertices, 2,556 triangles.
- Debug x64 validation after the plan rebuild: zero warnings / zero errors;
  the 1280 x 720 validation capture measured 88 FPS in Debug.
- Regraded the hall to a blue-black exhibit-driven lighting hierarchy: the
  water volume retains two asymmetric overhead banks, dry architecture receives
  only restrained aquarium bounce, and a single reduced upper-landing practical
  provides navigation without flattening the darkness.
- The final 1280 x 720 blue-black validation capture measured 181 FPS in Debug;
  this is a point-in-time observation rather than a guaranteed frame rate.
- Re-authored the visitor circulation from a diagrammatic two-floor brief: the
  lower portal sits to the right of the hero tank, a fully enclosed half-helix
  climbs behind the exhibit wall, and the route re-enters on the upper-left.
- Built the second floor as an open atrium gallery instead of a full slab: a
  cross-tank viewing deck joins two side platforms that continue to the rear,
  retaining the full-height first-floor view and future room connections.
- The revised GLB contains 7 material batches, 4,208 vertices and 2,726
  triangles; the 1280 x 720 layout-validation capture measured 159 FPS in Debug.
- Fixed entrance teleportation by matching the CPU traversal spline to the
  renderer's handedness-converted GLB coordinates, including the imported Z
  reflection that had placed the invisible height guide on the opposite side.
- Replaced XZ-only height snapping with current-height-aware spline selection
  and a 0.90 m/s vertical convergence limit, preventing overlapping ramp turns
  from selecting an upper floor in one frame.
- Increased the enclosed ramp clearance from 2.72 m to 3.35 m and converted
  the enclosed tunnel while retaining visitor-side clearance.
- Corrected the later wall interpretation: restored the ramp's exposed guard
  treatment and filled the approximately 4.25 m unused building void between
  the ramp outer edge and perimeter wall as solid architectural mass. This
  closes the hall edge without narrowing or walling off the visitor passage.
- The regenerated model contains 4,232 vertices and 2,738 triangles; the
  1280 x 720 void-fill validation capture measured 169 FPS in Debug.
- Replaced ramp height-only guidance with a player-radius-aware swept-corridor
  collision query. Proposed motion is constrained to the authored centre line,
  uses height to disambiguate overlapping turns and attempts axis-separated
  X/Z sliding before rejecting movement at a wall.
- Rebuilt the concealed passage as a 3.40 m-wide segmented elliptical arch:
  2.20 m vertical walls plus a 3.00 m crown rise provide 5.20 m total height
  and 3.38 m head clearance for a 1.82 m player. The hall ceiling was raised
  to 11.40 m to keep the second floor clear.
- Added sparse blue arch-crown navigation practicals, reused an arch-style
  checker pattern on all Watatsumi ramp/gallery flooring, and closed the
  first-floor left tank reveal so hidden tank/ramp construction is not visible.
- Added `tools/validate_watatsumi_traversal.py`: 2,880 static samples plus a
  1,640-step 60 Hz virtual capsule traversal verify the 64.222 m route, 2.72 m
  usable width, wall-push resolution and arrival at the upper exit.
- Final model: 7 material batches, 7,928 vertices, 4,586 triangles. The direct
  1280 x 720 tunnel capture measured 149 FPS in Debug before the final light.
- Removed the checker floor after visual review and returned the ramp/gallery
  to a restrained blue-black solid finish with only aquarium bounce.
- Replaced per-frame global nearest-point queries with stateful ramp traversal:
  entry acquisition initializes a persistent route parameter, subsequent
  frames search only its local neighbourhood, and every proposed position is
  projected inside the 1.36 m allowed centre offset. This prevents both wall
  escape and turn-to-turn snapping where the helix overlaps in plan view.
- Replaced the fixed vertical step clamp with frame-rate-independent exponential
  convergence to the continuously tracked floor height, removing uphill camera
  stair-stepping. The raised-arch 1280 x 720 capture measured 134 FPS in Debug.
- Replanned the upper gallery as an open U-shaped circulation route: removed
  the deck directly in front of the hero acrylic, retained two 4.20 m side
  arms and moved the cross-passage to the wall opposite the tank.
- Widened the concealed elliptical-arch ramp from 3.40 m to 4.20 m and updated
  the player-radius-aware collision corridor from a 1.36 m to a 1.76 m allowed
  centre offset. The resulting usable width is 3.52 m for a 0.34 m radius.
- Rebuilt the tank facade from a shared opening grid instead of overlapping
  patch boxes: a continuous 4.65 m-high wall now closes the full space above
  the acrylic, while 5.00 m-wide entrance/exit portals align with the ramp.
- Raised the lower portal clear height to 5.90 m and left the upper portal open
  from the 6.20 m landing to the raised ceiling. Static validation checks both
  openings against the 5.20 m arch crown as well as the full capsule route.
- Split water/exhibit lighting from dry-space lighting at both the CPU data and
  HLSL constant-buffer boundaries. Existing refracted aquarium banks retain
  their shadow, caustics, absorption and volumetric paths, while architecture
  consumes a dedicated `LocalLighting.hlsli` spotlight/point-light evaluator.
- Replaced global blue fill on dry architecture with inverse-square local
  lighting, smooth range falloff and inner/outer spotlight cones. Range tests
  reject non-contributing pixels early, and disabled lights are compacted on
  the CPU so the GPU loop processes only active lights (maximum eight).
- Added a dedicated `src/imgui` editor layer using Dear ImGui Win32 + DirectX11
  backends. F2 toggles the editor; position, direction, RGB color, range,
  intensity, point/spot type and cone angles are editable at runtime without
  coupling UI code to the renderer or aquarium shader path.
- Performance follow-up: changed the editor to opt-in instead of building an
  ImGui frame during normal play, skipped local-light buffer reconstruction on
  transparent water/glass passes, and enabled optimized Stage HLSL bytecode in
  Debug while retaining shader debug information. This targets tooling and
  unoptimized-loop overhead without reducing light range, color or cone quality.
- Rebalanced the binary dark/light result into a game-readable hybrid: a
  normal-oriented hemispherical ambient term preserves silhouettes, an
  analytic rectangular tank-window source supplies broad indirect blue bounce,
  and bounded exponential aerial perspective separates distant architecture.
- The hybrid adds no textures or render passes. Tank bounce evaluates the
  closest point on the authored window rectangle once per dry pixel, while the
  depth atmosphere uses the already available world/camera distance. All color,
  intensity, extent, range and fog values are exposed in the F2 lighting editor.
- Enlarged the Watatsumi hero tank to a 29.0 m-wide, 12.1 m-high flat-front
  semi-elliptical volume and kept its CPU lighting bounds, HLSL absorption,
  acrylic edge response, generated geometry and traversal collision in one
  consistent meter-scale coordinate system.
- Raised the two-storey hall and concealed ramp together instead of scaling the
  render mesh alone. The 89.272 m route validates at a 17.31% maximum grade,
  3.52 m usable width and 3.38 m player head clearance across 2,880 samples.
- Replaced rectangular portal gaps with generated elliptical portal collars:
  side shoulders and segmented curved spandrels meet the tunnel crown while
  retaining 6.16 m lower-entry clearance and a clear upper landing.
- Added a texture-free architectural material hierarchy inspired by optimized
  social-VR aquarium worlds. World-space hashed stone modules, large wall
  panels, dark ceilings and brushed trim share the existing Stage shader and
  require no new textures, samplers, lights or render passes.
- Rebalanced dark-scene visibility with low-level hemispherical ambience and
  material-aware tank/local-light response instead of a global fill light.
  This preserves silhouettes and navigation seams without flattening depth.
- Reused the existing Watatsumi ramp batch for skirting and the hero-window
  reveal frame, so the added architectural detail introduces no new material
  batch. Disabled/empty local-light rigs now exit before entering the GPU loop.
- Added zero-contribution pass elimination: Watatsumi and plain-greybox modes
  no longer execute the one-third-resolution six-step volume ray march or its
  temporal history pass when their authored volume strength is zero. The final
  composite also skips its five bilateral volume taps through a uniform branch.
- Extended the tagged capsule controller from the hero hall to the entrance,
  vestibule, jellyfish gallery and descending underwater arch. Adjacent floor
  rectangles now meet without artificial capsule-radius seams; authored wall,
  furniture, acrylic, rail and ramp tags provide the physical boundaries.
- Re-authored the Watatsumi facade as exact non-overlapping intervals. The rear
  shell, sealed service voids, portal shoulders and tank jambs now share edge
  coordinates, removing the former four-metre hidden gap and portal overlap.
- Completed the upper-floor safety envelope with rendered and collidable rear
  cross-passage rails. Portal collars reuse the existing brushed ramp material,
  adding contrast and wayfinding without another material batch or light pass.
- Added external regression validation instead of startup assertions: a grid
  reachability probe verifies both directions through Route 01-02, while exact
  rail clearance and Watatsumi seam dimensions are checked alongside the
  existing 2,880-sample helical-ramp traversal.
- Added capsule collision to the imported glass-side preview as well, including
  a tagged acrylic boundary and dry-side perimeter walls. The GLB importer now
  hashes fully baked vertices, indices and material identity to suppress exact
  duplicate mesh instances while preserving merely adjacent or coplanar parts.
- Unified generated-stage rendering and collision under the same -2.25 m world
  floor offset. Entrance, jellyfish, underwater-arch and Watatsumi eye heights,
  wall volumes, ramp points and upper-floor rails now share one coordinate
  convention, eliminating the apparent 2.25 m player hover.
- Expanded the Watatsumi concealed arch from 4.20 m to 5.40 m wide and from
  5.20 m to 6.10 m high. Entrance/exit portals grew to 6.20 m, both upper side
  walks and the rear cross-passage grew to 5.40 m, and the hall ceiling rose
  to preserve a 4.28 m head-clearance margin above the player capsule.
- Added a 0.12 m camera/body safety inset to path-wall resolution. Static and
  live wall-push probes keep the underwater-arch centre within 2.60 m and the
  widened Watatsumi ramp within 2.24 m, preventing near-plane peeks outside
  the rendered glass/architecture without adding per-frame render work.
- Rebuilt the Watatsumi upper gallery as a dimension-driven H plan: two
  complete 25.0 x 5.4 m arms joined by a centred 5.4 m cross-passage. Rails
  are emitted only along exposed perimeter segments, split at every walkway
  junction, and mirrored by named collision rails so geometry cannot pierce
  an opening or extend beyond its supporting floor.
- Reparameterized the concealed ramp height from horizontal travel distance.
  A two-metre integrated grade blend produces zero slope and exact floor
  height at both portals while holding the internal maximum grade to 15.76%,
  removing the former 0.18 m / 0.12 m landing snaps.
- Calibrated the first-person capsule to an approximately 1.70 m adult:
  1.70 m body height, a game-readable 1.65 m eye height, 0.32 m radius and
  0.32 m step height.
  Rendering, collision clearance and all preview spawn heights consume the
  same capsule values.
- Upgraded the Watatsumi tank to the arch-quality sequential transparency
  path: water first refracts the opaque tank, the HDR result is copied once,
  then thick acrylic refracts the already-filtered water. Beer-Lambert
  transmission now uses the actual rendered 10.20 m waterline instead of the
  pre-offset 12.45 m authoring coordinate.
- Re-authored three water-surface light banks from the same CPU definitions
  used by highlights, refracted axes and caustics. The full-screen volume pass
  remains disabled in this hall, retaining the prior GPU budget while adding
  only one HDR copy and no extra transparent geometry draws.
- Removed all five ellipsoid placeholder rocks from the hero tank, leaving a
  clear exhibit volume for later fish and environment authoring.
- Added a route-aware render graph fast path. Authored rooms no longer execute
  the full-screen analytic aquarium that was immediately cleared, nor render
  three unused 512 x 512 prototype shadow maps. Routes without temporal volume
  bind only the HDR color target, eliminating full-resolution depth and motion
  writes without changing the visible lighting result.
- Split the route composite from the underwater composite. Practical-light
  bloom uses two symmetric bilinear taps instead of the generic four-corner
  filter, and the zero-volume route path avoids an unnecessary depth fetch.
- Reduced thick-acrylic screen-space refraction from three scene fetches to one
  while retaining restrained chromatic dispersion analytically at grazing
  angles. Water absorption, Fresnel reflection, caustics and lighting remain
  unchanged.
- Added hysteretic dynamic resolution with discrete 100%, 90%, 82%, 76% and
  70% tiers. It targets 100 FPS, waits 0.8 seconds between decisions, ignores
  large timing spikes and requires 24% headroom before increasing quality,
  preventing resource-allocation thrash and resolution oscillation.
- Added primitive-batch AABB frustum culling with a 0.35 m displacement safety
  margin, preparing later rooms and instanced fish schools to skip completely
  off-camera geometry before issuing draw calls.
- Added reproducible route benchmarks through `AQUARIUM_START_VIEW` and exposed
  render scale plus smoothed frame time in the window diagnostics. On the same
  Debug x64 1280 x 720 capture, the Watatsumi view improved from 89 FPS to
  127 FPS at native scale; the entrance/jellyfish route measured 115 FPS and
  the underwater arch 151 FPS. A 1920 x 1080 stress capture recovered to
  105 FPS at the 70% safety tier.

References:

- https://shikoku-aquarium.jp/special/blog/archive/8/
- https://shikoku-aquarium.jp/information/
- https://www.taisei-design.jp/de/works/2020/shikokuaquarium.html
# Tagged capsule collision and NavMesh-ready traversal

- Implemented a reusable player capsule controller with sub-stepped movement
  to prevent tunnelling and axis-separated wall sliding.
- Replaced brute-force helical-ramp sampling with stateful local polyline
  projection, preventing vertically overlapping turns from being confused.
- Separated render geometry from tagged collision records (`Walkable`, `Ramp`,
  `Glass`, `Rail`, `Water`, and `Trigger`) and layer masks.
- Added tag queries to the reusable `GameObject` / `ObjectWorld` framework.
- Authored walkable and ramp surfaces so the same semantic data can later feed
  an enemy-AI NavMesh without coupling player collision to pathfinding.

# Lightweight underwater-arch surface lighting

- Preserved the open-water composition and fixed 5.80 m water surface instead
  of covering the route with additional service-ceiling geometry.
- Added 144 small low-poly bubbles as one transparent material batch. Side
  diffuser plumes widen and become denser toward the surface; a thin-film
  Fresnel/glint shader leaves the bubble interiors nearly invisible.
- Replaced the underwater arch's full-screen temporal volume ray march with
  six crossed tapered light cards. The cards now continue from the surface to
  the side water near the tank bed, outside the dry player corridor. Sharper
  longitudinal masks, animated surface breakup and view-angle response retain
  a clear long shaft without exposing a rectangular mesh boundary.
- Kept the physical water surface and acrylic render path, but avoided an
  additional depth/motion MRT, shadow map or post-process history allocation.
  The final Debug x64 1296 x 759 route capture ran at 105 FPS at 100% render
  scale while displaying the bubble and light-curtain layers.
- Strengthened the arch-only water displacement by 34% and added a capillary
  octave. Each of the six authored bubble diffusers now acts as the nearest
  radial wave source; the analytic height and gradient drive both displaced
  vertices and the water normal without CPU particle simulation.
- Added a coherent wobble to the existing single-batch bubble mesh, visually
  connecting the rising plumes to their surface disturbances. The dry route
  remains static and keeps the after-hours contrast.
- Enlarged receiver caustics from a 0.42 to 0.30 world-space frequency scale,
  widened the ridge threshold from 0.22 to 0.33, and increased floor/rock
  response. The pattern uses the refracted surface projection and a faster
  water time axis, producing broad moving bands instead of dense thin lines.
- Rejected the first per-pixel bubble-gradient caustics experiment after a
  113 FPS capture. Removing its floor-wide sqrt/exp/gradient work retained the
  linked surface motion and bold caustics; the final repeatable 1280 x 720
  route benchmark measured 144 FPS at 100% render scale.

References:

- https://www.georgiaaquarium.org/gallery/ocean-voyager/
- https://www.visitsealife.com/sydney/information/news/7-most-instagrammable-places-in-sydney-aquarium/
- https://www.visitsealife.com/birmingham/explore/aquarium-zones/ocean-tunnel/
- https://canadianpond.ca/solutions/bubble-curtains/

# Data-oriented multi-species aquarium schooling

- Added a procedural 130-triangle small-fish mesh with GPU vertex deformation:
  the head remains stable while phase-shifted amplitude grows toward the tail.
  Per-instance scale, tint, swim rate and phase create variation without extra
  models, bones or animation draws.
- Implemented fixed-30 Hz CPU Boids using a 2.4 m spatial hash. Alignment,
  cohesion and separation search only the 27 adjacent grid cells, while an
  authored looping school target controls exhibit composition and dedicated
  habitat steering prevents fish from entering glass, floor or dry walkways.
- Added a second sparse medium-fish profile without duplicating geometry. The
  species id changes proportions, tint, glint and Boids weights in the existing
  instanced shader, producing slower and less cohesive individuals at negligible
  CPU and asset cost.
- Rendered 54 small fish, 9 medium fish and 2 rays in the underwater arch; the
  Watatsumi tank uses 108 small fish, 12 medium fish and 3 rays. Per-instance
  frustum/distance rejection happens before compact dynamic-buffer uploads.
- Split the arch's small population into three 15-fish wide-route schools and
  one 9-fish entrance school, keeping the total at 54. Distinct route centres
  cover the entrance, middle and deep end without raising the simulation or
  instance budget. The overhead habitat derives its lower bound from the same
  semi-ellipse used by the acrylic canopy, keeping fish between glass and the
  water surface throughout the descending route.
- Used an analytic authored spline for the large rays instead of applying Boids
  to every species. A 27-triangle procedural ray mesh receives GPU wing
  deformation, keeping these sparse hero silhouettes deterministic and cheap.
- Added a true 22 m geometry LOD for small fish. Near silhouettes retain the
  130-triangle mesh; distant instances use a 10-triangle octahedral fish,
  reducing their triangle cost by 92%. Near, far and ray batches require at
  most three `DrawIndexedInstanced` calls for the whole habitat.
- Added a view-from-below silhouette response for overhead fish. Dark bodies
  with a restrained cyan Fresnel rim provide a fish-shadow cue without another
  draw call or shadow map. Biology is submitted before water and acrylic, so it
  participates in the established sequential absorption/refraction path.
- The arch's global exposure remains unchanged for an after-hours mood. A long
  broken emissive spine is evaluated directly on the displaced water surface,
  while stronger Beer-Lambert distance absorption and cyan in-scattering make
  the outer water volume readable without lifting the dry walkway.
- Reused the existing one-third-resolution, two-step temporal volume buffer.
  An initial all-three-light evaluation measured 126 FPS in capture and was
  rejected. A fixed coherent route split samples banks 0/1 before 32 m and
  banks 1/2 afterwards, covering the entire route without per-pixel ranking.
- Debug x64 validation reported zero warnings and zero errors. The repeatable
  1280 x 720 route benchmark retained 100% render scale at 174 FPS in the
  underwater arch, 122 FPS in the entrance route and 124 FPS in Watatsumi.
  The final full-resolution visual capture measured 146 FPS while screen
  capture and the development UI were active.
