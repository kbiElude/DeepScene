/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/passes_serializer.h"
#include "Analysis/APIDumpAnalysis/key_builder.h"

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_covering(const std::vector<uint8_t>& in_byte_vec,
                                                                       Model*                      out_model_ptr)
{
    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );
    /* The pass structure, and how many keys it has. */
    constexpr size_t MIN_PASS_N_BYTES = 2 * sizeof(uint32_t);

    const auto n_passes = reader.read_count(MIN_PASS_N_BYTES);
    bool       is_ok    = true;

    for (uint32_t n_pass = 0;
                  n_pass < n_passes && reader.has_failed() == false;
                ++n_pass)
    {
        const uint32_t pass_id = reader.read_u32  ();
        const auto     n_keys  = reader.read_count(sizeof(uint64_t) );

        if (pass_id >= out_model_ptr->pass_structure_interner.get_n_structures() )
        {
            is_ok = false;
        }

        for (uint32_t n_key = 0;
                      n_key < n_keys && reader.has_failed() == false;
                    ++n_key)
        {
            out_model_ptr->covering_draw_key_map[pass_id].insert(reader.read_u64() );
        }
    }

    return (is_ok                        &&
            reader.has_failed() == false &&
            reader.is_at_end ());
}

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_depths(const std::vector<uint8_t>&   in_byte_vec,
                                                                     const DepthHistogramMapMember& in_member_ptr,
                                                                     Model*                        out_model_ptr)
{
    /* The frame shape and the pass structure, and how many ranges have weight. */
    constexpr size_t MIN_HISTOGRAM_N_BYTES = 3 * sizeof(uint32_t);

    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );
    const auto n_histograms = reader.read_count(MIN_HISTOGRAM_N_BYTES);
    bool       is_ok        = true;

    for (uint32_t n_histogram = 0;
                  n_histogram < n_histograms && reader.has_failed() == false;
                ++n_histogram)
    {
        /* Read in the order they are stored. */
        const uint32_t shape_id  = reader.read_u32  ();
        const uint32_t pass_id   = reader.read_u32  ();
        const auto     n_weights = reader.read_count(sizeof(uint32_t) + sizeof(uint64_t) );
        DepthHistogram histogram;

        for (uint32_t n_weight = 0;
                      n_weight < n_weights && reader.has_failed() == false;
                    ++n_weight)
        {
            const uint32_t bin    = reader.read_u32();
            const uint64_t weight = reader.read_u64();

            is_ok = histogram.restore_weight(bin,
                                             weight) && is_ok;
        }

        /* The frame shape and the pass structure must exist. */
        if (pass_id  >= out_model_ptr->pass_structure_interner.get_n_structures() ||
            shape_id >= out_model_ptr->frame_shape_interner.get_n_structures   () )
        {
            is_ok = false;
        }

        (out_model_ptr->*in_member_ptr)[DepthHistogramKey(shape_id,
                                                          pass_id)] = histogram;
    }

    return (is_ok                        &&
            reader.has_failed() == false &&
            reader.is_at_end () );
}

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_draws(const std::vector<uint8_t>& in_byte_vec,
                                                                    Model*                      out_model_ptr)
{
    /* The key, then a structure with no shaders, bindings or vertex buffers: write_draw_structure() with empty vectors. */
    constexpr size_t MIN_DRAW_N_BYTES = sizeof(uint64_t)      /* key                           */
                                      + sizeof(uint64_t)      /* index buffer key              */
                                      + 2 * sizeof(uint8_t)   /* is indexed, is instanced      */
                                      + sizeof(uint32_t)      /* primitive topology            */
                                      + sizeof(uint32_t)      /* n shaders                     */
                                      + sizeof(uint64_t)      /* state key                     */
                                      + 2 * sizeof(uint32_t)  /* n bindings, n vertex buffers  */
                                      + sizeof(uint64_t);     /* vertex layout key             */

    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );

    const auto n_draws = reader.read_count(MIN_DRAW_N_BYTES);

    for (uint32_t n_draw = 0;
                  n_draw < n_draws && reader.has_failed() == false;
                ++n_draw)
    {
        const uint64_t key = reader.read_u64();

        out_model_ptr->draw_structure_map[key] = read_draw_structure(&reader);
    }

    return (reader.has_failed() == false &&
            reader.is_at_end() );
}

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_frames(const std::vector<uint8_t>& in_byte_vec,
                                                                     Model*                      out_model_ptr)
{
    constexpr size_t FRAME_N_BYTES = 5 * sizeof(uint64_t) + 2 * sizeof(uint32_t);

    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );

    const auto n_frames = reader.read_count(FRAME_N_BYTES);
    bool       is_ok    = true;

    for (uint32_t n_frame = 0;
                  n_frame < n_frames && reader.has_failed() == false;
                ++n_frame)
    {
        FrameRecord record;

        record.begin_timestamp_qpc = reader.read_u64();
        record.end_timestamp_qpc   = reader.read_u64();
        record.first_call_index    = reader.read_u64();
        record.frame_index         = n_frame;
        record.last_call_index     = reader.read_u64();
        record.n_draws             = reader.read_u64();
        record.n_passes            = reader.read_u32();
        record.shape_id            = reader.read_u32();

        /* The shape must exist. */
        if (record.shape_id >= out_model_ptr->frame_shape_interner.get_n_structures() )
        {
            is_ok = false;
        }

        out_model_ptr->frame_record_vec.push_back(record);
    }

    return (is_ok                        &&
            reader.has_failed() == false &&
            reader.is_at_end () );
}

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_meta(const std::vector<uint8_t>& in_byte_vec,
                                                                   AnalysisResult*             out_analysis_ptr)
{
    constexpr size_t MIN_GROUP_N_BYTES = sizeof(uint32_t) + sizeof(uint64_t);

    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );

    out_analysis_ptr->workload_reference.filename    = reader.read_string();
    out_analysis_ptr->workload_reference.header_hash = reader.read_u64   ();
    out_analysis_ptr->workload_reference.n_bytes     = reader.read_u64   ();

    auto& summary = out_analysis_ptr->summary;

    summary.first_timestamp_qpc = reader.read_u64 ();
    summary.is_truncated        = reader.read_bool();
    summary.last_timestamp_qpc  = reader.read_u64 ();
    summary.n_calls             = reader.read_u64 ();
    summary.n_calls_ignored     = reader.read_u64 ();
    summary.n_segments          = reader.read_u64 ();
    summary.n_trailing_calls    = reader.read_u64 ();
    summary.n_unfollowed_calls  = reader.read_u64 ();
    summary.n_unsupported_calls = reader.read_u64 ();
    summary.pointer_n_bytes     = reader.read_u32 ();
    summary.qpc_frequency       = reader.read_u64 ();

    out_analysis_ptr->model.n_key_collisions = reader.read_u64();

    const auto n_groups = reader.read_count(MIN_GROUP_N_BYTES);

    for (uint32_t n_group = 0;
                  n_group < n_groups && reader.has_failed() == false;
                ++n_group)
    {
        const auto group = read_enum(&reader,
                                     Capture::APIDumpLoader::APIGroup::UNKNOWN);

        summary.n_calls_by_group_map[group] = reader.read_u64();
    }

    const auto n_hazards = reader.read_count(MIN_GROUP_N_BYTES);

    for (uint32_t n_hazard = 0;
                  n_hazard < n_hazards && reader.has_failed() == false;
                ++n_hazard)
    {
        const auto hazard = read_enum(&reader,
                                      ResolutionHazard::SIZE_QUERIES);

        summary.n_resolution_hazards_map[hazard] = reader.read_u64();
    }

    return (reader.has_failed() == false &&
            reader.is_at_end () );
}

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_passes(const std::vector<uint8_t>& in_byte_vec,
                                                                     Model*                      out_model_ptr)
{
    /* write_pass_structure() with empty vectors. */
    constexpr size_t MIN_PASS_N_BYTES = 3 * sizeof(uint8_t)   /* clears color, depth, stencil */
                                      + 2 * sizeof(uint32_t)  /* n colors, n constants        */
                                      +     sizeof(uint64_t)  /* depth stencil view key       */
                                      + 2 * sizeof(uint32_t)  /* n draws, n draw signatures   */
                                      +     sizeof(uint64_t)  /* key                          */
                                      + 6 * sizeof(float);    /* viewport                     */

    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );

    const auto n_passes = reader.read_count(MIN_PASS_N_BYTES);
    bool       is_ok    = true;

    for (uint32_t n_pass = 0;
                  n_pass < n_passes && reader.has_failed() == false;
                ++n_pass)
    {
        const PassStructure structure = read_pass_structure(&reader);

        /* A pass is made of draws which exist. */
        for (const auto& draw_key : structure.draw_structure_key_vec)
        {
            if (out_model_ptr->draw_structure_map.find(draw_key) == out_model_ptr->draw_structure_map.end() )
            {
                is_ok = false;
            }
        }

        /* A duplicate or out-of-order structure means this is not a file we wrote. */
        if (out_model_ptr->pass_structure_interner.intern(structure,
                                                          structure.key) != n_pass)
        {
            is_ok = false;
        }
    }

    return (is_ok                        &&
            reader.has_failed() == false &&
            reader.is_at_end () );
}

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_representatives(const std::vector<uint8_t>& in_byte_vec,
                                                                              Model*                      out_model_ptr)
{
    constexpr size_t MIN_ENTRY_N_BYTES = 2 * sizeof(uint64_t) + 2 * sizeof(uint32_t);

    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );
    const auto n_shapes = out_model_ptr->frame_shape_interner.get_n_structures();
    bool       is_ok    = true;

    for (uint32_t n_shape = 0;
                  n_shape < n_shapes && reader.has_failed() == false;
                ++n_shape)
    {
        const auto n_entries = reader.read_count(MIN_ENTRY_N_BYTES);

        for (uint32_t n_entry = 0;
                      n_entry < n_entries && reader.has_failed() == false;
                    ++n_entry)
        {
            FrameDetail detail;
            uint64_t    frame_index = reader.read_u64();
            uint64_t    weight      = reader.read_u64();
            const auto  n_instances = reader.read_count(sizeof(uint64_t) );

            for (uint32_t n_instance = 0;
                          n_instance < n_instances && reader.has_failed() == false;
                        ++n_instance)
            {
                detail.pass_instance_vec.push_back(read_pass_instance(&reader) );

                if (detail.pass_instance_vec.back().structure_id >= out_model_ptr->pass_structure_interner.get_n_structures() )
                {
                    is_ok = false;
                }
            }

            const auto n_transfers = reader.read_count(sizeof(uint64_t) );

            for (uint32_t n_transfer = 0;
                          n_transfer < n_transfers && reader.has_failed() == false;
                        ++n_transfer)
            {
                detail.transfer_record_vec.push_back(read_transfer_record(&reader) );
            }

            /* The frame is one of the frames, and the resources are resources. */
            if (frame_index >= out_model_ptr->frame_record_vec.size() )
            {
                is_ok = false;
            }

            for (const auto& transfer : detail.transfer_record_vec)
            {
                const bool is_destination_valid = is_resource_id_valid(transfer.destination.resource_id,
                                                                       out_model_ptr->resource_vec.size() );
                const bool is_source_valid      = is_resource_id_valid(transfer.source.resource_id,
                                                                       out_model_ptr->resource_vec.size() );

                if (is_destination_valid == false ||
                    is_source_valid      == false)
                {
                    is_ok = false;
                }
            }

            /* Entries are stored as first, lightest, heaviest: the order add() expects. */
            out_model_ptr->representative_store.add(n_shape,
                                                    frame_index,
                                                    weight,
                                                    detail);
        }
    }

    return (is_ok                      &&
            reader.has_failed() == false &&
            reader.is_at_end() );
}

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resource_access(const std::vector<uint8_t>& in_byte_vec,
                                                                              Model*                      out_model_ptr)
{
    constexpr size_t ACCESS_N_BYTES = 6 * sizeof(uint64_t);

    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );

    const auto n_resources = reader.read_count                   (ACCESS_N_BYTES);
    const auto n_frames    = out_model_ptr->frame_record_vec.size();
    bool       is_ok       = (n_resources == out_model_ptr->resource_vec.size() );

    for (uint32_t n_resource = 0;
                  n_resource < n_resources && reader.has_failed() == false;
                ++n_resource)
    {
        ResourceAccess access;

        access.first_read_frame_index  = reader.read_u64();
        access.first_write_frame_index = reader.read_u64();
        access.last_write_frame_index  = reader.read_u64();
        access.n_frames_loop_carried   = reader.read_u64();
        access.n_frames_read           = reader.read_u64();
        access.n_frames_written        = reader.read_u64();

        if (is_resource_access_valid(access,
                                     n_frames) == false)
        {
            is_ok = false;
        }

        out_model_ptr->resource_access_vec.push_back(access);
    }

    return (is_ok                        &&
            reader.has_failed() == false &&
            reader.is_at_end () );
}

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resources(const std::vector<uint8_t>& in_byte_vec,
                                                                        Model*                      out_model_ptr)
{
    /* A resource with an empty name: write_resource_description() and the fields around it. */
    constexpr size_t MIN_RESOURCE_N_BYTES = 6 * sizeof(uint64_t)  /* created at, destroyed at, match key, API flags, content hash, size */
                                          + 14 * sizeof(uint32_t) /* name length, ID, parent view (3), description (9)                  */
                                          + 5 * sizeof(uint8_t);  /* is destroyed, description (4)                                      */

    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );
    const auto n_resources = reader.read_count(MIN_RESOURCE_N_BYTES);
    bool       is_ok       = true;

    for (uint32_t n_resource = 0;
                  n_resource < n_resources && reader.has_failed() == false;
                ++n_resource)
    {
        Resource resource;

        resource.created_at_call_index   = reader.read_u64          ();
        resource.debug_name              = reader.read_string       ();
        resource.description             = read_resource_description(&reader);
        resource.destroyed_at_call_index = reader.read_u64          ();
        resource.id                      = make_resource_id         (reader.read_u32() );
        resource.is_destroyed            = reader.read_bool         ();
        resource.match_key               = reader.read_u64          ();
        resource.parent_view             = read_resource_view       (&reader);

        /* IDs are consecutive from 1, and a resource is part of one that was made before it. */
        const auto id_value        = get_resource_id_value(resource.id);
        const bool is_parent_valid = is_resource_id_valid (resource.parent_view.resource_id,
                                                           n_resource);

        if (id_value        != n_resource + 1 ||
            is_parent_valid == false)
        {
            is_ok = false;
        }

        out_model_ptr->resource_vec.push_back(resource);
    }

    return (is_ok                      &&
            reader.has_failed() == false &&
            reader.is_at_end() );
}

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_series(const std::vector<uint8_t>& in_byte_vec,
                                                                     Model*                      out_model_ptr)
{
    constexpr size_t MIN_SERIES_N_BYTES = 2 * sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t);

    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );

    const auto n_series = reader.read_count(MIN_SERIES_N_BYTES);
    bool       is_ok    = true;

    for (uint32_t n_series_read = 0;
                  n_series_read < n_series && reader.has_failed() == false;
                ++n_series_read)
    {
        const uint32_t pass_id        = reader.read_u32();
        const uint32_t constant_index = reader.read_u32();
        const uint64_t n_updates      = reader.read_u64();
        const auto     n_samples      = reader.read_count(sizeof(uint64_t) + sizeof(uint32_t) );

        std::vector<ValueTimeSeries::Sample> sample_vec;

        sample_vec.reserve(n_samples);

        /* The pass structure and the constant index must exist. */
        if (pass_id        >= out_model_ptr->pass_structure_interner.get_n_structures()                                      ||
            constant_index >= out_model_ptr->pass_structure_interner.get_structure   (pass_id).constant_signature_vec.size() )
        {
            is_ok = false;
        }

        for (uint32_t n_sample = 0;
                      n_sample < n_samples && reader.has_failed() == false;
                    ++n_sample)
        {
            ValueTimeSeries::Sample sample;

            sample.timestamp_qpc = reader.read_u64     ();
            sample.value_u8_vec  = reader.read_byte_vec();

            sample_vec.push_back(std::move(sample) );
        }

        out_model_ptr->constant_series_map[ConstantSeriesKey(pass_id,
                                                             constant_index)].restore(n_updates,
                                                                                      std::move(sample_vec) );
    }

    return (is_ok                      &&
            reader.has_failed() == false &&
            reader.is_at_end() );
}

