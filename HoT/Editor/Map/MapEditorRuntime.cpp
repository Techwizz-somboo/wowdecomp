#include "HoT/Editor/Map/MapEditorRuntime.h"
#include <windows.h>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "HoT/Core/Chat.h"
#include "HoT/Editor/Editor.h"
#include "HoT/Editor/GizmoDraw.h"
#include "HoT/Editor/GizmoPick.h"
#include "HoT/Editor/Map/Adt/AdtDocument.h"
#include "HoT/Editor/Map/Adt/AdtStore.h"
#include "HoT/Editor/Map/Adt/AlphaMap.h"
#include "HoT/Editor/Map/BrushDecal.h"
#include "HoT/Editor/Map/Liquids.h"
#include "HoT/Editor/Map/MapClient.h"
#include "HoT/Editor/Map/MapCoords.h"
#include "HoT/Editor/Map/Placements.h"
#include "HoT/Editor/Map/Sculpt.h"
#include "HoT/Editor/Map/TextureBrush.h"
#include "HoT/Editor/Map/TextureLayers.h"
#include "HoT/Editor/Map/TileSession.h"
#include "HoT/Editor/VectorMath.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::MapEditor::Runtime {

namespace {
namespace GPick = HoT::Editor;
namespace GizmoDraw = HoT::Editor::GizmoDraw;
namespace VectorMath = HoT::Editor::VectorMath;

constexpr float kRadToDeg = 57.2957795f;

constexpr float kRingScale = 1.3f;

struct CursorInfo {
    bool m_valid = false;
    C3Vector m_position{};
    int32_t m_tileX = -1;
    int32_t m_tileY = -1;
    int32_t m_chunkX = -1;
    int32_t m_chunkY = -1;
    int32_t m_areaId = 0;
};

enum class Tool {
    Sculpt,
    Paint,
    Place,
    Liquid,
};

struct GizmoDrag {
    bool m_active = false;
    bool m_rotating = false;
    GPick::Axis m_axis = GPick::Axis::None;
    C3Vector m_startPosition{};
    C3Vector m_anchorOnAxis{};

    C3Vector m_startRotation{};
    float m_anchorAngle = 0.0f;
    C3Vector m_ringAxis{};
    C3Vector m_ringRefDir{};
    C3Vector m_ringRefPerp{};
};

struct RuntimeState {
    bool m_enabled = false;

    bool m_placeOnly = false;

    bool m_showGrid = false;
    float m_brushRadius = 10.0f;

    float m_brushStrength = 8.0f;
    Sculpt::Falloff m_falloff = Sculpt::Falloff::Smooth;
    Sculpt::Mode m_mode = Sculpt::Mode::Raise;

    Tool m_tool = Tool::Sculpt;

    int32_t m_paintLayer = 1;

    uint32_t m_liquidType = 1;

    float m_liquidOffset = 1.0f;

    std::vector<Liquids::TileRef> m_liquidPending;
    uint32_t m_lastLiquidMs = 0;

    bool m_sink = false;

    bool m_painting = false;
    uint32_t m_lastStrokeMs = 0;

    float m_strokeTarget = 0.0f;

    float m_mouseX = 0.0f;
    float m_mouseY = 0.0f;

    C3Vector m_rayStart{};
    C3Vector m_rayEnd{};
    float m_rayFraction = 0.0f;
    CursorInfo m_cursor{};

    Placements::Ref m_selection{};
    Placements::Transform m_selected{};
    std::string m_selectedName;
    GPick::Axis m_hoverAxis = GPick::Axis::None;
    GPick::Axis m_hoverRing = GPick::Axis::None;
    GizmoDrag m_drag{};

    Placements::Ref m_pending{};
    Placements::Transform m_pendingTransform{};
    uint32_t m_pendingUntilMs = 0;

    const char* m_lastMiss = "not run yet";
};

constexpr int8_t kRequiredSecurityLevel = 3;

bool HasEditorAccess() {
    return HoT::Session::HasClearance(kRequiredSecurityLevel);
}

template <int (*Fn)(lua_State*)>
int32_t Gated(lua_State* L) {
    if (!HasEditorAccess()) {
        return 0;
    }

    return Fn(L);
}

RuntimeState& State() {
    static RuntimeState state;
    return state;
}

bool& EventsRegistered() {
    static bool registered = false;
    return registered;
}

void Print(const char* format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    Chat::Print("%s", message);
}

bool PlaceOnly() {
    const RuntimeState& state = State();
    return state.m_placeOnly && !state.m_enabled;
}

bool PlacementLive() {
    const RuntimeState& state = State();
    return PlaceOnly() || (state.m_enabled && state.m_tool == Tool::Place);
}

bool AnyModeLive() {
    if (!HasEditorAccess()) {
        return false;
    }

    return State().m_enabled || PlaceOnly();
}

CursorInfo At(const C3Vector& position) {
    CursorInfo info;
    info.m_valid = true;
    info.m_position = position;
    info.m_tileX = Coords::TileX(position);
    info.m_tileY = Coords::TileY(position);
    info.m_chunkX = Coords::ChunkX(position);
    info.m_chunkY = Coords::ChunkY(position);

    CMapArea* area = Access::GetArea(info.m_tileX, info.m_tileY);
    if (CMapChunk* chunk = Access::GetChunk(area, info.m_chunkX, info.m_chunkY)) {
        info.m_areaId = chunk->m_areaId;
    }

    return info;
}

void UpdateCursor(CGWorldFrame* worldFrame) {
    RuntimeState& state = State();
    state.m_cursor = {};

    if (!worldFrame || !worldFrame->m_camera) {
        state.m_lastMiss = "no world frame or camera";
        return;
    }

    EventInputGetMousePosition(&state.m_mouseX, &state.m_mouseY);
    HoT::Editor::MouseToWorld(worldFrame, state.m_mouseX, state.m_mouseY, &state.m_rayStart, &state.m_rayEnd);

    C3Vector hit{};
    if (!Access::RaycastTerrain(state.m_rayStart, state.m_rayEnd, hit, &state.m_rayFraction)) {
        state.m_lastMiss = Access::IsActive() ? "raycast missed terrain" : "CMap is not active";
        return;
    }

    state.m_lastMiss = "";
    state.m_cursor = At(hit);
}

CursorInfo Focus() {
    RuntimeState& state = State();
    if (state.m_cursor.m_valid) {
        return state.m_cursor;
    }

    C3Vector at{};
    if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
        at = player->GetPosition();
        return At(at);
    }

    CGWorldFrame* worldFrame = CGWorldFrame::s_currentWorldFrame;
    if (!worldFrame || !worldFrame->m_camera) {
        return {};
    }

    return At(worldFrame->m_camera->m_position);
}

CursorInfo StandingAt() {
    C3Vector at{};
    if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
        at = player->GetPosition();
        return At(at);
    }

    return Focus();
}

void DrawChunkGrid(CGWorldFrame* worldFrame) {
    RuntimeState& state = State();
    const CursorInfo& cursor = state.m_cursor;
    if (!cursor.m_valid) {
        return;
    }

    CMapArea* area = Access::GetArea(cursor.m_tileX, cursor.m_tileY);
    CMapChunk* chunk = Access::GetChunk(area, cursor.m_chunkX, cursor.m_chunkY);
    if (!chunk || !chunk->m_height) {
        return;
    }

    CGxDevice* device = g_theGxDevicePtr;
    if (!device) {
        return;
    }

    const C3Vector& camera = worldFrame->m_camera->m_position;
    CImVector outer(0x30, 0xFF, 0x30, 0xFF);
    CImVector inner(0x20, 0x80, 0x20, 0xFF);

    device->RsPush();
    GxRsSet(GxRs_VertexShader, nullptr);
    GxRsSet(GxRs_PixelShader, nullptr);
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_DepthTest, 1);

    auto vertexAt = [&](int32_t index) {
        C3Vector world = Coords::VertexWorldPos(chunk, index);
        world.z += 0.15f;
        return VectorMath::Subtract(world, camera);
    };

    for (int32_t row = 0; row < 9; ++row) {
        for (int32_t col = 0; col < 9; ++col) {
            int32_t here = Coords::EncodeOuterVertex(row, col);
            if (col < 8) {
                GizmoDraw::DrawLine(vertexAt(here), vertexAt(Coords::EncodeOuterVertex(row, col + 1)), outer);
            }
            if (row < 8) {
                GizmoDraw::DrawLine(vertexAt(here), vertexAt(Coords::EncodeOuterVertex(row + 1, col)), outer);
            }
        }
    }

    for (int32_t row = 0; row < 8; ++row) {
        for (int32_t col = 0; col < 8; ++col) {
            C3Vector center = vertexAt(Coords::EncodeInnerVertex(row, col));
            GizmoDraw::DrawLine(vertexAt(Coords::EncodeOuterVertex(row, col)), center, inner);
            GizmoDraw::DrawLine(center, vertexAt(Coords::EncodeOuterVertex(row + 1, col + 1)), inner);
        }
    }

    device->RsPop();
}

float GizmoScale(const C3Vector& position, const C3Vector& camera) {
    float distance = VectorMath::Length(VectorMath::Subtract(position, camera));
    float scale = distance * 0.12f;
    if (scale < 1.0f) {
        scale = 1.0f;
    }
    if (scale > 25.0f) {
        scale = 25.0f;
    }

    return scale;
}

void ClearSelection() {
    RuntimeState& state = State();
    state.m_selection = {};
    state.m_selected = {};
    state.m_selectedName.clear();
    state.m_hoverAxis = GPick::Axis::None;
    state.m_hoverRing = GPick::Axis::None;
    state.m_drag = {};
}

bool RefreshSelection() {
    RuntimeState& state = State();
    if (!state.m_selection.Valid()) {
        return false;
    }

    Placements::Info info;
    if (!Placements::Describe(state.m_selection, info)) {
        ClearSelection();
        return false;
    }

    state.m_selected = info.m_transform;
    state.m_selectedName = info.m_name;
    return true;
}

void PrintSelection() {
    RuntimeState& state = State();
    const Placements::Ref& ref = state.m_selection;
    if (!ref.Valid()) {
        Print("MapEditor: nothing selected");
        return;
    }

    Print("MapEditor: %s %d in tile %d_%d  %s", ref.m_kind == Placements::Kind::Doodad ? "doodad" : "wmo",
        ref.m_index, ref.m_tileX, ref.m_tileY,
        state.m_selectedName.empty() ? "(name unknown)" : state.m_selectedName.c_str());
    Print("  pos %.2f %.2f %.2f  rot %.1f %.1f %.1f  scale %.3f  uid %u", state.m_selected.m_position.x,
        state.m_selected.m_position.y, state.m_selected.m_position.z, state.m_selected.m_rotation.x,
        state.m_selected.m_rotation.y, state.m_selected.m_rotation.z, state.m_selected.m_scale, ref.m_uniqueId);
}

