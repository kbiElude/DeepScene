/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d8_lock_replayer.h"
#include "Interception/Hooks/D3D8/types.h"
#include "Interception/Core/D3D8Utils/surface_layout.h"
#include <string.h>

const int Replay::StereoReplayEngineDLL::D3D8LockReplayer::ARGUMENT_ABSENT = -1;

Replay::StereoReplayEngineDLL::D3D8LockReplayer::D3D8LockReplayer(D3D8ReplayContext* in_context_ptr)
    :m_context_ptr(in_context_ptr)
{
    /* Stub */
}

int32_t Replay::StereoReplayEngineDLL::D3D8LockReplayer::get_dumped_int32(const D3D8ReplayContext& in_context,
                                                                          const size_t&            in_n_argument,
                                                                          const size_t&            in_index)
{
    size_t         n_bytes  = 0;
    const uint8_t* data_ptr = in_context.get_chunk_data_ptr(in_context.get_out_argument_ptr(in_n_argument),
                                                            &n_bytes);
    int32_t        result   = 0;

    if (data_ptr != nullptr                          &&
        n_bytes  >= (in_index + 1) * sizeof(int32_t) )
    {
        ::memcpy(&result,
                 data_ptr + in_index * sizeof(int32_t),
                 sizeof(int32_t) );
    }

    return result;
}

const Replay::StereoReplayEngineDLL::D3D8LockReplayer::LockSpec* Replay::StereoReplayEngineDLL::D3D8LockReplayer::get_lock_spec_ptr(const APIInterceptor::APIFunction& in_function)
{
    static const LockSpec LOCK_SPEC_ARRAY[] =
    {
        {13, APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_LOCK,        false, LockKind::BUFFER, ARGUMENT_ABSENT, 4,               ARGUMENT_ABSENT, 3,               1,               ARGUMENT_ABSENT, 2,               11},
        {0,  APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_UNLOCK,      true,  LockKind::BUFFER, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, 12},
        {8,  APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_LOCKRECT,        false, LockKind::RECT,   ARGUMENT_ABSENT, 3,               ARGUMENT_ABSENT, 1,               ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT,  9},
        {0,  APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_UNLOCKRECT,      true,  LockKind::RECT,   ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, 10},
        {14, APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_LOCKRECT,        false, LockKind::RECT,   ARGUMENT_ABSENT, 4,               1,               2,               ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, 16},
        {0,  APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_UNLOCKRECT,      true,  LockKind::RECT,   ARGUMENT_ABSENT, ARGUMENT_ABSENT, 1,               ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, 17},
        {14, APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DCUBETEXTURE8_LOCKRECT,    false, LockKind::RECT,   1,               5,               2,               3,               ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, 16},
        {0,  APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DCUBETEXTURE8_UNLOCKRECT,  true,  LockKind::RECT,   1,               ARGUMENT_ABSENT, 2,               ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, 17},
        {13, APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_LOCK,       false, LockKind::BUFFER, ARGUMENT_ABSENT, 4,               ARGUMENT_ABSENT, 3,               1,               ARGUMENT_ABSENT, 2,               11},
        {0,  APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_UNLOCK,     true,  LockKind::BUFFER, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, 12},
        {8,  APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVOLUME8_LOCKBOX,          false, LockKind::BOX,    ARGUMENT_ABSENT, 3,               ARGUMENT_ABSENT, 1,               ARGUMENT_ABSENT, 2,               ARGUMENT_ABSENT,  9},
        {0,  APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVOLUME8_UNLOCKBOX,        true,  LockKind::BOX,    ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, 10},
        {14, APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVOLUMETEXTURE8_LOCKBOX,   false, LockKind::BOX,    ARGUMENT_ABSENT, 4,               1,               2,               ARGUMENT_ABSENT, 3,               ARGUMENT_ABSENT, 16},
        {0,  APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVOLUMETEXTURE8_UNLOCKBOX, true,  LockKind::BOX,    ARGUMENT_ABSENT, ARGUMENT_ABSENT, 1,               ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, ARGUMENT_ABSENT, 17},

    };

    const LockSpec* result_ptr = nullptr;

    for (const auto& spec : LOCK_SPEC_ARRAY)
    {
        if (spec.function == in_function)
        {
            result_ptr = &spec;

            break;
        }
    }

    return result_ptr;
}

