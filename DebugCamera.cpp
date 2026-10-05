#include "DebugCamera.h"
#include "Input.h"


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

	// マウスホイールでZ軸前後移動
	if (input->GetMouseWheel() != 0) {
		move.z += static_cast<float>(input->GetMouseWheel()) * kWheelMoveSpeed;
	}

	if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f) {
		translation_ += move;
		result = true;
	}

	// 右クリック中にマウス移動で回転
	if (input->IsPressMouseButton(1)) {
		float rotateX = static_cast<float>(input->GetMouseMoveY()) * kRotateSpeed;
		float rotateY = static_cast<float>(input->GetMouseMoveX()) * kRotateSpeed;

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

	Input *input = Input::GetInstance();

	// 十字キーで上下左右移動
	if (input->IsPressGamePadButton(XINPUT_GAMEPAD_DPAD_LEFT)) {
		move.x -= kGamePadMoveSpeed;
	}

	if (input->IsPressGamePadButton(XINPUT_GAMEPAD_DPAD_RIGHT)) {
		move.x += kGamePadMoveSpeed;
	}

	if (input->IsPressGamePadButton(XINPUT_GAMEPAD_DPAD_UP)) {
		move.y += kGamePadMoveSpeed;
	}

	if (input->IsPressGamePadButton(XINPUT_GAMEPAD_DPAD_DOWN)) {
		move.y -= kGamePadMoveSpeed;
	}

	// L2 / R2でZ移動
	move.z += input->GetRightTrigger() * kGamePadZMoveSpeed;
	move.z -= input->GetLeftTrigger() * kGamePadZMoveSpeed;

	if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f) {
		translation_ += move;
		result = true;
	}

	// 左スティックでピボット回転
	float stickX = input->GetLeftStickX();
	float stickY = input->GetLeftStickY();

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