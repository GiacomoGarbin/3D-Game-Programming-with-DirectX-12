#pragma once

#include <vector>

#include <DirectXMath.h>
using namespace DirectX;

class Waves
{
	int mRowCount = 0;
	int mColCount = 0;

	int mVertexCount = 0;
	int mTriangleCount = 0;

	float mK1 = 0.0f;
	float mK2 = 0.0f;
	float mK3 = 0.0f;

	float mTimeStep = 0.0f;
	float mSpaceStep = 0.0f;

	std::vector<XMFLOAT3> mPrevData;
	std::vector<XMFLOAT3> mCurrData;

public:
	Waves(int rows, int cols, float dt, float dx, float speed, float damping);
	~Waves();

	int RowCount() const;
	int ColCount() const;
	int VertexCount() const;
	int TriangleCount() const;
	float GetWidth() const;
	float GetDepth() const;

	const XMFLOAT3& GetPosition(int i) const;

	void update(float dt);
	void disturb(int i, int j, float magnitude);
};

