$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$uiState = Join-Path $root 'native\lib\RecompFrontend\recompui\src\base\ui_state.cpp'

function Write-Step([string]$Message) {
    Write-Host "[RR64-UI-STATE-PATCH] $Message" -ForegroundColor DarkCyan
}

function Replace-Anchor {
    param(
        [Parameter(Mandatory=$true)][string]$Text,
        [Parameter(Mandatory=$true)][string]$Old,
        [Parameter(Mandatory=$true)][string]$New,
        [Parameter(Mandatory=$true)][string]$Label
    )

    $oldNormalized = $Old.Replace("`r`n", "`n").Replace("`r", "`n")
    $newNormalized = $New.Replace("`r`n", "`n").Replace("`r", "`n")
    if (-not $Text.Contains($oldNormalized)) {
        throw "Could not apply required v0.3.13 UI-state patch '$Label'. Expected anchor was not found in: $uiState"
    }

    Write-Step "Patched: $Label"
    return $Text.Replace($oldNormalized, $newNormalized)
}

if (-not (Test-Path -LiteralPath $uiState)) {
    throw "RecompFrontend UI-state source was not found: $uiState`nRun Setup-NativeDeps-KEEP-OPEN.cmd first."
}

$text = [System.IO.File]::ReadAllText($uiState)
$text = $text.Replace("`r`n", "`n").Replace("`r", "`n")
if ($text.Contains('// RR64_DIAG_V0313_UI_STATE_CTOR')) {
    Write-Step 'RecompFrontend UI-state v0.3.13 diagnostics are already installed.'
    exit 0
}

$backup = "$uiState.rr64-v0313.bak"
if (-not (Test-Path -LiteralPath $backup)) {
    Copy-Item -LiteralPath $uiState -Destination $backup -Force
    Write-Step "Backup created: $backup"
}

$oldText = @'
#include <chrono>
'@
$newText = @'
#include <chrono>
#include <cstdio>
#include <exception>
// RR64_DIAG_V0313_UI_STATE_INCLUDES
'@
$text = Replace-Anchor -Text $text -Old $oldText -New $newText -Label 'UI-state diagnostic includes'

$oldText = @'
    UIState(SDL_Window* window, plume::RenderInterface* interface, plume::RenderDevice* device) {

        system_interface = std::make_unique<SystemInterface_SDL>();
        system_interface->SetWindow(window);
        render_interface.init(interface, device);

        Rml::SetSystemInterface(system_interface.get());
        Rml::SetRenderInterface(render_interface.get_rml_interface());
        Rml::Factory::RegisterEventListenerInstancer(&event_listener_instancer);

        Rml::Initialise();

        recompui::register_custom_elements();

        // Apply the hack to replace RmlUi's default color parser with one that conforms to HTML5 alpha parsing for SASS compatibility
        recompui::apply_color_hack();

        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        
        context = Rml::CreateContext("main", Rml::Vector2i(width, height));

        Rml::Debugger::Initialise(context);
        {
            struct FontFace {
                const char* filename;
                bool fallback_face;
            };
            FontFace font_faces[] = {
                {"NotoEmoji-Regular.ttf", true},
                {"promptfont/promptfont.ttf", false},
            };

            for (const FontFace& face : font_faces) {
                auto font = recompui::file::get_asset_path(face.filename);
                Rml::LoadFontFace(font.string(), face.fallback_face);
            }

            if (primary_font.empty()) {
                throw std::runtime_error("No primary font was registered with recompui::register_primary_font");
            }

            auto primary_font_path = recompui::file::get_asset_path(primary_font.c_str());
            Rml::LoadFontFace(primary_font_path.string(), false);

            for (const auto& extra_font : extra_fonts) {
                auto extra_font_path = recompui::file::get_asset_path(extra_font.c_str());
                Rml::LoadFontFace(extra_font_path.string(), false);
            }
        }
    }

    void create_menus() {
        recompui::init_styling(recompui::file::get_asset_path("recomp.rcss"));
        recompui::init_launcher_menu();
        recompui::init_prompt_context();
        recompui::AssignPlayersModal::init();
        recompui::config::init_modal();
    }
