/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Implementation of FxRun classes - these are the actual FFT effects called by DtBlkFx

There is one FxRun1_0 class instance per effect type

In general the call sequence for processing data is as follows:
1. DtBLkFx has buffered a blk of data, performed FFT
2. DtBlkFx calls Get_DtBlkFx function to retrieve the specific effect instance derived from _DtBlkFx
class
3. DtBlkFx calls the "process" method on instance from (2)
4. Process method instantiates an Effect processing class (on the stack) and calls the "Run"
template function with the appropriate Effect
5. The run template (usually) instantiates a SplitMaskProcess (which handles processing the range
   between freqA & freqB or excluding the range between freqA & freqB)
6. The run template may instantiate another MaskProcess if the previous effect is say a harmonic
mask
7. The run method from the SplitMaskProcess (or other MaskProcess) is called
8. SplitMaskProcess (or other MaskProcess) calls run method of effect class passed into Run template
   function 0 or more times for each continuous piece of the spectrum that needs processing

The key things are:
   Heavy use of templates to try to force as much compile time optimization as possible.

   There are 2 levels of classes for each effect: the interface based on _DtBlkFx where there is one
instance per effect and the actual effect processing that is derived from the internal class
"ProcessBase".

   Masks and effects are both based on "ProcessBase" and calls flow downwards via the "run" method
of "ProcessBase" that they override.

   The ProcessBase effects are instantiated on the stack, so that means you can't store any data
between blocks in them

History
  Date       Version    Programmer         Comments
  16/2/03    1.0        Darrell Tam		Created

This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.
TODO:
  this is getting big and fat
***************************************************************************************************/

#include <StdAfx.h>

#define LOG_FILE_NAME "c:\\fx1_0.html"
#include "Debug.h"

#include "SinCosTable.h"

#include "DtBlkFx.hpp"
#include "FxRun1_0.h"
#include "FxState1_0.h"
#include "HarmData.h"

using namespace std;

// constants
enum { AUDIO_CHANNELS = BlkFxParam::AUDIO_CHANNELS };

long g_rand_i = 1;

//*************************************************************************************************
class PhaseCorrect
// find phase correction for bin shifting operations (old one, use other one)
{
public:
  // absolute sample position multiplied by sincos table length
  long long _mult;

  int _fft_len;

  // most recent correction
  cplxf _curr;
  cplxf& operator()() { return _curr; }

  cplxf& operator()(int bin_shift)
  {
    _curr = g_sincos_table[(_mult * bin_shift) / _fft_len];
    return _curr;
  }

  // MUST call init before use
  void init(DtBlkFx* b)
  {
    _mult = (long long)b->_blk_samp_abs * g_sincos_table.LEN;
    _fft_len = b->_freq_fft_n;
  }
};

//-------------------------------------------------------------------------------------------------

// initialize phase correction
void init(ShiftPhaseCorrect& phase_corr, DtBlkFx* b)
{
  phase_corr.init(b->_blk_samp_abs, b->_freq_fft_n);
}

// initialize temporary buffer
template <int CHANNELS> void init(CplxfTmp<CHANNELS>& buf, DtBlkFx* b)
{
  buf.init(b->_FFTdata<CHANNELS>(), b->_FFTdataTmp<CHANNELS>());
}

// initialize frq shift
template <int CHANNELS> void init(FrqShiftFft<CHANNELS>& proc, DtBlkFx* b)
{
  // init the temp buffer
  init(proc._buf, b);

  // init the phase corrector
  init(proc._phase_corr, b);

  // clear the overrun area
  proc.clrOverrun();
}

//*************************************************************************************************
void FxRun1_0::_fillValues(int num_presets, float out_min, float out_max, bool rng_inclusive)
// internal method that uses display value to fill _presets array
{
  //
  int start_idx = _value_presets.size();

  // ensure there's enough room
  _value_presets.resize(start_idx + num_presets);

  float in_max = (float)num_presets;
  if (rng_inclusive)
    in_max--;

  // generate
  for (int i = 0; i < num_presets; i++) {
    int j = i + start_idx;
    _value_presets[j].val = lin_scale((float)i, /*in min*/ 0.0f, in_max, out_min, out_max);
    dispVal(/*fx state*/ NULL, /*out*/ _value_presets[j].str, _value_presets[j].val);
  }
}

//-------------------------------------------------------------------------------------------------
void FxRun1_0::_fillValues(Rng<float> values)
{
  //
  int j = _value_presets.size();

  // resize
  _value_presets.resize(j + values.n);

  // fill the menu
  for (int i = 0; i < values.n; i++) {
    _value_presets[j].val = values[i];
    dispVal(/*FxState1_0*/ NULL, _value_presets[j].str, values[i]);
    j++;
  }
}

//*************************************************************************************************
class ProcessBase
//
// base class for effects, we use templates for everything so methods are not virtual
{
public:
  // which state called us
  _Ptr<FxState1_0> _s;

  // blkfx that state is attached to
  _Ptr<DtBlkFx> _b;

  ProcessBase(FxState1_0* s)
  {
    _s = s;
    _b = s->_b;
  }

  // whether to process backwards or not
  bool reverse() { return false; }

  // prepare to run using the previous process base
  void prepare(ProcessBase* parent) {}

  // run from b0 to b1 inclusive
  void run(long b0, long b1) {}

  // called after effect is done
  void done() {}
};

//*************************************************************************************************
class AmpProcess
    : public ProcessBase
//
// default process simply multiplies by amplitude
//
{
public:
  float _amp;

  AmpProcess(FxState1_0* s, float amp)
      : ProcessBase(s)
  {
    _amp = amp;
  }
  AmpProcess(FxState1_0* s)
      : ProcessBase(s)
  {
    _amp = s->temp.amp;
  }

  // apply scaling from bin b0 to b1
  void run(long b0, long b1)
  {
    VecPtr<cplxf, AUDIO_CHANNELS> d = _b->FFTdata();
    for (int i = b0; i <= b1; i++)
      d[i] *= _amp;
  }
};

//*************************************************************************************************
template <class T>
class MaskProcessBase
    : public ProcessBase
// base class for mask processing
{
public:
  // process that we're driving
  _Ptr<T> _process;

  MaskProcessBase(FxState1_0* s)
      : ProcessBase(s)
  {
  }

  // default these to calling through to next processing
  bool reverse() { return _process->reverse(); }

  void prepare(ProcessBase* parent) { _process->prepare(this); }

  void done() { _process->done(); }
};

//*************************************************************************************************
struct HarmParam
// harmonic parameters derived from a frequency and fx value
{
  // multiplier being used on fundamental
  float _multiply;

  // centre of harm-0 (bin)
  float _offs;

  // width of harmonics (bins)
  long _width;
  long _half_width;

  // spacing of harmonics (bins)
  float _spacing;

  // maximum bin (set to limit the number of harmonics processed)
  long _max_bin;

  // minimum bin (edge of lowest harmonic to process)
  long _min_bin;

  // original fundamental centre frequency bin supplied
  float _freq;

  HarmParam() { _multiply = 1.0f; }

  // initialize
  void init(float /*0..1*/ fx_val, // controls harmonic type & spacing
            float freq             // bin pos of fundamental freq
  )
  // initialize, expect _multiply to already be set
  {
    // note, fundamental freq is considered to be numbered harmonic-1 (0hz is harmonic-0)
    float f = freq * _multiply;

    if (f < 1) {
      // disable effect if frequency is invalid
      _multiply = 1;
      _freq = 1;
      _offs = 1;
      _spacing = 1;
      _width = 0;
      _half_width = 0;
      _max_bin = 0;
      _min_bin = 1;
      return;
    }

    _freq = freq;

    SplitParam<4> h(fx_val);

    switch (h.i_part) {
      case 0:
        _offs = f;
        _spacing = f;
        break; // all harmonics
      case 1:
        _offs = f;
        _spacing = f * 2;
        break; // odd harmonics
      case 2:
        _offs = f * 2;
        _spacing = f * 2;
        break; // even harmonics
      case 3:
        _offs = f * 0.5f;
        _spacing = f;
        break; // in between harmonics
    }

    // don't let harmonics get too small
    if (_spacing < 4)
      _spacing = 4;

    float fwidth = limit_range(_spacing * h.f_part, 0.0f, _spacing);
    _half_width = (long)fwidth / 2;
    _width = _half_width * 2;

    // don't do too many harmonics
    _max_bin = (long)(_spacing * 200);

    // don't go below fundamental (i.e. don't treat 0Hz as a harmonic)
    _min_bin = (long)_offs - _half_width;
  }
};

//*************************************************************************************************
template <class T>
class HarmMaskProcess
    : public MaskProcessBase<T>
    , public HarmParam {
public:
  // current centre bin
  float _curr_cent;

  // current harmonic (0 = fundamental*multiplier)
  float _curr_harm;

  void prepare(ProcessBase* parent) { MaskProcessBase<T>::_process->prepare(this); }

  HarmMaskProcess(FxState1_0* s, T& process, float /*0..1*/ fx_val, float /*bin pos*/ harm0)
      : MaskProcessBase<T>(s)
  {
    MaskProcessBase<T>::_process = &process;
    init(fx_val, harm0);
  }

  // round a frequency down to harmonic centre
  void roundHarm(float bin)
  {
    _curr_harm = floorf((bin - _offs) / _spacing);
    _curr_cent = _offs + _spacing * _curr_harm;
  }

  // run the process
  void run(long b0, long b1)
  {
    if (b0 < _min_bin)
      b0 = _min_bin;
    if (b1 > _max_bin)
      b1 = _max_bin;
    if (b0 > b1)
      return;

    bool looping = true;
    if (MaskProcessBase<T>::reverse()) {
      // process spectrum in reverse
      roundHarm(b1 + _spacing);

      // max bin for harmonic
      long p = b1;
      while (looping) {
        long v1 = RndToInt(_curr_cent) + _half_width;
        long v0 = v1 - _width;
        // don't go outside our range
        if (v0 <= b0) {
          v0 = b0;
          looping = false;
        }
        if (v1 > p)
          v1 = p;
        // process if range is good
        if (v1 >= v0) {
          MaskProcessBase<T>::_process->run(v0, v1);
          p = v0 - 1;
        }
        _curr_cent -= _spacing;
        _curr_harm--;
      }
    }
    else {
      // process spectrum fwd
      roundHarm((float)b0);

      // min bin for harmonic
      long p = b0;
      while (looping) {
        long v0 = RndToInt(_curr_cent) - _half_width;
        long v1 = v0 + _width;
        // don't go outside range
        if (v1 >= b1) {
          v1 = b1;
          looping = false;
        }
        if (v0 < p)
          v0 = p;
        // process if range is good
        if (v1 >= v0) {
          MaskProcessBase<T>::_process->run(v0, v1);
          p = v1 + 1;
        }
        _curr_cent += _spacing;
        _curr_harm++;
      }
    }
  }

protected:
  // if you don't construct with anything, make sure that you:
  // set _process, _multiply
  // call init()
  HarmMaskProcess(FxState1_0* s)
      : MaskProcessBase<T>(s)
  {
  }
};

//-------------------------------------------------------------------------------------------------
template <class T>
class AutoHarmMaskProcess
    : public HarmMaskProcess<T>
