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

float gray(float nits) {
    const float signal = lucent::nitsToPq(nits);
    float r = signal;
    float g = signal;
    float b = signal;
    lucent::toneMapToSdrLinear(r, g, b, true);
    return r;
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
    if (!near(lucent::nitsToPq(100.0F), 0.5080784215F, 0.001F)
        || !near(lucent::nitsToPq(10000.0F), 1.0F, 0.001F)) {
        std::cerr << "nits to pq\n";
        return 1;
    }
    const float maxPq = lucent::nitsToPq(lucent::kSdrTargetNits);
    const float knee = 1.5F * maxPq - 0.5F;
    if (!(knee > 0.0F) || !near(lucent::bt2390Eetf(knee * 0.5F, maxPq), knee * 0.5F, 1.0e-5F)) {
        std::cerr << "knee " << knee << "\n";
        return 1;
    }
    if (!near(lucent::bt2390Eetf(1.0F, maxPq), maxPq, 1.0e-4F)) {
        std::cerr << "eetf peak " << lucent::bt2390Eetf(1.0F, maxPq) << "\n";
        return 1;
    }
    // A code above 1 must still move. Clamping the input to 1 would hide it.
    if (!(lucent::bt2390Eetf(1.05F, maxPq) != lucent::bt2390Eetf(1.0F, maxPq))) {
        std::cerr << "eetf clamped before the curve\n";
        return 1;
    }

    const float stops[] = {20.0F, 50.0F, 100.0F, 203.0F, 500.0F, 1000.0F, 2000.0F, 4000.0F, 10000.0F};
    float previous = -1.0F;
    for (float nits : stops) {
        const float code = gray(nits);
        if (!(code > previous)) {
            std::cerr << "plateau at " << nits << " code " << code << " previous " << previous << "\n";
            return 1;
        }
        previous = code;
    }
    const float paper = gray(203.0F);
    const float mid = gray(1000.0F);
    const float high = gray(4000.0F);
    const float peak = gray(10000.0F);
    if (!(mid - paper > 0.05F) || !(high - mid > 0.02F) || !near(peak, 1.0F, 0.02F)) {
        std::cerr << "shoulder " << paper << " " << mid << " " << high << " " << peak << "\n";
        return 1;
    }

    float r = 0.0F;
    float g = lucent::nitsToPq(203.0F);
    float b = 0.0F;
    lucent::toneMapToSdrLinear(r, g, b, true);
    lucent::bt2020ToSrgb(r, g, b);
    lucent::mapGamut(r, g, b, 1.0F);
    if (r < 0.0F || g < 0.0F || b < 0.0F || r > 1.0F || g > 1.0F || b > 1.0F || !(g > r && g > b)
        || (near(r, g, 0.02F) && near(g, b, 0.02F))) {
        std::cerr << "gamut " << r << " " << g << " " << b << "\n";
        return 1;
    }

    r = 5.0F;
    g = 4.0F;
    b = 0.2F;
    lucent::mapGamut(r, g, b, 1.0F);
    if (!(r > g && g > b) || near(r, b, 0.05F)) {
        std::cerr << "highlight flatten " << r << " " << g << " " << b << "\n";
        return 1;
    }
    return 0;
}