bool Analysis::APIDumpAnalysis::PassesSerializer::deserialize_shapes(const std::vector<uint8_t>& in_byte_vec,
                                                                     Model*                      out_model_ptr)
{
    ByteReader reader(in_byte_vec.data(),
                      in_byte_vec.size() );

    const auto n_shapes = reader.read_count(sizeof(uint32_t) );
    bool       is_ok    = true;

    for (uint32_t n_shape = 0;
                  n_shape < n_shapes && reader.has_failed() == false;
                ++n_shape)
    {
        FrameShape            shape;
        std::vector<uint64_t> pass_key_vec;
        const auto            n_passes = reader.read_count(sizeof(uint32_t) );

        for (uint32_t n_pass = 0;
                      n_pass < n_passes && reader.has_failed() == false;
                    ++n_pass)
        {
            const uint32_t pass_id = reader.read_u32();

            if (pass_id >= out_model_ptr->pass_structure_interner.get_n_structures() )
            {
                is_ok = false;

                break;
            }

            shape.pass_structure_id_vec.push_back(pass_id);
            pass_key_vec.push_back               (out_model_ptr->pass_structure_interner.get_structure(pass_id).key);
        }

        if (is_ok)
        {
            const uint64_t pass_key_set_key  = KeyBuilder::compute_key_set_key(pass_key_vec);
            const auto     interned_shape_id = out_model_ptr->frame_shape_interner.intern(shape,
                                                                                          pass_key_set_key);

            if (interned_shape_id != n_shape)
            {
                is_ok = false;
            }
        }
    }

    return (is_ok                        &&
            reader.has_failed() == false &&
            reader.is_at_end () );
}

