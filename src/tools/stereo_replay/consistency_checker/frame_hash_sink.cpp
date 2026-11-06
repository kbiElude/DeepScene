/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/hasher.h"
#include "frame_hash_sink.h"

Stereo::Replay::ConsistencyChecker::FrameHashSink::FrameHashSink(const bool& in_keeps_images)
    :m_keeps_images(in_keeps_images)
{
    /* Stub */
}

const std::map<Stereo::Replay::ConsistencyChecker::FrameHashSink::ImageKey, uint64_t>& Stereo::Replay::ConsistencyChecker::FrameHashSink::get_hash_map() const
{
    return m_hash_map;
}

const Stereo::Replay::FrameImages::Image& Stereo::Replay::ConsistencyChecker::FrameHashSink::get_image(const ImageKey& in_key) const
{
    static const Stereo::Replay::FrameImages::Image empty_image;

    const auto image_iterator = m_image_map.find(in_key);

    return (image_iterator != m_image_map.end() ) ? image_iterator->second
                                                  : empty_image;
}

void Stereo::Replay::ConsistencyChecker::FrameHashSink::on_frame_image(const uint32_t&                              in_n_frame,
                                                                       const uint32_t&                              in_n_execution,
                                                                       const Stereo::Replay::EngineDLL::FrameImage& in_image)
{
    constexpr size_t N_BYTES_PER_PIXEL = 4;

    const size_t   n_bytes = static_cast<size_t>(in_image.width) * in_image.height * N_BYTES_PER_PIXEL;
    const ImageKey key(in_n_frame,
                       in_n_execution);

    m_hash_map[key] = Analysis::APIDumpAnalysis::Hasher::hash_bytes(in_image.pixel_ptr,
                                                                    n_bytes);

    if (m_keeps_images                                                   &&
        (m_frame_to_keep_set.empty()                                     ||
         m_frame_to_keep_set.find(in_n_frame) != m_frame_to_keep_set.end() ) )
    {
        Stereo::Replay::FrameImages::Image image;

        image.height = in_image.height;
        image.width  = in_image.width;

        image.pixel_u8_vec.assign(in_image.pixel_ptr,
                                  in_image.pixel_ptr + n_bytes);

        m_image_map[key] = std::move(image);
    }
}

void Stereo::Replay::ConsistencyChecker::FrameHashSink::set_frames_to_keep(const std::set<uint32_t>& in_frame_set)
{
    m_frame_to_keep_set = in_frame_set;
}

bool Stereo::Replay::ConsistencyChecker::FrameHashSink::wants_frame_image(const uint32_t& in_n_frame,
                                                                          const uint32_t& in_n_execution) const
{
    (void) in_n_frame;
    (void) in_n_execution;

    return true;
}
