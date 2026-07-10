#include "RedHill.h"

#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <windowsx.h>
#include <wrl.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12/d3dx12.h>
#include <dxcapi.h>
#include <DirectXMath.h>

#include <algorithm>
#include <memory>

#include "Renderer.h"
#include "Config.h"
#include "Utils.h"
#include "Camera.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace RHCore
{
	HWND hWnd;

	Camera camera;
	POINT lastMousePosition;
	std::unique_ptr<Renderer> renderer;
}

// Main message handler for the sample.
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_KEYDOWN:
		RHCore::OnKeyDown(static_cast<UINT8>(wParam));
		return 0;
		// Bundle the right, middle and left buttons for now
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONDOWN:
		RHCore::OnMouseButtonDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), hWnd);
		return 0;
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_LBUTTONUP:
		RHCore::OnMouseButtonUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		RHCore::OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{

	RHCore::Init(hInstance, nShowCmd);

	if (RHCore::hWnd == nullptr)
	{
		return 0;
	}
	RHCore::UpdateLoop();
	RHCore::Terminate();

	return 0;
}

void RHCore::Init(HINSTANCE hInstance, int nShowCmd)
{
	// Register the windows class

	WNDCLASS windowClass = {};

	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = RHConfig::className;

	::RegisterClass(&windowClass);

	// Create the window

	RECT windowRect = { 0, 0, static_cast<LONG>(RHConfig::width), static_cast<LONG>(RHConfig::height) };
	::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	hWnd = ::CreateWindowEx(
		0,									// Optional window styles.
		RHConfig::className,				// Window class
		RHConfig::windowName,				// Window text
		WS_OVERLAPPEDWINDOW,				// Window style

		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,       // Parent window
		nullptr,       // Menu
		hInstance,  // Instance handle
		nullptr        // Additional application data
	);

	if (hWnd == nullptr)
	{
		return;
	}

	::ShowWindow(hWnd, nShowCmd);

	camera.radius = 20.0f;
	camera.phi = DirectX::XM_PIDIV4;
	camera.theta = 1.5f * DirectX::XM_PI;
	camera.aspectRatio = static_cast<float>(RHConfig::width) / static_cast<float>(RHConfig::height);

	// Init Renderer
	renderer = std::make_unique<Renderer>(hWnd);
	renderer->Init();
}

void RHCore::UpdateLoop()
{
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
		else
		{
			renderer->Update(camera);
			renderer->Render();
		}
	}
}

void RHCore::Terminate()
{

	// Renderer Shutdown
	renderer->Destroy();
}

void RHCore::OnMouseButtonDown(WPARAM btnState, int x, int y, HWND& hWnd)
{
	lastMousePosition.x = x;
	lastMousePosition.y = y;
	::SetCapture(hWnd);
}

void RHCore::OnMouseButtonUp(WPARAM btnState, int x, int y)
{
	::ReleaseCapture();
}

void RHCore::OnMouseMove(WPARAM btnState, int x, int y)
{
	// Left button controls orbital position
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - lastMousePosition.x));
		float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - lastMousePosition.y));

		// Update angles based on input to orbit camera around box.
		camera.theta -= dx;
		camera.phi -= dy;

		// Restrict the angle mPhi.
		camera.phi = std::clamp(camera.phi, 0.1f, DirectX::XM_PI - 0.1f);
	}
	//right button controls radius
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(x - lastMousePosition.x);
		float dy = 0.005f * static_cast<float>(y - lastMousePosition.y);

		// Update the camera radius based on input.
		camera.radius += dx - dy;

		// Restrict the radius.
		camera.radius = std::clamp(camera.radius, 2.0f, 50.0f);
	}
	lastMousePosition.x = x;
	lastMousePosition.y = y;
}

void RHCore::OnKeyDown(UINT8 key)
{
	if (key == VK_SPACE)
	{
		renderer->ChangeSceneMode();
	}
	else if (key == VK_CONTROL)
	{
		renderer->ChangeEnvironment();
	}
}