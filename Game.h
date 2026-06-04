#pragma once
#include"Matrix4x4.h"

enum class DEMO_MODE {
	CHECK_MODE, // ImGuiでの三角形の制御
	EFFECT_MODE // 映像演出
};

class Game {
  public:
	Game();

	void Initialize();
	void Update();
	void Draw();

	~Game();
};