'@
$newText = @'
    UIState(SDL_Window* window, plume::RenderInterface* interface, plume::RenderDevice* device) {
        // RR64_DIAG_V0313_UI_STATE_CTOR
        fprintf(stderr, "[RR64-RF-STATE] UIState ctor entered.\n"); fflush(stderr);

        fprintf(stderr, "[RR64-RF-STATE] creating SDL system interface...\n"); fflush(stderr);
        system_interface = std::make_unique<SystemInterface_SDL>();
        fprintf(stderr, "[RR64-RF-STATE] SDL system interface created.\n"); fflush(stderr);
        system_interface->SetWindow(window);
        fprintf(stderr, "[RR64-RF-STATE] SDL window assigned.\n"); fflush(stderr);
        render_interface.init(interface, device);
        fprintf(stderr, "[RR64-RF-STATE] render interface initialized.\n"); fflush(stderr);

        Rml::SetSystemInterface(system_interface.get());
        fprintf(stderr, "[RR64-RF-STATE] Rml system interface assigned.\n"); fflush(stderr);
        Rml::RenderInterface* rr64RmlRenderInterface = render_interface.get_rml_interface();
        fprintf(stderr, "[RR64-RF-STATE] adapted render interface acquired: %p.\n", static_cast<void*>(rr64RmlRenderInterface)); fflush(stderr);
        Rml::SetRenderInterface(rr64RmlRenderInterface);
        fprintf(stderr, "[RR64-RF-STATE] Rml render interface assigned.\n"); fflush(stderr);
        Rml::Factory::RegisterEventListenerInstancer(&event_listener_instancer);
        fprintf(stderr, "[RR64-RF-STATE] event-listener instancer registered.\n"); fflush(stderr);

        fprintf(stderr, "[RR64-RF-STATE] calling Rml::Initialise...\n"); fflush(stderr);
        const bool rr64RmlInitialized = Rml::Initialise();
        fprintf(stderr, "[RR64-RF-STATE] Rml::Initialise returned %d.\n", rr64RmlInitialized ? 1 : 0); fflush(stderr);

        fprintf(stderr, "[RR64-RF-STATE] registering custom elements...\n"); fflush(stderr);
        recompui::register_custom_elements();
        fprintf(stderr, "[RR64-RF-STATE] custom elements registered.\n"); fflush(stderr);

        // Apply the hack to replace RmlUi's default color parser with one that conforms to HTML5 alpha parsing for SASS compatibility
        recompui::apply_color_hack();
        fprintf(stderr, "[RR64-RF-STATE] color parser hack applied.\n"); fflush(stderr);

        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        fprintf(stderr, "[RR64-RF-STATE] window dimensions: %d x %d.\n", width, height); fflush(stderr);

        fprintf(stderr, "[RR64-RF-STATE] creating main Rml context...\n"); fflush(stderr);
        context = Rml::CreateContext("main", Rml::Vector2i(width, height));
        fprintf(stderr, "[RR64-RF-STATE] main Rml context returned: %p.\n", static_cast<void*>(context)); fflush(stderr);

        Rml::Debugger::Initialise(context);
        fprintf(stderr, "[RR64-RF-STATE] Rml debugger initialized.\n"); fflush(stderr);
        {
            struct FontFace {
                const char* filename;
                bool fallback_face;
            };
            FontFace font_faces[] = {
                {"NotoEmoji-Regular.ttf", true},
                {"promptfont/promptfont.ttf", false},
            };

            for (const FontFace& face : font_faces) {
                auto font = recompui::file::get_asset_path(face.filename);
                const std::string fontPath = font.string();
                fprintf(stderr, "[RR64-RF-STATE] loading bundled font '%s' (path_len=%zu fallback=%d)...\n", face.filename, fontPath.size(), face.fallback_face ? 1 : 0); fflush(stderr);
                const bool loaded = Rml::LoadFontFace(fontPath, face.fallback_face);
                fprintf(stderr, "[RR64-RF-STATE] bundled font load returned %d.\n", loaded ? 1 : 0); fflush(stderr);
            }

            fprintf(stderr, "[RR64-RF-STATE] primary font name length=%zu.\n", primary_font.size()); fflush(stderr);
            if (primary_font.empty()) {
                throw std::runtime_error("No primary font was registered with recompui::register_primary_font");
            }

            auto primary_font_path = recompui::file::get_asset_path(primary_font.c_str());
            const std::string primaryFontPath = primary_font_path.string();
            fprintf(stderr, "[RR64-RF-STATE] loading primary font (path_len=%zu)...\n", primaryFontPath.size()); fflush(stderr);
            const bool primaryLoaded = Rml::LoadFontFace(primaryFontPath, false);
            fprintf(stderr, "[RR64-RF-STATE] primary font load returned %d.\n", primaryLoaded ? 1 : 0); fflush(stderr);

            for (const auto& extra_font : extra_fonts) {
                auto extra_font_path = recompui::file::get_asset_path(extra_font.c_str());
                const std::string extraFontPath = extra_font_path.string();
                fprintf(stderr, "[RR64-RF-STATE] loading extra font (name_len=%zu path_len=%zu)...\n", extra_font.size(), extraFontPath.size()); fflush(stderr);
                const bool extraLoaded = Rml::LoadFontFace(extraFontPath, false);
                fprintf(stderr, "[RR64-RF-STATE] extra font load returned %d.\n", extraLoaded ? 1 : 0); fflush(stderr);
            }
        }
        fprintf(stderr, "[RR64-RF-STATE] UIState ctor COMPLETE.\n"); fflush(stderr);
    }

    void create_menus() {
        // RR64_DIAG_V0313_UI_MENUS
        fprintf(stderr, "[RR64-RF-STATE] initializing UI styling...\n"); fflush(stderr);
        recompui::init_styling(recompui::file::get_asset_path("recomp.rcss"));
        fprintf(stderr, "[RR64-RF-STATE] UI styling initialized.\n"); fflush(stderr);
        recompui::init_launcher_menu();
        fprintf(stderr, "[RR64-RF-STATE] launcher menu initialized.\n"); fflush(stderr);
        recompui::init_prompt_context();
        fprintf(stderr, "[RR64-RF-STATE] prompt context initialized.\n"); fflush(stderr);
        recompui::AssignPlayersModal::init();
        fprintf(stderr, "[RR64-RF-STATE] player-assignment modal initialized.\n"); fflush(stderr);
        recompui::config::init_modal();
        fprintf(stderr, "[RR64-RF-STATE] configuration modal initialized.\n"); fflush(stderr);
    }
