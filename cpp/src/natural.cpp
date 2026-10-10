// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "natural.h"

#include <cctype>

namespace lucent {
namespace {

char foldAscii(unsigned char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return static_cast<char>(c);
}

struct Number {
    std::string_view value;
    size_t digits = 0;
};

Number readNumber(std::string_view text, size_t &index) {
    const size_t start = index;
    while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
        ++index;
    }
    const std::string_view digits = text.substr(start, index - start);
    size_t significant = 0;
    while (significant < digits.size() && digits[significant] == '0') {
        ++significant;
    }
    return Number{digits.substr(significant), digits.size()};
}

int compareNumber(const Number &left, const Number &right) {
    if (left.value.size() != right.value.size()) {
        return left.value.size() < right.value.size() ? -1 : 1;
    }
    if (left.value != right.value) {
        return left.value < right.value ? -1 : 1;
    }
    if (left.digits != right.digits) {
        return left.digits < right.digits ? -1 : 1;
    }
    return 0;
}

}  // namespace

int naturalCompare(std::string_view left, std::string_view right) {
    size_t i = 0;
    size_t j = 0;
    while (i < left.size() && j < right.size()) {
        const bool leftDigit = std::isdigit(static_cast<unsigned char>(left[i])) != 0;
        const bool rightDigit = std::isdigit(static_cast<unsigned char>(right[j])) != 0;
        if (leftDigit && rightDigit) {
            const int compared = compareNumber(readNumber(left, i), readNumber(right, j));
            if (compared != 0) {
                return compared;
            }
            continue;
        }
        const char a = foldAscii(static_cast<unsigned char>(left[i]));
        const char b = foldAscii(static_cast<unsigned char>(right[j]));
        if (a != b) {
            return static_cast<unsigned char>(a) < static_cast<unsigned char>(b) ? -1 : 1;
        }
        ++i;
        ++j;
    }
    if (i == left.size() && j == right.size()) {
        return 0;
    }
    return i == left.size() ? -1 : 1;
}

}  // namespace lucent
