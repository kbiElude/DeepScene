/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "redundant_state_counter.h"
#include "capture/api_dump_names/function_names.h"
#include "common/argument_reader.h"
#include <algorithm>
#include <string.h>

Stereo::Replay::Inspector::RedundantStateCounter::RedundantStateCounter(const std::vector<bool>& in_is_live_vec)
    :m_active_unit              (0),
     m_attribute_depth          (0),
     m_client_unit              (0),
     m_frame_matrix_calls_outside(0),
     m_frame_state_calls_outside(0),
     m_has_frame                (false),
     m_is_live_vec              (in_is_live_vec),
     m_is_recording             (false),
     m_last_context             (0),
     m_matrix_depth             (0),
     m_n_balanced_frames        (0),
     m_n_dead_frames_with_calls (0),
     m_n_matrix_calls_outside   (0),
     m_n_neutral_frames         (0),
     m_n_runs                   (0),
     m_n_runs_changed_keys      (0),
     m_n_runs_unchanged_keys    (0),
     m_n_state_calls_outside    (0),
     m_seen_frame               (0)
{
    /* Stub */
}

Stereo::Replay::Inspector::RedundantStateCounter::ClassifiedCall Stereo::Replay::Inspector::RedundantStateCounter::classify(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                                                            const char*                                  in_name_ptr) const
{
    constexpr uint32_t TEXTURE_COORD_ARRAY = 0x8078; /* GL_TEXTURE_COORD_ARRAY */

    static const char* const BARRIER_NAME_ARRAY[] =
    {
        "glCallList", "glCallLists", "glDeleteLists", "glDeleteTextures", "glEndList", "glFinish", "glFlush", "glNewList", "glPopAttrib", "glPopClientAttrib",
        "glPushAttrib", "glPushClientAttrib"
    };

    /* Calls whose effect depends on earlier calls, matched by their whole names and by the beginnings of their names. */
    static const char* const SEQUENCE_NAME_ARRAY[] =
    {
        "glBegin", "glBindTexture", "glEnd", "glFrustum", "glMatrixMode", "glOrtho", "glPopMatrix", "glPushMatrix"
    };

    static const char* const SEQUENCE_PREFIX_ARRAY[] =
    {
        "glClear", "glCompressed", "glCopyTex", "glDraw", "glGen", "glLoad", "glMult", "glRotate", "glScale", "glTexImage", "glTexSubImage", "glTranslate", "glVertex"
    };

    /* Functions that always set one piece of state, whatever their arguments. */
    static const char* const SINGLE_NAME_ARRAY[] =
    {
        "glAlphaFunc", "glBlendFunc", "glClearColor", "glClearDepth", "glColorMask", "glColorMaterial", "glCullFace", "glDepthFunc", "glDepthMask", "glDepthRange",
        "glFrontFace", "glLineWidth", "glLogicOp", "glPointSize", "glPolygonOffset", "glScissor", "glShadeModel", "glStencilFunc", "glStencilMask", "glStencilOp",
        "glViewport"
    };

    ClassifiedCall      result;
    std::vector<double> argument_vec;
    bool                are_arguments_known = true;

    for (const auto& argument : in_call.arg_in_vec)
    {
        double value = 0.0;

        if (APIInterceptor::read_number(argument,
                                        &value) == false)
        {
            are_arguments_known = false;
        }

        argument_vec.push_back(value);
    }

    /* The position of the argument that selects the piece of state; 0 if there is none. */
    const auto number_func = [&](const size_t& in_index)
    {
        return std::to_string(static_cast<uint64_t>( (in_index < argument_vec.size() ) ? argument_vec.at(in_index) : 0.0) );
    };

    /* If the function takes numbers, the arguments from the given one on form the value. An array is not looked into. */
    const auto set_keyed_func = [&](const std::string& in_key,
                                    const size_t&      in_first_value_index)
    {
        result.call_class = StateCallClass::KEYED;
        result.key        = in_key;

        result.value.has_value = (are_arguments_known && takes_array(in_name_ptr) == false);

        for (size_t n_argument = in_first_value_index;
                    n_argument < argument_vec.size();
                  ++n_argument)
        {
            result.value.value_vec.push_back(argument_vec.at(n_argument) );
        }
    };

    const std::string unit_text = std::to_string(m_active_unit);

    /* Queries are not barriers. The replay does not make them; what they would write is in the calls that follow, as it was dumped. */
    if (is_one_of(BARRIER_NAME_ARRAY,
                  sizeof(BARRIER_NAME_ARRAY) / sizeof(BARRIER_NAME_ARRAY[0]),
                  in_name_ptr) )
    {
        result.call_class = StateCallClass::BARRIER;
    }
    else
    if (::strcmp(in_name_ptr, "glEnable")  == 0 ||
        ::strcmp(in_name_ptr, "glDisable") == 0)
    {
        set_keyed_func("capability/" + number_func(0),
                       1);

        result.value.has_value = are_arguments_known;
        result.value.value_vec = {(::strcmp(in_name_ptr, "glEnable") == 0) ? 1.0 : 0.0};
    }
    else
    if (::strcmp(in_name_ptr, "glEnableClientState")  == 0 ||
        ::strcmp(in_name_ptr, "glDisableClientState") == 0)
    {
        const bool is_per_unit = (argument_vec.empty() == false && static_cast<uint32_t>(argument_vec.at(0) ) == TEXTURE_COORD_ARRAY);

        set_keyed_func("client/" + number_func(0) + "/" + ( (is_per_unit) ? std::to_string(m_client_unit) : std::string("0") ),
                       1);

        result.value.has_value = are_arguments_known;
        result.value.value_vec = {(::strcmp(in_name_ptr, "glEnableClientState") == 0) ? 1.0 : 0.0};
    }
    else
    if (::strcmp(in_name_ptr, "glActiveTexture") == 0)
    {
        set_keyed_func("active unit",
                       0);
    }
    else
    if (::strcmp(in_name_ptr, "glClientActiveTexture") == 0)
    {
        set_keyed_func("client unit",
                       0);
    }
    else
    if (has_prefix(in_name_ptr, "glMaterial") )
    {
        set_keyed_func("material/" + number_func(0) + "/" + number_func(1),
                       2);
    }
    else
    if (has_prefix(in_name_ptr, "glLightModel") )
    {
        set_keyed_func("light model/" + number_func(0),
                       1);
    }
    else
    if (has_prefix(in_name_ptr, "glLight") )
    {
        set_keyed_func("light/" + number_func(0) + "/" + number_func(1),
                       2);
    }
    else
    if (has_prefix(in_name_ptr, "glTexEnv") )
    {
        set_keyed_func("texture environment/" + unit_text + "/" + number_func(0) + "/" + number_func(1),
                       2);
    }
    else
    if (has_prefix(in_name_ptr, "glTexGen") )
    {
        set_keyed_func("texture generation/" + number_func(0) + "/" + number_func(1),
                       2);
    }
    else
    if (has_prefix(in_name_ptr, "glTexParameter") )
    {
        const auto binding_iterator = m_texture_binding_map.find(std::make_pair(m_active_unit,
                                                                                static_cast<uint32_t>( (argument_vec.empty() ) ? 0.0 : argument_vec.at(0) ) ) );
        const uint32_t texture      = (binding_iterator != m_texture_binding_map.end() ) ? binding_iterator->second
                                                                                          : 0;

        set_keyed_func("texture parameter/" + std::to_string(texture) + "/" + number_func(0) + "/" + number_func(1),
                       2);
    }
    else
    if (has_prefix(in_name_ptr, "glFog") &&
        has_prefix(in_name_ptr, "glFogCoord") == false)
    {
        set_keyed_func("fog/" + number_func(0),
                       1);
    }
    else
    if (::strcmp(in_name_ptr, "glPolygonMode") == 0 ||
        ::strcmp(in_name_ptr, "glHint")        == 0)
    {
        set_keyed_func(std::string(in_name_ptr) + "/" + number_func(0),
                       1);
    }
    else
    if (is_one_of(SINGLE_NAME_ARRAY,
                  sizeof(SINGLE_NAME_ARRAY) / sizeof(SINGLE_NAME_ARRAY[0]),
                  in_name_ptr) )
    {
        set_keyed_func(in_name_ptr,
                       0);
    }
    else
    if (has_prefix(in_name_ptr, "glVertexPointer") ||
        has_prefix(in_name_ptr, "glNormalPointer") ||
        has_prefix(in_name_ptr, "glColorPointer")  ||
        has_prefix(in_name_ptr, "glFogCoordPointer") )
    {
        set_keyed_func(std::string("pointer/") + in_name_ptr,
                       0);

        result.value.has_value = false;
    }
    else
    if (has_prefix(in_name_ptr, "glTexCoordPointer") )
    {
        set_keyed_func("pointer/texture coordinates/" + std::to_string(m_client_unit),
                       0);

        result.value.has_value = false;
    }
    else
    if (has_prefix(in_name_ptr, "glMultiTexCoord") )
    {
        set_keyed_func("current texture coordinates/" + number_func(0),
                       1);
    }
    else
    if (has_prefix(in_name_ptr, "glColor") )
    {
        set_keyed_func("current color",
                       0);
    }
    else
    if (has_prefix(in_name_ptr, "glNormal") )
    {
        set_keyed_func("current normal",
                       0);
    }
    else
    if (has_prefix(in_name_ptr, "glTexCoord") )
    {
        set_keyed_func("current texture coordinates/0",
                       0);
    }
    else
    if (has_prefix(in_name_ptr, "glFogCoord") )
    {
        set_keyed_func("current fog coordinate",
                       0);
    }
    else
    {
        const bool is_sequence_name   = is_one_of(SEQUENCE_NAME_ARRAY,
                                                  sizeof(SEQUENCE_NAME_ARRAY) / sizeof(SEQUENCE_NAME_ARRAY[0]),
                                                  in_name_ptr);
        bool       is_sequence_prefix = false;

        for (const char* prefix_ptr : SEQUENCE_PREFIX_ARRAY)
        {
            if (has_prefix(in_name_ptr,
                           prefix_ptr) )
            {
                is_sequence_prefix = true;
            }
        }

        if (is_sequence_name ||
            is_sequence_prefix)
        {
            result.call_class = StateCallClass::SEQUENCE;
        }
    }

    return result;
}

