#include "DirectXCore.h"
#include "engine/utility/Logger.h"
#include "engine/utility/StringUtility.h"
#include "engine/platform/Window.h"
#include <cassert>
#include <format>
#include <stdexcept>
#include <thread>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // USE_IMGUI

using namespace Microsoft::WRL;
using namespace Logger;
using namespace StringUtility;

void DirectXCore::Initialize(std::ostream &log, Window *window) {
	// FPS固定処理の初期化
	InitializeFixFPS();

	// 引数をメンバ変数にコピー
	log_ = &log;
	window_ = window;

	InitializeDevice();
	InitializeInfoQueue();
	InitializeCommand();
	InitializeSwapChain();
	InitializeDescriptorHeaps();
	InitializeRenderTargetView();
	InitializeDepthBuffer();
	InitializeFence();
	InitializeDXC();
	InitializeViewportAndScissor();
	InitializeImGui();
}

DirectXCore::~DirectXCore() {

	// フェンス用イベントを解放
	if (fenceEvent_ != nullptr) {
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}
}

void DirectXCore::PreDraw() {

	// 描画対象のバックバッファ番号を取得
	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	// バックバッファを描画可能な状態に変更
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	commandList_->ResourceBarrier(1, &barrier);

	// RTVとDSVのハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHandles_[backBufferIndex];
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	// 描画先を設定
	commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// 画面全体をクリア
	float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
	commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// 深度バッファをクリア
	commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// 描画用ディスクリプタヒープの設定
	ID3D12DescriptorHeap *descriptorHeaps[] = {srvDescriptorHeap_.Get()};
	commandList_->SetDescriptorHeaps(1, descriptorHeaps);

	// ビューポートの設定
	commandList_->RSSetViewports(1, &viewport_);

	// シザリング矩形の設定
	commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCore::PostDraw() {

	// 描画対象のバックバッファ番号を取得
	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	// バックバッファを表示可能な状態に変更
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	commandList_->ResourceBarrier(1, &barrier);

	// コマンドリストの記録を終了
	HRESULT hr = commandList_->Close();
	assert(SUCCEEDED(hr));

	// GPUにコマンドを実行させる
	ID3D12CommandList *commandLists[] = {commandList_.Get()};
	commandQueue_->ExecuteCommandLists(1, commandLists);

	// VSyncの設定（0: 無効、1: 有効）
	constexpr UINT kSyncInterval = 0;

	// 画面を表示する
	hr = swapChain_->Present(kSyncInterval, 0);
	assert(SUCCEEDED(hr));

	// GPUの処理完了を待機
	WaitForGPU();

	// VSync無効時のみ自前でFPSを制御
	if (kSyncInterval == 0) {
		UpdateFixFPS();
	}

	// 次フレームに向けてコマンドアロケータをリセット
	hr = commandAllocator_->Reset();
	assert(SUCCEEDED(hr));

	// 次フレームに向けてコマンドリストをリセット
	hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
	assert(SUCCEEDED(hr));
}

void DirectXCore::InitializeDevice() {

#ifdef _DEBUG

	ComPtr<ID3D12Debug1> debugController;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
		// デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();

		// さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}

#endif // _DEBUG

	// DXGIファクトリーの生成
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(dxgiFactory_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// 使用するアダプタ用の変数
	ComPtr<IDXGIAdapter4> useAdapter;

	// アダプタの列挙良い順にアダプタを頼む
	for (UINT i = 0;; ++i) {

		hr = dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(useAdapter.ReleaseAndGetAddressOf()));

		// 列挙終了
		if (hr == DXGI_ERROR_NOT_FOUND) {
			break;
		}

		// 列挙失敗
		if (FAILED(hr)) {
			throw std::runtime_error("Failed to enumerate adapter.");
		}

		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);

		if (FAILED(hr)) {
			throw std::runtime_error("Failed to get adapter description.");
		}

		// ソフトウェアアダプタ以外を採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Log(*log_, ConvertString(std::format(L"Use Adapter: {}\n", adapterDesc.Description)));
			break;
		}

		useAdapter.Reset();
	}

	// アダプタが見つからなかったらエラー
	assert(useAdapter != nullptr);

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
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(device_.GetAddressOf()));

		// 指定した機能レベルで生成できたかを確認
		if (SUCCEEDED(hr)) {
			// 生成できたのでログ出力を行ってループを抜ける
			Log(*log_, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}

	// デバイスが生成できなかったらエラー
	assert(device_ != nullptr);

	// デバイスの生成に成功したのでログに出す
	Log(*log_, "Complete create D3D12Device!!");
}

