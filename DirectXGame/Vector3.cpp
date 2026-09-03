#include "Vector3.h"
#include <assert.h>
#include <cmath>
#include "Matrix4x4.h"

/// ---Vector3---
// 加算
Vector3 Vector3::operator+(const Vector3& obj) const {
    return { x + obj.x, y + obj.y, z + obj.z };
}

// 減算
Vector3 Vector3::operator-(const Vector3& obj) const {
    return { x - obj.x, y - obj.y, z - obj.z };
}

// スカラー倍
Vector3 Vector3::operator*(float scalar) const {
    return { x * scalar, y * scalar, z * scalar };
}

// 内積
float Vector3::Dot(const Vector3& v1, const Vector3& v2) {
	float result;
	result =
		v1.x * v2.x +
		v1.y * v2.y +
		v1.z * v2.z;
	return result;
}

// 長さ(ノルム)
float Vector3::Length(const Vector3& v) {
	float result;
	result = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	return result;
}

// 正規化
Vector3 Vector3::Normalize(const Vector3& v) {
	Vector3 result;
	// 長さを計算
	float length = Length(v);

	// 0の時はエラーが出るのでそのまま0を返す分岐を作る
	if (length != 0.0f) {
		result.x = v.x / length;
		result.y = v.y / length;
		result.z = v.z / length;
	} else {
		// 長さが0の場合は０を返す
		result = {0.0f, 0.0f, 0.0f};
	}

	return result;
}

// クロス積
Vector3 Vector3::Cross(const Vector3& v1, const Vector3& v2) {
	Vector3 result;

	// a * b = {(a.y * b.z - a.z * b.y), (a.z * b.x - a.x * b.z), (a.x * b.y - a.y * b.x)}
	result.x = v1.y * v2.z - v1.z * v2.y;
	result.y = v1.z * v2.x - v1.x * v2.z;
	result.z = v1.x * v2.y - v1.y * v2.x;

	return result;
}

// 正射影ベクトル
Vector3 Vector3::Project(const Vector3& v1, const Vector3& v2) {
	float dot = Dot(v1, Normalize(v2));
	return Normalize(v2) * dot; // スカラー倍のオーバーロードを利用
}

// NOTE: Vector3::ScreenPrintf は Novice フレームワーク依存（Novice::ScreenPrintf）だったため削除。
// このプロジェクトではImGuiのImGui::Textなどで代替すること。

// 座標変換
Vector3 Vector3::Transform(const Vector3& vector, const Matrix4x4& matrix) {
	Vector3 result{};

	// (x, y, z, 1) * Matrix
	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];

	// w成分の計算
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];

	// 同次座標のwで割る
	assert(w != 0.0f);
	result.x /= w;
	result.y /= w;
	result.z /= w;

	return result;
}

// 線形補間
Vector3 Vector3::Lerp(const Vector3& v1, const Vector3& v2, float t) {
	Vector3 result;

	// t=0でv1、t=1でv2となる位置を各成分ごとに求める
	result = {
		.x = (1.0f - t) * v1.x + t * v2.x,
		.y = (1.0f - t) * v1.y + t * v2.y,
		.z = (1.0f - t) * v1.z + t * v2.z,
	};

	return result;
}