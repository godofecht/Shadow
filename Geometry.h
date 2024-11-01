#pragma once

template <typename T>
class Position
{
public:
    float x, y;

    Position(T x = 0.0f, T y = 0.0f)
        : x(x), y(y) 
    {}

    void update(T dx, T dy)
    {
        x += dx;
        y += dy;
    }
};

class Vector2D
{
public:
    float x, y;

    Vector2D(float x = 0.0f, float y = 0.0f)
        : x(x), y(y) 
    {}

    Vector2D normalized() const
    {
        float magnitude = std::sqrt(x * x + y * y);
        return magnitude > 0 ? Vector2D(x / magnitude, y / magnitude) : Vector2D(0, 0);
    }

    Vector2D operator*(float scalar) const
    {
        return Vector2D(x * scalar, y * scalar);
    }

    Vector2D operator+(const Vector2D& other) const
    {
        return Vector2D(x + other.x, y + other.y);
    }

    Vector2D operator-(const Vector2D& other) const
    {
        return Vector2D(x - other.x, y - other.y);
    }

    Vector2D& operator+=(const Vector2D& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vector2D& operator-=(const Vector2D& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    bool operator==(const Vector2D& other) const
    {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Vector2D& other) const
    {
        return !(*this == other);
    }
};

template <typename T>
class Rect
{
public:
    Rect(T x, T y, T _width, T _height) 
        : x(x), y(y), width(_width), height(_height)
    {
        updatePoints();
    }

    void updatePoints()
    {
        topLeft = Position<T>(x, y);
        topRight = Position<T>(x + width, y);
        bottomLeft = Position<T>(x, y + height);
        bottomRight = Position<T>(x + width, y + height);
        center = Position<T>(x + width / 2.0, y + height / 2.0);
    }

    T x, y, width, height;
    Position<T> center, topLeft, topRight, bottomLeft, bottomRight;
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