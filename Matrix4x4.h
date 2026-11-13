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

	static Vector3 TransformCoord(const Vector3 &v, const Matrix4x4 &m);

	// 平行移動行列の生成
	static Matrix4x4 MakeTranslateMatrix(const Vector3 &translation);

	// 拡縮行列の生成
	static Matrix4x4 MakeScaleMatrix(const Vector3 &scale);

	// X軸回転行列の生成
	static Matrix4x4 MakeRotateXMatrix(float rotate);

	// Y軸回転行列の生成
	static Matrix4x4 MakeRotateYMatrix(float rotate);

	// Z軸回転行列の生成
	static Matrix4x4 MakeRotateZMatrix(float rotate);

	// XYZ軸回転行列の生成
	static Matrix4x4 MakeRotateXYZMatrix(const Vector3 &rotate);

	// アフィン変換行列の生成
	static Matrix4x4 MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translation);
	static Matrix4x4 MakeAffineMatrix(const Transform &transform);

	// 透視投影行列の生成
	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	// 平行投影行列の生成
	static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	// ビューポート変換行列の生成
	static Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

	// ローカル座標をスクリーン座標に変換する関数
	static Vector3 TransformToScreen(const Vector3 &localVertex, const Matrix4x4 &worldViewProjectionMatrix, const Matrix4x4 &viewportMatrix);

	Matrix4x4 operator+(const Matrix4x4 &obj) const {
		return Add(*this, obj);
	};

	Matrix4x4 operator-(const Matrix4x4 &obj) const {
		return Subtract(*this, obj);
	};

	Matrix4x4 operator*(const Matrix4x4 &obj) const {
		return Multiply(*this, obj);
	};

	void operator+=(const Matrix4x4 &obj) {
		*this = Add(*this, obj);
	}

	void operator-=(const Matrix4x4 &obj) {
		*this = Subtract(*this, obj);
	}

	void operator*=(const Matrix4x4 &obj) {
		*this = Multiply(*this, obj);
	}

	bool operator==(const Matrix4x4 &obj) const;

	bool operator!=(const Matrix4x4 &obj) const {
		return !(*this == obj);
	}
};


struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 world;
};