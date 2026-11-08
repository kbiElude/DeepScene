/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_textures/texture_level_content.h"
#include <algorithm>
#include <tuple>

Analysis::APIDumpTextures::LevelContent Analysis::APIDumpTextures::TextureLevelContent::find(const TextureTimeline&                       in_timeline,
                                                                                             const Analysis::APIDumpAnalysis::ResourceId& in_resource_id,
                                                                                             const uint32_t&                              in_mip_level,
                                                                                             const LevelCut&                              in_cut)
{
    LayerMap     layer_map;
    LevelContent result;

    for (uint32_t n_event = 0;
                  n_event < in_timeline.event_vec.size();
                ++n_event)
    {
        const TextureEvent& event = in_timeline.event_vec.at(n_event);

        /* The events are in the order of the calls: none after the cut is counted. */
        if (event.frame_index > in_cut.last_frame_index ||
            event.call_index  > in_cut.last_call_index)
        {
            break;
        }

        if (event.destination.resource_id != in_resource_id ||
            event.destination.mip_level   != in_mip_level)
        {
            continue;
        }

        if (event.kind == TextureEventKind::SNAPSHOT)
        {
            result.has_snapshot         = true;
            result.snapshot_event_index = n_event;

            continue;
        }

        const bool is_another_level = (result.level_depth_or_layer_count != event.level_depth_or_layer_count ||
                                       result.level_format               != event.level_format               ||
                                       result.level_height               != event.level_height               ||
                                       result.level_width                != event.level_width);

        /* An upload with no pixels (a NULL pointer) gives a size and writes nothing. */
        const bool is_sizing_only   = (event.kind       == TextureEventKind::UPLOAD &&
                                       event.blob_index == NO_BLOB);
        const bool covers_level     = (event.destination.layer == 0                                  &&
                                       event.height            == event.level_height                 &&
                                       event.layer_count       == event.level_depth_or_layer_count   &&
                                       event.width             == event.level_width                  &&
                                       event.x                 == 0                                  &&
                                       event.y                 == 0);

        /* A level that is given another size or format is a new level: what it held is gone. So is one that is given its size again with no pixels: what it holds
         * is not known. A part of a level that is given no pixels changes nothing.
         */
        const bool is_emptied  = (is_sizing_only && covers_level);
        const bool is_replaced = (result.has_level && is_another_level);

        if (is_emptied ||
            is_replaced)
        {
            layer_map.clear();
        }

        result.has_level                   = true;
        result.level_depth_or_layer_count  = event.level_depth_or_layer_count;
        result.level_format                = event.level_format;
        result.level_height                = event.level_height;
        result.level_width                 = event.level_width;

        if (is_sizing_only)
        {
            continue;
        }

        for (uint32_t n_layer = event.destination.layer;
                      n_layer < event.destination.layer + event.layer_count;
                    ++n_layer)
        {
            Rectangle rectangle;

            rectangle.event_index = n_event;
            rectangle.height      = event.height;
            rectangle.width       = event.width;
            rectangle.x           = event.x;
            rectangle.y           = event.y;

            paint(rectangle,
                 &layer_map[n_layer]);
        }
    }

    make_pieces(layer_map,
               &result.piece_vec);

    return result;
}

void Analysis::APIDumpTextures::TextureLevelContent::make_pieces(const LayerMap&          in_layer_map,
                                                                 std::vector<LevelPiece>* out_piece_vec_ptr)
{
    typedef std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t> RectangleKey;

    const auto get_key_func = [](const Rectangle& in_rectangle)
    {
        return RectangleKey(in_rectangle.event_index,
                            in_rectangle.y,
                            in_rectangle.x,
                            in_rectangle.height,
                            in_rectangle.width);
    };

    std::vector<RectangleKey> previous_key_vec;
    uint32_t                  previous_layer          = 0;
    size_t                    previous_first_piece    = 0;

    for (const auto& layer_pair : in_layer_map)
    {
        std::vector<RectangleKey> key_vec;

        for (const auto& rectangle : layer_pair.second)
        {
            key_vec.push_back(get_key_func(rectangle) );
        }

        std::sort(key_vec.begin(),
                  key_vec.end  () );

        if (key_vec.empty() )
        {
            continue;
        }

        /* The next layer that holds the same rectangles of the same events is a part of the run. */
        if (previous_key_vec.empty() == false      &&
            layer_pair.first == previous_layer + 1 &&
            key_vec          == previous_key_vec)
        {
            for (size_t n_piece = previous_first_piece;
                        n_piece < out_piece_vec_ptr->size();
                      ++n_piece)
            {
                out_piece_vec_ptr->at(n_piece).layer_count++;
            }
        }
        else
        {
            previous_first_piece = out_piece_vec_ptr->size();

            for (const auto& key : key_vec)
            {
                LevelPiece piece;

                piece.event_index = std::get<0>(key);
                piece.first_layer = layer_pair.first;
                piece.height      = std::get<3>(key);
                piece.width       = std::get<4>(key);
                piece.x           = std::get<2>(key);
                piece.y           = std::get<1>(key);

                out_piece_vec_ptr->push_back(piece);
            }
        }

        previous_key_vec = key_vec;
        previous_layer   = layer_pair.first;
    }
}

void Analysis::APIDumpTextures::TextureLevelContent::paint(const Rectangle&        in_rectangle,
                                                           std::vector<Rectangle>* inout_rectangle_vec_ptr)
{
    std::vector<Rectangle> kept_vec;

    const uint32_t new_bottom = in_rectangle.y + in_rectangle.height;
    const uint32_t new_right  = in_rectangle.x + in_rectangle.width;

    for (const auto& old_rectangle : *inout_rectangle_vec_ptr)
    {
        const uint32_t old_bottom = old_rectangle.y + old_rectangle.height;
        const uint32_t old_right  = old_rectangle.x + old_rectangle.width;

        const bool is_apart = (new_right  <= old_rectangle.x ||
                               old_right  <= in_rectangle.x  ||
                               new_bottom <= old_rectangle.y ||
                               old_bottom <= in_rectangle.y);

        if (is_apart)
        {
            kept_vec.push_back(old_rectangle);
        }
        else
        {
            /* What the new rectangle leaves of the old one: the rows above it, the rows below it, and the parts to the left and right of it in the rows between. */
            const uint32_t middle_bottom = (std::min)(old_bottom,
                                                      new_bottom);
            const uint32_t middle_top    = (std::max)(old_rectangle.y,
                                                      in_rectangle.y);

            if (old_rectangle.y < middle_top)
            {
                Rectangle above = old_rectangle;

                above.height = middle_top - old_rectangle.y;

                kept_vec.push_back(above);
            }

            if (middle_bottom < old_bottom)
            {
                Rectangle below = old_rectangle;

                below.height = old_bottom - middle_bottom;
                below.y      = middle_bottom;

                kept_vec.push_back(below);
            }

            if (old_rectangle.x < in_rectangle.x)
            {
                Rectangle left = old_rectangle;

                left.height = middle_bottom - middle_top;
                left.width  = in_rectangle.x - old_rectangle.x;
                left.y      = middle_top;

                kept_vec.push_back(left);
            }

            if (new_right < old_right)
            {
                Rectangle right = old_rectangle;

                right.height = middle_bottom - middle_top;
                right.width  = old_right - new_right;
                right.x      = new_right;
                right.y      = middle_top;

                kept_vec.push_back(right);
            }
        }
    }

    kept_vec.push_back(in_rectangle);

    *inout_rectangle_vec_ptr = std::move(kept_vec);
}
