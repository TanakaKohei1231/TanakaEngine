#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include"Matrix4x4.h"
#include"externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>
#include "Vector2.h"
#include <sstream>
#include <vector>
#include <cstring>
#include <wrl.h>
#include <xaudio2.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include <Xinput.h>
#include <unordered_map>
#include "DebugCamera.h"
#include"Input.h"



#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "xinput.lib")

#ifdef USE_IMGUI

#include"externals/imgui/imgui.h"
#include"externals/imgui/imgui_impl_dx12.h"
#include"externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // USE_IMGUI


// クライアント領域のサイズ
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

enum LightingType {
	LIGHTING_NONE,
	LIGHTING_LAMBERT,
	LIGHTING_HALF_LAMBERT,
};

enum ModelType {
	MODEL_PLANE,
	MODEL_SPHERE,
	MODEL_MULTI_MESH,
	MODEL_MULTI_MATERIAL,
	MODEL_COUNT,
};


// ブレンドモード
enum BlendMode {
	BLEND_MODE_NONE,
	BLEND_MODE_NORMAL,
	BLEND_MODE_ADD,
	BLEND_MODE_SUBTRACT,
	BLEND_MODE_MULTIPLY,
	BLEND_MODE_SCREEN,

	BLEND_MODE_COUNT,
};

struct Vector4 {
	float x, y, z, w;
};

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct Material {
	Vector4 color;
	int32_t lightingType;
	float padding[3];
	Matrix4x4 uvTransform;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

struct DirectionalLight {
	Vector4 color;
	Vector3 direction;
	float intensity;
};

struct MaterialData {
	std::string name;
	std::string textureFilePath;
};

struct MeshData {
	std::string name;
	std::vector<VertexData> vertices;
	uint32_t materialIndex = 0;
};

struct ModelData {
	std::vector<MeshData> meshes;
	std::vector<MaterialData> materials;
};

struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker() {
		// リソースリークチェック
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(debug.GetAddressOf())))) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}
};

// チャンクヘッダ
struct ChunkHeader {
	char id[4];   // チャンク毎のID
	int32_t size; // チャンクサイズ
};

// RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk; // "RIFF"
	char type[4];      // "WAVE"
};

// FMTチャンク
struct FormatChunk {
	ChunkHeader chunk; // "fmt "
	WAVEFORMATEX fmt;  // 波形フォーマット
};

// 音声データ
struct SoundData {
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	BYTE *pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;
};

struct ModelMaterialResource {
	std::string name;
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU;
};

struct MeshResource {
	std::string name;
	std::vector<VertexData> vertices;
	uint32_t materialIndex = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
};

struct ModelResource {
	std::string name;
	ModelData modelData;
	std::vector<MeshResource> meshes;
	std::vector<ModelMaterialResource> materials;  // モデルが持つマテリアル一覧
};

struct Object3DInstance {
	int32_t modelIndex;
	Transform transform;

	Transform uvTransform;  // ObjectごとのUVTransform
	Vector4 color;
	int32_t lightingType;
	int32_t blendMode = BLEND_MODE_NORMAL;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material *materialData;

	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource;
	TransformationMatrix *wvpData;
};

struct Particle {
	Transform transform;
	Vector3 velocty;
};

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

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

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception){
	// 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = {0};
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth,
	                 time.wDay, time.wHour, time.wMinute);

	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	// processIDとクラッシュの発生したthreadIDを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	// 設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{0};
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;

	// Dumpの出力。minidumpNormalは必要最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	// 他に関連付けられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
	return EXCEPTION_EXECUTE_HANDLER;
}


Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
	const std::wstring &filePath,
	const wchar_t *profile,
	IDxcUtils *dxcUtils,
	IDxcCompiler3 *dxcCompiler,
	IDxcIncludeHandler *includeHandler,
	std::ostream &log
) {
	// これからシェーダーをコンパイルする旨をログに出す
	Log(log,ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));

	// hlslファイルを読む
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, shaderSource.GetAddressOf());

	// 読めなかったら止める
	assert(SUCCEEDED(hr));

	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF-8であることを伝える

	
	LPCWSTR arguments[] = {
		filePath.c_str(),          // コンパイル対象のhlslファイル名
		L"-E", L"main",            // エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", profile,            // shaderProfileの設定
		L"-Zi", L"-Qembed_debug",  // デバッグ用の情報を埋め込む
		L"-Od",                    // 最適化を外しておく
		L"-Zpr",                   // メモリレイアウトは行優先
	};

	// 実際にshaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler,
		IID_PPV_ARGS(shaderResult.GetAddressOf())
	);

	assert(SUCCEEDED(hr));

	// 警告、エラーがでたらログに出して止める
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(shaderError.GetAddressOf()), nullptr);

	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(log,shaderError->GetStringPointer());
		assert(false);
	}

	// コンパイル結果から実行用のバイナリ部分を取得
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shaderBlob.GetAddressOf()), nullptr);
	assert(SUCCEEDED(hr));

	// 成功のログを出す
	Log(log, ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));

	// 実行用のバイナリを返却
	return shaderBlob;
}


Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device> &device, size_t sizeInBytes) {

	assert(device != nullptr);

	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// リソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};

	// バッファリソースであることを示す
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;

	// リソースのサイズ
	resourceDesc.Width = sizeInBytes;

	// バッファの場合はこれで固定
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際にリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;

	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(resource.GetAddressOf()));

	assert(SUCCEEDED(hr));

	// リリースビルドではassertが通らないのでhrの値を使わないため、警告が出ないようにする
	(void)hr;

	return resource;
}

MeshResource CreateMeshResource(const Microsoft::WRL::ComPtr<ID3D12Device> &device, const MeshData &meshData) {
	MeshResource meshResource{};

	meshResource.name = meshData.name;
	meshResource.vertices = meshData.vertices;
	meshResource.materialIndex = meshData.materialIndex;

	meshResource.vertexResource = CreateBufferResource(device, sizeof(VertexData) * meshResource.vertices.size());

	meshResource.vertexBufferView.BufferLocation = meshResource.vertexResource->GetGPUVirtualAddress();

	meshResource.vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * meshResource.vertices.size());

	meshResource.vertexBufferView.StrideInBytes = sizeof(VertexData);

	VertexData *vertexData = nullptr;
	HRESULT hr = meshResource.vertexResource->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));
	assert(SUCCEEDED(hr));

	// リリースビルドではassertが通らないのでhrの値を使わないため、警告が出ないようにする
	(void)hr;

	std::memcpy(vertexData, meshResource.vertices.data(), sizeof(VertexData) * meshResource.vertices.size());

	return meshResource;
}

ModelResource CreateModelResource(
	const Microsoft::WRL::ComPtr<ID3D12Device> &device,
	const std::string &name,
	const ModelData &modelData,
	const std::vector<ModelMaterialResource> &materials
) {
	ModelResource modelResource{};

	modelResource.name = name;
	modelResource.modelData = modelData;
	modelResource.materials = materials;

	assert(!modelResource.materials.empty());

	for (const MeshData &meshData : modelData.meshes) {
		modelResource.meshes.push_back(CreateMeshResource(device, meshData));
	}

	return modelResource;
}


Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreatDescriptorHeap(
	const Microsoft::WRL::ComPtr<ID3D12Device> &device,
	D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible
) {

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
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


DirectX::ScratchImage LoadTexture(const std::string &filePath) {
	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);

	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	const DirectX::TexMetadata &metadata = image.GetMetadata();

	// 1x1のTextureはこれ以上小さいmipmapを作れないため、そのまま返す
	if (metadata.width == 1 && metadata.height == 1) {
		return image;
	}

	// ミニマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);

	assert(SUCCEEDED(hr));

	// ミニマップ付きのデータを返す
	return mipImages;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
	const Microsoft::WRL::ComPtr<ID3D12Device> &device,
	const DirectX::TexMetadata &metadata
) {
	// metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);                             // Textureの幅
	resourceDesc.Height = UINT(metadata.height);                           // Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);                   // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);            // 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;                                 // TextureのFormat
	resourceDesc.SampleDesc.Count = 1;                                     // サンプリングカウント。1固定。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数。普段使っているのは2次元

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作成する

	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,                // Heapの設定
		D3D12_HEAP_FLAG_NONE,           // Heapの特殊な設定。特になし。
		&resourceDesc,                  // Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST, // データ転送される設定
		nullptr,                        // Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(resource.GetAddressOf())
	);

	assert(SUCCEEDED(hr));

	// リリースビルドではassertが通らないのでhrの値を使わないため、警告が出ないようにする
	(void)hr;

	return resource;
}

