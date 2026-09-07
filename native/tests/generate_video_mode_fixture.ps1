param([Parameter(Mandatory=$true)][string]$OutputPath,[string]$GeneratedDirectory)
$ErrorActionPreference='Stop'
$source=Get-Content (Join-Path $GeneratedDirectory 'funcs_0.c') -Raw
$result="#include `"recomp.h`"`n"
foreach($name in @('func_8000A310','func_8000A35C','func_8000A3A8')) {
    $match=[regex]::Match($source,'(?ms)^RECOMP_FUNC void '+$name+'\([^\n]+\) \{.*?(?=^RECOMP_FUNC|\z)')
    if(-not $match.Success){throw "Missing $name"}
    $result+=$match.Value+"`n"
}
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($OutputPath))|Out-Null
[IO.File]::WriteAllText($OutputPath,$result)
