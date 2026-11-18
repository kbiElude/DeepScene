/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/gl_state_effect_describer.h"
#include "analysis/api_dump_index/state_effect_describer_set.h"
#include "analysis/api_dump_index/verbatim_state_effect_describer.h"
#include "analysis/api_dump_index/window_system_state_effect_describer.h"
#include "capture/api_dump_loader/api_info.h"
#include "common/macros.h"

Analysis::APIDumpIndex::StateEffectDescriberSet::StateEffectDescriberSet()
{
    m_describer_ptr_vec.emplace_back(new VerbatimStateEffectDescriber(Capture::APIDumpLoader::APIGroup::D3D7) );
    m_describer_ptr_vec.emplace_back(new VerbatimStateEffectDescriber(Capture::APIDumpLoader::APIGroup::D3D8) );
    m_describer_ptr_vec.emplace_back(new WindowSystemStateEffectDescriber(Capture::APIDumpLoader::APIGroup::GDI32) );
    m_describer_ptr_vec.emplace_back(new GLStateEffectDescriber() );
    m_describer_ptr_vec.emplace_back(new WindowSystemStateEffectDescriber(Capture::APIDumpLoader::APIGroup::USER32) );
    m_describer_ptr_vec.emplace_back(new WindowSystemStateEffectDescriber(Capture::APIDumpLoader::APIGroup::WGL) );
}

Analysis::APIDumpIndex::StateEffect Analysis::APIDumpIndex::StateEffectDescriberSet::describe(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    const Capture::APIDumpLoader::APIGroup group = Capture::APIDumpLoader::get_api_group(in_call.func);

    IStateEffectDescriber* describer_ptr = nullptr;

    for (const auto& candidate_ptr : m_describer_ptr_vec)
    {
        if (candidate_ptr->get_group() == group)
        {
            describer_ptr = candidate_ptr.get();

            break;
        }
    }

    AI_ASSERT_ALWAYS(describer_ptr != nullptr,
                     "A call is of an API that has no state effect describer.");

    return describer_ptr->describe(in_call);
}
