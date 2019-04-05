#ifndef _DT_VST_GUI_SUPPORT_H
#define _DT_VST_GUI_SUPPORT_H
/**************************************************************************************************
Copyright (c) 2005-2006 Darrell Tam
email: darrell.barrell@gmail.com
VST is a trademark of Steinberg Media Technologies GmbH

VST GUI support

A collection of classes and templates to help you work better using steinberg's VST GUI classes

To much stuff to list... please have a look through the file.

History
        Date       Version    Programmer         Comments
        16/2/03    1.0        Darrell Tam		Created


This code is free software

***************************************************************************************************/
#include <algorithm>

#include "misc_stuff.h"
#include <vstgui/vstcontrols.h>
#include <vstgui/vstgui.h>

#ifdef _DEBUG
#  include <fstream>
inline std::ostream& operator<<(std::ostream& o, CRect r)
{
  return o << "[" << r.left << "," << r.top << " " << r.width() << "x" << r.height() << "]";
}
inline std::ostream& operator<<(std::ostream& o, CPoint p)
{
  return o << "[" << p.x << "," << p.y << "]";
}
inline std::ostream& operator<<(std::ostream& o, CColor c)
{
  char buf[32];
  sprintf(buf, "#%02x%02x%02x", c.red, c.green, c.blue);
  return o << buf;
}
#endif

struct CColorRGB
    : public CColor
// CColor doesn't have the constructor
{
  CColorRGB(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 0xff)
  {
    red = r;
    green = g;
    blue = b;
    alpha = a;
  }
  CColorRGB(const CColor& src) { CColor::operator=(src); }
};

// attempt to cast something to a CRect via "GetRect"
struct CastCRect : public CRect {
  CastCRect() {}
  template <class SRC_RECT>
  CastCRect(const SRC_RECT& src_rect)
      : CRect(GetRect(src_rect))
  {
  }
};

//
inline CRect GetRect(const CRect& src)
{
  return CRect(src);
}
inline CRect GetRect(const CBitmap* bitmap)
{
  return CRect(0, 0, bitmap->getWidth(), bitmap->getHeight());
}
inline CRect GetRect(const CView* view)
{
  CRect r;
  view->getViewSize(r);
  return r;
}

// various functions to convert between CRect & CPoint
inline CRect MoveTo(const CPoint& top_left, const CastCRect& r)
{
  return CRect(top_left.x, top_left.y, r.width() + top_left.x, r.height() + top_left.y);
}
inline CRect MoveToOrigin(const CastCRect& r)
{
  return MoveTo(CPoint(0, 0), r);
}
inline CRect Offset(CPoint p, CastCRect r)
{
  return r.offset(p.x, p.y);
}
inline CRect MoveLeftEdge(int x, const CastCRect& r)
{
  return CRect(x, r.top, x + r.getWidth(), r.bottom);
}
inline CRect MoveTopEdge(int y, const CastCRect& r)
{
  return CRect(r.left, y, r.right, y + r.getHeight());
}
inline CRect MoveRightEdge(int x, const CastCRect& r)
{
  return CRect(x - r.getWidth(), r.top, x, r.bottom);
}
inline CRect MoveBottomEdge(int y, const CastCRect& r)
{
  return CRect(r.left, y - r.getHeight(), r.right, y);
}

inline CRect Bound(CastCRect r1, const CastCRect& r2)
{
  r1.bound(r2);
  return r1;
}

inline CPoint TopLeft(const CastCRect& r)
{
  return CPoint(r.left, r.top);
}
inline CPoint TopRight(const CastCRect& r)
{
  return CPoint(r.right, r.top);
}
inline CPoint BottomRight(const CastCRect& r)
{
  return CPoint(r.right, r.bottom);
}
inline CPoint BottomLeft(const CastCRect& r)
{
  return CPoint(r.left, r.bottom);
}

// get rectangle that encloses r1 & r2
inline CRect EnclosingRect(const CastCRect& r1, const CastCRect& r2)
{
  return CRect(std::min(r1.left, r2.left),
               std::min(r1.top, r2.top),
               std::max(r1.right, r2.right),
               std::max(r1.bottom, r2.bottom));
}

// treat "rect" as a tile that can be shifted an integer multiple of its width & height
// "num_tiles" allows the rectangle to span several tiles
inline CRect TileOffset(const CastCRect& rect, int idx_x, int idx_y, int num_tiles_x = 1,
                        int num_tiles_y = 1)
{
  int width = rect.getWidth(), height = rect.getHeight();
  int left = rect.left + width * idx_x, top = rect.top + height * idx_y;
  return CRect(left, top, left + width * num_tiles_x, top + height * num_tiles_y);
}

// CRect can be "tiled" by dividing the width & height by div_x & div_y respectively
// The top left point is then offset by idx_x & idx_y multiplied by the resulting width & height
inline CRect GetTile(CastCRect r, int div_x, int div_y, int idx_x = 0, int idx_y = 0,
                     int num_tiles_x = 1, int num_tiles_y = 1)
{
  r.setWidth(r.getWidth() / div_x);
  r.setHeight(r.getHeight() / div_y);
  return TileOffset(r, idx_x, idx_y, num_tiles_x, num_tiles_y);
}

// some operators for cpoints
inline CPoint operator-(CPoint a)
{
  return CPoint(-a.x, -a.y);
}
inline CPoint operator+(CPoint a, CPoint b)
{
  return CPoint(a.x + b.x, a.y + b.y);
}
inline CPoint operator-(CPoint a, CPoint b)
{
  return operator+(a, -b);
}
inline CPoint& operator+=(CPoint& a, CPoint b)
{
  return a.offset(b.x, b.y);
}
inline CPoint& operator-=(CPoint& a, CPoint b)
{
  return operator+=(a, -b);
}

// return centre point of rectangle
inline CPoint GetCent(const CastCRect& r)
{
  return CPoint((r.left + r.right) / 2, (r.top + r.bottom) / 2);
}

// return rectangle "r" centered horizontal at cx
inline CRect MoveXCent(int cx, CastCRect r)
{
  int w = r.width();
  r.left = cx - w / 2;
  r.right = r.left + w;
  return r;
}
inline CRect MoveXCent(const CastCRect& cx, CastCRect r)
{
  return MoveXCent((cx.left + cx.right) / 2, r);
}

// return rectangle "r" centered vertical at cy
inline CRect MoveYCent(int cy, CastCRect r)
{
  int h = r.height();
  r.top = cy - h / 2;
  r.bottom = r.top + h;
  return r;
}
inline CRect MoveYCent(const CastCRect& cy, CastCRect r)
{
  return MoveXCent((cy.top + cy.bottom) / 2, r);
}

