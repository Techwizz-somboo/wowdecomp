#ifndef CONTROLLER_DEVICES_H
#define CONTROLLER_DEVICES_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Controller/Buttons.h"
#include "Controller/Config.h"

namespace Controller {

// Values match the retail Enum.GamePadPowerLevel.
enum PowerLevel : int32_t {
    POWER_CRITICAL = 0,
    POWER_LOW = 1,
    POWER_MEDIUM = 2,
    POWER_HIGH = 3,
    POWER_WIRED = 4,
    POWER_UNKNOWN = 5,
};

struct StickState {
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_len = 0.0f;
};

// Stick and axis Y values are positive when pushed up.
struct MappedState {
    bool m_buttons[BUTTON_COUNT] = {};
    float m_axes[AXIS_COUNT] = {};
    StickState m_sticks[STICK_PHYSICAL_COUNT];
};

struct Device {
    int32_t m_id = 0;
    void* m_gamepad = nullptr;
    void* m_joystick = nullptr;
    std::string m_name;
    uint16_t m_vendorID = 0;
    uint16_t m_productID = 0;
    std::string m_labelStyle;
    std::vector<uint8_t> m_rawButtons;
    std::vector<float> m_rawAxes;
    MappedState m_state;
    ResolvedConfig m_config;
    int32_t m_powerLevel = POWER_UNKNOWN;
};

namespace Devices {

const int32_t COMBINED_DEVICE_ID = 0;

bool Initialize();
void Shutdown();
bool Available();

// Pumps SDL and refreshes every device, returns true when any input moved.
bool Update();

const std::vector<std::unique_ptr<Device>>& All();
Device* Find(int32_t id);
Device* Active();
const MappedState& Combined();

void ReloadConfigs();
bool AddMapping(const char* mapping);

void SetVibration(const char* type, float intensity);
void StopVibration();
void SetLed(float r, float g, float b);
void ClearLed();
bool GetLed(float& r, float& g, float& b);

}

}

#endif
