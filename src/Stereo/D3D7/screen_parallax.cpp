/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/D3D7/screen_parallax.h"
#include <algorithm>
#include <math.h>
#include <string.h>

float Stereo::D3D7::ScreenParallax::get_shift(const Parameters& in_parameters,
                                              const float&      in_reciprocal_depth)
{
    return get_shift(make_shift_factors(in_parameters),
                     in_reciprocal_depth);
}

float Stereo::D3D7::ScreenParallax::get_shift(const ShiftFactors& in_factors,
                                              const float&        in_reciprocal_depth)
{
    /* A vertex behind the camera (a reciprocal of the depth that is not above zero) is no place to have a picture of, and neither is one at the
     * camera (a reciprocal that is huge, or not a number): they are put at the edge of what there is, which is infinity at one end, and a tenth
     * of the convergence distance at the other.
     */
    const float reciprocal = (in_reciprocal_depth == in_reciprocal_depth) ? std::min(std::max(in_reciprocal_depth,
                                                                                              0.0f),
                                                                                     in_factors.max_reciprocal)
                                                                          : 0.0f; /* not a number */
    const float magnitude  = in_factors.magnitude_per_reciprocal * (reciprocal - in_factors.convergence_reciprocal);

    /* A camera that is moved to the left sees what is near further to the right. */
    return (in_factors.is_left) ? magnitude
                                : -magnitude;
}

bool Stereo::D3D7::ScreenParallax::is_valid(const Parameters& in_parameters)
{
    return (in_parameters.convergence_distance >  0.0f &&
            in_parameters.eye_separation       != 0.0f &&
            in_parameters.picture_width        >  0.0f);
}

Stereo::D3D7::ScreenParallax::ShiftFactors Stereo::D3D7::ScreenParallax::make_shift_factors(const Parameters& in_parameters)
{
    constexpr float DEGREES_TO_RADIANS = 3.14159265358979323846f / 180.0f;
    constexpr float MAX_DEPTH_FRACTION = 0.1f;
    constexpr float HALF               = 0.5f;

    ShiftFactors result;

    const float focal_length_pixels = (in_parameters.picture_width * HALF) / ::tanf(in_parameters.field_of_view_degrees * HALF * DEGREES_TO_RADIANS);

    result.convergence_reciprocal   = 1.0f / in_parameters.convergence_distance;
    result.is_left                  = (in_parameters.eye == Analysis::RecipeLoader::Eye::LEFT);
    result.magnitude_per_reciprocal = focal_length_pixels * (in_parameters.eye_separation * HALF);
    result.max_reciprocal           = result.convergence_reciprocal / MAX_DEPTH_FRACTION;

    return result;
}

void Stereo::D3D7::ScreenParallax::scale_vertices(const float&    in_scale,
                                                  const uint32_t& in_vertex_n_bytes,
                                                  const size_t&   in_n_vertices,
                                                  uint8_t*        inout_vertex_ptr)
{
    constexpr size_t N_POSITION_AXES = 2; /* x and y */

    for (size_t n_vertex = 0;
                n_vertex < in_n_vertices;
              ++n_vertex)
    {
        uint8_t* const vertex_ptr = inout_vertex_ptr + n_vertex * in_vertex_n_bytes;

        for (size_t n_axis = 0;
                    n_axis < N_POSITION_AXES;
                  ++n_axis)
        {
            float position = 0.0f;

            ::memcpy(&position,
                     vertex_ptr + n_axis * sizeof(position),
                     sizeof(position) );

            position *= in_scale;

            ::memcpy(vertex_ptr + n_axis * sizeof(position),
                     &position,
                     sizeof(position) );
        }
    }
}

void Stereo::D3D7::ScreenParallax::shift_vertices(const Parameters& in_parameters,
                                                  const uint32_t&   in_vertex_n_bytes,
                                                  const size_t&     in_n_vertices,
                                                  uint8_t*          inout_vertex_ptr)
{
    constexpr size_t RECIPROCAL_DEPTH_OFFSET = 3 * sizeof(float);
    constexpr size_t X_OFFSET                = 0;

    /* The same for every vertex of the draw. */
    const ShiftFactors factors = make_shift_factors(in_parameters);

    for (size_t n_vertex = 0;
                n_vertex < in_n_vertices;
              ++n_vertex)
    {
        uint8_t* const vertex_ptr = inout_vertex_ptr + n_vertex * in_vertex_n_bytes;
        float          reciprocal = 0.0f;
        float          x          = 0.0f;

        ::memcpy(&x,
                 vertex_ptr + X_OFFSET,
                 sizeof(x) );
        ::memcpy(&reciprocal,
                 vertex_ptr + RECIPROCAL_DEPTH_OFFSET,
                 sizeof(reciprocal) );

        x += get_shift(factors,
                       reciprocal);

        ::memcpy(vertex_ptr + X_OFFSET,
                 &x,
                 sizeof(x) );
    }
}

void Stereo::D3D7::ScreenParallax::shift_vertices_by(const float&    in_shift,
                                                     const uint32_t& in_vertex_n_bytes,
                                                     const size_t&   in_n_vertices,
                                                     uint8_t*        inout_vertex_ptr)
{
    constexpr size_t X_OFFSET = 0;

    for (size_t n_vertex = 0;
                n_vertex < in_n_vertices;
              ++n_vertex)
    {
        uint8_t* const vertex_ptr = inout_vertex_ptr + n_vertex * in_vertex_n_bytes;
        float          x          = 0.0f;

        ::memcpy(&x,
                 vertex_ptr + X_OFFSET,
                 sizeof(x) );

        x += in_shift;

        ::memcpy(vertex_ptr + X_OFFSET,
                 &x,
                 sizeof(x) );
    }
}
