/******************************************************************************
Miscellaneous stuff

History
  Date       Version    Programmer         Comments
  16/2/03    1.0        Darrell Tam		Created

Free software!


******************************************************************************/
#ifndef _DT_MISC_STUFF_H_
#define _DT_MISC_STUFF_H_

#include <algorithm>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef _DEBUG
#  include <iostream>
#endif

#ifdef _WIN32
#  include <windows.h>
#  include <xmmintrin.h>
#else // assume MAC
#  include <Accelerate/Accelerate.h>
#  include <libkern/OSAtomic.h>
#  ifdef __ppc__

#  else
#    include "/usr/include/gcc/darwin/3.3/xmmintrin.h" // not sure why this isn't in 4.0
#  endif
#endif

#include <memory>
#include <string>
#include <valarray>
#include <vector>

// for debugging
#define VAR(x) ", " #x "=" << (x)
#define VAR_(x) #x "=" << (x)

// stream "X" into std::string "S"
#define STRSTR(S, X)                                                                               \
  {                                                                                                \
    std::ostringstream my_ostringstream;                                                           \
    my_ostringstream << X;                                                                         \
    S = my_ostringstream.str();                                                                    \
  }

// sizeof an array in elements
#define NUM_ELEMENTS(a) (sizeof(a) / sizeof(a[0]))

// get a C-array element, return default if out of bounds
#define GET_ELEMENT(ARRAY, IDX, DEFAULT)                                                           \
  (((IDX) < 0 || (IDX) >= NUM_ELEMENTS(ARRAY)) ? (DEFAULT) : (ARRAY)[IDX])

#ifdef _DEBUG

#  ifdef _WIN32
#    include <sstream>
#    define ASSERTX(e, msg)                                                                        \
      {                                                                                            \
        if (!(e)) {                                                                                \
          std::ostringstream _assertx_msg;                                                         \
          _assertx_msg << "ASSERTION: " __FILE__ ":" << std::dec << __LINE__ << ": " #e << "==0"   \
                       << msg << std::endl;                                                        \
          MessageBox(/*window*/ NULL,                                                              \
                     /*text*/ _assertx_msg.str().c_str(),                                          \
                     /*title*/ "problem",                                                          \
                     /*type*/ MB_OK);                                                              \
        }                                                                                          \
      }

#  else // mac
#    ifdef __ppc__
#      define BRK asm { trap}
#    else
#      define BRK __asm { int 3}
#    endif
#    define ASSERTX(e, msg)                                                                        \
      {                                                                                            \
        if (__builtin_expect(!(e), 0)) {                                                           \
          std::cerr << "ASSERTION: " __FILE__ ":" << std::dec << __LINE__ << ": " #e << "==0"      \
                    << msg << std::endl;                                                           \
          BRK                                                                                      \
        }                                                                                          \
      }
#  endif
#else
#  define ASSERTX(e, msg)                                                                          \
    {                                                                                              \
    }
#endif

typedef unsigned long uint32;

//------------------------------------------------------------------------------------------
inline int RndToInt(float v)
{
  return (int)floorf(v + 0.5f);
}
inline int RndToInt(double v)
{
  return (int)floor(v + 0.5);
}
template <class T> inline T Rnd(float v)
{
  return (T)(v + 0.5f);
}
template <class T> inline T Rnd(double v)
{
  return (T)(v + 0.5f);
}

//-------------------------------------------------------------------------------------------------
// int sprintNum(char* s, float v, float min_unit=0.0f, float max_unit=1e10f);

//-------------------------------------------------------------------------------------------------
template <class T>
struct BuiltinWrapper
// wrapper for a built in, handy for deriving from
{
  T val;

  operator T&() { return val; }
  operator T() const { return val; }

  BuiltinWrapper& operator=(const T& val_)
  {
    val = val_;
    return *this;
  }

  bool /*true=val changed, false=val the same*/ chkChg(const T& val_)
  {
    if (val == val_)
      return false;
    val = val_;
    return true;
  }
};

#ifdef _WIN32
//------------------------------------------------------------------------------------------
inline float /*prev value*/ InterlockedExchange(float* target, float new_value)
{
  union {
    float value_f;
    LONG value_l;
  };
  value_f = new_value;
  value_l = InterlockedExchange((LONG*)target, value_l);
  return value_f;
}

//------------------------------------------------------------------------------------------
class CriticalSectionWrapper : public CRITICAL_SECTION {
public:
  CriticalSectionWrapper() { InitializeCriticalSection(this); }
  ~CriticalSectionWrapper() { DeleteCriticalSection(this); }
  void lock() { EnterCriticalSection(this); }
  void unlock() { LeaveCriticalSection(this); }
  operator CRITICAL_SECTION*() { return this; }
};

//------------------------------------------------------------------------------------------
class ScopeCriticalSection {
public:
  CRITICAL_SECTION* cs;
  ScopeCriticalSection(CRITICAL_SECTION* cs_)
  {
    cs = cs_;
    EnterCriticalSection(cs_);
  }
  ~ScopeCriticalSection() { LeaveCriticalSection(cs); }
};

//------------------------------------------------------------------------------------------
template <class T> inline bool GetProcAddress(T& result, HMODULE module, const char* fn_name)
{
  FARPROC t = GetProcAddress(module, fn_name);
  result = (T)t;
  return t ? true : false;
}

#else // assume MAC OS X

// wrap the MAC functions to look like windows
inline long InterlockedIncrement(long* v)
{
  return (long)OSAtomicIncrement32Barrier((int32_t*)v);
}
inline long InterlockedDecrement(long* v)
{
  return (long)OSAtomicDecrement32Barrier((int32_t*)v);
}

inline int strnlen(const char* src, int max_n)
{
  const char* s = src;
  const char* s_end = src + max_n;
  while (s < s_end && *s)
    s++;
  return (int)(s - src);
}

//------------------------------------------------------------------------------------------
struct CriticalSectionWrapper
// wrap a spinlock
{
  OSSpinLock sl;
  CriticalSectionWrapper() { sl = 0; }
  void lock() { OSSpinLockLock(&sl); }
  void unlock() { OSSpinLockUnlock(&sl); }
  operator OSSpinLock*() { return &sl; }
};

