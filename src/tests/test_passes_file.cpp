/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the .passes file: what is written is read back unchanged, and an invalid file is refused with a reason.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "analysis/api_dump_analysis/passes_file.h"
#include "analysis/api_dump_analysis/passes_format.h"
#include "analysis/api_dump_analysis/passes_serializer.h"
#include "data_fixtures.h"
#include <stdio.h>
#include <string.h>

namespace Tests
{
namespace PassesFileHelpers
{
    struct SectionPosition
    {
        size_t                                  offset;
        Analysis::APIDumpAnalysis::PassesFormat::SectionHeader header;
    };

    typedef std::vector<SectionPosition> SectionPositionVector;

    Analysis::APIDumpAnalysis::AnalysisResult analyze_slice()
    {
        Analysis::APIDumpAnalysis::AnalysisResult result = Tests::DataFixtures::get_fr08_slice_analysis();

        result.workload_reference.filename   = "fr08_slice.workload";
        result.workload_reference.header_hash = 0x1122334455667788ull;
        result.workload_reference.n_bytes     = 123456;

        return result;
    }

    SectionPositionVector get_section_position_vec(const std::vector<uint8_t>& in_byte_vec)
    {
        SectionPositionVector result_vec;
        size_t                position = sizeof(Analysis::APIDumpAnalysis::PassesFormat::FileHeader);

        while (position < in_byte_vec.size() )
        {
            SectionPosition section;

            section.offset = position;

            ::memcpy(&section.header,
                     in_byte_vec.data() + position,
                     sizeof(section.header) );

            position += sizeof(section.header) + static_cast<size_t>(section.header.n_stored_bytes);

            result_vec.push_back(section);
        }

        return result_vec;
    }

    std::string read_error_of(const std::vector<uint8_t>& in_byte_vec)
    {
        Analysis::APIDumpAnalysis::AnalysisResult analysis;
        std::string               error;

        EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(in_byte_vec,
                                                                      &analysis,
                                                                      &error) );
        EXPECT_FALSE(error.empty() );

        return error;
    }
}
}

TEST(PassesFile, AFileThatIsWrittenToDiskIsReadBack)
{
    const auto                analysis = Tests::PassesFileHelpers::analyze_slice();
    std::string               error;
    const char*               filename = "test_passes_file_roundtrip.passes";
    Analysis::APIDumpAnalysis::AnalysisResult loaded;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::write(filename,
                                                             analysis,
                                                            &error) ) << error;
    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::read (filename,
                                                            &loaded,
                                                            &error) ) << error;

    EXPECT_EQ(Analysis::APIDumpAnalysis::PassesFile::write_bytes(loaded),
              Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis) );

    ::remove(filename);
}

TEST(PassesFile, WritingReplacesTheOldFileAndLeavesNoTemporaryOne)
{
    const std::string         filename = "test_passes_file_replace.passes";
    Analysis::APIDumpAnalysis::AnalysisResult loaded;
    std::string               error;

    {
        FILE* file_ptr = ::fopen(filename.c_str(),
                                 "wb");

        ASSERT_NE(file_ptr,
                  nullptr);

        ::fputs("an older file",
                file_ptr);
        ::fclose(file_ptr);
    }

    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::write(filename,
                                                             Analysis::APIDumpAnalysis::AnalysisResult(),
                                                            &error) ) << error;
    EXPECT_TRUE (Analysis::APIDumpAnalysis::PassesFile::read(filename,
                                                            &loaded,
                                                            &error) ) << error;
    EXPECT_EQ   (::fopen((filename + ".tmp").c_str(),
                         "rb"),
                 nullptr);

    ::remove(filename.c_str() );
}

TEST(PassesFile, AFileThatDoesNotExistIsReported)
{
    Analysis::APIDumpAnalysis::AnalysisResult analysis;
    std::string               error;

    EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesFile::read("this_file_does_not_exist.passes",
                                                            &analysis,
                                                            &error) );
    EXPECT_FALSE(error.empty() );
}

