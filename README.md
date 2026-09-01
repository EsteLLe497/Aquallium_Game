# Aquarium Lighting Prototype

DirectX 11だけで動作する、水族館ライティングの独立試作です。外部エンジンや外部テクスチャは使用していません。

## 実装済みの表現

- 複合波による水面法線、Fresnel反射、ハイライト
- フル解像度の動的コースティクス
- RGB別の水中吸収・散乱
- Henyey–Greenstein位相関数を使った異方性散乱
- 64³ `R8_UNORM` 3Dノイズによる水中密度の揺らぎ
- 3灯のスポットライトを12ステップで積分する半解像度Volume
- 3スライスの`Texture2DArray` Shadow Map
- 実メッシュをライト視点へ描画する動的シャドウキャスター
- 比較サンプラを使ったPCF付きボリューメトリックシャドウ
- フル解像度Motion Vector
- Motion Vector、深度棄却、Neighborhood Clampを使うTemporal Reprojection
- 深度対応アップサンプル、HDR合成、ACES風Tone Mapping
- 深い群青へ寄せた展示水槽向けColor Grading
- ハイライト限定の軽量Bloom、強い黒レベル、周辺減光
- 角丸の暗い展示窓フレームとガラス縁反射
- 複合波の高さ場として交差判定できる動的水面
- 空気から水への屈折率を使ったライト軌道の時間変化
- 水面の傾きに同期する光柱の太さ、焦点、Shadow Map

## 描画パイプライン

1. CPU側で水面の高さ・法線を評価し、空気から水へ入る3灯の屈折方向を計算
2. `VSShadow`で水面付近の3個の遮光板を、屈折後のライト視点から512² Shadow Mapへ描画
3. `PSScene`で動的水面、フル解像度HDR Color、Linear Depth、Motion VectorをMRT出力
4. `PSVolume`で半解像度Volumeを屈折軸に沿ってレイマーチし、各サンプル位置でShadow Mapを参照
5. `PSTemporal`で前フレーム履歴をMotion Vector再投影
6. `PSComposite`で深度対応アップサンプル、HDR合成、Tone Mapping

Volumeは1ピクセル12サンプルです。ビーム外ではShadow Mapを読まず、RGB Transmittanceが十分小さくなった時点で早期終了します。Shadow Mapは3灯を1個の配列テクスチャへまとめているため、既存フレームワークでもリソース管理を分離しやすい構成です。

## ビルド

1. `AquariumLightingPrototype.sln`をVisual Studio 2022で開く
2. `Debug | x64`または`Release | x64`を選ぶ
3. ビルドして実行する

HLSLはビルド後に`build/<Configuration>/shaders`へコピーされ、起動時にコンパイルされます。

## 操作

| キー | 操作 |
|---|---|
| W / A / S / D | 視点方向を基準にプレイヤーを移動する |
| Shift + WASD | ダッシュ移動する |
| マウス移動 | 視点を操作する |
| 左クリック | 視線方向へ選択要求を送る（対象は今後追加） |
| Q / E | 自由視点時に下降 / 上昇する |
| 矢印キー | 視点操作のキーボード代替 |
| F2 | ライティングエディタ表示／FPSマウス操作を切り替える |
| J / L | コースティクスを弱く / 強くする |
| K / I | 体積光を弱く / 強くする |
| U / O | 露出を下げる / 上げる |
| N / M | 異方性を下げる / 上げる |
| Space | アニメーションを一時停止する |
| R | 調整値を初期状態へ戻す |
| Esc | 終了する |

## 自作フレームワークへ移植する場所

`D3D11App.*`はWin32、Device、SwapChainを持つ試作用ホストなので移植しません。主な移植対象は次の2つです。

- `AquariumRenderer.*`
  - Shadow Map、HDR Color、Linear Depth、Motion Vector、Volume履歴の生成とパス接続
  - 定数バッファ、比較サンプラ、半解像度レンダーターゲットの構築例
- `shaders/AquariumPrototype.hlsl`
  - `VSShadow`、`PSVolume`、`PSTemporal`、`PSComposite`
  - `SampleVolumetricShadow`、RGB媒体積分、深度棄却、Neighborhood Clamp

実ゲームへ組み込むときは以下を置き換えます。

1. 水面付近のサンプル遮光板を、実シーンの不透明・Alpha Testシャドウキャスター描画へ置換
2. `PSScene`の解析シーンを、既存Deferred/ForwardパスのHDR ColorとLinear Depthへ置換
3. サンプルのMotion Vectorを、実ジオメトリのCurrent/Previous Clip Position出力へ置換
4. 3灯の固定配列を、ライト管理側が渡すViewProjection、色、強度、コーン角へ置換
5. 水面法線を、実水面メッシュまたは法線テクスチャへ置換

体積光側が必要とする入力は、Linear Depth、Motion Vector、Shadow Map配列、ライト定数、3Dノイズだけです。シーン描画とは疎結合なので、既存DirectX 11フレームワークへ段階的に移植できます。

## 品質と負荷の調整

- 最も画質を保ちやすい負荷調整は、Volume解像度を1/2から1/3へ下げること
- 次に12ステップを8～10へ下げ、Temporal履歴率を少し上げる
- Shadow Mapはライトごとに必要時だけ更新する。静的ライト・静的遮蔽物なら再利用できる
- 画面外や水槽外のライトはVolumeパスへ渡さない
- 動的解像度を使う場合も、HDR SceneとMotion Vectorはフル解像度のまま保つと破綻しにくい

この試作のShadow Map生成は説明しやすさを優先して毎フレーム3灯すべて更新します。製品実装ではライト・遮蔽物のDirty管理を入れるのが次の最適化です。
