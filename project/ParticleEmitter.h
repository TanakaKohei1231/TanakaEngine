
#pragma once
#include "ParticleSystem.h"

class ParticleEmitter {
  public:
	void Initialize(const Vector3 &position, uint32_t count, float frequency);

	// 定期発生
	void Update(float deltaTime, ParticleSystem &particleSystem);

	// 即時発生
	uint32_t Emit(ParticleSystem &particleSystem);

	ParticleSpawnSettings &GetSpawnSettings() { return spawnSettings_; }

	void SetCount(uint32_t count) { count_ = count; }

	void SetFrequency(float frequency) {
		if (frequency > 0.0f) {
			frequency_ = frequency;
		} else {
			frequency_ = 0.0f;
		}
	}

	void SetPosition(const Vector3 &position) { transform_.translate = position; }

	const Transform &GetTransform() const { return transform_; }

	void SetEnabled(bool enabled) { enabled_ = enabled; }

	uint32_t GetCount() const { return count_; }

	float GetFrequency() const { return frequency_; }

	bool IsEnabled() const { return enabled_; }

  private:
	Transform transform_{};

	// 一度に生成する数
	uint32_t count_ = 3;

	// 発生間隔（秒）
	float frequency_ = 0.5f;

	// 発生間隔の累積時間
	float frequencyTime_ = 0.0f;

	// 粒子生成時の設定
	ParticleSpawnSettings spawnSettings_{};

	bool enabled_ = true;
};
