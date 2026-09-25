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
foreach ($name in @('func_80063B50', 'func_80064430')) {
 $match = [regex]::Match($stats, '(?ms)^RECOMP_FUNC void ' + $name + '\([^\n]+\) \{.*?(?=^RECOMP_FUNC|\z)')
 if (-not $match.Success) { throw "Missing original credit helper $name" }
 $result += $match.Value
}
# Execute the complete scenery handler, including native destruction and both
# statistics helpers. The notification service is the only production stub.
$match = [regex]::Match($combat, '(?ms)^RECOMP_FUNC void func_8005FD80\([^\n]+\) \{.*?(?=^RECOMP_FUNC|\z)')
if (-not $match.Success) { throw 'Missing original scenery destruction handler' }
$scenery = $match.Value
$guard = 'if (!rr64_valid_combat_statistics((unsigned)ctx->r16 + 0x2Cu)) goto rr64_scenery_destroyed;'
if (-not $scenery.Contains($guard) -or -not $scenery.Contains('rr64_scenery_destroyed:;')) {
 throw 'Production scenery credit guard missing'
}
$result += $scenery
# Negative control: remove only the new guard and probe the old calls before
# they access memory. This demonstrates the old invalid-credit attempt safely.
$result += "void test_statistics_probe(uint8_t*, recomp_context*);`nvoid test_mayhem_probe(uint8_t*, recomp_context*);`n"
$result += $scenery.Replace($guard, '').Replace('rr64_scenery_destroyed:;', '').
 Replace('func_8005FD80(', 'test_unguarded_scenery(').
 Replace('func_80063B50(', 'test_statistics_probe(').
 Replace('func_80064430(', 'test_mayhem_probe(')
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
[System.IO.File]::WriteAllText($OutputPath,$result,[System.Text.UTF8Encoding]::new($false))
