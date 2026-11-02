# Windows x64 / MSVC / GLFW release gate. Requires an interactive desktop.
[CmdletBinding()]
param(
    [string]$BuildDir = 'temp/plot-vulkan-release',
    [string]$OutputDir = ('temp/plot-vulkan-gate-' + (Get-Date -Format 'yyyyMMdd-HHmmss')),
    [string]$Generator = 'Visual Studio 18 2026',
    [ValidateRange(1, 100)][int]$ProbeRepeats = 5
)
$ErrorActionPreference = 'Stop'
$module = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repo = [IO.Path]::GetFullPath((Join-Path $module '../..'))
function FullPath([string]$path) {
    if ([IO.Path]::IsPathRooted($path)) { return [IO.Path]::GetFullPath($path) }
    return [IO.Path]::GetFullPath((Join-Path $repo $path))
}
$build = FullPath $BuildDir
$output = FullPath $OutputDir
if (Test-Path -LiteralPath $output) { throw "OutputDir must be new and empty: $output" }
New-Item -ItemType Directory -Path $output | Out-Null
function Run([string]$log, [string]$exe, [string[]]$arguments) {
    Write-Host "Running $exe $($arguments -join ' ')"
    Get-Command $exe -ErrorAction Stop | Out-Null
    # Windows PowerShell treats native stderr as ErrorRecord, even for warnings.
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $exe @arguments 2>&1 | Out-File -Encoding utf8 (Join-Path $output $log)
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedPreference }
    if ($code -ne 0) { throw "$exe failed ($code); see $output/$log" }
}
$oldLayers = $env:VK_INSTANCE_LAYERS
$oldSync = $env:VK_LAYER_VALIDATE_SYNC
Push-Location $repo
try {
    Run 'vulkaninfo.log' 'vulkaninfo' @('--summary')
    if (-not (Select-String -LiteralPath (Join-Path $output 'vulkaninfo.log') -SimpleMatch 'VK_LAYER_KHRONOS_validation')) {
        throw 'VK_LAYER_KHRONOS_validation is required; missing validation is not a passing gate.'
    }
    Run 'configure.log' 'cmake' @('-S', $repo, '-B', $build, '-G', $Generator, '-A', 'x64',
        '-DEUI_DEPS_MODE=bundled', '-DEUI_BUILD_APPS=OFF', '-DEUI_BUILD_USER_APPS=OFF',
        '-DEUI_BUILD_TEST_FIXTURES=ON', '-DEUI_ENABLE_INSTALL=ON', '-DEUI_BUILD_SHARED=OFF',
        '-DEUI_ENABLE_MODULES=ON', '-DEUI_ENABLE_PLOT=ON', '-DEUI_INSTALL_DEMO_ASSETS=OFF',
        '-DEUI_RENDER_BACKEND=vulkan', '-DEUI_WINDOW_BACKEND=glfw')
    $units = @(Get-ChildItem -LiteralPath (Join-Path $module 'tests/unit') -Filter 'plot_*.cpp' | ForEach-Object { $_.BaseName })
    $targets = @('eui_neo_module_plot', 'eui_neo_module_serial', 'eui_shadertoy_wrap',
        'plot_vulkan_probe', 'plot_interaction_benchmark', 'gpu_image_probe') + $units
    Run 'build.log' 'cmake' (@('--build', $build, '--config', 'Release', '--parallel', '8', '--target') + $targets)
    $env:VK_INSTANCE_LAYERS = 'VK_LAYER_KHRONOS_validation'
    $env:VK_LAYER_VALIDATE_SYNC = '1'
    Run 'test-inventory.json' 'ctest' @('--test-dir', $build, '-C', 'Release', '--show-only=json-v1')
    $inventory = Get-Content -LiteralPath (Join-Path $output 'test-inventory.json') -Raw | ConvertFrom-Json
    $testNames = @($inventory.tests | ForEach-Object { $_.name })
    foreach ($required in ($units + @('plot_vulkan_probe', 'plot_interaction_benchmark', 'plot_interaction_benchmark_125k', 'gpu_image_probe'))) {
        if ($required -notin $testNames) { throw "Required test was not registered: $required" }
    }
    $filter = '^(plot_.*|gpu_image_probe)$'
    Run 'ctest.log' 'ctest' @('--test-dir', $build, '-C', 'Release', '-R', $filter, '-V', '--no-tests=error')
    Run 'probe-repeat.log' 'ctest' @('--test-dir', $build, '-C', 'Release', '-R', '^(plot_vulkan_probe|gpu_image_probe)$',
        '--repeat', "until-fail:$ProbeRepeats", '-V', '--no-tests=error')
    foreach ($log in @('ctest.log', 'probe-repeat.log')) {
        if (Select-String -LiteralPath (Join-Path $output $log) -Pattern 'Validation Error|VUID-|SYNC-HAZARD') {
            throw "Validation diagnostics found in $log"
        }
    }
    Run 'interaction-volume.log' (Join-Path $build 'Release/plot_interaction_benchmark.exe') @('--volume')
    if (Select-String -LiteralPath (Join-Path $output 'interaction-volume.log') -Pattern 'Validation Error|VUID-|SYNC-HAZARD') {
        throw 'Validation diagnostics found in interaction-volume.log'
    }
    $sdk = Join-Path $output 'sdk'
    Run 'install.log' 'cmake' @('--install', $build, '--config', 'Release', '--prefix', $sdk)
    if (Test-Path -LiteralPath (Join-Path $sdk 'share/eui-neo/assets')) { throw 'Demo assets leaked into SDK' }
    foreach ($header in @('numeric.h', 'colormaps.h', 'data_storage.h', 'gpu_scene3d_shader.h', 'software_raster.h')) {
        if (Test-Path -LiteralPath (Join-Path $sdk "include/eui-neo/modules/plot/$header")) { throw "Private header leaked: $header" }
    }
    $exports = Get-ChildItem -LiteralPath (Join-Path $sdk 'lib/cmake/EuiNeo') -Filter '*.cmake'
    $cmakeRepo = $repo.Replace('\', '/')
    foreach ($file in $exports) {
        if (Select-String -LiteralPath $file.FullName -SimpleMatch $repo, $cmakeRepo) { throw "Source path leaked into $($file.Name)" }
    }
    $consumer = Join-Path $output 'consumer'
    Run 'consumer-configure.log' 'cmake' @('-S', (Join-Path $sdk 'share/eui-neo/modules/plot/examples/plot_binary_sdk'),
        '-B', $consumer, '-G', $Generator, '-A', 'x64', "-DCMAKE_PREFIX_PATH=$sdk",
        '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF', '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF')
    Run 'consumer-build.log' 'cmake' @('--build', $consumer, '--config', 'Release', '--parallel', '8')
    Run 'consumer-test.log' 'ctest' @('--test-dir', $consumer, '-C', 'Release', '--output-on-failure', '--no-tests=error')
    $apiConsumer = Join-Path $output 'api-consumer'
    Run 'api-consumer-configure.log' 'cmake' @('-S', (Join-Path $module 'tests/consumers/find-package'),
        '-B', $apiConsumer, '-G', $Generator, '-A', 'x64', "-DCMAKE_PREFIX_PATH=$sdk",
        '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF', '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF')
    Run 'api-consumer-build.log' 'cmake' @('--build', $apiConsumer, '--config', 'Release', '--parallel', '8')
    Run 'api-consumer-test.log' 'ctest' @('--test-dir', $apiConsumer, '-C', 'Release', '--output-on-failure', '--no-tests=error')
    $records = @(Get-ChildItem -LiteralPath $sdk -Recurse -File | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path = $_.FullName.Substring($sdk.Length + 1).Replace('\', '/'); bytes = $_.Length;
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
    $records | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $output 'sdk-manifest.json')
    $head = (& git rev-parse HEAD).Trim()
    $dirty = @(& git status --porcelain)
    [ordered]@{ result = 'technical-gate-passed'; utc = [DateTime]::UtcNow.ToString('o');
        base_commit = $head; dirty_worktree = ($dirty.Count -ne 0); worktree_changes = $dirty;
        backend = 'Vulkan CPU plot upload'; platform = 'Windows x64 MSVC GLFW Release';
        validation = 'VK_LAYER_KHRONOS_validation + synchronization'; probe_repeats = $ProbeRepeats;
        consumer = 'data ran; window compiled and linked; GUI launch is a separate check';
        sales_approval = 'Not granted by this technical gate; review licensing, support matrix and performance scope'
    } | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $output 'result.json')
    Write-Host "Technical gate passed. Evidence: $output"
} finally {
    $env:VK_INSTANCE_LAYERS = $oldLayers
    $env:VK_LAYER_VALIDATE_SYNC = $oldSync
    Pop-Location
}
