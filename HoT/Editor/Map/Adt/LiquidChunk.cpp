#include "HoT/Editor/Map/Adt/LiquidChunk.h"
#include <cstdio>
#include <cstring>

namespace HoT::MapEditor::Adt {

namespace {
constexpr size_t kHeaderTable = 256 * sizeof(SMLiquidChunkDesc);

const TopChunk* FindTop(const AdtDocument& doc, uint32_t id) {
    for (const TopChunk& top : doc.m_order) {
        if (top.m_id == id) {
            return &top;
        }
    }

    return nullptr;
}

void Append(std::vector<uint8_t>& blob, const void* from, size_t bytes) {
    const uint8_t* at = static_cast<const uint8_t*>(from);
    blob.insert(blob.end(), at, at + bytes);
}

void Pad(std::vector<uint8_t>& blob) {
    while (blob.size() % 4) {
        blob.push_back(0);
    }
}

size_t VertexBlockSize(uint16_t format, size_t n) {
    switch (format) {
        case kLiquidHeightDepth:
            return n * sizeof(float) + n;
        case kLiquidHeightUv:
            return n * sizeof(float) + n * 2 * sizeof(uint16_t);
        case kLiquidDepthOnly:
            return n;
        default:
            return 0;
    }
}
}

int32_t LiquidLayer::CellCount() const {
    int32_t count = 0;
    for (bool on : this->m_exists) {
        if (on) {
            ++count;
        }
    }

    return count;
}

bool ReadTileLiquid(const AdtDocument& doc, TileLiquid& out, std::string& error) {
    out = TileLiquid();

    const TopChunk* mh2o = FindTop(doc, kMH2O);
    if (!mh2o || mh2o->m_data.empty()) {
        return true;
    }

    const uint8_t* base = mh2o->m_data.data();
    size_t size = mh2o->m_data.size();

    if (size < kHeaderTable) {
        error = "MH2O is shorter than its 256 chunk headers";
        return false;
    }

    out.m_present = true;

    for (int32_t index = 0; index < 256; ++index) {
        SMLiquidChunkDesc head{};
        std::memcpy(&head, base + index * sizeof(SMLiquidChunkDesc), sizeof(SMLiquidChunkDesc));

        if (head.m_layerCount == 0) {
            continue;
        }

        ChunkLiquid& chunk = out.m_chunks[index];

        if (head.m_offsetAttributes == 0) {
            chunk.m_fishable = ~0ull;
            chunk.m_fatigue = ~0ull;
        } else if (static_cast<size_t>(head.m_offsetAttributes) + 16 <= size) {
            std::memcpy(&chunk.m_fishable, base + head.m_offsetAttributes, sizeof(uint64_t));
            std::memcpy(&chunk.m_fatigue, base + head.m_offsetAttributes + 8, sizeof(uint64_t));
        } else {
            error = "MH2O attributes run past the end of the chunk";
            return false;
        }

        for (uint32_t i = 0; i < head.m_layerCount; ++i) {
            size_t at = head.m_offsetInstances + i * sizeof(SMLiquidInstance);
            if (at + sizeof(SMLiquidInstance) > size) {
                error = "MH2O instance runs past the end of the chunk";
                return false;
            }

            SMLiquidInstance inst{};
            std::memcpy(&inst, base + at, sizeof(SMLiquidInstance));

            int32_t ox = inst.m_xOffset;
            int32_t oy = inst.m_yOffset;
            int32_t w = inst.m_width;
            int32_t h = inst.m_height;

            if (w < 1 || h < 1 || ox + w > kLiquidCellSide || oy + h > kLiquidCellSide) {
                char message[128];
                std::snprintf(message, sizeof(message),
                    "MH2O chunk %d layer %u covers %d,%d %dx%d, which is off the grid", index, i,
                    ox, oy, w, h);
                error = message;
                return false;
            }

            LiquidLayer layer;
            layer.m_type = inst.m_liquidType;
            layer.m_format = inst.m_vertexFormat;

            float seed = inst.m_vertexFormat == kLiquidDepthOnly ? 0.0f : inst.m_minHeight;
            for (int32_t v = 0; v < kLiquidVerts; ++v) {
                layer.m_height[v] = seed;
                layer.m_depth[v] = 255;
            }

            for (int32_t cy = 0; cy < h; ++cy) {
                for (int32_t cx = 0; cx < w; ++cx) {
                    bool on = true;
                    if (inst.m_offsetExistsBitmap) {
                        int32_t bit = cy * w + cx;
                        size_t byteAt = inst.m_offsetExistsBitmap + bit / 8;
                        if (byteAt >= size) {
                            error = "MH2O exists bitmap runs past the end of the chunk";
                            return false;
                        }

                        on = (base[byteAt] >> (bit & 7)) & 1;
                    }

                    if (on) {
                        layer.m_exists[LiquidCell(ox + cx, oy + cy)] = true;
                    }
                }
            }

            int32_t vw = w + 1;
            int32_t vh = h + 1;
            size_t n = static_cast<size_t>(vw) * vh;
            uint32_t vd = inst.m_offsetVertexData;

            if (vd && vd + VertexBlockSize(inst.m_vertexFormat, n) > size) {
                error = "MH2O vertex data runs past the end of the chunk";
                return false;
            }

            if (vd) {
                for (int32_t vy = 0; vy < vh; ++vy) {
                    for (int32_t vx = 0; vx < vw; ++vx) {
                        size_t src = static_cast<size_t>(vy) * vw + vx;
                        int32_t dst = LiquidVert(ox + vx, oy + vy);

                        switch (inst.m_vertexFormat) {
                            case kLiquidHeightDepth:
                                std::memcpy(&layer.m_height[dst], base + vd + src * 4, sizeof(float));
                                layer.m_depth[dst] = base[vd + n * 4 + src];
                                break;

                            case kLiquidHeightUv:
                                std::memcpy(&layer.m_height[dst], base + vd + src * 4, sizeof(float));
                                std::memcpy(&layer.m_uv[dst * 2], base + vd + (n + src) * 4,
                                    2 * sizeof(uint16_t));
                                break;

                            case kLiquidDepthOnly:
                                layer.m_depth[dst] = base[vd + src];
                                break;

                            default:
                                break;
                        }
                    }
                }
            }

            chunk.m_layers.push_back(layer);
        }
    }

    return true;
}

bool WriteTileLiquid(const TileLiquid& in, AdtDocument& doc, std::string& error) {
    std::vector<uint8_t> blob(kHeaderTable, 0);
    bool any = false;

    for (int32_t index = 0; index < 256; ++index) {
        const ChunkLiquid& chunk = in.m_chunks[index];

        std::vector<const LiquidLayer*> live;
        for (const LiquidLayer& layer : chunk.m_layers) {
            if (layer.Any()) {
                live.push_back(&layer);
            }
        }

        if (live.empty()) {
            continue;
        }

        any = true;

        SMLiquidChunkDesc head{};
        head.m_layerCount = static_cast<uint32_t>(live.size());

        Pad(blob);
        head.m_offsetAttributes = static_cast<uint32_t>(blob.size());
        Append(blob, &chunk.m_fishable, sizeof(uint64_t));
        Append(blob, &chunk.m_fatigue, sizeof(uint64_t));

        head.m_offsetInstances = static_cast<uint32_t>(blob.size());
        size_t instanceAt = blob.size();
        blob.resize(blob.size() + live.size() * sizeof(SMLiquidInstance), 0);

        for (size_t i = 0; i < live.size(); ++i) {
            const LiquidLayer& layer = *live[i];

            int32_t minX = kLiquidCellSide;
            int32_t minY = kLiquidCellSide;
            int32_t maxX = -1;
            int32_t maxY = -1;

            for (int32_t cy = 0; cy < kLiquidCellSide; ++cy) {
                for (int32_t cx = 0; cx < kLiquidCellSide; ++cx) {
                    if (!layer.m_exists[LiquidCell(cx, cy)]) {
                        continue;
                    }

                    minX = cx < minX ? cx : minX;
                    minY = cy < minY ? cy : minY;
                    maxX = cx > maxX ? cx : maxX;
                    maxY = cy > maxY ? cy : maxY;
                }
            }

            int32_t w = maxX - minX + 1;
            int32_t h = maxY - minY + 1;
            int32_t vw = w + 1;
            int32_t vh = h + 1;

            SMLiquidInstance inst{};
            inst.m_liquidType = layer.m_type;
            inst.m_vertexFormat = layer.m_format <= kLiquidDepthOnly ? layer.m_format
                                                                 : kLiquidHeightDepth;
            inst.m_xOffset = static_cast<uint8_t>(minX);
            inst.m_yOffset = static_cast<uint8_t>(minY);
            inst.m_width = static_cast<uint8_t>(w);
            inst.m_height = static_cast<uint8_t>(h);

            if (inst.m_vertexFormat == kLiquidDepthOnly) {
                for (int32_t vy = 0; vy < vh && inst.m_vertexFormat == kLiquidDepthOnly; ++vy) {
                    for (int32_t vx = 0; vx < vw; ++vx) {
                        if (layer.m_height[LiquidVert(minX + vx, minY + vy)] != 0.0f) {
                            inst.m_vertexFormat = kLiquidHeightDepth;
                            break;
                        }
                    }
                }
            }

            float lo = 0.0f;
            float hi = 0.0f;
            if (inst.m_vertexFormat != kLiquidDepthOnly) {
                lo = layer.m_height[LiquidVert(minX, minY)];
                hi = lo;

                for (int32_t vy = 0; vy < vh; ++vy) {
                    for (int32_t vx = 0; vx < vw; ++vx) {
                        float z = layer.m_height[LiquidVert(minX + vx, minY + vy)];
                        lo = z < lo ? z : lo;
                        hi = z > hi ? z : hi;
                    }
                }
            }

            inst.m_minHeight = lo;
            inst.m_maxHeight = hi;

            Pad(blob);
            inst.m_offsetExistsBitmap = static_cast<uint32_t>(blob.size());
            blob.resize(blob.size() + 8, 0);

            for (int32_t cy = 0; cy < h; ++cy) {
                for (int32_t cx = 0; cx < w; ++cx) {
                    if (!layer.m_exists[LiquidCell(minX + cx, minY + cy)]) {
                        continue;
                    }

                    int32_t bit = cy * w + cx;
                    blob[inst.m_offsetExistsBitmap + bit / 8] |= static_cast<uint8_t>(1 << (bit & 7));
                }
            }

            Pad(blob);
            inst.m_offsetVertexData = static_cast<uint32_t>(blob.size());

            if (inst.m_vertexFormat != kLiquidDepthOnly) {
                for (int32_t vy = 0; vy < vh; ++vy) {
                    for (int32_t vx = 0; vx < vw; ++vx) {
                        float z = layer.m_height[LiquidVert(minX + vx, minY + vy)];
                        Append(blob, &z, sizeof(float));
                    }
                }
            }

            if (inst.m_vertexFormat == kLiquidHeightUv) {
                for (int32_t vy = 0; vy < vh; ++vy) {
                    for (int32_t vx = 0; vx < vw; ++vx) {
                        int32_t v = LiquidVert(minX + vx, minY + vy);
                        Append(blob, &layer.m_uv[v * 2], 2 * sizeof(uint16_t));
                    }
                }
            } else {
                for (int32_t vy = 0; vy < vh; ++vy) {
                    for (int32_t vx = 0; vx < vw; ++vx) {
                        blob.push_back(layer.m_depth[LiquidVert(minX + vx, minY + vy)]);
                    }
                }
            }

            std::memcpy(blob.data() + instanceAt + i * sizeof(SMLiquidInstance), &inst,
                sizeof(SMLiquidInstance));
        }

        std::memcpy(blob.data() + index * sizeof(SMLiquidChunkDesc), &head, sizeof(SMLiquidChunkDesc));

        int32_t chunkX = index % 16;
        int32_t chunkY = index / 16;
        if (Mcnk* mcnk = doc.ChunkAt(chunkX, chunkY)) {
            for (size_t i = 0; i < mcnk->m_subs.size(); ++i) {
                if (mcnk->m_subs[i].m_id != kMCLQ) {
                    continue;
                }

                mcnk->m_subs.erase(mcnk->m_subs.begin() + i);
                mcnk->m_header.m_ofsLiquid = 0;
                mcnk->m_header.m_sizeLiquid = 0;

                mcnk->m_header.m_flags &= ~0x3Cu;
                break;
            }
        }
    }

    for (size_t i = 0; i < doc.m_order.size(); ++i) {
        if (doc.m_order[i].m_id != kMH2O) {
            continue;
        }

        if (any) {
            doc.m_order[i].m_data = std::move(blob);
        } else {
            doc.m_order.erase(doc.m_order.begin() + i);
        }

        return true;
    }

    if (!any) {
        return true;
    }

    for (size_t i = 0; i < doc.m_order.size(); ++i) {
        if (doc.m_order[i].m_mcnkIndex < 0) {
            continue;
        }

        TopChunk top;
        top.m_id = kMH2O;
        top.m_data = std::move(blob);
        doc.m_order.insert(doc.m_order.begin() + i, std::move(top));
        return true;
    }

    error = "tile has no MCNK to put MH2O in front of";
    return false;
}

}
