#pragma once

#include <vector>
#include <xm/xm.h>
#include <array>
#include <unordered_map>
#include <cassert>

#include "Texture.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using uint = unsigned int;

std::unordered_map<std::string, Texture> g_textures;

struct Vertex
{
	xm::vec3 pos;
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

class Model
{
public:
	std::vector<ModelEntry> m_entries;
};

void processNode(Model& model, const aiScene* scene, const aiNode* const node);

Model loadModel(std::string_view filename)
{
	std::string path = g_data_path + std::string(filename);
	Model current_model;
	Assimp::Importer in = Assimp::Importer();
	auto* scene = in.ReadFile(path, aiProcess_Triangulate);
	if (!scene || !scene->mRootNode)
	{
		assert(false && "couldn't load scene");
	}
	processNode(current_model, scene, scene->mRootNode);
	return current_model;
}

void processNode(Model& model, const aiScene* scene, const aiNode* const node)
{
	for (uint m = 0; m < node->mNumMeshes; ++m)
	{
		aiMesh* mesh = scene->mMeshes[m];

		auto& entry = model.m_entries.emplace_back();
		entry.mesh.m_indices.reserve(mesh->mNumFaces * 3);
		entry.mesh.m_vertices.reserve(mesh->mNumVertices);
		for (uint f = 0; f < mesh->mNumFaces; ++f)
		{
			aiFace face = mesh->mFaces[f];

			entry.mesh.m_indices.emplace_back(xm::uvec3{ face.mIndices[0], face.mIndices[1], face.mIndices[2] });
			
		}
		for (uint v = 0; v < mesh->mNumVertices; ++v)
		{
			Vertex vertex;
			vertex.pos.x = mesh->mVertices[v].x;
			vertex.pos.y = mesh->mVertices[v].y;
			vertex.pos.z = mesh->mVertices[v].z;
			vertex.uv.u = mesh->mTextureCoords[0][v].x;
			vertex.uv.v = mesh->mTextureCoords[0][v].y;

			entry.mesh.m_vertices.emplace_back(vertex);
		}
		aiString path;
		scene->mMaterials[mesh->mMaterialIndex]->GetTexture(aiTextureType::aiTextureType_DIFFUSE, 0, &path);

		auto it = g_textures.find(path.C_Str());
		if (it == g_textures.end())
		{
			entry.texture = &g_textures.insert({ path.C_Str(), loadTexture(path.C_Str()) }).first->second;
		}
		else
		{
			entry.texture = &it->second;
		}
	}
	for (uint c = 0; c < node->mNumChildren; ++c)
	{
		processNode(model, scene, node->mChildren[c]);
	}
}

Mesh g_cube_mesh({
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
	},
	{
		{ 0, 1, 2 },
		{ 0, 2, 3 },
		{ 4, 5, 6 },
		{ 4, 6, 7 },
		{ 8, 9, 10 },
		{ 8, 10, 11 },
		{ 12, 13, 14 },
		{ 12, 14, 15 },
		{ 16, 17, 18 },
		{ 16, 18, 19 },
		{ 20, 21, 22 },
		{ 20, 22, 23 }
	}
);

//TODO : transforms and scene graph