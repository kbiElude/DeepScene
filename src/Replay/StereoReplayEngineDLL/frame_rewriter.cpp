/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_buffer.h"
#include "frame_rewriter.h"
#include <string.h>

Replay::StereoReplayEngineDLL::FrameRewriter::FrameRewriter(const Analysis::RecipeApplier::RecipeApplier& in_applier)
    :m_applier(in_applier)
{
    /* Stub */
}

bool Replay::StereoReplayEngineDLL::FrameRewriter::has_matching_rule(const Capture::APIDumpLoader::DumpedAPICall&     in_call,
                                                                     const Analysis::RecipeApplier::CallPlace& in_place) const
{
    return (m_applier.get_matching_rule_ptr_vec(in_call,
                                                in_place).empty() == false);
}

bool Replay::StereoReplayEngineDLL::FrameRewriter::rewrite(const Capture::APIDumpLoader::DumpedAPICall&     in_call,
                                                           const Analysis::RecipeApplier::CallPlace& in_place,
                                                           const Analysis::RecipeLoader::Eye&        in_eye,
                                                           const float&                                     in_eye_separation,
                                                           OverlayChunkSource*                              inout_overlay_ptr,
                                                           Capture::APIDumpLoader::DumpedAPICall*           out_call_ptr) const
{
    constexpr uint32_t NEW_MATRIX_CHUNK_ID = 0xfffffff0; /* no chunk of a workload has an ID this big */

    const auto            rule_ptr_vec = m_applier.get_matching_rule_ptr_vec(in_call,
                                                                             in_place);
    std::array<float, 16> matrix       = {};
    size_t                n_argument   = 0;
    bool                  result       = false;

    if (rule_ptr_vec.empty() )
    {
        goto end;
    }

    n_argument = rule_ptr_vec.front()->matrix_argument_index;

    if (n_argument                             >= in_call.arg_in_vec.size()             ||
        in_call.arg_in_vec.at(n_argument).type != APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        goto end;
    }

    {
        const auto* chunk_ptr = inout_overlay_ptr->get_data_chunk_ptr(in_call.arg_in_vec.at(n_argument).get_data_chunk_id() );

        if (chunk_ptr          == nullptr       ||
            chunk_ptr->size() != sizeof(matrix) )
        {
            goto end;
        }

        ::memcpy(matrix.data(),
                 chunk_ptr->data(),
                 sizeof(matrix) );
    }

    for (const auto rule_ptr : rule_ptr_vec)
    {
        result |= Analysis::RecipeApplier::RecipeApplier::apply_rule(*rule_ptr,
                                                                     in_eye,
                                                                     in_eye_separation,
                                                                    &matrix);
    }

    if (result)
    {
        Capture::APIDumpLoader::DataChunk new_chunk(sizeof(matrix) );

        ::memcpy(new_chunk.data(),
                 matrix.data(),
                 sizeof(matrix) );

        *out_call_ptr = FrameBuffer::copy_call(in_call);

        out_call_ptr->arg_in_vec.at(n_argument) = APIInterceptor::APIFunctionArgument::create_data_chunk_id(NEW_MATRIX_CHUNK_ID);

        inout_overlay_ptr->set_chunk(NEW_MATRIX_CHUNK_ID,
                                     new_chunk);
    }

end:
    return result;
}
