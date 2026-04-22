#pragma once

#include <vector>
/*
using uint = unsigned int;

struct Vertex
{
    xm::vec3 pos;
    xm::vec2 uv;
};

class Mesh
{
    std::vector<Vertex> m_vertices;
    std::vector<uint> m_indices;

    Mesh(std::vector<Vertex>&& vertices, std::vector<uint>&& indices) : 
        m_vertices(std::move(vertices)), 
        m_indices(std::move(indices))
    {}
};

constexpr std::vector<Vertex> CUBE_VERTICES[] = {

    // -Z (front)
    {{-1,-1,-1}, {0,0}},
    {{ 1,-1,-1}, {1,0}},
    {{ 1, 1,-1}, {1,1}},
    {{-1, 1,-1}, {0,1}},

    // +Z (back)
    {{-1,-1, 1}, {1,0}},
    {{ 1,-1, 1}, {0,0}},
    {{ 1, 1, 1}, {0,1}},
    {{-1, 1, 1}, {1,1}},

    // -X (left) 
    {{-1,-1,-1}, {1,0}},
    {{-1, 1,-1}, {1,1}},
    {{-1, 1, 1}, {0,1}},
    {{-1,-1, 1}, {0,0}},

    // +X (right)
    {{ 1,-1,-1}, {0,0}},
    {{ 1, 1,-1}, {0,1}},
    {{ 1, 1, 1}, {1,1}},
    {{ 1,-1, 1}, {1,0}},

    // -Y (bottom) 
    {{-1,-1,-1}, {0,1}},
    {{-1,-1, 1}, {0,0}},
    {{ 1,-1, 1}, {1,0}},
    {{ 1,-1,-1}, {1,1}},

    // +Y (top)
    {{-1, 1,-1}, {0,0}},
    {{-1, 1, 1}, {0,1}},
    {{ 1, 1, 1}, {1,1}},
    {{ 1, 1,-1}, {1,0}},
};

constexpr std::vector<uint> CUBE_INDICES[] = {
    0, 1, 2,
    0, 2, 3,
    4, 5, 6,
    4, 6, 7,
    8, 9, 10,
    8, 10, 11,
    12, 13, 14,
    12, 14, 15,
    16, 17, 18,
    16, 18, 19,
    20, 21, 22,
    20, 22, 23
};

consteval Mesh initCube()
{
    Mesh cube(CUBE_VERTICES, CUBE_INDICES);
    return cube;
}

constexpr Mesh CUBE_MESH = initCube();

*/