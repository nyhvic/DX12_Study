#include"../include/DXApp.h"
#include"../include/DXUtil.h"

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

	InitRootSignature();
	InitShader();
	InitTriangle();
	InitPSO();
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
	// command queue flush
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
	m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get());

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

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

	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	// RTV 설정
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
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
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->IASetIndexBuffer(&m_indexBufferView);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);
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

void DXApp::InitRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
	m_device->CreateRootSignature(
		0,
		signature->GetBufferPointer(),
		signature->GetBufferSize(),
		IID_PPV_ARGS(&m_rootSignature)
	);
}

void DXApp::InitShader() {
	UINT compileFlags = 0;
	ComPtr<ID3DBlob> error;
#if defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	m_vsBlob = DXUtil::CompileShader(L"shaders/vertex.hlsl", nullptr, "VSMain", "vs_5_0");
	m_psBlob = DXUtil::CompileShader(L"shaders/pixel.hlsl", nullptr, "PSMain", "ps_5_0");

	m_inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void DXApp::InitTriangle() {
	std::array<Vertex,3> vertices =
	{
		Vertex{ XMFLOAT3{ 0.0f, 0.5f, 0.0f }, XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f } },
		Vertex{ XMFLOAT3{ 0.5f, -0.5f, 0.0f }, XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f } },
		Vertex{ XMFLOAT3{ -0.5f, -0.5f, 0.0f }, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f } }
	};
	
	std::array<UINT, 3> indices = { 0,1,2 };

	const UINT vertexBufferSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT indexBufferSize = (UINT)indices.size() * sizeof(UINT);

	//추후 이과정 묶어서 Create DefaultBuffer 같은 함수로 만들기
	D3D12_HEAP_PROPERTIES vertexHeapProps = {};
	vertexHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC vertexResDesc = {};
	vertexResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResDesc.Width = vertexBufferSize;
	vertexResDesc.Height = 1;
	vertexResDesc.DepthOrArraySize = 1;
	vertexResDesc.MipLevels = 1;
	vertexResDesc.Format = DXGI_FORMAT_UNKNOWN;
	vertexResDesc.SampleDesc.Count = 1;
	vertexResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	m_device->CreateCommittedResource(
		&vertexHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&vertexResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer)
	);

	// CPU → GPU 메모리로 버텍스 데이터 복사
	void* mappedVertexData = nullptr;
	m_vertexBuffer->Map(0, nullptr, &mappedVertexData);
	memcpy(mappedVertexData, vertices.data(), vertexBufferSize);
	m_vertexBuffer->Unmap(0, nullptr);

	// Vertex Buffer View 설정
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = vertexBufferSize;
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);

	D3D12_HEAP_PROPERTIES indexHeapProps = {};
	indexHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC indexResDesc = {};
	indexResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	indexResDesc.Width = indexBufferSize;
	indexResDesc.Height = 1;
	indexResDesc.DepthOrArraySize = 1;
	indexResDesc.MipLevels = 1;
	indexResDesc.Format = DXGI_FORMAT_UNKNOWN;
	indexResDesc.SampleDesc.Count = 1;
	indexResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	m_device->CreateCommittedResource(
		&indexHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&indexResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer)
	);

	void* mappedIndexData = nullptr;
	m_indexBuffer->Map(0, nullptr, &mappedIndexData);
	memcpy(mappedIndexData, indices.data(), indexBufferSize);
	m_indexBuffer->Unmap(0, nullptr);

	// Index Buffer View 설정
	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.SizeInBytes = indexBufferSize;
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}

void DXApp::InitPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = { m_vsBlob->GetBufferPointer(), m_vsBlob->GetBufferSize() };
	psoDesc.PS = { m_psBlob->GetBufferPointer(),  m_psBlob->GetBufferSize() };
	psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.NumRenderTargets = 1;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = UINT_MAX;

	// 래스터라이저 기본값
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.RasterizerState.DepthClipEnable = TRUE;

	// 블렌드 기본값
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
}