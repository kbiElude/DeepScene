/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d8_call_executor.h"
#include "Interception/Hooks/D3D8/types.h"
#include "d3d8_replay_invoker.h"
#include "pixel_converter.h"
#include <string.h>

Replay::StereoReplayEngineDLL::D3D8CallExecutor::D3D8CallExecutor(ObjectRemapper* in_remapper_ptr)
    :m_context            (in_remapper_ptr),
     m_d3d8_module        (nullptr),
     m_lock_replayer      (&m_context),
     m_scratch_device_ptr (nullptr),
     m_scratch_surface_ptr(nullptr),
     m_state_block_token  (0)
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

void Replay::StereoReplayEngineDLL::D3D8CallExecutor::close_back_buffer_image(BackBufferImage* inout_image_ptr)
{
    unlock_back_buffer_image(inout_image_ptr);

    /* The image surface stays: it is the scratch surface, kept for the next read. */
    if (inout_image_ptr->back_buffer_ptr != nullptr)
    {
        void** const vtable_ptr = *reinterpret_cast<void***>(inout_image_ptr->back_buffer_ptr);

        reinterpret_cast<ULONG (__stdcall*)(void*)>(vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_RELEASE])(inout_image_ptr->back_buffer_ptr);
    }

    *inout_image_ptr = BackBufferImage();
}

bool Replay::StereoReplayEngineDLL::D3D8CallExecutor::create_direct3d8(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       std::string*                                 out_error_ptr)
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
                                                              const IChunkSource&                          in_chunk_source,
                                                              std::string*                                 out_error_ptr)
{
    constexpr size_t CREATE_DEVICE_ARGUMENT = 6; /* ppReturnedDeviceInterface follows the object and 5 parameters */

    const auto invoker_iterator = m_invoker_map.find(in_call.func);
    bool       result           = false;

    if (in_chunk_source.get_pointer_n_bytes() != sizeof(void*) )
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
                         &in_chunk_source,
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

    if (result                                                                  &&
        in_call.func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3D8_CREATEDEVICE)
    {
        m_context.set_device_from_argument(CREATE_DEVICE_ARGUMENT);
    }

end:
    return result;
}

Replay::StereoReplayEngineDLL::ExecutionClass Replay::StereoReplayEngineDLL::D3D8CallExecutor::get_execution_class(const Capture::APIDumpLoader::DumpedAPICall& in_call) const
{
    ExecutionClass result = ExecutionClass::FIRST_TIME_ONLY;

    if (D3D8LockReplayer::is_lock_function(in_call.func) )
    {
        result = ExecutionClass::EVERY_TIME;

        goto end;
    }

    if (m_release_function_set.find(in_call.func) != m_release_function_set.end() )
    {
        result = ExecutionClass::AFTER_ALL;

        goto end;
    }

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_BEGINSCENE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CAPTURESTATEBLOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_APPLYSTATEBLOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWRECTPATCH:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWTRIPATCH:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_ENDSCENE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_LIGHTENABLE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_MULTIPLYTRANSFORM:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PROCESSVERTICES:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETCLIPPLANE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETCLIPSTATUS:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETCURRENTTEXTUREPALETTE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETINDICES:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETLIGHT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETMATERIAL:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETPALETTEENTRIES:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETPIXELSHADER:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETPIXELSHADERCONSTANT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERSTATE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERTARGET:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETSTREAMSOURCE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTEXTURE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTEXTURESTAGESTATE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADER:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADERCONSTANT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVIEWPORT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_UPDATETEXTURE:
        {
            result = ExecutionClass::EVERY_TIME;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETEPATCH:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETEPIXELSHADER:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETESTATEBLOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETEVERTEXSHADER:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSWAPCHAIN8_PRESENT:
        {
            result = ExecutionClass::AFTER_ALL;

            break;
        }

        default:
        {
            break;
        }
    }

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

bool Replay::StereoReplayEngineDLL::D3D8CallExecutor::open_back_buffer_image(const bool&      in_copies_from_back_buffer,
                                                                             const DWORD&     in_lock_flags,
                                                                             BackBufferImage* out_image_ptr)
{
    void* device_ptr = m_context.get_device_ptr();
    bool  result     = false;

    *out_image_ptr = BackBufferImage();

    if (device_ptr == nullptr)
    {
        goto end;
    }

    {
        void** const device_vtable_ptr = *reinterpret_cast<void***>(device_ptr);

        if (reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_GETBACKBUFFER>(device_vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_GETBACKBUFFER])(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr),
                                                                                                                                                                                       0, /* BackBuffer */
                                                                                                                                                                                       Interception::Hooks::D3D8::D3DBACKBUFFER_TYPE_MONO,
                                                                                                                                                                                       reinterpret_cast<Interception::Hooks::D3D8::IDirect3DSurface8**>(&out_image_ptr->back_buffer_ptr) ) < 0)
        {
            goto end;
        }

        void** const back_buffer_vtable_ptr = *reinterpret_cast<void***>(out_image_ptr->back_buffer_ptr);

        if (reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_GETDESC>(back_buffer_vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_GETDESC])(static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(out_image_ptr->back_buffer_ptr),
                                                                                                                                                                                 &out_image_ptr->description) < 0)
        {
            goto end;
        }

        /* A scratch surface of another device, size or format is no use. */
        if ( m_scratch_surface_ptr        != nullptr                            &&
            (m_scratch_device_ptr         != device_ptr                         ||
             m_scratch_description.Width  != out_image_ptr->description.Width   ||
             m_scratch_description.Height != out_image_ptr->description.Height  ||
             m_scratch_description.Format != out_image_ptr->description.Format) )
        {
            release_scratch_surface();
        }

        if (m_scratch_surface_ptr == nullptr)
        {
            if (reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_CREATEIMAGESURFACE>(device_vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_CREATEIMAGESURFACE])(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr),
                                                                                                                                                                                                     out_image_ptr->description.Width,
                                                                                                                                                                                                     out_image_ptr->description.Height,
                                                                                                                                                                                                     out_image_ptr->description.Format,
                                                                                                                                                                                                     reinterpret_cast<Interception::Hooks::D3D8::IDirect3DSurface8**>(&m_scratch_surface_ptr) ) < 0)
            {
                goto end;
            }

            m_scratch_description = out_image_ptr->description;
            m_scratch_device_ptr  = device_ptr;
        }

        out_image_ptr->image_ptr = m_scratch_surface_ptr;

        if (in_copies_from_back_buffer)
        {
            if (reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_COPYRECTS>(device_vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_COPYRECTS])(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr),
                                                                                                                                                                                   static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(out_image_ptr->back_buffer_ptr),
                                                                                                                                                                                   nullptr, /* pSourceRectsArray */
                                                                                                                                                                                   0,       /* cRects             */
                                                                                                                                                                                   static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(out_image_ptr->image_ptr),
                                                                                                                                                                                   nullptr) < 0) /* pDestPointsArray */
            {
                goto end;
            }
        }

        void** const image_vtable_ptr = *reinterpret_cast<void***>(out_image_ptr->image_ptr);

        if (reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_LOCKRECT>(image_vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_LOCKRECT])(static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(out_image_ptr->image_ptr),
                                                                                                                                                                             &out_image_ptr->locked_rect,
                                                                                                                                                                              nullptr, /* pRect */
                                                                                                                                                                              in_lock_flags) < 0)
        {
            goto end;
        }

        out_image_ptr->is_locked = true;
    }

    result = true;

