# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 530/622 lines (85.21%)

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
|   16 |   80 | `PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |   81 | `{` |
|    - |   82 | `	double r,x;` |
|    - |   83 | `	/* PHP requires exactly one argument. */` |
|   19 |   84 | `	if( nArg != 1 ){` |
|    4 |   85 | `		return PH7_VmThrowException(pCtx,` |
|    - |   86 | `			"ArgumentCountError",` |
|    - |   87 | `			"floor() expects exactly 1 argument, %d given",` |
|    1 |   88 | `			nArg` |
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
|   11 |  126 | `}` |
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
|   18 |  158 | `PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  159 | `{` |
|    - |  160 | `	double r, x;` |
|    - |  161 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|   20 |  162 | `	if( nArg != 1 ){` |
|  ! 0 |  163 | `		return PH7_VmThrowException(pCtx,` |
|    - |  164 | `			"ArgumentCountError",` |
|    - |  165 | `			"acos() expects exactly 1 argument, %d given",` |
|  ! 0 |  166 | `			nArg` |
|    - |  167 | `			);` |
|    - |  168 | `	}` |
|    - |  169 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|    - |  170 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|    - |  171 | `	 * the float conversion will handle them. */` |
|   20 |  172 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    4 |  173 | `		return PH7_VmThrowException(pCtx,` |
|    - |  174 | `			"TypeError",` |
|    - |  175 | `			"acos(): Argument #1 ($num) must be of type float, %s given",` |
|    1 |  176 | `			ph7_type_name(apArg[0])` |
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
|   11 |  190 | `}` |
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
|   18 |  245 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  246 | `{` |
|    - |  247 | `	double r, x;` |
|    - |  248 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|   20 |  249 | `	if( nArg != 1 ){` |
|  ! 0 |  250 | `		return PH7_VmThrowException(pCtx,` |
|    - |  251 | `			"ArgumentCountError",` |
|    - |  252 | `			"asin() expects exactly 1 argument, %d given",` |
|  ! 0 |  253 | `			nArg` |
|    - |  254 | `			);` |
|    - |  255 | `	}` |
|    - |  256 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|    - |  257 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|    - |  258 | `	 * the float conversion will handle them. */` |
|   20 |  259 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    4 |  260 | `		return PH7_VmThrowException(pCtx,` |
|    - |  261 | `			"TypeError",` |
|    - |  262 | `			"asin(): Argument #1 ($num) must be of type float, %s given",` |
|    1 |  263 | `			ph7_type_name(apArg[0])` |
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
|   11 |  277 | `}` |
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
|   12 |  309 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  310 | `{` |
|    - |  311 | `	double r,x;` |
|    - |  312 | `	/* PHP requires exactly one argument. */` |
|   15 |  313 | `	if( nArg != 1 ){` |
|    4 |  314 | `		return PH7_VmThrowException(pCtx,` |
|    - |  315 | `			"ArgumentCountError",` |
|    - |  316 | `			"ceil() expects exactly 1 argument, %d given",` |
|    1 |  317 | `			nArg` |
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
|    9 |  354 | `}` |
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
|   38 |  386 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  387 | `{` |
|    - |  388 | `	double r,x;` |
|    - |  389 | `	/* PHP enforces exactly one argument. */` |
|   41 |  390 | `	if( nArg != 1 ){` |
|    4 |  391 | `		return PH7_VmThrowException(pCtx,` |
|    - |  392 | `			"ArgumentCountError",` |
|    - |  393 | `			"atan() expects exactly 1 argument, %d given",` |
|    1 |  394 | `			nArg` |
|    - |  395 | `			);` |
|    - |  396 | `	}` |
|    - |  397 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, non-numeric strings).` |
|    - |  398 | `	 * PHP 8 reports a TypeError for wrong types. */` |
|   38 |  399 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    8 |  400 | `		return PH7_VmThrowException(pCtx,` |
|    - |  401 | `			"TypeError",` |
|    - |  402 | `			"atan(): Argument #1 ($num) must be of type float, %s given",` |
|    2 |  403 | `			ph7_type_name(apArg[0])` |
|    - |  404 | `			);` |
|    - |  405 | `	}` |
|   33 |  406 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  407 | `	/* Perform the requested operation */` |
|   33 |  408 | `	r = atan(x);` |
|    - |  409 | `	/* store the result back */` |
|   33 |  410 | `	ph7_result_double(pCtx,r);` |
|   33 |  411 | `	return PH7_OK;` |
|   22 |  412 | `}` |
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
|   52 |  445 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  446 | `{` |
|    - |  447 | `	double r,x,y;` |
|    - |  448 | `	/* PHP enforces exactly two arguments. */` |
|   56 |  449 | `	if( nArg != 2 ){` |
|    4 |  450 | `		return PH7_VmThrowException(pCtx,` |
|    - |  451 | `			"ArgumentCountError",` |
|    - |  452 | `			"atan2() expects exactly 2 arguments, %d given",` |
|    1 |  453 | `			nArg` |
|    - |  454 | `			);` |
|    - |  455 | `	}` |
|    - |  456 | `	/* Type checking: reject non-numeric values for $y (argument #1). */` |
|   53 |  457 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    4 |  458 | `		return PH7_VmThrowException(pCtx,` |
|    - |  459 | `			"TypeError",` |
|    - |  460 | `			"atan2(): Argument #1 ($y) must be of type float, %s given",` |
|    1 |  461 | `			ph7_type_name(apArg[0])` |
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
|   30 |  479 | `}` |
|    - |  480 | `/*` |
|    - |  481 | ` * float/int64 abs(float/int64 $arg )` |
|    - |  482 | ` *  Absolute value.` |
|    - |  483 | ` * Parameter` |
|    - |  484 | ` *  The number to process.` |
|    - |  485 | ` * Return` |
|    - |  486 | ` *  The absolute value of number.` |
|    - |  487 | ` */` |
|  130 |  488 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 |  489 | `{` |
|    - |  490 | `	int is_float;` |
|    - |  491 | `	/* PHP requires exactly one argument. */` |
|  134 |  492 | `	if( nArg != 1 ){` |
|    4 |  493 | `		return PH7_VmThrowException(pCtx,` |
|    - |  494 | `			"ArgumentCountError",` |
|    - |  495 | `			"abs() expects exactly 1 argument, %d given",` |
|    1 |  496 | `			nArg` |
|    - |  497 | `			);` |
|    - |  498 | `	}` |
|    - |  499 |  |
|  131 |  500 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    - |  501 | `		/* php's 8.1 null-to-scalar-parameter deprecation; abs(null) is still 0 */` |
|    3 |  502 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|    - |  503 | `			"abs(): Passing null to parameter #1 ($num) of type int\|float is deprecated");` |
|    1 |  504 | `	}` |
|    - |  505 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|  131 |  506 | `	is_float = ph7_value_is_float(apArg[0]);` |
|  131 |  507 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|    - |  508 | `		int len;` |
|   10 |  509 | `		sxu8 bReal = FALSE;` |
|   10 |  510 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  511 | `		sxi32 rcNum;` |
|   10 |  512 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|   10 |  513 | `		if( rcNum != SXRET_OK ){` |
|    3 |  514 | `			return PH7_VmThrowException(pCtx,` |
|    - |  515 | `				"TypeError",` |
|    - |  516 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  517 | `				);` |
|    - |  518 | `		}` |
|    7 |  519 | `		if( bReal ){` |
|    5 |  520 | `			is_float = 1;` |
|    2 |  521 | `		}` |
|    3 |  522 | `	}` |
|  128 |  523 | `	if( is_float ){` |
|    - |  524 | `		double r,x;` |
|   99 |  525 | `		x = ph7_value_to_double(apArg[0]);` |
|    - |  526 | `		/* Perform the requested operation */` |
|   99 |  527 | `		r = fabs(x);` |
|   99 |  528 | `		ph7_result_double(pCtx,r);` |
|   50 |  529 | `	}else{` |
|    - |  530 | ``		/* Read the full 64-bit value (the old 32-bit `int abs()` truncated any`` |
|    - |  531 | `		 * magnitude above 2^31 and was UB on INT_MIN). */` |
|   30 |  532 | `		sxi64 x = ph7_value_to_int64(apArg[0]);` |
|   30 |  533 | `		if( x == SMALLEST_INT64 ){` |
|    - |  534 | `			/* abs(PHP_INT_MIN) has no int representation, so PHP returns a float. */` |
|    3 |  535 | `			ph7_result_double(pCtx,-(double)x);` |
|    2 |  536 | `		}else{` |
|   28 |  537 | `			ph7_result_int64(pCtx,x < 0 ? -x : x);` |
|    - |  538 | `		}` |
|    - |  539 | `	}` |
|  128 |  540 | `	return PH7_OK;` |
|   69 |  541 | `}` |
|    - |  542 | `/*` |
|    - |  543 | ` * float log(float $arg,[int/float $base])` |
|    - |  544 | ` *  Natural logarithm.` |
|    - |  545 | ` * Parameter` |
|    - |  546 | ` *  $arg: The number to process.` |
|    - |  547 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|    - |  548 | ` * Return` |
|    - |  549 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|    - |  550 | ` * Note:` |
|    - |  551 | ` *  only Natural log and base-10 log are supported.` |
|    - |  552 | ` */` |
|   12 |  553 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  554 | `{` |
|    - |  555 | `	double r,x;` |
|   13 |  556 | `	if( nArg < 1 ){` |
|    - |  557 | `		/* Missing argument,return 0 */` |
|  ! 0 |  558 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  559 | `		return PH7_OK;` |
|    - |  560 | `	}` |
|   13 |  561 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  562 | `	/* Perform the requested operation */` |
|   13 |  563 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|    - |  564 | `		/* Base-10 log */` |
|    5 |  565 | `		r = log10(x);` |
|    3 |  566 | `	}else{` |
|    9 |  567 | `		r = log(x);` |
|    - |  568 | `	}` |
|    - |  569 | `	/* store the result back */` |
|   13 |  570 | `	ph7_result_double(pCtx,r);` |
|   13 |  571 | `	return PH7_OK;` |
|    7 |  572 | `}` |
|    - |  573 | `/*` |
|    - |  574 | ` * float log10(float $arg )` |
|    - |  575 | ` *  Base-10 logarithm.` |
|    - |  576 | ` * Parameter` |
|    - |  577 | ` *  The number to process.` |
|    - |  578 | ` * Return` |
|    - |  579 | ` *  The Base-10 logarithm of the given number.` |
|    - |  580 | ` */` |
|   14 |  581 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  582 | `{` |
|    - |  583 | `	double r,x;` |
|   15 |  584 | `	if( nArg < 1 ){` |
|    - |  585 | `		/* Missing argument,return 0 */` |
|  ! 0 |  586 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  587 | `		return PH7_OK;` |
|    - |  588 | `	}` |
|   15 |  589 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  590 | `	/* Perform the requested operation */` |
|   15 |  591 | `	r = log10(x);` |
|    - |  592 | `	/* store the result back */` |
|   15 |  593 | `	ph7_result_double(pCtx,r);` |
|   15 |  594 | `	return PH7_OK;` |
|    8 |  595 | `}` |
|    - |  596 | `/*` |
|    - |  597 | ` * number pow(number $base,number $exp)` |
|    - |  598 | ` *  Exponential expression.` |
|    - |  599 | ` * Parameter` |
|    - |  600 | ` *  base` |
|    - |  601 | ` *  The base to use.` |
|    - |  602 | ` * exp` |
|    - |  603 | ` *  The exponent.` |
|    - |  604 | ` * Return` |
|    - |  605 | ` *  base raised to the power of exp.` |
|    - |  606 | ` *  If the result can be represented as integer it will be returned` |
|    - |  607 | ` *  as type integer, else it will be returned as type float.` |
|    - |  608 | ` */` |
|    6 |  609 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  610 | `{` |
|    - |  611 | `	double r,x,y;` |
|    7 |  612 | `	if( nArg < 1 ){` |
|    - |  613 | `		/* Missing argument,return 0 */` |
|  ! 0 |  614 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  615 | `		return PH7_OK;` |
|    - |  616 | `	}` |
|    7 |  617 | `	x = ph7_value_to_double(apArg[0]);` |
|    7 |  618 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  619 | `	/* Perform the requested operation */` |
|    7 |  620 | `	r = pow(x,y);` |
|    7 |  621 | `	ph7_result_double(pCtx,r);` |
|    7 |  622 | `	return PH7_OK;` |
|    4 |  623 | `}` |
|    - |  624 | `/*` |
|    - |  625 | ` * float pi(void)` |
|    - |  626 | ` *  Returns an approximation of pi.` |
|    - |  627 | ` * Note` |
|    - |  628 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|    - |  629 | ` * Return` |
|    - |  630 | ` *  The value of pi as float.` |
|    - |  631 | ` */` |
|    2 |  632 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  633 | `{` |
|    1 |  634 | `	SXUNUSED(nArg); /* cc warning */` |
|    1 |  635 | `	SXUNUSED(apArg);` |
|    3 |  636 | `	ph7_result_double(pCtx,PH7_PI);` |
|    3 |  637 | `	return PH7_OK;` |
|    1 |  638 | `}` |
|    - |  639 | `/*` |
|    - |  640 | ` * float fmod(float $x,float $y)` |
|    - |  641 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|    - |  642 | ` * Parameters` |
|    - |  643 | ` * $x` |
|    - |  644 | ` *  The dividend` |
|    - |  645 | ` * $y` |
|    - |  646 | ` *  The divisor` |
|    - |  647 | ` * Return` |
|    - |  648 | ` *  The floating point remainder of x/y.` |
|    - |  649 | ` */` |
|    2 |  650 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  651 | `{` |
|    - |  652 | `	double x,y,r;` |
|    3 |  653 | `	if( nArg < 2 ){` |
|    - |  654 | `		/* Missing arguments */` |
|  ! 0 |  655 | `		ph7_result_double(pCtx,0);` |
|  ! 0 |  656 | `		return PH7_OK;` |
|    - |  657 | `	}` |
|    - |  658 | `	/* Extract given arguments */` |
|    3 |  659 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  660 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  661 | `	/* Perform the requested operation */` |
|    3 |  662 | `	r = fmod(x,y);` |
|    - |  663 | `	/* Processing result */` |
|    3 |  664 | `	ph7_result_double(pCtx,r);` |
|    3 |  665 | `	return PH7_OK;` |
|    2 |  666 | `}` |
|    - |  667 | `/*` |
|    - |  668 | ` * float hypot(float $x,float $y)` |
|    - |  669 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|    - |  670 | ` * Parameters` |
|    - |  671 | ` * $x` |
|    - |  672 | ` *  Length of first side` |
|    - |  673 | ` * $y` |
|    - |  674 | ` *  Length of first side` |
|    - |  675 | ` * Return` |
|    - |  676 | ` *  Calculated length of the hypotenuse.` |
|    - |  677 | ` */` |
|    2 |  678 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  679 | `{` |
|    - |  680 | `	double x,y,r;` |
|    3 |  681 | `	if( nArg < 2 ){` |
|    - |  682 | `		/* Missing arguments */` |
|  ! 0 |  683 | `		ph7_result_double(pCtx,0);` |
|  ! 0 |  684 | `		return PH7_OK;` |
|    - |  685 | `	}` |
|    - |  686 | `	/* Extract given arguments */` |
|    3 |  687 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  688 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  689 | `	/* Perform the requested operation */` |
|    3 |  690 | `	r = hypot(x,y);` |
|    - |  691 | `	/* Processing result */` |
|    3 |  692 | `	ph7_result_double(pCtx,r);` |
|    3 |  693 | `	return PH7_OK;` |
|    2 |  694 | `}` |
|    - |  695 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|    - |  696 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  697 | `/*` |
|    - |  698 | ` * PHP rounding modes (mirror ext/standard/php_math_round_mode.h).` |
|    - |  699 | ` * Only the four HALF_* integer constants are exposed to userland` |
|    - |  700 | ` * (PHP_ROUND_HALF_UP..HALF_ODD, see constant.c); the CEILING/FLOOR/` |
|    - |  701 | ` * TOWARD_ZERO/AWAY_FROM_ZERO modes (5..8) have no userland constant but` |
|    - |  702 | ` * are reachable by passing the raw integer to round()'s 3rd argument,` |
|    - |  703 | ` * which PHP 8.5 still accepts, so all eight are honored here.` |
|    - |  704 | ` */` |
|    - |  705 | `#define PH7_ROUND_HALF_UP        1` |
|    - |  706 | `#define PH7_ROUND_HALF_DOWN      2` |
|    - |  707 | `#define PH7_ROUND_HALF_EVEN      3` |
|    - |  708 | `#define PH7_ROUND_HALF_ODD       4` |
|    - |  709 | `#define PH7_ROUND_CEILING        5` |
|    - |  710 | `#define PH7_ROUND_FLOOR          6` |
|    - |  711 | `#define PH7_ROUND_TOWARD_ZERO    7` |
|    - |  712 | `#define PH7_ROUND_AWAY_FROM_ZERO 8` |
|    - |  713 | `/*` |
|    - |  714 | ` * 10**power via an exact lookup table for 0..22, falling back to pow()` |
|    - |  715 | ` * otherwise. Port of php-src PHP-8.5 ext/standard/math.c php_intpow10().` |
|    - |  716 | ` */` |
|  144 |  717 | `static double MathIntPow10(int power)` |
|    1 |  718 | `{` |
|    - |  719 | `	static const double powers[] = {` |
|    - |  720 | `		1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,` |
|    - |  721 | `		1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22` |
|    - |  722 | `	};` |
|  145 |  723 | `	if( power < 0 \|\| power > 22 ){` |
|    3 |  724 | `		return pow(10.0, (double)power);` |
|    - |  725 | `	}` |
|  143 |  726 | `	return powers[power];` |
|   73 |  727 | `}` |
|  126 |  728 | `static double MathRoundBasicEdge(double integral, double exponent, int places)` |
|    1 |  729 | `{` |
|   64 |  730 | `	return (places > 0)` |
|   36 |  731 | `		? fabs((integral + copysign(0.5, integral)) / exponent)` |
|  108 |  732 | `		: fabs((integral + copysign(0.5, integral)) * exponent);` |
|    1 |  733 | `}` |
|   12 |  734 | `static double MathRoundZeroEdge(double integral, double exponent, int places)` |
|    1 |  735 | `{` |
|    7 |  736 | `	return (places > 0)` |
|  ! 0 |  737 | `		? fabs((integral) / exponent)` |
|   12 |  738 | `		: fabs((integral) * exponent);` |
|    1 |  739 | `}` |
|    - |  740 | `/*` |
|    - |  741 | ` * Round the extracted integral part according to the requested mode.` |
|    - |  742 | ` * Faithful port of php-src PHP-8.5 ext/standard/math.c php_round_helper().` |
|    - |  743 | ` */` |
|  142 |  744 | `static double MathRoundHelper(double integral, double value, double exponent, int places, int mode)` |
|    1 |  745 | `{` |
|  143 |  746 | `	double value_abs = fabs(value);` |
|    - |  747 | `	double edge_case;` |
|  143 |  748 | `	switch( mode ){` |
|   43 |  749 | `		case PH7_ROUND_HALF_UP:` |
|   87 |  750 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   87 |  751 | `			if( value_abs >= edge_case ){` |
|   71 |  752 | `				return integral + copysign(1.0, integral);` |
|    - |  753 | `			}` |
|   17 |  754 | `			return integral;` |
|    6 |  755 | `		case PH7_ROUND_HALF_DOWN:` |
|   13 |  756 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   13 |  757 | `			if( value_abs > edge_case ){` |
|  ! 0 |  758 | `				return integral + copysign(1.0, integral);` |
|    - |  759 | `			}` |
|   13 |  760 | `			return integral;` |
|    2 |  761 | `		case PH7_ROUND_CEILING:` |
|    5 |  762 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  763 | `			if( value > 0.0 && value_abs > edge_case ){` |
|    3 |  764 | `				return integral + 1.0;` |
|    - |  765 | `			}` |
|    3 |  766 | `			return integral;` |
|    2 |  767 | `		case PH7_ROUND_FLOOR:` |
|    5 |  768 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  769 | `			if( value < 0.0 && value_abs > edge_case ){` |
|    3 |  770 | `				return integral - 1.0;` |
|    - |  771 | `			}` |
|    3 |  772 | `			return integral;` |
|    2 |  773 | `		case PH7_ROUND_TOWARD_ZERO:` |
|    5 |  774 | `			return integral;` |
|    2 |  775 | `		case PH7_ROUND_AWAY_FROM_ZERO:` |
|    5 |  776 | `			edge_case = MathRoundZeroEdge(integral, exponent, places);` |
|    5 |  777 | `			if( value_abs > edge_case ){` |
|    5 |  778 | `				return integral + copysign(1.0, integral);` |
|    - |  779 | `			}` |
|  ! 0 |  780 | `			return integral;` |
|    8 |  781 | `		case PH7_ROUND_HALF_EVEN:` |
|   17 |  782 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   17 |  783 | `			if( value_abs > edge_case ){` |
|  ! 0 |  784 | `				return integral + copysign(1.0, integral);` |
|   17 |  785 | `			}else if( value_abs == edge_case ){` |
|   17 |  786 | `				if( fmod(integral, 2.0) != 0.0 ){ /* integral not even -> make it even */` |
|    9 |  787 | `					return integral + copysign(1.0, integral);` |
|    - |  788 | `				}` |
|    4 |  789 | `			}` |
|    9 |  790 | `			return integral;` |
|    6 |  791 | `		case PH7_ROUND_HALF_ODD:` |
|   13 |  792 | `			edge_case = MathRoundBasicEdge(integral, exponent, places);` |
|   13 |  793 | `			if( value_abs > edge_case ){` |
|  ! 0 |  794 | `				return integral + copysign(1.0, integral);` |
|   13 |  795 | `			}else if( value_abs == edge_case ){` |
|   13 |  796 | `				if( fmod(integral, 2.0) == 0.0 ){ /* integral even -> make it odd */` |
|    7 |  797 | `					return integral + copysign(1.0, integral);` |
|    - |  798 | `				}` |
|    3 |  799 | `			}` |
|    7 |  800 | `			return integral;` |
|  ! 0 |  801 | `		default:` |
|  ! 0 |  802 | `			return integral; /* unreachable: mode validated by the caller */` |
|    - |  803 | `	}` |
|   72 |  804 | `}` |
|    - |  805 | `/*` |
|    - |  806 | `` * Round `value` to `places` decimals in `mode`. Faithful port of php-src`` |
|    - |  807 | ` * PHP-8.5 ext/standard/math.c _php_math_round() — the post-8.4` |
|    - |  808 | ` * integer-extraction algorithm with the +/-1 floating-point error` |
|    - |  809 | ` * correction step, required for byte-exact results on cases such as` |
|    - |  810 | ` * round(0.285, 2) == 0.29 that the old naive "+0.5" approach got wrong.` |
|    - |  811 | ` */` |
|  150 |  812 | `static double MathRound(double value, int places, int mode)` |
|    1 |  813 | `{` |
|    - |  814 | `	double exponent, tmp_value, tmp_value2;` |
|    - |  815 | `	int abs_places;` |
|  151 |  816 | `	if( !isfinite(value) \|\| value == 0.0 ){` |
|    7 |  817 | `		return value;` |
|    - |  818 | `	}` |
|    - |  819 | `	/* mirror php-src's clamp away from INT_MIN */` |
|  145 |  820 | `	if( places < -2147483647 ){` |
|  ! 0 |  821 | `		places = -2147483647;` |
|  ! 0 |  822 | `	}` |
|  145 |  823 | `	abs_places = places < 0 ? -places : places;` |
|  145 |  824 | `	exponent = MathIntPow10(abs_places);` |
|    - |  825 | `	/*` |
|    - |  826 | `	 * Extracting the integer part can be off by one ULP due to float error` |
|    - |  827 | `	 * (e.g. floor(0.285 * 1e10) == 2849999999). Try +/-1 and keep it if it` |
|    - |  828 | ``	 * divides back to exactly `value`.`` |
|    - |  829 | `	 */` |
|  145 |  830 | `	if( value >= 0.0 ){` |
|  115 |  831 | `		tmp_value = floor(places > 0 ? value * exponent : value / exponent);` |
|  115 |  832 | `		tmp_value2 = tmp_value + 1.0;` |
|   58 |  833 | `	}else{` |
|   31 |  834 | `		tmp_value = ceil(places > 0 ? value * exponent : value / exponent);` |
|   31 |  835 | `		tmp_value2 = tmp_value - 1.0;` |
|    - |  836 | `	}` |
|  145 |  837 | `	if( (places > 0 ? tmp_value2 / exponent : tmp_value2 * exponent) == value ){` |
|    3 |  838 | `		tmp_value = tmp_value2;` |
|    1 |  839 | `	}` |
|    - |  840 | `	/* Beyond our precision, so rounding it is pointless. */` |
|  145 |  841 | `	if( fabs(tmp_value) >= 1e16 ){` |
|    3 |  842 | `		return value;` |
|    - |  843 | `	}` |
|  143 |  844 | `	tmp_value = MathRoundHelper(tmp_value, value, exponent, places, mode);` |
|  143 |  845 | `	if( abs_places < 23 ){` |
|  143 |  846 | `		tmp_value = (places > 0) ? tmp_value / exponent : tmp_value * exponent;` |
|   72 |  847 | `	}else{` |
|    - |  848 | `		/*` |
|    - |  849 | `		 * Simple division would lose precision here; round-trip through a` |
|    - |  850 | `		 * string exactly like php-src does (snprintf "%15fe%d" + strtod).` |
|    - |  851 | `		 * libc snprintf is used (not SyBufferFormat, which is not` |
|    - |  852 | `		 * correctly-rounded) so the low bits match PHP. (SyStrToReal now` |
|    - |  853 | `		 * delegates to strtod too; the direct call here simply mirrors` |
|    - |  854 | `		 * php-src's own snprintf+strtod pairing.)` |
|    - |  855 | `		 */` |
|    - |  856 | `		char zBuf[64];` |
|  ! 0 |  857 | `		snprintf(zBuf, sizeof(zBuf), "%15fe%d", tmp_value, -places);` |
|  ! 0 |  858 | `		zBuf[sizeof(zBuf)-1] = '\0';` |
|  ! 0 |  859 | `		tmp_value = strtod(zBuf, 0);` |
|  ! 0 |  860 | `		if( !isfinite(tmp_value) \|\| isnan(tmp_value) ){` |
|  ! 0 |  861 | `			tmp_value = value;` |
|  ! 0 |  862 | `		}` |
|    - |  863 | `	}` |
|  143 |  864 | `	return tmp_value;` |
|   76 |  865 | `}` |
|    - |  866 | `/*` |
|    - |  867 | ` * float round ( int\|float $num [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|    - |  868 | ` *  Rounds a float.` |
|    - |  869 | ` * Parameters` |
|    - |  870 | ` *  $num       The value to round.` |
|    - |  871 | ` *  $precision The optional number of decimal digits to round to. May be` |
|    - |  872 | ` *             negative (rounds to the left of the decimal point).` |
|    - |  873 | ` *  $mode      One of PHP_ROUND_HALF_UP (default) / _HALF_DOWN / _HALF_EVEN /` |
|    - |  874 | ` *             _HALF_ODD, or the 8.5 integer modes CEILING / FLOOR /` |
|    - |  875 | ` *             TOWARD_ZERO / AWAY_FROM_ZERO (5..8).` |
|    - |  876 | ` * Return` |
|    - |  877 | ` *  The rounded value as a float.` |
|    - |  878 | ` */` |
|  176 |  879 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  880 | `{` |
|    - |  881 | `	double value, r;` |
|  177 |  882 | `	int places = 0;` |
|  177 |  883 | `	int mode = PH7_ROUND_HALF_UP;` |
|    - |  884 | `	/*` |
|    - |  885 | `	 * Legacy PHL contract: no argument -> int(0). PHP throws an` |
|    - |  886 | `	 * ArgumentCountError here, but two PHL-only (--SKIPIF-- zend_version)` |
|    - |  887 | `	 * tests assert round()===0, so keep the historical behavior.` |
|    - |  888 | `	 */` |
|  177 |  889 | `	if( nArg < 1 ){` |
|  ! 0 |  890 | `		ph7_result_int(pCtx,0);` |
|  ! 0 |  891 | `		return PH7_OK;` |
|    - |  892 | `	}` |
|  177 |  893 | `	if( nArg > 3 ){` |
|    4 |  894 | `		return PH7_VmThrowException(pCtx,` |
|    - |  895 | `			"ArgumentCountError",` |
|    - |  896 | `			"round() expects at most 3 arguments, %d given",` |
|    1 |  897 | `			nArg` |
|    - |  898 | `			);` |
|    - |  899 | `	}` |
|    - |  900 | `	/*` |
|    - |  901 | `	 * Validate argument #1: only int/float (and numeric strings) are` |
|    - |  902 | `	 * accepted; every other type raises a TypeError (mirrors floor()/ceil()).` |
|    - |  903 | `	 */` |
|  175 |  904 | `	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){` |
|    5 |  905 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |  906 | `			int len;` |
|    5 |  907 | `			sxu8 bReal = FALSE;` |
|    5 |  908 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    5 |  909 | `			if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|    4 |  910 | `				return PH7_VmThrowException(pCtx,` |
|    - |  911 | `					"TypeError",` |
|    - |  912 | `					"round(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |  913 | `					ph7_type_name(apArg[0])` |
|    - |  914 | `					);` |
|    - |  915 | `			}` |
|    2 |  916 | `		}else{` |
|  ! 0 |  917 | `			return PH7_VmThrowException(pCtx,` |
|    - |  918 | `				"TypeError",` |
|    - |  919 | `				"round(): Argument #1 ($num) must be of type int\|float, %s given",` |
|  ! 0 |  920 | `				ph7_type_name(apArg[0])` |
|    - |  921 | `				);` |
|    - |  922 | `		}` |
|    1 |  923 | `	}` |
|    - |  924 | `	/* Precision (arg #2). Negative values are valid; clamp to int range. */` |
|  173 |  925 | `	if( nArg > 1 ){` |
|  143 |  926 | `		sxi64 prec = ph7_value_to_int64(apArg[1]);` |
|  143 |  927 | `		if( prec > 2147483647 ){` |
|  ! 0 |  928 | `			places = 2147483647;` |
|  143 |  929 | `		}else if( prec < -2147483647 ){` |
|  ! 0 |  930 | `			places = -2147483647;` |
|  ! 0 |  931 | `		}else{` |
|  143 |  932 | `			places = (int)prec;` |
|    - |  933 | `		}` |
|   71 |  934 | `	}` |
|    - |  935 | `	/*` |
|    - |  936 | `	 * Mode (arg #3). PHP 8.5 accepts the integer modes 1..8. Read the full` |
|    - |  937 | `	 * 64-bit value before range-checking so a large out-of-range mode cannot` |
|    - |  938 | `	 * alias a valid 1..8 via a truncating 32-bit cast (e.g. 0x1_0000_0003).` |
|    - |  939 | `	 */` |
|  173 |  940 | `	if( nArg > 2 ){` |
|   73 |  941 | `		sxi64 m = ph7_value_to_int64(apArg[2]);` |
|   73 |  942 | `		if( m < PH7_ROUND_HALF_UP \|\| m > PH7_ROUND_AWAY_FROM_ZERO ){` |
|    5 |  943 | `			return PH7_VmThrowException(pCtx,` |
|    - |  944 | `				"ValueError",` |
|    - |  945 | `				"round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)"` |
|    - |  946 | `				);` |
|    - |  947 | `		}` |
|   69 |  948 | `		mode = (int)m;` |
|   34 |  949 | `	}` |
|  169 |  950 | `	value = ph7_value_to_double(apArg[0]);` |
|    - |  951 | `	/* Integer input with non-negative precision needs no rounding. */` |
|  169 |  952 | `	if( ph7_value_is_int(apArg[0]) && places >= 0 ){` |
|   19 |  953 | `		ph7_result_double(pCtx,value);` |
|   19 |  954 | `		return PH7_OK;` |
|    - |  955 | `	}` |
|  151 |  956 | `	r = MathRound(value, places, mode);` |
|  151 |  957 | `	ph7_result_double(pCtx,r);` |
|  151 |  958 | `	return PH7_OK;` |
|   89 |  959 | `}` |
|    - |  960 | `/*` |
|    - |  961 | ` * int intdiv(int $a, int $b)` |
|    - |  962 | ` *  Integer division.` |
|    - |  963 | ` * Parameters` |
|    - |  964 | ` *  $a` |
|    - |  965 | ` *   Number to be divided.` |
|    - |  966 | ` *  $b` |
|    - |  967 | ` *   Number which divides the $a.` |
|    - |  968 | ` * Return` |
|    - |  969 | ` *  The integer quotient of the division of $a by $b.` |
|    - |  970 | ` */` |
|  146 |  971 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 |  972 | `{` |
|    - |  973 | `	sxi64 a,b;` |
|    - |  974 | `	/* PHP requires exactly two arguments. */` |
|  149 |  975 | `	if( nArg != 2 ){` |
|  ! 0 |  976 | `		return PH7_VmThrowException(pCtx,` |
|    - |  977 | `			"ArgumentCountError",` |
|    - |  978 | `			"intdiv() expects exactly 2 arguments, %d given",` |
|  ! 0 |  979 | `			nArg` |
|    - |  980 | `			);` |
|    - |  981 | `	}` |
|    - |  982 | `	/* Type-check argument 1 */` |
|  146 |  983 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|  149 |  984 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 |  985 | `		return PH7_VmThrowException(pCtx,` |
|    - |  986 | `			"TypeError",` |
|    - |  987 | `			"intdiv(): Argument #1 ($num1) must be of type int, %s given",` |
|  ! 0 |  988 | `			ph7_type_name(apArg[0])` |
|    - |  989 | `			);` |
|    - |  990 | `	}` |
|  149 |  991 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  992 | `		int len;` |
|  ! 0 |  993 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|  ! 0 |  994 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 |  995 | `			return PH7_VmThrowException(pCtx,` |
|    - |  996 | `				"TypeError",` |
|    - |  997 | `				"intdiv(): Argument #1 ($num1) must be of type int, string given"` |
|    - |  998 | `				);` |
|    - |  999 | `		}` |
|  ! 0 | 1000 | `	}` |
|    - | 1001 | `	/* Type-check argument 2 */` |
|  146 | 1002 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|  149 | 1003 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|  ! 0 | 1004 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1005 | `			"TypeError",` |
|    - | 1006 | `			"intdiv(): Argument #2 ($num2) must be of type int, %s given",` |
|  ! 0 | 1007 | `			ph7_type_name(apArg[1])` |
|    - | 1008 | `			);` |
|    - | 1009 | `	}` |
|  149 | 1010 | `	if( ph7_value_is_string(apArg[1]) ){` |
|    - | 1011 | `		int len;` |
|  ! 0 | 1012 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|  ! 0 | 1013 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 | 1014 | `			return PH7_VmThrowException(pCtx,` |
|    - | 1015 | `				"TypeError",` |
|    - | 1016 | `				"intdiv(): Argument #2 ($num2) must be of type int, string given"` |
|    - | 1017 | `				);` |
|    - | 1018 | `		}` |
|  ! 0 | 1019 | `	}` |
|    - | 1020 | `	/* Convert both arguments to int64 */` |
|    - | 1021 | `	{` |
|    - | 1022 | `		/* php's ZPP contract for the two int params (lossy float / float-string` |
|    - | 1023 | `		 * deprecations); the manual type checks above already covered arrays,` |
|    - | 1024 | `		 * objects and non-numeric strings with the same messages. */` |
|  149 | 1025 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[0],"intdiv",1,"$num1","int",&a);` |
|  149 | 1026 | `		if( rcArg != PH7_OK ){` |
|  ! 0 | 1027 | `			return rcArg;` |
|    - | 1028 | `		}` |
|  149 | 1029 | `		rcArg = PH7_IntArgResolve(pCtx,apArg[1],"intdiv",2,"$num2","int",&b);` |
|  149 | 1030 | `		if( rcArg != PH7_OK ){` |
|  ! 0 | 1031 | `			return rcArg;` |
|    - | 1032 | `		}` |
|    - | 1033 | `	}` |
|    - | 1034 | `	/* Check for division by zero */` |
|  149 | 1035 | `	if( b == 0 ){` |
|    3 | 1036 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1037 | `			"DivisionByZeroError",` |
|    - | 1038 | `			"Division by zero"` |
|    - | 1039 | `			);` |
|    - | 1040 | `	}` |
|    - | 1041 | `	/* Check for overflow: PHP_INT_MIN / -1 */` |
|  146 | 1042 | `	if( a == SMALLEST_INT64 && b == -1 ){` |
|    3 | 1043 | `		return PH7_VmThrowException(pCtx,` |
|    - | 1044 | `			"ArithmeticError",` |
|    - | 1045 | `			"Division of PHP_INT_MIN by -1 is not an integer"` |
|    - | 1046 | `			);` |
|    - | 1047 | `	}` |
|    - | 1048 | `	/* Perform integer division */` |
|  143 | 1049 | `	ph7_result_int64(pCtx, a / b);` |
|  143 | 1050 | `	return PH7_OK;` |
|   76 | 1051 | `}` |
|    - | 1052 | `/*` |
|    - | 1053 | ` * string dechex(int $number)` |
|    - | 1054 | ` *  Decimal to hexadecimal.` |
|    - | 1055 | ` * Parameters` |
|    - | 1056 | ` *  $number` |
|    - | 1057 | ` *   Decimal value to convert` |
|    - | 1058 | ` * Return` |
|    - | 1059 | ` *  Hexadecimal string representation of number` |
|    - | 1060 | ` */` |
|   14 | 1061 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1062 | `{` |
|    - | 1063 | `	ph7_int64 iVal;` |
|   15 | 1064 | `	if( nArg < 1 ){` |
|    - | 1065 | `		/* Missing arguments,return null */` |
|  ! 0 | 1066 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1067 | `		return PH7_OK;` |
|    - | 1068 | `	}` |
|    - | 1069 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   15 | 1070 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1071 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement,` |
|    - | 1072 | `	 * so a negative value prints all 16 hex digits like PHP. */` |
|   15 | 1073 | `	ph7_result_string_format(pCtx,"%qx",iVal);` |
|   15 | 1074 | `	return PH7_OK;` |
|    8 | 1075 | `}` |
|    - | 1076 | `/*` |
|    - | 1077 | ` * string decoct(int $number)` |
|    - | 1078 | ` *  Decimal to Octal.` |
|    - | 1079 | ` * Parameters` |
|    - | 1080 | ` *  $number` |
|    - | 1081 | ` *   Decimal value to convert` |
|    - | 1082 | ` * Return` |
|    - | 1083 | ` *  Octal string representation of number` |
|    - | 1084 | ` */` |
|   12 | 1085 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1086 | `{` |
|    - | 1087 | `	ph7_int64 iVal;` |
|   13 | 1088 | `	if( nArg < 1 ){` |
|    - | 1089 | `		/* Missing arguments,return null */` |
|  ! 0 | 1090 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1091 | `		return PH7_OK;` |
|    - | 1092 | `	}` |
|    - | 1093 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   13 | 1094 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1095 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|   13 | 1096 | `	ph7_result_string_format(pCtx,"%qo",iVal);` |
|   13 | 1097 | `	return PH7_OK;` |
|    7 | 1098 | `}` |
|    - | 1099 | `/*` |
|    - | 1100 | ` * string decbin(int $number)` |
|    - | 1101 | ` *  Decimal to binary.` |
|    - | 1102 | ` * Parameters` |
|    - | 1103 | ` *  $number` |
|    - | 1104 | ` *   Decimal value to convert` |
|    - | 1105 | ` * Return` |
|    - | 1106 | ` *  Binary string representation of number` |
|    - | 1107 | ` */` |
|   10 | 1108 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1109 | `{` |
|    - | 1110 | `	ph7_int64 iVal;` |
|   11 | 1111 | `	if( nArg < 1 ){` |
|    - | 1112 | `		/* Missing arguments,return null */` |
|  ! 0 | 1113 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1114 | `		return PH7_OK;` |
|    - | 1115 | `	}` |
|    - | 1116 | `	/* Extract the given number as a full 64-bit integer (PHP casts $num to int). */` |
|   11 | 1117 | `	iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1118 | `	/* Format: the 'q' modifier emits the full unsigned 64-bit two's-complement. */` |
|   11 | 1119 | `	ph7_result_string_format(pCtx,"%qB",iVal);` |
|   11 | 1120 | `	return PH7_OK;` |
|    6 | 1121 | `}` |
|    - | 1122 | `/*` |
|    - | 1123 | ` * Convert a base-2/8/16 digit string to a number, mirroring PHP's` |
|    - | 1124 | ` * _php_math_basetozval (ext/standard/math.c) so hexdec/octdec/bindec agree with` |
|    - | 1125 | ` * php byte-for-byte: walk every byte, decode a digit (0-9,a-z,A-Z) or skip any` |
|    - | 1126 | ` * invalid one, accumulate into a signed 64-bit integer and transparently promote` |
|    - | 1127 | ` * to a double once the value would overflow PHP_INT_MAX. The context result is` |
|    - | 1128 | ` * set to an int when it fits, otherwise a float — PHP returns a float for values` |
|    - | 1129 | ` * above PHP_INT_MAX (e.g. hexdec("ffffffffffffffff") == 1.8446744073709552E+19).` |
|    - | 1130 | ` * A byte >= 0x80 (e.g. a UTF-8 continuation) matches none of the digit ranges and` |
|    - | 1131 | ` * is skipped, so leading/interior multibyte junk is ignored like php.` |
|    - | 1132 | ` * Note: php also raises E_DEPRECATED for skipped invalid characters; that notice` |
|    - | 1133 | ` * is not emitted here (a §3.7 deprecation-fidelity residual, value is correct).` |
|    - | 1134 | ` */` |
|   68 | 1135 | `static void MathBaseToNumber(ph7_context *pCtx,const char *zStr,int nLen,int base)` |
|    2 | 1136 | `{` |
|   70 | 1137 | `	sxi64 num = 0;      /* Integer accumulator */` |
|   70 | 1138 | `	double fnum = 0;    /* Float accumulator (used once num would overflow) */` |
|   70 | 1139 | `	int mode = 0;       /* 0 -> integer accumulation, 1 -> switched to float */` |
|   70 | 1140 | `	sxi64 cutoff = SXI64_HIGH / base;      /* PHP_INT_MAX / base */` |
|   70 | 1141 | `	int cutlim = (int)(SXI64_HIGH % base); /* PHP_INT_MAX % base */` |
|   70 | 1142 | `	int bIgnored = 0;   /* any character skipped below? php deprecates that */` |
|    - | 1143 | `	int i;` |
|  674 | 1144 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  606 | 1145 | `		int c = (unsigned char)zStr[i];` |
|  606 | 1146 | `		if( c >= '0' && c <= '9' ){` |
|  476 | 1147 | `			c -= '0';` |
|  369 | 1148 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|   31 | 1149 | `			c -= 'A' - 10;` |
|  117 | 1150 | `		}else if( c >= 'a' && c <= 'z' ){` |
|   94 | 1151 | `			c -= 'a' - 10;` |
|   48 | 1152 | `		}else{` |
|    9 | 1153 | `			bIgnored = 1;` |
|    9 | 1154 | `			continue; /* Not a digit character: skip */` |
|    - | 1155 | `		}` |
|  598 | 1156 | `		if( c >= base ){` |
|   10 | 1157 | `			bIgnored = 1;` |
|   10 | 1158 | `			continue; /* Digit out of range for this base: skip */` |
|    - | 1159 | `		}` |
|  590 | 1160 | `		if( mode == 0 ){` |
|  590 | 1161 | `			if( num < cutoff \|\| (num == cutoff && c <= cutlim) ){` |
|  584 | 1162 | `				num = num * base + c;` |
|  584 | 1163 | `				continue;` |
|    - | 1164 | `			}` |
|    - | 1165 | `			/* Adding this digit would overflow the 64-bit integer: fall back to` |
|    - | 1166 | `			 * float accumulation, seeding it with the value gathered so far. */` |
|    7 | 1167 | `			fnum = (double)num;` |
|    7 | 1168 | `			mode = 1;` |
|    3 | 1169 | `		}` |
|    7 | 1170 | `		fnum = fnum * base + c;` |
|    4 | 1171 | `	}` |
|   70 | 1172 | `	if( bIgnored ){` |
|    - | 1173 | `		/* php 8: characters that are not valid digits for this base are skipped,` |
|    - | 1174 | `		 * and the skipping itself is deprecated (the VALUE is unaffected). */` |
|   14 | 1175 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|    - | 1176 | `			"Invalid characters passed for attempted conversion, these have been ignored");` |
|    6 | 1177 | `	}` |
|   70 | 1178 | `	if( mode == 1 ){` |
|    7 | 1179 | `		ph7_result_double(pCtx,fnum);` |
|    4 | 1180 | `	}else{` |
|   64 | 1181 | `		ph7_result_int64(pCtx,num);` |
|    - | 1182 | `	}` |
|   70 | 1183 | `}` |
|    - | 1184 | `/*` |
|    - | 1185 | ` * int64 hexdec(string $hex_string)` |
|    - | 1186 | ` *  Hexadecimal to decimal.` |
|    - | 1187 | ` * Parameters` |
|    - | 1188 | ` *  $hex_string` |
|    - | 1189 | ` *   The hexadecimal string to convert` |
|    - | 1190 | ` * Return` |
|    - | 1191 | ` *  The decimal representation of hex_string (int, or float on overflow)` |
|    - | 1192 | ` */` |
|   32 | 1193 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1194 | `{` |
|    - | 1195 | `	const char *zString;` |
|    - | 1196 | `	int nLen;` |
|   34 | 1197 | `	if( nArg < 1 ){` |
|    - | 1198 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1199 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1200 | `		return PH7_OK;` |
|    - | 1201 | `	}` |
|   34 | 1202 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1203 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1204 | `		char zBuf[64];` |
|  ! 0 | 1205 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1206 | `			"hexdec(): Argument #1 ($hex_string) must be of type string, %s given",` |
|  ! 0 | 1207 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1208 | `	}` |
|    - | 1209 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1210 | `	 * hex-parses that (hexdec(255) == hexdec("255") == 0x255), so route every` |
|    - | 1211 | `	 * non-throwing value through ph7_value_to_string rather than reading it as` |
|    - | 1212 | `	 * a decimal integer. */` |
|   34 | 1213 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   34 | 1214 | `	MathBaseToNumber(pCtx,zString,nLen,16);` |
|   34 | 1215 | `	return PH7_OK;` |
|   18 | 1216 | `}` |
|    - | 1217 | `/*` |
|    - | 1218 | ` * int64 bindec(string $bin_string)` |
|    - | 1219 | ` *  Binary to decimal.` |
|    - | 1220 | ` * Parameters` |
|    - | 1221 | ` *  $bin_string` |
|    - | 1222 | ` *   The binary string to convert` |
|    - | 1223 | ` * Return` |
|    - | 1224 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|    - | 1225 | ` */` |
|   20 | 1226 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1227 | `{` |
|    - | 1228 | `	const char *zString;` |
|    - | 1229 | `	int nLen;` |
|   21 | 1230 | `	if( nArg < 1 ){` |
|    - | 1231 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1232 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1233 | `		return PH7_OK;` |
|    - | 1234 | `	}` |
|   21 | 1235 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1236 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1237 | `		char zBuf[64];` |
|  ! 0 | 1238 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1239 | `			"bindec(): Argument #1 ($binary_string) must be of type string, %s given",` |
|  ! 0 | 1240 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1241 | `	}` |
|    - | 1242 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1243 | `	 * binary-parses that (bindec(11) == bindec("11") == 3). */` |
|   21 | 1244 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   21 | 1245 | `	MathBaseToNumber(pCtx,zString,nLen,2);` |
|   21 | 1246 | `	return PH7_OK;` |
|   11 | 1247 | `}` |
|    - | 1248 | `/*` |
|    - | 1249 | ` * int64 octdec(string $oct_string)` |
|    - | 1250 | ` *  Octal to decimal.` |
|    - | 1251 | ` * Parameters` |
|    - | 1252 | ` *  $oct_string` |
|    - | 1253 | ` *   The octal string to convert` |
|    - | 1254 | ` * Return` |
|    - | 1255 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|    - | 1256 | ` */` |
|   16 | 1257 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1258 | `{` |
|    - | 1259 | `	const char *zString;` |
|    - | 1260 | `	int nLen;` |
|   17 | 1261 | `	if( nArg < 1 ){` |
|    - | 1262 | `		/* Missing arguments,return -1 */` |
|  ! 0 | 1263 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1264 | `		return PH7_OK;` |
|    - | 1265 | `	}` |
|   17 | 1266 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) ){` |
|    - | 1267 | `		/* PHP 8 throws a catchable TypeError for a non-string-coercible argument. */` |
|    - | 1268 | `		char zBuf[64];` |
|  ! 0 | 1269 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 1270 | `			"octdec(): Argument #1 ($octal_string) must be of type string, %s given",` |
|  ! 0 | 1271 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|    - | 1272 | `	}` |
|    - | 1273 | ``	/* PHP's `string` ZPP renders scalars/null to their string form and then`` |
|    - | 1274 | `	 * octal-parses that (octdec(11) == octdec("11") == 9). */` |
|   17 | 1275 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   17 | 1276 | `	MathBaseToNumber(pCtx,zString,nLen,8);` |
|   17 | 1277 | `	return PH7_OK;` |
|    9 | 1278 | `}` |
|    - | 1279 | `/*` |
|    - | 1280 | ` * srand([int $seed])` |
|    - | 1281 | ` * mt_srand([int $seed])` |
|    - | 1282 | ` *  Seed the random number generator.` |
|    - | 1283 | ` * Parameters` |
|    - | 1284 | ` * $seed` |
|    - | 1285 | ` *  Optional seed value` |
|    - | 1286 | ` * Return` |
|    - | 1287 | ` *  null.` |
|    - | 1288 | ` * Note:` |
|    - | 1289 | ` *  THIS FUNCTION IS A NO-OP.` |
|    - | 1290 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|    - | 1291 | ` */` |
|   20 | 1292 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1293 | `{` |
|   10 | 1294 | `	SXUNUSED(nArg);` |
|   10 | 1295 | `	SXUNUSED(apArg);` |
|   21 | 1296 | `	ph7_result_null(pCtx);` |
|   21 | 1297 | `	return PH7_OK;` |
|    1 | 1298 | `}` |
|    - | 1299 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - | 1300 | `/*` |
|    - | 1301 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|    - | 1302 | ` *  Convert a number between arbitrary bases.` |
|    - | 1303 | ` * Parameters` |
|    - | 1304 | ` * $number` |
|    - | 1305 | ` *  The number to convert` |
|    - | 1306 | ` * $frombase` |
|    - | 1307 | ` *  The base number is in` |
|    - | 1308 | ` * $tobase` |
|    - | 1309 | ` *  The base to convert number to` |
|    - | 1310 | ` * Return` |
|    - | 1311 | ` *  Number converted to base tobase` |
|    - | 1312 | ` */` |
|   58 | 1313 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1314 | `{` |
|    - | 1315 | `	static const char zDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";` |
|    - | 1316 | `	int nLen,iFbase,iTobase,i;` |
|    - | 1317 | `	int bIgnored;` |
|    - | 1318 | `	ph7_int64 iFbase64,iTobase64;` |
|    - | 1319 | `	const char *zNum;` |
|   59 | 1320 | `	sxu64 uNum = 0;` |
|   59 | 1321 | `	if( nArg < 3 ){` |
|    - | 1322 | `		/* Return the empty string*/` |
|  ! 0 | 1323 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1324 | `		return PH7_OK;` |
|    - | 1325 | `	}` |
|    - | 1326 | `	/* Base numbers. Read them as 64-bit so an out-of-range base can't wrap through` |
|    - | 1327 | `	 * a 32-bit truncation back into the 2..36 window and bypass the check below. */` |
|   59 | 1328 | `	iFbase64 = ph7_value_to_int64(apArg[1]);` |
|   59 | 1329 | `	iTobase64 = ph7_value_to_int64(apArg[2]);` |
|    - | 1330 | `	/* PHP 8 throws a catchable ValueError for a base outside 2..36; from_base` |
|    - | 1331 | `	 * is validated before to_base, both before the string is even parsed. */` |
|   59 | 1332 | `	if( iFbase64 < 2 \|\| iFbase64 > 36 ){` |
|    7 | 1333 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1334 | `			"base_convert(): Argument #2 ($from_base) must be between 2 and 36 (inclusive)");` |
|    - | 1335 | `	}` |
|   53 | 1336 | `	if( iTobase64 < 2 \|\| iTobase64 > 36 ){` |
|    5 | 1337 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1338 | `			"base_convert(): Argument #3 ($to_base) must be between 2 and 36 (inclusive)");` |
|    - | 1339 | `	}` |
|    - | 1340 | `	/* Both bases are now known to fit in [2,36], so the int form is exact. */` |
|   49 | 1341 | `	iFbase  = (int)iFbase64;` |
|   49 | 1342 | `	iTobase = (int)iTobase64;` |
|    - | 1343 | `	/* Parse the input number in from_base. Every base is handled the same way:` |
|    - | 1344 | `	 * digits 0-9 then a-z/A-Z map to 0-35; a character that is not a valid digit for` |
|    - | 1345 | `	 * from_base is ignored, and php raises an E_DEPRECATED saying so. */` |
|   49 | 1346 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    3 | 1347 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|    - | 1348 | `			"base_convert(): Passing null to parameter #1 ($num) of type string is deprecated");` |
|    1 | 1349 | `	}` |
|   49 | 1350 | `	zNum = ph7_value_to_string(apArg[0],&nLen);` |
|   49 | 1351 | `	bIgnored = 0;` |
|  147 | 1352 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   99 | 1353 | `		int c = (unsigned char)zNum[i];` |
|    - | 1354 | `		int d;` |
|   99 | 1355 | `		if( c >= '0' && c <= '9' ){` |
|   73 | 1356 | `			d = c - '0';` |
|   63 | 1357 | `		}else if( c >= 'a' && c <= 'z' ){` |
|   25 | 1358 | `			d = c - 'a' + 10;` |
|   15 | 1359 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|    3 | 1360 | `			d = c - 'A' + 10;` |
|    2 | 1361 | `		}else{` |
|  ! 0 | 1362 | `			d = 99;` |
|    - | 1363 | `		}` |
|   99 | 1364 | `		if( d >= iFbase ){` |
|    - | 1365 | `			/* Not a valid digit for this base: skip it (php), but say so afterwards. */` |
|    3 | 1366 | `			bIgnored = 1;` |
|    3 | 1367 | `			continue;` |
|    - | 1368 | `		}` |
|   97 | 1369 | `		uNum = uNum * (sxu64)iFbase + (sxu64)d;` |
|   49 | 1370 | `	}` |
|   49 | 1371 | `	if( bIgnored ){` |
|    3 | 1372 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|    - | 1373 | `			"Invalid characters passed for attempted conversion, these have been ignored");` |
|    1 | 1374 | `	}` |
|    - | 1375 | `	/* Format the result in to_base using lowercase digits. */` |
|   49 | 1376 | `	if( uNum == 0 ){` |
|    9 | 1377 | `		ph7_result_string(pCtx,"0",1);` |
|    5 | 1378 | `	}else{` |
|    - | 1379 | `		char zOut[70]; /* base-2 of a 64-bit value fits in 64 digits */` |
|   41 | 1380 | `		int n = 0,j;` |
|  133 | 1381 | `		while( uNum > 0 ){` |
|   93 | 1382 | `			zOut[n++] = zDigits[uNum % (sxu64)iTobase];` |
|   93 | 1383 | `			uNum /= (sxu64)iTobase;` |
|    1 | 1384 | `		}` |
|    - | 1385 | `		/* Digits were produced least-significant first: reverse in place. */` |
|   79 | 1386 | `		for( j = 0 ; j < n/2 ; ++j ){` |
|   39 | 1387 | `			char t = zOut[j];` |
|   39 | 1388 | `			zOut[j] = zOut[n - 1 - j];` |
|   39 | 1389 | `			zOut[n - 1 - j] = t;` |
|   20 | 1390 | `		}` |
|   41 | 1391 | `		ph7_result_string(pCtx,zOut,n);` |
|    - | 1392 | `	}` |
|   49 | 1393 | `	return PH7_OK;` |
|   30 | 1394 | `}` |
|    - | 1395 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - | 1396 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1397 |  |
