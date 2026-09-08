# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 557/655 lines (85.04%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `/*` |
|    - |    8 | ` * round() (defined below, under PH7_DISABLE_BUILTIN_FUNC rather than the` |
|    - |    9 | ` * math-func guard) needs floor/ceil/fabs/copysign/fmod/isfinite/pow plus the` |
|    - |   10 | ` * libc snprintf/strtod round-trip for its high-precision branch, so pull these` |
|    - |   11 | ` * in unconditionally here — they must be available even when` |
|    - |   12 | ` * PH7_ENABLE_MATH_FUNC is off. abs() is also used by the guarded math builtins.` |
|    - |   13 | ` */` |
|    - |   14 | `#include <math.h>` |
|    - |   15 | `#include <stdio.h>  /* snprintf: correctly-rounded high-precision round() round-trip */` |
|    - |   16 | `#include <stdlib.h> /* strtod (round-trip inverse), abs */` |
|    - |   17 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|    - |   18 |  |
|    - |   19 | `/*` |
|    - |   20 | ` * Section:` |
|    - |   21 | ` *    Math Functions.` |
|    - |   22 |  |
|    - |   23 | ` * Status:` |
|    - |   24 | ` *    Stable.` |
|    - |   25 | ` */` |
|    - |   26 | `/*` |
|    - |   27 | ` * float sqrt(float $arg )` |
|    - |   28 | ` *  Square root of the given number.` |
|    - |   29 | ` * Parameter` |
|    - |   30 | ` *  The number to process.` |
|    - |   31 | ` * Return` |
|    - |   32 | ` *  The square root of arg or the special value Nan of failure.` |
|    - |   33 | ` */` |
|    - |   34 | `/*` |
|    - |   35 | ` * The libm-backed float functions php has and PH7 lacked entirely. Doing these in PHP` |
|    - |   36 | ` * (log1p as log(1+x), acosh via logs, ...) would lose precision, so they go through libm.` |
|    - |   37 | ` */` |
|    - |   38 | `#define PH7_MATH_UNARY(NAME,CFUNC)                                        \` |
|    - |   39 | `PH7_PRIVATE int PH7_builtin_##NAME(ph7_context *pCtx,int nArg,ph7_value **apArg) \` |
|    - |   40 | `{                                                                          \` |
|    - |   41 | `	double x;                                                              \` |
|    - |   42 | `	if( nArg < 1 ){                                                        \` |
|    - |   43 | `		ph7_result_int(pCtx,0);                                            \` |
|    - |   44 | `		return PH7_OK;                                                     \` |
|    - |   45 | `	}                                                                      \` |
|    - |   46 | `	x = ph7_value_to_double(apArg[0]);                                     \` |
|    - |   47 | `	ph7_result_double(pCtx,CFUNC(x));                                      \` |
|    - |   48 | `	return PH7_OK;                                                          \` |
|    - |   49 | `}` |
|    3 |   50 | `PH7_MATH_UNARY(acosh,acosh)` |
|    3 |   51 | `PH7_MATH_UNARY(asinh,asinh)` |
|    3 |   52 | `PH7_MATH_UNARY(atanh,atanh)` |
|    3 |   53 | `PH7_MATH_UNARY(expm1,expm1)` |
|    3 |   54 | `PH7_MATH_UNARY(log1p,log1p)` |
|    2 |   55 | `PH7_PRIVATE int PH7_builtin_deg2rad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   56 | `{` |
|    - |   57 | `	double x;` |
|    3 |   58 | `	if( nArg < 1 ){` |
|  ! 0 |   59 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   60 | `		return PH7_OK;` |
|    - |   61 | `	}` |
|    3 |   62 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |   63 | `	ph7_result_double(pCtx,x * (3.14159265358979323846 / 180.0));` |
|    3 |   64 | `	return PH7_OK;` |
|    2 |   65 | `}` |
|    4 |   66 | `PH7_PRIVATE int PH7_builtin_rad2deg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   67 | `{` |
|    - |   68 | `	double x;` |
|    5 |   69 | `	if( nArg < 1 ){` |
|  ! 0 |   70 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   71 | `		return PH7_OK;` |
|    - |   72 | `	}` |
|    5 |   73 | `	x = ph7_value_to_double(apArg[0]);` |
|    5 |   74 | `	ph7_result_double(pCtx,x * (180.0 / 3.14159265358979323846));` |
|    5 |   75 | `	return PH7_OK;` |
|    3 |   76 | `}` |
|    2 |   77 | `PH7_PRIVATE int PH7_builtin_fpow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   78 | `{` |
|    - |   79 | `	double x,y;` |
|    3 |   80 | `	if( nArg < 2 ){` |
|  ! 0 |   81 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   82 | `		return PH7_OK;` |
|    - |   83 | `	}` |
|    3 |   84 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |   85 | `	y = ph7_value_to_double(apArg[1]);` |
|    3 |   86 | `	ph7_result_double(pCtx,pow(x,y));` |
|    3 |   87 | `	return PH7_OK;` |
|    2 |   88 | `}` |
|    2 |   89 | `PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   90 | `{` |
|    - |   91 | `	double r,x;` |
|    3 |   92 | `	if( nArg < 1 ){` |
|    - |   93 | `		/* Missing argument,return 0 */` |
|  ! 0 |   94 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   95 | `		return PH7_OK;` |
|    - |   96 | `	}` |
|    3 |   97 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |   98 | `	/* Perform the requested operation */` |
|    3 |   99 | `	r = sqrt(x);` |
|    - |  100 | `	/* store the result back */` |
|    3 |  101 | `	ph7_result_double(pCtx,r);` |
|    3 |  102 | `	return PH7_OK;` |
|    2 |  103 | `}` |
|    - |  104 | `/*` |
|    - |  105 | ` * float exp(float $arg )` |
|    - |  106 | ` *  Calculates the exponent of e.` |
|    - |  107 | ` * Parameter` |
|    - |  108 | ` *  The number to process.` |
|    - |  109 | ` * Return` |
|    - |  110 | ` *  'e' raised to the power of arg.` |
|    - |  111 | ` */` |
|   18 |  112 | `PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  113 | `{` |
|    - |  114 | `	double r,x;` |
|   19 |  115 | `	if( nArg < 1 ){` |
|    - |  116 | `		/* Missing argument,return 0 */` |
|  ! 0 |  117 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  118 | `		return PH7_OK;` |
|    - |  119 | `	}` |
|   19 |  120 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  121 | `	/* Perform the requested operation */` |
|   19 |  122 | `	r = exp(x);` |
|    - |  123 | `	/* store the result back */` |
|   19 |  124 | `	ph7_result_double(pCtx,r);` |
|   19 |  125 | `	return PH7_OK;` |
|   10 |  126 | `}` |
|    - |  127 | `/*` |
|    - |  128 | ` * float floor(float $arg )` |
|    - |  129 | ` *  Round fractions down.` |
|    - |  130 | ` * Parameter` |
|    - |  131 | ` *  The number to process.` |
|    - |  132 | ` * Return` |
|    - |  133 | ` *  Returns the next lowest integer value by rounding down value if necessary.` |
|    - |  134 | ` */` |
|   16 |  135 | `PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  136 | `{` |
|    - |  137 | `	double r,x;` |
|    - |  138 | `	/* PHP requires exactly one argument. */` |
|   19 |  139 | `	if( nArg != 1 ){` |
|    4 |  140 | `		return PH7_VmThrowException(pCtx,` |
|    - |  141 | `			"ArgumentCountError",` |
|    - |  142 | `			"floor() expects exactly 1 argument, %d given",` |
|    1 |  143 | `			nArg` |
|    - |  144 | `			);` |
|    - |  145 | `	}` |
|    - |  146 | `	/*` |
|    - |  147 | `	 * Validate argument type. Only int/float (and numeric strings) are accepted.` |
|    - |  148 | `	 * Other types (including non-numeric strings) raise a TypeError just like` |
|    - |  149 | `	 * ceil() and other math functions.` |
|    - |  150 | `	 */` |
|   16 |  151 | `	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){` |
|    6 |  152 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |  153 | `			int len;` |
|    6 |  154 | `			sxu8 bReal = FALSE;` |
|    6 |  155 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  156 | `			sxi32 rcNum;` |
|    6 |  157 | `			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|    6 |  158 | `			if( rcNum != SXRET_OK ){` |
|    4 |  159 | `				return PH7_VmThrowException(pCtx,` |
|    - |  160 | `					"TypeError",` |
|    - |  161 | `					"floor(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |  162 | `					ph7_type_name(apArg[0])` |
|    - |  163 | `					);` |
|    - |  164 | `			}` |
|    2 |  165 | `		}else{` |
|    - |  166 | `			/* Disallow all other types (arrays, objects, resources, etc.) */` |
|  ! 0 |  167 | `			return PH7_VmThrowException(pCtx,` |
|    - |  168 | `				"TypeError",` |
|    - |  169 | `				"floor(): Argument #1 ($num) must be of type int\|float, %s given",` |
|  ! 0 |  170 | `				ph7_type_name(apArg[0])` |
|    - |  171 | `				);` |
|    - |  172 | `		}` |
|    1 |  173 | `	}` |
|    - |  174 |  |
|   13 |  175 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  176 | `	/* Perform the requested operation */` |
|   13 |  177 | `	r = floor(x);` |
|    - |  178 | `	/* store the result back */` |
|   13 |  179 | `	ph7_result_double(pCtx,r);` |
|   13 |  180 | `	return PH7_OK;` |
|   11 |  181 | `}` |
|    - |  182 | `/*` |
|    - |  183 | ` * float cos(float $arg )` |
|    - |  184 | ` *  Cosine.` |
|    - |  185 | ` * Parameter` |
|    - |  186 | ` *  The number to process.` |
|    - |  187 | ` * Return` |
|    - |  188 | ` *  The cosine of arg.` |
|    - |  189 | ` */` |
|    2 |  190 | `PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  191 | `{` |
|    - |  192 | `	double r,x;` |
|    3 |  193 | `	if( nArg < 1 ){` |
|    - |  194 | `		/* Missing argument,return 0 */` |
|  ! 0 |  195 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  196 | `		return PH7_OK;` |
|    - |  197 | `	}` |
|    3 |  198 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  199 | `	/* Perform the requested operation */` |
|    3 |  200 | `	r = cos(x);` |
|    - |  201 | `	/* store the result back */` |
|    3 |  202 | `	ph7_result_double(pCtx,r);` |
|    3 |  203 | `	return PH7_OK;` |
|    2 |  204 | `}` |
|    - |  205 | `/*` |
|    - |  206 | ` * float acos(float $arg )` |
|    - |  207 | ` *  Arc cosine.` |
|    - |  208 | ` * Parameter` |
|    - |  209 | ` *  The number to process.` |
|    - |  210 | ` * Return` |
|    - |  211 | ` *  The arc cosine of arg.` |
|    - |  212 | ` */` |
|   18 |  213 | `PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  214 | `{` |
|    - |  215 | `	double r, x;` |
|    - |  216 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|   20 |  217 | `	if( nArg != 1 ){` |
|  ! 0 |  218 | `		return PH7_VmThrowException(pCtx,` |
|    - |  219 | `			"ArgumentCountError",` |
|    - |  220 | `			"acos() expects exactly 1 argument, %d given",` |
|  ! 0 |  221 | `			nArg` |
|    - |  222 | `			);` |
|    - |  223 | `	}` |
|    - |  224 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|    - |  225 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|    - |  226 | `	 * the float conversion will handle them. */` |
|   20 |  227 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    4 |  228 | `		return PH7_VmThrowException(pCtx,` |
|    - |  229 | `			"TypeError",` |
|    - |  230 | `			"acos(): Argument #1 ($num) must be of type float, %s given",` |
|    1 |  231 | `			ph7_type_name(apArg[0])` |
|    - |  232 | `			);` |
|    - |  233 | `	}` |
|    - |  234 | `	/* Convert to double now that we know it's numeric. */` |
|   17 |  235 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  236 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|   17 |  237 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|    5 |  238 | `		r = PH7_NAN_VALUE();` |
|    3 |  239 | `	}else{` |
|   13 |  240 | `		r = acos(x);` |
|    - |  241 | `	}` |
|    - |  242 | `	/* store the result back */` |
|   17 |  243 | `	ph7_result_double(pCtx,r);` |
|   17 |  244 | `	return PH7_OK;` |
|   11 |  245 | `}` |
|    - |  246 | `/*` |
|    - |  247 | ` * float cosh(float $arg )` |
|    - |  248 | ` *  Hyperbolic cosine.` |
|    - |  249 | ` * Parameter` |
|    - |  250 | ` *  The number to process.` |
|    - |  251 | ` * Return` |
|    - |  252 | ` *  The hyperbolic cosine of arg.` |
|    - |  253 | ` */` |
|   16 |  254 | `PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  255 | `{` |
|    - |  256 | `	double r,x;` |
|   17 |  257 | `	if( nArg < 1 ){` |
|    - |  258 | `		/* Missing argument,return 0 */` |
|  ! 0 |  259 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  260 | `		return PH7_OK;` |
|    - |  261 | `	}` |
|   17 |  262 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  263 | `	/* Perform the requested operation */` |
|   17 |  264 | `	r = cosh(x);` |
|    - |  265 | `	/* store the result back */` |
|   17 |  266 | `	ph7_result_double(pCtx,r);` |
|   17 |  267 | `	return PH7_OK;` |
|    9 |  268 | `}` |
|    - |  269 | `/*` |
|    - |  270 | ` * float sin(float $arg )` |
|    - |  271 | ` *  Sine.` |
|    - |  272 | ` * Parameter` |
|    - |  273 | ` *  The number to process.` |
|    - |  274 | ` * Return` |
|    - |  275 | ` *  The sine of arg.` |
|    - |  276 | ` */` |
|    2 |  277 | `PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  278 | `{` |
|    - |  279 | `	double r,x;` |
|    3 |  280 | `	if( nArg < 1 ){` |
|    - |  281 | `		/* Missing argument,return 0 */` |
|  ! 0 |  282 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  283 | `		return PH7_OK;` |
|    - |  284 | `	}` |
|    3 |  285 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  286 | `	/* Perform the requested operation */` |
|    3 |  287 | `	r = sin(x);` |
|    - |  288 | `	/* store the result back */` |
|    3 |  289 | `	ph7_result_double(pCtx,r);` |
|    3 |  290 | `	return PH7_OK;` |
|    2 |  291 | `}` |
|    - |  292 | `/*` |
|    - |  293 | ` * float asin(float $arg )` |
|    - |  294 | ` *  Arc sine.` |
|    - |  295 | ` * Parameter` |
|    - |  296 | ` *  The number to process.` |
|    - |  297 | ` * Return` |
|    - |  298 | ` *  The arc sine of arg.` |
|    - |  299 | ` */` |
|   18 |  300 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  301 | `{` |
|    - |  302 | `	double r, x;` |
|    - |  303 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|   20 |  304 | `	if( nArg != 1 ){` |
|  ! 0 |  305 | `		return PH7_VmThrowException(pCtx,` |
|    - |  306 | `			"ArgumentCountError",` |
|    - |  307 | `			"asin() expects exactly 1 argument, %d given",` |
|  ! 0 |  308 | `			nArg` |
|    - |  309 | `			);` |
|    - |  310 | `	}` |
|    - |  311 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|    - |  312 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|    - |  313 | `	 * the float conversion will handle them. */` |
|   20 |  314 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    4 |  315 | `		return PH7_VmThrowException(pCtx,` |
|    - |  316 | `			"TypeError",` |
|    - |  317 | `			"asin(): Argument #1 ($num) must be of type float, %s given",` |
|    1 |  318 | `			ph7_type_name(apArg[0])` |
|    - |  319 | `			);` |
|    - |  320 | `	}` |
|    - |  321 | `	/* Convert to double now that we know it's numeric. */` |
|   17 |  322 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  323 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|   17 |  324 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|    5 |  325 | `		r = PH7_NAN_VALUE();` |
|    3 |  326 | `	}else{` |
|   13 |  327 | `		r = asin(x);` |
|    - |  328 | `	}` |
|    - |  329 | `	/* store the result back */` |
|   17 |  330 | `	ph7_result_double(pCtx,r);` |
|   17 |  331 | `	return PH7_OK;` |
|   11 |  332 | `}` |
|    - |  333 | `/*` |
|    - |  334 | ` * float sinh(float $arg )` |
|    - |  335 | ` *  Hyperbolic sine.` |
|    - |  336 | ` * Parameter` |
|    - |  337 | ` *  The number to process.` |
|    - |  338 | ` * Return` |
|    - |  339 | ` *  The hyperbolic sine of arg.` |
|    - |  340 | ` */` |
|   18 |  341 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  342 | `{` |
|    - |  343 | `	double r,x;` |
|   19 |  344 | `	if( nArg < 1 ){` |
|    - |  345 | `		/* Missing argument,return 0 */` |
|  ! 0 |  346 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  347 | `		return PH7_OK;` |
|    - |  348 | `	}` |
|   19 |  349 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  350 | `	/* Perform the requested operation */` |
|   19 |  351 | `	r = sinh(x);` |
|    - |  352 | `	/* store the result back */` |
|   19 |  353 | `	ph7_result_double(pCtx,r);` |
|   19 |  354 | `	return PH7_OK;` |
|   10 |  355 | `}` |
|    - |  356 | `/*` |
|    - |  357 | ` * float ceil(float $arg )` |
|    - |  358 | ` *  Round fractions up.` |
|    - |  359 | ` * Parameter` |
|    - |  360 | ` *  The number to process.` |
|    - |  361 | ` * Return` |
|    - |  362 | ` *  The next highest integer value by rounding up value if necessary.` |
|    - |  363 | ` */` |
|   12 |  364 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  365 | `{` |
|    - |  366 | `	double r,x;` |
|    - |  367 | `	/* PHP requires exactly one argument. */` |
|   15 |  368 | `	if( nArg != 1 ){` |
|    4 |  369 | `		return PH7_VmThrowException(pCtx,` |
|    - |  370 | `			"ArgumentCountError",` |
|    - |  371 | `			"ceil() expects exactly 1 argument, %d given",` |
|    1 |  372 | `			nArg` |
|    - |  373 | `			);` |
|    - |  374 | `	}` |
|    - |  375 | `	/*` |
|    - |  376 | `	 * PHP only accepts ints, floats or numeric strings.  Any other types` |
|    - |  377 | `	 * (in particular non-numeric strings) should raise a TypeError.  We` |
|    - |  378 | `	 * mimic the approach used by abs() and perform an explicit numeric` |
|    - |  379 | `	 * check on strings before converting to double.` |
|    - |  380 | `	 */` |
|   12 |  381 | `	if( !ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]) ){` |
|    6 |  382 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |  383 | `			int len;` |
|    6 |  384 | `			sxu8 bReal = FALSE;` |
|    6 |  385 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  386 | `			sxi32 rcNum;` |
|    6 |  387 | `			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|    6 |  388 | `			if( rcNum != SXRET_OK ){` |
|    3 |  389 | `				return PH7_VmThrowException(pCtx,` |
|    - |  390 | `					"TypeError",` |
|    - |  391 | `					"ceil(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  392 | `					);` |
|    - |  393 | `			}` |
|    2 |  394 | `		}else{` |
|    - |  395 | `			/* Reject arrays, objects, resources, booleans, NULL, etc. */` |
|  ! 0 |  396 | `			return PH7_VmThrowException(pCtx,` |
|    - |  397 | `				"TypeError",` |
|    - |  398 | `				"ceil(): Argument #1 ($num) must be of type int\|float"` |
|    - |  399 | `				);` |
|    - |  400 | `		}` |
|    1 |  401 | `	}` |
|    - |  402 |  |
|    9 |  403 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  404 | `	/* Perform the requested operation */` |
|    9 |  405 | `	r = ceil(x);` |
|    - |  406 | `	/* store the result back */` |
|    9 |  407 | `	ph7_result_double(pCtx,r);` |
|    9 |  408 | `	return PH7_OK;` |
|    9 |  409 | `}` |
|    - |  410 | `/*` |
|    - |  411 | ` * float tan(float $arg )` |
|    - |  412 | ` *  Tangent.` |
|    - |  413 | ` * Parameter` |
|    - |  414 | ` *  The number to process.` |
|    - |  415 | ` * Return` |
|    - |  416 | ` *  The tangent of arg.` |
|    - |  417 | ` */` |
|    4 |  418 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  419 | `{` |
|    - |  420 | `	double r,x;` |
|    5 |  421 | `	if( nArg < 1 ){` |
|    - |  422 | `		/* Missing argument,return 0 */` |
|  ! 0 |  423 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  424 | `		return PH7_OK;` |
|    - |  425 | `	}` |
|    5 |  426 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  427 | `	/* Perform the requested operation */` |
|    5 |  428 | `	r = tan(x);` |
|    - |  429 | `	/* store the result back */` |
|    5 |  430 | `	ph7_result_double(pCtx,r);` |
|    5 |  431 | `	return PH7_OK;` |
|    3 |  432 | `}` |
|    - |  433 | `/*` |
|    - |  434 | ` * float atan(float $arg )` |
|    - |  435 | ` *  Arc tangent.` |
|    - |  436 | ` * Parameter` |
|    - |  437 | ` *  The number to process.` |
|    - |  438 | ` * Return` |
|    - |  439 | ` *  The arc tangent of arg.` |
|    - |  440 | ` */` |
|   38 |  441 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  442 | `{` |
|    - |  443 | `	double r,x;` |
|    - |  444 | `	/* PHP enforces exactly one argument. */` |
|   41 |  445 | `	if( nArg != 1 ){` |
|    4 |  446 | `		return PH7_VmThrowException(pCtx,` |
|    - |  447 | `			"ArgumentCountError",` |
|    - |  448 | `			"atan() expects exactly 1 argument, %d given",` |
|    1 |  449 | `			nArg` |
|    - |  450 | `			);` |
|    - |  451 | `	}` |
|    - |  452 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, non-numeric strings).` |
|    - |  453 | `	 * PHP 8 reports a TypeError for wrong types. */` |
|   38 |  454 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    8 |  455 | `		return PH7_VmThrowException(pCtx,` |
|    - |  456 | `			"TypeError",` |
|    - |  457 | `			"atan(): Argument #1 ($num) must be of type float, %s given",` |
|    2 |  458 | `			ph7_type_name(apArg[0])` |
|    - |  459 | `			);` |
|    - |  460 | `	}` |
|   33 |  461 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  462 | `	/* Perform the requested operation */` |
|   33 |  463 | `	r = atan(x);` |
|    - |  464 | `	/* store the result back */` |
|   33 |  465 | `	ph7_result_double(pCtx,r);` |
|   33 |  466 | `	return PH7_OK;` |
|   22 |  467 | `}` |
|    - |  468 | `/*` |
|    - |  469 | ` * float tanh(float $arg )` |
|    - |  470 | ` *  Hyperbolic tangent.` |
|    - |  471 | ` * Parameter` |
|    - |  472 | ` *  The number to process.` |
|    - |  473 | ` * Return` |
|    - |  474 | ` *  The Hyperbolic tangent of arg.` |
|    - |  475 | ` */` |
|   18 |  476 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  477 | `{` |
|    - |  478 | `	double r,x;` |
|   19 |  479 | `	if( nArg < 1 ){` |
|    - |  480 | `		/* Missing argument,return 0 */` |
|  ! 0 |  481 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  482 | `		return PH7_OK;` |
|    - |  483 | `	}` |
|   19 |  484 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  485 | `	/* Perform the requested operation */` |
|   19 |  486 | `	r = tanh(x);` |
|    - |  487 | `	/* store the result back */` |
|   19 |  488 | `	ph7_result_double(pCtx,r);` |
|   19 |  489 | `	return PH7_OK;` |
|   10 |  490 | `}` |
|    - |  491 | `/*` |
|    - |  492 | ` * float atan2(float $y,float $x)` |
|    - |  493 | ` *  Arc tangent of two variable.` |
|    - |  494 | ` * Parameter` |
|    - |  495 | ` *  $y = Dividend parameter.` |
|    - |  496 | ` *  $x = Divisor parameter.` |
|    - |  497 | ` * Return` |
|    - |  498 | ` *  The arc tangent of y/x in radian.` |
|    - |  499 | ` */` |
|   52 |  500 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  501 | `{` |
|    - |  502 | `	double r,x,y;` |
|    - |  503 | `	/* PHP enforces exactly two arguments. */` |
|   56 |  504 | `	if( nArg != 2 ){` |
|    4 |  505 | `		return PH7_VmThrowException(pCtx,` |
|    - |  506 | `			"ArgumentCountError",` |
|    - |  507 | `			"atan2() expects exactly 2 arguments, %d given",` |
|    1 |  508 | `			nArg` |
|    - |  509 | `			);` |
|    - |  510 | `	}` |
|    - |  511 | `	/* Type checking: reject non-numeric values for $y (argument #1). */` |
|   53 |  512 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    4 |  513 | `		return PH7_VmThrowException(pCtx,` |
|    - |  514 | `			"TypeError",` |
|    - |  515 | `			"atan2(): Argument #1 ($y) must be of type float, %s given",` |
|    1 |  516 | `			ph7_type_name(apArg[0])` |
|    - |  517 | `			);` |
|    - |  518 | `	}` |
|    - |  519 | `	/* Type checking: reject non-numeric values for $x (argument #2). */` |
|   50 |  520 | `	if( !ph7_value_is_numeric(apArg[1]) ){` |
|    4 |  521 | `		return PH7_VmThrowException(pCtx,` |
|    - |  522 | `			"TypeError",` |
|    - |  523 | `			"atan2(): Argument #2 ($x) must be of type float, %s given",` |
|    2 |  524 | `			ph7_type_name(apArg[1])` |
|    - |  525 | `			);` |
|    - |  526 | `	}` |
|   47 |  527 | `	y = ph7_value_to_double(apArg[0]);` |
|   47 |  528 | `	x = ph7_value_to_double(apArg[1]);` |
|    - |  529 | `	/* Perform the requested operation */` |
|   47 |  530 | `	r = atan2(y,x);` |
|    - |  531 | `	/* store the result back */` |
|   47 |  532 | `	ph7_result_double(pCtx,r);` |
|   47 |  533 | `	return PH7_OK;` |
|   30 |  534 | `}` |
|    - |  535 | `/*` |
|    - |  536 | ` * float/int64 abs(float/int64 $arg )` |
|    - |  537 | ` *  Absolute value.` |
|    - |  538 | ` * Parameter` |
|    - |  539 | ` *  The number to process.` |
|    - |  540 | ` * Return` |
|    - |  541 | ` *  The absolute value of number.` |
|    - |  542 | ` */` |
|  130 |  543 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  544 | `{` |
|    - |  545 | `	int is_float;` |
|    - |  546 | `	/* PHP requires exactly one argument. */` |
|  134 |  547 | `	if( nArg != 1 ){` |
|    4 |  548 | `		return PH7_VmThrowException(pCtx,` |
|    - |  549 | `			"ArgumentCountError",` |
|    - |  550 | `			"abs() expects exactly 1 argument, %d given",` |
|    1 |  551 | `			nArg` |
|    - |  552 | `			);` |
|    - |  553 | `	}` |
|    - |  554 |  |
|  131 |  555 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    - |  556 | `		/* php's 8.1 null-to-scalar-parameter deprecation; abs(null) is still 0 */` |
|    3 |  557 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|    - |  558 | `			"abs(): Passing null to parameter #1 ($num) of type int\|float is deprecated");` |
|    1 |  559 | `	}` |
|    - |  560 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|  131 |  561 | `	is_float = ph7_value_is_float(apArg[0]);` |
|  131 |  562 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|    - |  563 | `		int len;` |
|   10 |  564 | `		sxu8 bReal = FALSE;` |
|   10 |  565 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  566 | `		sxi32 rcNum;` |
|   10 |  567 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|   10 |  568 | `		if( rcNum != SXRET_OK ){` |
|    3 |  569 | `			return PH7_VmThrowException(pCtx,` |
|    - |  570 | `				"TypeError",` |
|    - |  571 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  572 | `				);` |
|    - |  573 | `		}` |
|    7 |  574 | `		if( bReal ){` |
|    5 |  575 | `			is_float = 1;` |
|    2 |  576 | `		}` |
|    3 |  577 | `	}` |
|  128 |  578 | `	if( is_float ){` |
|    - |  579 | `		double r,x;` |
|   99 |  580 | `		x = ph7_value_to_double(apArg[0]);` |
|    - |  581 | `		/* Perform the requested operation */` |
|   99 |  582 | `		r = fabs(x);` |
|   99 |  583 | `		ph7_result_double(pCtx,r);` |
|   50 |  584 | `	}else{` |
|    - |  585 | ``		/* Read the full 64-bit value (the old 32-bit `int abs()` truncated any`` |
|    - |  586 | `		 * magnitude above 2^31 and was UB on INT_MIN). */` |
|   30 |  587 | `		sxi64 x = ph7_value_to_int64(apArg[0]);` |
|   30 |  588 | `		if( x == SMALLEST_INT64 ){` |
|    - |  589 | `			/* abs(PHP_INT_MIN) has no int representation, so PHP returns a float. */` |
|    3 |  590 | `			ph7_result_double(pCtx,-(double)x);` |
|    2 |  591 | `		}else{` |
|   28 |  592 | `			ph7_result_int64(pCtx,x < 0 ? -x : x);` |
|    - |  593 | `		}` |
|    - |  594 | `	}` |
|  128 |  595 | `	return PH7_OK;` |
|   69 |  596 | `}` |
|    - |  597 | `/*` |
|    - |  598 | ` * float log(float $arg,[int/float $base])` |
|    - |  599 | ` *  Natural logarithm.` |
|    - |  600 | ` * Parameter` |
|    - |  601 | ` *  $arg: The number to process.` |
|    - |  602 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|    - |  603 | ` * Return` |
|    - |  604 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|    - |  605 | ` * Note:` |
|    - |  606 | ` *  only Natural log and base-10 log are supported.` |
|    - |  607 | ` */` |
|   12 |  608 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  609 | `{` |
|    - |  610 | `	double r,x;` |
|   13 |  611 | `	if( nArg < 1 ){` |
|    - |  612 | `		/* Missing argument,return 0 */` |
|  ! 0 |  613 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  614 | `		return PH7_OK;` |
|    - |  615 | `	}` |
|   13 |  616 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  617 | `	/* Perform the requested operation */` |
|   13 |  618 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|    - |  619 | `		/* Base-10 log */` |
|    5 |  620 | `		r = log10(x);` |
|    3 |  621 | `	}else{` |
|    9 |  622 | `		r = log(x);` |
|    - |  623 | `	}` |
|    - |  624 | `	/* store the result back */` |
|   13 |  625 | `	ph7_result_double(pCtx,r);` |
|   13 |  626 | `	return PH7_OK;` |
|    7 |  627 | `}` |
|    - |  628 | `/*` |
|    - |  629 | ` * float log10(float $arg )` |
|    - |  630 | ` *  Base-10 logarithm.` |
|    - |  631 | ` * Parameter` |
|    - |  632 | ` *  The number to process.` |
|    - |  633 | ` * Return` |
|    - |  634 | ` *  The Base-10 logarithm of the given number.` |
|    - |  635 | ` */` |
|   14 |  636 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  637 | `{` |
|    - |  638 | `	double r,x;` |
|   15 |  639 | `	if( nArg < 1 ){` |
|    - |  640 | `		/* Missing argument,return 0 */` |
|  ! 0 |  641 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  642 | `		return PH7_OK;` |
|    - |  643 | `	}` |
|   15 |  644 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  645 | `	/* Perform the requested operation */` |
|   15 |  646 | `	r = log10(x);` |
|    - |  647 | `	/* store the result back */` |
|   15 |  648 | `	ph7_result_double(pCtx,r);` |
|   15 |  649 | `	return PH7_OK;` |
|    8 |  650 | `}` |
|    - |  651 | `/*` |
|    - |  652 | ` * number pow(number $base,number $exp)` |
|    - |  653 | ` *  Exponential expression.` |
|    - |  654 | ` * Parameter` |
|    - |  655 | ` *  base` |
|    - |  656 | ` *  The base to use.` |
|    - |  657 | ` * exp` |
|    - |  658 | ` *  The exponent.` |
|    - |  659 | ` * Return` |
|    - |  660 | ` *  base raised to the power of exp.` |
|    - |  661 | ` *  If the result can be represented as integer it will be returned` |
|    - |  662 | ` *  as type integer, else it will be returned as type float.` |
|    - |  663 | ` */` |
|    6 |  664 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  665 | `{` |
|    - |  666 | `	double r,x,y;` |
|    7 |  667 | `	if( nArg < 1 ){` |
|    - |  668 | `		/* Missing argument,return 0 */` |
|  ! 0 |  669 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  670 | `		return PH7_OK;` |
|    - |  671 | `	}` |
|    7 |  672 | `	x = ph7_value_to_double(apArg[0]);` |
|    7 |  673 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  674 | `	/* Perform the requested operation */` |
|    7 |  675 | `	r = pow(x,y);` |
|    7 |  676 | `	ph7_result_double(pCtx,r);` |
|    7 |  677 | `	return PH7_OK;` |
|    4 |  678 | `}` |
|    - |  679 | `/*` |
|    - |  680 | ` * float pi(void)` |
|    - |  681 | ` *  Returns an approximation of pi.` |
|    - |  682 | ` * Note` |
|    - |  683 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|    - |  684 | ` * Return` |
|    - |  685 | ` *  The value of pi as float.` |
|    - |  686 | ` */` |
|    4 |  687 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  688 | `{` |
|    2 |  689 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  690 | `	SXUNUSED(apArg);` |
|    6 |  691 | `	ph7_result_double(pCtx,PH7_PI);` |
|    6 |  692 | `	return PH7_OK;` |
|    2 |  693 | `}` |
|    - |  694 | `/*` |
|    - |  695 | ` * float fmod(float $x,float $y)` |
|    - |  696 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|    - |  697 | ` * Parameters` |
|    - |  698 | ` * $x` |
|    - |  699 | ` *  The dividend` |
|    - |  700 | ` * $y` |
|    - |  701 | ` *  The divisor` |
|    - |  702 | ` * Return` |
|    - |  703 | ` *  The floating point remainder of x/y.` |
|    - |  704 | ` */` |
|    2 |  705 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  706 | `{` |
|    - |  707 | `	double x,y,r;` |
|    3 |  708 | `	if( nArg < 2 ){` |
|    - |  709 | `		/* Missing arguments */` |
|  ! 0 |  710 | `		ph7_result_double(pCtx,0);` |
|  ! 0 |  711 | `		return PH7_OK;` |
|    - |  712 | `	}` |
|    - |  713 | `	/* Extract given arguments */` |
|    3 |  714 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  715 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  716 | `	/* Perform the requested operation */` |
|    3 |  717 | `	r = fmod(x,y);` |
|    - |  718 | `	/* Processing result */` |
|    3 |  719 | `	ph7_result_double(pCtx,r);` |
|    3 |  720 | `	return PH7_OK;` |
|    2 |  721 | `}` |
|    - |  722 | `/*` |
|    - |  723 | ` * float hypot(float $x,float $y)` |
|    - |  724 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|    - |  725 | ` * Parameters` |
|    - |  726 | ` * $x` |
|    - |  727 | ` *  Length of first side` |
|    - |  728 | ` * $y` |
|    - |  729 | ` *  Length of first side` |
|    - |  730 | ` * Return` |
|    - |  731 | ` *  Calculated length of the hypotenuse.` |
|    - |  732 | ` */` |
|    2 |  733 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  734 | `{` |
|    - |  735 | `	double x,y,r;` |
|    3 |  736 | `	if( nArg < 2 ){` |
|    - |  737 | `		/* Missing arguments */` |
|  ! 0 |  738 | `		ph7_result_double(pCtx,0);` |
|  ! 0 |  739 | `		return PH7_OK;` |
|    - |  740 | `	}` |
|    - |  741 | `	/* Extract given arguments */` |
|    3 |  742 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  743 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  744 | `	/* Perform the requested operation */` |
|    3 |  745 | `	r = hypot(x,y);` |
|    - |  746 | `	/* Processing result */` |
|    3 |  747 | `	ph7_result_double(pCtx,r);` |
|    3 |  748 | `	return PH7_OK;` |
|    2 |  749 | `}` |
|    - |  750 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|    - |  751 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  752 | `/*` |
|    - |  753 | ` * PHP rounding modes (mirror ext/standard/php_math_round_mode.h).` |
|    - |  754 | ` * Only the four HALF_* integer constants are exposed to userland` |
|    - |  755 | ` * (PHP_ROUND_HALF_UP..HALF_ODD, see constant.c); the CEILING/FLOOR/` |
|    - |  756 | ` * TOWARD_ZERO/AWAY_FROM_ZERO modes (5..8) have no userland constant but` |
|    - |  757 | ` * are reachable by passing the raw integer to round()'s 3rd argument,` |
|    - |  758 | ` * which PHP 8.5 still accepts, so all eight are honored here.` |
|    - |  759 | ` */` |
|    - |  760 | `#define PH7_ROUND_HALF_UP        1` |
|    - |  761 | `#define PH7_ROUND_HALF_DOWN      2` |
|    - |  762 | `#define PH7_ROUND_HALF_EVEN      3` |
|    - |  763 | `#define PH7_ROUND_HALF_ODD       4` |
|    - |  764 | `#define PH7_ROUND_CEILING        5` |
|    - |  765 | `#define PH7_ROUND_FLOOR          6` |
|    - |  766 | `#define PH7_ROUND_TOWARD_ZERO    7` |
|    - |  767 | `#define PH7_ROUND_AWAY_FROM_ZERO 8` |
|    - |  768 | `/*` |
|    - |  769 | ` * 10**power via an exact lookup table for 0..22, falling back to pow()` |
|    - |  770 | ` * otherwise. Port of php-src PHP-8.5 ext/standard/math.c php_intpow10().` |
|    - |  771 | ` */` |
|  156 |  772 | `static double MathIntPow10(int power)` |
|    2 |  773 | `{` |
|    - |  774 | `	static const double powers[] = {` |
|    - |  775 | `		1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,` |
|    - |  776 | `		1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22` |
|    - |  777 | `	};` |
|  158 |  778 | `	if( power < 0 \|\| power > 22 ){` |
|    3 |  779 | `		return pow(10.0, (double)power);` |
|    - |  780 | `	}` |
|  156 |  781 | `	return powers[power];` |
|   80 |  782 | `}` |
|  138 |  783 | `static double MathRoundBasicEdge(double integral, double exponent, int places)` |
|    2 |  784 | `{` |
|   71 |  785 | `	return (places > 0)` |
|   44 |  786 | `		? fabs((integral + copysign(0.5, integral)) / exponent)` |
|  116 |  787 | `		: fabs((integral + copysign(0.5, integral)) * exponent);` |
|    2 |  788 | `}` |
|   12 |  789 | `static double MathRoundZeroEdge(double integral, double exponent, int places)` |
|    1 |  790 | `{` |
|    7 |  791 | `	return (places > 0)` |
|  ! 0 |  792 | `		? fabs((integral) / exponent)` |
|   12 |  793 | `		: fabs((integral) * exponent);` |
|    1 |  794 | `}` |
|    - |  795 | `/*` |
|    - |  796 | ` * Round the extracted integral part according to the requested mode.` |
|    - |  797 | ` * Faithful port of php-src PHP-8.5 ext/standard/math.c php_round_helper().` |
|    - |  798 | ` */` |
|  154 |  799 | `static double MathRoundHelper(double integral, double value, double exponent, int places, int mode)` |
|    2 |  800 | `{` |
|  156 |  801 | `	double value_abs = fabs(value);` |
|    - |  802 | `	double edge_case;` |
|  156 |  803 | `	switch( mode ){` |
|   49 |  804 | `		case PH7_ROUND_HALF_UP:` |
|  100 |  805 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|  100 |  806 | `			if( value_abs >= edge_case ){` |
|   78 |  807 | `				return integral + copysign(1.0, integral);` |
|    - |  808 | `			}` |
|   24 |  809 | `			return integral;` |
|    6 |  810 | `		case PH7_ROUND_HALF_DOWN:` |
|   13 |  811 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   13 |  812 | `			if( value_abs > edge_case ){` |
|  ! 0 |  813 | `				return integral + copysign(1.0, integral);` |
|    - |  814 | `			}` |
|   13 |  815 | `			return integral;` |
|    2 |  816 | `		case PH7_ROUND_CEILING:` |
|    5 |  817 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  818 | `			if( value > 0.0 && value_abs > edge_case ){` |
|    3 |  819 | `				return integral + 1.0;` |
|    - |  820 | `			}` |
|    3 |  821 | `			return integral;` |
|    2 |  822 | `		case PH7_ROUND_FLOOR:` |
|    5 |  823 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  824 | `			if( value < 0.0 && value_abs > edge_case ){` |
|    3 |  825 | `				return integral - 1.0;` |
|    - |  826 | `			}` |
|    3 |  827 | `			return integral;` |
|    2 |  828 | `		case PH7_ROUND_TOWARD_ZERO:` |
|    5 |  829 | `			return integral;` |
|    2 |  830 | `		case PH7_ROUND_AWAY_FROM_ZERO:` |
|    5 |  831 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  832 | `			if( value_abs > edge_case ){` |
|    5 |  833 | `				return integral + copysign(1.0, integral);` |
|    - |  834 | `			}` |
|  ! 0 |  835 | `			return integral;` |
|    8 |  836 | `		case PH7_ROUND_HALF_EVEN:` |
|   17 |  837 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   17 |  838 | `			if( value_abs > edge_case ){` |
|  ! 0 |  839 | `				return integral + copysign(1.0, integral);` |
|   17 |  840 | `			}else if( value_abs == edge_case ){` |
|   17 |  841 | `				if( fmod(integral, 2.0) != 0.0 ){ /* integral not even -> make it even */` |
|    9 |  842 | `					return integral + copysign(1.0, integral);` |
|    - |  843 | `				}` |
|    4 |  844 | `			}` |
|    9 |  845 | `			return integral;` |
|    6 |  846 | `		case PH7_ROUND_HALF_ODD:` |
|   13 |  847 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   13 |  848 | `			if( value_abs > edge_case ){` |
|  ! 0 |  849 | `				return integral + copysign(1.0, integral);` |
|   13 |  850 | `			}else if( value_abs == edge_case ){` |
|   13 |  851 | `				if( fmod(integral, 2.0) == 0.0 ){ /* integral even -> make it odd */` |
|    7 |  852 | `					return integral + copysign(1.0, integral);` |
|    - |  853 | `				}` |
|    3 |  854 | `			}` |
|    7 |  855 | `			return integral;` |
|  ! 0 |  856 | `		default:` |
|  ! 0 |  857 | `			return integral; /* unreachable: mode validated by the caller */` |
|    - |  858 | `	}` |
|   79 |  859 | `}` |
|    - |  860 | `/*` |
|    - |  861 | `` * Round `value` to `places` decimals in `mode`. Faithful port of php-src`` |
|    - |  862 | ` * PHP-8.5 ext/standard/math.c _php_math_round() — the post-8.4` |
|    - |  863 | ` * integer-extraction algorithm with the +/-1 floating-point error` |
|    - |  864 | ` * correction step, required for byte-exact results on cases such as` |
|    - |  865 | ` * round(0.285, 2) == 0.29 that the old naive "+0.5" approach got wrong.` |
|    - |  866 | ` */` |
|  162 |  867 | `static double MathRound(double value, int places, int mode)` |
|    2 |  868 | `{` |
|    - |  869 | `	double exponent, tmp_value, tmp_value2;` |
|    - |  870 | `	int abs_places;` |
|  164 |  871 | `	if( !isfinite(value) \|\| value == 0.0 ){` |
|    7 |  872 | `		return value;` |
|    - |  873 | `	}` |
|    - |  874 | `	/* mirror php-src's clamp away from INT_MIN */` |
|  158 |  875 | `	if( places < -2147483647 ){` |
|  ! 0 |  876 | `		places = -2147483647;` |
|  ! 0 |  877 | `	}` |
|  158 |  878 | `	abs_places = places < 0 ? -places : places;` |
|  158 |  879 | `	exponent = MathIntPow10(abs_places);` |
|    - |  880 | `	/*` |
|    - |  881 | `	 * Extracting the integer part can be off by one ULP due to float error` |
|    - |  882 | `	 * (e.g. floor(0.285 * 1e10) == 2849999999). Try +/-1 and keep it if it` |
|    - |  883 | ``	 * divides back to exactly `value`.`` |
|    - |  884 | `	 */` |
|  158 |  885 | `	if( value >= 0.0 ){` |
|  126 |  886 | `		tmp_value = floor(places > 0 ? value * exponent : value / exponent);` |
|  126 |  887 | `		tmp_value2 = tmp_value + 1.0;` |
|   64 |  888 | `	}else{` |
|   34 |  889 | `		tmp_value = ceil(places > 0 ? value * exponent : value / exponent);` |
|   34 |  890 | `		tmp_value2 = tmp_value - 1.0;` |
|    - |  891 | `	}` |
|  158 |  892 | `	if( (places > 0 ? tmp_value2 / exponent : tmp_value2 * exponent) == value ){` |
|    3 |  893 | `		tmp_value = tmp_value2;` |
|    1 |  894 | `	}` |
|    - |  895 | `	/* Beyond our precision, so rounding it is pointless. */` |
|  158 |  896 | `	if( fabs(tmp_value) >= 1e16 ){` |
|    3 |  897 | `		return value;` |
|    - |  898 | `	}` |
|  156 |  899 | `	tmp_value = MathRoundHelper(tmp_value, value, exponent, places, mode);` |
|  156 |  900 | `	if( abs_places < 23 ){` |
|  156 |  901 | `		tmp_value = (places > 0) ? tmp_value / exponent : tmp_value * exponent;` |
|   79 |  902 | `	}else{` |
|    - |  903 | `		/*` |
|    - |  904 | `		 * Simple division would lose precision here; round-trip through a` |
|    - |  905 | `		 * string exactly like php-src does (snprintf "%15fe%d" + strtod).` |
|    - |  906 | `		 * libc snprintf is used (not SyBufferFormat, which is not` |
|    - |  907 | `		 * correctly-rounded) so the low bits match PHP. (SyStrToReal now` |
|    - |  908 | `		 * delegates to strtod too; the direct call here simply mirrors` |
|    - |  909 | `		 * php-src's own snprintf+strtod pairing.)` |
|    - |  910 | `		 */` |
|    - |  911 | `		char zBuf[64];` |
|  ! 0 |  912 | `		snprintf(zBuf, sizeof(zBuf), "%15fe%d", tmp_value, -places);` |
|  ! 0 |  913 | `		zBuf[sizeof(zBuf)-1] = '\0';` |
|  ! 0 |  914 | `		tmp_value = strtod(zBuf, 0);` |
|  ! 0 |  915 | `		if( !isfinite(tmp_value) \|\| isnan(tmp_value) ){` |
|  ! 0 |  916 | `			tmp_value = value;` |
|  ! 0 |  917 | `		}` |
|    - |  918 | `	}` |
|  156 |  919 | `	return tmp_value;` |
|   83 |  920 | `}` |
|    - |  921 | `/*` |
|    - |  922 | ` * float round ( int\|float $num [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|    - |  923 | ` *  Rounds a float.` |
|    - |  924 | ` * Parameters` |
|    - |  925 | ` *  $num       The value to round.` |
|    - |  926 | ` *  $precision The optional number of decimal digits to round to. May be` |
|    - |  927 | ` *             negative (rounds to the left of the decimal point).` |
|    - |  928 | ` *  $mode      One of PHP_ROUND_HALF_UP (default) / _HALF_DOWN / _HALF_EVEN /` |
|    - |  929 | ` *             _HALF_ODD, or the 8.5 integer modes CEILING / FLOOR /` |
|    - |  930 | ` *             TOWARD_ZERO / AWAY_FROM_ZERO (5..8).` |
|    - |  931 | ` * Return` |
|    - |  932 | ` *  The rounded value as a float.` |
|    - |  933 | ` */` |
|  188 |  934 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  935 | `{` |
|    - |  936 | `	double value, r;` |
|  190 |  937 | `	int places = 0;` |
|  190 |  938 | `	int mode = PH7_ROUND_HALF_UP;` |
|    - |  939 | `	/*` |
|    - |  940 | `	 * Legacy PHL contract: no argument -> int(0). PHP throws an` |
|    - |  941 | `	 * ArgumentCountError here, but two PHL-only (--SKIPIF-- zend_version)` |
|    - |  942 | `	 * tests assert round()===0, so keep the historical behavior.` |
|    - |  943 | `	 */` |
|  190 |  944 | `	if( nArg < 1 ){` |
|  ! 0 |  945 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  946 | `		return PH7_OK;` |
|    - |  947 | `	}` |
|  190 |  948 | `	if( nArg > 3 ){` |
|    4 |  949 | `		return PH7_VmThrowException(pCtx,` |
|    - |  950 | `			"ArgumentCountError",` |
|    - |  951 | `			"round() expects at most 3 arguments, %d given",` |
|    1 |  952 | `			nArg` |
|    - |  953 | `			);` |
|    - |  954 | `	}` |
|    - |  955 | `	/*` |
|    - |  956 | `	 * Validate argument #1: only int/float (and numeric strings) are` |
|    - |  957 | `	 * accepted; every other type raises a TypeError (mirrors floor()/ceil()).` |
|    - |  958 | `	 */` |
|  188 |  959 | `	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){` |
|    5 |  960 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |  961 | `			int len;` |
|    5 |  962 | `			sxu8 bReal = FALSE;` |
|    5 |  963 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    5 |  964 | `			if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|    4 |  965 | `				return PH7_VmThrowException(pCtx,` |
|    - |  966 | `					"TypeError",` |
|    - |  967 | `					"round(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |  968 | `					ph7_type_name(apArg[0])` |
|    - |  969 | `					);` |
|    - |  970 | `			}` |
|    2 |  971 | `		}else{` |
|  ! 0 |  972 | `			return PH7_VmThrowException(pCtx,` |
|    - |  973 | `				"TypeError",` |
|    - |  974 | `				"round(): Argument #1 ($num) must be of type int\|float, %s given",` |
|  ! 0 |  975 | `				ph7_type_name(apArg[0])` |
|    - |  976 | `				);` |
|    - |  977 | `		}` |
|    1 |  978 | `	}` |
|    - |  979 | `	/* Precision (arg #2). Negative values are valid; clamp to int range. */` |
|  186 |  980 | `	if( nArg > 1 ){` |
|  156 |  981 | `		sxi64 prec = ph7_value_to_int64(apArg[1]);` |
|  156 |  982 | `		if( prec > 2147483647 ){` |
|  ! 0 |  983 | `			places = 2147483647;` |
|  156 |  984 | `		}else if( prec < -2147483647 ){` |
|  ! 0 |  985 | `			places = -2147483647;` |
|  ! 0 |  986 | `		}else{` |
|  156 |  987 | `			places = (int)prec;` |
|    - |  988 | `		}` |
|   77 |  989 | `	}` |
|    - |  990 | `	/*` |
|    - |  991 | `	 * Mode (arg #3). PHP 8.5 accepts the integer modes 1..8. Read the full` |
|    - |  992 | `	 * 64-bit value before range-checking so a large out-of-range mode cannot` |
|    - |  993 | `	 * alias a valid 1..8 via a truncating 32-bit cast (e.g. 0x1_0000_0003).` |
|    - |  994 | `	 */` |
|  186 |  995 | `	if( nArg > 2 ){` |
|   73 |  996 | `		sxi64 m = ph7_value_to_int64(apArg[2]);` |
|   73 |  997 | `		if( m < PH7_ROUND_HALF_UP \|\| m > PH7_ROUND_AWAY_FROM_ZERO ){` |
|    5 |  998 | `			return PH7_VmThrowException(pCtx,` |
|    - |  999 | `				"ValueError",` |
|    - | 1000 | `				"round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)"` |
|    - | 1001 | `				);` |
|    - | 1002 | `		}` |
|   69 | 1003 | `		mode = (int)m;` |
|   34 | 1004 | `	}` |
|  182 | 1005 | `	value = ph7_value_to_double(apArg[0]);` |
|    - | 1006 | `	/* Integer input with non-negative precision needs no rounding. */` |
|  182 | 1007 | `	if( ph7_value_is_int(apArg[0]) && places >= 0 ){` |
|   19 | 1008 | `		ph7_result_double(pCtx,value);` |
|   19 | 1009 | `		return PH7_OK;` |
|    - | 1010 | `	}` |
|  164 | 1011 | `	r = MathRound(value, places, mode);` |
|  164 | 1012 | `	ph7_result_double(pCtx,r);` |
|  164 | 1013 | `	return PH7_OK;` |
|   96 | 1014 | `}` |
|    - | 1015 | `/*` |
|    - | 1016 | ` * int intdiv(int $a, int $b)` |
|    - | 1017 | ` *  Integer division.` |
|    - | 1018 | ` * Parameters` |
|    - | 1019 | ` *  $a` |
|    - | 1020 | ` *   Number to be divided.` |
|    - | 1021 | ` *  $b` |
|    - | 1022 | ` *   Number which divides the $a.` |
|    - | 1023 | ` * Return` |
|    - | 1024 | ` *  The integer quotient of the division of $a by $b.` |
|    - | 1025 | ` */` |
|  146 | 1026 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 1027 | `{` |
|    - | 1028 | `	sxi64 a,b;` |
|    - | 1029 | `	/* PHP requires exactly two arguments. */` |
|  149 | 1030 | `	if( nArg != 2 ){` |
|  ! 0 | 1031 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1032 | `			"ArgumentCountError",` |
|    - | 1033 | `			"intdiv() expects exactly 2 arguments, %d given",` |
|  ! 0 | 1034 | `			nArg` |
|    - | 1035 | `			);` |
|    - | 1036 | `	}` |
|    - | 1037 | `	/* Type-check argument 1 */` |
|  146 | 1038 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|  149 | 1039 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 | 1040 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1041 | `			"TypeError",` |
|    - | 1042 | `			"intdiv(): Argument #1 ($num1) must be of type int, %s given",` |
|  ! 0 | 1043 | `			ph7_type_name(apArg[0])` |
|    - | 1044 | `			);` |
|    - | 1045 | `	}` |
|  149 | 1046 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - | 1047 | `		int len;` |
|  ! 0 | 1048 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|  ! 0 | 1049 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 | 1050 | `			return PH7_VmThrowException(pCtx,` |
|    - | 1051 | `				"TypeError",` |
|    - | 1052 | `				"intdiv(): Argument #1 ($num1) must be of type int, string given"` |
|    - | 1053 | `				);` |
|    - | 1054 | `		}` |
|  ! 0 | 1055 | `	}` |
|    - | 1056 | `	/* Type-check argument 2 */` |
|  146 | 1057 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|  149 | 1058 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|  ! 0 | 1059 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1060 | `			"TypeError",` |
|    - | 1061 | `			"intdiv(): Argument #2 ($num2) must be of type int, %s given",` |
|  ! 0 | 1062 | `			ph7_type_name(apArg[1])` |
|    - | 1063 | `			);` |
|    - | 1064 | `	}` |
|  149 | 1065 | `	if( ph7_value_is_string(apArg[1]) ){` |
|    - | 1066 | `		int len;` |
|  ! 0 | 1067 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|  ! 0 | 1068 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 | 1069 | `			return PH7_VmThrowException(pCtx,` |
|    - | 1070 | `				"TypeError",` |
|    - | 1071 | `				"intdiv(): Argument #2 ($num2) must be of type int, string given"` |
|    - | 1072 | `				);` |
|    - | 1073 | `		}` |
|  ! 0 | 1074 | `	}` |
|    - | 1075 | `	/* Convert both arguments to int64 */` |
|    - | 1076 | `	{` |
|    - | 1077 | `		/* php's ZPP contract for the two int params (lossy float / float-string` |
|    - | 1078 | `		 * deprecations); the manual type checks above already covered arrays,` |
|    - | 1079 | `		 * objects and non-numeric strings with the same messages. */` |
|  149 | 1080 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[0],"intdiv",1,"$num1","int",&a);` |
|  149 | 1081 | `		if( rcArg != PH7_OK ){` |
|  ! 0 | 1082 | `			return rcArg;` |
|    - | 1083 | `		}` |
|  149 | 1084 | `		rcArg = PH7_IntArgResolve(pCtx,apArg[1],"intdiv",2,"$num2","int",&b);` |
|  149 | 1085 | `		if( rcArg != PH7_OK ){` |
|  ! 0 | 1086 | `			return rcArg;` |
|    - | 1087 | `		}` |
|    - | 1088 | `	}` |
|    - | 1089 | `	/* Check for division by zero */` |
|  149 | 1090 | `	if( b == 0 ){` |
|    3 | 1091 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1092 | `			"DivisionByZeroError",` |
|    - | 1093 | `			"Division by zero"` |
|    - | 1094 | `			);` |
|    - | 1095 | `	}` |
|    - | 1096 | `	/* Check for overflow: PHP_INT_MIN / -1 */` |
|  146 | 1097 | `	if( a == SMALLEST_INT64 && b == -1 ){` |
|    3 | 1098 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1099 | `			"ArithmeticError",` |
|    - | 1100 | `			"Division of PHP_INT_MIN by -1 is not an integer"` |
|    - | 1101 | `			);` |
|    - | 1102 | `	}` |
|    - | 1103 | `	/* Perform integer division */` |
|  143 | 1104 | `	ph7_result_int64(pCtx, a / b);` |
|  143 | 1105 | `	return PH7_OK;` |
|   76 | 1106 | `}` |
|    - | 1107 | `/*` |
|    - | 1108 | ` * string dechex(int $number)` |
|    - | 1109 | ` *  Decimal to hexadecimal.` |
|    - | 1110 | ` * Parameters` |
|    - | 1111 | ` *  $number` |
|    - | 1112 | ` *   Decimal value to convert` |
|    - | 1113 | ` * Return` |
|    - | 1114 | ` *  Hexadecimal string representation of number` |
|    - | 1115 | ` */` |
|   14 | 1116 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1117 | `{` |
|    - | 1118 | `	ph7_int64 iVal;` |
|   15 | 1119 | `	if( nArg < 1 ){` |
|    - | 1120 | `		/* Missing arguments,return null */` |
|  ! 0 | 1121 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1122 | `		return PH7_OK;` |
|    - | 1123 | `	}` |
|    - | 1124 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   15 | 1125 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1126 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement,` |
|    - | 1127 | `	 * so a negative value prints all 16 hex digits like PHP. */` |
|   15 | 1128 | `	ph7_result_string_format(pCtx,"%qx",iVal);` |
|   15 | 1129 | `	return PH7_OK;` |
|    8 | 1130 | `}` |
|    - | 1131 | `/*` |
|    - | 1132 | ` * string decoct(int $number)` |
|    - | 1133 | ` *  Decimal to Octal.` |
|    - | 1134 | ` * Parameters` |
|    - | 1135 | ` *  $number` |
|    - | 1136 | ` *   Decimal value to convert` |
|    - | 1137 | ` * Return` |
|    - | 1138 | ` *  Octal string representation of number` |
|    - | 1139 | ` */` |
|   16 | 1140 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1141 | `{` |
|    - | 1142 | `	ph7_int64 iVal;` |
|   17 | 1143 | `	if( nArg < 1 ){` |
|    - | 1144 | `		/* Missing arguments,return null */` |
|  ! 0 | 1145 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1146 | `		return PH7_OK;` |
|    - | 1147 | `	}` |
|    - | 1148 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   17 | 1149 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1150 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|   17 | 1151 | `	ph7_result_string_format(pCtx,"%qo",iVal);` |
|   17 | 1152 | `	return PH7_OK;` |
|    9 | 1153 | `}` |
|    - | 1154 | `/*` |
|    - | 1155 | ` * string decbin(int $number)` |
|    - | 1156 | ` *  Decimal to binary.` |
|    - | 1157 | ` * Parameters` |
|    - | 1158 | ` *  $number` |
|    - | 1159 | ` *   Decimal value to convert` |
|    - | 1160 | ` * Return` |
|    - | 1161 | ` *  Binary string representation of number` |
|    - | 1162 | ` */` |
|   10 | 1163 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1164 | `{` |
|    - | 1165 | `	ph7_int64 iVal;` |
|   11 | 1166 | `	if( nArg < 1 ){` |
|    - | 1167 | `		/* Missing arguments,return null */` |
|  ! 0 | 1168 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1169 | `		return PH7_OK;` |
|    - | 1170 | `	}` |
|    - | 1171 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   11 | 1172 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1173 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|   11 | 1174 | `	ph7_result_string_format(pCtx,"%qB",iVal);` |
|   11 | 1175 | `	return PH7_OK;` |
|    6 | 1176 | `}` |
|    - | 1177 | `/*` |
|    - | 1178 | ` * Convert a base-2/8/16 digit string to a number, mirroring PHP's` |
|    - | 1179 | ` * _php_math_basetozval (ext/standard/math.c) so hexdec/octdec/bindec agree with` |
|    - | 1180 | ` * php byte-for-byte: walk every byte, decode a digit (0-9,a-z,A-Z) or skip any` |
|    - | 1181 | ` * invalid one, accumulate into a signed 64-bit integer and transparently promote` |
|    - | 1182 | ` * to a double once the value would overflow PHP_INT_MAX. The context result is` |
|    - | 1183 | ` * set to an int when it fits, otherwise a float — PHP returns a float for values` |
|    - | 1184 | ` * above PHP_INT_MAX (e.g. hexdec("ffffffffffffffff") == 1.8446744073709552E+19).` |
|    - | 1185 | ` * A byte >= 0x80 (e.g. a UTF-8 continuation) matches none of the digit ranges and` |
|    - | 1186 | ` * is skipped, so leading/interior multibyte junk is ignored like php.` |
|    - | 1187 | ` * Note: php also raises E_DEPRECATED for skipped invalid characters; that notice` |
|    - | 1188 | ` * is not emitted here (a §3.7 deprecation-fidelity residual, value is correct).` |
|    - | 1189 | ` */` |
|   88 | 1190 | `static void MathBaseToNumber(ph7_context *pCtx,const char *zStr,int nLen,int base)` |
|    3 | 1191 | `{` |
|   91 | 1192 | `	sxi64 num = 0;      /* Integer accumulator */` |
|   91 | 1193 | `	double fnum = 0;    /* Float accumulator (used once num would overflow) */` |
|   91 | 1194 | `	int mode = 0;       /* 0 -> integer accumulation, 1 -> switched to float */` |
|   91 | 1195 | `	sxi64 cutoff = SXI64_HIGH / base;      /* PHP_INT_MAX / base */` |
|   91 | 1196 | `	int cutlim = (int)(SXI64_HIGH % base); /* PHP_INT_MAX % base */` |
|   91 | 1197 | `	int bIgnored = 0;   /* any character skipped below? php deprecates that */` |
|    - | 1198 | `	int i;` |
|  735 | 1199 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  647 | 1200 | `		int c = (unsigned char)zStr[i];` |
|  647 | 1201 | `		if( c >= '0' && c <= '9' ){` |
|  505 | 1202 | `			c -= '0';` |
|  396 | 1203 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|   31 | 1204 | `			c -= 'A' - 10;` |
|  130 | 1205 | `		}else if( c >= 'a' && c <= 'z' ){` |
|  107 | 1206 | `			c -= 'a' - 10;` |
|   55 | 1207 | `		}else{` |
|    9 | 1208 | `			bIgnored = 1;` |
|    9 | 1209 | `			continue; /* Not a digit character: skip */` |
|    - | 1210 | `		}` |
|  639 | 1211 | `		if( c >= base ){` |
|   10 | 1212 | `			bIgnored = 1;` |
|   10 | 1213 | `			continue; /* Digit out of range for this base: skip */` |
|    - | 1214 | `		}` |
|  631 | 1215 | `		if( mode == 0 ){` |
|  631 | 1216 | `			if( num < cutoff \|\| (num == cutoff && c <= cutlim) ){` |
|  625 | 1217 | `				num = num * base + c;` |
|  625 | 1218 | `				continue;` |
|    - | 1219 | `			}` |
|    - | 1220 | `			/* Adding this digit would overflow the 64-bit integer: fall back to` |
|    - | 1221 | `			 * float accumulation, seeding it with the value gathered so far. */` |
|    7 | 1222 | `			fnum = (double)num;` |
|    7 | 1223 | `			mode = 1;` |
|    3 | 1224 | `		}` |
|    7 | 1225 | `		fnum = fnum * base + c;` |
|    4 | 1226 | `	}` |
|   91 | 1227 | `	if( bIgnored ){` |
|    - | 1228 | `		/* php 8: characters that are not valid digits for this base are skipped,` |
|    - | 1229 | `		 * and the skipping itself is deprecated (the VALUE is unaffected). */` |
|   14 | 1230 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|    - | 1231 | `			"Invalid characters passed for attempted conversion, these have been ignored");` |
|    6 | 1232 | `	}` |
|   91 | 1233 | `	if( mode == 1 ){` |
|    7 | 1234 | `		ph7_result_double(pCtx,fnum);` |
|    4 | 1235 | `	}else{` |
|   85 | 1236 | `		ph7_result_int64(pCtx,num);` |
|    - | 1237 | `	}` |
|   91 | 1238 | `}` |
|    - | 1239 | `/*` |
|    - | 1240 | ` * int64 hexdec(string $hex_string)` |
|    - | 1241 | ` *  Hexadecimal to decimal.` |
|    - | 1242 | ` * Parameters` |
|    - | 1243 | ` *  $hex_string` |
|    - | 1244 | ` *   The hexadecimal string to convert` |
|    - | 1245 | ` * Return` |
|    - | 1246 | ` *  The decimal representation of hex_string (int, or float on overflow)` |
|    - | 1247 | ` */` |
|   52 | 1248 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 1249 | `{` |
|    - | 1250 | `	const char *zString;` |
|    - | 1251 | `	int nLen;` |
|   55 | 1252 | `	if( nArg < 1 ){` |
|    - | 1253 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1254 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1255 | `		return PH7_OK;` |
|    - | 1256 | `	}` |
|   55 | 1257 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1258 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1259 | `		char zBuf[64];` |
|  ! 0 | 1260 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1261 | `			"hexdec(): Argument #1 ($hex_string) must be of type string, %s given",` |
|  ! 0 | 1262 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1263 | `	}` |
|    - | 1264 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1265 | `	 * hex-parses that (hexdec(255) == hexdec("255") == 0x255), so route every` |
|    - | 1266 | `	 * non-throwing value through ph7_value_to_string rather than reading it as` |
|    - | 1267 | `	 * a decimal integer. */` |
|   55 | 1268 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   55 | 1269 | `	MathBaseToNumber(pCtx,zString,nLen,16);` |
|   55 | 1270 | `	return PH7_OK;` |
|   29 | 1271 | `}` |
|    - | 1272 | `/*` |
|    - | 1273 | ` * int64 bindec(string $bin_string)` |
|    - | 1274 | ` *  Binary to decimal.` |
|    - | 1275 | ` * Parameters` |
|    - | 1276 | ` *  $bin_string` |
|    - | 1277 | ` *   The binary string to convert` |
|    - | 1278 | ` * Return` |
|    - | 1279 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|    - | 1280 | ` */` |
|   20 | 1281 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1282 | `{` |
|    - | 1283 | `	const char *zString;` |
|    - | 1284 | `	int nLen;` |
|   21 | 1285 | `	if( nArg < 1 ){` |
|    - | 1286 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1287 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1288 | `		return PH7_OK;` |
|    - | 1289 | `	}` |
|   21 | 1290 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1291 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1292 | `		char zBuf[64];` |
|  ! 0 | 1293 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1294 | `			"bindec(): Argument #1 ($binary_string) must be of type string, %s given",` |
|  ! 0 | 1295 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1296 | `	}` |
|    - | 1297 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1298 | `	 * binary-parses that (bindec(11) == bindec("11") == 3). */` |
|   21 | 1299 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   21 | 1300 | `	MathBaseToNumber(pCtx,zString,nLen,2);` |
|   21 | 1301 | `	return PH7_OK;` |
|   11 | 1302 | `}` |
|    - | 1303 | `/*` |
|    - | 1304 | ` * int64 octdec(string $oct_string)` |
|    - | 1305 | ` *  Octal to decimal.` |
|    - | 1306 | ` * Parameters` |
|    - | 1307 | ` *  $oct_string` |
|    - | 1308 | ` *   The octal string to convert` |
|    - | 1309 | ` * Return` |
|    - | 1310 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|    - | 1311 | ` */` |
|   16 | 1312 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1313 | `{` |
|    - | 1314 | `	const char *zString;` |
|    - | 1315 | `	int nLen;` |
|   17 | 1316 | `	if( nArg < 1 ){` |
|    - | 1317 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1318 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1319 | `		return PH7_OK;` |
|    - | 1320 | `	}` |
|   17 | 1321 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1322 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1323 | `		char zBuf[64];` |
|  ! 0 | 1324 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1325 | `			"octdec(): Argument #1 ($octal_string) must be of type string, %s given",` |
|  ! 0 | 1326 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1327 | `	}` |
|    - | 1328 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1329 | `	 * octal-parses that (octdec(11) == octdec("11") == 9). */` |
|   17 | 1330 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   17 | 1331 | `	MathBaseToNumber(pCtx,zString,nLen,8);` |
|   17 | 1332 | `	return PH7_OK;` |
|    9 | 1333 | `}` |
|    - | 1334 | `/*` |
|    - | 1335 | ` * srand([int $seed])` |
|    - | 1336 | ` * mt_srand([int $seed])` |
|    - | 1337 | ` *  Seed the random number generator.` |
|    - | 1338 | ` * Parameters` |
|    - | 1339 | ` * $seed` |
|    - | 1340 | ` *  Optional seed value` |
|    - | 1341 | ` * Return` |
|    - | 1342 | ` *  null.` |
|    - | 1343 | ` * Note:` |
|    - | 1344 | ` *  THIS FUNCTION IS A NO-OP.` |
|    - | 1345 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|    - | 1346 | ` */` |
|   20 | 1347 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1348 | `{` |
|   10 | 1349 | `	SXUNUSED(nArg);` |
|   10 | 1350 | `	SXUNUSED(apArg);` |
|   21 | 1351 | `	ph7_result_null(pCtx);` |
|   21 | 1352 | `	return PH7_OK;` |
|    1 | 1353 | `}` |
|    - | 1354 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - | 1355 | `/*` |
|    - | 1356 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|    - | 1357 | ` *  Convert a number between arbitrary bases.` |
|    - | 1358 | ` * Parameters` |
|    - | 1359 | ` * $number` |
|    - | 1360 | ` *  The number to convert` |
|    - | 1361 | ` * $frombase` |
|    - | 1362 | ` *  The base number is in` |
|    - | 1363 | ` * $tobase` |
|    - | 1364 | ` *  The base to convert number to` |
|    - | 1365 | ` * Return` |
|    - | 1366 | ` *  Number converted to base tobase` |
|    - | 1367 | ` */` |
|   58 | 1368 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1369 | `{` |
|    - | 1370 | `	static const char zDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";` |
|    - | 1371 | `	int nLen,iFbase,iTobase,i;` |
|    - | 1372 | `	int bIgnored;` |
|    - | 1373 | `	ph7_int64 iFbase64,iTobase64;` |
|    - | 1374 | `	const char *zNum;` |
|   59 | 1375 | `	sxu64 uNum = 0;` |
|   59 | 1376 | `	if( nArg < 3 ){` |
|    - | 1377 | `		/* Return the empty string*/` |
|  ! 0 | 1378 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1379 | `		return PH7_OK;` |
|    - | 1380 | `	}` |
|    - | 1381 | `	/* Base numbers. Read them as 64-bit so an out-of-range base can't wrap through` |
|    - | 1382 | `	 * a 32-bit truncation back into the 2..36 window and bypass the check below. */` |
|   59 | 1383 | `	iFbase64 = ph7_value_to_int64(apArg[1]);` |
|   59 | 1384 | `	iTobase64 = ph7_value_to_int64(apArg[2]);` |
|    - | 1385 | `	/* PHP 8 throws a catchable ValueError for a base outside 2..36; from_base` |
|    - | 1386 | `	 * is validated before to_base, both before the string is even parsed. */` |
|   59 | 1387 | `	if( iFbase64 < 2 \|\| iFbase64 > 36 ){` |
|    7 | 1388 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1389 | `			"base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)");` |
|    - | 1390 | `	}` |
|   53 | 1391 | `	if( iTobase64 < 2 \|\| iTobase64 > 36 ){` |
|    5 | 1392 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1393 | `			"base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)");` |
|    - | 1394 | `	}` |
|    - | 1395 | `	/* Both bases are now known to fit in [2,36], so the int form is exact. */` |
|   49 | 1396 | `	iFbase  = (int)iFbase64;` |
|   49 | 1397 | `	iTobase = (int)iTobase64;` |
|    - | 1398 | `	/* Parse the input number in from_base. Every base is handled the same way:` |
|    - | 1399 | `	 * digits 0-9 then a-z/A-Z map to 0-35; a character that is not a valid digit for` |
|    - | 1400 | `	 * from_base is ignored, and php raises an E_DEPRECATED saying so. */` |
|   49 | 1401 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    3 | 1402 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|    - | 1403 | `			"base_convert(): Passing null to parameter #1 ($num) of type string is deprecated");` |
|    1 | 1404 | `	}` |
|   49 | 1405 | `	zNum = ph7_value_to_string(apArg[0],&nLen);` |
|   49 | 1406 | `	bIgnored = 0;` |
|  147 | 1407 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   99 | 1408 | `		int c = (unsigned char)zNum[i];` |
|    - | 1409 | `		int d;` |
|   99 | 1410 | `		if( c >= '0' && c <= '9' ){` |
|   73 | 1411 | `			d = c - '0';` |
|   63 | 1412 | `		}else if( c >= 'a' && c <= 'z' ){` |
|   25 | 1413 | `			d = c - 'a' + 10;` |
|   15 | 1414 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|    3 | 1415 | `			d = c - 'A' + 10;` |
|    2 | 1416 | `		}else{` |
|  ! 0 | 1417 | `			d = 99;` |
|    - | 1418 | `		}` |
|   99 | 1419 | `		if( d >= iFbase ){` |
|    - | 1420 | `			/* Not a valid digit for this base: skip it (php), but say so afterwards. */` |
|    3 | 1421 | `			bIgnored = 1;` |
|    3 | 1422 | `			continue;` |
|    - | 1423 | `		}` |
|   97 | 1424 | `		uNum = uNum * (sxu64)iFbase + (sxu64)d;` |
|   49 | 1425 | `	}` |
|   49 | 1426 | `	if( bIgnored ){` |
|    3 | 1427 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|    - | 1428 | `			"Invalid characters passed for attempted conversion, these have been ignored");` |
|    1 | 1429 | `	}` |
|    - | 1430 | `	/* Format the result in to_base using lowercase digits. */` |
|   49 | 1431 | `	if( uNum == 0 ){` |
|    9 | 1432 | `		ph7_result_string(pCtx,"0",1);` |
|    5 | 1433 | `	}else{` |
|    - | 1434 | `		char zOut[70]; /* base-2 of a 64-bit value fits in 64 digits */` |
|   41 | 1435 | `		int n = 0,j;` |
|  133 | 1436 | `		while( uNum > 0 ){` |
|   93 | 1437 | `			zOut[n++] = zDigits[uNum % (sxu64)iTobase];` |
|   93 | 1438 | `			uNum /= (sxu64)iTobase;` |
|    1 | 1439 | `		}` |
|    - | 1440 | `		/* Digits were produced least-significant first: reverse in place. */` |
|   79 | 1441 | `		for( j = 0 ; j < n/2 ; ++j ){` |
|   39 | 1442 | `			char t = zOut[j];` |
|   39 | 1443 | `			zOut[j] = zOut[n - 1 - j];` |
|   39 | 1444 | `			zOut[n - 1 - j] = t;` |
|   20 | 1445 | `		}` |
|   41 | 1446 | `		ph7_result_string(pCtx,zOut,n);` |
|    - | 1447 | `	}` |
|   49 | 1448 | `	return PH7_OK;` |
|   30 | 1449 | `}` |
|    - | 1450 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - | 1451 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1452 |  |