//------------------------------------------------------------------------------------------
class ScopeCriticalSection {
public:
  OSSpinLock* sl;
  ScopeCriticalSection(OSSpinLock* sl_)
  {
    sl = sl_;
    OSSpinLockLock(sl_);
  }
  ~ScopeCriticalSection() { OSSpinLockUnlock(sl); }
};

#endif

//------------------------------------------------------------------------------------------
#ifdef __ppc__

// leave these alone on PPC, by default denormals are off
#  define SCOPE_NO_FP_EXCEPTIONS_OR_DENORMALS

#else // assume intel SSE
struct ScopeNoFpExceptionsOrDenormals {
  int prv_csr;

  ScopeNoFpExceptionsOrDenormals()
  {
    prv_csr = _mm_getcsr();

    int new_csr = prv_csr;

    // mask all exceptions
    new_csr |= _MM_MASK_MASK;

// turn some back on again in debug mode
#  ifdef _DEBUG
    new_csr &= ~(_MM_MASK_OVERFLOW | _MM_MASK_INVALID | _MM_MASK_DIV_ZERO);
#  endif

    // disable denormals
    new_csr |= _MM_FLUSH_ZERO_ON;

    //
    _mm_setcsr(new_csr);
  }

  ~ScopeNoFpExceptionsOrDenormals() { _mm_setcsr(prv_csr); }
};

#  define SCOPE_NO_FP_EXCEPTIONS_OR_DENORMALS                                                      \
    ScopeNoFpExceptionsOrDenormals _scope_no_exceptions_or_denormals_
#endif

//------------------------------------------------------------------------------------------
//
// return linear interpolation at point <frac> between <out_min> &
// <out_max>, where <frac> is in the range 0 to 1
//
inline float lin_interp(float frac /* 0..1 */, float out_min, float out_max)
{
  return out_min + (out_max - out_min) * frac;
}

template <class T> inline float lin_interp(float frac, const T& limit /*expect 2 element array*/)
{
  return lin_interp(frac, limit[0], limit[1]);
}

// linear scale value that is between "in_min" & "in_max" to
inline float lin_scale(float val, float in_min, float in_max, float out_min, float out_max)
{
  return lin_interp((val - in_min) / (in_max - in_min), out_min, out_max);
}

//------------------------------------------------------------------------------------------
inline float /* > 0 */ exp_interp(float frac /* 0..1 */, float out_min /* > 0 */,
                                  float out_max /* > 0 */)
// exponential interpolation between out_min & out_max
// note that the output can never be quite 0
{
  return expf(lin_interp(frac, logf(std::max(out_min, 1e-36f)), logf(std::max(out_max, 1e-36f))));
}

//------------------------------------------------------------------------------------------
// exponential interpolate
template <class T> inline float exp_interp(float frac, const T& limit /*expect 2 element array*/)
{
  return exp_interp(frac, limit[0], limit[1]);
}

//------------------------------------------------------------------------------------------
// find the sum of a vector "data"
template <class T> typename T::Type vec_sum(const T& data)
{
  typename T::Type sum = 0;
  for (int i = 0; i < data.size(); i++)
    sum += data[i];
  return sum;
}

//------------------------------------------------------------------------------------------
// vector mutliply src_vec by src_scalar, note that number of elements to process is taken from
// "src_vec"
template <class DST, class SRC>
inline void vec_mult(DST& dst_vec, const SRC& src_vec, typename SRC::Type src_scalar)
{
  for (int i = 0; i < src_vec.size(); i++)
    dst_vec[i] = src_vec[i] * src_scalar;
}

//------------------------------------------------------------------------------------------
// inplace vector multiply
template <class T> inline void vec_mult(T& /*in & out*/ vec, typename T::Type scalar)
{
  vec_mult(vec, vec, scalar);
}

//------------------------------------------------------------------------------------------
template <class T>
inline bool /*success*/ normalize_sum(T& /*in & out*/ data, typename T::Type target_sum)
{
  typedef typename T::Type Type;
  Type sum = vec_sum(data);

  // can we normalize?
  if (!sum)
    return false;

  //
  Type scale = target_sum / sum;
  vec_mult(/*in & out*/ data, scale);
  return true;
}

//------------------------------------------------------------------------------------------
template <class T> struct VecPrintStruct {
  std::vector<T> data;
  std::string separator;
};

//------------------------------------------------------------------------------------------
template <class T>
inline VecPrintStruct<typename T::Type> vec_print(const T& src, std::string separator)
{
  // unfortunately we need to copy the data rather than take a pointer in case it's
  // a temp object that goes out of scope by the time we get to print
  VecPrintStruct<typename T::Type> v;
  v.separator = separator;
  v.data.resize(src.size());
  for (int i = 0; i < src.size(); i++)
    v.data[i] = src[i];
  return v;
}

//------------------------------------------------------------------------------------------
template <class T> inline std::ostream& operator<<(std::ostream& o, const VecPrintStruct<T>& v)
{
  int last = v.data.size() - 1;
  for (int i = 0; i < last; i++)
    o << v.data[i] << v.separator;
  if (last >= 0)
    o << v.data[last];
  return o;
}
//------------------------------------------------------------------------------------------
template <class T> void vec_chg_size(T& vec, int num_elements)
{
  vec.resize(vec.size() + num_elements);
}

//------------------------------------------------------------------------------------------
// return true if "tst" is between upper & lower
template <class A, class B, class C>
inline bool within(const A& tst, const B& lower, const C& upper)
{
  return tst >= lower && tst <= upper;
}

//------------------------------------------------------------------------------------------
template <class A, class B, class C>
inline A limit_range(const A& in, const B& range_min, const C& range_max)
// limit range of "in" between "range_min" & "range_max"
{
  if (in < range_min)
    return range_min;
  if (in > range_max)
    return range_max;
  return in;
}

