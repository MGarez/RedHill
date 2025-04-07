#include "ApplicationClass.h"

ApplicationClass::ApplicationClass()
{
	m_d3d = nullptr;
	m_Camera = nullptr;
	m_Model = nullptr;
	m_ColorShader = nullptr;
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

	m_Camera = new CameraClass;
	m_Camera->SetPosition(0.f, 0.f, -2.f);

	m_Model = new ModelClass;
	if (!m_Model->Initialize(m_d3d->GetDevice()))
	{
		::MessageBox(hwnd, L"Could not initialize the model object", L"Error", MB_OK);
		return false;
	}

	m_ColorShader = new ColorShaderClass;

	if (!m_ColorShader->Initialize(m_d3d->GetDevice(), hwnd))
	{
		::MessageBox(hwnd, L"Could not initialize the color shader object", L"Error", MB_OK);
		return false;
	}

	return true;
}

void ApplicationClass::Shutdown()
{
	if (m_ColorShader)
	{
		m_ColorShader->Shutdown();
		delete m_ColorShader;
		m_ColorShader = nullptr;
	}

	if (m_Model)
	{
		m_Model->Shutdown();
		delete m_Model;
		m_Model = nullptr;
	}

	if (m_Camera)
	{
		delete m_Camera;
		m_Camera = nullptr;
	}
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
	m_d3d->BeginScene(0.f, 0.f, 0.f, 1.f);

	m_Camera->Render();

	XMMATRIX worldMatrix;
	XMMATRIX viewMatrix;
	XMMATRIX projectionMatrix;

	m_d3d->GetWorldMatrix(worldMatrix);
	m_Camera->GetViewMatrix(viewMatrix);
	m_d3d->GetProjectionMatrix(projectionMatrix);

	m_Model->Render(m_d3d->GetDeviceContext());

	if (!m_ColorShader->Render(m_d3d->GetDeviceContext(), m_Model->GetIndexCount(), worldMatrix, viewMatrix, projectionMatrix))
	{
		return false;
	}

	m_d3d->EndScene();
	return true;
}