// return rectangle "r" centred at point "p"
inline CRect MoveCent(CPoint p, const CastCRect& r)
{
  return MoveYCent(p.y, MoveXCent(p.x, r));
}
inline CRect MoveCent(const CastCRect& p, const CastCRect& r)
{
  return MoveCent(GetCent(p), r);
}

inline CRect LeftHalf(CastCRect r)
{
  r.right = GetCent(r).x;
  return r;
}
inline CRect RightHalf(CastCRect r)
{
  r.left = GetCent(r).x;
  return r;
}
inline CRect TopHalf(CastCRect r)
{
  r.bottom = GetCent(r).y;
  return r;
}
inline CRect BottomHalf(CastCRect r)
{
  r.top = GetCent(r).y;
  return r;
}

inline void FullFillRect(CDrawContext* context, CastCRect r)
{
  r.top--;
  r.left--;
  r.right++;
  r.bottom++;
  context->fillRect(r);
}

inline bool IsContained(const CastCRect& /*container*/ r1, const CastCRect& /*containee*/ r2)
// return true if "r1" contains "r2" completely
{
  return r1.top <= r2.top && r1.bottom >= r2.bottom && r1.left <= r2.left && r1.right >= r1.right;
}

// return true if "r" contains "p"
inline bool IsInside(const CastCRect& r, CPoint p)
{
  return p.isInside(r);
}

//-------------------------------------------------------------------------------------------------
inline void GenerateMouseMoved(CFrame* frame)
// in some cases a mouse move is lost - call this to generate
{
  CPoint mouse;
  frame->getCurrentMouseLocation(mouse);
  frame->onMouseMoved(mouse, frame->getCurrentMouseButtons());
}

//-------------------------------------------------------------------------------------------------
class SplitCRect
//
// split a rectangle into a number of smaller ones according to a ratio array (whose sum need not
// be normalized to 1) either horizontally or vertically
//
{
protected:
  // whether to split horiz or vertical
  bool split_vert;

  // ratio array
  Rng<float> ratio_array;

  // scaling to apply to turn ratio array into pixels
  float ratio_scale;

  // next position
  float next_pos;

  // current rectangle that was generated
  CRect curr_rect;

public:
  SplitCRect(const CastCRect& rect,   // rectangle to split up
             Rng<float> ratio_array_, // array of ratios to use to do the split
             bool split_vert_ = false // direction, default: split horizontally
  )
  {
    split_vert = split_vert_;
    ratio_array = ratio_array_;

    int pixels = split_vert ? rect.getHeight() : rect.getWidth();

    float sum = vec_sum(ratio_array);
    if (sum > 0)
      ratio_scale = (float)pixels / sum;

    curr_rect = rect;
    if (split_vert) {
      next_pos = (float)rect.top;
      curr_rect.setHeight(0);
    }
    else {
      next_pos = (float)rect.left;
      curr_rect.setWidth(0);
    }
  }

  // return true after last rectangle has been returned
  bool isEnd() const { return ratio_array.n <= 0; }

  // return the next rectangle split from the main one, rectangles returned from left to right or
  // top to bottom
  CRect next()
  {
    next_pos += ratio_array[0] * ratio_scale;
    ratio_array.adjStart(1);

    if (split_vert) {
      curr_rect.top = curr_rect.bottom;
      curr_rect.bottom = RndToInt(next_pos);
    }
    else {
      curr_rect.left = curr_rect.right;
      curr_rect.right = RndToInt(next_pos);
    }
    return curr_rect;
  }
};

//-------------------------------------------------------------------------------------------------
inline void InvertRect(CDrawContext* context, const CastCRect& r)
{
  // windows only...
  RECT r_ = {r.left, r.top, r.right, r.bottom};
  InvertRect((HDC)context->getSystemContext(), &r_);
}

//-------------------------------------------------------------------------------------------------
inline void* Select(CDrawContext* context, CBitmap* bitmap)
// select a bitmap into the context
{
  // windows only at the moment
  return SelectObject((HDC)context->getSystemContext(), (HBITMAP)bitmap->getHandle());
}

//-------------------------------------------------------------------------------------------------
bool /*yes?*/ ShouldDrawView(CastCRect update_rect, CView* v);
// return true if the view "v" needs to be redrawn and possibly clear its dirty flag
// if the view is dirty and the update_rect does not completely enclose "v" then return false
// (it will be redrawn later)

//-------------------------------------------------------------------------------------------------
template <class T>
struct VstGuiRef
    : public _PtrBase<T>
//
// wrapper for ptrs to vst gui objects
//
// remember() and forget() methods are automatically called
//
// Assign newly created objects using "setNew" because "CBaseObject" is constructed
// with a reference count of 1 and "setNew" will keep it the same. Watch out for some/most VST GUI
// methods that take a reference counted object (e.g. CViewContainer::addView) because they may
// decrement reference count twice on destruction!
//
// note: don't do view_container.addView(my_view) because it will free-twice (!), use AddView or
// NewView below
//
// As with any simple reference counting scheme, avoid circular references because the
// memory won't get deleted.
//
{
  VstGuiRef(const VstGuiRef& src) { set(src); }
  VstGuiRef(T* ptr_ = NULL) { set(ptr_); }
  ~VstGuiRef() { set(NULL); }

  // internal set
  void set(T* new_ptr)
  {
    T* old_ptr = ptr;
    if (new_ptr)
      new_ptr->remember();
    if (old_ptr)
      old_ptr->forget(); // object may get deleted at this point
    ptr = new_ptr;
  }

  // use this method to assign newly created objects (reference count of object stays the same)
  void setNew(T* newly_created_object)
  {
    set(newly_created_object);
    newly_created_object->forget();
  }

  // allocate new object (must have a constructor taking no args for this to compile)
  VstGuiRef& New()
  {
    setNew(new T);
    return *this;
  }

  // auto cast to compatible reference
  template <class T2> operator VstGuiRef<T2>() const { return (VstGuiRef<T2>(ptr)); }

  // template <class T2>
  //	VstGuiRef& operator = (const T2 &src) { set(src); return *this; }
  VstGuiRef& operator=(T* src)
  {
    set(src);
    return *this;
  }
  VstGuiRef& operator=(const VstGuiRef& src)
  {
    set(src);
    return *this;
  }
};

