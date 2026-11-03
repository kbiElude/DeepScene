/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/RecipeBake/command_line.h"

const char* Analysis::RecipeBake::CommandLine::get_usage_ptr()
{
    return "Usage: RecipeBake drive:\\path\\to\\recipe.json drive:\\path\\to\\file.passes [--deps file.deps] [-o baked.json]\n"
           "Bakes a recipe: writes into it what a converter which plays the application as it runs can know of each draw when the draw\n"
           "is issued, worked out from the analysis of the application (see APIDumpAnalysisExtractor). The recipe is written again in\n"
           "the formatting of this program; -o writes another file instead of replacing the recipe.\n"
           "\n"
           "--deps gives the dependencies between the frames (see APIDumpDependencyExtractor), which say which textures each eye is to have\n"
           "a copy of: without them a texture that frames use by turns (the one a frame renders to is the one the next frame reads) is\n"
           "missed. They must be of the workload that the .passes file is of.\n"
           "\n"
           "Exit codes: 0 done, 1 bad arguments, 2 a file cannot be read, 3 the recipe cannot be baked, 4 the file cannot be written.\n";
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

    result = true;

end:
    return result;
}
