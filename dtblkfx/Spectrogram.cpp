/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Spectrogram window
The spectrogram (waterfall) displays FFT data. Colour is used to represent magnitude of power
in FFT. Each block is put at the bottom of the display and the display scrolls upwards.

This class expects to be given a PixelFreqMap by the owning class in order to determine which
FFT bins correspond to which pixels

Bugs:

History
        Date       Version    Programmer         Comments
        16/2/03    -         Darrell Tam		Buzz version
        01/12/05    1.0        Darrell Tam		Ported to VST


This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

***************************************************************************************************/
#include <StdAfx.h>

#include "NoteFreq.h"
#include "Spectrogram.h"
#include "misc_stuff.h"

#define LOG_FILE_NAME "c:\\temp\\temp\\sgram.html"
#include "Debug.h"
using namespace std;

//-------------------------------------------------------------------------------------------------
Spectrogram::Spectrogram()
    : CViewContainer(CRect(), /*CFrame* parent*/ NULL)
{
  LOGG("", "Spectrogram" << VAR(this));

  paused = false;

  _ok = false;
}

Spectrogram::~Spectrogram()
{
  LOGG("", "~Spectrogram" << VAR(this));
}

//-------------------------------------------------------------------------------------------------
void Spectrogram::init(
    CRect rect,        // position & size of window
    Rng<float> pix_hz, // pixel to freq & width of client (Spectrogram does NOT take a copy)
    Rng<unsigned long> col_map, // colour map
    float octave_frac           // fraction of total span per octave
)
//
// set the rectangle size, width is 2 + width of pixel freq map (borders)
//
// expect col map to be already set
{
  LOG("", "Spectrogram::init" << VAR(rect) << VAR(pix_hz.n) << VAR(octave_frac));
  _ok = false;

  //
  tick_height = 1;

  //
  frame_col = CColorRGB(0, 0, 0);

  // could try to get these from somewhere
  _pwr_min = 1e-4f * 1e-4f;
  _pwr_max = 0.2f * 0.2f;

  _col_map = col_map;
  _col_log_offs = logf(_pwr_min);
  _col_log_scale = (float)col_map.n / (logf(_pwr_max) - _col_log_offs);

  min_samps_per_line = 600;

  //
  _name = "";

  /*CView::*/ size = rect;
  LOG("", "Spectrogram::init" << VAR(size));

  //
  _pix_hz = pix_hz;

  // find display rectangle based on pixel to hz mapping
  _display_rect = rect;
  _display_rect.setWidth(pix_hz.n);
  _display_rect.top++;                     // one pixel top edge
  _display_rect.bottom -= tick_height + 1; // one pixel bottom edge & tick height
  _display_rect = MoveXCent(/*cx*/ rect, /*r*/ _display_rect);

  //
  _image.create(pix_hz.n, rect.getHeight());
  if (!_image.ok()) {
    LOG("background:#f00", "Spectrogram::init, failed to create _image");
    return;
  }

  //
  _pix_per_octave = octave_frac * _image.getWidth();

  // fill image with opaque black
  for (int y = 0; y < _image.getHeight(); y++)
    for (RowRGBA p = _image.getRow(y); !p.isEnd(); p.incX())
      p.set(/*r*/ 0, /*g*/ 0, /*b*/ 0);

  // nothing hilited
  SetAllInRng(_hilite_x[0], -1);
  SetAllInRng(_hilite_x[1], -1);

  //
  _curr_max.resize(_image.getWidth());
  _curr_line_is_empty = true;

  LOG("", "Spectrogram::init" << VAR(_display_rect));

  // set initial position into buffer so that line 0 is displayed at the very top of the
  // client rectangle (buffer is unwrapped and can be drawn into normally initially)
  _disp_i[0] = _disp_i[1] = _display_rect.height() - 1;
  _image_i = _disp_i[0] + 1;
  _curr_line_samp_pos = 0;

  // overall rectangle for info_rect & name_rect
  CRect r = rect;
  r.top += 3;
  r.setHeight(18);
  Array<float, 5> text_ratio = {0.2f, 1.2f, 1.2f, 1.0f, 0.2f};
  SplitCRect text_rect(r, text_ratio);

  // skip the start
  text_rect.next();

  // mouse info is drawn here
  info_rect = text_rect.next();
  LOG("", "Spectrogram::init" << VAR(info_rect));

  // skip the middle
  text_rect.next();

  // msg/menu is drawn here
  name_rect = text_rect.next();
  LOG("", "Spectrogram::init" << VAR(name_rect));

  // no freq displayed
  _info_draw_state = OVERLAY_NOT_DRAWN;
  _info_hz[0] = _info_hz[1] = -1;

  // draw name
  _name_draw_state = OVERLAY_NEEDS_UPDATE;
  _name_dirty = true;

  // expect initColMap to have been called first
  if (!_col_map.n) {
    LOG("", "Spectrogram::init, Spectrogram::initColMap should be called first");
    return;
  }

  // accept mouse
  /*CView::*/ setMouseableArea(_display_rect);

  //
  _ok = true;

  //
  setDirty();
}

