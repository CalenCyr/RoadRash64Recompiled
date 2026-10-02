param(
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [string]$GeneratedDirectory = (Join-Path $PSScriptRoot '../../build/RecompiledFuncs')
)
$ErrorActionPreference = 'Stop'
$names = @('func_8001BD50', 'func_8001BDF8', 'func_8001C084',
    'func_8007B8D4', 'func_8001677C')
$found = @{}
foreach ($file in Get-ChildItem -LiteralPath $GeneratedDirectory -Filter '*.c' | Sort-Object Name) {
    $source = Get-Content -LiteralPath $file.FullName -Raw
    foreach ($name in $names) {
        $match = [regex]::Match($source,
            '(?ms)^RECOMP_FUNC void ' + [regex]::Escape($name) + '\([^\n]+\) \{.*?(?=^RECOMP_FUNC|\z)')
        if ($match.Success) {
            if ($found.ContainsKey($name)) { throw "Duplicate generated function: $name" }
            $found[$name] = $match.Value
        }
    }
}
$result = "// Original pool allocator/free and terrain streamer; do not edit.`n" +
    "#include `"recomp.h`"`n#include `"rr64_native.hpp`"`n#include `"funcs.h`"`n`n"
foreach ($name in $names) {
    if (-not $found.ContainsKey($name)) { throw "Missing generated function: $name" }
    $result += $found[$name] + "`n"
}
$stubs = @('func_8000CE14', 'func_8007AB1C', 'func_8007B6BC', 'func_8007B7A0',
    'func_8007D750', 'osRecvMesg_recomp')
foreach ($call in [regex]::Matches($result, '(\w+)\(rdram, ctx\)')) {
    if ($call.Groups[1].Value -notin $names -and $call.Groups[1].Value -notin $stubs) {
        throw "Uncovered generated terrain helper: $($call.Groups[1].Value)"
    }
}
$output = [IO.Path]::GetFullPath($OutputPath)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($output)) | Out-Null
if (-not [IO.File]::Exists($output) -or [IO.File]::ReadAllText($output) -cne $result) {
    [IO.File]::WriteAllText($output, $result, [Text.UTF8Encoding]::new($false))
}
Write-Output "Extracted $($names.Count) original allocator/streamer functions."
