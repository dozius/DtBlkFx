#ifndef _DT_NOTE_FREQ_H_
#define _DT_NOTE_FREQ_H_
/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Routines to convert between freq (hz) and musical notes plus some constants


History
        Date       Version    Programmer         Comments
        16/2/03    1.0        Darrell Tam		Created


This is completely free software

***************************************************************************************************/

#include "misc_stuff.h"
#include <math.h>

namespace NoteFreq {
const float a4_hz = (float)440.000031;
const float c0_hz = (float)16.35159898333181161; // a4_hz*powf(2.0f, -(4+9/12))
const float log_2 = (float)0.69314718055994528623;
const float log_c0_hz = (float)2.7943256897071098166;
const float inv_log_c0_hz = (float)1.442695040888963387;    // 1/logf(2)
const float mult_next_note = (float)1.0594630943592953098;  // 2^(1/12)
const float mult_prev_note = (float)0.94387431268169352805; // 2^(-1/12)
extern char* note_names[12];
}; // namespace NoteFreq

// given a note as a string, convert to hz
float /*freq hz*/ NoteToHz(string txt);

// return frequency of a note, where note is specified as the number of semitones away from "c0"
// where note_offs_c0 means c0 is 0, ..., c1 is 12, c#1 is 13, d1 is 14, ...
inline float NoteOffsToHz(float note_offs_c0)
{
  return NoteFreq::c0_hz * powf(2.0f, note_offs_c0 * (1.0f / 12.0f));
}

// given frequency in hz, return the offset in semitones from "c0" (same meaning as in NoteOffsToHz)
inline float /*offset from c0*/ HzToNoteOffs(float hz)
{
  using namespace NoteFreq;
  return 12.0f * inv_log_c0_hz * (logf(hz) - log_c0_hz);
}

// string format of notes: <note>[#]:<cents> to Hz
// example: c#4:-45

// since there is no key specified, half notes are always specified sharp (which
// is better for typing and printing anyway)

// conversion uses equal tempered scale

// convert note offs from c0 to txt
Rng<char> /*remaining string*/ NoteToTxt(Rng<char> out_txt, float offs_c0);

// convert hz to a string
// maximum string printed is 8 characters (for non-ridiculous frequencies)
inline Rng<char> /*remaining string*/ HzToNote(Rng<char> out_txt, float freq)
{
  return NoteToTxt(out_txt, HzToNoteOffs(freq));
}

// "stream" versions
// e.g.
//   CharArray<256> str;
//   float freq = 440.0f;
//   str << freq << " is " << HzToNote(440.0f);
//
struct NoteToTextStruct {
  float offs_c0;
};
inline NoteToTextStruct NoteToTxt(float offs_c0)
{
  NoteToTextStruct r = {offs_c0};
  return r;
}
inline NoteToTextStruct HzToNote(float freq)
{
  NoteToTextStruct r = {HzToNoteOffs(freq)};
  return r;
}
inline CharRng operator<<(CharRng dst, NoteToTextStruct s)
{
  return NoteToTxt(dst, s.offs_c0);
}

#endif
