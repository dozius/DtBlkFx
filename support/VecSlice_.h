#error Don't compile this one! use preprocess.pl to generate VecSlice.h & use that instead
#ifndef _DT_VEC_SLICE_H_
#define _DT_VEC_SLICE_H_


//P:$MAX_CHANNELS = 4;
//P:@BINARY_OP = (["+", "Add"], ["-", "Subtract"], ["*", "Multiply"], ["/", "Divide"], ["<<", "LeftShift"], [">>", "RightShift"], ["%", "Modulo"], ["|", "Or"], ["&", "And"], ["^", "Xor"]);

#include "misc_stuff.h"

//-------------------------------------------------------------------------------------------------
template <class T, int CHANNELS> struct VecSlice;
template <class T, int CHANNELS> struct Slice;

//-------------------------------------------------------------------------------------------------
template <class T, int CHANNELS> struct Slice
// a slice is similar to an array except that operators affect all elements
{
	T data[CHANNELS];

	// BE CAREFUL with this: break const to produce faster code
	T& val_(int ch) const { return (const_cast<Slice&>(*this)).data[ch]; }

	Slice() {}

	// copy from array
	template <class T2> Slice(const Array<T2, CHANNELS>& src) { for(int i = 0; i < CHANNELS; i++) data[i] = src[i]; }

	// copy from a VecSlice
	template <class T2> Slice(const VecSlice<T2, CHANNELS>& src) { SliceAssign(*this, src); }

	// copy from another type of Slice
	template <class T2> Slice(const Slice<T2, CHANNELS>& src) { SliceAssign(*this, src); }

	// try to copy from something else (e.g. set all elements to the same value)
	template <class T2> Slice(const T2& src) { SliceAssign(*this, src); }

	//declare all operator assignments
	//P:for $OP_ (@BINARY_OP) {
		//P:($OP, $OP_NAME) = @{$OP_};
		template <class B> Slice& operator $OP= (const B& b) { ${OP_NAME}Assign(*this, b); return *this; }
	//P:}

	// return a particular channel-element of the slice (not really const-correct)
	T operator [] (int ch) const { return data[ch]; }
	T& operator [] (int ch) { return data[ch]; }

};

//-------------------------------------------------------------------------------------------------
template <class T, int CHANNELS> struct VecPtr
// VecPtr is an array of pointers
//
// main purpose is to provide slices into the vectors - a slice is all channel pointers offset by
// the some number
//
{
	T* data[CHANNELS];

	VecPtr() {}

	// expect "T2" to be T* or compatible
	template <class T2> VecPtr(Array<T2, CHANNELS>& src, int offs = 0) { VecPtrAssign(*this, src, offs); }
		//{ for(int i = 0; i < CHANNELS; i++) data[i] = src.data[i]+offs; }

	// offset from another VecPtr (const_cast not actually needed)
	template <class T2> VecPtr(const VecPtr<T2, CHANNELS>& src, int offs) { VecPtrAssign(*this, const_cast<VecPtr<T2, CHANNELS>& >(src), offs); }
		//{ for(int i = 0; i < CHANNELS; i++) data[i] = src.data[i]+offs; }

	VecSlice<T, CHANNELS> GetVecSlice(int offs) const;

	// [] returns a vector slice (same as .GetVecSlice(offs) )
	VecSlice<T, CHANNELS> operator[] (int offs) const { return GetVecSlice(offs); }

	// () operator returns a particular element (same as doing [offs][ch])
	T& operator() (int offs, int ch) { return data[ch][offs]; }
	T operator() (int offs, int ch) const { return data[ch][offs]; }
};

