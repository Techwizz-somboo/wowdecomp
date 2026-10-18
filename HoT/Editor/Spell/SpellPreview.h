#ifndef HOT_EDITOR_SPELL_SPELLPREVIEW_H
#define HOT_EDITOR_SPELL_SPELLPREVIEW_H

#include <cstdint>

// Plays a spell's visuals on the active player by feeding fake SMSG_SPELL_START and SMSG_SPELL_GO to the client.
namespace HoT::Editor::SpellPreview {

bool Cast(int32_t spellId, uint64_t target);
void Stop();
bool IsPlaying();

void Apply();
void OnIdle();
void OnGameDestroy();

}

#endif
