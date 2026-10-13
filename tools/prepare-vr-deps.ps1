param([string]$Root = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$dependencies = @(
    @{ Name='SDL'; Url='https://github.com/libsdl-org/SDL.git'; Revision='5180c654d439ad7df5eb99481fd0e3f027db65de' },
    @{ Name='OpenXR-SDK'; Url='https://github.com/KhronosGroup/OpenXR-SDK.git'; Revision='f2448a8797c85814aa892efc1ab8707900fbcc78' },
    @{ Name='glslang'; Url='https://github.com/KhronosGroup/glslang.git'; Revision='b5782e52ee2f7b3e40bb9c80d15b47016e008bc9' }
)
foreach ($dependency in $dependencies) {
    $destination = Join-Path $Root ('build/deps/' + $dependency.Name)
    if (!(Test-Path -LiteralPath $destination)) {
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        git -C $destination init --quiet
        git -C $destination fetch --depth 1 $dependency.Url $dependency.Revision
        if ($LASTEXITCODE) { throw "Fetch failed: $($dependency.Name)" }
        git -C $destination checkout --detach FETCH_HEAD
        if ($LASTEXITCODE) { throw "Checkout failed: $($dependency.Name)" }
    }
    $actual = git -C $destination rev-parse HEAD
    if ($actual -ne $dependency.Revision) { throw "Wrong revision in $destination. Preserve your changes and use the documented revision." }
    Write-Host "$($dependency.Name): $actual"
}
