# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 533/611 lines (87.23%)

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
|    2 |   34 | `PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   35 | `{` |
|    - |   36 | `	double r,x;` |
|    3 |   37 | `	if( nArg < 1 ){` |
|    - |   38 | `		/* Missing argument,return 0 */` |
|  ! 0 |   39 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   40 | `		return PH7_OK;` |
|    - |   41 | `	}` |
|    3 |   42 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |   43 | `	/* Perform the requested operation */` |
|    3 |   44 | `	r = sqrt(x);` |
|    - |   45 | `	/* store the result back */` |
|    3 |   46 | `	ph7_result_double(pCtx,r);` |
|    3 |   47 | `	return PH7_OK;` |
|    2 |   48 | `}` |
|    - |   49 | `/*` |
|    - |   50 | ` * float exp(float $arg )` |
|    - |   51 | ` *  Calculates the exponent of e.` |
|    - |   52 | ` * Parameter` |
|    - |   53 | ` *  The number to process.` |
|    - |   54 | ` * Return` |
|    - |   55 | ` *  'e' raised to the power of arg.` |
|    - |   56 | ` */` |
|   18 |   57 | `PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   58 | `{` |
|    - |   59 | `	double r,x;` |
|   19 |   60 | `	if( nArg < 1 ){` |
|    - |   61 | `		/* Missing argument,return 0 */` |
|  ! 0 |   62 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |   63 | `		return PH7_OK;` |
|    - |   64 | `	}` |
|   19 |   65 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |   66 | `	/* Perform the requested operation */` |
|   19 |   67 | `	r = exp(x);` |
|    - |   68 | `	/* store the result back */` |
|   19 |   69 | `	ph7_result_double(pCtx,r);` |
|   19 |   70 | `	return PH7_OK;` |
|   10 |   71 | `}` |
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
|    2 |  135 | `PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  136 | `{` |
|    - |  137 | `	double r,x;` |
|    3 |  138 | `	if( nArg < 1 ){` |
|    - |  139 | `		/* Missing argument,return 0 */` |
|  ! 0 |  140 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  141 | `		return PH7_OK;` |
|    - |  142 | `	}` |
|    3 |  143 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  144 | `	/* Perform the requested operation */` |
|    3 |  145 | `	r = cos(x);` |
|    - |  146 | `	/* store the result back */` |
|    3 |  147 | `	ph7_result_double(pCtx,r);` |
|    3 |  148 | `	return PH7_OK;` |
|    2 |  149 | `}` |
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
|   16 |  199 | `PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  200 | `{` |
|    - |  201 | `	double r,x;` |
|   17 |  202 | `	if( nArg < 1 ){` |
|    - |  203 | `		/* Missing argument,return 0 */` |
|  ! 0 |  204 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  205 | `		return PH7_OK;` |
|    - |  206 | `	}` |
|   17 |  207 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  208 | `	/* Perform the requested operation */` |
|   17 |  209 | `	r = cosh(x);` |
|    - |  210 | `	/* store the result back */` |
|   17 |  211 | `	ph7_result_double(pCtx,r);` |
|   17 |  212 | `	return PH7_OK;` |
|    9 |  213 | `}` |
|    - |  214 | `/*` |
|    - |  215 | ` * float sin(float $arg )` |
|    - |  216 | ` *  Sine.` |
|    - |  217 | ` * Parameter` |
|    - |  218 | ` *  The number to process.` |
|    - |  219 | ` * Return` |
|    - |  220 | ` *  The sine of arg.` |
|    - |  221 | ` */` |
|    2 |  222 | `PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  223 | `{` |
|    - |  224 | `	double r,x;` |
|    3 |  225 | `	if( nArg < 1 ){` |
|    - |  226 | `		/* Missing argument,return 0 */` |
|  ! 0 |  227 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  228 | `		return PH7_OK;` |
|    - |  229 | `	}` |
|    3 |  230 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  231 | `	/* Perform the requested operation */` |
|    3 |  232 | `	r = sin(x);` |
|    - |  233 | `	/* store the result back */` |
|    3 |  234 | `	ph7_result_double(pCtx,r);` |
|    3 |  235 | `	return PH7_OK;` |
|    2 |  236 | `}` |
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
|   18 |  286 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  287 | `{` |
|    - |  288 | `	double r,x;` |
|   19 |  289 | `	if( nArg < 1 ){` |
|    - |  290 | `		/* Missing argument,return 0 */` |
|  ! 0 |  291 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  292 | `		return PH7_OK;` |
|    - |  293 | `	}` |
|   19 |  294 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  295 | `	/* Perform the requested operation */` |
|   19 |  296 | `	r = sinh(x);` |
|    - |  297 | `	/* store the result back */` |
|   19 |  298 | `	ph7_result_double(pCtx,r);` |
|   19 |  299 | `	return PH7_OK;` |
|   10 |  300 | `}` |
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
|    4 |  363 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  364 | `{` |
|    - |  365 | `	double r,x;` |
|    5 |  366 | `	if( nArg < 1 ){` |
|    - |  367 | `		/* Missing argument,return 0 */` |
|  ! 0 |  368 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  369 | `		return PH7_OK;` |
|    - |  370 | `	}` |
|    5 |  371 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  372 | `	/* Perform the requested operation */` |
|    5 |  373 | `	r = tan(x);` |
|    - |  374 | `	/* store the result back */` |
|    5 |  375 | `	ph7_result_double(pCtx,r);` |
|    5 |  376 | `	return PH7_OK;` |
|    3 |  377 | `}` |
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
|   18 |  421 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  422 | `{` |
|    - |  423 | `	double r,x;` |
|   19 |  424 | `	if( nArg < 1 ){` |
|    - |  425 | `		/* Missing argument,return 0 */` |
|  ! 0 |  426 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  427 | `		return PH7_OK;` |
|    - |  428 | `	}` |
|   19 |  429 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  430 | `	/* Perform the requested operation */` |
|   19 |  431 | `	r = tanh(x);` |
|    - |  432 | `	/* store the result back */` |
|   19 |  433 | `	ph7_result_double(pCtx,r);` |
|   19 |  434 | `	return PH7_OK;` |
|   10 |  435 | `}` |
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
|  136 |  488 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 |  489 | `{` |
|    - |  490 | `	int is_float;` |
|    - |  491 | `	/* PHP requires exactly one argument. */` |
|  141 |  492 | `	if( nArg != 1 ){` |
|   15 |  493 | `		return PH7_VmThrowException(pCtx,` |
|    - |  494 | `			"ArgumentCountError",` |
|    - |  495 | `			"abs() expects exactly 1 argument, %d given",` |
|    4 |  496 | `			nArg` |
|    - |  497 | `			);` |
|    - |  498 | `	}` |
|    - |  499 |  |
|    - |  500 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|  131 |  501 | `	is_float = ph7_value_is_float(apArg[0]);` |
|  131 |  502 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
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
|  128 |  518 | `	if( is_float ){` |
|    - |  519 | `		double r,x;` |
|   99 |  520 | `		x = ph7_value_to_double(apArg[0]);` |
|    - |  521 | `		/* Perform the requested operation */` |
|   99 |  522 | `		r = fabs(x);` |
|   99 |  523 | `		ph7_result_double(pCtx,r);` |
|   50 |  524 | `	}else{` |
|    - |  525 | ``		/* Read the full 64-bit value (the old 32-bit `int abs()` truncated any`` |
|    - |  526 | `		 * magnitude above 2^31 and was UB on INT_MIN). */` |
|   30 |  527 | `		sxi64 x = ph7_value_to_int64(apArg[0]);` |
|   30 |  528 | `		if( x == SMALLEST_INT64 ){` |
|    - |  529 | `			/* abs(PHP_INT_MIN) has no int representation, so PHP returns a float. */` |
|    3 |  530 | `			ph7_result_double(pCtx,-(double)x);` |
|    2 |  531 | `		}else{` |
|   28 |  532 | `			ph7_result_int64(pCtx,x < 0 ? -x : x);` |
|    - |  533 | `		}` |
|    - |  534 | `	}` |
|  128 |  535 | `	return PH7_OK;` |
|   73 |  536 | `}` |
|    - |  537 | `/*` |
|    - |  538 | ` * float log(float $arg,[int/float $base])` |
|    - |  539 | ` *  Natural logarithm.` |
|    - |  540 | ` * Parameter` |
|    - |  541 | ` *  $arg: The number to process.` |
|    - |  542 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|    - |  543 | ` * Return` |
|    - |  544 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|    - |  545 | ` * Note:` |
|    - |  546 | ` *  only Natural log and base-10 log are supported.` |
|    - |  547 | ` */` |
|   12 |  548 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  549 | `{` |
|    - |  550 | `	double r,x;` |
|   13 |  551 | `	if( nArg < 1 ){` |
|    - |  552 | `		/* Missing argument,return 0 */` |
|  ! 0 |  553 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  554 | `		return PH7_OK;` |
|    - |  555 | `	}` |
|   13 |  556 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  557 | `	/* Perform the requested operation */` |
|   13 |  558 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|    - |  559 | `		/* Base-10 log */` |
|    5 |  560 | `		r = log10(x);` |
|    3 |  561 | `	}else{` |
|    9 |  562 | `		r = log(x);` |
|    - |  563 | `	}` |
|    - |  564 | `	/* store the result back */` |
|   13 |  565 | `	ph7_result_double(pCtx,r);` |
|   13 |  566 | `	return PH7_OK;` |
|    7 |  567 | `}` |
|    - |  568 | `/*` |
|    - |  569 | ` * float log10(float $arg )` |
|    - |  570 | ` *  Base-10 logarithm.` |
|    - |  571 | ` * Parameter` |
|    - |  572 | ` *  The number to process.` |
|    - |  573 | ` * Return` |
|    - |  574 | ` *  The Base-10 logarithm of the given number.` |
|    - |  575 | ` */` |
|   14 |  576 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  577 | `{` |
|    - |  578 | `	double r,x;` |
|   15 |  579 | `	if( nArg < 1 ){` |
|    - |  580 | `		/* Missing argument,return 0 */` |
|  ! 0 |  581 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  582 | `		return PH7_OK;` |
|    - |  583 | `	}` |
|   15 |  584 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  585 | `	/* Perform the requested operation */` |
|   15 |  586 | `	r = log10(x);` |
|    - |  587 | `	/* store the result back */` |
|   15 |  588 | `	ph7_result_double(pCtx,r);` |
|   15 |  589 | `	return PH7_OK;` |
|    8 |  590 | `}` |
|    - |  591 | `/*` |
|    - |  592 | ` * number pow(number $base,number $exp)` |
|    - |  593 | ` *  Exponential expression.` |
|    - |  594 | ` * Parameter` |
|    - |  595 | ` *  base` |
|    - |  596 | ` *  The base to use.` |
|    - |  597 | ` * exp` |
|    - |  598 | ` *  The exponent.` |
|    - |  599 | ` * Return` |
|    - |  600 | ` *  base raised to the power of exp.` |
|    - |  601 | ` *  If the result can be represented as integer it will be returned` |
|    - |  602 | ` *  as type integer, else it will be returned as type float.` |
|    - |  603 | ` */` |
|    6 |  604 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  605 | `{` |
|    - |  606 | `	double r,x,y;` |
|    7 |  607 | `	if( nArg < 1 ){` |
|    - |  608 | `		/* Missing argument,return 0 */` |
|  ! 0 |  609 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  610 | `		return PH7_OK;` |
|    - |  611 | `	}` |
|    7 |  612 | `	x = ph7_value_to_double(apArg[0]);` |
|    7 |  613 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  614 | `	/* Perform the requested operation */` |
|    7 |  615 | `	r = pow(x,y);` |
|    7 |  616 | `	ph7_result_double(pCtx,r);` |
|    7 |  617 | `	return PH7_OK;` |
|    4 |  618 | `}` |
|    - |  619 | `/*` |
|    - |  620 | ` * float pi(void)` |
|    - |  621 | ` *  Returns an approximation of pi.` |
|    - |  622 | ` * Note` |
|    - |  623 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|    - |  624 | ` * Return` |
|    - |  625 | ` *  The value of pi as float.` |
|    - |  626 | ` */` |
|    2 |  627 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  628 | `{` |
|    1 |  629 | `	SXUNUSED(nArg); /* cc warning */` |
|    1 |  630 | `	SXUNUSED(apArg);` |
|    3 |  631 | `	ph7_result_double(pCtx,PH7_PI);` |
|    3 |  632 | `	return PH7_OK;` |
|    1 |  633 | `}` |
|    - |  634 | `/*` |
|    - |  635 | ` * float fmod(float $x,float $y)` |
|    - |  636 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|    - |  637 | ` * Parameters` |
|    - |  638 | ` * $x` |
|    - |  639 | ` *  The dividend` |
|    - |  640 | ` * $y` |
|    - |  641 | ` *  The divisor` |
|    - |  642 | ` * Return` |
|    - |  643 | ` *  The floating point remainder of x/y.` |
|    - |  644 | ` */` |
|    2 |  645 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  646 | `{` |
|    - |  647 | `	double x,y,r;` |
|    3 |  648 | `	if( nArg < 2 ){` |
|    - |  649 | `		/* Missing arguments */` |
|  ! 0 |  650 | `		ph7_result_double(pCtx,0);` |
|  ! 0 |  651 | `		return PH7_OK;` |
|    - |  652 | `	}` |
|    - |  653 | `	/* Extract given arguments */` |
|    3 |  654 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  655 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  656 | `	/* Perform the requested operation */` |
|    3 |  657 | `	r = fmod(x,y);` |
|    - |  658 | `	/* Processing result */` |
|    3 |  659 | `	ph7_result_double(pCtx,r);` |
|    3 |  660 | `	return PH7_OK;` |
|    2 |  661 | `}` |
|    - |  662 | `/*` |
|    - |  663 | ` * float hypot(float $x,float $y)` |
|    - |  664 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|    - |  665 | ` * Parameters` |
|    - |  666 | ` * $x` |
|    - |  667 | ` *  Length of first side` |
|    - |  668 | ` * $y` |
|    - |  669 | ` *  Length of first side` |
|    - |  670 | ` * Return` |
|    - |  671 | ` *  Calculated length of the hypotenuse.` |
|    - |  672 | ` */` |
|    2 |  673 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  674 | `{` |
|    - |  675 | `	double x,y,r;` |
|    3 |  676 | `	if( nArg < 2 ){` |
|    - |  677 | `		/* Missing arguments */` |
|  ! 0 |  678 | `		ph7_result_double(pCtx,0);` |
|  ! 0 |  679 | `		return PH7_OK;` |
|    - |  680 | `	}` |
|    - |  681 | `	/* Extract given arguments */` |
|    3 |  682 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  683 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  684 | `	/* Perform the requested operation */` |
|    3 |  685 | `	r = hypot(x,y);` |
|    - |  686 | `	/* Processing result */` |
|    3 |  687 | `	ph7_result_double(pCtx,r);` |
|    3 |  688 | `	return PH7_OK;` |
|    2 |  689 | `}` |
|    - |  690 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|    - |  691 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  692 | `/*` |
|    - |  693 | ` * PHP rounding modes (mirror ext/standard/php_math_round_mode.h).` |
|    - |  694 | ` * Only the four HALF_* integer constants are exposed to userland` |
|    - |  695 | ` * (PHP_ROUND_HALF_UP..HALF_ODD, see constant.c); the CEILING/FLOOR/` |
|    - |  696 | ` * TOWARD_ZERO/AWAY_FROM_ZERO modes (5..8) have no userland constant but` |
|    - |  697 | ` * are reachable by passing the raw integer to round()'s 3rd argument,` |
|    - |  698 | ` * which PHP 8.5 still accepts, so all eight are honored here.` |
|    - |  699 | ` */` |
|    - |  700 | `#define PH7_ROUND_HALF_UP        1` |
|    - |  701 | `#define PH7_ROUND_HALF_DOWN      2` |
|    - |  702 | `#define PH7_ROUND_HALF_EVEN      3` |
|    - |  703 | `#define PH7_ROUND_HALF_ODD       4` |
|    - |  704 | `#define PH7_ROUND_CEILING        5` |
|    - |  705 | `#define PH7_ROUND_FLOOR          6` |
|    - |  706 | `#define PH7_ROUND_TOWARD_ZERO    7` |
|    - |  707 | `#define PH7_ROUND_AWAY_FROM_ZERO 8` |
|    - |  708 | `/*` |
|    - |  709 | ` * 10**power via an exact lookup table for 0..22, falling back to pow()` |
|    - |  710 | ` * otherwise. Port of php-src PHP-8.5 ext/standard/math.c php_intpow10().` |
|    - |  711 | ` */` |
|  144 |  712 | `static double MathIntPow10(int power)` |
|    1 |  713 | `{` |
|    - |  714 | `	static const double powers[] = {` |
|    - |  715 | `		1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,` |
|    - |  716 | `		1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22` |
|    - |  717 | `	};` |
|  145 |  718 | `	if( power < 0 \|\| power > 22 ){` |
|    3 |  719 | `		return pow(10.0, (double)power);` |
|    - |  720 | `	}` |
|  143 |  721 | `	return powers[power];` |
|   73 |  722 | `}` |
|  126 |  723 | `static double MathRoundBasicEdge(double integral, double exponent, int places)` |
|    1 |  724 | `{` |
|   64 |  725 | `	return (places > 0)` |
|   36 |  726 | `		? fabs((integral + copysign(0.5, integral)) / exponent)` |
|  108 |  727 | `		: fabs((integral + copysign(0.5, integral)) * exponent);` |
|    1 |  728 | `}` |
|   12 |  729 | `static double MathRoundZeroEdge(double integral, double exponent, int places)` |
|    1 |  730 | `{` |
|    7 |  731 | `	return (places > 0)` |
|  ! 0 |  732 | `		? fabs((integral) / exponent)` |
|   12 |  733 | `		: fabs((integral) * exponent);` |
|    1 |  734 | `}` |
|    - |  735 | `/*` |
|    - |  736 | ` * Round the extracted integral part according to the requested mode.` |
|    - |  737 | ` * Faithful port of php-src PHP-8.5 ext/standard/math.c php_round_helper().` |
|    - |  738 | ` */` |
|  142 |  739 | `static double MathRoundHelper(double integral, double value, double exponent, int places, int mode)` |
|    1 |  740 | `{` |
|  143 |  741 | `	double value_abs = fabs(value);` |
|    - |  742 | `	double edge_case;` |
|  143 |  743 | `	switch( mode ){` |
|   43 |  744 | `		case PH7_ROUND_HALF_UP:` |
|   87 |  745 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   87 |  746 | `			if( value_abs >= edge_case ){` |
|   71 |  747 | `				return integral + copysign(1.0, integral);` |
|    - |  748 | `			}` |
|   17 |  749 | `			return integral;` |
|    6 |  750 | `		case PH7_ROUND_HALF_DOWN:` |
|   13 |  751 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   13 |  752 | `			if( value_abs > edge_case ){` |
|  ! 0 |  753 | `				return integral + copysign(1.0, integral);` |
|    - |  754 | `			}` |
|   13 |  755 | `			return integral;` |
|    2 |  756 | `		case PH7_ROUND_CEILING:` |
|    5 |  757 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  758 | `			if( value > 0.0 && value_abs > edge_case ){` |
|    3 |  759 | `				return integral + 1.0;` |
|    - |  760 | `			}` |
|    3 |  761 | `			return integral;` |
|    2 |  762 | `		case PH7_ROUND_FLOOR:` |
|    5 |  763 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  764 | `			if( value < 0.0 && value_abs > edge_case ){` |
|    3 |  765 | `				return integral - 1.0;` |
|    - |  766 | `			}` |
|    3 |  767 | `			return integral;` |
|    2 |  768 | `		case PH7_ROUND_TOWARD_ZERO:` |
|    5 |  769 | `			return integral;` |
|    2 |  770 | `		case PH7_ROUND_AWAY_FROM_ZERO:` |
|    5 |  771 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  772 | `			if( value_abs > edge_case ){` |
|    5 |  773 | `				return integral + copysign(1.0, integral);` |
|    - |  774 | `			}` |
|  ! 0 |  775 | `			return integral;` |
|    8 |  776 | `		case PH7_ROUND_HALF_EVEN:` |
|   17 |  777 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   17 |  778 | `			if( value_abs > edge_case ){` |
|  ! 0 |  779 | `				return integral + copysign(1.0, integral);` |
|   17 |  780 | `			}else if( value_abs == edge_case ){` |
|   17 |  781 | `				if( fmod(integral, 2.0) != 0.0 ){ /* integral not even -> make it even */` |
|    9 |  782 | `					return integral + copysign(1.0, integral);` |
|    - |  783 | `				}` |
|    4 |  784 | `			}` |
|    9 |  785 | `			return integral;` |
|    6 |  786 | `		case PH7_ROUND_HALF_ODD:` |
|   13 |  787 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   13 |  788 | `			if( value_abs > edge_case ){` |
|  ! 0 |  789 | `				return integral + copysign(1.0, integral);` |
|   13 |  790 | `			}else if( value_abs == edge_case ){` |
|   13 |  791 | `				if( fmod(integral, 2.0) == 0.0 ){ /* integral even -> make it odd */` |
|    7 |  792 | `					return integral + copysign(1.0, integral);` |
|    - |  793 | `				}` |
|    3 |  794 | `			}` |
|    7 |  795 | `			return integral;` |
|  ! 0 |  796 | `		default:` |
|  ! 0 |  797 | `			return integral; /* unreachable: mode validated by the caller */` |
|    - |  798 | `	}` |
|   72 |  799 | `}` |
|    - |  800 | `/*` |
|    - |  801 | `` * Round `value` to `places` decimals in `mode`. Faithful port of php-src`` |
|    - |  802 | ` * PHP-8.5 ext/standard/math.c _php_math_round() — the post-8.4` |
|    - |  803 | ` * integer-extraction algorithm with the +/-1 floating-point error` |
|    - |  804 | ` * correction step, required for byte-exact results on cases such as` |
|    - |  805 | ` * round(0.285, 2) == 0.29 that the old naive "+0.5" approach got wrong.` |
|    - |  806 | ` */` |
|  150 |  807 | `static double MathRound(double value, int places, int mode)` |
|    1 |  808 | `{` |
|    - |  809 | `	double exponent, tmp_value, tmp_value2;` |
|    - |  810 | `	int abs_places;` |
|  151 |  811 | `	if( !isfinite(value) \|\| value == 0.0 ){` |
|    7 |  812 | `		return value;` |
|    - |  813 | `	}` |
|    - |  814 | `	/* mirror php-src's clamp away from INT_MIN */` |
|  145 |  815 | `	if( places < -2147483647 ){` |
|  ! 0 |  816 | `		places = -2147483647;` |
|  ! 0 |  817 | `	}` |
|  145 |  818 | `	abs_places = places < 0 ? -places : places;` |
|  145 |  819 | `	exponent = MathIntPow10(abs_places);` |
|    - |  820 | `	/*` |
|    - |  821 | `	 * Extracting the integer part can be off by one ULP due to float error` |
|    - |  822 | `	 * (e.g. floor(0.285 * 1e10) == 2849999999). Try +/-1 and keep it if it` |
|    - |  823 | ``	 * divides back to exactly `value`.`` |
|    - |  824 | `	 */` |
|  145 |  825 | `	if( value >= 0.0 ){` |
|  115 |  826 | `		tmp_value = floor(places > 0 ? value * exponent : value / exponent);` |
|  115 |  827 | `		tmp_value2 = tmp_value + 1.0;` |
|   58 |  828 | `	}else{` |
|   31 |  829 | `		tmp_value = ceil(places > 0 ? value * exponent : value / exponent);` |
|   31 |  830 | `		tmp_value2 = tmp_value - 1.0;` |
|    - |  831 | `	}` |
|  145 |  832 | `	if( (places > 0 ? tmp_value2 / exponent : tmp_value2 * exponent) == value ){` |
|    3 |  833 | `		tmp_value = tmp_value2;` |
|    1 |  834 | `	}` |
|    - |  835 | `	/* Beyond our precision, so rounding it is pointless. */` |
|  145 |  836 | `	if( fabs(tmp_value) >= 1e16 ){` |
|    3 |  837 | `		return value;` |
|    - |  838 | `	}` |
|  143 |  839 | `	tmp_value = MathRoundHelper(tmp_value, value, exponent, places, mode);` |
|  143 |  840 | `	if( abs_places < 23 ){` |
|  143 |  841 | `		tmp_value = (places > 0) ? tmp_value / exponent : tmp_value * exponent;` |
|   72 |  842 | `	}else{` |
|    - |  843 | `		/*` |
|    - |  844 | `		 * Simple division would lose precision here; round-trip through a` |
|    - |  845 | `		 * string exactly like php-src does (snprintf "%15fe%d" + strtod).` |
|    - |  846 | `		 * libc snprintf is used (not SyBufferFormat, which is not` |
|    - |  847 | `		 * correctly-rounded) so the low bits match PHP. (SyStrToReal now` |
|    - |  848 | `		 * delegates to strtod too; the direct call here simply mirrors` |
|    - |  849 | `		 * php-src's own snprintf+strtod pairing.)` |
|    - |  850 | `		 */` |
|    - |  851 | `		char zBuf[64];` |
|  ! 0 |  852 | `		snprintf(zBuf, sizeof(zBuf), "%15fe%d", tmp_value, -places);` |
|  ! 0 |  853 | `		zBuf[sizeof(zBuf)-1] = '\0';` |
|  ! 0 |  854 | `		tmp_value = strtod(zBuf, 0);` |
|  ! 0 |  855 | `		if( !isfinite(tmp_value) \|\| isnan(tmp_value) ){` |
|  ! 0 |  856 | `			tmp_value = value;` |
|  ! 0 |  857 | `		}` |
|    - |  858 | `	}` |
|  143 |  859 | `	return tmp_value;` |
|   76 |  860 | `}` |
|    - |  861 | `/*` |
|    - |  862 | ` * float round ( int\|float $num [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|    - |  863 | ` *  Rounds a float.` |
|    - |  864 | ` * Parameters` |
|    - |  865 | ` *  $num       The value to round.` |
|    - |  866 | ` *  $precision The optional number of decimal digits to round to. May be` |
|    - |  867 | ` *             negative (rounds to the left of the decimal point).` |
|    - |  868 | ` *  $mode      One of PHP_ROUND_HALF_UP (default) / _HALF_DOWN / _HALF_EVEN /` |
|    - |  869 | ` *             _HALF_ODD, or the 8.5 integer modes CEILING / FLOOR /` |
|    - |  870 | ` *             TOWARD_ZERO / AWAY_FROM_ZERO (5..8).` |
|    - |  871 | ` * Return` |
|    - |  872 | ` *  The rounded value as a float.` |
|    - |  873 | ` */` |
|  178 |  874 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  875 | `{` |
|    - |  876 | `	double value, r;` |
|  179 |  877 | `	int places = 0;` |
|  179 |  878 | `	int mode = PH7_ROUND_HALF_UP;` |
|    - |  879 | `	/*` |
|    - |  880 | `	 * Legacy PHL contract: no argument -> int(0). PHP throws an` |
|    - |  881 | `	 * ArgumentCountError here, but two PHL-only (--SKIPIF-- zend_version)` |
|    - |  882 | `	 * tests assert round()===0, so keep the historical behavior.` |
|    - |  883 | `	 */` |
|  179 |  884 | `	if( nArg < 1 ){` |
|  ! 0 |  885 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  886 | `		return PH7_OK;` |
|    - |  887 | `	}` |
|  179 |  888 | `	if( nArg > 3 ){` |
|    4 |  889 | `		return PH7_VmThrowException(pCtx,` |
|    - |  890 | `			"ArgumentCountError",` |
|    - |  891 | `			"round() expects at most 3 arguments, %d given",` |
|    1 |  892 | `			nArg` |
|    - |  893 | `			);` |
|    - |  894 | `	}` |
|    - |  895 | `	/*` |
|    - |  896 | `	 * Validate argument #1: only int/float (and numeric strings) are` |
|    - |  897 | `	 * accepted; every other type raises a TypeError (mirrors floor()/ceil()).` |
|    - |  898 | `	 */` |
|  177 |  899 | `	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){` |
|    7 |  900 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |  901 | `			int len;` |
|    5 |  902 | `			sxu8 bReal = FALSE;` |
|    5 |  903 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    5 |  904 | `			if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|    4 |  905 | `				return PH7_VmThrowException(pCtx,` |
|    - |  906 | `					"TypeError",` |
|    - |  907 | `					"round(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |  908 | `					ph7_type_name(apArg[0])` |
|    - |  909 | `					);` |
|    - |  910 | `			}` |
|    2 |  911 | `		}else{` |
|    4 |  912 | `			return PH7_VmThrowException(pCtx,` |
|    - |  913 | `				"TypeError",` |
|    - |  914 | `				"round(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |  915 | `				ph7_type_name(apArg[0])` |
|    - |  916 | `				);` |
|    - |  917 | `		}` |
|    1 |  918 | `	}` |
|    - |  919 | `	/* Precision (arg #2). Negative values are valid; clamp to int range. */` |
|  173 |  920 | `	if( nArg > 1 ){` |
|  143 |  921 | `		sxi64 prec = ph7_value_to_int64(apArg[1]);` |
|  143 |  922 | `		if( prec > 2147483647 ){` |
|  ! 0 |  923 | `			places = 2147483647;` |
|  143 |  924 | `		}else if( prec < -2147483647 ){` |
|  ! 0 |  925 | `			places = -2147483647;` |
|  ! 0 |  926 | `		}else{` |
|  143 |  927 | `			places = (int)prec;` |
|    - |  928 | `		}` |
|   71 |  929 | `	}` |
|    - |  930 | `	/*` |
|    - |  931 | `	 * Mode (arg #3). PHP 8.5 accepts the integer modes 1..8. Read the full` |
|    - |  932 | `	 * 64-bit value before range-checking so a large out-of-range mode cannot` |
|    - |  933 | `	 * alias a valid 1..8 via a truncating 32-bit cast (e.g. 0x1_0000_0003).` |
|    - |  934 | `	 */` |
|  173 |  935 | `	if( nArg > 2 ){` |
|   73 |  936 | `		sxi64 m = ph7_value_to_int64(apArg[2]);` |
|   73 |  937 | `		if( m < PH7_ROUND_HALF_UP \|\| m > PH7_ROUND_AWAY_FROM_ZERO ){` |
|    5 |  938 | `			return PH7_VmThrowException(pCtx,` |
|    - |  939 | `				"ValueError",` |
|    - |  940 | `				"round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)"` |
|    - |  941 | `				);` |
|    - |  942 | `		}` |
|   69 |  943 | `		mode = (int)m;` |
|   34 |  944 | `	}` |
|  169 |  945 | `	value = ph7_value_to_double(apArg[0]);` |
|    - |  946 | `	/* Integer input with non-negative precision needs no rounding. */` |
|  169 |  947 | `	if( ph7_value_is_int(apArg[0]) && places >= 0 ){` |
|   19 |  948 | `		ph7_result_double(pCtx,value);` |
|   19 |  949 | `		return PH7_OK;` |
|    - |  950 | `	}` |
|  151 |  951 | `	r = MathRound(value, places, mode);` |
|  151 |  952 | `	ph7_result_double(pCtx,r);` |
|  151 |  953 | `	return PH7_OK;` |
|   90 |  954 | `}` |
|    - |  955 | `/*` |
|    - |  956 | ` * int intdiv(int $a, int $b)` |
|    - |  957 | ` *  Integer division.` |
|    - |  958 | ` * Parameters` |
|    - |  959 | ` *  $a` |
|    - |  960 | ` *   Number to be divided.` |
|    - |  961 | ` *  $b` |
|    - |  962 | ` *   Number which divides the $a.` |
|    - |  963 | ` * Return` |
|    - |  964 | ` *  The integer quotient of the division of $a by $b.` |
|    - |  965 | ` */` |
|  148 |  966 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  967 | `{` |
|    - |  968 | `	sxi64 a,b;` |
|    - |  969 | `	/* PHP requires exactly two arguments. */` |
|  152 |  970 | `	if( nArg != 2 ){` |
|    4 |  971 | `		return PH7_VmThrowException(pCtx,` |
|    - |  972 | `			"ArgumentCountError",` |
|    - |  973 | `			"intdiv() expects exactly 2 arguments, %d given",` |
|    1 |  974 | `			nArg` |
|    - |  975 | `			);` |
|    - |  976 | `	}` |
|    - |  977 | `	/* Type-check argument 1 */` |
|  146 |  978 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|  149 |  979 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 |  980 | `		return PH7_VmThrowException(pCtx,` |
|    - |  981 | `			"TypeError",` |
|    - |  982 | `			"intdiv(): Argument #1 ($num1) must be of type int, %s given",` |
|  ! 0 |  983 | `			ph7_type_name(apArg[0])` |
|    - |  984 | `			);` |
|    - |  985 | `	}` |
|  149 |  986 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  987 | `		int len;` |
|  ! 0 |  988 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|  ! 0 |  989 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 |  990 | `			return PH7_VmThrowException(pCtx,` |
|    - |  991 | `				"TypeError",` |
|    - |  992 | `				"intdiv(): Argument #1 ($num1) must be of type int, string given"` |
|    - |  993 | `				);` |
|    - |  994 | `		}` |
|  ! 0 |  995 | `	}` |
|    - |  996 | `	/* Type-check argument 2 */` |
|  146 |  997 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|  149 |  998 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|  ! 0 |  999 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1000 | `			"TypeError",` |
|    - | 1001 | `			"intdiv(): Argument #2 ($num2) must be of type int, %s given",` |
|  ! 0 | 1002 | `			ph7_type_name(apArg[1])` |
|    - | 1003 | `			);` |
|    - | 1004 | `	}` |
|  149 | 1005 | `	if( ph7_value_is_string(apArg[1]) ){` |
|    - | 1006 | `		int len;` |
|  ! 0 | 1007 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|  ! 0 | 1008 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 | 1009 | `			return PH7_VmThrowException(pCtx,` |
|    - | 1010 | `				"TypeError",` |
|    - | 1011 | `				"intdiv(): Argument #2 ($num2) must be of type int, string given"` |
|    - | 1012 | `				);` |
|    - | 1013 | `		}` |
|  ! 0 | 1014 | `	}` |
|    - | 1015 | `	/* Convert both arguments to int64 */` |
|    - | 1016 | `	{` |
|    - | 1017 | `		/* php's ZPP contract for the two int params (lossy float / float-string` |
|    - | 1018 | `		 * deprecations); the manual type checks above already covered arrays,` |
|    - | 1019 | `		 * objects and non-numeric strings with the same messages. */` |
|  149 | 1020 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[0],"intdiv",1,"$num1","int",&a);` |
|  149 | 1021 | `		if( rcArg != PH7_OK ){` |
|  ! 0 | 1022 | `			return rcArg;` |
|    - | 1023 | `		}` |
|  149 | 1024 | `		rcArg = PH7_IntArgResolve(pCtx,apArg[1],"intdiv",2,"$num2","int",&b);` |
|  149 | 1025 | `		if( rcArg != PH7_OK ){` |
|  ! 0 | 1026 | `			return rcArg;` |
|    - | 1027 | `		}` |
|    - | 1028 | `	}` |
|    - | 1029 | `	/* Check for division by zero */` |
|  149 | 1030 | `	if( b == 0 ){` |
|    3 | 1031 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1032 | `			"DivisionByZeroError",` |
|    - | 1033 | `			"Division by zero"` |
|    - | 1034 | `			);` |
|    - | 1035 | `	}` |
|    - | 1036 | `	/* Check for overflow: PHP_INT_MIN / -1 */` |
|  146 | 1037 | `	if( a == SMALLEST_INT64 && b == -1 ){` |
|    3 | 1038 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1039 | `			"ArithmeticError",` |
|    - | 1040 | `			"Division of PHP_INT_MIN by -1 is not an integer"` |
|    - | 1041 | `			);` |
|    - | 1042 | `	}` |
|    - | 1043 | `	/* Perform integer division */` |
|  143 | 1044 | `	ph7_result_int64(pCtx, a / b);` |
|  143 | 1045 | `	return PH7_OK;` |
|   78 | 1046 | `}` |
|    - | 1047 | `/*` |
|    - | 1048 | ` * string dechex(int $number)` |
|    - | 1049 | ` *  Decimal to hexadecimal.` |
|    - | 1050 | ` * Parameters` |
|    - | 1051 | ` *  $number` |
|    - | 1052 | ` *   Decimal value to convert` |
|    - | 1053 | ` * Return` |
|    - | 1054 | ` *  Hexadecimal string representation of number` |
|    - | 1055 | ` */` |
|   14 | 1056 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1057 | `{` |
|    - | 1058 | `	ph7_int64 iVal;` |
|   15 | 1059 | `	if( nArg < 1 ){` |
|    - | 1060 | `		/* Missing arguments,return null */` |
|  ! 0 | 1061 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1062 | `		return PH7_OK;` |
|    - | 1063 | `	}` |
|    - | 1064 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   15 | 1065 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1066 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement,` |
|    - | 1067 | `	 * so a negative value prints all 16 hex digits like PHP. */` |
|   15 | 1068 | `	ph7_result_string_format(pCtx,"%qx",iVal);` |
|   15 | 1069 | `	return PH7_OK;` |
|    8 | 1070 | `}` |
|    - | 1071 | `/*` |
|    - | 1072 | ` * string decoct(int $number)` |
|    - | 1073 | ` *  Decimal to Octal.` |
|    - | 1074 | ` * Parameters` |
|    - | 1075 | ` *  $number` |
|    - | 1076 | ` *   Decimal value to convert` |
|    - | 1077 | ` * Return` |
|    - | 1078 | ` *  Octal string representation of number` |
|    - | 1079 | ` */` |
|   12 | 1080 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1081 | `{` |
|    - | 1082 | `	ph7_int64 iVal;` |
|   13 | 1083 | `	if( nArg < 1 ){` |
|    - | 1084 | `		/* Missing arguments,return null */` |
|  ! 0 | 1085 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1086 | `		return PH7_OK;` |
|    - | 1087 | `	}` |
|    - | 1088 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   13 | 1089 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1090 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|   13 | 1091 | `	ph7_result_string_format(pCtx,"%qo",iVal);` |
|   13 | 1092 | `	return PH7_OK;` |
|    7 | 1093 | `}` |
|    - | 1094 | `/*` |
|    - | 1095 | ` * string decbin(int $number)` |
|    - | 1096 | ` *  Decimal to binary.` |
|    - | 1097 | ` * Parameters` |
|    - | 1098 | ` *  $number` |
|    - | 1099 | ` *   Decimal value to convert` |
|    - | 1100 | ` * Return` |
|    - | 1101 | ` *  Binary string representation of number` |
|    - | 1102 | ` */` |
|   10 | 1103 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1104 | `{` |
|    - | 1105 | `	ph7_int64 iVal;` |
|   11 | 1106 | `	if( nArg < 1 ){` |
|    - | 1107 | `		/* Missing arguments,return null */` |
|  ! 0 | 1108 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1109 | `		return PH7_OK;` |
|    - | 1110 | `	}` |
|    - | 1111 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   11 | 1112 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1113 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|   11 | 1114 | `	ph7_result_string_format(pCtx,"%qB",iVal);` |
|   11 | 1115 | `	return PH7_OK;` |
|    6 | 1116 | `}` |
|    - | 1117 | `/*` |
|    - | 1118 | ` * Convert a base-2/8/16 digit string to a number, mirroring PHP's` |
|    - | 1119 | ` * _php_math_basetozval (ext/standard/math.c) so hexdec/octdec/bindec agree with` |
|    - | 1120 | ` * php byte-for-byte: walk every byte, decode a digit (0-9,a-z,A-Z) or skip any` |
|    - | 1121 | ` * invalid one, accumulate into a signed 64-bit integer and transparently promote` |
|    - | 1122 | ` * to a double once the value would overflow PHP_INT_MAX. The context result is` |
|    - | 1123 | ` * set to an int when it fits, otherwise a float — PHP returns a float for values` |
|    - | 1124 | ` * above PHP_INT_MAX (e.g. hexdec("ffffffffffffffff") == 1.8446744073709552E+19).` |
|    - | 1125 | ` * A byte >= 0x80 (e.g. a UTF-8 continuation) matches none of the digit ranges and` |
|    - | 1126 | ` * is skipped, so leading/interior multibyte junk is ignored like php.` |
|    - | 1127 | ` * Note: php also raises E_DEPRECATED for skipped invalid characters; that notice` |
|    - | 1128 | ` * is not emitted here (a §3.7 deprecation-fidelity residual, value is correct).` |
|    - | 1129 | ` */` |
|   68 | 1130 | `static void MathBaseToNumber(ph7_context *pCtx,const char *zStr,int nLen,int base)` |
|    2 | 1131 | `{` |
|   70 | 1132 | `	sxi64 num = 0;      /* Integer accumulator */` |
|   70 | 1133 | `	double fnum = 0;    /* Float accumulator (used once num would overflow) */` |
|   70 | 1134 | `	int mode = 0;       /* 0 -> integer accumulation, 1 -> switched to float */` |
|   70 | 1135 | `	sxi64 cutoff = SXI64_HIGH / base;      /* PHP_INT_MAX / base */` |
|   70 | 1136 | `	int cutlim = (int)(SXI64_HIGH % base); /* PHP_INT_MAX % base */` |
|   70 | 1137 | `	int bIgnored = 0;   /* any character skipped below? php deprecates that */` |
|    - | 1138 | `	int i;` |
|  674 | 1139 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  606 | 1140 | `		int c = (unsigned char)zStr[i];` |
|  606 | 1141 | `		if( c >= '0' && c <= '9' ){` |
|  476 | 1142 | `			c -= '0';` |
|  369 | 1143 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|   31 | 1144 | `			c -= 'A' - 10;` |
|  117 | 1145 | `		}else if( c >= 'a' && c <= 'z' ){` |
|   94 | 1146 | `			c -= 'a' - 10;` |
|   48 | 1147 | `		}else{` |
|    9 | 1148 | `			bIgnored = 1;` |
|    9 | 1149 | `			continue; /* Not a digit character: skip */` |
|    - | 1150 | `		}` |
|  598 | 1151 | `		if( c >= base ){` |
|   10 | 1152 | `			bIgnored = 1;` |
|   10 | 1153 | `			continue; /* Digit out of range for this base: skip */` |
|    - | 1154 | `		}` |
|  590 | 1155 | `		if( mode == 0 ){` |
|  590 | 1156 | `			if( num < cutoff \|\| (num == cutoff && c <= cutlim) ){` |
|  584 | 1157 | `				num = num * base + c;` |
|  584 | 1158 | `				continue;` |
|    - | 1159 | `			}` |
|    - | 1160 | `			/* Adding this digit would overflow the 64-bit integer: fall back to` |
|    - | 1161 | `			 * float accumulation, seeding it with the value gathered so far. */` |
|    7 | 1162 | `			fnum = (double)num;` |
|    7 | 1163 | `			mode = 1;` |
|    3 | 1164 | `		}` |
|    7 | 1165 | `		fnum = fnum * base + c;` |
|    4 | 1166 | `	}` |
|   70 | 1167 | `	if( bIgnored ){` |
|    - | 1168 | `		/* php 8: characters that are not valid digits for this base are skipped,` |
|    - | 1169 | `		 * and the skipping itself is deprecated (the VALUE is unaffected). */` |
|   14 | 1170 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|    - | 1171 | `			"Invalid characters passed for attempted conversion, these have been ignored");` |
|    6 | 1172 | `	}` |
|   70 | 1173 | `	if( mode == 1 ){` |
|    7 | 1174 | `		ph7_result_double(pCtx,fnum);` |
|    4 | 1175 | `	}else{` |
|   64 | 1176 | `		ph7_result_int64(pCtx,num);` |
|    - | 1177 | `	}` |
|   70 | 1178 | `}` |
|    - | 1179 | `/*` |
|    - | 1180 | ` * int64 hexdec(string $hex_string)` |
|    - | 1181 | ` *  Hexadecimal to decimal.` |
|    - | 1182 | ` * Parameters` |
|    - | 1183 | ` *  $hex_string` |
|    - | 1184 | ` *   The hexadecimal string to convert` |
|    - | 1185 | ` * Return` |
|    - | 1186 | ` *  The decimal representation of hex_string (int, or float on overflow)` |
|    - | 1187 | ` */` |
|   36 | 1188 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1189 | `{` |
|    - | 1190 | `	const char *zString;` |
|    - | 1191 | `	int nLen;` |
|   38 | 1192 | `	if( nArg < 1 ){` |
|    - | 1193 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1194 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1195 | `		return PH7_OK;` |
|    - | 1196 | `	}` |
|   38 | 1197 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1198 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1199 | `		char zBuf[64];` |
|    7 | 1200 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1201 | `			"hexdec(): Argument #1 ($hex_string) must be of type string, %s given",` |
|    2 | 1202 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1203 | `	}` |
|    - | 1204 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1205 | `	 * hex-parses that (hexdec(255) == hexdec("255") == 0x255), so route every` |
|    - | 1206 | `	 * non-throwing value through ph7_value_to_string rather than reading it as` |
|    - | 1207 | `	 * a decimal integer. */` |
|   34 | 1208 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   34 | 1209 | `	MathBaseToNumber(pCtx,zString,nLen,16);` |
|   34 | 1210 | `	return PH7_OK;` |
|   20 | 1211 | `}` |
|    - | 1212 | `/*` |
|    - | 1213 | ` * int64 bindec(string $bin_string)` |
|    - | 1214 | ` *  Binary to decimal.` |
|    - | 1215 | ` * Parameters` |
|    - | 1216 | ` *  $bin_string` |
|    - | 1217 | ` *   The binary string to convert` |
|    - | 1218 | ` * Return` |
|    - | 1219 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|    - | 1220 | ` */` |
|   24 | 1221 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1222 | `{` |
|    - | 1223 | `	const char *zString;` |
|    - | 1224 | `	int nLen;` |
|   25 | 1225 | `	if( nArg < 1 ){` |
|    - | 1226 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1227 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1228 | `		return PH7_OK;` |
|    - | 1229 | `	}` |
|   25 | 1230 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1231 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1232 | `		char zBuf[64];` |
|    7 | 1233 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1234 | `			"bindec(): Argument #1 ($binary_string) must be of type string, %s given",` |
|    2 | 1235 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1236 | `	}` |
|    - | 1237 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1238 | `	 * binary-parses that (bindec(11) == bindec("11") == 3). */` |
|   21 | 1239 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   21 | 1240 | `	MathBaseToNumber(pCtx,zString,nLen,2);` |
|   21 | 1241 | `	return PH7_OK;` |
|   13 | 1242 | `}` |
|    - | 1243 | `/*` |
|    - | 1244 | ` * int64 octdec(string $oct_string)` |
|    - | 1245 | ` *  Octal to decimal.` |
|    - | 1246 | ` * Parameters` |
|    - | 1247 | ` *  $oct_string` |
|    - | 1248 | ` *   The octal string to convert` |
|    - | 1249 | ` * Return` |
|    - | 1250 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|    - | 1251 | ` */` |
|   20 | 1252 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1253 | `{` |
|    - | 1254 | `	const char *zString;` |
|    - | 1255 | `	int nLen;` |
|   21 | 1256 | `	if( nArg < 1 ){` |
|    - | 1257 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1258 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1259 | `		return PH7_OK;` |
|    - | 1260 | `	}` |
|   21 | 1261 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1262 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1263 | `		char zBuf[64];` |
|    7 | 1264 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1265 | `			"octdec(): Argument #1 ($octal_string) must be of type string, %s given",` |
|    2 | 1266 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1267 | `	}` |
|    - | 1268 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1269 | `	 * octal-parses that (octdec(11) == octdec("11") == 9). */` |
|   17 | 1270 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   17 | 1271 | `	MathBaseToNumber(pCtx,zString,nLen,8);` |
|   17 | 1272 | `	return PH7_OK;` |
|   11 | 1273 | `}` |
|    - | 1274 | `/*` |
|    - | 1275 | ` * srand([int $seed])` |
|    - | 1276 | ` * mt_srand([int $seed])` |
|    - | 1277 | ` *  Seed the random number generator.` |
|    - | 1278 | ` * Parameters` |
|    - | 1279 | ` * $seed` |
|    - | 1280 | ` *  Optional seed value` |
|    - | 1281 | ` * Return` |
|    - | 1282 | ` *  null.` |
|    - | 1283 | ` * Note:` |
|    - | 1284 | ` *  THIS FUNCTION IS A NO-OP.` |
|    - | 1285 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|    - | 1286 | ` */` |
|   20 | 1287 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1288 | `{` |
|   10 | 1289 | `	SXUNUSED(nArg);` |
|   10 | 1290 | `	SXUNUSED(apArg);` |
|   21 | 1291 | `	ph7_result_null(pCtx);` |
|   21 | 1292 | `	return PH7_OK;` |
|    1 | 1293 | `}` |
|    - | 1294 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - | 1295 | `/*` |
|    - | 1296 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|    - | 1297 | ` *  Convert a number between arbitrary bases.` |
|    - | 1298 | ` * Parameters` |
|    - | 1299 | ` * $number` |
|    - | 1300 | ` *  The number to convert` |
|    - | 1301 | ` * $frombase` |
|    - | 1302 | ` *  The base number is in` |
|    - | 1303 | ` * $tobase` |
|    - | 1304 | ` *  The base to convert number to` |
|    - | 1305 | ` * Return` |
|    - | 1306 | ` *  Number converted to base tobase` |
|    - | 1307 | ` */` |
|   58 | 1308 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1309 | `{` |
|    - | 1310 | `	static const char zDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";` |
|    - | 1311 | `	int nLen,iFbase,iTobase,i;` |
|    - | 1312 | `	ph7_int64 iFbase64,iTobase64;` |
|    - | 1313 | `	const char *zNum;` |
|   59 | 1314 | `	sxu64 uNum = 0;` |
|   59 | 1315 | `	if( nArg < 3 ){` |
|    - | 1316 | `		/* Return the empty string*/` |
|  ! 0 | 1317 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1318 | `		return PH7_OK;` |
|    - | 1319 | `	}` |
|    - | 1320 | `	/* Base numbers. Read them as 64-bit so an out-of-range base can't wrap through` |
|    - | 1321 | `	 * a 32-bit truncation back into the 2..36 window and bypass the check below. */` |
|   59 | 1322 | `	iFbase64 = ph7_value_to_int64(apArg[1]);` |
|   59 | 1323 | `	iTobase64 = ph7_value_to_int64(apArg[2]);` |
|    - | 1324 | `	/* PHP 8 throws a catchable ValueError for a base outside 2..36; from_base` |
|    - | 1325 | `	 * is validated before to_base, both before the string is even parsed. */` |
|   59 | 1326 | `	if( iFbase64 < 2 \|\| iFbase64 > 36 ){` |
|    7 | 1327 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1328 | `			"base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)");` |
|    - | 1329 | `	}` |
|   53 | 1330 | `	if( iTobase64 < 2 \|\| iTobase64 > 36 ){` |
|    5 | 1331 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1332 | `			"base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)");` |
|    - | 1333 | `	}` |
|    - | 1334 | `	/* Both bases are now known to fit in [2,36], so the int form is exact. */` |
|   49 | 1335 | `	iFbase  = (int)iFbase64;` |
|   49 | 1336 | `	iTobase = (int)iTobase64;` |
|    - | 1337 | `	/* Parse the input number in from_base. Every base is handled the same way:` |
|    - | 1338 | `	 * digits 0-9 then a-z/A-Z map to 0-35; a character that is not a valid digit` |
|    - | 1339 | `	 * for from_base is ignored (PHP additionally raises an E_DEPRECATED for the` |
|    - | 1340 | `	 * ignored characters — not yet emitted, see PLAN §3.1). */` |
|   49 | 1341 | `	zNum = ph7_value_to_string(apArg[0],&nLen);` |
|  147 | 1342 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   99 | 1343 | `		int c = (unsigned char)zNum[i];` |
|    - | 1344 | `		int d;` |
|   99 | 1345 | `		if( c >= '0' && c <= '9' ){` |
|   73 | 1346 | `			d = c - '0';` |
|   63 | 1347 | `		}else if( c >= 'a' && c <= 'z' ){` |
|   25 | 1348 | `			d = c - 'a' + 10;` |
|   15 | 1349 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|    3 | 1350 | `			d = c - 'A' + 10;` |
|    2 | 1351 | `		}else{` |
|  ! 0 | 1352 | `			d = 99;` |
|    - | 1353 | `		}` |
|   99 | 1354 | `		if( d >= iFbase ){` |
|    - | 1355 | `			/* Not a valid digit for this base: skip it (PHP). */` |
|    3 | 1356 | `			continue;` |
|    - | 1357 | `		}` |
|   97 | 1358 | `		uNum = uNum * (sxu64)iFbase + (sxu64)d;` |
|   49 | 1359 | `	}` |
|    - | 1360 | `	/* Format the result in to_base using lowercase digits. */` |
|   49 | 1361 | `	if( uNum == 0 ){` |
|    9 | 1362 | `		ph7_result_string(pCtx,"0",1);` |
|    5 | 1363 | `	}else{` |
|    - | 1364 | `		char zOut[70]; /* base-2 of a 64-bit value fits in 64 digits */` |
|   41 | 1365 | `		int n = 0,j;` |
|  133 | 1366 | `		while( uNum > 0 ){` |
|   93 | 1367 | `			zOut[n++] = zDigits[uNum % (sxu64)iTobase];` |
|   93 | 1368 | `			uNum /= (sxu64)iTobase;` |
|    1 | 1369 | `		}` |
|    - | 1370 | `		/* Digits were produced least-significant first: reverse in place. */` |
|   79 | 1371 | `		for( j = 0 ; j < n/2 ; ++j ){` |
|   39 | 1372 | `			char t = zOut[j];` |
|   39 | 1373 | `			zOut[j] = zOut[n - 1 - j];` |
|   39 | 1374 | `			zOut[n - 1 - j] = t;` |
|   20 | 1375 | `		}` |
|   41 | 1376 | `		ph7_result_string(pCtx,zOut,n);` |
|    - | 1377 | `	}` |
|   49 | 1378 | `	return PH7_OK;` |
|   30 | 1379 | `}` |
|    - | 1380 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - | 1381 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1382 |  |
