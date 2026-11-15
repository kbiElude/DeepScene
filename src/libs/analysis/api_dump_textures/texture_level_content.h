/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Computes what one level of a texture holds at a point of a workload, from the events of a .textures file. The events that write the level are
 * laid over each other in call order, and what remains visible of each event is a piece. A write that gives the level a different size or format
 * starts the level again, as specifying the level anew does.
 */
#ifndef APIDUMPTEXTURES_TEXTURE_LEVEL_CONTENT_H
#define APIDUMPTEXTURES_TEXTURE_LEVEL_CONTENT_H

#include "analysis/api_dump_textures/types.h"
#include <map>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        class TextureLevelContent
        {
        public:
            /* Returns the content of one level of a resource, as the events up to the cut leave it. */
            static LevelContent find(const TextureTimeline&                       in_timeline,
                                     const Analysis::APIDumpAnalysis::ResourceId& in_resource_id,
                                     const uint32_t&                              in_mip_level,
                                     const LevelCut&                              in_cut);

        private:
            /* A rectangle of one layer that an event wrote, in texels. */
            struct Rectangle
            {
                uint32_t event_index = 0;
                uint32_t height      = 0;
                uint32_t width       = 0;
                uint32_t x           = 0;
                uint32_t y           = 0;
            };

            /* The rectangles of each layer, by layer number. */
            typedef std::map<uint32_t, std::vector<Rectangle> > LayerMap;

            /* Converts the rectangles into pieces. Consecutive layers that hold the same rectangles from the same events become one piece with a
             * layer count.
             */
            static void make_pieces(const LayerMap&          in_layer_map,
                                    std::vector<LevelPiece>* out_piece_vec_ptr);

            /* Lays a rectangle over the rectangles of a layer. The parts of the old rectangles that stay visible are kept, and the new rectangle
             * is added.
             */
            static void paint(const Rectangle&        in_rectangle,
                              std::vector<Rectangle>* inout_rectangle_vec_ptr);
        };
    }
}

#endif /* APIDUMPTEXTURES_TEXTURE_LEVEL_CONTENT_H */
