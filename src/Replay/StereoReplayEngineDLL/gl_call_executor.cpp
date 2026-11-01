/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_call_executor.h"
#include "gl_function_types.h"
#include "gl_replay_invoker.h"
#include <string.h>

/* Turns the parenthesized list of the table's entry into the list of an initializer. */
#define GL_UNPACK_KINDS(...) __VA_ARGS__

Replay::StereoReplayEngineDLL::GlCallExecutor::GlCallExecutor(ObjectRemapper* in_remapper_ptr)
    :m_client_array_replayer(&m_context,
                             [this](const char* in_name_ptr)
                             {
                                 return get_proc_ptr(in_name_ptr);
                             }),
     m_context              (in_remapper_ptr),
     m_index_replayer       (&m_context,
                             [this](const char* in_name_ptr)
                             {
                                 return get_proc_ptr(in_name_ptr);
                             }),
     m_mapping_replayer     (&m_context,
                             [this](const char* in_name_ptr)
                             {
                                 return get_proc_ptr(in_name_ptr);
                             }),
     m_opengl32_module      (::LoadLibraryW(L"opengl32.dll") )
{
    /* Every function of the table. */
    #define GL_REPLAY_FUNCTION(FUNCTION, NAME, FUNCTION_TYPE, RETURN_KIND, KINDS, IGNORED_MASK) \
        { \
            typedef GlReplay::Invoker<FUNCTION_TYPE> FunctionReplay; \
            \
            add_function(FUNCTION, \
                         NAME, \
                         RETURN_KIND, \
                         std::vector<ObjectKind>{GL_UNPACK_KINDS KINDS}, \
                         IGNORED_MASK, \
                         &FunctionReplay::invoke, \
                         FunctionReplay::get_n_arguments()); \
        }

    #include "gl_replay_table.inl"

    #undef GL_REPLAY_FUNCTION
}

Replay::StereoReplayEngineDLL::GlCallExecutor::~GlCallExecutor()
{
    /* The context belongs to the replay thread: reset() has dropped what refers to it already. */
}

void Replay::StereoReplayEngineDLL::GlCallExecutor::add_function(const APIInterceptor::APIFunction& in_function,
                                                                 const char*                        in_name_ptr,
                                                                 const ObjectKind&                  in_return_kind,
                                                                 const std::vector<ObjectKind>&     in_param_kind_vec,
                                                                 const uint32_t&                    in_ignored_param_mask,
                                                                 const InvokeFuncPtr&               in_invoke_func_ptr,
                                                                 const size_t&                      in_n_head_arguments)
{
    FunctionEntry entry;

    entry.info.has_explicit_program = (::strncmp(in_name_ptr,
                                                 "glProgramUniform",
                                                 ::strlen("glProgramUniform") ) == 0);
    entry.info.ignored_param_mask   = in_ignored_param_mask;
    entry.info.name_ptr             = in_name_ptr;
    entry.info.param_kind_vec       = in_param_kind_vec;
    entry.info.return_kind          = in_return_kind;
    entry.invoke_func_ptr           = in_invoke_func_ptr;
    entry.n_head_arguments          = in_n_head_arguments;

    m_entry_map[in_function] = entry;
}

