#include "HoT/Editor/Quat.h"
#include <cmath>

namespace HoT::Editor {

static float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

Quat NormalizeQuat(Quat q) {
    float lenSq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;

    if (lenSq <= 0.0f) {
        return { 1.0f, 0.0f, 0.0f, 0.0f };
    }

    float invLen = 1.0f / sqrtf(lenSq);

    return { q.w * invLen, q.x * invLen, q.y * invLen, q.z * invLen };
}

Quat MultiplyQuat(Quat a, Quat b) {
    return {
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

Quat AxisAngleToQuat(float x, float y, float z, float angle) {
    float lenSq = x * x + y * y + z * z;

    if (lenSq <= 0.0f) {
        return { 1.0f, 0.0f, 0.0f, 0.0f };
    }

    float invLen = 1.0f / sqrtf(lenSq);
    float half = angle * 0.5f;
    float s = sinf(half);

    return NormalizeQuat({ cosf(half), x * invLen * s, y * invLen * s, z * invLen * s });
}

uint64_t PackQuat(Quat q) {
    q = NormalizeQuat(q);

    if (q.w < 0.0f) {
        q.w = -q.w;
        q.x = -q.x;
        q.y = -q.y;
        q.z = -q.z;
    }

    float x = Clamp(q.x, -1.0f, 1.0f);
    float y = Clamp(q.y, -1.0f, 1.0f);
    float z = Clamp(q.z, -1.0f, 1.0f);

    int32_t xi = static_cast<int32_t>(x * 2097152.0f + (x >= 0.0f ? 0.5f : -0.5f));
    int32_t yi = static_cast<int32_t>(y * 1048576.0f + (y >= 0.0f ? 0.5f : -0.5f));
    int32_t zi = static_cast<int32_t>(z * 1048576.0f + (z >= 0.0f ? 0.5f : -0.5f));

    if (xi > 2097151) {
        xi = 2097151;
    }
    if (xi < -2097152) {
        xi = -2097152;
    }
    if (yi > 1048575) {
        yi = 1048575;
    }
    if (yi < -1048576) {
        yi = -1048576;
    }
    if (zi > 1048575) {
        zi = 1048575;
    }
    if (zi < -1048576) {
        zi = -1048576;
    }

    uint64_t xu = static_cast<uint64_t>(static_cast<uint32_t>(xi)) & 0x3FFFFFULL;
    uint64_t yu = static_cast<uint64_t>(static_cast<uint32_t>(yi)) & 0x1FFFFFULL;
    uint64_t zu = static_cast<uint64_t>(static_cast<uint32_t>(zi)) & 0x1FFFFFULL;

    return (xu << 42) | (yu << 21) | zu;
}

Quat UnpackQuat(uint64_t packed) {
    int32_t xi = static_cast<int32_t>((packed >> 42) & 0x3FFFFFULL);
    int32_t yi = static_cast<int32_t>((packed >> 21) & 0x1FFFFFULL);
    int32_t zi = static_cast<int32_t>(packed & 0x1FFFFFULL);

    if (xi & 0x200000) {
        xi |= ~0x3FFFFF;
    }
    if (yi & 0x100000) {
        yi |= ~0x1FFFFF;
    }
    if (zi & 0x100000) {
        zi |= ~0x1FFFFF;
    }

    float x = static_cast<float>(xi) / 2097152.0f;
    float y = static_cast<float>(yi) / 1048576.0f;
    float z = static_cast<float>(zi) / 1048576.0f;

    float ww = 1.0f - x * x - y * y - z * z;
    float w = ww > 0.0f ? sqrtf(ww) : 0.0f;

    return NormalizeQuat({ w, x, y, z });
}

void QuatToEuler(Quat q, float& roll, float& pitch, float& yaw) {
    q = NormalizeQuat(q);

    float sinrCosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosrCosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    roll = atan2f(sinrCosp, cosrCosp);

    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    pitch = fabsf(sinp) >= 1.0f ? copysignf(3.14159265358979323846f * 0.5f, sinp) : asinf(sinp);

    float sinyCosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosyCosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    yaw = atan2f(sinyCosp, cosyCosp);
}

}