//
// auto harm mask aligns fundamental freq with peak within freq a/b within params
//
{
public:
  typedef HarmMaskProcess<T> base;

  AutoHarmMaskProcess(FxState1_0* s,
                      T& process,      // processing to run
                      float fx_val,    // harmonic type (all, odd, even, between)
                      float multiply,  // harmonic position multiply (1=peak is fundamental,
                                       // .5=fundamental is 1/2 peak, etc)
                      long b0, long b1 // bins to peak search over
                      )
      : HarmMaskProcess<T>(s)
  {
    base::_process = &process;
    base::_multiply = multiply;

    if (b0 > b1)
      swap(b0, b1);

    // always peak find on the left channel for stereo
    base::init(fx_val,
               PeakFindFft(base::_b->FFTdata(/*left*/ 0),
                           b0,
                           b1,
                           /*estimate fundamental*/ 5.0f) // find fft peak bin
    );
  }
};

//-------------------------------------------------------------------------------------------------
template <class T>
class SplitMaskProcess
    : public MaskProcessBase<T>
//
// default freq mask behaviour:
//   process region between freq A & freq B parameters if freq A < freq B
//   otherwise exclude region (process either side)
//
{
public:
  typedef MaskProcessBase<T> base;

  // bins from the params
  long _param_bin[2];

  SplitMaskProcess(FxState1_0* s, T& process)
      : MaskProcessBase<T>(s)
  {
    base::_process = &process;
    _param_bin[0] = base::_s->temp.bin[0];
    _param_bin[1] = base::_s->temp.bin[1];
  }

  void run(long b0, long b1)
  {
    if (_param_bin[0] <= _param_bin[1]) {
      // freqA < freqB : normal process inside region
      long v0 = max(b0, _param_bin[0]);
      long v1 = min(b1, _param_bin[1]);
      if (v1 >= v0)
        base::_process->run(v0, v1);
    }
    else {
      // freqA > freqB : process outside region
      long v1 = min(b1, _param_bin[1]);
      long v0 = max(b0, _param_bin[0]);
      if (base::reverse()) {
        if (v1 >= b0)
          base::_process->run(b0, v1);
        if (b1 >= v0)
          base::_process->run(v0, b1);
      }
      else {
        if (b1 >= v0)
          base::_process->run(v0, b1);
        if (v1 >= b0)
          base::_process->run(b0, v1);
      }
    }
  }
};

//-------------------------------------------------------------------------------------------------
template <class T, int SELECT_BELOW>
class ThreshMaskProcess
    : public MaskProcessBase<T>
//
// only process bins above or below a certain amplitude threshold on channel 0 (data from channel 1
// is ignored)
//
// SELECT_BELOW: true=select areas below threshold, false=select above threshold
{
  typedef MaskProcessBase<T> base;

public:
  // threshold from param (0..1)
  float _thresh_param;

  // width either side of a threshold hit to include
  long _width_bins;

  // width_bins*2 + 1
  long _width2_bins;

  //
  ThreshMaskProcess(FxState1_0* s, //
                    T& process,
                    float thresh_param // param to set threshold, 0..1
                    )
      : MaskProcessBase<T>(s)
  {
    base::_process = &process;

    //_width_bins = (long)(width_frac*(float)b->_freq_fft_n);
    _width_bins = 1;
    _width2_bins = _width_bins * 2 + 1;

    // arbitrary function on thresh param
    _thresh_param = powf(thresh_param, 0.8f);
  }

  template <int DIR /*1=fwd, -1=rev*/>
  bool /*peak found*/ findThreshBrk(float thresh_val, CplxfPtrPair& /*in-out*/ dat)
  {
    while (!dat.equal()) {
      float t = norm(*dat.a);
      if (SELECT_BELOW ? t < thresh_val : t >= thresh_val)
        return true;
      dat.a += DIR;
    }
    return false;
  }

  void run(long b0, long b1)
  {

    // find min & max pwr of channel 0
    CplxfPtrPair dat(base::_b->FFTdata(/*channel*/ 0), b0, b1 + 1);
    FindMinMax<float> pwr_lim(1e30f, 1e-30f);
    for (; !dat.equal(); dat.a++)
      pwr_lim(norm(*dat.a));

    // determine threshold by lerp min & max values
    float thresh_val = exp_interp(_thresh_param, pwr_lim);

    //
    long v0 = -1, v1 = -1;
    cplxf* dat_0 = base::_b->FFTdata(/*channel*/ 0);

    // find points breaking the threshold and include "width" data either side
    if (base::reverse()) {
      // find first bin to break threshold
      dat = CplxfPtrPair(base::_b->FFTdata(/*channel*/ 0), b1, b0 - 1);

      if (findThreshBrk</*DIR*/ -1>(thresh_val, dat))
        v1 = std::min<long>(dat.a - dat_0 + _width_bins, b1);

      cplxf* prev_dat_a = dat.a;

      // loop finding points breaking threshold
      while (!dat.equal()) {
        prev_dat_a = dat.a;
        dat.a--;
        if (!findThreshBrk</*DIR*/ -1>(thresh_val, dat))
          break;

        // distance from previous theshold breaker
        long dist = prev_dat_a - dat.a;
        if (dist > _width2_bins) {
          // run prev range
          v0 = std::max<long>(prev_dat_a - dat_0 - _width_bins, b0);
          if (v0 <= v1)
            base::_process->run(v0, v1);

          // set start position for this rng
          v1 = dat.a - dat_0 + _width_bins;
        }
      }

      v0 = std::max<long>(prev_dat_a - dat_0 - _width_bins, b0);
    }
    else {
      // find initial threshold break
      dat = CplxfPtrPair(base::_b->FFTdata(/*channel*/ 0), b0, b1 + 1);

      if (findThreshBrk</*DIR*/ 1>(thresh_val, dat))
        v0 = std::max<long>(dat.a - dat_0 - _width_bins, b0);

      cplxf* prev_dat_a = dat.a;

      // loop finding points breaking threshold
      while (!dat.equal()) {
        prev_dat_a = dat.a;
        dat.a++;
        if (!findThreshBrk</*DIR*/ 1>(thresh_val, dat))
          break;

        // distance from previous theshold breaker
        long dist = dat.a - prev_dat_a;
        if (dist > _width2_bins) {
          // run prev range
          v1 = std::min<long>(prev_dat_a - dat_0 + _width_bins, b1);
          if (v0 <= v1)
            base::_process->run(v0, v1);

          // set start position for this rng
          v0 = dat.a - dat_0 - _width_bins;
        }
      }

      v1 = std::min<long>(prev_dat_a - dat_0 + _width_bins, b1);
    }

    // run final rng
    if (v0 >= 0 && v0 <= v1)
      base::_process->run(v0, v1);
  }
};

//*************************************************************************************************
Rng<char> HarmDispVal(Rng<char> /*out*/ text, float /*0..1*/ val)
{
  const char* name[] = {" both", " odd", " even", " between"};
  SplitParam<4> h(val);
  return text << spr_percent(h.f_part) << GET_ELEMENT(name, h.i_part, "?");
}

//-------------------------------------------------------------------------------------------------
void InitHarmValuePresets(FxRun1_0* run)
{
  run->_fillValues(5, 0, 0.2499f);
  run->_fillValues(5, 0.25f, 0.4999f);
  run->_fillValues(5, 0.5f, 0.7499f);
  run->_fillValues(5, 0.75f, 1.0f);
}

//-------------------------------------------------------------------------------------------------
class HarmMaskFx
    : public FxRun1_0
// harmonic mask effect does no processing - it provides a freq mask for the next effect
{
public:
  HarmMaskFx(const char* name, bool freq_b_param_used)
      : FxRun1_0(name)
  {
    InitHarmValuePresets(this);
    _params_used[BlkFxParam::FX_FREQ_B] = freq_b_param_used;
    _params_used[BlkFxParam::FX_AMP] = false;
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return HarmDispVal(text, val);
  }

  virtual bool isMask() { return true; }
};
HarmMaskFx g_harm_mask("HarmMask", /*freq_b_param_used*/ false);
HarmMaskFx g_auto_harm_mask("AutoHarmMask", /*freq_b_param_used*/ true);
HarmMaskFx g_auto_subharm1_mask("ASubH1Mask", /*freq_b_param_used*/ true);
HarmMaskFx g_auto_subharm2_mask("ASubH2Mask", /*freq_b_param_used*/ true);
HarmMaskFx g_auto_subharm3_mask("ASubH3Mask", /*freq_b_param_used*/ true);

//*************************************************************************************************
Rng<char> ThreshDispVal(Rng<char> /*out*/ text, float /*0..1*/ val)
{
  const char* type[2] = {"above", "below"};
  SplitParam<2> split(val);
  return text << type[split.i_part] << " " << spr_percent(split.f_part);
}

//-------------------------------------------------------------------------------------------------
class ThreshMaskFx : public FxRun1_0 {
public:
  ThreshMaskFx()
      : FxRun1_0("ThreshMask")
  {
    _params_used[BlkFxParam::FX_AMP] = false;
    _params_used[BlkFxParam::FX_FREQ_A] = false;
    _params_used[BlkFxParam::FX_FREQ_B] = false;

    _fillValues(/*num*/ 5, /*min*/ 0.0f, /*max*/ 0.4999f);
    _fillValues(/*num*/ 5, /*min*/ 0.5f, /*max*/ 1.0f);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return ThreshDispVal(text, val);
  }

  virtual bool isMask() { return true; }

} g_thresh_mask;

//-------------------------------------------------------------------------------------------------
bool /*true=prev was a mask*/ getHarmMaskFreqMult(FxRun1_0* prev_fx, float& /*out*/ mult)
// return freq multiplier if the previous effect is a harmonic mask
{
  if (prev_fx == &g_auto_harm_mask)
    mult = 1.0f;
  else if (prev_fx == &g_auto_subharm1_mask)
    mult = 0.5f;
  else if (prev_fx == &g_auto_subharm2_mask)
    mult = 1.0f / 3.0f;
  else if (prev_fx == &g_auto_subharm3_mask)
    mult = 0.25f;
  else
    return false;
  return true;
}

//-------------------------------------------------------------------------------------------------
template <class T> inline void SplitMaskRun(FxState1_0* s, T& process)
// split process entire frequency range
{
  DtBlkFx* b = s->_b;
  SplitMaskProcess<T> split(s, process);
  split.prepare(/*parent*/ NULL);
  split.run(/*start bin*/ 0, /*end bin*/ b->_freq_fft_n / 2);
  split.done();
}

//-------------------------------------------------------------------------------------------------
template <class T>
inline bool /*ran mask*/ HarmMaskRun(FxState1_0* s,
                                     T& end_process // processing to perform after masking
)
// check whether the effect should be run with a harm mask based on the previous effect
// if so, run the effect with the harmonic mask
{
  // no previous effect for set-0
  FxState1_0* prev_s = s->prevFxState();
  if (!prev_s)
    return false;

  // check for previous effect being harmonic mask & run if so
  if (prev_s->temp.fft_fx == &g_harm_mask) {
    HarmMaskProcess<T> harmmask(prev_s, end_process, prev_s->temp.val, prev_s->temp.fbin[0]);
    SplitMaskRun(s, harmmask);
    return true;
  }

  // check for previous effect being auto harmonic mask
  // get harmonic multiplier
  float mult;
  if (!getHarmMaskFreqMult(prev_s->temp.fft_fx, /*out*/ mult))
    return false;

  AutoHarmMaskProcess<T> autoharm(prev_s,
                                  end_process,
                                  prev_s->temp.val, // effect value
                                  mult,             // harmonic multiplier
                                  prev_s->temp.bin[0],
                                  prev_s->temp.bin[1] // start & stop bins for search
  );

  SplitMaskRun(s, autoharm);
  return true;
}

