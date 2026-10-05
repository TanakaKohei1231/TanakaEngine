#include "Input.h"
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include <cassert>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;

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
	ComPtr<IDirectInputDevice8> keyboard;
	hr = directInput->CreateDevice(GUID_SysKeyboard, keyboard.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	// 入力データ形式のセット
	hr = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	// マウスデバイスの生成
	ComPtr<IDirectInputDevice8> mouse;
	hr = directInput->CreateDevice(GUID_SysMouse, mouse.GetAddressOf(), NULL);
	assert(SUCCEEDED(hr));

	// 入力データ形式のセット
	hr = mouse->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = mouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(hr));
}

void Input::Update() {}

bool IsPressKey(const BYTE *key, uint8_t keyNumber) {
	return key[keyNumber] != 0;
}

bool IsReleaseKey(const BYTE *key, uint8_t keyNumber) {
	return key[keyNumber] == 0;
}

bool IsTriggerKey(const BYTE *key, const BYTE *previousKey, uint8_t keyNumber) {
	return key[keyNumber] != 0 && previousKey[keyNumber] == 0;
}

bool IsReleaseTriggerKey(const BYTE *key, const BYTE *previousKey, uint8_t keyNumber) {
	return key[keyNumber] == 0 && previousKey[keyNumber] != 0;
}

bool IsPressGamePadButton(const XINPUT_STATE *gamePadState, WORD button) {
	return (gamePadState->Gamepad.wButtons & button) != 0;
}

bool IsReleaseGamePadButton(const XINPUT_STATE *gamePadState, WORD button) {
	return (gamePadState->Gamepad.wButtons & button) == 0;
}

bool IsTriggerGamePadButton(const XINPUT_STATE *gamePadState, const XINPUT_STATE *preGamePadState, WORD button) {
	return (gamePadState->Gamepad.wButtons & button) != 0 && (preGamePadState->Gamepad.wButtons & button) == 0;
}

bool IsReleaseTriggerGamePadButton(const XINPUT_STATE *gamePadState, const XINPUT_STATE *preGamePadState, WORD button) {
	return (gamePadState->Gamepad.wButtons & button) == 0 && (preGamePadState->Gamepad.wButtons & button) != 0;
}