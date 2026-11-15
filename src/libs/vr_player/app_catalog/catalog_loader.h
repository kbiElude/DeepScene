/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the catalog of the VR player, a JSON file. Reading is strict: unknown fields are refused, and errors say where they are.
 *
 *   {
 *     "catalog_version": 0,
 *     "apps": [
 *       {
 *         "id": "fr08",                              letters, digits, '-' and '_'; each id once
 *         "recipe": "recipes/fr08.json",             the recipe that converts the application to stereo
 *         "executable": "fr08/fr08v101.exe",         the program to start
 *         "arguments": "-windowed",                  (optional) command line arguments of the application
 *         "working_directory": "fr08",               (optional) default: the directory of the executable
 *         "title": "Fr-08: .the .product"            (optional) default: the name of the recipe target
 *         "launch_button": "Launch",                 (optional) text of a button in the application's first dialog; the launcher presses it
 *         "unavailable_reason": "...",               (optional) why the player cannot start the application (see below)
 *         "download": {                              (optional) where to get the application if its executable is missing:
 *           "urls": ["https://..."],                 https only; tried one after the other until one works
 *           "sha256": "...",                         hex SHA-256 of the archive (a zip file); an archive with another hash is not used
 *           "size": 66454,                           size of the archive in bytes
 *           "unpacked_size": 69184,                  the most that the unpacked files may take together
 *           "page": "https://..."                    (optional) where the archive can be downloaded by hand
 *         }
 *       }
 *     ]
 *   }
 *
 * Relative paths are relative to the directory of the catalog. An application whose recipe or executable is missing, or whose recipe is
 * invalid, is still listed, with the reason (AppEntry::unavailable_reason), so the catalog remains usable. An application that has a
 * download is installed into the directory next to the catalog that is named after its id, and its executable must be inside it.
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
