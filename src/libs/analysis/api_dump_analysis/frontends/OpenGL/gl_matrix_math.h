/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The matrices that the calls of OpenGL make, as the specification says them (column vectors, the columns kept one after the other).
 * A matrix which cannot be made (a box with no width, an axis of no length) is the identity.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_MATRIX_MATH_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_MATRIX_MATH_H

#include "analysis/api_dump_analysis/frontends/OpenGL/types.h"

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLMatrixMath
            {
            public:
                /* The matrix of glFrustum. */
                static GLMatrix get_frustum(const double& in_left,
                                            const double& in_right,
                                            const double& in_bottom,
                                            const double& in_top,
                                            const double& in_near,
                                            const double& in_far);

                static GLMatrix get_identity();

                /* The matrix of glOrtho. */
                static GLMatrix get_ortho(const double& in_left,
                                          const double& in_right,
                                          const double& in_bottom,
                                          const double& in_top,
                                          const double& in_near,
                                          const double& in_far);

                /* The matrix of glRotate: the angle is in degrees, the axis need not be of length 1. */
                static GLMatrix get_rotation(const double& in_angle_degrees,
                                             const double& in_x,
                                             const double& in_y,
                                             const double& in_z);

                static GLMatrix get_scale(const double& in_x,
                                          const double& in_y,
                                          const double& in_z);

                static GLMatrix get_translation(const double& in_x,
                                                const double& in_y,
                                                const double& in_z);

                /* What glMultMatrix makes of the current matrix and the one it is given: the current one first, as the left factor. */
                static GLMatrix multiply(const GLMatrix& in_left,
                                         const GLMatrix& in_right);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_MATRIX_MATH_H */
