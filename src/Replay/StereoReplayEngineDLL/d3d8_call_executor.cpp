/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d8_call_executor.h"
#include "Interception/Hooks/D3D8/types.h"
#include "d3d8_replay_invoker.h"
#include <string.h>

Replay::StereoReplayEngineDLL::D3D8CallExecutor::D3D8CallExecutor(ObjectRemapper* in_remapper_ptr)
    :m_context      (in_remapper_ptr),
     m_d3d8_module  (nullptr),
     m_lock_replayer(&m_context)
{
    /* Every method of the method table. */
    #define D3D8_METHOD(INTERFACE_TAG, interface_name, METHOD_TAG, method_name, vtable_index, return_type, parameters) \
        if (::strcmp(#method_name, "Release") == 0) \
        { \
            m_release_function_set.insert(APIInterceptor::APIFUNCTION_D3D8_##INTERFACE_TAG##_##METHOD_TAG); \
        } \
        \
        if (is_ignored_method(#method_name) == false) \
        { \
            typedef D3D8Replay::Invoker<Interception::Hooks::D3D8::PFN_##INTERFACE_TAG##_##METHOD_TAG, vtable_index> MethodReplay; \
            \
            m_invoker_map[APIInterceptor::APIFUNCTION_D3D8_##INTERFACE_TAG##_##METHOD_TAG] = {&MethodReplay::invoke, MethodReplay::get_n_arguments()}; \
        }
    #include "Interception/Hooks/D3D8/d3d8_methods.inl"
    #undef D3D8_METHOD
}

Replay::StereoReplayEngineDLL::D3D8CallExecutor::~D3D8CallExecutor()
{
    /* The objects belong to the replay thread: reset() has released them already. */
}

bool Replay::StereoReplayEngineDLL::D3D8CallExecutor::create_direct3d8(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                std::string*                        out_error_ptr)
{
    typedef void* (__stdcall* PFNDIRECT3DCREATE8PROC)(UINT);

    PFNDIRECT3DCREATE8PROC create_func_ptr = nullptr;
    void*                  object_ptr      = nullptr;
    bool                   result          = false;

    if (m_d3d8_module == nullptr)
    {
        m_d3d8_module = ::LoadLibraryW(L"d3d8.dll");
    }

    if (m_d3d8_module == nullptr)
    {
        *out_error_ptr = "d3d8.dll cannot be loaded.";

        goto end;
    }

    create_func_ptr = reinterpret_cast<PFNDIRECT3DCREATE8PROC>(::GetProcAddress(m_d3d8_module,
                                                                                "Direct3DCreate8") );

    if (create_func_ptr == nullptr)
    {
        *out_error_ptr = "Direct3DCreate8 is not available.";

        goto end;
    }

    object_ptr = create_func_ptr(in_call.arg_in_vec.at(0).get_u32() );

    if (object_ptr == nullptr)
    {
        *out_error_ptr = "Direct3DCreate8 failed.";

        goto end;
    }

    if (in_call.returned_value_ptr != nullptr)
    {
        m_context.register_object(in_call.returned_value_ptr->get_ptr_as_u64(),
                                  object_ptr);
    }

    result = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::D3D8CallExecutor::execute(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       const CallWalker&                   in_walker,
                                                                       std::string*                        out_error_ptr)
{
    const auto invoker_iterator = m_invoker_map.find(in_call.func);
    bool       result           = false;

    if (in_walker.get_segment().pointer_n_bytes != sizeof(void*) )
    {
        *out_error_ptr = "The workload was dumped by a process of another pointer size: use the 32-bit player for D3D8.";

        goto end;
    }

    if (in_call.func == APIInterceptor::APIFUNCTION_D3D8_DIRECT3DCREATE8)
    {
        result = create_direct3d8(in_call,
                                  out_error_ptr);

        goto end;
    }

    /* Release is shared by all the interfaces, and tells when an object is gone. */
    if (m_release_function_set.find(in_call.func) != m_release_function_set.end() )
    {
        result = release(in_call);

        goto end;
    }

    m_context.begin_call(&in_call,
                         &in_walker,
                         (invoker_iterator != m_invoker_map.end() ) ? invoker_iterator->second.n_head_arguments
                                                                    : 0);

    if (D3D8LockReplayer::is_lock_function(in_call.func) )
    {
        result = (D3D8LockReplayer::is_unlock_function(in_call.func) ) ? m_lock_replayer.unlock(in_call,
                                                                                                out_error_ptr)
                                                                       : m_lock_replayer.lock  (in_call,
                                                                                                out_error_ptr);

        goto end;
    }

    /* A method the replay does not do. */
    if (invoker_iterator == m_invoker_map.end() )
    {
        result = true;

        goto end;
    }

    result = invoker_iterator->second.invoke_func_ptr(&m_context,
                                                      out_error_ptr);

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::D3D8CallExecutor::is_ignored_method(const char* in_method_name_ptr)
{
    static const char* const IGNORED_METHOD_NAME_ARRAY[] =
    {
        "FreePrivateData",
        "GetClipPlane",
        "GetInfo",
        "GetPaletteEntries",
        "GetPixelShaderConstant",
        "GetPixelShaderFunction",
        "GetPrivateData",
        "GetVertexShaderConstant",
        "GetVertexShaderDeclaration",
        "GetVertexShaderFunction",
        "RegisterSoftwareDevice",
        "SetPrivateData",
    };
    bool result = false;

    for (const auto name_ptr : IGNORED_METHOD_NAME_ARRAY)
    {
        if (::strcmp(name_ptr,
                     in_method_name_ptr) == 0)
        {
            result = true;

            break;
        }
    }

    return result;
}

void Replay::StereoReplayEngineDLL::D3D8CallExecutor::on_frame_presented()
{
    /* Stub */
}

bool Replay::StereoReplayEngineDLL::D3D8CallExecutor::release(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    typedef ULONG (__stdcall* PFNRELEASEPROC)(void*);

    std::string error;
    void*       object_ptr = nullptr;

    /* An object which the replay has not got is not the replay's to release. */
    if (m_context.get_live_object(in_call.arg_in_vec.at(0),
                                 &object_ptr,
                                 &error) )
    {
        void** const vtable_ptr = *reinterpret_cast<void***>(object_ptr);
        const ULONG  n_refs     = reinterpret_cast<PFNRELEASEPROC>(vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_RELEASE])(object_ptr);

        if (n_refs == 0)
        {
            m_context.forget_object(in_call.arg_in_vec.at(0).get_ptr_as_u64() );
        }
    }

    return true;
}

void Replay::StereoReplayEngineDLL::D3D8CallExecutor::reset()
{
    m_lock_replayer.reset();
    m_context.release_all_objects();
}
