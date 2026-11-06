/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_SCREEN_PARALLAX_PLANNER_H
#define STEREO_CORE_SCREEN_PARALLAX_PLANNER_H

#include "analysis/recipe/recipe_applier.h"
#include "analysis/recipe/types.h"
#include <stdint.h>

namespace Stereo
{
    namespace Core
    {
        /* What is to be done to the vertices of a draw that are already in the space of the picture (see ScreenParallax). */
        struct ScreenShiftPlan
        {
            /* How the x of the vertices are moved. */
            enum class Kind
            {
                NONE,       /* as they are                                                              */
                PER_VERTEX, /* each by its depth, with the distances below (the draw is part of the scene) */
                UNIFORM     /* all by the same pixels (the draw is pushed toward the viewer as a whole)    */
            };

            float                       convergence_distance  = 0.0f; /* of PER_VERTEX, in the units of the camera of the application */
            Analysis::Recipe::Eye eye                   = Analysis::Recipe::Eye::LEFT;
            float                       eye_separation        = 0.0f; /* of PER_VERTEX, the same units                              */
            float                       field_of_view_degrees = 0.0f; /* of PER_VERTEX                                              */
            Kind                        kind                  = Kind::NONE;
            float                       picture_width         = 0.0f; /* of PER_VERTEX, in pixels                                   */
            float                       uniform_shift_pixels  = 0.0f; /* of UNIFORM, positive to the right                          */
        };

        /* Decides what is done to a draw of vertices that are in the space of the picture, for an eye. It is the same for an adapter that
         * plays a dump and one that follows an application as it runs: the adapter says what it knows of the draw and of the frame, and
         * moves the vertices the way the plan says, which is the part that depends on the API.
         */
        class ScreenParallaxPlanner
        {
        public:
            /* What the adapter knows of the draw. */
            struct Input
            {
                float                       convergence_distance = 0.0f;  /* of the scene the draw is in; 0: the recipe's own                    */
                uint64_t                    cover_key            = 0;     /* of the draw by its signature and size                                */
                Analysis::Recipe::Eye eye                  = Analysis::Recipe::Eye::LEFT;
                float                       eye_separation       = 0.0f;  /* of the frame, in the units of the camera of the application          */
                bool                        has_extent           = false; /* the adapter knows where the vertices of the draw are (reaches_both_sides)  */
                bool                        is_affected          = false; /* the recipe does not leave the draw alone                             */
                bool                        is_overlay_pushed    = false; /* the person wants the draws that are left alone pushed                */
                bool                        is_presented         = false; /* the draw is made to what is presented                                */
                float                       picture_width        = 0.0f;  /* of the view the vertices are positioned in, in pixels                */
                bool                        reaches_both_sides   = false; /* the vertices of the draw reach from the left edge of the view to the right one */
                float                       separation_limit     = 0.0f;  /* how far apart the eyes may be for the scene; 0: no limit             */
            };

            /* A draw that covers the whole of the view (a flash, a fade) is never moved sideways: it would leave a strip of the picture bare. The
             * recipe knows it by the key of the draw (its signature and size), which draws that do not cover the view may have too (a piece of the sky
             * which is made as a full-screen quad is): when the adapter knows where the vertices are, the draw has to reach both sides of the view as well.
             * A draw which the recipe affects is moved by its depth, if the recipe has screen parallax. One that it leaves alone is pushed,
             * if it is made to what is presented and the person wants it, by Comfort::overlay_parallax.
             */
            static ScreenShiftPlan plan(const Analysis::Recipe::RecipeApplier& in_applier,
                                        const Input&                           in_input);
        };
    }
}

#endif /* STEREO_CORE_SCREEN_PARALLAX_PLANNER_H */
