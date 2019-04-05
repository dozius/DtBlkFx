#ifndef _FX_RUN_1_0_H_
#define _FX_RUN_1_0_H_
/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

I am trying to move away from the 1.0 style effects because they were developed prior to param
morphing, can't store effect specific params and have become clumsy to deal with

FxRun1_0 : run time object to do the effect, one object per effect
FxRun only exists during the processing of a block. It has no data that persists between blocks.


History
        Date       Version    Programmer         Comments
        16/2/03    1.0        Darrell Tam		Created


This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

***************************************************************************************************/

#include "BlkFxParam.h"
#include "MorphParam.h"
#include <string.h>
#include <vector>

class DtBlkFx;
class FxState1_0;

// number of 1.0 effects
extern const int g_num_fx_1_0;

//-------------------------------------------------------------------------------------------------
class FxRun1_0
// 1.0 effects were stateless between runs and processed via this class, one instance per effect
{
protected:
  // effect name
  const char* _name;

  // whether a particular fx set param is used by this effect
  bool _params_used[BlkFxParam::NUM_FX_PARAMS];

public:
  // construct with effect name
  FxRun1_0(const char* name = "?")
  {
    _name = name;

    // default to all params being used
    for (int i = 0; i < BlkFxParam::NUM_FX_PARAMS; i++)
      _params_used[i] = true;
  }

  virtual ~FxRun1_0() {}

  // DtBlkFx calls this to perform the FFT effect
  virtual void process(FxState1_0* s) {}

  // get the display text corresponding to "val"
  virtual Rng<char> /*updated rng*/ dispVal(FxState1_0* s, Rng<char> /*out*/ text,
                                            float /*0..1*/ val)
  {
    return text << "-";
  }

  // get the effect name
  virtual const char* name() { return _name; }

  // return whether a particular FX_SET parameter is used (0..NUM_FX_PARAMS-1)
  virtual bool paramUsed(int param_idx) { return _params_used[param_idx]; }

  // return true if amp is used as a mix between instead of -inf to 0 dB but normal after that
  // default is amp is dB all the time
  virtual bool ampMixMode() { return false; }

public: // methods for the GUI
  // is this a mask effect or a normal?
  virtual bool isMask() { return false; }

  //
  virtual const char* getValueName(int idx)
  {
    if (!idx_within(idx, _value_presets))
      return NULL;
    return _value_presets[idx].str;
  }

  //
  virtual float getValue(int idx, float curr_val)
  {
    return idx >= (int)_value_presets.size() ? curr_val : _value_presets[idx].val;
  }

public: // internal stuff to do with value presets
  struct ValuePreset {
    float val;
    CharArray<64> str;
  };

  // internal method that uses display value to fill presets
  void _fillValues(int num_presets, float out_min = 0.0f, float out_max = 1.0f,
                   bool rng_inclusive = true);

  void _fillValues(Rng<float> values);

  //
  std::vector<ValuePreset> _value_presets;
};

// get fx run corresponding to "idx"
FxRun1_0* GetFxRun1_0(int idx);

#endif
