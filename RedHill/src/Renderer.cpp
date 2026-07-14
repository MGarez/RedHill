#include "Renderer.h"

#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Utils.h"
#include "Model.h"
#include "Camera.h"

Renderer::Renderer(HWND& hwnd):
	m_hWnd(hwnd),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<FLOAT>(RHConfig::width), static_cast<FLOAT>(RHConfig::height)),
	m_scissorRect(0, 0, static_cast<LONG>(RHConfig::width), static_cast<LONG>(RHConfig::height)),
	m_fenceValues{},
	m_sceneMode(SceneMode::SphereGrid),
	m_environmentIndex(0)
{
	m_environments[0].path = "resources/environments/alley.hdr";
	m_environments[1].path = "resources/environments/hills.hdr";
	m_environments[2].path = "resources/environments/aquarium.hdr";
	m_environments[3].path = "resources/environments/night.hdr";

	m_srvHeap = std::make_unique<DescriptorHeapAllocator>();
	m_rtvHeap = std::make_unique<DescriptorHeapAllocator>();
	m_dsvHeap = std::make_unique<DescriptorHeapAllocator>();
	m_computeMipMapsHeap = std::make_unique<DescriptorHeapAllocator>();
}

void Renderer::Init()
{
	InitPipeline();
	InitAssets();
}

void Renderer::Update(const Camera& camera)
{
	// Build model matrix (identity for now)
	XMMATRIX model = XMMatrixIdentity();

	// Build view matrix

	XMVECTOR position = camera.GetPosition();
	XMMATRIX view = camera.GetViewMatrix();

	// Build the projection matrix

	XMMATRIX projection = camera.GetProjectionMatrix();

	// Build the mvp
	ConstantFrameObject frameObject;

	XMMATRIX vpMatrix = view * projection;

	XMMATRIX mvpMatrix = model * vpMatrix;

	XMMATRIX invVP = XMMatrixInverse(nullptr, vpMatrix);

	// Build the ligh view and projection matrix
	XMVECTOR lightPosition = XMVectorSet(12.0f, 32.0f, 2.0f, 0.0f);
	XMVECTOR targetPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR upVector = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX lightView = XMMatrixLookAtLH(lightPosition, targetPosition, upVector);
	XMMATRIX lightProj = XMMatrixOrthographicLH(40.0f, 40.0f, 100.0f, 1.0f); // Near and far planes are swapped for reverse - Z
	XMMATRIX lightVP = lightView * lightProj;

	XMStoreFloat4x4(&frameObject.mvp, XMMatrixTranspose(mvpMatrix));
	XMStoreFloat4x4(&frameObject.invViewProj, XMMatrixTranspose(invVP));
	XMStoreFloat4x4(&frameObject.model, XMMatrixTranspose(model));
	XMStoreFloat4x4(&frameObject.lightViewProj, XMMatrixTranspose(lightVP));
	XMStoreFloat3(&frameObject.lightPos, lightPosition);
	XMStoreFloat3(&frameObject.cameraPos, position);
	frameObject.screenHeight = RHConfig::height;
	frameObject.screenWidth = RHConfig::width;
	frameObject.castsShadows = m_sceneMode == SceneMode::Object ? 1 : 0;
	frameObject.shadowMapSize = static_cast<float>(RHConfig::shadowMapSize);

	memcpy(m_constantBuffers[m_frameIndex].data, &frameObject, sizeof(frameObject));
}

void Renderer::Render()
{
	PopulateCommandList();

	// Execute the command list
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame
	CrashIfFailed(m_swapchain->Present(1, 0));

	// Wait for the GPU to finish
	MoveToNextFrame();
}

void Renderer::Destroy()
{
	// Wait for the gpu to finish all work
	WaitForGpu();

	// Close the handle
	CloseHandle(m_fenceEvent);
}

void Renderer::InitPipeline()
{
	// Enable debug layer
	UINT factoryFlags = 0;
#if defined (_DEBUG)
	ComPtr<ID3D12Debug> debugController;
	CrashIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
	factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	// Adapter selection

	ComPtr<IDXGIFactory6> factory;
	CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory));

	ComPtr<IDXGIAdapter1> adapter;
	for (UINT i = 0; SUCCEEDED(factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))); ++i)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		// Skip software adapters
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		// Check if it supports D3D12
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	// Create the device

	CrashIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));

	// Create the command queue

	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		CrashIfFailed(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));
	}

	// Create the Swapchain
	{
		ComPtr<IDXGISwapChain1> swapchain1;
		DXGI_SWAP_CHAIN_DESC1 desc = {};
		desc.BufferCount = RHConfig::frameNumber;
		desc.Width = RHConfig::width;
		desc.Height = RHConfig::height;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		CrashIfFailed(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), m_hWnd, &desc, nullptr, nullptr, &swapchain1));
		CrashIfFailed(factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER));
		CrashIfFailed(swapchain1.As(&m_swapchain));
	}

	m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

	// Create rtv heap

	m_rtvHeap->Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 25, 15); //  Sized for 2 backbuffers + 3 G-buffers + headroom for the bake-time cube RTV

	m_srvHeap->Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 30, 0);

	// Create per frame resources
	for (UINT i = 0; i < RHConfig::frameNumber; ++i)
	{
		// Create render target descriptors
		CrashIfFailed(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		m_backbufferHandles[i] = m_rtvHeap->AllocatePersistent();
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, m_backbufferHandles[i].cpu);

		CrashIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator[i])));
	}

	// Create the g-buffers RTVs (and guarantee they are contiguous) and emplace them in the SRV too
	m_albedoRtvHandle = m_rtvHeap->AllocatePersistent();
	m_normalRtvHandle = m_rtvHeap->AllocatePersistent();
	m_materialRtvHandle = m_rtvHeap->AllocatePersistent();

	m_albedoSrvHandle = m_srvHeap->AllocatePersistent();
	m_normalSrvHandle = m_srvHeap->AllocatePersistent();
	m_materialSrvHandle = m_srvHeap->AllocatePersistent();

	m_depthSrvHandle = m_srvHeap->AllocatePersistent(); // also allocate the depth buffer srv in a contiguous position with the gbuffers

	ConfigureRenderTarget(m_albedoRT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m_albedoRtvHandle.cpu, m_albedoSrvHandle.cpu);
	ConfigureRenderTarget(m_normalRT, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m_normalRtvHandle.cpu, m_normalSrvHandle.cpu);
	ConfigureRenderTarget(m_materialRT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, m_materialRtvHandle.cpu, m_materialSrvHandle.cpu);

	// Create dsv heap

	m_dsvHeap->Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 2, 0);
	m_depthDsvHandle = m_dsvHeap->AllocatePersistent();

	// Create depth buffer

	auto dsvHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto dsvResourceDescriptor = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, RHConfig::width, RHConfig::height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	auto dbClearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 0.0f, 0);

	CrashIfFailed(m_device->CreateCommittedResource(&dsvHeapProperties, D3D12_HEAP_FLAG_NONE,&dsvResourceDescriptor,D3D12_RESOURCE_STATE_DEPTH_READ, &dbClearValue,IID_PPV_ARGS(&m_depthStencil)));
	{
		// Create depth - stencil view
		D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
		desc.Flags = D3D12_DSV_FLAG_NONE;
		desc.Format = DXGI_FORMAT_D32_FLOAT;
		desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		m_device->CreateDepthStencilView(m_depthStencil.Get(), &desc, m_depthDsvHandle.cpu);
	}
	{
		// Create the srv descriptor for the depth buffer
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Texture2D.MipLevels = 1;
		m_device->CreateShaderResourceView(m_depthStencil.Get(), &desc, m_depthSrvHandle.cpu);
	}
}

