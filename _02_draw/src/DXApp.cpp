#include"../include/DXApp.h"

DXApp* DXApp::m_app = nullptr;
DXApp::DXApp(HINSTANCE hInstance):m_hInstance(hInstance) {
	m_app = this;
}

void DXApp::Init() {
	InitWindow();
	InitDX12();
}

LRESULT CALLBACK WinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return DXApp::GetApp()->MsgProc(hwnd, uMsg, wParam, lParam);
}

LRESULT DXApp::MsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_SIZE:
		if (m_device) {
			m_width = LOWORD(lParam);
			m_height = HIWORD(lParam);
			Resize(m_width, m_height);
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DXApp::InitWindow() {
#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer();
			dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif
	//윈도우 클래스 등록
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WinProc; 
	wc.hInstance = m_hInstance;
	wc.lpszClassName = L"WindowClass";
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClassExW(&wc);

	//윈도우 생성
	m_hwnd = CreateWindowExW(
		0,
		wc.lpszClassName,
		L"DXApp",//window name
		WS_OVERLAPPEDWINDOW, // 창 스타일
		CW_USEDEFAULT, CW_USEDEFAULT, // 창 위치
		1600, 900,
		nullptr, nullptr, m_hInstance, nullptr
	);
	ShowWindow(m_hwnd, 5);
	UpdateWindow(m_hwnd);
}

void DXApp::InitDX12() {
	
	InitFactory();
	InitDevice();
	InitCommandQueue();
	InitCommandObjects();
	InitFence();

	Resize(1600, 900);
}	

void DXApp::InitFactory() {
	CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&m_factory));
}

void DXApp::InitDevice() {
	D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0, // feature level 설정 
		IID_PPV_ARGS(&m_device)
	);
}

void DXApp::InitCommandQueue() {
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 직접 명령 리스트
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
}

void DXApp::InitCommandObjects() {
	m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
	m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
	m_commandList->Close();
}

void DXApp::InitFence() {
	m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	m_fenceValue = 1;
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void DXApp::Resize(UINT width, UINT height) {
	if (width == 0 || height == 0) {
		return;
	}
	m_width = width;
	m_height = height;
	
	WaitforGPU(); //기다리기 //or flush
	ReleaseRenderTargets();// 기존 렌더 타겟 초기화

	if(m_swapChain) {
		m_swapChain->ResizeBuffers(FRAME_COUNT, m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	}
	else {
		// 처음이면 SwapChain 새로 생성
		InitSwapChain(m_width, m_height);
	}
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// RTV 다시 생성
	InitRTV();

	// Viewport, ScissorRect 갱신
	m_viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	m_scissorRect = { 0, 0, (LONG)width, (LONG)height };
}

void DXApp::ReleaseRenderTargets()
{
	for (UINT i = 0; i < FRAME_COUNT; i++)
		m_renderTargets[i].Reset();  // ComPtr Reset = Release
}

void DXApp::InitSwapChain(UINT width, UINT height) {
	DXGI_SWAP_CHAIN_DESC1 scDesc = {};
	scDesc.BufferCount = FRAME_COUNT;
	scDesc.Width = width;
	scDesc.Height = height;
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.SampleDesc.Count = 1;
	ComPtr<IDXGISwapChain1> swapChain1;
	m_factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),
		m_hwnd,
		&scDesc,
		nullptr, nullptr, &swapChain1
	);
	swapChain1.As(&m_swapChain);
}

void DXApp::InitRTV() {
	// RTV 힙이 없으면 생성 (최초 1회만)
	if (!m_rtvHeap)
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FRAME_COUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV
		);
	}
	
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < FRAME_COUNT; ++i) {
		m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += m_rtvDescriptorSize;
	}
}

void DXApp::WaitforGPU() {
	// GPU가 현재 프레임 작업을 완료했는지 확인
	const UINT64 fence = m_fenceValue;
	m_commandQueue->Signal(m_fence.Get(), fence);
	m_fenceValue++;
	if (m_fence->GetCompletedValue() < fence) {
		m_fence->SetEventOnCompletion(fence, m_fenceEvent);
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
}

void DXApp::Cleanup() {
	WaitforGPU();
	CloseHandle(m_fenceEvent);
}

void DXApp::BeginFrame() {
	// 명령 기록 준비
	m_commandAllocator->Reset();
	m_commandList->Reset(m_commandAllocator.Get(), nullptr);

	// Barrier: Present → RenderTarget
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_commandList->ResourceBarrier(1, &barrier);

	// 현재 프레임 RTV 핸들 계산
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
		m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += (SIZE_T)m_frameIndex * m_rtvDescriptorSize;

	// 화면 Clear
	m_commandList->ClearRenderTargetView(rtvHandle, m_clearColor, 0, nullptr);
}

void DXApp::EndFrame()
{
	// Barrier: RenderTarget → Present
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_commandList->ResourceBarrier(1, &barrier);

	// 명령 제출
	m_commandList->Close();
	ID3D12CommandList* lists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(lists), lists);

	// 화면 출력
	m_swapChain->Present(1, 0);

	// 다음 프레임 준비
	WaitforGPU();
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DXApp::Render() {
	BeginFrame();
	//draw call
	EndFrame();
}

void DXApp::Run() {
	MSG msg = {};
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			Render();
		}
	}
}