void CommitSelection() {
    RuntimeState& state = State();
    if (!state.m_selection.Valid()) {
        return;
    }

    int32_t wasX = state.m_selection.m_tileX;
    int32_t wasY = state.m_selection.m_tileY;

    std::string error;
    if (Placements::SetTransform(state.m_selection, state.m_selected, error)) {
        if (state.m_selection.m_tileX != wasX || state.m_selection.m_tileY != wasY) {
            Print("MapEditor: moved from tile %d_%d into %d_%d, both need saving", wasX, wasY,
                state.m_selection.m_tileX, state.m_selection.m_tileY);
        }

        return;
    }

    Print("MapEditor: %s", error.c_str());
    RefreshSelection();
    Placements::Preview(state.m_selection, state.m_selected);
}

float* RotationFieldForRing(C3Vector& rotation, GPick::Axis axis) {
    switch (axis) {
        case GPick::Axis::X:
            return &rotation.z;
        case GPick::Axis::Y:
            return &rotation.x;
        case GPick::Axis::Z:
            return &rotation.y;
        default:
            return nullptr;
    }
}

void ApplyRingDrag(RuntimeState& state) {
    float angle = 0.0f;
    if (!GPick::RayAngleOnRing(state.m_rayStart, state.m_rayEnd, state.m_drag.m_startPosition,
            state.m_drag.m_ringAxis, state.m_drag.m_ringRefDir, state.m_drag.m_ringRefPerp, angle)) {
        return;
    }

    float delta = GPick::NormalizeSignedAngleDelta(angle - state.m_drag.m_anchorAngle) * kRadToDeg;

    C3Vector rotation = state.m_drag.m_startRotation;
    float* field = RotationFieldForRing(rotation, state.m_drag.m_axis);
    if (!field) {
        return;
    }

    *field = std::fmod(*field + delta, 360.0f);
    if (*field < 0.0f) {
        *field += 360.0f;
    }

    state.m_selected.m_rotation = rotation;
    Placements::Preview(state.m_selection, state.m_selected);
}

void ApplyArrowDrag(RuntimeState& state) {
    C3Vector axis = GPick::AxisDirection(state.m_drag.m_axis);
    float along = GPick::ClosestRayAxisParameter(state.m_rayStart, state.m_rayEnd,
        state.m_drag.m_startPosition, axis);
    C3Vector onAxis = VectorMath::Add(state.m_drag.m_startPosition, VectorMath::Scale(axis, along));
    C3Vector delta = VectorMath::Subtract(onAxis, state.m_drag.m_anchorOnAxis);

    state.m_selected.m_position = VectorMath::Add(state.m_drag.m_startPosition, delta);
    Placements::Preview(state.m_selection, state.m_selected);
}

void UpdatePlacement(CGWorldFrame* worldFrame) {
    RuntimeState& state = State();
    if (!PlacementLive() || !state.m_selection.Valid() || !worldFrame || !worldFrame->m_camera) {
        return;
    }

    const C3Vector& camera = worldFrame->m_camera->m_position;
    float scale = GizmoScale(state.m_selected.m_position, camera);
    float ringScale = scale * kRingScale;

    if (state.m_drag.m_active && state.m_drag.m_rotating) {
        ApplyRingDrag(state);
    } else if (state.m_drag.m_active) {
        ApplyArrowDrag(state);
    } else {
        state.m_hoverAxis = GPick::PickTranslationGizmo(state.m_rayStart, state.m_rayEnd,
            state.m_selected.m_position, scale);
        state.m_hoverRing = GPick::PickRotationGizmo(state.m_rayStart, state.m_rayEnd,
            state.m_selected.m_position, ringScale);
    }

    CGxDevice* device = g_theGxDevicePtr;
    if (!device) {
        return;
    }

    device->RsPush();
    GxRsSet(GxRs_VertexShader, nullptr);
    GxRsSet(GxRs_PixelShader, nullptr);
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_Lighting, 0);

    GxRsSet(GxRs_DepthTest, 0);

    bool dragging = state.m_drag.m_active;
    GPick::Axis arrow = dragging ? (state.m_drag.m_rotating ? GPick::Axis::None : state.m_drag.m_axis)
                                 : state.m_hoverAxis;
    GPick::Axis ring = dragging ? (state.m_drag.m_rotating ? state.m_drag.m_axis : GPick::Axis::None)
                                : state.m_hoverRing;

    C3Vector origin = VectorMath::Subtract(state.m_selected.m_position, camera);
    GizmoDraw::DrawTranslationGizmo(origin, scale, arrow);
    GizmoDraw::DrawRotationGizmo(origin, ringScale, ring);

    device->RsPop();
}

void SettleNewPlacement() {
    RuntimeState& state = State();
    if (!state.m_pending.Valid()) {
        return;
    }

    if (GetTickCount() > state.m_pendingUntilMs) {
        state.m_pending = {};
        return;
    }

    if (!Access::FindDoodadDef(state.m_pending.m_uniqueId)) {
        return;
    }

    Placements::Ref ref = state.m_pending;
    state.m_pending = {};

    std::string error;
    if (!Placements::SetTransform(ref, state.m_pendingTransform, error)) {
        Print("MapEditor: could not settle the new placement - %s", error.c_str());
    }
}

bool PublishTile(int32_t tileX, int32_t tileY);

constexpr uint32_t kLiquidTickMs = 100;

void StrokeLiquid(bool erase) {
    RuntimeState& state = State();
    uint32_t now = GetTickCount();
    if (now - state.m_lastLiquidMs < kLiquidTickMs) {
        return;
    }

    state.m_lastLiquidMs = now;

    std::string error;
    if (erase) {
        Liquids::Remove(state.m_cursor.m_position, state.m_brushRadius, -1, state.m_liquidPending,
            error);
        return;
    }

    Liquids::Add(state.m_cursor.m_position, state.m_brushRadius, state.m_liquidType,
        state.m_strokeTarget, state.m_liquidPending, error);
}

void FlushLiquidStroke() {
    RuntimeState& state = State();
    if (state.m_liquidPending.empty()) {
        return;
    }

    std::vector<Liquids::TileRef> tiles;
    tiles.swap(state.m_liquidPending);

    int32_t published = 0;
    for (size_t i = 0; i < tiles.size(); ++i) {
        bool seen = false;
        for (size_t j = 0; j < i && !seen; ++j) {
            seen = tiles[j].x == tiles[i].x && tiles[j].y == tiles[i].y;
        }

        if (!seen && PublishTile(tiles[i].x, tiles[i].y)) {
            ++published;
        }
    }

    Print("MapEditor: liquid stroke over %d tile%s", published, published == 1 ? "" : "s");
}

void UpdateStroke() {
    RuntimeState& state = State();
    uint32_t now = GetTickCount();

    if (!state.m_painting || !state.m_cursor.m_valid) {
        state.m_lastStrokeMs = now;
        return;
    }

    uint32_t elapsed = now - state.m_lastStrokeMs;
    state.m_lastStrokeMs = now;

    if (!elapsed || elapsed > 250) {
        return;
    }

    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    float direction = (shift != state.m_sink) ? -1.0f : 1.0f;
    float seconds = elapsed / 1000.0f;

    if (state.m_tool == Tool::Liquid) {
        StrokeLiquid(shift);
        return;
    }

    float closing = state.m_brushStrength * seconds / 8.0f;
    if (closing > 1.0f) {
        closing = 1.0f;
    }

    if (state.m_tool == Tool::Paint) {
        TextureBrush::Stroke paint;
        paint.m_center = state.m_cursor.m_position;
        paint.m_radius = state.m_brushRadius;
        paint.m_falloff = state.m_falloff;
        paint.m_layer = state.m_paintLayer;

        paint.m_blend = shift ? -closing : closing;

        TextureBrush::Apply(paint);
        return;
    }

    Sculpt::Stroke stroke;
    stroke.m_center = state.m_cursor.m_position;
    stroke.m_radius = state.m_brushRadius;
    stroke.m_falloff = state.m_falloff;
    stroke.m_mode = state.m_mode;
    stroke.m_targetHeight = state.m_strokeTarget;
    stroke.m_amount = direction * state.m_brushStrength * seconds;
    stroke.m_blend = closing;

    Sculpt::Apply(stroke);
}

int32_t OnMouseDown(const void* rawData, void*) {
    RuntimeState& state = State();
    if (!AnyModeLive()) {
        return 1;
    }

    const auto* data = static_cast<const EVENT_DATA_MOUSE*>(rawData);
    if (!data || data->m_button != MOUSE_BUTTON_LEFT) {
        return 1;
    }

    if (PlacementLive()) {
        if (state.m_selection.Valid() && state.m_hoverAxis != GPick::Axis::None) {
            C3Vector axis = GPick::AxisDirection(state.m_hoverAxis);
            float along = GPick::ClosestRayAxisParameter(state.m_rayStart, state.m_rayEnd,
                state.m_selected.m_position, axis);

            state.m_drag = {};
            state.m_drag.m_active = true;
            state.m_drag.m_axis = state.m_hoverAxis;
            state.m_drag.m_startPosition = state.m_selected.m_position;
            state.m_drag.m_anchorOnAxis = VectorMath::Add(state.m_selected.m_position, VectorMath::Scale(axis, along));
            return 0;
        }

        if (state.m_selection.Valid() && state.m_hoverRing != GPick::Axis::None) {
            C3Vector ringAxis{};
            C3Vector refDir{};
            C3Vector refPerp{};
            GPick::RingBasis(state.m_hoverRing, ringAxis, refDir, refPerp);

            float angle = 0.0f;
            if (!GPick::RayAngleOnRing(state.m_rayStart, state.m_rayEnd, state.m_selected.m_position,
                    ringAxis, refDir, refPerp, angle)) {
                return 0;
            }

            state.m_drag = {};
            state.m_drag.m_active = true;
            state.m_drag.m_rotating = true;
            state.m_drag.m_axis = state.m_hoverRing;
            state.m_drag.m_startPosition = state.m_selected.m_position;
            state.m_drag.m_startRotation = state.m_selected.m_rotation;
            state.m_drag.m_anchorAngle = angle;
            state.m_drag.m_ringAxis = ringAxis;
            state.m_drag.m_ringRefDir = refDir;
            state.m_drag.m_ringRefPerp = refPerp;
            return 0;
        }

        Placements::Ref picked;
        if (!Placements::Pick(state.m_rayStart, state.m_rayEnd, picked)) {
            if (PlaceOnly()) {
                return 1;
            }

            ClearSelection();
            Print("MapEditor: no placement under the cursor");
            return 0;
        }

        ClearSelection();
        state.m_selection = picked;
        if (RefreshSelection()) {
            PrintSelection();
        }

        return 0;
    }

    if (!state.m_cursor.m_valid) {
        return 1;
    }

    state.m_painting = true;
    state.m_lastStrokeMs = GetTickCount();
    state.m_strokeTarget = state.m_cursor.m_position.z;

    if (state.m_tool == Tool::Liquid) {
        state.m_strokeTarget += state.m_liquidOffset;

        state.m_lastLiquidMs = state.m_lastStrokeMs - kLiquidTickMs;
    }

    return 0;
}

