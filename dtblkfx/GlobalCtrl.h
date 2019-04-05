#ifndef _BLK_FX_GLOB_CTRL_CTRL_H_
#define _BLK_FX_GLOB_CTRL_CTRL_H_
/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Pls see GlobalCtrl.cpp for more information

History
        Date       Version    Programmer         Comments
        01/12/05    1.0        Darrell Tam		Created


This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

***************************************************************************************************/

#include "BlkFxParam.h"
#include "VstGuiSupport.h"
#include <vector>

class Gui;
class DtBlkFx;

class GlobalCtrl
    : public CViewContainer
    , public CControlListener
// global controls
{
public:
  GlobalCtrl();
  ~GlobalCtrl();

  // must call before use
  bool init(Gui* gui, CastCRect rect);

  // set a parameter
  void setParameter(long index, float v);

  //
  bool ok() const { return _ok; }

public:                                     // override virtual methods from CViewContainer
  virtual void draw(CDrawContext* context); ///< called if the view should draw itself
  virtual void drawRect(CDrawContext* context,
                        const CRect& update_rect); ///< called if the view should draw itself

  virtual bool removed(CView* parent); ///< view is removed from parent view

  virtual CMouseEventResult
  onMouseDown(CPoint& mouse, const long& buttons); ///< called when a mouse down event occurs

public
    : // override virtual methods from CControlListener (so we can get events from the pop-up menus)
  virtual void valueChanged(CControl* pControl);

public: // internal stuff
  DtBlkFx* blkFx();

  // all controls have their text rectangle aligned to the bottom of their rectangle
  CRect textRect(CastCRect r)
  {
    r.top = std::max(r.bottom - _text_height, r.top);
    return r;
  }

  //
  void updateMenus();

  // get blk size plan from _blksz control
  int getBlkSzPlan()
  {
    return limit_range((int)(_blksz->getValue() * NUM_FFT_SZ), 0, NUM_FFT_SZ - 1);
  }

public:
  //
  int _text_height;

  // true if all sub windows init'd ok
  bool _ok;

  // parent gui
  _Ptr<Gui> _gui;

  // GUI components
  VstGuiRef<DtPopupHSlider> _mixback;

  //
  VstGuiRef<DtOwnerToggle> _pwrmatch;

  // delay has both an invisi slider and a menu
  VstGuiRef<DtPopupHSlider> _delay;

  // units of the delay (beats or msec)
  BlkFxParam::Delay::Units _delay_units;

  // delay units for each menu entry
  std::vector<BlkFxParam::Delay::Units> _delay_menu_units;

  //
  VstGuiRef<DtPopupHSlider> _overlap;

  //
  VstGuiRef<DtOwnerToggle> _blksync;

  //
  VstGuiRef<DtPopupHSlider> _blksz;

  // current sample rate (used to generate delay values & menu)
  float _sample_rate;

  // current samples per beat (used to generate delay values & menu)
  float _samps_per_beat;
};

#endif
