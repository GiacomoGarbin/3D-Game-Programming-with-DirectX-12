#include "MathHelper.h"

XMFLOAT4X4 MathHelper::Identity4x4() 
{
    static XMFLOAT4X4 I(1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f);
    return I;
}

template<typename T>
T MathHelper::clamp(const T& x, const T& a, const T& b)
{
	return x < a ? a : (x > b ? b : x);
}