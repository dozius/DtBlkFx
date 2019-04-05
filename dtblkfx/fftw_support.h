#ifndef _DT_FFTW_SUPPORT_H_
#define _DT_FFTW_SUPPORT_H_

#include "../fftw/fftw3.h"
#include "FixPoint.h"
#include "cplxf.h"
#include "fft_frac_shift.h"
#include "misc_stuff.h"
#include "sincostable.h"
#include <complex>
#include <ostream>
#include <string>
#include <vector>

enum { SIN_COS_BITS = 12 };
extern SinCosTable<SIN_COS_BITS> g_sincos_table;

inline fftwf_complex* to_fftwf_complex(cplxf* t)
{
  return &(t->data);
}

#ifdef _WIN32
// on windows we load fftw ourselves
//-------------------------------------------------------------------------------------------------
// functions loaded from "libfftw3f-3.dll" - these can only be called if LoadFFTWdll was ok
// I load the dll manually so as I can check other directories that aren't in the path
namespace FFTWf {
extern void(__cdecl* destroy_plan)(fftwf_plan p);
extern void(__cdecl* execute_dft_c2r)(const fftwf_plan p, fftwf_complex* in, float* out);
extern void(__cdecl* execute_dft_r2c)(const fftwf_plan p, float* in, fftwf_complex* out);
extern void(__cdecl* free)(void* p);
extern void*(__cdecl* malloc)(size_t n);
extern fftwf_plan(__cdecl* plan_dft_c2r_1d)(int n, fftwf_complex* in, float* out, unsigned flags);
extern fftwf_plan(__cdecl* plan_dft_r2c_1d)(int n, float* in, fftwf_complex* out, unsigned flags);
}; // namespace FFTWf

// load fftw dll, eeror message written to "err_str"
bool /*true=success*/ LoadFFTWfDll(std::vector<std::string> paths, std::ostream* err_str);

#else

// keep the calls in the namespace but just wrap
namespace FFTWf {
inline void destroy_plan(fftwf_plan p)
{
  fftwf_destroy_plan(p);
}
inline void execute_dft_c2r(const fftwf_plan p, fftwf_complex* i, float* o)
{
  fftwf_execute_dft_c2r(p, i, o);
}
inline void execute_dft_r2c(const fftwf_plan p, float* i, fftwf_complex* o)
{
  fftwf_execute_dft_r2c(p, i, o);
}
inline void free(void* p)
{
  fftwf_free(p);
}
inline void* malloc(size_t n)
{
  return fftwf_malloc(n);
}
inline fftwf_plan plan_dft_c2r_1d(int n, fftwf_complex* i, float* o, unsigned flags)
{
  return fftwf_plan_dft_c2r_1d(n, i, o, flags);
}
inline fftwf_plan plan_dft_r2c_1d(int n, float* i, fftwf_complex* o, unsigned flags)
{
  return fftwf_plan_dft_r2c_1d(n, i, o, flags);
}
}; // namespace FFTWf

#endif

//-------------------------------------------------------------------------------------------------
template <class T>
struct ScopeFFTWfMalloc
    : public _PtrBase<T>
//
// wrapper for FFTWMalloc
// NOTE single ownership, use "Steal()" to transfer ownership
// memory is deleted when out of scope
//
{
  typedef _PtrBase<T> base;

  ScopeFFTWfMalloc() {}
  explicit ScopeFFTWfMalloc(int n_elements) { resize(n_elements); }
  ~ScopeFFTWfMalloc() { resize(0); }

  // resize number of elements or 0 to delete (original data is destroyed after resize)
  void resize(int n_elements)
  {
    if (base::ptr)
      FFTWf::free(base::ptr);
    if (n_elements) {
      base::ptr = (T*)FFTWf::malloc(n_elements * sizeof(T));
      if (!base::ptr)
        throw 0;
    }
    else
      base::ptr = NULL;
  }

protected:
  // can't copy or assign
  ScopeFFTWfMalloc(const ScopeFFTWfMalloc&) {}
  void operator=(const ScopeFFTWfMalloc&) {}
};

