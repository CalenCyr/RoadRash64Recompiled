. (Join-Path $PSScriptRoot 'Common.ps1')
$ErrorActionPreference = 'Stop'
Write-RR64 '============================================================' DarkGray
Write-RR64 ' Road Rash 64 Recomp - Native Build Prerequisite Check' Cyan
Write-RR64 '============================================================' DarkGray

$ok = $true
try { $git = Find-RR64Git; Write-RR64 "Git: $git" Green } catch { Write-RR64 $_.Exception.Message Red; $ok=$false }
try { $cmake = Find-RR64CMake; Write-RR64 "CMake: $cmake" Green } catch { Write-RR64 $_.Exception.Message Red; $ok=$false }
$vs = Find-RR64VisualStudioWithClang
if ($vs) {
    Write-RR64 "Visual Studio 2022 + clang-cl: $vs" Green
} else {
    Write-RR64 'Visual Studio 2022 Build Tools with C++/clang-cl: NOT DETECTED' Yellow
    Write-RR64 'Run Install-NativePrereqs-KEEP-OPEN.cmd, or use Visual Studio Installer -> Modify -> Desktop development with C++ + C++ Clang tools for Windows.' Yellow
    $ok = $false
}

$root = Get-RR64Root
$generated = Join-Path $root 'build\RecompiledFuncs\funcs_0.c'
if (Test-Path $generated) { Write-RR64 "Generated Road Rash C: $generated" Green }
else { Write-RR64 "Generated Road Rash C missing: $generated`nRun Seed Recompile first." Red; $ok=$false }

if ($ok) {
    Write-RR64 "`n[SUCCESS] Native build prerequisites are present." Green
    exit 0
}
Write-RR64 "`n[NOT READY] Install the missing native build prerequisite(s)." Yellow
exit 2
