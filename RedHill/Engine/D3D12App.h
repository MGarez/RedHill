#pragma once

#include "DXSample.h"
#include "MathUtils.h"

#include <memory>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct Mesh
{
    ComPtr<ID3DBlob> vBufferData = nullptr;
    ComPtr<ID3D12Resource> vBuffer = nullptr;
    ComPtr<ID3D12Resource> vBufferUplader = nullptr;

    UINT vBufferSize = 0;
    UINT vBufferStride = 0;

    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const 
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = vBuffer->GetGPUVirtualAddress();
        vbv.SizeInBytes = vBufferSize;
        vbv.StrideInBytes = vBufferStride;

        return vbv;
    }
};

struct CameraPosition
{
    float theta;
    float phi;
    float radius;
};

class D3D12App : public DXSample
{
public:
    D3D12App(UINT width, UINT height, std::wstring name);

    virtual void OnInit() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual void OnDestroy() override;

	virtual void OnMouseButtonDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseButtonUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

private:

    static const UINT FrameCount = 2;

    struct Vertex
    {
        XMFLOAT4 position;
        XMFLOAT4 color;
    };

	struct ObjectCB
	{
		XMFLOAT4X4 mvp = RHMath::Identity4x4();
		float padding[48]; // Padding so the constant buffer is 256-byte aligned.
	};
	static_assert((sizeof(ObjectCB) % 256) == 0, "Constant Buffer size must be 256-byte aligned");



    // Pipeline objects.

    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;

    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12Resource> m_depthStencil;

    ComPtr<ID3D12CommandAllocator> m_commandAllocator[FrameCount];
    //ComPtr<ID3D12CommandAllocator> m_bundleAllocator[FrameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    //ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    //ComPtr<ID3D12GraphicsCommandList> m_bundle;
    UINT m_rtvDescriptorSize;

    // App resources

    std::unique_ptr<Mesh> m_mesh = nullptr;

    ObjectCB* m_mappedConstantData;
    D3D12_GPU_VIRTUAL_ADDRESS m_constantDataGPUAddr;
    ComPtr<ID3D12Resource> m_constantBuffer;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FrameCount];

    POINT m_lastMousePosition;
    CameraPosition m_camera;

    void InitPipeline();
    void InitAssets();
    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGpu();

    // Helper function to create a buffer in the default heap
    ComPtr<ID3D12Resource> CreateDefaultBuffer(const void* data, ComPtr<ID3D12Resource>& uploadBuffer, UINT size);
};
