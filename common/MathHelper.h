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
};

#endif // MATH_HELPER_H