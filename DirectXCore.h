#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <iosfwd>
#include <cstdint>
#include <Windows.h>
#include <dxcapi.h>

// 前方宣言
class Window;

class DirectXCore {
  public:
	void Initialize(std::ostream &log, Window *window);

	// FenceによるGPU待機処理
	void WaitForGPU();

	// 描画前処理
	void PreDraw();

	// 描画後処理
	void PostDraw();

	DirectXCore() = default;
	~DirectXCore();



	//=================================================================
	// ゲッター
	//==================================================================

	// DirectXデバイス
	const Microsoft::WRL::ComPtr<ID3D12Device> &GetDevice() const { return device_; }

	// コマンド関連
	const Microsoft::WRL::ComPtr<ID3D12CommandQueue> &GetCommandQueue() const { return commandQueue_; }

	const Microsoft::WRL::ComPtr<ID3D12CommandAllocator> &GetCommandAllocator() const { return commandAllocator_; }

	const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> &GetCommandList() const { return commandList_; }

	// スワップチェーン
	const Microsoft::WRL::ComPtr<IDXGISwapChain4> &GetSwapChain() const { return swapChain_; }

	// ディスクリプタヒープ
	const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> &GetSrvDescriptorHeap() const { return srvDescriptorHeap_; }

	const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> &GetDsvDescriptorHeap() const { return dsvDescriptorHeap_; }

	uint32_t GetDescriptorSizeSRV() const { return descriptorSizeSRV_; }

	// バックバッファとRTV
	ID3D12Resource *GetBackBufferResource(uint32_t index) const { return swapChainResources_[index].Get(); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(uint32_t index) const { return rtvHandles_[index]; }

	// DXC関連
	const Microsoft::WRL::ComPtr<IDxcUtils> &GetDxcUtils() const { return dxcUtils_; }

	const Microsoft::WRL::ComPtr<IDxcCompiler3> &GetDxcCompiler() const { return dxcCompiler_; }

	const Microsoft::WRL::ComPtr<IDxcIncludeHandler> &GetIncludeHandler() const { return includeHandler_; }

	// ビューポートとシザリング矩形
	const D3D12_VIEWPORT &GetViewport() const { return viewport_; }

	const D3D12_RECT &GetScissorRect() const { return scissorRect_; }



	// コピー禁止
	DirectXCore(const DirectXCore &) = delete;
	DirectXCore &operator=(const DirectXCore &) = delete;

  private:
	// DXGIファクトリー
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;

	// DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device_;

	// コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;

	// コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;

	// コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

	// 深度ステンシルバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

	// スワップチェーン
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;

	// ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

	// ディスクリプタサイズ
	uint32_t descriptorSizeSRV_ = 0;

	// スワップチェーンのバックバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources_[2];

	// RTVのディスクリプタハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2]{};

	// フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

	// フェンスの現在値
	uint64_t fenceValue_ = 0;

	// フェンス待機用イベント
	HANDLE fenceEvent_ = nullptr;

	// DXC関連
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

	// ビューポート
	D3D12_VIEWPORT viewport_{};

	// シザリング矩形
	D3D12_RECT scissorRect_{};

	// ログ出力先
	std::ostream *log_ = nullptr;

	// ウィンドウ
	Window *window_ = nullptr;

	// デバイスの初期化
	void InitializeDevice();

	// InfoQueueの初期化
	void InitializeInfoQueue();

	// コマンド関連の初期化
	void InitializeCommand();

	// スワップチェーンの初期化
	void InitializeSwapChain();

	// ディスクリプタヒープの初期化
	void InitializeDescriptorHeaps();

	// レンダーターゲットビューの初期化
	void InitializeRenderTargetView();

	// 深度バッファの初期化
	void InitializeDepthBuffer();

	// フェンスの初期化
	void InitializeFence();

	// DXCコンパイラの初期化
	void InitializeDXC();

	// ビューポートとシザリング矩形の初期化
	void InitializeViewportAndScissor();

	// ImGuiの初期化
	void InitializeImGui();

	// ディスクリプタヒープの生成
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreatDescriptorHeap(const Microsoft::WRL::ComPtr<ID3D12Device> &device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	// 深度ステンシル用テクスチャリソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device> &device, int32_t width, int32_t height);
};