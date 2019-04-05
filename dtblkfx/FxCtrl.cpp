/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Each DtBlkFx effect has an FxCtrl gui panel
It contains the following controls:

Sliders for FreqA & FreqB that are aligned to the spectrograms in the main GUI
Amplitude invisi-slider
Effect type pull down menu
Effect value slider


History
        Date       Version     Programmer       Comments
        01/12/05    1.0        Darrell Tam		Created


This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

***************************************************************************************************/
#include <StdAfx.h>

#include "FxCtrl.h"
#include "BlkFxParam.h"
#include "DtBlkFx.hpp"
#include "Gui.h"
#include "Spectrogram.h"

#define LOG_FILE_NAME "c:\\fxctrl.html"
#include "Debug.h"

// get blk fx out of the main gui class
inline DtBlkFx* FxCtrl::blkFx()
{
  return _gui->blkFx();
}

//-------------------------------------------------------------------------------------------------
FxCtrl::FxCtrl()
    : CViewContainer(CRect(0, 0, 0, 0), // const CRect &rect,
                     NULL,              // CFrame *pParent,
                     NULL               // CBitmap *pBackground
      )
{
  LOGG("", "FxCtrl" << VAR(this));
  _ok = false;
}

//-------------------------------------------------------------------------------------------------
FxCtrl::~FxCtrl()
{
  LOGG("", "~FxCtrl" << VAR(this));
}

//-------------------------------------------------------------------------------------------------
template <class T> void setMouseIntercept(const T& ctrl, FxCtrl* fx)
{
  ctrl->setListener(fx);
  ctrl->interceptMouseEvents(/*target*/ fx,
                             /*pre handle*/ &FxCtrl::mouseInterceptBefore,
                             /*post handle*/ &FxCtrl::mouseInterceptAfter);
  AddView(ctrl, fx);
}

//-------------------------------------------------------------------------------------------------
bool /*success*/ FxCtrl::init(Gui* gui, CastCRect rect, int fx_set)
//
{
  int i;

  //
  _ok = false;

  //
  _drag_both_state = DRAG_BOTH_INACTIVE;

  // amp menu has not been built
  _amp_menu_mixmode = AMP_MENU_EMPTY;

  // fxval menu has not been built
  _fxval_menu_type = NULL;

  // who we belong to
  _gui = gui;

  // figure out where we are in the table
  _fx_set = fx_set;

  // param offset for this fx control
  _param_offset = BlkFxParam::paramOffs(_fx_set);

  // size of our rectangle
  /*CView::*/ size = rect;

  // mousable area is the same size
  /*CView::*/ mouseableArea = size;

  //
  LOG("", "FxCtrl::init" << VAR(this) << VAR(rect) << VAR(_param_offset));

  // split our rectangle
  Array<float, 5> ctrl_ratios = {1.0f, 1.0f, .6f, 1.2f, 1.2f};
  SplitCRect ctrl_split(/*rect*/ MoveToOrigin(this), ctrl_ratios);

  //
  for (i = 0; i < 2; i++) {
    _freq[i].New();
    _freq[i]->setSize(ctrl_split.next());
    _freq[i]->setTag(_param_offset + BlkFxParam::FX_FREQ_A + i);
    setMouseIntercept(_freq[i], this);
  }

  // place amp ctrl
  _amp.New();
  _amp->setSize(ctrl_split.next());
  _amp->setTag(_param_offset + BlkFxParam::FX_AMP);
  setMouseIntercept(_amp, this);

  // fx type ctrl
  _fxtype_ctrl.New();
  _fxtype_ctrl->setSize(ctrl_split.next());
  _fxtype_ctrl->setTag(_param_offset + BlkFxParam::FX_TYPE);
  setMouseIntercept(_fxtype_ctrl, this);

  // place fx value
  _fxval.New();
  _fxval->setSize(ctrl_split.next());
  _fxval->setTag(_param_offset + BlkFxParam::FX_VAL);
  setMouseIntercept(_fxval, this);

  // populate effect menu

  // unused entries map to an index of -1
  int max_effects = BlkFxParam::getEffectType(1.0f) + 1;
  _map_effect_to_menu.resize(max_effects, /*default*/ -1);
  _map_menu_to_param.reserve(max_effects);
  for (i = 0; i < g_num_fx_1_0; i++) {
    FxRun1_0* fx = GetFxRun1_0(i);
    if (strcmp(fx->name(), "DoNotUse") == 0)
      continue;

    //
    _map_effect_to_menu[i] = _map_menu_to_param.size();

    //
    _map_menu_to_param.push_back(BlkFxParam::getEffectTypeInv(i));

    //
    _fxtype_ctrl->addEntry(fx->name());
  }

  //
  updateAmpMenu();
  updateFxValMenu();

  //
  _ok = true;
  LOG("", "FxCtrl" << VAR(_ok));
  return true;
}

