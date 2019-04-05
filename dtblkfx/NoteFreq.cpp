/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com


History
        Date       Version    Programmer         Comments
        16/2/03    1.0        Darrell Tam		Created


This is completely free software

***************************************************************************************************/
#include <StdAfx.h>

#include "NoteFreq.h"
#include "misc_stuff.h"
#include <map>
#include <string.h>
#include <string>

using namespace std;

namespace NoteFreq {
char* note_names[] = {"c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"};
char* note_names_[] = {"c-", "c#", "d-", "d#", "e-", "f-", "f#", "g-", "g#", "a-", "a#", "b-"};

// given a note & octave, eg "c#5" return hz
typedef map<string, float> NoteToHzMap;
NoteToHzMap note_to_hz;

static struct CreateDestroy {
  CreateDestroy()
  {
    // populate note_to_hz map
    //
    float offs_c0 = 0.0f;
    for (int octave = 0; octave < 20; octave++) {
      for (int note = 0; note < 12; note++) {
        char t[64];
        sprintf(t, "%s%d", note_names[note], octave);
        note_to_hz[t] = NoteOffsToHz(offs_c0);
        offs_c0++;
      }
    }
  }
} g_create_destroy;
}; // namespace NoteFreq
using namespace NoteFreq;

//------------------------------------------------------------------------------------------
Rng<char> NoteToTxt(Rng<char> out_txt, float offs_c0)
{
  // don't print freqs below c0
  if (offs_c0 < 0)
    return out_txt << "-";

  int close = RndToInt(offs_c0);
  int cents = (int)((offs_c0 - (float)close) * 100.0f);
  int octave = close / 12;
  int note12 = close - 12 * octave;
  return out_txt << note_names_[note12] << octave << ":" << sprf("%+03d", cents);
}

//------------------------------------------------------------------------------------------
float /*freq hz*/ NoteToHz(string note)
{
  float cents = 0.0f;
  int colon_pos = note.find(":");
  if (colon_pos != -1) {
    // grab cents
    cents = (float)strtod(note.substr(colon_pos).c_str(), /*endptr*/ NULL);
    // remove everything prior
    note.resize(colon_pos);
  }
  NoteToHzMap::iterator i = note_to_hz.find(note);
  if (i == note_to_hz.end())
    return -1.0f; // can't understand note
  float freq = i->second;
  return freq * powf(2.0f, cents / (12.0f * 100.0f));
}
