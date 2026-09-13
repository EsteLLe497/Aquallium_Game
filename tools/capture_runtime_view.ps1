param(
    [ValidatePattern('^[1-9]$')]
    [string]$View = '8',
    [string]$Output = 'artifacts/runtime-view.png',
    [int]$WarmupSeconds = 5,
    [string]$InputScript = '',
    [string]$Executable = '',
    [switch]$SkipViewSwitch,
    [switch]$Title
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$executablePath = if ($Executable) {
    (Resolve-Path -LiteralPath $Executable).Path
} else {
    Join-Path $projectRoot 'build\Debug\AquariumLightingPrototype.exe'
}
$outputPath = if ([System.IO.Path]::IsPathRooted($Output)) {
    $Output
} else {
    Join-Path $projectRoot $Output
}

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AquariumCaptureNative
{
    public delegate bool EnumWindowsProc(IntPtr handle, IntPtr parameter);
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr handle, out Rect rect);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr handle);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr handle, IntPtr deviceContext, uint flags);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr handle, int command);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr handle, IntPtr insertAfter,
        int x, int y, int width, int height, uint flags);

    [DllImport("user32.dll")]
    public static extern void keybd_event(byte virtualKey, byte scanCode,
        uint flags, UIntPtr extraInfo);
    [DllImport("user32.dll")]
    public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extraInfo);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr handle, out uint processId);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr handle);

    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("kernel32.dll")]
    public static extern IntPtr GetConsoleWindow();

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr handle, ref System.Drawing.Point point);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);

    public static IntPtr FindLargestVisibleWindow(uint processId)
    {
        IntPtr best = IntPtr.Zero;
        long bestArea = 0;
        EnumWindows((handle, parameter) =>
        {
            uint owner;
            GetWindowThreadProcessId(handle, out owner);
            Rect rect;
            if (owner == processId && IsWindowVisible(handle) &&
                GetWindowRect(handle, out rect))
            {
                long area = Math.Max(0, rect.Right - rect.Left) *
                    (long)Math.Max(0, rect.Bottom - rect.Top);
                if (area > bestArea)
                {
                    bestArea = area;
                    best = handle;
                }
            }
            return true;
        }, IntPtr.Zero);
        return best;
    }
}
'@