TEST(PassesFile, AnAnalysisWithNothingInItIsReadBack)
{
    const Analysis::APIDumpAnalysis::AnalysisResult analysis;
    Analysis::APIDumpAnalysis::AnalysisResult       loaded;
    std::string                     error;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis),
                                                                 &loaded,
                                                                 &error) ) << error;

    EXPECT_EQ(loaded.model.frame_record_vec.size(),
              0u);
    EXPECT_EQ(loaded.model.pass_structure_interner.get_n_structures(),
              0u);
}

TEST(PassesFile, BytesAreTheSameForTheSameAnalysis)
{
    EXPECT_EQ(Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() ),
              Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() ) );
}

TEST(PassesFile, EverySectionIsCheckedByItsHash)
{
    const auto byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );

    for (const auto& section : Tests::PassesFileHelpers::get_section_position_vec(byte_vec) )
    {
        auto damaged_byte_vec = byte_vec;

        /* Flips a bit of the hash. */
        damaged_byte_vec.at(section.offset + offsetof(Analysis::APIDumpAnalysis::PassesFormat::SectionHeader, raw_hash) ) ^= 0x01;

        EXPECT_NE(Tests::PassesFileHelpers::read_error_of(damaged_byte_vec).find("hash"),
                  std::string::npos);
    }
}

TEST(PassesFile, ABitThatIsFlippedAnywhereInTheFileIsNoticed)
{
    constexpr size_t N_STEPS = 97;

    const auto byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );

    for (size_t n_byte = 0;
                n_byte < byte_vec.size();
                n_byte += N_STEPS)
    {
        auto damaged_byte_vec = byte_vec;

        damaged_byte_vec.at(n_byte) ^= 0x10;

        Analysis::APIDumpAnalysis::AnalysisResult analysis;
        std::string               error;

        EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(damaged_byte_vec,
                                                                      &analysis,
                                                                      &error) ) << "byte " << n_byte;
    }
}

TEST(PassesFile, ASectionThatIsClaimedToBeHugeIsRefused)
{
    auto       byte_vec         = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );
    uint64_t   n_raw_bytes      = 1ull << 40;
    const auto section_vec      = Tests::PassesFileHelpers::get_section_position_vec(byte_vec);

    ::memcpy(byte_vec.data() + section_vec.at(0).offset + offsetof(Analysis::APIDumpAnalysis::PassesFormat::SectionHeader, n_raw_bytes),
            &n_raw_bytes,
             sizeof(n_raw_bytes) );

    EXPECT_NE(Tests::PassesFileHelpers::read_error_of(byte_vec).find("impossible"),
              std::string::npos);
}

TEST(PassesFile, ASectionWhoseSizeIsNotTheOneOfItsDataIsRefusedBeforeItIsAllocated)
{
    auto       byte_vec    = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );
    uint64_t   n_raw_bytes = 1ull << 29; /* below the cap, so only the size of the data can tell */
    const auto section_vec = Tests::PassesFileHelpers::get_section_position_vec(byte_vec);

    ::memcpy(byte_vec.data() + section_vec.at(0).offset + offsetof(Analysis::APIDumpAnalysis::PassesFormat::SectionHeader, n_raw_bytes),
            &n_raw_bytes,
             sizeof(n_raw_bytes) );

    EXPECT_NE(Tests::PassesFileHelpers::read_error_of(byte_vec).find("impossible"),
              std::string::npos);
}

TEST(PassesFile, ASectionThatIsMissingIsReported)
{
    const auto byte_vec    = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );
    const auto section_vec = Tests::PassesFileHelpers::get_section_position_vec(byte_vec);

    for (size_t n_missing = 0;
                n_missing < section_vec.size();
              ++n_missing)
    {
        std::vector<uint8_t> shorter_byte_vec(byte_vec.begin(),
                                              byte_vec.begin() + section_vec.at(n_missing).offset);

        if (n_missing + 1 < section_vec.size() )
        {
            shorter_byte_vec.insert(shorter_byte_vec.end(),
                                    byte_vec.begin() + section_vec.at(n_missing + 1).offset,
                                    byte_vec.end() );
        }

        EXPECT_NE(Tests::PassesFileHelpers::read_error_of(shorter_byte_vec).find("missing"),
                  std::string::npos) << "section " << n_missing;
    }
}

