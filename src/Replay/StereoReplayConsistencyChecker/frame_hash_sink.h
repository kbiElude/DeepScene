/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYCONSISTENCYCHECKER_FRAME_HASH_SINK_H
#define STEREOREPLAYCONSISTENCYCHECKER_FRAME_HASH_SINK_H

#include "Replay/StereoReplayEngineDLL/i_frame_image_sink.h"
#include <map>
#include <utility>
#include <vector>

namespace Replay
{
    namespace StereoReplayConsistencyChecker
    {
        /* Keeps a hash of the back buffer after each execution of each frame, and the images too if asked to. */
        class FrameHashSink : public Replay::StereoReplayEngineDLL::IFrameImageSink
        {
        public:
            /* An image as 8-bit B, G, R and A (always 255) for each pixel, row after row. */
            struct Image
            {
                uint32_t             height = 0;
                std::vector<uint8_t> pixel_u8_vec;
                uint32_t             width  = 0;
            };

            /* A frame, and the execution of it that an image is of. */
            typedef std::pair<uint32_t, uint32_t> ImageKey;

            explicit FrameHashSink(const bool& in_keeps_images);

            const std::map<ImageKey, uint64_t>& get_hash_map() const;

            /* Empty if there is no such image, or it was not kept. */
            const Image& get_image(const ImageKey& in_key) const;

            void on_frame_image(const uint32_t&                            in_n_frame,
                                const uint32_t&                            in_n_execution,
                                const Replay::StereoReplayEngineDLL::FrameImage& in_image) override;

            bool wants_frame_image(const uint32_t& in_n_frame,
                                   const uint32_t& in_n_execution) const override;

        private:
            std::map<ImageKey, uint64_t> m_hash_map;
            std::map<ImageKey, Image>    m_image_map;
            bool                         m_keeps_images;
        };
    }
}

#endif /* STEREOREPLAYCONSISTENCYCHECKER_FRAME_HASH_SINK_H */
