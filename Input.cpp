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