TEST(PassesFile, ASectionThatIsRepeatedIsReported)
{
    auto       byte_vec    = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );
    const auto section_vec = Tests::PassesFileHelpers::get_section_position_vec(byte_vec);
    const auto first_end   = section_vec.at(1).offset;

    byte_vec.insert(byte_vec.end(),
                    byte_vec.begin() + section_vec.at(0).offset,
                    byte_vec.begin() + first_end);

    EXPECT_NE(Tests::PassesFileHelpers::read_error_of(byte_vec).find("repeated"),
              std::string::npos);
}

TEST(PassesFile, ASectionThatIsOfAnUnknownVersionIsRefused)
{
    auto       byte_vec    = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );
    const auto section_vec = Tests::PassesFileHelpers::get_section_position_vec(byte_vec);
    uint32_t   version     = 99;

    ::memcpy(byte_vec.data() + section_vec.at(3).offset + offsetof(Analysis::APIDumpAnalysis::PassesFormat::SectionHeader, version),
            &version,
             sizeof(version) );

    EXPECT_NE(Tests::PassesFileHelpers::read_error_of(byte_vec).find("version"),
              std::string::npos);
}

TEST(PassesFile, ASectionThatIsNotKnownIsSkipped)
{
    constexpr char UNKNOWN_BYTES[] = {1, 2, 3};

    const auto analysis    = Tests::PassesFileHelpers::analyze_slice();
    auto       byte_vec    = Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis);
    const auto section_vec = Tests::PassesFileHelpers::get_section_position_vec(byte_vec);

    Analysis::APIDumpAnalysis::PassesFormat::SectionHeader header = {};

    header.n_raw_bytes    = sizeof(UNKNOWN_BYTES);
    header.n_stored_bytes = sizeof(UNKNOWN_BYTES);
    header.tag            = Analysis::APIDumpAnalysis::PassesFormat::make_tag('Z', 'Z', 'Z', 'Z');
    header.version        = 7;

    std::vector<uint8_t> extra_byte_vec(sizeof(header) + sizeof(UNKNOWN_BYTES) );

    ::memcpy(extra_byte_vec.data(),
            &header,
             sizeof(header) );
    ::memcpy(extra_byte_vec.data() + sizeof(header),
             UNKNOWN_BYTES,
             sizeof(UNKNOWN_BYTES) );

    byte_vec.insert(byte_vec.begin() + section_vec.at(2).offset,
                    extra_byte_vec.begin(),
                    extra_byte_vec.end() );

    std::string               error;
    Analysis::APIDumpAnalysis::AnalysisResult loaded;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(byte_vec,
                                                                 &loaded,
                                                                 &error) ) << error;

    EXPECT_EQ(Analysis::APIDumpAnalysis::PassesFile::write_bytes(loaded),
              Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis) );
}

TEST(PassesFile, AFileThatEndsInTheMiddleIsRefused)
{
    constexpr size_t N_STEPS = 89;

    const auto byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );

    for (size_t n_bytes = 0;
                n_bytes < byte_vec.size();
                n_bytes += N_STEPS)
    {
        std::vector<uint8_t> shorter_byte_vec(byte_vec.begin(),
                                              byte_vec.begin() + n_bytes);

        Analysis::APIDumpAnalysis::AnalysisResult analysis;
        std::string               error;

        EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(shorter_byte_vec,
                                                                      &analysis,
                                                                      &error) ) << n_bytes << " bytes";
    }
}

TEST(PassesFile, AFileWithAnotherMagicIsRefused)
{
    auto byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );

    byte_vec.at(0) = 'X';

    EXPECT_NE(Tests::PassesFileHelpers::read_error_of(byte_vec).find("magic"),
              std::string::npos);
}

