/******************************************************************************

Generic Parameter FIFO for VST params

History
        Date       Version    Programmer         Comments
        16/2/03    1.0        Darrell Tam		 Created
        14/2/04    1.1        DT                 Changed to template based
        9/11/05    1.2        DT                 Ported from buzz to vst

This is completely free software

Notes,
was previously a template based on number of params but this made is clumsy
when moving functions outside because they would all need to be templated
******************************************************************************/

#ifndef _DT_PARAMS_DELAY_H_
#define _DT_PARAMS_DELAY_H_

#include "misc_stuff.h"
#include <vector>

//-------------------------------------------------------------------------------------------------
struct VstParamIdx
    : public BuiltinWrapper<int>
// wrapper for vst param indexes (int) so that MorphParam can be used as an index directly
{
  VstParamIdx() {}
  template <class T> VstParamIdx(const T& t) { val = toVstParamIdx(t); }
};

// usual thing that VstParamIdx's are constructed from
inline int toVstParamIdx(int i)
{
  return i;
}

//-------------------------------------------------------------------------------------------------
class ParamsDelay {
protected:
  // delay length
  int _length;

  // number of params
  int _n_params;

  // current input position
  int _in;

  // current output position (we interpolate between these)
  int _out_a, _out_b;

  // expected distance between samp_abs_new in Params (normally matches a fraction of the
  // tempo, e.g. samples per quarter-note). This is used to control interpolation - params
  // are only interpolated within this distance up to a change
  long _expected_dist;

  // current interpolation fraction of between _out_a & _out_b (set by setOutPos)
  // 0.0f=_out_a, 1.0f=_out_b
  float _interp_frac;

  // increment index "i", wrapping to 0 if need be
  void incIndex(int& i)
  {
    if (++i >= _length)
      i = 0;
  }

  // memory for array of param record, records have the following structure
  //
  // struct Record
  // {
  //     long samp_abs; // absolute sample position
  //     float vals[_n_params];
  //     bool any_explicit_set;
  //     bool explicit_set[_n_params];
  // };
  std::vector<char> _data;

  // std::vector<long> _samp_abs;
  // std::vector<float> _vals;

  // params can be overridden
  std::vector<bool> _use_override_param;
  std::vector<float> _override_param;

  // offset of each field in each data record
  int _field_byte_offs[2], _record_len_bytes;

protected:
  // access a record from _data (which is just an array of bytes) & cast
  template <class T> T* data(int i, int byte_offs) const
  {
    return (T*)(begin(_data) + i * _record_len_bytes + byte_offs);
  }

  // access various things in _data, do all this stuff our selves so that we create
  // contiguous records of any size
  long& samp_abs(int row) const { return *data<long>(row, 0); }
  float* vals(int row) const { return data<float>(row, sizeof(long)); }
  bool& any_explicit_set(int row) const { return *data<bool>(row, _field_byte_offs[0]); }
  bool* explicit_set(int row) const { return data<bool>(row, _field_byte_offs[1]); }

  // get param "param_idx" value from "row" using override if need be
  float /*0..1: ok*/ getVal(int row, int param_idx, bool allow_override = true) const
  {
    return _use_override_param[param_idx] && !allow_override ? _override_param[param_idx]
                                                             : vals(row)[param_idx];
  }

public:
  bool isParamIdxOk(int idx) { return idx >= 0 && idx < _n_params; }

  // must call init before use!
  void init(int n_params, int length)
  {

    _length = length;
    _n_params = n_params;

    // find offsets for data in our array
    _field_byte_offs[0] =
        sizeof(long) +
        sizeof(float) * _n_params; // any_explicit_set array: skip samp_abs_new field & vals array
    _field_byte_offs[1] =
        _field_byte_offs[0] + sizeof(bool); // explicit_set array: skip any_explicit_set
    _record_len_bytes =
        _field_byte_offs[1] + sizeof(bool) * _n_params; // total length: skip explicit_set

    // allocate required memory
    _data.resize(_record_len_bytes * _length);

    // clr first record
    memset(&begin(_data), 0, _record_len_bytes);

    // by default no values are forced
    _override_param.resize(n_params, 0.0f);
    _use_override_param.resize(n_params, false);

    init_();
  }

