#ifndef _DT_SPECTROGRAM_H_
#define _DT_SPECTROGRAM_H_
/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Pls see Spectrogram.cpp

History
        Date       Version    Programmer         Comments
        16/2/03    -        Darrell Tam		Buzz version
    01/12/05   1.0        DT                 ported to VST

This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

***************************************************************************************************/

#include "PixelFreqBin.h"
#include "VstGuiSupport.h"
#include "misc_stuff.h"

//------------------------------------------------------------------------------------------
class Spectrogram : public CViewContainer {
public:
  enum { NUM_HILITE_RECT = 3 };
  typedef Array<int, NUM_HILITE_RECT * 2> HiliteArray;

public: // initialization routines that MUST be called before use
  Spectrogram();
  virtual ~Spectrogram();

  // set the frequency-pixel mapping & initialize (must call before use)
  void init(CRect rect,                 // window position
            Rng<float> pix_hz,          // pixel to freq mapping (Spectrogram does NOT take a copy)
            Rng<unsigned long> col_map, // colour map (pointers taken, memory is not copied)
            float octave_frac           // fraction of total span per octave
  );

  // connect us with another sgram
  void linkSgram(Spectrogram* next) { _linked_sgram = next; }

  // set the name to display in the name rectangle
  void setName(const char* name)
  {
    _name = name;
    _name_dirty = true;
  }

  // one of the following updates must be called regularly to perform scrolling and so forth
  void update();
  void update(CDrawContext* context);

  // get image - to do: could un-circular buf image so that caller can draw into it
  CContextRGBA* getImage() { return &_image; }

  CRect getDisplayRect() const { return _display_rect; }

  // call when main effect suspends
  void suspend();

  // retunr true if sgram was init'd ok
  bool ok() { return _ok; }

  // only call newData & blkDone if rdy() returns true
  bool rdy() { return _ok && !paused; }

  // supply new FFT data
  void newData(cplxf** bin_rng, // appropriate bin rng's from PixelFreqBin (must match pix_freq
                                // passed to init)
               float pwr_scale  // scaling to apply to data
  );

  // call after "newData()" has been called for each channel to update sample position
  void blkDone(
      long samp_pos,  // absolute sample position for start of this blk
      long time_fft_n // number of samples in time domain (may be less samples than fft freq data)
  );

  // return pixel corresponding to fraction
  int fracToPix(float /*0..1*/ v)
  {
    int w = _image.getWidth() - 1;
    return limit_range(RndToInt(v * (float)w), 0, w);
  }

  // set vertical line using pixel position (as an offset into the client rect)
  void setHilitePix(int i, int x) { _hilite_x[1][i] = limit_range(x, 0, _image.getWidth() - 1); }

  // set vertical line using a fraction
  void setHiliteFrac(int i, float /*0..1*/ v) { setHilitePix(i, fracToPix(v)); }

  // clear the vertical line
  void clrHilite() { SetAllInRng(_hilite_x[1], -1); }

  // return hilite array that will be displayed
  HiliteArray& hilite() { return _hilite_x[1]; }

  // set info freq
  void setInfoFreq(float hz) { _info_hz[1] = hz; }

  // given a pixel, return hz
  float getPixToHz(int x) { return _pix_hz.get(x); }

public:
  // height of octave ticks at bottom in pixels
  int tick_height;

  // frame & tick colour
  CColor frame_col;

  // minimum number of samples per line of display (to limit scrolling speed)
  int min_samps_per_line;

  // client area (display area within borders & octave ticks)
  CRect _display_rect;

  // text rectangle to display mouse position info
  CRect info_rect;

  // rectange to display menu text
  CRect name_rect;

  // whether or not we're paused
  bool paused;

public: // override CViewContainer methods
  virtual CMouseEventResult
  onMouseDown(CPoint& mouse, const long& buttons); ///< called when a mouse down event occurs
  virtual CMouseEventResult
  onMouseMoved(CPoint& mouse, const long& buttons); ///< called when a mouse move event occurs
  virtual CMouseEventResult
  onMouseEntered(CPoint& mouse, const long& buttons); ///< called when the mouse enters this view
  virtual CMouseEventResult
  onMouseExited(CPoint& mouse, const long& buttons); ///< called when the mouse leaves this view

  virtual void draw(CDrawContext* context); ///< called if the view should draw itself
  virtual void drawRect(CDrawContext* context,
                        const CRect& update_rect); ///< called if the view should draw itself

  virtual bool removed(CView* parent); ///< view is removed from parent view

  // override this so that we get mouse moves without the mouse being pressed
  virtual CView* getViewAt(const CPoint& p, bool deep) const;

protected: // internals
  bool updateHiliteRects(CDrawContext* context);
  void drawDirtyOverlay(CDrawContext* context);

  unsigned long mapCol(float in);

  void
  getMaxVals(cplxf** bin_rng, // appropriate bin rng's from PixelFreqBin
             bool empty_line, // whether this is a new line (pass as constant to help optimizer)
             float pwr_scale  // pwr scale to apply
  );

  void drawBm(CDrawContext* context, //
              CRect clip_rect,       // limit drawing to this clipping rectangle
              bool invert_draw       // draw inverted
  );

  void drawBm(CDrawContext* context, //
              CRect clip_rect        // limit drawing to this clipping rectangle
  );

protected:
  // spectrograms can be linked together to show the same hilite, etc
  _Ptr<Spectrogram> _linked_sgram;

  // pixel to frequency mapping
  Rng<float> _pix_hz;

  // map to use for converting FFT bin pwr into a colour (host format RGBA)
  Rng<unsigned long> _col_map;

  // pixels per octave
  float _pix_per_octave;

  // colour scaling of log(pwr(fft data))
  float _col_log_offs, _col_log_scale;

  // scaling to use on FFT bin pwr when mapping to a colour
  float _pwr_min, _pwr_max;

  // 2D image data array
  // lines are arranged in a circular buffer
  CContextRGBA _image;

  // current line position in image_data & _samp_pos
  int _image_i;

  // we can display up to this line
  int _disp_i[2]; // [0]=currently displayed, [1]=can display to

  // absolute sample position of each line in the image_data
  std::valarray<long> _line_samp_pos;

  // current max values from fft for each pixel
  std::valarray<float> _curr_max;

  // sample position of previously displayed line
  long _curr_line_samp_pos;

  // true if a line has just been output
  bool _curr_line_is_empty;

  // vertical line under mouse (pixels in sgram relative coords)
  // rectangle "i" : [][i*2]=start pixel, [][i*2+1]=end pixel
  // [0][]=current drawn, [1][]=new value (value of -1=don't draw)
  HiliteArray _hilite_x[2];

  // info hz to display
  float _info_hz[2];

  //
  typedef enum { OVERLAY_NOT_DRAWN, OVERLAY_DRAWN, OVERLAY_NEEDS_UPDATE } OverlayDrawState;

  // mouse info text state
  OverlayDrawState _info_draw_state;

  // menu text state
  OverlayDrawState _name_draw_state;

  // true if name has changed and needs to be redrawn
  bool _name_dirty;

  // name displayed if menu is not set
  const char* _name;

  // true when all was init'd ok
  bool _ok;
};

#endif
