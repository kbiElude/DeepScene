/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the loader of the catalog of the VR player: what an application takes from the recipe and from the catalog, how an
 * application whose files are missing is listed, where an application is downloaded from, and what is refused, with the place of
 * the error.
 */
#include <gtest/gtest.h>
#include "VR/AppCatalog/catalog_loader.h"
#include "data_fixtures.h"
#include "fixtures.h"
#include "temp_directory.h"
#include "VR/AppInstaller/file_system.h"
#include <algorithm>
#include <fstream>
#include <Windows.h>

static std::string get_directory(const std::string& in_path)
{
    std::string result = in_path.substr(0,
                                        in_path.find_last_of("\\/") );

    std::replace(result.begin(),
                 result.end  (),
                 '/',
                 '\\');

    return result;
}

static std::string get_json_path(const std::string& in_path)
{
    std::string result;

    for (const char character : in_path)
    {
        result += (character == '\\') ? std::string("\\\\")
                                      : std::string(1,
                                                    character);
    }

    return result;
}

class CatalogLoaderTest : public ::testing::Test
{
protected:
    /* The text of a catalog of one application which is there, with the fields the test adds to it. */
    std::string get_text(const std::string& in_extra_fields = std::string()) const
    {
        return "{\"catalog_version\": 0, \"apps\": [{\"id\": \"fr08\", \"recipe\": \"fr08_example.json\", \"executable\": \"" +
               get_json_path(Tests::Fixtures::get_test_executable_path() ) + "\"" + in_extra_fields + "}]}";
    }

    bool parse(const std::string& in_text)
    {
        return VR::AppCatalog::CatalogLoader::parse(in_text,
                                                    get_directory(Tests::DataFixtures::get_recipe_example_filename() ),
                                                   &m_catalog,
                                                   &m_error);
    }

    VR::AppCatalog::Catalog m_catalog;
    std::string             m_error;
};

TEST_F(CatalogLoaderTest, AnApplicationTakesItsNameAndDescriptionFromTheRecipe)
{
    ASSERT_TRUE(parse(get_text() ) ) << m_error;
    ASSERT_EQ  (m_catalog.app_vec.size(),
                1u);

    const auto& app = m_catalog.app_vec.at(0);

    EXPECT_EQ(app.id,
              "fr08");
    EXPECT_EQ(app.name,
              "fr08");
    EXPECT_FALSE(app.description.empty() );
    EXPECT_FLOAT_EQ(app.overlay_parallax,
                    0.0f);
    EXPECT_TRUE (app.unavailable_reason.empty() ) << app.unavailable_reason;
    EXPECT_TRUE (app.arguments.empty() );
    EXPECT_EQ(app.executable_path,
              Tests::Fixtures::get_test_executable_path() );
    EXPECT_EQ(app.working_directory,
              get_directory(Tests::Fixtures::get_test_executable_path() ) );
}

TEST_F(CatalogLoaderTest, TheCatalogCanSayTheTitleTheArgumentsAndTheWorkingDirectory)
{
    ASSERT_TRUE(parse(get_text(", \"title\": \"The corridor\", \"arguments\": \"-windowed\", \"working_directory\": \"work/dir\"") ) ) << m_error;

    const auto& app = m_catalog.app_vec.at(0);

    EXPECT_EQ(app.name,
              "The corridor");
    EXPECT_EQ(app.arguments,
              "-windowed");
    EXPECT_EQ(app.working_directory,
              get_directory(Tests::DataFixtures::get_recipe_example_filename() ) + "\\work\\dir");
    EXPECT_TRUE(app.unavailable_reason.empty() ) << app.unavailable_reason;
}

TEST_F(CatalogLoaderTest, RelativePathsAreRelativeToTheCatalogAndTheOtherSeparatorIsAccepted)
{
    ASSERT_TRUE(parse("{\"catalog_version\": 0, \"apps\": [{\"id\": \"a\", \"recipe\": \"x/y.json\", \"executable\": \"z/w.exe\"}]}") ) << m_error;

    const std::string base = get_directory(Tests::DataFixtures::get_recipe_example_filename() );

    EXPECT_EQ(m_catalog.app_vec.at(0).recipe_path,
              base + "\\x\\y.json");
    EXPECT_EQ(m_catalog.app_vec.at(0).executable_path,
              base + "\\z\\w.exe");
}

