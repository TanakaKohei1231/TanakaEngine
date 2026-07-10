#include "DebugCamera.h"
#include "Input.h"

DebugCamera::DebugCamera(
	const BYTE *key, const BYTE *preKey,
	const DIMOUSESTATE2 *mouseState, const DIMOUSESTATE2 *preMouseState,
	const Vector3 &translation, const Vector3 &rotation
) {
	key_ = key;
	preKey_ = preKey;
	mouseState_ = mouseState;
	preMouseState_ = preMouseState;
	Initialize(translation,rotation);
}

void DebugCamera::Initialize(const Vector3 &translation, const Vector3 &rotation) {
	projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(0.45f, float(kWindowWidth) / float(kWindowHeight), 0.1f, 100.0f);
	translation_ = translation;
	rotation_ = rotation;

	// 初期回転行列
	matRot_ = Matrix4x4::MakeRotateXYZMatrix(rotation_);

	// 行列の更新
	UpdateMatrix();
}

void DebugCamera::Update() {
	bool isMoveCamera = false;

	// Wキーで上移動
	if (IsPressKey(key_, DIK_W)) {
		isMoveCamera = true;
		translation_.y += kMoveSpeed;
	}

	// Aキーで左移動
	if (IsPressKey(key_, DIK_A)) {
		isMoveCamera = true;
		translation_.x -= kMoveSpeed;
	}

	// Sキーで下移動
	if (IsPressKey(key_, DIK_S)) {
		isMoveCamera = true;
		translation_.y -= kMoveSpeed;
	}

	// Dキーで右移動
	if (IsPressKey(key_, DIK_D)) {
		isMoveCamera = true;
		translation_.x += kMoveSpeed;
	}

	// マウスホイールでZ軸前後移動
	if (mouseState_->lZ != 0) {
		isMoveCamera = true;
		translation_.z += static_cast<float>(mouseState_->lZ) * kWheelMoveSpeed;
	}

	// 左クリック中にマウス移動で回転
	if (mouseState_->rgbButtons[0] & 0x80) {
		float rotateX = static_cast<float>(mouseState_->lY) * kRotateSpeed;
		float rotateY = static_cast<float>(mouseState_->lX) * kRotateSpeed;

		if (rotateX != 0.0f || rotateY != 0.0f) {
			isMoveCamera = true;

			// 追加回転分の回転行列を生成
			Matrix4x4 matRotDelta = Matrix4x4::MakeIdentity4x4();
			matRotDelta = Matrix4x4::Multiply(matRotDelta, Matrix4x4::MakeRotateXMatrix(rotateX));
			matRotDelta = Matrix4x4::Multiply(matRotDelta, Matrix4x4::MakeRotateYMatrix(rotateY));

			// 累積回転行列を合成
			matRot_ = Matrix4x4::Multiply(matRotDelta, matRot_);
		}
	}

	// カメラが移動していたら行列を更新する
	if (isMoveCamera) {
		UpdateMatrix();
	}
}

void DebugCamera::UpdateMatrix() {
	// ピボット回転用
	// 先にカメラをZ方向などへ移動し、その後に回転を掛けることで、原点周りを回り込む
	Matrix4x4 translateMatrix = Matrix4x4::MakeTranslateMatrix(translation_);
	Matrix4x4 worldMatrix = Matrix4x4::Multiply(translateMatrix, matRot_);

	Matrix4x4 viewMatrix = Matrix4x4::Inverse(worldMatrix);
	viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix, projectionMatrix_);
}