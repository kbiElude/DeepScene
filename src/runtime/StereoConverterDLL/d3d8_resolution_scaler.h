/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D8_RESOLUTION_SCALER_H
#define STEREO_CONVERTERDLL_D3D8_RESOLUTION_SCALER_H

#include "Analysis/APIDumpAnalysis/types.h"
#include "Interception/Hooks/D3D8/argument_modifier.h"
#include "Stereo/Core/resolution_policy.h"
#include "Stereo/Core/stereo_core.h"
#include "Stereo/Core/types.h"
#include <functional>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Has a Direct3D 8 application render larger than it asks (see ResolutionPolicy), without its knowing. It makes the back buffer, and so
         * the depth buffer that goes with it, larger as the application makes its device, and makes the viewports that the application sets
         * for it larger: a viewport is in pixels, and what the application means by it is a part of the image, not of that many pixels. A
         * render target of another size (what the application made itself) is left as it is, and so are the viewports for it.
         *
         * What the application does that it cannot be kept from seeing the larger sizes in, or that does not scale with them, ends the
         * application, through the core, instead of showing it wrong (see on_pre()). Nothing of the kind is in fr-08; each production has
         * to be found out.
         */
        class D3D8ResolutionScaler : public Interception::Hooks::D3D8::IArgumentModifier
        {
        public:
            /* What the render target which the application renders to now is: the surface, or null if the scaler is not to know. */
            typedef std::function<void*()> RenderTargetFunc;

            /* The core must outlive the scaler. What the application renders is made as large as fits the target (the size of an eye of the
             * headset) with its proportions, when the application says how large it is; no target, a size of 0, or a target that is not larger
             * than what the application asks for, makes the scaler do nothing.
             */
            D3D8ResolutionScaler(const uint32_t&              in_target_width,
                                 const uint32_t&              in_target_height,
                                 Stereo::Core::StereoCore*    in_core_ptr,
                                 const RenderTargetFunc&      in_render_target_func,
                                 const Stereo::Core::LogFunc& in_log_func);
            ~D3D8ResolutionScaler() override;

            D3D8ResolutionScaler(const D3D8ResolutionScaler&)            = delete;
            D3D8ResolutionScaler& operator=(const D3D8ResolutionScaler&) = delete;

            void on_create_device(Interception::Hooks::D3D8::IDirect3D8*            in_direct3d_ptr,
                                  UINT                                              in_adapter,
                                  HWND                                              in_focus_window,
                                  Interception::Hooks::D3D8::D3DPRESENT_PARAMETERS* inout_parameters_ptr) override;

            /* A call of the application is about to be made. */
            void on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            void on_set_viewport(Interception::Hooks::D3D8::IDirect3DDevice8*    in_device_ptr,
                                 const Interception::Hooks::D3D8::D3DVIEWPORT8** inout_viewport_ptr_ptr) override;

            /* Puts the sizes that the application asked for back in the parameters of the device that it made, for the analysis to see: the
             * back buffer is part of what a draw is told by, and a draw must be told the same at any scale.
             */
            void restore_chunk(const APIInterceptor::APIFunction& in_function,
                               std::vector<uint8_t>*              inout_chunk_ptr) const;

        private:
            /* What the application does, as it is told to the person it is stopped for. */
            static const char* get_hazard_reason(const Analysis::APIDumpAnalysis::ResolutionHazard& in_hazard);

            uint32_t                                m_asked_height; /* of the back buffer, 0 until the device is made */
            uint32_t                                m_asked_width;
            Stereo::Core::StereoCore*               m_core_ptr;
            Stereo::Core::LogFunc                   m_log_func;
            Stereo::Core::ResolutionPolicy          m_policy; /* made when the application says how large its image is */
            uint32_t                                m_target_height;
            uint32_t                                m_target_width;
            uint32_t                                m_real_height; /* of the back buffer, 0 until the device is made */
            uint32_t                                m_real_width;
            RenderTargetFunc                        m_render_target_func;
            Interception::Hooks::D3D8::D3DVIEWPORT8 m_viewport;    /* the one set instead of the application's, for the length of a call */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D8_RESOLUTION_SCALER_H */
