/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Decides where the eyes converge and how far apart they are for the draw being made. It combines the values the player set, what the
 * recipe says about the scene, and what the recipe says about the draw. RecipeStereoPlanner owns it and asks it.
 *
 * Not thread-safe.
 */
#ifndef STEREO_CORE_CONVERGENCE_MODEL_H
#define STEREO_CORE_CONVERGENCE_MODEL_H

namespace Stereo
{
    namespace Core
    {
        class ConvergenceModel
        {
        public:
            ConvergenceModel();

            /* Called when a completed frame has a scene convergence distance (in_target). The stored scene distance moves toward in_target by
             * in_fraction_of_the_way. It jumps to in_target on the first call, and at a cut (a target more than twice or less than half of the
             * previous one) if set_snaps_at_cuts() was called with true. Returns false and changes nothing if in_target is not above 0.
             */
            bool follow_scene_convergence_distance(const float& in_target,
                                                   const float& in_fraction_of_the_way);

            /* Called when a completed frame has a scene limit for the eye separation (in_target). The limit eases, and snaps at a cut, like the
             * scene distance does. A target of 0 or less removes the limit at once.
             */
            void follow_scene_max_separation(const float& in_target,
                                             const float& in_fraction_of_the_way);

            /* The values stored by the setters. A value that was never set is 0, except the IPD scale, which is 1. */
            float get_convergence_distance() const;
            float get_depth_strength      () const;
            float get_eye_separation      () const;
            float get_ipd_scale           () const;

            /* The convergence distance the player set. If none was set, the scene distance. If there is none either, the recipe's distance for
             * the draw. 0 if all three are missing.
             */
            float get_effective_convergence_distance() const;

            /* If the depth strength and the effective convergence distance are both above 0, returns the natural separation: one thirtieth of the
             * effective convergence distance (a rule of thumb for the camera distance of a stereo shot), times the depth strength and the IPD
             * scale. The scene's maximum separation caps it. A depth strength below 1 lowers that cap by the same factor, and no strength
             * raises it. Otherwise returns the separation that was set.
             */
            float get_effective_eye_separation() const;

            /* Each returns false and changes nothing if the value is not a number or is below zero. */
            bool set_convergence_distance(const float& in_convergence_distance);
            bool set_depth_strength      (const float& in_depth_strength);
            bool set_eye_separation      (const float& in_eye_separation);

            /* The ratio of the viewer's eye distance to the eye distance that the natural separation assumes. The natural separation is
             * multiplied by it, the scene limit is not. The program that shows the images reports it while they are shown. It is 1 until then.
             * Returns false and changes nothing if the value is not a number above 0.
             */
            bool set_ipd_scale(const float& in_ipd_scale);

            /* The convergence distance that the recipe gives for the draw classified last; 0 if it gives none. */
            void set_draw_convergence_distance(const float& in_draw_convergence_distance);

            /* Whether the scene distance and limit jump to the target at a cut (see follow_scene_convergence_distance()) instead of easing across
             * it. Off until it is set.
             */
            void set_snaps_at_cuts(const bool& in_snaps_at_cuts);

            /* The fraction of the way to its target that a value covers in in_elapsed_seconds. It is 0 for no time and 0.05 for one frame at 72 Hz
             * (the headset rate that the easing was tuned for). The movement depends only on elapsed time: two frames of half the length cover as
             * much as one full frame, so a run at 1000 frames per second looks the same as a run at 72. After a long stall it is close to 1, so
             * the eyes are at the target.
             */
            static float get_fraction_of_the_way(const float& in_elapsed_seconds);

            /* The fraction of the way to its target that a value covers in one frame, whatever the frame takes: 0.05, the same as
             * get_fraction_of_the_way() for a frame at 72 Hz. The eyes then move by the same part of the way in each frame, so a long frame
             * does not make them jump.
             */
            static float get_fraction_per_frame();

            /* The duration of one frame at 72 Hz. get_fraction_of_the_way() returns 0.05 for it. */
            static float get_reference_frame_seconds();

        private:
            /* Moves in_current toward in_target by in_fraction_of_the_way, measured on the logarithm of the values. Returns in_target if
             * in_current is not above 0 or if the eased value is close enough to the target.
             */
            static float get_eased_value(const float& in_current,
                                         const float& in_target,
                                         const float& in_fraction_of_the_way);

            /* True if in_target is more than twice or less than half of in_previous_target. False if either of them is not above 0. */
            static bool is_cut(const float& in_previous_target,
                               const float& in_target);

            float m_convergence_distance;
            float m_depth_strength;
            float m_draw_convergence_distance;
            float m_eye_separation;
            float m_ipd_scale;
            float m_previous_scene_convergence_target; /* the previous frame's target; 0: none */
            float m_previous_scene_max_separation_target;
            float m_scene_convergence_distance; /* eased toward the recipe's scene distance                    */
            float m_scene_max_separation;       /* the same, for the eye separation limit; 0: none             */
            bool  m_snaps_at_cuts;              /* see set_snaps_at_cuts()                                     */
        };
    }
}

#endif /* STEREO_CORE_CONVERGENCE_MODEL_H */
