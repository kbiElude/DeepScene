/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_list_classifier.h"
#include "common/argument_reader.h"
#include "gl_display_list_tracker.h"
#include "slice_skipper.h"

Stereo::Replay::EngineDLL::GlDisplayListTracker::GlDisplayListTracker()
    :m_is_recording  (false),
     m_recording_list(0)
{
    /* Stub */
}

bool Stereo::Replay::EngineDLL::GlDisplayListTracker::find_eye_dependence(const uint32_t&                                            in_list,
                                                                          Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence* out_dependence_ptr) const
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

bool Stereo::Replay::EngineDLL::GlDisplayListTracker::find_sets_state(const uint32_t& in_list,
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

bool Stereo::Replay::EngineDLL::GlDisplayListTracker::is_recording() const
{
    return m_is_recording;
}

void Stereo::Replay::EngineDLL::GlDisplayListTracker::on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    typedef Analysis::APIDumpAnalysis::Frontends::GlListClassifier    GlListClassifier;
    typedef Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence GlListEyeDependence;

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
                /* A list that is not defined when the call is read may be anything when it is made: it is taken for the worst, and for one that sets state. */
                GlListEyeDependence called_dependence = GlListEyeDependence::EYE_DEPENDENT;
                bool                called_sets_state = true;

                if (has_number)
                {
                    find_eye_dependence(first_number,
                                        &called_dependence);
                    find_sets_state    (first_number,
                                        &called_sets_state);
                }

                m_recording_effects.dependence = GlListClassifier::combine(m_recording_effects.dependence,
                                                                           called_dependence);
                m_recording_effects.sets_state = (m_recording_effects.sets_state || called_sets_state);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCALLLISTS:
        {
            /* The lists are named by an array that is not looked at here. */
            if (m_is_recording)
            {
                m_recording_effects.dependence = GlListEyeDependence::EYE_DEPENDENT;
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
                const GlListEyeDependence call_dependence = GlListClassifier::classify_call(in_call.func);

                m_recording_effects.dependence = GlListClassifier::combine(m_recording_effects.dependence,
                                                                           call_dependence);

                /* What is not content is state, whatever it is: a state, a texture coordinate that the vertices that follow have, an object. */
                if (SliceSkipper::is_content_call(in_call.func) == false)
                {
                    m_recording_effects.sets_state = true;
                }
            }

            break;
        }
    }
}
