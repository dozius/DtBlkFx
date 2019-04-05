#ifndef _VST_PROGRAM_
#define _VST_PROGRAM_
/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

***************************************************************************************************/

#include "misc_stuff.h"
#include <string.h>

inline int PackedBytesPerVstProgram(int n_params)
{
  return 32 + 4 * n_params;
}

//-------------------------------------------------------------------------------------------------
template <int N_PARAMS> struct VstProgram {
  // name of this program: according to VST SDK program names must have space for at least 24 bytes
  CharArray<32> name;

  // array of params
  Array<float, N_PARAMS> params;

public: // name access
  void setName(const char* name_) { name << name_; }

  CharArray<32>& getName()
  {
    name.terminate(); // ensure null terminated
    return name;
  }

  void clrName() { Clear(name); }

  // number of bytes when loading/saving little endian binary
  static int packedBytes() { return PackedBytesPerVstProgram(N_PARAMS); }

  //
  void clear()
  {
    Clear(name);
    Clear(params);
  }

public: // constructors
  // default cosntructor
  VstProgram() { clear(); }

  // build from a string that contains "<name>:<param> <param> ..."
  VstProgram(const char* load_str) { loadFromString(load_str); }

public: // assignment from another
  template <int N_PARAMS_IN> VstProgram& operator=(const VstProgram<N_PARAMS_IN>& src)
  // can assign from another program of a different length
  {
    clear();

    name << src.name;

    // copy what we can
    Copy(/*dst*/ params, src.params, std::min(N_PARAMS_IN, N_PARAMS));

    return *this;
  }

public:                               // various ways of loading
  void loadFromString(const char* str // string containing contains "<name>:<param> <param> ..."
  )
  {
    clear();

    int i = 0;

    // copy string to name until ':' or end of string
    while (*str) {
      if (*str == ':') {
        str++;
        break;
      }

      if (idx_within(i, name))
        name[i++] = *str;
      str++;
    }
    name[i] = '\0';

    for (i = 0; i < N_PARAMS; i++) {
      char* next_str = NULL;

      // get the next param
      params[i] = limit_range((float)strtod(str, &next_str), 0.0f, 1.0f);

      // go to the next place in the string, return if end-of-string
      if (/*end-of-string*/ next_str == str)
        break;
      str = next_str;
    }
  }

  bool loadLittleEndian(LittleEndianMemStr* src_data, // data to load from
                        int num_params = N_PARAMS     // number of floats to read from "data"
  )
  {
    clear();

    // expect 32 character name
    if (!ShiftSrc(name, src_data))
      return false;
    name.terminate();

    // copy floats
    int i;
    for (i = 0; i < num_params && i < N_PARAMS; i++)
      if (!src_data->get32(params + i))
        return false;

    // read dummy values
    float dummy;
    for (; i < num_params; i++)
      if (!src_data->get32(&dummy))
        return false;

    return true;
  }

  bool saveLittleEndian(LittleEndianMemStr* dst_data, // data to load from
                        int num_params = N_PARAMS     // number of floats to save
  )
  {
    // save 32 char name
    ShiftDst(/*dst*/ dst_data, /*src*/ name);

    //
    int i;
    for (i = 0; i < num_params && i < N_PARAMS; i++)
      if (!dst_data->put32(params[i]))
        return false;

    for (; i < num_params; i++)
      if (!dst_data->put32(0.0f))
        return false;

    return true;
  }
};

#endif
