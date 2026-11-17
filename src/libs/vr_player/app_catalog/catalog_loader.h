/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the VR player catalog (JSON). Strict: unknown fields are refused and errors say where they are.
 *
 *   {
 *     "catalog_version": 0,
 *     "apps": [
 *       {
 *         "id": "example",                           letters, digits, '-' and '_'; unique
 *         "recipe": "recipes/example.json",          recipe that converts the application to stereo
 *         "executable": "example/example.exe",       program to start
 *         "arguments": "-windowed",                  (optional) command line arguments
 *         "working_directory": "example",            (optional) default: the executable's directory
 *         "title": "Example"                         (optional) default: the recipe target name
 *         "launch_button": "Launch",                 (optional) button in the first dialog, pressed by the launcher
 *         "untick_checkbox": "Fullscreen",           (optional) checkbox in that dialog, unticked before the button is pressed
 *         "unavailable_reason": "...",               (optional) why the application cannot be started
 *         "download": {                              (optional) source if the executable is missing:
 *           "urls": ["https://..."],                 https only; tried in order until one works
 *           "sha256": "...",                         hex SHA-256 of the zip archive; a different hash is refused
 *           "size": 66454,                           archive size in bytes
 *           "unpacked_size": 69184,                  maximum total size of the unpacked files
 *           "page": "https://..."                    (optional) page for a manual download
 *         }
 *       }
 *     ]
 *   }
 *
 * Relative paths are relative to the catalog directory. An application with a missing executable or missing/invalid recipe is still
 * listed with AppEntry::unavailable_reason. A download is installed into a directory named after the id next to the catalog; the
 * executable must be inside it.
 */
#ifndef APPCATALOG_CATALOG_LOADER_H
#define APPCATALOG_CATALOG_LOADER_H

#include "utils/json/json_reader.h"
#include "vr_player/app_catalog/types.h"
#include <string>
#include <vector>

namespace VRPlayer
{
    namespace AppCatalog
    {
        class CatalogLoader
        {
        public:
            /* Returns false and sets the error if the catalog cannot be read. */
            static bool load(const std::string& in_filename,
                             Catalog*           out_catalog_ptr,
                             std::string*       out_error_ptr);

            /* Relative paths in the catalog are resolved against in_base_directory. */
            static bool parse(const std::string& in_text,
                              const std::string& in_base_directory,
                              Catalog*           out_catalog_ptr,
                              std::string*       out_error_ptr);

        private:
            static void        check_availability(AppEntry*                            inout_app_ptr,
                                                  const std::string&                   in_title);
            static std::string get_directory     (const std::string&                   in_path);
            static bool        is_absolute_path  (const std::string&                   in_path);
            static bool        parse_app         (const Utils::Json::JsonValue&        in_app,
                                                  const std::string&                   in_path,
                                                  const std::string&                   in_base_directory,
                                                  AppEntry*                            out_app_ptr,
                                                  std::string*                         out_title_ptr,
                                                  std::string*                         out_error_ptr);
            static bool        parse_download    (const Utils::Json::JsonValue&        in_download,
                                                  const std::string&                   in_path,
                                                  const std::string&                   in_install_directory,
                                                  VRPlayer::AppInstaller::InstallSpec* out_spec_ptr,
                                                  std::string*                         out_error_ptr);
            static bool        parse_number      (const Utils::Json::JsonValue&        in_object,
                                                  const std::string&                   in_key,
                                                  const std::string&                   in_path,
                                                  uint64_t*                            out_number_ptr,
                                                  std::string*                         out_error_ptr);
            static bool        parse_string      (const Utils::Json::JsonValue&        in_object,
                                                  const std::string&                   in_key,
                                                  const bool&                          in_is_required,
                                                  const std::string&                   in_path,
                                                  std::string*                         out_string_ptr,
                                                  std::string*                         out_error_ptr);
            static std::string resolve_path      (const std::string&                   in_base_directory,
                                                  const std::string&                   in_path);
        };
    }
}

#endif /* APPCATALOG_CATALOG_LOADER_H */
