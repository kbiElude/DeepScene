/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/gl_function_classifier.h"
#include "analysis/api_dump_index/gl_state_effect_describer.h"
#include "common/argument_reader.h"
#include "common/macros.h"
#include <algorithm>

const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_CAPABILITY        = 1u << 0;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_CLIENT_ARRAY      = 1u << 1;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_CURRENT           = 1u << 2;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_FOG               = 1u << 3;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_LIGHT             = 1u << 4;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_LIGHT_MODEL       = 1u << 5;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_MATERIAL          = 1u << 6;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_PIXEL_STORE       = 1u << 7;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_POINTER           = 1u << 8;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_RASTER            = 1u << 9;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_TEXTURE_ENV       = 1u << 10;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_TEXTURE_GEN       = 1u << 11;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::DOMAIN_TEXTURE_PARAMETER = 1u << 12;

const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::SELECTOR_ACTIVE_TEXTURE        = 0;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::SELECTOR_CLIENT_ACTIVE_TEXTURE = 1;
const uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::SELECTOR_MATRIX_MODE           = 2;

Analysis::APIDumpIndex::GLStateEffectDescriber::GLStateEffectDescriber()
{
    /* Stub */
}

Analysis::APIDumpIndex::StateEffect Analysis::APIDumpIndex::GLStateEffectDescriber::describe(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    const bool was_recording = m_call_describer.is_recording();
    const auto call_class    = GLCallDescriber::get_function_class(in_call.func);

    StateEffect result;

    m_call_describer.describe(in_call);

    if (call_class == CallClass::CONTENT)
    {
        /* A dead frame does not make content calls; the ones of a list are kept, because the list is made of them. */
        AI_ASSERT_ALWAYS(was_recording,
                         "A content call of OpenGL was given to the state coalescing outside a display list.");

        result.kind = StateEffectKind::RECORDED;
    }
    else
    {
        result = classify(in_call);

        AI_ASSERT_ALWAYS(in_call.func != APIInterceptor::APIFUNCTION_GL_GLNEWLIST ||
                         was_recording == false,
                         "A display list is made while another is.");
        AI_ASSERT_ALWAYS(in_call.func != APIInterceptor::APIFUNCTION_GL_GLENDLIST ||
                         was_recording,
                         "A display list ends that was not made in the workload.");

        /* The calls of a list change no state until the list is called; the end of the list is made in place. */
        if (was_recording                                           &&
            in_call.func != APIInterceptor::APIFUNCTION_GL_GLENDLIST)
        {
            result = StateEffect();

            result.kind = StateEffectKind::RECORDED;
        }
    }

    return result;
}

Capture::APIDumpLoader::APIGroup Analysis::APIDumpIndex::GLStateEffectDescriber::get_group() const
{
    return Capture::APIDumpLoader::APIGroup::GL;
}

