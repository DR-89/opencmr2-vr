param(
    [Parameter(Mandatory=$true)][string]$Data,
    [string]$Apk = (Join-Path (Split-Path $PSScriptRoot -Parent) 'quest/app/build/outputs/apk/debug/app-debug.apk'),
    [string]$Serial,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$resolvedData = (Resolve-Path -LiteralPath $Data).Path
foreach ($required in @('Game/Bigfiles/common.bfl','FrontEnd/common.bfl','Sounds/surface.bfl')) {
    if (!(Test-Path -LiteralPath (Join-Path $resolvedData $required))) { throw "Missing original game data: $required" }
}
if (!(Test-Path -LiteralPath $Apk)) { throw "Build the Quest APK first: $Apk" }
$deviceArgs = @()
if ($Serial) { $deviceArgs = @('-s',$Serial) }
adb @deviceArgs install -r $Apk
if ($LASTEXITCODE) { throw 'APK install failed.' }
$transferArgs = @('--data', $resolvedData)
if ($Serial) { $transferArgs += @('--serial', $Serial) }
& $Python (Join-Path $PSScriptRoot 'quest-transfer-data.py') @transferArgs
if ($LASTEXITCODE) { throw 'Game data transfer failed.' }
adb @deviceArgs shell am force-stop org.opencmr2.vr
adb @deviceArgs shell am start -n org.opencmr2.vr/.VrActivity
if ($LASTEXITCODE) { throw 'Launch failed.' }
