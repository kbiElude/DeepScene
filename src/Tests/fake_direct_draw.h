/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Objects which have the virtual tables of DirectDraw 7 surfaces, a DirectDraw 7 object and a Direct3D 7 device, and keep what they are called
 * with, for the tests of what the converter does to them. Only the methods that the converter calls are there; the first member of each is the
 * pointer to its table, as the code that calls them expects.
 */
#ifndef TESTS_FAKE_DIRECT_DRAW_H
#define TESTS_FAKE_DIRECT_DRAW_H

#include "Common/types.h"
#include "Interception/Hooks/D3D7/types.h"
#include <array>
#include <memory>
#include <string>
#include <stdint.h>
#include <string.h>
#include <vector>

namespace Tests
{
    namespace D3D7 = Interception::Hooks::D3D7;

    constexpr size_t FAKE_TABLE_N_ENTRIES = 64;

    struct FakeDirectDraw;

    struct FakeSurface
    {
        void**                    vtable_ptr = nullptr; /* must be the first member */
        std::vector<FakeSurface*> attached_ptr_vec;
        D3D7::DDSURFACEDESC2      description = {};
        FakeDirectDraw*           owner_ptr   = nullptr;
        uint32_t                  n_references = 1;
        uint32_t                  n_add_refs   = 0;
        std::vector<std::string>  call_vec;             /* what was called with it, as text */
    };

    struct FakeDirectDraw
    {
        void**                                    vtable_ptr = nullptr; /* must be the first member */
        std::vector<D3D7::DDSURFACEDESC2>         created_description_vec;
        uint32_t                                  cooperative_level = 0;
        bool                                      fails_to_create   = false;
        bool                                      has_cooperative_level = false;
        uint32_t                                  n_display_mode_calls = 0;
        std::vector<std::unique_ptr<FakeSurface>> surface_ptr_vec;
    };

    struct FakeDevice
    {
        void**                    vtable_ptr = nullptr; /* must be the first member */
        std::vector<std::string>  call_vec;             /* in order: what was called, with the arguments that matter as text */
        std::vector<std::vector<uint8_t> > vertex_vec_vec; /* what the draws were given */
        D3D7::D3DVIEWPORT7        viewport = {};
        std::vector<D3D7::D3DRECT> last_clear_rect_vec;
    };

    inline std::array<void*, FAKE_TABLE_N_ENTRIES>& get_surface_table();
    inline std::array<void*, FAKE_TABLE_N_ENTRIES>& get_direct_draw_table();
    inline std::array<void*, FAKE_TABLE_N_ENTRIES>& get_device_table();

    inline FakeSurface* make_surface(FakeDirectDraw*              inout_direct_draw_ptr,
                                     const D3D7::DDSURFACEDESC2&  in_description)
    {
        auto surface_ptr = std::unique_ptr<FakeSurface>(new FakeSurface() );

        surface_ptr->description = in_description;
        surface_ptr->owner_ptr   = inout_direct_draw_ptr;
        surface_ptr->vtable_ptr  = get_surface_table().data();

        inout_direct_draw_ptr->surface_ptr_vec.push_back(std::move(surface_ptr) );

        return inout_direct_draw_ptr->surface_ptr_vec.back().get();
    }

    inline HRESULT __stdcall fake_surface_add_attached(void* in_self_ptr,
                                                       void* in_attached_ptr)
    {
        static_cast<FakeSurface*>(in_self_ptr)->attached_ptr_vec.push_back(static_cast<FakeSurface*>(in_attached_ptr) );

        return 0;
    }

    inline ULONG __stdcall fake_surface_add_ref(void* in_self_ptr)
    {
        auto* surface_ptr = static_cast<FakeSurface*>(in_self_ptr);

        surface_ptr->n_add_refs++;

        return ++surface_ptr->n_references;
    }

    inline HRESULT __stdcall fake_surface_blt(void*       in_self_ptr,
                                              const RECT* in_destination_rect_ptr,
                                              void*       in_source_ptr,
                                              const RECT* in_source_rect_ptr,
                                              DWORD       in_flags,
                                              const void* in_fx_ptr)
    {
        (void) in_destination_rect_ptr;
        (void) in_source_rect_ptr;
        (void) in_fx_ptr;

        static_cast<FakeSurface*>(in_self_ptr)->call_vec.push_back("Blt source " + std::to_string(reinterpret_cast<uintptr_t>(in_source_ptr) ) + " flags " + std::to_string(in_flags) );

        return 0;
    }

