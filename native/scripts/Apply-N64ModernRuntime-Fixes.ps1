$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$threadsSource = Join-Path $root 'native\lib\N64ModernRuntime\ultramodern\src\threads.cpp'
$timerSource = Join-Path $root 'native\lib\N64ModernRuntime\ultramodern\src\timer.cpp'
$eventsSource = Join-Path $root 'native\lib\N64ModernRuntime\ultramodern\src\events.cpp'

function Write-Step([string]$Message) {
    Write-Host "[RR64-RUNTIME-PATCH] $Message" -ForegroundColor DarkCyan
}

if (-not (Test-Path -LiteralPath $threadsSource)) {
    throw "N64ModernRuntime thread source was not found: $threadsSource`nRun Setup-NativeDeps-KEEP-OPEN.cmd first."
}
if (-not (Test-Path -LiteralPath $timerSource)) {
    throw "N64ModernRuntime timer source was not found: $timerSource`nRun Setup-NativeDeps-KEEP-OPEN.cmd first."
}
if (-not (Test-Path -LiteralPath $eventsSource)) {
    throw "N64ModernRuntime event source was not found: $eventsSource`nRun Setup-NativeDeps-KEEP-OPEN.cmd first."
}

$text = [System.IO.File]::ReadAllText($threadsSource)
$text = $text.Replace("`r`n", "`n").Replace("`r", "`n")
$marker = 'RR64_FIX_BOOTSTRAP_THREAD_TERMINATION'
if ($text.Contains($marker)) {
    Write-Step 'Bootstrap thread-termination fix is already installed.'
} else {
    $oldText = @'
        resume_thread(t);
        //throw ultramodern::thread_terminated{};
'@
    $newText = @'
        resume_thread(t);
        // The initial cartridge bootstrap is not an emulated OSThread. Once it
        // starts the first real N64 thread, unwind back to wait_for_game_started
        // instead of returning to the ROM's intentional post-boot BREAK.
        throw ultramodern::thread_terminated{}; // RR64_FIX_BOOTSTRAP_THREAD_TERMINATION
'@

    $oldNormalized = $oldText.Replace("`r`n", "`n").Replace("`r", "`n")
    $newNormalized = $newText.Replace("`r`n", "`n").Replace("`r", "`n")
    if (-not $text.Contains($oldNormalized)) {
        throw "Could not apply the bootstrap thread-termination fix. Expected anchor was not found in: $threadsSource"
    }

    $backup = "$threadsSource.rr64-bootstrap.bak"
    if (-not (Test-Path -LiteralPath $backup)) {
        Copy-Item -LiteralPath $threadsSource -Destination $backup -Force
        Write-Step "Backup created: $backup"
    }

    $text = $text.Replace($oldNormalized, $newNormalized)
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($threadsSource, $text, $utf8NoBom)
    Write-Step 'Restored first-thread bootstrap termination.'
}

