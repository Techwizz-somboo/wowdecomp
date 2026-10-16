#ifndef CONTROLLER_INPUT_H
#define CONTROLLER_INPUT_H

#include <cstdint>

namespace Controller::Input {

void Initialize();
void Shutdown();

// Turns gamepad button changes into key events, called from the OS input poll.
void OnPoll(bool appActive, bool activity);

// Sticks drive the cursor, movement and camera once per frame.
void OnIdle(float elapsed);

// Lets go of every held button, movement bit and look state.
void Release();

bool IsActive();
bool IsCursorControlEnabled();
void SetCursorControl(bool enable);
void SetAutoCursor(bool open);
bool IsFreeLookEnabled();
bool GetAllowHoverEventsWithFreeLook();
void SetAllowHoverEventsWithFreeLook(bool enable);

// Interacts with the facing mouseover, or the closest usable object in front of the player.
bool Interact();

}

#endif
