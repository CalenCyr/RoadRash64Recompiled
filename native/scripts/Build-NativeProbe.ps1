. (Join-Path $PSScriptRoot 'Common.ps1')
$ErrorActionPreference = 'Stop'
$root = Get-RR64Root
$native = Join-Path $root 'native'
$build = Join-Path $native 'build'
$log = New-RR64Log 'native-build.log'

Write-RR64 '============================================================' DarkGray
Write-RR64 ' Road Rash 64 Recomp - Playable Checkpoint Builder' Cyan
Write-RR64 '============================================================' DarkGray

$required = @(
    (Join-Path $root 'build\RecompiledFuncs\funcs.h'),
    (Join-Path $root 'build\RecompiledFuncs\funcs_0.c'),
    (Join-Path $root 'build\RecompiledFuncs\lookup.cpp'),
    (Join-Path $root 'build\RecompiledFuncs\recomp_overlays.inl'),
    (Join-Path $root 'build\roadrash64.us.z64'),
    (Join-Path $root 'config\roadrash64.us.audio_rsp.toml'),
    (Join-Path $root 'tools\N64Recomp\bin\RSPRecomp.exe'),
    (Join-Path $native 'lib\N64ModernRuntime\CMakeLists.txt'),
    (Join-Path $native 'lib\rt64\CMakeLists.txt'),
    (Join-Path $native 'lib\RecompFrontend\CMakeLists.txt')
)
foreach ($path in $required) {
    if (-not (Test-Path $path)) {
        throw "Missing required input: $path`nRun Setup-NativeDeps-KEEP-OPEN.cmd (and Seed Recompile if needed)."
    }
}

# v0.3.2: refuse to build stale generated C from the pre-boundary-fix seed.
# A legitimate N64Recomp output must never assign to the MIPS $zero register as
# a C lvalue. The old func_80017F54 boundary produced exactly that malformed C.
$badZeroWrites = @(Get-ChildItem -LiteralPath (Join-Path $root 'build\RecompiledFuncs') -Filter '*.c' -File |
    Select-String -Pattern '^\s*0\s*=')
if ($badZeroWrites.Count -gt 0) {
    $sample = $badZeroWrites[0]
    throw "Generated Road Rash C is stale and still contains an invalid `$zero write at $($sample.Path):$($sample.LineNumber). Install the v0.3.2 symbol fix, then run Run-SeedRecompile-KEEP-OPEN.cmd once before rebuilding the native probe."
}

# Recompile the task-type-2 audio microcode from the user's local ROM. Seed CPU
# recompilation recreates build/RecompiledFuncs, so this runs on every build.
$rspRecomp = Join-Path $root 'tools\N64Recomp\bin\RSPRecomp.exe'
$rspConfig = Join-Path $root 'config\roadrash64.us.audio_rsp.toml'
Invoke-RR64Native $rspRecomp @($rspConfig) $log $root
$rspOutput = Join-Path $root 'build\RecompiledFuncs\rr64_audio_rsp.cpp'
if (-not (Test-Path $rspOutput)) {
    throw "RSPRecomp reported success but audio output was not found: $rspOutput"
}
Write-RR64 'Road Rash audio RSP microcode recompiled locally.' Green

# Remove only failed/stale RecompFrontend shader outputs from an earlier build.
# This preserves the already-built RT64/runtime libraries and makes DXC rerun.
$shaderDir = Join-Path $build 'shaders'
if (Test-Path $shaderDir) {
    Get-ChildItem -LiteralPath $shaderDir -Filter 'Interface*.hlsl.*' -File -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue
}

$vs = Find-RR64VisualStudioWithClang
if (-not $vs) {
    throw 'Visual Studio 2022 with clang-cl was not detected. Run Install-NativePrereqs-KEEP-OPEN.cmd first.'
}
$cmake = Find-RR64CMake
Write-RR64 "Visual Studio: $vs" Green
Write-RR64 "CMake: $cmake" Green

# The pinned N64ModernRuntime checkout leaves the first-thread termination
# throw commented out. Without it, cartridge bootstrap returns into its
# intentional BREAK instruction and shuts down the host while the game thread
# is still running.
$runtimeFixPatch = Join-Path $PSScriptRoot 'Apply-N64ModernRuntime-Fixes.ps1'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runtimeFixPatch
if ($LASTEXITCODE -ne 0) { throw "N64ModernRuntime compatibility patch failed with exit code ${LASTEXITCODE}." }

# v0.3.10: patch the pinned local RT64/RecompFrontend checkout with temporary
# stage markers before compiling. This is idempotent and keeps backups beside
# the original source files.
$diagPatch = Join-Path $PSScriptRoot 'Apply-RendererDiagnostics.ps1'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $diagPatch
if ($LASTEXITCODE -ne 0) { throw "Renderer diagnostic patch failed with exit code ${LASTEXITCODE}." }

# v0.3.12: instrument the RecompFrontend RmlUi/Plume renderer constructor.
# This is the exact code reached by RT64's RenderHookInit in the normal D3D12 path.
$uiDiagPatch = Join-Path $PSScriptRoot 'Apply-UIRendererDiagnostics.ps1'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $uiDiagPatch
if ($LASTEXITCODE -ne 0) { throw "UI renderer diagnostic patch failed with exit code ${LASTEXITCODE}." }

# v0.3.13: continue tracing after the Plume renderer constructor returns,
# through RmlUi initialization, context/font setup, and frontend menus.
$uiStateDiagPatch = Join-Path $PSScriptRoot 'Apply-UIStateDiagnostics.ps1'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $uiStateDiagPatch
if ($LASTEXITCODE -ne 0) { throw "UI-state diagnostic patch failed with exit code ${LASTEXITCODE}." }

# v0.3.14: make the pinned frontend fail clearly on missing stylesheets instead
# of converting tellg() == -1 into a std::string length_error.
$assetFixPatch = Join-Path $PSScriptRoot 'Apply-FrontendAssetFixes.ps1'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $assetFixPatch
if ($LASTEXITCODE -ne 0) { throw "Frontend asset fix failed with exit code ${LASTEXITCODE}." }

New-Item -ItemType Directory -Force -Path $build | Out-Null

Invoke-RR64Native $cmake @(
    '-S',$native,
    '-B',$build,
    '-G','Visual Studio 17 2022',
    '-A','x64',
    '-T','ClangCL',
    '-DCMAKE_POLICY_DEFAULT_CMP0135=NEW'
) $log $root

Invoke-RR64Native $cmake @(
    '--build',$build,
    '--config','RelWithDebInfo',
    '--target','RoadRash64Recompiled',
    '--parallel'
) $log $root

$exe = Join-Path $build 'bin\RoadRash64Recompiled.exe'
if (-not (Test-Path $exe)) {
    throw "Build reported success but executable was not found: $exe"
}

Write-RR64 "`n[SUCCESS] Playable checkpoint built:" Green
Write-RR64 "  $exe" Green
Write-RR64 'Next: double-click Play-RoadRash64.cmd.' Cyan
Write-RR64 'Keyboard: Q accelerate, A/D steer, R or Left Shift brake, Space wheelie/hop, Enter pause/start.' Cyan
