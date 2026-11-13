#pragma once
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include <cstdint>
#include <d3d12.h>
#include <array>
#include <random>

struct Particle {
	Transform transform{};
	Vector3 velocity{};
	Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
	float lifetime = 0.0f;
	float currentTime = 0.0f;
};

// パーティクルの描画に必要な情報
struct ParticleDrawContext {
	ID3D12GraphicsCommandList *commandList = nullptr;
	ID3D12RootSignature *rootSignature = nullptr;
	ID3D12PipelineState *pipelineState = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	D3D12_GPU_VIRTUAL_ADDRESS materialBufferAddress = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU{};
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU{};

	uint32_t vertexCount = 0;
};

struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 world;
	Vector4 color;
};

// パーティクルの発生設定
struct ParticleSpawnSettings {
	Vector3 position{0.0f, 0.0f, 0.0f};

	// 発生位置のばらつき
	Vector3 positionRange{1.0f, 1.0f, 1.0f};

	// 初速度のばらつき
	Vector3 velocityRange{1.0f, 1.0f, 1.0f};

	// 寿命
	float minLifetime = 1.0f;
	float maxLifetime = 5.0f;
};

class ParticleSystem {
  public:
	void Initialize();

	void Update(float deltaTime, const Matrix4x4 &viewProjectionMatrix, const Matrix4x4& cameraMatrix, ParticleForGPU *instancingData);

	void Draw(const ParticleDrawContext &context) const;


	// Billboardの有効・無効
	void SetBillboardEnabled(bool enabled) { useBillboard_ = enabled; }

	bool IsBillboardEnabled() const { return useBillboard_; }

	// インスタンス数
	static inline const uint32_t kNumMaxInstance = 10;

  private:
	std::array<Particle, kNumMaxInstance> particles_;

	// 描画対象のパーティクル数
	uint32_t numAliveParticles_ = 0;

	Particle MakeNewParticle(const ParticleSpawnSettings &settings);

	std::mt19937 randomEngine_{std::random_device{}()};

	// Billboardを使用するか
	bool useBillboard_ = true;
};
