/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/extractor_command_line.h"
#include "analysis/api_dump_analysis/extractor_progress.h"
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "analysis/api_dump_textures/texture_blob_writer.h"
#include "analysis/api_dump_textures/texture_collector.h"
#include "analysis/api_dump_textures/textures_file.h"
#include "common/macros.h"
#include "extractor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>

void Analysis::APIDumpTextureExtractor::print_stats(const Analysis::APIDumpTextures::TextureTimeline& in_timeline)
{
    uint64_t n_generated_levels = 0;
    uint64_t n_render_writes    = 0;
    uint64_t n_snapshot_bytes   = 0;
    uint64_t n_snapshots        = 0;
    uint64_t n_uploads          = 0;
    uint64_t n_uploads_no_data  = 0;
    uint64_t n_workload_chunks  = 0;

    for (const auto& event : in_timeline.event_vec)
    {
        switch (event.kind)
        {
            case Analysis::APIDumpTextures::TextureEventKind::GENERATE_MIPS:
            {
                ++n_generated_levels;

                break;
            }

            case Analysis::APIDumpTextures::TextureEventKind::RENDER_WRITE:
            {
                ++n_render_writes;

                break;
            }

            case Analysis::APIDumpTextures::TextureEventKind::SNAPSHOT:
            {
                ++n_snapshots;

                /* What keeping all of them, as they are, would take before compression and before the same content is kept once. */
                n_snapshot_bytes += Utils::Graphics::TextureFormats::get_n_bytes(event.level_format,
                                                                                 event.width,
                                                                                 event.height) * event.layer_count;

                break;
            }

            case Analysis::APIDumpTextures::TextureEventKind::UPLOAD:
            {
                ++n_uploads;

                n_uploads_no_data += (event.blob_index == Analysis::APIDumpTextures::NO_BLOB) ? 1 : 0;

                break;
            }

            default:
            {
                AI_FAIL_UNSUPPORTED(event.kind);
            }
        }
    }

    for (const auto& blob : in_timeline.blob_vec)
    {
        n_workload_chunks += (blob.location == Analysis::APIDumpTextures::BlobLocation::WORKLOAD_CHUNK) ? 1 : 0;
    }

    fprintf(stdout,
            "Events:                  %llu\n"
            "  uploads:               %llu (%llu with no data: the level is given a size only)\n"
            "  render writes:         %llu (copies from the window)\n"
            "  generated levels:      %llu (made by the driver)\n"
            "  snapshot points:       %llu (where what rendering made is first read; no pixels are kept; %.1f MiB if all were kept, before compression and deduplication)\n"
            "Contents:                %llu\n"
            "  left in the workload:  %llu\n"
            "  stored:                %llu\n"
            "Resources:               %llu\n",
            static_cast<unsigned long long>(in_timeline.event_vec.size() ),
            static_cast<unsigned long long>(n_uploads),
            static_cast<unsigned long long>(n_uploads_no_data),
            static_cast<unsigned long long>(n_render_writes),
            static_cast<unsigned long long>(n_generated_levels),
            static_cast<unsigned long long>(n_snapshots),
            static_cast<double>(n_snapshot_bytes) / (1024.0 * 1024.0),
            static_cast<unsigned long long>(in_timeline.blob_vec.size() ),
            static_cast<unsigned long long>(n_workload_chunks),
            static_cast<unsigned long long>(in_timeline.blob_vec.size() - n_workload_chunks),
            static_cast<unsigned long long>(in_timeline.resource_vec.size() ) );
}

