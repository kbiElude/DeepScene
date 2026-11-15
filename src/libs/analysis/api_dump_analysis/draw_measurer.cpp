/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/draw_measurer.h"
#include <algorithm>
#include <math.h>

std::array<float, 12> Analysis::APIDumpAnalysis::DrawMeasurer::compute_model_view(const DrawMatrices& in_matrices)
{
    constexpr size_t N_COLUMNS      = 4;
    constexpr size_t N_VIEW_COLUMNS = 3; /* x, y and the depth */

    std::array<float, N_COLUMNS * N_VIEW_COLUMNS> result_f32mat;

    for (size_t n_row = 0;
                n_row < N_COLUMNS;
              ++n_row)
    {
        for (size_t n_column = 0;
                    n_column < N_VIEW_COLUMNS;
                  ++n_column)
        {
            float sum = 0.0f;

            for (size_t n_term = 0;
                        n_term < N_COLUMNS;
                      ++n_term)
            {
                sum += in_matrices.model_f32mat.at(n_row * N_COLUMNS + n_term) * in_matrices.view_f32mat.at(n_term * N_COLUMNS + n_column);
            }

            result_f32mat.at(n_row * N_VIEW_COLUMNS + n_column) = sum;
        }
    }

    return result_f32mat;
}

bool Analysis::APIDumpAnalysis::DrawMeasurer::find_draw_matrices(const ConstantStateMap& in_constant_state_map,
                                                                 DrawMatrices*           out_matrices_ptr)
{
    constexpr size_t N_MATRIX_ELEMENTS = 16;
    constexpr size_t PROJECTION_WW     = 15; /* the last element: 1 in an orthographic projection, 0 in a perspective one */

    bool is_orthographic = false;
    bool has_view        = false;
    bool is_model_set    = false;

    *out_matrices_ptr = DrawMatrices();

    /* The first model matrix in the constant map order is the one the draw uses. */
    for (const auto& state_pair : in_constant_state_map)
    {
        const auto& update = state_pair.second.update;

        if (update.value_u8_vec.size() == N_MATRIX_ELEMENTS * sizeof(float) )
        {
            switch (update.semantic)
            {
                case ConstantSemantic::MODEL_MATRIX:
                {
                    if (is_model_set == false)
                    {
                        ::memcpy(out_matrices_ptr->model_f32mat.data(),
                                 update.value_u8_vec.data(),
                                 sizeof(out_matrices_ptr->model_f32mat) );

                        is_model_set = true;
                    }

                    break;
                }

                case ConstantSemantic::PROJECTION_MATRIX:
                {
                    ::memcpy(out_matrices_ptr->projection_f32mat.data(),
                             update.value_u8_vec.data(),
                             sizeof(out_matrices_ptr->projection_f32mat) );

                    out_matrices_ptr->has_projection = true;

                    /* An orthographic projection does not shrink distant objects, so there is no depth to converge at. */
                    is_orthographic = (out_matrices_ptr->projection_f32mat.at(PROJECTION_WW) != 0.0f);

                    break;
                }

                /* OpenGL has a single matrix for model and view together. It is stored as the view matrix. The model matrix then stays the
                 * identity, so the combined matrix takes a vertex all the way to the camera.
                 */
                case ConstantSemantic::MODEL_VIEW_MATRIX:
                case ConstantSemantic::VIEW_MATRIX:
                {
                    ::memcpy(out_matrices_ptr->view_f32mat.data(),
                             update.value_u8_vec.data(),
                             sizeof(out_matrices_ptr->view_f32mat) );

                    has_view = true;

                    break;
                }

                default:
                {
                    break;
                }
            }
        }
    }

    /* The identity is the projection of a program that sets none, or that draws in the space of the picture. It carries no information, so it is
     * never reported as orthographic.
     */
    const std::array<float, N_MATRIX_ELEMENTS> identity_f32mat = {1.0f, 0.0f, 0.0f, 0.0f,
                                                                  0.0f, 1.0f, 0.0f, 0.0f,
                                                                  0.0f, 0.0f, 1.0f, 0.0f,
                                                                  0.0f, 0.0f, 0.0f, 1.0f};

    /* An orthographic projection does not derive w from the depth: elements 3, 7 and 11 are zero and element 15 is nonzero.
     * The identity matches this pattern too, so it is excluded.
     */
    const auto& projection_f32mat = out_matrices_ptr->projection_f32mat;

    out_matrices_ptr->has_view        = has_view;
    out_matrices_ptr->is_perspective  = (out_matrices_ptr->has_projection &&
                                         is_perspective_projection(projection_f32mat) );
    out_matrices_ptr->is_orthographic = (out_matrices_ptr->has_projection &&
                                         projection_f32mat.at(3)  == 0.0f &&
                                         projection_f32mat.at(7)  == 0.0f &&
                                         projection_f32mat.at(11) == 0.0f &&
                                         projection_f32mat.at(15) != 0.0f &&
                                         projection_f32mat       != identity_f32mat);

    return (has_view        &&
            is_orthographic == false);
}

