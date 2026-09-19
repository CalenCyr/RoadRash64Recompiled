param([Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
$source=Get-Content (Join-Path $PSScriptRoot '../src/rr64_achievements.cpp') -Raw
$result=''
foreach($name in @('save_progress','flush_progress')) {
 $match=[regex]::Match($source,'(?ms)^(?:bool|void) '+$name+'\([^\n]*\) \{.*?^\}')
 if(-not $match.Success){throw "Missing persistence function $name"}
 $body=$match.Value
 if($name -eq 'save_progress'){$body=$body.Replace('bool save_progress(', 'bool disk_save_progress(')}
 $result+=$body+"`n"
}
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
[System.IO.File]::WriteAllText($OutputPath,$result,[System.Text.UTF8Encoding]::new($false))