//-------------------------------------------------------------------------------------------------
template <class T>
inline bool /*ran mask*/ ThreshMaskRun(FxState1_0* s,
                                       T& end_process // processing to perform after masking
)
{
  // no previous effect for set-0
  FxState1_0* prev_s = s->prevFxState();
  if (!prev_s)
    return false;

  // threshold mask?
  if (prev_s->temp.fft_fx != &g_thresh_mask)
    return false;

  //
  SplitParam<2> split(prev_s->temp.val);
  if (split.i_part) {
    ThreshMaskProcess<T, /*SELECT_BELOW*/ 1> thresh_mask(prev_s, end_process, split.f_part);
    SplitMaskRun(s, thresh_mask);
  }
  else {
    ThreshMaskProcess<T, /*SELECT_BELOW*/ 0> thresh_mask(prev_s, end_process, split.f_part);
    SplitMaskRun(s, thresh_mask);
  }
  return true;
}

//-------------------------------------------------------------------------------------------------
template <class T> inline void MaskedRun(FxState1_0* s, T& end_process)
//
{
  // chk for/run harmonic mask
  if (HarmMaskRun(s, end_process))
    return;

  // chk for/run threshold mask
  if (ThreshMaskRun(s, end_process))
    return;

  // no previous mask effect, do normal split process
  SplitMaskRun(s, end_process);
}

//-------------------------------------------------------------------------------------------------
template <class T> void AutoHarmMaskRun(FxState1_0* s, T& end_process)
//
// run effects that require a harm mask
// create a default harm mask if previous effect isn't harmmask
//
{
  // check whether a harmonic mask was already specified
  if (HarmMaskRun(s, end_process))
    return;

  // if the previous effect wasn't a harmonic mask, assume autoharm mask
  DtBlkFx* b = s->_b;
  AutoHarmMaskProcess<T> auto_mask(
      s, end_process, /*fx_val*/ 0.2499f, /*freq mult*/ 1.0f, /*b0*/ 1, /*b1*/ b->_freq_fft_n / 8);
  MaskedRun(s, auto_mask);
}

//*************************************************************************************************
class HarmFiltFx : public FxRun1_0 {
public:
  HarmFiltFx()
      : FxRun1_0("HarmFilt")
  {
    InitHarmValuePresets(this);
  }

  virtual void process(FxState1_0* s)
  {
    float f_cent = min(s->temp.fbin[0], s->temp.fbin[1]);

    AmpProcess amp(s);
    HarmMaskProcess<AmpProcess> harm(s,
                                     amp,
                                     s->temp.val, // value
                                     f_cent       // centre
    );

    // adjust the start and end bins to include everything from bin 0 to the end bin specified
    s->temp.fbin[1] = max(s->temp.fbin[0], s->temp.fbin[1]);
    s->temp.bin[1] = RndToInt(s->temp.fbin[1]);
    s->temp.fbin[0] = 0;
    s->temp.bin[0] = 0;

    MaskedRun(s, harm);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return HarmDispVal(text, val);
  }

} g_harm_filt_fx;

//*************************************************************************************************
// no effect = does nothing
class NoFx : public FxRun1_0 {
public:
  NoFx()
      : FxRun1_0("DoNotUse")
  {
    memset(_params_used, 0, sizeof(_params_used));
  }
} g_no_fx;

//*************************************************************************************************
// filter effect
class FilterFx : public FxRun1_0 {
public:
  FilterFx()
      : FxRun1_0("Filter")
  {
    _params_used[BlkFxParam::FX_VAL] = false;
  }

  virtual void process(FxState1_0* s)
  {
    AmpProcess amp(s);
    MaskedRun(s, amp);
  }

} g_filter_fx;

//*************************************************************************************************
struct ContrastProcess
    : public AmpProcess
// "contrast" effect performs abs(x) ^ (2*raise+1) over the spectrum
//
{

  // pwr to raise to
  float _raise;

  // minimum & maximum values to apply power
  float _min_v, _max_v;

  //
  ContrastProcess(FxState1_0* s)
      : AmpProcess(s)
  {
    // convert from 0..1 to -1..1
    _raise = (s->temp.val - 0.5f) * 2;

    // arbitrary scaling function
    if (_raise >= 0.0f)
      _raise = lin_interp(_raise * _raise, 0.0f, 4.0f);
    else
      _raise = lin_interp(_raise * _raise, 0.0f, -0.5f);

    // find limits so as to prevent overflow when applying power
    if (_raise > 0) {
      _min_v = expf(-80.0f / (2 * _raise + 1));
      _max_v = expf(80.0f / (2 * _raise + 1));
    }
    else {
      _min_v = 1e-30f;
      _max_v = 1e30f;
    }
  }

  //
  void run(long b0, long b1)
  {
    for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
      CplxfPtrPair dat(_b->FFTdata(ch), b0, b1 + 1), x;

      // input power for this range
      float in_pwr = 0;

      // find input power
      for (x = dat; !x.equal(); x.a++)
        in_pwr += norm(*x.a);

      // find scale factor to normalize power to make sure powf works correctly
      float scale = MatchPwr(/*scale*/ 1.0f, /*target*/ (float)(b1 - b0 + 1), /*current*/ in_pwr);

      // output power for this range
      float out_pwr = 0;

      // variables from AmpProcess: _b, _amp
      for (CplxfPtrPair x = dat; !x.equal(); x.a++) {
        cplxf xc = *x * scale;
        float t = norm(xc);

        if (t < _min_v)
          xc = 0.0f;
        else if (t < _max_v)
          xc = xc * powf(t, _raise); // equivalent to abs(*x) <= abs(*x)^(2*raise+1)

        out_pwr += norm(xc);
        *x = xc;
      }

      // match output pwr to input power
      MatchPwr(/*scale*/ AmpProcess::_amp,
               /*target*/ in_pwr,
               /*current*/ out_pwr,
               CplxfPtrPair(_b->FFTdata(ch), b0, b1 + 1));
    }
  }
};

//-------------------------------------------------------------------------------------------------
class ContrastFx : public FxRun1_0 {
public:
  ContrastFx()
      : FxRun1_0("Contrast")
  {
    _fillValues(/*num*/ 9);
  }

  virtual void process(FxState1_0* s)
  {
    ContrastProcess contrast(s);
    MaskedRun(s, contrast);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return text << spr_percent(val * 2 - 1);
  }

} g_contrast_fx;

//******************************************************************************************
struct SmearProcess : public AmpProcess {
  float _smear;

  SmearProcess(FxState1_0* s)
      : AmpProcess(s)
  {
    _smear = s->temp.val;
  }

  void run(long b0, long b1)
  //
  // randomize the phase
  //
  {
    long rand_i = g_rand_i;
    for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
      for (CplxfPtrPair x(_b->FFTdata(ch), b0, b1 + 1); !x.equal(); x.a++) {
        *x = /*AmpProcess::*/ _amp * (*x) * (g_sincos_table[rand_i] * _smear + 1.0f - _smear);
        rand_i = prbs32(rand_i);
      }
    }
    g_rand_i = rand_i;
  }
};

//-------------------------------------------------------------------------------------------------
class SmearFx : public FxRun1_0 {
public:
  SmearFx()
      : FxRun1_0("Smear")
  {
    _fillValues(/*num*/ 5);
  }

  virtual void process(FxState1_0* s)
  {
    SmearProcess smear(s);
    MaskedRun(s, smear);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return text << spr_percent(val);
  }

} g_smear_fx;

//-------------------------------------------------------------------------------------------------
class ThreshFx : public FxRun1_0 {
public:
  ThreshFx()
      : FxRun1_0("Threshold")
  {
    _fillValues(/*num*/ 5, /*min*/ 0.0f, /*max*/ 0.4999f);
    _fillValues(/*num*/ 5, /*min*/ 0.5f, /*max*/ 1.0f);
  }

  virtual void process(FxState1_0* s)
  {
    AmpProcess amp(s);

    SplitParam<2> split(s->temp.val);
    if (split.i_part) {
      ThreshMaskProcess<AmpProcess, /*SELECT_BELOW*/ 1> thresh_mask(s, amp, split.f_part);
      MaskedRun(s, thresh_mask);
    }
    else {
      ThreshMaskProcess<AmpProcess, /*SELECT_BELOW*/ 0> thresh_mask(s, amp, split.f_part);
      MaskedRun(s, thresh_mask);
    }
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return ThreshDispVal(text, val);
  }

} g_thresh_fx;

//*************************************************************************************************
struct ClipProcess
    : public AmpProcess
// clipping is a fraction of maximum
{

  // threshold for clipping
  float _thresh_param;

  ClipProcess(FxState1_0* s)
      : AmpProcess(s)
  {
    _thresh_param = s->temp.val * s->temp.val;
  }

  void run(long b0, long b1)
  // clip bins with magnitude higher than the threshold
  {
    for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {

      CplxfPtrPair fft_data(_b->FFTdata(ch), b0, b1 + 1), x;

      // find min & max pwr of channel 0
      FindMinMax<float> pwr_lim(1e30f, 1e-30f);
      for (x = fft_data; !x.equal(); x.a++)
        pwr_lim(norm(*x.a));

      // determine clip-threshold by interpolating min & max values (note that a thresh param of
      // 0 means not very much clipping should be done)
      float thresh = exp_interp(1.0f - _thresh_param, pwr_lim);
      float sqrt_thresh = sqrtf(thresh);

      // do cliping
      float in_pwr = 0;
      float out_pwr = 0;
      for (x = fft_data; !x.equal(); x.a++) {
        float t0 = norm(*x.a);
        in_pwr += t0;
        if (t0 >= thresh) {
          float t1 = sqrt_thresh / sqrtf(t0);
          *x.a = *x.a * t1;
          out_pwr += thresh;
        }
        else
          out_pwr += t0;
      }

      // adjust pwr to match that before clipping
      MatchPwr(AmpProcess::_amp, in_pwr, out_pwr, fft_data);
    }
  }
};

//-------------------------------------------------------------------------------------------------
class ClipFx : public FxRun1_0 {
public:
  ClipFx()
      : FxRun1_0("Clip")
  {
    _fillValues(/*num*/ 5);
  }

  virtual void process(FxState1_0* s)
  {
    ClipProcess clip(s);
    MaskedRun(s, clip);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return text << spr_percent(val);
  }

} g_clip_fx;

//*************************************************************************************************
//-------------------------------------------------------------------------------------------------
static float /*octaves shift*/ ParamToOctave(float /*0..1*/ v)
{
  return lin_interp(v, -3.0f, 3.0f);
}

//-------------------------------------------------------------------------------------------------
class ShiftProcess
    : public AmpProcess
