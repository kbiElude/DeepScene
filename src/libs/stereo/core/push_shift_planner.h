/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_PUSH_SHIFT_PLANNER_H
#define STEREO_CORE_PUSH_SHIFT_PLANNER_H

#include "analysis/recipe/recipe_applier.h"
#include "analysis/recipe/types.h"
#include <stdint.h>

namespace Stereo
{
    namespace Core
    {
        /* Computes how far a pushed draw (one that the recipe lists in Recipe::pushed) is moved sideways for one eye, whatever its projection
         * is. The adapter moves the picture of the draw by that many pixels (in OpenGL: the origin of the viewport), to the right for the left
         * eye. The two pictures of the draw are then Comfort::overlay_parallax apart, as a fraction of the picture width, and the draw stands
         * in front of what it is drawn over.
         */
        class PushShiftPlanner
        {
        public:
            /* Where a pushed draw is made, in pixels: the left edge of its viewport and its width. */
            struct PushedViewport
            {
                int32_t width = 0;
                int32_t x     = 0;
            };

            /* The parallax between the eyes for an affected draw in a pass whose units are pixels, as a fraction of the picture width. It is
             * the eye separation (capped by the scene limit) divided by the picture width. It is 0 if the draw is not affected or the width is
             * not above 0. A 3D draw is moved by its depth instead (see RecipeStereoPlanner::get_eye_parallax()).
             */
            static float get_screen_layer_parallax(const bool&  in_is_affected,
                                                   const float& in_picture_width_pixels,
                                                   const float& in_eye_separation,
                                                   const float& in_separation_limit);

            /* Returns the viewport of a draw pushed by in_shift_pixels (positive to the right, truncated toward zero to whole pixels). A
             * side-reaching overlay is widened by the shift on each side, so that no edge is left bare.
             */
            static PushedViewport make_viewport(const int32_t& in_x,
                                                const int32_t& in_width,
                                                const float&   in_shift_pixels,
                                                const bool&    in_is_side_reaching);

            /* Returns the shift of the draw in pixels, positive to the right. It is 0 if in_parallax is not above 0 or the picture width is not
             * above 0. in_parallax is the parallax to push by (normally RecipeApplier::get_cover_push_parallax(): Comfort::overlay_parallax,
             * or Comfort::part_parallax for a part that protrudes). If the recipe sets Comfort::max_near_parallax, the push is limited to what
             * remains of it after the parallax of the draw's layer. An affected draw is already moved apart by the eyes. For a 2D pass whose
             * units are pixels that parallax is the eye separation (capped by the scene limit, see RecipeApplier::get_scene_max_separation)
             * divided by the picture width. The push is added to it, so that the two together stay within what the recipe allows.
             */
            static float plan(const Analysis::Recipe::RecipeApplier& in_applier,
                              const Analysis::Recipe::Eye&           in_eye,
                              const float&                           in_parallax,
                              const bool&                            in_is_affected,
                              const float&                           in_picture_width_pixels,
                              const float&                           in_eye_separation,
                              const float&                           in_separation_limit);

            /* The same, for a draw that the eyes already move apart by in_layer_parallax, a fraction of the picture width (negative behind the
             * screen, 0 if the eyes do not move it).
             */
            static float plan_with_layer(const Analysis::Recipe::RecipeApplier& in_applier,
                                         const Analysis::Recipe::Eye&           in_eye,
                                         const float&                           in_parallax,
                                         const float&                           in_layer_parallax,
                                         const float&                           in_picture_width_pixels);
        };
    }
}

#endif /* STEREO_CORE_PUSH_SHIFT_PLANNER_H */
