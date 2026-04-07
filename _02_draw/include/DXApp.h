#pragma once

#include<windows.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;	

constexpr UINT FRAME_COUNT = 2;	

class DXApp {
public:
	DXApp(HINSTANCE hInstance);
	void Init();
	void BeginFrame();
	void EndFrame();

	void WaitforGPU();
	void Cleanup();

	LRESULT MsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	ID3D12Device* GetDevice()      const { return g_device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return g_commandList.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle()   const;
	static DXApp* GetApp() { return g_app; }

protected:
	void InitWindow();
	void InitDX12();
	void InitFactory();
	void InitDevice();
	void InitCommandQueue();
	void InitSwapChain(HWND hwnd, UINT width, UINT height);
	void InitRTV();
	void InitCommandObjects();
	void InitFence();

	static DXApp* g_app;

	HWND g_hwnd = nullptr;
	HINSTANCE g_hInstance = nullptr;

	ComPtr<IDXGIFactory6>               g_factory;
	ComPtr<ID3D12Device>                g_device;
	ComPtr<ID3D12CommandQueue>          g_commandQueue;
	ComPtr<IDXGISwapChain3>             g_swapChain;

	ComPtr<ID3D12DescriptorHeap>        g_rtvHeap;
	UINT                                g_rtvDescriptorSize = 0;
	ComPtr<ID3D12Resource>              g_renderTargets[FRAME_COUNT];

	ComPtr<ID3D12CommandAllocator>      g_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList>   g_commandList;

	ComPtr<ID3D12Fence>                 g_fence;
	UINT64                              g_fenceValue = 0;
	HANDLE                              g_fenceEvent = nullptr;

	UINT                                g_frameIndex = 0;
	float                               g_clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};