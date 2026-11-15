/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_list_classifier.h"
#include "common/argument_reader.h"
#include "gl_display_list_tracker.h"
#include "slice_skipper.h"

Stereo::Replay::EngineDLL::GLDisplayListTracker::GLDisplayListTracker()
    :m_is_recording  (false),
     m_recording_list(0)
{
    /* Stub */
}

bool Stereo::Replay::EngineDLL::GLDisplayListTracker::find_eye_dependence(const uint32_t&                                            in_list,
                                                                          Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence* out_dependence_ptr) const
{
    const auto list_iterator = m_effects_map.find(in_list);
    bool       result        = false;

    if (list_iterator != m_effects_map.end() )
    {
        *out_dependence_ptr = list_iterator->second.dependence;
        result              = true;
    }

    return result;
}

bool Stereo::Replay::EngineDLL::GLDisplayListTracker::find_sets_state(const uint32_t& in_list,
                                                                      bool*           out_sets_state_ptr) const
{
    const auto list_iterator = m_effects_map.find(in_list);
    bool       result        = false;

    if (list_iterator != m_effects_map.end() )
    {
        *out_sets_state_ptr = list_iterator->second.sets_state;
        result              = true;
    }

    return result;
}

bool Stereo::Replay::EngineDLL::GLDisplayListTracker::is_recording() const
{
    return m_is_recording;
}

void Stereo::Replay::EngineDLL::GLDisplayListTracker::on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    typedef Analysis::APIDumpAnalysis::Frontends::GLListClassifier    GLListClassifier;
    typedef Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence GLListEyeDependence;

    uint32_t first_number = 0;
    bool     has_number   = false;

    if (in_call.arg_in_vec.empty() == false)
    {
        has_number = APIInterceptor::read_number(in_call.arg_in_vec.at(0),
                                                 &first_number);
    }

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
        {
            if (m_is_recording)
            {
                /* A list that is not defined when this call is recorded may get any content before it runs. Assume the worst: the list
                 * is eye-dependent and sets state.
                 */
                GLListEyeDependence called_dependence = GLListEyeDependence::EYE_DEPENDENT;
                bool                called_sets_state = true;

                if (has_number)
                {
                    find_eye_dependence(first_number,
                                        &called_dependence);
                    find_sets_state    (first_number,
                                        &called_sets_state);
                }

                m_recording_effects.dependence = GLListClassifier::combine(m_recording_effects.dependence,
                                                                           called_dependence);
                m_recording_effects.sets_state = (m_recording_effects.sets_state || called_sets_state);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCALLLISTS:
        {
            /* glCallLists takes the list names from an array that is not read here. Assume the worst. */
            if (m_is_recording)
            {
                m_recording_effects.dependence = GLListEyeDependence::EYE_DEPENDENT;
                m_recording_effects.sets_state = true;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDELETELISTS:
        {
            uint32_t range     = 0;
            bool     has_range = false;

            if (in_call.arg_in_vec.size() >= 2)
            {
                has_range = APIInterceptor::read_number(in_call.arg_in_vec.at(1),
                                                        &range);
            }

            if (has_number &&
                has_range)
            {
                for (uint32_t n_list = 0; n_list < range; ++n_list)
                {
                    m_effects_map.erase(first_number + n_list);
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLENDLIST:
        {
            if (m_is_recording)
            {
                m_effects_map[m_recording_list] = m_recording_effects;
                m_is_recording                  = false;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
        {
            m_is_recording      = has_number;
            m_recording_effects = ListEffects();
            m_recording_list    = first_number;

            break;
        }

        default:
        {
            if (m_is_recording)
            {
                const GLListEyeDependence call_dependence = GLListClassifier::classify_call(in_call.func);

                m_recording_effects.dependence = GLListClassifier::combine(m_recording_effects.dependence,
                                                                           call_dependence);

                /* Every call that is not content counts as setting state: a state value, a texture coordinate that later vertices
                 * inherit, or an object.
                 */
                if (SliceSkipper::is_content_call(in_call.func) == false)
                {
                    m_recording_effects.sets_state = true;
                }
            }

            break;
        }
    }
}
