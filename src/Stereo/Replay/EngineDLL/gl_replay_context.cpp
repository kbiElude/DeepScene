/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_replay_context.h"
#include <limits.h>
#include <string.h>

Stereo::Replay::EngineDLL::GlReplayContext::GlReplayContext(ObjectRemapper* in_remapper_ptr)
    :CallContext        (in_remapper_ptr),
     m_current_program  (0),
     m_function_info_ptr(nullptr),
     m_is_skipped       (false),
     m_live_return      (0)
{
    /* Stub */
}

std::vector<uint8_t>* Stereo::Replay::EngineDLL::GlReplayContext::allocate_scratch(const size_t& in_n_bytes)
{
    m_scratch_deque.emplace_back(in_n_bytes);

    return &m_scratch_deque.back();
}

std::string* Stereo::Replay::EngineDLL::GlReplayContext::allocate_string()
{
    m_string_deque.emplace_back();

    return &m_string_deque.back();
}

std::vector<const char*>* Stereo::Replay::EngineDLL::GlReplayContext::allocate_string_pointers()
{
    m_string_pointers_deque.emplace_back();

    return &m_string_pointers_deque.back();
}

void Stereo::Replay::EngineDLL::GlReplayContext::begin_call(const Capture::APIDumpLoader::DumpedAPICall* in_call_ptr,
                                                            const IChunkSource*                          in_chunk_source_ptr,
                                                            const size_t&                                in_n_head_arguments,
                                                            const GlFunctionInfo*                        in_function_info_ptr)
{
    CallContext::begin_call(in_call_ptr,
                            in_chunk_source_ptr,
                            in_n_head_arguments);

    m_function_info_ptr = in_function_info_ptr;
    m_is_skipped        = false;
    m_live_return       = 0;

    m_scratch_deque.clear        ();
    m_string_deque.clear         ();
    m_string_pointers_deque.clear();
}

void Stereo::Replay::EngineDLL::GlReplayContext::forget_locations()
{
    m_attrib_location_map.clear ();
    m_index_map.clear           ();
    m_uniform_location_map.clear();

    m_current_program = 0;
}

uint64_t Stereo::Replay::EngineDLL::GlReplayContext::get_current_program() const
{
    return m_current_program;
}

uint64_t Stereo::Replay::EngineDLL::GlReplayContext::get_index_program() const
{
    uint64_t result = m_current_program;

    /* The call that names the program it asks the index of (e.g. glUniformBlockBinding()) says which one. */
    if (m_function_info_ptr->param_kind_vec.empty()  == false               &&
        m_function_info_ptr->param_kind_vec.at   (0) == ObjectKind::PROGRAM)
    {
        const auto argument_ptr = get_in_argument_ptr(0);

        if (argument_ptr != nullptr)
        {
            read_number(*argument_ptr,
                        &result);
        }
    }

    return result;
}

uint64_t Stereo::Replay::EngineDLL::GlReplayContext::get_live_return() const
{
    return m_live_return;
}

Stereo::Replay::EngineDLL::ObjectKind Stereo::Replay::EngineDLL::GlReplayContext::get_param_kind(const size_t& in_n_argument) const
{
    return (in_n_argument < m_function_info_ptr->param_kind_vec.size() ) ? m_function_info_ptr->param_kind_vec.at(in_n_argument)
                                                                         : ObjectKind::NONE;
}

