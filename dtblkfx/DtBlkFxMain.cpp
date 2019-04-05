//-------------------------------------------------------------------------------------------------------
// DtBlkFx main
// based on VST example code

#include <StdAfx.h>

#include "DtBlkFx.hpp"
#include "Gui.h"
#include "PngVstGui.h"
#include "VstGuiSupport.h"
#include "fftw_support.h"
#include "misc_stuff.h"
#include "rfftw_float.h"
#include <sstream>

#include "Debug.h"

#ifdef _DEBUG
HtmlLog global_log("dtblkfx_global_log.html");
#endif

using namespace std;

#if defined(__GNUC__)
#  define VST_EXPORT extern "C" __attribute__((visibility("default")))
#else
#  define VST_EXPORT
#endif

#if _WIN32

// used by VST GUI framework for registering GUI message handler with windows
void* hInstance;

HINSTANCE g_dll_inst = NULL;

#endif

// full dir that our dll loaded from (including trailing slash)
CharArray<4096> g_plugin_path;

// protect against the (unlikely) event of our init being called more than once simultaneously
static CriticalSectionWrapper g_init_protection;
typedef enum {
  GLOBAL_LOAD_STATE_NEED_INIT,
  GLOBAL_LOAD_STATE_INIT_OK,
  GLOBAL_LOAD_STATE_INIT_FAIL,
  GLOBAL_LOAD_STATE_MISSING_FILES
} GlobalLoadState;

GlobalLoadState g_load_state = GLOBAL_LOAD_STATE_NEED_INIT;

// got images ok?
bool GlobalInitOk()
{
  return g_load_state == GLOBAL_LOAD_STATE_INIT_OK;
}

// path is <dll path>\BLKFX_DIR
#define BLKFX_DIR "dtblkfx\\"

#ifdef STEREO
#  define FILE_PREFIX BLKFX_DIR "stereo_"
#else
#  define FILE_PREFIX BLKFX_DIR "mono_"
#endif

//-------------------------------------------------------------------------------------------------
struct {
  VstGuiRef<CContextRGBA>* dst;
  const char* file_name;
} g_load_images[] = {
    {&DtPopupHSlider::Images::g_bg[DtPopupHSlider::COARSE], FILE_PREFIX "hslider_bg_coarse.png"},
    {&DtPopupHSlider::Images::g_bg[DtPopupHSlider::FINE], FILE_PREFIX "hslider_bg_fine.png"},
    {&DtPopupHSlider::Images::g_knob[DtPopupHSlider::COARSE],
     FILE_PREFIX "hslider_knob_coarse.png"},
    {&DtPopupHSlider::Images::g_knob[DtPopupHSlider::FINE], FILE_PREFIX "hslider_knob_fine.png"},
    {&Images::g_splash_bg, FILE_PREFIX "splash.png"},
    {&Images::g_glob_bg, FILE_PREFIX "global_ctrl.png"},
    {&Images::g_fx_bg, FILE_PREFIX "fx_bg.png"}};

vector<VstProgram<BlkFxParam::TOTAL_NUM>> g_blk_fx_presets;

//-------------------------------------------------------------------------------------------------
struct LoadPresets {
  _Ptr<FILE> f;

  LoadPresets(ostream* err)
  {
    // get presets file name
    CharArray<4096> path;
    path << g_plugin_path << FILE_PREFIX "presets.txt";

    // open presets file
    f = fopen(path, "r");
    if (!f) {
      *err << path << " could not be opened";

      g_blk_fx_presets.resize(20);
      g_blk_fx_presets[0].name << "file not found";
      for (int i = 1; i < (int)g_blk_fx_presets.size(); i++)
        g_blk_fx_presets[i].name << i << " user";
      return;
    }

    // load lines and build presets from those
    CharArray<8192> line;
    while (1) {
      if (!fgets(line, line.size(), f))
        break;

      g_blk_fx_presets.push_back(VstProgram<BlkFxParam::TOTAL_NUM>(line));
    }
  }

  operator bool() { return f ? true : false; }

  ~LoadPresets()
  {
    if (f)
      fclose(f);
  }
};

//-------------------------------------------------------------------------------------------------
VST_EXPORT AEffect* VSTPluginMain(audioMasterCallback audioMaster)
{
  LOGG("", "VSTPluginMain" << VAR((void*)audioMaster));

  // Get VST Version
  if (!audioMaster(0, audioMasterVersion, 0, 0, 0, 0))
    return 0; // old version

  try {
    // do one time init stuff here because it's safer than from DllMain (can't load DLLs there)
    ScopeCriticalSection sml(g_init_protection);

    // don't create anything if loading previously failed
    if (g_load_state == GLOBAL_LOAD_STATE_INIT_FAIL)
      return NULL;

    // otherwise init if we need to
    if (g_load_state == GLOBAL_LOAD_STATE_NEED_INIT) {

      // assume failure for now
      g_load_state = GLOBAL_LOAD_STATE_INIT_FAIL;

      ostringstream err_str;

// try to load FFTW dll (can't work without it)
#ifdef _WIN32
      vector<string> paths;
      paths.push_back(g_plugin_path.toString() + BLKFX_DIR);
      paths.push_back(g_plugin_path);
      if (!LoadFFTWfDll(paths, &err_str)) {
        MessageBox(NULL, err_str.str().c_str(), "DtBlkFx, DLL error", MB_OK);
        return NULL;
      }
#endif

      CreateFFTWfPlans();

      //
      g_load_state = GLOBAL_LOAD_STATE_MISSING_FILES;

      // now try to load images
      bool image_error = false;
      for (int i = 0; i < NUM_ELEMENTS(g_load_images); i++) {
        if (!ReadPng(g_load_images[i].dst->New(),
                     g_plugin_path.toString() + g_load_images[i].file_name,
                     &err_str))
          image_error = true;
      }

      // load presets from the text file
      if (!LoadPresets(&err_str))
        image_error = true;

      // check for error
      if (image_error)
        MessageBox(NULL, err_str.str().c_str(), "DtBlkFx image loading error", MB_OK);
      else
        g_load_state = GLOBAL_LOAD_STATE_INIT_OK;
    }
    // Create the AudioEffect
    AEffect* t = (new DtBlkFx(audioMaster))->getAeffect();

    return t;
  }
  // some kind of error (memory allocation or DLL loading...)
  catch (...) {
  }
  return NULL;
}

//-------------------------------------------------------------------------------------------------
#if _WIN32
#  include <windows.h>
BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpvReserved)
{
  LOGG("", "DllMain" << VAR(hInst) << VAR(dwReason) << VAR(lpvReserved));
  hInstance = hInst;

  switch (dwReason) {
    case DLL_PROCESS_ATTACH: {
      g_dll_inst = hInst;

      //
      Clear(g_plugin_path);
      GetModuleFileName(hInst, g_plugin_path, g_plugin_path.size());

      // string length
      int i = g_plugin_path.strlen() - 1;

      // remove dll filename by going backwards until first "\" or ":" (leaving it on)
      while (i >= 0 && g_plugin_path[i] != '\\' && g_plugin_path[i] != ':')
        g_plugin_path[i--] = 0;

      break;
    }

    case DLL_PROCESS_DETACH:
      // uninitilize wxWidgets
      break;
  }

  return TRUE;
}

#endif

// support for old hosts not looking for VSTPluginMain
#if (TARGET_API_MAC_CARBON && __ppc__)
VST_EXPORT AEffect* main_macho(audioMasterCallback audioMaster)
{
  return VSTPluginMain(audioMaster);
}
#endif
