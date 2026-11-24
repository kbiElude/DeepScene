/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the pictures of the cards as textures: a picture is decoded and uploaded once, with the shape that it has, and a file that is not
 * a picture has no texture and is not read again. The device is the software one, so no graphics card is needed.
 */
#include <gtest/gtest.h>
#include "temp_directory.h"
#include "../vr_player/thumbnail_textures.h"
#include <Windows.h>
#include <d3d11.h>
#include <fstream>
#include <vector>

/* The bytes of a BMP file of 24 bits, all of one color. The smallest picture that Windows decodes without anything else. */
static std::vector<char> get_bmp_bytes(const int& in_width,
                                       const int& in_height)
{
    const int          row_size   = ((in_width * 3 + 3) / 4) * 4;
    const int          pixel_size = row_size * in_height;
    std::vector<char>  result(54 + static_cast<size_t>(pixel_size),
                              static_cast<char>(0x80) );
    const int          file_size  = 54 + pixel_size;
    const int          header     = 40;
    const int          offset     = 54;
    const short        n_planes   = 1;
    const short        n_bits     = 24;

    result[0] = 'B';
    result[1] = 'M';

    ::memcpy(&result[2],  &file_size,  sizeof(file_size) );
    ::memset(&result[6],  0,           4);
    ::memcpy(&result[10], &offset,     sizeof(offset) );
    ::memcpy(&result[14], &header,     sizeof(header) );
    ::memcpy(&result[18], &in_width,   sizeof(in_width) );
    ::memcpy(&result[22], &in_height,  sizeof(in_height) );
    ::memcpy(&result[26], &n_planes,   sizeof(n_planes) );
    ::memcpy(&result[28], &n_bits,     sizeof(n_bits) );
    ::memset(&result[30], 0,           24);

    return result;
}

class ThumbnailTexturesTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const HRESULT result = ::D3D11CreateDevice(nullptr,
                                                   D3D_DRIVER_TYPE_WARP,
                                                   nullptr,
                                                   0,
                                                   nullptr,
                                                   0,
                                                   D3D11_SDK_VERSION,
                                                   &m_device_ptr,
                                                   nullptr,
                                                   nullptr);

        if (FAILED(result) )
        {
            GTEST_SKIP() << "No software device.";
        }
    }

    void TearDown() override
    {
        if (m_device_ptr != nullptr)
        {
            m_device_ptr->Release();
        }
    }

    void write_file(const std::string&       in_path,
                    const std::vector<char>& in_bytes)
    {
        std::ofstream output_file(in_path,
                                  std::ios::binary);

        output_file.write(in_bytes.data(),
                          static_cast<std::streamsize>(in_bytes.size() ) );
    }

    Tests::TempDirectory m_directory;
    ID3D11Device*        m_device_ptr = nullptr;
};

TEST_F(ThumbnailTexturesTest, APictureBecomesATextureWithTheShapeItHas)
{
    const std::string          path = m_directory.get_path() + "\\wide.bmp";
    VRPlayer::ThumbnailTextures textures(2);

    write_file(path,
               get_bmp_bytes(8,
                             4) );

    EXPECT_FALSE(textures.is_tried(0) );
    EXPECT_EQ   (textures.get_view_ptr(0),
                 nullptr);

    EXPECT_TRUE(textures.load(0,
                              m_device_ptr,
                              path) );

    EXPECT_TRUE(textures.is_tried(0) );
    EXPECT_NE  (textures.get_view_ptr(0),
                nullptr);
    EXPECT_FLOAT_EQ(textures.get_aspect(0),
                    2.0f);

    /* The other place is not touched. */
    EXPECT_FALSE(textures.is_tried(1) );
}

TEST_F(ThumbnailTexturesTest, AFileThatIsNotAPictureHasNoTextureAndIsNotReadAgain)
{
    const std::string           path = m_directory.get_path() + "\\not_a_picture.jpg";
    VRPlayer::ThumbnailTextures textures(1);

    write_file(path,
               std::vector<char>{'n', 'o', 'p', 'e'});

    EXPECT_FALSE(textures.load(0,
                               m_device_ptr,
                               path) );
    EXPECT_TRUE (textures.is_tried(0) );

    /* The file is made right afterwards, and still not read: the place has been tried. */
    write_file(path,
               get_bmp_bytes(4,
                             4) );

    EXPECT_FALSE(textures.load(0,
                               m_device_ptr,
                               path) );
    EXPECT_EQ   (textures.get_view_ptr(0),
                 nullptr);
}

TEST_F(ThumbnailTexturesTest, AFileThatIsNotThereHasNoTexture)
{
    VRPlayer::ThumbnailTextures textures(1);

    EXPECT_FALSE(textures.load(0,
                               m_device_ptr,
                               m_directory.get_path() + "\\absent.jpg") );
    EXPECT_EQ   (textures.get_view_ptr(0),
                 nullptr);
}
