#pragma once

#include <xm/xm.h>
#include <xm/math_helpers.h>

#include "RenderAlgorithms.h"
#include "BroadcastExecutor.h"

#include "Platform.h"

inline xm::uvec2 NDCtoPixelu(xm::vec2 pos, xm::ivec2 window_size)
{
    return xm::uvec2(window_size.x * (pos.x + 1.0f) / 2.0f, window_size.y * (pos.y + 1.0f) / 2.0f);
}

inline xm::ivec2 NDCtoPixeli(xm::vec2 pos, xm::ivec2 window_size)
{
    return xm::ivec2(window_size.x * (pos.x + 1.0f) / 2.0f, window_size.y * (pos.y + 1.0f) / 2.0f);
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