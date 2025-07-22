#include "D3D12App.h"
#include "Common.h"
#include "DXHelper.h"

#include <DirectXMath.h>

#include <array>
#include <algorithm>

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
	m_viewport(0.0f,0.0f, static_cast<FLOAT>(width), static_cast<FLOAT>(height)),
	m_scissorRect(0,0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_rtvDescriptorSize(0), 
	m_fenceValues{}
{
	m_camera.radius = 20.f;
	m_camera.phi = 0.0f;
	m_camera.theta = 0.0f;
}

void D3D12App::OnInit()
{
	InitPipeline();
	InitAssets();
}

void D3D12App::OnUpdate()
{
	// Modify the constant buffer to update geometry position
	// #TODO: Move this to other place and cache it only updating it if necessary

	// Build the model matrix (the identity in this case)
	XMMATRIX world = XMLoadFloat4x4(&RHMath::Identity4x4());

	// Conver camera position from spherical to cartesian;
	float x = m_camera.radius * sinf(m_camera.phi) * cosf(m_camera.theta);
	float y = m_camera.radius * sinf(m_camera.phi) * sinf(m_camera.theta);
	float z = m_camera.radius * cosf(m_camera.phi);

	// Build the view matrix

	XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // Our object is currently in the (0,0,0) position
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
	XMMATRIX view = XMMatrixLookAtLH(position, target, up);

	// Build the projection matrix
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, m_aspectRatio, 0.1f, 1000.0f);

	XMMATRIX mvpMatrix = projection * view * world;

	ObjectCB auxCB = {};

	// Shaders compiled with default row-major matrices
	XMStoreFloat4x4(&auxCB.mvp, XMMatrixTranspose(mvpMatrix));

	memcpy(m_mappedConstantData, &auxCB, sizeof(ObjectCB));
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

	MoveToNextFrame();
}

void D3D12App::OnDestroy()
{
	// Wait for the GPU to finish
	WaitForGpu();

	// Close the event handle
	CloseHandle(m_fenceEvent);
}

void D3D12App::OnMouseButtonDown(WPARAM btnState, int x, int y)
{
	m_lastMousePosition.x = x;
	m_lastMousePosition.y = y;
	::SetCapture(Win32Application::GetHwnd());
}

