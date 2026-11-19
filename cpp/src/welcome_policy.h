// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace lucent {

/// Whether this empty launch shows the welcome image, and the count to store.
struct WelcomeDecision {
    /// True when the welcome image should be shown.
    bool show = false;
    /// Count written back to settings after this decision.
    int storedCount = 0;
};

/// Empty launches show the bundled welcome image on the first three, then stop.
///
/// Turning the setting back on resets the stored count to zero.
inline WelcomeDecision decideWelcome(bool enabled, int storedCount) {
    if (!enabled || storedCount < 0) {
        return WelcomeDecision{false, storedCount < 0 ? 0 : storedCount};
    }
    if (storedCount < 3) {
        return WelcomeDecision{true, storedCount + 1};
    }
    return WelcomeDecision{false, storedCount};
}

/// Count stored when the user turns the welcome image back on.
inline int welcomeCountAfterReenable() {
    return 0;
}

}  // namespace lucent
