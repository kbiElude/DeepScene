/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Core/D3D7Utils/vertex_format.h"
#include "call_context.h"
#include "d3d7_screen_parallax.h"
#include "frame_buffer.h"
#include <algorithm>
#include <math.h>
#include <string.h>

float Stereo::Replay::EngineDLL::D3D7ScreenParallax::get_shift(const Parameters& in_parameters,
                                                               const float&      in_reciprocal_depth)
{
    /* A vertex behind the camera (a reciprocal of the depth that is not above zero) is no place to have a picture of, and neither is one at the
     * camera (a reciprocal that is huge, or not a number): they are put at the edge of what there is, which is infinity at one end, and a tenth
     * of the convergence distance at the other.
     */
    constexpr float DEGREES_TO_RADIANS = 3.14159265358979323846f / 180.0f;
    constexpr float MAX_DEPTH_FRACTION = 0.1f;
    constexpr float HALF               = 0.5f;

    const float convergence_reciprocal = 1.0f / in_parameters.convergence_distance;
    const float max_reciprocal         = convergence_reciprocal / MAX_DEPTH_FRACTION;
    const float reciprocal             = (in_reciprocal_depth == in_reciprocal_depth) ? std::min(std::max(in_reciprocal_depth,
                                                                                                          0.0f),
                                                                                                 max_reciprocal)
                                                                                      : 0.0f; /* not a number */
    const float focal_length_pixels    = (in_parameters.picture_width * HALF) / ::tanf(in_parameters.field_of_view_degrees * HALF * DEGREES_TO_RADIANS);
    const float magnitude              = focal_length_pixels * (in_parameters.eye_separation * HALF) * (reciprocal - convergence_reciprocal);

    /* A camera that is moved to the left sees what is near further to the right. */
    return (in_parameters.eye == Analysis::RecipeLoader::Eye::LEFT) ? magnitude
                                                                    : -magnitude;
}

