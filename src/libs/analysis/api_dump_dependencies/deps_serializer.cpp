/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/model.h"
#include "analysis/api_dump_analysis/passes_serializer.h"
#include "analysis/api_dump_dependencies/deps_serializer.h"

namespace
{
    /* The bits of what a frame does to a resource, as it is stored. */
    constexpr uint8_t USE_FLAG_FULLY_WRITTEN = 1 << 0;
    constexpr uint8_t USE_FLAG_READ          = 1 << 1;
    constexpr uint8_t USE_FLAG_SELF_SAMPLED  = 1 << 2;
    constexpr uint8_t USE_FLAG_WRITTEN       = 1 << 3;
    constexpr uint8_t USE_FLAG_ALL           = USE_FLAG_FULLY_WRITTEN | USE_FLAG_READ | USE_FLAG_SELF_SAMPLED | USE_FLAG_WRITTEN;

    /* The bits of what a version is. */
    constexpr uint8_t VERSION_FLAG_HAS_WRITER            = 1 << 0;
    constexpr uint8_t VERSION_FLAG_KEEPS_EARLIER_CONTENT = 1 << 1;
    constexpr uint8_t VERSION_FLAG_ALL                   = VERSION_FLAG_HAS_WRITER | VERSION_FLAG_KEEPS_EARLIER_CONTENT;

    /* Fields of a resource's totals, a use run, a version and a range of frames, with nothing in the variable parts. */
    constexpr size_t MIN_RANGE_N_BYTES    = 2 * sizeof(uint64_t);
    constexpr size_t MIN_RESOURCE_N_BYTES = sizeof(uint8_t) + 10 * sizeof(uint64_t) + 2 * sizeof(uint32_t);
    constexpr size_t MIN_RUN_N_BYTES      = 2 * sizeof(uint64_t) + 2 * sizeof(uint8_t);
    constexpr size_t MIN_VERSION_N_BYTES  = sizeof(uint8_t) + 3 * sizeof(uint64_t) + sizeof(uint32_t);
}