//------------------------------------------------------------------------------------------
template <class T> inline bool idx_within(int idx, const T& t)
{
  return within(idx, 0, (int)t.size() - 1);
}

//------------------------------------------------------------------------------------------
inline float lin_interp_limit(float frac, float out_min, float out_max)
{
  return limit_range(lin_interp(frac, out_min, out_max), out_min, out_max);
}

//------------------------------------------------------------------------------------------
template <class I, class X> inline I wrap(I i, const X& x)
// wrap index "i" inside of "x" that contains the method "size()" returning the number of
// elements
{
  I n = x.size();
  i %= n;
  if (i < 0)
    i += n;
  return i;
}

//------------------------------------------------------------------------------------------
inline long prbs32(long x)
{
  // maximal length, taps 32 31 29 1, from wikipedia
  return ((unsigned long)x >> 1) ^ (-(x & 1) & 0xd0000001UL);
}

//------------------------------------------------------------------------------------------
// these are a bit impolite but make things easy
template <class T> T* begin(const std::vector<T>& v)
{
  return &(const_cast<std::vector<T>&>(v)[0]);
}
template <class T> T* begin(const std::valarray<T>& v)
{
  return &(const_cast<std::valarray<T>&>(v)[0]);
}

//------------------------------------------------------------------------------------------
template <class T, class BASE = T>
struct new_auto_ptr
    : public std::auto_ptr<BASE>
// wrapper for auto_ptr that is initialized with a new instance of "T"
// i.e. instead of writing this:
//     auto_ptr<MyClass> my_class = new MyClass;
// use:
//     new_auto_ptr<MyClass> my_class;

{
  new_auto_ptr()
      : std::auto_ptr<BASE>(new T)
  {
  }
};

//------------------------------------------------------------------------------------------
// allow an auto_ptr to be set to a ptr
template <class A, class B> inline void set(std::auto_ptr<A>& a, B* b)
{
  a = std::auto_ptr<A>(b);
}

//------------------------------------------------------------------------------------------
template <class T>
struct _PtrBase
//
{
  // dereferenced type
  typedef T Type;

  T* ptr;

  _PtrBase() { ptr = NULL; }

  // steal a pointer to transfer ownership from controlled pointers (e.g. Ref<>)
  T* Steal()
  {
    T* r = ptr;
    ptr = NULL;
    return r;
  }

  // auto cast to compatible ptr
  operator T*() const { return ptr; }

  // dereference operator
  const T& operator*() const { return *ptr; }
  T& operator*() { return *ptr; }

  // pointing at operator
  // const T* operator -> () const { return ptr; }
  T* operator->() const { return ptr; }

  // cast to another type
  template <class T1> T1* cast() { return (T1*)ptr; }

  // address-of-pointer returns pointer to ptr (don't do this - it messes up stuff that
  // wants pointer to this class or derived classes)
  // T** operator& () { return &ptr; }

  _PtrBase& ForceAssign(void* ptr_)
  {
    ptr = (T*)ptr_;
    return *this;
  }
};

//------------------------------------------------------------------------------------------
template <class T>
struct _Ptr
    : public _PtrBase<T>
//
// pointer wrapper template
// pointer automatically set to NULL on creation
// Delete/DeleteArray methods clear the pointer
//
// Works like a normal C++ pointer in that there is no concept of object ownership
//
{
  // allow explicit construct from void ptr
  explicit _Ptr(void* void_ptr) { _PtrBase<T>::ptr = (T*)void_ptr; }

  _Ptr(T* ptr_ = NULL) { _PtrBase<T>::ptr = ptr_; }

  // auto cast to compatible ptr
  template <class T2> operator _Ptr<T2>() const { return (_Ptr<T2>(_PtrBase<T>::ptr)); }

  // New() can only use default constructor
  _Ptr& New()
  {
    _PtrBase<T>::ptr = new T;
    return *this;
  }

  // delete and clear pointer
  void Delete()
  {
    delete _PtrBase<T>::ptr;
    _PtrBase<T>::ptr = NULL;
  }

  // new & delete arrays (when calling, careful not to mix with New()/Delete() above)
  void NewArray(unsigned i) { _PtrBase<T>::ptr = new T[i]; }
  void DeleteArray()
  {
    delete[] _PtrBase<T>::ptr;
    _PtrBase<T>::ptr = NULL;
  }

  // need to define these ones
  _Ptr& operator++()
  {
    _PtrBase<T>::ptr++;
    return this;
  } // prefix
  _Ptr& operator--()
  {
    _PtrBase<T>::ptr--;
    return this;
  }                                                   // prefix
  _Ptr operator++(int) { return _PtrBase<T>::ptr++; } // postfix
  _Ptr operator--(int) { return _PtrBase<T>::ptr--; } // postfix

  // don't seem to need these ones
  //_Ptr& operator += (int n) { _PtrBase<T>::ptr += n; return this; }
  //_Ptr& operator -= (int n) { _PtrBase<T>::ptr -= n; return this; }
};

struct UCharRng;

//------------------------------------------------------------------------------------------
class Null {
public:
  template <class T> operator T*() { return NULL; }
};

//------------------------------------------------------------------------------------------
template <class T>
class Rng
    : public _PtrBase<T>