bool Analysis::APIDumpAnalysis::PassesSerializer::is_resource_access_valid(const ResourceAccess& in_access,
                                                                           const uint64_t&       in_n_frames)
{
    const bool has_reads_ok  = (in_access.n_frames_read             <= in_n_frames                      &&
                                (in_access.n_frames_read            == 0                                ||
                                 in_access.first_read_frame_index   < in_n_frames));
    const bool has_writes_ok = (in_access.n_frames_written          <= in_n_frames                      &&
                                (in_access.n_frames_written         == 0                                ||
                                 (in_access.first_write_frame_index <= in_access.last_write_frame_index &&
                                  in_access.last_write_frame_index  <  in_n_frames)));

    return (has_reads_ok                                                  &&
            has_writes_ok                                                 &&
            in_access.n_frames_loop_carried <= in_access.n_frames_read    &&
            in_access.n_frames_loop_carried <= in_access.n_frames_written);
}

bool Analysis::APIDumpAnalysis::PassesSerializer::is_resource_id_valid(const ResourceId& in_id,
                                                                       const size_t&     in_n_resources)
{
    return (in_id                        == ResourceId::NONE ||
            get_resource_id_value(in_id) <= in_n_resources);
}

Analysis::APIDumpAnalysis::DataReference Analysis::APIDumpAnalysis::PassesSerializer::read_data_reference(ByteReader* inout_reader_ptr)
{
    DataReference result;

    result.content_hash  = inout_reader_ptr->read_u64();
    result.data_chunk_id = inout_reader_ptr->read_u32();
    result.n_bytes       = inout_reader_ptr->read_u64();

    return result;
}

