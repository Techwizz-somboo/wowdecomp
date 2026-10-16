#include "HoT/Editor/GizmoDraw.h"
#include <cmath>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"

namespace HoT::Editor::GizmoDraw {

static constexpr float kPi = 3.14159265358979323846f;

static CImVector Red() {
    return CImVector(0x40, 0x40, 0xFF, 0xFF);
}

static CImVector Green() {
    return CImVector(0x40, 0xFF, 0x40, 0xFF);
}

static CImVector Blue() {
    return CImVector(0xFF, 0x40, 0x40, 0xFF);
}

static CImVector Yellow() {
    return CImVector(0x40, 0xFF, 0xFF, 0xFF);
}

static CImVector ColorFor(Axis axis, Axis selected, CImVector base) {
    return selected == axis ? Yellow() : base;
}

static void Submit(EGxPrim primType, uint32_t vertexCount, const C3Vector* verts, uint32_t indexCount, const uint16_t* indices, const CImVector& color) {
    GxPrimVertexPtr(vertexCount, verts, sizeof(C3Vector), nullptr, 0, &color, 0, nullptr, 0, nullptr, 0);
    GxPrimIndexPtr(indexCount, indices);

    CGxBatch batch{};
    batch.m_primType = primType;
    batch.m_minIndex = 0;
    batch.m_maxIndex = static_cast<uint16_t>(-1);
    batch.m_start = 0;
    batch.m_count = indexCount;
    GxDraw(&batch, 1);
}

void DrawArrowSolid(const C3Vector& origin, const C3Vector& dir, const C3Vector& side, float length, float headLength, float headRadius, float shaftRadius, CImVector color) {
    if (!g_theGxDevicePtr) {
        return;
    }

    C3Vector up = VectorMath::Cross(dir, side);
    float baseDist = length - headLength;
    C3Vector shaftBase = origin;
    C3Vector shaftTop = VectorMath::Add(origin, VectorMath::Scale(dir, baseDist));
    C3Vector tip = VectorMath::Add(origin, VectorMath::Scale(dir, length));

    constexpr int32_t kSegments = 12;
    constexpr int32_t kVertCount = 3 * kSegments + 3;
    constexpr int32_t kTriCount = kSegments * 5;
    constexpr int32_t kIndexCount = kTriCount * 3;

    C3Vector verts[kVertCount];

    for (int32_t i = 0; i < kSegments; ++i) {
        float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kSegments);
        float c = cosf(angle);
        float s = sinf(angle);
        C3Vector radial = VectorMath::Add(VectorMath::Scale(side, c), VectorMath::Scale(up, s));
        C3Vector shaftRadial = VectorMath::Scale(radial, shaftRadius);
        C3Vector headRadial = VectorMath::Scale(radial, headRadius);

        verts[i] = VectorMath::Add(shaftBase, shaftRadial);
        verts[kSegments + i] = VectorMath::Add(shaftTop, shaftRadial);
        verts[2 * kSegments + i] = VectorMath::Add(shaftTop, headRadial);
    }

    verts[3 * kSegments] = tip;
    verts[3 * kSegments + 1] = shaftBase;
    verts[3 * kSegments + 2] = shaftTop;

    uint16_t indices[kIndexCount];
    int32_t k = 0;

    for (int32_t i = 0; i < kSegments; ++i) {
        int32_t i0 = i;
        int32_t i1 = (i + 1) % kSegments;
        indices[k++] = static_cast<uint16_t>(i0);
        indices[k++] = static_cast<uint16_t>(kSegments + i0);
        indices[k++] = static_cast<uint16_t>(kSegments + i1);
        indices[k++] = static_cast<uint16_t>(i0);
        indices[k++] = static_cast<uint16_t>(kSegments + i1);
        indices[k++] = static_cast<uint16_t>(i1);

        indices[k++] = static_cast<uint16_t>(3 * kSegments + 1);
        indices[k++] = static_cast<uint16_t>(i1);
        indices[k++] = static_cast<uint16_t>(i0);

        int32_t head0 = 2 * kSegments + i;
        int32_t head1 = 2 * kSegments + ((i + 1) % kSegments);
        indices[k++] = static_cast<uint16_t>(3 * kSegments + 2);
        indices[k++] = static_cast<uint16_t>(head0);
        indices[k++] = static_cast<uint16_t>(head1);

        indices[k++] = static_cast<uint16_t>(3 * kSegments);
        indices[k++] = static_cast<uint16_t>(head1);
        indices[k++] = static_cast<uint16_t>(head0);
    }

