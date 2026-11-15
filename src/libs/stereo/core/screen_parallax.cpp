/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/screen_parallax.h"
#include <algorithm>
#include <math.h>
#include <string.h>

/* How close to an edge of the picture a vertex must be to count as at the edge, as a fraction of the picture width. The analysis uses the
 * same distance to decide whether a draw reaches an edge.
 */
static constexpr float EDGE_REACH = 0.005f;

bool Stereo::Core::ScreenParallax::apply_plan(const ScreenShiftPlan& in_plan,
                                              const uint32_t&        in_vertex_n_bytes,
                                              const size_t&          in_n_vertices,
                                              uint8_t*               inout_vertex_ptr)
{
    bool result = false;

    switch (in_plan.kind)
    {
        case ScreenShiftKind::PER_VERTEX:
        {
            const Parameters parameters = make_parameters(in_plan);

            if (is_valid(parameters) )
            {
                shift_vertices(parameters,
                               in_vertex_n_bytes,
                               in_n_vertices,
                               inout_vertex_ptr);

                result = true;
            }

            break;
        }

        case ScreenShiftKind::UNIFORM:
        {
            shift_vertices_by(in_plan.uniform_shift_pixels,
                              in_vertex_n_bytes,
                              in_n_vertices,
                              inout_vertex_ptr);

            result = true;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

float Stereo::Core::ScreenParallax::get_shift(const Parameters& in_parameters,
                                              const float&      in_reciprocal_depth)
{
    const ShiftFactors factors = make_shift_factors(in_parameters);

    return get_shift(factors,
                     in_reciprocal_depth);
}

float Stereo::Core::ScreenParallax::get_shift(const ShiftFactors& in_factors,
                                              const float&        in_reciprocal_depth)
{
    /* A reciprocal depth that is not above 0 means the vertex is at infinity or behind the camera. A reciprocal that is huge or not a number
     * means the vertex is at the camera. Neither position has a meaningful shift, so the reciprocal is limited to the range from 0 (infinity)
     * to max_reciprocal (a tenth of the convergence distance).
     */
    const float above_zero = std::max(in_reciprocal_depth,
                                      0.0f);
    const float reciprocal = (in_reciprocal_depth == in_reciprocal_depth) ? std::min(above_zero,
                                                                                     in_factors.max_reciprocal)
                                                                          : 0.0f; /* not a number */
    const float magnitude  = in_factors.magnitude_per_reciprocal * (reciprocal - in_factors.convergence_reciprocal);

    /* A camera moved to the left sees near objects further to the right. */
    return (in_factors.is_left) ?  magnitude
                                : -magnitude;
}

bool Stereo::Core::ScreenParallax::is_valid(const Parameters& in_parameters)
{
    return (in_parameters.convergence_distance >  0.0f &&
            in_parameters.eye_separation       != 0.0f &&
            in_parameters.picture_width        >  0.0f);
}

Stereo::Core::ScreenParallax::Parameters Stereo::Core::ScreenParallax::make_parameters(const ScreenShiftPlan& in_plan)
{
    Parameters result;

    result.convergence_distance  = in_plan.convergence_distance;
    result.eye                   = in_plan.eye;
    result.eye_separation        = in_plan.eye_separation;
    result.field_of_view_degrees = in_plan.field_of_view_degrees;
    result.picture_width         = in_plan.picture_width;

    return result;
}

Stereo::Core::ScreenParallax::ShiftFactors Stereo::Core::ScreenParallax::make_shift_factors(const Parameters& in_parameters)
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

bool Stereo::Core::ScreenParallax::reaches_both_sides(const uint8_t*  in_vertex_ptr,
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

    return (has_used                                            &&
            x_min    <= EDGE_REACH          * in_picture_width  &&
            x_max    >= (1.0f - EDGE_REACH) * in_picture_width);
}

void Stereo::Core::ScreenParallax::scale_vertices(const float&    in_scale,
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

void Stereo::Core::ScreenParallax::shift_vertices(const Parameters& in_parameters,
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
        float          reciprocal = 0.0f;
        float          x          = 0.0f;

        ::memcpy(&x,
                 vertex_ptr + X_OFFSET,
                 sizeof(x) );
        ::memcpy(&reciprocal,
                 vertex_ptr + RECIPROCAL_DEPTH_OFFSET,
                 sizeof(reciprocal) );

        const float shift = get_shift(factors,
                                      reciprocal);

        /* A vertex at an edge of the picture is not moved away from that edge. The polygon still covers up to the edge, and no gap that shows
         * the color behind it opens (for example where the ground ends at the side of the picture).
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

void Stereo::Core::ScreenParallax::shift_vertices_by(const float&    in_shift,
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