int32_t OnMouseUp(const void* rawData, void*) {
    const auto* data = static_cast<const EVENT_DATA_MOUSE*>(rawData);
    if (!data || data->m_button != MOUSE_BUTTON_LEFT) {
        return 1;
    }

    RuntimeState& state = State();
    state.m_painting = false;
    FlushLiquidStroke();

    if (state.m_drag.m_active) {
        state.m_drag = {};
        CommitSelection();
    }

    return 1;
}

int32_t SetBrushStrength(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        State().m_brushStrength = static_cast<float>(lua_tonumber(L, 1));
    }

    Print("MapEditor: brush strength %.2f yards/sec", State().m_brushStrength);
    return 0;
}

int32_t SetBrushMode(lua_State* L) {
    const char* name = lua_tolstring(L, 1, nullptr);
    RuntimeState& state = State();

    if (name && !std::strcmp(name, "lower")) {
        state.m_mode = Sculpt::Mode::Raise;
        state.m_sink = true;
    } else if (name && !std::strcmp(name, "flatten")) {
        state.m_mode = Sculpt::Mode::Flatten;
        state.m_sink = false;
    } else if (name && !std::strcmp(name, "smooth")) {
        state.m_mode = Sculpt::Mode::Smooth;
        state.m_sink = false;
    } else if (name && !std::strcmp(name, "noise")) {
        state.m_mode = Sculpt::Mode::Noise;
        state.m_sink = false;
    } else {
        state.m_mode = Sculpt::Mode::Raise;
        state.m_sink = false;
        name = "raise";
    }

    Print("MapEditor: brush mode %s (hold shift to invert)", name);
    return 0;
}

int32_t SetTool(lua_State* L) {
    const char* name = lua_tolstring(L, 1, nullptr);
    RuntimeState& state = State();

    if (name && !std::strcmp(name, "paint")) {
        state.m_tool = Tool::Paint;
        name = "paint";
    } else if (name && (!std::strcmp(name, "place") || !std::strcmp(name, "move"))) {
        state.m_tool = Tool::Place;
        name = "place - click a model to select it, drag an arrow to move it";
    } else if (name && (!std::strcmp(name, "liquid") || !std::strcmp(name, "water"))) {
        state.m_tool = Tool::Liquid;
        name = "liquid - drag to fill, hold shift to clear";
    } else {
        state.m_tool = Tool::Sculpt;
        name = "sculpt";
    }

    if (state.m_tool != Tool::Place) {
        ClearSelection();
    }

    Print("MapEditor: tool %s", name);
    return 0;
}

int32_t SetPaintLayer(lua_State* L) {
    RuntimeState& state = State();

    if (lua_isnumber(L, 1)) {
        int32_t layer = static_cast<int32_t>(lua_tonumber(L, 1));
        state.m_paintLayer = layer < 0 ? 0 : (layer > 3 ? 3 : layer);
    }

    Print("MapEditor: paint layer %d (hold shift to erase it)", state.m_paintLayer);
    return 0;
}

int ListTextures(lua_State*) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no terrain under the cursor");
        return 0;
    }

    CMapArea* area = Access::GetArea(cursor.m_tileX, cursor.m_tileY);
    CMapChunk* chunk = Access::GetChunk(area, cursor.m_chunkX, cursor.m_chunkY);
    if (!Access::IsAreaReady(area) || !chunk || !chunk->m_header || !chunk->m_layers) {
        Print("MapEditor: tile %d,%d is not loaded", cursor.m_tileX, cursor.m_tileY);
        return 0;
    }

    Print("MapEditor: chunk %d,%d of tile %d,%d has %u layer%s of %u tile textures",
        cursor.m_chunkX, cursor.m_chunkY, cursor.m_tileX, cursor.m_tileY, chunk->m_header->m_nLayers,
        chunk->m_header->m_nLayers == 1 ? "" : "s", Access::TextureCount(area));

    std::string error;
    Session::OpenTile* tile = Session::Open(cursor.m_tileX, cursor.m_tileY, error);
    const Adt::Mcnk* mcnk = tile ? tile->m_doc.ChunkAt(cursor.m_chunkX, cursor.m_chunkY) : nullptr;

    Adt::ChunkAlpha alpha;
    bool haveAlpha = mcnk && Adt::ReadChunkAlpha(*mcnk, Adt::FormatFor(*mcnk, Access::MapFlags()), alpha, error);

    int32_t texel = 0;
    if (haveAlpha) {
        int32_t row = static_cast<int32_t>(
            (chunk->m_topLeftCoords.x - cursor.m_position.x) / Coords::kTexelSize);
        int32_t col = static_cast<int32_t>(
            (chunk->m_topLeftCoords.y - cursor.m_position.y) / Coords::kTexelSize);

        row = row < 0 ? 0 : (row > 63 ? 63 : row);
        col = col < 0 ? 0 : (col > 63 ? 63 : col);
        texel = Coords::EncodeTexel(row, col);
    }

    int32_t base = 255;
    for (uint32_t i = 1; haveAlpha && i < alpha.m_maps.size(); ++i) {
        base -= alpha.m_maps[i].m_texel[texel];
    }

    for (uint32_t i = 0; i < chunk->m_header->m_nLayers && i < 4; ++i) {
        const SMLayer& layer = chunk->m_layers[i];
        const char* name = Access::TerrainTextureName(area, layer.m_textureId);

        if (!haveAlpha || i >= alpha.m_maps.size()) {
            Print("  layer %u -> tex %u  %s", i, layer.m_textureId, name ? name : "?");
            continue;
        }

        int32_t here = i ? alpha.m_maps[i].m_texel[texel] : (base < 0 ? 0 : base);
        Print("  layer %u -> tex %u  alpha %d  %s", i, layer.m_textureId, here,
            name ? name : "?");
    }

    return 0;
}

int ListTileTextures(lua_State*) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no terrain under the cursor");
        return 0;
    }

    std::vector<std::string> names;
    std::string error;
    if (!TextureLayers::ListTileTextures(cursor.m_tileX, cursor.m_tileY, names, error)) {
        Print("MapEditor: %s", error.c_str());
        return 0;
    }

    Print("MapEditor: tile %d,%d has %u texture%s", cursor.m_tileX, cursor.m_tileY,
        static_cast<uint32_t>(names.size()), names.size() == 1 ? "" : "s");

    for (size_t i = 0; i < names.size(); ++i) {
        Print("  %u  %s", static_cast<uint32_t>(i), names[i].c_str());
    }

    return 0;
}

int32_t AddLayer(lua_State* L) {
    RuntimeState& state = State();
    const char* texture = lua_tolstring(L, 1, nullptr);

    if (!texture || !*texture) {
        Print("MapEditor: MapEditor_AddLayer(\"Tileset\\\\...\\\\Something.blp\")");
        return 0;
    }

    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no cursor and no character to work from");
        return 0;
    }

    int32_t skipped = 0;
    std::string error;
    int32_t added = TextureLayers::Add(cursor.m_position, state.m_brushRadius, texture,
        skipped, error);

    if (!added) {
        Print("MapEditor: added nothing - %s", error.c_str());
        return 0;
    }

    Print("MapEditor: added %s to %d chunk%s, skipped %d", texture, added,
        added == 1 ? "" : "s", skipped);
    Print("  paint it with MapEditor_ListTextures to find its layer number");
    return 0;
}

int32_t RemoveLayer(lua_State* L) {
    RuntimeState& state = State();
    int32_t layer = lua_isnumber(L, 1)
        ? static_cast<int32_t>(lua_tonumber(L, 1))
        : state.m_paintLayer;

    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no cursor and no character to work from");
        return 0;
    }

    int32_t skipped = 0;
    std::string error;
    int32_t removed = TextureLayers::Remove(cursor.m_position, state.m_brushRadius, layer,
        skipped, error);

    if (!removed) {
        Print("MapEditor: removed nothing - %s", error.c_str());
        return 0;
    }

    Print("MapEditor: removed layer %d from %d chunk%s, skipped %d", layer, removed,
        removed == 1 ? "" : "s", skipped);
    Print("  layers above it have shifted down one");
    return 0;
}

int32_t SetFalloff(lua_State* L) {
    const char* name = lua_tolstring(L, 1, nullptr);
    RuntimeState& state = State();

    if (name && !std::strcmp(name, "linear")) {
        state.m_falloff = Sculpt::Falloff::Linear;
    } else if (name && !std::strcmp(name, "flat")) {
        state.m_falloff = Sculpt::Falloff::Flat;
    } else {
        state.m_falloff = Sculpt::Falloff::Smooth;
    }

    Print("MapEditor: falloff %s", name ? name : "smooth");
    return 0;
}

int SaveEdits(lua_State*) {
    int32_t saved = 0;
    std::string error;

    if (!Session::SaveDirty(saved, error)) {
        Print("MapEditor: save failed after %d tiles - %s", saved, error.c_str());
        return 0;
    }

    if (!saved) {
        Print("MapEditor: nothing to save");
    } else {
        Print("MapEditor: saved %d tile%s", saved, saved == 1 ? "" : "s");
    }

    return 0;
}

int RevertEdits(lua_State*) {
    int32_t reverted = Session::RevertAll();
    TextureBrush::Reset();
    TextureLayers::Reset();
    Print("MapEditor: reverted %d tile%s", reverted, reverted == 1 ? "" : "s");
    return 0;
}