//
{
public:
  // max bin
  long _max_bin;

  // phase correction
  PhaseCorrect _phase;

  // bin shift distance
  long _const_bin_shift;

  // centre freq multiplier to do scaled shift
  float _shift_mult;

  float _bin_to_hz, _hz_to_bin;

  // lowest bin that we will do type a shifting
  long _type_a_low_bin;
  long _type_b_width;

  // amplitude to apply to src
  float _mix_amp;

  // direction to run processing
  bool _reverse;
  bool reverse() { return _reverse; }

  //
  ShiftProcess(FxState1_0* s)
      : AmpProcess(s)
  {
    _phase.init(/*AmpProcess::*/ _b);

    // if amp is less than 1 then we're in mix-mode
    if (_amp < 1.0f)
      _mix_amp = 1.0f - _amp;
    else
      _mix_amp = 0.0f;

    long fft_len = _b->_freq_fft_n;
    _max_bin = fft_len / 2;

    // scaled frequency shift
    float freq_scale = powf(2.0f, ParamToOctave(s->temp.val));
    _shift_mult = freq_scale - 1.0f;

    // process in reverse when shifting up in frequency
    _reverse = (freq_scale > 1.0f);

    _bin_to_hz = _b->getSampleRate() / (float)fft_len;
    _hz_to_bin = 1.0f / _bin_to_hz;

    // arbitrary sizes
    _type_a_low_bin = fft_len / 130;
    _type_b_width = max(fft_len / 700, 1L);
  }

  void run(cplxf* dst, cplxf* src)
  {
    cplxf s = *src;
    *src = s * _mix_amp;
    *dst = *dst + s * _phase();
  }

  void run(long dst_bin, long src_bin, long n_bins)
  {
    if (src_bin < 1) {
      dst_bin += (1 - src_bin);
      n_bins -= (1 - src_bin);
      src_bin = 1;
    }
    if (src_bin + n_bins > _max_bin)
      n_bins = _max_bin - src_bin;

    if (dst_bin < 1) {
      src_bin += (1 - dst_bin);
      n_bins -= (1 - dst_bin);
      dst_bin = 1;
    }
    if (dst_bin + n_bins > _max_bin)
      n_bins = _max_bin - dst_bin;

    if (n_bins < 1)
      return;

    if (_reverse) {
      // process spectrum in reverse (shift up)
      for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
        cplxf* xd = _b->FFTdata(ch) + dst_bin + n_bins;
        for (CplxfPtrPair xs(_b->FFTdata(ch), src_bin + n_bins - 1, src_bin - 1); !xs.equal();
             xs.a--, xd--)
          run(xd, xs.a);
      }
    }
    else {
      // process spectrum forwards (shift down)
      for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
        cplxf* xd = _b->FFTdata(ch) + dst_bin;
        for (CplxfPtrPair xs(_b->FFTdata(ch), src_bin, src_bin + n_bins); !xs.equal(); xs.a++, xd++)
          run(xd, xs.a);
      }
    }
  }
  void shiftUpA(long b0, long b1)
  {
    float hz = (float)b1 * _bin_to_hz;
    float part_note = 48.0f * NoteFreq::inv_log_c0_hz * (logf(hz) - NoteFreq::log_c0_hz);
    float cf = NoteFreq::c0_hz * powf(2.0f, floorf(part_note) * (1.0f / 48.0f));
    float cb = cf * _hz_to_bin;

    long v1 = b1;
    while (1) {
      long bin_shift = RndToInt(cb * _shift_mult);
      float next_cb = cb * (float)0.98566319864018758778;
      long v0 = (long)((next_cb + cb) * 0.5f);
      if (v0 <= v1) {
        _phase() = _phase(bin_shift) * AmpProcess::_amp;
        run(v0 + bin_shift, v0, v1 - v0 + 1);
        v1 = v0 - 1;
        if (v1 < b0)
          break;
      }
      cb = next_cb;
    }
  }

  void shiftDownA(long b0, long b1)
  {
    float hz = (float)b0 * _bin_to_hz;
    float part_note = 48.0f * NoteFreq::inv_log_c0_hz * (logf(hz) - NoteFreq::log_c0_hz);
    float cf = NoteFreq::c0_hz * powf(2.0f, ceilf(part_note) * (1.0f / 48.0f));
    float cb = cf * _hz_to_bin;

    long v0 = b0;
    while (1) {
      long bin_shift = RndToInt(cb * _shift_mult);
      float next_cb = cb * (float)1.0145453349375237462;
      long v1 = (long)((next_cb + cb) * 0.5f);
      if (v0 <= v1) {
        _phase() = _phase(bin_shift) * AmpProcess::_amp;
        run(v0 + bin_shift, v0, v1 - v0 + 1);
        v0 = v1 + 1;
        if (v0 > b1)
          break;
      }
      cb = next_cb;
    }
  }

  void shiftUpB(long b0, long b1)
  {
    // process in reverse
    // try to align shifts with peaks in the data
    long v0[3], v1[3];
    // peak finding operates on left channel only
    PeakFindFft p[3];
    v1[0] = b1;
    while (1) {
      v0[0] = max(v1[0] - _type_b_width, b0);
      v1[1] = v0[0] - 1;
      v0[1] = max(v1[1] - _type_b_width, b0);
      v1[2] = v0[1] - 1;
      v0[2] = max(v1[2] - _type_b_width, b0);

      for (int i = 0; i < 3; i++)
        p[i](_b->FFTdata(0), v0[i], v1[i]);

      // decide whether to use p[0] or p[1] as the peak
      float cent;
      if (p[1].max_pwr > p[0].max_pwr && p[1].max_pwr > p[2].max_pwr)
        cent = p[1].max_bin;
      else
        cent = p[0].max_bin;

      long bin_shift = RndToInt(cent * _shift_mult);
      v0[0] = max((long)cent - _type_b_width, b0);
      _phase() = _phase(bin_shift) * AmpProcess::_amp;
      run(v0[0] + bin_shift, v0[0], v1[0] - v0[0] + 1);

      if (v0[0] <= b0)
        break;
      v1[0] = v0[0] - 1;
    }
  }

  void shiftDownB(long b0, long b1)
  {
    // try to align shifts with peaks in the data
    long v0[3], v1[3];
    PeakFindFft p[3];
    v0[0] = b0;
    while (1) {
      v1[0] = min(v0[0] + _type_b_width, b1);
      v0[1] = v1[0] + 1;
      v1[1] = min(v0[1] + _type_b_width, b1);
      v0[2] = v1[1] + 1;
      v1[2] = min(v0[2] + _type_b_width, b1);

      for (int i = 0; i < 3; i++)
        p[i](_b->FFTdata(0), v0[i], v1[i]);

      // decide whether to use p[0] or p[1] as the peak
      float cent;
      if (p[1].max_pwr > p[0].max_pwr && p[1].max_pwr > p[2].max_pwr)
        cent = p[1].max_bin;
      else
        cent = p[0].max_bin;

      long bin_shift = RndToInt(cent * _shift_mult);
      v1[0] = min((long)cent + _type_b_width, b1);
      //_phase() = _phase(bin_shift) * AmpProcess::_amp;
      _phase() = cplxf(1, 0) * AmpProcess::_amp;
      run(v0[0] + bin_shift, v0[0], v1[0] - v0[0] + 1);

      if (v1[0] >= b1)
        break;
      v0[0] = v1[0] + 1;
    }
  }

  void run(long b0, long b1)
  {
    if (b1 < 1)
      return;
    if (b0 < 1)
      b0 = 1;

    if (_reverse) {
      // shift up
      if (b1 >= _type_a_low_bin)
        shiftUpA(max(_type_a_low_bin, b0), b1);
      if (b0 < _type_a_low_bin)
        shiftUpB(b0, min(_type_a_low_bin - 1, b1));
    }
    else {
      // shift down
      if (b0 < _type_a_low_bin)
        shiftDownB(b0, min(_type_a_low_bin - 1, b1));
      if (b1 >= _type_a_low_bin)
        shiftDownA(max(_type_a_low_bin, b0), b1);
    }
  }
};

//-------------------------------------------------------------------------------------------------
// fixed shift amounts
float g_shift_value_presets_fixed[] = {0.5f,
                                       lin_scale(-12, -36, 36, 0, 1),
                                       lin_scale(12, -36, 36, 0, 1),
                                       lin_scale(-24, -36, 36, 0, 1),
                                       lin_scale(24, -36, 36, 0, 1)};

//-------------------------------------------------------------------------------------------------
void InitShiftValuePresets(FxRun1_0* run)
{
  // fixed shift amounts
  run->_fillValues(TO_RNG(g_shift_value_presets_fixed));

  // shift the shift amounts
  float amt_[] = {-12, -9, -7, -5, -3, -2, -1, 1, 2, 3, 5, 7, 9, 12};
  Rng<float> amt = TO_RNG(amt_);

  int j = run->_value_presets.size();

  //
  run->_value_presets.resize(j + amt.n);

  // shift by these
  for (int i = 0; i < amt.n; i++) {
    run->_value_presets[j].val = amt[i] / 72.0f; // 6 octaves worth of notes
    run->_value_presets[j].str << sprf("Change shift by %+d notes", (int)amt[i]);
    j++;
  }
}

//-------------------------------------------------------------------------------------------------
float GetShiftValuePreset(FxRun1_0* run, int idx, float curr_val)
{
  if (!idx_within(idx, run->_value_presets))
    return curr_val;

  // value from array
  float v = run->_value_presets[idx].val;

  // first menu item sets directly
  if (idx < NUM_ELEMENTS(g_shift_value_presets_fixed))
    return v;

  // all others change the value by the amount
  return limit_range(curr_val + v, 0.0f, 1.0f);
}

//-------------------------------------------------------------------------------------------------
class ShiftFx : public FxRun1_0 {
public:
  ShiftFx()
      : FxRun1_0("Shift")
  {
    InitShiftValuePresets(this);
  }

  //
  virtual float getValue(int idx, float curr_val)
  {
    return GetShiftValuePreset(this, idx, curr_val);
  }

  virtual void process(FxState1_0* s)
  {
    ShiftProcess shift(s);
    MaskedRun(s, shift);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return text << sprf("%+.2f notes", ParamToOctave(val) * 12.0f);
  }

  virtual bool ampMixMode() { return true; }

} g_shift_fx;

//*************************************************************************************************
class ConstShiftProcess
    : public AmpProcess
// shift frequency up or down by a constant number of Hz
{
public:
  static float /*Hz*/ paramToHz(float v /*0..1*/)
  //
  // Convert freq shift param to Hz
  //
  // return frequency as a fraction between -22050 to 22050 Hz
  // maps the linear input "v" to an arbitary linear+exponential function
  // (linear transitioning into exponential)
  //
  {
    const float lin = 2048.0f / 32768.0f;
    const int exp_raise = 8;
    const float exp_scale = (1.0f - lin) / (1 << exp_raise);
    v = (v - 0.5f) * 2;

    bool pos = true;
    if (v < 0) {
      v = -v;
      pos = false;
    }
    float r = (powf(2, v * exp_raise) - 1.0f) * exp_scale + v * lin;
    return (pos ? r : -r) * 22050.0f;
  }

public:
  FrqShiftFft<AUDIO_CHANNELS> _shift;

  // frequency shift in bins
  FixPoint<12> _frq_shift;

  // process in reverse??
  bool reverse() { return _shift._buf.getReverseDir(); }

  ConstShiftProcess(FxState1_0* s)
      : AmpProcess(s)
  {
    // shift freq by this much
    _frq_shift = paramToHz(s->temp.val) / _b->getSampleRate() * _b->_freq_fft_n;

    ::init(_shift, _b);
    _shift._buf.setReverseDir(_frq_shift > 0);

    // work out amp scaling
    _shift._dst_amp = AmpProcess::_amp;
    _shift._src_amp = limit_range(1.0f - _amp, 0.0f, 1.0f);
  }

  void run(long b0, long b1)
  {
    _shift.run</*CONJ*/ 0>(/*src first*/ b0, /*src last*/ b1, /*dst first*/ b0 + _frq_shift);
  }

  void done() { _shift.flush(); }
};