    inline HRESULT __stdcall fake_surface_get_attached(void*                      in_self_ptr,
                                                       const D3D7::DDSCAPS2*      in_caps_ptr,
                                                       void**                     out_attached_ptr_ptr)
    {
        auto*   surface_ptr = static_cast<FakeSurface*>(in_self_ptr);
        HRESULT result      = static_cast<HRESULT>(0x887602e8); /* DDERR_NOTFOUND */

        for (auto* attached_ptr : surface_ptr->attached_ptr_vec)
        {
            if ( (attached_ptr->description.ddsCaps.dwCaps & in_caps_ptr->dwCaps) == in_caps_ptr->dwCaps)
            {
                attached_ptr->n_references++;

                *out_attached_ptr_ptr = attached_ptr;
                result                = 0;

                break;
            }
        }

        return result;
    }

    inline HRESULT __stdcall fake_surface_get_description(void*                  in_self_ptr,
                                                          D3D7::DDSURFACEDESC2*  out_description_ptr)
    {
        *out_description_ptr = static_cast<FakeSurface*>(in_self_ptr)->description;

        return 0;
    }

    inline ULONG __stdcall fake_surface_release(void* in_self_ptr)
    {
        auto* surface_ptr = static_cast<FakeSurface*>(in_self_ptr);

        return (surface_ptr->n_references > 0) ? --surface_ptr->n_references
                                               : 0;
    }

    inline HRESULT __stdcall fake_direct_draw_create_surface(void*                       in_self_ptr,
                                                             const D3D7::DDSURFACEDESC2* in_description_ptr,
                                                             void**                      out_surface_ptr_ptr,
                                                             void*                       in_outer_ptr)
    {
        auto*   direct_draw_ptr = static_cast<FakeDirectDraw*>(in_self_ptr);
        HRESULT result          = static_cast<HRESULT>(0x88760218); /* DDERR_OUTOFVIDEOMEMORY */

        (void) in_outer_ptr;

        direct_draw_ptr->created_description_vec.push_back(*in_description_ptr);

        if (direct_draw_ptr->fails_to_create == false)
        {
            *out_surface_ptr_ptr = make_surface(direct_draw_ptr,
                                                *in_description_ptr);
            result               = 0;
        }

        return result;
    }

    inline HRESULT __stdcall fake_direct_draw_set_cooperative_level(void* in_self_ptr,
                                                                    void* in_window_ptr,
                                                                    DWORD in_flags)
    {
        auto* direct_draw_ptr = static_cast<FakeDirectDraw*>(in_self_ptr);

        (void) in_window_ptr;

        direct_draw_ptr->cooperative_level     = in_flags;
        direct_draw_ptr->has_cooperative_level = true;

        return 0;
    }

    inline HRESULT __stdcall fake_direct_draw_set_display_mode(void* in_self_ptr,
                                                               DWORD in_width,
                                                               DWORD in_height,
                                                               DWORD in_bpp,
                                                               DWORD in_refresh,
                                                               DWORD in_flags)
    {
        (void) in_width;
        (void) in_height;
        (void) in_bpp;
        (void) in_refresh;
        (void) in_flags;

        static_cast<FakeDirectDraw*>(in_self_ptr)->n_display_mode_calls++;

        return 0;
    }

    inline HRESULT __stdcall fake_device_clear(void*                      in_self_ptr,
                                               DWORD                      in_count,
                                               const D3D7::D3DRECT*       in_rects_ptr,
                                               DWORD                      in_flags,
                                               DWORD                      in_color,
                                               float                      in_z,
                                               DWORD                      in_stencil)
    {
        auto* device_ptr = static_cast<FakeDevice*>(in_self_ptr);

        (void) in_flags;
        (void) in_color;
        (void) in_z;
        (void) in_stencil;

        device_ptr->call_vec.push_back("Clear");

        device_ptr->last_clear_rect_vec.assign(in_rects_ptr,
                                               in_rects_ptr + in_count);

        return 0;
    }

    inline HRESULT __stdcall fake_device_draw_primitive(void*  in_self_ptr,
                                                        DWORD  in_type,
                                                        DWORD  in_fvf,
                                                        void*  in_vertices_ptr,
                                                        DWORD  in_n_vertices,
                                                        DWORD  in_flags)
    {
        constexpr size_t VERTEX_N_BYTES = 32; /* of the format of the tests */

        auto* device_ptr = static_cast<FakeDevice*>(in_self_ptr);

        (void) in_type;
        (void) in_fvf;
        (void) in_flags;

        device_ptr->call_vec.push_back("DrawPrimitive");

        device_ptr->vertex_vec_vec.emplace_back(static_cast<const uint8_t*>(in_vertices_ptr),
                                                static_cast<const uint8_t*>(in_vertices_ptr) + in_n_vertices * VERTEX_N_BYTES);

        return 0;
    }

    inline HRESULT __stdcall fake_device_get_viewport(void*                in_self_ptr,
                                                      D3D7::D3DVIEWPORT7*  out_viewport_ptr)
    {
        *out_viewport_ptr = static_cast<FakeDevice*>(in_self_ptr)->viewport;

        return 0;
    }

