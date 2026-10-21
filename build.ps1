param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$Python = 'python',
    [string]$Xmake = 'xmake'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
& $Python -c "import sys; assert sys.version_info[:2] == (3, 13), 'Python 3.13 required'"
if ($LASTEXITCODE) { throw 'Python 3.13 required' }
& $Python -B -X utf8 (Join-Path $root 'tools/check_palette_contract.py')
if ($LASTEXITCODE) { throw 'Palette contract check failed' }
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Use a new, empty output directory.' }
New-Item -ItemType Directory -Path $output | Out-Null
$kernel = Join-Path $output 'kernel'
New-Item -ItemType Directory -Path $kernel | Out-Null
Push-Location $kernel
try {
    & $Xmake f -P $root -o $kernel -m release -a x64 -y
    if ($LASTEXITCODE) { throw 'Kernel configuration failed' }
    & $Xmake build -P $root native-worldgen-check
    if ($LASTEXITCODE) { throw 'Kernel build failed' }
} finally { Pop-Location }
& (Join-Path $root 'ports/windows-client/build.ps1') -Xmake $Xmake -BuildDir (Join-Path $output 'client')
$pythonHome = & $Python -c "import sys;print(sys.base_prefix)"
if ($LASTEXITCODE) { throw 'Python 3.13 required' }
$node = (Get-Command node -ErrorAction Stop).Source
& $Python -B (Join-Path $root 'ports/windows-client/build_runtime.py') --output (Join-Path $output 'runtime') --python-home $pythonHome --node $node --template (Join-Path $root 'tests/fixtures/void-world/level.dat') --native-tool (Join-Path $kernel 'windows/x64/release/native-worldgen-check.exe')
if ($LASTEXITCODE) { throw 'Runtime assembly failed' }
$version = (Get-Content -LiteralPath (Join-Path $root 'ports/windows-client/manifest.json') -Raw | ConvertFrom-Json).version
& $Python -B (Join-Path $root 'ports/package_windows.py') --runtime (Join-Path $output 'runtime') --binary (Join-Path $output 'client/windows/x64/release/karma-worldgen-client.dll') --output (Join-Path $output "ShanHai-Windows-$version.zip")
if ($LASTEXITCODE) { throw 'Packaging failed' }
