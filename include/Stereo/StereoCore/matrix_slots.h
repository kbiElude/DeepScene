/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The matrices of the application that a rule of the recipe is about, and what the device has of each: the application sets them, a
 * draw needs the ones of its eye (see StereoCore, which owns it).
 *
 * Not thread-safe.
 */
#ifndef STEREOCORE_MATRIX_SLOTS_H
#define STEREOCORE_MATRIX_SLOTS_H

#include "Analysis/RecipeApplier/recipe_applier.h"
#include "Stereo/StereoCore/types.h"
#include <array>
#include <map>
#include <utility>
#include <vector>

namespace Stereo
{
    namespace StereoCore
    {
        class MatrixSlots
        {
        public:
            /* The matrices to put in the device before a draw of the class, for the eye, that are not in it yet: for a draw which is
             * affected, those the recipe changes are made for the eye with the distances, for any other the application's, except that
             * a draw to what the application presents is pushed toward the viewer (see Comfort::overlay_parallax) if the parallax is more
             * than zero. What was returned is taken to be put in.
             *
             * The changes are written to the first entries of the vector, which the caller keeps from one call to the next so that
             * the room of its entries is used again; the entries after them are not meant. Returns how many there are.
             */
            uint32_t bind(const Analysis::RecipeLoader::DrawClass& in_draw_class,
                          const Analysis::RecipeLoader::Eye&       in_eye,
                          const float&                             in_convergence_distance,
                          const float&                             in_eye_separation,
                          const bool&                              in_is_final_target,
                          const float&                             in_overlay_parallax,
                          std::vector<MatrixChange>*               inout_change_vec_ptr);

            /* Forgets the matrices kept, for when the device has other ones than the slots know of (it was reset). */
            void forget();

            /* A call of the application that sets a matrix. Returns true if a rule is about it: the matrix is kept and the caller does not
             * execute the call. Otherwise any matrix kept for the same call is forgotten.
             */
            bool on_matrix_call(const Analysis::RecipeApplier::RecipeApplier&           in_applier,
                                const APIInterceptor::APIFunction&                      in_function,
                                const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                const std::array<float, 16>&                            in_matrix);

        private:
            /* What the device has of a matrix, as far as the slots know. */
            enum class Bound
            {
                LEFT,
                MONO,
                NOTHING, /* the matrix was set by the application, and has not been put in the device since */
                OVERLAY_LEFT,
                OVERLAY_RIGHT,
                RIGHT
            };

            /* A matrix is told by the function and the integer arguments of its call (the state of D3DTS_VIEW, say). */
            typedef std::pair<APIInterceptor::APIFunction, std::vector<uint32_t> > SlotKey;

            /* A matrix of the application that a rule is about. */
            struct Slot
            {
                std::vector<APIInterceptor::APIFunctionArgument> argument_vec;
                Bound                                            bound                      = Bound::NOTHING;
                float                                            bound_convergence_distance = 0.0f;
                float                                            bound_overlay_parallax     = 0.0f;
                float                                            bound_separation           = 0.0f;
                APIInterceptor::APIFunction                      function                   = APIInterceptor::APIFunction::APIFUNCTION_UNKNOWN;
                std::array<float, 16>                            matrix_fp32vec16           = {}; /* as the application set it */
                std::vector<const Analysis::RecipeLoader::Rule*> rule_ptr_vec;
            };


            static void fill_slot_key(const APIInterceptor::APIFunction&                      in_function,
                                      const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                      SlotKey*                                                out_key_ptr);


            Capture::APIDumpLoader::DumpedAPICall m_scratch_call; /* of on_matrix_call(): its vectors keep their room from call to call */
            SlotKey                               m_scratch_key;  /* the same                                                          */
            std::map<SlotKey, Slot>               m_slot_map;
        };
    }
}

#endif /* STEREOCORE_MATRIX_SLOTS_H */