void DirectXCore::InitializeInfoQueue() {
#ifdef _DEBUG

	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;

	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
		// ヤバいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相性作用バグによるエラーメッセージ
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);
	}

	#endif // _DEBUG
}

void DirectXCore::InitializeCommand() {
	// コマンドキューの生成
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	HRESULT hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue_.GetAddressOf()));

	// コマンドキューの生成がうまくいかなかったら起動しない
	assert(SUCCEEDED(hr));

	// コマンドアロケータを生成する
	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator_.GetAddressOf()));

	// コマンドアロケータの生成がうまくいかなかったら起動しない
	assert(SUCCEEDED(hr));

	// コマンドリストを生成する
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(commandList_.GetAddressOf()));

	// コマンドリストの生成がうまくいかなかったら起動しない
	assert(SUCCEEDED(hr));

	hr = commandList_->Close();
	assert(SUCCEEDED(hr));
}

void DirectXCore::InitializeSwapChain() {
	// スワップチェーンを生成する
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = Window::kClientWidth;        // 画面の幅。ウィンドウのクライアント領域の幅と同じにする
	swapChainDesc.Height = Window::kClientHeight;      // 画面の高さ。ウィンドウのクライアント領域の高さと同じにする
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
	swapChainDesc.SampleDesc.Count = 1;                // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2;                               // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;    // モニタに映したら中身を破棄する

	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	ComPtr<IDXGISwapChain1> swapChain1;
	HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), window_->GetHwnd(), &swapChainDesc, nullptr, nullptr, swapChain1.GetAddressOf());
	assert(SUCCEEDED(hr));

	hr = swapChain1.As(&swapChain_);
	assert(SUCCEEDED(hr));
}

void DirectXCore::InitializeDescriptorHeaps() {
	// ディスクリプタヒープの生成
	// RTV用のヒープでディスクリプタの数は2。SRVはshader内で触るものではないので、shaderVisibleはfalse
	rtvDescriptorHeap_ = CreateDescriptorHeap(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// SRV用のヒープでディスクリプタの数は128。SRVはshader内で触るものなので、shaderVisibleはtrue
	srvDescriptorHeap_ = CreateDescriptorHeap(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、shaderVisibleはfalse
	dsvDescriptorHeap_ = CreateDescriptorHeap(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// DescriptorSizeを取得
	descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DirectXCore::InitializeRenderTargetView() {
	// スワップチェーンからリソースを引っ張ってくる
	HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(swapChainResources_[0].GetAddressOf()));

	// うまくいかなかったら起動しない
	assert(SUCCEEDED(hr));

	hr = swapChain_->GetBuffer(1, IID_PPV_ARGS(swapChainResources_[1].GetAddressOf()));
	assert(SUCCEEDED(hr));

	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;      // 出力結果をsRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む

	// ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	// RTVを2つ作るのでディスクリプタを2つ用意
	rtvHandles_[0] = rtvStartHandle;
	device_->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc, rtvHandles_[0]);

	// 2つ目のディスクリプタハンドルを得る
	rtvHandles_[1] = {rtvHandles_[0].ptr + device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)};

	// 2つ目を作る
	device_->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc, rtvHandles_[1]);
}

void DirectXCore::InitializeDepthBuffer() {
	depthStencilResource_ = CreateDepthStencilTextureResource(device_, Window::kClientWidth, Window::kClientHeight);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	// DSVHeapの先頭にDSVを作る
	device_->CreateDepthStencilView(
		depthStencilResource_.Get(),
		&dsvDesc,dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart()
	);
}

