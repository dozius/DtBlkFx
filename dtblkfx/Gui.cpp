/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

Top-level DtBlkFx gui

Holds the following panels:
    1x GlobalCtrl
    2x spectrogram
    4x FxCtrl


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
#include <StdAfx.h>

#define GUI_GLOBAL_EXTERN

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <valarray>

#include "BlkFxParam.h"
#include "DTBlkFx.hpp"

#include "Gui.h"
#include "Spectrogram.h"

#define LOG_FILE_NAME "c:\\temp\\temp\\gui.html"
#include "Debug.h"

using namespace std;
using namespace VSTGUI;

enum { SGRAM_MENU_ID = 1000 };

//-------------------------------------------------------------------------------------------------
Array<const char*, 7> g_sgram_menu_strings = {"Input L+R",
                                              "Input Left",
                                              "Input Right",
                                              "Output L+R",
                                              "Output Left",
                                              "Output Right",
                                              "Paused"};

//-------------------------------------------------------------------------------------------------
// channel map matches menu string
static bool g_stereo_ch_menu_map[/*in-out*/ 2][/*menu*/ 7][/*ch*/ 2] = {
    {
        // in
        {true, true},   // L+R in
        {true, false},  // left in only
        {false, true},  // right in only
        {false, false}, // L+R out
        {false, false}, // left out only
        {false, false}, // right out only
        {false, false}  // paused
    },
    {
        // out
        {false, false}, // L+R in
        {false, false}, // left in only
        {false, false}, // right in only
        {true, true},   // L+R out
        {true, false},  // left out only
        {false, true},  // right out only
        {false, false}  // paused
    }};

#if 0
//-------------------------------------------------------------------------------------------------
void GenerateAmpOverlay(CContextRGBA* bm)
// generate exponential amplitude gfx into _amp_bm
// this code was written (instead of just using a normal bitmap) back when the whole gui was
// going to be scalable
{
	if(!bm->ok()) return;

	int width = bm->getWidth();
	int height = bm->getHeight();
	float fwidth = (float)width;
	float fheight = (float)height;

	// generate an exponential curve
	int zero_db = (int)(0.6f*fwidth);
	float xscale = 1.0f/zero_db;
	float yscale = 0.8f*fheight;
	CPoint p;
	for(int x = 0; x < width; x++) {
		float top = yscale*(1-powf(20.0f, (float)p.x*xscale)/20.0f);

		int i_top = (int)floorf(top);

		Ptr2dRGBA p = bm->get2d(x, /*y*/0);
		for(int y = 0; y < height; y++) {
			// totally transparent
			if(y < i_top)
				p.setTransparent();

			// completely covered
			else if(y != i_top)
				p.set(/*r*/255, /*g*/0, /*b*/0);

			// partially covered
			else p.set(/*r*/255, /*g*/0, /*b*/0, RndToInt(255.0f*(1.0f-top+(float)i_top)));

			p.incY();
		}
	}
}
#endif

