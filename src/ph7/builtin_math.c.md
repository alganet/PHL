# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 707/838 lines (84.37%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `/*` |
|     - |    8 | ` * round() (defined below, under PH7_DISABLE_BUILTIN_FUNC rather than the` |
|     - |    9 | ` * math-func guard) needs floor/ceil/fabs/copysign/fmod/isfinite/pow plus the` |
|     - |   10 | ` * libc snprintf/strtod round-trip for its high-precision branch, so pull these` |
|     - |   11 | ` * in unconditionally here — they must be available even when` |
|     - |   12 | ` * PH7_ENABLE_MATH_FUNC is off. abs() is also used by the guarded math builtins.` |
|     - |   13 | ` */` |
|     - |   14 | `#include <math.h>` |
|     - |   15 | `#include <stdio.h>  /* snprintf: correctly-rounded high-precision round() round-trip */` |
|     - |   16 | `#include <stdlib.h> /* strtod (round-trip inverse), abs */` |
|     - |   17 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|     - |   18 |  |
|     - |   19 | `/*` |
|     - |   20 | ` * Section:` |
|     - |   21 | ` *    Math Functions.` |
|     - |   22 |  |
|     - |   23 | ` * Status:` |
|     - |   24 | ` *    Stable.` |
|     - |   25 | ` */` |
|     - |   26 | `/*` |
|     - |   27 | ` * float sqrt(float $arg )` |
|     - |   28 | ` *  Square root of the given number.` |
|     - |   29 | ` * Parameter` |
|     - |   30 | ` *  The number to process.` |
|     - |   31 | ` * Return` |
|     - |   32 | ` *  The square root of arg or the special value Nan of failure.` |
|     - |   33 | ` */` |
|     - |   34 | `/*` |
|     - |   35 | ` * The libm-backed float functions php has and PH7 lacked entirely. Doing these in PHP` |
|     - |   36 | ` * (log1p as log(1+x), acosh via logs, ...) would lose precision, so they go through libm.` |
|     - |   37 | ` */` |
|     - |   38 | `#define PH7_MATH_UNARY(NAME,CFUNC)                                        \` |
|     - |   39 | `PH7_PRIVATE int PH7_builtin_##NAME(ph7_context *pCtx,int nArg,ph7_value **apArg) \` |
|     - |   40 | `{                                                                          \` |
|     - |   41 | `	double x;                                                              \` |
|     - |   42 | `	if( nArg < 1 ){                                                        \` |
|     - |   43 | `		ph7_result_int(pCtx,0);                                            \` |
|     - |   44 | `		return PH7_OK;                                                     \` |
|     - |   45 | `	}                                                                      \` |
|     - |   46 | `	x = ph7_value_to_double(apArg[0]);                                     \` |
|     - |   47 | `	ph7_result_double(pCtx,CFUNC(x));                                      \` |
|     - |   48 | `	return PH7_OK;                                                          \` |
|     - |   49 | `}` |
|     3 |   50 | `PH7_MATH_UNARY(acosh,acosh)` |
|     3 |   51 | `PH7_MATH_UNARY(asinh,asinh)` |
|     3 |   52 | `PH7_MATH_UNARY(atanh,atanh)` |
|     3 |   53 | `PH7_MATH_UNARY(expm1,expm1)` |
|     3 |   54 | `PH7_MATH_UNARY(log1p,log1p)` |
|     2 |   55 | `PH7_PRIVATE int PH7_builtin_deg2rad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   56 | `{` |
|     - |   57 | `	double x;` |
|     3 |   58 | `	if( nArg < 1 ){` |
|   ! 0 |   59 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |   60 | `		return PH7_OK;` |
|     - |   61 | `	}` |
|     3 |   62 | `	x = ph7_value_to_double(apArg[0]);` |
|     3 |   63 | `	ph7_result_double(pCtx,x * (3.14159265358979323846 / 180.0));` |
|     3 |   64 | `	return PH7_OK;` |
|     2 |   65 | `}` |
|     6 |   66 | `PH7_PRIVATE int PH7_builtin_rad2deg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   67 | `{` |
|     - |   68 | `	double x;` |
|     8 |   69 | `	if( nArg < 1 ){` |
|   ! 0 |   70 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |   71 | `		return PH7_OK;` |
|     - |   72 | `	}` |
|     8 |   73 | `	x = ph7_value_to_double(apArg[0]);` |
|     8 |   74 | `	ph7_result_double(pCtx,x * (180.0 / 3.14159265358979323846));` |
|     8 |   75 | `	return PH7_OK;` |
|     5 |   76 | `}` |
|     4 |   77 | `PH7_PRIVATE int PH7_builtin_fpow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |   78 | `{` |
|     - |   79 | `	double x,y;` |
|     6 |   80 | `	if( nArg < 2 ){` |
|   ! 0 |   81 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |   82 | `		return PH7_OK;` |
|     - |   83 | `	}` |
|     6 |   84 | `	x = ph7_value_to_double(apArg[0]);` |
|     6 |   85 | `	y = ph7_value_to_double(apArg[1]);` |
|     6 |   86 | `	ph7_result_double(pCtx,pow(x,y));` |
|     6 |   87 | `	return PH7_OK;` |
|     4 |   88 | `}` |
|    16 |   89 | `PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |   90 | `{` |
|     - |   91 | `	double r,x;` |
|    17 |   92 | `	if( nArg < 1 ){` |
|     - |   93 | `		/* Missing argument,return 0 */` |
|   ! 0 |   94 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |   95 | `		return PH7_OK;` |
|     - |   96 | `	}` |
|    17 |   97 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |   98 | `	/* Perform the requested operation */` |
|    17 |   99 | `	r = sqrt(x);` |
|     - |  100 | `	/* store the result back */` |
|    17 |  101 | `	ph7_result_double(pCtx,r);` |
|    17 |  102 | `	return PH7_OK;` |
|     9 |  103 | `}` |
|     - |  104 | `/*` |
|     - |  105 | ` * float exp(float $arg )` |
|     - |  106 | ` *  Calculates the exponent of e.` |
|     - |  107 | ` * Parameter` |
|     - |  108 | ` *  The number to process.` |
|     - |  109 | ` * Return` |
|     - |  110 | ` *  'e' raised to the power of arg.` |
|     - |  111 | ` */` |
|    18 |  112 | `PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  113 | `{` |
|     - |  114 | `	double r,x;` |
|    19 |  115 | `	if( nArg < 1 ){` |
|     - |  116 | `		/* Missing argument,return 0 */` |
|   ! 0 |  117 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  118 | `		return PH7_OK;` |
|     - |  119 | `	}` |
|    19 |  120 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  121 | `	/* Perform the requested operation */` |
|    19 |  122 | `	r = exp(x);` |
|     - |  123 | `	/* store the result back */` |
|    19 |  124 | `	ph7_result_double(pCtx,r);` |
|    19 |  125 | `	return PH7_OK;` |
|    10 |  126 | `}` |
|     - |  127 | `/*` |
|     - |  128 | ` * float floor(float $arg )` |
|     - |  129 | ` *  Round fractions down.` |
|     - |  130 | ` * Parameter` |
|     - |  131 | ` *  The number to process.` |
|     - |  132 | ` * Return` |
|     - |  133 | ` *  Returns the next lowest integer value by rounding down value if necessary.` |
|     - |  134 | ` */` |
|    22 |  135 | `PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  136 | `{` |
|     - |  137 | `	double r,x;` |
|     - |  138 | `	/* PHP requires exactly one argument. */` |
|    23 |  139 | `	if( nArg != 1 ){` |
|   ! 0 |  140 | `		return PH7_VmThrowException(pCtx,` |
|     - |  141 | `			"ArgumentCountError",` |
|     - |  142 | `			"floor() expects exactly 1 argument, %d given",` |
|   ! 0 |  143 | `			nArg` |
|     - |  144 | `			);` |
|     - |  145 | `	}` |
|     - |  146 | ``	/* The `int\|float $num` row in aBuiltinSig[] screens this argument before the`` |
|     - |  147 | `	 * call: array, object, resource and non-numeric string are refused there,` |
|     - |  148 | `	 * with php's wording. The hand-rolled copy that used to sit here refused a` |
|     - |  149 | `	 * BOOL as well, which weak mode converts (php: ceil(true) is float(1)), and` |
|     - |  150 | `	 * one of its two branches had no ", %s given" tail at all. */` |
|     - |  151 |  |
|    23 |  152 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  153 | `	/* Perform the requested operation */` |
|    23 |  154 | `	r = floor(x);` |
|     - |  155 | `	/* store the result back */` |
|    23 |  156 | `	ph7_result_double(pCtx,r);` |
|    23 |  157 | `	return PH7_OK;` |
|    12 |  158 | `}` |
|     - |  159 | `/*` |
|     - |  160 | ` * float cos(float $arg )` |
|     - |  161 | ` *  Cosine.` |
|     - |  162 | ` * Parameter` |
|     - |  163 | ` *  The number to process.` |
|     - |  164 | ` * Return` |
|     - |  165 | ` *  The cosine of arg.` |
|     - |  166 | ` */` |
|     2 |  167 | `PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  168 | `{` |
|     - |  169 | `	double r,x;` |
|     3 |  170 | `	if( nArg < 1 ){` |
|     - |  171 | `		/* Missing argument,return 0 */` |
|   ! 0 |  172 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  173 | `		return PH7_OK;` |
|     - |  174 | `	}` |
|     3 |  175 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  176 | `	/* Perform the requested operation */` |
|     3 |  177 | `	r = cos(x);` |
|     - |  178 | `	/* store the result back */` |
|     3 |  179 | `	ph7_result_double(pCtx,r);` |
|     3 |  180 | `	return PH7_OK;` |
|     2 |  181 | `}` |
|     - |  182 | `/*` |
|     - |  183 | ` * float acos(float $arg )` |
|     - |  184 | ` *  Arc cosine.` |
|     - |  185 | ` * Parameter` |
|     - |  186 | ` *  The number to process.` |
|     - |  187 | ` * Return` |
|     - |  188 | ` *  The arc cosine of arg.` |
|     - |  189 | ` */` |
|    16 |  190 | `PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  191 | `{` |
|     - |  192 | `	double r, x;` |
|     - |  193 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|    17 |  194 | `	if( nArg != 1 ){` |
|   ! 0 |  195 | `		return PH7_VmThrowException(pCtx,` |
|     - |  196 | `			"ArgumentCountError",` |
|     - |  197 | `			"acos() expects exactly 1 argument, %d given",` |
|   ! 0 |  198 | `			nArg` |
|     - |  199 | `			);` |
|     - |  200 | `	}` |
|     - |  201 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|     - |  202 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|     - |  203 | `	 * the float conversion will handle them. */` |
|    17 |  204 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|   ! 0 |  205 | `		return PH7_VmThrowException(pCtx,` |
|     - |  206 | `			"TypeError",` |
|     - |  207 | `			"acos(): Argument #1 ($num) must be of type float, %s given",` |
|   ! 0 |  208 | `			ph7_type_name(apArg[0])` |
|     - |  209 | `			);` |
|     - |  210 | `	}` |
|     - |  211 | `	/* Convert to double now that we know it's numeric. */` |
|    17 |  212 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  213 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|    17 |  214 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|     5 |  215 | `		r = PH7_NAN_VALUE();` |
|     3 |  216 | `	}else{` |
|    13 |  217 | `		r = acos(x);` |
|     - |  218 | `	}` |
|     - |  219 | `	/* store the result back */` |
|    17 |  220 | `	ph7_result_double(pCtx,r);` |
|    17 |  221 | `	return PH7_OK;` |
|     9 |  222 | `}` |
|     - |  223 | `/*` |
|     - |  224 | ` * float cosh(float $arg )` |
|     - |  225 | ` *  Hyperbolic cosine.` |
|     - |  226 | ` * Parameter` |
|     - |  227 | ` *  The number to process.` |
|     - |  228 | ` * Return` |
|     - |  229 | ` *  The hyperbolic cosine of arg.` |
|     - |  230 | ` */` |
|    16 |  231 | `PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  232 | `{` |
|     - |  233 | `	double r,x;` |
|    17 |  234 | `	if( nArg < 1 ){` |
|     - |  235 | `		/* Missing argument,return 0 */` |
|   ! 0 |  236 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  237 | `		return PH7_OK;` |
|     - |  238 | `	}` |
|    17 |  239 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  240 | `	/* Perform the requested operation */` |
|    17 |  241 | `	r = cosh(x);` |
|     - |  242 | `	/* store the result back */` |
|    17 |  243 | `	ph7_result_double(pCtx,r);` |
|    17 |  244 | `	return PH7_OK;` |
|     9 |  245 | `}` |
|     - |  246 | `/*` |
|     - |  247 | ` * float sin(float $arg )` |
|     - |  248 | ` *  Sine.` |
|     - |  249 | ` * Parameter` |
|     - |  250 | ` *  The number to process.` |
|     - |  251 | ` * Return` |
|     - |  252 | ` *  The sine of arg.` |
|     - |  253 | ` */` |
|     2 |  254 | `PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  255 | `{` |
|     - |  256 | `	double r,x;` |
|     3 |  257 | `	if( nArg < 1 ){` |
|     - |  258 | `		/* Missing argument,return 0 */` |
|   ! 0 |  259 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  260 | `		return PH7_OK;` |
|     - |  261 | `	}` |
|     3 |  262 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  263 | `	/* Perform the requested operation */` |
|     3 |  264 | `	r = sin(x);` |
|     - |  265 | `	/* store the result back */` |
|     3 |  266 | `	ph7_result_double(pCtx,r);` |
|     3 |  267 | `	return PH7_OK;` |
|     2 |  268 | `}` |
|     - |  269 | `/*` |
|     - |  270 | ` * float asin(float $arg )` |
|     - |  271 | ` *  Arc sine.` |
|     - |  272 | ` * Parameter` |
|     - |  273 | ` *  The number to process.` |
|     - |  274 | ` * Return` |
|     - |  275 | ` *  The arc sine of arg.` |
|     - |  276 | ` */` |
|    16 |  277 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  278 | `{` |
|     - |  279 | `	double r, x;` |
|     - |  280 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|    17 |  281 | `	if( nArg != 1 ){` |
|   ! 0 |  282 | `		return PH7_VmThrowException(pCtx,` |
|     - |  283 | `			"ArgumentCountError",` |
|     - |  284 | `			"asin() expects exactly 1 argument, %d given",` |
|   ! 0 |  285 | `			nArg` |
|     - |  286 | `			);` |
|     - |  287 | `	}` |
|     - |  288 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|     - |  289 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|     - |  290 | `	 * the float conversion will handle them. */` |
|    17 |  291 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|   ! 0 |  292 | `		return PH7_VmThrowException(pCtx,` |
|     - |  293 | `			"TypeError",` |
|     - |  294 | `			"asin(): Argument #1 ($num) must be of type float, %s given",` |
|   ! 0 |  295 | `			ph7_type_name(apArg[0])` |
|     - |  296 | `			);` |
|     - |  297 | `	}` |
|     - |  298 | `	/* Convert to double now that we know it's numeric. */` |
|    17 |  299 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  300 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|    17 |  301 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|     5 |  302 | `		r = PH7_NAN_VALUE();` |
|     3 |  303 | `	}else{` |
|    13 |  304 | `		r = asin(x);` |
|     - |  305 | `	}` |
|     - |  306 | `	/* store the result back */` |
|    17 |  307 | `	ph7_result_double(pCtx,r);` |
|    17 |  308 | `	return PH7_OK;` |
|     9 |  309 | `}` |
|     - |  310 | `/*` |
|     - |  311 | ` * float sinh(float $arg )` |
|     - |  312 | ` *  Hyperbolic sine.` |
|     - |  313 | ` * Parameter` |
|     - |  314 | ` *  The number to process.` |
|     - |  315 | ` * Return` |
|     - |  316 | ` *  The hyperbolic sine of arg.` |
|     - |  317 | ` */` |
|    18 |  318 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  319 | `{` |
|     - |  320 | `	double r,x;` |
|    19 |  321 | `	if( nArg < 1 ){` |
|     - |  322 | `		/* Missing argument,return 0 */` |
|   ! 0 |  323 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  324 | `		return PH7_OK;` |
|     - |  325 | `	}` |
|    19 |  326 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  327 | `	/* Perform the requested operation */` |
|    19 |  328 | `	r = sinh(x);` |
|     - |  329 | `	/* store the result back */` |
|    19 |  330 | `	ph7_result_double(pCtx,r);` |
|    19 |  331 | `	return PH7_OK;` |
|    10 |  332 | `}` |
|     - |  333 | `/*` |
|     - |  334 | ` * float ceil(float $arg )` |
|     - |  335 | ` *  Round fractions up.` |
|     - |  336 | ` * Parameter` |
|     - |  337 | ` *  The number to process.` |
|     - |  338 | ` * Return` |
|     - |  339 | ` *  The next highest integer value by rounding up value if necessary.` |
|     - |  340 | ` */` |
|    18 |  341 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  342 | `{` |
|     - |  343 | `	double r,x;` |
|     - |  344 | `	/* PHP requires exactly one argument. */` |
|    19 |  345 | `	if( nArg != 1 ){` |
|   ! 0 |  346 | `		return PH7_VmThrowException(pCtx,` |
|     - |  347 | `			"ArgumentCountError",` |
|     - |  348 | `			"ceil() expects exactly 1 argument, %d given",` |
|   ! 0 |  349 | `			nArg` |
|     - |  350 | `			);` |
|     - |  351 | `	}` |
|     - |  352 | `	/* Type screening is the aBuiltinSig[] row's -- see floor() above. */` |
|     - |  353 |  |
|    19 |  354 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  355 | `	/* Perform the requested operation */` |
|    19 |  356 | `	r = ceil(x);` |
|     - |  357 | `	/* store the result back */` |
|    19 |  358 | `	ph7_result_double(pCtx,r);` |
|    19 |  359 | `	return PH7_OK;` |
|    10 |  360 | `}` |
|     - |  361 | `/*` |
|     - |  362 | ` * float tan(float $arg )` |
|     - |  363 | ` *  Tangent.` |
|     - |  364 | ` * Parameter` |
|     - |  365 | ` *  The number to process.` |
|     - |  366 | ` * Return` |
|     - |  367 | ` *  The tangent of arg.` |
|     - |  368 | ` */` |
|     4 |  369 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  370 | `{` |
|     - |  371 | `	double r,x;` |
|     5 |  372 | `	if( nArg < 1 ){` |
|     - |  373 | `		/* Missing argument,return 0 */` |
|   ! 0 |  374 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  375 | `		return PH7_OK;` |
|     - |  376 | `	}` |
|     5 |  377 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  378 | `	/* Perform the requested operation */` |
|     5 |  379 | `	r = tan(x);` |
|     - |  380 | `	/* store the result back */` |
|     5 |  381 | `	ph7_result_double(pCtx,r);` |
|     5 |  382 | `	return PH7_OK;` |
|     3 |  383 | `}` |
|     - |  384 | `/*` |
|     - |  385 | ` * float atan(float $arg )` |
|     - |  386 | ` *  Arc tangent.` |
|     - |  387 | ` * Parameter` |
|     - |  388 | ` *  The number to process.` |
|     - |  389 | ` * Return` |
|     - |  390 | ` *  The arc tangent of arg.` |
|     - |  391 | ` */` |
|    32 |  392 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  393 | `{` |
|     - |  394 | `	double r,x;` |
|     - |  395 | `	/* PHP enforces exactly one argument. */` |
|    33 |  396 | `	if( nArg != 1 ){` |
|   ! 0 |  397 | `		return PH7_VmThrowException(pCtx,` |
|     - |  398 | `			"ArgumentCountError",` |
|     - |  399 | `			"atan() expects exactly 1 argument, %d given",` |
|   ! 0 |  400 | `			nArg` |
|     - |  401 | `			);` |
|     - |  402 | `	}` |
|     - |  403 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, non-numeric strings).` |
|     - |  404 | `	 * PHP 8 reports a TypeError for wrong types. */` |
|    33 |  405 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|   ! 0 |  406 | `		return PH7_VmThrowException(pCtx,` |
|     - |  407 | `			"TypeError",` |
|     - |  408 | `			"atan(): Argument #1 ($num) must be of type float, %s given",` |
|   ! 0 |  409 | `			ph7_type_name(apArg[0])` |
|     - |  410 | `			);` |
|     - |  411 | `	}` |
|    33 |  412 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  413 | `	/* Perform the requested operation */` |
|    33 |  414 | `	r = atan(x);` |
|     - |  415 | `	/* store the result back */` |
|    33 |  416 | `	ph7_result_double(pCtx,r);` |
|    33 |  417 | `	return PH7_OK;` |
|    17 |  418 | `}` |
|     - |  419 | `/*` |
|     - |  420 | ` * float tanh(float $arg )` |
|     - |  421 | ` *  Hyperbolic tangent.` |
|     - |  422 | ` * Parameter` |
|     - |  423 | ` *  The number to process.` |
|     - |  424 | ` * Return` |
|     - |  425 | ` *  The Hyperbolic tangent of arg.` |
|     - |  426 | ` */` |
|    18 |  427 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  428 | `{` |
|     - |  429 | `	double r,x;` |
|    19 |  430 | `	if( nArg < 1 ){` |
|     - |  431 | `		/* Missing argument,return 0 */` |
|   ! 0 |  432 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  433 | `		return PH7_OK;` |
|     - |  434 | `	}` |
|    19 |  435 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  436 | `	/* Perform the requested operation */` |
|    19 |  437 | `	r = tanh(x);` |
|     - |  438 | `	/* store the result back */` |
|    19 |  439 | `	ph7_result_double(pCtx,r);` |
|    19 |  440 | `	return PH7_OK;` |
|    10 |  441 | `}` |
|     - |  442 | `/*` |
|     - |  443 | ` * float atan2(float $y,float $x)` |
|     - |  444 | ` *  Arc tangent of two variable.` |
|     - |  445 | ` * Parameter` |
|     - |  446 | ` *  $y = Dividend parameter.` |
|     - |  447 | ` *  $x = Divisor parameter.` |
|     - |  448 | ` * Return` |
|     - |  449 | ` *  The arc tangent of y/x in radian.` |
|     - |  450 | ` */` |
|    46 |  451 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  452 | `{` |
|     - |  453 | `	double r,x,y;` |
|     - |  454 | `	/* PHP enforces exactly two arguments. */` |
|    47 |  455 | `	if( nArg != 2 ){` |
|   ! 0 |  456 | `		return PH7_VmThrowException(pCtx,` |
|     - |  457 | `			"ArgumentCountError",` |
|     - |  458 | `			"atan2() expects exactly 2 arguments, %d given",` |
|   ! 0 |  459 | `			nArg` |
|     - |  460 | `			);` |
|     - |  461 | `	}` |
|     - |  462 | `	/* Type checking: reject non-numeric values for $y (argument #1). */` |
|    47 |  463 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|   ! 0 |  464 | `		return PH7_VmThrowException(pCtx,` |
|     - |  465 | `			"TypeError",` |
|     - |  466 | `			"atan2(): Argument #1 ($y) must be of type float, %s given",` |
|   ! 0 |  467 | `			ph7_type_name(apArg[0])` |
|     - |  468 | `			);` |
|     - |  469 | `	}` |
|     - |  470 | `	/* Type checking: reject non-numeric values for $x (argument #2). */` |
|    47 |  471 | `	if( !ph7_value_is_numeric(apArg[1]) ){` |
|   ! 0 |  472 | `		return PH7_VmThrowException(pCtx,` |
|     - |  473 | `			"TypeError",` |
|     - |  474 | `			"atan2(): Argument #2 ($x) must be of type float, %s given",` |
|   ! 0 |  475 | `			ph7_type_name(apArg[1])` |
|     - |  476 | `			);` |
|     - |  477 | `	}` |
|    47 |  478 | `	y = ph7_value_to_double(apArg[0]);` |
|    47 |  479 | `	x = ph7_value_to_double(apArg[1]);` |
|     - |  480 | `	/* Perform the requested operation */` |
|    47 |  481 | `	r = atan2(y,x);` |
|     - |  482 | `	/* store the result back */` |
|    47 |  483 | `	ph7_result_double(pCtx,r);` |
|    47 |  484 | `	return PH7_OK;` |
|    24 |  485 | `}` |
|     - |  486 | `/*` |
|     - |  487 | ` * float/int64 abs(float/int64 $arg )` |
|     - |  488 | ` *  Absolute value.` |
|     - |  489 | ` * Parameter` |
|     - |  490 | ` *  The number to process.` |
|     - |  491 | ` * Return` |
|     - |  492 | ` *  The absolute value of number.` |
|     - |  493 | ` */` |
|   146 |  494 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  495 | `{` |
|     - |  496 | `	int is_float;` |
|     - |  497 | `	/* PHP requires exactly one argument. */` |
|   148 |  498 | `	if( nArg != 1 ){` |
|   ! 0 |  499 | `		return PH7_VmThrowException(pCtx,` |
|     - |  500 | `			"ArgumentCountError",` |
|     - |  501 | `			"abs() expects exactly 1 argument, %d given",` |
|   ! 0 |  502 | `			nArg` |
|     - |  503 | `			);` |
|     - |  504 | `	}` |
|     - |  505 |  |
|   148 |  506 | `	if( ph7_value_is_null(apArg[0]) ){` |
|     - |  507 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|   ! 0 |  508 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  509 | `			"abs(): Argument #1 ($num) must be of type int\|float, null given");` |
|     - |  510 | `	}` |
|     - |  511 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|   148 |  512 | `	is_float = ph7_value_is_float(apArg[0]);` |
|   148 |  513 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|     - |  514 | `		int len;` |
|     9 |  515 | `		sxu8 bReal = FALSE;` |
|     9 |  516 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|     - |  517 | `		sxi32 rcNum;` |
|     9 |  518 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|     9 |  519 | `		if( rcNum != SXRET_OK ){` |
|   ! 0 |  520 | `			return PH7_VmThrowException(pCtx,` |
|     - |  521 | `				"TypeError",` |
|     - |  522 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|     - |  523 | `				);` |
|     - |  524 | `		}` |
|     9 |  525 | `		if( bReal ){` |
|     7 |  526 | `			is_float = 1;` |
|     3 |  527 | `		}` |
|     4 |  528 | `	}` |
|   148 |  529 | `	if( is_float ){` |
|     - |  530 | `		double r,x;` |
|   113 |  531 | `		x = ph7_value_to_double(apArg[0]);` |
|     - |  532 | `		/* Perform the requested operation */` |
|   113 |  533 | `		r = fabs(x);` |
|   113 |  534 | `		ph7_result_double(pCtx,r);` |
|    57 |  535 | `	}else{` |
|     - |  536 | ``		/* Read the full 64-bit value (the old 32-bit `int abs()` truncated any`` |
|     - |  537 | `		 * magnitude above 2^31 and was UB on INT_MIN). */` |
|    36 |  538 | `		sxi64 x = ph7_value_to_int64(apArg[0]);` |
|    36 |  539 | `		if( x == SMALLEST_INT64 ){` |
|     - |  540 | `			/* abs(PHP_INT_MIN) has no int representation, so PHP returns a float. */` |
|     3 |  541 | `			ph7_result_double(pCtx,-(double)x);` |
|     2 |  542 | `		}else{` |
|    34 |  543 | `			ph7_result_int64(pCtx,x < 0 ? -x : x);` |
|     - |  544 | `		}` |
|     - |  545 | `	}` |
|   148 |  546 | `	return PH7_OK;` |
|    75 |  547 | `}` |
|     - |  548 | `/*` |
|     - |  549 | ` * float log(float $arg,[int/float $base])` |
|     - |  550 | ` *  Natural logarithm.` |
|     - |  551 | ` * Parameter` |
|     - |  552 | ` *  $arg: The number to process.` |
|     - |  553 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|     - |  554 | ` * Return` |
|     - |  555 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|     - |  556 | ` * Note:` |
|     - |  557 | ` *  only Natural log and base-10 log are supported.` |
|     - |  558 | ` */` |
|    12 |  559 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  560 | `{` |
|     - |  561 | `	double r,x;` |
|    13 |  562 | `	if( nArg < 1 ){` |
|     - |  563 | `		/* Missing argument,return 0 */` |
|   ! 0 |  564 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  565 | `		return PH7_OK;` |
|     - |  566 | `	}` |
|    13 |  567 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  568 | `	/* Perform the requested operation */` |
|    13 |  569 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|     - |  570 | `		/* Base-10 log */` |
|     5 |  571 | `		r = log10(x);` |
|     3 |  572 | `	}else{` |
|     9 |  573 | `		r = log(x);` |
|     - |  574 | `	}` |
|     - |  575 | `	/* store the result back */` |
|    13 |  576 | `	ph7_result_double(pCtx,r);` |
|    13 |  577 | `	return PH7_OK;` |
|     7 |  578 | `}` |
|     - |  579 | `/*` |
|     - |  580 | ` * float log10(float $arg )` |
|     - |  581 | ` *  Base-10 logarithm.` |
|     - |  582 | ` * Parameter` |
|     - |  583 | ` *  The number to process.` |
|     - |  584 | ` * Return` |
|     - |  585 | ` *  The Base-10 logarithm of the given number.` |
|     - |  586 | ` */` |
|    14 |  587 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  588 | `{` |
|     - |  589 | `	double r,x;` |
|    15 |  590 | `	if( nArg < 1 ){` |
|     - |  591 | `		/* Missing argument,return 0 */` |
|   ! 0 |  592 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  593 | `		return PH7_OK;` |
|     - |  594 | `	}` |
|    15 |  595 | `	x = ph7_value_to_double(apArg[0]);` |
|     - |  596 | `	/* Perform the requested operation */` |
|    15 |  597 | `	r = log10(x);` |
|     - |  598 | `	/* store the result back */` |
|    15 |  599 | `	ph7_result_double(pCtx,r);` |
|    15 |  600 | `	return PH7_OK;` |
|     8 |  601 | `}` |
|     - |  602 | `/*` |
|     - |  603 | ` * mixed pow(mixed $num,mixed $exponent)` |
|     - |  604 | ` *  Exponential expression.` |
|     - |  605 | ` *` |
|     - |  606 | `` *  php does not implement pow() separately: the function and the `**` operator`` |
|     - |  607 | ` *  are the same ZEND_API pow_function, so they share an operand contract, a` |
|     - |  608 | ` *  result TYPE rule and every edge value. Reading the two arguments as doubles` |
|     - |  609 | `` *  and returning pow() shared none of it -- `pow(2,3)` answered float(8) where`` |
|     - |  610 | `` *  `2 ** 3` answers int(8) (a wrong TYPE for the most ordinary call there is),`` |
|     - |  611 | `` *  and the contract `**` enforces was absent entirely: pow('abc',2) answered`` |
|     - |  612 | ` *  float(0), pow([1],2) float(1) and pow($obj,2) float(1) after a conversion` |
|     - |  613 | ` *  warning, where every one of them is` |
|     - |  614 | `` *  `TypeError: Unsupported operand types: … ** int`.`` |
|     - |  615 | ` *` |
|     - |  616 | ` *  Both halves now come from the operator: VmArithOperandCheck() for the` |
|     - |  617 | ` *  contract (including the "A non-numeric value encountered" warning a` |
|     - |  618 | ` *  leading-numeric string gets before it computes with the prefix) and` |
|     - |  619 | ` *  PH7_MemObjPow() for the arithmetic.` |
|     - |  620 | ` * Return` |
|     - |  621 | ` *  base raised to the power of exp -- int when both operands are int, the` |
|     - |  622 | ` *  exponent is non-negative and the exact result fits in an int64; float` |
|     - |  623 | ` *  otherwise.` |
|     - |  624 | ` */` |
|    58 |  625 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  626 | `{` |
|    61 |  627 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  628 | `	ph7_value sBase,sExp;` |
|     - |  629 | `	SyBlob sMsg;` |
|     - |  630 | `	sxi32 rc;` |
|     - |  631 | `	/* Arity (exactly 2) is enforced from aBuiltinArity[] before the call. */` |
|    61 |  632 | `	if( nArg < 2 ){` |
|   ! 0 |  633 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  634 | `		return PH7_OK;` |
|     - |  635 | `	}` |
|    61 |  636 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    61 |  637 | `	if( VmArithOperandCheck(pVm,apArg[0],apArg[1],"**",&sMsg) != SXRET_OK ){` |
|    19 |  638 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|    12 |  639 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|    13 |  640 | `		SyBlobRelease(&sMsg);` |
|    13 |  641 | `		return rc;` |
|     - |  642 | `	}` |
|    49 |  643 | `	SyBlobRelease(&sMsg);` |
|     - |  644 | `	/* Work on COPIES: PH7_MemObjPow converts its operands in place, which the` |
|     - |  645 | `	 * opcode arm may do to its stack slots but a builtin may not do to the` |
|     - |  646 | `	 * caller's arguments. */` |
|    49 |  647 | `	PH7_MemObjInit(pVm,&sBase);` |
|    49 |  648 | `	PH7_MemObjInit(pVm,&sExp);` |
|    49 |  649 | `	PH7_MemObjLoad(apArg[0],&sBase);` |
|    49 |  650 | `	PH7_MemObjLoad(apArg[1],&sExp);` |
|    49 |  651 | `	PH7_MemObjPow(&sBase,&sExp,&sBase);` |
|    49 |  652 | `	if( (sBase.iFlags & MEMOBJ_REAL) != 0 ){` |
|    19 |  653 | `		ph7_result_double(pCtx,sBase.rVal);` |
|    10 |  654 | `	}else{` |
|    31 |  655 | `		ph7_result_int64(pCtx,sBase.x.iVal);` |
|     - |  656 | `	}` |
|    49 |  657 | `	PH7_MemObjRelease(&sBase);` |
|    49 |  658 | `	PH7_MemObjRelease(&sExp);` |
|    49 |  659 | `	return PH7_OK;` |
|    32 |  660 | `}` |
|     - |  661 | `/*` |
|     - |  662 | ` * float pi(void)` |
|     - |  663 | ` *  Returns an approximation of pi.` |
|     - |  664 | ` * Note` |
|     - |  665 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|     - |  666 | ` * Return` |
|     - |  667 | ` *  The value of pi as float.` |
|     - |  668 | ` */` |
|     4 |  669 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  670 | `{` |
|     2 |  671 | `	SXUNUSED(nArg); /* cc warning */` |
|     2 |  672 | `	SXUNUSED(apArg);` |
|     6 |  673 | `	ph7_result_double(pCtx,PH7_PI);` |
|     6 |  674 | `	return PH7_OK;` |
|     2 |  675 | `}` |
|     - |  676 | `/*` |
|     - |  677 | ` * float fmod(float $x,float $y)` |
|     - |  678 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|     - |  679 | ` * Parameters` |
|     - |  680 | ` * $x` |
|     - |  681 | ` *  The dividend` |
|     - |  682 | ` * $y` |
|     - |  683 | ` *  The divisor` |
|     - |  684 | ` * Return` |
|     - |  685 | ` *  The floating point remainder of x/y.` |
|     - |  686 | ` */` |
|     2 |  687 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  688 | `{` |
|     - |  689 | `	double x,y,r;` |
|     3 |  690 | `	if( nArg < 2 ){` |
|     - |  691 | `		/* Missing arguments */` |
|   ! 0 |  692 | `		ph7_result_double(pCtx,0);` |
|   ! 0 |  693 | `		return PH7_OK;` |
|     - |  694 | `	}` |
|     - |  695 | `	/* Extract given arguments */` |
|     3 |  696 | `	x = ph7_value_to_double(apArg[0]);` |
|     3 |  697 | `	y = ph7_value_to_double(apArg[1]);` |
|     - |  698 | `	/* Perform the requested operation */` |
|     3 |  699 | `	r = fmod(x,y);` |
|     - |  700 | `	/* Processing result */` |
|     3 |  701 | `	ph7_result_double(pCtx,r);` |
|     3 |  702 | `	return PH7_OK;` |
|     2 |  703 | `}` |
|     - |  704 | `/*` |
|     - |  705 | ` * float hypot(float $x,float $y)` |
|     - |  706 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|     - |  707 | ` * Parameters` |
|     - |  708 | ` * $x` |
|     - |  709 | ` *  Length of first side` |
|     - |  710 | ` * $y` |
|     - |  711 | ` *  Length of first side` |
|     - |  712 | ` * Return` |
|     - |  713 | ` *  Calculated length of the hypotenuse.` |
|     - |  714 | ` */` |
|     2 |  715 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  716 | `{` |
|     - |  717 | `	double x,y,r;` |
|     3 |  718 | `	if( nArg < 2 ){` |
|     - |  719 | `		/* Missing arguments */` |
|   ! 0 |  720 | `		ph7_result_double(pCtx,0);` |
|   ! 0 |  721 | `		return PH7_OK;` |
|     - |  722 | `	}` |
|     - |  723 | `	/* Extract given arguments */` |
|     3 |  724 | `	x = ph7_value_to_double(apArg[0]);` |
|     3 |  725 | `	y = ph7_value_to_double(apArg[1]);` |
|     - |  726 | `	/* Perform the requested operation */` |
|     3 |  727 | `	r = hypot(x,y);` |
|     - |  728 | `	/* Processing result */` |
|     3 |  729 | `	ph7_result_double(pCtx,r);` |
|     3 |  730 | `	return PH7_OK;` |
|     2 |  731 | `}` |
|     - |  732 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|     - |  733 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |  734 | `/* The PH7_ROUND_* mode numbering lives in ph7int.h: bcround() rounds by the` |
|     - |  735 | ` * same eight rules and reads the same RoundingMode cases. */` |
|     - |  736 | `/*` |
|     - |  737 | `` * php 8.4's `enum RoundingMode`, in php's own DECLARATION order -- which is the`` |
|     - |  738 | ` * order cases() reports and is NOT the order of the integer modes above. The two` |
|     - |  739 | ` * numberings disagree past the four HALF_* ones: php's integer 5 is CEILING and` |
|     - |  740 | ` * its enum's fifth case is TowardsZero, so the mapping has to be stated rather` |
|     - |  741 | ` * than computed from an ordinal. A PURE enum (no backing value), which is why the` |
|     - |  742 | `` * case carries its mode HERE instead of in a `case X = 5;` the script could read.`` |
|     - |  743 | ` *` |
|     - |  744 | ` * The enum is what round()'s third argument is documented as; the integer` |
|     - |  745 | ` * spelling stays accepted beside it because php still accepts it, which is what` |
|     - |  746 | `` * `RoundingMode\|int` in the signature says.`` |
|     - |  747 | ` */` |
|     - |  748 | `static const struct MathRoundingModeCase {` |
|     - |  749 | `	const char *zName;` |
|     - |  750 | `	int iMode;` |
|     - |  751 | `} aRoundingMode[] = {` |
|     - |  752 | `	{ "HalfAwayFromZero", PH7_ROUND_HALF_UP        },` |
|     - |  753 | `	{ "HalfTowardsZero",  PH7_ROUND_HALF_DOWN      },` |
|     - |  754 | `	{ "HalfEven",         PH7_ROUND_HALF_EVEN      },` |
|     - |  755 | `	{ "HalfOdd",          PH7_ROUND_HALF_ODD       },` |
|     - |  756 | `	{ "TowardsZero",      PH7_ROUND_TOWARD_ZERO    },` |
|     - |  757 | `	{ "AwayFromZero",     PH7_ROUND_AWAY_FROM_ZERO },` |
|     - |  758 | `	{ "NegativeInfinity", PH7_ROUND_FLOOR          },` |
|     - |  759 | `	{ "PositiveInfinity", PH7_ROUND_CEILING        },` |
|     - |  760 | `};` |
|     - |  761 | `/*` |
|     - |  762 | ` * Answer TRUE (and the integer mode) when pVal is a RoundingMode CASE.` |
|     - |  763 | ` *` |
|     - |  764 | ` * An enum case is an ordinary object here, so the test is its class plus the` |
|     - |  765 | `` * `name` slot every case carries -- the pure enum has no backing value to read.`` |
|     - |  766 | ` */` |
|   504 |  767 | `PH7_PRIVATE int PH7_RoundingModeCase(ph7_value *pVal,int *pMode)` |
|     1 |  768 | `{` |
|     - |  769 | `	ph7_class_instance *pObj;` |
|   505 |  770 | `	const char *zName = 0;` |
|   505 |  771 | `	int nName = 0;` |
|     - |  772 | `	sxu32 n;` |
|   505 |  773 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 ){` |
|   167 |  774 | `		return 0;` |
|     - |  775 | `	}` |
|   339 |  776 | `	pObj = (ph7_class_instance *)pVal->x.pOther;` |
|   338 |  777 | `	if( pObj->pClass == 0 \|\| pObj->pClass->sName.nByte != sizeof("RoundingMode")-1` |
|   339 |  778 | `	 \|\| SyMemcmp(pObj->pClass->sName.zString,"RoundingMode",sizeof("RoundingMode")-1) != 0 ){` |
|   ! 0 |  779 | `		return 0;` |
|     - |  780 | `	}` |
|   339 |  781 | `	PH7_NativeAttrStr(pObj,"name",&zName,&nName);` |
|  1537 |  782 | `	for( n = 0 ; n < SX_ARRAYSIZE(aRoundingMode) ; ++n ){` |
|  1537 |  783 | `		int nCase = (int)SyStrlen(aRoundingMode[n].zName);` |
|  1537 |  784 | `		if( nName == nCase && SyMemcmp(zName,aRoundingMode[n].zName,(sxu32)nCase) == 0 ){` |
|   339 |  785 | `			*pMode = aRoundingMode[n].iMode;` |
|   339 |  786 | `			return 1;` |
|     - |  787 | `		}` |
|   600 |  788 | `	}` |
|   ! 0 |  789 | `	return 0;` |
|   253 |  790 | `}` |
|     - |  791 | `/*` |
|     - |  792 | `` * Declare `enum RoundingMode` -- pure, eight cases, php's declaration order.`` |
|     - |  793 | ` */` |
|  5740 |  794 | `PH7_PRIVATE sxi32 PH7_VmInstallRoundingMode(ph7_vm *pVm)` |
|     5 |  795 | `{` |
|     - |  796 | `	/* The builder DUPLICATES each case name and, for an unbacked enum, keeps no` |
|     - |  797 | `	 * pointer into sValue at all, so this array may live on the stack. */` |
|     - |  798 | `	PH7_NativeEnumCase aCase[SX_ARRAYSIZE(aRoundingMode)];` |
|     - |  799 | `	sxu32 n;` |
| 51665 |  800 | `	for( n = 0 ; n < SX_ARRAYSIZE(aRoundingMode) ; ++n ){` |
| 45925 |  801 | `		aCase[n].zName = aRoundingMode[n].zName;` |
|     - |  802 | `		/* PH7_NATIVE_VAL_NULL: a pure enum's case has no backing value at all. */` |
| 45925 |  803 | `		aCase[n].sValue.zName = 0;` |
| 45925 |  804 | `		aCase[n].sValue.iMods = 0;` |
| 45925 |  805 | `		aCase[n].sValue.iType = PH7_NATIVE_VAL_NULL;` |
| 45925 |  806 | `		aCase[n].sValue.iValue = 0;` |
| 45925 |  807 | `		aCase[n].sValue.zValue = 0;` |
| 45925 |  808 | `		aCase[n].sValue.rValue = 0.0;` |
| 22965 |  809 | `	}` |
|  8615 |  810 | `	return PH7_InstallNativeEnum(&(*pVm),"RoundingMode",0,` |
|  2870 |  811 | `		aCase,SX_ARRAYSIZE(aCase),0,0);` |
|     5 |  812 | `}` |
|     - |  813 | `/*` |
|     - |  814 | ` * 10**power via an exact lookup table for 0..22, falling back to pow()` |
|     - |  815 | ` * otherwise. Port of php-src PHP-8.5 ext/standard/math.c php_intpow10().` |
|     - |  816 | ` */` |
|   414 |  817 | `static double MathIntPow10(int power)` |
|     3 |  818 | `{` |
|     - |  819 | `	static const double powers[] = {` |
|     - |  820 | `		1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,` |
|     - |  821 | `		1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22` |
|     - |  822 | `	};` |
|   417 |  823 | `	if( power < 0 \|\| power > 22 ){` |
|     5 |  824 | `		return pow(10.0, (double)power);` |
|     - |  825 | `	}` |
|   413 |  826 | `	return powers[power];` |
|   210 |  827 | `}` |
|   310 |  828 | `static double MathRoundBasicEdge(double integral, double exponent, int places)` |
|     3 |  829 | `{` |
|   158 |  830 | `	return (places > 0)` |
|    94 |  831 | `		? fabs((integral + copysign(0.5, integral)) / exponent)` |
|   263 |  832 | `		: fabs((integral + copysign(0.5, integral)) * exponent);` |
|     3 |  833 | `}` |
|    74 |  834 | `static double MathRoundZeroEdge(double integral, double exponent, int places)` |
|     1 |  835 | `{` |
|    38 |  836 | `	return (places > 0)` |
|   ! 0 |  837 | `		? fabs((integral) / exponent)` |
|    74 |  838 | `		: fabs((integral) * exponent);` |
|     1 |  839 | `}` |
|     - |  840 | `/*` |
|     - |  841 | ` * Round the extracted integral part according to the requested mode.` |
|     - |  842 | ` * Faithful port of php-src PHP-8.5 ext/standard/math.c php_round_helper().` |
|     - |  843 | ` */` |
|   408 |  844 | `static double MathRoundHelper(double integral, double value, double exponent, int places, int mode)` |
|     3 |  845 | `{` |
|   411 |  846 | `	double value_abs = fabs(value);` |
|     - |  847 | `	double edge_case;` |
|   411 |  848 | `	switch( mode ){` |
|   101 |  849 | `		case PH7_ROUND_HALF_UP:` |
|   205 |  850 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   205 |  851 | `			if( value_abs >= edge_case ){` |
|   128 |  852 | `				return integral + copysign(1.0, integral);` |
|     - |  853 | `			}` |
|    79 |  854 | `			return integral;` |
|    16 |  855 | `		case PH7_ROUND_HALF_DOWN:` |
|    33 |  856 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|    33 |  857 | `			if( value_abs > edge_case ){` |
|   ! 0 |  858 | `				return integral + copysign(1.0, integral);` |
|     - |  859 | `			}` |
|    33 |  860 | `			return integral;` |
|    13 |  861 | `		case PH7_ROUND_CEILING:` |
|    27 |  862 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    27 |  863 | `			if( value > 0.0 && value_abs > edge_case ){` |
|    15 |  864 | `				return integral + 1.0;` |
|     - |  865 | `			}` |
|    13 |  866 | `			return integral;` |
|    12 |  867 | `		case PH7_ROUND_FLOOR:` |
|    25 |  868 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    25 |  869 | `			if( value < 0.0 && value_abs > edge_case ){` |
|    11 |  870 | `				return integral - 1.0;` |
|     - |  871 | `			}` |
|    15 |  872 | `			return integral;` |
|    12 |  873 | `		case PH7_ROUND_TOWARD_ZERO:` |
|    25 |  874 | `			return integral;` |
|    12 |  875 | `		case PH7_ROUND_AWAY_FROM_ZERO:` |
|    25 |  876 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    25 |  877 | `			if( value_abs > edge_case ){` |
|    25 |  878 | `				return integral + copysign(1.0, integral);` |
|     - |  879 | `			}` |
|   ! 0 |  880 | `			return integral;` |
|    22 |  881 | `		case PH7_ROUND_HALF_EVEN:` |
|    45 |  882 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|    45 |  883 | `			if( value_abs > edge_case ){` |
|   ! 0 |  884 | `				return integral + copysign(1.0, integral);` |
|    45 |  885 | `			}else if( value_abs == edge_case ){` |
|    35 |  886 | `				if( fmod(integral, 2.0) != 0.0 ){ /* integral not even -> make it even */` |
|    19 |  887 | `					return integral + copysign(1.0, integral);` |
|     - |  888 | `				}` |
|     8 |  889 | `			}` |
|    27 |  890 | `			return integral;` |
|    16 |  891 | `		case PH7_ROUND_HALF_ODD:` |
|    33 |  892 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|    33 |  893 | `			if( value_abs > edge_case ){` |
|   ! 0 |  894 | `				return integral + copysign(1.0, integral);` |
|    33 |  895 | `			}else if( value_abs == edge_case ){` |
|    25 |  896 | `				if( fmod(integral, 2.0) == 0.0 ){ /* integral even -> make it odd */` |
|    15 |  897 | `					return integral + copysign(1.0, integral);` |
|     - |  898 | `				}` |
|     5 |  899 | `			}` |
|    19 |  900 | `			return integral;` |
|   ! 0 |  901 | `		default:` |
|   ! 0 |  902 | `			return integral; /* unreachable: mode validated by the caller */` |
|     - |  903 | `	}` |
|   207 |  904 | `}` |
|     - |  905 | `/*` |
|     - |  906 | `` * Round `value` to `places` decimals in `mode`. Faithful port of php-src`` |
|     - |  907 | ` * PHP-8.5 ext/standard/math.c _php_math_round() — the post-8.4` |
|     - |  908 | ` * integer-extraction algorithm with the +/-1 floating-point error` |
|     - |  909 | ` * correction step, required for byte-exact results on cases such as` |
|     - |  910 | ` * round(0.285, 2) == 0.29 that the old naive "+0.5" approach got wrong.` |
|     - |  911 | ` */` |
|   430 |  912 | `static double MathRound(double value, int places, int mode)` |
|     3 |  913 | `{` |
|     - |  914 | `	double exponent, tmp_value, tmp_value2;` |
|     - |  915 | `	int abs_places;` |
|   433 |  916 | `	if( !isfinite(value) \|\| value == 0.0 ){` |
|    17 |  917 | `		return value;` |
|     - |  918 | `	}` |
|     - |  919 | `	/* mirror php-src's clamp away from INT_MIN */` |
|   417 |  920 | `	if( places < -2147483647 ){` |
|   ! 0 |  921 | `		places = -2147483647;` |
|   ! 0 |  922 | `	}` |
|   417 |  923 | `	abs_places = places < 0 ? -places : places;` |
|   417 |  924 | `	exponent = MathIntPow10(abs_places);` |
|     - |  925 | `	/*` |
|     - |  926 | `	 * Extracting the integer part can be off by one ULP due to float error` |
|     - |  927 | `	 * (e.g. floor(0.285 * 1e10) == 2849999999). Try +/-1 and keep it if it` |
|     - |  928 | ``	 * divides back to exactly `value`.`` |
|     - |  929 | `	 */` |
|   417 |  930 | `	if( value >= 0.0 ){` |
|   321 |  931 | `		tmp_value = floor(places > 0 ? value * exponent : value / exponent);` |
|   321 |  932 | `		tmp_value2 = tmp_value + 1.0;` |
|   162 |  933 | `	}else{` |
|    97 |  934 | `		tmp_value = ceil(places > 0 ? value * exponent : value / exponent);` |
|    97 |  935 | `		tmp_value2 = tmp_value - 1.0;` |
|     - |  936 | `	}` |
|   417 |  937 | `	if( (places > 0 ? tmp_value2 / exponent : tmp_value2 * exponent) == value ){` |
|     7 |  938 | `		tmp_value = tmp_value2;` |
|     3 |  939 | `	}` |
|     - |  940 | `	/* Beyond our precision, so rounding it is pointless. */` |
|   417 |  941 | `	if( fabs(tmp_value) >= 1e16 ){` |
|     7 |  942 | `		return value;` |
|     - |  943 | `	}` |
|   411 |  944 | `	tmp_value = MathRoundHelper(tmp_value, value, exponent, places, mode);` |
|   411 |  945 | `	if( abs_places < 23 ){` |
|   411 |  946 | `		tmp_value = (places > 0) ? tmp_value / exponent : tmp_value * exponent;` |
|   207 |  947 | `	}else{` |
|     - |  948 | `		/*` |
|     - |  949 | `		 * Simple division would lose precision here; round-trip through a` |
|     - |  950 | `		 * string exactly like php-src does (snprintf "%15fe%d" + strtod).` |
|     - |  951 | `		 * libc snprintf is used (not SyBufferFormat, which is not` |
|     - |  952 | `		 * correctly-rounded) so the low bits match PHP. (SyStrToReal now` |
|     - |  953 | `		 * delegates to strtod too; the direct call here simply mirrors` |
|     - |  954 | `		 * php-src's own snprintf+strtod pairing.)` |
|     - |  955 | `		 */` |
|     - |  956 | `		char zBuf[64];` |
|   ! 0 |  957 | `		snprintf(zBuf, sizeof(zBuf), "%15fe%d", tmp_value, -places);` |
|   ! 0 |  958 | `		zBuf[sizeof(zBuf)-1] = '\0';` |
|   ! 0 |  959 | `		tmp_value = strtod(zBuf, 0);` |
|   ! 0 |  960 | `		if( !isfinite(tmp_value) \|\| isnan(tmp_value) ){` |
|   ! 0 |  961 | `			tmp_value = value;` |
|   ! 0 |  962 | `		}` |
|     - |  963 | `	}` |
|   411 |  964 | `	return tmp_value;` |
|   218 |  965 | `}` |
|     - |  966 | `/*` |
|     - |  967 | ` * float round ( int\|float $num [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|     - |  968 | ` *  Rounds a float.` |
|     - |  969 | ` * Parameters` |
|     - |  970 | ` *  $num       The value to round.` |
|     - |  971 | ` *  $precision The optional number of decimal digits to round to. May be` |
|     - |  972 | ` *             negative (rounds to the left of the decimal point).` |
|     - |  973 | ` *  $mode      One of PHP_ROUND_HALF_UP (default) / _HALF_DOWN / _HALF_EVEN /` |
|     - |  974 | ` *             _HALF_ODD, or the 8.5 integer modes CEILING / FLOOR /` |
|     - |  975 | ` *             TOWARD_ZERO / AWAY_FROM_ZERO (5..8).` |
|     - |  976 | ` * Return` |
|     - |  977 | ` *  The rounded value as a float.` |
|     - |  978 | ` */` |
|   360 |  979 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  980 | `{` |
|     - |  981 | `	double value, r;` |
|   361 |  982 | `	int places = 0;` |
|   361 |  983 | `	int mode = PH7_ROUND_HALF_UP;` |
|     - |  984 | `	/*` |
|     - |  985 | `	 * Legacy PHL contract: no argument -> int(0). PHP throws an` |
|     - |  986 | `	 * ArgumentCountError here, but two PHL-only (--SKIPIF-- zend_version)` |
|     - |  987 | `	 * tests assert round()===0, so keep the historical behavior.` |
|     - |  988 | `	 */` |
|   361 |  989 | `	if( nArg < 1 ){` |
|   ! 0 |  990 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  991 | `		return PH7_OK;` |
|     - |  992 | `	}` |
|   361 |  993 | `	if( nArg > 3 ){` |
|   ! 0 |  994 | `		return PH7_VmThrowException(pCtx,` |
|     - |  995 | `			"ArgumentCountError",` |
|     - |  996 | `			"round() expects at most 3 arguments, %d given",` |
|   ! 0 |  997 | `			nArg` |
|     - |  998 | `			);` |
|     - |  999 | `	}` |
|     - | 1000 | `	/* Argument #1's type is the aBuiltinSig[] row's -- see floor() above. */` |
|     - | 1001 | `	/* Precision (arg #2). Negative values are valid; clamp to int range. */` |
|   361 | 1002 | `	if( nArg > 1 ){` |
|   325 | 1003 | `		sxi64 prec = ph7_value_to_int64(apArg[1]);` |
|   325 | 1004 | `		if( prec > 2147483647 ){` |
|   ! 0 | 1005 | `			places = 2147483647;` |
|   325 | 1006 | `		}else if( prec < -2147483647 ){` |
|   ! 0 | 1007 | `			places = -2147483647;` |
|   ! 0 | 1008 | `		}else{` |
|   325 | 1009 | `			places = (int)prec;` |
|     - | 1010 | `		}` |
|   162 | 1011 | `	}` |
|     - | 1012 | `	/*` |
|     - | 1013 | ``	 * Mode (arg #3). php declares it `RoundingMode\|int`, so an enum CASE and the`` |
|     - | 1014 | `	 * raw integer both arrive here. The integer modes are 1..8; read the full` |
|     - | 1015 | `	 * 64-bit value before range-checking so a large out-of-range mode cannot` |
|     - | 1016 | `	 * alias a valid 1..8 via a truncating 32-bit cast (e.g. 0x1_0000_0003).` |
|     - | 1017 | `	 */` |
|   361 | 1018 | `	if( nArg > 2 ){` |
|     - | 1019 | ``		/* A RoundingMode case answers its integer mode straight into `mode`; the`` |
|     - | 1020 | `		 * integer spelling is range-checked here. */` |
|   253 | 1021 | `		if( !PH7_RoundingModeCase(apArg[2],&mode) ){` |
|   167 | 1022 | `			sxi64 m = ph7_value_to_int64(apArg[2]);` |
|   167 | 1023 | `			if( m < PH7_ROUND_HALF_UP \|\| m > PH7_ROUND_AWAY_FROM_ZERO ){` |
|    13 | 1024 | `				return PH7_VmThrowException(pCtx,` |
|     - | 1025 | `					"ValueError",` |
|     - | 1026 | `					"round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)"` |
|     - | 1027 | `					);` |
|     - | 1028 | `			}` |
|   155 | 1029 | `			mode = (int)m;` |
|    77 | 1030 | `		}` |
|   120 | 1031 | `	}` |
|   349 | 1032 | `	value = ph7_value_to_double(apArg[0]);` |
|     - | 1033 | `	/* Integer input with non-negative precision needs no rounding. */` |
|   349 | 1034 | `	if( ph7_value_is_int(apArg[0]) && places >= 0 ){` |
|    21 | 1035 | `		ph7_result_double(pCtx,value);` |
|    21 | 1036 | `		return PH7_OK;` |
|     - | 1037 | `	}` |
|   329 | 1038 | `	r = MathRound(value, places, mode);` |
|   329 | 1039 | `	ph7_result_double(pCtx,r);` |
|   329 | 1040 | `	return PH7_OK;` |
|   181 | 1041 | `}` |
|     - | 1042 | `/*` |
|     - | 1043 | ` * Assemble php's formatted number: the integer digits grouped from the right by` |
|     - | 1044 | ` * the thousands separator, then the decimal separator and exactly $decimals` |
|     - | 1045 | ` * fraction digits (right-padded with '0', since the printf may produce fewer).` |
|     - | 1046 | ` */` |
|   136 | 1047 | `static int NumberFormatEmit(ph7_context *pCtx,` |
|     - | 1048 | `	const char *zDigits,int nDigits,int bNeg,` |
|     - | 1049 | `	const char *zFrac,int nFrac,int nDec,` |
|     - | 1050 | `	const char *zPoint,int nPoint,const char *zSep,int nSep)` |
|     3 | 1051 | `{` |
|     - | 1052 | `	SyBlob sOut;` |
|     - | 1053 | `	int i;` |
|   139 | 1054 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   139 | 1055 | `	if( bNeg ){` |
|    12 | 1056 | `		SyBlobAppend(&sOut,"-",sizeof(char));` |
|     5 | 1057 | `	}` |
|   833 | 1058 | `	for( i = 0 ; i < nDigits ; ++i ){` |
|   697 | 1059 | `		if( i > 0 && nSep > 0 && ((nDigits - i) % 3) == 0 ){` |
|   173 | 1060 | `			SyBlobAppend(&sOut,zSep,(sxu32)nSep);` |
|    85 | 1061 | `		}` |
|   697 | 1062 | `		SyBlobAppend(&sOut,&zDigits[i],sizeof(char));` |
|   350 | 1063 | `	}` |
|   139 | 1064 | `	if( nDec > 0 ){` |
|    59 | 1065 | `		if( nPoint > 0 ){` |
|    55 | 1066 | `			SyBlobAppend(&sOut,zPoint,(sxu32)nPoint);` |
|    26 | 1067 | `		}` |
|    59 | 1068 | `		if( nFrac > nDec ){` |
|   ! 0 | 1069 | `			nFrac = nDec;` |
|   ! 0 | 1070 | `		}` |
|    59 | 1071 | `		if( nFrac > 0 ){` |
|    57 | 1072 | `			SyBlobAppend(&sOut,zFrac,(sxu32)nFrac);` |
|    27 | 1073 | `		}` |
|    63 | 1074 | `		for( i = nFrac ; i < nDec ; ++i ){` |
|     5 | 1075 | `			SyBlobAppend(&sOut,"0",sizeof(char));` |
|     3 | 1076 | `		}` |
|    28 | 1077 | `	}` |
|   139 | 1078 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   139 | 1079 | `	SyBlobRelease(&sOut);` |
|   139 | 1080 | `	return PH7_OK;` |
|     3 | 1081 | `}` |
|     - | 1082 | `/*` |
|     - | 1083 | ` * _php_math_number_format_long(): an INTEGER never goes through a double, so` |
|     - | 1084 | ` * every digit of a value past 2^53 survives. A NEGATIVE $decimals rounds the` |
|     - | 1085 | ` * integer itself, half away from zero.` |
|     - | 1086 | ` */` |
|    44 | 1087 | `static int NumberFormatLong(ph7_context *pCtx,sxi64 iVal,int nDec,` |
|     - | 1088 | `	const char *zPoint,int nPoint,const char *zSep,int nSep)` |
|     2 | 1089 | `{` |
|     - | 1090 | `	char zBuf[32];` |
|     - | 1091 | `	sxu64 uNum;` |
|    46 | 1092 | `	int bNeg = 0;` |
|    46 | 1093 | `	int n = 0;` |
|    46 | 1094 | `	if( iVal < 0 ){` |
|    11 | 1095 | `		bNeg = 1;` |
|     - | 1096 | `		/* -PHP_INT_MIN does not fit; negate through the unsigned domain. */` |
|    11 | 1097 | `		uNum = ((sxu64)-(iVal + 1)) + 1;` |
|     6 | 1098 | `	}else{` |
|    36 | 1099 | `		uNum = (sxu64)iVal;` |
|     - | 1100 | `	}` |
|    46 | 1101 | `	if( nDec < 0 ){` |
|     - | 1102 | `		/* php keeps a table of the 20 powers of ten a 64-bit value can hold and` |
|     - | 1103 | `		 * answers 0 past it; 10^19 is the last one that fits. */` |
|    21 | 1104 | `		if( nDec < -19 ){` |
|     3 | 1105 | `			uNum = 0;` |
|     2 | 1106 | `		}else{` |
|    19 | 1107 | `			sxu64 uPow = 1;` |
|     - | 1108 | `			sxu64 uRest;` |
|     - | 1109 | `			int k;` |
|    49 | 1110 | `			for( k = 0 ; k < -nDec ; ++k ){` |
|    31 | 1111 | `				uPow *= 10;` |
|    16 | 1112 | `			}` |
|    19 | 1113 | `			uRest = uNum % uPow;` |
|    19 | 1114 | `			uNum = uNum / uPow;` |
|    19 | 1115 | `			uNum = (uRest >= uPow / 2) ? uNum * uPow + uPow : uNum * uPow;` |
|     - | 1116 | `		}` |
|    21 | 1117 | `		if( uNum == 0 ){` |
|     - | 1118 | `			/* php never answers "-0". */` |
|     9 | 1119 | `			bNeg = 0;` |
|     4 | 1120 | `		}` |
|    10 | 1121 | `	}` |
|     - | 1122 | `	/* Decimal digits, most significant first. */` |
|    46 | 1123 | `	if( uNum == 0 ){` |
|    14 | 1124 | `		zBuf[n++] = '0';` |
|     8 | 1125 | `	}else{` |
|     - | 1126 | `		char zRev[32];` |
|    34 | 1127 | `		int nRev = 0;` |
|   372 | 1128 | `		while( uNum > 0 && nRev < (int)sizeof(zRev) ){` |
|   340 | 1129 | `			zRev[nRev++] = (char)('0' + (int)(uNum % 10));` |
|   340 | 1130 | `			uNum /= 10;` |
|     2 | 1131 | `		}` |
|   372 | 1132 | `		while( nRev > 0 ){` |
|   340 | 1133 | `			zBuf[n++] = zRev[--nRev];` |
|     2 | 1134 | `		}` |
|     - | 1135 | `	}` |
|    46 | 1136 | `	return NumberFormatEmit(pCtx,zBuf,n,bNeg,0,0,nDec > 0 ? nDec : 0,` |
|    22 | 1137 | `		zPoint,nPoint,zSep,nSep);` |
|     2 | 1138 | `}` |
|     - | 1139 | `/*` |
|     - | 1140 | ` * string number_format(int\|float $num,int $decimals = 0,` |
|     - | 1141 | ` *                      ?string $decimal_separator = ".",` |
|     - | 1142 | ` *                      ?string $thousands_separator = ",")` |
|     - | 1143 | ` *  Format a number with grouped thousands.` |
|     - | 1144 | ` */` |
|   204 | 1145 | `PH7_PRIVATE int PH7_builtin_number_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1146 | `{` |
|   207 | 1147 | `	const char *zPoint = ".", *zSep = ",";` |
|   207 | 1148 | `	int nPoint = 1, nSep = 1;` |
|   207 | 1149 | `	int nDec = 0;` |
|     - | 1150 | `	ph7_value sNum;` |
|     - | 1151 | `	double d;` |
|   207 | 1152 | `	int bNeg = 0;` |
|     - | 1153 | `	int nLen,nInt;` |
|     - | 1154 | `	char *zFmt;` |
|     - | 1155 | `	const char *zDot;` |
|   207 | 1156 | `	if( nArg < 1 ){` |
|     - | 1157 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|   ! 0 | 1158 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1159 | `		return PH7_OK;` |
|     - | 1160 | `	}` |
|     - | 1161 | `	/* Every refusal is worded here rather than by the shared screen: php's stub` |
|     - | 1162 | ``	 * declares `float $num` (which is what Reflection prints) but the ZPP macro`` |
|     - | 1163 | ``	 * behind it is Z_PARAM_NUMBER, whose TypeError says `int\|float`. An int stays`` |
|     - | 1164 | `	 * an INT, a numeric string takes the shape it looks like, and null is §10's` |
|     - | 1165 | `	 * refusal of a deprecation. */` |
|   207 | 1166 | `	if( !PH7_MemObjIsNumeric(apArg[0]) ){` |
|     - | 1167 | `		char zBuf[64];` |
|    60 | 1168 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1169 | `			"number_format(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    32 | 1170 | `			ph7_value_is_string(apArg[0]) ? "string"` |
|    18 | 1171 | `				: VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|     - | 1172 | `	}` |
|   175 | 1173 | `	if( nArg > 1 ){` |
|     - | 1174 | ``		/* php declares `int $decimals`; the string and float narrowings it only`` |
|     - | 1175 | `		 * DEPRECATES are rejected here (§10), as they are for count_chars(). */` |
|   124 | 1176 | `		if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|   124 | 1177 | `		 \|\| ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     - | 1178 | `			char zBuf[64];` |
|    14 | 1179 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1180 | `				"number_format(): Argument #2 ($decimals) must be of type int, %s given",` |
|     8 | 1181 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|     - | 1182 | `		}` |
|   119 | 1183 | `		if( ph7_value_is_string(apArg[1]) ){` |
|     - | 1184 | `			/* php wants the WHOLE string to be numeric ("2abc" is a TypeError,` |
|     - | 1185 | `			 * not 2), and a float-shaped one that would LOSE something is §10's` |
|     - | 1186 | `			 * refusal of a deprecation. */` |
|     - | 1187 | `			double dMode;` |
|     8 | 1188 | `			if( !PH7_MemObjStringIsNumeric(apArg[1]) ){` |
|     6 | 1189 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1190 | `					"number_format(): Argument #2 ($decimals) must be of type int, string given");` |
|     - | 1191 | `			}` |
|     3 | 1192 | `			dMode = ph7_value_to_double(apArg[1]);` |
|     - | 1193 | ``			/* Range first: `(sxi64)dMode` is undefined outside it (§2). */`` |
|     3 | 1194 | `			if( !PH7_RealFitsInt64(dMode) \|\| dMode != (double)(sxi64)dMode ){` |
|   ! 0 | 1195 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1196 | `					"number_format(): Argument #2 ($decimals) must be of type int, string given");` |
|     1 | 1197 | `			}` |
|   114 | 1198 | `		}else if( ph7_value_is_float(apArg[1]) ){` |
|     6 | 1199 | `			double dMode = ph7_value_to_double(apArg[1]);` |
|     6 | 1200 | `			if( !PH7_RealFitsInt64(dMode) \|\| dMode != (double)(sxi64)dMode ){` |
|     3 | 1201 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1202 | `					"number_format(): Argument #2 ($decimals) must be of type int, float given");` |
|     - | 1203 | `			}` |
|     1 | 1204 | `		}` |
|     - | 1205 | `		{` |
|   113 | 1206 | `			sxi64 iDec = ph7_value_to_int64(apArg[1]);` |
|     - | 1207 | `			/* php clamps the declared long onto an int before it formats. */` |
|   168 | 1208 | `			nDec = iDec > 2147483647 ? 2147483647` |
|   110 | 1209 | `			     : (iDec < -2147483647 ? -2147483647 : (int)iDec);` |
|     - | 1210 | `		}` |
|    55 | 1211 | `	}` |
|     - | 1212 | ``	/* Both separators are `?string`: null means php's default, not the empty`` |
|     - | 1213 | `	 * string. An empty string IS accepted and simply omits the separator, and an` |
|     - | 1214 | `	 * object that can stringify is coerced. */` |
|   161 | 1215 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|    36 | 1216 | `		if( !PH7_ArgSatisfiesString(apArg[2]) ){` |
|     - | 1217 | `			char zBuf[64];` |
|    14 | 1218 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1219 | `				"number_format(): Argument #3 ($decimal_separator) must be of type ?string, %s given",` |
|     8 | 1220 | `				VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|     - | 1221 | `		}` |
|    28 | 1222 | `		zPoint = ph7_value_to_string(apArg[2],&nPoint);` |
|    13 | 1223 | `	}` |
|   153 | 1224 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|    28 | 1225 | `		if( !PH7_ArgSatisfiesString(apArg[3]) ){` |
|     - | 1226 | `			char zBuf[64];` |
|     7 | 1227 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1228 | `				"number_format(): Argument #4 ($thousands_separator) must be of type ?string, %s given",` |
|     4 | 1229 | `				VmValueGivenName(apArg[3],zBuf,sizeof(zBuf)));` |
|     - | 1230 | `		}` |
|    24 | 1231 | `		zSep = ph7_value_to_string(apArg[3],&nSep);` |
|    11 | 1232 | `	}` |
|   149 | 1233 | `	PH7_MemObjInit(pCtx->pVm,&sNum);` |
|   149 | 1234 | `	PH7_MemObjStore(apArg[0],&sNum);` |
|   149 | 1235 | `	PH7_MemObjToNumeric(&sNum);` |
|   149 | 1236 | `	if( (sNum.iFlags & MEMOBJ_REAL) == 0 ){` |
|    44 | 1237 | `		int rc = NumberFormatLong(pCtx,sNum.x.iVal,nDec,zPoint,nPoint,zSep,nSep);` |
|    44 | 1238 | `		PH7_MemObjRelease(&sNum);` |
|    44 | 1239 | `		return rc;` |
|     - | 1240 | `	}` |
|   107 | 1241 | `	d = (double)sNum.rVal;` |
|   107 | 1242 | `	PH7_MemObjRelease(&sNum);` |
|     - | 1243 | `	/* A double past 2^52 has no fractional digits left, so php formats it as an` |
|     - | 1244 | `	 * INTEGER when it fits one — that is what keeps 4503599627370496.0 exact. */` |
|   104 | 1245 | `	if( (d >= 4503599627370496.0 \|\| d <= -4503599627370496.0)` |
|    60 | 1246 | `	 && PH7_RealFitsInt64(d) ){` |
|     3 | 1247 | `		return NumberFormatLong(pCtx,(sxi64)d,nDec,zPoint,nPoint,zSep,nSep);` |
|     - | 1248 | `	}` |
|   105 | 1249 | `	if( d < 0 ){` |
|    14 | 1250 | `		bNeg = 1;` |
|    14 | 1251 | `		d = -d;` |
|     6 | 1252 | `	}` |
|   105 | 1253 | `	d = MathRound(d,nDec,PH7_ROUND_HALF_UP);` |
|   105 | 1254 | `	if( nDec < 0 ){` |
|    16 | 1255 | `		nDec = 0;` |
|     7 | 1256 | `	}` |
|     - | 1257 | `	/* libc's %f, not the engine's formatter: php prints through its own` |
|     - | 1258 | `	 * snprintf here, so INF answers "inf" and NAN "nan" — and the engine's` |
|     - | 1259 | `	 * formatter caps the precision at 53 digits with a notice, where php` |
|     - | 1260 | `	 * honours whatever $decimals asks for. */` |
|   105 | 1261 | `	nLen = snprintf(0,0,"%.*f",nDec,d);` |
|   105 | 1262 | `	if( nLen < 0 ){` |
|   ! 0 | 1263 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1264 | `	}` |
|   105 | 1265 | `	zFmt = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen + 1,FALSE,TRUE);` |
|   105 | 1266 | `	if( zFmt == 0 ){` |
|   ! 0 | 1267 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1268 | `	}` |
|   105 | 1269 | `	snprintf(zFmt,(size_t)nLen + 1,"%.*f",nDec,d);` |
|   105 | 1270 | `	if( zFmt[0] < '0' \|\| zFmt[0] > '9' ){` |
|     - | 1271 | `		/* Not a number at all (inf/nan): php hands its buffer straight back,` |
|     - | 1272 | `		 * without a sign, a separator or any padding. */` |
|    11 | 1273 | `		ph7_result_string(pCtx,zFmt,nLen);` |
|    11 | 1274 | `		return PH7_OK;` |
|     - | 1275 | `	}` |
|    95 | 1276 | `	if( bNeg && d == 0 ){` |
|     - | 1277 | `		/* Rounded away to zero; php never answers "-0". */` |
|     5 | 1278 | `		bNeg = 0;` |
|     2 | 1279 | `	}` |
|     - | 1280 | `	/* php looks for '.' OR ',' — the decimal point its formatter produced. */` |
|    95 | 1281 | `	zDot = 0;` |
|    95 | 1282 | `	if( nDec > 0 ){` |
|     - | 1283 | `		int i;` |
|   273 | 1284 | `		for( i = 0 ; i < nLen ; ++i ){` |
|   273 | 1285 | `			if( zFmt[i] == '.' \|\| zFmt[i] == ',' ){` |
|    57 | 1286 | `				zDot = &zFmt[i];` |
|    57 | 1287 | `				break;` |
|     - | 1288 | `			}` |
|   111 | 1289 | `		}` |
|    27 | 1290 | `	}` |
|    95 | 1291 | `	nInt = zDot ? (int)(zDot - zFmt) : nLen;` |
|   168 | 1292 | `	return NumberFormatEmit(pCtx,zFmt,nInt,bNeg,` |
|    73 | 1293 | `		zDot ? zDot + 1 : 0,zDot ? nLen - nInt - 1 : 0,` |
|    46 | 1294 | `		nDec,zPoint,nPoint,zSep,nSep);` |
|   105 | 1295 | `}` |
|     - | 1296 | `/*` |
|     - | 1297 | ` * int intdiv(int $a, int $b)` |
|     - | 1298 | ` *  Integer division.` |
|     - | 1299 | ` * Parameters` |
|     - | 1300 | ` *  $a` |
|     - | 1301 | ` *   Number to be divided.` |
|     - | 1302 | ` *  $b` |
|     - | 1303 | ` *   Number which divides the $a.` |
|     - | 1304 | ` * Return` |
|     - | 1305 | ` *  The integer quotient of the division of $a by $b.` |
|     - | 1306 | ` */` |
|    26 | 1307 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1308 | `{` |
|     - | 1309 | `	sxi64 a,b;` |
|     - | 1310 | `	/* PHP requires exactly two arguments. */` |
|    29 | 1311 | `	if( nArg != 2 ){` |
|   ! 0 | 1312 | `		return PH7_VmThrowException(pCtx,` |
|     - | 1313 | `			"ArgumentCountError",` |
|     - | 1314 | `			"intdiv() expects exactly 2 arguments, %d given",` |
|   ! 0 | 1315 | `			nArg` |
|     - | 1316 | `			);` |
|     - | 1317 | `	}` |
|     - | 1318 | `	/* Type-check argument 1 */` |
|    26 | 1319 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|    29 | 1320 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|   ! 0 | 1321 | `		return PH7_VmThrowException(pCtx,` |
|     - | 1322 | `			"TypeError",` |
|     - | 1323 | `			"intdiv(): Argument #1 ($num1) must be of type int, %s given",` |
|   ! 0 | 1324 | `			ph7_type_name(apArg[0])` |
|     - | 1325 | `			);` |
|     - | 1326 | `	}` |
|    29 | 1327 | `	if( ph7_value_is_string(apArg[0]) ){` |
|     - | 1328 | `		int len;` |
|     3 | 1329 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|     3 | 1330 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|   ! 0 | 1331 | `			return PH7_VmThrowException(pCtx,` |
|     - | 1332 | `				"TypeError",` |
|     - | 1333 | `				"intdiv(): Argument #1 ($num1) must be of type int, string given"` |
|     - | 1334 | `				);` |
|     - | 1335 | `		}` |
|     1 | 1336 | `	}` |
|     - | 1337 | `	/* Type-check argument 2 */` |
|    26 | 1338 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|    29 | 1339 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|   ! 0 | 1340 | `		return PH7_VmThrowException(pCtx,` |
|     - | 1341 | `			"TypeError",` |
|     - | 1342 | `			"intdiv(): Argument #2 ($num2) must be of type int, %s given",` |
|   ! 0 | 1343 | `			ph7_type_name(apArg[1])` |
|     - | 1344 | `			);` |
|     - | 1345 | `	}` |
|    29 | 1346 | `	if( ph7_value_is_string(apArg[1]) ){` |
|     - | 1347 | `		int len;` |
|   ! 0 | 1348 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|   ! 0 | 1349 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|   ! 0 | 1350 | `			return PH7_VmThrowException(pCtx,` |
|     - | 1351 | `				"TypeError",` |
|     - | 1352 | `				"intdiv(): Argument #2 ($num2) must be of type int, string given"` |
|     - | 1353 | `				);` |
|     - | 1354 | `		}` |
|   ! 0 | 1355 | `	}` |
|     - | 1356 | `	/* Convert both arguments to int64 */` |
|     - | 1357 | `	{` |
|     - | 1358 | `		/* php's ZPP contract for the two int params (lossy float / float-string` |
|     - | 1359 | `		 * deprecations); the manual type checks above already covered arrays,` |
|     - | 1360 | `		 * objects and non-numeric strings with the same messages. */` |
|    29 | 1361 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[0],"intdiv",1,"$num1","int",&a);` |
|    29 | 1362 | `		if( rcArg != PH7_OK ){` |
|   ! 0 | 1363 | `			return rcArg;` |
|     - | 1364 | `		}` |
|    29 | 1365 | `		rcArg = PH7_IntArgResolve(pCtx,apArg[1],"intdiv",2,"$num2","int",&b);` |
|    29 | 1366 | `		if( rcArg != PH7_OK ){` |
|   ! 0 | 1367 | `			return rcArg;` |
|     - | 1368 | `		}` |
|     - | 1369 | `	}` |
|     - | 1370 | `	/* Check for division by zero */` |
|    29 | 1371 | `	if( b == 0 ){` |
|     6 | 1372 | `		return PH7_VmThrowException(pCtx,` |
|     - | 1373 | `			"DivisionByZeroError",` |
|     - | 1374 | `			"Division by zero"` |
|     - | 1375 | `			);` |
|     - | 1376 | `	}` |
|     - | 1377 | `	/* Check for overflow: PHP_INT_MIN / -1 */` |
|    25 | 1378 | `	if( a == SMALLEST_INT64 && b == -1 ){` |
|     3 | 1379 | `		return PH7_VmThrowException(pCtx,` |
|     - | 1380 | `			"ArithmeticError",` |
|     - | 1381 | `			"Division of PHP_INT_MIN by -1 is not an integer"` |
|     - | 1382 | `			);` |
|     - | 1383 | `	}` |
|     - | 1384 | `	/* Perform integer division */` |
|    22 | 1385 | `	ph7_result_int64(pCtx, a / b);` |
|    22 | 1386 | `	return PH7_OK;` |
|    16 | 1387 | `}` |
|     - | 1388 | `/*` |
|     - | 1389 | ` * string dechex(int $number)` |
|     - | 1390 | ` *  Decimal to hexadecimal.` |
|     - | 1391 | ` * Parameters` |
|     - | 1392 | ` *  $number` |
|     - | 1393 | ` *   Decimal value to convert` |
|     - | 1394 | ` * Return` |
|     - | 1395 | ` *  Hexadecimal string representation of number` |
|     - | 1396 | ` */` |
|    22 | 1397 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1398 | `{` |
|     - | 1399 | `	ph7_int64 iVal;` |
|    24 | 1400 | `	if( nArg < 1 ){` |
|     - | 1401 | `		/* Missing arguments,return null */` |
|   ! 0 | 1402 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1403 | `		return PH7_OK;` |
|     - | 1404 | `	}` |
|     - | 1405 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|    24 | 1406 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|     - | 1407 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement,` |
|     - | 1408 | `	 * so a negative value prints all 16 hex digits like PHP. */` |
|    24 | 1409 | `	ph7_result_string_format(pCtx,"%qx",iVal);` |
|    24 | 1410 | `	return PH7_OK;` |
|    13 | 1411 | `}` |
|     - | 1412 | `/*` |
|     - | 1413 | ` * string decoct(int $number)` |
|     - | 1414 | ` *  Decimal to Octal.` |
|     - | 1415 | ` * Parameters` |
|     - | 1416 | ` *  $number` |
|     - | 1417 | ` *   Decimal value to convert` |
|     - | 1418 | ` * Return` |
|     - | 1419 | ` *  Octal string representation of number` |
|     - | 1420 | ` */` |
|    16 | 1421 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1422 | `{` |
|     - | 1423 | `	ph7_int64 iVal;` |
|    17 | 1424 | `	if( nArg < 1 ){` |
|     - | 1425 | `		/* Missing arguments,return null */` |
|   ! 0 | 1426 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1427 | `		return PH7_OK;` |
|     - | 1428 | `	}` |
|     - | 1429 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|    17 | 1430 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|     - | 1431 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|    17 | 1432 | `	ph7_result_string_format(pCtx,"%qo",iVal);` |
|    17 | 1433 | `	return PH7_OK;` |
|     9 | 1434 | `}` |
|     - | 1435 | `/*` |
|     - | 1436 | ` * string decbin(int $number)` |
|     - | 1437 | ` *  Decimal to binary.` |
|     - | 1438 | ` * Parameters` |
|     - | 1439 | ` *  $number` |
|     - | 1440 | ` *   Decimal value to convert` |
|     - | 1441 | ` * Return` |
|     - | 1442 | ` *  Binary string representation of number` |
|     - | 1443 | ` */` |
|    10 | 1444 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1445 | `{` |
|     - | 1446 | `	ph7_int64 iVal;` |
|    11 | 1447 | `	if( nArg < 1 ){` |
|     - | 1448 | `		/* Missing arguments,return null */` |
|   ! 0 | 1449 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1450 | `		return PH7_OK;` |
|     - | 1451 | `	}` |
|     - | 1452 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|    11 | 1453 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|     - | 1454 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|    11 | 1455 | `	ph7_result_string_format(pCtx,"%qB",iVal);` |
|    11 | 1456 | `	return PH7_OK;` |
|     6 | 1457 | `}` |
|     - | 1458 | `/*` |
|     - | 1459 | ` * Convert a base-2/8/16 digit string to a number, mirroring PHP's` |
|     - | 1460 | ` * _php_math_basetozval (ext/standard/math.c) so hexdec/octdec/bindec agree with` |
|     - | 1461 | ` * php byte-for-byte: walk every byte, decode a digit (0-9,a-z,A-Z) or skip any` |
|     - | 1462 | ` * invalid one, accumulate into a signed 64-bit integer and transparently promote` |
|     - | 1463 | ` * to a double once the value would overflow PHP_INT_MAX. The context result is` |
|     - | 1464 | ` * set to an int when it fits, otherwise a float — PHP returns a float for values` |
|     - | 1465 | ` * above PHP_INT_MAX (e.g. hexdec("ffffffffffffffff") == 1.8446744073709552E+19).` |
|     - | 1466 | ` * A byte >= 0x80 (e.g. a UTF-8 continuation) matches none of the digit ranges and` |
|     - | 1467 | ` * is skipped, so leading/interior multibyte junk is ignored like php.` |
|     - | 1468 | ` * Note: php also raises E_DEPRECATED for skipped invalid characters; that notice` |
|     - | 1469 | ` * is not emitted here (a §3.7 deprecation-fidelity residual, value is correct).` |
|     - | 1470 | ` */` |
| 24718 | 1471 | `static void MathBaseToNumber(ph7_context *pCtx,const char *zStr,int nLen,int base)` |
|     3 | 1472 | `{` |
| 24721 | 1473 | `	sxi64 num = 0;      /* Integer accumulator */` |
| 24721 | 1474 | `	double fnum = 0;    /* Float accumulator (used once num would overflow) */` |
| 24721 | 1475 | `	int mode = 0;       /* 0 -> integer accumulation, 1 -> switched to float */` |
| 24721 | 1476 | `	sxi64 cutoff = SXI64_HIGH / base;      /* PHP_INT_MAX / base */` |
| 24721 | 1477 | `	int cutlim = (int)(SXI64_HIGH % base); /* PHP_INT_MAX % base */` |
| 24721 | 1478 | `	int bIgnored = 0;   /* any character skipped below? php deprecates that */` |
|     - | 1479 | `	int i;` |
| 74575 | 1480 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 49857 | 1481 | `		int c = (unsigned char)zStr[i];` |
| 49857 | 1482 | `		if( c >= '0' && c <= '9' ){` |
| 37940 | 1483 | `			c -= '0';` |
| 31229 | 1484 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|   ! 0 | 1485 | `			c -= 'A' - 10;` |
| 11919 | 1486 | `		}else if( c >= 'a' && c <= 'z' ){` |
| 11919 | 1487 | `			c -= 'a' - 10;` |
|  5974 | 1488 | `		}else{` |
|   ! 0 | 1489 | `			bIgnored = 1;` |
|   ! 0 | 1490 | `			continue; /* Not a digit character: skip */` |
|     - | 1491 | `		}` |
| 49857 | 1492 | `		if( c >= base ){` |
|    14 | 1493 | `			bIgnored = 1;` |
|    14 | 1494 | `			continue; /* Digit out of range for this base: skip */` |
|     - | 1495 | `		}` |
| 49844 | 1496 | `		if( mode == 0 ){` |
| 49844 | 1497 | `			if( num < cutoff \|\| (num == cutoff && c <= cutlim) ){` |
| 49838 | 1498 | `				num = num * base + c;` |
| 49838 | 1499 | `				continue;` |
|     - | 1500 | `			}` |
|     - | 1501 | `			/* Adding this digit would overflow the 64-bit integer: fall back to` |
|     - | 1502 | `			 * float accumulation, seeding it with the value gathered so far. */` |
|     7 | 1503 | `			fnum = (double)num;` |
|     7 | 1504 | `			mode = 1;` |
|     3 | 1505 | `		}` |
|     7 | 1506 | `		fnum = fnum * base + c;` |
|     4 | 1507 | `	}` |
| 24721 | 1508 | `	if( bIgnored ){` |
|     - | 1509 | `		/* php 8 skips characters that are not valid digits for this base and only` |
|     - | 1510 | `		 * DEPRECATES the skipping; §10 rejects the deprecated surface loudly, so this` |
|     - | 1511 | `		 * ValueError ABORTS the call (the result stored below never reaches the caller` |
|     - | 1512 | `		 * — the OP_CALL boundary reports the throw for us, VmHostFuncThrowRc).` |
|     - | 1513 | `		 * Twin-pinned by base_invalid_chars_abort{,_zend}.phpt. */` |
|    10 | 1514 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1515 | `			"Invalid characters passed for attempted conversion");` |
|    10 | 1516 | `		return;` |
|     - | 1517 | `	}` |
| 24712 | 1518 | `	if( mode == 1 ){` |
|     7 | 1519 | `		ph7_result_double(pCtx,fnum);` |
|     4 | 1520 | `	}else{` |
| 24706 | 1521 | `		ph7_result_int64(pCtx,num);` |
|     - | 1522 | `	}` |
| 12539 | 1523 | `}` |
|     - | 1524 | `/*` |
|     - | 1525 | ` * int64 hexdec(string $hex_string)` |
|     - | 1526 | ` *  Hexadecimal to decimal.` |
|     - | 1527 | ` * Parameters` |
|     - | 1528 | ` *  $hex_string` |
|     - | 1529 | ` *   The hexadecimal string to convert` |
|     - | 1530 | ` * Return` |
|     - | 1531 | ` *  The decimal representation of hex_string (int, or float on overflow)` |
|     - | 1532 | ` */` |
| 24678 | 1533 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1534 | `{` |
|     - | 1535 | `	const char *zString;` |
|     - | 1536 | `	int nLen;` |
| 24681 | 1537 | `	if( nArg < 1 ){` |
|     - | 1538 | `		/* Missing arguments,return -1 */` |
|   ! 0 | 1539 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1540 | `		return PH7_OK;` |
|     - | 1541 | `	}` |
| 24681 | 1542 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|     - | 1543 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|     - | 1544 | `		char zBuf[64];` |
|   ! 0 | 1545 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1546 | `			"hexdec(): Argument #1 ($hex_string) must be of type string, %s given",` |
|   ! 0 | 1547 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|     - | 1548 | `	}` |
|     - | 1549 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|     - | 1550 | `	 * hex-parses that (hexdec(255) == hexdec("255") == 0x255), so route every` |
|     - | 1551 | `	 * non-throwing value through ph7_value_to_string rather than reading it as` |
|     - | 1552 | `	 * a decimal integer. */` |
| 24681 | 1553 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
| 24681 | 1554 | `	MathBaseToNumber(pCtx,zString,nLen,16);` |
| 24681 | 1555 | `	return PH7_OK;` |
| 12519 | 1556 | `}` |
|     - | 1557 | `/*` |
|     - | 1558 | ` * int64 bindec(string $bin_string)` |
|     - | 1559 | ` *  Binary to decimal.` |
|     - | 1560 | ` * Parameters` |
|     - | 1561 | ` *  $bin_string` |
|     - | 1562 | ` *   The binary string to convert` |
|     - | 1563 | ` * Return` |
|     - | 1564 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|     - | 1565 | ` */` |
|    22 | 1566 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1567 | `{` |
|     - | 1568 | `	const char *zString;` |
|     - | 1569 | `	int nLen;` |
|    23 | 1570 | `	if( nArg < 1 ){` |
|     - | 1571 | `		/* Missing arguments,return -1 */` |
|   ! 0 | 1572 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1573 | `		return PH7_OK;` |
|     - | 1574 | `	}` |
|    23 | 1575 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|     - | 1576 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|     - | 1577 | `		char zBuf[64];` |
|   ! 0 | 1578 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1579 | `			"bindec(): Argument #1 ($binary_string) must be of type string, %s given",` |
|   ! 0 | 1580 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|     - | 1581 | `	}` |
|     - | 1582 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|     - | 1583 | `	 * binary-parses that (bindec(11) == bindec("11") == 3). */` |
|    23 | 1584 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    23 | 1585 | `	MathBaseToNumber(pCtx,zString,nLen,2);` |
|    23 | 1586 | `	return PH7_OK;` |
|    12 | 1587 | `}` |
|     - | 1588 | `/*` |
|     - | 1589 | ` * int64 octdec(string $oct_string)` |
|     - | 1590 | ` *  Octal to decimal.` |
|     - | 1591 | ` * Parameters` |
|     - | 1592 | ` *  $oct_string` |
|     - | 1593 | ` *   The octal string to convert` |
|     - | 1594 | ` * Return` |
|     - | 1595 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|     - | 1596 | ` */` |
|    18 | 1597 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1598 | `{` |
|     - | 1599 | `	const char *zString;` |
|     - | 1600 | `	int nLen;` |
|    19 | 1601 | `	if( nArg < 1 ){` |
|     - | 1602 | `		/* Missing arguments,return -1 */` |
|   ! 0 | 1603 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 1604 | `		return PH7_OK;` |
|     - | 1605 | `	}` |
|    19 | 1606 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|     - | 1607 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|     - | 1608 | `		char zBuf[64];` |
|   ! 0 | 1609 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1610 | `			"octdec(): Argument #1 ($octal_string) must be of type string, %s given",` |
|   ! 0 | 1611 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|     - | 1612 | `	}` |
|     - | 1613 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|     - | 1614 | `	 * octal-parses that (octdec(11) == octdec("11") == 9). */` |
|    19 | 1615 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 1616 | `	MathBaseToNumber(pCtx,zString,nLen,8);` |
|    19 | 1617 | `	return PH7_OK;` |
|    10 | 1618 | `}` |
|     - | 1619 | `/*` |
|     - | 1620 | ` * srand([int $seed])` |
|     - | 1621 | ` * mt_srand([int $seed])` |
|     - | 1622 | ` *  Seed the random number generator.` |
|     - | 1623 | ` * Parameters` |
|     - | 1624 | ` * $seed` |
|     - | 1625 | ` *  Optional seed value. php truncates it to 32 bits; a missing seed reseeds` |
|     - | 1626 | ` *  from OS entropy (a "random" seed), matching php's GENERATE_SEED().` |
|     - | 1627 | ` * Return` |
|     - | 1628 | ` *  null.` |
|     - | 1629 | ` * Note:` |
|     - | 1630 | ` *  srand()/mt_srand() are aliases (php 7.1+ backs both rand() and mt_rand()` |
|     - | 1631 | ` *  with the same MT19937). They reset only the userland generator, never the` |
|     - | 1632 | ` *  engine's internal RC4 entropy, so a seed makes rand()/mt_rand()/shuffle/` |
|     - | 1633 | ` *  str_shuffle/array_rand reproducible without disturbing object ids or uniqid.` |
|     - | 1634 | ` */` |
|   314 | 1635 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1636 | `{` |
|     - | 1637 | `	sxu32 nSeed;` |
|   316 | 1638 | `	int bLegacy = nArg > 1 && ph7_value_to_int64(apArg[1]) == PH7_MT_RAND_PHP;` |
|   316 | 1639 | `	if( bLegacy ){` |
|     - | 1640 | `		/* php 8.3 deprecated the legacy generator; the message carries no` |
|     - | 1641 | `		 * function prefix there. */` |
|    23 | 1642 | `		PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|     - | 1643 | `			"The MT_RAND_PHP variant of Mt19937 is deprecated");` |
|    11 | 1644 | `	}` |
|   316 | 1645 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     - | 1646 | `		/* php truncates the (weakly int-coerced) seed to 32 bits. */` |
|   310 | 1647 | `		nSeed = (sxu32)ph7_value_to_int64(apArg[0]);` |
|   156 | 1648 | `	}else{` |
|     - | 1649 | `		/* NULL is the declared default and means "no seed given": php reseeds` |
|     - | 1650 | `		 * from entropy for it, where this read it as the integer 0 — so` |
|     - | 1651 | ``		 * `mt_srand($cfg['seed'] ?? null)` pinned every run to one sequence. */`` |
|     - | 1652 | `		/* No seed: reseed from OS entropy, like php's GENERATE_SEED(). */` |
|     8 | 1653 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|   ! 0 | 1654 | `			nSeed = PH7_VmRandomNum(pCtx->pVm);` |
|   ! 0 | 1655 | `		}` |
|     - | 1656 | `	}` |
|     - | 1657 | `	/* $mode picks the GENERATOR, and php reads it as an equality test against` |
|     - | 1658 | `	 * MT_RAND_PHP alone: every other value, valid or not, is MT19937. It was` |
|     - | 1659 | `	 * declared in the signature and read by nothing, so a program that seeded` |
|     - | 1660 | `	 * with MT_RAND_PHP to reproduce a recorded sequence silently got a different` |
|     - | 1661 | `	 * one — and the constant naming it was undefined, so the call was a fatal. */` |
|   316 | 1662 | `	PH7_VmMtSrand(pCtx->pVm,nSeed,bLegacy);` |
|   316 | 1663 | `	ph7_result_null(pCtx);` |
|   316 | 1664 | `	return PH7_OK;` |
|     2 | 1665 | `}` |
|     - | 1666 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - | 1667 | `/*` |
|     - | 1668 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|     - | 1669 | ` *  Convert a number between arbitrary bases.` |
|     - | 1670 | ` * Parameters` |
|     - | 1671 | ` * $number` |
|     - | 1672 | ` *  The number to convert` |
|     - | 1673 | ` * $frombase` |
|     - | 1674 | ` *  The base number is in` |
|     - | 1675 | ` * $tobase` |
|     - | 1676 | ` *  The base to convert number to` |
|     - | 1677 | ` * Return` |
|     - | 1678 | ` *  Number converted to base tobase` |
|     - | 1679 | ` */` |
|    60 | 1680 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1681 | `{` |
|     - | 1682 | `	static const char zDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";` |
|     - | 1683 | `	int nLen,iFbase,iTobase,i;` |
|     - | 1684 | `	int bIgnored;` |
|     - | 1685 | `	ph7_int64 iFbase64,iTobase64;` |
|     - | 1686 | `	const char *zNum;` |
|    62 | 1687 | `	sxu64 uNum = 0;` |
|    62 | 1688 | `	if( nArg < 3 ){` |
|     - | 1689 | `		/* Return the empty string*/` |
|   ! 0 | 1690 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1691 | `		return PH7_OK;` |
|     - | 1692 | `	}` |
|     - | 1693 | `	/* Base numbers. Read them as 64-bit so an out-of-range base can't wrap through` |
|     - | 1694 | `	 * a 32-bit truncation back into the 2..36 window and bypass the check below. */` |
|    62 | 1695 | `	iFbase64 = ph7_value_to_int64(apArg[1]);` |
|    62 | 1696 | `	iTobase64 = ph7_value_to_int64(apArg[2]);` |
|     - | 1697 | `	/* PHP 8 throws a catchable ValueError for a base outside 2..36; from_base` |
|     - | 1698 | `	 * is validated before to_base, both before the string is even parsed. */` |
|    62 | 1699 | `	if( iFbase64 < 2 \|\| iFbase64 > 36 ){` |
|     7 | 1700 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1701 | `			"base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)");` |
|     - | 1702 | `	}` |
|    56 | 1703 | `	if( iTobase64 < 2 \|\| iTobase64 > 36 ){` |
|     5 | 1704 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1705 | `			"base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)");` |
|     - | 1706 | `	}` |
|     - | 1707 | `	/* Both bases are now known to fit in [2,36], so the int form is exact. */` |
|    52 | 1708 | `	iFbase  = (int)iFbase64;` |
|    52 | 1709 | `	iTobase = (int)iTobase64;` |
|     - | 1710 | `	/* Parse the input number in from_base. Every base is handled the same way:` |
|     - | 1711 | `	 * digits 0-9 then a-z/A-Z map to 0-35; a character that is not a valid digit for` |
|     - | 1712 | `	 * from_base is ignored, and php raises an E_DEPRECATED saying so. */` |
|    52 | 1713 | `	if( ph7_value_is_null(apArg[0]) ){` |
|   ! 0 | 1714 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1715 | `			"base_convert(): Argument #1 ($num) must be of type string, null given");` |
|     - | 1716 | `	}` |
|    52 | 1717 | `	zNum = ph7_value_to_string(apArg[0],&nLen);` |
|    52 | 1718 | `	bIgnored = 0;` |
|   162 | 1719 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   112 | 1720 | `		int c = (unsigned char)zNum[i];` |
|     - | 1721 | `		int d;` |
|   112 | 1722 | `		if( c >= '0' && c <= '9' ){` |
|    80 | 1723 | `			d = c - '0';` |
|    73 | 1724 | `		}else if( c >= 'a' && c <= 'z' ){` |
|    34 | 1725 | `			d = c - 'a' + 10;` |
|    16 | 1726 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|   ! 0 | 1727 | `			d = c - 'A' + 10;` |
|   ! 0 | 1728 | `		}else{` |
|   ! 0 | 1729 | `			d = 99;` |
|     - | 1730 | `		}` |
|   112 | 1731 | `		if( d >= iFbase ){` |
|     - | 1732 | `			/* Not a valid digit for this base: php skips it and deprecates the skip. */` |
|     6 | 1733 | `			bIgnored = 1;` |
|     6 | 1734 | `			continue;` |
|     - | 1735 | `		}` |
|   108 | 1736 | `		uNum = uNum * (sxu64)iFbase + (sxu64)d;` |
|    55 | 1737 | `	}` |
|    52 | 1738 | `	if( bIgnored ){` |
|     - | 1739 | `		/* §10 rejects php's deprecated surface loudly, and a throw ABORTS the call —` |
|     - | 1740 | `		 * the conversion below is not reached. See MathBaseToNumber's twin. */` |
|     6 | 1741 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1742 | `			"Invalid characters passed for attempted conversion");` |
|     - | 1743 | `	}` |
|     - | 1744 | `	/* Format the result in to_base using lowercase digits. */` |
|    48 | 1745 | `	if( uNum == 0 ){` |
|     5 | 1746 | `		ph7_result_string(pCtx,"0",1);` |
|     3 | 1747 | `	}else{` |
|     - | 1748 | `		char zOut[70]; /* base-2 of a 64-bit value fits in 64 digits */` |
|    44 | 1749 | `		int n = 0,j;` |
|   142 | 1750 | `		while( uNum > 0 ){` |
|   100 | 1751 | `			zOut[n++] = zDigits[uNum % (sxu64)iTobase];` |
|   100 | 1752 | `			uNum /= (sxu64)iTobase;` |
|     2 | 1753 | `		}` |
|     - | 1754 | `		/* Digits were produced least-significant first: reverse in place. */` |
|    84 | 1755 | `		for( j = 0 ; j < n/2 ; ++j ){` |
|    42 | 1756 | `			char t = zOut[j];` |
|    42 | 1757 | `			zOut[j] = zOut[n - 1 - j];` |
|    42 | 1758 | `			zOut[n - 1 - j] = t;` |
|    22 | 1759 | `		}` |
|    44 | 1760 | `		ph7_result_string(pCtx,zOut,n);` |
|     - | 1761 | `	}` |
|    48 | 1762 | `	return PH7_OK;` |
|    32 | 1763 | `}` |
|     - | 1764 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     - | 1765 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 1766 |  |
