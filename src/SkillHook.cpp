#include "PCH.h"
#include "SkillHook.h"
#include "Config.h"

namespace EA::SkillHook {

    // -----------------------------------------------------------------------
    // ADDRESS VERIFICATION
    // Source: CommonLibSSE-NG submodule, src/RE/P/PlayerCharacter.cpp
    //   PlayerCharacter::AddSkillExperience -> RELOCATION_ID(39413, 40488)
    //   SE ID: 39413  |  AE ID: 40488  (we target AE 1.6.1170, so AE ID is used)
    //
    // This is the function entry point (non-virtual, resolved via REL::Relocation).
    // Hook method: write_branch<5>  (patches the function prologue, not a call site)
    //
    // Signature in CommonLibSSE-NG:
    //   void PlayerCharacter::AddSkillExperience(RE::ActorValue a_skill, float a_experience)
    // -----------------------------------------------------------------------

    struct AddSkillExperienceHook {

        static void thunk(
            RE::PlayerCharacter* a_player,
            RE::ActorValue       a_skill,
            float                a_experience)
        {
            // EA: Discard all organic skill XP. Do nothing and return.
            // Skills can still be raised via console/trainers (those paths bypass this function).
            if (EA::Config::verbose) {
                logger::trace("[EA] SkillHook: Intercepted skill XP gain. Skill={}, Points={:.2f} -> Discarded.",
                    static_cast<std::uint32_t>(a_skill), a_experience);
            }
        }

        // Stores the original function pointer.
        // Required by the trampoline pattern even though we never call the original.
        static inline REL::Relocation<decltype(thunk)> func;
    };

    void Install() {
        // AE Address ID 40488 = PlayerCharacter::AddSkillExperience
        // Verified via CommonLibSSE-NG: RELOCATION_ID(39413, 40488)
        REL::Relocation<std::uintptr_t> target{ REL::ID(40488) };

        auto& trampoline = SKSE::GetTrampoline();
        trampoline.create(64);

        // write_branch<5>: hooks the function entry point (5-byte prologue patch).
        // Not write_call<5> because this is the function itself, not a call site.
        AddSkillExperienceHook::func =
            trampoline.write_branch<5>(target.address(), AddSkillExperienceHook::thunk);

        logger::info("[EA] SkillHook: Hook installed successfully at address ID 40488 (AddSkillExperience).");
    }
}