//-------------------------------------------------------------------------------------------------
void Spectrogram::suspend()
// call when sound suspends to reset sample position
{
  LOG("", "Spectrogram::suspend");
  _curr_line_samp_pos = 0;
}

//-------------------------------------------------------------------------------------------------
inline unsigned long Spectrogram::mapCol(float in)
//
// return the colour corresponding to log of "in"
//
{
  int out;
  if (in > _pwr_min) {
    out = (int)((logf(in) - _col_log_offs) * _col_log_scale + 0.5f);
    if (out >= _col_map.n)
      out = _col_map.n - 1;
  }
  else
    out = 0;

  return _col_map[out];
}

//-------------------------------------------------------------------------------------------------
inline void Spectrogram::getMaxVals(
    cplxf** bin_rng, // appropriate bin rng's from PixelFreqBin
    bool empty_line, // whether this is a new line (pass as constant to help optimizer)
    float pwr_scale  // pwr scale to apply
)
//
// get maximum values for each pixel from bin ranges
{
  // loop over the pixels to find max value in each bin range

  float* curr_max = begin(_curr_max);
  float* curr_max_end = curr_max + _curr_max.size();

  // find max power in for each pixel "n" in the bin range [rng[n], rng[n+1])
  cplxf* bin_a = *bin_rng++; // get first bin
  while (curr_max < curr_max_end) {
    cplxf* bin_b = *bin_rng++; // get end of this bin range

    // always do at least one bin for every output pixel
    float v = norm(*bin_a++) * pwr_scale;
    float max_v;
    if (empty_line)
      max_v = v;
    else {
      max_v = *curr_max;
      if (v > max_v)
        max_v = v;
    }

    // check any other bins up to the next bin position
    while (bin_a < bin_b) {
      v = norm(*bin_a++) * pwr_scale;
      if (v > max_v)
        max_v = v;
    }

    // save max for this pixel
    *curr_max++ = max_v;

    // start of next range is end of this range
    bin_a = bin_b;
  }
}

//------------------------------------------------------------------------------------------
void Spectrogram::newData(cplxf** bin_rng, // appropriate mapping from PixelFreqBin
                          float pwr_scale  // scaling to apply to data
)
// add some new data to the spectrogram
{
  //	LOG("", "newData" << VAR(samp_pos) << VAR(time_fft_n) << VAR(fft_idx));
  if (!_curr_line_is_empty) {
    getMaxVals(bin_rng, /*empty_line*/ false, pwr_scale);
  }
  else {
    _curr_line_is_empty = false;
    getMaxVals(bin_rng, /*empty_line*/ true, pwr_scale);
  }
}

