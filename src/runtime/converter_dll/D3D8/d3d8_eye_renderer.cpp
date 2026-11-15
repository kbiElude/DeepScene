/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_frontend.h"
#include "common/macros.h"
#include "common/vtable.h"
#include "live_call_analyzer.h"
#include "d3d8_eye_renderer.h"
#include "stereo/core/screen_space_vertex_shifter.h"

Stereo::ConverterDLL::D3D8EyeRenderer::D3D8EyeRenderer(Stereo::Core::RecipeStereoPlanner* in_core_ptr)
    :m_core_ptr      (in_core_ptr),
     m_device_ptr    (nullptr),
     m_draw_class    (Analysis::Recipe::DrawClass::AFFECTED),
     m_is_draw_handled(false),
     m_n_failed_twins(0)
{
    /* Stub */
}

void Stereo::ConverterDLL::D3D8EyeRenderer::apply_matrices(void*           in_device_ptr,
                                                           const uint32_t& in_n_changes) const
{
    constexpr size_t STATE_ARGUMENT_INDEX = 1;

    const auto set_transform_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_SETTRANSFORM>(in_device_ptr,
                                                                                                                                 Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_SETTRANSFORM);

    for (uint32_t n_change = 0;
                  n_change < in_n_changes;
                ++n_change)
    {
        const auto& change = m_change_vec.at(n_change);

        set_transform_func(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>    (in_device_ptr),
                           static_cast<Interception::Hooks::D3D8::D3DTRANSFORMSTATETYPE>(change.argument_vec.at      (STATE_ARGUMENT_INDEX).get_u32() ),
                           reinterpret_cast<const Interception::Hooks::D3D8::D3DMATRIX*>(change.matrix_fp32vec16.data() ));
    }
}

void* Stereo::ConverterDLL::D3D8EyeRenderer::get_back_buffer_twin_ptr(void* in_device_ptr)
{
    Interception::Hooks::D3D8::IDirect3DSurface8* back_buffer_ptr = nullptr;
    void*                                         result_ptr      = nullptr;

    const auto get_back_buffer_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_GETBACKBUFFER>(in_device_ptr,
                                                                                                                                    Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_GETBACKBUFFER);

    if (SUCCEEDED(get_back_buffer_func(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(in_device_ptr),
                                       0, /* BackBuffer */
                                       Interception::Hooks::D3D8::D3DBACKBUFFER_TYPE_MONO,
                                      &back_buffer_ptr) ))
    {
        result_ptr = get_twin_ptr(back_buffer_ptr);

        APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_RELEASE>(back_buffer_ptr,
                                                                                                 Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_RELEASE)(back_buffer_ptr);
    }

    return result_ptr;
}

uint64_t Stereo::ConverterDLL::D3D8EyeRenderer::get_n_failed_twins() const
{
    return m_n_failed_twins;
}

uint64_t Stereo::ConverterDLL::D3D8EyeRenderer::get_n_twins() const
{
    return m_twin_map.size();
}

void* Stereo::ConverterDLL::D3D8EyeRenderer::get_pointer_argument(const APIInterceptor::APIFunctionArgument& in_argument)
{
    return const_cast<void*>(in_argument.get_ptr() );
}

void* Stereo::ConverterDLL::D3D8EyeRenderer::get_render_target_ptr() const
{
    return m_mirror.render_target_ptr;
}

void* Stereo::ConverterDLL::D3D8EyeRenderer::get_twin_or_own_ptr(void* in_object_ptr) const
{
    void* const twin_ptr = get_twin_ptr(in_object_ptr);

    return (twin_ptr != nullptr) ? twin_ptr
                                 : in_object_ptr;
}

void* Stereo::ConverterDLL::D3D8EyeRenderer::get_twin_ptr(void* in_object_ptr) const
{
    const auto twin_iterator = m_twin_map.find(in_object_ptr);

    return (twin_iterator != m_twin_map.end() ) ? twin_iterator->second
                                                : nullptr;
}

