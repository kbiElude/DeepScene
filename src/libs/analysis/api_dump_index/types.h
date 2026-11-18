/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPINDEX_TYPES_H
#define APIDUMPINDEX_TYPES_H

#include <array>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpIndex
    {
        /* What a call does to the frames a replay makes (see ICallDescriber). */
        enum class CallClass
        {
            CONDITIONAL, /* content or state, by what it refers to (a display list call) */
            CONTENT,     /* draws, clears, copies, uploads, locks                        */
            STATE        /* everything else                                              */
        };

        /* What an OpenGL function is, for everything that has to recognize it (see GLFunctionClassifier). */
        enum class GLFunctionKind
        {
            BAKES_CURRENT_MATRIX,   /* its arguments are converted with the model-view matrix that is current at the call (a light position, a clip plane, the raster position) */
            CLIENT_ARRAY_STATE,     /* where the arrays that a draw reads are, and which are enabled                                                                            */
            CONDITIONAL,            /* content or state, by what it refers to (a display list call)                                                                            */
            CONTENT,                /* draws, clears, copies, uploads of pixels and reads of them                                                                              */
            CURRENT_VALUE,          /* a value for the vertices that follow (a color, a normal, a texture coordinate...)                                                       */
            NAME_OR_SIZE,           /* the names and sizes of objects, and the calls that only wait                                                                            */
            QUERY,                  /* returns what the program asks for, changes nothing                                                                                      */
            RENDER_STATE_UNTRACKED, /* changes how a draw looks and not its structure, so the model does not tell draws apart by it                                           */
            STATE,                  /* any other state call that is known: followed by the front end, or told in the state coalescing                                         */

            UNKNOWN                 /* not a function that any layer has met: every layer ends the process for it                                                              */
        };

        /* Whether the analysis follows what a function does (the front end of OpenGL), and if not, why not. */
        enum class GLFunctionSupport
        {
            SUPPORTED,
            UNSUPPORTED_DRAW_WRITE_STATE,  /* how a draw combines with the framebuffer, and the model does not follow it (the logic op, the blend equation) */
            UNSUPPORTED_FRAMEBUFFER_WRITE, /* the framebuffer is written from somewhere other than a draw, or lists named by an array are called         */
            UNSUPPORTED_TEXTURE            /* a texture of another kind than GL_TEXTURE_2D, or one that is changed in a way that is not followed       */
        };

        /* What a function is, and whether the analysis supports it. */
        struct GLFunctionInfo
        {
            GLFunctionKind    kind    = GLFunctionKind::UNKNOWN;
            GLFunctionSupport support = GLFunctionSupport::SUPPORTED;
        };

        /* How a call acts on the state that dead frames carry from call to call (see IStateEffectDescriber). */
        enum class StateEffectKind
        {
            ASSIGN,   /* sets a piece of state whatever it was: only the last such call before a reader is needed */
            BARRIER,  /* may read or change anything: all earlier calls come first; selectors are unknown after it */
            ORDERED,  /* reads some domains and selectors, changes no tracked state: made in place               */
            RECORDED, /* inside a recorded list: changes no state yet, made in place                              */
            SELECT    /* chooses what later calls act on: only the choice in force at a user is needed           */
        };

        /* A piece of state, named exactly (no hashing: a collision would lose a call). */
        typedef std::array<uint32_t, 4> StateKey;

        /* What a call does to state. Domains and selectors are small numbers of the API describer: a domain is a bit of domain_mask, a selector a bit of selector_mask. */
        struct StateEffect
        {
            uint32_t        domain_mask    = 0;                       /* ASSIGN: the domain written; ORDERED: the domains read */
            StateKey        key            = {{0, 0, 0, 0}};          /* ASSIGN: the piece of state, without what selectors choose */
            StateEffectKind kind           = StateEffectKind::BARRIER;
            uint32_t        selector       = 0;                       /* SELECT: the selector, below 32 */
            uint32_t        selector_mask  = 0;                       /* ASSIGN, ORDERED: the selectors the call uses */
            uint64_t        selector_value = 0;                       /* SELECT: what is chosen */
        };
    }
}

#endif /* APIDUMPINDEX_TYPES_H */
