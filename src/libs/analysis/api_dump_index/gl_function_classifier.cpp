/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Generated once from the groups that were kept in several places (the front end policy, its unsupported sets, the call describer and the table of the state
 * coalescing), and kept by hand from here on: the one place that says what a GL function is.
 */
#include "analysis/api_dump_index/gl_function_classifier.h"

Analysis::APIDumpIndex::GLFunctionInfo Analysis::APIDumpIndex::GLFunctionClassifier::classify(const APIInterceptor::APIFunction& in_function)
{
    GLFunctionInfo result;

    result.kind    = get_kind(in_function);
    result.support = get_support(in_function);

    return result;
}

Analysis::APIDumpIndex::GLFunctionKind Analysis::APIDumpIndex::GLFunctionClassifier::get_kind(const APIInterceptor::APIFunction& in_function)
{
    GLFunctionKind result = GLFunctionKind::UNKNOWN;

    switch (in_function)
    {
        /* The position or direction of a light, a clip plane, the planes for generating texture coordinates (the forms that take an array) and the raster position:
         * OpenGL converts them with the model-view matrix that is current at the moment of the call. */
        case APIInterceptor::APIFUNCTION_GL_GLCLIPPLANE:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTFV:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTIV:
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
        case APIInterceptor::APIFUNCTION_GL_GLTEXGENDV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXGENIV:
        {
            result = GLFunctionKind::BAKES_CURRENT_MATRIX;

            break;
        }

        /* The state of the arrays that a draw reads: where they are, and which are enabled. */
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
            result = GLFunctionKind::CLIENT_ARRAY_STATE;

            break;
        }

        /* Content or state, by what the call refers to: a call of a display list, or of lists by their names. */
        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
        {
            result = GLFunctionKind::CONDITIONAL;

            break;
        }

        /* Draws, clears, copies and the writes and reads of pixels: what a frame is made of. */
        case APIInterceptor::APIFUNCTION_GL_GLARRAYELEMENT:
        case APIInterceptor::APIFUNCTION_GL_GLBEGIN:
        case APIInterceptor::APIFUNCTION_GL_GLBITMAP:
        case APIInterceptor::APIFUNCTION_GL_GLCLEAR:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWRANGEELEMENTS:
        case APIInterceptor::APIFUNCTION_GL_GLEND:
        case APIInterceptor::APIFUNCTION_GL_GLMULTIDRAWARRAYS:
        case APIInterceptor::APIFUNCTION_GL_GLMULTIDRAWELEMENTS:
        case APIInterceptor::APIFUNCTION_GL_GLREADPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLRECTD:
        case APIInterceptor::APIFUNCTION_GL_GLRECTDV:
        case APIInterceptor::APIFUNCTION_GL_GLRECTF:
        case APIInterceptor::APIFUNCTION_GL_GLRECTFV:
        case APIInterceptor::APIFUNCTION_GL_GLRECTI:
        case APIInterceptor::APIFUNCTION_GL_GLRECTIV:
        case APIInterceptor::APIFUNCTION_GL_GLRECTS:
        case APIInterceptor::APIFUNCTION_GL_GLRECTSV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2D:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2DV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2F:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2FV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2I:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2IV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2S:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2SV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3D:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3DV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3F:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3I:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3IV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3S:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3SV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4D:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4DV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4F:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4FV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4I:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4IV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4S:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4SV:
        {
            result = GLFunctionKind::CONTENT;

            break;
        }

        /* Values for the vertices that follow: a color, a normal, a texture coordinate, an index, a flag, a position in the window. */
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
            result = GLFunctionKind::CURRENT_VALUE;

            break;
        }

        /* Names and sizes of objects, and the calls that only wait: nothing about a draw. */
        case APIInterceptor::APIFUNCTION_GL_GLFINISH:
        case APIInterceptor::APIFUNCTION_GL_GLFLUSH:
        case APIInterceptor::APIFUNCTION_GL_GLGENLISTS:
        case APIInterceptor::APIFUNCTION_GL_GLGENTEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLLISTBASE:
        case APIInterceptor::APIFUNCTION_GL_GLPRIORITIZETEXTURES:
        {
            result = GLFunctionKind::NAME_OR_SIZE;

            break;
        }

        /* The queries (glGet*, glIs*, glAre*): they return what the program asks for and change nothing. */
        case APIInterceptor::APIFUNCTION_GL_GLARETEXTURESRESIDENT:
        case APIInterceptor::APIFUNCTION_GL_GLGETACTIVEATOMICCOUNTERBUFFERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETACTIVEATTRIB:
        case APIInterceptor::APIFUNCTION_GL_GLGETACTIVESUBROUTINENAME:
        case APIInterceptor::APIFUNCTION_GL_GLGETACTIVESUBROUTINEUNIFORMIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETACTIVESUBROUTINEUNIFORMNAME:
        case APIInterceptor::APIFUNCTION_GL_GLGETACTIVEUNIFORM:
        case APIInterceptor::APIFUNCTION_GL_GLGETACTIVEUNIFORMBLOCKIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETACTIVEUNIFORMBLOCKNAME:
        case APIInterceptor::APIFUNCTION_GL_GLGETACTIVEUNIFORMNAME:
        case APIInterceptor::APIFUNCTION_GL_GLGETACTIVEUNIFORMSIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETATTACHEDSHADERS:
        case APIInterceptor::APIFUNCTION_GL_GLGETATTRIBLOCATION:
        case APIInterceptor::APIFUNCTION_GL_GLGETBOOLEANI_V:
        case APIInterceptor::APIFUNCTION_GL_GLGETBOOLEANV:
        case APIInterceptor::APIFUNCTION_GL_GLGETBUFFERPARAMETERI64V:
        case APIInterceptor::APIFUNCTION_GL_GLGETBUFFERPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETBUFFERPOINTERV:
        case APIInterceptor::APIFUNCTION_GL_GLGETBUFFERSUBDATA:
        case APIInterceptor::APIFUNCTION_GL_GLGETCLIPPLANE:
        case APIInterceptor::APIFUNCTION_GL_GLGETCOMPRESSEDTEXIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLGETCOMPRESSEDTEXTUREIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLGETCOMPRESSEDTEXTURESUBIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLGETDEBUGMESSAGELOG:
        case APIInterceptor::APIFUNCTION_GL_GLGETDOUBLEI_V:
        case APIInterceptor::APIFUNCTION_GL_GLGETDOUBLEV:
        case APIInterceptor::APIFUNCTION_GL_GLGETERROR:
        case APIInterceptor::APIFUNCTION_GL_GLGETFLOATI_V:
        case APIInterceptor::APIFUNCTION_GL_GLGETFLOATV:
        case APIInterceptor::APIFUNCTION_GL_GLGETFRAGDATAINDEX:
        case APIInterceptor::APIFUNCTION_GL_GLGETFRAGDATALOCATION:
        case APIInterceptor::APIFUNCTION_GL_GLGETFRAMEBUFFERATTACHMENTPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETFRAMEBUFFERPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETGRAPHICSRESETSTATUS:
        case APIInterceptor::APIFUNCTION_GL_GLGETINTEGER64I_V:
        case APIInterceptor::APIFUNCTION_GL_GLGETINTEGER64V:
        case APIInterceptor::APIFUNCTION_GL_GLGETINTEGERI_V:
        case APIInterceptor::APIFUNCTION_GL_GLGETINTEGERV:
        case APIInterceptor::APIFUNCTION_GL_GLGETINTERNALFORMATI64V:
        case APIInterceptor::APIFUNCTION_GL_GLGETINTERNALFORMATIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETLIGHTFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETLIGHTIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETMAPDV:
        case APIInterceptor::APIFUNCTION_GL_GLGETMAPFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETMAPIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETMATERIALFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETMATERIALIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETMULTISAMPLEFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETNAMEDBUFFERPARAMETERI64V:
        case APIInterceptor::APIFUNCTION_GL_GLGETNAMEDBUFFERPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETNAMEDBUFFERPOINTERV:
        case APIInterceptor::APIFUNCTION_GL_GLGETNAMEDBUFFERSUBDATA:
        case APIInterceptor::APIFUNCTION_GL_GLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETNAMEDFRAMEBUFFERPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETNAMEDRENDERBUFFERPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETNCOMPRESSEDTEXIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLGETNTEXIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLGETNUNIFORMDV:
        case APIInterceptor::APIFUNCTION_GL_GLGETNUNIFORMFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETNUNIFORMIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETNUNIFORMUIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETOBJECTLABEL:
        case APIInterceptor::APIFUNCTION_GL_GLGETOBJECTPTRLABEL:
        case APIInterceptor::APIFUNCTION_GL_GLGETPIXELMAPFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETPIXELMAPUIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETPIXELMAPUSV:
        case APIInterceptor::APIFUNCTION_GL_GLGETPOINTERV:
        case APIInterceptor::APIFUNCTION_GL_GLGETPOLYGONSTIPPLE:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMBINARY:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMINFOLOG:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMINTERFACEIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMPIPELINEINFOLOG:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMPIPELINEIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMRESOURCEINDEX:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMRESOURCEIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMRESOURCELOCATION:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMRESOURCELOCATIONINDEX:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMRESOURCENAME:
        case APIInterceptor::APIFUNCTION_GL_GLGETPROGRAMSTAGEIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETQUERYBUFFEROBJECTI64V:
        case APIInterceptor::APIFUNCTION_GL_GLGETQUERYBUFFEROBJECTIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETQUERYBUFFEROBJECTUI64V:
        case APIInterceptor::APIFUNCTION_GL_GLGETQUERYBUFFEROBJECTUIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETQUERYINDEXEDIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETQUERYIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETQUERYOBJECTI64V:
        case APIInterceptor::APIFUNCTION_GL_GLGETQUERYOBJECTIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETQUERYOBJECTUI64V:
        case APIInterceptor::APIFUNCTION_GL_GLGETQUERYOBJECTUIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETRENDERBUFFERPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETSAMPLERPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETSAMPLERPARAMETERIIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETSAMPLERPARAMETERIUIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETSAMPLERPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETSHADERINFOLOG:
        case APIInterceptor::APIFUNCTION_GL_GLGETSHADERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETSHADERPRECISIONFORMAT:
        case APIInterceptor::APIFUNCTION_GL_GLGETSHADERSOURCE:
        case APIInterceptor::APIFUNCTION_GL_GLGETSTRING:
        case APIInterceptor::APIFUNCTION_GL_GLGETSTRINGI:
        case APIInterceptor::APIFUNCTION_GL_GLGETSUBROUTINEINDEX:
        case APIInterceptor::APIFUNCTION_GL_GLGETSUBROUTINEUNIFORMLOCATION:
        case APIInterceptor::APIFUNCTION_GL_GLGETSYNCIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXENVFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXENVIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXGENDV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXGENIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXLEVELPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXLEVELPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXPARAMETERI_IV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXPARAMETERI_UIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXTUREIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXTURELEVELPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXTURELEVELPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXTUREPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXTUREPARAMETERIIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXTUREPARAMETERIUIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXTUREPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXTURESUBIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLGETTRANSFORMFEEDBACKI64_V:
        case APIInterceptor::APIFUNCTION_GL_GLGETTRANSFORMFEEDBACKIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTRANSFORMFEEDBACKI_V:
        case APIInterceptor::APIFUNCTION_GL_GLGETTRANSFORMFEEDBACKVARYING:
        case APIInterceptor::APIFUNCTION_GL_GLGETUNIFORMBLOCKINDEX:
        case APIInterceptor::APIFUNCTION_GL_GLGETUNIFORMDV:
        case APIInterceptor::APIFUNCTION_GL_GLGETUNIFORMFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETUNIFORMINDICES:
        case APIInterceptor::APIFUNCTION_GL_GLGETUNIFORMIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETUNIFORMLOCATION:
        case APIInterceptor::APIFUNCTION_GL_GLGETUNIFORMSUBROUTINEUIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETUNIFORMUIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETVERTEXARRAYINDEXED64IV:
        case APIInterceptor::APIFUNCTION_GL_GLGETVERTEXARRAYINDEXEDIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETVERTEXARRAYIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETVERTEXATTRIBDV:
        case APIInterceptor::APIFUNCTION_GL_GLGETVERTEXATTRIBFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETVERTEXATTRIBIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETVERTEXATTRIBI_IV:
        case APIInterceptor::APIFUNCTION_GL_GLGETVERTEXATTRIBI_UIV:
        case APIInterceptor::APIFUNCTION_GL_GLGETVERTEXATTRIBLDV:
        case APIInterceptor::APIFUNCTION_GL_GLGETVERTEXATTRIBPOINTERV:
        case APIInterceptor::APIFUNCTION_GL_GLISBUFFER:
        case APIInterceptor::APIFUNCTION_GL_GLISENABLED:
        case APIInterceptor::APIFUNCTION_GL_GLISENABLEDI:
        case APIInterceptor::APIFUNCTION_GL_GLISFRAMEBUFFER:
        case APIInterceptor::APIFUNCTION_GL_GLISLIST:
        case APIInterceptor::APIFUNCTION_GL_GLISPROGRAM:
        case APIInterceptor::APIFUNCTION_GL_GLISPROGRAMPIPELINE:
        case APIInterceptor::APIFUNCTION_GL_GLISQUERY:
        case APIInterceptor::APIFUNCTION_GL_GLISRENDERBUFFER:
        case APIInterceptor::APIFUNCTION_GL_GLISSAMPLER:
        case APIInterceptor::APIFUNCTION_GL_GLISSHADER:
        case APIInterceptor::APIFUNCTION_GL_GLISSYNC:
        case APIInterceptor::APIFUNCTION_GL_GLISTEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLISTRANSFORMFEEDBACK:
        case APIInterceptor::APIFUNCTION_GL_GLISVERTEXARRAY:
        {
            result = GLFunctionKind::QUERY;

            break;
        }

        /* Rendering state that the model does not use to tell draws apart: it changes how a draw looks, not its structure. */
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
            result = GLFunctionKind::RENDER_STATE_UNTRACKED;

            break;
        }

        /* State calls that are followed (the front end has a handler, or the state coalescing an entry) and the ones that the analysis refuses (see get_support()). */
        case APIInterceptor::APIFUNCTION_GL_GLACCUM:
        case APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLALPHAFUNC:
        case APIInterceptor::APIFUNCTION_GL_GLBINDBUFFER:
        case APIInterceptor::APIFUNCTION_GL_GLBINDIMAGETEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLBINDIMAGETEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTUREUNIT:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDCOLOR:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDEQUATION:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDEQUATIONSEPARATE:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNCSEPARATE:
        case APIInterceptor::APIFUNCTION_GL_GLCALLLISTS:
        case APIInterceptor::APIFUNCTION_GL_GLCLEARCOLOR:
        case APIInterceptor::APIFUNCTION_GL_GLCLEARDEPTH:
        case APIInterceptor::APIFUNCTION_GL_GLCLEARSTENCIL:
        case APIInterceptor::APIFUNCTION_GL_GLCLEARTEXIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLCLEARTEXSUBIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLCOLORMASK:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXSUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXSUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXSUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXTURESUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXTURESUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXTURESUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYIMAGESUBDATA:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXTURESUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXTURESUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXTURESUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCREATETEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLCULLFACE:
        case APIInterceptor::APIFUNCTION_GL_GLDELETELISTS:
        case APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC:
        case APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK:
        case APIInterceptor::APIFUNCTION_GL_GLDISABLE:
        case APIInterceptor::APIFUNCTION_GL_GLENABLE:
        case APIInterceptor::APIFUNCTION_GL_GLENDLIST:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE1D:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE2D:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE3D:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURELAYER:
        case APIInterceptor::APIFUNCTION_GL_GLFRONTFACE:
        case APIInterceptor::APIFUNCTION_GL_GLFRUSTUM:
        case APIInterceptor::APIFUNCTION_GL_GLGENERATEMIPMAP:
        case APIInterceptor::APIFUNCTION_GL_GLGENERATETEXTUREMIPMAP:
        case APIInterceptor::APIFUNCTION_GL_GLINVALIDATETEXIMAGEPROC:
        case APIInterceptor::APIFUNCTION_GL_GLINVALIDATETEXSUBIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLLINEWIDTH:
        case APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY:
        case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD:
        case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF:
        case APIInterceptor::APIFUNCTION_GL_GLLOGICOP:
        case APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE:
        case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXD:
        case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF:
        case APIInterceptor::APIFUNCTION_GL_GLNAMEDFRAMEBUFFERTEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLNAMEDFRAMEBUFFERTEXTURELAYER:
        case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
        case APIInterceptor::APIFUNCTION_GL_GLORTHO:
        case APIInterceptor::APIFUNCTION_GL_GLPIXELSTOREF:
        case APIInterceptor::APIFUNCTION_GL_GLPIXELSTOREI:
        case APIInterceptor::APIFUNCTION_GL_GLPOINTSIZE:
        case APIInterceptor::APIFUNCTION_GL_GLPOLYGONMODE:
        case APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB:
        case APIInterceptor::APIFUNCTION_GL_GLPOPCLIENTATTRIB:
        case APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX:
        case APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB:
        case APIInterceptor::APIFUNCTION_GL_GLPUSHCLIENTATTRIB:
        case APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX:
        case APIInterceptor::APIFUNCTION_GL_GLROTATED:
        case APIInterceptor::APIFUNCTION_GL_GLROTATEF:
        case APIInterceptor::APIFUNCTION_GL_GLSCALED:
        case APIInterceptor::APIFUNCTION_GL_GLSCALEF:
        case APIInterceptor::APIFUNCTION_GL_GLSCISSOR:
        case APIInterceptor::APIFUNCTION_GL_GLSHADEMODEL:
        case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
        case APIInterceptor::APIFUNCTION_GL_GLSTENCILMASK:
        case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
        case APIInterceptor::APIFUNCTION_GL_GLTEXBUFFER:
        case APIInterceptor::APIFUNCTION_GL_GLTEXBUFFERRANGE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2DMULTISMAPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE3DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE3DMULTISMAPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERF:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE2DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE3DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREBUFFER:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREBUFFERRANGE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE2DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE3DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREVIEW:
        case APIInterceptor::APIFUNCTION_GL_GLTRANSLATED:
        case APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF:
        case APIInterceptor::APIFUNCTION_GL_GLVIEWPORT:
        {
            result = GLFunctionKind::STATE;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

Analysis::APIDumpIndex::GLFunctionSupport Analysis::APIDumpIndex::GLFunctionClassifier::get_support(const APIInterceptor::APIFunction& in_function)
{
    GLFunctionSupport result = GLFunctionSupport::SUPPORTED;

    switch (in_function)
    {
        /* State that decides how a draw combines with the framebuffer, and that the model does not follow: the logic op, the blend equation, the separate blend factors and the blend color. */
        case APIInterceptor::APIFUNCTION_GL_GLBLENDCOLOR:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDEQUATION:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDEQUATIONSEPARATE:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNCSEPARATE:
        case APIInterceptor::APIFUNCTION_GL_GLLOGICOP:
        {
            result = GLFunctionSupport::UNSUPPORTED_DRAW_WRITE_STATE;

            break;
        }

        /* The framebuffer is written from somewhere other than a draw, or lists named by an array are called. */
        case APIInterceptor::APIFUNCTION_GL_GLBITMAP:
        case APIInterceptor::APIFUNCTION_GL_GLCALLLISTS:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWPIXELS:
        {
            result = GLFunctionSupport::UNSUPPORTED_FRAMEBUFFER_WRITE;

            break;
        }

        /* Calls that create or change a texture that is not a GL_TEXTURE_2D one, or change one in a way that is not followed (compressed, immutable, multisampled, 1D and 3D, buffer textures, views, images, framebuffer attachments, access by name, clear, copy, invalidate). */
        case APIInterceptor::APIFUNCTION_GL_GLBINDIMAGETEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLBINDIMAGETEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTUREUNIT:
        case APIInterceptor::APIFUNCTION_GL_GLCLEARTEXIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLCLEARTEXSUBIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXSUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXSUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXSUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXTURESUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXTURESUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXTURESUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYIMAGESUBDATA:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXTURESUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXTURESUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXTURESUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCREATETEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE1D:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE2D:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE3D:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURELAYER:
        case APIInterceptor::APIFUNCTION_GL_GLGENERATETEXTUREMIPMAP:
        case APIInterceptor::APIFUNCTION_GL_GLINVALIDATETEXIMAGEPROC:
        case APIInterceptor::APIFUNCTION_GL_GLINVALIDATETEXSUBIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLNAMEDFRAMEBUFFERTEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLNAMEDFRAMEBUFFERTEXTURELAYER:
        case APIInterceptor::APIFUNCTION_GL_GLTEXBUFFER:
        case APIInterceptor::APIFUNCTION_GL_GLTEXBUFFERRANGE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2DMULTISMAPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE3DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE3DMULTISMAPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE2DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE3DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREBUFFER:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREBUFFERRANGE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE2DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE3DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREVIEW:
        {
            result = GLFunctionSupport::UNSUPPORTED_TEXTURE;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}
