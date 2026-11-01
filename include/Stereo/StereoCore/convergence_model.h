/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Where the eyes converge, and how far apart they are, for the draw that is being made: what the player set, what the recipe says of
 * the scene, and what it says of the draw (see StereoCore, which asks it).
 *
 * Not thread-safe.
 */
#ifndef STEREOCORE_CONVERGENCE_MODEL_H
#define STEREOCORE_CONVERGENCE_MODEL_H

namespace Stereo
{
    namespace StereoCore
    {
        class ConvergenceModel
        {
        public:
            ConvergenceModel();

            /* The frame that was completed has a distance of its scene: the distance goes toward it by some of the way each frame, which
             * keeps a scene that is made of frames of slightly different shapes from pulsing, and is it at once the first time. Returns
             * false, and leaves it as it was, if there is none (0).
             */
            bool follow_scene_convergence_distance(const float& in_target);

            /* The frame that was completed has a limit of how far apart the eyes may be in its scene (see SceneSeparation): the limit goes
             * toward it as the distance does, along the logarithm. A scene with none (0) lifts the limit at once.
             */
            void follow_scene_max_separation(const float& in_target);

            /* The setters' values; 0 for the ones that were not set. */
            float get_convergence_distance() const;
            float get_depth_strength      () const;
            float get_eye_separation      () const;
            float get_ipd_scale           () const;

            /* The one that was set, or else the one of the scene, or else the recipe's for the draw; 0 if there is none. */
            float get_effective_convergence_distance() const;

            /* With a strength above 0, a fixed part of the effective convergence distance (a thirtieth, as a rule of thumb has the distance
             * between the cameras of a stereo shot) times the strength and the scale of the distance between the person's eyes, but no more than the limit of the scene, which is a ceiling that no
             * strength lifts (a strength below 1 brings the limit down with it); otherwise the one that was set.
             */
            float get_effective_eye_separation() const;

            /* Each returns false, and leaves things as they were, if the value is not a number or is below zero. */
            bool set_convergence_distance(const float& in_convergence_distance);
            bool set_depth_strength      (const float& in_depth_strength);
            bool set_eye_separation      (const float& in_eye_separation);

            /* How the distance between the eyes of the person who looks compares with the one that the natural separation is made for (the
             * natural separation is multiplied by it, and the limit of a scene is not), which the program that shows the images says while they
             * are shown. 1 until it does. Returns false, and leaves it as it was, if it is not a number above zero.
             */
            bool set_ipd_scale(const float& in_ipd_scale);

            /* The distance of the draw that was classified last, from the recipe; 0 if it has none. */
            void set_draw_convergence_distance(const float& in_draw_convergence_distance);

        private:
            float m_convergence_distance;
            float m_depth_strength;
            float m_draw_convergence_distance;
            float m_eye_separation;
            float m_ipd_scale;
            float m_scene_convergence_distance; /* eased toward the recipe's for the shape of the last frame */
            float m_scene_max_separation;       /* the same, for the limit of the eyes; 0: none                */
        };
    }
}

#endif /* STEREOCORE_CONVERGENCE_MODEL_H */