int DumpTile(lua_State*) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no terrain under the cursor");
        return 0;
    }

    CMapArea* area = Access::GetArea(cursor.m_tileX, cursor.m_tileY);
    if (!Access::IsAreaReady(area)) {
        Print("MapEditor: tile %d,%d is not loaded", cursor.m_tileX, cursor.m_tileY);
        return 0;
    }

    Print("MapEditor: %s_%d_%d  file %p size %d  index %d,%d", Access::MapName(),
        cursor.m_tileX, cursor.m_tileY, area->m_fileBuffer, area->m_fileSize, area->m_index.x, area->m_index.y);
    Print("  header %p flags 0x%X  mcin +0x%X mtex +0x%X mmdx +0x%X mddf +0x%X modf +0x%X mh2o +0x%X",
        area->m_header, area->m_header->m_flags, area->m_header->m_mcin, area->m_header->m_mtex,
        area->m_header->m_mmdx, area->m_header->m_mddf, area->m_header->m_modf, area->m_header->m_mh2o);
    Print("  doodads %d  wmos %d", area->m_doodadDefCount, area->m_mapObjDefCount);

    for (int32_t i = 0; i < area->m_doodadDefCount && i < 3; ++i) {
        const SMDoodadDef& def = area->m_doodadDef[i];
        const char* name = Access::DoodadFileName(area, def.m_nameId);
        Print("  MDDF[%d] id %u uid %u (%.1f %.1f %.1f) %s", i, def.m_nameId, def.m_uniqueId,
            def.m_position.x, def.m_position.y, def.m_position.z, name ? name : "?");
    }

    for (int32_t i = 0; i < area->m_mapObjDefCount && i < 3; ++i) {
        const SMMapObjDef& def = area->m_mapObjDef[i];
        const char* name = Access::MapObjFileName(area, def.m_nameId);
        Print("  MODF[%d] id %u uid %u (%.1f %.1f %.1f) %s", i, def.m_nameId, def.m_uniqueId,
            def.m_position.x, def.m_position.y, def.m_position.z, name ? name : "?");
    }

    CMapChunk* chunk = Access::GetChunk(area, cursor.m_chunkX, cursor.m_chunkY);
    SMChunkInfo* info = Access::GetChunkInfo(area, cursor.m_chunkX, cursor.m_chunkY);
    if (chunk && chunk->m_header && info) {
        const SMChunk* header = chunk->m_header;
        uint32_t fileOffset =
            static_cast<uint32_t>(chunk->m_chunkInfoBeginPtr - static_cast<uint8_t*>(area->m_fileBuffer));

        Print("  chunk %d,%d  MCIN off 0x%X size %u flags 0x%X  ptr off 0x%X", cursor.m_chunkX,
            cursor.m_chunkY, info->m_offset, info->m_size, info->m_flags, fileOffset);
        Print("    index %d,%d  areaId %d  layers %u  doodadRefs %u  wmoRefs %u  holes 0x%X",
            header->m_index.x, header->m_index.y, chunk->m_areaId, header->m_nLayers,
            header->m_nDoodadRefs, header->m_nMapObjRefs, header->m_holes);
        Print("    position (%.2f %.2f %.2f)  topLeftCoords (%.2f %.2f %.2f)", header->m_position.x,
            header->m_position.y, header->m_position.z, chunk->m_topLeftCoords.x,
            chunk->m_topLeftCoords.y, chunk->m_topLeftCoords.z);
        Print("    ofsAlpha 0x%X sizeAlpha %u  ofsLiquid 0x%X sizeLiquid %u  ofsMCCV 0x%X",
            header->m_ofsAlpha, header->m_sizeAlpha, header->m_ofsLiquid, header->m_sizeLiquid,
            header->m_ofsMCCV);
        Print("    height[0] %.3f -> world %.3f   height[144] %.3f -> world %.3f",
            chunk->m_height[0], Coords::VertexHeight(chunk, 0), chunk->m_height[144],
            Coords::VertexHeight(chunk, 144));

        for (uint32_t layer = 0; layer < header->m_nLayers && layer < 4; ++layer) {
            Print("    MCLY[%u] tex %u flags 0x%X ofsAlpha 0x%X", layer,
                chunk->m_layers[layer].m_textureId, chunk->m_layers[layer].m_flags,
                chunk->m_layers[layer].m_offsetInMCAL);
        }
    }

    return 0;
}

struct Coverage {
    int32_t m_tiles = 0;
    int32_t m_mh2o = 0;
    int32_t m_mfbo = 0;
    int32_t m_mtxf = 0;
    int32_t m_mclq = 0;
    int32_t m_mcsh = 0;
    int32_t m_mccv = 0;
    int32_t m_maxLayers = 0;

    int32_t m_alphaChunks = 0;
    int32_t m_alphaCompressed = 0;
    int32_t m_alphaFailed = 0;
    int32_t m_alphaMismatch = 0;
    int32_t m_alphaExact = 0;
    bool m_bigAlpha = false;
};

void MeasureAlpha(const Adt::Mcnk& chunk, uint32_t mapFlags, Coverage& cov) {
    Adt::AlphaFormat format = Adt::FormatFor(chunk, mapFlags);

    bool any = false;
    bool compressed = false;
    for (uint32_t i = 0; i < chunk.m_header.m_nLayers; ++i) {
        const Adt::SubChunk* mcly = chunk.Find(Adt::kMCLY);
        if (!mcly || mcly->m_data.size() < (i + 1) * sizeof(SMLayer)) {
            break;
        }

        SMLayer layer{};
        std::memcpy(&layer, mcly->m_data.data() + i * sizeof(SMLayer), sizeof(SMLayer));

        if (layer.m_flags & Adt::kLayerUseAlpha) {
            any = true;
        }
        if (layer.m_flags & Adt::kLayerCompressed) {
            compressed = true;
        }
    }

    if (!any) {
        return;
    }

    ++cov.m_alphaChunks;
    cov.m_alphaCompressed += compressed ? 1 : 0;

    std::string error;
    Adt::ChunkAlpha decoded;
    if (!Adt::ReadChunkAlpha(chunk, format, decoded, error)) {
        ++cov.m_alphaFailed;
        return;
    }

    Adt::Mcnk rebuilt = chunk;
    if (!Adt::WriteChunkAlpha(decoded, format, rebuilt, error)) {
        ++cov.m_alphaFailed;
        return;
    }

    Adt::ChunkAlpha again;
    if (!Adt::ReadChunkAlpha(rebuilt, format, again, error)) {
        ++cov.m_alphaFailed;
        return;
    }

    if (decoded.m_maps.size() != again.m_maps.size()) {
        ++cov.m_alphaMismatch;
        return;
    }

    for (size_t i = 0; i < decoded.m_maps.size(); ++i) {
        if (std::memcmp(decoded.m_maps[i].m_texel, again.m_maps[i].m_texel, Adt::kAlphaTexels) != 0) {
            ++cov.m_alphaMismatch;
            return;
        }
    }

    const Adt::SubChunk* before = chunk.Find(Adt::kMCAL);
    const Adt::SubChunk* after = rebuilt.Find(Adt::kMCAL);
    if (before && after && before->m_data == after->m_data) {
        ++cov.m_alphaExact;
    }
}

void Measure(const Adt::AdtDocument& doc, Coverage& cov, uint32_t mapFlags) {
    ++cov.m_tiles;
    cov.m_bigAlpha = (mapFlags & 0x4) != 0;

    for (const Adt::TopChunk& top : doc.m_order) {
        if (top.m_data.empty()) {
            continue;
        }

        if (top.m_id == Adt::kMH2O) {
            ++cov.m_mh2o;
        } else if (top.m_id == Adt::kMFBO) {
            ++cov.m_mfbo;
        } else if (top.m_id == Adt::kMTXF) {
            ++cov.m_mtxf;
        }
    }

    bool mclq = false;
    bool mcsh = false;
    bool mccv = false;

    for (const Adt::Mcnk& chunk : doc.m_chunks) {
        if (static_cast<int32_t>(chunk.m_header.m_nLayers) > cov.m_maxLayers) {
            cov.m_maxLayers = static_cast<int32_t>(chunk.m_header.m_nLayers);
        }

        MeasureAlpha(chunk, mapFlags, cov);

        for (const Adt::SubChunk& sub : chunk.m_subs) {
            if (sub.m_data.empty()) {
                continue;
            }

            if (sub.m_id == Adt::kMCLQ) {
                mclq = true;
            } else if (sub.m_id == Adt::kMCSH) {
                mcsh = true;
            } else if (sub.m_id == Adt::kMCCV) {
                mccv = true;
            }
        }
    }

    cov.m_mclq += mclq ? 1 : 0;
    cov.m_mcsh += mcsh ? 1 : 0;
    cov.m_mccv += mccv ? 1 : 0;
}

enum class TripResult {
    Missing,
    Failed,
    Ok
};

TripResult RoundTripTile(int32_t tileX, int32_t tileY, bool verbose,
    const char* mapName = nullptr, Coverage* cov = nullptr, bool detail = true,
    uint32_t mapFlags = 0) {
    std::string path = mapName ? Adt::TilePath(mapName, tileX, tileY)
                               : Adt::TilePath(tileX, tileY);
    std::string error;

    std::vector<uint8_t> original;
    if (!Adt::ReadFileBytes(path.c_str(), original, error)) {
        if (verbose) {
            Print("MapEditor: %d,%d read failed - %s", tileX, tileY, error.c_str());
        }
        return TripResult::Missing;
    }

    Adt::AdtDocument doc;
    if (!Adt::Read(original.data(), static_cast<uint32_t>(original.size()), doc, error)) {
        if (detail) {
            Print("MapEditor: %d,%d parse failed - %s", tileX, tileY, error.c_str());
        }
        return TripResult::Failed;
    }

    if (cov) {
        Measure(doc, *cov, mapFlags);
    }

    std::vector<uint8_t> rebuilt;
    if (!Adt::Write(doc, rebuilt, error)) {
        if (detail) {
            Print("MapEditor: %d,%d write failed - %s", tileX, tileY, error.c_str());
        }
        return TripResult::Failed;
    }

    if (verbose) {
        Print("MapEditor: %s  %u bytes  %u top chunks  %u MCNK", path.c_str(),
            static_cast<uint32_t>(original.size()), static_cast<uint32_t>(doc.m_order.size()),
            static_cast<uint32_t>(doc.m_chunks.size()));
    }

    if (rebuilt.size() != original.size()) {
        if (detail) {
            Print("MapEditor: %d,%d size differs - wrote %u, expected %u", tileX, tileY,
                static_cast<uint32_t>(rebuilt.size()),
                static_cast<uint32_t>(original.size()));
        }
        return TripResult::Failed;
    }

    uint32_t size = static_cast<uint32_t>(original.size());

    if (std::memcmp(original.data(), rebuilt.data(), size) == 0) {
        if (verbose) {
            Print("MapEditor: %d,%d round-trip is byte identical", tileX, tileY);
        }
        return TripResult::Ok;
    }

    uint32_t runs = 0;
    uint32_t i = 0;

    while (i < size && runs < 8 && detail) {
        if (original[i] == rebuilt[i]) {
            ++i;
            continue;
        }

        uint32_t start = i;
        while (i < size && original[i] != rebuilt[i]) {
            ++i;
        }

        uint32_t length = i - start;
        uint32_t was = 0;
        uint32_t now = 0;
        std::memcpy(&was, original.data() + start, length < 4 ? length : 4);
        std::memcpy(&now, rebuilt.data() + start, length < 4 ? length : 4);

        Print("  diff at 0x%X (%u bytes) %s: was 0x%X now 0x%X", start, length,
            Adt::DescribeOffset(doc, start).c_str(), was, now);

        if (runs == 0) {
            int32_t mcnk = Adt::ChunkIndexAtOffset(doc, start);
            if (mcnk >= 0) {
                for (const Adt::SubChunk& sub : doc.m_chunks[mcnk].m_subs) {
                    Print("    MCNK %d %s declares %u occupies %u", mcnk,
                        Adt::ChunkName(sub.m_id).c_str(), sub.DeclaredSize(),
                        static_cast<uint32_t>(sub.m_data.size()));
                }
            }
        }

        ++runs;
    }

    if (detail) {
        Print("MapEditor: %d,%d round-trip MISMATCH", tileX, tileY);
    }

    return TripResult::Failed;
}

int RoundTrip(lua_State*) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no terrain under the cursor");
        return 0;
    }

    RoundTripTile(cursor.m_tileX, cursor.m_tileY, true);
    return 0;
}

