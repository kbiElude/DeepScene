/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_PLAYER_PREVIEW_WINDOW_H
#define STEREO_REPLAY_PLAYER_PREVIEW_WINDOW_H

#include "tools/Replay/StereoReplayEngineDLL/i_frame_image_sink.h"
#include "eye_image_buffer.h"
#include "types.h"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>
#include <Windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11PixelShader;
struct ID3D11RenderTargetView;
struct ID3D11SamplerState;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;
struct ID3D11VertexShader;
struct ID3D10Blob;
typedef ID3D10Blob ID3DBlob;
struct IDXGISwapChain;

namespace Stereo
{
    namespace Replay
    {
        namespace Player
        {
            /* Shows the images of the eyes side by side (or the one image, for one eye). It has a Direct3D 11 device of its own, as
             * the images are read back from the device of the replay. The window lives on a thread of its own, so that it answers
             * the messages the replay sends to the windows of the process whatever the main thread is doing.
             */
            class PreviewWindow : public Stereo::Replay::EngineDLL::IFrameImageSink
            {
            public:
                /* Called, from the thread of the window, when the user closes it. Before create(). */
                typedef std::function<void()> CloseFunc;


                explicit PreviewWindow(const EyeMode& in_eye_mode);

                ~PreviewWindow();

                /* Returns false, with the reason, if the window or the device cannot be made. */
                bool create(std::string* out_error_ptr);

                /* Any thread; the images are copied. */
                void on_frame_image(const uint32_t&                              in_n_frame,
                                    const uint32_t&                              in_n_execution,
                                    const Stereo::Replay::EngineDLL::FrameImage& in_image) override;

                void set_close_func(const CloseFunc& in_close_func);

                /* The frames before the first are not shown, so they are not read back: for a replay that hurries up to a frame. The first
                 * and the last frame are always shown, since the replay may stop on them.
                 */
                void set_frame_range(const uint32_t& in_first_frame,
                                     const uint32_t& in_last_frame);

                /* False while the window is hidden, before the first frame to show, and for the frames in between which come sooner than the
                 * window can show them: reading an image waits for the device. The answer is the same for the executions of a frame.
                 * Replay thread only.
                 */
                bool wants_frame_image(const uint32_t& in_n_frame,
                                       const uint32_t& in_n_execution) const override;

            private:
                struct EyeTexture
                {
                    uint32_t                  height   = 0;
                    ID3D11Texture2D*          tex_ptr  = nullptr;
                    ID3D11ShaderResourceView* view_ptr = nullptr;
                    uint32_t                  width    = 0;
                };

                /* When an image was wanted last. Replay thread only, so it has no lock. */
                struct ImageRate
                {
                    uint32_t                              n_decided_frame = 0;
                    bool                                  is_decided      = false;
                    bool                                  is_wanted       = false;
                    std::chrono::steady_clock::time_point last_wanted_time;
                };

                /* How the thread says that the window is up, or why it is not. Lock order: no other lock is taken while it is held. */
                struct Startup
                {
                    std::condition_variable cv;
                    std::string             error;
                    bool                    is_done = false;
                    bool                    is_ok   = false;
                    std::mutex              mutex;
                };


                bool    create_device        (std::string*                 out_error_ptr);
                void    create_render_target ();
                bool    create_window        (std::string*                 out_error_ptr);
                void    destroy_render_target();
                void    destroy_resources    ();
                void    draw_eye             (const EyeTexture&            in_texture,
                                              const RECT&                  in_cell);
                LRESULT handle_message       (const HWND&                  in_window,
                                              const UINT&                  in_message,
                                              const WPARAM&                in_wparam,
                                              const LPARAM&                in_lparam);
                void    render               ();
                void    resize               ();
                void    run_thread           ();
                void    upload               (const EyeImageBuffer::Image& in_image,
                                              EyeTexture*                  inout_texture_ptr);

                static std::string      get_blob_text(ID3DBlob* in_blob_ptr);
                static LRESULT CALLBACK window_proc  (HWND      in_window,
                                                      UINT      in_message,
                                                      WPARAM    in_wparam,
                                                      LPARAM    in_lparam);

                template<typename T>
                static void release(T** inout_ptr_ptr);


                CloseFunc                                                 m_close_func;
                ID3D11DeviceContext*                                      m_device_context_ptr;
                ID3D11Device*                                             m_device_ptr;
                std::atomic<uint32_t>                                     m_first_frame;
                EyeMode                                                   m_eye_mode;
                EyeTexture                                                m_eye_texture[EyeImageBuffer::N_EYES];
                HWND                                                      m_hwnd;
                EyeImageBuffer                                            m_image_buffer;
                mutable ImageRate                                         m_image_rate;
                std::atomic<bool>                                         m_is_visible;
                std::atomic<uint32_t>                                     m_last_frame;
                ID3D11PixelShader*                                        m_pixel_shader_ptr;
                ID3D11RenderTargetView*                                   m_render_target_view_ptr;
                ID3D11SamplerState*                                       m_sampler_ptr;
                Startup                                                   m_startup;
                IDXGISwapChain*                                           m_swap_chain_ptr;
                std::thread                                               m_thread;
                std::array<EyeImageBuffer::Image, EyeImageBuffer::N_EYES> m_ui_image_array;
                ID3D11VertexShader*                                       m_vertex_shader_ptr;
            };
        }
    }
}

#endif /* STEREO_REPLAY_PLAYER_PREVIEW_WINDOW_H */
