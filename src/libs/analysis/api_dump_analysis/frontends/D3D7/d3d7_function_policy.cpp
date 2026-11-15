/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The groups and their reasons are what matters. The tables are maintained by hand.
 */
#include "analysis/api_dump_analysis/frontends/D3D7/d3d7_function_policy.h"
#include "analysis/api_dump_analysis/frontends/shared/function_policy_reason_table.h"
#include <iterator>

const char Analysis::APIDumpAnalysis::Frontends::D3D7FunctionPolicy::PALETTES[] = "makes or changes a palette, which gives the colors of the pixels of a surface, and the front end follows no palette";

const char* Analysis::APIDumpAnalysis::Frontends::D3D7FunctionPolicy::get_unsupported_reason_ptr(const APIInterceptor::APIFunction& in_function)
{
    static constexpr char NO_SURFACE_DATA[] = "makes or changes a surface, a buffer or the screen in a way that no data of the workload tells";
    static constexpr char STATE_BLOCKS[]    = "records or plays back state blocks, which are calls that the front end does not follow";

    /* Sorted by name. */
    static const FunctionPolicyReasonTable::Entry unsupported_entry_array[] =
    {
        {"ApplyStateBlock",                          STATE_BLOCKS},
        {"BeginStateBlock",                          STATE_BLOCKS},
        {"BltBatch",                                 NO_SURFACE_DATA},
        {"CaptureStateBlock",                        STATE_BLOCKS},
        {"CreateStateBlock",                         STATE_BLOCKS},
        {"DeleteAttachedSurface",                    "takes a surface off another, which can be the depth buffer that a render target is drawn with, and the front end does not follow it"},
        {"DeleteStateBlock",                         STATE_BLOCKS},
        {"EndStateBlock",                            STATE_BLOCKS},
        {"EvaluateMode",                             "tries a display mode, which changes the screen"},
        {"FlipToGDISurface",                         "shows the surface that GDI draws on, which holds what no call of the workload made"},
        {"IDirectDraw7::CreatePalette",              PALETTES},
        {"IDirectDrawPalette::SetEntries",           PALETTES},
        {"IDirectDrawSurface7::AddOverlayDirtyRect", "is about overlays, which the display puts over the screen with what no call of the workload made"},
        {"IDirectDrawSurface7::GetDC",               "gives GDI a way to write the surface, with calls that are not in the workload"},
        {"IDirectDrawSurface7::ReleaseDC",           "ends what GDI wrote to the surface, with calls that are not in the workload"},
        {"IDirectDrawSurface7::SetOverlayPosition",  "is about overlays, which the display puts over the screen with what no call of the workload made"},
        {"IDirectDrawSurface7::SetPalette",          PALETTES},
        {"MultiplyTransform",                        "multiplies a transform into the current one, and the front end follows only the matrices that are set"},
        {"ProcessVertices",                          "transforms vertices into a buffer, which is then in no call of the workload"},
        {"ProcessVerticesStrided",                   "transforms vertices into a buffer, which is then in no call of the workload"},
        {"Restore",                                  "gives a surface back its memory, and what it held is lost"},
        {"RestoreAllSurfaces",                       "gives the surfaces back their memory, and what they held is lost"},
        {"RestoreDisplayMode",                       "changes the display mode, which changes the screen"},
        {"SetColorKey",                              "decides which pixels a blit or a draw leaves, which the front end does not follow"},
        {"SetSurfaceDesc",                           NO_SURFACE_DATA},
        {"StartModeTest",                            "tries display modes, which changes the screen"},
        {"UpdateOverlay",                            "is about overlays, which the display puts over the screen with what no call of the workload made"},
        {"UpdateOverlayDisplay",                     "is about overlays, which the display puts over the screen with what no call of the workload made"},
        {"UpdateOverlayZOrder",                      "is about overlays, which the display puts over the screen with what no call of the workload made"}
    };

    return FunctionPolicyReasonTable::find_reason_ptr(unsupported_entry_array,
                                                      std::size(unsupported_entry_array),
                                                      in_function);
}