int RoundTripAll(lua_State*) {
    int32_t tested = 0;
    int32_t failed = 0;

    for (int32_t tileY = 0; tileY < 64; ++tileY) {
        for (int32_t tileX = 0; tileX < 64; ++tileX) {
            if (!Access::IsAreaReady(Access::GetArea(tileX, tileY))) {
                continue;
            }

            ++tested;
            if (RoundTripTile(tileX, tileY, false) != TripResult::Ok) {
                ++failed;
            }
        }
    }

    Print("MapEditor: round-tripped %d loaded tiles, %d failed", tested, failed);
    return 0;
}

int32_t RoundTripMap(lua_State* L) {
    const char* mapName = lua_tolstring(L, 1, nullptr);
    if (!mapName || !*mapName) {
        mapName = Access::MapName();
    }

    int32_t tested = 0;
    int32_t failed = 0;
    int32_t reported = 0;
    Coverage cov;

    uint32_t mapFlags = 0;
    std::string flagsError;
    if (!Adt::ReadMapFlags(mapName, mapFlags, flagsError)) {
        Print("MapEditor: no MPHD for %s (%s), assuming 4 bit alpha", mapName,
            flagsError.c_str());
    }

    for (int32_t tileY = 0; tileY < 64; ++tileY) {
        for (int32_t tileX = 0; tileX < 64; ++tileX) {
            bool detail = reported < 4;
            TripResult result = RoundTripTile(tileX, tileY, false, mapName, &cov, detail,
                mapFlags);

            if (result == TripResult::Missing) {
                continue;
            }

            ++tested;
            if (result == TripResult::Failed) {
                ++failed;
                if (detail) {
                    ++reported;
                }
            }
        }
    }

    Print("MapEditor: %s - round-tripped %d tiles, %d failed", mapName, tested, failed);
    Print("  covered: MH2O %d  MFBO %d  MTXF %d  MCLQ %d  MCSH %d  MCCV %d  max layers %d",
        cov.m_mh2o, cov.m_mfbo, cov.m_mtxf, cov.m_mclq, cov.m_mcsh, cov.m_mccv, cov.m_maxLayers);
    Print("  alpha: %s  %d chunks  %d compressed  %d exact  %d MISMATCH  %d FAILED",
        cov.m_bigAlpha ? "8 bit" : "4 bit", cov.m_alphaChunks, cov.m_alphaCompressed,
        cov.m_alphaExact, cov.m_alphaMismatch, cov.m_alphaFailed);
    return 0;
}

int SaveTile(lua_State*) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no terrain under the cursor");
        return 0;
    }

    const char* mapName = Access::MapName();
    std::string path = Adt::TilePath(mapName, cursor.m_tileX, cursor.m_tileY);

    std::vector<uint8_t> original;
    std::string error;
    if (!Adt::ReadFileBytes(path.c_str(), original, error)) {
        Print("MapEditor: read failed - %s", error.c_str());
        return 0;
    }

    Adt::AdtDocument doc;
    if (!Adt::Read(original.data(), static_cast<uint32_t>(original.size()), doc, error)) {
        Print("MapEditor: parse failed - %s", error.c_str());
        return 0;
    }

    std::vector<uint8_t> rebuilt;
    if (!Adt::Write(doc, rebuilt, error)) {
        Print("MapEditor: write failed - %s", error.c_str());
        return 0;
    }

    bool identical = rebuilt.size() == original.size() && std::memcmp(original.data(), rebuilt.data(), original.size()) == 0;

    if (!Adt::SaveTile(mapName, cursor.m_tileX, cursor.m_tileY, rebuilt, error)) {
        Print("MapEditor: save failed - %s", error.c_str());
        return 0;
    }

    Access::ReloadTile(cursor.m_tileX, cursor.m_tileY);

    Print("MapEditor: saved %d,%d (%u bytes, %s) and purged",
        cursor.m_tileX, cursor.m_tileY, static_cast<uint32_t>(rebuilt.size()),
        identical ? "byte identical" : "DIFFERS from source");
    Print("  %s",
        Adt::DiskTilePath(mapName, cursor.m_tileX, cursor.m_tileY).string().c_str());
    return 0;
}

int32_t RaiseChunk(lua_State* L) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no terrain under the cursor");
        return 0;
    }

    float delta = lua_isnumber(L, 1)
        ? static_cast<float>(lua_tonumber(L, 1))
        : 5.0f;

    const char* mapName = Access::MapName();
    std::string path = Adt::TilePath(mapName, cursor.m_tileX, cursor.m_tileY);

    std::vector<uint8_t> bytes;
    std::string error;
    if (!Adt::ReadFileBytes(path.c_str(), bytes, error)) {
        Print("MapEditor: read failed - %s", error.c_str());
        return 0;
    }

    Adt::AdtDocument doc;
    if (!Adt::Read(bytes.data(), static_cast<uint32_t>(bytes.size()), doc, error)) {
        Print("MapEditor: parse failed - %s", error.c_str());
        return 0;
    }

    Adt::Mcnk* chunk = doc.ChunkAt(cursor.m_chunkX, cursor.m_chunkY);
    Adt::SubChunk* mcvt = chunk ? chunk->Find(Adt::kMCVT) : nullptr;
    if (!mcvt || mcvt->m_data.size() < Adt::kMcvtHeights * sizeof(float)) {
        Print("MapEditor: chunk %d,%d has no usable MCVT", cursor.m_chunkX, cursor.m_chunkY);
        return 0;
    }

    float* heights = reinterpret_cast<float*>(mcvt->m_data.data());
    for (int32_t i = 0; i < Adt::kMcvtHeights; ++i) {
        heights[i] += delta;
    }

    std::vector<uint8_t> rebuilt;
    if (!Adt::Write(doc, rebuilt, error)) {
        Print("MapEditor: write failed - %s", error.c_str());
        return 0;
    }

    if (!Adt::SaveTile(mapName, cursor.m_tileX, cursor.m_tileY, rebuilt, error)) {
        Print("MapEditor: save failed - %s", error.c_str());
        return 0;
    }

    Access::ReloadTile(cursor.m_tileX, cursor.m_tileY);

    Print("MapEditor: raised chunk %d,%d of tile %d,%d by %.2f and purged",
        cursor.m_chunkX, cursor.m_chunkY, cursor.m_tileX, cursor.m_tileY, delta);
    return 0;
}

int RestoreTile(lua_State*) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no terrain under the cursor");
        return 0;
    }

    std::string error;
    if (!Adt::RestoreTile(Access::MapName(), cursor.m_tileX, cursor.m_tileY, error)) {
        Print("MapEditor: restore failed - %s", error.c_str());
        return 0;
    }

    Access::ReloadTile(cursor.m_tileX, cursor.m_tileY);
    Print("MapEditor: restored %d,%d and purged", cursor.m_tileX, cursor.m_tileY);
    return 0;
}

int ReloadTile(lua_State*) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no terrain under the cursor");
        return 0;
    }

    Access::ReloadTile(cursor.m_tileX, cursor.m_tileY);
    Print("MapEditor: purged tile %d,%d", cursor.m_tileX, cursor.m_tileY);
    return 0;
}

const char* ToolName(Tool tool) {
    switch (tool) {
        case Tool::Paint:
            return "paint";
        case Tool::Place:
            return "place";
        case Tool::Liquid:
            return "liquid";
        default:
            return "sculpt";
    }
}

const char* ModeName(const RuntimeState& state) {
    switch (state.m_mode) {
        case Sculpt::Mode::Flatten:
            return "flatten";
        case Sculpt::Mode::Smooth:
            return "smooth";
        case Sculpt::Mode::Noise:
            return "noise";
        default:
            return state.m_sink ? "lower" : "raise";
    }
}

const char* FalloffName(Sculpt::Falloff falloff) {
    switch (falloff) {
        case Sculpt::Falloff::Linear:
            return "linear";
        case Sculpt::Falloff::Flat:
            return "flat";
        default:
            return "smooth";
    }
}

int32_t GetState(lua_State* L) {
    const RuntimeState& state = State();

    lua_pushboolean(L, state.m_enabled ? 1 : 0);
    lua_pushstring(L, ToolName(state.m_tool));
    lua_pushstring(L, ModeName(state));
    lua_pushstring(L, FalloffName(state.m_falloff));
    lua_pushnumber(L, state.m_brushRadius);
    lua_pushnumber(L, state.m_brushStrength);
    lua_pushnumber(L, state.m_paintLayer);
    lua_pushboolean(L, state.m_showGrid ? 1 : 0);
    lua_pushnumber(L, Session::DirtyCount());
    lua_pushboolean(L, PlaceOnly() ? 1 : 0);
    return 10;
}

int32_t GetSelection(lua_State* L) {
    RuntimeState& state = State();
    if (!state.m_selection.Valid()) {
        lua_pushnil(L);
        return 1;
    }

    const Placements::Ref& ref = state.m_selection;
    lua_pushstring(L, ref.m_kind == Placements::Kind::Doodad ? "doodad" : "wmo");
    lua_pushnumber(L, ref.m_index);
    lua_pushnumber(L, ref.m_tileX);
    lua_pushnumber(L, ref.m_tileY);
    lua_pushnumber(L, ref.m_uniqueId);
    lua_pushstring(L, state.m_selectedName.c_str());
    lua_pushnumber(L, state.m_selected.m_position.x);
    lua_pushnumber(L, state.m_selected.m_position.y);
    lua_pushnumber(L, state.m_selected.m_position.z);
    lua_pushnumber(L, state.m_selected.m_rotation.x);
    lua_pushnumber(L, state.m_selected.m_rotation.y);
    lua_pushnumber(L, state.m_selected.m_rotation.z);
    lua_pushnumber(L, state.m_selected.m_scale);
    return 13;
}

int32_t GetChunkLayers(lua_State* L) {
    CursorInfo cursor = Focus();
    CMapArea* area = Access::GetArea(cursor.m_tileX, cursor.m_tileY);
    CMapChunk* chunk = Access::GetChunk(area, cursor.m_chunkX, cursor.m_chunkY);

    if (!cursor.m_valid || !Access::IsAreaReady(area) || !chunk || !chunk->m_header || !chunk->m_layers) {
        lua_pushnil(L);
        return 1;
    }

    for (uint32_t i = 0; i < 4; ++i) {
        const char* name = i < chunk->m_header->m_nLayers
            ? Access::TerrainTextureName(area, chunk->m_layers[i].m_textureId)
            : nullptr;

        lua_pushstring(L, name ? name : "");
    }

    return 4;
}

