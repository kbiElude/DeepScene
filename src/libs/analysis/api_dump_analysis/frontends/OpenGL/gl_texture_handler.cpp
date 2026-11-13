/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_texture_handler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_argument_reader.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_texture_format_mapper.h"
#include "analysis/api_dump_analysis/frontends/shared/call_view.h"
#include "analysis/api_dump_analysis/frontends/shared/unsupported_use.h"

Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::GlTextureHandler(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_builder_ptr       (in_builder_ptr),
     m_n_unfollowed_calls(0)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::add_texture_bindings(const GlContextState&                       in_state,
                                                                                  Analysis::APIDumpAnalysis::DrawDescription* out_description_ptr) const
{
    constexpr uint32_t TEXTURE_2D = 0x0DE1;

    /* A texture is sampled by a unit that has its target enabled and a texture of the program bound (one that has been given a size). */
    for (const auto& binding_pair : in_state.attributes.texture_binding_map)
    {
        const uint32_t unit             = binding_pair.first;
        const bool     is_enabled       = in_state.pipeline_state.is_unit_capability_enabled(unit,
                                                                                             TEXTURE_2D);
        const auto     texture_iterator = m_texture_map.find(binding_pair.second);
        const bool     is_known         = (texture_iterator != m_texture_map.end() );

        if (is_enabled &&
            is_known)
        {
            Analysis::APIDumpAnalysis::TextureBinding binding;

            binding.binding_point.index = unit;
            binding.view.resource_id    = texture_iterator->second.resource_id;

            out_description_ptr->texture_binding_vec.push_back(binding);
        }
    }
}

bool Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::find_resource_view(const uint64_t&                          in_address,
                                                                                Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const
{
    /* The objects of OpenGL are numbers, not addresses of the application: the address is the name of a texture, as the program has it. A texture has a resource
     * once it is given a size, and until then there is none to find.
     */
    const auto texture_iterator = m_texture_map.find(static_cast<uint32_t>(in_address) );
    bool       result           = false;

    if (texture_iterator != m_texture_map.end()                                         &&
        texture_iterator->second.resource_id != Analysis::APIDumpAnalysis::ResourceId::NONE)
    {
        out_view_ptr->resource_id = texture_iterator->second.resource_id;

        result = true;
    }

    return result;
}

const Analysis::APIDumpAnalysis::Frontends::GlTexture* Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::find_texture_ptr(const uint32_t& in_name) const
{
    const auto texture_iterator = m_texture_map.find(in_name);

    return (texture_iterator != m_texture_map.end() ) ? &texture_iterator->second
                                                      : nullptr;
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::generate_mipmap_levels(const GlContextState& in_state)
{
    const uint32_t name             = get_bound_texture_name(in_state);
    const auto     texture_iterator = m_texture_map.find(name);

    if (texture_iterator == m_texture_map.end()                ||
        texture_iterator->second.level_map.count(0) == 0)
    {
        /* A texture whose first level has not been given has nothing to make the levels of. */
        m_n_unfollowed_calls++;
    }
    else
    {
        GlTexture&           texture       = texture_iterator->second;
        GlTextureLevel       texture_level = texture.level_map.at(0);
        const auto           parameters_it = m_texture_parameter_map.find(name);
        const uint32_t       max_level     = (parameters_it != m_texture_parameter_map.end() ) ? parameters_it->second.max_level
                                                                                               : GlTextureParameters().max_level;
        const bool           wants_events  = m_builder_ptr->wants_texture_events();
        uint32_t             level         = 1;

        /* The levels go down to one texel by one: each is half of the one above, and no less than one. */
        while ((texture_level.width > 1 || texture_level.height > 1) &&
               level <= max_level)
        {
            Analysis::APIDumpAnalysis::ResourceView destination;
            Analysis::APIDumpAnalysis::ResourceView source;

            texture_level.height = (texture_level.height > 1) ? texture_level.height / 2 : 1;
            texture_level.width  = (texture_level.width  > 1) ? texture_level.width  / 2 : 1;

            texture.level_map[level] = texture_level;

            /* What the driver makes is not in the workload: the level is content that is read back, once something reads it. */
            texture.dirty_level_set.insert (level);
            texture.render_level_set.insert(level);

            /* The driver makes the whole of the level. */
            GlTextureRegion whole_level;

            whole_level.height = texture_level.height;
            whole_level.width  = texture_level.width;

            texture.render_region_map[level] = whole_level;

            destination.mip_level   = level;
            destination.resource_id = texture.resource_id;
            source.mip_level        = level - 1;
            source.resource_id      = texture.resource_id;

            m_builder_ptr->on_copy(source,
                                   destination);

            if (wants_events)
            {
                Analysis::APIDumpTextures::TextureEvent event = make_texture_event(texture,
                                                                                   level,
                                                                                   0,
                                                                                   0,
                                                                                   texture_level.width,
                                                                                   texture_level.height);

                event.generated_from_level = level - 1;
                event.kind                 = Analysis::APIDumpTextures::TextureEventKind::GENERATE_MIPS;

                m_builder_ptr->on_texture_event(event,
                                                nullptr,
                                                0,
                                                0);
            }

            ++level;
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::generate_mipmaps_if_automatic(const GlContextState& in_state,
                                                                                           const uint32_t&       in_level)
{
    const uint32_t name          = get_bound_texture_name(in_state);
    const auto     parameters_it = m_texture_parameter_map.find(name);

    /* GL_GENERATE_MIPMAP makes the levels below the first whenever the first changes. */
    if (in_level == 0                                           &&
        parameters_it != m_texture_parameter_map.end()          &&
        parameters_it->second.generates_mipmaps)
    {
        generate_mipmap_levels(in_state);
    }
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::get_bound_texture_name(const GlContextState& in_state)
{
    const uint32_t unit             = in_state.attributes.active_texture_unit;
    const auto     binding_iterator = in_state.attributes.texture_binding_map.find(unit);
    uint32_t       result           = 0;

    /* A unit that nothing has been bound to has the default texture, which is named 0. */
    if (binding_iterator != in_state.attributes.texture_binding_map.end() )
    {
        result = binding_iterator->second;
    }

    return result;
}

Analysis::APIDumpAnalysis::FormatClass Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::get_format_class(const uint32_t& in_internal_format)
{
    constexpr uint32_t DEPTH_COMPONENT   = 0x1902;
    constexpr uint32_t DEPTH_STENCIL     = 0x84F9;
    constexpr uint32_t FIRST_DEPTH_SIZED = 0x81A5; /* GL_DEPTH_COMPONENT16 to GL_DEPTH_COMPONENT32 */
    constexpr uint32_t LAST_DEPTH_SIZED  = 0x81A7;

    Analysis::APIDumpAnalysis::FormatClass result = Analysis::APIDumpAnalysis::FormatClass::COLOR;

    if (in_internal_format == DEPTH_COMPONENT                                                  ||
        (in_internal_format >= FIRST_DEPTH_SIZED && in_internal_format <= LAST_DEPTH_SIZED) )
    {
        result = Analysis::APIDumpAnalysis::FormatClass::DEPTH;
    }
    else
    if (in_internal_format == DEPTH_STENCIL)
    {
        result = Analysis::APIDumpAnalysis::FormatClass::DEPTH_STENCIL;
    }

    return result;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::get_n_unfollowed_calls() const
{
    return m_n_unfollowed_calls;
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::handle_active_texture(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                   GlContextState*                              inout_state_ptr)
{
    constexpr uint32_t TEXTURE0 = 0x84C0;

    std::array<uint32_t, 1> unit_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                         &unit_array) )
    {
        inout_state_ptr->attributes.active_texture_unit = unit_array.at(0) - TEXTURE0;
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::handle_bind_texture(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                 GlContextState*                              inout_state_ptr)
{
    constexpr uint32_t TEXTURE_2D = 0x0DE1;

    std::array<uint32_t, 2> bind_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                         &bind_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (bind_array.at(0) == TEXTURE_2D)
    {
        const uint32_t unit = inout_state_ptr->attributes.active_texture_unit;

        inout_state_ptr->attributes.texture_binding_map[unit] = bind_array.at(1);
    }
    else
    if (bind_array.at(1) != 0)
    {
        /* A texture of another target (one and three dimensions, cube maps, arrays, multisampled ones) is not followed; the default texture of one is not sampled. */
        UnsupportedUse::report_with_value(in_call.func,
                                          "a texture of a target other than GL_TEXTURE_2D is bound (the target)",
                                          bind_array.at(0) );
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::handle_delete_textures(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                    const Capture::APIDumpLoader::Workload&      in_workload)
{
    constexpr size_t COUNT_ARGUMENT = 0;
    constexpr size_t NAMES_ARGUMENT = 1;

    const CallView                           view(in_call,
                                                  in_workload,
                                                  in_call.arg_in_vec.size() );
    const Capture::APIDumpLoader::DataChunk* chunk_ptr = view.get_chunk_ptr(NAMES_ARGUMENT);
    int32_t                                  count     = 0;
    bool                                     is_read   = (in_call.arg_in_vec.size() > COUNT_ARGUMENT &&
                                                          chunk_ptr                 != nullptr);

    if (is_read)
    {
        is_read = APIInterceptor::read_number(in_call.arg_in_vec.at(COUNT_ARGUMENT),
                                              &count);
    }

    if (is_read)
    {
        is_read = (count            >= 0                                           &&
                   chunk_ptr->size() >= static_cast<size_t>(count) * sizeof(uint32_t) );
    }

    if (is_read == false)
    {
        m_n_unfollowed_calls++;
    }

    for (int32_t n_name = 0;
                 is_read    &&
                 n_name < count;
               ++n_name)
    {
        uint32_t name = 0;

        ::memcpy(&name,
                 chunk_ptr->data() + static_cast<size_t>(n_name) * sizeof(name),
                 sizeof(name) );

        const auto texture_iterator = m_texture_map.find(name);

        if (texture_iterator != m_texture_map.end() )
        {
            m_builder_ptr->on_destroy_resource(texture_iterator->second.resource_id);

            m_texture_map.erase(texture_iterator);
        }

        m_texture_parameter_map.erase(name);
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::handle_generate_mipmap(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                    const GlContextState&                        in_state)
{
    constexpr uint32_t TEXTURE_2D = 0x0DE1;

    std::array<uint32_t, 1> target_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                         &target_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (target_array.at(0) == TEXTURE_2D)
    {
        generate_mipmap_levels(in_state);
    }
    else
    {
        UnsupportedUse::report_with_value(in_call.func,
                                          "the levels of a texture of a target other than GL_TEXTURE_2D are made (the target)",
                                          target_array.at(0) );
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::handle_pixel_store(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                GlContextState*                              inout_state_ptr)
{
    constexpr uint32_t UNPACK_ALIGNMENT  = 0x0CF5;
    constexpr uint32_t UNPACK_ROW_LENGTH = 0x0CF2;

    std::array<int32_t, 2> parameter_array = {};

    /* The values that are not followed (the packing of what is read back, the skips) do not touch how an upload is laid out here. */
    if (GlArgumentReader::read_arguments(in_call,
                                         &parameter_array) )
    {
        switch (static_cast<uint32_t>(parameter_array.at(0) ) )
        {
            case UNPACK_ALIGNMENT:
            {
                inout_state_ptr->pixel_store.unpack_alignment = static_cast<uint32_t>(parameter_array.at(1) );

                break;
            }

            case UNPACK_ROW_LENGTH:
            {
                inout_state_ptr->pixel_store.unpack_row_length = static_cast<uint32_t>(parameter_array.at(1) );

                break;
            }

            default:
            {
                break;
            }
        }
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::handle_tex_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              const Capture::APIDumpLoader::Workload&      in_workload,
                                                                              GlContextState*                              inout_state_ptr)
{
    /* glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels). */
    constexpr uint32_t PROXY_TEXTURE_2D = 0x8064;
    constexpr uint32_t TEXTURE_2D       = 0x0DE1;

    std::array<uint32_t, 8> image_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                         &image_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (image_array.at(0) == PROXY_TEXTURE_2D)
    {
        /* A question of what the driver can make: nothing is changed. */
    }
    else
    if (image_array.at(0) != TEXTURE_2D)
    {
        /* The faces of cube maps, one and three dimensions and the like are not followed. */
        UnsupportedUse::report_with_value(in_call.func,
                                          "a level of a texture of a target other than GL_TEXTURE_2D is given (the target)",
                                          image_array.at(0) );
    }
    else
    {
        const uint32_t level = image_array.at(1);
        GlTextureLevel texture_level;

        texture_level.height          = image_array.at(4);
        texture_level.internal_format = image_array.at(2);
        texture_level.width           = image_array.at(3);

        if (level == 0)
        {
            specify_texture(*inout_state_ptr,
                            texture_level.internal_format,
                            texture_level.width,
                            texture_level.height);
        }

        record_texture_level(*inout_state_ptr,
                             level,
                             texture_level);

        /* The call gives the pixels of all of the level, which it makes the size it says: all of the texture, if the level is the only one that it has. A texture
         * with other levels keeps them, so what is given to this one is told no more than it was.
         */
        const uint32_t                             bound_name       = get_bound_texture_name(*inout_state_ptr);
        const auto                                 bound_iterator   = m_texture_map.find(bound_name);
        const bool                                 has_only_level   = (bound_iterator != m_texture_map.end() &&
                                                                       bound_iterator->second.level_map.size() <= 1);
        Analysis::APIDumpAnalysis::AccessRectangle level_rectangle;

        if (has_only_level)
        {
            level_rectangle.height   = texture_level.height;
            level_rectangle.is_known = true;
            level_rectangle.width    = texture_level.width;
        }

        upload_texture_level(in_call,
                             in_workload,
                             *inout_state_ptr,
                             level,
                             (has_only_level) ? Analysis::APIDumpAnalysis::AccessCoverage::FULL
                                              : Analysis::APIDumpAnalysis::AccessCoverage::UNKNOWN,
                             level_rectangle);

        report_upload(in_call,
                      in_workload,
                      *inout_state_ptr,
                      level,
                      0,
                      0,
                      texture_level.width,
                      texture_level.height,
                      image_array.at(6),
                      image_array.at(7) );

        generate_mipmaps_if_automatic(*inout_state_ptr,
                                      level);
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::handle_tex_parameter(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                  const GlContextState&                        in_state)
{
    /* glTexParameter(target, pname, param): of all the parameters, three change what the levels of a texture hold or which of them there are. */
    constexpr uint32_t BASE_LEVEL      = 0x813C;
    constexpr uint32_t GENERATE_MIPMAP = 0x8191;
    constexpr uint32_t MAX_LEVEL       = 0x813D;
    constexpr uint32_t TEXTURE_2D      = 0x0DE1;

    std::array<uint32_t, 2> parameter_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                         &parameter_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const uint32_t target               = parameter_array.at(0);
        const uint32_t parameter_name                 = parameter_array.at(1);
        const bool     is_level_parameter   = (parameter_name == BASE_LEVEL      ||
                                               parameter_name == GENERATE_MIPMAP ||
                                               parameter_name == MAX_LEVEL);
        const bool     is_array_form        = (in_call.func == APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERFV ||
                                               in_call.func == APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERIV);
        double         value                = 0.0;

        if (is_level_parameter == false)
        {
            /* A filter, a wrap, a border: it does not change what the levels hold. */
        }
        else
        if (target != TEXTURE_2D)
        {
            UnsupportedUse::report_with_value(in_call.func,
                                              "a parameter that decides the levels of a texture is set for a target other than GL_TEXTURE_2D (the target)",
                                              target);
        }
        else
        if (is_array_form)
        {
            UnsupportedUse::report_with_value(in_call.func,
                                              "a parameter that decides the levels of a texture is set with an array, which is not followed (the parameter)",
                                              parameter_name);
        }
        else
        if (in_call.arg_in_vec.size() < 3 ||
            APIInterceptor::read_number(in_call.arg_in_vec.at(2),
                                        &value) == false)
        {
            m_n_unfollowed_calls++;
        }
        else
        {
            const uint32_t texture_name = get_bound_texture_name(in_state);

            if (parameter_name == BASE_LEVEL &&
                value != 0.0)
            {
                UnsupportedUse::report_with_value(in_call.func,
                                                  "a first level other than the first is chosen (GL_TEXTURE_BASE_LEVEL)",
                                                  static_cast<uint64_t>(value) );
            }
            else
            if (parameter_name == GENERATE_MIPMAP)
            {
                m_texture_parameter_map[texture_name].generates_mipmaps = (value != 0.0);
            }
            else
            if (parameter_name == MAX_LEVEL)
            {
                m_texture_parameter_map[texture_name].max_level = static_cast<uint32_t>(value);
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::handle_tex_sub_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                  const Capture::APIDumpLoader::Workload&      in_workload,
                                                                                  GlContextState*                              inout_state_ptr)
{
    /* glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels): a write to a part of the level, to the dependencies and to the textures. */
    constexpr uint32_t PROXY_TEXTURE_2D = 0x8064;
    constexpr uint32_t TEXTURE_2D       = 0x0DE1;

    std::array<uint32_t, 8> image_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                         &image_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (image_array.at(0) == PROXY_TEXTURE_2D)
    {
        /* A question of what the driver can make: nothing is changed. */
    }
    else
    if (image_array.at(0) != TEXTURE_2D)
    {
        UnsupportedUse::report_with_value(in_call.func,
                                          "a part of a level of a texture of a target other than GL_TEXTURE_2D is given pixels (the target)",
                                          image_array.at(0) );
    }
    else
    {
        const uint32_t level            = image_array.at(1);
        const uint32_t name             = get_bound_texture_name(*inout_state_ptr);
        const auto     texture_iterator = m_texture_map.find(name);
        const bool     is_level_known   = (texture_iterator != m_texture_map.end() &&
                                           texture_iterator->second.level_map.count(level) != 0);

        /* A part of a level that the texture has not been given. */
        if (is_level_known)
        {
            /* What the pixels are laid over is changed: a level whose content rendering made is no longer what a snapshot of it was, and is read again. */
            if (texture_iterator->second.render_level_set.count(level) != 0)
            {
                texture_iterator->second.dirty_level_set.insert(level);
            }

            /* The call gives the pixels of a rectangle of the level. */
            Analysis::APIDumpAnalysis::AccessRectangle sub_image_rectangle;

            sub_image_rectangle.height   = image_array.at(5);
            sub_image_rectangle.is_known = true;
            sub_image_rectangle.width    = image_array.at(4);
            sub_image_rectangle.x        = image_array.at(2);
            sub_image_rectangle.y        = image_array.at(3);

            upload_texture_level(in_call,
                                 in_workload,
                                 *inout_state_ptr,
                                 level,
                                 Analysis::APIDumpAnalysis::AccessCoverage::PARTIAL,
                                 sub_image_rectangle);

            report_upload(in_call,
                          in_workload,
                          *inout_state_ptr,
                          level,
                          image_array.at(2),
                          image_array.at(3),
                          image_array.at(4),
                          image_array.at(5),
                          image_array.at(6),
                          image_array.at(7) );

            generate_mipmaps_if_automatic(*inout_state_ptr,
                                          level);
        }
        else
        {
            m_n_unfollowed_calls++;
        }
    }
}

bool Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::is_unsupported_texture_function(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    /* What makes or changes a texture that is not a GL_TEXTURE_2D, or in a way that is not followed: compressed images, storage that is fixed, multisampled and
     * three dimensional textures, one dimension, buffer textures, views, images, a texture that is a part of a framebuffer, the calls that name a texture
     * directly, and what clears, copies or invalidates the content of one.
     */
    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLBINDIMAGETEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLBINDIMAGETEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTUREUNIT:
        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLCLEARTEXIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLCLEARTEXSUBIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXSUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXSUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXSUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXTURESUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXTURESUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOMPRESSEDTEXTURESUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYIMAGESUBDATA:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXTURESUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXTURESUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXTURESUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLCREATETEXTURES:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE1D:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE2D:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURE3D:
        case APIInterceptor::APIFUNCTION_GL_GLFRAMEBUFFERTEXTURELAYER:
        case APIInterceptor::APIFUNCTION_GL_GLGENERATETEXTUREMIPMAP:
        case APIInterceptor::APIFUNCTION_GL_GLINVALIDATETEXIMAGEPROC:
        case APIInterceptor::APIFUNCTION_GL_GLINVALIDATETEXSUBIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLNAMEDFRAMEBUFFERTEXTURE:
        case APIInterceptor::APIFUNCTION_GL_GLNAMEDFRAMEBUFFERTEXTURELAYER:
        case APIInterceptor::APIFUNCTION_GL_GLTEXBUFFER:
        case APIInterceptor::APIFUNCTION_GL_GLTEXBUFFERRANGE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2DMULTISMAPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE3DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE3DMULTISMAPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE2DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSTORAGE3DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREBUFFER:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREBUFFERRANGE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE2DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESTORAGE3DMULTISAMPLE:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESUBIMAGE1D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTURESUBIMAGE3D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXTUREVIEW:
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

Analysis::APIDumpTextures::TextureEvent Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::make_texture_event(const GlTexture& in_texture,
                                                                                                                  const uint32_t&  in_level,
                                                                                                                  const uint32_t&  in_x_offset,
                                                                                                                  const uint32_t&  in_y_offset,
                                                                                                                  const uint32_t&  in_width,
                                                                                                                  const uint32_t&  in_height) const
{
    const GlTextureLevel&                   level = in_texture.level_map.at(in_level);
    Analysis::APIDumpTextures::TextureEvent result;

    result.destination.mip_level   = in_level;
    result.destination.resource_id = in_texture.resource_id;
    result.height                  = in_height;
    result.level_api_format        = level.internal_format;
    result.level_format            = GlTextureFormatMapper::get_texture_format(level.internal_format);
    result.level_height            = level.height;
    result.level_width             = level.width;
    result.width                   = in_width;
    result.x                       = in_x_offset;
    result.y                       = in_y_offset;

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::record_texture_level(const GlContextState& in_state,
                                                                                  const uint32_t&       in_level,
                                                                                  const GlTextureLevel& in_texture_level)
{
    const uint32_t name             = get_bound_texture_name(in_state);
    const auto     texture_iterator = m_texture_map.find(name);
    const bool     result           = (texture_iterator != m_texture_map.end() );

    if (result)
    {
        texture_iterator->second.level_map[in_level] = in_texture_level;

        /* The call gives the whole level, so what rendering made of it is gone (a copy that follows makes it again). */
        texture_iterator->second.dirty_level_set.erase (in_level);
        texture_iterator->second.render_level_set.erase(in_level);
        texture_iterator->second.render_region_map.erase(in_level);
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::report_render_write(const GlContextState& in_state,
                                                                                 const uint32_t&       in_level,
                                                                                 const uint32_t&       in_x_offset,
                                                                                 const uint32_t&       in_y_offset,
                                                                                 const uint32_t&       in_width,
                                                                                 const uint32_t&       in_height)
{
    const uint32_t name             = get_bound_texture_name(in_state);
    const auto     texture_iterator = m_texture_map.find(name);
    const bool     wants_events     = m_builder_ptr->wants_texture_events();
    const bool     is_level_known   = (texture_iterator != m_texture_map.end() &&
                                       texture_iterator->second.level_map.count(in_level) != 0);

    /* What rendering made of the level is not in the workload, so it is a content to be read when something first reads it. */
    if (is_level_known)
    {
        GlTexture&       texture        = texture_iterator->second;
        const auto       region_iterator = texture.render_region_map.find(in_level);
        GlTextureRegion  region;

        texture.dirty_level_set.insert (in_level);
        texture.render_level_set.insert(in_level);

        /* What rendering has made of the level is in the one rectangle that holds all the copies: the rest of it is storage that was given and not filled (its
         * content is not defined), or pixels that were uploaded (in the workload), and a snapshot has no use for either.
         */
        region.height = in_height;
        region.width  = in_width;
        region.x      = in_x_offset;
        region.y      = in_y_offset;

        if (region_iterator != texture.render_region_map.end() )
        {
            const GlTextureRegion& known   = region_iterator->second;
            const uint32_t         left    = (known.x < region.x) ? known.x : region.x;
            const uint32_t         bottom  = (known.y < region.y) ? known.y : region.y;
            const uint32_t         right   = (known.x + known.width  > region.x + region.width)  ? known.x + known.width
                                                                                                 : region.x + region.width;
            const uint32_t         top     = (known.y + known.height > region.y + region.height) ? known.y + known.height
                                                                                                 : region.y + region.height;

            region.height = top   - bottom;
            region.width  = right - left;
            region.x      = left;
            region.y      = bottom;
        }

        texture.render_region_map[in_level] = region;
    }

    if (wants_events &&
        is_level_known)
    {
        /* What is copied is the window, which has no source in the file of the textures: it is never stored. */
        Analysis::APIDumpTextures::TextureEvent event = make_texture_event(texture_iterator->second,
                                                                           in_level,
                                                                           in_x_offset,
                                                                           in_y_offset,
                                                                           in_width,
                                                                           in_height);

        event.kind = Analysis::APIDumpTextures::TextureEventKind::RENDER_WRITE;

        m_builder_ptr->on_texture_event(event,
                                        nullptr,
                                        0,
                                        0);
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::report_snapshots(const GlContextState& in_state)
{
    constexpr uint32_t TEXTURE_2D = 0x0DE1;

    const bool wants_events = m_builder_ptr->wants_texture_events();

    /* A draw reads the textures of the units that have the target enabled; what rendering made of a level of them, and nothing has read since, is a content that is
     * not in the workload: it is what a snapshot is for, once for each time it has changed. Which levels a draw reads depends on how far the texture is from the
     * eye, so each level that rendering has changed is taken to be read.
     */
    for (const auto& binding_pair : in_state.attributes.texture_binding_map)
    {
        const bool is_enabled       = in_state.pipeline_state.is_unit_capability_enabled(binding_pair.first,
                                                                                         TEXTURE_2D);
        const auto texture_iterator = m_texture_map.find(binding_pair.second);

        if (is_enabled                              &&
            texture_iterator != m_texture_map.end() &&
            texture_iterator->second.dirty_level_set.empty() == false)
        {
            GlTexture& texture = texture_iterator->second;

            for (const uint32_t level : texture.dirty_level_set)
            {
                if (wants_events &&
                    texture.level_map.count(level) != 0)
                {
                    /* The region that rendering has made of the level; the whole of it if it is not known (a level that rendering made without a copy). */
                    const GlTextureLevel& texture_level   = texture.level_map.at(level);
                    const auto            region_iterator = texture.render_region_map.find(level);
                    GlTextureRegion       region;

                    region.height = texture_level.height;
                    region.width  = texture_level.width;

                    if (region_iterator != texture.render_region_map.end() )
                    {
                        region = region_iterator->second;
                    }

                    Analysis::APIDumpTextures::TextureEvent event = make_texture_event(texture,
                                                                                       level,
                                                                                       region.x,
                                                                                       region.y,
                                                                                       region.width,
                                                                                       region.height);

                    event.kind = Analysis::APIDumpTextures::TextureEventKind::SNAPSHOT;

                    m_builder_ptr->on_texture_event(event,
                                                    nullptr,
                                                    0,
                                                    0);
                }
            }

            texture.dirty_level_set.clear();
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::report_upload(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                           const Capture::APIDumpLoader::Workload&      in_workload,
                                                                           const GlContextState&                        in_state,
                                                                           const uint32_t&                              in_level,
                                                                           const uint32_t&                              in_x_offset,
                                                                           const uint32_t&                              in_y_offset,
                                                                           const uint32_t&                              in_width,
                                                                           const uint32_t&                              in_height,
                                                                           const uint32_t&                              in_format,
                                                                           const uint32_t&                              in_type)
{
    constexpr size_t PIXELS_ARGUMENT = 8;

    const uint32_t name             = get_bound_texture_name(in_state);
    const auto     texture_iterator = m_texture_map.find(name);
    const bool     wants_events     = m_builder_ptr->wants_texture_events();

    if (wants_events                                                     &&
        texture_iterator != m_texture_map.end()                          &&
        texture_iterator->second.level_map.count(in_level) != 0)
    {
        const CallView                           view(in_call,
                                                      in_workload,
                                                      in_call.arg_in_vec.size() );
        const Capture::APIDumpLoader::DataChunk* chunk_ptr         = view.get_chunk_ptr(PIXELS_ARGUMENT);
        const uint32_t                           n_bytes_per_texel = GlTextureFormatMapper::get_n_bytes_per_texel(in_format,
                                                                                                                  in_type);
        const uint8_t*                           data_ptr          = nullptr;
        uint32_t                                 data_chunk_id     = 0;
        size_t                                   n_data_bytes      = 0;

        Analysis::APIDumpTextures::TextureEvent event = make_texture_event(texture_iterator->second,
                                                                           in_level,
                                                                           in_x_offset,
                                                                           in_y_offset,
                                                                           in_width,
                                                                           in_height);

        event.kind                     = Analysis::APIDumpTextures::TextureEventKind::UPLOAD;
        event.layout.api_format        = in_format;
        event.layout.api_type          = in_type;
        event.layout.row_pitch_n_bytes = GlTextureFormatMapper::get_unpack_row_pitch(in_width,
                                                                                     n_bytes_per_texel,
                                                                                     in_state.pixel_store.unpack_row_length,
                                                                                     in_state.pixel_store.unpack_alignment);

        /* The pixels are in the call, or there are none: the level is given a size and no data. The dumper makes a data chunk of every pointer to pixels that is not
         * an offset into a buffer, and of a null pointer one that has no bytes (see MemoryChunk): a chunk with no bytes is a null pointer, that is, storage that is
         * given and not filled (a texture that is to be copied to). Gerbera makes one, of 1024 by 512 texels, and the frame fills a part of it with a copy.
         */
        if (chunk_ptr != nullptr &&
            chunk_ptr->empty() == false)
        {
            data_chunk_id = in_call.arg_in_vec.at(PIXELS_ARGUMENT).get_data_chunk_id();
            data_ptr      = chunk_ptr->data();
            n_data_bytes  = chunk_ptr->size();
        }

        m_builder_ptr->on_texture_event(event,
                                        data_ptr,
                                        n_data_bytes,
                                        data_chunk_id);
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::specify_texture(const GlContextState& in_state,
                                                                             const uint32_t&       in_internal_format,
                                                                             const uint32_t&       in_width,
                                                                             const uint32_t&       in_height)
{
    const uint32_t name             = get_bound_texture_name(in_state);
    const auto     texture_iterator = m_texture_map.find(name);
    const bool     is_known         = (texture_iterator != m_texture_map.end() );
    bool           is_same          = false;

    if (is_known)
    {
        is_same = (texture_iterator->second.height          == in_height          &&
                   texture_iterator->second.internal_format == in_internal_format &&
                   texture_iterator->second.width           == in_width);
    }

    /* The size of the first level is the size of the texture: a texture that is given another one is another resource. */
    if (is_same == false)
    {
        Analysis::APIDumpAnalysis::ResourceDescription description;
        GlTexture                                      texture;

        if (is_known)
        {
            m_builder_ptr->on_destroy_resource(texture_iterator->second.resource_id);
        }

        description.format.format_class = get_format_class(in_internal_format);
        description.format.raw_value    = in_internal_format;
        description.height              = in_height;
        description.kind                = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D;
        description.width               = in_width;

        texture.height          = in_height;
        texture.internal_format = in_internal_format;
        texture.resource_id     = m_builder_ptr->on_create_resource(description,
                                                                    "texture");
        texture.width           = in_width;

        m_texture_map[name] = texture;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GlTextureHandler::upload_texture_level(const Capture::APIDumpLoader::DumpedAPICall&      in_call,
                                                                                  const Capture::APIDumpLoader::Workload&           in_workload,
                                                                                  const GlContextState&                             in_state,
                                                                                  const uint32_t&                                   in_level,
                                                                                  const Analysis::APIDumpAnalysis::AccessCoverage&  in_coverage,
                                                                                  const Analysis::APIDumpAnalysis::AccessRectangle& in_rectangle)
{
    constexpr size_t PIXELS_ARGUMENT = 8;

    const uint32_t name             = get_bound_texture_name(in_state);
    const auto     texture_iterator = m_texture_map.find(name);

    if (texture_iterator == m_texture_map.end() )
    {
        /* A level of a texture whose first level has not been given. */
        m_n_unfollowed_calls++;
    }
    else
    {
        const CallView                           view(in_call,
                                                      in_workload,
                                                      in_call.arg_in_vec.size() );
        Analysis::APIDumpAnalysis::DataReference data;
        Analysis::APIDumpAnalysis::ResourceView  destination;

        destination.mip_level   = in_level;
        destination.resource_id = texture_iterator->second.resource_id;

        /* The pixels are in the call, or a null pointer: the level is given a size and no data. */
        if (view.get_data_reference(PIXELS_ARGUMENT,
                                    &data) )
        {
            m_builder_ptr->on_upload(destination,
                                     data,
                                     in_coverage,
                                     in_rectangle);
        }
    }
}