//-------------------------------------------------------------------------------------------------
class ConstShiftFx : public FxRun1_0 {
public:
  ConstShiftFx()
      : FxRun1_0("ConstShift")
  {
    // shift amounts
    float values[] = {0.321054f, 0.389299f, 0.473659f, 0.476198f, 0.478758f, 0.481339f, 0.483942f,
                      0.486566f, 0.489211f, 0.491877f, 0.494564f, 0.497272f, 0.500000f, 0.502728f,
                      0.505436f, 0.508123f, 0.510789f, 0.513434f, 0.516058f, 0.518661f, 0.521242f,
                      0.523802f, 0.526341f, 0.610701f, 0.678946f};
    _fillValues(TO_RNG(values));
  }

  virtual void process(FxState1_0* s)
  {
    ConstShiftProcess shift(s);
    MaskedRun(s, shift);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return text << sprnum(ConstShiftProcess::paramToHz(val), /*min unit*/ 1.0f) << "Hz";
  }

  virtual bool ampMixMode() { return true; }

} g_const_shift_fx;

//*************************************************************************************************
class AutoHarmFx : public FxRun1_0 {
public:
  AutoHarmFx()
      : FxRun1_0("AutoHarm")
  {
    InitHarmValuePresets(this);
  }

  virtual void process(FxState1_0* s)
  {
    DtBlkFx* b = s->_b;

    //
    AmpProcess amp(s);

    // find a peak is in the first 1/8th of the spectrum
    AutoHarmMaskProcess<AmpProcess> harm(
        s, amp, /*value*/ s->temp.val, /*multiplier*/ 1.0f, /*b0*/ 0, /*b1*/ b->_freq_fft_n / 8);

    MaskedRun(s, harm);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return HarmDispVal(text, val);
  }

} g_auto_harm_fx;

//*************************************************************************************************
class ResizeProcess
    : public AmpProcess
// resize can resize in the freq & time domains
{
public:
  //
  FrqShiftFft<AUDIO_CHANNELS> _shift;

  // frequency scaling to apply to bins
  FixPoint<12> _frq_mult;

  // conjugate
  bool _conjugate_mode;

  ResizeProcess(FxState1_0* s, FixPoint<12> frq_mult, bool conjugate_mode)
      : AmpProcess(s)
  {

    ::init(_shift, _b);
    _shift._src_amp = limit_range(1.0f - _amp, 0.0f, 1.0f);
    _shift._dst_amp = _amp;
    _frq_mult = frq_mult;
    _conjugate_mode = conjugate_mode;
  }

  void run(long b0, long b1)
  {
    if (_conjugate_mode)
      _shift.run</*CONJ*/ 1>(/*src first*/ b0,
                             /*src last*/ b1,
                             /*dst*/ (b0 + b1 - (b1 - b0 + 1) * _frq_mult) / 2,
                             /*dst scale*/ _frq_mult);
    else
      _shift.run</*CONJ*/ 0>(/*src first*/ b0,
                             /*src last*/ b1,
                             /*dst*/ (b0 + b1 - (b1 - b0 + 1) * _frq_mult) / 2,
                             /*dst scale*/ _frq_mult);
  }

  void done() { _shift.flush(); }
};

//-------------------------------------------------------------------------------------------------
class ResizeFx : public FxRun1_0 {
public:
  struct Param : public SplitParam<2> {
    // arbitrary scale function on the floating point part
    Param(float param_val)
        : SplitParam<2>(param_val)
    {
      f_part = powf(lin_interp(f_part, -2.0f, 2.0f), 3);
    }
  };

  ResizeFx()
      : FxRun1_0("Resize")
  {
    float values[] = {0.000000f, 0.051575f, 0.092510f, 0.125000f, 0.150787f, 0.171255f, 0.191980f,
                      0.308020f, 0.328745f, 0.349213f, 0.375000f, 0.407490f, 0.448425f, 0.4999f,
                      0.500000f, 0.551575f, 0.592510f, 0.625000f, 0.650787f, 0.671255f, 0.691980f,
                      0.808020f, 0.828745f, 0.849213f, 0.875000f, 0.907490f, 0.948425f, 1.0f};
    _fillValues(TO_RNG(values));
  }

  virtual void process(FxState1_0* s)
  {
    Param p(s->temp.val);

    // time reverse/frequency reverse
    bool conj_mode = false;

    // work out whether to conjugate or not
    if (p.i_part)
      conj_mode = !conj_mode;
    if (p.f_part < 0)
      conj_mode = !conj_mode;

    // do the resize
    ResizeProcess resize(s, p.f_part, conj_mode);
    MaskedRun(s, resize);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    Param p(val);
    return text << spr_percent(p.f_part) << " time " << (p.i_part ? "rev" : "fwd");
  }
  virtual bool ampMixMode() { return true; }

} g_resize_fx;

//*************************************************************************************************
//-------------------------------------------------------------------------------------------------
class HarmMatchProcess : public AmpProcess {
public:
  // 2 harmonic sequences that we linearly interpolate against
  float* _harms[2];

  // number of harmonics per sequence
  int _n_harms;

  // scale factors (0..1) to apply to harm sequences
  float _harms_interp[2];

  // power scaling to apply to harmonics
  float _pwr_scale[AUDIO_CHANNELS];

  // true=copy fundamental harmonic, false=normal power matching
  bool _copy_mode;

  // amp to apply to original data
  float _orig_amp;

  // our parent must be HarmMask
  HarmMaskProcess<HarmMatchProcess>* _parent;

  // return interpolated harmonic power from table
  float harmPwr(int harmonic)
  {
    return _harms[0][harmonic] * _harms_interp[0] + _harms[1][harmonic] * _harms_interp[1];
  }

  void prepare(HarmMaskProcess<HarmMatchProcess>* parent)
  {
    AmpProcess::prepare(parent);

    _parent = parent;

    long max_bin = _b->_freq_fft_n / 2;

    // determine bin range of fundamental
    long cf = RndToInt(parent->_freq);
    long w2 = parent->_half_width;
    long f0 = limit_range(cf - w2, 1, max_bin);
    long f1 = min(cf + w2, max_bin);

    // make sure range is valid
    if (f0 > f1)
      return;

    // get pwr scaling to use
    float harm0_pwr = harmPwr(0);
    if (harm0_pwr <= 0)
      return;

    for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
      _pwr_scale[ch] = GetPwr(_b->FFTdata(ch), f0, f1) / harm0_pwr;
      if (_copy_mode)
        _pwr_scale[ch] *= _amp * _amp;
    }
  }

  HarmMatchProcess(FxState1_0* s, HarmData harm_data)
      : AmpProcess(s)
  {
    // default for pwr scale
    for (int ch = 0; ch < AUDIO_CHANNELS; ch++)
      _pwr_scale[ch] = 1.0f;

    _n_harms = harm_data.n_harms;

    // grab the fx params
    SplitParam<2> val(s->temp.val);

    //
    _copy_mode = val.i_part ? true : false;

    // find which harmonic sequences to interpolate from
    int max_table = harm_data.n_tables - 1;
    float ftable_i;
    _harms_interp[1] = modff(val.f_part * (float)max_table, &ftable_i);
    _harms_interp[0] = 1 - _harms_interp[1];

    int table_i = limit_range((int)ftable_i, 0, max_table);
    _harms[0] = harm_data.data + table_i * harm_data.n_harms;

    if (table_i < max_table)
      table_i++;
    _harms[1] = harm_data.data + table_i * harm_data.n_harms;

    // determine whether we need to mix or replace
    _orig_amp = max(1.0f - _amp, 0.0f);
  }

  void run(long b0, long b1)
  {
    long curr_harm = (long)_parent->_curr_harm;
    if (curr_harm < 0 || curr_harm >= _n_harms)
      return;

    // get required power for this harmonic
    float harm_pwr = harmPwr(curr_harm);

    if (_copy_mode) {
      // find copy position for fundamental
      long cent_offs = RndToInt(_parent->_freq - _parent->_curr_cent);
      long f0 = cent_offs + b0;
      long f1 = cent_offs + b1;

      // make sure bin range is within limits
      if (f1 >= _parent->_max_bin) {
        b1 += _parent->_max_bin - f1;
        f1 = _parent->_max_bin;
      }
      if (f0 < 1) {
        b0 += 1 - f0;
        f0 = 1;
      }

      // do nothing if inverted range
      if (f0 > f1 || b0 > b1)
        return;

      // do the copy for each channel from the fundamental
      for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
        // copy mode
        cplxf* src = _b->FFTdata(ch) + f0;

        // scaling for harmonic
        float scale = sqrtf(harm_pwr * _pwr_scale[ch]);

        if (_orig_amp) {
          // mix mode, mix source & destination
          // use scaling on source as per "scale mode"

          for (CplxfPtrPair x(_b->FFTdata(ch), b0, b1 + 1); !x.equal(); x.a++) {
            *x = (*x) * _orig_amp + (*src) * scale;
            src++;
          }
        }
        else {
          // not mix mode, replace destination
          for (CplxfPtrPair x(_b->FFTdata(ch), b0, b1 + 1); !x.equal(); x.a++) {
            *x = (*src) * scale;
            src++;
          }
        }
      }
    }
    else {

      // scale mode
      for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
        // find what we need to scale the existing data by to match the harmonic power
        float orig_pwr = GetPwr(_b->FFTdata(ch), b0, b1);
        float target_pwr = _pwr_scale[ch] * harm_pwr;

        // attempt to match to harmonic pwr
        float scale = _orig_amp + MatchPwr(_amp, target_pwr, orig_pwr);

        for (CplxfPtrPair x(_b->FFTdata(ch), b0, b1 + 1); !x.equal(); x.a++)
          *x = (*x) * scale;
      }
    }
  }
};

//-------------------------------------------------------------------------------------------------
class HarmMatchFx : public FxRun1_0 {
public:
  HarmData _data;
  HarmMatchFx(const char* name, HarmData data)
      : FxRun1_0(name)
  {
    _data = data;
    _fillValues(5, 0, 0.4999f);
    _fillValues(5, 0.5f, 1.0f);
  }

  virtual void process(FxState1_0* s)
  {
    HarmMatchProcess match(s, _data);
    AutoHarmMaskRun(s, match);
  }
  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    SplitParam<2> v(val);
    return text << (v.i_part ? "copy0" : "scale") << "/" << spr_percent(v.f_part);
  }
  virtual bool ampMixMode() { return true; }
};

extern HarmData sweep1_coeff;
HarmMatchFx g_sweep1_fx("Triangles", sweep1_coeff);

extern HarmData sweep2_coeff;
HarmMatchFx g_sweep2_fx("Squares", sweep2_coeff);

extern HarmData sweep3_coeff;
HarmMatchFx g_sweep3_fx("Saws", sweep3_coeff);

extern HarmData sweep4_coeff;
HarmMatchFx g_sweep4_fx("Pointy", sweep4_coeff);

extern HarmData sweep5_coeff;
HarmMatchFx g_sweep5_fx("Sweep", sweep5_coeff);

//*************************************************************************************************

