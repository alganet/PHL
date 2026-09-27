# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 674/803 lines (83.94%)

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
|    6 |   66 | `PH7_PRIVATE int PH7_builtin_rad2deg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |   67 | `{` |
|    - |   68 | `	double x;` |
|    8 |   69 | `	if( nArg < 1 ){` |
|  ! 0 |   70 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   71 | `		return PH7_OK;` |
|    - |   72 | `	}` |
|    8 |   73 | `	x = ph7_value_to_double(apArg[0]);` |
|    8 |   74 | `	ph7_result_double(pCtx,x * (180.0 / 3.14159265358979323846));` |
|    8 |   75 | `	return PH7_OK;` |
|    5 |   76 | `}` |
|    4 |   77 | `PH7_PRIVATE int PH7_builtin_fpow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |   78 | `{` |
|    - |   79 | `	double x,y;` |
|    6 |   80 | `	if( nArg < 2 ){` |
|  ! 0 |   81 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   82 | `		return PH7_OK;` |
|    - |   83 | `	}` |
|    6 |   84 | `	x = ph7_value_to_double(apArg[0]);` |
|    6 |   85 | `	y = ph7_value_to_double(apArg[1]);` |
|    6 |   86 | `	ph7_result_double(pCtx,pow(x,y));` |
|    6 |   87 | `	return PH7_OK;` |
|    4 |   88 | `}` |
|   16 |   89 | `PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   90 | `{` |
|    - |   91 | `	double r,x;` |
|   17 |   92 | `	if( nArg < 1 ){` |
|    - |   93 | `		/* Missing argument,return 0 */` |
|  ! 0 |   94 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   95 | `		return PH7_OK;` |
|    - |   96 | `	}` |
|   17 |   97 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |   98 | `	/* Perform the requested operation */` |
|   17 |   99 | `	r = sqrt(x);` |
|    - |  100 | `	/* store the result back */` |
|   17 |  101 | `	ph7_result_double(pCtx,r);` |
|   17 |  102 | `	return PH7_OK;` |
|    9 |  103 | `}` |
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
|   22 |  135 | `PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  136 | `{` |
|    - |  137 | `	double r,x;` |
|    - |  138 | `	/* PHP requires exactly one argument. */` |
|   23 |  139 | `	if( nArg != 1 ){` |
|  ! 0 |  140 | `		return PH7_VmThrowException(pCtx,` |
|    - |  141 | `			"ArgumentCountError",` |
|    - |  142 | `			"floor() expects exactly 1 argument, %d given",` |
|  ! 0 |  143 | `			nArg` |
|    - |  144 | `			);` |
|    - |  145 | `	}` |
|    - |  146 | ``	/* The `int\|float $num` row in aBuiltinSig[] screens this argument before the`` |
|    - |  147 | `	 * call: array, object, resource and non-numeric string are refused there,` |
|    - |  148 | `	 * with php's wording. The hand-rolled copy that used to sit here refused a` |
|    - |  149 | `	 * BOOL as well, which weak mode converts (php: ceil(true) is float(1)), and` |
|    - |  150 | `	 * one of its two branches had no ", %s given" tail at all. */` |
|    - |  151 |  |
|   23 |  152 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  153 | `	/* Perform the requested operation */` |
|   23 |  154 | `	r = floor(x);` |
|    - |  155 | `	/* store the result back */` |
|   23 |  156 | `	ph7_result_double(pCtx,r);` |
|   23 |  157 | `	return PH7_OK;` |
|   12 |  158 | `}` |
|    - |  159 | `/*` |
|    - |  160 | ` * float cos(float $arg )` |
|    - |  161 | ` *  Cosine.` |
|    - |  162 | ` * Parameter` |
|    - |  163 | ` *  The number to process.` |
|    - |  164 | ` * Return` |
|    - |  165 | ` *  The cosine of arg.` |
|    - |  166 | ` */` |
|    2 |  167 | `PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  168 | `{` |
|    - |  169 | `	double r,x;` |
|    3 |  170 | `	if( nArg < 1 ){` |
|    - |  171 | `		/* Missing argument,return 0 */` |
|  ! 0 |  172 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  173 | `		return PH7_OK;` |
|    - |  174 | `	}` |
|    3 |  175 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  176 | `	/* Perform the requested operation */` |
|    3 |  177 | `	r = cos(x);` |
|    - |  178 | `	/* store the result back */` |
|    3 |  179 | `	ph7_result_double(pCtx,r);` |
|    3 |  180 | `	return PH7_OK;` |
|    2 |  181 | `}` |
|    - |  182 | `/*` |
|    - |  183 | ` * float acos(float $arg )` |
|    - |  184 | ` *  Arc cosine.` |
|    - |  185 | ` * Parameter` |
|    - |  186 | ` *  The number to process.` |
|    - |  187 | ` * Return` |
|    - |  188 | ` *  The arc cosine of arg.` |
|    - |  189 | ` */` |
|   16 |  190 | `PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  191 | `{` |
|    - |  192 | `	double r, x;` |
|    - |  193 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|   17 |  194 | `	if( nArg != 1 ){` |
|  ! 0 |  195 | `		return PH7_VmThrowException(pCtx,` |
|    - |  196 | `			"ArgumentCountError",` |
|    - |  197 | `			"acos() expects exactly 1 argument, %d given",` |
|  ! 0 |  198 | `			nArg` |
|    - |  199 | `			);` |
|    - |  200 | `	}` |
|    - |  201 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|    - |  202 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|    - |  203 | `	 * the float conversion will handle them. */` |
|   17 |  204 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|  ! 0 |  205 | `		return PH7_VmThrowException(pCtx,` |
|    - |  206 | `			"TypeError",` |
|    - |  207 | `			"acos(): Argument #1 ($num) must be of type float, %s given",` |
|  ! 0 |  208 | `			ph7_type_name(apArg[0])` |
|    - |  209 | `			);` |
|    - |  210 | `	}` |
|    - |  211 | `	/* Convert to double now that we know it's numeric. */` |
|   17 |  212 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  213 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|   17 |  214 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|    5 |  215 | `		r = PH7_NAN_VALUE();` |
|    3 |  216 | `	}else{` |
|   13 |  217 | `		r = acos(x);` |
|    - |  218 | `	}` |
|    - |  219 | `	/* store the result back */` |
|   17 |  220 | `	ph7_result_double(pCtx,r);` |
|   17 |  221 | `	return PH7_OK;` |
|    9 |  222 | `}` |
|    - |  223 | `/*` |
|    - |  224 | ` * float cosh(float $arg )` |
|    - |  225 | ` *  Hyperbolic cosine.` |
|    - |  226 | ` * Parameter` |
|    - |  227 | ` *  The number to process.` |
|    - |  228 | ` * Return` |
|    - |  229 | ` *  The hyperbolic cosine of arg.` |
|    - |  230 | ` */` |
|   16 |  231 | `PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  232 | `{` |
|    - |  233 | `	double r,x;` |
|   17 |  234 | `	if( nArg < 1 ){` |
|    - |  235 | `		/* Missing argument,return 0 */` |
|  ! 0 |  236 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  237 | `		return PH7_OK;` |
|    - |  238 | `	}` |
|   17 |  239 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  240 | `	/* Perform the requested operation */` |
|   17 |  241 | `	r = cosh(x);` |
|    - |  242 | `	/* store the result back */` |
|   17 |  243 | `	ph7_result_double(pCtx,r);` |
|   17 |  244 | `	return PH7_OK;` |
|    9 |  245 | `}` |
|    - |  246 | `/*` |
|    - |  247 | ` * float sin(float $arg )` |
|    - |  248 | ` *  Sine.` |
|    - |  249 | ` * Parameter` |
|    - |  250 | ` *  The number to process.` |
|    - |  251 | ` * Return` |
|    - |  252 | ` *  The sine of arg.` |
|    - |  253 | ` */` |
|    2 |  254 | `PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  255 | `{` |
|    - |  256 | `	double r,x;` |
|    3 |  257 | `	if( nArg < 1 ){` |
|    - |  258 | `		/* Missing argument,return 0 */` |
|  ! 0 |  259 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  260 | `		return PH7_OK;` |
|    - |  261 | `	}` |
|    3 |  262 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  263 | `	/* Perform the requested operation */` |
|    3 |  264 | `	r = sin(x);` |
|    - |  265 | `	/* store the result back */` |
|    3 |  266 | `	ph7_result_double(pCtx,r);` |
|    3 |  267 | `	return PH7_OK;` |
|    2 |  268 | `}` |
|    - |  269 | `/*` |
|    - |  270 | ` * float asin(float $arg )` |
|    - |  271 | ` *  Arc sine.` |
|    - |  272 | ` * Parameter` |
|    - |  273 | ` *  The number to process.` |
|    - |  274 | ` * Return` |
|    - |  275 | ` *  The arc sine of arg.` |
|    - |  276 | ` */` |
|   16 |  277 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  278 | `{` |
|    - |  279 | `	double r, x;` |
|    - |  280 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|   17 |  281 | `	if( nArg != 1 ){` |
|  ! 0 |  282 | `		return PH7_VmThrowException(pCtx,` |
|    - |  283 | `			"ArgumentCountError",` |
|    - |  284 | `			"asin() expects exactly 1 argument, %d given",` |
|  ! 0 |  285 | `			nArg` |
|    - |  286 | `			);` |
|    - |  287 | `	}` |
|    - |  288 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|    - |  289 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|    - |  290 | `	 * the float conversion will handle them. */` |
|   17 |  291 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|  ! 0 |  292 | `		return PH7_VmThrowException(pCtx,` |
|    - |  293 | `			"TypeError",` |
|    - |  294 | `			"asin(): Argument #1 ($num) must be of type float, %s given",` |
|  ! 0 |  295 | `			ph7_type_name(apArg[0])` |
|    - |  296 | `			);` |
|    - |  297 | `	}` |
|    - |  298 | `	/* Convert to double now that we know it's numeric. */` |
|   17 |  299 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  300 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|   17 |  301 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|    5 |  302 | `		r = PH7_NAN_VALUE();` |
|    3 |  303 | `	}else{` |
|   13 |  304 | `		r = asin(x);` |
|    - |  305 | `	}` |
|    - |  306 | `	/* store the result back */` |
|   17 |  307 | `	ph7_result_double(pCtx,r);` |
|   17 |  308 | `	return PH7_OK;` |
|    9 |  309 | `}` |
|    - |  310 | `/*` |
|    - |  311 | ` * float sinh(float $arg )` |
|    - |  312 | ` *  Hyperbolic sine.` |
|    - |  313 | ` * Parameter` |
|    - |  314 | ` *  The number to process.` |
|    - |  315 | ` * Return` |
|    - |  316 | ` *  The hyperbolic sine of arg.` |
|    - |  317 | ` */` |
|   18 |  318 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  319 | `{` |
|    - |  320 | `	double r,x;` |
|   19 |  321 | `	if( nArg < 1 ){` |
|    - |  322 | `		/* Missing argument,return 0 */` |
|  ! 0 |  323 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  324 | `		return PH7_OK;` |
|    - |  325 | `	}` |
|   19 |  326 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  327 | `	/* Perform the requested operation */` |
|   19 |  328 | `	r = sinh(x);` |
|    - |  329 | `	/* store the result back */` |
|   19 |  330 | `	ph7_result_double(pCtx,r);` |
|   19 |  331 | `	return PH7_OK;` |
|   10 |  332 | `}` |
|    - |  333 | `/*` |
|    - |  334 | ` * float ceil(float $arg )` |
|    - |  335 | ` *  Round fractions up.` |
|    - |  336 | ` * Parameter` |
|    - |  337 | ` *  The number to process.` |
|    - |  338 | ` * Return` |
|    - |  339 | ` *  The next highest integer value by rounding up value if necessary.` |
|    - |  340 | ` */` |
|   18 |  341 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  342 | `{` |
|    - |  343 | `	double r,x;` |
|    - |  344 | `	/* PHP requires exactly one argument. */` |
|   19 |  345 | `	if( nArg != 1 ){` |
|  ! 0 |  346 | `		return PH7_VmThrowException(pCtx,` |
|    - |  347 | `			"ArgumentCountError",` |
|    - |  348 | `			"ceil() expects exactly 1 argument, %d given",` |
|  ! 0 |  349 | `			nArg` |
|    - |  350 | `			);` |
|    - |  351 | `	}` |
|    - |  352 | `	/* Type screening is the aBuiltinSig[] row's -- see floor() above. */` |
|    - |  353 |  |
|   19 |  354 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  355 | `	/* Perform the requested operation */` |
|   19 |  356 | `	r = ceil(x);` |
|    - |  357 | `	/* store the result back */` |
|   19 |  358 | `	ph7_result_double(pCtx,r);` |
|   19 |  359 | `	return PH7_OK;` |
|   10 |  360 | `}` |
|    - |  361 | `/*` |
|    - |  362 | ` * float tan(float $arg )` |
|    - |  363 | ` *  Tangent.` |
|    - |  364 | ` * Parameter` |
|    - |  365 | ` *  The number to process.` |
|    - |  366 | ` * Return` |
|    - |  367 | ` *  The tangent of arg.` |
|    - |  368 | ` */` |
|    4 |  369 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  370 | `{` |
|    - |  371 | `	double r,x;` |
|    5 |  372 | `	if( nArg < 1 ){` |
|    - |  373 | `		/* Missing argument,return 0 */` |
|  ! 0 |  374 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  375 | `		return PH7_OK;` |
|    - |  376 | `	}` |
|    5 |  377 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  378 | `	/* Perform the requested operation */` |
|    5 |  379 | `	r = tan(x);` |
|    - |  380 | `	/* store the result back */` |
|    5 |  381 | `	ph7_result_double(pCtx,r);` |
|    5 |  382 | `	return PH7_OK;` |
|    3 |  383 | `}` |
|    - |  384 | `/*` |
|    - |  385 | ` * float atan(float $arg )` |
|    - |  386 | ` *  Arc tangent.` |
|    - |  387 | ` * Parameter` |
|    - |  388 | ` *  The number to process.` |
|    - |  389 | ` * Return` |
|    - |  390 | ` *  The arc tangent of arg.` |
|    - |  391 | ` */` |
|   32 |  392 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  393 | `{` |
|    - |  394 | `	double r,x;` |
|    - |  395 | `	/* PHP enforces exactly one argument. */` |
|   33 |  396 | `	if( nArg != 1 ){` |
|  ! 0 |  397 | `		return PH7_VmThrowException(pCtx,` |
|    - |  398 | `			"ArgumentCountError",` |
|    - |  399 | `			"atan() expects exactly 1 argument, %d given",` |
|  ! 0 |  400 | `			nArg` |
|    - |  401 | `			);` |
|    - |  402 | `	}` |
|    - |  403 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, non-numeric strings).` |
|    - |  404 | `	 * PHP 8 reports a TypeError for wrong types. */` |
|   33 |  405 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|  ! 0 |  406 | `		return PH7_VmThrowException(pCtx,` |
|    - |  407 | `			"TypeError",` |
|    - |  408 | `			"atan(): Argument #1 ($num) must be of type float, %s given",` |
|  ! 0 |  409 | `			ph7_type_name(apArg[0])` |
|    - |  410 | `			);` |
|    - |  411 | `	}` |
|   33 |  412 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  413 | `	/* Perform the requested operation */` |
|   33 |  414 | `	r = atan(x);` |
|    - |  415 | `	/* store the result back */` |
|   33 |  416 | `	ph7_result_double(pCtx,r);` |
|   33 |  417 | `	return PH7_OK;` |
|   17 |  418 | `}` |
|    - |  419 | `/*` |
|    - |  420 | ` * float tanh(float $arg )` |
|    - |  421 | ` *  Hyperbolic tangent.` |
|    - |  422 | ` * Parameter` |
|    - |  423 | ` *  The number to process.` |
|    - |  424 | ` * Return` |
|    - |  425 | ` *  The Hyperbolic tangent of arg.` |
|    - |  426 | ` */` |
|   18 |  427 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  428 | `{` |
|    - |  429 | `	double r,x;` |
|   19 |  430 | `	if( nArg < 1 ){` |
|    - |  431 | `		/* Missing argument,return 0 */` |
|  ! 0 |  432 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  433 | `		return PH7_OK;` |
|    - |  434 | `	}` |
|   19 |  435 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  436 | `	/* Perform the requested operation */` |
|   19 |  437 | `	r = tanh(x);` |
|    - |  438 | `	/* store the result back */` |
|   19 |  439 | `	ph7_result_double(pCtx,r);` |
|   19 |  440 | `	return PH7_OK;` |
|   10 |  441 | `}` |
|    - |  442 | `/*` |
|    - |  443 | ` * float atan2(float $y,float $x)` |
|    - |  444 | ` *  Arc tangent of two variable.` |
|    - |  445 | ` * Parameter` |
|    - |  446 | ` *  $y = Dividend parameter.` |
|    - |  447 | ` *  $x = Divisor parameter.` |
|    - |  448 | ` * Return` |
|    - |  449 | ` *  The arc tangent of y/x in radian.` |
|    - |  450 | ` */` |
|   46 |  451 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  452 | `{` |
|    - |  453 | `	double r,x,y;` |
|    - |  454 | `	/* PHP enforces exactly two arguments. */` |
|   47 |  455 | `	if( nArg != 2 ){` |
|  ! 0 |  456 | `		return PH7_VmThrowException(pCtx,` |
|    - |  457 | `			"ArgumentCountError",` |
|    - |  458 | `			"atan2() expects exactly 2 arguments, %d given",` |
|  ! 0 |  459 | `			nArg` |
|    - |  460 | `			);` |
|    - |  461 | `	}` |
|    - |  462 | `	/* Type checking: reject non-numeric values for $y (argument #1). */` |
|   47 |  463 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|  ! 0 |  464 | `		return PH7_VmThrowException(pCtx,` |
|    - |  465 | `			"TypeError",` |
|    - |  466 | `			"atan2(): Argument #1 ($y) must be of type float, %s given",` |
|  ! 0 |  467 | `			ph7_type_name(apArg[0])` |
|    - |  468 | `			);` |
|    - |  469 | `	}` |
|    - |  470 | `	/* Type checking: reject non-numeric values for $x (argument #2). */` |
|   47 |  471 | `	if( !ph7_value_is_numeric(apArg[1]) ){` |
|  ! 0 |  472 | `		return PH7_VmThrowException(pCtx,` |
|    - |  473 | `			"TypeError",` |
|    - |  474 | `			"atan2(): Argument #2 ($x) must be of type float, %s given",` |
|  ! 0 |  475 | `			ph7_type_name(apArg[1])` |
|    - |  476 | `			);` |
|    - |  477 | `	}` |
|   47 |  478 | `	y = ph7_value_to_double(apArg[0]);` |
|   47 |  479 | `	x = ph7_value_to_double(apArg[1]);` |
|    - |  480 | `	/* Perform the requested operation */` |
|   47 |  481 | `	r = atan2(y,x);` |
|    - |  482 | `	/* store the result back */` |
|   47 |  483 | `	ph7_result_double(pCtx,r);` |
|   47 |  484 | `	return PH7_OK;` |
|   24 |  485 | `}` |
|    - |  486 | `/*` |
|    - |  487 | ` * float/int64 abs(float/int64 $arg )` |
|    - |  488 | ` *  Absolute value.` |
|    - |  489 | ` * Parameter` |
|    - |  490 | ` *  The number to process.` |
|    - |  491 | ` * Return` |
|    - |  492 | ` *  The absolute value of number.` |
|    - |  493 | ` */` |
|  136 |  494 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  495 | `{` |
|    - |  496 | `	int is_float;` |
|    - |  497 | `	/* PHP requires exactly one argument. */` |
|  138 |  498 | `	if( nArg != 1 ){` |
|  ! 0 |  499 | `		return PH7_VmThrowException(pCtx,` |
|    - |  500 | `			"ArgumentCountError",` |
|    - |  501 | `			"abs() expects exactly 1 argument, %d given",` |
|  ! 0 |  502 | `			nArg` |
|    - |  503 | `			);` |
|    - |  504 | `	}` |
|    - |  505 |  |
|  138 |  506 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    - |  507 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|  ! 0 |  508 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - |  509 | `			"abs(): Argument #1 ($num) must be of type int\|float, null given");` |
|    - |  510 | `	}` |
|    - |  511 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|  138 |  512 | `	is_float = ph7_value_is_float(apArg[0]);` |
|  138 |  513 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|    - |  514 | `		int len;` |
|    9 |  515 | `		sxu8 bReal = FALSE;` |
|    9 |  516 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  517 | `		sxi32 rcNum;` |
|    9 |  518 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|    9 |  519 | `		if( rcNum != SXRET_OK ){` |
|  ! 0 |  520 | `			return PH7_VmThrowException(pCtx,` |
|    - |  521 | `				"TypeError",` |
|    - |  522 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  523 | `				);` |
|    - |  524 | `		}` |
|    9 |  525 | `		if( bReal ){` |
|    7 |  526 | `			is_float = 1;` |
|    3 |  527 | `		}` |
|    4 |  528 | `	}` |
|  138 |  529 | `	if( is_float ){` |
|    - |  530 | `		double r,x;` |
|  103 |  531 | `		x = ph7_value_to_double(apArg[0]);` |
|    - |  532 | `		/* Perform the requested operation */` |
|  103 |  533 | `		r = fabs(x);` |
|  103 |  534 | `		ph7_result_double(pCtx,r);` |
|   52 |  535 | `	}else{` |
|    - |  536 | ``		/* Read the full 64-bit value (the old 32-bit `int abs()` truncated any`` |
|    - |  537 | `		 * magnitude above 2^31 and was UB on INT_MIN). */` |
|   36 |  538 | `		sxi64 x = ph7_value_to_int64(apArg[0]);` |
|   36 |  539 | `		if( x == SMALLEST_INT64 ){` |
|    - |  540 | `			/* abs(PHP_INT_MIN) has no int representation, so PHP returns a float. */` |
|    3 |  541 | `			ph7_result_double(pCtx,-(double)x);` |
|    2 |  542 | `		}else{` |
|   34 |  543 | `			ph7_result_int64(pCtx,x < 0 ? -x : x);` |
|    - |  544 | `		}` |
|    - |  545 | `	}` |
|  138 |  546 | `	return PH7_OK;` |
|   70 |  547 | `}` |
|    - |  548 | `/*` |
|    - |  549 | ` * float log(float $arg,[int/float $base])` |
|    - |  550 | ` *  Natural logarithm.` |
|    - |  551 | ` * Parameter` |
|    - |  552 | ` *  $arg: The number to process.` |
|    - |  553 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|    - |  554 | ` * Return` |
|    - |  555 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|    - |  556 | ` * Note:` |
|    - |  557 | ` *  only Natural log and base-10 log are supported.` |
|    - |  558 | ` */` |
|   12 |  559 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  560 | `{` |
|    - |  561 | `	double r,x;` |
|   13 |  562 | `	if( nArg < 1 ){` |
|    - |  563 | `		/* Missing argument,return 0 */` |
|  ! 0 |  564 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  565 | `		return PH7_OK;` |
|    - |  566 | `	}` |
|   13 |  567 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  568 | `	/* Perform the requested operation */` |
|   13 |  569 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|    - |  570 | `		/* Base-10 log */` |
|    5 |  571 | `		r = log10(x);` |
|    3 |  572 | `	}else{` |
|    9 |  573 | `		r = log(x);` |
|    - |  574 | `	}` |
|    - |  575 | `	/* store the result back */` |
|   13 |  576 | `	ph7_result_double(pCtx,r);` |
|   13 |  577 | `	return PH7_OK;` |
|    7 |  578 | `}` |
|    - |  579 | `/*` |
|    - |  580 | ` * float log10(float $arg )` |
|    - |  581 | ` *  Base-10 logarithm.` |
|    - |  582 | ` * Parameter` |
|    - |  583 | ` *  The number to process.` |
|    - |  584 | ` * Return` |
|    - |  585 | ` *  The Base-10 logarithm of the given number.` |
|    - |  586 | ` */` |
|   14 |  587 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  588 | `{` |
|    - |  589 | `	double r,x;` |
|   15 |  590 | `	if( nArg < 1 ){` |
|    - |  591 | `		/* Missing argument,return 0 */` |
|  ! 0 |  592 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  593 | `		return PH7_OK;` |
|    - |  594 | `	}` |
|   15 |  595 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  596 | `	/* Perform the requested operation */` |
|   15 |  597 | `	r = log10(x);` |
|    - |  598 | `	/* store the result back */` |
|   15 |  599 | `	ph7_result_double(pCtx,r);` |
|   15 |  600 | `	return PH7_OK;` |
|    8 |  601 | `}` |
|    - |  602 | `/*` |
|    - |  603 | ` * mixed pow(mixed $num,mixed $exponent)` |
|    - |  604 | ` *  Exponential expression.` |
|    - |  605 | ` *` |
|    - |  606 | `` *  php does not implement pow() separately: the function and the `**` operator`` |
|    - |  607 | ` *  are the same ZEND_API pow_function, so they share an operand contract, a` |
|    - |  608 | ` *  result TYPE rule and every edge value. Reading the two arguments as doubles` |
|    - |  609 | `` *  and returning pow() shared none of it -- `pow(2,3)` answered float(8) where`` |
|    - |  610 | `` *  `2 ** 3` answers int(8) (a wrong TYPE for the most ordinary call there is),`` |
|    - |  611 | `` *  and the contract `**` enforces was absent entirely: pow('abc',2) answered`` |
|    - |  612 | ` *  float(0), pow([1],2) float(1) and pow($obj,2) float(1) after a conversion` |
|    - |  613 | ` *  warning, where every one of them is` |
|    - |  614 | `` *  `TypeError: Unsupported operand types: … ** int`.`` |
|    - |  615 | ` *` |
|    - |  616 | ` *  Both halves now come from the operator: VmArithOperandCheck() for the` |
|    - |  617 | ` *  contract (including the "A non-numeric value encountered" warning a` |
|    - |  618 | ` *  leading-numeric string gets before it computes with the prefix) and` |
|    - |  619 | ` *  PH7_MemObjPow() for the arithmetic.` |
|    - |  620 | ` * Return` |
|    - |  621 | ` *  base raised to the power of exp -- int when both operands are int, the` |
|    - |  622 | ` *  exponent is non-negative and the exact result fits in an int64; float` |
|    - |  623 | ` *  otherwise.` |
|    - |  624 | ` */` |
|   52 |  625 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  626 | `{` |
|   55 |  627 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - |  628 | `	ph7_value sBase,sExp;` |
|    - |  629 | `	SyBlob sMsg;` |
|    - |  630 | `	sxi32 rc;` |
|    - |  631 | `	/* Arity (exactly 2) is enforced from aBuiltinArity[] before the call. */` |
|   55 |  632 | `	if( nArg < 2 ){` |
|  ! 0 |  633 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  634 | `		return PH7_OK;` |
|    - |  635 | `	}` |
|   55 |  636 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   55 |  637 | `	if( VmArithOperandCheck(pVm,apArg[0],apArg[1],"**",&sMsg) != SXRET_OK ){` |
|   19 |  638 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|   12 |  639 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|   13 |  640 | `		SyBlobRelease(&sMsg);` |
|   13 |  641 | `		return rc;` |
|    - |  642 | `	}` |
|   43 |  643 | `	SyBlobRelease(&sMsg);` |
|    - |  644 | `	/* Work on COPIES: PH7_MemObjPow converts its operands in place, which the` |
|    - |  645 | `	 * opcode arm may do to its stack slots but a builtin may not do to the` |
|    - |  646 | `	 * caller's arguments. */` |
|   43 |  647 | `	PH7_MemObjInit(pVm,&sBase);` |
|   43 |  648 | `	PH7_MemObjInit(pVm,&sExp);` |
|   43 |  649 | `	PH7_MemObjLoad(apArg[0],&sBase);` |
|   43 |  650 | `	PH7_MemObjLoad(apArg[1],&sExp);` |
|   43 |  651 | `	PH7_MemObjPow(&sBase,&sExp,&sBase);` |
|   43 |  652 | `	if( (sBase.iFlags & MEMOBJ_REAL) != 0 ){` |
|   15 |  653 | `		ph7_result_double(pCtx,sBase.rVal);` |
|    8 |  654 | `	}else{` |
|   29 |  655 | `		ph7_result_int64(pCtx,sBase.x.iVal);` |
|    - |  656 | `	}` |
|   43 |  657 | `	PH7_MemObjRelease(&sBase);` |
|   43 |  658 | `	PH7_MemObjRelease(&sExp);` |
|   43 |  659 | `	return PH7_OK;` |
|   29 |  660 | `}` |
|    - |  661 | `/*` |
|    - |  662 | ` * float pi(void)` |
|    - |  663 | ` *  Returns an approximation of pi.` |
|    - |  664 | ` * Note` |
|    - |  665 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|    - |  666 | ` * Return` |
|    - |  667 | ` *  The value of pi as float.` |
|    - |  668 | ` */` |
|    4 |  669 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  670 | `{` |
|    2 |  671 | `	SXUNUSED(nArg); /* cc warning */` |
|    2 |  672 | `	SXUNUSED(apArg);` |
|    6 |  673 | `	ph7_result_double(pCtx,PH7_PI);` |
|    6 |  674 | `	return PH7_OK;` |
|    2 |  675 | `}` |
|    - |  676 | `/*` |
|    - |  677 | ` * float fmod(float $x,float $y)` |
|    - |  678 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|    - |  679 | ` * Parameters` |
|    - |  680 | ` * $x` |
|    - |  681 | ` *  The dividend` |
|    - |  682 | ` * $y` |
|    - |  683 | ` *  The divisor` |
|    - |  684 | ` * Return` |
|    - |  685 | ` *  The floating point remainder of x/y.` |
|    - |  686 | ` */` |
|    2 |  687 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  688 | `{` |
|    - |  689 | `	double x,y,r;` |
|    3 |  690 | `	if( nArg < 2 ){` |
|    - |  691 | `		/* Missing arguments */` |
|  ! 0 |  692 | `		ph7_result_double(pCtx,0);` |
|  ! 0 |  693 | `		return PH7_OK;` |
|    - |  694 | `	}` |
|    - |  695 | `	/* Extract given arguments */` |
|    3 |  696 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  697 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  698 | `	/* Perform the requested operation */` |
|    3 |  699 | `	r = fmod(x,y);` |
|    - |  700 | `	/* Processing result */` |
|    3 |  701 | `	ph7_result_double(pCtx,r);` |
|    3 |  702 | `	return PH7_OK;` |
|    2 |  703 | `}` |
|    - |  704 | `/*` |
|    - |  705 | ` * float hypot(float $x,float $y)` |
|    - |  706 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|    - |  707 | ` * Parameters` |
|    - |  708 | ` * $x` |
|    - |  709 | ` *  Length of first side` |
|    - |  710 | ` * $y` |
|    - |  711 | ` *  Length of first side` |
|    - |  712 | ` * Return` |
|    - |  713 | ` *  Calculated length of the hypotenuse.` |
|    - |  714 | ` */` |
|    2 |  715 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  716 | `{` |
|    - |  717 | `	double x,y,r;` |
|    3 |  718 | `	if( nArg < 2 ){` |
|    - |  719 | `		/* Missing arguments */` |
|  ! 0 |  720 | `		ph7_result_double(pCtx,0);` |
|  ! 0 |  721 | `		return PH7_OK;` |
|    - |  722 | `	}` |
|    - |  723 | `	/* Extract given arguments */` |
|    3 |  724 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  725 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  726 | `	/* Perform the requested operation */` |
|    3 |  727 | `	r = hypot(x,y);` |
|    - |  728 | `	/* Processing result */` |
|    3 |  729 | `	ph7_result_double(pCtx,r);` |
|    3 |  730 | `	return PH7_OK;` |
|    2 |  731 | `}` |
|    - |  732 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|    - |  733 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  734 | `/*` |
|    - |  735 | ` * PHP rounding modes (mirror ext/standard/php_math_round_mode.h).` |
|    - |  736 | ` * Only the four HALF_* integer constants are exposed to userland` |
|    - |  737 | ` * (PHP_ROUND_HALF_UP..HALF_ODD, see constant.c); the CEILING/FLOOR/` |
|    - |  738 | ` * TOWARD_ZERO/AWAY_FROM_ZERO modes (5..8) have no userland constant but` |
|    - |  739 | ` * are reachable by passing the raw integer to round()'s 3rd argument,` |
|    - |  740 | ` * which PHP 8.5 still accepts, so all eight are honored here.` |
|    - |  741 | ` */` |
|    - |  742 | `#define PH7_ROUND_HALF_UP        1` |
|    - |  743 | `#define PH7_ROUND_HALF_DOWN      2` |
|    - |  744 | `#define PH7_ROUND_HALF_EVEN      3` |
|    - |  745 | `#define PH7_ROUND_HALF_ODD       4` |
|    - |  746 | `#define PH7_ROUND_CEILING        5` |
|    - |  747 | `#define PH7_ROUND_FLOOR          6` |
|    - |  748 | `#define PH7_ROUND_TOWARD_ZERO    7` |
|    - |  749 | `#define PH7_ROUND_AWAY_FROM_ZERO 8` |
|    - |  750 | `/*` |
|    - |  751 | ` * 10**power via an exact lookup table for 0..22, falling back to pow()` |
|    - |  752 | ` * otherwise. Port of php-src PHP-8.5 ext/standard/math.c php_intpow10().` |
|    - |  753 | ` */` |
|  240 |  754 | `static double MathIntPow10(int power)` |
|    3 |  755 | `{` |
|    - |  756 | `	static const double powers[] = {` |
|    - |  757 | `		1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,` |
|    - |  758 | `		1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22` |
|    - |  759 | `	};` |
|  243 |  760 | `	if( power < 0 \|\| power > 22 ){` |
|    5 |  761 | `		return pow(10.0, (double)power);` |
|    - |  762 | `	}` |
|  239 |  763 | `	return powers[power];` |
|  123 |  764 | `}` |
|  218 |  765 | `static double MathRoundBasicEdge(double integral, double exponent, int places)` |
|    3 |  766 | `{` |
|  112 |  767 | `	return (places > 0)` |
|   88 |  768 | `		? fabs((integral + copysign(0.5, integral)) / exponent)` |
|  174 |  769 | `		: fabs((integral + copysign(0.5, integral)) * exponent);` |
|    3 |  770 | `}` |
|   12 |  771 | `static double MathRoundZeroEdge(double integral, double exponent, int places)` |
|    1 |  772 | `{` |
|    7 |  773 | `	return (places > 0)` |
|  ! 0 |  774 | `		? fabs((integral) / exponent)` |
|   12 |  775 | `		: fabs((integral) * exponent);` |
|    1 |  776 | `}` |
|    - |  777 | `/*` |
|    - |  778 | ` * Round the extracted integral part according to the requested mode.` |
|    - |  779 | ` * Faithful port of php-src PHP-8.5 ext/standard/math.c php_round_helper().` |
|    - |  780 | ` */` |
|  234 |  781 | `static double MathRoundHelper(double integral, double value, double exponent, int places, int mode)` |
|    3 |  782 | `{` |
|  237 |  783 | `	double value_abs = fabs(value);` |
|    - |  784 | `	double edge_case;` |
|  237 |  785 | `	switch( mode ){` |
|   89 |  786 | `		case PH7_ROUND_HALF_UP:` |
|  181 |  787 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|  181 |  788 | `			if( value_abs >= edge_case ){` |
|  112 |  789 | `				return integral + copysign(1.0, integral);` |
|    - |  790 | `			}` |
|   71 |  791 | `			return integral;` |
|    6 |  792 | `		case PH7_ROUND_HALF_DOWN:` |
|   13 |  793 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   13 |  794 | `			if( value_abs > edge_case ){` |
|  ! 0 |  795 | `				return integral + copysign(1.0, integral);` |
|    - |  796 | `			}` |
|   13 |  797 | `			return integral;` |
|    2 |  798 | `		case PH7_ROUND_CEILING:` |
|    5 |  799 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  800 | `			if( value > 0.0 && value_abs > edge_case ){` |
|    3 |  801 | `				return integral + 1.0;` |
|    - |  802 | `			}` |
|    3 |  803 | `			return integral;` |
|    2 |  804 | `		case PH7_ROUND_FLOOR:` |
|    5 |  805 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  806 | `			if( value < 0.0 && value_abs > edge_case ){` |
|    3 |  807 | `				return integral - 1.0;` |
|    - |  808 | `			}` |
|    3 |  809 | `			return integral;` |
|    2 |  810 | `		case PH7_ROUND_TOWARD_ZERO:` |
|    5 |  811 | `			return integral;` |
|    2 |  812 | `		case PH7_ROUND_AWAY_FROM_ZERO:` |
|    5 |  813 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  814 | `			if( value_abs > edge_case ){` |
|    5 |  815 | `				return integral + copysign(1.0, integral);` |
|    - |  816 | `			}` |
|  ! 0 |  817 | `			return integral;` |
|    8 |  818 | `		case PH7_ROUND_HALF_EVEN:` |
|   17 |  819 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   17 |  820 | `			if( value_abs > edge_case ){` |
|  ! 0 |  821 | `				return integral + copysign(1.0, integral);` |
|   17 |  822 | `			}else if( value_abs == edge_case ){` |
|   17 |  823 | `				if( fmod(integral, 2.0) != 0.0 ){ /* integral not even -> make it even */` |
|    9 |  824 | `					return integral + copysign(1.0, integral);` |
|    - |  825 | `				}` |
|    4 |  826 | `			}` |
|    9 |  827 | `			return integral;` |
|    6 |  828 | `		case PH7_ROUND_HALF_ODD:` |
|   13 |  829 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   13 |  830 | `			if( value_abs > edge_case ){` |
|  ! 0 |  831 | `				return integral + copysign(1.0, integral);` |
|   13 |  832 | `			}else if( value_abs == edge_case ){` |
|   13 |  833 | `				if( fmod(integral, 2.0) == 0.0 ){ /* integral even -> make it odd */` |
|    7 |  834 | `					return integral + copysign(1.0, integral);` |
|    - |  835 | `				}` |
|    3 |  836 | `			}` |
|    7 |  837 | `			return integral;` |
|  ! 0 |  838 | `		default:` |
|  ! 0 |  839 | `			return integral; /* unreachable: mode validated by the caller */` |
|    - |  840 | `	}` |
|  120 |  841 | `}` |
|    - |  842 | `/*` |
|    - |  843 | `` * Round `value` to `places` decimals in `mode`. Faithful port of php-src`` |
|    - |  844 | ` * PHP-8.5 ext/standard/math.c _php_math_round() — the post-8.4` |
|    - |  845 | ` * integer-extraction algorithm with the +/-1 floating-point error` |
|    - |  846 | ` * correction step, required for byte-exact results on cases such as` |
|    - |  847 | ` * round(0.285, 2) == 0.29 that the old naive "+0.5" approach got wrong.` |
|    - |  848 | ` */` |
|  256 |  849 | `static double MathRound(double value, int places, int mode)` |
|    3 |  850 | `{` |
|    - |  851 | `	double exponent, tmp_value, tmp_value2;` |
|    - |  852 | `	int abs_places;` |
|  259 |  853 | `	if( !isfinite(value) \|\| value == 0.0 ){` |
|   17 |  854 | `		return value;` |
|    - |  855 | `	}` |
|    - |  856 | `	/* mirror php-src's clamp away from INT_MIN */` |
|  243 |  857 | `	if( places < -2147483647 ){` |
|  ! 0 |  858 | `		places = -2147483647;` |
|  ! 0 |  859 | `	}` |
|  243 |  860 | `	abs_places = places < 0 ? -places : places;` |
|  243 |  861 | `	exponent = MathIntPow10(abs_places);` |
|    - |  862 | `	/*` |
|    - |  863 | `	 * Extracting the integer part can be off by one ULP due to float error` |
|    - |  864 | `	 * (e.g. floor(0.285 * 1e10) == 2849999999). Try +/-1 and keep it if it` |
|    - |  865 | ``	 * divides back to exactly `value`.`` |
|    - |  866 | `	 */` |
|  243 |  867 | `	if( value >= 0.0 ){` |
|  213 |  868 | `		tmp_value = floor(places > 0 ? value * exponent : value / exponent);` |
|  213 |  869 | `		tmp_value2 = tmp_value + 1.0;` |
|  108 |  870 | `	}else{` |
|   31 |  871 | `		tmp_value = ceil(places > 0 ? value * exponent : value / exponent);` |
|   31 |  872 | `		tmp_value2 = tmp_value - 1.0;` |
|    - |  873 | `	}` |
|  243 |  874 | `	if( (places > 0 ? tmp_value2 / exponent : tmp_value2 * exponent) == value ){` |
|    7 |  875 | `		tmp_value = tmp_value2;` |
|    3 |  876 | `	}` |
|    - |  877 | `	/* Beyond our precision, so rounding it is pointless. */` |
|  243 |  878 | `	if( fabs(tmp_value) >= 1e16 ){` |
|    7 |  879 | `		return value;` |
|    - |  880 | `	}` |
|  237 |  881 | `	tmp_value = MathRoundHelper(tmp_value, value, exponent, places, mode);` |
|  237 |  882 | `	if( abs_places < 23 ){` |
|  237 |  883 | `		tmp_value = (places > 0) ? tmp_value / exponent : tmp_value * exponent;` |
|  120 |  884 | `	}else{` |
|    - |  885 | `		/*` |
|    - |  886 | `		 * Simple division would lose precision here; round-trip through a` |
|    - |  887 | `		 * string exactly like php-src does (snprintf "%15fe%d" + strtod).` |
|    - |  888 | `		 * libc snprintf is used (not SyBufferFormat, which is not` |
|    - |  889 | `		 * correctly-rounded) so the low bits match PHP. (SyStrToReal now` |
|    - |  890 | `		 * delegates to strtod too; the direct call here simply mirrors` |
|    - |  891 | `		 * php-src's own snprintf+strtod pairing.)` |
|    - |  892 | `		 */` |
|    - |  893 | `		char zBuf[64];` |
|  ! 0 |  894 | `		snprintf(zBuf, sizeof(zBuf), "%15fe%d", tmp_value, -places);` |
|  ! 0 |  895 | `		zBuf[sizeof(zBuf)-1] = '\0';` |
|  ! 0 |  896 | `		tmp_value = strtod(zBuf, 0);` |
|  ! 0 |  897 | `		if( !isfinite(tmp_value) \|\| isnan(tmp_value) ){` |
|  ! 0 |  898 | `			tmp_value = value;` |
|  ! 0 |  899 | `		}` |
|    - |  900 | `	}` |
|  237 |  901 | `	return tmp_value;` |
|  131 |  902 | `}` |
|    - |  903 | `/*` |
|    - |  904 | ` * float round ( int\|float $num [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|    - |  905 | ` *  Rounds a float.` |
|    - |  906 | ` * Parameters` |
|    - |  907 | ` *  $num       The value to round.` |
|    - |  908 | ` *  $precision The optional number of decimal digits to round to. May be` |
|    - |  909 | ` *             negative (rounds to the left of the decimal point).` |
|    - |  910 | ` *  $mode      One of PHP_ROUND_HALF_UP (default) / _HALF_DOWN / _HALF_EVEN /` |
|    - |  911 | ` *             _HALF_ODD, or the 8.5 integer modes CEILING / FLOOR /` |
|    - |  912 | ` *             TOWARD_ZERO / AWAY_FROM_ZERO (5..8).` |
|    - |  913 | ` * Return` |
|    - |  914 | ` *  The rounded value as a float.` |
|    - |  915 | ` */` |
|  178 |  916 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  917 | `{` |
|    - |  918 | `	double value, r;` |
|  179 |  919 | `	int places = 0;` |
|  179 |  920 | `	int mode = PH7_ROUND_HALF_UP;` |
|    - |  921 | `	/*` |
|    - |  922 | `	 * Legacy PHL contract: no argument -> int(0). PHP throws an` |
|    - |  923 | `	 * ArgumentCountError here, but two PHL-only (--SKIPIF-- zend_version)` |
|    - |  924 | `	 * tests assert round()===0, so keep the historical behavior.` |
|    - |  925 | `	 */` |
|  179 |  926 | `	if( nArg < 1 ){` |
|  ! 0 |  927 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  928 | `		return PH7_OK;` |
|    - |  929 | `	}` |
|  179 |  930 | `	if( nArg > 3 ){` |
|  ! 0 |  931 | `		return PH7_VmThrowException(pCtx,` |
|    - |  932 | `			"ArgumentCountError",` |
|    - |  933 | `			"round() expects at most 3 arguments, %d given",` |
|  ! 0 |  934 | `			nArg` |
|    - |  935 | `			);` |
|    - |  936 | `	}` |
|    - |  937 | `	/* Argument #1's type is the aBuiltinSig[] row's -- see floor() above. */` |
|    - |  938 | `	/* Precision (arg #2). Negative values are valid; clamp to int range. */` |
|  179 |  939 | `	if( nArg > 1 ){` |
|  143 |  940 | `		sxi64 prec = ph7_value_to_int64(apArg[1]);` |
|  143 |  941 | `		if( prec > 2147483647 ){` |
|  ! 0 |  942 | `			places = 2147483647;` |
|  143 |  943 | `		}else if( prec < -2147483647 ){` |
|  ! 0 |  944 | `			places = -2147483647;` |
|  ! 0 |  945 | `		}else{` |
|  143 |  946 | `			places = (int)prec;` |
|    - |  947 | `		}` |
|   71 |  948 | `	}` |
|    - |  949 | `	/*` |
|    - |  950 | `	 * Mode (arg #3). PHP 8.5 accepts the integer modes 1..8. Read the full` |
|    - |  951 | `	 * 64-bit value before range-checking so a large out-of-range mode cannot` |
|    - |  952 | `	 * alias a valid 1..8 via a truncating 32-bit cast (e.g. 0x1_0000_0003).` |
|    - |  953 | `	 */` |
|  179 |  954 | `	if( nArg > 2 ){` |
|   73 |  955 | `		sxi64 m = ph7_value_to_int64(apArg[2]);` |
|   73 |  956 | `		if( m < PH7_ROUND_HALF_UP \|\| m > PH7_ROUND_AWAY_FROM_ZERO ){` |
|    5 |  957 | `			return PH7_VmThrowException(pCtx,` |
|    - |  958 | `				"ValueError",` |
|    - |  959 | `				"round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)"` |
|    - |  960 | `				);` |
|    - |  961 | `		}` |
|   69 |  962 | `		mode = (int)m;` |
|   34 |  963 | `	}` |
|  175 |  964 | `	value = ph7_value_to_double(apArg[0]);` |
|    - |  965 | `	/* Integer input with non-negative precision needs no rounding. */` |
|  175 |  966 | `	if( ph7_value_is_int(apArg[0]) && places >= 0 ){` |
|   21 |  967 | `		ph7_result_double(pCtx,value);` |
|   21 |  968 | `		return PH7_OK;` |
|    - |  969 | `	}` |
|  155 |  970 | `	r = MathRound(value, places, mode);` |
|  155 |  971 | `	ph7_result_double(pCtx,r);` |
|  155 |  972 | `	return PH7_OK;` |
|   90 |  973 | `}` |
|    - |  974 | `/*` |
|    - |  975 | ` * Assemble php's formatted number: the integer digits grouped from the right by` |
|    - |  976 | ` * the thousands separator, then the decimal separator and exactly $decimals` |
|    - |  977 | ` * fraction digits (right-padded with '0', since the printf may produce fewer).` |
|    - |  978 | ` */` |
|  136 |  979 | `static int NumberFormatEmit(ph7_context *pCtx,` |
|    - |  980 | `	const char *zDigits,int nDigits,int bNeg,` |
|    - |  981 | `	const char *zFrac,int nFrac,int nDec,` |
|    - |  982 | `	const char *zPoint,int nPoint,const char *zSep,int nSep)` |
|    3 |  983 | `{` |
|    - |  984 | `	SyBlob sOut;` |
|    - |  985 | `	int i;` |
|  139 |  986 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  139 |  987 | `	if( bNeg ){` |
|   12 |  988 | `		SyBlobAppend(&sOut,"-",sizeof(char));` |
|    5 |  989 | `	}` |
|  833 |  990 | `	for( i = 0 ; i < nDigits ; ++i ){` |
|  697 |  991 | `		if( i > 0 && nSep > 0 && ((nDigits - i) % 3) == 0 ){` |
|  173 |  992 | `			SyBlobAppend(&sOut,zSep,(sxu32)nSep);` |
|   85 |  993 | `		}` |
|  697 |  994 | `		SyBlobAppend(&sOut,&zDigits[i],sizeof(char));` |
|  350 |  995 | `	}` |
|  139 |  996 | `	if( nDec > 0 ){` |
|   59 |  997 | `		if( nPoint > 0 ){` |
|   55 |  998 | `			SyBlobAppend(&sOut,zPoint,(sxu32)nPoint);` |
|   26 |  999 | `		}` |
|   59 | 1000 | `		if( nFrac > nDec ){` |
|  ! 0 | 1001 | `			nFrac = nDec;` |
|  ! 0 | 1002 | `		}` |
|   59 | 1003 | `		if( nFrac > 0 ){` |
|   57 | 1004 | `			SyBlobAppend(&sOut,zFrac,(sxu32)nFrac);` |
|   27 | 1005 | `		}` |
|   63 | 1006 | `		for( i = nFrac ; i < nDec ; ++i ){` |
|    5 | 1007 | `			SyBlobAppend(&sOut,"0",sizeof(char));` |
|    3 | 1008 | `		}` |
|   28 | 1009 | `	}` |
|  139 | 1010 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  139 | 1011 | `	SyBlobRelease(&sOut);` |
|  139 | 1012 | `	return PH7_OK;` |
|    3 | 1013 | `}` |
|    - | 1014 | `/*` |
|    - | 1015 | ` * _php_math_number_format_long(): an INTEGER never goes through a double, so` |
|    - | 1016 | ` * every digit of a value past 2^53 survives. A NEGATIVE $decimals rounds the` |
|    - | 1017 | ` * integer itself, half away from zero.` |
|    - | 1018 | ` */` |
|   44 | 1019 | `static int NumberFormatLong(ph7_context *pCtx,sxi64 iVal,int nDec,` |
|    - | 1020 | `	const char *zPoint,int nPoint,const char *zSep,int nSep)` |
|    2 | 1021 | `{` |
|    - | 1022 | `	char zBuf[32];` |
|    - | 1023 | `	sxu64 uNum;` |
|   46 | 1024 | `	int bNeg = 0;` |
|   46 | 1025 | `	int n = 0;` |
|   46 | 1026 | `	if( iVal < 0 ){` |
|   11 | 1027 | `		bNeg = 1;` |
|    - | 1028 | `		/* -PHP_INT_MIN does not fit; negate through the unsigned domain. */` |
|   11 | 1029 | `		uNum = ((sxu64)-(iVal + 1)) + 1;` |
|    6 | 1030 | `	}else{` |
|   36 | 1031 | `		uNum = (sxu64)iVal;` |
|    - | 1032 | `	}` |
|   46 | 1033 | `	if( nDec < 0 ){` |
|    - | 1034 | `		/* php keeps a table of the 20 powers of ten a 64-bit value can hold and` |
|    - | 1035 | `		 * answers 0 past it; 10^19 is the last one that fits. */` |
|   21 | 1036 | `		if( nDec < -19 ){` |
|    3 | 1037 | `			uNum = 0;` |
|    2 | 1038 | `		}else{` |
|   19 | 1039 | `			sxu64 uPow = 1;` |
|    - | 1040 | `			sxu64 uRest;` |
|    - | 1041 | `			int k;` |
|   49 | 1042 | `			for( k = 0 ; k < -nDec ; ++k ){` |
|   31 | 1043 | `				uPow *= 10;` |
|   16 | 1044 | `			}` |
|   19 | 1045 | `			uRest = uNum % uPow;` |
|   19 | 1046 | `			uNum = uNum / uPow;` |
|   19 | 1047 | `			uNum = (uRest >= uPow / 2) ? uNum * uPow + uPow : uNum * uPow;` |
|    - | 1048 | `		}` |
|   21 | 1049 | `		if( uNum == 0 ){` |
|    - | 1050 | `			/* php never answers "-0". */` |
|    9 | 1051 | `			bNeg = 0;` |
|    4 | 1052 | `		}` |
|   10 | 1053 | `	}` |
|    - | 1054 | `	/* Decimal digits, most significant first. */` |
|   46 | 1055 | `	if( uNum == 0 ){` |
|   14 | 1056 | `		zBuf[n++] = '0';` |
|    8 | 1057 | `	}else{` |
|    - | 1058 | `		char zRev[32];` |
|   34 | 1059 | `		int nRev = 0;` |
|  372 | 1060 | `		while( uNum > 0 && nRev < (int)sizeof(zRev) ){` |
|  340 | 1061 | `			zRev[nRev++] = (char)('0' + (int)(uNum % 10));` |
|  340 | 1062 | `			uNum /= 10;` |
|    2 | 1063 | `		}` |
|  372 | 1064 | `		while( nRev > 0 ){` |
|  340 | 1065 | `			zBuf[n++] = zRev[--nRev];` |
|    2 | 1066 | `		}` |
|    - | 1067 | `	}` |
|   46 | 1068 | `	return NumberFormatEmit(pCtx,zBuf,n,bNeg,0,0,nDec > 0 ? nDec : 0,` |
|   22 | 1069 | `		zPoint,nPoint,zSep,nSep);` |
|    2 | 1070 | `}` |
|    - | 1071 | `/*` |
|    - | 1072 | ` * string number_format(int\|float $num,int $decimals = 0,` |
|    - | 1073 | ` *                      ?string $decimal_separator = ".",` |
|    - | 1074 | ` *                      ?string $thousands_separator = ",")` |
|    - | 1075 | ` *  Format a number with grouped thousands.` |
|    - | 1076 | ` */` |
|  204 | 1077 | `PH7_PRIVATE int PH7_builtin_number_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 1078 | `{` |
|  207 | 1079 | `	const char *zPoint = ".", *zSep = ",";` |
|  207 | 1080 | `	int nPoint = 1, nSep = 1;` |
|  207 | 1081 | `	int nDec = 0;` |
|    - | 1082 | `	ph7_value sNum;` |
|    - | 1083 | `	double d;` |
|  207 | 1084 | `	int bNeg = 0;` |
|    - | 1085 | `	int nLen,nInt;` |
|    - | 1086 | `	char *zFmt;` |
|    - | 1087 | `	const char *zDot;` |
|  207 | 1088 | `	if( nArg < 1 ){` |
|    - | 1089 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|  ! 0 | 1090 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1091 | `		return PH7_OK;` |
|    - | 1092 | `	}` |
|    - | 1093 | `	/* Every refusal is worded here rather than by the shared screen: php's stub` |
|    - | 1094 | ``	 * declares `float $num` (which is what Reflection prints) but the ZPP macro`` |
|    - | 1095 | ``	 * behind it is Z_PARAM_NUMBER, whose TypeError says `int\|float`. An int stays`` |
|    - | 1096 | `	 * an INT, a numeric string takes the shape it looks like, and null is §10's` |
|    - | 1097 | `	 * refusal of a deprecation. */` |
|  207 | 1098 | `	if( !PH7_MemObjIsNumeric(apArg[0]) ){` |
|    - | 1099 | `		char zBuf[64];` |
|   60 | 1100 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1101 | `			"number_format(): Argument #1 ($num) must be of type int\|float, %s given",` |
|   32 | 1102 | `			ph7_value_is_string(apArg[0]) ? "string"` |
|   18 | 1103 | `				: VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1104 | `	}` |
|  175 | 1105 | `	if( nArg > 1 ){` |
|    - | 1106 | ``		/* php declares `int $decimals`; the string and float narrowings it only`` |
|    - | 1107 | `		 * DEPRECATES are rejected here (§10), as they are for count_chars(). */` |
|  124 | 1108 | `		if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|  124 | 1109 | `		 \|\| ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|    - | 1110 | `			char zBuf[64];` |
|   14 | 1111 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1112 | `				"number_format(): Argument #2 ($decimals) must be of type int, %s given",` |
|    8 | 1113 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|    - | 1114 | `		}` |
|  119 | 1115 | `		if( ph7_value_is_string(apArg[1]) ){` |
|    - | 1116 | `			/* php wants the WHOLE string to be numeric ("2abc" is a TypeError,` |
|    - | 1117 | `			 * not 2), and a float-shaped one that would LOSE something is §10's` |
|    - | 1118 | `			 * refusal of a deprecation. */` |
|    - | 1119 | `			double dMode;` |
|    8 | 1120 | `			if( !PH7_MemObjStringIsNumeric(apArg[1]) ){` |
|    6 | 1121 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1122 | `					"number_format(): Argument #2 ($decimals) must be of type int, string given");` |
|    - | 1123 | `			}` |
|    3 | 1124 | `			dMode = ph7_value_to_double(apArg[1]);` |
|    - | 1125 | ``			/* Range first: `(sxi64)dMode` is undefined outside it (§2). */`` |
|    3 | 1126 | `			if( !PH7_RealFitsInt64(dMode) \|\| dMode != (double)(sxi64)dMode ){` |
|  ! 0 | 1127 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1128 | `					"number_format(): Argument #2 ($decimals) must be of type int, string given");` |
|    1 | 1129 | `			}` |
|  114 | 1130 | `		}else if( ph7_value_is_float(apArg[1]) ){` |
|    6 | 1131 | `			double dMode = ph7_value_to_double(apArg[1]);` |
|    6 | 1132 | `			if( !PH7_RealFitsInt64(dMode) \|\| dMode != (double)(sxi64)dMode ){` |
|    3 | 1133 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1134 | `					"number_format(): Argument #2 ($decimals) must be of type int, float given");` |
|    - | 1135 | `			}` |
|    1 | 1136 | `		}` |
|    - | 1137 | `		{` |
|  113 | 1138 | `			sxi64 iDec = ph7_value_to_int64(apArg[1]);` |
|    - | 1139 | `			/* php clamps the declared long onto an int before it formats. */` |
|  168 | 1140 | `			nDec = iDec > 2147483647 ? 2147483647` |
|  110 | 1141 | `			     : (iDec < -2147483647 ? -2147483647 : (int)iDec);` |
|    - | 1142 | `		}` |
|   55 | 1143 | `	}` |
|    - | 1144 | ``	/* Both separators are `?string`: null means php's default, not the empty`` |
|    - | 1145 | `	 * string. An empty string IS accepted and simply omits the separator, and an` |
|    - | 1146 | `	 * object that can stringify is coerced. */` |
|  161 | 1147 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|   36 | 1148 | `		if( !PH7_ArgSatisfiesString(apArg[2]) ){` |
|    - | 1149 | `			char zBuf[64];` |
|   14 | 1150 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1151 | `				"number_format(): Argument #3 ($decimal_separator) must be of type ?string, %s given",` |
|    8 | 1152 | `				VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|    - | 1153 | `		}` |
|   28 | 1154 | `		zPoint = ph7_value_to_string(apArg[2],&nPoint);` |
|   13 | 1155 | `	}` |
|  153 | 1156 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|   28 | 1157 | `		if( !PH7_ArgSatisfiesString(apArg[3]) ){` |
|    - | 1158 | `			char zBuf[64];` |
|    7 | 1159 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1160 | `				"number_format(): Argument #4 ($thousands_separator) must be of type ?string, %s given",` |
|    4 | 1161 | `				VmValueGivenName(apArg[3],zBuf,sizeof(zBuf)));` |
|    - | 1162 | `		}` |
|   24 | 1163 | `		zSep = ph7_value_to_string(apArg[3],&nSep);` |
|   11 | 1164 | `	}` |
|  149 | 1165 | `	PH7_MemObjInit(pCtx->pVm,&sNum);` |
|  149 | 1166 | `	PH7_MemObjStore(apArg[0],&sNum);` |
|  149 | 1167 | `	PH7_MemObjToNumeric(&sNum);` |
|  149 | 1168 | `	if( (sNum.iFlags & MEMOBJ_REAL) == 0 ){` |
|   44 | 1169 | `		int rc = NumberFormatLong(pCtx,sNum.x.iVal,nDec,zPoint,nPoint,zSep,nSep);` |
|   44 | 1170 | `		PH7_MemObjRelease(&sNum);` |
|   44 | 1171 | `		return rc;` |
|    - | 1172 | `	}` |
|  107 | 1173 | `	d = (double)sNum.rVal;` |
|  107 | 1174 | `	PH7_MemObjRelease(&sNum);` |
|    - | 1175 | `	/* A double past 2^52 has no fractional digits left, so php formats it as an` |
|    - | 1176 | `	 * INTEGER when it fits one — that is what keeps 4503599627370496.0 exact. */` |
|  104 | 1177 | `	if( (d >= 4503599627370496.0 \|\| d <= -4503599627370496.0)` |
|   60 | 1178 | `	 && PH7_RealFitsInt64(d) ){` |
|    3 | 1179 | `		return NumberFormatLong(pCtx,(sxi64)d,nDec,zPoint,nPoint,zSep,nSep);` |
|    - | 1180 | `	}` |
|  105 | 1181 | `	if( d < 0 ){` |
|   14 | 1182 | `		bNeg = 1;` |
|   14 | 1183 | `		d = -d;` |
|    6 | 1184 | `	}` |
|  105 | 1185 | `	d = MathRound(d,nDec,PH7_ROUND_HALF_UP);` |
|  105 | 1186 | `	if( nDec < 0 ){` |
|   16 | 1187 | `		nDec = 0;` |
|    7 | 1188 | `	}` |
|    - | 1189 | `	/* libc's %f, not the engine's formatter: php prints through its own` |
|    - | 1190 | `	 * snprintf here, so INF answers "inf" and NAN "nan" — and the engine's` |
|    - | 1191 | `	 * formatter caps the precision at 53 digits with a notice, where php` |
|    - | 1192 | `	 * honours whatever $decimals asks for. */` |
|  105 | 1193 | `	nLen = snprintf(0,0,"%.*f",nDec,d);` |
|  105 | 1194 | `	if( nLen < 0 ){` |
|  ! 0 | 1195 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1196 | `	}` |
|  105 | 1197 | `	zFmt = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen + 1,FALSE,TRUE);` |
|  105 | 1198 | `	if( zFmt == 0 ){` |
|  ! 0 | 1199 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1200 | `	}` |
|  105 | 1201 | `	snprintf(zFmt,(size_t)nLen + 1,"%.*f",nDec,d);` |
|  105 | 1202 | `	if( zFmt[0] < '0' \|\| zFmt[0] > '9' ){` |
|    - | 1203 | `		/* Not a number at all (inf/nan): php hands its buffer straight back,` |
|    - | 1204 | `		 * without a sign, a separator or any padding. */` |
|   11 | 1205 | `		ph7_result_string(pCtx,zFmt,nLen);` |
|   11 | 1206 | `		return PH7_OK;` |
|    - | 1207 | `	}` |
|   95 | 1208 | `	if( bNeg && d == 0 ){` |
|    - | 1209 | `		/* Rounded away to zero; php never answers "-0". */` |
|    5 | 1210 | `		bNeg = 0;` |
|    2 | 1211 | `	}` |
|    - | 1212 | `	/* php looks for '.' OR ',' — the decimal point its formatter produced. */` |
|   95 | 1213 | `	zDot = 0;` |
|   95 | 1214 | `	if( nDec > 0 ){` |
|    - | 1215 | `		int i;` |
|  273 | 1216 | `		for( i = 0 ; i < nLen ; ++i ){` |
|  273 | 1217 | `			if( zFmt[i] == '.' \|\| zFmt[i] == ',' ){` |
|   57 | 1218 | `				zDot = &zFmt[i];` |
|   57 | 1219 | `				break;` |
|    - | 1220 | `			}` |
|  111 | 1221 | `		}` |
|   27 | 1222 | `	}` |
|   95 | 1223 | `	nInt = zDot ? (int)(zDot - zFmt) : nLen;` |
|  168 | 1224 | `	return NumberFormatEmit(pCtx,zFmt,nInt,bNeg,` |
|   73 | 1225 | `		zDot ? zDot + 1 : 0,zDot ? nLen - nInt - 1 : 0,` |
|   46 | 1226 | `		nDec,zPoint,nPoint,zSep,nSep);` |
|  105 | 1227 | `}` |
|    - | 1228 | `/*` |
|    - | 1229 | ` * int intdiv(int $a, int $b)` |
|    - | 1230 | ` *  Integer division.` |
|    - | 1231 | ` * Parameters` |
|    - | 1232 | ` *  $a` |
|    - | 1233 | ` *   Number to be divided.` |
|    - | 1234 | ` *  $b` |
|    - | 1235 | ` *   Number which divides the $a.` |
|    - | 1236 | ` * Return` |
|    - | 1237 | ` *  The integer quotient of the division of $a by $b.` |
|    - | 1238 | ` */` |
|   24 | 1239 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 | 1240 | `{` |
|    - | 1241 | `	sxi64 a,b;` |
|    - | 1242 | `	/* PHP requires exactly two arguments. */` |
|   28 | 1243 | `	if( nArg != 2 ){` |
|  ! 0 | 1244 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1245 | `			"ArgumentCountError",` |
|    - | 1246 | `			"intdiv() expects exactly 2 arguments, %d given",` |
|  ! 0 | 1247 | `			nArg` |
|    - | 1248 | `			);` |
|    - | 1249 | `	}` |
|    - | 1250 | `	/* Type-check argument 1 */` |
|   24 | 1251 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|   28 | 1252 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 | 1253 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1254 | `			"TypeError",` |
|    - | 1255 | `			"intdiv(): Argument #1 ($num1) must be of type int, %s given",` |
|  ! 0 | 1256 | `			ph7_type_name(apArg[0])` |
|    - | 1257 | `			);` |
|    - | 1258 | `	}` |
|   28 | 1259 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - | 1260 | `		int len;` |
|    3 | 1261 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    3 | 1262 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 | 1263 | `			return PH7_VmThrowException(pCtx,` |
|    - | 1264 | `				"TypeError",` |
|    - | 1265 | `				"intdiv(): Argument #1 ($num1) must be of type int, string given"` |
|    - | 1266 | `				);` |
|    - | 1267 | `		}` |
|    1 | 1268 | `	}` |
|    - | 1269 | `	/* Type-check argument 2 */` |
|   24 | 1270 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|   28 | 1271 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|  ! 0 | 1272 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1273 | `			"TypeError",` |
|    - | 1274 | `			"intdiv(): Argument #2 ($num2) must be of type int, %s given",` |
|  ! 0 | 1275 | `			ph7_type_name(apArg[1])` |
|    - | 1276 | `			);` |
|    - | 1277 | `	}` |
|   28 | 1278 | `	if( ph7_value_is_string(apArg[1]) ){` |
|    - | 1279 | `		int len;` |
|  ! 0 | 1280 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|  ! 0 | 1281 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 | 1282 | `			return PH7_VmThrowException(pCtx,` |
|    - | 1283 | `				"TypeError",` |
|    - | 1284 | `				"intdiv(): Argument #2 ($num2) must be of type int, string given"` |
|    - | 1285 | `				);` |
|    - | 1286 | `		}` |
|  ! 0 | 1287 | `	}` |
|    - | 1288 | `	/* Convert both arguments to int64 */` |
|    - | 1289 | `	{` |
|    - | 1290 | `		/* php's ZPP contract for the two int params (lossy float / float-string` |
|    - | 1291 | `		 * deprecations); the manual type checks above already covered arrays,` |
|    - | 1292 | `		 * objects and non-numeric strings with the same messages. */` |
|   28 | 1293 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[0],"intdiv",1,"$num1","int",&a);` |
|   28 | 1294 | `		if( rcArg != PH7_OK ){` |
|  ! 0 | 1295 | `			return rcArg;` |
|    - | 1296 | `		}` |
|   28 | 1297 | `		rcArg = PH7_IntArgResolve(pCtx,apArg[1],"intdiv",2,"$num2","int",&b);` |
|   28 | 1298 | `		if( rcArg != PH7_OK ){` |
|  ! 0 | 1299 | `			return rcArg;` |
|    - | 1300 | `		}` |
|    - | 1301 | `	}` |
|    - | 1302 | `	/* Check for division by zero */` |
|   28 | 1303 | `	if( b == 0 ){` |
|    6 | 1304 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1305 | `			"DivisionByZeroError",` |
|    - | 1306 | `			"Division by zero"` |
|    - | 1307 | `			);` |
|    - | 1308 | `	}` |
|    - | 1309 | `	/* Check for overflow: PHP_INT_MIN / -1 */` |
|   23 | 1310 | `	if( a == SMALLEST_INT64 && b == -1 ){` |
|    3 | 1311 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1312 | `			"ArithmeticError",` |
|    - | 1313 | `			"Division of PHP_INT_MIN by -1 is not an integer"` |
|    - | 1314 | `			);` |
|    - | 1315 | `	}` |
|    - | 1316 | `	/* Perform integer division */` |
|   20 | 1317 | `	ph7_result_int64(pCtx, a / b);` |
|   20 | 1318 | `	return PH7_OK;` |
|   16 | 1319 | `}` |
|    - | 1320 | `/*` |
|    - | 1321 | ` * string dechex(int $number)` |
|    - | 1322 | ` *  Decimal to hexadecimal.` |
|    - | 1323 | ` * Parameters` |
|    - | 1324 | ` *  $number` |
|    - | 1325 | ` *   Decimal value to convert` |
|    - | 1326 | ` * Return` |
|    - | 1327 | ` *  Hexadecimal string representation of number` |
|    - | 1328 | ` */` |
|   22 | 1329 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1330 | `{` |
|    - | 1331 | `	ph7_int64 iVal;` |
|   24 | 1332 | `	if( nArg < 1 ){` |
|    - | 1333 | `		/* Missing arguments,return null */` |
|  ! 0 | 1334 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1335 | `		return PH7_OK;` |
|    - | 1336 | `	}` |
|    - | 1337 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   24 | 1338 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1339 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement,` |
|    - | 1340 | `	 * so a negative value prints all 16 hex digits like PHP. */` |
|   24 | 1341 | `	ph7_result_string_format(pCtx,"%qx",iVal);` |
|   24 | 1342 | `	return PH7_OK;` |
|   13 | 1343 | `}` |
|    - | 1344 | `/*` |
|    - | 1345 | ` * string decoct(int $number)` |
|    - | 1346 | ` *  Decimal to Octal.` |
|    - | 1347 | ` * Parameters` |
|    - | 1348 | ` *  $number` |
|    - | 1349 | ` *   Decimal value to convert` |
|    - | 1350 | ` * Return` |
|    - | 1351 | ` *  Octal string representation of number` |
|    - | 1352 | ` */` |
|   16 | 1353 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1354 | `{` |
|    - | 1355 | `	ph7_int64 iVal;` |
|   17 | 1356 | `	if( nArg < 1 ){` |
|    - | 1357 | `		/* Missing arguments,return null */` |
|  ! 0 | 1358 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1359 | `		return PH7_OK;` |
|    - | 1360 | `	}` |
|    - | 1361 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   17 | 1362 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1363 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|   17 | 1364 | `	ph7_result_string_format(pCtx,"%qo",iVal);` |
|   17 | 1365 | `	return PH7_OK;` |
|    9 | 1366 | `}` |
|    - | 1367 | `/*` |
|    - | 1368 | ` * string decbin(int $number)` |
|    - | 1369 | ` *  Decimal to binary.` |
|    - | 1370 | ` * Parameters` |
|    - | 1371 | ` *  $number` |
|    - | 1372 | ` *   Decimal value to convert` |
|    - | 1373 | ` * Return` |
|    - | 1374 | ` *  Binary string representation of number` |
|    - | 1375 | ` */` |
|   10 | 1376 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1377 | `{` |
|    - | 1378 | `	ph7_int64 iVal;` |
|   11 | 1379 | `	if( nArg < 1 ){` |
|    - | 1380 | `		/* Missing arguments,return null */` |
|  ! 0 | 1381 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1382 | `		return PH7_OK;` |
|    - | 1383 | `	}` |
|    - | 1384 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   11 | 1385 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1386 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|   11 | 1387 | `	ph7_result_string_format(pCtx,"%qB",iVal);` |
|   11 | 1388 | `	return PH7_OK;` |
|    6 | 1389 | `}` |
|    - | 1390 | `/*` |
|    - | 1391 | ` * Convert a base-2/8/16 digit string to a number, mirroring PHP's` |
|    - | 1392 | ` * _php_math_basetozval (ext/standard/math.c) so hexdec/octdec/bindec agree with` |
|    - | 1393 | ` * php byte-for-byte: walk every byte, decode a digit (0-9,a-z,A-Z) or skip any` |
|    - | 1394 | ` * invalid one, accumulate into a signed 64-bit integer and transparently promote` |
|    - | 1395 | ` * to a double once the value would overflow PHP_INT_MAX. The context result is` |
|    - | 1396 | ` * set to an int when it fits, otherwise a float — PHP returns a float for values` |
|    - | 1397 | ` * above PHP_INT_MAX (e.g. hexdec("ffffffffffffffff") == 1.8446744073709552E+19).` |
|    - | 1398 | ` * A byte >= 0x80 (e.g. a UTF-8 continuation) matches none of the digit ranges and` |
|    - | 1399 | ` * is skipped, so leading/interior multibyte junk is ignored like php.` |
|    - | 1400 | ` * Note: php also raises E_DEPRECATED for skipped invalid characters; that notice` |
|    - | 1401 | ` * is not emitted here (a §3.7 deprecation-fidelity residual, value is correct).` |
|    - | 1402 | ` */` |
|  334 | 1403 | `static void MathBaseToNumber(ph7_context *pCtx,const char *zStr,int nLen,int base)` |
|    3 | 1404 | `{` |
|  337 | 1405 | `	sxi64 num = 0;      /* Integer accumulator */` |
|  337 | 1406 | `	double fnum = 0;    /* Float accumulator (used once num would overflow) */` |
|  337 | 1407 | `	int mode = 0;       /* 0 -> integer accumulation, 1 -> switched to float */` |
|  337 | 1408 | `	sxi64 cutoff = SXI64_HIGH / base;      /* PHP_INT_MAX / base */` |
|  337 | 1409 | `	int cutlim = (int)(SXI64_HIGH % base); /* PHP_INT_MAX % base */` |
|  337 | 1410 | `	int bIgnored = 0;   /* any character skipped below? php deprecates that */` |
|    - | 1411 | `	int i;` |
| 1423 | 1412 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 1089 | 1413 | `		int c = (unsigned char)zStr[i];` |
| 1089 | 1414 | `		if( c >= '0' && c <= '9' ){` |
|  808 | 1415 | `			c -= '0';` |
|  686 | 1416 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|  ! 0 | 1417 | `			c -= 'A' - 10;` |
|  283 | 1418 | `		}else if( c >= 'a' && c <= 'z' ){` |
|  283 | 1419 | `			c -= 'a' - 10;` |
|  143 | 1420 | `		}else{` |
|  ! 0 | 1421 | `			bIgnored = 1;` |
|  ! 0 | 1422 | `			continue; /* Not a digit character: skip */` |
|    - | 1423 | `		}` |
| 1089 | 1424 | `		if( c >= base ){` |
|   14 | 1425 | `			bIgnored = 1;` |
|   14 | 1426 | `			continue; /* Digit out of range for this base: skip */` |
|    - | 1427 | `		}` |
| 1076 | 1428 | `		if( mode == 0 ){` |
| 1076 | 1429 | `			if( num < cutoff \|\| (num == cutoff && c <= cutlim) ){` |
| 1070 | 1430 | `				num = num * base + c;` |
| 1070 | 1431 | `				continue;` |
|    - | 1432 | `			}` |
|    - | 1433 | `			/* Adding this digit would overflow the 64-bit integer: fall back to` |
|    - | 1434 | `			 * float accumulation, seeding it with the value gathered so far. */` |
|    7 | 1435 | `			fnum = (double)num;` |
|    7 | 1436 | `			mode = 1;` |
|    3 | 1437 | `		}` |
|    7 | 1438 | `		fnum = fnum * base + c;` |
|    4 | 1439 | `	}` |
|  337 | 1440 | `	if( bIgnored ){` |
|    - | 1441 | `		/* php 8 skips characters that are not valid digits for this base and only` |
|    - | 1442 | `		 * DEPRECATES the skipping; §10 rejects the deprecated surface loudly, so this` |
|    - | 1443 | `		 * ValueError ABORTS the call (the result stored below never reaches the caller` |
|    - | 1444 | `		 * — the OP_CALL boundary reports the throw for us, VmHostFuncThrowRc).` |
|    - | 1445 | `		 * Twin-pinned by base_invalid_chars_abort{,_zend}.phpt. */` |
|   10 | 1446 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1447 | `			"Invalid characters passed for attempted conversion");` |
|   10 | 1448 | `		return;` |
|    - | 1449 | `	}` |
|  328 | 1450 | `	if( mode == 1 ){` |
|    7 | 1451 | `		ph7_result_double(pCtx,fnum);` |
|    4 | 1452 | `	}else{` |
|  322 | 1453 | `		ph7_result_int64(pCtx,num);` |
|    - | 1454 | `	}` |
|  170 | 1455 | `}` |
|    - | 1456 | `/*` |
|    - | 1457 | ` * int64 hexdec(string $hex_string)` |
|    - | 1458 | ` *  Hexadecimal to decimal.` |
|    - | 1459 | ` * Parameters` |
|    - | 1460 | ` *  $hex_string` |
|    - | 1461 | ` *   The hexadecimal string to convert` |
|    - | 1462 | ` * Return` |
|    - | 1463 | ` *  The decimal representation of hex_string (int, or float on overflow)` |
|    - | 1464 | ` */` |
|  294 | 1465 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 1466 | `{` |
|    - | 1467 | `	const char *zString;` |
|    - | 1468 | `	int nLen;` |
|  297 | 1469 | `	if( nArg < 1 ){` |
|    - | 1470 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1471 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1472 | `		return PH7_OK;` |
|    - | 1473 | `	}` |
|  297 | 1474 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1475 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1476 | `		char zBuf[64];` |
|  ! 0 | 1477 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1478 | `			"hexdec(): Argument #1 ($hex_string) must be of type string, %s given",` |
|  ! 0 | 1479 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1480 | `	}` |
|    - | 1481 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1482 | `	 * hex-parses that (hexdec(255) == hexdec("255") == 0x255), so route every` |
|    - | 1483 | `	 * non-throwing value through ph7_value_to_string rather than reading it as` |
|    - | 1484 | `	 * a decimal integer. */` |
|  297 | 1485 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  297 | 1486 | `	MathBaseToNumber(pCtx,zString,nLen,16);` |
|  297 | 1487 | `	return PH7_OK;` |
|  150 | 1488 | `}` |
|    - | 1489 | `/*` |
|    - | 1490 | ` * int64 bindec(string $bin_string)` |
|    - | 1491 | ` *  Binary to decimal.` |
|    - | 1492 | ` * Parameters` |
|    - | 1493 | ` *  $bin_string` |
|    - | 1494 | ` *   The binary string to convert` |
|    - | 1495 | ` * Return` |
|    - | 1496 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|    - | 1497 | ` */` |
|   22 | 1498 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1499 | `{` |
|    - | 1500 | `	const char *zString;` |
|    - | 1501 | `	int nLen;` |
|   23 | 1502 | `	if( nArg < 1 ){` |
|    - | 1503 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1504 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1505 | `		return PH7_OK;` |
|    - | 1506 | `	}` |
|   23 | 1507 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1508 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1509 | `		char zBuf[64];` |
|  ! 0 | 1510 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1511 | `			"bindec(): Argument #1 ($binary_string) must be of type string, %s given",` |
|  ! 0 | 1512 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1513 | `	}` |
|    - | 1514 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1515 | `	 * binary-parses that (bindec(11) == bindec("11") == 3). */` |
|   23 | 1516 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   23 | 1517 | `	MathBaseToNumber(pCtx,zString,nLen,2);` |
|   23 | 1518 | `	return PH7_OK;` |
|   12 | 1519 | `}` |
|    - | 1520 | `/*` |
|    - | 1521 | ` * int64 octdec(string $oct_string)` |
|    - | 1522 | ` *  Octal to decimal.` |
|    - | 1523 | ` * Parameters` |
|    - | 1524 | ` *  $oct_string` |
|    - | 1525 | ` *   The octal string to convert` |
|    - | 1526 | ` * Return` |
|    - | 1527 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|    - | 1528 | ` */` |
|   18 | 1529 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1530 | `{` |
|    - | 1531 | `	const char *zString;` |
|    - | 1532 | `	int nLen;` |
|   19 | 1533 | `	if( nArg < 1 ){` |
|    - | 1534 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1535 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1536 | `		return PH7_OK;` |
|    - | 1537 | `	}` |
|   19 | 1538 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1539 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1540 | `		char zBuf[64];` |
|  ! 0 | 1541 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1542 | `			"octdec(): Argument #1 ($octal_string) must be of type string, %s given",` |
|  ! 0 | 1543 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1544 | `	}` |
|    - | 1545 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1546 | `	 * octal-parses that (octdec(11) == octdec("11") == 9). */` |
|   19 | 1547 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   19 | 1548 | `	MathBaseToNumber(pCtx,zString,nLen,8);` |
|   19 | 1549 | `	return PH7_OK;` |
|   10 | 1550 | `}` |
|    - | 1551 | `/*` |
|    - | 1552 | ` * srand([int $seed])` |
|    - | 1553 | ` * mt_srand([int $seed])` |
|    - | 1554 | ` *  Seed the random number generator.` |
|    - | 1555 | ` * Parameters` |
|    - | 1556 | ` * $seed` |
|    - | 1557 | ` *  Optional seed value. php truncates it to 32 bits; a missing seed reseeds` |
|    - | 1558 | ` *  from OS entropy (a "random" seed), matching php's GENERATE_SEED().` |
|    - | 1559 | ` * Return` |
|    - | 1560 | ` *  null.` |
|    - | 1561 | ` * Note:` |
|    - | 1562 | ` *  srand()/mt_srand() are aliases (php 7.1+ backs both rand() and mt_rand()` |
|    - | 1563 | ` *  with the same MT19937). They reset only the userland generator, never the` |
|    - | 1564 | ` *  engine's internal RC4 entropy, so a seed makes rand()/mt_rand()/shuffle/` |
|    - | 1565 | ` *  str_shuffle/array_rand reproducible without disturbing object ids or uniqid.` |
|    - | 1566 | ` */` |
|  314 | 1567 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1568 | `{` |
|    - | 1569 | `	sxu32 nSeed;` |
|  316 | 1570 | `	int bLegacy = nArg > 1 && ph7_value_to_int64(apArg[1]) == PH7_MT_RAND_PHP;` |
|  316 | 1571 | `	if( bLegacy ){` |
|    - | 1572 | `		/* php 8.3 deprecated the legacy generator; the message carries no` |
|    - | 1573 | `		 * function prefix there. */` |
|   23 | 1574 | `		PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|    - | 1575 | `			"The MT_RAND_PHP variant of Mt19937 is deprecated");` |
|   11 | 1576 | `	}` |
|  316 | 1577 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    - | 1578 | `		/* php truncates the (weakly int-coerced) seed to 32 bits. */` |
|  310 | 1579 | `		nSeed = (sxu32)ph7_value_to_int64(apArg[0]);` |
|  156 | 1580 | `	}else{` |
|    - | 1581 | `		/* NULL is the declared default and means "no seed given": php reseeds` |
|    - | 1582 | `		 * from entropy for it, where this read it as the integer 0 — so` |
|    - | 1583 | ``		 * `mt_srand($cfg['seed'] ?? null)` pinned every run to one sequence. */`` |
|    - | 1584 | `		/* No seed: reseed from OS entropy, like php's GENERATE_SEED(). */` |
|    8 | 1585 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|  ! 0 | 1586 | `			nSeed = PH7_VmRandomNum(pCtx->pVm);` |
|  ! 0 | 1587 | `		}` |
|    - | 1588 | `	}` |
|    - | 1589 | `	/* $mode picks the GENERATOR, and php reads it as an equality test against` |
|    - | 1590 | `	 * MT_RAND_PHP alone: every other value, valid or not, is MT19937. It was` |
|    - | 1591 | `	 * declared in the signature and read by nothing, so a program that seeded` |
|    - | 1592 | `	 * with MT_RAND_PHP to reproduce a recorded sequence silently got a different` |
|    - | 1593 | `	 * one — and the constant naming it was undefined, so the call was a fatal. */` |
|  316 | 1594 | `	PH7_VmMtSrand(pCtx->pVm,nSeed,bLegacy);` |
|  316 | 1595 | `	ph7_result_null(pCtx);` |
|  316 | 1596 | `	return PH7_OK;` |
|    2 | 1597 | `}` |
|    - | 1598 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - | 1599 | `/*` |
|    - | 1600 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|    - | 1601 | ` *  Convert a number between arbitrary bases.` |
|    - | 1602 | ` * Parameters` |
|    - | 1603 | ` * $number` |
|    - | 1604 | ` *  The number to convert` |
|    - | 1605 | ` * $frombase` |
|    - | 1606 | ` *  The base number is in` |
|    - | 1607 | ` * $tobase` |
|    - | 1608 | ` *  The base to convert number to` |
|    - | 1609 | ` * Return` |
|    - | 1610 | ` *  Number converted to base tobase` |
|    - | 1611 | ` */` |
|   60 | 1612 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1613 | `{` |
|    - | 1614 | `	static const char zDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";` |
|    - | 1615 | `	int nLen,iFbase,iTobase,i;` |
|    - | 1616 | `	int bIgnored;` |
|    - | 1617 | `	ph7_int64 iFbase64,iTobase64;` |
|    - | 1618 | `	const char *zNum;` |
|   62 | 1619 | `	sxu64 uNum = 0;` |
|   62 | 1620 | `	if( nArg < 3 ){` |
|    - | 1621 | `		/* Return the empty string*/` |
|  ! 0 | 1622 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1623 | `		return PH7_OK;` |
|    - | 1624 | `	}` |
|    - | 1625 | `	/* Base numbers. Read them as 64-bit so an out-of-range base can't wrap through` |
|    - | 1626 | `	 * a 32-bit truncation back into the 2..36 window and bypass the check below. */` |
|   62 | 1627 | `	iFbase64 = ph7_value_to_int64(apArg[1]);` |
|   62 | 1628 | `	iTobase64 = ph7_value_to_int64(apArg[2]);` |
|    - | 1629 | `	/* PHP 8 throws a catchable ValueError for a base outside 2..36; from_base` |
|    - | 1630 | `	 * is validated before to_base, both before the string is even parsed. */` |
|   62 | 1631 | `	if( iFbase64 < 2 \|\| iFbase64 > 36 ){` |
|    7 | 1632 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1633 | `			"base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)");` |
|    - | 1634 | `	}` |
|   56 | 1635 | `	if( iTobase64 < 2 \|\| iTobase64 > 36 ){` |
|    5 | 1636 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1637 | `			"base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)");` |
|    - | 1638 | `	}` |
|    - | 1639 | `	/* Both bases are now known to fit in [2,36], so the int form is exact. */` |
|   52 | 1640 | `	iFbase  = (int)iFbase64;` |
|   52 | 1641 | `	iTobase = (int)iTobase64;` |
|    - | 1642 | `	/* Parse the input number in from_base. Every base is handled the same way:` |
|    - | 1643 | `	 * digits 0-9 then a-z/A-Z map to 0-35; a character that is not a valid digit for` |
|    - | 1644 | `	 * from_base is ignored, and php raises an E_DEPRECATED saying so. */` |
|   52 | 1645 | `	if( ph7_value_is_null(apArg[0]) ){` |
|  ! 0 | 1646 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1647 | `			"base_convert(): Argument #1 ($num) must be of type string, null given");` |
|    - | 1648 | `	}` |
|   52 | 1649 | `	zNum = ph7_value_to_string(apArg[0],&nLen);` |
|   52 | 1650 | `	bIgnored = 0;` |
|  162 | 1651 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  112 | 1652 | `		int c = (unsigned char)zNum[i];` |
|    - | 1653 | `		int d;` |
|  112 | 1654 | `		if( c >= '0' && c <= '9' ){` |
|   80 | 1655 | `			d = c - '0';` |
|   73 | 1656 | `		}else if( c >= 'a' && c <= 'z' ){` |
|   34 | 1657 | `			d = c - 'a' + 10;` |
|   16 | 1658 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|  ! 0 | 1659 | `			d = c - 'A' + 10;` |
|  ! 0 | 1660 | `		}else{` |
|  ! 0 | 1661 | `			d = 99;` |
|    - | 1662 | `		}` |
|  112 | 1663 | `		if( d >= iFbase ){` |
|    - | 1664 | `			/* Not a valid digit for this base: php skips it and deprecates the skip. */` |
|    6 | 1665 | `			bIgnored = 1;` |
|    6 | 1666 | `			continue;` |
|    - | 1667 | `		}` |
|  108 | 1668 | `		uNum = uNum * (sxu64)iFbase + (sxu64)d;` |
|   55 | 1669 | `	}` |
|   52 | 1670 | `	if( bIgnored ){` |
|    - | 1671 | `		/* §10 rejects php's deprecated surface loudly, and a throw ABORTS the call —` |
|    - | 1672 | `		 * the conversion below is not reached. See MathBaseToNumber's twin. */` |
|    6 | 1673 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1674 | `			"Invalid characters passed for attempted conversion");` |
|    - | 1675 | `	}` |
|    - | 1676 | `	/* Format the result in to_base using lowercase digits. */` |
|   48 | 1677 | `	if( uNum == 0 ){` |
|    5 | 1678 | `		ph7_result_string(pCtx,"0",1);` |
|    3 | 1679 | `	}else{` |
|    - | 1680 | `		char zOut[70]; /* base-2 of a 64-bit value fits in 64 digits */` |
|   44 | 1681 | `		int n = 0,j;` |
|  142 | 1682 | `		while( uNum > 0 ){` |
|  100 | 1683 | `			zOut[n++] = zDigits[uNum % (sxu64)iTobase];` |
|  100 | 1684 | `			uNum /= (sxu64)iTobase;` |
|    2 | 1685 | `		}` |
|    - | 1686 | `		/* Digits were produced least-significant first: reverse in place. */` |
|   84 | 1687 | `		for( j = 0 ; j < n/2 ; ++j ){` |
|   42 | 1688 | `			char t = zOut[j];` |
|   42 | 1689 | `			zOut[j] = zOut[n - 1 - j];` |
|   42 | 1690 | `			zOut[n - 1 - j] = t;` |
|   22 | 1691 | `		}` |
|   44 | 1692 | `		ph7_result_string(pCtx,zOut,n);` |
|    - | 1693 | `	}` |
|   48 | 1694 | `	return PH7_OK;` |
|   32 | 1695 | `}` |
|    - | 1696 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - | 1697 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1698 |  |