bool Analysis::APIDumpAnalysis::DrawMeasurer::is_perspective_projection(const std::array<float, 16>& in_projection_f32mat)
{
    constexpr size_t PROJECTION_WW = 15; /* 1: orthographic, 0: perspective */
    constexpr size_t PROJECTION_WZ = 11; /* nonzero (1 or -1): perspective */

    return (in_projection_f32mat.at(PROJECTION_WZ) != 0.0f &&
            in_projection_f32mat.at(PROJECTION_WW) == 0.0f);
}

void Analysis::APIDumpAnalysis::DrawMeasurer::record_draw_depth(const DrawDescription&  in_draw,
                                                                const ConstantStateMap& in_constant_state_map,
                                                                PassDepth*              inout_depth_ptr)
{
    constexpr size_t DEPTH_COLUMN = 2;
    constexpr size_t N_COLUMNS    = 4;
    constexpr size_t ORIGIN_ROW   = 3;

    DrawMatrices matrices;
    const bool   has_depth = find_draw_matrices(in_constant_state_map,
                                                &matrices);

    if (has_depth)
    {
        const auto& model_f32mat = matrices.model_f32mat;
        const auto& view_f32mat  = matrices.view_f32mat;

        /* The origin of the object, in the space of the camera: the translation of the model matrix, through the view matrix. */
        const float view_z = model_f32mat.at(ORIGIN_ROW * N_COLUMNS + 0) * view_f32mat.at(0 * N_COLUMNS + DEPTH_COLUMN) +
                             model_f32mat.at(ORIGIN_ROW * N_COLUMNS + 1) * view_f32mat.at(1 * N_COLUMNS + DEPTH_COLUMN) +
                             model_f32mat.at(ORIGIN_ROW * N_COLUMNS + 2) * view_f32mat.at(2 * N_COLUMNS + DEPTH_COLUMN) +
                             view_f32mat.at (ORIGIN_ROW * N_COLUMNS                                                     + DEPTH_COLUMN);

        /* A right-handed camera looks down minus z, so what it sees has a negative z. */
        const float depth = (in_draw.is_camera_right_handed) ? -view_z
                                                             : view_z;

        inout_depth_ptr->depth_histogram.add(depth,
                                             in_draw.vertex_count);

        /* The x scale of a perspective projection says how wide the picture is. An orthographic projection has no depth at which to measure the width. */
        constexpr size_t PROJECTION_SCALE_X = 0;
        constexpr size_t PROJECTION_WW      = 15;

        if (matrices.has_projection                                          &&
            matrices.projection_f32mat.at(PROJECTION_WW)      == 0.0f       &&
            matrices.projection_f32mat.at(PROJECTION_SCALE_X) >  0.0f       &&
            in_draw.vertex_count                              >  0)
        {
            auto&        total      = inout_depth_ptr->projection_scale;
            const double weight_sum = static_cast<double>(total.weight) + static_cast<double>(in_draw.vertex_count);

            total.mean    = static_cast<float>( (static_cast<double>(total.mean) * static_cast<double>(total.weight) + static_cast<double>(matrices.projection_f32mat.at(PROJECTION_SCALE_X) ) * static_cast<double>(in_draw.vertex_count) ) / weight_sum);
            total.weight += in_draw.vertex_count;
        }
    }
}

