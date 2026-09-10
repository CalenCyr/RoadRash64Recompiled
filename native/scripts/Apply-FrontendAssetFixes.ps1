$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$uiContext = Join-Path $root 'native\lib\RecompFrontend\recompui\src\core\ui_context.cpp'

function Write-Step([string]$Message) {
    Write-Host "[RR64-ASSET-PATCH] $Message" -ForegroundColor DarkCyan
}

if (-not (Test-Path -LiteralPath $uiContext)) {
    throw "RecompFrontend UI-context source was not found: $uiContext`nRun Setup-NativeDeps-KEEP-OPEN.cmd first."
}

$text = [System.IO.File]::ReadAllText($uiContext)
$text = $text.Replace("`r`n", "`n").Replace("`r", "`n")
$includePatched = $text.Contains('// RR64_FIX_V0314_STYLESHEET_INCLUDE')
$readerPatched = $text.Contains('// RR64_FIX_V0314_STYLESHEET_READ')
if ($includePatched -and $readerPatched) {
    Write-Step 'RecompFrontend stylesheet-loading fix is already installed.'
    exit 0
}

$backup = "$uiContext.rr64-v0314.bak"
if (-not (Test-Path -LiteralPath $backup)) {
    Copy-Item -LiteralPath $uiContext -Destination $backup -Force
    Write-Step "Backup created: $backup"
}

if (-not $includePatched) {
    $includeOld = '#include <fstream>'
    $includeNew = @'
#include <fstream>
#include <stdexcept>
// RR64_FIX_V0314_STYLESHEET_INCLUDE
'@
    if (-not $text.Contains($includeOld)) {
        throw "Could not add the required v0.3.14 stylesheet include. Expected anchor was not found in: $uiContext"
    }
    $text = $text.Replace($includeOld, $includeNew)
}

$oldText = @'
    std::string style{};
    {
        std::ifstream style_stream{rcss_file};
        style_stream.seekg(0, std::ios::end);
        style.resize(style_stream.tellg());
        style_stream.seekg(0, std::ios::beg);

        style_stream.read(style.data(), style.size());
    }
'@
$newText = @'
    std::string style{};
    {
        // RR64_FIX_V0314_STYLESHEET_READ
        std::ifstream style_stream{rcss_file, std::ios::binary | std::ios::ate};
        if (!style_stream.is_open()) {
            throw std::runtime_error("Unable to open RecompFrontend stylesheet: " + rcss_file.string());
        }

        const std::streampos style_end = style_stream.tellg();
        if (style_end < 0) {
            throw std::runtime_error("Unable to determine RecompFrontend stylesheet size: " + rcss_file.string());
        }

        style.resize(static_cast<std::size_t>(style_end));
        style_stream.seekg(0, std::ios::beg);
        if (!style.empty()) {
            style_stream.read(style.data(), static_cast<std::streamsize>(style.size()));
            if (!style_stream) {
                throw std::runtime_error("Unable to read RecompFrontend stylesheet: " + rcss_file.string());
            }
        }
    }
'@

$oldNormalized = $oldText.Replace("`r`n", "`n").Replace("`r", "`n")
$newNormalized = $newText.Replace("`r`n", "`n").Replace("`r", "`n")
if (-not $readerPatched) {
    if (-not $text.Contains($oldNormalized)) {
        throw "Could not apply the required v0.3.14 stylesheet-loading fix. Expected anchor was not found in: $uiContext"
    }
    $text = $text.Replace($oldNormalized, $newNormalized)
}
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($uiContext, $text, $utf8NoBom)
Write-Step 'RecompFrontend stylesheet-loading fix is installed.'
