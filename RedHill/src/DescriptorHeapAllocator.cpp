#include "DescriptorHeapAllocator.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include "Utils.h"

#include <cassert>

void DescriptorHeapAllocator::Init(ID3D12Device* device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t persistentSize, const uint32_t transientSize)
{
	// Shader visibility is deduced from the type
	m_isShaderVisible = (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	m_type = type;

	m_persistentLimit = persistentSize;
	m_tempBase = m_persistentLimit;
	m_tempTop = m_tempBase;
	m_totalLimit = persistentSize + transientSize;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Flags = m_isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NumDescriptors = persistentSize + transientSize;
	desc.NodeMask = 0;
	desc.Type = m_type;
	CrashIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));

	m_descriptorSize = device->GetDescriptorHandleIncrementSize(m_type);
	m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();

	if (m_isShaderVisible)
	{
		m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
	}
}

DescriptorHandle DescriptorHeapAllocator::AllocatePersistent()
{
	if (m_persistentTop >= m_persistentLimit)
	{
		::OutputDebugStringA("Insufficient persistent descriptors available\n");
		::__debugbreak();
		return {};
	}

	DescriptorHandle handle;
	handle.cpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cpuStart, m_persistentTop, m_descriptorSize);

	if (m_isShaderVisible)
	{
		handle.gpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_gpuStart, m_persistentTop, m_descriptorSize);
	}
	++m_persistentTop;

	return handle;
}

DescriptorHandle DescriptorHeapAllocator::AllocateTransient()
{
	if (m_tempTop >= m_totalLimit)
	{
		::OutputDebugStringA("Insufficient transient descriptors available\n");
		::__debugbreak();
		return {};
	}

	DescriptorHandle handle;
	handle.cpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cpuStart, m_tempTop, m_descriptorSize);

	if (m_isShaderVisible)
	{
		handle.gpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_gpuStart, m_tempTop, m_descriptorSize);
	}
	++m_tempTop;

	return handle;
}

void DescriptorHeapAllocator::ResetTransient()
{
	m_tempTop = m_tempBase;
}
