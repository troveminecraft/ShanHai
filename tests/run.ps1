param([Parameter(Mandatory=$true)][string]$BuildDirectory,
      [Parameter(Mandatory=$true)][string]$OutputDirectory,
      [string]$FeedbackRecipe,
      [string]$MountainRecipe,
      [string]$PlateauRecipe,
      [string]$LandscapeRecipe)
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
    foreach ($test in @('test_spawn_worker.py','test_process_guard.py','test_release_versions.py')) {
        & python -B -X utf8 (Join-Path $PSScriptRoot $test)
        if ($LASTEXITCODE) { throw "Python regression failed: $test" }
    }
} finally { Pop-Location }
Push-Location $build
try {
    xmake f -P $PSScriptRoot -o $build -m release -a x64 -y
    if ($LASTEXITCODE) { throw 'Test configuration failed' }
    xmake build -P $PSScriptRoot -j 2
    if ($LASTEXITCODE) { throw 'Test compilation failed' }
} finally { Pop-Location }
$bin = Join-Path $build 'windows/x64/release'
$cases = @(
    @{Name='elevation_profile'; Args=@()},
    @{Name='elevation_variants'; Args=@((Join-Path $output 'inputs/positive.json'))},
    @{Name='world_tasks'; Args=@()},
    @{Name='terrain_session'; Args=@((Join-Path $output 'inputs/positive.json'),(Join-Path $output 'terrain-session'))},
    @{Name='kernel_facade'; Args=@((Join-Path $output 'inputs/positive.json'))},
    @{Name='new_world_files'; Args=@()},
    @{Name='raised_caverns'; Args=@()},
    @{Name='glacier_weather_policy'; Args=@()},
    @{Name='mega_cave_geometry'; Args=@()},
    @{Name='shared_small_features'; Args=@((Join-Path $output 'shared-small-features.json'))},
    @{Name='build_height'; Args=@($output)},
    @{Name='structure_height'; Args=@()},
    @{Name='vegetation_columns'; Args=@()},
    @{Name='mountain_height'; Args=@()},
    @{Name='high_mountain_underground'; Args=@((Join-Path $output 'inputs/positive.json'),(Join-Path $output 'high-mountain-underground.json'))},
    @{Name='regional_plan_concurrency'; Args=@()},
    @{Name='runtime_stability'; Args=@((Join-Path $output 'runtime-stability'))},
    @{Name='snow_loading'; Args=@((Join-Path $output 'inputs/positive.json'),(Join-Path $output 'snow-loading.json'))},
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
    $cases += @{Name='terrain_revision'; Args=@((Join-Path $output "inputs/$seed.json"),(Join-Path $output "revision-$seed.json"))}
    $cases += @{Name='badlands_regions'; Args=@((Join-Path $output "inputs/$seed.json"),(Join-Path $output "badlands-$seed.json"))}
    $cases += @{Name='completed_biomes'; Args=@((Join-Path $output "inputs/$seed.json"),(Join-Path $output "biomes-$seed.json"))}
    $cases += @{Name='regional_waterways'; Args=@((Join-Path $output "inputs/$seed.json"),(Join-Path $output "waterways-$seed.json"))}
}
if ($FeedbackRecipe) {
    $cases += @{Name='terrain_boundaries'; Args=@([IO.Path]::GetFullPath($FeedbackRecipe),(Join-Path $output 'coordinate-regressions.json'))}
}
if ($MountainRecipe) {
    $cases += @{Name='mountain_ecology'; Args=@([IO.Path]::GetFullPath($MountainRecipe),(Join-Path $output 'mountain-ecology.json'))}
}
if ($PlateauRecipe) {
    $cases += @{Name='regional_plateaus'; Args=@([IO.Path]::GetFullPath($PlateauRecipe),(Join-Path $output 'regional-plateaus.json'))}
}
if ($LandscapeRecipe) {
    $cases += @{Name='landscape_redo'; Args=@([IO.Path]::GetFullPath($LandscapeRecipe),(Join-Path $output 'landscape-redo.json'))}
}
foreach ($case in $cases) {
    $arguments = $case.Args
    Push-Location $PSScriptRoot
    try { & (Join-Path $bin ($case.Name+'.exe')) @arguments }
    finally { Pop-Location }
    if ($LASTEXITCODE) { throw "Failed: $($case.Name)" }
}
$raisedTerrain = Get-Content -LiteralPath (Join-Path $output 'raised-terrain-seed5.json') -Raw | ConvertFrom-Json
if ($raisedTerrain.lakeSamples -le 0) { throw 'Raised-terrain fixtures did not exercise a real closed lake basin' }
@{passed=$true; cases=$cases.Name; pythonCases=9; reproducibleSeeds=@('5','-1','9223372036854775807'); gameplayTestPerformed=$false} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'host-tests.json')