//-------------------------------------------------------------------------------------------------
template <class T, int CHANNELS> struct VecSlice : public VecPtr<T, CHANNELS>
// VecSlice<> is a dereferrenced slice from VecPtr<>
// the difference is that all operators work on the VecSlice because they reference or modify
// whatever we're pointing at
{
	typedef VecPtr<T, CHANNELS> base;

	// BE CAREFUL with this: break const to produce faster code
	T& val_(int ch) const { return *base::data[ch]; }

	// 
	VecSlice(const VecSlice& src): VecPtr<T, CHANNELS>(src) { }

	template <class T2> VecSlice(const VecPtr<T2, CHANNELS>& src, int offs) : VecPtr<T, CHANNELS>(src, offs) {}

	// NOTE: we behave deref'd: assignment changes what we point at
	// use a non-member function because of problems with declaring partially specialized templated methods
	VecSlice& operator = (const VecSlice& src) { VecSliceAssign(*this, src); return *this; }
	template <class T2> VecSlice& operator = (const T2& src) { VecSliceAssign(*this, src); return *this; }

	//declare all operator assignments
	//P:for $OP_ (@BINARY_OP) {
		//P:($OP, $OP_NAME) = @{$OP_};
		template <class B> VecSlice& operator $OP= (const B& b) { ${OP_NAME}Assign(*this, b); return *this; }
	//P:}
	
	// return particular channel of the slice
	T operator [] (int ch) const { return *base::data[ch]; }
	T& operator [] (int ch) { return *base::data[ch]; }

	// use () operator go move to other slices above or below this slice
	VecSlice operator() (int offs) const { return base::GetVecSlice(offs); }

protected:
	// VecSlices can *only* come out of a VecPtr
	//friend VecPtr; // g++ complains
	VecSlice() {}
};

template <class T, int CHANNELS> VecSlice<T, CHANNELS> VecPtr<T, CHANNELS>::GetVecSlice(int offs) const
	{ return VecSlice<T, CHANNELS>(*this, offs); }

template <class T, int CHANNELS> Slice<T, CHANNELS> ToSlice(const VecSlice<T, CHANNELS>& src)
	{ return Slice<T, CHANNELS>(src); }


// handle reversed non-slice argument
//P:for $NAME ("Slice", "VecSlice") {
	template<class A, class T, int CHANNELS> inline bool isIdentical(const A& a, const $NAME<T, CHANNELS>& b)
		{ return isIdentical(b, a); }
	//P:for $OP ("==", "!=") {
		template<class A, class T, int CHANNELS> inline Slice<bool, CHANNELS> operator $OP (const A& a, const $NAME<T, CHANNELS>& b)
			{ return b $OP a; }
	//P:}
//P:}


//-------------------------------------------------------------------------------------------------
template <class T, int CHANNELS>
	inline std::ostream& operator << (std::ostream& o, const Slice<T, CHANNELS>& s)
{
	o << "[" << s[0];
	for(int i = 1; i < CHANNELS; i++) o << ", " << s[i];
	return o << "]";
}
template <class T, int CHANNELS>
	inline std::ostream& operator << (std::ostream& o, const VecSlice<T, CHANNELS>& vs)
		{ return o << ToSlice(vs); }

