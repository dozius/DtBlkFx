/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com

History
        Date       Version    Programmer         Comments
        16/2/03    1.0        Darrell Tam		Created

This program is free software; you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.
TODO:
  this is getting big and fat
***************************************************************************************************/

#include <StdAfx.h>
#if 0
#  include "MainGuiPanel.h"
#  include "wx/spinctrl.h"
#  include <wx/display.h>
#  include <wx/listctrl.h>
#  include <wx/popupwin.h>
//#include "MorphParamEdit.h"
#endif

#include "DtBlkFx.hpp"
#include "FxState1_0.h"

#include "Debug.h"

#ifdef _DEBUG
long g_dbg_inst = 0;
#  ifdef _WIN32
#    define LOG_FILE_PATH "c:\\"
#  else
#    define LOG_FILE_PATH "~/"
#  endif
#endif

//-------------------------------------------------------------------------------------------------
FxRun1_0* FxState1_0::getFxRun(bool for_display)
{
  // don't interpolate effect type when for_display===false
  //
  float fx_type_param = _b->get(for_display ? &GetInput : &GetPrev, _param[BlkFxParam::FX_TYPE]);

  //
  int fx_type = BlkFxParam::getEffectType(fx_type_param);

  //
  return GetFxRun1_0(fx_type);
}

//-------------------------------------------------------------------------------------------------
void FxState1_0::init(DtBlkFx* b, int fx_set)
{
  //_disp_freq_mode = DISP_FREQ_HZ;

  _fx_set = fx_set;
  _b = b;

#ifdef _DEBUG
  CharArray<64> name;
  name << LOG_FILE_PATH "fxstate_" << InterlockedIncrement(&g_dbg_inst) << ".html";
  _html_log.open(name);
#endif

  LOG(0x088000, "FxState1_0::init" << VAR(b) << VAR(fx_set));

  // set all params to behave like 1.0 params
  int param_offs = BlkFxParam::paramOffs(_fx_set);
  for (int i = 0; i < BlkFxParam::NUM_FX_PARAMS; i++) {
    _param[i].setModeLin(param_offs + i);
    Array<float, 6> anchor_data = {0, .2f, .4f, .6f, .8f, 1};
    _param[i].setAll(anchor_data);
  }
}

//-------------------------------------------------------------------------------------------------
FxState1_0* FxState1_0::prevFxState()
// find the previous fx state based on our fx set number
{
  // no previous?
  if (_fx_set <= 0)
    return NULL;

  // get the prev fx state from blkfx
  return _b->_fx1_0 + _fx_set - 1;
}

//-------------------------------------------------------------------------------------------------
void FxState1_0::prepare()
// prepare to process
// fill temp variables according to params from "b"
{
  using namespace BlkFxParam;

  temp.fft_fx = getFxRun(false);

  // copy param values to temporaries
  //
  temp.amp_param = _b->get(&GetInterp, _param[FX_AMP]);
  temp.val = _b->get(&GetInterp, _param[FX_VAL]);

  temp.amp = BlkFxParam::getEffectAmpMult(temp.amp_param, temp.fft_fx->ampMixMode());

  // freq stuff
  for (int i = 0; i < 2; i++) {
    temp.freq_param[i] = _b->get(&GetInterp, _param[i]);
    temp.fbin[i] = _b->getFFTBin(temp.freq_param[i]);
    temp.bin[i] = RndToInt(temp.fbin[i]);
  }
}

