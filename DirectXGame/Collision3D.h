#pragma once

#include "Vector3.h"
#include "Matrix4x4.h"

// 線
struct Line { // 直線
	Vector3 origin; // 始点
	Vector3 diff; // 終点への差分ベクトル
};

struct Ray { // 半直線
	Vector3 origin; // 始点
	Vector3 diff; // 終点への差分ベクトル
};

struct Segment { // 線分
	Vector3 origin; // 始点
	Vector3 diff; // 終点への差分ベクトル
};

// 球
struct Sphere {
	Vector3 center; // 中心点
	float radius; // 半径
	unsigned int color;
};

// 平面
struct Plane {
	Vector3 normal; // 法線
	float distance; // 距離
};

// 三角形
struct Triangle {
	Vector3 vertices[3]; // 頂点
};

// ==========================================

/// --- AABB構造体 ---
struct AABB {
	Vector3 min; // 最小点
	Vector3 max; // 最大点
};

/// --- OBB構造体 ---
// AABB + 回転 = OBB
struct OBB {
	Vector3 center; // 中心点
	Vector3 orientations[3]; // 座標軸 正規化・直交必須 3x3回転行列の各行
	Vector3 size; // 座標軸方向の長さの半分 中心から面までの距離S
};

// ==========================================

class Collision3D {
public:
	// 正射影ベクトルと最近接点
	static Vector3 ClosestPoint(const Vector3& point, const Segment& segment);

	// 球と球の当たり判定
	static bool IsCollisionSphereAndSphere(const Sphere& s1, const Sphere& s2);

	// 球と平面の当たり判定
	static bool IsCollisionSphereAndPlane(const Sphere& sphere, const Plane& plane);

	// 線と平面の当たり判定
	static bool IsCollisionLineAndPlane(const Segment& segment, const Plane& plane);

	// 三角形と線の当たり判定
	static bool IsCollisionTriangleAndSegment(const Triangle& triangle, const Segment& segment);

	// ==========================================

	/// --- AABB ---
	// AABB同士の衝突判定
	static bool IsCollisionAabbAndAabb(const AABB& a, const AABB& b);

	// AABBと球の衝突判定
	static bool IsCollisionAabbAndSphere(const AABB& aabb, const Sphere& sphere);

	// AABBと線分の衝突判定
	static bool IsCollisionAabbAndSegment(const AABB& aabb, const Segment& segment);

	/// --- OBB ---
	// OBBをWorld座標系へ変換する行列を作成する関数
	static Matrix4x4 MakeOBBWorldMatrix(const OBB& obb);

	// OBBと球の衝突判定
	static bool IsCollisionObbAndSphere(const OBB& obb, const Sphere& sphere);

	// OBBと線の衝突判定
	static bool IsCollisionObbAndSegment(const OBB& obb, const Segment& segment);

	// OBBとOBBの衝突判定
	static bool IsCollisionObbAndObb(const OBB& obb1, const OBB& obb2);

	// ==========================================
	// NOTE: 元々ここにあった描画関数（DrawSphere / DrawPlane / DrawTriangle / DrawAABB / DrawOBB）は
	// 「Novice」フレームワーク依存（Novice::DrawLine）だったため削除済み。
	// このプロジェクトではKamataEngineの PrimitiveDrawer::DrawLine3d などで代替すること。

private:
	// OBBの8頂点をワールド座標で求める
	static void GetObbVertices(const OBB& obb, Vector3 vertices[8]);

	// 指定した軸で2つのOBBが分離しているか判定する
	static bool IsSeparatedOnAxis(const Vector3& axis, const Vector3 vertices1[8], const Vector3 vertices2[8]);
};