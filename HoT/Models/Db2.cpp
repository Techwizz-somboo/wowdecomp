#include "HoT/Models/Db2.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Core/Log.h"

namespace HoT::Models {

static const uint32_t MAGIC_WDC1 = 0x31434457;
static const uint32_t MAGIC_WDC2 = 0x32434457;
static const uint32_t MAGIC_1SLC = 0x434C5331;
static const uint32_t MAGIC_WDC3 = 0x33434457;

#pragma pack(push, 1)
struct FieldStructure {
    int16_t m_size;
    uint16_t m_position;
};

struct FieldStorageInfo {
    uint16_t m_offsetBits;
    uint16_t m_sizeBits;
    uint32_t m_additionalDataSize;
    uint32_t m_storageType;
    uint32_t m_p1;
    uint32_t m_p2;
    uint32_t m_p3;
};

struct Wdc1Header {
    uint32_t m_magic;
    uint32_t m_recordCount;
    uint32_t m_fieldCount;
    uint32_t m_recordSize;
    uint32_t m_stringTableSize;
    uint32_t m_tableHash;
    uint32_t m_layoutHash;
    uint32_t m_minId;
    uint32_t m_maxId;
    uint32_t m_locale;
    uint32_t m_copyTableSize;
    uint16_t m_flags;
    uint16_t m_idIndex;
    uint32_t m_totalFieldCount;
    uint32_t m_bitpackedDataOffset;
    uint32_t m_lookupColumnCount;
    uint32_t m_offsetMapOffset;
    uint32_t m_idListSize;
    uint32_t m_fieldStorageInfoSize;
    uint32_t m_commonDataSize;
    uint32_t m_palletDataSize;
    uint32_t m_relationshipDataSize;
};

struct Wdc23Header {
    uint32_t m_magic;
    uint32_t m_recordCount;
    uint32_t m_fieldCount;
    uint32_t m_recordSize;
    uint32_t m_stringTableSize;
    uint32_t m_tableHash;
    uint32_t m_layoutHash;
    uint32_t m_minId;
    uint32_t m_maxId;
    uint32_t m_locale;
    uint16_t m_flags;
    uint16_t m_idIndex;
    uint32_t m_totalFieldCount;
    uint32_t m_bitpackedDataOffset;
    uint32_t m_lookupColumnCount;
    uint32_t m_fieldStorageInfoSize;
    uint32_t m_commonDataSize;
    uint32_t m_palletDataSize;
    uint32_t m_sectionCount;
};

struct Wdc2Section {
    uint64_t m_tactKeyHash;
    uint32_t m_fileOffset;
    uint32_t m_recordCount;
    uint32_t m_stringTableSize;
    uint32_t m_copyTableSize;
    uint32_t m_offsetMapOffset;
    uint32_t m_idListSize;
    uint32_t m_relationshipDataSize;
};

struct Wdc3Section {
    uint64_t m_tactKeyHash;
    uint32_t m_fileOffset;
    uint32_t m_recordCount;
    uint32_t m_stringTableSize;
    uint32_t m_offsetRecordsEnd;
    uint32_t m_idListSize;
    uint32_t m_relationshipDataSize;
    uint32_t m_offsetMapIdCount;
    uint32_t m_copyTableCount;
};
#pragma pack(pop)

struct Section {
    const uint8_t* m_records = nullptr;
    uint32_t m_recordCount = 0;
    uint32_t m_recordsFileOffset = 0;
    uint32_t m_stringFileOffset = 0;
    uint32_t m_stringSize = 0;
    const uint32_t* m_idList = nullptr;
    uint32_t m_idListCount = 0;
    std::vector<std::pair<uint32_t, uint32_t>> m_copyTable;
    std::vector<std::pair<uint32_t, uint16_t>> m_offsetMap;
    std::vector<uint32_t> m_offsetMapIds;
    std::unordered_map<uint32_t, uint32_t> m_relationship;
    uint32_t m_stringBase = 0;
};

struct FieldMeta {
    uint32_t m_storageType = 0;
    uint32_t m_offsetBits = 0;
    uint32_t m_sizeBits = 0;
    uint32_t m_elementBytes = 4;
    uint32_t m_elementCount = 1;
    uint32_t m_palletBase = 0;
    int32_t m_defaultValue = 0;
    bool m_signed = false;
    uint32_t m_outOffset = 0;
    bool m_inlineId = false;
    std::unordered_map<uint32_t, uint32_t> m_common;
};

struct Parsed {
    uint32_t m_version = 0;
    uint32_t m_recordSize = 0;
    uint32_t m_totalFieldCount = 0;
    uint16_t m_idIndex = 0;
    int32_t m_minId = 0;
    int32_t m_maxId = 0;
    const FieldStructure* m_fieldStruct = nullptr;
    const FieldStorageInfo* m_fieldInfo = nullptr;
    uint32_t m_fieldInfoCount = 0;
    bool m_hasIdList = false;
    const uint8_t* m_palletData = nullptr;
    const uint8_t* m_commonData = nullptr;
    std::vector<Section> m_sections;
};

struct Decoded {
    uint32_t m_rowSize = 0;
    std::vector<char> m_records;
    std::vector<int32_t> m_ids;
    std::vector<char> m_strings;
};

static uint32_t Read32(const uint8_t* p) {
    uint32_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static uint16_t Read16(const uint8_t* p) {
    uint16_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static uint64_t ReadBits(const uint8_t* p, uint32_t bitOffset, uint32_t bitCount) {
    if (!bitCount) {
        return 0;
    }

    uint32_t byteIndex = bitOffset >> 3;
    uint32_t shift = bitOffset & 7;
    uint32_t need = (bitCount + shift + 7) >> 3;
    uint64_t value = 0;

    for (uint32_t i = 0; i < need && i < 8; i++) {
        value |= static_cast<uint64_t>(p[byteIndex + i]) << (8 * i);
    }

    value >>= shift;

    if (bitCount < 64) {
        value &= (static_cast<uint64_t>(1) << bitCount) - 1;
    }

    return value;
}

static int32_t SignExtend(uint64_t value, uint32_t bits) {
    if (!bits || bits >= 32) {
        return static_cast<int32_t>(value);
    }

    uint64_t mask = static_cast<uint64_t>(1) << (bits - 1);
    return static_cast<int32_t>((value ^ mask) - mask);
}

static void ReadRelationship(const uint8_t* p, Section& section) {
    uint32_t count = Read32(p);
    const uint8_t* entries = p + 12;

    for (uint32_t i = 0; i < count; i++) {
        section.m_relationship[Read32(entries + 8 * i + 4)] = Read32(entries + 8 * i);
    }
}

static bool ParseWdc1(const uint8_t* d, uint32_t size, Parsed& out) {
    if (size < sizeof(Wdc1Header)) {
        return false;
    }

    const Wdc1Header& h = *reinterpret_cast<const Wdc1Header*>(d);
    out.m_version = 1;
    out.m_recordSize = h.m_recordSize;
    out.m_totalFieldCount = h.m_totalFieldCount;
    out.m_idIndex = h.m_idIndex;
    out.m_minId = static_cast<int32_t>(h.m_minId);
    out.m_maxId = static_cast<int32_t>(h.m_maxId);

    uint32_t cursor = sizeof(Wdc1Header);
    out.m_fieldStruct = reinterpret_cast<const FieldStructure*>(d + cursor);
    cursor += h.m_totalFieldCount * sizeof(FieldStructure);

    Section section;

    if (!(h.m_flags & 1)) {
        section.m_records = d + cursor;
        section.m_recordCount = h.m_recordCount;
        section.m_recordsFileOffset = cursor;
        cursor += h.m_recordCount * h.m_recordSize;
        section.m_stringFileOffset = cursor;
        section.m_stringSize = h.m_stringTableSize;
        cursor += h.m_stringTableSize;
    } else {
        cursor = h.m_offsetMapOffset;
        uint32_t entries = h.m_maxId >= h.m_minId ? h.m_maxId - h.m_minId + 1 : 0;

        for (uint32_t i = 0; i < entries; i++) {
            uint32_t offset = Read32(d + cursor);
            uint16_t length = Read16(d + cursor + 4);
            cursor += 6;

            if (offset && length) {
                section.m_offsetMap.emplace_back(offset, length);
            }
        }

        section.m_recordCount = static_cast<uint32_t>(section.m_offsetMap.size());
    }

    if (h.m_idListSize) {
        section.m_idList = reinterpret_cast<const uint32_t*>(d + cursor);
        section.m_idListCount = h.m_idListSize / 4;
        cursor += h.m_idListSize;
        out.m_hasIdList = true;
    }

    if (h.m_copyTableSize) {
        for (uint32_t i = 0; i < h.m_copyTableSize / 8; i++) {
            section.m_copyTable.emplace_back(Read32(d + cursor + 8 * i), Read32(d + cursor + 8 * i + 4));
        }

        cursor += h.m_copyTableSize;
    }

    out.m_fieldInfo = reinterpret_cast<const FieldStorageInfo*>(d + cursor);
    out.m_fieldInfoCount = h.m_fieldStorageInfoSize / sizeof(FieldStorageInfo);
    cursor += h.m_fieldStorageInfoSize;
    out.m_palletData = d + cursor;
    cursor += h.m_palletDataSize;
    out.m_commonData = d + cursor;
    cursor += h.m_commonDataSize;

    if (h.m_relationshipDataSize) {
        ReadRelationship(d + cursor, section);
    }

    out.m_sections.push_back(std::move(section));
    return cursor <= size;
}

static bool ParseWdc23(const uint8_t* d, uint32_t size, Parsed& out, uint32_t version) {
    if (size < sizeof(Wdc23Header)) {
        return false;
    }

    const Wdc23Header& h = *reinterpret_cast<const Wdc23Header*>(d);
    out.m_version = version;
    out.m_recordSize = h.m_recordSize;
    out.m_totalFieldCount = h.m_totalFieldCount;
    out.m_idIndex = h.m_idIndex;
    out.m_minId = static_cast<int32_t>(h.m_minId);
    out.m_maxId = static_cast<int32_t>(h.m_maxId);

    uint32_t cursor = sizeof(Wdc23Header);
    const uint8_t* sectionHeaders = d + cursor;
    cursor += h.m_sectionCount * (version == 3 ? sizeof(Wdc3Section) : sizeof(Wdc2Section));
    out.m_fieldStruct = reinterpret_cast<const FieldStructure*>(d + cursor);
    cursor += h.m_totalFieldCount * sizeof(FieldStructure);
    out.m_fieldInfo = reinterpret_cast<const FieldStorageInfo*>(d + cursor);
    out.m_fieldInfoCount = h.m_fieldStorageInfoSize / sizeof(FieldStorageInfo);
    cursor += h.m_fieldStorageInfoSize;
    out.m_palletData = d + cursor;
    cursor += h.m_palletDataSize;
    out.m_commonData = d + cursor;
    cursor += h.m_commonDataSize;

    if (cursor > size) {
        return false;
    }

    bool offsetMap = (h.m_flags & 1) != 0;

    for (uint32_t s = 0; s < h.m_sectionCount; s++) {
        Section section;

        if (version == 3) {
            const Wdc3Section& sh = reinterpret_cast<const Wdc3Section*>(sectionHeaders)[s];
            uint32_t p = sh.m_fileOffset;

            if (!offsetMap) {
                section.m_records = d + p;
                section.m_recordCount = sh.m_recordCount;
                section.m_recordsFileOffset = p;
                p += sh.m_recordCount * h.m_recordSize;
                section.m_stringFileOffset = p;
                section.m_stringSize = sh.m_stringTableSize;
                p += sh.m_stringTableSize;
            } else {
                section.m_recordsFileOffset = p;
                p = sh.m_offsetRecordsEnd;
            }

            if (sh.m_idListSize) {
                section.m_idList = reinterpret_cast<const uint32_t*>(d + p);
                section.m_idListCount = sh.m_idListSize / 4;
                p += sh.m_idListSize;
                out.m_hasIdList = true;
            }

            for (uint32_t i = 0; i < sh.m_copyTableCount; i++) {
                section.m_copyTable.emplace_back(Read32(d + p + 8 * i), Read32(d + p + 8 * i + 4));
            }

            p += sh.m_copyTableCount * 8;

            if (offsetMap) {
                const uint8_t* map = d + p;
                const uint8_t* ids = map + sh.m_offsetMapIdCount * 6;

                for (uint32_t i = 0; i < sh.m_offsetMapIdCount; i++) {
                    section.m_offsetMap.emplace_back(Read32(map + i * 6), Read16(map + i * 6 + 4));
                    section.m_offsetMapIds.push_back(Read32(ids + i * 4));
                }

                section.m_recordCount = sh.m_offsetMapIdCount;
                p += sh.m_offsetMapIdCount * 10;
            }

            if (sh.m_relationshipDataSize) {
                ReadRelationship(d + p, section);
            }
        } else {
            const Wdc2Section& sh = reinterpret_cast<const Wdc2Section*>(sectionHeaders)[s];
            uint32_t p = sh.m_fileOffset;

            if (!offsetMap) {
                section.m_records = d + p;
                section.m_recordCount = sh.m_recordCount;
                section.m_recordsFileOffset = p;
                p += sh.m_recordCount * h.m_recordSize;
                section.m_stringFileOffset = p;
                section.m_stringSize = sh.m_stringTableSize;
                p += sh.m_stringTableSize;
            } else {
                section.m_recordsFileOffset = p;
                uint32_t entries = h.m_maxId >= h.m_minId ? h.m_maxId - h.m_minId + 1 : 0;
                uint32_t map = sh.m_offsetMapOffset;

                for (uint32_t i = 0; i < entries; i++) {
                    uint32_t offset = Read32(d + map);
                    uint16_t length = Read16(d + map + 4);
                    map += 6;

                    if (offset && length) {
                        section.m_offsetMap.emplace_back(offset, length);
                    }
                }

                section.m_recordCount = static_cast<uint32_t>(section.m_offsetMap.size());
                p = map;
            }

            if (sh.m_idListSize) {
                section.m_idList = reinterpret_cast<const uint32_t*>(d + p);
                section.m_idListCount = sh.m_idListSize / 4;
                p += sh.m_idListSize;
                out.m_hasIdList = true;
            }

            for (uint32_t i = 0; i < sh.m_copyTableSize / 8; i++) {
                section.m_copyTable.emplace_back(Read32(d + p + 8 * i), Read32(d + p + 8 * i + 4));
            }

            p += sh.m_copyTableSize;

            if (sh.m_relationshipDataSize) {
                ReadRelationship(d + p, section);
            }
        }

        out.m_sections.push_back(std::move(section));
    }

    return true;
}

static uint32_t AlignUp(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

static uint32_t StdWidth(uint32_t bytes) {
    if (bytes <= 1) {
        return 1;
    }

    if (bytes == 2) {
        return 2;
    }

    return bytes <= 4 ? 4 : 8;
}

static uint32_t BuildLayout(const Parsed& p, std::vector<FieldMeta>& meta, bool hasRelationship, uint32_t& relationshipOffset) {
    meta.resize(p.m_fieldInfoCount);

    uint32_t palletCursor = 0;
    uint32_t commonCursor = 0;
    uint32_t offset = 4;
    uint32_t maxAlign = 4;

    for (uint32_t i = 0; i < p.m_fieldInfoCount; i++) {
        const FieldStorageInfo& info = p.m_fieldInfo[i];
        FieldMeta& m = meta[i];
        m.m_storageType = info.m_storageType;
        m.m_offsetBits = info.m_offsetBits;
        m.m_sizeBits = info.m_sizeBits;

        uint32_t diskBytes = 4;

        if (i < p.m_totalFieldCount && p.m_fieldStruct) {
            diskBytes = static_cast<uint32_t>((32 - p.m_fieldStruct[i].m_size) / 8);

            if (!diskBytes) {
                diskBytes = 4;
            }
        }

        m.m_elementBytes = StdWidth(diskBytes);

        switch (info.m_storageType) {
            case 0: {
                uint32_t elementBits = diskBytes * 8;
                uint32_t totalBits = info.m_sizeBits ? info.m_sizeBits : elementBits;
                m.m_elementCount = elementBits ? totalBits / elementBits : 1;

                if (!m.m_elementCount) {
                    m.m_elementCount = 1;
                }

                break;
            }
            case 1:
                m.m_signed = (info.m_p3 & 1) != 0;
                break;
            case 2: {
                m.m_defaultValue = static_cast<int32_t>(info.m_p1);
                const uint8_t* common = p.m_commonData + commonCursor;

                for (uint32_t k = 0; k < info.m_additionalDataSize / 8; k++) {
                    m.m_common[Read32(common + 8 * k)] = Read32(common + 8 * k + 4);
                }

                commonCursor += info.m_additionalDataSize;
                break;
            }
            case 3:
                m.m_palletBase = palletCursor;
                palletCursor += info.m_additionalDataSize;
                break;
            case 4:
                m.m_elementCount = info.m_p3 ? info.m_p3 : 1;
                m.m_palletBase = palletCursor;
                palletCursor += info.m_additionalDataSize;
                break;
            default:
                break;
        }

        if (!p.m_hasIdList && i == p.m_idIndex) {
            m.m_inlineId = true;
            continue;
        }

        if (m.m_elementBytes > maxAlign) {
            maxAlign = m.m_elementBytes;
        }

        offset = AlignUp(offset, m.m_elementBytes);
        m.m_outOffset = offset;
        offset += m.m_elementBytes * m.m_elementCount;
    }

    if (hasRelationship) {
        offset = AlignUp(offset, 4);
        relationshipOffset = offset;
        offset += 4;
    }

    return AlignUp(offset, maxAlign);
}

static void WriteValue(char* dst, uint32_t width, uint64_t value) {
    for (uint32_t b = 0; b < width && b < 8; b++) {
        dst[b] = static_cast<char>((value >> (8 * b)) & 0xFF);
    }
}

static void DecodeField(const FieldMeta& m, const Parsed& p, const uint8_t* record, int32_t id, char* out) {
    if (m.m_inlineId) {
        return;
    }

    char* dst = out + m.m_outOffset;

    switch (m.m_storageType) {
        case 0: {
            uint32_t diskBytes = m.m_sizeBits && m.m_elementCount ? m.m_sizeBits / 8 / m.m_elementCount : m.m_elementBytes;

            if (!diskBytes) {
                diskBytes = m.m_elementBytes;
            }

            for (uint32_t e = 0; e < m.m_elementCount; e++) {
                const uint8_t* src = record + m.m_offsetBits / 8 + e * diskBytes;
                uint64_t value = 0;

                for (uint32_t b = 0; b < diskBytes && b < 8; b++) {
                    value |= static_cast<uint64_t>(src[b]) << (8 * b);
                }

                WriteValue(dst + e * m.m_elementBytes, m.m_elementBytes, value);
            }

            break;
        }
        case 1: {
            uint64_t value = ReadBits(record, m.m_offsetBits, m.m_sizeBits);
            int32_t signedValue = m.m_signed ? SignExtend(value, m.m_sizeBits) : static_cast<int32_t>(value);
            WriteValue(dst, m.m_elementBytes, static_cast<uint32_t>(signedValue));
            break;
        }
        case 2: {
            auto it = m.m_common.find(static_cast<uint32_t>(id));
            int32_t value = it != m.m_common.end() ? static_cast<int32_t>(it->second) : m.m_defaultValue;
            WriteValue(dst, m.m_elementBytes, static_cast<uint32_t>(value));
            break;
        }
        case 3: {
            uint64_t index = ReadBits(record, m.m_offsetBits, m.m_sizeBits);
            WriteValue(dst, m.m_elementBytes, Read32(p.m_palletData + m.m_palletBase + index * 4));
            break;
        }
        case 4: {
            uint64_t index = ReadBits(record, m.m_offsetBits, m.m_sizeBits);

            for (uint32_t a = 0; a < m.m_elementCount; a++) {
                WriteValue(dst + a * m.m_elementBytes, m.m_elementBytes, Read32(p.m_palletData + m.m_palletBase + (index * m.m_elementCount + a) * 4));
            }

            break;
        }
        default:
            break;
    }
}

static int32_t ReadScalar(const FieldMeta& m, const Parsed& p, const uint8_t* record) {
    switch (m.m_storageType) {
        case 0: {
            uint32_t diskBytes = m.m_sizeBits ? m.m_sizeBits / 8 : m.m_elementBytes;

            if (!diskBytes || diskBytes > 8) {
                diskBytes = 4;
            }

            const uint8_t* src = record + m.m_offsetBits / 8;
            uint64_t value = 0;

            for (uint32_t b = 0; b < diskBytes; b++) {
                value |= static_cast<uint64_t>(src[b]) << (8 * b);
            }

            return static_cast<int32_t>(value);
        }
        case 1:
            return static_cast<int32_t>(ReadBits(record, m.m_offsetBits, m.m_sizeBits));
        case 3:
            return static_cast<int32_t>(Read32(p.m_palletData + m.m_palletBase + ReadBits(record, m.m_offsetBits, m.m_sizeBits) * 4));
        default:
            return 0;
    }
}

static bool Decode(const uint8_t* data, uint32_t size, Decoded& out, const uint32_t* stringColumns, uint32_t stringColumnCount) {
    if (size < 4) {
        return false;
    }

    uint32_t magic = Read32(data);
    Parsed p;
    bool ok;

    if (magic == MAGIC_WDC1) {
        ok = ParseWdc1(data, size, p);
    } else if (magic == MAGIC_WDC2 || magic == MAGIC_1SLC) {
        ok = ParseWdc23(data, size, p, 2);
    } else if (magic == MAGIC_WDC3) {
        ok = ParseWdc23(data, size, p, 3);
    } else {
        return false;
    }

    if (!ok) {
        return false;
    }

    bool hasRelationship = false;

    for (const Section& section : p.m_sections) {
        if (!section.m_relationship.empty()) {
            hasRelationship = true;
        }
    }

    std::vector<FieldMeta> meta;
    uint32_t relationshipOffset = 0;
    out.m_rowSize = BuildLayout(p, meta, hasRelationship, relationshipOffset);

    for (Section& section : p.m_sections) {
        section.m_stringBase = static_cast<uint32_t>(out.m_strings.size());

        if (section.m_stringSize) {
            if (section.m_stringFileOffset + section.m_stringSize > size) {
                return false;
            }

            const char* strings = reinterpret_cast<const char*>(data + section.m_stringFileOffset);
            out.m_strings.insert(out.m_strings.end(), strings, strings + section.m_stringSize);
        }
    }

    if (out.m_strings.empty()) {
        out.m_strings.push_back('\0');
    }

    bool multiSection = p.m_sections.size() > 1;
    size_t sectionRows = 0;
    size_t copyRows = 0;

    for (const Section& section : p.m_sections) {
        sectionRows += section.m_recordCount;
        copyRows += section.m_copyTable.size();
    }

    size_t plannedRows = sectionRows + copyRows;

    if (!plannedRows || !out.m_rowSize || plannedRows > 0x40000000u / out.m_rowSize) {
        return false;
    }

    out.m_records.resize(plannedRows * out.m_rowSize);
    out.m_ids.reserve(plannedRows);

    size_t rowCursor = 0;
    std::unordered_map<int32_t, uint32_t> idToRow;

    if (copyRows) {
        idToRow.reserve(sectionRows);
    }

    for (const Section& section : p.m_sections) {
        bool offsetMap = !section.m_offsetMap.empty();

        for (uint32_t i = 0; i < section.m_recordCount; i++) {
            const uint8_t* record = offsetMap ? data + section.m_offsetMap[i].first : section.m_records + static_cast<size_t>(i) * p.m_recordSize;

            size_t recordEnd = static_cast<size_t>(record - data) + (offsetMap ? section.m_offsetMap[i].second : p.m_recordSize);

            if (recordEnd > size) {
                return false;
            }

            int32_t id;

            if (offsetMap) {
                id = !section.m_offsetMapIds.empty() ? static_cast<int32_t>(section.m_offsetMapIds[i]) : p.m_minId + static_cast<int32_t>(i);
            } else if (section.m_idList && i < section.m_idListCount) {
                id = static_cast<int32_t>(section.m_idList[i]);
            } else if (p.m_idIndex < meta.size()) {
                id = ReadScalar(meta[p.m_idIndex], p, record);
            } else {
                id = static_cast<int32_t>(i);
            }

            char* row = out.m_records.data() + rowCursor;
            rowCursor += out.m_rowSize;
            WriteValue(row, 4, static_cast<uint32_t>(id));

            for (const FieldMeta& field : meta) {
                DecodeField(field, p, record, id, row);
            }

            if (hasRelationship) {
                auto it = section.m_relationship.find(i);
                WriteValue(row + relationshipOffset, 4, it != section.m_relationship.end() ? it->second : 0u);
            }

            if (stringColumns && (p.m_version >= 2 || multiSection) && !offsetMap) {
                for (uint32_t c = 0; c < stringColumnCount; c++) {
                    uint32_t f = stringColumns[c];

                    if (f >= meta.size() || meta[f].m_inlineId) {
                        continue;
                    }

                    char* dst = row + meta[f].m_outOffset;
                    uint32_t raw;
                    memcpy(&raw, dst, 4);

                    if (!raw) {
                        continue;
                    }

                    uint32_t fixed;

                    if (p.m_version >= 2) {
                        uint32_t fieldPos = section.m_recordsFileOffset + i * p.m_recordSize + meta[f].m_offsetBits / 8;
                        fixed = raw + fieldPos - section.m_stringFileOffset + section.m_stringBase;
                    } else {
                        fixed = raw + section.m_stringBase;
                    }

                    memcpy(dst, &fixed, 4);
                }
            }

            if (copyRows) {
                idToRow[id] = static_cast<uint32_t>(out.m_ids.size());
            }

            out.m_ids.push_back(id);
        }
    }

    for (const Section& section : p.m_sections) {
        for (const auto& copy : section.m_copyTable) {
            auto it = idToRow.find(static_cast<int32_t>(copy.second));

            if (it == idToRow.end()) {
                continue;
            }

            memcpy(out.m_records.data() + rowCursor, out.m_records.data() + static_cast<size_t>(it->second) * out.m_rowSize, out.m_rowSize);
            rowCursor += out.m_rowSize;
            out.m_ids.push_back(static_cast<int32_t>(copy.first));
        }
    }

    if (out.m_ids.size() != plannedRows) {
        out.m_records.resize(out.m_ids.size() * out.m_rowSize);
    }

    return !out.m_ids.empty();
}

static bool ReadWholeFile(const char* fileName, std::vector<uint8_t>& bytes) {
    std::string path = std::string("DBFilesClient\\") + fileName;
    bytes.clear();

    FILE* loose = fopen(path.c_str(), "rb");

    if (loose) {
        bool read = false;

        if (fseek(loose, 0, SEEK_END) == 0) {
            long end = ftell(loose);

            if (end > 0 && fseek(loose, 0, SEEK_SET) == 0) {
                bytes.resize(static_cast<size_t>(end));
                read = fread(bytes.data(), 1, bytes.size(), loose) == bytes.size();
            }
        }

        fclose(loose);

        if (read) {
            return true;
        }

        bytes.clear();
    }

    SFileImpl* file = nullptr;

    if (!SFile::OpenEx(nullptr, path.c_str(), 0x20000, &file) || !file) {
        return false;
    }

    uint32_t size = SFileGetFileSize(file, nullptr);
    bytes.resize(size);
    bool read = size && SFileReadFile(file, bytes.data(), size, nullptr, nullptr, 0);
    SFileCloseFile(file);

    return read;
}

bool Db2Table::Load(const char* fileName) {
    if (this->m_loaded) {
        return true;
    }

    std::vector<uint8_t> bytes;

    if (!ReadWholeFile(fileName, bytes)) {
        LOG_INFO << "DB2: " << fileName << " not found";
        return false;
    }

    return this->LoadBytes(bytes.data(), static_cast<uint32_t>(bytes.size()), fileName);
}

bool Db2Table::LoadBytes(const uint8_t* data, uint32_t size, const char* name) {
    if (this->m_loaded) {
        return true;
    }

    Decoded decoded;

    if (!data || !Decode(data, size, decoded, this->m_stringColumns, this->m_stringColumnCount)) {
        LOG_ERROR << "DB2: " << name << " is not a supported DB2 (WDC1/2/3) or is malformed";
        return false;
    }

    if (decoded.m_rowSize != this->m_rowSize) {
        LOG_ERROR << "DB2: " << name << " decoded record size " << decoded.m_rowSize << " does not match " << this->m_rowSize;
        return false;
    }

    this->m_numRows = static_cast<uint32_t>(decoded.m_ids.size());
    this->m_records = std::move(decoded.m_records);
    this->m_ids = std::move(decoded.m_ids);
    this->m_strings = std::move(decoded.m_strings);
    this->BuildIndex();
    this->m_loaded = true;

    LOG_INFO << "DB2: loaded " << name << " (" << this->m_numRows << " rows)";

    return true;
}

void Db2Table::BuildIndex() {
    this->m_idIndex.clear();
    this->m_idsAscending = true;

    for (size_t i = 1; i < this->m_ids.size(); i++) {
        if (this->m_ids[i] <= this->m_ids[i - 1]) {
            this->m_idsAscending = false;
            break;
        }
    }

    if (this->m_ids.empty()) {
        this->m_minId = 0;
        this->m_maxId = -1;
        return;
    }

    if (this->m_idsAscending) {
        this->m_minId = this->m_ids.front();
        this->m_maxId = this->m_ids.back();
        return;
    }

    this->m_minId = 0x7FFFFFFF;
    this->m_maxId = -1;
    this->m_idIndex.reserve(this->m_ids.size());

    for (uint32_t i = 0; i < this->m_ids.size(); i++) {
        int32_t id = this->m_ids[i];
        this->m_idIndex[id] = i;
        this->m_minId = std::min(this->m_minId, id);
        this->m_maxId = std::max(this->m_maxId, id);
    }
}

const void* Db2Table::RowById(int32_t id) const {
    size_t row;

    if (this->m_idsAscending) {
        if (this->m_ids.empty() || id < this->m_minId || id > this->m_maxId) {
            return nullptr;
        }

        auto it = std::lower_bound(this->m_ids.begin(), this->m_ids.end(), id);

        if (it == this->m_ids.end() || *it != id) {
            return nullptr;
        }

        row = static_cast<size_t>(it - this->m_ids.begin());
    } else {
        auto it = this->m_idIndex.find(id);

        if (it == this->m_idIndex.end()) {
            return nullptr;
        }

        row = it->second;
    }

    return this->m_records.data() + row * this->m_rowSize;
}

const void* Db2Table::RowByIndex(uint32_t index) const {
    if (index >= this->m_numRows) {
        return nullptr;
    }

    return this->m_records.data() + static_cast<size_t>(index) * this->m_rowSize;
}

const char* Db2Table::Str(uint32_t offset) const {
    if (offset >= this->m_strings.size()) {
        return "";
    }

    return this->m_strings.data() + offset;
}

}
