#ifndef HOT_EDITOR_DBC_SCHEMA_H
#define HOT_EDITOR_DBC_SCHEMA_H

#include <cstdint>
#include <string>
#include <vector>

namespace HoT::Editor::Dbc {

enum class ColumnType : uint8_t {
    UInt,
    Int,
    Float,
    String,
    UInt8,
    Int8,
};

// Localized string groups keep only the client locale in the loaded record, the other columns are dropped.
enum class Slot : uint8_t {
    Value,
    LocaleString,
    Dropped,
};

struct Column {
    std::string name;
    ColumnType type = ColumnType::UInt;
    Slot slot = Slot::Value;
    uint32_t offset = 0;
    uint32_t size = 4;
    bool localeFlags = false;
};

struct Schema {
    std::string table;
    std::vector<Column> columns;
    uint32_t recordSize = 0;
    int32_t nameColumn = -1;
};

// Parsed once from Interface\GameEditor\Schema\<table>.txt, null when the binding is missing or empty.
const Schema* GetSchema(const char* table);

int32_t FindColumn(const Schema& schema, const char* name);

const char* TypeName(ColumnType type);

// The editor value type id used on the wire, 0 uint32, 1 int32, 2 float, 3 string.
uint8_t WireType(ColumnType type);

bool IsStringField(const Column& column);

}

#endif
