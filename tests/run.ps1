param([Parameter(Mandatory=$true)][string]$BuildDirectory,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$build = [IO.Path]::GetFullPath($BuildDirectory)
$output = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $build,$output | Out-Null
$root = Split-Path $PSScriptRoot -Parent
& python -B -X utf8 (Join-Path $root 'tools/check_palette_contract.py')
if ($LASTEXITCODE) { throw 'Palette contract check failed' }
& python -B -X utf8 (Join-Path $PSScriptRoot 'prepare_kernel_inputs.py') --output (Join-Path $output 'inputs')
if ($LASTEXITCODE) { throw 'Kernel fixture preparation failed' }
Push-Location $output
try {
    foreach ($test in @('test_spawn_worker.py','test_process_guard.py')) {
        & python -B -X utf8 (Join-Path $PSScriptRoot $test)
        if ($LASTEXITCODE) { throw "Python regression failed: $test" }
    }
} finally { Pop-Location }
Push-Location $build
try {
    xmake f -P $PSScriptRoot -o $build -m release -a x64 -y
    if ($LASTEXITCODE) { throw 'Test configuration failed' }
    xmake build -P $PSScriptRoot
    if ($LASTEXITCODE) { throw 'Test compilation failed' }
} finally { Pop-Location }
$bin = Join-Path $build 'windows/x64/release'
$cases = @(
    @{Name='build_height'; Args=@($output)},
    @{Name='structure_height'; Args=@()},
    @{Name='vegetation_columns'; Args=@()},
    @{Name='mountain_height'; Args=@()},
    @{Name='sea_level'; Args=@((Join-Path $output 'inputs/positive.json'),(Join-Path $output 'sea-level.json'))},
    @{Name='raised_terrain'; Args=@((Join-Path $output 'inputs/positive.json'),(Join-Path $output 'raised-terrain-seed5.json'))},
    @{Name='test_teleport_policy'; Args=@()},
    @{Name='palette_contract'; Args=@()},
    @{Name='creation_status'; Args=@()},
    @{Name='failure_record'; Args=@($output)},
    @{Name='kernel_bounds'; Args=@((Join-Path $output 'inputs/positive.json'))},
    @{Name='structure_prefill_workers'; Args=@((Join-Path $output 'inputs/positive.json'))},
    @{Name='structure_stage'; Args=@()},
    @{Name='structure_plan_store'; Args=@($output)},
    @{Name='hook_transaction'; Args=@()},
    @{Name='shared_structure_lifecycle'; Args=@($output,(Join-Path $output 'lifecycle.json'))},
    @{Name='view_gap_recovery'; Args=@()},
    @{Name='bds_structure_binding'; Args=@()},
    @{Name='world_scoped_cache'; Args=@()}
)
foreach ($seed in @('positive','negative','wide')) {
    $cases += @{Name='kernel_determinism'; Args=@((Join-Path $output "inputs/$seed.json"))}
    $cases += @{Name='completed_biomes'; Args=@((Join-Path $output "inputs/$seed.json"),(Join-Path $output "biomes-$seed.json"))}
    $cases += @{Name='regional_waterways'; Args=@((Join-Path $output "inputs/$seed.json"),(Join-Path $output "waterways-$seed.json"))}
}
foreach ($case in $cases) {
    $arguments = $case.Args
    & (Join-Path $bin ($case.Name+'.exe')) @arguments
    if ($LASTEXITCODE) { throw "Failed: $($case.Name)" }
}
$raisedTerrain = Get-Content -LiteralPath (Join-Path $output 'raised-terrain-seed5.json') -Raw | ConvertFrom-Json
if ($raisedTerrain.lakeSamples -le 0) { throw 'Raised-terrain fixtures did not exercise a real closed lake basin' }
@{passed=$true; cases=$cases.Name; pythonCases=8; reproducibleSeeds=@('5','-1','9223372036854775807'); gameplayTestPerformed=$false} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'host-tests.json')