// range is a pointer with length
{
public:
  typedef _PtrBase<T> base;

  int n; // length of range that we're pointing at

  Rng() { n = 0; }
  Rng(const Null&)
  {
    base::ptr = NULL;
    n = 0;
  }
  Rng(T* ptr_, int n_ = 1)
  {
    base::ptr = ptr_;
    n = n_;
  }
  Rng(T* ptr_, int offs_start, int offs_last /*inclusive*/)
  {
    base::ptr = ptr_ + offs_start;
    n = offs_last - offs_start + 1;
  }
  Rng(T* first, T* last /*inclusive*/)
  {
    base::ptr = first;
    n = last - first + 1;
  }

  // attempt to construct using toRng() conversion (for some reason we need to explicitly call
  // conversion)
  template <class T2> Rng(T2& src) { *this = toRng(src).operator Rng<T>(); }

  void adjStart(int i)
  {
    n -= i;
    base::ptr += i;
  }
  void adjLen(int i) { n += i; }

  // keep the end constant and set the length
  void setLenFromEnd(int len)
  {
    base::ptr = base::ptr + n - len;
    n = len;
  }

  int size() const { return n; }
  int sizeBytes() const { return n * sizeof(T); }

  T* begin() const { return base::ptr; }
  T* end() const { return n + base::ptr; }
  T* last() const { return end() - 1; }

  // get limits range
  const T& get(int i) const { return base::ptr[limit_range(i, 0, n - 1)]; }

  // get that returns "out_of_bounds" if "i" out of bounds
  const T& get(int i, const T& out_of_bounds) const
  {
    return idx_within(i, *this) ? data[i] : out_of_bounds;
  }

  // cast to compatible types
  template <class T2> operator Rng<T2>() const { return Rng<T2>(base::ptr, n); }
  template <class T2> operator _Ptr<T2>() const { return base::ptr; }

  void copyTo(void* dst) const { memcpy(dst, base::ptr, sizeBytes()); }
  void copyFrom(const void* src) { memcpy(base::ptr, src, sizeBytes()); }

  UCharRng toUCharRng() const;
};

// some things that we can cast to rng
template <class T> inline Rng<T> toRng(Rng<T>& r)
{
  return Rng<T>(r.ptr, r.n);
}
template <class T> inline Rng<T> toRng(T* ptr, int n = 1)
{
  return Rng<T>(ptr, n);
}
template <class T> inline Rng<T> toRng(std::vector<T>& v)
{
  return Rng<T>(&v[0], (int)v.size());
}
template <class T> inline Rng<T> toRng(std::valarray<T>& v)
{
  return Rng<T>(&v[0], (int)v.size());
}
// return true if the 0 <= idx < rng.size()
template <class T> bool withinRng(int idx, const T& rng)
{
  return idx >= 0 && idx < toRng(const_cast<T&>(rng)).size();
}

// turn an array into a range
#define TO_RNG(c_array) toRng(c_array, NUM_ELEMENTS(c_array))

//-------------------------------------------------------------------------------------------------
struct UCharRng
    : public Rng<unsigned char>
// Anything that can be cast to a Rng<> can also be cast to an unsigned char range
{
  typedef Rng<unsigned char> base;

  // anything that can be converted to a range can be converted to a UCharRng
  template <class T> UCharRng(T& src);

  // construct from a pointer
  template <class T>
  UCharRng(T* src, int n_items = 1)
      : base((unsigned char*)src, sizeof(T) * n_items)
  {
  }

  UCharRng(void* src, int n_bytes)
      : base((unsigned char*)src, n_bytes)
  {
  }
};

//-------------------------------------------------------------------------------------------------
struct VoidPtr
// void ptr wrapper that can be constructed from anything that can be converted to a Rng<>
{
  void* ptr;

  //
  VoidPtr(void* ptr_ = NULL) { ptr = ptr_; }

  // anything that can be converted to a range can be converted to a void ptr
  template <class T> VoidPtr(T& src) { ptr = toRng(src).ptr; }

  operator void*() { return ptr; }
  operator const void*() const { return ptr; }
};

//-------------------------------------------------------------------------------------------------
template <class T> inline UCharRng Rng<T>::toUCharRng() const
{
  return UCharRng((unsigned char*)base::ptr, sizeBytes());
}

template <class T>
inline UCharRng::UCharRng(T& src)
    : Rng<unsigned char>(toRng(src).toUCharRng())
{
}

// memcpy of ranges
inline void CopyFillDst(UCharRng dst, const VoidPtr& src)
{
  dst.copyFrom(src);
}
inline void CopyAllSrc(VoidPtr dst, UCharRng src)
{
  src.copyTo(dst);
}

// set all bytes to "value"
inline void SetBytes(UCharRng dst, unsigned char value)
{
  memset(dst.ptr, value, dst.n);
}

// do a bytewise clear
template <class T> inline void Clear(T* dst, int n)
{
  memset(dst, 0, n * sizeof(T));
}
inline void Clear(UCharRng dst)
{
  SetBytes(dst, 0);
}

// array copy
template <class T> inline void Copy(T* dst, const T* src, int n_items)
{
  memcpy(dst, src, n_items * sizeof(T));
}

// set all values in something that can be converted to a range
template <class A, class B> inline void SetAllInRng_(Rng<A> rng, const B& value)
{
  for (int i = 0; i < rng.n; i++)
    rng[i] = value;
}
template <class A, class B> inline void SetAllInRng(A& rng, const B& value)
{
  SetAllInRng_(toRng(rng), value);
}

// return true if memory overlaps
inline bool /*true=overlap*/ ChkMemOverlap(UCharRng a, UCharRng b)
{
  return a.ptr >= b.ptr && a.ptr <= b.last() || a.last() >= b.ptr && a.last() <= b.ptr;
}

//-------------------------------------------------------------------------------------------------
inline bool ShiftDst(Rng<unsigned char>* /*inout*/ dst, UCharRng src)
// copy "src" to the start of "dst" & move dst pointer forward (shift dst ptr)
{
  // check whether the item can fit
  if (dst->n < src.n)
    return false;

  // copy the item
  CopyAllSrc(*dst, src);

  // move forward in the dest buffer
  dst->adjStart(src.n);

  // all ok
  return true;
}

//-------------------------------------------------------------------------------------------------
inline bool ShiftSrc(UCharRng dst, Rng<unsigned char>* /*inout*/ src)
// copy src to dst and move the src pointer forward (shift src ptr)
{
  // check whether there's enough data to fill dst
  if (dst.n > src->n)
    return false;

  // copy the item
  CopyFillDst(dst, *src);

  // move forward in the src buffer
  src->adjStart(dst.n);

  // all ok
  return true;
}