//------------------------------------------------------------------------------------------
void Spectrogram::blkDone(
    long samp_pos,  // absolute sample position for start of this blk
    long time_fft_n // number of samples in time domain (may be less samples than fft freq data)
)
{
  // don't display the line until we have processed enough samples
  if (samp_pos + time_fft_n - _curr_line_samp_pos < /*Spectrogram::*/ min_samps_per_line)
    return;

  // render spectrogram line from the "curr_max" array into current line "_image_i"
  float* src = begin(_curr_max);
  for (RowRGBA dst = _image.getRow(/*y*/ _image_i); !dst.isEnd(); dst.incX())
    dst._set(mapCol(*src++));

  // current line sample position
  _curr_line_samp_pos = samp_pos + time_fft_n;

  // this line is ready to be displayed
  _disp_i[1] = _image_i;

  // circular buffer
  _image_i++;
  if (_image_i >= _image.getHeight())
    _image_i = 0;

  _curr_line_is_empty = true;
}

//-------------------------------------------------------------------------------------------------
void Spectrogram::drawBm(CDrawContext* context, //
                         CRect clip_r,          // limit drawing to this clipping rectangle
                         bool invert_draw       // draw inverted
)
//
// draw spectrogram bitmap, either inverted or non-inverted (not using _hilite_x)
//
// use _disp_i[0] as the index of most recent line in image buffer
{
  // LOG("", "drawBm" << VAR(name) << VAR(clip_r));
  clip_r.bound(_display_rect);
  if (clip_r.width() < 1)
    return;

  // make sure that we aren't trying to draw more than what we have
  if (_display_rect.bottom - clip_r.top > _image.getHeight()) {
    CRect r = clip_r;

    // adjust the output rectangle
    clip_r.top = _display_rect.bottom - _image.getHeight();

    // fill rectangle at top for which there is no data with black
    r.bottom = clip_r.top;
    if (r.height() > 0) {
      context->setFillColor(kBlackCColor);
      context->fillRect(r);
    }
  }

  CRect r = clip_r;

  // perform copy in at most 2 blt's (depending on whether we need to wrap in src buffer)
  //
  int src_x = r.left - _display_rect.left;

  while (1) {
    int n_lines = r.height();
    if (n_lines <= 0)
      break;

    // start line into image bitmap
    int src_i = 1 + _disp_i[0] - (_display_rect.bottom - r.top);
    bool temp = false;

    if (src_i < 0) {
      // wrap to end of source buffer
      src_i += _image.getHeight();

      // limit number of lines to end-of-buffer
      int max_lines = _image.getHeight() - src_i;
      if (max_lines < n_lines)
        n_lines = max_lines;
      temp = true;
    }

    // do the copy
    CRect dst_r = r;
    dst_r.bottom = dst_r.top + n_lines;
    CPoint src_p(src_x, src_i);
    _image.copyTo(context, dst_r, src_p, invert_draw);

    r.top = dst_r.bottom;
  }
}

//-------------------------------------------------------------------------------------------------
void Spectrogram::drawBm(CDrawContext* context, //
                         CRect clip_r           // limit drawing to this clipping rectangle
)
// draw the bitmap according with inverted regions according to _hilite_x array
{

  bool invert_draw = false;
  int x = 0;
  int i = 0;
  while (x < _display_rect.right) {
    int next_x = _hilite_x[0].get(/*idx*/ i++, /*idx out of bounds*/ _display_rect.right);
    if (next_x < x)
      next_x = x;

    CRect r;
    r.left = _display_rect.left + x;
    r.right = _display_rect.left + next_x;
    r.top = clip_r.top;
    r.bottom = clip_r.bottom;
    r.bound(clip_r);

    drawBm(context, r, invert_draw);

    invert_draw = !invert_draw;
    x = next_x;
  }
}

