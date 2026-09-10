$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$uiRenderer = Join-Path $root 'native\lib\RecompFrontend\recompui\src\renderer\ui_renderer.cpp'

function Write-Step([string]$Message) {
    Write-Host "[RR64-UI-PATCH] $Message" -ForegroundColor DarkCyan
}

function Ensure-Backup([string]$Path) {
    $backup = "$Path.rr64-v0312.bak"
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

    # Git checkouts on Windows may use CRLF while this hotfix payload uses LF.
    # Normalize all strings before matching so multi-line anchors are independent
    # of core.autocrlf / editor line-ending settings.
    $text = [System.IO.File]::ReadAllText($Path)
    $text = $text.Replace("`r`n", "`n").Replace("`r", "`n")
    $oldNormalized = $Old.Replace("`r`n", "`n").Replace("`r", "`n")
    $newNormalized = $New.Replace("`r`n", "`n").Replace("`r", "`n")
    $markerNormalized = $Marker.Replace("`r`n", "`n").Replace("`r", "`n")

    if ($text.Contains($markerNormalized)) {
        Write-Step "Already patched: $Label"
        return $true
    }

    if (-not $text.Contains($oldNormalized)) {
        if ($Required) {
            throw "Could not apply required v0.3.12 UI renderer patch '$Label'. Expected anchor was not found in: $Path"
        }
        Write-Warning "Optional v0.3.12 UI renderer anchor not found: $Label"
        return $false
    }

    $text = $text.Replace($oldNormalized, $newNormalized)
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $text, $utf8NoBom)
    Write-Step "Patched: $Label"
    return $true
}

if (-not (Test-Path -LiteralPath $uiRenderer)) {
    throw "RecompFrontend UI renderer source was not found: $uiRenderer`nRun Setup-NativeDeps-KEEP-OPEN.cmd first."
}
Ensure-Backup $uiRenderer

# Diagnostic includes.
$newText = @'
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <exception>
// RR64_DIAG_V0312_UI_INCLUDES
'@
$oldText = @'
#include <fstream>
#include <filesystem>
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_INCLUDES' -Old $oldText -New $newText -Label 'UI diagnostic includes' -Required $true | Out-Null

