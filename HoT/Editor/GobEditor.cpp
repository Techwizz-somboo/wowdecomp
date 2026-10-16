#include "HoT/Editor/GobEditor.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "HoT/Core/Chat.h"
#include "HoT/Editor/Editor.h"
#include "HoT/Editor/GizmoDraw.h"
#include "HoT/Editor/GizmoPick.h"
#include "HoT/Editor/Quat.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::Editor::GobEditor {

struct DragState {
    bool m_active = false;
    bool m_isRotation = false;
    Axis m_axis = Axis::None;
    C3Vector m_startObjPos{};
    C3Vector m_anchorOnAxis{};
    float m_anchorAngle = 0.0f;
    C3Vector m_ringAxis{};
    C3Vector m_ringRefDir{};
    C3Vector m_ringRefPerp{};
    Quat m_startRot{};
};

struct EditorState {
    C3Vector m_start{};
    C3Vector m_end{};
    C3Vector m_gizmoPosition{};
    Axis m_gizmoTranslationAxis = Axis::None;
    Axis m_gizmoRotationAxis = Axis::None;
    DragState m_gizmoDragState{};
    uint64_t m_currentObjectGuid = 0;
};

static constexpr float kTranslationGizmoScale = 1.0f;
static constexpr float kRotationGizmoScale = 1.3f;

static EditorState s_state;
static bool s_enabled;
static bool s_eventsRegistered;

DecodedGuid DecodeGuid(uint64_t full) {
    DecodedGuid guid{};
    guid.m_full = full;
    guid.m_type = static_cast<uint16_t>(full >> 48);
    guid.m_entry = static_cast<uint32_t>((full >> 24) & 0xFFFFFF);
    guid.m_low = static_cast<uint32_t>(full & 0xFFFFFF);

    return guid;
}

std::string GuidToString(uint64_t guid) {
    return std::to_string(guid);
}

CGGameObject_C* GameObjectByGuid(uint64_t guid) {
    return static_cast<CGGameObject_C*>(ClntObjMgrObjectPtr(WOWGUID{ guid }, TYPEMASK_GAMEOBJECT, __FILE__, __LINE__));
}

uint64_t& PackedRotation(CGGameObject_C* object) {
    return *reinterpret_cast<uint64_t*>(&object->m_passenger.m_facing);
}

static bool UpdateMouseRay(const EVENT_DATA_MOUSE* data) {
    if (!data) {
        return false;
    }

    CGWorldFrame* frame = CGWorldFrame::s_currentWorldFrame;

    if (!frame || !frame->m_camera) {
        return false;
    }

    float ddcX = 0.0f;
    float ddcY = 0.0f;
    NDCToDDC(data->m_x, data->m_y, &ddcX, &ddcY);
    MouseToWorld(frame, ddcX, ddcY, &s_state.m_start, &s_state.m_end);

    return true;
}

static int32_t OnMouseDown(const void* rawData, void* param) {
    if (!s_enabled) {
        return 1;
    }

    const EVENT_DATA_MOUSE* data = static_cast<const EVENT_DATA_MOUSE*>(rawData);

    if (!data || data->m_button != MOUSE_BUTTON_LEFT) {
        return 1;
    }

    if (!UpdateMouseRay(data)) {
        return 1;
    }

    EditorState& state = s_state;
    CGGameObject_C* gameObject = SelectedGameObject();

    if (gameObject && state.m_gizmoTranslationAxis != Axis::None) {
        C3Vector axisDir = AxisDirection(state.m_gizmoTranslationAxis);
        float t = ClosestRayAxisParameter(state.m_start, state.m_end, state.m_gizmoPosition, axisDir);

        state.m_gizmoDragState.m_active = true;
        state.m_gizmoDragState.m_isRotation = false;
        state.m_gizmoDragState.m_axis = state.m_gizmoTranslationAxis;
        state.m_gizmoDragState.m_startObjPos = state.m_gizmoPosition;
        state.m_gizmoDragState.m_anchorOnAxis = VectorMath::Add(state.m_gizmoPosition, VectorMath::Scale(axisDir, t));

        return 0;
    }

    if (gameObject && state.m_gizmoRotationAxis != Axis::None) {
        C3Vector axis{};
        C3Vector refDir{};
        C3Vector refPerp{};
        RingBasis(state.m_gizmoRotationAxis, axis, refDir, refPerp);

        float angle = 0.0f;

        if (!RayAngleOnRing(state.m_start, state.m_end, state.m_gizmoPosition, axis, refDir, refPerp, angle)) {
            return 1;
        }

        state.m_gizmoDragState.m_active = true;
        state.m_gizmoDragState.m_isRotation = true;
        state.m_gizmoDragState.m_axis = state.m_gizmoRotationAxis;
        state.m_gizmoDragState.m_startObjPos = state.m_gizmoPosition;
        state.m_gizmoDragState.m_anchorAngle = angle;
        state.m_gizmoDragState.m_ringAxis = axis;
        state.m_gizmoDragState.m_ringRefDir = refDir;
        state.m_gizmoDragState.m_ringRefPerp = refPerp;
        state.m_gizmoDragState.m_startRot = UnpackQuat(PackedRotation(gameObject));

        return 0;
    }

    return 1;
}

static int32_t OnMouseUp(const void* rawData, void* param) {
    const EVENT_DATA_MOUSE* data = static_cast<const EVENT_DATA_MOUSE*>(rawData);

    if (!data || data->m_button != MOUSE_BUTTON_LEFT) {
        return 1;
    }

    s_state.m_gizmoDragState.m_active = false;
    s_state.m_gizmoDragState.m_axis = Axis::None;

    return 1;
}

