param([string]$Data)
$ErrorActionPreference = 'Stop'
# Works next to a packaged executable, in build/pcvr, or from the source tools folder.
$locations = @(
    (Join-Path $PSScriptRoot 'opencmr2.exe'),
    (Join-Path $PSScriptRoot 'src/opencmr2.exe'),
    (Join-Path $PSScriptRoot '../build/pcvr/src/opencmr2.exe'),
    (Join-Path $PSScriptRoot '../build/windows-vr/src/opencmr2.exe')
)
$binary = $locations | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (!$binary) { throw 'OpenCMR2 executable not found. Place this launcher next to opencmr2.exe or build the Windows VR target first.' }
$binary = (Resolve-Path -LiteralPath $binary).Path
$binaryFolder = Split-Path $binary -Parent
$userFolder = if ((Split-Path $binaryFolder -Leaf) -eq 'src') { Join-Path (Split-Path $binaryFolder -Parent) 'user' } else { Join-Path $binaryFolder 'user' }
$launchArguments = @('--vr', '--user-dir', $userFolder)
if ($Data) { $launchArguments += @('--data', (Resolve-Path -LiteralPath $Data).Path) }
# With no override, the application validates remembered paths, then discovers
# installed PC copies and opens the folder picker only when needed.
& $binary @launchArguments
exit $LASTEXITCODE
