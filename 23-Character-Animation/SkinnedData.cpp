#include "SkinnedData.h"

float AnimationClip::GetClipStartTime() const
{
	// find smallest start time over all bones in this clip

	float t = MathHelper::infinity;
	for (UINT i = 0; i < BoneAnimations.size(); ++i)
	{
		t = min(t, BoneAnimations[i].GetStartTime());
	}

	return t;
}

float AnimationClip::GetClipEndTime()const
{
	// find largest end time over all bones in this clip

	float t = 0.0f;
	for (UINT i = 0; i < BoneAnimations.size(); ++i)
	{
		t = max(t, BoneAnimations[i].GetEndTime());
	}

	return t;
}

void AnimationClip::interpolate(const float t, std::vector<XMFLOAT4X4>& transforms) const
{
	for (UINT i = 0; i < BoneAnimations.size(); ++i)
	{
		BoneAnimations[i].interpolate(t, transforms[i]);
	}
}

UINT SkinnedData::GetBoneCount() const
{
	return mBoneHierarchy.size();
}

float SkinnedData::GetClipStartTime(const std::string& name) const
{
	auto clip = mAnimations.find(name);
	return clip->second.GetClipStartTime();
}

float SkinnedData::GetClipEndTime(const std::string& name) const
{
	auto clip = mAnimations.find(name);
	return clip->second.GetClipEndTime();
}

void SkinnedData::set(const std::vector<int>& hierarchy,
					  const std::vector<XMFLOAT4X4>& offsets,
					  const std::unordered_map<std::string, AnimationClip>& animations)
{
	mBoneHierarchy = hierarchy;
	mBoneOffsets = offsets;
	mAnimations = animations;
}

void SkinnedData::GetFinalTransforms(const std::string& name,
									 const float time,
									 std::vector<XMFLOAT4X4>& transforms) const
{
	const UINT bones = mBoneOffsets.size();

	std::vector<XMFLOAT4X4> ToParentTransforms(bones);

	// interpolate all the bones of this clip at the given time position
	const auto& clip = mAnimations.find(name);
	clip->second.interpolate(time, ToParentTransforms);

	// traverse the hierarchy and transform all the bones to the root space

	std::vector<XMFLOAT4X4> ToRootTransforms(bones);

	// the root bone has index 0, and it has no parent,
	// so its toRootTransform is just its local bone transform
	ToRootTransforms[0] = ToParentTransforms[0];

	// find the toRootTransform of the children
	for (UINT i = 1; i < bones; ++i)
	{
		const XMMATRIX ToParent = XMLoadFloat4x4(&ToParentTransforms[i]);

		const int ParentIndex = mBoneHierarchy[i];
		const XMMATRIX ParentToRoot = XMLoadFloat4x4(&ToRootTransforms[ParentIndex]);

		const XMMATRIX ToRoot = XMMatrixMultiply(ToParent, ParentToRoot);
		XMStoreFloat4x4(&ToRootTransforms[i], ToRoot);
	}

	// premultiply by the bone offset transform to get the final transform
	for (UINT i = 0; i < bones; ++i)
	{
		const XMMATRIX offset = XMLoadFloat4x4(&mBoneOffsets[i]);
		const XMMATRIX ToRoot = XMLoadFloat4x4(&ToRootTransforms[i]);
		const XMMATRIX transform = XMMatrixMultiply(offset, ToRoot);
		XMStoreFloat4x4(&transforms[i], XMMatrixTranspose(transform));
	}
}