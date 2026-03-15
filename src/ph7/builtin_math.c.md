# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 417/420 lines (99.29%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|    - |    8 |  |
|    - |    9 | `/*` |
|    - |   10 | ` * Section:` |
|    - |   11 | ` *    Math Functions.` |
|    - |   12 |  |
|    - |   13 | ` * Status:` |
|    - |   14 | ` *    Stable.` |
|    - |   15 | ` */` |
|    - |   16 | `#include <stdlib.h> /* abs */` |
|    - |   17 | `#include <math.h>` |
|    - |   18 | `/*` |
|    - |   19 | ` * float sqrt(float $arg )` |
|    - |   20 | ` *  Square root of the given number.` |
|    - |   21 | ` * Parameter` |
|    - |   22 | ` *  The number to process.` |
|    - |   23 | ` * Return` |
|    - |   24 | ` *  The square root of arg or the special value Nan of failure.` |
|    - |   25 | ` */` |
|    6 |   26 | `PH7_PRIVATE int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   27 |  |
|    - |   28 | `	double r,x;` |
|    7 |   29 | `	if( nArg < 1 ){` |
|    - |   30 | `		/* Missing argument,return 0 */` |
|    5 |   31 | `		ph7_result_int(pCtx,0);` |
|    5 |   32 | `		return PH7_OK;` |
|    - |   33 | `	}` |
|    3 |   34 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |   35 | `	/* Perform the requested operation */` |
|    3 |   36 | `	r = sqrt(x);` |
|    - |   37 | `	/* store the result back */` |
|    3 |   38 | `	ph7_result_double(pCtx,r);` |
|    3 |   39 | `	return PH7_OK;` |
|    4 |   40 |  |
|    - |   41 | `/*` |
|    - |   42 | ` * float exp(float $arg )` |
|    - |   43 | ` *  Calculates the exponent of e.` |
|    - |   44 | ` * Parameter` |
|    - |   45 | ` *  The number to process.` |
|    - |   46 | ` * Return` |
|    - |   47 | ` *  'e' raised to the power of arg.` |
|    - |   48 | ` */` |
|   20 |   49 | `PH7_PRIVATE int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |   50 |  |
|    - |   51 | `	double r,x;` |
|   21 |   52 | `	if( nArg < 1 ){` |
|    - |   53 | `		/* Missing argument,return 0 */` |
|    3 |   54 | `		ph7_result_int(pCtx,0);` |
|    3 |   55 | `		return PH7_OK;` |
|    - |   56 | `	}` |
|   19 |   57 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |   58 | `	/* Perform the requested operation */` |
|   19 |   59 | `	r = exp(x);` |
|    - |   60 | `	/* store the result back */` |
|   19 |   61 | `	ph7_result_double(pCtx,r);` |
|   19 |   62 | `	return PH7_OK;` |
|   11 |   63 |  |
|    - |   64 | `/*` |
|    - |   65 | ` * float floor(float $arg )` |
|    - |   66 | ` *  Round fractions down.` |
|    - |   67 | ` * Parameter` |
|    - |   68 | ` *  The number to process.` |
|    - |   69 | ` * Return` |
|    - |   70 | ` *  Returns the next lowest integer value by rounding down value if necessary.` |
|    - |   71 | ` */` |
|   14 |   72 | `PH7_PRIVATE int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |   73 |  |
|    - |   74 | `	double r,x;` |
|    - |   75 | `	/* PHP requires exactly one argument. */` |
|   16 |   76 | `	if( nArg != 1 ){` |
|    7 |   77 | `		return PH7_VmThrowException(pCtx,` |
|    - |   78 | `			"ArgumentCountError",` |
|    - |   79 | `			"floor() expects exactly 1 argument, %d given",` |
|    2 |   80 | `			nArg` |
|    - |   81 | `			);` |
|    - |   82 | `	}` |
|    - |   83 | `	/*` |
|    - |   84 | `	 * Validate argument type. Only int/float (and numeric strings) are accepted.` |
|    - |   85 | `	 * Other types (including non-numeric strings) raise a TypeError just like` |
|    - |   86 | `	 * ceil() and other math functions.` |
|    - |   87 | `	 */` |
|   12 |   88 | `	if( ph7_value_is_int(apArg[0]) == 0 && ph7_value_is_float(apArg[0]) == 0 ){` |
|    6 |   89 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |   90 | `			int len;` |
|    6 |   91 | `			sxu8 bReal = FALSE;` |
|    6 |   92 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |   93 | `			sxi32 rcNum;` |
|    6 |   94 | `			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|    6 |   95 | `			if( rcNum != SXRET_OK ){` |
|    4 |   96 | `				return PH7_VmThrowException(pCtx,` |
|    - |   97 | `					"TypeError",` |
|    - |   98 | `					"floor(): Argument #1 ($num) must be of type int\|float, %s given",` |
|    1 |   99 | `					ph7_type_name(apArg[0])` |
|    - |  100 | `					);` |
|    - |  101 | `			}` |
|    2 |  102 | `		}else{` |
|    - |  103 | `			/* Disallow all other types (arrays, objects, resources, etc.) */` |
|  ! 0 |  104 | `			return PH7_VmThrowException(pCtx,` |
|    - |  105 | `				"TypeError",` |
|    - |  106 | `				"floor(): Argument #1 ($num) must be of type int\|float, %s given",` |
|  ! 0 |  107 | `				ph7_type_name(apArg[0])` |
|    - |  108 | `				);` |
|    - |  109 | `		}` |
|    1 |  110 | `	}` |
|    - |  111 |  |
|    9 |  112 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  113 | `	/* Perform the requested operation */` |
|    9 |  114 | `	r = floor(x);` |
|    - |  115 | `	/* store the result back */` |
|    9 |  116 | `	ph7_result_double(pCtx,r);` |
|    9 |  117 | `	return PH7_OK;` |
|    9 |  118 |  |
|    - |  119 | `/*` |
|    - |  120 | ` * float cos(float $arg )` |
|    - |  121 | ` *  Cosine.` |
|    - |  122 | ` * Parameter` |
|    - |  123 | ` *  The number to process.` |
|    - |  124 | ` * Return` |
|    - |  125 | ` *  The cosine of arg.` |
|    - |  126 | ` */` |
|    4 |  127 | `PH7_PRIVATE int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  128 |  |
|    - |  129 | `	double r,x;` |
|    5 |  130 | `	if( nArg < 1 ){` |
|    - |  131 | `		/* Missing argument,return 0 */` |
|    3 |  132 | `		ph7_result_int(pCtx,0);` |
|    3 |  133 | `		return PH7_OK;` |
|    - |  134 | `	}` |
|    3 |  135 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  136 | `	/* Perform the requested operation */` |
|    3 |  137 | `	r = cos(x);` |
|    - |  138 | `	/* store the result back */` |
|    3 |  139 | `	ph7_result_double(pCtx,r);` |
|    3 |  140 | `	return PH7_OK;` |
|    3 |  141 |  |
|    - |  142 | `/*` |
|    - |  143 | ` * float acos(float $arg )` |
|    - |  144 | ` *  Arc cosine.` |
|    - |  145 | ` * Parameter` |
|    - |  146 | ` *  The number to process.` |
|    - |  147 | ` * Return` |
|    - |  148 | ` *  The arc cosine of arg.` |
|    - |  149 | ` */` |
|   22 |  150 | `PH7_PRIVATE int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  151 |  |
|    - |  152 | `	double r, x;` |
|    - |  153 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|   24 |  154 | `	if( nArg != 1 ){` |
|    4 |  155 | `		return PH7_VmThrowException(pCtx,` |
|    - |  156 | `			"ArgumentCountError",` |
|    - |  157 | `			"acos() expects exactly 1 argument, %d given",` |
|    1 |  158 | `			nArg` |
|    - |  159 | `			);` |
|    - |  160 | `	}` |
|    - |  161 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|    - |  162 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|    - |  163 | `	 * the float conversion will handle them. */` |
|   22 |  164 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    7 |  165 | `		return PH7_VmThrowException(pCtx,` |
|    - |  166 | `			"TypeError",` |
|    - |  167 | `			"acos(): Argument #1 ($num) must be of type float, %s given",` |
|    2 |  168 | `			ph7_type_name(apArg[0])` |
|    - |  169 | `			);` |
|    - |  170 | `	}` |
|    - |  171 | `	/* Convert to double now that we know it's numeric. */` |
|   17 |  172 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  173 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|   17 |  174 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|    5 |  175 | `		r = PH7_NAN_VALUE();` |
|    3 |  176 | `	}else{` |
|   13 |  177 | `		r = acos(x);` |
|    - |  178 | `	}` |
|    - |  179 | `	/* store the result back */` |
|   17 |  180 | `	ph7_result_double(pCtx,r);` |
|   17 |  181 | `	return PH7_OK;` |
|   13 |  182 |  |
|    - |  183 | `/*` |
|    - |  184 | ` * float cosh(float $arg )` |
|    - |  185 | ` *  Hyperbolic cosine.` |
|    - |  186 | ` * Parameter` |
|    - |  187 | ` *  The number to process.` |
|    - |  188 | ` * Return` |
|    - |  189 | ` *  The hyperbolic cosine of arg.` |
|    - |  190 | ` */` |
|   18 |  191 | `PH7_PRIVATE int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  192 |  |
|    - |  193 | `	double r,x;` |
|   19 |  194 | `	if( nArg < 1 ){` |
|    - |  195 | `		/* Missing argument,return 0 */` |
|    3 |  196 | `		ph7_result_int(pCtx,0);` |
|    3 |  197 | `		return PH7_OK;` |
|    - |  198 | `	}` |
|   17 |  199 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  200 | `	/* Perform the requested operation */` |
|   17 |  201 | `	r = cosh(x);` |
|    - |  202 | `	/* store the result back */` |
|   17 |  203 | `	ph7_result_double(pCtx,r);` |
|   17 |  204 | `	return PH7_OK;` |
|   10 |  205 |  |
|    - |  206 | `/*` |
|    - |  207 | ` * float sin(float $arg )` |
|    - |  208 | ` *  Sine.` |
|    - |  209 | ` * Parameter` |
|    - |  210 | ` *  The number to process.` |
|    - |  211 | ` * Return` |
|    - |  212 | ` *  The sine of arg.` |
|    - |  213 | ` */` |
|    8 |  214 | `PH7_PRIVATE int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  215 |  |
|    - |  216 | `	double r,x;` |
|    9 |  217 | `	if( nArg < 1 ){` |
|    - |  218 | `		/* Missing argument,return 0 */` |
|    7 |  219 | `		ph7_result_int(pCtx,0);` |
|    7 |  220 | `		return PH7_OK;` |
|    - |  221 | `	}` |
|    3 |  222 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  223 | `	/* Perform the requested operation */` |
|    3 |  224 | `	r = sin(x);` |
|    - |  225 | `	/* store the result back */` |
|    3 |  226 | `	ph7_result_double(pCtx,r);` |
|    3 |  227 | `	return PH7_OK;` |
|    5 |  228 |  |
|    - |  229 | `/*` |
|    - |  230 | ` * float asin(float $arg )` |
|    - |  231 | ` *  Arc sine.` |
|    - |  232 | ` * Parameter` |
|    - |  233 | ` *  The number to process.` |
|    - |  234 | ` * Return` |
|    - |  235 | ` *  The arc sine of arg.` |
|    - |  236 | ` */` |
|   14 |  237 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  238 |  |
|    - |  239 | `	double r,x;` |
|   15 |  240 | `	if( nArg < 1 ){` |
|    - |  241 | `		/* Missing argument,return 0 */` |
|    3 |  242 | `		ph7_result_int(pCtx,0);` |
|    3 |  243 | `		return PH7_OK;` |
|    - |  244 | `	}` |
|   13 |  245 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  246 | `	/* Perform the requested operation */` |
|   13 |  247 | `	r = asin(x);` |
|    - |  248 | `	/* store the result back */` |
|   13 |  249 | `	ph7_result_double(pCtx,r);` |
|   13 |  250 | `	return PH7_OK;` |
|    8 |  251 |  |
|    - |  252 | `/*` |
|    - |  253 | ` * float sinh(float $arg )` |
|    - |  254 | ` *  Hyperbolic sine.` |
|    - |  255 | ` * Parameter` |
|    - |  256 | ` *  The number to process.` |
|    - |  257 | ` * Return` |
|    - |  258 | ` *  The hyperbolic sine of arg.` |
|    - |  259 | ` */` |
|   20 |  260 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  261 |  |
|    - |  262 | `	double r,x;` |
|   21 |  263 | `	if( nArg < 1 ){` |
|    - |  264 | `		/* Missing argument,return 0 */` |
|    3 |  265 | `		ph7_result_int(pCtx,0);` |
|    3 |  266 | `		return PH7_OK;` |
|    - |  267 | `	}` |
|   19 |  268 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  269 | `	/* Perform the requested operation */` |
|   19 |  270 | `	r = sinh(x);` |
|    - |  271 | `	/* store the result back */` |
|   19 |  272 | `	ph7_result_double(pCtx,r);` |
|   19 |  273 | `	return PH7_OK;` |
|   11 |  274 |  |
|    - |  275 | `/*` |
|    - |  276 | ` * float ceil(float $arg )` |
|    - |  277 | ` *  Round fractions up.` |
|    - |  278 | ` * Parameter` |
|    - |  279 | ` *  The number to process.` |
|    - |  280 | ` * Return` |
|    - |  281 | ` *  The next highest integer value by rounding up value if necessary.` |
|    - |  282 | ` */` |
|   14 |  283 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  284 |  |
|    - |  285 | `	double r,x;` |
|    - |  286 | `	/* PHP requires exactly one argument. */` |
|   16 |  287 | `	if( nArg != 1 ){` |
|    7 |  288 | `		return PH7_VmThrowException(pCtx,` |
|    - |  289 | `			"ArgumentCountError",` |
|    - |  290 | `			"ceil() expects exactly 1 argument, %d given",` |
|    2 |  291 | `			nArg` |
|    - |  292 | `			);` |
|    - |  293 | `	}` |
|    - |  294 | `	/*` |
|    - |  295 | `	 * PHP only accepts ints, floats or numeric strings.  Any other types` |
|    - |  296 | `	 * (in particular non-numeric strings) should raise a TypeError.  We` |
|    - |  297 | `	 * mimic the approach used by abs() and perform an explicit numeric` |
|    - |  298 | `	 * check on strings before converting to double.` |
|    - |  299 | `	 */` |
|   12 |  300 | `	if( !ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]) ){` |
|    6 |  301 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |  302 | `			int len;` |
|    6 |  303 | `			sxu8 bReal = FALSE;` |
|    6 |  304 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  305 | `			sxi32 rcNum;` |
|    6 |  306 | `			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|    6 |  307 | `			if( rcNum != SXRET_OK ){` |
|    3 |  308 | `				return PH7_VmThrowException(pCtx,` |
|    - |  309 | `					"TypeError",` |
|    - |  310 | `					"ceil(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  311 | `					);` |
|    - |  312 | `			}` |
|    2 |  313 | `		}else{` |
|    - |  314 | `			/* Reject arrays, objects, resources, booleans, NULL, etc. */` |
|  ! 0 |  315 | `			return PH7_VmThrowException(pCtx,` |
|    - |  316 | `				"TypeError",` |
|    - |  317 | `				"ceil(): Argument #1 ($num) must be of type int\|float"` |
|    - |  318 | `				);` |
|    - |  319 | `		}` |
|    1 |  320 | `	}` |
|    - |  321 |  |
|    9 |  322 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  323 | `	/* Perform the requested operation */` |
|    9 |  324 | `	r = ceil(x);` |
|    - |  325 | `	/* store the result back */` |
|    9 |  326 | `	ph7_result_double(pCtx,r);` |
|    9 |  327 | `	return PH7_OK;` |
|    9 |  328 |  |
|    - |  329 | `/*` |
|    - |  330 | ` * float tan(float $arg )` |
|    - |  331 | ` *  Tangent.` |
|    - |  332 | ` * Parameter` |
|    - |  333 | ` *  The number to process.` |
|    - |  334 | ` * Return` |
|    - |  335 | ` *  The tangent of arg.` |
|    - |  336 | ` */` |
|    6 |  337 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  338 |  |
|    - |  339 | `	double r,x;` |
|    7 |  340 | `	if( nArg < 1 ){` |
|    - |  341 | `		/* Missing argument,return 0 */` |
|    3 |  342 | `		ph7_result_int(pCtx,0);` |
|    3 |  343 | `		return PH7_OK;` |
|    - |  344 | `	}` |
|    5 |  345 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  346 | `	/* Perform the requested operation */` |
|    5 |  347 | `	r = tan(x);` |
|    - |  348 | `	/* store the result back */` |
|    5 |  349 | `	ph7_result_double(pCtx,r);` |
|    5 |  350 | `	return PH7_OK;` |
|    4 |  351 |  |
|    - |  352 | `/*` |
|    - |  353 | ` * float atan(float $arg )` |
|    - |  354 | ` *  Arc tangent.` |
|    - |  355 | ` * Parameter` |
|    - |  356 | ` *  The number to process.` |
|    - |  357 | ` * Return` |
|    - |  358 | ` *  The arc tangent of arg.` |
|    - |  359 | ` */` |
|   16 |  360 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  361 |  |
|    - |  362 | `	double r,x;` |
|   17 |  363 | `	if( nArg < 1 ){` |
|    - |  364 | `		/* Missing argument,return 0 */` |
|    5 |  365 | `		ph7_result_int(pCtx,0);` |
|    5 |  366 | `		return PH7_OK;` |
|    - |  367 | `	}` |
|   13 |  368 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  369 | `	/* Perform the requested operation */` |
|   13 |  370 | `	r = atan(x);` |
|    - |  371 | `	/* store the result back */` |
|   13 |  372 | `	ph7_result_double(pCtx,r);` |
|   13 |  373 | `	return PH7_OK;` |
|    9 |  374 |  |
|    - |  375 | `/*` |
|    - |  376 | ` * float tanh(float $arg )` |
|    - |  377 | ` *  Hyperbolic tangent.` |
|    - |  378 | ` * Parameter` |
|    - |  379 | ` *  The number to process.` |
|    - |  380 | ` * Return` |
|    - |  381 | ` *  The Hyperbolic tangent of arg.` |
|    - |  382 | ` */` |
|   20 |  383 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  384 |  |
|    - |  385 | `	double r,x;` |
|   21 |  386 | `	if( nArg < 1 ){` |
|    - |  387 | `		/* Missing argument,return 0 */` |
|    3 |  388 | `		ph7_result_int(pCtx,0);` |
|    3 |  389 | `		return PH7_OK;` |
|    - |  390 | `	}` |
|   19 |  391 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  392 | `	/* Perform the requested operation */` |
|   19 |  393 | `	r = tanh(x);` |
|    - |  394 | `	/* store the result back */` |
|   19 |  395 | `	ph7_result_double(pCtx,r);` |
|   19 |  396 | `	return PH7_OK;` |
|   11 |  397 |  |
|    - |  398 | `/*` |
|    - |  399 | ` * float atan2(float $y,float $x)` |
|    - |  400 | ` *  Arc tangent of two variable.` |
|    - |  401 | ` * Parameter` |
|    - |  402 | ` *  $y = Dividend parameter.` |
|    - |  403 | ` *  $x = Divisor parameter.` |
|    - |  404 | ` * Return` |
|    - |  405 | ` *  The arc tangent of y/x in radian.` |
|    - |  406 | ` */` |
|   10 |  407 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  408 |  |
|    - |  409 | `	double r,x,y;` |
|   11 |  410 | `	if( nArg < 2 ){` |
|    - |  411 | `		/* Missing arguments,return 0 */` |
|    5 |  412 | `		ph7_result_int(pCtx,0);` |
|    5 |  413 | `		return PH7_OK;` |
|    - |  414 | `	}` |
|    7 |  415 | `	y = ph7_value_to_double(apArg[0]);` |
|    7 |  416 | `	x = ph7_value_to_double(apArg[1]);` |
|    - |  417 | `	/* Perform the requested operation */` |
|    7 |  418 | `	r = atan2(y,x);` |
|    - |  419 | `	/* store the result back */` |
|    7 |  420 | `	ph7_result_double(pCtx,r);` |
|    7 |  421 | `	return PH7_OK;` |
|    6 |  422 |  |
|    - |  423 | `/*` |
|    - |  424 | ` * float/int64 abs(float/int64 $arg )` |
|    - |  425 | ` *  Absolute value.` |
|    - |  426 | ` * Parameter` |
|    - |  427 | ` *  The number to process.` |
|    - |  428 | ` * Return` |
|    - |  429 | ` *  The absolute value of number.` |
|    - |  430 | ` */` |
|  122 |  431 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  432 |  |
|    - |  433 | `	int is_float;` |
|    - |  434 | `	/* PHP requires exactly one argument. */` |
|  124 |  435 | `	if( nArg != 1 ){` |
|   11 |  436 | `		return PH7_VmThrowException(pCtx,` |
|    - |  437 | `			"ArgumentCountError",` |
|    - |  438 | `			"abs() expects exactly 1 argument, %d given",` |
|    3 |  439 | `			nArg` |
|    - |  440 | `			);` |
|    - |  441 | `	}` |
|    - |  442 |  |
|    - |  443 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|  118 |  444 | `	is_float = ph7_value_is_float(apArg[0]);` |
|  118 |  445 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|    - |  446 | `		int len;` |
|   10 |  447 | `		sxu8 bReal = FALSE;` |
|   10 |  448 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  449 | `		sxi32 rcNum;` |
|   10 |  450 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|   10 |  451 | `		if( rcNum != SXRET_OK ){` |
|    3 |  452 | `			return PH7_VmThrowException(pCtx,` |
|    - |  453 | `				"TypeError",` |
|    - |  454 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  455 | `				);` |
|    - |  456 | `		}` |
|    7 |  457 | `		if( bReal ){` |
|    5 |  458 | `			is_float = 1;` |
|    2 |  459 | `		}` |
|    3 |  460 | `	}` |
|  116 |  461 | `	if( is_float ){` |
|    - |  462 | `		double r,x;` |
|   99 |  463 | `		x = ph7_value_to_double(apArg[0]);` |
|    - |  464 | `		/* Perform the requested operation */` |
|   99 |  465 | `		r = fabs(x);` |
|   99 |  466 | `		ph7_result_double(pCtx,r);` |
|   50 |  467 | `	}else{` |
|    - |  468 | `		int r,x;` |
|   18 |  469 | `		x = ph7_value_to_int(apArg[0]);` |
|    - |  470 | `		/* Perform the requested operation */` |
|   18 |  471 | `		r = abs(x);` |
|   18 |  472 | `		ph7_result_int(pCtx,r);` |
|    - |  473 | `	}` |
|  116 |  474 | `	return PH7_OK;` |
|   63 |  475 |  |
|    - |  476 | `/*` |
|    - |  477 | ` * float log(float $arg,[int/float $base])` |
|    - |  478 | ` *  Natural logarithm.` |
|    - |  479 | ` * Parameter` |
|    - |  480 | ` *  $arg: The number to process.` |
|    - |  481 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|    - |  482 | ` * Return` |
|    - |  483 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|    - |  484 | ` * Note:` |
|    - |  485 | ` *  only Natural log and base-10 log are supported.` |
|    - |  486 | ` */` |
|   14 |  487 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  488 |  |
|    - |  489 | `	double r,x;` |
|   15 |  490 | `	if( nArg < 1 ){` |
|    - |  491 | `		/* Missing argument,return 0 */` |
|    3 |  492 | `		ph7_result_int(pCtx,0);` |
|    3 |  493 | `		return PH7_OK;` |
|    - |  494 | `	}` |
|   13 |  495 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  496 | `	/* Perform the requested operation */` |
|   13 |  497 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|    - |  498 | `		/* Base-10 log */` |
|    5 |  499 | `		r = log10(x);` |
|    3 |  500 | `	}else{` |
|    9 |  501 | `		r = log(x);` |
|    - |  502 | `	}` |
|    - |  503 | `	/* store the result back */` |
|   13 |  504 | `	ph7_result_double(pCtx,r);` |
|   13 |  505 | `	return PH7_OK;` |
|    8 |  506 |  |
|    - |  507 | `/*` |
|    - |  508 | ` * float log10(float $arg )` |
|    - |  509 | ` *  Base-10 logarithm.` |
|    - |  510 | ` * Parameter` |
|    - |  511 | ` *  The number to process.` |
|    - |  512 | ` * Return` |
|    - |  513 | ` *  The Base-10 logarithm of the given number.` |
|    - |  514 | ` */` |
|   16 |  515 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  516 |  |
|    - |  517 | `	double r,x;` |
|   17 |  518 | `	if( nArg < 1 ){` |
|    - |  519 | `		/* Missing argument,return 0 */` |
|    3 |  520 | `		ph7_result_int(pCtx,0);` |
|    3 |  521 | `		return PH7_OK;` |
|    - |  522 | `	}` |
|   15 |  523 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  524 | `	/* Perform the requested operation */` |
|   15 |  525 | `	r = log10(x);` |
|    - |  526 | `	/* store the result back */` |
|   15 |  527 | `	ph7_result_double(pCtx,r);` |
|   15 |  528 | `	return PH7_OK;` |
|    9 |  529 |  |
|    - |  530 | `/*` |
|    - |  531 | ` * number pow(number $base,number $exp)` |
|    - |  532 | ` *  Exponential expression.` |
|    - |  533 | ` * Parameter` |
|    - |  534 | ` *  base` |
|    - |  535 | ` *  The base to use.` |
|    - |  536 | ` * exp` |
|    - |  537 | ` *  The exponent.` |
|    - |  538 | ` * Return` |
|    - |  539 | ` *  base raised to the power of exp.` |
|    - |  540 | ` *  If the result can be represented as integer it will be returned` |
|    - |  541 | ` *  as type integer, else it will be returned as type float.` |
|    - |  542 | ` */` |
|    8 |  543 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  544 |  |
|    - |  545 | `	double r,x,y;` |
|    9 |  546 | `	if( nArg < 1 ){` |
|    - |  547 | `		/* Missing argument,return 0 */` |
|    5 |  548 | `		ph7_result_int(pCtx,0);` |
|    5 |  549 | `		return PH7_OK;` |
|    - |  550 | `	}` |
|    5 |  551 | `	x = ph7_value_to_double(apArg[0]);` |
|    5 |  552 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  553 | `	/* Perform the requested operation */` |
|    5 |  554 | `	r = pow(x,y);` |
|    5 |  555 | `	ph7_result_double(pCtx,r);` |
|    5 |  556 | `	return PH7_OK;` |
|    5 |  557 |  |
|    - |  558 | `/*` |
|    - |  559 | ` * float pi(void)` |
|    - |  560 | ` *  Returns an approximation of pi.` |
|    - |  561 | ` * Note` |
|    - |  562 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|    - |  563 | ` * Return` |
|    - |  564 | ` *  The value of pi as float.` |
|    - |  565 | ` */` |
|    2 |  566 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  567 |  |
|    1 |  568 | `	SXUNUSED(nArg); /* cc warning */` |
|    1 |  569 | `	SXUNUSED(apArg);` |
|    3 |  570 | `	ph7_result_double(pCtx,PH7_PI);` |
|    3 |  571 | `	return PH7_OK;` |
|    1 |  572 |  |
|    - |  573 | `/*` |
|    - |  574 | ` * float fmod(float $x,float $y)` |
|    - |  575 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|    - |  576 | ` * Parameters` |
|    - |  577 | ` * $x` |
|    - |  578 | ` *  The dividend` |
|    - |  579 | ` * $y` |
|    - |  580 | ` *  The divisor` |
|    - |  581 | ` * Return` |
|    - |  582 | ` *  The floating point remainder of x/y.` |
|    - |  583 | ` */` |
|    8 |  584 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  585 |  |
|    - |  586 | `	double x,y,r;` |
|    9 |  587 | `	if( nArg < 2 ){` |
|    - |  588 | `		/* Missing arguments */` |
|    7 |  589 | `		ph7_result_double(pCtx,0);` |
|    7 |  590 | `		return PH7_OK;` |
|    - |  591 | `	}` |
|    - |  592 | `	/* Extract given arguments */` |
|    3 |  593 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  594 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  595 | `	/* Perform the requested operation */` |
|    3 |  596 | `	r = fmod(x,y);` |
|    - |  597 | `	/* Processing result */` |
|    3 |  598 | `	ph7_result_double(pCtx,r);` |
|    3 |  599 | `	return PH7_OK;` |
|    5 |  600 |  |
|    - |  601 | `/*` |
|    - |  602 | ` * float hypot(float $x,float $y)` |
|    - |  603 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|    - |  604 | ` * Parameters` |
|    - |  605 | ` * $x` |
|    - |  606 | ` *  Length of first side` |
|    - |  607 | ` * $y` |
|    - |  608 | ` *  Length of first side` |
|    - |  609 | ` * Return` |
|    - |  610 | ` *  Calculated length of the hypotenuse.` |
|    - |  611 | ` */` |
|    6 |  612 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  613 |  |
|    - |  614 | `	double x,y,r;` |
|    7 |  615 | `	if( nArg < 2 ){` |
|    - |  616 | `		/* Missing arguments */` |
|    5 |  617 | `		ph7_result_double(pCtx,0);` |
|    5 |  618 | `		return PH7_OK;` |
|    - |  619 | `	}` |
|    - |  620 | `	/* Extract given arguments */` |
|    3 |  621 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  622 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  623 | `	/* Perform the requested operation */` |
|    3 |  624 | `	r = hypot(x,y);` |
|    - |  625 | `	/* Processing result */` |
|    3 |  626 | `	ph7_result_double(pCtx,r);` |
|    3 |  627 | `	return PH7_OK;` |
|    4 |  628 |  |
|    - |  629 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|    - |  630 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  631 | `/*` |
|    - |  632 | ` * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|    - |  633 | ` *  Exponential expression.` |
|    - |  634 | ` * Parameter` |
|    - |  635 | ` *  $val` |
|    - |  636 | ` *   The value to round.` |
|    - |  637 | ` * $precision` |
|    - |  638 | ` *   The optional number of decimal digits to round to.` |
|    - |  639 | ` * $mode` |
|    - |  640 | ` *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.` |
|    - |  641 | ` *   (not supported).` |
|    - |  642 | ` * Return` |
|    - |  643 | ` *  The rounded value.` |
|    - |  644 | ` */` |
|   20 |  645 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  646 |  |
|   21 |  647 | `	int n = 0;` |
|    - |  648 | `	double r;` |
|   21 |  649 | `	if( nArg < 1 ){` |
|    - |  650 | `		/* Missing argument,return 0 */` |
|    5 |  651 | `		ph7_result_int(pCtx,0);` |
|    5 |  652 | `		return PH7_OK;` |
|    - |  653 | `	}` |
|    - |  654 | `	/* Extract the precision if available */` |
|   17 |  655 | `	if( nArg > 1 ){` |
|    5 |  656 | `		n = ph7_value_to_int(apArg[1]);` |
|    5 |  657 | `		if( n>30 ){` |
|    3 |  658 | `			n = 30;` |
|    1 |  659 | `		}` |
|    5 |  660 | `		if( n<0 ){` |
|    3 |  661 | `			n = 0;` |
|    1 |  662 | `		}` |
|    2 |  663 | `	}` |
|   17 |  664 | `	r = ph7_value_to_double(apArg[0]);` |
|    - |  665 | `	/* If Y==0 and X will fit in a 64-bit int,` |
|    - |  666 | `     * handle the rounding directly.Otherwise` |
|    - |  667 | `	 * use our own cutsom printf [i.e:SyBufferFormat()].` |
|    - |  668 | `     */` |
|   17 |  669 | `	if( n==0 && r>=0 && r < (double)(LARGEST_INT64-1) ){` |
|   13 |  670 | `    r = (double)((ph7_int64)(r+0.5));` |
|   11 |  671 | `	}else if( n==0 && r<0 && (-r) < (double)(LARGEST_INT64-1) ){` |
|    3 |  672 | `    r = -(double)((ph7_int64)((-r)+0.5));` |
|    2 |  673 | `  }else{` |
|    - |  674 | `	  char zBuf[256];` |
|    - |  675 | `	  sxu32 nLen;` |
|    3 |  676 | `	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);` |
|    - |  677 | `	  /* Convert the string to real number */` |
|    3 |  678 | `	  SyStrToReal(zBuf,nLen,(void *)&r,0);` |
|    - |  679 | `  }` |
|    - |  680 | `  /* Return thr rounded value */` |
|   17 |  681 | `  ph7_result_double(pCtx,r);` |
|   17 |  682 | `  return PH7_OK;` |
|   11 |  683 |  |
|    - |  684 | `/*` |
|    - |  685 | ` * string dechex(int $number)` |
|    - |  686 | ` *  Decimal to hexadecimal.` |
|    - |  687 | ` * Parameters` |
|    - |  688 | ` *  $number` |
|    - |  689 | ` *   Decimal value to convert` |
|    - |  690 | ` * Return` |
|    - |  691 | ` *  Hexadecimal string representation of number` |
|    - |  692 | ` */` |
|    6 |  693 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  694 |  |
|    - |  695 | `	int iVal;` |
|    7 |  696 | `	if( nArg < 1 ){` |
|    - |  697 | `		/* Missing arguments,return null */` |
|    5 |  698 | `		ph7_result_null(pCtx);` |
|    5 |  699 | `		return PH7_OK;` |
|    - |  700 | `	}` |
|    - |  701 | `	/* Extract the given number */` |
|    3 |  702 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  703 | `	/* Format */` |
|    3 |  704 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|    3 |  705 | `	return PH7_OK;` |
|    4 |  706 |  |
|    - |  707 | `/*` |
|    - |  708 | ` * string decoct(int $number)` |
|    - |  709 | ` *  Decimal to Octal.` |
|    - |  710 | ` * Parameters` |
|    - |  711 | ` *  $number` |
|    - |  712 | ` *   Decimal value to convert` |
|    - |  713 | ` * Return` |
|    - |  714 | ` *  Octal string representation of number` |
|    - |  715 | ` */` |
|    8 |  716 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  717 |  |
|    - |  718 | `	int iVal;` |
|    9 |  719 | `	if( nArg < 1 ){` |
|    - |  720 | `		/* Missing arguments,return null */` |
|    3 |  721 | `		ph7_result_null(pCtx);` |
|    3 |  722 | `		return PH7_OK;` |
|    - |  723 | `	}` |
|    - |  724 | `	/* Extract the given number */` |
|    7 |  725 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  726 | `	/* Format */` |
|    7 |  727 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|    7 |  728 | `	return PH7_OK;` |
|    5 |  729 |  |
|    - |  730 | `/*` |
|    - |  731 | ` * string decbin(int $number)` |
|    - |  732 | ` *  Decimal to binary.` |
|    - |  733 | ` * Parameters` |
|    - |  734 | ` *  $number` |
|    - |  735 | ` *   Decimal value to convert` |
|    - |  736 | ` * Return` |
|    - |  737 | ` *  Binary string representation of number` |
|    - |  738 | ` */` |
|    4 |  739 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  740 |  |
|    - |  741 | `	int iVal;` |
|    5 |  742 | `	if( nArg < 1 ){` |
|    - |  743 | `		/* Missing arguments,return null */` |
|    3 |  744 | `		ph7_result_null(pCtx);` |
|    3 |  745 | `		return PH7_OK;` |
|    - |  746 | `	}` |
|    - |  747 | `	/* Extract the given number */` |
|    3 |  748 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  749 | `	/* Format */` |
|    3 |  750 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|    3 |  751 | `	return PH7_OK;` |
|    3 |  752 |  |
|    - |  753 | `/*` |
|    - |  754 | ` * int64 hexdec(string $hex_string)` |
|    - |  755 | ` *  Hexadecimal to decimal.` |
|    - |  756 | ` * Parameters` |
|    - |  757 | ` *  $hex_string` |
|    - |  758 | ` *   The hexadecimal string to convert` |
|    - |  759 | ` * Return` |
|    - |  760 | ` *  The decimal representation of hex_string` |
|    - |  761 | ` */` |
|   24 |  762 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  763 |  |
|    - |  764 | `	const char *zString,*zEnd;` |
|    - |  765 | `	ph7_int64 iVal;` |
|    - |  766 | `	int nLen;` |
|   25 |  767 | `	if( nArg < 1 ){` |
|    - |  768 | `		/* Missing arguments,return -1 */` |
|    5 |  769 | `		ph7_result_int(pCtx,-1);` |
|    5 |  770 | `		return PH7_OK;` |
|    - |  771 | `	}` |
|   21 |  772 | `	iVal = 0;` |
|   21 |  773 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  774 | `		/* Extract the given string */` |
|   15 |  775 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    - |  776 | `		/* Delimit the string */` |
|   15 |  777 | `		zEnd = &zString[nLen];` |
|    - |  778 | `		/* Ignore non hex-stream */` |
|   21 |  779 | `		while( zString < zEnd ){` |
|   21 |  780 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|    - |  781 | `				/* UTF-8 stream */` |
|    5 |  782 | `				zString++;` |
|    9 |  783 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|    5 |  784 | `					zString++;` |
|    1 |  785 | `				}` |
|    3 |  786 | `			}else{` |
|   17 |  787 | `				if( SyisHex(zString[0]) ){` |
|   15 |  788 | `					break;` |
|    - |  789 | `				}` |
|    - |  790 | `				/* Ignore */` |
|    3 |  791 | `				zString++;` |
|    - |  792 | `			}` |
|    1 |  793 | `		}` |
|   15 |  794 | `		if( zString < zEnd ){` |
|    - |  795 | `			/* Cast */` |
|   15 |  796 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|    7 |  797 | `		}` |
|    8 |  798 | `	}else{` |
|    - |  799 | `		/* Extract as a 64-bit integer */` |
|    7 |  800 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  801 | `	}` |
|    - |  802 | `	/* Return the number */` |
|   21 |  803 | `	ph7_result_int64(pCtx,iVal);` |
|   21 |  804 | `	return PH7_OK;` |
|   13 |  805 |  |
|    - |  806 | `/*` |
|    - |  807 | ` * int64 bindec(string $bin_string)` |
|    - |  808 | ` *  Binary to decimal.` |
|    - |  809 | ` * Parameters` |
|    - |  810 | ` *  $bin_string` |
|    - |  811 | ` *   The binary string to convert` |
|    - |  812 | ` * Return` |
|    - |  813 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|    - |  814 | ` */` |
|   12 |  815 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  816 |  |
|    - |  817 | `	const char *zString;` |
|    - |  818 | `	ph7_int64 iVal;` |
|    - |  819 | `	int nLen;` |
|   13 |  820 | `	if( nArg < 1 ){` |
|    - |  821 | `		/* Missing arguments,return -1 */` |
|    5 |  822 | `		ph7_result_int(pCtx,-1);` |
|    5 |  823 | `		return PH7_OK;` |
|    - |  824 | `	}` |
|    9 |  825 | `	iVal = 0;` |
|    9 |  826 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  827 | `		/* Extract the given string */` |
|    7 |  828 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    7 |  829 | `		if( nLen > 0 ){` |
|    - |  830 | `			/* Perform a binary cast */` |
|    5 |  831 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    2 |  832 | `		}` |
|    4 |  833 | `	}else{` |
|    - |  834 | `		/* Extract as a 64-bit integer */` |
|    3 |  835 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  836 | `	}` |
|    - |  837 | `	/* Return the number */` |
|    9 |  838 | `	ph7_result_int64(pCtx,iVal);` |
|    9 |  839 | `	return PH7_OK;` |
|    7 |  840 |  |
|    - |  841 | `/*` |
|    - |  842 | ` * int64 octdec(string $oct_string)` |
|    - |  843 | ` *  Octal to decimal.` |
|    - |  844 | ` * Parameters` |
|    - |  845 | ` *  $oct_string` |
|    - |  846 | ` *   The octal string to convert` |
|    - |  847 | ` * Return` |
|    - |  848 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|    - |  849 | ` */` |
|    6 |  850 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  851 |  |
|    - |  852 | `	const char *zString;` |
|    - |  853 | `	ph7_int64 iVal;` |
|    - |  854 | `	int nLen;` |
|    7 |  855 | `	if( nArg < 1 ){` |
|    - |  856 | `		/* Missing arguments,return -1 */` |
|    3 |  857 | `		ph7_result_int(pCtx,-1);` |
|    3 |  858 | `		return PH7_OK;` |
|    - |  859 | `	}` |
|    5 |  860 | `	iVal = 0;` |
|    5 |  861 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  862 | `		/* Extract the given string */` |
|    3 |  863 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    3 |  864 | `		if( nLen > 0 ){` |
|    - |  865 | `			/* Perform the cast */` |
|    3 |  866 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    1 |  867 | `		}` |
|    2 |  868 | `	}else{` |
|    - |  869 | `		/* Extract as a 64-bit integer */` |
|    3 |  870 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  871 | `	}` |
|    - |  872 | `	/* Return the number */` |
|    5 |  873 | `	ph7_result_int64(pCtx,iVal);` |
|    5 |  874 | `	return PH7_OK;` |
|    4 |  875 |  |
|    - |  876 | `/*` |
|    - |  877 | ` * srand([int $seed])` |
|    - |  878 | ` * mt_srand([int $seed])` |
|    - |  879 | ` *  Seed the random number generator.` |
|    - |  880 | ` * Parameters` |
|    - |  881 | ` * $seed` |
|    - |  882 | ` *  Optional seed value` |
|    - |  883 | ` * Return` |
|    - |  884 | ` *  null.` |
|    - |  885 | ` * Note:` |
|    - |  886 | ` *  THIS FUNCTION IS A NO-OP.` |
|    - |  887 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|    - |  888 | ` */` |
|   20 |  889 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  890 |  |
|   10 |  891 | `	SXUNUSED(nArg);` |
|   10 |  892 | `	SXUNUSED(apArg);` |
|   21 |  893 | `	ph7_result_null(pCtx);` |
|   21 |  894 | `	return PH7_OK;` |
|    1 |  895 |  |
|    - |  896 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - |  897 | `/*` |
|    - |  898 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|    - |  899 | ` *  Convert a number between arbitrary bases.` |
|    - |  900 | ` * Parameters` |
|    - |  901 | ` * $number` |
|    - |  902 | ` *  The number to convert` |
|    - |  903 | ` * $frombase` |
|    - |  904 | ` *  The base number is in` |
|    - |  905 | ` * $tobase` |
|    - |  906 | ` *  The base to convert number to` |
|    - |  907 | ` * Return` |
|    - |  908 | ` *  Number converted to base tobase` |
|    - |  909 | ` */` |
|   48 |  910 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  911 |  |
|    - |  912 | `	int nLen,iFbase,iTobase;` |
|    - |  913 | `	const char *zNum;` |
|    - |  914 | `	ph7_int64 iNum;` |
|   49 |  915 | `	if( nArg < 3 ){` |
|    - |  916 | `		/* Return the empty string*/` |
|   13 |  917 | `		ph7_result_string(pCtx,"",0);` |
|   13 |  918 | `		return PH7_OK;` |
|    - |  919 | `	}` |
|    - |  920 | `	/* Base numbers */` |
|   37 |  921 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|   37 |  922 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|   37 |  923 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  924 | `		/* Extract the target number */` |
|   33 |  925 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|   33 |  926 | `		if( nLen < 1 ){` |
|    - |  927 | `			/* Return the empty string*/` |
|    5 |  928 | `			ph7_result_string(pCtx,"",0);` |
|    5 |  929 | `			return PH7_OK;` |
|    - |  930 | `		}` |
|    - |  931 | `		/* Base conversion */` |
|   29 |  932 | `		switch(iFbase){` |
|    5 |  933 | `		case 16:` |
|    - |  934 | `			/* Hex */` |
|   11 |  935 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|   11 |  936 | `			break;` |
|    3 |  937 | `		case 8:` |
|    - |  938 | `			/* Octal */` |
|    7 |  939 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    7 |  940 | `			break;` |
|    2 |  941 | `		case 2:` |
|    - |  942 | `			/* Binary */` |
|    5 |  943 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    5 |  944 | `			break;` |
|    4 |  945 | `		default:` |
|    - |  946 | `			/* Decimal */` |
|    9 |  947 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    8 |  948 | `			break;` |
|    - |  949 | `		}` |
|   15 |  950 | `	}else{` |
|    5 |  951 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|    - |  952 | `	}` |
|   33 |  953 | `	switch(iTobase){` |
|    3 |  954 | `	case 16:` |
|    - |  955 | `		/* Hex */` |
|    7 |  956 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|    7 |  957 | `		break;` |
|    1 |  958 | `	case 8:` |
|    - |  959 | `		/* Octal */` |
|    3 |  960 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|    3 |  961 | `		break;` |
|    1 |  962 | `	case 2:` |
|    - |  963 | `		/* Binary */` |
|    3 |  964 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|    3 |  965 | `		break;` |
|   11 |  966 | `	default:` |
|    - |  967 | `		/* Decimal */` |
|   23 |  968 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|   22 |  969 | `		break;` |
|    - |  970 | `	}` |
|   33 |  971 | `	return PH7_OK;` |
|   25 |  972 |  |
|    - |  973 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - |  974 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  975 |  |
