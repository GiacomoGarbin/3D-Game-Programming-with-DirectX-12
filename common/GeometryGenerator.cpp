#include "GeometryGenerator.h"
#include <cmath>

GeometryGenerator::VertexData::VertexData()
{}

GeometryGenerator::VertexData::VertexData(float px, float py, float pz,
										  float nx, float ny, float nz,
										  float tx, float ty, float tz,
										  float u, float v) :
	position(px, py, pz),
	normal(nx, ny, nz),
	tangent(tx, ty, tz),
	TexCoord(u, v)
{}

std::vector<uint16_t>& GeometryGenerator::MeshData::GetIndices16()
{
	if (indices16.empty())
	{
		for (uint32_t i : indices32)
		{
			indices16.push_back(static_cast<uint16_t>(i));
		}
	}

	return indices16;
}

GeometryGenerator::MeshData GeometryGenerator::CreateBox(float width, float height, float depth, uint32_t subdivisions)
{
	MeshData mesh;

	std::vector<VertexData>& vertices = mesh.vertices;
	vertices.resize(24);

	// half extents
	float w = 0.5f * width;
	float h = 0.5f * height;
	float d = 0.5f * depth;
	
	// front
	vertices[ 0] = VertexData(-w, -h, -d,  0,  0, -1, +1,  0,  0,  0, +1);
	vertices[ 1] = VertexData(-w, +h, -d,  0,  0, -1, +1,  0,  0,  0,  0);
	vertices[ 2] = VertexData(+w, +h, -d,  0,  0, -1, +1,  0,  0, +1,  0);
	vertices[ 3] = VertexData(+w, -h, -d,  0,  0, -1, +1,  0,  0, +1, +1);

	// back
	vertices[ 4] = VertexData(-w, -h, +d,  0,  0, +1, -1,  0,  0, +1, +1);
	vertices[ 5] = VertexData(+w, -h, +d,  0,  0, +1, -1,  0,  0,  0, +1);
	vertices[ 6] = VertexData(+w, +h, +d,  0,  0, +1, -1,  0,  0,  0,  0);
	vertices[ 7] = VertexData(-w, +h, +d,  0,  0, +1, -1,  0,  0, +1,  0);

	// top
	vertices[ 8] = VertexData(-w, +h, -d,  0, +1,  0, +1,  0,  0,  0, +1);
	vertices[ 9] = VertexData(-w, +h, +d,  0, +1,  0, +1,  0,  0,  0,  0);
	vertices[10] = VertexData(+w, +h, +d,  0, +1,  0, +1,  0,  0, +1,  0);
	vertices[11] = VertexData(+w, +h, -d,  0, +1,  0, +1,  0,  0, +1, +1);

	// bottom
	vertices[12] = VertexData(-w, -h, -d,  0, -1,  0, -1,  0,  0, +1, +1);
	vertices[13] = VertexData(+w, -h, -d,  0, -1,  0, -1,  0,  0,  0, +1);
	vertices[14] = VertexData(+w, -h, +d,  0, -1,  0, -1,  0,  0,  0,  0);
	vertices[15] = VertexData(-w, -h, +d,  0, -1,  0, -1,  0,  0, +1,  0);

	// left
	vertices[16] = VertexData(-w, -h, +d, -1,  0,  0,  0,  0, -1,  0, +1);
	vertices[17] = VertexData(-w, +h, +d, -1,  0,  0,  0,  0, -1,  0,  0);
	vertices[18] = VertexData(-w, +h, -d, -1,  0,  0,  0,  0, -1, +1,  0);
	vertices[19] = VertexData(-w, -h, -d, -1,  0,  0,  0,  0, -1, +1, +1);

	// right
	vertices[20] = VertexData(+w, -h, -d, +1,  0,  0,  0,  0, +1,  0, +1);
	vertices[21] = VertexData(+w, +h, -d, +1,  0,  0,  0,  0, +1,  0,  0);
	vertices[22] = VertexData(+w, +h, +d, +1,  0,  0,  0,  0, +1, +1,  0);
	vertices[23] = VertexData(+w, -h, +d, +1,  0,  0,  0,  0, +1, +1, +1);

	std::vector<uint32_t>& indices = mesh.indices32;
	indices.resize(36);

	// front
	indices[ 0] =  0; indices[ 1] =  1; indices[ 2] =  2;
	indices[ 3] =  0; indices[ 4] =  2; indices[ 5] =  3;

	// back
	indices[ 6] =  4; indices[ 7] =  5; indices[ 8] =  6;
	indices[ 9] =  4; indices[10] =  6; indices[11] =  7;

	// top
	indices[12] =  8; indices[13] =  9; indices[14] = 10;
	indices[15] =  8; indices[16] = 10; indices[17] = 11;

	// bottom
	indices[18] = 12; indices[19] = 13; indices[20] = 14;
	indices[21] = 12; indices[22] = 14; indices[23] = 15;

	// left
	indices[24] = 16; indices[25] = 17; indices[26] = 18;
	indices[27] = 16; indices[28] = 18; indices[29] = 19;

	// right
	indices[30] = 20; indices[31] = 21; indices[32] = 22;
	indices[33] = 20; indices[34] = 22; indices[35] = 23;

	// TODO: subdivisions

	return mesh;
}

