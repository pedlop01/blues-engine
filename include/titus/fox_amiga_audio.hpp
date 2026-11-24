// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/fox_amiga.hpp"
#include <functional>
#include <string>
#include <memory>
namespace titus::fox_amiga {
struct PatInstrument {
    std::string name;
    Bytes samples;
    unsigned length_words=0,volume=0,loop_start=0,loop_words=0;
};
enum class PatDialect { fox,prehistorik };
struct PatSong {
    Bytes patterns;
    std::array<PatInstrument,15> instruments;
    std::vector<unsigned> order;
    unsigned initial_speed=10;
    PatDialect dialect=PatDialect::fox;
};
// PAT has a 20-byte title, 15 external sample descriptions and 128 orders.
// Sample files are raw PCM; the original loader reserves their first two bytes.
PatSong pat_song(const Bytes&,const std::function<Bytes(const std::string&)>& read_sample);
// Shared 15-instrument external-sample PAT container; callers validate title/revision.
PatSong external_pat_song(const Bytes&,const std::function<Bytes(const std::string&)>& read_sample);
struct EffectSample { Bytes samples;unsigned period=0,hold_ticks=0; };
EffectSample effect_sample(const Bytes& raw,unsigned original_frequency);
struct PatChannel {
    unsigned instrument=0,period=0,base_period=0,slide_period=0,volume=0,effect=0,parameter=0;
    unsigned sample_offset=0,sample_length=0,loop_offset=0,loop_length=0;
    std::uint64_t phase=0;
    bool active=false;
};
class PatPlayer {
public:
    explicit PatPlayer(PatSong song);
    void play_effect(std::shared_ptr<const EffectSample> effect);
    void volume(unsigned music,unsigned effects);
    unsigned effect_ticks()const{return effect_ticks_;}
    void render_into(std::int16_t* output,unsigned frames,unsigned sample_rate=44100);
    void tick(); // One original replay interrupt, independent of host audio blocks.
    const std::array<PatChannel,4>& channels()const{return channels_;}
    unsigned order()const{return order_;}
    unsigned row()const{return row_;} // Next row to be read.
    unsigned sub_tick()const{return tick_;}
    unsigned speed()const{return speed_;}
    unsigned loops()const{return loops_;}
    unsigned extended_arpeggios()const{return extended_arpeggios_;}
    // Stereo signed PCM at a chosen rate; fixed point Paula sample stepping.
    // Calls tick at 50 Hz, preserving the phase across arbitrary block sizes.
    std::vector<std::int16_t> render(unsigned frames,unsigned sample_rate=44100);
private:
    PatSong song_;
    std::array<PatChannel,4> channels_{};
    unsigned order_=0,row_=0,tick_=0,speed_=10,loops_=0,volume_override_=0;
    int pending_order_=-1;
    bool pending_break_=false;
    std::uint64_t interrupt_phase_=0;
    unsigned extended_arpeggios_=0;
    unsigned render_rate_=0,music_volume_=100,effect_volume_=100,effect_ticks_=0;
    std::shared_ptr<const EffectSample> effect_;
    std::uint64_t effect_phase_=0;
    void effect(PatChannel&);
    void advance_order();
};
}
