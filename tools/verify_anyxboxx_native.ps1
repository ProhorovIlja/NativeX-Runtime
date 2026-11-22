# Pinned source build, optionally execute ONLY our own source-built PE32+ test fixtures.
[CmdletBinding()]
param([string]$WorkRoot = "", [switch]$RunSynthetic)
$ErrorActionPreference = "Stop"
if ($env:OS -ne "Windows_NT") { throw "Requires Windows x64" }
$sha = "1a2e2e677c18e6ae4053eb84d4ff2395c538c108"
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

if ($RunSynthetic) {
    $fixtureSource = Join-Path (Split-Path -Parent $PSScriptRoot) "fixtures/native-pe-exec"
    $fixtureBuild = Join-Path $WorkRoot "synthetic-pe-build"
    & cmake -S $fixtureSource -B $fixtureBuild -A x64
    if ($LASTEXITCODE -ne 0) { throw "Synthetic PE CMake configure failed" }
    & cmake --build $fixtureBuild --config Release --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "Synthetic PE build failed" }

    # Ensure the external loader uses its own public-source runtime DLLs.
    $env:ANYXBOXX_SYSTEM_RUNTIME = "0"
    Remove-Item Env:ANYXBOXX_TOMBOX_DIR -ErrorAction SilentlyContinue
    Remove-Item Env:ANYXBOXX_D3D12_XS -ErrorAction SilentlyContinue
    foreach ($case in @(
        @{ Name = "scarlett_pe_baseline"; Expected = 42 },
        @{ Name = "scarlett_pe_imports"; Expected = 43 },
        @{ Name = "scarlett_pe_gdk_shim"; Expected = 44 },
        @{ Name = "scarlett_pe_d3d12"; Expected = 45 },
        @{ Name = "scarlett_pe_dxgi_present"; Expected = 46 },
        @{ Name = "scarlett_pe_crtfree_copy"; Expected = 47 }
    )) {
        $name = [string]$case.Name
        $fixture = Join-Path $fixtureBuild ("Release/" + $name + ".exe")
        if (-not (Test-Path -LiteralPath $fixture)) { throw "Missing original fixture $name" }
        $info = [Diagnostics.ProcessStartInfo]::new()
        $info.FileName = $binary
        $info.ArgumentList.Add($fixture)
        $info.WorkingDirectory = Split-Path -Parent $fixture
        $info.UseShellExecute = $false
        $child = [Diagnostics.Process]::new()
        $child.StartInfo = $info
        if (-not $child.Start()) { throw "Unable to start pinned native loader" }
        try {
            if (-not $child.WaitForExit(10000)) {
                $child.Kill($true)
                $child.WaitForExit()
                throw "Synthetic executable $name exceeded 10-second watchdog"
            }
            $status = $child.ExitCode
        } finally {
            $child.Dispose()
        }
        if ($status -ne [int]$case.Expected) {
            throw "NATIVE_PE_FAIL probe=$name expected=$($case.Expected) actual=$status"
        }
        Write-Host "NATIVE_PE_PASS probe=$name expected_exit=$($case.Expected) actual_exit=$status"
    }
}

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
        "**No retail game was executed. Original source-built PE fixtures run only when -RunSynthetic is set.**"
    ) | Out-File -LiteralPath $env:GITHUB_STEP_SUMMARY -Encoding utf8 -Append
}
