param([Parameter(Mandatory=$true)][string]$OutputPath,
 [string]$GeneratedDirectory = (Join-Path $PSScriptRoot '../../build/RecompiledFuncs'))
$ErrorActionPreference = 'Stop'
$result = "#include `"recomp.h`"`n#include `"rr64_native.hpp`"`n#include `"funcs.h`"`n"
foreach ($entry in @(@('funcs_18.c','func_800728B8'), @('funcs_18.c','func_80073658'),
                    @('funcs_15.c','func_8005F420'), @('funcs_4.c','func_8001A288'),
                    @('funcs_4.c','func_8001A2E8'))) {
 $source = Get-Content (Join-Path $GeneratedDirectory $entry[0]) -Raw
 $match = [regex]::Match($source, '(?ms)^RECOMP_FUNC void ' + $entry[1] + '\([^\n]+\) \{.*?(?=^RECOMP_FUNC|\z)')
 if (-not $match.Success) { throw "Missing original helper $($entry[1])" }
 $result += $match.Value
}
function Slice($file, $name, $start, $end, $required, $tail) {
 $source = Get-Content (Join-Path $GeneratedDirectory $file) -Raw
 $begin = $source.IndexOf('    // 0x' + $start + ':')
 $finish = $source.IndexOf('    // 0x' + $end + ':', $begin)
 if ($begin -lt 0 -or $finish -le $begin) { throw "Missing native $name slice" }
 $body = $source.Substring($begin, $finish-$begin)
 if (-not $body.Contains($required)) { throw "Missing production hook in $name" }
 return "RECOMP_FUNC void $name(uint8_t* rdram, recomp_context* ctx) {`n$body`n$tail;`n}`n"
}
$result += Slice 'funcs_7.c' 'test_thrash_bike_selection' '80026C18' '80026EC0' 'rr64_local_bike_menu_level' ''
$result += Slice 'funcs_7.c' 'test_campaign_ending_exit' '80023800' '80023818' 'rr64_campaign_ending_exit' ''
$result += Slice 'funcs_6.c' 'test_validated_campaign_save' '80020AA4' '80020ABC' 'rr64_campaign_restore_unlocks' 'L_80020AC0:; L_80020B14:;'
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
[System.IO.File]::WriteAllText($OutputPath,$result,[System.Text.UTF8Encoding]::new($false))
