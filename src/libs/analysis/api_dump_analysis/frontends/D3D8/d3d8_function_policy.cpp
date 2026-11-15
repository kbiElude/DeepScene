/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The groups and their reasons are what matters. The tables are maintained by hand.
 */
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_function_policy.h"
#include "analysis/api_dump_analysis/frontends/shared/function_policy_reason_table.h"
#include <iterator>

const char* Analysis::APIDumpAnalysis::Frontends::D3D8FunctionPolicy::get_unsupported_reason_ptr(const APIInterceptor::APIFunction& in_function)
{
    static constexpr char PALETTES[]         = "makes or changes a palette, which gives the colors of the pixels of a texture, and the front end follows no palette";
    static constexpr char PATCHES[]          = "draws or deletes a patch, whose triangles the driver makes, and the front end does not follow them";
    static constexpr char SHADER_CONSTANTS[] = "sets constants that shaders read, which can be the matrices that a replay for the other eye has to change, and the front end follows no constant";
    static constexpr char STATE_BLOCKS[]     = "records or plays back state blocks, which are calls that the front end does not follow";
    static constexpr char VOLUME_WRITES[]    = "writes the texels of a volume, which the front end does not follow";

    /* Sorted by name. */
    static const FunctionPolicyReasonTable::Entry unsupported_entry_array[] =
    {
        {"ApplyStateBlock",                    STATE_BLOCKS},
        {"BeginStateBlock",                    STATE_BLOCKS},
        {"CaptureStateBlock",                  STATE_BLOCKS},
        {"CreateAdditionalSwapChain",          "makes a swap chain, whose frames the front end does not follow"},
        {"CreateStateBlock",                   STATE_BLOCKS},
        {"DeletePatch",                        PATCHES},
        {"DeleteStateBlock",                   STATE_BLOCKS},
        {"DrawRectPatch",                      PATCHES},
        {"DrawTriPatch",                       PATCHES},
        {"EndStateBlock",                      STATE_BLOCKS},
        {"GetFrontBuffer",                     "copies the screen into a surface, with what no call of the workload made"},
        {"IDirect3D8::RegisterSoftwareDevice", "adds a rasterizer that the workload does not carry"},
        {"IDirect3DSwapChain8::Present",       "presents a swap chain, whose frames the front end does not follow"},
        {"IDirect3DVolume8::UnlockBox",        VOLUME_WRITES},
        {"IDirect3DVolumeTexture8::UnlockBox", VOLUME_WRITES},
        {"MultiplyTransform",                  "multiplies a transform into the current one, and the front end follows only the matrices that are set"},
        {"ProcessVertices",                    "transforms vertices into a buffer, which is then in no call of the workload"},
        {"SetCurrentTexturePalette",           PALETTES},
        {"SetPaletteEntries",                  PALETTES},
        {"SetPixelShaderConstant",             SHADER_CONSTANTS},
        {"SetVertexShaderConstant",            SHADER_CONSTANTS}
    };

    return FunctionPolicyReasonTable::find_reason_ptr(unsupported_entry_array,
                                                      std::size(unsupported_entry_array),
                                                      in_function);
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8FunctionPolicy::is_left_out_by_design(const APIInterceptor::APIFunction& in_function)
{
    static constexpr char DISPLAY_ONLY[]     = "only changes how the display shows the screen, and no surface";
    static constexpr char DRIVER_HINT[]      = "is a hint of where the driver keeps a resource, and changes no content";
    static constexpr char LOOKS_ONLY[]       = "changes how a draw looks, and not which surface it writes";
    static constexpr char PRIVATE_DATA[]     = "is data that the application keeps for itself";
    static constexpr char STORED_ELSEWHERE[] = "is not where the data of what the application writes is stored: it is with the Unlock that follows";

    /* Sorted by name. */
    static const FunctionPolicyReasonTable::Entry left_out_entry_array[] =
    {
        {"AddDirtyBox",                 "tells the driver which part of a managed texture changed, and its data is with the Unlock"},
        {"AddDirtyRect",                "tells the driver which part of a managed texture changed, and its data is with the Unlock"},
        {"AddRef",                      "counts references to an object"},
        {"BeginScene",                  "brackets the calls of a frame, and writes nothing"},
        {"Direct3DCreate8",             "makes the object that the later calls name"},
        {"EndScene",                    "brackets the calls of a frame, and writes nothing"},
        {"FreePrivateData",             PRIVATE_DATA},
        {"Lock",                        STORED_ELSEWHERE},
        {"LockBox",                     STORED_ELSEWHERE},
        {"LockRect",                    STORED_ELSEWHERE},
        {"PreLoad",                     DRIVER_HINT},
        {"QueryInterface",              "asks for another interface of an object, which the front end follows by the pointer that the call returns"},
        {"ResourceManagerDiscardBytes", DRIVER_HINT},
        {"SetClipPlane",                "cuts the geometry with a plane, and writes the same surface"},
        {"SetClipStatus",               "is a record of the extents that the application keeps, and writes nothing"},
        {"SetCursorPosition",           DISPLAY_ONLY},
        {"SetCursorProperties",         DISPLAY_ONLY},
        {"SetGammaRamp",                DISPLAY_ONLY},
        {"SetLOD",                      DRIVER_HINT},
        {"SetMaterial",                 LOOKS_ONLY},
        {"SetPriority",                 DRIVER_HINT},
        {"SetPrivateData",              PRIVATE_DATA},
        {"ShowCursor",                  DISPLAY_ONLY},
        {"TestCooperativeLevel",        "asks whether the device can be used, and changes nothing"},
        {"ValidateDevice",              "asks whether a draw can be made, and changes nothing"}
    };

    const char* reason_ptr = FunctionPolicyReasonTable::find_reason_ptr(left_out_entry_array,
                                                                        std::size(left_out_entry_array),
                                                                        in_function);

    return (reason_ptr != nullptr ||
            FunctionPolicyReasonTable::is_query(in_function) );
}
