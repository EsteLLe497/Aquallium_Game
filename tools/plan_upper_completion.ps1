Add-Type -AssemblyName System.Drawing
$bmp = New-Object Drawing.Bitmap 1400,900
$g = [Drawing.Graphics]::FromImage($bmp)
$g.Clear([Drawing.Color]::FromArgb(9,19,31))
$font = New-Object Drawing.Font 'Meiryo',18
$pen = New-Object Drawing.Pen ([Drawing.Color]::Cyan),12
function Label($x,$y,$s) { $g.DrawString($s,$font,[Drawing.Brushes]::White,$x,$y) }
function Room($x,$y,$w,$h,$s,$color) {
 $brush = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml($color))
 $g.FillRectangle($brush,$x,$y,$w,$h)
 $g.DrawRectangle([Drawing.Pens]::LightBlue,$x,$y,$w,$h)
 Label ($x+12) ($y+12) $s
 $brush.Dispose()
}
Label 35 20 '2F 接続整理案 V2：縮尺なし・寸法確定前'
Label 35 60 'スロープを外周の専用帯に分離 / 接続部分の高さは断面で検証する'
$ramp = New-Object Drawing.Pen ([Drawing.Color]::Goldenrod),10
$g.DrawLines($ramp,[Drawing.Point[]]@([Drawing.Point]::new(1020,600),[Drawing.Point]::new(1020,150),[Drawing.Point]::new(355,150),[Drawing.Point]::new(355,220)))
Label 330 105 '外周スロープ（1F右 → 大水槽の背後 → 左通路の奥）'
Room 300 220 110 400 '左通路' '#244354'
Room 820 220 110 400 '右通路' '#244354'
Room 410 510 410 110 '横断・鑑賞通路' '#244354'
Room 430 230 370 255 '大水槽 / 水面は上壁で隠す' '#075577'
Label 455 330 'ガラス越しに見下ろす'
Room 20 510 240 110 'イルカルーム' '#164d68'
$g.DrawLine($pen,260,565,300,565)
Label 25 645 "以前の出口位置に配置`n左通路から入室"
Room 1080 290 280 160 'サンゴ礁展示室' '#16544f'
Room 1080 510 280 110 '管理室' '#39414b'
foreach($y in @(370,565)) { $g.DrawLine($pen,930,$y,1080,$y) }
Room 480 705 300 105 '屋外テラス' '#2d5551'
$g.DrawLine($pen,630,620,630,705)
Label 35 265 "スロープ出口 →`n左通路の奥に接続"
$g.DrawLine($ramp,355,185,355,220)
$g.FillPolygon([Drawing.Brushes]::Goldenrod,[Drawing.Point[]]@([Drawing.Point]::new(345,199),[Drawing.Point]::new(365,199),[Drawing.Point]::new(355,217)))
Label 965 640 '↑ 1Fスロープ入口'
Label 840 715 "右の接続は下のスロープと`n必要な頭上高さを確保する"
Label 35 840 '青：部屋への接続　黄：スロープ / この図では通路の上下重なりを示す'
$out = Join-Path $PSScriptRoot '..\design\upper-completion-proposal-v2.png'
$bmp.Save($out,[Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose(); $font.Dispose(); $pen.Dispose(); $ramp.Dispose()
Write-Output $out
