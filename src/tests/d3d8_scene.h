/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A small D3D8 application for the tests of the front end: a device, a vertex buffer and an index buffer which the draws
 * use, and the calls that make passes. The calls are run through the front end as they are made.
 */
#ifndef TESTS_D3D8_SCENE_H
#define TESTS_D3D8_SCENE_H

#include "d3d8_call_builder.h"
#include "d3d8_frontend_harness.h"
#include <stdint.h>

namespace Tests
{
    class D3D8Scene
    {
    public:
        D3D8Scene();

        /* Renders to the back buffer and the depth buffer of the device. */
        void bind_back_buffer();

        /* Draws n times: two triangles each, from the buffers of the scene. */
        void draw(const uint32_t& in_n_draws);

        /* Runs what has been called on the builder since the last time through the front end. */
        void feed();

        uint64_t             get_back_buffer_surface() const;
        D3D8CallBuilder&     get_builder            ();
        uint64_t             get_depth_surface      () const;
        D3D8FrontendHarness& get_harness            ();
        uint64_t             get_index_buffer       () const;
        uint64_t             get_vertex_buffer      () const;
        void                 present                ();

        /* Makes the camera of the scene a matrix whose first element is the number. */
        void set_view(const float& in_marker);

    private:
        uint64_t            m_back_buffer_surface;
        D3D8CallBuilder     m_builder;
        uint64_t            m_depth_surface;
        D3D8FrontendHarness m_harness;
        uint64_t            m_index_buffer;
        uint64_t            m_vertex_buffer;
    };
}

#endif /* TESTS_D3D8_SCENE_H */
