/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the .deps file: what is written is what is read, the same analysis gives the same bytes, and a file which is damaged, of another
 * version or not valid is refused.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpDependencies/dependency_analyzer.h"
#include "Analysis/APIDumpDependencies/deps_file.h"
#include "Analysis/APIDumpDependencies/deps_format.h"
#include <stddef.h>
#include <string.h>

namespace
{
    using namespace Analysis::APIDumpAnalysis;
    using namespace Analysis::APIDumpDependencies;

    /* An analysis of a small workload: a texture which is written and then sampled, and a target which is cleared, drawn to and sampled, in
     * four frames.
     */
    DependencyAnalysis make_analysis()
    {
        AccessLog             log;
        DependencyAnalysis    result;
        std::vector<Resource> resource_vec(2);

        for (size_t n_resource = 0;
                    n_resource < resource_vec.size();
                  ++n_resource)
        {
            resource_vec.at(n_resource).description.height = 64;
            resource_vec.at(n_resource).description.width  = 128;
            resource_vec.at(n_resource).debug_name         = "Surface#" + std::to_string(n_resource);
            resource_vec.at(n_resource).id                 = make_resource_id(static_cast<uint32_t>(n_resource) + 1);
            resource_vec.at(n_resource).match_key          = 0x1000 + n_resource;
        }

        uint64_t call_index = 0;

        for (uint64_t n_frame = 0;
                      n_frame < 4;
                    ++n_frame)
        {
            AccessEvent event;

            event.call_index  = call_index++;
            event.mode        = (n_frame == 0) ? AccessMode::WRITE
                                               : AccessMode::READ;
            event.coverage    = AccessCoverage::FULL;
            event.resource_id = make_resource_id(1);
            event.role        = (n_frame == 0) ? AccessRole::UPLOAD_DESTINATION
                                               : AccessRole::SAMPLED;

            log.on_access(event);

            event.call_index  = call_index++;
            event.mode        = AccessMode::WRITE;
            event.resource_id = make_resource_id(2);
            event.role        = AccessRole::CLEAR_TARGET;

            log.on_access(event);

            event.call_index = call_index++;
            event.mode       = AccessMode::READ;
            event.role       = AccessRole::SAMPLED;

            log.on_access(event);

            log.on_frame_ended(n_frame,
                               n_frame * 3,
                               n_frame * 3 + 2);
        }

        result.frame_call_range_vec          = log.get_frame_call_range_vec();
        result.is_truncated                  = true;
        result.n_calls                       = 12;
        result.n_calls_by_api_map["d3d7"]    = 12;
        result.pointer_n_bytes               = 4;
        result.qpc_frequency                 = 10000000;
        result.result                        = DependencyAnalyzer::analyze(log,
                                                                           resource_vec,
                                                                           1);
        result.resource_vec                  = resource_vec;
        result.workload_reference.filename    = "x.workload";
        result.workload_reference.header_hash = 0x1111;
        result.workload_reference.middle_hash = 0x2222;
        result.workload_reference.n_bytes     = 987654321;
        result.workload_reference.tail_hash   = 0x3333;

        return result;
    }

    /* Where the sections of the bytes of a file are: the offset of the header of each, and its tag. */
    struct SectionPosition
    {
        size_t   offset;
        uint32_t tag;
    };

    std::vector<SectionPosition> find_sections(const std::vector<uint8_t>& in_byte_vec)
    {
        std::vector<SectionPosition> result_vec;
        size_t                       position = sizeof(DepsFormat::FileHeader);

        while (position < in_byte_vec.size() )
        {
            SectionHeader header;

            ::memcpy(&header,
                     in_byte_vec.data() + position,
                     sizeof(header) );

            result_vec.push_back({position, header.tag});

            position += sizeof(header) + static_cast<size_t>(header.n_stored_bytes);
        }

        return result_vec;
    }
}