end:
    if (result == false)
    {
        close_back_buffer_image(out_image_ptr);
    }

    return result;
}

bool Replay::StereoReplayEngineDLL::D3D8CallExecutor::read_back_buffer(std::vector<uint8_t>* out_pixel_u8_vec_ptr,
                                                                       FrameImage*           out_image_ptr)
{
    BackBufferImage image;
    uint32_t        n_bytes_per_pixel = 0;
    uint32_t        n_row_bytes       = 0;
    bool            result            = false;

    if (open_back_buffer_image(true, /* in_copies_from_back_buffer */
                               Interception::Hooks::D3D8::D3DLOCK_READONLY,
                              &image) == false)
    {
        goto end;
    }

    if (out_pixel_u8_vec_ptr != nullptr)
    {
        n_bytes_per_pixel = PixelConverter::get_bytes_per_pixel(image.description.Format);

        if (n_bytes_per_pixel == 0)
        {
            goto end;
        }

        n_row_bytes = image.description.Width * n_bytes_per_pixel;

        out_pixel_u8_vec_ptr->resize(static_cast<size_t>(n_row_bytes) * image.description.Height);

        for (uint32_t n_row = 0;
                      n_row < image.description.Height;
                    ++n_row)
        {
            ::memcpy(out_pixel_u8_vec_ptr->data ()                        + static_cast<size_t>(n_row) * n_row_bytes,
                     static_cast<const uint8_t*>(image.locked_rect.pBits) + static_cast<size_t>(n_row) * image.locked_rect.Pitch,
                     n_row_bytes);
        }
    }

    if (out_image_ptr != nullptr)
    {
        if (PixelConverter::convert_to_bgra8(image.description.Format,
                                             image.description.Width,
                                             image.description.Height,
                                             static_cast<uint32_t>(image.locked_rect.Pitch),
                                             static_cast<const uint8_t*>(image.locked_rect.pBits),
                                            &m_capture_u8_vec) == false)
        {
            goto end;
        }

        out_image_ptr->height    = image.description.Height;
        out_image_ptr->pixel_ptr = m_capture_u8_vec.data();
        out_image_ptr->width     = image.description.Width;
    }

    result = true;

end:
    close_back_buffer_image(&image);

    return result;
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
        /* The scratch surface keeps the device alive. */
        if (object_ptr == m_context.get_device_ptr() )
        {
            release_scratch_surface();
        }

        void** const vtable_ptr = *reinterpret_cast<void***>(object_ptr);
        const ULONG  n_refs     = reinterpret_cast<PFNRELEASEPROC>(vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_RELEASE])(object_ptr);

        if (n_refs == 0)
        {
            m_context.forget_object(in_call.arg_in_vec.at(0).get_ptr_as_u64() );
        }
    }

    return true;
}

