// Modified for native Prehistorik Amiga by Titus Engine contributors, 2026-11-24.
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Krzysztof Sokołowski
// Amiga adaptation: bounded 32-bit Galois generator, independently compared
// with the original C5494 routine. The DOS kernel keeps its separate LCG.

#pragma once

#include <cstdint>

namespace olduvai::core {

class RandLcg16 {
public:
    explicit RandLcg16(std::uint32_t seed = 0u) : state_(seed) {}

    std::uint16_t bounded(unsigned bound) {
        bound &= 65535;
        unsigned iterations=(bound-1)&65535;
        do {
            const bool high=(state_&0x80000000u)!=0;
            state_ <<= 1;
            if(high || !state_)state_^=0x1d872b41u;
            iterations >>= 1;
        } while(iterations);
        return static_cast<std::uint16_t>(bound?((state_&65535u)*bound)>>16:state_&65535u);
    }
    std::uint16_t next() { return bounded(0); }

    std::uint32_t state() const { return state_; }
    void reseed(std::uint32_t s) { state_ = s; }

private:
    std::uint32_t state_;
};

// The single shared Amiga generator (the original keeps one global state that all
// call sites advance).  Callers that mirror an original Rand_LCG16 call
// site must use this instance so the shared sequence is consumed in order.
RandLcg16& global_rng();

}  // namespace olduvai::core