int32_t SelectPlacement(lua_State* L) {
    RuntimeState& state = State();
    const char* kind = lua_tolstring(L, 1, nullptr);

    Placements::Ref picked;
    if (kind && *kind) {
        picked.m_kind = std::strcmp(kind, "wmo") == 0 ? Placements::Kind::MapObj
                                                    : Placements::Kind::Doodad;
        picked.m_tileX = static_cast<int32_t>(lua_tonumber(L, 2));
        picked.m_tileY = static_cast<int32_t>(lua_tonumber(L, 3));
        picked.m_index = static_cast<int32_t>(lua_tonumber(L, 4));
        picked.m_uniqueId = static_cast<uint32_t>(lua_tonumber(L, 5));

        if (!Placements::Resolve(picked)) {
            ClearSelection();
            Print("MapEditor: that placement is no longer in tile %d_%d", picked.m_tileX,
                picked.m_tileY);
            return 0;
        }
    } else if (!Placements::Pick(state.m_rayStart, state.m_rayEnd, picked)) {
        ClearSelection();
        Print("MapEditor: no placement under the cursor");
        return 0;
    }

    ClearSelection();
    state.m_selection = picked;
    if (RefreshSelection()) {
        PrintSelection();
    }

    return 0;
}

int PlacementInfo(lua_State*) {
    RefreshSelection();
    PrintSelection();
    return 0;
}

int32_t ListPlacements(lua_State* L) {
    CursorInfo cursor = StandingAt();
    if (!cursor.m_valid) {
        Print("MapEditor: nowhere to measure from");
        return 0;
    }

    float radius = lua_isnumber(L, 1) ? static_cast<float>(lua_tonumber(L, 1)) : 30.0f;

    std::vector<Placements::Info> found;
    Placements::ListNear(cursor.m_position, radius, found);

    Print("MapEditor: %d placements within %.0f yards", static_cast<int32_t>(found.size()), radius);
    for (const Placements::Info& info : found) {
        Print("  %s %d in %d_%d  %.0f yards  %s",
            info.m_ref.m_kind == Placements::Kind::Doodad ? "doodad" : "wmo", info.m_ref.m_index,
            info.m_ref.m_tileX, info.m_ref.m_tileY, info.m_distance,
            info.m_name.empty() ? "(name unknown)" : info.m_name.c_str());
    }

    return 0;
}

int32_t GetNearbyPlacements(lua_State* L) {
    CursorInfo cursor = StandingAt();
    if (!cursor.m_valid) {
        lua_pushnil(L);
        return 1;
    }

    float radius = lua_isnumber(L, 1)
        ? static_cast<float>(lua_tonumber(L, 1))
        : 30.0f;

    std::vector<Placements::Info> found;
    Placements::ListNear(cursor.m_position, radius, found);

    lua_createtable(L, static_cast<int>(found.size()), 0);

    for (size_t i = 0; i < found.size(); ++i) {
        const Placements::Info& info = found[i];

        lua_createtable(L, 0, 11);

        lua_pushstring(L,
            info.m_ref.m_kind == Placements::Kind::Doodad ? "doodad" : "wmo");
        lua_setfield(L, -2, "kind");
        lua_pushnumber(L, info.m_ref.m_tileX);
        lua_setfield(L, -2, "tileX");
        lua_pushnumber(L, info.m_ref.m_tileY);
        lua_setfield(L, -2, "tileY");
        lua_pushnumber(L, info.m_ref.m_index);
        lua_setfield(L, -2, "index");
        lua_pushnumber(L, info.m_ref.m_uniqueId);
        lua_setfield(L, -2, "uid");
        lua_pushstring(L, info.m_name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushnumber(L, info.m_distance);
        lua_setfield(L, -2, "dist");
        lua_pushnumber(L, info.m_originDistance);
        lua_setfield(L, -2, "odist");
        lua_pushnumber(L, info.m_transform.m_position.x);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, info.m_transform.m_position.y);
        lua_setfield(L, -2, "y");
        lua_pushnumber(L, info.m_transform.m_position.z);
        lua_setfield(L, -2, "z");

        lua_rawseti(L, -2, static_cast<int>(i) + 1);
    }

    return 1;
}

int32_t MovePlacement(lua_State* L) {
    RuntimeState& state = State();
    if (!RefreshSelection()) {
        Print("MapEditor: nothing selected");
        return 0;
    }

    state.m_selected.m_position.x += lua_isnumber(L, 1) ? static_cast<float>(lua_tonumber(L, 1)) : 0.0f;
    state.m_selected.m_position.y += lua_isnumber(L, 2) ? static_cast<float>(lua_tonumber(L, 2)) : 0.0f;
    state.m_selected.m_position.z += lua_isnumber(L, 3) ? static_cast<float>(lua_tonumber(L, 3)) : 0.0f;

    CommitSelection();
    PrintSelection();
    return 0;
}

int32_t RotatePlacement(lua_State* L) {
    RuntimeState& state = State();
    if (!RefreshSelection()) {
        Print("MapEditor: nothing selected");
        return 0;
    }

    state.m_selected.m_rotation.y += lua_isnumber(L, 1) ? static_cast<float>(lua_tonumber(L, 1)) : 0.0f;
    state.m_selected.m_rotation.x += lua_isnumber(L, 2) ? static_cast<float>(lua_tonumber(L, 2)) : 0.0f;
    state.m_selected.m_rotation.z += lua_isnumber(L, 3) ? static_cast<float>(lua_tonumber(L, 3)) : 0.0f;

    CommitSelection();
    PrintSelection();
    return 0;
}

int32_t ScalePlacement(lua_State* L) {
    RuntimeState& state = State();
    if (!RefreshSelection()) {
        Print("MapEditor: nothing selected");
        return 0;
    }

    if (state.m_selection.m_kind != Placements::Kind::Doodad) {
        Print("MapEditor: MODF has no scale, only doodads can be resized");
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        Print("MapEditor: scale %.3f", state.m_selected.m_scale);
        return 0;
    }

    state.m_selected.m_scale = static_cast<float>(lua_tonumber(L, 1));
    CommitSelection();
    PrintSelection();
    return 0;
}

int DeletePlacement(lua_State*) {
    RuntimeState& state = State();
    if (!state.m_selection.Valid()) {
        Print("MapEditor: nothing selected");
        return 0;
    }

    Placements::Ref ref = state.m_selection;
    std::string error;
    if (!Placements::Remove(ref, error)) {
        Print("MapEditor: %s", error.c_str());
        return 0;
    }

    ClearSelection();
    Print("MapEditor: deleted %s %d from tile %d_%d, save to keep it",
        ref.m_kind == Placements::Kind::Doodad ? "doodad" : "wmo", ref.m_index, ref.m_tileX, ref.m_tileY);
    return 0;
}

bool PublishTile(int32_t tileX, int32_t tileY) {
    std::string error;
    if (!Session::SaveOne(tileX, tileY, error)) {
        Print("MapEditor: %s", error.c_str());
        return false;
    }

    Access::ReloadTile(tileX, tileY);
    return true;
}

constexpr uint32_t kSettleWindowMs = 10000;

void WatchNewPlacement(const Placements::Ref& ref, const Placements::Transform& transform) {
    if (ref.m_kind != Placements::Kind::Doodad) {
        return;
    }

    RuntimeState& state = State();
    state.m_pending = ref;
    state.m_pendingTransform = transform;
    state.m_pendingUntilMs = GetTickCount() + kSettleWindowMs;
}

void SelectNew(const Placements::Ref& ref, const char* what) {
    ClearSelection();
    State().m_selection = ref;
    RefreshSelection();

    Print("MapEditor: %s %s %d in tile %d_%d, uid %u", what,
        ref.m_kind == Placements::Kind::Doodad ? "doodad" : "wmo", ref.m_index, ref.m_tileX, ref.m_tileY,
        ref.m_uniqueId);
}

int32_t AddPlacement(lua_State* L) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no cursor and no character to work from");
        return 0;
    }

    const char* path = lua_tolstring(L, 1, nullptr);
    if (!path || !*path) {
        Print("MapEditor: usage - MapEditor_AddPlacement(\"World/path/model.m2\" [, scale])");
        return 0;
    }

    Placements::Kind kind;
    if (!Placements::KindForPath(path, kind)) {
        Print("MapEditor: %s is neither a model nor a WMO (.m2, .mdx, .mdl, .wmo)", path);
        return 0;
    }

    Placements::Transform transform;
    transform.m_position = cursor.m_position;
    transform.m_scale = lua_isnumber(L, 2) ? static_cast<float>(lua_tonumber(L, 2)) : 1.0f;

    Placements::Ref added;
    std::string error;
    if (!Placements::Add(kind, path, transform, added, error)) {
        Print("MapEditor: %s", error.c_str());
        return 0;
    }

    if (!PublishTile(added.m_tileX, added.m_tileY)) {
        return 0;
    }

    SelectNew(added, "placed");
    WatchNewPlacement(added, transform);
    return 0;
}

int ClonePlacement(lua_State*) {
    RuntimeState& state = State();
    if (!state.m_selection.Valid()) {
        Print("MapEditor: nothing selected");
        return 0;
    }

    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no cursor and no character to work from");
        return 0;
    }

    Placements::Transform transform = state.m_selected;
    transform.m_position = cursor.m_position;

    Placements::Ref added;
    std::string error;
    if (!Placements::Clone(state.m_selection, transform, added, error)) {
        Print("MapEditor: %s", error.c_str());
        return 0;
    }

    if (!PublishTile(added.m_tileX, added.m_tileY)) {
        return 0;
    }

    SelectNew(added, "cloned to");
    WatchNewPlacement(added, transform);
    return 0;
}

int32_t GetNearbyModels(lua_State* L) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        lua_pushnil(L);
        return 1;
    }

    float radius = lua_isnumber(L, 1) ? static_cast<float>(lua_tonumber(L, 1)) : 100.0f;

    std::vector<std::string> models;
    Placements::ListModels(cursor.m_position, radius, models);

    size_t show = models.size() < 40 ? models.size() : 40;

    // FIX: 40 returns is past the 20 slots Lua guarantees a C function, so grow the stack first.
    if (!lua_checkstack(L, static_cast<int>(show))) {
        return 0;
    }

    for (size_t i = 0; i < show; ++i) {
        lua_pushstring(L, models[i].c_str());
    }

    return static_cast<int>(show);
}

int32_t PublishLiquid(const std::vector<Liquids::TileRef>& tiles) {
    int32_t published = 0;
    for (const Liquids::TileRef& tile : tiles) {
        if (PublishTile(tile.x, tile.y)) {
            ++published;
        }
    }

    return published;
}

int32_t GetLiquidTypes(lua_State* L) {
    std::vector<Liquids::TypeInfo> types;
    Liquids::ListTypes(types);

    lua_createtable(L, static_cast<int>(types.size()), 0);

    for (size_t i = 0; i < types.size(); ++i) {
        lua_createtable(L, 0, 4);

        lua_pushnumber(L, types[i].m_id);
        lua_setfield(L, -2, "id");
        lua_pushstring(L, types[i].m_name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushnumber(L, types[i].m_soundBank);
        lua_setfield(L, -2, "bank");
        lua_pushnumber(L, types[i].m_materialId);
        lua_setfield(L, -2, "material");

        lua_rawseti(L, -2, static_cast<int>(i) + 1);
    }

    return 1;
}

int32_t SetLiquidType(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        return 0;
    }

    uint32_t id = static_cast<uint32_t>(lua_tonumber(L, 1));

    Liquids::TypeInfo info;
    if (!Liquids::GetType(id, info)) {
        Print("MapEditor: no LiquidType.dbc row %u", id);
        return 0;
    }

    State().m_liquidType = id;
    return 0;
}

