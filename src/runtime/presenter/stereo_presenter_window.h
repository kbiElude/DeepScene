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
#include "utils/ui/imgui_player_window_base.h"
#include <memory>
#include <string>

namespace Stereo
{
    namespace Presenter
    {
        /* The window that shows what the converter of an application passes on: the newest image, as large as the window allows with its proportions
         * kept. A stereo image has the two eyes side by side. The frames come from an importer, and the window shows them through a sink
         * (see PreviewSink), as the headset does through its own sink (see HeadsetSink).
         */
        class StereoPresenterWindow : public Utils::UI::ImGuiPlayerWindowBase
        {
        public:
            StereoPresenterWindow(std::unique_ptr<Stereo::Transport::IFrameImporter> in_importer_ptr,
                                  const std::string&                                 in_title);

            /* The importer of the window, for a sink that is to read the frames (see HeadsetSink). */
            Stereo::Transport::IFrameImporter* get_importer_ptr() const;

            /* Hands the frames to the headset. The headset sink then reads the frames, and the window no longer previews them. A window that is drawn in
             * step with the screen and shows every image would set the pace of the frames, and the headset must not wait for it.
             */
            void set_headset_sink(std::unique_ptr<HeadsetSink> in_sink_ptr);

        protected:
            void draw_contents(Utils::UI::ContentExtent* inout_extent_ptr) override;

        private:
            /* Resizes the window to the size of the image if the screen has room for it, so that the image is shown at its own size. */
            void fit_window_to_image(const uint32_t& in_width,
                                     const uint32_t& in_height);

            static Utils::UI::WindowOptions get_window_options(const std::string& in_title);

            std::unique_ptr<Stereo::Transport::IFrameImporter> m_importer_ptr;
            std::unique_ptr<HeadsetSink>                       m_headset_sink_ptr; /* declared after the importer so that it is destroyed first */
            bool                                               m_is_styled;
            std::unique_ptr<PreviewSink>                       m_sink_ptr; /* created when the window has its device */
        };
    }
}

#endif /* STEREO_PRESENTER_STEREO_PRESENTER_WINDOW_H */