//-------------------------------------------------------------------------------------------------
template <class T> bool AddView(const VstGuiRef<T>& view, CViewContainer* c)
// example use:
//   VstGuiRef<MyView> my_view;
//   my_view.New();
//   AddView(view_container, my_view);
//
{
  // add view increments ref count by one but on destruction decrements by 2 (!)
  if (view)
    view->remember();

  //
  return c->addView(view);
}

//-------------------------------------------------------------------------------------------------
// example
//   VstGuiRef<MyView> my_view;
//   NewView(view_container, my_view);
//
template <class T> bool NewView(VstGuiRef<T>& view, CViewContainer* c)
{
  view.New();
  return AddView(view, c);
}

#if 0
//-------------------------------------------------------------------------------------------------
template <class T> class VstGuiNoRefCount : public T
//
// disable reference counting for a vst gui object that that is a member variable or stack variable
//
// must have a constructor that takes no arguments
//
// be careful the owning object won't go out of scope before any objects that hold a reference
{
public:
	virtual void forget () {}
	virtual void remember () {}
	VstGuiNoRefCount() { nbReference = 10000; } // some large value
	~VstGuiNoRefCount() { nbReference = 0; }
};
#endif

//-------------------------------------------------------------------------------------------------
// alpha blend function using VST gui's function pointer to alphablend
bool /*true=ok*/ AlphaBlend(CDrawContext* dst_context,
                            CastCRect dst_rect,              // destination rectangle
                            const CDrawContext* src_context, // must have alpha channel & data must
                                                             // have been correctly pre-scaled
                            CastCRect src_rect,              // source rectangle
                            unsigned char overall_alpha,     // 255 for max
                            bool src_alpha = true            // whether to use alpha channel in src
);

//-------------------------------------------------------------------------------------------------
// use font colour for outside and fill colour for inside, this is not an efficient version
// can someone write a better one??
void OutlineDrawString(CDrawContext* context,
                       CastCRect r,     // rectangle to place txt
                       const char* str, // int str_len,
                       CHoriTxtAlign horiz_align = kCenterText
                       // int vert_align = 0	//0=top, 1=middle, 2=bottom
);

//-------------------------------------------------------------------------------------------------
// set the drawing context offset to match the given view
void OffsetDrawContext(CDrawContext* dc, CView* view);

//-------------------------------------------------------------------------------------------------
class CViewDrawContext
    : public CDrawContext
// construct a draw context given a view (would be nice to have this as another constructor on
// CDrawContext)
{
public:
  CViewDrawContext(CView* view)
      : CDrawContext(view->getFrame(), /*system context*/ NULL, view->getFrame()->getSystemWindow())
  {
    OffsetDrawContext(this, view);
  }
};

//-------------------------------------------------------------------------------------------------
struct PtrRGBA
    : public _Ptr<unsigned long>
// pixel pointer wrapper allowing efficient writing to 32 bit RGBA in-memory bitmaps
// (channels in host order)
{
  //
  PtrRGBA(_Ptr<unsigned long> ptr_ = NULL) { ptr = ptr_; }

  // raw get point, note that RGB is alpha prescaled
  unsigned long _get() { return *ptr; }

  // raw draw point (no premultiply applied)
  void _set(unsigned long colour) { *ptr = colour; }

  // raw draw point (no premultiply applied)
  void _set(unsigned long r, unsigned long g, unsigned long b, unsigned long a)
  {
    _set((a << 24) | (r << 16) | (g << 8) | b);
  }

  // draw opaque point (r,g,b: 0..255)
  void set(unsigned long r, unsigned long g, unsigned long b) { _set(r, g, b, 255); }

  // draw point with transparency and premultiplied alpha (r,g,b,a values should be 0..255)
  // all premultiplied alpha match round(channel*alpha/255)
  //
  void set(unsigned long r, unsigned long g, unsigned long b, unsigned long a)
  {
    unsigned a1 = a * 257;
    // unsigned a1 = (a<<8)+a; // perhaps this is faster
    _set((r * a1 + 32896) >> 16, (g * a1 + 32896) >> 16, (b * a1 + 32896) >> 16, a);
  }

  void set(CColor c) { set(c.red, c.green, c.blue, c.alpha); }

  // draw completely transparent point
  void setTransparent() { _set(0UL); }

  // access colour channels
  unsigned char getAlpha() { return (unsigned char)((*ptr) >> 24); }
  unsigned char getRed() { return (unsigned char)((*ptr) >> 16); }
  unsigned char getGreen() { return (unsigned char)((*ptr) >> 8); }
  unsigned char getBlue() { return (unsigned char)(*ptr); }

  // operators do the same as "set"
  void operator()(unsigned long r, unsigned long g, unsigned long b, unsigned long a)
  {
    set(r, g, b, a);
  }
  void operator()(unsigned long r, unsigned long g, unsigned long b) { set(r, g, b); }
  void operator()(CColor c) { set(c); }

  //
  void incX(int n_pixels = 1) { ptr += n_pixels; }
};

//-------------------------------------------------------------------------------------------------
struct Ptr2dRGBA
    : public PtrRGBA
// same as PtrRGBA except also can increment pointer in the x or y directions
//
{
  // in windows this is -width
  int longs_per_line;

  Ptr2dRGBA(_Ptr<unsigned long> ptr_ = NULL, int longs_per_line_ = 0)
  {
    ptr = ptr_;
    longs_per_line = longs_per_line_;
  }

  void incY(int n_pixels = 1) { ptr += n_pixels * longs_per_line; }
};

//-------------------------------------------------------------------------------------------------
struct RowRGBA
    : public PtrRGBA
// pointer to a horizontal line of pixels with an end pointer
{
  // end pointer
  unsigned long* end;

  //
  RowRGBA(_Ptr<unsigned long> ptr_ = NULL, int width = 0)
  {
    ptr = ptr_;
    end = ptr + width;
  }

  // can construct from a range
  RowRGBA(Rng<unsigned long> src)
  {
    ptr = src.ptr;
    end = ptr + src.n;
  }

  bool isEnd() { return ptr == end; }
};

//-------------------------------------------------------------------------------------------------
struct ColumnRGBA
    : public Ptr2dRGBA
// pointer to a vertical line of pixels with an end pointer
{
  // end pointer
  unsigned long* end;

  //
  ColumnRGBA(_Ptr<unsigned long> ptr_, int longs_per_line_, int height)
  {
    ptr = ptr_;
    longs_per_line = longs_per_line_;
    end = ptr + height * longs_per_line;
  }

  bool isEnd() { return ptr == end; }
};

