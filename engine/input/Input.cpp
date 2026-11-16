#include "Input.h"
#include <cassert>
#include <string>
#include "engine/platform/Window.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;

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

Input *Input::GetInstance() {
	static Input instance;
	return &instance;
}

void Input::Initialize(Window *window) {

	// DirectInputの初期化
	ComPtr<IDirectInput8> directInput = nullptr;

	HRESULT hr = DirectInput8Create(
		window->GetHInstance(), DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		reinterpret_cast<void **>(directInput.GetAddressOf()),
		nullptr
	);

	assert(SUCCEEDED(hr));

	// キーボードデバイスの生成
	hr = directInput->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	// 入力データ形式のセット
	hr = keyboard_->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = keyboard_->SetCooperativeLevel(window->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	// マウスデバイスの生成
	hr = directInput->CreateDevice(GUID_SysMouse, mouse_.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	// 入力データ形式のセット
	hr = mouse_->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = mouse_->SetCooperativeLevel(window->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(hr));
}

void Input::Update() {
	HRESULT hr;

	// 前フレームのキー入力状態を保存する
	std::memcpy(preKey_, key_, sizeof(key_));

	// キーボード情報の取得開始
	keyboard_->Acquire();

	// 全キーの入力状態を取得する
	keyboard_->GetDeviceState(sizeof(key_), key_);

	// 前フレームのマウス入力状態を保存する
	preMouseState_ = mouseState_;

	// マウス入力状態を初期化する
	mouseState_ = {};

	// マウス情報の取得開始
	mouse_->Acquire();

	// マウスの入力状態を取得する
	hr = mouse_->GetDeviceState(sizeof(mouseState_), &mouseState_);

	// 前フレームのゲームパッド入力状態を保存する
	preGamePadState_ = gamePadState_;

	// ゲームパッド入力状態を初期化する
	gamePadState_ = {};

	// ゲームパッドの入力状態を取得する
	DWORD gamePadResult = XInputGetState(0, &gamePadState_);
	isGamePadConnected_ = gamePadResult == ERROR_SUCCESS;

	// ゲームパッドが接続されてなければ入力状態を初期化
	if (!isGamePadConnected_) {
		gamePadState_ = {};
		preGamePadState_ = {};
	}
}

Input::~Input() {
	// キーボード入力の取得終了
	keyboard_->Unacquire();

	// マウス入力の取得終了
	mouse_->Unacquire();
}

bool Input::IsPressKey(uint8_t keyNumber) const { return key_[keyNumber] != 0; }

bool Input::IsTriggerKey(uint8_t keyNumber) const {
	return key_[keyNumber] != 0 && preKey_[keyNumber] == 0;
}

bool Input::IsReleaseTriggerKey(uint8_t keyNumber) const {
	return key_[keyNumber] == 0 && preKey_[keyNumber] != 0;
}

bool Input::IsPressGamePadButton(WORD button) const {
	return (gamePadState_.Gamepad.wButtons & button) != 0;
}

bool Input::IsTriggerGamePadButton(WORD button) const {
	return (gamePadState_.Gamepad.wButtons & button) != 0 && (preGamePadState_.Gamepad.wButtons & button) == 0;
}

bool Input::IsReleaseTriggerGamePadButton(WORD button) const {
	return (gamePadState_.Gamepad.wButtons & button) == 0 && (preGamePadState_.Gamepad.wButtons & button) != 0;
}

LONG Input::GetMouseMoveX() const { return mouseState_.lX; }

LONG Input::GetMouseMoveY() const { return mouseState_.lY; }

LONG Input::GetMouseWheel() const { return mouseState_.lZ; }

bool Input::IsPressMouseButton(uint8_t buttonNumber) const {
	return (mouseState_.rgbButtons[buttonNumber] & 0x80) != 0;
}

float Input::GetLeftStickX() const {
	return NormalizeStickValue(gamePadState_.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
}

float Input::GetLeftStickY() const {
	return NormalizeStickValue(gamePadState_.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
}

float Input::GetLeftTrigger() const { return NormalizeTriggerValue(gamePadState_.Gamepad.bLeftTrigger); }

float Input::GetRightTrigger() const { return NormalizeTriggerValue(gamePadState_.Gamepad.bRightTrigger); }
