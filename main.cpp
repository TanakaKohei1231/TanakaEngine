#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>
#include<chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// クライアント領域のサイズ
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;



// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

// ログファイル作成関数
std::ofstream CreateLogFile() {
	// ログのディレクトリを用意
	std::filesystem::create_directory("logs");

	// 現在時刻の取得(UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	// コンマ秒を削る
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

	// 日本時間に変換
	std::chrono::zoned_time localTime{std::chrono::current_zone(), nowSeconds};

	// formatを使って年月日_時分秒の文字列に変換
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);

	// 時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + dateString + ".log";

	// ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);

	return logStream;
}


// デバッグ用のログ出力関数
void Log(std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}

// stringからwstringへの変換関数
std::wstring ConvertString(const std::string &str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]), static_cast<int>(str.size()), NULL, 0);

	if (sizeNeeded == 0) {
		return std::wstring();
	}

	std::wstring result(sizeNeeded, 0);

	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);

	return result;
}

// wstringからstringへの変換関数
std::string ConvertString(const std::wstring &str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);

	if (sizeNeeded == 0) {
		return std::string();
	}

	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);

	return result;
}


// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// ログファイルの作成
	std::ofstream log = CreateLogFile();

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	WNDCLASS wc{};

	// ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;

	// ウィンドウクラス名
	wc.lpszClassName = L"CG2WindowClass";

	// インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);

	// カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスを登録する
	RegisterClass(&wc);

	RECT wrc{0, 0, kClientWidth, kClientHeight};

	// クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName, // クラス名
		L"CG2", // タイトルバーの文字
		WS_OVERLAPPEDWINDOW, // ウィンドウスタイル
		CW_USEDEFAULT, // x座標
		CW_USEDEFAULT, // y座標
		wrc.right - wrc.left, // ウィンドウ幅
		wrc.bottom - wrc.top, // ウィンドウ高さ
		nullptr, // 親ウィンドウハンドル
		nullptr, // メニューハンドル
		wc.hInstance, // インスタンスハンドル
		nullptr                 // オプション
	);

	// ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

	MSG msg{};

	// DXGIファクトリーの生成
	IDXGIFactory7 *dxgiFactory = nullptr;

	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	// 使用するアダプタ用の変数
	IDXGIAdapter4 *useAdapter = nullptr;

	// アダプタの列挙良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {
		// アダプタの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		// ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			// 採用したアダプタの名前をログに出す
			Log(log, ConvertString(std::format(L"Use Adapter: {}\n", adapterDesc.Description)));

			break;
		}

		// ソフトウェアアダプタは見なかったことにする
		useAdapter = nullptr;
	}

	// アダプタが見つからなかったらエラー
	assert(useAdapter != nullptr);

	ID3D12Device *device = nullptr;

	// 機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
	};

	const char* featureLevelStrings[] = {
		"12.2",
		"12.1",
		"12.0",
	};

	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));

		// 指定した機能レベルで生成できたかを確認
		if (SUCCEEDED(hr)) {
			// 生成できたのでログ出力を行ってループを抜ける
			Log(log, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}

	// デバイスが生成できなかったらエラー
	assert(device != nullptr);

	// デバイスの生成に成功したのでログに出す
	Log(log, "Complete create D3D12Device!!\n");


	// ===========================================================
	// メインループ
	// ===========================================================

	// ウィンドウのバツボタンが押されるまでループ
	while (msg.message != WM_QUIT) {
		// ウィンドウにメッセージが来ていたら最優先で処理する
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			// ゲームの処理
		}
	}

	Log(log, ConvertString(std::format(L"\nlog\n")));

	return 0;
}