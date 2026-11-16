#pragma once
#include <Windows.h>
#include <cstdint>

class Window {
  public:
	void Initialize();

	// メッセージの処理
	bool ProcessMessage();

	// 終了処理
	void Finalize();

	HWND GetHwnd() const { return hwnd_; }
	HINSTANCE GetHInstance() const { return wc_.hInstance; }

	// クライアント領域のサイズ
	static inline const int32_t kClientWidth = 1280;
	static inline const int32_t kClientHeight = 720;
  private:
	// ウィンドウプロシージャ
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND hwnd_ = nullptr;

	// ウィンドウサイズを表す構造体
	WNDCLASS wc_{};

	// タイマー分解能の変更状態
	bool timerResolutionRequested_ = false;
};