//-------------------------------------------------------------------------------------------------
struct ScopeFFTWfPlan
    : public _PtrBase<fftwf_plan_s>
//
// wrapper for plan
// note single ownership, use "Steal()" to transfer ownership
// plan is destroyed when out of scope
{
  typedef _PtrBase<fftwf_plan_s> base;
  void Destroy()
  {
    if (base::ptr)
      FFTWf::destroy_plan(ptr);
  }
  ScopeFFTWfPlan(fftwf_plan plan = NULL) { base::ptr = plan; }
  ScopeFFTWfPlan& operator=(fftwf_plan plan)
  {
    Destroy();
    base::ptr = plan;
    return *this;
  }
  ~ScopeFFTWfPlan() { Destroy(); }

protected:
  // can't copy or assign
  ScopeFFTWfPlan(const ScopeFFTWfPlan&) {}
  void operator=(const ScopeFFTWfPlan&) {}
};

//*************************************************************************************************
class ShiftPhaseCorrect
//
// determine phase correction for a particular number of bins shift
//
{
public:
  // absolute sample position
  long long _mult;

  // fft length
  int _fft_len;

  // find phase correction multiply
  cplxf operator()(FixPoint<12 /*g_sincos_table.LEN*/> bin_shift) const
  {
    return g_sincos_table[(_mult * bin_shift.val) / _fft_len];
  }

  int maxBin() { return _fft_len / 2; }

  // MUST call init before use
  void init(long blk_samp_abs, // absolute sample starting position of block
            int fft_len        // fft length
  )
  {
    _mult = blk_samp_abs;
    _fft_len = fft_len;
  }
};

//*************************************************************************************************
inline float /*pwr*/ GetPwr(CplxfPtrPair x /*must be fwd*/)
{
  float pwr = 0.0f;
  for (; !x.equal(); x.a++)
    pwr += norm(*x);
  return pwr;
}
inline float /*pwr*/ GetPwr(cplxf* x, long b0, long b1 /*b0 <= b1*/)
{
  return GetPwr(CplxfPtrPair(x, b0, b1 + 1));
}
inline float /*pwr*/ GetPwr(cplxf* x, long n_bins)
{
  return GetPwr(CplxfPtrPair(x, n_bins));
}

//-------------------------------------------------------------------------------------------------
inline float /*amp*/ MatchPwr(float amp, double target_pwr, double curr_pwr)
// return scaling so as to match curr_pwr to target_pwr and apply "amp"
{
  double s = amp;
  if (target_pwr <= 0)
    return 0;
  if (curr_pwr > 0)
    s *= sqrt(target_pwr / curr_pwr);

  // limit the scaling
  if (s > 1e30)
    return 1.0f;
  if (s < 1e-37)
    return 0.0f;
  return (float)s;
}
//-------------------------------------------------------------------------------------------------
inline void MatchPwr(float amp, double target_pwr, double curr_pwr,
                     CplxfPtrPair x // must be fwd
)
{
  amp = MatchPwr(amp, target_pwr, curr_pwr);
  for (; !x.equal(); x.a++)
    *x = (*x) * amp;
}