//-------------------------------------------------------------------------------------------------
inline void MaybeSwap32(void* /*4-byte*/ dst, void* /*4-byte*/ src)
//
// swap byte order on big-endian machines (ppc), otherwise 4-byte copy
{
#ifdef __ppc__
  // do byte swapping
  _Ptr<unsigned char> dstc(dst), srcc(src);
  dstc[0] = srcc[3];
  dstc[1] = srcc[2];
  dstc[2] = srcc[1];
  dstc[3] = srcc[0];
#else
  // little-endian (assume non word aligned is ok)
  *(unsigned long*)dst = *(unsigned long*)src;
#endif
}

//-------------------------------------------------------------------------------------------------
inline void MaybeSwap16(void* /*2-byte*/ dst, void* /*2-byte*/ src)
// only swap byte order on big-endian machines (ppc)
{
#ifdef __ppc__
  // do byte swapping
  _Ptr<unsigned char> dstc(dst), srcc(src);
  dstc[0] = srcc[1];
  dstc[1] = srcc[0];
#else
  *(unsigned short*)dst = *(unsigned short*)src;
#endif
}

//-------------------------------------------------------------------------------------------------
struct LittleEndianMemStr
    : public UCharRng
//
// Access a buffer containing data in little endian format
// data is shifted to the start of the buffer
//
{
  template <class T>
  LittleEndianMemStr(T& t)
      : UCharRng(t)
  {
  }
  LittleEndianMemStr(unsigned char* mem, int n_bytes)
      : UCharRng(mem, n_bytes)
  {
  }
  explicit LittleEndianMemStr(void* mem, int n_bytes)
      : UCharRng(mem, n_bytes)
  {
  }

  template <class T> bool put32(T src)
  {
    // NOTE: if there's a compile error here then "dst" does not point to a 4 byte quantity
    typedef char chk_type[sizeof(T) == 4 ? 1 : -1];

    unsigned long v;
    MaybeSwap32(&v, &src);
    return ShiftDst(/*dst*/ this, /*src*/ &v);
  }

  template <class T> bool get32(T* /*must be 4 byte*/ dst)
  {
    // NOTE: if there's a compile error here then "dst" does not point to a 4 byte quantity
    typedef char chk_type[sizeof(T) == 4 ? 1 : -1];

    unsigned long v;
    if (!ShiftSrc(/*dst*/ &v, /*src*/ this))
      return false;
    MaybeSwap32(dst, &v);
    return true;
  }

  template <class T> bool put16(T src)
  {
    // NOTE: if there's a compile error here then "dst" does not point to a 2 byte value
    typedef char chk_type[sizeof(T) == 2 ? 1 : -1];

    unsigned short v;
    MaybeSwap16(&v, &src);
    return ShiftDst(/*dst*/ this, /*src*/ &v);
  }

  template <class T> bool get16(T* /*must be 4 byte*/ dst)
  {
    // NOTE: if there's a compile error here then "dst" does not point to a 2 byte value
    typedef char chk_type[sizeof(T) == 2 ? 1 : -1];

    unsigned short v;
    if (!ShiftSrc(/*dst*/ &v, /*src*/ this))
      return false;
    MaybeSwap16(dst, &v);
    return true;
  }
};

//-------------------------------------------------------------------------------------------------
inline Rng<char> v_rng_sprf(Rng<char> dst, const char* fmt, const va_list args)
//
// wrapper for vsnprintf to print into a range & return remaining
//
{
  // not enough room? (there has to be space for zero termination)
  if (dst.n < 1)
    return dst;

//
#ifdef _WIN32
  int n = _vsnprintf_s(dst, dst.n, _TRUNCATE, fmt, const_cast<va_list>(args));

  // check for truncation
  if (n < 0) {
    n = dst.n - 1;
    dst[n] = 0;
  }
#else
  int n = vsnprintf(dst, dst.n, fmt, const_cast<va_list>(args));

  // check for truncation
  if (n > dst.n - 1)
    n = dst.n - 1;
#endif

  dst.adjStart(n);

  // positive means success so consume characters from dest buffer
  return dst;
}

//-------------------------------------------------------------------------------------------------
inline Rng<char> rng_sprf(Rng<char> dst, const char* fmt, ...)
// rng print is sprintf to a range
// printf to a char rng (or anything that can be converted) using "vsprintf_s"
// return remaining rng after writing to "dst"
{
  va_list args;
  va_start(args, fmt);
  return v_rng_sprf(dst, fmt, args);
}

//-------------------------------------------------------------------------------------------------
class RefCountObj
//
// Reference counting object to use with Ref<> template
// Always inherit reference counted objects from "RefCountObj" as virtual
//
{
public:
  mutable long _ref_count;
  RefCountObj(long ref_count = 0) { _ref_count = ref_count; }

  void incRefCount() const
  {
    if (_ref_count >= 0)
      InterlockedIncrement(&_ref_count);
  }

  bool /*true=object should NOT be deleted*/ decRefCount() const
  {
    if (_ref_count < 0)
      return true;
    return InterlockedDecrement(&_ref_count) != 0;
  }
};

//-------------------------------------------------------------------------------------------------
template <class T>
class Ref
    : public _PtrBase<T>
//
// reference counting template
//
{
public:
  Ref(T* x = 0)
  {
    _PtrBase<T>::ptr = 0;
    set(x);
  }
  Ref(const Ref& src)
  {
    _PtrBase<T>::ptr = 0;
    set(src);
  }
  ~Ref() { set(0); }

  void set(T* new_ptr)
  {
    T* old_ptr = _PtrBase<T>::ptr;
    _PtrBase<T>::ptr = new_ptr;

    // incr new object first in case new & old are the same
    if (new_ptr)
      new_ptr->incRefCount();
    if (old_ptr && !old_ptr->decRefCount())
      delete old_ptr;
  }

  // auto cast to compatible reference
  template <class T2> operator Ref<T2>() const { return (Ref<T2>(_PtrBase<T>::ptr)); }

  // assignment operators
  Ref& operator=(T* src)
  {
    set(src);
    return *this;
  }
  Ref& operator=(const Ref& src)
  {
    set(src);
    return *this;
  }

protected:
  // we're not pointing at an array, don't call this
  T& operator[](int) const { throw 0; }
};

