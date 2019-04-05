#ifndef _FREQ_PIXEL_MAP_H_
#define _FREQ_PIXEL_MAP_H_
#include "rfftw_float.h"

struct PixelFreqBin
//
// build fft bin to pixel mapping for each fft blk sz that we support for a fixed cplxf array
//
{
  struct BinRngMap : public std::valarray<cplxf*> {
  }; // do this to stop MSVC debug complaining
  // map for each
  BinRngMap map[NUM_FFT_SZ];

  // return cplx ranges to map to pixels (note: don't use const version of [] since "&"
  // will return the address of result on the stack)
  cplxf** getMap(int fft_idx) { return begin(map[fft_idx]); }

  void init(Rng<float> pixel_to_hz, // Hz for each pixel
            float sample_freq,
            cplxf* fft_dat // output from FFT to build pointers to
  );
};

#endif