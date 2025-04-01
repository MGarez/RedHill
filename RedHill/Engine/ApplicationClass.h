#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


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
};

