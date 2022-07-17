#pragma once

#include "utils.h"

class Camera
{
	// camera coordinate system with coordinates relative to world space
	XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
	XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
	XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

	// cache frustum properties
	float mNearZ = 0.0f;
	float mFarZ = 0.0f;
	float mAspectRatio = 0.0f;
	float mFovY = 0.0f;
	float mNearWindowHeight = 0.0f;
	float mFarWindowHeight = 0.0f;

	bool mViewDirty = true;

	// cache view/proj matrices
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

public:
	Camera();
	~Camera();

	XMVECTOR GetPositionV() const;
	XMFLOAT3 GetPositionF() const;
	void SetPosition(const float x, const float y, const float z);
	void SetPosition(const XMFLOAT3& v);

	XMVECTOR GetRightV() const;
	XMFLOAT3 GetRightF() const;
	XMVECTOR GetUpV() const;
	XMFLOAT3 GetUpF() const;
	XMVECTOR GetLookV() const;
	XMFLOAT3 GetLookF() const;

	float GetNearZ() const;
	float GetFarZ() const;
	float GetAspectRatio() const;
	float GetFovY() const;
	float GetFovX() const;

	float GetNearWindowWidth() const;
	float GetNearWindowHeight() const;
	float GetFarWindowWidth() const;
	float GetFarWindowHeight() const;

	// set frustum
	void SetLens(const float FovY, const float AspectRatio, const float zn, const float zf);

	// define camera space
	void LookAt(const FXMVECTOR& position, const FXMVECTOR& target, const FXMVECTOR& up);
	void LookAt(const XMFLOAT3& position, const XMFLOAT3& target, const XMFLOAT3& up);

	XMMATRIX GetView() const;
	XMMATRIX GetProj() const;

	XMFLOAT4X4 GetViewF() const;
	XMFLOAT4X4 GetProjF() const;

	// move the camera
	void strafe(const float distance);
	void walk(const float distance);

	// rotate the camera
	void pitch(const float angle);
	void RotateY(const float angle);

	// after modifying camera position/orientation, call to rebuild the view matrix
	void UpdateViewMatrix();
};