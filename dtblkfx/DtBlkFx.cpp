/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Effect main class

General algorithm is:
1. Delay input into input buffer (_x0)
2. If we can interpolate the params or we're forced to output data FFT data from _x0 to _x1
3. run an effect from FFTFx()
4. IFFT data from _x1 to _x2
5. mix data from _x2 into _x3 (output buffer), doing a cross fade with any overlap of data
   already in there

History
  Date		Version Programmer		Comments
  16/02/03	-       Darrell Tam		Created buzz version
    01/10/05	1.0     Darrell Tam     Ported to VST
  14/08/06	1.1		Darrell Tam		Param morphing & flexible block mixing

This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

***************************************************************************************************/
#include <StdAfx.h>

#include <stdio.h>
#include <string.h>

#include "WrapProcessFloatVec.h"

#include "DtBlkFx.hpp"
#include "Gui.h"
#include "rfftw_float.h"

// filled by BlkFxMain.cpp
extern vector<VstProgram<BlkFxParam::TOTAL_NUM>> g_blk_fx_presets;

//#define WR_PARAM
#ifdef WR_PARAM
#  include <fstream>
std::ofstream f_param("/tmp/param.dat");
#endif

#define LOG(a, b)                                                                                  \
  {                                                                                                \
  }

#if 0
#  define LOG_FILE_NAME "c:\\blkfx.html"
#  include "Debug.h"

#  ifdef _DEBUG
  static const char* g_vars_hdr[] = {
    "curr_samp<br>_abs",
    "buf_end_abs",
    "x0_i",
    "x0_n",
    "x0_xform_i",
    "x3_o",
    "x3_end_abs",
    "dst_fft_abs",
    "blk_samp_abs",
    "time_fft_n",
    "freq_fft_n",
    "data_pre<br>_x0_n",
    "shoulder_n",
    "next_blk<br>_fwd_n",
    "extra_data",
    "fadein_n",
    "fadeout_n"
  };

  static HtmlTable g_vars(
    "c:\\blkfx_vars.html",	// output file
    "example.css",			// style sheet
    g_vars_hdr, NUM_ELEMENTS(g_vars_hdr) // heading data
  );
#  endif
#endif

using namespace std;

// constants
enum {
  AUDIO_CHANNELS = BlkFxParam::AUDIO_CHANNELS,

  // fftw needs data to be aligned to this (must be pwr of 2)
  FFTW_ALIGNMENT = 16,

  // alignment mask
  X0_INDEX_ROUNDING_MASK = ~(FFTW_ALIGNMENT - 1),

  // minimum number of samples that we move forward through blks
  MIN_BLK_FWD_N = FFTW_ALIGNMENT + /*arbitrary*/ 16,

  // value saved into chunks
  CHUNK_TAG = 0x99887766
};

// from BlkFxMain.cpp
extern bool GlobalInitOk();

//-------------------------------------------------------------------------------------------------
DtBlkFx::DtBlkFx(audioMasterCallback audioMaster)
    : AudioEffectX(audioMaster,
                   /*kNumPrograms*/ g_blk_fx_presets.size() + 1,
                   /*kNumParams*/ BlkFxParam::TOTAL_NUM)
// note, expect any allocation failures to throw an exception
{
  int i;

  LOG("", "DtBlkFx constructor");

  _vst_param_idx_focus = -1;

  _params.init(/*n params*/ BlkFxParam::TOTAL_NUM, /*delay length*/ 140);

  // set GUI if we've loaded images ok
  if (GlobalInitOk())
    setEditor(new Gui(this));

  //
  for (i = 0; i < _fx1_0.size(); i++)
    _fx1_0[i].init(this, /*fx set*/ i);

  //
  configParams1_0();

  // get buffer space
  _x0_sz = X0_INDEX_ROUNDING_MASK & (8 * 44100); // length of buffer is arbitrary
  _x0_force_out_sz = _x0_sz - MAX_FFT_SZ;        // force output when x0 contains this much data
  _x3_sz = 5 * 44100;
  for (i = 0; i < AUDIO_CHANNELS; i++) {
    _chan[i].x0.resize(
        _x0_sz + MAX_FFT_SZ); // input buffer with extra space at end to unwrap data for processing
    _chan[i].x1.resize(MAX_FFT_SZ / 2 +
                       32 * 2); // FFT'd complex data with space either side for shift overflow
    _chan[i].x2.resize(
        MAX_FFT_SZ +
        32 * 2); // inverse FFT & temporary buffer with space either side for shift overflow
    _chan[i].x3.resize(
        _x3_sz); // output buffer (length is arbitrary, as long as > MAX_FFT_SZ plus a few)
  }
  _max_delay_n = _x3_sz - MAX_FFT_SZ - 2048;

  // copy presets into the program
  _program.reserve(/*AudioEffect::*/ numPrograms);
  _program = g_blk_fx_presets;
  _program.push_back(BlkFxProgram("> reset current <:0.0 0.062745098 0.1098039 0.35"));

  //
  setNumInputs(AUDIO_CHANNELS);
  setNumOutputs(AUDIO_CHANNELS);

  // hasVu ();					// who knows what this is meant to do?
  // hasClip(false);			// we don't clip (is that what this is all about?)
  canProcessReplacing(true); // we implement process & processReplacing
  programsAreChunks(true);   // we handle all of our own param packing

  // arbitrary initial values
  _samps_per_beat = 4000.0f;
  _min_blk_fwd_samps = 64;

  // didn't work in fl studio or renoise for delay of 4096
  _initial_delay = 0;
  setInitialDelay(_initial_delay);

  // these were registered from steinberg
  if (AUDIO_CHANNELS == 2)
    setUniqueID('h526');
  else
    setUniqueID('g319');

  init();

  // put in arbitrary initial params - they should be overwritten by first param update
  _params.put(/*samp abs*/ 0, BlkFxParam::DELAY, 16.0f / 255.0f);
  _params.put(/*samp abs*/ 0, BlkFxParam::FFT_LEN, BlkFxParam::getFFTLenParam(16));
  _params.put(/*samp abs*/ 0, BlkFxParam::OVERLAP, 0.35f);

  resume(); // flush buffer
}

