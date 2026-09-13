param([ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [ValidateSet('render_blackout_probe','render_beach_probe','test_dialogue_expression','test_power_outage','test_audio_flow','test_opening_flow','test_screen_fade')]
    [string]$Target='render_blackout_probe')
$ErrorActionPreference='Stop'
$vc='C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207'
$sdk='C:\Program Files (x86)\Windows Kits\10'
$includes=@("/I$vc\include")
foreach($part in 'ucrt','shared','um','winrt') { $includes+="/I$sdk\Include\10.0.26100.0\$part" }
$names=switch($Target) {
    'test_audio_flow' { 'SoundEffects' }
    'test_opening_flow' { 'CollisionWorld' }
    'test_screen_fade' { }
    'test_dialogue_expression' { 'DialoguePlayer','StoryTexture','imgui','imgui_draw','imgui_tables','imgui_widgets' }
    'test_power_outage' { 'PowerOutage' }
    'render_beach_probe' { 'BeachPreviewRenderer','StageModel' }
    default { 'AquariumRenderer','JellyfishRenderer','FishRenderer','StageModel' }
}
$objects=@($names | ForEach-Object { "build\obj\$Configuration\$_.obj" })
$runtime=if($Configuration -eq 'Debug'){'/MDd'}else{'/MD'}
& "$vc\bin\Hostx64\x64\cl.exe" /nologo /std:c++20 /EHsc $runtime /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /utf-8 @includes "tools\$Target.cpp" @objects /link "/LIBPATH:$vc\lib\x64" "/LIBPATH:$sdk\Lib\10.0.26100.0\ucrt\x64" "/LIBPATH:$sdk\Lib\10.0.26100.0\um\x64" d3d11.lib dxgi.lib d3dcompiler.lib windowscodecs.lib ole32.lib user32.lib gdi32.lib winmm.lib "/OUT:build\$Configuration\$Target.exe"
exit $LASTEXITCODE
