/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The decisions of a converter that plays an application as it runs and renders each eye of what the application draws, whatever the
 * API is. An adapter for an API tells StereoCore what the application does (it sets a matrix, it draws) and carries out its answers
 * (set these matrices, draw this once or once per eye). The adapter knows nothing about the application, only about the recipe, which
 * must be baked (see RecipeBake).
 *
 * A converter knows less when a draw is issued than an offline replay does. It does not know which pass the draw belongs to. A draw is
 * therefore identified by its signature and the view matrix (see Recipe::draw_rule_vec), and a recipe rule that is about a pass or a
 * frame shape cannot be followed. If an application needs more than that, the converter fails at once, through the function it was
 * given, instead of producing wrong images.
 *
 * Not thread-safe: an adapter calls it from the thread that calls the device.
 */
#ifndef STEREO_CORE_STEREO_CORE_H
#define STEREO_CORE_STEREO_CORE_H

#include "analysis/recipe/recipe_applier.h"
#include "common/api_function_argument.h"
#include "stereo/core/convergence_model.h"
#include "stereo/core/depth_buffer_statistics.h"
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
            /* Fails, through in_fail_func, if the recipe is not baked or has rules that cannot be followed while the application runs. */
            StereoCore(const Analysis::Recipe::Recipe& in_recipe,
                       const FailFunc&                 in_fail_func);

            /* Collects the matrices to put in the device before a draw of in_draw_class for in_eye, leaving out those that are already in it.
             * For an affected draw, the matrices that the recipe changes are made for the eye. For any other draw they are the application's
             * own matrices. A draw that goes to the final target (what the application presents) and is not affected is pushed toward the
             * viewer if the recipe says so. The caller must treat the returned matrices as put in the device. Do not call it for a skipped
             * draw, which is not played.
             *
             * The changes are written to the first entries of the vector. The caller keeps the vector from one call to the next so that the
             * storage of its entries is reused. The entries after the returned count are not valid. Returns the number of changes.
             */
            uint32_t bind_matrices(const Analysis::Recipe::DrawClass& in_draw_class,
                                   const Analysis::Recipe::Eye&       in_eye,
                                   std::vector<MatrixChange>*         inout_change_vec_ptr,
                                   const bool&                        in_is_final_target = false);

            /* Returns what the recipe does to a draw, found by the draw's signature and the view matrix that the application has now. A draw
             * that covers the whole view (a flash) is not pushed toward the viewer, because its picture would move and leave a strip of the
             * screen bare.
             */
            Analysis::Recipe::DrawClass classify_draw(const uint64_t& in_signature_key,
                                                      const uint64_t& in_cover_key);

            /* Forgets the stored matrices. Call it when the device holds other matrices than the converter knows of (after a device reset). */
            void forget_matrices();

            /* Accessors for the values that were set (see set_convergence_distance()). A convergence distance of 0 means that the recipe's
             * distances are in use.
             */
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
            const std::array<float, 16>& get_view_matrix                   () const;

            /* Returns the matrix that is put in front of the camera of a draw for the eye. It is laid out as OpenGL lays out matrices (column
             * after column). It moves the camera half the eye separation to the side (the left eye is on the left) and shears the scene by
             * depth, so that what is at the convergence distance stays where it is. The separation and the distance are the effective ones
             * (see ConvergenceModel). With no convergence distance the matrix is only the translation.
             */
            std::array<float, 16> make_eye_shift_matrix(const Analysis::Recipe::Eye& in_eye) const;

            /* Returns true if the last draw is attached to the camera and is therefore made the same way for both eyes. That is the case when
             * its origin depth (in front of the camera, whatever the handedness) is below Comfort::camera_attached_depth times the
             * convergence distance, and the draw is not in eye space.
             */
            bool is_last_draw_attached_to_camera(const float& in_origin_depth) const;

            /* Copies the last draw's vertices (in pixels, see ScreenParallax) and moves the copy for the eye. It asks plan_screen_shift() what
             * to do and leaves the application's vertices alone. Returns true if the output vector holds the vertices to draw, because they
             * were moved or because in_must_copy asked for a copy.
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

            /* Returns true if the recipe wants a run of single-pixel reads by the application to be served from a picture that is read once
             * (see Recipe::caches_pixel_reads).
             */
            bool get_caches_pixel_reads() const;

            /* The fraction of the surfaces of a scene that counts as its nearest (see Comfort::near_fraction). */
            float get_near_fraction() const;

            /* Whether the recipe has the converter measure how deep each frame is (see Comfort::measures_depth). */
            bool get_measures_depth() const;

            /* Reports how deep the frame that the application has just presented was, measured from its depth buffer (see
             * DepthBufferStatistics), together with the x scale of its projection. The eyes follow the convergence distance and the maximum
             * separation that the comfort settings of the recipe give for that depth (see RecipeApplier::get_comfort_limits()). If the recipe
             * has comfort settings for the shape of the frame, they win over the values baked for the shape (see on_frame_completed()).
             * Returns false and does nothing if the range has no depth.
             */
            bool on_frame_depth(const uint64_t&   in_shape_key,
                                const DepthRange& in_depth_range,
                                const float&      in_projection_scale);

            /* Reports that the frame the application has just presented had the shape in_shape_key (see CompletedFrame::shape_key). If the
             * recipe has a convergence distance for that shape (see Recipe::scene_convergence_vec), the draws that follow converge there. The
             * convergence eases toward the distance over several frames, which keeps a scene made of frames with slightly different shapes
             * from pulsing. The first distance is taken at once. If the recipe does not know the shape, the convergence distance and the
             * limit on the eye separation stay as they are.
             */
            void on_frame_completed(const uint64_t& in_shape_key);

            /* Handles an application call that sets a matrix. The arguments are those of the call, and in_matrix is the matrix inside them.
             * Returns true if a recipe rule is about this matrix: the converter stores the matrix and puts it in the device when a draw needs
             * it (see bind_matrices()), so the caller must not execute the call. Otherwise the caller executes the call, and any matrix
             * stored for the same call is forgotten.
             */
            bool on_matrix_call(const APIInterceptor::APIFunction&                      in_function,
                                const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                const std::array<float, 16>&                            in_matrix);

            /* Reports that the application set the view matrix, which the rules for draws may be about. Call it in addition to
             * on_matrix_call() when the call sets the view matrix.
             */
            void on_view_matrix_set(const std::array<float, 16>& in_matrix);

            /* Returns how far apart the eyes move a point at in_origin_depth (negative in front of the camera), as a fraction of the picture
             * width. The value is positive in front of the convergence distance and negative behind it. It is 0 if the point is not in front
             * of the camera.
             */
            float get_eye_parallax(const float& in_origin_depth,
                                   const float& in_projection_x_scale,
                                   const bool&  in_is_perspective) const;

            /* Returns how far, in pixels, the draw that classify_draw() last classified is moved sideways for the eye if it is a protruding
             * part (see get_is_last_draw_part()). The result puts the part where the eyes would have put it if it had been drawn once per eye.
             * It is the shift that the affected draws get. It depends on the depth of the draw's origin in eye space (a negative number: the z
             * of the translation of its model-view matrix) and on the projection the draw uses (its x scale, and whether it is perspective).
             * A part drawn once for both eyes is moved by this shift and by the push, so that it stands where the object it belongs to
             * stands, and slightly in front of it. Positive is to the right, as for plan_push_shift(). It is 0 if the draw is not a part.
             */
            float plan_follow_shift(const Analysis::Recipe::Eye& in_eye,
                                    const float&                 in_picture_width_pixels,
                                    const float&                 in_origin_depth,
                                    const float&                 in_projection_x_scale,
                                    const bool&                  in_is_perspective) const;

            /* Returns the shift of a protruding part that is drawn once for both eyes. It is the sum of the shift from plan_follow_shift() and
             * the push (plan_push_shift()), limited to what the scene allows: the nearest and the farthest parallax of the recipe, or of the
             * scene if the scene sets its own. Without the limit, a part whose origin is very near the camera would be moved without bound.
             * Positive is to the right. The result equals plan_push_shift() if the draw is not a part.
             */
            float plan_part_shift(const Analysis::Recipe::Eye& in_eye,
                                  const float&                 in_picture_width_pixels,
                                  const float&                 in_origin_depth,
                                  const float&                 in_projection_x_scale,
                                  const bool&                  in_is_perspective) const;

            /* Returns how far, in pixels, the draw that classify_draw() last classified is moved sideways for the eye (see PushShiftPlanner),
             * if the recipe pushes it (see get_is_last_draw_pushed()). in_picture_width_pixels is the width of the viewport the draw is made
             * with. The effective separation already respects the scene's limit, so no limit is passed.
             */
            float plan_push_shift(const Analysis::Recipe::Eye& in_eye,
                                  const float&                 in_picture_width_pixels) const;

            /* The same, for an adapter that says whether the eyes move the draw. A draw that is made as it is (attached to the camera, 2D)
             * gets the whole push.
             */
            float plan_push_shift(const Analysis::Recipe::Eye& in_eye,
                                  const float&                 in_picture_width_pixels,
                                  const bool&                  in_is_moved_by_eyes) const;

            /* The same, for a draw that the eyes already move apart by in_layer_parallax, a fraction of the picture width (see
             * get_eye_parallax(); 0 if they do not move it).
             */
            float plan_push_shift_with_layer(const Analysis::Recipe::Eye& in_eye,
                                             const float&                 in_picture_width_pixels,
                                             const float&                 in_layer_parallax) const;

            /* Returns what to do to the vertices of the draw that classify_draw() last classified, for the eye, when the vertices are in the
             * space of the picture (see ScreenParallaxPlanner). It uses the distances that the core has now, which combine the scene's values
             * and the person's settings. in_picture_width is the width of the viewport the draw is made with. The draw counts as presented
             * (in_is_presented) if it goes to what the application presents.
             */
            ScreenShiftPlan plan_screen_shift(const Analysis::Recipe::Eye& in_eye,
                                              const bool&                  in_has_extent,
                                              const bool&                  in_is_presented,
                                              const float&                 in_picture_width,
                                              const bool&                  in_reaches_both_sides) const;

            /* Fails, through the fail function, if in_is_ok is false. An adapter calls it where it finds that the application does something
             * that could only be converted correctly by holding back the calls of a pass.
             */
            void require_fast_path(const bool&        in_is_ok,
                                   const std::string& in_reason) const;

            /* Fails, through the fail function, with in_reason as the whole message if in_is_ok is false. An adapter calls it for anything
             * that the converter cannot do for the application, whatever the cause (for example, the images are larger than they can be
             * passed on).
             */
            void require_supported(const bool&        in_is_ok,
                                   const std::string& in_reason) const;

            /* Sets the distance at which the eyes converge, in the units of the matrices. What is at that distance appears at the same place
             * in both eyes, for every draw. The initial value is 0, which leaves the choice to the recipe. The recipe gives the distance of the
             * scene (see on_frame_completed()), and until a scene is known, the distance for the signature of the draw (see
             * Recipe::convergence_vec). If it has neither, there is no convergence.
             */
            void set_convergence_distance(const float& in_convergence_distance);

            /* Sets how strong the 3D is: 1 is natural, 0 is none, 2 is twice natural. Above 0, the distance between the eyes is not the one
             * set by set_eye_separation(). It follows the convergence distance of each draw (see set_convergence_distance()): one thirtieth
             * of it (a rule of thumb for the camera distance of a stereo shot) times the strength. That keeps the 3D equally strong in a
             * corridor and on a hill, which a fixed distance in the application's camera units cannot do. For a draw with no convergence
             * distance, the separation that was set is used.
             */
            void set_depth_strength(const float& in_depth_strength);

            /* Sets the distance between the eyes, in the units of the matrices. */
            void set_eye_separation(const float& in_eye_separation);

            /* Sets how long frames take. The eyes use it to follow a scene (see ConvergenceModel::get_fraction_of_the_way()). It is the time
             * since the previous frame, and it must be set before the completed frame is reported (on_frame_completed(), on_frame_depth()).
             * Until it is set, the interval is the duration of a 72 Hz frame, as in a replay or a test, which have no meaningful clock.
             */
            void set_frame_interval_seconds(const float& in_frame_interval_seconds);

            /* Sets how the eye distance of the viewer compares with the eye distance that the separation is made for (see
             * ConvergenceModel::set_ipd_scale()).
             */
            void set_ipd_scale(const float& in_ipd_scale);

            /* Sets whether the draws that the recipe leaves alone and that go to what is presented are pushed toward the viewer, by the amount
             * the recipe gives (Comfort::overlay_parallax; no push if it is 0). It is off until it is set: the recipe says how far, the person
             * says whether.
             */
            void set_is_overlay_pushed(const bool& in_is_pushed);

            /* Sets the shape of the last completed frame. It identifies the scene of the draws that follow. on_frame_depth() does not set it,
             * so the converter must.
             */
            void set_scene_shape_key(const uint64_t& in_shape_key);

        private:
            void fail(const std::string& in_message) const;


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

#endif /* STEREO_CORE_STEREO_CORE_H */
