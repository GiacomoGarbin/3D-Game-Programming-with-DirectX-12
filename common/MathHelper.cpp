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

float MathHelper::RandFloat()
{
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

float MathHelper::RandFloat(float a, float b)
{
    return a + RandFloat() * (b - a);
}

int MathHelper::RandInt(int a, int b)
{
    return a + rand() % ((b - a) + 1);
}


XMVECTOR MathHelper::SphericalToCartesian(float radius, float theta, float phi)
{
	return XMVectorSet(radius * std::sin(phi) * std::cos(theta),
                       radius * std::cos(phi),
                       radius * std::sin(phi) * std::sin(theta),
                       1.0f);
}