//-------------------------------------------------------------------------------------------------
CControl* FxCtrl::getCtrlFocus()
// return which of our (if any) controls has focus or NULL for nothing
{
  if (_freq[0]->getState() != _freq[0]->INACTIVE)
    return _freq[0];
  if (_freq[1]->getState() != _freq[1]->INACTIVE)
    return _freq[1];
  if (_amp->getState() != _amp->INACTIVE)
    return _amp;
  if (_fxtype_ctrl->getState() != _fxtype_ctrl->INACTIVE)
    return _fxtype_ctrl;
  if (_fxval->getState() != _fxval->INACTIVE)
    return _fxval;
  return NULL;
}

//-------------------------------------------------------------------------------------------------
void FxCtrl::drawRect(CDrawContext* context, const CRect& update_rect)
// virtual, override CViewContainer
{

  int i;
  LOG("", "drawRect" << VAR(this) << VAR(update_rect));

  // make sure that nothing is dirty because we always do a full redraw
  setDirty(false);
  _freq[0]->setDirty(false);
  _freq[1]->setDirty(false);
  _amp->setDirty(false);
  _fxtype_ctrl->setDirty(false);
  _fxval->setDirty(false);

  // colours
  CColorRGB focus_txt_col(0xff, 0x80, 0x80), normal_txt_col(0xff, 0xff, 0xff);

  // temporary bitmap to draw into
  CContextRGBA* temp_bm = &_gui->_temp_bm;
  temp_bm->setClipRect(MoveToOrigin(this));

  // get the effect type that we are drawing for
  FxRun1_0* fft_fx = blkFx()->_fx1_0[_fx_set].getFxRun();

  // determine effect state
  enum {
    NORMAL_EFFECT, //
    MASK_EFFECT,
    MASK_NO_EFFECT // the next effect is also a mask
  } fx_state = fft_fx->isMask() ? MASK_EFFECT : NORMAL_EFFECT;

  // if we're a mask effect, check the next effect to see whether it is normal or also a mask
  if (fx_state == MASK_EFFECT) {
    if (!idx_within(_fx_set + 1, blkFx()->_fx1_0)) {
      // mask in the last effect position doesn't do anything
      fx_state = MASK_NO_EFFECT;
    }
    else {
      // if the next effect is also a mask then this mask does nothing
      FxRun1_0* next_fx = blkFx()->_fx1_0[_fx_set + 1].getFxRun();
      if (next_fx->isMask())
        fx_state = MASK_NO_EFFECT;
    }
  }

  // background image is made from 5 horizontal tiles
  // 0=inactive bg, 1=row selected bg, 2=control selected bg, 3=amp bg, 4=freq bg
  CRect bg_img_rect = GetTile(Images::g_fx_bg, /*num x*/ 1, /*num y*/ 5, /*idx x*/ 0, /*idx y*/ 0);
  CRect bg_inactive_r = TileOffset(bg_img_rect, /*x*/ 0, /*y*/ 0);
  CRect bg_row_focus_r = TileOffset(bg_img_rect, /*x*/ 0, /*y*/ 1);
  CRect bg_ctrl_focus_r = TileOffset(bg_img_rect, /*x*/ 0, /*y*/ 2);
  CRect amp_r = TileOffset(bg_img_rect, /*x*/ 0, /*y*/ 3);
  CRect freq_scale_r = TileOffset(bg_img_rect, /*x*/ 0, /*y*/ 4);

  // draw the background
  CControl* active_ctrl = getCtrlFocus();
  if (fx_state != MASK_NO_EFFECT && (active_ctrl || _drag_both_state)) {
    // draw in 3 pieces, left of active control, active control and right of active control
    CastCRect dst_r[3] = {bg_img_rect, bg_img_rect, bg_img_rect};

    //
    CastCRect active_ctrl_r;
    if (_drag_both_state)
      active_ctrl_r = EnclosingRect(_freq[0], _freq[1]);
    else
      active_ctrl_r = active_ctrl;

    dst_r[0].right = dst_r[1].left = active_ctrl_r.left;
    dst_r[2].left = dst_r[1].right = active_ctrl_r.right;

    // top-left of bg images
    CPoint src_p[3] = {TopLeft(bg_row_focus_r), TopLeft(bg_ctrl_focus_r), TopLeft(bg_row_focus_r)};

    // blend
    for (i = 0; i < 3; i++)
      Images::g_fx_bg->blend(/*dest*/ temp_bm,
                             dst_r[i],
                             src_p[i] + TopLeft(dst_r[i]),
                             /*alpha*/ 255,
                             /*use src alpha*/ false);
  }
  else {
    // row not selected
    Images::g_fx_bg->blend(/*dest*/ temp_bm,
                           /*dest*/ CPoint(0, 0),
                           /*src rect*/ bg_inactive_r,
                           /*alpha*/ 255,
                           /*use src alpha*/ false);
  }

  // for mask effects, remove the 1 pixel high separating line at the bottom by copying the second
  // last line of the inactive rectangle
  if (fx_state == MASK_EFFECT) {
    // bottom line
    CRect dst_r = bg_inactive_r;
    dst_r.top = dst_r.bottom - 1;
    // 1 before bottom line of inactive bg
    CPoint src_p(0, dst_r.top - 1);
    //
    Images::g_fx_bg->blend(/*dest*/ temp_bm, dst_r, src_p, /*alpha*/ 255, /*use src alpha*/ false);
  }

  // copy on the amp overlay
  float amp_v = _amp->getValue();
  amp_r.right = RndToInt(lin_interp(amp_v, (float)amp_r.left + 5, (float)amp_r.right - 5));
  unsigned char amp_alpha = 255;
  if (!fft_fx->paramUsed(BlkFxParam::FX_AMP))
    amp_alpha = 63;
  else if (_amp->getState() == _amp->INACTIVE)
    amp_alpha = 127;
  Images::g_fx_bg->blend(
      /*dst contect*/ temp_bm, /*dest*/ CPoint(amp_r.left, 0), /*src*/ amp_r, /*alpha*/ amp_alpha);

  //
  // draw the frequency scale
  //

  // freq value (0..1)
  float freq_val[2];

  // freq position (pixels)
  int freq_pos[2];

  //
  for (i = 0; i < 2; i++) {
    freq_val[i] = _freq[i]->getValue();
    freq_pos[i] = RndToInt(
        lin_interp(freq_val[i], (float)freq_scale_r.left + 5, (float)freq_scale_r.right - 5));
  }

  unsigned char freq_alpha =
      fft_fx->paramUsed(BlkFxParam::FX_FREQ_A) || fft_fx->paramUsed(BlkFxParam::FX_FREQ_B) ? 255
                                                                                           : 127;

  // draw freq scale in 1 or 2 parts
  if (freq_val[0] <= freq_val[1]) {
    // draw in between freqs (include range)
    CRect r = freq_scale_r;
    r.left = freq_pos[0];
    r.right = freq_pos[1];
    Images::g_fx_bg->blend(
        /*dst*/ temp_bm, /*dst*/ CPoint(r.left, 0), /*src rect*/ r, /*alpha*/ freq_alpha);
  }
  else {
    // draw outside freqs (exclude range)
    CRect r = freq_scale_r;
    r.right = freq_pos[1];
    Images::g_fx_bg->blend(
        /*dst*/ temp_bm, /*dst*/ CPoint(r.left, 0), /*src rect*/ r, /*alpha*/ freq_alpha);
    r = freq_scale_r;
    r.left = freq_pos[0];
    Images::g_fx_bg->blend(
        /*dst*/ temp_bm, /*dst*/ CPoint(r.left, 0), /*src rect*/ r, /*alpha*/ freq_alpha);
  }

  //
  // write some text on top of everything
  //
  temp_bm->setFont(kNormalFont, /*sz*/ 0, kBoldFace);
  temp_bm->setFillColor(CColorRGB(0, 0, 0));

  CharArray<256> str;

  // freq text
  for (i = 0; i < 2; i++) {
    bool active = _freq[i]->getState() > _freq[i]->INACTIVE || _drag_both_state;
    temp_bm->setFontColor(active ? focus_txt_col : normal_txt_col);

    if (!fft_fx->paramUsed(BlkFxParam::FX_FREQ_A + i))
      OutlineDrawString(temp_bm, _freq[i], "-", kCenterText);

    else {
      float hz = BlkFxParam::getHz(freq_val[i]);
      // HzToNote(str, hz);
      str << sprnum(hz, /*min unit*/ 1.0f) << "Hz";
      OutlineDrawString(temp_bm, _freq[i], str, kCenterText);
    }
  }

  // amp text
  temp_bm->setFontColor(_amp->getState() == _amp->INACTIVE ? normal_txt_col : focus_txt_col);

  bool mix_mode = fft_fx->ampMixMode();
  float amp_mult = BlkFxParam::getEffectAmpMult(amp_v, mix_mode);

  if (!fft_fx->paramUsed(BlkFxParam::FX_AMP))
    str << "-";
  else if (mix_mode && amp_mult <= 1.0f)
    str << sprf("%.1f %%", amp_mult * 100.0f);
  else if (amp_mult > 0)
    str << sprf("%.1f dB", BlkFxParam::getEffectAmp(amp_v));
  else
    str << "-inf dB";

  OutlineDrawString(temp_bm, /*rect*/ _amp, str, kCenterText);

  // effect type text
  temp_bm->setFontColor(_fxtype_ctrl->getState() == _fxtype_ctrl->INACTIVE ? normal_txt_col
                                                                           : focus_txt_col);
  OutlineDrawString(temp_bm, /*rect*/ _fxtype_ctrl, fft_fx->name(), kCenterText);

  // effect value text
  temp_bm->setFontColor(_fxval->getState() != _fxval->INACTIVE ? focus_txt_col : normal_txt_col);
  fft_fx->dispVal(blkFx()->_fx1_0 + _fx_set, str, _fxval->getValue());
  OutlineDrawString(temp_bm, /*rect*/ _fxval, str, kCenterText);

  // copy into main drawing area
  CViewDrawContext dc(this);
  temp_bm->blend(&dc,
                 /*dst rect*/ this,
                 /*src point*/ CPoint(0, 0),
                 /*overall alpha*/ 255,
                 /*src alpha*/ false);
}

