#include <cstdio>
#include <cstring>
#include "HoT/Editor/Map/Adt/AdtDocument.h"

namespace HoT::MapEditor::Adt {

namespace {
constexpr uint32_t kIffSize = 8;
constexpr uint32_t kMcnkHeaderSize = 128;

void Append(std::vector<uint8_t>& out, const void* data, size_t size) {
    const uint8_t* begin = static_cast<const uint8_t*>(data);
    out.insert(out.end(), begin, begin + size);
}

void AppendU32(std::vector<uint8_t>& out, uint32_t value) {
    Append(out, &value, sizeof(value));
}

void PatchU32(std::vector<uint8_t>& out, uint32_t at, uint32_t value) {
    std::memcpy(out.data() + at, &value, sizeof(value));
}

void RelayoutMcnkHeader(const Mcnk& chunk, SMChunk& header) {
    header = chunk.m_header;

    uint32_t at = kIffSize + kMcnkHeaderSize;
    for (const SubChunk& sub : chunk.m_subs) {
        uint32_t span = kIffSize + static_cast<uint32_t>(sub.m_data.size());

        switch (sub.m_id) {
            case kMCVT:
                header.m_ofsHeight = at;
                break;
            case kMCNR:
                header.m_ofsNormal = at;
                break;
            case kMCLY:
                header.m_ofsLayer = at;
                break;
            case kMCRF:
                header.m_ofsRefs = at;
                break;
            case kMCAL:
                header.m_ofsAlpha = at;
                if (sub.Resized()) {
                    header.m_sizeAlpha = span;
                }
                break;
            case kMCSH:
                header.m_ofsShadow = at;
                if (sub.Resized()) {
                    header.m_sizeShadow = static_cast<uint32_t>(sub.m_data.size());
                }
                break;
            case kMCSE:
                header.m_ofsSndEmitters = at;
                break;
            case kMCLQ:
                header.m_ofsLiquid = at;
                if (sub.Resized()) {
                    header.m_sizeLiquid = span;
                }
                break;
            case kMCCV:
                header.m_ofsMCCV = at;
                break;
            default:
                break;
        }

        at += span;
    }
}
}

bool Write(const AdtDocument& doc, std::vector<uint8_t>& out, std::string& error) {
    out.clear();

    uint32_t mhdrData = 0;
    uint32_t offsets[12] = {};
    uint32_t mcinData = 0;
    bool sawMhdr = false;
    bool sawMcin = false;

    uint32_t mcnkPos[256] = {};
    uint32_t mcnkSize[256] = {};
    int32_t mcnkCount = 0;

    for (const TopChunk& top : doc.m_order) {
        uint32_t start = static_cast<uint32_t>(out.size());

        if (top.m_mcnkIndex >= 0) {
            if (top.m_mcnkIndex >= static_cast<int32_t>(doc.m_chunks.size())) {
                error = "MCNK index out of range";
                return false;
            }

            const Mcnk& chunk = doc.m_chunks[top.m_mcnkIndex];
            SMChunk header{};
            RelayoutMcnkHeader(chunk, header);

            AppendU32(out, kMCNK);
            AppendU32(out, 0);
            Append(out, &header, kMcnkHeaderSize);

            for (const SubChunk& sub : chunk.m_subs) {
                AppendU32(out, sub.m_id);
                AppendU32(out, sub.DeclaredSize());
                if (!sub.m_data.empty()) {
                    Append(out, sub.m_data.data(), sub.m_data.size());
                }
            }

            uint32_t total = static_cast<uint32_t>(out.size()) - start;
            PatchU32(out, start + 4, total - kIffSize);

            if (mcnkCount < 256) {
                mcnkPos[mcnkCount] = start;
                mcnkSize[mcnkCount] = total;
            }
            ++mcnkCount;
            continue;
        }

        AppendU32(out, top.m_id);
        AppendU32(out, static_cast<uint32_t>(top.m_data.size()));
        if (!top.m_data.empty()) {
            Append(out, top.m_data.data(), top.m_data.size());
        }

        switch (top.m_id) {
            case kMHDR:
                mhdrData = start + kIffSize;
                sawMhdr = true;
                break;
            case kMCIN:
                offsets[0] = start;
                mcinData = start + kIffSize;
                sawMcin = true;
                break;
            case kMTEX:
                offsets[1] = start;
                break;
            case kMMDX:
                offsets[2] = start;
                break;
            case kMMID:
                offsets[3] = start;
                break;
            case kMWMO:
                offsets[4] = start;
                break;
            case kMWID:
                offsets[5] = start;
                break;
            case kMDDF:
                offsets[6] = start;
                break;
            case kMODF:
                offsets[7] = start;
                break;
            case kMFBO:
                offsets[8] = start;
                break;
            case kMH2O:
                offsets[9] = start;
                break;
            case kMTXF:
                offsets[10] = start;
                break;
            default:
                break;
        }
    }

    if (!sawMhdr) {
        error = "document has no MHDR";
        return false;
    }

    SMAreaHeader header = doc.m_header;
    uint32_t* fields[11] = { &header.m_mcin, &header.m_mtex, &header.m_mmdx, &header.m_mmid,
        &header.m_mwmo, &header.m_mwid, &header.m_mddf, &header.m_modf, &header.m_mfbo, &header.m_mh2o,
        &header.m_mtxf };

    for (int i = 0; i < 11; ++i) {
        *fields[i] = offsets[i] ? offsets[i] - mhdrData : 0;
    }

    std::memcpy(out.data() + mhdrData, &header, sizeof(header));

    if (sawMcin) {
        for (int32_t i = 0; i < mcnkCount && i < 256; ++i) {
            SMChunkInfo entry = doc.m_info[i];
            entry.m_offset = mcnkPos[i];
            entry.m_size = doc.m_mcinSizeIncludesHeader ? mcnkSize[i] : mcnkSize[i] - kIffSize;
            std::memcpy(out.data() + mcinData + i * sizeof(SMChunkInfo), &entry, sizeof(entry));
        }
    }

    return true;
}

}