TEST(PassesFile, AFileOfAnotherKeySchemeIsRefused)
{
    auto     byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );
    uint32_t version  = Analysis::APIDumpAnalysis::PassesFormat::KEY_SCHEME_VERSION + 1;

    ::memcpy(byte_vec.data() + offsetof(Analysis::APIDumpAnalysis::PassesFormat::FileHeader, key_scheme_version),
            &version,
             sizeof(version) );

    EXPECT_NE(Tests::PassesFileHelpers::read_error_of(byte_vec).find("key scheme"),
              std::string::npos);
}

TEST(PassesFile, AFileOfAnotherVersionIsRefused)
{
    auto     byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );
    uint32_t version  = Analysis::APIDumpAnalysis::PassesFormat::FILE_VERSION + 1;

    ::memcpy(byte_vec.data() + offsetof(Analysis::APIDumpAnalysis::PassesFormat::FileHeader, version),
            &version,
             sizeof(version) );

    EXPECT_NE(Tests::PassesFileHelpers::read_error_of(byte_vec).find("version"),
              std::string::npos);
}

TEST(PassesFile, TheAnalysisIsReadBackAsItWas)
{
    const auto                analysis = Tests::PassesFileHelpers::analyze_slice();
    std::string               error;
    Analysis::APIDumpAnalysis::AnalysisResult loaded;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis),
                                                                 &loaded,
                                                                 &error) ) << error;

    /* Everything is written, so equal bytes mean equal analyses. */
    EXPECT_EQ(Analysis::APIDumpAnalysis::PassesFile::write_bytes(loaded),
              Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis) );

    EXPECT_EQ(loaded.model.draw_structure_map.size(),
              analysis.model.draw_structure_map.size() );
    EXPECT_EQ(loaded.model.frame_record_vec.size(),
              10u);
    EXPECT_EQ(loaded.model.frame_shape_interner.get_n_structures(),
              4u);
    EXPECT_EQ(loaded.model.pass_structure_interner.get_n_structures(),
              13u);
    EXPECT_EQ(loaded.model.resource_vec.size(),
              analysis.model.resource_vec.size() );
    EXPECT_EQ(loaded.summary.n_calls,
              16836u);
    EXPECT_EQ(loaded.summary.n_calls_by_group_map,
              analysis.summary.n_calls_by_group_map);
    EXPECT_EQ(loaded.workload_reference.filename,
              "fr08_slice.workload");
    EXPECT_EQ(loaded.workload_reference.header_hash,
              0x1122334455667788ull);

    for (size_t n_frame = 0;
                n_frame < analysis.model.frame_record_vec.size();
              ++n_frame)
    {
        EXPECT_EQ(loaded.model.frame_record_vec.at(n_frame).shape_id,
                  analysis.model.frame_record_vec.at(n_frame).shape_id);
        EXPECT_EQ(loaded.model.frame_record_vec.at(n_frame).frame_index,
                  n_frame);
    }

    for (uint32_t n_pass = 0;
                  n_pass < analysis.model.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        EXPECT_TRUE(loaded.model.pass_structure_interner.get_structure(n_pass) == analysis.model.pass_structure_interner.get_structure(n_pass) );
    }

    for (uint32_t n_shape = 0;
                  n_shape < analysis.model.frame_shape_interner.get_n_structures();
                ++n_shape)
    {
        EXPECT_EQ(loaded.model.representative_store.get_entry_ptr_vec(n_shape).size(),
                  analysis.model.representative_store.get_entry_ptr_vec(n_shape).size() );
    }
}

TEST(PassesFile, TheFileIsSmall)
{
    const auto byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(Tests::PassesFileHelpers::analyze_slice() );

    EXPECT_GT(byte_vec.size(),
              sizeof(Analysis::APIDumpAnalysis::PassesFormat::FileHeader) );
    EXPECT_LT(byte_vec.size(),
              1024u * 1024u);
}

