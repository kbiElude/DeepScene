/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo_d3d7_call_executor.h"

Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::StereoD3D7CallExecutor(::Replay::ReplayEngineDLL::ObjectRemapper* in_remapper_ptr)
    :D3D7CallExecutor  (in_remapper_ptr),
     m_eye             (Analysis::RecipeLoader::Eye::LEFT),
     m_is_building_twin(false),
     m_needs_twin      (false)
{
    /* Stub */
}

bool Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::after_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                   std::string*                                 out_error_ptr)
{
    bool is_setup = false;
    bool result   = true;

    /* What sets up an object that each eye has a copy of is made for both: the call that makes the copy, and the ones that follow it. The
     * calls that are made in each execution of a frame are made for the eye of the execution.
     */
    if (m_is_building_twin == false)
    {
        is_setup     = (m_needs_twin                                                                                        ||
                        (get_execution_class(in_call) == ::Replay::ReplayEngineDLL::ExecutionClass::FIRST_TIME_ONLY &&
                         involves_per_eye_object() ) );
        m_needs_twin = false;
    }

    if (is_setup)
    {
        const Analysis::RecipeLoader::Eye eye = m_eye;

        m_eye              = Analysis::RecipeLoader::Eye::RIGHT;
        m_is_building_twin = true;

        result = run_handler(in_call.func,
                             out_error_ptr);

        m_is_building_twin = false;
        m_eye              = eye;
    }

    return result;
}

void* Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::find_right_twin(void* in_left_object_ptr) const
{
    const auto twin_iterator = m_right_twin_map.find(in_left_object_ptr);

    return (twin_iterator != m_right_twin_map.end() ) ? twin_iterator->second
                                                      : nullptr;
}

bool Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::involves_per_eye_object() const
{
    const auto& call   = get_call_context().get_call();
    bool        result = false;

    if (m_right_twin_map.empty() == false)
    {
        for (size_t n_argument = 0;
                    n_argument < call.arg_in_vec.size() && result == false;
                  ++n_argument)
        {
            const auto& argument   = call.arg_in_vec.at(n_argument);
            uint64_t    live_value = 0;

            if (argument.type                 == APIInterceptor::ARGTYPE_VOID_PTR &&
                argument.get_ptr_as_u64()     != 0                                &&
                get_call_context().get_remapper_ptr()->get(::Replay::ReplayEngineDLL::ObjectKind::D3D7_OBJECT,
                                                           argument.get_ptr_as_u64(),
                                                          &live_value) )
            {
                result = (find_right_twin(reinterpret_cast<void*>(live_value) ) != nullptr);
            }
        }
    }

    return result;
}

void Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::on_object_gone(void* in_object_ptr)
{
    m_right_twin_map.erase(in_object_ptr);
}

void Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::on_object_registered(const uint64_t& in_dumped_value,
                                                                             void*           in_live_object_ptr)
{
    (void) in_live_object_ptr;

    /* Asked when the object is made, which is when the copy is: it is made by the call being made again, for the right eye. */
    if (m_per_eye_predicate                    &&
        m_per_eye_predicate(in_dumped_value) )
    {
        m_needs_twin = true;
    }
}

void Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::on_object_released(void* in_object_ptr)
{
    /* The copy of the other eye is let go of with it. */
    void* const twin_ptr = find_right_twin(in_object_ptr);

    if (twin_ptr != nullptr)
    {
        release_object_reference(twin_ptr);
    }
}

void Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::on_reset()
{
    m_right_twin_map.clear();
}

void* Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::resolve_object(void* in_object_ptr) const
{
    void* const twin_ptr = (m_eye            == Analysis::RecipeLoader::Eye::RIGHT &&
                            in_object_ptr    != nullptr)                           ? find_right_twin(in_object_ptr)
                                                                                   : nullptr;

    return (twin_ptr != nullptr) ? twin_ptr
                                 : in_object_ptr;
}

void Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::set_eye(const Analysis::RecipeLoader::Eye& in_eye)
{
    /* The device keeps what was set on it by the eye before: the target and the textures must be those of this eye, or that eye
     * would draw into (and read) the other's copy.
     */
    const bool is_changing = (m_eye != in_eye);

    m_eye = in_eye;

    if (is_changing                &&
        get_device_ptr() != nullptr)
    {
        void* const render_target_ptr = get_render_target_ptr();

        if (render_target_ptr != nullptr)
        {
            void* const twin_ptr = find_right_twin(render_target_ptr);

            if (twin_ptr != nullptr)
            {
                set_device_render_target( (m_eye == Analysis::RecipeLoader::Eye::RIGHT) ? twin_ptr
                                                                                        : render_target_ptr);
            }
        }

        for (const auto& stage_iterator : get_stage_texture_map() )
        {
            void* const twin_ptr = (stage_iterator.second != nullptr) ? find_right_twin(stage_iterator.second)
                                                                      : nullptr;

            if (twin_ptr != nullptr)
            {
                set_device_texture(stage_iterator.first,
                                   (m_eye == Analysis::RecipeLoader::Eye::RIGHT) ? twin_ptr
                                                                                 : stage_iterator.second);
            }
        }
    }
}

void Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::set_per_eye_predicate(const PerEyePredicate& in_predicate)
{
    m_per_eye_predicate = in_predicate;
}

bool Stereo::Replay::EngineDLL::StereoD3D7CallExecutor::take_object(const uint64_t& in_dumped_value,
                                                                    void*           in_live_object_ptr)
{
    uint64_t left_value = 0;
    bool     result     = false;

    /* The object is the copy of the right eye of the one that the call made for the left eye. */
    if (m_is_building_twin)
    {
        if (get_call_context().get_remapper_ptr()->get(::Replay::ReplayEngineDLL::ObjectKind::D3D7_OBJECT,
                                                       in_dumped_value,
                                                      &left_value) )
        {
            m_right_twin_map[reinterpret_cast<void*>(left_value)] = in_live_object_ptr;
        }

        result = true;
    }

    return result;
}
