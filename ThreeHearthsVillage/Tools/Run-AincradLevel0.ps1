param([ValidateRange(0,600)][int]$ReviewSeconds=0,[switch]$Capture,[switch]$Offscreen)
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$editor='C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
if(-not(Test-Path -LiteralPath $editor)){throw 'Set $editor to the installed UE 5.8 editor.'}
$argsList=@(('"'+(Join-Path $project 'CropoutSampleProject.uproject')+'"'),'/Game/ThreeHearths/Maps/L_AincradLevel0','-game','-windowed','-ResX=1600','-ResY=1000','-nosplash','-nop4','-HearthDisableApi','-ExecCmds="t.MaxFPS 30"')
if($Capture){$argsList+='-AincradCapture'}
if($ReviewSeconds -gt 0){$argsList+=('-AincradReviewSeconds='+$ReviewSeconds)}
$records=Join-Path $project 'Saved\ThreeHearths\AincradLevel0\Runs'
New-Item -ItemType Directory -Force -Path $records | Out-Null
$run=[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
$argsList+=('-abslog="'+(Join-Path $records ($run+'.log'))+'"')
if($Offscreen){$argsList+=@('-RenderOffscreen','-unattended','-nosound')}
$launch=@{FilePath=$editor;ArgumentList=$argsList;PassThru=$true}
if($Offscreen){$launch.WindowStyle='Hidden'}
$owned=Start-Process @launch
@{pid=$owned.Id;start_utc=$owned.StartTime.ToUniversalTime().ToString('o');seconds=$ReviewSeconds;map='L_AincradLevel0';api_enabled=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $records ($run+'.owned.json'))
Write-Output "Level0 PID=$($owned.Id); log=$run.log"
try {
    if($ReviewSeconds -gt 0) {
        if(-not $owned.WaitForExit(($ReviewSeconds+120)*1000)){throw 'Bounded review exceeded runtime'}
    } else {$owned.WaitForExit()}
    Write-Output "Level0 exit=$($owned.ExitCode)"
    exit $owned.ExitCode
} finally {
    if(-not $owned.HasExited){Stop-Process -Id $owned.Id -ErrorAction SilentlyContinue}
}