uint32_t Replay::StereoReplayEngineDLL::D3D8LockReplayer::get_u32_argument(const D3D8ReplayContext& in_context,
                                                                           const int&               in_n_argument)
{
    const auto argument_ptr = (in_n_argument != ARGUMENT_ABSENT) ? in_context.get_in_argument_ptr(static_cast<size_t>(in_n_argument) )
                                                                 : nullptr;

    return (argument_ptr != nullptr) ? argument_ptr->get_u32()
                                     : 0;
}

bool Replay::StereoReplayEngineDLL::D3D8LockReplayer::is_lock_function(const APIInterceptor::APIFunction& in_function)
{
    return (get_lock_spec_ptr(in_function) != nullptr);
}

bool Replay::StereoReplayEngineDLL::D3D8LockReplayer::is_unlock_function(const APIInterceptor::APIFunction& in_function)
{
    const auto spec_ptr = get_lock_spec_ptr(in_function);

    return (spec_ptr             != nullptr &&
            spec_ptr->is_unlock);
}

bool Replay::StereoReplayEngineDLL::D3D8LockReplayer::lock(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                           std::string*                                 out_error_ptr)
{
    void*      object_ptr = nullptr;
    bool       result     = false;
    const auto spec_ptr   = get_lock_spec_ptr(in_call.func);

    if (spec_ptr == nullptr)
    {
        *out_error_ptr = "Not a lock.";

        goto end;
    }

    if (m_context_ptr->get_live_object(in_call.arg_in_vec.at(0),
                                      &object_ptr,
                                       out_error_ptr) == false)
    {
        goto end;
    }

    {
        const LockKey key(object_ptr,
                          get_u32_argument(*m_context_ptr,
                                           spec_ptr->n_face_argument),
                          get_u32_argument(*m_context_ptr,
                                           spec_ptr->n_level_argument) );

        switch (spec_ptr->kind)
        {
            case LockKind::BOX:
            {
                result = lock_box(in_call,
                                  object_ptr,
                                  key,
                                  out_error_ptr);

                break;
            }

            case LockKind::BUFFER:
            {
                result = lock_buffer(in_call,
                                     object_ptr,
                                     key,
                                     out_error_ptr);

                break;
            }

            case LockKind::RECT:
            {
                result = lock_rect(in_call,
                                   object_ptr,
                                   key,
                                   out_error_ptr);

                break;
            }

            default:
            {
                *out_error_ptr = "Not a lock.";

                break;
            }
        }
    }

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::D3D8LockReplayer::lock_box(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                               void*                                        in_object_ptr,
                                                               const LockKey&                               in_key,
                                                               std::string*                                 out_error_ptr)
{
    typedef HRESULT (__stdcall* PFNLEVELBOXPROC)    (void*, UINT, Interception::Hooks::D3D8::D3DVOLUME_DESC*);
    typedef HRESULT (__stdcall* PFNLEVELBOXLOCKPROC)(void*, UINT, Interception::Hooks::D3D8::D3DLOCKED_BOX*, const Interception::Hooks::D3D8::D3DBOX*, DWORD);
    typedef HRESULT (__stdcall* PFNVOLUMEDESCPROC)  (void*, Interception::Hooks::D3D8::D3DVOLUME_DESC*);
    typedef HRESULT (__stdcall* PFNVOLUMELOCKPROC)  (void*, Interception::Hooks::D3D8::D3DLOCKED_BOX*, const Interception::Hooks::D3D8::D3DBOX*, DWORD);

    Interception::Hooks::D3D8::D3DBOX         box        = {};
    const Interception::Hooks::D3D8::D3DBOX*  box_ptr    = nullptr;
    Interception::Hooks::D3D8::D3DVOLUME_DESC desc       = {};
    HRESULT                                   hresult    = E_FAIL;
    const uint32_t                            level      = std::get<2>(in_key);
    Interception::Hooks::D3D8::D3DLOCKED_BOX  locked     = {};
    LockRecord                                record     = {};
    bool                                      result     = false;
    const auto                                spec_ptr   = get_lock_spec_ptr         (in_call.func);
    void** const                              vtable_ptr = *reinterpret_cast<void***>(in_object_ptr);
    const DWORD                               flags      = get_u32_argument          (*m_context_ptr,
                                                                                     spec_ptr->n_flags_argument);

    {
        size_t         n_bytes  = 0;
        const uint8_t* data_ptr = m_context_ptr->get_chunk_data_ptr(m_context_ptr->get_in_argument_ptr(static_cast<size_t>(spec_ptr->n_region_argument) ),
                                                                    &n_bytes);

        if (n_bytes >= sizeof(box) )
        {
            ::memcpy(&box,
                     data_ptr,
                     sizeof(box) );

            box_ptr = &box;
        }
    }

    if (spec_ptr->n_level_argument != ARGUMENT_ABSENT)
    {
        hresult = reinterpret_cast<PFNLEVELBOXLOCKPROC>(vtable_ptr[spec_ptr->vtable_index])(in_object_ptr,
                                                                                            level,
                                                                                           &locked,
                                                                                            box_ptr,
                                                                                            flags);

        if (hresult >= 0)
        {
            reinterpret_cast<PFNLEVELBOXPROC>(vtable_ptr[spec_ptr->desc_vtable_index])(in_object_ptr,
                                                                                       level,
                                                                                      &desc);
        }
    }
    else
    {
        hresult = reinterpret_cast<PFNVOLUMELOCKPROC>(vtable_ptr[spec_ptr->vtable_index])(in_object_ptr,
                                                                                         &locked,
                                                                                          box_ptr,
                                                                                          flags);

        if (hresult >= 0)
        {
            reinterpret_cast<PFNVOLUMEDESCPROC>(vtable_ptr[spec_ptr->desc_vtable_index])(in_object_ptr,
                                                                                         &desc);
        }
    }

    if (hresult < 0)
    {
        *out_error_ptr = "Locking a volume failed.";

        goto end;
    }

    Interception::Core::D3D8Utils::get_surface_region_layout(desc.Format,
                                                             (box_ptr != nullptr) ? box.Right  - box.Left : desc.Width,
                                                             (box_ptr != nullptr) ? box.Bottom - box.Top  : desc.Height,
                                                            &record.n_rows,
                                                            &record.n_row_bytes);

    record.depth              = (box_ptr != nullptr) ? box.Back - box.Front
                                                     : desc.Depth;
    record.dumped_row_pitch   = get_dumped_int32(*m_context_ptr,
                                                 static_cast<size_t>(spec_ptr->n_locked_argument),
                                                 0);
    record.dumped_slice_pitch = get_dumped_int32(*m_context_ptr,
                                                 static_cast<size_t>(spec_ptr->n_locked_argument),
                                                 1);
    record.flags              = flags;
    record.live_data_ptr      = static_cast<uint8_t*>(locked.pBits);
    record.live_row_pitch     = locked.RowPitch;
    record.live_slice_pitch   = locked.SlicePitch;

    m_lock_record_map[in_key] = record;
    result                    = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::D3D8LockReplayer::lock_buffer(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                  void*                                        in_object_ptr,
                                                                  const LockKey&                               in_key,
                                                                  std::string*                                 out_error_ptr)
{
    typedef HRESULT (__stdcall* PFNBUFFERDESCPROC)(void*, Interception::Hooks::D3D8::D3DVERTEXBUFFER_DESC*);
    typedef HRESULT (__stdcall* PFNBUFFERLOCKPROC)(void*, UINT, UINT, BYTE**, DWORD);

    BYTE*                                           data_ptr   = nullptr;
    Interception::Hooks::D3D8::D3DVERTEXBUFFER_DESC desc       = {};
    HRESULT                                         hresult    = E_FAIL;
    LockRecord                                      record     = {};
    bool                                            result     = false;
    const auto                                      spec_ptr   = get_lock_spec_ptr         (in_call.func);
    void** const                                    vtable_ptr = *reinterpret_cast<void***>(in_object_ptr);
    const DWORD                                     flags      = get_u32_argument          (*m_context_ptr,
                                                                                            spec_ptr->n_flags_argument);

    const UINT offset     = get_u32_argument(*m_context_ptr,
                                             spec_ptr->n_offset_argument);
    const UINT size       = get_u32_argument(*m_context_ptr,
                                             spec_ptr->n_size_argument);

    hresult = reinterpret_cast<PFNBUFFERLOCKPROC>(vtable_ptr[spec_ptr->vtable_index])(in_object_ptr,
                                                                                      offset,
                                                                                      size,
                                                                                     &data_ptr,
                                                                                      flags);

    if (hresult < 0)
    {
        *out_error_ptr = "Locking a buffer failed.";

        goto end;
    }

    reinterpret_cast<PFNBUFFERDESCPROC>(vtable_ptr[spec_ptr->desc_vtable_index])(in_object_ptr,
                                                                                &desc);

    /* A size of 0 locks everything from the offset on. */
    record.flags          = flags;
    record.live_data_ptr  = data_ptr;
    record.n_buffer_bytes = (size != 0)          ? size
                          : (desc.Size > offset) ? desc.Size - offset
                                                 : 0;

    m_lock_record_map[in_key] = record;
    result                    = true;

end:
    return result;
}

bool Replay::StereoReplayEngineDLL::D3D8LockReplayer::lock_rect(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                void*                                        in_object_ptr,
                                                                const LockKey&                               in_key,
                                                                std::string*                                 out_error_ptr)
{
    typedef HRESULT (__stdcall* PFNCUBELOCKPROC)   (void*, UINT, UINT, Interception::Hooks::D3D8::D3DLOCKED_RECT*, const RECT*, DWORD);
    typedef HRESULT (__stdcall* PFNLEVELDESCPROC)  (void*, UINT, Interception::Hooks::D3D8::D3DSURFACE_DESC*);
    typedef HRESULT (__stdcall* PFNLEVELLOCKPROC)  (void*, UINT, Interception::Hooks::D3D8::D3DLOCKED_RECT*, const RECT*, DWORD);
    typedef HRESULT (__stdcall* PFNSURFACEDESCPROC)(void*, Interception::Hooks::D3D8::D3DSURFACE_DESC*);
    typedef HRESULT (__stdcall* PFNSURFACELOCKPROC)(void*, Interception::Hooks::D3D8::D3DLOCKED_RECT*, const RECT*, DWORD);

    Interception::Hooks::D3D8::D3DSURFACE_DESC desc       = {};
    const uint32_t                             face       = std::get<1>(in_key);
    HRESULT                                    hresult    = E_FAIL;
    const uint32_t                             level      = std::get<2>(in_key);
    Interception::Hooks::D3D8::D3DLOCKED_RECT  locked     = {};
    LockRecord                                 record     = {};
    RECT                                       rect       = {};
    const RECT*                                rect_ptr   = nullptr;
    bool                                       result     = false;
    const auto                                 spec_ptr   = get_lock_spec_ptr         ( in_call.func);
    void** const                               vtable_ptr = *reinterpret_cast<void***>( in_object_ptr);
    const DWORD                                flags      = get_u32_argument          (*m_context_ptr,
                                                                                        spec_ptr->n_flags_argument);

    {
        /* The rectangle is the data the dumper stored after the arguments of the method. */
        size_t         n_bytes  = 0;
        const uint8_t* data_ptr = m_context_ptr->get_chunk_data_ptr(&in_call.arg_in_vec.back(),
                                                                    &n_bytes);

        if (n_bytes >= sizeof(rect) )
        {
            ::memcpy(&rect,
                     data_ptr,
                     sizeof(rect) );

            rect_ptr = &rect;
        }
    }

    if (spec_ptr->n_face_argument != ARGUMENT_ABSENT)
    {
        hresult = reinterpret_cast<PFNCUBELOCKPROC>(vtable_ptr[spec_ptr->vtable_index])(in_object_ptr,
                                                                                        face,
                                                                                        level,
                                                                                       &locked,
                                                                                        rect_ptr,
                                                                                        flags);
    }
    else
    if (spec_ptr->n_level_argument != ARGUMENT_ABSENT)
    {
        hresult = reinterpret_cast<PFNLEVELLOCKPROC>(vtable_ptr[spec_ptr->vtable_index])(in_object_ptr,
                                                                                         level,
                                                                                        &locked,
                                                                                         rect_ptr,
                                                                                         flags);
    }
    else
    {
        hresult = reinterpret_cast<PFNSURFACELOCKPROC>(vtable_ptr[spec_ptr->vtable_index])(in_object_ptr,
                                                                                          &locked,
                                                                                           rect_ptr,
                                                                                           flags);
    }

    if (hresult < 0)
    {
        *out_error_ptr = "Locking a surface failed.";

        goto end;
    }

    if (spec_ptr->n_level_argument != ARGUMENT_ABSENT)
    {
        reinterpret_cast<PFNLEVELDESCPROC>(vtable_ptr[spec_ptr->desc_vtable_index])(in_object_ptr,
                                                                                    level,
                                                                                   &desc);
    }
    else
    {
        reinterpret_cast<PFNSURFACEDESCPROC>(vtable_ptr[spec_ptr->desc_vtable_index])(in_object_ptr,
                                                                                     &desc);
    }

    Interception::Core::D3D8Utils::get_surface_region_layout(desc.Format,
                                                             (rect_ptr != nullptr) ? static_cast<uint32_t>(rect.right  - rect.left) : desc.Width,
                                                             (rect_ptr != nullptr) ? static_cast<uint32_t>(rect.bottom - rect.top)  : desc.Height,
                                                            &record.n_rows,
                                                            &record.n_row_bytes);

    record.depth            = 1;
    record.dumped_row_pitch = get_dumped_int32(*m_context_ptr,
                                               static_cast<size_t>(spec_ptr->n_locked_argument),
                                               0);
    record.flags            = flags;
    record.live_data_ptr    = static_cast<uint8_t*>(locked.pBits);
    record.live_row_pitch   = locked.Pitch;

    m_lock_record_map[in_key] = record;
    result                    = true;

end:
    return result;
}

void Replay::StereoReplayEngineDLL::D3D8LockReplayer::reset()
{
    m_lock_record_map.clear();
}

bool Replay::StereoReplayEngineDLL::D3D8LockReplayer::unlock(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                             std::string*                                 out_error_ptr)
{
    typedef HRESULT (__stdcall* PFNUNLOCK0PROC)(void*);
    typedef HRESULT (__stdcall* PFNUNLOCK1PROC)(void*, UINT);
    typedef HRESULT (__stdcall* PFNUNLOCK2PROC)(void*, UINT, UINT);

    size_t     n_bytes    = 0;
    void*      object_ptr = nullptr;
    bool       result     = false;
    const auto spec_ptr   = get_lock_spec_ptr(in_call.func);

    if (spec_ptr == nullptr)
    {
        *out_error_ptr = "Not an unlock.";

        goto end;
    }

    if (m_context_ptr->get_live_object(in_call.arg_in_vec.at(0),
                                      &object_ptr,
                                       out_error_ptr) == false)
    {
        goto end;
    }

    {
        const LockKey key(object_ptr,
                          get_u32_argument(*m_context_ptr,
                                            spec_ptr->n_face_argument),
                          get_u32_argument(*m_context_ptr,
                                            spec_ptr->n_level_argument));

        const uint8_t* data_ptr   = m_context_ptr->get_chunk_data_ptr(&in_call.arg_in_vec.back(), /* the data the dumper stored is the last argument */
                                                                      &n_bytes);
        const uint32_t face       = std::get<1>                      (key);
        const auto     iterator   = m_lock_record_map.find           (key);
        const uint32_t level      = std::get<2>                      (key);
        void** const   vtable_ptr = *reinterpret_cast<void***>       (object_ptr);

        if (iterator != m_lock_record_map.end() )
        {
            const LockRecord& record = iterator->second;

            if (data_ptr                                                     != nullptr &&
                (record.flags & Interception::Hooks::D3D8::D3DLOCK_READONLY) == 0)
            {
                if (spec_ptr->kind == LockKind::BUFFER)
                {
                    ::memcpy(record.live_data_ptr,
                             data_ptr,
                             (n_bytes < record.n_buffer_bytes) ? n_bytes
                                                               : record.n_buffer_bytes);
                }
                else
                {
                    for (uint32_t n_slice = 0;
                                  n_slice < record.depth;
                                ++n_slice)
                    {
                        for (uint32_t n_row = 0;
                                      n_row < record.n_rows;
                                    ++n_row)
                        {
                            const int64_t source_offset = static_cast<int64_t>(n_slice) * record.dumped_slice_pitch +
                                                          static_cast<int64_t>(n_row)   * record.dumped_row_pitch;
                            const int64_t target_offset = static_cast<int64_t>(n_slice) * record.live_slice_pitch   +
                                                          static_cast<int64_t>(n_row)   * record.live_row_pitch;

                            if (source_offset                                             < 0        ||
                                static_cast<uint64_t>(source_offset) + record.n_row_bytes > n_bytes)
                            {
                                break;
                            }

                            ::memcpy(record.live_data_ptr + target_offset,
                                     data_ptr + source_offset,
                                     record.n_row_bytes);
                        }
                    }
                }
            }

            m_lock_record_map.erase(iterator);

            if (spec_ptr->n_face_argument != ARGUMENT_ABSENT)
            {
                reinterpret_cast<PFNUNLOCK2PROC>(vtable_ptr[spec_ptr->vtable_index])(object_ptr,
                                                                                     face,
                                                                                     level);
            }
            else
            if (spec_ptr->n_level_argument != ARGUMENT_ABSENT)
            {
                reinterpret_cast<PFNUNLOCK1PROC>(vtable_ptr[spec_ptr->vtable_index])(object_ptr,
                                                                                     level);
            }
            else
            {
                reinterpret_cast<PFNUNLOCK0PROC>(vtable_ptr[spec_ptr->vtable_index])(object_ptr);
            }
        }
    }

    result = true;

end:
    return result;
}
