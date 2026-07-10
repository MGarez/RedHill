#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12/d3dx12.h>
#include <dxcapi.h>
#include <DirectXMath.h>

#include <wrl.h>

#include <memory>
#include <string>

#include "Config.h"
#include "DescriptorHeapAllocator.h"
#include "Model.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct ConstantFrameObject
{
	XMFLOAT4X4 mvp;
	XMFLOAT4X4 model;
	XMFLOAT4X4 invViewProj;
	XMFLOAT4X4 lightViewProj;
	XMFLOAT3 cameraPos;
	float screenWidth;
	XMFLOAT3 lightPos;
	float screenHeight;
	int32_t castsShadows;
	float shadowMapSize;
};

struct ConstantBuffer
{
	ComPtr<ID3D12Resource> resource;
	UINT8* data;
};

struct EnvironmentSet
{
	ComPtr<ID3D12Resource> equirect;
	ComPtr<ID3D12Resource> equirectUploader;
	ComPtr<ID3D12Resource> cubemap;
	ComPtr<ID3D12Resource> irradiance;
	ComPtr<ID3D12Resource> prefilter;

	//  Srv handles for the environment maps
	DescriptorHandle equirectSrvHandle;
	DescriptorHandle cubemapSrvHandle;
	DescriptorHandle irradianceSrvHandle;
	DescriptorHandle prefilterSrvHandle;

	std::string path;
};

enum class SceneMode
{
	Object,
	SphereGrid
};

class Renderer
{
public:
	Renderer(HWND& hwnd);
	~Renderer() = default;

	void Init();
	void Update(const struct Camera& camera);
	void Render();
	void Destroy();

	void ChangeSceneMode();
	void ChangeEnvironment();

private:
	void InitPipeline();
	void InitAssets();
	void PopulateCommandList();
	void MoveToNextFrame();
	void WaitForGpu();

	void LoadAssets();
	void SetupShadowPass();
	void SetupGeometryPass();
	void SetupFloorGeometry();
	void SetupObjectModeGeometry();
	void SetupSphereGridGeometry();
	void SetupLightPass();
	void SetupConstantBuffers();
	void SetupEnvironments();

	void RecordShadowPass();

	void DrawFloor();
	void DrawObject();
	void DrawSphereGrid();

	ComPtr<ID3D12RootSignature> BuildNoTextureGeoRootSignature();
	ComPtr<ID3D12PipelineState> BuildNoTextureGeoPSO(ID3D12RootSignature* rootSig, const wchar_t* shaderPath);

	void ConfigureRenderTarget(ComPtr<ID3D12Resource>& rtResource, const DXGI_FORMAT format, const D3D12_RESOURCE_STATES& state, const CD3DX12_CPU_DESCRIPTOR_HANDLE& rtvHandle, const CD3DX12_CPU_DESCRIPTOR_HANDLE& srvHandle);
	ComPtr<ID3D12Resource> CreateDefaultResource(ComPtr<ID3D12Resource>& uploadResource, const CD3DX12_RESOURCE_DESC& resourceDesc, const D3D12_SUBRESOURCE_DATA& resourceData, const D3D12_RESOURCE_STATES& initialState, const D3D12_RESOURCE_STATES& finalState);
	ComPtr<ID3D12Resource> CreateTextureFromFile(ComPtr<ID3D12Resource>& uploadResource, const std::string& textureFile, const DXGI_FORMAT format, const CD3DX12_CPU_DESCRIPTOR_HANDLE& srvHandle);
	ComPtr<ID3D12Resource> CreateHDRTextureFromFile(ComPtr<ID3D12Resource>& uploadResource, const std::string& textureFile, const CD3DX12_CPU_DESCRIPTOR_HANDLE& srvHandle);

	ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const std::wstring& entryPoint, const std::wstring& target) const;

	void BakeEnvironmentCubemap();
	void BakeIrradianceMap();
	void BakePrefilterMap();

	void BeginBakePass(ID3D12PipelineState* pso, ID3D12RootSignature* rootSig);
	void SetFaceBasis(UINT face);
	ComPtr<ID3D12PipelineState> BuildBakePSO(ID3D12RootSignature* rootSig, const wchar_t* shaderPath);
	void RenderCubemapFaces(ID3D12Resource* target, UINT mip, UINT faceSize);
	void BakeBrdfLut();

	void GenerateMipMaps(const UINT baseSize,const UINT mipLevels);

