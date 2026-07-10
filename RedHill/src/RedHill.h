#pragma once

#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>

namespace RHCore
{
	void Init(HINSTANCE hInstance, int nShowCmd);
	void UpdateLoop();
	void Terminate();

	void OnMouseButtonDown(WPARAM btnState, int x, int y, HWND& hWnd);
	void OnMouseButtonUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

	void OnKeyDown(UINT8 key);
};