TEST(PassesFile, WhatTheFramesDidWithEachResourceIsReadBack)
{
    const auto                analysis = Tests::PassesFileHelpers::analyze_slice();
    std::string               error;
    Analysis::APIDumpAnalysis::AnalysisResult loaded;

    ASSERT_GT(analysis.model.resource_access_vec.size(),
              0u);
    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis),
                                                                 &loaded,
                                                                 &error) ) << error;

    ASSERT_EQ(loaded.model.resource_access_vec.size(),
              analysis.model.resource_access_vec.size() );

    for (size_t n_resource = 0;
                n_resource < analysis.model.resource_access_vec.size();
              ++n_resource)
    {
        const auto& expected = analysis.model.resource_access_vec.at(n_resource);
        const auto& actual   = loaded.model.resource_access_vec.at(n_resource);

        EXPECT_EQ(actual.first_read_frame_index,
                  expected.first_read_frame_index);
        EXPECT_EQ(actual.first_write_frame_index,
                  expected.first_write_frame_index);
        EXPECT_EQ(actual.last_write_frame_index,
                  expected.last_write_frame_index);
        EXPECT_EQ(actual.n_frames_loop_carried,
                  expected.n_frames_loop_carried);
        EXPECT_EQ(actual.n_frames_read,
                  expected.n_frames_read);
        EXPECT_EQ(actual.n_frames_written,
                  expected.n_frames_written);
    }
}

TEST(PassesFile, AccessRecordsThatCannotBeRightAreRefused)
{
    const auto analysis = Tests::PassesFileHelpers::analyze_slice();

    {
        /* The count differs from the number of resources. */
        auto model = analysis.model;

        model.resource_access_vec.pop_back();

        Analysis::APIDumpAnalysis::Model loaded;

        loaded.resource_vec = analysis.model.resource_vec;

        EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resource_access(Analysis::APIDumpAnalysis::PassesSerializer::serialize_resource_access(model),
                                                                                             &loaded) );
    }

    {
        /* More reading frames than there are frames. */
        auto model = analysis.model;

        model.resource_access_vec.at(0).n_frames_read = model.frame_record_vec.size() + 1;

        Analysis::APIDumpAnalysis::Model loaded;

        loaded.frame_record_vec = analysis.model.frame_record_vec;
        loaded.resource_vec     = analysis.model.resource_vec;

        EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resource_access(Analysis::APIDumpAnalysis::PassesSerializer::serialize_resource_access(model),
                                                                                             &loaded) );
    }

    {
        /* More loop-carried frames than writing frames. */
        auto model = analysis.model;

        model.resource_access_vec.at(0).n_frames_loop_carried = model.resource_access_vec.at(0).n_frames_written + 1;

        Analysis::APIDumpAnalysis::Model loaded;

        loaded.frame_record_vec = analysis.model.frame_record_vec;
        loaded.resource_vec     = analysis.model.resource_vec;

        EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resource_access(Analysis::APIDumpAnalysis::PassesSerializer::serialize_resource_access(model),
                                                                                             &loaded) );
    }

    {
        /* Unchanged data is accepted. */
        Analysis::APIDumpAnalysis::Model loaded;

        loaded.frame_record_vec = analysis.model.frame_record_vec;
        loaded.resource_vec     = analysis.model.resource_vec;

        EXPECT_TRUE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resource_access(Analysis::APIDumpAnalysis::PassesSerializer::serialize_resource_access(analysis.model),
                                                                                            &loaded) );
    }
}

