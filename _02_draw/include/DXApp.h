#pragma once

#include<windows.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<wrl/client.h>
#include<DirectXMath.h>
#include<d3dcompiler.h>
#include<vector>
#include<array>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;	
using namespace DirectX;

struct Vertex {
	XMFLOAT3 pos;
	XMFLOAT4 color;
};

constexpr UINT FRAME_COUNT = 2;	

class DXApp {
public:
	DXApp(HINSTANCE hInstance);
	void Init();
	void Run();

	void WaitforGPU();
	void Cleanup();

	LRESULT MsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	ID3D12Device* GetDevice()      const { return m_device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }
	//D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle()   const;
	const D3D12_VIEWPORT& GetViewport()    const { return m_viewport; }
	const D3D12_RECT& GetScissorRect() const { return m_scissorRect; }
	static DXApp* GetApp() { return m_app; }

	UINT dxgiFlags = 0;

protected:
	void InitWindow();
	void InitDX12();

	void InitFactory();
	void InitDevice();
	void InitCommandQueue();
	void InitCommandObjects();
	void InitFence();

	void ReleaseRenderTargets();
	void InitSwapChain(UINT width, UINT height);
	void InitRTV();
	void Resize(UINT width, UINT height);

	void InitRootSignature();
	void InitShader();
	void InitTriangle();
	void InitPSO();

	void BeginFrame();
	void EndFrame();
	void Render();


	UINT m_width;
	UINT m_height;

	D3D12_VIEWPORT m_viewport = {};
	D3D12_RECT     m_scissorRect = {};

	static DXApp* m_app;

	HWND m_hwnd = nullptr;
	HINSTANCE m_hInstance = nullptr;

	ComPtr<IDXGIFactory6>               m_factory;
	ComPtr<ID3D12Device>                m_device;
	ComPtr<ID3D12CommandQueue>          m_commandQueue;
	ComPtr<IDXGISwapChain3>             m_swapChain;

	ComPtr<ID3D12DescriptorHeap>        m_rtvHeap;
	UINT                                m_rtvDescriptorSize = 0;
	ComPtr<ID3D12Resource>              m_renderTargets[FRAME_COUNT];

	ComPtr<ID3D12CommandAllocator>      m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList>   m_commandList;

	ComPtr<ID3D12Fence>                 m_fence;
	UINT64                              m_fenceValue = 0;
	HANDLE                              m_fenceEvent = nullptr;
	UINT                                m_frameIndex = 0;
	float                               m_clearColor[4] = { 0.2f, 0.2f, 0.2f, 0.0f };

	ComPtr<ID3D12RootSignature> m_rootSignature;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
	ComPtr<ID3D12PipelineState> m_pipelineState; //PSO
	ComPtr<ID3D12Resource>      m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW    m_vertexBufferView = {};
	ComPtr<ID3D12Resource>      m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW     m_indexBufferView = {};
	UINT m_indexCount = 0;

	ComPtr<ID3DBlob> m_vsBlob;
	ComPtr<ID3DBlob> m_psBlob;
};