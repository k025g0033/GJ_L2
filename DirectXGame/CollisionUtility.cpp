#include "CollisionUtility.h"
#include "Collision3D.h"

// NOTE: このファイルは意図的に "using namespace KamataEngine;" を書かない。
// Collision3D.h側のVector3(グローバル)とKamataEngine::Vector3が両方スコープに入ると
// 「Vector3」という書き方があいまいになりコンパイルエラーになるため、
// KamataEngine側の型は必ず "KamataEngine::" を付けて書く。

namespace CollisionUtility {

bool IsCollisionBoxAndSphere(
    const KamataEngine::Vector3& boxCenter, float halfWidth, float halfHeight, float halfDepth,
    const KamataEngine::Vector3& sphereCenter, float sphereRadius) {

	// KamataEngine::Vector3 -> Collision3D用のVector3へ変換
	AABB aabb;
	aabb.min = {boxCenter.x - halfWidth, boxCenter.y - halfHeight, boxCenter.z - halfDepth};
	aabb.max = {boxCenter.x + halfWidth, boxCenter.y + halfHeight, boxCenter.z + halfDepth};

	Sphere sphere;
	sphere.center = {sphereCenter.x, sphereCenter.y, sphereCenter.z};
	sphere.radius = sphereRadius;
	sphere.color = 0xffffffff;

	return Collision3D::IsCollisionAabbAndSphere(aabb, sphere);
}

} // namespace CollisionUtility
