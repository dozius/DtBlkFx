#ifndef _HTML_LOG_H_
#define _HTML_LOG_H_

#include "misc_stuff.h"

#include <fstream>
#include <sstream>
#include <string.h>
#include <time.h>
#include <vector>

//-------------------------------------------------------------------------------------------------
class HtmlLog {
protected:
  std::ofstream _f;
  CriticalSectionWrapper _protection;

public:
  bool open(const char* file_name)
  {
    _f.open(file_name);
    if (!_f)
      return false;
    _f << "<html><body><pre>" << std::endl;
    return true;
  }

  HtmlLog() {} // call "open"
  HtmlLog(const char* file_name) { open(file_name); }

  std::ostream&
  startline(const char* style // eg: "color:#ff0000;background:#000000" for red on black
  )
  {
    bool print_style = style && style[0];
    float clk_sec = (float)clock() / 1000.0f;

    _protection.lock();
    _f << SprfCharArray<16>("%.3f ", clk_sec);
    if (print_style)
      _f << "<span style='" << style << "'>";
    else
      _f << "<span>";

    return _f;
  }

  std::ostream& startline(unsigned long fg_bg_12bit // eg 0xF00000 for red on black
  )
  {
    return startline(SprfCharArray<32>(
        "color:#%03x;background:#%03x", (fg_bg_12bit >> 12) & 0xFFF, fg_bg_12bit & 0xFFF));
  }

  // must ALWAYS call endline() otherwise _protection will be still locked
  void endline()
  {
    _f << "</span>" << std::endl;
    _protection.unlock();
  }
};

// print to the html log
#define HTML_LOG(html_log, style, stream)                                                          \
  {                                                                                                \
    try {                                                                                          \
      (html_log).startline(style) << stream;                                                       \
    }                                                                                              \
    catch (...) {                                                                                  \
    }                                                                                              \
    (html_log).endline();                                                                          \
  }

// print to the html log and to the debugger
#define DBG_HTML_LOG(html_log, html_style, stream)                                                 \
  {                                                                                                \
    std::ostringstream _html_log_stream_;                                                          \
    _html_log_stream_ << stream;                                                                   \
    (html_log).startline(style) << _html_log_stream_.str();                                        \
    (html_log).endline();                                                                          \
    _CrtDbgReport(_CRT_WARN, __FILE__, __LINE__, "", "%s", _html_log_stream_.str().c_str());       \
  }

//-------------------------------------------------------------------------------------------------
class HtmlTable {
public:
  // header line
  std::string _hdr;

  // current output line
  std::ostringstream _line;

  // line number
  int _line_n;

  // number of lines per "page" before we print the hdr
  int _lines_per_pg;

  // output stream
  std::ofstream _f;

  HtmlTable(const char* file_name, const char* style_sheet, const char** labels, int n_labels)
  {
    _f.open(file_name);

    _f << "<html>" << std::endl
       << "<link rel=\"stylesheet\" href=\"" << style_sheet << "\" type=\"text/css\">"
       << "<body><table>" << std::endl;

    _line << "<tr>";
    _line << "<th>time";
    for (int i = 0; i < n_labels; i++)
      _line << "<th>" << labels[i];
    _hdr = _line.str();
    _line.str("");
    _line_n = 0;
    _lines_per_pg = 30;
  }

  ~HtmlTable() { _f << "</table></body></html>" << std::endl; }

  void printHdr() { _f << _hdr << std::endl; }

  template <class T> HtmlTable& operator<<(T& value)
  {
    _line << "<td>" << value;
    return *this;
  }

  void flushLine()
  {
    if (!_line_n)
      printHdr();

    _f << "<tr><td>" << SprfCharArray<16>("%.3f", (float)clock() / 1000.0f) << _line.str()
       << std::endl;

    _line.str("");
    if (++_line_n >= _lines_per_pg)
      _line_n = 0;
  }

  struct Dummy {
    int dummy;
  };
  static Dummy end_row;

  HtmlTable& operator<<(Dummy& dummy)
  {
    flushLine();
    return *this;
  }
};

#endif
