#ifndef _DT_BLK_FX_H_
#define _DT_BLK_FX_H_
/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Effect main class

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

#include <vector>
#include <vstsdk/public.sdk/source/vst2.x/audioeffectx.h>

#include "BlkFxParam.h"
#include "FxState1_0.h"
#include "MorphParam.h"
#include "ParamsDelay.h"
#include "VstProgram.h"
#include "misc_stuff.h"

class Gui;

//------------------------------------------------------------------------
class DtBlkFx : public AudioEffectX {
public:
  enum { MAX_FX = 16, AUDIO_CHANNELS = BlkFxParam::AUDIO_CHANNELS };

  DtBlkFx(audioMasterCallback audioMaster);
  ~DtBlkFx();

public: // override AudioEffect methods
  virtual void process(float** inputs, float** outputs, VstInt32 sampleframes);
  virtual void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
  virtual void setBlockSize(VstInt32 samps);

  virtual void setProgram(VstInt32 program);
  virtual void setProgramName(char* name) { currProgram().setName(name); }
  virtual void getProgramName(char* name) { strcpy(name, currProgram().getName()); }
  virtual bool getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text);

  virtual VstInt32 getChunk(void** data, bool isPreset = false);
  virtual VstInt32 setChunk(void* data, VstInt32 byteSize, bool isPreset = false);

  virtual void setParameter(VstInt32 index, float value);
  virtual float getParameter(VstInt32 index);
  virtual void getParameterLabel(VstInt32 index, char* label);
  virtual void getParameterDisplay(VstInt32 index, char* text);
  virtual void getParameterName(VstInt32 index, char* text);

  virtual void resume();
  virtual void suspend();

  virtual bool getEffectName(char* name);
  virtual bool getVendorString(char* text);
  virtual bool getProductString(char* text);
  virtual VstInt32 getVendorVersion() { return 1200; }

  virtual VstPlugCategory getPlugCategory() { return kPlugCategEffect; }

public: // our own methods
  // cast the editor to our gui
  Gui* gui() { return (Gui*)/*AudioEffect::*/ editor; }

  // get a global param for display
  bool getParamDisplayGlobal(BlkFxParam::SplitParamNum& p, float v, CharRng text);

  // return the bin index from param value
  float /*bin*/ getFFTBin(float /*0..1*/ param)
  {
    return limit_range(
        _freq_fft_n * BlkFxParam::getHz(param) / sampleRate, 0.0f, _freq_fft_n * 0.5f);
  }

  // return fixed-position fft buffer (with offset to allow for shift overrun)
  cplxf* FFTdata(int ch) { return _chan[ch].x1 + 32; }

  //
  template <int CHANNELS> VecPtr<cplxf, CHANNELS> _FFTdata()
  {
    VecPtr<cplxf, CHANNELS> fft;
    for (int i = 0; i < CHANNELS; i++)
      fft.data[i] = FFTdata(i);
    return fft;
  }

  // treat "x2" as a temporary
  template <int CHANNELS> VecPtr<cplxf, CHANNELS> _FFTdataTmp()
  {
    VecPtr<cplxf, CHANNELS> fft_tmp;
    for (int i = 0; i < CHANNELS; i++)
      fft_tmp.data[i] = _chan[i].x2.cast<cplxf>() + 32; // allow room for FrqShiftFft overrun
    return fft_tmp;
  }

  VecPtr<cplxf, AUDIO_CHANNELS> FFTdata() { return _FFTdata<AUDIO_CHANNELS>(); }
  VecPtr<cplxf, AUDIO_CHANNELS> FFTdataTmp() { return _FFTdataTmp<AUDIO_CHANNELS>(); }

  // for debugging
  bool chkInRng(int ch, cplxf* p, int& offs)
  {
    offs = p - _chan[ch].x1;
    return offs >= 0 && offs <= _freq_fft_n / 2;
  }

  // reduce fft plan to be just big for "avail_samps"
  static long reducePlan(long plan /*start*/, long avail_samps);

  // guess what the current FFT length will be (for display purposes)
  void guessFFTLen(int& /*return*/ freq_fft_n, int& /*return*/ time_fft_n);

  // guess what a frequency will be rounded to (for display purposes)
  float /*hz*/ guessRoundHz(float freq_hz, float bin_adjust_frac = 0.0f);

  //
  long /*samples*/ getDelaySamps(const BlkFxParam::Delay& delay);

  //
  float getSampsPerBeat() { return _samps_per_beat; }

  // get the most recently set param
  float getCurrParam(int idx) { return _params.getInput(idx); }

