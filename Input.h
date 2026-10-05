#pragma once
#include <Windows.h>
#include <cstdint>
#include <Xinput.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>

class Input {
  public:
	static Input *GetInstance();

	void Initialize(HINSTANCE hInstance, HWND hwnd);
	void Update();

	// キーを押した状態か
	bool IsPressKey(uint8_t keyNumber) const;

	// キーを押した瞬間か
	bool IsTriggerKey(uint8_t keyNumber) const;

	// キーを離した瞬間か
	bool IsReleaseTriggerKey(uint8_t keyNumber) const;

	// ゲームパッドのボタンを押した状態か
	bool IsPressGamePadButton(WORD button) const;

	// ゲームパッドのボタンを押した瞬間か
	bool IsTriggerGamePadButton(WORD button) const;

	// ゲームパッドのボタンを離した瞬間か
	bool IsReleaseTriggerGamePadButton(WORD button) const;

	// マウスのX方向移動量
	LONG GetMouseMoveX() const;

	// マウスのY方向移動量
	LONG GetMouseMoveY() const;

	// マウスホイールの移動量
	LONG GetMouseWheel() const;

	// マウスボタンを押しているか
	bool IsPressMouseButton(uint8_t buttonNumber) const;

	// 左スティックのX入力
	float GetLeftStickX() const;

	// 左スティックのY入力
	float GetLeftStickY() const;

	// Lトリガーの入力
	float GetLeftTrigger() const;

	// Rトリガーの入力
	float GetRightTrigger() const;

	// ゲームパッドが接続されているかどうか
	bool IsGamePadConnected() const { return isGamePadConnected_; }



	// コピーコンストラクタと代入演算子を削除してシングルトンパターンにする
	Input(const Input &) = delete;
	Input &operator=(const Input &) = delete;

  private:
	// シングルトンパターンにするためのコンストラクタとデストラクタをprivateにする
	Input() = default;
	~Input();

	// キーボードのデバイス
	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_ = nullptr;

	// マウスのデバイス
	Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse_ = nullptr;

	// 全キーの入力状態
	BYTE key_[256] = {};

	// 前フレームの全キーの入力状態
	BYTE preKey_[256] = {};

	// マウスの入力状態
	DIMOUSESTATE2 mouseState_{};

	// 前フレームのマウス入力状態
	DIMOUSESTATE2 preMouseState_{};

	// ゲームパッドの入力状態
	XINPUT_STATE gamePadState_{};

	// 前フレームのゲームパッド入力状態
	XINPUT_STATE preGamePadState_{};

	// ゲームパッドが接続されているか
	bool isGamePadConnected_ = false;
};