//-------------------------------------------------------------------------------------------------
// now for lots of inlined code
//
//P:for $CHANNELS (1..$MAX_CHANNELS) {

	//---------------------------------------------------------------------------------------------
	template <class T, class T2/*Array or VecPtr*/>
		inline void VecPtrAssign(VecPtr<T, $CHANNELS>& dst, T2& src, int offs)
	// force a const (removal) cast to handle Array<T*> that returns const pointer reference
	{
		//P:for $I (0..$CHANNELS-1) {
			dst.data[$I] = src.data[$I]+offs;
		//P:}
	}

	//---------------------------------------------------------------------------------------------
	template <class T> inline Slice<T, $CHANNELS> ToSlice(
		//P:for ($I=0; $I < $CHANNELS-1; $I++) {
			const T& arg_$I,
		//P:}
		const T& arg_$I
	)
	// take a variable number of arguments to generate a Slice
	{
		Slice<T, $CHANNELS> r;
		//P:for $I (0..$CHANNELS-1) {
			r[$I] = arg_$I;
		//P:}
		return r;
	}

	//P:for $NAME ("Slice", "VecSlice") {
		//P:$TYPE = "$NAME<T, $CHANNELS>";

		//-----------------------------------------------------------------------------------------
		template<class T, class SRC> inline void ${NAME}Assign($TYPE& dst, const SRC& src)
		// assign "src" to all elements
		{
			//P:for $I (0..$CHANNELS-1) {
				dst[$I] = src;
			//P:}
		}
		//-----------------------------------------------------------------------------------------
		template<class T> inline Slice<T, $CHANNELS> operator- (const $TYPE& src)
		// return negative of all elements
		{
			Slice<T, $CHANNELS> r;
			//P:for $I (0..$CHANNELS-1) {
				r[$I] = -src.val_($I);
			//P:}
			return r;
		}

		//-----------------------------------------------------------------------------------------
		template<class T, class T2> inline bool isIdentical(const $TYPE& a, const T2& b)
		// return true if all elements in "a" have the same value as in "b"
		{
			return
			//P:for ($I=0; $I < $CHANNELS-1; $I++) {
				a.val_($I) == b &&
			//P:}
			a.val_($I) == b;
		}

		//P:for $OP ("==", "!=") {
			//-------------------------------------------------------------------------------------
			template<class T, class B> inline Slice<bool, $CHANNELS> operator $OP (const $TYPE& a, const B& b)
			// element-wise $OP
			{
				return ToSlice(
					//P:for ($I=0; $I < $CHANNELS-1; $I++) {
						a.val_($I) $OP b,
					//P:}
					a.val_($I) $OP b
				);
			}
		//P:}

		//P:for $TYPE2 ("VecSlice<T2, $CHANNELS>", "Slice<T2, $CHANNELS>") {
			//-----------------------------------------------------------------------------------------
			template<class T, class T2> inline void ${NAME}Assign($TYPE& dst, const $TYPE2& src)
			// element-wise assign
			{
				//P:for $I (0..$CHANNELS-1) {
					dst[$I] = src.val_($I);
				//P:}
			}

			//-----------------------------------------------------------------------------------------
			template<class T, class T2> inline bool isIdentical(const $TYPE& a, const $TYPE2& b)
			// return true if all elements in "a" have the same value as in "b"
			{
				return
				//P:for ($I=0; $I < $CHANNELS-1; $I++) {
					a.val_($I) == b.val_($I) &&
				//P:}
				a.val_($I) == b.val_($I);
			}

			//P:for $OP ("==", "!=") {
				//-------------------------------------------------------------------------------------
				template<class T, class T2> inline Slice<bool, $CHANNELS> operator $OP (const $TYPE& a, const $TYPE2& b)
				// element-wise $OP
				{
					return ToSlice(
						//P:for ($I=0; $I < $CHANNELS-1; $I++) {
							a.val_($I) $OP b.val_($I),
						//P:}
						a.val_($I) $OP b.val_($I)
					);
				}
			//P:}
		//P:}
		//P:for $OP_ (@BINARY_OP) {
			//P:($OP, $OP_NAME) = @{$OP_};
			//-------------------------------------------------------------------------------------
			template<class T, class B> inline void ${OP_NAME}Assign($TYPE& dst, B b)
			// apply $OP= b on all elements of a, take a copy of "b" in case it is an element from "dst"
			{
				//P:for $I (0..$CHANNELS-1) {
					dst.val_($I) $OP= b;
				//P:}
				//dst $OP= Slice<B, $CHANNELS>(b); // sometimes this one produces faster code
			}
			
			//-------------------------------------------------------------------------------------
			template <class T, class B> inline Slice<T, $CHANNELS> operator $OP (const $TYPE& a, const B& b)
			// apply $OP b on all elements of a
			{
				Slice<T, $CHANNELS> r;
				//P:for $I (0..$CHANNELS-1) {
					r[$I] = a.val_($I) $OP b;
				//P:}
				return r;
			}
			//-------------------------------------------------------------------------------------
			template <class T, class A> inline Slice<A, $CHANNELS> operator $OP (const A& a, const $TYPE& b)
			// apply a $OP on all elements of b, assume return type should match "A"
			{
				Slice<A, $CHANNELS> r;
				//P:for $I (0..$CHANNELS-1) {
					r[$I] = a $OP b.val_($I);
				//P:}
				return r;
			}

			//P:for $TYPE2 ("VecSlice<T2, $CHANNELS>", "Slice<T2, $CHANNELS>") {
				//---------------------------------------------------------------------------------
				template <class T, class T2> inline void ${OP_NAME}Assign($TYPE& dst, const $TYPE2& b)
				// element wise $OP=, note, this will work ok even if dst & b are the same object
				{
					//P:for $I (0..$CHANNELS-1) {
						dst.val_($I) $OP= b.val_($I);
					//P:}
				}
				//---------------------------------------------------------------------------------
				template <class T, class T2> inline Slice<T, $CHANNELS> operator $OP (const $TYPE& a, const $TYPE2& b)
				// element wise $OP
				{
					Slice<T, $CHANNELS> r;
					//P:for $I (0..$CHANNELS-1) {
						r[$I] = a.val_($I) $OP b.val_($I);
					//P:}
					return r;
				}
			//P:}
		//P:}
	//P:}
