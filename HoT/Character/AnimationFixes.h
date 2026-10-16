#ifndef HOT_CHARACTER_ANIMATIONFIXES_H
#define HOT_CHARACTER_ANIMATIONFIXES_H

#include <cstdint>

class CGUnit_C;
class CM2Model;

namespace HoT {
class Module;
}

// Monk unarmed attacks, channels cast while moving on the torso, animation ids past the stock table and mount swim/fly animations.
namespace HoT::Animation {

static const uint32_t MONK_ATTACK_MAINHAND = 672;
static const uint32_t MONK_ATTACK_OFFHAND = 674;
static const uint32_t MONK_READY = 678;

// Monks and units with a SPELL_AURA_MONK_ANIMS aura, cast or known.
bool UsesUnarmedAnimations(const CGUnit_C* unit);

// Swaps a melee attack animation for the monk one when the unit uses unarmed animations.
uint32_t RemapMonkAttack(CGUnit_C* unit, uint32_t animId);

// PlayAttackerRound animation for a hand that holds no weapon.
uint32_t UnarmedAttackAnim(const CGUnit_C* unit, bool offhand);

// GetCombatReadyAnim result when neither hand holds a weapon.
uint32_t UnarmedReadyAnim(const CGUnit_C* unit);

// Called first in CGUnit_C::ResolveAnimForModel, may remap animId and returns 0xFFFFFFFF to let the stock lookup run.
uint32_t ResolveAnimForModel(CGUnit_C* unit, uint32_t& animId, CM2Model* model);

// True when ProcessAnimationEvent should play this animation on the torso bone.
bool RouteChannelToTorso(CGUnit_C* unit, uint32_t animId);

// Called first in CGUnit_C::SetKeyBoneSequence, may move the sequence to the torso bone and remap it.
void OnSetKeyBoneSequence(CGUnit_C* unit, CM2Model* model, uint32_t& boneId, uint32_t& sequenceId, uint32_t time, int32_t blend, int32_t primary, int32_t force);

// Called at the end of CGUnit_C::PreAnimate.
void OnPreAnimate(CGUnit_C* unit);

// Lives for the whole of CGUnit_C::UpdateBaseAnimation and splits a moving channel between legs and torso on the way out.
class UpdateBaseAnimationScope {
    public:
    UpdateBaseAnimationScope(CGUnit_C* unit, uint32_t animFlags, int32_t mask);
    ~UpdateBaseAnimationScope();

    UpdateBaseAnimationScope(const UpdateBaseAnimationScope&) = delete;
    UpdateBaseAnimationScope& operator=(const UpdateBaseAnimationScope&) = delete;

    private:
    CGUnit_C* m_unit;
    uint32_t m_animFlags;
    int32_t m_mask;
    int32_t m_rootChannelAnim;
};

// Lives for the whole of CGUnit_C::PlayFallLandAnimation and keeps a channel on the torso after landing.
class FallLandScope {
    public:
    explicit FallLandScope(CGUnit_C* unit) : m_unit(unit) {}
    ~FallLandScope();

    FallLandScope(const FallLandScope&) = delete;
    FallLandScope& operator=(const FallLandScope&) = delete;

    private:
    CGUnit_C* m_unit;
};

// The monk aura spell list is rebuilt on next use, called when Spell.dbc reloads.
void InvalidateSpellScan();

Module* GetModule();

}

#endif