Analysis::APIDumpAnalysis::DrawStructure Analysis::APIDumpAnalysis::PassesSerializer::read_draw_structure(ByteReader* inout_reader_ptr)
{
    constexpr size_t TEXTURE_BINDING_KEY_N_BYTES = 3 * sizeof(uint32_t) + sizeof(uint64_t);

    DrawStructure result;

    result.index_buffer_key   = inout_reader_ptr->read_u64 ();
    result.is_indexed         = inout_reader_ptr->read_bool();
    result.is_instanced       = inout_reader_ptr->read_bool();
    result.primitive_topology = read_enum                  (inout_reader_ptr,
                                                            PrimitiveTopology::UNKNOWN);

    const auto n_shaders = inout_reader_ptr->read_count(sizeof(uint64_t) );

    for (uint32_t n_shader = 0;
                  n_shader < n_shaders;
                ++n_shader)
    {
        result.shader_key_vec.push_back(inout_reader_ptr->read_u64() );
    }

    result.state_key = inout_reader_ptr->read_u64();

    {
        const auto n_bindings = inout_reader_ptr->read_count(TEXTURE_BINDING_KEY_N_BYTES);

        for (uint32_t n_binding = 0;
                      n_binding < n_bindings;
                    ++n_binding)
        {
            TextureBindingKey binding;

            binding.binding_point.element = inout_reader_ptr->read_u32();
            binding.binding_point.group   = inout_reader_ptr->read_u32();
            binding.binding_point.index   = inout_reader_ptr->read_u32();
            binding.view_key              = inout_reader_ptr->read_u64();

            result.texture_binding_key_vec.push_back(binding);
        }
    }

    {
        const auto n_vertex_buffers = inout_reader_ptr->read_count(sizeof(uint64_t) );

        for (uint32_t n_vertex_buffer = 0;
                      n_vertex_buffer < n_vertex_buffers;
                    ++n_vertex_buffer)
        {
            result.vertex_buffer_key_vec.push_back(inout_reader_ptr->read_u64() );
        }
    }

    result.vertex_layout_key = inout_reader_ptr->read_u64();

    return result;
}

