# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 745/875 lines (85.14%)

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
|    150 |  498 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  499 | `{` |
|      - |  500 | `	int is_float;` |
|      - |  501 | `	/* PHP requires exactly one argument. */` |
|    153 |  502 | `	if( nArg != 1 ){` |
|    ! 0 |  503 | `		return PH7_VmThrowException(pCtx,` |
|      - |  504 | `			"ArgumentCountError",` |
|      - |  505 | `			"abs() expects exactly 1 argument, %d given",` |
|    ! 0 |  506 | `			nArg` |
|      - |  507 | `			);` |
|      - |  508 | `	}` |
|      - |  509 |  |
|    153 |  510 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  511 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|    ! 0 |  512 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  513 | `			"abs(): Argument #1 ($num) must be of type int\|float, null given");` |
|      - |  514 | `	}` |
|      - |  515 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|    153 |  516 | `	is_float = ph7_value_is_float(apArg[0]);` |
|    153 |  517 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
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
|    153 |  533 | `	if( is_float ){` |
|      - |  534 | `		double r,x;` |
|    113 |  535 | `		x = ph7_value_to_double(apArg[0]);` |
|      - |  536 | `		/* Perform the requested operation */` |
|    113 |  537 | `		r = fabs(x);` |
|    113 |  538 | `		ph7_result_double(pCtx,r);` |
|     57 |  539 | `	}else{` |
|      - |  540 | ``		/* Read the full 64-bit value (the old 32-bit `int abs()` truncated any`` |
|      - |  541 | `		 * magnitude above 2^31 and was UB on INT_MIN). */` |
|     41 |  542 | `		sxi64 x = ph7_value_to_int64(apArg[0]);` |
|     41 |  543 | `		if( x == SMALLEST_INT64 ){` |
|      - |  544 | `			/* abs(PHP_INT_MIN) has no int representation, so PHP returns a float. */` |
|      3 |  545 | `			ph7_result_double(pCtx,-(double)x);` |
|      2 |  546 | `		}else{` |
|     39 |  547 | `			ph7_result_int64(pCtx,x < 0 ? -x : x);` |
|      - |  548 | `		}` |
|      - |  549 | `	}` |
|    153 |  550 | `	return PH7_OK;` |
|     78 |  551 | `}` |
|      - |  552 | `/*` |
|      - |  553 | ` * float log(float $num, float $base = M_E)` |
|      - |  554 | ` *  Logarithm of $num to the given base, or the natural logarithm.` |
|      - |  555 | ` *` |
|      - |  556 | ` *  The base used to be read with ph7_value_to_int() and compared against 10, so` |
|      - |  557 | ` *  every base that was not exactly 10 -- 2 included -- silently answered the` |
|      - |  558 | ` *  NATURAL logarithm instead: log(8,2) was 2.0794415416798357 where php answers` |
|      - |  559 | ` *  3.0. A wrong number with no diagnostic on it.` |
|      - |  560 | ` *` |
|      - |  561 | ` *  php's rule, and the reason it is not just log(num)/log(base): the two bases` |
|      - |  562 | ` *  people actually write have exact libm primitives, and the division does not` |
|      - |  563 | ` *  reproduce them -- log(1e300)/log(10) is 299.99999999999994 where log10()` |
|      - |  564 | ` *  gives 300. So 10 and 2 are taken by log10()/log2() before anything else,` |
|      - |  565 | ` *  and that ordering is observable: base 2 and base 10 are answered even when` |
|      - |  566 | ` *  a general base of the same value would have to pass the screens below.` |
|      - |  567 | ` *  Then a base at or below zero is a ValueError, base 1 is NAN (the division` |
|      - |  568 | ` *  would be x/0), and everything else -- INF and NAN included -- divides.` |
|      - |  569 | ` */` |
|     96 |  570 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  571 | `{` |
|      - |  572 | `	double r,x,base;` |
|     97 |  573 | `	if( nArg < 1 ){` |
|      - |  574 | `		/* Missing argument,return 0 */` |
|    ! 0 |  575 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  576 | `		return PH7_OK;` |
|      - |  577 | `	}` |
|     97 |  578 | `	x = ph7_value_to_double(apArg[0]);` |
|     97 |  579 | `	if( nArg < 2 ){` |
|      - |  580 | `		/* The default base is M_E, and log(x)/log(M_E) is log(x) exactly. */` |
|     27 |  581 | `		ph7_result_double(pCtx,log(x));` |
|     27 |  582 | `		return PH7_OK;` |
|      - |  583 | `	}` |
|     71 |  584 | `	base = ph7_value_to_double(apArg[1]);` |
|     71 |  585 | `	if( base == 10.0 ){` |
|     11 |  586 | `		r = log10(x);` |
|     66 |  587 | `	}else if( base == 2.0 ){` |
|     19 |  588 | `		r = log2(x);` |
|     52 |  589 | `	}else if( base <= 0.0 ){` |
|      7 |  590 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  591 | `			"log(): Argument #2 ($base) must be greater than 0");` |
|     37 |  592 | `	}else if( base == 1.0 ){` |
|      9 |  593 | `		r = PH7_NAN_VALUE();` |
|      5 |  594 | `	}else{` |
|     29 |  595 | `		r = log(x) / log(base);` |
|      - |  596 | `	}` |
|      - |  597 | `	/* store the result back */` |
|     65 |  598 | `	ph7_result_double(pCtx,r);` |
|     65 |  599 | `	return PH7_OK;` |
|     49 |  600 | `}` |
|      - |  601 | `/*` |
|      - |  602 | ` * float log10(float $arg )` |
|      - |  603 | ` *  Base-10 logarithm.` |
|      - |  604 | ` * Parameter` |
|      - |  605 | ` *  The number to process.` |
|      - |  606 | ` * Return` |
|      - |  607 | ` *  The Base-10 logarithm of the given number.` |
|      - |  608 | ` */` |
|     14 |  609 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  610 | `{` |
|      - |  611 | `	double r,x;` |
|     15 |  612 | `	if( nArg < 1 ){` |
|      - |  613 | `		/* Missing argument,return 0 */` |
|    ! 0 |  614 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  615 | `		return PH7_OK;` |
|      - |  616 | `	}` |
|     15 |  617 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  618 | `	/* Perform the requested operation */` |
|     15 |  619 | `	r = log10(x);` |
|      - |  620 | `	/* store the result back */` |
|     15 |  621 | `	ph7_result_double(pCtx,r);` |
|     15 |  622 | `	return PH7_OK;` |
|      8 |  623 | `}` |
|      - |  624 | `/*` |
|      - |  625 | ` * mixed pow(mixed $num,mixed $exponent)` |
|      - |  626 | ` *  Exponential expression.` |
|      - |  627 | ` *` |
|      - |  628 | `` *  php does not implement pow() separately: the function and the `**` operator`` |
|      - |  629 | ` *  are the same ZEND_API pow_function, so they share an operand contract, a` |
|      - |  630 | ` *  result TYPE rule and every edge value. Reading the two arguments as doubles` |
|      - |  631 | `` *  and returning pow() shared none of it -- `pow(2,3)` answered float(8) where`` |
|      - |  632 | `` *  `2 ** 3` answers int(8) (a wrong TYPE for the most ordinary call there is),`` |
|      - |  633 | `` *  and the contract `**` enforces was absent entirely: pow('abc',2) answered`` |
|      - |  634 | ` *  float(0), pow([1],2) float(1) and pow($obj,2) float(1) after a conversion` |
|      - |  635 | ` *  warning, where every one of them is` |
|      - |  636 | `` *  `TypeError: Unsupported operand types: … ** int`.`` |
|      - |  637 | ` *` |
|      - |  638 | ` *  Both halves now come from the operator: VmArithOperandCheck() for the` |
|      - |  639 | ` *  contract (including the "A non-numeric value encountered" warning a` |
|      - |  640 | ` *  leading-numeric string gets before it computes with the prefix) and` |
|      - |  641 | ` *  PH7_MemObjPow() for the arithmetic.` |
|      - |  642 | ` * Return` |
|      - |  643 | ` *  base raised to the power of exp -- int when both operands are int, the` |
|      - |  644 | ` *  exponent is non-negative and the exact result fits in an int64; float` |
|      - |  645 | ` *  otherwise.` |
|      - |  646 | ` */` |
|     58 |  647 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  648 | `{` |
|     61 |  649 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  650 | `	ph7_value sBase,sExp;` |
|      - |  651 | `	SyBlob sMsg;` |
|      - |  652 | `	sxi32 rc;` |
|      - |  653 | `	/* Arity (exactly 2) is enforced from aBuiltinArity[] before the call. */` |
|     61 |  654 | `	if( nArg < 2 ){` |
|    ! 0 |  655 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  656 | `		return PH7_OK;` |
|      - |  657 | `	}` |
|     61 |  658 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     61 |  659 | `	if( VmArithOperandCheck(pVm,apArg[0],apArg[1],"**",&sMsg) != SXRET_OK ){` |
|     19 |  660 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|     12 |  661 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|     13 |  662 | `		SyBlobRelease(&sMsg);` |
|     13 |  663 | `		return rc;` |
|      - |  664 | `	}` |
|     49 |  665 | `	SyBlobRelease(&sMsg);` |
|      - |  666 | `	/* Work on COPIES: PH7_MemObjPow converts its operands in place, which the` |
|      - |  667 | `	 * opcode arm may do to its stack slots but a builtin may not do to the` |
|      - |  668 | `	 * caller's arguments. */` |
|     49 |  669 | `	PH7_MemObjInit(pVm,&sBase);` |
|     49 |  670 | `	PH7_MemObjInit(pVm,&sExp);` |
|     49 |  671 | `	PH7_MemObjLoad(apArg[0],&sBase);` |
|     49 |  672 | `	PH7_MemObjLoad(apArg[1],&sExp);` |
|     49 |  673 | `	PH7_MemObjPow(&sBase,&sExp,&sBase);` |
|     49 |  674 | `	if( (sBase.iFlags & MEMOBJ_REAL) != 0 ){` |
|     19 |  675 | `		ph7_result_double(pCtx,sBase.rVal);` |
|     10 |  676 | `	}else{` |
|     31 |  677 | `		ph7_result_int64(pCtx,sBase.x.iVal);` |
|      - |  678 | `	}` |
|     49 |  679 | `	PH7_MemObjRelease(&sBase);` |
|     49 |  680 | `	PH7_MemObjRelease(&sExp);` |
|     49 |  681 | `	return PH7_OK;` |
|     32 |  682 | `}` |
|      - |  683 | `/*` |
|      - |  684 | ` * float pi(void)` |
|      - |  685 | ` *  Returns an approximation of pi.` |
|      - |  686 | ` * Note` |
|      - |  687 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|      - |  688 | ` * Return` |
|      - |  689 | ` *  The value of pi as float.` |
|      - |  690 | ` */` |
|      4 |  691 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  692 | `{` |
|      2 |  693 | `	SXUNUSED(nArg); /* cc warning */` |
|      2 |  694 | `	SXUNUSED(apArg);` |
|      6 |  695 | `	ph7_result_double(pCtx,PH7_PI);` |
|      6 |  696 | `	return PH7_OK;` |
|      2 |  697 | `}` |
|      - |  698 | `/*` |
|      - |  699 | ` * float fmod(float $x,float $y)` |
|      - |  700 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|      - |  701 | ` * Parameters` |
|      - |  702 | ` * $x` |
|      - |  703 | ` *  The dividend` |
|      - |  704 | ` * $y` |
|      - |  705 | ` *  The divisor` |
|      - |  706 | ` * Return` |
|      - |  707 | ` *  The floating point remainder of x/y.` |
|      - |  708 | ` */` |
|      2 |  709 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  710 | `{` |
|      - |  711 | `	double x,y,r;` |
|      3 |  712 | `	if( nArg < 2 ){` |
|      - |  713 | `		/* Missing arguments */` |
|    ! 0 |  714 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  715 | `		return PH7_OK;` |
|      - |  716 | `	}` |
|      - |  717 | `	/* Extract given arguments */` |
|      3 |  718 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  719 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  720 | `	/* Perform the requested operation */` |
|      3 |  721 | `	r = fmod(x,y);` |
|      - |  722 | `	/* Processing result */` |
|      3 |  723 | `	ph7_result_double(pCtx,r);` |
|      3 |  724 | `	return PH7_OK;` |
|      2 |  725 | `}` |
|      - |  726 | `/*` |
|      - |  727 | ` * float hypot(float $x,float $y)` |
|      - |  728 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|      - |  729 | ` * Parameters` |
|      - |  730 | ` * $x` |
|      - |  731 | ` *  Length of first side` |
|      - |  732 | ` * $y` |
|      - |  733 | ` *  Length of first side` |
|      - |  734 | ` * Return` |
|      - |  735 | ` *  Calculated length of the hypotenuse.` |
|      - |  736 | ` */` |
|      2 |  737 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  738 | `{` |
|      - |  739 | `	double x,y,r;` |
|      3 |  740 | `	if( nArg < 2 ){` |
|      - |  741 | `		/* Missing arguments */` |
|    ! 0 |  742 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  743 | `		return PH7_OK;` |
|      - |  744 | `	}` |
|      - |  745 | `	/* Extract given arguments */` |
|      3 |  746 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  747 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  748 | `	/* Perform the requested operation */` |
|      3 |  749 | `	r = hypot(x,y);` |
|      - |  750 | `	/* Processing result */` |
|      3 |  751 | `	ph7_result_double(pCtx,r);` |
|      3 |  752 | `	return PH7_OK;` |
|      2 |  753 | `}` |
|      - |  754 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  755 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  756 | `/* The PH7_ROUND_* mode numbering lives in ph7int.h: bcround() rounds by the` |
|      - |  757 | ` * same eight rules and reads the same RoundingMode cases. */` |
|      - |  758 | `/*` |
|      - |  759 | `` * php 8.4's `enum RoundingMode`, in php's own DECLARATION order -- which is the`` |
|      - |  760 | ` * order cases() reports and is NOT the order of the integer modes above. The two` |
|      - |  761 | ` * numberings disagree past the four HALF_* ones: php's integer 5 is CEILING and` |
|      - |  762 | ` * its enum's fifth case is TowardsZero, so the mapping has to be stated rather` |
|      - |  763 | ` * than computed from an ordinal. A PURE enum (no backing value), which is why the` |
|      - |  764 | `` * case carries its mode HERE instead of in a `case X = 5;` the script could read.`` |
|      - |  765 | ` *` |
|      - |  766 | ` * The enum is what round()'s third argument is documented as; the integer` |
|      - |  767 | ` * spelling stays accepted beside it because php still accepts it, which is what` |
|      - |  768 | `` * `RoundingMode\|int` in the signature says.`` |
|      - |  769 | ` */` |
|      - |  770 | `static const struct MathRoundingModeCase {` |
|      - |  771 | `	const char *zName;` |
|      - |  772 | `	int iMode;` |
|      - |  773 | `} aRoundingMode[] = {` |
|      - |  774 | `	{ "HalfAwayFromZero", PH7_ROUND_HALF_UP        },` |
|      - |  775 | `	{ "HalfTowardsZero",  PH7_ROUND_HALF_DOWN      },` |
|      - |  776 | `	{ "HalfEven",         PH7_ROUND_HALF_EVEN      },` |
|      - |  777 | `	{ "HalfOdd",          PH7_ROUND_HALF_ODD       },` |
|      - |  778 | `	{ "TowardsZero",      PH7_ROUND_TOWARD_ZERO    },` |
|      - |  779 | `	{ "AwayFromZero",     PH7_ROUND_AWAY_FROM_ZERO },` |
|      - |  780 | `	{ "NegativeInfinity", PH7_ROUND_FLOOR          },` |
|      - |  781 | `	{ "PositiveInfinity", PH7_ROUND_CEILING        },` |
|      - |  782 | `};` |
|      - |  783 | `/*` |
|      - |  784 | ` * Answer TRUE (and the integer mode) when pVal is a RoundingMode CASE.` |
|      - |  785 | ` *` |
|      - |  786 | ` * An enum case is an ordinary object here, so the test is its class plus the` |
|      - |  787 | `` * `name` slot every case carries -- the pure enum has no backing value to read.`` |
|      - |  788 | ` */` |
|    504 |  789 | `PH7_PRIVATE int PH7_RoundingModeCase(ph7_value *pVal,int *pMode)` |
|      1 |  790 | `{` |
|      - |  791 | `	ph7_class_instance *pObj;` |
|    505 |  792 | `	const char *zName = 0;` |
|    505 |  793 | `	int nName = 0;` |
|      - |  794 | `	sxu32 n;` |
|    505 |  795 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 ){` |
|    167 |  796 | `		return 0;` |
|      - |  797 | `	}` |
|    339 |  798 | `	pObj = (ph7_class_instance *)pVal->x.pOther;` |
|    338 |  799 | `	if( pObj->pClass == 0 \|\| pObj->pClass->sName.nByte != sizeof("RoundingMode")-1` |
|    339 |  800 | `	 \|\| SyMemcmp(pObj->pClass->sName.zString,"RoundingMode",sizeof("RoundingMode")-1) != 0 ){` |
|    ! 0 |  801 | `		return 0;` |
|      - |  802 | `	}` |
|    339 |  803 | `	PH7_NativeAttrStr(pObj,"name",&zName,&nName);` |
|   1537 |  804 | `	for( n = 0 ; n < SX_ARRAYSIZE(aRoundingMode) ; ++n ){` |
|   1537 |  805 | `		int nCase = (int)SyStrlen(aRoundingMode[n].zName);` |
|   1537 |  806 | `		if( nName == nCase && SyMemcmp(zName,aRoundingMode[n].zName,(sxu32)nCase) == 0 ){` |
|    339 |  807 | `			*pMode = aRoundingMode[n].iMode;` |
|    339 |  808 | `			return 1;` |
|      - |  809 | `		}` |
|    600 |  810 | `	}` |
|    ! 0 |  811 | `	return 0;` |
|    253 |  812 | `}` |
|      - |  813 | `/*` |
|      - |  814 | `` * Declare `enum RoundingMode` -- pure, eight cases, php's declaration order.`` |
|      - |  815 | ` */` |
|   8445 |  816 | `PH7_PRIVATE sxi32 PH7_VmInstallRoundingMode(ph7_vm *pVm)` |
|      5 |  817 | `{` |
|      - |  818 | `	/* The builder DUPLICATES each case name and, for an unbacked enum, keeps no` |
|      - |  819 | `	 * pointer into sValue at all, so this array may live on the stack. */` |
|      - |  820 | `	PH7_NativeEnumCase aCase[SX_ARRAYSIZE(aRoundingMode)];` |
|      - |  821 | `	sxu32 n;` |
|  76010 |  822 | `	for( n = 0 ; n < SX_ARRAYSIZE(aRoundingMode) ; ++n ){` |
|  67565 |  823 | `		aCase[n].zName = aRoundingMode[n].zName;` |
|      - |  824 | `		/* PH7_NATIVE_VAL_NULL: a pure enum's case has no backing value at all. */` |
|  67565 |  825 | `		aCase[n].sValue.zName = 0;` |
|  67565 |  826 | `		aCase[n].sValue.iMods = 0;` |
|  67565 |  827 | `		aCase[n].sValue.iType = PH7_NATIVE_VAL_NULL;` |
|  67565 |  828 | `		aCase[n].sValue.iValue = 0;` |
|  67565 |  829 | `		aCase[n].sValue.zValue = 0;` |
|  67565 |  830 | `		aCase[n].sValue.rValue = 0.0;` |
|  33741 |  831 | `	}` |
|  12667 |  832 | `	return PH7_InstallNativeEnum(&(*pVm),"RoundingMode",0,` |
|   4217 |  833 | `		aCase,SX_ARRAYSIZE(aCase),0,0);` |
|      5 |  834 | `}` |
|      - |  835 | `/*` |
|      - |  836 | ` * 10**power via an exact lookup table for 0..22, falling back to pow()` |
|      - |  837 | ` * otherwise. Port of php-src PHP-8.5 ext/standard/math.c php_intpow10().` |
|      - |  838 | ` */` |
|    422 |  839 | `static double MathIntPow10(int power)` |
|      5 |  840 | `{` |
|      - |  841 | `	static const double powers[] = {` |
|      - |  842 | `		1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,` |
|      - |  843 | `		1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22` |
|      - |  844 | `	};` |
|    427 |  845 | `	if( power < 0 \|\| power > 22 ){` |
|      5 |  846 | `		return pow(10.0, (double)power);` |
|      - |  847 | `	}` |
|    423 |  848 | `	return powers[power];` |
|    216 |  849 | `}` |
|    318 |  850 | `static double MathRoundBasicEdge(double integral, double exponent, int places)` |
|      5 |  851 | `{` |
|    164 |  852 | `	return (places > 0)` |
|     98 |  853 | `		? fabs((integral + copysign(0.5, integral)) / exponent)` |
|    269 |  854 | `		: fabs((integral + copysign(0.5, integral)) * exponent);` |
|      5 |  855 | `}` |
|     74 |  856 | `static double MathRoundZeroEdge(double integral, double exponent, int places)` |
|      1 |  857 | `{` |
|     38 |  858 | `	return (places > 0)` |
|    ! 0 |  859 | `		? fabs((integral) / exponent)` |
|     74 |  860 | `		: fabs((integral) * exponent);` |
|      1 |  861 | `}` |
|      - |  862 | `/*` |
|      - |  863 | ` * Round the extracted integral part according to the requested mode.` |
|      - |  864 | ` * Faithful port of php-src PHP-8.5 ext/standard/math.c php_round_helper().` |
|      - |  865 | ` */` |
|    416 |  866 | `static double MathRoundHelper(double integral, double value, double exponent, int places, int mode)` |
|      5 |  867 | `{` |
|    421 |  868 | `	double value_abs = fabs(value);` |
|      - |  869 | `	double edge_case;` |
|    421 |  870 | `	switch( mode ){` |
|    105 |  871 | `		case PH7_ROUND_HALF_UP:` |
|    215 |  872 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|    215 |  873 | `			if( value_abs >= edge_case ){` |
|    133 |  874 | `				return integral + copysign(1.0, integral);` |
|      - |  875 | `			}` |
|     85 |  876 | `			return integral;` |
|     16 |  877 | `		case PH7_ROUND_HALF_DOWN:` |
|     33 |  878 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|     33 |  879 | `			if( value_abs > edge_case ){` |
|    ! 0 |  880 | `				return integral + copysign(1.0, integral);` |
|      - |  881 | `			}` |
|     33 |  882 | `			return integral;` |
|     13 |  883 | `		case PH7_ROUND_CEILING:` |
|     27 |  884 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|     27 |  885 | `			if( value > 0.0 && value_abs > edge_case ){` |
|     15 |  886 | `				return integral + 1.0;` |
|      - |  887 | `			}` |
|     13 |  888 | `			return integral;` |
|     12 |  889 | `		case PH7_ROUND_FLOOR:` |
|     25 |  890 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|     25 |  891 | `			if( value < 0.0 && value_abs > edge_case ){` |
|     11 |  892 | `				return integral - 1.0;` |
|      - |  893 | `			}` |
|     15 |  894 | `			return integral;` |
|     12 |  895 | `		case PH7_ROUND_TOWARD_ZERO:` |
|     25 |  896 | `			return integral;` |
|     12 |  897 | `		case PH7_ROUND_AWAY_FROM_ZERO:` |
|     25 |  898 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|     25 |  899 | `			if( value_abs > edge_case ){` |
|     25 |  900 | `				return integral + copysign(1.0, integral);` |
|      - |  901 | `			}` |
|    ! 0 |  902 | `			return integral;` |
|     22 |  903 | `		case PH7_ROUND_HALF_EVEN:` |
|     45 |  904 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|     45 |  905 | `			if( value_abs > edge_case ){` |
|    ! 0 |  906 | `				return integral + copysign(1.0, integral);` |
|     45 |  907 | `			}else if( value_abs == edge_case ){` |
|     35 |  908 | `				if( fmod(integral, 2.0) != 0.0 ){ /* integral not even -> make it even */` |
|     19 |  909 | `					return integral + copysign(1.0, integral);` |
|      - |  910 | `				}` |
|      8 |  911 | `			}` |
|     27 |  912 | `			return integral;` |
|     16 |  913 | `		case PH7_ROUND_HALF_ODD:` |
|     33 |  914 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|     33 |  915 | `			if( value_abs > edge_case ){` |
|    ! 0 |  916 | `				return integral + copysign(1.0, integral);` |
|     33 |  917 | `			}else if( value_abs == edge_case ){` |
|     25 |  918 | `				if( fmod(integral, 2.0) == 0.0 ){ /* integral even -> make it odd */` |
|     15 |  919 | `					return integral + copysign(1.0, integral);` |
|      - |  920 | `				}` |
|      5 |  921 | `			}` |
|     19 |  922 | `			return integral;` |
|    ! 0 |  923 | `		default:` |
|    ! 0 |  924 | `			return integral; /* unreachable: mode validated by the caller */` |
|      - |  925 | `	}` |
|    213 |  926 | `}` |
|      - |  927 | `/*` |
|      - |  928 | `` * Round `value` to `places` decimals in `mode`. Faithful port of php-src`` |
|      - |  929 | ` * PHP-8.5 ext/standard/math.c _php_math_round() — the post-8.4` |
|      - |  930 | ` * integer-extraction algorithm with the +/-1 floating-point error` |
|      - |  931 | ` * correction step, required for byte-exact results on cases such as` |
|      - |  932 | ` * round(0.285, 2) == 0.29 that the old naive "+0.5" approach got wrong.` |
|      - |  933 | ` */` |
|    438 |  934 | `static double MathRound(double value, int places, int mode)` |
|      5 |  935 | `{` |
|      - |  936 | `	double exponent, tmp_value, tmp_value2;` |
|      - |  937 | `	int abs_places;` |
|    443 |  938 | `	if( !isfinite(value) \|\| value == 0.0 ){` |
|     17 |  939 | `		return value;` |
|      - |  940 | `	}` |
|      - |  941 | `	/* mirror php-src's clamp away from INT_MIN */` |
|    427 |  942 | `	if( places < -2147483647 ){` |
|    ! 0 |  943 | `		places = -2147483647;` |
|    ! 0 |  944 | `	}` |
|    427 |  945 | `	abs_places = places < 0 ? -places : places;` |
|    427 |  946 | `	exponent = MathIntPow10(abs_places);` |
|      - |  947 | `	/*` |
|      - |  948 | `	 * Extracting the integer part can be off by one ULP due to float error` |
|      - |  949 | `	 * (e.g. floor(0.285 * 1e10) == 2849999999). Try +/-1 and keep it if it` |
|      - |  950 | ``	 * divides back to exactly `value`.`` |
|      - |  951 | `	 */` |
|    427 |  952 | `	if( value >= 0.0 ){` |
|    329 |  953 | `		tmp_value = floor(places > 0 ? value * exponent : value / exponent);` |
|    329 |  954 | `		tmp_value2 = tmp_value + 1.0;` |
|    167 |  955 | `	}else{` |
|     99 |  956 | `		tmp_value = ceil(places > 0 ? value * exponent : value / exponent);` |
|     99 |  957 | `		tmp_value2 = tmp_value - 1.0;` |
|      - |  958 | `	}` |
|    427 |  959 | `	if( (places > 0 ? tmp_value2 / exponent : tmp_value2 * exponent) == value ){` |
|      7 |  960 | `		tmp_value = tmp_value2;` |
|      3 |  961 | `	}` |
|      - |  962 | `	/* Beyond our precision, so rounding it is pointless. */` |
|    427 |  963 | `	if( fabs(tmp_value) >= 1e16 ){` |
|      7 |  964 | `		return value;` |
|      - |  965 | `	}` |
|    421 |  966 | `	tmp_value = MathRoundHelper(tmp_value, value, exponent, places, mode);` |
|    421 |  967 | `	if( abs_places < 23 ){` |
|    421 |  968 | `		tmp_value = (places > 0) ? tmp_value / exponent : tmp_value * exponent;` |
|    213 |  969 | `	}else{` |
|      - |  970 | `		/*` |
|      - |  971 | `		 * Simple division would lose precision here; round-trip through a` |
|      - |  972 | `		 * string exactly like php-src does (snprintf "%15fe%d" + strtod).` |
|      - |  973 | `		 * libc snprintf is used (not SyBufferFormat, which is not` |
|      - |  974 | `		 * correctly-rounded) so the low bits match PHP. (SyStrToReal now` |
|      - |  975 | `		 * delegates to strtod too; the direct call here simply mirrors` |
|      - |  976 | `		 * php-src's own snprintf+strtod pairing.)` |
|      - |  977 | `		 */` |
|      - |  978 | `		char zBuf[64];` |
|    ! 0 |  979 | `		snprintf(zBuf, sizeof(zBuf), "%15fe%d", tmp_value, -places);` |
|    ! 0 |  980 | `		zBuf[sizeof(zBuf)-1] = '\0';` |
|    ! 0 |  981 | `		tmp_value = strtod(zBuf, 0);` |
|    ! 0 |  982 | `		if( !isfinite(tmp_value) \|\| isnan(tmp_value) ){` |
|    ! 0 |  983 | `			tmp_value = value;` |
|    ! 0 |  984 | `		}` |
|      - |  985 | `	}` |
|    421 |  986 | `	return tmp_value;` |
|    224 |  987 | `}` |
|      - |  988 | `/*` |
|      - |  989 | ` * float round ( int\|float $num [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|      - |  990 | ` *  Rounds a float.` |
|      - |  991 | ` * Parameters` |
|      - |  992 | ` *  $num       The value to round.` |
|      - |  993 | ` *  $precision The optional number of decimal digits to round to. May be` |
|      - |  994 | ` *             negative (rounds to the left of the decimal point).` |
|      - |  995 | ` *  $mode      One of PHP_ROUND_HALF_UP (default) / _HALF_DOWN / _HALF_EVEN /` |
|      - |  996 | ` *             _HALF_ODD, or the 8.5 integer modes CEILING / FLOOR /` |
|      - |  997 | ` *             TOWARD_ZERO / AWAY_FROM_ZERO (5..8).` |
|      - |  998 | ` * Return` |
|      - |  999 | ` *  The rounded value as a float.` |
|      - | 1000 | ` */` |
|    366 | 1001 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1002 | `{` |
|      - | 1003 | `	double value, r;` |
|    368 | 1004 | `	int places = 0;` |
|    368 | 1005 | `	int mode = PH7_ROUND_HALF_UP;` |
|      - | 1006 | `	/*` |
|      - | 1007 | `	 * Legacy PHL contract: no argument -> int(0). PHP throws an` |
|      - | 1008 | `	 * ArgumentCountError here, but two PHL-only (--SKIPIF-- zend_version)` |
|      - | 1009 | `	 * tests assert round()===0, so keep the historical behavior.` |
|      - | 1010 | `	 */` |
|    368 | 1011 | `	if( nArg < 1 ){` |
|    ! 0 | 1012 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1013 | `		return PH7_OK;` |
|      - | 1014 | `	}` |
|    368 | 1015 | `	if( nArg > 3 ){` |
|    ! 0 | 1016 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1017 | `			"ArgumentCountError",` |
|      - | 1018 | `			"round() expects at most 3 arguments, %d given",` |
|    ! 0 | 1019 | `			nArg` |
|      - | 1020 | `			);` |
|      - | 1021 | `	}` |
|      - | 1022 | `	/* Argument #1's type is the aBuiltinSig[] row's -- see floor() above. */` |
|      - | 1023 | `	/* Precision (arg #2). Negative values are valid; clamp to int range. */` |
|    368 | 1024 | `	if( nArg > 1 ){` |
|    328 | 1025 | `		sxi64 prec = ph7_value_to_int64(apArg[1]);` |
|    328 | 1026 | `		if( prec > 2147483647 ){` |
|    ! 0 | 1027 | `			places = 2147483647;` |
|    328 | 1028 | `		}else if( prec < -2147483647 ){` |
|    ! 0 | 1029 | `			places = -2147483647;` |
|    ! 0 | 1030 | `		}else{` |
|    328 | 1031 | `			places = (int)prec;` |
|      - | 1032 | `		}` |
|    163 | 1033 | `	}` |
|      - | 1034 | `	/*` |
|      - | 1035 | ``	 * Mode (arg #3). php declares it `RoundingMode\|int`, so an enum CASE and the`` |
|      - | 1036 | `	 * raw integer both arrive here. The integer modes are 1..8; read the full` |
|      - | 1037 | `	 * 64-bit value before range-checking so a large out-of-range mode cannot` |
|      - | 1038 | `	 * alias a valid 1..8 via a truncating 32-bit cast (e.g. 0x1_0000_0003).` |
|      - | 1039 | `	 */` |
|    368 | 1040 | `	if( nArg > 2 ){` |
|      - | 1041 | ``		/* A RoundingMode case answers its integer mode straight into `mode`; the`` |
|      - | 1042 | `		 * integer spelling is range-checked here. */` |
|    253 | 1043 | `		if( !PH7_RoundingModeCase(apArg[2],&mode) ){` |
|    167 | 1044 | `			sxi64 m = ph7_value_to_int64(apArg[2]);` |
|    167 | 1045 | `			if( m < PH7_ROUND_HALF_UP \|\| m > PH7_ROUND_AWAY_FROM_ZERO ){` |
|     13 | 1046 | `				return PH7_VmThrowException(pCtx,` |
|      - | 1047 | `					"ValueError",` |
|      - | 1048 | `					"round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)"` |
|      - | 1049 | `					);` |
|      - | 1050 | `			}` |
|    155 | 1051 | `			mode = (int)m;` |
|     77 | 1052 | `		}` |
|    120 | 1053 | `	}` |
|    356 | 1054 | `	value = ph7_value_to_double(apArg[0]);` |
|      - | 1055 | `	/* Integer input with non-negative precision needs no rounding. */` |
|    356 | 1056 | `	if( ph7_value_is_int(apArg[0]) && places >= 0 ){` |
|     21 | 1057 | `		ph7_result_double(pCtx,value);` |
|     21 | 1058 | `		return PH7_OK;` |
|      - | 1059 | `	}` |
|    336 | 1060 | `	r = MathRound(value, places, mode);` |
|    336 | 1061 | `	ph7_result_double(pCtx,r);` |
|    336 | 1062 | `	return PH7_OK;` |
|    185 | 1063 | `}` |
|      - | 1064 | `/*` |
|      - | 1065 | ` * Assemble php's formatted number: the integer digits grouped from the right by` |
|      - | 1066 | ` * the thousands separator, then the decimal separator and exactly $decimals` |
|      - | 1067 | ` * fraction digits (right-padded with '0', since the printf may produce fewer).` |
|      - | 1068 | ` */` |
|    138 | 1069 | `static int NumberFormatEmit(ph7_context *pCtx,` |
|      - | 1070 | `	const char *zDigits,int nDigits,int bNeg,` |
|      - | 1071 | `	const char *zFrac,int nFrac,int nDec,` |
|      - | 1072 | `	const char *zPoint,int nPoint,const char *zSep,int nSep)` |
|      4 | 1073 | `{` |
|      - | 1074 | `	SyBlob sOut;` |
|      - | 1075 | `	int i;` |
|    142 | 1076 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    142 | 1077 | `	if( bNeg ){` |
|     12 | 1078 | `		SyBlobAppend(&sOut,"-",sizeof(char));` |
|      5 | 1079 | `	}` |
|    844 | 1080 | `	for( i = 0 ; i < nDigits ; ++i ){` |
|    706 | 1081 | `		if( i > 0 && nSep > 0 && ((nDigits - i) % 3) == 0 ){` |
|    176 | 1082 | `			SyBlobAppend(&sOut,zSep,(sxu32)nSep);` |
|     86 | 1083 | `		}` |
|    706 | 1084 | `		SyBlobAppend(&sOut,&zDigits[i],sizeof(char));` |
|    355 | 1085 | `	}` |
|    142 | 1086 | `	if( nDec > 0 ){` |
|     62 | 1087 | `		if( nPoint > 0 ){` |
|     58 | 1088 | `			SyBlobAppend(&sOut,zPoint,(sxu32)nPoint);` |
|     27 | 1089 | `		}` |
|     62 | 1090 | `		if( nFrac > nDec ){` |
|    ! 0 | 1091 | `			nFrac = nDec;` |
|    ! 0 | 1092 | `		}` |
|     62 | 1093 | `		if( nFrac > 0 ){` |
|     60 | 1094 | `			SyBlobAppend(&sOut,zFrac,(sxu32)nFrac);` |
|     28 | 1095 | `		}` |
|     66 | 1096 | `		for( i = nFrac ; i < nDec ; ++i ){` |
|      5 | 1097 | `			SyBlobAppend(&sOut,"0",sizeof(char));` |
|      3 | 1098 | `		}` |
|     29 | 1099 | `	}` |
|    142 | 1100 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    142 | 1101 | `	SyBlobRelease(&sOut);` |
|    142 | 1102 | `	return PH7_OK;` |
|      4 | 1103 | `}` |
|      - | 1104 | `/*` |
|      - | 1105 | ` * _php_math_number_format_long(): an INTEGER never goes through a double, so` |
|      - | 1106 | ` * every digit of a value past 2^53 survives. A NEGATIVE $decimals rounds the` |
|      - | 1107 | ` * integer itself, half away from zero.` |
|      - | 1108 | ` */` |
|     44 | 1109 | `static int NumberFormatLong(ph7_context *pCtx,sxi64 iVal,int nDec,` |
|      - | 1110 | `	const char *zPoint,int nPoint,const char *zSep,int nSep)` |
|      2 | 1111 | `{` |
|      - | 1112 | `	char zBuf[32];` |
|      - | 1113 | `	sxu64 uNum;` |
|     46 | 1114 | `	int bNeg = 0;` |
|     46 | 1115 | `	int n = 0;` |
|     46 | 1116 | `	if( iVal < 0 ){` |
|     11 | 1117 | `		bNeg = 1;` |
|      - | 1118 | `		/* -PHP_INT_MIN does not fit; negate through the unsigned domain. */` |
|     11 | 1119 | `		uNum = ((sxu64)-(iVal + 1)) + 1;` |
|      6 | 1120 | `	}else{` |
|     36 | 1121 | `		uNum = (sxu64)iVal;` |
|      - | 1122 | `	}` |
|     46 | 1123 | `	if( nDec < 0 ){` |
|      - | 1124 | `		/* php keeps a table of the 20 powers of ten a 64-bit value can hold and` |
|      - | 1125 | `		 * answers 0 past it; 10^19 is the last one that fits. */` |
|     21 | 1126 | `		if( nDec < -19 ){` |
|      3 | 1127 | `			uNum = 0;` |
|      2 | 1128 | `		}else{` |
|     19 | 1129 | `			sxu64 uPow = 1;` |
|      - | 1130 | `			sxu64 uRest;` |
|      - | 1131 | `			int k;` |
|     49 | 1132 | `			for( k = 0 ; k < -nDec ; ++k ){` |
|     31 | 1133 | `				uPow *= 10;` |
|     16 | 1134 | `			}` |
|     19 | 1135 | `			uRest = uNum % uPow;` |
|     19 | 1136 | `			uNum = uNum / uPow;` |
|     19 | 1137 | `			uNum = (uRest >= uPow / 2) ? uNum * uPow + uPow : uNum * uPow;` |
|      - | 1138 | `		}` |
|     21 | 1139 | `		if( uNum == 0 ){` |
|      - | 1140 | `			/* php never answers "-0". */` |
|      9 | 1141 | `			bNeg = 0;` |
|      4 | 1142 | `		}` |
|     10 | 1143 | `	}` |
|      - | 1144 | `	/* Decimal digits, most significant first. */` |
|     46 | 1145 | `	if( uNum == 0 ){` |
|     14 | 1146 | `		zBuf[n++] = '0';` |
|      8 | 1147 | `	}else{` |
|      - | 1148 | `		char zRev[32];` |
|     34 | 1149 | `		int nRev = 0;` |
|    372 | 1150 | `		while( uNum > 0 && nRev < (int)sizeof(zRev) ){` |
|    340 | 1151 | `			zRev[nRev++] = (char)('0' + (int)(uNum % 10));` |
|    340 | 1152 | `			uNum /= 10;` |
|      2 | 1153 | `		}` |
|    372 | 1154 | `		while( nRev > 0 ){` |
|    340 | 1155 | `			zBuf[n++] = zRev[--nRev];` |
|      2 | 1156 | `		}` |
|      - | 1157 | `	}` |
|     46 | 1158 | `	return NumberFormatEmit(pCtx,zBuf,n,bNeg,0,0,nDec > 0 ? nDec : 0,` |
|     22 | 1159 | `		zPoint,nPoint,zSep,nSep);` |
|      2 | 1160 | `}` |
|      - | 1161 | `/*` |
|      - | 1162 | ` * string number_format(int\|float $num,int $decimals = 0,` |
|      - | 1163 | ` *                      ?string $decimal_separator = ".",` |
|      - | 1164 | ` *                      ?string $thousands_separator = ",")` |
|      - | 1165 | ` *  Format a number with grouped thousands.` |
|      - | 1166 | ` */` |
|    206 | 1167 | `PH7_PRIVATE int PH7_builtin_number_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1168 | `{` |
|    210 | 1169 | `	const char *zPoint = ".", *zSep = ",";` |
|    210 | 1170 | `	int nPoint = 1, nSep = 1;` |
|    210 | 1171 | `	int nDec = 0;` |
|      - | 1172 | `	ph7_value sNum;` |
|      - | 1173 | `	double d;` |
|    210 | 1174 | `	int bNeg = 0;` |
|      - | 1175 | `	int nLen,nInt;` |
|      - | 1176 | `	char *zFmt;` |
|      - | 1177 | `	const char *zDot;` |
|    210 | 1178 | `	if( nArg < 1 ){` |
|      - | 1179 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|    ! 0 | 1180 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1181 | `		return PH7_OK;` |
|      - | 1182 | `	}` |
|      - | 1183 | `	/* Every refusal is worded here rather than by the shared screen: php's stub` |
|      - | 1184 | ``	 * declares `float $num` (which is what Reflection prints) but the ZPP macro`` |
|      - | 1185 | ``	 * behind it is Z_PARAM_NUMBER, whose TypeError says `int\|float`. An int stays`` |
|      - | 1186 | `	 * an INT, a numeric string takes the shape it looks like, and null is the scope policy's` |
|      - | 1187 | `	 * refusal of a deprecation. */` |
|    210 | 1188 | `	if( !PH7_MemObjIsNumeric(apArg[0]) ){` |
|      - | 1189 | `		char zBuf[64];` |
|     60 | 1190 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1191 | `			"number_format(): Argument #1 ($num) must be of type int\|float, %s given",` |
|     32 | 1192 | `			ph7_value_is_string(apArg[0]) ? "string"` |
|     18 | 1193 | `				: VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|      - | 1194 | `	}` |
|    178 | 1195 | `	if( nArg > 1 ){` |
|      - | 1196 | ``		/* php declares `int $decimals`; the string and float narrowings it only`` |
|      - | 1197 | `		 * DEPRECATES are rejected here (the scope policy), as they are for count_chars(). */` |
|    126 | 1198 | `		if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|    127 | 1199 | `		 \|\| ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|      - | 1200 | `			char zBuf[64];` |
|     14 | 1201 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1202 | `				"number_format(): Argument #2 ($decimals) must be of type int, %s given",` |
|      8 | 1203 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 1204 | `		}` |
|    122 | 1205 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 1206 | `			/* php wants the WHOLE string to be numeric ("2abc" is a TypeError,` |
|      - | 1207 | `			 * not 2), and a float-shaped one that would LOSE something is the scope policy's` |
|      - | 1208 | `			 * refusal of a deprecation. */` |
|      - | 1209 | `			double dMode;` |
|      8 | 1210 | `			if( !PH7_MemObjStringIsNumeric(apArg[1]) ){` |
|      6 | 1211 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1212 | `					"number_format(): Argument #2 ($decimals) must be of type int, string given");` |
|      - | 1213 | `			}` |
|      3 | 1214 | `			dMode = ph7_value_to_double(apArg[1]);` |
|      - | 1215 | ``			/* Range first: `(sxi64)dMode` is undefined outside it. */`` |
|      3 | 1216 | `			if( !PH7_RealFitsInt64(dMode) \|\| dMode != (double)(sxi64)dMode ){` |
|    ! 0 | 1217 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1218 | `					"number_format(): Argument #2 ($decimals) must be of type int, string given");` |
|      1 | 1219 | `			}` |
|    117 | 1220 | `		}else if( ph7_value_is_float(apArg[1]) ){` |
|      6 | 1221 | `			double dMode = ph7_value_to_double(apArg[1]);` |
|      6 | 1222 | `			if( !PH7_RealFitsInt64(dMode) \|\| dMode != (double)(sxi64)dMode ){` |
|      3 | 1223 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1224 | `					"number_format(): Argument #2 ($decimals) must be of type int, float given");` |
|      - | 1225 | `			}` |
|      1 | 1226 | `		}` |
|      - | 1227 | `		{` |
|    116 | 1228 | `			sxi64 iDec = ph7_value_to_int64(apArg[1]);` |
|      - | 1229 | `			/* php clamps the declared long onto an int before it formats. */` |
|    172 | 1230 | `			nDec = iDec > 2147483647 ? 2147483647` |
|    112 | 1231 | `			     : (iDec < -2147483647 ? -2147483647 : (int)iDec);` |
|      - | 1232 | `		}` |
|     56 | 1233 | `	}` |
|      - | 1234 | ``	/* Both separators are `?string`: null means php's default, not the empty`` |
|      - | 1235 | `	 * string. An empty string IS accepted and simply omits the separator, and an` |
|      - | 1236 | `	 * object that can stringify is coerced. */` |
|    164 | 1237 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     37 | 1238 | `		if( !PH7_ArgSatisfiesString(apArg[2]) ){` |
|      - | 1239 | `			char zBuf[64];` |
|     14 | 1240 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1241 | `				"number_format(): Argument #3 ($decimal_separator) must be of type ?string, %s given",` |
|      8 | 1242 | `				VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 1243 | `		}` |
|     29 | 1244 | `		zPoint = ph7_value_to_string(apArg[2],&nPoint);` |
|     13 | 1245 | `	}` |
|    156 | 1246 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     29 | 1247 | `		if( !PH7_ArgSatisfiesString(apArg[3]) ){` |
|      - | 1248 | `			char zBuf[64];` |
|      7 | 1249 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1250 | `				"number_format(): Argument #4 ($thousands_separator) must be of type ?string, %s given",` |
|      4 | 1251 | `				VmValueGivenName(apArg[3],zBuf,sizeof(zBuf)));` |
|      - | 1252 | `		}` |
|     25 | 1253 | `		zSep = ph7_value_to_string(apArg[3],&nSep);` |
|     11 | 1254 | `	}` |
|    152 | 1255 | `	PH7_MemObjInit(pCtx->pVm,&sNum);` |
|    152 | 1256 | `	PH7_MemObjStore(apArg[0],&sNum);` |
|    152 | 1257 | `	PH7_MemObjToNumeric(&sNum);` |
|    152 | 1258 | `	if( (sNum.iFlags & MEMOBJ_REAL) == 0 ){` |
|     44 | 1259 | `		int rc = NumberFormatLong(pCtx,sNum.x.iVal,nDec,zPoint,nPoint,zSep,nSep);` |
|     44 | 1260 | `		PH7_MemObjRelease(&sNum);` |
|     44 | 1261 | `		return rc;` |
|      - | 1262 | `	}` |
|    110 | 1263 | `	d = (double)sNum.rVal;` |
|    110 | 1264 | `	PH7_MemObjRelease(&sNum);` |
|      - | 1265 | `	/* A double past 2^52 has no fractional digits left, so php formats it as an` |
|      - | 1266 | `	 * INTEGER when it fits one — that is what keeps 4503599627370496.0 exact. */` |
|    106 | 1267 | `	if( (d >= 4503599627370496.0 \|\| d <= -4503599627370496.0)` |
|     62 | 1268 | `	 && PH7_RealFitsInt64(d) ){` |
|      3 | 1269 | `		return NumberFormatLong(pCtx,(sxi64)d,nDec,zPoint,nPoint,zSep,nSep);` |
|      - | 1270 | `	}` |
|    108 | 1271 | `	if( d < 0 ){` |
|     14 | 1272 | `		bNeg = 1;` |
|     14 | 1273 | `		d = -d;` |
|      6 | 1274 | `	}` |
|    108 | 1275 | `	d = MathRound(d,nDec,PH7_ROUND_HALF_UP);` |
|    108 | 1276 | `	if( nDec < 0 ){` |
|     16 | 1277 | `		nDec = 0;` |
|      7 | 1278 | `	}` |
|      - | 1279 | `	/* libc's %f, not the engine's formatter: php prints through its own` |
|      - | 1280 | `	 * snprintf here, so INF answers "inf" and NAN "nan" — and the engine's` |
|      - | 1281 | `	 * formatter caps the precision at 53 digits with a notice, where php` |
|      - | 1282 | `	 * honours whatever $decimals asks for. */` |
|    108 | 1283 | `	nLen = snprintf(0,0,"%.*f",nDec,d);` |
|    108 | 1284 | `	if( nLen < 0 ){` |
|    ! 0 | 1285 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1286 | `	}` |
|    108 | 1287 | `	zFmt = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen + 1,FALSE,TRUE);` |
|    108 | 1288 | `	if( zFmt == 0 ){` |
|    ! 0 | 1289 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1290 | `	}` |
|    108 | 1291 | `	snprintf(zFmt,(size_t)nLen + 1,"%.*f",nDec,d);` |
|    108 | 1292 | `	if( zFmt[0] < '0' \|\| zFmt[0] > '9' ){` |
|      - | 1293 | `		/* Not a number at all (inf/nan): php hands its buffer straight back,` |
|      - | 1294 | `		 * without a sign, a separator or any padding. */` |
|     11 | 1295 | `		ph7_result_string(pCtx,zFmt,nLen);` |
|     11 | 1296 | `		return PH7_OK;` |
|      - | 1297 | `	}` |
|     98 | 1298 | `	if( bNeg && d == 0 ){` |
|      - | 1299 | `		/* Rounded away to zero; php never answers "-0". */` |
|      5 | 1300 | `		bNeg = 0;` |
|      2 | 1301 | `	}` |
|      - | 1302 | `	/* php looks for '.' OR ',' — the decimal point its formatter produced. */` |
|     98 | 1303 | `	zDot = 0;` |
|     98 | 1304 | `	if( nDec > 0 ){` |
|      - | 1305 | `		int i;` |
|    284 | 1306 | `		for( i = 0 ; i < nLen ; ++i ){` |
|    284 | 1307 | `			if( zFmt[i] == '.' \|\| zFmt[i] == ',' ){` |
|     60 | 1308 | `				zDot = &zFmt[i];` |
|     60 | 1309 | `				break;` |
|      - | 1310 | `			}` |
|    116 | 1311 | `		}` |
|     28 | 1312 | `	}` |
|     98 | 1313 | `	nInt = zDot ? (int)(zDot - zFmt) : nLen;` |
|    173 | 1314 | `	return NumberFormatEmit(pCtx,zFmt,nInt,bNeg,` |
|     75 | 1315 | `		zDot ? zDot + 1 : 0,zDot ? nLen - nInt - 1 : 0,` |
|     47 | 1316 | `		nDec,zPoint,nPoint,zSep,nSep);` |
|    107 | 1317 | `}` |
|      - | 1318 | `/*` |
|      - | 1319 | ` * int intdiv(int $a, int $b)` |
|      - | 1320 | ` *  Integer division.` |
|      - | 1321 | ` * Parameters` |
|      - | 1322 | ` *  $a` |
|      - | 1323 | ` *   Number to be divided.` |
|      - | 1324 | ` *  $b` |
|      - | 1325 | ` *   Number which divides the $a.` |
|      - | 1326 | ` * Return` |
|      - | 1327 | ` *  The integer quotient of the division of $a by $b.` |
|      - | 1328 | ` */` |
|    150 | 1329 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1330 | `{` |
|      - | 1331 | `	char zGiven[64];` |
|      - | 1332 | `	sxi64 a,b;` |
|      - | 1333 | `	/* PHP requires exactly two arguments. */` |
|    154 | 1334 | `	if( nArg != 2 ){` |
|    ! 0 | 1335 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1336 | `			"ArgumentCountError",` |
|      - | 1337 | `			"intdiv() expects exactly 2 arguments, %d given",` |
|    ! 0 | 1338 | `			nArg` |
|      - | 1339 | `			);` |
|      - | 1340 | `	}` |
|      - | 1341 | `	/* Type-check argument 1 */` |
|    150 | 1342 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|    154 | 1343 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1344 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1345 | `			"TypeError",` |
|      - | 1346 | `			"intdiv(): Argument #1 ($num1) must be of type int, %s given",` |
|    ! 0 | 1347 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|      - | 1348 | `			);` |
|      - | 1349 | `	}` |
|    154 | 1350 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1351 | `		int len;` |
|      3 | 1352 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|      3 | 1353 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|    ! 0 | 1354 | `			return PH7_VmThrowException(pCtx,` |
|      - | 1355 | `				"TypeError",` |
|      - | 1356 | `				"intdiv(): Argument #1 ($num1) must be of type int, string given"` |
|      - | 1357 | `				);` |
|      - | 1358 | `		}` |
|      1 | 1359 | `	}` |
|      - | 1360 | `	/* Type-check argument 2 */` |
|    150 | 1361 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|    154 | 1362 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|    ! 0 | 1363 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1364 | `			"TypeError",` |
|      - | 1365 | `			"intdiv(): Argument #2 ($num2) must be of type int, %s given",` |
|    ! 0 | 1366 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|      - | 1367 | `			);` |
|      - | 1368 | `	}` |
|    154 | 1369 | `	if( ph7_value_is_string(apArg[1]) ){` |
|      - | 1370 | `		int len;` |
|    ! 0 | 1371 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|    ! 0 | 1372 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|    ! 0 | 1373 | `			return PH7_VmThrowException(pCtx,` |
|      - | 1374 | `				"TypeError",` |
|      - | 1375 | `				"intdiv(): Argument #2 ($num2) must be of type int, string given"` |
|      - | 1376 | `				);` |
|      - | 1377 | `		}` |
|    ! 0 | 1378 | `	}` |
|      - | 1379 | `	/* Convert both arguments to int64 */` |
|      - | 1380 | `	{` |
|      - | 1381 | `		/* php's ZPP contract for the two int params (lossy float / float-string` |
|      - | 1382 | `		 * deprecations); the manual type checks above already covered arrays,` |
|      - | 1383 | `		 * objects and non-numeric strings with the same messages. */` |
|    154 | 1384 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[0],"intdiv",1,"$num1","int",&a);` |
|    154 | 1385 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 1386 | `			return rcArg;` |
|      - | 1387 | `		}` |
|    154 | 1388 | `		rcArg = PH7_IntArgResolve(pCtx,apArg[1],"intdiv",2,"$num2","int",&b);` |
|    154 | 1389 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 1390 | `			return rcArg;` |
|      - | 1391 | `		}` |
|      - | 1392 | `	}` |
|      - | 1393 | `	/* Check for division by zero */` |
|    154 | 1394 | `	if( b == 0 ){` |
|      6 | 1395 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1396 | `			"DivisionByZeroError",` |
|      - | 1397 | `			"Division by zero"` |
|      - | 1398 | `			);` |
|      - | 1399 | `	}` |
|      - | 1400 | `	/* Check for overflow: PHP_INT_MIN / -1 */` |
|    150 | 1401 | `	if( a == SMALLEST_INT64 && b == -1 ){` |
|      3 | 1402 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1403 | `			"ArithmeticError",` |
|      - | 1404 | `			"Division of PHP_INT_MIN by -1 is not an integer"` |
|      - | 1405 | `			);` |
|      - | 1406 | `	}` |
|      - | 1407 | `	/* Perform integer division */` |
|    147 | 1408 | `	ph7_result_int64(pCtx, a / b);` |
|    147 | 1409 | `	return PH7_OK;` |
|     79 | 1410 | `}` |
|      - | 1411 | `/*` |
|      - | 1412 | ` * string dechex(int $number)` |
|      - | 1413 | ` *  Decimal to hexadecimal.` |
|      - | 1414 | ` * Parameters` |
|      - | 1415 | ` *  $number` |
|      - | 1416 | ` *   Decimal value to convert` |
|      - | 1417 | ` * Return` |
|      - | 1418 | ` *  Hexadecimal string representation of number` |
|      - | 1419 | ` */` |
|     24 | 1420 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1421 | `{` |
|      - | 1422 | `	ph7_int64 iVal;` |
|     26 | 1423 | `	if( nArg < 1 ){` |
|      - | 1424 | `		/* Missing arguments,return null */` |
|    ! 0 | 1425 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1426 | `		return PH7_OK;` |
|      - | 1427 | `	}` |
|      - | 1428 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|     26 | 1429 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1430 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement,` |
|      - | 1431 | `	 * so a negative value prints all 16 hex digits like PHP. */` |
|     26 | 1432 | `	ph7_result_string_format(pCtx,"%qx",iVal);` |
|     26 | 1433 | `	return PH7_OK;` |
|     14 | 1434 | `}` |
|      - | 1435 | `/*` |
|      - | 1436 | ` * string decoct(int $number)` |
|      - | 1437 | ` *  Decimal to Octal.` |
|      - | 1438 | ` * Parameters` |
|      - | 1439 | ` *  $number` |
|      - | 1440 | ` *   Decimal value to convert` |
|      - | 1441 | ` * Return` |
|      - | 1442 | ` *  Octal string representation of number` |
|      - | 1443 | ` */` |
|     20 | 1444 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1445 | `{` |
|      - | 1446 | `	ph7_int64 iVal;` |
|     22 | 1447 | `	if( nArg < 1 ){` |
|      - | 1448 | `		/* Missing arguments,return null */` |
|    ! 0 | 1449 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1450 | `		return PH7_OK;` |
|      - | 1451 | `	}` |
|      - | 1452 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|     22 | 1453 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1454 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|     22 | 1455 | `	ph7_result_string_format(pCtx,"%qo",iVal);` |
|     22 | 1456 | `	return PH7_OK;` |
|     12 | 1457 | `}` |
|      - | 1458 | `/*` |
|      - | 1459 | ` * string decbin(int $number)` |
|      - | 1460 | ` *  Decimal to binary.` |
|      - | 1461 | ` * Parameters` |
|      - | 1462 | ` *  $number` |
|      - | 1463 | ` *   Decimal value to convert` |
|      - | 1464 | ` * Return` |
|      - | 1465 | ` *  Binary string representation of number` |
|      - | 1466 | ` */` |
|     30 | 1467 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1468 | `{` |
|      - | 1469 | `	ph7_int64 iVal;` |
|     31 | 1470 | `	if( nArg < 1 ){` |
|      - | 1471 | `		/* Missing arguments,return null */` |
|    ! 0 | 1472 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1473 | `		return PH7_OK;` |
|      - | 1474 | `	}` |
|      - | 1475 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|     31 | 1476 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1477 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|     31 | 1478 | `	ph7_result_string_format(pCtx,"%qB",iVal);` |
|     31 | 1479 | `	return PH7_OK;` |
|     16 | 1480 | `}` |
|      - | 1481 | `/*` |
|      - | 1482 | ` * The three bytes php's _php_math_basetozval accepts BEFORE it starts reading` |
|      - | 1483 | ` * digits, and that no program is deprecated for writing:` |
|      - | 1484 | ` *` |
|      - | 1485 | ` *   - leading and trailing WHITESPACE, trimmed off both ends (' ', '\t', '\n',` |
|      - | 1486 | ` *     '\r', '\v', '\f' — a NUL is not whitespace and stays an invalid digit);` |
|      - | 1487 | `` *   - the base's own PREFIX, when the base is one that has one: `0x`/`0X` for 16,`` |
|      - | 1488 | `` *     `0o`/`0O` for 8, `0b`/`0B` for 2. It is stripped only for the MATCHING base,`` |
|      - | 1489 | `` *     which is why hexdec("0o17") still deprecates its `o` and base_convert with`` |
|      - | 1490 | `` *     from_base 36 reads the `x` of "0x1f" as the digit 33.`` |
|      - | 1491 | ` *` |
|      - | 1492 | `` * Both were treated as invalid characters here, so `hexdec("0xff")` — the literal`` |
|      - | 1493 | ` * a program hands back to the engine after reading it out of source, and exactly` |
|      - | 1494 | ` * what nikic/php-parser passes — raised the ValueError the scope policy keeps for php's` |
|      - | 1495 | ` * DEPRECATED skipping instead of answering 255.` |
|      - | 1496 | ` *` |
|      - | 1497 | ` * On return the two out-parameters name the digit run; the caller decides what a` |
|      - | 1498 | ` * leftover non-digit means.` |
|      - | 1499 | ` */` |
|  37500 | 1500 | `static void MathBaseTrimPrefix(const char **pz,int *pn,int base)` |
|      4 | 1501 | `{` |
|  37504 | 1502 | `	const char *z = *pz;` |
|  37504 | 1503 | `	const char *zEnd = z + *pn;` |
|  56446 | 1504 | `	while( z < zEnd && (z[0]==' '\|\|z[0]=='\t'\|\|z[0]=='\n'\|\|z[0]=='\r'\|\|z[0]=='\v'\|\|z[0]=='\f') ){` |
|     17 | 1505 | `		z++;` |
|      1 | 1506 | `	}` |
|  56446 | 1507 | `	while( z < zEnd && (zEnd[-1]==' '\|\|zEnd[-1]=='\t'\|\|zEnd[-1]=='\n'` |
|  37495 | 1508 | `	                 \|\| zEnd[-1]=='\r'\|\|zEnd[-1]=='\v'\|\|zEnd[-1]=='\f') ){` |
|     17 | 1509 | `		zEnd--;` |
|      1 | 1510 | `	}` |
|  37504 | 1511 | `	if( zEnd - z >= 2 && z[0] == '0' ){` |
|   3739 | 1512 | `		int c = z[1];` |
|   3736 | 1513 | `		if( (base == 16 && (c=='x'\|\|c=='X'))` |
|   3725 | 1514 | `		 \|\| (base == 8  && (c=='o'\|\|c=='O'))` |
|   3661 | 1515 | `		 \|\| (base == 2  && (c=='b'\|\|c=='B')) ){` |
|    149 | 1516 | `			z += 2;` |
|    134 | 1517 | `		}` |
|   1808 | 1518 | `	}` |
|  37384 | 1519 | `	*pz = z;` |
|  37384 | 1520 | `	*pn = (int)(zEnd - z);` |
|  37384 | 1521 | `}` |
|      - | 1522 | `/*` |
|      - | 1523 | ` * Convert a base-2/8/16 digit string to a number, mirroring PHP's` |
|      - | 1524 | ` * _php_math_basetozval (ext/standard/math.c) so hexdec/octdec/bindec agree with` |
|      - | 1525 | ` * php byte-for-byte: trim the ends and the base prefix (above), then walk every` |
|      - | 1526 | ` * byte, decode a digit (0-9,a-z,A-Z) or skip any invalid one, accumulate into a` |
|      - | 1527 | ` * signed 64-bit integer and transparently promote to a double once the value` |
|      - | 1528 | ` * would overflow PHP_INT_MAX. The context result is set to an int when it fits,` |
|      - | 1529 | ` * otherwise a float — PHP returns a float for values above PHP_INT_MAX (e.g.` |
|      - | 1530 | ` * hexdec("ffffffffffffffff") == 1.8446744073709552E+19).` |
|      - | 1531 | ` * A byte >= 0x80 (e.g. a UTF-8 continuation) matches none of the digit ranges and` |
|      - | 1532 | ` * is skipped, so leading/interior multibyte junk is ignored like php.` |
|      - | 1533 | ` * Note: php also raises E_DEPRECATED for skipped invalid characters; that notice` |
|      - | 1534 | ` * is not emitted here (a deprecation-fidelity residual, value is correct).` |
|      - | 1535 | ` */` |
|  37438 | 1536 | `static void MathBaseToNumber(ph7_context *pCtx,const char *zStr,int nLen,int base)` |
|      4 | 1537 | `{` |
|  37442 | 1538 | `	sxi64 num = 0;      /* Integer accumulator */` |
|  37442 | 1539 | `	double fnum = 0;    /* Float accumulator (used once num would overflow) */` |
|  37442 | 1540 | `	int mode = 0;       /* 0 -> integer accumulation, 1 -> switched to float */` |
|  37442 | 1541 | `	sxi64 cutoff = SXI64_HIGH / base;      /* PHP_INT_MAX / base */` |
|  37442 | 1542 | `	int cutlim = (int)(SXI64_HIGH % base); /* PHP_INT_MAX % base */` |
|  37442 | 1543 | `	int bIgnored = 0;   /* any character skipped below? php deprecates that */` |
|      - | 1544 | `	int i;` |
|  37442 | 1545 | `	MathBaseTrimPrefix(&zStr,&nLen,base);` |
| 113556 | 1546 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  76118 | 1547 | `		int c = (unsigned char)zStr[i];` |
|  76118 | 1548 | `		if( c >= '0' && c <= '9' ){` |
|  59106 | 1549 | `			c -= '0';` |
|  46833 | 1550 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|      5 | 1551 | `			c -= 'A' - 10;` |
|  17014 | 1552 | `		}else if( c >= 'a' && c <= 'z' ){` |
|  17012 | 1553 | `			c -= 'a' - 10;` |
|   8594 | 1554 | `		}else{` |
|    ! 0 | 1555 | `			bIgnored = 1;` |
|    ! 0 | 1556 | `			continue; /* Not a digit character: skip */` |
|      - | 1557 | `		}` |
|  76118 | 1558 | `		if( c >= base ){` |
|     14 | 1559 | `			bIgnored = 1;` |
|     14 | 1560 | `			continue; /* Digit out of range for this base: skip */` |
|      - | 1561 | `		}` |
|  76106 | 1562 | `		if( mode == 0 ){` |
|  76106 | 1563 | `			if( num < cutoff \|\| (num == cutoff && c <= cutlim) ){` |
|  76098 | 1564 | `				num = num * base + c;` |
|  76098 | 1565 | `				continue;` |
|      - | 1566 | `			}` |
|      - | 1567 | `			/* Adding this digit would overflow the 64-bit integer: fall back to` |
|      - | 1568 | `			 * float accumulation, seeding it with the value gathered so far. */` |
|      9 | 1569 | `			fnum = (double)num;` |
|      9 | 1570 | `			mode = 1;` |
|      4 | 1571 | `		}` |
|      9 | 1572 | `		fnum = fnum * base + c;` |
|      5 | 1573 | `	}` |
|  37442 | 1574 | `	if( bIgnored ){` |
|      - | 1575 | `		/* php 8 skips characters that are not valid digits for this base and only` |
|      - | 1576 | `		 * DEPRECATES the skipping; the scope policy rejects the deprecated surface loudly, so this` |
|      - | 1577 | `		 * ValueError ABORTS the call (the result stored below never reaches the caller` |
|      - | 1578 | `		 * — the OP_CALL boundary reports the throw for us, VmHostFuncThrowRc).` |
|      - | 1579 | `		 * Twin-pinned by base_invalid_chars_abort{,_zend}.phpt. */` |
|     10 | 1580 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1581 | `			"Invalid characters passed for attempted conversion");` |
|     10 | 1582 | `		return;` |
|      - | 1583 | `	}` |
|  37434 | 1584 | `	if( mode == 1 ){` |
|      9 | 1585 | `		ph7_result_double(pCtx,fnum);` |
|      5 | 1586 | `	}else{` |
|  37426 | 1587 | `		ph7_result_int64(pCtx,num);` |
|      - | 1588 | `	}` |
|  18899 | 1589 | `}` |
|      - | 1590 | `/*` |
|      - | 1591 | ` * int64 hexdec(string $hex_string)` |
|      - | 1592 | ` *  Hexadecimal to decimal.` |
|      - | 1593 | ` * Parameters` |
|      - | 1594 | ` *  $hex_string` |
|      - | 1595 | ` *   The hexadecimal string to convert` |
|      - | 1596 | ` * Return` |
|      - | 1597 | ` *  The decimal representation of hex_string (int, or float on overflow)` |
|      - | 1598 | ` */` |
|  37246 | 1599 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1600 | `{` |
|      - | 1601 | `	const char *zString;` |
|      - | 1602 | `	int nLen;` |
|  37250 | 1603 | `	if( nArg < 1 ){` |
|      - | 1604 | `		/* Missing arguments,return -1 */` |
|    ! 0 | 1605 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1606 | `		return PH7_OK;` |
|      - | 1607 | `	}` |
|  37246 | 1608 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0])` |
|  37250 | 1609 | `	 \|\| PH7_ArgIsUnstringableObject(apArg[0]) ){` |
|      - | 1610 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|      - | 1611 | `		char zBuf[64];` |
|    ! 0 | 1612 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1613 | `			"hexdec(): Argument #1 ($hex_string) must be of type string, %s given",` |
|    ! 0 | 1614 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|      - | 1615 | `	}` |
|      - | 1616 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|      - | 1617 | `	 * hex-parses that (hexdec(255) == hexdec("255") == 0x255), so route every` |
|      - | 1618 | `	 * non-throwing value through ph7_value_to_string rather than reading it as` |
|      - | 1619 | `	 * a decimal integer. */` |
|  37250 | 1620 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  37250 | 1621 | `	MathBaseToNumber(pCtx,zString,nLen,16);` |
|  37250 | 1622 | `	return PH7_OK;` |
|  18803 | 1623 | `}` |
|      - | 1624 | `/*` |
|      - | 1625 | ` * int64 bindec(string $bin_string)` |
|      - | 1626 | ` *  Binary to decimal.` |
|      - | 1627 | ` * Parameters` |
|      - | 1628 | ` *  $bin_string` |
|      - | 1629 | ` *   The binary string to convert` |
|      - | 1630 | ` * Return` |
|      - | 1631 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|      - | 1632 | ` */` |
|    162 | 1633 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1634 | `{` |
|      - | 1635 | `	const char *zString;` |
|      - | 1636 | `	int nLen;` |
|    164 | 1637 | `	if( nArg < 1 ){` |
|      - | 1638 | `		/* Missing arguments,return -1 */` |
|    ! 0 | 1639 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1640 | `		return PH7_OK;` |
|      - | 1641 | `	}` |
|    162 | 1642 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0])` |
|    164 | 1643 | `	 \|\| PH7_ArgIsUnstringableObject(apArg[0]) ){` |
|      - | 1644 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|      - | 1645 | `		char zBuf[64];` |
|    ! 0 | 1646 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1647 | `			"bindec(): Argument #1 ($binary_string) must be of type string, %s given",` |
|    ! 0 | 1648 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|      - | 1649 | `	}` |
|      - | 1650 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|      - | 1651 | `	 * binary-parses that (bindec(11) == bindec("11") == 3). */` |
|    164 | 1652 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    164 | 1653 | `	MathBaseToNumber(pCtx,zString,nLen,2);` |
|    164 | 1654 | `	return PH7_OK;` |
|     83 | 1655 | `}` |
|      - | 1656 | `/*` |
|      - | 1657 | ` * int64 octdec(string $oct_string)` |
|      - | 1658 | ` *  Octal to decimal.` |
|      - | 1659 | ` * Parameters` |
|      - | 1660 | ` *  $oct_string` |
|      - | 1661 | ` *   The octal string to convert` |
|      - | 1662 | ` * Return` |
|      - | 1663 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|      - | 1664 | ` */` |
|     30 | 1665 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1666 | `{` |
|      - | 1667 | `	const char *zString;` |
|      - | 1668 | `	int nLen;` |
|     32 | 1669 | `	if( nArg < 1 ){` |
|      - | 1670 | `		/* Missing arguments,return -1 */` |
|    ! 0 | 1671 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1672 | `		return PH7_OK;` |
|      - | 1673 | `	}` |
|     30 | 1674 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0])` |
|     32 | 1675 | `	 \|\| PH7_ArgIsUnstringableObject(apArg[0]) ){` |
|      - | 1676 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|      - | 1677 | `		char zBuf[64];` |
|    ! 0 | 1678 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1679 | `			"octdec(): Argument #1 ($octal_string) must be of type string, %s given",` |
|    ! 0 | 1680 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|      - | 1681 | `	}` |
|      - | 1682 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|      - | 1683 | `	 * octal-parses that (octdec(11) == octdec("11") == 9). */` |
|     32 | 1684 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     32 | 1685 | `	MathBaseToNumber(pCtx,zString,nLen,8);` |
|     32 | 1686 | `	return PH7_OK;` |
|     17 | 1687 | `}` |
|      - | 1688 | `/*` |
|      - | 1689 | ` * srand([int $seed])` |
|      - | 1690 | ` * mt_srand([int $seed])` |
|      - | 1691 | ` *  Seed the random number generator.` |
|      - | 1692 | ` * Parameters` |
|      - | 1693 | ` * $seed` |
|      - | 1694 | ` *  Optional seed value. php truncates it to 32 bits; a missing seed reseeds` |
|      - | 1695 | ` *  from OS entropy (a "random" seed), matching php's GENERATE_SEED().` |
|      - | 1696 | ` * Return` |
|      - | 1697 | ` *  null.` |
|      - | 1698 | ` * Note:` |
|      - | 1699 | ` *  srand()/mt_srand() are aliases (php 7.1+ backs both rand() and mt_rand()` |
|      - | 1700 | ` *  with the same MT19937). They reset only the userland generator, never the` |
|      - | 1701 | ` *  engine's internal RC4 entropy, so a seed makes rand()/mt_rand()/shuffle/` |
|      - | 1702 | ` *  str_shuffle/array_rand reproducible without disturbing object ids or uniqid.` |
|      - | 1703 | ` */` |
|    314 | 1704 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1705 | `{` |
|      - | 1706 | `	sxu32 nSeed;` |
|    317 | 1707 | `	int bLegacy = nArg > 1 && ph7_value_to_int64(apArg[1]) == PH7_MT_RAND_PHP;` |
|    317 | 1708 | `	if( bLegacy ){` |
|      - | 1709 | `		/* php 8.3 deprecated the legacy generator; the message carries no` |
|      - | 1710 | `		 * function prefix there. */` |
|     23 | 1711 | `		PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|      - | 1712 | `			"The MT_RAND_PHP variant of Mt19937 is deprecated");` |
|     11 | 1713 | `	}` |
|    317 | 1714 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|      - | 1715 | `		/* php truncates the (weakly int-coerced) seed to 32 bits. */` |
|    311 | 1716 | `		nSeed = (sxu32)ph7_value_to_int64(apArg[0]);` |
|    157 | 1717 | `	}else{` |
|      - | 1718 | `		/* NULL is the declared default and means "no seed given": php reseeds` |
|      - | 1719 | `		 * from entropy for it, where this read it as the integer 0 — so` |
|      - | 1720 | ``		 * `mt_srand($cfg['seed'] ?? null)` pinned every run to one sequence. */`` |
|      - | 1721 | `		/* No seed: reseed from OS entropy, like php's GENERATE_SEED(). */` |
|      8 | 1722 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|    ! 0 | 1723 | `			nSeed = PH7_VmRandomNum(pCtx->pVm);` |
|    ! 0 | 1724 | `		}` |
|      - | 1725 | `	}` |
|      - | 1726 | `	/* $mode picks the GENERATOR, and php reads it as an equality test against` |
|      - | 1727 | `	 * MT_RAND_PHP alone: every other value, valid or not, is MT19937. It was` |
|      - | 1728 | `	 * declared in the signature and read by nothing, so a program that seeded` |
|      - | 1729 | `	 * with MT_RAND_PHP to reproduce a recorded sequence silently got a different` |
|      - | 1730 | `	 * one — and the constant naming it was undefined, so the call was a fatal. */` |
|    317 | 1731 | `	PH7_VmMtSrand(pCtx->pVm,nSeed,bLegacy);` |
|    317 | 1732 | `	ph7_result_null(pCtx);` |
|    317 | 1733 | `	return PH7_OK;` |
|      3 | 1734 | `}` |
|      - | 1735 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 1736 | `/*` |
|      - | 1737 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|      - | 1738 | ` *  Convert a number between arbitrary bases.` |
|      - | 1739 | ` * Parameters` |
|      - | 1740 | ` * $number` |
|      - | 1741 | ` *  The number to convert` |
|      - | 1742 | ` * $frombase` |
|      - | 1743 | ` *  The base number is in` |
|      - | 1744 | ` * $tobase` |
|      - | 1745 | ` *  The base to convert number to` |
|      - | 1746 | ` * Return` |
|      - | 1747 | ` *  Number converted to base tobase` |
|      - | 1748 | ` */` |
|     72 | 1749 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1750 | `{` |
|      - | 1751 | `	static const char zDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";` |
|      - | 1752 | `	int nLen,iFbase,iTobase,i;` |
|      - | 1753 | `	int bIgnored;` |
|      - | 1754 | `	ph7_int64 iFbase64,iTobase64;` |
|      - | 1755 | `	const char *zNum;` |
|     74 | 1756 | `	sxu64 uNum = 0;` |
|     74 | 1757 | `	if( nArg < 3 ){` |
|      - | 1758 | `		/* Return the empty string*/` |
|    ! 0 | 1759 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1760 | `		return PH7_OK;` |
|      - | 1761 | `	}` |
|      - | 1762 | `	/* Base numbers. Read them as 64-bit so an out-of-range base can't wrap through` |
|      - | 1763 | `	 * a 32-bit truncation back into the 2..36 window and bypass the check below. */` |
|     74 | 1764 | `	iFbase64 = ph7_value_to_int64(apArg[1]);` |
|     74 | 1765 | `	iTobase64 = ph7_value_to_int64(apArg[2]);` |
|      - | 1766 | `	/* PHP 8 throws a catchable ValueError for a base outside 2..36; from_base` |
|      - | 1767 | `	 * is validated before to_base, both before the string is even parsed. */` |
|     74 | 1768 | `	if( iFbase64 < 2 \|\| iFbase64 > 36 ){` |
|      7 | 1769 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1770 | `			"base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)");` |
|      - | 1771 | `	}` |
|     68 | 1772 | `	if( iTobase64 < 2 \|\| iTobase64 > 36 ){` |
|      5 | 1773 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1774 | `			"base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)");` |
|      - | 1775 | `	}` |
|      - | 1776 | `	/* Both bases are now known to fit in [2,36], so the int form is exact. */` |
|     64 | 1777 | `	iFbase  = (int)iFbase64;` |
|     64 | 1778 | `	iTobase = (int)iTobase64;` |
|      - | 1779 | `	/* Parse the input number in from_base. Every base is handled the same way:` |
|      - | 1780 | `	 * digits 0-9 then a-z/A-Z map to 0-35; a character that is not a valid digit for` |
|      - | 1781 | `	 * from_base is ignored, and php raises an E_DEPRECATED saying so. */` |
|     64 | 1782 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 1783 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1784 | `			"base_convert(): Argument #1 ($num) must be of type string, null given");` |
|      - | 1785 | `	}` |
|     64 | 1786 | `	zNum = ph7_value_to_string(apArg[0],&nLen);` |
|     64 | 1787 | `	bIgnored = 0;` |
|     64 | 1788 | `	MathBaseTrimPrefix(&zNum,&nLen,iFbase);` |
|    206 | 1789 | `	for( i = 0 ; i < nLen ; ++i ){` |
|    144 | 1790 | `		int c = (unsigned char)zNum[i];` |
|      - | 1791 | `		int d;` |
|    144 | 1792 | `		if( c >= '0' && c <= '9' ){` |
|    104 | 1793 | `			d = c - '0';` |
|     93 | 1794 | `		}else if( c >= 'a' && c <= 'z' ){` |
|     42 | 1795 | `			d = c - 'a' + 10;` |
|     20 | 1796 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|    ! 0 | 1797 | `			d = c - 'A' + 10;` |
|    ! 0 | 1798 | `		}else{` |
|    ! 0 | 1799 | `			d = 99;` |
|      - | 1800 | `		}` |
|    144 | 1801 | `		if( d >= iFbase ){` |
|      - | 1802 | `			/* Not a valid digit for this base: php skips it and deprecates the skip. */` |
|      6 | 1803 | `			bIgnored = 1;` |
|      6 | 1804 | `			continue;` |
|      - | 1805 | `		}` |
|    140 | 1806 | `		uNum = uNum * (sxu64)iFbase + (sxu64)d;` |
|     71 | 1807 | `	}` |
|     64 | 1808 | `	if( bIgnored ){` |
|      - | 1809 | `		/* the scope policy rejects php's deprecated surface loudly, and a throw ABORTS the call —` |
|      - | 1810 | `		 * the conversion below is not reached. See MathBaseToNumber's twin. */` |
|      6 | 1811 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1812 | `			"Invalid characters passed for attempted conversion");` |
|      - | 1813 | `	}` |
|      - | 1814 | `	/* Format the result in to_base using lowercase digits. */` |
|     60 | 1815 | `	if( uNum == 0 ){` |
|      5 | 1816 | `		ph7_result_string(pCtx,"0",1);` |
|      3 | 1817 | `	}else{` |
|      - | 1818 | `		char zOut[70]; /* base-2 of a 64-bit value fits in 64 digits */` |
|     56 | 1819 | `		int n = 0,j;` |
|    188 | 1820 | `		while( uNum > 0 ){` |
|    134 | 1821 | `			zOut[n++] = zDigits[uNum % (sxu64)iTobase];` |
|    134 | 1822 | `			uNum /= (sxu64)iTobase;` |
|      2 | 1823 | `		}` |
|      - | 1824 | `		/* Digits were produced least-significant first: reverse in place. */` |
|    110 | 1825 | `		for( j = 0 ; j < n/2 ; ++j ){` |
|     56 | 1826 | `			char t = zOut[j];` |
|     56 | 1827 | `			zOut[j] = zOut[n - 1 - j];` |
|     56 | 1828 | `			zOut[n - 1 - j] = t;` |
|     29 | 1829 | `		}` |
|     56 | 1830 | `		ph7_result_string(pCtx,zOut,n);` |
|      - | 1831 | `	}` |
|     60 | 1832 | `	return PH7_OK;` |
|     38 | 1833 | `}` |
|      - | 1834 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 1835 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1836 |  |