void Stereo::ConverterDLL::D3D8EyeRenderer::draw_pre_transformed(const APIInterceptor::APIFunction&         in_function,
                                                                 const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                                 const float&                               in_picture_width)
{
    /* Where the draws that carry vertices keep their arguments. */
    constexpr size_t INDEXED_INDICES_ARGUMENT     = 5;
    constexpr size_t INDEXED_INDEX_FORMAT_ARGUMENT = 6;
    constexpr size_t INDEXED_MIN_INDEX_ARGUMENT   = 2;
    constexpr size_t INDEXED_N_INDICES_ARGUMENT   = 3;
    constexpr size_t INDEXED_N_PRIMITIVES_ARGUMENT = 4;
    constexpr size_t INDEXED_STRIDE_ARGUMENT      = 8;
    constexpr size_t INDEXED_VERTICES_ARGUMENT    = 7;
    constexpr size_t PLAIN_N_PRIMITIVES_ARGUMENT  = 2;
    constexpr size_t PLAIN_STRIDE_ARGUMENT        = 4;
    constexpr size_t PLAIN_VERTICES_ARGUMENT      = 3;
    constexpr size_t PRIMITIVE_TYPE_ARGUMENT      = 1;

    const bool is_indexed = (in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP);
    const bool has_vertices = (is_indexed ||
                               in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP);

    AI_ASSERT_ALWAYS(has_vertices,
                     "The application draws vertices that are positioned in pixels (a vertex format with D3DFVF_XYZRHW, as a HUD or text uses) from a vertex buffer, "
                     "and the converter moves them for the second eye of Direct3D 8 only when the application gives them in the call (DrawPrimitiveUP, DrawIndexedPrimitiveUP).");

    void* const    device_ptr        = get_pointer_argument(in_arguments_ptr[0]);
    const uint32_t primitive_type    = in_arguments_ptr[PRIMITIVE_TYPE_ARGUMENT].get_u32();
    const uint32_t n_primitives      = (is_indexed) ? in_arguments_ptr[INDEXED_N_PRIMITIVES_ARGUMENT].get_u32()
                                                    : in_arguments_ptr[PLAIN_N_PRIMITIVES_ARGUMENT].get_u32  ();
    const uint32_t vertex_n_bytes    = (is_indexed) ? in_arguments_ptr[INDEXED_STRIDE_ARGUMENT].get_u32()
                                                    : in_arguments_ptr[PLAIN_STRIDE_ARGUMENT].get_u32  ();
    const uint8_t* vertices_ptr      = static_cast<const uint8_t*>((is_indexed) ? in_arguments_ptr[INDEXED_VERTICES_ARGUMENT].get_ptr()
                                                                                : in_arguments_ptr[PLAIN_VERTICES_ARGUMENT].get_ptr  () );
    const uint16_t* indices_ptr      = nullptr;
    uint32_t        n_indices        = 0;
    uint32_t        n_vertices       = 0;

    /* Indices name vertices counted from the first of the array. */
    if (is_indexed)
    {
        const uint32_t index_format = in_arguments_ptr[INDEXED_INDEX_FORMAT_ARGUMENT].get_u32();

        n_vertices = in_arguments_ptr[INDEXED_MIN_INDEX_ARGUMENT].get_u32() + in_arguments_ptr[INDEXED_N_INDICES_ARGUMENT].get_u32();

        /* With 32-bit indices the whole array is considered. */
        if (index_format == Interception::Hooks::D3D8::D3DFMT_INDEX16)
        {
            indices_ptr = static_cast<const uint16_t*>(in_arguments_ptr[INDEXED_INDICES_ARGUMENT].get_ptr() );
            n_indices   = Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::get_vertex_count(static_cast<Interception::Hooks::D3D8::D3DPRIMITIVETYPE>(primitive_type),
                                                                                               n_primitives);
        }
    }
    else
    {
        n_vertices = Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::get_vertex_count(static_cast<Interception::Hooks::D3D8::D3DPRIMITIVETYPE>(primitive_type),
                                                                                          n_primitives);
    }

    const bool is_presented = (m_mirror.render_target_ptr == m_mirror.back_buffer_ptr);
    const bool is_movable   = (vertices_ptr   != nullptr           &&
                               vertex_n_bytes >= 4 * sizeof(float) &&
                               n_vertices     != 0);

    /* The application's vertices are not touched: the draw uses a copy. */
    const auto draw_eye_func = [&](const Analysis::Recipe::Eye& in_eye)
    {
        const bool has_copy = (is_movable &&
                               m_core_ptr->make_screen_vertices(in_eye,
                                                                is_presented,
                                                                in_picture_width,
                                                                vertices_ptr,
                                                                vertex_n_bytes,
                                                                n_vertices,
                                                                indices_ptr,
                                                                n_indices,
                                                                false, /* in_must_copy */
                                                               &m_vertex_vec) );

        draw_with_vertices(in_function,
                           in_arguments_ptr,
                           (has_copy) ? m_vertex_vec.data()
                                      : vertices_ptr);
    };

    const auto draw_right_eye_func = [&draw_eye_func]()
    {
        draw_eye_func(Analysis::Recipe::Eye::RIGHT);
    };

    draw_eye_func(Analysis::Recipe::Eye::LEFT);

    play_right_eye(device_ptr,
                   draw_right_eye_func);
}