//-------------------------------------------------------------------------------------------------
template <int CHANNELS> class HarmShiftProcess : public AmpProcess {
public:
  FrqShiftFft<CHANNELS> _shift;

  // scaling to get bin shift (frq_mult-1)
  float _shift_mult;

  //
  bool _repitch_mode;

  // our parent must be HarmMask
  HarmMaskProcess<HarmShiftProcess>* _parent;

  bool reverse() { return _shift._buf.getReverseDir(); }

  void prepare(HarmMaskProcess<HarmShiftProcess>* parent)
  {
    AmpProcess::prepare(parent);
    _parent = parent;

    float frq_mult = 1.0f;
    float fx_val = _s->temp.val;

    if (!_repitch_mode) {
      // shift mode - get multiplier from octave shift
      frq_mult = powf(2.0f, ParamToOctave(fx_val));
    }
    else {
// repitch mode, scale so that fundamental is moved to the destination note

// check for repitch to right channel
#ifdef STEREO
      if (fx_val >= 0.7f) {
        // find peak in bottom 1/8th spectrum of right channel
        PeakFindFft peak_r(_b->FFTdata(/*left*/ 1),
                           /*b0*/ 1,
                           /*b1*/ _b->_freq_fft_n / 8,
                           /*estimate fundamental*/ 5.0f);

        // repitch
        if (peak_r.max_pwr > 1e-25f && _parent->_freq > 0)
          frq_mult = (float)peak_r / _parent->_freq;

        frq_mult *= powf(2.0f,
                         lin_scale(fx_val,
                                   0.7f,
                                   1.0f, // in min & max
                                   -24.0f,
                                   24.0f // out min & max notes
                                   ) *
                             (1.0f / 12.0f) // notes to multiply factor
        );
      }
      else // otherwise standard repitch
#endif
      {
        float targ_hz = NoteOffsToHz(fx_val * BlkFxParam::noteSpan());
        float denom = _parent->_freq * _b->getSampleRate();
        if (denom > 0)
          frq_mult = targ_hz * (float)_b->_freq_fft_n / denom;
      }
    }

    _shift_mult = frq_mult - 1.0f;
    _shift._buf.setReverseDir(frq_mult > 1);
  }

  HarmShiftProcess(FxState1_0* s, bool repitch_mode)
      : AmpProcess(s)
  {
    _repitch_mode = repitch_mode;

    ::init(_shift, _b);
    _shift._src_amp = limit_range(1.0f - _amp, 0.0f, 1.0f);
    _shift._dst_amp = _amp;
  }

  void run(long b0, long b1)
  {
    // determine offset to apply to bins
    FixPoint<12> bin_shift = _parent->_curr_cent * _shift_mult;
    _shift.run</*CONJ*/ 0>(/*src first*/ b0, /*src last*/ b1, /*dst first*/ b0 + bin_shift);
  }

  void done() { _shift.flush(); }
};

//-------------------------------------------------------------------------------------------------
class HarmShiftFx : public FxRun1_0 {
public:
  HarmShiftFx()
      : FxRun1_0("HarmShift")
  {
    InitShiftValuePresets(this);
  }

  //
  virtual float getValue(int idx, float curr_val)
  {
    return GetShiftValuePreset(this, idx, curr_val);
  }

  virtual void process(FxState1_0* s)
  {
    HarmShiftProcess<AUDIO_CHANNELS> shift(s, /*repitch*/ false);
    AutoHarmMaskRun(s, shift);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    // display octave shift as notes (semitones) shift
    float notes = ParamToOctave(val) * 12.0f;
    return text << sprf("%+.2f notes", notes);
  }

  virtual bool ampMixMode() { return true; }
} g_harm_shift;

//-------------------------------------------------------------------------------------------------
float g_repitch_value_presets_fixed[] = {
    12.0f * 2.0f * BlkFxParam::noteFrac(), // set to C2
    12.0f * 3.0f * BlkFxParam::noteFrac(), // set to C3
    12.0f * 4.0f * BlkFxParam::noteFrac(), // set to C4
    12.0f * 5.0f * BlkFxParam::noteFrac(), // set to C5
    12.0f * 6.0f * BlkFxParam::noteFrac(), // set to C6
#ifdef STEREO
    lin_scale(0, -24, 24, 0.7f, 1.0f),   // follow Right channel (0 note shift)
    lin_scale(12, -24, 24, 0.7f, 1.0f),  // follow Right channel (12 note shift)
    lin_scale(-12, -24, 24, 0.7f, 1.0f), // follow Right channel (-12 note shift)
#endif
};

//-------------------------------------------------------------------------------------------------
class HarmRepitchFx : public FxRun1_0 {
public:
  HarmRepitchFx()
      : FxRun1_0("HarmRepitch")
  {
    _fillValues(TO_RNG(g_repitch_value_presets_fixed));

    // shift amounts
    float amt_[] = {-12, -9, -7, -5, -3, -2, -1, 1, 2, 3, 5, 7, 9, 12};
    Rng<float> amt = TO_RNG(amt_);

    //
    int j = _value_presets.size();

    //
    _value_presets.resize(j + amt.n);

    // shift target note by these
    for (int i = 0; i < amt.n; i++) {
      _value_presets[j].val = amt[i];
      _value_presets[j].str << sprf("Change by %+d notes", (int)amt[i]);
      j++;
    }
  }

  virtual float getValue(int idx, float curr_val)
  {
    if (!idx_within(idx, _value_presets))
      return curr_val;

    //
    float v = _value_presets[idx].val;

    // first menu item sets directly
    if (idx < NUM_ELEMENTS(g_repitch_value_presets_fixed))
      return v;

    // see whether we are matching right channel
    if (curr_val >= 0.7f)
      return limit_range(curr_val + v * (0.3f / 48.0f), 0.7f, 1.0f);

    // otherwise setting directly
    return limit_range(curr_val + v * BlkFxParam::noteFrac(), 0.0f, 0.699f);
  }

  virtual void process(FxState1_0* s)
  {
#ifdef STEREO
    if (s->temp.val >= 0.7f) {
      // bodge so that matching repitch only runs on the left channel
      HarmShiftProcess<1> shift(s, /*repitch*/ true);
      AutoHarmMaskRun(s, shift);
    }
    else
#endif
    {
      HarmShiftProcess<AUDIO_CHANNELS> shift(s, /*repitch*/ true);
      AutoHarmMaskRun(s, shift);
    }
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
#ifdef STEREO
    if (val >= 0.7f)
      return text << sprf("right%+.2fnotes", lin_scale(val, 0.7f, 1.0f, -24, 24));
#endif

    float offs_c0 = val * BlkFxParam::noteSpan();
    return NoteToTxt(text, offs_c0);
  }

  virtual bool ampMixMode() { return true; }
} g_harm_repitch;

//*************************************************************************************************
class ResampleProcess : public AmpProcess {
public:
  FrqShiftFft<AUDIO_CHANNELS> _shift;

  FixPoint<12> _frq_mult;

  // process in reverse??
  bool reverse() { return _shift._buf.getReverseDir(); }

  ResampleProcess(FxState1_0* s)
      : AmpProcess(s)
  {
    // mult freqs by this much
    _frq_mult = powf(2.0f, ParamToOctave(s->temp.val));

    ::init(_shift, _b);
    _shift._buf.setReverseDir(_frq_mult > 1);

    // work out amp scaling
    _shift._dst_amp = _amp;
    _shift._src_amp = limit_range(1.0f - _amp, 0.0f, 1.0f);
  }

  void run(long b0, long b1)
  {
    _shift.run</*CONJ*/ 0>(
        /*src first*/ b0, /*src last*/ b1, /*dst first*/ b0 * _frq_mult, /*dst scale*/ _frq_mult);
  }

  void done() { _shift.flush(); }
};

//-------------------------------------------------------------------------------------------------
class ResampleFx : public FxRun1_0 {
public:
  ResampleFx()
      : FxRun1_0("Resample")
  {
    InitShiftValuePresets(this);
  }

  //
  virtual float getValue(int idx, float curr_val)
  {
    return GetShiftValuePreset(this, idx, curr_val);
  }

  virtual void process(FxState1_0* s)
  {
    ResampleProcess resample(s);
    MaskedRun(s, resample);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    // display octave shift as notes (semitones) shift
    float notes = ParamToOctave(val) * 12.0f;
    return text << sprf("%+.2f notes", notes);
  }

  virtual bool ampMixMode() { return true; }
} g_resample_fx;

//*************************************************************************************************
#ifdef STEREO

//*************************************************************************************************
class VocodeFx
    : public FxRun1_0
// vocoder is a stereo effect
// it uses the frequency envelope from channel 0 (left) to modulate channel 1 (right)
{
public:
  VocodeFx()
      : FxRun1_0("Vocode")
  {
    // standard
    {
      float vals_[] = {1, 10, 20, 40, 100, 200, 300};
      Rng<float> vals = TO_RNG(vals_);
      for (int i = 0; i < vals.n; i++) {
        float v = lin_scale(vals[i], 1, 400, 0, 0.8749f);
        _fillValues(toRng(&v));
      }
    }
    // multiply
    {
      float vals_[] = {0, 25, 50, 75, 100};
      Rng<float> vals = TO_RNG(vals_);
      for (int i = 0; i < vals.n; i++) {
        float v = lin_scale(vals[i], 0, 100, 0.875f, 1.0f);
        _fillValues(toRng(&v));
      }
    }
  }

  static float getSegs(float params_val)
  {
    return min(params_val * (399.0f / .875f) + 1.0f, 400.0f);
  }

  // top 12.5% of value mixes in spectrum multiply
  static float getMultFrac(float params_val) { return max((params_val - 0.875f) * 8.0f, 0.0f); }

  virtual void process(FxState1_0* s)
  {
    DtBlkFx* b = s->_b;

    float amp = s->temp.amp;
    float mult_frac = getMultFrac(s->temp.val);

    // fraction of voc/mult in output
    float wet_frac = min(amp, 1.0f);

    float voc_amp = amp * (1 - mult_frac);
    float voc_mixback = 1 - wet_frac * (1 - mult_frac);

    float mult_amp = amp * mult_frac;
    float mult_mixback = 1 - wet_frac * mult_frac;

    // do vocode (envelope match)
    if (voc_amp > 0.0f) {

      // get src & dst bin ranges
      Array<long, 2> src_b;
      Array<long, 2> dst_b;
      if (s->temp.bin[0] > s->temp.bin[1]) {
        // freqA > freqB, use range to select source
        src_b[0] = s->temp.bin[1];
        src_b[1] = s->temp.bin[0];
        dst_b[0] = 1;
        dst_b[1] = b->_freq_fft_n / 2;
      }
      else {
        // freqB > freqA, use range to select dest
        dst_b = s->temp.bin;
        src_b[0] = 1;
        src_b[1] = b->_freq_fft_n / 2;
      }

      // get number of bins in src & dst
      long dst_n = dst_b[1] - dst_b[0];
      if (!dst_n)
        return;
      long src_n = src_b[1] - src_b[0];
      if (!src_n)
        return;

      // get src & dst data ranges (the range corresponds to each block)
      CplxfPtrPair src;
      src.a = b->FFTdata(0) + src_b[0];

      CplxfPtrPair dst;
      dst.a = b->FFTdata(1) + dst_b[0];
      cplxf* dst_end = b->FFTdata(1) + dst_b[1];

      // get number of segments (arbitrary scaling)
      float n_segs = limit_range(getSegs(s->temp.val), 1.0f, (float)dst_n);

      float fsrc_stp = (float)src_n / n_segs;
      float fdst_stp = (float)dst_n / n_segs;

      float fsrc_b = (float)src_b[0];
      float fdst_b = (float)dst_b[0];

      // run in a loop over the number of segments to match pwr in each dst segment with src
      while (1) {
        // find the src & dst ranges
        fdst_b += fdst_stp;
        fsrc_b += fsrc_stp;

        cplxf* src_next = b->FFTdata(0) + (long)fsrc_b;

        src.b = src_next;
        dst.b = (long)fdst_b + b->FFTdata(1);
        if (dst.b > dst_end)
          break;

        // get the power from the src segment (or from src start bin if empty)
        float src_pwr;
        if (src.equal())
          src_pwr = norm(*src);
        else
          src_pwr = GetPwr(src);

        float dst_in_pwr = GetPwr(dst);
        float dst_scale = MatchPwr(voc_amp, src_pwr, dst_in_pwr) + voc_mixback;

        // scale the dst segment to match the src segment
        for (; !dst.equal(); dst.a++)
          *dst = (*dst) * dst_scale;

        // next segment
        src.a = src_next;
      }
    }

    // multiply mode
    // TODO: make this efficient
    // note, the freq range multiplied will be different to the vocoded range if freqb < freqa

    if (mult_amp > 0.0f) {
      // lerp mix between dst & dst*src based on mixback

      Array<long, 2> bin = s->temp.bin;
      if (bin[0] > bin[1])
        swap(bin[0], bin[1]);
      CplxfPtrPair ch0_(b->FFTdata(0), bin[0], bin[1] + 1), ch0;
      cplxf* ch1_ = b->FFTdata(1) + bin[0];
      cplxf* ch1;

      // pass1: ch0 = ch0*ch1
      float ch0_out_pwr = 0.0f;

      // ch0 input power processed
      float mult_ch0_in_pwr = 0.0f;

      for (ch0 = ch0_, ch1 = ch1_; !ch0.equal(); ch0.a++, ch1++) {
        mult_ch0_in_pwr += norm(*ch0);
        (*ch0) = (*ch0) * (*ch1);
        ch0_out_pwr += norm(*ch0);
      }

      // pass2: power match to ch0 in pwr & mix to output
      float ch0_scale = MatchPwr(mult_amp, mult_ch0_in_pwr, ch0_out_pwr);
      for (ch0 = ch0_, ch1 = ch1_; !ch0.equal(); ch0.a++, ch1++)
        (*ch1) = ch0_scale * (*ch0) + mult_mixback * (*ch1);
    }

    // copy the dst back to channel 0 (src) and make the output power come from channel 0
    memcpy(b->FFTdata(0), b->FFTdata(1), (b->_freq_fft_n / 2 + 1) * sizeof(cplxf));
    b->_chan[1].total_in_pwr = b->_chan[0].total_in_pwr;
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    float mult = getMultFrac(val);
    if (mult > 0.0f)
      return text << spr_percent(mult) << " multiply";
    return text << RndToInt(getSegs(val)) << " segs";
  }

  virtual bool ampMixMode() { return true; }
} g_vocode;

