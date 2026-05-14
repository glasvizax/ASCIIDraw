#pragma once

#include <vector>
#include <array>
#include <unordered_map>
#include <cassert>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <xm/xm.h>

#include "Texture.h"
#include "Aliases.h"

struct Material
{
	float ambient;
	float diffuse;
	float specular;
	float shininess;
};

struct Vertex
{
	xm::vec3 pos;
	xm::vec3 normal;
	xm::vec2 uv;
};

struct Mesh
{
	std::vector<Vertex> m_vertices;
	std::vector<xm::uvec3> m_indices;
};

struct ModelEntry
{
	Mesh mesh;
	Texture* texture;
};

struct Model
{
	std::vector<ModelEntry> m_entries;
};

extern Mesh g_cube_mesh;

void processNode(Model& model, const aiScene* scene, const aiNode* const node);

Model loadModel(std::string_view filename);

Mesh generateLandscape(float width, float height, uint precision_width, uint precision_height, float (y_func)(float, float));

//TODO : transforms and scene graph