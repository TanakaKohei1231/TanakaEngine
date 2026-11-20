#include "ParticleSystem.h"
#include "Ease.h"
#include <cassert>
#include <numbers>
#include <algorithm>

void ParticleSystem::Initialize() {
	// 全パーティクルを未使用状態にする
	for (Particle &particle : particles_) {
		particle = {};
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
		// 未使用スロットなら処理しない
		if (!particles_[i].isActive) {
			continue;
		}

		// 経過時間を更新
		particles_[i].currentTime += deltaTime;

		// 寿命が尽きたらスロットを解放
		if (particles_[i].currentTime >= particles_[i].lifetime) {
			particles_[i].isActive = false;
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

		// GPUの最大描画数を超える場合は転送しない
		if (numAliveParticles_ >= kNumMaxInstance) {
			continue;
		}

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


uint32_t ParticleSystem::Emit(const ParticleSpawnSettings &settings, uint32_t count) {
	uint32_t emittedCount = 0;

	for (Particle &particle : particles_) {
		// 必要数に達したら終了
		if (emittedCount >= count) {
			break;
		}

		// 使用中のスロットは飛ばす
		if (particle.isActive) {
			continue;
		}

		// 空きスロットに新しい粒子を生成
		particle = MakeNewParticle(settings);

		++emittedCount;
	}

	return emittedCount;
}


Particle ParticleSystem::MakeNewParticle(const ParticleSpawnSettings &settings) {
	Particle particle{};

	// -1.0～1.0の乱数
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

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
		settings.baseVelocity.x + distribution(randomEngine_) * settings.velocityRange.x,
		settings.baseVelocity.y + distribution(randomEngine_) * settings.velocityRange.y,
		settings.baseVelocity.z + distribution(randomEngine_) * settings.velocityRange.z
	};

	// RGBを基準色からランダムに変化させる
	particle.color = {
		std::clamp(settings.baseColor.x + distribution(randomEngine_) * settings.colorRange.x, 0.0f, 1.0f),
		std::clamp(settings.baseColor.y + distribution(randomEngine_) * settings.colorRange.y, 0.0f, 1.0f),
		std::clamp(settings.baseColor.z + distribution(randomEngine_) * settings.colorRange.z, 0.0f, 1.0f),
		settings.baseColor.w
	};


	// 寿命
	particle.lifetime = lifetimeDistribution(randomEngine_);
	particle.currentTime = 0.0f;

	particle.isActive = true;

	return particle;
}
