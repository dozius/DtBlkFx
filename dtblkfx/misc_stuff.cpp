#include <StdAfx.h>

#include "misc_stuff.h"

//-------------------------------------------------------------------------------------------------
CharRng RngPrintNumStruct::print(Rng<char> dst) const
{
  struct {
    float max_val;
    const char* fmt;
    float mult;
  } stuff[] = {
      {1e-5f, "%.2fu", 1e6f},
      {1e-4f, "%.1fu", 1e6f}, // micro
      {1e-3f, "%.0fu", 1e6f}, // micro
      {1e-2f, "%.2fm", 1e3f}, // milli
      {1e-1f, "%.1fm", 1e3f}, // milli
      {1e0f, "%.0fm", 1e3f},  // milli
      {1e1f, "%.2f", 1.0f},   // no multiplier
      {1e2f, "%.1f", 1.0f},   // no multiplier
      {1e3f, "%.0f", 1.0f},   // no multiplier
      {1e4f, "%.2fk", 1e-3f}, // kilo
      {1e5f, "%.1fk", 1e-3f}, // kilo
      {1e6f, "%.0fk", 1e-3f}, // kilo
      {1e7f, "%.2fM", 1e-6f}, // mega
      {1e8f, "%.1fM", 1e-6f}, // mega
      {1e9f, "%.0fM", 1e-6f}  // mega
  };

  float abs_v = fabsf(v);
  int i = 0;
  while (i < NUM_ELEMENTS(stuff) - 1) {
    if (stuff[i].max_val >= max_unit)
      break;
    if (stuff[i].max_val > min_unit && abs_v < stuff[i].max_val)
      break;
    i++;
  }
  return dst << sprf(stuff[i].fmt, v * stuff[i].mult);
}

CharRng operator<<(CharRng dst, PercentStruct s)
{
  const char* fmt[] = {"%2.0f%%",
                       "%2.1f%%",
                       "%2.2f%%",
                       "%2.3f%%",
                       "%2.4f%%",
                       "%2.5f%%",
                       "%2.6f%%",
                       "%2.7f%%",
                       "%2.8f%%",
                       "%2.9f%%"};
  return dst << sprf(GET_ELEMENT(fmt, s.decimal_places, "%2.9f%%"), s.frac * 100.0f);
}
