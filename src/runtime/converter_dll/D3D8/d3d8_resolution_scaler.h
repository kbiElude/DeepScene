/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D8_RESOLUTION_SCALER_H
#define STEREO_CONVERTERDLL_D3D8_RESOLUTION_SCALER_H

#include "analysis/api_dump_analysis/types.h"
#include "common/api_function_argument.h"
#include "interception/hooks/D3D8/argument_modifier.h"
#include "stereo/core/resolution_policy.h"
#include "stereo/core/recipe_stereo_planner.h"
#include "stereo/core/types.h"
#include <functional>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Makes a Direct3D 8 application render larger than it asks for (see ResolutionPolicy), without its knowledge. When the application
         * creates its device, the scaler enlarges the back buffer, and with it the depth buffer. It also enlarges every viewport that the
         * application sets for the back buffer: a viewport is in pixels, but the application means a part of the image, not a number of
         * pixels. A render target of another size (one the application created itself) stays as it is, and so do the viewports set for it.
         *
         * If the application does something that exposes the larger sizes, or that does not scale with them, the scaler ends the application
         * through the core instead of showing wrong images (see on_pre()). fr-08 does nothing of this kind. Every other production has to be
         * checked separately.
         */
        class D3D8ResolutionScaler : public Interception::Hooks::D3D8::IArgumentModifier
        {
        public:
            /* The render target that the application currently renders to (a surface), or null if it is unknown. */
            typedef std::function<void*()> RenderTargetFunc;

            /* The core must outlive the scaler. When the application states how large its image is, the scaler enlarges it as far as it fits the
             * target (the size of one eye of the headset), keeping the proportions. The scaler does nothing if there is no target, if the target
             * size is 0, or if the target is not larger than the size the application asks for.
             */
            D3D8ResolutionScaler(const uint32_t&                    in_target_width,
                                 const uint32_t&                    in_target_height,
                                 Stereo::Core::RecipeStereoPlanner* in_core_ptr,
                                 const RenderTargetFunc&            in_render_target_func,
                                 const Stereo::Core::LogFunc&       in_log_func);
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

            /* Writes the sizes that the application asked for back into the device parameters of the chunk, so that the analysis sees them. The
             * back buffer is part of what identifies a draw, and a draw must have the same identity at any scale.
             */
            void restore_chunk(const APIInterceptor::APIFunction& in_function,
                               std::vector<uint8_t>*              inout_chunk_ptr) const;

        private:
            uint32_t                                m_asked_height; /* the height the application asked for for the back buffer; 0 until the device is created */
            uint32_t                                m_asked_width;
            Stereo::Core::RecipeStereoPlanner*      m_core_ptr;
            Stereo::Core::LogFunc                   m_log_func;
            Stereo::Core::ResolutionPolicy          m_policy; /* set when the application states how large its image is */
            uint32_t                                m_target_height;
            uint32_t                                m_target_width;
            uint32_t                                m_real_height; /* the enlarged height of the back buffer; 0 until the device is created */
            uint32_t                                m_real_width;
            RenderTargetFunc                        m_render_target_func;
            Interception::Hooks::D3D8::D3DVIEWPORT8 m_viewport;    /* the viewport set in place of the application's, valid for the duration of one call */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D8_RESOLUTION_SCALER_H */
