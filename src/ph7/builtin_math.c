/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * round() (defined below, under PH7_DISABLE_BUILTIN_FUNC rather than the
 * math-func guard) needs floor/ceil/fabs/copysign/fmod/isfinite/pow plus the
 * libc snprintf/strtod round-trip for its high-precision branch, so pull these
 * in unconditionally here — they must be available even when
 * PH7_ENABLE_MATH_FUNC is off. abs() is also used by the guarded math builtins.
 */
#include <math.h>
#include <stdio.h>  /* snprintf: correctly-rounded high-precision round() round-trip */
#include <stdlib.h> /* strtod (round-trip inverse), abs */
#ifdef PH7_ENABLE_MATH_FUNC

/*
 * Section:
 *    Math Functions.

 * Status:
 *    Stable.
 */
/*
 * float sqrt(float $arg )
 *  Square root of the given number.
 * Parameter
 *  The number to process.
 * Return
 *  The square root of arg or the special value Nan of failure.
 */
/*
 * The libm-backed float functions php has and PH7 lacked entirely. Doing these in PHP
 * (log1p as log(1+x), acosh via logs, ...) would lose precision, so they go through libm.
 */
#define PH7_MATH_UNARY(NAME,CFUNC)                                        \
PH7_PRIVATE int PH7_builtin_##NAME(ph7_context *pCtx,int nArg,ph7_value **apArg) \
{                                                                          \
	double x;                                                              \
	if( nArg < 1 ){                                                        \
		ph7_result_int(pCtx,0);                                            \
		return PH7_OK;                                                     \
	}                                                                      \
	x = ph7_value_to_double(apArg[0]);                                     \
	ph7_result_double(pCtx,CFUNC(x));                                      \
	return PH7_OK;                                                          \
}
PH7_MATH_UNARY(acosh,acosh)
PH7_MATH_UNARY(asinh,asinh)
PH7_MATH_UNARY(atanh,atanh)
PH7_MATH_UNARY(expm1,expm1)
PH7_MATH_UNARY(log1p,log1p)
PH7_PRIVATE int PH7_builtin_deg2rad(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double x;
	if( nArg < 1 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	ph7_result_double(pCtx,x * (3.14159265358979323846 / 180.0));
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_rad2deg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double x;
	if( nArg < 1 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	ph7_result_double(pCtx,x * (180.0 / 3.14159265358979323846));
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_fpow(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double x,y;
	if( nArg < 2 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	y = ph7_value_to_double(apArg[1]);
	ph7_result_double(pCtx,pow(x,y));
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = sqrt(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float exp(float $arg )
 *  Calculates the exponent of e.
 * Parameter
 *  The number to process.
 * Return
 *  'e' raised to the power of arg.
 */
PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = exp(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float floor(float $arg )
 *  Round fractions down.
 * Parameter
 *  The number to process.
 * Return
 *  Returns the next lowest integer value by rounding down value if necessary.
 */
PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	/* PHP requires exactly one argument. */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"floor() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/*
	 * Validate argument type. Only int/float (and numeric strings) are accepted.
	 * Other types (including non-numeric strings) raise a TypeError just like
	 * ceil() and other math functions.
	 */
	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){
		if( ph7_value_is_string(apArg[0]) ){
			int len;
			sxu8 bReal = FALSE;
			const char *zStr = ph7_value_to_string(apArg[0], &len);
			sxi32 rcNum;
			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);
			if( rcNum != SXRET_OK ){
				return PH7_VmThrowException(pCtx,
					"TypeError",
					"floor(): Argument #1 ($num) must be of type int|float, %s given",
					ph7_type_name(apArg[0])
					);
			}
		}else{
			/* Disallow all other types (arrays, objects, resources, etc.) */
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"floor(): Argument #1 ($num) must be of type int|float, %s given",
				ph7_type_name(apArg[0])
				);
		}
	}

	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = floor(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float cos(float $arg )
 *  Cosine.
 * Parameter
 *  The number to process.
 * Return
 *  The cosine of arg.
 */
PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = cos(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float acos(float $arg )
 *  Arc cosine.
 * Parameter
 *  The number to process.
 * Return
 *  The arc cosine of arg.
 */
PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r, x;
	/* PHP enforces exactly one argument and a floatable parameter. */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"acos() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)
	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but
	 * the float conversion will handle them. */
	if( !ph7_value_is_numeric(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"acos(): Argument #1 ($num) must be of type float, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Convert to double now that we know it's numeric. */
	x = ph7_value_to_double(apArg[0]);
	/* Handle domain error ourselves.  PHP returns NaN for |x|>1. */
	if( x < -1.0 || x > 1.0 ){
		r = PH7_NAN_VALUE();
	}else{
		r = acos(x);
	}
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float cosh(float $arg )
 *  Hyperbolic cosine.
 * Parameter
 *  The number to process.
 * Return
 *  The hyperbolic cosine of arg.
 */
PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = cosh(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float sin(float $arg )
 *  Sine.
 * Parameter
 *  The number to process.
 * Return
 *  The sine of arg.
 */
PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = sin(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float asin(float $arg )
 *  Arc sine.
 * Parameter
 *  The number to process.
 * Return
 *  The arc sine of arg.
 */
PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r, x;
	/* PHP enforces exactly one argument and a floatable parameter. */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"asin() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)
	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but
	 * the float conversion will handle them. */
	if( !ph7_value_is_numeric(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"asin(): Argument #1 ($num) must be of type float, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Convert to double now that we know it's numeric. */
	x = ph7_value_to_double(apArg[0]);
	/* Handle domain error ourselves.  PHP returns NaN for |x|>1. */
	if( x < -1.0 || x > 1.0 ){
		r = PH7_NAN_VALUE();
	}else{
		r = asin(x);
	}
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float sinh(float $arg )
 *  Hyperbolic sine.
 * Parameter
 *  The number to process.
 * Return
 *  The hyperbolic sine of arg.
 */
PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = sinh(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float ceil(float $arg )
 *  Round fractions up.
 * Parameter
 *  The number to process.
 * Return
 *  The next highest integer value by rounding up value if necessary.
 */
PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	/* PHP requires exactly one argument. */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"ceil() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/*
	 * PHP only accepts ints, floats or numeric strings.  Any other types
	 * (in particular non-numeric strings) should raise a TypeError.  We
	 * mimic the approach used by abs() and perform an explicit numeric
	 * check on strings before converting to double.
	 */
	if( !ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]) ){
		if( ph7_value_is_string(apArg[0]) ){
			int len;
			sxu8 bReal = FALSE;
			const char *zStr = ph7_value_to_string(apArg[0], &len);
			sxi32 rcNum;
			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);
			if( rcNum != SXRET_OK ){
				return PH7_VmThrowException(pCtx,
					"TypeError",
					"ceil(): Argument #1 ($num) must be of type int|float, string given"
					);
			}
		}else{
			/* Reject arrays, objects, resources, booleans, NULL, etc. */
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"ceil(): Argument #1 ($num) must be of type int|float"
				);
		}
	}

	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = ceil(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float tan(float $arg )
 *  Tangent.
 * Parameter
 *  The number to process.
 * Return
 *  The tangent of arg.
 */
PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = tan(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float atan(float $arg )
 *  Arc tangent.
 * Parameter
 *  The number to process.
 * Return
 *  The arc tangent of arg.
 */
PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	/* PHP enforces exactly one argument. */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"atan() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Type checking: reject non-numeric values (arrays, objects, resources, non-numeric strings).
	 * PHP 8 reports a TypeError for wrong types. */
	if( !ph7_value_is_numeric(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"atan(): Argument #1 ($num) must be of type float, %s given",
			ph7_type_name(apArg[0])
			);
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = atan(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float tanh(float $arg )
 *  Hyperbolic tangent.
 * Parameter
 *  The number to process.
 * Return
 *  The Hyperbolic tangent of arg.
 */
PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = tanh(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float atan2(float $y,float $x)
 *  Arc tangent of two variable.
 * Parameter
 *  $y = Dividend parameter.
 *  $x = Divisor parameter.
 * Return
 *  The arc tangent of y/x in radian.
 */
PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x,y;
	/* PHP enforces exactly two arguments. */
	if( nArg != 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"atan2() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	/* Type checking: reject non-numeric values for $y (argument #1). */
	if( !ph7_value_is_numeric(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"atan2(): Argument #1 ($y) must be of type float, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Type checking: reject non-numeric values for $x (argument #2). */
	if( !ph7_value_is_numeric(apArg[1]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"atan2(): Argument #2 ($x) must be of type float, %s given",
			ph7_type_name(apArg[1])
			);
	}
	y = ph7_value_to_double(apArg[0]);
	x = ph7_value_to_double(apArg[1]);
	/* Perform the requested operation */
	r = atan2(y,x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float/int64 abs(float/int64 $arg )
 *  Absolute value.
 * Parameter
 *  The number to process.
 * Return
 *  The absolute value of number.
 */
PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int is_float;
	/* PHP requires exactly one argument. */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"abs() expects exactly 1 argument, %d given",
			nArg
			);
	}

	if( ph7_value_is_null(apArg[0]) ){
		/* php only DEPRECATES null here; PHL rejects it. */
		return PH7_VmThrowException(pCtx,"TypeError",
			"abs(): Argument #1 ($num) must be of type int|float, null given");
	}
	/* Numeric strings with decimal/exponent are treated as real values. */
	is_float = ph7_value_is_float(apArg[0]);
	if( !is_float && ph7_value_is_string(apArg[0]) ){
		int len;
		sxu8 bReal = FALSE;
		const char *zStr = ph7_value_to_string(apArg[0], &len);
		sxi32 rcNum;
		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);
		if( rcNum != SXRET_OK ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"abs(): Argument #1 ($num) must be of type int|float, string given"
				);
		}
		if( bReal ){
			is_float = 1;
		}
	}
	if( is_float ){
		double r,x;
		x = ph7_value_to_double(apArg[0]);
		/* Perform the requested operation */
		r = fabs(x);
		ph7_result_double(pCtx,r);
	}else{
		/* Read the full 64-bit value (the old 32-bit `int abs()` truncated any
		 * magnitude above 2^31 and was UB on INT_MIN). */
		sxi64 x = ph7_value_to_int64(apArg[0]);
		if( x == SMALLEST_INT64 ){
			/* abs(PHP_INT_MIN) has no int representation, so PHP returns a float. */
			ph7_result_double(pCtx,-(double)x);
		}else{
			ph7_result_int64(pCtx,x < 0 ? -x : x);
		}
	}
	return PH7_OK;
}
/*
 * float log(float $arg,[int/float $base])
 *  Natural logarithm.
 * Parameter
 *  $arg: The number to process.
 *  $base: The optional logarithmic base to use. (only base-10 is supported)
 * Return
 *  The logarithm of arg to base, if given, or the natural logarithm.
 * Note:
 *  only Natural log and base-10 log are supported.
 */
PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){
		/* Base-10 log */
		r = log10(x);
	}else{
		r = log(x);
	}
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float log10(float $arg )
 *  Base-10 logarithm.
 * Parameter
 *  The number to process.
 * Return
 *  The Base-10 logarithm of the given number.
 */
PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = log10(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * number pow(number $base,number $exp)
 *  Exponential expression.
 * Parameter
 *  base
 *  The base to use.
 * exp
 *  The exponent.
 * Return
 *  base raised to the power of exp.
 *  If the result can be represented as integer it will be returned
 *  as type integer, else it will be returned as type float.
 */
PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x,y;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	y = ph7_value_to_double(apArg[1]);
	/* Perform the requested operation */
	r = pow(x,y);
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float pi(void)
 *  Returns an approximation of pi.
 * Note
 *  you can use the M_PI constant which yields identical results to pi().
 * Return
 *  The value of pi as float.
 */
PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	ph7_result_double(pCtx,PH7_PI);
	return PH7_OK;
}
/*
 * float fmod(float $x,float $y)
 *  Returns the floating point remainder (modulo) of the division of the arguments.
 * Parameters
 * $x
 *  The dividend
 * $y
 *  The divisor
 * Return
 *  The floating point remainder of x/y.
 */
PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double x,y,r;
	if( nArg < 2 ){
		/* Missing arguments */
		ph7_result_double(pCtx,0);
		return PH7_OK;
	}
	/* Extract given arguments */
	x = ph7_value_to_double(apArg[0]);
	y = ph7_value_to_double(apArg[1]);
	/* Perform the requested operation */
	r = fmod(x,y);
	/* Processing result */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float hypot(float $x,float $y)
 *  Calculate the length of the hypotenuse of a right-angle triangle .
 * Parameters
 * $x
 *  Length of first side
 * $y
 *  Length of first side
 * Return
 *  Calculated length of the hypotenuse.
 */
PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double x,y,r;
	if( nArg < 2 ){
		/* Missing arguments */
		ph7_result_double(pCtx,0);
		return PH7_OK;
	}
	/* Extract given arguments */
	x = ph7_value_to_double(apArg[0]);
	y = ph7_value_to_double(apArg[1]);
	/* Perform the requested operation */
	r = hypot(x,y);
	/* Processing result */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
#endif /* PH7_ENABLE_MATH_FUNC */
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * PHP rounding modes (mirror ext/standard/php_math_round_mode.h).
 * Only the four HALF_* integer constants are exposed to userland
 * (PHP_ROUND_HALF_UP..HALF_ODD, see constant.c); the CEILING/FLOOR/
 * TOWARD_ZERO/AWAY_FROM_ZERO modes (5..8) have no userland constant but
 * are reachable by passing the raw integer to round()'s 3rd argument,
 * which PHP 8.5 still accepts, so all eight are honored here.
 */
#define PH7_ROUND_HALF_UP        1
#define PH7_ROUND_HALF_DOWN      2
#define PH7_ROUND_HALF_EVEN      3
#define PH7_ROUND_HALF_ODD       4
#define PH7_ROUND_CEILING        5
#define PH7_ROUND_FLOOR          6
#define PH7_ROUND_TOWARD_ZERO    7
#define PH7_ROUND_AWAY_FROM_ZERO 8
/*
 * 10**power via an exact lookup table for 0..22, falling back to pow()
 * otherwise. Port of php-src PHP-8.5 ext/standard/math.c php_intpow10().
 */
static double MathIntPow10(int power)
{
	static const double powers[] = {
		1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
		1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
	};
	if( power < 0 || power > 22 ){
		return pow(10.0, (double)power);
	}
	return powers[power];
}
static double MathRoundBasicEdge(double integral, double exponent, int places)
{
	return (places > 0)
		? fabs((integral + copysign(0.5, integral)) / exponent)
		: fabs((integral + copysign(0.5, integral)) * exponent);
}
static double MathRoundZeroEdge(double integral, double exponent, int places)
{
	return (places > 0)
		? fabs((integral) / exponent)
		: fabs((integral) * exponent);
}
/*
 * Round the extracted integral part according to the requested mode.
 * Faithful port of php-src PHP-8.5 ext/standard/math.c php_round_helper().
 */
static double MathRoundHelper(double integral, double value, double exponent, int places, int mode)
{
	double value_abs = fabs(value);
	double edge_case;
	switch( mode ){
		case PH7_ROUND_HALF_UP:
			edge_case = MathRoundBasicEdge(integral, exponent, places);
			if( value_abs >= edge_case ){
				return integral + copysign(1.0, integral);
			}
			return integral;
		case PH7_ROUND_HALF_DOWN:
			edge_case = MathRoundBasicEdge(integral, exponent, places);
			if( value_abs > edge_case ){
				return integral + copysign(1.0, integral);
			}
			return integral;
		case PH7_ROUND_CEILING:
			edge_case = MathRoundZeroEdge(integral, exponent, places);
			if( value > 0.0 && value_abs > edge_case ){
				return integral + 1.0;
			}
			return integral;
		case PH7_ROUND_FLOOR:
			edge_case = MathRoundZeroEdge(integral, exponent, places);
			if( value < 0.0 && value_abs > edge_case ){
				return integral - 1.0;
			}
			return integral;
		case PH7_ROUND_TOWARD_ZERO:
			return integral;
		case PH7_ROUND_AWAY_FROM_ZERO:
			edge_case = MathRoundZeroEdge(integral, exponent, places);
			if( value_abs > edge_case ){
				return integral + copysign(1.0, integral);
			}
			return integral;
		case PH7_ROUND_HALF_EVEN:
			edge_case = MathRoundBasicEdge(integral, exponent, places);
			if( value_abs > edge_case ){
				return integral + copysign(1.0, integral);
			}else if( value_abs == edge_case ){
				if( fmod(integral, 2.0) != 0.0 ){ /* integral not even -> make it even */
					return integral + copysign(1.0, integral);
				}
			}
			return integral;
		case PH7_ROUND_HALF_ODD:
			edge_case = MathRoundBasicEdge(integral, exponent, places);
			if( value_abs > edge_case ){
				return integral + copysign(1.0, integral);
			}else if( value_abs == edge_case ){
				if( fmod(integral, 2.0) == 0.0 ){ /* integral even -> make it odd */
					return integral + copysign(1.0, integral);
				}
			}
			return integral;
		default:
			return integral; /* unreachable: mode validated by the caller */
	}
}
/*
 * Round `value` to `places` decimals in `mode`. Faithful port of php-src
 * PHP-8.5 ext/standard/math.c _php_math_round() — the post-8.4
 * integer-extraction algorithm with the +/-1 floating-point error
 * correction step, required for byte-exact results on cases such as
 * round(0.285, 2) == 0.29 that the old naive "+0.5" approach got wrong.
 */
static double MathRound(double value, int places, int mode)
{
	double exponent, tmp_value, tmp_value2;
	int abs_places;
	if( !isfinite(value) || value == 0.0 ){
		return value;
	}
	/* mirror php-src's clamp away from INT_MIN */
	if( places < -2147483647 ){
		places = -2147483647;
	}
	abs_places = places < 0 ? -places : places;
	exponent = MathIntPow10(abs_places);
	/*
	 * Extracting the integer part can be off by one ULP due to float error
	 * (e.g. floor(0.285 * 1e10) == 2849999999). Try +/-1 and keep it if it
	 * divides back to exactly `value`.
	 */
	if( value >= 0.0 ){
		tmp_value = floor(places > 0 ? value * exponent : value / exponent);
		tmp_value2 = tmp_value + 1.0;
	}else{
		tmp_value = ceil(places > 0 ? value * exponent : value / exponent);
		tmp_value2 = tmp_value - 1.0;
	}
	if( (places > 0 ? tmp_value2 / exponent : tmp_value2 * exponent) == value ){
		tmp_value = tmp_value2;
	}
	/* Beyond our precision, so rounding it is pointless. */
	if( fabs(tmp_value) >= 1e16 ){
		return value;
	}
	tmp_value = MathRoundHelper(tmp_value, value, exponent, places, mode);
	if( abs_places < 23 ){
		tmp_value = (places > 0) ? tmp_value / exponent : tmp_value * exponent;
	}else{
		/*
		 * Simple division would lose precision here; round-trip through a
		 * string exactly like php-src does (snprintf "%15fe%d" + strtod).
		 * libc snprintf is used (not SyBufferFormat, which is not
		 * correctly-rounded) so the low bits match PHP. (SyStrToReal now
		 * delegates to strtod too; the direct call here simply mirrors
		 * php-src's own snprintf+strtod pairing.)
		 */
		char zBuf[64];
		snprintf(zBuf, sizeof(zBuf), "%15fe%d", tmp_value, -places);
		zBuf[sizeof(zBuf)-1] = '\0';
		tmp_value = strtod(zBuf, 0);
		if( !isfinite(tmp_value) || isnan(tmp_value) ){
			tmp_value = value;
		}
	}
	return tmp_value;
}
/*
 * float round ( int|float $num [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )
 *  Rounds a float.
 * Parameters
 *  $num       The value to round.
 *  $precision The optional number of decimal digits to round to. May be
 *             negative (rounds to the left of the decimal point).
 *  $mode      One of PHP_ROUND_HALF_UP (default) / _HALF_DOWN / _HALF_EVEN /
 *             _HALF_ODD, or the 8.5 integer modes CEILING / FLOOR /
 *             TOWARD_ZERO / AWAY_FROM_ZERO (5..8).
 * Return
 *  The rounded value as a float.
 */
PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double value, r;
	int places = 0;
	int mode = PH7_ROUND_HALF_UP;
	/*
	 * Legacy PHL contract: no argument -> int(0). PHP throws an
	 * ArgumentCountError here, but two PHL-only (--SKIPIF-- zend_version)
	 * tests assert round()===0, so keep the historical behavior.
	 */
	if( nArg < 1 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 3 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"round() expects at most 3 arguments, %d given",
			nArg
			);
	}
	/*
	 * Validate argument #1: only int/float (and numeric strings) are
	 * accepted; every other type raises a TypeError (mirrors floor()/ceil()).
	 */
	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){
		if( ph7_value_is_string(apArg[0]) ){
			int len;
			sxu8 bReal = FALSE;
			const char *zStr = ph7_value_to_string(apArg[0], &len);
			if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){
				return PH7_VmThrowException(pCtx,
					"TypeError",
					"round(): Argument #1 ($num) must be of type int|float, %s given",
					ph7_type_name(apArg[0])
					);
			}
		}else{
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"round(): Argument #1 ($num) must be of type int|float, %s given",
				ph7_type_name(apArg[0])
				);
		}
	}
	/* Precision (arg #2). Negative values are valid; clamp to int range. */
	if( nArg > 1 ){
		sxi64 prec = ph7_value_to_int64(apArg[1]);
		if( prec > 2147483647 ){
			places = 2147483647;
		}else if( prec < -2147483647 ){
			places = -2147483647;
		}else{
			places = (int)prec;
		}
	}
	/*
	 * Mode (arg #3). PHP 8.5 accepts the integer modes 1..8. Read the full
	 * 64-bit value before range-checking so a large out-of-range mode cannot
	 * alias a valid 1..8 via a truncating 32-bit cast (e.g. 0x1_0000_0003).
	 */
	if( nArg > 2 ){
		sxi64 m = ph7_value_to_int64(apArg[2]);
		if( m < PH7_ROUND_HALF_UP || m > PH7_ROUND_AWAY_FROM_ZERO ){
			return PH7_VmThrowException(pCtx,
				"ValueError",
				"round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)"
				);
		}
		mode = (int)m;
	}
	value = ph7_value_to_double(apArg[0]);
	/* Integer input with non-negative precision needs no rounding. */
	if( ph7_value_is_int(apArg[0]) && places >= 0 ){
		ph7_result_double(pCtx,value);
		return PH7_OK;
	}
	r = MathRound(value, places, mode);
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * Assemble php's formatted number: the integer digits grouped from the right by
 * the thousands separator, then the decimal separator and exactly $decimals
 * fraction digits (right-padded with '0', since the printf may produce fewer).
 */
static int NumberFormatEmit(ph7_context *pCtx,
	const char *zDigits,int nDigits,int bNeg,
	const char *zFrac,int nFrac,int nDec,
	const char *zPoint,int nPoint,const char *zSep,int nSep)
{
	SyBlob sOut;
	int i;
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	if( bNeg ){
		SyBlobAppend(&sOut,"-",sizeof(char));
	}
	for( i = 0 ; i < nDigits ; ++i ){
		if( i > 0 && nSep > 0 && ((nDigits - i) % 3) == 0 ){
			SyBlobAppend(&sOut,zSep,(sxu32)nSep);
		}
		SyBlobAppend(&sOut,&zDigits[i],sizeof(char));
	}
	if( nDec > 0 ){
		if( nPoint > 0 ){
			SyBlobAppend(&sOut,zPoint,(sxu32)nPoint);
		}
		if( nFrac > nDec ){
			nFrac = nDec;
		}
		if( nFrac > 0 ){
			SyBlobAppend(&sOut,zFrac,(sxu32)nFrac);
		}
		for( i = nFrac ; i < nDec ; ++i ){
			SyBlobAppend(&sOut,"0",sizeof(char));
		}
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/*
 * _php_math_number_format_long(): an INTEGER never goes through a double, so
 * every digit of a value past 2^53 survives. A NEGATIVE $decimals rounds the
 * integer itself, half away from zero.
 */
static int NumberFormatLong(ph7_context *pCtx,sxi64 iVal,int nDec,
	const char *zPoint,int nPoint,const char *zSep,int nSep)
{
	char zBuf[32];
	sxu64 uNum;
	int bNeg = 0;
	int n = 0;
	if( iVal < 0 ){
		bNeg = 1;
		/* -PHP_INT_MIN does not fit; negate through the unsigned domain. */
		uNum = ((sxu64)-(iVal + 1)) + 1;
	}else{
		uNum = (sxu64)iVal;
	}
	if( nDec < 0 ){
		/* php keeps a table of the 20 powers of ten a 64-bit value can hold and
		 * answers 0 past it; 10^19 is the last one that fits. */
		if( nDec < -19 ){
			uNum = 0;
		}else{
			sxu64 uPow = 1;
			sxu64 uRest;
			int k;
			for( k = 0 ; k < -nDec ; ++k ){
				uPow *= 10;
			}
			uRest = uNum % uPow;
			uNum = uNum / uPow;
			uNum = (uRest >= uPow / 2) ? uNum * uPow + uPow : uNum * uPow;
		}
		if( uNum == 0 ){
			/* php never answers "-0". */
			bNeg = 0;
		}
	}
	/* Decimal digits, most significant first. */
	if( uNum == 0 ){
		zBuf[n++] = '0';
	}else{
		char zRev[32];
		int nRev = 0;
		while( uNum > 0 && nRev < (int)sizeof(zRev) ){
			zRev[nRev++] = (char)('0' + (int)(uNum % 10));
			uNum /= 10;
		}
		while( nRev > 0 ){
			zBuf[n++] = zRev[--nRev];
		}
	}
	return NumberFormatEmit(pCtx,zBuf,n,bNeg,0,0,nDec > 0 ? nDec : 0,
		zPoint,nPoint,zSep,nSep);
}
/*
 * string number_format(int|float $num,int $decimals = 0,
 *                      ?string $decimal_separator = ".",
 *                      ?string $thousands_separator = ",")
 *  Format a number with grouped thousands.
 */
PH7_PRIVATE int PH7_builtin_number_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPoint = ".", *zSep = ",";
	int nPoint = 1, nSep = 1;
	int nDec = 0;
	ph7_value sNum;
	double d;
	int bNeg = 0;
	int nLen,nInt;
	char *zFmt;
	const char *zDot;
	if( nArg < 1 ){
		/* Arity is enforced from aBuiltinSig[] before the call. */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Every refusal is worded here rather than by the shared screen: php's stub
	 * declares `float $num` (which is what Reflection prints) but the ZPP macro
	 * behind it is Z_PARAM_NUMBER, whose TypeError says `int|float`. An int stays
	 * an INT, a numeric string takes the shape it looks like, and null is §10's
	 * refusal of a deprecation. */
	if( !PH7_MemObjIsNumeric(apArg[0]) ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"number_format(): Argument #1 ($num) must be of type int|float, %s given",
			ph7_value_is_string(apArg[0]) ? "string"
				: VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));
	}
	if( nArg > 1 ){
		/* php declares `int $decimals`; the string and float narrowings it only
		 * DEPRECATES are rejected here (§10), as they are for count_chars(). */
		if( ph7_value_is_array(apArg[1]) || ph7_value_is_object(apArg[1])
		 || ph7_value_is_resource(apArg[1]) || ph7_value_is_null(apArg[1]) ){
			char zBuf[64];
			return PH7_VmThrowException(pCtx,"TypeError",
				"number_format(): Argument #2 ($decimals) must be of type int, %s given",
				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));
		}
		if( ph7_value_is_string(apArg[1]) ){
			/* php wants the WHOLE string to be numeric ("2abc" is a TypeError,
			 * not 2), and a float-shaped one that would LOSE something is §10's
			 * refusal of a deprecation. */
			double dMode;
			if( !PH7_MemObjStringIsNumeric(apArg[1]) ){
				return PH7_VmThrowException(pCtx,"TypeError",
					"number_format(): Argument #2 ($decimals) must be of type int, string given");
			}
			dMode = ph7_value_to_double(apArg[1]);
			if( dMode != (double)(sxi64)dMode ){
				return PH7_VmThrowException(pCtx,"TypeError",
					"number_format(): Argument #2 ($decimals) must be of type int, string given");
			}
		}else if( ph7_value_is_float(apArg[1]) ){
			double dMode = ph7_value_to_double(apArg[1]);
			if( dMode != (double)(sxi64)dMode ){
				return PH7_VmThrowException(pCtx,"TypeError",
					"number_format(): Argument #2 ($decimals) must be of type int, float given");
			}
		}
		{
			sxi64 iDec = ph7_value_to_int64(apArg[1]);
			/* php clamps the declared long onto an int before it formats. */
			nDec = iDec > 2147483647 ? 2147483647
			     : (iDec < -2147483647 ? -2147483647 : (int)iDec);
		}
	}
	/* Both separators are `?string`: null means php's default, not the empty
	 * string. An empty string IS accepted and simply omits the separator, and an
	 * object that can stringify is coerced. */
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		if( !PH7_ArgSatisfiesString(apArg[2]) ){
			char zBuf[64];
			return PH7_VmThrowException(pCtx,"TypeError",
				"number_format(): Argument #3 ($decimal_separator) must be of type ?string, %s given",
				VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));
		}
		zPoint = ph7_value_to_string(apArg[2],&nPoint);
	}
	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){
		if( !PH7_ArgSatisfiesString(apArg[3]) ){
			char zBuf[64];
			return PH7_VmThrowException(pCtx,"TypeError",
				"number_format(): Argument #4 ($thousands_separator) must be of type ?string, %s given",
				VmValueGivenName(apArg[3],zBuf,sizeof(zBuf)));
		}
		zSep = ph7_value_to_string(apArg[3],&nSep);
	}
	PH7_MemObjInit(pCtx->pVm,&sNum);
	PH7_MemObjStore(apArg[0],&sNum);
	PH7_MemObjToNumeric(&sNum);
	if( (sNum.iFlags & MEMOBJ_REAL) == 0 ){
		int rc = NumberFormatLong(pCtx,sNum.x.iVal,nDec,zPoint,nPoint,zSep,nSep);
		PH7_MemObjRelease(&sNum);
		return rc;
	}
	d = (double)sNum.rVal;
	PH7_MemObjRelease(&sNum);
	/* A double past 2^52 has no fractional digits left, so php formats it as an
	 * INTEGER when it fits one — that is what keeps 4503599627370496.0 exact. */
	if( (d >= 4503599627370496.0 || d <= -4503599627370496.0)
	 && d >= -9223372036854775808.0 && d < 9223372036854775808.0 ){
		return NumberFormatLong(pCtx,(sxi64)d,nDec,zPoint,nPoint,zSep,nSep);
	}
	if( d < 0 ){
		bNeg = 1;
		d = -d;
	}
	d = MathRound(d,nDec,PH7_ROUND_HALF_UP);
	if( nDec < 0 ){
		nDec = 0;
	}
	/* libc's %f, not the engine's formatter: php prints through its own
	 * snprintf here, so INF answers "inf" and NAN "nan" — and the engine's
	 * formatter caps the precision at 53 digits with a notice, where php
	 * honours whatever $decimals asks for. */
	nLen = snprintf(0,0,"%.*f",nDec,d);
	if( nLen < 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	zFmt = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen + 1,FALSE,TRUE);
	if( zFmt == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	snprintf(zFmt,(size_t)nLen + 1,"%.*f",nDec,d);
	if( zFmt[0] < '0' || zFmt[0] > '9' ){
		/* Not a number at all (inf/nan): php hands its buffer straight back,
		 * without a sign, a separator or any padding. */
		ph7_result_string(pCtx,zFmt,nLen);
		return PH7_OK;
	}
	if( bNeg && d == 0 ){
		/* Rounded away to zero; php never answers "-0". */
		bNeg = 0;
	}
	/* php looks for '.' OR ',' — the decimal point its formatter produced. */
	zDot = 0;
	if( nDec > 0 ){
		int i;
		for( i = 0 ; i < nLen ; ++i ){
			if( zFmt[i] == '.' || zFmt[i] == ',' ){
				zDot = &zFmt[i];
				break;
			}
		}
	}
	nInt = zDot ? (int)(zDot - zFmt) : nLen;
	return NumberFormatEmit(pCtx,zFmt,nInt,bNeg,
		zDot ? zDot + 1 : 0,zDot ? nLen - nInt - 1 : 0,
		nDec,zPoint,nPoint,zSep,nSep);
}
/*
 * int intdiv(int $a, int $b)
 *  Integer division.
 * Parameters
 *  $a
 *   Number to be divided.
 *  $b
 *   Number which divides the $a.
 * Return
 *  The integer quotient of the division of $a by $b.
 */
PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 a,b;
	/* PHP requires exactly two arguments. */
	if( nArg != 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"intdiv() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	/* Type-check argument 1 */
	if( ph7_value_is_array(apArg[0]) || ph7_value_is_object(apArg[0])
		|| ph7_value_is_resource(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"intdiv(): Argument #1 ($num1) must be of type int, %s given",
			ph7_type_name(apArg[0])
			);
	}
	if( ph7_value_is_string(apArg[0]) ){
		int len;
		const char *zStr = ph7_value_to_string(apArg[0], &len);
		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"intdiv(): Argument #1 ($num1) must be of type int, string given"
				);
		}
	}
	/* Type-check argument 2 */
	if( ph7_value_is_array(apArg[1]) || ph7_value_is_object(apArg[1])
		|| ph7_value_is_resource(apArg[1]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"intdiv(): Argument #2 ($num2) must be of type int, %s given",
			ph7_type_name(apArg[1])
			);
	}
	if( ph7_value_is_string(apArg[1]) ){
		int len;
		const char *zStr = ph7_value_to_string(apArg[1], &len);
		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"intdiv(): Argument #2 ($num2) must be of type int, string given"
				);
		}
	}
	/* Convert both arguments to int64 */
	{
		/* php's ZPP contract for the two int params (lossy float / float-string
		 * deprecations); the manual type checks above already covered arrays,
		 * objects and non-numeric strings with the same messages. */
		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[0],"intdiv",1,"$num1","int",&a);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
		rcArg = PH7_IntArgResolve(pCtx,apArg[1],"intdiv",2,"$num2","int",&b);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
	}
	/* Check for division by zero */
	if( b == 0 ){
		return PH7_VmThrowException(pCtx,
			"DivisionByZeroError",
			"Division by zero"
			);
	}
	/* Check for overflow: PHP_INT_MIN / -1 */
	if( a == SMALLEST_INT64 && b == -1 ){
		return PH7_VmThrowException(pCtx,
			"ArithmeticError",
			"Division of PHP_INT_MIN by -1 is not an integer"
			);
	}
	/* Perform integer division */
	ph7_result_int64(pCtx, a / b);
	return PH7_OK;
}
/*
 * string dechex(int $number)
 *  Decimal to hexadecimal.
 * Parameters
 *  $number
 *   Decimal value to convert
 * Return
 *  Hexadecimal string representation of number
 */
PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 iVal;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */
	iVal = ph7_value_to_int64(apArg[0]);
	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement,
	 * so a negative value prints all 16 hex digits like PHP. */
	ph7_result_string_format(pCtx,"%qx",iVal);
	return PH7_OK;
}
/*
 * string decoct(int $number)
 *  Decimal to Octal.
 * Parameters
 *  $number
 *   Decimal value to convert
 * Return
 *  Octal string representation of number
 */
PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 iVal;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */
	iVal = ph7_value_to_int64(apArg[0]);
	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */
	ph7_result_string_format(pCtx,"%qo",iVal);
	return PH7_OK;
}
/*
 * string decbin(int $number)
 *  Decimal to binary.
 * Parameters
 *  $number
 *   Decimal value to convert
 * Return
 *  Binary string representation of number
 */
PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 iVal;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */
	iVal = ph7_value_to_int64(apArg[0]);
	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */
	ph7_result_string_format(pCtx,"%qB",iVal);
	return PH7_OK;
}
/*
 * Convert a base-2/8/16 digit string to a number, mirroring PHP's
 * _php_math_basetozval (ext/standard/math.c) so hexdec/octdec/bindec agree with
 * php byte-for-byte: walk every byte, decode a digit (0-9,a-z,A-Z) or skip any
 * invalid one, accumulate into a signed 64-bit integer and transparently promote
 * to a double once the value would overflow PHP_INT_MAX. The context result is
 * set to an int when it fits, otherwise a float — PHP returns a float for values
 * above PHP_INT_MAX (e.g. hexdec("ffffffffffffffff") == 1.8446744073709552E+19).
 * A byte >= 0x80 (e.g. a UTF-8 continuation) matches none of the digit ranges and
 * is skipped, so leading/interior multibyte junk is ignored like php.
 * Note: php also raises E_DEPRECATED for skipped invalid characters; that notice
 * is not emitted here (a §3.7 deprecation-fidelity residual, value is correct).
 */
