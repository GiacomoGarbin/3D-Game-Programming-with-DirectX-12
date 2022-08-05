#include "AnimationHelper.h"

KeyFrame::KeyFrame() :
	time(0.0f),
	translation(0.0f, 0.0f, 0.0f),
	scale(1.0f, 1.0f, 1.0f),
	rotation(0.0f, 0.0f, 0.0f, 1.0f)
{}

KeyFrame::~KeyFrame()
{}


float BoneAnimation::GetStartTime()const
{
	return KeyFrames.front().time;
}

float BoneAnimation::GetEndTime()const
{
	return KeyFrames.back().time;
}

void BoneAnimation::interpolate(const float time, XMFLOAT4X4& world) const
{
	const XMVECTOR O = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	if (time <= KeyFrames.front().time)
	{
		const XMVECTOR S = XMLoadFloat3(&KeyFrames.front().scale);
		const XMVECTOR T = XMLoadFloat3(&KeyFrames.front().translation);
		const XMVECTOR R = XMLoadFloat4(&KeyFrames.front().rotation);

		XMStoreFloat4x4(&world, XMMatrixAffineTransformation(S, O, R, T));
	}
	else if (time >= KeyFrames.back().time)
	{
		const XMVECTOR S = XMLoadFloat3(&KeyFrames.back().scale);
		const XMVECTOR T = XMLoadFloat3(&KeyFrames.back().translation);
		const XMVECTOR R = XMLoadFloat4(&KeyFrames.back().rotation);

		XMStoreFloat4x4(&world, XMMatrixAffineTransformation(S, O, R, T));
	}
	else
	{
		for (UINT i = 0; i < KeyFrames.size() - 1; ++i)
		{
			if (time >= KeyFrames[i].time && time <= KeyFrames[i + 1].time)
			{
				const float t = (time - KeyFrames[i].time) / (KeyFrames[i + 1].time - KeyFrames[i].time);

				const XMVECTOR S0 = XMLoadFloat3(&KeyFrames[i].scale);
				const XMVECTOR S1 = XMLoadFloat3(&KeyFrames[i + 1].scale);

				const XMVECTOR T0 = XMLoadFloat3(&KeyFrames[i].translation);
				const XMVECTOR T1 = XMLoadFloat3(&KeyFrames[i + 1].translation);

				const XMVECTOR R0 = XMLoadFloat4(&KeyFrames[i].rotation);
				const XMVECTOR R1 = XMLoadFloat4(&KeyFrames[i + 1].rotation);

				const XMVECTOR S = XMVectorLerp(S0, S1, t);
				const XMVECTOR T = XMVectorLerp(T0, T1, t);
				const XMVECTOR R = XMQuaternionSlerp(R0, R1, t);

				XMStoreFloat4x4(&world, XMMatrixAffineTransformation(S, O, R, T));
				break;
			}
		}
	}
}