//-------------------------------------------------------------------------------------------------
void FxCtrl::draw(CDrawContext* context)
// virtual, override CViewContainer
// draw all
{
  drawRect(context, size);
}

#if 0
//-------------------------------------------------------------------------------------------------
CMouseEventResult FxCtrl::onMouseDown (CPoint &mouse, const long& buttons)		///< called when a mouse down event occurs
// virtual, override CViewContainer
{
	// intercept mouse down to modify drag behaviour of sliders: right button (or ctrl+lbutton) drags both
	//
	CPoint mouse_child = mouse-TopLeft(/*CView::*/size);

	for(int i = 0; i < 2; i++) {
		// control must have focus
		if(_freq[i]->getState() != DtHSlider::FOCUS) continue;

		// must be right button or ctrl + left button
		if(!(buttons&kRButton) && !(buttons&(kLButton|kControl))) continue;

		// mouse must not be over knob
		if(_freq[i]->isInKnob(mouse_child)) continue;

		for(int j = 0; j < 2; j++)
			_freq[j]->setState(DtHSlider::DRAGGING, mouse_child.x);

		//
		mouseDownView = this;

		// note, we now get mouse move events
		return kMouseEventHandled;
	}
	//
	return CViewContainer::onMouseDown(mouse, buttons);
}



