#pragma once

#include <DirectXMath.h>
using namespace DirectX;

class CameraClass
{
public:

	CameraClass();
	CameraClass(const CameraClass& other);
	~CameraClass();

	void SetPosition(float x, float y, float z);
	void SetRotation(float x, float y, float z);

	XMFLOAT3 GetPosition() const;
	XMFLOAT3 GetRotation() const;

	void Render();
	void GetViewMatrix(XMMATRIX& viewMatrix) const;

private:

	float m_positionX;
	float m_positionY;
	float m_positionZ;

	float m_rotationX;
	float m_rotationY;
	float m_rotationZ;

	XMMATRIX m_viewMatrix;

};

