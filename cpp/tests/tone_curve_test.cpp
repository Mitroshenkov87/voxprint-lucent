// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "tone_curve.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool near(float value, float expected, float tol) {
    return std::fabs(value - expected) <= tol;
}

}  // namespace

int main() {
    if (!near(lucent::pqToNits(0.0F), 0.0F, 0.01F) || !near(lucent::pqToNits(1.0F), 10000.0F, 1.0F)) {
        std::cerr << "pq endpoints\n";
        return 1;
    }
    if (!near(lucent::pqToNits(0.5080784215F), 100.0F, 0.5F)) {
        std::cerr << "pq 100 nits\n";
        return 1;
    }
    const float paper = lucent::sdrShoulder(203.0F);
    const float spec = lucent::sdrShoulder(10000.0F);
    const float mid = lucent::sdrShoulder(1000.0F);
    if (!near(paper, 0.80F, 0.001F) || !near(spec, 1.0F, 0.001F) || !(mid > paper && mid < spec)) {
        std::cerr << "shoulder " << paper << " " << mid << " " << spec << "\n";
        return 1;
    }
    float r = 0.0F;
    float g = 203.0F;
    float b = 0.0F;
    const float y = lucent::bt2020Luma(r, g, b);
    const float scale = lucent::sdrShoulder(y) / y;
    r *= scale;
    g *= scale;
    b *= scale;
    lucent::bt2020ToSrgb(r, g, b);
    lucent::mapGamut(r, g, b, 1.0F);
    if (r < 0.0F || g < 0.0F || b < 0.0F || r > 1.0F || g > 1.0F || b > 1.0F || !(g > r && g > b)) {
        std::cerr << "gamut " << r << " " << g << " " << b << "\n";
        return 1;
    }
    return 0;
}
