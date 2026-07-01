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
		int r,x;
		x = ph7_value_to_int(apArg[0]);
		/* Perform the requested operation */
		r = abs(x);
		ph7_result_int(pCtx,r);
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
		 * libc snprintf/strtod are used (not SyBufferFormat/SyStrToReal,
		 * which are not correctly-rounded) so the low bits match PHP — the
		 * same reason vm_serialize.c uses libc strtod for its float repr.
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
	a = ph7_value_to_int64(apArg[0]);
	b = ph7_value_to_int64(apArg[1]);
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
	int iVal;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the given number */
	iVal = ph7_value_to_int(apArg[0]);
	/* Format */
	ph7_result_string_format(pCtx,"%x",iVal);
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
	int iVal;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the given number */
	iVal = ph7_value_to_int(apArg[0]);
	/* Format */
	ph7_result_string_format(pCtx,"%o",iVal);
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
	int iVal;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the given number */
	iVal = ph7_value_to_int(apArg[0]);
	/* Format */
	ph7_result_string_format(pCtx,"%B",iVal);
	return PH7_OK;
}
/*
 * int64 hexdec(string $hex_string)
 *  Hexadecimal to decimal.
 * Parameters
 *  $hex_string
 *   The hexadecimal string to convert
 * Return
 *  The decimal representation of hex_string
 */
PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zEnd;
	ph7_int64 iVal;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	iVal = 0;
	if( ph7_value_is_string(apArg[0]) ){
		/* Extract the given string */
		zString = ph7_value_to_string(apArg[0],&nLen);
		/* Delimit the string */
		zEnd = &zString[nLen];
		/* Ignore non hex-stream */
		while( zString < zEnd ){
			if( (unsigned char)zString[0] >= 0xc0 ){
				/* UTF-8 stream */
				zString++;
				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){
					zString++;
				}
			}else{
				if( SyisHex(zString[0]) ){
					break;
				}
				/* Ignore */
				zString++;
			}
		}
		if( zString < zEnd ){
			/* Cast */
			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);
		}
	}else{
		/* Extract as a 64-bit integer */
		iVal = ph7_value_to_int64(apArg[0]);
	}
	/* Return the number */
	ph7_result_int64(pCtx,iVal);
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
	ph7_int64 iVal;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	iVal = 0;
	if( ph7_value_is_string(apArg[0]) ){
		/* Extract the given string */
		zString = ph7_value_to_string(apArg[0],&nLen);
		if( nLen > 0 ){
			/* Perform a binary cast */
			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);
		}
	}else{
		/* Extract as a 64-bit integer */
		iVal = ph7_value_to_int64(apArg[0]);
	}
	/* Return the number */
	ph7_result_int64(pCtx,iVal);
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
	ph7_int64 iVal;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	iVal = 0;
	if( ph7_value_is_string(apArg[0]) ){
		/* Extract the given string */
		zString = ph7_value_to_string(apArg[0],&nLen);
		if( nLen > 0 ){
			/* Perform the cast */
			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);
		}
	}else{
		/* Extract as a 64-bit integer */
		iVal = ph7_value_to_int64(apArg[0]);
	}
	/* Return the number */
	ph7_result_int64(pCtx,iVal);
	return PH7_OK;
}
/*
 * srand([int $seed])
 * mt_srand([int $seed])
 *  Seed the random number generator.
 * Parameters
 * $seed
 *  Optional seed value
 * Return
 *  null.
 * Note:
 *  THIS FUNCTION IS A NO-OP.
 *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.
 */
PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
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
	int nLen,iFbase,iTobase;
	const char *zNum;
	ph7_int64 iNum;
	if( nArg < 3 ){
		/* Return the empty string*/
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Base numbers */
	iFbase  = ph7_value_to_int(apArg[1]);
	iTobase = ph7_value_to_int(apArg[2]);
	if( ph7_value_is_string(apArg[0]) ){
		/* Extract the target number */
		zNum = ph7_value_to_string(apArg[0],&nLen);
		if( nLen < 1 ){
			/* Return the empty string*/
			ph7_result_string(pCtx,"",0);
			return PH7_OK;
		}
		/* Base conversion */
		switch(iFbase){
		case 16:
			/* Hex */
			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);
			break;
		case 8:
			/* Octal */
			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);
			break;
		case 2:
			/* Binary */
			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);
			break;
		default:
			/* Decimal */
			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);
			break;
		}
	}else{
		iNum = ph7_value_to_int64(apArg[0]);
	}
	switch(iTobase){
	case 16:
		/* Hex */
		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */
		break;
	case 8:
		/* Octal */
		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */
		break;
	case 2:
		/* Binary */
		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */
		break;
	default:
		/* Decimal */
		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */
		break;
	}
	return PH7_OK;
}
#endif /* PH7_DISABLE_DISK_IO */
#endif /* PH7_DISABLE_BUILTIN_FUNC */
