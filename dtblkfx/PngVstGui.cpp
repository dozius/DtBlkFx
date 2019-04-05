#include <StdAfx.h>

#include "../libpng/png.h"
#include "PngVstGui.h"

using namespace std;

//-------------------------------------------------------------------------------------------------
struct InternalReadPng
// internal glue
{
  string file_name;
  _Ptr<png_struct> png_ptr;
  _Ptr<png_info> info_ptr;
  _Ptr<FILE> fp;

  // errors & other messages are written here
  ostream* err_str;

  // whether reading was ok
  bool ok;

  InternalReadPng(CContextRGBA* dst, string file_name_, ostream* err_str_);
  ~InternalReadPng();
};

//-------------------------------------------------------------------------------------------------
static void PngErrorCb(png_structp png_ptr, png_const_charp err_msg)
{
  InternalReadPng* t = (InternalReadPng*)(png_ptr->error_ptr);
  if (t->err_str)
    *t->err_str << "PNG: " << err_msg << " while reading " << t->file_name << endl;
}

//-------------------------------------------------------------------------------------------------
static void PngWarningCb(png_structp png_ptr, png_const_charp warn_msg)
{
  InternalReadPng* t = (InternalReadPng*)(png_ptr->error_ptr);
  if (t->err_str)
    *t->err_str << "PNG warning: " << warn_msg << " while reading " << t->file_name << endl;
}

//-------------------------------------------------------------------------------------------------
InternalReadPng::~InternalReadPng()
{
  /* clean up after the read, and free any memory allocated - REQUIRED */
  if (png_ptr)
    png_destroy_read_struct(&png_ptr.ptr, info_ptr ? &info_ptr.ptr : NULL, NULL);

  if (fp)
    fclose(fp);
}

//-------------------------------------------------------------------------------------------------
InternalReadPng::InternalReadPng(CContextRGBA* dst, string file_name_, ostream* err_str_)
{
  ok = false;
  file_name = file_name_;
  err_str = err_str_;

  if (!dst) {
    if (err_str)
      *err_str << "PNG: Internal error, no memory for destination" << file_name << endl;
    return;
  }

  fp = fopen(file_name.c_str(), "rb");
  if (!fp) {
    if (err_str)
      *err_str << "PNG: Couldn't open " << file_name << endl;
    return;
  }

  //
  png_ptr =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)this, &PngErrorCb, &PngWarningCb);

  if (!png_ptr) {
    if (err_str)
      *err_str << "PNG: internal error, couldn't create read structure while reading " << file_name
               << endl;
    return;
  }

  //
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    if (err_str)
      *err_str << "PNG: internal error, couldn't create info structure while reading " << file_name
               << endl;
    return;
  }

  // if there's an error, we jump back to here
  if (setjmp(png_jmpbuf(png_ptr))) {
    if (err_str)
      *err_str << "PNG: error while reading " << file_name << endl;
    return;
  }

  //
  png_init_io(png_ptr, fp);

  png_read_png(png_ptr,
               info_ptr,
               PNG_TRANSFORM_SWAP_ENDIAN |  // swap bytes if 16 bit
                   PNG_TRANSFORM_STRIP_16 | // turn 16 bit into 8 bit
                   PNG_TRANSFORM_PACKING,   // expand 1,2 or 4 to 8 bit
               NULL);

  // see whether this file is good
  png_uint_32 width;
  png_uint_32 height;
  int bit_depth;
  int color_type;
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, 0, 0, 0);

  if (bit_depth != 8 || !(color_type & PNG_COLOR_TYPE_RGB)) {
    if (err_str)
      *err_str << "PNG: format of " << file_name << " is not PNG24 or PNG32" << endl;
    return;
  }

  // create the bitmap
  dst->create(width, height);
  if (!dst->ok()) {
    if (err_str)
      *err_str << "PNG: couldn't create bitmap while reading " << file_name << ", width=" << width
               << ", height=" << height << endl;
    return;
  }
  bool has_alpha = color_type & PNG_COLOR_MASK_ALPHA ? true : false;

  // copy data to our own bitmap
  png_bytepp src_rows = png_get_rows(png_ptr, info_ptr);
  for (png_uint_32 y = 0; y < height; y++) {

    // process a line
    png_bytep src_p = src_rows[y];
    if (has_alpha) {
      // PNG 32
      for (RowRGBA dst_p = dst->getRow(y); !dst_p.isEnd(); dst_p.incX()) {
        unsigned r = *src_p++, g = *src_p++, b = *src_p++, a = *src_p++;
        dst_p.set(r, g, b, a);
      }
    }
    else {
      // PNG 24
      for (RowRGBA dst_p = dst->getRow(y); !dst_p.isEnd(); dst_p.incX()) {
        unsigned r = *src_p++, g = *src_p++, b = *src_p++;
        dst_p.set(r, g, b);
      }
    }
  }

  //
  ok = true;
}

//-------------------------------------------------------------------------------------------------
bool /*success*/ ReadPng(CContextRGBA* dst, string file_name, ostream* err_str)
// outside interface, read the PNG file into the alpha context
{
  InternalReadPng read_png(dst, file_name, err_str);
  return read_png.ok;
}