//-------------------------------------------------------------------------------------------------
bool /*true=printed*/ FxState1_0::getParamDisplay(BlkFxParam::SplitParamNum& p, float v,
                                                  Rng<char> str)
{
  using namespace BlkFxParam;

  // check whether our param is attached to the vst param
  if (_param[p.fx_param].vstParamIdx() != p.param)
    return false;

  // get value after morph
  v = _param[p.fx_param](v);

  // get the effect runtime interface
  FxRun1_0* fx_run = getFxRun();

  // is the param used?
  if (!fx_run->paramUsed(p.fx_param)) {
    str << "-";
    return true;
  }

  switch (p.fx_param) {
    case FX_FREQ_A:
    case FX_FREQ_B: {
      // put in note & freq
      float freq_hz = _b->guessRoundHz(BlkFxParam::getHz(v));
      str << sprnum(freq_hz, /*min unit*/ 1.0f) << "Hz";
      return true;
    }

    case FX_AMP: {
      bool mix_mode = fx_run->ampMixMode();
      float amp_mult = getEffectAmpMult(v, mix_mode);
      if (mix_mode && amp_mult <= 1.0f)
        str << RndToInt(amp_mult * 100.0f) << "%";
      else if (amp_mult > 0)
        str << sprf("%.1f dB", getEffectAmp(v));
      else
        str << "-inf dB";
      return true;
    }

    case FX_TYPE: {
      str << fx_run->name();
      return true;
    }

    case FX_VAL: {
      fx_run->dispVal(this, /*out*/ str, v);
      return true;
    }
  }
  // text not printed
  return false;
}

#if 0
//*************************************************************************************************
// GUI stuff
//*************************************************************************************************

//-------------------------------------------------------------------------------------------------
bool FxState1_0::openGui(MainGuiPanel *parent, wxSize sz)
{
	LOG(0x0ff000, "FxState1_0::openGui" << VAR(sz));

	_top_panel_focus = false;


	// generate positions of controls
	using namespace BlkFxParam;

	if(!_frame_wdw.New()->Create(parent->_fx_scroller, wxID_ANY, wxDefaultPosition, sz, wxNO_BORDER))
		return false;

	if(!_top_panel.New()->Create(_frame_wdw, wxID_ANY, wxEmptyString, wxPoint(0, 0), wxSize(sz.x, 15), wxNO_BORDER))
		return false;
	ConnectMouseAndPaintEvents(_top_panel, this, &FxState1_0::onMouseTopPanel, &FxState1_0::onPaintTopPanel);

	Array<float, 3> widths = { 1, 1, 2 };
	SplitWxRect r(wxRect(0, 15, sz.x, sz.y-15), widths);

	if(!_amp_param_edit.New()->Create(_b, _frame_wdw, r.next(), _param + FX_AMP))
		return false;
	_amp_param_edit->topWdwPaint(this, &FxState1_0::onPaintAmpEdit);

	if(!_fxval_param_edit.New()->Create(_b, _frame_wdw, r.next(), _param + FX_VAL))
		return false;
	_fxval_param_edit->topWdwPaint(this, &FxState1_0::onPaintFxValEdit);

	if(!_freq_param_edit.New()->Create(_b, _frame_wdw, r.next(), _param + FX_FREQ_A, _param + FX_FREQ_B))
		return false;
	_freq_param_edit->topWdwPaint(this, &FxState1_0::onPaintFreqEdit);

	// add us to our parent box sizer
	parent->_fx_scroller->GetSizer()->Add(_frame_wdw);

	return true;
}

//-------------------------------------------------------------------------------------------------
void FxState1_0::closeGui()
{
	// TODO: make sure this is synchronous with wx event system
	//_frame_wdw = NULL;
	//_top_panel = NULL;
	//_morph_edit = NULL;
}

/*
//-------------------------------------------------------------------------------------------------
void FxState1_0::setTextColour(wxDC& dc, int ctrl_idx)
// set text colour based on whether control is active or not
{
	dc.SetTextForeground(wxColour(0, 0, 0));

	wxColour col;
	if(_ctrl_focus == ctrl_idx) col = wxColour(255, 0, 0);
	else col = wxColour(255,255,255);
	dc.SetTextBackground(col);
}
*/

//-------------------------------------------------------------------------------------------------
// return param value at input to blkfx param fifo 
inline float FxState1_0::getParamInput(int idx) { return _b->get(&GetInput, _param[idx]); }

