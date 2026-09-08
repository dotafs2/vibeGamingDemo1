param(
    [string]$EngineExe = 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe',
    [ValidateRange(0,3600)][int]$ReviewDurationSeconds = 0,
    [switch]$RenderOffscreen
)
$ErrorActionPreference = 'Stop'
$medievalProject = Join-Path $PSScriptRoot 'CropoutSampleProject.uproject'
$seedDirectory = Join-Path $PSScriptRoot 'Content\ThreeHearths\Data\MedievalShowcase'
$saveDirectory = Join-Path $PSScriptRoot 'Saved\ThreeHearths\MedievalShowcase'
if (-not (Test-Path -LiteralPath $EngineExe)) { throw "Unreal Editor not found: $EngineExe" }
if ($ReviewDurationSeconds -gt 0 -and $ReviewDurationSeconds -lt 30) { throw 'A timed review must last at least 30 seconds.' }
New-Item -ItemType Directory -Path $saveDirectory -Force | Out-Null
$worldFile = Join-Path $saveDirectory 'world.json'
$historyFile = Join-Path $saveDirectory 'history.json'
# Copy the reviewed checkpoint only on the first launch. Existing progress,
# the normal organic world, and its backup are never replaced by this launcher.
if (-not (Test-Path -LiteralPath $worldFile)) {
    if (Test-Path -LiteralPath ($worldFile + '.bak')) { throw 'A showcase backup exists; restore or inspect it before creating a new world.' }
    Copy-Item -LiteralPath (Join-Path $seedDirectory 'history.json') -Destination $historyFile
    Copy-Item -LiteralPath (Join-Path $seedDirectory 'world.json') -Destination $worldFile
}
$medievalArguments = @(
    ('"' + $medievalProject + '"'), '/Game/ThreeHearths/Maps/L_ThreeHearthsVillage',
    '-game', '-windowed', '-ResX=1440', '-ResY=900',
    '-HearthOrganicVillage', '-HearthDisableApi', '-HearthUnpaused', '-HearthSimulationSpeed=1',
    ('-HearthWorld="' + $worldFile + '"'), ('-HearthHistory="' + $historyFile + '"'),
    ('-abslog="' + (Join-Path $saveDirectory 'preview.log') + '"')
)
if ($ReviewDurationSeconds -gt 0) { $medievalArguments += "-HearthReviewDurationSeconds=$ReviewDurationSeconds" }
if ($RenderOffscreen) {
    $medievalArguments += @('-RenderOffscreen','-unattended','-ExecCmds="t.MaxFPS 30"')
    $previewProcess = Start-Process -FilePath $EngineExe -ArgumentList $medievalArguments -WindowStyle Hidden -PassThru
    @{pid=$previewProcess.Id; started_utc=$previewProcess.StartTime.ToUniversalTime().ToString('o'); executable=$EngineExe; purpose='Bounded offline medieval showcase acceptance'} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $saveDirectory 'owned-preview.json')
    $previewProcess.WaitForExit()
    exit $previewProcess.ExitCode
}
# This is the interactive launcher the user opens to explore the village.
Start-Process -FilePath $EngineExe -ArgumentList $medievalArguments -WindowStyle Normal -Wait
