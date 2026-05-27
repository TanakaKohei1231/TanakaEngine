#pragma once
#include "Vector3.h"

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

class Matrix4x4 {
  public:
	float m_[4][4] = {};

	// 行列の加算
	static Matrix4x4 Add(const Matrix4x4 &m1, const Matrix4x4 &m2);

	// 行列の減算
	static Matrix4x4 Subtract(const Matrix4x4 &m1, const Matrix4x4 &m2);

	// 行列の乗算
	static Matrix4x4 Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2);

	// 逆行列
	static Matrix4x4 Inverse(const Matrix4x4 &m);

	// 転置行列
	static Matrix4x4 Transpose(const Matrix4x4 &m);

	// 単位行列の生成
	static Matrix4x4 MakeIdentity4x4();

	static Vector3 Transform(const Vector3 &v, const Matrix4x4 &m);

	// 平行移動行列の生成
	static Matrix4x4 MakeTranslateMatrix(const Vector3 &translation);

	// 拡縮行列の生成
	static Matrix4x4 MakeScaleMatrix(const Vector3 &scale);

	// X軸回転行列の生成
	static Matrix4x4 MakeRotateXMatrix(float radian);

	// Y軸回転行列の生成
	static Matrix4x4 MakeRotateYMatrix(float radian);

	// Z軸回転行列の生成
	static Matrix4x4 MakeRotateZMatrix(float radian);

	// 全回転行列の生成
	static Matrix4x4 MakeRotateXYZMatrix(const Vector3 &radian) {
		Matrix4x4 rotateXMatrix = MakeRotateXMatrix(radian.x);
		Matrix4x4 rotateYMatrix = MakeRotateYMatrix(radian.y);
		Matrix4x4 rotateZMatrix = MakeRotateZMatrix(radian.z);

		return Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));
	}

	// アフィン行列の生成
	static Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate);

  private:
	static const int kColumnWidth = 60;
};