int32_t SetLiquidOffset(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        return 0;
    }

    float offset = static_cast<float>(lua_tonumber(L, 1));
    if (offset < 0.0f) {
        offset = 0.0f;
    }
    if (offset > 500.0f) {
        offset = 500.0f;
    }

    State().m_liquidOffset = offset;
    return 0;
}

int32_t GetLiquidType(lua_State* L) {
    uint32_t id = State().m_liquidType;

    Liquids::TypeInfo info;
    bool known = Liquids::GetType(id, info);

    lua_pushnumber(L, id);
    lua_pushstring(L, known ? info.m_name.c_str() : "?");
    lua_pushnumber(L, known ? info.m_soundBank : 0);
    lua_pushnumber(L, State().m_liquidOffset);
    return 4;
}

int32_t AddLiquid(lua_State* L) {
    RuntimeState& state = State();
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no cursor and no character to work from");
        return 0;
    }

    float height = lua_isnumber(L, 1)
        ? static_cast<float>(lua_tonumber(L, 1))
        : cursor.m_position.z + state.m_liquidOffset;

    std::vector<Liquids::TileRef> touched;
    std::string error;
    int32_t changed = Liquids::Add(cursor.m_position, state.m_brushRadius, state.m_liquidType,
        height, touched, error);

    if (!changed) {
        Print("MapEditor: added no liquid - %s", error.c_str());
        return 0;
    }

    Liquids::TypeInfo info;
    const char* name = Liquids::GetType(state.m_liquidType, info) ? info.m_name.c_str() : "?";

    Print("MapEditor: %s at %.1f over %d chunk%s", name, height, changed,
        changed == 1 ? "" : "s");
    PublishLiquid(touched);
    return 0;
}

int32_t RemoveLiquid(lua_State* L) {
    RuntimeState& state = State();
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no cursor and no character to work from");
        return 0;
    }

    int32_t layer = lua_isnumber(L, 1)
        ? static_cast<int32_t>(lua_tonumber(L, 1))
        : -1;

    std::vector<Liquids::TileRef> touched;
    std::string error;
    int32_t changed = Liquids::Remove(cursor.m_position, state.m_brushRadius, layer, touched,
        error);

    if (!changed) {
        Print("MapEditor: removed no liquid - %s", error.c_str());
        return 0;
    }

    Print("MapEditor: cleared liquid from %d chunk%s", changed, changed == 1 ? "" : "s");
    PublishLiquid(touched);
    return 0;
}

int32_t RetypeLiquid(lua_State* L) {
    RuntimeState& state = State();
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no cursor and no character to work from");
        return 0;
    }

    int32_t layer = lua_isnumber(L, 1)
        ? static_cast<int32_t>(lua_tonumber(L, 1))
        : -1;

    std::vector<Liquids::TileRef> touched;
    std::string error;
    int32_t changed = Liquids::SetType(cursor.m_position, state.m_brushRadius, layer,
        state.m_liquidType, touched, error);

    if (!changed) {
        Print("MapEditor: retyped nothing - %s", error.c_str());
        return 0;
    }

    Liquids::TypeInfo info;
    const char* name = Liquids::GetType(state.m_liquidType, info) ? info.m_name.c_str() : "?";

    Print("MapEditor: %d chunk%s now %s", changed, changed == 1 ? " is" : "s are", name);
    PublishLiquid(touched);
    return 0;
}

int LiquidHeight(lua_State* L, bool relative) {
    RuntimeState& state = State();
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no cursor and no character to work from");
        return 0;
    }

    if (!lua_isnumber(L, 1)) {
        Print("MapEditor: usage - MapEditor_%sLiquidHeight(%s [, layer])",
            relative ? "Nudge" : "Set", relative ? "delta" : "z");
        return 0;
    }

    float height = static_cast<float>(lua_tonumber(L, 1));
    int32_t layer = lua_isnumber(L, 2)
        ? static_cast<int32_t>(lua_tonumber(L, 2))
        : -1;

    std::vector<Liquids::TileRef> touched;
    std::string error;
    int32_t changed = Liquids::SetHeight(cursor.m_position, state.m_brushRadius, layer,
        height, relative, touched, error);

    if (!changed) {
        Print("MapEditor: moved nothing - %s", error.c_str());
        return 0;
    }

    Print("MapEditor: surface %s %.2f over %d chunk%s", relative ? "moved by" : "set to",
        height, changed, changed == 1 ? "" : "s");
    PublishLiquid(touched);
    return 0;
}

int32_t SetLiquidHeight(lua_State* L) {
    return LiquidHeight(L, false);
}

int32_t NudgeLiquidHeight(lua_State* L) {
    return LiquidHeight(L, true);
}

int32_t SetLiquidFlags(lua_State* L) {
    RuntimeState& state = State();
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no cursor and no character to work from");
        return 0;
    }

    int32_t fishable = lua_type(L, 1) <= 0 ? -1 : (lua_toboolean(L, 1) ? 1 : 0);
    int32_t fatigue = lua_type(L, 2) <= 0 ? -1 : (lua_toboolean(L, 2) ? 1 : 0);

    if (fishable < 0 && fatigue < 0) {
        Print("MapEditor: usage - MapEditor_SetLiquidFlags(fishable, fatigue), nil to keep");
        return 0;
    }

    std::vector<Liquids::TileRef> touched;
    std::string error;
    int32_t changed = Liquids::SetFlags(cursor.m_position, state.m_brushRadius, fishable,
        fatigue, touched, error);

    if (!changed) {
        Print("MapEditor: no flags changed - %s", error.c_str());
        return 0;
    }

    Print("MapEditor: flags on %d chunk%s", changed, changed == 1 ? "" : "s");
    PublishLiquid(touched);
    return 0;
}

int32_t GetChunkLiquid(lua_State* L) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        lua_pushnil(L);
        return 1;
    }

    Liquids::ChunkInfo info;
    std::string error;
    if (!Liquids::Describe(cursor.m_tileX, cursor.m_tileY, cursor.m_chunkX, cursor.m_chunkY, info, error)) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 3);

    lua_pushnumber(L, Liquids::CountBits(info.m_fishable));
    lua_setfield(L, -2, "fishable");
    lua_pushnumber(L, Liquids::CountBits(info.m_fatigue));
    lua_setfield(L, -2, "fatigue");

    lua_createtable(L, static_cast<int>(info.m_layers.size()), 0);

    for (size_t i = 0; i < info.m_layers.size(); ++i) {
        const Liquids::LayerInfo& layer = info.m_layers[i];

        Liquids::TypeInfo type;
        bool known = Liquids::GetType(layer.m_type, type);

        lua_createtable(L, 0, 6);

        lua_pushnumber(L, layer.m_type);
        lua_setfield(L, -2, "type");
        lua_pushstring(L, known ? type.m_name.c_str() : "?");
        lua_setfield(L, -2, "name");
        lua_pushnumber(L, layer.m_format);
        lua_setfield(L, -2, "format");
        lua_pushnumber(L, layer.m_cells);
        lua_setfield(L, -2, "cells");
        lua_pushnumber(L, layer.m_minHeight);
        lua_setfield(L, -2, "minHeight");
        lua_pushnumber(L, layer.m_maxHeight);
        lua_setfield(L, -2, "maxHeight");

        lua_rawseti(L, -2, static_cast<int>(i) + 1);
    }

    lua_setfield(L, -2, "layers");
    return 1;
}

int LiquidInfo(lua_State*) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        Print("MapEditor: no terrain under the cursor");
        return 0;
    }

    Liquids::ChunkInfo info;
    std::string error;
    if (!Liquids::Describe(cursor.m_tileX, cursor.m_tileY, cursor.m_chunkX, cursor.m_chunkY, info, error)) {
        Print("MapEditor: %s", error.c_str());
        return 0;
    }

    Print("MapEditor: chunk %d,%d of tile %d,%d has %u liquid layer%s", cursor.m_chunkX,
        cursor.m_chunkY, cursor.m_tileX, cursor.m_tileY, static_cast<uint32_t>(info.m_layers.size()),
        info.m_layers.size() == 1 ? "" : "s");

    for (size_t i = 0; i < info.m_layers.size(); ++i) {
        const Liquids::LayerInfo& layer = info.m_layers[i];

        Liquids::TypeInfo type;
        const char* name = Liquids::GetType(layer.m_type, type) ? type.m_name.c_str() : "?";

        Print("  layer %u  type %u %s  format %u  %d cell%s  z %.2f to %.2f",
            static_cast<uint32_t>(i), layer.m_type, name, layer.m_format, layer.m_cells,
            layer.m_cells == 1 ? "" : "s", layer.m_minHeight, layer.m_maxHeight);
    }

    if (!info.m_layers.empty()) {
        Print("  fishable cells %d  fatigue cells %d", Liquids::CountBits(info.m_fishable),
            Liquids::CountBits(info.m_fatigue));
    }

    return 0;
}

int32_t SetEnabled(lua_State* L) {
    State().m_enabled = lua_toboolean(L, 1);
    return 0;
}

int32_t IsEnabled(lua_State* L) {
    lua_pushboolean(L, State().m_enabled ? 1 : 0);
    return 1;
}

int32_t SetPlacementMode(lua_State* L) {
    State().m_placeOnly = lua_toboolean(L, 1);
    return 0;
}

int32_t SetBrushRadius(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        return 0;
    }

    float radius = static_cast<float>(lua_tonumber(L, 1));
    if (radius < 1.0f) {
        radius = 1.0f;
    }
    if (radius > 200.0f) {
        radius = 200.0f;
    }

    State().m_brushRadius = radius;
    return 0;
}

int32_t GetBrushRadius(lua_State* L) {
    lua_pushnumber(L, State().m_brushRadius);
    return 1;
}

int32_t SetShowGrid(lua_State* L) {
    State().m_showGrid = lua_toboolean(L, 1);
    return 0;
}

int32_t GetCursorInfo(lua_State* L) {
    CursorInfo cursor = Focus();
    if (!cursor.m_valid) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, cursor.m_position.x);
    lua_pushnumber(L, cursor.m_position.y);
    lua_pushnumber(L, cursor.m_position.z);
    lua_pushnumber(L, cursor.m_tileX);
    lua_pushnumber(L, cursor.m_tileY);
    lua_pushnumber(L, cursor.m_chunkX);
    lua_pushnumber(L, cursor.m_chunkY);
    lua_pushnumber(L, cursor.m_areaId);
    lua_pushboolean(L, State().m_cursor.m_valid ? 1 : 0);
    return 9;
}

