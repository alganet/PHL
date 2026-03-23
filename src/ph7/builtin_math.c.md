# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 426/429 lines (99.30%)

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
|   34 |  398 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  399 | `	/* Perform the requested operation */` |
|   34 |  400 | `	r = atan(x);` |
|    - |  401 | `	/* store the result back */` |
|   34 |  402 | `	ph7_result_double(pCtx,r);` |
|   34 |  403 | `	return PH7_OK;` |
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
|   10 |  437 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  438 |  |
|    - |  439 | `	double r,x,y;` |
|   11 |  440 | `	if( nArg < 2 ){` |
|    - |  441 | `		/* Missing arguments,return 0 */` |
|    5 |  442 | `		ph7_result_int(pCtx,0);` |
|    5 |  443 | `		return PH7_OK;` |
|    - |  444 | `	}` |
|    7 |  445 | `	y = ph7_value_to_double(apArg[0]);` |
|    7 |  446 | `	x = ph7_value_to_double(apArg[1]);` |
|    - |  447 | `	/* Perform the requested operation */` |
|    7 |  448 | `	r = atan2(y,x);` |
|    - |  449 | `	/* store the result back */` |
|    7 |  450 | `	ph7_result_double(pCtx,r);` |
|    7 |  451 | `	return PH7_OK;` |
|    6 |  452 |  |
|    - |  453 | `/*` |
|    - |  454 | ` * float/int64 abs(float/int64 $arg )` |
|    - |  455 | ` *  Absolute value.` |
|    - |  456 | ` * Parameter` |
|    - |  457 | ` *  The number to process.` |
|    - |  458 | ` * Return` |
|    - |  459 | ` *  The absolute value of number.` |
|    - |  460 | ` */` |
|  122 |  461 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  462 |  |
|    - |  463 | `	int is_float;` |
|    - |  464 | `	/* PHP requires exactly one argument. */` |
|  124 |  465 | `	if( nArg != 1 ){` |
|   11 |  466 | `		return PH7_VmThrowException(pCtx,` |
|    - |  467 | `			"ArgumentCountError",` |
|    - |  468 | `			"abs() expects exactly 1 argument, %d given",` |
|    3 |  469 | `			nArg` |
|    - |  470 | `			);` |
|    - |  471 | `	}` |
|    - |  472 |  |
|    - |  473 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|  118 |  474 | `	is_float = ph7_value_is_float(apArg[0]);` |
|  118 |  475 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|    - |  476 | `		int len;` |
|   10 |  477 | `		sxu8 bReal = FALSE;` |
|   10 |  478 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  479 | `		sxi32 rcNum;` |
|   10 |  480 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|   10 |  481 | `		if( rcNum != SXRET_OK ){` |
|    3 |  482 | `			return PH7_VmThrowException(pCtx,` |
|    - |  483 | `				"TypeError",` |
|    - |  484 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  485 | `				);` |
|    - |  486 | `		}` |
|    7 |  487 | `		if( bReal ){` |
|    5 |  488 | `			is_float = 1;` |
|    2 |  489 | `		}` |
|    3 |  490 | `	}` |
|  116 |  491 | `	if( is_float ){` |
|    - |  492 | `		double r,x;` |
|   99 |  493 | `		x = ph7_value_to_double(apArg[0]);` |
|    - |  494 | `		/* Perform the requested operation */` |
|   99 |  495 | `		r = fabs(x);` |
|   99 |  496 | `		ph7_result_double(pCtx,r);` |
|   50 |  497 | `	}else{` |
|    - |  498 | `		int r,x;` |
|   18 |  499 | `		x = ph7_value_to_int(apArg[0]);` |
|    - |  500 | `		/* Perform the requested operation */` |
|   18 |  501 | `		r = abs(x);` |
|   18 |  502 | `		ph7_result_int(pCtx,r);` |
|    - |  503 | `	}` |
|  116 |  504 | `	return PH7_OK;` |
|   63 |  505 |  |
|    - |  506 | `/*` |
|    - |  507 | ` * float log(float $arg,[int/float $base])` |
|    - |  508 | ` *  Natural logarithm.` |
|    - |  509 | ` * Parameter` |
|    - |  510 | ` *  $arg: The number to process.` |
|    - |  511 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|    - |  512 | ` * Return` |
|    - |  513 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|    - |  514 | ` * Note:` |
|    - |  515 | ` *  only Natural log and base-10 log are supported.` |
|    - |  516 | ` */` |
|   14 |  517 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  518 |  |
|    - |  519 | `	double r,x;` |
|   15 |  520 | `	if( nArg < 1 ){` |
|    - |  521 | `		/* Missing argument,return 0 */` |
|    3 |  522 | `		ph7_result_int(pCtx,0);` |
|    3 |  523 | `		return PH7_OK;` |
|    - |  524 | `	}` |
|   13 |  525 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  526 | `	/* Perform the requested operation */` |
|   13 |  527 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|    - |  528 | `		/* Base-10 log */` |
|    5 |  529 | `		r = log10(x);` |
|    3 |  530 | `	}else{` |
|    9 |  531 | `		r = log(x);` |
|    - |  532 | `	}` |
|    - |  533 | `	/* store the result back */` |
|   13 |  534 | `	ph7_result_double(pCtx,r);` |
|   13 |  535 | `	return PH7_OK;` |
|    8 |  536 |  |
|    - |  537 | `/*` |
|    - |  538 | ` * float log10(float $arg )` |
|    - |  539 | ` *  Base-10 logarithm.` |
|    - |  540 | ` * Parameter` |
|    - |  541 | ` *  The number to process.` |
|    - |  542 | ` * Return` |
|    - |  543 | ` *  The Base-10 logarithm of the given number.` |
|    - |  544 | ` */` |
|   16 |  545 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  546 |  |
|    - |  547 | `	double r,x;` |
|   17 |  548 | `	if( nArg < 1 ){` |
|    - |  549 | `		/* Missing argument,return 0 */` |
|    3 |  550 | `		ph7_result_int(pCtx,0);` |
|    3 |  551 | `		return PH7_OK;` |
|    - |  552 | `	}` |
|   15 |  553 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  554 | `	/* Perform the requested operation */` |
|   15 |  555 | `	r = log10(x);` |
|    - |  556 | `	/* store the result back */` |
|   15 |  557 | `	ph7_result_double(pCtx,r);` |
|   15 |  558 | `	return PH7_OK;` |
|    9 |  559 |  |
|    - |  560 | `/*` |
|    - |  561 | ` * number pow(number $base,number $exp)` |
|    - |  562 | ` *  Exponential expression.` |
|    - |  563 | ` * Parameter` |
|    - |  564 | ` *  base` |
|    - |  565 | ` *  The base to use.` |
|    - |  566 | ` * exp` |
|    - |  567 | ` *  The exponent.` |
|    - |  568 | ` * Return` |
|    - |  569 | ` *  base raised to the power of exp.` |
|    - |  570 | ` *  If the result can be represented as integer it will be returned` |
|    - |  571 | ` *  as type integer, else it will be returned as type float.` |
|    - |  572 | ` */` |
|    8 |  573 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  574 |  |
|    - |  575 | `	double r,x,y;` |
|    9 |  576 | `	if( nArg < 1 ){` |
|    - |  577 | `		/* Missing argument,return 0 */` |
|    5 |  578 | `		ph7_result_int(pCtx,0);` |
|    5 |  579 | `		return PH7_OK;` |
|    - |  580 | `	}` |
|    5 |  581 | `	x = ph7_value_to_double(apArg[0]);` |
|    5 |  582 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  583 | `	/* Perform the requested operation */` |
|    5 |  584 | `	r = pow(x,y);` |
|    5 |  585 | `	ph7_result_double(pCtx,r);` |
|    5 |  586 | `	return PH7_OK;` |
|    5 |  587 |  |
|    - |  588 | `/*` |
|    - |  589 | ` * float pi(void)` |
|    - |  590 | ` *  Returns an approximation of pi.` |
|    - |  591 | ` * Note` |
|    - |  592 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|    - |  593 | ` * Return` |
|    - |  594 | ` *  The value of pi as float.` |
|    - |  595 | ` */` |
|    2 |  596 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  597 |  |
|    1 |  598 | `	SXUNUSED(nArg); /* cc warning */` |
|    1 |  599 | `	SXUNUSED(apArg);` |
|    3 |  600 | `	ph7_result_double(pCtx,PH7_PI);` |
|    3 |  601 | `	return PH7_OK;` |
|    1 |  602 |  |
|    - |  603 | `/*` |
|    - |  604 | ` * float fmod(float $x,float $y)` |
|    - |  605 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|    - |  606 | ` * Parameters` |
|    - |  607 | ` * $x` |
|    - |  608 | ` *  The dividend` |
|    - |  609 | ` * $y` |
|    - |  610 | ` *  The divisor` |
|    - |  611 | ` * Return` |
|    - |  612 | ` *  The floating point remainder of x/y.` |
|    - |  613 | ` */` |
|    8 |  614 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  615 |  |
|    - |  616 | `	double x,y,r;` |
|    9 |  617 | `	if( nArg < 2 ){` |
|    - |  618 | `		/* Missing arguments */` |
|    7 |  619 | `		ph7_result_double(pCtx,0);` |
|    7 |  620 | `		return PH7_OK;` |
|    - |  621 | `	}` |
|    - |  622 | `	/* Extract given arguments */` |
|    3 |  623 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  624 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  625 | `	/* Perform the requested operation */` |
|    3 |  626 | `	r = fmod(x,y);` |
|    - |  627 | `	/* Processing result */` |
|    3 |  628 | `	ph7_result_double(pCtx,r);` |
|    3 |  629 | `	return PH7_OK;` |
|    5 |  630 |  |
|    - |  631 | `/*` |
|    - |  632 | ` * float hypot(float $x,float $y)` |
|    - |  633 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|    - |  634 | ` * Parameters` |
|    - |  635 | ` * $x` |
|    - |  636 | ` *  Length of first side` |
|    - |  637 | ` * $y` |
|    - |  638 | ` *  Length of first side` |
|    - |  639 | ` * Return` |
|    - |  640 | ` *  Calculated length of the hypotenuse.` |
|    - |  641 | ` */` |
|    6 |  642 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  643 |  |
|    - |  644 | `	double x,y,r;` |
|    7 |  645 | `	if( nArg < 2 ){` |
|    - |  646 | `		/* Missing arguments */` |
|    5 |  647 | `		ph7_result_double(pCtx,0);` |
|    5 |  648 | `		return PH7_OK;` |
|    - |  649 | `	}` |
|    - |  650 | `	/* Extract given arguments */` |
|    3 |  651 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  652 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  653 | `	/* Perform the requested operation */` |
|    3 |  654 | `	r = hypot(x,y);` |
|    - |  655 | `	/* Processing result */` |
|    3 |  656 | `	ph7_result_double(pCtx,r);` |
|    3 |  657 | `	return PH7_OK;` |
|    4 |  658 |  |
|    - |  659 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|    - |  660 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  661 | `/*` |
|    - |  662 | ` * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|    - |  663 | ` *  Exponential expression.` |
|    - |  664 | ` * Parameter` |
|    - |  665 | ` *  $val` |
|    - |  666 | ` *   The value to round.` |
|    - |  667 | ` * $precision` |
|    - |  668 | ` *   The optional number of decimal digits to round to.` |
|    - |  669 | ` * $mode` |
|    - |  670 | ` *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.` |
|    - |  671 | ` *   (not supported).` |
|    - |  672 | ` * Return` |
|    - |  673 | ` *  The rounded value.` |
|    - |  674 | ` */` |
|   20 |  675 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  676 |  |
|   21 |  677 | `	int n = 0;` |
|    - |  678 | `	double r;` |
|   21 |  679 | `	if( nArg < 1 ){` |
|    - |  680 | `		/* Missing argument,return 0 */` |
|    5 |  681 | `		ph7_result_int(pCtx,0);` |
|    5 |  682 | `		return PH7_OK;` |
|    - |  683 | `	}` |
|    - |  684 | `	/* Extract the precision if available */` |
|   17 |  685 | `	if( nArg > 1 ){` |
|    5 |  686 | `		n = ph7_value_to_int(apArg[1]);` |
|    5 |  687 | `		if( n>30 ){` |
|    3 |  688 | `			n = 30;` |
|    1 |  689 | `		}` |
|    5 |  690 | `		if( n<0 ){` |
|    3 |  691 | `			n = 0;` |
|    1 |  692 | `		}` |
|    2 |  693 | `	}` |
|   17 |  694 | `	r = ph7_value_to_double(apArg[0]);` |
|    - |  695 | `	/* If Y==0 and X will fit in a 64-bit int,` |
|    - |  696 | `     * handle the rounding directly.Otherwise` |
|    - |  697 | `	 * use our own cutsom printf [i.e:SyBufferFormat()].` |
|    - |  698 | `     */` |
|   17 |  699 | `	if( n==0 && r>=0 && r < (double)(LARGEST_INT64-1) ){` |
|   13 |  700 | `    r = (double)((ph7_int64)(r+0.5));` |
|   11 |  701 | `	}else if( n==0 && r<0 && (-r) < (double)(LARGEST_INT64-1) ){` |
|    3 |  702 | `    r = -(double)((ph7_int64)((-r)+0.5));` |
|    2 |  703 | `  }else{` |
|    - |  704 | `	  char zBuf[256];` |
|    - |  705 | `	  sxu32 nLen;` |
|    3 |  706 | `	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);` |
|    - |  707 | `	  /* Convert the string to real number */` |
|    3 |  708 | `	  SyStrToReal(zBuf,nLen,(void *)&r,0);` |
|    - |  709 | `  }` |
|    - |  710 | `  /* Return thr rounded value */` |
|   17 |  711 | `  ph7_result_double(pCtx,r);` |
|   17 |  712 | `  return PH7_OK;` |
|   11 |  713 |  |
|    - |  714 | `/*` |
|    - |  715 | ` * string dechex(int $number)` |
|    - |  716 | ` *  Decimal to hexadecimal.` |
|    - |  717 | ` * Parameters` |
|    - |  718 | ` *  $number` |
|    - |  719 | ` *   Decimal value to convert` |
|    - |  720 | ` * Return` |
|    - |  721 | ` *  Hexadecimal string representation of number` |
|    - |  722 | ` */` |
|    6 |  723 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  724 |  |
|    - |  725 | `	int iVal;` |
|    7 |  726 | `	if( nArg < 1 ){` |
|    - |  727 | `		/* Missing arguments,return null */` |
|    5 |  728 | `		ph7_result_null(pCtx);` |
|    5 |  729 | `		return PH7_OK;` |
|    - |  730 | `	}` |
|    - |  731 | `	/* Extract the given number */` |
|    3 |  732 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  733 | `	/* Format */` |
|    3 |  734 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|    3 |  735 | `	return PH7_OK;` |
|    4 |  736 |  |
|    - |  737 | `/*` |
|    - |  738 | ` * string decoct(int $number)` |
|    - |  739 | ` *  Decimal to Octal.` |
|    - |  740 | ` * Parameters` |
|    - |  741 | ` *  $number` |
|    - |  742 | ` *   Decimal value to convert` |
|    - |  743 | ` * Return` |
|    - |  744 | ` *  Octal string representation of number` |
|    - |  745 | ` */` |
|    8 |  746 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  747 |  |
|    - |  748 | `	int iVal;` |
|    9 |  749 | `	if( nArg < 1 ){` |
|    - |  750 | `		/* Missing arguments,return null */` |
|    3 |  751 | `		ph7_result_null(pCtx);` |
|    3 |  752 | `		return PH7_OK;` |
|    - |  753 | `	}` |
|    - |  754 | `	/* Extract the given number */` |
|    7 |  755 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  756 | `	/* Format */` |
|    7 |  757 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|    7 |  758 | `	return PH7_OK;` |
|    5 |  759 |  |
|    - |  760 | `/*` |
|    - |  761 | ` * string decbin(int $number)` |
|    - |  762 | ` *  Decimal to binary.` |
|    - |  763 | ` * Parameters` |
|    - |  764 | ` *  $number` |
|    - |  765 | ` *   Decimal value to convert` |
|    - |  766 | ` * Return` |
|    - |  767 | ` *  Binary string representation of number` |
|    - |  768 | ` */` |
|    4 |  769 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  770 |  |
|    - |  771 | `	int iVal;` |
|    5 |  772 | `	if( nArg < 1 ){` |
|    - |  773 | `		/* Missing arguments,return null */` |
|    3 |  774 | `		ph7_result_null(pCtx);` |
|    3 |  775 | `		return PH7_OK;` |
|    - |  776 | `	}` |
|    - |  777 | `	/* Extract the given number */` |
|    3 |  778 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  779 | `	/* Format */` |
|    3 |  780 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|    3 |  781 | `	return PH7_OK;` |
|    3 |  782 |  |
|    - |  783 | `/*` |
|    - |  784 | ` * int64 hexdec(string $hex_string)` |
|    - |  785 | ` *  Hexadecimal to decimal.` |
|    - |  786 | ` * Parameters` |
|    - |  787 | ` *  $hex_string` |
|    - |  788 | ` *   The hexadecimal string to convert` |
|    - |  789 | ` * Return` |
|    - |  790 | ` *  The decimal representation of hex_string` |
|    - |  791 | ` */` |
|   24 |  792 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  793 |  |
|    - |  794 | `	const char *zString,*zEnd;` |
|    - |  795 | `	ph7_int64 iVal;` |
|    - |  796 | `	int nLen;` |
|   25 |  797 | `	if( nArg < 1 ){` |
|    - |  798 | `		/* Missing arguments,return -1 */` |
|    5 |  799 | `		ph7_result_int(pCtx,-1);` |
|    5 |  800 | `		return PH7_OK;` |
|    - |  801 | `	}` |
|   21 |  802 | `	iVal = 0;` |
|   21 |  803 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  804 | `		/* Extract the given string */` |
|   15 |  805 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    - |  806 | `		/* Delimit the string */` |
|   15 |  807 | `		zEnd = &zString[nLen];` |
|    - |  808 | `		/* Ignore non hex-stream */` |
|   21 |  809 | `		while( zString < zEnd ){` |
|   21 |  810 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|    - |  811 | `				/* UTF-8 stream */` |
|    5 |  812 | `				zString++;` |
|    9 |  813 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|    5 |  814 | `					zString++;` |
|    1 |  815 | `				}` |
|    3 |  816 | `			}else{` |
|   17 |  817 | `				if( SyisHex(zString[0]) ){` |
|   15 |  818 | `					break;` |
|    - |  819 | `				}` |
|    - |  820 | `				/* Ignore */` |
|    3 |  821 | `				zString++;` |
|    - |  822 | `			}` |
|    1 |  823 | `		}` |
|   15 |  824 | `		if( zString < zEnd ){` |
|    - |  825 | `			/* Cast */` |
|   15 |  826 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|    7 |  827 | `		}` |
|    8 |  828 | `	}else{` |
|    - |  829 | `		/* Extract as a 64-bit integer */` |
|    7 |  830 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  831 | `	}` |
|    - |  832 | `	/* Return the number */` |
|   21 |  833 | `	ph7_result_int64(pCtx,iVal);` |
|   21 |  834 | `	return PH7_OK;` |
|   13 |  835 |  |
|    - |  836 | `/*` |
|    - |  837 | ` * int64 bindec(string $bin_string)` |
|    - |  838 | ` *  Binary to decimal.` |
|    - |  839 | ` * Parameters` |
|    - |  840 | ` *  $bin_string` |
|    - |  841 | ` *   The binary string to convert` |
|    - |  842 | ` * Return` |
|    - |  843 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|    - |  844 | ` */` |
|   12 |  845 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  846 |  |
|    - |  847 | `	const char *zString;` |
|    - |  848 | `	ph7_int64 iVal;` |
|    - |  849 | `	int nLen;` |
|   13 |  850 | `	if( nArg < 1 ){` |
|    - |  851 | `		/* Missing arguments,return -1 */` |
|    5 |  852 | `		ph7_result_int(pCtx,-1);` |
|    5 |  853 | `		return PH7_OK;` |
|    - |  854 | `	}` |
|    9 |  855 | `	iVal = 0;` |
|    9 |  856 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  857 | `		/* Extract the given string */` |
|    7 |  858 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    7 |  859 | `		if( nLen > 0 ){` |
|    - |  860 | `			/* Perform a binary cast */` |
|    5 |  861 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    2 |  862 | `		}` |
|    4 |  863 | `	}else{` |
|    - |  864 | `		/* Extract as a 64-bit integer */` |
|    3 |  865 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  866 | `	}` |
|    - |  867 | `	/* Return the number */` |
|    9 |  868 | `	ph7_result_int64(pCtx,iVal);` |
|    9 |  869 | `	return PH7_OK;` |
|    7 |  870 |  |
|    - |  871 | `/*` |
|    - |  872 | ` * int64 octdec(string $oct_string)` |
|    - |  873 | ` *  Octal to decimal.` |
|    - |  874 | ` * Parameters` |
|    - |  875 | ` *  $oct_string` |
|    - |  876 | ` *   The octal string to convert` |
|    - |  877 | ` * Return` |
|    - |  878 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|    - |  879 | ` */` |
|    6 |  880 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  881 |  |
|    - |  882 | `	const char *zString;` |
|    - |  883 | `	ph7_int64 iVal;` |
|    - |  884 | `	int nLen;` |
|    7 |  885 | `	if( nArg < 1 ){` |
|    - |  886 | `		/* Missing arguments,return -1 */` |
|    3 |  887 | `		ph7_result_int(pCtx,-1);` |
|    3 |  888 | `		return PH7_OK;` |
|    - |  889 | `	}` |
|    5 |  890 | `	iVal = 0;` |
|    5 |  891 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  892 | `		/* Extract the given string */` |
|    3 |  893 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    3 |  894 | `		if( nLen > 0 ){` |
|    - |  895 | `			/* Perform the cast */` |
|    3 |  896 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    1 |  897 | `		}` |
|    2 |  898 | `	}else{` |
|    - |  899 | `		/* Extract as a 64-bit integer */` |
|    3 |  900 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  901 | `	}` |
|    - |  902 | `	/* Return the number */` |
|    5 |  903 | `	ph7_result_int64(pCtx,iVal);` |
|    5 |  904 | `	return PH7_OK;` |
|    4 |  905 |  |
|    - |  906 | `/*` |
|    - |  907 | ` * srand([int $seed])` |
|    - |  908 | ` * mt_srand([int $seed])` |
|    - |  909 | ` *  Seed the random number generator.` |
|    - |  910 | ` * Parameters` |
|    - |  911 | ` * $seed` |
|    - |  912 | ` *  Optional seed value` |
|    - |  913 | ` * Return` |
|    - |  914 | ` *  null.` |
|    - |  915 | ` * Note:` |
|    - |  916 | ` *  THIS FUNCTION IS A NO-OP.` |
|    - |  917 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|    - |  918 | ` */` |
|   20 |  919 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  920 |  |
|   10 |  921 | `	SXUNUSED(nArg);` |
|   10 |  922 | `	SXUNUSED(apArg);` |
|   21 |  923 | `	ph7_result_null(pCtx);` |
|   21 |  924 | `	return PH7_OK;` |
|    1 |  925 |  |
|    - |  926 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - |  927 | `/*` |
|    - |  928 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|    - |  929 | ` *  Convert a number between arbitrary bases.` |
|    - |  930 | ` * Parameters` |
|    - |  931 | ` * $number` |
|    - |  932 | ` *  The number to convert` |
|    - |  933 | ` * $frombase` |
|    - |  934 | ` *  The base number is in` |
|    - |  935 | ` * $tobase` |
|    - |  936 | ` *  The base to convert number to` |
|    - |  937 | ` * Return` |
|    - |  938 | ` *  Number converted to base tobase` |
|    - |  939 | ` */` |
|   48 |  940 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  941 |  |
|    - |  942 | `	int nLen,iFbase,iTobase;` |
|    - |  943 | `	const char *zNum;` |
|    - |  944 | `	ph7_int64 iNum;` |
|   49 |  945 | `	if( nArg < 3 ){` |
|    - |  946 | `		/* Return the empty string*/` |
|   13 |  947 | `		ph7_result_string(pCtx,"",0);` |
|   13 |  948 | `		return PH7_OK;` |
|    - |  949 | `	}` |
|    - |  950 | `	/* Base numbers */` |
|   37 |  951 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|   37 |  952 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|   37 |  953 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  954 | `		/* Extract the target number */` |
|   33 |  955 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|   33 |  956 | `		if( nLen < 1 ){` |
|    - |  957 | `			/* Return the empty string*/` |
|    5 |  958 | `			ph7_result_string(pCtx,"",0);` |
|    5 |  959 | `			return PH7_OK;` |
|    - |  960 | `		}` |
|    - |  961 | `		/* Base conversion */` |
|   29 |  962 | `		switch(iFbase){` |
|    5 |  963 | `		case 16:` |
|    - |  964 | `			/* Hex */` |
|   11 |  965 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|   11 |  966 | `			break;` |
|    3 |  967 | `		case 8:` |
|    - |  968 | `			/* Octal */` |
|    7 |  969 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    7 |  970 | `			break;` |
|    2 |  971 | `		case 2:` |
|    - |  972 | `			/* Binary */` |
|    5 |  973 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    5 |  974 | `			break;` |
|    4 |  975 | `		default:` |
|    - |  976 | `			/* Decimal */` |
|    9 |  977 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    8 |  978 | `			break;` |
|    - |  979 | `		}` |
|   15 |  980 | `	}else{` |
|    5 |  981 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|    - |  982 | `	}` |
|   33 |  983 | `	switch(iTobase){` |
|    3 |  984 | `	case 16:` |
|    - |  985 | `		/* Hex */` |
|    7 |  986 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|    7 |  987 | `		break;` |
|    1 |  988 | `	case 8:` |
|    - |  989 | `		/* Octal */` |
|    3 |  990 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|    3 |  991 | `		break;` |
|    1 |  992 | `	case 2:` |
|    - |  993 | `		/* Binary */` |
|    3 |  994 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|    3 |  995 | `		break;` |
|   11 |  996 | `	default:` |
|    - |  997 | `		/* Decimal */` |
|   23 |  998 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|   22 |  999 | `		break;` |
|    - | 1000 | `	}` |
|   33 | 1001 | `	return PH7_OK;` |
|   25 | 1002 |  |
|    - | 1003 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - | 1004 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1005 |  |
