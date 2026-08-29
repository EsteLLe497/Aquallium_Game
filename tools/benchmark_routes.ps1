param(
    [string]$Executable = ".\build\Debug\AquariumLightingPrototype.exe",
    [int]$Width = 1280,
    [int]$Height = 720,
    [int]$WarmupSeconds = 12
)

$ErrorActionPreference = "Stop"
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class AquariumRouteBenchmarkWindow
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr window, StringBuilder text, int count);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(
        IntPtr window, IntPtr insertAfter, int x, int y,
        int width, int height, uint flags);
}
"@

$results = foreach ($view in @("3", "4", "6"))
{
    $env:AQUARIUM_START_VIEW = $view
    $process = Start-Process `
        -FilePath $resolvedExecutable `
        -WorkingDirectory (Split-Path -Parent $resolvedExecutable) `
        -PassThru
    Remove-Item Env:\AQUARIUM_START_VIEW

    try
    {
        Start-Sleep -Seconds 3
        $process.Refresh()
        if ($process.HasExited)
        {
            throw "View $view exited during initialization."
        }
        [AquariumRouteBenchmarkWindow]::SetWindowPos(
            $process.MainWindowHandle,
            [IntPtr]::Zero,
            -20000,
            0,
            $Width,
            $Height,
            0x0010) | Out-Null
        Start-Sleep -Seconds $WarmupSeconds
        $process.Refresh()
        if ($process.HasExited)
        {
            throw "View $view exited during the benchmark."
        }

        $title = New-Object System.Text.StringBuilder 512
        [AquariumRouteBenchmarkWindow]::GetWindowText(
            $process.MainWindowHandle,
            $title,
            $title.Capacity) | Out-Null
        $text = $title.ToString()
        $match = [regex]::Match(
            $text,
            "\| (?<fps>[0-9]+) FPS .* Scale (?<scale>[0-9]+)% / (?<ms>[0-9.]+) ms")
        [pscustomobject]@{
            View = $view
            FPS = if ($match.Success) { [int]$match.Groups["fps"].Value } else { 0 }
            ScalePercent = if ($match.Success) { [int]$match.Groups["scale"].Value } else { 0 }
            SmoothedMilliseconds = if ($match.Success) { [double]$match.Groups["ms"].Value } else { 0.0 }
            Title = $text
        }
    }
    finally
    {
        if (!$process.HasExited)
        {
            Stop-Process -Id $process.Id -Force
        }
    }
}

$results | Format-Table View, FPS, ScalePercent, SmoothedMilliseconds -AutoSize
