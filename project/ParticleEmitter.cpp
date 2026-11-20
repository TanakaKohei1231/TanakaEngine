
#include "ParticleEmitter.h"
#include <cassert>

void ParticleEmitter::Initialize(const Vector3 &position, uint32_t count, float frequency) {
	assert(frequency > 0.0f);

	transform_.scale = {1.0f, 1.0f, 1.0f};
	transform_.rotate = {0.0f, 0.0f, 0.0f};
	transform_.translate = position;

	count_ = count;
	frequency_ = frequency;
	frequencyTime_ = 0.0f;

	spawnSettings_ = {};
	enabled_ = true;
}

uint32_t ParticleEmitter::Emit(ParticleSystem &particleSystem) {
	// エミッターの位置を生成基準にする
	ParticleSpawnSettings settings = spawnSettings_;
	settings.position = transform_.translate;

	return particleSystem.Emit(settings, count_);
}

void ParticleEmitter::Update(float deltaTime, ParticleSystem &particleSystem) {
	if (!enabled_) {
		return;
	}

	if (frequency_ <= 0.0f) {
		return;
	}

	frequencyTime_ += deltaTime;

	// 経過した発生間隔の回数だけ生成
	while (frequencyTime_ >= frequency_) {
		Emit(particleSystem);

		// 余剰時間を残す
		frequencyTime_ -= frequency_;
	}
}
