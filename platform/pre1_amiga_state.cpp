// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit field serialization for the isolated Amiga state; no ABI-dependent
// structs, pointer bytes or platform padding are stored.
#include "titus/pre1_amiga_save.hpp"
namespace olduvai::core {
void to_json(titus::Json &j, const Entity &v) {
  j = titus::Json::object();
  j["obj_type"] = v.obj_type;
  j["x"] = v.x;
  j["y"] = v.y;
  j["prev_x"] = v.prev_x;
  j["prev_y"] = v.prev_y;
  j["fx"] = v.fx;
  j["fy"] = v.fy;
  j["f_current_y"] = v.f_current_y;
  j["f_throw_x"] = v.f_throw_x;
  j["f_throw_y"] = v.f_throw_y;
  j["f_draw_dy"] = v.f_draw_dy;
  j["prev_current_y"] = v.prev_current_y;
  j["prev_throw_x"] = v.prev_throw_x;
  j["prev_throw_y"] = v.prev_throw_y;
  j["prev_draw_dy"] = v.prev_draw_dy;
  j["state"] = v.state;
  j["sprite"] = v.sprite;
  j["facing_left"] = v.facing_left;
  j["active"] = v.active;
  j["visible"] = v.visible;
  j["frame_counter"] = v.frame_counter;
  j["init_x"] = v.init_x;
  j["init_y"] = v.init_y;
  j["init_state"] = v.init_state;
  j["respawns"] = v.respawns;
  j["direction"] = v.direction;
  j["state_counter"] = v.state_counter;
  j["ko_counter"] = v.ko_counter;
  j["y_top"] = v.y_top;
  j["y_bottom"] = v.y_bottom;
  j["dy"] = v.dy;
  j["current_y"] = v.current_y;
  j["throw_flag"] = v.throw_flag;
  j["throw_x"] = v.throw_x;
  j["throw_y"] = v.throw_y;
  j["fireball_request"] = v.fireball_request;
  j["counter"] = v.counter;
  j["spr_num"] = v.spr_num;
  j["mask"] = v.mask;
  j["walk_offsets"] = v.walk_offsets;
  j["init_spr"] = v.init_spr;
  j["away_spr"] = v.away_spr;
  j["ko_spr"] = v.ko_spr;
  j["hits_to_ko"] = v.hits_to_ko;
  j["dat00"] = v.dat00;
  j["probe_di"] = v.probe_di;
  j["probe_si"] = v.probe_si;
  j["club_reach_right"] = v.club_reach_right;
  j["club_reach_left"] = v.club_reach_left;
  j["direction_flag"] = v.direction_flag;
  j["base_hits_to_ko"] = v.base_hits_to_ko;
  j["alt_hits_to_ko"] = v.alt_hits_to_ko;
  j["base_club_reach_right"] = v.base_club_reach_right;
  j["alt_club_reach_right"] = v.alt_club_reach_right;
  j["alt_spr_num"] = v.alt_spr_num;
  j["alt_away_spr"] = v.alt_away_spr;
  j["alt_walk_offsets"] = v.alt_walk_offsets;
  j["bird_heights"] = v.bird_heights;
  j["bird_height_idx"] = v.bird_height_idx;
  j["bird_anim"] = v.bird_anim;
  j["off_screen_left"] = v.off_screen_left;
  j["bird_spawn_x"] = v.bird_spawn_x;
  j["draw_dy"] = v.draw_dy;
  j["spider_di"] = v.spider_di;
  j["launcher_spr"] = v.launcher_spr;
  j["proj_x"] = v.proj_x;
  j["proj_y"] = v.proj_y;
  j["hit_w"] = v.hit_w;
  j["hit_h"] = v.hit_h;
  j["speed"] = v.speed;
  j["body_sprite"] = v.body_sprite;
  j["body_x"] = v.body_x;
  j["body_y"] = v.body_y;
  j["head_x"] = v.head_x;
  j["head_y"] = v.head_y;
  j["wing_x"] = v.wing_x;
  j["wing_phase"] = v.wing_phase;
  j["wing_subcounter"] = v.wing_subcounter;
  j["body_phase"] = v.body_phase;
  j["body_subcounter"] = v.body_subcounter;
  j["anchor_a"] = v.anchor_a;
  j["anchor_b"] = v.anchor_b;
  j["bonus_rise_dy"] = v.bonus_rise_dy;
  j["bonus_rise_y"] = v.bonus_rise_y;
  j["bonus_pending"] = v.bonus_pending;
}
void from_json(const titus::Json &j, Entity &v) {
  if (!j.is_object() || j.size() != 85)
    throw std::runtime_error("Invalid Entity save fields");
  j.at("obj_type").get_to(v.obj_type);
  j.at("x").get_to(v.x);
  j.at("y").get_to(v.y);
  j.at("prev_x").get_to(v.prev_x);
  j.at("prev_y").get_to(v.prev_y);
  j.at("fx").get_to(v.fx);
  j.at("fy").get_to(v.fy);
  j.at("f_current_y").get_to(v.f_current_y);
  j.at("f_throw_x").get_to(v.f_throw_x);
  j.at("f_throw_y").get_to(v.f_throw_y);
  j.at("f_draw_dy").get_to(v.f_draw_dy);
  j.at("prev_current_y").get_to(v.prev_current_y);
  j.at("prev_throw_x").get_to(v.prev_throw_x);
  j.at("prev_throw_y").get_to(v.prev_throw_y);
  j.at("prev_draw_dy").get_to(v.prev_draw_dy);
  j.at("state").get_to(v.state);
  j.at("sprite").get_to(v.sprite);
  j.at("facing_left").get_to(v.facing_left);
  j.at("active").get_to(v.active);
  j.at("visible").get_to(v.visible);
  j.at("frame_counter").get_to(v.frame_counter);
  j.at("init_x").get_to(v.init_x);
  j.at("init_y").get_to(v.init_y);
  j.at("init_state").get_to(v.init_state);
  j.at("respawns").get_to(v.respawns);
  j.at("direction").get_to(v.direction);
  j.at("state_counter").get_to(v.state_counter);
  j.at("ko_counter").get_to(v.ko_counter);
  j.at("y_top").get_to(v.y_top);
  j.at("y_bottom").get_to(v.y_bottom);
  j.at("dy").get_to(v.dy);
  j.at("current_y").get_to(v.current_y);
  j.at("throw_flag").get_to(v.throw_flag);
  j.at("throw_x").get_to(v.throw_x);
  j.at("throw_y").get_to(v.throw_y);
  j.at("fireball_request").get_to(v.fireball_request);
  j.at("counter").get_to(v.counter);
  j.at("spr_num").get_to(v.spr_num);
  j.at("mask").get_to(v.mask);
  j.at("walk_offsets").get_to(v.walk_offsets);
  j.at("init_spr").get_to(v.init_spr);
  j.at("away_spr").get_to(v.away_spr);
  j.at("ko_spr").get_to(v.ko_spr);
  j.at("hits_to_ko").get_to(v.hits_to_ko);
  j.at("dat00").get_to(v.dat00);
  j.at("probe_di").get_to(v.probe_di);
  j.at("probe_si").get_to(v.probe_si);
  j.at("club_reach_right").get_to(v.club_reach_right);
  j.at("club_reach_left").get_to(v.club_reach_left);
  j.at("direction_flag").get_to(v.direction_flag);
  j.at("base_hits_to_ko").get_to(v.base_hits_to_ko);
  j.at("alt_hits_to_ko").get_to(v.alt_hits_to_ko);
  j.at("base_club_reach_right").get_to(v.base_club_reach_right);
  j.at("alt_club_reach_right").get_to(v.alt_club_reach_right);
  j.at("alt_spr_num").get_to(v.alt_spr_num);
  j.at("alt_away_spr").get_to(v.alt_away_spr);
  j.at("alt_walk_offsets").get_to(v.alt_walk_offsets);
  j.at("bird_heights").get_to(v.bird_heights);
  j.at("bird_height_idx").get_to(v.bird_height_idx);
  j.at("bird_anim").get_to(v.bird_anim);
  j.at("off_screen_left").get_to(v.off_screen_left);
  j.at("bird_spawn_x").get_to(v.bird_spawn_x);
  j.at("draw_dy").get_to(v.draw_dy);
  j.at("spider_di").get_to(v.spider_di);
  j.at("launcher_spr").get_to(v.launcher_spr);
  j.at("proj_x").get_to(v.proj_x);
  j.at("proj_y").get_to(v.proj_y);
  j.at("hit_w").get_to(v.hit_w);
  j.at("hit_h").get_to(v.hit_h);
  j.at("speed").get_to(v.speed);
  j.at("body_sprite").get_to(v.body_sprite);
  j.at("body_x").get_to(v.body_x);
  j.at("body_y").get_to(v.body_y);
  j.at("head_x").get_to(v.head_x);
  j.at("head_y").get_to(v.head_y);
  j.at("wing_x").get_to(v.wing_x);
  j.at("wing_phase").get_to(v.wing_phase);
  j.at("wing_subcounter").get_to(v.wing_subcounter);
  j.at("body_phase").get_to(v.body_phase);
  j.at("body_subcounter").get_to(v.body_subcounter);
  j.at("anchor_a").get_to(v.anchor_a);
  j.at("anchor_b").get_to(v.anchor_b);
  j.at("bonus_rise_dy").get_to(v.bonus_rise_dy);
  j.at("bonus_rise_y").get_to(v.bonus_rise_y);
  j.at("bonus_pending").get_to(v.bonus_pending);
}
} // namespace olduvai::core
namespace olduvai::systems {
void to_json(titus::Json &j, const ScoreBonus &v) {
  j = titus::Json::object();
  j["counter"] = v.counter;
  j["x"] = v.x;
  j["y"] = v.y;
  j["sprite"] = v.sprite;
  j["prev_x"] = v.prev_x;
  j["prev_y"] = v.prev_y;
  j["fx"] = v.fx;
  j["fy"] = v.fy;
  j["active_this_frame"] = v.active_this_frame;
}
void from_json(const titus::Json &j, ScoreBonus &v) {
  if (!j.is_object() || j.size() != 9)
    throw std::runtime_error("Invalid ScoreBonus save fields");
  j.at("counter").get_to(v.counter);
  j.at("x").get_to(v.x);
  j.at("y").get_to(v.y);
  j.at("sprite").get_to(v.sprite);
  j.at("prev_x").get_to(v.prev_x);
  j.at("prev_y").get_to(v.prev_y);
  j.at("fx").get_to(v.fx);
  j.at("fy").get_to(v.fy);
  j.at("active_this_frame").get_to(v.active_this_frame);
}
void to_json(titus::Json &j, const PlayerState &v) {
  j = titus::Json::object();
  j["x"] = v.x;
  j["y"] = v.y;
  j["prev_x"] = v.prev_x;
  j["prev_y"] = v.prev_y;
  j["prev_dx"] = v.prev_dx;
  j["prev_dy"] = v.prev_dy;
  j["y_vel"] = v.y_vel;
  j["gravity_flag"] = v.gravity_flag;
  j["facing_left"] = v.facing_left;
  j["walk_frame"] = v.walk_frame;
  j["dx"] = v.dx;
  j["dy"] = v.dy;
  j["club_flag"] = v.club_flag;
  j["attack_latch"] = v.attack_latch;
  j["axe_flag"] = v.axe_flag;
  j["climbing"] = v.climbing;
  j["climb_y_min"] = v.climb_y_min;
  j["climb_y_max"] = v.climb_y_max;
  j["cave_warp_freeze"] = v.cave_warp_freeze;
  j["cave_warp_pending"] = v.cave_warp_pending;
  j["halo_spr"] = v.halo_spr;
  j["hit_counter"] = v.hit_counter;
  j["hit_blink"] = v.hit_blink;
  j["energy"] = v.energy;
  j["platform_flag"] = v.platform_flag;
  j["death_counter"] = v.death_counter;
  j["ghost_rise"] = v.ghost_rise;
  j["death_save_x"] = v.death_save_x;
  j["death_save_y"] = v.death_save_y;
  j["lives"] = v.lives;
  j["restart_x"] = v.restart_x;
  j["restart_y"] = v.restart_y;
  j["restart_screen"] = v.restart_screen;
  j["restart_cave_index"] = v.restart_cave_index;
  j["restart_secret_index"] = v.restart_secret_index;
  j["restart_glider"] = v.restart_glider;
  j["saved_y_vel"] = v.saved_y_vel;
  j["sprite"] = v.sprite;
}
void from_json(const titus::Json &j, PlayerState &v) {
  if (!j.is_object() || j.size() != 38)
    throw std::runtime_error("Invalid PlayerState save fields");
  j.at("x").get_to(v.x);
  j.at("y").get_to(v.y);
  j.at("prev_x").get_to(v.prev_x);
  j.at("prev_y").get_to(v.prev_y);
  j.at("prev_dx").get_to(v.prev_dx);
  j.at("prev_dy").get_to(v.prev_dy);
  j.at("y_vel").get_to(v.y_vel);
  j.at("gravity_flag").get_to(v.gravity_flag);
  j.at("facing_left").get_to(v.facing_left);
  j.at("walk_frame").get_to(v.walk_frame);
  j.at("dx").get_to(v.dx);
  j.at("dy").get_to(v.dy);
  j.at("club_flag").get_to(v.club_flag);
  j.at("attack_latch").get_to(v.attack_latch);
  j.at("axe_flag").get_to(v.axe_flag);
  j.at("climbing").get_to(v.climbing);
  j.at("climb_y_min").get_to(v.climb_y_min);
  j.at("climb_y_max").get_to(v.climb_y_max);
  j.at("cave_warp_freeze").get_to(v.cave_warp_freeze);
  j.at("cave_warp_pending").get_to(v.cave_warp_pending);
  j.at("halo_spr").get_to(v.halo_spr);
  j.at("hit_counter").get_to(v.hit_counter);
  j.at("hit_blink").get_to(v.hit_blink);
  j.at("energy").get_to(v.energy);
  j.at("platform_flag").get_to(v.platform_flag);
  j.at("death_counter").get_to(v.death_counter);
  j.at("ghost_rise").get_to(v.ghost_rise);
  j.at("death_save_x").get_to(v.death_save_x);
  j.at("death_save_y").get_to(v.death_save_y);
  j.at("lives").get_to(v.lives);
  j.at("restart_x").get_to(v.restart_x);
  j.at("restart_y").get_to(v.restart_y);
  j.at("restart_screen").get_to(v.restart_screen);
  j.at("restart_cave_index").get_to(v.restart_cave_index);
  j.at("restart_secret_index").get_to(v.restart_secret_index);
  j.at("restart_glider").get_to(v.restart_glider);
  j.at("saved_y_vel").get_to(v.saved_y_vel);
  j.at("sprite").get_to(v.sprite);
}
void to_json(titus::Json &j, const InputState &v) {
  j = titus::Json::object();
  j["left"] = v.left;
  j["right"] = v.right;
  j["jump"] = v.jump;
  j["down"] = v.down;
  j["attack"] = v.attack;
}
void from_json(const titus::Json &j, InputState &v) {
  if (!j.is_object() || j.size() != 5)
    throw std::runtime_error("Invalid InputState save fields");
  j.at("left").get_to(v.left);
  j.at("right").get_to(v.right);
  j.at("jump").get_to(v.jump);
  j.at("down").get_to(v.down);
  j.at("attack").get_to(v.attack);
}
void to_json(titus::Json &j, const SystemsState &v) {
  j = titus::Json::object();
  j["player"] = v.player;
  j["input"] = v.input;
  j["collision"] = v.collision;
  j["current_level"] = v.current_level;
  j["current_screen"] = v.current_screen;
  j["screen_height"] = v.screen_height;
  j["screen_change"] = v.screen_change;
  j["game_over"] = v.game_over;
  j["frame_counter"] = v.frame_counter;
  j["timer"] = v.timer;
  j["cave_flag"] = v.cave_flag;
  j["cave_index"] = v.cave_index;
  j["cave_return_screen"] = v.cave_return_screen;
  j["cave_return_x"] = v.cave_return_x;
  j["cave_return_y"] = v.cave_return_y;
  j["secret_flag"] = v.secret_flag;
  j["secret_index"] = v.secret_index;
  j["secret_return_screen"] = v.secret_return_screen;
  j["secret_return_x"] = v.secret_return_x;
  j["secret_return_y"] = v.secret_return_y;
  j["cave_entrance_mask"] = v.cave_entrance_mask;
  j["glider_active"] = v.glider_active;
  j["glider_x"] = v.glider_x;
  j["glider_y"] = v.glider_y;
  j["halo_flight_flag"] = v.halo_flight_flag;
  j["god_mode"] = v.god_mode;
  j["flash_frames"] = v.flash_frames;
  j["cave_emerge_frames"] = v.cave_emerge_frames;
  j["cave_descent_third_shown"] = v.cave_descent_third_shown;
  j["enhanced_active"] = v.enhanced_active;
  j["time_stopped"] = v.time_stopped;
  j["teleport_out_ticks"] = v.teleport_out_ticks;
  j["teleport_in_ticks"] = v.teleport_in_ticks;
  j["pending_sign_teleport"] = v.pending_sign_teleport;
  j["pending_tel_screen"] = v.pending_tel_screen;
  j["pending_tel_x"] = v.pending_tel_x;
  j["pending_tel_y"] = v.pending_tel_y;
  j["teleport_fx_x"] = v.teleport_fx_x;
  j["teleport_fx_y"] = v.teleport_fx_y;
  j["jump_apex_sfx_pending"] = v.jump_apex_sfx_pending;
  j["entities"] = v.entities;
  j["score"] = v.score;
  j["next_life_score"] = v.next_life_score;
  j["food_count"] = v.food_count;
  j["bonus_trigger"] = v.bonus_trigger;
  j["timer_counter"] = v.timer_counter;
  j["l3a_phase_counter"] = v.l3a_phase_counter;
  j["fireball_flag"] = v.fireball_flag;
  j["fireball_x"] = v.fireball_x;
  j["fireball_y"] = v.fireball_y;
  j["score_bonuses"] = v.score_bonuses;
  j["transition_skip"] = v.transition_skip;
  j["level_complete"] = v.level_complete;
  j["get_ready_counter"] = v.get_ready_counter;
  j["skip_player_update"] = v.skip_player_update;
  j["sfx_hit_pending"] = v.sfx_hit_pending;
  j["sfx_generic_pending"] = v.sfx_generic_pending;
  j["sfx_spring_pending"] = v.sfx_spring_pending;
  j["screen_clear_of_monsters"] = v.screen_clear_of_monsters;
  j["death_halo_active"] = v.death_halo_active;
  j["death_halo_x"] = v.death_halo_x;
  j["death_halo_y"] = v.death_halo_y;
  j["stone_state"] = v.stone_state;
  j["stone_x"] = v.stone_x;
  j["stone_y"] = v.stone_y;
  j["prev_stone_x"] = v.prev_stone_x;
  j["prev_stone_y"] = v.prev_stone_y;
  j["prev_fireball_x"] = v.prev_fireball_x;
  j["prev_fireball_y"] = v.prev_fireball_y;
  j["prev_glider_x"] = v.prev_glider_x;
  j["prev_glider_y"] = v.prev_glider_y;
  j["prev_death_halo_x"] = v.prev_death_halo_x;
  j["prev_death_halo_y"] = v.prev_death_halo_y;
  j["stone_fx"] = v.stone_fx;
  j["stone_fy"] = v.stone_fy;
  j["fireball_fx"] = v.fireball_fx;
  j["fireball_fy"] = v.fireball_fy;
  j["death_halo_fx"] = v.death_halo_fx;
  j["death_halo_fy"] = v.death_halo_fy;
  j["glider_fx"] = v.glider_fx;
  j["glider_fy"] = v.glider_fy;
  j["l3_descent_smoke_jitter"] = v.l3_descent_smoke_jitter;
  j["secret_spring_bouncing"] = v.secret_spring_bouncing;
  j["secret_exit_x"] = v.secret_exit_x;
}
void from_json(const titus::Json &j, SystemsState &v) {
  if (!j.is_object() || j.size() != 84)
    throw std::runtime_error("Invalid SystemsState save fields");
  j.at("player").get_to(v.player);
  j.at("input").get_to(v.input);
  j.at("collision").get_to(v.collision);
  j.at("current_level").get_to(v.current_level);
  j.at("current_screen").get_to(v.current_screen);
  j.at("screen_height").get_to(v.screen_height);
  j.at("screen_change").get_to(v.screen_change);
  j.at("game_over").get_to(v.game_over);
  j.at("frame_counter").get_to(v.frame_counter);
  j.at("timer").get_to(v.timer);
  j.at("cave_flag").get_to(v.cave_flag);
  j.at("cave_index").get_to(v.cave_index);
  j.at("cave_return_screen").get_to(v.cave_return_screen);
  j.at("cave_return_x").get_to(v.cave_return_x);
  j.at("cave_return_y").get_to(v.cave_return_y);
  j.at("secret_flag").get_to(v.secret_flag);
  j.at("secret_index").get_to(v.secret_index);
  j.at("secret_return_screen").get_to(v.secret_return_screen);
  j.at("secret_return_x").get_to(v.secret_return_x);
  j.at("secret_return_y").get_to(v.secret_return_y);
  j.at("cave_entrance_mask").get_to(v.cave_entrance_mask);
  j.at("glider_active").get_to(v.glider_active);
  j.at("glider_x").get_to(v.glider_x);
  j.at("glider_y").get_to(v.glider_y);
  j.at("halo_flight_flag").get_to(v.halo_flight_flag);
  j.at("god_mode").get_to(v.god_mode);
  j.at("flash_frames").get_to(v.flash_frames);
  j.at("cave_emerge_frames").get_to(v.cave_emerge_frames);
  j.at("cave_descent_third_shown").get_to(v.cave_descent_third_shown);
  j.at("enhanced_active").get_to(v.enhanced_active);
  j.at("time_stopped").get_to(v.time_stopped);
  j.at("teleport_out_ticks").get_to(v.teleport_out_ticks);
  j.at("teleport_in_ticks").get_to(v.teleport_in_ticks);
  j.at("pending_sign_teleport").get_to(v.pending_sign_teleport);
  j.at("pending_tel_screen").get_to(v.pending_tel_screen);
  j.at("pending_tel_x").get_to(v.pending_tel_x);
  j.at("pending_tel_y").get_to(v.pending_tel_y);
  j.at("teleport_fx_x").get_to(v.teleport_fx_x);
  j.at("teleport_fx_y").get_to(v.teleport_fx_y);
  j.at("jump_apex_sfx_pending").get_to(v.jump_apex_sfx_pending);
  j.at("entities").get_to(v.entities);
  j.at("score").get_to(v.score);
  j.at("next_life_score").get_to(v.next_life_score);
  j.at("food_count").get_to(v.food_count);
  j.at("bonus_trigger").get_to(v.bonus_trigger);
  j.at("timer_counter").get_to(v.timer_counter);
  j.at("l3a_phase_counter").get_to(v.l3a_phase_counter);
  j.at("fireball_flag").get_to(v.fireball_flag);
  j.at("fireball_x").get_to(v.fireball_x);
  j.at("fireball_y").get_to(v.fireball_y);
  j.at("score_bonuses").get_to(v.score_bonuses);
  j.at("transition_skip").get_to(v.transition_skip);
  j.at("level_complete").get_to(v.level_complete);
  j.at("get_ready_counter").get_to(v.get_ready_counter);
  j.at("skip_player_update").get_to(v.skip_player_update);
  j.at("sfx_hit_pending").get_to(v.sfx_hit_pending);
  j.at("sfx_generic_pending").get_to(v.sfx_generic_pending);
  j.at("sfx_spring_pending").get_to(v.sfx_spring_pending);
  j.at("screen_clear_of_monsters").get_to(v.screen_clear_of_monsters);
  j.at("death_halo_active").get_to(v.death_halo_active);
  j.at("death_halo_x").get_to(v.death_halo_x);
  j.at("death_halo_y").get_to(v.death_halo_y);
  j.at("stone_state").get_to(v.stone_state);
  j.at("stone_x").get_to(v.stone_x);
  j.at("stone_y").get_to(v.stone_y);
  j.at("prev_stone_x").get_to(v.prev_stone_x);
  j.at("prev_stone_y").get_to(v.prev_stone_y);
  j.at("prev_fireball_x").get_to(v.prev_fireball_x);
  j.at("prev_fireball_y").get_to(v.prev_fireball_y);
  j.at("prev_glider_x").get_to(v.prev_glider_x);
  j.at("prev_glider_y").get_to(v.prev_glider_y);
  j.at("prev_death_halo_x").get_to(v.prev_death_halo_x);
  j.at("prev_death_halo_y").get_to(v.prev_death_halo_y);
  j.at("stone_fx").get_to(v.stone_fx);
  j.at("stone_fy").get_to(v.stone_fy);
  j.at("fireball_fx").get_to(v.fireball_fx);
  j.at("fireball_fy").get_to(v.fireball_fy);
  j.at("death_halo_fx").get_to(v.death_halo_fx);
  j.at("death_halo_fy").get_to(v.death_halo_fy);
  j.at("glider_fx").get_to(v.glider_fx);
  j.at("glider_fy").get_to(v.glider_fy);
  j.at("l3_descent_smoke_jitter").get_to(v.l3_descent_smoke_jitter);
  j.at("secret_spring_bouncing").get_to(v.secret_spring_bouncing);
  j.at("secret_exit_x").get_to(v.secret_exit_x);
}
} // namespace olduvai::systems
namespace olduvai::systems {
void to_json(titus::Json &j, const BossPlayerState &v) {
  j = titus::Json::object();
  j["x"] = v.x;
  j["y"] = v.y;
  j["prev_x"] = v.prev_x;
  j["prev_y"] = v.prev_y;
  j["facing_left"] = v.facing_left;
  j["walk_frame"] = v.walk_frame;
  j["jump_counter"] = v.jump_counter;
  j["jump_peak"] = v.jump_peak;
  j["attack_latch"] = v.attack_latch;
  j["fire_held"] = v.fire_held;
  j["club_flag"] = v.club_flag;
  j["death_counter"] = v.death_counter;
  j["dead_counter"] = v.dead_counter;
  j["dead_anim_y"] = v.dead_anim_y;
  j["dead_x_save"] = v.dead_x_save;
  j["respawn_x"] = v.respawn_x;
  j["respawn_y"] = v.respawn_y;
  j["halo_flag"] = v.halo_flag;
  j["hit_counter"] = v.hit_counter;
  j["halo_toggle"] = v.halo_toggle;
  j["knockback_stun"] = v.knockback_stun;
  j["timer"] = v.timer;
  j["lives"] = v.lives;
  j["score"] = v.score;
  j["sprite"] = v.sprite;
  j["sprite_dx"] = v.sprite_dx;
  j["sprite_dy"] = v.sprite_dy;
  j["club_spr"] = v.club_spr;
  j["club_dx"] = v.club_dx;
  j["club_dy"] = v.club_dy;
  j["jump_apex_sfx_pending"] = v.jump_apex_sfx_pending;
}
void from_json(const titus::Json &j, BossPlayerState &v) {
  if (!j.is_object() || j.size() != 31)
    throw std::runtime_error("Invalid BossPlayerState save fields");
  j.at("x").get_to(v.x);
  j.at("y").get_to(v.y);
  j.at("prev_x").get_to(v.prev_x);
  j.at("prev_y").get_to(v.prev_y);
  j.at("facing_left").get_to(v.facing_left);
  j.at("walk_frame").get_to(v.walk_frame);
  j.at("jump_counter").get_to(v.jump_counter);
  j.at("jump_peak").get_to(v.jump_peak);
  j.at("attack_latch").get_to(v.attack_latch);
  j.at("fire_held").get_to(v.fire_held);
  j.at("club_flag").get_to(v.club_flag);
  j.at("death_counter").get_to(v.death_counter);
  j.at("dead_counter").get_to(v.dead_counter);
  j.at("dead_anim_y").get_to(v.dead_anim_y);
  j.at("dead_x_save").get_to(v.dead_x_save);
  j.at("respawn_x").get_to(v.respawn_x);
  j.at("respawn_y").get_to(v.respawn_y);
  j.at("halo_flag").get_to(v.halo_flag);
  j.at("hit_counter").get_to(v.hit_counter);
  j.at("halo_toggle").get_to(v.halo_toggle);
  j.at("knockback_stun").get_to(v.knockback_stun);
  j.at("timer").get_to(v.timer);
  j.at("lives").get_to(v.lives);
  j.at("score").get_to(v.score);
  j.at("sprite").get_to(v.sprite);
  j.at("sprite_dx").get_to(v.sprite_dx);
  j.at("sprite_dy").get_to(v.sprite_dy);
  j.at("club_spr").get_to(v.club_spr);
  j.at("club_dx").get_to(v.club_dx);
  j.at("club_dy").get_to(v.club_dy);
  j.at("jump_apex_sfx_pending").get_to(v.jump_apex_sfx_pending);
}
} // namespace olduvai::systems
namespace olduvai::systems {
void to_json(titus::Json &j, const L2ProjectileSlot &v) {
  j = titus::Json::object();
  j["ptype"] = v.ptype;
  j["direction"] = v.direction;
  j["x"] = v.x;
  j["frame"] = v.frame;
  j["prev_x"] = v.prev_x;
  j["fx"] = v.fx;
}
void from_json(const titus::Json &j, L2ProjectileSlot &v) {
  if (!j.is_object() || j.size() != 6)
    throw std::runtime_error("Invalid L2ProjectileSlot save fields");
  j.at("ptype").get_to(v.ptype);
  j.at("direction").get_to(v.direction);
  j.at("x").get_to(v.x);
  j.at("frame").get_to(v.frame);
  j.at("prev_x").get_to(v.prev_x);
  j.at("fx").get_to(v.fx);
}
void to_json(titus::Json &j, const L2BossState &v) {
  j = titus::Json::object();
  j["health"] = v.health;
  j["jaw_state"] = v.jaw_state;
  j["stomp_timer"] = v.stomp_timer;
  j["arm_frame"] = v.arm_frame;
  j["win_flag"] = v.win_flag;
  j["slots"] = v.slots;
  j["cooldown"] = v.cooldown;
  j["sfx_hit_pending"] = v.sfx_hit_pending;
  j["health_column_pending"] = v.health_column_pending;
}
void from_json(const titus::Json &j, L2BossState &v) {
  if (!j.is_object() || j.size() != 9)
    throw std::runtime_error("Invalid L2BossState save fields");
  j.at("health").get_to(v.health);
  j.at("jaw_state").get_to(v.jaw_state);
  j.at("stomp_timer").get_to(v.stomp_timer);
  j.at("arm_frame").get_to(v.arm_frame);
  j.at("win_flag").get_to(v.win_flag);
  j.at("slots").get_to(v.slots);
  j.at("cooldown").get_to(v.cooldown);
  j.at("sfx_hit_pending").get_to(v.sfx_hit_pending);
  j.at("health_column_pending").get_to(v.health_column_pending);
}
} // namespace olduvai::systems
namespace olduvai::systems {
void to_json(titus::Json &j, const L4BossState &v) {
  j = titus::Json::object();
  j["health"] = v.health;
  j["boss_x"] = v.boss_x;
  j["boss_y"] = v.boss_y;
  j["prev_x"] = v.prev_x;
  j["prev_y"] = v.prev_y;
  j["fx"] = v.fx;
  j["fy"] = v.fy;
  j["walk_frame"] = v.walk_frame;
  j["direction"] = v.direction;
  j["stun_counter"] = v.stun_counter;
  j["hit_flash"] = v.hit_flash;
  j["spring_state"] = v.spring_state;
  j["win_flag"] = v.win_flag;
  j["sfx_hit_pending"] = v.sfx_hit_pending;
}
void from_json(const titus::Json &j, L4BossState &v) {
  if (!j.is_object() || j.size() != 14)
    throw std::runtime_error("Invalid L4BossState save fields");
  j.at("health").get_to(v.health);
  j.at("boss_x").get_to(v.boss_x);
  j.at("boss_y").get_to(v.boss_y);
  j.at("prev_x").get_to(v.prev_x);
  j.at("prev_y").get_to(v.prev_y);
  j.at("fx").get_to(v.fx);
  j.at("fy").get_to(v.fy);
  j.at("walk_frame").get_to(v.walk_frame);
  j.at("direction").get_to(v.direction);
  j.at("stun_counter").get_to(v.stun_counter);
  j.at("hit_flash").get_to(v.hit_flash);
  j.at("spring_state").get_to(v.spring_state);
  j.at("win_flag").get_to(v.win_flag);
  j.at("sfx_hit_pending").get_to(v.sfx_hit_pending);
}
} // namespace olduvai::systems
namespace olduvai::systems {
void to_json(titus::Json &j, const L6BossState &v) {
  j = titus::Json::object();
  j["health"] = v.health;
  j["frame_counter"] = v.frame_counter;
  j["ground_punch_state"] = v.ground_punch_state;
  j["win_flag"] = v.win_flag;
  j["win_counter"] = v.win_counter;
  j["cycle_idx"] = v.cycle_idx;
  j["cycle_tick"] = v.cycle_tick;
  j["hit_reaction_counter"] = v.hit_reaction_counter;
  j["slam_slow_tick"] = v.slam_slow_tick;
  j["sfx_hit_pending"] = v.sfx_hit_pending;
}
void from_json(const titus::Json &j, L6BossState &v) {
  if (!j.is_object() || j.size() != 10)
    throw std::runtime_error("Invalid L6BossState save fields");
  j.at("health").get_to(v.health);
  j.at("frame_counter").get_to(v.frame_counter);
  j.at("ground_punch_state").get_to(v.ground_punch_state);
  j.at("win_flag").get_to(v.win_flag);
  j.at("win_counter").get_to(v.win_counter);
  j.at("cycle_idx").get_to(v.cycle_idx);
  j.at("cycle_tick").get_to(v.cycle_tick);
  j.at("hit_reaction_counter").get_to(v.hit_reaction_counter);
  j.at("slam_slow_tick").get_to(v.slam_slow_tick);
  j.at("sfx_hit_pending").get_to(v.sfx_hit_pending);
}
} // namespace olduvai::systems
