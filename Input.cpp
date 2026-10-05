#include "Input.h"
#include <cassert>
#include <string>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;

Input *Input::GetInstance() {
	static Input instance;
	return &instance;
}

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {

	// DirectInputの初期化
	ComPtr<IDirectInput8> directInput = nullptr;

	HRESULT hr = DirectInput8Create(
		hInstance, DIRECTINPUT_VERSION,
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
	hr = keyboard_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	// マウスデバイスの生成
	hr = directInput->CreateDevice(GUID_SysMouse, mouse_.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	// 入力データ形式のセット
	hr = mouse_->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = mouse_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
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