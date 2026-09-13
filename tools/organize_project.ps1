param([switch]$Cleanup)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
function Assert-ProjectPath([string]$path) {
    $full=[IO.Path]::GetFullPath($path)
    if(-not $full.StartsWith($project+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw "Outside project: $full"}
    return $full
}
if(-not $Cleanup) {
    New-Item -ItemType Directory -Force -Path "$project\asset\font","$project\asset\texture" | Out-Null
    $old=Assert-ProjectPath "$project\model"
    $new=Assert-ProjectPath "$project\asset\model"
    if(Test-Path -LiteralPath $old) {
        if(Test-Path -LiteralPath $new) {throw 'Both model locations exist; merge requires review.'}
        Move-Item -LiteralPath $old -Destination $new
    }
    Copy-Item -LiteralPath 'C:\Users\ths40319\Downloads\cinecaption2.26\cinecaption2.26\cinecaption226.ttf' -Destination "$project\asset\font\cinecaption226.ttf"
    Copy-Item -LiteralPath 'C:\Users\ths40319\Downloads\cinecaption2.26\cinecaption2.26\cinecaption2.26.txt' -Destination "$project\asset\font\license-original.txt"
    # Narrow mechanical path migration. Preserve unrelated edits and UTF-8.
    $utf8=New-Object Text.UTF8Encoding($false)
    foreach($f in Get-ChildItem -LiteralPath "$project\tools" -Filter '*.py' -File) {
        $s=[IO.File]::ReadAllText($f.FullName)
        $t=$s.Replace('ROOT / "model"','ROOT / "asset" / "model"')
        if($s -ne $t) {[IO.File]::WriteAllText($f.FullName,$t,$utf8)}
    }
    $f="$project\src\rendering\AquariumRenderer.cpp"
    $s=[IO.File]::ReadAllText($f)
    $t=$s.Replace('L"model" /','L"asset" / L"model" /')
    if($s -ne $t) {[IO.File]::WriteAllText($f,$t,$utf8)}
    foreach($f in @("$project\AquariumLightingPrototype.vcxproj","$project\AquariumLightingPrototype.vcxproj.filters")) {
        $s=[IO.File]::ReadAllText($f)
        $t=$s.Replace('Include="model\','Include="asset\model\').Replace('$(ProjectDir)model\','$(ProjectDir)asset\model\').Replace('$(OutDir)model','$(OutDir)asset\model')
        if($s -ne $t) {[IO.File]::WriteAllText($f,$t,$utf8)}
    }
    exit
}
# Only human-authored design notes and generated review media are removed.
# Runtime assets, generated collision headers/manifests, source and vendor
# licences remain. A verified zip outside the project permits recovery.
$files=@(Get-ChildItem -LiteralPath $project -File | Where-Object {($_.Extension -eq '.md' -and $_.Name -notin 'TECHNICAL_NOTES.md','AGENTS.md') -or $_.Extension -in '.png','.jpg','.pre','.cso','.log'})
foreach($dir in @('design','concepts','artifacts','.froxel-baseline','.runtime-baseline','.greybox-runtime-probe')) {
    $p=Assert-ProjectPath "$project\$dir"
    if(Test-Path -LiteralPath $p) {
        $files+=Get-ChildItem -LiteralPath $p -File -Recurse | Where-Object {$_.Extension -in '.png','.jpg','.jpeg','.webp','.mp4','.md'}
    }
}
foreach($dir in @('build\Debug','build\Release')) {
    $p=Assert-ProjectPath "$project\$dir"
    if(Test-Path -LiteralPath $p) {$files+=Get-ChildItem -LiteralPath $p -File | Where-Object {$_.Extension -in '.png','.jpg','.log'}}
}
$files=@($files | Sort-Object FullName -Unique)
if($files.Count -eq 0) {exit}
$archiveDir=Join-Path (Split-Path -Parent $project) '_archive'
New-Item -ItemType Directory -Force -Path $archiveDir | Out-Null
$zipPath=Join-Path $archiveDir ('aquarium-review-files-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.zip')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::Open($zipPath,'Create')
try {foreach($f in $files) {
    $p=Assert-ProjectPath $f.FullName
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$p,$p.Substring($project.Length+1)) | Out-Null
}} finally {$zip.Dispose()}
$verify=[IO.Compression.ZipFile]::OpenRead($zipPath)
try {if($verify.Entries.Count -ne $files.Count) {throw 'Archive count mismatch'}
    foreach($f in $files) {if($verify.GetEntry($f.FullName.Substring($project.Length+1)).Length -ne $f.Length) {throw 'Archive length mismatch'}}
} finally {$verify.Dispose()}
foreach($f in $files) {Remove-Item -LiteralPath (Assert-ProjectPath $f.FullName)}
# Remove stale project-view documentation entries, never source/build entries.
foreach($f in @("$project\AquariumLightingPrototype.vcxproj","$project\AquariumLightingPrototype.vcxproj.filters")) {
    $s=[IO.File]::ReadAllText($f)
    $t=[regex]::Replace($s,'(?ms)^\s*<None Include="(?!TECHNICAL_NOTES\.md|AGENTS\.md)[^"\r\n]+\.(?:md|png|jpg)"\s*(?:/>|>.*?</None>)','')
    [IO.File]::WriteAllText($f,$t,(New-Object Text.UTF8Encoding($false)))
}
Write-Output "Archived and removed $($files.Count) review files: $zipPath"
