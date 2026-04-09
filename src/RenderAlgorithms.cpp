
#include "RenderAlgorithms.h"

#include <algorithm>

#include "BroadcastExecutor.h"
#include "Platform.h"

void pushTriangleBarycenterRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c, BroadcastExecutor& exec)
{
    xm::ivec2 bbmin, bbmax;
    bbmin.x = std::min(std::min(a.x, b.x), c.x);
    bbmin.y = std::min(std::min(a.y, b.y), c.y);
    bbmax.x = std::max(std::max(a.x, b.x), c.x);
    bbmax.y = std::max(std::max(a.y, b.y), c.y);

    int triangle_area_sign = xm::cross2D(b - a, c - a);
    if (triangle_area_sign == 0)
    {
        return;
    }

    xm::ivec2 per_thread;
    int width = bbmax.x - bbmin.x, height = bbmax.y - bbmin.y;
    int current_thread_count = exec.m_thread_count + 1;
    bool horizontal = false;
    if (height > width)
    {
        per_thread.x = width / current_thread_count;
        per_thread.y = height;
    }
    else
    {
        per_thread.x = width;
        per_thread.y = height / current_thread_count;
        horizontal = true;
    }

    exec.pushSync([
        _per_thread = per_thread,
        _bbmin = bbmin,
        _bbmax = bbmax,
        _triangle_area_sign = triangle_area_sign,
        _a = a, _b = b, _c = c,
        _symbol = symbol,
        _horizontal = horizontal
    ]
    (unsigned int thread_idx, unsigned int thread_count)
        {
            int curr_y, curr_x;
            if (_horizontal)
            {
                curr_x = 0;
                curr_y = thread_idx * _per_thread.y;
            }
            else
            {
                curr_x = thread_idx * _per_thread.x;
                curr_y = 0;
            }

            curr_y += _bbmin.y;
            curr_x += _bbmin.x;
            if (thread_idx != (thread_count - 1))
            {
                for (int x = 0; x < _per_thread.x; ++x)
                {
                    for (int y = 0; y < _per_thread.y; ++y)
                    {
                        xm::ivec2 curr(curr_x + x, curr_y + y);
                        int alpha_sgn = xm::cross2D(_b - curr, _c - curr) * _triangle_area_sign;
                        int beta_sgn = xm::cross2D(_c - curr, _a - curr) * _triangle_area_sign;
                        int gamma_sgn = xm::cross2D(_a - curr, _b - curr) * _triangle_area_sign;

                        if (alpha_sgn >= 0 && beta_sgn >= 0 && gamma_sgn >= 0)
                        {
                            platform::drawPixel(curr.x, curr.y, _symbol);
                        }
                    }
                }
            }
            else
            {
                for (int x = curr_x; x <= _bbmax.x; ++x)
                {
                    for (int y = curr_y; y <= _bbmax.y; ++y)
                    {
                        xm::ivec2 curr(x, y);
                        int alpha_sgn = xm::cross2D(_b - curr, _c - curr) * _triangle_area_sign;
                        int beta_sgn = xm::cross2D(_c - curr, _a - curr) * _triangle_area_sign;
                        int gamma_sgn = xm::cross2D(_a - curr, _b - curr) * _triangle_area_sign;

                        if (alpha_sgn >= 0 && beta_sgn >= 0 && gamma_sgn >= 0)
                        {
                            platform::drawPixel(curr.x, curr.y, _symbol);
                        }
                    }
                }

            }

        });
}


void pushTriangleBarycenterRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c)
{
    xm::ivec2 bbmin, bbmax;
    bbmin.x = std::min(std::min(a.x, b.x), c.x);
    bbmin.y = std::min(std::min(a.y, b.y), c.y);
    bbmax.x = std::max(std::max(a.x, b.x), c.x);
    bbmax.y = std::max(std::max(a.y, b.y), c.y);

    int triangle_area_sign = xm::cross2D(b - a, c - a);

    for (int x = bbmin.x; x <= bbmax.x; ++x)
    {
        for (int y = bbmin.y; y <= bbmax.y; ++y)
        {
            xm::ivec2 curr(x, y);
            int alpha_sgn = xm::cross2D(b - curr, c - curr) * triangle_area_sign;
            int beta_sgn = xm::cross2D(c - curr, a - curr) * triangle_area_sign;
            int gamma_sgn = xm::cross2D(a - curr, b - curr) * triangle_area_sign;

            if (alpha_sgn >= 0 && beta_sgn >= 0 && gamma_sgn >= 0)
            {
                platform::drawPixel(curr.x, curr.y, symbol);
            }
        }
    }

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
