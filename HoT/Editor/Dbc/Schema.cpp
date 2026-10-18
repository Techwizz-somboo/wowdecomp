#include "HoT/Editor/Dbc/Schema.h"
#include <cctype>
#include <cstring>
#include <memory>
#include <sstream>
#include <unordered_map>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Core/Log.h"
#include "WoW/Source/Client.h"

namespace HoT::Editor::Dbc {

static constexpr uint32_t kLocaleCount = 16;
static constexpr uint32_t kLocaleGroupSize = kLocaleCount + 1;

static std::unordered_map<std::string, std::unique_ptr<Schema>> s_schemas;

static std::string Lower(const std::string& text) {
    std::string out = text;

    for (char& c : out) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    return out;
}

static bool ReadText(const std::string& path, std::string& text) {
    SFileImpl* file = nullptr;

    if (!SFile::OpenEx(nullptr, path.c_str(), 0, &file) || !file) {
        return false;
    }

    uint32_t size = SFileGetFileSize(file, nullptr);
    text.resize(size);
    bool ok = !size || SFileReadFile(file, text.data(), size, nullptr, nullptr, 0);
    SFileCloseFile(file);

    return ok;
}

static bool ParseType(const std::string& token, ColumnType& type) {
    std::string t = Lower(token);

    if (t == "uint" || t == "uint32" || t == "unsigned") {
        type = ColumnType::UInt;
    } else if (t == "int" || t == "int32") {
        type = ColumnType::Int;
    } else if (t == "float") {
        type = ColumnType::Float;
    } else if (t == "uint8" || t == "byte") {
        type = ColumnType::UInt8;
    } else if (t == "int8") {
        type = ColumnType::Int8;
    } else if (t == "string") {
        type = ColumnType::String;
    } else {
        return false;
    }

    return true;
}

// Strips trailing digits, an underscore and a Flag or Flags suffix so Name3 and NameFlags share a stem.
static std::string GroupStem(const std::string& name) {
    std::string s = name;

    while (!s.empty() && isdigit(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }

    if (!s.empty() && s.back() == '_') {
        s.pop_back();
    }

    for (const char* suffix : { "Flags", "Flag" }) {
        size_t length = strlen(suffix);

        if (s.size() > length && s.compare(s.size() - length, length, suffix) == 0) {
            s.erase(s.size() - length);
            break;
        }
    }

    return s;
}

static bool IsLocaleGroup(const std::vector<Column>& columns, size_t start) {
    if (start + kLocaleGroupSize > columns.size() || columns[start].type != ColumnType::String) {
        return false;
    }

    bool allStrings = columns[start + kLocaleCount].type != ColumnType::String;

    for (size_t i = 0; i < kLocaleCount && allStrings; i++) {
        allStrings = columns[start + i].type == ColumnType::String;
    }

    if (allStrings) {
        return true;
    }

    std::string stem = GroupStem(columns[start].name);

    for (size_t i = 1; i < kLocaleGroupSize; i++) {
        if (GroupStem(columns[start + i].name) != stem) {
            return false;
        }
    }

    return true;
}

static uint32_t Align(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static void Layout(Schema& schema) {
    uint32_t locale = s_locale >= 0 && static_cast<uint32_t>(s_locale) < kLocaleCount ? static_cast<uint32_t>(s_locale) : 0;
    uint32_t offset = 0;
    uint32_t maxAlign = 4;
    std::vector<Column>& columns = schema.columns;

    for (size_t i = 0; i < columns.size();) {
        if (IsLocaleGroup(columns, i)) {
            offset = Align(offset, 8);
            maxAlign = 8;

            for (uint32_t k = 0; k < kLocaleGroupSize; k++) {
                Column& column = columns[i + k];
                column.slot = k == locale ? Slot::LocaleString : Slot::Dropped;
                column.offset = offset;
                column.size = k == locale ? 8 : 0;
                column.localeFlags = k == kLocaleCount;
            }

            offset += 8;
            i += kLocaleGroupSize;
            continue;
        }

        Column& column = columns[i];

        switch (column.type) {
            case ColumnType::String:
                column.size = 8;
                break;
            case ColumnType::UInt8:
            case ColumnType::Int8:
                column.size = 1;
                break;
            default:
                column.size = 4;
                break;
        }

        offset = Align(offset, column.size);
        maxAlign = column.size > maxAlign ? column.size : maxAlign;
        column.offset = offset;
        column.slot = Slot::Value;
        offset += column.size;
        i++;
    }

    schema.recordSize = Align(offset, maxAlign);
}

static std::unique_ptr<Schema> Parse(const std::string& table, const std::string& text) {
    auto schema = std::make_unique<Schema>();
    schema->table = table;

    std::istringstream lines(text);
    std::string line;

    while (std::getline(lines, line)) {
        std::istringstream tokens(line);
        std::string typeToken;
        std::string name;
        std::string flag;

        if (!(tokens >> typeToken) || typeToken[0] == '#' || !(tokens >> name)) {
            continue;
        }

        Column column;

        if (!ParseType(typeToken, column.type)) {
            // Directive lines such as IGNORE_ORDER carry no column.
            continue;
        }

        if ((tokens >> flag) && Lower(flag) == "string") {
            column.type = ColumnType::String;
        }

        column.name = name;
        schema->columns.push_back(std::move(column));
    }

    if (schema->columns.empty()) {
        return nullptr;
    }

    Layout(*schema);

    for (size_t i = 0; i < schema->columns.size(); i++) {
        if (IsStringField(schema->columns[i])) {
            schema->nameColumn = static_cast<int32_t>(i);
            break;
        }
    }

    return schema;
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

    std::string text;
    std::unique_ptr<Schema> schema;

    if (ReadText(std::string("Interface\\GameEditor\\Schema\\") + table + ".txt", text)) {
        schema = Parse(table, text);
    }

    if (!schema) {
        LOG_DEBUG << "no game editor schema for " << table;
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