//-------------------------------------------------------------------------------------------------
CMouseEventResult FxCtrl::onMouseUp (CPoint &mouse, const long& buttons)		///< called when a mouse up event occurs
{
	if(mouseDownView == this) {
		CPoint mouse_child = mouse-TopLeft(/*CView::*/size);

		// if we get the mouse up then it is because we claimed it for both freq sliders
		for(int i = 0; i < 2; i++)
			_freq[i]->onMouseUp(mouse_child, buttons);

		mouseDownView = 0;
		return kMouseEventHandled;
	}
	//
	return CViewContainer::onMouseUp(mouse, buttons);
}
#endif

//-------------------------------------------------------------------------------------------------
void FxCtrl::doHiliteSgrams()
{
  int i;

  // do nothing if none of our controls have focus
  CControl* active_ctrl = getCtrlFocus();

  //
  if (!active_ctrl && !_drag_both_state)
    return;

  // display frequency range on the sgram if mouse is over the control

  // generate sgram hilite
  Spectrogram::HiliteArray hilite;
  SetAllInRng(hilite, -1);

  // value 0..1 of frq A & frq B
  float val[2];

  // pixel position in sgram for frq A & frq B
  int pix[2];

  // grab values
  for (i = 0; i < 2; i++) {
    val[i] = _freq[i]->getValue();
    pix[i] = _gui->_sgram[0]->fracToPix(val[i]);
  }

  // end pixel
  int end_pix = _gui->_sgram[0]->fracToPix(1.0f) + 1;

  // set info freq
  if (_drag_both_state) {
    // clear info freq
    _gui->_sgram[0]->setInfoFreq(-1);

    //
    active_ctrl = NULL;
  }
  else {
    // display freq info of active control on sgram
    for (i = 0; i < 2; i++)
      if (active_ctrl == _freq[i])
        _gui->_sgram[0]->setInfoFreq(BlkFxParam::getHz(val[i]));
  }

  // work out difference between freq's in pixels
  int diff = pix[1] - pix[0];

  //
  if (diff > 2 && active_ctrl == _freq[0]) {
    hilite[0] = pix[0];
    hilite[1] = pix[0] + 1;
    hilite[2] = pix[0] + 2;
    hilite[3] = pix[1] + 1;
  }
  else if (diff > 2 && active_ctrl == _freq[1]) {
    hilite[0] = pix[0];
    hilite[1] = pix[1] - 1;
    hilite[2] = pix[1];
    hilite[3] = pix[1] + 1;
  }
  else if (diff < -2 && active_ctrl == _freq[0]) {
    hilite[0] = 0;
    hilite[1] = pix[1] + 1;
    hilite[2] = pix[0];
    hilite[3] = pix[0] + 1;
    hilite[4] = pix[0] + 2;
    hilite[5] = end_pix;
  }
  else if (diff < -2 && active_ctrl == _freq[1]) {
    hilite[0] = 0;
    hilite[1] = pix[1] - 1;
    hilite[2] = pix[1];
    hilite[3] = pix[1] + 1;
    hilite[4] = pix[0];
    hilite[5] = end_pix;
  }
  else if (diff >= 0) {
    hilite[0] = pix[0];
    hilite[1] = pix[1];
  }
  else {
    hilite[0] = 0;
    hilite[1] = pix[1] + 1;
    hilite[2] = pix[0];
    hilite[3] = end_pix;
  }

  for (i = 0; i < 2; i++)
    _gui->_sgram[i]->hilite() = hilite;
}

