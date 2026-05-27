#include "Matrix4x4.h"
#include <cmath>

Matrix4x4 Matrix4x4::Add(const Matrix4x4 &m1, const Matrix4x4 &m2) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result.m_[i][j] = m1.m_[i][j] + m2.m_[i][j];
		}
	}
	return result;
}

Matrix4x4 Matrix4x4::Subtract(const Matrix4x4 &m1, const Matrix4x4 &m2) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result.m_[i][j] = m1.m_[i][j] - m2.m_[i][j];
		}
	}
	return result;
}

Matrix4x4 Matrix4x4::Multiply(const Matrix4x4 &m1, const Matrix4x4 &m2) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			for (int k = 0; k < 4; ++k) {
				result.m_[i][j] += m1.m_[i][k] * m2.m_[k][j];
			}
		}
	}
	return result;
}


Matrix4x4 Matrix4x4::Inverse(const Matrix4x4 &m) {
	Matrix4x4 result{};

	float determinant =
	    m.m_[0][0] *
	        (m.m_[1][1] * m.m_[2][2] * m.m_[3][3] + m.m_[1][2] * m.m_[2][3] * m.m_[3][1] +
	         m.m_[1][3] * m.m_[2][1] * m.m_[3][2] - m.m_[1][3] * m.m_[2][2] * m.m_[3][1] -
	         m.m_[1][2] * m.m_[2][1] * m.m_[3][3] - m.m_[1][1] * m.m_[2][3] * m.m_[3][2]) -
	    m.m_[0][1] *
	        (m.m_[1][0] * m.m_[2][2] * m.m_[3][3] + m.m_[1][2] * m.m_[2][3] * m.m_[3][0] +
	         m.m_[1][3] * m.m_[2][0] * m.m_[3][2] - m.m_[1][3] * m.m_[2][2] * m.m_[3][0] -
	         m.m_[1][2] * m.m_[2][0] * m.m_[3][3] - m.m_[1][0] * m.m_[2][3] * m.m_[3][2]) +
	    m.m_[0][2] *
	        (m.m_[1][0] * m.m_[2][1] * m.m_[3][3] + m.m_[1][1] * m.m_[2][3] * m.m_[3][0] +
	         m.m_[1][3] * m.m_[2][0] * m.m_[3][1] - m.m_[1][3] * m.m_[2][1] * m.m_[3][0] -
	         m.m_[1][1] * m.m_[2][0] * m.m_[3][3] - m.m_[1][0] * m.m_[2][3] * m.m_[3][1]) -
	    m.m_[0][3] *
	        (m.m_[1][0] * m.m_[2][1] * m.m_[3][2] + m.m_[1][1] * m.m_[2][2] * m.m_[3][0] +
	         m.m_[1][2] * m.m_[2][0] * m.m_[3][1] - m.m_[1][2] * m.m_[2][1] * m.m_[3][0] -
	         m.m_[1][1] * m.m_[2][0] * m.m_[3][2] - m.m_[1][0] * m.m_[2][2] * m.m_[3][1]);

	if (determinant == 0.0f) {
		return result;
	}

	float inverseDeterminant = 1.0f / determinant;

	result.m_[0][0] =
	    inverseDeterminant *
	    (m.m_[1][1] * m.m_[2][2] * m.m_[3][3] + m.m_[1][2] * m.m_[2][3] * m.m_[3][1] +
	     m.m_[1][3] * m.m_[2][1] * m.m_[3][2] - m.m_[1][3] * m.m_[2][2] * m.m_[3][1] -
	     m.m_[1][2] * m.m_[2][1] * m.m_[3][3] - m.m_[1][1] * m.m_[2][3] * m.m_[3][2]);

	result.m_[0][1] =
	    inverseDeterminant *
	    -(m.m_[0][1] * m.m_[2][2] * m.m_[3][3] + m.m_[0][2] * m.m_[2][3] * m.m_[3][1] +
	      m.m_[0][3] * m.m_[2][1] * m.m_[3][2] - m.m_[0][3] * m.m_[2][2] * m.m_[3][1] -
	      m.m_[0][2] * m.m_[2][1] * m.m_[3][3] - m.m_[0][1] * m.m_[2][3] * m.m_[3][2]);

	result.m_[0][2] =
	    inverseDeterminant *
	    (m.m_[0][1] * m.m_[1][2] * m.m_[3][3] + m.m_[0][2] * m.m_[1][3] * m.m_[3][1] +
	     m.m_[0][3] * m.m_[1][1] * m.m_[3][2] - m.m_[0][3] * m.m_[1][2] * m.m_[3][1] -
	     m.m_[0][2] * m.m_[1][1] * m.m_[3][3] - m.m_[0][1] * m.m_[1][3] * m.m_[3][2]);

	result.m_[0][3] =
	    inverseDeterminant *
	    -(m.m_[0][1] * m.m_[1][2] * m.m_[2][3] + m.m_[0][2] * m.m_[1][3] * m.m_[2][1] +
	      m.m_[0][3] * m.m_[1][1] * m.m_[2][2] - m.m_[0][3] * m.m_[1][2] * m.m_[2][1] -
	      m.m_[0][2] * m.m_[1][1] * m.m_[2][3] - m.m_[0][1] * m.m_[1][3] * m.m_[2][2]);

	result.m_[1][0] =
	    inverseDeterminant *
	    -(m.m_[1][0] * m.m_[2][2] * m.m_[3][3] + m.m_[1][2] * m.m_[2][3] * m.m_[3][0] +
	      m.m_[1][3] * m.m_[2][0] * m.m_[3][2] - m.m_[1][3] * m.m_[2][2] * m.m_[3][0] -
	      m.m_[1][2] * m.m_[2][0] * m.m_[3][3] - m.m_[1][0] * m.m_[2][3] * m.m_[3][2]);

	result.m_[1][1] =
	    inverseDeterminant *
	    (m.m_[0][0] * m.m_[2][2] * m.m_[3][3] + m.m_[0][2] * m.m_[2][3] * m.m_[3][0] +
	     m.m_[0][3] * m.m_[2][0] * m.m_[3][2] - m.m_[0][3] * m.m_[2][2] * m.m_[3][0] -
	     m.m_[0][2] * m.m_[2][0] * m.m_[3][3] - m.m_[0][0] * m.m_[2][3] * m.m_[3][2]);

	result.m_[1][2] =
	    inverseDeterminant *
	    -(m.m_[0][0] * m.m_[1][2] * m.m_[3][3] + m.m_[0][2] * m.m_[1][3] * m.m_[3][0] +
	      m.m_[0][3] * m.m_[1][0] * m.m_[3][2] - m.m_[0][3] * m.m_[1][2] * m.m_[3][0] -
	      m.m_[0][2] * m.m_[1][0] * m.m_[3][3] - m.m_[0][0] * m.m_[1][3] * m.m_[3][2]);

	result.m_[1][3] =
	    inverseDeterminant *
	    (m.m_[0][0] * m.m_[1][2] * m.m_[2][3] + m.m_[0][2] * m.m_[1][3] * m.m_[2][0] +
	     m.m_[0][3] * m.m_[1][0] * m.m_[2][2] - m.m_[0][3] * m.m_[1][2] * m.m_[2][0] -
	     m.m_[0][2] * m.m_[1][0] * m.m_[2][3] - m.m_[0][0] * m.m_[1][3] * m.m_[2][2]);

	result.m_[2][0] =
	    inverseDeterminant *
	    (m.m_[1][0] * m.m_[2][1] * m.m_[3][3] + m.m_[1][1] * m.m_[2][3] * m.m_[3][0] +
	     m.m_[1][3] * m.m_[2][0] * m.m_[3][1] - m.m_[1][3] * m.m_[2][1] * m.m_[3][0] -
	     m.m_[1][1] * m.m_[2][0] * m.m_[3][3] - m.m_[1][0] * m.m_[2][3] * m.m_[3][1]);

	result.m_[2][1] =
	    inverseDeterminant *
	    -(m.m_[0][0] * m.m_[2][1] * m.m_[3][3] + m.m_[0][1] * m.m_[2][3] * m.m_[3][0] +
	      m.m_[0][3] * m.m_[2][0] * m.m_[3][1] - m.m_[0][3] * m.m_[2][1] * m.m_[3][0] -
	      m.m_[0][1] * m.m_[2][0] * m.m_[3][3] - m.m_[0][0] * m.m_[2][3] * m.m_[3][1]);

	result.m_[2][2] =
	    inverseDeterminant *
	    (m.m_[0][0] * m.m_[1][1] * m.m_[3][3] + m.m_[0][1] * m.m_[1][3] * m.m_[3][0] +
	     m.m_[0][3] * m.m_[1][0] * m.m_[3][1] - m.m_[0][3] * m.m_[1][1] * m.m_[3][0] -
	     m.m_[0][1] * m.m_[1][0] * m.m_[3][3] - m.m_[0][0] * m.m_[1][3] * m.m_[3][1]);

	result.m_[2][3] =
	    inverseDeterminant *
	    -(m.m_[0][0] * m.m_[1][1] * m.m_[2][3] + m.m_[0][1] * m.m_[1][3] * m.m_[2][0] +
	      m.m_[0][3] * m.m_[1][0] * m.m_[2][1] - m.m_[0][3] * m.m_[1][1] * m.m_[2][0] -
	      m.m_[0][1] * m.m_[1][0] * m.m_[2][3] - m.m_[0][0] * m.m_[1][3] * m.m_[2][1]);

	result.m_[3][0] =
	    inverseDeterminant *
	    -(m.m_[1][0] * m.m_[2][1] * m.m_[3][2] + m.m_[1][1] * m.m_[2][2] * m.m_[3][0] +
	      m.m_[1][2] * m.m_[2][0] * m.m_[3][1] - m.m_[1][2] * m.m_[2][1] * m.m_[3][0] -
	      m.m_[1][1] * m.m_[2][0] * m.m_[3][2] - m.m_[1][0] * m.m_[2][2] * m.m_[3][1]);

	result.m_[3][1] =
	    inverseDeterminant *
	    (m.m_[0][0] * m.m_[2][1] * m.m_[3][2] + m.m_[0][1] * m.m_[2][2] * m.m_[3][0] +
	     m.m_[0][2] * m.m_[2][0] * m.m_[3][1] - m.m_[0][2] * m.m_[2][1] * m.m_[3][0] -
	     m.m_[0][1] * m.m_[2][0] * m.m_[3][2] - m.m_[0][0] * m.m_[2][2] * m.m_[3][1]);

	result.m_[3][2] =
	    inverseDeterminant *
	    -(m.m_[0][0] * m.m_[1][1] * m.m_[3][2] + m.m_[0][1] * m.m_[1][2] * m.m_[3][0] +
	      m.m_[0][2] * m.m_[1][0] * m.m_[3][1] - m.m_[0][2] * m.m_[1][1] * m.m_[3][0] -
	      m.m_[0][1] * m.m_[1][0] * m.m_[3][2] - m.m_[0][0] * m.m_[1][2] * m.m_[3][1]);

	result.m_[3][3] =
	    inverseDeterminant *
	    (m.m_[0][0] * m.m_[1][1] * m.m_[2][2] + m.m_[0][1] * m.m_[1][2] * m.m_[2][0] +
	     m.m_[0][2] * m.m_[1][0] * m.m_[2][1] - m.m_[0][2] * m.m_[1][1] * m.m_[2][0] -
	     m.m_[0][1] * m.m_[1][0] * m.m_[2][2] - m.m_[0][0] * m.m_[1][2] * m.m_[2][1]);

	return result;
}