void Analysis::APIDumpTextureExtractor::print_usage()
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "APIDumpTextureExtractor drive:\\path\\to\\file.workload [-o file.textures] [--include-render-content] [--max-frames N]\n"
            "                        [--progress-interval SECONDS] [--stats] [--threads N]\n"
            "\n"
            "Finds what happens to the textures of a workload (the pixels that are given to a level, what is copied to one from the window, the levels that the driver\n"
            "makes, and the points where what rendering made is first read), and stores it in a .textures file, with a file of blobs beside it (the same name with .data\n"
            "after it). The pixels that the workload carries are left where they are in it: the file says where.\n"
            "\n"
            "  -o                         The file to write (default: the workload's name with .textures)\n"
            "  --include-render-content   Also keep what rendering made, at each snapshot point (needs a replay of the workload, which is not made yet: ends the process)\n"
            "  --max-frames N             Stop after the Nth frame (default: all of them)\n"
            "  --progress-interval        Seconds between progress lines (default: 5; 0: after every segment)\n"
            "  --stats                    Print what was found when done\n"
            "  --threads N                Threads that read the workload ahead of the analysis (default: the cores of the machine, up to 8; 0: none)\n"
            "\n"
            "Exit codes: 0 done, 1 bad arguments, 2 workload cannot be read, 3 files cannot be written, 4 what the workload holds of the textures could not all be kept.\n");
}