TEST(PassesFile, TheSmallestRecordOfEachKindIsReadBack)
{
    constexpr uint32_t N_RECORDS = 50;

    Analysis::APIDumpAnalysis::Model model;

    for (uint32_t n_record = 0;
                  n_record < N_RECORDS;
                ++n_record)
    {
        Analysis::APIDumpAnalysis::PassStructure pass_structure;
        Analysis::APIDumpAnalysis::Resource      resource;

        pass_structure.key = n_record + 1;
        resource.id        = Analysis::APIDumpAnalysis::make_resource_id(n_record + 1);

        model.draw_structure_map[n_record + 1] = Analysis::APIDumpAnalysis::DrawStructure();
        model.resource_vec.push_back(resource);

        model.pass_structure_interner.intern(pass_structure,
                                             pass_structure.key);
    }

    {
        Analysis::APIDumpAnalysis::Model loaded;

        EXPECT_TRUE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_draws(Analysis::APIDumpAnalysis::PassesSerializer::serialize_draws(model),
                                                                                  &loaded) );
        EXPECT_EQ(loaded.draw_structure_map.size(),
                  N_RECORDS);
    }

    {
        Analysis::APIDumpAnalysis::Model loaded;

        EXPECT_TRUE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_passes(Analysis::APIDumpAnalysis::PassesSerializer::serialize_passes(model),
                                                                                   &loaded) );
        EXPECT_EQ(loaded.pass_structure_interner.get_n_structures(),
                  N_RECORDS);
    }

    {
        Analysis::APIDumpAnalysis::Model loaded;

        EXPECT_TRUE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resources(Analysis::APIDumpAnalysis::PassesSerializer::serialize_resources(model),
                                                                                      &loaded) );
        EXPECT_EQ(loaded.resource_vec.size(),
                  N_RECORDS);
    }
}

TEST(PassesFile, AValueOfAnEnumerationThatThereIsNoSuchValueOfIsRefused)
{
    {
        Analysis::APIDumpAnalysis::Model model;
        Analysis::APIDumpAnalysis::Model loaded;
        Analysis::APIDumpAnalysis::DrawStructure draw;

        draw.primitive_topology = static_cast<Analysis::APIDumpAnalysis::PrimitiveTopology>(99);

        model.draw_structure_map[1] = draw;

        EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_draws(Analysis::APIDumpAnalysis::PassesSerializer::serialize_draws(model),
                                                                                   &loaded) );
    }

    {
        Analysis::APIDumpAnalysis::Model    model;
        Analysis::APIDumpAnalysis::Model    loaded;
        Analysis::APIDumpAnalysis::Resource resource;

        resource.id               = Analysis::APIDumpAnalysis::make_resource_id(1);
        resource.description.kind = static_cast<Analysis::APIDumpAnalysis::ResourceKind>(99);

        model.resource_vec.push_back(resource);

        EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resources(Analysis::APIDumpAnalysis::PassesSerializer::serialize_resources(model),
                                                                                       &loaded) );
    }

    {
        Analysis::APIDumpAnalysis::AnalysisResult analysis;
        Analysis::APIDumpAnalysis::AnalysisResult loaded;
        std::string               error;

        analysis.summary.n_calls_by_group_map[static_cast<Capture::APIDumpLoader::APIGroup>(99)] = 1;

        EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis),
                                                                      &loaded,
                                                                      &error) );
    }
}

TEST(PassesFile, APassOfADrawThatThereIsNotIsRefused)
{
    Analysis::APIDumpAnalysis::Model         model;
    Analysis::APIDumpAnalysis::Model         loaded;
    Analysis::APIDumpAnalysis::PassStructure pass_structure;

    pass_structure.draw_structure_key_vec.push_back(0x1234);
    pass_structure.key = 1;

    model.pass_structure_interner.intern(pass_structure,
                                         pass_structure.key);

    EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_passes(Analysis::APIDumpAnalysis::PassesSerializer::serialize_passes(model),
                                                                                &loaded) );

    loaded.draw_structure_map[0x1234] = Analysis::APIDumpAnalysis::DrawStructure();

    EXPECT_TRUE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_passes(Analysis::APIDumpAnalysis::PassesSerializer::serialize_passes(model),
                                                                               &loaded) );
}