void Renderer::InitAssets()
{
	// Create the command list and leave it open for initialization purposes
	CrashIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

	// Create utils and shader compiler
	CrashIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils)));
	CrashIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_shaderCompiler)));

	LoadAssets();
	SetupShadowPass();
	SetupGeometryPass();
	SetupLightPass();
	SetupEnvironments();

	SetupConstantBuffers();

	// Create a fence
	CrashIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	++m_fenceValues[m_frameIndex];

	// Create an event handler
	m_fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		CrashIfFailed(HRESULT_FROM_WIN32(::GetLastError()));
	}

	// Close and execute the initialization commands
	CrashIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Wait for the GPU to finish
	WaitForGpu();

	// Here we could release the upload resources and the dead compute heap but we will keep them for now.
	// TODO: Improve the general resource management (upload resources, dead resources, etc)
}

void Renderer::PopulateCommandList()
{
	// Reset the command allocator (safe because GPU is done)
	CrashIfFailed(m_commandAllocator[m_frameIndex]->Reset());

	// Reset the command list with the allocator and set the geometry pass PSO
	CrashIfFailed(m_commandList->Reset(m_commandAllocator[m_frameIndex].Get(), nullptr));

	// Set the full descriptor heap

	ID3D12DescriptorHeap* heaps[] = { m_srvHeap->Heap() };
	m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

	// Record the shadow pass only in Object mode (sphere-grid should skip the shadow pass)
	if (m_sceneMode == SceneMode::Object)
	{
	RecordShadowPass();
	}

	// Set the viewport and scissor rect
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// ------- Geometry pass ---------

	// Transition the gbuffers and depth buffer from (pixel_shader_resource) to render target(in case of the gbuffers) and depth write (in case of the depth buffer )
	CD3DX12_RESOURCE_BARRIER barriersToGeometry[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(m_albedoRT.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(m_normalRT.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(m_materialRT.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencil.Get(), D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE),
	};
	m_commandList->ResourceBarrier(_countof(barriersToGeometry), barriersToGeometry);

	// Set the gbuffers as render targets
	m_commandList->OMSetRenderTargets(3, &m_albedoRtvHandle.cpu, true, &m_depthDsvHandle.cpu); // We know that the handles are contiguous so we can use the first one and set the rest automatically

	// clear the render targets and depth buffer
	const float gbufferClearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };

	m_commandList->ClearRenderTargetView(m_albedoRtvHandle.cpu, gbufferClearColor, 0, nullptr);
	m_commandList->ClearRenderTargetView(m_normalRtvHandle.cpu, gbufferClearColor, 0, nullptr);
	m_commandList->ClearRenderTargetView(m_materialRtvHandle.cpu, gbufferClearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(m_depthDsvHandle.cpu, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);

	if (m_sceneMode == SceneMode::Object)
	{
		DrawFloor();
		DrawObject();
	}
	else if (m_sceneMode == SceneMode::SphereGrid)
	{
		DrawSphereGrid();
	}

	// ------- Light pass ----
	// // Transition the gbuffers and depth buffer  from render target(in case of the gbuffers) and depth write (in case of the depth buffer) to from (pixel_shader_resource) and the backbuffer rt from present to render target
	CD3DX12_RESOURCE_BARRIER barriersToLight[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(m_albedoRT.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(m_normalRT.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(m_materialRT.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencil.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),  D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
	};

	m_commandList->ResourceBarrier(_countof(barriersToLight), barriersToLight);
	// Set state for the light pass (light pass PSO, root signature, root constants if needed)
	m_commandList->SetPipelineState(m_lightPSO.Get());
	m_commandList->SetGraphicsRootSignature(m_lightRootSignature.Get());
	m_commandList->SetGraphicsRootConstantBufferView(1, m_constantBuffers[m_frameIndex].resource->GetGPUVirtualAddress());

	// Set  graphic root descriptor table with the appropriate srv handle and range (the gbuffers and depth buffer that are contiguous in the srv heap)
	m_commandList->SetGraphicsRootDescriptorTable(0, m_albedoSrvHandle.gpu);

	// Set the other root descriptor tables for the brdf lut
	m_commandList->SetGraphicsRootDescriptorTable(2, m_brdfLutSrvHandle.gpu);

	// Set the descriptor table with the shadow map
	m_commandList->SetGraphicsRootDescriptorTable(3, m_shadowSrvHandle.gpu);

	// Set the other root descriptor tables for the environment maps (irradiance and prefilter which are contiguous by design)
	m_commandList->SetGraphicsRootDescriptorTable(4, m_environments[m_environmentIndex].irradianceSrvHandle.gpu);

	// Set the backbuffer as render target
	m_commandList->OMSetRenderTargets(1,&m_backbufferHandles[m_frameIndex].cpu , false, nullptr); // dont use depth buffer here

	// Draw the fullscreen triangle
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	// ------- Skybox pass -------
	CD3DX12_RESOURCE_BARRIER toDepthRead = CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencil.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_READ);
	m_commandList->ResourceBarrier(1, &toDepthRead);

	m_commandList->SetPipelineState(m_skyPSO.Get());
	m_commandList->SetGraphicsRootSignature(m_skyRootSignature.Get());
	m_commandList->SetGraphicsRootConstantBufferView(1, m_constantBuffers[m_frameIndex].resource->GetGPUVirtualAddress());
	m_commandList->SetGraphicsRootDescriptorTable(0, m_environments[m_environmentIndex].cubemapSrvHandle.gpu);

	// Set the backbuffer now using the depth buffer
	m_commandList->OMSetRenderTargets(1, &m_backbufferHandles[m_frameIndex].cpu, false, &m_depthDsvHandle.cpu);

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	// Transition back buffer: RENDER_TARGET → PRESENT
	CD3DX12_RESOURCE_BARRIER barrierRtToPres = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_commandList->ResourceBarrier(1, &barrierRtToPres);

	CrashIfFailed(m_commandList->Close());
}

void Renderer::MoveToNextFrame()
{
	// Signal the current frame
	const UINT64 currentValue = m_fenceValues[m_frameIndex];
	CrashIfFailed(m_commandQueue->Signal(m_fence.Get(), currentValue));

	// Change the frame index, check if the work is completed and if not wait until is ready
	m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();

	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		CrashIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next completion:
	m_fenceValues[m_frameIndex] = currentValue + 1;
}

void Renderer::WaitForGpu()
{
	CrashIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	CrashIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	++m_fenceValues[m_frameIndex];
}

void Renderer::LoadAssets()
{

	m_object = std::make_unique<PBRMesh>();

	m_object->albedoTextureSrvHandle = m_srvHeap->AllocatePersistent();
	m_object->normalTextureSrvHandle = m_srvHeap->AllocatePersistent();
	m_object->metalRoughnessTextureSrvHandle = m_srvHeap->AllocatePersistent();
	m_object->aoTextureSrvHandle = m_srvHeap->AllocatePersistent();

	m_object->albedoTexture = CreateTextureFromFile(m_object->albedoTextureUploader, "resources/Default_albedo.jpg", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, m_object->albedoTextureSrvHandle.cpu);
	m_object->normalTexture = CreateTextureFromFile(m_object->normalTextureUploader, "resources/Default_normal.jpg", DXGI_FORMAT_R8G8B8A8_UNORM, m_object->normalTextureSrvHandle.cpu);
	m_object->metalRoughnessTexture = CreateTextureFromFile(m_object->metalRoughnessTextureUploader, "resources/Default_metalRoughness.jpg", DXGI_FORMAT_R8G8B8A8_UNORM, m_object->metalRoughnessTextureSrvHandle.cpu);
	m_object->aoTexture = CreateTextureFromFile(m_object->aoTextureUploader, "resources/Default_AO.jpg", DXGI_FORMAT_R8G8B8A8_UNORM, m_object->aoTextureSrvHandle.cpu);
	m_object->GenerateVertexAndIndexFromObj("resources/helmet.obj");

	{
		// Initialize vertex buffer
		m_object->vBufferStride = sizeof(Vertex);
		m_object->vBufferSize = m_object->vertices_data.size() * sizeof(Vertex);

		D3D12_SUBRESOURCE_DATA vBufferData = {};
		vBufferData.pData = m_object->vertices_data.data();
		vBufferData.RowPitch = m_object->vBufferSize;
		vBufferData.SlicePitch = m_object->vBufferSize;

		auto vBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_object->vBufferSize);
		m_object->vBuffer = CreateDefaultResource(m_object->vBufferUploader, vBufferDesc, vBufferData, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		// Initialize index buffer
		m_object->iBufferFormat = DXGI_FORMAT_R32_UINT;
		m_object->iBufferSize = m_object->indices_data.size() * sizeof(uint32_t);

		D3D12_SUBRESOURCE_DATA iBufferData = {};
		iBufferData.pData = m_object->indices_data.data();
		iBufferData.RowPitch = m_object->iBufferSize;
		iBufferData.SlicePitch = m_object->iBufferSize;

		auto iBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_object->iBufferSize);
		m_object->iBuffer = CreateDefaultResource(m_object->iBufferUploader, iBufferDesc, iBufferData, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	}

	// Sphere grid setup
	m_sphereGrid = std::make_unique<PBRMesh>();
	m_sphereGrid->GenerateSphere(4);
	{
		// Initialize vertex buffer
		m_sphereGrid->vBufferStride = sizeof(Vertex);
		m_sphereGrid->vBufferSize = m_sphereGrid->vertices_data.size() * sizeof(Vertex);

		D3D12_SUBRESOURCE_DATA vBufferData = {};
		vBufferData.pData = m_sphereGrid->vertices_data.data();
		vBufferData.RowPitch = m_sphereGrid->vBufferSize;
		vBufferData.SlicePitch = m_sphereGrid->vBufferSize;

		auto vBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_sphereGrid->vBufferSize);
		m_sphereGrid->vBuffer = CreateDefaultResource(m_sphereGrid->vBufferUploader, vBufferDesc, vBufferData, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		// Initialize index buffer
		m_sphereGrid->iBufferFormat = DXGI_FORMAT_R32_UINT;
		m_sphereGrid->iBufferSize = m_sphereGrid->indices_data.size() * sizeof(uint32_t);

		D3D12_SUBRESOURCE_DATA iBufferData = {};
		iBufferData.pData = m_sphereGrid->indices_data.data();
		iBufferData.RowPitch = m_sphereGrid->iBufferSize;
		iBufferData.SlicePitch = m_sphereGrid->iBufferSize;

		auto iBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_sphereGrid->iBufferSize);
		m_sphereGrid->iBuffer = CreateDefaultResource(m_sphereGrid->iBufferUploader, iBufferDesc, iBufferData, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	}

	m_floor = std::make_unique<PBRMesh>();
	m_floor->GenerateFloor(15.0f);

	{
		// Initialize vertex buffer
		m_floor->vBufferStride = sizeof(Vertex);
		m_floor->vBufferSize = m_floor->vertices_data.size() * sizeof(Vertex);

		D3D12_SUBRESOURCE_DATA vBufferData = {};
		vBufferData.pData = m_floor->vertices_data.data();
		vBufferData.RowPitch = m_floor->vBufferSize;
		vBufferData.SlicePitch = m_floor->vBufferSize;

		auto vBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_floor->vBufferSize);
		m_floor->vBuffer = CreateDefaultResource(m_floor->vBufferUploader, vBufferDesc, vBufferData, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		// Initialize index buffer
		m_floor->iBufferFormat = DXGI_FORMAT_R32_UINT;
		m_floor->iBufferSize = m_floor->indices_data.size() * sizeof(uint32_t);

		D3D12_SUBRESOURCE_DATA iBufferData = {};
		iBufferData.pData = m_floor->indices_data.data();
		iBufferData.RowPitch = m_floor->iBufferSize;
		iBufferData.SlicePitch = m_floor->iBufferSize;

		auto iBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_floor->iBufferSize);
		m_floor->iBuffer = CreateDefaultResource(m_floor->iBufferUploader, iBufferDesc, iBufferData, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	}

}

void Renderer::SetupShadowPass()
{

	// Create shadowmap resource and handles

	auto shadowmapResourceDescriptor = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, RHConfig::shadowMapSize, RHConfig::shadowMapSize, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	auto shadowHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto shadowClearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 0.0f, 0);
	CrashIfFailed(m_device->CreateCommittedResource(&shadowHeapProperties, D3D12_HEAP_FLAG_NONE, &shadowmapResourceDescriptor, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &shadowClearValue, IID_PPV_ARGS(&m_shadowMap)));

	// Get handlers for the srv and dsv
	m_shadowDsvHandle = m_dsvHeap->AllocatePersistent();
	m_shadowSrvHandle = m_srvHeap->AllocatePersistent();

	{
		D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
		desc.Flags = D3D12_DSV_FLAG_NONE;
		desc.Format = DXGI_FORMAT_D32_FLOAT;
		desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		m_device->CreateDepthStencilView(m_shadowMap.Get(), &desc, m_shadowDsvHandle.cpu);
	}
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Texture2D.MipLevels = 1;
		m_device->CreateShaderResourceView(m_shadowMap.Get(), &desc, m_shadowSrvHandle.cpu);
	}

	m_shadowRootSignature = BuildNoTextureGeoRootSignature();

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,0 ,0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	// Compile shaders
	auto vertexShader = CompileShader(L"Shaders/ShadowShader.hlsl", L"VSMain", L"vs_6_0");

	// Describe and create the graphics pipeline state object (PSO)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = m_shadowRootSignature.Get();
		desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		desc.InputLayout = { inputLayout, _countof(inputLayout) };
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.DepthStencilState.DepthEnable = TRUE;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 0;
		desc.SampleDesc.Count = 1;
		CrashIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_shadowPSO)));
	}
}

