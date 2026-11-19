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

/// Inverse of `pqToNits`. Nits are not clamped to 10000, so a value past the
/// PQ peak stays above 1 instead of being flattened first.
inline float nitsToPq(float nits) {
    if (!(nits > 0.0F)) {
        return 0.0F;
    }
    constexpr float m1 = 0.1593017578125F;
    constexpr float m2 = 78.84375F;
    constexpr float c1 = 0.8359375F;
    constexpr float c2 = 18.8515625F;
    constexpr float c3 = 18.6875F;
    const float yp = std::pow(nits / 10000.0F, m1);
    return std::pow((c1 + c2 * yp) / (1.0F + c3 * yp), m2);
}

/// SDR desktop peak used by the BT.2390 roll-off, in nits.
///
/// PQ(100) is about 0.508, so the knee `1.5 * maxPq - 0.5` stays positive.
/// A 203 nit target puts diffuse white on the shoulder and piles the chart
/// into one code.
inline constexpr float kSdrTargetNits = 100.0F;

/// BT.2390 EETF in PQ. `signal` and `maxLum` are PQ codes (1.0 is 10000 nits).
///
/// The hermite runs from the knee to 1 and lands on `maxLum` with a zero
/// slope, so the highlight ramp bends instead of cutting off. `signal` is not
/// clamped to 1 before that polynomial.
inline float bt2390Eetf(float signal, float maxLum) {
    const float lw = std::clamp(maxLum, 0.0F, 1.0F);
    if (!(signal > 0.0F) || !(lw > 0.0F)) {
        return 0.0F;
    }
    const float ks = 1.5F * lw - 0.5F;
    if (ks >= 1.0F || signal < ks) {
        return signal;
    }
    const float span = 1.0F - ks;
    const float t = (signal - ks) / span;
    const float t2 = t * t;
    const float t3 = t2 * t;
    return (2.0F * t3 - 3.0F * t2 + 1.0F) * ks + (t3 - 2.0F * t2 + t) * span
        + (-2.0F * t3 + 3.0F * t2) * lw;
}

/// PQ or HLG channels to linear light relative to `kSdrTargetNits`.
///
/// The curve is applied to max(R, G, B) in PQ and the same ratio scales the
/// other two channels, which keeps the hue. Nothing is clamped to the display
/// peak before the EETF.
inline void toneMapToSdrLinear(float &r, float &g, float &b, bool pq) {
    if (!pq) {
        const float sceneY = std::max(bt2020Luma(r, g, b), 1.0e-6F);
        r = nitsToPq(hlgToNits(r, sceneY));
        g = nitsToPq(hlgToNits(g, sceneY));
        b = nitsToPq(hlgToNits(b, sceneY));
    }
    const float maxPq = nitsToPq(kSdrTargetNits);
    const float peak = std::max(r, std::max(g, b));
    if (!(peak > 0.0F)) {
        r = 0.0F;
        g = 0.0F;
        b = 0.0F;
        return;
    }
    const float scale = bt2390Eetf(peak, maxPq) / peak;
    r = pqToNits(r * scale) / kSdrTargetNits;
    g = pqToNits(g * scale) / kSdrTargetNits;
    b = pqToNits(b * scale) / kSdrTargetNits;
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
        const float top = std::max(r, std::max(g, b));
        if (top > hi && top > 0.0F) {
            const float s = hi / top;
            r *= s;
            g *= s;
            b *= s;
        }
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
