/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The contents of the VR player catalog: the applications that can be played in VR, each with the recipe that converts it and what is
 * needed to start it.
 */
#ifndef APPCATALOG_TYPES_H
#define APPCATALOG_TYPES_H

#include "vr_player/app_installer/types.h"
#include <string>
#include <vector>

namespace VRPlayer
{
    namespace AppCatalog
    {
        struct AppEntry
        {
            std::string                         arguments;                  /* command line arguments of the application                                        */
            std::string                         description;                /* from the target of the recipe                                                    */
            std::string                         executable_path;            /* absolute                                                                         */
            std::string                         id;                         /* in the catalog; letters, digits, '-' and '_'                                     */
            VRPlayer::AppInstaller::InstallSpec install_spec;               /* where to download it, if the catalog says so; empty otherwise                    */
            bool                                is_download_needed = false; /* the executable is missing and the catalog says where to download it              */
            std::string                         launch_button_text;         /* text of a button in the first dialog of the application, which the launcher presses for the user; empty: none */
            std::string                         name;                       /* the displayed name: the title, else the name of the recipe target, else the id */
            float                               overlay_parallax   = 0.0f;  /* from the comfort section of the recipe: how far overlays may be pushed toward the viewer; 0 if the recipe gives no amount, which means the application wants none */
            std::string                         recipe_path;                /* absolute                                                                         */
            std::string                         thumbnail_path;             /* where the picture of the application is kept once it is downloaded; empty if the catalog gives no address */
            std::string                         thumbnail_url;              /* https address of the picture; empty if there is none                             */
            std::string                         unavailable_reason;         /* empty if the application can be launched                                         */
            std::string                         untick_checkbox_text;       /* text of a checkbox in the same dialog, which the launcher unticks before it presses the button; empty: none */
            std::string                         working_directory;          /* absolute; the directory of the executable unless the catalog names another       */
        };

        struct Catalog
        {
            std::vector<AppEntry> app_vec;
        };
    }
}

#endif /* APPCATALOG_TYPES_H */
