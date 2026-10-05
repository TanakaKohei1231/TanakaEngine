#pragma once
#include <Windows.h>
#include <cstdint>
#include <Xinput.h>

class Input {
  public:
	void Initialize(HINSTANCE hInstance, HWND hwnd);
	void Update();

  private:
};

// キーを押した状態か
bool IsPressKey(const BYTE *key, uint8_t keyNumber);

// キーを離した状態か
bool IsReleaseKey(const BYTE *key, uint8_t keyNumber);

// キーを押した瞬間か
bool IsTriggerKey(const BYTE *key, const BYTE *previousKey, uint8_t keyNumber);

// キーを離した瞬間か
bool IsReleaseTriggerKey(const BYTE *key, const BYTE *previousKey, uint8_t keyNumber);

// ゲームパッドのボタンを押した状態か
bool IsPressGamePadButton(const XINPUT_STATE *gamePadState, WORD button);

// ゲームパッドのボタンを離した状態か
bool IsReleaseGamePadButton(const XINPUT_STATE *gamePadState, WORD button);

// ゲームパッドのボタンを押した瞬間か
bool IsTriggerGamePadButton(const XINPUT_STATE *gamePadState, const XINPUT_STATE *preGamePadState, WORD button);

// ゲームパッドのボタンを離した瞬間か
bool IsReleaseTriggerGamePadButton(const XINPUT_STATE *gamePadState, const XINPUT_STATE *preGamePadState, WORD button);