void Renderer::SetupGeometryPass()
{
	SetupFloorGeometry();
	SetupObjectModeGeometry();
	SetupSphereGridGeometry();
}

void Renderer::SetupFloorGeometry()
{
	m_floorRootSignature = BuildNoTextureGeoRootSignature();
	m_floorPSO = BuildNoTextureGeoPSO(m_floorRootSignature.Get(), L"Shaders/NoTextureMesh.hlsl");
}

void Renderer::SetupObjectModeGeometry()
{
	// Create the root signature
	{
		// Declare and initialize a descriptor table
		CD3DX12_DESCRIPTOR_RANGE1 descRange[1];
		descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		// Create and initialize the root parameters list
		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc;
		rootDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		CrashIfFailed(D3DX12SerializeVersionedRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
		CrashIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_geoObjectRootSignature)));
	}

	// Define Input layout
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,0 ,0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,0 ,12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};
	auto vertexShader = CompileShader(L"Shaders/GeometryShader.hlsl", L"VSMain", L"vs_6_0");
	auto pixelShader = CompileShader(L"Shaders/GeometryShader.hlsl", L"PSMain", L"ps_6_0");

	// Describe and create the graphics pipeline state object (PSO)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.InputLayout = { inputLayout, _countof(inputLayout) };
		desc.pRootSignature = m_geoObjectRootSignature.Get();
		desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 3;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		CrashIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_geoObjectPSO)));
	}
}