'@
$text = Replace-Anchor -Text $text -Old $oldText -New $newText -Label 'UIState constructor and menu stages'

$oldText = @'
void init_hook(plume::RenderInterface* interface, plume::RenderDevice* device) {
#if defined(__linux__)
    std::locale::global(std::locale::classic());
#endif
    ui_state = std::make_unique<UIState>(window, interface, device);
    ui_state->create_menus();
}
'@
$newText = @'
void init_hook(plume::RenderInterface* interface, plume::RenderDevice* device) {
#if defined(__linux__)
    std::locale::global(std::locale::classic());
#endif
    // RR64_DIAG_V0313_INIT_HOOK
    fprintf(stderr, "[RR64-RF-STATE] init_hook entered.\n"); fflush(stderr);
    try {
        ui_state = std::make_unique<UIState>(window, interface, device);
        fprintf(stderr, "[RR64-RF-STATE] UIState allocation/constructor returned.\n"); fflush(stderr);
        ui_state->create_menus();
        fprintf(stderr, "[RR64-RF-STATE] init_hook COMPLETE.\n"); fflush(stderr);
    }
    catch (const std::exception& ex) {
        fprintf(stderr, "[RR64-RF-STATE] init_hook exception: %s\n", ex.what()); fflush(stderr);
        throw;
    }
    catch (...) {
        fprintf(stderr, "[RR64-RF-STATE] init_hook unknown exception.\n"); fflush(stderr);
        throw;
    }
}
'@
$text = Replace-Anchor -Text $text -Old $oldText -New $newText -Label 'render init-hook exception boundary'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($uiState, $text, $utf8NoBom)
Write-Step 'RecompFrontend UI-state v0.3.13 diagnostics are installed.'
