#include "D3D12App.h"
#include "Common.h"
#include "DXHelper.h"

/*

Initialize the pipeline:

	Enable the debug layer
	Create the device
	Create the command queue
	Create the swap chain
	Create a render target view descriptor heap
	Create frame resources (a render target view for each frame)
	Create a command allocator

Initialize the assets:

	Create an empty root signature
	Compile the shaders
	Create the vertex input layout
	Create a pipeline state object description, then the object
	Create the command list
	Close the command list
	Create and load the vertex buffers
	Create the vertex buffer views
	Create a fence
	Create an event handle
	Wait for the GPU to finish

Update:

	Modify the constant, vertex, index buffers and everything else as necessary

Render:

	Populate the command list
		Reset the command list allocator
		Reset the command list
		Set the graphics root signature
		Set the viewport and scissor rectangles
		Set a resource barrier, indicating the back buffer is to be used as a render target
		Record commands into the command list
		Indicate the back buffer will be used to present after the command list has executed
		Close the command list to further recording
	Execute the command list
	Present the frame
	Wait for the GPU to finish

Destroy:

	Wait for the GPU to finish
	Close the event handle

*/

using Microsoft::WRL::ComPtr;

D3D12App::D3D12App(UINT width, UINT height, std::wstring name)
	:DXSample(width, height, name),
	m_frameIndex(0),
	m_rtvDescriptorSize(0)
{
}

void D3D12App::OnInit()
{
	InitPipeline();
	InitAssets();
}

void D3D12App::OnUpdate()
{
	// Modify the constant, vertex, index buffers and everything else as necessary

	// (nothing yet)
}

void D3D12App::OnRender()
{
	// Populate the command list
	 
	PopulateCommandList();
	// Execute the command list
	
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	 
	// Present the frame
	 
	ThrowIfFailed(m_swapChain->Present(1, 0));
	 
	// Wait for the GPU to finish

	WaitForPreviousFrame();
}

void D3D12App::OnDestroy()
{
	// Wait for the GPU to finish
	WaitForPreviousFrame();

	// Close the event handle
	CloseHandle(m_fenceEvent);
}

void D3D12App::InitPipeline()
{
	UINT createFactoryFlags = 0;
	// Enable the debug layer
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	// Create the device
	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory)));

	if (m_useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
	}
	else
	{
		ComPtr<IDXGIAdapter1> adapter;
		GetHardwareAdapter(factory.Get(), &adapter);

		ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
	}
	
	// Create the command queue
		
	D3D12_COMMAND_QUEUE_DESC cq_desc = {};
	cq_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cq_desc.NodeMask = 0;
	cq_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cq_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&cq_desc, IID_PPV_ARGS(&m_commandQueue)));

	// Create the swap chain

	DXGI_SWAP_CHAIN_DESC1 sc_desc = {};
	sc_desc.BufferCount = FrameCount;
	sc_desc.Width = m_width;
	sc_desc.Height = m_height;
	sc_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sc_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sc_desc.SampleDesc.Count = 1;
	 
	ComPtr<IDXGISwapChain1> swapchain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), Win32Application::GetHwnd(), &sc_desc, nullptr, nullptr, &swapchain));

	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapchain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create a render target view descriptor heap

	D3D12_DESCRIPTOR_HEAP_DESC rtv_desc = {};
	rtv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtv_desc.NodeMask = 0;
	rtv_desc.NumDescriptors = FrameCount;
	rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&m_rtvHeap)));

	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		
	// Create frame resources (a render target view for each frame)
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT i = 0; i < FrameCount; ++i)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, m_rtvDescriptorSize);
	}

	// Create a command allocator
	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void D3D12App::InitAssets()
{
	// Create an empty root signature

	// Compile the shaders

	// Create the vertex input layout

	// Create a pipeline state object description, then the object

	// Create the command list
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	 
	// Close the command list: Command lists are created in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.

	m_commandList->Close();
	 
	// Create and load the vertex buffers

	// Create the vertex buffer views

	// Create a fence

	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValue = 1;

	// Create an event handle
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	// Wait for the GPU to finish
	WaitForPreviousFrame();
}

void D3D12App::PopulateCommandList()
{
	// Reset the command list allocator

	ThrowIfFailed(m_commandAllocator->Reset());

	// Reset the command list

	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Set the graphics root signature
	
	// Set the viewport and scissor rectangles

	// Set a resource barrier, indicating the back buffer is to be used as a render target
	
	CD3DX12_RESOURCE_BARRIER rt_barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->ResourceBarrier(1, &rt_barrier);

	// Record commands into the command list

	const float clearColor[] = { 1.0f, 0.2f, 0.6f, 1.0f };
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// Indicate the back buffer will be used to present after the command list has executed
	CD3DX12_RESOURCE_BARRIER pst_barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_commandList->ResourceBarrier(1, &pst_barrier);

	// Close the command list to further recording
	ThrowIfFailed(m_commandList->Close());
}

void D3D12App::WaitForPreviousFrame()
{
	// Waiting for frame is not a best practise. This is temporal before implementing frame buffering

	// Signal and increment fence value
	const UINT64  fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	++m_fenceValue;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	// Advance backbuffer
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