//-------------------------------------------------------------------------------------------------
class CContextRGBA
    : public CDrawContext
//
// Similar to COffscreenContext except that bitmap created is always RGB+alpha with memory
// directly accessible
//
// Windows only... can some one port to mac pls?
//
{
protected:
  void cleanup();

public:
  CContextRGBA(int width = 0, // pixels
               int height = 0 // pixels
  );

  virtual ~CContextRGBA();

  // normal create
  void create(int width, int height);

  // create from a windows resource
  void createFromResourceBitmap(int resoure_bitmap_id);

public:
  // return true if the bitmap memory was allocated ok
  virtual bool ok() const { return _bitmap_data ? true : false; }

  //
  long getWidth() const { return _width; }
  long getHeight() const { return _height; }

  //
  CRect getSize() const
  {
    return Offset(-/*CDrawContext::*/ offset, CRect(0, 0, getWidth(), getHeight()));
  }

  // need to bound the clip rectangle otherwise drawing doesn't work properly (mswin)
  void setClipRect(CRect clip) { CDrawContext::setClipRect(Bound(clip, getSize())); }

public:
  // specify dst & src as rect - this can scale too
  bool blend(CDrawContext* dst_context,
             CastCRect dst_rect,          // destination rectangle
             CastCRect src_rect,          // source rectangle
             unsigned char overall_alpha, // 255 for max
             bool src_alpha = true) const
  {
    if (!ok())
      return false;
    return AlphaBlend(dst_context, dst_rect, this, src_rect, overall_alpha, src_alpha);
  }

  // non scaling, specify dst rect & src point
  bool blend(CDrawContext* dst_context,
             CastCRect dst_rect,                // destination rectangle
             CPoint src_point,                  // source position
             unsigned char overall_alpha = 255, // 255 for max
             bool src_alpha = true) const
  {
    return blend(dst_context, dst_rect, MoveTo(src_point, dst_rect), overall_alpha, src_alpha);
  }

  // non scaling, specify dst point & src rect
  bool blend(CDrawContext* dst_context,
             CPoint dst_point,                  // destination rectangle
             CastCRect src_rect,                // source position
             unsigned char overall_alpha = 255, // 255 for max
             bool src_alpha = true) const
  {
    return blend(dst_context, MoveTo(dst_point, src_rect), src_rect, overall_alpha, src_alpha);
  }

  // non-scaling version, always copy from origin
  bool blend(CDrawContext* dst_context,
             CastCRect dst_rect,                // destination rectangle
             unsigned char overall_alpha = 255, // 255 for max
             bool src_alpha = true) const
  {
    return blend(dst_context, dst_rect, CPoint(0, 0), overall_alpha, src_alpha);
  }

  // non alpha-blend copy (can do an inverted copy)
  void copyTo(CDrawContext* dst_context,
              CastCRect dst_rect, // destination rectangle
              CPoint src_point,   // source position
              bool invert_src = false) const
  {
    dst_rect = Offset(dst_context->offset, dst_rect);
    src_point += /*CDrawContext::*/ offset;
#ifdef _WIN32
    BitBlt((HDC)dst_context->getSystemContext(),
           dst_rect.left,
           dst_rect.top,
           dst_rect.width(),
           dst_rect.height(),
           (HDC)getSystemContext(),
           src_point.x,
           src_point.y,
           invert_src ? NOTSRCCOPY : SRCCOPY);
#endif
  }

  void copyTo(CDrawContext* dst_context,
              CPoint dst_point,   // destination position
              CastCRect src_rect, // source rectangle
              bool invert_src = false) const
  {
    copyTo(dst_context, MoveTo(dst_point, src_rect), TopLeft(src_rect), invert_src);
  }

public:
  // note: msdn says that you should call GdiFlush() before accessing bitmap memory directly
  // (all of these drawPoint methods access memory directly) after using any of the system drawing
  // routines

  // return data (y=height-1, x=0)
  PtrRGBA data() { return _bitmap_data; }

  // return pointer to data with p(0,0) corresponding to top left hand corner (no bounds checking)
  PtrRGBA data(int x, int y) { return _bitmap_data + x + (_height - 1 - y) * _width; }
  PtrRGBA data(const CPoint& p) { return data(p.x, p.y); }

  // get data as 2d
  Ptr2dRGBA get2d(int x, int y) { return Ptr2dRGBA(data(x, y), /*longs per line*/ -getWidth()); }
  Ptr2dRGBA get2d(CPoint p) { return get2d(p.x, p.y); }

  // return pixel range for a line (row) (note, NO bounds checking)
  RowRGBA getRow(int y) { return RowRGBA(data(/*x*/ 0, y), getWidth()); }

  // return pixel range for a column (note, NO bounds checking)
  ColumnRGBA getColumn(int x)
  {
    return ColumnRGBA(data(x, /*y*/ 0), /*longs per line*/ -getWidth(), getHeight());
  }

protected:
  // bitmap width & height
  int _width, _height;

  // windows stuff
  HBITMAP _hbm;
  HGDIOBJ _hbm_prev;

  _Ptr<unsigned long> _bitmap_data;
};
inline CRect GetRect(const CContextRGBA* t)
{
  return CRect(0, 0, t->getWidth(), t->getHeight());
}

//-------------------------------------------------------------------------------------------------
void GenerateGradientRGBA(
    Rng<unsigned long> dst_rgba, // destination RGBA
    Rng<Array<float, 5>> col_vec // each 5-element array consists of: position (0..1), r, g, b, a
);
// generate an RGBA gradient into dst_rgba from set points in "col_vec"
// RGBA as 32 bit host-order colour

//-------------------------------------------------------------------------------------------------
class DtPopupWdw
    : public CBaseObject
//
// wrapper for MSW layered popup window
//
// currently can not receive events (mouse, keys, etc)
//
{
public:
  //
  DtPopupWdw();
  ~DtPopupWdw();

  // create window (but don't display)
  bool create(CFrame* parent_frame);

public:
  //
  void show(bool state = true);

  // set window size & bitmap with an offset into the bitmap
  void update(CastCRect sz,                     // position & size of window, screen coordinates
              CContextRGBA* bm,                 // source bitmap to copy from
              CPoint bm_offs = CPoint(0, 0),    // offset into bitmap to display
              unsigned char overall_alpha = 255 // overall alpha
  );

  // set window position & bitmap (display all of bitmap)
  void update(CPoint pos,                       // position of window, screen coordinates
              CContextRGBA* bm,                 // source bitmap to copy from
              unsigned char overall_alpha = 255 // overall alpha
  )
  {
    CRect sz(0, 0, bm->getWidth(), bm->getHeight());
    sz.offset(pos.x, pos.y);
    update(sz, bm, /*bm_offs*/ CPoint(0, 0), overall_alpha);
  }

  void move(CPoint top_left /*screen coords*/);

  void close();

  bool isOpen() const { return _wdw ? true : false; }

public:
  // most recent window size & position
  CRect _size;

  // windows handle
  HWND _wdw;
};

