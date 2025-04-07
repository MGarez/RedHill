#pragma once

#include "D3DClass.h"
#include "CameraClass.h"
#include "ModelClass.h"
#include "ColorShaderClass.h"

const bool FULL_SCREEN = false; // should be false until setted up to be windowed fullscreen
const bool VSYNC_ENABLED = true;
const float SCREEN_DEPTH = 1000.0f;
const float SCREEN_NEAR = 0.3f;

class ApplicationClass
{
public:

	ApplicationClass();
	ApplicationClass(const ApplicationClass& other);
	~ApplicationClass();

	bool Initialize(int screenWidth, int screenHeight, HWND hwnd);
	void Shutdown();
	bool Frame();

private:
	
	bool Render();

private:

	D3DClass* m_d3d;
	CameraClass* m_Camera;
	ModelClass* m_Model;
	ColorShaderClass* m_ColorShader;
};