//P:}

#if 0
//-------------------------------------------------------------------------------------------------
// unfortunately all of the loop processing stuff runs slower than doing manual loops otherwise
// it would be quite nice except for silly type promotion problems
//
namespace LoopProcess
{
	template <class LOOP, class ARG, class RET> struct Foreach1;

	//P:for $OP (@BINARY_OP) {
		struct $OP->[1] {
			template <class DST, class A, class B> __inline static DST get(const int& i, const A& a, const B& b) { return a.get<DST>(i) $OP->[0] b.get<DST>(i); }
			template <class DST, class SRC> __inline static void assign(DST& dst, const SRC& src) { dst $OP->[0]= src; }
		};
	//P:}
	struct Assign {
		template <class DST, class SRC> static __inline void assign(DST& dst, const SRC& src) { dst = src; }
	};

	template <class A, class B, class OP> struct BinaryOp
	{
		A a;
		B b;
		template <class A2, class B2> __inline BinaryOp(const A2& a_, const B2& b_) : a(a_), b(b_) {}
		template <class R> __inline R get(const int& i) const { return OP::template get<R>(i, a, b); }
	};

	template <class T> struct Single
	{
		T t;
		template <class T2> __inline Single(const T2& t_):  t(t_) { }
		template <class T2/*ignored*/> __inline const T& get(const int& i) const { return t; }
	};


	struct LoopFwd { __inline static int get(const int& i) { return i; } };
	struct LoopRev { __inline static int get(const int& i) { return -i; } };

	template <class ARRAY_T, class GET_EL_T, class DIR> struct Loop
	{
		ARRAY_T data;
		__inline Loop(const ARRAY_T& data_) : data(data_) { }
		template <class R/*ignored*/> __inline GET_EL_T get(const int& i) const { return data[DIR::get(i)]; }
	};

	template <
		class ARRAY_T,
		class GET_EL_T,
		class OP_EL_T,
		class DIR
	>
		struct LoopAssign : public Loop<ARRAY_T, GET_EL_T, DIR>
	{
		typedef Loop<ARRAY_T, GET_EL_T, DIR> base;

		int n;

		__inline LoopAssign(const ARRAY_T& data_, int n_) : base(data_), n(n_) { }

		// do element wise operator assign
		template <class OP, class SRC> __inline LoopAssign& run(const SRC& src)
		{
			for(int i = 0; i < n; i++) OP::assign(base::data[DIR::get(i)], src.template get<OP_EL_T>(i));
			return *this;
		}
	
		//P:for $ASSIGN_OP (["", "Assign"], @BINARY_OP) {
			template <class SRC> __inline LoopAssign& operator$ASSIGN_OP->[0]= (const SRC& src) { return run<$ASSIGN_OP->[1]>(Single<SRC>(src)); }
			template <class A, class B, class OP> __inline LoopAssign& operator$ASSIGN_OP->[0]= (const BinaryOp<A, B, OP>& src) { return run<$ASSIGN_OP->[1]>(src); }
			template <class ARRAY_T2, class GET_EL_T2, class DIR2> __inline LoopAssign& operator$ASSIGN_OP->[0]= (const Loop<ARRAY_T2, GET_EL_T2, DIR2>& src) { return run<$ASSIGN_OP->[1]>(src); }
			template <class LOOP2, class ARG2, class RET2> __inline LoopAssign& operator$ASSIGN_OP->[0]= (const Foreach1<LOOP2, ARG2, RET2>& src) { return run<$ASSIGN_OP->[1]>(src); }
		//P:}
	};