//-------------------------------------------------------------------------------------------------
bool /*false=nothing to be done*/ Spectrogram::updateHiliteRects(CDrawContext* context)
// update vertical rectangles by inverting the difference between currently drawn (_hilite_x[0])
// and new rects (_hilite_x[1])
{
  // is there anything to be done?
  if (_hilite_x[0] == _hilite_x[1])
    return false;

  // whether we are inverting this
  bool do_invert = false;

  // current x position (offset from display rect)
  int curr_x = 0;

  // max x
  int max_x = _display_rect.getWidth();

  // index into hilite_x array
  int idx[2] = {0, 0};

  //
  while (curr_x < max_x) {
    int x[2];
    for (int i = 0; i < 2; i++)
      x[i] = limit_range(_hilite_x[i].get(idx[i], max_x), curr_x, max_x);

    // possibly invert to the left-most position
    int next_idx = x[0] < x[1] ? 0 : 1;
    int next_x = x[next_idx];
    idx[next_idx]++;

    //
    if (do_invert) {
      CRect r;
      r.left = _display_rect.left + curr_x;
      r.right = _display_rect.left + next_x;
      r.top = _display_rect.top;
      r.bottom = _display_rect.bottom;
      if (r.width() > 0)
        InvertRect(context, r);
    }

    do_invert = !do_invert;
    curr_x = next_x;
  }

  // updated
  _hilite_x[0] = _hilite_x[1];

  // redraw overlays if need be
  if (_info_draw_state)
    _info_draw_state = OVERLAY_NEEDS_UPDATE;
  if (_name_draw_state)
    _name_draw_state = OVERLAY_NEEDS_UPDATE;

  //
  return true;
}

//-------------------------------------------------------------------------------------------------
void Spectrogram::drawDirtyOverlay(CDrawContext* context)
// draw whatever overlay stuff is dirty (require that background has already been erased)
// clear dirty flags
{
  // LOG("", "drawOverlay" << VAR(_name));

  // prepare to write some text
  context->setFont(kNormalFont, /*sz*/ 0, kBoldFace);
  context->setFillColor(CColorRGB(255, 255, 255));
  context->setFontColor(CColorRGB(0, 0, 0));

  // draw the vline info if need be
  if (_info_hz[0] != _info_hz[1] || _info_draw_state == OVERLAY_NEEDS_UPDATE) {
    _info_hz[0] = _info_hz[1];

    if (_info_draw_state != OVERLAY_NOT_DRAWN) {
      drawBm(context, info_rect);
      _info_draw_state = OVERLAY_NOT_DRAWN;
    }

    if (_info_hz[0] >= 0) {
      CharArray<64> str;

      str << HzToNote(_info_hz[0]);
      OutlineDrawString(context,                               // draw context
                        LeftHalf(/*Spectrogram::*/ info_rect), // rectangle
                        str,                                   // string
                        kCenterText                            // horiz alignment
      );

      str << sprnum(_info_hz[0], /*min unit*/ 1.0f) << "Hz";
      OutlineDrawString(context,                                // draw context
                        RightHalf(/*Spectrogram::*/ info_rect), // rectangle
                        str,                                    // string
                        kCenterText                             // horiz alignment
      );
      _info_draw_state = OVERLAY_DRAWN;
    }
  }

  // draw the msg if need be
  if (_name_dirty || _name_draw_state == OVERLAY_NEEDS_UPDATE) {
    _name_dirty = false;

    if (_name_draw_state != OVERLAY_NOT_DRAWN)
      drawBm(context, name_rect);

    const char* str = _name;
    if (paused)
      str = "paused";

    // draw some text
    OutlineDrawString(context,    // draw context
                      name_rect,  // rectangle
                      str,        // string
                      kCenterText // horiz alignment
    );
    _name_draw_state = OVERLAY_DRAWN;
  }
}