//-------------------------------------------------------------------------------------------------
void FxState1_0::onPaintTopPanel(wxPaintEvent&)
{
	wxPaintDC dc(_top_panel);
	dc.SetBrush(wxBrush(*wxBLACK_BRUSH));
	dc.SetPen(wxPen(*wxTRANSPARENT_PEN));
	dc.DrawRectangle(wxPoint(), _top_panel->GetSize());

	Array<float, 3> widths = { 1, 1, 2 };
	SplitWxRect split(_top_panel->GetSize(), widths);

	int fx_type = BlkFxParam::getEffectType(getParamInput(BlkFxParam::FX_TYPE));
	FxRun1_0* fft_fx = GetFxRun1_0(fx_type);

	dc.SetTextBackground(_top_panel_focus ? Colour::text_focus_bg : Colour::text_default_bg);
	dc.SetTextForeground(_top_panel_focus ? Colour::text_focus_fg : Colour::text_default_fg);

	OutlineDrawText(dc, split.next(), fft_fx->name());
}


//-------------------------------------------------------------------------------------------------
class PopupListCtrl : public wxListCtrl
{
public:
	PopupListCtrl()
	{
		_curr_sel = -1;
		_orig_sel = -1;
		_got_button_down = false;
		Array<wxEventType, 3> mouse_down = { wxEVT_LEFT_DOWN, wxEVT_MIDDLE_DOWN, wxEVT_RIGHT_DOWN };
		ConnectEvents(this, mouse_down, wxMouseEventHandler(PopupListCtrl::OnButtonDown));

		Array<wxEventType, 3> mouse_up = { wxEVT_LEFT_UP, wxEVT_MIDDLE_UP, wxEVT_RIGHT_UP };
		ConnectEvents(this, mouse_up, wxMouseEventHandler(PopupListCtrl::OnButtonUp));

		Connect(wxEVT_MOTION, wxMouseEventHandler(PopupListCtrl::OnMouse));
		Connect(wxEVT_LEAVE_WINDOW, wxMouseEventHandler(PopupListCtrl::OnLeave));
	}

	~PopupListCtrl()
	{
	}

	// set the selection to "sel" and move the toplevel parent window so that it is centred on "pos"
	void CentreWdwOnSelection(int orig_sel, wxPoint/*screen coords*/pos)
	{
		// set the selection
		_curr_sel = orig_sel;
		_orig_sel = orig_sel;
		SetItemBackgroundColour(orig_sel, Colour::popup_orig_bg);
		EnsureVisible(orig_sel);

		// get current selection position
		wxPoint item_pos;
		GetItemPosition(orig_sel, /*out*/item_pos);
		item_pos = ClientToScreen(item_pos);

		// find top level window
		wxWindow* top_level = wxGetTopLevelParent(this);
		if(!top_level) return;

		// find item relative to top level window
		item_pos = top_level->ScreenToClient(item_pos);

		// find top left
		pos.x -= item_pos.x + GetSize().x/2;
		pos.y -= item_pos.y + 4;

		// make sure popup is on screen
		wxWindow* disp_w = top_level->GetParent();
		if(!disp_w) disp_w = top_level;
		int disp_n = wxDisplay::GetFromWindow(disp_w);

		if(disp_n != wxNOT_FOUND) {
			wxRect geom = wxDisplay(disp_n).GetGeometry();
			dtWxRect wdw_r(pos, /*sz*/top_level);
			EnsureAInsideB(/*a*/wdw_r, /*b*/wxDisplay(disp_n));
			pos = wdw_r.GetTopLeft();
		}

		top_level->Move(pos);
	}

	void unhiliteCurrSel()
	{
		if(_curr_sel < 0) return;
		SetItemBackgroundColour(
			_curr_sel,
			_curr_sel == _orig_sel ? Colour::popup_orig_bg : Colour::popup_default_bg
		);
	}

	void OnMouse( wxMouseEvent &ev)
	{
		// determine which item the mouse is over & change it's colour
		long sel = -1; 
		int flags = 0;
		if(!ev.LeftIsDown() || ev.LeftIsDown() && _got_button_down)
			sel = HitTest(ev.GetPosition(), /*out*/flags);

		if(sel != _curr_sel) {
			unhiliteCurrSel();

			_curr_sel = sel;

			if(sel >= 0) {
				SetItemBackgroundColour(sel, Colour::popup_focus_bg);
				EnsureVisible(sel);
			}
		}

		// pass event on
		ev.Skip();
	}

