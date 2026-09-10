$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$frontend = Join-Path $root 'native\lib\RecompFrontend\recompui\src\renderer\rt64_render_context.cpp'
$rt64 = Join-Path $root 'native\lib\rt64\src\hle\rt64_application.cpp'
$rdp = Join-Path $root 'native\lib\rt64\src\hle\rt64_rdp.cpp'

function Write-Step([string]$Message) {
    Write-Host "[RR64-DIAG-PATCH] $Message" -ForegroundColor DarkCyan
}

function Ensure-Backup([string]$Path) {
    $backup = "$Path.rr64-v0310.bak"
    if (-not (Test-Path -LiteralPath $backup)) {
        Copy-Item -LiteralPath $Path -Destination $backup -Force
        Write-Step "Backup created: $backup"
    }
}

function Replace-Anchor {
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)][string]$Marker,
        [Parameter(Mandatory=$true)][string]$Old,
        [Parameter(Mandatory=$true)][string]$New,
        [Parameter(Mandatory=$true)][string]$Label,
        [bool]$Required = $false
    )

    $text = [System.IO.File]::ReadAllText($Path)
    if ($text.Contains($Marker)) {
        Write-Step "Already patched: $Label"
        return $true
    }

    if (-not $text.Contains($Old)) {
        if ($Required) {
            throw "Could not apply required renderer diagnostic patch '$Label'. Expected anchor was not found in: $Path"
        }
        Write-Warning "Optional renderer diagnostic anchor not found: $Label"
        return $false
    }

    $text = $text.Replace($Old, $New)
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $text, $utf8NoBom)
    Write-Step "Patched: $Label"
    return $true
}

