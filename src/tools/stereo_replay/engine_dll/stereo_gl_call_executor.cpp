/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "capture/api_dump_names/function_names.h"
#include "common/api_function_argument.h"
#include "gl_texture_parameter_cache.h"
#include "object_remapper.h"
#include "stereo_gl_call_executor.h"
#include <algorithm>
#include <string.h>
#include <vector>
#include <Windows.h>

namespace
{
    typedef void         (APIENTRY* PFNACTIVETEXTUREPROC)  (unsigned int);
    typedef void         (APIENTRY* PFNBINDTEXTUREPROC)    (unsigned int, unsigned int);
    typedef void         (APIENTRY* PFNDELETETEXTURESPROC) (int, const unsigned int*);
    typedef unsigned int (APIENTRY* PFNGETERRORPROC)       ();
    typedef void         (APIENTRY* PFNGENTEXTURESPROC)    (int, unsigned int*);
    typedef void         (APIENTRY* PFNGETINTEGERVPROC)    (unsigned int, int*);
    typedef void         (APIENTRY* PFNGETTEXPARAMETERIVPROC)(unsigned int, unsigned int, int*);
    typedef void         (APIENTRY* PFNTEXPARAMETERIPROC)  (unsigned int, unsigned int, int);

    constexpr unsigned int ACTIVE_TEXTURE       = 0x84E0;
    constexpr unsigned int MAX_TEXTURE_UNITS    = 0x84E2;
    constexpr unsigned int TEXTURE0             = 0x84C0;
    constexpr unsigned int TEXTURE_2D           = 0x0DE1;
    constexpr unsigned int TEXTURE_BINDING_2D   = 0x8069;

    /* More units than this are not looked at: a program of the age of fixed-function GL does not use them. */
    constexpr int MAX_N_UNITS = 8;
}

Stereo::Replay::EngineDLL::StereoGlCallExecutor::StereoGlCallExecutor(::Replay::ReplayEngineDLL::ObjectRemapper* in_remapper_ptr)
    :GlCallExecutor  (in_remapper_ptr),
     m_eye           (Analysis::Recipe::Eye::LEFT),
     m_is_replicating(false),
     m_n_executions  (1),
     m_remapper_ptr  (in_remapper_ptr)
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::StereoGlCallExecutor::apply_eye()
{
    const auto active_texture_func_ptr = reinterpret_cast<PFNACTIVETEXTUREPROC>(get_proc_ptr("glActiveTexture") );
    const auto bind_func_ptr           = reinterpret_cast<PFNBINDTEXTUREPROC>   (get_proc_ptr("glBindTexture") );
    const auto get_integerv_func_ptr   = reinterpret_cast<PFNGETINTEGERVPROC>   (get_proc_ptr("glGetIntegerv") );

    int active_unit = 0;
    int n_units     = 1;

    if (m_twin_map.empty() )
    {
        goto end;
    }

    /* What the program's name for a texture is mapped to: the copy of the eye. */
    for (const auto& twin_pair : m_twin_map)
    {
        m_remapper_ptr->add(::Replay::ReplayEngineDLL::ObjectKind::TEXTURE,
                            twin_pair.first,
                            (m_eye == Analysis::Recipe::Eye::RIGHT) ? twin_pair.second.right_live_name
                                                                    : twin_pair.second.left_live_name);
    }

    if (::wglGetCurrentContext() == nullptr ||
        bind_func_ptr            == nullptr ||
        get_integerv_func_ptr    == nullptr)
    {
        goto end;
    }

    /* What a unit has bound is what the eye before left there: the copy of this eye is bound in its place. */
    if (active_texture_func_ptr != nullptr)
    {
        get_integerv_func_ptr(ACTIVE_TEXTURE,
                             &active_unit);
        get_integerv_func_ptr(MAX_TEXTURE_UNITS,
                             &n_units);

        n_units = (std::min)((std::max)(n_units,
                                        1),
                             MAX_N_UNITS);
    }

    for (int n_unit = 0;
             n_unit < n_units;
           ++n_unit)
    {
        int bound_name = 0;

        if (active_texture_func_ptr != nullptr)
        {
            active_texture_func_ptr(TEXTURE0 + static_cast<unsigned int>(n_unit) );
        }

        get_integerv_func_ptr(TEXTURE_BINDING_2D,
                             &bound_name);

        for (const auto& twin_pair : m_twin_map)
        {
            if (bound_name == static_cast<int>(twin_pair.second.left_live_name)  ||
                bound_name == static_cast<int>(twin_pair.second.right_live_name) )
            {
                bind_func_ptr(TEXTURE_2D,
                              (m_eye == Analysis::Recipe::Eye::RIGHT) ? twin_pair.second.right_live_name
                                                                      : twin_pair.second.left_live_name);

                break;
            }
        }
    }

    if (active_texture_func_ptr != nullptr)
    {
        active_texture_func_ptr(static_cast<unsigned int>(active_unit) );
    }

end:
    return;
}