void Stereo::ConverterDLL::D3D8EyeRenderer::draw_with_vertices(const APIInterceptor::APIFunction&         in_function,
                                                               const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                               const uint8_t*                             in_vertices_ptr) const
{
    void* const                                  device_ptr = get_pointer_argument(in_arguments_ptr[0]);
    Interception::Hooks::D3D8::IDirect3DDevice8* device     = static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr);
    void* const                                  vertex_ptr = const_cast<uint8_t*>(in_vertices_ptr);

    if (in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP)
    {
        APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP>(device_ptr,
                                                                                                               Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP)(device,
                                                                                                                                                                                                static_cast<Interception::Hooks::D3D8::D3DPRIMITIVETYPE>(in_arguments_ptr[1].get_u32() ),
                                                                                                                                                                                                in_arguments_ptr[2].get_u32                             (),
                                                                                                                                                                                                in_arguments_ptr[3].get_u32                             (),
                                                                                                                                                                                                in_arguments_ptr[4].get_u32                             (),
                                                                                                                                                                                                in_arguments_ptr[5].get_ptr                             (),
                                                                                                                                                                                                static_cast<Interception::Hooks::D3D8::D3DFORMAT>       (in_arguments_ptr[6].get_u32() ),
                                                                                                                                                                                                vertex_ptr,
                                                                                                                                                                                                in_arguments_ptr[8].get_u32                             () );
    }
    else
    {
        APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP>(device_ptr,
                                                                                                        Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP)(device,
                                                                                                                                                                                  static_cast<Interception::Hooks::D3D8::D3DPRIMITIVETYPE>(in_arguments_ptr[1].get_u32() ),
                                                                                                                                                                                  in_arguments_ptr[2].get_u32                             (),
                                                                                                                                                                                  vertex_ptr,
                                                                                                                                                                                  in_arguments_ptr[4].get_u32                             () );
    }
}

void Stereo::ConverterDLL::D3D8EyeRenderer::handle_copy_rects(const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t DESTINATION_ARGUMENT_INDEX = 4;
    constexpr size_t N_RECTS_ARGUMENT_INDEX     = 3;
    constexpr size_t POINTS_ARGUMENT_INDEX      = 5;
    constexpr size_t RECTS_ARGUMENT_INDEX       = 2;
    constexpr size_t SOURCE_ARGUMENT_INDEX      = 1;

    void* const destination_ptr      = get_pointer_argument(in_arguments_ptr[DESTINATION_ARGUMENT_INDEX]);
    void* const source_ptr           = get_pointer_argument(in_arguments_ptr[SOURCE_ARGUMENT_INDEX]);
    void* const destination_twin_ptr = get_twin_ptr        (destination_ptr);
    void* const source_for_right_ptr = get_twin_or_own_ptr (source_ptr);

    if (destination_twin_ptr != nullptr)
    {
        void* const device_ptr = get_pointer_argument(in_arguments_ptr[0]);

        APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_COPYRECTS>(device_ptr,
                                                                                  Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_COPYRECTS)(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr),
                                                                                                                                                      static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(source_for_right_ptr),
                                                                                                                                                      static_cast<const RECT*>                                  (in_arguments_ptr[RECTS_ARGUMENT_INDEX].get_ptr() ),
                                                                                                                                                      in_arguments_ptr[N_RECTS_ARGUMENT_INDEX].get_u32          (),
                                                                                                                                                      static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(destination_twin_ptr),
                                                                                                                                                      static_cast<const POINT*>                                 (in_arguments_ptr[POINTS_ARGUMENT_INDEX].get_ptr() ));
    }
}

void Stereo::ConverterDLL::D3D8EyeRenderer::handle_create_device(const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t DEVICE_ARGUMENT_INDEX = 6;

    Interception::Hooks::D3D8::IDirect3DSurface8* depth_ptr         = nullptr;
    Interception::Hooks::D3D8::IDirect3DSurface8* render_target_ptr = nullptr;

    m_device_ptr = get_pointer_argument(in_arguments_ptr[DEVICE_ARGUMENT_INDEX]);

    if (m_device_ptr != nullptr)
    {
        /* The device creates its back buffer and depth buffer itself. The application never asked for them. */
        if (SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_GETRENDERTARGET>(m_device_ptr,
                                                                                                                      Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_GETRENDERTARGET)(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(m_device_ptr),
                                                                                                                                                                                               &render_target_ptr) ))
        {
            make_surface_twin_ptr(m_device_ptr,
                                  render_target_ptr);

            m_mirror.back_buffer_ptr   = render_target_ptr;
            m_mirror.render_target_ptr = render_target_ptr;
        }

        if (SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_GETDEPTHSTENCILSURFACE>(m_device_ptr,
                                                                                                                             Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_GETDEPTHSTENCILSURFACE)(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(m_device_ptr),
                                                                                                                                                                                                             &depth_ptr) ))
        {
            make_surface_twin_ptr(m_device_ptr,
                                  depth_ptr);

            m_mirror.depth_stencil_ptr = depth_ptr;
        }
    }
}