void Stereo::Replay::Inspector::RedundantStateCounter::end_frame()
{
    const bool is_balanced = (m_attribute_depth == 0 &&
                              m_matrix_depth    == 0);

    if (is_balanced)
    {
        ++m_n_balanced_frames;
    }

    m_n_matrix_calls_outside += static_cast<uint64_t>(m_frame_matrix_calls_outside);
    m_n_state_calls_outside  += static_cast<uint64_t>(m_frame_state_calls_outside);

    if (is_balanced                         &&
        m_frame_matrix_calls_outside == 0   &&
        m_frame_state_calls_outside  == 0)
    {
        ++m_n_neutral_frames;
    }
}

void Stereo::Replay::Inspector::RedundantStateCounter::end_run()
{
    if (m_pending_map.empty() == false)
    {
        ++m_n_runs;

        /* What must be made when the run is over: the last value of each piece of state whose value differs from the one it had when the run began. */
        for (const auto& pending : m_pending_map)
        {
            const auto state_iterator = m_state_map.find(pending.first);
            const bool is_the_same    = (state_iterator != m_state_map.end()             &&
                                         state_iterator->second.has_value                &&
                                         pending.second.value_before_run.has_value       &&
                                         state_iterator->second.value_vec == pending.second.value_before_run.value_vec);

            if (is_the_same)
            {
                ++m_n_runs_unchanged_keys;
            }
            else
            {
                ++m_n_runs_changed_keys;
            }
        }

        m_pending_map.clear();
    }
}

