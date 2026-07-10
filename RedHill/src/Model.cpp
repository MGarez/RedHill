#include "Model.h"

#include <map>
#include <cmath>
#include <cstdint>
#include <algorithm>

#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "mikktspace.h"

struct MeshLoader
{
	std::vector<Vertex>& vertex;
	std::vector<uint32_t>& index;

	static int GetNumFaces(const SMikkTSpaceContext* ctx)
	{
		MeshLoader* meshLoader = static_cast<MeshLoader*>(ctx->m_pUserData);
		return meshLoader->index.size() / 3;
	}

	static int GetNumVerticesOfFace(const SMikkTSpaceContext* ctx, const int face)
	{
		return 3;
	}

	static void GetPosition(const SMikkTSpaceContext* ctx, float posOut[3], const int face, const int vert)
	{
		MeshLoader* meshLoader = static_cast<MeshLoader*>(ctx->m_pUserData);
		const uint32_t& vI = meshLoader->index[face * 3 + vert];
		const Vertex& v = meshLoader->vertex[vI];
		posOut[0] = v.position[0];
		posOut[1] = v.position[1];
		posOut[2] = v.position[2];
	}

	static void GetNormal(const SMikkTSpaceContext* ctx, float normOut[3], const int face, const int vert)
	{
		MeshLoader* meshLoader = static_cast<MeshLoader*>(ctx->m_pUserData);
		const uint32_t& vI = meshLoader->index[face * 3 + vert];
		const Vertex& v = meshLoader->vertex[vI];
		normOut[0] = v.normal[0];
		normOut[1] = v.normal[1];
		normOut[2] = v.normal[2];
	}
	static void GetTexCoord(const SMikkTSpaceContext* ctx, float texcOut[2], const int face, const int vert)
	{
		MeshLoader* meshLoader = static_cast<MeshLoader*>(ctx->m_pUserData);
		const uint32_t& vI = meshLoader->index[face * 3 + vert];
		const Vertex& v = meshLoader->vertex[vI];
		texcOut[0] = v.uv[0];
		texcOut[1] = v.uv[1];
	}
	static void SetTSpaceBasic(const SMikkTSpaceContext* ctx, const float tangent[3], const float sign, const int face, const int vert)
	{
		MeshLoader* meshLoader = static_cast<MeshLoader*>(ctx->m_pUserData);
		const uint32_t& vI = meshLoader->index[face * 3 + vert];
		Vertex& v = meshLoader->vertex[vI];
		v.tangent[0] = tangent[0];
		v.tangent[1] = tangent[1];
		v.tangent[2] = tangent[2];
		v.tangent[3] = sign;
	}
};

struct VertexKey
{
	int32_t posIndex;
	int32_t uvIndex;
	int32_t normIndex;

	bool operator<(const VertexKey& o) const noexcept
	{
		if (posIndex == o.posIndex)
		{
			if (uvIndex == o.uvIndex)
			{
				return normIndex < o.normIndex;
			}
			return uvIndex < o.uvIndex;
		}
		return posIndex < o.posIndex;
	}
};

void PBRMesh::GenerateVertexAndIndexFromObj(const std::string& objFile)
{
	tinyobj::ObjReaderConfig config;
	config.triangulate = true;

	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(objFile, config))
	{
		if (!reader.Error().empty())
		{
			::OutputDebugStringA(reader.Error().c_str());
			::__debugbreak();
		}
		// TODO: Handle error
	}

	if (!reader.Warning().empty())
	{
		::OutputDebugStringA(reader.Warning().c_str());
	}

	const tinyobj::attrib_t& attrib = reader.GetAttrib();
	const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
	const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();

	std::map<VertexKey, uint32_t> vertexMap;

	for (const auto& shape : shapes)
	{
		for (const auto& idx : shape.mesh.indices)
		{
			VertexKey key{ idx.vertex_index, idx.texcoord_index, idx.normal_index };

			auto it = vertexMap.find(key);
			if (it == vertexMap.end())
			{
				Vertex v;
				v.position[0] = attrib.vertices[3 * idx.vertex_index];
				v.position[1] = attrib.vertices[3 * idx.vertex_index + 1];
				v.position[2] = attrib.vertices[3 * idx.vertex_index + 2];

				v.uv[0] = attrib.texcoords[2 * idx.texcoord_index];
				// Convert the uvs to d3d12 conventions
				v.uv[1] = 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1];

				v.normal[0] = attrib.normals[3 * idx.normal_index];
				v.normal[1] = attrib.normals[3 * idx.normal_index + 1];
				v.normal[2] = attrib.normals[3 * idx.normal_index + 2];

				v.tangent[0] = 0.0f;
				v.tangent[1] = 0.0f;
				v.tangent[2] = 0.0f;
				v.tangent[3] = 0.0f;

				uint32_t newIndex = vertices_data.size();
				vertices_data.push_back(v);
				vertexMap[key] = newIndex;
				indices_data.push_back(newIndex);
			}
			else
			{
				indices_data.push_back(it->second);
			}
		}
	}

	// Tangent space generation

	MeshLoader mesh{ vertices_data, indices_data };

	SMikkTSpaceInterface ispace = {};
	ispace.m_getNumFaces = MeshLoader::GetNumFaces;
	ispace.m_getNumVerticesOfFace = MeshLoader::GetNumVerticesOfFace;
	ispace.m_getPosition = MeshLoader::GetPosition;
	ispace.m_getNormal = MeshLoader::GetNormal;
	ispace.m_getTexCoord = MeshLoader::GetTexCoord;
	ispace.m_setTSpaceBasic = MeshLoader::SetTSpaceBasic;
	ispace.m_setTSpace = nullptr;

	SMikkTSpaceContext ctx = {};
	ctx.m_pUserData = &mesh;
	ctx.m_pInterface = &ispace;

	//  We could check the return to handle errors
	genTangSpaceDefault(&ctx);

}

