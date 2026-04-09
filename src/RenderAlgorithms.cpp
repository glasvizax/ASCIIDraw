
#include "RenderAlgorithms.h"

#include <algorithm>

#include "BroadcastExecutor.h"
#include "Platform.h"

void pushTriangleBarycenterRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c, BroadcastExecutor& exec)
{
    pushTriangleBarycenterRaw(symbol, a, b, c, exec,
        []
        (char symbol, int x, int y, float alpha, float beta, float gamma)
        {
            platform::drawPixel(x, y, symbol);
        }
    );
}


void pushTriangleBarycenterRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c)
{
    pushTriangleBarycenterRaw(symbol, a, b, c, 
        []
        (char symbol, int x, int y, float alpha, float beta, float gamma)
        {
            platform::drawPixel(x, y, symbol);
        }
    );
}


void pushTriangleScanlineRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c)
{
    if (a.y > b.y)
    {
        swap(a, b);
    }
    if (b.y > c.y)
    {
        swap(b, c);
    }
    if (a.y > b.y)
    {
        swap(a, b);
    }
    int x1, x2;
    if (a.y != b.y)
    {
        for (int y = a.y; y < b.y; ++y)
        {
            float t1 = (y - a.y) / static_cast<float>(c.y - a.y);
            x1 = std::round(a.x + (c.x - a.x) * t1);

            float t2 = (y - a.y) / static_cast<float>(b.y - a.y);
            x2 = std::round(a.x + (b.x - a.x) * t2);

            if (x1 > x2)
            {
                std::swap(x1, x2);
            }

            for (int x = x1; x <= x2; ++x)
            {
                platform::drawPixel(x, y, symbol);
            }
        }
    }
    else
    {
        x1 = a.x;
        x2 = b.x;

        if (x1 > x2)
        {
            std::swap(x1, x2);
        }

        for (int x = x1; x <= x2; ++x)
        {
            platform::drawPixel(x, b.y, symbol);
        }
    }
    if (b.y != c.y)
    {
        for (int y = b.y; y <= c.y; ++y)
        {
            float t1 = (y - a.y) / static_cast<float>(c.y - a.y);
            x1 = std::round(a.x + (c.x - a.x) * t1);

            float t2 = (y - b.y) / static_cast<float>(c.y - b.y);
            x2 = std::round(b.x + (c.x - b.x) * t2);

            if (x1 > x2)
            {
                std::swap(x1, x2);
            }

            for (int x = x1; x <= x2; ++x)
            {
                platform::drawPixel(x, y, symbol);
            }
        }
    }
    else
    {
        x1 = b.x;
        x2 = c.x;

        if (x1 > x2)
        {
            std::swap(x1, x2);
        }

        for (int x = x1; x <= x2; ++x)
        {
            platform::drawPixel(x, b.y, symbol);
        }
    }

}

void pushLineRaw(char symbol, xm::ivec2 a, xm::ivec2 b)
{
    bool steep = std::abs(b.x - a.x) < std::abs(b.y - a.y);
    if (steep)
    {
        std::swap(a.x, a.y);
        std::swap(b.x, b.y);
    }

    if (a.x > b.x)
    {
        swap(a, b);
    }

    int y = a.y;
    int sy = (b.y > a.y) ? 1 : -1;
    int err = 0;

    for (int x = a.x; x <= b.x; ++x)
    {
        if (steep)
        {
            platform::drawPixel(y, x, symbol);
        }
        else
        {
            platform::drawPixel(x, y, symbol);
        }
        err += 2 * std::abs(b.y - a.y);
        if (err >= (b.x - a.x))
        {
            y += sy;
            err -= 2 * (b.x - a.x);
        }
    }
}
