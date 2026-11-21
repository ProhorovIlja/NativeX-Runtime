# Independent native-source build only. Does not launch a title or generated binary.
[CmdletBinding()]
param([string]$WorkRoot = "")
$ErrorActionPreference = "Stop"
if ($env:OS -ne "Windows_NT") { throw "Requires Windows x64" }
$sha = "c9a243270ceb309c15fe82b0b6b763ff29061d9e"
$origin = "https://github.com/LooperSalty/AnyXboxX.git"
if (-not $WorkRoot) {
    $root = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [IO.Path]::GetTempPath() }
    $WorkRoot = Join-Path $root ("scarlett-native-source-" + [guid]::NewGuid().ToString("N"))
}
if (Test-Path -LiteralPath $WorkRoot) { throw "Refusing to overwrite existing directory" }
New-Item -ItemType Directory -Path $WorkRoot | Out-Null
$source = Join-Path $WorkRoot "source"
$build = Join-Path $WorkRoot "build"
New-Item -ItemType Directory -Path $source | Out-Null
$env:GIT_TERMINAL_PROMPT = "0"
& git -C $source init --quiet
if ($LASTEXITCODE -ne 0) { throw "git init failed" }
& git -C $source remote add origin $origin
if ($LASTEXITCODE -ne 0) { throw "git remote failed" }
& git -C $source fetch --quiet --depth 1 origin $sha
if ($LASTEXITCODE -ne 0) { throw "Pinned public SHA fetch failed" }
& git -C $source -c advice.detachedHead=false checkout --quiet --detach FETCH_HEAD
if ($LASTEXITCODE -ne 0) { throw "Source checkout failed" }
$actual = (& git -C $source rev-parse HEAD).Trim()
if ($actual -cne $sha) { throw "Upstream SHA mismatch" }
if (-not (Test-Path (Join-Path $source "runtime/CMakeLists.txt"))) {
    throw "Expected pinned native runtime CMake source absent"
}
Write-Host "Building public source LooperSalty/AnyXboxX exact SHA $sha"
& cmake -S (Join-Path $source "runtime") -B $build -A x64
if ($LASTEXITCODE -ne 0) { throw "Native CMake configure failed" }
& cmake --build $build --config Release --parallel 2
if ($LASTEXITCODE -ne 0) { throw "Native runtime source build failed" }
$binary = Join-Path $build "bin/anyxboxx.exe"
if (-not (Test-Path -LiteralPath $binary)) { throw "Native launcher file missing" }
$names = @("xgameruntime.dll", "xmem.dll", "d3d12_xs.dll", "kernelx.dll", "anyxboxx_tls.dll")
foreach ($name in $names) {
    if (-not (Test-Path (Join-Path $build ("bin/" + $name)))) {
        throw "Missing expected runtime library: $name"
    }
}
$size = (Get-Item -LiteralPath $binary).Length
Write-Host "SOURCE_BUILD_PASS sha=$sha launcher_bytes=$size expected_dlls=5"
if ($env:GITHUB_STEP_SUMMARY) {
    @(
        "## Independent native C++ source build",
        "",
        "Pinned public upstream SHA: $sha",
        "",
        "Launcher file size: $size bytes",
        "",
        "Expected DLL files: 5 present",
        "",
        "**Build-only evidence. Neither the launcher nor a game was executed.**"
    ) | Out-File -LiteralPath $env:GITHUB_STEP_SUMMARY -Encoding utf8 -Append
}