//*************************************************************************************************
template <int CHANNELS_>
struct CplxfTmp
//
// temporary buffer that is cleared as required & flushed (by adding) back to original
//
{
  enum { CHANNELS = CHANNELS_ };

  VecPtr<cplxf, CHANNELS> _data, // corresponding data (src/dst)
      _tmp,                      // temporary buffer shadowing data
      _dst;                      // destination is either _data or _tmp, updated by fwd()/rev()

  // first & last bins (process inclusive) that are stored in tmp
  int _tmp_first_bin, _tmp_last_bin;

  // false=src being processed forwards, true=src being processed in reverse
  bool _reverse_direction;

  //
  CplxfTmp() { _reverse_direction = false; }

  void init(const VecPtr<cplxf, CHANNELS>& data, const VecPtr<cplxf, CHANNELS>& tmp)
  // must call init before use
  {
    _data = data;
    _tmp = tmp;

    // tmp buf is currently empty
    _tmp_first_bin = 0;
    _tmp_last_bin = -1;
  }

  ~CplxfTmp() { flushTmp(); }

  VecSlice<cplxf, CHANNELS> data(int idx) const { return _data[idx]; }
  VecSlice<cplxf, CHANNELS> dst(int idx) const { return _dst[idx]; }
  VecSlice<cplxf, CHANNELS> tmp(int idx) const { return _tmp[idx]; }

  VecPtr<cplxf, CHANNELS> data() const { return _data; }
  VecPtr<cplxf, CHANNELS> dst() const { return _dst; }
  VecPtr<cplxf, CHANNELS> tmp() const { return _tmp; }

  void setData() {}
  void setTmp() {}

  void setReverseDir(bool b = true) { _reverse_direction = b; }
  bool getReverseDir() const { return _reverse_direction; }

  void clrTmp(int first_bin, int last_bin)
  {
    for (int i = first_bin; i <= last_bin; i++)
      _tmp[i] = 0.0f;
  }

  void extendTmp(int dst_first_bin, int dst_last_bin)
  {
    // copy pointers
    _dst = _tmp;

    // is tmp empty? tmp will be the dst range given
    if (_tmp_first_bin > _tmp_last_bin) {
      clrTmp(dst_first_bin, dst_last_bin);
      _tmp_first_bin = dst_first_bin;
      _tmp_last_bin = dst_last_bin;
    }
    else {
      // otherwise we need to zero-extend tmp

      // extend to the left
      if (dst_first_bin < _tmp_first_bin) {
        clrTmp(dst_first_bin, _tmp_first_bin - 1);
        _tmp_first_bin = dst_first_bin;
      }

      // extend to the right
      if (dst_last_bin > _tmp_last_bin) {
        clrTmp(_tmp_last_bin + 1, dst_last_bin);
        _tmp_last_bin = dst_last_bin;
      }
    }
  }

  void extendTmp(int last_bin) { extendTmp(0, last_bin); }

  // flush a particulat section of the tmp buffer (if possible)
  void flushTmp(int first_bin, int last_bin)
  {
    if (first_bin <= _tmp_first_bin) {
      if (last_bin < _tmp_first_bin)
        return; // don't have this data to flush

      first_bin = _tmp_first_bin;
      _tmp_first_bin = last_bin + 1;
    }
    else if (last_bin < _tmp_last_bin)
      return; // can't segment tmp buffer

    if (last_bin >= _tmp_last_bin) {
      if (first_bin > _tmp_last_bin)
        return; // don't have this data to flush
      last_bin = _tmp_last_bin;
      _tmp_last_bin = first_bin - 1;
    }
    else if (first_bin > _tmp_first_bin)
      return; // can't segment tmp buffer

    // flush the buffer
    for (int i = first_bin; i <= last_bin; i++)
      _data[i] += _tmp[i];
  }

  // force all of the tmp buffer to be flushed
  void flushTmp() { flushTmp(_tmp_first_bin, _tmp_last_bin); }

  // force direct to data, don't go via tmp buffer
  void forceDirect(int src_first_bin, int src_last_bin)
  {
    _dst = _data;

    if (_reverse_direction)
      flushTmp(src_last_bin + 1, _tmp_last_bin);
    else
      flushTmp(_tmp_first_bin, src_first_bin - 1);
  }

  void prepare(int src_first_bin, int src_last_bin, int dst_first_bin, int dst_last_bin)
  // prepare to process data, go via tmp buffer if there's overlap
  {
    if (_reverse_direction) {
      // chk whether dst does not overlap with src
      if (dst_first_bin > src_last_bin) {
        // flush as much of tmp as possible
        flushTmp(src_last_bin + 1, _tmp_last_bin);

        // process directly into src data (copy pointers)
        _dst = _data;
        return;
      }
      // dst overlaps with src...

      // flush any tmp right of src & left of dst
      flushTmp(src_last_bin + 1, dst_first_bin - 1);

      // flush any tmp to the right of src & dst
      flushTmp(std::max(dst_last_bin + 1, src_last_bin + 1), _tmp_last_bin);
    }
    else {
      // chk whether dst does not overlap with src
      if (dst_last_bin < src_first_bin) {
        // flush as much of tmp as possible
        flushTmp(_tmp_first_bin, src_first_bin - 1);

        // process directly into src data (copy pointers)
        _dst = _data;
        return;
      }
      // dst overlaps with src...

      // flush any tmp to the left of src & right of dst
      flushTmp(dst_last_bin + 1, src_first_bin - 1);

      // flush any tmp to the left of src & dst
      flushTmp(_tmp_first_bin, std::min(dst_first_bin - 1, src_first_bin - 1));
    }
    extendTmp(dst_first_bin, dst_last_bin);
  }
};

