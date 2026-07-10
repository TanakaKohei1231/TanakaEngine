#include "Vector3.h"
#include <cmath>

Vector3 Vector3::Add(const Vector3 &v1, const Vector3 &v2) {
	Vector3 result{};
	result.x = v1.x + v2.x;
	result.y = v1.y + v2.y;
	result.z = v1.z + v2.z;
	return result;
}

Vector3 Vector3::Subtract(const Vector3 &v1, const Vector3 &v2) {
	Vector3 result{};
	result.x = v1.x - v2.x;
	result.y = v1.y - v2.y;
	result.z = v1.z - v2.z;
	return result;
}

Vector3 Vector3::Multiply(float scalar, const Vector3 &v) {
	Vector3 result{};
	result.x = v.x * scalar;
	result.y = v.y * scalar;
	result.z = v.z * scalar;
	return result;
}

float Vector3::Dot(const Vector3 &v1, const Vector3 &v2) { return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z; }

Vector3 Vector3::Normalize(const Vector3 &v) {
	Vector3 result{};
	float length = Length(v);
	if (length != 0.0f) {
		result.x = v.x / length;
		result.y = v.y / length;
		result.z = v.z / length;
	}
	return result;
}

float Vector3::Length(const Vector3 &v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }

Vector3 Vector3::Cross(const Vector3 &v1, const Vector3 &v2) {
	Vector3 result{};

	result.x = v1.y * v2.z - v1.z * v2.y;
	result.y = v1.z * v2.x - v1.x * v2.z;
	result.z = v1.x * v2.y - v1.y * v2.x;

	return result;
}

Vector3 Vector3::Perpendicular(const Vector3 &vector) {
	if (vector.x != 0.0f || vector.y != 0.0f) {
		return {-vector.y, vector.x, 0.0f};
	}

	return {0.0f, -vector.z, vector.y};
}

Vector3 Vector3::Lerp(const Vector3 &start, const Vector3 &end, float t) { return start + (end - start) * t; }

Vector3 Vector3::Bezier(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, float t) {
	Vector3 p0p1 = Lerp(p0, p1, t);
	Vector3 p1p2 = Lerp(p1, p2, t);
	Vector3 p = Lerp(p0p1, p1p2, t);

	return p;
}

Vector3 Vector3::Reflect(const Vector3 &input, const Vector3 &normal) {
	return input - normal * (2.0f * Dot(input, normal));
}

Vector3 Vector3::Project(const Vector3 &v1, const Vector3 &v2) {
	float dot = Dot(v1, v2);
	float lengthSquared = Dot(v2, v2);

	if (lengthSquared == 0.0f) {
		return {};
	}

	return v2 * (dot / lengthSquared);
}
