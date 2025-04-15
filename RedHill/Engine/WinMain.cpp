#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "D3D12App.h"

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nShowCmd)
{
	const wchar_t* class_name = L"RH window class";

	WNDCLASS window_class = {};

	window_class.hInstance = hInstance;
	window_class.lpszClassName = class_name;
	window_class.lpfnWndProc = WndProc;

	::RegisterClass(&window_class);

	HWND hWnd = ::CreateWindowEx(
		0,
		class_name,
		L"Red Hill",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL 
	);

	if (!hWnd)
	{
		return 0;
	}

	::ShowWindow(hWnd, nShowCmd);

	MSG msg = {};

	while (msg.message != WM_QUIT)
	{
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}

	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}


