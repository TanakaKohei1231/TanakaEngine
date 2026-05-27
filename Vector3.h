#pragma once

class Vector3 {
public:
	float x;
	float y;
	float z;

	// ベクトルの加算
	static Vector3 Add(const Vector3& v1, const Vector3& v2);

	// ベクトルの減算
	static Vector3 Subtract(const Vector3& v1, const Vector3& v2);

	// ベクトルのスカラー倍
	static Vector3 Multiply(float scalar, const Vector3& v);

	// ベクトルの内積
	static float Dot(const Vector3& v1, const Vector3& v2);

	// ベクトルの長さ
	static float Length(const Vector3& v);

	// ベクトルの正規化
	static Vector3 Normalize(const Vector3& v);
};