//-------------------------------------------------------------------------------------------------
class DtHSlider
    : public CControl
//
// this version of HSlider is designed for owner draw
// does not support all of the CControl stuff (like min & max etc)
//
// Note, to ensure that no "setValue()" events are missed, the GUI should
// setDirty(false) before getValue()
//
{
public:
  // state of slider
  typedef enum {
    INACTIVE = 0, // mouse not over slider
    FOCUS,        // mouse is inside slider rectangle (but not over knob)
    OVER_KNOB,    // mouse is over knob
    DRAGGING,     // mouse clicked on knob
    STEPPING      // mouse clicked in rectangle but not on knob
  } State;

public:
  // must call initSlideRect after construction
  DtHSlider();
  ~DtHSlider();

  // clear everything
  void init();

  // init
  void setSize(CastCRect slide_rect, // full rectangle extends 1/2 knob_rect either side
               int knob_width = 21   // width of knob
  );

  // get knob centre, local (parent) coords
  int getKnobCentX() const { return RndToInt(_slide.getRescaled(getValue())); }

  // return "r" centred around knob centre
  CRect /*parent coords*/ getKnobRect(CastCRect r) const
  {
    return MoveCent(CPoint(getKnobCentX(), GetCent(this).y), r);
  }

  // return default knob rect
  CRect /*parent coords*/ getKnobRect() const
  {
    return getKnobRect(CRect(/*x*/ 0, /*y*/ CView::size.top, _knob_width, CView::size.bottom));
  }

  // return rectangle bounds for knob centre
  CRect /*parent coords*/ getSlideRect() const
  {
    CRect t = CView::size;
    t.right -= (_knob_width + 1) / 2;
    t.left += _knob_width / 2;
    return t;
  }

  // return true if "p" is inside the knob rectangle
  bool isInKnob(CPoint /*parent coords*/ p) { return getKnobRect().pointInside(p); }

  // force the state (e.g. force knob slide)
  void setState(State state, int click_pos_x /*parent coords*/, float drag_scale = 1.0f);

  //
  State getState() const { return _state; }

  // set the value, send to listener if changed & set dirty if the knob has moved
  void setValue(float /*0..1*/ v, bool send_to_listener);

public: // override CView methods
  // volatile just in case this is somehow inlined
  virtual void setDirty(const bool state = true)
  {
    (volatile bool&)bDirty = state;
  } ///< set the view to dirty so that it is redrawn in the next idle. Thread Safe !

  // expect owner to draw
  virtual void draw(CDrawContext* pContext) {}

  // mouse methods
  virtual CMouseEventResult
  onMouseDown(CPoint& mouse, const long& buttons); ///< called when a mouse down event occurs
  virtual CMouseEventResult onMouseUp(CPoint& mouse,
                                      const long& buttons); ///< called when a mouse up event occurs
  virtual CMouseEventResult
  onMouseMoved(CPoint& mouse, const long& buttons); ///< called when a mouse move event occurs
  virtual CMouseEventResult
  onMouseEntered(CPoint& mouse, const long& buttons); ///< called when the mouse enters this view
  virtual CMouseEventResult
  onMouseExited(CPoint& mouse, const long& buttons); ///< called when the mouse leaves this view
  virtual bool
  onWheel(const CPoint& where, const CMouseWheelAxis& axis, const float& distance,
          const long& buttons); ///< called if a mouse wheel event is happening over this view

public: // override CControl methods
  // our setvalue is slightly different to CControl
  virtual void setValue(float /*0..1*/ v) { setValue(v, /*send to listener*/ false); }

  // go back to CView isDirty()
  virtual bool isDirty() const { return bDirty; } ///< check if view is dirty

public:
  // increment per call to update() when "stepping"
  float _step_increment;

  // what drag scaling to use on right button or if ctrl+left button
  float _fine_scale_frac;

  // what units to use with the mouse wheel
  float _wheel_increment;

  // width of knob (pixels)
  int _knob_width;

  // slider position (rescaled parent coords)
  FracRescale _slide;

public:
  // what the slider is doing
  State _state;

public: // the following are valid during dragging
  // current drag scale (inverse of magnify)
  float _drag_scale;

  // slide position/offset (parent coords) during drag (similar to _slide but offset
  // and possibly magnified)
  FracRescale _drag;
};

//-------------------------------------------------------------------------------------------------
class COptionMenuOwnerDraw : public COptionMenu {
public:
  typedef enum { INACTIVE, FOCUS, MENU_OPEN } State;

public:
  COptionMenuOwnerDraw();
  void init();

  void setSize(CastCRect r);

  State getState() const { return _state; }

  bool setCurrent(long index, bool count_separator = true)
  // allow index to be -1 which means nothing selected
  {
    if (index != -1)
      return COptionMenu::setCurrent(index, count_separator);
    currentIndex = -1;
    return true;
  }

  void setValue(int index)
  // allow index to be -1 which means nothing selected
  {
    if (index != -1)
      return COptionMenu::setValue((float)index);
    currentIndex = -1;
  }

public:
  // these do nothing because this is an owner-draw control
  virtual void draw(CDrawContext* /*context*/) {} ///< called if the view should draw itself
  virtual void drawRect(CDrawContext* /*context*/, const CRect& /*update_rect*/) {
  } ///< called if the view should draw itself

  //
  virtual CMouseEventResult
  onMouseDown(CPoint& mouse, const long& buttons); ///< called when a mouse down event occurs
  virtual CMouseEventResult
  onMouseEntered(CPoint& mouse, const long& buttons); ///< called when the mouse enters this view
  virtual CMouseEventResult
  onMouseExited(CPoint& mouse, const long& buttons); ///< called when the mouse leaves this view

  // hook this to update our state
  virtual void takeFocus();

public:
  State _state;
};

