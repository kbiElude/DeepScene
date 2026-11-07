/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_commands.h"
#include "analysis/api_dump_analysis/depth_histogram.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "analysis/api_dump_analysis/timeline_builder.h"
#include "common/macros.h"
#include <algorithm>
#include <set>

bool Analysis::APIDumpAnalysis::QueryCommands::get_first_frame_index(const AnalysisResult& in_analysis,
                                                                     const uint32_t&       in_pass_id,
                                                                     uint64_t*             out_frame_index_ptr)
{
    const auto& model_ref = in_analysis.model;
    bool        result    = false;

    /* The frames are in order, and a frame has the passes of its shape. */
    for (const auto& frame_record : model_ref.frame_record_vec)
    {
        const auto& shape = model_ref.frame_shape_interner.get_structure(frame_record.shape_id);

        if (std::binary_search(shape.pass_structure_id_vec.begin(),
                               shape.pass_structure_id_vec.end  (),
                               in_pass_id) )
        {
            *out_frame_index_ptr = frame_record.frame_index;
            result               = true;

            break;
        }
    }

    return result;
}

Analysis::APIDumpAnalysis::PassDepthSpread Analysis::APIDumpAnalysis::QueryCommands::get_pass_depth_spread(const AnalysisResult& in_analysis,
                                                                                                           const uint32_t&       in_pass_id)
{
    constexpr double FRACTION_FAR    = 0.9;
    constexpr double FRACTION_MEDIAN = 0.5;
    constexpr double FRACTION_NEAR   = 0.1;
    constexpr float  MAX_FLAT_RATIO  = 1.1f; /* a range is 9% deeper than the one before: flat is one range, or two next to each other */

    const auto&     model_ref         = in_analysis.model;
    DepthHistogram  origin_histogram;
    PassDepthSpread result;
    DepthHistogram  surface_histogram;

    for (const auto& histogram_pair : model_ref.depth_histogram_map)
    {
        if (histogram_pair.first.second == in_pass_id)
        {
            origin_histogram.merge(histogram_pair.second);
        }
    }

    for (const auto& histogram_pair : model_ref.surface_depth_histogram_map)
    {
        if (histogram_pair.first.second == in_pass_id)
        {
            surface_histogram.merge(histogram_pair.second);
        }
    }

    /* The vertices of the draws say more than the origins of their objects, which may be anywhere. */
    const auto& histogram = (surface_histogram.is_empty() ) ? origin_histogram
                                                            : surface_histogram;

    if (histogram.is_empty() == false)
    {
        result.depth_far    = histogram.get_depth_at_fraction(FRACTION_FAR);
        result.depth_median = histogram.get_depth_at_fraction(FRACTION_MEDIAN);
        result.depth_near   = histogram.get_depth_at_fraction(FRACTION_NEAR);
        result.has_depth    = (result.depth_near > 0.0f);
        result.n_vertices   = histogram.get_total_weight();

        if (result.has_depth)
        {
            result.far_to_near_ratio = result.depth_far / result.depth_near;
            result.is_flat           = (result.far_to_near_ratio <= MAX_FLAT_RATIO);
        }
    }

    return result;
}

const char* Analysis::APIDumpAnalysis::QueryCommands::get_primitive_topology_name(const PrimitiveTopology& in_topology)
{
    const char* result_ptr = "unknown";

    switch (in_topology)
    {
        case PrimitiveTopology::LINE_LIST:
        {
            result_ptr = "line_list";

            break;
        }

        case PrimitiveTopology::LINE_STRIP:
        {
            result_ptr = "line_strip";

            break;
        }

        case PrimitiveTopology::POINT_LIST:
        {
            result_ptr = "point_list";

            break;
        }

        case PrimitiveTopology::TRIANGLE_FAN:
        {
            result_ptr = "triangle_fan";

            break;
        }

        case PrimitiveTopology::TRIANGLE_LIST:
        {
            result_ptr = "triangle_list";

            break;
        }

        case PrimitiveTopology::TRIANGLE_STRIP:
        {
            result_ptr = "triangle_strip";

            break;
        }

        case PrimitiveTopology::UNKNOWN:
        {
            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_topology);

            break;
        }
    }

    return result_ptr;
}