template<typename Enum>
Enum Analysis::APIDumpAnalysis::PassesSerializer::read_enum(ByteReader* inout_reader_ptr,
                                                            const Enum& in_last_value)
{
    const uint32_t value  = inout_reader_ptr->read_u32();
    Enum           result = static_cast<Enum>(0);

    if (value > static_cast<uint32_t>(in_last_value) )
    {
        inout_reader_ptr->fail();
    }
    else
    {
        result = static_cast<Enum>(value);
    }

    return result;
}

Analysis::APIDumpAnalysis::PassInstance Analysis::APIDumpAnalysis::PassesSerializer::read_pass_instance(ByteReader* inout_reader_ptr)
{
    PassInstance result;

    result.begin_call_index     = inout_reader_ptr->read_u64();
    result.begin_timestamp_qpc  = inout_reader_ptr->read_u64();
    result.end_call_index       = inout_reader_ptr->read_u64();
    result.last_draw_call_index = inout_reader_ptr->read_u64();
    result.n_draws              = inout_reader_ptr->read_u64();
    result.n_vertices           = inout_reader_ptr->read_u64();
    result.structure_id         = inout_reader_ptr->read_u32();

    return result;
}

Analysis::APIDumpAnalysis::PassStructure Analysis::APIDumpAnalysis::PassesSerializer::read_pass_structure(ByteReader* inout_reader_ptr)
{
    constexpr size_t CONSTANT_SIGNATURE_N_BYTES = 3 * sizeof(uint64_t) + 3 * sizeof(uint32_t);

    PassStructure result;

    result.clears_color   = inout_reader_ptr->read_bool();
    result.clears_depth   = inout_reader_ptr->read_bool();
    result.clears_stencil = inout_reader_ptr->read_bool();

    const auto n_colors = inout_reader_ptr->read_count(sizeof(uint64_t) );

    for (uint32_t n_color = 0;
                  n_color < n_colors;
                ++n_color)
    {
        result.color_view_key_vec.push_back(inout_reader_ptr->read_u64() );
    }

    const auto n_constants = inout_reader_ptr->read_count(CONSTANT_SIGNATURE_N_BYTES);

    for (uint32_t n_constant = 0;
                  n_constant < n_constants;
                ++n_constant)
    {
        ConstantSignature signature;

        signature.block_key   = inout_reader_ptr->read_u64();
        signature.block_kind  = read_enum(inout_reader_ptr,
                                          ConstantBlockKind::PUSH_CONSTANTS);
        signature.byte_offset = inout_reader_ptr->read_u64();
        signature.byte_size   = inout_reader_ptr->read_u64();
        signature.provenance  = read_enum(inout_reader_ptr,
                                          ConstantProvenance::INFERRED);
        signature.semantic    = read_enum(inout_reader_ptr,
                                          ConstantSemantic::UNKNOWN);

        result.constant_signature_vec.push_back(signature);
    }

    result.depth_stencil_view_key = inout_reader_ptr->read_u64();

    const auto n_draws = inout_reader_ptr->read_count(sizeof(uint64_t) );

    for (uint32_t n_draw = 0;
                  n_draw < n_draws;
                ++n_draw)
    {
        result.draw_structure_key_vec.push_back(inout_reader_ptr->read_u64() );
    }

    const auto n_signatures = inout_reader_ptr->read_count(sizeof(DrawSignature) );

    for (uint32_t n_signature = 0;
                  n_signature < n_signatures;
                ++n_signature)
    {
        DrawSignature signature;

        signature.signature_key = inout_reader_ptr->read_u64();
        signature.structure_key = inout_reader_ptr->read_u64();

        result.draw_signature_vec.push_back(signature);
    }

    result.key     = inout_reader_ptr->read_u64();
    result.viewport = read_viewport(inout_reader_ptr);

    return result;
}

Analysis::APIDumpAnalysis::ResourceDescription Analysis::APIDumpAnalysis::PassesSerializer::read_resource_description(ByteReader* inout_reader_ptr)
{
    ResourceDescription result;

    result.api_flags            = inout_reader_ptr->read_u64 ();
    result.content_hash         = inout_reader_ptr->read_u64 ();
    result.depth_or_layer_count = inout_reader_ptr->read_u32 ();
    result.format.format_class  = read_enum                  (inout_reader_ptr,
                                                              FormatClass::UNKNOWN);
    result.format.raw_value     = inout_reader_ptr->read_u32 ();
    result.has_content_hash     = inout_reader_ptr->read_bool();
    result.height               = inout_reader_ptr->read_u32 ();
    result.is_depth_stencil     = inout_reader_ptr->read_bool();
    result.is_dynamic           = inout_reader_ptr->read_bool();
    result.is_render_target     = inout_reader_ptr->read_bool();
    result.kind                 = read_enum                  (inout_reader_ptr,
                                                              ResourceKind::UNKNOWN);
    result.mip_level_count      = inout_reader_ptr->read_u32 ();
    result.sample_count         = inout_reader_ptr->read_u32 ();
    result.shader_stage         = read_enum                  (inout_reader_ptr,
                                                              ShaderStage::UNKNOWN);
    result.size_in_bytes        = inout_reader_ptr->read_u64 ();
    result.width                = inout_reader_ptr->read_u32 ();

    return result;
}

Analysis::APIDumpAnalysis::ResourceView Analysis::APIDumpAnalysis::PassesSerializer::read_resource_view(ByteReader* inout_reader_ptr)
{
    ResourceView result;

    result.layer       = inout_reader_ptr->read_u32();
    result.mip_level   = inout_reader_ptr->read_u32();
    result.resource_id = make_resource_id          (inout_reader_ptr->read_u32() );

    return result;
}

