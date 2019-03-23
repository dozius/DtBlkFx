#if !defined(AFX_STDAFX_H__73DF3553_DC0A_4EF8_9D18_3EECA5C12684__INCLUDED_)
#define AFX_STDAFX_H__73DF3553_DC0A_4EF8_9D18_3EECA5C12684__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WINVER  0x0500 // this is needed for UpdateLayeredWindow
#define _WIN32_WINNT 0x0500 // this is needed for the WS_EX_LAYERED

#define _CRT_SECURE_NO_DEPRECATE

// stop windows from defining min & max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>


#include <stdio.h>
#include <string.h>
#include <vector>
#include <valarray>
#include <string>

#include <vstsdk/public.sdk/source/vst2.x/audioeffectx.h>
#include <vstsdk/public.sdk/source/vst2.x/aeffeditor.h>

#include <vstgui/vstgui.h>
#include <vstgui/vstcontrols.h>
#include <vstgui/aeffguieditor.h>

using namespace std;

//#include <dt/VecSlice.h>

#endif
