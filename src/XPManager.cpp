#include "PCH.h"
#include "XPManager.h"
#include "Config.h"

namespace EA::XPManager {

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    static float                          s_currentXP    = 0.0f;
    static std::unordered_set<RE::FormID> s_deadActors;
    static std::unordered_set<RE::FormID> s_completedQuests;

    // -----------------------------------------------------------------------
    // Cosave accessors
    // -----------------------------------------------------------------------
    float GetCurrentXP()         { return s_currentXP; }
    void  SetCurrentXP(float xp) { s_currentXP = xp; }

    // -----------------------------------------------------------------------
    // Kill guard
    // -----------------------------------------------------------------------
    bool RegisterKill(RE::FormID actorID) {
        if (s_deadActors.contains(actorID)) {
            logger::debug("[EA] Kill guard: FormID {:08X} already dead — skipped.", actorID);
            return false;
        }
        s_deadActors.insert(actorID);
        return true;
    }

    void ResetKillGuard() {
        s_deadActors.clear();
        logger::debug("[EA] Kill guard: cleared.");
    }

    // -----------------------------------------------------------------------
    // Quest guard
    // -----------------------------------------------------------------------
    bool RegisterQuestXP(RE::FormID questID) {
        if (s_completedQuests.contains(questID)) {
            logger::debug("[EA] Quest guard: FormID {:08X} already awarded XP — skipped.", questID);
            return false;
        }
        s_completedQuests.insert(questID);
        return true;
    }

    void AwardXPIfQuestNew(RE::FormID questID, float amount, std::string_view source) {
        if (!RegisterQuestXP(questID)) return;
        AwardXP(amount, source);
    }

    void ResetQuestGuard() {
        s_completedQuests.clear();
        logger::info("[EA] Quest guard: cleared.");
    }

    // -----------------------------------------------------------------------
    // AwardXP
    //
    // Feeds XP directly into the engine's native character XP bucket.
    // The engine checks xp >= levelThreshold every tick and calls
    // AdvanceLevel() when crossed — handling the attribute screen, perk point,
    // level increment, threshold update for the next level, and overflow carry
    // entirely natively. No chaining code needed on our side.
    // -----------------------------------------------------------------------
    void AwardXP(float amount, std::string_view source) {
        if (amount <= 0.0f) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            logger::warn("[EA] AwardXP: PlayerCharacter is null.");
            return;
        }

        auto* skills = player->GetInfoRuntimeData().skills;
        if (!skills || !skills->data) {
            logger::warn("[EA] AwardXP: PlayerSkills or data is null.");
            return;
        }

        s_currentXP += amount;
        skills->data->xp += amount;

        logger::info("[EA] XP +{:.1f} from '{}' | "
                     "engine xp={:.1f} / threshold={:.1f} | "
                     "GetLevel()={}",
            amount, source,
            skills->data->xp,
            skills->data->levelThreshold,
            static_cast<int>(player->GetLevel()));

        RE::DebugNotification(
            std::format("+{:.0f} XP ({})", amount, source).c_str());
    }
}
