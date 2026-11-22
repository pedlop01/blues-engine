
SDL_CFLAGS := `sdl2-config --cflags`
SDL_LIBS   := `sdl2-config --libs`

MODPLUG_LIBS ?= -lmodplug

BB := decode.c game.c level.c objects.c state.c resource.c screen.c sound.c staticres.c tiles.c unpack.c
JA := game.c level.c resource.c screen.c sound.c staticres.c unpack.c
P2 := bosses.c game.c level.c monsters.c resource.c screen.c sound.c staticres.c unpack.c

BB_SRCS := $(foreach f,$(BB),bb/$f)
JA_SRCS := $(foreach f,$(JA),ja/$f)
P2_SRCS := $(foreach f,$(P2),p2/$f)
SRCS := $(BB_SRCS) $(JA_SRCS) $(P2_SRCS)
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)
APP_OBJS := main.o mixer.o dos_audio.o third_party/nuked-opl3/opl3.o sys_sdl2.o util.o
GAME_OBJS := game_bb.o game_ja.o game_p2.o

.PHONY: all clean

CPPFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wpedantic -MMD $(SDL_CFLAGS) -I. -g

all: blues

game_bb.o: CPPFLAGS += -fvisibility=hidden

game_bb.o: $(BB_SRCS:.c=.o)
	ld -r -o $@ $^
	objcopy --localize-hidden $@

game_ja.o: CPPFLAGS += -fvisibility=hidden

game_ja.o: $(JA_SRCS:.c=.o)
	ld -r -o $@ $^
	objcopy --localize-hidden $@

game_p2.o: CPPFLAGS += -fvisibility=hidden

game_p2.o: $(P2_SRCS:.c=.o)
	ld -r -o $@ $^
	objcopy --localize-hidden $@

blues: $(APP_OBJS) $(GAME_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(SDL_LIBS) $(MODPLUG_LIBS)

VIEW_TEST_OBJS := $(filter-out bb/level.o,$(BB_SRCS:.c=.o)) mixer.o dos_audio.o third_party/nuked-opl3/opl3.o util.o
build/tests/check_viewport: tests/check_viewport.c bb/level.c sys_sdl2.c main.c $(VIEW_TEST_OBJS)
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DBLUES_ONLY $(LDFLAGS) -o $@ tests/check_viewport.c $(VIEW_TEST_OBJS) $(SDL_LIBS) $(MODPLUG_LIBS)

build/tests/check_sprite_interpolation: tests/check_sprite_interpolation.c sys_sdl2.c util.o
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/check_sprite_interpolation.c util.o $(SDL_LIBS)

build/tests/check_pause_menu: tests/check_pause_menu.c sys_sdl2.c sys_sdl2_menu.h util.o
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/check_pause_menu.c util.o $(SDL_LIBS)

build/tests/check_preferences: tests/check_preferences.c sys_sdl2.c sys_sdl2_preferences.h sys_sdl2_menu.h util.o
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/check_preferences.c util.o $(SDL_LIBS)

build/tests/check_mixer_volumes: tests/check_mixer_volumes.c mixer.c audio_gain.h util.o
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/check_mixer_volumes.c util.o $(MODPLUG_LIBS)

clean:
	$(RM) $(OBJS) $(DEPS) $(APP_OBJS) $(APP_OBJS:.o=.d) $(GAME_OBJS) blues
	$(RM) build/tests/check_preferences build/tests/check_preferences.d
	$(RM) build/tests/check_mixer_volumes build/tests/check_mixer_volumes.d
	$(RM) build/tests/check_pause_menu build/tests/check_pause_menu.d
	$(RM) build/tests/check_viewport build/tests/check_viewport.d build/tests/check_sprite_interpolation build/tests/check_sprite_interpolation.d

-include $(DEPS) main.d mixer.d dos_audio.d sys_sdl2.d util.d third_party/nuked-opl3/opl3.d build/tests/check_viewport.d

-include build/tests/check_sprite_interpolation.d

-include build/tests/check_pause_menu.d

-include build/tests/check_mixer_volumes.d

-include build/tests/check_preferences.d