void Analysis::APIDumpAnalysis::DrawMeasurer::record_draw_facts(const DrawDescription&      in_draw,
                                                                const std::set<ResourceId>& in_color_copy_target_set,
                                                                const bool&                 in_keeps_history,
                                                                const ConstantStateMap&     in_constant_state_map,
                                                                const Viewport&             in_viewport,
                                                                bool*                       inout_has_seen_3d_draw_ptr,
                                                                DrawFacts*                  inout_facts_ptr)
{
    constexpr size_t DEPTH_COLUMN   = 2;
    constexpr size_t N_VIEW_COLUMNS = 3; /* x, y and the depth */
    constexpr size_t ORIGIN_ROW     = 3; /* the translation is the last row */

    DrawFacts&   facts             = *inout_facts_ptr;
    bool         has_place         = false;
    bool         has_vertex_behind = false; /* a vertex is behind the camera: a sky, not eye space */
    bool         is_3d             = false;
    bool         is_eye_space      = false;
    bool         is_flat_layer     = false;
    DrawMatrices matrices;
    float        origin_depth      = 0.0f;
    float        view_depth        = 0.0f;
    float        x_max             = -1.0e30f;
    float        x_min             = 1.0e30f;
    float        y_max             = -1.0e30f;
    float        y_min             = 1.0e30f;
    float        z_max             = -1.0e30f;
    float        z_min             = 1.0e30f;

    facts.n_draws += 1;

    if (*inout_has_seen_3d_draw_ptr)
    {
        facts.n_draws_after_3d_draw += 1;
    }

    if (in_draw.reads_color_attachments)
    {
        facts.n_blended_draws += 1;
    }

    for (const auto& binding : in_draw.texture_binding_vec)
    {
        if (in_color_copy_target_set.count(binding.view.resource_id) != 0)
        {
            facts.n_draws_sampling_copied_color += 1;

            break;
        }
    }

    if (in_keeps_history                      == false   ||
        in_draw.geometry_ptr                  == nullptr ||
        in_draw.geometry_ptr->position_vec.empty() )
    {
        goto end;
    }

    if (in_draw.geometry_ptr->is_pre_transformed)
    {
        is_flat_layer = true;

        /* The positions are in pixels. The third coordinate is the reciprocal of the depth. */
        if (in_viewport.width  > 0.0f &&
            in_viewport.height > 0.0f)
        {
            for (const auto& position : in_draw.geometry_ptr->position_vec)
            {
                const float x = 2.0f * (position[0] - in_viewport.x) / in_viewport.width  - 1.0f;
                const float y = 2.0f * (position[1] - in_viewport.y) / in_viewport.height - 1.0f;

                x_max = (x > x_max) ? x : x_max;
                x_min = (x < x_min) ? x : x_min;
                y_max = (y > y_max) ? y : y_max;
                y_min = (y < y_min) ? y : y_min;

                if (position[2] > 0.0f)
                {
                    view_depth = 1.0f / position[2];
                    z_max      = (view_depth > z_max) ? view_depth : z_max;
                    z_min      = (view_depth < z_min) ? view_depth : z_min;
                }

                has_place = true;
            }
        }
    }
    else
    {
        find_draw_matrices(in_constant_state_map,
                           &matrices);

        is_flat_layer = matrices.is_orthographic;

        if (matrices.has_projection &&
            matrices.has_view)
        {
            const auto   model_view_f32mat = compute_model_view(matrices);
            const float* projection        = matrices.projection_f32mat.data();

            /* The depth of the object origin is the z of the translation of the model-view matrix. */
            const float origin_view_z = model_view_f32mat.at(ORIGIN_ROW * N_VIEW_COLUMNS + DEPTH_COLUMN);

            origin_depth = (in_draw.is_camera_right_handed) ? -origin_view_z
                                                            :  origin_view_z;
            is_3d        = matrices.is_perspective;

            for (const auto& position : in_draw.geometry_ptr->position_vec)
            {
                float view_position[N_VIEW_COLUMNS];

                for (size_t n_column = 0;
                            n_column < N_VIEW_COLUMNS;
                          ++n_column)
                {
                    view_position[n_column] = position[0] * model_view_f32mat.at(0 * N_VIEW_COLUMNS + n_column) +
                                              position[1] * model_view_f32mat.at(1 * N_VIEW_COLUMNS + n_column) +
                                              position[2] * model_view_f32mat.at(2 * N_VIEW_COLUMNS + n_column) +
                                                            model_view_f32mat.at(3 * N_VIEW_COLUMNS + n_column);
                }

                /* Projects the vertex. Dividing by w makes a perspective projection shrink objects with distance. In an orthographic projection w is 1. */
                const float w = view_position[0] * projection[3]  +
                                view_position[1] * projection[7]  +
                                view_position[2] * projection[11] +
                                                   projection[15];

                if (w > 0.0f)
                {
                    const float x = (view_position[0] * projection[0] + view_position[1] * projection[4] + view_position[2] * projection[8] + projection[12]) / w;
                    const float y = (view_position[0] * projection[1] + view_position[1] * projection[5] + view_position[2] * projection[9] + projection[13]) / w;

                    x_max = (x > x_max) ? x : x_max;
                    x_min = (x < x_min) ? x : x_min;
                    y_max = (y > y_max) ? y : y_max;
                    y_min = (y < y_min) ? y : y_min;

                    view_depth = (in_draw.is_camera_right_handed) ? -view_position[2]
                                                                  :  view_position[2];
                    z_max      = (view_depth > z_max) ? view_depth : z_max;
                    z_min      = (view_depth < z_min) ? view_depth : z_min;

                    has_place = true;
                }
                else
                {
                    has_vertex_behind = true;
                }
            }
        }
    }

    if (is_flat_layer)
    {
        facts.n_draws_flat_layer += 1;
    }

    if (has_place)
    {
        constexpr float EYE_SPACE_DEPTH_RATIO = 20.0f; /* origin depth must be this many times below vertex depth */
        constexpr float PICTURE_EXTENT        = 2.0f;  /* edges are at -1 and 1 */

        /* Eye space: the object origin is at the camera, but its vertices are not. */
        is_eye_space = (is_3d                                                 &&
                        has_vertex_behind                             == false &&
                        z_min                                          > 0.0f &&
                        ::fabsf(origin_depth) * EYE_SPACE_DEPTH_RATIO < z_min);

        /* The part of the picture that the bounding box covers. Whatever lies outside the edges is not part of the picture. */
        const float x_inside_max = std::min(x_max,
                                            1.0f);
        const float x_inside_min = std::max(x_min,
                                            -1.0f);
        const float y_inside_max = std::min(y_max,
                                            1.0f);
        const float y_inside_min = std::max(y_min,
                                            -1.0f);

        const float box_width  = std::max(0.0f,
                                          x_inside_max - x_inside_min) / PICTURE_EXTENT;
        const float box_height = std::max(0.0f,
                                          y_inside_max - y_inside_min) / PICTURE_EXTENT;
        const float fraction   = box_width * box_height;
        const bool  is_first   = (facts.n_draws_with_footprint == 0);

        facts.area_fraction_max = (is_first || fraction > facts.area_fraction_max) ? fraction
                                                                                   : facts.area_fraction_max;
        facts.area_fraction_min = (is_first || fraction < facts.area_fraction_min) ? fraction
                                                                                   : facts.area_fraction_min;
        facts.view_depth_max    = (is_first || z_max     > facts.view_depth_max)   ? z_max
                                                                                   : facts.view_depth_max;
        facts.view_depth_min    = (is_first || z_min     < facts.view_depth_min)   ? z_min
                                                                                   : facts.view_depth_min;
        facts.x_max             = (is_first || x_max     > facts.x_max)            ? x_max
                                                                                   : facts.x_max;
        facts.x_min             = (is_first || x_min     < facts.x_min)            ? x_min
                                                                                   : facts.x_min;
        facts.y_max             = (is_first || y_max     > facts.y_max)            ? y_max
                                                                                   : facts.y_max;
        facts.y_min             = (is_first || y_min     < facts.y_min)            ? y_min
                                                                                   : facts.y_min;

        facts.n_draws_with_footprint += 1;

        if (is_eye_space)
        {
            facts.n_draws_eye_space += 1;
        }

        if (is_3d)
        {
            *inout_has_seen_3d_draw_ptr = true;
        }
    }

end:
    return;
}

