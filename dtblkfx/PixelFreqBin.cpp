#include <StdAfx.h>

#include "BlkFxParam.h"
#include "PixelFreqBin.h"

#define LOG_FILE_NAME "c:\\PixFreqBin.html"
#include "Debug.h"
using namespace std;

//-------------------------------------------------------------------------------------------------
void PixelFreqBin::init(Rng<float> pixel_to_hz, // Hz for each pixel (and number of pixels)
                        float sample_freq,      //
                        cplxf* fft_dat          // output from FFT to build pointers to
)
{
  float n_pixels = (float)pixel_to_hz.n;
  float octave_per_pix = BlkFxParam::octaveSpan() / n_pixels;

  // multiplier equivalent to -half pixel
  float half_pix = powf(2.0f, -0.5f * octave_per_pix);

  // generate pixel to fft-bin range for each fft length
  // each pixel corresponds to a range of 1 or more bins in the fft
  //
  for (int i = 0; i < NUM_FFT_SZ; i++) {
    map[i].resize(pixel_to_hz.size() + 1);

    int fft_len = g_fft_sz[i];
    int max_bin = fft_len / 2; // max bin is 1/2 fft len

    // find scaling to turn pixel_to_hz[x-0.5] into fft bin position
    float hz_to_bin = half_pix * (float)fft_len / sample_freq;

    // first range starts at bin 0
    map[i][0] = fft_dat;
    int prev_bin = 0;
    int x;
    // find the start bin for each pixel
    for (x = 1; x < pixel_to_hz.size(); x++) {
      int bin = limit_range(
          RndToInt(pixel_to_hz[x] * hz_to_bin), prev_bin, max_bin // gaurantee increasing
      );
      map[i][x] = fft_dat + bin;
      prev_bin = bin;
    }
    // end of final pixel range
    map[i][x] = fft_dat + max_bin + 1;
  }
}