void D3D12App::OnMouseButtonUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void D3D12App::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePosition.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePosition.y));

		// Update angles based on input to orbit camera around box.
		m_camera.theta += dx;
		m_camera.phi += dy;

		// Restrict the angle mPhi.
		m_camera.phi = std::clamp(m_camera.phi, 0.1f, XM_PI - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(x - m_lastMousePosition.x);
		float dy = 0.005f * static_cast<float>(y - m_lastMousePosition.y);

		// Update the camera radius based on input.
		m_camera.radius += dx - dy;

		// Restrict the radius.
		m_camera.radius = std::clamp(m_camera.radius, 3.0f, 15.0f);
	}

	m_lastMousePosition.x = x;
	m_lastMousePosition.y = y;
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

	// Create descriptor heaps

	D3D12_DESCRIPTOR_HEAP_DESC rtv_desc = {};
	rtv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtv_desc.NodeMask = 0;
	rtv_desc.NumDescriptors = FrameCount;
	rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&m_rtvHeap)));

	// #NOTA: Es posible que esto haya que moverlo a debajo del dsv_desc
	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


	D3D12_DESCRIPTOR_HEAP_DESC dsv_desc = {};
	dsv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsv_desc.NumDescriptors = 1;
	dsv_desc.NodeMask = 0;
	dsv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&dsv_desc, IID_PPV_ARGS(&m_dsvHeap)));

	/*
	D3D12_DESCRIPTOR_HEAP_DESC cbv_desc = {};
	cbv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbv_desc.NodeMask = 0;
	cbv_desc.NumDescriptors = 1;
	cbv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&cbv_desc, IID_PPV_ARGS(&m_cbvHeap)));
	*/
		
	// Create frame resources (a render target view for each frame)
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT i = 0; i < FrameCount; ++i)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, m_rtvDescriptorSize);
		// Create a command allocator
		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator[i])));
		//ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&m_bundleAllocator[i])));
	}

	// Create depth-stencil view
	D3D12_DEPTH_STENCIL_VIEW_DESC ds_desc = {};
	ds_desc.Flags = D3D12_DSV_FLAG_NONE;
	ds_desc.Format = DXGI_FORMAT_D32_FLOAT;
	ds_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0),
		IID_PPV_ARGS(&m_depthStencil)));

	m_device->CreateDepthStencilView(m_depthStencil.Get(), &ds_desc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12App::InitAssets()
{
	// Create a root signature with a constant buffer view (as a root descriptor)


	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}
	

	CD3DX12_ROOT_PARAMETER1 rp[1] = {};
	rp[0].InitAsConstantBufferView(0, 0);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = 
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Init_1_1(_countof(rp), rp, 0, nullptr, rootSignatureFlags);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
	ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

	// Create the constant buffer and map the resource

	size_t cbSize = FrameCount * sizeof(ObjectCB);

	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(cbSize), 
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_constantBuffer.ReleaseAndGetAddressOf())));

	ThrowIfFailed(m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedConstantData)));
	m_constantDataGPUAddr = m_constantBuffer->GetGPUVirtualAddress();

	// Shader compilation

	ComPtr<ID3DBlob> vShader;
	ComPtr<ID3DBlob> pShader;

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"BasicShader.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vShader, nullptr));
	ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"BasicShader.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pShader, nullptr));

	// Create the vertex input layout
	D3D12_INPUT_ELEMENT_DESC inputDesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Create a pipeline state object description, then the object

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputDesc, _countof(inputDesc) };
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vShader.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pShader.Get());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

	// Create the command list
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	 
	//Command lists are created in the recording state we leave it open to record the vertex buffer initialization commands
	 
	// Create and load the vertex buffers
	std::array<Vertex, 3> triangleVertices =
	{
		Vertex({ XMFLOAT4(0.0f, 0.25f, 0.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) }),
		Vertex({ XMFLOAT4(0.25f, -0.25f, 0.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) }),
		Vertex({ XMFLOAT4(-0.25f, -0.25f, 0.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) })
	};

	const UINT vertexStride = sizeof(Vertex);
	const UINT vertexBufferSize = sizeof(triangleVertices);

	// Create an abstraction with vertex data, buffer and uploader
	m_mesh = std::make_unique<Mesh>();
	
	m_mesh->vBufferSize = vertexBufferSize;
	m_mesh->vBufferStride = vertexStride;

	ThrowIfFailed(D3DCreateBlob(m_mesh->vBufferSize, &m_mesh->vBufferData));
	::CopyMemory(m_mesh->vBufferData->GetBufferPointer(), triangleVertices.data(), m_mesh->vBufferSize);

	m_mesh->vBuffer = CreateDefaultBuffer(triangleVertices.data(), m_mesh->vBufferUplader, m_mesh->vBufferSize);

	// Create the constant buffer;

	const UINT constantBufferSize = sizeof(ObjectCB);
	auto cbv_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBuffer)));

	// Create a fence
	ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	++m_fenceValues[m_frameIndex];

	// Create an event handle
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}


	// Close and execute the initialization commands
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	
	// Wait for the GPU to finish
	WaitForGpu();
}

void D3D12App::PopulateCommandList()
{
	// Reset the command list allocator

	ThrowIfFailed(m_commandAllocator[m_frameIndex].Get()->Reset());

	// Reset the command list

	ThrowIfFailed(m_commandList->Reset(m_commandAllocator[m_frameIndex].Get(), m_pipelineState.Get()));

	// Set the graphics root signature
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	// Set the viewport and scissor rectangles

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Set necessary state
	m_commandList->SetGraphicsRootConstantBufferView(0, m_constantDataGPUAddr);


	// Set a resource barrier, indicating the back buffer is to be used as a render target
	
	CD3DX12_RESOURCE_BARRIER rt_barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->ResourceBarrier(1, &rt_barrier);


	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands into the command list

	const float clearColor[] = { 1.0f, 0.2f, 1.f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &m_mesh->GetVertexBufferView());
	m_commandList->DrawInstanced(3, 1, 0, 0);

	// Indicate the back buffer will be used to present after the command list has executed
	CD3DX12_RESOURCE_BARRIER pst_barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_commandList->ResourceBarrier(1, &pst_barrier);

	// Close the command list to further recording
	ThrowIfFailed(m_commandList->Close());
}

void D3D12App::MoveToNextFrame()
{
	// Schedule a signal command in the queue
	const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	// Update the frame index
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

void D3D12App::WaitForGpu()
{
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	++m_fenceValues[m_frameIndex];
}

ComPtr<ID3D12Resource> D3D12App::CreateDefaultBuffer(const void* data, ComPtr<ID3D12Resource>& uploadBuffer, UINT size)
{
	// Create the default buffer
	ComPtr<ID3D12Resource> defBuffer;

	m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(size), D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&defBuffer));

	// Create an intermediate upload buffer to move the data to the default buffer
	
	m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(size), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));

	D3D12_SUBRESOURCE_DATA sbrdata = {};
	sbrdata.pData = data;
	sbrdata.RowPitch = size;
	sbrdata.SlicePitch = size;

	// Schedule a copy (transition -> copy -> transition)

	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

	UpdateSubresources<1>(m_commandList.Get(), defBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &sbrdata);

	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));

	return defBuffer;
}

