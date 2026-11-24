// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/fox_amiga_rules.hpp"
namespace titus::fox_amiga {
// Fixed logical words preserve the original 92-byte state, including animation
// data addresses. Addresses identify bounded tables, never executable code.
struct EnemyBody {
    std::array<std::uint16_t,46> words{};
    unsigned get(unsigned offset)const;
    int signed_get(unsigned offset)const;
    void set(unsigned offset,int value);
    std::uint32_t animation()const;
    void animation(std::uint32_t address);
};
struct EnemyWorld { std::vector<EnemyBody> enemies;unsigned boss_power=0,hit_cooldown=0; };
EnemyWorld initialize_enemies(const Level&,const NativeRules&);
struct EnemyStepContext {
    unsigned frame=0,invulnerability=0,sound_event=0;
    Point camera;
    bool invulnerable=false;
    std::array<ObjectBody,4> projectiles{};
    std::vector<unsigned> attacks,visible_enemies;
};
// Updates original logical records. Rendering and collision resolution consume
// the resulting lists separately, preserving the original simulation order.
void step_enemies(const Level&,EnemyWorld&,Body&,EnemyStepContext&,const NativeRules&);
void step_enemy_projectiles(Body&,EnemyStepContext&,const NativeRules&);
void resolve_enemy_impacts(EnemyWorld&,ObjectWorld&,EnemyStepContext&);
Bytes dynamic_enemy_projection(const EnemyWorld&);
}
