#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <exception>
using namespace DirectX;

class D3DClass
{
public:

	D3DClass();
	D3DClass(const D3DClass& other);
	~D3DClass();

	bool Initialize(int screenWidth, int screenHeight, bool vsync, HWND hwnd, bool fullscreen, float screenDepth, float screenNear);
	void Shutdown();

	void BeginScene(float red, float green, float blue, float alpha);
	void EndScene();
	ID3D11Device* GetDevice() const;
	ID3D11DeviceContext* GetDeviceContext() const;

	void GetProjectionMatrix(XMMATRIX& projectionMatrix) const;
	void GetWorldMatrix(XMMATRIX& worldMatrix) const;
	void GetOrthoMatrix(XMMATRIX& orthoMatrix) const;

	void GetVideoCardInfo(char* cardName, int& memory) const;

	void SetBackBufferRenderTarget();
	void ResetViewport();

private:

	bool m_vsync_enabled;
	int m_videoCardMemory;
	char m_videoCardDescription[128];
	IDXGISwapChain* m_swapChain;
	ID3D11Device* m_device;
	ID3D11DeviceContext* m_deviceContext;
	ID3D11RenderTargetView* m_renderTargetView;
	ID3D11Texture2D* m_depthStencilBuffer;
	ID3D11DepthStencilState* m_depthStencilState;
	ID3D11DepthStencilView* m_depthStencilView;
	ID3D11RasterizerState* m_rasterState;
	XMMATRIX m_projectionMatrix;
	XMMATRIX m_worldMatrix;
	XMMATRIX m_orthoMatrix;

	D3D11_VIEWPORT m_viewport;

};

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::exception();
	}
}