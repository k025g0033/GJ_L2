#pragma once

#include <algorithm>

namespace AudioSettings {

inline int seLevel = 10;
inline int bgmLevel = 10;

inline float GetSeScale() { return static_cast<float>(seLevel) / 10.0f; }
inline float GetBgmScale() { return static_cast<float>(bgmLevel) / 10.0f; }
inline float GetSeVolume() { return 0.3f * GetSeScale(); }

inline void ChangeSeLevel(int amount) { seLevel = std::clamp(seLevel + amount, 0, 10); }
inline void ChangeBgmLevel(int amount) { bgmLevel = std::clamp(bgmLevel + amount, 0, 10); }

} // namespace AudioSettings
