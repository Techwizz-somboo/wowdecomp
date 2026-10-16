#if defined(WOWDECOMP_ARCH_64)

#include <atomic>
#include <cstdint>
#include <windows.h>
#include "Engine/Source/Os/OsMemory.h"

static const uintptr_t s_nearRange = 0x60000000;

// HoT: walks resume past the last block handed out, so an allocation no longer scans every live block before it.
static std::atomic<uintptr_t> s_nearHint;

// HoT: only the pages the caller asked for are committed instead of the whole 64KB granule.
static void* AllocAt(uintptr_t candidate, uintptr_t bytes, size_t size) {
    void* ptr = VirtualAlloc(reinterpret_cast<void*>(candidate), bytes, MEM_RESERVE, PAGE_READWRITE);

    if (!ptr) {
        return nullptr;
    }

    if (!VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE)) {
        VirtualFree(ptr, 0, MEM_RELEASE);
        return nullptr;
    }

    s_nearHint.store(candidate + bytes, std::memory_order_relaxed);
    return ptr;
}

static void* AllocUpward(uintptr_t address, uintptr_t maximum, uintptr_t bytes, size_t size, uintptr_t granularity) {
    MEMORY_BASIC_INFORMATION region;

    while (address < maximum && VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region))) {
        uintptr_t base = reinterpret_cast<uintptr_t>(region.BaseAddress);
        uintptr_t end = base + region.RegionSize;

        if (region.State == MEM_FREE) {
            uintptr_t candidate = ((base > address ? base : address) + granularity - 1) & ~(granularity - 1);

            if (candidate + bytes <= end && candidate + bytes <= maximum) {
                void* ptr = AllocAt(candidate, bytes, size);

                if (ptr) {
                    return ptr;
                }
            }
        }

        address = end;
    }

    return nullptr;
}

void* OsAllocNear(const void* anchor, size_t size) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    uintptr_t granularity = info.dwAllocationGranularity;
    uintptr_t bytes = (size + granularity - 1) & ~(granularity - 1);
    uintptr_t center = reinterpret_cast<uintptr_t>(anchor);
    uintptr_t minimum = reinterpret_cast<uintptr_t>(info.lpMinimumApplicationAddress);
    uintptr_t maximum = reinterpret_cast<uintptr_t>(info.lpMaximumApplicationAddress);

    if (center > minimum + s_nearRange) {
        minimum = center - s_nearRange;
    }

    if (center + s_nearRange < maximum) {
        maximum = center + s_nearRange;
    }

    uintptr_t hint = s_nearHint.load(std::memory_order_relaxed);

    if (hint > center && hint < maximum) {
        if (void* ptr = AllocUpward(hint, maximum, bytes, size, granularity)) {
            return ptr;
        }
    }

    if (void* ptr = AllocUpward(center, maximum, bytes, size, granularity)) {
        return ptr;
    }

    MEMORY_BASIC_INFORMATION region;
    uintptr_t address = center;

    while (address > minimum && VirtualQuery(reinterpret_cast<void*>(address - 1), &region, sizeof(region))) {
        uintptr_t base = reinterpret_cast<uintptr_t>(region.BaseAddress);
        uintptr_t end = base + region.RegionSize;

        if (end > address) {
            end = address;
        }

        if (region.State == MEM_FREE && end - base >= bytes) {
            uintptr_t candidate = (end - bytes) & ~(granularity - 1);

            if (candidate >= base && candidate >= minimum) {
                void* ptr = AllocAt(candidate, bytes, size);

                if (ptr) {
                    return ptr;
                }
            }
        }

        address = base;
    }

    return nullptr;
}

void OsFreeNear(void* ptr) {
    if (ptr) {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
}

#endif
