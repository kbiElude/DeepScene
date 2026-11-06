/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/D3D7/screen_parallax.h"
#include <algorithm>
#include <math.h>
#include <string.h>

/* How near to an edge of the picture a vertex has to be to be at it, as a part of the width: what the analysis counts as reaching it. */
static constexpr float EDGE_REACH = 0.005f;

float Stereo::D3D7::ScreenParallax::get_shift(const Parameters& in_parameters,
                                              const float&      in_reciprocal_depth)
{
    const ShiftFactors factors = make_shift_factors(in_parameters);

    return get_shift(factors,
                     in_reciprocal_depth);
}

float Stereo::D3D7::ScreenParallax::get_shift(const ShiftFactors& in_factors,
                                              const float&        in_reciprocal_depth)
{
    /* A vertex behind the camera (a reciprocal of the depth that is not above zero) is no place to have a picture of, and neither is one at the
     * camera (a reciprocal that is huge, or not a number): they are put at the edge of what there is, which is infinity at one end, and a tenth
     * of the convergence distance at the other.
     */
    const float above_zero = std::max(in_reciprocal_depth,
                                      0.0f);
    const float reciprocal = (in_reciprocal_depth == in_reciprocal_depth) ? std::min(above_zero,
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
    result.is_left                  = (in_parameters.eye == Analysis::Recipe::Eye::LEFT);
    result.magnitude_per_reciprocal = focal_length_pixels * (in_parameters.eye_separation * HALF);
    result.max_reciprocal           = result.convergence_reciprocal / MAX_DEPTH_FRACTION;

    return result;
}

bool Stereo::D3D7::ScreenParallax::reaches_both_sides(const uint8_t*  in_vertex_ptr,
                                                      const uint32_t& in_vertex_n_bytes,
                                                      const size_t&   in_n_vertices,
                                                      const uint16_t* in_index_ptr,
                                                      const size_t&   in_n_indices,
                                                      const float&    in_picture_width)
{
    const size_t n_used   = (in_index_ptr != nullptr) ? in_n_indices
                                                      : in_n_vertices;
    bool         has_used = false;
    float        x_max    = 0.0f;
    float        x_min    = 0.0f;

    for (size_t n_use = 0;
                n_use < n_used;
              ++n_use)
    {
        const size_t n_vertex = (in_index_ptr != nullptr) ? in_index_ptr[n_use]
                                                          : n_use;

        if (n_vertex < in_n_vertices)
        {
            float x = 0.0f;

            ::memcpy(&x,
                     in_vertex_ptr + n_vertex * in_vertex_n_bytes,
                     sizeof(x) );

            if (x == x) /* a number */
            {
                x_max    = (has_used == false || x > x_max) ? x : x_max;
                x_min    = (has_used == false || x < x_min) ? x : x_min;
                has_used = true;
            }
        }
    }

    return (has_used                                                  &&
            x_min <= EDGE_REACH * in_picture_width                    &&
            x_max >= (1.0f - EDGE_REACH) * in_picture_width);
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
    const float        edge_n_pixels = EDGE_REACH * in_parameters.picture_width;
    const float        right_edge_x  = in_parameters.picture_width - edge_n_pixels;
    const ShiftFactors factors       = make_shift_factors(in_parameters);

    for (size_t n_vertex = 0;
                n_vertex < in_n_vertices;
              ++n_vertex)
    {
        uint8_t* const vertex_ptr = inout_vertex_ptr + n_vertex * in_vertex_n_bytes;
        float reciprocal = 0.0f;
        float x          = 0.0f;

        ::memcpy(&x,
                 vertex_ptr + X_OFFSET,
                 sizeof(x) );
        ::memcpy(&reciprocal,
                 vertex_ptr + RECIPROCAL_DEPTH_OFFSET,
                 sizeof(reciprocal) );

        const float shift = get_shift(factors,
                                      reciprocal);

        /* A vertex at an edge of the picture which the shift would take from it is left there: what the polygon covered up to the edge it covers
         * still, and no gap of the colour behind it opens (where the ground ends at the side of the picture, for one).
         */
        const bool is_held_at_left  = (x     <= edge_n_pixels &&
                                       shift >  0.0f);
        const bool is_held_at_right = (x     >= right_edge_x  &&
                                       shift <  0.0f);

        if (is_held_at_left  == false &&
            is_held_at_right == false)
        {
            x += shift;
        }

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
