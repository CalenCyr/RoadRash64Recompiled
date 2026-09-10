. (Join-Path $PSScriptRoot 'Common.ps1')
$ErrorActionPreference = 'Stop'
$root = Get-RR64Root
$native = Join-Path $root 'native'
$lib = Join-Path $native 'lib'
$log = New-RR64Log 'native-deps.log'
$git = Find-RR64Git
New-Item -ItemType Directory -Force -Path $lib | Out-Null

$deps = @(
    @{ Name='N64ModernRuntime'; Url='https://github.com/N64Recomp/N64ModernRuntime.git'; Commit='ca568b6ad79b9029d14077f0c3ffa757727c5559' },
    @{ Name='RecompFrontend'; Url='https://github.com/N64Recomp/RecompFrontend.git'; Commit='d0d90ba49f46f4896aaeda362056c21b1e342561' },
    @{ Name='rt64'; Url='https://github.com/rt64/rt64.git'; Commit='6f1c2d99a4ea571c139f449c326fd176ba8f3496' }
)

Write-RR64 '============================================================' DarkGray
Write-RR64 ' Road Rash 64 Recomp - Native Dependency Setup' Cyan
Write-RR64 '============================================================' DarkGray
Write-RR64 "Git: $git"
Write-RR64 "Log: $log"

foreach ($dep in $deps) {
    $dest = Join-Path $lib $dep.Name
    Write-RR64 "`n[$($dep.Name)] pin $($dep.Commit)" Yellow

    if (-not (Test-Path (Join-Path $dest '.git'))) {
        if (Test-Path $dest) {
            $backup = $dest + '.incomplete-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
            Move-Item -LiteralPath $dest -Destination $backup
            Write-RR64 "Moved incomplete directory to: $backup" DarkYellow
        }
        Invoke-RR64Native $git @('-c','core.longpaths=true','clone','--filter=blob:none','--no-checkout',$dep.Url,$dest) $log $root
    }

    # Fetch the exact commit; no dependency drift between user runs.
    Invoke-RR64Native $git @('-C',$dest,'fetch','--depth','1','origin',$dep.Commit) $log $root
    Invoke-RR64Native $git @('-C',$dest,'checkout','--detach',$dep.Commit) $log $root
    Invoke-RR64Native $git @('-C',$dest,'-c','core.longpaths=true','submodule','sync','--recursive') $log $root
    Invoke-RR64Native $git @('-C',$dest,'-c','core.longpaths=true','submodule','update','--init','--recursive','--depth','1') $log $root

    $head = (& $git -C $dest rev-parse HEAD 2>$null).Trim()
    if ($head -ne $dep.Commit) {
        throw "$($dep.Name) checkout mismatch. Expected $($dep.Commit), got $head"
    }
    Write-RR64 "Verified: $head" Green
}

Write-RR64 "`n[SUCCESS] Native dependencies are pinned and ready." Green
Write-RR64 'Next: run Check-NativePrereqs.cmd, then Build-NativeProbe-KEEP-OPEN.cmd.' Cyan
