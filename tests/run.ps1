param([Parameter(Mandatory=$true)][string]$BuildDirectory,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$build = [IO.Path]::GetFullPath($BuildDirectory)
$output = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $build,$output | Out-Null
Push-Location $build
try {
    xmake f -P $PSScriptRoot -o $build -m release -a x64 -y
    if ($LASTEXITCODE) { throw 'Test configuration failed' }
    xmake build -P $PSScriptRoot
    if ($LASTEXITCODE) { throw 'Test compilation failed' }
} finally { Pop-Location }
$bin = Join-Path $build 'windows/x64/release'
$cases = @(
    @{Name='structure_stage'; Args=@()},
    @{Name='structure_plan_store'; Args=@($output)},
    @{Name='hook_transaction'; Args=@()},
    @{Name='shared_structure_lifecycle'; Args=@($output,(Join-Path $output 'lifecycle.json'))},
    @{Name='view_gap_recovery'; Args=@()},
    @{Name='bds_structure_binding'; Args=@()},
    @{Name='world_scoped_cache'; Args=@()}
)
foreach ($case in $cases) {
    $arguments = $case.Args
    & (Join-Path $bin ($case.Name+'.exe')) @arguments
    if ($LASTEXITCODE) { throw "Failed: $($case.Name)" }
}
@{passed=$true; cases=$cases.Name; gameplayTestPerformed=$false} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'host-tests.json')