    void OnButtonUp( wxMouseEvent &ev )
	{
		// ignore mouse release if nothing has been selected or we didn't get the press
		if(_curr_sel < 0 || !_got_button_down) {
			ev.Skip();
			return;
		}

		_got_button_down = false;

		// close owning toplevel window if something is selected
		wxFrame* popup = static_cast<wxFrame*>(wxGetTopLevelParent(this));
		if(!popup) return;
		//popup->Dismiss();
		popup->Destroy();
	}

    void OnButtonDown( wxMouseEvent &ev )
	{
		_got_button_down = true;
		OnMouse(ev);
		if(_curr_sel >= 0) ev.Skip(false);
	}

    void OnLeave( wxMouseEvent &ev )
	{
		unhiliteCurrSel();
		_curr_sel = -1;
		ev.Skip();
	}

	// get currently selected item (or -1 if none)
	int getCurrSel() const { return _curr_sel; }

	// get selection from an event (assuming event was for this popup)
	static int getCurrSel(wxEvent& ev)
	{
		PopupListCtrl* ctrl = static_cast<PopupListCtrl*>(ev.GetEventObject());
		return ctrl->_curr_sel;
	}

	// currently selected item (or -1 if none)
	int _curr_sel;

	// originally selected item
	int _orig_sel;

	//
	bool _got_button_down;

};


//-------------------------------------------------------------------------------------------------
void FxState1_0::onLeftUpTopPanelPopup(wxMouseEvent& ev)
{
	// always pass event
	ev.Skip(); 

	// get selected object from event
	int curr_sel = PopupListCtrl::getCurrSel(ev);
	if(curr_sel < 0) return;
	float val = BlkFxParam::getEffectTypeInv(curr_sel);

	// simplified param setting - assume fx type doesn't have a curve
	VstParamIdx vst_idx(_param[BlkFxParam::FX_TYPE]);
	if(vst_idx < 0 || blkfx()->isMorphMode()) _param[BlkFxParam::FX_TYPE].set(val);
	else blkfx()->setParameterAutomated(vst_idx, val);

	_top_panel->Refresh(/*erase bg*/false);
}


//-------------------------------------------------------------------------------------------------
void FxState1_0::onMouseTopPanel(wxMouseEvent& ev)
{
	ChkChg redraw;
	redraw(_top_panel_focus) = IsInsideWdw(ev);

	if(ev.LeftDown()) {
		wxSize sz(120, 400);
		int fx_type = BlkFxParam::getEffectType(getParamInput(BlkFxParam::FX_TYPE));

		wxPopupTransientWindow* popup = new wxPopupTransientWindow(_top_panel);

	    wxPanel* panel = new wxPanel(popup, wxID_ANY, wxPoint(0,0), sz);
		wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
	    panel->SetSizer(sizer);

		PopupListCtrl* list_ctrl = new PopupListCtrl;
		list_ctrl->Create(panel, wxID_ANY, wxPoint(0, 0), sz, wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL);
		sizer->Add(list_ctrl, 0, wxALL, 2);

		panel->SetAutoLayout( true );
		panel->SetSizer( sizer );
		sizer->Fit(panel);
		sizer->Fit(popup);

		// add the strings to the list box
		list_ctrl->InsertColumn(0, wxString());
		for(int i = 0; i < g_num_fx_1_0; i++)
			list_ctrl->InsertItem(i, wxString(GetFxRun1_0(i)->name()));

		list_ctrl->CentreWdwOnSelection(
			fx_type,										// current selection
			_top_panel->ClientToScreen(ev.GetPosition())	// position to centre of selection over
		);

		list_ctrl->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(FxState1_0::onLeftUpTopPanelPopup), /*data*/NULL, this);

		popup->Popup(panel);
	}
	if(redraw) _top_panel->Refresh(/*erase*/false);
}

