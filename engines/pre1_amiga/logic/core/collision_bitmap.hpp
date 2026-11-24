// Modified for native Prehistorik Amiga by Titus Engine contributors, 2026-11-24.
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Krzysztof Sokołowski
// 320x200 binary collision bitmap (40 bytes/row, MSB-first), the ground/
// wall probe surface for player and monster logic.
//
// Semantics mirror the original routines exactly, including the asymmetry:
//   set_bit DISCARDS out-of-range coordinates,          // FUN_27f7_003b
//   test CLAMPS x to 0..319 and y to 199 — a negative y
//   behaves as 199 via the unsigned 16-bit row compare. // FUN_27f7_135e
// stamp_tile writes DUR segments with a horizontal
// 0 <= x < 320 check per pixel.                          // FUN_27f7_006c

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "formats/dur.hpp"

namespace olduvai::core {

class CollisionBitmap {
public:
    static constexpr int kWidth = 320;
    static constexpr int kHeight = 200;

    void clear() { data_.fill(0); has_left_=has_right_=false; }
    const std::array<std::uint8_t,8000>& bytes() const { return data_; }
    void restore(const std::array<std::uint8_t,8000>& bytes) { data_=bytes; has_left_=has_right_=false; }

    void neighbors(const std::array<std::uint8_t,8000>* left,const std::array<std::uint8_t,8000>* right){
        has_left_=left!=nullptr;has_right_=right!=nullptr;
        for(unsigned y=0;y<200;++y)for(unsigned b=0;b<4;++b){if(left)left_[y*4+b]=(*left)[y*40+36+b];if(right)right_[y*4+b]=(*right)[y*40+b];}
    }
    void set_bit(int x, int y) {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
        data_[static_cast<std::size_t>(y) * 40 + (x >> 3)] |=
            static_cast<std::uint8_t>(0x80u >> (x & 7));
    }

    // True = empty (walkable), false = solid.
    bool test(int x, int y) const {
        if(y<0 || y>199)y=199;
        if(x<0 && x>=-32 && has_left_)return !(left_[y*4+((x+32)>>3)]&(0x80u>>((x+32)&7)));
        if(x>=320 && x<352 && has_right_)return !(right_[y*4+((x-320)>>3)]&(0x80u>>((x-320)&7)));
        if (x < 0) x = 0;
        else if (x > 319) x = 319;
        // Both arms land on 199 for DIFFERENT reasons, and keeping them apart
        // is the point: below is the original's unsigned 16-bit compare quirk
        // (a negative y reads as a huge value and falls out the top), above is
        // an ordinary clamp. Compare the x pair, which clamps to 0 and 319 —
        // merging these two would hide that y is asymmetric where x is not,
        // and the asymmetry is the faithfulness. Pinned by the oracle diff.
        // NOLINTNEXTLINE(bugprone-branch-clone)
        if (y < 0) y = 199;   // unsigned 16-bit compare quirk: negative → 199
        else if (y > 199) y = 199;
        const std::uint8_t b =
            data_[static_cast<std::size_t>(y) * 40 + (x >> 3)];
        return (b & (0x80u >> (x & 7))) == 0;
    }

    void stamp_tile(const std::vector<formats::CollisionSegment>& segments,
                    int x, int y) {
        for (const auto& seg : segments) {
            int sx = seg.dx + x;
            const int sy = seg.dy + y;
            for (int i = 0; i < seg.width; ++i, ++sx) {
                if (sx >= 0 && sx < kWidth) set_bit(sx, sy);
            }
        }
    }

private:
    std::array<std::uint8_t, 8000> data_{};
    std::array<std::uint8_t,800> left_{},right_{};
    bool has_left_=false,has_right_=false;
};

}  // namespace olduvai::core