Matrix4x4 Matrix4x4::Transpose(const Matrix4x4 &m) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result.m_[i][j] = m.m_[j][i];
		}
	}
	return result;
}

Matrix4x4 Matrix4x4::MakeIdentity4x4() {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i) {
		result.m_[i][i] = 1.0f;
	}
	return result;
}


Vector3 Matrix4x4::Transform(const Vector3& v, const Matrix4x4& m) {
	Vector3 result{};

	float w = 0.0f;

	result.x = v.x * m.m_[0][0] + v.y * m.m_[1][0] + v.z * m.m_[2][0] + 1.0f * m.m_[3][0];
	result.y = v.x * m.m_[0][1] + v.y * m.m_[1][1] + v.z * m.m_[2][1] + 1.0f * m.m_[3][1];
	result.z = v.x * m.m_[0][2] + v.y * m.m_[1][2] + v.z * m.m_[2][2] + 1.0f * m.m_[3][2];
	w		 = v.x * m.m_[0][3] + v.y * m.m_[1][3] + v.z * m.m_[2][3] + 1.0f * m.m_[3][3];

	if (w != 0.0f) {
		result.x /= w;
		result.y /= w;
		result.z /= w;
	}

	return result;
}

Matrix4x4 Matrix4x4::MakeTranslateMatrix(const Vector3 &translation) {
	Matrix4x4 result = MakeIdentity4x4();

	result.m_[3][0] = translation.x;	result.m_[3][1] = translation.y;	result.m_[3][2] = translation.z;

	return result;
}