	template <class LOOP, class ARG, class RET> struct Foreach1
	{
		typedef RET (*FN)(const ARG&);
		FN fn;
		LOOP loop;
		__inline Foreach1(FN fn_, const LOOP& loop_): fn(fn_), loop(loop_) { }
		template <class T2/*ignored*/> __inline const RET get(const int& i) const { return (*fn)(loop.get<ARG>(i)); }
	};

};

template <class T> __inline LoopProcess::Single<T> Single(const T& t) { return LoopProcess::Single<T>(t); }


//P:sub t_proc { # prefix, type, pass
//P:	my ($prefix, $type, $pass) = @_;
//P:	s/\+/$prefix/g for ($type, $pass);
//P:	my ($name, $targs) = $type =~ /(.*)(?:<(.*)>)/;
//P:	(my $args = $targs) =~ s/[\{\}]//g; 
//P:	my @t = $targs =~ /\{(\w+)\}/g;
//P:	my $decl = $name.(@t ? "<".join(", ", @t).">" : "");
//P:	($args, $decl, $pass ne ""? $pass:$decl)
//P:}

//P:for $OP (@BINARY_OP) {
	//P:@types = (
	//P:	["LoopProcess::Loop<class {+ARRAY_T}, class {+GET_EL_T}, class {+DIR}>", "", 1],
	//P:	["LoopProcess::BinaryOp<class {+A}, class {+B}, class {+OP}>", "", 1],
	//P:	["LoopProcess::Foreach1<class {+LOOP}, class {+ARG}, class {+RET}>", "", 1],
	//P:	["LoopProcess::Single<class {+T}>", "", 1],
	//P:	["LoopProcess::Single<class {+T}>", "+T", 0]
	//P:);
	//P:for $a (@types) {
		//P:for $b (@types) {
			//P: next if !$a->[2] && !$b->[2];
			//P:($A_ARGS, $A_DECL, $A_PASS) = t_proc('A', @{$a});
			//P:($B_ARGS, $B_DECL, $B_PASS) = t_proc('B', @{$b});
			template <$A_ARGS, $B_ARGS>
				__inline LoopProcess::BinaryOp<$A_DECL, $B_DECL, LoopProcess::$OP->[1] >
					operator $OP->[0] (const $A_PASS& a, const $B_PASS& b)
			{
				return LoopProcess::BinaryOp<$A_DECL, $B_DECL, LoopProcess::$OP->[1] >(a, b);
			}
		//P:}
	//P:}
//P:}


//P:for $DIR ("Fwd", "Rev") {
	//P:for $T (["class T, int CH", "VecPtr<T, CH>", "VecSlice<T, CH>", "Slice<T, CH>"], ["class T", "T*", "T", "T"]) {
		// lvalue or rvalue: $DIR $T->[1] (operation type $T->[2], get type $T->[3])
		template <$T->[0]>
			__inline LoopProcess::LoopAssign<$T->[1], $T->[2], $T->[3], LoopProcess::Loop$DIR >
				$DIR(const $T->[1]& data, int n)
					{ return LoopProcess::LoopAssign<$T->[1], $T->[2], $T->[3], LoopProcess::Loop$DIR >(data, n); }

		// rvalue only: $DIR $T->[1] (get type $T->[2])
		template <$T->[0]>
			__inline LoopProcess::Loop<$T->[1], $T->[2], LoopProcess::Loop$DIR >
				$DIR(const $T->[1]& data)
					{ return LoopProcess::Loop<$T->[1], $T->[2], LoopProcess::Loop$DIR >(data); }

	//P:}
//P:}

template <class LOOP, class ARG, class RET>
	__inline LoopProcess::Foreach1<LOOP, ARG, RET> Foreach(RET (*fn)(const ARG&), const LOOP& loop)
		{ return LoopProcess::Foreach1<LOOP, ARG, RET>(fn, loop); }
#endif

#endif
