#ifndef JSON_PAPER_CLOCK_H
#define JSON_PAPER_CLOCK_H

#include <time.h>

namespace DeviceClock {
// Reject the unset system clock (before 2024-01-01), including after a cold boot.
constexpr time_t validEpoch = 1704067200LL;
}

#endif
