/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com


History
        Date       Version    Programmer         Comments
        16/2/03    1.0        Darrell Tam		Created


This software is completely free software

***************************************************************************************************/
#include <StdAfx.h>

#include "rfftw_float.h"

// these blocks were found by choosing the fastest 4 block sizes between each pwr-of-2 (including
// the pwr-of-2) blocks, most of them were auto selected but a few were hand picked
int g_fft_sz[NUM_FFT_SZ] = {256,   320,   384,   448,   512,   640,   768,   896,   1024,
                            1280,  1536,  1792,  2048,  2560,  3072,  3584,  4096,  5120,
                            6144,  7168,  8192,  9600,  12288, 13440, 16384, 20480, 24576,
                            28672, 32768, 40500, 49152, 57600, 65536, 80640};

ScopeFFTWfPlan g_fft_plan[NUM_FFT_SZ], g_ifft_plan[NUM_FFT_SZ];

//-------------------------------------------------------------------------------------------------
void CreateFFTWfPlans()
{
  // dummy arrays that we use to create the plan
  ScopeFFTWfMalloc<float> a;
  a.resize(MAX_FFT_SZ);

  ScopeFFTWfMalloc<cplxf> b;
  b.resize(MAX_FFT_SZ / 2 + 1);

  // create the plans
  for (int i = 0; i < NUM_FFT_SZ; i++) {
    g_fft_plan[i] = FFTWf::plan_dft_r2c_1d(g_fft_sz[i], a, to_fftwf_complex(b), FFTW_ESTIMATE);
    // g_fft_plan[i] = FFTWf::plan_dft_r2c_1d(g_fft_sz[i], (float*)NULL, (fftwf_complex*)NULL,
    // FFTW_ESTIMATE);
    g_ifft_plan[i] = FFTWf::plan_dft_c2r_1d(g_fft_sz[i], to_fftwf_complex(b), a, FFTW_ESTIMATE);
    // g_ifft_plan[i] = FFTWf::plan_dft_c2r_1d(g_fft_sz[i], (fftwf_complex*)NULL, (float*)NULL,
    // FFTW_ESTIMATE);

    if (!g_fft_plan[i] || !g_ifft_plan[i])
      throw 0;
  }
}
