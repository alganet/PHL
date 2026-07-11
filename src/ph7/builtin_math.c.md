# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 514/590 lines (87.12%)

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
|  128 |  488 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 |  489 | `{` |
|    - |  490 | `	int is_float;` |
|    - |  491 | `	/* PHP requires exactly one argument. */` |
|  133 |  492 | `	if( nArg != 1 ){` |
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
|   69 |  532 | `}` |
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
|   12 |  544 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  545 | `{` |
|    - |  546 | `	double r,x;` |
|   13 |  547 | `	if( nArg < 1 ){` |
|    - |  548 | `		/* Missing argument,return 0 */` |
|  ! 0 |  549 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  550 | `		return PH7_OK;` |
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
|    7 |  563 | `}` |
|    - |  564 | `/*` |
|    - |  565 | ` * float log10(float $arg )` |
|    - |  566 | ` *  Base-10 logarithm.` |
|    - |  567 | ` * Parameter` |
|    - |  568 | ` *  The number to process.` |
|    - |  569 | ` * Return` |
|    - |  570 | ` *  The Base-10 logarithm of the given number.` |
|    - |  571 | ` */` |
|   14 |  572 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  573 | `{` |
|    - |  574 | `	double r,x;` |
|   15 |  575 | `	if( nArg < 1 ){` |
|    - |  576 | `		/* Missing argument,return 0 */` |
|  ! 0 |  577 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  578 | `		return PH7_OK;` |
|    - |  579 | `	}` |
|   15 |  580 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  581 | `	/* Perform the requested operation */` |
|   15 |  582 | `	r = log10(x);` |
|    - |  583 | `	/* store the result back */` |
|   15 |  584 | `	ph7_result_double(pCtx,r);` |
|   15 |  585 | `	return PH7_OK;` |
|    8 |  586 | `}` |
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
|    6 |  600 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  601 | `{` |
|    - |  602 | `	double r,x,y;` |
|    7 |  603 | `	if( nArg < 1 ){` |
|    - |  604 | `		/* Missing argument,return 0 */` |
|  ! 0 |  605 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  606 | `		return PH7_OK;` |
|    - |  607 | `	}` |
|    7 |  608 | `	x = ph7_value_to_double(apArg[0]);` |
|    7 |  609 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  610 | `	/* Perform the requested operation */` |
|    7 |  611 | `	r = pow(x,y);` |
|    7 |  612 | `	ph7_result_double(pCtx,r);` |
|    7 |  613 | `	return PH7_OK;` |
|    4 |  614 | `}` |
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
|    2 |  641 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  642 | `{` |
|    - |  643 | `	double x,y,r;` |
|    3 |  644 | `	if( nArg < 2 ){` |
|    - |  645 | `		/* Missing arguments */` |
|  ! 0 |  646 | `		ph7_result_double(pCtx,0);` |
|  ! 0 |  647 | `		return PH7_OK;` |
|    - |  648 | `	}` |
|    - |  649 | `	/* Extract given arguments */` |
|    3 |  650 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  651 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  652 | `	/* Perform the requested operation */` |
|    3 |  653 | `	r = fmod(x,y);` |
|    - |  654 | `	/* Processing result */` |
|    3 |  655 | `	ph7_result_double(pCtx,r);` |
|    3 |  656 | `	return PH7_OK;` |
|    2 |  657 | `}` |
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
|    2 |  669 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  670 | `{` |
|    - |  671 | `	double x,y,r;` |
|    3 |  672 | `	if( nArg < 2 ){` |
|    - |  673 | `		/* Missing arguments */` |
|  ! 0 |  674 | `		ph7_result_double(pCtx,0);` |
|  ! 0 |  675 | `		return PH7_OK;` |
|    - |  676 | `	}` |
|    - |  677 | `	/* Extract given arguments */` |
|    3 |  678 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  679 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  680 | `	/* Perform the requested operation */` |
|    3 |  681 | `	r = hypot(x,y);` |
|    - |  682 | `	/* Processing result */` |
|    3 |  683 | `	ph7_result_double(pCtx,r);` |
|    3 |  684 | `	return PH7_OK;` |
|    2 |  685 | `}` |
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
|    - |  842 | `		 * libc snprintf is used (not SyBufferFormat, which is not` |
|    - |  843 | `		 * correctly-rounded) so the low bits match PHP. (SyStrToReal now` |
|    - |  844 | `		 * delegates to strtod too; the direct call here simply mirrors` |
|    - |  845 | `		 * php-src's own snprintf+strtod pairing.)` |
|    - |  846 | `		 */` |
|    - |  847 | `		char zBuf[64];` |
|  ! 0 |  848 | `		snprintf(zBuf, sizeof(zBuf), "%15fe%d", tmp_value, -places);` |
|  ! 0 |  849 | `		zBuf[sizeof(zBuf)-1] = '\0';` |
|  ! 0 |  850 | `		tmp_value = strtod(zBuf, 0);` |
|  ! 0 |  851 | `		if( !isfinite(tmp_value) \|\| isnan(tmp_value) ){` |
|  ! 0 |  852 | `			tmp_value = value;` |
|  ! 0 |  853 | `		}` |
|    - |  854 | `	}` |
|  141 |  855 | `	return tmp_value;` |
|   75 |  856 | `}` |
|    - |  857 | `/*` |
|    - |  858 | ` * float round ( int\|float $num [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|    - |  859 | ` *  Rounds a float.` |
|    - |  860 | ` * Parameters` |
|    - |  861 | ` *  $num       The value to round.` |
|    - |  862 | ` *  $precision The optional number of decimal digits to round to. May be` |
|    - |  863 | ` *             negative (rounds to the left of the decimal point).` |
|    - |  864 | ` *  $mode      One of PHP_ROUND_HALF_UP (default) / _HALF_DOWN / _HALF_EVEN /` |
|    - |  865 | ` *             _HALF_ODD, or the 8.5 integer modes CEILING / FLOOR /` |
|    - |  866 | ` *             TOWARD_ZERO / AWAY_FROM_ZERO (5..8).` |
|    - |  867 | ` * Return` |
|    - |  868 | ` *  The rounded value as a float.` |
|    - |  869 | ` */` |
|  168 |  870 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  871 | `{` |
|    - |  872 | `	double value, r;` |
|  169 |  873 | `	int places = 0;` |
|  169 |  874 | `	int mode = PH7_ROUND_HALF_UP;` |
|    - |  875 | `	/*` |
|    - |  876 | `	 * Legacy PHL contract: no argument -> int(0). PHP throws an` |
|    - |  877 | `	 * ArgumentCountError here, but two PHL-only (--SKIPIF-- zend_version)` |
|    - |  878 | `	 * tests assert round()===0, so keep the historical behavior.` |
|    - |  879 | `	 */` |
|  169 |  880 | `	if( nArg < 1 ){` |
|  ! 0 |  881 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  882 | `		return PH7_OK;` |
|    - |  883 | `	}` |
|  169 |  884 | `	if( nArg > 3 ){` |
|    4 |  885 | `		return PH7_VmThrowException(pCtx,` |
|    - |  886 | `			"ArgumentCountError",` |
|    - |  887 | `			"round() expects at most 3 arguments, %d given",` |
|    1 |  888 | `			nArg` |
|    - |  889 | `			);` |
|    - |  890 | `	}` |
|    - |  891 | `	/*` |
|    - |  892 | `	 * Validate argument #1: only int/float (and numeric strings) are` |
|    - |  893 | `	 * accepted; every other type raises a TypeError (mirrors floor()/ceil()).` |
|    - |  894 | `	 */` |
|  167 |  895 | `	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){` |
|    7 |  896 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |  897 | `			int len;` |
|    5 |  898 | `			sxu8 bReal = FALSE;` |
|    5 |  899 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    5 |  900 | `			if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|    4 |  901 | `				return PH7_VmThrowException(pCtx,` |
|    - |  902 | `					"TypeError",` |
|    - |  903 | `					"round(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |  904 | `					ph7_type_name(apArg[0])` |
|    - |  905 | `					);` |
|    - |  906 | `			}` |
|    2 |  907 | `		}else{` |
|    4 |  908 | `			return PH7_VmThrowException(pCtx,` |
|    - |  909 | `				"TypeError",` |
|    - |  910 | `				"round(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |  911 | `				ph7_type_name(apArg[0])` |
|    - |  912 | `				);` |
|    - |  913 | `		}` |
|    1 |  914 | `	}` |
|    - |  915 | `	/* Precision (arg #2). Negative values are valid; clamp to int range. */` |
|  163 |  916 | `	if( nArg > 1 ){` |
|  137 |  917 | `		sxi64 prec = ph7_value_to_int64(apArg[1]);` |
|  137 |  918 | `		if( prec > 2147483647 ){` |
|  ! 0 |  919 | `			places = 2147483647;` |
|  137 |  920 | `		}else if( prec < -2147483647 ){` |
|  ! 0 |  921 | `			places = -2147483647;` |
|  ! 0 |  922 | `		}else{` |
|  137 |  923 | `			places = (int)prec;` |
|    - |  924 | `		}` |
|   68 |  925 | `	}` |
|    - |  926 | `	/*` |
|    - |  927 | `	 * Mode (arg #3). PHP 8.5 accepts the integer modes 1..8. Read the full` |
|    - |  928 | `	 * 64-bit value before range-checking so a large out-of-range mode cannot` |
|    - |  929 | `	 * alias a valid 1..8 via a truncating 32-bit cast (e.g. 0x1_0000_0003).` |
|    - |  930 | `	 */` |
|  163 |  931 | `	if( nArg > 2 ){` |
|   73 |  932 | `		sxi64 m = ph7_value_to_int64(apArg[2]);` |
|   73 |  933 | `		if( m < PH7_ROUND_HALF_UP \|\| m > PH7_ROUND_AWAY_FROM_ZERO ){` |
|    5 |  934 | `			return PH7_VmThrowException(pCtx,` |
|    - |  935 | `				"ValueError",` |
|    - |  936 | `				"round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)"` |
|    - |  937 | `				);` |
|    - |  938 | `		}` |
|   69 |  939 | `		mode = (int)m;` |
|   34 |  940 | `	}` |
|  159 |  941 | `	value = ph7_value_to_double(apArg[0]);` |
|    - |  942 | `	/* Integer input with non-negative precision needs no rounding. */` |
|  159 |  943 | `	if( ph7_value_is_int(apArg[0]) && places >= 0 ){` |
|   11 |  944 | `		ph7_result_double(pCtx,value);` |
|   11 |  945 | `		return PH7_OK;` |
|    - |  946 | `	}` |
|  149 |  947 | `	r = MathRound(value, places, mode);` |
|  149 |  948 | `	ph7_result_double(pCtx,r);` |
|  149 |  949 | `	return PH7_OK;` |
|   85 |  950 | `}` |
|    - |  951 | `/*` |
|    - |  952 | ` * int intdiv(int $a, int $b)` |
|    - |  953 | ` *  Integer division.` |
|    - |  954 | ` * Parameters` |
|    - |  955 | ` *  $a` |
|    - |  956 | ` *   Number to be divided.` |
|    - |  957 | ` *  $b` |
|    - |  958 | ` *   Number which divides the $a.` |
|    - |  959 | ` * Return` |
|    - |  960 | ` *  The integer quotient of the division of $a by $b.` |
|    - |  961 | ` */` |
|   20 |  962 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  963 | `{` |
|    - |  964 | `	sxi64 a,b;` |
|    - |  965 | `	/* PHP requires exactly two arguments. */` |
|   24 |  966 | `	if( nArg != 2 ){` |
|    4 |  967 | `		return PH7_VmThrowException(pCtx,` |
|    - |  968 | `			"ArgumentCountError",` |
|    - |  969 | `			"intdiv() expects exactly 2 arguments, %d given",` |
|    1 |  970 | `			nArg` |
|    - |  971 | `			);` |
|    - |  972 | `	}` |
|    - |  973 | `	/* Type-check argument 1 */` |
|   18 |  974 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|   21 |  975 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 |  976 | `		return PH7_VmThrowException(pCtx,` |
|    - |  977 | `			"TypeError",` |
|    - |  978 | `			"intdiv(): Argument #1 ($num1) must be of type int, %s given",` |
|  ! 0 |  979 | `			ph7_type_name(apArg[0])` |
|    - |  980 | `			);` |
|    - |  981 | `	}` |
|   21 |  982 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  983 | `		int len;` |
|  ! 0 |  984 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|  ! 0 |  985 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 |  986 | `			return PH7_VmThrowException(pCtx,` |
|    - |  987 | `				"TypeError",` |
|    - |  988 | `				"intdiv(): Argument #1 ($num1) must be of type int, string given"` |
|    - |  989 | `				);` |
|    - |  990 | `		}` |
|  ! 0 |  991 | `	}` |
|    - |  992 | `	/* Type-check argument 2 */` |
|   18 |  993 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|   21 |  994 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|  ! 0 |  995 | `		return PH7_VmThrowException(pCtx,` |
|    - |  996 | `			"TypeError",` |
|    - |  997 | `			"intdiv(): Argument #2 ($num2) must be of type int, %s given",` |
|  ! 0 |  998 | `			ph7_type_name(apArg[1])` |
|    - |  999 | `			);` |
|    - | 1000 | `	}` |
|   21 | 1001 | `	if( ph7_value_is_string(apArg[1]) ){` |
|    - | 1002 | `		int len;` |
|  ! 0 | 1003 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|  ! 0 | 1004 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 | 1005 | `			return PH7_VmThrowException(pCtx,` |
|    - | 1006 | `				"TypeError",` |
|    - | 1007 | `				"intdiv(): Argument #2 ($num2) must be of type int, string given"` |
|    - | 1008 | `				);` |
|    - | 1009 | `		}` |
|  ! 0 | 1010 | `	}` |
|    - | 1011 | `	/* Convert both arguments to int64 */` |
|   21 | 1012 | `	a = ph7_value_to_int64(apArg[0]);` |
|   21 | 1013 | `	b = ph7_value_to_int64(apArg[1]);` |
|    - | 1014 | `	/* Check for division by zero */` |
|   21 | 1015 | `	if( b == 0 ){` |
|    3 | 1016 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1017 | `			"DivisionByZeroError",` |
|    - | 1018 | `			"Division by zero"` |
|    - | 1019 | `			);` |
|    - | 1020 | `	}` |
|    - | 1021 | `	/* Check for overflow: PHP_INT_MIN / -1 */` |
|   18 | 1022 | `	if( a == SMALLEST_INT64 && b == -1 ){` |
|    3 | 1023 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1024 | `			"ArithmeticError",` |
|    - | 1025 | `			"Division of PHP_INT_MIN by -1 is not an integer"` |
|    - | 1026 | `			);` |
|    - | 1027 | `	}` |
|    - | 1028 | `	/* Perform integer division */` |
|   15 | 1029 | `	ph7_result_int64(pCtx, a / b);` |
|   15 | 1030 | `	return PH7_OK;` |
|   14 | 1031 | `}` |
|    - | 1032 | `/*` |
|    - | 1033 | ` * string dechex(int $number)` |
|    - | 1034 | ` *  Decimal to hexadecimal.` |
|    - | 1035 | ` * Parameters` |
|    - | 1036 | ` *  $number` |
|    - | 1037 | ` *   Decimal value to convert` |
|    - | 1038 | ` * Return` |
|    - | 1039 | ` *  Hexadecimal string representation of number` |
|    - | 1040 | ` */` |
|    4 | 1041 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1042 | `{` |
|    - | 1043 | `	int iVal;` |
|    5 | 1044 | `	if( nArg < 1 ){` |
|    - | 1045 | `		/* Missing arguments,return null */` |
|  ! 0 | 1046 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1047 | `		return PH7_OK;` |
|    - | 1048 | `	}` |
|    - | 1049 | `	/* Extract the given number */` |
|    5 | 1050 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - | 1051 | `	/* Format */` |
|    5 | 1052 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|    5 | 1053 | `	return PH7_OK;` |
|    3 | 1054 | `}` |
|    - | 1055 | `/*` |
|    - | 1056 | ` * string decoct(int $number)` |
|    - | 1057 | ` *  Decimal to Octal.` |
|    - | 1058 | ` * Parameters` |
|    - | 1059 | ` *  $number` |
|    - | 1060 | ` *   Decimal value to convert` |
|    - | 1061 | ` * Return` |
|    - | 1062 | ` *  Octal string representation of number` |
|    - | 1063 | ` */` |
|    6 | 1064 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1065 | `{` |
|    - | 1066 | `	int iVal;` |
|    7 | 1067 | `	if( nArg < 1 ){` |
|    - | 1068 | `		/* Missing arguments,return null */` |
|  ! 0 | 1069 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1070 | `		return PH7_OK;` |
|    - | 1071 | `	}` |
|    - | 1072 | `	/* Extract the given number */` |
|    7 | 1073 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - | 1074 | `	/* Format */` |
|    7 | 1075 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|    7 | 1076 | `	return PH7_OK;` |
|    4 | 1077 | `}` |
|    - | 1078 | `/*` |
|    - | 1079 | ` * string decbin(int $number)` |
|    - | 1080 | ` *  Decimal to binary.` |
|    - | 1081 | ` * Parameters` |
|    - | 1082 | ` *  $number` |
|    - | 1083 | ` *   Decimal value to convert` |
|    - | 1084 | ` * Return` |
|    - | 1085 | ` *  Binary string representation of number` |
|    - | 1086 | ` */` |
|    4 | 1087 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1088 | `{` |
|    - | 1089 | `	int iVal;` |
|    5 | 1090 | `	if( nArg < 1 ){` |
|    - | 1091 | `		/* Missing arguments,return null */` |
|  ! 0 | 1092 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1093 | `		return PH7_OK;` |
|    - | 1094 | `	}` |
|    - | 1095 | `	/* Extract the given number */` |
|    5 | 1096 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - | 1097 | `	/* Format */` |
|    5 | 1098 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|    5 | 1099 | `	return PH7_OK;` |
|    3 | 1100 | `}` |
|    - | 1101 | `/*` |
|    - | 1102 | ` * int64 hexdec(string $hex_string)` |
|    - | 1103 | ` *  Hexadecimal to decimal.` |
|    - | 1104 | ` * Parameters` |
|    - | 1105 | ` *  $hex_string` |
|    - | 1106 | ` *   The hexadecimal string to convert` |
|    - | 1107 | ` * Return` |
|    - | 1108 | ` *  The decimal representation of hex_string` |
|    - | 1109 | ` */` |
|   20 | 1110 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1111 | `{` |
|    - | 1112 | `	const char *zString,*zEnd;` |
|    - | 1113 | `	ph7_int64 iVal;` |
|    - | 1114 | `	int nLen;` |
|   21 | 1115 | `	if( nArg < 1 ){` |
|    - | 1116 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1117 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1118 | `		return PH7_OK;` |
|    - | 1119 | `	}` |
|   21 | 1120 | `	iVal = 0;` |
|   21 | 1121 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - | 1122 | `		/* Extract the given string */` |
|   15 | 1123 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    - | 1124 | `		/* Delimit the string */` |
|   15 | 1125 | `		zEnd = &zString[nLen];` |
|    - | 1126 | `		/* Ignore non hex-stream */` |
|   21 | 1127 | `		while( zString < zEnd ){` |
|   21 | 1128 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|    - | 1129 | `				/* UTF-8 stream */` |
|    5 | 1130 | `				zString++;` |
|    9 | 1131 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|    5 | 1132 | `					zString++;` |
|    1 | 1133 | `				}` |
|    3 | 1134 | `			}else{` |
|   17 | 1135 | `				if( SyisHex(zString[0]) ){` |
|   15 | 1136 | `					break;` |
|    - | 1137 | `				}` |
|    - | 1138 | `				/* Ignore */` |
|    3 | 1139 | `				zString++;` |
|    - | 1140 | `			}` |
|    1 | 1141 | `		}` |
|   15 | 1142 | `		if( zString < zEnd ){` |
|    - | 1143 | `			/* Cast */` |
|   15 | 1144 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|    7 | 1145 | `		}` |
|    8 | 1146 | `	}else{` |
|    - | 1147 | `		/* Extract as a 64-bit integer */` |
|    7 | 1148 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1149 | `	}` |
|    - | 1150 | `	/* Return the number */` |
|   21 | 1151 | `	ph7_result_int64(pCtx,iVal);` |
|   21 | 1152 | `	return PH7_OK;` |
|   11 | 1153 | `}` |
|    - | 1154 | `/*` |
|    - | 1155 | ` * int64 bindec(string $bin_string)` |
|    - | 1156 | ` *  Binary to decimal.` |
|    - | 1157 | ` * Parameters` |
|    - | 1158 | ` *  $bin_string` |
|    - | 1159 | ` *   The binary string to convert` |
|    - | 1160 | ` * Return` |
|    - | 1161 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|    - | 1162 | ` */` |
|   10 | 1163 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1164 | `{` |
|    - | 1165 | `	const char *zString;` |
|    - | 1166 | `	ph7_int64 iVal;` |
|    - | 1167 | `	int nLen;` |
|   11 | 1168 | `	if( nArg < 1 ){` |
|    - | 1169 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1170 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1171 | `		return PH7_OK;` |
|    - | 1172 | `	}` |
|   11 | 1173 | `	iVal = 0;` |
|   11 | 1174 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - | 1175 | `		/* Extract the given string */` |
|    9 | 1176 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    9 | 1177 | `		if( nLen > 0 ){` |
|    - | 1178 | `			/* Perform a binary cast */` |
|    7 | 1179 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    3 | 1180 | `		}` |
|    5 | 1181 | `	}else{` |
|    - | 1182 | `		/* Extract as a 64-bit integer */` |
|    3 | 1183 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1184 | `	}` |
|    - | 1185 | `	/* Return the number */` |
|   11 | 1186 | `	ph7_result_int64(pCtx,iVal);` |
|   11 | 1187 | `	return PH7_OK;` |
|    6 | 1188 | `}` |
|    - | 1189 | `/*` |
|    - | 1190 | ` * int64 octdec(string $oct_string)` |
|    - | 1191 | ` *  Octal to decimal.` |
|    - | 1192 | ` * Parameters` |
|    - | 1193 | ` *  $oct_string` |
|    - | 1194 | ` *   The octal string to convert` |
|    - | 1195 | ` * Return` |
|    - | 1196 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|    - | 1197 | ` */` |
|    4 | 1198 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1199 | `{` |
|    - | 1200 | `	const char *zString;` |
|    - | 1201 | `	ph7_int64 iVal;` |
|    - | 1202 | `	int nLen;` |
|    5 | 1203 | `	if( nArg < 1 ){` |
|    - | 1204 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1205 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1206 | `		return PH7_OK;` |
|    - | 1207 | `	}` |
|    5 | 1208 | `	iVal = 0;` |
|    5 | 1209 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - | 1210 | `		/* Extract the given string */` |
|    3 | 1211 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    3 | 1212 | `		if( nLen > 0 ){` |
|    - | 1213 | `			/* Perform the cast */` |
|    3 | 1214 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    1 | 1215 | `		}` |
|    2 | 1216 | `	}else{` |
|    - | 1217 | `		/* Extract as a 64-bit integer */` |
|    3 | 1218 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1219 | `	}` |
|    - | 1220 | `	/* Return the number */` |
|    5 | 1221 | `	ph7_result_int64(pCtx,iVal);` |
|    5 | 1222 | `	return PH7_OK;` |
|    3 | 1223 | `}` |
|    - | 1224 | `/*` |
|    - | 1225 | ` * srand([int $seed])` |
|    - | 1226 | ` * mt_srand([int $seed])` |
|    - | 1227 | ` *  Seed the random number generator.` |
|    - | 1228 | ` * Parameters` |
|    - | 1229 | ` * $seed` |
|    - | 1230 | ` *  Optional seed value` |
|    - | 1231 | ` * Return` |
|    - | 1232 | ` *  null.` |
|    - | 1233 | ` * Note:` |
|    - | 1234 | ` *  THIS FUNCTION IS A NO-OP.` |
|    - | 1235 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|    - | 1236 | ` */` |
|   20 | 1237 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1238 | `{` |
|   10 | 1239 | `	SXUNUSED(nArg);` |
|   10 | 1240 | `	SXUNUSED(apArg);` |
|   21 | 1241 | `	ph7_result_null(pCtx);` |
|   21 | 1242 | `	return PH7_OK;` |
|    1 | 1243 | `}` |
|    - | 1244 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - | 1245 | `/*` |
|    - | 1246 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|    - | 1247 | ` *  Convert a number between arbitrary bases.` |
|    - | 1248 | ` * Parameters` |
|    - | 1249 | ` * $number` |
|    - | 1250 | ` *  The number to convert` |
|    - | 1251 | ` * $frombase` |
|    - | 1252 | ` *  The base number is in` |
|    - | 1253 | ` * $tobase` |
|    - | 1254 | ` *  The base to convert number to` |
|    - | 1255 | ` * Return` |
|    - | 1256 | ` *  Number converted to base tobase` |
|    - | 1257 | ` */` |
|   58 | 1258 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1259 | `{` |
|    - | 1260 | `	static const char zDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";` |
|    - | 1261 | `	int nLen,iFbase,iTobase,i;` |
|    - | 1262 | `	ph7_int64 iFbase64,iTobase64;` |
|    - | 1263 | `	const char *zNum;` |
|   59 | 1264 | `	sxu64 uNum = 0;` |
|   59 | 1265 | `	if( nArg < 3 ){` |
|    - | 1266 | `		/* Return the empty string*/` |
|  ! 0 | 1267 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1268 | `		return PH7_OK;` |
|    - | 1269 | `	}` |
|    - | 1270 | `	/* Base numbers. Read them as 64-bit so an out-of-range base can't wrap through` |
|    - | 1271 | `	 * a 32-bit truncation back into the 2..36 window and bypass the check below. */` |
|   59 | 1272 | `	iFbase64 = ph7_value_to_int64(apArg[1]);` |
|   59 | 1273 | `	iTobase64 = ph7_value_to_int64(apArg[2]);` |
|    - | 1274 | `	/* PHP 8 throws a catchable ValueError for a base outside 2..36; from_base` |
|    - | 1275 | `	 * is validated before to_base, both before the string is even parsed. */` |
|   59 | 1276 | `	if( iFbase64 < 2 \|\| iFbase64 > 36 ){` |
|    7 | 1277 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1278 | `			"base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)");` |
|    - | 1279 | `	}` |
|   53 | 1280 | `	if( iTobase64 < 2 \|\| iTobase64 > 36 ){` |
|    5 | 1281 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1282 | `			"base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)");` |
|    - | 1283 | `	}` |
|    - | 1284 | `	/* Both bases are now known to fit in [2,36], so the int form is exact. */` |
|   49 | 1285 | `	iFbase  = (int)iFbase64;` |
|   49 | 1286 | `	iTobase = (int)iTobase64;` |
|    - | 1287 | `	/* Parse the input number in from_base. Every base is handled the same way:` |
|    - | 1288 | `	 * digits 0-9 then a-z/A-Z map to 0-35; a character that is not a valid digit` |
|    - | 1289 | `	 * for from_base is ignored (PHP additionally raises an E_DEPRECATED for the` |
|    - | 1290 | `	 * ignored characters — not yet emitted, see PLAN §3.1). */` |
|   49 | 1291 | `	zNum = ph7_value_to_string(apArg[0],&nLen);` |
|  147 | 1292 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   99 | 1293 | `		int c = (unsigned char)zNum[i];` |
|    - | 1294 | `		int d;` |
|   99 | 1295 | `		if( c >= '0' && c <= '9' ){` |
|   73 | 1296 | `			d = c - '0';` |
|   63 | 1297 | `		}else if( c >= 'a' && c <= 'z' ){` |
|   25 | 1298 | `			d = c - 'a' + 10;` |
|   15 | 1299 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|    3 | 1300 | `			d = c - 'A' + 10;` |
|    2 | 1301 | `		}else{` |
|  ! 0 | 1302 | `			d = 99;` |
|    - | 1303 | `		}` |
|   99 | 1304 | `		if( d >= iFbase ){` |
|    - | 1305 | `			/* Not a valid digit for this base: skip it (PHP). */` |
|    3 | 1306 | `			continue;` |
|    - | 1307 | `		}` |
|   97 | 1308 | `		uNum = uNum * (sxu64)iFbase + (sxu64)d;` |
|   49 | 1309 | `	}` |
|    - | 1310 | `	/* Format the result in to_base using lowercase digits. */` |
|   49 | 1311 | `	if( uNum == 0 ){` |
|    9 | 1312 | `		ph7_result_string(pCtx,"0",1);` |
|    5 | 1313 | `	}else{` |
|    - | 1314 | `		char zOut[70]; /* base-2 of a 64-bit value fits in 64 digits */` |
|   41 | 1315 | `		int n = 0,j;` |
|  133 | 1316 | `		while( uNum > 0 ){` |
|   93 | 1317 | `			zOut[n++] = zDigits[uNum % (sxu64)iTobase];` |
|   93 | 1318 | `			uNum /= (sxu64)iTobase;` |
|    1 | 1319 | `		}` |
|    - | 1320 | `		/* Digits were produced least-significant first: reverse in place. */` |
|   79 | 1321 | `		for( j = 0 ; j < n/2 ; ++j ){` |
|   39 | 1322 | `			char t = zOut[j];` |
|   39 | 1323 | `			zOut[j] = zOut[n - 1 - j];` |
|   39 | 1324 | `			zOut[n - 1 - j] = t;` |
|   20 | 1325 | `		}` |
|   41 | 1326 | `		ph7_result_string(pCtx,zOut,n);` |
|    - | 1327 | `	}` |
|   49 | 1328 | `	return PH7_OK;` |
|   30 | 1329 | `}` |
|    - | 1330 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - | 1331 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1332 |  |
