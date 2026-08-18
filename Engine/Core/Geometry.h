// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include <cmath>
#include <SDL2/SDL.h>
#include <initializer_list>

class Point2D
{
public:
    float x, y;

    Point2D (float x = 0.0f, float y = 0.0f) : x (x), y (y)
    {}

    Point2D (std::initializer_list<float> list)
    {
        auto it = list.begin();
        x = (it != list.end()) ? *it++ : 0.0f;
        y = (it != list.end()) ? *it : 0.0f;
    }

    Point2D operator-() const
    {
        return Point2D (-x, -y);
    }

    Point2D operator+ (const Point2D& other) const
    {
        return Point2D (x + other.x, y + other.y);
    }

    Point2D operator- (const Point2D& other) const
    {
        return Point2D (x - other.x, y - other.y);
    }

    void translate (const Point2D& other)
    {
        x += other.x;
        y += other.y;
    }

    bool operator== (const Point2D& other) const
    {
        return x == other.x && y == other.y;
    }
};

class Vector2D
{
public:
    Point2D origin;

    Vector2D(float x = 0.0f, float y = 0.0f)
        : origin (x, y)
    {}

    Vector2D normalized() const
    {
        float magnitude = std::sqrt (origin.x * origin.x + origin.y * origin.y);
        return magnitude > 0 ? Vector2D (origin.x / magnitude, origin.y / magnitude) : Vector2D(0, 0);
    }

    Vector2D operator*(float scalar) const
    {
        return Vector2D (origin.x * scalar, origin.y * scalar);
    }

    Vector2D operator+(const Vector2D& other) const
    {
        return Vector2D (origin.x + other.origin.x, origin.y + other.origin.y);
    }

    Vector2D operator-(const Vector2D& other) const
    {
        return Vector2D (origin.x - other.origin.x, origin.y - other.origin.y);
    }

    Vector2D& operator+=(const Vector2D& other)
    {
        origin.translate (other.origin);
        return *this;
    }

    Vector2D& operator-=(const Vector2D& other)
    {
        origin.translate (-other.origin);
        return *this;
    }

    bool operator==(const Vector2D& other) const
    {
        return (origin == other.origin);
    }

    bool operator!=(const Vector2D& other) const
    {
        return !(*this == other);
    }
};

class Vector3
{
public:
    float x, y, z;

    Vector3 (float x = 0.0f, float y = 0.0f, float z = 0.0f) : x (x), y (y), z (z)
    {}

    Vector3 operator+ (const Vector3& other) const { return Vector3 (x + other.x, y + other.y, z + other.z); }
    Vector3 operator- (const Vector3& other) const { return Vector3 (x - other.x, y - other.y, z - other.z); }
    Vector3 operator* (float scalar) const { return Vector3 (x * scalar, y * scalar, z * scalar); }
};

class Matrix4x4
{
public:
    float m[4][4] = {};

    Matrix4x4() {
        for (int i = 0; i < 4; i++) m[i][i] = 1.0f; // Identity by default
    }

    static Matrix4x4 identity() {
        return Matrix4x4();
    }

    static Matrix4x4 rotationX (float angle) {
        Matrix4x4 mat;
        float s = std::sin (angle);
        float c = std::cos (angle);
        mat.m[1][1] = c; mat.m[1][2] = -s;
        mat.m[2][1] = s; mat.m[2][2] = c;
        return mat;
    }

    static Matrix4x4 rotationY (float angle) {
        Matrix4x4 mat;
        float s = std::sin (angle);
        float c = std::cos (angle);
        mat.m[0][0] = c; mat.m[0][2] = s;
        mat.m[2][0] = -s; mat.m[2][2] = c;
        return mat;
    }

    static Matrix4x4 rotationZ (float angle) {
        Matrix4x4 mat;
        float s = std::sin (angle);
        float c = std::cos (angle);
        mat.m[0][0] = c; mat.m[0][1] = -s;
        mat.m[1][0] = s; mat.m[1][1] = c;
        return mat;
    }

    static Matrix4x4 translation (float x, float y, float z) {
        Matrix4x4 mat;
        mat.m[0][3] = x;
        mat.m[1][3] = y;
        mat.m[2][3] = z;
        return mat;
    }

    static Matrix4x4 projection (float fov, float aspect, float nearPlane, float farPlane) {
        Matrix4x4 mat;
        float f = 1.0f / std::tan (fov / 2.0f);
        mat.m[0][0] = f / aspect;
        mat.m[1][1] = f;
        mat.m[2][2] = (farPlane + nearPlane) / (nearPlane - farPlane);
        mat.m[2][3] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
        mat.m[3][2] = -1.0f;
        mat.m[3][3] = 0.0f;
        return mat;
    }

    Matrix4x4 operator* (const Matrix4x4& other) const {
        Matrix4x4 res;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                res.m[i][j] = m[i][0] * other.m[0][j] +
                               m[i][1] * other.m[1][j] +
                               m[i][2] * other.m[2][j] +
                               m[i][3] * other.m[3][j];
            }
        }
        return res;
    }

    Vector3 multiplyVector (const Vector3& v) const {
        float x = v.x * m[0][0] + v.y * m[0][1] + v.z * m[0][2] + m[0][3];
        float y = v.x * m[1][0] + v.y * m[1][1] + v.z * m[1][2] + m[1][3];
        float z = v.x * m[2][0] + v.y * m[2][1] + v.z * m[2][2] + m[2][3];
        float w = v.x * m[3][0] + v.y * m[3][1] + v.z * m[3][2] + m[3][3];

        if (w != 0.0f) {
            x /= w; y /= w; z /= w;
        }
        return Vector3 (x, y, z);
    }
};

template <typename T>
class Rect
{
public:
    Rect (T x, T y, T _width, T _height) 
        : x (x), y (y), width (_width), height (_height)
    {
        updatePoints();
    }

    Rect () : x (0), y (0), width (0), height (0)
    {
        updatePoints();
    }

    void updatePoints()
    {
        topLeft = Point2D (x, y);
        topRight = Point2D (x + width, y);
        bottomLeft = Point2D (x, y + height);
        bottomRight = Point2D (x + width, y + height);
        center = Point2D (x + width / 2.0f, y + height / 2.0f);
    }

    T x, y, width, height;
    Point2D center, topLeft, topRight, bottomLeft, bottomRight;
};


class RenderUtils 
{
public:
    static SDL_Rect createRenderQuad (const Rect<float>& bounds) 
    {
        return SDL_Rect 
        {
            static_cast<int>(bounds.x),
            static_cast<int>(bounds.y),
            static_cast<int>(bounds.width),
            static_cast<int>(bounds.height)
        };
    }
};