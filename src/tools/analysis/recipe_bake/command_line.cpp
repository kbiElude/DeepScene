/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "command_line.h"

const char* Analysis::RecipeBake::CommandLine::get_usage_ptr()
{
    return "Usage: RecipeBake drive:\\path\\to\\recipe.json drive:\\path\\to\\file.passes [--deps file.deps] [-o baked.json]\n"
           "       RecipeBake --check drive:\\path\\to\\recipe.json drive:\\path\\to\\file.passes\n"
           "       RecipeBake --overlays drive:\\path\\to\\recipe.json drive:\\path\\to\\file.passes\n"
           "       RecipeBake --parity drive:\\path\\to\\recipe.json drive:\\path\\to\\file.passes\n"
           "       RecipeBake --scenes drive:\\path\\to\\recipe.json drive:\\path\\to\\file.passes [--screen-degrees 90]\n"
           "Bakes a recipe: writes into it what a converter which plays the application as it runs can know of each draw when the draw\n"
           "is issued, worked out from the analysis of the application (see APIDumpAnalysisExtractor). The recipe is written again in\n"
           "the formatting of this program; -o writes another file instead of replacing the recipe.\n"
           "\n"
           "--deps gives the dependencies between the frames (see APIDumpDependencyExtractor), which say which textures each eye is to have\n"
           "a copy of: without them a texture that frames use by turns (the one a frame renders to is the one the next frame reads) is\n"
           "missed. They must be of the workload that the .passes file is of.\n"
           "\n"
           "--check writes nothing: it says which kinds of frames and which draw signatures of the workload the recipe does not know (a baked recipe of\n"
           "another capture, or of a shorter one, leaves them to chance: a frame of an unknown shape is played as the unknown_frame_policy of the recipe says,\n"
           "and a draw of an unknown signature is an affected one). It also says which passes the recipe leaves alone look like 3D scenes (their vertices are at\n"
           "many depths, where an overlay is flat), and which a replay leaves alone and the converter that plays the application as it runs does not.\n"
           "\n"
           "--overlays writes nothing either: it prints, as JSON, the draws that may be 2D overlays. A draw is listed if the recipe leaves it alone (as a replay asks it, by the\n"
           "keys of the draw and of its pass, and as a converter which plays the application as it runs asks it, by the signature of the draw over the cameras of its pass)\n"
           "or if the camera of its pass never changes (the hint of APIDumpAnalysisQuery overlays); \"agreement\" says which of the two think so. \"depth\" says how deep the\n"
           "vertices of its pass are: a flat one is an overlay, and one of many depths is a 3D scene seen from one place.\n"
           "--parity writes nothing: it asks the recipe about each draw of the analysis the way the replay does (by the keys of the pass and of the draw) and the way the converter that plays the\n"
           "application as it runs does (by the signature of the draw, over each camera of its pass), with each size the draw was seen with, and lists the draws on which the answers differ (class or push).\n"
           "--scenes writes nothing: it prints, as JSON, each scene (kind of frame) that has depth: where its frames are (the runs of frames that follow one another, the cuts), where the eyes converge\n"
           "and how far apart they may be with the recipe as it is (its comfort, and what it says of the scene), and what that makes of the parallax of the nearest and of the farthest surfaces as a part of\n"
           "the width of the picture and as an angle on a screen that is --screen-degrees wide (the log of the headset says it), at the strength 1 that the player starts with.\n"
           "\n"
           "Exit codes: 0 done, 1 bad arguments, 2 a file cannot be read, 3 the recipe cannot be baked, 4 the file cannot be written, 5 (--check) the recipe\n"
           "does not know all of the workload, 6 (--check) it knows it but leaves alone what looks like a 3D scene, or what a replay and the converter treat differently, 7 (--parity) the replay\n"
           "and the converter decide differently for some draw.\n";

}

bool Analysis::RecipeBake::CommandLine::parse(const std::vector<std::string>& in_argument_vec,
                                              Options*                        out_options_ptr,
                                              std::string*                    out_error_ptr)
{
    size_t n_positionals = 0;
    bool   result        = false;

    *out_options_ptr = Options();

    for (size_t n_argument = 0;
                n_argument < in_argument_vec.size();
              ++n_argument)
    {
        const std::string& argument = in_argument_vec.at(n_argument);

        if (argument == "--check")
        {
            out_options_ptr->checks_only = true;
        }
        else
        if (argument == "--parity")
        {
            out_options_ptr->audits_parity = true;
        }
        else
        if (argument == "--overlays")
        {
            out_options_ptr->lists_overlays = true;
        }
        else
        if (argument == "--scenes")
        {
            out_options_ptr->shows_scenes = true;
        }
        else
        if (argument == "--screen-degrees")
        {
            if (n_argument + 1 >= in_argument_vec.size() )
            {
                *out_error_ptr = "--screen-degrees needs a number";

                goto end;
            }

            out_options_ptr->screen_degrees = static_cast<float>(::atof(in_argument_vec.at(++n_argument).c_str() ) );

            if (out_options_ptr->screen_degrees <= 0.0f ||
                out_options_ptr->screen_degrees >  360.0f)
            {
                *out_error_ptr = "--screen-degrees needs a number above 0 and up to 360";

                goto end;
            }
        }
        else
        if (argument == "-o")
        {
            if (n_argument + 1 >= in_argument_vec.size() )
            {
                *out_error_ptr = "-o needs a file name";

                goto end;
            }

            out_options_ptr->output_filename = in_argument_vec.at(++n_argument);
        }
        else
        if (argument == "--deps")
        {
            if (n_argument + 1 >= in_argument_vec.size() )
            {
                *out_error_ptr = "--deps needs a file name";

                goto end;
            }

            out_options_ptr->deps_filename = in_argument_vec.at(++n_argument);
        }
        else
        if (argument.empty() == false &&
            argument.at(0)   == '-')
        {
            *out_error_ptr = "Unknown argument " + argument;

            goto end;
        }
        else
        if (n_positionals == 0)
        {
            out_options_ptr->recipe_filename = argument;

            n_positionals++;
        }
        else
        if (n_positionals == 1)
        {
            out_options_ptr->passes_filename = argument;

            n_positionals++;
        }
        else
        {
            *out_error_ptr = "Unexpected argument " + argument;

            goto end;
        }
    }

    if (n_positionals != 2)
    {
        *out_error_ptr = "A recipe and a .passes file are needed";

        goto end;
    }

    if (out_options_ptr->checks_only &&
        out_options_ptr->lists_overlays)
    {
        *out_error_ptr = "--check and --overlays cannot be given together";

        goto end;
    }

    if (out_options_ptr->shows_scenes                                      &&
        (out_options_ptr->checks_only || out_options_ptr->lists_overlays))
    {
        *out_error_ptr = "--scenes cannot be given with --check or --overlays";

        goto end;
    }

    if (out_options_ptr->audits_parity                                                                              &&
        (out_options_ptr->checks_only || out_options_ptr->lists_overlays || out_options_ptr->shows_scenes) )
    {
        *out_error_ptr = "--parity cannot be given with --check, --overlays or --scenes";

        goto end;
    }

    result = true;

end:
    return result;
}