[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(
	const Microsoft::WRL::ComPtr<ID3D12Resource> &texture,
	const DirectX::ScratchImage &mipImages,
	const Microsoft::WRL::ComPtr<ID3D12Device> &device,
	const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> &commandList
) {
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;

	DirectX::PrepareUpload(
		device.Get(),
		mipImages.GetImages(),
		mipImages.GetImageCount(),
		mipImages.GetMetadata(),
		subresources
	);

	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);

	UpdateSubresources(
		commandList.Get(),
		texture.Get(),
		intermediateResource.Get(),
		0, 0,
		UINT(subresources.size()),
		subresources.data()
	);

	// Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(
	const Microsoft::WRL::ComPtr<ID3D12Device>& device,
	int32_t width,
	int32_t height
) {
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
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(resource.GetAddressOf()));

	assert(SUCCEEDED(hr));

	// リリースビルドではassertが通らないのでhrの値を使わないため、警告が出ないようにする
	(void)hr;

	return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
	const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> &descriptorHeap,uint32_t descriptorSize, uint32_t index
) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();

	handleCPU.ptr += (descriptorSize * index);

	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
	const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> &descriptorHeap, uint32_t descriptorSize, uint32_t index
) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();

	handleGPU.ptr += (descriptorSize * index);

	return handleGPU;
}

std::vector<MaterialData> LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename) {
	std::vector<MaterialData> materials;
	MaterialData currentMaterial{};

	bool hasMaterial = false;
	std::string line;

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "newmtl") {
			if (hasMaterial) {
				materials.push_back(currentMaterial);
				currentMaterial = {};
			}

			s >> currentMaterial.name;
			hasMaterial = true;

		} else if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;

			currentMaterial.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	if (hasMaterial) {
		materials.push_back(currentMaterial);
	}

	return materials;
}

ModelData LoadObjFile(const std::string &directoryPath, const std::string &filename) {
	ModelData modelData;            // 構築するModelData
	std::vector<Vector4> positions; // 位置
	std::vector<Vector3> normals;   // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::unordered_map<std::string, uint32_t> materialIndexMap;

	std::string line;

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	uint32_t currentMaterialIndex = 0;
	std::string currentMeshName = "default";

	MeshData currentMesh{};
	currentMesh.name = currentMeshName;
	currentMesh.materialIndex = currentMaterialIndex;

	auto PushCurrentMesh = [&]() {
		if (!currentMesh.vertices.empty()) {
			modelData.meshes.push_back(currentMesh);

			currentMesh = {};
			currentMesh.name = currentMeshName;
			currentMesh.materialIndex = currentMaterialIndex;
		}
	};

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "mtllib") {
			std::string materialFilename;
			s >> materialFilename;

			modelData.materials = LoadMaterialTemplateFile(directoryPath, materialFilename);

			for (uint32_t materialIndex = 0; materialIndex < modelData.materials.size(); ++materialIndex) {
				materialIndexMap[modelData.materials[materialIndex].name] = materialIndex;
			}

		} else if (identifier == "usemtl") {
			PushCurrentMesh();

			std::string materialName;
			s >> materialName;

			if (materialIndexMap.contains(materialName)) {
				currentMaterialIndex = materialIndexMap[materialName];
			} else {
				currentMaterialIndex = 0;
			}

			currentMesh.name = currentMeshName + "_" + materialName;
			currentMesh.materialIndex = currentMaterialIndex;

		} else if (identifier == "o" || identifier == "g") {
			PushCurrentMesh();

			s >> currentMeshName;

			if (currentMeshName.empty()) {
				currentMeshName = "mesh";
			}

			currentMesh.name = currentMeshName;
			currentMesh.materialIndex = currentMaterialIndex;

		} else if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {
			std::vector<VertexData> faceVertices;
			std::string vertexDefinition;

			while (s >> vertexDefinition) {
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3] = {};

				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');

					if (!index.empty()) {
						elementIndices[element] = static_cast<uint32_t>(std::stoi(index));
					}
				}

				assert(elementIndices[0] != 0);
				assert(elementIndices[0] <= positions.size());

				Vector4 position = positions[elementIndices[0] - 1];

				// UVが無いOBJもあるため、無い場合は(0, 0)を入れる
				Vector2 texcoord = {0.0f, 0.0f};

				if (elementIndices[1] != 0) {
					assert(elementIndices[1] <= texcoords.size());

					texcoord = texcoords[elementIndices[1] - 1];
					texcoord.x = 1.0f - texcoord.x;
					texcoord.y = 1.0f - texcoord.y;
				}

				// 法線が無い場合の保険
				Vector3 normal = {0.0f, 0.0f, 1.0f};

				if (elementIndices[2] != 0) {
					assert(elementIndices[2] <= normals.size());

					normal = normals[elementIndices[2] - 1];
					normal.x *= -1.0f;
				}

				position.x *= -1.0f;

				faceVertices.push_back({position, texcoord, normal});
			}

			// 三角形以外の面も三角形に分割して登録する
			for (uint32_t vertexIndex = 1; vertexIndex + 1 < faceVertices.size(); ++vertexIndex) {
				currentMesh.vertices.push_back(faceVertices[0]);
				currentMesh.vertices.push_back(faceVertices[vertexIndex]);
				currentMesh.vertices.push_back(faceVertices[vertexIndex + 1]);
			}
		}
	}

	PushCurrentMesh();

	return modelData;
}


ModelData CreateSphereModelData(uint32_t subdivision, float radius) {
	ModelData modelData{};
	MeshData sphereMesh{};
	sphereMesh.name = "sphere";

	const float kPi = static_cast<float>(M_PI);

	for (uint32_t latitudeIndex = 0; latitudeIndex < subdivision; ++latitudeIndex) {
		float latitude0 = kPi * static_cast<float>(latitudeIndex) / static_cast<float>(subdivision);
		float latitude1 = kPi * static_cast<float>(latitudeIndex + 1) / static_cast<float>(subdivision);

		for (uint32_t longitudeIndex = 0; longitudeIndex < subdivision; ++longitudeIndex) {
			float longitude0 = 2.0f * kPi * static_cast<float>(longitudeIndex) / static_cast<float>(subdivision);
			float longitude1 = 2.0f * kPi * static_cast<float>(longitudeIndex + 1) / static_cast<float>(subdivision);

			Vector3 position0 = {
				radius * std::sin(latitude0) * std::cos(longitude0), radius * std::cos(latitude0),
				radius * std::sin(latitude0) * std::sin(longitude0)
			};

			Vector3 position1 = {
				radius * std::sin(latitude1) * std::cos(longitude0), radius * std::cos(latitude1),
				radius * std::sin(latitude1) * std::sin(longitude0)
			};

			Vector3 position2 = {
				radius * std::sin(latitude0) * std::cos(longitude1), radius * std::cos(latitude0),
				radius * std::sin(latitude0) * std::sin(longitude1)
			};

			Vector3 position3 = {
				radius * std::sin(latitude1) * std::cos(longitude1), radius * std::cos(latitude1),
				radius * std::sin(latitude1) * std::sin(longitude1)
			};

			Vector2 texcoord0 = {
				static_cast<float>(longitudeIndex) / static_cast<float>(subdivision),
				static_cast<float>(latitudeIndex) / static_cast<float>(subdivision)
			};

			Vector2 texcoord1 = {
				static_cast<float>(longitudeIndex) / static_cast<float>(subdivision),
				static_cast<float>(latitudeIndex + 1) / static_cast<float>(subdivision)
			};

			Vector2 texcoord2 = {
				static_cast<float>(longitudeIndex + 1) / static_cast<float>(subdivision),
				static_cast<float>(latitudeIndex) / static_cast<float>(subdivision)
			};

			Vector2 texcoord3 = {
				static_cast<float>(longitudeIndex + 1) / static_cast<float>(subdivision),
				static_cast<float>(latitudeIndex + 1) / static_cast<float>(subdivision)
			};

			Vector3 normal0 = Vector3::Normalize(position0);
			Vector3 normal1 = Vector3::Normalize(position1);
			Vector3 normal2 = Vector3::Normalize(position2);
			Vector3 normal3 = Vector3::Normalize(position3);

			// 1枚目の三角形
			sphereMesh.vertices.push_back({{position0.x, position0.y, position0.z, 1.0f}, texcoord0, normal0});
			sphereMesh.vertices.push_back({{position2.x, position2.y, position2.z, 1.0f}, texcoord2, normal2});
			sphereMesh.vertices.push_back({{position1.x, position1.y, position1.z, 1.0f}, texcoord1, normal1});

			// 2枚目の三角形
			sphereMesh.vertices.push_back({{position2.x, position2.y, position2.z, 1.0f}, texcoord2, normal2});
			sphereMesh.vertices.push_back({{position3.x, position3.y, position3.z, 1.0f}, texcoord3, normal3});
			sphereMesh.vertices.push_back({{position1.x, position1.y, position1.z, 1.0f}, texcoord1, normal1});
		}
	}
	modelData.meshes.push_back(sphereMesh);

	return modelData;
}

