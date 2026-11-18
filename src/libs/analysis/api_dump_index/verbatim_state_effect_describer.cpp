/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/verbatim_state_effect_describer.h"
#include "capture/api_dump_loader/api_info.h"
#include "common/macros.h"

Analysis::APIDumpIndex::VerbatimStateEffectDescriber::VerbatimStateEffectDescriber(const Capture::APIDumpLoader::APIGroup& in_group)
    :m_group(in_group)
{
    AI_ASSERT_ALWAYS(in_group == Capture::APIDumpLoader::APIGroup::D3D7 ||
                     in_group == Capture::APIDumpLoader::APIGroup::D3D8,
                     "A verbatim describer is made for Direct3D 7 or 8 only.");
}

Analysis::APIDumpIndex::StateEffect Analysis::APIDumpIndex::VerbatimStateEffectDescriber::describe(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    StateEffect result;

    AI_ASSERT_ALWAYS(Capture::APIDumpLoader::get_api_group(in_call.func) == m_group,
                     "A call was given to the describer of another API.");

    result.kind = StateEffectKind::BARRIER;

    return result;
}

Capture::APIDumpLoader::APIGroup Analysis::APIDumpIndex::VerbatimStateEffectDescriber::get_group() const
{
    return m_group;
}
