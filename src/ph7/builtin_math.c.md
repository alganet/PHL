# src/ph7/builtin_math.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 423/426 lines (99.30%)

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
|   16 |  378 | `PH7_PRIVATE int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  379 |  |
|    - |  380 | `	double r,x;` |
|   17 |  381 | `	if( nArg < 1 ){` |
|    - |  382 | `		/* Missing argument,return 0 */` |
|    5 |  383 | `		ph7_result_int(pCtx,0);` |
|    5 |  384 | `		return PH7_OK;` |
|    - |  385 | `	}` |
|   13 |  386 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  387 | `	/* Perform the requested operation */` |
|   13 |  388 | `	r = atan(x);` |
|    - |  389 | `	/* store the result back */` |
|   13 |  390 | `	ph7_result_double(pCtx,r);` |
|   13 |  391 | `	return PH7_OK;` |
|    9 |  392 |  |
|    - |  393 | `/*` |
|    - |  394 | ` * float tanh(float $arg )` |
|    - |  395 | ` *  Hyperbolic tangent.` |
|    - |  396 | ` * Parameter` |
|    - |  397 | ` *  The number to process.` |
|    - |  398 | ` * Return` |
|    - |  399 | ` *  The Hyperbolic tangent of arg.` |
|    - |  400 | ` */` |
|   20 |  401 | `PH7_PRIVATE int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  402 |  |
|    - |  403 | `	double r,x;` |
|   21 |  404 | `	if( nArg < 1 ){` |
|    - |  405 | `		/* Missing argument,return 0 */` |
|    3 |  406 | `		ph7_result_int(pCtx,0);` |
|    3 |  407 | `		return PH7_OK;` |
|    - |  408 | `	}` |
|   19 |  409 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  410 | `	/* Perform the requested operation */` |
|   19 |  411 | `	r = tanh(x);` |
|    - |  412 | `	/* store the result back */` |
|   19 |  413 | `	ph7_result_double(pCtx,r);` |
|   19 |  414 | `	return PH7_OK;` |
|   11 |  415 |  |
|    - |  416 | `/*` |
|    - |  417 | ` * float atan2(float $y,float $x)` |
|    - |  418 | ` *  Arc tangent of two variable.` |
|    - |  419 | ` * Parameter` |
|    - |  420 | ` *  $y = Dividend parameter.` |
|    - |  421 | ` *  $x = Divisor parameter.` |
|    - |  422 | ` * Return` |
|    - |  423 | ` *  The arc tangent of y/x in radian.` |
|    - |  424 | ` */` |
|   10 |  425 | `PH7_PRIVATE int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  426 |  |
|    - |  427 | `	double r,x,y;` |
|   11 |  428 | `	if( nArg < 2 ){` |
|    - |  429 | `		/* Missing arguments,return 0 */` |
|    5 |  430 | `		ph7_result_int(pCtx,0);` |
|    5 |  431 | `		return PH7_OK;` |
|    - |  432 | `	}` |
|    7 |  433 | `	y = ph7_value_to_double(apArg[0]);` |
|    7 |  434 | `	x = ph7_value_to_double(apArg[1]);` |
|    - |  435 | `	/* Perform the requested operation */` |
|    7 |  436 | `	r = atan2(y,x);` |
|    - |  437 | `	/* store the result back */` |
|    7 |  438 | `	ph7_result_double(pCtx,r);` |
|    7 |  439 | `	return PH7_OK;` |
|    6 |  440 |  |
|    - |  441 | `/*` |
|    - |  442 | ` * float/int64 abs(float/int64 $arg )` |
|    - |  443 | ` *  Absolute value.` |
|    - |  444 | ` * Parameter` |
|    - |  445 | ` *  The number to process.` |
|    - |  446 | ` * Return` |
|    - |  447 | ` *  The absolute value of number.` |
|    - |  448 | ` */` |
|  122 |  449 | `PH7_PRIVATE int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  450 |  |
|    - |  451 | `	int is_float;` |
|    - |  452 | `	/* PHP requires exactly one argument. */` |
|  124 |  453 | `	if( nArg != 1 ){` |
|   11 |  454 | `		return PH7_VmThrowException(pCtx,` |
|    - |  455 | `			"ArgumentCountError",` |
|    - |  456 | `			"abs() expects exactly 1 argument, %d given",` |
|    3 |  457 | `			nArg` |
|    - |  458 | `			);` |
|    - |  459 | `	}` |
|    - |  460 |  |
|    - |  461 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|  118 |  462 | `	is_float = ph7_value_is_float(apArg[0]);` |
|  118 |  463 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|    - |  464 | `		int len;` |
|   10 |  465 | `		sxu8 bReal = FALSE;` |
|   10 |  466 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|    - |  467 | `		sxi32 rcNum;` |
|   10 |  468 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|   10 |  469 | `		if( rcNum != SXRET_OK ){` |
|    3 |  470 | `			return PH7_VmThrowException(pCtx,` |
|    - |  471 | `				"TypeError",` |
|    - |  472 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|    - |  473 | `				);` |
|    - |  474 | `		}` |
|    7 |  475 | `		if( bReal ){` |
|    5 |  476 | `			is_float = 1;` |
|    2 |  477 | `		}` |
|    3 |  478 | `	}` |
|  116 |  479 | `	if( is_float ){` |
|    - |  480 | `		double r,x;` |
|   99 |  481 | `		x = ph7_value_to_double(apArg[0]);` |
|    - |  482 | `		/* Perform the requested operation */` |
|   99 |  483 | `		r = fabs(x);` |
|   99 |  484 | `		ph7_result_double(pCtx,r);` |
|   50 |  485 | `	}else{` |
|    - |  486 | `		int r,x;` |
|   18 |  487 | `		x = ph7_value_to_int(apArg[0]);` |
|    - |  488 | `		/* Perform the requested operation */` |
|   18 |  489 | `		r = abs(x);` |
|   18 |  490 | `		ph7_result_int(pCtx,r);` |
|    - |  491 | `	}` |
|  116 |  492 | `	return PH7_OK;` |
|   63 |  493 |  |
|    - |  494 | `/*` |
|    - |  495 | ` * float log(float $arg,[int/float $base])` |
|    - |  496 | ` *  Natural logarithm.` |
|    - |  497 | ` * Parameter` |
|    - |  498 | ` *  $arg: The number to process.` |
|    - |  499 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|    - |  500 | ` * Return` |
|    - |  501 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|    - |  502 | ` * Note:` |
|    - |  503 | ` *  only Natural log and base-10 log are supported.` |
|    - |  504 | ` */` |
|   14 |  505 | `PH7_PRIVATE int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  506 |  |
|    - |  507 | `	double r,x;` |
|   15 |  508 | `	if( nArg < 1 ){` |
|    - |  509 | `		/* Missing argument,return 0 */` |
|    3 |  510 | `		ph7_result_int(pCtx,0);` |
|    3 |  511 | `		return PH7_OK;` |
|    - |  512 | `	}` |
|   13 |  513 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  514 | `	/* Perform the requested operation */` |
|   13 |  515 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|    - |  516 | `		/* Base-10 log */` |
|    5 |  517 | `		r = log10(x);` |
|    3 |  518 | `	}else{` |
|    9 |  519 | `		r = log(x);` |
|    - |  520 | `	}` |
|    - |  521 | `	/* store the result back */` |
|   13 |  522 | `	ph7_result_double(pCtx,r);` |
|   13 |  523 | `	return PH7_OK;` |
|    8 |  524 |  |
|    - |  525 | `/*` |
|    - |  526 | ` * float log10(float $arg )` |
|    - |  527 | ` *  Base-10 logarithm.` |
|    - |  528 | ` * Parameter` |
|    - |  529 | ` *  The number to process.` |
|    - |  530 | ` * Return` |
|    - |  531 | ` *  The Base-10 logarithm of the given number.` |
|    - |  532 | ` */` |
|   16 |  533 | `PH7_PRIVATE int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  534 |  |
|    - |  535 | `	double r,x;` |
|   17 |  536 | `	if( nArg < 1 ){` |
|    - |  537 | `		/* Missing argument,return 0 */` |
|    3 |  538 | `		ph7_result_int(pCtx,0);` |
|    3 |  539 | `		return PH7_OK;` |
|    - |  540 | `	}` |
|   15 |  541 | `	x = ph7_value_to_double(apArg[0]);` |
|    - |  542 | `	/* Perform the requested operation */` |
|   15 |  543 | `	r = log10(x);` |
|    - |  544 | `	/* store the result back */` |
|   15 |  545 | `	ph7_result_double(pCtx,r);` |
|   15 |  546 | `	return PH7_OK;` |
|    9 |  547 |  |
|    - |  548 | `/*` |
|    - |  549 | ` * number pow(number $base,number $exp)` |
|    - |  550 | ` *  Exponential expression.` |
|    - |  551 | ` * Parameter` |
|    - |  552 | ` *  base` |
|    - |  553 | ` *  The base to use.` |
|    - |  554 | ` * exp` |
|    - |  555 | ` *  The exponent.` |
|    - |  556 | ` * Return` |
|    - |  557 | ` *  base raised to the power of exp.` |
|    - |  558 | ` *  If the result can be represented as integer it will be returned` |
|    - |  559 | ` *  as type integer, else it will be returned as type float.` |
|    - |  560 | ` */` |
|    8 |  561 | `PH7_PRIVATE int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  562 |  |
|    - |  563 | `	double r,x,y;` |
|    9 |  564 | `	if( nArg < 1 ){` |
|    - |  565 | `		/* Missing argument,return 0 */` |
|    5 |  566 | `		ph7_result_int(pCtx,0);` |
|    5 |  567 | `		return PH7_OK;` |
|    - |  568 | `	}` |
|    5 |  569 | `	x = ph7_value_to_double(apArg[0]);` |
|    5 |  570 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  571 | `	/* Perform the requested operation */` |
|    5 |  572 | `	r = pow(x,y);` |
|    5 |  573 | `	ph7_result_double(pCtx,r);` |
|    5 |  574 | `	return PH7_OK;` |
|    5 |  575 |  |
|    - |  576 | `/*` |
|    - |  577 | ` * float pi(void)` |
|    - |  578 | ` *  Returns an approximation of pi.` |
|    - |  579 | ` * Note` |
|    - |  580 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|    - |  581 | ` * Return` |
|    - |  582 | ` *  The value of pi as float.` |
|    - |  583 | ` */` |
|    2 |  584 | `PH7_PRIVATE int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  585 |  |
|    1 |  586 | `	SXUNUSED(nArg); /* cc warning */` |
|    1 |  587 | `	SXUNUSED(apArg);` |
|    3 |  588 | `	ph7_result_double(pCtx,PH7_PI);` |
|    3 |  589 | `	return PH7_OK;` |
|    1 |  590 |  |
|    - |  591 | `/*` |
|    - |  592 | ` * float fmod(float $x,float $y)` |
|    - |  593 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|    - |  594 | ` * Parameters` |
|    - |  595 | ` * $x` |
|    - |  596 | ` *  The dividend` |
|    - |  597 | ` * $y` |
|    - |  598 | ` *  The divisor` |
|    - |  599 | ` * Return` |
|    - |  600 | ` *  The floating point remainder of x/y.` |
|    - |  601 | ` */` |
|    8 |  602 | `PH7_PRIVATE int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  603 |  |
|    - |  604 | `	double x,y,r;` |
|    9 |  605 | `	if( nArg < 2 ){` |
|    - |  606 | `		/* Missing arguments */` |
|    7 |  607 | `		ph7_result_double(pCtx,0);` |
|    7 |  608 | `		return PH7_OK;` |
|    - |  609 | `	}` |
|    - |  610 | `	/* Extract given arguments */` |
|    3 |  611 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  612 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  613 | `	/* Perform the requested operation */` |
|    3 |  614 | `	r = fmod(x,y);` |
|    - |  615 | `	/* Processing result */` |
|    3 |  616 | `	ph7_result_double(pCtx,r);` |
|    3 |  617 | `	return PH7_OK;` |
|    5 |  618 |  |
|    - |  619 | `/*` |
|    - |  620 | ` * float hypot(float $x,float $y)` |
|    - |  621 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|    - |  622 | ` * Parameters` |
|    - |  623 | ` * $x` |
|    - |  624 | ` *  Length of first side` |
|    - |  625 | ` * $y` |
|    - |  626 | ` *  Length of first side` |
|    - |  627 | ` * Return` |
|    - |  628 | ` *  Calculated length of the hypotenuse.` |
|    - |  629 | ` */` |
|    6 |  630 | `PH7_PRIVATE int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  631 |  |
|    - |  632 | `	double x,y,r;` |
|    7 |  633 | `	if( nArg < 2 ){` |
|    - |  634 | `		/* Missing arguments */` |
|    5 |  635 | `		ph7_result_double(pCtx,0);` |
|    5 |  636 | `		return PH7_OK;` |
|    - |  637 | `	}` |
|    - |  638 | `	/* Extract given arguments */` |
|    3 |  639 | `	x = ph7_value_to_double(apArg[0]);` |
|    3 |  640 | `	y = ph7_value_to_double(apArg[1]);` |
|    - |  641 | `	/* Perform the requested operation */` |
|    3 |  642 | `	r = hypot(x,y);` |
|    - |  643 | `	/* Processing result */` |
|    3 |  644 | `	ph7_result_double(pCtx,r);` |
|    3 |  645 | `	return PH7_OK;` |
|    4 |  646 |  |
|    - |  647 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|    - |  648 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  649 | `/*` |
|    - |  650 | ` * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|    - |  651 | ` *  Exponential expression.` |
|    - |  652 | ` * Parameter` |
|    - |  653 | ` *  $val` |
|    - |  654 | ` *   The value to round.` |
|    - |  655 | ` * $precision` |
|    - |  656 | ` *   The optional number of decimal digits to round to.` |
|    - |  657 | ` * $mode` |
|    - |  658 | ` *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.` |
|    - |  659 | ` *   (not supported).` |
|    - |  660 | ` * Return` |
|    - |  661 | ` *  The rounded value.` |
|    - |  662 | ` */` |
|   20 |  663 | `PH7_PRIVATE int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  664 |  |
|   21 |  665 | `	int n = 0;` |
|    - |  666 | `	double r;` |
|   21 |  667 | `	if( nArg < 1 ){` |
|    - |  668 | `		/* Missing argument,return 0 */` |
|    5 |  669 | `		ph7_result_int(pCtx,0);` |
|    5 |  670 | `		return PH7_OK;` |
|    - |  671 | `	}` |
|    - |  672 | `	/* Extract the precision if available */` |
|   17 |  673 | `	if( nArg > 1 ){` |
|    5 |  674 | `		n = ph7_value_to_int(apArg[1]);` |
|    5 |  675 | `		if( n>30 ){` |
|    3 |  676 | `			n = 30;` |
|    1 |  677 | `		}` |
|    5 |  678 | `		if( n<0 ){` |
|    3 |  679 | `			n = 0;` |
|    1 |  680 | `		}` |
|    2 |  681 | `	}` |
|   17 |  682 | `	r = ph7_value_to_double(apArg[0]);` |
|    - |  683 | `	/* If Y==0 and X will fit in a 64-bit int,` |
|    - |  684 | `     * handle the rounding directly.Otherwise` |
|    - |  685 | `	 * use our own cutsom printf [i.e:SyBufferFormat()].` |
|    - |  686 | `     */` |
|   17 |  687 | `	if( n==0 && r>=0 && r < (double)(LARGEST_INT64-1) ){` |
|   13 |  688 | `    r = (double)((ph7_int64)(r+0.5));` |
|   11 |  689 | `	}else if( n==0 && r<0 && (-r) < (double)(LARGEST_INT64-1) ){` |
|    3 |  690 | `    r = -(double)((ph7_int64)((-r)+0.5));` |
|    2 |  691 | `  }else{` |
|    - |  692 | `	  char zBuf[256];` |
|    - |  693 | `	  sxu32 nLen;` |
|    3 |  694 | `	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);` |
|    - |  695 | `	  /* Convert the string to real number */` |
|    3 |  696 | `	  SyStrToReal(zBuf,nLen,(void *)&r,0);` |
|    - |  697 | `  }` |
|    - |  698 | `  /* Return thr rounded value */` |
|   17 |  699 | `  ph7_result_double(pCtx,r);` |
|   17 |  700 | `  return PH7_OK;` |
|   11 |  701 |  |
|    - |  702 | `/*` |
|    - |  703 | ` * string dechex(int $number)` |
|    - |  704 | ` *  Decimal to hexadecimal.` |
|    - |  705 | ` * Parameters` |
|    - |  706 | ` *  $number` |
|    - |  707 | ` *   Decimal value to convert` |
|    - |  708 | ` * Return` |
|    - |  709 | ` *  Hexadecimal string representation of number` |
|    - |  710 | ` */` |
|    6 |  711 | `PH7_PRIVATE int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  712 |  |
|    - |  713 | `	int iVal;` |
|    7 |  714 | `	if( nArg < 1 ){` |
|    - |  715 | `		/* Missing arguments,return null */` |
|    5 |  716 | `		ph7_result_null(pCtx);` |
|    5 |  717 | `		return PH7_OK;` |
|    - |  718 | `	}` |
|    - |  719 | `	/* Extract the given number */` |
|    3 |  720 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  721 | `	/* Format */` |
|    3 |  722 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|    3 |  723 | `	return PH7_OK;` |
|    4 |  724 |  |
|    - |  725 | `/*` |
|    - |  726 | ` * string decoct(int $number)` |
|    - |  727 | ` *  Decimal to Octal.` |
|    - |  728 | ` * Parameters` |
|    - |  729 | ` *  $number` |
|    - |  730 | ` *   Decimal value to convert` |
|    - |  731 | ` * Return` |
|    - |  732 | ` *  Octal string representation of number` |
|    - |  733 | ` */` |
|    8 |  734 | `PH7_PRIVATE int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  735 |  |
|    - |  736 | `	int iVal;` |
|    9 |  737 | `	if( nArg < 1 ){` |
|    - |  738 | `		/* Missing arguments,return null */` |
|    3 |  739 | `		ph7_result_null(pCtx);` |
|    3 |  740 | `		return PH7_OK;` |
|    - |  741 | `	}` |
|    - |  742 | `	/* Extract the given number */` |
|    7 |  743 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  744 | `	/* Format */` |
|    7 |  745 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|    7 |  746 | `	return PH7_OK;` |
|    5 |  747 |  |
|    - |  748 | `/*` |
|    - |  749 | ` * string decbin(int $number)` |
|    - |  750 | ` *  Decimal to binary.` |
|    - |  751 | ` * Parameters` |
|    - |  752 | ` *  $number` |
|    - |  753 | ` *   Decimal value to convert` |
|    - |  754 | ` * Return` |
|    - |  755 | ` *  Binary string representation of number` |
|    - |  756 | ` */` |
|    4 |  757 | `PH7_PRIVATE int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  758 |  |
|    - |  759 | `	int iVal;` |
|    5 |  760 | `	if( nArg < 1 ){` |
|    - |  761 | `		/* Missing arguments,return null */` |
|    3 |  762 | `		ph7_result_null(pCtx);` |
|    3 |  763 | `		return PH7_OK;` |
|    - |  764 | `	}` |
|    - |  765 | `	/* Extract the given number */` |
|    3 |  766 | `	iVal = ph7_value_to_int(apArg[0]);` |
|    - |  767 | `	/* Format */` |
|    3 |  768 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|    3 |  769 | `	return PH7_OK;` |
|    3 |  770 |  |
|    - |  771 | `/*` |
|    - |  772 | ` * int64 hexdec(string $hex_string)` |
|    - |  773 | ` *  Hexadecimal to decimal.` |
|    - |  774 | ` * Parameters` |
|    - |  775 | ` *  $hex_string` |
|    - |  776 | ` *   The hexadecimal string to convert` |
|    - |  777 | ` * Return` |
|    - |  778 | ` *  The decimal representation of hex_string` |
|    - |  779 | ` */` |
|   24 |  780 | `PH7_PRIVATE int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  781 |  |
|    - |  782 | `	const char *zString,*zEnd;` |
|    - |  783 | `	ph7_int64 iVal;` |
|    - |  784 | `	int nLen;` |
|   25 |  785 | `	if( nArg < 1 ){` |
|    - |  786 | `		/* Missing arguments,return -1 */` |
|    5 |  787 | `		ph7_result_int(pCtx,-1);` |
|    5 |  788 | `		return PH7_OK;` |
|    - |  789 | `	}` |
|   21 |  790 | `	iVal = 0;` |
|   21 |  791 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  792 | `		/* Extract the given string */` |
|   15 |  793 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    - |  794 | `		/* Delimit the string */` |
|   15 |  795 | `		zEnd = &zString[nLen];` |
|    - |  796 | `		/* Ignore non hex-stream */` |
|   21 |  797 | `		while( zString < zEnd ){` |
|   21 |  798 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|    - |  799 | `				/* UTF-8 stream */` |
|    5 |  800 | `				zString++;` |
|    9 |  801 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|    5 |  802 | `					zString++;` |
|    1 |  803 | `				}` |
|    3 |  804 | `			}else{` |
|   17 |  805 | `				if( SyisHex(zString[0]) ){` |
|   15 |  806 | `					break;` |
|    - |  807 | `				}` |
|    - |  808 | `				/* Ignore */` |
|    3 |  809 | `				zString++;` |
|    - |  810 | `			}` |
|    1 |  811 | `		}` |
|   15 |  812 | `		if( zString < zEnd ){` |
|    - |  813 | `			/* Cast */` |
|   15 |  814 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|    7 |  815 | `		}` |
|    8 |  816 | `	}else{` |
|    - |  817 | `		/* Extract as a 64-bit integer */` |
|    7 |  818 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  819 | `	}` |
|    - |  820 | `	/* Return the number */` |
|   21 |  821 | `	ph7_result_int64(pCtx,iVal);` |
|   21 |  822 | `	return PH7_OK;` |
|   13 |  823 |  |
|    - |  824 | `/*` |
|    - |  825 | ` * int64 bindec(string $bin_string)` |
|    - |  826 | ` *  Binary to decimal.` |
|    - |  827 | ` * Parameters` |
|    - |  828 | ` *  $bin_string` |
|    - |  829 | ` *   The binary string to convert` |
|    - |  830 | ` * Return` |
|    - |  831 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|    - |  832 | ` */` |
|   12 |  833 | `PH7_PRIVATE int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  834 |  |
|    - |  835 | `	const char *zString;` |
|    - |  836 | `	ph7_int64 iVal;` |
|    - |  837 | `	int nLen;` |
|   13 |  838 | `	if( nArg < 1 ){` |
|    - |  839 | `		/* Missing arguments,return -1 */` |
|    5 |  840 | `		ph7_result_int(pCtx,-1);` |
|    5 |  841 | `		return PH7_OK;` |
|    - |  842 | `	}` |
|    9 |  843 | `	iVal = 0;` |
|    9 |  844 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  845 | `		/* Extract the given string */` |
|    7 |  846 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    7 |  847 | `		if( nLen > 0 ){` |
|    - |  848 | `			/* Perform a binary cast */` |
|    5 |  849 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    2 |  850 | `		}` |
|    4 |  851 | `	}else{` |
|    - |  852 | `		/* Extract as a 64-bit integer */` |
|    3 |  853 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  854 | `	}` |
|    - |  855 | `	/* Return the number */` |
|    9 |  856 | `	ph7_result_int64(pCtx,iVal);` |
|    9 |  857 | `	return PH7_OK;` |
|    7 |  858 |  |
|    - |  859 | `/*` |
|    - |  860 | ` * int64 octdec(string $oct_string)` |
|    - |  861 | ` *  Octal to decimal.` |
|    - |  862 | ` * Parameters` |
|    - |  863 | ` *  $oct_string` |
|    - |  864 | ` *   The octal string to convert` |
|    - |  865 | ` * Return` |
|    - |  866 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|    - |  867 | ` */` |
|    6 |  868 | `PH7_PRIVATE int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  869 |  |
|    - |  870 | `	const char *zString;` |
|    - |  871 | `	ph7_int64 iVal;` |
|    - |  872 | `	int nLen;` |
|    7 |  873 | `	if( nArg < 1 ){` |
|    - |  874 | `		/* Missing arguments,return -1 */` |
|    3 |  875 | `		ph7_result_int(pCtx,-1);` |
|    3 |  876 | `		return PH7_OK;` |
|    - |  877 | `	}` |
|    5 |  878 | `	iVal = 0;` |
|    5 |  879 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  880 | `		/* Extract the given string */` |
|    3 |  881 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|    3 |  882 | `		if( nLen > 0 ){` |
|    - |  883 | `			/* Perform the cast */` |
|    3 |  884 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|    1 |  885 | `		}` |
|    2 |  886 | `	}else{` |
|    - |  887 | `		/* Extract as a 64-bit integer */` |
|    3 |  888 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|    - |  889 | `	}` |
|    - |  890 | `	/* Return the number */` |
|    5 |  891 | `	ph7_result_int64(pCtx,iVal);` |
|    5 |  892 | `	return PH7_OK;` |
|    4 |  893 |  |
|    - |  894 | `/*` |
|    - |  895 | ` * srand([int $seed])` |
|    - |  896 | ` * mt_srand([int $seed])` |
|    - |  897 | ` *  Seed the random number generator.` |
|    - |  898 | ` * Parameters` |
|    - |  899 | ` * $seed` |
|    - |  900 | ` *  Optional seed value` |
|    - |  901 | ` * Return` |
|    - |  902 | ` *  null.` |
|    - |  903 | ` * Note:` |
|    - |  904 | ` *  THIS FUNCTION IS A NO-OP.` |
|    - |  905 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|    - |  906 | ` */` |
|   20 |  907 | `PH7_PRIVATE int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  908 |  |
|   10 |  909 | `	SXUNUSED(nArg);` |
|   10 |  910 | `	SXUNUSED(apArg);` |
|   21 |  911 | `	ph7_result_null(pCtx);` |
|   21 |  912 | `	return PH7_OK;` |
|    1 |  913 |  |
|    - |  914 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - |  915 | `/*` |
|    - |  916 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|    - |  917 | ` *  Convert a number between arbitrary bases.` |
|    - |  918 | ` * Parameters` |
|    - |  919 | ` * $number` |
|    - |  920 | ` *  The number to convert` |
|    - |  921 | ` * $frombase` |
|    - |  922 | ` *  The base number is in` |
|    - |  923 | ` * $tobase` |
|    - |  924 | ` *  The base to convert number to` |
|    - |  925 | ` * Return` |
|    - |  926 | ` *  Number converted to base tobase` |
|    - |  927 | ` */` |
|   48 |  928 | `PH7_PRIVATE int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  929 |  |
|    - |  930 | `	int nLen,iFbase,iTobase;` |
|    - |  931 | `	const char *zNum;` |
|    - |  932 | `	ph7_int64 iNum;` |
|   49 |  933 | `	if( nArg < 3 ){` |
|    - |  934 | `		/* Return the empty string*/` |
|   13 |  935 | `		ph7_result_string(pCtx,"",0);` |
|   13 |  936 | `		return PH7_OK;` |
|    - |  937 | `	}` |
|    - |  938 | `	/* Base numbers */` |
|   37 |  939 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|   37 |  940 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|   37 |  941 | `	if( ph7_value_is_string(apArg[0]) ){` |
|    - |  942 | `		/* Extract the target number */` |
|   33 |  943 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|   33 |  944 | `		if( nLen < 1 ){` |
|    - |  945 | `			/* Return the empty string*/` |
|    5 |  946 | `			ph7_result_string(pCtx,"",0);` |
|    5 |  947 | `			return PH7_OK;` |
|    - |  948 | `		}` |
|    - |  949 | `		/* Base conversion */` |
|   29 |  950 | `		switch(iFbase){` |
|    5 |  951 | `		case 16:` |
|    - |  952 | `			/* Hex */` |
|   11 |  953 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|   11 |  954 | `			break;` |
|    3 |  955 | `		case 8:` |
|    - |  956 | `			/* Octal */` |
|    7 |  957 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    7 |  958 | `			break;` |
|    2 |  959 | `		case 2:` |
|    - |  960 | `			/* Binary */` |
|    5 |  961 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    5 |  962 | `			break;` |
|    4 |  963 | `		default:` |
|    - |  964 | `			/* Decimal */` |
|    9 |  965 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|    8 |  966 | `			break;` |
|    - |  967 | `		}` |
|   15 |  968 | `	}else{` |
|    5 |  969 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|    - |  970 | `	}` |
|   33 |  971 | `	switch(iTobase){` |
|    3 |  972 | `	case 16:` |
|    - |  973 | `		/* Hex */` |
|    7 |  974 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|    7 |  975 | `		break;` |
|    1 |  976 | `	case 8:` |
|    - |  977 | `		/* Octal */` |
|    3 |  978 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|    3 |  979 | `		break;` |
|    1 |  980 | `	case 2:` |
|    - |  981 | `		/* Binary */` |
|    3 |  982 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|    3 |  983 | `		break;` |
|   11 |  984 | `	default:` |
|    - |  985 | `		/* Decimal */` |
|   23 |  986 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|   22 |  987 | `		break;` |
|    - |  988 | `	}` |
|   33 |  989 | `	return PH7_OK;` |
|   25 |  990 |  |
|    - |  991 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - |  992 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  993 |  |
