#include "HoT/DB/ClientDB.h"
#include <vector>
#include "HoT/Core/Log.h"

namespace HoT::DB {

static std::vector<void*> s_retired;
static uint64_t s_retiredBytes = 0;

template <class V>
static bool ReadValue(SFileImpl* f, V& value) {
    return SFileReadFile(f, &value, sizeof(value), nullptr, nullptr, 0) != 0;
}

SFileImpl* OpenTable(const char* filename, int32_t columns, int32_t rowSize, int32_t& numRecords, uint32_t& stringSize) {
    SFileImpl* f = nullptr;

    if (!SFile::OpenEx(nullptr, filename, 0x20000, &f) || !f) {
        LOG_DEBUG << filename << " not found";
        return nullptr;
    }

    uint32_t signature = 0;
    int32_t fileColumns = 0;
    int32_t fileRowSize = 0;
    numRecords = 0;
    stringSize = 0;

    if (!ReadValue(f, signature) || signature != 'CBDW') {
        LOG_ERROR << filename << " has an invalid signature";
    } else if (!ReadValue(f, numRecords) || !ReadValue(f, fileColumns) || !ReadValue(f, fileRowSize) || !ReadValue(f, stringSize)) {
        LOG_ERROR << filename << " has a truncated header";
    } else if (numRecords && fileColumns != columns) {
        LOG_ERROR << filename << " has " << fileColumns << " columns, expected " << columns;
    } else if (numRecords && fileRowSize != rowSize) {
        LOG_ERROR << filename << " has row size " << fileRowSize << ", expected " << rowSize;
    } else {
        return f;
    }

    SFileCloseFile(f);
    return nullptr;
}

bool ReadStrings(SFileImpl* f, const char* filename, char* strings, uint32_t stringSize) {
    if (!stringSize || SFileReadFile(f, strings, stringSize, nullptr, nullptr, 0)) {
        return true;
    }

    LOG_ERROR << filename << ": cannot read string table";
    return false;
}

void Retire(void* block, uint32_t bytes) {
    if (!block) {
        return;
    }

    s_retired.push_back(block);
    s_retiredBytes += bytes;
}

uint64_t RetiredBytes() {
    return s_retiredBytes;
}

}