bool Analysis::APIDumpDependencies::DepsSerializer::deserialize_dependencies(const std::vector<uint8_t>& in_byte_vec,
                                                                             DependencyAnalysis*         inout_analysis_ptr)
{
    Utils::BinaryIO::ByteReader reader(in_byte_vec.data(),
                                       in_byte_vec.size());

    const uint64_t n_frames    = reader.read_u64();
    const uint32_t n_resources = reader.read_count(MIN_RESOURCE_N_BYTES);
    bool           is_ok       = (n_resources == inout_analysis_ptr->resource_vec.size() );

    inout_analysis_ptr->result.n_frames = n_frames;

    inout_analysis_ptr->result.resource_vec.clear();

    for (uint32_t n_resource = 0;
                  n_resource < n_resources && reader.has_failed() == false && is_ok;
                ++n_resource)
    {
        ResourceDependencies dependencies;
        ResourceSummary&     summary      = dependencies.summary;

        summary.dependency_class                        = read_enum      (&reader,
                                                                          DependencyClass::UNUSED);
        summary.first_frame_index                       = reader.read_u64();
        summary.last_frame_index                        = reader.read_u64();
        summary.longest_carry_n_frames                  = reader.read_u64();
        summary.n_frames_carrying                       = reader.read_u64();
        summary.n_frames_carrying_possibly              = reader.read_u64();
        summary.n_frames_read                           = reader.read_u64();
        summary.n_frames_self_sampled                   = reader.read_u64();
        summary.n_frames_used                           = reader.read_u64();
        summary.n_frames_using_earlier_content          = reader.read_u64();
        summary.n_frames_using_earlier_content_possibly = reader.read_u64();
        summary.n_frames_written                        = reader.read_u64();

        const uint32_t n_runs       = reader.read_count(MIN_RUN_N_BYTES);
        uint64_t       n_next_frame = 0; /* the runs are in order, and do not overlap */

        for (uint32_t n_run = 0;
                      n_run < n_runs && reader.has_failed() == false;
                    ++n_run)
        {
            UseRun run;

            run.first_frame_index = reader.read_u64();
            run.last_frame_index  = reader.read_u64();
            run.use.carry         = read_enum      (&reader,
                                                    CarryKind::POSSIBLE);

            const uint8_t flags = reader.read_u8();

            run.use.is_fully_written = ((flags & USE_FLAG_FULLY_WRITTEN) != 0);
            run.use.is_read          = ((flags & USE_FLAG_READ)          != 0);
            run.use.is_self_sampled  = ((flags & USE_FLAG_SELF_SAMPLED)  != 0);
            run.use.is_written       = ((flags & USE_FLAG_WRITTEN)       != 0);

            if ((flags & ~USE_FLAG_ALL) != 0                     ||
                run.first_frame_index   <  n_next_frame          ||
                run.last_frame_index    <  run.first_frame_index ||
                run.last_frame_index    >= n_frames)
            {
                is_ok = false;
            }

            n_next_frame = run.last_frame_index + 1;

            dependencies.use_run_vec.push_back(run);
        }

        const uint32_t n_versions = reader.read_count(MIN_VERSION_N_BYTES);

        for (uint32_t n_version = 0;
                      n_version < n_versions && reader.has_failed() == false;
                    ++n_version)
        {
            ResourceVersion version;
            const uint8_t   flags   = reader.read_u8();

            version.has_writer                 = ((flags & VERSION_FLAG_HAS_WRITER)            != 0);
            version.keeps_earlier_content      = ((flags & VERSION_FLAG_KEEPS_EARLIER_CONTENT) != 0);
            version.n_consumer_frames          = reader.read_u64();
            version.n_possible_consumer_frames = reader.read_u64();
            version.writer_frame_index         = reader.read_u64();

            const uint32_t n_ranges              = reader.read_count(MIN_RANGE_N_BYTES);
            uint64_t       n_next_consumer_frame = 0;

            for (uint32_t n_range = 0;
                          n_range < n_ranges && reader.has_failed() == false;
                        ++n_range)
            {
                FrameRange range;

                range.first_frame_index = reader.read_u64();
                range.last_frame_index  = reader.read_u64();

                if (range.first_frame_index <  n_next_consumer_frame   ||
                    range.last_frame_index  <  range.first_frame_index ||
                    range.last_frame_index  >= n_frames)
                {
                    is_ok = false;
                }

                n_next_consumer_frame = range.last_frame_index + 1;

                version.consumer_range_vec.push_back(range);
            }

            if ((flags & ~VERSION_FLAG_ALL)                               != 0                         ||
                (version.has_writer         && version.writer_frame_index >= n_frames)                 ||
                version.n_possible_consumer_frames                        >  version.n_consumer_frames)
            {
                is_ok = false;
            }

            dependencies.version_vec.push_back(version);
        }

        inout_analysis_ptr->result.resource_vec.push_back(dependencies);
    }

    const bool has_failed = reader.has_failed();
    const bool is_at_end  = reader.is_at_end();

    return (is_ok               &&
            has_failed == false &&
            is_at_end);
}

bool Analysis::APIDumpDependencies::DepsSerializer::deserialize_frames(const std::vector<uint8_t>& in_byte_vec,
                                                                       DependencyAnalysis*         inout_analysis_ptr)
{
    Utils::BinaryIO::ByteReader reader(in_byte_vec.data(),
                                       in_byte_vec.size());
    const uint32_t n_frames        = reader.read_count(MIN_RANGE_N_BYTES);
    bool           is_ok           = true;
    uint64_t       next_call_index = 0;

    inout_analysis_ptr->frame_call_range_vec.clear();

    for (uint32_t n_frame = 0;
                  n_frame < n_frames && reader.has_failed() == false;
                ++n_frame)
    {
        FrameCallRange range;

        range.first_call_index = reader.read_u64();
        range.last_call_index  = reader.read_u64();

        /* The frames follow each other. */
        if (range.first_call_index <  next_call_index        ||
            range.last_call_index  <  range.first_call_index)
        {
            is_ok = false;
        }

        next_call_index = range.last_call_index + 1;

        inout_analysis_ptr->frame_call_range_vec.push_back(range);
    }

    const bool has_failed = reader.has_failed();
    const bool is_at_end  = reader.is_at_end();

    return (is_ok               &&
            has_failed == false &&
            is_at_end);
}