protected: // internal methods
  void configParams1_0();
  void init();

  void copyInBuf(float** in_buf_, long buf_n);
  void paramsChkSync();
  void paramsChk();
  void findBlkInPos();
  void prepMixOut();
  void doFFT();
  void procFFT();
  template <class SRC> void mixToX3(SRC src, int ch);
  void ifftAndMixOut();
  void nextBlk();
  void zeroFillOutput();

  void _process(float** in_buf, long buf_n);

public: //
  // critical section is used to protect against gui & audio processing thread
  CriticalSectionWrapper _protect;

  // initial delay passed to setInitialDelay()
  long _initial_delay;

  // maximum number of samples delay
  long _max_delay_n;

  // current absolute sample position of input/output updated with every input buf
  // i.e. absolute position of _x0[_x0_i+x0_n] / _x3[_x3_o]
  long _curr_samp_abs;

  // number of samples to move forward in input buffer to process next blk
  long _next_blk_fwd_n;

  // absolute sample position of x0[x0_i] and also x1[0]/x2[0] when processing a fft-blk
  long _blk_samp_abs;

  long _x0_sz;           // wraping position of x0
  long _x0_force_out_sz; // force output if x0_n exceeds this

  long _x0_i;          // first sample of current blk in _x0
  long _x0_n;          // number of samples in the pre-fft buffer
  long _x0_n_past_end; // number of samples copied past _x0_sz

  long _x3_sz;       // total size of x3
  long _x3_o;        // output FIFO output index
  long _x3_end_abs;  // 1 + abs sample position of final sample in _x3
  bool _x3_is_clear; // true when x3 is initialized with 0's

  enum {
    PARAMS_CHK_SYNC,  // params need to be processed for the current blk
    PARAMS_NONINTERP, // params have been gathered but can't be interpolated
    PARAMS_INTERP_OK  // params can be properly interpolated for the current blk
  } _params_state;
  bool _params_need_processing;

  long _plan;    // actual plan (maybe different from desired plan if forced to output data early)
  long _delay_n; // output delay

  long _freq_fft_n; // actual fft blk sz processed (may not correspond to _desired_plan if output
                    // forced early)

  // block mix function generated from _blk_mix_param
  Array<float, 32> _blk_mix_fn;
  long _blk_mix_fn_n;

  // all state for DtBlkFx params are stored here
  Array<FxState1_0, BlkFxParam::NUM_FX_SETS> _fx1_0;

  // channel specific data
  struct Chan {
    ScopeFFTWfMalloc<float> x0; // pre FFT circular buffer, note: special alignment
    ScopeFFTWfMalloc<cplxf> x1; // FFT'd data (frequency-domain), note: special alignment
    ScopeFFTWfMalloc<float> x2; // IFFT'd data (time-domain) and may be used as a temporary buffer
                                // during effects, note: special alignment
    std::valarray<float> x3;    // output FIFO

    float total_in_pwr;  // x1 input power
    float total_out_pwr; // current x1 output power after effects

    // these 2 calculated after processing done
    float out_pwr_scale; // pwr scaling (total_in_pwr/total_out_pwr)
    float out_scale;     // sqrt(out_pwr_scale)
  } _chan[AUDIO_CHANNELS];

public: // temporary variables used during blk processing
  // sample position of next call to _process() (1+end of current buffer)
  long _buf_end_abs;

  // how much data is buffered in x0 after the current blk (if we were to process it)
  // may be negative meaning that we don't have all the data for the blk although after
  // preparing to process this is always >= 0 because the blk is shifted backwards
  long _extra_data;

  // destination absolute sample position for current fft blk (i.e. blk_samp_abs + delay)
  long _dst_fft_abs;

  // corresponds to the amount of old data that we've fft'd (data prior to _x0_i)
  long _data_pre_x0_n;

  // actual amount of data in the block that we are interested in (use in conjunction
  // with _time_offset)
  long _time_fft_n;

  // number of samples we overlap with prev blk
  long _fadein_n;

  // normally 0, number of samples we overlap with existing data
  long _fadeout_n;

  // mixback multiplier
  float _mixback;

public: // polled variables that are updated periodically
  // samples per beat
  float _samps_per_beat;

  // most recent start of beat sample position
  long _beat_start_abs;

  // last sample position bpm was polled
  long _prev_poll_abs;

  // number of samples before we do the poll
  long _samps_per_poll;

  // minimum number of samples that we move forward to get the next block
  long _min_blk_fwd_samps;

  // check whether enough samples have elapsed to update polled variables
  void pollUpdate(bool force);

