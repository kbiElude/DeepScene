/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The application's matrices that a recipe rule is about, and which version of each one the device holds. The application sets the
 * matrices, and a draw needs the versions for its eye. RecipeStereoPlanner owns it.
 *
 * Not thread-safe.
 */
#ifndef STEREO_CORE_MATRIX_SLOTS_H
#define STEREO_CORE_MATRIX_SLOTS_H

#include "analysis/recipe/recipe_applier.h"
#include "common/api_function_argument.h"
#include "stereo/core/types.h"
#include <array>
#include <map>
#include <utility>
#include <vector>

namespace Stereo
{
    namespace Core
    {
        class MatrixSlots
        {
        public:
            /* Collects the matrices to put in the device before a draw of in_draw_class for in_eye, leaving out those that are already in it.
             * For an affected draw, the matrices that the recipe changes are made for the eye with the given distances. For any other draw
             * they are the application's own matrices. The exception is a draw to the final target (what the application presents): it is
             * pushed toward the viewer if in_overlay_parallax is above 0 (see Comfort::overlay_parallax). The caller must treat the returned
             * matrices as put in the device.
             *
             * The changes are written to the first entries of the vector. The caller keeps the vector from one call to the next so that the
             * storage of its entries is reused. The entries after the returned count are not valid. Returns the number of changes.
             */
            uint32_t bind(const Analysis::Recipe::DrawClass& in_draw_class,
                          const Analysis::Recipe::Eye&       in_eye,
                          const float&                       in_convergence_distance,
                          const float&                       in_eye_separation,
                          const bool&                        in_is_final_target,
                          const float&                       in_overlay_parallax,
                          std::vector<MatrixChange>*         inout_change_vec_ptr);

            /* Forgets the stored matrices. Call it when the device holds other matrices than the slots know of (after a device reset). */
            void forget();

            /* Handles an application call that sets a matrix. Returns true if a rule is about this matrix: the matrix is stored and the caller
             * must not execute the call. Otherwise returns false and forgets any matrix stored for the same call.
             */
            bool on_matrix_call(const Analysis::Recipe::RecipeApplier&                  in_applier,
                                const APIInterceptor::APIFunction&                      in_function,
                                const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                const std::array<float, 16>&                            in_matrix);

        private:
            /* A matrix is identified by the function and the integer arguments of the call that sets it (for example, the D3DTS_VIEW state). */
            typedef std::pair<APIInterceptor::APIFunction, std::vector<uint32_t> > SlotKey;

            /* A matrix of the application that a rule is about. */
            struct Slot
            {
                std::vector<APIInterceptor::APIFunctionArgument> argument_vec;
                MatrixSlotBinding                                bound                      = MatrixSlotBinding::NOTHING;
                float                                            bound_convergence_distance = 0.0f;
                float                                            bound_overlay_parallax     = 0.0f;
                float                                            bound_separation           = 0.0f;
                APIInterceptor::APIFunction                      function                   = APIInterceptor::APIFunction::APIFUNCTION_UNKNOWN;
                std::array<float, 16>                            matrix_fp32vec16           = {}; /* as the application set it */
                std::vector<const Analysis::Recipe::MatrixRule*> rule_ptr_vec;
            };


            static void fill_slot_key(const APIInterceptor::APIFunction&                      in_function,
                                      const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                      SlotKey*                                                out_key_ptr);


            Capture::APIDumpLoader::DumpedAPICall m_scratch_call; /* scratch for on_matrix_call(): its vectors keep their storage between calls */
            SlotKey                               m_scratch_key;  /* scratch for the slot key of on_matrix_call()                               */
            std::map<SlotKey, Slot>               m_slot_map;
        };
    }
}

#endif /* STEREO_CORE_MATRIX_SLOTS_H */
