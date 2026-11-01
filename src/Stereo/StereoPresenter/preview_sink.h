/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOPRESENTER_PREVIEW_SINK_H
#define STEREOPRESENTER_PREVIEW_SINK_H

#include "Stereo/StereoPresenter/i_eye_sink.h"
#include <functional>
#include <stdint.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;

namespace Stereo
{
    namespace StereoPresenter
    {
        /* Keeps the newest frame in a Direct3D 11 texture, for a window to draw. The texture is made again when the size of the frames
         * changes, and the function is told then.
         */
        class PreviewSink : public IEyeSink
        {
        public:
            typedef std::function<void(const uint32_t& in_width,
                                       const uint32_t& in_height)> SizeChangedFunc;

            /* The device and its context must outlive the sink. */
            PreviewSink(ID3D11Device*          in_device_ptr,
                        ID3D11DeviceContext*   in_context_ptr,
                        const SizeChangedFunc& in_size_changed_func);
            ~PreviewSink();

            PreviewSink(const PreviewSink&)            = delete;
            PreviewSink& operator=(const PreviewSink&) = delete;

            /* Null until the first frame. */
            ID3D11ShaderResourceView* get_view_ptr() const;

            uint32_t get_height() const;

            uint32_t get_width () const;

            void on_frame(const Stereo::StereoTransport::ImportedFrame& in_frame) override;

        private:
            void release_texture();

            ID3D11DeviceContext* m_context_ptr;
            ID3D11Device*        m_device_ptr;
            uint32_t             m_height;
            uint64_t             m_last_frame_index;
            SizeChangedFunc      m_size_changed_func;
            ID3D11Texture2D*     m_texture_ptr;
            ID3D11ShaderResourceView* m_view_ptr;
            uint32_t             m_width;
        };
    }
}

#endif /* STEREOPRESENTER_PREVIEW_SINK_H */