int Analysis::APIDumpTextureExtractor::run(int   in_argc,
                                           char* in_argv[])
{
    std::string                                     error;
    ExitCode                                        exit_code                = ExitCode::SUCCESS;
    bool                                            includes_render_content  = false;
    Analysis::APIDumpAnalysis::ExtractorOptions     options;
    Analysis::APIDumpAnalysis::ExtractorProfile     profile;
    Analysis::APIDumpTextures::TextureBlobWriter    writer;
    Analysis::APIDumpTextures::TextureCollector     collector(&writer);
    Analysis::APIDumpTextures::TextureTimeline      timeline;

    /* Without history: the textures need no more of the frames than the model builder keeps anyway. */
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session(false);

    /* The pixels of an upload are in data chunks, which are all read. */
    profile.default_skip_data_chunks_above_n_bytes = 0;
    profile.output_extension                       = ".textures";

    const auto program_option_func = [&includes_render_content](const std::string& in_argument,
                                                                const bool&        in_has_next,
                                                                const std::string& in_next_argument,
                                                                bool*              out_is_known_ptr,
                                                                bool*              out_uses_next_ptr,
                                                                std::string*       out_error_ptr)
    {
        (void) in_has_next;
        (void) in_next_argument;

        bool result = true;

        *out_is_known_ptr  = (in_argument == "--include-render-content");
        *out_uses_next_ptr = false;

        if (*out_is_known_ptr)
        {
            includes_render_content = true;
        }
        else
        if (in_argument == "--skip-chunks-above")
        {
            /* The pixels of an upload are in the data chunks, and a chunk that is left out comes as one with no bytes, which cannot be told from the one that the dumper
             * did not store: so no chunk is left out.
             */
            *out_error_ptr = "--skip-chunks-above is not for this program: the pixels of the uploads are in the data chunks, which are all read";

            result = false;
        }

        return result;
    };

    if (Analysis::APIDumpAnalysis::ExtractorCommandLine::parse(in_argc,
                                                               in_argv,
                                                               profile,
                                                               program_option_func,
                                                              &options,
                                                              &error) == false)
    {
        fprintf(stdout,
                "Error: %s\n\n",
                error.c_str() );

        print_usage();

        exit_code = ExitCode::BAD_ARGUMENTS;

        goto end;
    }

    /* What rendering made is not in the workload: keeping it takes a replay that reads the textures back at the snapshot points, which does not exist yet. A file
     * that has none of it, from a run that was asked for it, would be taken for one that has it: so the process ends instead.
     */
    AI_ASSERT_ALWAYS(includes_render_content == false,
                     "--include-render-content needs a replay of the workload that reads the textures back at the snapshot points, which is not implemented yet.");

    {
        const std::string blob_filename = Analysis::APIDumpTextures::TexturesFile::get_blob_filename(options.output_filename);

        Analysis::APIDumpAnalysis::ExtractorProgress progress(options.progress_interval_seconds);

        const auto progress_func = progress.get_progress_func();

        if (writer.open(blob_filename,
                       &error) == false)
        {
            fprintf(stdout,
                    "Error: %s\n",
                    error.c_str() );

            exit_code = ExitCode::CANNOT_WRITE_TEXTURES;

            goto end;
        }

        fprintf(stdout,
                "Reading %s\n",
                options.input_filename.c_str() );
        fflush(stdout);

        session.set_max_frames                    (options.max_frames);
        session.set_n_reader_threads              (options.n_threads);
        session.set_skip_data_chunks_above_n_bytes(options.skip_data_chunks_above_n_bytes);
        session.set_texture_sink                  (&collector);

        if (session.run(options.input_filename.c_str(),
                        progress_func,
                       &error) == false)
        {
            fprintf(stdout,
                    "Error: %s\n",
                    error.c_str() );

            exit_code = ExitCode::CANNOT_READ_WORKLOAD;

            goto end;
        }

        {
            auto model = session.take_model();

            timeline.event_vec              = collector.get_event_vec();
            timeline.includes_render_content = false;
            timeline.resource_vec           = std::move(model.resource_vec);
            timeline.workload_reference     = session.get_workload_reference();
        }

        if (writer.close(&error) == false)
        {
            fprintf(stdout,
                    "Error: %s\n",
                    error.c_str() );

            exit_code = ExitCode::CANNOT_WRITE_TEXTURES;

            goto end;
        }

        timeline.blob_file_n_bytes = writer.get_n_bytes();
        timeline.blob_vec          = writer.get_blob_vec();

        if (session.get_summary().is_truncated)
        {
            fprintf(stdout,
                    "Warning: the workload ends in the middle of a segment; it was analyzed as far as it goes.\n");
        }

        fprintf(stdout,
                "Read %llu calls in %.1fs; %llu events of the textures\n",
                static_cast<unsigned long long>(session.get_summary().n_calls),
                progress.get_elapsed_seconds(),
                static_cast<unsigned long long>(timeline.event_vec.size() ) );

        if (collector.get_error().empty() == false)
        {
            fprintf(stdout,
                    "Error: %s\n",
                    collector.get_error().c_str() );

            exit_code = ExitCode::TEXTURES_ARE_INCOMPLETE;

            goto end;
        }

        if (Analysis::APIDumpTextures::TexturesFile::write(options.output_filename,
                                                           timeline,
                                                          &error) == false)
        {
            fprintf(stdout,
                    "Error: %s\n",
                    error.c_str() );

            exit_code = ExitCode::CANNOT_WRITE_TEXTURES;

            goto end;
        }

        /* What was written is read back, and is what was made: nothing of what is kept of the textures is kept as something else. */
        {
            Analysis::APIDumpTextures::TextureTimeline written_timeline;

            const bool is_read = Analysis::APIDumpTextures::TexturesFile::read(options.output_filename,
                                                                               &written_timeline,
                                                                               &error);

            if (is_read == false ||
                Analysis::APIDumpTextures::TexturesFile::write_bytes(written_timeline) != Analysis::APIDumpTextures::TexturesFile::write_bytes(timeline) )
            {
                fprintf(stdout,
                        "Error: %s does not read back as it was written%s%s\n",
                        options.output_filename.c_str(),
                        (is_read) ? "" : ": ",
                        (is_read) ? "" : error.c_str() );

                exit_code = ExitCode::CANNOT_WRITE_TEXTURES;

                goto end;
            }
        }

        fprintf(stdout,
                "Wrote %s and %s\n",
                options.output_filename.c_str(),
                blob_filename.c_str() );

        if (options.print_stats)
        {
            print_stats(timeline);
        }
    }

end:
    return static_cast<int>(exit_code);
}

int main(int   in_argc,
         char* in_argv[])
{
    return Analysis::APIDumpTextureExtractor::run(in_argc,
                                                  in_argv);
}
