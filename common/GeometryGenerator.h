#pragma once

#include <vector>

#include <DirectXMath.h>
using namespace DirectX;

class GeometryGenerator
{
public:

	struct VertexData
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT3 tangent;
		XMFLOAT2 TexCoord;

		VertexData();
		VertexData(float px, float py, float pz,
				   float nx, float ny, float nz,
				   float tx, float ty, float tz,
				   float u, float v);
	};

	struct MeshData
	{
		std::vector<VertexData> vertices;
		std::vector<uint16_t> indices16;
		std::vector<uint32_t> indices32;

		std::vector<uint16_t>& GetIndices16();
	};

	MeshData CreateBox(float width, float height, float depth, uint32_t subdivisions);
	MeshData CreateGrid(float width, float depth, uint32_t m, uint32_t n);
	MeshData CreateCylinder(float RadiusBottom, float RadiusTop, float height, uint32_t SliceCount, uint32_t StackCount);
	MeshData CreateSphere(float radius, uint32_t SliceCount, uint32_t StackCount);
};