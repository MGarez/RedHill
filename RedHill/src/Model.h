#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12/d3dx12.h>
#include <dxcapi.h>
#include <DirectXMath.h>

#include <wrl.h>
#include "DescriptorHeapAllocator.h"

#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

struct Vertex
{
	float position[3] = { 0.0f, 0.0f, 0.0f };
	float uv[2] = { 0.0f, 0.0f };
	float normal[3] = { 0.0f, 0.0f, 0.0f };
	float tangent[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct PBRMesh
{

public:
	ComPtr<ID3D12Resource> vBuffer;
	ComPtr<ID3D12Resource> vBufferUploader;

	ComPtr<ID3D12Resource> iBuffer;
	ComPtr<ID3D12Resource> iBufferUploader;

	ComPtr<ID3D12Resource> albedoTexture;
	ComPtr<ID3D12Resource> albedoTextureUploader;
	ComPtr<ID3D12Resource> normalTexture;
	ComPtr<ID3D12Resource> normalTextureUploader;
	ComPtr<ID3D12Resource> metalRoughnessTexture;
	ComPtr<ID3D12Resource> metalRoughnessTextureUploader;
	ComPtr<ID3D12Resource> aoTexture;
	ComPtr<ID3D12Resource> aoTextureUploader;

	// Let's store the descriptor handles for the textures in the mesh itself, so we can easily bind them when rendering
	// Note: In a more complex engine, you might want to manage these handles in a more centralized way with a material system, but for this example, we'll keep it simple.
	// These handles are assumed to be contiguous in the descriptor heap but we will store them all
	DescriptorHandle albedoTextureSrvHandle;
	DescriptorHandle normalTextureSrvHandle;
	DescriptorHandle metalRoughnessTextureSrvHandle;
	DescriptorHandle aoTextureSrvHandle;

	UINT vBufferSize = 0;
	UINT vBufferStride = 0;

	UINT iBufferSize = 0;
	DXGI_FORMAT iBufferFormat = DXGI_FORMAT_R16_UINT;

	std::vector<Vertex> vertices_data = {};
	std::vector<uint32_t> indices_data = {};

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv = {};
		vbv.BufferLocation = vBuffer->GetGPUVirtualAddress();
		vbv.SizeInBytes = vBufferSize;
		vbv.StrideInBytes = vBufferStride;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const
	{
		D3D12_INDEX_BUFFER_VIEW ibv = {};
		ibv.BufferLocation = iBuffer->GetGPUVirtualAddress();
		ibv.SizeInBytes = iBufferSize;
		ibv.Format = iBufferFormat;

		return ibv;
	}

	void GenerateVertexAndIndexFromObj(const std::string& objFile);

	void GenerateSphere(uint32_t subdivisions);

	void GenerateFloor(float size);

};
