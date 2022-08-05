#pragma once

#include "utils.h"

struct KeyFrame
{
	KeyFrame();
	~KeyFrame();

	float time;
	XMFLOAT3 translation;
	XMFLOAT3 scale;
	XMFLOAT4 rotation;
};

struct BoneAnimation
{
	float GetStartTime() const;
	float GetEndTime() const;

	void interpolate(const float time, XMFLOAT4X4& world) const;

	// key frames sorted by time
	std::vector<KeyFrame> KeyFrames;
};
