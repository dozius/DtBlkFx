#ifndef _DEBUG_H_
#define _DEBUG_H_

#define LOG(style, str)                                                                            \
  {                                                                                                \
  }
#define DBG_CODE(a)
#define DBG(a)                                                                                     \
  {                                                                                                \
  }
#define LOGG(style, str)                                                                           \
  {                                                                                                \
  }

#ifdef _DEBUG
#  undef DBG_CODE
#  define DBG_CODE(a) a

#  include "HtmlLog.h"

// print to debug file
#  ifdef LOG_FILE_NAME
static HtmlLog dbg(LOG_FILE_NAME);

#    undef LOG
#    define LOG(style, str) HTML_LOG(dbg, style, str)
#  endif

// global log file
extern HtmlLog global_log;
#  undef LOGG
#  define LOGG(style, str) HTML_LOG(global_log, style, str)

//
#  include <sstream>
#  undef DBG
#  define DBG(stream)                                                                              \
    {                                                                                              \
      std::ostringstream dbg_ostrstream;                                                           \
      dbg_ostrstream << stream << std::endl;                                                       \
      _CrtDbgReport(_CRT_WARN, __FILE__, __LINE__, "DtBlkFx", "%s", dbg_ostrstream.str().c_str()); \
    }

#endif

#endif
