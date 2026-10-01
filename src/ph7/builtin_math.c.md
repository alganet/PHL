# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 732/862 lines (84.92%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * round() (defined below, under PH7_DISABLE_BUILTIN_FUNC rather than the` |
|      - |    9 | ` * math-func guard) needs floor/ceil/fabs/copysign/fmod/isfinite/pow plus the` |
|      - |   10 | ` * libc snprintf/strtod round-trip for its high-precision branch, so pull these` |
|      - |   11 | ` * in unconditionally here — they must be available even when` |
|      - |   12 | ` * PH7_ENABLE_MATH_FUNC is off. abs() is also used by the guarded math builtins.` |
|      - |   13 | ` */` |
|      - |   14 | `#include <math.h>` |
|      - |   15 | `#include <stdio.h>  /* snprintf: correctly-rounded high-precision round() round-trip */` |
|      - |   16 | `#include <stdlib.h> /* strtod (round-trip inverse), abs */` |
|      - |   17 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |   18 |  |
|      - |   19 | `/*` |
|      - |   20 | ` * Section:` |
|      - |   21 | ` *    Math Functions.` |
|      - |   22 |  |
|      - |   23 | ` * Status:` |
|      - |   24 | ` *    Stable.` |
|      - |   25 | ` */` |
|      - |   26 | `/*` |
|      - |   27 | ` * float sqrt(float $arg )` |
|      - |   28 | ` *  Square root of the given number.` |
|      - |   29 | ` * Parameter` |
|      - |   30 | ` *  The number to process.` |
|      - |   31 | ` * Return` |
|      - |   32 | ` *  The square root of arg or the special value Nan of failure.` |
|      - |   33 | ` */` |
|      - |   34 | `/*` |
|      - |   35 | ` * The libm-backed float functions php has and PH7 lacked entirely. Doing these in PHP` |
|      - |   36 | ` * (log1p as log(1+x), acosh via logs, ...) would lose precision, so they go through libm.` |
|      - |   37 | ` */` |
|      - |   38 | `#define PH7_MATH_UNARY(NAME,CFUNC)                                        \` |
|      - |   39 | `PH7_PRIVATE int PH7_builtin_##NAME(ph7_context *pCtx,int nArg,ph7_value **apArg) \` |
|      - |   40 | `{                                                                          \` |
|      - |   41 | `	double x;                                                              \` |
|      - |   42 | `	if( nArg < 1 ){                                                        \` |
|      - |   43 | `		ph7_result_int(pCtx,0);                                            \` |
|      - |   44 | `		return PH7_OK;                                                     \` |
|      - |   45 | `	}                                                                      \` |
|      - |   46 | `	x = ph7_value_to_double(apArg[0]);                                     \` |
|      - |   47 | `	ph7_result_double(pCtx,CFUNC(x));                                      \` |
|      - |   48 | `	return PH7_OK;                                                          \` |
|      - |   49 | `}` |
|      3 |   50 | `PH7_MATH_UNARY(acosh,acosh)` |
|      3 |   51 | `PH7_MATH_UNARY(asinh,asinh)` |
|      3 |   52 | `PH7_MATH_UNARY(atanh,atanh)` |
|      3 |   53 | `PH7_MATH_UNARY(expm1,expm1)` |
|      3 |   54 | `PH7_MATH_UNARY(log1p,log1p)` |
|      2 |   55 | `PH7_PRIVATE int PH7_builtin_deg2rad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   56 | `{` |
|      - |   57 | `	double x;` |
|      3 |   58 | `	if( nArg < 1 ){` |
|    ! 0 |   59 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |   60 | `		return PH7_OK;` |
|      - |   61 | `	}` |
|      3 |   62 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |   63 | `	ph7_result_double(pCtx,x * (3.14159265358979323846 / 180.0));` |
|      3 |   64 | `	return PH7_OK;` |
|      2 |   65 | `}` |
|      6 |   66 | `PH7_PRIVATE int PH7_builtin_rad2deg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   67 | `{` |
|      - |   68 | `	double x;` |
|      8 |   69 | `	if( nArg < 1 ){` |
|    ! 0 |   70 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |   71 | `		return PH7_OK;` |
|      - |   72 | `	}` |
|      8 |   73 | `	x = ph7_value_to_double(apArg[0]);` |
|      8 |   74 | `	ph7_result_double(pCtx,x * (180.0 / 3.14159265358979323846));` |
|      8 |   75 | `	return PH7_OK;` |
|      5 |   76 | `}` |
|      4 |   77 | `PH7_PRIVATE int PH7_builtin_fpow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   78 | `{` |
|      - |   79 | `	double x,y;` |
|      6 |   80 | `	if( nArg < 2 ){` |
|    ! 0 |   81 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |   82 | `		return PH7_OK;` |
|      - |   83 | `	}` |
|      6 |   84 | `	x = ph7_value_to_double(apArg[0]);` |
|      6 |   85 | `	y = ph7_value_to_double(apArg[1]);` |
|      6 |   86 | `	ph7_result_double(pCtx,pow(x,y));` |
|      6 |   87 | `	return PH7_OK;` |
|      4 |   88 | `}` |
|     16 |   89 | `PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   90 | `{` |
|      - |   91 | `	double r,x;` |
|     17 |   92 | `	if( nArg < 1 ){` |
|      - |   93 | `		/* Missing argument,return 0 */` |
|    ! 0 |   94 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |   95 | `		return PH7_OK;` |
|      - |   96 | `	}` |
|     17 |   97 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |   98 | `	/* Perform the requested operation */` |
|     17 |   99 | `	r = sqrt(x);` |
|      - |  100 | `	/* store the result back */` |
|     17 |  101 | `	ph7_result_double(pCtx,r);` |
|     17 |  102 | `	return PH7_OK;` |
|      9 |  103 | `}` |
|      - |  104 | `/*` |
|      - |  105 | ` * float exp(float $arg )` |
|      - |  106 | ` *  Calculates the exponent of e.` |
|      - |  107 | ` * Parameter` |
|      - |  108 | ` *  The number to process.` |
|      - |  109 | ` * Return` |
|      - |  110 | ` *  'e' raised to the power of arg.` |
|      - |  111 | ` */` |
|     18 |  112 | `PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  113 | `{` |
|      - |  114 | `	double r,x;` |
|     19 |  115 | `	if( nArg < 1 ){` |
|      - |  116 | `		/* Missing argument,return 0 */` |
|    ! 0 |  117 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  118 | `		return PH7_OK;` |
|      - |  119 | `	}` |
|     19 |  120 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  121 | `	/* Perform the requested operation */` |
|     19 |  122 | `	r = exp(x);` |
|      - |  123 | `	/* store the result back */` |
|     19 |  124 | `	ph7_result_double(pCtx,r);` |
|     19 |  125 | `	return PH7_OK;` |
|     10 |  126 | `}` |
|      - |  127 | `/*` |
|      - |  128 | ` * float floor(float $arg )` |
|      - |  129 | ` *  Round fractions down.` |
|      - |  130 | ` * Parameter` |
|      - |  131 | ` *  The number to process.` |
|      - |  132 | ` * Return` |
|      - |  133 | ` *  Returns the next lowest integer value by rounding down value if necessary.` |
|      - |  134 | ` */` |
|     22 |  135 | `PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  136 | `{` |
|      - |  137 | `	double r,x;` |
|      - |  138 | `	/* PHP requires exactly one argument. */` |
|     23 |  139 | `	if( nArg != 1 ){` |
|    ! 0 |  140 | `		return PH7_VmThrowException(pCtx,` |
|      - |  141 | `			"ArgumentCountError",` |
|      - |  142 | `			"floor() expects exactly 1 argument, %d given",` |
|    ! 0 |  143 | `			nArg` |
|      - |  144 | `			);` |
|      - |  145 | `	}` |
|      - |  146 | ``	/* The `int\|float $num` row in aBuiltinSig[] screens this argument before the`` |
|      - |  147 | `	 * call: array, object, resource and non-numeric string are refused there,` |
|      - |  148 | `	 * with php's wording. The hand-rolled copy that used to sit here refused a` |
|      - |  149 | `	 * BOOL as well, which weak mode converts (php: ceil(true) is float(1)), and` |
|      - |  150 | `	 * one of its two branches had no ", %s given" tail at all. */` |
|      - |  151 |  |
|     23 |  152 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  153 | `	/* Perform the requested operation */` |
|     23 |  154 | `	r = floor(x);` |
|      - |  155 | `	/* store the result back */` |
|     23 |  156 | `	ph7_result_double(pCtx,r);` |
|     23 |  157 | `	return PH7_OK;` |
|     12 |  158 | `}` |
|      - |  159 | `/*` |
|      - |  160 | ` * float cos(float $arg )` |
|      - |  161 | ` *  Cosine.` |
|      - |  162 | ` * Parameter` |
|      - |  163 | ` *  The number to process.` |
|      - |  164 | ` * Return` |
|      - |  165 | ` *  The cosine of arg.` |
|      - |  166 | ` */` |
|      2 |  167 | `PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  168 | `{` |
|      - |  169 | `	double r,x;` |
|      3 |  170 | `	if( nArg < 1 ){` |
|      - |  171 | `		/* Missing argument,return 0 */` |
|    ! 0 |  172 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  173 | `		return PH7_OK;` |
|      - |  174 | `	}` |
|      3 |  175 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  176 | `	/* Perform the requested operation */` |
|      3 |  177 | `	r = cos(x);` |
|      - |  178 | `	/* store the result back */` |
|      3 |  179 | `	ph7_result_double(pCtx,r);` |
|      3 |  180 | `	return PH7_OK;` |
|      2 |  181 | `}` |
|      - |  182 | `/*` |
|      - |  183 | ` * float acos(float $arg )` |
|      - |  184 | ` *  Arc cosine.` |
|      - |  185 | ` * Parameter` |
|      - |  186 | ` *  The number to process.` |
|      - |  187 | ` * Return` |
|      - |  188 | ` *  The arc cosine of arg.` |
|      - |  189 | ` */` |
|     16 |  190 | `PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  191 | `{` |
|      - |  192 | `	char zGiven[64];` |
|      - |  193 | `	double r, x;` |
|      - |  194 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|     17 |  195 | `	if( nArg != 1 ){` |
|    ! 0 |  196 | `		return PH7_VmThrowException(pCtx,` |
|      - |  197 | `			"ArgumentCountError",` |
|      - |  198 | `			"acos() expects exactly 1 argument, %d given",` |
|    ! 0 |  199 | `			nArg` |
|      - |  200 | `			);` |
|      - |  201 | `	}` |
|      - |  202 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|      - |  203 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|      - |  204 | `	 * the float conversion will handle them. */` |
|     17 |  205 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    ! 0 |  206 | `		return PH7_VmThrowException(pCtx,` |
|      - |  207 | `			"TypeError",` |
|      - |  208 | `			"acos(): Argument #1 ($num) must be of type float, %s given",` |
|    ! 0 |  209 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|      - |  210 | `			);` |
|      - |  211 | `	}` |
|      - |  212 | `	/* Convert to double now that we know it's numeric. */` |
|     17 |  213 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  214 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|     17 |  215 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|      5 |  216 | `		r = PH7_NAN_VALUE();` |
|      3 |  217 | `	}else{` |
|     13 |  218 | `		r = acos(x);` |
|      - |  219 | `	}` |
|      - |  220 | `	/* store the result back */` |
|     17 |  221 | `	ph7_result_double(pCtx,r);` |
|     17 |  222 | `	return PH7_OK;` |
|      9 |  223 | `}` |
|      - |  224 | `/*` |
|      - |  225 | ` * float cosh(float $arg )` |
|      - |  226 | ` *  Hyperbolic cosine.` |
|      - |  227 | ` * Parameter` |
|      - |  228 | ` *  The number to process.` |
|      - |  229 | ` * Return` |
|      - |  230 | ` *  The hyperbolic cosine of arg.` |
|      - |  231 | ` */` |
|     16 |  232 | `PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  233 | `{` |
|      - |  234 | `	double r,x;` |
|     17 |  235 | `	if( nArg < 1 ){` |
|      - |  236 | `		/* Missing argument,return 0 */` |
|    ! 0 |  237 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  238 | `		return PH7_OK;` |
|      - |  239 | `	}` |
|     17 |  240 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  241 | `	/* Perform the requested operation */` |
|     17 |  242 | `	r = cosh(x);` |
|      - |  243 | `	/* store the result back */` |
|     17 |  244 | `	ph7_result_double(pCtx,r);` |
|     17 |  245 | `	return PH7_OK;` |
|      9 |  246 | `}` |
|      - |  247 | `/*` |
|      - |  248 | ` * float sin(float $arg )` |
|      - |  249 | ` *  Sine.` |
|      - |  250 | ` * Parameter` |
|      - |  251 | ` *  The number to process.` |
|      - |  252 | ` * Return` |
|      - |  253 | ` *  The sine of arg.` |
|      - |  254 | ` */` |
|      2 |  255 | `PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  256 | `{` |
|      - |  257 | `	double r,x;` |
|      3 |  258 | `	if( nArg < 1 ){` |
|      - |  259 | `		/* Missing argument,return 0 */` |
|    ! 0 |  260 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  261 | `		return PH7_OK;` |
|      - |  262 | `	}` |
|      3 |  263 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  264 | `	/* Perform the requested operation */` |
|      3 |  265 | `	r = sin(x);` |
|      - |  266 | `	/* store the result back */` |
|      3 |  267 | `	ph7_result_double(pCtx,r);` |
|      3 |  268 | `	return PH7_OK;` |
|      2 |  269 | `}` |
|      - |  270 | `/*` |
|      - |  271 | ` * float asin(float $arg )` |
|      - |  272 | ` *  Arc sine.` |
|      - |  273 | ` * Parameter` |
|      - |  274 | ` *  The number to process.` |
|      - |  275 | ` * Return` |
|      - |  276 | ` *  The arc sine of arg.` |
|      - |  277 | ` */` |
|     16 |  278 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  279 | `{` |
|      - |  280 | `	char zGiven[64];` |
|      - |  281 | `	double r, x;` |
|      - |  282 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|     17 |  283 | `	if( nArg != 1 ){` |
|    ! 0 |  284 | `		return PH7_VmThrowException(pCtx,` |
|      - |  285 | `			"ArgumentCountError",` |
|      - |  286 | `			"asin() expects exactly 1 argument, %d given",` |
|    ! 0 |  287 | `			nArg` |
|      - |  288 | `			);` |
|      - |  289 | `	}` |
|      - |  290 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|      - |  291 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|      - |  292 | `	 * the float conversion will handle them. */` |
|     17 |  293 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    ! 0 |  294 | `		return PH7_VmThrowException(pCtx,` |
|      - |  295 | `			"TypeError",` |
|      - |  296 | `			"asin(): Argument #1 ($num) must be of type float, %s given",` |
|    ! 0 |  297 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|      - |  298 | `			);` |
|      - |  299 | `	}` |
|      - |  300 | `	/* Convert to double now that we know it's numeric. */` |
|     17 |  301 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  302 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|     17 |  303 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|      5 |  304 | `		r = PH7_NAN_VALUE();` |
|      3 |  305 | `	}else{` |
|     13 |  306 | `		r = asin(x);` |
|      - |  307 | `	}` |
|      - |  308 | `	/* store the result back */` |
|     17 |  309 | `	ph7_result_double(pCtx,r);` |
|     17 |  310 | `	return PH7_OK;` |
|      9 |  311 | `}` |
|      - |  312 | `/*` |
|      - |  313 | ` * float sinh(float $arg )` |
|      - |  314 | ` *  Hyperbolic sine.` |
|      - |  315 | ` * Parameter` |
|      - |  316 | ` *  The number to process.` |
|      - |  317 | ` * Return` |
|      - |  318 | ` *  The hyperbolic sine of arg.` |
|      - |  319 | ` */` |
|     18 |  320 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  321 | `{` |
|      - |  322 | `	double r,x;` |
|     19 |  323 | `	if( nArg < 1 ){` |
|      - |  324 | `		/* Missing argument,return 0 */` |
|    ! 0 |  325 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  326 | `		return PH7_OK;` |
|      - |  327 | `	}` |
|     19 |  328 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  329 | `	/* Perform the requested operation */` |
|     19 |  330 | `	r = sinh(x);` |
|      - |  331 | `	/* store the result back */` |
|     19 |  332 | `	ph7_result_double(pCtx,r);` |
|     19 |  333 | `	return PH7_OK;` |
|     10 |  334 | `}` |
|      - |  335 | `/*` |
|      - |  336 | ` * float ceil(float $arg )` |
|      - |  337 | ` *  Round fractions up.` |
|      - |  338 | ` * Parameter` |
|      - |  339 | ` *  The number to process.` |
|      - |  340 | ` * Return` |
|      - |  341 | ` *  The next highest integer value by rounding up value if necessary.` |
|      - |  342 | ` */` |
|     18 |  343 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  344 | `{` |
|      - |  345 | `	double r,x;` |
|      - |  346 | `	/* PHP requires exactly one argument. */` |
|     19 |  347 | `	if( nArg != 1 ){` |
|    ! 0 |  348 | `		return PH7_VmThrowException(pCtx,` |
|      - |  349 | `			"ArgumentCountError",` |
|      - |  350 | `			"ceil() expects exactly 1 argument, %d given",` |
|    ! 0 |  351 | `			nArg` |
|      - |  352 | `			);` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Type screening is the aBuiltinSig[] row's -- see floor() above. */` |
|      - |  355 |  |
|     19 |  356 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  357 | `	/* Perform the requested operation */` |
|     19 |  358 | `	r = ceil(x);` |
|      - |  359 | `	/* store the result back */` |
|     19 |  360 | `	ph7_result_double(pCtx,r);` |
|     19 |  361 | `	return PH7_OK;` |
|     10 |  362 | `}` |
|      - |  363 | `/*` |
|      - |  364 | ` * float tan(float $arg )` |
|      - |  365 | ` *  Tangent.` |
|      - |  366 | ` * Parameter` |
|      - |  367 | ` *  The number to process.` |
|      - |  368 | ` * Return` |
|      - |  369 | ` *  The tangent of arg.` |
|      - |  370 | ` */` |
|      4 |  371 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  372 | `{` |
|      - |  373 | `	double r,x;` |
|      5 |  374 | `	if( nArg < 1 ){` |
|      - |  375 | `		/* Missing argument,return 0 */` |
|    ! 0 |  376 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  377 | `		return PH7_OK;` |
|      - |  378 | `	}` |
|      5 |  379 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  380 | `	/* Perform the requested operation */` |
|      5 |  381 | `	r = tan(x);` |
|      - |  382 | `	/* store the result back */` |
|      5 |  383 | `	ph7_result_double(pCtx,r);` |
|      5 |  384 | `	return PH7_OK;` |
|      3 |  385 | `}` |
|      - |  386 | `/*` |
|      - |  387 | ` * float atan(float $arg )` |
|      - |  388 | ` *  Arc tangent.` |
|      - |  389 | ` * Parameter` |
|      - |  390 | ` *  The number to process.` |
|      - |  391 | ` * Return` |
|      - |  392 | ` *  The arc tangent of arg.` |
|      - |  393 | ` */` |
|     32 |  394 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  395 | `{` |
|      - |  396 | `	char zGiven[64];` |
|      - |  397 | `	double r,x;` |
|      - |  398 | `	/* PHP enforces exactly one argument. */` |
|     33 |  399 | `	if( nArg != 1 ){` |
|    ! 0 |  400 | `		return PH7_VmThrowException(pCtx,` |
|      - |  401 | `			"ArgumentCountError",` |
|      - |  402 | `			"atan() expects exactly 1 argument, %d given",` |
|    ! 0 |  403 | `			nArg` |
|      - |  404 | `			);` |
|      - |  405 | `	}` |
|      - |  406 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, non-numeric strings).` |
|      - |  407 | `	 * PHP 8 reports a TypeError for wrong types. */` |
|     33 |  408 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    ! 0 |  409 | `		return PH7_VmThrowException(pCtx,` |
|      - |  410 | `			"TypeError",` |
|      - |  411 | `			"atan(): Argument #1 ($num) must be of type float, %s given",` |
|    ! 0 |  412 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|      - |  413 | `			);` |
|      - |  414 | `	}` |
|     33 |  415 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  416 | `	/* Perform the requested operation */` |
|     33 |  417 | `	r = atan(x);` |
|      - |  418 | `	/* store the result back */` |
|     33 |  419 | `	ph7_result_double(pCtx,r);` |
|     33 |  420 | `	return PH7_OK;` |
|     17 |  421 | `}` |
|      - |  422 | `/*` |
|      - |  423 | ` * float tanh(float $arg )` |
|      - |  424 | ` *  Hyperbolic tangent.` |
|      - |  425 | ` * Parameter` |
|      - |  426 | ` *  The number to process.` |
|      - |  427 | ` * Return` |
|      - |  428 | ` *  The Hyperbolic tangent of arg.` |
|      - |  429 | ` */` |
|     18 |  430 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  431 | `{` |
|      - |  432 | `	double r,x;` |
|     19 |  433 | `	if( nArg < 1 ){` |
|      - |  434 | `		/* Missing argument,return 0 */` |
|    ! 0 |  435 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  436 | `		return PH7_OK;` |
|      - |  437 | `	}` |
|     19 |  438 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  439 | `	/* Perform the requested operation */` |
|     19 |  440 | `	r = tanh(x);` |
|      - |  441 | `	/* store the result back */` |
|     19 |  442 | `	ph7_result_double(pCtx,r);` |
|     19 |  443 | `	return PH7_OK;` |
|     10 |  444 | `}` |
|      - |  445 | `/*` |
|      - |  446 | ` * float atan2(float $y,float $x)` |
|      - |  447 | ` *  Arc tangent of two variable.` |
|      - |  448 | ` * Parameter` |
|      - |  449 | ` *  $y = Dividend parameter.` |
|      - |  450 | ` *  $x = Divisor parameter.` |
|      - |  451 | ` * Return` |
|      - |  452 | ` *  The arc tangent of y/x in radian.` |
|      - |  453 | ` */` |
|     46 |  454 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  455 | `{` |
|      - |  456 | `	char zGiven[64];` |
|      - |  457 | `	double r,x,y;` |
|      - |  458 | `	/* PHP enforces exactly two arguments. */` |
|     47 |  459 | `	if( nArg != 2 ){` |
|    ! 0 |  460 | `		return PH7_VmThrowException(pCtx,` |
|      - |  461 | `			"ArgumentCountError",` |
|      - |  462 | `			"atan2() expects exactly 2 arguments, %d given",` |
|    ! 0 |  463 | `			nArg` |
|      - |  464 | `			);` |
|      - |  465 | `	}` |
|      - |  466 | `	/* Type checking: reject non-numeric values for $y (argument #1). */` |
|     47 |  467 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    ! 0 |  468 | `		return PH7_VmThrowException(pCtx,` |
|      - |  469 | `			"TypeError",` |
|      - |  470 | `			"atan2(): Argument #1 ($y) must be of type float, %s given",` |
|    ! 0 |  471 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|      - |  472 | `			);` |
|      - |  473 | `	}` |
|      - |  474 | `	/* Type checking: reject non-numeric values for $x (argument #2). */` |
|     47 |  475 | `	if( !ph7_value_is_numeric(apArg[1]) ){` |
|    ! 0 |  476 | `		return PH7_VmThrowException(pCtx,` |
|      - |  477 | `			"TypeError",` |
|      - |  478 | `			"atan2(): Argument #2 ($x) must be of type float, %s given",` |
|    ! 0 |  479 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|      - |  480 | `			);` |
|      - |  481 | `	}` |
|     47 |  482 | `	y = ph7_value_to_double(apArg[0]);` |
|     47 |  483 | `	x = ph7_value_to_double(apArg[1]);` |
|      - |  484 | `	/* Perform the requested operation */` |
|     47 |  485 | `	r = atan2(y,x);` |
|      - |  486 | `	/* store the result back */` |
|     47 |  487 | `	ph7_result_double(pCtx,r);` |
|     47 |  488 | `	return PH7_OK;` |
|     24 |  489 | `}` |
|      - |  490 | `/*` |
|      - |  491 | ` * float/int64 abs(float/int64 $arg )` |
|      - |  492 | ` *  Absolute value.` |
|      - |  493 | ` * Parameter` |
|      - |  494 | ` *  The number to process.` |
|      - |  495 | ` * Return` |
|      - |  496 | ` *  The absolute value of number.` |
|      - |  497 | ` */` |
|    148 |  498 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  499 | `{` |
|      - |  500 | `	int is_float;` |
|      - |  501 | `	/* PHP requires exactly one argument. */` |
|    151 |  502 | `	if( nArg != 1 ){` |
|    ! 0 |  503 | `		return PH7_VmThrowException(pCtx,` |
|      - |  504 | `			"ArgumentCountError",` |
|      - |  505 | `			"abs() expects exactly 1 argument, %d given",` |
|    ! 0 |  506 | `			nArg` |
|      - |  507 | `			);` |
|      - |  508 | `	}` |
|      - |  509 |  |
|    151 |  510 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  511 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|    ! 0 |  512 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  513 | `			"abs(): Argument #1 ($num) must be of type int\|float, null given");` |
|      - |  514 | `	}` |
|      - |  515 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|    151 |  516 | `	is_float = ph7_value_is_float(apArg[0]);` |
|    151 |  517 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|      - |  518 | `		int len;` |
|      9 |  519 | `		sxu8 bReal = FALSE;` |
|      9 |  520 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|      - |  521 | `		sxi32 rcNum;` |
|      9 |  522 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|      9 |  523 | `		if( rcNum != SXRET_OK ){` |
|    ! 0 |  524 | `			return PH7_VmThrowException(pCtx,` |
|      - |  525 | `				"TypeError",` |
|      - |  526 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|      - |  527 | `				);` |
|      - |  528 | `		}` |
|      9 |  529 | `		if( bReal ){` |
|      7 |  530 | `			is_float = 1;` |
|      3 |  531 | `		}` |
|      4 |  532 | `	}` |
|    151 |  533 | `	if( is_float ){` |
|      - |  534 | `		double r,x;` |
|    113 |  535 | `		x = ph7_value_to_double(apArg[0]);` |
|      - |  536 | `		/* Perform the requested operation */` |
|    113 |  537 | `		r = fabs(x);` |
|    113 |  538 | `		ph7_result_double(pCtx,r);` |
|     57 |  539 | `	}else{` |
|      - |  540 | ``		/* Read the full 64-bit value (the old 32-bit `int abs()` truncated any`` |
|      - |  541 | `		 * magnitude above 2^31 and was UB on INT_MIN). */` |
|     39 |  542 | `		sxi64 x = ph7_value_to_int64(apArg[0]);` |
|     39 |  543 | `		if( x == SMALLEST_INT64 ){` |
|      - |  544 | `			/* abs(PHP_INT_MIN) has no int representation, so PHP returns a float. */` |
|      3 |  545 | `			ph7_result_double(pCtx,-(double)x);` |
|      2 |  546 | `		}else{` |
|     37 |  547 | `			ph7_result_int64(pCtx,x < 0 ? -x : x);` |
|      - |  548 | `		}` |
|      - |  549 | `	}` |
|    151 |  550 | `	return PH7_OK;` |
|     77 |  551 | `}` |
|      - |  552 | `/*` |
|      - |  553 | ` * float log(float $arg,[int/float $base])` |
|      - |  554 | ` *  Natural logarithm.` |
|      - |  555 | ` * Parameter` |
|      - |  556 | ` *  $arg: The number to process.` |
|      - |  557 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|      - |  558 | ` * Return` |
|      - |  559 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|      - |  560 | ` * Note:` |
|      - |  561 | ` *  only Natural log and base-10 log are supported.` |
|      - |  562 | ` */` |
|     12 |  563 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  564 | `{` |
|      - |  565 | `	double r,x;` |
|     13 |  566 | `	if( nArg < 1 ){` |
|      - |  567 | `		/* Missing argument,return 0 */` |
|    ! 0 |  568 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  569 | `		return PH7_OK;` |
|      - |  570 | `	}` |
|     13 |  571 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  572 | `	/* Perform the requested operation */` |
|     13 |  573 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|      - |  574 | `		/* Base-10 log */` |
|      5 |  575 | `		r = log10(x);` |
|      3 |  576 | `	}else{` |
|      9 |  577 | `		r = log(x);` |
|      - |  578 | `	}` |
|      - |  579 | `	/* store the result back */` |
|     13 |  580 | `	ph7_result_double(pCtx,r);` |
|     13 |  581 | `	return PH7_OK;` |
|      7 |  582 | `}` |
|      - |  583 | `/*` |
|      - |  584 | ` * float log10(float $arg )` |
|      - |  585 | ` *  Base-10 logarithm.` |
|      - |  586 | ` * Parameter` |
|      - |  587 | ` *  The number to process.` |
|      - |  588 | ` * Return` |
|      - |  589 | ` *  The Base-10 logarithm of the given number.` |
|      - |  590 | ` */` |
|     14 |  591 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  592 | `{` |
|      - |  593 | `	double r,x;` |
|     15 |  594 | `	if( nArg < 1 ){` |
|      - |  595 | `		/* Missing argument,return 0 */` |
|    ! 0 |  596 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  597 | `		return PH7_OK;` |
|      - |  598 | `	}` |
|     15 |  599 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  600 | `	/* Perform the requested operation */` |
|     15 |  601 | `	r = log10(x);` |
|      - |  602 | `	/* store the result back */` |
|     15 |  603 | `	ph7_result_double(pCtx,r);` |
|     15 |  604 | `	return PH7_OK;` |
|      8 |  605 | `}` |
|      - |  606 | `/*` |
|      - |  607 | ` * mixed pow(mixed $num,mixed $exponent)` |
|      - |  608 | ` *  Exponential expression.` |
|      - |  609 | ` *` |
|      - |  610 | `` *  php does not implement pow() separately: the function and the `**` operator`` |
|      - |  611 | ` *  are the same ZEND_API pow_function, so they share an operand contract, a` |
|      - |  612 | ` *  result TYPE rule and every edge value. Reading the two arguments as doubles` |
|      - |  613 | `` *  and returning pow() shared none of it -- `pow(2,3)` answered float(8) where`` |
|      - |  614 | `` *  `2 ** 3` answers int(8) (a wrong TYPE for the most ordinary call there is),`` |
|      - |  615 | `` *  and the contract `**` enforces was absent entirely: pow('abc',2) answered`` |
|      - |  616 | ` *  float(0), pow([1],2) float(1) and pow($obj,2) float(1) after a conversion` |
|      - |  617 | ` *  warning, where every one of them is` |
|      - |  618 | `` *  `TypeError: Unsupported operand types: … ** int`.`` |
|      - |  619 | ` *` |
|      - |  620 | ` *  Both halves now come from the operator: VmArithOperandCheck() for the` |
|      - |  621 | ` *  contract (including the "A non-numeric value encountered" warning a` |
|      - |  622 | ` *  leading-numeric string gets before it computes with the prefix) and` |
|      - |  623 | ` *  PH7_MemObjPow() for the arithmetic.` |
|      - |  624 | ` * Return` |
|      - |  625 | ` *  base raised to the power of exp -- int when both operands are int, the` |
|      - |  626 | ` *  exponent is non-negative and the exact result fits in an int64; float` |
|      - |  627 | ` *  otherwise.` |
|      - |  628 | ` */` |
|     58 |  629 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  630 | `{` |
|     61 |  631 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  632 | `	ph7_value sBase,sExp;` |
|      - |  633 | `	SyBlob sMsg;` |
|      - |  634 | `	sxi32 rc;` |
|      - |  635 | `	/* Arity (exactly 2) is enforced from aBuiltinArity[] before the call. */` |
|     61 |  636 | `	if( nArg < 2 ){` |
|    ! 0 |  637 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  638 | `		return PH7_OK;` |
|      - |  639 | `	}` |
|     61 |  640 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     61 |  641 | `	if( VmArithOperandCheck(pVm,apArg[0],apArg[1],"**",&sMsg) != SXRET_OK ){` |
|     19 |  642 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|     12 |  643 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|     13 |  644 | `		SyBlobRelease(&sMsg);` |
|     13 |  645 | `		return rc;` |
|      - |  646 | `	}` |
|     49 |  647 | `	SyBlobRelease(&sMsg);` |
|      - |  648 | `	/* Work on COPIES: PH7_MemObjPow converts its operands in place, which the` |
|      - |  649 | `	 * opcode arm may do to its stack slots but a builtin may not do to the` |
|      - |  650 | `	 * caller's arguments. */` |
|     49 |  651 | `	PH7_MemObjInit(pVm,&sBase);` |
|     49 |  652 | `	PH7_MemObjInit(pVm,&sExp);` |
|     49 |  653 | `	PH7_MemObjLoad(apArg[0],&sBase);` |
|     49 |  654 | `	PH7_MemObjLoad(apArg[1],&sExp);` |
|     49 |  655 | `	PH7_MemObjPow(&sBase,&sExp,&sBase);` |
|     49 |  656 | `	if( (sBase.iFlags & MEMOBJ_REAL) != 0 ){` |
|     19 |  657 | `		ph7_result_double(pCtx,sBase.rVal);` |
|     10 |  658 | `	}else{` |
|     31 |  659 | `		ph7_result_int64(pCtx,sBase.x.iVal);` |
|      - |  660 | `	}` |
|     49 |  661 | `	PH7_MemObjRelease(&sBase);` |
|     49 |  662 | `	PH7_MemObjRelease(&sExp);` |
|     49 |  663 | `	return PH7_OK;` |
|     32 |  664 | `}` |
|      - |  665 | `/*` |
|      - |  666 | ` * float pi(void)` |
|      - |  667 | ` *  Returns an approximation of pi.` |
|      - |  668 | ` * Note` |
|      - |  669 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|      - |  670 | ` * Return` |
|      - |  671 | ` *  The value of pi as float.` |
|      - |  672 | ` */` |
|      4 |  673 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  674 | `{` |
|      2 |  675 | `	SXUNUSED(nArg); /* cc warning */` |
|      2 |  676 | `	SXUNUSED(apArg);` |
|      6 |  677 | `	ph7_result_double(pCtx,PH7_PI);` |
|      6 |  678 | `	return PH7_OK;` |
|      2 |  679 | `}` |
|      - |  680 | `/*` |
|      - |  681 | ` * float fmod(float $x,float $y)` |
|      - |  682 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|      - |  683 | ` * Parameters` |
|      - |  684 | ` * $x` |
|      - |  685 | ` *  The dividend` |
|      - |  686 | ` * $y` |
|      - |  687 | ` *  The divisor` |
|      - |  688 | ` * Return` |
|      - |  689 | ` *  The floating point remainder of x/y.` |
|      - |  690 | ` */` |
|      2 |  691 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  692 | `{` |
|      - |  693 | `	double x,y,r;` |
|      3 |  694 | `	if( nArg < 2 ){` |
|      - |  695 | `		/* Missing arguments */` |
|    ! 0 |  696 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  697 | `		return PH7_OK;` |
|      - |  698 | `	}` |
|      - |  699 | `	/* Extract given arguments */` |
|      3 |  700 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  701 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  702 | `	/* Perform the requested operation */` |
|      3 |  703 | `	r = fmod(x,y);` |
|      - |  704 | `	/* Processing result */` |
|      3 |  705 | `	ph7_result_double(pCtx,r);` |
|      3 |  706 | `	return PH7_OK;` |
|      2 |  707 | `}` |
|      - |  708 | `/*` |
|      - |  709 | ` * float hypot(float $x,float $y)` |
|      - |  710 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|      - |  711 | ` * Parameters` |
|      - |  712 | ` * $x` |
|      - |  713 | ` *  Length of first side` |
|      - |  714 | ` * $y` |
|      - |  715 | ` *  Length of first side` |
|      - |  716 | ` * Return` |
|      - |  717 | ` *  Calculated length of the hypotenuse.` |
|      - |  718 | ` */` |
|      2 |  719 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  720 | `{` |
|      - |  721 | `	double x,y,r;` |
|      3 |  722 | `	if( nArg < 2 ){` |
|      - |  723 | `		/* Missing arguments */` |
|    ! 0 |  724 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  725 | `		return PH7_OK;` |
|      - |  726 | `	}` |
|      - |  727 | `	/* Extract given arguments */` |
|      3 |  728 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  729 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  730 | `	/* Perform the requested operation */` |
|      3 |  731 | `	r = hypot(x,y);` |
|      - |  732 | `	/* Processing result */` |
|      3 |  733 | `	ph7_result_double(pCtx,r);` |
|      3 |  734 | `	return PH7_OK;` |
|      2 |  735 | `}` |
|      - |  736 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  737 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  738 | `/* The PH7_ROUND_* mode numbering lives in ph7int.h: bcround() rounds by the` |
|      - |  739 | ` * same eight rules and reads the same RoundingMode cases. */` |
|      - |  740 | `/*` |
|      - |  741 | `` * php 8.4's `enum RoundingMode`, in php's own DECLARATION order -- which is the`` |
|      - |  742 | ` * order cases() reports and is NOT the order of the integer modes above. The two` |
|      - |  743 | ` * numberings disagree past the four HALF_* ones: php's integer 5 is CEILING and` |
|      - |  744 | ` * its enum's fifth case is TowardsZero, so the mapping has to be stated rather` |
|      - |  745 | ` * than computed from an ordinal. A PURE enum (no backing value), which is why the` |
|      - |  746 | `` * case carries its mode HERE instead of in a `case X = 5;` the script could read.`` |
|      - |  747 | ` *` |
|      - |  748 | ` * The enum is what round()'s third argument is documented as; the integer` |
|      - |  749 | ` * spelling stays accepted beside it because php still accepts it, which is what` |
|      - |  750 | `` * `RoundingMode\|int` in the signature says.`` |
|      - |  751 | ` */` |
|      - |  752 | `static const struct MathRoundingModeCase {` |
|      - |  753 | `	const char *zName;` |
|      - |  754 | `	int iMode;` |
|      - |  755 | `} aRoundingMode[] = {` |
|      - |  756 | `	{ "HalfAwayFromZero", PH7_ROUND_HALF_UP        },` |
|      - |  757 | `	{ "HalfTowardsZero",  PH7_ROUND_HALF_DOWN      },` |
|      - |  758 | `	{ "HalfEven",         PH7_ROUND_HALF_EVEN      },` |
|      - |  759 | `	{ "HalfOdd",          PH7_ROUND_HALF_ODD       },` |
|      - |  760 | `	{ "TowardsZero",      PH7_ROUND_TOWARD_ZERO    },` |
|      - |  761 | `	{ "AwayFromZero",     PH7_ROUND_AWAY_FROM_ZERO },` |
|      - |  762 | `	{ "NegativeInfinity", PH7_ROUND_FLOOR          },` |
|      - |  763 | `	{ "PositiveInfinity", PH7_ROUND_CEILING        },` |
|      - |  764 | `};` |
|      - |  765 | `/*` |
|      - |  766 | ` * Answer TRUE (and the integer mode) when pVal is a RoundingMode CASE.` |
|      - |  767 | ` *` |
|      - |  768 | ` * An enum case is an ordinary object here, so the test is its class plus the` |
|      - |  769 | `` * `name` slot every case carries -- the pure enum has no backing value to read.`` |
|      - |  770 | ` */` |
|    504 |  771 | `PH7_PRIVATE int PH7_RoundingModeCase(ph7_value *pVal,int *pMode)` |
|      1 |  772 | `{` |
|      - |  773 | `	ph7_class_instance *pObj;` |
|    505 |  774 | `	const char *zName = 0;` |
|    505 |  775 | `	int nName = 0;` |
|      - |  776 | `	sxu32 n;` |
|    505 |  777 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 ){` |
|    167 |  778 | `		return 0;` |
|      - |  779 | `	}` |
|    339 |  780 | `	pObj = (ph7_class_instance *)pVal->x.pOther;` |
|    338 |  781 | `	if( pObj->pClass == 0 \|\| pObj->pClass->sName.nByte != sizeof("RoundingMode")-1` |
|    339 |  782 | `	 \|\| SyMemcmp(pObj->pClass->sName.zString,"RoundingMode",sizeof("RoundingMode")-1) != 0 ){` |
|    ! 0 |  783 | `		return 0;` |
|      - |  784 | `	}` |
|    339 |  785 | `	PH7_NativeAttrStr(pObj,"name",&zName,&nName);` |
|   1537 |  786 | `	for( n = 0 ; n < SX_ARRAYSIZE(aRoundingMode) ; ++n ){` |
|   1537 |  787 | `		int nCase = (int)SyStrlen(aRoundingMode[n].zName);` |
|   1537 |  788 | `		if( nName == nCase && SyMemcmp(zName,aRoundingMode[n].zName,(sxu32)nCase) == 0 ){` |
|    339 |  789 | `			*pMode = aRoundingMode[n].iMode;` |
|    339 |  790 | `			return 1;` |
|      - |  791 | `		}` |
|    600 |  792 | `	}` |
|    ! 0 |  793 | `	return 0;` |
|    253 |  794 | `}` |
|      - |  795 | `/*` |
|      - |  796 | `` * Declare `enum RoundingMode` -- pure, eight cases, php's declaration order.`` |
|      - |  797 | ` */` |
|   6721 |  798 | `PH7_PRIVATE sxi32 PH7_VmInstallRoundingMode(ph7_vm *pVm)` |
|      5 |  799 | `{` |
|      - |  800 | `	/* The builder DUPLICATES each case name and, for an unbacked enum, keeps no` |
|      - |  801 | `	 * pointer into sValue at all, so this array may live on the stack. */` |
|      - |  802 | `	PH7_NativeEnumCase aCase[SX_ARRAYSIZE(aRoundingMode)];` |
|      - |  803 | `	sxu32 n;` |
|  60494 |  804 | `	for( n = 0 ; n < SX_ARRAYSIZE(aRoundingMode) ; ++n ){` |
|  53773 |  805 | `		aCase[n].zName = aRoundingMode[n].zName;` |
|      - |  806 | `		/* PH7_NATIVE_VAL_NULL: a pure enum's case has no backing value at all. */` |
|  53773 |  807 | `		aCase[n].sValue.zName = 0;` |
|  53773 |  808 | `		aCase[n].sValue.iMods = 0;` |
|  53773 |  809 | `		aCase[n].sValue.iType = PH7_NATIVE_VAL_NULL;` |
|  53773 |  810 | `		aCase[n].sValue.iValue = 0;` |
|  53773 |  811 | `		aCase[n].sValue.zValue = 0;` |
|  53773 |  812 | `		aCase[n].sValue.rValue = 0.0;` |
|  26853 |  813 | `	}` |
|  10082 |  814 | `	return PH7_InstallNativeEnum(&(*pVm),"RoundingMode",0,` |
|   3356 |  815 | `		aCase,SX_ARRAYSIZE(aCase),0,0);` |
|      5 |  816 | `}` |
|      - |  817 | `/*` |
|      - |  818 | ` * 10**power via an exact lookup table for 0..22, falling back to pow()` |
|      - |  819 | ` * otherwise. Port of php-src PHP-8.5 ext/standard/math.c php_intpow10().` |
|      - |  820 | ` */` |
|    420 |  821 | `static double MathIntPow10(int power)` |
|      4 |  822 | `{` |
|      - |  823 | `	static const double powers[] = {` |
|      - |  824 | `		1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,` |
|      - |  825 | `		1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22` |
|      - |  826 | `	};` |
|    424 |  827 | `	if( power < 0 \|\| power > 22 ){` |
|      5 |  828 | `		return pow(10.0, (double)power);` |
|      - |  829 | `	}` |
|    420 |  830 | `	return powers[power];` |
|    214 |  831 | `}` |
|    316 |  832 | `static double MathRoundBasicEdge(double integral, double exponent, int places)` |
|      4 |  833 | `{` |
|    162 |  834 | `	return (places > 0)` |
|     96 |  835 | `		? fabs((integral + copysign(0.5, integral)) / exponent)` |
|    268 |  836 | `		: fabs((integral + copysign(0.5, integral)) * exponent);` |
|      4 |  837 | `}` |
|     74 |  838 | `static double MathRoundZeroEdge(double integral, double exponent, int places)` |
|      1 |  839 | `{` |
|     38 |  840 | `	return (places > 0)` |
|    ! 0 |  841 | `		? fabs((integral) / exponent)` |
|     74 |  842 | `		: fabs((integral) * exponent);` |
|      1 |  843 | `}` |
|      - |  844 | `/*` |
|      - |  845 | ` * Round the extracted integral part according to the requested mode.` |
|      - |  846 | ` * Faithful port of php-src PHP-8.5 ext/standard/math.c php_round_helper().` |
|      - |  847 | ` */` |
|    414 |  848 | `static double MathRoundHelper(double integral, double value, double exponent, int places, int mode)` |
|      4 |  849 | `{` |
|    418 |  850 | `	double value_abs = fabs(value);` |
|      - |  851 | `	double edge_case;` |
|    418 |  852 | `	switch( mode ){` |
|    104 |  853 | `		case PH7_ROUND_HALF_UP:` |
|    212 |  854 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|    212 |  855 | `			if( value_abs >= edge_case ){` |
|    133 |  856 | `				return integral + copysign(1.0, integral);` |
|      - |  857 | `			}` |
|     82 |  858 | `			return integral;` |
|     16 |  859 | `		case PH7_ROUND_HALF_DOWN:` |
|     33 |  860 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|     33 |  861 | `			if( value_abs > edge_case ){` |
|    ! 0 |  862 | `				return integral + copysign(1.0, integral);` |
|      - |  863 | `			}` |
|     33 |  864 | `			return integral;` |
|     13 |  865 | `		case PH7_ROUND_CEILING:` |
|     27 |  866 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|     27 |  867 | `			if( value > 0.0 && value_abs > edge_case ){` |
|     15 |  868 | `				return integral + 1.0;` |
|      - |  869 | `			}` |
|     13 |  870 | `			return integral;` |
|     12 |  871 | `		case PH7_ROUND_FLOOR:` |
|     25 |  872 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|     25 |  873 | `			if( value < 0.0 && value_abs > edge_case ){` |
|     11 |  874 | `				return integral - 1.0;` |
|      - |  875 | `			}` |
|     15 |  876 | `			return integral;` |
|     12 |  877 | `		case PH7_ROUND_TOWARD_ZERO:` |
|     25 |  878 | `			return integral;` |
|     12 |  879 | `		case PH7_ROUND_AWAY_FROM_ZERO:` |
|     25 |  880 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|     25 |  881 | `			if( value_abs > edge_case ){` |
|     25 |  882 | `				return integral + copysign(1.0, integral);` |
|      - |  883 | `			}` |
|    ! 0 |  884 | `			return integral;` |
|     22 |  885 | `		case PH7_ROUND_HALF_EVEN:` |
|     45 |  886 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|     45 |  887 | `			if( value_abs > edge_case ){` |
|    ! 0 |  888 | `				return integral + copysign(1.0, integral);` |
|     45 |  889 | `			}else if( value_abs == edge_case ){` |
|     35 |  890 | `				if( fmod(integral, 2.0) != 0.0 ){ /* integral not even -> make it even */` |
|     19 |  891 | `					return integral + copysign(1.0, integral);` |
|      - |  892 | `				}` |
|      8 |  893 | `			}` |
|     27 |  894 | `			return integral;` |
|     16 |  895 | `		case PH7_ROUND_HALF_ODD:` |
|     33 |  896 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|     33 |  897 | `			if( value_abs > edge_case ){` |
|    ! 0 |  898 | `				return integral + copysign(1.0, integral);` |
|     33 |  899 | `			}else if( value_abs == edge_case ){` |
|     25 |  900 | `				if( fmod(integral, 2.0) == 0.0 ){ /* integral even -> make it odd */` |
|     15 |  901 | `					return integral + copysign(1.0, integral);` |
|      - |  902 | `				}` |
|      5 |  903 | `			}` |
|     19 |  904 | `			return integral;` |
|    ! 0 |  905 | `		default:` |
|    ! 0 |  906 | `			return integral; /* unreachable: mode validated by the caller */` |
|      - |  907 | `	}` |
|    211 |  908 | `}` |
|      - |  909 | `/*` |
|      - |  910 | `` * Round `value` to `places` decimals in `mode`. Faithful port of php-src`` |
|      - |  911 | ` * PHP-8.5 ext/standard/math.c _php_math_round() — the post-8.4` |
|      - |  912 | ` * integer-extraction algorithm with the +/-1 floating-point error` |
|      - |  913 | ` * correction step, required for byte-exact results on cases such as` |
|      - |  914 | ` * round(0.285, 2) == 0.29 that the old naive "+0.5" approach got wrong.` |
|      - |  915 | ` */` |
|    436 |  916 | `static double MathRound(double value, int places, int mode)` |
|      4 |  917 | `{` |
|      - |  918 | `	double exponent, tmp_value, tmp_value2;` |
|      - |  919 | `	int abs_places;` |
|    440 |  920 | `	if( !isfinite(value) \|\| value == 0.0 ){` |
|     17 |  921 | `		return value;` |
|      - |  922 | `	}` |
|      - |  923 | `	/* mirror php-src's clamp away from INT_MIN */` |
|    424 |  924 | `	if( places < -2147483647 ){` |
|    ! 0 |  925 | `		places = -2147483647;` |
|    ! 0 |  926 | `	}` |
|    424 |  927 | `	abs_places = places < 0 ? -places : places;` |
|    424 |  928 | `	exponent = MathIntPow10(abs_places);` |
|      - |  929 | `	/*` |
|      - |  930 | `	 * Extracting the integer part can be off by one ULP due to float error` |
|      - |  931 | `	 * (e.g. floor(0.285 * 1e10) == 2849999999). Try +/-1 and keep it if it` |
|      - |  932 | ``	 * divides back to exactly `value`.`` |
|      - |  933 | `	 */` |
|    424 |  934 | `	if( value >= 0.0 ){` |
|    326 |  935 | `		tmp_value = floor(places > 0 ? value * exponent : value / exponent);` |
|    326 |  936 | `		tmp_value2 = tmp_value + 1.0;` |
|    165 |  937 | `	}else{` |
|     99 |  938 | `		tmp_value = ceil(places > 0 ? value * exponent : value / exponent);` |
|     99 |  939 | `		tmp_value2 = tmp_value - 1.0;` |
|      - |  940 | `	}` |
|    424 |  941 | `	if( (places > 0 ? tmp_value2 / exponent : tmp_value2 * exponent) == value ){` |
|      7 |  942 | `		tmp_value = tmp_value2;` |
|      3 |  943 | `	}` |
|      - |  944 | `	/* Beyond our precision, so rounding it is pointless. */` |
|    424 |  945 | `	if( fabs(tmp_value) >= 1e16 ){` |
|      7 |  946 | `		return value;` |
|      - |  947 | `	}` |
|    418 |  948 | `	tmp_value = MathRoundHelper(tmp_value, value, exponent, places, mode);` |
|    418 |  949 | `	if( abs_places < 23 ){` |
|    418 |  950 | `		tmp_value = (places > 0) ? tmp_value / exponent : tmp_value * exponent;` |
|    211 |  951 | `	}else{` |
|      - |  952 | `		/*` |
|      - |  953 | `		 * Simple division would lose precision here; round-trip through a` |
|      - |  954 | `		 * string exactly like php-src does (snprintf "%15fe%d" + strtod).` |
|      - |  955 | `		 * libc snprintf is used (not SyBufferFormat, which is not` |
|      - |  956 | `		 * correctly-rounded) so the low bits match PHP. (SyStrToReal now` |
|      - |  957 | `		 * delegates to strtod too; the direct call here simply mirrors` |
|      - |  958 | `		 * php-src's own snprintf+strtod pairing.)` |
|      - |  959 | `		 */` |
|      - |  960 | `		char zBuf[64];` |
|    ! 0 |  961 | `		snprintf(zBuf, sizeof(zBuf), "%15fe%d", tmp_value, -places);` |
|    ! 0 |  962 | `		zBuf[sizeof(zBuf)-1] = '\0';` |
|    ! 0 |  963 | `		tmp_value = strtod(zBuf, 0);` |
|    ! 0 |  964 | `		if( !isfinite(tmp_value) \|\| isnan(tmp_value) ){` |
|    ! 0 |  965 | `			tmp_value = value;` |
|    ! 0 |  966 | `		}` |
|      - |  967 | `	}` |
|    418 |  968 | `	return tmp_value;` |
|    222 |  969 | `}` |
|      - |  970 | `/*` |
|      - |  971 | ` * float round ( int\|float $num [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|      - |  972 | ` *  Rounds a float.` |
|      - |  973 | ` * Parameters` |
|      - |  974 | ` *  $num       The value to round.` |
|      - |  975 | ` *  $precision The optional number of decimal digits to round to. May be` |
|      - |  976 | ` *             negative (rounds to the left of the decimal point).` |
|      - |  977 | ` *  $mode      One of PHP_ROUND_HALF_UP (default) / _HALF_DOWN / _HALF_EVEN /` |
|      - |  978 | ` *             _HALF_ODD, or the 8.5 integer modes CEILING / FLOOR /` |
|      - |  979 | ` *             TOWARD_ZERO / AWAY_FROM_ZERO (5..8).` |
|      - |  980 | ` * Return` |
|      - |  981 | ` *  The rounded value as a float.` |
|      - |  982 | ` */` |
|    364 |  983 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  984 | `{` |
|      - |  985 | `	double value, r;` |
|    365 |  986 | `	int places = 0;` |
|    365 |  987 | `	int mode = PH7_ROUND_HALF_UP;` |
|      - |  988 | `	/*` |
|      - |  989 | `	 * Legacy PHL contract: no argument -> int(0). PHP throws an` |
|      - |  990 | `	 * ArgumentCountError here, but two PHL-only (--SKIPIF-- zend_version)` |
|      - |  991 | `	 * tests assert round()===0, so keep the historical behavior.` |
|      - |  992 | `	 */` |
|    365 |  993 | `	if( nArg < 1 ){` |
|    ! 0 |  994 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  995 | `		return PH7_OK;` |
|      - |  996 | `	}` |
|    365 |  997 | `	if( nArg > 3 ){` |
|    ! 0 |  998 | `		return PH7_VmThrowException(pCtx,` |
|      - |  999 | `			"ArgumentCountError",` |
|      - | 1000 | `			"round() expects at most 3 arguments, %d given",` |
|    ! 0 | 1001 | `			nArg` |
|      - | 1002 | `			);` |
|      - | 1003 | `	}` |
|      - | 1004 | `	/* Argument #1's type is the aBuiltinSig[] row's -- see floor() above. */` |
|      - | 1005 | `	/* Precision (arg #2). Negative values are valid; clamp to int range. */` |
|    365 | 1006 | `	if( nArg > 1 ){` |
|    325 | 1007 | `		sxi64 prec = ph7_value_to_int64(apArg[1]);` |
|    325 | 1008 | `		if( prec > 2147483647 ){` |
|    ! 0 | 1009 | `			places = 2147483647;` |
|    325 | 1010 | `		}else if( prec < -2147483647 ){` |
|    ! 0 | 1011 | `			places = -2147483647;` |
|    ! 0 | 1012 | `		}else{` |
|    325 | 1013 | `			places = (int)prec;` |
|      - | 1014 | `		}` |
|    162 | 1015 | `	}` |
|      - | 1016 | `	/*` |
|      - | 1017 | ``	 * Mode (arg #3). php declares it `RoundingMode\|int`, so an enum CASE and the`` |
|      - | 1018 | `	 * raw integer both arrive here. The integer modes are 1..8; read the full` |
|      - | 1019 | `	 * 64-bit value before range-checking so a large out-of-range mode cannot` |
|      - | 1020 | `	 * alias a valid 1..8 via a truncating 32-bit cast (e.g. 0x1_0000_0003).` |
|      - | 1021 | `	 */` |
|    365 | 1022 | `	if( nArg > 2 ){` |
|      - | 1023 | ``		/* A RoundingMode case answers its integer mode straight into `mode`; the`` |
|      - | 1024 | `		 * integer spelling is range-checked here. */` |
|    253 | 1025 | `		if( !PH7_RoundingModeCase(apArg[2],&mode) ){` |
|    167 | 1026 | `			sxi64 m = ph7_value_to_int64(apArg[2]);` |
|    167 | 1027 | `			if( m < PH7_ROUND_HALF_UP \|\| m > PH7_ROUND_AWAY_FROM_ZERO ){` |
|     13 | 1028 | `				return PH7_VmThrowException(pCtx,` |
|      - | 1029 | `					"ValueError",` |
|      - | 1030 | `					"round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)"` |
|      - | 1031 | `					);` |
|      - | 1032 | `			}` |
|    155 | 1033 | `			mode = (int)m;` |
|     77 | 1034 | `		}` |
|    120 | 1035 | `	}` |
|    353 | 1036 | `	value = ph7_value_to_double(apArg[0]);` |
|      - | 1037 | `	/* Integer input with non-negative precision needs no rounding. */` |
|    353 | 1038 | `	if( ph7_value_is_int(apArg[0]) && places >= 0 ){` |
|     21 | 1039 | `		ph7_result_double(pCtx,value);` |
|     21 | 1040 | `		return PH7_OK;` |
|      - | 1041 | `	}` |
|    333 | 1042 | `	r = MathRound(value, places, mode);` |
|    333 | 1043 | `	ph7_result_double(pCtx,r);` |
|    333 | 1044 | `	return PH7_OK;` |
|    183 | 1045 | `}` |
|      - | 1046 | `/*` |
|      - | 1047 | ` * Assemble php's formatted number: the integer digits grouped from the right by` |
|      - | 1048 | ` * the thousands separator, then the decimal separator and exactly $decimals` |
|      - | 1049 | ` * fraction digits (right-padded with '0', since the printf may produce fewer).` |
|      - | 1050 | ` */` |
|    138 | 1051 | `static int NumberFormatEmit(ph7_context *pCtx,` |
|      - | 1052 | `	const char *zDigits,int nDigits,int bNeg,` |
|      - | 1053 | `	const char *zFrac,int nFrac,int nDec,` |
|      - | 1054 | `	const char *zPoint,int nPoint,const char *zSep,int nSep)` |
|      4 | 1055 | `{` |
|      - | 1056 | `	SyBlob sOut;` |
|      - | 1057 | `	int i;` |
|    142 | 1058 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    142 | 1059 | `	if( bNeg ){` |
|     12 | 1060 | `		SyBlobAppend(&sOut,"-",sizeof(char));` |
|      5 | 1061 | `	}` |
|    844 | 1062 | `	for( i = 0 ; i < nDigits ; ++i ){` |
|    706 | 1063 | `		if( i > 0 && nSep > 0 && ((nDigits - i) % 3) == 0 ){` |
|    176 | 1064 | `			SyBlobAppend(&sOut,zSep,(sxu32)nSep);` |
|     86 | 1065 | `		}` |
|    706 | 1066 | `		SyBlobAppend(&sOut,&zDigits[i],sizeof(char));` |
|    355 | 1067 | `	}` |
|    142 | 1068 | `	if( nDec > 0 ){` |
|     62 | 1069 | `		if( nPoint > 0 ){` |
|     58 | 1070 | `			SyBlobAppend(&sOut,zPoint,(sxu32)nPoint);` |
|     27 | 1071 | `		}` |
|     62 | 1072 | `		if( nFrac > nDec ){` |
|    ! 0 | 1073 | `			nFrac = nDec;` |
|    ! 0 | 1074 | `		}` |
|     62 | 1075 | `		if( nFrac > 0 ){` |
|     60 | 1076 | `			SyBlobAppend(&sOut,zFrac,(sxu32)nFrac);` |
|     28 | 1077 | `		}` |
|     66 | 1078 | `		for( i = nFrac ; i < nDec ; ++i ){` |
|      5 | 1079 | `			SyBlobAppend(&sOut,"0",sizeof(char));` |
|      3 | 1080 | `		}` |
|     29 | 1081 | `	}` |
|    142 | 1082 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    142 | 1083 | `	SyBlobRelease(&sOut);` |
|    142 | 1084 | `	return PH7_OK;` |
|      4 | 1085 | `}` |
|      - | 1086 | `/*` |
|      - | 1087 | ` * _php_math_number_format_long(): an INTEGER never goes through a double, so` |
|      - | 1088 | ` * every digit of a value past 2^53 survives. A NEGATIVE $decimals rounds the` |
|      - | 1089 | ` * integer itself, half away from zero.` |
|      - | 1090 | ` */` |
|     44 | 1091 | `static int NumberFormatLong(ph7_context *pCtx,sxi64 iVal,int nDec,` |
|      - | 1092 | `	const char *zPoint,int nPoint,const char *zSep,int nSep)` |
|      2 | 1093 | `{` |
|      - | 1094 | `	char zBuf[32];` |
|      - | 1095 | `	sxu64 uNum;` |
|     46 | 1096 | `	int bNeg = 0;` |
|     46 | 1097 | `	int n = 0;` |
|     46 | 1098 | `	if( iVal < 0 ){` |
|     11 | 1099 | `		bNeg = 1;` |
|      - | 1100 | `		/* -PHP_INT_MIN does not fit; negate through the unsigned domain. */` |
|     11 | 1101 | `		uNum = ((sxu64)-(iVal + 1)) + 1;` |
|      6 | 1102 | `	}else{` |
|     36 | 1103 | `		uNum = (sxu64)iVal;` |
|      - | 1104 | `	}` |
|     46 | 1105 | `	if( nDec < 0 ){` |
|      - | 1106 | `		/* php keeps a table of the 20 powers of ten a 64-bit value can hold and` |
|      - | 1107 | `		 * answers 0 past it; 10^19 is the last one that fits. */` |
|     21 | 1108 | `		if( nDec < -19 ){` |
|      3 | 1109 | `			uNum = 0;` |
|      2 | 1110 | `		}else{` |
|     19 | 1111 | `			sxu64 uPow = 1;` |
|      - | 1112 | `			sxu64 uRest;` |
|      - | 1113 | `			int k;` |
|     49 | 1114 | `			for( k = 0 ; k < -nDec ; ++k ){` |
|     31 | 1115 | `				uPow *= 10;` |
|     16 | 1116 | `			}` |
|     19 | 1117 | `			uRest = uNum % uPow;` |
|     19 | 1118 | `			uNum = uNum / uPow;` |
|     19 | 1119 | `			uNum = (uRest >= uPow / 2) ? uNum * uPow + uPow : uNum * uPow;` |
|      - | 1120 | `		}` |
|     21 | 1121 | `		if( uNum == 0 ){` |
|      - | 1122 | `			/* php never answers "-0". */` |
|      9 | 1123 | `			bNeg = 0;` |
|      4 | 1124 | `		}` |
|     10 | 1125 | `	}` |
|      - | 1126 | `	/* Decimal digits, most significant first. */` |
|     46 | 1127 | `	if( uNum == 0 ){` |
|     14 | 1128 | `		zBuf[n++] = '0';` |
|      8 | 1129 | `	}else{` |
|      - | 1130 | `		char zRev[32];` |
|     34 | 1131 | `		int nRev = 0;` |
|    372 | 1132 | `		while( uNum > 0 && nRev < (int)sizeof(zRev) ){` |
|    340 | 1133 | `			zRev[nRev++] = (char)('0' + (int)(uNum % 10));` |
|    340 | 1134 | `			uNum /= 10;` |
|      2 | 1135 | `		}` |
|    372 | 1136 | `		while( nRev > 0 ){` |
|    340 | 1137 | `			zBuf[n++] = zRev[--nRev];` |
|      2 | 1138 | `		}` |
|      - | 1139 | `	}` |
|     46 | 1140 | `	return NumberFormatEmit(pCtx,zBuf,n,bNeg,0,0,nDec > 0 ? nDec : 0,` |
|     22 | 1141 | `		zPoint,nPoint,zSep,nSep);` |
|      2 | 1142 | `}` |
|      - | 1143 | `/*` |
|      - | 1144 | ` * string number_format(int\|float $num,int $decimals = 0,` |
|      - | 1145 | ` *                      ?string $decimal_separator = ".",` |
|      - | 1146 | ` *                      ?string $thousands_separator = ",")` |
|      - | 1147 | ` *  Format a number with grouped thousands.` |
|      - | 1148 | ` */` |
|    206 | 1149 | `PH7_PRIVATE int PH7_builtin_number_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1150 | `{` |
|    210 | 1151 | `	const char *zPoint = ".", *zSep = ",";` |
|    210 | 1152 | `	int nPoint = 1, nSep = 1;` |
|    210 | 1153 | `	int nDec = 0;` |
|      - | 1154 | `	ph7_value sNum;` |
|      - | 1155 | `	double d;` |
|    210 | 1156 | `	int bNeg = 0;` |
|      - | 1157 | `	int nLen,nInt;` |
|      - | 1158 | `	char *zFmt;` |
|      - | 1159 | `	const char *zDot;` |
|    210 | 1160 | `	if( nArg < 1 ){` |
|      - | 1161 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|    ! 0 | 1162 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1163 | `		return PH7_OK;` |
|      - | 1164 | `	}` |
|      - | 1165 | `	/* Every refusal is worded here rather than by the shared screen: php's stub` |
|      - | 1166 | ``	 * declares `float $num` (which is what Reflection prints) but the ZPP macro`` |
|      - | 1167 | ``	 * behind it is Z_PARAM_NUMBER, whose TypeError says `int\|float`. An int stays`` |
|      - | 1168 | `	 * an INT, a numeric string takes the shape it looks like, and null is §10's` |
|      - | 1169 | `	 * refusal of a deprecation. */` |
|    210 | 1170 | `	if( !PH7_MemObjIsNumeric(apArg[0]) ){` |
|      - | 1171 | `		char zBuf[64];` |
|     60 | 1172 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1173 | `			"number_format(): Argument #1 ($num) must be of type int\|float, %s given",` |
|     32 | 1174 | `			ph7_value_is_string(apArg[0]) ? "string"` |
|     18 | 1175 | `				: VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|      - | 1176 | `	}` |
|    178 | 1177 | `	if( nArg > 1 ){` |
|      - | 1178 | ``		/* php declares `int $decimals`; the string and float narrowings it only`` |
|      - | 1179 | `		 * DEPRECATES are rejected here (§10), as they are for count_chars(). */` |
|    126 | 1180 | `		if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|    127 | 1181 | `		 \|\| ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|      - | 1182 | `			char zBuf[64];` |
|     14 | 1183 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1184 | `				"number_format(): Argument #2 ($decimals) must be of type int, %s given",` |
|      8 | 1185 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 1186 | `		}` |
|    122 | 1187 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 1188 | `			/* php wants the WHOLE string to be numeric ("2abc" is a TypeError,` |
|      - | 1189 | `			 * not 2), and a float-shaped one that would LOSE something is §10's` |
|      - | 1190 | `			 * refusal of a deprecation. */` |
|      - | 1191 | `			double dMode;` |
|      8 | 1192 | `			if( !PH7_MemObjStringIsNumeric(apArg[1]) ){` |
|      6 | 1193 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1194 | `					"number_format(): Argument #2 ($decimals) must be of type int, string given");` |
|      - | 1195 | `			}` |
|      3 | 1196 | `			dMode = ph7_value_to_double(apArg[1]);` |
|      - | 1197 | ``			/* Range first: `(sxi64)dMode` is undefined outside it (§2). */`` |
|      3 | 1198 | `			if( !PH7_RealFitsInt64(dMode) \|\| dMode != (double)(sxi64)dMode ){` |
|    ! 0 | 1199 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1200 | `					"number_format(): Argument #2 ($decimals) must be of type int, string given");` |
|      1 | 1201 | `			}` |
|    117 | 1202 | `		}else if( ph7_value_is_float(apArg[1]) ){` |
|      6 | 1203 | `			double dMode = ph7_value_to_double(apArg[1]);` |
|      6 | 1204 | `			if( !PH7_RealFitsInt64(dMode) \|\| dMode != (double)(sxi64)dMode ){` |
|      3 | 1205 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1206 | `					"number_format(): Argument #2 ($decimals) must be of type int, float given");` |
|      - | 1207 | `			}` |
|      1 | 1208 | `		}` |
|      - | 1209 | `		{` |
|    116 | 1210 | `			sxi64 iDec = ph7_value_to_int64(apArg[1]);` |
|      - | 1211 | `			/* php clamps the declared long onto an int before it formats. */` |
|    172 | 1212 | `			nDec = iDec > 2147483647 ? 2147483647` |
|    112 | 1213 | `			     : (iDec < -2147483647 ? -2147483647 : (int)iDec);` |
|      - | 1214 | `		}` |
|     56 | 1215 | `	}` |
|      - | 1216 | ``	/* Both separators are `?string`: null means php's default, not the empty`` |
|      - | 1217 | `	 * string. An empty string IS accepted and simply omits the separator, and an` |
|      - | 1218 | `	 * object that can stringify is coerced. */` |
|    164 | 1219 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     37 | 1220 | `		if( !PH7_ArgSatisfiesString(apArg[2]) ){` |
|      - | 1221 | `			char zBuf[64];` |
|     14 | 1222 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1223 | `				"number_format(): Argument #3 ($decimal_separator) must be of type ?string, %s given",` |
|      8 | 1224 | `				VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 1225 | `		}` |
|     29 | 1226 | `		zPoint = ph7_value_to_string(apArg[2],&nPoint);` |
|     13 | 1227 | `	}` |
|    156 | 1228 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     29 | 1229 | `		if( !PH7_ArgSatisfiesString(apArg[3]) ){` |
|      - | 1230 | `			char zBuf[64];` |
|      7 | 1231 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1232 | `				"number_format(): Argument #4 ($thousands_separator) must be of type ?string, %s given",` |
|      4 | 1233 | `				VmValueGivenName(apArg[3],zBuf,sizeof(zBuf)));` |
|      - | 1234 | `		}` |
|     25 | 1235 | `		zSep = ph7_value_to_string(apArg[3],&nSep);` |
|     11 | 1236 | `	}` |
|    152 | 1237 | `	PH7_MemObjInit(pCtx->pVm,&sNum);` |
|    152 | 1238 | `	PH7_MemObjStore(apArg[0],&sNum);` |
|    152 | 1239 | `	PH7_MemObjToNumeric(&sNum);` |
|    152 | 1240 | `	if( (sNum.iFlags & MEMOBJ_REAL) == 0 ){` |
|     44 | 1241 | `		int rc = NumberFormatLong(pCtx,sNum.x.iVal,nDec,zPoint,nPoint,zSep,nSep);` |
|     44 | 1242 | `		PH7_MemObjRelease(&sNum);` |
|     44 | 1243 | `		return rc;` |
|      - | 1244 | `	}` |
|    110 | 1245 | `	d = (double)sNum.rVal;` |
|    110 | 1246 | `	PH7_MemObjRelease(&sNum);` |
|      - | 1247 | `	/* A double past 2^52 has no fractional digits left, so php formats it as an` |
|      - | 1248 | `	 * INTEGER when it fits one — that is what keeps 4503599627370496.0 exact. */` |
|    106 | 1249 | `	if( (d >= 4503599627370496.0 \|\| d <= -4503599627370496.0)` |
|     62 | 1250 | `	 && PH7_RealFitsInt64(d) ){` |
|      3 | 1251 | `		return NumberFormatLong(pCtx,(sxi64)d,nDec,zPoint,nPoint,zSep,nSep);` |
|      - | 1252 | `	}` |
|    108 | 1253 | `	if( d < 0 ){` |
|     14 | 1254 | `		bNeg = 1;` |
|     14 | 1255 | `		d = -d;` |
|      6 | 1256 | `	}` |
|    108 | 1257 | `	d = MathRound(d,nDec,PH7_ROUND_HALF_UP);` |
|    108 | 1258 | `	if( nDec < 0 ){` |
|     16 | 1259 | `		nDec = 0;` |
|      7 | 1260 | `	}` |
|      - | 1261 | `	/* libc's %f, not the engine's formatter: php prints through its own` |
|      - | 1262 | `	 * snprintf here, so INF answers "inf" and NAN "nan" — and the engine's` |
|      - | 1263 | `	 * formatter caps the precision at 53 digits with a notice, where php` |
|      - | 1264 | `	 * honours whatever $decimals asks for. */` |
|    108 | 1265 | `	nLen = snprintf(0,0,"%.*f",nDec,d);` |
|    108 | 1266 | `	if( nLen < 0 ){` |
|    ! 0 | 1267 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1268 | `	}` |
|    108 | 1269 | `	zFmt = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen + 1,FALSE,TRUE);` |
|    108 | 1270 | `	if( zFmt == 0 ){` |
|    ! 0 | 1271 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1272 | `	}` |
|    108 | 1273 | `	snprintf(zFmt,(size_t)nLen + 1,"%.*f",nDec,d);` |
|    108 | 1274 | `	if( zFmt[0] < '0' \|\| zFmt[0] > '9' ){` |
|      - | 1275 | `		/* Not a number at all (inf/nan): php hands its buffer straight back,` |
|      - | 1276 | `		 * without a sign, a separator or any padding. */` |
|     11 | 1277 | `		ph7_result_string(pCtx,zFmt,nLen);` |
|     11 | 1278 | `		return PH7_OK;` |
|      - | 1279 | `	}` |
|     98 | 1280 | `	if( bNeg && d == 0 ){` |
|      - | 1281 | `		/* Rounded away to zero; php never answers "-0". */` |
|      5 | 1282 | `		bNeg = 0;` |
|      2 | 1283 | `	}` |
|      - | 1284 | `	/* php looks for '.' OR ',' — the decimal point its formatter produced. */` |
|     98 | 1285 | `	zDot = 0;` |
|     98 | 1286 | `	if( nDec > 0 ){` |
|      - | 1287 | `		int i;` |
|    284 | 1288 | `		for( i = 0 ; i < nLen ; ++i ){` |
|    284 | 1289 | `			if( zFmt[i] == '.' \|\| zFmt[i] == ',' ){` |
|     60 | 1290 | `				zDot = &zFmt[i];` |
|     60 | 1291 | `				break;` |
|      - | 1292 | `			}` |
|    116 | 1293 | `		}` |
|     28 | 1294 | `	}` |
|     98 | 1295 | `	nInt = zDot ? (int)(zDot - zFmt) : nLen;` |
|    173 | 1296 | `	return NumberFormatEmit(pCtx,zFmt,nInt,bNeg,` |
|     75 | 1297 | `		zDot ? zDot + 1 : 0,zDot ? nLen - nInt - 1 : 0,` |
|     47 | 1298 | `		nDec,zPoint,nPoint,zSep,nSep);` |
|    107 | 1299 | `}` |
|      - | 1300 | `/*` |
|      - | 1301 | ` * int intdiv(int $a, int $b)` |
|      - | 1302 | ` *  Integer division.` |
|      - | 1303 | ` * Parameters` |
|      - | 1304 | ` *  $a` |
|      - | 1305 | ` *   Number to be divided.` |
|      - | 1306 | ` *  $b` |
|      - | 1307 | ` *   Number which divides the $a.` |
|      - | 1308 | ` * Return` |
|      - | 1309 | ` *  The integer quotient of the division of $a by $b.` |
|      - | 1310 | ` */` |
|     30 | 1311 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1312 | `{` |
|      - | 1313 | `	char zGiven[64];` |
|      - | 1314 | `	sxi64 a,b;` |
|      - | 1315 | `	/* PHP requires exactly two arguments. */` |
|     33 | 1316 | `	if( nArg != 2 ){` |
|    ! 0 | 1317 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1318 | `			"ArgumentCountError",` |
|      - | 1319 | `			"intdiv() expects exactly 2 arguments, %d given",` |
|    ! 0 | 1320 | `			nArg` |
|      - | 1321 | `			);` |
|      - | 1322 | `	}` |
|      - | 1323 | `	/* Type-check argument 1 */` |
|     30 | 1324 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|     33 | 1325 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1326 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1327 | `			"TypeError",` |
|      - | 1328 | `			"intdiv(): Argument #1 ($num1) must be of type int, %s given",` |
|    ! 0 | 1329 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|      - | 1330 | `			);` |
|      - | 1331 | `	}` |
|     33 | 1332 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1333 | `		int len;` |
|      3 | 1334 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|      3 | 1335 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|    ! 0 | 1336 | `			return PH7_VmThrowException(pCtx,` |
|      - | 1337 | `				"TypeError",` |
|      - | 1338 | `				"intdiv(): Argument #1 ($num1) must be of type int, string given"` |
|      - | 1339 | `				);` |
|      - | 1340 | `		}` |
|      1 | 1341 | `	}` |
|      - | 1342 | `	/* Type-check argument 2 */` |
|     30 | 1343 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|     33 | 1344 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|    ! 0 | 1345 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1346 | `			"TypeError",` |
|      - | 1347 | `			"intdiv(): Argument #2 ($num2) must be of type int, %s given",` |
|    ! 0 | 1348 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|      - | 1349 | `			);` |
|      - | 1350 | `	}` |
|     33 | 1351 | `	if( ph7_value_is_string(apArg[1]) ){` |
|      - | 1352 | `		int len;` |
|    ! 0 | 1353 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|    ! 0 | 1354 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|    ! 0 | 1355 | `			return PH7_VmThrowException(pCtx,` |
|      - | 1356 | `				"TypeError",` |
|      - | 1357 | `				"intdiv(): Argument #2 ($num2) must be of type int, string given"` |
|      - | 1358 | `				);` |
|      - | 1359 | `		}` |
|    ! 0 | 1360 | `	}` |
|      - | 1361 | `	/* Convert both arguments to int64 */` |
|      - | 1362 | `	{` |
|      - | 1363 | `		/* php's ZPP contract for the two int params (lossy float / float-string` |
|      - | 1364 | `		 * deprecations); the manual type checks above already covered arrays,` |
|      - | 1365 | `		 * objects and non-numeric strings with the same messages. */` |
|     33 | 1366 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[0],"intdiv",1,"$num1","int",&a);` |
|     33 | 1367 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 1368 | `			return rcArg;` |
|      - | 1369 | `		}` |
|     33 | 1370 | `		rcArg = PH7_IntArgResolve(pCtx,apArg[1],"intdiv",2,"$num2","int",&b);` |
|     33 | 1371 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 1372 | `			return rcArg;` |
|      - | 1373 | `		}` |
|      - | 1374 | `	}` |
|      - | 1375 | `	/* Check for division by zero */` |
|     33 | 1376 | `	if( b == 0 ){` |
|      6 | 1377 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1378 | `			"DivisionByZeroError",` |
|      - | 1379 | `			"Division by zero"` |
|      - | 1380 | `			);` |
|      - | 1381 | `	}` |
|      - | 1382 | `	/* Check for overflow: PHP_INT_MIN / -1 */` |
|     29 | 1383 | `	if( a == SMALLEST_INT64 && b == -1 ){` |
|      3 | 1384 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1385 | `			"ArithmeticError",` |
|      - | 1386 | `			"Division of PHP_INT_MIN by -1 is not an integer"` |
|      - | 1387 | `			);` |
|      - | 1388 | `	}` |
|      - | 1389 | `	/* Perform integer division */` |
|     27 | 1390 | `	ph7_result_int64(pCtx, a / b);` |
|     27 | 1391 | `	return PH7_OK;` |
|     18 | 1392 | `}` |
|      - | 1393 | `/*` |
|      - | 1394 | ` * string dechex(int $number)` |
|      - | 1395 | ` *  Decimal to hexadecimal.` |
|      - | 1396 | ` * Parameters` |
|      - | 1397 | ` *  $number` |
|      - | 1398 | ` *   Decimal value to convert` |
|      - | 1399 | ` * Return` |
|      - | 1400 | ` *  Hexadecimal string representation of number` |
|      - | 1401 | ` */` |
|     24 | 1402 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1403 | `{` |
|      - | 1404 | `	ph7_int64 iVal;` |
|     26 | 1405 | `	if( nArg < 1 ){` |
|      - | 1406 | `		/* Missing arguments,return null */` |
|    ! 0 | 1407 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1408 | `		return PH7_OK;` |
|      - | 1409 | `	}` |
|      - | 1410 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|     26 | 1411 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1412 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement,` |
|      - | 1413 | `	 * so a negative value prints all 16 hex digits like PHP. */` |
|     26 | 1414 | `	ph7_result_string_format(pCtx,"%qx",iVal);` |
|     26 | 1415 | `	return PH7_OK;` |
|     14 | 1416 | `}` |
|      - | 1417 | `/*` |
|      - | 1418 | ` * string decoct(int $number)` |
|      - | 1419 | ` *  Decimal to Octal.` |
|      - | 1420 | ` * Parameters` |
|      - | 1421 | ` *  $number` |
|      - | 1422 | ` *   Decimal value to convert` |
|      - | 1423 | ` * Return` |
|      - | 1424 | ` *  Octal string representation of number` |
|      - | 1425 | ` */` |
|     16 | 1426 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1427 | `{` |
|      - | 1428 | `	ph7_int64 iVal;` |
|     17 | 1429 | `	if( nArg < 1 ){` |
|      - | 1430 | `		/* Missing arguments,return null */` |
|    ! 0 | 1431 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1432 | `		return PH7_OK;` |
|      - | 1433 | `	}` |
|      - | 1434 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|     17 | 1435 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1436 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|     17 | 1437 | `	ph7_result_string_format(pCtx,"%qo",iVal);` |
|     17 | 1438 | `	return PH7_OK;` |
|      9 | 1439 | `}` |
|      - | 1440 | `/*` |
|      - | 1441 | ` * string decbin(int $number)` |
|      - | 1442 | ` *  Decimal to binary.` |
|      - | 1443 | ` * Parameters` |
|      - | 1444 | ` *  $number` |
|      - | 1445 | ` *   Decimal value to convert` |
|      - | 1446 | ` * Return` |
|      - | 1447 | ` *  Binary string representation of number` |
|      - | 1448 | ` */` |
|     30 | 1449 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1450 | `{` |
|      - | 1451 | `	ph7_int64 iVal;` |
|     31 | 1452 | `	if( nArg < 1 ){` |
|      - | 1453 | `		/* Missing arguments,return null */` |
|    ! 0 | 1454 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1455 | `		return PH7_OK;` |
|      - | 1456 | `	}` |
|      - | 1457 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|     31 | 1458 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1459 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|     31 | 1460 | `	ph7_result_string_format(pCtx,"%qB",iVal);` |
|     31 | 1461 | `	return PH7_OK;` |
|     16 | 1462 | `}` |
|      - | 1463 | `/*` |
|      - | 1464 | ` * The three bytes php's _php_math_basetozval accepts BEFORE it starts reading` |
|      - | 1465 | ` * digits, and that no program is deprecated for writing:` |
|      - | 1466 | ` *` |
|      - | 1467 | ` *   - leading and trailing WHITESPACE, trimmed off both ends (' ', '\t', '\n',` |
|      - | 1468 | ` *     '\r', '\v', '\f' — a NUL is not whitespace and stays an invalid digit);` |
|      - | 1469 | `` *   - the base's own PREFIX, when the base is one that has one: `0x`/`0X` for 16,`` |
|      - | 1470 | `` *     `0o`/`0O` for 8, `0b`/`0B` for 2. It is stripped only for the MATCHING base,`` |
|      - | 1471 | `` *     which is why hexdec("0o17") still deprecates its `o` and base_convert with`` |
|      - | 1472 | `` *     from_base 36 reads the `x` of "0x1f" as the digit 33.`` |
|      - | 1473 | ` *` |
|      - | 1474 | `` * Both were treated as invalid characters here, so `hexdec("0xff")` — the literal`` |
|      - | 1475 | ` * a program hands back to the engine after reading it out of source, and exactly` |
|      - | 1476 | ` * what nikic/php-parser passes — raised the ValueError §10 keeps for php's` |
|      - | 1477 | ` * DEPRECATED skipping instead of answering 255.` |
|      - | 1478 | ` *` |
|      - | 1479 | ` * On return the two out-parameters name the digit run; the caller decides what a` |
|      - | 1480 | ` * leftover non-digit means.` |
|      - | 1481 | ` */` |
|  34680 | 1482 | `static void MathBaseTrimPrefix(const char **pz,int *pn,int base)` |
|      4 | 1483 | `{` |
|  34684 | 1484 | `	const char *z = *pz;` |
|  34684 | 1485 | `	const char *zEnd = z + *pn;` |
|  52217 | 1486 | `	while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]=='\n'\|\|z[0]=='\r'\|\|z[0]=='\v'\|\|z[0]=='\f') ){` |
|     17 | 1487 | `		z++;` |
|      1 | 1488 | `	}` |
|  52217 | 1489 | `	while( z < zEnd && (zEnd[-1]==' '\|\|zEnd[-1]=='\t'\|\|zEnd[-1]=='\n'` |
|  34675 | 1490 | `	                 \|\| zEnd[-1]=='\r'\|\|zEnd[-1]=='\v'\|\|zEnd[-1]=='\f') ){` |
|     17 | 1491 | `		zEnd--;` |
|      1 | 1492 | `	}` |
|  34684 | 1493 | `	if( zEnd - z >= 2 && z[0] == '0' ){` |
|   3441 | 1494 | `		int c = z[1];` |
|   3438 | 1495 | `		if( (base == 16 && (c=='x'\|\|c=='X'))` |
|   3427 | 1496 | `		 \|\| (base == 8  && (c=='o'\|\|c=='O'))` |
|   3363 | 1497 | `		 \|\| (base == 2  && (c=='b'\|\|c=='B')) ){` |
|    149 | 1498 | `			z += 2;` |
|    134 | 1499 | `		}` |
|   1659 | 1500 | `	}` |
|  34564 | 1501 | `	*pz = z;` |
|  34564 | 1502 | `	*pn = (int)(zEnd - z);` |
|  34564 | 1503 | `}` |
|      - | 1504 | `/*` |
|      - | 1505 | ` * Convert a base-2/8/16 digit string to a number, mirroring PHP's` |
|      - | 1506 | ` * _php_math_basetozval (ext/standard/math.c) so hexdec/octdec/bindec agree with` |
|      - | 1507 | ` * php byte-for-byte: trim the ends and the base prefix (above), then walk every` |
|      - | 1508 | ` * byte, decode a digit (0-9,a-z,A-Z) or skip any invalid one, accumulate into a` |
|      - | 1509 | ` * signed 64-bit integer and transparently promote to a double once the value` |
|      - | 1510 | ` * would overflow PHP_INT_MAX. The context result is set to an int when it fits,` |
|      - | 1511 | ` * otherwise a float — PHP returns a float for values above PHP_INT_MAX (e.g.` |
|      - | 1512 | ` * hexdec("ffffffffffffffff") == 1.8446744073709552E+19).` |
|      - | 1513 | ` * A byte >= 0x80 (e.g. a UTF-8 continuation) matches none of the digit ranges and` |
|      - | 1514 | ` * is skipped, so leading/interior multibyte junk is ignored like php.` |
|      - | 1515 | ` * Note: php also raises E_DEPRECATED for skipped invalid characters; that notice` |
|      - | 1516 | ` * is not emitted here (a §3.7 deprecation-fidelity residual, value is correct).` |
|      - | 1517 | ` */` |
|  34618 | 1518 | `static void MathBaseToNumber(ph7_context *pCtx,const char *zStr,int nLen,int base)` |
|      4 | 1519 | `{` |
|  34622 | 1520 | `	sxi64 num = 0;      /* Integer accumulator */` |
|  34622 | 1521 | `	double fnum = 0;    /* Float accumulator (used once num would overflow) */` |
|  34622 | 1522 | `	int mode = 0;       /* 0 -> integer accumulation, 1 -> switched to float */` |
|  34622 | 1523 | `	sxi64 cutoff = SXI64_HIGH / base;      /* PHP_INT_MAX / base */` |
|  34622 | 1524 | `	int cutlim = (int)(SXI64_HIGH % base); /* PHP_INT_MAX % base */` |
|  34622 | 1525 | `	int bIgnored = 0;   /* any character skipped below? php deprecates that */` |
|      - | 1526 | `	int i;` |
|  34622 | 1527 | `	MathBaseTrimPrefix(&zStr,&nLen,base);` |
| 105094 | 1528 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  70476 | 1529 | `		int c = (unsigned char)zStr[i];` |
|  70476 | 1530 | `		if( c >= '0' && c <= '9' ){` |
|  54458 | 1531 | `			c -= '0';` |
|  43546 | 1532 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|      5 | 1533 | `			c -= 'A' - 10;` |
|  16019 | 1534 | `		}else if( c >= 'a' && c <= 'z' ){` |
|  16017 | 1535 | `			c -= 'a' - 10;` |
|   8067 | 1536 | `		}else{` |
|    ! 0 | 1537 | `			bIgnored = 1;` |
|    ! 0 | 1538 | `			continue; /* Not a digit character: skip */` |
|      - | 1539 | `		}` |
|  70476 | 1540 | `		if( c >= base ){` |
|     14 | 1541 | `			bIgnored = 1;` |
|     14 | 1542 | `			continue; /* Digit out of range for this base: skip */` |
|      - | 1543 | `		}` |
|  70463 | 1544 | `		if( mode == 0 ){` |
|  70463 | 1545 | `			if( num < cutoff \|\| (num == cutoff && c <= cutlim) ){` |
|  70455 | 1546 | `				num = num * base + c;` |
|  70455 | 1547 | `				continue;` |
|      - | 1548 | `			}` |
|      - | 1549 | `			/* Adding this digit would overflow the 64-bit integer: fall back to` |
|      - | 1550 | `			 * float accumulation, seeding it with the value gathered so far. */` |
|      9 | 1551 | `			fnum = (double)num;` |
|      9 | 1552 | `			mode = 1;` |
|      4 | 1553 | `		}` |
|      9 | 1554 | `		fnum = fnum * base + c;` |
|      5 | 1555 | `	}` |
|  34622 | 1556 | `	if( bIgnored ){` |
|      - | 1557 | `		/* php 8 skips characters that are not valid digits for this base and only` |
|      - | 1558 | `		 * DEPRECATES the skipping; §10 rejects the deprecated surface loudly, so this` |
|      - | 1559 | `		 * ValueError ABORTS the call (the result stored below never reaches the caller` |
|      - | 1560 | `		 * — the OP_CALL boundary reports the throw for us, VmHostFuncThrowRc).` |
|      - | 1561 | `		 * Twin-pinned by base_invalid_chars_abort{,_zend}.phpt. */` |
|     10 | 1562 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1563 | `			"Invalid characters passed for attempted conversion");` |
|     10 | 1564 | `		return;` |
|      - | 1565 | `	}` |
|  34613 | 1566 | `	if( mode == 1 ){` |
|      9 | 1567 | `		ph7_result_double(pCtx,fnum);` |
|      5 | 1568 | `	}else{` |
|  34605 | 1569 | `		ph7_result_int64(pCtx,num);` |
|      - | 1570 | `	}` |
|  17490 | 1571 | `}` |
|      - | 1572 | `/*` |
|      - | 1573 | ` * int64 hexdec(string $hex_string)` |
|      - | 1574 | ` *  Hexadecimal to decimal.` |
|      - | 1575 | ` * Parameters` |
|      - | 1576 | ` *  $hex_string` |
|      - | 1577 | ` *   The hexadecimal string to convert` |
|      - | 1578 | ` * Return` |
|      - | 1579 | ` *  The decimal representation of hex_string (int, or float on overflow)` |
|      - | 1580 | ` */` |
|  34430 | 1581 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1582 | `{` |
|      - | 1583 | `	const char *zString;` |
|      - | 1584 | `	int nLen;` |
|  34434 | 1585 | `	if( nArg < 1 ){` |
|      - | 1586 | `		/* Missing arguments,return -1 */` |
|    ! 0 | 1587 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1588 | `		return PH7_OK;` |
|      - | 1589 | `	}` |
|  34434 | 1590 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|      - | 1591 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|      - | 1592 | `		char zBuf[64];` |
|    ! 0 | 1593 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1594 | `			"hexdec(): Argument #1 ($hex_string) must be of type string, %s given",` |
|    ! 0 | 1595 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|      - | 1596 | `	}` |
|      - | 1597 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|      - | 1598 | `	 * hex-parses that (hexdec(255) == hexdec("255") == 0x255), so route every` |
|      - | 1599 | `	 * non-throwing value through ph7_value_to_string rather than reading it as` |
|      - | 1600 | `	 * a decimal integer. */` |
|  34434 | 1601 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  34434 | 1602 | `	MathBaseToNumber(pCtx,zString,nLen,16);` |
|  34434 | 1603 | `	return PH7_OK;` |
|  17396 | 1604 | `}` |
|      - | 1605 | `/*` |
|      - | 1606 | ` * int64 bindec(string $bin_string)` |
|      - | 1607 | ` *  Binary to decimal.` |
|      - | 1608 | ` * Parameters` |
|      - | 1609 | ` *  $bin_string` |
|      - | 1610 | ` *   The binary string to convert` |
|      - | 1611 | ` * Return` |
|      - | 1612 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|      - | 1613 | ` */` |
|    160 | 1614 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1615 | `{` |
|      - | 1616 | `	const char *zString;` |
|      - | 1617 | `	int nLen;` |
|    161 | 1618 | `	if( nArg < 1 ){` |
|      - | 1619 | `		/* Missing arguments,return -1 */` |
|    ! 0 | 1620 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1621 | `		return PH7_OK;` |
|      - | 1622 | `	}` |
|    161 | 1623 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|      - | 1624 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|      - | 1625 | `		char zBuf[64];` |
|    ! 0 | 1626 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1627 | `			"bindec(): Argument #1 ($binary_string) must be of type string, %s given",` |
|    ! 0 | 1628 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|      - | 1629 | `	}` |
|      - | 1630 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|      - | 1631 | `	 * binary-parses that (bindec(11) == bindec("11") == 3). */` |
|    161 | 1632 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    161 | 1633 | `	MathBaseToNumber(pCtx,zString,nLen,2);` |
|    161 | 1634 | `	return PH7_OK;` |
|     81 | 1635 | `}` |
|      - | 1636 | `/*` |
|      - | 1637 | ` * int64 octdec(string $oct_string)` |
|      - | 1638 | ` *  Octal to decimal.` |
|      - | 1639 | ` * Parameters` |
|      - | 1640 | ` *  $oct_string` |
|      - | 1641 | ` *   The octal string to convert` |
|      - | 1642 | ` * Return` |
|      - | 1643 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|      - | 1644 | ` */` |
|     28 | 1645 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1646 | `{` |
|      - | 1647 | `	const char *zString;` |
|      - | 1648 | `	int nLen;` |
|     29 | 1649 | `	if( nArg < 1 ){` |
|      - | 1650 | `		/* Missing arguments,return -1 */` |
|    ! 0 | 1651 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1652 | `		return PH7_OK;` |
|      - | 1653 | `	}` |
|     29 | 1654 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|      - | 1655 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|      - | 1656 | `		char zBuf[64];` |
|    ! 0 | 1657 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1658 | `			"octdec(): Argument #1 ($octal_string) must be of type string, %s given",` |
|    ! 0 | 1659 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|      - | 1660 | `	}` |
|      - | 1661 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|      - | 1662 | `	 * octal-parses that (octdec(11) == octdec("11") == 9). */` |
|     29 | 1663 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 1664 | `	MathBaseToNumber(pCtx,zString,nLen,8);` |
|     29 | 1665 | `	return PH7_OK;` |
|     15 | 1666 | `}` |
|      - | 1667 | `/*` |
|      - | 1668 | ` * srand([int $seed])` |
|      - | 1669 | ` * mt_srand([int $seed])` |
|      - | 1670 | ` *  Seed the random number generator.` |
|      - | 1671 | ` * Parameters` |
|      - | 1672 | ` * $seed` |
|      - | 1673 | ` *  Optional seed value. php truncates it to 32 bits; a missing seed reseeds` |
|      - | 1674 | ` *  from OS entropy (a "random" seed), matching php's GENERATE_SEED().` |
|      - | 1675 | ` * Return` |
|      - | 1676 | ` *  null.` |
|      - | 1677 | ` * Note:` |
|      - | 1678 | ` *  srand()/mt_srand() are aliases (php 7.1+ backs both rand() and mt_rand()` |
|      - | 1679 | ` *  with the same MT19937). They reset only the userland generator, never the` |
|      - | 1680 | ` *  engine's internal RC4 entropy, so a seed makes rand()/mt_rand()/shuffle/` |
|      - | 1681 | ` *  str_shuffle/array_rand reproducible without disturbing object ids or uniqid.` |
|      - | 1682 | ` */` |
|    314 | 1683 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1684 | `{` |
|      - | 1685 | `	sxu32 nSeed;` |
|    317 | 1686 | `	int bLegacy = nArg > 1 && ph7_value_to_int64(apArg[1]) == PH7_MT_RAND_PHP;` |
|    317 | 1687 | `	if( bLegacy ){` |
|      - | 1688 | `		/* php 8.3 deprecated the legacy generator; the message carries no` |
|      - | 1689 | `		 * function prefix there. */` |
|     23 | 1690 | `		PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|      - | 1691 | `			"The MT_RAND_PHP variant of Mt19937 is deprecated");` |
|     11 | 1692 | `	}` |
|    317 | 1693 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|      - | 1694 | `		/* php truncates the (weakly int-coerced) seed to 32 bits. */` |
|    311 | 1695 | `		nSeed = (sxu32)ph7_value_to_int64(apArg[0]);` |
|    157 | 1696 | `	}else{` |
|      - | 1697 | `		/* NULL is the declared default and means "no seed given": php reseeds` |
|      - | 1698 | `		 * from entropy for it, where this read it as the integer 0 — so` |
|      - | 1699 | ``		 * `mt_srand($cfg['seed'] ?? null)` pinned every run to one sequence. */`` |
|      - | 1700 | `		/* No seed: reseed from OS entropy, like php's GENERATE_SEED(). */` |
|      8 | 1701 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|    ! 0 | 1702 | `			nSeed = PH7_VmRandomNum(pCtx->pVm);` |
|    ! 0 | 1703 | `		}` |
|      - | 1704 | `	}` |
|      - | 1705 | `	/* $mode picks the GENERATOR, and php reads it as an equality test against` |
|      - | 1706 | `	 * MT_RAND_PHP alone: every other value, valid or not, is MT19937. It was` |
|      - | 1707 | `	 * declared in the signature and read by nothing, so a program that seeded` |
|      - | 1708 | `	 * with MT_RAND_PHP to reproduce a recorded sequence silently got a different` |
|      - | 1709 | `	 * one — and the constant naming it was undefined, so the call was a fatal. */` |
|    317 | 1710 | `	PH7_VmMtSrand(pCtx->pVm,nSeed,bLegacy);` |
|    317 | 1711 | `	ph7_result_null(pCtx);` |
|    317 | 1712 | `	return PH7_OK;` |
|      3 | 1713 | `}` |
|      - | 1714 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 1715 | `/*` |
|      - | 1716 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|      - | 1717 | ` *  Convert a number between arbitrary bases.` |
|      - | 1718 | ` * Parameters` |
|      - | 1719 | ` * $number` |
|      - | 1720 | ` *  The number to convert` |
|      - | 1721 | ` * $frombase` |
|      - | 1722 | ` *  The base number is in` |
|      - | 1723 | ` * $tobase` |
|      - | 1724 | ` *  The base to convert number to` |
|      - | 1725 | ` * Return` |
|      - | 1726 | ` *  Number converted to base tobase` |
|      - | 1727 | ` */` |
|     72 | 1728 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1729 | `{` |
|      - | 1730 | `	static const char zDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";` |
|      - | 1731 | `	int nLen,iFbase,iTobase,i;` |
|      - | 1732 | `	int bIgnored;` |
|      - | 1733 | `	ph7_int64 iFbase64,iTobase64;` |
|      - | 1734 | `	const char *zNum;` |
|     74 | 1735 | `	sxu64 uNum = 0;` |
|     74 | 1736 | `	if( nArg < 3 ){` |
|      - | 1737 | `		/* Return the empty string*/` |
|    ! 0 | 1738 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1739 | `		return PH7_OK;` |
|      - | 1740 | `	}` |
|      - | 1741 | `	/* Base numbers. Read them as 64-bit so an out-of-range base can't wrap through` |
|      - | 1742 | `	 * a 32-bit truncation back into the 2..36 window and bypass the check below. */` |
|     74 | 1743 | `	iFbase64 = ph7_value_to_int64(apArg[1]);` |
|     74 | 1744 | `	iTobase64 = ph7_value_to_int64(apArg[2]);` |
|      - | 1745 | `	/* PHP 8 throws a catchable ValueError for a base outside 2..36; from_base` |
|      - | 1746 | `	 * is validated before to_base, both before the string is even parsed. */` |
|     74 | 1747 | `	if( iFbase64 < 2 \|\| iFbase64 > 36 ){` |
|      7 | 1748 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1749 | `			"base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)");` |
|      - | 1750 | `	}` |
|     68 | 1751 | `	if( iTobase64 < 2 \|\| iTobase64 > 36 ){` |
|      5 | 1752 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1753 | `			"base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)");` |
|      - | 1754 | `	}` |
|      - | 1755 | `	/* Both bases are now known to fit in [2,36], so the int form is exact. */` |
|     64 | 1756 | `	iFbase  = (int)iFbase64;` |
|     64 | 1757 | `	iTobase = (int)iTobase64;` |
|      - | 1758 | `	/* Parse the input number in from_base. Every base is handled the same way:` |
|      - | 1759 | `	 * digits 0-9 then a-z/A-Z map to 0-35; a character that is not a valid digit for` |
|      - | 1760 | `	 * from_base is ignored, and php raises an E_DEPRECATED saying so. */` |
|     64 | 1761 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 1762 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1763 | `			"base_convert(): Argument #1 ($num) must be of type string, null given");` |
|      - | 1764 | `	}` |
|     64 | 1765 | `	zNum = ph7_value_to_string(apArg[0],&nLen);` |
|     64 | 1766 | `	bIgnored = 0;` |
|     64 | 1767 | `	MathBaseTrimPrefix(&zNum,&nLen,iFbase);` |
|    206 | 1768 | `	for( i = 0 ; i < nLen ; ++i ){` |
|    144 | 1769 | `		int c = (unsigned char)zNum[i];` |
|      - | 1770 | `		int d;` |
|    144 | 1771 | `		if( c >= '0' && c <= '9' ){` |
|    104 | 1772 | `			d = c - '0';` |
|     93 | 1773 | `		}else if( c >= 'a' && c <= 'z' ){` |
|     42 | 1774 | `			d = c - 'a' + 10;` |
|     20 | 1775 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|    ! 0 | 1776 | `			d = c - 'A' + 10;` |
|    ! 0 | 1777 | `		}else{` |
|    ! 0 | 1778 | `			d = 99;` |
|      - | 1779 | `		}` |
|    144 | 1780 | `		if( d >= iFbase ){` |
|      - | 1781 | `			/* Not a valid digit for this base: php skips it and deprecates the skip. */` |
|      6 | 1782 | `			bIgnored = 1;` |
|      6 | 1783 | `			continue;` |
|      - | 1784 | `		}` |
|    140 | 1785 | `		uNum = uNum * (sxu64)iFbase + (sxu64)d;` |
|     71 | 1786 | `	}` |
|     64 | 1787 | `	if( bIgnored ){` |
|      - | 1788 | `		/* §10 rejects php's deprecated surface loudly, and a throw ABORTS the call —` |
|      - | 1789 | `		 * the conversion below is not reached. See MathBaseToNumber's twin. */` |
|      6 | 1790 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1791 | `			"Invalid characters passed for attempted conversion");` |
|      - | 1792 | `	}` |
|      - | 1793 | `	/* Format the result in to_base using lowercase digits. */` |
|     60 | 1794 | `	if( uNum == 0 ){` |
|      5 | 1795 | `		ph7_result_string(pCtx,"0",1);` |
|      3 | 1796 | `	}else{` |
|      - | 1797 | `		char zOut[70]; /* base-2 of a 64-bit value fits in 64 digits */` |
|     56 | 1798 | `		int n = 0,j;` |
|    188 | 1799 | `		while( uNum > 0 ){` |
|    134 | 1800 | `			zOut[n++] = zDigits[uNum % (sxu64)iTobase];` |
|    134 | 1801 | `			uNum /= (sxu64)iTobase;` |
|      2 | 1802 | `		}` |
|      - | 1803 | `		/* Digits were produced least-significant first: reverse in place. */` |
|    110 | 1804 | `		for( j = 0 ; j < n/2 ; ++j ){` |
|     56 | 1805 | `			char t = zOut[j];` |
|     56 | 1806 | `			zOut[j] = zOut[n - 1 - j];` |
|     56 | 1807 | `			zOut[n - 1 - j] = t;` |
|     29 | 1808 | `		}` |
|     56 | 1809 | `		ph7_result_string(pCtx,zOut,n);` |
|      - | 1810 | `	}` |
|     60 | 1811 | `	return PH7_OK;` |
|     38 | 1812 | `}` |
|      - | 1813 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 1814 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1815 |  |