//-------------------------------------------------------------------------------------------------
class DtPopupHSliderInternal
// popup slider
// does not do any event handling
//
{
public:
  DtPopupHSliderInternal();
  ~DtPopupHSliderInternal();

  void create(CFrame* frame, //
              int top,       // y position of top of rectangle (screen coords)
              int width,     // width of active area
              int height,    // height of active area
              float magnify  // magnify scaling to apply to slider
  );

public:
  // set position of control (nothing is redrawn until update is called)
  void shift(float value, // value 0..1 at "x"
             float pixel  // x pixel offset (screen coordinates)
  );

  // drag the slider and the background rectangle if "new_knob_x" is off the edge
  float /*value*/ drag(int pixel /*screen coords*/);

  //
  void update(float value, // position of knob (0..1)
              int bm_idx,  // which image to use from the bitmaps (assume vertically stacked)
              CContextRGBA* bg_bm,   // background bitmap
              CContextRGBA* knob_bm, // knob bitmap
              bool force_full_update =
                  false // by default, only update bitmaps if the index has changed since last time
  );

  // display the popups
  void show();

  // close the popups
  void close();

public:
  // given a pixel, return the corresponding value
  float /*0..1*/ getValue(float /*screen coord*/ pixel) const { return _rescale.getFrac(pixel); }

  // given a value, return the screen coordinate
  float /*screen coord*/ getPixel(float /*0..1*/ value) const
  {
    return _rescale.getRescaled(value);
  }

  // get background rect (active area)
  CRect /*screen coords*/ getBgRect() const { return _bg_rect; }

  //
  bool isValueInsideBgRect(float /*0..1*/ value) const
  {
    return within(RndToInt(getPixel(value)), _bg_rect.left, _bg_rect.right - 1);
  }

  // return true if the popups are open
  bool popupsAreOpen() const { return _bg_wdw.isOpen() && _knob_wdw.isOpen(); }

protected:
  // magnify to apply to slider
  float _magnify;

  // convert from value 0..1 to screen coords
  FracRescale _rescale;

  // pop-up windows
  DtPopupWdw _bg_wdw, _knob_wdw;

  // active background rectangle, without border (width & height as specified in init), screen
  // coordinates
  CRect _bg_rect;

public: // the following attempt to minimize redrawing
  // which image is being displayed (each bitmap has 2 images stacked vertically: not-selected &
  // selected)
  int _bg_i, _knob_i;
};

//-------------------------------------------------------------------------------------------------
class DtPopupHSlider
    : public COptionMenu
    ,                         // we have a pull down menu
      public CControlListener // we intercept our own messages from COptionMenu

// horizontal slider
// currently non popup behaviour is owner draw
//
// Note, to ensure that no "setValue()" events are missed, the GUI should
// setDirty(false) before getValue()
//
{
public:
  typedef enum {
    INACTIVE = -10, // mouse is not over control
    FOCUS,          // mouse is over control but button not clicked
    MENU_OPEN,      // option menu has been dropped down (right mouse)
    PRE_DRAG,       // left mouse has been clicked but neither control is selected yet
    COARSE = 0,     // active control (mouse is clicked)
    FINE = 1
  } State;

public:
  // default images for popup sliders (need to be loaded before use)
  struct Images {
    // index is COARSE or FINE, each image is tiled 1 x 2 (non-selected, selected)
    static Array<VstGuiRef<CContextRGBA>, 2> g_bg, g_knob;
  };

public:
  //
  DtPopupHSlider();
  ~DtPopupHSlider();

  // set everything back to defaults
  void init();

  // initialize size & mousable area to "rect", parent coordinates
  void setSize(CastCRect rect);

  //
  CControlListener* getListener() const { return _listener; }

  //
  void setListener(CControlListener* l) { _listener = l; }

  //
  State getState() const { return _state; }

  // set the value, send to listener if changed & set dirty if the knob has moved
  void setValue(float /*0..1*/ v, bool send_to_listener = false);

  // set which buttons we're allowed to accept
  void setHitTestButtonMask(long new_mask) { _hittest_button_mask = new_mask; }

public: // menu interface
  //
  void setMenuSnap(bool snap_state = true);

  // only valid during listener->valueChanged() >= 0 if menu item selected otherwise -1 for slider
  // drag
  int lastMenuIdx() { return _last_menu_idx; }

  //
  void removeAllMenuEntries();

  //
  int /*position added*/ addMenuEntry(float value,    // 0..1 or -ve for no associated value
                                      const char* txt //
  );

  bool setMenuEntry(long index,     // menu index
                    float value,    // 0..1 or -ve for no associated value
                    const char* txt //
  );

  //
  int findClosestMenuIdx(float /*0..1*/ v) const;

public: // override CView methods
  // volatile in case inlined
  virtual void setDirty(const bool state = true)
  {
    (volatile bool&)bDirty = state;
  } ///< set the view to dirty so that it is redrawn in the next idle. Thread Safe !

  // expect owner to draw
  virtual void draw(CDrawContext* /*context*/) {} ///< called if the view should draw itself
  virtual void drawRect(CDrawContext* /*context*/, const CRect& /*update_rect*/) {
  } ///< called if the view should draw itself

  // mouse methods
  virtual CMouseEventResult
  onMouseDown(CPoint& mouse, const long& buttons); ///< called when a mouse down event occurs
  virtual CMouseEventResult onMouseUp(CPoint& mouse,
                                      const long& buttons); ///< called when a mouse up event occurs
  virtual CMouseEventResult
  onMouseMoved(CPoint& mouse, const long& buttons); ///< called when a mouse move event occurs
  virtual CMouseEventResult
  onMouseEntered(CPoint& mouse, const long& buttons); ///< called when the mouse enters this view
  virtual CMouseEventResult
  onMouseExited(CPoint& mouse, const long& buttons); ///< called when the mouse leaves this view
  virtual bool
  onWheel(const CPoint& where, const CMouseWheelAxis& axis, const float& distance,
          const long& buttons); ///< called if a mouse wheel event is happening over this view

  //
  virtual bool hitTest(const CPoint& where, const long buttons);

public: // override CControl methods
  // note that COptionMenu calls the virtual setValue(), but direct calls to our class call
  // setValue(float, bool), above virtual void setValue(float v);

  // go back to CView isDirty()
  virtual bool isDirty() const { return bDirty; } ///< check if view is dirty

public: // override COptionMenu
  // hook this to update our state
  virtual void takeFocus();

public: // override CControlListener
  virtual void valueChanged(VSTGUI::CControl* pControl);

protected: // internals
  // update popups to current value if they're open
  void updatePopups();

  //
  bool popupsAreOpen() const { return _slider[0].popupsAreOpen() && _slider[1].popupsAreOpen(); }

  //
  CMouseEventResult sliderMouseClick(CPoint mouse, long buttons);

  //
  CMouseEventResult menuMouseClick(CPoint mouse, long buttons);

protected:
  // mask of mouse buttons that we allow hittest to return true
  long _hittest_button_mask;

  // fraction of scale that fine slider affects
  float _fine_scale_frac;

  // current state
  State _state;

  //
  bool _snap_to_menu;

  //
  std::vector<float> _menu_item_to_value;

  // this is the real listener since CControl::listener points back to us (for
  // intercepting menu change)
  _Ptr<CControlListener> _listener;

protected: // temporary stuff that is only used during mouse drag
  // map parent coords to frame coords
  CPoint _wdw_frame_offs;

  // map parent coords to screen coords
  CPoint _wdw_screen_offs;

  // position of initial click
  CPoint _click_pos;

  // COARSE & FINE sliders
  DtPopupHSliderInternal _slider[2];

  // index is COARSE or FINE
  Array<VstGuiRef<CContextRGBA>, 2> _bg_bm;

  // index is COARSE or FINE
  Array<VstGuiRef<CContextRGBA>, 2> _knob_bm;

  // save the value before take focus because it may mangle it (by changing it to the menu
  // index)
  float _value_before_take_focus;

  // used to differentiate between menu item being selected and the slider
  int _last_menu_idx;
};