bool Stereo::Replay::EngineDLL::StereoGlCallExecutor::changes_bound_texture(const APIInterceptor::APIFunction& in_function)
{
    static const char* const prefix_ptr_array[] =
    {
        "glCompressedTexImage",
        "glCompressedTexSubImage",
        "glCopyTexImage",
        "glCopyTexSubImage",
        "glGenerateMipmap",
        "glTexImage",
        "glTexParameter",
        "glTexStorage",
        "glTexSubImage"
    };

    const char* name_ptr = Capture::APIDumpNames::get_function_name(in_function);
    bool        result   = false;

    for (const char* prefix_ptr : prefix_ptr_array)
    {
        if (name_ptr != nullptr                   &&
            ::strncmp(name_ptr,
                      prefix_ptr,
                      ::strlen(prefix_ptr) ) == 0)
        {
            result = true;

            break;
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoGlCallExecutor::execute(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                              const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                              std::string*                                   out_error_ptr)
{
    const auto get_integerv_func_ptr = reinterpret_cast<PFNGETINTEGERVPROC>(get_proc_ptr("glGetIntegerv") );

    std::vector<std::pair<uint32_t, uint32_t> > other_copy_vec; /* the dumped name and the live name of the copy of the other eye, of the textures that are deleted */
    bool                                        result = false;

    /* What is made again for the other eye is not looked at again. */
    if (m_is_replicating)
    {
        result = GlCallExecutor::execute(in_call,
                                         in_chunk_source,
                                         out_error_ptr);

        goto end;
    }

    /* The copy of the other eye of a texture that is deleted is deleted with it: the call finds the one that the eye has, and the remapper forgets it. */
    if (in_call.func == APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES &&
        m_twin_map.empty() == false                                     &&
        in_call.arg_in_vec.size() > 1                                   &&
        in_call.arg_in_vec.at(1).type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        const auto* chunk_ptr = in_chunk_source.get_data_chunk_ptr(in_call.arg_in_vec.at(1).get_data_chunk_id() );

        for (size_t n_name = 0;
                    chunk_ptr != nullptr                                       &&
                    (n_name + 1) * sizeof(uint32_t) <= chunk_ptr->size();
                  ++n_name)
        {
            uint32_t dumped_name = 0;

            ::memcpy(&dumped_name,
                     chunk_ptr->data() + n_name * sizeof(uint32_t),
                     sizeof(uint32_t) );

            const auto twin_iterator = m_twin_map.find(dumped_name);

            if (twin_iterator != m_twin_map.end() )
            {
                other_copy_vec.push_back({dumped_name,
                                          (m_eye == Analysis::Recipe::Eye::RIGHT) ? twin_iterator->second.left_live_name
                                                                                  : twin_iterator->second.right_live_name});
            }
        }
    }

    result = GlCallExecutor::execute(in_call,
                                     in_chunk_source,
                                     out_error_ptr);

    if (result == false)
    {
        goto end;
    }

    if (other_copy_vec.empty() == false)
    {
        const auto delete_func_ptr = reinterpret_cast<PFNDELETETEXTURESPROC>(get_proc_ptr("glDeleteTextures") );

        for (const auto& other_copy : other_copy_vec)
        {
            if (delete_func_ptr != nullptr)
            {
                delete_func_ptr(1,
                               &other_copy.second);
            }

            m_twin_map.erase(other_copy.first);
        }
    }
    else
    if (changes_bound_texture(in_call.func) &&
        get_integerv_func_ptr != nullptr    &&
        ::wglGetCurrentContext() != nullptr)
    {
        int      bound_name   = 0;
        uint64_t dumped_value = 0;
        bool     has_dumped   = false;

        get_integerv_func_ptr(TEXTURE_BINDING_2D,
                             &bound_name);

        /* The copy that is bound is one of a texture that each eye has a copy of, or a texture of the program that is not known to be. */
        for (const auto& twin_pair : m_twin_map)
        {
            if (static_cast<uint32_t>(bound_name) == twin_pair.second.left_live_name ||
                static_cast<uint32_t>(bound_name) == twin_pair.second.right_live_name)
            {
                dumped_value = twin_pair.first;
                has_dumped   = true;

                break;
            }
        }

        if (has_dumped == false &&
            bound_name != 0)
        {
            has_dumped = m_remapper_ptr->find_dumped(::Replay::ReplayEngineDLL::ObjectKind::TEXTURE,
                                                     static_cast<uint64_t>(bound_name),
                                                    &dumped_value);
        }

        if (has_dumped                             &&
            m_twin_map.count(static_cast<uint32_t>(dumped_value) ) == 0 &&
            m_per_eye_predicate                    &&
            m_per_eye_predicate(dumped_value) )
        {
            make_twin(static_cast<uint32_t>(dumped_value),
                      static_cast<uint32_t>(bound_name) );
        }

        if (has_dumped                             &&
            m_n_executions == 1                    &&
            m_twin_map.count(static_cast<uint32_t>(dumped_value) ) != 0)
        {
            result = replicate_for_other_eye(in_call,
                                             in_chunk_source,
                                             static_cast<uint32_t>(dumped_value),
                                             out_error_ptr);
        }
    }

end:
    return result;
}

bool Stereo::Replay::EngineDLL::StereoGlCallExecutor::make_twin(const uint32_t& in_dumped_name,
                                                                const uint32_t& in_live_name)
{
    const auto bind_func_ptr         = reinterpret_cast<PFNBINDTEXTUREPROC>      (get_proc_ptr("glBindTexture") );
    const auto gen_func_ptr          = reinterpret_cast<PFNGENTEXTURESPROC>      (get_proc_ptr("glGenTextures") );
    const auto get_error_func_ptr    = reinterpret_cast<PFNGETERRORPROC>         (get_proc_ptr("glGetError") );
    const auto get_parameter_func_ptr= reinterpret_cast<PFNGETTEXPARAMETERIVPROC>(get_proc_ptr("glGetTexParameteriv") );
    const auto set_parameter_func_ptr= reinterpret_cast<PFNTEXPARAMETERIPROC>    (get_proc_ptr("glTexParameteri") );

    /* What a texture has besides its pixels, that a copy takes: the parameters that the cache of them looks at, so that a call it leaves out is one that the twin
     * would not have changed either.
     */
    const auto&      parameter_vec = GlTextureParameterCache::get_cached_parameters();
    const size_t     n_parameters  = parameter_vec.size();
    std::vector<int> value_vec(n_parameters);
    unsigned int     fresh_name    = 0;
    bool             result        = false;
    TextureTwin      twin;

    if (bind_func_ptr          == nullptr ||
        gen_func_ptr           == nullptr ||
        get_parameter_func_ptr == nullptr ||
        set_parameter_func_ptr == nullptr)
    {
        goto end;
    }

    gen_func_ptr(1,
                &fresh_name);

    if (fresh_name == 0)
    {
        goto end;
    }

    /* The parameters are read from the texture that is bound, and set on the copy. */
    for (size_t n_parameter = 0;
                n_parameter < n_parameters;
              ++n_parameter)
    {
        const unsigned int parameter = parameter_vec.at(n_parameter);
        int&               value     = value_vec.at    (n_parameter);

        get_parameter_func_ptr(TEXTURE_2D,
                               parameter,
                              &value);
    }

    bind_func_ptr(TEXTURE_2D,
                  fresh_name);

    for (size_t n_parameter = 0;
                n_parameter < n_parameters;
              ++n_parameter)
    {
        const unsigned int parameter = parameter_vec.at(n_parameter);
        const int          value     = value_vec.at    (n_parameter);

        set_parameter_func_ptr(TEXTURE_2D,
                               parameter,
                               value);
    }

    bind_func_ptr(TEXTURE_2D,
                  in_live_name);

    /* A parameter that the driver does not have left an error that the program did not make. */
    while (get_error_func_ptr != nullptr &&
           get_error_func_ptr() != 0)
    {
    }

    twin.left_live_name  = (m_eye == Analysis::Recipe::Eye::RIGHT) ? fresh_name
                                                                   : in_live_name;
    twin.right_live_name = (m_eye == Analysis::Recipe::Eye::RIGHT) ? in_live_name
                                                                   : fresh_name;

    m_twin_map[in_dumped_name] = twin;


    result = true;

end:
    return result;
}

void Stereo::Replay::EngineDLL::StereoGlCallExecutor::reset()
{
    GlCallExecutor::reset();

    m_twin_map.clear();
}

bool Stereo::Replay::EngineDLL::StereoGlCallExecutor::replicate_for_other_eye(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                                              const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                                              const uint32_t&                                in_dumped_name,
                                                                              std::string*                                   out_error_ptr)
{
    const auto bind_func_ptr = reinterpret_cast<PFNBINDTEXTUREPROC>(get_proc_ptr("glBindTexture") );
    const auto twin_iterator = m_twin_map.find(in_dumped_name);

    bool result = false;

    if (bind_func_ptr == nullptr ||
        twin_iterator == m_twin_map.end() )
    {
        goto end;
    }

    {
        const uint32_t current_name = (m_eye == Analysis::Recipe::Eye::RIGHT) ? twin_iterator->second.right_live_name
                                                                               : twin_iterator->second.left_live_name;
        const uint32_t other_name   = (m_eye == Analysis::Recipe::Eye::RIGHT) ? twin_iterator->second.left_live_name
                                                                               : twin_iterator->second.right_live_name;

        m_is_replicating = true;

        bind_func_ptr(TEXTURE_2D,
                      other_name);

        result = execute(in_call,
                         in_chunk_source,
                         out_error_ptr);

        bind_func_ptr(TEXTURE_2D,
                      current_name);

        m_is_replicating = false;
    }

end:
    return result;
}

void Stereo::Replay::EngineDLL::StereoGlCallExecutor::restore_state()
{
    GlCallExecutor::restore_state();

    /* The state that was put back has what the program had bound before: the copies of this eye are bound again. */
    apply_eye();
}

void Stereo::Replay::EngineDLL::StereoGlCallExecutor::set_eye(const Analysis::Recipe::Eye& in_eye)
{
    m_eye = in_eye;

    apply_eye();
}

void Stereo::Replay::EngineDLL::StereoGlCallExecutor::set_n_executions(const uint32_t& in_n_executions)
{
    m_n_executions = in_n_executions;
}

void Stereo::Replay::EngineDLL::StereoGlCallExecutor::set_per_eye_predicate(const PerEyePredicate& in_predicate)
{
    m_per_eye_predicate = in_predicate;
}