Matrix4x4 Matrix4x4::MakeScaleMatrix(const Vector3 &scale) {
	Matrix4x4 result = MakeIdentity4x4();

	result.m_[0][0] = scale.x;
	result.m_[1][1] = scale.y;
	result.m_[2][2] = scale.z;
	
	return result;
}

Matrix4x4 Matrix4x4::MakeRotateXMatrix(float radian) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m_[1][1] = cosf(radian);
	result.m_[1][2] = sinf(radian);
	result.m_[2][1] = -sinf(radian);
	result.m_[2][2] = cosf(radian);
	return result;
}

Matrix4x4 Matrix4x4::MakeRotateYMatrix(float radian) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m_[0][0] = cosf(radian);
	result.m_[0][2] = -sinf(radian);
	result.m_[2][0] = sinf(radian);
	result.m_[2][2] = cosf(radian);
	return result;
}

Matrix4x4 Matrix4x4::MakeRotateZMatrix(float radian) {
	Matrix4x4 result = MakeIdentity4x4();
	result.m_[0][0] = cosf(radian);
	result.m_[0][1] = sinf(radian);
	result.m_[1][0] = -sinf(radian);
	result.m_[1][1] = cosf(radian);
	return result;
}

Matrix4x4 Matrix4x4::MakeAffineMatrix(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate) {
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
	Matrix4x4 rotateMatrix = MakeRotateXYZMatrix(rotate);
	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);
	return Multiply(scaleMatrix, Multiply(rotateMatrix, translateMatrix));
}