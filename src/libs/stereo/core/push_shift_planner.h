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
            /* Where a pushed draw is made, in pixels: the left edge of its viewport and its width. */
            struct PushedViewport
            {
                int32_t width = 0;
                int32_t x     = 0;
            };

            /* How far apart the eyes move an affected draw in a pass of pixels, as a part of the picture width: the separation (held to the scene
             * limit) over the width, 0 if they do not move it. A 3D draw is moved by its depth (see StereoCore::get_eye_parallax()).
             */
            static float get_screen_layer_parallax(const bool&  in_is_affected,
                                                   const float& in_picture_width_pixels,
                                                   const float& in_eye_separation,
                                                   const float& in_separation_limit);

            /* The viewport of a draw pushed by the shift (positive to the right, taken toward zero to whole pixels). A side-reaching overlay is
             * widened by the shift on each side, so no edge is bare.
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

            /* The same, for a draw the eyes move apart by the given part of the picture width (negative behind the screen, 0 if they do not move
             * it).
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
