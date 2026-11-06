/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/query_commands.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "common/macros.h"

const char* Analysis::APIDumpAnalysis::QueryCommands::get_transfer_kind_name(const TransferKind& in_kind)
{
    const char* result_ptr = "unknown";

    switch (in_kind)
    {
        case TransferKind::COPY:
        {
            result_ptr = "copy";

            break;
        }

        case TransferKind::UPLOAD:
        {
            result_ptr = "upload";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_kind);

            break;
        }
    }

    return result_ptr;
}

Analysis::APIDumpAnalysis::QueryStatus Analysis::APIDumpAnalysis::QueryCommands::run_frame(const AnalysisResult&    in_analysis,
                                                                                           const QueryArguments&    in_arguments,
                                                                                           Utils::Json::JsonWriter* inout_writer_ptr,
                                                                                           std::string*             out_error_ptr)
{
    const RepresentativeStore::Entry*   entry_ptr     = nullptr;
    RepresentativeStore::EntryPtrVector entry_ptr_vec;
    uint64_t                            frame_index   = 0;
    const auto&                         model_ref     = in_analysis.model;
    QueryStatus                         result        = QueryStatus::BAD_ARGUMENTS;

    if (in_arguments.has_shape_id)
    {
        if (in_arguments.shape_id >= model_ref.frame_shape_interner.get_n_structures() )
        {
            *out_error_ptr = "Frame shape "                                                     +
                             std::to_string(in_arguments.shape_id)                              +
                             " does not exist (there are "                                      +
                             std::to_string(model_ref.frame_shape_interner.get_n_structures() ) +
                             ")";
            result         = QueryStatus::NOT_FOUND;

            goto end;
        }

        entry_ptr_vec = model_ref.representative_store.get_entry_ptr_vec(static_cast<uint32_t>(in_arguments.shape_id) );

        if (entry_ptr_vec.empty() )
        {
            *out_error_ptr = "Frame shape " + std::to_string(in_arguments.shape_id) + " has no representative frame";
            result         = QueryStatus::NOT_FOUND;

            goto end;
        }

        entry_ptr = entry_ptr_vec.front();

        for (const auto& candidate_ptr : entry_ptr_vec)
        {
            bool is_better = false;

            switch (in_arguments.which)
            {
                case RepresentativeChoice::HEAVIEST:
                {
                    is_better = (candidate_ptr->weight > entry_ptr->weight);

                    break;
                }

                case RepresentativeChoice::LIGHTEST:
                {
                    is_better = (candidate_ptr->weight < entry_ptr->weight);

                    break;
                }

                case RepresentativeChoice::FIRST:
                {
                    break;
                }

                default:
                {
                    AI_FAIL_UNSUPPORTED(in_arguments.which);

                    break;
                }
            }

            if (is_better)
            {
                entry_ptr = candidate_ptr;
            }
        }

        frame_index = entry_ptr->frame_index;
    }
    else
    if (in_arguments.has_frame_index)
    {
        if (in_arguments.frame_index >= model_ref.frame_record_vec.size() )
        {
            *out_error_ptr = "Frame "                                           +
                             std::to_string(in_arguments.frame_index)           +
                             " does not exist (there are "                      +
                             std::to_string(model_ref.frame_record_vec.size() ) +
                             ")";
            result         = QueryStatus::NOT_FOUND;

            goto end;
        }

        frame_index   = in_arguments.frame_index;
        entry_ptr_vec = model_ref.representative_store.get_entry_ptr_vec(model_ref.frame_record_vec.at(static_cast<size_t>(frame_index) ).shape_id);

        for (const auto& candidate_ptr : entry_ptr_vec)
        {
            if (candidate_ptr->frame_index == frame_index)
            {
                entry_ptr = candidate_ptr;
            }
        }
    }
    else
    {
        *out_error_ptr = "frame needs a frame index, or --shape";

        goto end;
    }

    inout_writer_ptr->begin_object();

    if (in_arguments.has_shape_id)
    {
        const char* which_ptr = "first";

        if (in_arguments.which == RepresentativeChoice::HEAVIEST)
        {
            which_ptr = "heaviest";
        }
        else
        if (in_arguments.which == RepresentativeChoice::LIGHTEST)
        {
            which_ptr = "lightest";
        }

        inout_writer_ptr->write_key   ("shape_id");
        inout_writer_ptr->write_u64   (in_arguments.shape_id);
        inout_writer_ptr->write_key   ("which");
        inout_writer_ptr->write_string(which_ptr);
        inout_writer_ptr->write_key   ("representatives");
        inout_writer_ptr->begin_array();

        for (const auto& candidate_ptr : entry_ptr_vec)
        {
            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key("frame_index");
            inout_writer_ptr->write_u64(candidate_ptr->frame_index);
            inout_writer_ptr->write_key("n_draws");
            inout_writer_ptr->write_u64(candidate_ptr->weight);
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array();
    }

    inout_writer_ptr->write_key("frame");

    write_frame(in_analysis,
                frame_index,
                entry_ptr,
                inout_writer_ptr);

    inout_writer_ptr->end_object();

    result = QueryStatus::OK;

end:
    return result;
}

void Analysis::APIDumpAnalysis::QueryCommands::write_frame(const AnalysisResult&             in_analysis,
                                                           const uint64_t&                   in_frame_index,
                                                           const RepresentativeStore::Entry* in_entry_ptr,
                                                           Utils::Json::JsonWriter*          inout_writer_ptr)
{
    const auto& record = in_analysis.model.frame_record_vec.at(static_cast<size_t>(in_frame_index) );

    inout_writer_ptr->begin_object();

    inout_writer_ptr->write_key   ("frame_index");
    inout_writer_ptr->write_u64   (record.frame_index);
    inout_writer_ptr->write_key   ("shape_id");
    inout_writer_ptr->write_u64   (record.shape_id);
    inout_writer_ptr->write_key    ("shape_key");
    inout_writer_ptr->write_hex_u64(ModelBuilder::compute_shape_key(in_analysis.model,
                                                                    record.shape_id) );
    inout_writer_ptr->write_key   ("begin_seconds");
    inout_writer_ptr->write_double(get_seconds(in_analysis,
                                               record.begin_timestamp_qpc) );
    inout_writer_ptr->write_key   ("end_seconds");
    inout_writer_ptr->write_double(get_seconds(in_analysis,
                                               record.end_timestamp_qpc) );
    inout_writer_ptr->write_key   ("n_draws");
    inout_writer_ptr->write_u64   (record.n_draws);
    inout_writer_ptr->write_key   ("n_passes");
    inout_writer_ptr->write_u64   (record.n_passes);

    inout_writer_ptr->write_key("calls");
    inout_writer_ptr->begin_object();
    inout_writer_ptr->write_key("first_call_index");
    inout_writer_ptr->write_u64(record.first_call_index);
    inout_writer_ptr->write_key("last_call_index");
    inout_writer_ptr->write_u64(record.last_call_index);
    inout_writer_ptr->write_key("n_calls");
    inout_writer_ptr->write_u64(record.last_call_index - record.first_call_index + 1);
    inout_writer_ptr->write_key("printer_command");
    inout_writer_ptr->write_string("APIDumpPrinter \"" + in_analysis.workload_reference.filename + "\" --frame " +
                                   std::to_string(record.frame_index) );
    inout_writer_ptr->end_object();

    inout_writer_ptr->write_key ("has_detail");
    inout_writer_ptr->write_bool(in_entry_ptr != nullptr);

    if (in_entry_ptr != nullptr)
    {
        inout_writer_ptr->write_key("passes");
        inout_writer_ptr->begin_array();

        for (const auto& instance : in_entry_ptr->detail.pass_instance_vec)
        {
            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key   ("pass_id");
            inout_writer_ptr->write_u64   (instance.structure_id);
            inout_writer_ptr->write_key   ("begin_seconds");
            inout_writer_ptr->write_double(get_seconds(in_analysis,
                                                       instance.begin_timestamp_qpc) );
            inout_writer_ptr->write_key   ("begin_call_index");
            inout_writer_ptr->write_u64   (instance.begin_call_index);
            inout_writer_ptr->write_key   ("end_call_index");
            inout_writer_ptr->write_u64   (instance.end_call_index);
            inout_writer_ptr->write_key   ("n_draws");
            inout_writer_ptr->write_u64   (instance.n_draws);
            inout_writer_ptr->write_key   ("n_vertices");
            inout_writer_ptr->write_u64   (instance.n_vertices);
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array();
        inout_writer_ptr->write_key("transfers");
        inout_writer_ptr->begin_array();

        for (const auto& transfer : in_entry_ptr->detail.transfer_record_vec)
        {
            inout_writer_ptr->begin_object();
            inout_writer_ptr->write_key   ("kind");
            inout_writer_ptr->write_string(get_transfer_kind_name(transfer.kind) );
            inout_writer_ptr->write_key   ("call_index");
            inout_writer_ptr->write_u64   (transfer.call_index);
            inout_writer_ptr->write_key   ("source");

            write_resource_view(in_analysis,
                                transfer.source,
                                inout_writer_ptr);

            inout_writer_ptr->write_key   ("destination");

            write_resource_view(in_analysis,
                                transfer.destination,
                                inout_writer_ptr);

            inout_writer_ptr->write_key    ("data_n_bytes");
            inout_writer_ptr->write_u64    (transfer.data.n_bytes);
            inout_writer_ptr->write_key    ("data_content_hash");
            inout_writer_ptr->write_hex_u64(transfer.data.content_hash);
            inout_writer_ptr->end_object();
        }

        inout_writer_ptr->end_array();
    }

    inout_writer_ptr->end_object();
}

void Analysis::APIDumpAnalysis::QueryCommands::write_resource_view(const AnalysisResult&    in_analysis,
                                                                   const ResourceView&      in_view,
                                                                   Utils::Json::JsonWriter* inout_writer_ptr)
{
    const uint32_t resource_id_value = get_resource_id_value(in_view.resource_id);

    if (resource_id_value == 0                                       ||
        resource_id_value >  in_analysis.model.resource_vec.size() )
    {
        inout_writer_ptr->write_null();
    }
    else
    {
        const auto& resource = in_analysis.model.resource_vec.at(resource_id_value - 1);

        inout_writer_ptr->begin_object();
        inout_writer_ptr->write_key   ("resource_id");
        inout_writer_ptr->write_u64   (resource_id_value);
        inout_writer_ptr->write_key   ("name");
        inout_writer_ptr->write_string(resource.debug_name);
        inout_writer_ptr->write_key   ("kind");
        inout_writer_ptr->write_string(get_resource_kind_name(resource.description.kind) );
        inout_writer_ptr->write_key   ("mip_level");
        inout_writer_ptr->write_u64   (in_view.mip_level);
        inout_writer_ptr->write_key   ("layer");
        inout_writer_ptr->write_u64   (in_view.layer);
        inout_writer_ptr->end_object();
    }
}
