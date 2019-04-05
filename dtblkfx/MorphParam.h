#ifndef _DT_MORPH_PARAM_H_
#define _DT_MORPH_PARAM_H_

#include <valarray>

#include "ParamsDelay.h"
#include "misc_stuff.h"

class MorphParam;

//-------------------------------------------------------------------------------------------------
struct MorphParamArrayAccess
// array style access to MorphParam
{
  _Ptr<MorphParam> morph;

  //
  int idx;

  // since morph params are actually 2-d arrays, this controls the direction
  bool idx_is_anchor; // false = idx is param number

  int size();
  float operator[](int i) const;
  float& operator[](int i);
};

//-------------------------------------------------------------------------------------------------
class MorphParam
//
// each morph param has a particular number of points that are interpolated using
// a vst-param as an index so as to produce the final param
{
public:
  // interpolation mode
  enum Mode {
    MODE_FIXED,      // ignore vst_param_idx & always use anchor_data[0]
    MODE_LIN_INTERP, // linear interpolate morph data
    MODE_STEP,       // step morph data
    MODE_CLOSEST_ANCHOR = MODE_STEP
  };

protected: // internals
  // morph control point data is actually 2d in format [anchor idx][param idx]
  std::valarray<float> anchor_data;

  // number of morph anchor points (anchor_data.size() >= anchor_n*param_n)
  int anchor_n;

  // number of params
  int param_n;

  // which vst param we're attached to
  int vst_param_idx;

  // interpolation mode (see above)
  Mode mode;

public:
  // auto cast to the mode
  Mode getMode() const { return mode; }
  operator Mode() const { return mode; }
  MorphParam& operator=(Mode mode_)
  {
    mode = mode_;
    return *this;
  }

  void setModeFixed() { mode = MODE_FIXED; }
  void setModeLin(int vst_param_idx_)
  {
    vst_param_idx = vst_param_idx_;
    mode = MODE_LIN_INTERP;
  }
  void setModeStep(int vst_param_idx_)
  {
    vst_param_idx = vst_param_idx_;
    mode = MODE_STEP;
  }

public:
  MorphParam()
  // default to fixed, 1 param and 1 morph point
  {
    setModeFixed();
    param_n = 1;
    anchor_n = 1;
    vst_param_idx = 0;
    anchor_data.resize(/*num params*/ param_n * anchor_n, /*value*/ 0);
  }

  // access methods
  int /*-1 if in MODE_FIXED*/ vstParamIdx() const
  {
    return mode == MODE_FIXED ? -1 : vst_param_idx;
  }

  int numParams() const { return param_n; }
  int numAnchorPoints() const { return anchor_n; }

  // set morph table size
  void setNumAnchorPoints(int n)
  {
    // only grow (so data is not lost if shrunk)
    if (n > anchor_n)
      anchor_data.resize(n * param_n, 0.0f);
    anchor_n = n;
  }

  // set the number of params
  void setNumParams(int n)
  {
    int old_param_n = param_n;
    param_n = n;

    // resize and clear
    std::valarray<float> old_anchor_data = anchor_data;
    anchor_data.resize(anchor_n * param_n, 0.0f);

    // copy old morph params across
    int bytes_cpy = sizeof(float) * std::min(old_param_n, param_n);
    for (int i = 0; i < anchor_n; i++)
      memcpy(anchorData(i), begin(old_anchor_data) + i * old_param_n, bytes_cpy);
  }

  // set all morph points from "src_data", src data must be contiguous 2d array
  // in format [anchor_idx][param_idx]
  void setAll(Rng<float> src_data, int param_n_ = 1)
  {
    param_n = param_n_;
    anchor_n = src_data.n / param_n;
    anchor_data.resize(param_n * anchor_n);
    src_data.copyTo(begin(anchor_data));
  }

  // return anchor array for a particular param
  MorphParamArrayAccess anchorArray(int param_idx = 0)
  {
    MorphParamArrayAccess a;
    a.morph = this;
    a.idx_is_anchor = false;
    a.idx = param_idx;
    return a;
  }

  // return array for a particular morph set
  MorphParamArrayAccess paramArray(int anchor_idx = 0)
  {
    MorphParamArrayAccess a;
    a.morph = this;
    a.idx_is_anchor = true;
    a.idx = anchor_idx;
    return a;
  }

  // limit anchor index
  int limitAnchorIdx(int anchor_idx) const { return limit_range(anchor_idx, 0, anchor_n - 1); }

  int limitParamIdx(int param_idx) const { return limit_range(param_idx, 0, param_n - 1); }

  // get params morph data for a particular anchor index (data is param_n long)
  float* anchorData(int anchor_idx) const
  {
    const float* it = std::begin(anchor_data);
    return (float*)it + limitAnchorIdx(anchor_idx) * param_n;
  }

  float& anchorData(int anchor_idx, int param_idx) { return anchorData(anchor_idx)[param_idx]; }

  // given a fraction, return closest anchor idx
  int closestAnchorIdx(float /*0..1*/ idx_f) const
  {
    return RndToInt(lin_interp_limit(idx_f, 0.0f, (float)anchor_n - 1));
  }

  // find what fraction a particular anchor index corresponds to
  float /*0..1*/ anchorIdxToFrac(int anchor_idx) const
  {
    return anchor_n > 1 ? (float)limitAnchorIdx(anchor_idx) / ((float)anchor_n - 1) : 0.5f;
  }

  // return anchor data from rounded morph position (equivalent to MODE_STEP)
  float* closestAnchorData(float /*0..1*/ idx_f) const
  {
    return anchorData(closestAnchorIdx(idx_f));
  }

  // get a particular anchor point
  float get(int anchor_idx, int param_idx = 0) const
  {
    return anchorData(anchor_idx)[limitParamIdx(param_idx)];
  }

  // set a morph control point to "value"
  void set(float value, int anchor_idx = 0, int param_idx = 0)
  {
    anchorData(anchor_idx)[limitParamIdx(param_idx)] = value;
  }

  // get morph data given the control param
  int                    /*num values written*/
  get(float morph_pos,   // control position of morph (ignored if MODE_FIXED), 0..1
      Rng<float> result, // destination
      int param_idx,     // first index of param to return
      int /*Mode*/ mode_ // force a different mode
      ) const
  {
    int max_n = param_n - param_idx;
    if (result.n > max_n)
      result.n = max_n;
    if (result.n <= 0)
      return 0;

    // otherwise normal interpolation
    switch (mode_) {
      case MODE_LIN_INTERP: {
        // linear interpolation
        float t = morph_pos * (float)(anchor_n - 1);
        float floor_t = floorf(t);
        int i = (int)floor_t;
        const float* src0 = anchorData(i) + param_idx;
        const float* src1 = anchorData(i + 1) + param_idx;
        float mix1 = t - floor_t;
        float mix0 = 1 - mix1;

        for (; result.n; result.adjStart(1))
          *result = *src0++ * mix0 + *src1++ * mix1;
        break;
      }

      case MODE_STEP:
        // copy closest anchor data
        result.copyFrom(closestAnchorData(morph_pos) + param_idx);
        break;

      default:
        // always copy from anchor data morph position 0
        result.copyFrom(anchorData(/*anchor idx*/ 0) + param_idx);
    }
    return result.n;
  }

  // see get() above, use current mode
  int get(float morph_pos, Rng<float> result, int param_idx = 0) const
  {
    return get(morph_pos, result, param_idx, mode);
  }

  // get results using morph param out of "vst_param_array"
  // e.g
  //     MorphParam my_param;
  //     my_param.setNumParams(4);
  //
  //     float result[4];
  //
  //     // get control param out of an array of float
  //     float my_float_array[10];
  //     my_param.update(my_float_array, /*out*/result);
  //
  //     // get control param out of a params delay
  //     ParamsDelay<10> my_paramsdelay;
  //     my_param.update(my_paramsdelay.getInterpFn(), /*out*/result);
  //
  template <class T>
  int /*elements copied*/ get(const T& vst_param_array, Rng<float> result, int param_idx = 0) const
  {
    return get(mode != MODE_FIXED ? vst_param_array[vst_param_idx] : 0.0f, result, param_idx);
  }

  // get a single param
  template <class T> float getSingle(T& t /*float 0..1 or array of float*/, int param_idx = 0) const
  {
    Array<float, 1> r;
    get(t, /*out*/ r, param_idx);
    return r[0];
  }

  // operator() calls "getSingle"
  template <class T> float operator()(const T& t, int param_idx = 0) const
  {
    return getSingle(t, param_idx);
  }

  //
  bool /*got point*/ nextLineDrawPoint(int& state, float& /*0..1*/ x, float& /*0..1*/ y,
                                       int param = 0)
  // note y is "upside down" so that anchor point 0 can be drawn at the bottom of the window
  {
    if (mode != MODE_STEP) {
      // assume linear
      // state is directly anchor index
      if (state >= anchor_n)
        return false; // no more points
      x = get(state, param);
      y = 1.0f - anchorIdxToFrac(state);
      state++;
      return true;
    }

    // MODE STEP
    if (state >= anchor_n * 2)
      return false;
    x = get(state / 2, param);
    float y_stp = 1.0f / (float)(anchor_n - 1);
    float y_mult = ((float)state + 1) / 2 - 0.5f;
    y = limit_range(y_stp * y_mult, 1.0f, 0.0f);
    state++;
    return true;
  }

  // auto cast to pointer
  operator MorphParam*() { return this; }
};

//-------------------------------------------------------------------------------------------------
// allow MorphParam to be used as a vstparam index
// make sure that you check that vstParamIdx() is != -1 before passing
inline int toVstParamIdx(const MorphParam& morph)
{
  return morph.vstParamIdx();
}
inline int toVstParamIdx(const MorphParam* morph)
{
  return toVstParamIdx(*morph);
}

//-------------------------------------------------------------------------------------------------
inline int MorphParamArrayAccess::size()
{
  if (idx_is_anchor)
    return morph->numParams();
  return morph->numAnchorPoints();
}

inline float MorphParamArrayAccess::operator[](int i) const
{
  if (idx_is_anchor)
    return morph->get(idx, i);
  return morph->get(i, idx);
}

inline float& MorphParamArrayAccess::operator[](int i)
{
  if (idx_is_anchor)
    return morph->anchorData(idx, i);
  return morph->anchorData(i, idx);
}

#endif