  ParamsDelay() // don't forget to call init before use
  {
    _expected_dist = 0x7fffffff; // large initial setting (disable)
  }

  void setExpectedDist(int n) { _expected_dist = n; }

  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
public: // parameter override (use for param preview)
  void setOverride(VstParamIdx idx, bool state) { _use_override_param[idx] = state; }
  void setOverride(VstParamIdx idx, float v)
  {
    setOverride(idx, true);
    _override_param[idx] = v;
  }
  float /*0..1*/ getOverride(VstParamIdx idx) const { return _override_param[idx]; }
  bool isOverridden(VstParamIdx idx) const { return _use_override_param[idx]; }

  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
public:
  // use "setOutPos" to set the sample position
  // get the most recent input value to the delay
  float getInput(VstParamIdx idx) const { return getVal(_in, idx); }
  float getInputNoForced(VstParamIdx idx) const { return getVal(_in, idx, /*use forced*/ false); }

  // get non-interpolated value of current param position rounded down (prev) or up (next)
  float getPrev(VstParamIdx idx) const { return getVal(_out_a, idx); }
  float getNext(VstParamIdx idx) const { return getVal(_out_b, idx); }

  // get linear interpolated value of current position
  float getInterp(VstParamIdx idx) const
  {
    return lin_interp(_interp_frac, getPrev(idx), getNext(idx));
  }

  // get the absolute sample position for the current param or next out param
  long getOutSampAbs(bool next_param = false) const
  {
    return samp_abs(next_param ? _out_b : _out_a);
  }

  // get timestamp of current input
  long getInputSampAbs() const { return samp_abs(_in); }

  // return true if "getNonInterp()" corresponds to an explicitly set parameter
  bool isExplicitlySet(VstParamIdx idx, bool next_param = false) const
  {
    return explicit_set(next_param ? _out_b : _out_a)[idx];
  }