if (-not $Title) { $env:AQUARIUM_START_VIEW = $View }
$process = Start-Process -FilePath $executablePath -WorkingDirectory $projectRoot -PassThru
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(12)
    do {
        Start-Sleep -Milliseconds 200
        $process.Refresh()
    } while ($process.MainWindowHandle -eq [IntPtr]::Zero -and
             -not $process.HasExited -and
             [DateTime]::UtcNow -lt $deadline)

    if ($process.HasExited -or $process.MainWindowHandle -eq [IntPtr]::Zero) {
        throw 'Aquarium window did not become available.'
    }

    Start-Sleep -Seconds $WarmupSeconds
    $process.Refresh()
    if ($process.HasExited) {
        throw "Aquarium process exited during warmup with code $($process.ExitCode)."
    }
    $windowHandle = [AquariumCaptureNative]::FindLargestVisibleWindow(
        [uint32]$process.Id)
    if ($windowHandle -eq [IntPtr]::Zero) {
        throw 'Aquarium render window did not become available.'
    }
    $previousForeground = [AquariumCaptureNative]::GetForegroundWindow()
    if ($previousForeground -ne [IntPtr]::Zero -and
        $previousForeground -ne $windowHandle) {
        [AquariumCaptureNative]::ShowWindow($previousForeground, 0) | Out-Null
    }
    [AquariumCaptureNative]::ShowWindow($windowHandle, 9) | Out-Null
    $consoleHandle = [AquariumCaptureNative]::GetConsoleWindow()
    if ($consoleHandle -ne [IntPtr]::Zero) {
        [AquariumCaptureNative]::ShowWindow($consoleHandle, 0) | Out-Null
    }
    [AquariumCaptureNative]::SetWindowPos(
        $windowHandle, [IntPtr](-1), 0, 0, 0, 0, 0x0013) | Out-Null
    [AquariumCaptureNative]::SetForegroundWindow($windowHandle) | Out-Null
    Start-Sleep -Milliseconds 750
    if (-not $SkipViewSwitch -and -not $Title) {
        $viewKey = [byte][char]$View
        [AquariumCaptureNative]::keybd_event($viewKey, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 140
        [AquariumCaptureNative]::keybd_event($viewKey, 0, 2, [UIntPtr]::Zero)
        Start-Sleep -Seconds 2
    }
    if ($InputScript) {
        $virtualKeys = @{
            W = 0x57; A = 0x41; S = 0x53; D = 0x44
            LEFT = 0x25; UP = 0x26; RIGHT = 0x27; DOWN = 0x28
            SHIFT = 0x10
            ESC = 0x1B; ENTER = 0x0D; BACKSPACE = 0x08
            '0' = 0x30; '1' = 0x31; '2' = 0x32; '3' = 0x33; '4' = 0x34
            '5' = 0x35; '6' = 0x36; '7' = 0x37; '8' = 0x38; '9' = 0x39
            LCLICK = 0x01
            F = 0x46; F2 = 0x71; F3 = 0x72; WAIT = 0
        }
        foreach ($operation in $InputScript.Split(',')) {
            $parts = $operation.Trim().Split(':')
            if ($parts.Count -eq 3 -and $parts[0] -eq 'MOVE') {
                $point = New-Object System.Drawing.Point ([int]$parts[1]), ([int]$parts[2])
                [AquariumCaptureNative]::ClientToScreen($windowHandle, [ref]$point) | Out-Null
                [AquariumCaptureNative]::SetCursorPos($point.X, $point.Y) | Out-Null
                Start-Sleep -Milliseconds 250
                continue
            }
            if ($parts.Count -ne 2 -or -not $virtualKeys.ContainsKey($parts[0])) {
                throw "Invalid input operation: $operation"
            }
            $key = [byte]$virtualKeys[$parts[0]]
            $duration = [double]::Parse(
                $parts[1], [Globalization.CultureInfo]::InvariantCulture)
            if ($key -eq 0) { Start-Sleep -Milliseconds ([int]($duration * 1000.0)); continue }
            if ($key -eq 1) { [AquariumCaptureNative]::mouse_event(2,0,0,0,[UIntPtr]::Zero) }
            else { [AquariumCaptureNative]::keybd_event($key, 0, 0, [UIntPtr]::Zero) }
            Start-Sleep -Milliseconds ([int]($duration * 1000.0))
            if ($key -eq 1) { [AquariumCaptureNative]::mouse_event(4,0,0,0,[UIntPtr]::Zero) }
            else { [AquariumCaptureNative]::keybd_event($key, 0, 2, [UIntPtr]::Zero) }
            Start-Sleep -Milliseconds 180
        }
    }

    $process.Refresh()
    if ($process.HasExited) {
        throw "Aquarium process exited after scripted input with code $($process.ExitCode)."
    }

    $rect = New-Object AquariumCaptureNative+Rect
    if (-not [AquariumCaptureNative]::GetWindowRect($windowHandle, [ref]$rect)) {
        throw 'GetWindowRect failed.'
    }

    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
        } finally {
            $graphics.Dispose()
        }
        [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($outputPath)) | Out-Null
        $bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $bitmap.Dispose()
    }
    Write-Output $outputPath
} finally {
    Remove-Item Env:AQUARIUM_START_VIEW -ErrorAction SilentlyContinue
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
    }
    if ($previousForeground -and $previousForeground -ne [IntPtr]::Zero) {
        [AquariumCaptureNative]::ShowWindow($previousForeground, 9) | Out-Null
    }
}