Analysis::APIDumpIndex::StateEffect Analysis::APIDumpIndex::GLStateEffectDescriber::classify(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    constexpr uint32_t MATRIX_SELECTOR_MASK = 1u << SELECTOR_MATRIX_MODE;
    constexpr uint32_t TEXTURE_UNIT_MASK    = 1u << SELECTOR_ACTIVE_TEXTURE;
    constexpr uint32_t CLIENT_UNIT_MASK     = 1u << SELECTOR_CLIENT_ACTIVE_TEXTURE;

    /* The values that are tested. */
    constexpr uint32_t GL_COMPILE             = 0x1300;
    constexpr uint32_t GL_POSITION            = 0x1203;
    constexpr uint32_t GL_SPOT_DIRECTION      = 0x1204;
    constexpr uint32_t GL_TEXTURE_COORD_ARRAY = 0x8078;

    /* Capabilities that are a flag of their own, whatever the active texture unit is. */
    constexpr uint32_t PLAIN_CAPABILITY_ARRAY[] =
    {
        0x0B10, /* GL_POINT_SMOOTH           */
        0x0B20, /* GL_LINE_SMOOTH            */
        0x0B24, /* GL_LINE_STIPPLE           */
        0x0B41, /* GL_POLYGON_SMOOTH         */
        0x0B42, /* GL_POLYGON_STIPPLE        */
        0x0B44, /* GL_CULL_FACE              */
        0x0B50, /* GL_LIGHTING               */
        0x0B60, /* GL_FOG                    */
        0x0B71, /* GL_DEPTH_TEST             */
        0x0B90, /* GL_STENCIL_TEST           */
        0x0BA1, /* GL_NORMALIZE              */
        0x0BC0, /* GL_ALPHA_TEST             */
        0x0BD0, /* GL_DITHER                 */
        0x0BE2, /* GL_BLEND                  */
        0x0BF2, /* GL_COLOR_LOGIC_OP         */
        0x0C11, /* GL_SCISSOR_TEST           */
        0x0D80, /* GL_AUTO_NORMAL            */
        0x2A01, /* GL_POLYGON_OFFSET_POINT   */
        0x2A02, /* GL_POLYGON_OFFSET_LINE    */
        0x3000, /* GL_CLIP_PLANE0            */
        0x3001, /* GL_CLIP_PLANE1            */
        0x3002, /* GL_CLIP_PLANE2            */
        0x3003, /* GL_CLIP_PLANE3            */
        0x3004, /* GL_CLIP_PLANE4            */
        0x3005, /* GL_CLIP_PLANE5            */
        0x4000, /* GL_LIGHT0                 */
        0x4001, /* GL_LIGHT1                 */
        0x4002, /* GL_LIGHT2                 */
        0x4003, /* GL_LIGHT3                 */
        0x4004, /* GL_LIGHT4                 */
        0x4005, /* GL_LIGHT5                 */
        0x4006, /* GL_LIGHT6                 */
        0x4007, /* GL_LIGHT7                 */
        0x8037, /* GL_POLYGON_OFFSET_FILL    */
        0x803A  /* GL_RESCALE_NORMAL         */
    };

    /* Capabilities that are a flag of each texture unit. */
    constexpr uint32_t UNIT_CAPABILITY_ARRAY[] =
    {
        0x0C60, /* GL_TEXTURE_GEN_S     */
        0x0C61, /* GL_TEXTURE_GEN_T     */
        0x0C62, /* GL_TEXTURE_GEN_R     */
        0x0C63, /* GL_TEXTURE_GEN_Q     */
        0x0DE0, /* GL_TEXTURE_1D        */
        0x0DE1, /* GL_TEXTURE_2D        */
        0x806F, /* GL_TEXTURE_3D        */
        0x8513  /* GL_TEXTURE_CUBE_MAP  */
    };

    /* Client arrays that are not those of a texture unit. */
    constexpr uint32_t PLAIN_CLIENT_ARRAY_ARRAY[] =
    {
        0x8074, /* GL_VERTEX_ARRAY           */
        0x8075, /* GL_NORMAL_ARRAY           */
        0x8076, /* GL_COLOR_ARRAY            */
        0x8077, /* GL_INDEX_ARRAY            */
        0x8079, /* GL_EDGE_FLAG_ARRAY        */
        0x8457, /* GL_FOG_COORD_ARRAY        */
        0x845E  /* GL_SECONDARY_COLOR_ARRAY  */
    };

    StateEffect result;

    /* A function that no layer has met ends the process here, whatever the table below says of it. */
    if (GLFunctionClassifier::get_kind(in_call.func) == GLFunctionKind::UNKNOWN)
    {
        AI_FAIL_UNSUPPORTED(in_call.func);
    }

    switch (in_call.func)
    {
        /* Pieces of state that a call sets to a value, whatever it was. */
        case APIInterceptor::APIFUNCTION_GL_GLALPHAFUNC:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::ALPHA_FUNC,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::BLEND_FUNC,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCLEARCOLOR:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::CLEAR_COLOR,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCLEARDEPTH:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::CLEAR_DEPTH,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCLEARSTENCIL:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::CLEAR_STENCIL,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCOLORMASK:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::COLOR_MASK,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCULLFACE:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::CULL_FACE,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::DEPTH_FUNC,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::DEPTH_MASK,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDISABLE:
        case APIInterceptor::APIFUNCTION_GL_GLENABLE:
        {
            const uint32_t capability = read_argument(in_call,
                                                      0);

            if (is_in(capability,
                      PLAIN_CAPABILITY_ARRAY,
                      sizeof(PLAIN_CAPABILITY_ARRAY) / sizeof(PLAIN_CAPABILITY_ARRAY[0]) ) )
            {
                result = make_assign(DOMAIN_CAPABILITY,
                                     Family::CAPABILITY,
                                     capability,
                                     0,
                                     0);
            }
            else
            if (is_in(capability,
                      UNIT_CAPABILITY_ARRAY,
                      sizeof(UNIT_CAPABILITY_ARRAY) / sizeof(UNIT_CAPABILITY_ARRAY[0]) ) )
            {
                result = make_assign(DOMAIN_CAPABILITY,
                                     Family::CAPABILITY,
                                     capability,
                                     0,
                                     TEXTURE_UNIT_MASK);
            }
            else
            {
                /* GL_COLOR_MATERIAL and every other one: a barrier, because what enabling it does to other state is not tracked. */
                result = make_barrier();
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDISABLECLIENTSTATE:
        case APIInterceptor::APIFUNCTION_GL_GLENABLECLIENTSTATE:
        {
            const uint32_t array = read_argument(in_call,
                                                 0);

            if (array == GL_TEXTURE_COORD_ARRAY)
            {
                result = make_assign(DOMAIN_CLIENT_ARRAY,
                                     Family::CLIENT_ARRAY,
                                     array,
                                     0,
                                     CLIENT_UNIT_MASK);
            }
            else
            if (is_in(array,
                      PLAIN_CLIENT_ARRAY_ARRAY,
                      sizeof(PLAIN_CLIENT_ARRAY_ARRAY) / sizeof(PLAIN_CLIENT_ARRAY_ARRAY[0]) ) )
            {
                result = make_assign(DOMAIN_CLIENT_ARRAY,
                                     Family::CLIENT_ARRAY,
                                     array,
                                     0,
                                     0);
            }
            else
            {
                result = make_barrier();
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLFOGF:
        case APIInterceptor::APIFUNCTION_GL_GLFOGFV:
        case APIInterceptor::APIFUNCTION_GL_GLFOGI:
        {
            const uint32_t fog_parameter = read_argument(in_call,
                                                         0);

            result = make_assign(DOMAIN_FOG,
                                 Family::FOG,
                                 fog_parameter,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLFRONTFACE:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::FRONT_FACE,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLLIGHTF:
        case APIInterceptor::APIFUNCTION_GL_GLLIGHTFV:
        {
            const uint32_t light = read_argument(in_call,
                                                 0);
            const uint32_t name  = read_argument(in_call,
                                                 1);

            if (name == GL_POSITION        ||
                name == GL_SPOT_DIRECTION)
            {
                /* The value is transformed by the matrix at the time of the call, so the call stays between the calls that move the matrix. */
                result = make_ordered(0,
                                      0);
            }
            else
            {
                result = make_assign(DOMAIN_LIGHT,
                                     Family::LIGHT,
                                     light,
                                     name,
                                     0);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLLIGHTMODELI:
        {
            const uint32_t light_model_parameter = read_argument(in_call,
                                                                 0);

            result = make_assign(DOMAIN_LIGHT_MODEL,
                                 Family::LIGHT_MODEL,
                                 light_model_parameter,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLLINEWIDTH:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::LINE_WIDTH,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLMATERIALFV:
        case APIInterceptor::APIFUNCTION_GL_GLMATERIALI:
        {
            const uint32_t face = read_argument(in_call,
                                                0);
            const uint32_t material_parameter = read_argument(in_call,
                                                              1);

            result = make_assign(DOMAIN_MATERIAL,
                                 Family::MATERIAL,
                                 face,
                                 material_parameter,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3F:
        {
            result = make_assign(DOMAIN_CURRENT,
                                 Family::NORMAL,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLNORMALPOINTER:
        {
            result = make_assign(DOMAIN_POINTER,
                                 Family::POINTER,
                                 0x8075 /* GL_NORMAL_ARRAY */,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLPIXELSTOREI:
        {
            const uint32_t pixel_store_parameter = read_argument(in_call,
                                                                 0);

            result = make_assign(DOMAIN_PIXEL_STORE,
                                 Family::PIXEL_STORE,
                                 pixel_store_parameter,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLPOLYGONMODE:
        {
            const uint32_t face = read_argument(in_call,
                                                0);

            result = make_assign(DOMAIN_RASTER,
                                 Family::POLYGON_MODE,
                                 face,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLPOLYGONOFFSET:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::POLYGON_OFFSET,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLSCISSOR:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::SCISSOR,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLSHADEMODEL:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::SHADE_MODEL,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::STENCIL_FUNC,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLSTENCILMASK:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::STENCIL_MASK,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::STENCIL_OP,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2F:
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD4F:
        {
            result = make_assign(DOMAIN_CURRENT,
                                 Family::TEXCOORD,
                                 0,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORDPOINTER:
        {
            result = make_assign(DOMAIN_POINTER,
                                 Family::POINTER,
                                 GL_TEXTURE_COORD_ARRAY,
                                 0,
                                 CLIENT_UNIT_MASK);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXENVF:
        case APIInterceptor::APIFUNCTION_GL_GLTEXENVI:
        {
            const uint32_t target = read_argument(in_call,
                                                  0);
            const uint32_t environment_parameter = read_argument(in_call,
                                                                 1);

            result = make_assign(DOMAIN_TEXTURE_ENV,
                                 Family::TEXTURE_ENV,
                                 target,
                                 environment_parameter,
                                 TEXTURE_UNIT_MASK);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXGENI:
        {
            const uint32_t coordinate = read_argument(in_call,
                                                      0);
            const uint32_t generation_parameter = read_argument(in_call,
                                                                1);

            result = make_assign(DOMAIN_TEXTURE_GEN,
                                 Family::TEXTURE_GEN,
                                 coordinate,
                                 generation_parameter,
                                 TEXTURE_UNIT_MASK);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI:
        {
            const uint32_t target = read_argument(in_call,
                                                  0);
            const uint32_t texture_parameter = read_argument(in_call,
                                                             1);

            result = make_assign(DOMAIN_TEXTURE_PARAMETER,
                                 Family::TEXTURE_PARAMETER,
                                 target,
                                 texture_parameter,
                                 TEXTURE_UNIT_MASK);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEXPOINTER:
        {
            result = make_assign(DOMAIN_POINTER,
                                 Family::POINTER,
                                 0x8074 /* GL_VERTEX_ARRAY */,
                                 0,
                                 0);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVIEWPORT:
        {
            result = make_assign(DOMAIN_RASTER,
                                 Family::VIEWPORT,
                                 0,
                                 0,
                                 0);

            break;
        }

        /* What later calls act on. */
        case APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE:
        {
            const uint32_t unit = read_argument(in_call,
                                                0);

            result = make_select(SELECTOR_ACTIVE_TEXTURE,
                                 unit);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCLIENTACTIVETEXTURE:
        {
            const uint32_t unit = read_argument(in_call,
                                                0);

            result = make_select(SELECTOR_CLIENT_ACTIVE_TEXTURE,
                                 unit);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE:
        {
            const uint32_t mode = read_argument(in_call,
                                                0);

            result = make_select(SELECTOR_MATRIX_MODE,
                                 mode);

            break;
        }

        /* Calls that are made in place: the matrices, which depend on the matrix mode and on each other. */
        case APIInterceptor::APIFUNCTION_GL_GLFRUSTUM:
        case APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY:
        case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXD:
        case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF:
        case APIInterceptor::APIFUNCTION_GL_GLORTHO:
        case APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX:
        case APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX:
        case APIInterceptor::APIFUNCTION_GL_GLROTATEF:
        case APIInterceptor::APIFUNCTION_GL_GLSCALEF:
        case APIInterceptor::APIFUNCTION_GL_GLTRANSLATED:
        case APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF:
        {
            result = make_ordered(0,
                                  MATRIX_SELECTOR_MASK);

            break;
        }

        /* A bound texture takes the parameters that were set before the bind. */
        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
        {
            result = make_ordered(DOMAIN_TEXTURE_PARAMETER,
                                  TEXTURE_UNIT_MASK);

            break;
        }

        /* Calls that read and set none of the state above. */
        case APIInterceptor::APIFUNCTION_GL_GLGENLISTS:
        case APIInterceptor::APIFUNCTION_GL_GLGENTEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLGETBOOLEANV:
        case APIInterceptor::APIFUNCTION_GL_GLGETERROR:
        case APIInterceptor::APIFUNCTION_GL_GLGETFLOATV:
        case APIInterceptor::APIFUNCTION_GL_GLGETINTEGERV:
        case APIInterceptor::APIFUNCTION_GL_GLGETSTRING:
        {
            result = make_ordered(0,
                                  0);

            break;
        }

        /* Calls that may read or set anything. glColor sets the material too when GL_COLOR_MATERIAL is on. */
        case APIInterceptor::APIFUNCTION_GL_GLACCUM:
        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3F:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3FV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3UB:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4F:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4FV:
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4UB:
        case APIInterceptor::APIFUNCTION_GL_GLDELETELISTS:
        case APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLENDLIST:
        case APIInterceptor::APIFUNCTION_GL_GLFLUSH:
        case APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB:
        case APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE2D:
        {
            result = make_barrier();

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
        {
            const uint32_t mode = read_argument(in_call,
                                                1);

            /* The calls of a list that is made and executed at once would change state that is tracked here. */
            AI_ASSERT_ALWAYS(mode == GL_COMPILE,
                             "glNewList(GL_COMPILE_AND_EXECUTE) is not supported by the state coalescing.");

            result = make_barrier();

            break;
        }

        default:
        {
            /* A known function that has no entry of its own above. */
            result = make_generic_effect(in_call.func);

            break;
        }
    }

    return result;
}

bool Analysis::APIDumpIndex::GLStateEffectDescriber::is_in(const uint32_t& in_value,
                                                           const uint32_t* in_value_ptr,
                                                           const size_t&   in_n_values)
{
    return (std::find(in_value_ptr,
                      in_value_ptr + in_n_values,
                      in_value) != in_value_ptr + in_n_values);
}

Analysis::APIDumpIndex::StateEffect Analysis::APIDumpIndex::GLStateEffectDescriber::make_assign(const uint32_t& in_domain,
                                                                                                const Family&   in_family,
                                                                                                const uint32_t& in_first,
                                                                                                const uint32_t& in_second,
                                                                                                const uint32_t& in_selector_mask)
{
    StateEffect result;

    result.domain_mask   = in_domain;
    result.key           = {{static_cast<uint32_t>(in_family), in_first, in_second, 0}};
    result.kind          = StateEffectKind::ASSIGN;
    result.selector_mask = in_selector_mask;

    return result;
}

Analysis::APIDumpIndex::StateEffect Analysis::APIDumpIndex::GLStateEffectDescriber::make_barrier()
{
    StateEffect result;

    result.kind = StateEffectKind::BARRIER;

    return result;
}

Analysis::APIDumpIndex::StateEffect Analysis::APIDumpIndex::GLStateEffectDescriber::make_generic_effect(const APIInterceptor::APIFunction& in_function)
{
    StateEffect result;

    switch (GLFunctionClassifier::get_kind(in_function))
    {
        case GLFunctionKind::NAME_OR_SIZE:
        case GLFunctionKind::QUERY:
        {
            /* They set no state that is tracked, and a replay does not use what a query returns (it is made into memory that the replay discards). */
            result = make_ordered(0,
                                  0);

            break;
        }

        default:
        {
            /* It may read or set anything, so it is a barrier: made in place, and nothing moves across it. */
            result = make_barrier();

            break;
        }
    }

    return result;
}

Analysis::APIDumpIndex::StateEffect Analysis::APIDumpIndex::GLStateEffectDescriber::make_ordered(const uint32_t& in_domain_mask,
                                                                                                 const uint32_t& in_selector_mask)
{
    StateEffect result;

    result.domain_mask   = in_domain_mask;
    result.kind          = StateEffectKind::ORDERED;
    result.selector_mask = in_selector_mask;

    return result;
}

Analysis::APIDumpIndex::StateEffect Analysis::APIDumpIndex::GLStateEffectDescriber::make_select(const uint32_t& in_selector,
                                                                                                const uint32_t& in_value)
{
    StateEffect result;

    result.kind           = StateEffectKind::SELECT;
    result.selector       = in_selector;
    result.selector_value = in_value;

    return result;
}

uint32_t Analysis::APIDumpIndex::GLStateEffectDescriber::read_argument(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       const size_t&        in_index)
{
    uint32_t value  = 0;
    bool     is_set = false;

    if (in_index < in_call.arg_in_vec.size() )
    {
        is_set = APIInterceptor::read_number(in_call.arg_in_vec.at(in_index),
                                            &value);
    }

    AI_ASSERT_ALWAYS(is_set,
                     "A call of OpenGL has no number where the state coalescing needs one to tell what the call sets.");

    return value;
}
