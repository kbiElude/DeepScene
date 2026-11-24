/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The pictures of the applications as textures of the window: a file is decoded with the Windows Imaging Component (it reads JPEG, PNG and
 * more, and is part of Windows) and uploaded once. A picture that cannot be read has no texture, and is not tried again.
 */
#ifndef VRPLAYER_THUMBNAIL_TEXTURES_H
#define VRPLAYER_THUMBNAIL_TEXTURES_H

#include <stddef.h>
#include <string>
#include <vector>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace VRPlayer
{
    class ThumbnailTextures
    {
    public:
        /* One place for each application of the catalog. */
        explicit ThumbnailTextures(const size_t& in_n_apps);

        ~ThumbnailTextures();

        ThumbnailTextures(const ThumbnailTextures&)            = delete;
        ThumbnailTextures& operator=(const ThumbnailTextures&) = delete;

        /* Width over height of the picture; 0 if there is none. */
        float get_aspect(const size_t& in_n_app) const;

        /* The texture to draw, or nullptr. */
        ID3D11ShaderResourceView* get_view_ptr(const size_t& in_n_app) const;

        /* True once the file was read, whether or not it could be decoded. */
        bool is_tried(const size_t& in_n_app) const;

        /* Reads the file and makes the texture. Does nothing if the file has been read before. Returns true if the application has a texture. */
        bool load(const size_t&      in_n_app,
                  ID3D11Device*      in_device_ptr,
                  const std::string& in_file_path);

    private:
        struct Entry
        {
            float                     aspect   = 0.0f;
            bool                      is_tried = false;
            ID3D11ShaderResourceView* view_ptr = nullptr;
        };

        std::vector<Entry> m_entry_vec;
    };
}

#endif /* VRPLAYER_THUMBNAIL_TEXTURES_H */