Analysis::APIDumpAnalysis::TransferRecord Analysis::APIDumpAnalysis::PassesSerializer::read_transfer_record(ByteReader* inout_reader_ptr)
{
    TransferRecord result;

    result.call_index  = inout_reader_ptr->read_u64();
    result.data        = read_data_reference       (inout_reader_ptr);
    result.destination = read_resource_view        (inout_reader_ptr);
    result.kind        = read_enum                 (inout_reader_ptr,
                                                    TransferKind::UPLOAD);
    result.source      = read_resource_view        (inout_reader_ptr);

    return result;
}

Analysis::APIDumpAnalysis::Viewport Analysis::APIDumpAnalysis::PassesSerializer::read_viewport(ByteReader* inout_reader_ptr)
{
    Viewport result;

    result.height    = inout_reader_ptr->read_f32();
    result.max_depth = inout_reader_ptr->read_f32();
    result.min_depth = inout_reader_ptr->read_f32();
    result.width     = inout_reader_ptr->read_f32();
    result.x         = inout_reader_ptr->read_f32();
    result.y         = inout_reader_ptr->read_f32();

    return result;
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_covering(const Model& in_model)
{
    ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(in_model.covering_draw_key_map.size() ));

    for (const auto& pass_pair : in_model.covering_draw_key_map)
    {
        writer.write_u32(pass_pair.first);
        writer.write_u32(static_cast<uint32_t>(pass_pair.second.size() ));

        for (const auto& draw_key : pass_pair.second)
        {
            writer.write_u64(draw_key);
        }
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_depths(const Model&                  in_model,
                                                                                   const DepthHistogramMapMember& in_member_ptr)
{
    const auto& histogram_map = in_model.*in_member_ptr;
    ByteWriter  writer;

    writer.write_u32(static_cast<uint32_t>(histogram_map.size() ));

    for (const auto& histogram_pair : histogram_map)
    {
        uint32_t n_weights = 0;

        for (uint32_t n_bin = 0;
                      n_bin < DepthHistogram::get_n_bins();
                    ++n_bin)
        {
            if (histogram_pair.second.get_weight(n_bin) != 0)
            {
                n_weights++;
            }
        }

        writer.write_u32(histogram_pair.first.first);
        writer.write_u32(histogram_pair.first.second);
        writer.write_u32(n_weights);

        for (uint32_t n_bin = 0;
                      n_bin < DepthHistogram::get_n_bins();
                    ++n_bin)
        {
            if (histogram_pair.second.get_weight(n_bin) != 0)
            {
                writer.write_u32(n_bin);
                writer.write_u64(histogram_pair.second.get_weight(n_bin) );
            }
        }
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_draws(const Model& in_model)
{
    ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(in_model.draw_structure_map.size() ));

    for (const auto& draw_pair : in_model.draw_structure_map)
    {
        writer.write_u64(draw_pair.first);

        write_draw_structure(draw_pair.second,
                            &writer);
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_frames(const Model& in_model)
{
    ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(in_model.frame_record_vec.size() ));

    for (const auto& record : in_model.frame_record_vec)
    {
        writer.write_u64(record.begin_timestamp_qpc);
        writer.write_u64(record.end_timestamp_qpc);
        writer.write_u64(record.first_call_index);
        writer.write_u64(record.last_call_index);
        writer.write_u64(record.n_draws);
        writer.write_u32(record.n_passes);
        writer.write_u32(record.shape_id);
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_meta(const AnalysisResult& in_analysis)
{
    ByteWriter writer;

    writer.write_string(in_analysis.workload_reference.filename);
    writer.write_u64   (in_analysis.workload_reference.header_hash);
    writer.write_u64   (in_analysis.workload_reference.n_bytes);

    writer.write_u64 (in_analysis.summary.first_timestamp_qpc);
    writer.write_bool(in_analysis.summary.is_truncated);
    writer.write_u64 (in_analysis.summary.last_timestamp_qpc);
    writer.write_u64 (in_analysis.summary.n_calls);
    writer.write_u64 (in_analysis.summary.n_calls_ignored);
    writer.write_u64 (in_analysis.summary.n_segments);
    writer.write_u64 (in_analysis.summary.n_trailing_calls);
    writer.write_u64 (in_analysis.summary.n_unfollowed_calls);
    writer.write_u64 (in_analysis.summary.n_unsupported_calls);
    writer.write_u32 (in_analysis.summary.pointer_n_bytes);
    writer.write_u64 (in_analysis.summary.qpc_frequency);

    writer.write_u64(in_analysis.model.n_key_collisions);

    writer.write_u32(static_cast<uint32_t>(in_analysis.summary.n_calls_by_group_map.size() ));

    for (const auto& group_pair : in_analysis.summary.n_calls_by_group_map)
    {
        writer.write_u32(static_cast<uint32_t>(group_pair.first) );
        writer.write_u64(group_pair.second);
    }

    writer.write_u32(static_cast<uint32_t>(in_analysis.summary.n_resolution_hazards_map.size() ));

    for (const auto& hazard_pair : in_analysis.summary.n_resolution_hazards_map)
    {
        writer.write_u32(static_cast<uint32_t>(hazard_pair.first) );
        writer.write_u64(hazard_pair.second);
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_passes(const Model& in_model)
{
    const auto n_passes = in_model.pass_structure_interner.get_n_structures();
    ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(n_passes) );

    for (uint32_t n_pass = 0;
                  n_pass < n_passes;
                ++n_pass)
    {
        const auto& pass_structure = in_model.pass_structure_interner.get_structure(n_pass);

        write_pass_structure(pass_structure,
                            &writer);
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_representatives(const Model& in_model)
{
    const auto n_shapes = in_model.frame_shape_interner.get_n_structures();
    ByteWriter writer;

    for (uint32_t n_shape = 0;
                  n_shape < n_shapes;
                ++n_shape)
    {
        const auto entry_ptr_vec = in_model.representative_store.get_entry_ptr_vec(n_shape);

        writer.write_u32(static_cast<uint32_t>(entry_ptr_vec.size() ));

        for (const auto& entry_ptr : entry_ptr_vec)
        {
            writer.write_u64(entry_ptr->frame_index);
            writer.write_u64(entry_ptr->weight);

            writer.write_u32(static_cast<uint32_t>(entry_ptr->detail.pass_instance_vec.size() ));

            for (const auto& instance : entry_ptr->detail.pass_instance_vec)
            {
                write_pass_instance(instance,
                                   &writer);
            }

            writer.write_u32(static_cast<uint32_t>(entry_ptr->detail.transfer_record_vec.size() ));

            for (const auto& record : entry_ptr->detail.transfer_record_vec)
            {
                write_transfer_record(record,
                                     &writer);
            }
        }
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_resource_access(const Model& in_model)
{
    ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(in_model.resource_access_vec.size() ));

    for (const auto& access : in_model.resource_access_vec)
    {
        writer.write_u64(access.first_read_frame_index);
        writer.write_u64(access.first_write_frame_index);
        writer.write_u64(access.last_write_frame_index);
        writer.write_u64(access.n_frames_loop_carried);
        writer.write_u64(access.n_frames_read);
        writer.write_u64(access.n_frames_written);
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_resources(const Model& in_model)
{
    ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(in_model.resource_vec.size() ));

    for (const auto& resource : in_model.resource_vec)
    {
        writer.write_u64   (resource.created_at_call_index);
        writer.write_string(resource.debug_name);

        write_resource_description(resource.description,
                                  &writer);

        writer.write_u64 (resource.destroyed_at_call_index);
        writer.write_u32 (get_resource_id_value(resource.id) );
        writer.write_bool(resource.is_destroyed);
        writer.write_u64 (resource.match_key);

        write_resource_view(resource.parent_view,
                           &writer);
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_series(const Model& in_model)
{
    ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(in_model.constant_series_map.size() ));

    for (const auto& series_pair : in_model.constant_series_map)
    {
        writer.write_u32(series_pair.first.first);
        writer.write_u32(series_pair.first.second);
        writer.write_u64(series_pair.second.get_n_updates() );
        writer.write_u32(static_cast<uint32_t>(series_pair.second.get_sample_vec().size() ));

        for (const auto& sample : series_pair.second.get_sample_vec() )
        {
            writer.write_u64     (sample.timestamp_qpc);
            writer.write_byte_vec(sample.value_u8_vec);
        }
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesSerializer::serialize_shapes(const Model& in_model)
{
    const auto n_shapes = in_model.frame_shape_interner.get_n_structures();
    ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(n_shapes) );

    for (uint32_t n_shape = 0;
                  n_shape < n_shapes;
                ++n_shape)
    {
        const auto& shape = in_model.frame_shape_interner.get_structure(n_shape);

        writer.write_u32(static_cast<uint32_t>(shape.pass_structure_id_vec.size() ));

        for (const auto& pass_id : shape.pass_structure_id_vec)
        {
            writer.write_u32(pass_id);
        }
    }

    return writer.take_bytes();
}

void Analysis::APIDumpAnalysis::PassesSerializer::write_data_reference(const DataReference& in_reference,
                                                                       ByteWriter*          inout_writer_ptr)
{
    inout_writer_ptr->write_u64(in_reference.content_hash);
    inout_writer_ptr->write_u32(in_reference.data_chunk_id);
    inout_writer_ptr->write_u64(in_reference.n_bytes);
}

void Analysis::APIDumpAnalysis::PassesSerializer::write_draw_structure(const DrawStructure& in_structure,
                                                                       ByteWriter*          inout_writer_ptr)
{
    inout_writer_ptr->write_u64 (in_structure.index_buffer_key);
    inout_writer_ptr->write_bool(in_structure.is_indexed);
    inout_writer_ptr->write_bool(in_structure.is_instanced);
    inout_writer_ptr->write_u32 (static_cast<uint32_t>(in_structure.primitive_topology) );

    inout_writer_ptr->write_u32(static_cast<uint32_t>(in_structure.shader_key_vec.size() ));

    for (const auto& shader_key : in_structure.shader_key_vec)
    {
        inout_writer_ptr->write_u64(shader_key);
    }

    inout_writer_ptr->write_u64(in_structure.state_key);
    inout_writer_ptr->write_u32(static_cast<uint32_t>(in_structure.texture_binding_key_vec.size() ));

    for (const auto& binding : in_structure.texture_binding_key_vec)
    {
        inout_writer_ptr->write_u32(binding.binding_point.element);
        inout_writer_ptr->write_u32(binding.binding_point.group);
        inout_writer_ptr->write_u32(binding.binding_point.index);
        inout_writer_ptr->write_u64(binding.view_key);
    }

    inout_writer_ptr->write_u32(static_cast<uint32_t>(in_structure.vertex_buffer_key_vec.size() ));

    for (const auto& vertex_buffer_key : in_structure.vertex_buffer_key_vec)
    {
        inout_writer_ptr->write_u64(vertex_buffer_key);
    }

    inout_writer_ptr->write_u64(in_structure.vertex_layout_key);
}

void Analysis::APIDumpAnalysis::PassesSerializer::write_pass_instance(const PassInstance& in_instance,
                                                                      ByteWriter*         inout_writer_ptr)
{
    inout_writer_ptr->write_u64(in_instance.begin_call_index);
    inout_writer_ptr->write_u64(in_instance.begin_timestamp_qpc);
    inout_writer_ptr->write_u64(in_instance.end_call_index);
    inout_writer_ptr->write_u64(in_instance.last_draw_call_index);
    inout_writer_ptr->write_u64(in_instance.n_draws);
    inout_writer_ptr->write_u64(in_instance.n_vertices);
    inout_writer_ptr->write_u32(in_instance.structure_id);
}

void Analysis::APIDumpAnalysis::PassesSerializer::write_pass_structure(const PassStructure& in_structure,
                                                                       ByteWriter*          inout_writer_ptr)
{
    inout_writer_ptr->write_bool(in_structure.clears_color);
    inout_writer_ptr->write_bool(in_structure.clears_depth);
    inout_writer_ptr->write_bool(in_structure.clears_stencil);

    inout_writer_ptr->write_u32(static_cast<uint32_t>(in_structure.color_view_key_vec.size() ));

    for (const auto& color_view_key : in_structure.color_view_key_vec)
    {
        inout_writer_ptr->write_u64(color_view_key);
    }

    inout_writer_ptr->write_u32(static_cast<uint32_t>(in_structure.constant_signature_vec.size() ));

    for (const auto& signature : in_structure.constant_signature_vec)
    {
        inout_writer_ptr->write_u64(signature.block_key);
        inout_writer_ptr->write_u32(static_cast<uint32_t>(signature.block_kind) );
        inout_writer_ptr->write_u64(signature.byte_offset);
        inout_writer_ptr->write_u64(signature.byte_size);
        inout_writer_ptr->write_u32(static_cast<uint32_t>(signature.provenance) );
        inout_writer_ptr->write_u32(static_cast<uint32_t>(signature.semantic) );
    }

    inout_writer_ptr->write_u64(in_structure.depth_stencil_view_key);
    inout_writer_ptr->write_u32(static_cast<uint32_t>(in_structure.draw_structure_key_vec.size() ));

    for (const auto& draw_structure_key : in_structure.draw_structure_key_vec)
    {
        inout_writer_ptr->write_u64(draw_structure_key);
    }

    inout_writer_ptr->write_u32(static_cast<uint32_t>(in_structure.draw_signature_vec.size() ));

    for (const auto& draw_signature : in_structure.draw_signature_vec)
    {
        inout_writer_ptr->write_u64(draw_signature.signature_key);
        inout_writer_ptr->write_u64(draw_signature.structure_key);
    }

    inout_writer_ptr->write_u64(in_structure.key);

    write_viewport(in_structure.viewport,
                   inout_writer_ptr);
}

void Analysis::APIDumpAnalysis::PassesSerializer::write_resource_description(const ResourceDescription& in_description,
                                                                             ByteWriter*                inout_writer_ptr)
{
    inout_writer_ptr->write_u64 (in_description.api_flags);
    inout_writer_ptr->write_u64 (in_description.content_hash);
    inout_writer_ptr->write_u32 (in_description.depth_or_layer_count);
    inout_writer_ptr->write_u32 (static_cast<uint32_t>(in_description.format.format_class) );
    inout_writer_ptr->write_u32 (in_description.format.raw_value);
    inout_writer_ptr->write_bool(in_description.has_content_hash);
    inout_writer_ptr->write_u32 (in_description.height);
    inout_writer_ptr->write_bool(in_description.is_depth_stencil);
    inout_writer_ptr->write_bool(in_description.is_dynamic);
    inout_writer_ptr->write_bool(in_description.is_render_target);
    inout_writer_ptr->write_u32 (static_cast<uint32_t>(in_description.kind) );
    inout_writer_ptr->write_u32 (in_description.mip_level_count);
    inout_writer_ptr->write_u32 (in_description.sample_count);
    inout_writer_ptr->write_u32 (static_cast<uint32_t>(in_description.shader_stage) );
    inout_writer_ptr->write_u64 (in_description.size_in_bytes);
    inout_writer_ptr->write_u32 (in_description.width);
}

void Analysis::APIDumpAnalysis::PassesSerializer::write_resource_view(const ResourceView& in_view,
                                                                      ByteWriter*         inout_writer_ptr)
{
    inout_writer_ptr->write_u32(in_view.layer);
    inout_writer_ptr->write_u32(in_view.mip_level);
    inout_writer_ptr->write_u32(get_resource_id_value(in_view.resource_id) );
}

void Analysis::APIDumpAnalysis::PassesSerializer::write_transfer_record(const TransferRecord& in_record,
                                                                        ByteWriter*           inout_writer_ptr)
{
    inout_writer_ptr->write_u64(in_record.call_index);

    write_data_reference(in_record.data,
                         inout_writer_ptr);
    write_resource_view (in_record.destination,
                         inout_writer_ptr);

    inout_writer_ptr->write_u32(static_cast<uint32_t>(in_record.kind) );

    write_resource_view(in_record.source,
                        inout_writer_ptr);
}

void Analysis::APIDumpAnalysis::PassesSerializer::write_viewport(const Viewport& in_viewport,
                                                                 ByteWriter*     inout_writer_ptr)
{
    inout_writer_ptr->write_f32(in_viewport.height);
    inout_writer_ptr->write_f32(in_viewport.max_depth);
    inout_writer_ptr->write_f32(in_viewport.min_depth);
    inout_writer_ptr->write_f32(in_viewport.width);
    inout_writer_ptr->write_f32(in_viewport.x);
    inout_writer_ptr->write_f32(in_viewport.y);
}
