/******************************************************************************
Wrap process

  Classes to perform an operation on a FIFO - if the "process()" operation
  passes the end of the FIFO then it is called again for the remainder
  of data at the start of FIFO.

History
        Date       Version    Programmer         Comments
        16/2/03    1.0        Darrell Tam		Created

This is completely free software

******************************************************************************/

#ifndef _WRAP_PROCESS_FLOAT_VEC_H_
#define _WRAP_PROCESS_FLOAT_VEC_H_
#include "misc_stuff.h"

//------------------------------------------------------------------------------------------
template <class P>
inline long wrapProcess(P& p,         // class containing "process(float*, int n)"
                        Rng<float> x, // circular buffer to process
                        long i,       // index into vector "x" to start at
                        long n        // number of elements to process
)
// process vector "x", wrapping at the end of "x" back to the start
// implemented as a template instead of interface class to help optimizer
//
// return index of final element processed+1 (may end up past end of array)
{
  p.setLen(n);
  i = wrap(i, x);
  long n0 = x.size() - i;
  if (n >= n0) {
    p.process(&x[i], n0);
    n -= n0;
    i = 0;
  }
  if (n > 0)
    p.process(&x[i], n);
  return i + n;
}

//------------------------------------------------------------------------------------------
// some different src & dst mixing classes
struct PNoSrc {
  void setLen(long n) {}
  float operator()() { return 0; }
};
struct P1Src {
  float *ptr, scale;
  P1Src(float* ptr_ = NULL, float scale_ = 1)
  {
    ptr = ptr_;
    scale = scale_;
  }
  void setLen(long n) {}
  float operator()() { return *ptr++ * scale; }
};
struct P2Src {
  P1Src a, b;
  void setLen(long n) {}
  float operator()() { return *a.ptr++ * a.scale + *b.ptr++ * b.scale; }
};
struct PBlatDst {
  void setLen(long n) {}
  void operator()(float* dst, float src) { *dst = src; }
};
struct PScaleDst : public PBlatDst {
  float scale;
  void operator()(float* dst, float src) { *dst = *dst * scale + src; }
};
struct PMixDst
// mix src into dst
{
  ValStp<float> mix;

  // set mix fraction for destination
  void setMix(float start, float end)
  {
    mix.val = start;
    mix.stp = end - start;
  }

  void setLen(long n) { mix.stp /= (float)n; }
  void operator()(float* dst, float src)
  {
    *dst = *dst * mix + src * (1 - mix);
    mix.next();
  }
};
struct PRampDst : public PMixDst {
  void operator()(float* dst, float src)
  {
    *dst = *dst * mix + src;
    mix.next();
  }
};
//------------------------------------------------------------------------------------------
template <class SRC, class DST>
struct PSrcDstWrap
// dst is the fifo
{
  SRC src;
  DST dst;
  void setLen(long n)
  {
    src.setLen(n);
    dst.setLen(n);
  }
  void process(float* dst_data, long n)
  {
    float* dst_e = dst_data + n;
    for (; dst_data != dst_e; dst_data++)
      dst(dst_data, src());
  }
};

//------------------------------------------------------------------------------------------
struct PZero
// zero data
// 0 => x
{
  void setLen(long n) {}
  inline void process(float* x, long n) { memset(x, 0, n * sizeof(float)); }
};

//------------------------------------------------------------------------------------------
struct PCopyOut
    : public PZero
// copy FIFO "x" data out to a non-fifo buffer
// x => dst
{
  float* dst;
  PCopyOut(float* dst_ = NULL) { dst = dst_; }
  void process(float* src, long n)
  {
    memcpy(dst, src, n * sizeof(float));
    dst += n;
  }
};

//------------------------------------------------------------------------------------------
struct PAddOut
    : public PCopyOut
// add FIFO data "x" to dst (non-FIFO buffer)
// x + dst => dst
{
  PAddOut(float* dst_ = NULL) { dst = dst_; }
  void process(float* x, long n)
  {
    float* xe = x + n;
    for (; x != xe; x++)
      *dst++ += *x;
  }
};

//------------------------------------------------------------------------------------------
template <class INTERP_PROC, int REVERSE = 0>
struct PLinInterp
//
//
{
public: // the following must be initialized by caller prior to wrapProcess
  // instance of class to process
  INTERP_PROC proc;

  // interp envelope array, each element is 0..1, length must be >= 2
  Rng<float> interp;

public: // internal variables to PLinInterp_
  int interp_i;

  // length of each interpolation segment (& 1/fseg_len)
  float fseg_len, inv_seg;

  // position of the next segment
  float fseg_next;
  long seg_next;

  // sample position after segment has been processed
  long samp_i;

  // current multiplier
  ValStp<float> curr_mult;

public:
  PLinInterp(Rng<float> interp_ /*length must be >=2*/)
  // interp_ is data to be resampled & passed to INTERP_PROC() to match length passed to setLen()
  {
    interp = interp_;
  }

  PLinInterp(float* interp_data, int interp_len /*must be >=2*/)
  {
    interp = toRng(interp_data, interp_len);
  }

public: // called by wrapProcess
  //	"interp" data will be resampled to length "dst_n"
  void setLen(long dst_n)
  {
    // init
    interp_i = REVERSE ? interp.n - 1 : 0;
    seg_next = 0;
    fseg_next = 0;
    samp_i = 0;
    fseg_len = (float)dst_n / (float)(interp.n - 1);
    inv_seg = 1 / fseg_len;
    proc.setLen(dst_n);
  }

  // call operator() (dst, resampled_interp) on "INTERP_PROC" where resampled_interp
  // corresponds to values from original interp data linearly interpolated to match length
  // "dst_n" passed in setLen
  void process(float* dst, long n)
  {
    float* dst_e_final = dst + n;
    bool done = false;
    while (!done) {
      // chk whether we need to prepare for the next segment
      if (samp_i >= seg_next)
        prepSeg();

      // find end sample for this segment
      float* dst_e = dst + seg_next - samp_i;
      if (dst_e >= dst_e_final) {
        done = true;
        dst_e = dst_e_final;
      }
      // sample position after processing
      samp_i += (long)(dst_e - dst);

      // process segment
      while (dst < dst_e) {
        proc(dst++, curr_mult);
        curr_mult.next();
      }
    }
  }

public: // internal method
  void prepSeg()
  {
    // get new step value
    float curr_interp = interp[interp_i];
    if (REVERSE) {
      if (interp_i > 0)
        interp_i--;
    }
    else {
      if (interp_i < interp.n - 1)
        interp_i++;
    }
    curr_mult.stp = (interp[interp_i] - curr_interp) * inv_seg;

    // work out start value for this segment
    float frac = (float)samp_i - fseg_next;
    curr_mult.val = curr_interp + curr_mult.stp * frac;

    // prepare for next interp segment
    fseg_next += fseg_len;
    seg_next = (long)ceilf(fseg_next);
  }
};

template <class SRC> struct PMix {
  SRC src;
  void setLen(long n) { src.setLen(n); }
  void operator()(float* dst, float src_mix) { *dst = lin_interp(src_mix, *dst, src()); }
};

struct PScaleCopyOut {
  float* dst;
  void setLen(long n) {}
  void operator()(float* src, float mix) { *dst++ = *src * mix; }
};

#endif