std::vector<uint32_t> Analysis::APIDumpAnalysis::QueryCommands::get_static_camera_pass_id_vec(const AnalysisResult& in_analysis)
{
    const auto&           model_ref  = in_analysis.model;
    std::vector<uint32_t> result_vec;

    /* The camera of a pass which never changes is the sign of a pass that is drawn in screen space. */
    for (uint32_t n_pass = 0;
                  n_pass < model_ref.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        const auto& structure         = model_ref.pass_structure_interner.get_structure(n_pass);
        bool        has_camera        = false;
        bool        is_camera_changed = false;

        for (size_t n_constant = 0;
                    n_constant < structure.constant_signature_vec.size();
                  ++n_constant)
        {
            const auto& signature   = structure.constant_signature_vec.at(n_constant);
            const auto  series_iter = model_ref.constant_series_map.find (ConstantSeriesKey(n_pass,
                                                                                            static_cast<uint32_t>(n_constant) ));

            if ((signature.semantic == ConstantSemantic::VIEW_MATRIX             ||
                 signature.semantic == ConstantSemantic::VIEW_PROJECTION_MATRIX) &&
                 series_iter        != model_ref.constant_series_map.end() )
            {
                const size_t n_samples = series_iter->second.get_sample_vec().size();

                has_camera        = true;
                is_camera_changed = (is_camera_changed ||
                                     n_samples > 1);
            }
        }

        if (has_camera                 &&
            is_camera_changed == false)
        {
            result_vec.push_back(n_pass);
        }
    }

    return result_vec;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_depth(const AnalysisResult&    in_analysis,
                                                                                           Utils::Json::JsonWriter* inout_writer_ptr)
{
    constexpr double FRACTION_FAR    = 0.9;
    constexpr double FRACTION_MEDIAN = 0.5;
    constexpr double FRACTION_NEAR   = 0.1;

    const auto& model_ref = in_analysis.model;

    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key   ("passes");
    inout_writer_ptr->begin_array();

    /* A pass whose objects have their origins behind the camera still has surfaces in front of it. */
    std::set<DepthHistogramKey> key_set;
    const DepthHistogram        no_histogram;

    for (const auto& histogram_pair : model_ref.depth_histogram_map)
    {
        key_set.insert(histogram_pair.first);
    }

    for (const auto& histogram_pair : model_ref.surface_depth_histogram_map)
    {
        key_set.insert(histogram_pair.first);
    }

    for (const auto& key : key_set)
    {
        const auto  origin_iterator = model_ref.depth_histogram_map.find(key);
        const auto& origin          = (origin_iterator != model_ref.depth_histogram_map.end() ) ? origin_iterator->second
                                                                                                  : no_histogram;
        const auto& structure       = model_ref.pass_structure_interner.get_structure(key.second);

        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("shape_id");
        inout_writer_ptr->write_u64    (key.first);
        inout_writer_ptr->write_key    ("shape_key");
        inout_writer_ptr->write_hex_u64(ModelBuilder::compute_shape_key(model_ref,
                                                                        key.first) );
        inout_writer_ptr->write_key    ("pass_id");
        inout_writer_ptr->write_u64    (key.second);
        inout_writer_ptr->write_key    ("pass_key");
        inout_writer_ptr->write_hex_u64(structure.key);
        inout_writer_ptr->write_key    ("n_vertices");
        inout_writer_ptr->write_u64    (origin.get_total_weight() );
        inout_writer_ptr->write_key    ("depth_near");
        inout_writer_ptr->write_float  (origin.get_depth_at_fraction(FRACTION_NEAR) );
        inout_writer_ptr->write_key    ("depth_median");
        inout_writer_ptr->write_float  (origin.get_depth_at_fraction(FRACTION_MEDIAN) );
        inout_writer_ptr->write_key    ("depth_far");
        inout_writer_ptr->write_float  (origin.get_depth_at_fraction(FRACTION_FAR) );

        /* Of the vertices that the draws draw, and not of the origins of their objects, if the analysis could tell them. */
        const auto surface_iterator = model_ref.surface_depth_histogram_map.find(key);

        if (surface_iterator != model_ref.surface_depth_histogram_map.end() )
        {
            inout_writer_ptr->write_key  ("surface_n_vertices");
            inout_writer_ptr->write_u64  (surface_iterator->second.get_total_weight() );
            inout_writer_ptr->write_key  ("surface_depth_nearest");
            inout_writer_ptr->write_float(surface_iterator->second.get_depth_at_fraction(0.0005) );
            inout_writer_ptr->write_key  ("surface_depth_near");
            inout_writer_ptr->write_float(surface_iterator->second.get_depth_at_fraction(FRACTION_NEAR) );
            inout_writer_ptr->write_key  ("surface_depth_median");
            inout_writer_ptr->write_float(surface_iterator->second.get_depth_at_fraction(FRACTION_MEDIAN) );
            inout_writer_ptr->write_key  ("surface_depth_far");
            inout_writer_ptr->write_float(surface_iterator->second.get_depth_at_fraction(FRACTION_FAR) );
        }

        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_overlays(const AnalysisResult&    in_analysis,
                                                                                              Utils::Json::JsonWriter* inout_writer_ptr)
{
    const auto&                 model_ref   = in_analysis.model;
    const std::vector<uint32_t> pass_id_vec = get_static_camera_pass_id_vec(in_analysis);
    const Timeline              timeline    = TimelineBuilder::build(in_analysis);

    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key   ("note");
    inout_writer_ptr->write_string("A hint, not a fact: a pass with a camera which never changes may also be a 3D scene seen from one place. One whose vertices are at about one depth is "
                                   "likely an overlay; one whose vertices are at many depths (depth.is_flat is false) is a 3D scene, and is left out of the unaffected list.");

    inout_writer_ptr->write_key("candidates");
    inout_writer_ptr->begin_array();

    for (const auto& pass_id : pass_id_vec)
    {
        const auto& structure    = model_ref.pass_structure_interner.get_structure(pass_id);
        const auto  depth_spread = get_pass_depth_spread(in_analysis,
                                                         pass_id);

        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key    ("pass_id");
        inout_writer_ptr->write_u64    (pass_id);
        inout_writer_ptr->write_key    ("key");
        inout_writer_ptr->write_hex_u64(structure.key);
        inout_writer_ptr->write_key    ("n_frames");
        inout_writer_ptr->write_u64    (timeline.pass_usage_vec.at(pass_id).n_frames);
        inout_writer_ptr->write_key    ("n_draw_structures");
        inout_writer_ptr->write_u64    (structure.draw_structure_key_vec.size() );
        write_depth_spread(depth_spread,
                           inout_writer_ptr);
        inout_writer_ptr->end_object();
    }

    inout_writer_ptr->end_array();

    /* A draw of a candidate pass is a candidate too: a person looks at them one by one, and a recipe can tell them apart by their signatures. */
    inout_writer_ptr->write_key("draws");
    inout_writer_ptr->begin_array();

    for (const auto& pass_id : pass_id_vec)
    {
        const auto& structure         = model_ref.pass_structure_interner.get_structure(pass_id);
        uint64_t    first_frame_index = 0;
        const bool  has_first_frame   = get_first_frame_index(in_analysis,
                                                              pass_id,
                                                             &first_frame_index);
        const auto  depth_spread      = get_pass_depth_spread(in_analysis,
                                                              pass_id);

        for (const auto& draw_signature : structure.draw_signature_vec)
        {
            const auto draw_iterator = model_ref.draw_structure_map.find(draw_signature.structure_key);

            inout_writer_ptr->begin_object();

            inout_writer_ptr->write_key    ("signature_key");
            inout_writer_ptr->write_hex_u64(draw_signature.signature_key);
            inout_writer_ptr->write_key    ("structure_key");
            inout_writer_ptr->write_hex_u64(draw_signature.structure_key);
            inout_writer_ptr->write_key    ("pass_id");
            inout_writer_ptr->write_u64    (pass_id);
            inout_writer_ptr->write_key    ("pass_key");
            inout_writer_ptr->write_hex_u64(structure.key);
            inout_writer_ptr->write_key    ("n_frames");
            inout_writer_ptr->write_u64    (timeline.pass_usage_vec.at(pass_id).n_frames);

            if (has_first_frame)
            {
                inout_writer_ptr->write_key("first_frame_index");
                inout_writer_ptr->write_u64(first_frame_index);
            }

            if (draw_iterator != model_ref.draw_structure_map.end() )
            {
                inout_writer_ptr->write_key    ("topology");
                inout_writer_ptr->write_string (get_primitive_topology_name(draw_iterator->second.primitive_topology) );
                inout_writer_ptr->write_key    ("is_indexed");
                inout_writer_ptr->write_bool   (draw_iterator->second.is_indexed);
                inout_writer_ptr->write_key    ("vertex_layout_key");
                inout_writer_ptr->write_hex_u64(draw_iterator->second.vertex_layout_key);
            }

            write_depth_spread(depth_spread,
                               inout_writer_ptr);

            /* What makes the draw a candidate; a hint as the pass is. */
            inout_writer_ptr->write_key("reasons");
            inout_writer_ptr->begin_inline_array();
            inout_writer_ptr->write_string("static_camera");

            if (depth_spread.has_depth)
            {
                inout_writer_ptr->write_string((depth_spread.is_flat) ? "flat_depth"
                                                                      : "deep_depth");
            }

            if (structure.clears_color == false)
            {
                inout_writer_ptr->write_string("draws_over_existing_color");
            }

            inout_writer_ptr->end_array();

            inout_writer_ptr->end_object();
        }
    }

    inout_writer_ptr->end_array();

    inout_writer_ptr->write_key("unaffected");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key("pass_keys");
    inout_writer_ptr->begin_array();

    /* A pass whose vertices are at many depths is a 3D scene seen from one place, and is not a candidate for the list. */
    for (const auto& pass_id : pass_id_vec)
    {
        const auto depth_spread = get_pass_depth_spread(in_analysis,
                                                        pass_id);

        if (depth_spread.has_depth == false ||
            depth_spread.is_flat)
        {
            inout_writer_ptr->write_hex_u64(model_ref.pass_structure_interner.get_structure(pass_id).key);
        }
    }

    inout_writer_ptr->end_array();
    inout_writer_ptr->end_object();

    inout_writer_ptr->end_object();

    return QueryStatus::OK;
}

void Analysis::APIDumpAnalysis::QueryCommands::write_depth_spread(const PassDepthSpread&   in_spread,
                                                                  Utils::Json::JsonWriter* inout_writer_ptr)
{
    inout_writer_ptr->write_key("depth");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key("has_depth");
    inout_writer_ptr->write_bool(in_spread.has_depth);

    if (in_spread.has_depth)
    {
        inout_writer_ptr->write_key  ("n_vertices");
        inout_writer_ptr->write_u64  (in_spread.n_vertices);
        inout_writer_ptr->write_key  ("near");
        inout_writer_ptr->write_float(in_spread.depth_near);
        inout_writer_ptr->write_key  ("median");
        inout_writer_ptr->write_float(in_spread.depth_median);
        inout_writer_ptr->write_key  ("far");
        inout_writer_ptr->write_float(in_spread.depth_far);
        inout_writer_ptr->write_key  ("far_to_near_ratio");
        inout_writer_ptr->write_float(in_spread.far_to_near_ratio);
        inout_writer_ptr->write_key  ("is_flat");
        inout_writer_ptr->write_bool (in_spread.is_flat);
    }

    inout_writer_ptr->end_object();
}
