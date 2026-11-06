/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The decisions of a converter which plays an application as it runs and renders each eye of what the application draws, whatever
 * the API is. An adapter for an API tells it what the application does (it sets a matrix, it draws) and does what it answers (set
 * these matrices, draw this once or for each eye); it knows nothing of the application, only of the recipe, which must be baked
 * (see RecipeBake).
 *
 * What a converter can know when a draw is issued is less than what an offline replay knows: the pass the draw belongs to is not
 * known yet, so a draw is told by its signature and the view matrix (see Recipe::draw_rule_vec), and a rule of the recipe that is
 * about a pass or a frame shape cannot be followed. Where an application needs more than that, the converter fails at once, through
 * the function it was given, instead of producing images that are wrong.
 *
 * Not thread-safe: an adapter calls it from the thread that calls the device.
 */
#ifndef STEREO_CORE_STEREO_CORE_H
#define STEREO_CORE_STEREO_CORE_H

#include "analysis/recipe/recipe_applier.h"
#include "common/api_function_argument.h"
#include "stereo/core/convergence_model.h"
#include "stereo/core/matrix_slots.h"
#include "stereo/core/screen_parallax_planner.h"
#include "stereo/core/types.h"
#include <array>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Core
    {
        class StereoCore
        {
        public:
            /* Fails, through the function, if the recipe is not baked or has rules that cannot be followed as the application runs. */
            StereoCore(const Analysis::Recipe::Recipe& in_recipe,
                       const FailFunc&                 in_fail_func);

            /* The matrices to put in the device before a draw of the class, for the eye, that are not in it yet: for a draw which is
             * affected, those the recipe changes are made for the eye, for any other the application's (a draw that is made to what the
             * application presents, and is not affected, is pushed toward the viewer if the recipe says so). What was returned is taken to
             * be put in. Not for a skipped draw, which is not played.
             *
             * The changes are written to the first entries of the vector, which the caller keeps from one call to the next so that
             * the room of its entries is used again; the entries after them are not meant. Returns how many there are.
             */
            uint32_t bind_matrices(const Analysis::Recipe::DrawClass& in_draw_class,
                                   const Analysis::Recipe::Eye&       in_eye,
                                   std::vector<MatrixChange>*         inout_change_vec_ptr,
                                   const bool&                        in_is_final_target = false);

            /* What the recipe does to a draw by its signature and the view matrix the application has now. A draw that covers the whole of the
             * view (a flash) is not pushed toward the viewer: the picture of it would be moved, and leave a strip of the screen bare.
             */
            Analysis::Recipe::DrawClass classify_draw(const uint64_t& in_signature_key,
                                                      const uint64_t& in_cover_key);

            /* Forgets the matrices kept, for when the device has other ones than the converter knows of (it was reset). */
            void forget_matrices();

            /* What was set (see set_convergence_distance()); 0 if the recipe's distances are in use. */
            float                        get_convergence_distance() const;
            float                        get_depth_strength      () const;
            float                        get_eye_separation      () const;
            float                        get_ipd_scale           () const;
            bool                         get_is_overlay_pushed   () const;
            uint64_t                     get_n_unknown_draws     () const;
            uint64_t                     get_n_unknown_frames    () const;
            const std::array<float, 16>& get_view_matrix         () const;

            /* The frame that the application has just presented was of this shape (see CompletedFrame::shape_key). If the recipe has a
             * distance at which the eyes converge for it (see Recipe::scene_convergence_vec), the draws from now on converge there, by
             * easing toward it over some frames, which keeps a scene that is made of frames of slightly different shapes from pulsing;
             * the first one is taken at once. A shape the recipe does not know leaves the distance, and the limit of how far apart the eyes
             * may be, as they are.
             */
            void on_frame_completed(const uint64_t& in_shape_key);

            /* A call of the application that sets a matrix (the arguments are those of the call, the matrix is the one in them). Returns
             * true if a rule of the recipe is about it: the converter keeps the matrix and puts it in the device when a draw needs it
             * (see bind_matrices()), so the caller does not execute the call. Otherwise the caller does, and any matrix kept for the same
             * call is forgotten.
             */
            bool on_matrix_call(const APIInterceptor::APIFunction&                      in_function,
                                const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                const std::array<float, 16>&                            in_matrix);

            /* The application set the view matrix, which the rules of draws may be about. Besides on_matrix_call(), if that is a call. */
            void on_view_matrix_set(const std::array<float, 16>& in_matrix);

            /* What is to be done to the vertices of the draw that classify_draw() last classified, for the eye, if they are in the space of the
             * picture (see ScreenParallaxPlanner): by the distances the core has now, which are the ones of the scene and the person. The
             * picture width is that of the viewport the draw is made with, and the draw is presented if it is made to what the application
             * presents.
             */
            ScreenShiftPlan plan_screen_shift(const Analysis::Recipe::Eye& in_eye,
                                              const bool&                  in_has_extent,
                                              const bool&                  in_is_presented,
                                              const float&                 in_picture_width,
                                              const bool&                  in_reaches_both_sides) const;

            /* Fails, through the function, if the condition does not hold: for the places where an adapter finds that the application
             * does something that only holding the calls of a pass back could convert right.
             */
            void require_fast_path(const bool&        in_is_ok,
                                   const std::string& in_reason) const;

            /* Fails, through the function, with the reason alone if the condition does not hold: for what an adapter finds that the
             * converter cannot do for the application, whatever the way (the images are larger than they can be passed on, say).
             */
            void require_supported(const bool&        in_is_ok,
                                   const std::string& in_reason) const;

            /* The distance at which the eyes converge, in the units of the matrices; what is there is at the same place in both eyes, for
             * every draw. Zero, which is what it is until it is set, leaves it to the recipe: the distance of the scene (see
             * on_frame_completed()), and, until a scene is known, that of the signature of the draw (see Recipe::convergence_vec); with
             * neither, no convergence.
             */
            void set_convergence_distance(const float& in_convergence_distance);

            /* How strong the 3D is: 1 is the natural one, 0 is none, 2 is twice that. Above 0, the distance between the eyes is not the
             * one set by set_eye_separation(), but follows the distance at which the eyes converge for each draw (see
             * set_convergence_distance()): a fixed part of it (a thirtieth, as a rule of thumb has the distance between the cameras of a
             * stereo shot), times the strength, so that the 3D is as strong in a corridor as on a hill, which a distance in the units
             * of the camera of the application cannot be. For a draw with no distance to converge at, the one that was set is used.
             */
            void set_depth_strength(const float& in_depth_strength);

            /* The distance between the eyes, in the units of the matrices. */
            void set_eye_separation(const float& in_eye_separation);

            /* How the eyes of the person who looks compare with the ones that the separation is made for (see ConvergenceModel::set_ipd_scale()). */
            void set_ipd_scale(const float& in_ipd_scale);

            /* Whether the draws that the recipe leaves alone and are made to what is presented are pushed toward the viewer, by what the recipe
             * says (Comfort::overlay_parallax; none if it says 0). Off until it is asked for: the recipe says how far, the person whether.
             */
            void set_is_overlay_pushed(const bool& in_is_pushed);

        private:
            void fail(const std::string& in_message) const;


            Analysis::Recipe::RecipeApplier m_applier;
            ConvergenceModel                m_convergence_model;
            FailFunc                        m_fail_func;
            bool                            m_is_last_draw_covering; /* the draw that was classified last covers the whole of the view */
            bool                            m_is_overlay_pushed;     /* the person asks for the overlay to be pushed                    */
            Analysis::Recipe::DrawClass     m_last_draw_class;       /* of the draw that was classified last                            */
            uint64_t                        m_last_draw_cover_key;   /* the same                                                         */
            MatrixSlots                     m_matrix_slots;
            uint64_t                        m_n_unknown_draws;
            uint64_t                        m_n_unknown_frames;
            std::array<float, 16>           m_view_matrix_fp32vec16;
        };
    }
}

#endif /* STEREO_CORE_STEREO_CORE_H */