TEST_F(CatalogLoaderTest, DotsInAPathAreResolvedSoThatTheDisplayedPathIsWhereTheFileIs)
{
    ASSERT_TRUE(VR::AppCatalog::CatalogLoader::parse("{\"catalog_version\": 0, \"apps\": [{\"id\": \"a\", \"recipe\": \"../r/./x.json\", \"executable\": \"C:/p/../q/e.exe\"}]}",
                                                     "D:\\base\\dir",
                                                    &m_catalog,
                                                    &m_error) ) << m_error;

    EXPECT_EQ(m_catalog.app_vec.at(0).recipe_path,
              "D:\\base\\r\\x.json");
    EXPECT_EQ(m_catalog.app_vec.at(0).executable_path,
              "C:\\q\\e.exe");

    /* A ".." above the root of a drive has nothing to remove. */
    ASSERT_TRUE(VR::AppCatalog::CatalogLoader::parse("{\"catalog_version\": 0, \"apps\": [{\"id\": \"a\", \"recipe\": \"r.json\", \"executable\": \"C:/../e.exe\"}]}",
                                                     "D:\\base",
                                                    &m_catalog,
                                                    &m_error) ) << m_error;

    EXPECT_EQ(m_catalog.app_vec.at(0).executable_path,
              "C:\\..\\e.exe");
}

TEST_F(CatalogLoaderTest, AnApplicationWithoutItsFilesIsListedWithTheReason)
{
    ASSERT_TRUE(parse("{\"catalog_version\": 0, \"apps\": [{\"id\": \"no-recipe\", \"recipe\": \"nothing.json\", \"executable\": \"" +
                      get_json_path(Tests::Fixtures::get_test_executable_path() ) + "\"}," +
                      "{\"id\": \"no-exe\", \"recipe\": \"fr08_example.json\", \"executable\": \"nothing.exe\"}," +
                      "{\"id\": \"bad-recipe\", \"recipe\": \"" + get_json_path(Tests::Fixtures::get_test_executable_path() ) + "\", \"executable\": \"" +
                      get_json_path(Tests::Fixtures::get_test_executable_path() ) + "\"}]}") ) << m_error;
    ASSERT_EQ  (m_catalog.app_vec.size(),
                3u);

    EXPECT_NE(m_catalog.app_vec.at(0).unavailable_reason.find("The recipe cannot be found"),
              std::string::npos);
    EXPECT_EQ(m_catalog.app_vec.at(0).name,
              "no-recipe");

    EXPECT_NE(m_catalog.app_vec.at(1).unavailable_reason.find("The application cannot be found"),
              std::string::npos);
    EXPECT_EQ(m_catalog.app_vec.at(1).name,
              "fr08");

    EXPECT_NE(m_catalog.app_vec.at(2).unavailable_reason.find("The recipe is not valid"),
              std::string::npos);
}

TEST_F(CatalogLoaderTest, AnEmptyCatalogAndAByteOrderMarkAreAccepted)
{
    EXPECT_TRUE(parse("{\"catalog_version\": 0, \"apps\": []}") ) << m_error;
    EXPECT_TRUE(m_catalog.app_vec.empty() );

    EXPECT_TRUE(parse("\xEF\xBB\xBF{\"catalog_version\": 0, \"apps\": []}") ) << m_error;
}

TEST_F(CatalogLoaderTest, WhatIsNotRightIsRefusedWithItsPlace)
{
    const struct
    {
        std::string text;
        std::string error_part;
    } case_array[] =
    {
        {"{",                                                                               "Not valid JSON"},
        {"[]",                                                                              "must be a JSON object"},
        {"{\"catalog_version\": 0, \"apps\": [], \"x\": 1}",                                "unknown field \"x\""},
        {"{\"apps\": []}",                                                                  "catalog_version"},
        {"{\"catalog_version\": 1, \"apps\": []}",                                          "catalog_version"},
        {"{\"catalog_version\": 0}",                                                        "apps: expected a list"},
        {"{\"catalog_version\": 0, \"apps\": [5]}",                                         "apps[0]: expected an object"},
        {"{\"catalog_version\": 0, \"apps\": [{\"recipe\": \"r\", \"executable\": \"e\"}]}", "apps[0].id: is missing"},
        {"{\"catalog_version\": 0, \"apps\": [{\"id\": \"\", \"recipe\": \"r\", \"executable\": \"e\"}]}", "apps[0].id: expected a text which is not empty"},
        {"{\"catalog_version\": 0, \"apps\": [{\"id\": \"a b\", \"recipe\": \"r\", \"executable\": \"e\"}]}", "apps[0].id: only letters"},
        {"{\"catalog_version\": 0, \"apps\": [{\"id\": \"a\", \"executable\": \"e\"}]}",    "apps[0].recipe: is missing"},
        {"{\"catalog_version\": 0, \"apps\": [{\"id\": \"a\", \"recipe\": \"r\"}]}",        "apps[0].executable: is missing"},
        {"{\"catalog_version\": 0, \"apps\": [{\"id\": \"a\", \"recipe\": \"r\", \"executable\": 3}]}", "apps[0].executable: expected"},
        {"{\"catalog_version\": 0, \"apps\": [{\"id\": \"a\", \"recipe\": \"r\", \"executable\": \"e\", \"arguments\": 1}]}", "apps[0].arguments: expected a text"},
        {"{\"catalog_version\": 0, \"apps\": [{\"id\": \"a\", \"recipe\": \"r\", \"executable\": \"e\", \"api\": \"d3d8\"}]}", "apps[0]: unknown field \"api\""},
        {"{\"catalog_version\": 0, \"apps\": [{\"id\": \"a\", \"recipe\": \"r\", \"executable\": \"e\"}, {\"id\": \"a\", \"recipe\": \"r\", \"executable\": \"e\"}]}", "apps[1].id: \"a\" is used"}
    };

    for (const auto& test_case : case_array)
    {
        EXPECT_FALSE(parse(test_case.text) ) << test_case.text;
        EXPECT_NE   (m_error.find(test_case.error_part),
                     std::string::npos) << test_case.text << " -> " << m_error;
    }
}

