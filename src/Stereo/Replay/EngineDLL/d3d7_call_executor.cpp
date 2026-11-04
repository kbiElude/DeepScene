/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Common/vtable.h"
#include "d3d7_call_executor.h"
#include "Interception/Core/D3D7Utils/argument_info.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Core/D3D7Utils/methods.h"
#include "Interception/Core/D3D7Utils/value_readers.h"
#include "pixel_converter.h"
#include <string.h>

/* The values of the headers of DirectX 7 which only the replay needs. */
static constexpr DWORD DDLOCK_EVENT_FLAG           = 0x00000002;
static constexpr DWORD DDLOCK_READONLY_FLAG        = 0x00000010;
static constexpr DWORD DDLOCK_WAIT_FLAG            = 0x00000001;
static constexpr DWORD DDLOCK_WRITEONLY_FLAG       = 0x00000020;
static constexpr DWORD DDSCL_FPUPRESERVE_FLAG      = 0x00001000;
static constexpr DWORD DDSCL_FPUSETUP_FLAG         = 0x00000800;
static constexpr DWORD DDSCL_MULTITHREADED_FLAG    = 0x00000400;
static constexpr DWORD DDSCL_NORMAL_FLAG           = 0x00000008;
static constexpr DWORD DDSD_FLAGS_FOR_SIZED_SURFACE = Interception::Core::D3D7Utils::DDSD_CAPS | Interception::Core::D3D7Utils::DDSD_HEIGHT | Interception::Core::D3D7Utils::DDSD_WIDTH;
static constexpr size_t IID_N_BYTES                = sizeof(GUID);

template<typename ReturnType, typename... Arguments>
ReturnType Stereo::Replay::EngineDLL::D3D7CallExecutor::call_method(void*           in_object_ptr,
                                                                    const uint32_t& in_vtable_index,
                                                                    Arguments...    in_arguments)
{
    typedef ReturnType (__stdcall* MethodPtr)(void*, Arguments...);

    void** const vtable_ptr = APIInterceptor::get_vtable_ptr(in_object_ptr);

    return APIInterceptor::get_method_ptr<MethodPtr>(vtable_ptr, in_vtable_index)(in_object_ptr,
                                                                    in_arguments...);
}

Stereo::Replay::EngineDLL::D3D7CallExecutor::D3D7CallExecutor(ObjectRemapper* in_remapper_ptr)
    :m_add_ref_function_set    (),
     m_back_buffer_ptr         (nullptr),
     m_clipper_ptr             (nullptr),
     m_context                 (in_remapper_ptr),
     m_device_ptr              (nullptr),
     m_direct_draw_ptr         (nullptr),
     m_display_height          (0),
     m_display_width           (0),
     m_eye                     (Analysis::RecipeLoader::Eye::LEFT),
     m_is_building_twin        (false),
     m_locked_image_ptr        (nullptr),
     m_needs_twin              (false),
     m_primary_ptr             (nullptr),
     m_render_target_ptr       (nullptr),
     m_window                  (nullptr)
{
    /* AddRef and Release are methods of every interface. */
    #define D3D7_METHOD(INTERFACE_TAG, interface_name, METHOD_TAG, method_name, vtable_index, return_type, parameters) \
        if (::strcmp(#method_name, "AddRef") == 0) \
        { \
            m_add_ref_function_set.insert(APIInterceptor::APIFUNCTION_D3D7_##INTERFACE_TAG##_##METHOD_TAG); \
            m_handler_map[APIInterceptor::APIFUNCTION_D3D7_##INTERFACE_TAG##_##METHOD_TAG] = &D3D7CallExecutor::add_ref; \
        } \
        else \
        if (::strcmp(#method_name, "Release") == 0) \
        { \
            m_release_function_set.insert(APIInterceptor::APIFUNCTION_D3D7_##INTERFACE_TAG##_##METHOD_TAG); \
            m_handler_map[APIInterceptor::APIFUNCTION_D3D7_##INTERFACE_TAG##_##METHOD_TAG] = &D3D7CallExecutor::release; \
        } \
        else \
        if (::strcmp(#method_name, "QueryInterface") == 0) \
        { \
            m_handler_map[APIInterceptor::APIFUNCTION_D3D7_##INTERFACE_TAG##_##METHOD_TAG] = &D3D7CallExecutor::query_interface; \
        }

    #include "Interception/Hooks/D3D7/d3d7_methods.inl"

    #undef D3D7_METHOD

    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_DIRECTDRAWCREATEEX]                          = &D3D7CallExecutor::create_direct_draw;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3D7_CREATEDEVICE]                     = &D3D7CallExecutor::create_device;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_BEGINSCENE]                 = &D3D7CallExecutor::begin_scene;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR]                      = &D3D7CallExecutor::clear;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE]       = &D3D7CallExecutor::draw_indexed_primitive;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE]              = &D3D7CallExecutor::draw_primitive;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_ENDSCENE]                   = &D3D7CallExecutor::end_scene;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LIGHTENABLE]                = &D3D7CallExecutor::light_enable;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LOAD]                       = &D3D7CallExecutor::load;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETLIGHT]                   = &D3D7CallExecutor::set_light;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETMATERIAL]                = &D3D7CallExecutor::set_material;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE]             = &D3D7CallExecutor::set_render_state;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERTARGET]            = &D3D7CallExecutor::set_render_target;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTEXTURE]                 = &D3D7CallExecutor::set_texture;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTEXTURESTAGESTATE]       = &D3D7CallExecutor::set_texture_stage_state;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTRANSFORM]               = &D3D7CallExecutor::set_transform;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETVIEWPORT]                = &D3D7CallExecutor::set_viewport;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE]                  = &D3D7CallExecutor::create_surface;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETCOOPERATIVELEVEL]            = &D3D7CallExecutor::set_cooperative_level;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETDISPLAYMODE]                 = &D3D7CallExecutor::set_display_mode;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE]      = &D3D7CallExecutor::add_attached_surface;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT]                     = &D3D7CallExecutor::blt;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST]                 = &D3D7CallExecutor::blt_fast;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP]                    = &D3D7CallExecutor::flip;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE]      = &D3D7CallExecutor::get_attached_surface;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_LOCK]                    = &D3D7CallExecutor::lock;
    m_handler_map[APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_UNLOCK]                  = &D3D7CallExecutor::unlock;
}

