// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "display_hdr.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi1_6.h>
#endif

namespace lucent {
namespace {

#if defined(_WIN32)
bool queryDxgiHdr() {
    IDXGIFactory1 *factory = nullptr;
    if (CreateDXGIFactory1(IID_PPV_ARGS(&factory)) != S_OK || factory == nullptr) {
        return false;
    }
    bool hdr = false;
    for (UINT adapterIndex = 0;; ++adapterIndex) {
        IDXGIAdapter1 *adapter = nullptr;
        if (factory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        for (UINT outputIndex = 0;; ++outputIndex) {
            IDXGIOutput *output = nullptr;
            if (adapter->EnumOutputs(outputIndex, &output) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            IDXGIOutput6 *output6 = nullptr;
            if (output->QueryInterface(IID_PPV_ARGS(&output6)) == S_OK && output6 != nullptr) {
                DXGI_OUTPUT_DESC1 desc{};
                if (output6->GetDesc1(&desc) == S_OK) {
                    const bool pq = desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
                    const bool scrgb = desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
                    hdr = hdr || pq || scrgb;
                }
                output6->Release();
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return hdr;
}
#endif

}  // namespace

bool displayPrefersHdr() {
#if defined(_WIN32)
    static const bool value = queryDxgiHdr();
    return value;
#else
    return false;
#endif
}

}  // namespace lucent