//*************************************************************************************************
class HarmMatch2Process : public AmpProcess {

public:
  // our parent must be HarmMaskProcess
  HarmMaskProcess<HarmMatch2Process>* _parent;

  // mixback amp
  float _mix_back;

  // max FFT bin
  long _max_bin;

  int _src_ch, _dst_ch;

public:
  // expect to be filled in by caller
  HarmParam _src_harm;

  HarmMatch2Process(FxState1_0* s, int src_ch, int dst_ch)
      : AmpProcess(s)
  {
    _src_ch = src_ch;
    _dst_ch = dst_ch;

    // max FFT bin
    _max_bin = _b->_freq_fft_n / 2;

    // determine whether we need to mix or replace
    _mix_back = max(1.0f - _amp, 0.0f);
  }

  void prepare(HarmMaskProcess<HarmMatch2Process>* parent)
  {
    AmpProcess::prepare(parent);
    _parent = parent;
  }

  void run(long b0, long b1)
  // scale the power of the destination harmonic to match the src harmonic
  {
    //
    long curr_harm = (long)_parent->_curr_harm;
    if (curr_harm < 0)
      return;

    // power in destination harmonic
    float dst_in_pwr = GetPwr(_b->FFTdata(_dst_ch), b0, b1);

    // determine src harmonic
    long s0, s1;
    s0 = (long)(_src_harm._offs + curr_harm * _src_harm._spacing) - _src_harm._half_width;
    s1 = s0 + _src_harm._width;
    if (s0 < 1)
      s0 = 1;
    if (s1 > _max_bin)
      s1 = _max_bin;

    // get src harmonic pwr
    float src_pwr = dst_in_pwr;
    if (s1 >= s0)
      src_pwr = GetPwr(_b->FFTdata(_src_ch), s0, s1);

    // attempt to match dst pwr to src pwr
    float scale = MatchPwr(_amp, src_pwr, dst_in_pwr) + _mix_back;

    // scale the destination data
    for (CplxfPtrPair x(_b->FFTdata(_dst_ch), b0, b1 + 1); !x.equal(); x.a++)
      *x.a = (*x.a) * scale;
  }
};

//-------------------------------------------------------------------------------------------------
class HarmMatch2Fx : public FxRun1_0 {
public:
  bool _RL_mode;
  HarmMatch2Fx(const char* name, bool RL_mode)
      : FxRun1_0(name)
  {
    InitHarmValuePresets(this);
    _RL_mode = RL_mode;
  }

  virtual void process(FxState1_0* s)
  {
    // RL_mode=0: src ch is left, dst ch is right
    // RL_mode=1: src ch is right, dst ch is left
    DtBlkFx* b = s->_b;

    int src_ch = 0, dst_ch = 1;
    if (_RL_mode)
      swap(src_ch, dst_ch);

    HarmMatch2Process match(s, src_ch, dst_ch);

    // find peaks in src & dst data
    Array<long, 2> dst_bin = s->temp.bin, src_bin;
    if (dst_bin[0] > dst_bin[1])
      swap(dst_bin[0], dst_bin[1]);
    src_bin = dst_bin;

    float fx_val = s->temp.val;

    // chk whether the previous effect is a harmonic mask, if so use its params to control the
    // source
    FxState1_0* prev_s = s->prevFxState();
    if (prev_s) {
      if (getHarmMaskFreqMult(prev_s->temp.fft_fx, /*out*/ match._src_harm._multiply)) {
        // use previous mask as src bin control
        src_bin = prev_s->temp.bin;
        if (src_bin[0] > src_bin[1])
          swap(src_bin[0], src_bin[1]);
        fx_val = prev_s->temp.val;
      }
    }

    float src_peak =
        PeakFindFft(b->FFTdata(src_ch), src_bin[0], src_bin[1], /*estimate fundamental*/ 5.0f);
    match._src_harm.init(fx_val, src_peak);

    float dst_peak =
        PeakFindFft(b->FFTdata(dst_ch), dst_bin[0], dst_bin[1], /*estimate fundamental*/ 5.0f);
    HarmMaskProcess<HarmMatch2Process> mask(s, match, s->temp.val, dst_peak);
    SplitMaskRun(s, mask);

    // copy dst data back to src (both channels are now "dst")
    memcpy(b->FFTdata(src_ch), b->FFTdata(dst_ch), (b->_freq_fft_n / 2 + 1) * sizeof(cplxf));

    // always match overall pwr to left ch (no matter which is src & dst)
    b->_chan[1].total_in_pwr = b->_chan[0].total_in_pwr;
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    return HarmDispVal(text, val);
  }

  virtual bool ampMixMode() { return true; }
} g_harmmatchLR_fx("HarmMatchLR", /*R-L mode*/ false),
    g_harmmatchRL_fx("HarmMatchRL", /*R-L mode*/ true);

//*************************************************************************************************
struct CrossMixProcess
    : public AmpProcess
//
// do a convolving mix between left & right channels
//
{
  // which channels is src & dst
  int _src_ch, _dst_ch;

  // number of bins processed
  int _bins_processed;

  //
  float _mix_src;
  float _mix_dst;
  float _raise_src;
  float _raise_dst;

  CrossMixProcess(FxState1_0* s)
      : AmpProcess(s)
  {
    _bins_processed = 0;

    SplitParam<2> val(s->temp.val);
    _src_ch = 0;
    _dst_ch = 1;
    if (val.i_part)
      swap(_src_ch, _dst_ch);

    _mix_dst = val.f_part;
    _mix_src = 1.0f - _mix_dst;

    // each bin is processed like this: |v| ^ 2 ^ raise * .5 = |v| ^ raise
    // raise is between 0 & 1
    //
    float bias = 1.6f;
    _raise_dst = limit_range(bias * val.f_part, 0.0f, 1.0f) * .5f;
    _raise_src = limit_range(bias * (1 - val.f_part), 0.0f, 1.0f) * .5f;
  }

  void run(long b0, long b1)
  {
    _bins_processed = b1 - b0 + 1;

    cplxf* src = _b->FFTdata(_src_ch) + b0;
    CplxfPtrPair dst_(_b->FFTdata(_dst_ch), b0, b1 + 1);
    CplxfPtrPair dst;

    float src_pwr = 0.0f;
    float dst_pwr = 0.0f;
    float mix_pwr_temp = 0.0f;
    for (dst = dst_; !dst.equal(); src++, dst.a++) {
      float norm_src = norm(*src);
      float norm_dst = norm(*dst);
      src_pwr += norm_src;
      dst_pwr += norm_dst;
      *dst = polar_to_cplxf(powf(norm_src, _raise_src) * powf(norm_dst, _raise_dst),
                            angle(*src) * _mix_src + angle(*dst) * _mix_dst);
      mix_pwr_temp += norm(*dst);
    }

    // power match output for segment
    float mix_pwr = src_pwr * _mix_src + dst_pwr * _mix_dst;
    MatchPwr(_amp, /*desired*/ mix_pwr, /*current*/ mix_pwr_temp, /*data*/ dst_);
  }

  void done()
  {
    // determine power correction by linearly interpolating using mix ratio and number of bins
    // processed
    float& total_src_in_pwr = _b->_chan[_src_ch].total_in_pwr;
    float& total_dst_in_pwr = _b->_chan[_dst_ch].total_in_pwr;

    float mix_pwr = _mix_src * total_src_in_pwr + _mix_dst * total_dst_in_pwr;
    float proc_frac = (float)_bins_processed / (float)(_b->_freq_fft_n / 2 + 1);
    total_dst_in_pwr = lin_interp(proc_frac, total_dst_in_pwr, mix_pwr);

    // make both channels have the same data
    memcpy(_b->FFTdata(_src_ch), _b->FFTdata(_dst_ch), (_b->_freq_fft_n / 2 + 1) * sizeof(cplxf));
    total_src_in_pwr = total_dst_in_pwr;
  }
};

//-------------------------------------------------------------------------------------------------
class CrossMixFx : public FxRun1_0 {
public:
  CrossMixFx()
      : FxRun1_0("CrossMix")
  {
    _fillValues(5, 0, 0.4999f);
    _fillValues(5, 0.5f, 1.0f);
  }

  virtual void process(FxState1_0* s)
  {
    CrossMixProcess mix(s);
    MaskedRun(s, mix);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    SplitParam<2> sval(val);
    return text << spr_percent(1 - sval.f_part) << (sval.i_part ? "R on L" : "L on R");
  }
} g_crossmix_fx;

//*************************************************************************************************
struct WeirdMixProcess
    : public AmpProcess
