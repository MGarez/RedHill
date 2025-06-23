#pragma once

#include "DXSample.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class D3D12App : public DXSample
{
public:
    D3D12App(UINT width, UINT height, std::wstring name);

    virtual void OnInit() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual void OnDestroy() override;

private:

    static const UINT FrameCount = 2;

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
    };

	struct SceneConstantBuffer
	{
		XMFLOAT4 offset;
		float padding[60]; // Padding so the constant buffer is 256-byte aligned.
	};
	static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");



    // Pipeline objects.

    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;

    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_bundleAllocator[FrameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12GraphicsCommandList> m_bundle;
    UINT m_rtvDescriptorSize;

    // App resources
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_constantBuffer;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FrameCount];

    void InitPipeline();
    void InitAssets();
    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGpu();
};