  // return true if any param is explicitly set
  long isAnyExplicitSet(bool next_param = false) const
  {
    return any_explicit_set(next_param ? _out_b : _out_a);
  }

  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void resetAndCopyIn()
  // reset the params vector and copy the most recent params but reset abs time to 0
  {
    // copy current input params to time 0
    memcpy(data<char>(/*idx*/ 0, /*byte offs*/ 0),
           data<char>(/*idx*/ _in, /*byte offs*/ 0),
           _record_len_bytes);

    // reset sample abs time
    samp_abs(0) = 0;

    init_();
  }

  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool /*can go to next?*/ nextOutParam()
  {
    // set the position to what was the next param
    _out_a = _out_b;
    _interp_frac = 0.0f;

    // if we can't increment then we can't get to the next param
    if (_out_b == _in)
      return false;

    incIndex(_out_b);
    return true;
  }

  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool /*already exists*/ put(long samp_abs_new, VstParamIdx idx, float value)
  //
  // put a parameter with index "idx" and value "value" into a param node at absolute
  // sample position "samp_abs_new"
  //
  // return true if the param already exists
  {
    bool already_exists = false;
    // does this param have the same time as the most recent one?
    long samp_diff = samp_abs_new - samp_abs(_in);
    if (samp_diff <= 0)
      already_exists = true;
    else {
      // make a new Params entry if the time is different

      // make an extra entry point if the time gap is bigger than the expected
      // parameter distance
      if (samp_diff > _expected_dist) {
        cpInNode();
        any_explicit_set(_in) = false;
        samp_abs(_in) = samp_abs_new - _expected_dist;
      }
      cpInNode();
      any_explicit_set(_in) = true;
      samp_abs(_in) = samp_abs_new;
    }
    vals(_in)[idx] = value;
    explicit_set(_in)[idx] = true;
    return already_exists;
  }

  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  bool /*can interpolate*/ setOutPos(long samp_abs_new, bool find_fraction = true)
  //
  // set output sample position, if we can
  // Note, only move forward (don't try to go backwards)
  //
  {
    // look for params
    while (1) {

      if (samp_abs(_out_b) > samp_abs_new) {
        // find distance from requested sample to _out_a position
        long numer = samp_abs_new - samp_abs(_out_a);

        // can't go in reverse, samp_abs_new lies before "_out_a"
        if (numer < 0) {
          _interp_frac = 0.0f;
          return false;
        }
        // can interpolate ok

        // don't do fraction calculation unless needed
        if (!find_fraction)
          return true;

        // distance between params
        float denom = (float)(samp_abs(_out_b) - samp_abs(_out_a));

        // denominator should never be <=0, but just in case
        if (denom <= 0.0f) {
          _interp_frac = 0.0f;
          return true;
        }

        _interp_frac = (float)numer / denom;
        return true;
      }

      // is it an exact match?
      if (samp_abs(_out_b) == samp_abs_new) {
        _out_a = _out_b;
        if (_out_b != _in)
          incIndex(_out_b);
        _interp_frac = 0.0f;
        return true;
      }

      // have we hit the end? Put both out ptrs at the end
      if (_out_b == _in) {
        _out_a = _out_b;
        _interp_frac = 0.0f;
        return false;
      }

      _out_a = _out_b;
      incIndex(_out_b);
    }
  }

protected:
  //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  void cpInNode()
  //
  // make a new node past current "_in" and that is a copy with all "explicit_set" to false
  //
  {
    int next = _in;
    incIndex(next);

    // copy previous _in to new _in
    memcpy(vals(next), vals(_in), sizeof(float) * _n_params);

    // clear explicit sets
    memset(explicit_set(next), 0, sizeof(bool) * _n_params);

    // check whether we've filled the buffer & need to bump the pointers
    if (next == _out_a) {
      _interp_frac = 0.0f;
      incIndex(_out_a);
    }
    if (next == _out_b)
      incIndex(_out_b);

    // done
    _in = next;
  }

  // internal init
  void init_()
  {
    _in = 0;
    _out_a = _in;
    _out_b = _in;
    _interp_frac = 0.0f;
  }
};

//-------------------------------------------------------------------------------------------------
struct ParamsDelayGet
    : public _PtrBase<const ParamsDelay>
// params can be accessed as though they are an array from a particular point
// (input, interp, curr or next positions) see GetXxx functions below
//
{
  typedef float (ParamsDelay::*Fn)(VstParamIdx) const;
  Fn fn;
  ParamsDelayGet(const ParamsDelay& p, Fn f)
  {
    ptr = &p;
    fn = f;
  }

  // allow access as though array
  float operator[](VstParamIdx i) const { return (ptr->*fn)(i); }
  float operator[](int i) const { return (ptr->*fn)(i); }
};

//-------------------------------------------------------------------------------------------------
// access ParamsDelay as an array using these functions
//
inline ParamsDelayGet GetInput(const ParamsDelay& p)
{
  return ParamsDelayGet(p, &ParamsDelay::getInput);
}
inline ParamsDelayGet GetInputNoForced(const ParamsDelay& p)
{
  return ParamsDelayGet(p, &ParamsDelay::getInputNoForced);
}
inline ParamsDelayGet GetPrev(const ParamsDelay& p)
{
  return ParamsDelayGet(p, &ParamsDelay::getPrev);
}
inline ParamsDelayGet GetNext(const ParamsDelay& p)
{
  return ParamsDelayGet(p, &ParamsDelay::getNext);
}
inline ParamsDelayGet GetInterp(const ParamsDelay& p)
{
  return ParamsDelayGet(p, &ParamsDelay::getInterp);
}

// above functions match this type
typedef ParamsDelayGet (*ParamsDelayGetFn)(const ParamsDelay&);

#endif
