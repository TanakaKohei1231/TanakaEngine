#include "Input.h"

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