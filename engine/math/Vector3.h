#pragma once
class Vector3 {
  public:
	float x;
	float y;
	float z;

	// ベクトルの加算
	static Vector3 Add(const Vector3 &v1, const Vector3 &v2);

	// ベクトルの減算
	static Vector3 Subtract(const Vector3 &v1, const Vector3 &v2);

	// ベクトルのスカラー倍
	static Vector3 Multiply(float scalar, const Vector3 &v);

	// ベクトルの内積
	static float Dot(const Vector3 &v1, const Vector3 &v2);

	// ベクトルの長さ
	static float Length(const Vector3 &v);

	// ベクトルの正規化
	static Vector3 Normalize(const Vector3 &v);

	// クロス積
	static Vector3 Cross(const Vector3 &v1, const Vector3 &v2);

	// ベクトルの垂直ベクトルを求める
	static Vector3 Perpendicular(const Vector3 &vector);

	// 線形補間
	static Vector3 Lerp(const Vector3 &start, const Vector3 &end, float t);

	// ベジェ曲線
	static Vector3 Bezier(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, float t);

	// 反射ベクトル
	static Vector3 Reflect(const Vector3 &input, const Vector3 &normal);

	// ベクトルの射影
	static Vector3 Project(const Vector3 &v1, const Vector3 &v2);

	// ==========================================================
	// 演算子オーバーロード
	// ==========================================================

	Vector3 operator+(const Vector3 &obj) const {
		return Add(*this, obj);
	}

	Vector3 operator-(const Vector3 &obj) const {
		return Subtract(*this, obj);
	}

	Vector3 operator*(const float scalar) const {
		return Multiply(scalar, *this);
	}

	Vector3 operator/(const float scalar) const {
		return Multiply(1.0f / scalar, *this);
	}

	void operator+=(const Vector3 &obj){
		*this = Add(*this, obj);
	}

	void operator-=(const Vector3 &obj) {
		*this = Subtract(*this, obj);
	}

	void operator*=(const float scalar) {
		*this = Multiply(scalar, *this);
	}

	void operator/=(const float scalar) {
		*this = Multiply(1.0f / scalar, *this);
	}

	// 単項演算子
	Vector3 operator+() const {
		return *this;
	}

	Vector3 operator-() const {
		return Multiply(-1.0f, *this);
	}
};

