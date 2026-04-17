#pragma once

#include <xm/xm.h>

class BroadcastExecutor;

void pushLineRaw(char symbol, xm::ivec2 a, xm::ivec2 b);
void pushTriangleScanlineRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c);
void pushTriangleBarycenterRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c);
void pushTriangleBarycenterRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c, BroadcastExecutor& exec);

template <typename FragmentShader>
void pushTriangleBarycenterRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c, FragmentShader fragment_shader)
{
    xm::ivec2 bbmin, bbmax;
    bbmin.x = std::min(std::min(a.x, b.x), c.x);
    bbmin.y = std::min(std::min(a.y, b.y), c.y);
    bbmax.x = std::max(std::max(a.x, b.x), c.x);
    bbmax.y = std::max(std::max(a.y, b.y), c.y);

    float triangle_area = xm::cross2D(b - a, c - a);

    if (triangle_area == 0)
    {
        return;
    }

    for (int x = bbmin.x; x <= bbmax.x; ++x)
    {
        for (int y = bbmin.y; y <= bbmax.y; ++y)
        {
            xm::ivec2 curr(x, y);
            float alpha = xm::cross2D(b - curr, c - curr) / triangle_area;
            float beta = xm::cross2D(c - curr, a - curr) / triangle_area;
            float gamma = xm::cross2D(a - curr, b - curr) / triangle_area;

            if (alpha >= 0 && beta >= 0 && gamma >= 0)
            {
                fragment_shader(symbol, curr.x, curr.y, alpha, beta, gamma);
            }
        }
    }

}

template <typename FragmentShader>
void pushTriangleBarycenterRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c, BroadcastExecutor& exec, FragmentShader fragment_shader)
{
    xm::ivec2 bbmin, bbmax;
    bbmin.x = std::min(std::min(a.x, b.x), c.x);
    bbmin.y = std::min(std::min(a.y, b.y), c.y);
    bbmax.x = std::max(std::max(a.x, b.x), c.x);
    bbmax.y = std::max(std::max(a.y, b.y), c.y);

    float triangle_area = xm::cross2D(b - a, c - a);

    if (triangle_area == 0)
    {
        return;
    }

    xm::ivec2 per_thread;
    int width = bbmax.x - bbmin.x + 1, height = bbmax.y - bbmin.y + 1;
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
        _triangle_area = triangle_area,
        _a = a, _b = b, _c = c,
        _symbol = symbol,
        _horizontal = horizontal,
        &fragment_shader
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
                        float alpha = xm::cross2D(_b - curr, _c - curr) / _triangle_area;
                        float beta = xm::cross2D(_c - curr, _a - curr) / _triangle_area;
                        float gamma = xm::cross2D(_a - curr, _b - curr) / _triangle_area;

                        if (alpha >= 0 && beta >= 0 && gamma >= 0)
                        {
                            fragment_shader(_symbol, curr.x, curr.y, alpha, beta, gamma);
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
                        float alpha = xm::cross2D(_b - curr, _c - curr) / _triangle_area;
                        float beta = xm::cross2D(_c - curr, _a - curr) / _triangle_area;
                        float gamma = xm::cross2D(_a - curr, _b - curr) / _triangle_area;

                        if (alpha >= 0 && beta >= 0 && gamma >= 0)
                        {
                            fragment_shader(_symbol, curr.x, curr.y, alpha, beta, gamma);
                        }
                    }
                }

            }

        });
}

template <typename FragmentShader>
void pushLineRaw(char symbol, xm::ivec2 a, xm::ivec2 b, FragmentShader fragment_shader)
{
    bool steep = std::abs(b.x - a.x) < std::abs(b.y - a.y);
    if (steep)
    {
        std::swap(a.x, a.y);
        std::swap(b.x, b.y);
    }

    if (a.x > b.x)
    {
        xm::swap(a, b);
    }

    int y = a.y;
    int sy = (b.y > a.y) ? 1 : -1;
    int err = 0;

    for (int x = a.x; x <= b.x; ++x)
    {
        if (steep)
        {
            fragment_shader(y, x, symbol);
        }
        else
        {
            fragment_shader(x, y, symbol);
        }
        err += 2 * std::abs(b.y - a.y);
        if (err >= (b.x - a.x))
        {
            y += sy;
            err -= 2 * (b.x - a.x);
        }
    }
}