/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_D3D7_CALL_EXECUTOR_H
#define STEREO_REPLAY_ENGINEDLL_D3D7_CALL_EXECUTOR_H

#include "Interception/Hooks/D3D7/types.h"
#include "call_context.h"
#include "i_call_executor.h"
#include "live_object_list.h"
#include "object_remapper.h"
#include <map>
#include <string>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <Windows.h>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Replays the DirectDraw 7 and Direct3D 7 calls on the ddraw.dll of the system. The objects are created by the replay, and the
             * dump's pointers to them are mapped to the live ones. Only a replay of the same pointer size as the dumped process can do it
             * (DirectDraw 7 is 32-bit).
             *
             * The replay is in a window, whatever the application did: a program which takes over the screen with a flipping chain gets a
             * primary surface which shows the window, a back buffer of its own which is not on the screen, and a flip which is a blit of
             * the one to the other. The calls which ask the API about itself are not replayed, and the methods which the replay has no
             * code for fail it, rather than leave the picture wrong.
             */
            class D3D7CallExecutor : public ICallExecutor
            {
            public:
                D3D7CallExecutor(ObjectRemapper* in_remapper_ptr);
                ~D3D7CallExecutor();

                bool           execute            (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                   const IChunkSource&                          in_chunk_source,
                                                   std::string*                                 out_error_ptr) override;
                ExecutionClass get_execution_class(const Capture::APIDumpLoader::DumpedAPICall& in_call)         const override;
                void           on_frame_presented ()                                                             override;
                bool           read_back_buffer   (std::vector<uint8_t>*                        out_pixel_u8_vec_ptr,
                                                   FrameImage*                                  out_image_ptr) override;
                void           reset              ()                                                             override;
                void           restore_state      ()                                                             override;
                void           save_state         ()                                                             override;
                void           set_eye            (const Analysis::RecipeLoader::Eye&           in_eye)          override;
                void           set_per_eye_predicate(const PerEyePredicate&                 in_predicate)    override;
                bool           write_back_buffer  (const std::vector<uint8_t>&                  in_pixel_u8_vec) override;

            private:
                typedef bool (D3D7CallExecutor::*HandlerFuncPtr)(std::string* out_error_ptr);

                /* What the replayed calls have set on the device, for putting it back as it was (Direct3D 7 has no state blocks). The objects are
                 * the left ones, as for the textures of the stages; a state which was never set is not here, and is left as it is.
                 */
                struct DeviceState
                {
                    std::map<uint32_t, uint32_t>                     render_state_map;
                    void*                                            render_target_ptr = nullptr;
                    std::map<uint32_t, void*>                        stage_texture_map;
                    std::map<std::pair<uint32_t, uint32_t>, uint32_t> texture_stage_state_map; /* by the stage and the state */
                    std::map<uint32_t, std::vector<uint8_t> >        transform_map;
                    std::vector<uint8_t>                             viewport_u8_vec;
                };

                /* A surface that is locked: where the pixels are, and how the dump had them laid out. */
                struct LockedSurface
                {
                    Interception::Hooks::D3D7::DDSURFACEDESC2 description = {};
                    uint32_t                                  dumped_pitch = 0;
                    bool                                      has_rect     = false;
                    RECT                                      rect         = {};
                };

                bool add_ref                (std::string* out_error_ptr);
                bool begin_scene            (std::string* out_error_ptr);
                bool blt                    (std::string* out_error_ptr);
                bool blt_fast               (std::string* out_error_ptr);
                bool clear                  (std::string* out_error_ptr);
                bool create_device          (std::string* out_error_ptr);
                bool create_direct_draw     (std::string* out_error_ptr);
                bool create_surface         (std::string* out_error_ptr);
                bool draw_indexed_primitive (std::string* out_error_ptr);
                bool draw_primitive         (std::string* out_error_ptr);
                bool end_scene              (std::string* out_error_ptr);
                bool flip                   (std::string* out_error_ptr);
                bool get_attached_surface   (std::string* out_error_ptr);
                bool add_attached_surface   (std::string* out_error_ptr);
                bool light_enable           (std::string* out_error_ptr);
                bool load                   (std::string* out_error_ptr);
                bool lock                   (std::string* out_error_ptr);
                bool query_interface        (std::string* out_error_ptr);
                bool release                (std::string* out_error_ptr);
                bool set_cooperative_level  (std::string* out_error_ptr);
                bool set_display_mode       (std::string* out_error_ptr);
                bool set_light              (std::string* out_error_ptr);
                bool set_material           (std::string* out_error_ptr);
                bool set_render_state       (std::string* out_error_ptr);
                bool set_render_target      (std::string* out_error_ptr);
                bool set_texture            (std::string* out_error_ptr);
                bool set_texture_stage_state(std::string* out_error_ptr);
                bool set_transform          (std::string* out_error_ptr);
                bool set_viewport           (std::string* out_error_ptr);
                bool unlock                 (std::string* out_error_ptr);

                /* Makes the primary surface show the window: the back buffer is blitted to it. */
                void present_to_window();

                /* The chunk of the argument of the call, or nullptr if it is empty (a null pointer). */
                const uint8_t* get_chunk_ptr(const size_t& in_n_argument,
                                             size_t*       out_n_bytes_ptr = nullptr) const;

                /* The live object that the argument stands for; false, and the error, if the replay has not created it. A null pointer is
                 * allowed, and gives nullptr.
                 */
                /* The right eye's copy of the object, if the object is one that each eye has a copy of; nullptr if not. */
                void* find_right_twin(void* in_left_object_ptr) const;

                bool get_live_object(const size_t& in_n_argument,
                                     void**        out_object_ptr,
                                     std::string*  out_error_ptr) const;

                /* The object that the dump's pointer stands for, whatever the eye is: the left one, which the copies of the other eye hang on. */
                bool get_left_object(const size_t& in_n_argument,
                                     void**        out_object_ptr,
                                     std::string*  out_error_ptr) const;

                bool get_u32(const size_t& in_n_argument,
                             uint32_t*     out_value_ptr) const;

                /* The back buffer, which is the picture of a frame. */
                /* True if an object of the call has a copy for each eye: a call which sets such an object up is made for both. */
                bool involves_per_eye_object() const;

                bool lock_frame_image  (const DWORD&                               in_flags,
                                          Interception::Hooks::D3D7::DDSURFACEDESC2* out_description_ptr);
                void unlock_frame_image();

                /* The object is the replay's from now on. */
                void register_object(const size_t& in_n_argument,
                                     void*         in_live_object_ptr);

                /* True for the methods which only ask the API about itself, whose results the replay does not need. */
                static bool is_query_method(const char* in_method_name_ptr);

                template<typename ReturnType, typename... Arguments>
                static ReturnType call_method(void*           in_object_ptr,
                                              const uint32_t& in_vtable_index,
                                              Arguments...    in_arguments);


                void*                                                          m_back_buffer_ptr;     /* the replay's own reference */
                std::vector<uint8_t>                                           m_capture_u8_vec;
                void*                                                          m_clipper_ptr;
                Analysis::RecipeLoader::Eye                                    m_eye;
                bool                                                           m_is_building_twin;    /* the call is made for the right eye's copies */
                bool                                                           m_needs_twin;          /* the call made an object which each eye has a copy of */
                PerEyePredicate                                                m_per_eye_predicate;
                std::map<void*, void*>                                         m_right_twin_map;      /* the copy of each eye's object: by the left one */
                CallContext                                                    m_context;
                uint32_t                                                       m_display_height;
                uint32_t                                                       m_display_width;
                DeviceState                                                    m_device_state;        /* of the render and texture stage states, transforms and viewport; the rest is below */
                void*                                                          m_device_ptr;          /* the device that the target and textures below are set on */
                void*                                                          m_direct_draw_ptr;
                std::unordered_map<APIInterceptor::APIFunction, HandlerFuncPtr> m_handler_map;
                LiveObjectList                                                 m_live_objects;
                std::map<void*, LockedSurface>                                 m_locked_surface_map;
                std::map<void*, std::vector<uint64_t> >                        m_dumped_value_map;    /* the pointers of the dump that stand for each object */
                std::map<void*, uint32_t>                                      m_reference_count_map; /* the references the replay holds to each object */
                void*                                                          m_primary_ptr;
                std::unordered_set<APIInterceptor::APIFunction>                m_release_function_set;
                DeviceState                                                    m_saved_device_state;  /* as save_state() found it */
                void*                                                          m_render_target_ptr;   /* the left object, as for the textures below */
                std::map<uint32_t, void*>                                      m_stage_texture_map;   /* the left object set on each texture stage */
                HWND                                                          m_window;
                std::unordered_set<APIInterceptor::APIFunction>                m_add_ref_function_set;
                void*                                                          m_locked_image_ptr;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_D3D7_CALL_EXECUTOR_H */
