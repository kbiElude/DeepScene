/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d8_resolution_scaler.h"
#include "eye_image_limits.h"
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_resolution_hazards.h"
#include "common/macros.h"
#include "common/vtable.h"
#include <string.h>

Stereo::ConverterDLL::D3D8ResolutionScaler::D3D8ResolutionScaler(const uint32_t&              in_target_width,
                                                                 const uint32_t&              in_target_height,
                                                                 Stereo::Core::StereoCore*    in_core_ptr,
                                                                 const RenderTargetFunc&      in_render_target_func,
                                                                 const Stereo::Core::LogFunc& in_log_func)
    :m_asked_height      (0),
     m_asked_width       (0),
     m_core_ptr          (in_core_ptr),
     m_log_func          (in_log_func),
     m_policy            (Stereo::Core::ResolutionPolicy::get_min_scale() ),
     m_real_height       (0),
     m_real_width        (0),
     m_render_target_func(in_render_target_func),
     m_target_height     (in_target_height),
     m_target_width      (in_target_width),
     m_viewport          ()
{
    /* Whether anything is made larger is known when the application makes its device; until then the scaler is there for it. */
    if (m_target_width  > 0 &&
        m_target_height > 0)
    {
        Interception::Hooks::D3D8::add_argument_modifier(this);
    }
}

Stereo::ConverterDLL::D3D8ResolutionScaler::~D3D8ResolutionScaler()
{
    Interception::Hooks::D3D8::remove_argument_modifier(this);
}

