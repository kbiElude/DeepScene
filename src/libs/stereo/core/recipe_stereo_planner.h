/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * API-neutral decisions of a live converter that renders each eye. An adapter reports what the application does and carries out the answers.
 * The recipe must be baked (see RecipeBake). A draw is known only by signature and view matrix (see Recipe::draw_rule_vec), so pass or
 * frame-shape rules cannot be followed; the converter then fails through its fail function. Not thread-safe: call from the device thread.
 */
#ifndef STEREO_CORE_RECIPE_STEREO_PLANNER_H
#define STEREO_CORE_RECIPE_STEREO_PLANNER_H

#include "analysis/recipe/recipe_applier.h"
#include "common/api_function_argument.h"
#include "stereo/core/convergence_model.h"
#include "stereo/core/depth_range_measurer.h"
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
        class RecipeStereoPlanner
        {
        public:
            /* Fails, through in_fail_func, if the recipe is not baked or has rules that cannot be followed while the application runs. */
            RecipeStereoPlanner(const Analysis::Recipe::Recipe& in_recipe,
                                const FailFunc&                 in_fail_func);

            /* Collects the matrices to put in the device before a draw of in_draw_class for in_eye, leaving out those already in it.
             * Affected draws get eye matrices, others the application's own. Unaffected draws to the final target may be pushed. The
             * caller must treat the result as put in the device. Do not call for a skipped draw.
             * Written to the first entries of the vector (keep it between calls to reuse storage); entries after the count are invalid.
             * Returns the number of changes.
             */
            uint32_t bind_matrices(const Analysis::Recipe::DrawClass& in_draw_class,
                                   const Analysis::Recipe::Eye&       in_eye,
                                   std::vector<MatrixChange>*         inout_change_vec_ptr,
                                   const bool&                        in_is_final_target = false);

            /* Returns what the recipe does to a draw, by its signature and the current view matrix. A draw covering the whole view
             * is never pushed (it would leave a bare strip).
             */
            Analysis::Recipe::DrawClass classify_draw(const uint64_t& in_signature_key,
                                                      const uint64_t& in_cover_key);

            /* Forgets the stored matrices. Call it when the device holds other matrices than the converter knows of (after a device reset). */
            void forget_matrices();

            /* Accessors for the set values (see set_convergence_distance()). A convergence distance of 0 means the recipe's are in use. */
            float                        get_convergence_distance          () const;
            float                        get_depth_strength                () const;
            float                        get_effective_convergence_distance() const;
            float                        get_effective_eye_separation      () const;
            float                        get_eye_separation                () const;
            float                        get_ipd_scale                     () const;
            bool                         get_is_last_draw_eye_space        () const;
            bool                         get_is_last_draw_part             () const;
            bool                         get_is_last_draw_pushed           () const;
            bool                         get_is_last_draw_side_reaching    () const;
            bool                         get_is_overlay_pushed             () const;
            uint64_t                     get_n_unknown_draws               () const;
            uint64_t                     get_n_unknown_frames              () const;

            /* Returns the matrix put in front of a draw's camera for the eye, in OpenGL (column-major) layout: half the eye separation
             * sideways plus a depth shear that keeps the convergence distance fixed. Uses the effective separation and distance (see
             * ConvergenceModel). Without a convergence distance it is only the translation.
             */
            std::array<float, 16> make_eye_shift_matrix(const Analysis::Recipe::Eye& in_eye) const;

            /* Returns true if the last draw is attached to the camera (drawn the same for both eyes): its origin depth is below
             * Comfort::camera_attached_depth times the convergence distance, and it is not in eye space.
             */
            bool is_last_draw_attached_to_camera(const float& in_origin_depth) const;

            /* Copies the last draw's vertices (in pixels, see ScreenSpaceVertexShifter) and moves the copy for the eye per
             * plan_screen_shift(); the application's vertices are untouched. Returns true if the output holds the vertices to draw
             * (moved, or in_must_copy asked for a copy).
             */
            bool make_screen_vertices(const Analysis::Recipe::Eye& in_eye,
                                      const bool&                  in_is_presented,
                                      const float&                 in_picture_width,
                                      const uint8_t*               in_vertex_ptr,
                                      const uint32_t&              in_vertex_n_bytes,
                                      const size_t&                in_n_vertices,
                                      const uint16_t*              in_index_ptr,
                                      const size_t&                in_n_indices,
                                      const bool&                  in_must_copy,
                                      std::vector<uint8_t>*        out_vertex_vec_ptr) const;

            /* How close to the camera a draw must be to count as attached to it, as a fraction of the convergence distance (see
             * Comfort::camera_attached_depth). 0: none.
             */
            float get_camera_attached_depth() const;

            /* Returns true if the recipe wants single-pixel reads served from one picture read once (see Recipe::caches_pixel_reads). */
            bool get_caches_pixel_reads() const;

            /* The fraction of the surfaces of a scene that counts as its nearest (see Comfort::near_fraction). */
            float get_near_fraction() const;

            /* Whether the recipe has the converter measure how deep each frame is (see Comfort::measures_depth). */
            bool get_measures_depth() const;

            /* Reports the depth of the just-presented frame, measured from its depth buffer (see DepthRangeMeasurer), with the x scale of
             * its projection. The eyes follow the convergence distance and maximum separation that the recipe's comfort settings give for
             * it (see RecipeApplier::get_comfort_limits()); comfort settings for the frame shape win over baked values. Easing as in
             * on_frame_completed(). Returns false and does nothing if the range has no depth.
             */
            bool on_frame_depth(const uint64_t&   in_shape_key,
                                const DepthRange& in_depth_range,
                                const float&      in_projection_scale);

            /* Reports that the just-presented frame had the shape in_shape_key (see CompletedFrame::shape_key). If the recipe has a
             * convergence distance for it (see Recipe::scene_convergence_vec), later draws converge there, easing over several frames (by
             * time with comfort.ease_by_time). The first distance, and a far one with comfort.snap_at_cuts, is taken at once. For an
             * unknown shape the limit and distance stay, except a distance above the recipe's default for draws (see ViewAnchorShift)
             * comes down to it.
             */
            void on_frame_completed(const uint64_t& in_shape_key);

            /* Handles an application call that sets a matrix (in_matrix is the matrix inside its arguments). Returns true if a rule is
             * about it: the matrix is stored and bound at draw time (see bind_matrices()), so the caller must not execute the call.
             * Otherwise the caller executes it and any matrix stored for the same call is forgotten.
             */
            bool on_matrix_call(const APIInterceptor::APIFunction&                      in_function,
                                const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                const std::array<float, 16>&                            in_matrix);

            /* Reports a view matrix set by the application. Call it in addition to on_matrix_call(). */
            void on_view_matrix_set(const std::array<float, 16>& in_matrix);

            /* Returns how far apart the eyes move a point at in_origin_depth (negative in front of the camera), as a fraction of the
             * picture width: positive nearer than the convergence distance, negative beyond it, 0 if not in front of the camera.
             */
            float get_eye_parallax(const float& in_origin_depth,
                                   const float& in_projection_x_scale,
                                   const bool&  in_is_perspective) const;

            /* Returns the sideways shift in pixels, for the eye, of the last classified draw if it is a protruding part (see
             * get_is_last_draw_part()): the shift the affected draws get. Depends on the origin depth in eye space (negative: z of the
             * model-view translation) and the projection (x scale, perspective or not). Positive is right, as for plan_push_shift().
             * 0 if the draw is not a part.
             */
            float plan_follow_shift(const Analysis::Recipe::Eye& in_eye,
                                    const float&                 in_picture_width_pixels,
                                    const float&                 in_origin_depth,
                                    const float&                 in_projection_x_scale,
                                    const bool&                  in_is_perspective) const;

            /* Returns the shift of a protruding part drawn once for both eyes: plan_follow_shift() plus the push (plan_push_shift()),
             * limited to the near and far parallax of the recipe (or the scene's own). Positive is right. Equals plan_push_shift()
             * if the draw is not a part.
             */
            float plan_part_shift(const Analysis::Recipe::Eye& in_eye,
                                  const float&                 in_picture_width_pixels,
                                  const float&                 in_origin_depth,
                                  const float&                 in_projection_x_scale,
                                  const bool&                  in_is_perspective) const;

            /* Returns the sideways shift in pixels, for the eye, of the last classified draw if the recipe pushes it (see
             * PushShiftPlanner, get_is_last_draw_pushed()). in_picture_width_pixels is the draw's viewport width. The separation
             * already respects the scene limit.
             */
            float plan_push_shift(const Analysis::Recipe::Eye& in_eye,
                                  const float&                 in_picture_width_pixels) const;

            /* The same, for an adapter that says whether the eyes move the draw. A draw made as it is (attached, 2D) gets the whole push. */
            float plan_push_shift(const Analysis::Recipe::Eye& in_eye,
                                  const float&                 in_picture_width_pixels,
                                  const bool&                  in_is_moved_by_eyes) const;

            /* The same, for a draw the eyes already move apart by in_layer_parallax (picture-width fraction; see get_eye_parallax()). */
            float plan_push_shift_with_layer(const Analysis::Recipe::Eye& in_eye,
                                             const float&                 in_picture_width_pixels,
                                             const float&                 in_layer_parallax) const;

            /* Returns what to do to the last classified draw's vertices for the eye when they are in picture space (see
             * ScreenParallaxPlanner), using the current distances (scene values plus the person's settings). in_picture_width is the
             * draw's viewport width; in_is_presented is whether it goes to what the application presents.
             */
            ScreenShiftPlan plan_screen_shift(const Analysis::Recipe::Eye& in_eye,
                                              const bool&                  in_has_extent,
                                              const bool&                  in_is_presented,
                                              const float&                 in_picture_width,
                                              const bool&                  in_reaches_both_sides) const;

            /* Fails, through the fail function, if in_is_ok is false. For application behavior that would need holding back a pass's calls. */
            void require_fast_path(const bool&        in_is_ok,
                                   const std::string& in_reason) const;

            /* Fails, through the fail function, with in_reason as the whole message if in_is_ok is false. For anything the converter
             * cannot do for the application.
             */
            void require_supported(const bool&        in_is_ok,
                                   const std::string& in_reason) const;

            /* Sets the convergence distance, in matrix units, for every draw. The initial 0 leaves it to the recipe: the scene's
             * distance (see on_frame_completed()), else the draw signature's (see Recipe::convergence_vec), else none.
             */
            void set_convergence_distance(const float& in_convergence_distance);

            /* Sets the 3D strength: 1 natural, 0 none, 2 twice natural. Above 0, the eye distance is not the one from
             * set_eye_separation() but one thirtieth of each draw's convergence distance (see set_convergence_distance()) times the
             * strength. A draw with no convergence distance uses the separation that was set.
             */
            void set_depth_strength(const float& in_depth_strength);

            /* Sets the distance between the eyes, in the units of the matrices. */
            void set_eye_separation(const float& in_eye_separation);

            /* Sets the time since the previous frame, used for easing (see ConvergenceModel::get_fraction_of_the_way()). Set it before
             * on_frame_completed() and on_frame_depth(). Until set, it is a 72 Hz frame.
             */
            void set_frame_interval_seconds(const float& in_frame_interval_seconds);

            /* Sets how the eye distance of the viewer compares with the eye distance that the separation is made for (see
             * ConvergenceModel::set_ipd_scale()).
             */
            void set_ipd_scale(const float& in_ipd_scale);

            /* Sets whether left-alone draws to the presented image are pushed by Comfort::overlay_parallax (0: no push). Off until set;
             * the recipe says how far and which draws (RecipeApplier::decide_draw()), the person says whether.
             */
            void set_is_overlay_pushed(const bool& in_is_pushed);

            /* Sets the shape of the last completed frame, which identifies the scene of later draws. on_frame_depth() does not set it. */
            void set_scene_shape_key(const uint64_t& in_shape_key);

        private:
            void  fail(const std::string& in_message) const;

            /* Fraction of the way to a scene's distance and limit the eyes go per frame: by frame time with comfort.ease_by_time, else fixed. */
            float get_fraction_of_the_way() const;


            Analysis::Recipe::RecipeApplier m_applier;
            ConvergenceModel                m_convergence_model;
            FailFunc                        m_fail_func;
            float                           m_frame_interval_seconds;     /* the duration of the last completed frame                       */
            bool                            m_is_last_draw_covering;      /* the last classified draw covers the whole view                 */
            bool                            m_is_last_draw_eye_space;     /* the recipe says its vertices are in eye space                  */
            bool                            m_is_last_draw_part;          /* the last classified draw is a part that protrudes              */
            bool                            m_is_last_draw_pushed;        /* the recipe pushes the last classified draw                     */
            bool                            m_is_last_draw_side_reaching; /* a flat overlay from edge to edge                               */
            bool                            m_is_overlay_pushed;          /* the person asks for the overlay to be pushed                   */
            Analysis::Recipe::DrawClass     m_last_draw_class;            /* class of the last classified draw                              */
            uint64_t                        m_last_draw_cover_key;        /* cover key of the last classified draw                          */
            MatrixSlots                     m_matrix_slots;
            uint64_t                        m_n_unknown_draws;
            uint64_t                        m_n_unknown_frames;
            uint64_t                        m_scene_shape_key;            /* shape of the last completed frame: the scene the draws are in  */
            std::array<float, 16>           m_view_matrix_fp32vec16;
        };
    }
}

#endif /* STEREO_CORE_RECIPE_STEREO_PLANNER_H */