TEST_F(CatalogLoaderTest, ACatalogWhichIsNotThereIsNotRead)
{
    EXPECT_FALSE(VR::AppCatalog::CatalogLoader::load("Z:/no/such/catalog.json",
                                                    &m_catalog,
                                                    &m_error) );
    EXPECT_NE   (m_error.find("Cannot open"),
                 std::string::npos);
}

TEST_F(CatalogLoaderTest, TheCatalogWhichComesWithTheRepositoryListsFr08)
{
    /* Its executable is downloaded when it is first launched, so the catalog is usable without it. */
    ASSERT_TRUE(VR::AppCatalog::CatalogLoader::load(std::string(DEMOVRPLAYER_APPS_DIR) + "/apps.json",
                                                   &m_catalog,
                                                   &m_error) ) << m_error;
    ASSERT_EQ  (m_catalog.app_vec.size(),
                1u);

    EXPECT_EQ(m_catalog.app_vec.at(0).id,
              "fr08");
    EXPECT_EQ(m_catalog.app_vec.at(0).name,
              "fr08");
    EXPECT_EQ(m_catalog.app_vec.at(0).unavailable_reason.find("recipe"),
              std::string::npos) << m_catalog.app_vec.at(0).unavailable_reason;

    /* Where it comes from: mirrors which are https, and what the archive has to be. */
    EXPECT_GE(m_catalog.app_vec.at(0).install_spec.url_vec.size(),
              2u);
    EXPECT_EQ(m_catalog.app_vec.at(0).install_spec.sha256_hex.size(),
              64u);
    EXPECT_EQ(m_catalog.app_vec.at(0).install_spec.n_archive_bytes,
              66454u);
    EXPECT_NE(m_catalog.app_vec.at(0).install_spec.install_directory.find("\\apps\\fr08"),
              std::string::npos);

    /* fr08 wants its overlays (the text and circles of its scenes) in front of the screen: the recipe has an amount. */
    EXPECT_GT(m_catalog.app_vec.at(0).overlay_parallax,
              0.0f);
}

TEST_F(CatalogLoaderTest, AnApplicationTakesTheAmountOfItsOverlayFromTheRecipe)
{
    ASSERT_TRUE(VR::AppCatalog::CatalogLoader::parse("{\"catalog_version\": 0, \"apps\": [{\"id\": \"fr08\", \"recipe\": \"fr08_overlay.json\", \"executable\": \"" +
                                                     get_json_path(Tests::Fixtures::get_test_executable_path() ) + "\"}]}",
                                                     get_directory(Tests::DataFixtures::get_recipe_example_filename() ),
                                                    &m_catalog,
                                                    &m_error) ) << m_error;
    ASSERT_EQ  (m_catalog.app_vec.size(),
                1u);

    EXPECT_TRUE (m_catalog.app_vec.at(0).unavailable_reason.empty() ) << m_catalog.app_vec.at(0).unavailable_reason;
    EXPECT_FLOAT_EQ(m_catalog.app_vec.at(0).overlay_parallax,
                    0.04f);
}

/* The text of a catalog of one application which is downloaded: the executable, and the download. */
static std::string get_download_text(const std::string& in_executable,
                                     const std::string& in_download)
{
    return "{\"catalog_version\": 0, \"apps\": [{\"id\": \"fr08\", \"recipe\": \"" +
           get_json_path(Tests::DataFixtures::get_recipe_example_filename() ) + "\", \"executable\": \"" + in_executable +
           "\", \"download\": " + in_download + "}]}";
}

static const char* const GOOD_DOWNLOAD = "{\"urls\": [\"https://a.example/x.zip\", \"https://b.example/x.zip\"], "
                                         "\"sha256\": \"ABCDEF0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789\", "
                                         "\"size\": 1000, \"unpacked_size\": 5000, \"page\": \"https://p.example/x\"}";