GeometryGenerator::MeshData GeometryGenerator::CreateGrid(float width, float depth, uint32_t m, uint32_t n)
{
	MeshData mesh;

	uint32_t VertexCount = m * n;
	uint32_t FaceCount = (m - 1) * (n - 1) * 2;

	float HalfWidth = 0.5f * width;
	float HalfDepth = 0.5f * depth;

	float dx = width / (n - 1);
	float dz = depth / (m - 1);

	float du = 1.0f / (n - 1);
	float dv = 1.0f / (m - 1);

	mesh.vertices.resize(VertexCount);

	for (uint32_t i = 0; i < m; ++i)
	{
		float z = HalfDepth - i * dz;

		for (uint32_t j = 0; j < n; ++j)
		{
			float x = j * dx - HalfWidth;

			mesh.vertices[i * n + j].position = XMFLOAT3(x, 0.0f, z);
			mesh.vertices[i * n + j].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
			mesh.vertices[i * n + j].tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);
			mesh.vertices[i * n + j].TexCoord = XMFLOAT2(j * du, i * dv);
		}
	}

	mesh.indices32.resize(FaceCount * 3);

	uint32_t k = 0;

	for (uint32_t i = 0; i < m - 1; ++i)
	{
		for (uint32_t j = 0; j < n - 1; ++j)
		{
			mesh.indices32[k + 0] = (i + 0) * n + (j + 0);
			mesh.indices32[k + 1] = (i + 0) * n + (j + 1);
			mesh.indices32[k + 2] = (i + 1) * n + (j + 0);

			mesh.indices32[k + 3] = (i + 1) * n + (j + 0);
			mesh.indices32[k + 4] = (i + 0) * n + (j + 1);
			mesh.indices32[k + 5] = (i + 1) * n + (j + 1);

			k += 6;
		}
	}

	return mesh;
}

