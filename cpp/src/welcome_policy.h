// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace lucent {

struct WelcomeDecision {
    bool show = false;
    int storedCount = 0;
};

// Empty launches show the bundled welcome image on the first three, then stop.
// Turning the setting back on resets the stored count to zero.
inline WelcomeDecision decideWelcome(bool enabled, int storedCount) {
    if (!enabled || storedCount < 0) {
        return WelcomeDecision{false, storedCount < 0 ? 0 : storedCount};
    }
    if (storedCount < 3) {
        return WelcomeDecision{true, storedCount + 1};
    }
    return WelcomeDecision{false, storedCount};
}

inline int welcomeCountAfterReenable() {
    return 0;
}

}  // namespace lucent