foreach ($path in @($frontend, $rt64, $rdp)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Renderer source was not found: $path`nRun Setup-NativeDeps-KEEP-OPEN.cmd first."
    }
    Ensure-Backup $path
}

# -----------------------------------------------------------------------------
# Road Rash 64 far-plane primitive depth compatibility.
#
# Host D32 depth clears clamp the N64's maximum encoded clear value to 1.0.
# Road Rash uses primitive Z=0x7FFF for several full-screen menu layers. RT64's
# strict LESS comparison would otherwise reject those pixels as equal to the
# clamped clear value, leaving the menu art invisible.
# -----------------------------------------------------------------------------
$newText = @'
#include <cassert>
#include <cmath>
// RR64_COMPAT_FAR_PRIM_DEPTH_INCLUDE
'@
Replace-Anchor -Path $rdp -Marker '// RR64_COMPAT_FAR_PRIM_DEPTH_INCLUDE' -Old '#include <cassert>' -New $newText -Label 'Road Rash far-plane depth include' -Required $true | Out-Null

$newText = @'
    void RDP::setPrimDepth(uint16_t z, uint16_t dz) {
        const float Fixed15ToFloat = 1.0f / 32767.0f;
        const float Fixed16ToFloat = 1.0f / 65535.0f;
        hlslpp::float2 &primDepth = primDepthStack[primDepthStackSize - 1];
        // RR64_COMPAT_FAR_PRIM_DEPTH: D32 clear values are clamped to 1.0 by
        // host APIs. Keep the N64's maximum primitive Z just below that clear
        // value so RT64's strict LESS comparison does not discard far-plane UI.
        const uint16_t maskedZ = z & 0x7FFFU;
        primDepth.x = (maskedZ == 0x7FFFU)
            ? std::nextafter(1.0f, 0.0f)
            : maskedZ * Fixed15ToFloat;
        primDepth.y = (dz & 0xFFFFU) * Fixed16ToFloat;
        state->updateDrawStatusAttribute(DrawAttribute::PrimDepth);
    }
'@
$oldText = @'
    void RDP::setPrimDepth(uint16_t z, uint16_t dz) {
        const float Fixed15ToFloat = 1.0f / 32767.0f;
        const float Fixed16ToFloat = 1.0f / 65535.0f;
        hlslpp::float2 &primDepth = primDepthStack[primDepthStackSize - 1];
        primDepth.x = (z & 0x7FFFU) * Fixed15ToFloat;
        primDepth.y = (dz & 0xFFFFU) * Fixed16ToFloat;
        state->updateDrawStatusAttribute(DrawAttribute::PrimDepth);
    }
'@
Replace-Anchor -Path $rdp -Marker '// RR64_COMPAT_FAR_PRIM_DEPTH:' -Old $oldText -New $newText -Label 'Road Rash far-plane primitive depth' -Required $true | Out-Null

# -----------------------------------------------------------------------------
# RecompFrontend diagnostics.
# -----------------------------------------------------------------------------
$newText = @'
#include <algorithm>
#include <cstdio>
#include <cstdlib>
// RR64_DIAG_V0310_FRONTEND_INCLUDES
'@
Replace-Anchor -Path $frontend -Marker '// RR64_DIAG_V0310_FRONTEND_INCLUDES' -Old '#include <algorithm>' -New $newText -Label 'frontend diagnostic includes' -Required $true | Out-Null

$newText = @'
    // RR64_DIAG_V0310_FRONTEND_ENTRY
    fprintf(stderr, "[RR64-RF-STAGE] RT64Context constructor entered. rdram=%p window=%p thread=%u\n",
        static_cast<void*>(rdram), reinterpret_cast<void*>(window_handle.window), static_cast<unsigned>(window_handle.thread_id));
    fflush(stderr);

    const char* rr64SkipUiHook = std::getenv("RR64_RT64_SKIP_UI_HOOK");
    if ((rr64SkipUiHook != nullptr) && (rr64SkipUiHook[0] == '1') && (rr64SkipUiHook[1] == '\0')) {
        fprintf(stderr, "[RR64-RF-STAGE] UI render hook intentionally SKIPPED for diagnostic run.\n");
        fflush(stderr);
    }
    else {
        fprintf(stderr, "[RR64-RF-STAGE] Installing RecompFrontend RT64 render hooks...\n");
        fflush(stderr);
        recompui::set_render_hooks();
        fprintf(stderr, "[RR64-RF-STAGE] RecompFrontend RT64 render hooks installed.\n");
        fflush(stderr);
    }
'@
Replace-Anchor -Path $frontend -Marker '// RR64_DIAG_V0310_FRONTEND_ENTRY' -Old '    recompui::set_render_hooks();' -New $newText -Label 'frontend constructor entry/render hook' -Required $true | Out-Null

$newText = @'
    RT64::Application::Core appCore{};
    // RR64_DIAG_V0310_APPCORE
    fprintf(stderr, "[RR64-RF-STAGE] Building RT64 Application::Core.\n");
    fflush(stderr);
'@
Replace-Anchor -Path $frontend -Marker '// RR64_DIAG_V0310_APPCORE' -Old '    RT64::Application::Core appCore{};' -New $newText -Label 'frontend app core' | Out-Null

$newText = @'
    RT64::ApplicationConfiguration appConfig{};
    // RR64_DIAG_V0310_APPCONFIG
    fprintf(stderr, "[RR64-RF-STAGE] RT64 ApplicationConfiguration value-initialized.\n");
    fflush(stderr);
'@
Replace-Anchor -Path $frontend -Marker '// RR64_DIAG_V0310_APPCONFIG' -Old '    RT64::ApplicationConfiguration appConfig;' -New $newText -Label 'frontend application config' -Required $true | Out-Null

$newText = @'
    appConfig.useConfigurationFile = false;
    // RecompFrontend owns the application/config directory. Disable RT64's
    // separate data-path auto-detection during this bootstrap diagnostic.
    appConfig.detectDataPath = false;
    // RR64_DIAG_V0310_NO_DATAPATH
    fprintf(stderr, "[RR64-RF-STAGE] RT64 config: useConfigurationFile=0 detectDataPath=0.\n");
    fflush(stderr);
'@
Replace-Anchor -Path $frontend -Marker '// RR64_DIAG_V0310_NO_DATAPATH' -Old '    appConfig.useConfigurationFile = false;' -New $newText -Label 'frontend data-path bypass' -Required $true | Out-Null

$newText = @'
    // RR64_DIAG_V0310_APP_CREATE
    fprintf(stderr, "[RR64-RF-STAGE] Constructing RT64::Application...\n");
    fflush(stderr);
    app = std::make_unique<RT64::Application>(appCore, appConfig);
    fprintf(stderr, "[RR64-RF-STAGE] RT64::Application constructor returned. app=%p\n", static_cast<void*>(app.get()));
    fflush(stderr);
'@
Replace-Anchor -Path $frontend -Marker '// RR64_DIAG_V0310_APP_CREATE' -Old '    app = std::make_unique<RT64::Application>(appCore, appConfig);' -New $newText -Label 'frontend RT64 application construction' -Required $true | Out-Null

$newText = @'
    // RR64_DIAG_V0310_USER_CONFIG
    fprintf(stderr, "[RR64-RF-STAGE] Applying RecompFrontend graphics config to RT64...\n");
    fflush(stderr);
    set_application_user_config(app.get(), cur_config);
    fprintf(stderr, "[RR64-RF-STAGE] RecompFrontend graphics config applied. api_option=%d\n", static_cast<int>(cur_config.api_option));
    fflush(stderr);
'@
Replace-Anchor -Path $frontend -Marker '// RR64_DIAG_V0310_USER_CONFIG' -Old '    set_application_user_config(app.get(), cur_config);' -New $newText -Label 'frontend user graphics config' -Required $true | Out-Null

$newText = @'
    // RR64_DIAG_V0310_SETUP_CALL
    fprintf(stderr, "[RR64-RF-STAGE] Calling RT64::Application::setup(thread=%u)...\n", static_cast<unsigned>(thread_id));
    fflush(stderr);
    setup_result = map_setup_result(app->setup(thread_id));
    fprintf(stderr, "[RR64-RF-STAGE] RT64::Application::setup returned result=%d chosen_api=%d.\n",
        static_cast<int>(setup_result), static_cast<int>(app->chosenGraphicsAPI));
    fflush(stderr);
'@
Replace-Anchor -Path $frontend -Marker '// RR64_DIAG_V0310_SETUP_CALL' -Old '    setup_result = map_setup_result(app->setup(thread_id));' -New $newText -Label 'frontend RT64 setup call' -Required $true | Out-Null

# -----------------------------------------------------------------------------
# RT64 diagnostics. These are intentionally optional except for the setup entry:
# minor source differences should not make the build wrapper fail. The D3D12
# no-UI-hook runner remains useful even if a specific marker is unavailable.
# -----------------------------------------------------------------------------
$newText = @'
#include "rt64_application.h"
#include <cstdio>
// RR64_DIAG_V0310_RT64_INCLUDES
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_RT64_INCLUDES' -Old '#include "rt64_application.h"' -New $newText -Label 'RT64 diagnostic include' | Out-Null

$newText = @'
Application::Application(const Core &core, const ApplicationConfiguration &appConfig) {
    // RR64_DIAG_V0310_RT64_CTOR
    fprintf(stderr, "[RR64-RT64-STAGE] Application ctor begin. detectDataPath=%d useConfig=%d\n",
        appConfig.detectDataPath ? 1 : 0, appConfig.useConfigurationFile ? 1 : 0);
    fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_RT64_CTOR' -Old 'Application::Application(const Core &core, const ApplicationConfiguration &appConfig) {' -New $newText -Label 'RT64 application constructor' | Out-Null

$newText = @'
    fprintf(stderr, "[RR64-RT64-STAGE] Timer::initialize...\n"); fflush(stderr);
    Timer::initialize();
    // RR64_DIAG_V0310_TIMER
    fprintf(stderr, "[RR64-RT64-STAGE] Timer initialized.\n"); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_TIMER' -Old '    Timer::initialize();' -New $newText -Label 'RT64 timer init' | Out-Null

$newText = @'
    fprintf(stderr, "[RR64-RT64-STAGE] FileDialog::initialize...\n"); fflush(stderr);
    FileDialog::initialize();
    // RR64_DIAG_V0310_FILE_DIALOG
    fprintf(stderr, "[RR64-RT64-STAGE] FileDialog initialized.\n"); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_FILE_DIALOG' -Old '    FileDialog::initialize();' -New $newText -Label 'RT64 file dialog init' | Out-Null

$newText = @'
    fprintf(stderr, "[RR64-RT64-STAGE] userPaths.setupPaths...\n"); fflush(stderr);
    userPaths.setupPaths(this->appConfig.dataPath);
    // RR64_DIAG_V0310_PATHS
    fprintf(stderr, "[RR64-RT64-STAGE] userPaths.setupPaths returned.\n"); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_PATHS' -Old '    userPaths.setupPaths(this->appConfig.dataPath);' -New $newText -Label 'RT64 path setup' | Out-Null

$newText = @'
Application::SetupResult Application::setup(uint32_t threadId) {
    // RR64_DIAG_V0310_SETUP_BEGIN
    fprintf(stderr, "[RR64-RT64-STAGE] Application::setup begin thread=%u.\n", static_cast<unsigned>(threadId));
    fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_SETUP_BEGIN' -Old 'Application::SetupResult Application::setup(uint32_t threadId) {' -New $newText -Label 'RT64 setup begin' -Required $true | Out-Null

$newText = @'
    // RR64_DIAG_V0310_INTERPRETER
    fprintf(stderr, "[RR64-RT64-STAGE] Creating interpreter/state...\n"); fflush(stderr);
    interpreter = std::make_unique<Interpreter>();
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_INTERPRETER' -Old '    interpreter = std::make_unique<Interpreter>();' -New $newText -Label 'RT64 interpreter' | Out-Null

$newText = @'
    // RR64_DIAG_V0310_WINDOW
    fprintf(stderr, "[RR64-RT64-STAGE] Creating/wrapping ApplicationWindow...\n"); fflush(stderr);
    appWindow = std::make_unique<ApplicationWindow>();
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_WINDOW' -Old '    appWindow = std::make_unique<ApplicationWindow>();' -New $newText -Label 'RT64 window wrapper' | Out-Null

$newText = @'
    appWindow->detectRefreshRate();
    // RR64_DIAG_V0310_REFRESH
    fprintf(stderr, "[RR64-RT64-STAGE] Window setup complete; refresh rate detected.\n"); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_REFRESH' -Old '    appWindow->detectRefreshRate();' -New $newText -Label 'RT64 refresh detect' | Out-Null

$newText = @'
    chosenGraphicsAPI = UserConfiguration::resolveGraphicsAPI(userConfig.graphicsAPI);
    // RR64_DIAG_V0310_CHOSEN_API
    fprintf(stderr, "[RR64-RT64-STAGE] Graphics API resolved to enum=%d (requested=%d).\n",
        static_cast<int>(chosenGraphicsAPI), static_cast<int>(userConfig.graphicsAPI)); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_CHOSEN_API' -Old '    chosenGraphicsAPI = UserConfiguration::resolveGraphicsAPI(userConfig.graphicsAPI);' -New $newText -Label 'RT64 API resolve' | Out-Null

$newText = @'
            fprintf(stderr, "[RR64-RT64-STAGE] CreateD3D12Interface...\n"); fflush(stderr);
            renderInterface = CreateD3D12Interface();
            // RR64_DIAG_V0310_D3D12_INTERFACE
            fprintf(stderr, "[RR64-RT64-STAGE] CreateD3D12Interface returned %p.\n", static_cast<void*>(renderInterface.get())); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_D3D12_INTERFACE' -Old '            renderInterface = CreateD3D12Interface();' -New $newText -Label 'RT64 D3D12 interface' | Out-Null

$newText = @'
            fprintf(stderr, "[RR64-RT64-STAGE] CreateVulkanInterfaceWrapper...\n"); fflush(stderr);
            renderInterface = CreateVulkanInterfaceWrapper(appWindow->windowHandle);
            // RR64_DIAG_V0310_VK_INTERFACE
            fprintf(stderr, "[RR64-RT64-STAGE] CreateVulkanInterfaceWrapper returned %p.\n", static_cast<void*>(renderInterface.get())); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_VK_INTERFACE' -Old '            renderInterface = CreateVulkanInterfaceWrapper(appWindow->windowHandle);' -New $newText -Label 'RT64 Vulkan interface' | Out-Null

$newText = @'
            fprintf(stderr, "[RR64-RT64-STAGE] RenderInterface::createDevice...\n"); fflush(stderr);
            device = renderInterface->createDevice();
            // RR64_DIAG_V0310_DEVICE
            fprintf(stderr, "[RR64-RT64-STAGE] createDevice returned %p.\n", static_cast<void*>(device.get())); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_DEVICE' -Old '            device = renderInterface->createDevice();' -New $newText -Label 'RT64 device creation' | Out-Null

$newText = @'
    // RR64_DIAG_V0310_DESCRIPTION
    fprintf(stderr, "[RR64-RT64-STAGE] Reading device description...\n"); fflush(stderr);
    RenderDeviceDescription deviceDescription = device->getDescription();
    fprintf(stderr, "[RR64-RT64-STAGE] Device description read (name_len=%zu vendor=0x%X).\n",
        deviceDescription.name.size(), static_cast<unsigned>(deviceDescription.vendor)); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_DESCRIPTION' -Old '    RenderDeviceDescription deviceDescription = device->getDescription();' -New $newText -Label 'RT64 device description' | Out-Null

$newText = @'
    // RR64_DIAG_V0310_HOOK
    fprintf(stderr, "[RR64-RT64-STAGE] About to query/call RenderHookInit.\n"); fflush(stderr);
    RenderHookInit *initHook = GetRenderHookInit();
    fprintf(stderr, "[RR64-RT64-STAGE] RenderHookInit present=%d.\n", initHook != nullptr ? 1 : 0); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_HOOK' -Old '    RenderHookInit *initHook = GetRenderHookInit();' -New $newText -Label 'RT64 render hook pointer' | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RT64-STAGE] Calling RecompFrontend RenderHookInit...\n"); fflush(stderr);
        initHook(renderInterface.get(), device.get());
        // RR64_DIAG_V0310_HOOK_RETURN
        fprintf(stderr, "[RR64-RT64-STAGE] RenderHookInit returned.\n"); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_HOOK_RETURN' -Old '        initHook(renderInterface.get(), device.get());' -New $newText -Label 'RT64 render hook call' | Out-Null

$newText = @'
    // RR64_DIAG_V0310_WORKERS
    fprintf(stderr, "[RR64-RT64-STAGE] Creating render workers/uploaders...\n"); fflush(stderr);
    drawDataUploader = std::make_unique<BufferUploader>(device.get());
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_WORKERS' -Old '    drawDataUploader = std::make_unique<BufferUploader>(device.get());' -New $newText -Label 'RT64 workers' | Out-Null

$newText = @'
    fprintf(stderr, "[RR64-RT64-STAGE] Creating swap chain...\n"); fflush(stderr);
    swapChain = presentGraphicsWorker->commandQueue->createSwapChain(swapChainDesc);
    // RR64_DIAG_V0310_SWAPCHAIN
    fprintf(stderr, "[RR64-RT64-STAGE] Swap chain returned %p.\n", static_cast<void*>(swapChain.get())); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_SWAPCHAIN' -Old '    swapChain = presentGraphicsWorker->commandQueue->createSwapChain(swapChainDesc);' -New $newText -Label 'RT64 swap chain' | Out-Null

$newText = @'
    // RR64_DIAG_V0310_SHADERS
    fprintf(stderr, "[RR64-RT64-STAGE] Creating shader library (HDR=%d HWResolve=%d)...\n", usesHDR ? 1 : 0, usesHardwareResolve ? 1 : 0); fflush(stderr);
    shaderLibrary = std::make_unique<ShaderLibrary>(usesHDR, usesHardwareResolve);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_SHADERS' -Old '    shaderLibrary = std::make_unique<ShaderLibrary>(usesHDR, usesHardwareResolve);' -New $newText -Label 'RT64 shader library' | Out-Null

$newText = @'
    shaderLibrary->setupMultisamplingShaders(renderInterface.get(), device.get(), multisampling);
    // RR64_DIAG_V0310_SHADERS_DONE
    fprintf(stderr, "[RR64-RT64-STAGE] Common/multisampling shaders initialized.\n"); fflush(stderr);
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_SHADERS_DONE' -Old '    shaderLibrary->setupMultisamplingShaders(renderInterface.get(), device.get(), multisampling);' -New $newText -Label 'RT64 shader setup' | Out-Null

# Use a specific final-success anchor so an earlier successful helper return is
# never mislabeled as the end of Application::setup.
$oldText = @'
    state->rdp->setGBI();

    return SetupResult::Success;
'@
$newText = @'
    state->rdp->setGBI();

    // RR64_DIAG_V0310_SETUP_SUCCESS
    fprintf(stderr, "[RR64-RT64-STAGE] Application::setup reached Success.\n"); fflush(stderr);
    return SetupResult::Success;
'@
Replace-Anchor -Path $rt64 -Marker '// RR64_DIAG_V0310_SETUP_SUCCESS' -Old $oldText -New $newText -Label 'RT64 setup success' | Out-Null

Write-Step 'RecompFrontend and RT64 stage diagnostics are installed.'
Write-Step 'RT64 private data-path detection is disabled for this bootstrap diagnostic.'
