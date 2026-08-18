// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for Vector3 and Matrix4x4
#include "../Engine/Core/Geometry.h"
#include "test_main.h"
#include <cmath>

REGISTER_TEST(test_Vector3_creation)
{
    Vector3 v(1.0f, 2.0f, 3.0f);
    ASSERT_EQ(1.0f, v.x);
    ASSERT_EQ(2.0f, v.y);
    ASSERT_EQ(3.0f, v.z);

    Vector3 defaultV;
    ASSERT_EQ(0.0f, defaultV.x);
    ASSERT_EQ(0.0f, defaultV.y);
    ASSERT_EQ(0.0f, defaultV.z);
}

REGISTER_TEST(test_Vector3_operations)
{
    Vector3 a(1.0f, 2.0f, 3.0f);
    Vector3 b(4.0f, 5.0f, 6.0f);

    Vector3 sum = a + b;
    ASSERT_EQ(5.0f, sum.x);
    ASSERT_EQ(7.0f, sum.y);
    ASSERT_EQ(9.0f, sum.z);

    Vector3 diff = b - a;
    ASSERT_EQ(3.0f, diff.x);
    ASSERT_EQ(3.0f, diff.y);
    ASSERT_EQ(3.0f, diff.z);

    Vector3 scaled = a * 2.0f;
    ASSERT_EQ(2.0f, scaled.x);
    ASSERT_EQ(4.0f, scaled.y);
    ASSERT_EQ(6.0f, scaled.z);
}

REGISTER_TEST(test_Matrix4x4_identity)
{
    Matrix4x4 id = Matrix4x4::identity();
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            if (i == j)
            {
                ASSERT_EQ(1.0f, id.m[i][j]);
            }
            else
            {
                ASSERT_EQ(0.0f, id.m[i][j]);
            }
        }
    }
}

REGISTER_TEST(test_Matrix4x4_translation)
{
    Matrix4x4 t = Matrix4x4::translation(5.0f, 10.0f, 15.0f);
    ASSERT_EQ(5.0f, t.m[0][3]);
    ASSERT_EQ(10.0f, t.m[1][3]);
    ASSERT_EQ(15.0f, t.m[2][3]);

    // Identity elsewhere
    ASSERT_EQ(1.0f, t.m[0][0]);
    ASSERT_EQ(1.0f, t.m[1][1]);
    ASSERT_EQ(1.0f, t.m[2][2]);
    ASSERT_EQ(0.0f, t.m[0][1]);
}

REGISTER_TEST(test_Matrix4x4_rotationZ)
{
    // 90-degree rotation about Z maps +X to +Y
    Matrix4x4 rot = Matrix4x4::rotationZ(3.14159265f / 2.0f);
    Vector3 v = rot.multiplyVector(Vector3(1.0f, 0.0f, 0.0f));
    ASSERT_NEAR(0.0f, v.x, 0.001f);
    ASSERT_NEAR(1.0f, v.y, 0.001f);
    ASSERT_NEAR(0.0f, v.z, 0.001f);
}

REGISTER_TEST(test_Matrix4x4_rotationX)
{
    // 90-degree rotation about X maps +Y to +Z
    Matrix4x4 rot = Matrix4x4::rotationX(3.14159265f / 2.0f);
    Vector3 v = rot.multiplyVector(Vector3(0.0f, 1.0f, 0.0f));
    ASSERT_NEAR(0.0f, v.x, 0.001f);
    ASSERT_NEAR(0.0f, v.y, 0.001f);
    ASSERT_NEAR(1.0f, v.z, 0.001f);
}

REGISTER_TEST(test_Matrix4x4_rotationY)
{
    // 90-degree rotation about Y maps +Z to +X
    Matrix4x4 rot = Matrix4x4::rotationY(3.14159265f / 2.0f);
    Vector3 v = rot.multiplyVector(Vector3(0.0f, 0.0f, 1.0f));
    ASSERT_NEAR(1.0f, v.x, 0.001f);
    ASSERT_NEAR(0.0f, v.y, 0.001f);
    ASSERT_NEAR(0.0f, v.z, 0.001f);
}

REGISTER_TEST(test_Matrix4x4_multiplyVector_translation)
{
    Matrix4x4 t = Matrix4x4::translation(5.0f, -3.0f, 2.0f);
    Vector3 v = t.multiplyVector(Vector3(1.0f, 1.0f, 1.0f));
    ASSERT_EQ(6.0f, v.x);
    ASSERT_EQ(-2.0f, v.y);
    ASSERT_EQ(3.0f, v.z);
}

REGISTER_TEST(test_Matrix4x4_multiply_identity)
{
    Matrix4x4 a = Matrix4x4::translation(2.0f, 3.0f, 4.0f);
    Matrix4x4 id = Matrix4x4::identity();

    Matrix4x4 product = a * id;
    ASSERT_EQ(2.0f, product.m[0][3]);
    ASSERT_EQ(3.0f, product.m[1][3]);
    ASSERT_EQ(4.0f, product.m[2][3]);
    ASSERT_EQ(1.0f, product.m[0][0]);
    ASSERT_EQ(1.0f, product.m[1][1]);
}

REGISTER_TEST(test_Matrix4x4_projection)
{
    // Perspective projection: -1 in (3,2), 0 in (3,3)
    Matrix4x4 p = Matrix4x4::projection(1.0f, 1.5f, 0.1f, 100.0f);
    ASSERT_NEAR(-1.0f, p.m[3][2], 0.001f);
    ASSERT_NEAR(0.0f, p.m[3][3], 0.001f);

    // f = 1/tan(fov/2), m[0][0] = f/aspect
    float f = 1.0f / std::tan(0.5f);
    ASSERT_NEAR(f / 1.5f, p.m[0][0], 0.001f);
    ASSERT_NEAR(f, p.m[1][1], 0.001f);
}