//-------------------------------------------------------------------------------------------------
void FxCtrl::updateFxValMenu()
{
  // find effect run
  FxRun1_0* fft_fx = blkFx()->_fx1_0[_fx_set].getFxRun();

  // do nothing if menu has already been built
  if (fft_fx == _fxval_menu_type)
    return;
  _fxval_menu_type = fft_fx;

  // clear menu
  _fxval->removeAllMenuEntries();

  // copy menu items in a loop
  int i = 0;
  while (1) {
    const char* str = fft_fx->getValueName(i++);
    if (!str)
      return;
    _fxval->addMenuEntry(-1 /*no value*/, str);
  }
}

//-------------------------------------------------------------------------------------------------
void FxCtrl::updateAmpMenu()
{
  FxRun1_0* fft_fx = blkFx()->_fx1_0[_fx_set].getFxRun();

  //
  int amp_menu_mixmode = fft_fx->ampMixMode() ? AMP_MENU_PERCENT : AMP_MENU_DB;

  // only rebuild if need be
  if (amp_menu_mixmode == _amp_menu_mixmode)
    return;
  _amp_menu_mixmode = amp_menu_mixmode;

  //
  _amp->removeAllMenuEntries();

  //
  CharArray<64> str;
  float dB;

  if (amp_menu_mixmode == AMP_MENU_DB) {
    _amp->addMenuEntry(0.0f, "-inf");
    // dB starts at -40
    dB = -40;
  }
  else {
    for (int i = 0; i <= 100; i += 25) {
      str << i << "%";
      _amp->addMenuEntry(BlkFxParam::getAmpParam0dB() * 0.01f * (float)i, str);
    }
    // dB starts at 10
    dB = 10.0f;
  }

  // populate amp menu
  while (dB <= 40.0f) {
    str << (int)dB << " dB";
    _amp->addMenuEntry(BlkFxParam::getAmpParam(dB), str);
    dB += 10.0f;
  }
}