bool Analysis::APIDumpDependencies::DepsSerializer::deserialize_meta(const std::vector<uint8_t>& in_byte_vec,
                                                                     DependencyAnalysis*         out_analysis_ptr)
{
    constexpr size_t MIN_API_N_BYTES = sizeof(uint32_t) + sizeof(uint64_t); /* the length of a name, and a number of calls */

    Utils::BinaryIO::ByteReader reader(in_byte_vec.data(),
                                       in_byte_vec.size());

    *out_analysis_ptr = DependencyAnalysis();

    out_analysis_ptr->workload_reference.filename    = reader.read_string();
    out_analysis_ptr->workload_reference.header_hash = reader.read_u64   ();
    out_analysis_ptr->workload_reference.middle_hash = reader.read_u64   ();
    out_analysis_ptr->workload_reference.n_bytes     = reader.read_u64   ();
    out_analysis_ptr->workload_reference.tail_hash   = reader.read_u64   ();

    out_analysis_ptr->is_truncated    = reader.read_bool();
    out_analysis_ptr->n_calls         = reader.read_u64 ();
    out_analysis_ptr->pointer_n_bytes = reader.read_u32 ();
    out_analysis_ptr->qpc_frequency   = reader.read_u64 ();

    const uint32_t n_apis = reader.read_count(MIN_API_N_BYTES);

    for (uint32_t n_api = 0;
                  n_api < n_apis && reader.has_failed() == false;
                ++n_api)
    {
        const std::string name = reader.read_string();

        out_analysis_ptr->n_calls_by_api_map[name] = reader.read_u64();
    }

    const bool has_failed = reader.has_failed();
    const bool is_at_end  = reader.is_at_end();

    return (has_failed == false &&
            is_at_end);
}

bool Analysis::APIDumpDependencies::DepsSerializer::deserialize_resources(const std::vector<uint8_t>& in_byte_vec,
                                                                          DependencyAnalysis*         out_analysis_ptr)
{
    Analysis::APIDumpAnalysis::Model model;
    const bool                       result = Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resources(in_byte_vec,
                                                                                                               &model);

    out_analysis_ptr->resource_vec = std::move(model.resource_vec);

    return result;
}

template<typename Enum>
Enum Analysis::APIDumpDependencies::DepsSerializer::read_enum(Utils::BinaryIO::ByteReader* inout_reader_ptr,
                                                              const Enum&                  in_last_value)
{
    const uint8_t value  = inout_reader_ptr->read_u8();
    Enum          result = static_cast<Enum>(0);

    if (value > static_cast<uint8_t>(in_last_value) )
    {
        inout_reader_ptr->fail();
    }
    else
    {
        result = static_cast<Enum>(value);
    }

    return result;
}

