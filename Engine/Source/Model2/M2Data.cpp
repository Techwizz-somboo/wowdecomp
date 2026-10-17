#include "Engine/Source/Model2/M2Data.h"
#include <cstdio>
#include "Engine/Source/Storm/H/storm.h"

void M2ArrayOffsetOverflow(const void* array, const void* data) {
    char message[128];
    snprintf(message, sizeof(message), "M2Array at %p cannot reach %p with a 32-bit offset", array, data);
    SErrDisplayAppFatalMessage(message, __FILE__, __LINE__);
}

// VERIFY: f762ece4 auto
// OFFSET: 0x0081CE60
ubyte4::operator uint32_t() const {
    return *reinterpret_cast<const uint32_t*>(this->b);
}

// VERIFY: 501d6d87 auto
// OFFSET: 0x008248B0
uint8_t ubyte4::operator[](uint32_t index) const {
    return this->b[index];
}

// VERIFY: 0535117b manual
// OFFSET: 0x00978AD0
fixed16::fixed16(float value) {
    if (value < 1.0f) {
        if (value > -1.0f) {
            this->m_value = static_cast<int16_t>(value * 32767.0f + 0.5f);
        } else {
            this->m_value = -32767;
        }
    } else {
        this->m_value = 0x7FFF;
    }
}
