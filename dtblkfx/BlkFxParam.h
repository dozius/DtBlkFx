#ifndef _DT_BLKFX_PARAM_H_
#define _DT_BLKFX_PARAM_H_
/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

BlkFxParam : namespace to help with VST parameters used by DtBlkFx

History
        Date       Version    Programmer         Comments
        01/12/05    1.0        Darrell Tam		Created


This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

***************************************************************************************************/

#include "NoteFreq.h"
#include "misc_stuff.h"
#include "rfftw_float.h"
#include <math.h>

// namespace to hold stuff to do with vst params used in dt blkfx
namespace BlkFxParam {

// parameter values
typedef enum {
  // global params that apply to all fx
  MIX_BACK = 0, // original mix back fraction & power match control
  DELAY,        // delay units is 1/4 * signature * bpm
  FFT_LEN,      // fft len is
  OVERLAP,      // overlap fraction
  NUM_GLOBAL_PARAMS,

  // each set of fx has these params (add i*NUM_FX+NUM_GLOBAL)
  FX_FREQ_A = 0,
  FX_FREQ_B,
  FX_AMP,
  FX_TYPE,
  FX_VAL,
  NUM_FX_PARAMS,

  NUM_FX_SETS = 8,
  TOTAL_NUM = NUM_GLOBAL_PARAMS + NUM_FX_PARAMS * NUM_FX_SETS,

  // number of ticks per beat
  TICKS_PER_BEAT = 16,

#ifdef STEREO
  AUDIO_CHANNELS = 2
#elif MONO
  AUDIO_CHANNELS = 1
#else
#  error define STEREO or MONO
#endif
} constants;

// get the parameter offset given an fx set
inline int paramOffs(int fx_set)
{
  return NUM_GLOBAL_PARAMS + fx_set * NUM_FX_PARAMS;
}

struct SplitParamNum {
  int param;      // original param number
  bool ok;        // true if param is in range
  int glob_param; // global param number or -1 if effect set
  int fx_set;     // effect set or -1 if global
  int fx_param;   // effect param or -1 if global

