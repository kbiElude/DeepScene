/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/shared/fixed_function_vertex_format.h"
#include "interception/core/D3D7Utils/constants.h"
#include "interception/hooks/D3D8/types.h"

/* The two APIs say the same by the same bits: the one answer below is the answer for both. */
static_assert(Interception::Core::D3D7Utils::D3DFVF_POSITION_MASK == Interception::Hooks::D3D8::D3DFVF_POSITION_MASK,
              "Direct3D 7 and 8 must mask the position of a vertex format alike");
static_assert(Interception::Core::D3D7Utils::D3DFVF_XYZRHW == Interception::Hooks::D3D8::D3DFVF_XYZRHW,
              "Direct3D 7 and 8 must mark vertices in pixels alike");

bool Analysis::APIDumpAnalysis::Frontends::FixedFunctionVertexFormat::is_pre_transformed(const uint32_t& in_format)
{
    return ((in_format & Interception::Core::D3D7Utils::D3DFVF_POSITION_MASK) == Interception::Core::D3D7Utils::D3DFVF_XYZRHW);
}
