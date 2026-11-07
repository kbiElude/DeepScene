/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/recipe/recipe_loader.h"
#include "utils/files/file_system.h"
#include "vr_player/app_catalog/catalog_loader.h"
#include <algorithm>
#include <set>

void VRPlayer::AppCatalog::CatalogLoader::check_availability(AppEntry*          inout_app_ptr,
                                                             const std::string& in_title)
{
    std::string              error;
    Analysis::Recipe::Recipe recipe;

    inout_app_ptr->name = in_title;

    if (Utils::Files::FileSystem::is_file(inout_app_ptr->recipe_path) == false)
    {
        inout_app_ptr->unavailable_reason = "The recipe cannot be found: " +
                                            inout_app_ptr->recipe_path;
    }
    else
    if (Analysis::Recipe::RecipeLoader::load(inout_app_ptr->recipe_path,
                                             &recipe,
                                             &error) == false)
    {
        inout_app_ptr->unavailable_reason = "The recipe is not valid: " +
                                            error;
    }
    else
    {
        inout_app_ptr->description      = recipe.target_description;
        inout_app_ptr->overlay_parallax = recipe.comfort.overlay_parallax;

        if (inout_app_ptr->name.empty() )
        {
            inout_app_ptr->name = recipe.target_name;
        }
    }

    if (inout_app_ptr->unavailable_reason.empty() )
    {
        const bool is_executable_found = Utils::Files::FileSystem::is_file(inout_app_ptr->executable_path);

        if (is_executable_found == false)
        {
            if (inout_app_ptr->install_spec.url_vec.empty() )
            {
                inout_app_ptr->unavailable_reason = "The application cannot be found: " +
                                                    inout_app_ptr->executable_path;
            }
            else
            {
                inout_app_ptr->is_download_needed = true;
            }
        }
    }

    if (inout_app_ptr->name.empty() )
    {
        inout_app_ptr->name = inout_app_ptr->id;
    }
}

std::string VRPlayer::AppCatalog::CatalogLoader::get_directory(const std::string& in_path)
{
    const size_t n_separator = in_path.find_last_of("\\/");

    return (n_separator != std::string::npos) ? in_path.substr(0,
                                                               n_separator)
                                              : std::string();
}

bool VRPlayer::AppCatalog::CatalogLoader::is_absolute_path(const std::string& in_path)
{
    /* A drive, or a network share; a path which starts with a separator only is of the current drive. */
    const size_t n_characters = in_path.size();
    bool         result       = false;

    if (n_characters >= 2)
    {
        const char first_character  = in_path.at(0);
        const char second_character = in_path.at(1);

        result = (second_character == ':'  ||
                  (first_character  == '\\' &&
                   second_character == '\\') );
    }

    return result;
}

bool VRPlayer::AppCatalog::CatalogLoader::load(const std::string& in_filename,
                                               Catalog*           out_catalog_ptr,
                                               std::string*       out_error_ptr)
{
    bool        result = false;
    std::string text;

    if (Utils::Json::JsonReader::read_file(in_filename,
                                           &text) == false)
    {
        *out_error_ptr = "Cannot open " + in_filename;

        goto end;
    }

    {
        const std::string resolved_filename = resolve_path(std::string(),
                                                           in_filename);
        const std::string directory         = get_directory(resolved_filename);

        result = parse(text,
                       directory,
                       out_catalog_ptr,
                       out_error_ptr);
    }

end:
    return result;
}