void Renderer::SetupSphereGridGeometry()
{

	m_geoSphereRootSignature = BuildNoTextureGeoRootSignature();
	m_geoSpherePSO = BuildNoTextureGeoPSO(m_geoSphereRootSignature.Get(), L"Shaders/SphereGridGeo.hlsl");

}

void Renderer::SetupLightPass()
{
	// Create a root signature
	{
		// Declare and initialize a descriptor table
		CD3DX12_DESCRIPTOR_RANGE1 descRange[4];
		descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		descRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		descRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		descRange[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 6, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

		// Create and initialize the root parameters list (just a descriptor table for now)
		CD3DX12_ROOT_PARAMETER1 rootParameters[5];
		rootParameters[0].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[2].InitAsDescriptorTable(1, &descRange[1], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[3].InitAsDescriptorTable(1, &descRange[2], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[4].InitAsDescriptorTable(1, &descRange[3], D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_STATIC_SAMPLER_DESC samplerDesc[2] = {};

		samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc[0].MipLODBias = 0;
		samplerDesc[0].MaxAnisotropy = 0;
		samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc[0].MinLOD = 0.0f;
		samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc[0].ShaderRegister = 0;
		samplerDesc[0].RegisterSpace = 0;
		samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		samplerDesc[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc[1].MipLODBias = 0;
		samplerDesc[1].MaxAnisotropy = 1;
		samplerDesc[1].ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		samplerDesc[1].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc[1].MinLOD = 0.0f;
		samplerDesc[1].MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc[1].ShaderRegister = 1;
		samplerDesc[1].RegisterSpace = 0;
		samplerDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc;
		rootDesc.Init_1_1(_countof(rootParameters), rootParameters, _countof(samplerDesc), samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_NONE);
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		CrashIfFailed(D3DX12SerializeVersionedRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
		CrashIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_lightRootSignature)));
	}

	// Compile shaders
	auto vertexShader = CompileShader(L"Shaders/LightShader.hlsl", L"VSMain", L"vs_6_0");
	auto pixelShader = CompileShader(L"Shaders/LightShader.hlsl", L"PSMain", L"ps_6_0");

	// Describe and create the graphics pipeline state object (PSO)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = m_lightRootSignature.Get();
		desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.DepthStencilState.DepthEnable = FALSE;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		CrashIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_lightPSO)));
	}

}

void Renderer::SetupConstantBuffers()
{
	// Create and setup the constant buffers
	const UINT constantBufferSize = (sizeof(ConstantFrameObject) + 255) & ~255;
	auto properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

	for (size_t i = 0; i < RHConfig::frameNumber; ++i)
	{
		CrashIfFailed(m_device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBuffers[i].resource)));
		CrashIfFailed(m_constantBuffers[i].resource->Map(0, nullptr, reinterpret_cast<void**>(&m_constantBuffers[i].data)));
	}

}

void Renderer::DrawFloor()
{
	// Set state for the geometry pass (geometry pass root signature, the PSO and the root constant).
	m_commandList->SetPipelineState(m_floorPSO.Get());
	m_commandList->SetGraphicsRootSignature(m_floorRootSignature.Get());
	m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffers[m_frameIndex].resource->GetGPUVirtualAddress());

	// set primitive topology, vertex and index buffer and draw

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	auto vview = m_floor->GetVertexBufferView();
	auto iview = m_floor->GetIndexBufferView();
	m_commandList->IASetVertexBuffers(0, 1, &vview);
	m_commandList->IASetIndexBuffer(&iview);
	m_commandList->DrawIndexedInstanced(m_floor->indices_data.size(), 1, 0, 0, 0);
}

void Renderer::DrawObject()
{
	// Set state for the geometry pass (geometry pass root signature, the PSO and the root constant).
	m_commandList->SetPipelineState(m_geoObjectPSO.Get());
	m_commandList->SetGraphicsRootSignature(m_geoObjectRootSignature.Get());
	m_commandList->SetGraphicsRootConstantBufferView(1, m_constantBuffers[m_frameIndex].resource->GetGPUVirtualAddress());

	// Set graphic root descriptor table with the appropriate srv handle and range (the textures are right after the gbuffer rts + depth buffer
	m_commandList->SetGraphicsRootDescriptorTable(0, m_object->albedoTextureSrvHandle.gpu);

	// set primitive topology, vertex and index buffer and draw

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	auto vview = m_object->GetVertexBufferView();
	auto iview = m_object->GetIndexBufferView();
	m_commandList->IASetVertexBuffers(0, 1, &vview);
	m_commandList->IASetIndexBuffer(&iview);
	m_commandList->DrawIndexedInstanced(m_object->indices_data.size(), 1, 0, 0, 0);

}

