#include <StdAfx.h>

#include "fftw_support.h"
#include <sstream>
#include <string.h>

using namespace std;

SinCosTable</*bits*/ 12> g_sincos_table;

#ifdef _WIN32

namespace FFTWf {
void(__cdecl* destroy_plan)(fftwf_plan p);
void(__cdecl* execute_dft_c2r)(const fftwf_plan p, fftwf_complex* in, float* out);
void(__cdecl* execute_dft_r2c)(const fftwf_plan p, float* in, fftwf_complex* out);
void(__cdecl* free)(void* p);
void*(__cdecl* malloc)(size_t n);
fftwf_plan(__cdecl* plan_dft_c2r_1d)(int n, fftwf_complex* in, float* out, unsigned flags);
fftwf_plan(__cdecl* plan_dft_r2c_1d)(int n, float* in, fftwf_complex* out, unsigned flags);
}; // namespace FFTWf

static HMODULE g_fftwf_dll = NULL;

//-------------------------------------------------------------------------------------------------
bool /*true=success*/ LoadFFTWfDll(std::vector<std::string> paths, std::ostream* err_str)
//
// load fftw ourself so that we can try to load it from a particular directory
//
// throw error if loading failed
{
  char* dll_name = "libfftw3f-3.dll";
  int i;

  // go through each path to see if we can find it
  for (i = 0; i < (int)paths.size(); i++) {
    g_fftwf_dll = LoadLibraryA((paths[i] + dll_name).c_str());
    if (g_fftwf_dll)
      break;
  }

  // otherwise try loading from system path
  if (!g_fftwf_dll) {
    g_fftwf_dll = LoadLibraryA(dll_name);
    if (!g_fftwf_dll) {
      if (err_str) {
        *err_str << "Can't load " << dll_name << " from ";
        const char* sep = "";
        for (i = 0; i < (int)paths.size(); i++) {
          *err_str << sep << paths[i];
          sep = ", ";
        }

        *err_str << " or any system path, please re-install" << endl;
      }
      return false;
    }
  }

  bool ok = true;

#  define LOAD_FN(fn)                                                                              \
    if (!GetProcAddress(FFTWf::fn, g_fftwf_dll, "fftwf_" #fn)) {                                   \
      ok = false;                                                                                  \
      if (err_str)                                                                                 \
        *err_str << "Can't load function " #fn " from " << dll_name << ", please re-install"       \
                 << endl;                                                                          \
    }

  // now grab some functions from the DLL
  LOAD_FN(destroy_plan);
  LOAD_FN(execute_dft_c2r);
  LOAD_FN(execute_dft_r2c);
  LOAD_FN(free);
  LOAD_FN(malloc);
  LOAD_FN(plan_dft_c2r_1d);
  LOAD_FN(plan_dft_r2c_1d);

  return ok;
}

#endif

//-------------------------------------------------------------------------------------------------
float EstFftBin(cplxf* fft, long centre_bin /*0..fft_len/2*/)
// this is a simple way to find the fractional frequency based on 3
// bins centered at the peak, does not work so well when the freq
// is changing by more than a bin or as the freq approaches DC
//
// note, assumes that there is always another bin after "centre_bin"
{
  // try to pull out fractional frequency

  // bins: t0=1 before centre, t1=centre, t2=1 after centre
  cplxf t0, t1 = fft[centre_bin], t2;

  // grab the bin before max
  //
  if (centre_bin > 0)
    t0 = fft[centre_bin - 1];
  else
    t0 = conj(fft[centre_bin + 1]); // reflect at DC, assume there's always another bin

  // grab the bin after max (assume that there is always another bin)
  t2 = fft[centre_bin + 1];

  // get bin differences before and after max
  cplxf d0 = t1 - t0, d1 = t2 - t1;

  // work out whether we should use the left or the right
  float p0 = abs(t0) * .4f + abs(d0);
  float p1 = abs(t2) * .4f + abs(d1);

  // work out scaling
  float s = p0 + p1;

  // very strange if this happens...
  if (!s)
    return (float)centre_bin;

  // do some arbitrary stuff to the scale factor
  s = p0 / s;
  s = 0.5f * (1 + limit_range((s - 0.5f) * 5, -1.0f, 1.0f));

  // figure out what the fractional frequency is
  float d0_abs = abs(d0);
  float d1_abs = abs(d1);

  float f0 = 0, f1 = 0;
  if (d0_abs)
    f0 = abs(t0) / d0_abs * cosf(angle(t0) - angle(d0));

  if (d1_abs)
    f1 = 1 + abs(t1) / d1_abs * cosf(angle(t1) - angle(d1));

  return (float)centre_bin + limit_range(f0 * s + f1 * (1 - s), -0.5f, 0.5f);
}

//-------------------------------------------------------------------------------------------------
void PeakFindFft::operator()(cplxf* fft, int b0,
                             int b1, // b1 >= b0
                             float estimate_fundamental)
// see header for information
{
  // fundamental
  harmonic = 1.0f;

  // impossible, in case nothing found
  max_pwr = -1.0f;

  // arbitrary if nothing found
  max_bin = (float)b0;

  //
  // do first pass to find peak
  //
  int max_bin_t = -1;

  // previous bin value
  cplxf prv = 0;

  // power of previous bin
  float prv_pwr = 0;

  //
  for (int i = b0; i <= b1; i++) {
    cplxf cur = fft[i];

    // power in this bin
    float cur_pwr = norm(cur);

    // approx power in frequency 1/2 way between previous & current
    float cur_pwr_h = norm(cur * cplxf(0.0f, .79f) + prv * cplxf(0.0f, -.79f));

    // update peak if 1/2 way freq (before current) is the greatest pwr
    if (cur_pwr_h > max_pwr) {
      max_pwr = cur_pwr_h;
      max_bin_t = i;

      // go back 1 bin if the previous bin has more power than current
      if (prv_pwr > cur_pwr)
        max_bin_t--;
    }

    // update peak if current bin has greatest pwr
    if (cur_pwr > max_pwr) {
      max_pwr = cur_pwr;
      max_bin_t = i;
    }

    prv = cur;
    prv_pwr = cur_pwr;
  }

  // nothing found
  if (max_bin_t < 0)
    return;

  // estimate fractional bin
  max_bin = EstFftBin(fft, max_bin_t);

  // now guess whether we actually found a harmonic
  //

  // don't try to find harmonics below this bin
  int b0t = max(b0, 3 /*arbitrary*/);

  // if we are a harmonic, expect either the bin above or below the fundamental to have at least
  // this much pwr
  float thresh = /*arbitrary*/ 0.1f * max_pwr;

  // hunt for the fundamental
  for (float harm_i = 2.0f; harm_i <= estimate_fundamental; harm_i++) {

    // possible position for rounded fundamental bin position
    max_bin_t = (int)floorf(max_bin / harm_i);
    if (max_bin_t < b0t)
      break;

    // inspect bins above & below suspected fundamental
    float pwr0 = norm(fft[max_bin_t]), pwr1 = norm(fft[max_bin_t + 1]);

    // is there enough power in either bin for it to be a fundamental?
    if (pwr0 > thresh || pwr1 > thresh)
      harmonic = harm_i;
  }

  // adjust max bin to correspond to estimated fundamental (if we decided that)
  max_bin /= harmonic;
}
