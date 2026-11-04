/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Core/D3D7Utils/vertex_format.h"
#include "call_context.h"
#include "d3d7_draw_rewriter.h"
#include "frame_buffer.h"

bool Stereo::Replay::EngineDLL::D3D7DrawRewriter::rewrite(const Capture::APIDumpLoader::DumpedAPICall&    in_call,
                                                          const ::Replay::ReplayEngineDLL::IChunkSource&  in_chunk_source,
                                                          const Stereo::D3D7::ScreenParallax::Parameters& in_parameters,
                                                          OverlayChunkSource*                             inout_overlay_ptr,
                                                          Capture::APIDumpLoader::DumpedAPICall*          out_call_ptr)
{
    bool result = false;

    if (Stereo::D3D7::ScreenParallax::is_valid(in_parameters) )
    {
        result = rewrite_vertices(in_call,
                                  in_chunk_source,
                                  [&in_parameters](const uint32_t& in_vertex_n_bytes,
                                                   const size_t&   in_n_vertices,
                                                   uint8_t*        inout_vertex_ptr)
                                  {
                                      Stereo::D3D7::ScreenParallax::shift_vertices(in_parameters,
                                                                                   in_vertex_n_bytes,
                                                                                   in_n_vertices,
                                                                                   inout_vertex_ptr);
                                  },
                                  inout_overlay_ptr,
                                  out_call_ptr);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7DrawRewriter::rewrite_by(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                             const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                             const float&                                   in_shift,
                                                             OverlayChunkSource*                            inout_overlay_ptr,
                                                             Capture::APIDumpLoader::DumpedAPICall*         out_call_ptr)
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
                                      Stereo::D3D7::ScreenParallax::shift_vertices_by(in_shift,
                                                                                      in_vertex_n_bytes,
                                                                                      in_n_vertices,
                                                                                      inout_vertex_ptr);
                                  },
                                  inout_overlay_ptr,
                                  out_call_ptr);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::D3D7DrawRewriter::rewrite_vertices(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                                   const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                                   const ShiftFunc&                               in_shift_func,
                                                                   OverlayChunkSource*                            inout_overlay_ptr,
                                                                   Capture::APIDumpLoader::DumpedAPICall*         out_call_ptr)
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