bool VRPlayer::AppCatalog::CatalogLoader::parse(const std::string& in_text,
                                                const std::string& in_base_directory,
                                                Catalog*           out_catalog_ptr,
                                                std::string*       out_error_ptr)
{
    constexpr double SUPPORTED_CATALOG_VERSION = 0.0;

    std::string            json_error;
    std::set<std::string>  id_set;
    Utils::Json::JsonValue root;
    bool                   result     = false;

    *out_catalog_ptr = Catalog();

    const std::string catalog_text = Utils::Json::JsonReader::strip_byte_order_mark(in_text);

    if (Utils::Json::JsonReader::parse(catalog_text,
                                       &root,
                                       &json_error) == false)
    {
        *out_error_ptr = "Not valid JSON: " +
                         json_error;

        goto end;
    }

    if (root.type != Utils::Json::JsonValueType::OBJECT)
    {
        *out_error_ptr = "The catalog must be a JSON object";

        goto end;
    }

    {
        const std::string* unknown_key_ptr = root.find_unknown_key_ptr({"apps", "catalog_version"});

        if (unknown_key_ptr != nullptr)
        {
            *out_error_ptr = "unknown field \"" +
                             *unknown_key_ptr   +
                             "\"";

            goto end;
        }
    }

    {
        const auto& version_value = root.at("catalog_version");

        if (version_value.type         != Utils::Json::JsonValueType::NUMBER ||
            version_value.number_value != SUPPORTED_CATALOG_VERSION)
        {
            *out_error_ptr = "catalog_version: must be 0";

            goto end;
        }
    }

    if (root.at("apps").type != Utils::Json::JsonValueType::ARRAY)
    {
        *out_error_ptr = "apps: expected a list";

        goto end;
    }

    for (size_t n_app = 0;
                n_app < root.at("apps").item_vec.size();
              ++n_app)
    {
        AppEntry          app;
        const std::string path  = "apps[" + std::to_string(n_app) + "]";
        std::string       title;

        if (parse_app(root.at("apps").item_vec.at(n_app),
                      path,
                      in_base_directory,
                     &app,
                     &title,
                      out_error_ptr) == false)
        {
            goto end;
        }

        if (id_set.insert(app.id).second == false)
        {
            *out_error_ptr = path                                +
                             ".id: \""                           +
                             app.id                              +
                             "\" is used by another application";

            goto end;
        }

        check_availability(&app,
                           title);

        out_catalog_ptr->app_vec.push_back(app);
    }

    result = true;

end:
    return result;
}

bool VRPlayer::AppCatalog::CatalogLoader::parse_app(const Utils::Json::JsonValue& in_app,
                                                    const std::string&            in_path,
                                                    const std::string&            in_base_directory,
                                                    AppEntry*                     out_app_ptr,
                                                    std::string*                  out_title_ptr,
                                                    std::string*                  out_error_ptr)
{
    std::string executable;
    std::string recipe;
    bool        result            = false;
    std::string working_directory;

    if (in_app.type != Utils::Json::JsonValueType::OBJECT)
    {
        *out_error_ptr = in_path                +
                         ": expected an object";

        goto end;
    }

    {
        const std::string* unknown_key_ptr = in_app.find_unknown_key_ptr({"arguments",
                                                                          "download",
                                                                          "executable",
                                                                          "id",
                                                                          "recipe",
                                                                          "title",
                                                                          "working_directory"});

        if (unknown_key_ptr != nullptr)
        {
            *out_error_ptr = in_path + ": unknown field \"" + *unknown_key_ptr + "\"";

            goto end;
        }
    }

    {
        bool is_parsed = parse_string(in_app,
                                      "id",
                                      true, /* in_is_required */
                                      in_path,
                                     &out_app_ptr->id,
                                      out_error_ptr);

        if (is_parsed)
        {
            is_parsed = parse_string(in_app,
                                     "recipe",
                                     true, /* in_is_required */
                                     in_path,
                                    &recipe,
                                     out_error_ptr);
        }

        if (is_parsed)
        {
            is_parsed = parse_string(in_app,
                                     "executable",
                                     true, /* in_is_required */
                                     in_path,
                                    &executable,
                                     out_error_ptr);
        }

        if (is_parsed)
        {
            is_parsed = parse_string(in_app,
                                     "arguments",
                                     false, /* in_is_required */
                                     in_path,
                                    &out_app_ptr->arguments,
                                     out_error_ptr);
        }

        if (is_parsed)
        {
            is_parsed = parse_string(in_app,
                                     "working_directory",
                                     false, /* in_is_required */
                                     in_path,
                                    &working_directory,
                                     out_error_ptr);
        }

        if (is_parsed)
        {
            is_parsed = parse_string(in_app,
                                     "title",
                                     false, /* in_is_required */
                                     in_path,
                                     out_title_ptr,
                                     out_error_ptr);
        }

        if (is_parsed == false)
        {
            goto end;
        }
    }

    if (out_app_ptr->id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != std::string::npos)
    {
        *out_error_ptr = in_path + ".id: only letters, digits, '-' and '_' are allowed";

        goto end;
    }

    out_app_ptr->executable_path   = resolve_path(in_base_directory,
                                                  executable);

    {
        const auto* download_ptr = in_app.find_ptr("download");

        if (download_ptr != nullptr)
        {
            const std::string install_directory = resolve_path(in_base_directory,
                                                               out_app_ptr->id);

            if (parse_download(*download_ptr,
                               in_path + ".download",
                               install_directory,
                              &out_app_ptr->install_spec,
                               out_error_ptr) == false)
            {
                goto end;
            }

            /* The executable is in what is downloaded; a catalog which says otherwise has a mistake in it. */
            const size_t n_directory_characters  = install_directory.size() + 1;
            const size_t n_executable_characters = out_app_ptr->executable_path.size();
            bool         is_outside_directory    = true;

            if (n_executable_characters > n_directory_characters)
            {
                const std::string directory_with_separator = install_directory + "\\";
                const char* const directory_ptr            = directory_with_separator.c_str();
                const char* const executable_path_ptr      = out_app_ptr->executable_path.c_str();
                const int         path_difference          = _strnicmp(executable_path_ptr,
                                                                       directory_ptr,
                                                                       n_directory_characters);

                is_outside_directory = (path_difference != 0);
            }

            if (is_outside_directory)
            {
                *out_error_ptr = in_path                                   +
                                 ".executable: has to be in "              +
                                 install_directory                         +
                                 " for an application which is downloaded";

                goto end;
            }
        }
    }
    out_app_ptr->recipe_path       = resolve_path(in_base_directory,
                                                  recipe);
    out_app_ptr->working_directory = (working_directory.empty() ) ? get_directory(out_app_ptr->executable_path)
                                                                  : resolve_path (in_base_directory,
                                                                                  working_directory);

    result = true;

end:
    return result;
}

