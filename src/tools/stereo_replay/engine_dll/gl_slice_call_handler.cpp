/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/gl_call_describer.h"
#include "common/argument_reader.h"
#include "common/macros.h"
#include "gl_attribute_coverage.h"
#include "gl_slice_call_handler.h"
#include <vector>

Stereo::Replay::EngineDLL::GLSliceCallHandler::GLSliceCallHandler()
{
    /* Stub */
}

Capture::APIDumpLoader::CallKeptFunc Stereo::Replay::EngineDLL::GLSliceCallHandler::create_call_kept_func(const FrameLiveFunc& in_is_live_func) const
{
    /* Each function tracks only the lists and the attribute pushes of its own segment, because the threads that read segments work
     * independently of each other.
     */
    bool                  is_recording     = false;
    uint64_t              last_frame_index = UINT64_MAX;
    std::vector<uint32_t> push_mask_vec; /* masks of the glPushAttrib calls of the frame that are not popped yet, the most recent last */

    return Capture::APIDumpLoader::CallKeptFunc([in_is_live_func,
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
                                                            /* Calls of a list that is being recorded are kept: leaving one out would remove it from the list. A call of a list is
                                                             * decided by should_execute_call(), which knows the list.
                                                             */
                                                            if (is_recording == false                       &&
                                                                in_call.func != APIInterceptor::APIFUNCTION_GL_GLCALLLIST)
                                                            {
                                                                const auto call_class = Analysis::APIDumpIndex::GLCallDescriber::get_function_class(in_call.func);
                                                                const bool is_live    = in_is_live_func(in_frame_index);

                                                                is_kept = (call_class == Analysis::APIDumpIndex::CallClass::STATE ||
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
}

Capture::APIDumpLoader::APIGroup Stereo::Replay::EngineDLL::GLSliceCallHandler::get_group() const
{
    return Capture::APIDumpLoader::APIGroup::GL;
}

bool Stereo::Replay::EngineDLL::GLSliceCallHandler::is_chunk_wanted_in_dead_frame(const Capture::APIDumpLoader::DumpedAPICall& in_call) const
{
    (void) in_call;

    /* The calls of a display list that is being recorded must keep their chunks, and this function cannot tell which calls those are. Only the
     * calls themselves are left out.
     */
    return true;
}

bool Stereo::Replay::EngineDLL::GLSliceCallHandler::is_list_call_skippable(const Capture::APIDumpLoader::DumpedAPICall& in_call) const
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

bool Stereo::Replay::EngineDLL::GLSliceCallHandler::should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                        const bool&                                  in_is_dead_frame)
{
    bool result = true;

    /* Track the lists in every frame, because a list that a live frame calls may have been recorded in a dead frame. */
    const bool was_recording = m_list_tracker.is_recording();

    m_list_tracker.on_call(in_call);

    /* Calls of a list that is being recorded are kept: leaving one out would remove it from every call of the list. */
    if (in_is_dead_frame &&
        was_recording == false)
    {
        const auto call_class = Analysis::APIDumpIndex::GLCallDescriber::get_function_class(in_call.func);

        result = (call_class == Analysis::APIDumpIndex::CallClass::STATE);

        if (in_call.func == APIInterceptor::APIFUNCTION_GL_GLCALLLIST)
        {
            result = (is_list_call_skippable(in_call) == false);
        }
    }

    return result;
}