$timerText = [System.IO.File]::ReadAllText($timerSource)
$timerText = $timerText.Replace("`r`n", "`n").Replace("`r", "`n")
$timerMarker = 'RR64_FIX_STRICT_MONOTONIC_COUNT'
if ($timerText.Contains($timerMarker)) {
    Write-Step 'Strictly monotonic N64 Count fix is already installed.'
} else {
    $oldTimerText = @'
extern "C" u32 osGetCount() {
    uint64_t total_count = time_now();

    // Allow for overflows, which is how osGetCount behaves
    return (uint32_t)total_count;
}
'@
    $newTimerText = @'
extern "C" u32 osGetCount() {
    static std::atomic<uint64_t> last_count{0};
    const uint64_t host_count = time_now();
    uint64_t previous = last_count.load(std::memory_order_relaxed);

    // Road Rash reads CP0 Count closely enough that two calls can land in the
    // same host microsecond. Real N64 Count continues advancing between those
    // instructions; returning equality makes the game mistake it for a full
    // 32-bit wrap and inject an enormous physics delta.
    for (;;) {
        const uint64_t next = host_count > previous ? host_count : previous + 1;
        if (last_count.compare_exchange_weak(
                previous,
                next,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            return static_cast<uint32_t>(next); // RR64_FIX_STRICT_MONOTONIC_COUNT
        }
    }
}
'@

    $oldTimerNormalized = $oldTimerText.Replace("`r`n", "`n").Replace("`r", "`n")
    $newTimerNormalized = $newTimerText.Replace("`r`n", "`n").Replace("`r", "`n")
    if (-not $timerText.Contains($oldTimerNormalized)) {
        throw "Could not apply the strictly monotonic N64 Count fix. Expected anchor was not found in: $timerSource"
    }

    $timerBackup = "$timerSource.rr64-count.bak"
    if (-not (Test-Path -LiteralPath $timerBackup)) {
        Copy-Item -LiteralPath $timerSource -Destination $timerBackup -Force
        Write-Step "Backup created: $timerBackup"
    }

    if (-not $timerText.Contains('#include <atomic>')) {
        $timerText = $timerText.Replace('#include <thread>', "#include <atomic>`n#include <thread>")
    }
    $timerText = $timerText.Replace($oldTimerNormalized, $newTimerNormalized)
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($timerSource, $timerText, $utf8NoBom)
    Write-Step 'Installed strictly monotonic N64 Count behavior.'
}

$eventsText = [System.IO.File]::ReadAllText($eventsSource)
$eventsText = $eventsText.Replace("`r`n", "`n").Replace("`r", "`n")
$eventsMarker = 'RR64_FIX_DEFER_GFX_SP_COMPLETION'
if ($eventsText.Contains($eventsMarker)) {
    Write-Step 'Graphics-task input lifetime fix is already installed.'
} else {
    $oldEventsText = @'
            if (const auto* task_action = std::get_if<SpTaskAction>(&action)) {
                // Tell the game that the RSP completed instantly. This will allow it to queue other task types, but it won't
                // start another graphics task until the RDP is also complete. Games usually preserve the RSP inputs until the RDP
                // is finished as well, so sending this early shouldn't be an issue in most cases.
                // If this causes issues then the logic can be replaced with responding to yield requests.
                sp_complete();
                ultramodern::measure_input_latency();

                PTR(u64) displaylist = task_action->task.t.data_ptr;
                ultramodern::extensions::on_displaylist_submitted(displaylist);

                [[maybe_unused]] auto renderer_start = std::chrono::high_resolution_clock::now();
                renderer_context->send_dl(&task_action->task);
                [[maybe_unused]] auto renderer_end = std::chrono::high_resolution_clock::now();

                dp_complete();
'@
    $newEventsText = @'
            if (const auto* task_action = std::get_if<SpTaskAction>(&action)) {
                ultramodern::measure_input_latency();

                PTR(u64) displaylist = task_action->task.t.data_ptr;
                ultramodern::extensions::on_displaylist_submitted(displaylist);

                [[maybe_unused]] auto renderer_start = std::chrono::high_resolution_clock::now();
                renderer_context->send_dl(&task_action->task);
                [[maybe_unused]] auto renderer_end = std::chrono::high_resolution_clock::now();

                // RR64_FIX_DEFER_GFX_SP_COMPLETION: Do not release the graphics
                // task's RSP inputs until RT64 has
                // finished parsing them. Road Rash 64 reuses display-list and
                // world-geometry storage after the SP completion message; an
                // early message can therefore race the renderer and produce a
                // frame with missing terrain while later draw layers survive.
                sp_complete();
                dp_complete();
'@

    $oldEventsNormalized = $oldEventsText.Replace("`r`n", "`n").Replace("`r", "`n")
    $newEventsNormalized = $newEventsText.Replace("`r`n", "`n").Replace("`r", "`n")
    if (-not $eventsText.Contains($oldEventsNormalized)) {
        throw "Could not apply the graphics-task input lifetime fix. Expected anchor was not found in: $eventsSource"
    }

    $eventsBackup = "$eventsSource.rr64-gfx-task.bak"
    if (-not (Test-Path -LiteralPath $eventsBackup)) {
        Copy-Item -LiteralPath $eventsSource -Destination $eventsBackup -Force
        Write-Step "Backup created: $eventsBackup"
    }

    $eventsText = $eventsText.Replace($oldEventsNormalized, $newEventsNormalized)
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($eventsSource, $eventsText, $utf8NoBom)
    Write-Step 'Installed graphics-task input lifetime synchronization.'
}

function Install-CanonicalRuntimeSource {
    param(
        [Parameter(Mandatory = $true)][string]$Target,
        [Parameter(Mandatory = $true)][string]$Template,
        [Parameter(Mandatory = $true)][string]$Marker,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if (-not (Test-Path -LiteralPath $Target -PathType Leaf)) {
        throw "N64ModernRuntime source was not found: $Target`nRun Setup-NativeDeps-KEEP-OPEN.cmd first."
    }
    if (-not (Test-Path -LiteralPath $Template -PathType Leaf)) {
        throw "Road Rash runtime patch source is missing: $Template"
    }

    $targetText = [System.IO.File]::ReadAllText($Target)
    if ($targetText.Contains($Marker)) {
        Write-Step "$Description is already installed."
        return
    }

    $backup = "$Target.rr64-original.bak"
    if (-not (Test-Path -LiteralPath $backup)) {
        Copy-Item -LiteralPath $Target -Destination $backup -Force
        Write-Step "Backup created: $backup"
    }

    $templateText = [System.IO.File]::ReadAllText($Template)
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Target, $templateText, $encoding)
    Write-Step "Installed $Description."
}

$controllerSource = Join-Path $root 'native\lib\N64ModernRuntime\librecomp\src\cont.cpp'
$controllerTemplate = Join-Path $root 'native\patches\N64ModernRuntime\librecomp\src\cont.cpp'
$pakSource = Join-Path $root 'native\lib\N64ModernRuntime\librecomp\src\pak.cpp'
$pakTemplate = Join-Path $root 'native\patches\N64ModernRuntime\librecomp\src\pak.cpp'

Install-CanonicalRuntimeSource `
    -Target $controllerSource `
    -Template $controllerTemplate `
    -Marker 'RR64_FIX_CONTROLLER_ERROR_PROPAGATION' `
    -Description 'controller response/error propagation fix'
Install-CanonicalRuntimeSource `
    -Target $pakSource `
    -Template $pakTemplate `
    -Marker 'RR64_FIX_VIRTUAL_CONTROLLER_PAK' `
    -Description 'persistent virtual Controller Pak backend'