Analysis::APIDumpAnalysis::ViewCoverage Analysis::APIDumpAnalysis::DrawMeasurer::record_draw_surface_depth(const DrawDescription&  in_draw,
                                                                                                           const ConstantStateMap& in_constant_state_map,
                                                                                                           PassDepth*              inout_depth_ptr)
{
    /* How close a vertex must be to an edge of the picture for the draw to count as reaching it, in picture units (the edges are at -1 and 1). */
    constexpr float EDGE_REACH = 0.995f;

    ViewCoverage result = ViewCoverage::UNKNOWN;

    constexpr size_t N_VIEW_COLUMNS    = 3; /* x, y and the depth */
    constexpr size_t PROJECTION_SCALE_X = 0;
    constexpr size_t PROJECTION_SCALE_Y = 5;
    constexpr size_t PROJECTION_Z_SCALE = 10;
    constexpr size_t PROJECTION_Z_SHIFT = 14;

    DrawMatrices matrices;

    if (in_draw.geometry_ptr != nullptr)
    {
        if (in_draw.geometry_ptr->position_vec.empty() == false)
        {
            const bool has_depth = find_draw_matrices(in_constant_state_map,
                                                      &matrices);

            if (has_depth)
            {
                const auto&    position_vec = in_draw.geometry_ptr->position_vec;
                const float*   projection   = matrices.projection_f32mat.data();
                const uint64_t weight       = std::max<uint64_t>(1,
                                                                 in_draw.vertex_count / position_vec.size() );

                /* The nearest depth that is drawn, taken from the projection. D3D puts the near plane at minus the z shift over the z scale.
                 * OpenGL puts it at the z shift over the z scale, less one (its z scale is (f + n) / (n - f) and its z shift is 2fn / (n - f)).
                 */
                float near_depth = 0.0f;

                if (matrices.has_projection)
                {
                    if (in_draw.is_camera_right_handed)
                    {
                        const float scale_less_one = projection[PROJECTION_Z_SCALE] - 1.0f;

                        near_depth = (scale_less_one != 0.0f) ? projection[PROJECTION_Z_SHIFT] / scale_less_one
                                                              : 0.0f;
                    }
                    else
                    if (projection[PROJECTION_Z_SCALE] != 0.0f)
                    {
                        near_depth = -projection[PROJECTION_Z_SHIFT] / projection[PROJECTION_Z_SCALE];
                    }
                }

                const auto model_view_f32mat = compute_model_view(matrices);

                float x_max = -1.0e30f;
                float x_min = 1.0e30f;
                float y_max = -1.0e30f;
                float y_min = 1.0e30f;

                for (const auto& position : position_vec)
                {
                    float view_position[N_VIEW_COLUMNS];

                    for (size_t n_column = 0;
                                n_column < N_VIEW_COLUMNS;
                              ++n_column)
                    {
                        view_position[n_column] = position[0] * model_view_f32mat.at(0 * N_VIEW_COLUMNS + n_column) +
                                                  position[1] * model_view_f32mat.at(1 * N_VIEW_COLUMNS + n_column) +
                                                  position[2] * model_view_f32mat.at(2 * N_VIEW_COLUMNS + n_column) +
                                                                model_view_f32mat.at(3 * N_VIEW_COLUMNS + n_column);
                    }

                    /* A right-handed camera looks down minus z, so what it sees has a negative z. */
                    const float depth = (in_draw.is_camera_right_handed) ? -view_position[2]
                                                                         : view_position[2];

                    /* Tracks where the vertex lies in the picture, whether or not it is in view: a draw that reaches past an edge covers that edge. */
                    if (depth > 0.0f && matrices.has_projection)
                    {
                        const float x = view_position[0] * projection[PROJECTION_SCALE_X] / depth;
                        const float y = view_position[1] * projection[PROJECTION_SCALE_Y] / depth;

                        x_max = (x > x_max) ? x : x_max;
                        x_min = (x < x_min) ? x : x_min;
                        y_max = (y > y_max) ? y : y_max;
                        y_min = (y < y_min) ? y : y_min;
                    }

                    /* A vertex counts as seen if it is in front of the camera and inside the view. Vertices behind the camera or off to the side are not seen. */
                    bool is_seen = (depth > 0.0f && depth >= near_depth);

                    if (is_seen && matrices.has_projection)
                    {
                        const float x_extent = ::fabsf(view_position[0] * projection[PROJECTION_SCALE_X]);
                        const float y_extent = ::fabsf(view_position[1] * projection[PROJECTION_SCALE_Y]);

                        is_seen = (x_extent <= depth &&
                                   y_extent <= depth);
                    }

                    if (is_seen)
                    {
                        inout_depth_ptr->surface_depth_histogram.add(depth,
                                                                     weight);
                    }
                }

                /* If no vertex is in front of the camera, nothing can be said about the view and the coverage stays UNKNOWN. */
                if (x_min <= x_max)
                {
                    result = (x_min <= -EDGE_REACH &&
                              x_max >=  EDGE_REACH &&
                              y_min <= -EDGE_REACH &&
                              y_max >=  EDGE_REACH) ? ViewCoverage::COVERING
                                                    : ViewCoverage::PARTIAL;
                }
            }
        }
    }

    return result;
}

