/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Loads a recipe from JSON text or a file. Every check that needs no application is done here: names, numbers and keys are
 * valid, and no unknown field is left. The format is described in docs/recipe-format.html.
 */
#ifndef RECIPELOADER_RECIPE_LOADER_H
#define RECIPELOADER_RECIPE_LOADER_H

#include "analysis/api_dump_analysis/json_reader.h"
#include "analysis/recipe_loader/types.h"
#include <string>
#include <vector>

namespace Analysis
{
    namespace RecipeLoader
    {
        class RecipeLoader
        {
        public:
            /* One line for each list of pass keys or of draws in a pass: a replay knows which pass a draw is in, a converter that plays the application as it runs does not, and leaves them out. */
            static std::vector<std::string> describe_pass_selectors_live_ignores(const Recipe& in_recipe);

            /* Returns false, and says why, if the recipe cannot be read. */
            static bool load(const std::string& in_filename,
                             Recipe*            out_recipe_ptr,
                             std::string*       out_error_ptr);

            static bool parse(const std::string& in_text,
                              Recipe*            out_recipe_ptr,
                              std::string*       out_error_ptr);

        private:
            /* The API of a function that a rule can be about: the names of its transform states are its own. */
            enum class Api
            {
                D3D7,
                D3D8
            };

            /* A function that a rule can be about. */
            struct CallDescriptor
            {
                Api                         api;
                APIInterceptor::APIFunction function;
                uint32_t                    matrix_argument_index;
                const char*                 name;
                uint32_t                    state_argument_index;
            };

            typedef std::vector<std::pair<uint64_t, float> > KeyDistanceVector;

            static bool                  check_keys              (const Analysis::APIDumpAnalysis::JsonValue& in_object,
                                                                  const std::vector<std::string>&             in_allowed_key_vec,
                                                                  const std::string&                          in_path,
                                                                  std::string*                                out_error_ptr);
            static const CallDescriptor* find_call_descriptor_ptr(const std::string&                          in_name);

            /* True if a key, or a draw of a pass, is in both lists. */
            static bool has_common_key      (const std::vector<uint64_t>& in_a,
                                             const std::vector<uint64_t>& in_b);
            static bool has_common_pass_draw(const std::vector<PassDraw>& in_a,
                                             const std::vector<PassDraw>& in_b);

            static bool parse_action(const Analysis::APIDumpAnalysis::JsonValue& in_action,
                                     const std::string&                          in_path,
                                     Rule*                                       inout_rule_ptr,
                                     std::string*                                out_error_ptr);
            static bool parse_pass_draw_array   (const Analysis::APIDumpAnalysis::JsonValue& in_array,
                                                 const std::string&                          in_path,
                                                 std::vector<PassDraw>*                      out_pass_draw_vec_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_per_eye_textures(const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                               const std::string&                          in_path,
                                               std::vector<PerEyeTexture>*                 out_texture_vec_ptr,
                                               std::string*                                out_error_ptr);
            static bool parse_baked (const Analysis::APIDumpAnalysis::JsonValue& in_baked,
                                     const std::string&                          in_path,
                                     Recipe*                                     inout_recipe_ptr,
                                     std::string*                                out_error_ptr);

            static bool parse_comfort           (const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                 const std::string&                          in_path,
                                                 Comfort*                                    out_comfort_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_draw_rule         (const Analysis::APIDumpAnalysis::JsonValue& in_rule,
                                                 const std::string&                          in_path,
                                                 DrawRule*                                   out_rule_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_draw_selector     (const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                 const std::string&                          in_path,
                                                 DrawSelector*                               out_selector_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_hex_key           (const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                 const std::string&                          in_path,
                                                 uint64_t*                                   out_key_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_hex_key_array     (const Analysis::APIDumpAnalysis::JsonValue& in_array,
                                                 const std::string&                          in_path,
                                                 std::vector<uint64_t>*                      out_key_vec_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_key_distance_array(const Analysis::APIDumpAnalysis::JsonValue& in_array,
                                                 const std::string&                          in_path,
                                                 const std::string&                          in_key_name,
                                                 const std::string&                          in_distance_name,
                                                 KeyDistanceVector*                          out_key_distance_vec_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_matrix            (const Analysis::APIDumpAnalysis::JsonValue& in_array,
                                                 const std::string&                          in_path,
                                                 std::array<float, 16>*                      out_matrix_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_rule              (const Analysis::APIDumpAnalysis::JsonValue& in_rule,
                                                 const std::string&                          in_path,
                                                 Rule*                                       out_rule_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_screen_parallax   (const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                 const std::string&                          in_path,
                                                 ScreenParallax*                             out_screen_parallax_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_state             (const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                 const std::string&                          in_path,
                                                 const CallDescriptor&                       in_call,
                                                 uint32_t*                                   out_state_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_unsigned          (const Analysis::APIDumpAnalysis::JsonValue& in_value,
                                                 const std::string&                          in_path,
                                                 uint32_t*                                   out_value_ptr,
                                                 std::string*                                out_error_ptr);
            static bool parse_when              (const Analysis::APIDumpAnalysis::JsonValue& in_when,
                                                 const std::string&                          in_path,
                                                 const CallDescriptor&                       in_call,
                                                 Rule*                                       inout_rule_ptr,
                                                 std::string*                                out_error_ptr);
        };
    }
}

#endif /* RECIPELOADER_RECIPE_LOADER_H */
