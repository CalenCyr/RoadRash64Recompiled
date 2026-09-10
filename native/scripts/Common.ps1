Set-StrictMode -Version 2.0

function Get-RR64Root {
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

function New-RR64Log([string]$Name) {
    $root = Get-RR64Root
    $analysis = Join-Path $root 'analysis'
    New-Item -ItemType Directory -Force -Path $analysis | Out-Null
    $path = Join-Path $analysis $Name
    Set-Content -LiteralPath $path -Value ("Road Rash 64 native stage log - " + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')) -Encoding UTF8
    return $path
}

function Write-RR64([string]$Message, [ConsoleColor]$Color = [ConsoleColor]::Gray) {
    Write-Host $Message -ForegroundColor $Color
}

function Invoke-RR64Native {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$false)][AllowEmptyCollection()][string[]]$Arguments = @(),
        [Parameter(Mandatory=$true)][string]$LogPath,
        [Parameter(Mandatory=$false)][string]$WorkingDirectory = ''
    )

    $displayArgs = ($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"' + ($_ -replace '"','\"') + '"' } else { $_ }
    }) -join ' '
    $commandLine = '> "' + $FilePath + '" ' + $displayArgs
    Write-RR64 $commandLine Cyan
    Add-Content -LiteralPath $LogPath -Value $commandLine

    $oldErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        if ($WorkingDirectory) { Push-Location $WorkingDirectory }
        try {
            & $FilePath @Arguments 2>&1 | ForEach-Object {
                $line = $_.ToString()
                Write-Host $line
                Add-Content -LiteralPath $LogPath -Value $line
            }
            $code = $LASTEXITCODE
        }
        finally {
            if ($WorkingDirectory) { Pop-Location }
        }
    }
    finally {
        $ErrorActionPreference = $oldErrorAction
    }

    if ($null -eq $code) { $code = 0 }
    if ($code -ne 0) {
        throw "Native command failed with exit code ${code}: $FilePath $displayArgs"
    }
}

function Find-RR64Git {
    $cmd = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidates = @(
        'C:\Program Files\Git\cmd\git.exe',
        'C:\Program Files\Git\bin\git.exe'
    )
    foreach ($candidate in $candidates) { if (Test-Path $candidate) { return $candidate } }
    throw 'Git was not found. The N64Recomp builder previously used Git; make sure it is still installed.'
}

function Find-RR64CMake {
    $cmd = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $root = Get-RR64Root
    $candidates = @(
        (Join-Path $root 'toolchain\bin\cmake.exe'),
        'E:\PC Ports\Soul Calibur 2\RingOut Plus - PC Port\toolchain\bin\cmake.exe',
        'C:\Program Files\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    )
    foreach ($candidate in $candidates) { if (Test-Path $candidate) { return $candidate } }
    throw 'CMake was not found.'
}

function Find-VSWhere {
    $candidate = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $candidate) { return $candidate }
    return $null
}

function Find-RR64VisualStudioWithClang {
    $vswhere = Find-VSWhere
    if (-not $vswhere) { return $null }
    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath 2>$null
    if ($LASTEXITCODE -eq 0 -and $path) { return ($path | Select-Object -First 1).Trim() }
    return $null
}
