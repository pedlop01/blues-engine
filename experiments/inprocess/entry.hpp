// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "titus/catalog.hpp"
namespace titus_pilot {
extern void (*on_ready)();
double cpu_ms();
titus::Json enter(const titus::fs::path& request);
}
