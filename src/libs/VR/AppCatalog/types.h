/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the catalog of the VR player holds: the applications which can be played in VR, each with the recipe which converts it
 * and what it takes to start it.
 */
#ifndef APPCATALOG_TYPES_H
#define APPCATALOG_TYPES_H

#include "VR/AppInstaller/types.h"
#include <string>
#include <vector>

namespace VR
{
    namespace AppCatalog
    {
        struct AppEntry
        {
            std::string                   arguments;                  /* of the command line of the application                                           */
            std::string                   description;                /* from the target of the recipe                                                    */
            std::string                   executable_path;            /* absolute                                                                         */
            std::string                   id;                         /* in the catalog; letters, digits, '-' and '_'                                     */
            VR::AppInstaller::InstallSpec install_spec;               /* where it can be got, if the catalog says; empty if not                           */
            bool                          is_download_needed = false; /* the executable is not there, and the catalog says where to get it                */
            std::string                   name;                       /* what is shown: the title, else the name of the target of the recipe, else the id */
            float                         overlay_parallax   = 0.0f;  /* from the comfort of the recipe: how far its overlays can be pushed toward the viewer; 0 if it has no amount, which means the application wants none */
            std::string                   recipe_path;                /* absolute                                                                         */
            std::string                   unavailable_reason;         /* empty if the application can be launched                                         */
            std::string                   working_directory;          /* absolute; the directory of the executable unless the catalog says another        */
        };

        struct Catalog
        {
            std::vector<AppEntry> app_vec;
        };
    }
}

#endif /* APPCATALOG_TYPES_H */