//*************************************************************************************************
template <int CHANNELS>
void ClrFftFracShiftOverrun(VecPtr<cplxf, CHANNELS> fft, int max_bin /*fft_len/2*/)
{
  for (int i = 1; i < FFTFracShift::W; i++) {
    fft[-i] = 0.0f;
    fft[max_bin + i] = 0.0f;
  }
}

//*************************************************************************************************
template <int CHANNELS>
struct FrqShiftFft
//
// note that processing may overrun start & end of the buffer by FFTFracShift::W bins
{
public: // all of these need to be initialized before using
  CplxfTmp<CHANNELS> _buf;

  // amplitude scaling
  Slice<float, CHANNELS> _src_amp, _dst_amp;

  // phase correction
  ShiftPhaseCorrect _phase_corr;

public:
  // call before "run()" to clear out the overruns
  void clrOverrun()
  {
    ClrFftFracShiftOverrun(_buf.data(), _phase_corr.maxBin());
    int max_bin = _phase_corr.maxBin();
    for (int i = 1; i < FFTFracShift::W; i++) {
      _buf.data(-i) = 0.0f;
      _buf.data(max_bin + i) = 0.0f;
    }
  }

  // do the frequency shifting
  template <int CONJ>
  void run(int src_first_bin,          // start bin
           int src_last_bin,           // stop bin
           FixPoint<12> dst_first_bin, // initial destination bin (may be out of range)
           FixPoint<12> shift_stp = 1  // shift scaling to apply (destination step)
  )
  {
    // maximum bin to process to (inclusive)
    int max_bin = _phase_corr.maxBin();

    FixPoint<12> dst_last_bin = dst_first_bin + shift_stp * (src_last_bin - src_first_bin);

    //
    // check whether destination bins go out of range & adjust src bins
    //
    if (shift_stp > 0) {
      // rescaling spectrum forward
      if (dst_first_bin > max_bin || dst_last_bin < 0) {
        // can't process anything, destination out of bounds
        procSrcOnly(src_first_bin, src_last_bin);
        return;
      }

      float shift_stp_inv = -1.0f / (float)shift_stp;

      if (dst_first_bin < 0) {
        // out of bounds on the left
        int offs = (int)ceilf(shift_stp_inv * (float)dst_first_bin);
        dst_first_bin += offs * shift_stp;
        procSrcOnly(src_first_bin, src_first_bin + offs - 1);
        src_first_bin += offs;
      }
      if (dst_last_bin > max_bin) {
        // out of bounds on the right
        int offs = (int)ceilf(shift_stp_inv * (float)(max_bin - dst_last_bin));
        dst_last_bin -= offs * shift_stp;
        procSrcOnly(src_last_bin - offs + 1, src_last_bin);
        src_last_bin -= offs;
      }
    }
    else if (shift_stp < 0) {
      // rescaling with spectrum reversed
      if (dst_first_bin < 0 || dst_last_bin > max_bin) {
        // can't process anything, destination out of bounds
        procSrcOnly(src_first_bin, src_last_bin);
        return;
      }
      float shift_stp_inv = 1.0f / (float)shift_stp;

      if (dst_first_bin > max_bin) {
        // out of bounds on the right but we need to adjust src on the left
        int offs = (int)ceilf(shift_stp_inv * (float)(max_bin - dst_first_bin));
        dst_first_bin += offs * shift_stp;
        procSrcOnly(src_first_bin, src_first_bin + offs - 1);
        src_first_bin += offs;
      }
      if (dst_last_bin < 0) {
        // out of bounds on the left but we need to adjust src on the right
        int offs = (int)ceilf(shift_stp_inv * (float)dst_last_bin);
        dst_last_bin -= offs * shift_stp;
        procSrcOnly(src_last_bin - offs + 1, src_last_bin);
        src_last_bin -= offs;
      }
    }
    else /*shift_stp == 0*/ {
      // see whether everything ends up out of range
      if (dst_first_bin < 0 || dst_last_bin > max_bin) {
        procSrcOnly(src_first_bin, src_last_bin);
        return;
      }
    }

    // make sure that there's something to do
    if (src_last_bin < src_first_bin)
      return;

    //
    // work out whether we can process inplace or not
    //

    // work out whether we can force a write directly to the buffer
    int dst_first_t = dst_first_bin.getRound();
    int dst_last_t = dst_last_bin.getRound();

    bool force_direct = false;
    if (_buf.getReverseDir() && shift_stp >= 0) {
      dst_first_t -= FFTFracShift::W_OFFS;
      dst_last_t -= FFTFracShift::W_OFFS;
      force_direct = dst_first_t > src_first_bin && dst_last_t > src_last_bin;
    }
    else if (_buf.getReverseDir() && shift_stp <= 1) {
      dst_first_t += FFTFracShift::W_OFFS;
      dst_last_t += FFTFracShift::W_OFFS;
      force_direct = dst_first_t < src_first_bin && dst_last_t < src_last_bin;
    }

    //
    if (force_direct)
      _buf.forceDirect(src_first_bin, src_last_bin);

    // otherwise let overlap buffer decide
    else if (shift_stp >= 0)
      _buf.prepare(src_first_bin,
                   src_last_bin,
                   dst_first_bin.getRound() - FFTFracShift::W_OFFS,
                   dst_last_bin.getRound() + FFTFracShift::W_OFFS);

    else
      _buf.prepare(src_first_bin,
                   src_last_bin,
                   dst_last_bin.getRound() - FFTFracShift::W_OFFS,
                   dst_first_bin.getRound() + FFTFracShift::W_OFFS);

    //
    // do the actual processing
    //
    FixPoint<12> dst_bin = _buf.getReverseDir() ? dst_last_bin : dst_first_bin;
    if (shift_stp == 1) {
      // constant shift
      Slice<cplxf, CHANNELS> mult = _phase_corr(dst_first_bin - src_first_bin) * _dst_amp;

      if (_buf.getReverseDir()) {
        for (int src_bin = src_last_bin; src_bin >= src_first_bin; src_bin--) {
          Slice<cplxf, CHANNELS> m = getSrc<CONJ>(src_bin) * mult;
          _buf.data(src_bin) *= _src_amp;
          FFTFracShift::add(_buf.dst(), dst_bin, m);
          dst_bin -= 1;
        }
      }
      else {
        for (int src_bin = src_first_bin; src_bin <= src_last_bin; src_bin++) {
          Slice<cplxf, CHANNELS> m = getSrc<CONJ>(src_bin) * mult;
          _buf.data(src_bin) *= _src_amp;
          FFTFracShift::add(_buf.dst(), dst_bin, m);
          dst_bin += 1;
        }
      }
    }
    else if (_buf.getReverseDir()) {
      // scaling shift
      for (int src_bin = src_last_bin; src_bin >= src_first_bin; src_bin--) {
        Slice<cplxf, CHANNELS> m =
            getSrc<CONJ>(src_bin) * _dst_amp * _phase_corr(dst_bin - src_bin);
        _buf.data(src_bin) *= _src_amp;
        FFTFracShift::add(_buf.dst(), dst_bin, m);
        dst_bin -= shift_stp;
      }
    }
    else {
      // scaling shift
      for (int src_bin = src_first_bin; src_bin <= src_last_bin; src_bin++) {
        Slice<cplxf, CHANNELS> m =
            getSrc<CONJ>(src_bin) * _dst_amp * _phase_corr(dst_bin - src_bin);
        _buf.data(src_bin) *= _src_amp;
        FFTFracShift::add(_buf.dst(), dst_bin, m);
        dst_bin += shift_stp;
      }
    }
  }

  void flush(bool fold = true)
  {
    _buf.flushTmp();

    if (!fold)
      return;

    // fold data that overlapped past edges back in
    int max_bin = _phase_corr.maxBin();
    for (int i = 1; i < FFTFracShift::W; i++) {
      _buf.data(i) += conj(_buf.data(-i));
      _buf.data(max_bin - i) += conj(_buf.data(max_bin + i));
    }
  }

public: // internals
  template <int CONJ> Slice<cplxf, CHANNELS> getSrc(int idx)
  {
    if (CONJ)
      return conj(_buf.data(idx));
    return _buf.data(idx);
  }

  void procSrcOnly(int first_bin, int last_bin)
  {
    for (int i = first_bin; i <= last_bin; i++)
      _buf.data(i) *= _src_amp;
  }
};

