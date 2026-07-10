#pragma once

#include <DirectXMath.h>

struct Camera
{
	float radius;
	float theta;
	float phi;
	float aspectRatio;

	DirectX::XMVECTOR GetPosition() const;
	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix() const;
};
