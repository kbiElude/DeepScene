/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A small D3D7 application for the tests of the front end: a 640 x 480 display with a primary surface that flips with a back buffer,
 * a depth buffer attached to the back buffer, and a device that renders to it. The calls are run through the front end as they are
 * made.
 */
#ifndef TESTS_D3D7_SCENE_H
#define TESTS_D3D7_SCENE_H

#include "d3d7_call_builder.h"
#include "d3d7_frontend_harness.h"
#include <stdint.h>

namespace Tests
{
    class D3D7Scene
    {
    public:
        D3D7Scene();

        /* Draws n times: a triangle list of one triangle each. */
        void draw(const uint32_t& in_n_draws);

        /* Runs what has been called on the builder since the last time through the front end. */
        void feed();

        uint64_t            get_back_buffer_surface() const;
        D3D7CallBuilder&    get_builder            ();
        uint64_t            get_depth_surface      () const;
        D3D7FrontendHarness& get_harness           ();
        uint64_t            get_primary_surface    () const;
        void                present                ();

    private:
        uint64_t            m_back_buffer_surface;
        D3D7CallBuilder     m_builder;
        uint64_t            m_depth_surface;
        D3D7FrontendHarness m_harness;
        uint64_t            m_primary_surface;
    };
}

#endif /* TESTS_D3D7_SCENE_H */
