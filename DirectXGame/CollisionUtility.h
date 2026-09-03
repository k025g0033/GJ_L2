#pragma once
#include "KamataEngine.h"

// NOTE: このプロジェクトでは、当たり判定の計算に(以前から使っていた)Collision3Dクラスを利用する。
// Collision3Dクラスは KamataEngine とは別の独自のVector3/Matrix4x4型を使っているため、
// KamataEngine::Vector3 <-> Collision3D用Vector3 の変換をこのファイルの中だけで行い、
// GameSceneなど他のファイルからはKamataEngineの型だけを使って呼び出せるようにする。
// （こうしておくことで、"using namespace KamataEngine;" を使っている他のファイルで
// 　Vector3という名前が「KamataEngine::Vector3」なのか「Collision3D用のVector3」なのか
// 　あいまいになってコンパイルエラーになる問題を避けられる）
namespace CollisionUtility {

// 直方体(AABB)と球(Sphere)の当たり判定
// center/halfWidth/halfHeight/halfDepth : 直方体側（例:プレイヤー）の中心座標と各軸方向の半径
// sphereCenter/sphereRadius             : 球側（例:クローンの素）の中心座標と半径
bool IsCollisionBoxAndSphere(
    const KamataEngine::Vector3& boxCenter, float halfWidth, float halfHeight, float halfDepth,
    const KamataEngine::Vector3& sphereCenter, float sphereRadius);

} // namespace CollisionUtility
