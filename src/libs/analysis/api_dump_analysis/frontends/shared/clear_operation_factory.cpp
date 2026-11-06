/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/shared/clear_operation_factory.h"

Analysis::APIDumpAnalysis::ClearOperation Analysis::APIDumpAnalysis::Frontends::ClearOperationFactory::create(const bool&     in_clears_color,
                                                                                                              const bool&     in_clears_depth,
                                                                                                              const bool&     in_clears_stencil,
                                                                                                              const uint32_t& in_color_argb,
                                                                                                              const float&    in_depth,
                                                                                                              const uint32_t& in_stencil)
{
    constexpr uint32_t ALPHA_SHIFT = 24;
    constexpr uint32_t BYTE_MASK   = 0xFF;
    constexpr float    BYTE_MAX    = 255.0f;
    constexpr uint32_t GREEN_SHIFT = 8;
    constexpr uint32_t RED_SHIFT   = 16;

    Analysis::APIDumpAnalysis::ClearOperation result;

    result.clears_color   = in_clears_color;
    result.clears_depth   = in_clears_depth;
    result.clears_stencil = in_clears_stencil;
    result.color_fp32vec4 = {static_cast<float>((in_color_argb >> RED_SHIFT)   & BYTE_MASK) / BYTE_MAX,
                             static_cast<float>((in_color_argb >> GREEN_SHIFT) & BYTE_MASK) / BYTE_MAX,
                             static_cast<float>( in_color_argb                 & BYTE_MASK) / BYTE_MAX,
                             static_cast<float>((in_color_argb >> ALPHA_SHIFT) & BYTE_MASK) / BYTE_MAX};
    result.depth          = in_depth;
    result.stencil        = in_stencil;

    return result;
}