  SplitParamNum(int param_num)
  {
    param = param_num;
    fx_param = -1;
    glob_param = -1;
    fx_set = -1;
    ok = false;
    if (param_num < 0 || param_num >= TOTAL_NUM)
      return;

    ok = true;
    fx_set = param_num - NUM_GLOBAL_PARAMS;
    if (fx_set < 0) {
      // global param
      fx_set = -1;
      glob_param = param_num;
    }
    else {
      // param belongs to effect set
      fx_param = fx_set % NUM_FX_PARAMS;
      fx_set = limit_range(fx_set / NUM_FX_PARAMS, 0, NUM_FX_SETS - 1);
    }
  }
};

// --- MIXBACK
// ---------------------------------------------------------------------------------------------
inline float getPwrMatch(float /*0..1*/ mixback_param)
// get pwr match part of mixback param
{
  return mixback_param <= 0.5f ? 1.0f : 0.0f;
}

inline float getMixBackFrac(float /*0..1*/ mixback_param)
// get the mixback fraction of the mixback param
{
  return limit_range(mixback_param <= 0.5f ? mixback_param * 2.0f
                                           : (1.0f - mixback_param) * (1.0f / 0.499f),
                     0.0f,
                     1.0f);
}

inline float /*0..1*/ getMixbackParam(float /*0..1*/ mixback_frac, bool pwr_match)
// combine pwr match and mixback fraction into overlap param
{
  return pwr_match ? limit_range(mixback_frac * 0.5f, 0.0f, 0.5f)
                   : limit_range(1.0f - mixback_frac * 0.499f, 0.501f, 1.0f);
}

// --- DELAY
// -----------------------------------------------------------------------------------------------
struct Delay
    : public SplitParam<2>
// f_part is converted to delay units (beats or msec)
{
  // possible values for i_part
  typedef enum { BEATS = 0, MSEC = 1 } Units;

  // split param
  Delay(float /*0..1*/ param = 0.0f) { operator()(param); }

  // construct using amount
  Delay(Units units, float amount) { setAmount(units, amount); }

  // set param in given beats or msec
  void setAmount(Units units, float amount /* beats or msec */)
  {
    setUnits(units);
    f_part = limit_range(
        amount * (i_part == MSEC ? 1.0f / 6000.0f : 2.0f * TICKS_PER_BEAT / 255.0f), 0.0f, 1.0f);
  }

  // return fractional part as beats or msec (depending on i_part)
  float /*beats or msec*/ getAmount() const
  {
    return f_part * (i_part == MSEC ? 6000.0f : 0.5f * 255.0f / TICKS_PER_BEAT);
  }

  // get/set i_part in units
  Units getUnits() const { return (Units)i_part; }
  void setUnits(Units new_units) { i_part = (int)new_units; }

  //
  static Delay beats(float /*beats*/ v) { return Delay(BEATS, v); }
  static Delay msec(float /*msec*/ v) { return Delay(MSEC, v); }
};

// --- FFT_LEN
// ---------------------------------------------------------------------------------------------
inline long /*plan index*/ getPlan(float /*0..1*/ fft_len_param)
// given vst param for fft_len, return fft plan number
{
  // scaling is compatible with 1.0 version
  return limit_range((int)(fft_len_param * 255.0f / 4.0f - 1.5f), 0, NUM_FFT_SZ - 1);
}

inline float /*0..1*/ getFFTLenParam(int plan_index)
// turn plan index into fft length
{
  // scaling is compatible with 1.0 version
  return limit_range(((float)plan_index + 2.0f) * (4.0f / 255.0f), 0.0f, 1.0f);
}

// --- OVERLAP
// ---------------------------------------------------------------------------------------------
inline bool getBlkSync(float /*0..1*/ overlap_param)
// get blk sync part of the overlap param
{
  return overlap_param > 0.5f;
}

inline float getOverlapPart(float /*0..1*/ overlap_param)
// get the overlap part of the overlap param
{
  return overlap_param > 0.5f ? 2.0f - overlap_param * 2.0f : overlap_param * 2.0f;
}

inline long getBlkShiftFwd(float /*0..1*/ overlap_param, int blk_len)
// return number of samples we will shift forward in the blk given the overlap_param
{
  long fwd_n = (long)lin_interp(overlap_param,
                                (float)(blk_len - 16), // overlap=0: maximum shift
                                (float)blk_len * .15f  // overlap=1: mimimum shift
  );
  if (fwd_n < 32)
    fwd_n = 32;
  return fwd_n;
}

inline float /*0..1*/ getOverlapParam(float /*0..1*/ overlap_part, bool sync)
//
// combine overlap part and sync into overlap param
//
// overlap_part units are 0=approx 0%, 1=approx 85% (depends on blk sz)
//
{
  return sync ? limit_range((2.0f - overlap_part) * 0.5f, 0.501f, 1.0f)
              : limit_range(overlap_part * 0.5f, 0.0f, 0.499f);
}

// --- FX_FREQ_A/FX_FREQ_B --------------------------------------------------------------------
// freq params correspond to exponential frequency which means that octaves are
// linear and the param is actually a note offset from c0

// number of notes spanned in freq param
inline float noteSpan()
{
  return 255.0f * 0.5f;
}

// number of octaves spanned in freq param
inline float octaveSpan()
{
  return noteSpan() / 12.0f;
}

// fraction of freq param corresponding to 1 octave
inline float octaveFrac()
{
  return 1.0f / octaveSpan();
}

// fraction of freq param corresponding to 1 note
inline float noteFrac()
{
  return 1.0f / noteSpan();
}

inline float /*freq hz*/ getHzNonZero(float /*0..1*/ param)
// map the vst param to a freq (with no 0 hz)
{
  return NoteOffsToHz(/*offs c0*/ param * noteSpan());
}

inline float /*freq hz*/ getHz(float /*0..1*/ param)
// if param is 0 then return 0, otherwise return "getFreqNonZero"
{
  return param <= 0.0f ? 0.0f : getHzNonZero(param);
}

inline void genPixelToHz(Rng<float> /*out*/ dst_data)
{
  ValStp<float> param(0.0f, 1.0f / (float)dst_data.n);
  for (int i = 0; i < (int)dst_data.n; i++) {
    dst_data[i] = BlkFxParam::getHz(param);
    param.next();
  }
}

// --- FX_TYPE --------------------------------------------------------------------------------
inline long /*index*/ getEffectType(float /*0..1*/ param)
{
  // align to renoise effect values, allow for up to 32 possible effects
  return (long)(param * 255.0) / 8;
}

// inverse function to return the effect type param given an index
inline float /*param 0..1*/ getEffectTypeInv(long index)
{
  return (float)(index * 8 + 4) / 255.0f;
}

// --- FX_AMP ---------------------------------------------------------------------------------
inline float /*dB*/ getEffectAmp(float /*0..1*/ param)
{
  return lin_interp(param, -60.0f, 40.0f);
}

// turn db into param
inline float /*0..1*/ getAmpParam(float db /*-60 .. 40*/)
{
  return limit_range(db * 0.01f + 0.6f, 0.0f, 1.0f);
}

// get 0dB point of amp param
inline float getAmpParam0dB()
{
  return getAmpParam(0);
}

inline float /*multiply*/ getEffectAmpMult(float /*0..1*/ param, bool mix_mode)
// convert amp param into linear multiplier (i.e. param->dB->linear)
// if mix_mode is true and amp is less than 0dB point, linear
{
  // check for mix_mode linear range
  if (mix_mode && param < getAmpParam0dB())
    return param * (1.0f / getAmpParam0dB());

  // normal exponential range
  return param <= 0.0f ? 0.0f : powf(10.0f, getEffectAmp(param) * 0.05f);
}

}; // namespace BlkFxParam

#endif