private:
	// Pipeline objects.

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<IDXGISwapChain3> m_swapchain;

	std::unique_ptr<DescriptorHeapAllocator> m_rtvHeap;
	ComPtr<ID3D12Resource> m_renderTargets[RHConfig::frameNumber];

	ComPtr<ID3D12Resource> m_albedoRT;
	ComPtr<ID3D12Resource> m_normalRT;
	ComPtr<ID3D12Resource> m_materialRT;

	std::unique_ptr<DescriptorHeapAllocator> m_dsvHeap;
	ComPtr<ID3D12Resource> m_depthStencil;

	std::unique_ptr<DescriptorHeapAllocator> m_srvHeap;

	ComPtr<ID3D12RootSignature> m_skyRootSignature;
	ComPtr<ID3D12PipelineState> m_skyPSO;

	ComPtr<ID3D12RootSignature> m_bakeRootSignature;
	ComPtr<ID3D12PipelineState> m_bakePSO;

	ComPtr<ID3D12RootSignature> m_irradianceRootSignature;
	ComPtr<ID3D12PipelineState> m_irradiancePSO;

	ComPtr<ID3D12RootSignature> m_prefilterRootSignature;
	ComPtr<ID3D12PipelineState> m_prefilterPSO;

	ComPtr<ID3D12RootSignature> m_computeMipMapsRootSignature;
	ComPtr<ID3D12PipelineState> m_computeMipMapsPSO;
	std::unique_ptr<DescriptorHeapAllocator> m_computeMipMapsHeap; // We could have this in the general srv heap but we will keep it separate for clarity

	EnvironmentSet m_environments[RHConfig::environmentsNumber];
	UINT m_environmentIndex;

	ComPtr<ID3D12CommandAllocator> m_commandAllocator[RHConfig::frameNumber];
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	ConstantBuffer m_constantBuffers[RHConfig::frameNumber];

	ComPtr<ID3D12RootSignature> m_geoObjectRootSignature;
	ComPtr<ID3D12PipelineState> m_geoObjectPSO;

	ComPtr<ID3D12RootSignature> m_geoSphereRootSignature;
	ComPtr<ID3D12PipelineState> m_geoSpherePSO;

	ComPtr<ID3D12RootSignature> m_lightRootSignature;
	ComPtr<ID3D12PipelineState> m_lightPSO;

	ComPtr<ID3D12RootSignature> m_floorRootSignature;
	ComPtr<ID3D12PipelineState> m_floorPSO;

	ComPtr<ID3D12Resource> m_brdfLUT;
	ComPtr<ID3D12RootSignature> m_lutRootSignature;
	ComPtr<ID3D12PipelineState> m_lutPSO;

	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[RHConfig::frameNumber];

	std::unique_ptr<struct PBRMesh> m_object;
	std::unique_ptr<struct PBRMesh> m_sphereGrid;
	std::unique_ptr<struct PBRMesh> m_floor;

	ComPtr<IDxcUtils> m_utils;
	ComPtr<IDxcCompiler3> m_shaderCompiler;

	ComPtr<ID3D12Resource> m_shadowMap;

	ComPtr<ID3D12RootSignature> m_shadowRootSignature;
	ComPtr<ID3D12PipelineState> m_shadowPSO;

	HWND m_hWnd;

	SceneMode m_sceneMode;

	// Global handles
	DescriptorHandle m_backbufferHandles[RHConfig::frameNumber];

	DescriptorHandle m_albedoRtvHandle;
	DescriptorHandle m_albedoSrvHandle;

	DescriptorHandle m_normalRtvHandle;
	DescriptorHandle m_normalSrvHandle;

	DescriptorHandle m_materialRtvHandle;
	DescriptorHandle m_materialSrvHandle;

	DescriptorHandle m_depthDsvHandle;
	DescriptorHandle m_depthSrvHandle;

	DescriptorHandle m_brdfLutSrvHandle;

	DescriptorHandle m_shadowSrvHandle;
	DescriptorHandle m_shadowDsvHandle;
};
