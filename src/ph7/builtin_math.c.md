# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 554/587 lines (94.38%)

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
|    6 |   34 | `PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   35 | `{` |
|    - |   36 | `	double r,x;` |
|    7 |   37 | `	if( nArg < 1 ){` |
|    - |   38 | `		/* Missing argument,return 0 */` |
|    5 |   39 | `		ph7_result_int(pCtx,0);` |
|    5 |   40 | `		return PH7_OK;` |
|    - |   41 | `	}` |
|    3 |   42 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |   43 | `	/* Perform the requested operation */` |
|    3 |   44 | `	r = sqrt(x);` |
|    - |   45 | `	/* store the result back */` |
|    3 |   46 | `	ph7_result_double(pCtx,r);` |
|    3 |   47 | `	return PH7_OK;` |
|    4 |   48 | `}` |
|    - |   49 | `/*` |
|    - |   50 | ` * float exp(float $arg )` |
|    - |   51 | ` *  Calculates the exponent of e.` |
|    - |   52 | ` * Parameter` |
|    - |   53 | ` *  The number to process.` |
|    - |   54 | ` * Return` |
|    - |   55 | ` *  'e' raised to the power of arg.` |
|    - |   56 | ` */` |
|   20 |   57 | `PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   58 | `{` |
|    - |   59 | `	double r,x;` |
|   21 |   60 | `	if( nArg < 1 ){` |
|    - |   61 | `		/* Missing argument,return 0 */` |
|    3 |   62 | `		ph7_result_int(pCtx,0);` |
|    3 |   63 | `		return PH7_OK;` |
|    - |   64 | `	}` |
|   19 |   65 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |   66 | `	/* Perform the requested operation */` |
|   19 |   67 | `	r = exp(x);` |
|    - |   68 | `	/* store the result back */` |
|   19 |   69 | `	ph7_result_double(pCtx,r);` |
|   19 |   70 | `	return PH7_OK;` |
|   11 |   71 | `}` |
|    - |   72 | `/*` |
|    - |   73 | ` * float floor(float $arg )` |
|    - |   74 | ` *  Round fractions down.` |
|    - |   75 | ` * Parameter` |
|    - |   76 | ` *  The number to process.` |
|    - |   77 | ` * Return` |
|    - |   78 | ` *  Returns the next lowest integer value by rounding down value if necessary.` |
|    - |   79 | ` */` |
|   18 |   80 | `PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |   81 | `{` |
|    - |   82 | `	double r,x;` |
|    - |   83 | `	/* PHP requires exactly one argument. */` |
|   22 |   84 | `	if( nArg != 1 ){` |
|    8 |   85 | `		return PH7_VmThrowException(pCtx,` |
|    - |   86 | `			"ArgumentCountError",` |
|    - |   87 | `			"floor() expects exactly 1 argument, %d given",` |
|    2 |   88 | `			nArg` |
|    - |   89 | `			);` |
|    - |   90 | `	}` |
|    - |   91 | `	/*` |
|    - |   92 | `	 * Validate argument type. Only int/float (and numeric strings) are accepted.` |
|    - |   93 | `	 * Other types (including non-numeric strings) raise a TypeError just like` |
|    - |   94 | `	 * ceil() and other math functions.` |
|    - |   95 | `	 */` |
|   16 |   96 | `	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){` |
|    6 |   97 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |   98 | `			int len;` |
|    6 |   99 | `			sxu8 bReal = FALSE;` |
|    6 |  100 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  101 | `			sxi32 rcNum;` |
|    6 |  102 | `			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|    6 |  103 | `			if( rcNum != SXRET_OK ){` |
|    4 |  104 | `				return PH7_VmThrowException(pCtx,` |
|    - |  105 | `					"TypeError",` |
|    - |  106 | `					"floor(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |  107 | `					ph7_type_name(apArg[0])` |
|    - |  108 | `					);` |
|    - |  109 | `			}` |
|    2 |  110 | `		}else{` |
|    - |  111 | `			/* Disallow all other types (arrays, objects, resources, etc.) */` |
|  ! 0 |  112 | `			return PH7_VmThrowException(pCtx,` |
|    - |  113 | `				"TypeError",` |
|    - |  114 | `				"floor(): Argument #1 ($num) must be of type int\|float, %s given",` |
|  ! 0 |  115 | `				ph7_type_name(apArg[0])` |
|    - |  116 | `				);` |
|    - |  117 | `		}` |
|    1 |  118 | `	}` |
|    - |  119 |  |
|   13 |  120 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  121 | `	/* Perform the requested operation */` |
|   13 |  122 | `	r = floor(x);` |
|    - |  123 | `	/* store the result back */` |
|   13 |  124 | `	ph7_result_double(pCtx,r);` |
|   13 |  125 | `	return PH7_OK;` |
|   13 |  126 | `}` |
|    - |  127 | `/*` |
|    - |  128 | ` * float cos(float $arg )` |
|    - |  129 | ` *  Cosine.` |
|    - |  130 | ` * Parameter` |
|    - |  131 | ` *  The number to process.` |
|    - |  132 | ` * Return` |
|    - |  133 | ` *  The cosine of arg.` |
|    - |  134 | ` */` |
|    4 |  135 | `PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  136 | `{` |
|    - |  137 | `	double r,x;` |
|    5 |  138 | `	if( nArg < 1 ){` |
|    - |  139 | `		/* Missing argument,return 0 */` |
|    3 |  140 | `		ph7_result_int(pCtx,0);` |
|    3 |  141 | `		return PH7_OK;` |
|    - |  142 | `	}` |
|    3 |  143 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  144 | `	/* Perform the requested operation */` |
|    3 |  145 | `	r = cos(x);` |
|    - |  146 | `	/* store the result back */` |
|    3 |  147 | `	ph7_result_double(pCtx,r);` |
|    3 |  148 | `	return PH7_OK;` |
|    3 |  149 | `}` |
|    - |  150 | `/*` |
|    - |  151 | ` * float acos(float $arg )` |
|    - |  152 | ` *  Arc cosine.` |
|    - |  153 | ` * Parameter` |
|    - |  154 | ` *  The number to process.` |
|    - |  155 | ` * Return` |
|    - |  156 | ` *  The arc cosine of arg.` |
|    - |  157 | ` */` |
|   22 |  158 | `PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  159 | `{` |
|    - |  160 | `	double r, x;` |
|    - |  161 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|   26 |  162 | `	if( nArg != 1 ){` |
|    4 |  163 | `		return PH7_VmThrowException(pCtx,` |
|    - |  164 | `			"ArgumentCountError",` |
|    - |  165 | `			"acos() expects exactly 1 argument, %d given",` |
|    1 |  166 | `			nArg` |
|    - |  167 | `			);` |
|    - |  168 | `	}` |
|    - |  169 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|    - |  170 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|    - |  171 | `	 * the float conversion will handle them. */` |
|   23 |  172 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    8 |  173 | `		return PH7_VmThrowException(pCtx,` |
|    - |  174 | `			"TypeError",` |
|    - |  175 | `			"acos(): Argument #1 ($num) must be of type float, %s given",` |
|    2 |  176 | `			ph7_type_name(apArg[0])` |
|    - |  177 | `			);` |
|    - |  178 | `	}` |
|    - |  179 | `	/* Convert to double now that we know it's numeric. */` |
|   17 |  180 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  181 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|   17 |  182 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|    5 |  183 | `		r = PH7_NAN_VALUE();` |
|    3 |  184 | `	}else{` |
|   13 |  185 | `		r = acos(x);` |
|    - |  186 | `	}` |
|    - |  187 | `	/* store the result back */` |
|   17 |  188 | `	ph7_result_double(pCtx,r);` |
|   17 |  189 | `	return PH7_OK;` |
|   15 |  190 | `}` |
|    - |  191 | `/*` |
|    - |  192 | ` * float cosh(float $arg )` |
|    - |  193 | ` *  Hyperbolic cosine.` |
|    - |  194 | ` * Parameter` |
|    - |  195 | ` *  The number to process.` |
|    - |  196 | ` * Return` |
|    - |  197 | ` *  The hyperbolic cosine of arg.` |
|    - |  198 | ` */` |
|   18 |  199 | `PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  200 | `{` |
|    - |  201 | `	double r,x;` |
|   19 |  202 | `	if( nArg < 1 ){` |
|    - |  203 | `		/* Missing argument,return 0 */` |
|    3 |  204 | `		ph7_result_int(pCtx,0);` |
|    3 |  205 | `		return PH7_OK;` |
|    - |  206 | `	}` |
|   17 |  207 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  208 | `	/* Perform the requested operation */` |
|   17 |  209 | `	r = cosh(x);` |
|    - |  210 | `	/* store the result back */` |
|   17 |  211 | `	ph7_result_double(pCtx,r);` |
|   17 |  212 | `	return PH7_OK;` |
|   10 |  213 | `}` |
|    - |  214 | `/*` |
|    - |  215 | ` * float sin(float $arg )` |
|    - |  216 | ` *  Sine.` |
|    - |  217 | ` * Parameter` |
|    - |  218 | ` *  The number to process.` |
|    - |  219 | ` * Return` |
|    - |  220 | ` *  The sine of arg.` |
|    - |  221 | ` */` |
|    8 |  222 | `PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  223 | `{` |
|    - |  224 | `	double r,x;` |
|    9 |  225 | `	if( nArg < 1 ){` |
|    - |  226 | `		/* Missing argument,return 0 */` |
|    7 |  227 | `		ph7_result_int(pCtx,0);` |
|    7 |  228 | `		return PH7_OK;` |
|    - |  229 | `	}` |
|    3 |  230 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  231 | `	/* Perform the requested operation */` |
|    3 |  232 | `	r = sin(x);` |
|    - |  233 | `	/* store the result back */` |
|    3 |  234 | `	ph7_result_double(pCtx,r);` |
|    3 |  235 | `	return PH7_OK;` |
|    5 |  236 | `}` |
|    - |  237 | `/*` |
|    - |  238 | ` * float asin(float $arg )` |
|    - |  239 | ` *  Arc sine.` |
|    - |  240 | ` * Parameter` |
|    - |  241 | ` *  The number to process.` |
|    - |  242 | ` * Return` |
|    - |  243 | ` *  The arc sine of arg.` |
|    - |  244 | ` */` |
|   22 |  245 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  246 | `{` |
|    - |  247 | `	double r, x;` |
|    - |  248 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|   26 |  249 | `	if( nArg != 1 ){` |
|    4 |  250 | `		return PH7_VmThrowException(pCtx,` |
|    - |  251 | `			"ArgumentCountError",` |
|    - |  252 | `			"asin() expects exactly 1 argument, %d given",` |
|    1 |  253 | `			nArg` |
|    - |  254 | `			);` |
|    - |  255 | `	}` |
|    - |  256 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|    - |  257 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|    - |  258 | `	 * the float conversion will handle them. */` |
|   23 |  259 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    8 |  260 | `		return PH7_VmThrowException(pCtx,` |
|    - |  261 | `			"TypeError",` |
|    - |  262 | `			"asin(): Argument #1 ($num) must be of type float, %s given",` |
|    2 |  263 | `			ph7_type_name(apArg[0])` |
|    - |  264 | `			);` |
|    - |  265 | `	}` |
|    - |  266 | `	/* Convert to double now that we know it's numeric. */` |
|   17 |  267 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  268 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|   17 |  269 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|    5 |  270 | `		r = PH7_NAN_VALUE();` |
|    3 |  271 | `	}else{` |
|   13 |  272 | `		r = asin(x);` |
|    - |  273 | `	}` |
|    - |  274 | `	/* store the result back */` |
|   17 |  275 | `	ph7_result_double(pCtx,r);` |
|   17 |  276 | `	return PH7_OK;` |
|   15 |  277 | `}` |
|    - |  278 | `/*` |
|    - |  279 | ` * float sinh(float $arg )` |
|    - |  280 | ` *  Hyperbolic sine.` |
|    - |  281 | ` * Parameter` |
|    - |  282 | ` *  The number to process.` |
|    - |  283 | ` * Return` |
|    - |  284 | ` *  The hyperbolic sine of arg.` |
|    - |  285 | ` */` |
|   20 |  286 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  287 | `{` |
|    - |  288 | `	double r,x;` |
|   21 |  289 | `	if( nArg < 1 ){` |
|    - |  290 | `		/* Missing argument,return 0 */` |
|    3 |  291 | `		ph7_result_int(pCtx,0);` |
|    3 |  292 | `		return PH7_OK;` |
|    - |  293 | `	}` |
|   19 |  294 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  295 | `	/* Perform the requested operation */` |
|   19 |  296 | `	r = sinh(x);` |
|    - |  297 | `	/* store the result back */` |
|   19 |  298 | `	ph7_result_double(pCtx,r);` |
|   19 |  299 | `	return PH7_OK;` |
|   11 |  300 | `}` |
|    - |  301 | `/*` |
|    - |  302 | ` * float ceil(float $arg )` |
|    - |  303 | ` *  Round fractions up.` |
|    - |  304 | ` * Parameter` |
|    - |  305 | ` *  The number to process.` |
|    - |  306 | ` * Return` |
|    - |  307 | ` *  The next highest integer value by rounding up value if necessary.` |
|    - |  308 | ` */` |
|   14 |  309 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  310 | `{` |
|    - |  311 | `	double r,x;` |
|    - |  312 | `	/* PHP requires exactly one argument. */` |
|   18 |  313 | `	if( nArg != 1 ){` |
|    8 |  314 | `		return PH7_VmThrowException(pCtx,` |
|    - |  315 | `			"ArgumentCountError",` |
|    - |  316 | `			"ceil() expects exactly 1 argument, %d given",` |
|    2 |  317 | `			nArg` |
|    - |  318 | `			);` |
|    - |  319 | `	}` |
|    - |  320 | `	/*` |
|    - |  321 | `	 * PHP only accepts ints, floats or numeric strings.  Any other types` |
|    - |  322 | `	 * (in particular non-numeric strings) should raise a TypeError.  We` |
|    - |  323 | `	 * mimic the approach used by abs() and perform an explicit numeric` |
|    - |  324 | `	 * check on strings before converting to double.` |
|    - |  325 | `	 */` |
|   12 |  326 | `	if( !ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]) ){` |
|    6 |  327 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |  328 | `			int len;` |
|    6 |  329 | `			sxu8 bReal = FALSE;` |
|    6 |  330 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  331 | `			sxi32 rcNum;` |
|    6 |  332 | `			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|    6 |  333 | `			if( rcNum != SXRET_OK ){` |
|    3 |  334 | `				return PH7_VmThrowException(pCtx,` |
|    - |  335 | `					"TypeError",` |
|    - |  336 | `					"ceil(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  337 | `					);` |
|    - |  338 | `			}` |
|    2 |  339 | `		}else{` |
|    - |  340 | `			/* Reject arrays, objects, resources, booleans, NULL, etc. */` |
|  ! 0 |  341 | `			return PH7_VmThrowException(pCtx,` |
|    - |  342 | `				"TypeError",` |
|    - |  343 | `				"ceil(): Argument #1 ($num) must be of type int\|float"` |
|    - |  344 | `				);` |
|    - |  345 | `		}` |
|    1 |  346 | `	}` |
|    - |  347 |  |
|    9 |  348 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  349 | `	/* Perform the requested operation */` |
|    9 |  350 | `	r = ceil(x);` |
|    - |  351 | `	/* store the result back */` |
|    9 |  352 | `	ph7_result_double(pCtx,r);` |
|    9 |  353 | `	return PH7_OK;` |
|   11 |  354 | `}` |
|    - |  355 | `/*` |
|    - |  356 | ` * float tan(float $arg )` |
|    - |  357 | ` *  Tangent.` |
|    - |  358 | ` * Parameter` |
|    - |  359 | ` *  The number to process.` |
|    - |  360 | ` * Return` |
|    - |  361 | ` *  The tangent of arg.` |
|    - |  362 | ` */` |
|    6 |  363 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  364 | `{` |
|    - |  365 | `	double r,x;` |
|    7 |  366 | `	if( nArg < 1 ){` |
|    - |  367 | `		/* Missing argument,return 0 */` |
|    3 |  368 | `		ph7_result_int(pCtx,0);` |
|    3 |  369 | `		return PH7_OK;` |
|    - |  370 | `	}` |
|    5 |  371 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  372 | `	/* Perform the requested operation */` |
|    5 |  373 | `	r = tan(x);` |
|    - |  374 | `	/* store the result back */` |
|    5 |  375 | `	ph7_result_double(pCtx,r);` |
|    5 |  376 | `	return PH7_OK;` |
|    4 |  377 | `}` |
|    - |  378 | `/*` |
|    - |  379 | ` * float atan(float $arg )` |
|    - |  380 | ` *  Arc tangent.` |
|    - |  381 | ` * Parameter` |
|    - |  382 | ` *  The number to process.` |
|    - |  383 | ` * Return` |
|    - |  384 | ` *  The arc tangent of arg.` |
|    - |  385 | ` */` |
|   46 |  386 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 |  387 | `{` |
|    - |  388 | `	double r,x;` |
|    - |  389 | `	/* PHP enforces exactly one argument. */` |
|   51 |  390 | `	if( nArg != 1 ){` |
|   12 |  391 | `		return PH7_VmThrowException(pCtx,` |
|    - |  392 | `			"ArgumentCountError",` |
|    - |  393 | `			"atan() expects exactly 1 argument, %d given",` |
|    3 |  394 | `			nArg` |
|    - |  395 | `			);` |
|    - |  396 | `	}` |
|    - |  397 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, non-numeric strings).` |
|    - |  398 | `	 * PHP 8 reports a TypeError for wrong types. */` |
|   44 |  399 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|   16 |  400 | `		return PH7_VmThrowException(pCtx,` |
|    - |  401 | `			"TypeError",` |
|    - |  402 | `			"atan(): Argument #1 ($num) must be of type float, %s given",` |
|    4 |  403 | `			ph7_type_name(apArg[0])` |
|    - |  404 | `			);` |
|    - |  405 | `	}` |
|   33 |  406 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  407 | `	/* Perform the requested operation */` |
|   33 |  408 | `	r = atan(x);` |
|    - |  409 | `	/* store the result back */` |
|   33 |  410 | `	ph7_result_double(pCtx,r);` |
|   33 |  411 | `	return PH7_OK;` |
|   28 |  412 | `}` |
|    - |  413 | `/*` |
|    - |  414 | ` * float tanh(float $arg )` |
|    - |  415 | ` *  Hyperbolic tangent.` |
|    - |  416 | ` * Parameter` |
|    - |  417 | ` *  The number to process.` |
|    - |  418 | ` * Return` |
|    - |  419 | ` *  The Hyperbolic tangent of arg.` |
|    - |  420 | ` */` |
|   20 |  421 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  422 | `{` |
|    - |  423 | `	double r,x;` |
|   21 |  424 | `	if( nArg < 1 ){` |
|    - |  425 | `		/* Missing argument,return 0 */` |
|    3 |  426 | `		ph7_result_int(pCtx,0);` |
|    3 |  427 | `		return PH7_OK;` |
|    - |  428 | `	}` |
|   19 |  429 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  430 | `	/* Perform the requested operation */` |
|   19 |  431 | `	r = tanh(x);` |
|    - |  432 | `	/* store the result back */` |
|   19 |  433 | `	ph7_result_double(pCtx,r);` |
|   19 |  434 | `	return PH7_OK;` |
|   11 |  435 | `}` |
|    - |  436 | `/*` |
|    - |  437 | ` * float atan2(float $y,float $x)` |
|    - |  438 | ` *  Arc tangent of two variable.` |
|    - |  439 | ` * Parameter` |
|    - |  440 | ` *  $y = Dividend parameter.` |
|    - |  441 | ` *  $x = Divisor parameter.` |
|    - |  442 | ` * Return` |
|    - |  443 | ` *  The arc tangent of y/x in radian.` |
|    - |  444 | ` */` |
|   60 |  445 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 |  446 | `{` |
|    - |  447 | `	double r,x,y;` |
|    - |  448 | `	/* PHP enforces exactly two arguments. */` |
|   65 |  449 | `	if( nArg != 2 ){` |
|   12 |  450 | `		return PH7_VmThrowException(pCtx,` |
|    - |  451 | `			"ArgumentCountError",` |
|    - |  452 | `			"atan2() expects exactly 2 arguments, %d given",` |
|    3 |  453 | `			nArg` |
|    - |  454 | `			);` |
|    - |  455 | `	}` |
|    - |  456 | `	/* Type checking: reject non-numeric values for $y (argument #1). */` |
|   58 |  457 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|   11 |  458 | `		return PH7_VmThrowException(pCtx,` |
|    - |  459 | `			"TypeError",` |
|    - |  460 | `			"atan2(): Argument #1 ($y) must be of type float, %s given",` |
|    3 |  461 | `			ph7_type_name(apArg[0])` |
|    - |  462 | `			);` |
|    - |  463 | `	}` |
|    - |  464 | `	/* Type checking: reject non-numeric values for $x (argument #2). */` |
|   50 |  465 | `	if( !ph7_value_is_numeric(apArg[1]) ){` |
|    4 |  466 | `		return PH7_VmThrowException(pCtx,` |
|    - |  467 | `			"TypeError",` |
|    - |  468 | `			"atan2(): Argument #2 ($x) must be of type float, %s given",` |
|    2 |  469 | `			ph7_type_name(apArg[1])` |
|    - |  470 | `			);` |
|    - |  471 | `	}` |
|   47 |  472 | `	y = ph7_value_to_double(apArg[0]);` |
|   47 |  473 | `	x = ph7_value_to_double(apArg[1]);` |
|    - |  474 | `	/* Perform the requested operation */` |
|   47 |  475 | `	r = atan2(y,x);` |
|    - |  476 | `	/* store the result back */` |
|   47 |  477 | `	ph7_result_double(pCtx,r);` |
|   47 |  478 | `	return PH7_OK;` |
|   35 |  479 | `}` |
|    - |  480 | `/*` |
|    - |  481 | ` * float/int64 abs(float/int64 $arg )` |
|    - |  482 | ` *  Absolute value.` |
|    - |  483 | ` * Parameter` |
|    - |  484 | ` *  The number to process.` |
|    - |  485 | ` * Return` |
|    - |  486 | ` *  The absolute value of number.` |
|    - |  487 | ` */` |
|  128 |  488 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  489 | `{` |
|    - |  490 | `	int is_float;` |
|    - |  491 | `	/* PHP requires exactly one argument. */` |
|  132 |  492 | `	if( nArg != 1 ){` |
|   15 |  493 | `		return PH7_VmThrowException(pCtx,` |
|    - |  494 | `			"ArgumentCountError",` |
|    - |  495 | `			"abs() expects exactly 1 argument, %d given",` |
|    4 |  496 | `			nArg` |
|    - |  497 | `			);` |
|    - |  498 | `	}` |
|    - |  499 |  |
|    - |  500 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|  123 |  501 | `	is_float = ph7_value_is_float(apArg[0]);` |
|  123 |  502 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|    - |  503 | `		int len;` |
|   10 |  504 | `		sxu8 bReal = FALSE;` |
|   10 |  505 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  506 | `		sxi32 rcNum;` |
|   10 |  507 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|   10 |  508 | `		if( rcNum != SXRET_OK ){` |
|    3 |  509 | `			return PH7_VmThrowException(pCtx,` |
|    - |  510 | `				"TypeError",` |
|    - |  511 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  512 | `				);` |
|    - |  513 | `		}` |
|    7 |  514 | `		if( bReal ){` |
|    5 |  515 | `			is_float = 1;` |
|    2 |  516 | `		}` |
|    3 |  517 | `	}` |
|  120 |  518 | `	if( is_float ){` |
|    - |  519 | `		double r,x;` |
|  101 |  520 | `		x = ph7_value_to_double(apArg[0]);` |
|    - |  521 | `		/* Perform the requested operation */` |
|  101 |  522 | `		r = fabs(x);` |
|  101 |  523 | `		ph7_result_double(pCtx,r);` |
|   51 |  524 | `	}else{` |
|    - |  525 | `		int r,x;` |
|   20 |  526 | `		x = ph7_value_to_int(apArg[0]);` |
|    - |  527 | `		/* Perform the requested operation */` |
|   20 |  528 | `		r = abs(x);` |
|   20 |  529 | `		ph7_result_int(pCtx,r);` |
|    - |  530 | `	}` |
|  120 |  531 | `	return PH7_OK;` |
|   68 |  532 | `}` |
|    - |  533 | `/*` |
|    - |  534 | ` * float log(float $arg,[int/float $base])` |
|    - |  535 | ` *  Natural logarithm.` |
|    - |  536 | ` * Parameter` |
|    - |  537 | ` *  $arg: The number to process.` |
|    - |  538 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|    - |  539 | ` * Return` |
|    - |  540 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|    - |  541 | ` * Note:` |
|    - |  542 | ` *  only Natural log and base-10 log are supported.` |
|    - |  543 | ` */` |
|   14 |  544 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  545 | `{` |
|    - |  546 | `	double r,x;` |
|   15 |  547 | `	if( nArg < 1 ){` |
|    - |  548 | `		/* Missing argument,return 0 */` |
|    3 |  549 | `		ph7_result_int(pCtx,0);` |
|    3 |  550 | `		return PH7_OK;` |
|    - |  551 | `	}` |
|   13 |  552 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  553 | `	/* Perform the requested operation */` |
|   13 |  554 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|    - |  555 | `		/* Base-10 log */` |
|    5 |  556 | `		r = log10(x);` |
|    3 |  557 | `	}else{` |
|    9 |  558 | `		r = log(x);` |
|    - |  559 | `	}` |
|    - |  560 | `	/* store the result back */` |
|   13 |  561 | `	ph7_result_double(pCtx,r);` |
|   13 |  562 | `	return PH7_OK;` |
|    8 |  563 | `}` |
|    - |  564 | `/*` |
|    - |  565 | ` * float log10(float $arg )` |
|    - |  566 | ` *  Base-10 logarithm.` |
|    - |  567 | ` * Parameter` |
|    - |  568 | ` *  The number to process.` |
|    - |  569 | ` * Return` |
|    - |  570 | ` *  The Base-10 logarithm of the given number.` |
|    - |  571 | ` */` |
|   16 |  572 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  573 | `{` |
|    - |  574 | `	double r,x;` |
|   17 |  575 | `	if( nArg < 1 ){` |
|    - |  576 | `		/* Missing argument,return 0 */` |
|    3 |  577 | `		ph7_result_int(pCtx,0);` |
|    3 |  578 | `		return PH7_OK;` |
|    - |  579 | `	}` |
|   15 |  580 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  581 | `	/* Perform the requested operation */` |
|   15 |  582 | `	r = log10(x);` |
|    - |  583 | `	/* store the result back */` |
|   15 |  584 | `	ph7_result_double(pCtx,r);` |
|   15 |  585 | `	return PH7_OK;` |
|    9 |  586 | `}` |
|    - |  587 | `/*` |
|    - |  588 | ` * number pow(number $base,number $exp)` |
|    - |  589 | ` *  Exponential expression.` |
|    - |  590 | ` * Parameter` |
|    - |  591 | ` *  base` |
|    - |  592 | ` *  The base to use.` |
|    - |  593 | ` * exp` |
|    - |  594 | ` *  The exponent.` |
|    - |  595 | ` * Return` |
|    - |  596 | ` *  base raised to the power of exp.` |
|    - |  597 | ` *  If the result can be represented as integer it will be returned` |
|    - |  598 | ` *  as type integer, else it will be returned as type float.` |
|    - |  599 | ` */` |
|   10 |  600 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  601 | `{` |
|    - |  602 | `	double r,x,y;` |
|   11 |  603 | `	if( nArg < 1 ){` |
|    - |  604 | `		/* Missing argument,return 0 */` |
|    5 |  605 | `		ph7_result_int(pCtx,0);` |
|    5 |  606 | `		return PH7_OK;` |
|    - |  607 | `	}` |
|    7 |  608 | `	x = ph7_value_to_double(apArg[0]);` |
|    7 |  609 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  610 | `	/* Perform the requested operation */` |
|    7 |  611 | `	r = pow(x,y);` |
|    7 |  612 | `	ph7_result_double(pCtx,r);` |
|    7 |  613 | `	return PH7_OK;` |
|    6 |  614 | `}` |
|    - |  615 | `/*` |
|    - |  616 | ` * float pi(void)` |
|    - |  617 | ` *  Returns an approximation of pi.` |
|    - |  618 | ` * Note` |
|    - |  619 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|    - |  620 | ` * Return` |
|    - |  621 | ` *  The value of pi as float.` |
|    - |  622 | ` */` |
|    2 |  623 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  624 | `{` |
|    1 |  625 | `	SXUNUSED(nArg); /* cc warning */` |
|    1 |  626 | `	SXUNUSED(apArg);` |
|    3 |  627 | `	ph7_result_double(pCtx,PH7_PI);` |
|    3 |  628 | `	return PH7_OK;` |
|    1 |  629 | `}` |
|    - |  630 | `/*` |
|    - |  631 | ` * float fmod(float $x,float $y)` |
|    - |  632 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|    - |  633 | ` * Parameters` |
|    - |  634 | ` * $x` |
|    - |  635 | ` *  The dividend` |
|    - |  636 | ` * $y` |
|    - |  637 | ` *  The divisor` |
|    - |  638 | ` * Return` |
|    - |  639 | ` *  The floating point remainder of x/y.` |
|    - |  640 | ` */` |
|    8 |  641 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  642 | `{` |
|    - |  643 | `	double x,y,r;` |
|    9 |  644 | `	if( nArg < 2 ){` |
|    - |  645 | `		/* Missing arguments */` |
|    7 |  646 | `		ph7_result_double(pCtx,0);` |
|    7 |  647 | `		return PH7_OK;` |
|    - |  648 | `	}` |
|    - |  649 | `	/* Extract given arguments */` |
|    3 |  650 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  651 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  652 | `	/* Perform the requested operation */` |
|    3 |  653 | `	r = fmod(x,y);` |
|    - |  654 | `	/* Processing result */` |
|    3 |  655 | `	ph7_result_double(pCtx,r);` |
|    3 |  656 | `	return PH7_OK;` |
|    5 |  657 | `}` |
|    - |  658 | `/*` |
|    - |  659 | ` * float hypot(float $x,float $y)` |
|    - |  660 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|    - |  661 | ` * Parameters` |
|    - |  662 | ` * $x` |
|    - |  663 | ` *  Length of first side` |
|    - |  664 | ` * $y` |
|    - |  665 | ` *  Length of first side` |
|    - |  666 | ` * Return` |
|    - |  667 | ` *  Calculated length of the hypotenuse.` |
|    - |  668 | ` */` |
|    6 |  669 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  670 | `{` |
|    - |  671 | `	double x,y,r;` |
|    7 |  672 | `	if( nArg < 2 ){` |
|    - |  673 | `		/* Missing arguments */` |
|    5 |  674 | `		ph7_result_double(pCtx,0);` |
|    5 |  675 | `		return PH7_OK;` |
|    - |  676 | `	}` |
|    - |  677 | `	/* Extract given arguments */` |
|    3 |  678 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  679 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  680 | `	/* Perform the requested operation */` |
|    3 |  681 | `	r = hypot(x,y);` |
|    - |  682 | `	/* Processing result */` |
|    3 |  683 | `	ph7_result_double(pCtx,r);` |
|    3 |  684 | `	return PH7_OK;` |
|    4 |  685 | `}` |
|    - |  686 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|    - |  687 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  688 | `/*` |
|    - |  689 | ` * PHP rounding modes (mirror ext/standard/php_math_round_mode.h).` |
|    - |  690 | ` * Only the four HALF_* integer constants are exposed to userland` |
|    - |  691 | ` * (PHP_ROUND_HALF_UP..HALF_ODD, see constant.c); the CEILING/FLOOR/` |
|    - |  692 | ` * TOWARD_ZERO/AWAY_FROM_ZERO modes (5..8) have no userland constant but` |
|    - |  693 | ` * are reachable by passing the raw integer to round()'s 3rd argument,` |
|    - |  694 | ` * which PHP 8.5 still accepts, so all eight are honored here.` |
|    - |  695 | ` */` |
|    - |  696 | `#define PH7_ROUND_HALF_UP        1` |
|    - |  697 | `#define PH7_ROUND_HALF_DOWN      2` |
|    - |  698 | `#define PH7_ROUND_HALF_EVEN      3` |
|    - |  699 | `#define PH7_ROUND_HALF_ODD       4` |
|    - |  700 | `#define PH7_ROUND_CEILING        5` |
|    - |  701 | `#define PH7_ROUND_FLOOR          6` |
|    - |  702 | `#define PH7_ROUND_TOWARD_ZERO    7` |
|    - |  703 | `#define PH7_ROUND_AWAY_FROM_ZERO 8` |
|    - |  704 | `/*` |
|    - |  705 | ` * 10**power via an exact lookup table for 0..22, falling back to pow()` |
|    - |  706 | ` * otherwise. Port of php-src PHP-8.5 ext/standard/math.c php_intpow10().` |
|    - |  707 | ` */` |
|  142 |  708 | `static double MathIntPow10(int power)` |
|    1 |  709 | `{` |
|    - |  710 | `	static const double powers[] = {` |
|    - |  711 | `		1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,` |
|    - |  712 | `		1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22` |
|    - |  713 | `	};` |
|  143 |  714 | `	if( power < 0 \|\| power > 22 ){` |
|    3 |  715 | `		return pow(10.0, (double)power);` |
|    - |  716 | `	}` |
|  141 |  717 | `	return powers[power];` |
|   72 |  718 | `}` |
|  124 |  719 | `static double MathRoundBasicEdge(double integral, double exponent, int places)` |
|    1 |  720 | `{` |
|   63 |  721 | `	return (places > 0)` |
|   34 |  722 | `		? fabs((integral + copysign(0.5, integral)) / exponent)` |
|  107 |  723 | `		: fabs((integral + copysign(0.5, integral)) * exponent);` |
|    1 |  724 | `}` |
|   12 |  725 | `static double MathRoundZeroEdge(double integral, double exponent, int places)` |
|    1 |  726 | `{` |
|    7 |  727 | `	return (places > 0)` |
|  ! 0 |  728 | `		? fabs((integral) / exponent)` |
|   12 |  729 | `		: fabs((integral) * exponent);` |
|    1 |  730 | `}` |
|    - |  731 | `/*` |
|    - |  732 | ` * Round the extracted integral part according to the requested mode.` |
|    - |  733 | ` * Faithful port of php-src PHP-8.5 ext/standard/math.c php_round_helper().` |
|    - |  734 | ` */` |
|  140 |  735 | `static double MathRoundHelper(double integral, double value, double exponent, int places, int mode)` |
|    1 |  736 | `{` |
|  141 |  737 | `	double value_abs = fabs(value);` |
|    - |  738 | `	double edge_case;` |
|  141 |  739 | `	switch( mode ){` |
|   42 |  740 | `		case PH7_ROUND_HALF_UP:` |
|   85 |  741 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   85 |  742 | `			if( value_abs >= edge_case ){` |
|   69 |  743 | `				return integral + copysign(1.0, integral);` |
|    - |  744 | `			}` |
|   17 |  745 | `			return integral;` |
|    6 |  746 | `		case PH7_ROUND_HALF_DOWN:` |
|   13 |  747 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   13 |  748 | `			if( value_abs > edge_case ){` |
|  ! 0 |  749 | `				return integral + copysign(1.0, integral);` |
|    - |  750 | `			}` |
|   13 |  751 | `			return integral;` |
|    2 |  752 | `		case PH7_ROUND_CEILING:` |
|    5 |  753 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  754 | `			if( value > 0.0 && value_abs > edge_case ){` |
|    3 |  755 | `				return integral + 1.0;` |
|    - |  756 | `			}` |
|    3 |  757 | `			return integral;` |
|    2 |  758 | `		case PH7_ROUND_FLOOR:` |
|    5 |  759 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  760 | `			if( value < 0.0 && value_abs > edge_case ){` |
|    3 |  761 | `				return integral - 1.0;` |
|    - |  762 | `			}` |
|    3 |  763 | `			return integral;` |
|    2 |  764 | `		case PH7_ROUND_TOWARD_ZERO:` |
|    5 |  765 | `			return integral;` |
|    2 |  766 | `		case PH7_ROUND_AWAY_FROM_ZERO:` |
|    5 |  767 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  768 | `			if( value_abs > edge_case ){` |
|    5 |  769 | `				return integral + copysign(1.0, integral);` |
|    - |  770 | `			}` |
|  ! 0 |  771 | `			return integral;` |
|    8 |  772 | `		case PH7_ROUND_HALF_EVEN:` |
|   17 |  773 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   17 |  774 | `			if( value_abs > edge_case ){` |
|  ! 0 |  775 | `				return integral + copysign(1.0, integral);` |
|   17 |  776 | `			}else if( value_abs == edge_case ){` |
|   17 |  777 | `				if( fmod(integral, 2.0) != 0.0 ){ /* integral not even -> make it even */` |
|    9 |  778 | `					return integral + copysign(1.0, integral);` |
|    - |  779 | `				}` |
|    4 |  780 | `			}` |
|    9 |  781 | `			return integral;` |
|    6 |  782 | `		case PH7_ROUND_HALF_ODD:` |
|   13 |  783 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   13 |  784 | `			if( value_abs > edge_case ){` |
|  ! 0 |  785 | `				return integral + copysign(1.0, integral);` |
|   13 |  786 | `			}else if( value_abs == edge_case ){` |
|   13 |  787 | `				if( fmod(integral, 2.0) == 0.0 ){ /* integral even -> make it odd */` |
|    7 |  788 | `					return integral + copysign(1.0, integral);` |
|    - |  789 | `				}` |
|    3 |  790 | `			}` |
|    7 |  791 | `			return integral;` |
|  ! 0 |  792 | `		default:` |
|  ! 0 |  793 | `			return integral; /* unreachable: mode validated by the caller */` |
|    - |  794 | `	}` |
|   71 |  795 | `}` |
|    - |  796 | `/*` |
|    - |  797 | `` * Round `value` to `places` decimals in `mode`. Faithful port of php-src`` |
|    - |  798 | ` * PHP-8.5 ext/standard/math.c _php_math_round() — the post-8.4` |
|    - |  799 | ` * integer-extraction algorithm with the +/-1 floating-point error` |
|    - |  800 | ` * correction step, required for byte-exact results on cases such as` |
|    - |  801 | ` * round(0.285, 2) == 0.29 that the old naive "+0.5" approach got wrong.` |
|    - |  802 | ` */` |
|  148 |  803 | `static double MathRound(double value, int places, int mode)` |
|    1 |  804 | `{` |
|    - |  805 | `	double exponent, tmp_value, tmp_value2;` |
|    - |  806 | `	int abs_places;` |
|  149 |  807 | `	if( !isfinite(value) \|\| value == 0.0 ){` |
|    7 |  808 | `		return value;` |
|    - |  809 | `	}` |
|    - |  810 | `	/* mirror php-src's clamp away from INT_MIN */` |
|  143 |  811 | `	if( places < -2147483647 ){` |
|  ! 0 |  812 | `		places = -2147483647;` |
|  ! 0 |  813 | `	}` |
|  143 |  814 | `	abs_places = places < 0 ? -places : places;` |
|  143 |  815 | `	exponent = MathIntPow10(abs_places);` |
|    - |  816 | `	/*` |
|    - |  817 | `	 * Extracting the integer part can be off by one ULP due to float error` |
|    - |  818 | `	 * (e.g. floor(0.285 * 1e10) == 2849999999). Try +/-1 and keep it if it` |
|    - |  819 | ``	 * divides back to exactly `value`.`` |
|    - |  820 | `	 */` |
|  143 |  821 | `	if( value >= 0.0 ){` |
|  113 |  822 | `		tmp_value = floor(places > 0 ? value * exponent : value / exponent);` |
|  113 |  823 | `		tmp_value2 = tmp_value + 1.0;` |
|   57 |  824 | `	}else{` |
|   31 |  825 | `		tmp_value = ceil(places > 0 ? value * exponent : value / exponent);` |
|   31 |  826 | `		tmp_value2 = tmp_value - 1.0;` |
|    - |  827 | `	}` |
|  143 |  828 | `	if( (places > 0 ? tmp_value2 / exponent : tmp_value2 * exponent) == value ){` |
|    3 |  829 | `		tmp_value = tmp_value2;` |
|    1 |  830 | `	}` |
|    - |  831 | `	/* Beyond our precision, so rounding it is pointless. */` |
|  143 |  832 | `	if( fabs(tmp_value) >= 1e16 ){` |
|    3 |  833 | `		return value;` |
|    - |  834 | `	}` |
|  141 |  835 | `	tmp_value = MathRoundHelper(tmp_value, value, exponent, places, mode);` |
|  141 |  836 | `	if( abs_places < 23 ){` |
|  141 |  837 | `		tmp_value = (places > 0) ? tmp_value / exponent : tmp_value * exponent;` |
|   71 |  838 | `	}else{` |
|    - |  839 | `		/*` |
|    - |  840 | `		 * Simple division would lose precision here; round-trip through a` |
|    - |  841 | `		 * string exactly like php-src does (snprintf "%15fe%d" + strtod).` |
|    - |  842 | `		 * libc snprintf/strtod are used (not SyBufferFormat/SyStrToReal,` |
|    - |  843 | `		 * which are not correctly-rounded) so the low bits match PHP — the` |
|    - |  844 | `		 * same reason vm_serialize.c uses libc strtod for its float repr.` |
|    - |  845 | `		 */` |
|    - |  846 | `		char zBuf[64];` |
|  ! 0 |  847 | `		snprintf(zBuf, sizeof(zBuf), "%15fe%d", tmp_value, -places);` |
|  ! 0 |  848 | `		zBuf[sizeof(zBuf)-1] = '\0';` |
|  ! 0 |  849 | `		tmp_value = strtod(zBuf, 0);` |
|  ! 0 |  850 | `		if( !isfinite(tmp_value) \|\| isnan(tmp_value) ){` |
|  ! 0 |  851 | `			tmp_value = value;` |
|  ! 0 |  852 | `		}` |
|    - |  853 | `	}` |
|  141 |  854 | `	return tmp_value;` |
|   75 |  855 | `}` |
|    - |  856 | `/*` |
|    - |  857 | ` * float round ( int\|float $num [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|    - |  858 | ` *  Rounds a float.` |
|    - |  859 | ` * Parameters` |
|    - |  860 | ` *  $num       The value to round.` |
|    - |  861 | ` *  $precision The optional number of decimal digits to round to. May be` |
|    - |  862 | ` *             negative (rounds to the left of the decimal point).` |
|    - |  863 | ` *  $mode      One of PHP_ROUND_HALF_UP (default) / _HALF_DOWN / _HALF_EVEN /` |
|    - |  864 | ` *             _HALF_ODD, or the 8.5 integer modes CEILING / FLOOR /` |
|    - |  865 | ` *             TOWARD_ZERO / AWAY_FROM_ZERO (5..8).` |
|    - |  866 | ` * Return` |
|    - |  867 | ` *  The rounded value as a float.` |
|    - |  868 | ` */` |
|  172 |  869 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  870 | `{` |
|    - |  871 | `	double value, r;` |
|  173 |  872 | `	int places = 0;` |
|  173 |  873 | `	int mode = PH7_ROUND_HALF_UP;` |
|    - |  874 | `	/*` |
|    - |  875 | `	 * Legacy PHL contract: no argument -> int(0). PHP throws an` |
|    - |  876 | `	 * ArgumentCountError here, but two PHL-only (--SKIPIF-- zend_version)` |
|    - |  877 | `	 * tests assert round()===0, so keep the historical behavior.` |
|    - |  878 | `	 */` |
|  173 |  879 | `	if( nArg < 1 ){` |
|    5 |  880 | `		ph7_result_int(pCtx,0);` |
|    5 |  881 | `		return PH7_OK;` |
|    - |  882 | `	}` |
|  169 |  883 | `	if( nArg > 3 ){` |
|    4 |  884 | `		return PH7_VmThrowException(pCtx,` |
|    - |  885 | `			"ArgumentCountError",` |
|    - |  886 | `			"round() expects at most 3 arguments, %d given",` |
|    1 |  887 | `			nArg` |
|    - |  888 | `			);` |
|    - |  889 | `	}` |
|    - |  890 | `	/*` |
|    - |  891 | `	 * Validate argument #1: only int/float (and numeric strings) are` |
|    - |  892 | `	 * accepted; every other type raises a TypeError (mirrors floor()/ceil()).` |
|    - |  893 | `	 */` |
|  167 |  894 | `	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){` |
|    7 |  895 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |  896 | `			int len;` |
|    5 |  897 | `			sxu8 bReal = FALSE;` |
|    5 |  898 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    5 |  899 | `			if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|    4 |  900 | `				return PH7_VmThrowException(pCtx,` |
|    - |  901 | `					"TypeError",` |
|    - |  902 | `					"round(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |  903 | `					ph7_type_name(apArg[0])` |
|    - |  904 | `					);` |
|    - |  905 | `			}` |
|    2 |  906 | `		}else{` |
|    4 |  907 | `			return PH7_VmThrowException(pCtx,` |
|    - |  908 | `				"TypeError",` |
|    - |  909 | `				"round(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |  910 | `				ph7_type_name(apArg[0])` |
|    - |  911 | `				);` |
|    - |  912 | `		}` |
|    1 |  913 | `	}` |
|    - |  914 | `	/* Precision (arg #2). Negative values are valid; clamp to int range. */` |
|  163 |  915 | `	if( nArg > 1 ){` |
|  137 |  916 | `		sxi64 prec = ph7_value_to_int64(apArg[1]);` |
|  137 |  917 | `		if( prec > 2147483647 ){` |
|  ! 0 |  918 | `			places = 2147483647;` |
|  137 |  919 | `		}else if( prec < -2147483647 ){` |
|  ! 0 |  920 | `			places = -2147483647;` |
|  ! 0 |  921 | `		}else{` |
|  137 |  922 | `			places = (int)prec;` |
|    - |  923 | `		}` |
|   68 |  924 | `	}` |
|    - |  925 | `	/*` |
|    - |  926 | `	 * Mode (arg #3). PHP 8.5 accepts the integer modes 1..8. Read the full` |
|    - |  927 | `	 * 64-bit value before range-checking so a large out-of-range mode cannot` |
|    - |  928 | `	 * alias a valid 1..8 via a truncating 32-bit cast (e.g. 0x1_0000_0003).` |
|    - |  929 | `	 */` |
|  163 |  930 | `	if( nArg > 2 ){` |
|   73 |  931 | `		sxi64 m = ph7_value_to_int64(apArg[2]);` |
|   73 |  932 | `		if( m < PH7_ROUND_HALF_UP \|\| m > PH7_ROUND_AWAY_FROM_ZERO ){` |
|    5 |  933 | `			return PH7_VmThrowException(pCtx,` |
|    - |  934 | `				"ValueError",` |
|    - |  935 | `				"round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)"` |
|    - |  936 | `				);` |
|    - |  937 | `		}` |
|   69 |  938 | `		mode = (int)m;` |
|   34 |  939 | `	}` |
|  159 |  940 | `	value = ph7_value_to_double(apArg[0]);` |
|    - |  941 | `	/* Integer input with non-negative precision needs no rounding. */` |
|  159 |  942 | `	if( ph7_value_is_int(apArg[0]) && places >= 0 ){` |
|   11 |  943 | `		ph7_result_double(pCtx,value);` |
|   11 |  944 | `		return PH7_OK;` |
|    - |  945 | `	}` |
|  149 |  946 | `	r = MathRound(value, places, mode);` |
|  149 |  947 | `	ph7_result_double(pCtx,r);` |
|  149 |  948 | `	return PH7_OK;` |
|   87 |  949 | `}` |
|    - |  950 | `/*` |
|    - |  951 | ` * int intdiv(int $a, int $b)` |
|    - |  952 | ` *  Integer division.` |
|    - |  953 | ` * Parameters` |
|    - |  954 | ` *  $a` |
|    - |  955 | ` *   Number to be divided.` |
|    - |  956 | ` *  $b` |
|    - |  957 | ` *   Number which divides the $a.` |
|    - |  958 | ` * Return` |
|    - |  959 | ` *  The integer quotient of the division of $a by $b.` |
|    - |  960 | ` */` |
|   20 |  961 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  962 | `{` |
|    - |  963 | `	sxi64 a,b;` |
|    - |  964 | `	/* PHP requires exactly two arguments. */` |
|   24 |  965 | `	if( nArg != 2 ){` |
|    4 |  966 | `		return PH7_VmThrowException(pCtx,` |
|    - |  967 | `			"ArgumentCountError",` |
|    - |  968 | `			"intdiv() expects exactly 2 arguments, %d given",` |
|    1 |  969 | `			nArg` |
|    - |  970 | `			);` |
|    - |  971 | `	}` |
|    - |  972 | `	/* Type-check argument 1 */` |
|   18 |  973 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|   21 |  974 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 |  975 | `		return PH7_VmThrowException(pCtx,` |
|    - |  976 | `			"TypeError",` |
|    - |  977 | `			"intdiv(): Argument #1 ($num1) must be of type int, %s given",` |
|  ! 0 |  978 | `			ph7_type_name(apArg[0])` |
|    - |  979 | `			);` |
|    - |  980 | `	}` |
|   21 |  981 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  982 | `		int len;` |
|  ! 0 |  983 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|  ! 0 |  984 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 |  985 | `			return PH7_VmThrowException(pCtx,` |
|    - |  986 | `				"TypeError",` |
|    - |  987 | `				"intdiv(): Argument #1 ($num1) must be of type int, string given"` |
|    - |  988 | `				);` |
|    - |  989 | `		}` |
|  ! 0 |  990 | `	}` |
|    - |  991 | `	/* Type-check argument 2 */` |
|   18 |  992 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|   21 |  993 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|  ! 0 |  994 | `		return PH7_VmThrowException(pCtx,` |
|    - |  995 | `			"TypeError",` |
|    - |  996 | `			"intdiv(): Argument #2 ($num2) must be of type int, %s given",` |
|  ! 0 |  997 | `			ph7_type_name(apArg[1])` |
|    - |  998 | `			);` |
|    - |  999 | `	}` |
|   21 | 1000 | `	if( ph7_value_is_string(apArg[1]) ){` |
|    - | 1001 | `		int len;` |
|  ! 0 | 1002 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|  ! 0 | 1003 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 | 1004 | `			return PH7_VmThrowException(pCtx,` |
|    - | 1005 | `				"TypeError",` |
|    - | 1006 | `				"intdiv(): Argument #2 ($num2) must be of type int, string given"` |
|    - | 1007 | `				);` |
|    - | 1008 | `		}` |
|  ! 0 | 1009 | `	}` |
|    - | 1010 | `	/* Convert both arguments to int64 */` |
|   21 | 1011 | `	a = ph7_value_to_int64(apArg[0]);` |
|   21 | 1012 | `	b = ph7_value_to_int64(apArg[1]);` |
|    - | 1013 | `	/* Check for division by zero */` |
|   21 | 1014 | `	if( b == 0 ){` |
|    3 | 1015 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1016 | `			"DivisionByZeroError",` |
|    - | 1017 | `			"Division by zero"` |
|    - | 1018 | `			);` |
|    - | 1019 | `	}` |
|    - | 1020 | `	/* Check for overflow: PHP_INT_MIN / -1 */` |
|   18 | 1021 | `	if( a == SMALLEST_INT64 && b == -1 ){` |
|    3 | 1022 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1023 | `			"ArithmeticError",` |
|    - | 1024 | `			"Division of PHP_INT_MIN by -1 is not an integer"` |
|    - | 1025 | `			);` |
|    - | 1026 | `	}` |
|    - | 1027 | `	/* Perform integer division */` |
|   15 | 1028 | `	ph7_result_int64(pCtx, a / b);` |
|   15 | 1029 | `	return PH7_OK;` |
|   14 | 1030 | `}` |
|    - | 1031 | `/*` |
|    - | 1032 | ` * string dechex(int $number)` |
|    - | 1033 | ` *  Decimal to hexadecimal.` |
|    - | 1034 | ` * Parameters` |
|    - | 1035 | ` *  $number` |
|    - | 1036 | ` *   Decimal value to convert` |
|    - | 1037 | ` * Return` |
|    - | 1038 | ` *  Hexadecimal string representation of number` |
|    - | 1039 | ` */` |
|    8 | 1040 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1041 | `{` |
|    - | 1042 | `	int iVal;` |
|    9 | 1043 | `	if( nArg < 1 ){` |
|    - | 1044 | `		/* Missing arguments,return null */` |
|    5 | 1045 | `		ph7_result_null(pCtx);` |
|    5 | 1046 | `		return PH7_OK;` |
|    - | 1047 | `	}` |
|    - | 1048 | `	/* Extract the given number */` |
|    5 | 1049 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - | 1050 | `	/* Format */` |
|    5 | 1051 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|    5 | 1052 | `	return PH7_OK;` |
|    5 | 1053 | `}` |
|    - | 1054 | `/*` |
|    - | 1055 | ` * string decoct(int $number)` |
|    - | 1056 | ` *  Decimal to Octal.` |
|    - | 1057 | ` * Parameters` |
|    - | 1058 | ` *  $number` |
|    - | 1059 | ` *   Decimal value to convert` |
|    - | 1060 | ` * Return` |
|    - | 1061 | ` *  Octal string representation of number` |
|    - | 1062 | ` */` |
|    8 | 1063 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1064 | `{` |
|    - | 1065 | `	int iVal;` |
|    9 | 1066 | `	if( nArg < 1 ){` |
|    - | 1067 | `		/* Missing arguments,return null */` |
|    3 | 1068 | `		ph7_result_null(pCtx);` |
|    3 | 1069 | `		return PH7_OK;` |
|    - | 1070 | `	}` |
|    - | 1071 | `	/* Extract the given number */` |
|    7 | 1072 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - | 1073 | `	/* Format */` |
|    7 | 1074 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|    7 | 1075 | `	return PH7_OK;` |
|    5 | 1076 | `}` |
|    - | 1077 | `/*` |
|    - | 1078 | ` * string decbin(int $number)` |
|    - | 1079 | ` *  Decimal to binary.` |
|    - | 1080 | ` * Parameters` |
|    - | 1081 | ` *  $number` |
|    - | 1082 | ` *   Decimal value to convert` |
|    - | 1083 | ` * Return` |
|    - | 1084 | ` *  Binary string representation of number` |
|    - | 1085 | ` */` |
|    6 | 1086 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1087 | `{` |
|    - | 1088 | `	int iVal;` |
|    7 | 1089 | `	if( nArg < 1 ){` |
|    - | 1090 | `		/* Missing arguments,return null */` |
|    3 | 1091 | `		ph7_result_null(pCtx);` |
|    3 | 1092 | `		return PH7_OK;` |
|    - | 1093 | `	}` |
|    - | 1094 | `	/* Extract the given number */` |
|    5 | 1095 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - | 1096 | `	/* Format */` |
|    5 | 1097 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|    5 | 1098 | `	return PH7_OK;` |
|    4 | 1099 | `}` |
|    - | 1100 | `/*` |
|    - | 1101 | ` * int64 hexdec(string $hex_string)` |
|    - | 1102 | ` *  Hexadecimal to decimal.` |
|    - | 1103 | ` * Parameters` |
|    - | 1104 | ` *  $hex_string` |
|    - | 1105 | ` *   The hexadecimal string to convert` |
|    - | 1106 | ` * Return` |
|    - | 1107 | ` *  The decimal representation of hex_string` |
|    - | 1108 | ` */` |
|   24 | 1109 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1110 | `{` |
|    - | 1111 | `	const char *zString,*zEnd;` |
|    - | 1112 | `	ph7_int64 iVal;` |
|    - | 1113 | `	int nLen;` |
|   25 | 1114 | `	if( nArg < 1 ){` |
|    - | 1115 | `		/* Missing arguments,return -1 */` |
|    5 | 1116 | `		ph7_result_int(pCtx,-1);` |
|    5 | 1117 | `		return PH7_OK;` |
|    - | 1118 | `	}` |
|   21 | 1119 | `	iVal = 0;` |
|   21 | 1120 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - | 1121 | `		/* Extract the given string */` |
|   15 | 1122 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    - | 1123 | `		/* Delimit the string */` |
|   15 | 1124 | `		zEnd = &zString[nLen];` |
|    - | 1125 | `		/* Ignore non hex-stream */` |
|   21 | 1126 | `		while( zString < zEnd ){` |
|   21 | 1127 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|    - | 1128 | `				/* UTF-8 stream */` |
|    5 | 1129 | `				zString++;` |
|    9 | 1130 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|    5 | 1131 | `					zString++;` |
|    1 | 1132 | `				}` |
|    3 | 1133 | `			}else{` |
|   17 | 1134 | `				if( SyisHex(zString[0]) ){` |
|   15 | 1135 | `					break;` |
|    - | 1136 | `				}` |
|    - | 1137 | `				/* Ignore */` |
|    3 | 1138 | `				zString++;` |
|    - | 1139 | `			}` |
|    1 | 1140 | `		}` |
|   15 | 1141 | `		if( zString < zEnd ){` |
|    - | 1142 | `			/* Cast */` |
|   15 | 1143 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|    7 | 1144 | `		}` |
|    8 | 1145 | `	}else{` |
|    - | 1146 | `		/* Extract as a 64-bit integer */` |
|    7 | 1147 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1148 | `	}` |
|    - | 1149 | `	/* Return the number */` |
|   21 | 1150 | `	ph7_result_int64(pCtx,iVal);` |
|   21 | 1151 | `	return PH7_OK;` |
|   13 | 1152 | `}` |
|    - | 1153 | `/*` |
|    - | 1154 | ` * int64 bindec(string $bin_string)` |
|    - | 1155 | ` *  Binary to decimal.` |
|    - | 1156 | ` * Parameters` |
|    - | 1157 | ` *  $bin_string` |
|    - | 1158 | ` *   The binary string to convert` |
|    - | 1159 | ` * Return` |
|    - | 1160 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|    - | 1161 | ` */` |
|   14 | 1162 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1163 | `{` |
|    - | 1164 | `	const char *zString;` |
|    - | 1165 | `	ph7_int64 iVal;` |
|    - | 1166 | `	int nLen;` |
|   15 | 1167 | `	if( nArg < 1 ){` |
|    - | 1168 | `		/* Missing arguments,return -1 */` |
|    5 | 1169 | `		ph7_result_int(pCtx,-1);` |
|    5 | 1170 | `		return PH7_OK;` |
|    - | 1171 | `	}` |
|   11 | 1172 | `	iVal = 0;` |
|   11 | 1173 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - | 1174 | `		/* Extract the given string */` |
|    9 | 1175 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    9 | 1176 | `		if( nLen > 0 ){` |
|    - | 1177 | `			/* Perform a binary cast */` |
|    7 | 1178 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    3 | 1179 | `		}` |
|    5 | 1180 | `	}else{` |
|    - | 1181 | `		/* Extract as a 64-bit integer */` |
|    3 | 1182 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1183 | `	}` |
|    - | 1184 | `	/* Return the number */` |
|   11 | 1185 | `	ph7_result_int64(pCtx,iVal);` |
|   11 | 1186 | `	return PH7_OK;` |
|    8 | 1187 | `}` |
|    - | 1188 | `/*` |
|    - | 1189 | ` * int64 octdec(string $oct_string)` |
|    - | 1190 | ` *  Octal to decimal.` |
|    - | 1191 | ` * Parameters` |
|    - | 1192 | ` *  $oct_string` |
|    - | 1193 | ` *   The octal string to convert` |
|    - | 1194 | ` * Return` |
|    - | 1195 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|    - | 1196 | ` */` |
|    6 | 1197 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1198 | `{` |
|    - | 1199 | `	const char *zString;` |
|    - | 1200 | `	ph7_int64 iVal;` |
|    - | 1201 | `	int nLen;` |
|    7 | 1202 | `	if( nArg < 1 ){` |
|    - | 1203 | `		/* Missing arguments,return -1 */` |
|    3 | 1204 | `		ph7_result_int(pCtx,-1);` |
|    3 | 1205 | `		return PH7_OK;` |
|    - | 1206 | `	}` |
|    5 | 1207 | `	iVal = 0;` |
|    5 | 1208 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - | 1209 | `		/* Extract the given string */` |
|    3 | 1210 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    3 | 1211 | `		if( nLen > 0 ){` |
|    - | 1212 | `			/* Perform the cast */` |
|    3 | 1213 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    1 | 1214 | `		}` |
|    2 | 1215 | `	}else{` |
|    - | 1216 | `		/* Extract as a 64-bit integer */` |
|    3 | 1217 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1218 | `	}` |
|    - | 1219 | `	/* Return the number */` |
|    5 | 1220 | `	ph7_result_int64(pCtx,iVal);` |
|    5 | 1221 | `	return PH7_OK;` |
|    4 | 1222 | `}` |
|    - | 1223 | `/*` |
|    - | 1224 | ` * srand([int $seed])` |
|    - | 1225 | ` * mt_srand([int $seed])` |
|    - | 1226 | ` *  Seed the random number generator.` |
|    - | 1227 | ` * Parameters` |
|    - | 1228 | ` * $seed` |
|    - | 1229 | ` *  Optional seed value` |
|    - | 1230 | ` * Return` |
|    - | 1231 | ` *  null.` |
|    - | 1232 | ` * Note:` |
|    - | 1233 | ` *  THIS FUNCTION IS A NO-OP.` |
|    - | 1234 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|    - | 1235 | ` */` |
|   20 | 1236 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1237 | `{` |
|   10 | 1238 | `	SXUNUSED(nArg);` |
|   10 | 1239 | `	SXUNUSED(apArg);` |
|   21 | 1240 | `	ph7_result_null(pCtx);` |
|   21 | 1241 | `	return PH7_OK;` |
|    1 | 1242 | `}` |
|    - | 1243 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - | 1244 | `/*` |
|    - | 1245 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|    - | 1246 | ` *  Convert a number between arbitrary bases.` |
|    - | 1247 | ` * Parameters` |
|    - | 1248 | ` * $number` |
|    - | 1249 | ` *  The number to convert` |
|    - | 1250 | ` * $frombase` |
|    - | 1251 | ` *  The base number is in` |
|    - | 1252 | ` * $tobase` |
|    - | 1253 | ` *  The base to convert number to` |
|    - | 1254 | ` * Return` |
|    - | 1255 | ` *  Number converted to base tobase` |
|    - | 1256 | ` */` |
|   48 | 1257 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1258 | `{` |
|    - | 1259 | `	int nLen,iFbase,iTobase;` |
|    - | 1260 | `	const char *zNum;` |
|    - | 1261 | `	ph7_int64 iNum;` |
|   49 | 1262 | `	if( nArg < 3 ){` |
|    - | 1263 | `		/* Return the empty string*/` |
|   13 | 1264 | `		ph7_result_string(pCtx,"",0);` |
|   13 | 1265 | `		return PH7_OK;` |
|    - | 1266 | `	}` |
|    - | 1267 | `	/* Base numbers */` |
|   37 | 1268 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|   37 | 1269 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|   37 | 1270 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - | 1271 | `		/* Extract the target number */` |
|   33 | 1272 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|   33 | 1273 | `		if( nLen < 1 ){` |
|    - | 1274 | `			/* Return the empty string*/` |
|    5 | 1275 | `			ph7_result_string(pCtx,"",0);` |
|    5 | 1276 | `			return PH7_OK;` |
|    - | 1277 | `		}` |
|    - | 1278 | `		/* Base conversion */` |
|   29 | 1279 | `		switch(iFbase){` |
|    5 | 1280 | `		case 16:` |
|    - | 1281 | `			/* Hex */` |
|   11 | 1282 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|   11 | 1283 | `			break;` |
|    3 | 1284 | `		case 8:` |
|    - | 1285 | `			/* Octal */` |
|    7 | 1286 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    7 | 1287 | `			break;` |
|    2 | 1288 | `		case 2:` |
|    - | 1289 | `			/* Binary */` |
|    5 | 1290 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    5 | 1291 | `			break;` |
|    4 | 1292 | `		default:` |
|    - | 1293 | `			/* Decimal */` |
|    9 | 1294 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    8 | 1295 | `			break;` |
|    - | 1296 | `		}` |
|   15 | 1297 | `	}else{` |
|    5 | 1298 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|    - | 1299 | `	}` |
|   33 | 1300 | `	switch(iTobase){` |
|    3 | 1301 | `	case 16:` |
|    - | 1302 | `		/* Hex */` |
|    7 | 1303 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|    7 | 1304 | `		break;` |
|    1 | 1305 | `	case 8:` |
|    - | 1306 | `		/* Octal */` |
|    3 | 1307 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|    3 | 1308 | `		break;` |
|    1 | 1309 | `	case 2:` |
|    - | 1310 | `		/* Binary */` |
|    3 | 1311 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|    3 | 1312 | `		break;` |
|   11 | 1313 | `	default:` |
|    - | 1314 | `		/* Decimal */` |
|   23 | 1315 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|   22 | 1316 | `		break;` |
|    - | 1317 | `	}` |
|   33 | 1318 | `	return PH7_OK;` |
|   25 | 1319 | `}` |
|    - | 1320 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - | 1321 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1322 |  |