void PBRMesh::GenerateSphere(uint32_t subdivisions)
{
	vertices_data.clear();
	indices_data.clear();

	// Add the vertices to the vertex buffer, project them into the unit sphere and keep track of the indices in the index buffer
	auto AddVertex = [this](float x, float y, float z) -> uint32_t
	{
		float invLength = 1.0f / sqrtf(x * x + y * y + z * z);
		Vertex v = {};
		v.position[0] = x * invLength;
		v.position[1] = y * invLength;
		v.position[2] = z * invLength;

		// Normal is the same as the position for a unit sphere
		v.normal[0] = v.position[0];
		v.normal[1] = v.position[1];
		v.normal[2] = v.position[2];

		uint32_t index = static_cast<uint32_t>(vertices_data.size());
		vertices_data.push_back(v);
		return index;
	};

	const float phi = (1.0f + sqrt(5.0f)) * 0.5f; // golden ratio
	// Add the 12 base vertices of an icosahedron

	AddVertex(-1.0f, phi, 0.0f);
	AddVertex(1.0f, phi, 0.0f);
	AddVertex(-1.0f, -phi, 0.0f);
	AddVertex(1.0f, -phi, 0.0f);

	AddVertex(0.0f, -1.0f, phi);
	AddVertex(0.0f, 1.0f, phi);
	AddVertex(0.0f, -1.0f, -phi);
	AddVertex(0.0f, 1.0f, -phi);

	AddVertex(phi, 0.0f, -1.0f);
	AddVertex(phi, 0.0f, 1.0f);
	AddVertex(-phi, 0.0f, -1.0f);
	AddVertex(-phi, 0.0f, 1.0f);

	// Build an auxiliary base index buffer
	std::vector<uint32_t> baseIndices =
	{
		0, 11, 5,   0, 5, 1,    0, 1, 7,    0, 7, 10,   0, 10, 11,
		1, 5, 9,    5, 11, 4,   11, 10, 2,  10, 7, 6,   7, 1, 8,
		3, 9, 4,    3, 4, 2,    3, 2, 6,    3, 6, 8,    3, 8, 9,
		4, 9, 5,    2, 4, 11,   6, 2, 10,   8, 6, 7,    9, 8, 1
	};

	std::map<uint64_t, uint32_t> vertexMidPointCache;

	auto GetMidPoint = [&](uint32_t a, uint32_t b) -> uint32_t
	{
			auto lohi = std::minmax(a, b);
			auto key = static_cast<uint64_t>(lohi.first) << 32 | static_cast<uint64_t>(lohi.second);
			auto it = vertexMidPointCache.find(key);
			if (it != vertexMidPointCache.end())
			{
				return it->second;
			}

			const Vertex& va = vertices_data[a];
			const Vertex& vb = vertices_data[b];
			uint32_t midIndex = AddVertex(
				(va.position[0] + vb.position[0]) * 0.5f,
				(va.position[1] + vb.position[1]) * 0.5f,
				(va.position[2] + vb.position[2]) * 0.5f
			);
			vertexMidPointCache[key] = midIndex;
			return midIndex;
	};

	for (uint32_t i = 0; i < subdivisions; ++i)
	{
		std::vector<uint32_t> newIndices;
		for (size_t j = 0; j < baseIndices.size(); j += 3)
		{
			uint32_t v0 = baseIndices[j];
			uint32_t v1 = baseIndices[j + 1];
			uint32_t v2 = baseIndices[j + 2];

			uint32_t v01 = GetMidPoint(v0, v1);
			uint32_t v12 = GetMidPoint(v1, v2);
			uint32_t v20 = GetMidPoint(v2, v0);

			newIndices.insert(newIndices.end(), { v0, v01, v20 });
			newIndices.insert(newIndices.end(), { v1, v12, v01 });
			newIndices.insert(newIndices.end(), { v2, v20, v12 });
			newIndices.insert(newIndices.end(), { v01, v12, v20 });
		}
		baseIndices = std::move(newIndices);
	}

	indices_data = std::move(baseIndices);
}

void PBRMesh::GenerateFloor(float size)
{
	// Lets assume a square floor centered at the origin, with a given size
	vertices_data.clear();
	indices_data.clear();

	auto AddVertex = [this](float x, float y, float z) -> uint32_t
		{
			Vertex v = {};
			v.position[0] = x;
			v.position[1] = y;
			v.position[2] = z;

			// Normal is up for a flat floor
			v.normal[0] = 0;
			v.normal[1] = 1;
			v.normal[2] = 0;

			uint32_t index = static_cast<uint32_t>(vertices_data.size());
			vertices_data.push_back(v);
			return index;
		};

	AddVertex(-size / 2.0f, -1.0f, -size / 2.0f);
	AddVertex(size / 2.0f, -1.0f, size / 2.0f);
	AddVertex(size / 2.0f, -1.0f, -size / 2.0f);
	AddVertex(-size / 2.0f, -1.0f, size / 2.0f);

	indices_data = { 0, 1, 2, 0, 3, 1 };
}
