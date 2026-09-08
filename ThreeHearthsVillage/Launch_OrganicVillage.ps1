param(
    [string]$EngineExe = 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe',
    [switch]$FreshPreview
)
$ErrorActionPreference = 'Stop'
$organicProject = Join-Path $PSScriptRoot 'CropoutSampleProject.uproject'
if (-not (Test-Path -LiteralPath $EngineExe)) { throw "Unreal Editor not found: $EngineExe" }
if (-not (Test-Path -LiteralPath $organicProject)) { throw "Project not found: $organicProject" }
$organicArguments = @($organicProject, '/Game/ThreeHearths/Maps/L_ThreeHearthsVillage', '-game', '-windowed',
    '-ResX=1440', '-ResY=900', '-HearthOrganicVillage', '-HearthDisableApi', '-HearthSimulationSpeed=1')
if ($FreshPreview) { $organicArguments += '-HearthNoWorldPersistence' }
# Interactive local preview. Normal runs use organic-world.json; FreshPreview never writes a world.
& $EngineExe @organicArguments