void Stereo::Replay::Inspector::RedundantStateCounter::finish()
{
    if (m_has_frame)
    {
        end_frame();

        m_has_frame = false;
    }

    end_run();
}

void Stereo::Replay::Inspector::RedundantStateCounter::follow_frame(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                    const char*                                  in_name_ptr,
                                                                    const ClassifiedCall&                        in_classified)
{
    static const char* const MATRIX_CHANGE_PREFIX_ARRAY[] =
    {
        "glFrustum", "glLoadIdentity", "glLoadMatrix", "glMultMatrix", "glOrtho", "glRotate", "glScale", "glTranslate"
    };

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB:
        {
            --m_attribute_depth;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX:
        {
            --m_matrix_depth;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB:
        {
            uint64_t mask = 0;

            if (in_call.arg_in_vec.empty() == false)
            {
                APIInterceptor::read_number(in_call.arg_in_vec.at(0),
                                           &mask);
            }

            ++m_attribute_depth;
            ++m_push_mask_count_map[mask];

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX:
        {
            ++m_matrix_depth;

            break;
        }

        default:
        {
            bool changes_matrix = false;

            for (const char* prefix_ptr : MATRIX_CHANGE_PREFIX_ARRAY)
            {
                if (has_prefix(in_name_ptr,
                               prefix_ptr) )
                {
                    changes_matrix = true;
                }
            }

            if (changes_matrix &&
                m_matrix_depth == 0)
            {
                ++m_frame_matrix_calls_outside;
            }

            if (in_classified.call_class == StateCallClass::KEYED &&
                m_attribute_depth        == 0)
            {
                ++m_frame_state_calls_outside;
            }

            break;
        }
    }
}

const std::map<std::string, uint64_t>& Stereo::Replay::Inspector::RedundantStateCounter::get_barrier_count_map() const
{
    return m_barrier_count_map;
}

std::vector<Stereo::Replay::Inspector::RedundantStateCounter::FunctionEntry> Stereo::Replay::Inspector::RedundantStateCounter::get_entries() const
{
    std::vector<FunctionEntry> result;

    for (const auto& entry_pair : m_entry_map)
    {
        result.push_back(entry_pair.second);
    }

    std::sort(result.begin(),
              result.end  (),
              [](const FunctionEntry& in_a,
                 const FunctionEntry& in_b)
              {
                  return (in_a.n_calls > in_b.n_calls);
              });

    return result;
}

uint64_t Stereo::Replay::Inspector::RedundantStateCounter::get_n_balanced_frames() const
{
    return m_n_balanced_frames;
}

uint64_t Stereo::Replay::Inspector::RedundantStateCounter::get_n_changed_keys() const
{
    return m_n_runs_changed_keys;
}

uint64_t Stereo::Replay::Inspector::RedundantStateCounter::get_n_dead_frames_with_calls() const
{
    return m_n_dead_frames_with_calls;
}

uint64_t Stereo::Replay::Inspector::RedundantStateCounter::get_n_matrix_calls_outside_pushes() const
{
    return m_n_matrix_calls_outside;
}

uint64_t Stereo::Replay::Inspector::RedundantStateCounter::get_n_neutral_frames() const
{
    return m_n_neutral_frames;
}

uint64_t Stereo::Replay::Inspector::RedundantStateCounter::get_n_state_calls_outside_pushes() const
{
    return m_n_state_calls_outside;
}

const std::map<uint64_t, uint64_t>& Stereo::Replay::Inspector::RedundantStateCounter::get_push_mask_count_map() const
{
    return m_push_mask_count_map;
}

uint64_t Stereo::Replay::Inspector::RedundantStateCounter::get_n_runs() const
{
    return m_n_runs;
}

uint64_t Stereo::Replay::Inspector::RedundantStateCounter::get_n_unchanged_keys() const
{
    return m_n_runs_unchanged_keys;
}

bool Stereo::Replay::Inspector::RedundantStateCounter::has_prefix(const char* in_name_ptr,
                                                                  const char* in_prefix_ptr)
{
    return (::strncmp(in_name_ptr,
                      in_prefix_ptr,
                      ::strlen(in_prefix_ptr) ) == 0);
}

bool Stereo::Replay::Inspector::RedundantStateCounter::is_one_of(const char* const* in_name_ptr_array,
                                                                 const size_t&      in_n_names,
                                                                 const char*        in_name_ptr)
{
    bool result = false;

    for (size_t n_name = 0;
                n_name < in_n_names;
              ++n_name)
    {
        if (::strcmp(in_name_ptr_array[n_name],
                     in_name_ptr) == 0)
        {
            result = true;

            break;
        }
    }

    return result;
}

void Stereo::Replay::Inspector::RedundantStateCounter::on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                        const uint32_t&                              in_n_frame,
                                                                        const bool&                                  in_was_executed)
{
    constexpr uint32_t TEXTURE0 = 0x84C0; /* GL_TEXTURE0 */

    const char* name_ptr = Capture::APIDumpNames::get_function_name(in_call.func);

    if (name_ptr != nullptr)
    {
        const bool is_wgl = has_prefix(name_ptr,
                                       "wgl");

        if (is_wgl ||
            has_prefix(name_ptr, "gl") )
        {
            const bool is_dead = (in_n_frame < m_is_live_vec.size() &&
                                  m_is_live_vec.at(in_n_frame) == false);

            /* The first call of a frame that is made sees the state as it is, so the calls that were left out before it are made first. */
            if (is_dead == false)
            {
                end_run();
            }

            /* The pushes of a dead frame are tracked from its first call to its last. */
            if (is_wgl == false)
            {
                if (is_dead == false ||
                    (m_has_frame && in_n_frame != m_seen_frame) )
                {
                    if (m_has_frame)
                    {
                        end_frame();

                        m_has_frame = false;
                    }
                }

                if (is_dead && m_has_frame == false)
                {
                    m_attribute_depth            = 0;
                    m_frame_matrix_calls_outside = 0;
                    m_frame_state_calls_outside  = 0;
                    m_has_frame                  = true;
                    m_matrix_depth               = 0;
                    m_seen_frame                 = in_n_frame;

                    ++m_n_dead_frames_with_calls;
                }
            }

            /* A barrier ends the current group of calls that can be taken together, and the knowledge of the state that was gathered in that group. */
            const auto barrier_func = [&]()
            {
                if (is_dead)
                {
                    m_barrier_count_map[name_ptr]++;
                }

                end_run();

                m_state_map.clear();
            };

            if (is_wgl)
            {
                uint64_t context     = 0;
                bool     has_context = false;

                if (in_call.arg_in_vec.size() >= 2)
                {
                    has_context = APIInterceptor::read_number(in_call.arg_in_vec.at(1),
                                                              &context);
                }

                /* Making a different context current, creating a context or deleting one switches to another state. Making the same context current
                 * again, or making no context current, changes nothing. */
                if (::strcmp(name_ptr, "wglMakeCurrent") == 0)
                {
                    if (in_was_executed &&
                        has_context     &&
                        context != 0)
                    {
                        if (m_last_context != 0 &&
                            m_last_context != context)
                        {
                            barrier_func();
                        }

                        m_last_context = context;
                    }
                }
                else
                if (::strcmp(name_ptr, "wglCreateContext") == 0 ||
                    ::strcmp(name_ptr, "wglDeleteContext") == 0)
                {
                    if (in_was_executed)
                    {
                        barrier_func();
                    }
                }
            }
            else
            {
                const ClassifiedCall classified  = classify(in_call,
                                                            name_ptr);
                FunctionEntry*       entry_ptr   = nullptr;
                const bool           is_in_list  = m_is_recording;

                if (is_dead)
                {
                    entry_ptr = &m_entry_map[in_call.func];

                    entry_ptr->call_class = classified.call_class;
                    entry_ptr->function   = in_call.func;

                    ++entry_ptr->n_calls;

                    if (in_was_executed == false)
                    {
                        ++entry_ptr->n_skipped_by_others;
                    }
                }

                if (in_call.func == APIInterceptor::APIFUNCTION_GL_GLENDLIST)
                {
                    m_is_recording = false;
                }

                if (is_in_list)
                {
                    /* Calls that are recorded into a list are kept in the list and change no state at this point. */
                    if (entry_ptr != nullptr)
                    {
                        ++entry_ptr->n_in_lists;
                    }
                }
                else
                if (in_was_executed)
                {
                    uint32_t first_number = 0;
                    bool     has_number   = false;

                    if (in_call.arg_in_vec.empty() == false)
                    {
                        has_number = APIInterceptor::read_number(in_call.arg_in_vec.at(0),
                                                                 &first_number);
                    }

                    switch (in_call.func)
                    {
                        case APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE:
                        {
                            if (has_number &&
                                first_number >= TEXTURE0)
                            {
                                m_active_unit = first_number - TEXTURE0;
                            }

                            break;
                        }

                        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
                        {
                            uint32_t name     = 0;
                            bool     has_name = false;

                            if (in_call.arg_in_vec.size() >= 2)
                            {
                                has_name = APIInterceptor::read_number(in_call.arg_in_vec.at(1),
                                                                       &name);
                            }

                            if (has_number &&
                                has_name)
                            {
                                m_texture_binding_map[std::make_pair(m_active_unit,
                                                                     first_number)] = name;
                            }

                            break;
                        }

                        case APIInterceptor::APIFUNCTION_GL_GLCLIENTACTIVETEXTURE:
                        {
                            if (has_number &&
                                first_number >= TEXTURE0)
                            {
                                m_client_unit = first_number - TEXTURE0;
                            }

                            break;
                        }

                        case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
                        {
                            m_is_recording = true;

                            break;
                        }

                        default:
                        {
                            break;
                        }
                    }

                    if (is_dead)
                    {
                        follow_frame(in_call,
                                     name_ptr,
                                     classified);
                    }

                    if (classified.call_class == StateCallClass::BARRIER)
                    {
                        barrier_func();
                    }
                    else
                    if (classified.call_class == StateCallClass::KEYED)
                    {
                        if (entry_ptr != nullptr)
                        {
                            const auto state_iterator = m_state_map.find(classified.key);
                            const auto pending_iterator = m_pending_map.find(classified.key);

                            ++entry_ptr->n_keyed;

                            /* An earlier call already gave the piece of state this value. */
                            if (state_iterator != m_state_map.end()            &&
                                state_iterator->second.has_value               &&
                                classified.value.has_value                     &&
                                state_iterator->second.value_vec == classified.value.value_vec)
                            {
                                ++entry_ptr->n_repeats;
                            }

                            /* The earlier call in the same run that set this piece of state is overwritten, so it need not be made. */
                            if (pending_iterator != m_pending_map.end() )
                            {
                                ++m_entry_map[pending_iterator->second.function].n_overwritten_later;

                                pending_iterator->second.function = in_call.func;
                            }
                            else
                            {
                                PendingEntry pending;

                                pending.function = in_call.func;

                                if (state_iterator != m_state_map.end() )
                                {
                                    pending.value_before_run = state_iterator->second;
                                }

                                m_pending_map[classified.key] = pending;
                            }
                        }

                        m_state_map[classified.key] = classified.value;
                    }
                }
            }
        }
    }
}

bool Stereo::Replay::Inspector::RedundantStateCounter::should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                           const uint32_t&                              in_n_frame)
{
    (void) in_call;
    (void) in_n_frame;

    return true;
}

/* A function that takes an array ends in the letter v (glMaterialfv, glTexEnviv, glColor3fv). */
bool Stereo::Replay::Inspector::RedundantStateCounter::takes_array(const char* in_name_ptr)
{
    const size_t n_characters = ::strlen(in_name_ptr);

    return (n_characters > 0 &&
            in_name_ptr[n_characters - 1] == 'v');
}
