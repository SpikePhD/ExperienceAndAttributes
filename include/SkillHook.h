#pragma once

namespace EA::SkillHook {
    // Installs the trampoline hook that silently discards all organic skill XP gain.
    // Must be called during SKSEPlugin_Load, after SKSE::Init(), on kDataLoaded.
    void Install();
}