Analysis::APIDumpAnalysis::ViewCoverage Analysis::APIDumpAnalysis::DrawMeasurer::record_screen_depth(const DrawDescription& in_draw,
                                                                                                     const Viewport&        in_viewport,
                                                                                                     PassDepth*             inout_depth_ptr)
{
    /* How close a vertex must be to an edge of the viewport for the draw to count as reaching it, as a fraction of the viewport size. */
    constexpr float EDGE_REACH = 0.005f;

    /* The largest spread of the reciprocal depth across a draw for the draw to count as flat, as a fraction of the largest reciprocal depth. */
    constexpr float FLAT_DEPTH_SPREAD = 0.001f;

    ViewCoverage       result               = ViewCoverage::UNKNOWN;
    float              reciprocal_depth_max = 0.0f;
    float              reciprocal_depth_min = 1.0e30f;
    const auto&        position_vec         = in_draw.geometry_ptr->position_vec;
    const uint64_t     weight               = std::max<uint64_t>(1,
                                                                 in_draw.vertex_count / std::max<size_t>(1,
                                                                                                          position_vec.size() ) );
    const bool         has_viewport         = (in_viewport.width > 0.0f && in_viewport.height > 0.0f);
    std::vector<float> seen_depth_vec;
    float              x_max                = -1.0e30f;
    float              x_min                = 1.0e30f;
    float              y_max                = -1.0e30f;
    float              y_min                = 1.0e30f;

    for (const auto& position : position_vec)
    {
        const float reciprocal_depth = position[2];

        /* A vertex that is not in front of the camera has no depth and is not part of the view. */
        if (reciprocal_depth > 0.0f)
        {
            const float depth      = 1.0f / reciprocal_depth;
            const bool  is_in_view = (has_viewport == false                               ||
                                      (position[0] >= in_viewport.x                       &&
                                       position[0] <= in_viewport.x + in_viewport.width   &&
                                       position[1] >= in_viewport.y                       &&
                                       position[1] <= in_viewport.y + in_viewport.height) );

            x_max = (position[0] > x_max) ? position[0] : x_max;
            x_min = (position[0] < x_min) ? position[0] : x_min;
            y_max = (position[1] > y_max) ? position[1] : y_max;
            y_min = (position[1] < y_min) ? position[1] : y_min;

            reciprocal_depth_max = (reciprocal_depth > reciprocal_depth_max) ? reciprocal_depth : reciprocal_depth_max;
            reciprocal_depth_min = (reciprocal_depth < reciprocal_depth_min) ? reciprocal_depth : reciprocal_depth_min;

            if (is_in_view)
            {
                inout_depth_ptr->surface_depth_histogram.add(depth,
                                                             weight);

                seen_depth_vec.push_back(depth);
            }
        }
    }

    /* Other draws are counted at the depth of their object origin, which comes from the matrices. Here the draw is counted at the median depth of its seen vertices. */
    if (seen_depth_vec.empty() == false)
    {
        std::sort(seen_depth_vec.begin(),
                  seen_depth_vec.end() );

        inout_depth_ptr->depth_histogram.add(seen_depth_vec.at(seen_depth_vec.size() / 2),
                                             in_draw.vertex_count);
    }

    if (x_min <= x_max &&
        has_viewport)
    {
        const bool reaches_sides = (x_min <= in_viewport.x                      + EDGE_REACH * in_viewport.width &&
                                    x_max >= in_viewport.x + in_viewport.width  - EDGE_REACH * in_viewport.width);
        const bool reaches_ends  = (y_min <= in_viewport.y                      + EDGE_REACH * in_viewport.height &&
                                    y_max >= in_viewport.y + in_viewport.height - EDGE_REACH * in_viewport.height);

        /* The sides matter to the eyes: when a picture is shifted sideways, a draw that does not reach both sides leaves a bare strip at one of them.
         * A flat quad (all at one depth, such as a backdrop, a flash or a fade) that spans from side to side covers the picture, whether or not the
         * picture has bars above and below. A mesh that spans from side to side, such as a floor, does not cover the picture.
         */
        const bool is_flat        = (reciprocal_depth_max - reciprocal_depth_min <= FLAT_DEPTH_SPREAD * reciprocal_depth_max);

        result = (reaches_sides               &&
                  (reaches_ends || is_flat) ) ? ViewCoverage::COVERING
                                              : ViewCoverage::PARTIAL;
    }

    return result;
}
