/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_dependencies/frame_slice_planner.h"
#include "capture/api_dump_loader/api_info.h"
#include "common/argument_reader.h"
#include "common/macros.h"
#include "gl_attribute_coverage.h"
#include "slice_skipper.h"
#include <vector>

Stereo::Replay::EngineDLL::SliceSkipper::SliceSkipper()
    :m_slice_ptr(nullptr)
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::SliceSkipper::clear_slice()
{
    m_slice_ptr = nullptr;
}

Capture::APIDumpLoader::CallKeptFuncFactory Stereo::Replay::EngineDLL::SliceSkipper::get_call_kept_func_factory() const
{
    Capture::APIDumpLoader::CallKeptFuncFactory      result;
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;

    /* A slice is never freed (see get_chunk_wanted_func()), so the lambda can keep a pointer to it. Each function the factory
     * creates tracks only the lists and the attribute pushes of its own segment, because the threads that read segments work
     * independently of each other.
     */
    if (slice_ptr != nullptr)
    {
        result = [slice_ptr]()
                 {
                     bool                  is_recording     = false;
                     uint64_t              last_frame_index = UINT64_MAX;
                     std::vector<uint32_t> push_mask_vec; /* masks of the glPushAttrib calls of the frame that are not popped yet, the most recent last */

                     return Capture::APIDumpLoader::CallKeptFunc([slice_ptr,
                                                                  is_recording,
                                                                  last_frame_index,
                                                                  push_mask_vec](const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                 const uint64_t&                              in_frame_index) mutable
                                                                 {
                                                                     bool is_kept = true;

                                                                     switch (in_call.func)
                                                                     {
                                                                         case APIInterceptor::APIFUNCTION_GL_GLENDLIST:
                                                                         {
                                                                             /* A list that began in the previous segment has its first calls there. Calls left out of those are left out of the
                                                                              * list itself, so this case is not supported.
                                                                              */
                                                                             AI_ASSERT_ALWAYS(is_recording,
                                                                                              "A display list is made across two segments of the workload, so that the calls of its first part are not known when its last part is read. Seeking over it is not implemented.");

                                                                             is_recording = false;

                                                                             break;
                                                                         }

                                                                         case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
                                                                         {
                                                                             is_recording = true;

                                                                             break;
                                                                         }

                                                                         default:
                                                                         {
                                                                             const auto group = Capture::APIDumpLoader::get_api_group(in_call.func);

                                                                             /* Calls of a list that is being recorded are kept: leaving one out would remove it from the list. A call of a list is
                                                                              * decided by should_execute_call(), which knows the list.
                                                                              */
                                                                             if (is_recording == false                                  &&
                                                                                 group        == Capture::APIDumpLoader::APIGroup::GL   &&
                                                                                 in_call.func != APIInterceptor::APIFUNCTION_GL_GLCALLLIST)
                                                                             {
                                                                                 const bool is_content = is_content_call(in_call.func);
                                                                                 const bool is_live    = Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                                                                                                                   in_frame_index);

                                                                                 is_kept = (is_content == false ||
                                                                                            is_live);

                                                                                 /* In a frame that is left out, the calls between a push of attributes and its pop that the pop undoes need not be kept,
                                                                                  * because nothing that is executed reads them before the pop discards them (see GLAttributeCoverage). The push, the pop and
                                                                                  * the calls that the pop does not undo are kept. A frame that is left out must pop everything it pushes, because the state its
                                                                                  * skipped calls would have left is never created.
                                                                                  */
                                                                                 if (in_frame_index != last_frame_index)
                                                                                 {
                                                                                     AI_ASSERT_ALWAYS(push_mask_vec.empty(),
                                                                                                      "A frame that a seek leaves out pushes attributes and ends before it pops them, so that the calls that set state in between, which are left out, would have left it as the frame left it. Seeking over it is not implemented.");

                                                                                     last_frame_index = in_frame_index;
                                                                                 }

                                                                                 if (is_live == false)
                                                                                 {
                                                                                     if (in_call.func == APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB)
                                                                                     {
                                                                                         uint32_t mask = 0;

                                                                                         if (in_call.arg_in_vec.empty() == false)
                                                                                         {
                                                                                             APIInterceptor::read_number(in_call.arg_in_vec.at(0),
                                                                                                                        &mask);
                                                                                         }

                                                                                         push_mask_vec.push_back(mask);
                                                                                     }
                                                                                     else
                                                                                     if (in_call.func == APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB)
                                                                                     {
                                                                                         if (push_mask_vec.empty() == false)
                                                                                         {
                                                                                             push_mask_vec.pop_back();
                                                                                         }
                                                                                     }
                                                                                     else
                                                                                     if (push_mask_vec.empty() == false)
                                                                                     {
                                                                                         const bool is_undone = GLAttributeCoverage::is_undone_by_pop(push_mask_vec.back(),
                                                                                                                                                       in_call.func);

                                                                                         if (is_undone)
                                                                                         {
                                                                                             is_kept = false;
                                                                                         }
                                                                                     }
                                                                                 }
                                                                             }

                                                                             break;
                                                                         }
                                                                     }

                                                                     return is_kept;
                                                                 });
                 };
    }

    return result;
}