bool Analysis::APIDumpAnalysis::Frontends::D3D7FunctionPolicy::is_left_out_by_design(const APIInterceptor::APIFunction& in_function)
{
    static constexpr char DISPLAY_ONLY[]     = "only changes how the display shows the primary surface, and no surface";
    static constexpr char DRIVER_HINT[]      = "is a hint of where the driver keeps a resource, and changes no content";
    static constexpr char LOOKS_ONLY[]       = "changes how a draw looks, and not which surface it writes";
    static constexpr char STORED_ELSEWHERE[] = "is not where the data of what the application writes is stored: it is with the Unlock that follows";

    /* Sorted by name. */
    static const FunctionPolicyReasonTable::Entry left_out_entry_array[] =
    {
        {"AddRef",                          "counts references to an object"},
        {"BeginScene",                      "brackets the calls of a frame, and writes nothing"},
        {"ChangeUniquenessValue",           "is a number that the application keeps for itself"},
        {"Compact",                         DRIVER_HINT},
        {"ComputeSphereVisibility",         "asks whether a sphere is seen, and changes nothing"},
        {"CreateClipper",                   "makes a clipper, which only restricts what a blit to a window writes"},
        {"DirectDrawCreate",                "makes the object that the later calls name"},
        {"DirectDrawCreateClipper",         "makes the object that the later calls name"},
        {"DirectDrawCreateEx",              "makes the object that the later calls name"},
        {"DirectDrawEnumerateA",            "asks which devices there are"},
        {"DirectDrawEnumerateExA",          "asks which devices there are"},
        {"DirectDrawEnumerateExW",          "asks which devices there are"},
        {"DirectDrawEnumerateW",            "asks which devices there are"},
        {"EndScene",                        "brackets the calls of a frame, and writes nothing"},
        {"EvictManagedTextures",            DRIVER_HINT},
        {"FreePrivateData",                 "is data that the application keeps for itself"},
        {"IDirect3DVertexBuffer7::Lock",    STORED_ELSEWHERE},
        {"IDirectDraw7::Initialize",        "is for objects made without DirectDrawCreateEx: the objects of the workload come initialized"},
        {"IDirectDrawClipper::Initialize",  "is for objects made without DirectDrawCreateClipper: the objects of the workload come initialized"},
        {"IDirectDrawPalette::Initialize",  PALETTES},
        {"IDirectDrawSurface7::Initialize", "is for objects made without CreateSurface: the objects of the workload come initialized"},
        {"IDirectDrawSurface7::Lock",       STORED_ELSEWHERE},
        {"Optimize",                        "lets the driver lay the vertices of a buffer out as it likes, and changes none of them"},
        {"PageLock",                        DRIVER_HINT},
        {"PageUnlock",                      DRIVER_HINT},
        {"PreLoad",                         DRIVER_HINT},
        {"QueryInterface",                  "asks for another interface of an object, which the front end follows by the pointer that the call returns"},
        {"SetClipList",                     "gives a clipper a list, which only restricts what a blit to a window writes"},
        {"SetClipPlane",                    "cuts the geometry with a plane, and writes the same surface"},
        {"SetClipStatus",                   "is a record of the extents that the application keeps, and writes nothing"},
        {"SetClipper",                      "gives a surface a clipper, which only restricts what a blit to a window writes"},
        {"SetColorControls",                DISPLAY_ONLY},
        {"SetCooperativeLevel",             "says who owns the window and the screen, and changes no surface"},
        {"SetGammaRamp",                    DISPLAY_ONLY},
        {"SetHWnd",                         "gives a clipper a window, which only restricts what a blit to a window writes"},
        {"SetLOD",                          DRIVER_HINT},
        {"SetMaterial",                     LOOKS_ONLY},
        {"SetPriority",                     DRIVER_HINT},
        {"SetPrivateData",                  "is data that the application keeps for itself"},
        {"TestCooperativeLevel",            "asks whether the device can be used, and changes nothing"},
        {"ValidateDevice",                  "asks whether a draw can be made, and changes nothing"},
        {"WaitForVerticalBlank",            "is about when, and writes nothing"}
    };

    const char* reason_ptr = FunctionPolicyReasonTable::find_reason_ptr(left_out_entry_array,
                                                                        std::size(left_out_entry_array),
                                                                        in_function);

    return (reason_ptr != nullptr ||
            FunctionPolicyReasonTable::is_query(in_function) );
}
