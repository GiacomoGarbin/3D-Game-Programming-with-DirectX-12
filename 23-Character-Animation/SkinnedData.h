#pragma once

#include "utils.h"
#include "AnimationHelper.h"

struct AnimationClip
{
	float GetClipStartTime() const;
	float GetClipEndTime() const;

	void interpolate(const float t, std::vector<XMFLOAT4X4>& transforms) const;

	std::vector<BoneAnimation> BoneAnimations;
};

class SkinnedData
{
	// gives parent index of i-th bone
	std::vector<int> mBoneHierarchy;
	std::vector<XMFLOAT4X4> mBoneOffsets;
	std::unordered_map<std::string, AnimationClip> mAnimations;

public:
	UINT GetBoneCount() const;

	float GetClipStartTime(const std::string& name) const;
	float GetClipEndTime(const std::string& name) const;

	void set(const std::vector<int>& hierarchy,
			 const std::vector<XMFLOAT4X4>& offsets,
			 const std::unordered_map<std::string, AnimationClip>& animations);

	// TODO: cache the result to improve performance
	void GetFinalTransforms(const std::string& name,
							const float time,
							std::vector<XMFLOAT4X4>& transforms) const;
};
