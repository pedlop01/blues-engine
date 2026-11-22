/* Test-only access to sequencer ticks and OPL register writes. */
#include "../third_party/nuked-opl3/opl3.h"
#include <assert.h>

static unsigned trace_count;
static uint16_t trace[16384];
static void trace_write(opl3_chip *chip, uint16_t reg, uint8_t value) {
	assert(trace_count < sizeof(trace) / sizeof(trace[0]));
	trace[trace_count++] = (reg << 8) | value;
	OPL3_WriteRegBuffered(chip, reg, value);
}
#define OPL3_WriteRegBuffered trace_write
#include "../dos_audio.c"
#undef OPL3_WriteRegBuffered

unsigned test_tick(struct dos_audio *a, uint16_t *output) {
	trace_count = 0;
	tick(a);
	memcpy(output, trace, trace_count * sizeof(*trace));
	return trace_count;
}

int test_failed(struct dos_audio *a) { return a->failed; }
