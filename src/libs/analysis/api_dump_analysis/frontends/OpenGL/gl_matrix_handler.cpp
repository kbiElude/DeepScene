/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_matrix_handler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_argument_reader.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_matrix_math.h"
#include "analysis/api_dump_analysis/frontends/shared/call_view.h"
#include "analysis/api_dump_analysis/frontends/shared/unsupported_use.h"

const uint32_t Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::MODELVIEW_MODE = 0x1700;

Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::GLMatrixHandler(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_builder_ptr       (in_builder_ptr),
     m_n_unfollowed_calls(0)
{
    /* Stub */
}

std::vector<Analysis::APIDumpAnalysis::Frontends::GLMatrix>* Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::get_current_stack_ptr(GLContextState* inout_state_ptr)
{
    constexpr uint32_t MODELVIEW  = 0x1700;
    constexpr uint32_t PROJECTION = 0x1701;

    std::vector<GLMatrix>* result_ptr = nullptr;

    switch (inout_state_ptr->attributes.matrix_mode)
    {
        case MODELVIEW:
        {
            result_ptr = &inout_state_ptr->modelview_f32mat_vec;

            break;
        }

        case PROJECTION:
        {
            result_ptr = &inout_state_ptr->projection_f32mat_vec;

            break;
        }

        default:
        {
            /* The texture and the color matrices. */
            result_ptr = &inout_state_ptr->other_f32mat_vec;

            break;
        }
    }

    return result_ptr;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::get_n_unfollowed_calls() const
{
    return m_n_unfollowed_calls;
}

void Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::handle_matrix(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                          const Capture::APIDumpLoader::Workload&      in_workload,
                                                                          GLContextState*                              inout_state_ptr)
{
    const bool is_identity = (in_call.func == APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY);
    const bool is_load     = (is_identity                                                       ||
                              in_call.func == APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD      ||
                              in_call.func == APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF);
    bool       has_operand = true;
    GLMatrix   operand     = GLMatrixMath::get_identity();

    if (is_identity == false)
    {
        has_operand = read_matrix_operand(in_call,
                                          in_workload,
                                          &operand);
    }

    if (has_operand == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        std::vector<GLMatrix>* stack_ptr = get_current_stack_ptr(inout_state_ptr);
        const GLMatrix         current   = stack_ptr->back();

        if (is_load)
        {
            stack_ptr->back() = operand;
        }
        else
        {
            stack_ptr->back() = GLMatrixMath::multiply(current,
                                                       operand);
        }

        /* A load replaces what the matrix was, so the camera of everything that is done to it next begins with the load: it is where a player puts the shift of an eye.
         * A push copies the matrix with its camera, a pop gives back the one of the matrix below, and what is multiplied in leaves it as it is.
         */
        if (is_load                                                  &&
            inout_state_ptr->attributes.matrix_mode == MODELVIEW_MODE)
        {
            inout_state_ptr->modelview_anchor_call_index_vec.back() = m_builder_ptr->get_call_index();
        }

        report_matrices(*inout_state_ptr);
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::handle_matrix_mode(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                               GLContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> mode_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                        &mode_array) )
    {
        inout_state_ptr->attributes.matrix_mode = mode_array.at(0);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::handle_pop_matrix(GLContextState* inout_state_ptr)
{
    std::vector<GLMatrix>* stack_ptr = get_current_stack_ptr(inout_state_ptr);

    /* A stack is never empty: the matrix at its bottom is the one that the context starts with. */
    if (stack_ptr->size() <= 1)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        stack_ptr->pop_back();

        if (inout_state_ptr->attributes.matrix_mode == MODELVIEW_MODE)
        {
            inout_state_ptr->modelview_anchor_call_index_vec.pop_back();
        }

        report_matrices(*inout_state_ptr);
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::handle_push_matrix(GLContextState* inout_state_ptr)
{
    std::vector<GLMatrix>* stack_ptr = get_current_stack_ptr(inout_state_ptr);
    const GLMatrix         current   = stack_ptr->back();

    stack_ptr->push_back(current);

    if (inout_state_ptr->attributes.matrix_mode == MODELVIEW_MODE)
    {
        inout_state_ptr->modelview_anchor_call_index_vec.push_back(inout_state_ptr->modelview_anchor_call_index_vec.back() );
    }
}

bool Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::read_matrix_chunk(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              const Capture::APIDumpLoader::Workload&      in_workload,
                                                                              const bool&                                  in_is_double,
                                                                              GLMatrix*                                    out_matrix_f32mat_ptr)
{
    constexpr size_t MATRIX_ARGUMENT = 0;

    const CallView view(in_call,
                        in_workload,
                        in_call.arg_in_vec.size());
    const Capture::APIDumpLoader::DataChunk* chunk_ptr = view.get_chunk_ptr(MATRIX_ARGUMENT);
    bool                                     result    = false;

    if (chunk_ptr != nullptr)
    {
        const size_t n_bytes = chunk_ptr->size();

        if (in_is_double == false &&
            n_bytes      == sizeof(GLMatrix) )
        {
            ::memcpy(out_matrix_f32mat_ptr->data(),
                     chunk_ptr->data(),
                     sizeof(GLMatrix) );

            result = true;
        }
        else
        if (in_is_double &&
            n_bytes      == sizeof(std::array<double, 16>) )
        {
            std::array<double, 16> double_array = {};

            ::memcpy(double_array.data(),
                     chunk_ptr->data(),
                     sizeof(double_array) );

            for (size_t n_element = 0;
                        n_element < double_array.size();
                      ++n_element)
            {
                out_matrix_f32mat_ptr->at(n_element) = static_cast<float>(double_array.at(n_element) );
            }

            result = true;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::read_matrix_operand(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                const Capture::APIDumpLoader::Workload&      in_workload,
                                                                                GLMatrix*                                    out_matrix_f32mat_ptr)
{
    bool result = false;

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLFRUSTUM:
        case APIInterceptor::APIFUNCTION_GL_GLORTHO:
        {
            std::array<double, 6> box_array = {};

            result = GLArgumentReader::read_arguments(in_call,
                                                     &box_array);

            if (result)
            {
                const bool is_frustum = (in_call.func == APIInterceptor::APIFUNCTION_GL_GLFRUSTUM);

                *out_matrix_f32mat_ptr = (is_frustum) ? GLMatrixMath::get_frustum(box_array.at(0),
                                                                                  box_array.at(1),
                                                                                  box_array.at(2),
                                                                                  box_array.at(3),
                                                                                  box_array.at(4),
                                                                                  box_array.at(5) )
                                                      : GLMatrixMath::get_ortho  (box_array.at(0),
                                                                                  box_array.at(1),
                                                                                  box_array.at(2),
                                                                                  box_array.at(3),
                                                                                  box_array.at(4),
                                                                                  box_array.at(5) );
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD:
        case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXD:
        {
            result = read_matrix_chunk(in_call,
                                       in_workload,
                                       true, /* in_is_double */
                                       out_matrix_f32mat_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF:
        case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF:
        {
            result = read_matrix_chunk(in_call,
                                       in_workload,
                                       false, /* in_is_double */
                                       out_matrix_f32mat_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLROTATED:
        case APIInterceptor::APIFUNCTION_GL_GLROTATEF:
        {
            std::array<double, 4> rotation_array = {};

            result = GLArgumentReader::read_arguments(in_call,
                                                     &rotation_array);

            if (result)
            {
                *out_matrix_f32mat_ptr = GLMatrixMath::get_rotation(rotation_array.at(0),
                                                                    rotation_array.at(1),
                                                                    rotation_array.at(2),
                                                                    rotation_array.at(3) );
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLSCALED:
        case APIInterceptor::APIFUNCTION_GL_GLSCALEF:
        case APIInterceptor::APIFUNCTION_GL_GLTRANSLATED:
        case APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF:
        {
            std::array<double, 3> vector_array = {};

            result = GLArgumentReader::read_arguments(in_call,
                                                     &vector_array);

            if (result)
            {
                const bool is_scale = (in_call.func == APIInterceptor::APIFUNCTION_GL_GLSCALED ||
                                       in_call.func == APIInterceptor::APIFUNCTION_GL_GLSCALEF);

                *out_matrix_f32mat_ptr = (is_scale) ? GLMatrixMath::get_scale      (vector_array.at(0),
                                                                                    vector_array.at(1),
                                                                                    vector_array.at(2) )
                                                    : GLMatrixMath::get_translation(vector_array.at(0),
                                                                                    vector_array.at(1),
                                                                                    vector_array.at(2) );
            }

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::report_matrices(const GLContextState& in_state)
{
    const GLMatrix& modelview  = in_state.modelview_f32mat_vec.back();
    const GLMatrix& projection = in_state.projection_f32mat_vec.back();

    report_matrix(modelview,
                  Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_VIEW_MATRIX,
                  0,
                  &m_reported_modelview);
    report_matrix(projection,
                  Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                  sizeof(GLMatrix),
                  &m_reported_projection);
}

void Analysis::APIDumpAnalysis::Frontends::GLMatrixHandler::report_matrix(const GLMatrix&                                    in_matrix_f32mat,
                                                                          const Analysis::APIDumpAnalysis::ConstantSemantic& in_semantic,
                                                                          const uint64_t&                                    in_byte_offset,
                                                                          GLReportedMatrix*                                  inout_reported_ptr)
{
    const bool is_changed = (inout_reported_ptr->has_value     == false ||
                             inout_reported_ptr->matrix_f32mat != in_matrix_f32mat);

    if (is_changed)
    {
        Analysis::APIDumpAnalysis::ConstantUpdate update;

        /* The state of the fixed-function pipeline has no memory the application can see: the front end says where the matrices are in it. */
        update.location.block_kind  = Analysis::APIDumpAnalysis::ConstantBlockKind::FIXED_FUNCTION_STATE;
        update.location.byte_offset = in_byte_offset;
        update.location.byte_size   = sizeof(GLMatrix);
        update.provenance           = Analysis::APIDumpAnalysis::ConstantProvenance::API_DEFINED;
        update.semantic             = in_semantic;

        update.value_u8_vec.resize(sizeof(GLMatrix) );

        ::memcpy(update.value_u8_vec.data(),
                 in_matrix_f32mat.data(),
                 sizeof(GLMatrix) );

        m_builder_ptr->on_update_constant(update);

        inout_reported_ptr->has_value     = true;
        inout_reported_ptr->matrix_f32mat = in_matrix_f32mat;
    }
}
