/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The reverse of D3D8/argument_capture.h: turns the arguments that the dumper stored back into the arguments of the
 * call. A restorer is made for each argument of a method; its category (see D3D8Utils/types.h) tells what it does:
 *  - prepare() reads the dumped argument,
 *  - get() hands out the value to pass to the method,
 *  - finish() takes what the method wrote (an object, a handle) and registers it.
 */
#ifndef STEREOREPLAYENGINEDLL_D3D8_ARGUMENT_RESTORER_H
#define STEREOREPLAYENGINEDLL_D3D8_ARGUMENT_RESTORER_H

#include "Interception/Core/D3D8Utils/types.h"
#include "d3d8_replay_context.h"
#include <string.h>
#include <string>
#include <type_traits>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        namespace D3D8Restore
        {
            /* Fixes the contents of a structure so that it makes sense in the replay. */
            template<typename S>
            struct StructFixup
            {
                static void apply(S*                 inout_value_ptr,
                                  D3D8ReplayContext* in_context_ptr)
                {
                    (void) inout_value_ptr;
                    (void) in_context_ptr;
                }
            };

            /* The device is created on a window of the replay, and in a window: the replay is not meant to take over the
             * screen.
             */
            template<>
            struct StructFixup<Interception::Hooks::D3D8::D3DPRESENT_PARAMETERS>
            {
                /* The format of the desktop: the back buffer of a window has to have it (D3D8 does not take D3DFMT_UNKNOWN). */
                static Interception::Hooks::D3D8::D3DFORMAT get_display_format(D3D8ReplayContext* in_context_ptr)
                {
                    typedef HRESULT (__stdcall* PFNGETADAPTERDISPLAYMODEPROC)(void*, UINT, Interception::Hooks::D3D8::D3DDISPLAYMODE*);
                    typedef HRESULT (__stdcall* PFNGETDISPLAYMODEPROC)       (void*, Interception::Hooks::D3D8::D3DDISPLAYMODE*);

                    const auto&                               call   = in_context_ptr->get_call();
                    std::string                               error;
                    Interception::Hooks::D3D8::D3DDISPLAYMODE mode   = {};
                    void*                                     object = nullptr;
                    Interception::Hooks::D3D8::D3DFORMAT      result = Interception::Hooks::D3D8::D3DFMT_X8R8G8B8;

                    if (in_context_ptr->get_live_object(call.arg_in_vec.at(0),
                                                       &object,
                                                       &error) )
                    {
                        HRESULT      hresult    = E_FAIL;
                        void** const vtable_ptr = *reinterpret_cast<void***>(object);

                        /* The adapter's mode for a device that is being created, the device's own after that. */
                        if (call.func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3D8_CREATEDEVICE)
                        {
                            hresult = reinterpret_cast<PFNGETADAPTERDISPLAYMODEPROC>(vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3D8_GETADAPTERDISPLAYMODE])(object,
                                                                                                                                                                            call.arg_in_vec.at(1).get_u32(),
                                                                                                                                                                           &mode);
                        }
                        else
                        {
                            hresult = reinterpret_cast<PFNGETDISPLAYMODEPROC>(vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_GETDISPLAYMODE])(object,
                                                                                                                                                                  &mode);
                        }

                        if (hresult >= 0)
                        {
                            result = mode.Format;
                        }
                    }

                    return result;
                }

                static void apply(Interception::Hooks::D3D8::D3DPRESENT_PARAMETERS* inout_value_ptr,
                                  D3D8ReplayContext*           in_context_ptr)
                {
                    uint64_t live_window = 0;

                    if (in_context_ptr->get_remapper_ptr()->get(ObjectKind::HWND,
                                                                reinterpret_cast<uint64_t>(inout_value_ptr->hDeviceWindow),
                                                               &live_window) )
                    {
                        inout_value_ptr->hDeviceWindow = reinterpret_cast<HWND>(live_window);
                    }
                    else
                    {
                        inout_value_ptr->hDeviceWindow = nullptr;
                    }

                    /* In a window, the back buffer is as big as the window, and flipping is not possible. */
                    inout_value_ptr->BackBufferCount                 = 1;
                    inout_value_ptr->BackBufferFormat                = get_display_format(in_context_ptr);
                    inout_value_ptr->BackBufferHeight                = 0;
                    inout_value_ptr->BackBufferWidth                 = 0;
                    inout_value_ptr->FullScreen_PresentationInterval = Interception::Hooks::D3D8::D3DPRESENT_INTERVAL_DEFAULT;
                    inout_value_ptr->FullScreen_RefreshRateInHz      = 0;
                    inout_value_ptr->SwapEffect                      = Interception::Hooks::D3D8::D3DSWAPEFFECT_DISCARD;
                    inout_value_ptr->Windowed                        = TRUE;
                }
            };

            /* The restorers which have nothing to do after the call. */
            class PassiveRestorer
            {
            public:
                void finish(D3D8ReplayContext* in_context_ptr,
                            const size_t&      in_n_argument,
                            const bool&        in_is_success)
                {
                    (void) in_context_ptr;
                    (void) in_n_argument;
                    (void) in_is_success;
                }
            };

            template<typename T,
                     Interception::Core::D3D8Utils::ArgumentCategory category>
            class ArgumentRestorer;

            /* const S*: an input structure. */
            template<typename T>
            class ArgumentRestorer<T, Interception::Core::D3D8Utils::ArgumentCategory::CONST_STRUCT> : public PassiveRestorer
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

                bool prepare(D3D8ReplayContext* in_context_ptr,
                             const size_t&      in_n_argument,
                             std::string*       out_error_ptr)
                {
                    typedef typename Interception::Core::D3D8Utils::PointeeOf<T>::type StructType;

                    size_t         n_bytes  = 0;
                    const uint8_t* data_ptr = in_context_ptr->get_chunk_data_ptr(in_context_ptr->get_in_argument_ptr(in_n_argument),
                                                                                 &n_bytes);

                    (void) out_error_ptr;

                    m_value = (n_bytes >= sizeof(StructType) ) ? reinterpret_cast<T>(data_ptr)
                                                               : nullptr;

                    return true;
                }

            private:
                T m_value;
            };

            /* const GUID&. */
            template<typename T>
            class ArgumentRestorer<T, Interception::Core::D3D8Utils::ArgumentCategory::GUID> : public PassiveRestorer
            {
            public:
                ArgumentRestorer()
                    :m_value()
                {
                    /* Stub */
                }

                const GUID& get() const
                {
                    return m_value;
                }

                bool prepare(D3D8ReplayContext* in_context_ptr,
                             const size_t&      in_n_argument,
                             std::string*       out_error_ptr)
                {
                    size_t         n_bytes  = 0;
                    const uint8_t* data_ptr = in_context_ptr->get_chunk_data_ptr(in_context_ptr->get_in_argument_ptr(in_n_argument),
                                                                                 &n_bytes);

                    (void) out_error_ptr;

                    if (n_bytes >= sizeof(GUID) )
                    {
                        ::memcpy(&m_value,
                                 data_ptr,
                                 sizeof(GUID) );
                    }

                    return true;
                }

            private:
                GUID m_value;
            };

            /* S*: a structure which the method writes (and, for a few, reads, too). */
            template<typename T>
            class ArgumentRestorer<T, Interception::Core::D3D8Utils::ArgumentCategory::MUTABLE_STRUCT> : public PassiveRestorer
            {
            public:
                typedef typename Interception::Core::D3D8Utils::PointeeOf<T>::type StructType;

                ArgumentRestorer()
                    :m_has_pointer(false),
                     m_value      ()
                {
                    /* Stub */
                }

                T get()
                {
                    return (m_has_pointer) ? &m_value
                                           : nullptr;
                }

                bool prepare(D3D8ReplayContext* in_context_ptr,
                             const size_t&      in_n_argument,
                             std::string*       out_error_ptr)
                {
                    const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);

                    (void) out_error_ptr;

                    if (Interception::Core::D3D8Utils::StructTraits<StructType>::is_input_output)
                    {
                        size_t         n_bytes  = 0;
                        const uint8_t* data_ptr = in_context_ptr->get_chunk_data_ptr(argument_ptr,
                                                                                     &n_bytes);

                        if (n_bytes >= sizeof(StructType) )
                        {
                            ::memcpy(&m_value,
                                     data_ptr,
                                     sizeof(StructType) );

                            m_has_pointer = true;

                            StructFixup<StructType>::apply(&m_value,
                                                           in_context_ptr);
                        }
                    }
                    else
                    {
                        /* An output structure is dumped after the call; before it, there is just the pointer. */
                        m_has_pointer = (argument_ptr != nullptr);

                        if (m_has_pointer)
                        {
                            m_has_pointer = (argument_ptr->type != APIInterceptor::ARGTYPE_DATA_CHUNK_ID);
                        }

                        if (m_has_pointer)
                        {
                            m_has_pointer = (argument_ptr->get_ptr_as_u64() != 0);
                        }
                    }

                    return true;
                }

            private:
                bool       m_has_pointer;
                StructType m_value;
            };

            /* I*: an interface which the replay has created. */
            template<typename T>
            class ArgumentRestorer<T, Interception::Core::D3D8Utils::ArgumentCategory::OBJECT> : public PassiveRestorer
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

                bool prepare(D3D8ReplayContext* in_context_ptr,
                             const size_t&      in_n_argument,
                             std::string*       out_error_ptr)
                {
                    const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);
                    void*      object_ptr   = nullptr;
                    bool       result       = false;

                    if (argument_ptr == nullptr)
                    {
                        *out_error_ptr = "A D3D8 call has too few arguments.";

                        goto end;
                    }

                    if (in_context_ptr->get_live_object(*argument_ptr,
                                                       &object_ptr,
                                                        out_error_ptr) == false)
                    {
                        goto end;
                    }

                    m_value = static_cast<T>(object_ptr);
                    result  = true;

                end:
                    return result;
                }

            private:
                T m_value;
            };

            /* I** and void**: a pointer which the method writes, and which the replay registers. */
            template<typename Pointee>
            class OutPointerRestorer
            {
            public:
                OutPointerRestorer()
                    :m_has_pointer(false),
                     m_value      (nullptr)
                {
                    /* Stub */
                }

                void finish(D3D8ReplayContext* in_context_ptr,
                            const size_t&      in_n_argument,
                            const bool&        in_is_success)
                {
                    const auto out_argument_ptr = in_context_ptr->get_out_argument_ptr(in_n_argument);

                    if (in_is_success               &&
                        m_value          != nullptr &&
                        out_argument_ptr != nullptr)
                    {
                        in_context_ptr->register_object(out_argument_ptr->get_ptr_as_u64(),
                                                        m_value);
                    }
                }

                Pointee** get()
                {
                    return (m_has_pointer) ? &m_value
                                           : nullptr;
                }

                bool prepare(D3D8ReplayContext* in_context_ptr,
                             const size_t&      in_n_argument,
                             std::string*       out_error_ptr)
                {
                    const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);

                    (void) out_error_ptr;

                    m_has_pointer = (argument_ptr != nullptr);

                    if (m_has_pointer)
                    {
                        m_has_pointer = (argument_ptr->get_ptr_as_u64() != 0);
                    }

                    if (m_has_pointer)
                    {
                        in_context_ptr->expect_creation();
                    }

                    return true;
                }

            private:
                bool     m_has_pointer;
                Pointee* m_value;
            };

            template<typename T>
            class ArgumentRestorer<T, Interception::Core::D3D8Utils::ArgumentCategory::OBJECT_OUT>
                : public OutPointerRestorer<typename Interception::Core::D3D8Utils::PointeeOf<typename Interception::Core::D3D8Utils::PointeeOf<T>::type>::type>
            {
                /* Stub */
            };

            template<typename T>
            class ArgumentRestorer<T, Interception::Core::D3D8Utils::ArgumentCategory::POINTER_OUT> : public OutPointerRestorer<void>
            {
                /* Stub */
            };

            /* Integers, floats and enums. */
            template<typename T,
                     bool     is_number = (std::is_arithmetic<T>::value || std::is_enum<T>::value)>
            class ScalarRestorer : public PassiveRestorer
            {
            public:
                ScalarRestorer()
                    :m_value()
                {
                    /* Stub */
                }

                T get() const
                {
                    return m_value;
                }

                bool prepare(D3D8ReplayContext* in_context_ptr,
                             const size_t&      in_n_argument,
                             std::string*       out_error_ptr)
                {
                    typedef typename Interception::Hooks::D3D8::ScalarConversion<T>::type NumberType;

                    const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);
                    NumberType number       = NumberType();
                    bool       result       = false;

                    if (argument_ptr == nullptr)
                    {
                        *out_error_ptr = "A D3D8 call has too few arguments.";

                        goto end;
                    }

                    if (read_number(*argument_ptr,
                                   &number) == false)
                    {
                        *out_error_ptr = "A D3D8 call has an argument of an unexpected kind.";

                        goto end;
                    }

                    /* Shader handles and state block tokens are DWORDs. */
                    if (std::is_integral<NumberType>::value)
                    {
                        number = static_cast<NumberType>(in_context_ptr->map_handle_argument(in_n_argument,
                                                                                             static_cast<uint32_t>(number) ));
                    }

                    m_value = static_cast<T>(number);
                    result  = true;

                end:
                    return result;
                }

            private:
                T m_value;
            };

            /* A pointer: the data the dumper stored apart from the arguments, if there is any. */
            template<typename T>
            class ScalarRestorer<T, false> : public PassiveRestorer
            {
            public:
                ScalarRestorer()
                    :m_value(nullptr)
                {
                    /* Stub */
                }

                T get() const
                {
                    return m_value;
                }

                bool prepare(D3D8ReplayContext* in_context_ptr,
                             const size_t&      in_n_argument,
                             std::string*       out_error_ptr)
                {
                    size_t n_bytes = 0;

                    (void) out_error_ptr;

                    m_value = reinterpret_cast<T>(const_cast<uint8_t*>(in_context_ptr->get_extra_data_ptr(in_n_argument,
                                                                                                          &n_bytes) ));

                    return true;
                }

            private:
                T m_value;
            };

            template<typename T>
            class ArgumentRestorer<T, Interception::Core::D3D8Utils::ArgumentCategory::SCALAR> : public ScalarRestorer<T>
            {
                /* Stub */
            };

            /* A window of the replay. */
            template<>
            class ArgumentRestorer<HWND, Interception::Core::D3D8Utils::ArgumentCategory::SCALAR> : public PassiveRestorer
            {
            public:
                ArgumentRestorer()
                    :m_value(nullptr)
                {
                    /* Stub */
                }

                HWND get() const
                {
                    return m_value;
                }

                bool prepare(D3D8ReplayContext* in_context_ptr,
                             const size_t&      in_n_argument,
                             std::string*       out_error_ptr)
                {
                    const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);
                    uint64_t   live_window  = 0;
                    bool       result       = false;

                    if (argument_ptr == nullptr)
                    {
                        *out_error_ptr = "A D3D8 call has too few arguments.";

                        goto end;
                    }

                    if (in_context_ptr->get_live_handle(ObjectKind::HWND,
                                                       *argument_ptr,
                                                       &live_window) == false)
                    {
                        *out_error_ptr = "A window which the replay has not created is used.";

                        goto end;
                    }

                    m_value = reinterpret_cast<HWND>(live_window);
                    result  = true;

                end:
                    return result;
                }

            private:
                HWND m_value;
            };

            /* BYTE**: where a lock gives out the memory. */
            template<>
            class ArgumentRestorer<BYTE**, Interception::Core::D3D8Utils::ArgumentCategory::SCALAR> : public PassiveRestorer
            {
            public:
                ArgumentRestorer()
                    :m_value(nullptr)
                {
                    /* Stub */
                }

                BYTE** get()
                {
                    return &m_value;
                }

                bool prepare(D3D8ReplayContext* in_context_ptr,
                             const size_t&      in_n_argument,
                             std::string*       out_error_ptr)
                {
                    (void) in_context_ptr;
                    (void) in_n_argument;
                    (void) out_error_ptr;

                    return true;
                }

            private:
                BYTE* m_value;
            };

            /* U*: an integer which the method writes; a handle which the method creates is registered. */
            template<typename T>
            class ArgumentRestorer<T, Interception::Core::D3D8Utils::ArgumentCategory::SCALAR_OUT>
            {
            public:
                typedef typename Interception::Core::D3D8Utils::PointeeOf<T>::type NumberType;

                ArgumentRestorer()
                    :m_has_pointer(false),
                     m_value      ()
                {
                    /* Stub */
                }

                void finish(D3D8ReplayContext* in_context_ptr,
                            const size_t&      in_n_argument,
                            const bool&        in_is_success)
                {
                    if (in_is_success &&
                        m_has_pointer)
                    {
                        in_context_ptr->register_handle(in_n_argument,
                                                        static_cast<uint32_t>(m_value) );
                    }
                }

                T get()
                {
                    return (m_has_pointer) ? &m_value
                                           : nullptr;
                }

                bool prepare(D3D8ReplayContext* in_context_ptr,
                             const size_t&      in_n_argument,
                             std::string*       out_error_ptr)
                {
                    const auto argument_ptr = in_context_ptr->get_in_argument_ptr(in_n_argument);

                    (void) out_error_ptr;

                    m_has_pointer = (argument_ptr != nullptr);

                    if (m_has_pointer)
                    {
                        m_has_pointer = (argument_ptr->get_ptr_as_u64() != 0);
                    }

                    return true;
                }

            private:
                bool       m_has_pointer;
                NumberType m_value;
            };
        }
    }
}

#endif /* STEREOREPLAYENGINEDLL_D3D8_ARGUMENT_RESTORER_H */