bool Stereo::Replay::EngineDLL::D3D7ScreenParallax::rewrite(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                            const IChunkSource&                          in_chunk_source,
                                                            const Parameters&                            in_parameters,
                                                            OverlayChunkSource*                          inout_overlay_ptr,
                                                            Capture::APIDumpLoader::DumpedAPICall*       out_call_ptr)
{
    bool result = false;

    if (in_parameters.convergence_distance  >  0.0f &&
        in_parameters.eye_separation        != 0.0f &&
        in_parameters.picture_width         >  0.0f)
    {
        result = rewrite_vertices(in_call,
                                  in_chunk_source,
                                  [&in_parameters](const uint32_t& in_vertex_n_bytes,
                                                   const size_t&   in_n_vertices,
                                                   uint8_t*        inout_vertex_ptr)
                                  {
                                      shift_vertices(in_parameters,
                                                     in_vertex_n_bytes,
                                                     in_n_vertices,
                                                     inout_vertex_ptr);
                                  },
                                  inout_overlay_ptr,
                                  out_call_ptr);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7ScreenParallax::rewrite_by(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                               const IChunkSource&                          in_chunk_source,
                                                               const float&                                 in_shift,
                                                               OverlayChunkSource*                          inout_overlay_ptr,
                                                               Capture::APIDumpLoader::DumpedAPICall*       out_call_ptr)
{
    bool result = false;

    if (in_shift != 0.0f)
    {
        result = rewrite_vertices(in_call,
                                  in_chunk_source,
                                  [in_shift](const uint32_t& in_vertex_n_bytes,
                                             const size_t&   in_n_vertices,
                                             uint8_t*        inout_vertex_ptr)
                                  {
                                      constexpr size_t X_OFFSET = 0;

                                      for (size_t n_vertex = 0;
                                                  n_vertex < in_n_vertices;
                                                ++n_vertex)
                                      {
                                          float x = 0.0f;

                                          ::memcpy(&x,
                                                   inout_vertex_ptr + n_vertex * in_vertex_n_bytes + X_OFFSET,
                                                   sizeof(x) );

                                          x += in_shift;

                                          ::memcpy(inout_vertex_ptr + n_vertex * in_vertex_n_bytes + X_OFFSET,
                                                   &x,
                                                   sizeof(x) );
                                      }
                                  },
                                  inout_overlay_ptr,
                                  out_call_ptr);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7ScreenParallax::rewrite_vertices(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                     const IChunkSource&                          in_chunk_source,
                                                                     const ShiftFunc&                             in_shift_func,
                                                                     OverlayChunkSource*                          inout_overlay_ptr,
                                                                     Capture::APIDumpLoader::DumpedAPICall*       out_call_ptr)
{
    constexpr uint32_t NEW_VERTICES_CHUNK_ID = 0xfffffff1; /* no chunk of a workload has an ID this big */
    constexpr size_t   FVF_ARGUMENT          = 2;          /* of both draws */
    constexpr size_t   N_VERTICES_ARGUMENT   = 4;          /* of both draws */

    const Capture::APIDumpLoader::DataChunk* chunk_ptr         = nullptr;
    uint32_t                                 fvf               = 0;
    size_t                                   n_vertices        = 0;
    size_t                                   n_vertices_arg    = 0;
    size_t                                   n_vertices_chunk  = 0;
    size_t                                   n_argument        = 0;
    bool                                     result            = false;
    uint32_t                                 vertex_n_bytes    = 0;
    uint32_t                                 value             = 0;

    if (in_call.func != APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE &&
        in_call.func != APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE)
    {
        goto end;
    }

    if (in_call.arg_in_vec.size() <= N_VERTICES_ARGUMENT                           ||
        read_number(in_call.arg_in_vec.at(FVF_ARGUMENT),
                   &fvf)                                                == false   ||
        read_number(in_call.arg_in_vec.at(N_VERTICES_ARGUMENT),
                   &value)                                              == false)
    {
        goto end;
    }

    /* Vertices which a matrix moves are the matrix's. */
    if ((fvf & Interception::Core::D3D7Utils::D3DFVF_POSITION_MASK) != Interception::Core::D3D7Utils::D3DFVF_XYZRHW)
    {
        goto end;
    }

    n_vertices     = value;
    vertex_n_bytes = Interception::Core::D3D7Utils::VertexFormat::get_n_vertex_bytes(fvf);

    /* The vertices are the last data chunk of the call (the indices of an indexed draw come before them). */
    for (size_t n_candidate = 0;
                n_candidate < in_call.arg_in_vec.size();
              ++n_candidate)
    {
        if (in_call.arg_in_vec.at(n_candidate).type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
        {
            n_argument = n_candidate;
        }
    }

    if (n_argument == 0 ||
        vertex_n_bytes == 0)
    {
        goto end;
    }

    chunk_ptr = in_chunk_source.get_data_chunk_ptr(in_call.arg_in_vec.at(n_argument).get_data_chunk_id() );

    /* What the call says there are must be there. */
    if (chunk_ptr == nullptr)
    {
        goto end;
    }

    n_vertices_chunk = chunk_ptr->size() / vertex_n_bytes;
    n_vertices_arg   = n_vertices;

    if (n_vertices_chunk < n_vertices_arg ||
        n_vertices_arg   == 0)
    {
        goto end;
    }

    {
        Capture::APIDumpLoader::DataChunk new_chunk(*chunk_ptr);

        in_shift_func(vertex_n_bytes,
                      n_vertices_arg,
                      new_chunk.data() );

        *out_call_ptr = FrameBuffer::copy_call(in_call);

        out_call_ptr->arg_in_vec.at(n_argument) = APIInterceptor::APIFunctionArgument::create_data_chunk_id(NEW_VERTICES_CHUNK_ID);

        inout_overlay_ptr->set_chunk(NEW_VERTICES_CHUNK_ID,
                                     new_chunk);
    }

    result = true;

end:
    return result;
}

void Stereo::Replay::EngineDLL::D3D7ScreenParallax::shift_vertices(const Parameters& in_parameters,
                                                                   const uint32_t&   in_vertex_n_bytes,
                                                                   const size_t&     in_n_vertices,
                                                                   uint8_t*          inout_vertex_ptr)
{
    constexpr size_t RECIPROCAL_DEPTH_OFFSET = 3 * sizeof(float);
    constexpr size_t X_OFFSET                = 0;

    for (size_t n_vertex = 0;
                n_vertex < in_n_vertices;
              ++n_vertex)
    {
        uint8_t* const vertex_ptr = inout_vertex_ptr + n_vertex * in_vertex_n_bytes;
        float          reciprocal = 0.0f;
        float          x          = 0.0f;

        ::memcpy(&x,
                 vertex_ptr + X_OFFSET,
                 sizeof(x) );
        ::memcpy(&reciprocal,
                 vertex_ptr + RECIPROCAL_DEPTH_OFFSET,
                 sizeof(reciprocal) );

        x += get_shift(in_parameters,
                       reciprocal);

        ::memcpy(vertex_ptr + X_OFFSET,
                 &x,
                 sizeof(x) );
    }
}