void Renderer::DrawSphereGrid()
{
	// Set state for the geometry pass (geometry pass root signature, the PSO and the root constant).
	m_commandList->SetPipelineState(m_geoSpherePSO.Get());
	m_commandList->SetGraphicsRootSignature(m_geoSphereRootSignature.Get());
	m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffers[m_frameIndex].resource->GetGPUVirtualAddress());

	// set primitive topology, vertex and index buffer and draw

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	auto vview = m_sphereGrid->GetVertexBufferView();
	auto iview = m_sphereGrid->GetIndexBufferView();
	m_commandList->IASetVertexBuffers(0, 1, &vview);
	m_commandList->IASetIndexBuffer(&iview);
	m_commandList->DrawIndexedInstanced(m_sphereGrid->indices_data.size(), 25, 0, 0, 0);

}

ComPtr<ID3D12RootSignature> Renderer::BuildNoTextureGeoRootSignature()
{
	CD3DX12_ROOT_PARAMETER1 rootParameters[1];
	rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, D3D12_SHADER_VISIBILITY_VERTEX);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc;
	rootDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	CrashIfFailed(D3DX12SerializeVersionedRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));

	ComPtr<ID3D12RootSignature> rootSig;
	CrashIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSig)));
	return rootSig;
}

ComPtr<ID3D12PipelineState> Renderer::BuildNoTextureGeoPSO(ID3D12RootSignature* rootSig, const wchar_t* shaderPath)
{
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,0 ,0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0 , 20 , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
	auto vertexShader = CompileShader(shaderPath, L"VSMain", L"vs_6_0");
	auto pixelShader = CompileShader(shaderPath, L"PSMain", L"ps_6_0");

	// Describe and create the graphics pipeline state object (PSO)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.InputLayout = { inputLayout, _countof(inputLayout) };
	desc.pRootSignature = rootSig;
	desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 3;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> pso;
	CrashIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)));
	return pso;
}

void Renderer::ChangeSceneMode()
{
	if (m_sceneMode == SceneMode::Object)
	{
		m_sceneMode = SceneMode::SphereGrid;
	}
	else if (m_sceneMode == SceneMode::SphereGrid)
	{
		m_sceneMode = SceneMode::Object;
	}
}

void Renderer::SetupEnvironments()
{
	// Init the resources for each environment map
	D3D12_RESOURCE_DESC cubemapDesc = {};
	cubemapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	cubemapDesc.Width = 1024;                       // per-face size
	cubemapDesc.Height = 1024;
	cubemapDesc.DepthOrArraySize = 6;               // 6 faces
	cubemapDesc.MipLevels = 11;
	cubemapDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	cubemapDesc.SampleDesc.Count = 1;
	cubemapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_RESOURCE_DESC irradianceMapDesc = {};
	irradianceMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	irradianceMapDesc.Width = 32;
	irradianceMapDesc.Height = 32;
	irradianceMapDesc.DepthOrArraySize = 6;
	irradianceMapDesc.MipLevels = 1;
	irradianceMapDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	irradianceMapDesc.SampleDesc.Count = 1;
	irradianceMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_RESOURCE_DESC prefilterMapDesc = {};
	prefilterMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	prefilterMapDesc.Width = 128;
	prefilterMapDesc.Height = 128;
	prefilterMapDesc.DepthOrArraySize = 6;
	prefilterMapDesc.MipLevels = 5;
	prefilterMapDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	prefilterMapDesc.SampleDesc.Count = 1;
	prefilterMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_SHADER_RESOURCE_VIEW_DESC cubeMapSRV = {};
	cubeMapSRV.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	cubeMapSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	cubeMapSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	cubeMapSRV.TextureCube.MostDetailedMip = 0;
	cubeMapSRV.TextureCube.MipLevels = 11;

	D3D12_SHADER_RESOURCE_VIEW_DESC irradianceMapSRV = {};
	irradianceMapSRV.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	irradianceMapSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	irradianceMapSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	irradianceMapSRV.TextureCube.MipLevels = 1;

	D3D12_SHADER_RESOURCE_VIEW_DESC prefilterMapSRV = {};
	prefilterMapSRV.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	prefilterMapSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	prefilterMapSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	prefilterMapSRV.TextureCube.MostDetailedMip = 0;
	prefilterMapSRV.TextureCube.MipLevels = 5;

	for (UINT i = 0; i < RHConfig::environmentsNumber; ++i)
	{

		// Store the SRV handles for the environment maps (equirectangular, cubemap, irradiance and prefilter) contiguous in the SRV heap.
		m_environments[i].equirectSrvHandle = m_srvHeap->AllocatePersistent();
		m_environments[i].cubemapSrvHandle = m_srvHeap->AllocatePersistent();
		m_environments[i].irradianceSrvHandle = m_srvHeap->AllocatePersistent();
		m_environments[i].prefilterSrvHandle = m_srvHeap->AllocatePersistent();

		// Load the HDR equirectangular texture
		m_environments[i].equirect = CreateHDRTextureFromFile(m_environments[i].equirectUploader, m_environments[i].path, m_environments[i].equirectSrvHandle.cpu);

		// Create the cubemap resource and its srv entry
		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
		CrashIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &cubemapDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&m_environments[i].cubemap)));
		m_device->CreateShaderResourceView(m_environments[i].cubemap.Get(), &cubeMapSRV, m_environments[i].cubemapSrvHandle.cpu);

		// Create the irradiance map resource and its srv entry (same description as the cubemap)
		CrashIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &irradianceMapDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&m_environments[i].irradiance)));
		m_device->CreateShaderResourceView(m_environments[i].irradiance.Get(), &irradianceMapSRV, m_environments[i].irradianceSrvHandle.cpu);

		CrashIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &prefilterMapDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&m_environments[i].prefilter)));
		m_device->CreateShaderResourceView(m_environments[i].prefilter.Get(), &prefilterMapSRV, m_environments[i].prefilterSrvHandle.cpu);
	}

	BakeEnvironmentCubemap();
	GenerateMipMaps(1024, 11);
	BakeIrradianceMap();
	BakePrefilterMap();
	BakeBrdfLut();

	// Set up skybox pass resources
	{
		CD3DX12_DESCRIPTOR_RANGE1 descRange[1];
		descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc;
		rootDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_NONE);
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		CrashIfFailed(D3DX12SerializeVersionedRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
		CrashIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_skyRootSignature)));

	}

	auto vertexShader = CompileShader(L"Shaders/SkyBoxShader.hlsl", L"VSMain", L"vs_6_0");
	auto pixelShader = CompileShader(L"Shaders/SkyBoxShader.hlsl", L"PSMain", L"ps_6_0");

	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = m_skyRootSignature.Get();
		desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.DepthStencilState.DepthEnable = TRUE;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		desc.SampleDesc.Count = 1;
		CrashIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_skyPSO)));
	}

}

