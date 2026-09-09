param(
    [ValidateSet('play','validate','capture','reference-capture','character-capture','market-demo','market-demo-capture','tolbana-demo','tolbana-demo-capture')][string]$Mode = 'market-demo',
    [string]$GodotPath = '',
    [ValidatePattern('^[a-zA-Z0-9_-]{1,40}$')][string]$Revision = 'manual'
)
$ErrorActionPreference = 'Stop'
if (-not $GodotPath) {
    $candidate = Get-Command godot -ErrorAction SilentlyContinue
    if ($candidate) { $GodotPath = $candidate.Source }
    else {
        $GodotPath = Join-Path $env:USERPROFILE '.cache/level0-tools/godot-4.7.2-mono/Godot_v4.7.2-stable_mono_win64/Godot_v4.7.2-stable_mono_win64_console.exe'
    }
}
if (-not (Test-Path -LiteralPath $GodotPath -PathType Leaf)) {
    throw 'Godot was not found. Import project.godot in Godot 4.7.2, or supply -GodotPath with your executable path.'
}
$engineArgs = @('--path', $PSScriptRoot)
if ($Mode -eq 'validate') { $engineArgs += @('--headless','--quit-after','1200','--','--validate') }
elseif ($Mode -eq 'capture') { $engineArgs += @('--quit-after','900','--','--capture') }
elseif ($Mode -eq 'reference-capture') { $engineArgs += @('--quit-after','1800','--','--reference-capture') }
elseif ($Mode -eq 'character-capture') { $engineArgs += @('--quit-after','1800','--','--character-capture') }
elseif ($Mode -eq 'market-demo') { $engineArgs += @('--script','res://Art/ReferenceScenes/run_market_craft_v5.gd') }
elseif ($Mode -eq 'market-demo-capture') { $engineArgs += @('--audio-driver','Dummy','--script','res://Art/ReferenceScenes/capture_market_craft_v5.gd','--',"--revision=$Revision") }
elseif ($Mode -eq 'tolbana-demo') { $engineArgs += @('--script','res://Art/ReferenceScenes/run_market_craft_v5.gd','--','--tolbana') }
elseif ($Mode -eq 'tolbana-demo-capture') { $engineArgs += @('--audio-driver','Dummy','--script','res://Art/ReferenceScenes/capture_tolbana_craft_v5.gd','--',"--revision=$Revision") }
& $GodotPath @engineArgs
exit $LASTEXITCODE
