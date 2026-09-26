param([Parameter(Mandatory=$true)][string]$OutputPath,
 [string]$GeneratedDirectory = (Join-Path $PSScriptRoot '../../build/RecompiledFuncs'))
$ErrorActionPreference = 'Stop'
$result = "#include `"recomp.h`"`n#include `"rr64_native.hpp`"`n#include `"funcs.h`"`n"
# Exercise the original initializer/copy with the production hooks present.
# Only the unrelated name-entry UI setup is stubbed by the smoke executable.
foreach ($entry in @(@('funcs_18.c','func_8007280C'), @('funcs_5.c','func_8001A440'))) {
 $source = Get-Content (Join-Path $GeneratedDirectory $entry[0]) -Raw
 $match = [regex]::Match($source, '(?ms)^RECOMP_FUNC void ' + $entry[1] + '\([^\n]+\) \{.*?(?=^RECOMP_FUNC|\z)')
 if (-not $match.Success) { throw "Missing original name helper $($entry[1])" }
 if ($entry[1] -eq 'func_8007280C' -and
     $match.Value.IndexOf('rr64_profile_name_new_campaign(rdram);') -le
     $match.Value.IndexOf('MEM_B(0X8, ctx->r2) = 0;')) {
  throw 'Campaign profile hook missing or before the native field reset'
 }
 $result += $match.Value
}
$source = Get-Content (Join-Path $GeneratedDirectory 'funcs_17.c') -Raw
$begin = $source.IndexOf('    // 0x8006C4D4:')
$end = $source.IndexOf('    // 0x8006C4E8:', $begin)
if ($begin -lt 0 -or $end -le $begin) { throw 'Missing native solo rider name copy' }
$body = $source.Substring($begin, $end-$begin)
if (-not $body.Contains('if (rr64_thrash_options_active()) rr64_profile_name_thrash(rdram);')) {
 throw 'Solo Thrash profile guard missing'
}
$result += "RECOMP_FUNC void test_thrash_profile_init(uint8_t* rdram, recomp_context* ctx) {`n$body`n}`n"
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
[System.IO.File]::WriteAllText($OutputPath,$result,[System.Text.UTF8Encoding]::new($false))