void Stereo::ConverterDLL::D3D8EyeRenderer::handle_create_texture(const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t FORMAT_ARGUMENT_INDEX  = 5;
    constexpr size_t HEIGHT_ARGUMENT_INDEX  = 2;
    constexpr size_t LEVELS_ARGUMENT_INDEX  = 3;
    constexpr size_t POOL_ARGUMENT_INDEX    = 6;
    constexpr size_t TEXTURE_ARGUMENT_INDEX = 7;
    constexpr size_t USAGE_ARGUMENT_INDEX   = 4;
    constexpr size_t WIDTH_ARGUMENT_INDEX   = 1;

    void* const    texture_ptr = get_pointer_argument(in_arguments_ptr[TEXTURE_ARGUMENT_INDEX]);
    const uint32_t usage       = in_arguments_ptr[USAGE_ARGUMENT_INDEX].get_u32();

    if (texture_ptr                                                != nullptr &&
        (usage & Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET) != 0)
    {
        void* const                                   device_ptr = get_pointer_argument(in_arguments_ptr[0]);
        Interception::Hooks::D3D8::IDirect3DTexture8* twin_ptr   = nullptr;

        if (SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_CREATETEXTURE>(device_ptr,
                                                                                                                    Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_CREATETEXTURE)(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr),
                                                                                                                                                                                            in_arguments_ptr[WIDTH_ARGUMENT_INDEX].get_u32           (),
                                                                                                                                                                                            in_arguments_ptr[HEIGHT_ARGUMENT_INDEX].get_u32          (),
                                                                                                                                                                                            in_arguments_ptr[LEVELS_ARGUMENT_INDEX].get_u32          (),
                                                                                                                                                                                            in_arguments_ptr[USAGE_ARGUMENT_INDEX].get_u32           (),
                                                                                                                                                                                            static_cast<Interception::Hooks::D3D8::D3DFORMAT>        (in_arguments_ptr[FORMAT_ARGUMENT_INDEX].get_u32() ),
                                                                                                                                                                                            static_cast<Interception::Hooks::D3D8::D3DPOOL>          (in_arguments_ptr[POOL_ARGUMENT_INDEX].get_u32  () ),
                                                                                                                                                                                           &twin_ptr) ))
        {
            m_twin_map[texture_ptr] = twin_ptr;
        }
        else
        {
            m_n_failed_twins++;
        }
    }
}

void Stereo::ConverterDLL::D3D8EyeRenderer::handle_get_surface(const APIInterceptor::APIFunctionArgument& in_surface_argument)
{
    void* const surface_ptr = get_pointer_argument(in_surface_argument);

    if (surface_ptr  != nullptr &&
        m_device_ptr != nullptr)
    {
        make_surface_twin_ptr(m_device_ptr,
                              surface_ptr);
    }
}

void Stereo::ConverterDLL::D3D8EyeRenderer::handle_get_surface_level(const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t LEVEL_ARGUMENT_INDEX   = 1;
    constexpr size_t SURFACE_ARGUMENT_INDEX = 2;

    void* const surface_ptr       = get_pointer_argument(in_arguments_ptr[SURFACE_ARGUMENT_INDEX]);
    void* const texture_twin_ptr  = get_twin_ptr        (get_pointer_argument(in_arguments_ptr[0]));

    /* The surface of a level of the texture is the surface of the same level of its twin. */
    if (surface_ptr      != nullptr &&
        texture_twin_ptr != nullptr)
    {
        Interception::Hooks::D3D8::IDirect3DSurface8* twin_ptr = nullptr;

        if (SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DTEXTURE8_GETSURFACELEVEL>(texture_twin_ptr,
                                                                                                                       Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DTEXTURE8_GETSURFACELEVEL)(static_cast<Interception::Hooks::D3D8::IDirect3DTexture8*>(texture_twin_ptr),
                                                                                                                                                                                                  in_arguments_ptr[LEVEL_ARGUMENT_INDEX].get_u32(),
                                                                                                                                                                                                 &twin_ptr) ))
        {
            m_twin_map[surface_ptr] = twin_ptr;
        }
        else
        {
            m_n_failed_twins++;
        }
    }
}