//-------------------------------------------------------------------------------------------------
void FxState1_0::onPaintFreqEdit(wxPaintEvent& ev)
{
	wxWindow* wdw = ToWxWindow(ev);

	// which freq param is this window?
	int morph_idx = (wdw == _freq_param_edit->topWdw(0) ? 0 : 1);

	wxPaintDC dc(wdw);
	dc.SetBrush(wxBrush(*wxBLACK_BRUSH));
	dc.SetPen(wxPen(*wxBLACK_PEN));
	dc.DrawRectangle(wxPoint(), wdw->GetSize());

	// effect value text
	int fx_type = BlkFxParam::getEffectType(getParamInput(BlkFxParam::FX_TYPE));
	FxRun1_0* fft_fx = GetFxRun1_0(fx_type);

	_freq_param_edit->textMorphPrepare(dc, morph_idx);

	CharArray<64> str;

	if(!fft_fx->paramUsed(morph_idx)) {
		rngPrint(str, "-");
		OutlineDrawText(dc, wdw->GetSize(), str);
		return;
	}

	float val = _freq_param_edit->topSliderValue(morph_idx);

	float hz = BlkFxParam::getHz(val);

	if(_disp_freq_mode == DISP_FREQ_NOTE) {
		HzToNote(str, hz);
		OutlineDrawText(dc, wdw->GetSize(), str);
	}
	else {
		Rng<char> s = str;
		s = rngPrintNum(s, hz, /*min unit*/1.0f);
		s = rngPrint(s, "Hz");
		OutlineDrawText(dc, wdw->GetSize(), str);
	}
}

//-------------------------------------------------------------------------------------------------
void FxState1_0::onPaintAmpEdit(wxPaintEvent& ev)
{
	wxWindow* wdw = ToWxWindow(ev);
	wxPaintDC dc(wdw);
	dc.SetBrush(wxBrush(*wxBLACK_BRUSH));
	dc.SetPen(wxPen(*wxBLACK_PEN));
	dc.DrawRectangle(wxPoint(), wdw->GetSize());

	// effect value text
	int fx_type = BlkFxParam::getEffectType(getParamInput(BlkFxParam::FX_TYPE));
	FxRun1_0* fft_fx = GetFxRun1_0(fx_type);

	_amp_param_edit->textMorphPrepare(dc);

	CharArray<64> str;

	float val = _amp_param_edit->topSliderValue();

	bool mix_mode = fft_fx->ampMixMode();
	float amp_mult = BlkFxParam::getEffectAmpMult(val, mix_mode);

	if(!fft_fx->paramUsed(BlkFxParam::FX_AMP))
		sprintf(str, "-");
	else if(mix_mode && amp_mult <= 1.0f)
		sprintf(str, "%.0f%% wet", amp_mult*100.0f);
	else if(amp_mult > 0)
		sprintf(str, "%.0f dB", BlkFxParam::getEffectAmp(val));
	else
		sprintf(str, "-inf dB");

	OutlineDrawText(dc, wdw->GetSize(), str);
}

//-------------------------------------------------------------------------------------------------
void FxState1_0::onPaintFxValEdit(wxPaintEvent& ev)
{
	wxWindow* wdw = ToWxWindow(ev);
	wxPaintDC dc(wdw);
	dc.SetBrush(wxBrush(*wxBLACK_BRUSH));
	dc.SetPen(wxPen(*wxBLACK_PEN));
	dc.DrawRectangle(wxPoint(), wdw->GetSize());

	// effect value text
	int fx_type = BlkFxParam::getEffectType(getParamInput(BlkFxParam::FX_TYPE));
	FxRun1_0* fft_fx = GetFxRun1_0(fx_type);

	_fxval_param_edit->textMorphPrepare(dc);

	CharArray<64> str;
	fft_fx->dispVal(this, str, _fxval_param_edit->topSliderValue());
	OutlineDrawText(dc, wdw->GetSize(), str);
}

