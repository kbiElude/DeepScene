/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/matrix_slots.h"

uint32_t Stereo::Core::MatrixSlots::bind(const Analysis::Recipe::DrawClass& in_draw_class,
                                         const Analysis::Recipe::Eye&       in_eye,
                                         const float&                       in_convergence_distance,
                                         const float&                       in_eye_separation,
                                         const bool&                        in_is_final_target,
                                         const float&                       in_overlay_parallax,
                                         std::vector<MatrixChange>*         inout_change_vec_ptr)
{
    uint32_t   n_changes  = 0;
    const bool is_overlay = (in_draw_class       != Analysis::Recipe::DrawClass::AFFECTED &&
                             in_is_final_target  &&
                             in_overlay_parallax >  0.0f);
    const bool is_left    = (in_eye == Analysis::Recipe::Eye::LEFT);

    Bound wanted = Bound::MONO;

    if (in_draw_class == Analysis::Recipe::DrawClass::AFFECTED)
    {
        wanted = (is_left) ? Bound::LEFT
                           : Bound::RIGHT;
    }
    else
    if (is_overlay)
    {
        wanted = (is_left) ? Bound::OVERLAY_LEFT
                           : Bound::OVERLAY_RIGHT;
    }

    for (auto& slot_pair : m_slot_map)
    {
        Slot& slot           = slot_pair.second;
        bool  is_bound_right = (slot.bound == wanted);

        /* A matrix made for another separation is not the one to have, though it is for the eye. */
        if (wanted == Bound::LEFT ||
            wanted == Bound::RIGHT)
        {
            is_bound_right = is_bound_right                                             &&
                             slot.bound_convergence_distance == in_convergence_distance &&
                             slot.bound_separation           == in_eye_separation;
        }
        else
        if (is_overlay)
        {
            is_bound_right = is_bound_right                                     &&
                             slot.bound_overlay_parallax == in_overlay_parallax;
        }

        if (is_bound_right == false)
        {
            bool is_for_eye = false;

            if (n_changes == inout_change_vec_ptr->size() )
            {
                inout_change_vec_ptr->emplace_back();
            }

            MatrixChange& change = inout_change_vec_ptr->at(n_changes);

            change.argument_vec.assign(slot.argument_vec.begin(),
                                       slot.argument_vec.end  () );

            change.function         = slot.function;
            change.matrix_fp32vec16 = slot.matrix_fp32vec16;

            if (wanted == Bound::LEFT ||
                wanted == Bound::RIGHT)
            {
                for (const auto* rule_ptr : slot.rule_ptr_vec)
                {
                    if (Analysis::Recipe::RecipeApplier::apply_rule(*rule_ptr,
                                                                    in_eye,
                                                                    in_eye_separation,
                                                                    &change.matrix_fp32vec16,
                                                                    in_convergence_distance) )
                    {
                        is_for_eye = true;
                    }
                }
            }
            else
            if (is_overlay)
            {
                for (const auto* rule_ptr : slot.rule_ptr_vec)
                {
                    if (Analysis::Recipe::RecipeApplier::apply_overlay_shift(*rule_ptr,
                                                                             in_eye,
                                                                             in_overlay_parallax,
                                                                             &change.matrix_fp32vec16) )
                    {
                        is_for_eye = true;
                    }
                }
            }

            slot.bound                      = (is_for_eye) ? wanted
                                                           : Bound::MONO;
            slot.bound_convergence_distance = in_convergence_distance;
            slot.bound_overlay_parallax     = in_overlay_parallax;
            slot.bound_separation           = in_eye_separation;

            ++n_changes;
        }
    }

    return n_changes;
}

void Stereo::Core::MatrixSlots::fill_slot_key(const APIInterceptor::APIFunction&                      in_function,
                                              const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                              SlotKey*                                                out_key_ptr)
{
    out_key_ptr->first = in_function;

    out_key_ptr->second.clear();

    for (const auto& argument : in_argument_vec)
    {
        if (argument.type == APIInterceptor::ARGTYPE_U32)
        {
            out_key_ptr->second.push_back(argument.get_u32() );
        }
    }
}

void Stereo::Core::MatrixSlots::forget()
{
    m_slot_map.clear();
}

bool Stereo::Core::MatrixSlots::on_matrix_call(const Analysis::Recipe::RecipeApplier&                  in_applier,
                                               const APIInterceptor::APIFunction&                      in_function,
                                               const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                               const std::array<float, 16>&                            in_matrix)
{
    bool result = false;
    Slot slot;


    if (in_applier.has_rule_for(in_function) == false)
    {
        goto end;
    }

    /* The places of a call are not known as it is made. */
    m_scratch_call.arg_in_vec.assign(in_argument_vec.begin(),
                                     in_argument_vec.end  () );

    m_scratch_call.func = in_function;

    fill_slot_key(in_function,
                  in_argument_vec,
                 &m_scratch_key);

    slot.rule_ptr_vec = in_applier.get_matching_rule_ptr_vec(m_scratch_call,
                                                             Analysis::Recipe::CallPlace() );

    /* Whatever this call replaces is not what the device is to have. */
    m_slot_map.erase(m_scratch_key);

    if (slot.rule_ptr_vec.empty() == false)
    {
        slot.argument_vec     = in_argument_vec;
        slot.function         = in_function;
        slot.matrix_fp32vec16 = in_matrix;

        m_slot_map[m_scratch_key] = slot;
        result                    = true;
    }

end:
    return result;
}
