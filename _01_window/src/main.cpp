#include<Windows.h>
#include<d3d12.h> // Direct3D 12 헤더
#include<dxgi1_6.h> // DXGI 헤더
#include<wrl/client.h> // Microsoft::WRL::ComPtr 사용을 위한 헤더

#pragma comment(lib, "d3d12.lib") // Direct3D 12 라이브러리 링크
#pragma comment(lib, "dxgi.lib") // DXGI 라이브러리 링크

using Microsoft::WRL::ComPtr;

constexpr UINT FRAME_COUNT = 2; // 프레임 버퍼 수

ComPtr<IDXGIFactory6> g_factory; // DXGI 팩토리, gpu, 디스플레이 어댑터를 관리한다네
ComPtr<ID3D12Device> g_device; //DX12 디바이스, gpu와 상호작용하는 인터페이스
ComPtr<ID3D12CommandQueue> g_commandQueue; // command queue
ComPtr<IDXGISwapChain3> g_swapChain; // swap chain
UINT g_frameIndex = 0; // 버퍼 인덱스
ComPtr<ID3D12DescriptorHeap> g_rtvHeap; // RTV 디스크립터 힙
UINT g_rtvDescriptorSize = 0; // RTV 디스크립터 크기
ComPtr<ID3D12Resource> g_renderTargets[FRAME_COUNT]; // RTV - 렌더 타겟 버퍼
ComPtr<ID3D12CommandAllocator> g_commandAllocator; // command allocator
ComPtr<ID3D12GraphicsCommandList> g_commandList; // command list
ComPtr<ID3D12Fence> g_fence; // fence
UINT64 g_fenceValue = 0; // fence value
HANDLE g_fenceEvent; // fence event


LRESULT CALLBACK WinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	/*
	event callback 함수
	*/
	switch (uMsg) {
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void InitDX12(HWND hwnd) {
	//dx12 초기화

	UINT dxgiFlags = 0;// 디버그 레이어 활성화, 개발 중엔 키는게 좋다네요

#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer();
			dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	/*
	전역변수 ComPtr로 관리할 객체 생성, Create~~ 메소드로 생성
	*/
	//DXGI factory
	CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&g_factory));
	//D3D12 device
	D3D12CreateDevice(
		nullptr, // 기본 어댑터 사용, 기본 gpu 사용하는듯
		D3D_FEATURE_LEVEL_11_0, // feature level 설정 
		IID_PPV_ARGS(&g_device)
	);

	//command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 직접 명령 리스트
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue));

	//swap chain
	DXGI_SWAP_CHAIN_DESC1 scDesc = {};
	scDesc.BufferCount = FRAME_COUNT; // 버퍼 수
	scDesc.Width = 1600; // 창 너비
	scDesc.Height = 900; // 창 높이
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 버퍼 포맷, RGBA 0~255
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 렌더 타겟으로 사용
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 스왑 효과, 버퍼 교체 방식, DX12가 권장한다네요
	scDesc.SampleDesc.Count = 1; // 멀티샘플링 설정, 1이면 멀티샘플링 사용 안함, 나중에 바꿔야징
	ComPtr<IDXGISwapChain1> swapChain1;
	g_factory->CreateSwapChainForHwnd(
		g_commandQueue.Get(), // 명령 큐
		hwnd, // 창 핸들
		&scDesc, // 스왑 체인 설명
		nullptr, nullptr, &swapChain1
	);
	swapChain1.As(&g_swapChain); // IDXGISwapChain3 인터페이스로 변환, IDXGISwapChain3는 IDXGISwapChain1의 확장 버전, 최신 기능 사용 가능(get current back buffer index)
	g_frameIndex = g_swapChain->GetCurrentBackBufferIndex(); // 현재 버퍼 인덱스 가져오기

	//RTV Descriptor Heap 생성, rtv에 대한 descriptor
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = FRAME_COUNT; // RTV 수, 프레임 버퍼 수와 같게
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap));
	g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); // RTV 디스크립터 크기

	//RTV - 렌더링 결과 저장하는 버퍼
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_rtvHeap->GetCPUDescriptorHandleForHeapStart()); // RTV 힙의 시작 핸들
	for (UINT i = 0; i < FRAME_COUNT; ++i) {
		g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i])); // 스왑 체인에서 버퍼 가져오기
		g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, rtvHandle); // RTV 생성
		rtvHandle.ptr += g_rtvDescriptorSize; // 다음 RTV 디스크립터 위치로 이동
	}

	//Command allocator - 명령들 실제 저장
	//Command list - Allocator에 명령 저장, submit
	g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator));
	g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_commandList));
	g_commandList->Close(); // 생성 시 열린 상태, 닫음
	//command list 순서 - WaitForPreviousFrame -> Reset -> Record commands -> Close -> Execute(submit) -> Present

	//fence
	g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
	g_fenceValue = 1; // 초기 펜스 값
	g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr); // gpu 작업 완료 이벤트 핸들러
	
}

void WaitForGPU() {
	g_commandQueue->Signal(g_fence.Get(), g_fenceValue); // 현재 펜스 값으로 시그널, gpu가 이 시점까지 작업 완료하면 펜스 값이 증가

	if (g_fence->GetCompletedValue() < g_fenceValue) { // gpu가 아직 작업 완료 안했으면
		g_fence->SetEventOnCompletion(g_fenceValue, g_fenceEvent); // 펜스 값이 될 때 이벤트 발생
		WaitForSingleObject(g_fenceEvent, INFINITE); // 이벤트 대기
	}
	g_fenceValue++; // 다음 프레임을 위해 펜스 값 증가
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow) {
	/*
	windows api 창
	*/
	//윈도우 클래스 등록
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WinProc; //callback
	wc.hInstance = hInstance;
	wc.lpszClassName = L"WindowClass";
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClassExW(&wc);

	//윈도우 생성
	HWND hwnd = CreateWindowExW(
		0,
		wc.lpszClassName,
		L"Hello, Window!",//window name
		WS_OVERLAPPEDWINDOW, // 창 스타일
		CW_USEDEFAULT, CW_USEDEFAULT, // 창 위치
		1600, 900,
		nullptr, nullptr, hInstance, nullptr
	);	

	ShowWindow(hwnd, nCmdShow);

	InitDX12(hwnd); // DirectX 12 초기화

	//메시지 루프
	MSG msg = {};

	while (true) {
		if(PeekMessage(&msg, nullptr, 0,0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			continue;
		}
	}

	return 0;
}

void Render() {
	//command list reset
	g_commandAllocator->Reset();
	g_commandList->Reset(g_commandAllocator.Get(), nullptr);

	//barrier 설정, Present(그릴 수 있는 상태)에서 Render target으로 전환
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = g_renderTargets[g_frameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	g_commandList->ResourceBarrier(1, &barrier);

	// RTV
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	rtvHandle.ptr += g_frameIndex * g_rtvDescriptorSize; // 현재 버퍼 RTV 핸들
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f }; // RGBA
	g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr); // rtv 한 색으로 클리어

	//barrier render target to present
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	g_commandList->ResourceBarrier(1, &barrier);

	//command close, submit
	g_commandList->Close();
	ID3D12CommandList* lists[] = { g_commandList.Get() };
	g_commandQueue->ExecuteCommandLists(1, lists); // 명령 리스트 실행
	
	//출력
	g_swapChain->Present(1, 0); // 화면에 표시, 1은 수직 동기화, 0은 옵션

	//GPU 작업 대기
	WaitForGPU();
	g_frameIndex = g_swapChain->GetCurrentBackBufferIndex(); // 다음 프레임 버퍼 인덱스 가져오기
}