void Replay::StereoReplayEngineDLL::D3D8CallExecutor::release_scratch_surface()
{
    if (m_scratch_surface_ptr != nullptr)
    {
        void** const vtable_ptr = *reinterpret_cast<void***>(m_scratch_surface_ptr);

        reinterpret_cast<ULONG (__stdcall*)(void*)>(vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_RELEASE])(m_scratch_surface_ptr);

        m_scratch_surface_ptr = nullptr;
    }

    m_scratch_description = {};
    m_scratch_device_ptr  = nullptr;
}

void Replay::StereoReplayEngineDLL::D3D8CallExecutor::reset()
{
    m_state_block_token = 0;

    release_scratch_surface      ();
    m_lock_replayer.reset        ();
    m_context.release_all_objects();
}

void Replay::StereoReplayEngineDLL::D3D8CallExecutor::restore_state()
{
    void* const device_ptr = m_context.get_device_ptr();

    if (device_ptr != nullptr &&
        m_state_block_token != 0)
    {
        void** const vtable_ptr = *reinterpret_cast<void***>(device_ptr);

        reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_APPLYSTATEBLOCK>(vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_APPLYSTATEBLOCK])(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr),
                                                                                                                                                                                m_state_block_token);
    }
}

void Replay::StereoReplayEngineDLL::D3D8CallExecutor::save_state()
{
    void* const device_ptr = m_context.get_device_ptr();

    if (device_ptr != nullptr)
    {
        void** const vtable_ptr = *reinterpret_cast<void***>(device_ptr);

        if (m_state_block_token == 0)
        {
            /* Creating a state block captures the state. */
            reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_CREATESTATEBLOCK>(vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_CREATESTATEBLOCK])(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr),
                                                                                                                                                                                      Interception::Hooks::D3D8::D3DSBT_ALL,
                                                                                                                                                                                      &m_state_block_token);
        }
        else
        {
            reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_CAPTURESTATEBLOCK>(vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_CAPTURESTATEBLOCK])(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr),
                                                                                                                                                                                        m_state_block_token);
        }
    }
}

void Replay::StereoReplayEngineDLL::D3D8CallExecutor::unlock_back_buffer_image(BackBufferImage* inout_image_ptr)
{
    if (inout_image_ptr->is_locked)
    {
        void** const image_vtable_ptr = *reinterpret_cast<void***>(inout_image_ptr->image_ptr);

        reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_UNLOCKRECT>(image_vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_UNLOCKRECT])(static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(inout_image_ptr->image_ptr) );

        inout_image_ptr->is_locked = false;
    }
}

bool Replay::StereoReplayEngineDLL::D3D8CallExecutor::write_back_buffer(const std::vector<uint8_t>& in_pixel_u8_vec)
{
    BackBufferImage image;
    uint32_t        n_bytes_per_pixel = 0;
    uint32_t        n_row_bytes       = 0;
    bool            result            = false;

    if (open_back_buffer_image(false, /* in_copies_from_back_buffer */
                               0,     /* in_lock_flags */
                              &image) == false)
    {
        goto end;
    }

    n_bytes_per_pixel = PixelConverter::get_bytes_per_pixel(image.description.Format);
    n_row_bytes       = image.description.Width                                       * n_bytes_per_pixel;

    if (n_bytes_per_pixel      == 0                                                           ||
        in_pixel_u8_vec.size() != static_cast<size_t>(n_row_bytes) * image.description.Height)
    {
        goto end;
    }

    for (uint32_t n_row = 0;
                  n_row < image.description.Height;
                ++n_row)
    {
        ::memcpy(static_cast<uint8_t*>(image.locked_rect.pBits) + static_cast<size_t>(n_row) * image.locked_rect.Pitch,
                 in_pixel_u8_vec.data ()                        + static_cast<size_t>(n_row) * n_row_bytes,
                 n_row_bytes);
    }

    unlock_back_buffer_image(&image);

    {
        void* const  device_ptr        = m_context.get_device_ptr();
        void** const device_vtable_ptr = *reinterpret_cast<void***>(device_ptr);

        result = (reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_COPYRECTS>(device_vtable_ptr[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_COPYRECTS])(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*> (device_ptr),
                                                                                                                                                                                     static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(image.image_ptr),
                                                                                                                                                                                     nullptr, /* pSourceRectsArray */
                                                                                                                                                                                     0,       /* cRects             */
                                                                                                                                                                                     static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(image.back_buffer_ptr),
                                                                                                                                                                                     nullptr) >= 0); /* pDestPointsArray */
    }

end:
    close_back_buffer_image(&image);

    return result;
}