uint64_t Stereo::Replay::EngineDLL::GlReplayContext::get_uniform_program() const
{
    uint64_t result = m_current_program;

    /* glProgramUniform*() name the program themselves. */
    if (m_function_info_ptr->has_explicit_program)
    {
        const auto argument_ptr = get_in_argument_ptr(0);

        if (argument_ptr != nullptr)
        {
            read_number(*argument_ptr,
                        &result);
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::GlReplayContext::is_param_ignored(const size_t& in_n_argument) const
{
    return (in_n_argument                                                     <  sizeof(m_function_info_ptr->ignored_param_mask) * CHAR_BIT &&
            (m_function_info_ptr->ignored_param_mask & (1u << in_n_argument)) != 0);
}

bool Stereo::Replay::EngineDLL::GlReplayContext::is_skipped() const
{
    return m_is_skipped;
}

uint32_t Stereo::Replay::EngineDLL::GlReplayContext::map_argument(const size_t&   in_n_argument,
                                                                  const uint32_t& in_dumped_value) const
{
    constexpr uint32_t COMPAT_GL_COLOR = 0x1800;

    const auto kind   = get_param_kind(in_n_argument);
    uint32_t   result = in_dumped_value;

    if (kind != ObjectKind::NONE)
    {
        uint32_t buffer = COMPAT_GL_COLOR;

        /* The draw buffers are those of the color buffer; the depth and the stencil have just one (the 0-th). */
        if (kind          == ObjectKind::FRAG_DATA_LOCATION &&
            in_n_argument >  0)
        {
            const int clear_difference = ::strncmp(m_function_info_ptr->name_ptr,
                                                   "glClear",
                                                   ::strlen("glClear") );

            if (clear_difference == 0)
            {
                read_number(*get_in_argument_ptr(in_n_argument - 1),
                            &buffer);
            }
        }

        if (buffer == COMPAT_GL_COLOR)
        {
            result = map_name(kind,
                              in_dumped_value);
        }
    }

    return result;
}

uint32_t Stereo::Replay::EngineDLL::GlReplayContext::map_attrib_location(const uint32_t& in_dumped_location) const
{
    const auto iterator = m_attrib_location_map.find(in_dumped_location);

    return (iterator != m_attrib_location_map.end() ) ? iterator->second
                                                      : in_dumped_location;
}

uint32_t Stereo::Replay::EngineDLL::GlReplayContext::map_index(const ObjectKind& in_kind,
                                                               const uint64_t&   in_dumped_program,
                                                               const uint32_t&   in_scope,
                                                               const uint32_t&   in_dumped_index) const
{
    const auto iterator = m_index_map.find(IndexKey(in_kind,
                                                    in_dumped_program,
                                                    in_scope,
                                                    in_dumped_index) );

    return (iterator != m_index_map.end() ) ? iterator->second
                                            : in_dumped_index;
}

uint32_t Stereo::Replay::EngineDLL::GlReplayContext::map_name(const ObjectKind& in_kind,
                                                              const uint32_t&   in_dumped_name) const
{
    uint64_t live_value = 0;
    uint32_t result     = in_dumped_name;

    switch (in_kind)
    {
        case ObjectKind::ATTRIB_LOCATION:
        {
            result = map_attrib_location(in_dumped_name);

            break;
        }

        case ObjectKind::UNIFORM_LOCATION:
        {
            result = static_cast<uint32_t>(map_uniform_location(static_cast<int32_t>(in_dumped_name) ));

            break;
        }

        case ObjectKind::FRAG_DATA_LOCATION:
        case ObjectKind::PROGRAM_RESOURCE_INDEX:
        case ObjectKind::UNIFORM_BLOCK_INDEX:
        {
            result = map_index(in_kind,
                               get_index_program(),
                               0, /* in_scope */
                               in_dumped_name);

            break;
        }

        case ObjectKind::NONE:
        {
            break;
        }

        default:
        {
            if (get_remapper_ptr()->get(in_kind,
                                        in_dumped_name,
                                       &live_value) )
            {
                result = static_cast<uint32_t>(live_value);
            }

            break;
        }
    }

    return result;
}

int32_t Stereo::Replay::EngineDLL::GlReplayContext::map_uniform_location(const int32_t& in_dumped_location) const
{
    constexpr uint32_t PROGRAM_SHIFT = 32;

    const uint64_t program = get_uniform_program();
    const uint64_t key     = (program << PROGRAM_SHIFT) | static_cast<uint32_t>(in_dumped_location);
    int32_t        result  = in_dumped_location;

    if (in_dumped_location >= 0)
    {
        /* The location of an element of an array is that of the array's first one, plus the index. */
        auto iterator = m_uniform_location_map.upper_bound(key);

        if (iterator != m_uniform_location_map.begin() )
        {
            --iterator;

            if ((iterator->first >> PROGRAM_SHIFT) == program)
            {
                result = iterator->second + (in_dumped_location - static_cast<int32_t>(iterator->first & 0xffffffffu) );
            }
        }
    }

    return result;
}

void Stereo::Replay::EngineDLL::GlReplayContext::register_attrib_location(const uint32_t& in_dumped_location,
                                                                          const uint32_t& in_live_location)
{
    m_attrib_location_map[in_dumped_location] = in_live_location;
}

void Stereo::Replay::EngineDLL::GlReplayContext::register_index(const ObjectKind& in_kind,
                                                                const uint64_t&   in_dumped_program,
                                                                const uint32_t&   in_scope,
                                                                const uint32_t&   in_dumped_index,
                                                                const uint32_t&   in_live_index)
{
    m_index_map[IndexKey(in_kind,
                         in_dumped_program,
                         in_scope,
                         in_dumped_index)] = in_live_index;
}

void Stereo::Replay::EngineDLL::GlReplayContext::register_name(const ObjectKind& in_kind,
                                                               const uint32_t&   in_dumped_name,
                                                               const uint32_t&   in_live_name)
{
    get_remapper_ptr()->add(in_kind,
                            in_dumped_name,
                            in_live_name);
}

void Stereo::Replay::EngineDLL::GlReplayContext::register_returned_name(const uint64_t& in_live_value)
{
    uint64_t    dumped   = 0;
    const auto  kind     = m_function_info_ptr->return_kind;
    const auto& returned = get_call().returned_value_ptr;

    if (kind     != ObjectKind::NONE &&
        returned != nullptr)
    {
        if (kind == ObjectKind::SYNC)
        {
            dumped = returned->get_ptr_as_u64();
        }
        else
        {
            read_number(*returned,
                        &dumped);
        }

        if (dumped != 0)
        {
            if (kind == ObjectKind::DISPLAY_LIST)
            {
                /* glGenLists() hands out a range. */
                uint64_t n_lists = 1;

                if (get_in_argument_ptr(0) != nullptr)
                {
                    read_number(*get_in_argument_ptr(0),
                                &n_lists);
                }

                for (uint64_t n_list = 0;
                              n_list < n_lists;
                            ++n_list)
                {
                    get_remapper_ptr()->add(kind,
                                            dumped + n_list,
                                            in_live_value + n_list);
                }
            }
            else
            {
                get_remapper_ptr()->add(kind,
                                        dumped,
                                        in_live_value);
            }
        }
    }
}

void Stereo::Replay::EngineDLL::GlReplayContext::register_uniform_location(const uint64_t& in_dumped_program,
                                                                           const int32_t&  in_dumped_location,
                                                                           const int32_t&  in_live_location)
{
    constexpr uint32_t PROGRAM_SHIFT = 32;

    m_uniform_location_map[(in_dumped_program << PROGRAM_SHIFT) | static_cast<uint32_t>(in_dumped_location)] = in_live_location;
}

void Stereo::Replay::EngineDLL::GlReplayContext::request_skip()
{
    m_is_skipped = true;
}

void Stereo::Replay::EngineDLL::GlReplayContext::set_current_program(const uint64_t& in_dumped_program)
{
    m_current_program = in_dumped_program;
}

void Stereo::Replay::EngineDLL::GlReplayContext::set_live_return(const uint64_t& in_value)
{
    m_live_return = in_value;
}
