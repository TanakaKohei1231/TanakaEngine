#include "ParticleSystem.h"
#include "Ease.h"
#include <cassert>
#include <numbers>

void ParticleSystem::Initialize() {
	ParticleSpawnSettings settings{};

	for (uint32_t i = 0; i < kNumMaxInstance; ++i) {
		particles_[i] = MakeNewParticle(settings);
	}

	numAliveParticles_ = 0;
}

void ParticleSystem::Update(float deltaTime, const Matrix4x4 &viewProjectionMatrix, const Matrix4x4& cameraMatrix, ParticleForGPU *instancingData) {
	assert(instancingData != nullptr);

	// Billboard用の回転行列
	Matrix4x4 billboardMatrix = Matrix4x4::MakeIdentity4x4();

	if (useBillboard_) {
		// 板ポリの正面を合わせる
		Matrix4x4 backToFrontMatrix = Matrix4x4::MakeRotateYMatrix(std::numbers::pi_v<float>);

		// カメラの回転を適用
		billboardMatrix = Matrix4x4::Multiply(backToFrontMatrix, cameraMatrix);

		// 平行移動を取り除く
		billboardMatrix.m_[3][0] = 0.0f;
		billboardMatrix.m_[3][1] = 0.0f;
		billboardMatrix.m_[3][2] = 0.0f;
	}

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
		Matrix4x4 worldMatrix{};

		if (useBillboard_) {
			// Scale行列
			Matrix4x4 scaleMatrix = Matrix4x4::MakeScaleMatrix(particles_[i].transform.scale);

			// Z回転行列
			Matrix4x4 rotateZMatrix = Matrix4x4::MakeRotateZMatrix(particles_[i].transform.rotate.z);

			// Translate行列
			Matrix4x4 translateMatrix = Matrix4x4::MakeTranslateMatrix(particles_[i].transform.translate);

			worldMatrix = Matrix4x4::Multiply(Matrix4x4::Multiply(Matrix4x4::Multiply(scaleMatrix, rotateZMatrix), billboardMatrix), translateMatrix);

		} else {
			// 従来のWorld行列
			worldMatrix = Matrix4x4::MakeAffineMatrix(particles_[i].transform);
		}

		// WorldViewProjection行列
		Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);

		// 生存時間の進行度
		float t = particles_[i].currentTime / particles_[i].lifetime;

		// イージングを使用して透明度を計算
		float alpha = 1.0f - Ease::EaseIn(t);

		// 生存しているパーティクルだけを先頭から詰める
		instancingData[numAliveParticles_].WVP = worldViewProjectionMatrix;
		instancingData[numAliveParticles_].world = worldMatrix;
		instancingData[numAliveParticles_].color = particles_[i].color;
		instancingData[numAliveParticles_].color.w = particles_[i].color.w * alpha;

		++numAliveParticles_;
	}
}


void ParticleSystem::Draw(const ParticleDrawContext &context) const {
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

Particle ParticleSystem::MakeNewParticle(const ParticleSpawnSettings &settings) {
	Particle particle{};

	// -1.0～1.0の乱数
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

	// 0.0～1.0の乱数
	std::uniform_real_distribution<float> colorDistribution(0.0f, 1.0f);

	// 寿命の乱数
	std::uniform_real_distribution<float> lifetimeDistribution(settings.minLifetime, settings.maxLifetime);

	// 拡縮と回転
	particle.transform.scale = {1.0f, 1.0f, 1.0f};
	particle.transform.rotate = {0.0f, 0.0f, 0.0f};

	// 発生位置
	particle.transform.translate = {
		settings.position.x + distribution(randomEngine_) * settings.positionRange.x,
		settings.position.y + distribution(randomEngine_) * settings.positionRange.y,
		settings.position.z + distribution(randomEngine_) * settings.positionRange.z
	};

	// 初速度
	particle.velocity = {
		distribution(randomEngine_) * settings.velocityRange.x,
		distribution(randomEngine_) * settings.velocityRange.y,
		distribution(randomEngine_) * settings.velocityRange.z
	};

	// 色
	particle.color = {colorDistribution(randomEngine_), colorDistribution(randomEngine_), colorDistribution(randomEngine_), 1.0f};

	// 寿命
	particle.lifetime = lifetimeDistribution(randomEngine_);
	particle.currentTime = 0.0f;

	return particle;
}