//-------------------------------------------------------------------------------------------------
void Spectrogram::update(CDrawContext* context)
//
// must be called periodically, performs scrolling if need be and displays mouse information
{
  // work out the number of lines to scroll
  int new_disp_i = _disp_i[1];

  // note: _disp_i[1] may be updated asynchronously to this function - ensure code is multi-thread
  // safe
  int scroll_n = new_disp_i - _disp_i[0];
  if (scroll_n < 0)
    scroll_n += _image.getHeight();

  // if not scrolling: redraw overlay if need be
  if (!scroll_n) {
    updateHiliteRects(context);
    drawDirtyOverlay(context);
    return;
  }

  // scroll upwards
  // LOG("", "update" << VAR(_name) << VAR(scroll_n));

  // remove overlay before scrolling
  if (_info_draw_state)
    drawBm(context, info_rect);
  if (_name_draw_state)
    drawBm(context, name_rect);
  _info_draw_state = OVERLAY_NOT_DRAWN;
  _name_draw_state = OVERLAY_NOT_DRAWN;

  // update the index to most recent line
  _disp_i[0] = new_disp_i;

  // work out what to scroll in frame coords
  CPoint frame_topleft = TopLeft(_display_rect);

  // use the correct local to frame
  CView::localToFrame(frame_topleft);

  // work out scroll region in frame coords
  CRect frame_scroll = MoveTo(frame_topleft, _display_rect);

  // scroll the window
  RECT r = {frame_scroll.left, frame_scroll.top, frame_scroll.right, frame_scroll.bottom};
  HWND wnd = (HWND)(/*CView::*/ getFrame()->getSystemWindow());
  ScrollWindow(wnd, 0, -scroll_n, /*scroll rect*/ &r, /*clip rect*/ &r);

  // force repaint of missing part synchronously
  UpdateWindow(wnd);

  // force redraw of overlay
  _info_hz[0] = -1;
  _name_dirty = true;
  drawDirtyOverlay(context);
}

//-------------------------------------------------------------------------------------------------
void Spectrogram::update()
{
  CViewDrawContext dc(this);
  update(&dc);
}

//-------------------------------------------------------------------------------------------------
void Spectrogram::drawRect(CDrawContext* context, const CRect& update_rect)
// virtual override CViewContainer::drawRect
{
  // LOG("", "drawRect" << VAR(_name) << VAR(_display_rect) << VAR(update_rect));

  // draw borders and ticks unless update_rect is completely contained within display rect
  if (!IsContained(_display_rect, update_rect)) {
    CRect r;

    context->setFillColor(/*Spectrogram::*/ frame_col);

    // top border
    r = /*CView::*/ size;
    r.bottom = _display_rect.top - 1;
    FullFillRect(context, r);
    // bottom border
    r.top = _display_rect.bottom;
    r.bottom = r.top;
    FullFillRect(context, r);
    // left edge
    r.top = size.top;
    r.bottom = _display_rect.bottom;
    r.left = size.left;
    r.right = _display_rect.left - 1;
    FullFillRect(context, r);
    // right edge
    r.left = _display_rect.right;
    r.right = size.right - 1;
    FullFillRect(context, r);

    // octave ticks background
    r.top = _display_rect.bottom + 1;
    r.bottom = size.bottom - 1;
    r.left = size.left;
    r.right = size.right - 1;

    //
    context->setFillColor(/*Spectrogram::*/ ~frame_col);
    FullFillRect(context, r);

    // octave ticks
    context->setFillColor(/*Spectrogram::*/ frame_col);
    for (float x = 0; x < _display_rect.width(); x += _pix_per_octave) {
      r.left = RndToInt(x) + _display_rect.left;
      r.right = r.left;
      FullFillRect(context, r);
    }
  }

  // draw the bitmap & let "update" put on the overlay
  drawBm(context, update_rect);

  // not sure what the state of these are now
  if (_info_draw_state == OVERLAY_DRAWN)
    _info_draw_state = OVERLAY_NEEDS_UPDATE;
  if (_name_draw_state == OVERLAY_DRAWN)
    _name_draw_state = OVERLAY_NEEDS_UPDATE;

  //
  setDirty(false);
}

