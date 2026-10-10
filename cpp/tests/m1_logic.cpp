// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "natural.h"
#include "placement.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect(bool ok, const char *message) {
    if (!ok) {
        std::cerr << message << '\n';
        ++failures;
    }
}

bool near(double value, double expected) {
    return std::abs(value - expected) < 1e-6;
}

}  // namespace

int main() {
    expect(lucent::naturalCompare("img2", "img10") < 0, "img2 should sort before img10");
    expect(lucent::naturalCompare("img10", "img2") > 0, "img10 should sort after img2");
    expect(lucent::naturalCompare("B", "a") > 0, "natural order is case-insensitive");
    expect(lucent::naturalCompare("2", "02") < 0, "equal numbers keep the shorter digit run first");
    expect(lucent::naturalCompare("same", "same") == 0, "equal names compare equal");

    const lucent::Placement fit = lucent::placementFor(lucent::ZoomMode::Fit, 4000, 3000, 1600, 1000);
    expect(near(fit.scale, 1000.0 / 3000.0), "fit uses the limiting axis");
    expect(near(fit.originY, 0.0), "fit is vertically flush for this box");
    expect(near(fit.originX, -400.0), "fit letterboxes the wider axis");

    const lucent::Placement one = lucent::placementFor(lucent::ZoomMode::OneToOne, 100, 80, 1600, 1000);
    expect(near(one.scale, 1.0), "1:1 scale stays 1");

    const lucent::Placement zoomed = lucent::zoomAtCursor(lucent::Placement{1.0, 0.0, 0.0}, 2.0, 100.0, 100.0);
    expect(near(zoomed.scale, 2.0), "wheel doubles the scale");
    expect(near(zoomed.originX + 100.0 / zoomed.scale, 100.0), "the image point under the cursor stays put");
    expect(near(zoomed.originY + 100.0 / zoomed.scale, 100.0), "the image point under the cursor stays put");

    const lucent::Placement panned = lucent::panBy(lucent::Placement{2.0, 10.0, 4.0}, 20.0, -8.0);
    expect(near(panned.originX, 0.0), "pan moves the origin by view pixels over scale");
    expect(near(panned.originY, 8.0), "pan moves the origin by view pixels over scale");

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    return 0;
}