bool VRPlayer::AppCatalog::CatalogLoader::parse_download(const Utils::Json::JsonValue&        in_download,
                                                         const std::string&                   in_path,
                                                         const std::string&                   in_install_directory,
                                                         VRPlayer::AppInstaller::InstallSpec* out_spec_ptr,
                                                         std::string*                         out_error_ptr)
{
    bool is_parsed = false;
    bool result    = false;

    if (in_download.type != Utils::Json::JsonValueType::OBJECT)
    {
        *out_error_ptr = in_path                +
                         ": expected an object";

        goto end;
    }

    {
        const std::string* unknown_key_ptr = in_download.find_unknown_key_ptr({"page",
                                                                               "sha256",
                                                                               "size",
                                                                               "unpacked_size",
                                                                               "urls"});

        if (unknown_key_ptr != nullptr)
        {
            *out_error_ptr = in_path + ": unknown field \"" + *unknown_key_ptr + "\"";

            goto end;
        }
    }

    out_spec_ptr->install_directory = in_install_directory;

    {
        bool        are_urls_valid = false;
        const auto* urls_ptr       = in_download.find_ptr("urls");

        if (urls_ptr != nullptr)
        {
            const bool is_empty = urls_ptr->item_vec.empty();
            const bool is_list  = (urls_ptr->type == Utils::Json::JsonValueType::ARRAY);

            are_urls_valid = (is_list &&
                              is_empty == false);
        }

        if (are_urls_valid == false)
        {
            *out_error_ptr = in_path + ".urls: expected a list of addresses, with one at least";

            goto end;
        }

        for (size_t n_url = 0;
                    n_url < urls_ptr->item_vec.size();
                  ++n_url)
        {
            const Utils::Json::JsonValue& url_value = urls_ptr->item_vec.at(n_url);

            bool is_https_address = false;

            if (url_value.type == Utils::Json::JsonValueType::STRING)
            {
                const int scheme_difference = url_value.string_value.compare(0,
                                                                             8,
                                                                             "https://");

                is_https_address = (scheme_difference == 0);
            }

            if (is_https_address == false)
            {
                *out_error_ptr = in_path                           +
                                 ".urls["                          +
                                 std::to_string(n_url)             +
                                 "]: expected an address of https";

                goto end;
            }

            out_spec_ptr->url_vec.push_back(url_value.string_value);
        }
    }

    is_parsed = parse_string(in_download,
                             "sha256",
                             true, /* in_is_required */
                             in_path,
                            &out_spec_ptr->sha256_hex,
                             out_error_ptr);

    if (is_parsed == false)
    {
        goto end;
    }

    std::transform(out_spec_ptr->sha256_hex.begin(),
                   out_spec_ptr->sha256_hex.end  (),
                   out_spec_ptr->sha256_hex.begin(),
                   ::tolower);

    {
        const size_t first_non_hex_position = out_spec_ptr->sha256_hex.find_first_not_of("0123456789abcdef");
        const size_t n_digits               = out_spec_ptr->sha256_hex.size();

        if (n_digits               != 64                 ||
            first_non_hex_position != std::string::npos)
        {
            *out_error_ptr = in_path + ".sha256: expected 64 digits of hex";

            goto end;
        }
    }

    if (parse_number(in_download,
                     "size",
                     in_path,
                    &out_spec_ptr->n_archive_bytes,
                     out_error_ptr) == false)
    {
        goto end;
    }

    if (parse_number(in_download,
                     "unpacked_size",
                     in_path,
                    &out_spec_ptr->n_unpacked_bytes_max,
                     out_error_ptr) == false)
    {
        goto end;
    }

    if (parse_string(in_download,
                     "page",
                     false, /* in_is_required */
                     in_path,
                    &out_spec_ptr->page_url,
                     out_error_ptr) == false)
    {
        goto end;
    }

    result = true;

end:
    return result;
}

