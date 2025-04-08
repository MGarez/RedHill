#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include "TextureClass.h"
using namespace DirectX;
class ModelClass
{
private:

	struct VertexType
	{
		XMFLOAT3 position;
		XMFLOAT2 texture;
	};

public:

	ModelClass();
	ModelClass(const ModelClass& other);
	~ModelClass();

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, char* filename);
	void Shutdown();
	void Render(ID3D11DeviceContext* deviceContext);

	int GetIndexCount() const;
	ID3D11ShaderResourceView* GetTexture();

private:

	bool InitializeBuffers(ID3D11Device* device);
	void ShutdownBuffers();
	void RenderBuffers(ID3D11DeviceContext* deviceContext);
	bool LoadTexture(ID3D11Device* device, ID3D11DeviceContext* deviceContext, char* filename);
	void ReleaseTexture();


private:

	ID3D11Buffer* m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;
	int m_vertexCount;
	int m_indexCount;
	TextureClass* m_texture;
};