TEST(PassesFile, AResourceWhichIsAPartOfOneThatCameLaterIsRefused)
{
    Analysis::APIDumpAnalysis::Model    model;
    Analysis::APIDumpAnalysis::Model    loaded;
    Analysis::APIDumpAnalysis::Resource first;
    Analysis::APIDumpAnalysis::Resource second;

    first.id                         = Analysis::APIDumpAnalysis::make_resource_id(1);
    second.id                        = Analysis::APIDumpAnalysis::make_resource_id(2);
    first.parent_view.resource_id    = Analysis::APIDumpAnalysis::make_resource_id(2);

    model.resource_vec.push_back(first);
    model.resource_vec.push_back(second);

    EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resources(Analysis::APIDumpAnalysis::PassesSerializer::serialize_resources(model),
                                                                                   &loaded) );

    first.parent_view.resource_id = Analysis::APIDumpAnalysis::ResourceId::NONE;
    second.parent_view.resource_id = Analysis::APIDumpAnalysis::make_resource_id(1);

    model.resource_vec.clear();
    model.resource_vec.push_back(first);
    model.resource_vec.push_back(second);

    Analysis::APIDumpAnalysis::Model reloaded;

    EXPECT_TRUE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_resources(Analysis::APIDumpAnalysis::PassesSerializer::serialize_resources(model),
                                                                                  &reloaded) );
}

TEST(PassesFile, ARepresentativeFrameThatIsNotOneOfTheFramesIsRefused)
{
    const auto                       analysis = Tests::PassesFileHelpers::analyze_slice();
    Analysis::APIDumpAnalysis::Model loaded;

    loaded.frame_shape_interner     = analysis.model.frame_shape_interner;
    loaded.pass_structure_interner  = analysis.model.pass_structure_interner;
    loaded.resource_vec             = analysis.model.resource_vec;

    /* No frames at all: every frame of a representative is one too many. */
    EXPECT_FALSE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_representatives(Analysis::APIDumpAnalysis::PassesSerializer::serialize_representatives(analysis.model),
                                                                                         &loaded) );

    Analysis::APIDumpAnalysis::Model loaded_with_frames = loaded;

    loaded_with_frames.frame_record_vec = analysis.model.frame_record_vec;

    Analysis::APIDumpAnalysis::Model fresh;

    fresh.frame_shape_interner    = analysis.model.frame_shape_interner;
    fresh.pass_structure_interner = analysis.model.pass_structure_interner;
    fresh.resource_vec            = analysis.model.resource_vec;
    fresh.frame_record_vec        = analysis.model.frame_record_vec;

    EXPECT_TRUE(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_representatives(Analysis::APIDumpAnalysis::PassesSerializer::serialize_representatives(analysis.model),
                                                                                        &fresh) );
}

TEST(PassesFile, AResourceOfARepresentativeThatThereIsNotIsRefused)
{
    /* One shape, one frame, and a representative of it with a copy to a resource. */
    for (const uint32_t destination_id : {0u, 99u})
    {
        Analysis::APIDumpAnalysis::FrameDetail detail;
        Analysis::APIDumpAnalysis::Model       model;
        Analysis::APIDumpAnalysis::Model       loaded;
        Analysis::APIDumpAnalysis::TransferRecord transfer;

        transfer.destination.resource_id = Analysis::APIDumpAnalysis::make_resource_id(destination_id);

        detail.transfer_record_vec.push_back(transfer);

        model.frame_record_vec.resize(1);
        model.frame_shape_interner.intern(Analysis::APIDumpAnalysis::FrameShape(),
                                          1);
        model.representative_store.add(0,
                                       0,
                                       1,
                                       detail);

        loaded.frame_record_vec     = model.frame_record_vec;
        loaded.frame_shape_interner = model.frame_shape_interner;

        EXPECT_EQ(Analysis::APIDumpAnalysis::PassesSerializer::deserialize_representatives(Analysis::APIDumpAnalysis::PassesSerializer::serialize_representatives(model),
                                                                                          &loaded),
                  destination_id == 0) << destination_id;
    }
}