//------------------------------------------------------------------------------------------
template <class T, int _SIZE>
struct Array
// fixed length C array wrapper,
// advantage over normal declaration is that is assignment is allowed
{
  // element type
  typedef T Type;

  // number of elements
  enum { SIZE = _SIZE };
  static int size() { return SIZE; }

  // data store
  T data[SIZE];

  // force deep-copy from a pointer
  void copyFrom(const T* p) { *this = *(const Array*)p; }

  // array operator
  // const T& operator[] (int i) const { return data[i]; }
  // T& operator[] (int i) { return data[i]; }

  // get array element with saturation
  const T& get(int i) const { return data[limit_range(i, 0, SIZE - 1)]; }

  // get that returns "out_of_bounds" if "i" out of bounds
  const T& get(int i, const T& out_of_bounds) const
  {
    return idx_within(i, *this) ? data[i] : out_of_bounds;
  }

  T* begin() { return data; }
  T* end() { return data + SIZE; }
  T* last() { return end() - 1; }

  // cast to a range
  Rng<T> rng() { return toRng(data, size()); }
  operator Rng<T>() { return rng(); }

  // cast to pointer
  operator T*() { return data; }
  operator const T*() const { return data; }

  // cast to our kind of ptr
  operator _Ptr<T>() { return data; }
  operator _Ptr<const T>() const { return data; }
};

template <class T, int N> inline Rng<T> toRng(Array<T, N>& src)
{
  return src.rng();
}

template <class T, int N> inline bool operator==(const Array<T, N>& a, const Array<T, N>& b)
{
  for (int i = 0; i < N; i++)
    if (a[i] != b[i])
      return false;
  return true;
}

template <class T, int N> inline bool operator!=(const Array<T, N>& a, const Array<T, N>& b)
{
  return !operator==(a, b);
}

//------------------------------------------------------------------------------------------
// CharArray zeros first element on construction & gives some string things
template <int LENGTH> class CharArray : public Array<char, LENGTH> {
public:
  typedef Array<char, LENGTH> base;

  // ensure NULL termination
  void terminate()
  {
    if (LENGTH > 0)
      base::data[LENGTH - 1] = 0;
  }

  // empty
  CharArray() { base::data[0] = 0; }

  // copy "src" to until 0 termination & leaves CharArray 0 terminated
  CharArray(Rng<char> src /*0 terminated*/)
  {
    int n = std::min(LENGTH - 1, src.n);
    char* dst = base::data;
    while (n > 0 && *src.ptr) {
      *dst++ = *src.ptr++;
      n--;
    }

    // zero terminate
    *dst = 0;
  }

  // copies at most LENGTH-1 characters & leaves CharArray 0 terminated
  CharArray(const char* src /*0 terminated*/)
  {
    int i = 0;

    if (src) {
      // only copy if src is non-NULL
      while (i < LENGTH - 1 && *src)
        base::data[i++] = *src++;
    }

    // zero terminate
    base::data[min(i, LENGTH - 1)] = 0;
  }

  // legnth of string
  int strlen() { return (int)strnlen(base::data, LENGTH); }

  // concat "src" to the end of string
  void strcat(const char* src)
  {
    strncat(base::data, src, LENGTH);
    terminate();
  }

  // return from the (first) 0 to the end of the array - use to stream append to
  // a string: CharArray<32> t = "Hi there"; t.unused() << " everyone";
  Rng<char> unused()
  {
    int n = strlen();
    return toRng(base::data + n, LENGTH - n);
  }

  // can cast to a string - needed this one for things like std::vector<string>::push_back()
  // wouldn't auto-cast via char* operator
  operator std::string() const { return base::data; }
  std::string toString() const { return base::data; }
};
template <int N> inline Rng<char> toRng(CharArray<N>& src)
{
  return src.rng();
}

template <int LENGTH> bool operator==(const CharArray<LENGTH>& a, const char* b)
{
  return strncmp(a, b, LENGTH) == 0;
}
template <int LENGTH> bool operator==(const char* a, const CharArray<LENGTH>& b)
{
  return strncmp(a, b, LENGTH) == 0;
}
template <int LEN1, int LEN2> bool operator==(const CharArray<LEN1>& a, const CharArray<LEN2>& b)
{
  return strncmp(a, b, std::min(LEN1, LEN2)) == 0;
}

//------------------------------------------------------------------------------------------
template <int LENGTH> struct SprfCharArray : public CharArray<LENGTH> {
  SprfCharArray(const char* fmt, ...)
  {
    va_list args;
    va_start(args, fmt);

//
#ifdef _WIN32
    _vsnprintf_s(data, LENGTH, _TRUNCATE, fmt, args);
#else
    vsnprintf(this->data, LENGTH, fmt, args);
#endif
  }
};

//------------------------------------------------------------------------------------------
template <class VAL, class STP = VAL>
struct ValStp
    : public BuiltinWrapper<VAL>
// structure to hold a value and a step
{
  // base class
  typedef BuiltinWrapper<VAL> base;

  STP stp;
  ValStp(VAL val_ = 0, STP stp_ = 0)
  {
    base::val = val_;
    stp = stp_;
  }
  ValStp& operator=(const VAL& val_) { base::val = val_; }

  ValStp& next(STP i)
  {
    base::val += stp * i;
    return *this;
  }
  ValStp& prev(STP i)
  {
    base::val -= stp * i;
    return *this;
  }
  ValStp& next()
  {
    base::val += stp;
    return *this;
  }
  ValStp& prev()
  {
    base::val -= stp;
    return *this;
  }

  ValStp& set(VAL val_, STP stp_)
  {
    base::val = val_;
    stp = stp_;
    return *this;
  }
  ValStp& operator()(VAL val_, STP stp_) { return set(val_, stp_); }
};

