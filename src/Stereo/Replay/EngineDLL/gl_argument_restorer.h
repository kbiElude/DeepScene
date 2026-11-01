/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Turns the arguments that the dumper stored back into the arguments of a GL call. How an argument was stored depends
 * on its type: a number is stored as it is; a pointer is a data chunk when the data was copied (an input array, a
 * string), and just a pointer value when it is an offset in a buffer or a place for the driver to write to (the data
 * written is a chunk of the arguments after the call).
 */
#ifndef STEREO_REPLAY_ENGINEDLL_GL_ARGUMENT_RESTORER_H
#define STEREO_REPLAY_ENGINEDLL_GL_ARGUMENT_RESTORER_H

#include "gl_replay_context.h"
#include <string.h>
#include <string>
#include <type_traits>
#include <Windows.h>
#include "Khronos/GL/glcorearb.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            namespace GlRestore
            {
                enum class ArgumentCategory
                {
                    FUNCTION,
                    NUMBER,
                    POINTER,
                    STRING,
                    STRING_ARRAY,
                    SYNC
                };

                template<typename T>
                struct ArgumentCategoryOf
                {
                    static const ArgumentCategory value = (std::is_arithmetic<T>::value                 || std::is_enum<T>::value)                                         ? ArgumentCategory::NUMBER
                                                        :  std::is_same<T, GLsync>::value                                                                                  ? ArgumentCategory::SYNC
                                                        :  std::is_same<T, const GLchar*>::value                                                                           ? ArgumentCategory::STRING
                                                        :  std::is_same<T, const GLchar* const*>::value                                                                    ? ArgumentCategory::STRING_ARRAY
                                                        : (std::is_pointer<T>::value                    && std::is_function<typename std::remove_pointer<T>::type>::value) ? ArgumentCategory::FUNCTION
                                                                                                                                                                           : ArgumentCategory::POINTER;
                };

                /* The restorers which have nothing to do after the call. */
                class PassiveRestorer
                {
                public:
                    void finish(GlReplayContext* in_context_ptr,
                                const size_t&    in_n_argument,
                                const bool&      in_is_success)
                    {
                        (void) in_context_ptr;
                        (void) in_n_argument;
                        (void) in_is_success;
                    }
                };

                template<typename T,
                         ArgumentCategory category>
                class ArgumentRestorer;

                /* A pointer to a function (a callback): not replayed. */
                template<typename T>
                class ArgumentRestorer<T, ArgumentCategory::FUNCTION> : public PassiveRestorer
                {
                public:
                    T get() const
                    {
                        return nullptr;
                    }

                    bool prepare(GlReplayContext* in_context_ptr,
                                 const size_t&    in_n_argument,
                                 std::string*     out_error_ptr)
                    {
                        (void) in_context_ptr;
                        (void) in_n_argument;
                        (void) out_error_ptr;

                        return true;
                    }
                };

                /* Integers and floats; the ones which are the names of objects, or locations, are mapped. */
                template<typename T>
                class ArgumentRestorer<T, ArgumentCategory::NUMBER> : public PassiveRestorer
                {
                public:
                    ArgumentRestorer()
                        :m_value()
                    {
                        /* Stub */
                    }

                    T get() const
                    {
                        return m_value;
                    }

                    bool prepare(GlReplayContext* in_context_ptr,
                                 const size_t&    in_n_argument,
                                 std::string*     out_error_ptr)
                    {
                        const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);
                        bool       is_read      = false;
                        const auto kind         = in_context_ptr->get_param_kind     (in_n_argument);
                        bool       result       = false;

                        if (argument_ptr != nullptr)
                        {
                            is_read = read_number(*argument_ptr,
                                                 &m_value);
                        }

                        if (is_read == false)
                        {
                            *out_error_ptr = "A GL call has an argument of an unexpected kind.";

                            goto end;
                        }

                        if (kind != ObjectKind::NONE)
                        {
                            m_value = static_cast<T>(in_context_ptr->map_argument(in_n_argument,
                                                                                  static_cast<uint32_t>(m_value) ));
                        }

                        result = true;

                    end:
                        return result;
                    }

                private:
                    T m_value;
                };

                /* U* and const U*: an array, or a place to write to, or an offset in a buffer. */
                template<typename T>
                class ArgumentRestorer<T, ArgumentCategory::POINTER>
                {
                public:
                    ArgumentRestorer()
                        :m_dumped_names_ptr(nullptr),
                         m_n_dumped_bytes  (0),
                         m_scratch_ptr     (nullptr),
                         m_value           (nullptr)
                    {
                        /* Stub */
                    }

                    /* The names the driver wrote are the ones the dump holds: from now on, the dump's names mean them. */
                    void finish(GlReplayContext* in_context_ptr,
                                const size_t&    in_n_argument,
                                const bool&      in_is_success)
                    {
                        const auto kind = in_context_ptr->get_param_kind(in_n_argument);

                        if (in_is_success                          &&
                            m_scratch_ptr      != nullptr          &&
                            m_dumped_names_ptr != nullptr          &&
                            kind               != ObjectKind::NONE)
                        {
                            const size_t n_names = m_n_dumped_bytes / sizeof(uint32_t);

                            for (size_t n_name = 0;
                                        n_name < n_names && (n_name + 1) * sizeof(uint32_t) <= m_scratch_ptr->size();
                                      ++n_name)
                            {
                                uint32_t dumped_name = 0;
                                uint32_t live_name   = 0;

                                ::memcpy(&dumped_name,
                                         m_dumped_names_ptr + n_name * sizeof(uint32_t),
                                         sizeof(uint32_t) );
                                ::memcpy(&live_name,
                                         m_scratch_ptr->data() + n_name * sizeof(uint32_t),
                                         sizeof(uint32_t) );

                                in_context_ptr->register_name(kind,
                                                              dumped_name,
                                                              live_name);
                            }
                        }
                    }

                    T get() const
                    {
                        return m_value;
                    }

                    bool prepare(GlReplayContext* in_context_ptr,
                                 const size_t&    in_n_argument,
                                 std::string*     out_error_ptr)
                    {
                        const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);
                        bool       result       = false;

                        if (argument_ptr == nullptr)
                        {
                            *out_error_ptr = "A GL call has too few arguments.";

                            goto end;
                        }

                        if (in_context_ptr->is_param_ignored(in_n_argument) == false)
                        {
                            if (argument_ptr->type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
                            {
                                prepare_array(in_context_ptr,
                                              in_n_argument,
                                              *argument_ptr);
                            }
                            else
                            {
                                prepare_pointer(in_context_ptr,
                                                in_n_argument,
                                                *argument_ptr);
                            }
                        }

                        result = true;

                    end:
                        return result;
                    }

                private:
                    /* Data the dumper copied. An array of names has to be mapped, the rest is passed as it is. */
                    void prepare_array(GlReplayContext*                           in_context_ptr,
                                       const size_t&                              in_n_argument,
                                       const APIInterceptor::APIFunctionArgument& in_argument)
                    {
                        size_t         n_bytes  = 0;
                        const uint8_t* data_ptr = in_context_ptr->get_chunk_data_ptr(&in_argument,
                                                                                     &n_bytes);
                        const auto     kind     = in_context_ptr->get_param_kind    (in_n_argument);

                        if (data_ptr == nullptr)
                        {
                            /* An empty array is a null pointer. */
                        }
                        else
                        if (kind != ObjectKind::NONE)
                        {
                            m_scratch_ptr = in_context_ptr->allocate_scratch(n_bytes);

                            for (size_t n_name = 0;
                                        (n_name + 1) * sizeof(uint32_t) <= n_bytes;
                                      ++n_name)
                            {
                                uint32_t name = 0;

                                ::memcpy(&name,
                                         data_ptr + n_name * sizeof(uint32_t),
                                         sizeof(uint32_t) );

                                name = in_context_ptr->map_name(kind,
                                                                name);

                                ::memcpy(m_scratch_ptr->data() + n_name * sizeof(uint32_t),
                                         &name,
                                         sizeof(uint32_t) );
                            }

                            m_value       = reinterpret_cast<T>(m_scratch_ptr->data() );
                            m_scratch_ptr = nullptr; /* an input: nothing to register */
                        }
                        else
                        {
                            m_value = reinterpret_cast<T>(const_cast<uint8_t*>(data_ptr) );
                        }
                    }

                    /* An offset, or memory of the dumped process: the driver writes to it, and the dump holds what it wrote. */
                    void prepare_pointer(GlReplayContext*                           in_context_ptr,
                                         const size_t&                              in_n_argument,
                                         const APIInterceptor::APIFunctionArgument& in_argument)
                    {
                        /* A pointer value below this is an offset in a buffer; the one above is an address of the dumped process. */
                        constexpr uint64_t MAX_BUFFER_OFFSET = 0x10000000;

                        size_t     n_bytes          = 0;
                        const auto out_argument_ptr = in_context_ptr->get_out_argument_ptr(in_n_argument);
                        const auto pointer_value    = in_argument.get_ptr_as_u64          ();
                        const auto out_data_ptr     = in_context_ptr->get_chunk_data_ptr  (out_argument_ptr,
                                                                                           &n_bytes);

                        if (out_data_ptr != nullptr)
                        {
                            m_dumped_names_ptr = out_data_ptr;
                            m_n_dumped_bytes   = n_bytes;
                            m_scratch_ptr      = in_context_ptr->allocate_scratch(n_bytes);
                            m_value            = reinterpret_cast<T>             (m_scratch_ptr->data() );
                        }
                        else
                        if (pointer_value == 0)
                        {
                            m_value = nullptr;
                        }
                        else
                        if (pointer_value < MAX_BUFFER_OFFSET)
                        {
                            m_value = reinterpret_cast<T>(static_cast<uintptr_t>(pointer_value) );
                        }
                        else
                        {
                            /* Memory the replay does not have: the call cannot be replayed. */
                            in_context_ptr->request_skip();
                        }
                    }

                    const uint8_t*        m_dumped_names_ptr;
                    size_t                m_n_dumped_bytes;
                    std::vector<uint8_t>* m_scratch_ptr;
                    T                     m_value;
                };

                /* const GLchar*: a string. */
                template<typename T>
                class ArgumentRestorer<T, ArgumentCategory::STRING> : public PassiveRestorer
                {
                public:
                    ArgumentRestorer()
                        :m_value(nullptr)
                    {
                        /* Stub */
                    }

                    T get() const
                    {
                        return m_value;
                    }

                    bool prepare(GlReplayContext* in_context_ptr,
                                 const size_t&    in_n_argument,
                                 std::string*     out_error_ptr)
                    {
                        const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);
                        bool       result       = false;

                        if (argument_ptr == nullptr)
                        {
                            *out_error_ptr = "A GL call has too few arguments.";

                            goto end;
                        }

                        if (argument_ptr->type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
                        {
                            size_t         n_bytes  = 0;
                            const uint8_t* data_ptr = in_context_ptr->get_chunk_data_ptr(argument_ptr,
                                                                                         &n_bytes);

                            if (data_ptr != nullptr)
                            {
                                std::string* string_ptr = in_context_ptr->allocate_string();

                                string_ptr->assign(reinterpret_cast<const char*>(data_ptr),
                                                   n_bytes);

                                /* The copy holds the terminator, too; the string is terminated by the container. */
                                string_ptr->resize(::strlen(string_ptr->c_str() ));

                                m_value = string_ptr->c_str();
                            }
                        }
                        else
                        if (argument_ptr->get_ptr_as_u64() != 0)
                        {
                            in_context_ptr->request_skip();
                        }

                        result = true;

                    end:
                        return result;
                    }

                private:
                    T m_value;
                };

                /* const GLchar* const*: strings, stored as a count and, for each, a length and the characters. */
                template<typename T>
                class ArgumentRestorer<T, ArgumentCategory::STRING_ARRAY> : public PassiveRestorer
                {
                public:
                    ArgumentRestorer()
                        :m_value(nullptr)
                    {
                        /* Stub */
                    }

                    T get() const
                    {
                        return m_value;
                    }

                    bool prepare(GlReplayContext* in_context_ptr,
                                 const size_t&    in_n_argument,
                                 std::string*     out_error_ptr)
                    {
                        const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);
                        bool       result       = false;

                        if (argument_ptr == nullptr)
                        {
                            *out_error_ptr = "A GL call has too few arguments.";

                            goto end;
                        }

                        if (argument_ptr->type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
                        {
                            size_t         n_bytes  = 0;
                            const uint8_t* data_ptr = in_context_ptr->get_chunk_data_ptr(argument_ptr,
                                                                                         &n_bytes);

                            if (data_ptr != nullptr)
                            {
                                uint32_t n_strings              = 0;
                                size_t   offset                 = sizeof(uint32_t);
                                auto     string_pointer_vec_ptr = in_context_ptr->allocate_string_pointers();

                                ::memcpy(&n_strings,
                                         data_ptr,
                                         sizeof(uint32_t) );

                                for (uint32_t n_string = 0;
                                              n_string < n_strings && offset + sizeof(uint32_t) <= n_bytes;
                                            ++n_string)
                                {
                                    uint32_t n_characters = 0;

                                    ::memcpy(&n_characters,
                                             data_ptr + offset,
                                             sizeof(uint32_t) );

                                    offset += sizeof(uint32_t);

                                    if (offset + n_characters > n_bytes)
                                    {
                                        break;
                                    }

                                    std::string* string_ptr = in_context_ptr->allocate_string();

                                    string_ptr->assign(reinterpret_cast<const char*>(data_ptr + offset),
                                                       n_characters);

                                    string_pointer_vec_ptr->push_back(string_ptr->c_str() );

                                    offset += n_characters;
                                }

                                m_value = string_pointer_vec_ptr->data();
                            }
                        }
                        else
                        if (argument_ptr->get_ptr_as_u64() != 0)
                        {
                            in_context_ptr->request_skip();
                        }

                        result = true;

                    end:
                        return result;
                    }

                private:
                    T m_value;
                };

                /* GLsync: a handle of the replay. */
                template<typename T>
                class ArgumentRestorer<T, ArgumentCategory::SYNC> : public PassiveRestorer
                {
                public:
                    ArgumentRestorer()
                        :m_value(nullptr)
                    {
                        /* Stub */
                    }

                    T get() const
                    {
                        return m_value;
                    }

                    bool prepare(GlReplayContext* in_context_ptr,
                                 const size_t&    in_n_argument,
                                 std::string*     out_error_ptr)
                    {
                        const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);
                        uint64_t   live_value   = 0;
                        bool       result       = false;

                        if (argument_ptr == nullptr)
                        {
                            *out_error_ptr = "A GL call has too few arguments.";

                            goto end;
                        }

                        if (in_context_ptr->get_remapper_ptr()->get(ObjectKind::SYNC,
                                                                    argument_ptr->get_ptr_as_u64(),
                                                                   &live_value) )
                        {
                            m_value = reinterpret_cast<T>(static_cast<uintptr_t>(live_value) );
                        }

                        result = true;

                    end:
                        return result;
                    }

                private:
                    T m_value;
                };
            }
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_GL_ARGUMENT_RESTORER_H */
