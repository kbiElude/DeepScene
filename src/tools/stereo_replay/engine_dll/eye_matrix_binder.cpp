/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "eye_matrix_binder.h"

Stereo::Replay::EngineDLL::EyeMatrixBinder::Slot::Slot()
    :bound                         (EyeMatrixBinding::NOTHING),
     bound_convergence_distance    (0.0f),
     bound_separation              (0.0f),
     has_rewritten_call            (false),
     rewritten_convergence_distance(0.0f),
     rewritten_eye                 (Analysis::Recipe::Eye::LEFT),
     rewritten_separation          (0.0f)
{
    /* Stub */
}

Stereo::Replay::EngineDLL::EyeMatrixBinder::EyeMatrixBinder(const FrameRewriter& in_rewriter,
                                                            const ExecuteFunc&   in_execute_func)
    :m_execute_func(in_execute_func),
     m_rewriter    (in_rewriter)
{
    /* Stub */
}

bool Stereo::Replay::EngineDLL::EyeMatrixBinder::before_draw(const bool&                  in_is_affected,
                                                             const Analysis::Recipe::Eye& in_eye,
                                                             const float&                 in_eye_separation,
                                                             const float&                 in_convergence_distance)
{
    bool                   result = true;
    const EyeMatrixBinding wanted = (in_is_affected == false)                       ? EyeMatrixBinding::MONO
                                  : (in_eye         == Analysis::Recipe::Eye::LEFT) ? EyeMatrixBinding::LEFT
                                                                                    : EyeMatrixBinding::RIGHT;

    for (auto& slot_pair : m_slot_map)
    {
        Slot& slot = *slot_pair.second;

        /* A matrix that was made for a different separation or convergence distance is stale, even if it is bound for the right eye. */
        if (slot.bound                        != wanted                     ||
            (wanted                           != EyeMatrixBinding::MONO     &&
             (slot.bound_separation           != in_eye_separation          ||
              slot.bound_convergence_distance != in_convergence_distance) ) )
        {
            bool is_for_eye = false;

            if (wanted != EyeMatrixBinding::MONO)
            {
                /* The rewritten call is reused if it was made for this eye, separation and convergence distance; otherwise it is made again. */
                is_for_eye = (slot.has_rewritten_call                                          &&
                              slot.rewritten_eye                  == in_eye                    &&
                              slot.rewritten_separation           == in_eye_separation         &&
                              slot.rewritten_convergence_distance == in_convergence_distance);

                if (is_for_eye == false)
                {
                    is_for_eye = rewrite(in_eye,
                                         in_eye_separation,
                                         in_convergence_distance,
                                        &slot);
                }
            }

            if (is_for_eye)
            {
                result = m_execute_func(slot.rewritten_call,
                                        *slot.overlay);
            }
            else
            {
                result = m_execute_func(slot.buffer.get_call_vec().front(),
                                        slot.buffer);
            }

            slot.bound                      = (is_for_eye) ? wanted
                                                           : EyeMatrixBinding::MONO;
            slot.bound_convergence_distance = in_convergence_distance;
            slot.bound_separation           = in_eye_separation;

            if (result == false)
            {
                break;
            }
        }
    }

    return result;
}

void Stereo::Replay::EngineDLL::EyeMatrixBinder::forget()
{
    m_slot_map.clear();
}

Stereo::Replay::EngineDLL::EyeMatrixBinder::SlotKey Stereo::Replay::EngineDLL::EyeMatrixBinder::get_slot_key(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    SlotKey result;

    result.first = in_call.func;

    for (const auto& argument : in_call.arg_in_vec)
    {
        if (argument.type == APIInterceptor::ARGTYPE_U32)
        {
            result.second.push_back(argument.get_u32() );
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::EyeMatrixBinder::on_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                         const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                         const Analysis::Recipe::CallPlace&             in_place,
                                                         const Analysis::Recipe::Eye&                   in_eye,
                                                         const float&                                   in_eye_separation,
                                                         const float&                                   in_convergence_distance)
{
    SlotKey key;
    bool    result = false;
    auto    slot   = std::unique_ptr<Slot>();

    /* Most calls to a function that has a rule do not match the rule. They need no copy; they only replace the slot of an earlier call that did
     * match. */
    if (m_rewriter.has_matching_rule(in_call,
                                     in_place) == false)
    {
        if (m_slot_map.empty() == false)
        {
            m_slot_map.erase(get_slot_key(in_call) );
        }

        goto end;
    }

    key  = get_slot_key(in_call);

    slot.reset(new Slot() );

    slot->buffer.add_call(in_call,
                          in_chunk_source);

    slot->overlay.reset(new OverlayChunkSource(slot->buffer) );
    slot->place = in_place;

    result = rewrite(in_eye,
                     in_eye_separation,
                     in_convergence_distance,
                     slot.get() );

    /* Drop the slot of the earlier call that this call replaces; the device must not use it. */
    m_slot_map.erase(key);

    if (result)
    {
        m_slot_map[key] = std::move(slot);
    }

end:
    return result;
}

bool Stereo::Replay::EngineDLL::EyeMatrixBinder::rewrite(const Analysis::Recipe::Eye& in_eye,
                                                         const float&                 in_eye_separation,
                                                         const float&                 in_convergence_distance,
                                                         Slot*                        inout_slot_ptr) const
{
    inout_slot_ptr->has_rewritten_call             = m_rewriter.rewrite(inout_slot_ptr->buffer.get_call_vec().front(),
                                                                        inout_slot_ptr->place,
                                                                        in_eye,
                                                                        in_eye_separation,
                                                                        inout_slot_ptr->overlay.get(),
                                                                       &inout_slot_ptr->rewritten_call,
                                                                        in_convergence_distance);
    inout_slot_ptr->rewritten_convergence_distance = in_convergence_distance;
    inout_slot_ptr->rewritten_eye                  = in_eye;
    inout_slot_ptr->rewritten_separation           = in_eye_separation;

    return inout_slot_ptr->has_rewritten_call;
}
