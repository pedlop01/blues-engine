// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_enemies.hpp"
#include <algorithm>
#include <stdexcept>
namespace titus::fox_amiga {
namespace {
unsigned data_word(const Bytes& data,unsigned offset){if(offset+2>data.size())throw std::runtime_error("Enemy table outside original data");return unsigned(data[offset])*256+data[offset+1];}
unsigned source_word(const Enemy& source,unsigned offset){if(offset+2>source.parameters.size())throw std::runtime_error("Enemy parameter outside source record");return source.parameters[offset]|(unsigned(source.parameters[offset+1])<<8);}
unsigned group(const Bytes& data,unsigned frame){
    for(unsigned p=0x1b4c;p+6<=data.size();p+=6){unsigned type=data_word(data,p);if(!type || type>=32768)return 1000;if(frame>=data_word(data,p+2) && frame<=data_word(data,p+4))return type;}
    throw std::runtime_error("Missing enemy classification sentinel");
}
unsigned carry_frame(unsigned type){
    switch(type){
    case 1:return 4;case 6:return 29;case 11:return 48;case 13:return 57;case 14:return 66;
    case 16:return 0x5300;case 17:return 85;case 18:return 0x5f00;case 19:return 102;
    case 21:return 0x7000;case 22:return 0x7700;case 23:return 0x7d00;case 28:return 0x9200;
    default:return 0;
    }
}
}
unsigned EnemyBody::get(unsigned offset)const{if((offset&1) || offset>=92)throw std::runtime_error("Invalid enemy word offset");return words[offset/2];}
int EnemyBody::signed_get(unsigned offset)const{unsigned value=get(offset);return value<32768?int(value):int(value)-65536;}
void EnemyBody::set(unsigned offset,int value){if((offset&1) || offset>=92)throw std::runtime_error("Invalid enemy word offset");words[offset/2]=std::uint16_t(unsigned(value)&65535);}
std::uint32_t EnemyBody::animation()const{return (get(26)<<16)|get(28);}
void EnemyBody::animation(std::uint32_t value){set(26,int(value>>16));set(28,int(value&65535));}
EnemyWorld initialize_enemies(const Level& level,const NativeRules& rules){
    if(rules.table_data.size()!=9644 || level.enemies.size()>50 || !level.number || level.number>16 || level.number==15)throw std::runtime_error("Invalid native enemy initialization inputs");
    EnemyWorld world;
    for(const auto& source:level.enemies){
        const unsigned raw=source_word(source,4);if(raw==65535 || raw!=source.raw_sprite)throw std::runtime_error("Invalid compact enemy source");
        EnemyBody enemy;auto set=[&](unsigned offset,int value){enemy.set(offset,value);};
        const unsigned behavior=source_word(source,6)&31;unsigned kind=behavior>18?0:(raw&8191)+1;
        set(0,int(kind));set(2,int(source_word(source,0)));set(4,int(source_word(source,2)));set(6,int(source_word(source,8)));set(14,int(std::min(240u,source_word(source,12))));set(24,int(behavior));
        if(kind){
            bool found=false;
            for(unsigned p=0x12da;p+2<=rules.table_data.size();p+=2){const unsigned n=data_word(rules.table_data,p);if(n==0xfc18)break;if(n==kind-1){enemy.animation(0xb0000+p);found=true;break;}}
            if(!found){set(0,0);enemy.animation(0);}
            const auto word=[&](unsigned offset){return source_word(source,offset);};
            const auto byte=[&](unsigned offset){return unsigned(source.parameters.at(offset));};
            switch(behavior){
            case 0:case 1:
                set(6,word(8)?1:0);set(32,int(word(15)));set(34,int(word(17)));break;
            case 2:
                set(36,int(byte(15)));set(38,int(byte(16)*2));set(40,int(word(17)));break;
            case 3:case 4:case 5:case 6:
                set(32,int(word(15)));set(34,int(word(17)));set(42,int(byte(19)));set(44,word(20)?1:0);set(48,int(word(22)));
                if(behavior<=4)set(46,int(word(24)));
                break;
            case 7:case 8:case 9:case 10:case 11:case 13:case 14:case 15:case 16:
                set(50,int(word(15)));set(52,int(word(17)));set(60,word(19)?1:0);set(54,int(byte(20)));set(56,int(word(21)));set(58,int(word(23)));
                if(!(enemy.get(4)&15)){set(4,enemy.signed_get(4)-1);set(52,int(enemy.get(52))-1);}
                break;
            case 12:
                set(62,int(word(15)));set(64,int(word(17)>1?word(17)>>1:word(17)));set(66,int(byte(17)));set(68,int(byte(19)));set(70,int(word(20)));break;
            case 17:
                set(6,0);set(72,int(word(15)));set(74,int(word(17)));set(76,int(word(19)));set(78,int(word(21)));break;
            case 18:
                set(80,int(word(15)));set(82,int(word(17)));set(84,int(byte(19)>1?byte(19)>>1:byte(19)));set(86,int(word(20)));set(88,int(word(22)));break;
            default:break;
            }
            if(enemy.get(0)){
                const unsigned type=group(rules.table_data,enemy.get(0)-1);set(90,int(carry_frame(type)));
                if((type>=29 && type<=32) || type==40 || type==51){set(18,int(enemy.get(18)|1));world.boss_power=data_word(rules.table_data,0x360+(level.number-1)*2);}
            }
        }
        world.enemies.push_back(enemy);
    }
    return world;
}
Bytes dynamic_enemy_projection(const EnemyWorld& world){
    if(world.enemies.size()>50)throw std::runtime_error("Too many Amiga enemies");
    Bytes bytes;bytes.reserve(world.enemies.size()*92);
    for(const auto& enemy:world.enemies)for(auto word:enemy.words){bytes.push_back(std::uint8_t(word>>8));bytes.push_back(std::uint8_t(word));}
    return bytes;
}
bool pick_enemy(EnemyWorld& world,const Body& player){
    if(world.carried || world.enemies.size()>50)return false;
    for(unsigned index=0;index<world.enemies.size();++index){auto& enemy=world.enemies[index];
        if(!enemy.get(0) || (enemy.get(24)&192) || !(enemy.get(90)&255) || !(enemy.get(18)&0x8000) || std::abs(player.feet.y-enemy.signed_get(4))>4)continue;
        int x=enemy.signed_get(2),width=int(enemy.get(20));
        if(player.facing_right?(x<=player.feet.x || player.feet.x<=x-width):(player.feet.x<=x || player.feet.x>x+width))continue;
        enemy.set(16,1);world.attachment={x-player.feet.x,enemy.signed_get(4)-player.feet.y};enemy.set(6,0);enemy.set(8,0);enemy.set(0,(enemy.get(90)&255)+1);enemy.set(24,192);
        unsigned last=unsigned(world.enemies.size()-1);std::swap(enemy,world.enemies[last]);world.carried=last;return true;
    }
    return false;
}
void throw_enemy(EnemyWorld& world,const Body& player,bool upward){
    if(!world.carried)return;
    if(*world.carried>=world.enemies.size())throw std::runtime_error("Invalid carried enemy");
    auto& enemy=world.enemies[*world.carried];enemy.set(16,0);enemy.set(14,0);
    if(upward)enemy.set(8,-160);
    else{enemy.set(6,player.facing_right?160:-160);enemy.set(8,enemy.get(18)&4?1:0);if(!(enemy.get(18)&4))enemy.set(18,enemy.get(18)|128);enemy.set(4,player.feet.y-16);}
    world.carried.reset();
}
void drop_enemy(const Level& level,EnemyWorld& world,const Body& player){
    if(!world.carried)return;
    if(*world.carried>=world.enemies.size())throw std::runtime_error("Invalid carried enemy");
    auto& enemy=world.enemies[*world.carried];enemy.set(6,0);enemy.set(16,0);enemy.set(8,16);enemy.set(14,0);world.carried.reset();
    int x=std::clamp(enemy.signed_get(2),int(enemy.get(20)/2),4096-int(enemy.get(20)/2)),y=enemy.signed_get(4);
    if(collision_at(level,x,y).horizontal&1)x=player.feet.x;
    if((collision_at(level,x,y).floor&1) || collision_at(level,x,y).ceiling)y=player.feet.y;
    enemy.set(2,x);enemy.set(4,y);
}
void attach_enemy(EnemyWorld& world,Point player_feet,Point attachment){
    if(!world.carried)return;
    if(*world.carried>=world.enemies.size())throw std::runtime_error("Invalid carried enemy");
    world.attachment=attachment;auto& enemy=world.enemies[*world.carried];enemy.set(2,player_feet.x+attachment.x);enemy.set(4,player_feet.y+attachment.y);
}

}
