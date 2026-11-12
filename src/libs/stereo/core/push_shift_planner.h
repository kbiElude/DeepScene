/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_PUSH_SHIFT_PLANNER_H
#define STEREO_CORE_PUSH_SHIFT_PLANNER_H

#include "analysis/recipe/recipe_applier.h"
#include "analysis/recipe/types.h"

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
