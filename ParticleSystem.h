#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"
#include <cstdint>
#include <d3d12.h>
#include <array>

struct Particle {
	Transform transform;
	Vector3 velocity;
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

class ParticleSystem {
  public:
	void Initialize();

	void Update(float deltaTime, const Matrix4x4 &viewProjectionMatrix, TransformationMatrix *instancingData);

	void Draw(const ParticleDrawContext &context);

	// インスタンス数
	static inline const uint32_t kNumInstance = 10;

  private:
	std::array<Particle, kNumInstance> particles_;
};
