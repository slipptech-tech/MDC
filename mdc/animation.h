
#pragma once
#include <algorithm>
#include <cmath>

namespace MDCAnimation {
    inline float EaseOut(float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return 1.0f - std::pow(1.0f - value, 3.0f);
    }

    inline float Lerp(float from, float to, float speed) {
        return from + (to - from) * std::clamp(speed, 0.0f, 1.0f);
    }

    struct AnimatedValue {
        float value = 0.0f;
        float target = 0.0f;

        void Update(float speed = 0.18f) {
            value = Lerp(value, target, speed);
        }

        void Set(float newTarget) {
            target = newTarget;
        }
    };
}
