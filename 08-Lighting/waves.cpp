#include "waves.h"

Waves::Waves(int rows, int cols, float dt, float dx, float speed, float damping) :
	mRowCount(rows),
	mColCount(cols),
	mVertexCount(rows*cols),
	mTriangleCount((rows-1)*(cols-1)*2),
	mTimeStep(dt),
	mSpaceStep(dx)
{
	float d = damping * dt + 2.0f;
	float e = (speed * speed) * (dt * dt) / (dx * dx);
	mK1 = (damping * dt - 2.0f) / d;
	mK2 = (4.0f - 8.0f * e) / d;
	mK3 = (2.0f * e) / d;

	mPrevData.resize(mVertexCount);
	mCurrData.resize(mVertexCount);

	mNormals.resize(mVertexCount);

	float HalfWidth = (cols - 1) * dx * 0.5f;
	float HalfDepth = (rows - 1) * dx * 0.5f;

	for (int i = 0; i < rows; ++i)
	{
		float z = HalfDepth - i * dx;

		for (int j = 0; j < cols; ++j)
		{
			float x = j * dx - HalfWidth;

			mPrevData[i * cols + j] = XMFLOAT3(x, 0.0f, z);
			mCurrData[i * cols + j] = XMFLOAT3(x, 0.0f, z);

			mNormals[i * cols + j] = XMFLOAT3(0.0f, 1.0f, 0.0f);
		}
	}
}

Waves::~Waves()
{}

int Waves::RowCount() const
{
	return mRowCount;
}

int Waves::ColCount() const
{
	return mColCount;
}

int Waves::VertexCount() const
{
	return mVertexCount;
}

int Waves::TriangleCount() const
{
	return mTriangleCount;
}

float Waves::GetWidth() const
{
	return mColCount * mSpaceStep;
}

float Waves::GetDepth() const
{
	return mRowCount * mSpaceStep;
}

const XMFLOAT3& Waves::GetPosition(int i) const
{
	return mCurrData[i];
}

const XMFLOAT3& Waves::GetNormal(int i) const
{
	return mNormals[i];
}

void Waves::update(float dt)
{
	static float t = 0;

	t += dt;

	if (t >= mTimeStep)
	{
		for (int i = 1; i < mRowCount - 1; ++i)
		{
			for (int j = 1; j < mColCount - 1; ++j)
			{
				mPrevData[i * mColCount + j].y =
					mK1 * mPrevData[i * mColCount + j].y +
					mK2 * mCurrData[i * mColCount + j].y +
					mK3 * (mCurrData[(i + 1) * mColCount + (j + 0)].y +
						   mCurrData[(i - 1) * mColCount + (j + 0)].y +
						   mCurrData[(i + 0) * mColCount + (j + 1)].y +
						   mCurrData[(i + 0) * mColCount + (j - 1)].y);
			}
		}

		std::swap(mPrevData, mCurrData);

		t = 0.0f;

		// compute normals and tangents
		for (int i = 1; i < mRowCount - 1; ++i)
		{
			for (int j = 1; j < mColCount - 1; ++j)
			{
				float l = mCurrData[(i + 0) * mColCount + (j - 1)].y;
				float r = mCurrData[(i + 0) * mColCount + (j + 1)].y;
				float t = mCurrData[(i - 1) * mColCount + (j + 0)].y;
				float b = mCurrData[(i + 1) * mColCount + (j + 0)].y;
				
				mNormals[i * mColCount + j].x = l - r;
				mNormals[i * mColCount + j].y = 2.0f * mSpaceStep;
				mNormals[i * mColCount + j].z = b - t;

				XMStoreFloat3(&mNormals[i * mColCount + j], XMVector3Normalize(XMLoadFloat3(&mNormals[i * mColCount + j])));

				// compute tangent
			}
		}
	}
}

void Waves::disturb(int i, int j, float magnitude)
{
	assert(i > 1 && i < mRowCount - 2);
	assert(j > 1 && j < mColCount - 2);

	float HalfMagnitude = magnitude * 0.5f;

	mCurrData[(i + 0) * mColCount + (j + 0)].y += magnitude;
	mCurrData[(i + 0) * mColCount + (j + 1)].y += HalfMagnitude;
	mCurrData[(i + 0) * mColCount + (j - 1)].y += HalfMagnitude;
	mCurrData[(i + 1) * mColCount + (j + 0)].y += HalfMagnitude;
	mCurrData[(i - 1) * mColCount + (j + 0)].y += HalfMagnitude;
}