const char* Stereo::ConverterDLL::D3D8ResolutionScaler::get_hazard_reason(const Analysis::APIDumpAnalysis::ResolutionHazard& in_hazard)
{
    const char* result_ptr = "The application does what depends on the size of what it renders to";

    switch (in_hazard)
    {
        case Analysis::APIDumpAnalysis::ResolutionHazard::CLEARS_WITH_RECTANGLES:
        {
            result_ptr = "The application clears rectangles, which are in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::COPIES_WITH_RECTANGLES:
        {
            result_ptr = "The application copies rectangles between surfaces, which are in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::POINT_SIZE_STATES:
        {
            result_ptr = "The application sets the size of points, which is in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::SCREEN_SPACE_VERTICES:
        {
            result_ptr = "The application draws vertices that are positioned in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::SHADER_CREATIONS:
        {
            result_ptr = "The application makes shaders, whose constants may hold the size of the screen";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES:
        {
            result_ptr = "The application reads the size of the viewport or of a surface, which is of a size it does not know of";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_hazard);

            break;
        }
    }

    return result_ptr;
}

void Stereo::ConverterDLL::D3D8ResolutionScaler::on_create_device(Interception::Hooks::D3D8::IDirect3D8*            in_direct3d_ptr,
                                                                  UINT                                              in_adapter,
                                                                  HWND                                              in_focus_window,
                                                                  Interception::Hooks::D3D8::D3DPRESENT_PARAMETERS* inout_parameters_ptr)
{
    (void) in_direct3d_ptr;
    (void) in_adapter;
    (void) in_focus_window;

    /* A size of 0 is the size of the window, which is not the application's to say. */
    m_core_ptr->require_fast_path(inout_parameters_ptr->BackBufferWidth  != 0 &&
                                  inout_parameters_ptr->BackBufferHeight != 0,
                                  "The application lets its window decide how large the back buffer is, which cannot be made larger");

    if (inout_parameters_ptr->BackBufferWidth  != 0 &&
        inout_parameters_ptr->BackBufferHeight != 0)
    {
        m_asked_height = inout_parameters_ptr->BackBufferHeight;
        m_asked_width  = inout_parameters_ptr->BackBufferWidth;
        m_policy       = Stereo::Core::ResolutionPolicy(Stereo::Core::ResolutionPolicy::get_fit_scale(m_target_width,
                                                                                                      m_target_height,
                                                                                                      m_asked_width,
                                                                                                      m_asked_height) );
        m_real_height  = m_policy.scale_extent(inout_parameters_ptr->BackBufferHeight);
        m_real_width   = m_policy.scale_extent(inout_parameters_ptr->BackBufferWidth);

        m_log_func("The back buffer is made "                             +
                   std::to_string(m_real_width)                           +
                   " x "                                                  +
                   std::to_string(m_real_height)                          +
                   " instead of "                                         +
                   std::to_string(inout_parameters_ptr->BackBufferWidth)  +
                   " x "                                                  +
                   std::to_string(inout_parameters_ptr->BackBufferHeight) +
                   " (scale "                                             +
                   std::to_string(m_policy.get_scale() )                  +
                   ")");

        /* Both eyes are passed on side by side, in an image of a size which has its limit; it is better said here than seen as no picture. */
        {
            const bool        is_supported = EyeImageLimits::fits(m_real_width,
                                                                  m_real_height);
            const std::string reason       = "At the render scale "                                    +
                                             std::to_string(m_policy.get_scale() )                     +
                                             " the images of the eyes are "                            +
                                             std::to_string(m_real_width)                              +
                                             " x "                                                     +
                                             std::to_string(m_real_height)                             +
                                             " each, which is larger than what can be passed on ("     +
                                             std::to_string(EyeImageLimits::get_max_width() )          +
                                             " x "                                                     +
                                             std::to_string(EyeImageLimits::get_max_height() )         +
                                             " each); lower the render scale";

            m_core_ptr->require_supported(is_supported,
                                          reason);
        }

        inout_parameters_ptr->BackBufferHeight = m_real_height;
        inout_parameters_ptr->BackBufferWidth  = m_real_width;
    }
}

void Stereo::ConverterDLL::D3D8ResolutionScaler::on_pre(const APIInterceptor::APIFunction&         in_function,
                                                        const uint32_t&                            in_n_arguments,
                                                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::CLEARS_WITH_RECTANGLES;

    /* What the application does that it cannot be kept from seeing the larger sizes in, or that does not scale with them, ends it. */
    const bool is_identity = m_policy.is_identity();

    if (is_identity == false)
    {
        const bool is_hazard = Analysis::APIDumpAnalysis::Frontends::D3D8ResolutionHazards::classify(in_function,
                                                                                                     in_n_arguments,
                                                                                                     in_arguments_ptr,
                                                                                                    &hazard);

        if (is_hazard)
        {
            const std::string reason = std::string(get_hazard_reason(hazard) ) + ", while it is rendered larger than it asks";

            m_core_ptr->require_fast_path(false, /* in_is_ok */
                                          reason);
        }
    }
}

void Stereo::ConverterDLL::D3D8ResolutionScaler::on_set_viewport(Interception::Hooks::D3D8::IDirect3DDevice8*    in_device_ptr,
                                                                 const Interception::Hooks::D3D8::D3DVIEWPORT8** inout_viewport_ptr_ptr)
{
    void* const render_target_ptr = (m_render_target_func) ? m_render_target_func()
                                                           : nullptr;

    (void) in_device_ptr;

    /* The viewports for the back buffer are made as large as it was; those for another target are the application's. */
    if (render_target_ptr != nullptr &&
        m_real_width      != 0)
    {
        Interception::Hooks::D3D8::D3DSURFACE_DESC description = {};
        void** const                               vtable      = APIInterceptor::get_vtable_ptr(render_target_ptr);

        const HRESULT description_result = APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_GETDESC>(vtable, Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_GETDESC)(static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(render_target_ptr),
                                                                                                                                                                                                &description);

        if (SUCCEEDED(description_result) )
        {
            if (description.Width  == m_real_width &&
                description.Height == m_real_height)
            {
                m_viewport = **inout_viewport_ptr_ptr;

                m_viewport.X      = m_policy.scale_extent(m_viewport.X);
                m_viewport.Y      = m_policy.scale_extent(m_viewport.Y);
                m_viewport.Width  = m_policy.scale_extent(m_viewport.Width);
                m_viewport.Height = m_policy.scale_extent(m_viewport.Height);

                /* Rounding must not take the viewport out of the target. */
                if (m_viewport.X + m_viewport.Width > m_real_width)
                {
                    m_viewport.Width = (m_viewport.X < m_real_width) ? m_real_width - m_viewport.X
                                                                     : 0;
                }

                if (m_viewport.Y + m_viewport.Height > m_real_height)
                {
                    m_viewport.Height = (m_viewport.Y < m_real_height) ? m_real_height - m_viewport.Y
                                                                       : 0;
                }

                *inout_viewport_ptr_ptr = &m_viewport;
            }
        }
    }
}

void Stereo::ConverterDLL::D3D8ResolutionScaler::restore_chunk(const APIInterceptor::APIFunction& in_function,
                                                               std::vector<uint8_t>*              inout_chunk_ptr) const
{
    const size_t n_chunk_bytes = inout_chunk_ptr->size();

    if (in_function   == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3D8_CREATEDEVICE &&
        m_asked_width != 0                                                        &&
        n_chunk_bytes == sizeof(Interception::Hooks::D3D8::D3DPRESENT_PARAMETERS) )
    {
        Interception::Hooks::D3D8::D3DPRESENT_PARAMETERS parameters;

        ::memcpy(&parameters,
                 inout_chunk_ptr->data(),
                 sizeof(parameters) );

        parameters.BackBufferHeight = m_asked_height;
        parameters.BackBufferWidth  = m_asked_width;

        ::memcpy(inout_chunk_ptr->data(),
                &parameters,
                 sizeof(parameters) );
    }
}