    Submit(GxPrim_Triangles, kVertCount, verts, kIndexCount, indices, color);
}

void DrawTranslationGizmo(const C3Vector& origin, float scale, Axis selected) {
    float length = 1.0f * scale;
    float headLength = 0.25f * scale;
    float headRadius = 0.08f * scale;
    float shaftRadius = 0.02f * scale;

    DrawArrowSolid(origin, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, length, headLength, headRadius, shaftRadius, ColorFor(Axis::X, selected, Red()));
    DrawArrowSolid(origin, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, length, headLength, headRadius, shaftRadius, ColorFor(Axis::Y, selected, Green()));
    DrawArrowSolid(origin, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, length, headLength, headRadius, shaftRadius, ColorFor(Axis::Z, selected, Blue()));
}

void DrawLine(const C3Vector& a, const C3Vector& b, CImVector color) {
    if (!g_theGxDevicePtr) {
        return;
    }

    C3Vector verts[2] = { a, b };
    uint16_t indices[2] = { 0, 1 };
    Submit(GxPrim_Lines, 2, verts, 2, indices, color);
}

void DrawSphere(const C3Vector& center, float radius, CImVector color) {
    if (!g_theGxDevicePtr) {
        return;
    }

    constexpr int32_t kStacks = 12;
    constexpr int32_t kSlices = 16;
    constexpr int32_t kRingVerts = (kStacks - 1) * kSlices;
    constexpr int32_t kVertCount = kRingVerts + 2;
    constexpr int32_t kSouthPole = kVertCount - 1;
    constexpr int32_t kTriCount = kSlices * 2 + (kStacks - 2) * kSlices * 2;
    constexpr int32_t kIndexCount = kTriCount * 3;

    C3Vector verts[kVertCount];
    verts[0] = { center.x, center.y, center.z + radius };
    verts[kSouthPole] = { center.x, center.y, center.z - radius };

    for (int32_t stack = 1; stack < kStacks; ++stack) {
        float phi = kPi * static_cast<float>(stack) / static_cast<float>(kStacks);
        float z = cosf(phi);
        float r = sinf(phi);

        for (int32_t i = 0; i < kSlices; ++i) {
            float theta = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kSlices);
            int32_t index = 1 + (stack - 1) * kSlices + i;
            verts[index] = { center.x + radius * r * cosf(theta), center.y + radius * r * sinf(theta), center.z + radius * z };
        }
    }

    uint16_t indices[kIndexCount];
    int32_t k = 0;

    for (int32_t i = 0; i < kSlices; ++i) {
        indices[k++] = 0;
        indices[k++] = static_cast<uint16_t>(1 + i);
        indices[k++] = static_cast<uint16_t>(1 + ((i + 1) % kSlices));
    }

    for (int32_t stack = 0; stack < kStacks - 2; ++stack) {
        int32_t ringA = 1 + stack * kSlices;
        int32_t ringB = ringA + kSlices;

        for (int32_t i = 0; i < kSlices; ++i) {
            int32_t a0 = ringA + i;
            int32_t a1 = ringA + (i + 1) % kSlices;
            int32_t b0 = ringB + i;
            int32_t b1 = ringB + (i + 1) % kSlices;
            indices[k++] = static_cast<uint16_t>(a0);
            indices[k++] = static_cast<uint16_t>(b0);
            indices[k++] = static_cast<uint16_t>(b1);
            indices[k++] = static_cast<uint16_t>(a0);
            indices[k++] = static_cast<uint16_t>(b1);
            indices[k++] = static_cast<uint16_t>(a1);
        }
    }

    int32_t lastRing = 1 + (kStacks - 2) * kSlices;

    for (int32_t i = 0; i < kSlices; ++i) {
        indices[k++] = static_cast<uint16_t>(kSouthPole);
        indices[k++] = static_cast<uint16_t>(lastRing + ((i + 1) % kSlices));
        indices[k++] = static_cast<uint16_t>(lastRing + i);
    }

    Submit(GxPrim_Triangles, kVertCount, verts, kIndexCount, indices, color);
}

