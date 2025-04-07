#include "CameraClass.h"

CameraClass::CameraClass()
{
	m_positionX = 0.0f;
	m_positionY = 0.0f;
	m_positionZ = 0.0f;

	m_rotationX = 0.0f;
	m_rotationY = 0.0f;
	m_rotationZ = 0.0f;
}

CameraClass::CameraClass(const CameraClass& other)
{
}

CameraClass::~CameraClass()
{
}

void CameraClass::SetPosition(float x, float y, float z)
{
	m_positionX = x;
	m_positionY = y;
	m_positionZ = z;
}

void CameraClass::SetRotation(float x, float y, float z)
{
	m_rotationX = x;
	m_rotationY = y;
	m_rotationZ = z;
}

XMFLOAT3 CameraClass::GetPosition() const
{
    return {m_positionX, m_positionY, m_positionZ};
}

XMFLOAT3 CameraClass::GetRotation() const
{
	return {m_rotationX, m_rotationY, m_rotationZ};
}

void CameraClass::Render()
{
	XMFLOAT3 up = { 0.f, 1.f, 0.f };
	
	XMVECTOR upVector = XMLoadFloat3(&up);

	XMFLOAT3 position = { m_positionX, m_positionY, m_positionZ };

	XMVECTOR positionVector = XMLoadFloat3(&position);

	XMFLOAT3 lookAt = { 0.f, 0.f, 1.f };

	XMVECTOR lookAtVector = XMLoadFloat3(&lookAt);

	float pitch = XMConvertToRadians(m_rotationX);
	float yaw = XMConvertToRadians(m_rotationY);
	float roll = XMConvertToRadians(m_rotationZ);

	XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	// transform by the rotation matrix so the view is correctly rotated at the origin
	lookAtVector = XMVector3TransformCoord(lookAtVector, rotationMatrix);
	upVector = XMVector3TransformCoord(upVector, rotationMatrix);

	// translate the rotated camera position to the location of the viewer
	lookAtVector = XMVectorAdd(positionVector, lookAtVector);

	m_viewMatrix = XMMatrixLookAtLH(positionVector, lookAtVector, upVector);
}

void CameraClass::GetViewMatrix(XMMATRIX& viewMatrix) const
{
	viewMatrix = m_viewMatrix;
}
