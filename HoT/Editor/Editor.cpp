#include "HoT/Editor/Editor.h"
#include <string>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "HoT/Core/Modules.h"
#include "HoT/Editor/FreeCam.h"
#include "HoT/Editor/GobEditor.h"
#include "HoT/Editor/Map/BrushDecal.h"
#include "HoT/Editor/Map/MapEditorRuntime.h"
#include "HoT/Editor/MouseFunctions.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::Editor {

void MouseToWorld(CGWorldFrame* frame, float mouseX, float mouseY, C3Vector* start, C3Vector* end) {
    CGxDevice* device = g_theGxDevicePtr;

    if (!device || !frame || !frame->m_camera) {
        return;
    }

    // FIX: the DLL kept pointers to the live matrices, so its restore wrote back the new ones.
    C44Matrix projection = device->m_projection;
    CGxMatrixStack& stack = device->m_xforms[GxXform_View];
    C44Matrix view = stack.m_mtx[stack.m_level];
    frame->m_camera->SetupWorldProjection(frame->m_viewportDDC);

    if (!frame->GetLineSegment(mouseX, mouseY, start, end)) {
        *start = { 0.0f, 0.0f, 0.0f };
        *end = { 0.0f, 0.0f, 0.0f };
    }

    device->XformSetProjection(projection);
    device->XformSetView(view);
}

void BeforeWorldRender(CGWorldFrame* frame, C3Vector& decalPosition, float& decalSize, int32_t& decalStyle) {
    MapEditor::BrushDecal::Bind(&decalPosition, &decalSize, &decalStyle);
    MapEditor::Runtime::BeforeWorldRender(frame);
}

void AfterWorldRender(CGWorldFrame* frame) {
    GobEditor::OnWorldRender(frame);
    MapEditor::Runtime::AfterWorldRender(frame);
}

int32_t OnHitTestPoint(int32_t kind, const HITTESTRESULT* result) {
    MouseFunctions::RecordHitTest(kind, result);

    return kind;
}

CameraUpdateScope::~CameraUpdateScope() {
    FreeCam::OnCameraUpdated(this->m_camera);
}

static int32_t Packet_SMSG_GOB_EDITOR_SELECT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint64_t guid = packet.GetUInt64();

    GobEditor::SelectGameObject(guid);
    CGGameObject_C* object = GobEditor::SelectedGameObject();

    if (object) {
        // FIX: the DLL passed a std::string object through the varargs for %s.
        std::string text = GobEditor::GuidToString(guid);
        const C3Vector& position = object->m_passenger.m_position;
        Lua::SignalEvent("HOT_GOB_SELECTED", "%s%u%f%f%f", text.c_str(), 0u, position.x, position.y, position.z);
    }

    return 1;
}

class EditorModule : public Module {
    public:
    const char* Name() const override {
        return "Editor";
    }

    void OnInitializeGlobal() override {
        GobEditor::Apply();
        MapEditor::Runtime::Apply();
        FreeCam::RegisterScripts();
        MouseFunctions::RegisterScripts();
        Net::RegisterWorldHandler(Net::SMSG_GOB_EDITOR_SELECT, Packet_SMSG_GOB_EDITOR_SELECT);
    }

    void OnPlayerClientInitialize() override {
        GobEditor::OnGameInitialize();
        MapEditor::Runtime::OnGameClientInitialize();
        FreeCam::OnGameInitialize();
    }

    void OnDestroyGame() override {
        GobEditor::OnGameDestroy();
        MapEditor::Runtime::OnGameClientDestroy();
        FreeCam::OnGameDestroy();
    }
};

Module* GetModule() {
    static EditorModule module;
    return &module;
}

}
