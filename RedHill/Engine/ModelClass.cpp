#include "ModelClass.h"

ModelClass::ModelClass()
{
    m_vertexBuffer = nullptr;
    m_vertexCount = 0;
    m_indexBuffer = nullptr;
    m_indexCount = 0;
    m_texture = nullptr;
}

ModelClass::ModelClass(const ModelClass& other)
{
}

ModelClass::~ModelClass()
{
}

bool ModelClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, char* filename)
{
    return InitializeBuffers(device) && LoadTexture(device, deviceContext, filename);
}

void ModelClass::Shutdown()
{
    ReleaseTexture();
    ShutdownBuffers();
}

void ModelClass::Render(ID3D11DeviceContext* deviceContext)
{
    RenderBuffers(deviceContext);
}

int ModelClass::GetIndexCount() const
{
    return m_indexCount;
}

ID3D11ShaderResourceView* ModelClass::GetTexture()
{
    return m_texture->GetTexture();
}

bool ModelClass::InitializeBuffers(ID3D11Device* device)
{
    m_vertexCount = 3;
    m_indexCount = 3;

    VertexType* vertices = new VertexType[m_vertexCount];

    if (!vertices)
    {
        return false;
    }

    unsigned long* indices = new unsigned long[m_indexCount];

    if (!indices)
    {
        return false;
    }

	vertices[0].position = XMFLOAT3(-1.0f, -1.0f, 0.0f);  // Bottom left.
    vertices[0].texture = XMFLOAT2(0.f, 1.f);

	vertices[1].position = XMFLOAT3(0.0f, 1.0f, 0.0f);  // Top middle.
	vertices[1].texture = XMFLOAT2(0.5f, 0.f);

	vertices[2].position = XMFLOAT3(1.0f, -1.0f, 0.0f);  // Bottom right.
    vertices[2].texture = XMFLOAT2(1.f, 1.f);

    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;

    D3D11_BUFFER_DESC  vertexBufferDesc;
    D3D11_SUBRESOURCE_DATA vertexData;

    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(VertexType) * m_vertexCount;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;
    vertexBufferDesc.MiscFlags = 0;
    vertexBufferDesc.StructureByteStride = 0;

    vertexData.pSysMem = vertices;
    vertexData.SysMemPitch = 0;
    vertexData.SysMemSlicePitch = 0;

    if (FAILED(device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer)))
    {
        return false;
    }

	D3D11_BUFFER_DESC  indexBufferDesc;
	D3D11_SUBRESOURCE_DATA indexData;

	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	if (FAILED(device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer)))
	{
		return false;
	}

    delete[] vertices;
    vertices = nullptr;

    delete[] indices;
    indices = nullptr;
  
    return true;
}

void ModelClass::ShutdownBuffers()
{
    if (m_indexBuffer)
    {
        m_indexBuffer->Release();
        m_indexBuffer = nullptr;
        m_indexCount = 0;
    }
    if (m_vertexBuffer)
    {
        m_vertexBuffer->Release();
        m_vertexBuffer = nullptr;
        m_vertexCount = 0;
    }
}

void ModelClass::RenderBuffers(ID3D11DeviceContext* deviceContext)
{
    unsigned int stride = sizeof(VertexType);
    unsigned int offset = 0;

    deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    deviceContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

bool ModelClass::LoadTexture(ID3D11Device* device, ID3D11DeviceContext* deviceContext, char* filename)
{
    m_texture = new TextureClass();
    return m_texture->Initialize(device, deviceContext, filename);
}

void ModelClass::ReleaseTexture()
{
	if (m_texture)
	{
		m_texture->Shutdown();
		delete m_texture;
		m_texture = nullptr;
	}
}
