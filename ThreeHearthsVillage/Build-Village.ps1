param([Parameter(Mandatory = $true)][string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$buildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$projectFile = Join-Path $PSScriptRoot 'CropoutSampleProject.uproject'
if (-not (Test-Path -LiteralPath $buildTool -PathType Leaf)) {
    throw "No UE build tool at $buildTool. Pass the UE 5.8 installation directory."
}
& $buildTool UnrealEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE -NoEngineChanges -MaxParallelActions=4
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
