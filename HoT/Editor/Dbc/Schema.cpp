#include "HoT/Editor/Dbc/Schema.h"
#include <cctype>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <utility>
#include "HoT/Core/Log.h"
#include "HoT/Editor/Dbc/Binding.h"
#include "WoW/Source/Client.h"

namespace HoT::Editor::Dbc {

static constexpr uint32_t kLocaleCount = 16;

static std::unordered_map<std::string, std::unique_ptr<Schema>> s_schemas;

static std::string Lower(const std::string& text) {
    std::string out = text;

    for (char& c : out) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    return out;
}

static uint32_t SizeOf(ColumnType type) {
    switch (type) {
        case ColumnType::String:
            return sizeof(const char*);
        case ColumnType::UInt8:
        case ColumnType::Int8:
            return 1;
        default:
            return 4;
    }
}

Builder::Builder(Schema& schema) : m_schema(schema) {
}

void Builder::Value(const char* name, uint32_t offset, ColumnType type) {
    Column column;
    column.name = name;
    column.type = type;
    column.slot = Slot::Value;
    column.offset = offset;
    column.size = SizeOf(type);
    m_schema.columns.push_back(std::move(column));
}

void Builder::Locale(const char* member, uint32_t offset) {
    uint32_t locale = s_locale >= 0 && static_cast<uint32_t>(s_locale) < kLocaleCount ? static_cast<uint32_t>(s_locale) : 0;

    for (uint32_t k = 0; k < kLocaleCount; k++) {
        Column column;
        column.name = std::string(member) + "[" + std::to_string(k) + "]";
        column.type = ColumnType::String;
        column.slot = k == locale ? Slot::LocaleString : Slot::Dropped;
        column.offset = k == locale ? offset : 0;
        column.size = k == locale ? SizeOf(ColumnType::String) : 0;
        m_schema.columns.push_back(std::move(column));
    }

    Column flags;
    flags.name = std::string(member) + "Flags";
    flags.type = ColumnType::UInt;
    flags.slot = Slot::Dropped;
    flags.size = 0;
    flags.localeFlags = true;
    m_schema.columns.push_back(std::move(flags));
}

const Schema* GetSchema(const char* table) {
    if (!table || !*table) {
        return nullptr;
    }

    std::string key = Lower(table);
    auto it = s_schemas.find(key);

    if (it != s_schemas.end()) {
        return it->second.get();
    }

    std::unique_ptr<Schema> schema;

    if (const Binding* binding = FindBinding(table)) {
        schema = std::make_unique<Schema>();
        schema->table = binding->table;
        schema->recordSize = binding->recordSize;
        Builder builder(*schema);
        binding->bind(builder);

        // Tables whose first string column is a file or folder name rather than what players see.
        static const std::pair<const char*, const char*> kNameColumns[] = {
            { "map", "m_mapName" },
            { "itemdisplayinfo", "m_inventoryIcon" },
        };
        const char* preferred = nullptr;

        for (const auto& [name, member] : kNameColumns) {
            if (key == name) {
                preferred = member;
            }
        }

        for (size_t i = 0; i < schema->columns.size() && preferred; i++) {
            if (IsStringField(schema->columns[i]) && schema->columns[i].name.rfind(preferred, 0) == 0) {
                schema->nameColumn = static_cast<int32_t>(i);
                break;
            }
        }

        for (size_t i = 0; i < schema->columns.size() && schema->nameColumn < 0; i++) {
            if (IsStringField(schema->columns[i])) {
                schema->nameColumn = static_cast<int32_t>(i);
                break;
            }
        }
    } else {
        LOG_DEBUG << "no game editor binding for " << table;
    }

    return s_schemas.emplace(key, std::move(schema)).first->second.get();
}

int32_t FindColumn(const Schema& schema, const char* name) {
    if (!name) {
        return -1;
    }

    for (size_t i = 0; i < schema.columns.size(); i++) {
        if (_stricmp(schema.columns[i].name.c_str(), name) == 0) {
            return static_cast<int32_t>(i);
        }
    }

    return -1;
}

const char* TypeName(ColumnType type) {
    switch (type) {
        case ColumnType::Int:
        case ColumnType::Int8:
            return "int";
        case ColumnType::Float:
            return "float";
        case ColumnType::String:
            return "string";
        default:
            return "uint";
    }
}

uint8_t WireType(ColumnType type) {
    switch (type) {
        case ColumnType::Int:
        case ColumnType::Int8:
            return 1;
        case ColumnType::Float:
            return 2;
        case ColumnType::String:
            return 3;
        default:
            return 0;
    }
}

bool IsStringField(const Column& column) {
    return column.slot == Slot::LocaleString || (column.slot == Slot::Value && column.type == ColumnType::String);
}

}
