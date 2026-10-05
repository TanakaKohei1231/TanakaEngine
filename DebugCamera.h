#pragma once
#include"Vector3.h"
#include"Matrix4x4.h"
#include <Windows.h>
#include <Xinput.h>

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>

class DebugCamera {
  public:
	DebugCamera(const Vector3 &translation = defaultTranslation, const Vector3 &rotation = defaultRotation);

	void Initialize(const Vector3 &translation = defaultTranslation, const Vector3 &rotation = defaultRotation);
	void Update();

	const Matrix4x4 &GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

  private:

	// ローカル回転角
	Vector3 rotation_{};

	// ローカル座標
	Vector3 translation_ = {};

	// 累積回転行列
	Matrix4x4 matRot_{};

	// 射影行列
	Matrix4x4 projectionMatrix_{};

	Matrix4x4 viewProjectionMatrix_{};

	// キーボード・マウスでの操作
	bool UpdateKeyMouseMove();

	// ゲームパッドでの操作
	bool UpdateGamePadMove();

	// 行列の更新
	void UpdateMatrix();

	// 画面の大きさ
	static inline const float kWindowWidth = 1280.0f;
	static inline const float kWindowHeight = 720.0f;

	// デフォルト引数の数値
	static inline const Vector3 defaultTranslation = {0.0f, 0.0f, -50.0f};
	static inline const Vector3 defaultRotation = {0.0f, 0.0f, 0.0f};

	// 移動速度
	static inline const float kMoveSpeed = 0.1f;

	// ホイール移動速度
	static inline const float kWheelMoveSpeed = 0.01f;

	// 回転速度
	static inline const float kRotateSpeed = 0.005f;

	// ゲームパッドによる移動速度
	static inline const float kGamePadMoveSpeed = 0.1f;

	// ゲームパッドによるZ移動速度
	static inline const float kGamePadZMoveSpeed = 0.2f;

	// ゲームパッドによる回転速度
	static inline const float kGamePadRotateSpeed = 0.03f;
};
