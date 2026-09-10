. (Join-Path $PSScriptRoot 'Common.ps1')
$ErrorActionPreference = 'Stop'

Write-RR64 '============================================================' DarkGray
Write-RR64 ' Road Rash 64 Recomp - Native Windows Build Tools Installer' Cyan
Write-RR64 '============================================================' DarkGray

$existing = Find-RR64VisualStudioWithClang
if ($existing) {
    Write-RR64 "Already ready: $existing" Green
    exit 0
}

$components = @(
    'Microsoft.VisualStudio.Workload.VCTools',
    'Microsoft.VisualStudio.Component.VC.Llvm.Clang',
    'Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset'
)

function Test-RR64VSReady {
    $detected = Find-RR64VisualStudioWithClang
    if ($detected) {
        Write-RR64 "[SUCCESS] Visual Studio clang-cl toolset detected: $detected" Green
        return $true
    }
    return $false
}

function Invoke-RR64VSBootstrapper {
    $bootstrapper = Join-Path $env:TEMP 'rr64_vs_buildtools_2022.exe'
    $url = 'https://aka.ms/vs/17/release/vs_buildtools.exe'

    Write-RR64 '' Gray
    Write-RR64 'Using Microsoft''s official Visual Studio 2022 Build Tools bootstrapper...' Cyan
    Write-RR64 "Download: $url" DarkGray

    try {
        if (Test-Path $bootstrapper) {
            Remove-Item -LiteralPath $bootstrapper -Force -ErrorAction SilentlyContinue
        }

        # Invoke-WebRequest is available in Windows PowerShell 5.1 and does not
        # depend on WinGet/App Installer being healthy.
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $bootstrapper
    }
    catch {
        Write-RR64 "Failed to download the Microsoft bootstrapper: $($_.Exception.Message)" Red
        return 20
    }

    if (-not (Test-Path $bootstrapper)) {
        Write-RR64 'The Visual Studio Build Tools bootstrapper was not downloaded.' Red
        return 21
    }

    $args = @(
        '--passive',
        '--wait',
        '--norestart',
        '--nocache',
        '--add', 'Microsoft.VisualStudio.Workload.VCTools',
        '--includeRecommended',
        '--add', 'Microsoft.VisualStudio.Component.VC.Llvm.Clang',
        '--add', 'Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset'
    )

    Write-RR64 'Launching the official Microsoft installer. A UAC prompt may appear.' Yellow
    Write-RR64 'Installing: Desktop C++ Build Tools + MSVC + Windows SDK + clang-cl.' Yellow

    $p = Start-Process -FilePath $bootstrapper -ArgumentList $args -Wait -PassThru
    $code = $p.ExitCode

    # Visual Studio setup uses 3010 for success with reboot required.
    if ($code -ne 0 -and $code -ne 3010) {
        Write-RR64 "Visual Studio Build Tools installer returned exit code $code." Red
        return $code
    }

    if ($code -eq 3010) {
        Write-RR64 'Build Tools installed successfully; Windows reports that a reboot is recommended.' Yellow
    }

    if (Test-RR64VSReady) { return 0 }

    Write-RR64 'Installation completed, but clang-cl has not appeared yet.' Yellow
    Write-RR64 'If Visual Studio Installer is still finishing work, let it complete and rerun Check-NativePrereqs.cmd.' Yellow
    return 3
}

$winget = Get-Command winget.exe -ErrorAction SilentlyContinue
if ($winget) {
    Write-RR64 'Trying WinGet first...' Cyan
    Write-RR64 'This uses the official Microsoft.VisualStudio.2022.BuildTools package.' DarkGray

    # IMPORTANT: invoke winget directly rather than Start-Process. Windows
    # PowerShell 5.1's Start-Process -ArgumentList flattens the array and can
    # split the --override payload at spaces, producing WinGet 0x8A150002.
    $override = '--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset'
    $wingetArgs = @(
        'install',
        '--id', 'Microsoft.VisualStudio.2022.BuildTools',
        '--exact',
        '--source', 'winget',
        '--accept-source-agreements',
        '--accept-package-agreements',
        '--override', $override
    )

    $oldErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $winget.Source @wingetArgs
        $wingetCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $oldErrorAction
    }

    if ($null -eq $wingetCode) { $wingetCode = 0 }

    if ($wingetCode -eq 0) {
        if (Test-RR64VSReady) { exit 0 }
        Write-RR64 'WinGet completed, but clang-cl is not detected yet. Falling back to the Microsoft bootstrapper.' Yellow
    }
    else {
        $unsigned = [uint32]($wingetCode -band 0xFFFFFFFFL)
        $hex = ('0x{0:X8}' -f $unsigned)
        Write-RR64 "WinGet returned $wingetCode ($hex). Falling back to the Microsoft bootstrapper." Yellow
        if ($hex -eq '0x8A150002') {
            Write-RR64 '0x8A150002 means WinGet rejected its command-line arguments; this is no longer fatal.' DarkYellow
        }
    }
}
else {
    Write-RR64 'WinGet is not available. That is okay; using the Microsoft bootstrapper directly.' Yellow
}

$result = Invoke-RR64VSBootstrapper
if ($result -eq 0) {
    Write-RR64 '' Gray
    Write-RR64 '[SUCCESS] Native Windows build prerequisites are installed.' Green
    Write-RR64 'Next: run Check-NativePrereqs.cmd, then Build-NativeProbe-KEEP-OPEN.cmd.' Cyan
    exit 0
}

Write-RR64 '' Gray
Write-RR64 '[NOT READY] Automatic Build Tools installation did not complete.' Red
Write-RR64 'Open Visual Studio Installer -> Build Tools 2022 -> Modify, then select:' Yellow
Write-RR64 '  Desktop development with C++' Yellow
Write-RR64 '  C++ Clang tools for Windows' Yellow
Write-RR64 '  Windows 11 SDK' Yellow
exit $result
