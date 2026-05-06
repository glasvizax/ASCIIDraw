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

class SceneObject;
class Scene;

std::unordered_map<std::string, Texture> g_textures;

struct Vertex
{
	xm::vec3 pos;
	xm::vec2 uv;
};

class Mesh
{
public:
	std::vector<Vertex> m_vertices;
	std::vector<uint> m_indices;
	Mesh(const std::vector<Vertex>& vertices, const std::vector<uint>& indices) :
		m_vertices(vertices),
		m_indices(indices)
	{
	}
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

void loadModel(std::string_view filename)
{
	std::string path = g_data_path + std::string(filename);
	Model current_model;
	Assimp::Importer in = Assimp::Importer();
	auto* scene = in.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
	if (!scene || !scene->mRootNode)
	{
		assert(false && "couldn't load scene");
	}
	processNode(current_model, scene, scene->mRootNode);
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

			for (uint i = 0; i < face.mNumIndices; ++i)
			{
				entry.mesh.m_indices.emplace_back(face.mIndices[i]);
			}
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
	}
);


/*
struct Transform
{
	//xm::vec3 m_rotation; TODO: eulers and quaternions
	xm::vec3 translation;
	xm::vec3 scale;

	xm::mat4 local;
	xm::mat4 world;

	bool local_dirty = true;
	bool world_dirty = true;
};

class SceneObject
{
	friend Scene;
	std::vector<SceneObject*> m_children;
	SceneObject* m_parent;

	Transform m_transform;
	Model m_model;

public:

	xm::vec3 getScale() const
	{
		return m_transform.scale;
	}

	xm::vec3 getTranslation() const
	{
		return m_transform.translation;
	}

	void setScale(xm::vec3 new_scale) 
	{
		m_transform.scale = new_scale;
		m_transform.local_dirty = true;
	}

	void setTranslation(xm::vec3 new_translation)
	{
		m_transform.translation = new_translation;
		m_transform.local_dirty = true;
	}

	void setParent(SceneObject* parent)
	{
		m_parent = parent;
	}

	void addChild(SceneObject* scene_object)
	{
		m_children.emplace_back(scene_object);
	}

	void update(float dt)
	{
		if(m_transform.local_dirty)
		{
			xm::mat4 new_local(1.0f);
			new_local = xm::scale(new_local, m_transform.scale);
			//TODO : rotation 
			new_local = xm::translate(new_local, m_transform.translation);
			m_transform.local = new_local;

		}
	}
};

class Scene
{
	std::vector<SceneObject*> m_children;

public:

	void addChild(SceneObject* child)
	{
		m_children.emplace_back(child)->m_parent = nullptr;
	}

	void update(float dt) 
	{
		for(auto o : m_children)
		{
			o->update(dt);
		}
	}

	static void addSubChild(SceneObject* parent, SceneObject* child)
	{
		parent->m_children.emplace_back(child);
		child->m_parent = child;
	}
};
*/