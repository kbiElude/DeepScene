/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "capture/api_dump_loader/api_info.h"
#include "capture/api_dump_names/function_names.h"
#include "common/argument_reader.h"
#include "common/macros.h"
#include "gl_run_call_classifier.h"
#include <string>

namespace
{
    /* The functions that are listed here are told apart by what they are. The rest of OpenGL is told apart by the name: a call that asks for something begins with glGet or glIs. */
    Stereo::ConverterDLL::RunCallClass classify_gl_function(const APIInterceptor::APIFunction& in_function)
    {
        Stereo::ConverterDLL::RunCallClass result = Stereo::ConverterDLL::RunCallClass::FLUSH;

        switch (in_function)
        {
            case APIInterceptor::APIFUNCTION_GL_GLBEGIN:
            case APIInterceptor::APIFUNCTION_GL_GLEND:
            {
                result = Stereo::ConverterDLL::RunCallClass::NEUTRAL;

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLFRUSTUM:
            case APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY:
            case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD:
            case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF:
            case APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE:
            case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXD:
            case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF:
            case APIInterceptor::APIFUNCTION_GL_GLORTHO:
            case APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX:
            case APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX:
            case APIInterceptor::APIFUNCTION_GL_GLROTATED:
            case APIInterceptor::APIFUNCTION_GL_GLROTATEF:
            case APIInterceptor::APIFUNCTION_GL_GLSCALED:
            case APIInterceptor::APIFUNCTION_GL_GLSCALEF:
            case APIInterceptor::APIFUNCTION_GL_GLTRANSLATED:
            case APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF:
            {
                result = Stereo::ConverterDLL::RunCallClass::MATRIX;

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
            case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
            case APIInterceptor::APIFUNCTION_GL_GLCOLORMASK:
            case APIInterceptor::APIFUNCTION_GL_GLCULLFACE:
            case APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC:
            case APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK:
            case APIInterceptor::APIFUNCTION_GL_GLDISABLE:
            case APIInterceptor::APIFUNCTION_GL_GLENABLE:
            case APIInterceptor::APIFUNCTION_GL_GLFRONTFACE:
            case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
            case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
            case APIInterceptor::APIFUNCTION_GL_GLTEXENVI:
            {
                result = Stereo::ConverterDLL::RunCallClass::RECORDED;

                break;
            }

            default:
            {
                std::string name;

                {
                    const char* const name_ptr = Capture::APIDumpNames::get_function_name(in_function);

                    name = (name_ptr != nullptr) ? name_ptr
                                                 : "";
                }

                const size_t cut = name.find_last_of(":. ");

                if (cut != std::string::npos)
                {
                    name = name.substr(cut + 1);
                }

                if (name.compare(0, 5, "glGet") == 0 ||
                    name.compare(0, 4, "glIs")  == 0)
                {
                    result = Stereo::ConverterDLL::RunCallClass::QUERY;
                }

                break;
            }
        }

        return result;
    }

    /* How many numbers a recorded state call has. */
    uint32_t get_n_state_arguments(const APIInterceptor::APIFunction& in_function)
    {
        uint32_t result = 0;

        switch (in_function)
        {
            case APIInterceptor::APIFUNCTION_GL_GLCULLFACE:
            case APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC:
            case APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK:
            case APIInterceptor::APIFUNCTION_GL_GLDISABLE:
            case APIInterceptor::APIFUNCTION_GL_GLENABLE:
            case APIInterceptor::APIFUNCTION_GL_GLFRONTFACE:
            {
                result = 1;

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
            case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
            {
                result = 2;

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
            case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
            case APIInterceptor::APIFUNCTION_GL_GLTEXENVI:
            {
                result = 3;

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLCOLORMASK:
            {
                result = 4;

                break;
            }

            default:
            {
                AI_FAIL_UNSUPPORTED(in_function);

                break;
            }
        }

        return result;
    }

    std::vector<Stereo::ConverterDLL::RunCallClass> make_class_table()
    {
        std::vector<Stereo::ConverterDLL::RunCallClass> result(APIInterceptor::APIFUNCTION_COUNT,
                                                               Stereo::ConverterDLL::RunCallClass::NEUTRAL);

        for (uint32_t n_api_func = 0;
                      n_api_func < APIInterceptor::APIFUNCTION_COUNT;
                    ++n_api_func)
        {
            const auto function = static_cast<APIInterceptor::APIFunction>(n_api_func);
            const auto group    = Capture::APIDumpLoader::get_api_group(function);

            /* The calls of the window system that make or change the context, and the end of the frame, need the right eye to be complete. */
            if (group == Capture::APIDumpLoader::APIGroup::GL)
            {
                result.at(n_api_func) = classify_gl_function(function);
            }
            else
            if (group    == Capture::APIDumpLoader::APIGroup::WGL ||
                function == APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS)
            {
                result.at(n_api_func) = Stereo::ConverterDLL::RunCallClass::FLUSH;
            }
        }

        return result;
    }
}

Stereo::ConverterDLL::RunCallClass Stereo::ConverterDLL::GLRunCallClassifier::classify(const APIInterceptor::APIFunction& in_function)
{
    static const std::vector<RunCallClass> class_vec = make_class_table();

    return class_vec.at(in_function);
}

bool Stereo::ConverterDLL::GLRunCallClassifier::read_state_call(const APIInterceptor::APIFunction&         in_function,
                                                          const uint32_t&                            in_n_arguments,
                                                          const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                          RunStateCall*                              out_call_ptr)
{
    const uint32_t n_values = get_n_state_arguments(in_function);
    bool           result   = (in_n_arguments >= n_values);

    out_call_ptr->function = in_function;

    for (uint32_t n_value = 0;
                  n_value < n_values && result;
                ++n_value)
    {
        result = APIInterceptor::read_number(in_arguments_ptr[n_value],
                                            &out_call_ptr->value_i32vec4.at(n_value) );
    }

    return result;
}
