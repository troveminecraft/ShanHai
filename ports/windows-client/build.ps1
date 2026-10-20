param([string]$Xmake = 'xmake', [string]$BuildDir = '')
$ErrorActionPreference = 'Stop'
# This xmake version keeps caches under the process working directory even with
# -P. Use a real directory change to avoid touching the BDS project's cache.
if (!$BuildDir) { $BuildDir = Join-Path $PSScriptRoot '../../../build/windows-client' }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
New-Item -ItemType Directory -Force $BuildDir | Out-Null
Push-Location $BuildDir
try {
    & $Xmake f -P $PSScriptRoot -o $BuildDir -m release -a x64 -y
    if ($LASTEXITCODE) { throw 'Client SDK configuration failed' }
    foreach ($target in @('karma-worldgen-client')) {
        & $Xmake build -P $PSScriptRoot $target
        if ($LASTEXITCODE) { throw "Client compile/link check failed: $target" }
    }
} finally {
    Pop-Location
}
