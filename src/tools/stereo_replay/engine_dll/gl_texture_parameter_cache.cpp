/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "common/argument_reader.h"
#include "gl_texture_parameter_cache.h"

Stereo::Replay::EngineDLL::GlTextureParameterCache::GlTextureParameterCache()
    :m_active_unit                 (0),
     m_is_recording                (false),
     m_is_unit_known               (true), /* the unit of a context that is new */
     m_last_context                (0),
     m_last_frame                  (0),
     m_n_skipped_calls             (0),
     m_recording_list              (0),
     m_recording_sets_texture_state(false),
     m_skipped_last_call           (false)
{
    /* Stub */
}

uint64_t Stereo::Replay::EngineDLL::GlTextureParameterCache::get_n_skipped_calls() const
{
    return m_n_skipped_calls;
}

void Stereo::Replay::EngineDLL::GlTextureParameterCache::on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                          const uint32_t&                              in_n_frame,
                                                                          const bool&                                  in_was_executed)
{
    const bool skipped_by_this_cache = m_skipped_last_call;
    const bool is_state_call         = is_texture_state_call(in_call.func);

    uint32_t   list_number           = 0;
    bool       has_list_number       = false;

    (void) in_n_frame;

    m_skipped_last_call = false;

    if (in_call.arg_in_vec.empty() == false)
    {
        has_list_number = APIInterceptor::read_number(in_call.arg_in_vec.at(0),
                                                      &list_number);
    }

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
        {
            const size_t n_known_lists   = m_known_list_set.count  (list_number);
            const size_t n_texture_lists = m_texture_list_set.count(list_number);
            const bool   is_list_safe    = (has_list_number        &&
                                            n_known_lists   != 0   &&
                                            n_texture_lists == 0);

            /* A list that is not known may be anything: it is made, so what is known of the textures is not. */
            if (is_list_safe == false)
            {
                forget_all();

                if (m_is_recording)
                {
                    m_recording_sets_texture_state = true;
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCALLLISTS:
        {
            forget_all();

            if (m_is_recording)
            {
                m_recording_sets_texture_state = true;
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

            if (has_list_number &&
                has_range)
            {
                for (uint32_t n_list = 0;
                              n_list < range;
                            ++n_list)
                {
                    m_known_list_set.erase  (list_number + n_list);
                    m_texture_list_set.erase(list_number + n_list);
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLENDLIST:
        {
            if (m_is_recording)
            {
                m_known_list_set.insert(m_recording_list);

                if (m_recording_sets_texture_state)
                {
                    m_texture_list_set.insert(m_recording_list);
                }
                else
                {
                    m_texture_list_set.erase(m_recording_list);
                }

                m_is_recording = false;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
        {
            m_is_recording                 = has_list_number;
            m_recording_list               = list_number;
            m_recording_sets_texture_state = false;

            break;
        }

        default:
        {
            if (m_is_recording)
            {
                /* What a list does is done when it is called, or now if it is made and made at once: it is not known which, so both are taken for it. */
                if (is_state_call)
                {
                    m_recording_sets_texture_state = true;

                    forget_all();
                }
            }
            else
            if (in_was_executed)
            {
                update_for_executed_call(in_call);
            }
            else
            if (skipped_by_this_cache == false &&
                is_state_call)
            {
                /* A call that another interceptor left out: what it would have done is not known. */
                forget_all();
            }

            break;
        }
    }
}

bool Stereo::Replay::EngineDLL::GlTextureParameterCache::should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                             const uint32_t&                              in_n_frame)
{
    uint32_t parameter = 0;
    bool     result    = true;
    uint32_t target    = 0;
    double   value     = 0.0;

    /* A replay that starts over makes the context again. */
    if (in_n_frame < m_last_frame)
    {
        forget_all();
    }

    m_last_frame        = in_n_frame;
    m_skipped_last_call = false;

    if (m_is_recording == false)
    {
        const bool is_scalar_call = read_scalar_parameter_call(in_call,
                                                              &parameter,
                                                              &target,
                                                              &value);

        if (is_scalar_call)
        {
            if (is_cached_parameter(parameter) )
            {
                uint32_t texture = 0;

                if (find_bound_texture(target,
                                      &texture) )
                {
                    const ParameterKey parameter_key(target,
                                                     texture,
                                                     parameter);
                    const auto         value_iterator = m_value_map.find(parameter_key);

                    if (value_iterator != m_value_map.end() )
                    {
                        if (value_iterator->second == value)
                        {
                            m_skipped_last_call = true;
                            result              = false;

                            ++m_n_skipped_calls;
                        }
                    }
                }
            }
        }
    }

    return result;
}

const std::vector<unsigned int>& Stereo::Replay::EngineDLL::GlTextureParameterCache::get_cached_parameters()
{
    static const std::vector<unsigned int> result =
    {
        0x2800, /* GL_TEXTURE_MAG_FILTER   */
        0x2801, /* GL_TEXTURE_MIN_FILTER   */
        0x2802, /* GL_TEXTURE_WRAP_S       */
        0x2803, /* GL_TEXTURE_WRAP_T       */
        0x8072, /* GL_TEXTURE_WRAP_R       */
        0x8191, /* GL_GENERATE_MIPMAP      */
        0x813C, /* GL_TEXTURE_BASE_LEVEL   */
        0x813D, /* GL_TEXTURE_MAX_LEVEL    */
        0x813A, /* GL_TEXTURE_MIN_LOD      */
        0x813B  /* GL_TEXTURE_MAX_LOD      */
    };

    return result;
}

bool Stereo::Replay::EngineDLL::GlTextureParameterCache::find_bound_texture(const uint32_t& in_target,
                                                                            uint32_t*       out_texture_ptr) const
{
    const BindingKey binding_key(m_active_unit,
                                 in_target);
    bool             result = false;

    if (m_is_unit_known)
    {
        const auto binding_iterator = m_bound_texture_map.find(binding_key);

        if (binding_iterator != m_bound_texture_map.end() )
        {
            *out_texture_ptr = binding_iterator->second;
            result           = true;
        }
    }

    return result;
}

void Stereo::Replay::EngineDLL::GlTextureParameterCache::forget_all()
{
    /* A call of a list, or glPopAttrib, may have changed the active unit as well. */
    m_bound_texture_map.clear();
    m_value_map.clear();

    m_is_unit_known = false;
}

void Stereo::Replay::EngineDLL::GlTextureParameterCache::update_for_executed_call(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    constexpr uint32_t TEXTURE0 = 0x84C0; /* GL_TEXTURE0 */

    uint32_t parameter = 0;
    uint32_t target    = 0;
    double   value     = 0.0;

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE:
        {
            uint32_t unit_enum = 0;
            bool     has_unit  = false;

            if (in_call.arg_in_vec.empty() == false)
            {
                has_unit = APIInterceptor::read_number(in_call.arg_in_vec.at(0),
                                                       &unit_enum);
            }

            if (has_unit          &&
                unit_enum >= TEXTURE0)
            {
                m_active_unit   = unit_enum - TEXTURE0;
                m_is_unit_known = true;
            }
            else
            {
                forget_all();
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
        {
            uint32_t name     = 0;
            bool     has_name = false;

            if (in_call.arg_in_vec.size() >= 2)
            {
                const bool has_target = APIInterceptor::read_number(in_call.arg_in_vec.at(0),
                                                                    &target);

                has_name = (has_target                                                          &&
                            APIInterceptor::read_number(in_call.arg_in_vec.at(1),
                                                        &name) );
            }

            if (has_name == false)
            {
                forget_all();
            }
            else
            if (m_is_unit_known)
            {
                const BindingKey binding_key(m_active_unit,
                                             target);

                m_bound_texture_map[binding_key] = name;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERF:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI:
        {
            const bool is_scalar_call = read_scalar_parameter_call(in_call,
                                                                  &parameter,
                                                                  &target,
                                                                  &value);
            uint32_t   texture        = 0;

            if (is_scalar_call == false)
            {
                forget_all();
            }
            else
            if (is_cached_parameter(parameter) )
            {
                if (find_bound_texture(target,
                                      &texture) )
                {
                    const ParameterKey parameter_key(target,
                                                     texture,
                                                     parameter);

                    m_value_map[parameter_key] = value;
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERIV:
        {
            /* The value is in a chunk that is not looked at here: what the texture has of the parameter is not known from now on. */
            bool has_parameter = false;

            if (in_call.arg_in_vec.size() >= 2)
            {
                const bool has_target = APIInterceptor::read_number(in_call.arg_in_vec.at(0),
                                                                    &target);

                has_parameter = (has_target                                                          &&
                                 APIInterceptor::read_number(in_call.arg_in_vec.at(1),
                                                             &parameter) );
            }

            if (has_parameter == false)
            {
                forget_all();
            }
            else
            {
                uint32_t texture = 0;

                if (find_bound_texture(target,
                                      &texture) )
                {
                    const ParameterKey parameter_key(target,
                                                     texture,
                                                     parameter);

                    m_value_map.erase(parameter_key);
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERF:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERI:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERIIV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERIUIV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERIV:
        case APIInterceptor::APIFUNCTION_WGL_WGLCREATECONTEXT:
        case APIInterceptor::APIFUNCTION_WGL_WGLCREATECONTEXTATTRIBSARB:
        case APIInterceptor::APIFUNCTION_WGL_WGLDELETECONTEXT:
        case APIInterceptor::APIFUNCTION_WGL_WGLSHARELISTS:
        {
            forget_all();

            break;
        }

        case APIInterceptor::APIFUNCTION_WGL_WGLMAKECURRENT:
        {
            uint64_t context     = 0;
            bool     has_context = false;

            if (in_call.arg_in_vec.size() >= 2)
            {
                has_context = APIInterceptor::read_number(in_call.arg_in_vec.at(1),
                                                          &context);
            }

            /* The textures belong to a context: another one has others. Making none current leaves them as they are. */
            if (has_context  &&
                context != 0)
            {
                if (m_last_context != 0       &&
                    m_last_context != context)
                {
                    forget_all();
                }

                m_last_context = context;
            }

            break;
        }

        default:
        {
            break;
        }
    }
}

bool Stereo::Replay::EngineDLL::GlTextureParameterCache::is_cached_parameter(const uint32_t& in_parameter)
{
    const auto& parameter_vec = get_cached_parameters();
    bool        result        = false;

    for (const auto cached_parameter : parameter_vec)
    {
        if (cached_parameter == in_parameter)
        {
            result = true;

            break;
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::GlTextureParameterCache::is_texture_state_call(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERF:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERIV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERF:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERI:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERIIV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERIUIV:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREPARAMETERIV:
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

bool Stereo::Replay::EngineDLL::GlTextureParameterCache::read_scalar_parameter_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                    uint32_t*                                    out_parameter_ptr,
                                                                                    uint32_t*                                    out_target_ptr,
                                                                                    double*                                      out_value_ptr)
{
    bool result = false;

    if ((in_call.func == APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERF ||
         in_call.func == APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI) &&
        in_call.arg_in_vec.size() >= 3)
    {
        const bool has_target    = APIInterceptor::read_number(in_call.arg_in_vec.at(0),
                                                               out_target_ptr);
        const bool has_parameter = APIInterceptor::read_number(in_call.arg_in_vec.at(1),
                                                               out_parameter_ptr);
        const bool has_value     = APIInterceptor::read_number(in_call.arg_in_vec.at(2),
                                                               out_value_ptr);

        result = (has_target    &&
                  has_parameter &&
                  has_value);
    }

    return result;
}
