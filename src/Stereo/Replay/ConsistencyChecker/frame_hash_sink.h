/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_HASH_SINK_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_HASH_SINK_H

#include "Stereo/Replay/EngineDLL/i_frame_image_sink.h"
#include "Stereo/Replay/FrameImages/types.h"
#include <map>
#include <utility>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            /* Keeps a hash of the back buffer after each execution of each frame, and the images too if asked to. */
            class FrameHashSink : public Stereo::Replay::EngineDLL::IFrameImageSink
            {
            public:
                /* A frame, and the execution of it that an image is of. */
                typedef std::pair<uint32_t, uint32_t> ImageKey;

                explicit FrameHashSink(const bool& in_keeps_images);

                const std::map<ImageKey, uint64_t>& get_hash_map() const;

                /* Empty if there is no such image, or it was not kept. */
                const Stereo::Replay::FrameImages::Image& get_image(const ImageKey& in_key) const;

                void on_frame_image(const uint32_t&                              in_n_frame,
                                    const uint32_t&                              in_n_execution,
                                    const Stereo::Replay::EngineDLL::FrameImage& in_image) override;

                bool wants_frame_image(const uint32_t& in_n_frame,
                                       const uint32_t& in_n_execution) const override;

            private:
                std::map<ImageKey, uint64_t>                           m_hash_map;
                std::map<ImageKey, Stereo::Replay::FrameImages::Image> m_image_map;
                bool                                                   m_keeps_images;
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_HASH_SINK_H */