//------------------------------------------------------------------------------------------
template <class T> struct FindMinMax : public Array<T, 2> {
  typedef Array<T, 2> base;
  T& min() { return base::data[0]; }
  T& max() { return base::data[1]; }
  T min() const { return base::data[0]; }
  T max() const { return base::data[1]; }

  FindMinMax(T min_v, // choose something big initially
             T max_v  // choose something small
  )
  {
    base::data[0] = min_v;
    base::data[1] = max_v;
  }

  // update min & max
  void operator()(T v)
  {
    if (v < base::data[0])
      base::data[0] = v;
    if (v > base::data[1])
      base::data[1] = v;
  }
};

#ifdef _OSTREAM_
template <class T> inline std::ostream& operator<<(std::ostream& o, FindMinMax<T> mm)
{
  return o << mm.min() << ":" << mm.max();
}
#endif

//*************************************************************************************************
template <int _N>
struct SplitParam
//
// split a param (range 0..1) into an integer part (range 0..N-1) and a fraction (range 0..1)
// N: number of integer parts to split into
//
{
  enum { N = _N };

  int i_part;   // 0 .. N-1
  float f_part; // 0 .. 0.9999f

  SplitParam() {}

  // combined param
  SplitParam(float /*0..1*/ param) { operator()(param); }

  // construct with 2 parts
  SplitParam(int i, float /*0..1*/ f) { operator()(i, f); }

  // set parts
  void operator()(int i, float f)
  {
    i_part = i;
    f_part = f;
  }

  //
  void operator()(float /*0..1*/ param)
  // divide the value into 2 parts
  {
    // special case
    if (param >= 1.0f) {
      i_part = N - 1;
      f_part = 1.0f;
      return;
    }
    // normal case
    float i;
    param *= (float)N;
    f_part = modff(param, &i);
    i_part = limit_range((int)i, 0, N - 1);
  }

  operator float()
  // combine 2 params
  {
    float r = limit_range((float)i_part, 0.0f, (float)(N - 1));
    r += limit_range(f_part, 0.0f, 0.9999f);
    return r * (1.0f / (float)N);
  }
};

//*************************************************************************************************
// return true if the pointer exists and it contains a method "ok" that returns true
template <class PTR> inline bool isOk(PTR ptr)
{
  return ptr && ptr->ok();
}

//*************************************************************************************************
template <class A, class B>
inline bool /*true=was different, false=was same*/
SetIfDifferent(A& a, const B& b)
// set a = b if different and return true
{
  if (a == b)
    return false;
  a = b;
  return true;
}

struct ChkChg;
//-------------------------------------------------------------------------------------------------
template <class DST> struct ChkChgPtr {
  DST* ptr;
  ChkChg* chg;
  template <class SRC> bool /*has changed?*/ operator=(const SRC& src);
};

//-------------------------------------------------------------------------------------------------
struct ChkChg
    : public BuiltinWrapper<bool>
// wrapper to record whether a value was changed
{
  ChkChg(bool val_ = false) { val = val_; }

  // perform dst = src & set internal val to true if they were different
  template <class DST, class SRC> bool operator()(DST& dst, const SRC& src)
  {
    if (dst == src)
      return false;
    dst = src;
    *this = true;
    return true;
  }

  // another way is to use this and assignment
  template <class DST> ChkChgPtr<DST> operator()(DST& dst)
  {
    ChkChgPtr<DST> cc;
    cc.ptr = &dst;
    cc.chg = this;
    return cc;
  }

  // constructor
  template <class A, class B> ChkChg(A& a, const B& b)
  {
    *this = false;
    operator()(a, b);
  }

  // return true if value has changed since last call to "hasChanged()" & reset "changed"
  bool tstAndSet(bool val_ = true)
  {
    bool r = val;
    val = val_;
    return r;
  }

  bool tstAndClr() { return tstAndSet(false); }
};

template <class DST> template <class SRC> inline bool ChkChgPtr<DST>::operator=(const SRC& src)
{
  return (*chg)(*ptr, src);
};

//-------------------------------------------------------------------------------------------------
template <class T>
struct ValueChange
//
// hold a value and remember whether it has changed since last call to "hasChanged()"
//
{
  // value we hold
  T val;

  // whether value has changed since last call to "hasChanged()"
  ChkChg chg;

  // default: assume that we start off with a changed value
  ValueChange(bool initial_chg = true) { chg = initial_chg; }

  // construct with value & change state
  ValueChange(const T& initial_val, bool initial_chg)
  {
    val = initial_val;
    chg = initial_chg;
  }

  // auto-cast & return copy of value
  operator T() const { return val; }

  // set value & return whether new value is different from previous
  bool /*true=value changed, false=no change*/ set(const T& v) { return chg(&val, v); }

  // assignment works
  ValueChange& operator=(const T& v)
  {
    set(v);
    return *this;
  }

  // force set
  void set(const T& v, bool force_chg)
  {
    val = v;
    chg = force_chg;
  }

  void forceChanged(bool state = true) { chg = state; }

  // peek at the changed without resetting
  bool peekChanged() const { return chg; }

  // return true if value has changed since last call to "hasChanged()" & reset "changed"
  bool hasChanged() { return chg.tstAndClr(); }
};

//-------------------------------------------------------------------------------------------------
struct FindClosestArrayElement
//
// Find closest point on an x-y plot of a 1-d array
//
{
  // closest index found
  int best_i;

  // norm value corresponding to best_i
  float best_norm;

  void init()
  {
    best_i = -1;       // nothing found yet
    best_norm = 1e20f; // big number
  }

  //
  FindClosestArrayElement() { init(); }

  // see "find" below
  template <class ARRAY>
  FindClosestArrayElement(ARRAY& values, float /*0..1*/ approx_idxf, float approx_value,
                          float aspect_ratio)
  {
    init();
    find(values, approx_idxf, approx_value, aspect_ratio);
  }

  // go through all points in the array to find closest (in the euclidean sense) to
  // the approx idx & value supplied
  template <class ARRAY>
  int find(ARRAY& values,              // array of values to search over
           float /*0..1*/ approx_idxf, // vertical: y axis, horizontal: x axis
           float approx_value,         // vertical: x axis, horizontal: y axis
           float aspect_ratio          // vertical: height/width, horizontal: width/height
  )
  {
    int n = values.size();
    ValStp<float> idxf;

    if (n > 1)
      idxf.stp = aspect_ratio / ((float)n - 1);

    approx_idxf *= aspect_ratio;

    for (int i = 0; i < n; i++) {
      float d_val = approx_value - values[i];
      float d_idx = approx_idxf - idxf;
      float norm = d_val * d_val + d_idx * d_idx;
      if (norm < best_norm) {
        best_norm = norm;
        best_i = i;
      }
      idxf.next();
    }
    return best_i;
  }

  operator int() { return best_i; }
};