//-------------------------------------------------------------------------------------------------
Gui::Gui(AudioEffect* effect)
    : AEffGUIEditor(effect)
{
  LOGG("", "Gui::Gui" << VAR(this));
  _ok = false;

  /*AEffGUIEditor*/ frame = 0;

  /*AEffGUIEditor::rect*/
  rect.left = 0;
  rect.top = 0;
  rect.right = (VstInt16)Images::g_glob_bg->getWidth();
  rect.bottom =
      (VstInt16)(Images::g_glob_bg->getHeight() + Images::g_splash_bg->getHeight() + // spectrograms
                 Images::g_fx_bg->getHeight() / 5 * BlkFxParam::NUM_FX_SETS);

  // build the colour gradient map for the spectrograms, each 5 element array
  // contains: position (0..1), R, G, B, A
  Array<float, 5> col_vec[] = {/*   pos     R     G     B     A*/
                               {0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
                               {0.2f, 0.0f, 0.0f, 1.0f, 1.0f},
                               {0.4f, 0.0f, 1.0f, 1.0f, 1.0f},
                               {0.6f, 0.0f, 0.8f, 0.0f, 1.0f},
                               {0.8f, 1.0f, 1.0f, 0.0f, 1.0f},
                               {1.0f, 1.0f, 0.0f, 0.0f, 1.0f}};
  _col_map.resize(1000);
  GenerateGradientRGBA(_col_map, TO_RNG(col_vec));
}

//-------------------------------------------------------------------------------------------------
Gui::~Gui()
{
  LOGG("", "Gui::~Gui" << VAR(this));
}

//-------------------------------------------------------------------------------------------------
bool /*success*/ Gui::openSgram(int i, CPoint* p)
{
  LOGG("", "Gui::openSgram" << VAR(this) << VAR(i) << VAR(*p));

  // determine size of sgram based on splash background size
  CRect splash_bg_rect =
      GetTile(Images::g_splash_bg, /*num x*/ 1, /*num y*/ 2, /*idx x*/ 0, /*idx y*/ i);

  //
  NewView(_sgram[i], frame);

  //
  _sgram[i]->init(MoveTo(*p, splash_bg_rect), // size of window
                  _pix_hz,  // pixel to freq & width of client (Spectrogram does NOT take a copy)
                  _col_map, // colour map
                  BlkFxParam::octaveFrac() // fraction of total span per octave
  );

  //
  p->y += splash_bg_rect.getHeight();

  //
  if (!_sgram[i]->ok()) {
    LOG("background:#f00", "Gui::openSgram failed");
    return false;
  }

  // add a menu in stereo mode
  if (BlkFxParam::AUDIO_CHANNELS == 2) {
    NewView(_sgram_menu[i], _sgram[i]);

    _sgram_menu[i]->setSize(MoveToOrigin(_sgram[i]));
    _sgram_menu[i]->setTag(SGRAM_MENU_ID + i);
    _sgram_menu[i]->setListener(this);
    _sgram_menu[i]->setStyle(kCheckStyle);

    for (int j = 0; j < g_sgram_menu_strings.size(); j++)
      _sgram_menu[i]->addEntry(g_sgram_menu_strings[j], j);

    _sgram_menu[i]->setValue(i ? 3 : 0);
    _sgram[i]->setName(g_sgram_menu_strings[i ? 3 : 0]);
  }
  else {
    _sgram[i]->setName(i ? "Output" : "Input");
  }

  // draw into the sgram bitmap
  CContextRGBA* sgram_dc = _sgram[i]->getImage();

  // copy background image
  Images::g_splash_bg->blend(
      sgram_dc,                                                  // dest draw context
      TopLeft(_sgram[i]) - TopLeft(_sgram[i]->getDisplayRect()), // destination point
      splash_bg_rect,                                            // src rect
      255,                                                       // overall alpha
      false                                                      // use source alpha?
  );

  // write some text on the second sgram
  if (i != 1)
    return true;

  // draw some stuff into the second sgram
  sgram_dc->setFontColor(CColorRGB(255, 255, 255));
  sgram_dc->setFillColor(CColorRGB(0, 0, 0));
  sgram_dc->setFont(kNormalFont, /*sz*/ 0, kBoldFace);
  CRect txt_r(0, 10, sgram_dc->getWidth(), 10 + 15);

  const char* tip[] = {"~", "Hi there BlkFx user", "~"};
  for (int l = 0; l < 3; l++) {
    OutlineDrawString(sgram_dc, txt_r, tip[l]);
    txt_r.offset(0, 15);
  }

  CharArray<128> str;
  Rng<char> s = str;

#ifdef STEREO
  s = s << "Stereo";
#else
  s = s << "Mono";
#endif

  s = s << " (compiled " __DATE__ ")";
#ifdef _DEBUG
  s = s << " DEBUG";
#endif

  txt_r.offset(0, 5);
  OutlineDrawString(sgram_dc, txt_r, str);
  return true;
}

//-------------------------------------------------------------------------------------------------
bool Gui::open(void* system_window)
// virtual, override AEffGUIEditor::open
// open the GUI window
//
{
  int i;

  LOGG("", "Gui::open" << VAR(this) << VAR(system_window));

  CRect frame_rect(0, 0, rect.right, rect.bottom);

  _ok = false;

  AEffGUIEditor::open(system_window);

  // build pixel to freq mapping
  _pix_hz.resize(frame_rect.getWidth() - 10);
  BlkFxParam::genPixelToHz(_pix_hz);

  // build pixel to bin range mapping
  for (i = 0; i < BlkFxParam::AUDIO_CHANNELS; i++)
    _pix_bin[i].init(_pix_hz, //
                     blkFx()->getSampleRate(),
                     blkFx()->FFTdata(i) // FFT data buffer
    );

  // temporary bitmap for children to use as an offscreen buffer
  _temp_bm.create(rect.right, max(Images::g_glob_bg->getHeight(), Images::g_fx_bg->getHeight()));
  if (!_temp_bm.ok())
    return false;

  // create main window
  frame = new CFrame(frame_rect, system_window, /*gui editor*/ this);

  // make sure frame doesn't do any erasing because it flickers
  frame->setTransparency(true);

  // position of each control
  CPoint p(0, 0);

  // place global controls
  NewView(_glob_ctrl, frame);
  _glob_ctrl->init(/*gui*/ this, /*size*/ Offset(p, Images::g_glob_bg));
  if (!_glob_ctrl->ok())
    return false;
  p.y += Images::g_glob_bg->getHeight();

  // place spectrograms
  for (i = 0; i < 2; i++)
    if (!openSgram(i, &p))
      return false;

  _sgram[0]->linkSgram(_sgram[1]);
  _sgram[1]->linkSgram(_sgram[0]);

  // create fx ctrls
  CRect fx_rect = GetTile(Images::g_fx_bg, /*num x*/ 1, /*num y*/ 5);
  for (i = 0; i < BlkFxParam::NUM_FX_SETS; i++) {
    NewView(_fx_ctrl[i], frame);
    _fx_ctrl[i]->init(/*gui*/ this, /*size*/ Offset(p, fx_rect), i);
    p.y += fx_rect.height();
  }

  _ok = true;

  // load params from the effect
  for (i = 0; i < BlkFxParam::TOTAL_NUM; i++)
    setParameter(i, blkFx()->getCurrParam(i));

  return true;
}

//-------------------------------------------------------------------------------------------------
void Gui::FFTDataRdy(int a // 0=input data, 1=output data
)
// preconditions:
//   DtBlkFx::_protection is locked
{
  // note that there is a potential race if the gui is open, closed & re-opened during this
  // method but I think that it being closed & reopened so quickly as to cause a problem is
  // unlikely

  if (!_ok)
    return;

  int plan = blkFx()->_plan;
  long samp_pos = blkFx()->_blk_samp_abs;
  long time_fft_n = blkFx()->_time_fft_n;
  if (BlkFxParam::AUDIO_CHANNELS == 1) {
    // mono, input data always goes into upper sgram & output into lower sgram
    if (!_sgram[a]->rdy())
      return;

    _sgram[a]->newData(_pix_bin[/*channel*/ 0].getMap(plan), /*scale*/ 1.0f);
    _sgram[a]->blkDone(samp_pos, time_fft_n);
    return;
  }

  // stereo, check which channels to display
  for (int sgram_i = 0; sgram_i < 2; sgram_i++) {
    if (!_sgram[sgram_i]->rdy())
      continue;

    // convert menu number 0..6 to 0..2
    // input data corresponds to menu values 0..2, output data to 3..5
    int menu_val = (int)(_sgram_menu[sgram_i]->getValue());

    // for safety
    if (!withinRng(menu_val, g_sgram_menu_strings))
      continue;

    // update spectrogram with audio data
    bool update = false;
    for (int ch = 0; ch < 2; ch++) {
      if (g_stereo_ch_menu_map[a][menu_val][ch]) {
        float pwr_scale = 1.0f;
        if (a == 1)
          pwr_scale = blkFx()->_chan[ch].out_pwr_scale;
        _sgram[sgram_i]->newData(_pix_bin[ch].getMap(plan), pwr_scale);
        update = true;
      }
    }

    if (update)
      _sgram[sgram_i]->blkDone(samp_pos, time_fft_n);
  }
}

#if 0
//-------------------------------------------------------------------------------------------------
bool Gui::keysRequired ()
{
	// don't expect keys for now
	//return false;

	if (frame && frame->getEditView()) return true;
	else return false;
}

//-------------------------------------------------------------------------------------------------
long Gui::onKeyDown (VstKeyCode &keyCode)
{
	if (frame && 0 && (keyCode.character >= 'a' && keyCode.character <= 'z'))
	{
		char val[64];
		char modifiers[32];
		strcpy (modifiers, "");
		if (keyCode.modifier & MODIFIER_SHIFT)
			strcpy (modifiers, "Shift+");
		if (keyCode.modifier & MODIFIER_ALTERNATE)
			strcat (modifiers, "Alt+");
		if (keyCode.modifier & MODIFIER_COMMAND)
			strcat (modifiers, "Cmd+");
		if (keyCode.modifier & MODIFIER_CONTROL)
			strcat (modifiers, "Ctrl+");

		sprintf (val, "onKeyDown : '%s%c'", modifiers, (char)(keyCode.character));
		return 1;
	}

	if (frame && (keyCode.virt == VKEY_UP || keyCode.virt == VKEY_DOWN))
	{
		CView *pView = frame->getCurrentView ();
	}
	return -1;
}

//-------------------------------------------------------------------------------------------------
long Gui::onKeyUp (VstKeyCode &keyCode)
{
	if (0 && (keyCode.character >= 'a' && keyCode.character <= 'z'))
	{
		char val[64];
		char modifiers[32];
		strcpy (modifiers, "");
		if (keyCode.modifier & MODIFIER_SHIFT)
			strcpy (modifiers, "Shift+");
		if (keyCode.modifier & MODIFIER_ALTERNATE)
			strcat (modifiers, "Alt+");
		if (keyCode.modifier & MODIFIER_COMMAND)
			strcat (modifiers, "Cmd+");
		if (keyCode.modifier & MODIFIER_CONTROL)
			strcat (modifiers, "Ctrl+");

		sprintf (val, "onKeyUp : '%s%c'", modifiers, (char)(keyCode.character));
		return 1;
	}

	return -1; 
}

#endif

//-------------------------------------------------------------------------------------------------
void Gui::resume()
// preconditions:
//   DtBlkFx::_protection is locked
{
  LOG("", "Gui::resume");
  // called when the plugin will be On
}

//-------------------------------------------------------------------------------------------------
void Gui::suspend()
// preconditions:
//   DtBlkFx::_protection is locked
{
  LOG("", "Gui::suspend");
  if (!_ok)
    return;

  // called when the plugin will be Off
  for (int i = 0; i < 2; i++)
    _sgram[i]->suspend();
}

//-------------------------------------------------------------------------------------------------
void Gui::close()
// virtual, override AEffGUIEditor::close()
// the GUI window is closing
{
  LOGG("", "Gui::close" << VAR(this) << VAR(frame));

  _ok = false;

  // assume blkfx hasn't been deleted yet, make sure that we are sync'd with it before going on
  // since it might be currently updating the sgrams (which are about to be deleted)
  {
    ScopeCriticalSection scs(blkFx()->_protect);
  }

  // release these things
  _sgram[0] = NULL;
  _sgram[1] = NULL;
  _glob_ctrl = NULL;
  for (int i = 0; i < BlkFxParam::NUM_FX_SETS; i++)
    _fx_ctrl[i] = NULL;

  //
  delete /*AEffGUIEditor::*/ frame;
  frame = 0;

  //
  AEffGUIEditor::close();
}

//-------------------------------------------------------------------------------------------------
void Gui::idle()
// virtual, override AEffGUIEditor::idle()
// this is called regularly by the host app
{
  // LOG("", "Gui::idle");

  // call down (default will trigger repaint of invalid windows)
  AEffGUIEditor::idle();

  if (!_ok)
    return;

  // if (/*AEffGUIEditor::*/inIdleStuff) return;

  // inject these events so that we can catch the mouse leaving the window
  GenerateMouseMoved(frame);

  // update the spectrograms
  CViewDrawContext dc(frame);
  for (int i = 0; i < 2; i++) {
    OffsetDrawContext(&dc, _sgram[i]);
    _sgram[i]->update(&dc);
  }
}

//-------------------------------------------------------------------------------------------------
void Gui::setParameter(long index, float v)
// virtual, override AEffGUIEditor::setParameter()
//
// called by DtBlkFx to update the control's value
{
  // test if the plug is opened
  if (!_ok)
    return;

  BlkFxParam::SplitParamNum p(index);

  if (p.glob_param >= 0)
    _glob_ctrl->setParameter(p.glob_param, v);

  else if (p.fx_param >= 0 && idx_within(p.fx_set, _fx_ctrl))
    _fx_ctrl[p.fx_set]->setParameter(p.fx_param, v);
}

//-------------------------------------------------------------------------------------------------
void Gui::valueChanged(CControl* ctrl)
// virtual, override CControlListener
//
// handle sgram menus
{
  if (!_ok)
    return;

  // handle the results of spectrogram menus
  //
  int i = ctrl->getTag() - SGRAM_MENU_ID;
  if (i < 0 || i >= 2)
    return;

  // update the spectrogram name with the menu selection
  int sel = (int)_sgram_menu[i]->getValue();

  // update sgram
  if (withinRng(sel, g_sgram_menu_strings))
    _sgram[i]->setName(g_sgram_menu_strings[sel]);
}