//-------------------------------------------------------------------------------------------------
DtBlkFx::~DtBlkFx()
{
  // make sure gui is closed, probably don't need to
  // TODO: check whether we need to do this
  if (gui())
    gui()->close();
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::configParams1_0()
// configure params for 1.0 compatible operation
//
{
  // set morphing to behave like blkfx 1.0
  _param_morph_mode = false;

  _mixback_param.setModeLin(BlkFxParam::MIX_BACK);
  float mixback_data[] = {0, 1, 0};
  _mixback_param.setAll(TO_RNG(mixback_data));

  _pwr_match_param.setModeStep(BlkFxParam::MIX_BACK);
  float pwr_match_data[] = {1, 0};
  _pwr_match_param.setAll(TO_RNG(pwr_match_data));

  _delay_param.setModeLin(BlkFxParam::DELAY);
  float delay_data[] = {0, 1};
  _delay_param.setAll(TO_RNG(delay_data));

  _fft_len_param.setModeLin(BlkFxParam::FFT_LEN);
  float fft_len_data[] = {0, 1};
  _fft_len_param.setAll(TO_RNG(fft_len_data));

  _overlap_param.setModeLin(BlkFxParam::OVERLAP);
  float overlap_data[] = {0, 1, 0};
  _overlap_param.setAll(TO_RNG(overlap_data));

  _beat_sync_param.setModeStep(BlkFxParam::OVERLAP);
  float beat_sync_data[] = {0, 1};
  _beat_sync_param.setAll(TO_RNG(beat_sync_data));

  _param_sync_param.setModeStep(BlkFxParam::OVERLAP);
  float param_sync_data[] = {0, 1};
  _beat_sync_param.setAll(TO_RNG(param_sync_data));

  // interpolation envelope for blk mix
  _blk_mix_param.setModeFixed();
  float blk_mix_data[] = {0, 1};
  _blk_mix_param.setAll(TO_RNG(blk_mix_data), /*param_n*/ 2);

  _blk_shoulder_frac_param.setModeFixed();
  _blk_shoulder_frac_param.set(/*value*/ 0, /*morph_idx*/ 0);

  // window applied to shoulder (only applicable when _blk_shoulder_frac_param is > 0)
  _blk_shoulder_wdw_param.setModeFixed();
  _blk_shoulder_wdw_param.setNumParams(20);
  _blk_shoulder_wdw_param.setNumAnchorPoints(4);

  // generate default raised cosine, 1/4 sin, all 1, all 0 windows
  unsigned wdw_n = _blk_shoulder_wdw_param.numParams();
  float fwdw_n = (float)wdw_n;
  ValStp<float> a[4];
  a[0].val = 0;
  a[0].stp = 3.1415926f / fwdw_n;
  a[1].val = 0;
  a[1].stp = 0.5f * 3.1415926f / fwdw_n;
  a[2].val = 1.0f;
  a[2].stp = 0;
  a[3].val = 0;
  a[3].stp = 0;
  for (unsigned i = 0; i < wdw_n; i++) {
    float r[4] = {0.5f - 0.5f * cosf(a[0]), sinf(a[1]), a[2], a[3]};
    for (int j = 0; j < 4; j++) {
      _blk_shoulder_wdw_param.set(r[j], /*anchor idx*/ j, /*param idx*/ i);
      a[j].next();
    }
  }
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::init()
// clear out all buffers
{
  unsigned i;

  // current absolute sample position of input/output updated with every input buf
  // i.e. absolute position of _x0[_x0_i+x0_n] / _x3[_x3_o]
  _curr_samp_abs = 0;
  _next_blk_fwd_n = 0;

  // absolute sample position of start of current fft blk at x0[x0_i]/x1[0]/x2[0]
  _blk_samp_abs = 0;
  _x0_n_past_end = 0; // number of samples copied past end of buffer
  _x0_i = 0;          // pre-fft buffer offset
  _x0_n = 0;          // number of samples in the pre-fft buffer

  _x3_o = 0;       // output FIFO output index
  _x3_end_abs = 0; // no data in _x3

  // clear the output buffers
  for (i = 0; i < AUDIO_CHANNELS; i++)
    Clear(_chan[i].x3);

  _x3_is_clear = true;

  // these will be updated on first poll
  _prev_poll_abs = 0;
  _samps_per_poll = 0;
  _beat_start_abs = 0;

  _params_state = PARAMS_CHK_SYNC;
  _params_need_processing = true;

  // these variables will be updated on first paramsChk()
  _dst_fft_abs = 0;
  _freq_fft_n = 4096;
}

//-------------------------------------------------------------------------------------------------
VstInt32 DtBlkFx::getChunk(void** vdata, bool is_preset)
// virtual, override AudioEffect
// only save using the latest chunk version
{
  LOG("", "DtBlkFx::getChunk" << VAR(isPreset));

  int i;

  // num programs to save excluding current params (don't save any when saving just for the preset)
  int num_programs = is_preset ? 0 : _program.size();

  // get enough space to copy current data and all presets
  int payload_bytes =
      PackedBytesPerVstProgram(BlkFxParam::TOTAL_NUM) * (num_programs + 1 /*curr params*/);
  _chunk_data.resize(/*tag field*/ 4 + /*vers field*/ 4 + /*n programs field*/ 4 +
                     /*curr program field*/ 4 + payload_bytes);

  // cast to what we need
  LittleEndianMemStr le_data(_chunk_data);

  // chunk tag
  le_data.put32((int)CHUNK_TAG);

  // version
  le_data.put32(101);

  // number of programs
  le_data.put32(num_programs);

  // current program
  le_data.put32(is_preset ? 0 : currProgramNum());

  // grab current settings & save
  VstProgram<BlkFxParam::TOTAL_NUM> curr_program;
  curr_program.name = currProgram().name;
  for (i = 0; i < BlkFxParam::TOTAL_NUM; i++)
    curr_program.params[i] = _params.getInputNoForced(i);
  curr_program.saveLittleEndian(&le_data);

  // save the programs if we're not a preset
  if (!is_preset) {
    // save all presets
    LOG("", "DtBlkFx::getChunk saving" << VAR(chunk_data->num_programs));
    for (i = 0; i < num_programs; i++)
      _program[i].saveLittleEndian(&le_data, 4 + 5 * 8);
  }

  *vdata = &_chunk_data[0];
  return _chunk_data.size();
}

//-------------------------------------------------------------------------------------------------
VstInt32 /*1=success*/ DtBlkFx::setChunk(void* vdata, VstInt32 n_bytes, bool isPreset)
// virtual, override AudioEffect
{
  LittleEndianMemStr le_data(vdata, n_bytes);

  unsigned long tag;
  if (!le_data.get32(&tag))
    return 0;
  if (tag != CHUNK_TAG)
    return 0;

  unsigned long vers;
  if (!le_data.get32(&vers))
    return 0;

  if (vers == 0) {
    // preset recall not supported with old version
    if (isPreset)
      return 0;

    // attempt to recall everything, write 0's if we run out of params in the data
    for (int i = 0; i < /*num params*/ 4 + 5 * 4; i++) {
      float param = 0.0f;
      le_data.get32(&param);
      setParameter(i, param);
    }
    return 1;
  }

  // these versions are nearly the same
  if (vers == 100 || vers == 101) {
    long num_programs;
    if (!le_data.get32(&num_programs))
      return 0;

    long curr_program;
    if (vers == 101)
      if (!le_data.get32(&curr_program))
        return 0;

    int num_params = 4 + 5 * 4; // vers 100 num params: 4 effects
    if (vers == 101)
      num_params = 4 + 5 * 8; // 8 effects

    //
    if (isPreset) {
      // recall a single program
      currProgram().loadLittleEndian(&le_data, num_params);

      // update everything
      setProgram(currProgramNum());
    }
    else {
      // recall everything: current params & all programs
      long i;

      // get current params
      BlkFxProgram curr;
      if (!curr.loadLittleEndian(&le_data, num_params))
        return 0;

      // max number of programs that we can load
      long max_num_programs = AudioEffect::numPrograms - 1;

      // calculate the number of programs based on the number of bytes
      num_programs = min((long)le_data.n / PackedBytesPerVstProgram(num_programs), num_programs);

      // don't load more than what we have space for
      num_programs = min(max_num_programs, num_programs);

      // load all the programs
      for (i = 0; i < num_programs; i++)
        _program[i].loadLittleEndian(&le_data, num_params);

      // check whether we need to rename the last param loaded
      if (i > 0 && i <= max_num_programs &&
          strcmp(_program[i - 1].getName(), "> reset current <") == 0)
        _program[i - 1].setName("unnamed");

      // determine current program
      if (vers == 101)
        AudioEffect::curProgram = curr_program;
      else
        // current program number is stored as the name of the "curr" params
        // I did it like this because I forgot to add a specific field for this in a prerelease
        AudioEffect::curProgram = strtol(curr.getName(), /*end ptr*/ NULL, /*base*/ 10);

      curProgram = currProgramNum(); // limit range
      LOG("", "DtBlkFx::setChunk v1.0" << VAR(curProgram));

      // set all of the current params
      for (i = 0; i < BlkFxParam::TOTAL_NUM; i++)
        setParameter(i, curr.params[i]);
    }
    return 1;
  }

  LOG("", "DtBlkFx::setChunk, unknown type" << VAR(byteSize) << VAR(isPreset));
  return 0; // can't recall
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::setProgram(VstInt32 num)
// virtual, override AudioEffect
// called by vst-host to set a program
{
  LOG("", "DtBlkFx::setProgram" << VAR(num));

  // normal case
  if (num < (int)_program.size() - 1)
    AudioEffect::setProgram(num);
  else {
    // check for the special "reset current program"
    // reset current program to defaults
    if (idx_within(currProgramNum(), g_blk_fx_presets))
      currProgram() = g_blk_fx_presets[currProgramNum()];
  }

  // update all params out of the program
  for (int i = 0; i < BlkFxParam::TOTAL_NUM; i++)
    setParameter(i, currProgram().params[i]);
}

//-------------------------------------------------------------------------------------------------
bool DtBlkFx::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text)
// virtual, override AudioEffect
// called by vst-host to get the name of a program by category
{
  LOG("", "DtBlkFx::getProgramNameIndexed" << VAR(category) << VAR(index) << VAR(_program.size()));
  if (index < 0 || index >= (int)_program.size())
    return false;
  strcpy(text, _program[index].name);
  return true;
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::resume()
// virtual, override AudioEffect
// called by vst-host to get ready to start processing
{
  LOG("", "DtBlkFx::resume");
  AudioEffectX::resume();

  ScopeCriticalSection scs(_protect);
  if (gui())
    gui()->resume();
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::suspend()
// virtual, override AudioEffect
// called by vst-host when sound is stopping
{
  LOG("", "DtBlkFx::suspend");
  AudioEffectX::suspend();

  ScopeCriticalSection scs(_protect);

  // roll back params to just contain most recent and reset sample position
  _params.resetAndCopyIn();

  // clear buffers
  init();

  if (gui())
    gui()->suspend();
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::setBlockSize(VstInt32 sz)
// virtual, override AudioEffect
// called by vst-host to indicate max number of samples that will be passed to process
{
  AudioEffectX::setBlockSize(sz);
  _max_delay_n = _x3_sz - MAX_FFT_SZ - sz;
}

struct MyInfo : public VstTimeInfo {
  void dbgprint()
  {
    LOG("",
        "VstTimeInfo" << VAR(samplePos) << VAR(sampleRate) << VAR(nanoSeconds) << VAR(ppqPos)
                      << VAR(tempo) << VAR(barStartPos) << VAR(cycleStartPos) << VAR(cycleEndPos)
                      << VAR(timeSigNumerator) << VAR(timeSigDenominator) << VAR(smpteOffset)
                      << VAR(smpteFrameRate) << VAR(samplesToNextClock) << VAR(flags));
  }
};
//-------------------------------------------------------------------------------------------------
void DtBlkFx::guessFFTLen(int& /*return*/ freq_fft_n, int& /*return*/ time_fft_n)
//
// guess the FFT length that will be used from the most recently buffered params for display
// params - this may not match the actual length that will be used: input buffer might be full
// and a block is forced to be output AND length of block passed to process() is not taken
// into account
//
{
  // get the delay in samples
  long delay_n = getDelaySamps(get(&GetInput, _delay_param));

  // get the fft length in samples
  int plan = BlkFxParam::getPlan(get(&GetInput, _fft_len_param));

  freq_fft_n = g_fft_sz[plan];

  // get time length based on shoulder fraction
  time_fft_n =
      (int)((float)freq_fft_n * lin_interp(get(&GetInput, _blk_shoulder_frac_param), 1.0f, .25f));

  // is the blk longer than the delay? if so reduce blk length
  if (time_fft_n > delay_n) {
    time_fft_n = freq_fft_n = g_fft_sz[reducePlan(plan, delay_n)];
    if (time_fft_n > delay_n)
      time_fft_n = delay_n;
  }
}

//-------------------------------------------------------------------------------------------------
float /*hz*/ DtBlkFx::guessRoundHz(float freq_hz, float bin_adjust_frac)
// given a freq, round using the guessed FFT len
{
  bool neg = false;
  if (freq_hz < 0.0f) {
    freq_hz = -freq_hz;
    neg = true;
  }
  int fft_n, time_n;
  guessFFTLen(/*return*/ fft_n, /*return*/ time_n);

  float hz_per_bin = sampleRate / fft_n;
  float bin = limit_range(freq_hz / hz_per_bin, 0.0f, fft_n / 2.0f);
  float r = (floorf(bin + 0.5f) + bin_adjust_frac) * hz_per_bin;
  return neg ? -r : r;
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::pollUpdate(bool force)
// internal method, called regularly from process methods
//
// update polled variables from vst host (to do with tempo)
//
{
  if (!force && _curr_samp_abs - _prev_poll_abs < _samps_per_poll)
    return;

  // ask host for tempo
  VstTimeInfo* ti = getTimeInfo(kVstTempoValid | kVstPpqPosValid);
  if (ti) {
    //((MyInfo*)ti)->dbgprint();
    if (ti->flags & kVstTempoValid)
      _samps_per_beat = (float)(60.0 * sampleRate / ti->tempo);

    float ppq_pos = (float)ti->ppqPos;
    float ppq_frac = ppq_pos - floorf(ppq_pos);

    // work out sample position of the start of the current beat
    if (ti->flags & kVstPpqPosValid)
      _beat_start_abs = _curr_samp_abs - (long)(ppq_frac * _samps_per_beat);
  }

  // calculate samples per tick (based on tempo)
  float samps_per_tick = (1.0f / (float)BlkFxParam::TICKS_PER_BEAT) * _samps_per_beat;

  // interpolate params 1/4 beat before the value changes
  _params.setExpectedDist((long)(_samps_per_beat * .25f));

  // LOG("", "pollUpdate"
  // <<VAR(_samps_per_beat)<<VAR(ppq_pos)<<VAR(ppq_frac)<<VAR(_beat_start_abs));

  // poll every beat
  _samps_per_poll = (long)(_samps_per_beat);
  _prev_poll_abs = _curr_samp_abs;

  // min number of samps we can mv fwd to get the next blk
  _min_blk_fwd_samps = max(64L, (long)samps_per_tick);
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::setParameter(VstInt32 index, float value)
// virtual, override AudioEffect
// called by vst-host to set param
{
  // safety
  if (index < 0 || index >= BlkFxParam::TOTAL_NUM)
    return;

  // safety
  value = limit_range(value, 0.0f, 1.0f);

  //
  ScopeCriticalSection scs(_protect);

  // copy change into the current program
  currProgram().params[index] = value;

// write param to the file
#ifdef WR_PARAM
  f_param.seekp(index * 4);
  f_param.write((char*)&value, 4);
  f_param.flush();
#endif

  if (_params.put(_curr_samp_abs, index, value) /*already exists*/ == false) {
    // this is a new time, update params
    pollUpdate(/*force*/ true);

    // trigger sample processing loop to check params again
    if (_params_state != PARAMS_INTERP_OK)
      _params_need_processing = true;
  }

  if (gui())
    gui()->setParameter(index, value);
}

//-------------------------------------------------------------------------------------------------
float DtBlkFx::getParameter(VstInt32 index)
// virtual, override AudioEffect
// called by vst-host to get param value
{
  using namespace BlkFxParam;
  if (index < 0 || index >= TOTAL_NUM)
    return 0.0f;
  // use most recently input value
  return _params.getInputNoForced(index);
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::getParameterName(VstInt32 index, char* label)
// virtual, override AudioEffect
// called by vst-host to get name of param
{
  CharRng str(label, kVstMaxParamStrLen);
  using namespace BlkFxParam;

  // first 10 params params can be used as morph
  // if(index < 10) label += sprintf(label, "%d/", index);

  SplitParamNum p(index);
  // try global params
  switch (p.glob_param) {
    case MIX_BACK:
      str << "Mixbk";
      return;
    case DELAY:
      str << "Delay";
      return;
    case FFT_LEN:
      str << "BlkSz";
      return;
    case OVERLAP:
      str << "Ovrlp";
      return;
  }
  // try fx set
  switch (p.fx_param) {
    case FX_FREQ_A:
      str << p.fx_set << ".FrqA";
      return;
    case FX_FREQ_B:
      str << p.fx_set << ".FrqB";
      return;
    case FX_AMP:
      str << p.fx_set << ".Amp";
      return;
    case FX_TYPE:
      str << p.fx_set << ".Fx";
      return;
    case FX_VAL:
      str << p.fx_set << ".Val";
      return;
  }
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::getParameterLabel(VstInt32 index, char* label)
// virtual, override AudioEffect
// called by vst-host to get param label
{
  CharRng str(label, kVstMaxParamStrLen);
  str << "";
}

//-------------------------------------------------------------------------------------------------
bool /*true=printed*/ DtBlkFx::getParamDisplayGlobal(BlkFxParam::SplitParamNum& p, float v,
                                                     CharRng text)
{
  using namespace BlkFxParam;

  // see if index belongs to the global params
  switch (p.glob_param) {
    case MIX_BACK: {
      bool printed = false;
      if (_pwr_match_param.vstParamIdx() == MIX_BACK) {
        text = text << (_pwr_match_param(v) > 0.5f ? "match" : "filter");
        printed = true;
      }
      if (_mixback_param.vstParamIdx() == MIX_BACK) {
        text << (printed ? " " : "") << spr_percent(_mixback_param(v));
        printed = true;
      }
      return printed;
    }

    case DELAY: {
      if (_delay_param.vstParamIdx() != DELAY)
        return false;

      BlkFxParam::Delay delay(_delay_param(v));
      float delay_samps = (float)getDelaySamps(delay) - _initial_delay;
      if (delay.getUnits() == delay.BEATS)
        text << sprf("%.2f beats", delay_samps / getSampsPerBeat());
      else
        text << sprnum(delay_samps / getSampleRate()) << "sec";
      return true;
    }
    case FFT_LEN: {
      if (_fft_len_param.vstParamIdx() != FFT_LEN)
        return false;

      int fft_len, time_len;
      guessFFTLen(/*return*/ fft_len, /*return*/ time_len);

      // determine blk size in seconds
      float sec = fft_len / sampleRate;

      // print blk size in seconds or msec
      text << sprnum(sec) << "sec";
      return true;
    }

    case OVERLAP: {
      bool printed = false;
      if (_beat_sync_param.vstParamIdx() == OVERLAP && _beat_sync_param(v) > 0.5f ||
          _param_sync_param.vstParamIdx() == OVERLAP && _param_sync_param(v) > 0.5f) {
        text = text << "sync";
        printed = true;
      }
      if (_overlap_param.vstParamIdx() == OVERLAP) {
        int fft_len, time_len;
        guessFFTLen(/*return*/ fft_len, /*return*/ time_len);
        long fwd_n = getBlkShiftFwd(_overlap_param(v), time_len);
        float overlap = 1.0f - ((float)fwd_n / (float)time_len);

        text << (printed ? " " : "") << spr_percent(overlap);
        printed = true;
      }
      return printed;
    }
  }
  return false; // param not printed
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::getParameterDisplay(VstInt32 index, char* text)
// virtual, override AudioEffect
// called by vst-host to convert a param to text
{
  // CharRng str(text, kVstMaxParamStrLen); // surely no one only gives us 8 chars??
  CharRng str(text, /*max len inc. zero*/ 12);

  float v = _params.getInput(index);
  BlkFxParam::SplitParamNum p(index);

  if (p.ok) {
    if (getParamDisplayGlobal(p, v, str))
      return;
    if (p.fx_set >= 0 && _fx1_0[p.fx_set].getParamDisplay(p, v, str))
      return;
  }
  // otherwise morph mode, so print param as a percentage
  str << spr_percent(v);
}

//-------------------------------------------------------------------------------------------------
bool DtBlkFx::getEffectName(char* name)
// virtual, override AudioEffect
// called by vst-host to return name of effect
{
  LOG("", "DtBlkFx::getEffectName");

  CharRng(name, kVstMaxEffectNameLen) << "DTBlkFx"
#ifdef STEREO
                                         "S"
#endif
#ifdef _DEBUG
                                         "Dbg"
#endif
      ;
  return true;
}

//-------------------------------------------------------------------------------------------------
bool DtBlkFx::getProductString(char* text)
// virtual, override AudioEffect
// called by vst-host to get product string
{
  LOG("", "DtBlkFx::getProductString");
  CharRng(text, kVstMaxEffectNameLen) << "DTBlkFx"
#ifdef STEREO
                                         "S"
#endif
#ifdef _DEBUG
                                         "Dbg"
#endif
      ;
  return true;
}

//-------------------------------------------------------------------------------------------------
bool DtBlkFx::getVendorString(char* text)
// virtual, override AudioEffect
// called by vst-host to get vendor string
{
  LOG("", "DtBlkFx::getVendorString");
  CharRng(text, kVstMaxVendorStrLen) << "darrell";
  return true;
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::copyInBuf(float** in_buf_, long buf_n)
// internal method
// copy input data into the pre-fft buffer
// note that we are allowed to copy data beyond the end of our normal wrap position. The x0 buffer
// contains extra memory corresponding to the length of largest fft blk.
//
// assume that we don't get blocks bigger than our buffer size (several seconds worth of data)
{
  long in_buf_offs = 0;
  while (buf_n) {
    // how many samples to copy
    long n = buf_n;

    // offset into buffer at which to write data
    long t = _x0_i + _x0_n;

    // wrap to the start of the buffer if need be
    if (t >= _x0_sz)
      t -= _x0_sz;

    // find how much the copy will overflow past _x0_sz
    long overflow = t + buf_n - _x0_sz;

    // don't overflow at all if there's too much data - I don't expect this to ever happen
    // (this is the only way that the loop could run more than once)
    if (overflow > MAX_FFT_SZ) {
      n -= overflow;
      overflow = 0;
    }
    if (overflow >= 0)
      _x0_n_past_end = overflow;

    // copy all data into buffer (note that this may be past _x0_sz, which is fine)
    for (int i = 0; i < AUDIO_CHANNELS; i++) {
      float* x0_dat = _chan[i].x0;
      Copy(x0_dat + t, in_buf_[i] + in_buf_offs, n);

      // copy any overflowed data to start of buffer
      if (overflow > 0)
        Copy(x0_dat, x0_dat + _x0_sz, overflow);
    }

    buf_n -= n;
    in_buf_offs += n;
    _x0_n += n;
  }
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::paramsChkSync()
// internal method
//
// check params for "next_blk_fwd_n" samples from _blk_samp_abs to see whether we need to sync to
// any of them
{
  // see whether we can skip the param sync stage because param syncing is off and not
  // automated (so that it can't be turned on)
  if (_param_sync_param == MorphParam::MODE_FIXED && _param_sync_param(/*ignored*/ 0.0f) < 0.5f) {
    _params_state = PARAMS_NONINTERP;
    return;
  }

  // chk each param set in the fifo to see whether we need to sync to it until we reach the
  // next blk position
  while (1) {
    // how far forward is the next param from the previous output blk
    long param_fwd = _params.getOutSampAbs(/*next*/ true) - _blk_samp_abs;

    // done for this state if the next param exceeds next blk position
    if (param_fwd >= _next_blk_fwd_n) {
      _params_state = PARAMS_NONINTERP;
      return;
    }

    // see if this param is a candidate for syncing to
    if (param_fwd > _min_blk_fwd_samps && _params.isAnyExplicitSet(/*next*/ true)) {

      // should we sync the fft blk with param?
      if (get(&GetInterp, _param_sync_param) > 0.5f) {
        _next_blk_fwd_n = param_fwd;
        _params_state = PARAMS_NONINTERP;
        LOG(0xfff800, "Param sync, abs_pos=" << _blk_samp_abs + param_fwd);
        return;
      }
    }

    // if we can, go to the next param
    if (!_params.nextOutParam())
      return;
  }
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::paramsChk()
// internal method
//
// check whether params need processing & update variables used in processing loop
//
{
  if (!_params_need_processing)
    return;
  _params_need_processing = false;

  // check whether we should sync with a param
  if (_params_state == PARAMS_CHK_SYNC)
    paramsChkSync();

  // find src position of fft blk
  long src_fft_abs = _blk_samp_abs + _next_blk_fwd_n;

  // see if we can interpolate
  if (_params_state == PARAMS_NONINTERP && _params.setOutPos(src_fft_abs))
    _params_state = PARAMS_INTERP_OK;

  // number of samples to do fft blk
  _plan = BlkFxParam::getPlan(get(&GetInterp, _fft_len_param));
  _freq_fft_n = g_fft_sz[_plan];

  // this is how much of the blk we want to process
  _time_fft_n =
      (int)((float)_freq_fft_n * lin_interp(get(&GetInterp, _blk_shoulder_frac_param), 1.0f, .25f));

  // center the data to be processed (this will be adjusted if there isn't enough data to fill
  // the blk)
  _data_pre_x0_n = (_freq_fft_n - _time_fft_n) / 2;

  // find (possible) output position of the current blk using current delay
  _delay_n = getDelaySamps(get(&GetInterp, _delay_param));
  _dst_fft_abs = src_fft_abs + _delay_n;

  // make sure dest position isn't in data that we've already output (i.e. behind current sample
  // pos) by increasing the delay
  long behind_n = _curr_samp_abs - _dst_fft_abs;
  if (behind_n > 0) {
    _delay_n += behind_n;
    _dst_fft_abs = _curr_samp_abs;
  }
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::findBlkInPos()
// internal method
// prepare input blk
// variables update: _x0_i, _x0_n, _blk_samp_abs, _data_pre_x0_n, _time_fft_n, _freq_fft_n,
// _extra_data
{
  // this should never happen: not enough data to go forward
  ASSERTX(_next_blk_fwd_n <= _x0_n, VAR(_next_blk_fwd_n) << VAR(_x0_n));

  // move forward in the input buffer
  _x0_i += _next_blk_fwd_n;
  _x0_n -= _next_blk_fwd_n;
  _blk_samp_abs += _next_blk_fwd_n;

  // wrap to the start of the buffer (assume fwd_n is < _x0_sz)
  if (_x0_i >= _x0_sz)
    _x0_i -= _x0_sz;

  // chk whether we have enough data to do blk size & windowing requested
  if (_extra_data >= 0) {
    // all good, enough data to process
  }
  else if (_time_fft_n <= _x0_n) {
    // there's enough data if we don't honour exact spectrum windowing requested
    _data_pre_x0_n = _freq_fft_n - _x0_n;
    _extra_data = 0;

    // shouldn't happen, x0 pre-data becomes negative
    ASSERTX(_data_pre_x0_n < 0, VAR(_data_pre_x0_n) << VAR(_x0_n));
    if (_data_pre_x0_n < 0)
      _data_pre_x0_n = 0;
  }
  else {
    // not enough data, reduce fft length to what data we have
    _plan = reducePlan(_plan, _x0_n);
    _time_fft_n = _freq_fft_n = g_fft_sz[_plan];

    // now do we have enough data?
    _extra_data = _x0_n - _freq_fft_n;
    _data_pre_x0_n = 0;

    // if there's no extra data then pad with old data
    if (_extra_data < 0) {
      _data_pre_x0_n = -_extra_data;
      _time_fft_n = _x0_n;
      _extra_data = 0;
    }
  }
}
//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::doFFT()
// internal method
//
// perform windowing on shoulder (if need be) and FFT on all channels
//
{
  int i;

  // work out where we'll transform from
  long x0_xform_i = _x0_i - _data_pre_x0_n;
  if (x0_xform_i < 0)
    x0_xform_i += _x0_sz;

  // find the amount of shoulder data that we can apply a window function to (window is symmetrical)
  int shoulder_n =
      min(/*right*/ _data_pre_x0_n, /*left*/ _freq_fft_n - _time_fft_n - _data_pre_x0_n);

  // if the shoulder windowing needs to be applied then we'll copy input data to "x2", window and
  // then transform
  if (shoulder_n > /*arbirary*/ 12) {

    // get the shoulder function to apply
    Array<float, 48> shoulder_fn;
    int shoulder_fn_n = get(&GetInterp, _blk_shoulder_wdw_param, shoulder_fn);

    for (i = 0; i < AUDIO_CHANNELS; i++) {
      // position within x0
      long x0_x = x0_xform_i;

      // get x0 data range excluding overflow region
      Rng<float> x0(_chan[i].x0, _x0_sz);

      // apply window to left shoulder
      PLinInterp<PScaleCopyOut> p0(shoulder_fn, shoulder_fn_n);
      p0.proc.dst = _chan[0].x2; // always channel 0 to improve caching performance
      x0_x = wrapProcess(p0, x0, x0_x, shoulder_n);

      // copy mid section directly
      PCopyOut p1;
      p1.dst = p0.proc.dst;
      x0_x = wrapProcess(p1, x0, x0_x, _freq_fft_n - shoulder_n * 2);

      // apply window to right shoulder
      PLinInterp<PScaleCopyOut, /*reverse*/ 1> p2(shoulder_fn, shoulder_fn_n);
      p2.proc.dst = p1.dst;
      x0_x = wrapProcess(p2, x0, x0_x, shoulder_n);

      // and do the fft
      FFTWf::execute_dft_r2c(g_fft_plan[_plan], _chan[0].x2, to_fftwf_complex(FFTdata(i)));
    }
  }
  else {
    // do some data alignment to keep fftw happy

    // adjust pre-data to do the rounding
    int round_down = x0_xform_i & ~X0_INDEX_ROUNDING_MASK;
    x0_xform_i -= round_down;
    _extra_data += round_down;
    _data_pre_x0_n += round_down;
    _time_fft_n -= round_down;

    // shouldn't happen
    ASSERTX(_time_fft_n >= 0, VAR(_time_fft_n));
    if (_time_fft_n < 0)
      _time_fft_n = 0;

    // no windowing of data, make sure it is contiguous in x0 if it has wrapped past the end
    long x0_sz_ext = _x0_sz + _x0_n_past_end;
    long split_n = (x0_xform_i + _freq_fft_n) - x0_sz_ext;
    if (split_n > 0) {
      for (i = 0; i < AUDIO_CHANNELS; i++) {
        float* x0_dat = _chan[i].x0;
        Copy(x0_dat + x0_sz_ext, x0_dat + _x0_n_past_end, split_n);
      }
      _x0_n_past_end += split_n;
    }

    // do the fft
    for (i = 0; i < AUDIO_CHANNELS; i++) {
      float* x0_dat = _chan[i].x0;
      FFTWf::execute_dft_r2c(g_fft_plan[_plan], x0_dat + x0_xform_i, to_fftwf_complex(FFTdata(i)));
    }
  }

  // scale spectrum and find power for power matching
  float scale = 1.0f / (float)_freq_fft_n;
  for (i = 0; i < AUDIO_CHANNELS; i++) {
    // find power of spectrum (so that we can match to this afterwards)
    float acc = 0.0f;

    CplxfPtrPair in(FFTdata(i), _freq_fft_n / 2 + 1);
    for (; !in.equal(); in.a++) {
      in() = in() * scale;
      acc += norm(*in.a);
    }

    _chan[i].total_out_pwr = acc;
    _chan[i].total_in_pwr = acc;
  }
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::prepMixOut()
// internal method
//
// Prepare to mix blk to output by finding the number of samples that the current blk overlaps
// with the previous blk (_fadein_n & _fadeout_n)
//
{
  // the amount of xfade-in corresponds to the overlap of this fft blk with existing data
  _fadein_n = _x3_end_abs - _dst_fft_abs;

  // normal case is not to xfade-out current fft-blk because next blk will do the xfade
  _fadeout_n = 0;

  // get the absolute ending position of blk
  long dst_fft_end_abs = _dst_fft_abs + _time_fft_n;

  // how much the new fft blk will increase the amount of data in x3
  long extend_x3 = dst_fft_end_abs - _x3_end_abs;

  if (_fadein_n >= 0) {
    // normal case: previous data and the current fft blk overlap, cross-fade

    if (extend_x3 < 0) {
      // special case: current fft blk ends up entirely within existing x3 data,

      // do nothing if the blk ends prior to the current output position (should not happen)
      if (_curr_samp_abs - dst_fft_end_abs >= 0) {
        LOG("", "mixToOutputBuffer possible ERROR" << VAR(_curr_samp_abs - dst_fft_end_abs));
        return;
      }

      // cross fade all of fft blk in & out
      _fadein_n = _time_fft_n / 2;
      _fadeout_n = _time_fft_n - _fadein_n;

      // one case is not handled, part of current blk is before current sample position
      // (should never happen)
    }
  }
  else {
    // special case: there is a gap between the end of prev data and the current fft blk

    // zero-fill gap (and a bit extra because we will still cross-fade to the current blk)
    long zero_o = _x3_end_abs - _curr_samp_abs;
    long zero_n = -_fadein_n;

    // arbitrary fade in of current fft blk
    _fadein_n = 16;

    // zero data in the gap (unless x3 is already cleared)
    if (!_x3_is_clear) {
      PZero pzero;
      for (int i = 0; i < AUDIO_CHANNELS; i++)
        wrapProcess(pzero, _chan[i].x3, _x3_o + zero_o, zero_n + _fadein_n);
    }
    _x3_is_clear = false;

    // arbitrary fade out of previous data
    long prev_fade_n = 16;
    long prev_fade_o = zero_o - prev_fade_n;

    // don't fade data that has already been output
    if (prev_fade_o < 0) {
      prev_fade_n += prev_fade_o;
      prev_fade_o = 0;
    }
    if (prev_fade_n > 0) {
      for (int i = 0; i < AUDIO_CHANNELS; i++) {
        PSrcDstWrap<PNoSrc, PRampDst> p;
        p.dst.setMix(/*start*/ 1, /*end*/ 0);
        wrapProcess(p, _chan[i].x3, _x3_o + prev_fade_o, prev_fade_n);
      }
    }
    // LOG("", "mixToOutputBuffer gap" << VAR(zero_o) << VAR(zero_n) << VAR(prev_fade_n) <<
    // VAR(prev_fade_o));
  }

  // update the end position of data in x3 if it is extended (this is slightly early but that
  // doesn't matter)
  if (extend_x3 > 0)
    _x3_end_abs = dst_fft_end_abs;

  // ensure fade in is not too big
  if (_fadein_n > _time_fft_n)
    _fadein_n = _time_fft_n;
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::procFFT()
// internal method
// process the FFT'd data (x1) using the fx_params
{
  int i;

  // run all of the 1.0 effects (collect params first & then process)
  for (i = 0; i < BlkFxParam::NUM_FX_SETS; i++)
    _fx1_0[i].prepare();
  for (i = 0; i < BlkFxParam::NUM_FX_SETS; i++)
    _fx1_0[i].process();

  // power match amount
  float pwr_match = get(&GetInterp, _pwr_match_param);

  // post process, work pwr out scaling
  for (i = 0; i < AUDIO_CHANNELS; i++) {

    float out_pwr = GetPwr(FFTdata(i), _freq_fft_n / 2 + 1);

    // match the output to the input power
    // power match mode, scale output to match input power
    double pwr_scale = 1.0f;
    if (out_pwr > 1e-30)
      pwr_scale *= _chan[i].total_in_pwr / out_pwr;

    if (pwr_scale > 1e30)
      pwr_scale = 1.0f; // too big
    if (pwr_scale < 1e-30)
      pwr_scale = 0.0f; // too small (or worse, negative)

    _chan[i].out_pwr_scale = lin_interp(pwr_match, 1.0f, (float)pwr_scale);
    _chan[i].out_scale = sqrtf(_chan[i].out_pwr_scale);
  }
}

//-------------------------------------------------------------------------------------------------
template <class SRC> inline void DtBlkFx::mixToX3(SRC src, int ch)
// internal method
//
{
  // position to start fade-in
  long x3_o = _dst_fft_abs - _curr_samp_abs + _x3_o;

  // use lerp'd mix function for fade in
  if (_fadein_n > 0) {
    PLinInterp<PMix<SRC>> p(_blk_mix_fn, _blk_mix_fn_n);
    p.proc.src = src;
    x3_o = wrapProcess(p, _chan[ch].x3, x3_o, _fadein_n);
    src = p.proc.src;
  }

  // number of samples in mid section
  long cp_n = _time_fft_n - _fadein_n - _fadeout_n;

  // copy mid section
  if (cp_n > 0) {
    PSrcDstWrap<SRC, PBlatDst> p;
    p.src = src;
    x3_o = wrapProcess(p, _chan[ch].x3, x3_o, cp_n);
    src = p.src;
  }

  // linear cross-mix
  if (_fadeout_n > 0) {
    PSrcDstWrap<SRC, PMixDst> p;
    p.src = src;
    p.dst.setMix(/*start*/ 0, /*end*/ 1);
    wrapProcess(p, _chan[ch].x3, x3_o, _fadeout_n);
  }
}

//-------------------------------------------------------------------------------------------------
void LineMix(float* data_0,   // in data array
             float* data_1,   // in data array
             float* data_out, // out data array
             int data_n,      // number of data points
             float mix_ratio, // 0=data_0, 1=data_1
             float y_scale    // y scaling: 0=linear sum >1 (e.g. 100) = rubber mix
)
{
  float
      //
      *data_in[2] = {data_0, data_1},

      // normalizing length multipliers
      norm_len_mul[2];

  // work out normalizing factors based on curve lengths
  for (int i = 0; i < 2; i++) {
    // find total integral length of curve
    float total_len = 0;
    float* d = data_in[i];
    float* d_end = d + data_n;

    float prv = *d;
    while (++d < d_end) {
      float dy_scaled = y_scale * (*d - prv);
      total_len += sqrtf(dy_scaled * dy_scaled + 1);
      prv = *d;
    }

    // turn length into a multiplier
    norm_len_mul[i] = 1 / total_len;
  }

  float
      // current index into each input array
      idx[2] = {0, 0},

      // current input array value
      val[2] = {*data_0, *data_1},

      // cumulative intput length
      len[2] = {0, 0},

      // line seg multiplier for lerping onto curve "b"
      line_seg_mul[2] = {0, 0},

      // current delta-y for each input curve
      y_delta[2] = {0, 0},

      //
      *data_in_last[2] = {data_0 + data_n - 1, data_1 + data_n - 1},

      // current & prv lerp'd mixed values
      lerp_x = -1, lerp_y = 0, prv_lerp_x = -1, prv_lerp_y = 0,

      // end of output data
                                                    *data_out_end = data_out + data_n,

      // current output index
      out_idx = 0;

  // do all the lerping
  while (data_out < data_out_end) {
    while (lerp_x <= out_idx) {
      int a;
      float ratio;

      // which curve should we use as curve "a"?
      if (len[0] < len[1]) {
        a = 0;
        ratio = mix_ratio;
      }
      else {
        a = 1;
        ratio = 1 - mix_ratio;
      }
      int b = 1 - a;

      // lerp corresponding point on curve "b"
      float frac = (len[b] - len[a]) * line_seg_mul[b];
      float idx_b = idx[b] - frac;
      float val_b = val[b] - frac * y_delta[b];

      // lerp between curves a & b to get the mixed value
      prv_lerp_x = lerp_x;
      prv_lerp_y = lerp_y;
      lerp_x = lin_interp(ratio, idx[a], idx_b);
      lerp_y = lin_interp(ratio, val[a], val_b);

      // stop if we've reached the zend of curve "a"
      if (data_in[a] >= data_in_last[a])
        break;

      // move to the next point on curve "a"
      idx[a]++;
      data_in[a]++;

      // get current data point from curve "a"
      float cur = *data_in[a];

      // y-axis difference between new point & previous
      y_delta[a] = cur - val[a];

      // 2-d distance between this point & previous
      float dy_scaled = y_delta[a] * y_scale;
      float dl = sqrtf(dy_scaled * dy_scaled + 1) * norm_len_mul[a];
      len[a] += dl;

      // update value with current value
      val[a] = cur;

      // determine lerping multiplier when mapping a point onto this curve
      line_seg_mul[a] = 1 / dl;
    }

    // lerp between prv & curr points to get final output
    if (lerp_x != prv_lerp_x) {
      float frac = (lerp_x - out_idx) / (lerp_x - prv_lerp_x);
      *data_out++ = lin_interp(frac, lerp_y, prv_lerp_y);
    }
    else
      *data_out++ = lerp_y;

    out_idx++;
  }
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::ifftAndMixOut()
// internal method
// perform ifft & mix to output
{
  // do iFFT, then fade-in, direct copy and fade-out to output buffer
  for (int i = 0; i < AUDIO_CHANNELS; i++) {
    Chan& chan = _chan[i];
    float* x2 = _chan[0].x2; // always use x2 from ch0 to help cache performance

    // inverse fft, always ifft into channel-0 x2 to improve cache hits
    FFTWf::execute_dft_c2r(g_ifft_plan[_plan], /*in*/ to_fftwf_complex(FFTdata(i)), /*out*/ x2);

    // skip pre data
    x2 += _data_pre_x0_n;

#if 0
    // scale data
    for(int j = 0; j < _time_fft_n; j++) x2[j] *= chan.out_scale;

    // line mixer
    float* x1 = (float*)chan.x1.ptr;
    LineMix(/*in*/x2, /*in*/chan.x0+_x0_i, /*out*/x1, _time_fft_n, _mixback, /*yscale*/100);
    mixToX3(P1Src(x1, /*scale*/1), i);
#endif

    // output data is completely fft blk
    float x2_scale = (1.0f - _mixback) * chan.out_scale;
    if (_mixback <= 0.0f)
      mixToX3(P1Src(x2, x2_scale), i);

    else {
      // output data is a mix of original and processed
      P2Src src;
      float* x0_dat = chan.x0;
      src.a = P1Src(x0_dat + _x0_i, _mixback);
      src.b = P1Src(x2, x2_scale);
      mixToX3(src, i);
    }
  }
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::nextBlk()
// internal method
// update everything ready for the next block
//
{
  // get next blk forward
  _next_blk_fwd_n = BlkFxParam::getBlkShiftFwd(get(&GetInterp, _overlap_param), _time_fft_n);

  // get the next overlap param to see if blksync is on

  // synchronize next blk to start-of-beat position if beat_sync
  float beat_sync = get(&GetInterp, _beat_sync_param) * _samps_per_beat;
  if (beat_sync > /*arbitrary*/ 16) {
    // next blk sample position as determined from current overlap param
    long next_blk_samp_abs = _blk_samp_abs + _next_blk_fwd_n;

    // find the sample position of the closest sync position before current next blk
    float units = ceilf((_beat_start_abs - next_blk_samp_abs) / beat_sync);
    long sync_abs = (long)(_beat_start_abs - units * beat_sync);
    long sync_fwd = sync_abs - _blk_samp_abs;

    // LOG("", "nextBlk, blksync" <<VAR(_next_blk_fwd_n) <<VAR(sync_fwd) <<VAR(_blk_samp_abs)
    // <<VAR(next_blk_samp_abs)<<VAR(beats)<<VAR(sync_abs)<<VAR(overlap_param));
    if (sync_fwd > _min_blk_fwd_samps && sync_fwd <= _next_blk_fwd_n)
      _next_blk_fwd_n = sync_fwd;
  }

  _params_state = PARAMS_CHK_SYNC;
  _params_need_processing = true;
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::zeroFillOutput()
// zero fill x3 if it has no data to output for this blk - this will happen before the first
// fft blk has been processed (although x3 will already be cleared) or if the delay is
// increased in some cases
//
{
  long zero_n = _buf_end_abs - _x3_end_abs;
  if (zero_n <= 0 || _x3_is_clear)
    return;

  // sanity check that we aren't
  ASSERTX(zero_n < _x3_sz, VAR(_x3_sz) << VAR(_buf_end_abs) << VAR(_x3_end_abs));

  int i;
  long zero_o = _x3_end_abs - _curr_samp_abs;
  PZero pzero;
  for (i = 0; i < AUDIO_CHANNELS; i++)
    wrapProcess(pzero, _chan[i].x3, _x3_o + zero_o, zero_n);

  // fade out previous data (if there's any in this blk)
  long fade_n = 16; // arbitrary
  long fade_o = zero_o - fade_n;
  if (fade_o < 0) {
    fade_n = fade_o;
    fade_o = 0;
  }
  if (fade_n > 0) {
    for (i = 0; i < AUDIO_CHANNELS; i++) {
      PSrcDstWrap<PNoSrc, PRampDst> p;
      p.dst.setMix(/*start*/ 1.0f, /*end*/ 0.0f);
      wrapProcess(p, _chan[i].x3, _x3_o + fade_o, fade_n);
    }
  }

  _x3_end_abs = _buf_end_abs;
}

//-------------------------------------------------------------------------------------------------
inline void DtBlkFx::_process(float** in_buf, long buf_n)
// internal method
{
  ScopeCriticalSection scs(_protect);

  pollUpdate(/*force*/ false);

  copyInBuf(in_buf, buf_n);

  // 1 + absolute position of the final sample to output now
  _buf_end_abs = _curr_samp_abs + buf_n;

  // process fft-blks in a loop
  while (1) {
    paramsChk();

    // how much extra data is there over what we need to process the blk?
    _extra_data = _x0_n + _data_pre_x0_n - _next_blk_fwd_n - _freq_fft_n;

    // determine whether we are forced to output a blk because of the delay or
    // we've run out of input buffer
    bool force_out = (_buf_end_abs - _dst_fft_abs > 0 || _x0_n >= _x0_force_out_sz);

    // chk whether we should process the blk (forced to output or we have interpolate params
    // and we have enough data for a blk)
    bool should_proc = force_out || (_params_state == PARAMS_INTERP_OK && _extra_data >= 0);
    if (!should_proc)
      break;

    findBlkInPos();

    _mixback = get(&GetInterp, _mixback_param);

    // blk mix update
    _blk_mix_fn_n = get(&GetInterp, _blk_mix_param, _blk_mix_fn);

    if (_mixback >= 1.0f) {
      prepMixOut();
      // no ffts because 100% mixback
      for (int i = 0; i < AUDIO_CHANNELS; i++) {
        float* x0_dat = _chan[i].x0;
        mixToX3(P1Src(x0_dat + _x0_i), i);
      }
    }
    else {
      // normal case, we need to do the FFTs
      doFFT();
      if (gui())
        gui()->FFTDataRdy(0 /*input*/);
      prepMixOut();

      procFFT();
      if (gui())
        gui()->FFTDataRdy(1 /*output*/);
      ifftAndMixOut();
    }
    nextBlk();

    // ensure stop if we run out of data to process
    if (_extra_data <= 0)
      break;
  }

  //
  ASSERTX(_buf_end_abs == _curr_samp_abs + buf_n,
          VAR(_buf_end_abs) << VAR(_curr_samp_abs) << VAR(buf_n));

  //
  zeroFillOutput();

  // update absolute sample position
  _curr_samp_abs = _buf_end_abs;
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::process(float** inputs, float** outputs, VstInt32 samps)
// virtual, override AudioEffect
// called by vst-host to process data from "inputs" and add to "outputs"
{
  SCOPE_NO_FP_EXCEPTIONS_OR_DENORMALS;

  _process(inputs, samps);
  for (int i = 0; i < AUDIO_CHANNELS; i++) {
    PAddOut p(outputs[i]);
    wrapProcess(p, _chan[i].x3, _x3_o, samps);
  }
  _x3_o = wrap(samps + _x3_o, _chan[0].x3);
}

//-------------------------------------------------------------------------------------------------
void DtBlkFx::processReplacing(float** inputs, float** outputs, VstInt32 samps)
// virtual, override AudioEffect
// called by vst-host to process data from "inputs" and replace data in "outputs"
{
  SCOPE_NO_FP_EXCEPTIONS_OR_DENORMALS;

  _process(inputs, samps);
  for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
    PCopyOut p(outputs[ch]);
    wrapProcess(p, _chan[ch].x3, _x3_o, samps);
  }

  _x3_o = wrap(samps + _x3_o, _chan[0].x3);
}
