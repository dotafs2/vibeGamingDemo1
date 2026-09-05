param([Parameter(Mandatory = $true)][string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$editorFile = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$projectFile = Join-Path $PSScriptRoot 'CropoutSampleProject.uproject'
if (-not (Test-Path -LiteralPath $editorFile -PathType Leaf)) {
    throw "No UnrealEditor at $editorFile. Pass the UE 5.8 installation directory."
}
$editorArguments = '"{0}" /Game/ThreeHearths/Maps/L_ThreeHearthsVillage -ExecCmds="t.MaxFPS 30"' -f $projectFile
Start-Process -FilePath $editorFile -ArgumentList $editorArguments -WindowStyle Normal | Out-Null
