#pragma once

#include <d3d12.h>
#include <d3dx12/d3dx12.h>
#include <wrl.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

struct DescriptorHandle
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpu = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpu = {};
};

class DescriptorHeapAllocator
{
public:
	DescriptorHeapAllocator() = default;
	DescriptorHeapAllocator(const DescriptorHeapAllocator&) = delete;
	DescriptorHeapAllocator& operator=(const DescriptorHeapAllocator&) = delete;

	void Init(ID3D12Device* device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t persistentSize, const uint32_t transientSize);

	DescriptorHandle AllocatePersistent();

	DescriptorHandle AllocateTransient();

	void ResetTransient();

	ID3D12DescriptorHeap* Heap() const { return m_heap.Get(); }

private:

	ComPtr<ID3D12DescriptorHeap> m_heap;
	bool m_isShaderVisible = false;

	D3D12_DESCRIPTOR_HEAP_TYPE  m_type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE m_cpuStart = {};
	CD3DX12_GPU_DESCRIPTOR_HANDLE m_gpuStart = {};

	uint32_t m_persistentTop = 0;
	uint32_t m_persistentLimit = 0;
	uint32_t m_tempBase = 0;      // == m_persistentLimit
	uint32_t m_tempTop = 0;
	uint32_t m_totalLimit= 0;

	UINT m_descriptorSize = 0;

};