//-------------------------------------------------------------------------------------------------
struct FracRescale
//
// rescale a frac (0..1) value to a new range with offset
{
  // effective length in rescaled units (pixels), actually width-1
  float eff_len;

  // offset in rescaled units (pixels)
  float offset;

  // init with rescaled length (so that 0..rescale_length-1 corresponds to 0..1)
  void init(float rescaled_length, float magnify = 1.0f)
  {
    eff_len = magnify * std::max(rescaled_length - 1, 1.0f);
  }

  // adjust offset so that "frac" corresponds to "rescaled" (pixel)
  void setOffs(float /*0..1*/ frac, float rescaled_position)
  {
    offset = rescaled_position - eff_len * frac;
  }

  // set offset directly
  void setOffs(float rescaled_position) { offset = rescaled_position; }

  // get value given pixel
  float /*0..1*/ getFrac(float rescaled) const
  {
    return limit_range((rescaled - offset) / eff_len, 0.0f, 1.0f);
  }

  // get pixel given value
  float /*pixel*/ getRescaled(float /*0..1*/ frac) const { return frac * eff_len + offset; }

  float /*pixel*/ getLeft() { return offset; }
  float /*pixel*/ getRight() { return offset + eff_len; }
};

//-------------------------------------------------------------------------------------------------
struct CharRng
    : public Rng<char>
//
// wrapper for char ranges to force compiler to be more restrictive on conversion
// so that << functions don't interfere with normal ostreams
//
{
  CharRng(Rng<char> src)
      : Rng<char>(src)
  {
  }
  CharRng(std::vector<char>& src)
      : Rng<char>(src)
  {
  }
  CharRng(std::valarray<char>& src)
      : Rng<char>(src)
  {
  }
  CharRng(char* src, int len)
      : Rng<char>(src, len)
  {
  }
  template <int N>
  CharRng(Array<char, N>& src)
      : Rng<char>(src)
  {
  }
};

//
// Functions that allow you to efficiently "stream" into a CharArray or Rng<char> (CharRng) or
// anything that can be converted to one of these (e.g. toRng(std::vector<char>)
//
// e.g.
// CharArray<1024> str;
// str << "Hi there " << 1234;
//
// Note that the semantics are slightly different to normal io streams (or string streams)
// in that data is always written to the start of the string

//-------------------------------------------------------------------------------------------------
inline CharRng operator<<(CharRng dst, const char* s /*zero terminated*/)
{
  // leave room for 0 termination
  dst.n--;

  // copy string
  while (dst.n > 0 && *s) {
    *dst.ptr++ = *s++;
    dst.n--;
  }

  // correct length
  dst.n++;

  // NULL terminate if possible
  if (dst.n > 0)
    dst[0] = 0;
  return dst;
}

//-------------------------------------------------------------------------------------------------
inline CharRng operator<<(CharRng dst, Rng<const char> src /*zero terminated*/)
{
  // find the length before copy if there's overlap
  if (ChkMemOverlap(dst, src))
    src.n = strnlen(src, src.n);

  // copy string
  int n = std::min(dst.n - 1, src.n), n_start = n;
  while (n > 0 && *src.ptr) {
    *dst.ptr++ = *src.ptr++;
    n--;
  }

  // correct length
  dst.n -= n_start - n;

  // NULL terminate if possible
  if (dst.n > 0)
    dst[0] = 0;
  return dst;
}

// make this one explicit otherwise compiler can't choose between Rng & const char*
template <int N> inline CharRng operator<<(CharRng dst, const Array<char, N>& src)
{
  return dst << Rng<const char>(src.data, src.size());
}

// can stream a string
inline CharRng operator<<(CharRng dst, const std::string& src)
{
  return dst << src.c_str();
}

//-------------------------------------------------------------------------------------------------
struct RngPrintNumStruct
// used for "sprnum"
{
  float v, min_unit, max_unit;
  CharRng print(Rng<char> dst) const;
};

//-------------------------------------------------------------------------------------------------
inline RngPrintNumStruct sprnum(float v, float min_unit = 0.0f, float max_unit = 1e10f)
//
// print a number with units u=micro (1e-6), m=milli (1e-3), k=kilo (1e3)
{
  RngPrintNumStruct r = {v, min_unit, max_unit};
  return r;
}

//
inline CharRng operator<<(CharRng dst, const RngPrintNumStruct& s)
{
  return s.print(dst);
}

//-------------------------------------------------------------------------------------------------
inline CharRng rngPrintNum(Rng<char> dst, float v, float min_unit = 0.0f, float max_unit = 1e10f)
{
  return dst << sprnum(v, min_unit, max_unit);
}

//
#include "sprf.h"

//
inline CharRng operator<<(CharRng dst, int n)
{
  return dst << sprf("%d", n);
}
inline CharRng operator<<(CharRng dst, unsigned int n)
{
  return dst << sprf("%u", n);
}
inline CharRng operator<<(CharRng dst, float n)
{
  return dst << sprf("%f", n);
}
inline CharRng operator<<(CharRng dst, double n)
{
  return dst << sprf("%lf", n);
}

//-------------------------------------------------------------------------------------------------
//
//   CharArray<32> str;
//   str << spr_percent(50.0f);
//   // str contains now "50%"
//
struct PercentStruct {
  float frac;
  int decimal_places;
};

inline PercentStruct spr_percent(float /*0..1*/ frac, int decimal_places = 0)
{
  PercentStruct r = {frac, decimal_places};
  return r;
}

CharRng operator<<(CharRng dst, PercentStruct s);

#endif
