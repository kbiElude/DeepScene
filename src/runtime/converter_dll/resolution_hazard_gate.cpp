/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "common/macros.h"
#include "resolution_hazard_gate.h"
#include <string>

const char* Stereo::ConverterDLL::ResolutionHazardGate::get_reason(const Analysis::APIDumpAnalysis::ResolutionHazard& in_hazard)
{
    const char* result_ptr = "The application does what depends on the size of what it renders to";

    switch (in_hazard)
    {
        case Analysis::APIDumpAnalysis::ResolutionHazard::CLEARS_WITH_RECTANGLES:
        {
            result_ptr = "The application clears rectangles, which are in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::COPIES_WITH_RECTANGLES:
        {
            result_ptr = "The application copies rectangles between surfaces, which are in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::LINE_WIDTH_STATES:
        {
            result_ptr = "The application sets the width of lines, which is in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::PIXEL_READBACKS:
        {
            result_ptr = "The application reads pixels of what it renders to, at places in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::POINT_SIZE_STATES:
        {
            result_ptr = "The application sets the size of points, which is in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::RASTER_OPERATIONS:
        {
            result_ptr = "The application draws pixels or bitmaps at places in pixels, or zooms them";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::SCISSOR_BOXES:
        {
            result_ptr = "The application sets a scissor box, which is in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::SCREEN_SPACE_VERTICES:
        {
            result_ptr = "The application draws vertices that are positioned in pixels";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::SHADER_CREATIONS:
        {
            result_ptr = "The application makes shaders, whose constants may hold the size of the screen";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES:
        {
            result_ptr = "The application reads the size of the viewport or of a surface, which is of a size it does not know of";

            break;
        }

        case Analysis::APIDumpAnalysis::ResolutionHazard::TEXTURE_UPLOADS_TO_RENDER_TARGETS:
        {
            result_ptr = "The application gives pixels to a texture that it renders to, which is larger than it knows";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_hazard);

            break;
        }
    }

    return result_ptr;
}

void Stereo::ConverterDLL::ResolutionHazardGate::require_unaffected_by_change(Stereo::Core::RecipeStereoPlanner*                 in_core_ptr,
                                                                              const bool&                                        in_is_changed,
                                                                              const Analysis::APIDumpAnalysis::ResolutionHazard& in_hazard)
{
    if (in_is_changed)
    {
        const std::string reason = std::string(get_reason(in_hazard) ) +
                                   ", while it is rendered larger than it asks or in another place of the image";

        in_core_ptr->require_fast_path(false, /* in_is_ok */
                                       reason);
    }
}

void Stereo::ConverterDLL::ResolutionHazardGate::require_unaffected_by_scale(Stereo::Core::RecipeStereoPlanner*                 in_core_ptr,
                                                                             const Stereo::Core::ResolutionPolicy&              in_policy,
                                                                             const Analysis::APIDumpAnalysis::ResolutionHazard& in_hazard)
{
    require_unaffected_by_change(in_core_ptr,
                                 (in_policy.is_identity() == false),
                                 in_hazard);
}
