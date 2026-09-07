param([string]$EngineRoot='C:\UE_5.8',[switch]$LocalOnly)
$ErrorActionPreference='Stop'
$project=Join-Path $PSScriptRoot 'CropoutSampleProject.uproject'
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$python=Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$gateway=Join-Path $PSScriptRoot 'Plugins\ThreeHearths\Tools\kimi_gateway.py'
$saved=Join-Path $PSScriptRoot 'Saved\ThreeHearths'
$descriptor=Join-Path $saved 'Budget\gateway-endpoint.json'
$ownedGateway=$null
if(-not(Test-Path -LiteralPath $editor)){throw "UE editor not found: $editor"}
try {
    if(-not $LocalOnly) {
        & $python $gateway status --profile city-validation | Out-Null
        if($LASTEXITCODE -ne 0){throw 'The authorized city validation ledger is unavailable. No budget has been initialized or reset.'}
        $existing=$null
        if(Test-Path -LiteralPath $descriptor){$existing=Get-Content -LiteralPath $descriptor -Raw | ConvertFrom-Json}
        $running=$existing -and $existing.budget_profile -eq 'city-validation' -and (Get-Process -Id $existing.pid -ErrorAction SilentlyContinue)
        if(-not $running) {
            $ownedGateway=Start-Process -FilePath $python -ArgumentList @("`"$gateway`"",'serve','--profile','city-validation') -PassThru -WindowStyle Hidden
            $ready=$false
            for($attempt=0;$attempt -lt 40;$attempt++) {
                if($ownedGateway.HasExited){throw 'Kimi gateway failed to start. Check the local configuration and occupied port.'}
                if(Test-Path -LiteralPath $descriptor){$info=Get-Content -LiteralPath $descriptor -Raw | ConvertFrom-Json;if($info.pid -eq $ownedGateway.Id){$ready=$true;break}}
                Start-Sleep -Milliseconds 250
            }
            if(-not $ready){throw 'Kimi gateway did not become ready in ten seconds.'}
        }
    }
    $argsForEditor=@("`"$project`"",'-d3d11','-HearthUnpaused',"`"-HearthWorld=$saved\World\city-sample.json`"","`"-HearthHistory=$saved\city-sample-history.json`"")
    if($LocalOnly){$argsForEditor+='-HearthDisableApi'}
    # A user-invoked launcher deliberately opens the interactive editor.
    $editorProcess=Start-Process -FilePath $editor -ArgumentList $argsForEditor -PassThru
    $editorProcess.WaitForExit()
} finally {
    if($ownedGateway) {
        if(-not $ownedGateway.HasExited){Stop-Process -Id $ownedGateway.Id}
        if(Test-Path -LiteralPath $descriptor){$info=Get-Content -LiteralPath $descriptor -Raw | ConvertFrom-Json;if($info.pid -eq $ownedGateway.Id){Remove-Item -LiteralPath $descriptor}}
    }
}