TEST(PassesFile, TheDepthOfThePassesIsReadBack)
{
    std::vector<uint8_t>      byte_vec;
    Analysis::APIDumpAnalysis::AnalysisResult analysis = Tests::PassesFileHelpers::analyze_slice();
    Analysis::APIDumpAnalysis::AnalysisResult read;
    std::string               error;

    ASSERT_FALSE(analysis.model.depth_histogram_map.empty() );
    byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis);

    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(byte_vec,
                                                                 &read,
                                                                 &error) ) << error;

    ASSERT_EQ(read.model.depth_histogram_map.size(),
              analysis.model.depth_histogram_map.size() );

    for (const auto& histogram_pair : analysis.model.depth_histogram_map)
    {
        const auto read_iterator = read.model.depth_histogram_map.find(histogram_pair.first);

        ASSERT_NE(read_iterator,
                  read.model.depth_histogram_map.end() );

        for (uint32_t n_bin = 0;
                      n_bin < Analysis::APIDumpAnalysis::DepthHistogram::get_n_bins();
                    ++n_bin)
        {
            EXPECT_EQ(read_iterator->second.get_weight(n_bin),
                      histogram_pair.second.get_weight(n_bin) );
        }
    }
}

TEST(PassesFile, WhatTheApplicationDoesThatDependsOnPixelsIsReadBack)
{
    std::vector<uint8_t>      byte_vec;
    Analysis::APIDumpAnalysis::AnalysisResult analysis = Tests::PassesFileHelpers::analyze_slice();
    Analysis::APIDumpAnalysis::AnalysisResult read;
    std::string               error;

    analysis.summary.n_resolution_hazards_map[Analysis::APIDumpAnalysis::ResolutionHazard::COPIES_WITH_RECTANGLES] = 12;
    analysis.summary.n_resolution_hazards_map[Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES]           = 3;

    byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis);

    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(byte_vec,
                                                                 &read,
                                                                 &error) ) << error;

    EXPECT_EQ(read.summary.n_resolution_hazards_map,
              analysis.summary.n_resolution_hazards_map);
}

TEST(PassesFile, TheDepthOfTheSurfacesIsReadBack)
{
    Analysis::APIDumpAnalysis::AnalysisResult analysis = Tests::PassesFileHelpers::analyze_slice();
    Analysis::APIDumpAnalysis::AnalysisResult read;
    std::string                               error;
    Analysis::APIDumpAnalysis::DepthHistogram histogram;

    histogram.add(2.0f,
                  7);
    histogram.add(40.0f,
                  9);

    analysis.model.surface_depth_histogram_map[Analysis::APIDumpAnalysis::DepthHistogramKey(1,
                                                                                            2)] = histogram;

    const std::vector<uint8_t> byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis);

    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(byte_vec,
                                                                 &read,
                                                                 &error) ) << error;

    const auto read_iterator = read.model.surface_depth_histogram_map.find(Analysis::APIDumpAnalysis::DepthHistogramKey(1,
                                                                                                                        2) );

    ASSERT_NE(read_iterator,
              read.model.surface_depth_histogram_map.end() );

    for (uint32_t n_bin = 0;
                  n_bin < Analysis::APIDumpAnalysis::DepthHistogram::get_n_bins();
                ++n_bin)
    {
        EXPECT_EQ(read_iterator->second.get_weight(n_bin),
                  histogram.get_weight(n_bin) );
    }
}

TEST(PassesFile, TheSignaturesOfTheDrawsThatCoverTheViewAreReadBack)
{
    Analysis::APIDumpAnalysis::AnalysisResult analysis = Tests::PassesFileHelpers::analyze_slice();
    Analysis::APIDumpAnalysis::AnalysisResult read;
    std::string                               error;

    analysis.model.covering_draw_key_map[0] = {0x1234u, 0xfedcba9876543210ull};

    const std::vector<uint8_t> byte_vec = Analysis::APIDumpAnalysis::PassesFile::write_bytes(analysis);

    ASSERT_TRUE(Analysis::APIDumpAnalysis::PassesFile::read_bytes(byte_vec,
                                                                 &read,
                                                                 &error) ) << error;

    EXPECT_EQ(read.model.covering_draw_key_map,
              analysis.model.covering_draw_key_map);
}
