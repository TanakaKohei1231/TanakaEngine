#include "ParticleSystem.h"
#include <cassert>

void ParticleSystem::Initialize() {
	for (uint32_t i = 0; i < kNumInstance; ++i) {
		particles_[i].transform.scale = {1.0f, 1.0f, 1.0f};
		particles_[i].transform.rotate = {0.0f, 0.0f, 0.0f};
		particles_[i].transform.translate = {i * 0.1f, i * 0.1f, i * 0.1f};
		particles_[i].velocity = {0.0f, 1.0f, 0.0f};
	}
}

void ParticleSystem::Update(float deltaTime, const Matrix4x4 &viewProjectionMatrix, TransformationMatrix *instancingData) {
	assert(instancingData != nullptr);

	for (uint32_t i = 0; i < kNumInstance; ++i) {
		// 速度の反映
		particles_[i].transform.translate += particles_[i].velocity * deltaTime;

		// 各インスタンスのTransformを行列に変換する
		// World行列
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(particles_[i].transform);

		// WorldViewProjection行列
		Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);

		// Instancing用Resourceに書き込む
		instancingData[i].WVP = worldViewProjectionMatrix;
		instancingData[i].World = worldMatrix;
	}
}


void ParticleSystem::Draw(const ParticleDrawContext &context) {
	ID3D12GraphicsCommandList *commandList = context.commandList;

	assert(commandList != nullptr);
	assert(context.rootSignature != nullptr);
	assert(context.pipelineState != nullptr);

	// Particle用RootSignatureを設定
	commandList->SetGraphicsRootSignature(context.rootSignature);

	// Particle用PSOを設定
	commandList->SetPipelineState(context.pipelineState);

	// 三角形として描画
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Particle用VertexBufferを設定
	commandList->IASetVertexBuffers(0, 1, &context.vertexBufferView);

	// Material用CBVを設定
	commandList->SetGraphicsRootConstantBufferView(0, context.materialBufferAddress);

	// Instancing用SRVを設定
	commandList->SetGraphicsRootDescriptorTable(1, context.instancingSrvHandleGPU);

	// Texture用SRVを設定
	commandList->SetGraphicsRootDescriptorTable(2, context.textureSrvHandleGPU);

	// Instancing描画
	commandList->DrawInstanced(context.vertexCount, kNumInstance, 0, 0);
}