//-------------------------------------------------------------------------------------------------
void FxCtrl::mouseInterceptBefore(Event::Args* args)
{
  if (!_ok)
    return;
  Event::MouseArgs* mouse = (Event::MouseArgs*)args;

  // update menus if need be
  if (mouse->event_type == Event::kMouseDown && mouse->buttons & kRButton) {
    if (mouse->src_view == _amp)
      updateAmpMenu();
    else if (mouse->src_view == _fxval)
      updateFxValMenu();
  }

  // bodgy invisible slider, steal the mouse click if drag state both or
  // right button pressed
  //
  if (mouse->event_type == Event::kMouseDown &&
      ((mouse->buttons & kRButton) || ((mouse->buttons & kLButton) && (mouse->buttons & kShift))) &&
      (mouse->src_view == _freq[0] || mouse->src_view == _freq[1])) {

    //
    _drag_both_state = DRAG_BOTH_ACTIVE;

    // redraw
    setDirty();

    // change cursor
    getFrame()->setCursor(kCursorHSize);

    //
    for (int i = 0; i < 2; i++) {
      // use the sgram width as the slider width
      _drag_both_rescale[i].init((float)getWidth() - 10.0f);

      // set the offset used for each slider
      _drag_both_rescale[i].setOffs(_freq[i]->getValue(), (float)mouse->mouse.x);
    }

    // steal event, don't pass to control
    mouse->result = kMouseEventHandled;
    return;
  }

  // steal mouse events when dragging both sliders
  if (_drag_both_state == DRAG_BOTH_ACTIVE) {

    // mouse moved?
    if (mouse->event_type == Event::kMouseMoved) {
      for (int i = 0; i < 2; i++) {

        // work out value from mouse position
        float /*0..1*/ v = _drag_both_rescale[i].getFrac((float)mouse->mouse.x);

        // set our freq control & loopback to valueChanged()
        _freq[i]->beginEdit();
        _freq[i]->setValue(v, /*send to listener*/ true);
        _freq[i]->endEdit();
      }
    }

    // mouse released
    else if (mouse->event_type == Event::kMouseUp) {
      // change cursor back
      getFrame()->setCursor(kCursorDefault);

      // assume inactive, mouseInterceptAfter() will fix this if it needs to
      _drag_both_state = DRAG_BOTH_INACTIVE;

      // cause a redraw
      setDirty();
    }

    // steal event, don't pass to control
    mouse->result = kMouseEventHandled;
    return;
  }

  // next call is the control event handler followed by mouseInterceptAfter()
}