GeometryGenerator::MeshData GeometryGenerator::CreateCylinder(float RadiusBottom, float RadiusTop, float height, uint32_t SliceCount, uint32_t StackCount)
{
	MeshData mesh;

	float StackHeight = height / StackCount;

	float RadiusStep = (RadiusTop - RadiusBottom) / StackCount;

	uint32_t RingCount = StackCount + 1;

	float DeltaTheta = 2.0f * XM_PI / SliceCount;

	for (uint32_t i = 0; i < RingCount; ++i)
	{
		float y = i * StackHeight - 0.5f * height;
		float r = RadiusBottom + i * RadiusStep;

		for (uint32_t j = 0; j <= SliceCount; ++j)
		{
			VertexData vertex;

			float c = std::cos(j * DeltaTheta);
			float s = std::sin(j * DeltaTheta);

			vertex.position = XMFLOAT3(r * c, y, r * s);

			vertex.TexCoord.x = static_cast<float>(j) / SliceCount;
			vertex.TexCoord.y = 1.0f - static_cast<float>(i) / StackCount;

			vertex.tangent = XMFLOAT3(-s, 0.0f, +c);

			float DeltaRadius = RadiusBottom - RadiusTop;

			XMFLOAT3 bitangent(DeltaRadius * c, -height, DeltaRadius * s);

			XMVECTOR T = XMLoadFloat3(&vertex.tangent);
			XMVECTOR B = XMLoadFloat3(&bitangent);
			XMVECTOR N = XMVector3Normalize(XMVector3Cross(T, B));
			XMStoreFloat3(&vertex.normal, N);

			mesh.vertices.push_back(vertex);
		}
	}

	uint32_t RingVertexCount = SliceCount + 1;

	for (uint32_t i = 0; i < StackCount; ++i)
	{
		for (uint32_t j = 0; j < SliceCount; ++j)
		{
			mesh.indices32.push_back((i + 0) * RingVertexCount + (j + 0));
			mesh.indices32.push_back((i + 1) * RingVertexCount + (j + 0));
			mesh.indices32.push_back((i + 1) * RingVertexCount + (j + 1));

			mesh.indices32.push_back((i + 0) * RingVertexCount + (j + 0));
			mesh.indices32.push_back((i + 1) * RingVertexCount + (j + 1));
			mesh.indices32.push_back((i + 0) * RingVertexCount + (j + 1));
		}
	}

	auto BuildCylinderCap = [](MeshData& mesh,
							   float height,
							   uint32_t SliceCount,
							   float DeltaTheta,
							   float sign,
							   float radius)
	{
		uint32_t BaseIndex = mesh.vertices.size();

		float y = sign * 0.5f * height;

		for (uint32_t j = 0; j <= SliceCount; ++j)
		{
			VertexData vertex;

			float x = radius * std::cos(j * DeltaTheta);
			float z = radius * std::sin(j * DeltaTheta);

			float u = x / height + 0.5f;
			float v = z / height + 0.5f;

			vertex.position = XMFLOAT3(x, y, z);
			vertex.normal = XMFLOAT3(0.0f, sign, 0.0f);
			vertex.tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);
			vertex.TexCoord = XMFLOAT2(u, v);

			mesh.vertices.push_back(vertex);
		}

		VertexData CenterVertex;

		CenterVertex.position = XMFLOAT3(0.0f, y, 0.0f);
		CenterVertex.normal = XMFLOAT3(0.0f, sign, 0.0f);
		CenterVertex.tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);
		CenterVertex.TexCoord = XMFLOAT2(0.5f, 0.5f);

		mesh.vertices.push_back(CenterVertex);

		uint32_t CenterIndex = mesh.vertices.size() - 1;

		for (uint32_t j = 0; j < SliceCount; ++j)
		{
			mesh.indices32.push_back(CenterIndex);
			mesh.indices32.push_back(BaseIndex + j + (sign > 0 ? 1 : 0));
			mesh.indices32.push_back(BaseIndex + j + (sign > 0 ? 0 : 1));
		}
	};

	BuildCylinderCap(mesh, height, SliceCount, DeltaTheta, +1.0f, RadiusTop);
	BuildCylinderCap(mesh, height, SliceCount, DeltaTheta, -1.0f, RadiusBottom);

	return mesh;
}

