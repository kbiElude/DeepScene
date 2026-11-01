/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/hasher.h"
#include "frame_hash_sink.h"

Replay::StereoReplayConsistencyChecker::FrameHashSink::FrameHashSink(const bool& in_keeps_images)
    :m_keeps_images(in_keeps_images)
{
    /* Stub */
}

const std::map<Replay::StereoReplayConsistencyChecker::FrameHashSink::ImageKey, uint64_t>& Replay::StereoReplayConsistencyChecker::FrameHashSink::get_hash_map() const
{
    return m_hash_map;
}

const Replay::StereoReplayConsistencyChecker::FrameHashSink::Image& Replay::StereoReplayConsistencyChecker::FrameHashSink::get_image(const ImageKey& in_key) const
{
    static const Image empty_image;

    const auto image_iterator = m_image_map.find(in_key);

    return (image_iterator != m_image_map.end() ) ? image_iterator->second
                                                  : empty_image;
}

void Replay::StereoReplayConsistencyChecker::FrameHashSink::on_frame_image(const uint32_t&                            in_n_frame,
                                                                           const uint32_t&                            in_n_execution,
                                                                           const Replay::StereoReplayEngineDLL::FrameImage& in_image)
{
    constexpr size_t N_BYTES_PER_PIXEL = 4;

    const size_t   n_bytes = static_cast<size_t>(in_image.width) * in_image.height * N_BYTES_PER_PIXEL;
    const ImageKey key(in_n_frame,
                       in_n_execution);

    m_hash_map[key] = Analysis::APIDumpAnalysis::Hasher::hash_bytes(in_image.pixel_ptr,
                                                                    n_bytes);

    if (m_keeps_images)
    {
        Image image;

        image.height = in_image.height;
        image.width  = in_image.width;

        image.pixel_u8_vec.assign(in_image.pixel_ptr,
                                  in_image.pixel_ptr + n_bytes);

        m_image_map[key] = std::move(image);
    }
}

bool Replay::StereoReplayConsistencyChecker::FrameHashSink::wants_frame_image(const uint32_t& in_n_frame,
                                                                              const uint32_t& in_n_execution) const
{
    (void) in_n_frame;
    (void) in_n_execution;

    return true;
}