void Stereo::ConverterDLL::D3D8EyeRenderer::handle_update_texture(const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t DESTINATION_ARGUMENT_INDEX = 2;
    constexpr size_t SOURCE_ARGUMENT_INDEX      = 1;

    void* const destination_ptr      = get_pointer_argument(in_arguments_ptr[DESTINATION_ARGUMENT_INDEX]);
    void* const source_ptr           = get_pointer_argument(in_arguments_ptr[SOURCE_ARGUMENT_INDEX]);
    void* const destination_twin_ptr = get_twin_ptr        (destination_ptr);
    void* const source_for_right_ptr = get_twin_or_own_ptr (source_ptr);

    if (destination_twin_ptr != nullptr)
    {
        void* const device_ptr = get_pointer_argument(in_arguments_ptr[0]);

        APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_UPDATETEXTURE>(device_ptr,
                                                                                                      Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_UPDATETEXTURE)(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>     (device_ptr),
                                                                                                                                                                              static_cast<Interception::Hooks::D3D8::IDirect3DBaseTexture8*>(source_for_right_ptr),
                                                                                                                                                                              static_cast<Interception::Hooks::D3D8::IDirect3DBaseTexture8*>(destination_twin_ptr));
    }
}

void* Stereo::ConverterDLL::D3D8EyeRenderer::make_surface_twin_ptr(void* in_device_ptr,
                                                                   void* in_surface_ptr)
{
    Interception::Hooks::D3D8::D3DSURFACE_DESC    description = {};
    bool                                          is_twin_wanted = false;
    Interception::Hooks::D3D8::IDirect3DSurface8* twin_ptr    = nullptr;

    if (get_twin_ptr(in_surface_ptr) == nullptr)
    {
        if (SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_GETDESC>(in_surface_ptr,
                                                                                                               Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_GETDESC)(static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(in_surface_ptr),
                                                                                                                                                                                 &description) ))
        {
            if ((description.Usage & Interception::Hooks::D3D8::D3DUSAGE_DEPTHSTENCIL) != 0)
            {
                is_twin_wanted = true;

                APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_CREATEDEPTHSTENCILSURFACE>(in_device_ptr,
                                                                                                                          Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_CREATEDEPTHSTENCILSURFACE)(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(in_device_ptr),
                                                                                                                                                                                                              description.Width,
                                                                                                                                                                                                              description.Height,
                                                                                                                                                                                                              description.Format,
                                                                                                                                                                                                              description.MultiSampleType,
                                                                                                                                                                                                             &twin_ptr);
            }
            else
            if ((description.Usage & Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET) != 0)
            {
                is_twin_wanted = true;

                APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_CREATERENDERTARGET>(in_device_ptr,
                                                                                                                   Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_CREATERENDERTARGET)(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(in_device_ptr),
                                                                                                                                                                                                description.Width,
                                                                                                                                                                                                description.Height,
                                                                                                                                                                                                description.Format,
                                                                                                                                                                                                description.MultiSampleType,
                                                                                                                                                                                                FALSE, /* Lockable */
                                                                                                                                                                                               &twin_ptr);
            }
        }
        else
        {
            /* A surface that cannot be described cannot be given a twin. */
            is_twin_wanted = true;
        }

        if (twin_ptr != nullptr)
        {
            m_twin_map[in_surface_ptr] = twin_ptr;
        }
        else
        if (is_twin_wanted)
        {
            m_n_failed_twins++;
        }
    }

    return get_twin_ptr(in_surface_ptr);
}

