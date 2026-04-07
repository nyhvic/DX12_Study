#include"../include/DXApp.h"

DXApp* DXApp::g_app = nullptr;
DXApp::DXApp(HINSTANCE hInstance):g_hInstance(hInstance) {
	g_app = this;
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
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DXApp::InitWindow() {
	//윈도우 클래스 등록
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WinProc; 
	wc.hInstance = g_hInstance;
	wc.lpszClassName = L"WindowClass";
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClassExW(&wc);

	//윈도우 생성
	g_hwnd = CreateWindowExW(
		0,
		wc.lpszClassName,
		L"DXApp",//window name
		WS_OVERLAPPEDWINDOW, // 창 스타일
		CW_USEDEFAULT, CW_USEDEFAULT, // 창 위치
		1600, 900,
		nullptr, nullptr, g_hInstance, nullptr
	);
	ShowWindow(g_hwnd, 5);
	UpdateWindow(g_hwnd);
}

void DXApp::InitDX12() {
	/*
	InitFactory();
	InitDevice();
	InitCommandQueue();
	InitSwapChain(g_hwnd, 1600, 900);
	InitRTV();
	InitCommandObjects();
	InitFence();
	*/
}	
