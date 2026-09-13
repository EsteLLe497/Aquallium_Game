param(
    [Parameter(Mandatory = $true)] [string]$Video,
    [Parameter(Mandatory = $true)] [string]$OutputDirectory,
    [double[]]$Times = @(0.0)
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase

[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
for ($index = 0; $index -lt $Times.Count; ++$index) {
    # Reopening for each timestamp avoids MediaPlayer returning its cached
    # compositor surface when several non-linear seeks happen in one instance.
    $player = [System.Windows.Media.MediaPlayer]::new()
    $player.ScrubbingEnabled = $true
    $player.Open([Uri]::new($Video))
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while (($player.NaturalVideoWidth -le 0 -or $player.NaturalVideoHeight -le 0) -and
           [DateTime]::UtcNow -lt $deadline) {
        $player.Play()
        Start-Sleep -Milliseconds 100
        $player.Pause()
        [System.Windows.Threading.Dispatcher]::CurrentDispatcher.Invoke(
            [Action]{},
            [System.Windows.Threading.DispatcherPriority]::Background)
    }
    if ($player.NaturalVideoWidth -le 0 -or $player.NaturalVideoHeight -le 0) {
        throw 'The video decoder did not expose a renderable frame.'
    }
    $player.Position = [TimeSpan]::FromSeconds($Times[$index])
    $player.Play()
    Start-Sleep -Milliseconds 450
    $player.Pause()
    [System.Windows.Threading.Dispatcher]::CurrentDispatcher.Invoke(
        [Action]{},
        [System.Windows.Threading.DispatcherPriority]::Render)

    $visual = [System.Windows.Media.DrawingVisual]::new()
    $context = $visual.RenderOpen()
    $context.DrawVideo($player, [Windows.Rect]::new(
        0.0, 0.0,
        [double]$player.NaturalVideoWidth,
        [double]$player.NaturalVideoHeight))
    $context.Close()
    $bitmap = [System.Windows.Media.Imaging.RenderTargetBitmap]::new(
        $player.NaturalVideoWidth,
        $player.NaturalVideoHeight,
        96.0, 96.0,
        [System.Windows.Media.PixelFormats]::Pbgra32)
    $bitmap.Render($visual)
    $encoder = [System.Windows.Media.Imaging.PngBitmapEncoder]::new()
    $encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
    $path = Join-Path $OutputDirectory ('frame_{0:D2}.png' -f $index)
    $stream = [System.IO.File]::Create($path)
    try { $encoder.Save($stream) } finally { $stream.Dispose() }
    Write-Output $path
    $player.Close()
}
