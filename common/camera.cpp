#include "camera.h"

Camera::Camera()
{
	SetLens(0.25f * XM_PI, 1.0f, 1.0f, 1000.0f);
}

Camera::~Camera()
{}

XMVECTOR Camera::GetPositionV() const
{
	return XMLoadFloat3(&mPosition);
}

XMFLOAT3 Camera::GetPositionF() const
{
	return mPosition;
}

void Camera::SetPosition(const float x, const float y, const float z)
{
	mPosition = XMFLOAT3(x, y, z);
	mViewDirty = true;
}
void Camera::SetPosition(const XMFLOAT3& position)
{
	mPosition = position;
	mViewDirty = true;
}

XMVECTOR Camera::GetRightV() const
{
	return XMLoadFloat3(&mRight);
}

XMFLOAT3 Camera::GetRightF() const
{
	return mRight;
}

XMVECTOR Camera::GetUpV() const
{
	return XMLoadFloat3(&mUp);
}

XMFLOAT3 Camera::GetUpF() const
{
	return mUp;
}

XMVECTOR Camera::GetLookV() const
{
	return XMLoadFloat3(&mLook);
}

XMFLOAT3 Camera::GetLookF() const
{
	return mLook;
}

float Camera::GetNearZ() const
{
	return mNearZ;
}

float Camera::GetFarZ() const
{
	return mFarZ;
}

float Camera::GetAspectRatio() const
{
	return mAspectRatio;
}

float Camera::GetFovY() const
{
	return mFovY;
}

float Camera::GetFovX() const
{
	float HalfWidth = 0.5f * GetNearWindowWidth();
	return 2.0f * atan(HalfWidth / mNearZ);
}

float Camera::GetNearWindowWidth() const
{
	return mAspectRatio * mNearWindowHeight;
}

float Camera::GetNearWindowHeight() const
{
	return mNearWindowHeight;
}

float Camera::GetFarWindowWidth() const
{
	return mAspectRatio * mFarWindowHeight;
}

float Camera::GetFarWindowHeight() const
{
	return mFarWindowHeight;
}

void Camera::SetLens(const float FovY, const float AspectRatio, const float zn, const float zf)
{
	// cache frustum properties
	mFovY = FovY;
	mAspectRatio = AspectRatio;
	mNearZ = zn;
	mFarZ = zf;

	mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
	mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

	XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspectRatio, mNearZ, mFarZ);
	XMStoreFloat4x4(&mProj, P);
}

void Camera::LookAt(const FXMVECTOR& position, const FXMVECTOR& target, const FXMVECTOR& up)
{
	const XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, position));
	const XMVECTOR R = XMVector3Normalize(XMVector3Cross(up, L));
	const XMVECTOR U = XMVector3Cross(L, R);

	XMStoreFloat3(&mPosition, position);
	XMStoreFloat3(&mLook, L);
	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);

	mViewDirty = true;
}

void Camera::LookAt(const XMFLOAT3& position, const XMFLOAT3& target, const XMFLOAT3& up)
{
	const XMVECTOR P = XMLoadFloat3(&position);
	const XMVECTOR T = XMLoadFloat3(&target);
	const XMVECTOR U = XMLoadFloat3(&up);

	LookAt(P, T, U);

	mViewDirty = true;
}

XMMATRIX Camera::GetView() const
{
	assert(!mViewDirty);
	return XMLoadFloat4x4(&mView);
}

XMMATRIX Camera::GetProj() const
{
	return XMLoadFloat4x4(&mProj);
}

XMFLOAT4X4 Camera::GetViewF() const
{
	assert(!mViewDirty);
	return mView;
}

XMFLOAT4X4 Camera::GetProjF() const
{
	return mProj;
}

void Camera::strafe(const float distance)
{
	const XMVECTOR d = XMVectorReplicate(distance);
	const XMVECTOR r = XMLoadFloat3(&mRight);
	const XMVECTOR p = XMLoadFloat3(&mPosition);
	// mPosition += distance * mRight
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(d, r, p));

	mViewDirty = true;
}

void Camera::walk(const float distance)
{
	const XMVECTOR d = XMVectorReplicate(distance);
	const XMVECTOR l = XMLoadFloat3(&mLook);
	const XMVECTOR p = XMLoadFloat3(&mPosition);
	// mPosition += distance * mLook
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(d, l, p));

	mViewDirty = true;
}

void Camera::pitch(const float angle)
{
	const XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);
	// rotate up and look vectors about the right vector
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void Camera::RotateY(const float angle)
{
	const XMMATRIX R = XMMatrixRotationY(angle);
	// rotate the basis vectors about the world y-axis
	XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void Camera::UpdateViewMatrix()
{
	if (mViewDirty)
	{
		XMVECTOR R = XMLoadFloat3(&mRight);
		XMVECTOR U = XMLoadFloat3(&mUp);
		XMVECTOR L = XMLoadFloat3(&mLook);
		const XMVECTOR P = XMLoadFloat3(&mPosition);

		// keep camera's axes orthogonal to each other and of unit length
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		// U, L already ortho-normal, so no need to normalize cross product
		R = XMVector3Cross(U, L);

		// fill in the view matrix entries
		const float x = -XMVectorGetX(XMVector3Dot(P, R));
		const float y = -XMVectorGetX(XMVector3Dot(P, U));
		const float z = -XMVectorGetX(XMVector3Dot(P, L));

		XMStoreFloat3(&mRight, R);
		XMStoreFloat3(&mUp, U);
		XMStoreFloat3(&mLook, L);

		mView(0, 0) = mRight.x;
		mView(1, 0) = mRight.y;
		mView(2, 0) = mRight.z;
		mView(3, 0) = x;

		mView(0, 1) = mUp.x;
		mView(1, 1) = mUp.y;
		mView(2, 1) = mUp.z;
		mView(3, 1) = y;

		mView(0, 2) = mLook.x;
		mView(1, 2) = mLook.y;
		mView(2, 2) = mLook.z;
		mView(3, 2) = z;

		mView(0, 3) = 0.0f;
		mView(1, 3) = 0.0f;
		mView(2, 3) = 0.0f;
		mView(3, 3) = 1.0f;

		mViewDirty = false;
	}
}