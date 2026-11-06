/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the catalog of the VR player. It is a JSON file (strict: unknown fields are refused, and errors say where they are):
 *
 *   {
 *     "catalog_version": 0,
 *     "apps": [
 *       {
 *         "id": "fr08",                              letters, digits, '-' and '_'; each once
 *         "recipe": "recipes/fr08.json",             the recipe which converts the application to stereo
 *         "executable": "fr08/fr08v101.exe",         what to start
 *         "arguments": "-windowed",                  (optional)
 *         "working_directory": "fr08",               (optional) the directory of the executable by default
 *         "title": "Fr-08: .the .product"            (optional) the name of the target of the recipe by default
 *         "download": {                             (optional) where the application is got if its executable is not there:
 *           "urls": ["https://..."],                  https only; one after the other until one works
 *           "sha256": "...",                         of the archive (a zip file), in hex: what is not that is not used
 *           "size": 66454,                          bytes of the archive
 *           "unpacked_size": 69184,                 the most that the files may take together
 *           "page": "https://..."                   (optional) where the archive can be got by hand
 *         }
 *       }
 *     ]
 *   }
 *
 * Relative paths are relative to the directory of the catalog. An application whose recipe or executable is not there, or whose
 * recipe is not valid, is listed with the reason (AppEntry::unavailable_reason): the catalog is still usable. An application which
 * has a download is put in the directory of the catalog which has its id as the name, and its executable has to be in it.
 */
#ifndef APPCATALOG_CATALOG_LOADER_H
#define APPCATALOG_CATALOG_LOADER_H

#include "utils/json/json_reader.h"
#include "vr/app_catalog/types.h"
#include <string>
#include <vector>

namespace VR
{
    namespace AppCatalog
    {
        class CatalogLoader
        {
        public:
            /* Returns false, and says why, if the catalog cannot be read. */
            static bool load(const std::string& in_filename,
                             Catalog*           out_catalog_ptr,
                             std::string*       out_error_ptr);

            /* The paths of the catalog which are relative are made so against the base directory. */
            static bool parse(const std::string& in_text,
                              const std::string& in_base_directory,
                              Catalog*           out_catalog_ptr,
                              std::string*       out_error_ptr);

        private:
            static void        check_availability(AppEntry*                      inout_app_ptr,
                                                  const std::string&             in_title);
            static std::string get_directory     (const std::string&             in_path);
            static bool        is_absolute_path  (const std::string&             in_path);
            static bool        parse_app         (const Utils::Json::JsonValue&  in_app,
                                                  const std::string&             in_path,
                                                  const std::string&             in_base_directory,
                                                  AppEntry*                      out_app_ptr,
                                                  std::string*                   out_title_ptr,
                                                  std::string*                   out_error_ptr);
            static bool        parse_download    (const Utils::Json::JsonValue&  in_download,
                                                  const std::string&             in_path,
                                                  const std::string&             in_install_directory,
                                                  VR::AppInstaller::InstallSpec* out_spec_ptr,
                                                  std::string*                   out_error_ptr);
            static bool        parse_number      (const Utils::Json::JsonValue&  in_object,
                                                  const std::string&             in_key,
                                                  const std::string&             in_path,
                                                  uint64_t*                      out_number_ptr,
                                                  std::string*                   out_error_ptr);
            static bool        parse_string      (const Utils::Json::JsonValue&  in_object,
                                                  const std::string&             in_key,
                                                  const bool&                    in_is_required,
                                                  const std::string&             in_path,
                                                  std::string*                   out_string_ptr,
                                                  std::string*                   out_error_ptr);
            static std::string resolve_path      (const std::string&             in_base_directory,
                                                  const std::string&             in_path);
        };
    }
}

#endif /* APPCATALOG_CATALOG_LOADER_H */