std::vector<uint8_t> Analysis::APIDumpDependencies::DepsSerializer::serialize_dependencies(const DependencyAnalysis& in_analysis)
{
    Utils::BinaryIO::ByteWriter writer;

    writer.write_u64(in_analysis.result.n_frames);
    writer.write_u32(static_cast<uint32_t>(in_analysis.result.resource_vec.size() ) );

    for (const auto& dependencies : in_analysis.result.resource_vec)
    {
        const ResourceSummary& summary = dependencies.summary;

        writer.write_u8 (static_cast<uint8_t>(summary.dependency_class) );
        writer.write_u64(summary.first_frame_index);
        writer.write_u64(summary.last_frame_index);
        writer.write_u64(summary.longest_carry_n_frames);
        writer.write_u64(summary.n_frames_carrying);
        writer.write_u64(summary.n_frames_carrying_possibly);
        writer.write_u64(summary.n_frames_read);
        writer.write_u64(summary.n_frames_self_sampled);
        writer.write_u64(summary.n_frames_used);
        writer.write_u64(summary.n_frames_using_earlier_content);
        writer.write_u64(summary.n_frames_using_earlier_content_possibly);
        writer.write_u64(summary.n_frames_written);

        writer.write_u32(static_cast<uint32_t>(dependencies.use_run_vec.size() ) );

        for (const auto& run : dependencies.use_run_vec)
        {
            const uint8_t flags = static_cast<uint8_t>((run.use.is_fully_written ? USE_FLAG_FULLY_WRITTEN : 0) |
                                                       (run.use.is_read          ? USE_FLAG_READ          : 0) |
                                                       (run.use.is_self_sampled  ? USE_FLAG_SELF_SAMPLED  : 0) |
                                                       (run.use.is_written       ? USE_FLAG_WRITTEN       : 0) );

            writer.write_u64(run.first_frame_index);
            writer.write_u64(run.last_frame_index);
            writer.write_u8 (static_cast<uint8_t>(run.use.carry) );
            writer.write_u8 (flags);
        }

        writer.write_u32(static_cast<uint32_t>(dependencies.version_vec.size() ) );

        for (const auto& version : dependencies.version_vec)
        {
            const uint8_t flags = static_cast<uint8_t>((version.has_writer            ? VERSION_FLAG_HAS_WRITER            : 0) |
                                                       (version.keeps_earlier_content ? VERSION_FLAG_KEEPS_EARLIER_CONTENT : 0) );

            writer.write_u8 (flags);
            writer.write_u64(version.n_consumer_frames);
            writer.write_u64(version.n_possible_consumer_frames);
            writer.write_u64(version.writer_frame_index);
            writer.write_u32(static_cast<uint32_t>(version.consumer_range_vec.size() ) );

            for (const auto& range : version.consumer_range_vec)
            {
                writer.write_u64(range.first_frame_index);
                writer.write_u64(range.last_frame_index);
            }
        }
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpDependencies::DepsSerializer::serialize_frames(const DependencyAnalysis& in_analysis)
{
    Utils::BinaryIO::ByteWriter writer;

    writer.write_u32(static_cast<uint32_t>(in_analysis.frame_call_range_vec.size() ) );

    for (const auto& range : in_analysis.frame_call_range_vec)
    {
        writer.write_u64(range.first_call_index);
        writer.write_u64(range.last_call_index);
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpDependencies::DepsSerializer::serialize_meta(const DependencyAnalysis& in_analysis)
{
    Utils::BinaryIO::ByteWriter writer;

    writer.write_string(in_analysis.workload_reference.filename);
    writer.write_u64   (in_analysis.workload_reference.header_hash);
    writer.write_u64   (in_analysis.workload_reference.middle_hash);
    writer.write_u64   (in_analysis.workload_reference.n_bytes);
    writer.write_u64   (in_analysis.workload_reference.tail_hash);

    writer.write_bool(in_analysis.is_truncated);
    writer.write_u64 (in_analysis.n_calls);
    writer.write_u32 (in_analysis.pointer_n_bytes);
    writer.write_u64 (in_analysis.qpc_frequency);

    writer.write_u32(static_cast<uint32_t>(in_analysis.n_calls_by_api_map.size() ) );

    for (const auto& api : in_analysis.n_calls_by_api_map)
    {
        writer.write_string(api.first);
        writer.write_u64   (api.second);
    }

    return writer.take_bytes();
}

std::vector<uint8_t> Analysis::APIDumpDependencies::DepsSerializer::serialize_resources(const DependencyAnalysis& in_analysis)
{
    Analysis::APIDumpAnalysis::Model model;

    model.resource_vec = in_analysis.resource_vec;

    return Analysis::APIDumpAnalysis::PassesSerializer::serialize_resources(model);
}