void Renderer::RecordShadowPass()
{
	// Transition the shadow map to write the depth
	CD3DX12_RESOURCE_BARRIER shadowToDepthWrite = CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	m_commandList->ResourceBarrier(1, &shadowToDepthWrite);

	// Set the viewport and scissor
	CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(RHConfig::shadowMapSize), static_cast<float>(RHConfig::shadowMapSize));
	CD3DX12_RECT     scissor(0, 0, RHConfig::shadowMapSize, RHConfig::shadowMapSize);
	m_commandList->RSSetViewports(1, &viewport);
	m_commandList->RSSetScissorRects(1, &scissor);

	m_commandList->OMSetRenderTargets(0, nullptr, false, &m_shadowDsvHandle.cpu);

	m_commandList->ClearDepthStencilView(m_shadowDsvHandle.cpu, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
	m_commandList->SetPipelineState(m_shadowPSO.Get());
	m_commandList->SetGraphicsRootSignature(m_shadowRootSignature.Get());
	m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffers[m_frameIndex].resource->GetGPUVirtualAddress());

	// Draw the floor

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	auto floorVView = m_floor->GetVertexBufferView();
	auto floorIView = m_floor->GetIndexBufferView();
	m_commandList->IASetVertexBuffers(0, 1, &floorVView);
	m_commandList->IASetIndexBuffer(&floorIView);
	m_commandList->DrawIndexedInstanced(m_floor->indices_data.size(), 1, 0, 0, 0);

	// Draw the object
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	auto objectVView = m_object->GetVertexBufferView();
	auto objectIView = m_object->GetIndexBufferView();
	m_commandList->IASetVertexBuffers(0, 1, &objectVView);
	m_commandList->IASetIndexBuffer(&objectIView);
	m_commandList->DrawIndexedInstanced(m_object->indices_data.size(), 1, 0, 0, 0);

	CD3DX12_RESOURCE_BARRIER shadowToPixelResource = CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_commandList->ResourceBarrier(1, &shadowToPixelResource);

	// Restore main viewport & scissor in case the shadow pass changed them
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

}

void Renderer::BakeEnvironmentCubemap()
{
	// Create the root signature for the baking pass
	{
		CD3DX12_DESCRIPTOR_RANGE1 descRange[1];
		descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsConstants(12, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc;
		rootDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_NONE);
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		CrashIfFailed(D3DX12SerializeVersionedRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
		CrashIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_bakeRootSignature)));

	}

	m_bakePSO = BuildBakePSO(m_bakeRootSignature.Get(), L"Shaders/BakeCubemapShader.hlsl");

	BeginBakePass(m_bakePSO.Get(), m_bakeRootSignature.Get());

	for (UINT env = 0; env < RHConfig::environmentsNumber; ++env)
	{
		m_commandList->SetGraphicsRootDescriptorTable(0, m_environments[env].equirectSrvHandle.gpu);

		RenderCubemapFaces(m_environments[env].cubemap.Get(), 0, 1024);
		CD3DX12_RESOURCE_BARRIER toSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_environments[env].cubemap.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_commandList->ResourceBarrier(1, &toSRV);
	}
}

void Renderer::BakeIrradianceMap()
{
	// Create the root signature for the baking pass
	{
		CD3DX12_DESCRIPTOR_RANGE1 descRange[1];
		descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsConstants(12, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc;
		rootDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_NONE);
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		CrashIfFailed(D3DX12SerializeVersionedRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
		CrashIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_irradianceRootSignature)));

	}

	m_irradiancePSO = BuildBakePSO(m_irradianceRootSignature.Get(), L"Shaders/IrradianceBake.hlsl");

	BeginBakePass(m_irradiancePSO.Get(), m_irradianceRootSignature.Get());

	for (UINT env = 0; env < RHConfig::environmentsNumber; ++env)
	{
		m_commandList->SetGraphicsRootDescriptorTable(0, m_environments[env].cubemapSrvHandle.gpu);
		RenderCubemapFaces(m_environments[env].irradiance.Get(), 0, 32);
		CD3DX12_RESOURCE_BARRIER toSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_environments[env].irradiance.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_commandList->ResourceBarrier(1, &toSRV);

	}
}

void Renderer::BakePrefilterMap()
{
	// Create the root signature for the baking pass
	{
		CD3DX12_DESCRIPTOR_RANGE1 descRange[1];
		descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[0].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsConstants(12, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[2].InitAsConstants(1, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc;
		rootDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_NONE);
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		CrashIfFailed(D3DX12SerializeVersionedRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
		CrashIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_prefilterRootSignature)));

	}

	m_prefilterPSO = BuildBakePSO(m_prefilterRootSignature.Get(), L"Shaders/PrefilterBake.hlsl");

	BeginBakePass(m_prefilterPSO.Get(), m_prefilterRootSignature.Get());

	for (UINT env = 0; env < RHConfig::environmentsNumber; ++env)
	{
		m_commandList->SetGraphicsRootDescriptorTable(0, m_environments[env].cubemapSrvHandle.gpu);
		for (UINT mip = 0; mip < 5; ++mip)
		{
			float roughness = static_cast<float>(mip) / 4.0f; // roughness in [0,1] across the 5 prefilter mips
			m_commandList->SetGraphicsRoot32BitConstants(2, 1, &roughness, 0);

			UINT mipSize = 128 >> mip;
			RenderCubemapFaces(m_environments[env].prefilter.Get(), mip, mipSize);
		}
		CD3DX12_RESOURCE_BARRIER toSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_environments[env].prefilter.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_commandList->ResourceBarrier(1, &toSRV);
	}
}

void Renderer::BeginBakePass(ID3D12PipelineState* pso, ID3D12RootSignature* rootSig)
{
	m_commandList->SetPipelineState(pso);
	m_commandList->SetGraphicsRootSignature(rootSig);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* heaps[] = { m_srvHeap->Heap() };
	m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);
}

static const XMFLOAT3 kFaceForward[6] =
{
	{ 1, 0, 0}, {-1, 0, 0}, {0,  1, 0}, {0, -1, 0}, {0, 0,  1}, {0, 0, -1}
};
static const XMFLOAT3 kFaceUp[6] =
{
	{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}
};

// Set the per-face orientation basis as VS root constants (param 1, 12 floats).
void Renderer::SetFaceBasis(UINT face)
{
	XMVECTOR fwd = XMLoadFloat3(&kFaceForward[face]);
	XMVECTOR up = XMLoadFloat3(&kFaceUp[face]);
	XMVECTOR right = XMVector3Cross(up, fwd);

	struct { XMFLOAT4 r0, r1, r2; } basis;
	XMStoreFloat4(&basis.r0, right);
	XMStoreFloat4(&basis.r1, up);
	XMStoreFloat4(&basis.r2, fwd);
	m_commandList->SetGraphicsRoot32BitConstants(1, 12, &basis, 0);
}