TEST_F(CatalogLoaderTest, AnApplicationWhichIsNotThereIsDownloadedWhenTheCatalogSaysWhereFrom)
{
    const Tests::TempDirectory directory;

    ASSERT_TRUE(VR::AppCatalog::CatalogLoader::parse(get_download_text("fr08/fr08v101.exe",
                                                                       GOOD_DOWNLOAD),
                                                     directory.get_path(),
                                                    &m_catalog,
                                                    &m_error) ) << m_error;
    ASSERT_EQ  (m_catalog.app_vec.size(),
                1u);

    const VR::AppCatalog::AppEntry& app = m_catalog.app_vec.at(0);

    EXPECT_TRUE(app.is_download_needed);
    EXPECT_TRUE(app.unavailable_reason.empty() ) << app.unavailable_reason;

    EXPECT_EQ(app.install_spec.install_directory,
              directory.get_path() + "\\fr08");
    EXPECT_EQ(app.install_spec.url_vec.size(),
              2u);
    EXPECT_EQ(app.install_spec.sha256_hex,
              "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
    EXPECT_EQ(app.install_spec.n_archive_bytes,
              1000u);
    EXPECT_EQ(app.install_spec.n_unpacked_bytes_max,
              5000u);
    EXPECT_EQ(app.install_spec.page_url,
              "https://p.example/x");
    EXPECT_EQ(app.executable_path,
              directory.get_path() + "\\fr08\\fr08v101.exe");
}

TEST_F(CatalogLoaderTest, AnApplicationWhichIsThereIsNotDownloaded)
{
    const Tests::TempDirectory directory;
    std::string                error;

    ASSERT_TRUE(VR::AppInstaller::FileSystem::create_directories(directory.get_path() + "\\fr08",
                                                                &error) ) << error;

    {
        std::ofstream file(directory.get_path() + "\\fr08\\fr08v101.exe");

        file << "program";
    }

    ASSERT_TRUE(VR::AppCatalog::CatalogLoader::parse(get_download_text("fr08/fr08v101.exe",
                                                                       GOOD_DOWNLOAD),
                                                     directory.get_path(),
                                                    &m_catalog,
                                                    &m_error) ) << m_error;
    ASSERT_EQ  (m_catalog.app_vec.size(),
                1u);

    EXPECT_FALSE(m_catalog.app_vec.at(0).is_download_needed);
    EXPECT_TRUE (m_catalog.app_vec.at(0).unavailable_reason.empty() );
}

TEST_F(CatalogLoaderTest, WhatIsNotRightInADownloadIsRefusedWithItsPlace)
{
    struct Case
    {
        std::string executable;
        std::string download;
        std::string expected;
    };

    const std::string good = GOOD_DOWNLOAD;
    const Case        case_array[] =
    {
        {"fr08/a.exe",   "{\"urls\": [\"http://a.example/x.zip\"], " + good.substr(good.find("\"sha256\"") ),             "urls[0]: expected an address of https"},
        {"fr08/a.exe",   "{\"urls\": [], " + good.substr(good.find("\"sha256\"") ),                                       "urls: expected a list"},
        {"fr08/a.exe",   "{\"urls\": [\"https://a.example/x.zip\"], \"sha256\": \"abc\", \"size\": 1, \"unpacked_size\": 1}",     "sha256: expected 64 digits"},
        {"fr08/a.exe",   "{\"urls\": [\"https://a.example/x.zip\"], \"sha256\": \"" + std::string(64, 'a') + "\", \"unpacked_size\": 1}", "size: expected a whole number"},
        {"fr08/a.exe",   "{\"urls\": [\"https://a.example/x.zip\"], \"sha256\": \"" + std::string(64, 'a') + "\", \"size\": 0, \"unpacked_size\": 1}", "size: expected a whole number"},
        {"fr08/a.exe",   "{\"urls\": [\"https://a.example/x.zip\"], \"sha256\": \"" + std::string(64, 'a') + "\", \"size\": 1}",   "unpacked_size: expected a whole number"},
        {"fr08/a.exe",   "{\"urls\": [\"https://a.example/x.zip\"], \"sha256\": \"" + std::string(64, 'a') + "\", \"size\": 1, \"unpacked_size\": 1, \"mirror\": 1}", "unknown field \"mirror\""},
        {"other/a.exe",  good,                                                                                            ".executable: has to be in"},
        {"fr08/a.exe",   "[]",                                                                                            "expected an object"}
    };

    for (const Case& test_case : case_array)
    {
        EXPECT_FALSE(VR::AppCatalog::CatalogLoader::parse(get_download_text(test_case.executable,
                                                                            test_case.download),
                                                          "C:\\apps",
                                                         &m_catalog,
                                                         &m_error) ) << test_case.expected;
        EXPECT_NE   (m_error.find(test_case.expected),
                     std::string::npos) << m_error << " (expected: " << test_case.expected << ")";
    }
}
