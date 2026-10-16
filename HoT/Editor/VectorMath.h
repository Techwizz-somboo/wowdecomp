#ifndef HOT_EDITOR_VECTORMATH_H
#define HOT_EDITOR_VECTORMATH_H

#include <cmath>
#include "Engine/Source/Tempest/C3Vector.h"

namespace HoT::Editor::VectorMath {

inline C3Vector Add(const C3Vector& left, const C3Vector& right) {
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

inline C3Vector Subtract(const C3Vector& left, const C3Vector& right) {
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

inline C3Vector Scale(const C3Vector& value, float scalar) {
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

inline float Dot(const C3Vector& left, const C3Vector& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

inline C3Vector Cross(const C3Vector& left, const C3Vector& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

inline float Length(const C3Vector& value) {
    return sqrtf(Dot(value, value));
}

inline C3Vector Normalize(C3Vector value) {
    float length = Length(value);

    if (length <= 0.0f) {
        return {};
    }

    return Scale(value, 1.0f / length);
}

}

#endif
