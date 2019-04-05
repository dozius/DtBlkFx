#ifndef _BLK_FX_FX_CTRL_H_
#define _BLK_FX_FX_CTRL_H_
/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Please see FxCtrl.cpp for more information

History
        Date       Version    Programmer         Comments
        01/12/05    1.0       Darrell Tam		 Created


This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

***************************************************************************************************/

#include "VstGuiSupport.h"
#include "FxRun1_0.h"
#include <memory>

class Gui;
class DtBlkFx;

class FxCtrl
    : public CViewContainer
    , public CControlListener
// frequency control is a CViewContainer
{
public:
  FxCtrl();
  ~FxCtrl();

  // must call before use, gui must have spectrograms init'd
  bool init(Gui* gui, CastCRect rect, int fx_set);

  // set a parameter
  void setParameter(long index, float v);

public:                                     // override virtual methods from CView
  virtual void draw(CDrawContext* context); ///< called if the view should draw itself
  virtual void drawRect(CDrawContext* context,
                        const CRect& update_rect); ///< called if the view should draw itself

  //
public: // override virtual methods from CControlListener
  virtual void valueChanged(CControl* ctrl);

public:
  // mouse intercept
  void mouseInterceptBefore(Event::Args* args);
  void mouseInterceptAfter(Event::Args* args);

public:
  // return which of our (if any) controls has focus or -1 for nothing (matches BlkFxParam param
  // numbers)
  CControl* getCtrlFocus();

  // hilite sgrams
  void doHiliteSgrams();

  //
  void updateAmpMenu();

  //
  void updateFxValMenu();

public: // internal stuff
  DtBlkFx* blkFx();

  // what effect set we are
  int _fx_set;

  // parameter offset for this fx set
  int _param_offset;

  // parent gui
  _Ptr<Gui> _gui;

  // true if we have init'd ok
  bool _ok;

  // effect type
  VstGuiRef<CViewEventIntercept<COptionMenuOwnerDraw, FxCtrl>> _fxtype_ctrl;

  // freq control (2 Sliders)
  VstGuiRef<CViewEventIntercept<DtPopupHSlider, FxCtrl>> _freq[2];

  // effect value control
  VstGuiRef<CViewEventIntercept<DtPopupHSlider, FxCtrl>> _fxval;

  // amplitude control knob
  VstGuiRef<CViewEventIntercept<DtPopupHSlider, FxCtrl>> _amp;

  // whether the amp menu has been built
  enum { AMP_MENU_EMPTY = -1, AMP_MENU_PERCENT, AMP_MENU_DB };
  int _amp_menu_mixmode;

  // effect type that the fx val menu has been built for
  _Ptr<FxRun1_0> _fxval_menu_type;

  // true if both sliders are being dragged
  enum { DRAG_BOTH_INACTIVE = 0, DRAG_BOTH_FOCUS, DRAG_BOTH_ACTIVE } _drag_both_state;

  // only valid while _drag_both_freq is true
  FracRescale _drag_both_rescale[2];

  // map menu index to effect index
  std::vector<float /*0..1*/> _map_menu_to_param;
  std::vector<int /*effect idx*/> _map_effect_to_menu;
};

#endif