public:
  // presets are stored here (and can be edited)
  typedef VstProgram<BlkFxParam::TOTAL_NUM> BlkFxProgram;
  std::vector<BlkFxProgram> _program;

  // params are delayed by the same amount of time as the audio
  ParamsDelay _params;

  // get value of vst param
  float /*0..1*/ getVstParamVal(ParamsDelayGetFn get_fn, VstParamIdx idx)
  {
    return idx < 0 ? 0.0f : (*get_fn)(_params)[idx];
  }

  // get morphed values using vst params (if need be)
  int /*elements copied*/ get(ParamsDelayGetFn get_fn, const MorphParam* morph, Rng<float> result,
                              int param_idx = 0)
  {
    return morph->get((*get_fn)(_params), result, param_idx);
  }

  // get single morphed param value using vst param (if need be)
  float get(ParamsDelayGetFn get_fn, const MorphParam* morph, int param_idx = 0)
  {
    Array<float, 1> result;
    get(get_fn, morph, result, param_idx);
    return result[0];
  }

public: // preview access
  // during preview all params attached to the vst param will be output immediately

  //
  bool isPreview(VstParamIdx idx) const { return idx < 0 ? false : _params.isOverridden(idx); }

  //
  void setPreview(VstParamIdx idx, float val)
  {
    if (idx >= 0)
      _params.setOverride(idx, val);
  }

  //
  void setPreviewState(VstParamIdx idx, bool state)
  {
    if (idx >= 0)
      _params.setOverride(idx, state);
  }

  //
  void setPreview(MorphParam* morph, int anchor_idx)
  {
    setPreview(*morph, morph->anchorIdxToFrac(anchor_idx));
  }

public: // for displaying vst param index focus in the gui
  bool isVstParamIdxFocus(VstParamIdx idx) const { return _vst_param_idx_focus == idx; }
  void setVstParamIdxFocus(VstParamIdx idx, void* cookie)
  {
    _vst_param_idx_focus = idx;
    _vst_param_idx_focus_cookie = cookie;
  }
  void clrVstParamIdxFocus() { setVstParamIdxFocus(-1, NULL); }
  void clrVstParamIdxFocus(void* cookie)
  {
    if (_vst_param_idx_focus_cookie == cookie)
      clrVstParamIdxFocus();
  }

public:
  MorphParam _mixback_param;
  MorphParam _delay_param;
  MorphParam _fft_len_param;
  MorphParam _overlap_param;

  MorphParam _pwr_match_param; // 0=filter mode, 1=pwr match
  MorphParam _beat_sync_param;
  MorphParam _param_sync_param;

  // blk fraction to be used as a shoulder - the shoulder is extra data on
  // the left and right of the blk that is transformed but not mixed back to output
  MorphParam _blk_shoulder_frac_param;

  // window applied to shoulder (only applicable when _blk_shoulder_frac_param is > 0)
  MorphParam _blk_shoulder_wdw_param;

  // interpolation envelope for blk mix
  MorphParam _blk_mix_param;

  // which vst param index is in focus in the GUI
  VstParamIdx _vst_param_idx_focus;
  void* _vst_param_idx_focus_cookie;

  // access program number
  int currProgramNum() { return limit_range(curProgram, 0, (int)_program.size() - 1); }

  // access current preset
  BlkFxProgram& currProgram() { return _program[currProgramNum()]; }

public: // gui state stuff
  //
  bool _param_morph_mode;
  bool isMorphMode() const { return _param_morph_mode; }

public:
  // contiguous space for chunk data & presets when saving
  std::valarray<unsigned char> _chunk_data;
};

//-------------------------------------------------------------------------------------------------
inline long DtBlkFx::getDelaySamps(const BlkFxParam::Delay& delay)
// internal method
// given the vst param, return the corresponding number of samples delay (including any initial
// delay)
{
  float samps = delay.getAmount();
  if (delay.i_part == delay.MSEC)
    samps *= sampleRate * 1e-3f;
  else
    samps *= _samps_per_beat;

  return limit_range((long)samps + _initial_delay,
                     100,         // arbitrary minimum size
                     _max_delay_n // max delay
  );
}
//-------------------------------------------------------------------------------------------------
inline /*static*/ long DtBlkFx::reducePlan(long plan /*start*/, long avail_samps)
// static method
// given "avail_samps", return plan that corresponds to data that is at most twice the length
//
{
  while (plan > 0 && g_fft_sz[plan] / 2 > avail_samps)
    plan--;
  return plan;
}

#endif