#  if 0
//-------------------------------------------------------------------------------------------------
void FxState1_0::onPaintTopPanel(wxPaintEvent&)
{
	using namespace BlkFxParam;
	int i;

	wxSize sz = _top_panel->GetSize();
	float wdw_width_f = (float)sz.GetWidth();

	wxBufferedPaintDC dc(_top_panel);
	dc.SetLogicalFunction(wxCOPY);

	// get all of the current params
	int fx_type = BlkFxParam::getEffectType(getParamInput(FX_TYPE));
	FxRun1_0* fft_fx = GetFxRun1_0(fx_type);

	// draw background
	if(_ctrl_focus >= 0)
		dc.SetPen(*wxBLACK_PEN);
	else
		dc.SetPen(*wxRED_PEN);

	dc.SetBrush(*wxBLACK_BRUSH);
	dc.DrawRectangle(dtWxRect(sz));

	// draw freq bars
	dc.SetPen(*wxWHITE_PEN);
	dc.SetBrush(*wxWHITE_BRUSH);
	int freq_pix[2];
	for(i = 0; i < 2; i++)
		freq_pix[i] = (int)lin_interp(getParamInput(i), 0.0f, wdw_width_f-1);

	if(freq_pix[1] > freq_pix[0]) {
		// draw segment between freqs
		dc.DrawRectangle(wxRect(freq_pix[0], 0, freq_pix[1]-freq_pix[0], 5));
	}
	else {
		// exclude segment between freqs
		dc.DrawRectangle(wxRect(0, 0, freq_pix[1], 5));
		dc.DrawRectangle(wxRect(freq_pix[0], 0, sz.GetWidth()-freq_pix[0], 5));
	}

	// draw amp bar
	dc.SetPen(*wxWHITE_PEN);
	dc.SetBrush(*wxWHITE_BRUSH);
	int amp_pix = (int)lin_interp(getParamInput(FX_AMP), 0.0f, wdw_width_f-1);
	dc.DrawRectangle(wxRect(0, 5, amp_pix, 5));

	//
	// write some text on top of everything
	//

	dc.SetFont(*wxNORMAL_FONT);

	// freq text
	//
	char str[256], *s;

	for(i = 0; i < 2; i++) {
		setTextColour(dc, i);
		float hz = BlkFxParam::getHz(getParamInput(i));

		if(!fft_fx->paramUsed(i)) {
			s = str;
			s += sprintf(s, "-");
			OutlineDrawText(dc, _ctrl[i], str);
			continue;
		}
		Array<float, 2> ratio = { .6f, .4f };
		SplitWxRect split(_ctrl[i], ratio, /*split vertical=*/false, /*already normalized=*/true);

		s = str;
		s += HzToNote(s, hz);
		OutlineDrawText(dc, split.next(), str);

		s = str;
		s += sprintNum(s, hz, /*min unit*/1.0f);
		s += sprintf(s, "Hz");
		OutlineDrawText(dc, split.next(), str);
	}

	// amp text
	setTextColour(dc, FX_AMP);

	s = str;
	bool mix_mode = fft_fx->ampMixMode();
	float amp_mult = BlkFxParam::getEffectAmpMult(getParamInput(FX_AMP), mix_mode);

	if(!fft_fx->paramUsed(FX_AMP))
		s += sprintf(s, "-");
	else if(mix_mode && amp_mult <= 1.0f)
		s += sprintf(s, "%.0f%% wet", amp_mult*100.0f);
	else if(amp_mult > 0)
		s += sprintf(s, "%.0f dB", BlkFxParam::getEffectAmp(getParamInput(FX_AMP)));
	else
		s += sprintf(s, "-inf dB");
	OutlineDrawText(dc, _ctrl[FX_AMP], str);
	
	// effect type text
	setTextColour(dc, FX_TYPE);
	const char* name_str = fft_fx->name();
	OutlineDrawText(dc, _ctrl[FX_TYPE], name_str);

	// effect value text
	setTextColour(dc, FX_VAL);
	s = str;
	s += fft_fx->dispVal(this, s, getParamInput(FX_VAL));
	OutlineDrawText(dc, _ctrl[FX_VAL], str);
}
#  endif

#endif