//-------------------------------------------------------------------------------------------------
class DtOwnerToggle
    : public CControl
//
// owner draw toggle button (value is either 0.0f or 1.0f)
//
{
public:
  typedef enum {
    INACTIVE,
    FOCUS,
    MOUSE_DOWN_OUTSIDE, // we got the mouse click
    MOUSE_DOWN_INSIDE
  } State;

public:
  DtOwnerToggle();
  ~DtOwnerToggle();

  // set everything back to defaults
  void init();

  //
  void setSize(CastCRect rect);

  //
  State getState() const { return _state; }

  // get value as a bool
  bool getBoolValue() const { return CControl::value != 0.0f; }

  // set value
  void setBoolValue(bool new_value, bool send_to_listener);

  // if the mouse is pressed, return the value that will be taken if it is released
  bool getTentativeValue() const
  {
    return _state == MOUSE_DOWN_INSIDE ? !getBoolValue() : getBoolValue();
  }

public:
  // volatile in case inlined
  virtual void setDirty(const bool state = true)
  {
    (volatile bool&)bDirty = state;
  } ///< set the view to dirty so that it is redrawn in the next idle. Thread Safe !

  // expect owner to draw
  virtual void draw(CDrawContext* pContext) {}

  virtual CMouseEventResult
  onMouseDown(CPoint& mouse, const long& buttons); ///< called when a mouse down event occurs
  virtual CMouseEventResult onMouseUp(CPoint& mouse,
                                      const long& buttons); ///< called when a mouse up event occurs
  virtual CMouseEventResult
  onMouseMoved(CPoint& mouse, const long& buttons); ///< called when a mouse move event occurs
  virtual CMouseEventResult
  onMouseEntered(CPoint& mouse, const long& buttons); ///< called when the mouse enters this view
  virtual CMouseEventResult
  onMouseExited(CPoint& mouse, const long& buttons); ///< called when the mouse leaves this view

public: // override CControl methods
  //
  virtual void setValue(float v) { setBoolValue(v != 0.0f, /*send to listener*/ false); }

  // go back to CView isDirty()
  virtual bool isDirty() const { return bDirty; } ///< check if view is dirty

public:
  // mouse state
  State _state;
};

//-------------------------------------------------------------------------------------------------
namespace Event {
// event types for mouse
typedef enum {
  kMouseDown,
  kMouseUp,
  kMouseMoved,
  kMouseEntered,
  kMouseExited,
  kMouseWheel
} MouseType;

struct Args {
  // cookie supplied by handler
  int target_cookie;
};

struct MouseArgs : public Args {
  //
  bool post_call;
  //
  MouseType event_type;
  // source CView
  CView* src_view;
  // result to be returned by event target
  CMouseEventResult result;
  // mouse location in view (actually view-parent) coords
  CPoint mouse;
  // buttons currently pressed
  long buttons;
};

// mouse extra
struct MouseWheelArgs : public MouseArgs {
  //
  CMouseWheelAxis axis;
  //
  float distance;
};
}; // namespace Event

//-------------------------------------------------------------------------------------------------
template <class VIEW, class EVENT_TARGET>
class CViewEventIntercept
    : public VIEW
// intercept mouse events from VIEW (derived from CView) and call a function from
// EVENT_TARGET. This is all a bit clumsy but is simple
//
// EVENT_TARGET should have a function similar to this:
// void mouseHandler(Event::Args* args)
// {
//     Event::MouseArgs* mouse = (Event::MouseArgs*);
//
//     // do this if the event is to be eaten and not passed to the original handlers
//     mouse.result = kMouseEventHandled;
//
//     // process events
//     switch(mouse->event_type) {
//         Event::MOUSE_DOWN:
//         break;
//         Event::MOUSE_UP:
//         break;
//         ... etc ...
//     }
// }
//
{
  typedef void (EVENT_TARGET::*EventFn)(Event::Args*);

  //
  _Ptr<EVENT_TARGET> _event_target;

  //
  EventFn _mouse_fn_before, _mouse_fn_after;

  // cookie supplied by target
  int _mouse_target_cookie;

public:
  CViewEventIntercept()
  {
    _mouse_fn_before = NULL;
    _mouse_fn_after = NULL;
  }

  //
  void interceptMouseEvents(
      EVENT_TARGET* event_target, // target
      EventFn mouse_fn_before,    // method to be called prior to mouse handler methods
      EventFn mouse_fn_after,     // method to be called after mouse handler
      int cookie = 0              // cookie to pass back
  )
  {
    _event_target = event_target;
    _mouse_fn_before = mouse_fn_before;
    _mouse_fn_after = mouse_fn_after;
    _mouse_target_cookie = cookie;
  }

public:
#define CViewEventIntercept_PRE()                                                                  \
  args.target_cookie = _mouse_target_cookie;                                                       \
  args.post_call = false;                                                                          \
  args.src_view = this;                                                                            \
  args.result = kMouseEventNotHandled;                                                             \
  args.mouse = mouse;                                                                              \
  args.buttons = buttons;                                                                          \
  if (_mouse_fn_before)                                                                            \
    (_event_target->*_mouse_fn_before)(&args);

#define CViewEventIntercept_POST()                                                                 \
  args.post_call = true;                                                                           \
  if (_mouse_fn_after)                                                                             \
    (_event_target->*_mouse_fn_after)(&args);

#define CViewEventIntercept_MOUSE(METHOD, EVENT_TYPE)                                              \
  virtual CMouseEventResult METHOD(CPoint& mouse, const long& buttons)                             \
  {                                                                                                \
    Event::MouseArgs args;                                                                         \
    args.event_type = EVENT_TYPE;                                                                  \
    CViewEventIntercept_PRE();                                                                     \
    if (args.result != kMouseEventHandled)                                                         \
      args.result = VIEW::METHOD(mouse, buttons);                                                  \
    CViewEventIntercept_POST();                                                                    \
    return args.result;                                                                            \
  }

  CViewEventIntercept_MOUSE(onMouseDown, Event::kMouseDown)
      CViewEventIntercept_MOUSE(onMouseUp, Event::kMouseUp)
          CViewEventIntercept_MOUSE(onMouseMoved, Event::kMouseMoved)
              CViewEventIntercept_MOUSE(onMouseEntered, Event::kMouseEntered)
                  CViewEventIntercept_MOUSE(onMouseExited, Event::kMouseExited)

                      virtual bool onWheel(const CPoint& mouse, const CMouseWheelAxis& axis,
                                           const float& distance, const long& buttons)
  {
    Event::MouseWheelArgs args;
    args.event_type = Event::kMouseWheel;
    args.axis = axis;
    args.distance = distance;
    CViewEventIntercept_PRE();
    if (args.result != kMouseEventHandled)
      if (VIEW::onWheel(mouse, axis, distance, buttons))
        args.result = kMouseEventHandled;
    CViewEventIntercept_POST();
    return args.result == kMouseEventHandled;
  }
};

