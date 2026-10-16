#include "HoT/Runtime/Layout.h"
#include <vector>
#include "Engine/Source/Frame/CFrameStrataNode.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "HoT/Runtime/LuaRuntime.h"

namespace HoT::Layout {

static std::vector<uint8_t> s_used;
static std::vector<uint32_t> s_remap;
static std::vector<uint32_t> s_bucketStart;
static std::vector<CSimpleFrame*> s_frames;
static std::vector<CSimpleFrame*> s_sorted;

void CompressStrata(CSimpleTop* top, int32_t strataIndex) {
    LuaRuntime::Timer timer(LuaRuntime::Phase::Compress);

    if (strataIndex < FRAME_STRATA_WORLD || strataIndex > FRAME_STRATA_TOOLTIP) {
        return;
    }

    CFrameStrata* strata = top->m_strata[strataIndex];

    if (!strata) {
        return;
    }

    uint32_t topLevel = strata->m_topLevel;

    if (topLevel == 0) {
        return;
    }

    uint32_t levelCount = strata->m_levels.Count();
    s_used.assign(topLevel, 0);

    for (uint32_t level = 0; level < topLevel && level < levelCount; level++) {
        CFrameStrataNode* node = strata->m_levels[level];

        if (node && node->m_frames.Head()) {
            s_used[level] = 1;
        }
    }

    s_frames.clear();

    for (CSimpleFrame* frame = top->m_frames.Head(); frame; frame = top->m_frames.Next(frame)) {
        if (frame->m_strata != static_cast<uint32_t>(strataIndex)) {
            continue;
        }

        uint32_t level = static_cast<uint32_t>(frame->m_level);

        if (level >= topLevel) {
            continue;
        }

        s_used[level] = 1;
        s_frames.push_back(frame);
    }

    s_remap.resize(topLevel);
    uint32_t used = 0;

    for (uint32_t level = 0; level < topLevel; level++) {
        s_remap[level] = used;

        if (s_used[level]) {
            used++;
        }
    }

    if (used == topLevel) {
        return;
    }

    // Lowest levels move first so a frame never lands on a level still waiting to move.
    s_bucketStart.assign(topLevel + 1, 0);

    for (CSimpleFrame* frame : s_frames) {
        s_bucketStart[frame->m_level + 1]++;
    }

    for (uint32_t level = 0; level < topLevel; level++) {
        s_bucketStart[level + 1] += s_bucketStart[level];
    }

    s_sorted.resize(s_frames.size());

    for (CSimpleFrame* frame : s_frames) {
        s_sorted[s_bucketStart[frame->m_level]++] = frame;
    }

    for (CSimpleFrame* frame : s_sorted) {
        uint32_t level = static_cast<uint32_t>(frame->m_level);
        uint32_t target = s_remap[level];

        if (target != level) {
            frame->SetFrameLevel(static_cast<int32_t>(target), 0);
        }
    }

    strata->m_topLevel = used;
}

}
