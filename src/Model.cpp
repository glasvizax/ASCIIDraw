#include "Model.h"

static std::unordered_map<std::string, Texture> s_textures;

Model loadModel(std::string_view filename)
{
	std::string path = g_data_path + std::string(filename);
	Model current_model;
	Assimp::Importer in = Assimp::Importer();
	auto* scene = in.ReadFile(path, aiProcess_Triangulate);
	if (!scene || !scene->mRootNode)
	{
		assert(false && "couldn't load scene");
		return current_model;
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

		auto it = s_textures.find(path.C_Str());
		if (it == s_textures.end())
		{
			entry.texture = &s_textures.insert({ path.C_Str(), loadTexture(path.C_Str()) }).first->second;
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

Mesh generateLandscape(float width, float height, uint precision_width, uint precision_height, float(y_func)(float, float))
{
	Mesh res;
	res.m_vertices.reserve((precision_width + 1) * (precision_height + 1));
	res.m_indices.reserve((precision_width * 2) * precision_height);

	const uint stride = precision_width + 1;
	for (uint j = 0; j < precision_height; ++j)
	{
		for (uint i = 0; i < precision_width; ++i)
		{
			uint a = j * stride + i;
			uint b = a + 1;
			uint c = a + stride;
			uint d = c + 1;
			res.m_indices.emplace_back(xm::uvec3{ a, b, c });
			res.m_indices.emplace_back(xm::uvec3{ b, d, c });
		}
	}

	float start_x = -width / 2;
	float start_z = height / 2;
	float x_step = width / precision_width;
	float z_step = -height / precision_height;

	float dx = 0.01;
	float dz = 0.01;
	for (uint j = 0; j < precision_height + 1; ++j)
	{
		for (uint i = 0; i < precision_width + 1; ++i)
		{
			float x = start_x + i * x_step;
			float z = start_z + j * z_step;
			float y = y_func(x, z);

			float ydx = y_func(x + dx, z);
			float ydz = y_func(x, z + dz);
			xm::vec3 normal = xm::normalize(xm::cross(xm::vec3(0.0f, ydz - y, dz), xm::vec3(dx, ydx - y, 0.0f)));

			float u = i / static_cast<float>(precision_width);
			float v = j / static_cast<float>(precision_height);
			xm::vec3 pos(x, y, z);
			xm::vec2 uv(u, v);
			res.m_vertices.emplace_back(Vertex(pos, normal, uv));
		}
	}

	return res;
}

Mesh g_cube_mesh(
	{
		// -Z (front)
		{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
		{{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
		{{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
		{{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},

		// +Z (back)
		{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
		{{ 1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
		{{ 1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		{{-1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},

		// -X (left)
		{{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
		{{-1.0f,  1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
		{{-1.0f,  1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
		{{-1.0f, -1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

		// +X (right)
		{{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{ 1.0f,  1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
		{{ 1.0f,  1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
		{{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},

		// -Y (bottom)
		{{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
		{{-1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
		{{ 1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
		{{ 1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},

		// +Y (top)
		{{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
		{{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
		{{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
		{{ 1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	},
	{
		{ 0,  1,  2 },
		{ 0,  2,  3 },
		{ 4,  5,  6 },
		{ 4,  6,  7 },
		{ 8,  9, 10 },
		{ 8, 10, 11 },
		{ 12, 13, 14 },
		{ 12, 14, 15 },
		{ 16, 17, 18 },
		{ 16, 18, 19 },
		{ 20, 21, 22 },
		{ 20, 22, 23 }
	}
);