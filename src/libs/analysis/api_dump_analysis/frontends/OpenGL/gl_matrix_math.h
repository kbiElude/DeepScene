/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Builds the matrices that the OpenGL calls produce, exactly as the specification defines them (column vectors, stored column after column).
 * A matrix that cannot be built (a box with no width, an axis of length zero) is the identity.
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

                /* The result of glMultMatrix for the current matrix and the given matrix. The current matrix is the left factor. */
                static GLMatrix multiply(const GLMatrix& in_left,
                                         const GLMatrix& in_right);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_MATRIX_MATH_H */
