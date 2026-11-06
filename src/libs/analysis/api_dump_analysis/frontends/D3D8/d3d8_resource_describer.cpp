/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_resource_describer.h"
#include "analysis/api_dump_analysis/frontends/shared/mip_chain.h"
#include <algorithm>

uint64_t Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::compute_api_flags(const Interception::Hooks::D3D8::D3DPOOL& in_pool,
                                                                                        const uint32_t&                           in_usage)
{
    constexpr uint32_t POOL_SHIFT = 32;

    return (static_cast<uint64_t>(in_pool) << POOL_SHIFT) | in_usage;
}

Analysis::APIDumpAnalysis::Format Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::convert_format(const Interception::Hooks::D3D8::D3DFORMAT& in_format)
{
    Analysis::APIDumpAnalysis::Format result;

    result.raw_value = static_cast<uint32_t>(in_format);

    switch (in_format)
    {
        case Interception::Hooks::D3D8::D3DFMT_R8G8B8:
        case Interception::Hooks::D3D8::D3DFMT_A8R8G8B8:
        case Interception::Hooks::D3D8::D3DFMT_X8R8G8B8:
        case Interception::Hooks::D3D8::D3DFMT_R5G6B5:
        case Interception::Hooks::D3D8::D3DFMT_X1R5G5B5:
        case Interception::Hooks::D3D8::D3DFMT_A1R5G5B5:
        case Interception::Hooks::D3D8::D3DFMT_A4R4G4B4:
        case Interception::Hooks::D3D8::D3DFMT_R3G3B2:
        case Interception::Hooks::D3D8::D3DFMT_A8:
        case Interception::Hooks::D3D8::D3DFMT_A8R3G3B2:
        case Interception::Hooks::D3D8::D3DFMT_X4R4G4B4:
        case Interception::Hooks::D3D8::D3DFMT_A2B10G10R10:
        case Interception::Hooks::D3D8::D3DFMT_G16R16:
        case Interception::Hooks::D3D8::D3DFMT_A8P8:
        case Interception::Hooks::D3D8::D3DFMT_P8:
        case Interception::Hooks::D3D8::D3DFMT_L8:
        case Interception::Hooks::D3D8::D3DFMT_A8L8:
        case Interception::Hooks::D3D8::D3DFMT_A4L4:
        case Interception::Hooks::D3D8::D3DFMT_V8U8:
        case Interception::Hooks::D3D8::D3DFMT_L6V5U5:
        case Interception::Hooks::D3D8::D3DFMT_X8L8V8U8:
        case Interception::Hooks::D3D8::D3DFMT_Q8W8V8U8:
        case Interception::Hooks::D3D8::D3DFMT_V16U16:
        case Interception::Hooks::D3D8::D3DFMT_W11V11U10:
        case Interception::Hooks::D3D8::D3DFMT_A2W10V10U10:
        {
            result.format_class = Analysis::APIDumpAnalysis::FormatClass::COLOR;

            break;
        }

        case Interception::Hooks::D3D8::D3DFMT_DXT1:
        case Interception::Hooks::D3D8::D3DFMT_DXT2:
        case Interception::Hooks::D3D8::D3DFMT_DXT3:
        case Interception::Hooks::D3D8::D3DFMT_DXT4:
        case Interception::Hooks::D3D8::D3DFMT_DXT5:
        {
            result.format_class = Analysis::APIDumpAnalysis::FormatClass::COMPRESSED;

            break;
        }

        case Interception::Hooks::D3D8::D3DFMT_D16_LOCKABLE:
        case Interception::Hooks::D3D8::D3DFMT_D32:
        case Interception::Hooks::D3D8::D3DFMT_D16:
        case Interception::Hooks::D3D8::D3DFMT_D24X8:
        {
            result.format_class = Analysis::APIDumpAnalysis::FormatClass::DEPTH;

            break;
        }

        case Interception::Hooks::D3D8::D3DFMT_D15S1:
        case Interception::Hooks::D3D8::D3DFMT_D24S8:
        case Interception::Hooks::D3D8::D3DFMT_D24X4S4:
        {
            result.format_class = Analysis::APIDumpAnalysis::FormatClass::DEPTH_STENCIL;

            break;
        }

        default:
        {
            /* Packed YUV formats, the formats of buffers, and those that are not known. */
            result.format_class = Analysis::APIDumpAnalysis::FormatClass::UNKNOWN;

            break;
        }
    }

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_auto_depth_stencil(const Interception::Core::D3D8Utils::PresentParameters& in_parameters)
{
    auto result = describe_surface(in_parameters.back_buffer_width,
                                   in_parameters.back_buffer_height,
                                   in_parameters.auto_depth_stencil_format,
                                   Interception::Hooks::D3D8::D3DUSAGE_DEPTHSTENCIL,
                                   Interception::Hooks::D3D8::D3DPOOL_DEFAULT);

    result.sample_count = get_n_samples(in_parameters.multi_sample_type);

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_back_buffer(const Interception::Core::D3D8Utils::PresentParameters& in_parameters)
{
    auto result = describe_surface(in_parameters.back_buffer_width,
                                   in_parameters.back_buffer_height,
                                   in_parameters.back_buffer_format,
                                   Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET,
                                   Interception::Hooks::D3D8::D3DPOOL_DEFAULT);

    result.kind         = Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE;
    result.sample_count = get_n_samples(in_parameters.multi_sample_type);

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_cube_texture(const CallView& in_view)
{
    constexpr size_t   EDGE_LENGTH_ARGUMENT = 1;
    constexpr size_t   FORMAT_ARGUMENT      = 4;
    constexpr size_t   LEVELS_ARGUMENT      = 2;
    constexpr uint32_t N_CUBE_FACES         = 6;
    constexpr size_t   POOL_ARGUMENT        = 5;
    constexpr size_t   USAGE_ARGUMENT       = 3;

    const uint32_t                             edge_length = in_view.get_u32(EDGE_LENGTH_ARGUMENT);
    const Interception::Hooks::D3D8::D3DFORMAT format      = static_cast<Interception::Hooks::D3D8::D3DFORMAT>(in_view.get_u32(FORMAT_ARGUMENT) );
    const uint32_t                             level_count = in_view.get_u32(LEVELS_ARGUMENT);
    const Interception::Hooks::D3D8::D3DPOOL   pool        = static_cast<Interception::Hooks::D3D8::D3DPOOL>  (in_view.get_u32(POOL_ARGUMENT) );
    const uint32_t                             usage       = in_view.get_u32(USAGE_ARGUMENT);
    auto                                       result      = describe_surface(edge_length,
                                                                              edge_length,
                                                                              format,
                                                                              usage,
                                                                              pool);

    result.depth_or_layer_count = N_CUBE_FACES;
    result.kind                 = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_CUBE;
    result.mip_level_count      = MipChain::get_level_count(level_count,
                                                            edge_length,
                                                            edge_length);

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_depth_stencil_surface(const CallView& in_view)
{
    constexpr size_t FORMAT_ARGUMENT       = 3;
    constexpr size_t HEIGHT_ARGUMENT       = 2;
    constexpr size_t MULTI_SAMPLE_ARGUMENT = 4;
    constexpr size_t WIDTH_ARGUMENT        = 1;

    const Interception::Hooks::D3D8::D3DFORMAT           format            = static_cast<Interception::Hooks::D3D8::D3DFORMAT>         (in_view.get_u32(FORMAT_ARGUMENT) );
    const uint32_t                                       height            = in_view.get_u32(HEIGHT_ARGUMENT);
    const Interception::Hooks::D3D8::D3DMULTISAMPLE_TYPE multi_sample_type = static_cast<Interception::Hooks::D3D8::D3DMULTISAMPLE_TYPE>(in_view.get_u32(MULTI_SAMPLE_ARGUMENT) );
    const uint32_t                                       width             = in_view.get_u32(WIDTH_ARGUMENT);
    auto                                                 result            = describe_surface(width,
                                                                                              height,
                                                                                              format,
                                                                                              Interception::Hooks::D3D8::D3DUSAGE_DEPTHSTENCIL,
                                                                                              Interception::Hooks::D3D8::D3DPOOL_DEFAULT);

    result.sample_count = get_n_samples(multi_sample_type);

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_image_surface(const CallView& in_view)
{
    constexpr size_t FORMAT_ARGUMENT = 3;
    constexpr size_t HEIGHT_ARGUMENT = 2;
    constexpr size_t WIDTH_ARGUMENT  = 1;

    const Interception::Hooks::D3D8::D3DFORMAT format = static_cast<Interception::Hooks::D3D8::D3DFORMAT>(in_view.get_u32(FORMAT_ARGUMENT) );
    const uint32_t                             height = in_view.get_u32(HEIGHT_ARGUMENT);
    const uint32_t                             width  = in_view.get_u32(WIDTH_ARGUMENT);

    return describe_surface(width,
                            height,
                            format,
                            0,
                            Interception::Hooks::D3D8::D3DPOOL_SYSTEMMEM);
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_index_buffer(const CallView& in_view)
{
    constexpr size_t FORMAT_ARGUMENT = 3;
    constexpr size_t LENGTH_ARGUMENT = 1;
    constexpr size_t POOL_ARGUMENT   = 4;
    constexpr size_t USAGE_ARGUMENT  = 2;

    const Interception::Hooks::D3D8::D3DPOOL       pool   = static_cast<Interception::Hooks::D3D8::D3DPOOL>(in_view.get_u32(POOL_ARGUMENT) );
    Analysis::APIDumpAnalysis::ResourceDescription result;
    const uint32_t                                 usage  = in_view.get_u32(USAGE_ARGUMENT);

    result.api_flags        = compute_api_flags(pool,
                                                usage);
    result.format.raw_value = in_view.get_u32  (FORMAT_ARGUMENT);
    result.is_dynamic       = ((usage & Interception::Hooks::D3D8::D3DUSAGE_DYNAMIC) != 0);
    result.kind             = Analysis::APIDumpAnalysis::ResourceKind::BUFFER_INDEX;
    result.size_in_bytes    = in_view.get_u32(LENGTH_ARGUMENT);

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_render_target(const CallView& in_view)
{
    constexpr size_t FORMAT_ARGUMENT       = 3;
    constexpr size_t HEIGHT_ARGUMENT       = 2;
    constexpr size_t MULTI_SAMPLE_ARGUMENT = 4;
    constexpr size_t WIDTH_ARGUMENT        = 1;

    const Interception::Hooks::D3D8::D3DFORMAT           format            = static_cast<Interception::Hooks::D3D8::D3DFORMAT>         (in_view.get_u32(FORMAT_ARGUMENT) );
    const uint32_t                                       height            = in_view.get_u32(HEIGHT_ARGUMENT);
    const Interception::Hooks::D3D8::D3DMULTISAMPLE_TYPE multi_sample_type = static_cast<Interception::Hooks::D3D8::D3DMULTISAMPLE_TYPE>(in_view.get_u32(MULTI_SAMPLE_ARGUMENT) );
    const uint32_t                                       width             = in_view.get_u32(WIDTH_ARGUMENT);
    auto                                                 result            = describe_surface(width,
                                                                                              height,
                                                                                              format,
                                                                                              Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET,
                                                                                              Interception::Hooks::D3D8::D3DPOOL_DEFAULT);

    result.sample_count = get_n_samples(multi_sample_type);

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_surface(const uint32_t&                             in_width,
                                                                                                                             const uint32_t&                             in_height,
                                                                                                                             const Interception::Hooks::D3D8::D3DFORMAT& in_format,
                                                                                                                             const uint32_t&                             in_usage,
                                                                                                                             const Interception::Hooks::D3D8::D3DPOOL&   in_pool)
{
    Analysis::APIDumpAnalysis::ResourceDescription result;

    result.api_flags        = compute_api_flags(in_pool,
                                                in_usage);
    result.format           = convert_format   (in_format);
    result.height           = in_height;
    result.is_depth_stencil = ((in_usage & Interception::Hooks::D3D8::D3DUSAGE_DEPTHSTENCIL) != 0);
    result.is_dynamic       = ((in_usage & Interception::Hooks::D3D8::D3DUSAGE_DYNAMIC)      != 0);
    result.is_render_target = ((in_usage & Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET) != 0);
    result.kind             = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D;
    result.width            = in_width;

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_texture(const CallView& in_view)
{
    constexpr size_t FORMAT_ARGUMENT = 5;
    constexpr size_t HEIGHT_ARGUMENT = 2;
    constexpr size_t LEVELS_ARGUMENT = 3;
    constexpr size_t POOL_ARGUMENT   = 6;
    constexpr size_t USAGE_ARGUMENT  = 4;
    constexpr size_t WIDTH_ARGUMENT  = 1;

    const Interception::Hooks::D3D8::D3DFORMAT format      = static_cast<Interception::Hooks::D3D8::D3DFORMAT>(in_view.get_u32(FORMAT_ARGUMENT) );
    const uint32_t                             height      = in_view.get_u32(HEIGHT_ARGUMENT);
    const uint32_t                             level_count = in_view.get_u32(LEVELS_ARGUMENT);
    const Interception::Hooks::D3D8::D3DPOOL   pool        = static_cast<Interception::Hooks::D3D8::D3DPOOL>  (in_view.get_u32(POOL_ARGUMENT) );
    const uint32_t                             usage       = in_view.get_u32(USAGE_ARGUMENT);
    const uint32_t                             width       = in_view.get_u32(WIDTH_ARGUMENT);
    auto                                       result      = describe_surface(width,
                                                                              height,
                                                                              format,
                                                                              usage,
                                                                              pool);

    result.mip_level_count = MipChain::get_level_count(level_count,
                                                       result.width,
                                                       result.height);

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_vertex_buffer(const CallView& in_view)
{
    constexpr size_t LENGTH_ARGUMENT = 1;
    constexpr size_t POOL_ARGUMENT   = 4;
    constexpr size_t USAGE_ARGUMENT  = 2;

    const Interception::Hooks::D3D8::D3DPOOL       pool   = static_cast<Interception::Hooks::D3D8::D3DPOOL>(in_view.get_u32(POOL_ARGUMENT) );
    Analysis::APIDumpAnalysis::ResourceDescription result;
    const uint32_t                                 usage  = in_view.get_u32(USAGE_ARGUMENT);

    result.api_flags     = compute_api_flags(pool,
                                             usage);
    result.is_dynamic    = ((usage & Interception::Hooks::D3D8::D3DUSAGE_DYNAMIC) != 0);
    result.kind          = Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX;
    result.size_in_bytes = in_view.get_u32(LENGTH_ARGUMENT);

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::describe_volume_texture(const CallView& in_view)
{
    constexpr size_t DEPTH_ARGUMENT  = 3;
    constexpr size_t FORMAT_ARGUMENT = 6;
    constexpr size_t HEIGHT_ARGUMENT = 2;
    constexpr size_t LEVELS_ARGUMENT = 4;
    constexpr size_t POOL_ARGUMENT   = 7;
    constexpr size_t USAGE_ARGUMENT  = 5;
    constexpr size_t WIDTH_ARGUMENT  = 1;

    const Interception::Hooks::D3D8::D3DFORMAT format      = static_cast<Interception::Hooks::D3D8::D3DFORMAT>(in_view.get_u32(FORMAT_ARGUMENT) );
    const uint32_t                             height      = in_view.get_u32(HEIGHT_ARGUMENT);
    const uint32_t                             level_count = in_view.get_u32(LEVELS_ARGUMENT);
    const Interception::Hooks::D3D8::D3DPOOL   pool        = static_cast<Interception::Hooks::D3D8::D3DPOOL>  (in_view.get_u32(POOL_ARGUMENT) );
    const uint32_t                             usage       = in_view.get_u32(USAGE_ARGUMENT);
    const uint32_t                             width       = in_view.get_u32(WIDTH_ARGUMENT);
    auto                                       result      = describe_surface(width,
                                                                              height,
                                                                              format,
                                                                              usage,
                                                                              pool);

    result.depth_or_layer_count = in_view.get_u32(DEPTH_ARGUMENT);
    result.kind                 = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_3D;
    result.mip_level_count      = MipChain::get_level_count(level_count,
                                                            result.width,
                                                            result.height);

    return result;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::D3D8ResourceDescriber::get_n_samples(const Interception::Hooks::D3D8::D3DMULTISAMPLE_TYPE& in_type)
{
    /* D3DMULTISAMPLE_NONE is 0, and the others are the number of samples. */
    return (in_type == Interception::Hooks::D3D8::D3DMULTISAMPLE_NONE) ? 1
                                                                       : static_cast<uint32_t>(in_type);
}
