#pragma once

// 前方宣言
struct Matrix4x4;

// --- Vector3 ---
struct Vector3 {
	float x, y, z;

	// 演算子オーバーロードの宣言
	Vector3 operator+(const Vector3& obj) const;
	Vector3 operator-(const Vector3& obj) const;
	Vector3 operator*(float scalar) const;

	// ---静的関数の宣言---
	// 内積
	static float Dot(const Vector3& v1, const Vector3& v2);
	// 長さ(ノルム)
	static float Length(const Vector3& v);
	// 正規化
	static Vector3 Normalize(const Vector3& v);
	// クロス積
	static Vector3 Cross(const Vector3& v1, const Vector3& v2);
	// 正射影ベクトル
	static Vector3 Project(const Vector3& v1, const Vector3& v2);
	// 座標変換
	static Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);
	// 線形補間
	static Vector3 Lerp(const Vector3& v1, const Vector3& v2, float t);
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};