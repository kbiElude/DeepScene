/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d8_replay_context.h"
#include "Interception/Hooks/D3D8/types.h"
#include <algorithm>

Replay::StereoReplayEngineDLL::D3D8ReplayContext::D3D8ReplayContext(ObjectRemapper* in_remapper_ptr)
    :CallContext           (in_remapper_ptr),
     m_device_ptr          (nullptr),
     m_is_creation_expected(false)
{
    /* Stub */
}

void Replay::StereoReplayEngineDLL::D3D8ReplayContext::begin_call(const Capture::APIDumpLoader::DumpedAPICall* in_call_ptr,
                                                                  const IChunkSource*                          in_chunk_source_ptr,
                                                                  const size_t&                                in_n_head_arguments)
{
    CallContext::begin_call(in_call_ptr,
                            in_chunk_source_ptr,
                            in_n_head_arguments);

    m_is_creation_expected = false;
}

void Replay::StereoReplayEngineDLL::D3D8ReplayContext::expect_creation()
{
    m_is_creation_expected = true;
}

void Replay::StereoReplayEngineDLL::D3D8ReplayContext::forget_object(const uint64_t& in_dumped_value)
{
    uint64_t live_value = 0;

    if (get_remapper_ptr()->get(ObjectKind::D3D8_OBJECT,
                                in_dumped_value,
                               &live_value) )
    {
        if (reinterpret_cast<void*>(live_value) == m_device_ptr)
        {
            m_device_ptr = nullptr;
        }

        m_live_objects.remove(reinterpret_cast<void*>(live_value) );
    }

    get_remapper_ptr()->remove(ObjectKind::D3D8_OBJECT,
                               in_dumped_value);
}

void* Replay::StereoReplayEngineDLL::D3D8ReplayContext::get_device_ptr() const
{
    return m_device_ptr;
}

const uint8_t* Replay::StereoReplayEngineDLL::D3D8ReplayContext::get_extra_data_ptr(const size_t& in_n_argument,
                                                                                    size_t*       out_n_bytes_ptr) const
{
    static const ExtraBinding EXTRA_BINDING_ARRAY[] =
    {
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR,                   2, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS,               2, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS,               5, 1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEPIXELSHADER,       1, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVERTEXSHADER,      1, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVERTEXSHADER,      2, 1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP,  5, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP,  7, 1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP,         3, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWRECTPATCH,           2, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWTRIPATCH,            2, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT,                 1, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT,                 2, 1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETCLIPPLANE,            2, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETPALETTEENTRIES,       2, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETPIXELSHADERCONSTANT,  2, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADERCONSTANT, 2, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSWAPCHAIN8_PRESENT,              1, 0},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSWAPCHAIN8_PRESENT,              2, 1},
    };

    const uint8_t* result_ptr = nullptr;

    *out_n_bytes_ptr = 0;

    for (const auto& binding : EXTRA_BINDING_ARRAY)
    {
        if (binding.function   == get_call().func &&
            binding.n_argument == in_n_argument)
        {
            result_ptr = get_chunk_data_ptr(get_in_argument_ptr(get_n_head_arguments() + binding.n_extra_argument),
                                            out_n_bytes_ptr);

            break;
        }
    }

    return result_ptr;
}

const Replay::StereoReplayEngineDLL::D3D8ReplayContext::HandleBinding* Replay::StereoReplayEngineDLL::D3D8ReplayContext::get_handle_binding_ptr(const APIInterceptor::APIFunction& in_function,
                                                                                                                                          const size_t&                      in_n_argument,
                                                                                                                                          const bool&                        in_is_output)
{
    static const HandleBinding HANDLE_BINDING_ARRAY[] =
    {
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_APPLYSTATEBLOCK,    false, ObjectKind::D3D8_STATE_BLOCK,   1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CAPTURESTATEBLOCK,  false, ObjectKind::D3D8_STATE_BLOCK,   1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEPIXELSHADER,  true,  ObjectKind::D3D8_PIXEL_SHADER,  2},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATESTATEBLOCK,   true,  ObjectKind::D3D8_STATE_BLOCK,   2},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVERTEXSHADER, true,  ObjectKind::D3D8_VERTEX_SHADER, 3},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETEPIXELSHADER,  false, ObjectKind::D3D8_PIXEL_SHADER,  1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETESTATEBLOCK,   false, ObjectKind::D3D8_STATE_BLOCK,   1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETEVERTEXSHADER, false, ObjectKind::D3D8_VERTEX_SHADER, 1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_ENDSTATEBLOCK,      true,  ObjectKind::D3D8_STATE_BLOCK,   1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETPIXELSHADER,     false, ObjectKind::D3D8_PIXEL_SHADER,  1},
        {APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADER,    false, ObjectKind::D3D8_VERTEX_SHADER, 1},
    };

    const HandleBinding* result_ptr = nullptr;

    for (const auto& binding : HANDLE_BINDING_ARRAY)
    {
        if (binding.function   == in_function   &&
            binding.n_argument == in_n_argument &&
            binding.is_output  == in_is_output)
        {
            result_ptr = &binding;

            break;
        }
    }

    return result_ptr;
}

