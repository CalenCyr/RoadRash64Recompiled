param([Parameter(Mandatory=$true)][string]$OutputPath,
 [string]$GeneratedDirectory = (Join-Path $PSScriptRoot '../../build/RecompiledFuncs'))
$ErrorActionPreference = 'Stop'
$combat = Get-Content (Join-Path $GeneratedDirectory 'funcs_15.c') -Raw
$stats = Get-Content (Join-Path $GeneratedDirectory 'funcs_16.c') -Raw
$result = "#include `"recomp.h`"`n#include `"rr64_native.hpp`"`n#include `"funcs.h`"`n"
# Execute the exact generated credit branches, including production guards.
# Surrounding collision/damage is outside this fixture's scope.
foreach ($case in @(@('weapon','800615B4','L_800615E8:','L_800615EC'),
                    @('impact','80061778','L_800617AC:','L_800617B0'))) {
 $begin = $combat.IndexOf('    // 0x' + $case[1] + ':')
 $end = $combat.IndexOf($case[2], $begin)
 if ($begin -lt 0 -or $end -le $begin) { throw 'Missing native credit branch' }
 $body = $combat.Substring($begin, $end-$begin)
 if (-not $body.Contains('rr64_valid_combat_statistics')) { throw 'Production credit guard missing' }
 $result += "RECOMP_FUNC void test_credit_$($case[0])(uint8_t* rdram, recomp_context* ctx) {`nint c1cs=0;`n"
 $result += $body + "`n$($case[2])`n$($case[3]):;`n}`n"
}
$match = [regex]::Match($stats, '(?ms)^RECOMP_FUNC void func_80063B50\([^\n]+\) \{.*?(?=^RECOMP_FUNC|\z)')
if (-not $match.Success) { throw 'Missing original statistics writer' }
$result += $match.Value
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
[System.IO.File]::WriteAllText($OutputPath,$result,[System.Text.UTF8Encoding]::new($false))
