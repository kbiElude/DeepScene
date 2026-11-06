/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_HASH_SINK_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_HASH_SINK_H

#include "tools/stereo_replay/engine_dll/i_frame_image_sink.h"
#include "utils/images/types.h"
#include <map>
#include <set>
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
                const Utils::Images::Image& get_image(const ImageKey& in_key) const;

                void on_frame_image(const uint32_t&                              in_n_frame,
                                    const uint32_t&                              in_n_execution,
                                    const Stereo::Replay::EngineDLL::FrameImage& in_image) override;

                /* Only the images of these frames are kept (the hashes are of all of them); with none, the images of all, if the sink keeps images. */
                void set_frames_to_keep(const std::set<uint32_t>& in_frame_set);

                bool wants_frame_image(const uint32_t& in_n_frame,
                                       const uint32_t& in_n_execution) const override;

            private:
                std::set<uint32_t>                                     m_frame_to_keep_set;
                std::map<ImageKey, uint64_t>                           m_hash_map;
                std::map<ImageKey, Utils::Images::Image> m_image_map;
                bool                                                   m_keeps_images;
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_HASH_SINK_H */
