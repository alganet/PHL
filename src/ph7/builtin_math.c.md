# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 452/467 lines (96.79%)

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
|   22 |  237 | `PH7_PRIVATE int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  238 |  |
|    - |  239 | `	double r, x;` |
|    - |  240 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|   24 |  241 | `	if( nArg != 1 ){` |
|    4 |  242 | `		return PH7_VmThrowException(pCtx,` |
|    - |  243 | `			"ArgumentCountError",` |
|    - |  244 | `			"asin() expects exactly 1 argument, %d given",` |
|    1 |  245 | `			nArg` |
|    - |  246 | `			);` |
|    - |  247 | `	}` |
|    - |  248 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|    - |  249 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|    - |  250 | `	 * the float conversion will handle them. */` |
|   22 |  251 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|    7 |  252 | `		return PH7_VmThrowException(pCtx,` |
|    - |  253 | `			"TypeError",` |
|    - |  254 | `			"asin(): Argument #1 ($num) must be of type float, %s given",` |
|    2 |  255 | `			ph7_type_name(apArg[0])` |
|    - |  256 | `			);` |
|    - |  257 | `	}` |
|    - |  258 | `	/* Convert to double now that we know it's numeric. */` |
|   17 |  259 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  260 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|   17 |  261 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|    5 |  262 | `		r = PH7_NAN_VALUE();` |
|    3 |  263 | `	}else{` |
|   13 |  264 | `		r = asin(x);` |
|    - |  265 | `	}` |
|    - |  266 | `	/* store the result back */` |
|   17 |  267 | `	ph7_result_double(pCtx,r);` |
|   17 |  268 | `	return PH7_OK;` |
|   13 |  269 |  |
|    - |  270 | `/*` |
|    - |  271 | ` * float sinh(float $arg )` |
|    - |  272 | ` *  Hyperbolic sine.` |
|    - |  273 | ` * Parameter` |
|    - |  274 | ` *  The number to process.` |
|    - |  275 | ` * Return` |
|    - |  276 | ` *  The hyperbolic sine of arg.` |
|    - |  277 | ` */` |
|   20 |  278 | `PH7_PRIVATE int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  279 |  |
|    - |  280 | `	double r,x;` |
|   21 |  281 | `	if( nArg < 1 ){` |
|    - |  282 | `		/* Missing argument,return 0 */` |
|    3 |  283 | `		ph7_result_int(pCtx,0);` |
|    3 |  284 | `		return PH7_OK;` |
|    - |  285 | `	}` |
|   19 |  286 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  287 | `	/* Perform the requested operation */` |
|   19 |  288 | `	r = sinh(x);` |
|    - |  289 | `	/* store the result back */` |
|   19 |  290 | `	ph7_result_double(pCtx,r);` |
|   19 |  291 | `	return PH7_OK;` |
|   11 |  292 |  |
|    - |  293 | `/*` |
|    - |  294 | ` * float ceil(float $arg )` |
|    - |  295 | ` *  Round fractions up.` |
|    - |  296 | ` * Parameter` |
|    - |  297 | ` *  The number to process.` |
|    - |  298 | ` * Return` |
|    - |  299 | ` *  The next highest integer value by rounding up value if necessary.` |
|    - |  300 | ` */` |
|   14 |  301 | `PH7_PRIVATE int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  302 |  |
|    - |  303 | `	double r,x;` |
|    - |  304 | `	/* PHP requires exactly one argument. */` |
|   16 |  305 | `	if( nArg != 1 ){` |
|    7 |  306 | `		return PH7_VmThrowException(pCtx,` |
|    - |  307 | `			"ArgumentCountError",` |
|    - |  308 | `			"ceil() expects exactly 1 argument, %d given",` |
|    2 |  309 | `			nArg` |
|    - |  310 | `			);` |
|    - |  311 | `	}` |
|    - |  312 | `	/*` |
|    - |  313 | `	 * PHP only accepts ints, floats or numeric strings.  Any other types` |
|    - |  314 | `	 * (in particular non-numeric strings) should raise a TypeError.  We` |
|    - |  315 | `	 * mimic the approach used by abs() and perform an explicit numeric` |
|    - |  316 | `	 * check on strings before converting to double.` |
|    - |  317 | `	 */` |
|   12 |  318 | `	if( !ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]) ){` |
|    6 |  319 | `		if( ph7_value_is_string(apArg[0]) ){` |
|    - |  320 | `			int len;` |
|    6 |  321 | `			sxu8 bReal = FALSE;` |
|    6 |  322 | `			const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  323 | `			sxi32 rcNum;` |
|    6 |  324 | `			rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|    6 |  325 | `			if( rcNum != SXRET_OK ){` |
|    3 |  326 | `				return PH7_VmThrowException(pCtx,` |
|    - |  327 | `					"TypeError",` |
|    - |  328 | `					"ceil(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  329 | `					);` |
|    - |  330 | `			}` |
|    2 |  331 | `		}else{` |
|    - |  332 | `			/* Reject arrays, objects, resources, booleans, NULL, etc. */` |
|  ! 0 |  333 | `			return PH7_VmThrowException(pCtx,` |
|    - |  334 | `				"TypeError",` |
|    - |  335 | `				"ceil(): Argument #1 ($num) must be of type int\|float"` |
|    - |  336 | `				);` |
|    - |  337 | `		}` |
|    1 |  338 | `	}` |
|    - |  339 |  |
|    9 |  340 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  341 | `	/* Perform the requested operation */` |
|    9 |  342 | `	r = ceil(x);` |
|    - |  343 | `	/* store the result back */` |
|    9 |  344 | `	ph7_result_double(pCtx,r);` |
|    9 |  345 | `	return PH7_OK;` |
|    9 |  346 |  |
|    - |  347 | `/*` |
|    - |  348 | ` * float tan(float $arg )` |
|    - |  349 | ` *  Tangent.` |
|    - |  350 | ` * Parameter` |
|    - |  351 | ` *  The number to process.` |
|    - |  352 | ` * Return` |
|    - |  353 | ` *  The tangent of arg.` |
|    - |  354 | ` */` |
|    6 |  355 | `PH7_PRIVATE int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  356 |  |
|    - |  357 | `	double r,x;` |
|    7 |  358 | `	if( nArg < 1 ){` |
|    - |  359 | `		/* Missing argument,return 0 */` |
|    3 |  360 | `		ph7_result_int(pCtx,0);` |
|    3 |  361 | `		return PH7_OK;` |
|    - |  362 | `	}` |
|    5 |  363 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  364 | `	/* Perform the requested operation */` |
|    5 |  365 | `	r = tan(x);` |
|    - |  366 | `	/* store the result back */` |
|    5 |  367 | `	ph7_result_double(pCtx,r);` |
|    5 |  368 | `	return PH7_OK;` |
|    4 |  369 |  |
|    - |  370 | `/*` |
|    - |  371 | ` * float atan(float $arg )` |
|    - |  372 | ` *  Arc tangent.` |
|    - |  373 | ` * Parameter` |
|    - |  374 | ` *  The number to process.` |
|    - |  375 | ` * Return` |
|    - |  376 | ` *  The arc tangent of arg.` |
|    - |  377 | ` */` |
|   46 |  378 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  379 |  |
|    - |  380 | `	double r,x;` |
|    - |  381 | `	/* PHP enforces exactly one argument. */` |
|   48 |  382 | `	if( nArg != 1 ){` |
|   11 |  383 | `		return PH7_VmThrowException(pCtx,` |
|    - |  384 | `			"ArgumentCountError",` |
|    - |  385 | `			"atan() expects exactly 1 argument, %d given",` |
|    3 |  386 | `			nArg` |
|    - |  387 | `			);` |
|    - |  388 | `	}` |
|    - |  389 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, non-numeric strings).` |
|    - |  390 | `	 * PHP 8 reports a TypeError for wrong types. */` |
|   42 |  391 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|   14 |  392 | `		return PH7_VmThrowException(pCtx,` |
|    - |  393 | `			"TypeError",` |
|    - |  394 | `			"atan(): Argument #1 ($num) must be of type float, %s given",` |
|    4 |  395 | `			ph7_type_name(apArg[0])` |
|    - |  396 | `			);` |
|    - |  397 | `	}` |
|   33 |  398 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  399 | `	/* Perform the requested operation */` |
|   33 |  400 | `	r = atan(x);` |
|    - |  401 | `	/* store the result back */` |
|   33 |  402 | `	ph7_result_double(pCtx,r);` |
|   33 |  403 | `	return PH7_OK;` |
|   25 |  404 |  |
|    - |  405 | `/*` |
|    - |  406 | ` * float tanh(float $arg )` |
|    - |  407 | ` *  Hyperbolic tangent.` |
|    - |  408 | ` * Parameter` |
|    - |  409 | ` *  The number to process.` |
|    - |  410 | ` * Return` |
|    - |  411 | ` *  The Hyperbolic tangent of arg.` |
|    - |  412 | ` */` |
|   20 |  413 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  414 |  |
|    - |  415 | `	double r,x;` |
|   21 |  416 | `	if( nArg < 1 ){` |
|    - |  417 | `		/* Missing argument,return 0 */` |
|    3 |  418 | `		ph7_result_int(pCtx,0);` |
|    3 |  419 | `		return PH7_OK;` |
|    - |  420 | `	}` |
|   19 |  421 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  422 | `	/* Perform the requested operation */` |
|   19 |  423 | `	r = tanh(x);` |
|    - |  424 | `	/* store the result back */` |
|   19 |  425 | `	ph7_result_double(pCtx,r);` |
|   19 |  426 | `	return PH7_OK;` |
|   11 |  427 |  |
|    - |  428 | `/*` |
|    - |  429 | ` * float atan2(float $y,float $x)` |
|    - |  430 | ` *  Arc tangent of two variable.` |
|    - |  431 | ` * Parameter` |
|    - |  432 | ` *  $y = Dividend parameter.` |
|    - |  433 | ` *  $x = Divisor parameter.` |
|    - |  434 | ` * Return` |
|    - |  435 | ` *  The arc tangent of y/x in radian.` |
|    - |  436 | ` */` |
|   60 |  437 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  438 |  |
|    - |  439 | `	double r,x,y;` |
|    - |  440 | `	/* PHP enforces exactly two arguments. */` |
|   62 |  441 | `	if( nArg != 2 ){` |
|   10 |  442 | `		return PH7_VmThrowException(pCtx,` |
|    - |  443 | `			"ArgumentCountError",` |
|    - |  444 | `			"atan2() expects exactly 2 arguments, %d given",` |
|    3 |  445 | `			nArg` |
|    - |  446 | `			);` |
|    - |  447 | `	}` |
|    - |  448 | `	/* Type checking: reject non-numeric values for $y (argument #1). */` |
|   56 |  449 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|   10 |  450 | `		return PH7_VmThrowException(pCtx,` |
|    - |  451 | `			"TypeError",` |
|    - |  452 | `			"atan2(): Argument #1 ($y) must be of type float, %s given",` |
|    3 |  453 | `			ph7_type_name(apArg[0])` |
|    - |  454 | `			);` |
|    - |  455 | `	}` |
|    - |  456 | `	/* Type checking: reject non-numeric values for $x (argument #2). */` |
|   50 |  457 | `	if( !ph7_value_is_numeric(apArg[1]) ){` |
|    4 |  458 | `		return PH7_VmThrowException(pCtx,` |
|    - |  459 | `			"TypeError",` |
|    - |  460 | `			"atan2(): Argument #2 ($x) must be of type float, %s given",` |
|    2 |  461 | `			ph7_type_name(apArg[1])` |
|    - |  462 | `			);` |
|    - |  463 | `	}` |
|   47 |  464 | `	y = ph7_value_to_double(apArg[0]);` |
|   47 |  465 | `	x = ph7_value_to_double(apArg[1]);` |
|    - |  466 | `	/* Perform the requested operation */` |
|   47 |  467 | `	r = atan2(y,x);` |
|    - |  468 | `	/* store the result back */` |
|   47 |  469 | `	ph7_result_double(pCtx,r);` |
|   47 |  470 | `	return PH7_OK;` |
|   32 |  471 |  |
|    - |  472 | `/*` |
|    - |  473 | ` * float/int64 abs(float/int64 $arg )` |
|    - |  474 | ` *  Absolute value.` |
|    - |  475 | ` * Parameter` |
|    - |  476 | ` *  The number to process.` |
|    - |  477 | ` * Return` |
|    - |  478 | ` *  The absolute value of number.` |
|    - |  479 | ` */` |
|  126 |  480 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  481 |  |
|    - |  482 | `	int is_float;` |
|    - |  483 | `	/* PHP requires exactly one argument. */` |
|  128 |  484 | `	if( nArg != 1 ){` |
|   14 |  485 | `		return PH7_VmThrowException(pCtx,` |
|    - |  486 | `			"ArgumentCountError",` |
|    - |  487 | `			"abs() expects exactly 1 argument, %d given",` |
|    4 |  488 | `			nArg` |
|    - |  489 | `			);` |
|    - |  490 | `	}` |
|    - |  491 |  |
|    - |  492 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|  120 |  493 | `	is_float = ph7_value_is_float(apArg[0]);` |
|  120 |  494 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|    - |  495 | `		int len;` |
|   10 |  496 | `		sxu8 bReal = FALSE;` |
|   10 |  497 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  498 | `		sxi32 rcNum;` |
|   10 |  499 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|   10 |  500 | `		if( rcNum != SXRET_OK ){` |
|    3 |  501 | `			return PH7_VmThrowException(pCtx,` |
|    - |  502 | `				"TypeError",` |
|    - |  503 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  504 | `				);` |
|    - |  505 | `		}` |
|    7 |  506 | `		if( bReal ){` |
|    5 |  507 | `			is_float = 1;` |
|    2 |  508 | `		}` |
|    3 |  509 | `	}` |
|  118 |  510 | `	if( is_float ){` |
|    - |  511 | `		double r,x;` |
|   99 |  512 | `		x = ph7_value_to_double(apArg[0]);` |
|    - |  513 | `		/* Perform the requested operation */` |
|   99 |  514 | `		r = fabs(x);` |
|   99 |  515 | `		ph7_result_double(pCtx,r);` |
|   50 |  516 | `	}else{` |
|    - |  517 | `		int r,x;` |
|   20 |  518 | `		x = ph7_value_to_int(apArg[0]);` |
|    - |  519 | `		/* Perform the requested operation */` |
|   20 |  520 | `		r = abs(x);` |
|   20 |  521 | `		ph7_result_int(pCtx,r);` |
|    - |  522 | `	}` |
|  118 |  523 | `	return PH7_OK;` |
|   65 |  524 |  |
|    - |  525 | `/*` |
|    - |  526 | ` * float log(float $arg,[int/float $base])` |
|    - |  527 | ` *  Natural logarithm.` |
|    - |  528 | ` * Parameter` |
|    - |  529 | ` *  $arg: The number to process.` |
|    - |  530 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|    - |  531 | ` * Return` |
|    - |  532 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|    - |  533 | ` * Note:` |
|    - |  534 | ` *  only Natural log and base-10 log are supported.` |
|    - |  535 | ` */` |
|   14 |  536 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  537 |  |
|    - |  538 | `	double r,x;` |
|   15 |  539 | `	if( nArg < 1 ){` |
|    - |  540 | `		/* Missing argument,return 0 */` |
|    3 |  541 | `		ph7_result_int(pCtx,0);` |
|    3 |  542 | `		return PH7_OK;` |
|    - |  543 | `	}` |
|   13 |  544 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  545 | `	/* Perform the requested operation */` |
|   13 |  546 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|    - |  547 | `		/* Base-10 log */` |
|    5 |  548 | `		r = log10(x);` |
|    3 |  549 | `	}else{` |
|    9 |  550 | `		r = log(x);` |
|    - |  551 | `	}` |
|    - |  552 | `	/* store the result back */` |
|   13 |  553 | `	ph7_result_double(pCtx,r);` |
|   13 |  554 | `	return PH7_OK;` |
|    8 |  555 |  |
|    - |  556 | `/*` |
|    - |  557 | ` * float log10(float $arg )` |
|    - |  558 | ` *  Base-10 logarithm.` |
|    - |  559 | ` * Parameter` |
|    - |  560 | ` *  The number to process.` |
|    - |  561 | ` * Return` |
|    - |  562 | ` *  The Base-10 logarithm of the given number.` |
|    - |  563 | ` */` |
|   16 |  564 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  565 |  |
|    - |  566 | `	double r,x;` |
|   17 |  567 | `	if( nArg < 1 ){` |
|    - |  568 | `		/* Missing argument,return 0 */` |
|    3 |  569 | `		ph7_result_int(pCtx,0);` |
|    3 |  570 | `		return PH7_OK;` |
|    - |  571 | `	}` |
|   15 |  572 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  573 | `	/* Perform the requested operation */` |
|   15 |  574 | `	r = log10(x);` |
|    - |  575 | `	/* store the result back */` |
|   15 |  576 | `	ph7_result_double(pCtx,r);` |
|   15 |  577 | `	return PH7_OK;` |
|    9 |  578 |  |
|    - |  579 | `/*` |
|    - |  580 | ` * number pow(number $base,number $exp)` |
|    - |  581 | ` *  Exponential expression.` |
|    - |  582 | ` * Parameter` |
|    - |  583 | ` *  base` |
|    - |  584 | ` *  The base to use.` |
|    - |  585 | ` * exp` |
|    - |  586 | ` *  The exponent.` |
|    - |  587 | ` * Return` |
|    - |  588 | ` *  base raised to the power of exp.` |
|    - |  589 | ` *  If the result can be represented as integer it will be returned` |
|    - |  590 | ` *  as type integer, else it will be returned as type float.` |
|    - |  591 | ` */` |
|   10 |  592 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  593 |  |
|    - |  594 | `	double r,x,y;` |
|   11 |  595 | `	if( nArg < 1 ){` |
|    - |  596 | `		/* Missing argument,return 0 */` |
|    5 |  597 | `		ph7_result_int(pCtx,0);` |
|    5 |  598 | `		return PH7_OK;` |
|    - |  599 | `	}` |
|    7 |  600 | `	x = ph7_value_to_double(apArg[0]);` |
|    7 |  601 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  602 | `	/* Perform the requested operation */` |
|    7 |  603 | `	r = pow(x,y);` |
|    7 |  604 | `	ph7_result_double(pCtx,r);` |
|    7 |  605 | `	return PH7_OK;` |
|    6 |  606 |  |
|    - |  607 | `/*` |
|    - |  608 | ` * float pi(void)` |
|    - |  609 | ` *  Returns an approximation of pi.` |
|    - |  610 | ` * Note` |
|    - |  611 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|    - |  612 | ` * Return` |
|    - |  613 | ` *  The value of pi as float.` |
|    - |  614 | ` */` |
|    2 |  615 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  616 |  |
|    1 |  617 | `	SXUNUSED(nArg); /* cc warning */` |
|    1 |  618 | `	SXUNUSED(apArg);` |
|    3 |  619 | `	ph7_result_double(pCtx,PH7_PI);` |
|    3 |  620 | `	return PH7_OK;` |
|    1 |  621 |  |
|    - |  622 | `/*` |
|    - |  623 | ` * float fmod(float $x,float $y)` |
|    - |  624 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|    - |  625 | ` * Parameters` |
|    - |  626 | ` * $x` |
|    - |  627 | ` *  The dividend` |
|    - |  628 | ` * $y` |
|    - |  629 | ` *  The divisor` |
|    - |  630 | ` * Return` |
|    - |  631 | ` *  The floating point remainder of x/y.` |
|    - |  632 | ` */` |
|    8 |  633 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  634 |  |
|    - |  635 | `	double x,y,r;` |
|    9 |  636 | `	if( nArg < 2 ){` |
|    - |  637 | `		/* Missing arguments */` |
|    7 |  638 | `		ph7_result_double(pCtx,0);` |
|    7 |  639 | `		return PH7_OK;` |
|    - |  640 | `	}` |
|    - |  641 | `	/* Extract given arguments */` |
|    3 |  642 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  643 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  644 | `	/* Perform the requested operation */` |
|    3 |  645 | `	r = fmod(x,y);` |
|    - |  646 | `	/* Processing result */` |
|    3 |  647 | `	ph7_result_double(pCtx,r);` |
|    3 |  648 | `	return PH7_OK;` |
|    5 |  649 |  |
|    - |  650 | `/*` |
|    - |  651 | ` * float hypot(float $x,float $y)` |
|    - |  652 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|    - |  653 | ` * Parameters` |
|    - |  654 | ` * $x` |
|    - |  655 | ` *  Length of first side` |
|    - |  656 | ` * $y` |
|    - |  657 | ` *  Length of first side` |
|    - |  658 | ` * Return` |
|    - |  659 | ` *  Calculated length of the hypotenuse.` |
|    - |  660 | ` */` |
|    6 |  661 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  662 |  |
|    - |  663 | `	double x,y,r;` |
|    7 |  664 | `	if( nArg < 2 ){` |
|    - |  665 | `		/* Missing arguments */` |
|    5 |  666 | `		ph7_result_double(pCtx,0);` |
|    5 |  667 | `		return PH7_OK;` |
|    - |  668 | `	}` |
|    - |  669 | `	/* Extract given arguments */` |
|    3 |  670 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  671 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  672 | `	/* Perform the requested operation */` |
|    3 |  673 | `	r = hypot(x,y);` |
|    - |  674 | `	/* Processing result */` |
|    3 |  675 | `	ph7_result_double(pCtx,r);` |
|    3 |  676 | `	return PH7_OK;` |
|    4 |  677 |  |
|    - |  678 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|    - |  679 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  680 | `/*` |
|    - |  681 | ` * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|    - |  682 | ` *  Exponential expression.` |
|    - |  683 | ` * Parameter` |
|    - |  684 | ` *  $val` |
|    - |  685 | ` *   The value to round.` |
|    - |  686 | ` * $precision` |
|    - |  687 | ` *   The optional number of decimal digits to round to.` |
|    - |  688 | ` * $mode` |
|    - |  689 | ` *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.` |
|    - |  690 | ` *   (not supported).` |
|    - |  691 | ` * Return` |
|    - |  692 | ` *  The rounded value.` |
|    - |  693 | ` */` |
|   20 |  694 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  695 |  |
|   21 |  696 | `	int n = 0;` |
|    - |  697 | `	double r;` |
|   21 |  698 | `	if( nArg < 1 ){` |
|    - |  699 | `		/* Missing argument,return 0 */` |
|    5 |  700 | `		ph7_result_int(pCtx,0);` |
|    5 |  701 | `		return PH7_OK;` |
|    - |  702 | `	}` |
|    - |  703 | `	/* Extract the precision if available */` |
|   17 |  704 | `	if( nArg > 1 ){` |
|    5 |  705 | `		n = ph7_value_to_int(apArg[1]);` |
|    5 |  706 | `		if( n>30 ){` |
|    3 |  707 | `			n = 30;` |
|    1 |  708 | `		}` |
|    5 |  709 | `		if( n<0 ){` |
|    3 |  710 | `			n = 0;` |
|    1 |  711 | `		}` |
|    2 |  712 | `	}` |
|   17 |  713 | `	r = ph7_value_to_double(apArg[0]);` |
|    - |  714 | `	/* If Y==0 and X will fit in a 64-bit int,` |
|    - |  715 | `     * handle the rounding directly.Otherwise` |
|    - |  716 | `	 * use our own cutsom printf [i.e:SyBufferFormat()].` |
|    - |  717 | `     */` |
|   17 |  718 | `	if( n==0 && r>=0 && r < (double)(LARGEST_INT64-1) ){` |
|   13 |  719 | `    r = (double)((ph7_int64)(r+0.5));` |
|   11 |  720 | `	}else if( n==0 && r<0 && (-r) < (double)(LARGEST_INT64-1) ){` |
|    3 |  721 | `    r = -(double)((ph7_int64)((-r)+0.5));` |
|    2 |  722 | `  }else{` |
|    - |  723 | `	  char zBuf[256];` |
|    - |  724 | `	  sxu32 nLen;` |
|    3 |  725 | `	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);` |
|    - |  726 | `	  /* Convert the string to real number */` |
|    3 |  727 | `	  SyStrToReal(zBuf,nLen,(void *)&r,0);` |
|    - |  728 | `  }` |
|    - |  729 | `  /* Return thr rounded value */` |
|   17 |  730 | `  ph7_result_double(pCtx,r);` |
|   17 |  731 | `  return PH7_OK;` |
|   11 |  732 |  |
|    - |  733 | `/*` |
|    - |  734 | ` * int intdiv(int $a, int $b)` |
|    - |  735 | ` *  Integer division.` |
|    - |  736 | ` * Parameters` |
|    - |  737 | ` *  $a` |
|    - |  738 | ` *   Number to be divided.` |
|    - |  739 | ` *  $b` |
|    - |  740 | ` *   Number which divides the $a.` |
|    - |  741 | ` * Return` |
|    - |  742 | ` *  The integer quotient of the division of $a by $b.` |
|    - |  743 | ` */` |
|   20 |  744 | `PH7_PRIVATE int PH7_builtin_intdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  745 |  |
|    - |  746 | `	sxi64 a,b;` |
|    - |  747 | `	/* PHP requires exactly two arguments. */` |
|   22 |  748 | `	if( nArg != 2 ){` |
|    4 |  749 | `		return PH7_VmThrowException(pCtx,` |
|    - |  750 | `			"ArgumentCountError",` |
|    - |  751 | `			"intdiv() expects exactly 2 arguments, %d given",` |
|    1 |  752 | `			nArg` |
|    - |  753 | `			);` |
|    - |  754 | `	}` |
|    - |  755 | `	/* Type-check argument 1 */` |
|   18 |  756 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|   20 |  757 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 |  758 | `		return PH7_VmThrowException(pCtx,` |
|    - |  759 | `			"TypeError",` |
|    - |  760 | `			"intdiv(): Argument #1 ($num1) must be of type int, %s given",` |
|  ! 0 |  761 | `			ph7_type_name(apArg[0])` |
|    - |  762 | `			);` |
|    - |  763 | `	}` |
|   20 |  764 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  765 | `		int len;` |
|  ! 0 |  766 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|  ! 0 |  767 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 |  768 | `			return PH7_VmThrowException(pCtx,` |
|    - |  769 | `				"TypeError",` |
|    - |  770 | `				"intdiv(): Argument #1 ($num1) must be of type int, string given"` |
|    - |  771 | `				);` |
|    - |  772 | `		}` |
|  ! 0 |  773 | `	}` |
|    - |  774 | `	/* Type-check argument 2 */` |
|   18 |  775 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|   20 |  776 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|  ! 0 |  777 | `		return PH7_VmThrowException(pCtx,` |
|    - |  778 | `			"TypeError",` |
|    - |  779 | `			"intdiv(): Argument #2 ($num2) must be of type int, %s given",` |
|  ! 0 |  780 | `			ph7_type_name(apArg[1])` |
|    - |  781 | `			);` |
|    - |  782 | `	}` |
|   20 |  783 | `	if( ph7_value_is_string(apArg[1]) ){` |
|    - |  784 | `		int len;` |
|  ! 0 |  785 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|  ! 0 |  786 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|  ! 0 |  787 | `			return PH7_VmThrowException(pCtx,` |
|    - |  788 | `				"TypeError",` |
|    - |  789 | `				"intdiv(): Argument #2 ($num2) must be of type int, string given"` |
|    - |  790 | `				);` |
|    - |  791 | `		}` |
|  ! 0 |  792 | `	}` |
|    - |  793 | `	/* Convert both arguments to int64 */` |
|   20 |  794 | `	a = ph7_value_to_int64(apArg[0]);` |
|   20 |  795 | `	b = ph7_value_to_int64(apArg[1]);` |
|    - |  796 | `	/* Check for division by zero */` |
|   20 |  797 | `	if( b == 0 ){` |
|    3 |  798 | `		return PH7_VmThrowException(pCtx,` |
|    - |  799 | `			"DivisionByZeroError",` |
|    - |  800 | `			"Division by zero"` |
|    - |  801 | `			);` |
|    - |  802 | `	}` |
|    - |  803 | `	/* Check for overflow: PHP_INT_MIN / -1 */` |
|   18 |  804 | `	if( a == SMALLEST_INT64 && b == -1 ){` |
|    3 |  805 | `		return PH7_VmThrowException(pCtx,` |
|    - |  806 | `			"ArithmeticError",` |
|    - |  807 | `			"Division of PHP_INT_MIN by -1 is not an integer"` |
|    - |  808 | `			);` |
|    - |  809 | `	}` |
|    - |  810 | `	/* Perform integer division */` |
|   15 |  811 | `	ph7_result_int64(pCtx, a / b);` |
|   15 |  812 | `	return PH7_OK;` |
|   12 |  813 |  |
|    - |  814 | `/*` |
|    - |  815 | ` * string dechex(int $number)` |
|    - |  816 | ` *  Decimal to hexadecimal.` |
|    - |  817 | ` * Parameters` |
|    - |  818 | ` *  $number` |
|    - |  819 | ` *   Decimal value to convert` |
|    - |  820 | ` * Return` |
|    - |  821 | ` *  Hexadecimal string representation of number` |
|    - |  822 | ` */` |
|    8 |  823 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  824 |  |
|    - |  825 | `	int iVal;` |
|    9 |  826 | `	if( nArg < 1 ){` |
|    - |  827 | `		/* Missing arguments,return null */` |
|    5 |  828 | `		ph7_result_null(pCtx);` |
|    5 |  829 | `		return PH7_OK;` |
|    - |  830 | `	}` |
|    - |  831 | `	/* Extract the given number */` |
|    5 |  832 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  833 | `	/* Format */` |
|    5 |  834 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|    5 |  835 | `	return PH7_OK;` |
|    5 |  836 |  |
|    - |  837 | `/*` |
|    - |  838 | ` * string decoct(int $number)` |
|    - |  839 | ` *  Decimal to Octal.` |
|    - |  840 | ` * Parameters` |
|    - |  841 | ` *  $number` |
|    - |  842 | ` *   Decimal value to convert` |
|    - |  843 | ` * Return` |
|    - |  844 | ` *  Octal string representation of number` |
|    - |  845 | ` */` |
|    8 |  846 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  847 |  |
|    - |  848 | `	int iVal;` |
|    9 |  849 | `	if( nArg < 1 ){` |
|    - |  850 | `		/* Missing arguments,return null */` |
|    3 |  851 | `		ph7_result_null(pCtx);` |
|    3 |  852 | `		return PH7_OK;` |
|    - |  853 | `	}` |
|    - |  854 | `	/* Extract the given number */` |
|    7 |  855 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  856 | `	/* Format */` |
|    7 |  857 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|    7 |  858 | `	return PH7_OK;` |
|    5 |  859 |  |
|    - |  860 | `/*` |
|    - |  861 | ` * string decbin(int $number)` |
|    - |  862 | ` *  Decimal to binary.` |
|    - |  863 | ` * Parameters` |
|    - |  864 | ` *  $number` |
|    - |  865 | ` *   Decimal value to convert` |
|    - |  866 | ` * Return` |
|    - |  867 | ` *  Binary string representation of number` |
|    - |  868 | ` */` |
|    6 |  869 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  870 |  |
|    - |  871 | `	int iVal;` |
|    7 |  872 | `	if( nArg < 1 ){` |
|    - |  873 | `		/* Missing arguments,return null */` |
|    3 |  874 | `		ph7_result_null(pCtx);` |
|    3 |  875 | `		return PH7_OK;` |
|    - |  876 | `	}` |
|    - |  877 | `	/* Extract the given number */` |
|    5 |  878 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  879 | `	/* Format */` |
|    5 |  880 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|    5 |  881 | `	return PH7_OK;` |
|    4 |  882 |  |
|    - |  883 | `/*` |
|    - |  884 | ` * int64 hexdec(string $hex_string)` |
|    - |  885 | ` *  Hexadecimal to decimal.` |
|    - |  886 | ` * Parameters` |
|    - |  887 | ` *  $hex_string` |
|    - |  888 | ` *   The hexadecimal string to convert` |
|    - |  889 | ` * Return` |
|    - |  890 | ` *  The decimal representation of hex_string` |
|    - |  891 | ` */` |
|   24 |  892 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  893 |  |
|    - |  894 | `	const char *zString,*zEnd;` |
|    - |  895 | `	ph7_int64 iVal;` |
|    - |  896 | `	int nLen;` |
|   25 |  897 | `	if( nArg < 1 ){` |
|    - |  898 | `		/* Missing arguments,return -1 */` |
|    5 |  899 | `		ph7_result_int(pCtx,-1);` |
|    5 |  900 | `		return PH7_OK;` |
|    - |  901 | `	}` |
|   21 |  902 | `	iVal = 0;` |
|   21 |  903 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  904 | `		/* Extract the given string */` |
|   15 |  905 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    - |  906 | `		/* Delimit the string */` |
|   15 |  907 | `		zEnd = &zString[nLen];` |
|    - |  908 | `		/* Ignore non hex-stream */` |
|   21 |  909 | `		while( zString < zEnd ){` |
|   21 |  910 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|    - |  911 | `				/* UTF-8 stream */` |
|    5 |  912 | `				zString++;` |
|    9 |  913 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|    5 |  914 | `					zString++;` |
|    1 |  915 | `				}` |
|    3 |  916 | `			}else{` |
|   17 |  917 | `				if( SyisHex(zString[0]) ){` |
|   15 |  918 | `					break;` |
|    - |  919 | `				}` |
|    - |  920 | `				/* Ignore */` |
|    3 |  921 | `				zString++;` |
|    - |  922 | `			}` |
|    1 |  923 | `		}` |
|   15 |  924 | `		if( zString < zEnd ){` |
|    - |  925 | `			/* Cast */` |
|   15 |  926 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|    7 |  927 | `		}` |
|    8 |  928 | `	}else{` |
|    - |  929 | `		/* Extract as a 64-bit integer */` |
|    7 |  930 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  931 | `	}` |
|    - |  932 | `	/* Return the number */` |
|   21 |  933 | `	ph7_result_int64(pCtx,iVal);` |
|   21 |  934 | `	return PH7_OK;` |
|   13 |  935 |  |
|    - |  936 | `/*` |
|    - |  937 | ` * int64 bindec(string $bin_string)` |
|    - |  938 | ` *  Binary to decimal.` |
|    - |  939 | ` * Parameters` |
|    - |  940 | ` *  $bin_string` |
|    - |  941 | ` *   The binary string to convert` |
|    - |  942 | ` * Return` |
|    - |  943 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|    - |  944 | ` */` |
|   14 |  945 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  946 |  |
|    - |  947 | `	const char *zString;` |
|    - |  948 | `	ph7_int64 iVal;` |
|    - |  949 | `	int nLen;` |
|   15 |  950 | `	if( nArg < 1 ){` |
|    - |  951 | `		/* Missing arguments,return -1 */` |
|    5 |  952 | `		ph7_result_int(pCtx,-1);` |
|    5 |  953 | `		return PH7_OK;` |
|    - |  954 | `	}` |
|   11 |  955 | `	iVal = 0;` |
|   11 |  956 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  957 | `		/* Extract the given string */` |
|    9 |  958 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    9 |  959 | `		if( nLen > 0 ){` |
|    - |  960 | `			/* Perform a binary cast */` |
|    7 |  961 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    3 |  962 | `		}` |
|    5 |  963 | `	}else{` |
|    - |  964 | `		/* Extract as a 64-bit integer */` |
|    3 |  965 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  966 | `	}` |
|    - |  967 | `	/* Return the number */` |
|   11 |  968 | `	ph7_result_int64(pCtx,iVal);` |
|   11 |  969 | `	return PH7_OK;` |
|    8 |  970 |  |
|    - |  971 | `/*` |
|    - |  972 | ` * int64 octdec(string $oct_string)` |
|    - |  973 | ` *  Octal to decimal.` |
|    - |  974 | ` * Parameters` |
|    - |  975 | ` *  $oct_string` |
|    - |  976 | ` *   The octal string to convert` |
|    - |  977 | ` * Return` |
|    - |  978 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|    - |  979 | ` */` |
|    6 |  980 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  981 |  |
|    - |  982 | `	const char *zString;` |
|    - |  983 | `	ph7_int64 iVal;` |
|    - |  984 | `	int nLen;` |
|    7 |  985 | `	if( nArg < 1 ){` |
|    - |  986 | `		/* Missing arguments,return -1 */` |
|    3 |  987 | `		ph7_result_int(pCtx,-1);` |
|    3 |  988 | `		return PH7_OK;` |
|    - |  989 | `	}` |
|    5 |  990 | `	iVal = 0;` |
|    5 |  991 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  992 | `		/* Extract the given string */` |
|    3 |  993 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    3 |  994 | `		if( nLen > 0 ){` |
|    - |  995 | `			/* Perform the cast */` |
|    3 |  996 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    1 |  997 | `		}` |
|    2 |  998 | `	}else{` |
|    - |  999 | `		/* Extract as a 64-bit integer */` |
|    3 | 1000 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - | 1001 | `	}` |
|    - | 1002 | `	/* Return the number */` |
|    5 | 1003 | `	ph7_result_int64(pCtx,iVal);` |
|    5 | 1004 | `	return PH7_OK;` |
|    4 | 1005 |  |
|    - | 1006 | `/*` |
|    - | 1007 | ` * srand([int $seed])` |
|    - | 1008 | ` * mt_srand([int $seed])` |
|    - | 1009 | ` *  Seed the random number generator.` |
|    - | 1010 | ` * Parameters` |
|    - | 1011 | ` * $seed` |
|    - | 1012 | ` *  Optional seed value` |
|    - | 1013 | ` * Return` |
|    - | 1014 | ` *  null.` |
|    - | 1015 | ` * Note:` |
|    - | 1016 | ` *  THIS FUNCTION IS A NO-OP.` |
|    - | 1017 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|    - | 1018 | ` */` |
|   20 | 1019 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1020 |  |
|   10 | 1021 | `	SXUNUSED(nArg);` |
|   10 | 1022 | `	SXUNUSED(apArg);` |
|   21 | 1023 | `	ph7_result_null(pCtx);` |
|   21 | 1024 | `	return PH7_OK;` |
|    1 | 1025 |  |
|    - | 1026 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - | 1027 | `/*` |
|    - | 1028 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|    - | 1029 | ` *  Convert a number between arbitrary bases.` |
|    - | 1030 | ` * Parameters` |
|    - | 1031 | ` * $number` |
|    - | 1032 | ` *  The number to convert` |
|    - | 1033 | ` * $frombase` |
|    - | 1034 | ` *  The base number is in` |
|    - | 1035 | ` * $tobase` |
|    - | 1036 | ` *  The base to convert number to` |
|    - | 1037 | ` * Return` |
|    - | 1038 | ` *  Number converted to base tobase` |
|    - | 1039 | ` */` |
|   48 | 1040 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1041 |  |
|    - | 1042 | `	int nLen,iFbase,iTobase;` |
|    - | 1043 | `	const char *zNum;` |
|    - | 1044 | `	ph7_int64 iNum;` |
|   49 | 1045 | `	if( nArg < 3 ){` |
|    - | 1046 | `		/* Return the empty string*/` |
|   13 | 1047 | `		ph7_result_string(pCtx,"",0);` |
|   13 | 1048 | `		return PH7_OK;` |
|    - | 1049 | `	}` |
|    - | 1050 | `	/* Base numbers */` |
|   37 | 1051 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|   37 | 1052 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|   37 | 1053 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - | 1054 | `		/* Extract the target number */` |
|   33 | 1055 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|   33 | 1056 | `		if( nLen < 1 ){` |
|    - | 1057 | `			/* Return the empty string*/` |
|    5 | 1058 | `			ph7_result_string(pCtx,"",0);` |
|    5 | 1059 | `			return PH7_OK;` |
|    - | 1060 | `		}` |
|    - | 1061 | `		/* Base conversion */` |
|   29 | 1062 | `		switch(iFbase){` |
|    5 | 1063 | `		case 16:` |
|    - | 1064 | `			/* Hex */` |
|   11 | 1065 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|   11 | 1066 | `			break;` |
|    3 | 1067 | `		case 8:` |
|    - | 1068 | `			/* Octal */` |
|    7 | 1069 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    7 | 1070 | `			break;` |
|    2 | 1071 | `		case 2:` |
|    - | 1072 | `			/* Binary */` |
|    5 | 1073 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    5 | 1074 | `			break;` |
|    4 | 1075 | `		default:` |
|    - | 1076 | `			/* Decimal */` |
|    9 | 1077 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    8 | 1078 | `			break;` |
|    - | 1079 | `		}` |
|   15 | 1080 | `	}else{` |
|    5 | 1081 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|    - | 1082 | `	}` |
|   33 | 1083 | `	switch(iTobase){` |
|    3 | 1084 | `	case 16:` |
|    - | 1085 | `		/* Hex */` |
|    7 | 1086 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|    7 | 1087 | `		break;` |
|    1 | 1088 | `	case 8:` |
|    - | 1089 | `		/* Octal */` |
|    3 | 1090 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|    3 | 1091 | `		break;` |
|    1 | 1092 | `	case 2:` |
|    - | 1093 | `		/* Binary */` |
|    3 | 1094 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|    3 | 1095 | `		break;` |
|   11 | 1096 | `	default:` |
|    - | 1097 | `		/* Decimal */` |
|   23 | 1098 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|   22 | 1099 | `		break;` |
|    - | 1100 | `	}` |
|   33 | 1101 | `	return PH7_OK;` |
|   25 | 1102 |  |
|    - | 1103 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - | 1104 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1105 |  |