    inline HRESULT __stdcall fake_device_set_render_target(void* in_self_ptr,
                                                           void* in_surface_ptr,
                                                           DWORD in_flags)
    {
        (void) in_flags;

        static_cast<FakeDevice*>(in_self_ptr)->call_vec.push_back("SetRenderTarget " + std::to_string(reinterpret_cast<uintptr_t>(in_surface_ptr) ) );

        return 0;
    }

    inline HRESULT __stdcall fake_device_set_texture(void*  in_self_ptr,
                                                     DWORD  in_stage,
                                                     void*  in_texture_ptr)
    {
        static_cast<FakeDevice*>(in_self_ptr)->call_vec.push_back("SetTexture " + std::to_string(in_stage) + " " + std::to_string(reinterpret_cast<uintptr_t>(in_texture_ptr) ) );

        return 0;
    }

    inline HRESULT __stdcall fake_device_set_viewport(void*                      in_self_ptr,
                                                      const D3D7::D3DVIEWPORT7*  in_viewport_ptr)
    {
        auto* device_ptr = static_cast<FakeDevice*>(in_self_ptr);

        device_ptr->viewport = *in_viewport_ptr;

        device_ptr->call_vec.push_back("SetViewport " + std::to_string(in_viewport_ptr->dwWidth) + "x" + std::to_string(in_viewport_ptr->dwHeight) );

        return 0;
    }

    inline std::array<void*, FAKE_TABLE_N_ENTRIES>& get_surface_table()
    {
        static std::array<void*, FAKE_TABLE_N_ENTRIES> table = {};

        if (table.at(D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE) == nullptr)
        {
            table.at(D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE) = reinterpret_cast<void*>(&fake_surface_add_attached);
            table.at(D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_ADDREF)             = reinterpret_cast<void*>(&fake_surface_add_ref);
            table.at(D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_BLT)                = reinterpret_cast<void*>(&fake_surface_blt);
            table.at(D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE) = reinterpret_cast<void*>(&fake_surface_get_attached);
            table.at(D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETSURFACEDESC)     = reinterpret_cast<void*>(&fake_surface_get_description);
            table.at(D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE)            = reinterpret_cast<void*>(&fake_surface_release);
        }

        return table;
    }

    inline std::array<void*, FAKE_TABLE_N_ENTRIES>& get_direct_draw_table()
    {
        static std::array<void*, FAKE_TABLE_N_ENTRIES> table = {};

        if (table.at(D3D7::VTABLE_INDEX_IDIRECTDRAW7_CREATESURFACE) == nullptr)
        {
            table.at(D3D7::VTABLE_INDEX_IDIRECTDRAW7_CREATESURFACE)        = reinterpret_cast<void*>(&fake_direct_draw_create_surface);
            table.at(D3D7::VTABLE_INDEX_IDIRECTDRAW7_SETCOOPERATIVELEVEL)  = reinterpret_cast<void*>(&fake_direct_draw_set_cooperative_level);
            table.at(D3D7::VTABLE_INDEX_IDIRECTDRAW7_SETDISPLAYMODE)       = reinterpret_cast<void*>(&fake_direct_draw_set_display_mode);
        }

        return table;
    }

    inline std::array<void*, FAKE_TABLE_N_ENTRIES>& get_device_table()
    {
        static std::array<void*, FAKE_TABLE_N_ENTRIES> table = {};

        if (table.at(D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_DRAWPRIMITIVE) == nullptr)
        {
            table.at(D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_CLEAR)           = reinterpret_cast<void*>(&fake_device_clear);
            table.at(D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_DRAWPRIMITIVE)   = reinterpret_cast<void*>(&fake_device_draw_primitive);
            table.at(D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_GETVIEWPORT)     = reinterpret_cast<void*>(&fake_device_get_viewport);
            table.at(D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETRENDERTARGET) = reinterpret_cast<void*>(&fake_device_set_render_target);
            table.at(D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTEXTURE)      = reinterpret_cast<void*>(&fake_device_set_texture);
            table.at(D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETVIEWPORT)     = reinterpret_cast<void*>(&fake_device_set_viewport);
        }

        return table;
    }

    inline std::unique_ptr<FakeDirectDraw> make_direct_draw()
    {
        auto result_ptr = std::unique_ptr<FakeDirectDraw>(new FakeDirectDraw() );

        result_ptr->vtable_ptr = get_direct_draw_table().data();

        return result_ptr;
    }

    inline std::unique_ptr<FakeDevice> make_device()
    {
        auto result_ptr = std::unique_ptr<FakeDevice>(new FakeDevice() );

        result_ptr->vtable_ptr = get_device_table().data();

        return result_ptr;
    }
}

#endif /* TESTS_FAKE_DIRECT_DRAW_H */
