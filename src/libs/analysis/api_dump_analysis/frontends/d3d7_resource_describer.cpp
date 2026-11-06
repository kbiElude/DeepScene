/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/d3d7_resource_describer.h"
#include "analysis/api_dump_analysis/frontends/mip_chain.h"
#include "utils/hashing/hasher.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Core/D3D7Utils/vertex_format.h"

Analysis::APIDumpAnalysis::Format Analysis::APIDumpAnalysis::Frontends::D3D7ResourceDescriber::convert_pixel_format(const Interception::Hooks::D3D7::DDPIXELFORMAT& in_format)
{
    Utils::Hashing::Hasher            hasher;
    Analysis::APIDumpAnalysis::Format result;

    if ( (in_format.dwFlags & Interception::Core::D3D7Utils::DDPF_ZBUFFER) != 0)
    {
        result.format_class = ( (in_format.dwFlags & Interception::Core::D3D7Utils::DDPF_STENCILBUFFER) != 0) ? Analysis::APIDumpAnalysis::FormatClass::DEPTH_STENCIL
                                                                                                                : Analysis::APIDumpAnalysis::FormatClass::DEPTH;
    }
    else
    if ( (in_format.dwFlags & Interception::Core::D3D7Utils::DDPF_FOURCC) != 0)
    {
        result.format_class = Analysis::APIDumpAnalysis::FormatClass::COMPRESSED;
    }
    else
    {
        result.format_class = Analysis::APIDumpAnalysis::FormatClass::COLOR;
    }

    /* What the format is, in a number: the code of a compressed format, or a digest of the flags, the size and the masks. */
    if ( (in_format.dwFlags & Interception::Core::D3D7Utils::DDPF_FOURCC) != 0)
    {
        result.raw_value = in_format.dwFourCC;
    }
    else
    {
        hasher.add_u32(in_format.dwFlags);
        hasher.add_u32(in_format.dwRGBBitCount);
        hasher.add_u32(in_format.dwRBitMask);
        hasher.add_u32(in_format.dwGBitMask);
        hasher.add_u32(in_format.dwBBitMask);
        hasher.add_u32(in_format.dwRGBAlphaBitMask);

        result.raw_value = static_cast<uint32_t>(hasher.get_digest() );
    }

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D7ResourceDescriber::describe_back_buffer(const Analysis::APIDumpAnalysis::ResourceDescription& in_primary)
{
    Analysis::APIDumpAnalysis::ResourceDescription result = in_primary;

    result.is_render_target = true;

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D7ResourceDescriber::describe_surface(const Interception::Core::D3D7Utils::SurfaceDescription& in_description,
                                                                                                                             const uint32_t&                                          in_screen_width,
                                                                                                                             const uint32_t&                                          in_screen_height)
{
    constexpr uint32_t CAPS2_SHIFT = 32;

    const uint32_t                                 caps            = in_description.caps.dwCaps;
    const uint32_t                                 flags           = in_description.flags;
    const bool                                     is_3d_device    = ( (caps & Interception::Core::D3D7Utils::DDSCAPS_3DDEVICE)    != 0);
    const bool                                     is_depth_buffer = ( (caps & Interception::Core::D3D7Utils::DDSCAPS_ZBUFFER)     != 0);
    const bool                                     is_swap_chain   = ( (caps & (Interception::Core::D3D7Utils::DDSCAPS_BACKBUFFER |
                                                                                Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE)) != 0);
    Analysis::APIDumpAnalysis::ResourceDescription result;

    result.api_flags        = static_cast<uint64_t>(caps) | (static_cast<uint64_t>(in_description.caps.dwCaps2) << CAPS2_SHIFT);
    result.height           = ( (flags & Interception::Core::D3D7Utils::DDSD_HEIGHT) != 0) ? in_description.height
                                                                                              : in_screen_height;
    result.is_depth_stencil = is_depth_buffer;
    result.is_render_target = (is_depth_buffer == false &&
                               (is_swap_chain  ||
                                is_3d_device) );
    result.kind             = (is_swap_chain) ? Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE
                                              : Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D;
    result.width            = ( (flags & Interception::Core::D3D7Utils::DDSD_WIDTH) != 0) ? in_description.width
                                                                                             : in_screen_width;

    if ( (flags & Interception::Core::D3D7Utils::DDSD_PIXELFORMAT) != 0)
    {
        result.format = convert_pixel_format(in_description.pixel_format);
    }
    else
    {
        result.format.format_class = Analysis::APIDumpAnalysis::FormatClass::COLOR;
    }

    /* A mip map chain has the number of levels it says, or all of them down to 1x1 if it does not say. */
    if ( (caps & Interception::Core::D3D7Utils::DDSCAPS_MIPMAP) != 0)
    {
        const uint32_t n_levels_asked = ( (flags & Interception::Core::D3D7Utils::DDSD_MIPMAPCOUNT) != 0) ? in_description.mip_map_count
                                                                                                             : 0;

        result.mip_level_count = MipChain::get_level_count(n_levels_asked,
                                                           result.width,
                                                           result.height);
    }

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::Frontends::D3D7ResourceDescriber::describe_vertex_buffer(const Interception::Hooks::D3D7::D3DVERTEXBUFFERDESC& in_description)
{
    Analysis::APIDumpAnalysis::ResourceDescription result;

    result.api_flags     = in_description.dwCaps;
    result.kind          = Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX;
    result.size_in_bytes = static_cast<uint64_t>(in_description.dwNumVertices) * Interception::Core::D3D7Utils::VertexFormat::get_n_vertex_bytes(in_description.dwFVF);

    return result;
}