//-------------------------------------------------------------------------------------------------
void Spectrogram::draw(CDrawContext* context)
// virtual override CViewContainer::draw
//
{
  // redraw everything
  drawRect(context, size);
}

//-------------------------------------------------------------------------------------------------
bool Spectrogram::removed(CView* parent)
// virtual, override CViewContainer::removed
{
  LOG("", "Spectrogram::removed");
  CViewContainer::removed(parent);
  _ok = false;
  return true;
}

//-------------------------------------------------------------------------------------------------
CMouseEventResult Spectrogram::onMouseDown(CPoint& mouse, const long& buttons)
// virtual, override CViewContainer
{
  LOG("", "Spectrogram::onMouseDown");

  // see whether we have any children that will take the event
  CMouseEventResult r = CViewContainer::onMouseDown(mouse, buttons);
  if (r != kMouseEventNotHandled)
    return r;

  // otherwise default behaviour is to pause
  paused = !paused;
  _name_dirty = false;
  return kMouseEventHandled;
}

//-------------------------------------------------------------------------------------------------
CMouseEventResult Spectrogram::onMouseMoved(CPoint& mouse, const long& buttons)
// virtual, override CViewContainer
{
  LOG("", "Spectrogram::onMouseMoved");
  // mouse is inside our rectangle, draw a vertical line under it

  // note that because of the way that VSTGUI works, "mouse" is actually relative to the
  // top-left of our "size" window - not out parent (as is the case with CView)
  int x = mouse.x - _display_rect.left + size.x;
  float /*0..1*/ x_frac = (float)x / (float)(_image.getWidth() - 1);

  // freq to display
  setInfoFreq(_pix_hz.get(x));

  //
  CViewDrawContext dc(getFrame());

  // draw the same thing in all linked sgrams
  Spectrogram* sgram = this;
  while (sgram) {
    sgram->clrHilite();

    int sgram_x = sgram->fracToPix(x_frac);
    sgram->setHilitePix(0, sgram_x);
    sgram->setHilitePix(1, sgram_x + 1);

    OffsetDrawContext(&dc, sgram);
    sgram->update(&dc);

    // get next
    sgram = sgram->_linked_sgram;
    if (sgram == this)
      break;
  }

  return kMouseEventHandled;
}

//-------------------------------------------------------------------------------------------------
CMouseEventResult Spectrogram::onMouseEntered(CPoint& mouse, const long& buttons)
// virtual, override CViewContainer
{
  LOG("", "Spectrogram::onMouseEntered");
  // treat the same as mouse moved
  return Spectrogram::onMouseMoved(mouse, buttons);
}

//-------------------------------------------------------------------------------------------------
CMouseEventResult Spectrogram::onMouseExited(CPoint& mouse, const long& buttons)
// virtual, override CViewContainer
{
  LOG("", "Spectrogram::onMouseExited");

  CViewDrawContext dc(getFrame());

  // mouse has gone out of focus, clear our info and all linked sgrams
  Spectrogram* sgram = this;
  while (sgram) {
    OffsetDrawContext(&dc, sgram);
    sgram->clrHilite();
    sgram->setInfoFreq(-1);
    sgram->update(&dc);

    // get next
    sgram = sgram->_linked_sgram;
    if (sgram == this)
      break;
  }

  return kMouseEventHandled;
}

//-------------------------------------------------------------------------------------------------
CView* Spectrogram::getViewAt(const CPoint& p, bool /*deep*/) const
// virtual, override CViewContainer
// normally CViewContainer doesn't get mouse move events (CFrame uses getViewAt to find the
// target view) - in this one we return ourselves as the target view
{
  if (!pParentFrame)
    return NULL;
  if (!IsInside(_display_rect, p))
    return NULL;
  return const_cast<Spectrogram*>(this);
}
