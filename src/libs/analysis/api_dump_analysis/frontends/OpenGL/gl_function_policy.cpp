/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Generated from the list of the GL functions that the front end does not name, then read and kept by hand: the groups and their reasons are what matters.
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_function_policy.h"
#include "capture/api_dump_names/function_names.h"
#include <string.h>

bool Analysis::APIDumpAnalysis::Frontends::GlFunctionPolicy::bakes_current_matrix(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    /* A position or a direction of a light, a clip plane, the planes of the generation of texture coordinates (the forms that take an array: the others choose a mode) and the
     * raster position are given in the coordinates of the program, and OpenGL turns them into those of the eye with the model-view matrix of the moment of the call.
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

bool Analysis::APIDumpAnalysis::Frontends::GlFunctionPolicy::changes_what_a_draw_writes(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    /* The stencil test and what it does, the logic op, what blending does with the values (the equation, the separate factors, the constant color), and which side
     * of a face is the front (which decides which draws reach the framebuffer, as the culling that is followed does). The model follows the functions of blending
     * and of culling that are the first of their kind, and the draws that are told apart by them; these are the rest of them.
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

bool Analysis::APIDumpAnalysis::Frontends::GlFunctionPolicy::is_left_out_by_design(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    switch (in_function)
    {
        /* Values for the vertices that follow: The model builds the geometry of a draw from the positions of its vertices alone, so what else a vertex has (a color, a normal, a texture coordinate) is not a part of its structure; the replay makes the calls. */
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

        /* State of the arrays that a draw reads: A draw of arrays comes with a snapshot of what it reads (see GlArrayDrawDescriber), so where the arrays are and which are on is not needed. */
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

        /* Names and sizes that nothing follows: The name of a list or a texture is taken from the program that makes it (the list by glNewList, the texture by its first image); the base of the names of glCallLists has no use, as that call ends the process. */
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

        /* State of the rendering that the model does not tell the draws apart by: It changes how a draw looks, not what its structure is (the geometry, the resources, the state that the model follows); the replay makes the calls, so what is shown is as it was. */
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
            /* The queries that return what the program asks for and change nothing (glGet*, glIs*, glAre*): a name rule, since there are a hundred and fifty. */
            result = is_query(in_function);

            break;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GlFunctionPolicy::is_query(const APIInterceptor::APIFunction& in_function)
{
    const char* name_ptr = Capture::APIDumpNames::get_function_name(in_function);

    return (::strncmp(name_ptr, "glGet", 5) == 0 ||
            ::strncmp(name_ptr, "glIs",  4) == 0 ||
            ::strncmp(name_ptr, "glAre", 5) == 0);
}
