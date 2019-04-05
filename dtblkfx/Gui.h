/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Top level DtBlkFx GUI class
please see Gui.cpp for more information

History
        Date       Version    Programmer         Comments
        16/2/03    1.0        Darrell Tam		Created


This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

***************************************************************************************************/

#ifndef __DT_BLKFX_Gui__
#define __DT_BLKFX_Gui__

#include "VstGuiSupport.h"
#include "misc_stuff.h"

#include <vstgui/aeffguieditor.h>

#include "BlkFxParam.h"
#include "GlobalCtrl.h"
#include "Spectrogram.h"

#include "FxCtrl.h"

// gui elements used by FxCtrl
// amplify overlay graphics

#ifndef GUI_GLOBAL_EXTERN
#  define GUI_GLOBAL_EXTERN extern
#endif

namespace Images {
GUI_GLOBAL_EXTERN VstGuiRef<CContextRGBA>
    // splash background in sgrams
    g_splash_bg,
    // globals controls background
    g_glob_bg,
    // effect background, tiled:
    g_fx_bg;
};

//-----------------------------------------------------------------------------
class Gui
    : public AEffGUIEditor
    , public CControlListener {
public:
  Gui(AudioEffect* effect);
  virtual ~Gui();

  DtBlkFx* blkFx() { return (DtBlkFx*)effect; }

public:                 // methods called by DtBlkFx class
  void FFTDataRdy(int a // 0=input data, 1=output data
  );

  void suspend();
  void resume();
  // bool keysRequired ();

public:                         // override AEffGUIEditor methods
  virtual bool open(void* ptr); ///< Open editor, pointer to parent windows is platform-dependent
                                ///< (HWND on Windows, WindowRef on Mac).
  virtual void close();         ///< Close editor (detach from parent window)
  virtual void setParameter(long index, float value);
  virtual void idle();

  // VST 2.1
  // virtual long onKeyDown (VstKeyCode &keyCode);
  // virtual long onKeyUp (VstKeyCode &keyCode);

public
    : // override virtual methods from CControlListener (so we can get events from the pop-up menus)
  virtual void valueChanged(CControl* ctrl);

public: // internal stuff
  bool openSgram(int i, CPoint* p);

  // gradient colour map to draw spectrum with
  std::valarray<unsigned long> _col_map;

  // pixel to frequency (Hz) mapping
  std::valarray<float> _pix_hz;

  // pixel to bin mappings
  Array<PixelFreqBin, BlkFxParam::AUDIO_CHANNELS> _pix_bin;

  // global controls
  VstGuiRef<GlobalCtrl> _glob_ctrl;

  // spectrograms for input & output (for mono) or various in stereo
  Array<VstGuiRef<Spectrogram>, 2> _sgram;

  // sgram menus
  Array<VstGuiRef<COptionMenuOwnerDraw>, 2> _sgram_menu;

  // fx controls
  Array<VstGuiRef<FxCtrl>, BlkFxParam::NUM_FX_SETS> _fx_ctrl;

  // temporary bitmap for children to use
  CContextRGBA _temp_bm;

  // true if gui is open ok
  bool _ok;
};

#endif
