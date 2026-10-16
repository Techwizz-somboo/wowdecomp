#ifndef HOT_EDITOR_EDITOR_H
#define HOT_EDITOR_EDITOR_H

#include <cstdint>

class C3Vector;
class CGCamera;
class CGWorldFrame;
struct HITTESTRESULT;

namespace HoT {

class Module;

namespace Editor {

// Called first thing in CGWorldFrame::OnWorldRender, the decal refs are the spell cursor shadow statics.
void BeforeWorldRender(CGWorldFrame* frame, C3Vector& decalPosition, float& decalSize, int32_t& decalStyle);

// Called last thing in CGWorldFrame::OnWorldRender.
void AfterWorldRender(CGWorldFrame* frame);

// Wraps the result of CGWorldFrame::HitTestPoint to remember what the mouse is over.
int32_t OnHitTestPoint(int32_t kind, const HITTESTRESULT* result);

// Lives for the length of CGCamera::UpdateCallback so the free camera gets the last word.
class CameraUpdateScope {
    public:
    explicit CameraUpdateScope(CGCamera* camera) : m_camera(camera) {}
    ~CameraUpdateScope();

    CameraUpdateScope(const CameraUpdateScope&) = delete;
    CameraUpdateScope& operator=(const CameraUpdateScope&) = delete;

    private:
    CGCamera* m_camera;
};

// Turns a DDC mouse position into a world ray, leaving the device transforms as it found them.
void MouseToWorld(CGWorldFrame* frame, float mouseX, float mouseY, C3Vector* start, C3Vector* end);

Module* GetModule();

}

}

#endif