//
//
{
  // mode 0 or mode 1 mixing
  int _mode;

  // mix ratio of Left to Right: 0=full left, 1=full right
  float _mix_ratio;

  // power scale to apply for morph mode 0
  float _val_pwr_scale;

  // number of segments for morph mode 1
  float _num_segs;

  // number of bins processed
  int _bins_processed;

  WeirdMixProcess(FxState1_0* s)
      : AmpProcess(s)
  {
    _bins_processed = 0;

    SplitParam<2> sval(s->temp.val);
    _mix_ratio = sval.f_part;
    _mode = sval.i_part;

    // mode 0 mixing, power scale
    _val_pwr_scale = 1e8f / _b->_freq_fft_n;

    // mode 1 mixing, number of segments
    _num_segs = 4;

    if (_amp < 1.0f) {
      _amp *= _amp;
      _val_pwr_scale *= _amp;
      _num_segs = lin_interp(_amp, 1, _num_segs);
      _amp = 1.0f;
    }
  }

  void runMode0(int n_bins, cplxf* data_in[], cplxf* data_out)
  // internal run processing
  {
    //

    // curve "length" normalizing multipliers (originally this really was length but now it's
    // something else)
    float norm_len_mul[2];
    for (int i = 0; i < 2; i++) {
      // find total integral length of curve
      float total_len = 0;
      CplxfPtrPair curr(data_in[i], n_bins);

      cplxf prv = *curr.a;
      float pwr = norm(prv);
      while (++curr.a < curr.b) {
        total_len += logf(_val_pwr_scale * norm(*curr.a - prv) + 2.7183f);
        prv = *curr.a;
        pwr += norm(prv);
      }

      // turn length into a multiplier
      if (total_len <= 0)
        total_len = 1;
      norm_len_mul[i] = 1 / total_len;
    }

    cplxf
        // current value delta from previous point for each input curve
        val_delta[2] = {cplxf(0, 0), cplxf(0, 0)},

        // current input array value
        val[2] = {data_in[/*ch*/ 0][/*idx*/ 0], data_in[/*ch*/ 1][/*idx*/ 0]},

        // current & prv lerp'd values
        lerp_val(0, 0), prv_lerp_val(0, 0);

    CplxfPtrPair curr_in[2] = {data_in[0], data_in[1]}, curr_out = data_out;

    curr_in[0].b += n_bins;
    curr_in[1].b += n_bins;
    curr_out.b += n_bins;

    float
        // current index into each input array
        idx[2] = {0, 0},

        // cumulative intput length
        len[2] = {0, 0},

        // line seg multiplier for lerping onto curve "b"
        line_seg_mul[2] = {0, 0},

        // current & prv lerp'd mixed values
        lerp_idx = -1, prv_lerp_idx = -1,

        // current output index
        out_idx = 0;

    // do all the lerping
    while (!curr_out.equal()) {
      while (lerp_idx <= out_idx) {
        int a; // channel to prcoess
        float ratio;

        // which curve should we use as curve "a"?
        if (len[0] < len[1]) {
          a = 0;
          ratio = _mix_ratio;
        }
        else {
          a = 1;
          ratio = 1 - _mix_ratio;
        }
        int b = 1 - a;

        // lerp corresponding point on curve "b"
        float frac = (len[b] - len[a]) * line_seg_mul[b];
        float idx_b = idx[b] - frac;
        cplxf val_b = val[b] - frac * val_delta[b];

        // lerp between curves a & b to get the mixed value
        prv_lerp_idx = lerp_idx;
        prv_lerp_val = lerp_val;
        lerp_idx = lin_interp(ratio, idx[a], idx_b);
        lerp_val = lin_interp(ratio, val[a], val_b);

        // stop if we've reached the end of curve "a"
        if (curr_in[a].equal())
          break;

        // move to the next point on curve "a"
        idx[a]++;
        curr_in[/*ch*/ a].a++;

        // get current data point from curve "a"
        cplxf cur = *curr_in[a].a;

        // y-axis difference between new point & previous
        val_delta[a] = cur - val[a];

        // "distance" between this point & previous
        float dl = logf(_val_pwr_scale * norm(val_delta[a]) + 2.7183f) * norm_len_mul[a];
        len[a] += dl;

        // update value with current value
        val[a] = cur;

        // determine lerping multiplier when mapping a point onto this curve
        if (dl > 0)
          line_seg_mul[a] = 1 / dl;
      }

      // lerp between prv & curr points to get final output
      cplxf out;
      if (lerp_idx != prv_lerp_idx) {
        float frac = (lerp_idx - out_idx) / (lerp_idx - prv_lerp_idx);
        out = lin_interp(frac, lerp_val, prv_lerp_val) * AmpProcess::_amp;
      }
      else
        out = lerp_val * AmpProcess::_amp;

      *curr_out.a++ = out;
      out_idx++;
    }
  }

  void runMode1(int n_bins, cplxf* data_in[], cplxf* data_out)
  // run called by the framework
  {
    int i;

    // work out power of spectrum segment & find pwr per segment to process
    float pwr_per_seg[/*channel*/ 2] = {0, 0};
    for (/*channel*/ i = 0; i < 2; i++) {
      float pwr = GetPwr(data_in[i], n_bins);

      // work out power per segment that we'll stretch & mix
      pwr_per_seg[i] = pwr / _num_segs;
    }

    // adjust mix ratio if one/both channels are below the minimum power value
    const float min_val = 1e-10;
    float mix_limit[2] = {0, 1};
    if (pwr_per_seg[0] < min_val)
      mix_limit[0] = lin_interp(pwr_per_seg[0] / min_val, 1.0f, 0.0f);
    if (pwr_per_seg[1] < min_val)
      mix_limit[1] = lin_interp(pwr_per_seg[1] / min_val, 0.0f, 1.0f);
    if (mix_limit[1] < mix_limit[0])
      swap(mix_limit[0], mix_limit[1]);

    float mix_ratio = lin_interp(_mix_ratio, mix_limit);

    // final bin to process from input
    cplxf* end_in[/*channel*/ 2] = {data_in[0] + n_bins, data_in[1] + n_bins};

    // current sample of input segment being processed
    CplxfPtrPair seg_in[/*channel*/ 2] = {data_in[0], data_in[1]};

    // current output bin
    int dst_bin = 0;

    // phase correction when stretching
    PhaseCorrect phase;
    phase.init(_b);

    while (dst_bin < n_bins) {
      // work out segment end for each channel based on power
      for (/*channel*/ i = 0; i < 2; i++) {

        // find end point based on segment power
        float acc_pwr = 0;
        while (acc_pwr <= pwr_per_seg[i] && seg_in[i].b < end_in[i])
          acc_pwr += norm(*seg_in[i].b++);
      }

      // find the interpolated stop dst bin
      int dst_stop_bin = RndToInt(lin_interp(
          mix_ratio, (float)(seg_in[0].b - data_in[0]), (float)(seg_in[1].b - data_in[1])));

      // make sure that at least one output bin is processed
      if (dst_stop_bin <= dst_bin)
        dst_stop_bin = dst_bin + 1;

      // work out step size for each channel
      float dst_len_f = (float)(dst_stop_bin - dst_bin) + 1, src_bin_f[/*channel*/ 2],
            in_stp[/*channel*/ 2];

      for (/*channel*/ i = 0; i < 2; i++) {
        src_bin_f[i] = (float)(seg_in[i].a - data_in[i]);
        in_stp[i] = (float)seg_in[i].size() / dst_len_f;
      }

      // do the scale & mix
      for (; dst_bin < dst_stop_bin; dst_bin++) {

        // accumulate
        cplxf t[/*channel*/ 2] = {0, 0};
        for (/*channel*/ i = 0; i < 2; i++) {
          src_bin_f[i] += in_stp[i];
          cplxf* end = data_in[i] + RndToInt(src_bin_f[i]);
          int shift = dst_bin - (int)(seg_in[i].a - data_in[i]);
          while (seg_in[i].a < end)
            t[i] += *seg_in[i].a++ * phase(shift--);
        }

        cplxf v = lin_interp(mix_ratio, t[0], t[1]) * AmpProcess::_amp;
        *data_out++ = v;
      }
    }
  }

  void run(long b0, long b1)
  {
    int n_bins = b1 - b0 + 1;
    _bins_processed += n_bins;

    cplxf* fft_data[2] = {_b->FFTdata(0) + b0, _b->FFTdata(1) + b0};
    cplxf* temp_buf = _b->_chan[0].x2.cast<cplxf>();

    if (_mode)
      runMode1(n_bins, fft_data, temp_buf);
    else
      runMode0(n_bins, fft_data, temp_buf);

    // copy the data processed back to both channels
    memcpy(fft_data[0], temp_buf, n_bins * sizeof(cplxf));
    memcpy(fft_data[1], temp_buf, n_bins * sizeof(cplxf));
  }

  void done()
  // done() called by the frame work
  // adjust total power based for channels based on mix ratio and number of bins processed
  {
    float& in_pwr0 = _b->_chan[0].total_in_pwr;
    float& in_pwr1 = _b->_chan[1].total_in_pwr;

    float mix_pwr = lin_interp(_mix_ratio, in_pwr0, in_pwr1);
    float proc_frac = (float)_bins_processed / (float)(_b->_freq_fft_n / 2 + 1);
    in_pwr0 = lin_interp(proc_frac, in_pwr0, mix_pwr);
    in_pwr1 = lin_interp(proc_frac, in_pwr1, mix_pwr);
  }
};

//-------------------------------------------------------------------------------------------------
class WarpMixFx : public FxRun1_0 {
public:
  WarpMixFx()
      : FxRun1_0("WarpMix")
  {
    _fillValues(5, 0, 0.4999f);
    _fillValues(5, 0.5f, 1.0f);
  }

  virtual void process(FxState1_0* s)
  {
    WeirdMixProcess mix(s);
    MaskedRun(s, mix);
  }

  virtual Rng<char> /*updated*/ dispVal(FxState1_0*, Rng<char> /*out*/ text, float /*0..1*/ val)
  {
    SplitParam<2> sval(val);
    return text << RndToInt(sval.f_part * 100.0f) << " mode " << sval.i_part;
  }

  // actually distortion amount
  virtual bool ampMixMode() { return true; }

} g_warpmix_fx;

#endif

//*************************************************************************************************
FxRun1_0* g_fft_fx_table[] = {&g_filter_fx,
                              &g_contrast_fx,
                              &g_smear_fx,
                              &g_thresh_fx,
                              &g_clip_fx,
                              &g_resize_fx,
                              &g_resample_fx,
                              &g_shift_fx,
                              &g_const_shift_fx,
                              &g_no_fx,
                              &g_no_fx,
                              &g_harm_shift,
                              &g_harm_repitch,
                              &g_harm_filt_fx,
                              &g_auto_harm_fx,
                              &g_sweep1_fx,
                              &g_sweep2_fx,
                              &g_sweep3_fx,
                              &g_sweep4_fx,
                              &g_sweep5_fx,
                              &g_harm_mask,
                              &g_auto_harm_mask,
                              &g_auto_subharm1_mask,
                              &g_auto_subharm2_mask,
                              &g_auto_subharm3_mask,
                              &g_thresh_mask,
#ifdef STEREO
                              &g_vocode,
                              &g_harmmatchLR_fx,
                              &g_harmmatchRL_fx,
                              &g_crossmix_fx,
                              &g_warpmix_fx
#endif
};

const int g_num_fx_1_0 = NUM_ELEMENTS(g_fft_fx_table);

FxRun1_0* GetFxRun1_0(int idx)
{
  if (idx < 0 || idx >= g_num_fx_1_0)
    return &g_no_fx;
  return g_fft_fx_table[idx];
}

//*************************************************************************************************
