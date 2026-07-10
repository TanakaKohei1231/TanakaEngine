#pragma once


struct Vector2 {

	/*-------- 変数 --------
	------------------------------------------------*/
	float x;
	float y;



	/*-------- 関数 --------
	------------------------------------------------*/

	/// <summary>
	/// デフォルトコンストラクタ
	/// </summary>
	constexpr Vector2()
		: x(0.0f), y(0.0f) {
	}


	/// <summary>
	/// コンストラクタ
	/// </summary>
	/// <param name="x">xに格納する値</param>
	/// <param name="y">yに格納する値</param>
	constexpr Vector2(float x, float y) 
		: x(x), y(y) {
	}


	/// <summary>
	/// ベクトルの大きさを求める
	/// </summary>
	/// <returns></returns>
	float Length() const;


	/// <summary>
	/// ベクトルの正規化
	/// </summary>
	/// <returns></returns>
	Vector2 Normalize() const;


	/// <summary>
	/// ベクトルの大きさの二乗を求める
	/// </summary>
	constexpr float LengthSquared() const {
		return x * x + y * y;
	}


	/// <summary>
	/// ドット積
	/// </summary>
	/// <param name="vec"></param>
	/// <returns></returns>
	constexpr float Dot(const Vector2 &vec) const {
		return x * vec.x + y * vec.y;
	}


	/// <summary>
	/// クロス積
	/// </summary>
	/// <param name="vec"></param>
	/// <returns></returns>
	constexpr float Cross(const Vector2 &vec) const {
		return x * vec.y - y * vec.x;
	}


	/*-------- 演算子オーバーロード --------
	------------------------------------------------*/

	// ベクトル同士の加算
	constexpr Vector2 operator+(const Vector2 &vec) const {
		return Vector2(x + vec.x, y + vec.y);
	}

	// ベクトル同士の減算
	constexpr Vector2 operator-(const Vector2 &vec) const {
		return Vector2(x - vec.x, y - vec.y);
	}

	// スカラ乗
	constexpr Vector2 operator*(float scalar) const {
		return Vector2(x * scalar, y * scalar);
	}

	// スカラ除
	constexpr Vector2 operator/(float scalar) const {
		return Vector2(x / scalar, y / scalar);
	}

	// 加算代入
	constexpr Vector2 &operator+=(const Vector2 &vec) {
		x += vec.x;
		y += vec.y;
		return *this;
	}

	// 減算代入
	constexpr Vector2 &operator-=(const Vector2 &vec) {
		x -= vec.x;
		y -= vec.y;
		return *this;
	}

	// 乗算代入
	constexpr Vector2 &operator*=(float scalar) {
		x *= scalar;
		y *= scalar;
		return *this;
	}

	// 除算代入
	constexpr Vector2 &operator/=(float scalar) {
		x /= scalar;
		y /= scalar;
		return *this;
	}

	// 単項マイナス
	constexpr Vector2 operator-() const {
		return { -x, -y };
	}

	// 等価比較
	constexpr bool operator==(const Vector2 &v) const {
		return x == v.x && y == v.y;
	}

	// 不等価比較
	constexpr bool operator!=(const Vector2 &v) const {
		return !(*this == v);
	}

};


// スカラー * ベクトル
constexpr Vector2 operator*(float scalar, const Vector2 &vec) {
	return { vec.x * scalar, vec.y * scalar };
}