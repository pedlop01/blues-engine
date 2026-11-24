/* Stable IDs refer to named tables, never executable addresses. */
#ifndef FAMILY_REGIONS_H
#define FAMILY_REGIONS_H
#include <stddef.h>
#include <stdint.h>
struct family_region {
  const void *data;
  size_t size;
};
const struct family_region *family_static_regions(size_t *count);
const struct family_region *family_boss_regions(size_t *count);
const struct family_region *family_monster_regions(size_t *count);
#endif
