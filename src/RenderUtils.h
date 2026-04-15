#pragma once

#include <xm/xm.h>
#include <xm/math_helpers.h>

#include "RenderAlgorithms.h"
#include "BroadcastExecutor.h"

#include "Platform.h"

static xm::ivec2 s_window_size;

void pushWindowSize(xm::ivec2 size)
{
    s_window_size = size;
}

inline xm::uvec2 NDCtoPixelu(xm::vec2 pos)
{
    return xm::uvec2(s_window_size.x * (pos.x + 1.0f) / 2.0f, s_window_size.y * (pos.y + 1.0f) / 2.0f);
}

inline xm::ivec2 NDCtoPixeli(xm::vec2 pos)
{
    return xm::ivec2(s_window_size.x * (pos.x + 1.0f) / 2.0f, s_window_size.y * (pos.y + 1.0f) / 2.0f);
}

template <typename FragmentShader>
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c, FragmentShader fragment_shader)
{
    pushTriangleBarycenterRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b), NDCtoPixeli(c), fragment_shader);
}

template <typename FragmentShader>
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c, BroadcastExecutor& exec, FragmentShader fragment_shader)
{
    pushTriangleBarycenterRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b), NDCtoPixeli(c), exec, fragment_shader);
}

void pushLine(char symbol, xm::vec2 a, xm::vec2 b)
{
    pushLineRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b));
}

template <typename FragmentShader>
void pushLine(char symbol, xm::vec2 a, xm::vec2 b, FragmentShader fragment_shader)
{
    pushLineRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b), fragment_shader);
}

void pushPixel(char symbol, xm::vec2 pos)
{
    xm::ivec2 pixel = NDCtoPixeli(pos);
    platform::drawPixel(pixel.x, pixel.y, symbol);
}

void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c)
{
    pushTriangleScanlineRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b), NDCtoPixeli(c));
}

void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c, BroadcastExecutor& exec)
{
    pushTriangleBarycenterRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b), NDCtoPixeli(c), exec);
}

xm::vec3 naiveWorldTransoform(xm::vec3 vec, xm::vec3 pos)
{
    return vec + pos;
}

xm::vec3 naiveViewTransform(xm::vec3 vec, xm::vec3 eye_pos, xm::vec3 look_dir, xm::vec3 eye_up)
{
    xm::vec3 z = xm::normalize(-look_dir);
    xm::vec3 x = xm::normalize(xm::cross(z, eye_up));
    xm::vec3 y = xm::cross(x, z);

    xm::vec3 p = vec - eye_pos;

    return xm::vec3(
        xm::dot(p, x),
        xm::dot(p, y),
        xm::dot(p, z)
    );
}

xm::vec3 naivePerspective(xm::vec3 vec, int n, int f, int fov_vert, int width, int height)
{
    float r, t;

    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float radians = (xm::PI / 180.0f) * fov_vert;

    t = std::tan(radians / 2);
    r = t * aspect;

    float x = vec.x / (-vec.z * r);
    float y = vec.y / (-vec.z * t);
    float z = -(vec.z + n) / (f - n);
                  
    return xm::vec3(x, y, z);
}