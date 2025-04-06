#include "ApplicationClass.h"

ApplicationClass::ApplicationClass()
{
	m_d3d = nullptr;
}

ApplicationClass::ApplicationClass(const ApplicationClass& other)
{
}

ApplicationClass::~ApplicationClass()
{
}

bool ApplicationClass::Initialize(int screenWidth, int screenHeight, HWND hwnd)
{
	m_d3d = new D3DClass();

	if (!m_d3d->Initialize(screenWidth, screenHeight, VSYNC_ENABLED, hwnd, FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR))
	{
		::MessageBox(hwnd, L"Could not initialize Direct3D", L"Error", MB_OK);
		return false;
	}
	return true;
}

void ApplicationClass::Shutdown()
{
	if (m_d3d)
	{
		m_d3d->Shutdown();
		delete m_d3d;
		m_d3d = nullptr;
	}
}

bool ApplicationClass::Frame()
{
	return Render();
}

bool ApplicationClass::Render()
{
	m_d3d->BeginScene(1.f, 1.f, 0.5f, 1.f);
	m_d3d->EndScene();
	return true;
}


