/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_SCREEN_PARALLAX_PLANNER_H
#define STEREO_CORE_SCREEN_PARALLAX_PLANNER_H

#include "analysis/recipe/recipe_applier.h"
#include "analysis/recipe/types.h"
#include "stereo/core/types.h"
#include <stdint.h>

namespace Stereo
{
    namespace Core
    {
        /* What to do to the vertices of a draw that are already in the space of the picture (see ScreenSpaceVertexShifter). */
        struct ScreenShiftPlan
        {
            float                 convergence_distance  = 0.0f; /* used by PER_VERTEX, in the application's camera units         */
            Analysis::Recipe::Eye eye                   = Analysis::Recipe::Eye::LEFT;
            float                 eye_separation        = 0.0f; /* used by PER_VERTEX, in the same units                         */
            float                 field_of_view_degrees = 0.0f; /* used by PER_VERTEX                                            */
            ScreenShiftKind       kind                  = ScreenShiftKind::NONE;
            float                 picture_width         = 0.0f; /* used by PER_VERTEX, in pixels                                 */
            float                 uniform_shift_pixels  = 0.0f; /* used by UNIFORM, positive to the right                        */
        };

        /* Decides what to do to a draw whose vertices are in the space of the picture, for one eye. The same logic serves an adapter that
         * plays a dump and an adapter that follows a running application. The adapter reports what it knows about the draw and the frame,
         * and then moves the vertices as the plan says. That step depends on the API.
         */
        class ScreenParallaxPlanner
        {
        public:
            /* What the adapter knows about the draw. */
            struct Input
            {
                float                 convergence_distance = 0.0f;  /* of the scene the draw is in; 0: use the recipe's own                           */
                uint64_t              cover_key            = 0;     /* identifies the draw by its signature and size                                  */
                Analysis::Recipe::Eye eye                  = Analysis::Recipe::Eye::LEFT;
                float                 eye_separation       = 0.0f;  /* of the frame, in the application's camera units                                */
                bool                  has_extent           = false; /* the adapter knows where the draw's vertices are (see reaches_both_sides)       */
                bool                  is_affected          = false; /* the recipe affects the draw                                                    */
                bool                  is_overlay_pushed    = false; /* the person wants the draws that the recipe leaves alone to be pushed           */
                bool                  is_presented         = false; /* the draw goes to the target that is presented                                  */
                float                 picture_width        = 0.0f;  /* of the view the vertices are positioned in, in pixels                          */
                bool                  reaches_both_sides   = false; /* the draw's vertices reach from the left edge of the view to the right edge     */
                float                 separation_limit     = 0.0f;  /* the largest eye separation the scene allows; 0: no limit                       */
            };

            /* A draw that covers the whole view (a flash, a fade) is not moved sideways, because that would leave a strip of the picture bare.
             * The recipe recognizes such a draw by its key (its signature and size). Draws that do not cover the view can have the same key
             * (a piece of sky drawn as a full-screen quad, for example). So if the adapter does not know where the vertices are, a draw with
             * such a key is left alone. If it does know (has_extent), the draw is left alone only if its vertices also reach both sides of
             * the view. A draw that the recipe affects is moved by its depth, if the recipe has screen parallax. A draw that the recipe
             * leaves alone is pushed by Comfort::overlay_parallax if it goes to the presented target and the person wants it.
             */
            static ScreenShiftPlan plan(const Analysis::Recipe::RecipeApplier& in_applier,
                                        const Input&                           in_input);
        };
    }
}

#endif /* STEREO_CORE_SCREEN_PARALLAX_PLANNER_H */
