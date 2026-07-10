#include "Camera.h"

using namespace DirectX;

XMVECTOR Camera::GetPosition() const
{
	float x = radius * sinf(phi) * cosf(theta);
	float z = radius * sinf(phi) * sinf(theta);
	float y = radius * cosf(phi);

	// Build view matrix

	return XMVectorSet(x, y, z, 1.0f);
}

XMMATRIX Camera::GetViewMatrix() const
{
	XMVECTOR target = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // Our camera is orbital around the origin for now
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	return XMMatrixLookAtLH(GetPosition(), target, up);
}

XMMATRIX Camera::GetProjectionMatrix() const
{
	return XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 1000.0f, 0.1f); // Near and far planes are swapped for reverse-Z
}