int Debug(lua_State*) {
    const RuntimeState& state = State();
    CGWorldFrame* worldFrame = CGWorldFrame::s_currentWorldFrame;

    Print("MapEditor: placementMode %d", PlaceOnly() ? 1 : 0);
    Print("MapEditor: enabled %d  mapActive %d  worldFrame %p  camera %p", state.m_enabled ? 1 : 0,
        Access::IsActive() ? 1 : 0, worldFrame, worldFrame ? worldFrame->m_camera : nullptr);
    Print("  mouse ddc (%.4f %.4f)", state.m_mouseX, state.m_mouseY);
    Print("  ray (%.1f %.1f %.1f) -> (%.1f %.1f %.1f) frac %.4f", state.m_rayStart.x,
        state.m_rayStart.y, state.m_rayStart.z, state.m_rayEnd.x, state.m_rayEnd.y, state.m_rayEnd.z,
        state.m_rayFraction);

    if (state.m_cursor.m_valid) {
        Print("  hit (%.2f %.2f %.2f)  tile %d,%d  chunk %d,%d  area %d", state.m_cursor.m_position.x,
            state.m_cursor.m_position.y, state.m_cursor.m_position.z, state.m_cursor.m_tileX,
            state.m_cursor.m_tileY, state.m_cursor.m_chunkX, state.m_cursor.m_chunkY, state.m_cursor.m_areaId);

        CMapArea* hitArea = Access::GetArea(state.m_cursor.m_tileX, state.m_cursor.m_tileY);
        CMapChunk* hitChunk = Access::GetChunk(hitArea, state.m_cursor.m_chunkX, state.m_cursor.m_chunkY);
        if (hitChunk && hitChunk->m_height) {
            const C3Vector& corner = hitChunk->m_topLeftCoords;
            float dx = corner.x - state.m_cursor.m_position.x;
            float dy = corner.y - state.m_cursor.m_position.y;
            bool inside = dx >= 0.0f && dx <= Coords::kChunkSize && dy >= 0.0f && dy <= Coords::kChunkSize;

            Print("  chunk corner (%.2f %.2f %.2f)  offset (%.2f %.2f)  inside %d", corner.x,
                corner.y, corner.z, dx, dy, inside ? 1 : 0);

            int32_t best = 0;
            float bestDist = 1e30f;
            for (int32_t i = 0; i < Access::kVertsPerChunk; ++i) {
                float d = Coords::Distance2D(Coords::VertexWorldPos(hitChunk, i), state.m_cursor.m_position);
                if (d < bestDist) {
                    bestDist = d;
                    best = i;
                }
            }

            C3Vector nearest = Coords::VertexWorldPos(hitChunk, best);
            Print("  nearest vertex %d at (%.2f %.2f %.2f)  dist2d %.2f  dz %.2f", best, nearest.x,
                nearest.y, nearest.z, bestDist, nearest.z - state.m_cursor.m_position.z);
        }
    } else {
        Print("  no hit: %s", state.m_lastMiss);
    }

    if (worldFrame && worldFrame->m_camera) {
        const C3Vector& camera = worldFrame->m_camera->m_position;
        int32_t tileX = Coords::TileX(camera);
        int32_t tileY = Coords::TileY(camera);
        CMapArea* area = Access::GetArea(tileX, tileY);

        Print("  camera (%.1f %.1f %.1f) -> tile %d,%d  area %p ready %d", camera.x, camera.y,
            camera.z, tileX, tileY, area, Access::IsAreaReady(area) ? 1 : 0);

        if (area) {
            Print("  area index %d,%d (want %d,%d)", area->m_index.x, area->m_index.y, tileX, tileY);
        }
    }

    std::vector<std::string> lines;
    Placements::DebugRay(state.m_rayStart, state.m_rayEnd, lines);
    for (const std::string& line : lines) {
        Print("%s", line.c_str());
    }

    return 0;
}
}

void Apply() {
    Lua::RegisterFunction("MapEditor_SetEnabled", &Gated<&SetEnabled>);
    Lua::RegisterFunction("MapEditor_IsEnabled", &Gated<&IsEnabled>);
    Lua::RegisterFunction("MapEditor_SetPlacementMode", &Gated<&SetPlacementMode>);
    Lua::RegisterFunction("MapEditor_SetBrushRadius", &Gated<&SetBrushRadius>);
    Lua::RegisterFunction("MapEditor_SetBrushStrength", &Gated<&SetBrushStrength>);
    Lua::RegisterFunction("MapEditor_SetBrushMode", &Gated<&SetBrushMode>);
    Lua::RegisterFunction("MapEditor_SetTool", &Gated<&SetTool>);
    Lua::RegisterFunction("MapEditor_SetPaintLayer", &Gated<&SetPaintLayer>);
    Lua::RegisterFunction("MapEditor_ListTextures", &Gated<&ListTextures>);
    Lua::RegisterFunction("MapEditor_ListTileTextures", &Gated<&ListTileTextures>);
    Lua::RegisterFunction("MapEditor_AddLayer", &Gated<&AddLayer>);
    Lua::RegisterFunction("MapEditor_RemoveLayer", &Gated<&RemoveLayer>);
    Lua::RegisterFunction("MapEditor_SetFalloff", &Gated<&SetFalloff>);
    Lua::RegisterFunction("MapEditor_SaveEdits", &Gated<&SaveEdits>);
    Lua::RegisterFunction("MapEditor_RevertEdits", &Gated<&RevertEdits>);
    Lua::RegisterFunction("MapEditor_GetBrushRadius", &Gated<&GetBrushRadius>);
    Lua::RegisterFunction("MapEditor_SetShowGrid", &Gated<&SetShowGrid>);
    Lua::RegisterFunction("MapEditor_GetCursorInfo", &Gated<&GetCursorInfo>);
    Lua::RegisterFunction("MapEditor_DumpTile", &Gated<&DumpTile>);
    Lua::RegisterFunction("MapEditor_SaveTile", &Gated<&SaveTile>);
    Lua::RegisterFunction("MapEditor_RaiseChunk", &Gated<&RaiseChunk>);
    Lua::RegisterFunction("MapEditor_RestoreTile", &Gated<&RestoreTile>);
    Lua::RegisterFunction("MapEditor_ReloadTile", &Gated<&ReloadTile>);
    Lua::RegisterFunction("MapEditor_RoundTrip", &Gated<&RoundTrip>);
    Lua::RegisterFunction("MapEditor_RoundTripAll", &Gated<&RoundTripAll>);
    Lua::RegisterFunction("MapEditor_RoundTripMap", &Gated<&RoundTripMap>);
    Lua::RegisterFunction("MapEditor_GetState", &Gated<&GetState>);
    Lua::RegisterFunction("MapEditor_GetSelection", &Gated<&GetSelection>);
    Lua::RegisterFunction("MapEditor_GetChunkLayers", &Gated<&GetChunkLayers>);
    Lua::RegisterFunction("MapEditor_SelectPlacement", &Gated<&SelectPlacement>);
    Lua::RegisterFunction("MapEditor_PlacementInfo", &Gated<&PlacementInfo>);
    Lua::RegisterFunction("MapEditor_ListPlacements", &Gated<&ListPlacements>);
    Lua::RegisterFunction("MapEditor_GetNearbyPlacements", &Gated<&GetNearbyPlacements>);
    Lua::RegisterFunction("MapEditor_MovePlacement", &Gated<&MovePlacement>);
    Lua::RegisterFunction("MapEditor_RotatePlacement", &Gated<&RotatePlacement>);
    Lua::RegisterFunction("MapEditor_ScalePlacement", &Gated<&ScalePlacement>);
    Lua::RegisterFunction("MapEditor_DeletePlacement", &Gated<&DeletePlacement>);
    Lua::RegisterFunction("MapEditor_AddPlacement", &Gated<&AddPlacement>);
    Lua::RegisterFunction("MapEditor_ClonePlacement", &Gated<&ClonePlacement>);
    Lua::RegisterFunction("MapEditor_GetNearbyModels", &Gated<&GetNearbyModels>);
    Lua::RegisterFunction("MapEditor_GetLiquidTypes", &Gated<&GetLiquidTypes>);
    Lua::RegisterFunction("MapEditor_SetLiquidType", &Gated<&SetLiquidType>);
    Lua::RegisterFunction("MapEditor_GetLiquidType", &Gated<&GetLiquidType>);
    Lua::RegisterFunction("MapEditor_SetLiquidOffset", &Gated<&SetLiquidOffset>);
    Lua::RegisterFunction("MapEditor_AddLiquid", &Gated<&AddLiquid>);
    Lua::RegisterFunction("MapEditor_RemoveLiquid", &Gated<&RemoveLiquid>);
    Lua::RegisterFunction("MapEditor_RetypeLiquid", &Gated<&RetypeLiquid>);
    Lua::RegisterFunction("MapEditor_SetLiquidHeight", &Gated<&SetLiquidHeight>);
    Lua::RegisterFunction("MapEditor_NudgeLiquidHeight", &Gated<&NudgeLiquidHeight>);
    Lua::RegisterFunction("MapEditor_SetLiquidFlags", &Gated<&SetLiquidFlags>);
    Lua::RegisterFunction("MapEditor_LiquidInfo", &Gated<&LiquidInfo>);
    Lua::RegisterFunction("MapEditor_GetChunkLiquid", &Gated<&GetChunkLiquid>);
    Lua::RegisterFunction("MapEditor_Debug", &Gated<&Debug>);
}

void OnGameClientInitialize() {
    if (EventsRegistered()) {
        return;
    }

    EventRegisterEx(EVENT_ID_MOUSEDOWN, OnMouseDown, nullptr, 10.0f);
    EventRegisterEx(EVENT_ID_MOUSEUP, OnMouseUp, nullptr, 10.0f);
    EventsRegistered() = true;
}

void OnGameClientDestroy() {
    if (EventsRegistered()) {
        EventUnregister(EVENT_ID_MOUSEDOWN, OnMouseDown);
        EventUnregister(EVENT_ID_MOUSEUP, OnMouseUp);
        EventsRegistered() = false;
    }

    RuntimeState& state = State();
    state.m_cursor = {};
    state.m_painting = false;
    state.m_lastMiss = "game client destroyed";

    ClearSelection();

    TextureBrush::Reset();
    TextureLayers::Reset();
    Session::OnMapChanged();
}

void BeforeWorldRender(CGWorldFrame*) {
    RuntimeState& state = State();
    if (!state.m_enabled || !HasEditorAccess()) {
        return;
    }

    if (state.m_cursor.m_valid) {
        BrushDecal::Show(state.m_cursor.m_position, state.m_brushRadius, true);
    } else {
        BrushDecal::Hide();
    }
}

void AfterWorldRender(CGWorldFrame* worldFrame) {
    RuntimeState& state = State();

    SettleNewPlacement();

    if (!AnyModeLive()) {
        state.m_cursor = {};
        state.m_painting = false;
        FlushLiquidStroke();
        return;
    }

    UpdateCursor(worldFrame);

    if (state.m_enabled) {
        UpdateStroke();
    } else {
        state.m_painting = false;
        FlushLiquidStroke();
    }

    UpdatePlacement(worldFrame);

    if (state.m_showGrid && state.m_enabled && worldFrame && worldFrame->m_camera) {
        DrawChunkGrid(worldFrame);
    }
}

}