TEST(DepsFile, WhatIsWrittenIsWhatIsRead)
{
    const DependencyAnalysis   analysis = make_analysis();
    DependencyAnalysis         loaded;
    std::string                error;
    const std::vector<uint8_t> byte_vec = DepsFile::write_bytes(analysis);

    ASSERT_FALSE(byte_vec.empty() );
    ASSERT_TRUE (DepsFile::read_bytes(byte_vec,
                                     &loaded,
                                     &error) ) << error;

    EXPECT_EQ(loaded.workload_reference.filename,
              "x.workload");
    EXPECT_EQ(loaded.workload_reference.header_hash,
              0x1111u);
    EXPECT_EQ(loaded.workload_reference.middle_hash,
              0x2222u);
    EXPECT_EQ(loaded.workload_reference.n_bytes,
              987654321u);
    EXPECT_EQ(loaded.workload_reference.tail_hash,
              0x3333u);
    EXPECT_TRUE(loaded.is_truncated);
    EXPECT_EQ(loaded.n_calls,
              12u);
    EXPECT_EQ(loaded.n_calls_by_api_map.at("d3d7"),
              12u);
    EXPECT_EQ(loaded.pointer_n_bytes,
              4u);
    EXPECT_EQ(loaded.qpc_frequency,
              10000000u);

    ASSERT_EQ(loaded.resource_vec.size(),
              2u);
    EXPECT_EQ(loaded.resource_vec.at(1).debug_name,
              "Surface#1");
    EXPECT_EQ(loaded.resource_vec.at(1).match_key,
              0x1001u);

    ASSERT_EQ(loaded.frame_call_range_vec.size(),
              4u);
    EXPECT_EQ(loaded.frame_call_range_vec.at(2).first_call_index,
              6u);
    EXPECT_EQ(loaded.frame_call_range_vec.at(2).last_call_index,
              8u);

    EXPECT_EQ(loaded.result.n_frames,
              analysis.result.n_frames);
    ASSERT_EQ(loaded.result.resource_vec.size(),
              analysis.result.resource_vec.size() );

    for (size_t n_resource = 0;
                n_resource < analysis.result.resource_vec.size();
              ++n_resource)
    {
        const auto& a = analysis.result.resource_vec.at(n_resource);
        const auto& b = loaded.result.resource_vec.at  (n_resource);

        EXPECT_EQ(a.summary.dependency_class,
                  b.summary.dependency_class);
        EXPECT_EQ(a.summary.n_frames_used,
                  b.summary.n_frames_used);
        EXPECT_EQ(a.summary.n_frames_using_earlier_content,
                  b.summary.n_frames_using_earlier_content);
        EXPECT_EQ(a.summary.longest_carry_n_frames,
                  b.summary.longest_carry_n_frames);

        ASSERT_EQ(a.use_run_vec.size(),
                  b.use_run_vec.size() );

        for (size_t n_run = 0;
                    n_run < a.use_run_vec.size();
                  ++n_run)
        {
            EXPECT_EQ(a.use_run_vec.at(n_run).first_frame_index,
                      b.use_run_vec.at(n_run).first_frame_index);
            EXPECT_EQ(a.use_run_vec.at(n_run).last_frame_index,
                      b.use_run_vec.at(n_run).last_frame_index);
            EXPECT_TRUE(a.use_run_vec.at(n_run).use == b.use_run_vec.at(n_run).use);
        }

        ASSERT_EQ(a.version_vec.size(),
                  b.version_vec.size() );

        for (size_t n_version = 0;
                    n_version < a.version_vec.size();
                  ++n_version)
        {
            EXPECT_EQ(a.version_vec.at(n_version).has_writer,
                      b.version_vec.at(n_version).has_writer);
            EXPECT_EQ(a.version_vec.at(n_version).keeps_earlier_content,
                      b.version_vec.at(n_version).keeps_earlier_content);
            EXPECT_EQ(a.version_vec.at(n_version).n_consumer_frames,
                      b.version_vec.at(n_version).n_consumer_frames);
            EXPECT_EQ(a.version_vec.at(n_version).writer_frame_index,
                      b.version_vec.at(n_version).writer_frame_index);
            EXPECT_EQ(a.version_vec.at(n_version).consumer_range_vec.size(),
                      b.version_vec.at(n_version).consumer_range_vec.size() );
        }
    }
}

TEST(DepsFile, TheSameAnalysisGivesTheSameBytesAndAFileCanBeWrittenAndReadBack)
{
    const DependencyAnalysis analysis = make_analysis();
    std::string              error;
    DependencyAnalysis       loaded;

    EXPECT_EQ(DepsFile::write_bytes(analysis),
              DepsFile::write_bytes(analysis) );

    const std::string filename = ::testing::TempDir() + "deps_file_test.deps";

    ASSERT_TRUE(DepsFile::write(filename,
                                analysis,
                               &error) ) << error;
    ASSERT_TRUE(DepsFile::read (filename,
                               &loaded,
                               &error) ) << error;

    EXPECT_EQ(loaded.result.resource_vec.size(),
              2u);

    ::remove(filename.c_str() );

    EXPECT_FALSE(DepsFile::read(filename,
                               &loaded,
                               &error) );
}

