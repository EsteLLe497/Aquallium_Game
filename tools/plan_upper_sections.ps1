Add-Type -AssemblyName System.Drawing
$bmp = New-Object Drawing.Bitmap 1700,1100
$g = [Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$g.Clear([Drawing.ColorTranslator]::FromHtml('#09131f'))
$font = New-Object Drawing.Font 'Meiryo',15
$small = New-Object Drawing.Font 'Meiryo',12
function Txt($x,$y,$s) { $g.DrawString($s,$font,[Drawing.Brushes]::White,$x,$y) }
function Note($x,$y,$s) { $g.DrawString($s.Replace('\n',"`n"),$small,[Drawing.Brushes]::LightBlue,$x,$y) }
function Rect($x,$y,$w,$h,$color) {
 $b = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml($color))
 $g.FillRectangle($b,[single]$x,[single]$y,[single]$w,[single]$h)
 $g.DrawRectangle([Drawing.Pens]::LightSlateGray,[single]$x,[single]$y,[single]$w,[single]$h)
 $b.Dispose()
}
# Plan coordinates are runtime X/Z, heights relative to the 1F floor.
function PX($x) { return 455 + 14*$x }
function PZ($z) { return 650 - 14*$z }
function PlanRect($x0,$z0,$x1,$z1,$color) { Rect (PX $x0) (PZ $z1) (14*($x1-$x0)) (14*($z1-$z0)) $color }
Txt 35 20 '2F 接続設計 V3 — 平面と断面でスロープの干渉を解消'
Note 35 56 '提案寸法 / 単位 m / 高さは1F床=0 / ゲーム本体は未変更 / 建築構造の実施設計ではありません'
Txt 35 108 '平面：1Fスロープと2Fを重ねて表示（同一縮尺）'
PlanRect -14.25 0 -9.45 18 '#244354'
PlanRect 9.45 0 14.25 18 '#244354'
PlanRect -9.45 0.9 9.45 7.35 '#244354'
PlanRect -8.5 8 8.5 16 '#075577'
Txt 355 451 '大水槽 17 × 8'
Note 349 485 '水面 +10.20 / 上壁で隠す'
PlanRect -24.25 0 -14.25 10 '#164d68'
Note 122 530 "イルカルーム\n10 × 10"
PlanRect 21 1 31 8 '#39414b'
Note 760 569 "管理室\n10 × 7"
PlanRect 21 8.3 31 18.3 '#16544f'
Note 759 436 "サンゴ礁展示室\n10 × 10"
PlanRect -5 -9 5 0 '#2d5551'
PlanRect -1.5 0 1.5 0.9 '#267e87'
PlanRect -14.4 4 -14.1 7 '#267e87'
Note 405 701 "テラス\n10 × 9"
# Only bridges overlap the ramp in plan. Their entire footprint is over its
# level section, including a 0.50 m buffer before the rising section.
PlanRect 14.1 4 21.15 7 '#267e87'
PlanRect 14.1 9 21.15 12 '#267e87'
$rampPoints = New-Object 'System.Collections.Generic.List[System.Drawing.PointF]'
$rampPoints.Add([Drawing.PointF]::new((PX 18),(PZ 3)))
$rampPoints.Add([Drawing.PointF]::new((PX 18),(PZ 24)))
for($i=1;$i -le 24;$i++) { $a=$i*[Math]::PI/48; $rampPoints.Add([Drawing.PointF]::new((PX (15+3*[Math]::Cos($a))),(PZ (24+3*[Math]::Sin($a))))) }
$rampPoints.Add([Drawing.PointF]::new((PX -8.85),(PZ 27)))
for($i=1;$i -le 24;$i++) { $a=[Math]::PI/2+$i*[Math]::PI/48; $rampPoints.Add([Drawing.PointF]::new((PX (-8.85+3*[Math]::Cos($a))),(PZ (24+3*[Math]::Sin($a))))) }
$rampPoints.Add([Drawing.PointF]::new((PX -11.85),(PZ 18)))
$shellPen=New-Object Drawing.Pen ([Drawing.Color]::FromArgb(105,91,60)),64.4
$pathPen=New-Object Drawing.Pen ([Drawing.Color]::Goldenrod),3
$g.DrawLines($shellPen,$rampPoints.ToArray())
$g.DrawLines($pathPen,$rampPoints.ToArray())
# Redraw bridge overlays so their upper-storey crossing reads explicitly.
PlanRect 14.1 4 21.15 7 '#267e87'
PlanRect 14.1 9 21.15 12 '#267e87'
Note 610 491 'B'
Note 610 561 'A'
Note 320 166 '奥側で上昇：中心線曲率半径3.0 / 内側半径0.9'
Note 202 365 '2F出口 +5.50 →'
Note 681 654 '1F入口 +0.00'
Note 686 358 'Z=12.50から上昇'
Note 322 803 '青緑：2F接続通路 / 黄：下階スロープ'
Note 35 850 "右通路外壁 X=14.25\nスロープ外形 X=15.70～20.30\n右室の内側壁 X=21.00\n→ 通路外壁との隙間1.45、部屋との隙間0.70"
Note 485 850 "2F通路幅4.80 / 接続通路幅3.00\nイルカルーム入口：左手前 Z=4～7\nテラス入口：手前中央 X=-1.5～1.5\n右室の寸法には展示水槽を含む"
# Section A/B share a flat lower ramp, avoiding uncertain height stacking.
Txt 1000 112 '断面 A・B：右側2室への接続'
Note 1000 149 'A Z=4～7 / B Z=9～12：どちらも同じ断面'
Rect 1030 265 580 22 '#267e87'
Txt 1035 218 '2F床上 +5.50 / 床厚0.25'
Note 1040 297 '床下面 +5.25'
# Vertical scale: 65 px/m, y=650 at datum.
$arch=New-Object Drawing.Pen ([Drawing.Color]::Goldenrod),13
$g.DrawArc($arch,1130,390,330,180,180,180)
$g.DrawLine($arch,1130,480,1130,650)
$g.DrawLine($arch,1460,480,1460,650)
Rect 1120 650 350 12 '#756046'
Txt 1180 530 'スロープ内'
Note 1150 575 '有効幅4.20 / 高さ4.00'
Note 1035 675 'スロープ床 +0.00 / アーチ天井外側 +4.20'
Txt 1050 335 '離隔：5.25 − 4.20 = 1.05 m'
Note 1000 739 "スロープ外形幅4.60（側壁各0.20）\n天井厚0.20、接続床厚0.25を計上\n右室は専用帯の外側：水槽も帯内へ突出させない"
$riseLength = 11.5 + 3*[Math]::PI + 23.85 + 6
$grade=5.5/$riseLength*100
Txt 1000 867 ('上昇区間 約{0:F2} m / 勾配 約{1:F2}%' -f $riseLength,$grade)
Note 1000 910 "角のアーチ・床下面・出口開口はメッシュで再検証\n1F入口の短い接続部は既存フロアに合わせて調整\n未接続室へ向かう穴を残さず、壁を開口単位で分割"
Txt 35 1008 '確認点：左奥出口 / 左手前イルカ / 右奥サンゴ・右手前管理 / 手前中央テラス'
$out=Join-Path $PSScriptRoot '..\design\upper-completion-sections-v3.png'
$bmp.Save($out,[Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$bmp.Dispose();$font.Dispose();$small.Dispose();$pathPen.Dispose();$shellPen.Dispose();$arch.Dispose()
Write-Output $out
