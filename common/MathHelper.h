#ifndef MATH_HELPER_H
#define MATH_HELPER_H

#include <cmath>

#include <DirectXMath.h>
using namespace DirectX;

class MathHelper
{
public:
	static const float infinity;

	static XMFLOAT4X4 Identity4x4()
	{
		static XMFLOAT4X4 I(1.0f, 0.0f, 0.0f, 0.0f,
							0.0f, 1.0f, 0.0f, 0.0f,
							0.0f, 0.0f, 1.0f, 0.0f,
							0.0f, 0.0f, 0.0f, 1.0f);
		return I;
	}

	template<typename T>
	static T clamp(const T& x, const T& a, const T& b);

	static float RandFloat(); // random float in [0, 1)
	static float RandFloat(float a, float b); // random float in [a, b)
	static int RandInt(int a, int b); // random int in [a, b]

	static XMVECTOR SphericalToCartesian(float radius, float theta, float phi);
};

#endif // MATH_HELPER_H