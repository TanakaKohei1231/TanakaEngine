#include "Window.h"
#include <cassert>

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // USE_IMGUI

void Window::Initialize() {
	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	// ウィンドウプロシージャ
	wc_.lpfnWndProc = WindowProc;

	// ウィンドウクラス名
	wc_.lpszClassName = L"CG2WindowClass";

	// インスタンスハンドル
	wc_.hInstance = GetModuleHandle(nullptr);

	// カーソル
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスを登録する
	ATOM registerClassResult = RegisterClass(&wc_);
	assert(registerClassResult != 0);

	RECT wrc{0, 0, kClientWidth, kClientHeight};

	// クライアント領域を元に実際のサイズにwrcを変更してもらう
	BOOL adjustWindowRectResult = AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
	assert(adjustWindowRectResult != FALSE);

	// ウィンドウの生成
	hwnd_ = CreateWindow(
		wc_.lpszClassName, // クラス名
		L"CG2", // タイトルバーの文字
		WS_OVERLAPPEDWINDOW, // ウィンドウスタイル
		CW_USEDEFAULT, // x座標
		CW_USEDEFAULT, // y座標
		wrc.right - wrc.left, // ウィンドウ幅
		wrc.bottom - wrc.top, // ウィンドウ高さ
		nullptr, // 親ウィンドウハンドル
		nullptr, // メニューハンドル
		wc_.hInstance, // インスタンスハンドル
		nullptr                 // オプション
	);

	assert(hwnd_ != nullptr);

	// ウィンドウを表示する
	ShowWindow(hwnd_, SW_SHOW);
}

bool Window::ProcessMessage() {
	MSG msg{};

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		// ウィンドウのバツを押されたらfalse
		if (msg.message == WM_QUIT) {
			return false;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return true;
}

void Window::Finalize() {
	if (hwnd_ != nullptr) {
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}

	UnregisterClass(wc_.lpszClassName, wc_.hInstance);
}

LRESULT Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

#ifdef USE_IMGUI

	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
		return true;
	}

#endif // USE_IMGUI

	switch (msg) {
		// ウィンドウが破棄された
	case WM_DESTROY:
		// OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wParam, lParam);
}