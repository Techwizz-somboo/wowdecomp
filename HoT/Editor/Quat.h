#ifndef HOT_EDITOR_QUAT_H
#define HOT_EDITOR_QUAT_H

#include <cstdint>

namespace HoT::Editor {

struct Quat {
    float w;
    float x;
    float y;
    float z;
};

Quat NormalizeQuat(Quat q);
Quat MultiplyQuat(Quat a, Quat b);
Quat AxisAngleToQuat(float x, float y, float z, float angle);

// The 64 bit packed rotation gameobjects carry, 22 bits of x and 21 each of y and z.
uint64_t PackQuat(Quat q);
Quat UnpackQuat(uint64_t packed);

void QuatToEuler(Quat q, float& roll, float& pitch, float& yaw);

}

#endif
