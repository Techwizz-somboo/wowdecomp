#include "HoT/MSDF/Manager.h"

namespace HoT::MSDF {

static SYSTEM_INFO& SystemInfo() {
    static SYSTEM_INFO s_info = []() {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return info;
    }();

    return s_info;
}

void Manager::MappedBlock::Close() {
    this->m_view.Close();
    this->m_mapping.Close();
    this->m_file.Close();
    this->m_header = nullptr;
    this->m_entries = nullptr;
    this->m_hashTable = nullptr;
    this->m_payload = nullptr;
    this->m_entryCount = 0;
}

void Manager::MappedBlock::Reset() {
    this->Close();
    this->m_slotIndex = 0xFFFFFFFF;
    this->m_key = BlockKey();
}

Manager::ArenaState::ArenaState() {
    this->m_slotAddresses.fill(nullptr);
    this->m_slotToBlockIndex.fill(0xFFFFFFFF);

    if (!PlaceholderVA::Available() || IsWine()) {
        return;
    }

    const uint32_t maxGlyphDim = SDF_RENDER_SIZE + 2 * SDF_SPREAD;
    const uint32_t maxBytesPerGlyph = maxGlyphDim * maxGlyphDim * 4;
    const size_t maxBlockSize = sizeof(BlockFileHeader) + BLOCK_SIZE * sizeof(GlyphEntry) + BLOCK_SIZE * sizeof(uint32_t) + BLOCK_SIZE * maxBytesPerGlyph;

    const size_t granularity = SystemInfo().dwAllocationGranularity;
    this->m_slotSize = ((maxBlockSize + granularity - 1) / granularity) * granularity;

    this->m_base = PlaceholderVA::VirtualAlloc2P()(GetCurrentProcess(), nullptr, this->m_slotSize * MAX_ARENA_SLOTS, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);

    if (!this->m_base) {
        return;
    }

    for (size_t i = 0; i < MAX_ARENA_SLOTS; i++) {
        void* slotAddr = static_cast<char*>(this->m_base) + i * this->m_slotSize;
        this->m_slotAddresses[i] = slotAddr;
        VirtualFreeEx(GetCurrentProcess(), slotAddr, this->m_slotSize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
    }
}

Manager::ArenaState::~ArenaState() {
    if (this->m_base) {
        VirtualFreeEx(GetCurrentProcess(), this->m_base, 0, MEM_RELEASE);
        this->m_base = nullptr;
    }
}

void* Manager::ArenaState::GetFreeSlot(uint32_t blockIndex, uint32_t& outSlotIndex) {
    if (!this->m_base || this->m_freeMask == 0) {
        return nullptr;
    }

    uint32_t slotIndex = 0;

    while (!(this->m_freeMask & (1ULL << slotIndex))) {
        slotIndex++;
    }

    this->m_freeMask &= ~(1ULL << slotIndex);
    this->m_slotToBlockIndex[slotIndex] = blockIndex;
    outSlotIndex = slotIndex;

    return this->m_slotAddresses[slotIndex];
}

void Manager::ArenaState::FreeSlot(uint32_t slotIndex) {
    if (!this->m_base || slotIndex >= MAX_ARENA_SLOTS || !this->IsSlotOccupied(slotIndex)) {
        return;
    }

    void* slotAddr = this->m_slotAddresses[slotIndex];
    uintptr_t current = reinterpret_cast<uintptr_t>(slotAddr);
    uintptr_t end = current + this->m_slotSize;

    while (current < end) {
        MEMORY_BASIC_INFORMATION mbi;

        if (VirtualQuery(reinterpret_cast<void*>(current), &mbi, sizeof(mbi)) == 0) {
            break;
        }

        if (reinterpret_cast<uintptr_t>(mbi.BaseAddress) >= end) {
            break;
        }

        if (mbi.State != MEM_FREE) {
            if (mbi.Type == MEM_MAPPED) {
                PlaceholderVA::UnmapViewOfFile2P()(GetCurrentProcess(), mbi.BaseAddress, 0);
            } else {
                VirtualFree(mbi.BaseAddress, 0, MEM_RELEASE);
            }
        }

        current += mbi.RegionSize;
    }

    VirtualFreeEx(GetCurrentProcess(), slotAddr, 0, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
    PlaceholderVA::VirtualAlloc2P()(GetCurrentProcess(), slotAddr, this->m_slotSize, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);

    this->m_freeMask |= 1ULL << slotIndex;
    this->m_slotToBlockIndex[slotIndex] = 0xFFFFFFFF;
}

void Manager::ArenaState::FlushAll() {
    for (uint32_t i = 0; i < MAX_ARENA_SLOTS; i++) {
        if (this->IsSlotOccupied(i)) {
            this->FreeSlot(i);
        }
    }
}

Manager::ArenaState& Manager::Arena() {
    static ArenaState s_arena;
    return s_arena;
}

bool Manager::CacheAvailable() {
    return Arena().m_base != nullptr;
}

size_t Manager::AllocationGranularity() {
    return SystemInfo().dwAllocationGranularity;
}

void Manager::FreeBlock(uint32_t blockIndex) {
    if (blockIndex >= MAX_ARENA_SLOTS) {
        return;
    }

    MappedBlock& block = s_mappedBlocks[blockIndex];
    BlockKey key = block.m_key;

    if (block.m_slotIndex != 0xFFFFFFFF) {
        Arena().FreeSlot(block.m_slotIndex);
    }

    block.Reset();
    s_blockCache.erase(key);

    if (s_lastBlockIndex == blockIndex) {
        s_lastBlockIndex = 0xFFFFFFFF;
        s_lastBlockKey = BlockKey();
    }
}

void Manager::FreeBlockByKey(BlockKey key) {
    auto it = s_blockCache.find(key);

    if (it != s_blockCache.end()) {
        FreeBlock(it->second);
    }
}

void Manager::FlushAll() {
    for (MappedBlock& block : s_mappedBlocks) {
        block.Reset();
    }

    s_blockCache.clear();
    Arena().FlushAll();

    s_lastBlockIndex = 0xFFFFFFFF;
    s_lastBlockKey = BlockKey();
}

uint32_t Manager::RegisterFont(FontHash hash) {
    auto it = s_fontHashToId.find(hash);

    if (it != s_fontHashToId.end()) {
        return it->second;
    }

    uint32_t fontId = s_nextFontId++;
    s_fontHashToId.emplace(hash, fontId);

    return fontId;
}

bool Manager::LoadMappedBlock(const BlockWrap& wrap, MappedBlock& outBlock, void* slotAddr, uint32_t slotIndex) {
    ArenaState& arena = Arena();

    outBlock.m_file.m_handle = CreateFileW(wrap.m_path.native().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);

    if (!outBlock.m_file.IsValid()) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    LARGE_INTEGER fileSize;

    if (!GetFileSizeEx(outBlock.m_file.m_handle, &fileSize)) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_fileSize = static_cast<uint64_t>(fileSize.QuadPart);

    const size_t granularity = AllocationGranularity();
    uint64_t splitSize = ((outBlock.m_fileSize + granularity - 1) / granularity) * granularity;

    if (splitSize < granularity) {
        splitSize = granularity;
    }

    if (splitSize > arena.m_slotSize) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_slotIndex = slotIndex;

    if (!VirtualFreeEx(GetCurrentProcess(), slotAddr, static_cast<SIZE_T>(splitSize), MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_mapping.m_handle = CreateFileMappingW(outBlock.m_file.m_handle, nullptr, PAGE_READONLY, static_cast<DWORD>(splitSize >> 32), static_cast<DWORD>(splitSize & 0xFFFFFFFF), nullptr);

    if (!outBlock.m_mapping.IsValid()) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_view.m_ptr = PlaceholderVA::MapViewOfFile3P()(outBlock.m_mapping.m_handle, nullptr, slotAddr, 0, static_cast<SIZE_T>(splitSize), MEM_REPLACE_PLACEHOLDER, PAGE_READONLY, nullptr, 0);

    if (!outBlock.m_view.m_ptr) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_view.m_placeholder = true;

    const uint8_t* base = static_cast<const uint8_t*>(outBlock.m_view.m_ptr);
    outBlock.m_header = reinterpret_cast<const BlockFileHeader*>(base);

    if (outBlock.m_header->m_magic != BLOCK_MAGIC || outBlock.m_header->m_version != CACHE_VERSION || outBlock.m_header->m_blockId != wrap.m_key.m_blockId || outBlock.m_header->m_entryCount > BLOCK_SIZE) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_entryCount = outBlock.m_header->m_entryCount;
    outBlock.m_entries = reinterpret_cast<const GlyphEntry*>(base + sizeof(BlockFileHeader));

    size_t hashTableOffset = sizeof(BlockFileHeader) + static_cast<size_t>(outBlock.m_entryCount) * sizeof(GlyphEntry);
    outBlock.m_hashTable = reinterpret_cast<const uint32_t*>(base + hashTableOffset);

    size_t payloadOffset = hashTableOffset + BLOCK_SIZE * sizeof(uint32_t);

    if (outBlock.m_fileSize < payloadOffset) {
        arena.FreeSlot(slotIndex);
        return false;
    }

    outBlock.m_payload = base + payloadOffset;

    size_t maxPayload = static_cast<size_t>(outBlock.m_fileSize - payloadOffset);

    for (uint32_t i = 0; i < outBlock.m_entryCount; i++) {
        const GlyphEntry& entry = outBlock.m_entries[i];

        if (entry.m_dataSize > 0 && static_cast<size_t>(entry.m_dataOffset) + entry.m_dataSize > maxPayload) {
            arena.FreeSlot(slotIndex);
            return false;
        }
    }

    outBlock.m_key = wrap.m_key;

    return true;
}

Manager::MappedBlock* Manager::GetOrLoadMappedBlock(const BlockWrap& wrap) {
    if (s_lastBlockIndex != 0xFFFFFFFF && s_lastBlockKey == wrap.m_key) {
        return &s_mappedBlocks[s_lastBlockIndex];
    }

    auto it = s_blockCache.find(wrap.m_key);

    if (it != s_blockCache.end()) {
        s_lastBlockIndex = it->second;
        s_lastBlockKey = wrap.m_key;
        return &s_mappedBlocks[it->second];
    }

    ArenaState& arena = Arena();

    if (arena.m_freeMask == 0) {
        FlushAll();
    }

    uint32_t slotIndex = 0;
    void* slotAddr = arena.GetFreeSlot(wrap.m_key.m_blockId, slotIndex);

    if (!slotAddr) {
        return nullptr;
    }

    MappedBlock& block = s_mappedBlocks[slotIndex];

    if (!LoadMappedBlock(wrap, block, slotAddr, slotIndex)) {
        // The slot is already freed, close the handles a failed map left open.
        block.m_view.m_ptr = nullptr;
        block.Reset();
        return nullptr;
    }

    s_lastBlockIndex = slotIndex;
    s_lastBlockKey = wrap.m_key;
    s_blockCache[wrap.m_key] = slotIndex;

    return &block;
}

bool Manager::LoadGlyph(const BlockWrap& wrap, uint32_t codepoint, GlyphMetrics& outMetrics) {
    MappedBlock* block = GetOrLoadMappedBlock(wrap);

    if (!block) {
        return false;
    }

    uint32_t entryIndex = block->m_hashTable[codepoint & (BLOCK_SIZE - 1)];

    if (entryIndex == 0xFFFFFFFF || entryIndex >= block->m_entryCount) {
        return false;
    }

    const GlyphEntry& entry = block->m_entries[entryIndex];

    if (entry.m_codepoint != codepoint) {
        return false;
    }

    outMetrics.m_width = entry.m_width;
    outMetrics.m_height = entry.m_height;
    outMetrics.m_bitmapTop = entry.m_bitmapTop;
    outMetrics.m_bitmapLeft = entry.m_bitmapLeft;
    outMetrics.m_pixels = entry.m_dataSize > 0 ? block->m_payload + entry.m_dataOffset : nullptr;

    return true;
}

}