//-------------------------------------------------------------------------------------------------
void FxCtrl::mouseInterceptAfter(Event::Args* args)
// called after child has processed
{
  if (!_ok)
    return;

  Event::MouseArgs* mouse = (Event::MouseArgs*)args;

  // check whether shift was released while we had focus
  if (_drag_both_state == DRAG_BOTH_FOCUS && !(mouse->buttons & kShift)) {
    _drag_both_state = DRAG_BOTH_INACTIVE;
    setDirty();
  }

  // check whether shift is held over a freq control
  if ((mouse->buttons & kShift) &&
      (mouse->src_view == _freq[0] && _freq[0]->getState() == _freq[0]->FOCUS ||
       mouse->src_view == _freq[1] && _freq[1]->getState() == _freq[1]->FOCUS)) {
    // take focus for both
    _drag_both_state = DRAG_BOTH_FOCUS;

    // cause a redraw
    setDirty();
  }

  // clr hilites if mouse exits any child control
  if (mouse->event_type == Event::kMouseExited) {

    // clear sgram hilite stuff
    _gui->_sgram[0]->setInfoFreq(-1);
    _gui->_sgram[0]->clrHilite();
    _gui->_sgram[1]->clrHilite();

    //
    if (_drag_both_state) {
      _drag_both_state = DRAG_BOTH_INACTIVE;
      setDirty();
    }
    return;
  }

  // otherwise hilite sgrams
  doHiliteSgrams();
}

//-------------------------------------------------------------------------------------------------
void FxCtrl::valueChanged(CControl* ctrl)
// virtual, override CControlListener
//
// called by the gui control when changed by the user
{
  LOG("", "valueChanged" << VAR(_param_offset) << VAR(ctrl));

  // safety
  if (!_ok)
    return;

  // do a full redraw if something changed
  setDirty();

  // get current
  float param_val = ctrl->getValue();

  // for effect value menu changes let the effect map menu index into a param value
  if (ctrl == _fxval) {
    int menu_idx = _fxval->lastMenuIdx();
    if (menu_idx >= 0) {
      FxRun1_0* fft_fx = blkFx()->_fx1_0[_fx_set].getFxRun();
      param_val = fft_fx->getValue(menu_idx, param_val);
    }
  }
  // otherwise check whether it's the fx menu (we need to convert the menu item to param val)
  // note that all other controls already give their value 0..1
  else if (ctrl == _fxtype_ctrl) {
    // ignore if idx out of range
    if (!idx_within((int)param_val, _map_menu_to_param))
      return;

    // convert to effect param
    param_val /*0..1*/ = _map_menu_to_param[(int)param_val];

    // assume previous fx gui pane needs to be redrawn (in case 2 mask effects in a row were
    // selected or deselected)
    if (_fx_set > 0)
      _gui->_fx_ctrl[_fx_set - 1]->setDirty();
  }

  // send param off
  blkFx()->setParameterAutomated(ctrl->getTag(), param_val);
}

//-------------------------------------------------------------------------------------------------
void FxCtrl::setParameter(long index, float v)
//
// called when host application changes a parameter
//
// NOTE, we expect this to be called synchronously with "init" since we're relying on controls not
// being deleted
//
{
  LOG("", "setParameter" << VAR(_param_offset) << VAR(index) << VAR(v));
  if (!_ok)
    return;

  switch (index) {
    case BlkFxParam::FX_FREQ_A:
      _freq[0]->setValue(v);
      doHiliteSgrams();
      break;
    case BlkFxParam::FX_FREQ_B:
      _freq[1]->setValue(v);
      doHiliteSgrams();
      break;
    case BlkFxParam::FX_AMP:
      _amp->setValue(v);
      break;
    case BlkFxParam::FX_TYPE:
      _fxtype_ctrl->setValue(_map_effect_to_menu[BlkFxParam::getEffectType(v)]);
      break;
    case BlkFxParam::FX_VAL:
      _fxval->setValue(v);
      break;
  }
}