GeometryGenerator::MeshData GeometryGenerator::CreateSphere(float radius, uint32_t SliceCount, uint32_t StackCount)
{
	MeshData mesh;

	VertexData VertexPoleTop;
	VertexPoleTop.position = XMFLOAT3(0.0f, +radius, 0.0f);
	VertexPoleTop.normal   = XMFLOAT3(0.0f, +1.0f, 0.0f);
	VertexPoleTop.tangent  = XMFLOAT3(1.0f, 0.0f, 0.0f);
	VertexPoleTop.TexCoord = XMFLOAT2(0.0f, 0.0f);

	mesh.vertices.push_back(VertexPoleTop);
	
	float DeltaPhi = XM_PI / StackCount;
	float DeltaTheta = 2.0f * XM_PI / SliceCount;

	for (uint32_t i = 1; i < StackCount; ++i) // <-
	{
		float phi = i * DeltaPhi;

		for (uint32_t j = 0; j <= SliceCount; ++j)
		{
			float theta = j * DeltaTheta;

			VertexData vertex;

			// spherical to cartesian
			vertex.position.x = radius * std::sin(phi) * std::cos(theta);
			vertex.position.y = radius * std::cos(phi);
			vertex.position.z = radius * std::sin(phi) * std::sin(theta);

			XMVECTOR P = XMLoadFloat3(&vertex.position);
			XMStoreFloat3(&vertex.normal, XMVector3Normalize(P));

			// partial derivative of P with respect to theta
			vertex.tangent.x = -radius * std::sin(phi) * std::sin(theta);
			vertex.tangent.y = 0.0f;
			vertex.tangent.z = +radius * std::sin(phi) * std::cos(theta);

			XMVECTOR T = XMLoadFloat3(&vertex.tangent);
			XMStoreFloat3(&vertex.tangent, XMVector3Normalize(T));

			vertex.TexCoord.x = theta / XM_2PI;
			vertex.TexCoord.y = phi / XM_PI;

			mesh.vertices.push_back(vertex);
		}
	}

	VertexData VertexPoleBottom;
	VertexPoleBottom.position = XMFLOAT3(0.0f, -radius, 0.0f);
	VertexPoleBottom.normal = XMFLOAT3(0.0f, -1.0f, 0.0f);
	VertexPoleBottom.tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);
	VertexPoleBottom.TexCoord = XMFLOAT2(0.0f, 1.0f);

	mesh.vertices.push_back(VertexPoleBottom);

	for (uint32_t i = 1; i <= SliceCount; ++i)
	{
		mesh.indices32.push_back(0);
		mesh.indices32.push_back(i + 1);
		mesh.indices32.push_back(i);
	}

	uint32_t BaseIndex = 1;
	uint32_t RingVertexCount = SliceCount + 1;

	for (uint32_t i = 0; i < StackCount - 2; ++i)
	{
		for (uint32_t j = 0; j < SliceCount; ++j)
		{
			mesh.indices32.push_back(BaseIndex + (i + 0) * RingVertexCount + (j + 0));
			mesh.indices32.push_back(BaseIndex + (i + 0) * RingVertexCount + (j + 1));
			mesh.indices32.push_back(BaseIndex + (i + 1) * RingVertexCount + (j + 0));

			mesh.indices32.push_back(BaseIndex + (i + 1) * RingVertexCount + (j + 0));
			mesh.indices32.push_back(BaseIndex + (i + 0) * RingVertexCount + (j + 1));
			mesh.indices32.push_back(BaseIndex + (i + 1) * RingVertexCount + (j + 1));
		}
	}

	uint32_t VertexPoleBottomIndex = mesh.vertices.size() - 1;

	BaseIndex = VertexPoleBottomIndex - RingVertexCount;

	for (uint32_t i = 0; i < SliceCount; ++i)
	{
		mesh.indices32.push_back(VertexPoleBottomIndex);
		mesh.indices32.push_back(BaseIndex + i);
		mesh.indices32.push_back(BaseIndex + i + 1);
	}

	return mesh;
}

GeometryGenerator::MeshData GeometryGenerator::CreateQuad(float x, float y, float w, float h, float depth)
{
	MeshData mesh;

	mesh.vertices.resize(4);
	mesh.indices32.resize(6);

	// position coordinates specified in NDC space
	mesh.vertices[0] = VertexData(x,     y - h, depth, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	mesh.vertices[1] = VertexData(x,     y,     depth, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	mesh.vertices[2] = VertexData(x + w, y,     depth, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	mesh.vertices[3] = VertexData(x + w, y - h, depth, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	mesh.indices32[0] = 0;
	mesh.indices32[1] = 1;
	mesh.indices32[2] = 2;

	mesh.indices32[3] = 0;
	mesh.indices32[4] = 2;
	mesh.indices32[5] = 3;

	return mesh;
}