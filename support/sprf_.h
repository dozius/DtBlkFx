/*
 run through preprocess.pl to generate actual code
 perl preprocess.pl sprf_.h >sprf.h


generate lots of functions that take variable arguments so that you can do things
like this:

	CharArray<2048> my_string;
	my_string << sprf("%.3f %d", 12.3456, 13);
	// my_string now contains "12.346 13"

*/

#ifndef _DT_MISC_STUFF_H_
#include "misc_stuff.h"
#else
#ifndef _DT_SPRF_H_
#define _DT_SPRF_H_
// sprf(<up to 10 arguments>)

//P:for $I (1..10) {
//-------------------------------------------------------------------------------------------------
	//P:$t_def = join ", ", (map { "class ARG$_" } (1..$I));
	//P:$t_arg = join ", ", (map { "ARG$_" } (1..$I));

	template <$t_def> struct RngSprintf$I
	{
		const char* fmt;
		//P:for (1..$I) {
			ARG$_ arg$_;
		//P:}
	};

	template <$t_def> inline CharRng operator << (CharRng dst, RngSprintf$I<$t_arg> s)
	{
		//P:$tmp = join "", (map { ", s.arg$_" } (1..$I));
		return rng_sprf(dst, s.fmt$tmp);
	}

	//P:$tmp = join "", (map { ", ARG$_ arg$_" } (1..$I));
	template <$t_def> inline RngSprintf$I<$t_arg> sprf(const char* fmt$tmp)
	{
		//P:$tmp = join "", (map { ", arg$_" } (1..$I));
		RngSprintf$I<$t_arg> r = { fmt$tmp };
		return r;
	}
//P:}

#endif
#endif