bool VRPlayer::AppCatalog::CatalogLoader::parse_number(const Utils::Json::JsonValue& in_object,
                                                       const std::string&            in_key,
                                                       const std::string&            in_path,
                                                       uint64_t*                     out_number_ptr,
                                                       std::string*                  out_error_ptr)
{
    constexpr double N_BYTES_MAX = 4294967296.0;

    const auto* value_ptr = in_object.find_ptr(in_key);
    bool        result    = false;

    if (value_ptr               == nullptr                                                            ||
        value_ptr->type         != Utils::Json::JsonValueType::NUMBER                                 ||
        value_ptr->number_value <  1.0                                                                ||
        value_ptr->number_value >  N_BYTES_MAX                                                        ||
        value_ptr->number_value != static_cast<double>(static_cast<uint64_t>(value_ptr->number_value) ) )
    {
        *out_error_ptr = in_path                                                    +
                         "."                                                        +
                         in_key                                                     +
                         ": expected a whole number of bytes, from 1 to 4294967296";

        goto end;
    }

    *out_number_ptr = static_cast<uint64_t>(value_ptr->number_value);
    result          = true;

end:
    return result;
}

bool VRPlayer::AppCatalog::CatalogLoader::parse_string(const Utils::Json::JsonValue& in_object,
                                                       const std::string&            in_key,
                                                       const bool&                   in_is_required,
                                                       const std::string&            in_path,
                                                       std::string*                  out_string_ptr,
                                                       std::string*                  out_error_ptr)
{
    const auto* value_ptr = in_object.find_ptr(in_key);
    bool        result    = false;

    if (value_ptr == nullptr)
    {
        if (in_is_required)
        {
            *out_error_ptr = in_path        +
                             "."            +
                             in_key         +
                             ": is missing";

            goto end;
        }

        result = true;

        goto end;
    }

    {
        const bool is_text_empty = value_ptr->string_value.empty();

        if (value_ptr->type != Utils::Json::JsonValueType::STRING ||
            (in_is_required                                       &&
             is_text_empty) )
        {
            *out_error_ptr = in_path                                         +
                             "."                                             +
                             in_key                                          +
                             ": expected "                                   +
                             ((in_is_required) ? "a text which is not empty"
                                               : "a text");

            goto end;
        }
    }

    *out_string_ptr = value_ptr->string_value;
    result          = true;

end:
    return result;
}

std::string VRPlayer::AppCatalog::CatalogLoader::resolve_path(const std::string& in_base_directory,
                                                              const std::string& in_path)
{
    const bool               has_base_directory = (in_base_directory.empty() == false);
    const bool               is_absolute        = is_absolute_path(in_path);
    std::string              joined             = (is_absolute || has_base_directory == false) ? in_path
                                                                                               : in_base_directory +
                                                                                                 "\\"              +
                                                                                                 in_path;
    std::vector<std::string> part_vec;
    std::string              result;
    size_t                   start              = 0;

    std::replace(joined.begin(),
                 joined.end  (),
                 '/',
                 '\\');

    /* "." and ".." are resolved, so that what is shown is where the file is; a ".." which has nothing to remove is kept. */
    while (start <= joined.size() )
    {
        size_t            end  = joined.find('\\',
                                             start);
        const size_t      stop = (end == std::string::npos) ? joined.size()
                                                            : end;
        const std::string part = joined.substr(start,
                                               stop - start);

        bool can_remove_previous = false;

        /* A ".." removes the part before it, unless that is a ".." or a drive. */
        if (part == "..")
        {
            const bool has_previous = (part_vec.empty() == false);

            if (has_previous)
            {
                const std::string& previous_part  = part_vec.back();
                const size_t       drive_position = previous_part.find(':');

                can_remove_previous = (previous_part  != ".." &&
                                       drive_position == std::string::npos);
            }
        }

        if (part == ".")
        {
            /* Nothing to add. */
        }
        else
        if (can_remove_previous)
        {
            part_vec.pop_back();
        }
        else
        {
            part_vec.push_back(part);
        }

        start = stop + 1;
    }

    for (size_t n_part = 0;
                n_part < part_vec.size();
              ++n_part)
    {
        result += (n_part > 0) ? "\\" + part_vec.at(n_part)
                               :        part_vec.at(n_part);
    }

    return result;
}
