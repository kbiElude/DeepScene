/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The list of GL functions that the front end does not name was generated and is maintained by hand. The groups and their reasons are what matters.
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_function_policy.h"
#include "capture/api_dump_names/function_names.h"
#include <string.h>

bool Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::bakes_current_matrix(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    /* The position or direction of a light, a clip plane, the planes for generating texture coordinates (the forms that take an array; the other forms
     * choose a mode) and the raster position are given in the coordinates of the program. OpenGL converts them to eye coordinates with the model-view
     * matrix that is current at the moment of the call.
     */
    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLCLIPPLANE:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTFV:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTIV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXGENDV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXGENIV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS2D:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS2DV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS2F:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS2FV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS2I:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS2IV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS2S:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS2SV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS3D:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS3DV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS3F:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS3FV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS3I:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS3IV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS3S:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS3SV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS4D:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS4DV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS4F:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS4FV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS4I:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS4IV:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS4S:
        case APIInterceptor::APIFUNCTION_GL_GLRASTERPOS4SV:
        {
            result = true;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::changes_what_a_draw_writes(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    /* The stencil test and its operations, the logic op, the blend equation, the separate blend factors, the blend constant color, and the choice of
     * which face side is the front. These decide which draws reach the framebuffer and how they combine with it, as the culling that the model follows
     * does. The model follows the basic blending and culling functions (glBlendFunc, glCullFace) and tells draws apart by them. The functions listed here are the remaining ones.
     */
    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLBLENDCOLOR:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDEQUATION:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDEQUATIONSEPARATE:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNCSEPARATE:
        case APIInterceptor::APIFUNCTION_GL_GLFRONTFACE:
        case APIInterceptor::APIFUNCTION_GL_GLLOGICOP:
        case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
        case APIInterceptor::APIFUNCTION_GL_GLSTENCILMASK:
        case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
        {
            result = true;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::is_left_out_by_design(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    switch (in_function)
    {
        /* Values for the vertices that follow. The model builds the geometry of a draw from the vertex positions alone, so a color, a normal or a
         * texture coordinate is not part of the draw structure. The replay still makes these calls.
         */
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3B:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3BV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3DV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3F:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3FV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3I:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3IV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3S:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3SV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3UB:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3UBV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3UI:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3UIV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3US:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3USV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4B:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4BV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4D:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4DV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4F:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4FV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4I:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4IV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4S:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4SV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4UB:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4UBV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4UI:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4UIV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4US:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4USV:
        case APIInterceptor::APIFUNCTION_GL_GLEDGEFLAG:
        case APIInterceptor::APIFUNCTION_GL_GLEDGEFLAGV:
        case APIInterceptor::APIFUNCTION_GL_GLFOGCOORDD:
        case APIInterceptor::APIFUNCTION_GL_GLFOGCOORDDV:
        case APIInterceptor::APIFUNCTION_GL_GLFOGCOORDF:
        case APIInterceptor::APIFUNCTION_GL_GLFOGCOORDFV:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXD:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXDV:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXF:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXFV:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXI:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXIV:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXS:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXSV:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXUB:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXUBV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD1D:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD1DV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD1F:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD1FV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD1I:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD1IV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD1S:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD1SV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD2D:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD2DV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD2F:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD2FV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD2I:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD2IV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD2S:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD2SV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD3D:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD3DV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD3F:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD3FV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD3I:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD3IV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD3S:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD3SV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD4D:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD4DV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD4F:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD4FV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD4I:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD4IV:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD4S:
        case APIInterceptor::APIFUNCTION_GL_GLMULTITEXCOORD4SV:
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3B:
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3BV:
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3D:
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3DV:
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3F:
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3FV:
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3I:
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3IV:
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3S:
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3SV:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3B:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3BV:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3D:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3DV:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3F:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3FV:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3I:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3IV:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3S:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3SV:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3UB:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3UBV:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3UI:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3UIV:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3US:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLOR3USV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD1DV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD1F:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD1FV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD1I:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD1IV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD1S:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD1SV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2DV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2F:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2FV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2I:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2IV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2S:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2SV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD3DV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD3F:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD3FV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD3I:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD3IV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD3S:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD3SV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD4D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD4DV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD4F:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD4FV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD4I:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD4IV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD4S:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD4SV:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS2D:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS2DV:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS2F:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS2FV:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS2I:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS2IV:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS2S:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS2SV:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS3D:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS3DV:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS3F:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS3FV:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS3I:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS3IV:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS3S:
        case APIInterceptor::APIFUNCTION_GL_GLWINDOWPOS3SV:
        {
            result = true;

            break;
        }

        /* State of the arrays that a draw reads. A draw from arrays comes with a snapshot of the data it reads (see GLArrayDrawDescriber), so the
         * location of the arrays and which of them are enabled are not needed.
         */
        case APIInterceptor::APIFUNCTION_GL_GLCLIENTACTIVETEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLCOLORPOINTER:
        case APIInterceptor::APIFUNCTION_GL_GLDISABLECLIENTSTATE:
        case APIInterceptor::APIFUNCTION_GL_GLEDGEFLAGPOINTER:
        case APIInterceptor::APIFUNCTION_GL_GLENABLECLIENTSTATE:
        case APIInterceptor::APIFUNCTION_GL_GLFOGCOORDPOINTER:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXPOINTER:
        case APIInterceptor::APIFUNCTION_GL_GLINTERLEAVEDARRRAYS:
        case APIInterceptor::APIFUNCTION_GL_GLNORMALPOINTER:
        case APIInterceptor::APIFUNCTION_GL_GLSECONDARYCOLORPOINTER:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORDPOINTER:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEXPOINTER:
        {
            result = true;

            break;
        }

        /* Names and sizes that nothing follows. A list or texture name comes from the program that creates it (a list by glNewList, a texture by its
         * first image). The base of the names of glCallLists is not needed, because that call ends the process.
         */
        case APIInterceptor::APIFUNCTION_GL_GLFINISH:
        case APIInterceptor::APIFUNCTION_GL_GLFLUSH:
        case APIInterceptor::APIFUNCTION_GL_GLGENLISTS:
        case APIInterceptor::APIFUNCTION_GL_GLGENTEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLLISTBASE:
        case APIInterceptor::APIFUNCTION_GL_GLPRIORITIZETEXTURES:
        {
            result = true;

            break;
        }

        /* Rendering state that the model does not use to tell draws apart. It changes how a draw looks, not its structure (the geometry, the resources
         * and the state that the model follows). The replay still makes these calls, so the result looks as it did originally.
         */
        case APIInterceptor::APIFUNCTION_GL_GLCLAMPCOLOR:
        case APIInterceptor::APIFUNCTION_GL_GLCLEARINDEX:
        case APIInterceptor::APIFUNCTION_GL_GLCOLORMATERIAL:
        case APIInterceptor::APIFUNCTION_GL_GLDEPTHRANGE:
        case APIInterceptor::APIFUNCTION_GL_GLFOGF:
        case APIInterceptor::APIFUNCTION_GL_GLFOGFV:
        case APIInterceptor::APIFUNCTION_GL_GLFOGI:
        case APIInterceptor::APIFUNCTION_GL_GLFOGIV:
        case APIInterceptor::APIFUNCTION_GL_GLHINT:
        case APIInterceptor::APIFUNCTION_GL_GLINDEXMASK:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTF:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTI:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTMODELF:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTMODELFV:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTMODELI:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTMODELIV:
        case APIInterceptor::APIFUNCTION_GL_GLLINESTIPPLE:
        case APIInterceptor::APIFUNCTION_GL_GLMATERIALF:
        case APIInterceptor::APIFUNCTION_GL_GLMATERIALFV:
        case APIInterceptor::APIFUNCTION_GL_GLMATERIALI:
        case APIInterceptor::APIFUNCTION_GL_GLMATERIALIV:
        case APIInterceptor::APIFUNCTION_GL_GLPOINTPARAMETERF:
        case APIInterceptor::APIFUNCTION_GL_GLPOINTPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLPOINTPARAMETERI:
        case APIInterceptor::APIFUNCTION_GL_GLPOINTPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLPOLYGONOFFSET:
        case APIInterceptor::APIFUNCTION_GL_GLPOLYGONSTIPPLE:
        case APIInterceptor::APIFUNCTION_GL_GLSAMPLECOVERAGE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXENVF:
        case APIInterceptor::APIFUNCTION_GL_GLTEXENVFV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXENVI:
        case APIInterceptor::APIFUNCTION_GL_GLTEXENVIV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXGEND:
        case APIInterceptor::APIFUNCTION_GL_GLTEXGENI:
        {
            result = true;

            break;
        }

        default:
        {
            /* The queries (glGet*, glIs*, glAre*) return what the program asks for and change nothing. They are matched by a name rule, because there are about a hundred and fifty of them. */
            result = is_query(in_function);

            break;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::is_query(const APIInterceptor::APIFunction& in_function)
{
    const char* name_ptr = Capture::APIDumpNames::get_function_name(in_function);

    return (::strncmp(name_ptr, "glGet", 5) == 0 ||
            ::strncmp(name_ptr, "glIs",  4) == 0 ||
            ::strncmp(name_ptr, "glAre", 5) == 0);
}