// All three bake PSOs are identical except the shader.
ComPtr<ID3D12PipelineState> Renderer::BuildBakePSO(ID3D12RootSignature* rootSig, const wchar_t* shaderPath)
{
	auto vs = CompileShader(shaderPath, L"VSMain", L"vs_6_0");
	auto ps = CompileShader(shaderPath, L"PSMain", L"ps_6_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = rootSig;
	desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> pso;
	CrashIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)));
	return pso;
}

void Renderer::RenderCubemapFaces(ID3D12Resource* target, UINT mip, UINT faceSize)
{
	CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(faceSize), static_cast<float>(faceSize));
	CD3DX12_RECT     scissor(0, 0, faceSize, faceSize);
	m_commandList->RSSetViewports(1, &viewport);
	m_commandList->RSSetScissorRects(1, &scissor);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
	rtvDesc.Texture2DArray.MipSlice = mip;
	rtvDesc.Texture2DArray.ArraySize = 1;
	rtvDesc.Texture2DArray.PlaneSlice = 0;

	for (UINT face = 0; face < 6; ++face)
	{
		rtvDesc.Texture2DArray.FirstArraySlice = face;
		DescriptorHandle rtv = m_rtvHeap->AllocateTransient();
		m_device->CreateRenderTargetView(target, &rtvDesc, rtv.cpu);

		SetFaceBasis(face);

		m_commandList->OMSetRenderTargets(1, &rtv.cpu, FALSE, nullptr);
		m_commandList->DrawInstanced(3, 1, 0, 0);
	}
	m_rtvHeap->ResetTransient();
}

void Renderer::BakeBrdfLut()
{
	// Create the brdf lut resource and emplace it on the srv
	{
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16_FLOAT, 512, 512, 1, 1);
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		CrashIfFailed(m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&m_brdfLUT)));
	}

	// Create the srv for the brdf lut
	m_brdfLutSrvHandle = m_srvHeap->AllocatePersistent();

	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		m_device->CreateShaderResourceView(m_brdfLUT.Get(), &srvDesc, m_brdfLutSrvHandle.cpu);
	}

	// Create the root signature for the LUT baking
	{
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc;
		rootDesc.Init_1_1(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		CrashIfFailed(D3DX12SerializeVersionedRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
		CrashIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_lutRootSignature)));
	}

	auto vertexShader = CompileShader(L"Shaders/Brdflut.hlsl", L"VSMain", L"vs_6_0");
	auto pixelShader = CompileShader(L"Shaders/Brdflut.hlsl", L"PSMain", L"ps_6_0");
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = m_lutRootSignature.Get();
		desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.DepthStencilState.DepthEnable = FALSE;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
		desc.SampleDesc.Count = 1;
		CrashIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_lutPSO)));
	}

	DescriptorHandle tempLutHandle = m_rtvHeap->AllocateTransient();
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		m_device->CreateRenderTargetView(m_brdfLUT.Get(), &rtvDesc, tempLutHandle.cpu);
	}

	// Record the single fullscreen draw.
	CD3DX12_VIEWPORT viewport(0.0f, 0.0f, 512.0f, 512.0f);
	CD3DX12_RECT scissor(0, 0, 512, 512);

	m_commandList->SetPipelineState(m_lutPSO.Get());
	m_commandList->SetGraphicsRootSignature(m_lutRootSignature.Get());
	m_commandList->RSSetViewports(1, &viewport);
	m_commandList->RSSetScissorRects(1, &scissor);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->OMSetRenderTargets(1, &tempLutHandle.cpu, FALSE, nullptr);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	CD3DX12_RESOURCE_BARRIER toSRV = CD3DX12_RESOURCE_BARRIER::Transition(m_brdfLUT.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_commandList->ResourceBarrier(1, &toSRV);

	m_rtvHeap->ResetTransient();
}

void Renderer::ConfigureRenderTarget(ComPtr<ID3D12Resource>& rtResource, const DXGI_FORMAT format, const D3D12_RESOURCE_STATES& state,  const CD3DX12_CPU_DESCRIPTOR_HANDLE& rtvHandle, const CD3DX12_CPU_DESCRIPTOR_HANDLE& srvHandle)
{
	// Create the target view
	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, RHConfig::width, RHConfig::height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	D3D12_CLEAR_VALUE clearVal = {};
	clearVal.Format = format;
	CrashIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &texDesc, state, &clearVal, IID_PPV_ARGS(&rtResource)));
	m_device->CreateRenderTargetView(rtResource.Get(), nullptr, rtvHandle);

	// Create the shader resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Texture2D.MipLevels = 1;
	m_device->CreateShaderResourceView(rtResource.Get(), &desc, srvHandle);
}

ComPtr<ID3D12Resource> Renderer::CreateDefaultResource(ComPtr<ID3D12Resource>& uploadResource, const CD3DX12_RESOURCE_DESC& resourceDesc, const D3D12_SUBRESOURCE_DATA& resourceData, const D3D12_RESOURCE_STATES& initialState, const D3D12_RESOURCE_STATES& finalState)
{

	auto heapPropertiesDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto heapPropertiesUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	// Create the default resource

	ComPtr<ID3D12Resource> defaultResource;
	CrashIfFailed(m_device->CreateCommittedResource(&heapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &resourceDesc, initialState, nullptr, IID_PPV_ARGS(&defaultResource)));

	UINT64 uploadSize = GetRequiredIntermediateSize(defaultResource.Get(), 0, 1);
	auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

	CrashIfFailed(m_device->CreateCommittedResource(&heapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadResource)));

	UpdateSubresources(m_commandList.Get(), defaultResource.Get(), uploadResource.Get(), 0, 0, 1, &resourceData);

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultResource.Get(), initialState, finalState);

	m_commandList->ResourceBarrier(1, &barrier);

	return defaultResource;
}

ComPtr<ID3D12Resource> Renderer::CreateTextureFromFile(ComPtr<ID3D12Resource>& uploadResource, const std::string& textureFile, const DXGI_FORMAT format, const CD3DX12_CPU_DESCRIPTOR_HANDLE& srvHandle)
{
	int textureWidth, textureHeight, textureChannels;

	// TODO: Check null or assert
	unsigned char* pixel = stbi_load(textureFile.c_str(), &textureWidth, &textureHeight, &textureChannels, 4);

	if (!pixel)
	{
		::OutputDebugStringA(("stbi_load failed: " + textureFile + "\n").c_str());
		::__debugbreak();
	}

	auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, textureWidth, textureHeight);

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = &pixel[0];
	textureData.RowPitch = textureWidth * 4 * sizeof(unsigned char);
	textureData.SlicePitch = textureData.RowPitch * textureHeight;

	ComPtr<ID3D12Resource> textureResource = CreateDefaultResource(uploadResource, texDesc, textureData,D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	stbi_image_free(pixel);

	// Create srv descriptor for the texture
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Texture2D.MipLevels = 1;

	m_device->CreateShaderResourceView(textureResource.Get(), &desc, srvHandle);

	return textureResource;
}

