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
        /* How far a draw that the recipe names as pushed (Recipe::pushed) is moved sideways for an eye, whatever its projection is: the adapter moves the
         * picture of the draw by that many pixels (OpenGL: the origin of the viewport), to the right for the left eye, so that the two pictures of it
         * are apart by Comfort::overlay_parallax, as a part of the width of the picture, and it stands in front of what it is drawn over.
         */
        class PushShiftPlanner
        {
        public:
            /* Where a draw that is pushed is made, in pixels: the left edge of its viewport and its width. */
            struct PushedViewport
            {
                int32_t width = 0;
                int32_t x     = 0;
            };

            /* The viewport that a draw is made with when it is pushed by the shift (positive to the right), for the viewport that it is made with otherwise: moved by the shift, which is
             * taken toward zero to whole pixels. A draw that reaches from one side of the picture to the other (a 2D overlay, see RecipeApplier::is_side_reaching()) would leave the
             * side that it moves away from bare, so its picture is made as much wider as the shift, on each side: its edge that the shift would expose stays where it was, and the
             * other one goes past the edge of the view. The viewport of an adapter is as wide as the view in most cases, so this is the same for every API.
             */
            static PushedViewport make_viewport(const int32_t& in_x,
                                                const int32_t& in_width,
                                                const float&   in_shift_pixels,
                                                const bool&    in_is_side_reaching);

            /* The shift of the draw in pixels, positive to the right: 0 if the recipe has no overlay parallax, or no width is given. The parallax is
             * the one given (RecipeApplier::get_cover_push_parallax(): Comfort::overlay_parallax, or Comfort::part_parallax for a part that protrudes), but no more than what is left of the nearest parallax the recipe allows (Comfort::max_near_parallax, if it has one) when
             * the draw has what the layer it is in has: a draw that the recipe affects is already moved apart by the eyes, which for a 2D pass whose units are
             * pixels is the separation (held to the limit of the scene, if there is one; see RecipeApplier::get_scene_max_separation) over the width of the
             * picture, and the push is added to that, so that the two do not come to more than the recipe allows.
             */
            static float plan(const Analysis::Recipe::RecipeApplier& in_applier,
                              const Analysis::Recipe::Eye&           in_eye,
                              const float&                           in_parallax,
                              const bool&                            in_is_affected,
                              const float&                           in_picture_width_pixels,
                              const float&                           in_eye_separation,
                              const float&                           in_separation_limit);
        };
    }
}

#endif /* STEREO_CORE_PUSH_SHIFT_PLANNER_H */
