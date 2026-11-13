#include "ParticleSystem.h"
#include <cassert>
#include <random>

void ParticleSystem::Initialize() {
	for (uint32_t i = 0; i < kNumMaxInstance; ++i) {
		particles_[i].transform.scale = {1.0f, 1.0f, 1.0f};
		particles_[i].transform.rotate = {0.0f, 0.0f, 0.0f};
		particles_[i].transform.translate = {i * 0.1f, i * 0.1f, i * 0.1f};
		particles_[i].velocity = {0.0f, 1.0f, 0.0f};
		particles_[i].lifetime = 5.0f;
		particles_[i].currentTime = 0.0f;
	}
}

void ParticleSystem::Update(float deltaTime, const Matrix4x4 &viewProjectionMatrix, ParticleForGPU *instancingData) {
	assert(instancingData != nullptr);

	numAliveParticles_ = 0;

	for (uint32_t i = 0; i < kNumMaxInstance; ++i) {
		particles_[i].currentTime += deltaTime;

		// パーティクルの寿命が尽きていたら更新しない
		if (particles_[i].currentTime >= particles_[i].lifetime) {
			continue;
		}


		// 速度の反映
		particles_[i].transform.translate += particles_[i].velocity * deltaTime;

		// 各インスタンスのTransformを行列に変換する
		// World行列
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(particles_[i].transform);

		// WorldViewProjection行列
		Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);

		// 生存しているパーティクルだけを先頭から詰める
		instancingData[numAliveParticles_].WVP = worldViewProjectionMatrix;

		instancingData[numAliveParticles_].world = worldMatrix;

		instancingData[numAliveParticles_].color = particles_[i].color;

		++numAliveParticles_;
	}
}


void ParticleSystem::Draw(const ParticleDrawContext &context) {
	ID3D12GraphicsCommandList *commandList = context.commandList;

	assert(commandList != nullptr);
	assert(context.rootSignature != nullptr);
	assert(context.pipelineState != nullptr);

	// 生存しているパーティクルがなければ描画しない
	if (numAliveParticles_ == 0) {
		return;
	}

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
	commandList->DrawInstanced(context.vertexCount, numAliveParticles_, 0, 0);
}
