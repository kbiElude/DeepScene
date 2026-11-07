/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_matrix_math.h"
#include <math.h>

Analysis::APIDumpAnalysis::Frontends::GlMatrix Analysis::APIDumpAnalysis::Frontends::GlMatrixMath::get_frustum(const double& in_left,
                                                                                                                const double& in_right,
                                                                                                                const double& in_bottom,
                                                                                                                const double& in_top,
                                                                                                                const double& in_near,
                                                                                                                const double& in_far)
{
    const double depth  = in_far   - in_near;
    const double height = in_top   - in_bottom;
    const double width  = in_right - in_left;
    GlMatrix     result = get_identity();

    if (depth  != 0.0 &&
        height != 0.0 &&
        width  != 0.0)
    {
        result = {};

        result.at(0)  = static_cast<float>(2.0 * in_near / width);
        result.at(5)  = static_cast<float>(2.0 * in_near / height);
        result.at(8)  = static_cast<float>( (in_right + in_left)   / width);
        result.at(9)  = static_cast<float>( (in_top   + in_bottom) / height);
        result.at(10) = static_cast<float>(-(in_far   + in_near)   / depth);
        result.at(11) = -1.0f;
        result.at(14) = static_cast<float>(-2.0 * in_far * in_near / depth);
    }

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GlMatrix Analysis::APIDumpAnalysis::Frontends::GlMatrixMath::get_identity()
{
    GlMatrix result = {};

    result.at(0)  = 1.0f;
    result.at(5)  = 1.0f;
    result.at(10) = 1.0f;
    result.at(15) = 1.0f;

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GlMatrix Analysis::APIDumpAnalysis::Frontends::GlMatrixMath::get_ortho(const double& in_left,
                                                                                                              const double& in_right,
                                                                                                              const double& in_bottom,
                                                                                                              const double& in_top,
                                                                                                              const double& in_near,
                                                                                                              const double& in_far)
{
    const double depth  = in_far   - in_near;
    const double height = in_top   - in_bottom;
    const double width  = in_right - in_left;
    GlMatrix     result = get_identity();

    if (depth  != 0.0 &&
        height != 0.0 &&
        width  != 0.0)
    {
        result.at(0)  = static_cast<float>(2.0 / width);
        result.at(5)  = static_cast<float>(2.0 / height);
        result.at(10) = static_cast<float>(-2.0 / depth);
        result.at(12) = static_cast<float>(-(in_right + in_left)   / width);
        result.at(13) = static_cast<float>(-(in_top   + in_bottom) / height);
        result.at(14) = static_cast<float>(-(in_far   + in_near)   / depth);
    }

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GlMatrix Analysis::APIDumpAnalysis::Frontends::GlMatrixMath::get_rotation(const double& in_angle_degrees,
                                                                                                                const double& in_x,
                                                                                                                const double& in_y,
                                                                                                                const double& in_z)
{
    constexpr double DEGREES_PER_HALF_TURN = 180.0;
    constexpr double HALF_TURN_RADIANS     = 3.14159265358979323846;

    const double axis_length = ::sqrt(in_x * in_x + in_y * in_y + in_z * in_z);
    GlMatrix     result      = get_identity();

    if (axis_length != 0.0)
    {
        const double angle       = in_angle_degrees * HALF_TURN_RADIANS / DEGREES_PER_HALF_TURN;
        const double cosine      = ::cos(angle);
        const double one_minus_c = 1.0 - cosine;
        const double sine        = ::sin(angle);
        const double x           = in_x / axis_length;
        const double y           = in_y / axis_length;
        const double z           = in_z / axis_length;

        result.at(0)  = static_cast<float>(x * x * one_minus_c + cosine);
        result.at(1)  = static_cast<float>(y * x * one_minus_c + z * sine);
        result.at(2)  = static_cast<float>(x * z * one_minus_c - y * sine);
        result.at(4)  = static_cast<float>(x * y * one_minus_c - z * sine);
        result.at(5)  = static_cast<float>(y * y * one_minus_c + cosine);
        result.at(6)  = static_cast<float>(y * z * one_minus_c + x * sine);
        result.at(8)  = static_cast<float>(x * z * one_minus_c + y * sine);
        result.at(9)  = static_cast<float>(y * z * one_minus_c - x * sine);
        result.at(10) = static_cast<float>(z * z * one_minus_c + cosine);
    }

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GlMatrix Analysis::APIDumpAnalysis::Frontends::GlMatrixMath::get_scale(const double& in_x,
                                                                                                              const double& in_y,
                                                                                                              const double& in_z)
{
    GlMatrix result = get_identity();

    result.at(0)  = static_cast<float>(in_x);
    result.at(5)  = static_cast<float>(in_y);
    result.at(10) = static_cast<float>(in_z);

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GlMatrix Analysis::APIDumpAnalysis::Frontends::GlMatrixMath::get_translation(const double& in_x,
                                                                                                                    const double& in_y,
                                                                                                                    const double& in_z)
{
    GlMatrix result = get_identity();

    result.at(12) = static_cast<float>(in_x);
    result.at(13) = static_cast<float>(in_y);
    result.at(14) = static_cast<float>(in_z);

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GlMatrix Analysis::APIDumpAnalysis::Frontends::GlMatrixMath::multiply(const GlMatrix& in_left,
                                                                                                             const GlMatrix& in_right)
{
    constexpr size_t N_COLUMNS = 4;

    GlMatrix result = {};

    for (size_t n_column = 0;
                n_column < N_COLUMNS;
              ++n_column)
    {
        for (size_t n_row = 0;
                    n_row < N_COLUMNS;
                  ++n_row)
        {
            float sum = 0.0f;

            for (size_t n_term = 0;
                        n_term < N_COLUMNS;
                      ++n_term)
            {
                sum += in_left.at(n_term * N_COLUMNS + n_row) * in_right.at(n_column * N_COLUMNS + n_term);
            }

            result.at(n_column * N_COLUMNS + n_row) = sum;
        }
    }

    return result;
}