void Stereo::ConverterDLL::D3D8EyeRenderer::on_post(const APIInterceptor::APIFunction&         in_function,
                                                    const uint32_t&                            in_n_arguments,
                                                    const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t BACK_BUFFER_ARGUMENT_INDEX        = 3;
    constexpr size_t DEPTH_STENCIL_ARGUMENT_INDEX      = 1;
    constexpr size_t NEW_DEPTH_STENCIL_ARGUMENT_INDEX  = 5;
    constexpr size_t NEW_RENDER_TARGET_ARGUMENT_INDEX  = 6;
    constexpr size_t RENDER_TARGET_ARGUMENT_INDEX      = 1;

    (void) in_n_arguments;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3D8_CREATEDEVICE:
        {
            handle_create_device(in_arguments_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS:
        {
            handle_copy_rects(in_arguments_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEDEPTHSTENCILSURFACE:
        {
            handle_get_surface(in_arguments_ptr[NEW_DEPTH_STENCIL_ARGUMENT_INDEX]);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATERENDERTARGET:
        {
            handle_get_surface(in_arguments_ptr[NEW_RENDER_TARGET_ARGUMENT_INDEX]);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATETEXTURE:
        {
            m_device_ptr = get_pointer_argument(in_arguments_ptr[0]);

            handle_create_texture(in_arguments_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETBACKBUFFER:
        {
            handle_get_surface(in_arguments_ptr[BACK_BUFFER_ARGUMENT_INDEX]);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETDEPTHSTENCILSURFACE:
        {
            handle_get_surface(in_arguments_ptr[DEPTH_STENCIL_ARGUMENT_INDEX]);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETRENDERTARGET:
        {
            handle_get_surface(in_arguments_ptr[RENDER_TARGET_ARGUMENT_INDEX]);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_UPDATETEXTURE:
        {
            handle_update_texture(in_arguments_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_GETSURFACELEVEL:
        {
            handle_get_surface_level(in_arguments_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR:
        {
            play_right_eye(in_function,
                           in_arguments_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADER:
        {
            constexpr size_t VERTEX_SHADER_ARGUMENT_INDEX = 1;

            m_mirror.vertex_shader = in_arguments_ptr[VERTEX_SHADER_ARGUMENT_INDEX].get_u32();

            break;
        }

        default:
        {
            /* A skipped draw is not repeated for the right eye. */
            const bool is_draw = LiveCallAnalyzer::is_draw(in_function);

            if (is_draw)
            {
                /* Not made again: the renderer made it for both eyes in on_pre_draw(). */
                if (m_draw_class      != Analysis::Recipe::DrawClass::SKIPPED &&
                    m_is_draw_handled == false)
                {
                    play_right_eye(in_function,
                                   in_arguments_ptr);
                }
            }

            break;
        }
    }
}

bool Stereo::ConverterDLL::D3D8EyeRenderer::on_pre(const APIInterceptor::APIFunction&         in_function,
                                                   const uint32_t&                            in_n_arguments,
                                                   const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t CUBE_USAGE_ARGUMENT_INDEX     = 3;
    constexpr size_t DEPTH_STENCIL_ARGUMENT_INDEX  = 2;
    constexpr size_t RENDER_TARGET_ARGUMENT_INDEX  = 1;
    constexpr size_t STAGE_ARGUMENT_INDEX          = 1;
    constexpr size_t TEXTURE_ARGUMENT_INDEX        = 2;
    constexpr size_t VOLUME_USAGE_ARGUMENT_INDEX   = 5;

    (void) in_n_arguments;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DCUBETEXTURE8_LOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_LOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_LOCKRECT:
        {
            const bool is_ok = (get_twin_ptr(get_pointer_argument(in_arguments_ptr[0])) == nullptr);

            m_core_ptr->require_fast_path(is_ok,
                                          "The application locks a render target, whose images the right eye would not have");

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATECUBETEXTURE:
        {
            m_core_ptr->require_fast_path((in_arguments_ptr[CUBE_USAGE_ARGUMENT_INDEX].get_u32() & Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET) == 0,
                                          "The application renders to a cube texture");

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVOLUMETEXTURE:
        {
            m_core_ptr->require_fast_path((in_arguments_ptr[VOLUME_USAGE_ARGUMENT_INDEX].get_u32() & Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET) == 0,
                                          "The application renders to a volume texture");

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_RESET:
        {
            m_core_ptr->require_fast_path(false, /* in_is_ok */
                                          "The application resets the device, which loses the twins of what it renders to");

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERTARGET:
        {
            void* const depth_stencil_ptr = get_pointer_argument(in_arguments_ptr[DEPTH_STENCIL_ARGUMENT_INDEX]);
            void* const render_target_ptr = get_pointer_argument(in_arguments_ptr[RENDER_TARGET_ARGUMENT_INDEX]);

            /* In Direct3D 8, passing no surface leaves the render target or the depth buffer unchanged. */
            if (render_target_ptr != nullptr)
            {
                const bool is_ok = (get_twin_ptr(render_target_ptr) != nullptr);

                m_core_ptr->require_fast_path(is_ok,
                                              "The application renders to a surface that the converter did not see being made");

                m_mirror.render_target_ptr = render_target_ptr;
            }

            if (depth_stencil_ptr != nullptr)
            {
                m_mirror.depth_stencil_ptr = depth_stencil_ptr;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTEXTURE:
        {
            const uint32_t stage = in_arguments_ptr[STAGE_ARGUMENT_INDEX].get_u32();

            if (stage < N_TEXTURE_STAGES)
            {
                m_mirror.texture_ptr_array.at(stage) = get_pointer_argument(in_arguments_ptr[TEXTURE_ARGUMENT_INDEX]);
            }

            break;
        }

        default:
        {
            break;
        }
    }

    return true;
}

bool Stereo::ConverterDLL::D3D8EyeRenderer::on_pre_draw(const Analysis::Recipe::DrawClass&         in_draw_class,
                                                        const APIInterceptor::APIFunction&         in_function,
                                                        const float&                               in_picture_width,
                                                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    bool result = (in_draw_class != Analysis::Recipe::DrawClass::SKIPPED);

    m_draw_class      = in_draw_class;
    m_is_draw_handled = false;

    if (in_draw_class != Analysis::Recipe::DrawClass::SKIPPED)
    {
        /* XYZRHW vertices are in pixels, so no matrix moves them. The renderer makes the draw for both eyes, with the vertices moved by the core's plan. */
        const bool is_pre_transformed = Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::is_pre_transformed_vertex_shader(m_mirror.vertex_shader);

        if (is_pre_transformed)
        {
            draw_pre_transformed(in_function,
                                 in_arguments_ptr,
                                 in_picture_width);

            m_is_draw_handled = true;
            result            = false;
        }
        else
        {
            void* const    device_ptr      = get_pointer_argument(in_arguments_ptr[0]);
            const bool     is_final_target = (m_mirror.render_target_ptr == m_mirror.back_buffer_ptr);
            const uint32_t n_changes       = m_core_ptr->bind_matrices(in_draw_class,
                                                                       Analysis::Recipe::Eye::LEFT,
                                                                      &m_change_vec,
                                                                       is_final_target);

            apply_matrices(device_ptr,
                           n_changes);
        }
    }

    return result;
}

void Stereo::ConverterDLL::D3D8EyeRenderer::play_right_eye(const APIInterceptor::APIFunction&         in_function,
                                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    const auto draw_func = [this, &in_function, in_arguments_ptr]()
    {
        repeat_call(in_function,
                    in_arguments_ptr);
    };

    play_right_eye(get_pointer_argument(in_arguments_ptr[0]),
                   draw_func);
}

void Stereo::ConverterDLL::D3D8EyeRenderer::play_right_eye(void*                        in_device_ptr,
                                                           const std::function<void()>& in_draw_func)
{
    void* const                                  device_ptr         = in_device_ptr;
    Interception::Hooks::D3D8::IDirect3DDevice8* device             = static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr);
    std::array<bool, N_TEXTURE_STAGES>           is_texture_swapped = {};
    void*                                        twin_depth_ptr     = nullptr;
    void*                                        twin_target_ptr    = nullptr;
    Interception::Hooks::D3D8::D3DVIEWPORT8      viewport           = {};

    const auto get_viewport_func      = APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_GETVIEWPORT>    (device_ptr,
                                                                                                                                        Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_GETVIEWPORT);
    const auto set_render_target_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_SETRENDERTARGET>(device_ptr,
                                                                                                                                        Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_SETRENDERTARGET);
    const auto set_texture_func       = APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_SETTEXTURE>     (device_ptr,
                                                                                                                                        Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_SETTEXTURE);
    const auto set_viewport_func      = APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_SETVIEWPORT>    (device_ptr,
                                                                                                                                        Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_SETVIEWPORT);

    twin_target_ptr = get_twin_ptr(m_mirror.render_target_ptr);
    twin_depth_ptr  = (m_mirror.depth_stencil_ptr != nullptr) ? get_twin_ptr(m_mirror.depth_stencil_ptr)
                                                              : nullptr;

    m_core_ptr->require_fast_path(twin_target_ptr            != nullptr,
                                  "The application draws to a surface that the converter did not see being made");
    m_core_ptr->require_fast_path(m_mirror.depth_stencil_ptr == nullptr ||
                                  twin_depth_ptr             != nullptr,
                                  "The application draws with a depth buffer that the converter did not see being made");

    if (twin_target_ptr != nullptr)
    {
        get_viewport_func(device,
                         &viewport);

        /* Setting a render target can reset the viewport to cover the whole target. */
        set_render_target_func(device,
                               static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(twin_target_ptr),
                               static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(twin_depth_ptr) );
        set_viewport_func     (device,
                              &viewport);

        for (uint32_t n_stage = 0;
                      n_stage < N_TEXTURE_STAGES;
                    ++n_stage)
        {
            void* const twin_texture_ptr = (m_mirror.texture_ptr_array.at(n_stage) != nullptr) ? get_twin_ptr(m_mirror.texture_ptr_array.at(n_stage) )
                                                                                               : nullptr;

            if (twin_texture_ptr != nullptr)
            {
                set_texture_func(device,
                                 n_stage,
                                 static_cast<Interception::Hooks::D3D8::IDirect3DBaseTexture8*>(twin_texture_ptr) );

                is_texture_swapped.at(n_stage) = true;
            }
        }

        {
            const uint32_t n_changes = m_core_ptr->bind_matrices(m_draw_class,
                                                                 Analysis::Recipe::Eye::RIGHT,
                                                                &m_change_vec,
                                                                 m_mirror.render_target_ptr == m_mirror.back_buffer_ptr);

            apply_matrices(device_ptr,
                           n_changes);
        }

        in_draw_func();

        /* The render target and depth buffer that the application set are set in the device again. */
        set_render_target_func(device,
                               static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(m_mirror.render_target_ptr),
                               static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(m_mirror.depth_stencil_ptr) );
        set_viewport_func     (device,
                              &viewport);

        for (uint32_t n_stage = 0;
                      n_stage < N_TEXTURE_STAGES;
                    ++n_stage)
        {
            if (is_texture_swapped.at(n_stage) )
            {
                set_texture_func(device,
                                 n_stage,
                                 static_cast<Interception::Hooks::D3D8::IDirect3DBaseTexture8*>(m_mirror.texture_ptr_array.at(n_stage) ));
            }
        }
    }
}

void Stereo::ConverterDLL::D3D8EyeRenderer::repeat_call(const APIInterceptor::APIFunction&         in_function,
                                                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr) const
{
    void* const                                  device_ptr = get_pointer_argument(in_arguments_ptr[0]);
    Interception::Hooks::D3D8::IDirect3DDevice8* device     = static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(device_ptr);

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR:
        {
            APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_CLEAR>(device_ptr,
                                                                                                  Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_CLEAR)(device,
                                                                                                                                                                  in_arguments_ptr[1].get_u32                           (),
                                                                                                                                                                  static_cast<const Interception::Hooks::D3D8::D3DRECT*>(in_arguments_ptr[2].get_ptr() ),
                                                                                                                                                                  in_arguments_ptr[3].get_u32                           (),
                                                                                                                                                                  in_arguments_ptr[4].get_u32                           (),
                                                                                                                                                                  in_arguments_ptr[5].get_fp32                          (),
                                                                                                                                                                  in_arguments_ptr[6].get_u32                           () );

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE:
        {
            APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE>(device_ptr,
                                                                                                                 Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE)(device,
                                                                                                                                                                                                static_cast<Interception::Hooks::D3D8::D3DPRIMITIVETYPE>(in_arguments_ptr[1].get_u32() ),
                                                                                                                                                                                                in_arguments_ptr[2].get_u32                             (),
                                                                                                                                                                                                in_arguments_ptr[3].get_u32                             (),
                                                                                                                                                                                                in_arguments_ptr[4].get_u32                             (),
                                                                                                                                                                                                in_arguments_ptr[5].get_u32                             () );

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP:
        {
            APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP>(device_ptr,
                                                                                                                   Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP)(device,
                                                                                                                                                                                                    static_cast<Interception::Hooks::D3D8::D3DPRIMITIVETYPE>(in_arguments_ptr[1].get_u32() ),
                                                                                                                                                                                                    in_arguments_ptr[2].get_u32                             (),
                                                                                                                                                                                                    in_arguments_ptr[3].get_u32                             (),
                                                                                                                                                                                                    in_arguments_ptr[4].get_u32                             (),
                                                                                                                                                                                                    in_arguments_ptr[5].get_ptr                             (),
                                                                                                                                                                                                    static_cast<Interception::Hooks::D3D8::D3DFORMAT>       (in_arguments_ptr[6].get_u32() ),
                                                                                                                                                                                                    in_arguments_ptr[7].get_ptr                             (),
                                                                                                                                                                                                    in_arguments_ptr[8].get_u32                             () );

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE:
        {
            APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_DRAWPRIMITIVE>(device_ptr,
                                                                                                          Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_DRAWPRIMITIVE)(device,
                                                                                                                                                                                  static_cast<Interception::Hooks::D3D8::D3DPRIMITIVETYPE>(in_arguments_ptr[1].get_u32() ),
                                                                                                                                                                                  in_arguments_ptr[2].get_u32                             (),
                                                                                                                                                                                  in_arguments_ptr[3].get_u32                             () );

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP:
        {
            APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP>(device_ptr,
                                                                                                            Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP)(device,
                                                                                                                                                                                      static_cast<Interception::Hooks::D3D8::D3DPRIMITIVETYPE>(in_arguments_ptr[1].get_u32() ),
                                                                                                                                                                                      in_arguments_ptr[2].get_u32                             (),
                                                                                                                                                                                      in_arguments_ptr[3].get_ptr                             (),
                                                                                                                                                                                      in_arguments_ptr[4].get_u32                             () );

            break;
        }

        default:
        {
            break;
        }
    }
}
