#ifndef _DT_CPLXF_H_
#define _DT_CPLXF_H_
//P:$MAX_CHANNELS = 4;

#include <complex>
#include "misc_stuff.h"
#include "VecSlice.h"

//-------------------------------------------------------------------------------------------------
struct cplxf
// adjacent complex
//
// I wrote this class because std::complex didn't optimize very well in vc6 (when I checked the
// asm code it had rubbish that wouldn't optimize away)
//
{
	// element 0 is real, element 1 is data
	float data[2];

	cplxf() {}
	cplxf(const float& r, const float& i = 0.0f) { data[0] = r; data[1] = i; }
	cplxf operator () (const float& r, const float& i) { data[0] = r; data[1] = i; return *this; }
	cplxf(const std::complex<float>& src) { data[0] = src.real(); data[1] = src.imag(); }

	float real() const { return data[0]; }
	float imag() const { return data[1]; }

	float& real() { return data[0]; }
	float& imag() { return data[1]; }

	float& setReal(const float& r) { return data[0] = r; }
	float& setImag(const float& i) { return data[1] = i; }

	// auto-cast to std::complex
	operator std::complex<float> () { return std::complex<float>(data[0], data[1]); }

	cplxf operator*(const cplxf& b) const { return cplxf(data[0]*b.data[0]-data[1]*b.data[1], data[0]*b.data[1]+data[1]*b.data[0]); } 
	cplxf operator+(const cplxf& b) const { return cplxf(data[0]+b.data[0], data[1]+b.data[1]); } 
	cplxf operator-(const cplxf& b) const { return cplxf(data[0]-b.data[0], data[1]-b.data[1]); } 
	cplxf operator*(const float& b) const { return cplxf(data[0]*b, data[1]*b); }
	cplxf operator+(const float& b) const { return cplxf(data[0]+b, data[1]); }
	cplxf operator-(const float& b) const { return cplxf(data[0]-b, data[1]); } 
	cplxf operator-() const { return cplxf(-data[0], -data[1]); } 
	bool operator==(const cplxf& b) const { return data[0] == b.data[0] && data[1] == b.data[1]; } 
	bool operator!=(const cplxf& b) const { return data[0] != b.data[0] || data[1] != b.data[1]; } 

	template <class T> cplxf& operator += (const T& b) { return *this = *this+b; }
	template <class T> cplxf& operator -= (const T& b) { return *this = *this-b; }
	template <class T> cplxf& operator *= (const T& b) { return *this = *this*b; }
};


inline cplxf operator-(const float& a, const cplxf& b) { return cplxf(a-b.data[0], a-b.data[1]); } 
inline cplxf operator*(const float& a, const cplxf& b) { return b*a; } 
inline cplxf operator+(const float& a, const cplxf& b) { return b+a; } 


inline float norm(const cplxf& a) { return a.real()*a.real()+a.imag()*a.imag(); } 
inline float angle(const cplxf& a) { return atan2f(a.imag(), a.real()); }
inline float abs(const cplxf& a) { return sqrtf(norm(a)); }

inline cplxf polar_to_cplxf(const float& mag, const float& ang) { return mag*cplxf(cosf(ang), sinf(ang)); }
inline cplxf conj(const cplxf& src) { return cplxf(src.data[0], -src.data[1]); }

// element wise multiply
inline cplxf el_mul(const cplxf& a, const cplxf& b) { return cplxf(a.real()*b.real(), a.imag()*b.imag()); }


#define cplxf_I cplxf(0.0f, 1.0f)

// can linear interpolate
inline cplxf lin_interp(const float& frac, const cplxf& out_min, const cplxf& out_max)
	{ return out_min + (out_max-out_min)*frac; }

//-------------------------------------------------------------------------------------------------
struct CplxfPtrPair
// pair of pointers to cplxf (use for defining an array range)
{
	cplxf *a, *b;

	CplxfPtrPair() {}

	CplxfPtrPair(cplxf* start, long len = 0)
	{
		a = start;
		b = a+len;
	}

	CplxfPtrPair(cplxf* start, long b0, long b1)
	{
		a = start+b0;
		b = start+b1;
	}

	bool equal() const { return a == b; }

	// these operators dereference "a"
	cplxf& operator * () { return *a; }
	cplxf& operator ()() { return *a; }

	// return number of elements in length (assume "b" points to one after last element in range)
	int size() { return (int)(b-a); }
};


// can use slices to do element wise things
inline Slice<float, 2>& ToSlice(cplxf& c) { return reinterpret_cast <Slice<float, 2>& >(c); }
inline Slice<float, 2> ToSlice(const cplxf& c) { return ToSlice(c.data[0], c.data[1]); }

inline std::ostream& operator << (std::ostream& o, const cplxf& c) { return o << c.data[0] << "+i*" << c.data[1]; }


//-------------------------------------------------------------------------------------------------
// inline code to conjugate vecslices
//
//P:for $NAME ("Slice", "VecSlice") {
	//P:for $CHANNELS (1..$MAX_CHANNELS) {
		inline Slice<cplxf, $CHANNELS> conj(const $NAME<cplxf, $CHANNELS>& src)
		{
			Slice<cplxf, $CHANNELS> r;
			//P:for $I (0..$CHANNELS-1) {
				r.val_($I) = conj(src.val_($I));
			//P:}
			return r;
		}
	//P:}
//P:}

#endif