ComPtr<ID3D12Resource> Renderer::CreateHDRTextureFromFile(ComPtr<ID3D12Resource>& uploadResource, const std::string& textureFile, const CD3DX12_CPU_DESCRIPTOR_HANDLE& srvHandle)
{
	int textureWidth, textureHeight, textureChannels;

	// TODO: Check null or assert
	float* pixel = stbi_loadf(textureFile.c_str(), &textureWidth, &textureHeight, &textureChannels, 4);

	if (!pixel)
	{
		::OutputDebugStringA(("stbi_loadf failed: " + textureFile + "\n").c_str());
		::__debugbreak();
	}

	auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, textureWidth, textureHeight);

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = &pixel[0];
	textureData.RowPitch = static_cast<UINT64>(textureWidth) * 4 * sizeof(float);
	textureData.SlicePitch = textureData.RowPitch * textureHeight;

	ComPtr<ID3D12Resource> textureResource = CreateDefaultResource(uploadResource, texDesc, textureData, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	stbi_image_free(pixel);

	// Create srv descriptor for the texture
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Texture2D.MipLevels = 1;

	m_device->CreateShaderResourceView(textureResource.Get(), &desc, srvHandle);

	return textureResource;
}

ComPtr<IDxcBlob> Renderer::CompileShader(const std::wstring& filePath, const std::wstring& entryPoint, const std::wstring& target) const
{
	// Load the shader file
	ComPtr<IDxcBlobEncoding> sourceBlob;
	CrashIfFailed(m_utils->LoadFile(filePath.c_str(), nullptr, &sourceBlob));

	DxcBuffer sourceBuffer = {};
	sourceBuffer.Encoding = DXC_CP_ACP;
	sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
	sourceBuffer.Size = sourceBlob->GetBufferSize();

	// Build the vector arguments
	std::vector<LPCWSTR> args;
	args.push_back(L"-E"); args.push_back(entryPoint.c_str());
	args.push_back(L"-T"); args.push_back(target.c_str());
#if defined(_DEBUG)
	args.push_back(L"-Zi"); // Debug info
	args.push_back(L"-Zss");
	args.push_back(L"-Od");  // Skip optimization
#endif
	args.push_back(L"-I"); args.push_back(L"Shaders");

	ComPtr<IDxcIncludeHandler> includeHandler;
	CrashIfFailed(m_utils->CreateDefaultIncludeHandler(&includeHandler));

	ComPtr<IDxcResult> result;
	CrashIfFailed(m_shaderCompiler->Compile(&sourceBuffer, args.data(), static_cast<UINT32>(args.size()), includeHandler.Get(), IID_PPV_ARGS(&result)));

	// Check for errors
	ComPtr<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
	{
		::OutputDebugStringA(errors->GetStringPointer());
	}

	HRESULT compileStatus;
	result->GetStatus(&compileStatus);
	CrashIfFailed(compileStatus);

	// Get compiled shader
	ComPtr<IDxcBlob> shader;
	CrashIfFailed(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr));
	return shader;
}

void Renderer::GenerateMipMaps(const UINT baseSize, const UINT mipLevels)
{
	const UINT passes = mipLevels - 1;

	auto cs = CompileShader(L"Shaders/MipDownsample.hlsl", L"CSMain", L"cs_6_0");

	{
		CD3DX12_DESCRIPTOR_RANGE1 descRanges[2];
		descRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		descRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[0].InitAsDescriptorTable(2, descRanges);
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc;
		rootDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		CrashIfFailed(D3DX12SerializeVersionedRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
		CrashIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_computeMipMapsRootSignature)));
	}

	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = m_computeMipMapsRootSignature.Get();
		desc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };
		CrashIfFailed(m_device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_computeMipMapsPSO)));
	}

	m_computeMipMapsHeap->Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, RHConfig::environmentsNumber * passes * 2, 0);
	m_commandList->SetPipelineState(m_computeMipMapsPSO.Get());
	m_commandList->SetComputeRootSignature(m_computeMipMapsRootSignature.Get());

	ID3D12DescriptorHeap* heaps[] = { m_computeMipMapsHeap->Heap() };
	m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

	for (UINT env = 0; env < RHConfig::environmentsNumber; ++env)
	{
		ID3D12Resource* cube = m_environments[env].cubemap.Get();

		for (UINT i = 1; i < mipLevels; ++i)
		{
			const UINT srcMip = i - 1;

			DescriptorHandle srvHandle = m_computeMipMapsHeap->AllocatePersistent();
			DescriptorHandle uavHandle = m_computeMipMapsHeap->AllocatePersistent();

			D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
			srv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv.Texture2DArray.MostDetailedMip = srcMip;
			srv.Texture2DArray.MipLevels = 1;
			srv.Texture2DArray.ArraySize = 6;
			m_device->CreateShaderResourceView(cube, &srv, srvHandle.cpu);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
			uav.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uav.Texture2DArray.MipSlice = i;
			uav.Texture2DArray.ArraySize = 6;
			m_device->CreateUnorderedAccessView(cube, nullptr, &uav, uavHandle.cpu);

			CD3DX12_RESOURCE_BARRIER pre[6];
			for (UINT f = 0; f < 6; ++f)
			{
				UINT dstSub = D3D12CalcSubresource(i, f, 0, mipLevels, 6);
				pre[f] = CD3DX12_RESOURCE_BARRIER::Transition(cube, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, dstSub);
			}
			m_commandList->ResourceBarrier(6, pre);

			m_commandList->SetComputeRootDescriptorTable(0, srvHandle.gpu);

			UINT dstSize = baseSize >> i;
			UINT groups = (dstSize + 7) / 8;
			m_commandList->Dispatch(groups, groups, 6);

			CD3DX12_RESOURCE_BARRIER post[6];
			for (UINT f = 0; f < 6; ++f)
			{
				UINT dstSub = D3D12CalcSubresource(i, f, 0, mipLevels, 6);
				post[f] = CD3DX12_RESOURCE_BARRIER::Transition(cube, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, dstSub);
			}
			m_commandList->ResourceBarrier(6, post);
		}
		CD3DX12_RESOURCE_BARRIER toPS = CD3DX12_RESOURCE_BARRIER::Transition(cube, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_commandList->ResourceBarrier(1, &toPS);
	}
}

void Renderer::ChangeEnvironment()
{
	m_environmentIndex = (m_environmentIndex + 1) % RHConfig::environmentsNumber;
}
