#pragma once
#include <Windows.h>
#include <cstdint>
#include <dinput.h>

// キーを押した状態か
bool IsPressKey(const BYTE *key, uint8_t keyNumber);

// キーを離した状態か
bool IsReleaseKey(const BYTE *key, uint8_t keyNumber);

// キーを押した瞬間か
bool IsTriggerKey(const BYTE *key, const BYTE *previousKey, uint8_t keyNumber);

// キーを離した瞬間か
bool IsReleaseTriggerKey(const BYTE *key, const BYTE *previousKey, uint8_t keyNumber);