void DirectXCore::InitializeFence() {

	// 初期値0でFenceを作る
	fenceValue_ = 0;

	HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// FenceのSignalを待つためのイベントを作成する
	fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent_ != nullptr);
}

void DirectXCore::InitializeDXC() {

	// DXCのユーティリティを生成
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// DXCコンパイラを生成
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxcCompiler_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// IncludeHandlerを生成
	hr = dxcUtils_->CreateDefaultIncludeHandler(includeHandler_.GetAddressOf());
	assert(SUCCEEDED(hr));
}

void DirectXCore::InitializeViewportAndScissor() {

	// ビューポート
	viewport_.Width = static_cast<float>(Window::kClientWidth);
	viewport_.Height = static_cast<float>(Window::kClientHeight);
	viewport_.TopLeftX = 0.0f;
	viewport_.TopLeftY = 0.0f;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	// シザリング矩形
	scissorRect_.left = 0;
	scissorRect_.right = Window::kClientWidth;
	scissorRect_.top = 0;
	scissorRect_.bottom = Window::kClientHeight;
}

void DirectXCore::InitializeImGui() {
	#ifdef USE_IMGUI

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(window_->GetHwnd());
	ImGui_ImplDX12_Init(
		device_.Get(), 2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, srvDescriptorHeap_.Get(),
		srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart()
	);
	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->Build();

	#endif // USE_IMGUI
}

void DirectXCore::InitializeFixFPS() {

	// 1フレームの目標時間
	constexpr auto kFrameInterval = std::chrono::nanoseconds(1000000000 / 60);

	// 次フレームの目標時刻を設定
	nextFrameTime_ = std::chrono::steady_clock::now() + kFrameInterval;
}

void DirectXCore::UpdateFixFPS() {

	// 1フレームの目標時間
	constexpr auto kFrameInterval = std::chrono::nanoseconds(1000000000 / 60);

	// 現在時刻を取得
	auto now = std::chrono::steady_clock::now();

	// 目標時刻に到達していなければ待機
	if (now < nextFrameTime_) {
		std::this_thread::sleep_until(nextFrameTime_);
	}

	// 待機後の現在時刻を取得
	now = std::chrono::steady_clock::now();

	// 次フレームの目標時刻を更新
	if (now < nextFrameTime_) {

		nextFrameTime_ += kFrameInterval;

	} else {

		// 目標時刻から何フレーム分経過したか
		auto elapsedFrames = (now - nextFrameTime_) / kFrameInterval;

		// 遅れた分を飛ばして次の目標時刻へ進める
		nextFrameTime_ += kFrameInterval * (elapsedFrames + 1);
	}
}


void DirectXCore::WaitForGPU() {

	// Fence値を更新
	fenceValue_++;

	// GPUにSignalを送信
	HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
	assert(SUCCEEDED(hr));

	// GPUの処理が終了するまで待機
	if (fence_->GetCompletedValue() < fenceValue_) {
		hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		assert(SUCCEEDED(hr));

		WaitForSingleObject(fenceEvent_, INFINITE);
	}
}

ComPtr<ID3D12DescriptorHeap> DirectXCore::CreateDescriptorHeap(const ComPtr<ID3D12Device> &device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(descriptorHeap.GetAddressOf()));
	
	// ディスクリプタヒープの生成がうまくいかなかったら起動しない
	assert(SUCCEEDED(hr));

	// リリースビルドではassertが通らないのでhrの値を使わないため、警告が出ないようにする
	(void)hr;

	return descriptorHeap;
}


ComPtr<ID3D12Resource> DirectXCore::CreateDepthStencilTextureResource(const ComPtr<ID3D12Device>& device,int32_t width, int32_t height) {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// Resourceの生成
	ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(resource.GetAddressOf()));

	assert(SUCCEEDED(hr));

	// リリースビルドではassertが通らないのでhrの値を使わないため、警告が出ないようにする
	(void)hr;

	return resource;
}