bool Replay::StereoReplayEngineDLL::D3D8ReplayContext::get_live_handle(const ObjectKind&                          in_kind,
                                                                       const APIInterceptor::APIFunctionArgument& in_argument,
                                                                       uint64_t*                                  out_live_value_ptr) const
{
    return get_remapper_ptr()->get(in_kind,
                                   in_argument.get_ptr_as_u64(),
                                   out_live_value_ptr);
}

bool Replay::StereoReplayEngineDLL::D3D8ReplayContext::get_live_object(const APIInterceptor::APIFunctionArgument& in_argument,
                                                                       void**                                     out_live_object_ptr,
                                                                       std::string*                               out_error_ptr) const
{
    uint64_t live_value = 0;
    bool     result     = get_live_handle(ObjectKind::D3D8_OBJECT,
                                          in_argument,
                                         &live_value);

    if (result)
    {
        *out_live_object_ptr = reinterpret_cast<void*>(live_value);
    }
    else
    {
        *out_error_ptr = "A D3D8 object which the replay has not created is used.";
    }

    return result;
}

bool Replay::StereoReplayEngineDLL::D3D8ReplayContext::is_creation_expected() const
{
    return m_is_creation_expected;
}

uint32_t Replay::StereoReplayEngineDLL::D3D8ReplayContext::map_handle_argument(const size_t&   in_n_argument,
                                                                               const uint32_t& in_dumped_value) const
{
    const auto binding_ptr = get_handle_binding_ptr(get_call().func,
                                                    in_n_argument,
                                                    false); /* in_is_output */
    uint64_t   live_value  = 0;
    uint32_t   result      = in_dumped_value;

    if (binding_ptr != nullptr)
    {
        if (get_remapper_ptr()->get(binding_ptr->kind,
                                    in_dumped_value,
                                   &live_value) )
        {
            result = static_cast<uint32_t>(live_value);
        }
    }

    return result;
}

void Replay::StereoReplayEngineDLL::D3D8ReplayContext::register_handle(const size_t&   in_n_argument,
                                                                       const uint32_t& in_live_value)
{
    const auto binding_ptr = get_handle_binding_ptr(get_call().func,
                                                    in_n_argument,
                                                    true); /* in_is_output */
    const auto out_ptr     = get_out_argument_ptr(in_n_argument);

    if (binding_ptr != nullptr &&
        out_ptr     != nullptr)
    {
        get_remapper_ptr()->add(binding_ptr->kind,
                                out_ptr->get_u32(),
                                in_live_value);
    }
}

void Replay::StereoReplayEngineDLL::D3D8ReplayContext::register_object(const uint64_t& in_dumped_value,
                                                                       void*           in_live_object_ptr)
{
    if (in_dumped_value != 0)
    {
        get_remapper_ptr()->add(ObjectKind::D3D8_OBJECT,
                                in_dumped_value,
                                reinterpret_cast<uint64_t>(in_live_object_ptr) );

        m_live_objects.add(in_live_object_ptr);
    }
}

void Replay::StereoReplayEngineDLL::D3D8ReplayContext::set_device_from_argument(const size_t& in_n_argument)
{
    uint64_t   live_value       = 0;
    const auto out_argument_ptr = get_out_argument_ptr(in_n_argument);

    if (out_argument_ptr != nullptr)
    {
        const bool is_live = get_remapper_ptr()->get(ObjectKind::D3D8_OBJECT,
                                                     out_argument_ptr->get_ptr_as_u64(),
                                                    &live_value);

        if (is_live)
        {
            m_device_ptr = reinterpret_cast<void*>(live_value);
        }
    }
}

void Replay::StereoReplayEngineDLL::D3D8ReplayContext::release_all_objects()
{
    constexpr size_t MAX_N_RELEASES_PER_OBJECT = 4096; /* sanity limit */

    typedef ULONG (__stdcall* PFNRELEASEPROC)(void*);

    /* The device is the first one to be created, so it is the last one to go. */
    for (void* const object_ptr : m_live_objects.get_newest_first_vec() )
    {
        void** const   vtable_ptr       = *reinterpret_cast<void***>      (object_ptr);
        const auto     release_func_ptr = reinterpret_cast<PFNRELEASEPROC>(vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_RELEASE]);

        size_t n_release = 0;

        while (n_release < MAX_N_RELEASES_PER_OBJECT &&
               release_func_ptr(object_ptr) > 0)
        {
            ++n_release;
        }
    }

    m_device_ptr = nullptr;

    m_live_objects.clear();
}
