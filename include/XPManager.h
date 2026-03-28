#pragma once

namespace EA::XPManager {

    // Awards XP from a named source. Feeds directly into the engine's native
    // XP bucket — the engine handles level-up UI, perk points, and overflow carry.
    void AwardXP(float amount, std::string_view source);

    // Cosave accessors.
    float GetCurrentXP();
    void  SetCurrentXP(float xp);

    // Kill deduplication guard.
    // Returns true if this is a new kill (XP should be awarded).
    // Returns false if this FormID was already processed this session.
    bool RegisterKill(RE::FormID actorID);

    // Clears the kill guard set. Call on game load and new game.
    void ResetKillGuard();

    // Quest completion deduplication guard.
    // Returns true if this quest FormID has not yet been awarded XP this session.
    // Returns false if already processed (skip).
    bool RegisterQuestXP(RE::FormID questID);

    // Awards XP only if the quest FormID has not been seen before this session.
    // Combines RegisterQuestXP + AwardXP in one call.
    void AwardXPIfQuestNew(RE::FormID questID, float amount, std::string_view source);

    // Clears the quest guard set. Call on game load and new game.
    void ResetQuestGuard();
}
