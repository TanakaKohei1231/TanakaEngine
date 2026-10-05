#include "DebugCamera.h"
#include "Input.h"

namespace {

float NormalizeStickValue(SHORT value, SHORT deadZone) {
	if (-deadZone < value && value < deadZone) {
		return 0.0f;
	}

	if (value < 0) {
		return static_cast<float>(value) / 32768.0f;
	}

	return static_cast<float>(value) / 32767.0f;
}

float NormalizeTriggerValue(BYTE value) {
	if (value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
		return 0.0f;
	}

	return static_cast<float>(value) / 255.0f;
}

} // namespace



DebugCamera::DebugCamera(const Vector3 &translation, const Vector3 &rotation) {
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
	// キーボード・マウスによるカメラ操作
	bool isKeyMouseMove = UpdateKeyMouseMove();

	// ゲームパッドによるカメラ操作
	bool isGamePadMove = UpdateGamePadMove();

	// カメラが移動していたら行列を更新する
	if (isKeyMouseMove || isGamePadMove) {
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


bool DebugCamera::UpdateKeyMouseMove() {
	bool result = false;

	// 移動量
	Vector3 move = {0.0f, 0.0f, 0.0f};

	Input *input = Input::GetInstance();

	// Wキーで上移動
	if (input->IsPressKey(DIK_W)) {
		move.y += kMoveSpeed;
	}

	// Aキーで左移動
	if (input->IsPressKey(DIK_A)) {
		move.x -= kMoveSpeed;
	}

	// Sキーで下移動
	if (input->IsPressKey(DIK_S)) {
		move.y -= kMoveSpeed;
	}

	// Dキーで右移動
	if (input->IsPressKey(DIK_D)) {
		move.x += kMoveSpeed;
	}

	DIMOUSESTATE2 mouseState = input->GetMouseState();

	// マウスホイールでZ軸前後移動
	if (mouseState.lZ != 0) {
		move.z += static_cast<float>(mouseState.lZ) * kWheelMoveSpeed;
	}

	if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f) {
		translation_ += move;
		result = true;
	}

	// 右クリック中にマウス移動で回転
	if (mouseState.rgbButtons[1] & 0x80) {
		float rotateX = static_cast<float>(mouseState.lY) * kRotateSpeed;
		float rotateY = static_cast<float>(mouseState.lX) * kRotateSpeed;

		if (rotateX != 0.0f || rotateY != 0.0f) {
			result = true;

			// 追加回転分の回転行列を生成
			Matrix4x4 matRotDelta = Matrix4x4::MakeIdentity4x4();
			matRotDelta = Matrix4x4::Multiply(matRotDelta, Matrix4x4::MakeRotateXMatrix(rotateX));
			matRotDelta = Matrix4x4::Multiply(matRotDelta, Matrix4x4::MakeRotateYMatrix(rotateY));

			// 累積回転行列を合成
			matRot_ = Matrix4x4::Multiply(matRotDelta, matRot_);
		}
	}

	return result;
}


bool DebugCamera::UpdateGamePadMove () {
	bool result = false;

	// ゲームパッドが接続されていなければ終了
	if (!Input::GetInstance()->IsGamePadConnected()) {
		return false;
	}

	// 移動量
	Vector3 move = {0.0f, 0.0f, 0.0f};

	XINPUT_STATE gamePadState = Input::GetInstance()->GetGamePadState();

	// 十字キーで上下左右移動
	if (gamePadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
		move.x -= kGamePadMoveSpeed;
	}

	if (gamePadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
		move.x += kGamePadMoveSpeed;
	}

	if (gamePadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) {
		move.y += kGamePadMoveSpeed;
	}

	if (gamePadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
		move.y -= kGamePadMoveSpeed;
	}

	// L2 / R2でZ軸前後移動
	float leftTrigger = NormalizeTriggerValue(gamePadState.Gamepad.bLeftTrigger);
	float rightTrigger = NormalizeTriggerValue(gamePadState.Gamepad.bRightTrigger);

	// L2 / R2でZ移動
	move.z += rightTrigger * kGamePadZMoveSpeed;
	move.z -= leftTrigger * kGamePadZMoveSpeed;

	if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f) {
		translation_ += move;
		result = true;
	}

	// 左スティックでピボット回転
	float stickX = NormalizeStickValue(gamePadState.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	float stickY = NormalizeStickValue(gamePadState.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

	if (stickX != 0.0f || stickY != 0.0f) {
		result = true;

		float rotateX = -stickY * kGamePadRotateSpeed;
		float rotateY = stickX * kGamePadRotateSpeed;

		// 追加回転分の回転行列を生成
		Matrix4x4 matRotDelta = Matrix4x4::MakeIdentity4x4();
		matRotDelta = Matrix4x4::Multiply(matRotDelta, Matrix4x4::MakeRotateXMatrix(rotateX));
		matRotDelta = Matrix4x4::Multiply(matRotDelta, Matrix4x4::MakeRotateYMatrix(rotateY));

		// 累積回転行列を合成
		matRot_ = Matrix4x4::Multiply(matRotDelta, matRot_);
	}

	return result;
}