#if 0
struct CallArgs
{
	int args_type;
	CallArgs(int args_type_) { args_type = args_type_; }
	virtual ~CallArgs() {}
};

struct CallWrapper
{
	typedef std::list<CallWrapper*> List;

	int call_type;

	CallWrapper(int call_type_ = 0) { call_type = call_type_; }
	virtual ~CallWrapper() {}
	virtual void call(CallArgs* args) = 0;
	virtual void type() { return call_type; }
};
void Call(CallWrapper::List& list, CallArgs* args)
{
	CallWrapper::List::iterator i = list.begin();
	while(i != list.end()) {
		(*i)->call(args);
		i++;
	}
}

template <class T> class MethodWrapper : public CallWrapper
{
	typedef (T::*Method)(CallWrapper* call_wrapper, CallArgs* args, CallArgs* ret);
	T* target;
	void Method method;

	MethodWrapper(T* target_, Method method_, int call_type_ = 0) : CallWrapper(call_type_)
		{ target = target_; method = method_; }

	virtual void call(CallArgs* args, CallArgs* ret) { target->*method(this, args, ret); }
};

#endif

#if 0
//-------------------------------------------------------------------------------------------------
struct CObjListNode : public virtual CBaseObject
//
// intrusive list designed for sending messages but can be used for anything
{

	CObjListNode() { obj_list_id = DEFAULT_ID; }

	~CObjListNode() { remove(); }

	//
	VstGuiRef<CObjListNode> obj_list_prev, obj_list_next;

	// insert us before "b"
	void insertBefore(CObjListNode* b)
	{
		obj_list_next = b;
		obj_list_prev = b->obj_list_prev;
		if(b->obj_list_prev) b->obj_list_prev->obj_list_next = this;
		b->obj_list_prev = this;
	}

	// insert us after "b"
	void insertAfter(CObjListNode* b)
	{
		obj_list_next = b->obj_list_next;
		obj_list_prev = b;
		if(b->obj_list_next) b->obj_list_next->obj_list_prev = this;
		b->obj_list_next = this;
	}

	// splice us out of the list (if we are inserted)
	void remove()
	{
		if(obj_list_prev) obj_list_prev->obj_list_next = obj_list_next;
		if(obj_list_next) obj_list_next->obj_list_next = obj_list_prev;
		obj_list_prev = NULL;
		obj_list_next = NULL;
	}	
};

//-------------------------------------------------------------------------------------------------
struct Msg : public CObjListNode
// messages
{
	Msg() { msg_id = 0; }

	// source of message
	_Ptr<CBaseObject> msg_src;

	// object we keep alive (or NULL if we're a member)
	VstGuiRef<CBaseObject> msg_src_ref;

	// object list id (used for messages)
	int msg_id;

};

//-------------------------------------------------------------------------------------------------
template <class LIST_NODE = CObjListNode> struct CObjListBase
{
	//
	CObjListBase()
	{
		us.obj_list_prev = &us;
		us.obj_list_next = &us;
	}

	//
	~CObjListBase() { removeAll(); }

	//
	void addTail(CObjListNode* node) { node.insertBefore(&us); }
	void addHead(CObjListNode* node) { node.insertBefore(us.obj_list_next); }

	//
	bool isEmpty() const { return us.obj_list_prev == &us; }

	//
	VstGuiRef<LIST_NODE> peekHead() { return isEmpty() ? NULL : static_cast<LIST_NODE>(us).obj_list_next; }

	//
	VstGuiRef<LIST_NODE> getHead()
	{
		VstGuiRef<LIST_NODE> r = peekHead();
		if(r) r->remove();
		return r;
	}

	//
	VstGuiRef<LIST_NODE> peekTail() { return isEmpty() ? NULL : static_cast<LIST_NODE>(us).obj_list_prev; }

	//
	VstGuiRef<LIST_NODE> getTail()
	{
		VstGuiRef<LIST_NODE> r = peekTail();
		if(r) r->remove();
		return r;
	}

	//
	void removeAll() { while(getHead()); }

	// obj_list_prev is the tail, obj_list_next is the head
	CObjListNode us; 
};

typedef CObjListBase<Msg> MsgListBase;

#endif

#if 0

//-------------------------------------------------------------------------------------------------
class ImprovedCFrame : public CFrame
{
public:
	ImprovedCFrame(CRect size, void *system_wdw, VSTGUIEditorInterface *vst_editor) :
		CFrame(size, system_wdw, vst_editor) {}

public:
	CMouseEventResult onMouseDown (CPoint &where, const long& buttons);
	CMouseEventResult onMouseUp (CPoint &where, const long& buttons);
	CMouseEventResult onMouseMoved (CPoint &where, const long& buttons);

};
#endif

#endif
