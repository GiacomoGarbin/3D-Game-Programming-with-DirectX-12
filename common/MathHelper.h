#ifndef MATH_HELPER_H
#define MATH_HELPER_H

#include <cmath>

#include "utils.h"

class MathHelper
{
public:
	static XMFLOAT4X4 Identity4x4();

	template<typename T>
	static T clamp(const T& x, const T& a, const T& b);

	static float RandFloat(); // random float in [0, 1)
	static float RandFloat(float a, float b); // random float in [a, b)
	static int RandInt(int a, int b); // random int in [a, b]
};

#endif // MATH_HELPER_H