Capture::APIDumpLoader::ChunkWantedFunc Stereo::Replay::EngineDLL::SliceSkipper::get_chunk_wanted_func() const
{
    Capture::APIDumpLoader::ChunkWantedFunc                result;
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;

    /* A slice is never freed, so the function can outlive this call. It decides like should_execute_call(), except for the GL calls (see below). */
    if (slice_ptr != nullptr)
    {
        result = [slice_ptr](const Capture::APIDumpLoader::DumpedAPICall& in_call,
                             const uint64_t&                              in_frame_index)
                 {
                     const bool is_slice_live = Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                                                          in_frame_index);
                     bool       is_wanted     = true;

                     if (is_slice_live == false)
                     {
                         const bool is_content = is_content_call(in_call.func);
                         const auto group      = Capture::APIDumpLoader::get_api_group(in_call.func);

                         /* The chunks of all GL calls are wanted. The calls of a display list that is being recorded must keep their
                          * chunks, and this function cannot tell which calls those are. Only the calls themselves are left out.
                          */
                         is_wanted = (is_content == false ||
                                      group      == Capture::APIDumpLoader::APIGroup::GL);
                     }

                     return is_wanted;
                 };
    }

    return result;
}

bool Stereo::Replay::EngineDLL::SliceSkipper::is_content_call(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVESTRIDED:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVEVB:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVESTRIDED:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVEVB:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LOAD:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DVERTEXBUFFER7_LOCK:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DVERTEXBUFFER7_UNLOCK:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_LOCK:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_UNLOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_LOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_UNLOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_LOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_UNLOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_LOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_UNLOCKRECT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_LOCK:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_UNLOCK:
        case APIInterceptor::APIFUNCTION_GL_GLARRAYELEMENT:
        case APIInterceptor::APIFUNCTION_GL_GLBEGIN:
        case APIInterceptor::APIFUNCTION_GL_GLBITMAP:
        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
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

bool Stereo::Replay::EngineDLL::SliceSkipper::is_dead_frame(const uint32_t& in_n_frame) const
{
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;

    bool result = false;

    if (slice_ptr != nullptr)
    {
        const bool is_slice_live = Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                                             in_n_frame);

        result = (is_slice_live == false);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::SliceSkipper::is_list_call_skippable(const Capture::APIDumpLoader::DumpedAPICall& in_call) const
{
    Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence dependence = Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::EYE_DEPENDENT;

    uint32_t list     = 0;
    bool     is_found = false;

    if (in_call.arg_in_vec.empty() == false)
    {
        is_found = APIInterceptor::read_number(in_call.arg_in_vec.at(0),
                                               &list);
    }

    if (is_found)
    {
        is_found = m_list_tracker.find_eye_dependence(list,
                                                      &dependence);
    }

    const bool is_independent = (dependence == Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::INDEPENDENT);

    /* A list that changes the matrices, the projection, the viewport or the scissor box (or calls such a list, or is not defined)
     * changes state that is specific to an eye. If its call is left out, that change does not happen, and later frames would
     * continue from a state the capture never had. Such a list is not supported, so stop with an error instead of showing a
     * wrong picture.
     */
    AI_ASSERT_ALWAYS(is_found && is_independent,
                     "A display list that is related to the location of the eye (it loads or moves a matrix, makes something relative to it, sets a projection, a viewport or a scissor box, calls such a list, or is not defined) is called in a frame that the slice leaves out. Seeking over it is not implemented.");

    /* A list that does more than draw (it sets state, or the color or texture coordinate that later vertices inherit) is executed.
     * Frames that are not left out continue from what it leaves, and leaving it out would remove that.
     */
    bool sets_state = true;

    m_list_tracker.find_sets_state(list,
                                   &sets_state);

    return (is_independent &&
            sets_state == false);
}

void Stereo::Replay::EngineDLL::SliceSkipper::on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                               const uint32_t&                              in_n_frame,
                                                               const bool&                                  in_was_executed)
{
    (void) in_call;
    (void) in_n_frame;
    (void) in_was_executed;
}

void Stereo::Replay::EngineDLL::SliceSkipper::set_slice(const Analysis::APIDumpDependencies::FrameSlice& in_slice)
{
    std::lock_guard<std::mutex> lock(m_slice_mutex);

    m_slice_ptr_deque.push_back(std::unique_ptr<Analysis::APIDumpDependencies::FrameSlice>(new Analysis::APIDumpDependencies::FrameSlice(in_slice) ) );

    m_slice_ptr = m_slice_ptr_deque.back().get();
}

bool Stereo::Replay::EngineDLL::SliceSkipper::should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                  const uint32_t&                              in_n_frame)
{
    const Analysis::APIDumpDependencies::FrameSlice* const slice_ptr = m_slice_ptr;

    bool result = true;

    /* Track the lists in every frame, because a list that a live frame calls may have been recorded in a dead frame. */
    const bool was_recording = m_list_tracker.is_recording();

    m_list_tracker.on_call(in_call);

    if (slice_ptr != nullptr)
    {
        const bool is_slice_live = Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(*slice_ptr,
                                                                                             in_n_frame);

        /* Calls of a list that is being recorded are kept: leaving one out would remove it from every call of the list. */
        if (is_slice_live == false &&
            was_recording == false)
        {
            const bool is_content = is_content_call(in_call.func);

            result = (is_content == false);

            if (in_call.func == APIInterceptor::APIFUNCTION_GL_GLCALLLIST)
            {
                result = (is_list_call_skippable(in_call) == false);
            }
        }
    }

    return result;
}
