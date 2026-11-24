// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_audio.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>
namespace titus::fox_amiga {
namespace {
unsigned word(const Bytes& b,std::size_t p){if(p+2>b.size())throw std::runtime_error("Truncated PAT word");return unsigned(b[p])*256+b[p+1];}
// Standard Amiga note periods, also used by the original's arpeggio search.
constexpr unsigned periods[]={856,808,762,720,678,640,604,570,538,508,480,453,
    428,404,381,360,339,320,302,285,269,254,240,226,
    214,202,190,180,170,160,151,143,135,127,120,113};
}
PatSong pat_song(const Bytes& bytes,const std::function<Bytes(const std::string&)>& read_sample){
    if(bytes.size()<600 || bytes[0]!='l' || bytes[1]!='a' || bytes[2]!='g')throw std::runtime_error("Unsupported PAT header");
    const unsigned count=bytes[470];
    if(!count || count>128)throw std::runtime_error("Invalid PAT order count");
    PatSong song;unsigned last=0;
    for(unsigned n=0;n<count;++n){last=std::max(last,unsigned(bytes[472+n]));song.order.push_back(bytes[472+n]);}
    // The original scans all 128 entries to find the pattern allocation.
    for(unsigned n=0;n<128;++n)last=std::max(last,unsigned(bytes[472+n]));
    if(last>127 || bytes.size()!=600+std::size_t(last+1)*1024)throw std::runtime_error("Invalid PAT pattern extent");
    song.patterns.assign(bytes.begin()+600,bytes.end());
    for(unsigned n=0;n<15;++n){
        auto& i=song.instruments[n];const unsigned p=20+n*30;
        for(unsigned k=0;k<22 && bytes[p+k];++k){
            unsigned ch=bytes[p+k];
            if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-'))throw std::runtime_error("Unsafe PAT sample name");
            i.name+=char(ch);
        }
        i.length_words=word(bytes,p+22);i.volume=word(bytes,p+24);
        i.loop_start=word(bytes,p+26);i.loop_words=word(bytes,p+28);
        if(i.volume>64)throw std::runtime_error("Invalid PAT instrument volume");
        if(i.name.empty()){if(i.length_words)throw std::runtime_error("PAT sample has no name");continue;}
        if(!i.length_words)throw std::runtime_error("PAT sample has no length");
        const auto raw=read_sample(i.name);const unsigned size=i.length_words*2;
        if(raw.size()<size-2)throw std::runtime_error("Truncated external PAT sample");
        i.samples.assign(size,0);std::copy_n(raw.begin(),size-2,i.samples.begin()+2);
        if(i.loop_words>1 && (i.loop_start+2*i.loop_words>size))throw std::runtime_error("PAT sample loop outside data");
    }
    for(std::size_t p=0;p<song.patterns.size();p+=4){
        unsigned note=word(song.patterns,p);
        if(note && (note<113 || note>856))throw std::runtime_error("Invalid PAT note period");
        if((song.patterns[p+2]&15)==0 && song.patterns[p+3] && note && std::find(std::begin(periods),std::end(periods),note)==std::end(periods))throw std::runtime_error("Unsupported PAT arpeggio period");
    }
    return song;
}
EffectSample effect_sample(const Bytes& raw,unsigned frequency){
    if(raw.size()<2 || raw.size()>131070 || frequency<55 || frequency>3579545)throw std::runtime_error("Invalid Amiga effect sample");
    EffectSample result;result.samples.assign(raw.begin(),raw.begin()+(raw.size()&~std::size_t(1)));result.samples[0]=result.samples[1]=0;
    result.period=3579545/frequency;result.hold_ticks=unsigned(((raw.size()*10000/frequency)*50)/10000)+10;
    if(result.period>65535 || result.hold_ticks>65535)throw std::runtime_error("Amiga effect exceeds replay limits");
    return result;
}
void PatPlayer::play_effect(std::shared_ptr<const EffectSample> effect){
    if(!effect || effect->samples.size()<2 || effect->samples.size()>131070 || (effect->samples.size()&1) || !effect->period || effect->period>65535 || !effect->hold_ticks || effect->hold_ticks>65535)throw std::runtime_error("Invalid prepared Amiga effect");
    effect_=std::move(effect);effect_phase_=0;effect_ticks_=effect_->hold_ticks;
    channels_[3].active=false;channels_[3].period=effect_->period;channels_[3].volume=64;
}
void PatPlayer::volume(unsigned music,unsigned effects){if(music>100 || effects>100)throw std::runtime_error("Invalid Amiga audio volume");music_volume_=music;effect_volume_=effects;}
PatPlayer::PatPlayer(PatSong song):song_(std::move(song)){
    if(song_.order.empty() || song_.order.size()>128 || song_.patterns.empty() || song_.patterns.size()>128*1024 || song_.patterns.size()%1024)throw std::runtime_error("Invalid PAT song");
    for(auto n:song_.order)if(n>=song_.patterns.size()/1024)throw std::runtime_error("PAT order outside patterns");
    for(const auto& i:song_.instruments){
        if(i.samples.empty()){if(i.length_words)throw std::runtime_error("Missing PAT instrument data");continue;}
        if(i.length_words>65535 || i.samples.size()!=i.length_words*2 || i.volume>64 || (i.loop_words>1 && i.loop_start+2*std::uint64_t(i.loop_words)>i.samples.size()))throw std::runtime_error("Invalid PAT instrument extent");
    }
    for(std::size_t p=0;p<song_.patterns.size();p+=4){auto note=word(song_.patterns,p);if(note && (note<113 || note>856))throw std::runtime_error("Invalid PAT playback period");}

}
void PatPlayer::advance_order(){if(++order_>=song_.order.size()){order_=0;++loops_;}}
void PatPlayer::effect(PatChannel& c){
    switch(c.effect){
    case 0:{
        unsigned offset=0;
        if(tick_==1 || tick_==4)offset=c.parameter>>4;
        else if(tick_==2 || tick_==5)offset=c.parameter&15;
        else if(tick_!=3)return;
        auto p=std::find(std::begin(periods),std::end(periods),c.base_period);
        if(p==std::end(periods))throw std::runtime_error("PAT arpeggio has no base note");
        auto index=unsigned(p-std::begin(periods))+offset;
        if(index>=std::size(periods))throw std::runtime_error("PAT arpeggio exceeds note table");
        c.period=periods[index];break;
    }
    case 1:c.period=c.slide_period=unsigned(std::max(113,int(c.slide_period)-int(c.parameter&15)));break;
    case 2:c.period=c.slide_period=std::min(856u,c.slide_period+(c.parameter&15));break;
    case 11:pending_order_=c.parameter;break;
    case 12:c.volume=c.parameter&127;break;
    case 13:pending_break_=true;break;
    case 14:break; // Analogue low-pass switch, not a sequencer state change.
    case 15:speed_=c.parameter&31;break;
    default:break; // Original replay also ignores the other effect codes.
    }
}
void PatPlayer::tick(){
    const bool occupied=effect_ticks_!=0;
    struct HoldTick { unsigned& ticks;~HoldTick(){if(ticks)--ticks;} } hold{effect_ticks_};
    if(++tick_<speed_){
        for(unsigned n=0;n<(occupied?3u:4u);++n){auto& c=channels_[n];if(c.parameter || c.effect==11 || c.effect==12 || c.effect==13 || c.effect==15)effect(c);}
        return;
    }
    if(pending_order_>=0){
        if(unsigned(pending_order_)>=song_.order.size())throw std::runtime_error("PAT position jump outside order");
        order_=unsigned(pending_order_);row_=0;pending_order_=-1;
    }
    if(pending_break_){pending_break_=false;row_=0;advance_order();}
    tick_=0;
    std::size_t p=std::size_t(song_.order[order_])*1024+row_*16;
    for(unsigned channel=0;channel<4;++channel){
        auto& c=channels_[channel];
        if(channel==3 && occupied){p+=4;continue;}
        const unsigned note=word(song_.patterns,p),instrument=song_.patterns[p+2]>>4;
        c.effect=song_.patterns[p+2]&15;c.parameter=song_.patterns[p+3];p+=4;
        if(instrument){
            const auto& i=song_.instruments[instrument-1];
            if(i.samples.empty())throw std::runtime_error("PAT refers to an absent instrument");
            // The original uses one shared row-volume override, reset only
            // when an instrument is loaded. It can affect subsequent channels.
            volume_override_=c.effect==12?c.parameter+1:0;
            c.instrument=instrument;c.sample_offset=i.loop_words!=1?i.loop_start:0;
            c.sample_length=i.loop_words!=1?i.loop_words*2:i.length_words*2;
            c.loop_offset=i.loop_start;c.loop_length=std::max(1u,i.loop_words)*2;c.volume=i.volume;
        }else if(c.instrument)c.volume=song_.instruments[c.instrument-1].volume;
        if(note){
            if(!c.instrument)throw std::runtime_error("PAT note has no instrument");
            c.period=c.base_period=c.slide_period=note;c.active=true;c.phase=0;
        }
        if(volume_override_)c.volume=volume_override_-1;
    }
    if(++row_==64){row_=0;advance_order();}
}
std::vector<std::int16_t> PatPlayer::render(unsigned frames,unsigned sample_rate){
    if(sample_rate<8000 || sample_rate>192000 || frames>sample_rate*60)throw std::runtime_error("Invalid PAT PCM request");
    std::vector<std::int16_t> pcm(std::size_t(frames)*2);render_into(pcm.data(),frames,sample_rate);return pcm;
}
void PatPlayer::render_into(std::int16_t* pcm,unsigned frames,unsigned sample_rate){
    if(sample_rate<8000 || sample_rate>192000 || frames>sample_rate*60)throw std::runtime_error("Invalid PAT PCM request");
    if(render_rate_ && render_rate_!=sample_rate)throw std::runtime_error("Cannot change PAT sample rate during replay");
    if(frames && !pcm)throw std::runtime_error("Missing audio output buffer");
    render_rate_=sample_rate;
    for(unsigned n=0;n<frames;++n){
        interrupt_phase_+=50;if(interrupt_phase_>=sample_rate){interrupt_phase_-=sample_rate;tick();}
        int left=0,right=0;
        for(unsigned j=0;j<4;++j){
            if(j==3 && effect_ticks_ && effect_){
                const unsigned index=unsigned(effect_phase_>>32);
                if(index<effect_->samples.size()){
                    unsigned byte=effect_->samples[index];left+=(byte<128?int(byte):int(byte)-256)*64*int(effect_volume_)/100;
                    effect_phase_+=(std::uint64_t(3546895)<<32)/(std::uint64_t(sample_rate)*effect_->period);
                }
                continue;
            }
            auto& c=channels_[j];if(!c.active || !c.period)continue;
            const auto& data=song_.instruments[c.instrument-1].samples;
            unsigned index=unsigned(c.phase>>32);
            if(index>=c.sample_length){
                if(c.loop_length<=2){c.active=false;continue;}
                index=(index-c.sample_length)%c.loop_length;c.sample_offset=c.loop_offset;c.sample_length=c.loop_length;
                c.phase=(std::uint64_t(index)<<32)|(c.phase&0xffffffffu);
            }
            if(c.sample_offset+index>=data.size())throw std::runtime_error("PAT playback outside sample");
            const unsigned byte=data[c.sample_offset+index];int sample=(byte<128?int(byte):int(byte)-256)*int(std::min(c.volume,64u));
            sample=sample*int(music_volume_)/100;
            if(j==0 || j==3)left+=sample;else right+=sample;
            c.phase+=(std::uint64_t(3546895)<<32)/(std::uint64_t(sample_rate)*c.period);
        }
        pcm[n*2]=std::int16_t(left);pcm[n*2+1]=std::int16_t(right);
    }
}
}