//-------------------------------------------------------------------------------------------------
float EstFftBin(cplxf* fft, long centre /*0..fft_len/2*/);
//
// simple method to find fractional bin frequency. This frq estimation method
// seemed to give the best result for the least amount of work.
//
// the ratio formula is:
// frac0 = real(bin[p] / (bin[p]-bin[p-1]))
// frac1 = real(1+bin[p+1] / (bin[p+1]-bin[p]))
//
// where "p" is the peak
//
// And combining using weights derived from bins either side of peak integer and difference
//
// Works well if the frequency of the signal is not changing too much (not more than a bin)
// and not modulated by more than 100%
//
// this can be shown for the continuous time integral corresponding to "bin" for a fractional
// frequency "frac", the results are almost the same for the DFT (not quite but close
// enough)
//
// DFT(bin) ~= integral(t=0..1, exp(i.2pi.t.frac) . exp(-i.2pi.t.bin) . dt)
//     ~= integral(w=0..1, exp(i.2pi.t.(frac-bin)) . dt)
//     ~= 1/(i.2pi.(frac-bin)) (exp(i.2pi.(frac-bin) - exp(0))
//     ~= (exp(i.2pi.frac)-1)/(i.2pi.(frac-bin))
// now substitute the result into the above equations
//
// the FFT gives slightly different results compared to continuous time but it's close
// enough

//*************************************************************************************************
struct PeakFindFft
//
// find peak bin from FFT data between b0 & b1 inclusive
//
// Firstly hunt through the data looking for largest integer bin or estimated 1/2
// bin and use the above EstFftBin() function
//
{
  // max power found of "harmonic"
  float max_pwr;

  // maximum bin (or fundamental if harmonic > 1, actual max_bin is harmonic*max_bin)
  float max_bin;

  // which harmonic was found (if estimate_fundamental > 1)
  float harmonic;

  //
  PeakFindFft() {}

  //
  // note may overrun "b1" by 1 bin when determining peak
  PeakFindFft(cplxf* fft, int b0, int b1, float estimate_fundamental = 1.0f)
  {
    operator()(fft, b0, b1, estimate_fundamental);
  }

  void operator()(cplxf* fft, int b0, int b1 /* b1 >= b0, b1 < fft_len/2 */,
                  float estimate_fundamental = 1.0f);

  // return peak bin position
  operator float() { return max_bin; }
};

#endif