Stereo::Replay::EngineDLL::D3D7CallExecutor::~D3D7CallExecutor()
{
    reset();
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::add_attached_surface(std::string* out_error_ptr)
{
    constexpr size_t ATTACHED_ARGUMENT = 1;
    constexpr size_t SURFACE_ARGUMENT  = 0;

    void* attached_ptr = nullptr;
    void* surface_ptr  = nullptr;
    bool  result       = false;

    if (get_live_object(SURFACE_ARGUMENT,
                       &surface_ptr,
                        out_error_ptr) == false ||
        get_live_object(ATTACHED_ARGUMENT,
                       &attached_ptr,
                        out_error_ptr) == false)
    {
        goto end;
    }

    result = (call_method<HRESULT>(surface_ptr,
                                   Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE,
                                   attached_ptr) >= 0);

    if (result == false)
    {
        *out_error_ptr = "AddAttachedSurface failed in the replay.";
    }

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::add_ref(std::string* out_error_ptr)
{
    void* object_ptr = nullptr;
    bool  result     = get_live_object(0,
                                      &object_ptr,
                                       out_error_ptr);

    if (result &&
        object_ptr != nullptr)
    {
        call_method<ULONG>(object_ptr,
                           Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_ADDREF);

        m_reference_count_map[object_ptr]++;
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::begin_scene(std::string* out_error_ptr)
{
    void* device_ptr = nullptr;
    bool  result     = get_live_object(0,
                                      &device_ptr,
                                       out_error_ptr);

    if (result &&
        device_ptr != nullptr)
    {
        call_method<HRESULT>(device_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_BEGINSCENE);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::blt(std::string* out_error_ptr)
{
    constexpr size_t DESTINATION_RECT_ARGUMENT = 1;
    constexpr size_t EFFECTS_ARGUMENT          = 5;
    constexpr size_t FLAGS_ARGUMENT            = 4;
    constexpr size_t SOURCE_ARGUMENT           = 2;
    constexpr size_t SOURCE_RECT_ARGUMENT      = 3;

    uint32_t flags      = 0;
    void*    source_ptr = nullptr;
    void*    surface_ptr = nullptr;
    bool     result     = false;

    if (get_live_object(0,
                       &surface_ptr,
                        out_error_ptr) == false ||
        get_live_object(SOURCE_ARGUMENT,
                       &source_ptr,
                        out_error_ptr) == false ||
        get_u32(FLAGS_ARGUMENT,
               &flags)                 == false)
    {
        goto end;
    }

    result = (call_method<HRESULT>(surface_ptr,
                                   Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_BLT,
                                   reinterpret_cast<const RECT*>(get_chunk_ptr(DESTINATION_RECT_ARGUMENT) ),
                                   source_ptr,
                                   reinterpret_cast<const RECT*>(get_chunk_ptr(SOURCE_RECT_ARGUMENT) ),
                                   static_cast<DWORD>(flags),
                                   const_cast<uint8_t*>(get_chunk_ptr(EFFECTS_ARGUMENT) ) ) >= 0);

    if (result == false)
    {
        *out_error_ptr = "Blt failed in the replay.";
    }

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::blt_fast(std::string* out_error_ptr)
{
    constexpr size_t FLAGS_ARGUMENT       = 5;
    constexpr size_t SOURCE_ARGUMENT      = 3;
    constexpr size_t SOURCE_RECT_ARGUMENT = 4;
    constexpr size_t X_ARGUMENT           = 1;
    constexpr size_t Y_ARGUMENT           = 2;

    uint32_t flags       = 0;
    void*    source_ptr  = nullptr;
    void*    surface_ptr = nullptr;
    uint32_t x           = 0;
    uint32_t y           = 0;
    bool     result      = false;

    if (get_live_object(0,
                       &surface_ptr,
                        out_error_ptr) == false ||
        get_live_object(SOURCE_ARGUMENT,
                       &source_ptr,
                        out_error_ptr) == false ||
        get_u32(FLAGS_ARGUMENT,
               &flags)                 == false ||
        get_u32(X_ARGUMENT,
               &x)                     == false ||
        get_u32(Y_ARGUMENT,
               &y)                     == false)
    {
        goto end;
    }

    result = (call_method<HRESULT>(surface_ptr,
                                   Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_BLTFAST,
                                   static_cast<DWORD>(x),
                                   static_cast<DWORD>(y),
                                   source_ptr,
                                   reinterpret_cast<const RECT*>(get_chunk_ptr(SOURCE_RECT_ARGUMENT) ),
                                   static_cast<DWORD>(flags) ) >= 0);

    if (result == false)
    {
        *out_error_ptr = "BltFast failed in the replay.";
    }

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::clear(std::string* out_error_ptr)
{
    constexpr size_t COLOR_ARGUMENT   = 4;
    constexpr size_t DEPTH_ARGUMENT   = 5;
    constexpr size_t FLAGS_ARGUMENT   = 3;
    constexpr size_t N_RECTS_ARGUMENT = 1;
    constexpr size_t RECTS_ARGUMENT   = 7; /* follows the arguments of the method, in a data chunk */
    constexpr size_t STENCIL_ARGUMENT = 6;

    uint32_t color     = 0;
    float    depth     = 0.0f;
    void*    device_ptr = nullptr;
    uint32_t flags     = 0;
    uint32_t n_rects   = 0;
    uint32_t stencil   = 0;
    bool     result    = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_u32(COLOR_ARGUMENT,
               &color)                 == false ||
        get_u32(FLAGS_ARGUMENT,
               &flags)                 == false ||
        get_u32(N_RECTS_ARGUMENT,
               &n_rects)               == false ||
        get_u32(STENCIL_ARGUMENT,
               &stencil)               == false ||
        read_number(*m_context.get_in_argument_ptr(DEPTH_ARGUMENT),
                    &depth)            == false)
    {
        goto end;
    }

    result = true;

    call_method<HRESULT>(device_ptr,
                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_CLEAR,
                         static_cast<DWORD>(n_rects),
                         reinterpret_cast<const Interception::Hooks::D3D7::D3DRECT*>(get_chunk_ptr(RECTS_ARGUMENT) ),
                         static_cast<DWORD>(flags),
                         static_cast<DWORD>(color),
                         depth,
                         static_cast<DWORD>(stencil) );

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::create_device(std::string* out_error_ptr)
{
    constexpr size_t DEVICE_ARGUMENT = 3;
    constexpr size_t IID_ARGUMENT    = 1;
    constexpr size_t SURFACE_ARGUMENT = 2;

    void*          device_ptr  = nullptr;
    void*          direct3d_ptr = nullptr;
    size_t         n_iid_bytes = 0;
    const uint8_t* iid_ptr     = get_chunk_ptr(IID_ARGUMENT,
                                              &n_iid_bytes);
    void*          surface_ptr = nullptr;
    bool           result      = false;

    if (get_live_object(0,
                       &direct3d_ptr,
                        out_error_ptr) == false ||
        get_live_object(SURFACE_ARGUMENT,
                       &surface_ptr,
                        out_error_ptr) == false)
    {
        goto end;
    }

    if (iid_ptr     == nullptr ||
        n_iid_bytes != IID_N_BYTES)
    {
        *out_error_ptr = "CreateDevice has no device type in the dump.";

        goto end;
    }

    if (call_method<HRESULT, const GUID&, void*, void**>(direct3d_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3D7_CREATEDEVICE,
                             *reinterpret_cast<const GUID*>(iid_ptr),
                             surface_ptr,
                            &device_ptr) < 0)
    {
        *out_error_ptr = "CreateDevice failed in the replay.";

        goto end;
    }

    register_object(DEVICE_ARGUMENT,
                    device_ptr);

    m_render_target_ptr = surface_ptr;
    result              = true;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::create_direct_draw(std::string* out_error_ptr)
{
    typedef HRESULT (__stdcall* PFNDIRECTDRAWCREATEEXPROC)(GUID*, void**, const GUID&, IUnknown*);

    constexpr size_t DIRECT_DRAW_ARGUMENT = 1;
    constexpr size_t IID_ARGUMENT         = 2;

    void*          direct_draw_ptr = nullptr;
    size_t         n_iid_bytes     = 0;
    const uint8_t* iid_ptr         = get_chunk_ptr(IID_ARGUMENT,
                                                  &n_iid_bytes);
    HMODULE        module          = ::LoadLibraryA("ddraw.dll");
    bool           result          = false;

    const auto create_func_ptr = reinterpret_cast<PFNDIRECTDRAWCREATEEXPROC>((module != nullptr) ? ::GetProcAddress(module,
                                                                                                                    "DirectDrawCreateEx")
                                                                                                  : nullptr);

    if (create_func_ptr == nullptr ||
        iid_ptr         == nullptr ||
        n_iid_bytes     != IID_N_BYTES)
    {
        *out_error_ptr = "DirectDrawCreateEx cannot be called in the replay.";

        goto end;
    }

    /* The device of the dump is not known (the dumper stores no more than the pointer to its GUID): the primary one. */
    if (create_func_ptr(nullptr,
                       &direct_draw_ptr,
                        *reinterpret_cast<const GUID*>(iid_ptr),
                        nullptr) < 0)
    {
        *out_error_ptr = "DirectDrawCreateEx failed in the replay.";

        goto end;
    }

    register_object(DIRECT_DRAW_ARGUMENT,
                    direct_draw_ptr);

    result = true;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::create_surface(std::string* out_error_ptr)
{
    constexpr size_t DESCRIPTION_ARGUMENT = 1;
    constexpr size_t SURFACE_ARGUMENT     = 2;

    Interception::Hooks::D3D7::DDSURFACEDESC2 description = {};
    void*                                     direct_draw_ptr = nullptr;
    size_t                                    n_description_bytes = 0;
    const uint8_t*                            description_ptr = get_chunk_ptr(DESCRIPTION_ARGUMENT,
                                                                             &n_description_bytes);
    bool                                      is_primary = false;
    void*                                     surface_ptr = nullptr;
    bool                                      result = false;

    if (get_live_object(0,
                       &direct_draw_ptr,
                        out_error_ptr) == false)
    {
        goto end;
    }

    if (description_ptr     == nullptr ||
        n_description_bytes != sizeof(description) )
    {
        *out_error_ptr = "CreateSurface has no description in the dump.";

        goto end;
    }

    ::memcpy(&description,
             description_ptr,
             sizeof(description) );

    is_primary = ( (description.ddsCaps.dwCaps & Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE) != 0);

    /* The primary surface shows the window; the chain of buffers that it flips with is the replay's own (see get_attached_surface()). */
    if (is_primary)
    {
        description.dwBackBufferCount = 0;
        description.dwFlags           = Interception::Core::D3D7Utils::DDSD_CAPS;
        description.ddsCaps.dwCaps    = Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE;
    }

    if (call_method<HRESULT>(direct_draw_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAW7_CREATESURFACE,
                             &description,
                             &surface_ptr,
                             static_cast<IUnknown*>(nullptr) ) < 0)
    {
        *out_error_ptr = "CreateSurface failed in the replay.";

        goto end;
    }

    register_object(SURFACE_ARGUMENT,
                    surface_ptr);

    if (is_primary)
    {
        m_direct_draw_ptr = direct_draw_ptr;
        m_primary_ptr     = surface_ptr;

        /* Only the window is drawn to. */
        if (m_window != nullptr)
        {
            if (call_method<HRESULT>(direct_draw_ptr,
                                     Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAW7_CREATECLIPPER,
                                     static_cast<DWORD>(0),
                                     &m_clipper_ptr,
                                     static_cast<IUnknown*>(nullptr) ) >= 0)
            {
                m_live_objects.add(m_clipper_ptr);

                m_reference_count_map[m_clipper_ptr]++;

                call_method<HRESULT>(m_clipper_ptr,
                                     Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWCLIPPER_SETHWND,
                                     static_cast<DWORD>(0),
                                     m_window);
                call_method<HRESULT>(surface_ptr,
                                     Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_SETCLIPPER,
                                     m_clipper_ptr);
            }
        }
    }

    result = true;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::draw_indexed_primitive(std::string* out_error_ptr)
{
    constexpr size_t FLAGS_ARGUMENT      = 7;
    constexpr size_t FVF_ARGUMENT        = 2;
    constexpr size_t INDICES_ARGUMENT    = 8; /* follow the arguments of the method, in data chunks */
    constexpr size_t N_INDICES_ARGUMENT  = 6;
    constexpr size_t N_VERTICES_ARGUMENT = 4;
    constexpr size_t TYPE_ARGUMENT       = 1;
    constexpr size_t VERTICES_ARGUMENT   = 9;

    void*    device_ptr = nullptr;
    uint32_t flags      = 0;
    uint32_t fvf        = 0;
    uint32_t n_indices  = 0;
    uint32_t n_vertices = 0;
    uint32_t type       = 0;
    bool     result     = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_u32(FLAGS_ARGUMENT,
               &flags)                 == false ||
        get_u32(FVF_ARGUMENT,
               &fvf)                   == false ||
        get_u32(N_INDICES_ARGUMENT,
               &n_indices)             == false ||
        get_u32(N_VERTICES_ARGUMENT,
               &n_vertices)            == false ||
        get_u32(TYPE_ARGUMENT,
               &type)                  == false)
    {
        goto end;
    }

    result = true;

    call_method<HRESULT>(device_ptr,
                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE,
                         static_cast<Interception::Hooks::D3D7::D3DPRIMITIVETYPE>(type),
                         static_cast<DWORD>(fvf),
                         const_cast<uint8_t*>(get_chunk_ptr(VERTICES_ARGUMENT) ),
                         static_cast<DWORD>(n_vertices),
                         reinterpret_cast<WORD*>(const_cast<uint8_t*>(get_chunk_ptr(INDICES_ARGUMENT) )),
                         static_cast<DWORD>(n_indices),
                         static_cast<DWORD>(flags) );

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::draw_primitive(std::string* out_error_ptr)
{
    constexpr size_t FLAGS_ARGUMENT      = 5;
    constexpr size_t FVF_ARGUMENT        = 2;
    constexpr size_t N_VERTICES_ARGUMENT = 4;
    constexpr size_t TYPE_ARGUMENT       = 1;
    constexpr size_t VERTICES_ARGUMENT   = 6; /* follow the arguments of the method, in a data chunk */

    void*    device_ptr = nullptr;
    uint32_t flags      = 0;
    uint32_t fvf        = 0;
    uint32_t n_vertices = 0;
    uint32_t type       = 0;
    bool     result     = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_u32(FLAGS_ARGUMENT,
               &flags)                 == false ||
        get_u32(FVF_ARGUMENT,
               &fvf)                   == false ||
        get_u32(N_VERTICES_ARGUMENT,
               &n_vertices)            == false ||
        get_u32(TYPE_ARGUMENT,
               &type)                  == false)
    {
        goto end;
    }

    result = true;

    call_method<HRESULT>(device_ptr,
                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_DRAWPRIMITIVE,
                         static_cast<Interception::Hooks::D3D7::D3DPRIMITIVETYPE>(type),
                         static_cast<DWORD>(fvf),
                         const_cast<uint8_t*>(get_chunk_ptr(VERTICES_ARGUMENT) ),
                         static_cast<DWORD>(n_vertices),
                         static_cast<DWORD>(flags) );

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::end_scene(std::string* out_error_ptr)
{
    void* device_ptr = nullptr;
    bool  result     = get_live_object(0,
                                      &device_ptr,
                                       out_error_ptr);

    if (result &&
        device_ptr != nullptr)
    {
        call_method<HRESULT>(device_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_ENDSCENE);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::execute(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                          const IChunkSource&                          in_chunk_source,
                                                          std::string*                                 out_error_ptr)
{
    const auto handler_iterator = m_handler_map.find(in_call.func);
    bool       result           = false;

    if (in_chunk_source.get_pointer_n_bytes() != sizeof(void*) )
    {
        *out_error_ptr = "The workload was dumped by a process of another pointer size: use the 32-bit player for D3D7.";

        goto end;
    }

    m_context.begin_call(&in_call,
                         &in_chunk_source,
                         Interception::Core::D3D7Utils::get_n_arguments(in_call.func) );

    /* A call which failed in the dump changed nothing. */
    if (in_call.returned_value_ptr       != nullptr                     &&
        in_call.returned_value_ptr->type == APIInterceptor::ARGTYPE_I32 &&
        in_call.returned_value_ptr->get_i32() < 0)
    {
        result = true;

        goto end;
    }

    if (handler_iterator != m_handler_map.end() )
    {
        m_needs_twin = false;

        result = (this->*handler_iterator->second)(out_error_ptr);

        /* What sets up an object that each eye has a copy of is made for both: the call that makes the copy, and the ones that follow it. The
         * calls that are made in each execution of a frame are made for the eye of the execution.
         */
        if (result            &&
            m_is_building_twin == false)
        {
            const bool is_setup = (m_needs_twin                                                          ||
                                   (get_execution_class(in_call) == ExecutionClass::FIRST_TIME_ONLY &&
                                    involves_per_eye_object() ) );

            if (is_setup)
            {
                const Analysis::RecipeLoader::Eye eye = m_eye;

                m_eye              = Analysis::RecipeLoader::Eye::RIGHT;
                m_is_building_twin = true;

                result = (this->*handler_iterator->second)(out_error_ptr);

                m_is_building_twin = false;
                m_eye              = eye;
            }
        }
    }
    else
    {
        const auto* method_info_ptr = Interception::Core::D3D7Utils::get_method_info_ptr(in_call.func);

        /* What asks the API about itself, and what the dump holds of the enumerations (the calls the application made in them are there too),
         * is not replayed. Any other call is something the replay has no code for.
         */
        if (method_info_ptr == nullptr ||
            is_query_method(method_info_ptr->method_name_ptr) )
        {
            result = true;
        }
        else
        {
            *out_error_ptr = std::string(method_info_ptr->full_name_ptr) + " is not replayed yet.";
        }
    }

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::flip(std::string* out_error_ptr)
{
    (void) out_error_ptr;

    /* The window shows what the application has drawn. */
    present_to_window();

    return true;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::get_attached_surface(std::string* out_error_ptr)
{
    constexpr size_t ATTACHED_ARGUMENT = 2;
    constexpr size_t CAPS_ARGUMENT     = 1;

    size_t         n_caps_bytes = 0;
    const uint8_t* caps_ptr     = get_chunk_ptr(CAPS_ARGUMENT,
                                                &n_caps_bytes);
    uint32_t       caps         = 0;
    void*          surface_ptr  = nullptr;
    void*          attached_ptr = nullptr;
    bool           result       = false;

    if (get_live_object(0,
                       &surface_ptr,
                        out_error_ptr) == false)
    {
        goto end;
    }

    if (caps_ptr     == nullptr ||
        n_caps_bytes <  sizeof(caps) )
    {
        *out_error_ptr = "GetAttachedSurface has no caps in the dump.";

        goto end;
    }

    ::memcpy(&caps,
             caps_ptr,
             sizeof(caps) );

    if ( (caps & Interception::Core::D3D7Utils::DDSCAPS_BACKBUFFER) != 0 &&
        surface_ptr == m_primary_ptr)
    {
        /* The back buffer of the chain is one of the replay's own, as large as the screen of the dump, and a 3D device can render to it. */
        if (m_back_buffer_ptr == nullptr)
        {
            Interception::Hooks::D3D7::DDSURFACEDESC2 description = {};

            description.dwFlags        = DDSD_FLAGS_FOR_SIZED_SURFACE;
            description.dwHeight       = m_display_height;
            description.dwSize         = sizeof(description);
            description.dwWidth        = m_display_width;
            description.ddsCaps.dwCaps = Interception::Core::D3D7Utils::DDSCAPS_3DDEVICE | Interception::Core::D3D7Utils::DDSCAPS_OFFSCREENPLAIN | Interception::Core::D3D7Utils::DDSCAPS_VIDEOMEMORY;

            if (call_method<HRESULT>(m_direct_draw_ptr,
                                     Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAW7_CREATESURFACE,
                                     &description,
                                     &m_back_buffer_ptr,
                                     static_cast<IUnknown*>(nullptr) ) < 0)
            {
                *out_error_ptr = "The back buffer cannot be made in the replay.";

                goto end;
            }

            m_live_objects.add(m_back_buffer_ptr);

            m_reference_count_map[m_back_buffer_ptr]++; /* the replay's own */
        }

        /* The application's reference. */
        call_method<ULONG>(m_back_buffer_ptr,
                           Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_ADDREF);

        attached_ptr = m_back_buffer_ptr;
    }
    else
    if (call_method<HRESULT>(surface_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE,
                             reinterpret_cast<const Interception::Hooks::D3D7::DDSCAPS2*>(caps_ptr),
                             &attached_ptr) < 0)
    {
        *out_error_ptr = "GetAttachedSurface failed in the replay.";

        goto end;
    }

    register_object(ATTACHED_ARGUMENT,
                    attached_ptr);

    result = true;

end:
    return result;
}

Stereo::Replay::EngineDLL::ExecutionClass Stereo::Replay::EngineDLL::D3D7CallExecutor::get_execution_class(const Capture::APIDumpLoader::DumpedAPICall& in_call) const
{
    ExecutionClass result = ExecutionClass::FIRST_TIME_ONLY;

    if (m_release_function_set.find(in_call.func) != m_release_function_set.end() )
    {
        result = ExecutionClass::AFTER_ALL;

        goto end;
    }

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_BEGINSCENE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_ENDSCENE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LIGHTENABLE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LOAD:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETLIGHT:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETMATERIAL:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERTARGET:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTEXTURE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTEXTURESTAGESTATE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTRANSFORM:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETVIEWPORT:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_LOCK:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_UNLOCK:
        {
            result = ExecutionClass::EVERY_TIME;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP:
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

void* Stereo::Replay::EngineDLL::D3D7CallExecutor::find_right_twin(void* in_left_object_ptr) const
{
    const auto twin_iterator = m_right_twin_map.find(in_left_object_ptr);

    return (twin_iterator != m_right_twin_map.end() ) ? twin_iterator->second
                                                      : nullptr;
}

const uint8_t* Stereo::Replay::EngineDLL::D3D7CallExecutor::get_chunk_ptr(const size_t& in_n_argument,
                                                                          size_t*       out_n_bytes_ptr) const
{
    size_t n_bytes = 0;

    const uint8_t* result_ptr = m_context.get_chunk_data_ptr(m_context.get_in_argument_ptr(in_n_argument),
                                                            &n_bytes);

    if (out_n_bytes_ptr != nullptr)
    {
        *out_n_bytes_ptr = n_bytes;
    }

    return result_ptr;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::get_left_object(const size_t& in_n_argument,
                                                                  void**        out_object_ptr,
                                                                  std::string*  out_error_ptr) const
{
    const APIInterceptor::APIFunctionArgument* argument_ptr = m_context.get_in_argument_ptr(in_n_argument);
    uint64_t                                   live_value   = 0;
    bool                                       result       = false;

    *out_object_ptr = nullptr;

    if (argument_ptr       == nullptr ||
        argument_ptr->type != APIInterceptor::ARGTYPE_VOID_PTR)
    {
        *out_error_ptr = "An argument of a D3D7 call is not an object.";
    }
    else
    if (argument_ptr->get_ptr_as_u64() == 0)
    {
        result = true;
    }
    else
    if (m_context.get_remapper_ptr()->get(ObjectKind::D3D7_OBJECT,
                                          argument_ptr->get_ptr_as_u64(),
                                         &live_value) )
    {
        *out_object_ptr = reinterpret_cast<void*>(live_value);
        result          = true;
    }
    else
    {
        *out_error_ptr = "A D3D7 object which the replay has not created is used.";
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::get_live_object(const size_t& in_n_argument,
                                                                  void**        out_object_ptr,
                                                                  std::string*  out_error_ptr) const
{
    const bool result = get_left_object(in_n_argument,
                                        out_object_ptr,
                                        out_error_ptr);

    /* The copy of the eye, for an object that each eye has a copy of. */
    if (result                                                 &&
        m_eye             == Analysis::RecipeLoader::Eye::RIGHT &&
        *out_object_ptr   != nullptr)
    {
        void* const twin_ptr = find_right_twin(*out_object_ptr);

        if (twin_ptr != nullptr)
        {
            *out_object_ptr = twin_ptr;
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::get_u32(const size_t& in_n_argument,
                                                          uint32_t*     out_value_ptr) const
{
    const APIInterceptor::APIFunctionArgument* argument_ptr = m_context.get_in_argument_ptr(in_n_argument);

    return (argument_ptr != nullptr &&
            read_number(*argument_ptr,
                         out_value_ptr) );
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::involves_per_eye_object() const
{
    bool result = false;

    if (m_right_twin_map.empty() == false)
    {
        for (size_t n_argument = 0;
                    n_argument < m_context.get_call().arg_in_vec.size() && result == false;
                  ++n_argument)
        {
            const auto& argument = m_context.get_call().arg_in_vec.at(n_argument);
            uint64_t    live_value = 0;

            if (argument.type                 == APIInterceptor::ARGTYPE_VOID_PTR &&
                argument.get_ptr_as_u64()     != 0                                &&
                m_context.get_remapper_ptr()->get(ObjectKind::D3D7_OBJECT,
                                                  argument.get_ptr_as_u64(),
                                                 &live_value) )
            {
                result = (find_right_twin(reinterpret_cast<void*>(live_value) ) != nullptr);
            }
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::is_query_method(const char* in_method_name_ptr)
{
    static const char* const prefix_ptr_array[] =
    {
        "Enum",
        "Get",
        "Is",
        "Test",
        "Validate"
    };

    bool result = false;

    for (const char* prefix_ptr : prefix_ptr_array)
    {
        if (::strncmp(in_method_name_ptr,
                      prefix_ptr,
                      ::strlen(prefix_ptr) ) == 0)
        {
            result = true;

            break;
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::light_enable(std::string* out_error_ptr)
{
    constexpr size_t ENABLE_ARGUMENT = 2;
    constexpr size_t INDEX_ARGUMENT  = 1;

    void*    device_ptr = nullptr;
    uint32_t enable     = 0;
    uint32_t index      = 0;
    bool     result     = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_u32(ENABLE_ARGUMENT,
               &enable)                == false ||
        get_u32(INDEX_ARGUMENT,
               &index)                 == false)
    {
        goto end;
    }

    result = true;

    call_method<HRESULT>(device_ptr,
                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_LIGHTENABLE,
                         static_cast<DWORD>(index),
                         static_cast<BOOL>(enable) );

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::load(std::string* out_error_ptr)
{
    constexpr size_t DESTINATION_ARGUMENT       = 1;
    constexpr size_t DESTINATION_POINT_ARGUMENT = 2;
    constexpr size_t FLAGS_ARGUMENT             = 5;
    constexpr size_t SOURCE_ARGUMENT            = 3;
    constexpr size_t SOURCE_RECT_ARGUMENT       = 4;

    void*    destination_ptr = nullptr;
    void*    device_ptr      = nullptr;
    uint32_t flags           = 0;
    void*    source_ptr      = nullptr;
    bool     result          = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_live_object(DESTINATION_ARGUMENT,
                       &destination_ptr,
                        out_error_ptr) == false ||
        get_live_object(SOURCE_ARGUMENT,
                       &source_ptr,
                        out_error_ptr) == false ||
        get_u32(FLAGS_ARGUMENT,
               &flags)                 == false)
    {
        goto end;
    }

    result = (call_method<HRESULT>(device_ptr,
                                   Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_LOAD,
                                   destination_ptr,
                                   reinterpret_cast<const POINT*>(get_chunk_ptr(DESTINATION_POINT_ARGUMENT) ),
                                   source_ptr,
                                   reinterpret_cast<const RECT*>(get_chunk_ptr(SOURCE_RECT_ARGUMENT) ),
                                   static_cast<DWORD>(flags) ) >= 0);

    if (result == false)
    {
        *out_error_ptr = "Load failed in the replay.";
    }

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::lock(std::string* out_error_ptr)
{
    constexpr size_t DESCRIPTION_ARGUMENT = 2;
    constexpr size_t FLAGS_ARGUMENT       = 3;
    constexpr size_t RECT_ARGUMENT        = 1;

    LockedSurface  locked;
    size_t         n_rect_bytes = 0;
    const uint8_t* rect_ptr     = get_chunk_ptr(RECT_ARGUMENT,
                                               &n_rect_bytes);
    uint32_t       flags        = 0;
    void*          surface_ptr  = nullptr;
    bool           result       = false;

    if (get_live_object(0,
                       &surface_ptr,
                        out_error_ptr) == false ||
        get_u32(FLAGS_ARGUMENT,
               &flags)                 == false)
    {
        goto end;
    }

    locked.has_rect = (rect_ptr != nullptr && n_rect_bytes == sizeof(RECT) );

    if (locked.has_rect)
    {
        ::memcpy(&locked.rect,
                 rect_ptr,
                 sizeof(RECT) );
    }

    /* The pitch that the dump's pixels have is told by the description that the lock wrote. */
    {
        const APIInterceptor::APIFunctionArgument*        out_argument_ptr = m_context.get_out_argument_ptr(DESCRIPTION_ARGUMENT);
        size_t                                            n_bytes          = 0;
        const uint8_t*                                    data_ptr         = m_context.get_chunk_data_ptr(out_argument_ptr,
                                                                                                          &n_bytes);
        Interception::Core::D3D7Utils::SurfaceDescription dumped_description;

        if (data_ptr != nullptr &&
            Interception::Core::D3D7Utils::parse_surface_description(data_ptr,
                                                                     n_bytes,
                                                                    &dumped_description) )
        {
            locked.dumped_pitch = static_cast<uint32_t>(dumped_description.pitch);
        }
    }

    locked.description.dwSize = sizeof(locked.description);

    if (call_method<HRESULT>(surface_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_LOCK,
                             (locked.has_rect) ? &locked.rect
                                               : static_cast<RECT*>(nullptr),
                             &locked.description,
                             static_cast<DWORD>(flags & ~DDLOCK_EVENT_FLAG),
                             static_cast<HANDLE>(nullptr) ) < 0)
    {
        *out_error_ptr = "Lock failed in the replay.";

        goto end;
    }

    m_locked_surface_map[surface_ptr] = locked;

    result = true;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::lock_frame_image(const DWORD&                               in_flags,
                                                                     Interception::Hooks::D3D7::DDSURFACEDESC2* out_description_ptr)
{
    bool result = false;

    *out_description_ptr = {};

    out_description_ptr->dwSize = sizeof(*out_description_ptr);

    /* The picture of a frame is what the flip shows: the back buffer, which is not what is rendered to when the frame ends in a texture. */
    void* const image_ptr = (m_back_buffer_ptr != nullptr) ? m_back_buffer_ptr
                                                           : m_render_target_ptr;

    if (image_ptr != nullptr &&
        m_locked_image_ptr == nullptr)
    {
        if (call_method<HRESULT>(image_ptr,
                                 Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_LOCK,
                                 static_cast<RECT*>(nullptr),
                                 out_description_ptr,
                                 static_cast<DWORD>(in_flags | DDLOCK_WAIT_FLAG),
                                 static_cast<HANDLE>(nullptr) ) >= 0)
        {
            m_locked_image_ptr = image_ptr;
            result                     = true;
        }
    }

    return result;
}

void Stereo::Replay::EngineDLL::D3D7CallExecutor::on_frame_presented()
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::D3D7CallExecutor::present_to_window()
{
    RECT  client_rect = {};
    POINT origin      = {0, 0};

    if (m_back_buffer_ptr != nullptr &&
        m_primary_ptr     != nullptr &&
        m_window          != nullptr &&
        ::GetClientRect(m_window,
                       &client_rect) )
    {
        ::ClientToScreen(m_window,
                        &origin);

        RECT destination_rect = {origin.x,
                                 origin.y,
                                 origin.x + client_rect.right,
                                 origin.y + client_rect.bottom};

        if (destination_rect.right  > destination_rect.left &&
            destination_rect.bottom > destination_rect.top)
        {
            call_method<HRESULT>(m_primary_ptr,
                                 Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_BLT,
                                 &destination_rect,
                                 m_back_buffer_ptr,
                                 static_cast<RECT*>(nullptr),
                                 static_cast<DWORD>(DDLOCK_WAIT_FLAG),
                                 static_cast<uint8_t*>(nullptr) );
        }
    }
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::query_interface(std::string* out_error_ptr)
{
    constexpr size_t IID_ARGUMENT    = 1;
    constexpr size_t OBJECT_ARGUMENT = 2;

    size_t         n_iid_bytes = 0;
    const uint8_t* iid_ptr     = get_chunk_ptr(IID_ARGUMENT,
                                              &n_iid_bytes);
    void*          object_ptr  = nullptr;
    void*          self_ptr    = nullptr;
    bool           result      = false;

    const APIInterceptor::APIFunctionArgument* out_argument_ptr = m_context.get_out_argument_ptr(OBJECT_ARGUMENT);

    if (get_live_object(0,
                       &self_ptr,
                        out_error_ptr) == false)
    {
        goto end;
    }

    /* A query which the dump's application was refused (an interface the driver has not got) gave it nothing. */
    if (out_argument_ptr                       == nullptr ||
        out_argument_ptr->type                 != APIInterceptor::ARGTYPE_VOID_PTR ||
        out_argument_ptr->get_ptr_as_u64()     == 0)
    {
        result = true;

        goto end;
    }

    if (iid_ptr     == nullptr ||
        n_iid_bytes != IID_N_BYTES)
    {
        *out_error_ptr = "QueryInterface has no IID in the dump.";

        goto end;
    }

    if (call_method<HRESULT, const GUID&, void**>(self_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAW7_QUERYINTERFACE,
                             *reinterpret_cast<const GUID*>(iid_ptr),
                             &object_ptr) < 0)
    {
        *out_error_ptr = "QueryInterface failed in the replay.";

        goto end;
    }

    register_object(OBJECT_ARGUMENT,
                    object_ptr);

    result = true;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::read_back_buffer(std::vector<uint8_t>* out_pixel_u8_vec_ptr,
                                                                   FrameImage*           out_image_ptr)
{
    Interception::Hooks::D3D7::DDSURFACEDESC2 description = {};
    uint32_t                                  format      = 0;
    bool                                      result      = false;

    if (lock_frame_image(DDLOCK_READONLY_FLAG,
                          &description) == false)
    {
        goto end;
    }

    /* The format of the pixels, as the converter names them. */
    {
        constexpr uint32_t BITS_PER_BYTE = 8;

        const auto& pixel_format = description.ddpfPixelFormat;

        if (pixel_format.dwRGBBitCount == 32 * 1)
        {
            format = (pixel_format.dwRGBAlphaBitMask != 0) ? Interception::Hooks::D3D8::D3DFMT_A8R8G8B8
                                                            : Interception::Hooks::D3D8::D3DFMT_X8R8G8B8;
        }
        else
        if (pixel_format.dwRGBBitCount == 16)
        {
            format = (pixel_format.dwGBitMask == 0x07E0) ? Interception::Hooks::D3D8::D3DFMT_R5G6B5
                                                          : Interception::Hooks::D3D8::D3DFMT_X1R5G5B5;
        }

        (void) BITS_PER_BYTE;
    }

    if (PixelConverter::get_bytes_per_pixel(format) == 0)
    {
        goto end;
    }

    if (out_pixel_u8_vec_ptr != nullptr)
    {
        const uint32_t n_row_bytes = description.dwWidth * PixelConverter::get_bytes_per_pixel(format);

        out_pixel_u8_vec_ptr->resize(static_cast<size_t>(n_row_bytes) * description.dwHeight);

        for (uint32_t n_row = 0;
                      n_row < description.dwHeight;
                    ++n_row)
        {
            ::memcpy(out_pixel_u8_vec_ptr->data()                          + static_cast<size_t>(n_row) * n_row_bytes,
                     static_cast<const uint8_t*>(description.lpSurface)    + static_cast<size_t>(n_row) * description.lPitch,
                     n_row_bytes);
        }
    }

    if (out_image_ptr != nullptr)
    {
        if (PixelConverter::convert_to_bgra8(format,
                                             description.dwWidth,
                                             description.dwHeight,
                                             static_cast<uint32_t>(description.lPitch),
                                             static_cast<const uint8_t*>(description.lpSurface),
                                            &m_capture_u8_vec) == false)
        {
            goto end;
        }

        out_image_ptr->height    = description.dwHeight;
        out_image_ptr->pixel_ptr = m_capture_u8_vec.data();
        out_image_ptr->width     = description.dwWidth;
    }

    result = true;

end:
    unlock_frame_image();

    return result;
}

void Stereo::Replay::EngineDLL::D3D7CallExecutor::register_object(const size_t& in_n_argument,
                                                                  void*         in_live_object_ptr)
{
    const APIInterceptor::APIFunctionArgument* out_argument_ptr = m_context.get_out_argument_ptr(in_n_argument);

    if (out_argument_ptr       != nullptr &&
        out_argument_ptr->type == APIInterceptor::ARGTYPE_VOID_PTR &&
        out_argument_ptr->get_ptr_as_u64() != 0)
    {
        uint64_t left_value = 0;

        if (m_is_building_twin)
        {
            /* The object is the copy of the right eye of the one that the call made for the left eye. */
            if (m_context.get_remapper_ptr()->get(ObjectKind::D3D7_OBJECT,
                                                  out_argument_ptr->get_ptr_as_u64(),
                                                 &left_value) )
            {
                m_right_twin_map[reinterpret_cast<void*>(left_value)] = in_live_object_ptr;
            }

            m_live_objects.add(in_live_object_ptr);

            m_reference_count_map[in_live_object_ptr]++;
        }
        else
        {
            m_context.get_remapper_ptr()->add(ObjectKind::D3D7_OBJECT,
                                              out_argument_ptr->get_ptr_as_u64(),
                                              reinterpret_cast<uint64_t>(in_live_object_ptr) );

            /* A call that hands out an object hands out a reference to it. */
            m_live_objects.add(in_live_object_ptr);

            m_dumped_value_map[in_live_object_ptr].push_back(out_argument_ptr->get_ptr_as_u64() );
            m_reference_count_map[in_live_object_ptr]++;

            /* Asked when the object is made, which is when the copy is: it is made by the call being made again, for the right eye. */
            if (m_per_eye_predicate                                           &&
                m_per_eye_predicate(out_argument_ptr->get_ptr_as_u64() ) )
            {
                m_needs_twin = true;
            }
        }
    }
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::release(std::string* out_error_ptr)
{
    void* object_ptr = nullptr;
    bool  result     = get_left_object(0,
                                      &object_ptr,
                                       out_error_ptr);

    /* An object which the replay has not got is not the replay's to release. */

    if (result == false)
    {
        out_error_ptr->clear();

        result = true;
    }
    else
    if (object_ptr != nullptr)
    {
        const ULONG n_refs = call_method<ULONG>(object_ptr,
                                                Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE);

        if (m_reference_count_map[object_ptr] > 0)
        {
            m_reference_count_map[object_ptr]--;
        }

        /* The copy of the other eye is let go of with it. */
        void* const twin_ptr = find_right_twin(object_ptr);

        if (twin_ptr != nullptr)
        {
            const ULONG n_twin_refs = call_method<ULONG>(twin_ptr,
                                                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE);

            if (m_reference_count_map[twin_ptr] > 0)
            {
                m_reference_count_map[twin_ptr]--;
            }

            if (n_twin_refs == 0)
            {
                m_live_objects.remove(twin_ptr);
                m_locked_surface_map.erase(twin_ptr);
                m_reference_count_map.erase(twin_ptr);
            }
        }

        if (n_refs == 0)
        {
            /* The object is gone, whichever pointers of the dump stood for it. */
            for (const uint64_t dumped_value : m_dumped_value_map[object_ptr])
            {
                m_context.get_remapper_ptr()->remove(ObjectKind::D3D7_OBJECT,
                                                     dumped_value);
            }

            m_dumped_value_map.erase(object_ptr);
            m_right_twin_map.erase(object_ptr);
            m_live_objects.remove(object_ptr);
            m_locked_surface_map.erase(object_ptr);
            m_reference_count_map.erase(object_ptr);

            if (object_ptr == m_primary_ptr)
            {
                m_primary_ptr = nullptr;
            }

            if (object_ptr == m_device_ptr)
            {
                m_device_ptr = nullptr;
            }

            if (object_ptr == m_render_target_ptr)
            {
                m_render_target_ptr = nullptr;
            }

            for (auto stage_iterator  = m_stage_texture_map.begin();
                      stage_iterator != m_stage_texture_map.end();)
            {
                if (stage_iterator->second == object_ptr)
                {
                    stage_iterator = m_stage_texture_map.erase(stage_iterator);
                }
                else
                {
                    ++stage_iterator;
                }
            }
        }
    }

    return result;
}

void Stereo::Replay::EngineDLL::D3D7CallExecutor::reset()
{
    unlock_frame_image();

    /* The device is created after the objects it renders to, so it is released before them. */
    for (void* const object_ptr : m_live_objects.get_newest_first_vec() )
    {
        /* Exactly the references the replay holds: an object may be kept alive by others (an attached surface), and be gone with them. */
        const uint32_t n_references = m_reference_count_map[object_ptr];
        ULONG          n_left       = 1;

        for (uint32_t n_release = 0;
                      n_release < n_references && n_left > 0;
                    ++n_release)
        {
            n_left = call_method<ULONG>(object_ptr,
                                        Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE);
        }
    }

    m_dumped_value_map.clear();
    m_right_twin_map.clear();
    m_live_objects.clear();
    m_locked_surface_map.clear();
    m_reference_count_map.clear();

    m_back_buffer_ptr   = nullptr;
    m_clipper_ptr       = nullptr;
    m_direct_draw_ptr   = nullptr;
    m_display_height    = 0;
    m_display_width     = 0;
    m_primary_ptr       = nullptr;
    m_render_target_ptr = nullptr;
    m_window            = nullptr;

    m_device_ptr = nullptr;
    m_stage_texture_map.clear();

    m_device_state       = DeviceState();
    m_saved_device_state = DeviceState();
}

void Stereo::Replay::EngineDLL::D3D7CallExecutor::restore_state()
{
    const bool is_right = (m_eye == Analysis::RecipeLoader::Eye::RIGHT);

    /* The copy of the eye, for an object which each eye has a copy of. */
    const auto get_eye_object_func = [&](void* in_left_object_ptr)
    {
        void* const twin_ptr = (is_right && in_left_object_ptr != nullptr) ? find_right_twin(in_left_object_ptr)
                                                                           : nullptr;

        return (twin_ptr != nullptr) ? twin_ptr
                                     : in_left_object_ptr;
    };

    if (m_device_ptr == nullptr)
    {
        goto end;
    }

    for (const auto& render_state : m_saved_device_state.render_state_map)
    {
        const auto current_iterator = m_device_state.render_state_map.find(render_state.first);

        if (current_iterator == m_device_state.render_state_map.end() ||
            current_iterator->second != render_state.second)
        {
            call_method<HRESULT>(m_device_ptr,
                                 Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETRENDERSTATE,
                                 static_cast<Interception::Hooks::D3D7::D3DRENDERSTATETYPE>(render_state.first),
                                 static_cast<DWORD>(render_state.second) );
        }
    }

    for (const auto& stage_state : m_saved_device_state.texture_stage_state_map)
    {
        const auto current_iterator = m_device_state.texture_stage_state_map.find(stage_state.first);

        if (current_iterator == m_device_state.texture_stage_state_map.end() ||
            current_iterator->second != stage_state.second)
        {
            call_method<HRESULT>(m_device_ptr,
                                 Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTEXTURESTAGESTATE,
                                 static_cast<DWORD>(stage_state.first.first),
                                 static_cast<Interception::Hooks::D3D7::D3DTEXTURESTAGESTATETYPE>(stage_state.first.second),
                                 static_cast<DWORD>(stage_state.second) );
        }
    }

    for (const auto& transform : m_saved_device_state.transform_map)
    {
        const auto current_iterator = m_device_state.transform_map.find(transform.first);

        if (current_iterator == m_device_state.transform_map.end() ||
            current_iterator->second != transform.second)
        {
            call_method<HRESULT>(m_device_ptr,
                                 Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTRANSFORM,
                                 static_cast<Interception::Hooks::D3D7::D3DTRANSFORMSTATETYPE>(transform.first),
                                 reinterpret_cast<const Interception::Hooks::D3D7::D3DMATRIX*>(transform.second.data() ) );
        }
    }

    if (m_saved_device_state.viewport_u8_vec.empty() == false &&
        m_saved_device_state.viewport_u8_vec       != m_device_state.viewport_u8_vec)
    {
        call_method<HRESULT>(m_device_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETVIEWPORT,
                             reinterpret_cast<const Interception::Hooks::D3D7::D3DVIEWPORT7*>(m_saved_device_state.viewport_u8_vec.data() ) );
    }

    /* What is rendered to, and the textures: the frame may set them first thing, or may rely on what the frame before left. */
    if (m_saved_device_state.render_target_ptr != nullptr &&
        m_saved_device_state.render_target_ptr != m_render_target_ptr)
    {
        call_method<HRESULT>(m_device_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETRENDERTARGET,
                             get_eye_object_func(m_saved_device_state.render_target_ptr),
                             static_cast<DWORD>(0) );
    }

    for (const auto& stage_texture : m_saved_device_state.stage_texture_map)
    {
        const auto current_iterator = m_stage_texture_map.find(stage_texture.first);

        if (current_iterator == m_stage_texture_map.end() ||
            current_iterator->second != stage_texture.second)
        {
            call_method<HRESULT>(m_device_ptr,
                                 Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTEXTURE,
                                 static_cast<DWORD>(stage_texture.first),
                                 get_eye_object_func(stage_texture.second) );
        }
    }

    m_device_state                  = m_saved_device_state;
    m_render_target_ptr             = m_saved_device_state.render_target_ptr;
    m_stage_texture_map             = m_saved_device_state.stage_texture_map;
    m_device_state.render_target_ptr = nullptr;
    m_device_state.stage_texture_map.clear();

end:
    return;
}

void Stereo::Replay::EngineDLL::D3D7CallExecutor::save_state()
{
    m_saved_device_state                   = m_device_state;
    m_saved_device_state.render_target_ptr = m_render_target_ptr;
    m_saved_device_state.stage_texture_map = m_stage_texture_map;
}

void Stereo::Replay::EngineDLL::D3D7CallExecutor::set_eye(const Analysis::RecipeLoader::Eye& in_eye)
{
    /* The device keeps what was set on it by the eye before: the target and the textures must be those of this eye, or that eye
     * would draw into (and read) the other's copy.
     */
    const bool is_changing = (m_eye != in_eye);

    m_eye = in_eye;

    if (is_changing           &&
        m_device_ptr != nullptr)
    {
        if (m_render_target_ptr != nullptr)
        {
            void* const twin_ptr = find_right_twin(m_render_target_ptr);

            if (twin_ptr != nullptr)
            {
                call_method<HRESULT>(m_device_ptr,
                                     Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETRENDERTARGET,
                                     (m_eye == Analysis::RecipeLoader::Eye::RIGHT) ? twin_ptr
                                                                                   : m_render_target_ptr,
                                     static_cast<DWORD>(0) );
            }
        }

        for (const auto& stage_iterator : m_stage_texture_map)
        {
            void* const twin_ptr = (stage_iterator.second != nullptr) ? find_right_twin(stage_iterator.second)
                                                                      : nullptr;

            if (twin_ptr != nullptr)
            {
                call_method<HRESULT>(m_device_ptr,
                                     Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTEXTURE,
                                     static_cast<DWORD>(stage_iterator.first),
                                     (m_eye == Analysis::RecipeLoader::Eye::RIGHT) ? twin_ptr
                                                                                   : stage_iterator.second);
            }
        }
    }
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::set_cooperative_level(std::string* out_error_ptr)
{
    constexpr size_t FLAGS_ARGUMENT  = 2;
    constexpr size_t WINDOW_ARGUMENT = 1;

    void*    direct_draw_ptr = nullptr;
    uint32_t flags           = 0;
    uint64_t live_window     = 0;
    bool     result          = false;

    if (get_live_object(0,
                       &direct_draw_ptr,
                        out_error_ptr) == false ||
        get_u32(FLAGS_ARGUMENT,
               &flags)                 == false)
    {
        goto end;
    }

    /* The replay is in a window of its own, however the application was. */
    if (m_context.get_remapper_ptr()->get(ObjectKind::HWND,
                                          m_context.get_in_argument_ptr(WINDOW_ARGUMENT)->get_ptr_as_u64(),
                                         &live_window) )
    {
        m_window = reinterpret_cast<HWND>(live_window);
    }

    result = true;

    call_method<HRESULT>(direct_draw_ptr,
                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAW7_SETCOOPERATIVELEVEL,
                         m_window,
                         static_cast<DWORD>(DDSCL_NORMAL_FLAG | (flags & (DDSCL_FPUPRESERVE_FLAG | DDSCL_FPUSETUP_FLAG | DDSCL_MULTITHREADED_FLAG))) );

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::set_display_mode(std::string* out_error_ptr)
{
    constexpr size_t HEIGHT_ARGUMENT = 2;
    constexpr size_t WIDTH_ARGUMENT  = 1;

    (void) out_error_ptr;

    /* The screen is left as it is; the size of the display of the dump is the size of the back buffer. */
    get_u32(HEIGHT_ARGUMENT,
           &m_display_height);
    get_u32(WIDTH_ARGUMENT,
           &m_display_width);

    return true;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::set_light(std::string* out_error_ptr)
{
    constexpr size_t INDEX_ARGUMENT = 1;
    constexpr size_t LIGHT_ARGUMENT = 2;

    void*    device_ptr = nullptr;
    uint32_t index      = 0;
    bool     result     = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_u32(INDEX_ARGUMENT,
               &index)                 == false)
    {
        goto end;
    }

    result = true;

    call_method<HRESULT>(device_ptr,
                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETLIGHT,
                         static_cast<DWORD>(index),
                         reinterpret_cast<const Interception::Hooks::D3D7::D3DLIGHT7*>(get_chunk_ptr(LIGHT_ARGUMENT) ) );

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::set_material(std::string* out_error_ptr)
{
    constexpr size_t MATERIAL_ARGUMENT = 1;

    void* device_ptr = nullptr;
    bool  result     = get_live_object(0,
                                      &device_ptr,
                                       out_error_ptr);

    if (result)
    {
        call_method<HRESULT>(device_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETMATERIAL,
                             reinterpret_cast<const Interception::Hooks::D3D7::D3DMATERIAL7*>(get_chunk_ptr(MATERIAL_ARGUMENT) ) );
    }

    return result;
}

void Stereo::Replay::EngineDLL::D3D7CallExecutor::set_per_eye_predicate(const PerEyePredicate& in_predicate)
{
    m_per_eye_predicate = in_predicate;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::set_render_state(std::string* out_error_ptr)
{
    constexpr size_t STATE_ARGUMENT = 1;
    constexpr size_t VALUE_ARGUMENT = 2;

    void*    device_ptr = nullptr;
    uint32_t state      = 0;
    uint32_t value      = 0;
    bool     result     = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_u32(STATE_ARGUMENT,
               &state)                 == false ||
        get_u32(VALUE_ARGUMENT,
               &value)                 == false)
    {
        goto end;
    }

    result = true;

    call_method<HRESULT>(device_ptr,
                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETRENDERSTATE,
                         static_cast<Interception::Hooks::D3D7::D3DRENDERSTATETYPE>(state),
                         static_cast<DWORD>(value) );

    m_device_ptr                         = device_ptr;
    m_device_state.render_state_map[state] = value;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::set_render_target(std::string* out_error_ptr)
{
    constexpr size_t FLAGS_ARGUMENT   = 2;
    constexpr size_t SURFACE_ARGUMENT = 1;

    void*    device_ptr       = nullptr;
    uint32_t flags            = 0;
    void*    left_surface_ptr = nullptr;
    void*    surface_ptr      = nullptr;
    bool     result           = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_live_object(SURFACE_ARGUMENT,
                       &surface_ptr,
                        out_error_ptr) == false ||
        get_left_object(SURFACE_ARGUMENT,
                       &left_surface_ptr,
                        out_error_ptr) == false ||
        get_u32(FLAGS_ARGUMENT,
               &flags)                 == false)
    {
        goto end;
    }

    result = (call_method<HRESULT>(device_ptr,
                                   Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETRENDERTARGET,
                                   surface_ptr,
                                   static_cast<DWORD>(flags) ) >= 0);

    if (result)
    {
        m_device_ptr        = device_ptr;
        m_render_target_ptr = left_surface_ptr;
    }
    else
    {
        *out_error_ptr = "SetRenderTarget failed in the replay.";
    }

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::set_texture(std::string* out_error_ptr)
{
    constexpr size_t STAGE_ARGUMENT   = 1;
    constexpr size_t TEXTURE_ARGUMENT = 2;

    void*    device_ptr       = nullptr;
    uint32_t stage            = 0;
    void*    left_texture_ptr = nullptr;
    void*    texture_ptr      = nullptr;
    bool     result           = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_live_object(TEXTURE_ARGUMENT,
                       &texture_ptr,
                        out_error_ptr) == false ||
        get_left_object(TEXTURE_ARGUMENT,
                       &left_texture_ptr,
                        out_error_ptr) == false ||
        get_u32(STAGE_ARGUMENT,
               &stage)                 == false)
    {
        goto end;
    }

    result = true;

    call_method<HRESULT>(device_ptr,
                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTEXTURE,
                         static_cast<DWORD>(stage),
                         texture_ptr);

    m_device_ptr               = device_ptr;
    m_stage_texture_map[stage] = left_texture_ptr;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::set_texture_stage_state(std::string* out_error_ptr)
{
    constexpr size_t STAGE_ARGUMENT = 1;
    constexpr size_t STATE_ARGUMENT = 2;
    constexpr size_t VALUE_ARGUMENT = 3;

    void*    device_ptr = nullptr;
    uint32_t stage      = 0;
    uint32_t state      = 0;
    uint32_t value      = 0;
    bool     result     = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_u32(STAGE_ARGUMENT,
               &stage)                 == false ||
        get_u32(STATE_ARGUMENT,
               &state)                 == false ||
        get_u32(VALUE_ARGUMENT,
               &value)                 == false)
    {
        goto end;
    }

    result = true;

    call_method<HRESULT>(device_ptr,
                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTEXTURESTAGESTATE,
                         static_cast<DWORD>(stage),
                         static_cast<Interception::Hooks::D3D7::D3DTEXTURESTAGESTATETYPE>(state),
                         static_cast<DWORD>(value) );

    m_device_ptr                                                      = device_ptr;
    m_device_state.texture_stage_state_map[std::make_pair(stage,
                                                          state)] = value;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::set_transform(std::string* out_error_ptr)
{
    constexpr size_t MATRIX_ARGUMENT = 2;
    constexpr size_t STATE_ARGUMENT  = 1;

    void*    device_ptr = nullptr;
    uint32_t state      = 0;
    bool     result     = false;

    if (get_live_object(0,
                       &device_ptr,
                        out_error_ptr) == false ||
        get_u32(STATE_ARGUMENT,
               &state)                 == false)
    {
        goto end;
    }

    result = true;

    {
        size_t         n_matrix_bytes = 0;
        const uint8_t* matrix_ptr     = get_chunk_ptr(MATRIX_ARGUMENT,
                                                     &n_matrix_bytes);

        call_method<HRESULT>(device_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTRANSFORM,
                             static_cast<Interception::Hooks::D3D7::D3DTRANSFORMSTATETYPE>(state),
                             reinterpret_cast<const Interception::Hooks::D3D7::D3DMATRIX*>(matrix_ptr) );

        m_device_ptr = device_ptr;

        if (matrix_ptr != nullptr)
        {
            m_device_state.transform_map[state].assign(matrix_ptr,
                                                       matrix_ptr + n_matrix_bytes);
        }
    }

end:
    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::set_viewport(std::string* out_error_ptr)
{
    constexpr size_t VIEWPORT_ARGUMENT = 1;

    void* device_ptr = nullptr;
    bool  result     = get_live_object(0,
                                      &device_ptr,
                                       out_error_ptr);

    if (result)
    {
        size_t         n_viewport_bytes = 0;
        const uint8_t* viewport_ptr     = get_chunk_ptr(VIEWPORT_ARGUMENT,
                                                        &n_viewport_bytes);

        call_method<HRESULT>(device_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETVIEWPORT,
                             reinterpret_cast<const Interception::Hooks::D3D7::D3DVIEWPORT7*>(viewport_ptr) );

        m_device_ptr = device_ptr;

        if (viewport_ptr != nullptr)
        {
            m_device_state.viewport_u8_vec.assign(viewport_ptr,
                                                  viewport_ptr + n_viewport_bytes);
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::unlock(std::string* out_error_ptr)
{
    constexpr size_t RECT_ARGUMENT = 1;

    size_t         n_data_bytes = 0;
    const uint8_t* data_ptr     = get_chunk_ptr(m_context.get_n_head_arguments(),
                                               &n_data_bytes);
    void*          surface_ptr  = nullptr;
    bool           result       = false;

    (void) RECT_ARGUMENT;

    if (get_live_object(0,
                       &surface_ptr,
                        out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto locked_iterator = m_locked_surface_map.find(surface_ptr);

        /* What the application wrote goes into the pixels, row by row: the pitch of the replay is not that of the dump. */
        if (locked_iterator != m_locked_surface_map.end() )
        {
            const LockedSurface& locked        = locked_iterator->second;
            const auto&          pixel_format  = locked.description.ddpfPixelFormat;
            const uint32_t       height        = (locked.has_rect) ? static_cast<uint32_t>(locked.rect.bottom - locked.rect.top)
                                                                   : locked.description.dwHeight;
            const uint32_t       width         = (locked.has_rect) ? static_cast<uint32_t>(locked.rect.right - locked.rect.left)
                                                                   : locked.description.dwWidth;
            const uint32_t       n_row_bytes   = width * (pixel_format.dwRGBBitCount / 8);
            const uint32_t       live_pitch    = static_cast<uint32_t>(locked.description.lPitch);
            if (data_ptr != nullptr)
            {
                if (n_row_bytes == 0 ||
                    locked.dumped_pitch == 0)
                {
                    /* A format with no rows of pixels (a compressed one): the data are the bytes of the surface. */
                    ::memcpy(locked.description.lpSurface,
                             data_ptr,
                             n_data_bytes);
                }
                else
                {
                    for (uint32_t n_row = 0;
                                  n_row < height;
                                ++n_row)
                    {
                        const size_t source_offset = static_cast<size_t>(n_row) * locked.dumped_pitch;

                        if (source_offset + n_row_bytes <= n_data_bytes)
                        {
                            ::memcpy(static_cast<uint8_t*>(locked.description.lpSurface) + static_cast<size_t>(n_row) * live_pitch,
                                     data_ptr + source_offset,
                                     n_row_bytes);
                        }
                    }
                }
            }

            m_locked_surface_map.erase(locked_iterator);
        }
    }

    result = true;

    call_method<HRESULT>(surface_ptr,
                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_UNLOCK,
                         static_cast<RECT*>(nullptr) );

end:
    return result;
}

void Stereo::Replay::EngineDLL::D3D7CallExecutor::unlock_frame_image()
{
    if (m_locked_image_ptr != nullptr)
    {
        call_method<HRESULT>(m_locked_image_ptr,
                             Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_UNLOCK,
                             static_cast<RECT*>(nullptr) );

        m_locked_image_ptr = nullptr;
    }
}

bool Stereo::Replay::EngineDLL::D3D7CallExecutor::write_back_buffer(const std::vector<uint8_t>& in_pixel_u8_vec)
{
    Interception::Hooks::D3D7::DDSURFACEDESC2 description = {};
    bool                                      result      = false;

    if (lock_frame_image(DDLOCK_WRITEONLY_FLAG,
                          &description) == false)
    {
        goto end;
    }

    {
        const uint32_t n_row_bytes = description.dwWidth * (description.ddpfPixelFormat.dwRGBBitCount / 8);

        result = (n_row_bytes != 0 &&
                  in_pixel_u8_vec.size() == static_cast<size_t>(n_row_bytes) * description.dwHeight);

        for (uint32_t n_row = 0;
                      result && n_row < description.dwHeight;
                    ++n_row)
        {
            ::memcpy(static_cast<uint8_t*>(description.lpSurface) + static_cast<size_t>(n_row) * description.lPitch,
                     in_pixel_u8_vec.data()                       + static_cast<size_t>(n_row) * n_row_bytes,
                     n_row_bytes);
        }
    }

end:
    unlock_frame_image();

    return result;
}
