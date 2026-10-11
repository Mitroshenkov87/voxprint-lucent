// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cmath>

namespace lucent {

/// PQ (ST 2084) signal in 0..1 to luminance in nits. 1.0 is 10000 nits.
inline float pqToNits(float signal) {
    const float v = std::clamp(signal, 0.0F, 1.0F);
    constexpr float m1 = 0.1593017578125F;
    constexpr float m2 = 78.84375F;
    constexpr float c1 = 0.8359375F;
    constexpr float c2 = 18.8515625F;
    constexpr float c3 = 18.6875F;
    const float vp = std::pow(v, 1.0F / m2);
    const float num = std::max(vp - c1, 0.0F);
    const float den = c2 - c3 * vp;
    if (den <= 0.0F) {
        return 0.0F;
    }
    return std::pow(num / den, 1.0F / m1) * 10000.0F;
}

/// HLG signal in 0..1 to display luminance in nits, assuming a 1000 nit peak.
inline float hlgToNits(float signal, float sceneLuma) {
    const float e = std::clamp(signal, 0.0F, 1.0F);
    constexpr float a = 0.17883277F;
    constexpr float b = 0.28466892F;
    constexpr float c = 0.55991073F;
    const float scene = e <= 0.5F ? (e * e) / 3.0F : (std::exp((e - c) / a) + b) / 12.0F;
    constexpr float peak = 1000.0F;
    constexpr float gamma = 1.2F;
    const float y = std::max(sceneLuma, 1.0e-6F);
    return scene * std::pow(y, gamma - 1.0F) * peak;
}

/// BT.2020 luminance in nits.
inline float bt2020Luma(float r, float g, float b) {
    return 0.2627F * r + 0.6780F * g + 0.0593F * b;
}

/// Map luminance so 203 nits (diffuse white) lands at 0.8 and 10000 nits at 1.
///
/// The roll-off is a smooth shoulder above diffuse white. BT.2390's hermite
/// EETF is for a target brighter than about 3333 nits, so an SDR desktop uses
/// this shoulder instead of clipping the highlights.
inline float sdrShoulder(float nits) {
    constexpr float paper = 203.0F;
    constexpr float peak = 10000.0F;
    constexpr float diffuseOut = 0.80F;
    if (nits <= paper) {
        return std::max(nits, 0.0F) / paper * diffuseOut;
    }
    const float h = std::clamp((nits - paper) / (peak - paper), 0.0F, 1.0F);
    // Square root, not a flat hermite start, so a 1000 nit highlight is not
    // the same code as diffuse white.
    const float s = std::sqrt(h);
    return diffuseOut + (1.0F - diffuseOut) * s;
}

/// Linear BT.2020 to linear sRGB, same scale (nits or relative).
inline void bt2020ToSrgb(float &r, float &g, float &b) {
    const float rr = 1.6604910021F * r - 0.5876411388F * g - 0.0728498633F * b;
    const float gg = -0.1245504745F * r + 1.1328998971F * g - 0.0083494226F * b;
    const float bb = -0.0181507634F * r - 0.1005788980F * g + 1.1187296614F * b;
    r = rr;
    g = gg;
    b = bb;
}

/// Pull chroma toward luminance until every channel is inside 0..`hi`.
inline void mapGamut(float &r, float &g, float &b, float hi) {
    const float y = 0.2126F * r + 0.7152F * g + 0.0722F * b;
    if (y <= 0.0F) {
        r = 0.0F;
        g = 0.0F;
        b = 0.0F;
        return;
    }
    if (y >= hi) {
        r = hi;
        g = hi;
        b = hi;
        return;
    }
    const float lo = std::min(r, std::min(g, b));
    const float top = std::max(r, std::max(g, b));
    float scale = 1.0F;
    if (lo < 0.0F && y > lo) {
        scale = std::min(scale, y / (y - lo));
    }
    if (top > hi && top > y) {
        scale = std::min(scale, (hi - y) / (top - y));
    }
    r = y + (r - y) * scale;
    g = y + (g - y) * scale;
    b = y + (b - y) * scale;
}

/// Linear 0..1 to sRGB 0..1.
inline float srgbEncode(float linear) {
    const float c = std::clamp(linear, 0.0F, 1.0F);
    if (c <= 0.0031308F) {
        return 12.92F * c;
    }
    return 1.055F * std::pow(c, 1.0F / 2.4F) - 0.055F;
}

/// sRGB 0..1 to linear 0..1.
inline float srgbDecode(float encoded) {
    const float c = std::clamp(encoded, 0.0F, 1.0F);
    if (c <= 0.04045F) {
        return c / 12.92F;
    }
    return std::pow((c + 0.055F) / 1.055F, 2.4F);
}

}  // namespace lucent
