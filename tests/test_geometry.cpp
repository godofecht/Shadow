// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for Geometry.h components
#include "../Engine/Core/Geometry.h"
#include "test_main.h"
#include <cmath>

REGISTER_TEST(test_Point2D_creation)
{
    Point2D p(3.0f, 4.0f);
    ASSERT_EQ(3.0f, p.x);
    ASSERT_EQ(4.0f, p.y);

    Point2D defaultP;
    ASSERT_EQ(0.0f, defaultP.x);
    ASSERT_EQ(0.0f, defaultP.y);
}

REGISTER_TEST(test_Point2D_initializer_list)
{
    Point2D p{1.0f, 2.0f};
    ASSERT_EQ(1.0f, p.x);
    ASSERT_EQ(2.0f, p.y);

    Point2D partial{5.0f};
    ASSERT_EQ(5.0f, partial.x);
    ASSERT_EQ(0.0f, partial.y);
}

REGISTER_TEST(test_Point2D_operations)
{
    Point2D p1(1.0f, 2.0f);
    Point2D p2(3.0f, 4.0f);

    Point2D sum = p1 + p2;
    ASSERT_EQ(4.0f, sum.x);
    ASSERT_EQ(6.0f, sum.y);

    Point2D diff = p2 - p1;
    ASSERT_EQ(2.0f, diff.x);
    ASSERT_EQ(2.0f, diff.y);

    Point2D neg = -p1;
    ASSERT_EQ(-1.0f, neg.x);
    ASSERT_EQ(-2.0f, neg.y);
}

REGISTER_TEST(test_Point2D_translate_and_equality)
{
    Point2D p(1.0f, 2.0f);
    p.translate(Point2D(10.0f, -5.0f));
    ASSERT_EQ(11.0f, p.x);
    ASSERT_EQ(-3.0f, p.y);

    Point2D a(1.0f, 2.0f);
    Point2D b(1.0f, 2.0f);
    Point2D c(1.0f, 3.0f);
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a == c);
}

REGISTER_TEST(test_Vector2D_creation)
{
    Vector2D v(3.0f, 4.0f);
    ASSERT_EQ(3.0f, v.origin.x);
    ASSERT_EQ(4.0f, v.origin.y);

    Vector2D defaultV;
    ASSERT_EQ(0.0f, defaultV.origin.x);
    ASSERT_EQ(0.0f, defaultV.origin.y);
}

REGISTER_TEST(test_Vector2D_normalized)
{
    Vector2D v(3.0f, 4.0f);
    Vector2D normalized = v.normalized();

    float magnitude = std::sqrt(normalized.origin.x * normalized.origin.x + normalized.origin.y * normalized.origin.y);
    ASSERT_NEAR(1.0f, magnitude, 0.001f);

    // Zero vector normalizes to zero, not NaN
    Vector2D zero(0.0f, 0.0f);
    Vector2D zeroNorm = zero.normalized();
    ASSERT_EQ(0.0f, zeroNorm.origin.x);
    ASSERT_EQ(0.0f, zeroNorm.origin.y);
}

REGISTER_TEST(test_Vector2D_operations)
{
    Vector2D v1(1.0f, 2.0f);
    Vector2D v2(3.0f, 4.0f);

    Vector2D sum = v1 + v2;
    ASSERT_EQ(4.0f, sum.origin.x);
    ASSERT_EQ(6.0f, sum.origin.y);

    Vector2D diff = v2 - v1;
    ASSERT_EQ(2.0f, diff.origin.x);
    ASSERT_EQ(2.0f, diff.origin.y);

    Vector2D scaled = v1 * 2.0f;
    ASSERT_EQ(2.0f, scaled.origin.x);
    ASSERT_EQ(4.0f, scaled.origin.y);
}

REGISTER_TEST(test_Vector2D_compound_assignment)
{
    Vector2D v(1.0f, 2.0f);
    v += Vector2D(10.0f, -1.0f);
    ASSERT_EQ(11.0f, v.origin.x);
    ASSERT_EQ(1.0f, v.origin.y);

    v -= Vector2D(1.0f, 1.0f);
    ASSERT_EQ(10.0f, v.origin.x);
    ASSERT_EQ(0.0f, v.origin.y);
}

REGISTER_TEST(test_Vector2D_equality)
{
    Vector2D a(1.0f, 2.0f);
    Vector2D b(1.0f, 2.0f);
    Vector2D c(1.0f, 3.0f);
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    ASSERT_TRUE(a != c);
}

REGISTER_TEST(test_Rect_creation)
{
    Rect<float> r1(0.0f, 0.0f, 100.0f, 50.0f);
    ASSERT_EQ(0.0f, r1.x);
    ASSERT_EQ(0.0f, r1.y);
    ASSERT_EQ(100.0f, r1.width);
    ASSERT_EQ(50.0f, r1.height);

    Rect<float> r2;
    ASSERT_EQ(0.0f, r2.x);
    ASSERT_EQ(0.0f, r2.y);
    ASSERT_EQ(0.0f, r2.width);
    ASSERT_EQ(0.0f, r2.height);
}

REGISTER_TEST(test_Rect_points)
{
    Rect<float> r(0.0f, 0.0f, 100.0f, 50.0f);

    ASSERT_EQ(50.0f, r.center.x);
    ASSERT_EQ(25.0f, r.center.y);

    ASSERT_EQ(0.0f, r.topLeft.x);
    ASSERT_EQ(0.0f, r.topLeft.y);

    ASSERT_EQ(100.0f, r.topRight.x);
    ASSERT_EQ(0.0f, r.topRight.y);

    ASSERT_EQ(0.0f, r.bottomLeft.x);
    ASSERT_EQ(50.0f, r.bottomLeft.y);

    ASSERT_EQ(100.0f, r.bottomRight.x);
    ASSERT_EQ(50.0f, r.bottomRight.y);
}

REGISTER_TEST(test_Rect_updatePoints)
{
    Rect<float> r(0.0f, 0.0f, 100.0f, 50.0f);

    r.width = 200.0f;
    r.height = 100.0f;
    r.updatePoints();

    ASSERT_EQ(100.0f, r.center.x);
    ASSERT_EQ(50.0f, r.center.y);

    ASSERT_EQ(200.0f, r.bottomRight.x);
    ASSERT_EQ(100.0f, r.bottomRight.y);

    // Repositioning updates corners
    r.x = 10.0f;
    r.y = 20.0f;
    r.updatePoints();
    ASSERT_EQ(110.0f, r.center.x);
    ASSERT_EQ(70.0f, r.center.y);
    ASSERT_EQ(10.0f, r.topLeft.x);
    ASSERT_EQ(20.0f, r.topLeft.y);
}

REGISTER_TEST(test_RenderUtils_createRenderQuad)
{
    Rect<float> r(1.5f, 2.5f, 100.5f, 50.5f);
    SDL_Rect quad = RenderUtils::createRenderQuad(r);
    ASSERT_EQ(1, quad.x);
    ASSERT_EQ(2, quad.y);
    ASSERT_EQ(100, quad.w);
    ASSERT_EQ(50, quad.h);
}
