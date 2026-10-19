#ifndef HOT_EDITOR_DBC_BINDING_H
#define HOT_EDITOR_DBC_BINDING_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include "HoT/Editor/Dbc/Schema.h"

namespace HoT::Editor::Dbc {

// Appends columns in file order, offsets point into the client record.
class Builder {
    public:
    explicit Builder(Schema& schema);

    void Value(const char* name, uint32_t offset, ColumnType type);
    // A localized string takes 16 locale columns and a flags column, the record keeps the client locale only.
    void Locale(const char* member, uint32_t offset);

    private:
    Schema& m_schema;
};

template <class M>
constexpr ColumnType TypeOf() {
    using T = std::remove_cv_t<std::remove_reference_t<M>>;

    if constexpr (std::is_same_v<T, float>) {
        return ColumnType::Float;
    } else if constexpr (std::is_same_v<T, const char*>) {
        return ColumnType::String;
    } else if constexpr (std::is_same_v<T, int8_t>) {
        return ColumnType::Int8;
    } else if constexpr (std::is_same_v<T, uint8_t>) {
        return ColumnType::UInt8;
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        return ColumnType::UInt;
    } else {
        static_assert(std::is_same_v<T, int32_t>, "unsupported record member type");
        return ColumnType::Int;
    }
}

struct Binding {
    const char* table;
    uint32_t recordSize;
    void (*bind)(Builder& builder);
};

const Binding* FindBinding(const char* table);

}

#define DBC_VALUE(member) builder.Value(#member, static_cast<uint32_t>(offsetof(Rec, member)), TypeOf<decltype(std::declval<Rec&>().member)>())
#define DBC_LOCALE(member) builder.Locale(#member, static_cast<uint32_t>(offsetof(Rec, member)))

#endif