static void MathBaseToNumber(ph7_context *pCtx,const char *zStr,int nLen,int base)
{
	sxi64 num = 0;      /* Integer accumulator */
	double fnum = 0;    /* Float accumulator (used once num would overflow) */
	int mode = 0;       /* 0 -> integer accumulation, 1 -> switched to float */
	sxi64 cutoff = SXI64_HIGH / base;      /* PHP_INT_MAX / base */
	int cutlim = (int)(SXI64_HIGH % base); /* PHP_INT_MAX % base */
	int bIgnored = 0;   /* any character skipped below? php deprecates that */
	int i;
	for( i = 0 ; i < nLen ; ++i ){
		int c = (unsigned char)zStr[i];
		if( c >= '0' && c <= '9' ){
			c -= '0';
		}else if( c >= 'A' && c <= 'Z' ){
			c -= 'A' - 10;
		}else if( c >= 'a' && c <= 'z' ){
			c -= 'a' - 10;
		}else{
			bIgnored = 1;
			continue; /* Not a digit character: skip */
		}
		if( c >= base ){
			bIgnored = 1;
			continue; /* Digit out of range for this base: skip */
		}
		if( mode == 0 ){
			if( num < cutoff || (num == cutoff && c <= cutlim) ){
				num = num * base + c;
				continue;
			}
			/* Adding this digit would overflow the 64-bit integer: fall back to
			 * float accumulation, seeding it with the value gathered so far. */
			fnum = (double)num;
			mode = 1;
		}
		fnum = fnum * base + c;
	}
	if( bIgnored ){
		/* php 8 skips characters that are not valid digits for this base and only
		 * DEPRECATES the skipping; §10 rejects the deprecated surface loudly, so this
		 * ValueError ABORTS the call (the result stored below never reaches the caller
		 * — the OP_CALL boundary reports the throw for us, VmHostFuncThrowRc).
		 * Twin-pinned by base_invalid_chars_abort{,_zend}.phpt. */
		PH7_VmThrowException(pCtx,"ValueError",
			"Invalid characters passed for attempted conversion");
		return;
	}
	if( mode == 1 ){
		ph7_result_double(pCtx,fnum);
	}else{
		ph7_result_int64(pCtx,num);
	}
}
/*
 * int64 hexdec(string $hex_string)
 *  Hexadecimal to decimal.
 * Parameters
 *  $hex_string
 *   The hexadecimal string to convert
 * Return
 *  The decimal representation of hex_string (int, or float on overflow)
 */
PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	if( ph7_value_is_array(apArg[0]) || ph7_value_is_object(apArg[0]) || ph7_value_is_resource(apArg[0]) ){
		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"hexdec(): Argument #1 ($hex_string) must be of type string, %s given",
			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));
	}
	/* PHP's `string` ZPP renders scalars/null to their string form and then
	 * hex-parses that (hexdec(255) == hexdec("255") == 0x255), so route every
	 * non-throwing value through ph7_value_to_string rather than reading it as
	 * a decimal integer. */
	zString = ph7_value_to_string(apArg[0],&nLen);
	MathBaseToNumber(pCtx,zString,nLen,16);
	return PH7_OK;
}
/*
 * int64 bindec(string $bin_string)
 *  Binary to decimal.
 * Parameters
 *  $bin_string
 *   The binary string to convert
 * Return
 *  Returns the decimal equivalent of the binary number represented by the binary_string argument.
 */
PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	if( ph7_value_is_array(apArg[0]) || ph7_value_is_object(apArg[0]) || ph7_value_is_resource(apArg[0]) ){
		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"bindec(): Argument #1 ($binary_string) must be of type string, %s given",
			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));
	}
	/* PHP's `string` ZPP renders scalars/null to their string form and then
	 * binary-parses that (bindec(11) == bindec("11") == 3). */
	zString = ph7_value_to_string(apArg[0],&nLen);
	MathBaseToNumber(pCtx,zString,nLen,2);
	return PH7_OK;
}
/*
 * int64 octdec(string $oct_string)
 *  Octal to decimal.
 * Parameters
 *  $oct_string
 *   The octal string to convert
 * Return
 *  Returns the decimal equivalent of the octal number represented by the octal_string argument.
 */
PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	if( ph7_value_is_array(apArg[0]) || ph7_value_is_object(apArg[0]) || ph7_value_is_resource(apArg[0]) ){
		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"octdec(): Argument #1 ($octal_string) must be of type string, %s given",
			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));
	}
	/* PHP's `string` ZPP renders scalars/null to their string form and then
	 * octal-parses that (octdec(11) == octdec("11") == 9). */
	zString = ph7_value_to_string(apArg[0],&nLen);
	MathBaseToNumber(pCtx,zString,nLen,8);
	return PH7_OK;
}
/*
 * srand([int $seed])
 * mt_srand([int $seed])
 *  Seed the random number generator.
 * Parameters
 * $seed
 *  Optional seed value. php truncates it to 32 bits; a missing seed reseeds
 *  from OS entropy (a "random" seed), matching php's GENERATE_SEED().
 * Return
 *  null.
 * Note:
 *  srand()/mt_srand() are aliases (php 7.1+ backs both rand() and mt_rand()
 *  with the same MT19937). They reset only the userland generator, never the
 *  engine's internal RC4 entropy, so a seed makes rand()/mt_rand()/shuffle/
 *  str_shuffle/array_rand reproducible without disturbing object ids or uniqid.
 */
PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxu32 nSeed;
	if( nArg > 0 ){
		/* php truncates the (weakly int-coerced) seed to 32 bits. */
		nSeed = (sxu32)ph7_value_to_int64(apArg[0]);
	}else{
		/* No seed: reseed from OS entropy, like php's GENERATE_SEED(). */
		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){
			nSeed = PH7_VmRandomNum(pCtx->pVm);
		}
	}
	PH7_VmMtSrand(pCtx->pVm,nSeed);
	ph7_result_null(pCtx);
	return PH7_OK;
}
#ifndef PH7_DISABLE_DISK_IO
/*
 * string base_convert(string $number,int $frombase,int $tobase)
 *  Convert a number between arbitrary bases.
 * Parameters
 * $number
 *  The number to convert
 * $frombase
 *  The base number is in
 * $tobase
 *  The base to convert number to
 * Return
 *  Number converted to base tobase
 */
PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const char zDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	int nLen,iFbase,iTobase,i;
	int bIgnored;
	ph7_int64 iFbase64,iTobase64;
	const char *zNum;
	sxu64 uNum = 0;
	if( nArg < 3 ){
		/* Return the empty string*/
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Base numbers. Read them as 64-bit so an out-of-range base can't wrap through
	 * a 32-bit truncation back into the 2..36 window and bypass the check below. */
	iFbase64 = ph7_value_to_int64(apArg[1]);
	iTobase64 = ph7_value_to_int64(apArg[2]);
	/* PHP 8 throws a catchable ValueError for a base outside 2..36; from_base
	 * is validated before to_base, both before the string is even parsed. */
	if( iFbase64 < 2 || iFbase64 > 36 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)");
	}
	if( iTobase64 < 2 || iTobase64 > 36 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)");
	}
	/* Both bases are now known to fit in [2,36], so the int form is exact. */
	iFbase  = (int)iFbase64;
	iTobase = (int)iTobase64;
	/* Parse the input number in from_base. Every base is handled the same way:
	 * digits 0-9 then a-z/A-Z map to 0-35; a character that is not a valid digit for
	 * from_base is ignored, and php raises an E_DEPRECATED saying so. */
	if( ph7_value_is_null(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"base_convert(): Argument #1 ($num) must be of type string, null given");
	}
	zNum = ph7_value_to_string(apArg[0],&nLen);
	bIgnored = 0;
	for( i = 0 ; i < nLen ; ++i ){
		int c = (unsigned char)zNum[i];
		int d;
		if( c >= '0' && c <= '9' ){
			d = c - '0';
		}else if( c >= 'a' && c <= 'z' ){
			d = c - 'a' + 10;
		}else if( c >= 'A' && c <= 'Z' ){
			d = c - 'A' + 10;
		}else{
			d = 99;
		}
		if( d >= iFbase ){
			/* Not a valid digit for this base: php skips it and deprecates the skip. */
			bIgnored = 1;
			continue;
		}
		uNum = uNum * (sxu64)iFbase + (sxu64)d;
	}
	if( bIgnored ){
		/* §10 rejects php's deprecated surface loudly, and a throw ABORTS the call —
		 * the conversion below is not reached. See MathBaseToNumber's twin. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"Invalid characters passed for attempted conversion");
	}
	/* Format the result in to_base using lowercase digits. */
	if( uNum == 0 ){
		ph7_result_string(pCtx,"0",1);
	}else{
		char zOut[70]; /* base-2 of a 64-bit value fits in 64 digits */
		int n = 0,j;
		while( uNum > 0 ){
			zOut[n++] = zDigits[uNum % (sxu64)iTobase];
			uNum /= (sxu64)iTobase;
		}
		/* Digits were produced least-significant first: reverse in place. */
		for( j = 0 ; j < n/2 ; ++j ){
			char t = zOut[j];
			zOut[j] = zOut[n - 1 - j];
			zOut[n - 1 - j] = t;
		}
		ph7_result_string(pCtx,zOut,n);
	}
	return PH7_OK;
}
#endif /* PH7_DISABLE_DISK_IO */
#endif /* PH7_DISABLE_BUILTIN_FUNC */
