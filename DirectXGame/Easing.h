#pragma once

#include <algorithm>
#include <cmath>

/// <summary>
/// イージング計算
/// </summary>
class Easing {
public:
	// 最初が非常に速く、終わり際がゆっくりになる
	static float EaseOutExpo(float t) {
		t = std::clamp(t, 0.0f, 1.0f);

		if (t >= 1.0f) {
			return 1.0f;
		}

		return 1.0f - std::pow(2.0f, -10.0f * t);
	}

	// 最初が速く、徐々に減速する
	static float EaseOutCubic(float t) {
		t = std::clamp(t, 0.0f, 1.0f);
		float inverse = 1.0f - t;
		return 1.0f - inverse * inverse * inverse;
	}

	// 最初が速く、終わり際に大きく減速する
	static float EaseOutQuint(float t) {
		t = std::clamp(t, 0.0f, 1.0f);
		float inverse = 1.0f - t;
		return 1.0f - inverse * inverse * inverse * inverse * inverse;
	}

	// 開始値から終了値まで、終点に近づくほど減速しながら補間する
	static float EaseOut(float start, float end, float t) {
		// 進行度を0.0f～1.0fに制限
		t = std::clamp(t, 0.0f, 1.0f);

		// EaseOutの進行度を計算
		float easedT = 1.0f - (1.0f - t) * (1.0f - t);

		// 開始値から終了値まで補間
		return start + (end - start) * easedT;
	}

	// 開始値から終了値まで、徐々に加速しながら補間する
	static float EaseIn(float start, float end, float t) {
		t = std::clamp(t, 0.0f, 1.0f);

		float easedT = t * t;

		return start + (end - start) * easedT;
	}
};
