#pragma once
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include <cstdint>
#include <d3d12.h>
#include <array>

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

class ParticleSystem {
  public:
	void Initialize();

	void Update(float deltaTime, const Matrix4x4 &viewProjectionMatrix, ParticleForGPU *instancingData);

	void Draw(const ParticleDrawContext &context);

	// インスタンス数
	static inline const uint32_t kNumMaxInstance = 10;

  private:
	std::array<Particle, kNumMaxInstance> particles_;

	// 描画対象のパーティクル数
	uint32_t numAliveParticles_ = 0;
};
