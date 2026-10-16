#ifndef HOT_RUNTIME_LUAPOOL_H
#define HOT_RUNTIME_LUAPOOL_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace HoT::LuaRuntime {

// Size class allocator for the Lua state, small blocks come from 256 KB slabs and the rest from a private heap.
class LuaPool {
    public:
    LuaPool();
    ~LuaPool();

    LuaPool(const LuaPool&) = delete;
    LuaPool& operator=(const LuaPool&) = delete;

    void* Reallocate(void* ptr, size_t osize, size_t nsize);

    uint64_t BytesAllocated() const { return this->m_bytesAllocated; }
    uint64_t AllocCalls() const { return this->m_allocCalls; }
    uint64_t FreeCalls() const { return this->m_freeCalls; }
    size_t LiveBytes() const { return this->m_live; }
    size_t SlabBytes() const { return this->m_slabs.size() * SLAB_SIZE; }

    private:
    static const size_t SMALL_LIMIT = 2048;
    static const size_t SLAB_SIZE = 256 * 1024;
    static const uint32_t CLASS_COUNT = 40;

    struct FreeBlock {
        FreeBlock* m_next;
    };

    struct SizeClass {
        FreeBlock* m_free;
        uint32_t m_size;
    };

    uint32_t ClassOf(size_t size) const { return this->m_classOf[(size + 15) >> 4]; }

    void* Allocate(size_t size);
    void Free(void* ptr, size_t size);
    void* AllocateSmall(uint32_t cls);
    bool Refill(uint32_t cls);

    SizeClass m_classes[CLASS_COUNT] = {};
    uint8_t m_classOf[(SMALL_LIMIT >> 4) + 1] = {};
    std::vector<void*> m_slabs;
    void* m_heap = nullptr;
    uint64_t m_bytesAllocated = 0;
    uint64_t m_allocCalls = 0;
    uint64_t m_freeCalls = 0;
    size_t m_live = 0;
};

}

#endif
