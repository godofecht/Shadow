#pragma once

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
        center = Point2D (x + width / 2.0, y + height / 2.0);
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