D3D12_BLEND_DESC CreateBlendDesc(BlendMode blendMode) {
	D3D12_BLEND_DESC blendDesc{};

	D3D12_RENDER_TARGET_BLEND_DESC &renderTargetBlendDesc = blendDesc.RenderTarget[0];

	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// ブレンドなしの状態を基本設定にする
	renderTargetBlendDesc.BlendEnable = FALSE;
	renderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
	renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	renderTargetBlendDesc.DestBlend = D3D12_BLEND_ZERO;

	// アルファ値
	renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;

	switch (blendMode) {
	case BLEND_MODE_NONE:
		renderTargetBlendDesc.BlendEnable = FALSE;
		break;

	case BLEND_MODE_NORMAL:
		renderTargetBlendDesc.BlendEnable = TRUE;
		renderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		break;

	case BLEND_MODE_ADD:
		renderTargetBlendDesc.BlendEnable = TRUE;
		renderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.DestBlend = D3D12_BLEND_ONE;
		break;

	case BLEND_MODE_SUBTRACT:
		renderTargetBlendDesc.BlendEnable = TRUE;
		renderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		renderTargetBlendDesc.DestBlend = D3D12_BLEND_ONE;
		break;

	case BLEND_MODE_MULTIPLY:
		renderTargetBlendDesc.BlendEnable = TRUE;
		renderTargetBlendDesc.SrcBlend = D3D12_BLEND_DEST_COLOR;
		renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.DestBlend = D3D12_BLEND_ZERO;
		break;

	case BLEND_MODE_SCREEN:
		renderTargetBlendDesc.BlendEnable = TRUE;
		renderTargetBlendDesc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.DestBlend = D3D12_BLEND_ONE;
		break;

	default:
		renderTargetBlendDesc.BlendEnable = FALSE;
		break;
	}

	return blendDesc;
}

SoundData SoundLoadWave(const char *filename) {
	// ファイル入力ストリームのインスタンス
	std::ifstream file;

	// .wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);

	// ファイルオープン失敗を検出する
	assert(file.is_open());

	// RIFFヘッダーの読み込み
	RiffHeader riff{};
	file.read(reinterpret_cast<char *>(&riff), sizeof(riff));

	// ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}

	// タイプがWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}

	// Formatチャンクの読み込み
	FormatChunk format{};

	// チャンクヘッダーの確認
	file.read(reinterpret_cast<char *>(&format.chunk), sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}

	// チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read(reinterpret_cast<char *>(&format.fmt), format.chunk.size);

	// Dataチャンクの読み込み
	ChunkHeader data{};
	file.read(reinterpret_cast<char *>(&data), sizeof(data));

	// JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0) {
		// 読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);

		// 再読み込み
		file.read(reinterpret_cast<char *>(&data), sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}

	// Dataチャンクのデータ部（波形データ）の読み込み
	char *pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	// Waveファイルを閉じる
	file.close();

	// returnする為の音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE *>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}


// 音声データ解放
void SoundUnload(SoundData *soundData) {
	// バッファのメモリを解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

// 音声再生
void SoundPlayWave(IXAudio2 *xAudio2, const SoundData &soundData) {
	HRESULT result;

	// 波形フォーマットを元にSourceVoiceの生成
	IXAudio2SourceVoice *pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	// 再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	// 波形データの再生
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	assert(SUCCEEDED(result));

	result = pSourceVoice->Start();
	assert(SUCCEEDED(result));
}


Object3DInstance CreateObject3DInstance(
	const Microsoft::WRL::ComPtr<ID3D12Device> &device, int32_t modelIndex,
	const Transform &transform
) {

	Object3DInstance object{};

	object.modelIndex = modelIndex;
	object.transform = transform;
	object.color = {1.0f, 1.0f, 1.0f, 1.0f};
	object.lightingType = LIGHTING_HALF_LAMBERT;
	object.uvTransform.scale = {1.0f, 1.0f, 1.0f};
	object.uvTransform.rotate = {0.0f, 0.0f, 0.0f};
	object.uvTransform.translate = {0.0f, 0.0f, 0.0f};

	// マテリアル用リソースを作成
	object.materialResource = CreateBufferResource(device, sizeof(Material));

	HRESULT hr = object.materialResource->Map(0, nullptr, reinterpret_cast<void **>(&object.materialData));
	assert(SUCCEEDED(hr));

	object.materialData->color = object.color;
	object.materialData->lightingType = object.lightingType;
	object.materialData->uvTransform = Matrix4x4::MakeIdentity4x4();

	// WVP用リソースを作成
	object.wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));

	hr = object.wvpResource->Map(0, nullptr, reinterpret_cast<void **>(&object.wvpData));
	assert(SUCCEEDED(hr));

	object.wvpData->WVP = Matrix4x4::MakeIdentity4x4();
	object.wvpData->World = Matrix4x4::MakeIdentity4x4();

	return object;
}


// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	CoInitializeEx(0, COINIT_MULTITHREADED);

	D3DResourceLeakChecker leakCheck;



	// ログファイルの作成
	std::ofstream log = CreateLogFile();

	// 誰も捕捉できなかった場合に捕捉する関数を登録
	SetUnhandledExceptionFilter(ExportDump);

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

	#ifdef _DEBUG

	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
		// デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();

		// さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}

	#endif // _DEBUG


	// ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

	MSG msg{};

	// DXGIファクトリーの生成
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;

	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// 使用するアダプタ用の変数
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter;

	// アダプタの列挙良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
		i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(useAdapter.ReleaseAndGetAddressOf())) != DXGI_ERROR_NOT_FOUND;++i
		) {
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
		useAdapter.Reset();
	}

	// アダプタが見つからなかったらエラー
	assert(useAdapter != nullptr);

	Microsoft::WRL::ComPtr<ID3D12Device> device;

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
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(device.GetAddressOf()));

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

	#ifdef _DEBUG

	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;

	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
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


	// コマンドキューの生成
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue.GetAddressOf()));

	// コマンドキューの生成がうまくいかなかったら起動しない
	assert(SUCCEEDED(hr));

	// コマンドアロケータを生成する
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.GetAddressOf()));

	// コマンドアロケータの生成がうまくいかなかったら起動しない
	assert(SUCCEEDED(hr));

	// コマンドリストを生成する
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(commandList.GetAddressOf()));

	// コマンドリストの生成がうまくいかなかったら起動しない
	assert(SUCCEEDED(hr));

	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	// スワップチェーンを生成する
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;		 // 画面の幅。ウィンドウのクライアント領域の幅と同じにする
	swapChainDesc.Height = kClientHeight;	 // 画面の高さ。ウィンドウのクライアント領域の高さと同じにする
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2; // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;    // モニタに映したら中身を破棄する

	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, swapChain1.GetAddressOf());
	assert(SUCCEEDED(hr));

	hr = swapChain1.As(&swapChain);
	assert(SUCCEEDED(hr));

	// ディスクリプタヒープの生成
	// RTV用のヒープでディスクリプタの数は2。SRVはshader内で触るものではないので、shaderVisibleはfalse
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = CreatDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// SRV用のヒープでディスクリプタの数は128。SRVはshader内で触るものなので、shaderVisibleはtrue
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreatDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、shaderVisibleはfalse
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = CreatDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// DescriptorSizeを取得しておく
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// スワップチェーンからリソースを引っ張ってくる
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2];
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(swapChainResources[0].GetAddressOf()));

	// うまくいかなかったら起動しない
	assert(SUCCEEDED(hr));

	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(swapChainResources[1].GetAddressOf()));
	assert(SUCCEEDED(hr));

	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をsRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む

	// ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	// RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);

	// 2つ目のディスクリプタハンドルを得る
	rtvHandles[1] = {rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)};

	// 2つ目を作る
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);


	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	// DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(
		depthStencilResource.Get(),
		&dsvDesc,dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
	);

	// 初期値0でFenceを作る
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	uint64_t fenceValue = 0;

	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// FenceのSignalを待つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);

	// dxcCompilerを初期化
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils.GetAddressOf()));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxcCompiler.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// 現時点でincludeはしないが、includeに対応するための設定を行っておく
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
	hr = dxcUtils->CreateDefaultIncludeHandler(includeHandler.GetAddressOf());
	assert(SUCCEEDED(hr));


	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成。PixelShaderのmaterialとVertexShaderのTransform
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビュー
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで見えるようにする
	rootParameters[0].Descriptor.ShaderRegister = 0;                    // b0の0と一致する。もしb11だったら11にする

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビュー
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで見えるようにする
	rootParameters[1].Descriptor.ShaderRegister = 0;                    // b0の0と一致する。もしb11だったら11にする

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;                                                   // 0から始まる
	descriptorRange[0].NumDescriptors = 1;                                                       // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;                              // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;      // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;                // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;             // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数

	// DirectionalLight用
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;
	
	descriptionRootSignature.pParameters = rootParameters;              // RootParameterの配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);  // RootParameterの数

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;   // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;     // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;                       // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;                               // レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズして	バイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	hr = D3D12SerializeRootSignature(
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		signatureBlob.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (FAILED(hr)) {
		Log(log, reinterpret_cast<char *>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// バイナリを元に生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.GetAddressOf())
	);
	assert(SUCCEEDED(hr));

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	std::vector<D3D12_BLEND_DESC> blendDescs{};
	blendDescs.reserve(BLEND_MODE_COUNT);

	for (int32_t blendMode = 0; blendMode < BLEND_MODE_COUNT; ++blendMode) {
		blendDescs.push_back(CreateBlendDesc(static_cast<BlendMode>(blendMode)));
	}

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get(), log);
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl", L"ps_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get(), log);
	assert(pixelShaderBlob != nullptr);

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;

	// 書き込み
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// PSOの設定
	std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> graphicsPipelineStates{};
	graphicsPipelineStates.resize(BLEND_MODE_COUNT);

	for (int32_t blendMode = 0; blendMode < BLEND_MODE_COUNT; ++blendMode) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
		graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
		graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
		graphicsPipelineStateDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
		graphicsPipelineStateDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};
		graphicsPipelineStateDesc.BlendState = blendDescs[blendMode];
		graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
		graphicsPipelineStateDesc.NumRenderTargets = 1;
		graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		graphicsPipelineStateDesc.SampleDesc.Count = 1;
		graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

		// DepthStencilの設定
		graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
		graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		// 実際に生成
		hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(graphicsPipelineStates[blendMode].GetAddressOf()));
		assert(SUCCEEDED(hr));
	}

	// ===========================================================
	// Particle用RootSignatureの作成
	// ===========================================================

	// Particle用RootSignatureDesc
	D3D12_ROOT_SIGNATURE_DESC particleRootSignatureDesc{};
	particleRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Particle用RootParameter
	D3D12_ROOT_PARAMETER particleRootParameters[3] = {};

	// Material用CBV
	particleRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	particleRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	particleRootParameters[0].Descriptor.ShaderRegister = 0;

	// Instancing用SRVのDescriptorRange
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};

	descriptorRangeForInstancing[0].BaseShaderRegister = 0;                      // 0から始まる
	descriptorRangeForInstancing[0].NumDescriptors = 1;                          // 数は1つ
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Instancing用DescriptorTable
	particleRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	particleRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;          // VertexShaderで使う
	particleRootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing; // Tableの中身の配列を指定
	particleRootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing); // Tableで利用する数

	// Texture用SRVのDescriptorRange
	D3D12_DESCRIPTOR_RANGE particleTextureDescriptorRange[1] = {};

	particleTextureDescriptorRange[0].BaseShaderRegister = 0;
	particleTextureDescriptorRange[0].NumDescriptors = 1;
	particleTextureDescriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	particleTextureDescriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Texture用DescriptorTable
	particleRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	particleRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	particleRootParameters[2].DescriptorTable.pDescriptorRanges = particleTextureDescriptorRange;
	particleRootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(particleTextureDescriptorRange);

	// RootParameterを登録
	particleRootSignatureDesc.pParameters = particleRootParameters;
	particleRootSignatureDesc.NumParameters = _countof(particleRootParameters);

	// Particle専用StaticSampler
	D3D12_STATIC_SAMPLER_DESC particleStaticSamplers[1] = {};

	particleStaticSamplers[0] = staticSamplers[0];

	particleRootSignatureDesc.pStaticSamplers = particleStaticSamplers;
	particleRootSignatureDesc.NumStaticSamplers = _countof(particleStaticSamplers);

	// RootSignatureをシリアライズ
	Microsoft::WRL::ComPtr<ID3DBlob> particleSignatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> particleErrorBlob;

	hr = D3D12SerializeRootSignature(&particleRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, particleSignatureBlob.GetAddressOf(), particleErrorBlob.GetAddressOf());

	if (FAILED(hr)) {
		if (particleErrorBlob != nullptr) {
			Log(log, reinterpret_cast<char *>(particleErrorBlob->GetBufferPointer()));
		}
		assert(false);
	}

	// Particle用RootSignatureを生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> particleRootSignature;

	hr = device->CreateRootSignature(0, particleSignatureBlob->GetBufferPointer(), particleSignatureBlob->GetBufferSize(), IID_PPV_ARGS(particleRootSignature.GetAddressOf()));

	assert(SUCCEEDED(hr));

	// ===========================================================
	// Particle用GraphicsPipelineStateの作成
	// ===========================================================

	// Particle用InputLayout
	D3D12_INPUT_ELEMENT_DESC particleInputElementDescs[3] = {};

	for (uint32_t index = 0; index < _countof(particleInputElementDescs); ++index) {
		particleInputElementDescs[index] = inputElementDescs[index];
	}

	D3D12_INPUT_LAYOUT_DESC particleInputLayoutDesc{};
	particleInputLayoutDesc.pInputElementDescs = particleInputElementDescs;
	particleInputLayoutDesc.NumElements = _countof(particleInputElementDescs);

	// Particle用BlendState
	D3D12_BLEND_DESC particleBlendDesc = CreateBlendDesc(BLEND_MODE_NORMAL);

	// Particle用RasterizerState
	D3D12_RASTERIZER_DESC particleRasterizerDesc = rasterizerDesc;

	// Particle用DepthStencilState
	D3D12_DEPTH_STENCIL_DESC particleDepthStencilDesc = depthStencilDesc;

	// Particle用VertexShaderのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> particleVertexShaderBlob = CompileShader(L"Particle.VS.hlsl", L"vs_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get(), log);

	assert(particleVertexShaderBlob != nullptr);

	// Particle用PixelShaderのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> particlePixelShaderBlob = CompileShader(L"Particle.PS.hlsl", L"ps_6_0", dxcUtils.Get(), dxcCompiler.Get(), includeHandler.Get(), log);

	assert(particlePixelShaderBlob != nullptr);

	// Particle用PSOの設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC particlePipelineStateDesc{};

	particlePipelineStateDesc.pRootSignature = particleRootSignature.Get();
	particlePipelineStateDesc.InputLayout = particleInputLayoutDesc;
	particlePipelineStateDesc.VS = {particleVertexShaderBlob->GetBufferPointer(), particleVertexShaderBlob->GetBufferSize()};
	particlePipelineStateDesc.PS = {particlePixelShaderBlob->GetBufferPointer(), particlePixelShaderBlob->GetBufferSize()};

	particlePipelineStateDesc.BlendState = particleBlendDesc;
	particlePipelineStateDesc.RasterizerState = particleRasterizerDesc;
	particlePipelineStateDesc.DepthStencilState = particleDepthStencilDesc;

	particlePipelineStateDesc.NumRenderTargets = 1;
	particlePipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	particlePipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	particlePipelineStateDesc.SampleDesc.Count = 1;
	particlePipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	particlePipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// Particle用PSOを生成
	Microsoft::WRL::ComPtr<ID3D12PipelineState> particlePipelineState;

	hr = device->CreateGraphicsPipelineState(&particlePipelineStateDesc, IID_PPV_ARGS(particlePipelineState.GetAddressOf()));

	assert(SUCCEEDED(hr));

	// マテリアル用のリソースを作る。今回はcolorを1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = CreateBufferResource(device, sizeof(Material));

	// マテリアルにデータを書き込む。
	Material *materialData = nullptr;

	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));

	// 今回は赤を書き込んでみる
	materialData->color = {1.0f, 1.0f, 1.0f, 1.0f};

	// 初期ライティング方式はHalf Lambert
	materialData->lightingType = LIGHTING_HALF_LAMBERT;

	materialData->uvTransform = Matrix4x4::MakeIdentity4x4();

	// スプライトはライティングしないので、スプライト用のマテリアルリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = CreateBufferResource(device, sizeof(Material));

	Material *materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void **>(&materialDataSprite));
	materialDataSprite->color = {1.0f, 1.0f, 1.0f, 1.0f};
	materialDataSprite->lightingType = LIGHTING_NONE;
	materialDataSprite->uvTransform = Matrix4x4::MakeIdentity4x4();

	// 平行光源用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));

	// データを書き込む
	DirectionalLight *directionalLightData = nullptr;

	// 書き込むためのアドレスを取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void **>(&directionalLightData));

	// デフォルト値
	directionalLightData->color = {1.0f, 1.0f, 1.0f, 1.0f};
	directionalLightData->direction = {0.0f, -1.0f, 0.0f};
	directionalLightData->intensity = 1.0f;

	// TransformationMatrix用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));

	// データを書き込む
	TransformationMatrix *wvpData = nullptr;

	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void **>(&wvpData));

	// 単位行列を書き込んでおく
	wvpData->WVP = Matrix4x4::MakeIdentity4x4();
	wvpData->World = Matrix4x4::MakeIdentity4x4();

	// モデル読み込み
	ModelData modelData = LoadObjFile("resources", "plane.obj");

	// Particle用モデルの作成

	ModelData particleModelData{};
	MeshData particleMeshData{};
	particleMeshData.name = "particle";

	// 左上
	particleMeshData.vertices.push_back({{1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});

	// 右上
	particleMeshData.vertices.push_back({{-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});

	// 左下
	particleMeshData.vertices.push_back({{1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}});

	// 右上
	particleMeshData.vertices.push_back({{-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});

	// 左下
	particleMeshData.vertices.push_back({{1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}});

	// 右下
	particleMeshData.vertices.push_back({{-1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}});

	particleModelData.meshes.push_back(particleMeshData);

	// Particle用VertexBufferの作成
	MeshResource particleMeshResource = CreateMeshResource(device, particleMeshData);

	// ===========================================================
	// Instancing用Resourceの作成
	// ===========================================================

	// インスタンス数
	const uint32_t kNumInstance = 10;

	// Instancing用のTransformationMatrixリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource = CreateBufferResource(device, sizeof(TransformationMatrix) * kNumInstance);

	// 書き込むためのアドレスを取得
	TransformationMatrix *instancingData = nullptr;

	hr = instancingResource->Map(0, nullptr, reinterpret_cast<void **>(&instancingData));

	assert(SUCCEEDED(hr));

	// 単位行列を書き込んでおく
	for (uint32_t index = 0; index < kNumInstance; ++index) {
		instancingData[index].WVP = Matrix4x4::MakeIdentity4x4();
		instancingData[index].World = Matrix4x4::MakeIdentity4x4();
	}

	// Particle用Transformの作成
	Particle particles[kNumInstance];

	for (uint32_t index = 0; index < kNumInstance; ++index) {
		particles[index].transform.scale = {1.0f, 1.0f, 1.0f};
		particles[index].transform.rotate = {0.0f, 0.0f, 0.0f};
		particles[index].transform.translate = {index * 0.1f, index * 0.1f, index * 0.1f};
	}

	ModelData sphereModelData = CreateSphereModelData(16, 1.0f);

	// 配布モデル
	ModelData multiMeshModelData = LoadObjFile("resources", "multiMesh.obj");
	ModelData multiMaterialModelData = LoadObjFile("resources", "multiMaterial.obj");
	ModelData teapotModelData = LoadObjFile("resources", "teapot.obj");
	ModelData bunnyModelData = LoadObjFile("resources", "bunny.obj");
	ModelData suzanneModelData = LoadObjFile("resources", "suzanne.obj");
	ModelData fenceModelData = LoadObjFile("resources", "fence.obj");

	// Sprite用の頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 4);

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};

	// リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();

	// 使用するリソースのサイズは頂点4つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;

	// 1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	// Sprite用のTransformationMatrix用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));

	// データを書き込む
	TransformationMatrix *transformationMatrixDataSprite = nullptr;

	// 書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void **>(&transformationMatrixDataSprite));

	// 単位行列を書きこんでおく
	transformationMatrixDataSprite->WVP = Matrix4x4::MakeIdentity4x4();
	transformationMatrixDataSprite->World = Matrix4x4::MakeIdentity4x4();

	VertexData *vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void **>(&vertexDataSprite));

	// 左下
	vertexDataSprite[0].position = {0.0f, 360.0f, 0.0f, 1.0f};
	vertexDataSprite[0].texcoord = {0.0f, 1.0f};
	vertexDataSprite[0].normal = {0.0f, 0.0f, -1.0f};

	// 左上
	vertexDataSprite[1].position = {0.0f, 0.0f, 0.0f, 1.0f};
	vertexDataSprite[1].texcoord = {0.0f, 0.0f};
	vertexDataSprite[1].normal = {0.0f, 0.0f, -1.0f};

	// 右下
	vertexDataSprite[2].position = {640.0f, 360.0f, 0.0f, 1.0f};
	vertexDataSprite[2].texcoord = {1.0f, 1.0f};
	vertexDataSprite[2].normal = {0.0f, 0.0f, -1.0f};

	// 右上
	vertexDataSprite[3].position = {640.0f, 0.0f, 0.0f, 1.0f};
	vertexDataSprite[3].texcoord = {1.0f, 0.0f};
	vertexDataSprite[3].normal = {0.0f, 0.0f, -1.0f};

	// Sprite用のインデックスリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);

	// インデックスバッファビューを作成する
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};

	// リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();

	// 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;

	// インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書き込む
	uint32_t *indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void **>(&indexDataSprite));

	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;

	// ビューポート
	D3D12_VIEWPORT viewport{};
	// クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = static_cast<float>(kClientWidth);
	viewport.Height = static_cast<float>(kClientHeight);
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// シザー矩形
	D3D12_RECT scissorRect{};
	// 基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata &metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = CreateTextureResource(device, metadata);

	// 2枚目のTextureを読んで転送する
	assert(!modelData.materials.empty());

	DirectX::ScratchImage mipImages2 = LoadTexture(modelData.materials[0].textureFilePath);
	const DirectX::TexMetadata &metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(device, metadata2);

	// monsterBallのTextureを読んで転送する
	DirectX::ScratchImage monsterBallMipImages = LoadTexture("resources/monsterBall.png");
	const DirectX::TexMetadata &monsterBallMetadata = monsterBallMipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> monsterBallTextureResource = CreateTextureResource(device, monsterBallMetadata);

	// checkerBoardのTextureを読んで転送する
	DirectX::ScratchImage checkerBoardMipImages = LoadTexture("resources/checkerBoard.png");
	const DirectX::TexMetadata &checkerBoardMetadata = checkerBoardMipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> checkerBoardTextureResource = CreateTextureResource(device, checkerBoardMetadata);

	// white1x1のTextureを読んで転送する
	DirectX::ScratchImage white1x1MipImages = LoadTexture("resources/white1x1.png");
	const DirectX::TexMetadata &white1x1Metadata = white1x1MipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> white1x1TextureResource = CreateTextureResource(device, white1x1Metadata);

	// fenceのTextureを読んで転送する
	DirectX::ScratchImage fenceMipImages = LoadTexture("resources/fence.png");
	const DirectX::TexMetadata &fenceMetadata = fenceMipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> fenceTextureResource = CreateTextureResource(device, fenceMetadata);

	// Texture転送用のCommandListを準備
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));

	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));

	// Textureデータを転送するコマンドを積む
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(textureResource, mipImages, device, commandList);

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = UploadTextureData(textureResource2, mipImages2, device, commandList);

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateMonsterBallResource = UploadTextureData(monsterBallTextureResource, monsterBallMipImages, device, commandList);

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateCheckerBoardResource = UploadTextureData(checkerBoardTextureResource, checkerBoardMipImages, device, commandList);

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateWhite1x1Resource = UploadTextureData(white1x1TextureResource, white1x1MipImages, device, commandList);
	
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateFenceResource = UploadTextureData(fenceTextureResource, fenceMipImages, device, commandList);

	// CommandListをCloseし、CommandQueueにExecuteCommandListsを使いキックする
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList *commandLists[] = {commandList.Get()};
	commandQueue->ExecuteCommandLists(1, commandLists);

	// 実行を待つ
	fenceValue++;
	hr = commandQueue->Signal(fence.Get(), fenceValue);
	assert(SUCCEEDED(hr));

	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// ここまできたら転送は終わっているので、IntermediateResourceは解放しても良い
	intermediateResource.Reset();
	intermediateResource2.Reset();
	intermediateMonsterBallResource.Reset();
	intermediateCheckerBoardResource.Reset();
	intermediateWhite1x1Resource.Reset();
	intermediateFenceResource.Reset();

	// ===========================================================
	// Texture用SRVの作成
	// ===========================================================

	// uvCheckerのmetadataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// uvChecker用のSRVを作成するDescriptorHeapの場所を決める
	// index 0 はImGuiが使うため、index 1 を使う
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);

	// uvChecker用SRVの生成
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	// planeTextureのmetadataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// planeTexture用のSRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

	// planeTexture用SRVの生成
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	// monsterBallのmetadataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC monsterBallSrvDesc{};
	monsterBallSrvDesc.Format = monsterBallMetadata.format;
	monsterBallSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	monsterBallSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	monsterBallSrvDesc.Texture2D.MipLevels = UINT(monsterBallMetadata.mipLevels);

	// monsterBall用のSRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE monsterBallTextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);

	D3D12_GPU_DESCRIPTOR_HANDLE monsterBallTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);

	// monsterBall用SRVの生成
	device->CreateShaderResourceView(monsterBallTextureResource.Get(), &monsterBallSrvDesc, monsterBallTextureSrvHandleCPU);

	// checkerBoardのmetadataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC checkerBoardSrvDesc{};
	checkerBoardSrvDesc.Format = checkerBoardMetadata.format;
	checkerBoardSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	checkerBoardSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	checkerBoardSrvDesc.Texture2D.MipLevels = UINT(checkerBoardMetadata.mipLevels);
	
	// checkerBoard用のSRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE checkerBoardTextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 4);
	
	D3D12_GPU_DESCRIPTOR_HANDLE checkerBoardTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 4);
	
	// checkerBoard用SRVの生成
	device->CreateShaderResourceView(
		checkerBoardTextureResource.Get(),
		&checkerBoardSrvDesc,
		checkerBoardTextureSrvHandleCPU
	);

	// white1x1のmetadataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC white1x1SrvDesc{};
	white1x1SrvDesc.Format = white1x1Metadata.format;
	white1x1SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	white1x1SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	white1x1SrvDesc.Texture2D.MipLevels = UINT(white1x1Metadata.mipLevels);

	// white1x1用のSRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE white1x1TextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 5);

	D3D12_GPU_DESCRIPTOR_HANDLE white1x1TextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 5);

	// white1x1用SRVの生成
	device->CreateShaderResourceView(white1x1TextureResource.Get(), &white1x1SrvDesc, white1x1TextureSrvHandleCPU);

	// fenceのmetadataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC fenceSrvDesc{};
	fenceSrvDesc.Format = fenceMetadata.format;
	fenceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	fenceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	fenceSrvDesc.Texture2D.MipLevels = UINT(fenceMetadata.mipLevels);

	// fence用のSRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE fenceTextureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 6);

	D3D12_GPU_DESCRIPTOR_HANDLE fenceTextureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 6);

	// fence用SRVの生成
	device->CreateShaderResourceView(fenceTextureResource.Get(), &fenceSrvDesc, fenceTextureSrvHandleCPU);

	// ===========================================================
	// Instancing用SRVの作成
	// ===========================================================

	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};

	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(TransformationMatrix);

	// DescriptorHeapの7番に登録
	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 7);

	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 7);

	// SRVを生成
	device->CreateShaderResourceView(instancingResource.Get(), &instancingSrvDesc, instancingSrvHandleCPU);

	// 読み込み済みモデル一覧
	std::vector<ModelResource> modelResources;

	ModelMaterialResource uvCheckerMaterial{};
	uvCheckerMaterial.name = "uvChecker";
	uvCheckerMaterial.textureResource = textureResource;
	uvCheckerMaterial.textureSrvHandleGPU = textureSrvHandleGPU;

	ModelMaterialResource planeMaterial{};
	planeMaterial.name = "planeTexture";
	planeMaterial.textureResource = textureResource2;
	planeMaterial.textureSrvHandleGPU = textureSrvHandleGPU2;

	ModelMaterialResource monsterBallMaterial{};
	monsterBallMaterial.name = "monsterBall";
	monsterBallMaterial.textureResource = monsterBallTextureResource;
	monsterBallMaterial.textureSrvHandleGPU = monsterBallTextureSrvHandleGPU;

	ModelMaterialResource checkerBoardMaterial{};
	checkerBoardMaterial.name = "checkerBoard";
	checkerBoardMaterial.textureResource = checkerBoardTextureResource;
	checkerBoardMaterial.textureSrvHandleGPU = checkerBoardTextureSrvHandleGPU;

	ModelMaterialResource white1x1Material{};
	white1x1Material.name = "white1x1";
	white1x1Material.textureResource = white1x1TextureResource;
	white1x1Material.textureSrvHandleGPU = white1x1TextureSrvHandleGPU;

	ModelMaterialResource fenceMaterial{};
	fenceMaterial.name = "fence";
	fenceMaterial.textureResource = fenceTextureResource;
	fenceMaterial.textureSrvHandleGPU = fenceTextureSrvHandleGPU;


	modelResources.push_back(
		CreateModelResource(
			device,
			"plane",
			modelData,
			std::vector<ModelMaterialResource>{planeMaterial}
		)
	);
	
	modelResources.push_back(
		CreateModelResource(
			device,
			"sphere",
			sphereModelData,
			std::vector<ModelMaterialResource>{uvCheckerMaterial}
		)
	);
	
	modelResources.push_back(
		CreateModelResource(
			device,
			"multiMesh",
			multiMeshModelData,
			std::vector<ModelMaterialResource>{uvCheckerMaterial}
		)
	);
	
	modelResources.push_back(
		CreateModelResource(
			device,
			"multiMaterial",
			multiMaterialModelData,
			std::vector<ModelMaterialResource>{uvCheckerMaterial, monsterBallMaterial}
		)
	);

	modelResources.push_back(
		CreateModelResource(
			device,
			"teapot",
			teapotModelData,
			std::vector<ModelMaterialResource>{checkerBoardMaterial}
		)
	);
	
	modelResources.push_back(
		CreateModelResource(
			device,
			"bunny",
			bunnyModelData,
			std::vector<ModelMaterialResource>{uvCheckerMaterial}
		)
	);
	
	modelResources.push_back(
		CreateModelResource(
			device,
			"suzanne",
			suzanneModelData,
			std::vector<ModelMaterialResource>{white1x1Material}
		)
	);

	modelResources.push_back(
		CreateModelResource(
			device,
			"fence",
			fenceModelData,
			std::vector<ModelMaterialResource>{fenceMaterial}
		)
	);


	// ===========================================================
	// ImGuiの初期化
	// ===========================================================

	#ifdef USE_IMGUI



	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(
		device.Get(), swapChainDesc.BufferCount, rtvDesc.Format, srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);
	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->Build();

	#endif // USE_IMGUI


	// ===========================================================
	// XAudio2の初期化
	// ===========================================================

	// XAudio2エンジンのインスタンスを生成
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	hr = XAudio2Create(xAudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(hr));

	// マスターボイスを生成
	IXAudio2MasteringVoice *masterVoice = nullptr;
	hr = xAudio2->CreateMasteringVoice(&masterVoice);
	assert(SUCCEEDED(hr));

	// 音声読み込み
	SoundData soundData1 = SoundLoadWave("resources/Alarm01.wav");

	// 音声再生
	SoundPlayWave(xAudio2.Get(), soundData1);



	// Inputの初期化
	Input::GetInstance()->Initialize(wc.hInstance, hwnd);


	// ===========================================================
	// ゲーム用変数の宣言
	// ===========================================================


	Transform transform = {};
	transform.scale = {1.0f, 1.0f, 1.0f};

	Transform transformSprite = {};
	transformSprite.scale = {1.0f, 1.0f, 1.0f};
	transformSprite.rotate = {0.0f, 0.0f, 0.0f};
	transformSprite.translate = {0.0f, 0.0f, 0.0f};

	Transform uvTransformSprite = {};
	uvTransformSprite.scale = {1.0f, 1.0f, 1.0f};
	uvTransformSprite.rotate = {0.0f, 0.0f, 0.0f};
	uvTransformSprite.translate = {0.0f, 0.0f, 0.0f};

	// ライティング方式
	int lightingType = LIGHTING_HALF_LAMBERT;

	const char *lightingTypeItems[] = {
		"None",
		"Lambert",
		"Half Lambert",
	};

	DebugCamera *debugCamera = new DebugCamera;

	#ifdef USE_IMGUI

	// ImGuiで選択中のモデル
	int32_t selectedModelIndex = 0;

	#endif

	// 生成済みオブジェクト一覧
	std::vector<Object3DInstance> objectInstances;

	// 初期配置のオブジェクト
	Transform initialObjectTransform{};
	initialObjectTransform.scale = {1.0f, 1.0f, 1.0f};
	initialObjectTransform.rotate = {0.0f, 0.0f, 0.0f};
	initialObjectTransform.translate = {0.0f, 0.0f, 0.0f};

	objectInstances.push_back(CreateObject3DInstance(device, 0, initialObjectTransform));

	// パーティクルに初期速度を与える
	for (uint32_t i = 0; i < kNumInstance; ++i) {
		particles[i].velocty = {0.0f, 1.0f, 0.0f};
	}

	const float kDeltaTime = 1.0f / 60.0f;

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

			// 入力更新
			Input::GetInstance()->Update();


			// ===========================================================
			// ImGuiの準備
			// ===========================================================

			#ifdef USE_IMGUI

			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			#endif // USE_IMGUI

			// ===========================================================
			// ここからゲームの処理
			// ===========================================================


			//
			// 更新処理
			//

			// 数字の0キーが押されていたら出力ウィンドウに「Press 0」と表示
			if (Input::GetInstance()->IsPressKey(DIK_0)) {
				OutputDebugStringA("Press 0\n");
			}

			// 数字の0キーを押した瞬間なら出力ウィンドウに「Trigger 0」と表示
			if (Input::GetInstance()->IsTriggerKey(DIK_0)) {
				OutputDebugStringA("Trigger 0\n");
			}

			// 数字の0キーを離した瞬間なら出力ウィンドウに「Release 0」と表示
			if (Input::GetInstance()->IsReleaseTriggerKey(DIK_0)) {
				OutputDebugStringA("Release 0\n");
			}

			// Spaceキーを押した瞬間なら音声を再生
			if (Input::GetInstance()->IsTriggerKey(DIK_SPACE)) {
				SoundPlayWave(xAudio2.Get(), soundData1);
			}

			// GamePadのAボタンを押した瞬間なら音声を再生
			if (Input::GetInstance()->IsTriggerGamePadButton(XINPUT_GAMEPAD_A)) {
				SoundPlayWave(xAudio2.Get(), soundData1);
			}

			#ifdef USE_IMGUI


			ImGui::Begin("Texture Color");
			// サウンドのキー説明
			ImGui::Text("Press SPACE key : Play Sound");

			// ゲームパッドのキー説明
			ImGui::SeparatorText("GamePad");
			ImGui::Text("GamePad : %s", Input::GetInstance()->IsGamePadConnected() ? "Connected" : "Not Connected");
			ImGui::Text("Left Stick : Pivot Rotate Camera");
			ImGui::Text("LT / RT : Move Camera Z");
			ImGui::Text("D-Pad : Move Camera XY");

			ImGui::SeparatorText("Model Create");

			std::vector<const char *> modelNames;

			for (ModelResource &modelResource : modelResources) {
				modelNames.push_back(modelResource.name.c_str());
			}

			if (selectedModelIndex < 0 || selectedModelIndex >= static_cast<int32_t>(modelResources.size())) {
				selectedModelIndex = 0;
			}

			ImGui::Combo("Create Model", &selectedModelIndex, modelNames.data(), static_cast<int32_t>(modelNames.size()));

			if (ImGui::Button("Create")) {
				Transform newObjectTransform{};
				newObjectTransform.scale = {1.0f, 1.0f, 1.0f};
				newObjectTransform.rotate = {0.0f, 0.0f, 0.0f};

				// 生成するたびに少し横へずらす
				newObjectTransform.translate = {static_cast<float>(objectInstances.size()) * 1.5f, 0.0f, 0.0f};

				objectInstances.push_back(CreateObject3DInstance(device, selectedModelIndex, newObjectTransform));
			}

			ImGui::SeparatorText("Object List");

			for (int32_t objectIndex = 0; objectIndex < static_cast<int32_t>(objectInstances.size()); ++objectIndex) {
				Object3DInstance &object = objectInstances[objectIndex];
				ModelResource &model = modelResources[object.modelIndex];

				ImGui::PushID(objectIndex);

				std::string treeLabel = std::format("Object {} : {}", objectIndex, model.name);

				bool shouldDelete = false;

				if (ImGui::TreeNode(treeLabel.c_str())) {
					ImGui::ColorEdit4("Color", &object.color.x);
					ImGui::Combo("Lighting Type", &object.lightingType, lightingTypeItems, _countof(lightingTypeItems));

					const char *blendModeItems[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen",};

					ImGui::Combo("Blend Mode", &object.blendMode, blendModeItems, _countof(blendModeItems));

					ImGui::DragFloat3("Translate", &object.transform.translate.x, 0.01f);
					ImGui::DragFloat3("Rotate", &object.transform.rotate.x, 0.01f);
					ImGui::DragFloat3("Scale", &object.transform.scale.x, 0.01f);

					if (ImGui::TreeNode("UVTransform")) {
						ImGui::DragFloat2("UV Translate", &object.uvTransform.translate.x, 0.01f, -10.0f, 10.0f);
						ImGui::DragFloat2("UV Scale", &object.uvTransform.scale.x, 0.01f, -10.0f, 10.0f);
						ImGui::SliderAngle("UV Rotate", &object.uvTransform.rotate.z);

						ImGui::TreePop();
					}

					if (ImGui::Button("Delete")) {
						shouldDelete = true;
					}

					ImGui::TreePop();
				}

				ImGui::PopID();

				if (shouldDelete) {
					objectInstances.erase(objectInstances.begin() + objectIndex);
					break;
				}
			}

			if (ImGui::TreeNode("Sprite")) {
				ImGui::DragFloat3("Translate##Sprite", &transformSprite.translate.x, 0.1f);
				ImGui::DragFloat3("Rotate##Sprite", &transformSprite.rotate.x, 0.01f);
				ImGui::DragFloat3("Scale##Sprite", &transformSprite.scale.x, 0.01f);
				ImGui::TreePop();

				if (ImGui::TreeNode("UVTransform")) {
					ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
					ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
					ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
					ImGui::TreePop();
				}
			}

			ImGui::Combo("Lighting Type", &lightingType, lightingTypeItems, _countof(lightingTypeItems));

			ImGui::ColorEdit4("Light Color", &directionalLightData->color.x);
			ImGui::DragFloat3("Light Direction", &directionalLightData->direction.x, 0.01f);
			ImGui::DragFloat("Light Intensity", &directionalLightData->intensity, 0.01f);

			ImGui::End();

			#endif // USE_IMGUI

			// ライティングタイプの適用
			materialData->lightingType = lightingType;


			debugCamera->Update();

			for (Object3DInstance &object : objectInstances) {
				Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(object.transform);
				Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, debugCamera->GetViewProjectionMatrix());

				object.wvpData->WVP = worldViewProjectionMatrix;
				object.wvpData->World = worldMatrix;

				object.materialData->color = object.color;
				object.materialData->lightingType = object.lightingType;

				Matrix4x4 objectUvTransformMatrix = Matrix4x4::MakeScaleMatrix(object.uvTransform.scale);
				objectUvTransformMatrix = Matrix4x4::Multiply(objectUvTransformMatrix, Matrix4x4::MakeRotateZMatrix(object.uvTransform.rotate.z));
				objectUvTransformMatrix = Matrix4x4::Multiply(objectUvTransformMatrix, Matrix4x4::MakeTranslateMatrix(object.uvTransform.translate));

				object.materialData->uvTransform = objectUvTransformMatrix;
			}

			// ===========================================================
			// Particleの更新処理
			// ===========================================================

			
			for (uint32_t i = 0; i < kNumInstance; ++i) {
				// 速度の反映
				particles[i].transform.translate += particles[i].velocty * kDeltaTime;

				// 各インスタンスのTransformを行列に変換する
				// World行列
				Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(particles[i].transform);

				// WorldViewProjection行列
				Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, debugCamera->GetViewProjectionMatrix());

				// Instancing用Resourceに書き込む
				instancingData[i].WVP = worldViewProjectionMatrix;
				instancingData[i].World = worldMatrix;
			}

			// Sprite用のWorldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixSprite = Matrix4x4::MakeAffineMatrix(transformSprite);
			Matrix4x4 viewMatrixSprite = Matrix4x4::MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = Matrix4x4::MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = Matrix4x4::Multiply(worldMatrixSprite, Matrix4x4::Multiply(viewMatrixSprite, projectionMatrixSprite));

			transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
			transformationMatrixDataSprite->World = worldMatrixSprite;

			Matrix4x4 uvTransformMatrix = Matrix4x4::MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Matrix4x4::Multiply(uvTransformMatrix, Matrix4x4::MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Matrix4x4::Multiply(uvTransformMatrix, Matrix4x4::MakeTranslateMatrix(uvTransformSprite.translate));

			materialDataSprite->uvTransform = uvTransformMatrix;

			// Lightの方向は正規化しておく
			directionalLightData->direction = Vector3::Normalize(directionalLightData->direction);


			#ifdef USE_IMGUI

			// ImGuiの内部コマンドを生成する
			ImGui::Render();

			#endif // USE_IMGUI


			//
			// 更新処理終了
			//



			// ===========================================================
			// ここまでゲームの処理
			// ===========================================================


			// 次フレーム用のコマンドリストを準備
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator.Get(), nullptr);
			assert(SUCCEEDED(hr));

			// これから書き込むバックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};

			// 今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

			// Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

			// バリアを張る対象のリソース。現在バックバッファに対して行う
			barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();

			// 遷移前のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;

			// 遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			// 描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

			// 指定した色で画面全体をクリアする
			float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f}; // 青っぽい色
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

			// 指定した深度で画面全体をクリアする
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// 描画用DescriptorHeapの設定
			ID3D12DescriptorHeap *descriptorHeaps[] = {srvDescriptorHeap.Get()};
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			commandList->RSSetViewports(1, &viewport); // Viewportを設定

			commandList->RSSetScissorRects(1, &scissorRect); // Scissorを設定

			// RootSignatureを設定。PSOに設定しているけど別途設定が必要
			commandList->SetGraphicsRootSignature(rootSignature.Get());

			// 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			for (Object3DInstance &object : objectInstances) {
				ModelResource &model = modelResources[object.modelIndex];

				assert(0 <= object.blendMode);
				assert(object.blendMode < BLEND_MODE_COUNT);

				// BlendModeごとにPSOを切り替える
				commandList->SetPipelineState(graphicsPipelineStates[object.blendMode].Get());

				// モデル用Materialを設定
				commandList->SetGraphicsRootConstantBufferView(0, object.materialResource->GetGPUVirtualAddress());

				// モデル用WVPを設定
				commandList->SetGraphicsRootConstantBufferView(1, object.wvpResource->GetGPUVirtualAddress());

				// DirectionalLightを設定
				commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

				for (MeshResource &mesh : model.meshes) {
					assert(mesh.materialIndex < model.materials.size());

					ModelMaterialResource &material = model.materials[mesh.materialIndex];

					commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);

					// Meshごとにマテリアル、今回はテクスチャを切り替える
					commandList->SetGraphicsRootDescriptorTable(2, material.textureSrvHandleGPU);

					// 描画
					commandList->DrawInstanced(UINT(mesh.vertices.size()), 1, 0, 0);
				}
			}

			// ===========================================================
			// Particleの描画処理
			// ===========================================================

			// Particle用RootSignatureを設定
			commandList->SetGraphicsRootSignature(particleRootSignature.Get());

			// Particle用PSOを設定
			commandList->SetPipelineState(particlePipelineState.Get());

			// 三角形として描画
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// Particle用VertexBufferを設定
			commandList->IASetVertexBuffers(0, 1, &particleMeshResource.vertexBufferView);

			// Material用CBVを設定
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

			// Instancing用StructuredBufferのSRVを設定
			commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU);

			// Texture用SRVを設定
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

			// 6頂点の四角形を10個分Instancing描画
			commandList->DrawInstanced(UINT(particleMeshResource.vertices.size()), kNumInstance, 0, 0);

			// Sprite描画用にRootSignatureを戻す
			commandList->SetGraphicsRootSignature(rootSignature.Get());

			// Sprite用BlendModeをNormalに戻す
			commandList->SetPipelineState(graphicsPipelineStates[BLEND_MODE_NORMAL].Get());

			// Sprite用TextureをuvCheckerに戻す
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

			// Sprite用Materialに切り替える
			commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());

			// Spriteの描画。変更が必要なものだけ変更する
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite); // VBVを設定
			commandList->IASetIndexBuffer(&indexBufferViewSprite);

			// TransformationMatrixCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(
				1,
				transformationMatrixResourceSprite->GetGPUVirtualAddress()
			);

			// 描画
			commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);


			#ifdef USE_IMGUI

			// 実際のcommandListのImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

			#endif // USE_IMGUI



			// 画面に描く処理は終わり、画面に映すので状態を遷移
			// 今回はRenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);


			// コマンドリストの内容を確定させる
			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			// GPUにコマンドリストの実行を行わせる
			commandQueue->ExecuteCommandLists(1, commandLists);

			// GPUとOSに画面に画面の交換を行うよう通知する
			swapChain->Present(1, 0);

			// Fenceの値を更新
			fenceValue++;

			// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal(fence.Get(), fenceValue);

			// Fenceの値が指定したSignal値にたどり着いているか確認する
			// GetCompletedValueの初期値はFence作成時に渡した初期値
			if (fence->GetCompletedValue() < fenceValue) {

				// 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);

				// イベント待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}
		}
	}

	#ifdef USE_IMGUI

	// ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	#endif // USE_IMGUI

	// XAudio2解放
	xAudio2.Reset();

	// 音声データ解放
	SoundUnload(&soundData1);

	// デバッグカメラの開放
	delete debugCamera;


	Log(log, ConvertString(std::format(L"\nlog\n")));

	CoUninitialize();
	CloseHandle(fenceEvent);
	CloseWindow(hwnd);


	return 0;
}