void Replay::StereoReplayEngineDLL::GlCallExecutor::after_call(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    int64_t dumped_location = 0;
    uint64_t dumped_program = 0;

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLGETATTRIBLOCATION:
        {
            /* The index that the app uses is the one the dump holds; the driver may hand out another. */
            if (in_call.returned_value_ptr != nullptr)
            {
                read_number(*in_call.returned_value_ptr,
                            &dumped_location);

                if (dumped_location                                    >= 0 &&
                    static_cast<int32_t>(m_context.get_live_return() ) >= 0)
                {
                    m_context.register_attrib_location(static_cast<uint32_t>(dumped_location),
                                                       static_cast<uint32_t>(m_context.get_live_return() ));
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLGETUNIFORMLOCATION:
        {
            if (in_call.returned_value_ptr != nullptr &&
                in_call.arg_in_vec.empty() == false)
            {
                read_number(*in_call.returned_value_ptr,
                            &dumped_location);
                read_number(in_call.arg_in_vec.at(0),
                            &dumped_program);

                if (dumped_location                                    >= 0 &&
                    static_cast<int32_t>(m_context.get_live_return() ) >= 0)
                {
                    m_context.register_uniform_location(dumped_program,
                                                        static_cast<int32_t>(dumped_location),
                                                        static_cast<int32_t>(m_context.get_live_return() ));
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLGETFRAGDATALOCATION:
        {
            register_returned_index(in_call,
                                    ObjectKind::FRAG_DATA_LOCATION,
                                    false); /* in_has_scope */

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMRESOURCEINDEX:
        {
            constexpr uint32_t COMPAT_GL_SHADER_STORAGE_BLOCK = 0x92E6;

            uint32_t program_interface = 0;

            if (in_call.arg_in_vec.size() > 1)
            {
                read_number(in_call.arg_in_vec.at(1),
                            &program_interface);
            }

            /* The indices of the other resources are not used by the app to set anything. */
            if (program_interface == COMPAT_GL_SHADER_STORAGE_BLOCK)
            {
                register_returned_index(in_call,
                                        ObjectKind::PROGRAM_RESOURCE_INDEX,
                                        false); /* in_has_scope */
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLGETSUBROUTINEINDEX:
        {
            register_returned_index(in_call,
                                    ObjectKind::SUBROUTINE_INDEX,
                                    true); /* in_has_scope */

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLGETSUBROUTINEUNIFORMLOCATION:
        {
            register_returned_index(in_call,
                                    ObjectKind::SUBROUTINE_UNIFORM_LOCATION,
                                    true); /* in_has_scope */

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLGETUNIFORMBLOCKINDEX:
        {
            register_returned_index(in_call,
                                    ObjectKind::UNIFORM_BLOCK_INDEX,
                                    false); /* in_has_scope */

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLUSEPROGRAM:
        {
            if (in_call.arg_in_vec.empty() == false)
            {
                read_number(in_call.arg_in_vec.at(0),
                            &dumped_program);

                m_context.set_current_program(dumped_program);
            }

            break;
        }

        default:
        {
            break;
        }
    }

    m_context.register_returned_name(m_context.get_live_return() );
}

bool Replay::StereoReplayEngineDLL::GlCallExecutor::execute(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                            const IChunkSource&                          in_chunk_source,
                                                            std::string*                                 out_error_ptr)
{
    const auto entry_iterator = m_entry_map.find(in_call.func);
    void*      proc_ptr       = nullptr;
    bool       result         = false;

    if (entry_iterator == m_entry_map.end() )
    {
        *out_error_ptr = "The GL executor does not know the function.";

        goto end;
    }

    if (::wglGetCurrentContext() == nullptr)
    {
        *out_error_ptr = "A GL call is made while no GL context is current.";

        goto end;
    }

    m_context.begin_call(&in_call,
                         &in_chunk_source,
                         entry_iterator->second.n_head_arguments,
                         &entry_iterator->second.info);

    if (GlMappingReplayer::is_mapping_function(in_call.func) )
    {
        result = m_mapping_replayer.replay(in_call,
                                           out_error_ptr);

        goto end;
    }

    if (GlIndexReplayer::is_index_function(in_call.func) )
    {
        result = m_index_replayer.replay(in_call,
                                         out_error_ptr);

        goto end;
    }

    /* What the dumper stored after the arguments of a draw call: the part of the app's vertex arrays which it reads. */
    if (in_call.arg_in_vec.size() > entry_iterator->second.n_head_arguments)
    {
        size_t         n_bytes  = 0;
        const uint8_t* data_ptr = m_context.get_chunk_data_ptr(&in_call.arg_in_vec.at(entry_iterator->second.n_head_arguments),
                                                               &n_bytes);

        if (data_ptr != nullptr)
        {
            m_client_array_replayer.apply(data_ptr,
                                          n_bytes);
        }
    }

    proc_ptr = get_proc_ptr(entry_iterator->second.info.name_ptr);

    if (proc_ptr == nullptr)
    {
        *out_error_ptr = std::string("The driver does not have ") + entry_iterator->second.info.name_ptr + ".";

        goto end;
    }

    if (entry_iterator->second.invoke_func_ptr(&m_context,
                                               proc_ptr,
                                               out_error_ptr) == false)
    {
        goto end;
    }

    if (m_context.is_skipped() == false)
    {
        after_call(in_call);
    }

    result = true;

end:
    return result;
}

void* Replay::StereoReplayEngineDLL::GlCallExecutor::get_proc_ptr(const char* in_name_ptr)
{
    const auto iterator = m_proc_ptr_map.find(in_name_ptr);
    void*      result   = nullptr;

    if (iterator != m_proc_ptr_map.end() )
    {
        result = iterator->second;

        goto end;
    }

    /* The extensions and the newer versions come from the driver; the oldest functions are exports of opengl32.dll. */
    result = reinterpret_cast<void*>(::wglGetProcAddress(in_name_ptr) );

    if (is_valid_proc(result) == false)
    {
        result = (m_opengl32_module != nullptr) ? reinterpret_cast<void*>(::GetProcAddress(m_opengl32_module,
                                                                                           in_name_ptr) )
                                                : nullptr;
    }

    if (result != nullptr)
    {
        m_proc_ptr_map[in_name_ptr] = result;
    }

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::GlCallExecutor::is_valid_proc(const void* in_proc_ptr)
{
    constexpr uintptr_t MAX_INVALID_PROC_VALUE = 3;

    const uintptr_t value = reinterpret_cast<uintptr_t>(in_proc_ptr);

    return (value >  MAX_INVALID_PROC_VALUE     &&
            value != static_cast<uintptr_t>(-1) );
}

void Replay::StereoReplayEngineDLL::GlCallExecutor::on_frame_presented()
{
    /* Stub */
}

void Replay::StereoReplayEngineDLL::GlCallExecutor::register_returned_index(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                            const ObjectKind&                            in_kind,
                                                                            const bool&                                  in_has_scope)
{
    constexpr uint32_t INVALID_INDEX = 0xFFFFFFFFu;

    int64_t  dumped_index   = -1;
    uint64_t dumped_program = 0;
    uint32_t scope          = 0;

    if (in_call.returned_value_ptr != nullptr &&
        in_call.arg_in_vec.empty() == false)
    {
        read_number(*in_call.returned_value_ptr,
                    &dumped_index);
        read_number(in_call.arg_in_vec.at(0),
                    &dumped_program);

        /* The shader stage is the second argument of the functions which have a scope. */
        if (in_has_scope                  &&
            in_call.arg_in_vec.size() > 1)
        {
            read_number(in_call.arg_in_vec.at(1),
                        &scope);
        }

        /* An index which does not exist is -1, or GL_INVALID_INDEX. */
        if (dumped_index                                        >= 0             &&
            static_cast<uint32_t>(dumped_index)                 != INVALID_INDEX &&
            static_cast<uint32_t>(m_context.get_live_return() ) != INVALID_INDEX)
        {
            m_context.register_index(in_kind,
                                     dumped_program,
                                     scope,
                                     static_cast<uint32_t>(dumped_index),
                                     static_cast<uint32_t>(m_context.get_live_return() ));
        }
    }
}

void Replay::StereoReplayEngineDLL::GlCallExecutor::reset()
{
    m_context.forget_locations();
    m_mapping_replayer.reset  ();
    m_proc_ptr_map.clear      ();
}