# Constructor entry.
$oldText = @'
    RmlRenderInterface_RT64_impl(plume::RenderInterface* interface, plume::RenderDevice* device) {
'@
$newText = @'
    RmlRenderInterface_RT64_impl(plume::RenderInterface* interface, plume::RenderDevice* device) {
        // RR64_DIAG_V0312_UI_CTOR_ENTRY
        fprintf(stderr, "[RR64-RF-UI] ctor entered interface=%p device=%p\n", static_cast<void*>(interface), static_cast<void*>(device));
        fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_CTOR_ENTRY' -Old $oldText -New $newText -Label 'UI constructor entry' -Required $true | Out-Null

# Interface/device assignment complete.
$oldText = @'
        device_ = device;
'@
$newText = @'
        device_ = device;
        // RR64_DIAG_V0312_UI_DEVICE_ASSIGNED
        fprintf(stderr, "[RR64-RF-UI] interface/device pointers assigned.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_DEVICE_ASSIGNED' -Old $oldText -New $newText -Label 'UI interface/device assignment' | Out-Null

# Sample-count query.
$oldText = @'
        const plume::RenderSampleCounts desired_sample_count = plume::RenderSampleCount::COUNT_8;
        if (device_->getSampleCountsSupported(SwapChainFormat) & desired_sample_count) {
            multisampling_.sampleCount = desired_sample_count;
        }
'@
$newText = @'
        const plume::RenderSampleCounts desired_sample_count = plume::RenderSampleCount::COUNT_8;
        // RR64_DIAG_V0312_UI_SAMPLE_COUNTS
        fprintf(stderr, "[RR64-RF-UI] querying sample counts...\n"); fflush(stderr);
        const plume::RenderSampleCounts rr64_supported_sample_counts = device_->getSampleCountsSupported(SwapChainFormat);
        fprintf(stderr, "[RR64-RF-UI] sample-count query returned.\n"); fflush(stderr);
        if (rr64_supported_sample_counts & desired_sample_count) {
            multisampling_.sampleCount = desired_sample_count;
        }
        fprintf(stderr, "[RR64-RF-UI] multisampling selection complete.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_SAMPLE_COUNTS' -Old $oldText -New $newText -Label 'UI sample-count query' -Required $true | Out-Null

# Initial dynamic buffers.
$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating upload buffer...\n"); fflush(stderr);
        resize_dynamic_buffer(upload_buffer_, initial_upload_buffer_size, false);
        // RR64_DIAG_V0312_UI_UPLOAD_BUFFER
        fprintf(stderr, "[RR64-RF-UI] upload buffer OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_UPLOAD_BUFFER' -Old '        resize_dynamic_buffer(upload_buffer_, initial_upload_buffer_size, false);' -New $newText -Label 'UI upload buffer' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating vertex buffer...\n"); fflush(stderr);
        resize_dynamic_buffer(vertex_buffer_, initial_vertex_buffer_size, false);
        // RR64_DIAG_V0312_UI_VERTEX_BUFFER
        fprintf(stderr, "[RR64-RF-UI] vertex buffer OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_VERTEX_BUFFER' -Old '        resize_dynamic_buffer(vertex_buffer_, initial_vertex_buffer_size, false);' -New $newText -Label 'UI vertex buffer' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating index buffer...\n"); fflush(stderr);
        resize_dynamic_buffer(index_buffer_, initial_index_buffer_size, false);
        // RR64_DIAG_V0312_UI_INDEX_BUFFER
        fprintf(stderr, "[RR64-RF-UI] index buffer OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_INDEX_BUFFER' -Old '        resize_dynamic_buffer(index_buffer_, initial_index_buffer_size, false);' -New $newText -Label 'UI index buffer' -Required $true | Out-Null

# Vertex input elements. Each semantic gets its own marker because std::length_error
# can be produced while constructing/copying string-backed input descriptions.
$newText = @'
        fprintf(stderr, "[RR64-RF-UI] building vertex input vector...\n"); fflush(stderr);
        std::vector<plume::RenderInputElement> vertex_elements{};
        // RR64_DIAG_V0312_UI_VERTEX_VECTOR
        fprintf(stderr, "[RR64-RF-UI] vertex input vector created.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_VERTEX_VECTOR' -Old '        std::vector<plume::RenderInputElement> vertex_elements{};' -New $newText -Label 'UI vertex input vector' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] adding POSITION input...\n"); fflush(stderr);
        vertex_elements.emplace_back(plume::RenderInputElement{ "POSITION", 0, 0, plume::RenderFormat::R32G32_FLOAT, 0, offsetof(Rml::Vertex, position) });
        // RR64_DIAG_V0312_UI_POSITION
        fprintf(stderr, "[RR64-RF-UI] POSITION input OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_POSITION' -Old '        vertex_elements.emplace_back(plume::RenderInputElement{ "POSITION", 0, 0, plume::RenderFormat::R32G32_FLOAT, 0, offsetof(Rml::Vertex, position) });' -New $newText -Label 'UI POSITION input' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] adding COLOR input...\n"); fflush(stderr);
        vertex_elements.emplace_back(plume::RenderInputElement{ "COLOR", 0, 1, plume::RenderFormat::R8G8B8A8_UNORM, 0, offsetof(Rml::Vertex, colour) });
        // RR64_DIAG_V0312_UI_COLOR
        fprintf(stderr, "[RR64-RF-UI] COLOR input OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_COLOR' -Old '        vertex_elements.emplace_back(plume::RenderInputElement{ "COLOR", 0, 1, plume::RenderFormat::R8G8B8A8_UNORM, 0, offsetof(Rml::Vertex, colour) });' -New $newText -Label 'UI COLOR input' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] adding TEXCOORD input...\n"); fflush(stderr);
        vertex_elements.emplace_back(plume::RenderInputElement{ "TEXCOORD", 0, 2, plume::RenderFormat::R32G32_FLOAT, 0, offsetof(Rml::Vertex, tex_coord) });
        // RR64_DIAG_V0312_UI_TEXCOORD
        fprintf(stderr, "[RR64-RF-UI] TEXCOORD input OK; vertex format complete.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_TEXCOORD' -Old '        vertex_elements.emplace_back(plume::RenderInputElement{ "TEXCOORD", 0, 2, plume::RenderFormat::R32G32_FLOAT, 0, offsetof(Rml::Vertex, tex_coord) });' -New $newText -Label 'UI TEXCOORD input' -Required $true | Out-Null

# Sampler descriptor: v0.3.12 intentionally value-initializes this structure.
$newText = @'
        // RR64_DIAG_V0312_UI_SAMPLER_DESC_ZERO
        fprintf(stderr, "[RR64-RF-UI] value-initializing sampler descriptor...\n"); fflush(stderr);
        plume::RenderSamplerDesc samplerDesc{};
        fprintf(stderr, "[RR64-RF-UI] sampler descriptor initialized.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_SAMPLER_DESC_ZERO' -Old '        plume::RenderSamplerDesc samplerDesc;' -New $newText -Label 'zero-initialize sampler descriptor' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating nearest sampler...\n"); fflush(stderr);
        nearestSampler_ = device_->createSampler(samplerDesc);
        // RR64_DIAG_V0312_UI_NEAREST_SAMPLER
        fprintf(stderr, "[RR64-RF-UI] nearest sampler OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_NEAREST_SAMPLER' -Old '        nearestSampler_ = device_->createSampler(samplerDesc);' -New $newText -Label 'UI nearest sampler' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating linear sampler...\n"); fflush(stderr);
        linearSampler_ = device_->createSampler(samplerDesc);
        // RR64_DIAG_V0312_UI_LINEAR_SAMPLER
        fprintf(stderr, "[RR64-RF-UI] linear sampler OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_LINEAR_SAMPLER' -Old '        linearSampler_ = device_->createSampler(samplerDesc);' -New $newText -Label 'UI linear sampler' -Required $true | Out-Null

# Shader capability/creation.
$newText = @'
        // RR64_DIAG_V0312_UI_SHADER_FORMAT
        fprintf(stderr, "[RR64-RF-UI] querying shader format...\n"); fflush(stderr);
        plume::RenderShaderFormat shaderFormat = interface_->getCapabilities().shaderFormat;
        fprintf(stderr, "[RR64-RF-UI] shader format query OK (enum=%d).\n", static_cast<int>(shaderFormat)); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_SHADER_FORMAT' -Old '        plume::RenderShaderFormat shaderFormat = interface_->getCapabilities().shaderFormat;' -New $newText -Label 'UI shader format' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating vertex shader...\n"); fflush(stderr);
        vertex_shader_ = device_->createShader(GET_SHADER_BLOB(InterfaceVS, shaderFormat), GET_SHADER_SIZE(InterfaceVS, shaderFormat), "VSMain", shaderFormat);
        // RR64_DIAG_V0312_UI_VERTEX_SHADER
        fprintf(stderr, "[RR64-RF-UI] vertex shader OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_VERTEX_SHADER' -Old '        vertex_shader_ = device_->createShader(GET_SHADER_BLOB(InterfaceVS, shaderFormat), GET_SHADER_SIZE(InterfaceVS, shaderFormat), "VSMain", shaderFormat);' -New $newText -Label 'UI vertex shader' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating pixel shader...\n"); fflush(stderr);
        pixel_shader_ = device_->createShader(GET_SHADER_BLOB(InterfacePS, shaderFormat), GET_SHADER_SIZE(InterfacePS, shaderFormat), "PSMain", shaderFormat);
        // RR64_DIAG_V0312_UI_PIXEL_SHADER
        fprintf(stderr, "[RR64-RF-UI] pixel shader OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_PIXEL_SHADER' -Old '        pixel_shader_ = device_->createShader(GET_SHADER_BLOB(InterfacePS, shaderFormat), GET_SHADER_SIZE(InterfacePS, shaderFormat), "PSMain", shaderFormat);' -New $newText -Label 'UI pixel shader' -Required $true | Out-Null

# Sampler descriptor set.
$newText = @'
        plume::RenderDescriptorSetBuilder sampler_set_builder{};
        // RR64_DIAG_V0312_UI_SAMPLER_SET_BUILDER
        fprintf(stderr, "[RR64-RF-UI] building sampler descriptor set...\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_SAMPLER_SET_BUILDER' -Old '        plume::RenderDescriptorSetBuilder sampler_set_builder{};' -New $newText -Label 'UI sampler set builder' -Required $true | Out-Null

$newText = @'
        sampler_set_ = sampler_set_builder.create(device_);
        // RR64_DIAG_V0312_UI_SAMPLER_SET_CREATE
        fprintf(stderr, "[RR64-RF-UI] sampler descriptor set OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_SAMPLER_SET_CREATE' -Old '        sampler_set_ = sampler_set_builder.create(device_);' -New $newText -Label 'UI sampler set creation' -Required $true | Out-Null

# Texture descriptor-set builder.
$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating texture descriptor-set builder...\n"); fflush(stderr);
        texture_set_builder_ = std::make_unique<plume::RenderDescriptorSetBuilder>();
        // RR64_DIAG_V0312_UI_TEXTURE_SET_BUILDER
        fprintf(stderr, "[RR64-RF-UI] texture descriptor-set builder OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_TEXTURE_SET_BUILDER' -Old '        texture_set_builder_ = std::make_unique<plume::RenderDescriptorSetBuilder>();' -New $newText -Label 'UI texture set builder' -Required $true | Out-Null

$newText = @'
        texture_set_builder_->end();
        // RR64_DIAG_V0312_UI_TEXTURE_SET_DONE
        fprintf(stderr, "[RR64-RF-UI] texture descriptor-set layout complete.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_TEXTURE_SET_DONE' -Old '        texture_set_builder_->end();' -New $newText -Label 'UI texture set layout' -Required $true | Out-Null

# Pipeline layout.
$newText = @'
        plume::RenderPipelineLayoutBuilder layout_builder{};
        // RR64_DIAG_V0312_UI_LAYOUT_BUILDER
        fprintf(stderr, "[RR64-RF-UI] building pipeline layout...\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_LAYOUT_BUILDER' -Old '        plume::RenderPipelineLayoutBuilder layout_builder{};' -New $newText -Label 'UI pipeline layout builder' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating pipeline layout object...\n"); fflush(stderr);
        layout_ = layout_builder.create(device_);
        // RR64_DIAG_V0312_UI_LAYOUT_CREATE
        fprintf(stderr, "[RR64-RF-UI] pipeline layout OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_LAYOUT_CREATE' -Old '        layout_ = layout_builder.create(device_);' -New $newText -Label 'UI pipeline layout creation' -Required $true | Out-Null

# Graphics pipeline.
$newText = @'
        plume::RenderGraphicsPipelineDesc pipeline_desc{};
        // RR64_DIAG_V0312_UI_PIPELINE_DESC
        fprintf(stderr, "[RR64-RF-UI] graphics pipeline description initialized.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_PIPELINE_DESC' -Old '        plume::RenderGraphicsPipelineDesc pipeline_desc{};' -New $newText -Label 'UI graphics pipeline description' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating primary graphics pipeline...\n"); fflush(stderr);
        pipeline_ = device_->createGraphicsPipeline(pipeline_desc);
        // RR64_DIAG_V0312_UI_PIPELINE_CREATE
        fprintf(stderr, "[RR64-RF-UI] primary graphics pipeline OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_PIPELINE_CREATE' -Old '        pipeline_ = device_->createGraphicsPipeline(pipeline_desc);' -New $newText -Label 'UI primary graphics pipeline' -Required $true | Out-Null

$newText = @'
            fprintf(stderr, "[RR64-RF-UI] creating MSAA graphics pipeline...\n"); fflush(stderr);
            pipeline_ms_ = device_->createGraphicsPipeline(pipeline_desc);
            // RR64_DIAG_V0312_UI_MSAA_PIPELINE
            fprintf(stderr, "[RR64-RF-UI] MSAA graphics pipeline OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_MSAA_PIPELINE' -Old '            pipeline_ms_ = device_->createGraphicsPipeline(pipeline_desc);' -New $newText -Label 'UI MSAA graphics pipeline' | Out-Null

$newText = @'
            fprintf(stderr, "[RR64-RF-UI] creating MSAA screen descriptor set...\n"); fflush(stderr);
            screen_descriptor_set_ = device_->createDescriptorSet(plume::RenderDescriptorSetDesc(&screen_descriptor_range, 1));
            // RR64_DIAG_V0312_UI_SCREEN_SET
            fprintf(stderr, "[RR64-RF-UI] MSAA screen descriptor set OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_SCREEN_SET' -Old '            screen_descriptor_set_ = device_->createDescriptorSet(plume::RenderDescriptorSetDesc(&screen_descriptor_range, 1));' -New $newText -Label 'UI MSAA screen descriptor set' | Out-Null

$newText = @'
            fprintf(stderr, "[RR64-RF-UI] creating MSAA screen vertex buffer...\n"); fflush(stderr);
            screen_vertex_buffer_ = device_->createBuffer(plume::RenderBufferDesc::VertexBuffer(screen_vertex_buffer_size_, plume::RenderHeapType::UPLOAD));
            // RR64_DIAG_V0312_UI_SCREEN_VERTEX_BUFFER
            fprintf(stderr, "[RR64-RF-UI] MSAA screen vertex buffer OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_SCREEN_VERTEX_BUFFER' -Old '            screen_vertex_buffer_ = device_->createBuffer(plume::RenderBufferDesc::VertexBuffer(screen_vertex_buffer_size_, plume::RenderHeapType::UPLOAD));' -New $newText -Label 'UI MSAA screen vertex buffer' | Out-Null

# Copy queue/list/fence.
$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating copy command queue...\n"); fflush(stderr);
        copy_command_queue_ = device->createCommandQueue(plume::RenderCommandListType::COPY);
        // RR64_DIAG_V0312_UI_COPY_QUEUE
        fprintf(stderr, "[RR64-RF-UI] copy command queue OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_COPY_QUEUE' -Old '        copy_command_queue_ = device->createCommandQueue(plume::RenderCommandListType::COPY);' -New $newText -Label 'UI copy queue' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating copy command list...\n"); fflush(stderr);
        copy_command_list_ = copy_command_queue_->createCommandList();
        // RR64_DIAG_V0312_UI_COPY_LIST
        fprintf(stderr, "[RR64-RF-UI] copy command list OK.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_COPY_LIST' -Old '        copy_command_list_ = copy_command_queue_->createCommandList();' -New $newText -Label 'UI copy list' -Required $true | Out-Null

$newText = @'
        fprintf(stderr, "[RR64-RF-UI] creating copy command fence...\n"); fflush(stderr);
        copy_command_fence_ = device->createCommandFence();
        // RR64_DIAG_V0312_UI_COPY_FENCE
        fprintf(stderr, "[RR64-RF-UI] copy command fence OK.\n"); fflush(stderr);
        fprintf(stderr, "[RR64-RF-UI] ctor COMPLETE.\n"); fflush(stderr);
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_COPY_FENCE' -Old '        copy_command_fence_ = device->createCommandFence();' -New $newText -Label 'UI copy fence/constructor complete' -Required $true | Out-Null

# Public init wrapper catches and labels exceptions before letting RT64's existing
# outer handler report them as renderer-creation failures.
$oldText = @'
void recompui::RmlRenderInterface_RT64::init(plume::RenderInterface* interface, plume::RenderDevice* device) {
    impl = std::make_unique<RmlRenderInterface_RT64_impl>(interface, device);
}
'@
$newText = @'
void recompui::RmlRenderInterface_RT64::init(plume::RenderInterface* interface, plume::RenderDevice* device) {
    // RR64_DIAG_V0312_UI_INIT_WRAPPER
    fprintf(stderr, "[RR64-RF-UI] RmlRenderInterface_RT64::init entered.\n"); fflush(stderr);
    try {
        impl = std::make_unique<RmlRenderInterface_RT64_impl>(interface, device);
        fprintf(stderr, "[RR64-RF-UI] RmlRenderInterface_RT64::init COMPLETE.\n"); fflush(stderr);
    }
    catch (const std::exception& ex) {
        fprintf(stderr, "[RR64-RF-UI] RmlRenderInterface_RT64::init exception: %s\n", ex.what()); fflush(stderr);
        throw;
    }
    catch (...) {
        fprintf(stderr, "[RR64-RF-UI] RmlRenderInterface_RT64::init unknown exception.\n"); fflush(stderr);
        throw;
    }
}
'@
Replace-Anchor -Path $uiRenderer -Marker '// RR64_DIAG_V0312_UI_INIT_WRAPPER' -Old $oldText -New $newText -Label 'UI init wrapper exception trace' -Required $true | Out-Null

Write-Step 'RecompFrontend UI renderer v0.3.12 diagnostics are installed.'
Write-Step 'RenderSamplerDesc is value-initialized to eliminate uninitialized descriptor fields.'
