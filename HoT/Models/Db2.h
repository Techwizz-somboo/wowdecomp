#ifndef HOT_MODELS_DB2_H
#define HOT_MODELS_DB2_H

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace HoT::Models {

// A WDC1, WDC2 or WDC3 table decoded into rows of [id][fields in file order at native width][relationship id].
class Db2Table {
    public:
    explicit Db2Table(uint32_t rowSize) : m_rowSize(rowSize) {}

    // Reads DBFilesClient\fileName as a loose file first, then from the mounted archives.
    bool Load(const char* fileName);
    bool LoadBytes(const uint8_t* data, uint32_t size, const char* name);

    // Field indices holding string offsets, only WDC2 and later or multi-section tables need them.
    void SetStringColumns(const uint32_t* columns, uint32_t count) {
        this->m_stringColumns = columns;
        this->m_stringColumnCount = count;
    }

    uint32_t RowCount() const {
        return this->m_numRows;
    }

    const void* RowById(int32_t id) const;
    const void* RowByIndex(uint32_t index) const;
    const char* Str(uint32_t offset) const;

    private:
    void BuildIndex();

    uint32_t m_rowSize;
    const uint32_t* m_stringColumns = nullptr;
    uint32_t m_stringColumnCount = 0;
    bool m_loaded = false;
    uint32_t m_numRows = 0;
    int32_t m_minId = 0x7FFFFFFF;
    int32_t m_maxId = -1;
    bool m_idsAscending = false;
    std::vector<char> m_records;
    std::vector<int32_t> m_ids;
    std::vector<char> m_strings;
    std::unordered_map<int32_t, uint32_t> m_idIndex;
};

}

#endif
