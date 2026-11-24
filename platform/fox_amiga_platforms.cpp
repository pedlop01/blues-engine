// SPDX-License-Identifier: GPL-3.0-or-later
#include "titus/fox_amiga_platforms.hpp"
#include <stdexcept>
namespace titus::fox_amiga {
std::vector<PlatformBody> initialize_platforms(const Level& level){
    if(level.elevators.size()>10)throw std::runtime_error("Too many native platforms");
    std::vector<PlatformBody> result;
    for(const auto& source:level.elevators){
        PlatformBody platform;platform.body.kind=source.kind<=71?source.kind:0;platform.body.feet=source.position;
        platform.origin=source.position;platform.direction=platform.source_direction=source.direction;
        platform.speed=source.speed;platform.range=source.range;
        platform.source_range=source.parameters[8]|unsigned(source.parameters[9])<<8;
        result.push_back(platform);
    }
    return result;
}
void step_platforms(std::vector<PlatformBody>& platforms,Body& player,Point camera,const NativeRules& rules){
    if(platforms.size()>10)throw std::runtime_error("Too many native platforms");
    for(auto& platform:platforms){auto& body=platform.body;if(!body.kind)continue;
        if(body.kind>71)throw std::runtime_error("Invalid native platform sprite");
        const int speed=int(platform.speed);
        switch(platform.direction){
        case 0:body.feet.y-=speed;break;case 2:body.feet.y+=speed;break;
        case 1:body.feet.x+=speed;if(body.flags&0x4000)player.feet.x+=speed;break;
        case 3:body.feet.x-=speed;if(body.flags&0x4000)player.feet.x-=speed;break;
        default:break;
        }
        if(platform.direction<4 && ++platform.counter==platform.range){platform.direction^=2;platform.counter=0;}
        body.flags&=0x7fff;const auto& sprite=rules.sprites[30+body.kind-1];body.width=sprite.width;body.height=sprite.height;
        int left=body.feet.x-camera.x-sprite.anchor_x+(camera.x&15),top=body.feet.y-camera.y-sprite.anchor_y+1;
        if(top>175 || left>335 || left+int(sprite.width)<=0 || top+int(sprite.height)<=0)continue;
        body.flags=(body.flags|0x8000)&~0x4000u;
        // Original support test uses the lower edge plus height, rather than
        // the visual anchor. Keep this separate from drawing coordinates.
        const int bottom=body.feet.y+int(body.height),half=int(body.width/2);
        if(player.lock_ticks || player.vy<=0 || bottom<=player.feet.y || body.feet.y-4>=player.feet.y || body.feet.x-4>=player.feet.x || body.feet.x+4+half*2<=player.feet.x)continue;
        player.feet.y=body.feet.y;player.vy=0;player.state=player.return_state;body.flags|=0x4000;
    }
}
}