static int32_t OnMouseMove(const void* rawData, void* param) {
    if (!s_enabled) {
        return 1;
    }

    const EVENT_DATA_MOUSE* data = static_cast<const EVENT_DATA_MOUSE*>(rawData);

    if (!UpdateMouseRay(data)) {
        return 1;
    }

    EditorState& state = s_state;
    CGGameObject_C* gameObject = SelectedGameObject();

    if (!gameObject) {
        state.m_gizmoTranslationAxis = Axis::None;
        state.m_gizmoRotationAxis = Axis::None;
        state.m_gizmoDragState.m_active = false;

        return 1;
    }

    DragState& drag = state.m_gizmoDragState;

    if (drag.m_active) {
        if (!drag.m_isRotation) {
            C3Vector axisDir = AxisDirection(drag.m_axis);
            float t = ClosestRayAxisParameter(state.m_start, state.m_end, drag.m_startObjPos, axisDir);
            C3Vector currentOnAxis = VectorMath::Add(drag.m_startObjPos, VectorMath::Scale(axisDir, t));
            C3Vector delta = VectorMath::Subtract(currentOnAxis, drag.m_anchorOnAxis);
            state.m_gizmoPosition = VectorMath::Add(drag.m_startObjPos, delta);

            gameObject->m_passenger.m_position = state.m_gizmoPosition;
            gameObject->UpdateWorldObject(0);

            return 1;
        }

        float currentAngle = 0.0f;

        if (!RayAngleOnRing(state.m_start, state.m_end, drag.m_startObjPos, drag.m_ringAxis, drag.m_ringRefDir, drag.m_ringRefPerp, currentAngle)) {
            return 1;
        }

        float delta = NormalizeSignedAngleDelta(currentAngle - drag.m_anchorAngle);
        Quat deltaRot = AxisAngleToQuat(drag.m_ringAxis.x, drag.m_ringAxis.y, drag.m_ringAxis.z, delta);
        Quat finalRot = MultiplyQuat(deltaRot, drag.m_startRot);
        PackedRotation(gameObject) = PackQuat(finalRot);
        gameObject->UpdateWorldObject(0);

        return 1;
    }

    state.m_gizmoPosition = gameObject->m_passenger.m_position;
    state.m_gizmoTranslationAxis = PickTranslationGizmo(state.m_start, state.m_end, state.m_gizmoPosition, kTranslationGizmoScale);
    state.m_gizmoRotationAxis = PickRotationGizmo(state.m_start, state.m_end, state.m_gizmoPosition, kRotationGizmoScale);

    return 1;
}

CGGameObject_C* SelectedGameObject() {
    if (s_state.m_currentObjectGuid == 0) {
        return nullptr;
    }

    return GameObjectByGuid(s_state.m_currentObjectGuid);
}

uint64_t CurrentSelectedGobGUID() {
    return s_state.m_currentObjectGuid;
}

void ClearSelection() {
    s_state.m_currentObjectGuid = 0;
    s_state.m_gizmoTranslationAxis = Axis::None;
    s_state.m_gizmoRotationAxis = Axis::None;
    s_state.m_gizmoDragState = {};
}

bool SelectGameObject(uint64_t guid) {
    CGGameObject_C* gameObject = GameObjectByGuid(guid);

    if (!gameObject) {
        ClearSelection();
        Chat::Print("EditorRuntime: object not found (guid %llu)", static_cast<unsigned long long>(guid));

        return false;
    }

    s_state.m_currentObjectGuid = guid;
    s_state.m_gizmoPosition = gameObject->m_passenger.m_position;
    s_state.m_gizmoTranslationAxis = Axis::None;
    s_state.m_gizmoRotationAxis = Axis::None;
    s_state.m_gizmoDragState = {};

    return true;
}

void Apply() {
    s_enabled = true;
}

void OnGameInitialize() {
    if (!s_enabled || s_eventsRegistered) {
        return;
    }

    EventRegisterEx(EVENT_ID_MOUSEDOWN, OnMouseDown, nullptr, 10.0f);
    EventRegisterEx(EVENT_ID_MOUSEUP, OnMouseUp, nullptr, 10.0f);
    EventRegisterEx(EVENT_ID_MOUSEMOVE, OnMouseMove, nullptr, 10.0f);
    s_eventsRegistered = true;
}

void OnGameDestroy() {
    if (s_eventsRegistered) {
        EventUnregister(EVENT_ID_MOUSEDOWN, OnMouseDown);
        EventUnregister(EVENT_ID_MOUSEUP, OnMouseUp);
        EventUnregister(EVENT_ID_MOUSEMOVE, OnMouseMove);
        s_eventsRegistered = false;
    }

    s_state = {};
}

void OnWorldRender(CGWorldFrame* frame) {
    if (!s_enabled || !frame || !frame->m_camera || s_state.m_currentObjectGuid == 0) {
        return;
    }

    CGGameObject_C* gameObject = SelectedGameObject();

    if (!gameObject) {
        return;
    }

    if (!s_state.m_gizmoDragState.m_active) {
        s_state.m_gizmoPosition = gameObject->m_passenger.m_position;
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

    CImVector transparent;
    GxSceneClear(2, transparent);
    GxRsSet(GxRs_DepthTest, 1);

    C3Vector drawPosition = VectorMath::Subtract(s_state.m_gizmoPosition, frame->m_camera->m_position);
    GizmoDraw::DrawTranslationGizmo(drawPosition, kTranslationGizmoScale, s_state.m_gizmoTranslationAxis);
    GizmoDraw::DrawRotationGizmo(drawPosition, kRotationGizmoScale, s_state.m_gizmoRotationAxis);

    device->RsPop();
}

}
