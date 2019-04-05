#ifndef _RFFTW_FLOAT_
#define _RFFTW_FLOAT_
/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@hotmail.com OR barrell@grapevine.net

Interface to FFTW

History
        Date       Version    Programmer         Comments
        16/2/03    1.0        Darrell Tam		Created


This is completely free software

***************************************************************************************************/

#ifndef FFTW_ENABLE_FLOAT
#  define FFTW_ENABLE_FLOAT
#endif

#include "fftw_support.h"

// constants
enum { NUM_FFT_SZ = 34, MIN_FFT_SZ = 256, MAX_FFT_SZ = 80640 };

// array of block sizes that we have plans built for
extern int g_fft_sz[NUM_FFT_SZ];

// array of plans that we have built
extern ScopeFFTWfPlan g_fft_plan[NUM_FFT_SZ], g_ifft_plan[NUM_FFT_SZ];

// create plans, throw error if failure
extern void CreateFFTWfPlans();

#endif
