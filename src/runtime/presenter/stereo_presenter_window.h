/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_PRESENTER_STEREO_PRESENTER_WINDOW_H
#define STEREO_PRESENTER_STEREO_PRESENTER_WINDOW_H

#include "stereo/transport/i_frame_importer.h"
#include "headset_sink.h"
#include "i_eye_sink.h"
#include "preview_sink.h"
#include "vr/ui_host/imgui_window_host.h"
#include <memory>
#include <string>

namespace Stereo
{
    namespace Presenter
    {
        /* The window which shows what the converter of an application passes on: the newest image, as large as the window lets it
         * be with its proportions kept. Stereo images, one for each eye, are side by side. The frames come from an importer, and
         * the window shows them through a sink (see PreviewSink), as the headset will.
         */
        class StereoPresenterWindow : public VR::UIHost::ImGuiWindowHost
        {
        public:
            StereoPresenterWindow(std::unique_ptr<Stereo::Transport::IFrameImporter> in_importer_ptr,
                                  const std::string&                                 in_title);

            /* The importer of the window, for a sink which is to read the frames (see HeadsetSink). */
            Stereo::Transport::IFrameImporter* get_importer_ptr() const;

            /* Hands the frames to the headset. The headset is then the one which reads them, and the window no longer previews them: what a
             * window which is drawn in step with the screen, and shows each image, does to the pace of the frames is what the headset is not to
             * wait for.
             */
            void set_headset_sink(std::unique_ptr<HeadsetSink> in_sink_ptr);

        protected:
            void draw_contents(VR::UIHost::ContentExtent* inout_extent_ptr) override;

        private:
            /* The window is made as large as the image, if the screen has room for it, so that it is shown as it is. */
            void fit_window_to_image(const uint32_t& in_width,
                                     const uint32_t& in_height);

            static VR::UIHost::WindowOptions get_window_options(const std::string& in_title);

            std::unique_ptr<Stereo::Transport::IFrameImporter> m_importer_ptr;
            std::unique_ptr<HeadsetSink>                       m_headset_sink_ptr; /* after the importer: it goes first */
            bool                                               m_is_styled;
            std::unique_ptr<PreviewSink>                       m_sink_ptr; /* made when the window has its device */
        };
    }
}

#endif /* STEREO_PRESENTER_STEREO_PRESENTER_WINDOW_H */