TEST(DepsFile, AFileWhichIsNotOneOrIsOfAnotherVersionIsRefused)
{
    std::string                error;
    DependencyAnalysis         loaded;
    std::vector<uint8_t>       byte_vec = DepsFile::write_bytes(make_analysis() );

    {
        auto wrong_magic_byte_vec = byte_vec;

        wrong_magic_byte_vec.at(0) ^= 0xFF;

        EXPECT_FALSE(DepsFile::read_bytes(wrong_magic_byte_vec,
                                         &loaded,
                                         &error) );
    }

    {
        /* The analysis changed since the file was made: what its sections mean is not what they meant. */
        auto other_analysis_byte_vec = byte_vec;

        other_analysis_byte_vec.at(offsetof(DepsFormat::FileHeader, analysis_version) ) ^= 0x01;

        EXPECT_FALSE(DepsFile::read_bytes(other_analysis_byte_vec,
                                         &loaded,
                                         &error) );
        EXPECT_NE(error.find("make it again"),
                  std::string::npos);
    }

    {
        auto other_file_version_byte_vec = byte_vec;

        other_file_version_byte_vec.at(offsetof(DepsFormat::FileHeader, version) ) ^= 0x01;

        EXPECT_FALSE(DepsFile::read_bytes(other_file_version_byte_vec,
                                         &loaded,
                                         &error) );
    }

    EXPECT_FALSE(DepsFile::read_bytes(std::vector<uint8_t>(4),
                                     &loaded,
                                     &error) );
}

TEST(DepsFile, ADamagedOrCutFileIsRefused)
{
    std::string          error;
    DependencyAnalysis   loaded;
    std::vector<uint8_t> byte_vec = DepsFile::write_bytes(make_analysis() );

    const auto section_vec = find_sections(byte_vec);

    ASSERT_EQ(section_vec.size(),
              4u);

    /* A byte of what a section holds is not what it was. */
    {
        auto damaged_byte_vec = byte_vec;

        damaged_byte_vec.at(section_vec.at(3).offset + offsetof(SectionHeader, raw_hash) ) ^= 0x01;

        EXPECT_FALSE(DepsFile::read_bytes(damaged_byte_vec,
                                         &loaded,
                                         &error) );
        EXPECT_NE(error.find("RDEP"),
                  std::string::npos);
    }

    /* The file ends in the middle of the last section. */
    {
        auto cut_byte_vec = byte_vec;

        cut_byte_vec.resize(cut_byte_vec.size() - 3);

        EXPECT_FALSE(DepsFile::read_bytes(cut_byte_vec,
                                         &loaded,
                                         &error) );
    }

    /* A section is missing. */
    {
        auto short_byte_vec = byte_vec;

        short_byte_vec.resize(section_vec.at(3).offset);

        EXPECT_FALSE(DepsFile::read_bytes(short_byte_vec,
                                         &loaded,
                                         &error) );
        EXPECT_NE(error.find("RDEP"),
                  std::string::npos);
    }
}

TEST(DepsFile, DependenciesWhichDoNotFitTheFramesOrTheResourcesAreInvalid)
{
    std::string        error;
    DependencyAnalysis loaded;

    /* A frame which the run of a resource is in is not one of the frames of the file. */
    {
        DependencyAnalysis analysis = make_analysis();

        analysis.result.resource_vec.at(0).use_run_vec.back().last_frame_index = 99;

        EXPECT_FALSE(DepsFile::read_bytes(DepsFile::write_bytes(analysis),
                                         &loaded,
                                         &error) );
        EXPECT_NE(error.find("RDEP"),
                  std::string::npos);
    }

    /* The resources of the dependencies are not the resources of the file. */
    {
        DependencyAnalysis analysis = make_analysis();

        analysis.result.resource_vec.pop_back();

        EXPECT_FALSE(DepsFile::read_bytes(DepsFile::write_bytes(analysis),
                                         &loaded,
                                         &error) );
    }

    /* Frames whose calls overlap. */
    {
        DependencyAnalysis analysis = make_analysis();

        analysis.frame_call_range_vec.at(1).first_call_index = 0;

        EXPECT_FALSE(DepsFile::read_bytes(DepsFile::write_bytes(analysis),
                                         &loaded,
                                         &error) );
        EXPECT_NE(error.find("FCAL"),
                  std::string::npos);
    }
}