void DrawWireSphere(const C3Vector& center, float radius, CImVector color) {
    constexpr int32_t kSegments = 32;
    C3Vector verts[kSegments * 3];
    uint16_t indices[kSegments * 2 * 3];
    int32_t v = 0;
    int32_t k = 0;

    for (int32_t axis = 0; axis < 3; ++axis) {
        int32_t base = v;

        for (int32_t i = 0; i < kSegments; ++i) {
            float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kSegments);
            float c = cosf(angle) * radius;
            float s = sinf(angle) * radius;
            C3Vector p = center;

            if (axis == 0) {
                p.y += c;
                p.z += s;
            } else if (axis == 1) {
                p.x += c;
                p.z += s;
            } else {
                p.x += c;
                p.y += s;
            }

            verts[v++] = p;
        }

        for (int32_t i = 0; i < kSegments; ++i) {
            indices[k++] = static_cast<uint16_t>(base + i);
            indices[k++] = static_cast<uint16_t>(base + (i + 1) % kSegments);
        }
    }

    if (!g_theGxDevicePtr) {
        return;
    }

    Submit(GxPrim_Lines, v, verts, k, indices, color);
}

void DrawTorus(const C3Vector& origin, const C3Vector& axis, const C3Vector& side, float ringRadius, float tubeRadius, CImVector color) {
    if (!g_theGxDevicePtr) {
        return;
    }

    C3Vector up = VectorMath::Cross(axis, side);
    constexpr int32_t kMajor = 32;
    constexpr int32_t kMinor = 8;
    constexpr int32_t kVertCount = kMajor * kMinor;
    constexpr int32_t kIndexCount = kMajor * kMinor * 6;

    C3Vector verts[kVertCount];

    for (int32_t i = 0; i < kMajor; ++i) {
        float u = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kMajor);
        C3Vector radial = VectorMath::Add(VectorMath::Scale(side, cosf(u)), VectorMath::Scale(up, sinf(u)));
        C3Vector ringCenter = VectorMath::Add(origin, VectorMath::Scale(radial, ringRadius));

        for (int32_t j = 0; j < kMinor; ++j) {
            float v = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(kMinor);
            C3Vector tube = VectorMath::Add(VectorMath::Scale(radial, cosf(v)), VectorMath::Scale(axis, sinf(v)));
            verts[i * kMinor + j] = VectorMath::Add(ringCenter, VectorMath::Scale(tube, tubeRadius));
        }
    }

    uint16_t indices[kIndexCount];
    int32_t k = 0;

    for (int32_t i = 0; i < kMajor; ++i) {
        int32_t nextI = (i + 1) % kMajor;

        for (int32_t j = 0; j < kMinor; ++j) {
            int32_t nextJ = (j + 1) % kMinor;
            int32_t a = i * kMinor + j;
            int32_t b = nextI * kMinor + j;
            int32_t c = nextI * kMinor + nextJ;
            int32_t d = i * kMinor + nextJ;
            indices[k++] = static_cast<uint16_t>(a);
            indices[k++] = static_cast<uint16_t>(b);
            indices[k++] = static_cast<uint16_t>(c);
            indices[k++] = static_cast<uint16_t>(a);
            indices[k++] = static_cast<uint16_t>(c);
            indices[k++] = static_cast<uint16_t>(d);
        }
    }

    Submit(GxPrim_Triangles, kVertCount, verts, kIndexCount, indices, color);
}

void DrawRotationGizmo(const C3Vector& origin, float scale, Axis selected) {
    float ringRadius = 1.0f * scale;
    float tubeRadius = 0.02f * scale;

    DrawTorus(origin, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, ringRadius, tubeRadius, ColorFor(Axis::X, selected, Red()));
    DrawTorus(origin, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, ringRadius, tubeRadius, ColorFor(Axis::Y, selected, Green()));
    DrawTorus(origin, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, ringRadius, tubeRadius, ColorFor(Axis::Z, selected, Blue()));
}

}
