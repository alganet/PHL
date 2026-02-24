# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3720/4367 lines (85.18%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |    8 | `/*` |
|      - |    9 | ` * Section:` |
|      - |   10 | ` *    Variable handling Functions.` |
|      - |   11 | ` * Status:` |
|      - |   12 | ` *    Stable.` |
|      - |   13 | ` */` |
|      - |   14 | `/*` |
|      - |   15 | ` * bool is_bool($var)` |
|      - |   16 | ` *  Finds out whether a variable is a boolean.` |
|      - |   17 | ` * Parameters` |
|      - |   18 | ` *   $var: The variable being evaluated.` |
|      - |   19 | ` * Return` |
|      - |   20 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |   21 | ` */` |
|     32 |   22 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   23 |  |
|     33 |   24 | `	int res = 0; /* Assume false by default */` |
|     33 |   25 | `	if( nArg > 0 ){` |
|     29 |   26 | `		res = ph7_value_is_bool(apArg[0]);` |
|     14 |   27 | `	}` |
|      - |   28 | `	/* Query result */` |
|     33 |   29 | `	ph7_result_bool(pCtx,res);` |
|     33 |   30 | `	return PH7_OK;` |
|      1 |   31 |  |
|      - |   32 | `/*` |
|      - |   33 | ` * bool is_float($var)` |
|      - |   34 | ` * bool is_real($var)` |
|      - |   35 | ` * bool is_double($var)` |
|      - |   36 | ` *  Finds out whether a variable is a float.` |
|      - |   37 | ` * Parameters` |
|      - |   38 | ` *   $var: The variable being evaluated.` |
|      - |   39 | ` * Return` |
|      - |   40 | ` *  TRUE if var is a float. False otherwise.` |
|      - |   41 | ` */` |
|     42 |   42 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   43 |  |
|     43 |   44 | `	int res = 0; /* Assume false by default */` |
|     43 |   45 | `	if( nArg > 0 ){` |
|     41 |   46 | `		res = ph7_value_is_float(apArg[0]);` |
|     20 |   47 | `	}` |
|      - |   48 | `	/* Query result */` |
|     43 |   49 | `	ph7_result_bool(pCtx,res);` |
|     43 |   50 | `	return PH7_OK;` |
|      1 |   51 |  |
|      - |   52 | `/*` |
|      - |   53 | ` * bool is_int($var)` |
|      - |   54 | ` * bool is_integer($var)` |
|      - |   55 | ` * bool is_long($var)` |
|      - |   56 | ` *  Finds out whether a variable is an integer.` |
|      - |   57 | ` * Parameters` |
|      - |   58 | ` *   $var: The variable being evaluated.` |
|      - |   59 | ` * Return` |
|      - |   60 | ` *  TRUE if var is an integer. False otherwise.` |
|      - |   61 | ` */` |
|     96 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   63 |  |
|     97 |   64 | `	int res = 0; /* Assume false by default */` |
|     97 |   65 | `	if( nArg > 0 ){` |
|     95 |   66 | `		res = ph7_value_is_int(apArg[0]);` |
|     47 |   67 | `	}` |
|      - |   68 | `	/* Query result */` |
|     97 |   69 | `	ph7_result_bool(pCtx,res);` |
|     97 |   70 | `	return PH7_OK;` |
|      1 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * bool is_string($var)` |
|      - |   74 | ` *  Finds out whether a variable is a string.` |
|      - |   75 | ` * Parameters` |
|      - |   76 | ` *   $var: The variable being evaluated.` |
|      - |   77 | ` * Return` |
|      - |   78 | ` *  TRUE if var is string. False otherwise.` |
|      - |   79 | ` */` |
|     56 |   80 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   81 |  |
|     57 |   82 | `	int res = 0; /* Assume false by default */` |
|     57 |   83 | `	if( nArg > 0 ){` |
|     55 |   84 | `		res = ph7_value_is_string(apArg[0]);` |
|     27 |   85 | `	}` |
|      - |   86 | `	/* Query result */` |
|     57 |   87 | `	ph7_result_bool(pCtx,res);` |
|     57 |   88 | `	return PH7_OK;` |
|      1 |   89 |  |
|      - |   90 | `/*` |
|      - |   91 | ` * bool is_null($var)` |
|      - |   92 | ` *  Finds out whether a variable is NULL.` |
|      - |   93 | ` * Parameters` |
|      - |   94 | ` *   $var: The variable being evaluated.` |
|      - |   95 | ` * Return` |
|      - |   96 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |   97 | ` */` |
|     24 |   98 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   99 |  |
|     25 |  100 | `	int res = 0; /* Assume false by default */` |
|     25 |  101 | `	if( nArg > 0 ){` |
|     23 |  102 | `		res = ph7_value_is_null(apArg[0]);` |
|     11 |  103 | `	}` |
|      - |  104 | `	/* Query result */` |
|     25 |  105 | `	ph7_result_bool(pCtx,res);` |
|     25 |  106 | `	return PH7_OK;` |
|      1 |  107 |  |
|      - |  108 | `/*` |
|      - |  109 | ` * bool is_numeric($var)` |
|      - |  110 | ` *  Find out whether a variable is NULL.` |
|      - |  111 | ` * Parameters` |
|      - |  112 | ` *  $var: The variable being evaluated.` |
|      - |  113 | ` * Return` |
|      - |  114 | ` *  True if var is numeric. False otherwise.` |
|      - |  115 | ` */` |
|     28 |  116 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  117 |  |
|     30 |  118 | `	int res = 0; /* Assume false by default */` |
|     30 |  119 | `	if( nArg > 0 ){` |
|     28 |  120 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     13 |  121 | `	}` |
|      - |  122 | `	/* Query result */` |
|     30 |  123 | `	ph7_result_bool(pCtx,res);` |
|     30 |  124 | `	return PH7_OK;` |
|      2 |  125 |  |
|      - |  126 | `/*` |
|      - |  127 | ` * bool is_scalar($var)` |
|      - |  128 | ` *  Find out whether a variable is a scalar.` |
|      - |  129 | ` * Parameters` |
|      - |  130 | ` *  $var: The variable being evaluated.` |
|      - |  131 | ` * Return` |
|      - |  132 | ` *  True if var is scalar. False otherwise.` |
|      - |  133 | ` */` |
|     14 |  134 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  135 |  |
|     15 |  136 | `	int res = 0; /* Assume false by default */` |
|     15 |  137 | `	if( nArg > 0 ){` |
|     13 |  138 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  139 | `	}` |
|      - |  140 | `	/* Query result */` |
|     15 |  141 | `	ph7_result_bool(pCtx,res);` |
|     15 |  142 | `	return PH7_OK;` |
|      1 |  143 |  |
|      - |  144 | `/*` |
|      - |  145 | ` * bool is_array($var)` |
|      - |  146 | ` *  Find out whether a variable is an array.` |
|      - |  147 | ` * Parameters` |
|      - |  148 | ` *  $var: The variable being evaluated.` |
|      - |  149 | ` * Return` |
|      - |  150 | ` *  True if var is an array. False otherwise.` |
|      - |  151 | ` */` |
|     94 |  152 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  153 |  |
|     96 |  154 | `	int res = 0; /* Assume false by default */` |
|     96 |  155 | `	if( nArg > 0 ){` |
|     94 |  156 | `		res = ph7_value_is_array(apArg[0]);` |
|     46 |  157 | `	}` |
|      - |  158 | `	/* Query result */` |
|     96 |  159 | `	ph7_result_bool(pCtx,res);` |
|     96 |  160 | `	return PH7_OK;` |
|      2 |  161 |  |
|      - |  162 | `/*` |
|      - |  163 | ` * bool is_object($var)` |
|      - |  164 | ` *  Find out whether a variable is an object.` |
|      - |  165 | ` * Parameters` |
|      - |  166 | ` *  $var: The variable being evaluated.` |
|      - |  167 | ` * Return` |
|      - |  168 | ` *  True if var is an object. False otherwise.` |
|      - |  169 | ` */` |
|     20 |  170 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  171 |  |
|     21 |  172 | `	int res = 0; /* Assume false by default */` |
|     21 |  173 | `	if( nArg > 0 ){` |
|     19 |  174 | `		res = ph7_value_is_object(apArg[0]);` |
|      9 |  175 | `	}` |
|      - |  176 | `	/* Query result */` |
|     21 |  177 | `	ph7_result_bool(pCtx,res);` |
|     21 |  178 | `	return PH7_OK;` |
|      1 |  179 |  |
|      - |  180 | `/*` |
|      - |  181 | ` * bool is_resource($var)` |
|      - |  182 | ` *  Find out whether a variable is a resource.` |
|      - |  183 | ` * Parameters` |
|      - |  184 | ` *  $var: The variable being evaluated.` |
|      - |  185 | ` * Return` |
|      - |  186 | ` *  True if a resource. False otherwise.` |
|      - |  187 | ` */` |
|     60 |  188 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  189 |  |
|     62 |  190 | `	int res = 0; /* Assume false by default */` |
|     62 |  191 | `	if( nArg > 0 ){` |
|     60 |  192 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  193 | `	}` |
|     62 |  194 | `	ph7_result_bool(pCtx,res);` |
|     62 |  195 | `	return PH7_OK;` |
|      2 |  196 |  |
|      - |  197 | `/*` |
|      - |  198 | ` * float floatval($var)` |
|      - |  199 | ` *  Get float value of a variable.` |
|      - |  200 | ` * Parameter` |
|      - |  201 | ` *  $var: The variable being processed.` |
|      - |  202 | ` * Return` |
|      - |  203 | ` *  the float value of a variable.` |
|      - |  204 | ` */` |
|      6 |  205 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  206 |  |
|      7 |  207 | `	if( nArg < 1 ){` |
|      - |  208 | `		/* return 0.0 */` |
|      3 |  209 | `		ph7_result_double(pCtx,0);` |
|      2 |  210 | `	}else{` |
|      - |  211 | `		double dval;` |
|      - |  212 | `		/* Perform the cast */` |
|      5 |  213 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  214 | `		ph7_result_double(pCtx,dval);` |
|      - |  215 | `	}` |
|      7 |  216 | `	return PH7_OK;` |
|      1 |  217 |  |
|      - |  218 | `/*` |
|      - |  219 | ` * int intval($var)` |
|      - |  220 | ` *  Get integer value of a variable.` |
|      - |  221 | ` * Parameter` |
|      - |  222 | ` *  $var: The variable being processed.` |
|      - |  223 | ` * Return` |
|      - |  224 | ` *  the int value of a variable.` |
|      - |  225 | ` */` |
|      6 |  226 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  227 |  |
|      7 |  228 | `	if( nArg < 1 ){` |
|      - |  229 | `		/* return 0 */` |
|      3 |  230 | `		ph7_result_int(pCtx,0);` |
|      2 |  231 | `	}else{` |
|      - |  232 | `		sxi64 iVal;` |
|      - |  233 | `		/* Perform the cast */` |
|      5 |  234 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      5 |  235 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  236 | `	}` |
|      7 |  237 | `	return PH7_OK;` |
|      1 |  238 |  |
|      - |  239 | `/*` |
|      - |  240 | ` * string strval($var)` |
|      - |  241 | ` *  Get the string representation of a variable.` |
|      - |  242 | ` * Parameter` |
|      - |  243 | ` *  $var: The variable being processed.` |
|      - |  244 | ` * Return` |
|      - |  245 | ` *  the string value of a variable.` |
|      - |  246 | ` */` |
|      4 |  247 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  248 |  |
|      5 |  249 | `	if( nArg < 1 ){` |
|      - |  250 | `		/* return NULL */` |
|      3 |  251 | `		ph7_result_null(pCtx);` |
|      2 |  252 | `	}else{` |
|      - |  253 | `		const char *zVal;` |
|      3 |  254 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  255 | `		/* Perform the cast */` |
|      3 |  256 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  257 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  258 | `	}` |
|      5 |  259 | `	return PH7_OK;` |
|      1 |  260 |  |
|      - |  261 | `/*` |
|      - |  262 | ` * bool empty($var)` |
|      - |  263 | ` *  Determine whether a variable is empty.` |
|      - |  264 | ` * Parameters` |
|      - |  265 | ` *   $var: The variable being checked.` |
|      - |  266 | ` * Return` |
|      - |  267 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  268 | ` */` |
|  15470 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  15472 |  271 | `	int res = 1; /* Assume empty by default */` |
|  15472 |  272 | `	if( nArg > 0 ){` |
|  15470 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   7734 |  274 | `	}` |
|  15472 |  275 | `	ph7_result_bool(pCtx,res);` |
|  15472 |  276 | `	return PH7_OK;` |
|      - |  277 |  |
|      2 |  278 |  |
|      - |  279 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  280 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  281 | `#endif` |
|      - |  282 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  283 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  284 | `#endif` |
|      - |  285 |  |
|      - |  286 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  287 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  288 |  |
|      - |  289 | `/*` |
|      - |  290 | ` * Section:` |
|      - |  291 | ` *    Math Functions.` |
|      - |  292 |  |
|      - |  293 | ` * Status:` |
|      - |  294 | ` *    Stable.` |
|      - |  295 | ` */` |
|      - |  296 | `#include <stdlib.h> /* abs */` |
|      - |  297 | `#include <math.h>` |
|      - |  298 | `/*` |
|      - |  299 | ` * float sqrt(float $arg )` |
|      - |  300 | ` *  Square root of the given number.` |
|      - |  301 | ` * Parameter` |
|      - |  302 | ` *  The number to process.` |
|      - |  303 | ` * Return` |
|      - |  304 | ` *  The square root of arg or the special value Nan of failure.` |
|      - |  305 | ` */` |
|      6 |  306 | `static int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  307 |  |
|      - |  308 | `	double r,x;` |
|      7 |  309 | `	if( nArg < 1 ){` |
|      - |  310 | `		/* Missing argument,return 0 */` |
|      5 |  311 | `		ph7_result_int(pCtx,0);` |
|      5 |  312 | `		return PH7_OK;` |
|      - |  313 | `	}` |
|      3 |  314 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  315 | `	/* Perform the requested operation */` |
|      3 |  316 | `	r = sqrt(x);` |
|      - |  317 | `	/* store the result back */` |
|      3 |  318 | `	ph7_result_double(pCtx,r);` |
|      3 |  319 | `	return PH7_OK;` |
|      4 |  320 |  |
|      - |  321 | `/*` |
|      - |  322 | ` * float exp(float $arg )` |
|      - |  323 | ` *  Calculates the exponent of e.` |
|      - |  324 | ` * Parameter` |
|      - |  325 | ` *  The number to process.` |
|      - |  326 | ` * Return` |
|      - |  327 | ` *  'e' raised to the power of arg.` |
|      - |  328 | ` */` |
|     20 |  329 | `static int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  330 |  |
|      - |  331 | `	double r,x;` |
|     21 |  332 | `	if( nArg < 1 ){` |
|      - |  333 | `		/* Missing argument,return 0 */` |
|      3 |  334 | `		ph7_result_int(pCtx,0);` |
|      3 |  335 | `		return PH7_OK;` |
|      - |  336 | `	}` |
|     19 |  337 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  338 | `	/* Perform the requested operation */` |
|     19 |  339 | `	r = exp(x);` |
|      - |  340 | `	/* store the result back */` |
|     19 |  341 | `	ph7_result_double(pCtx,r);` |
|     19 |  342 | `	return PH7_OK;` |
|     11 |  343 |  |
|      - |  344 | `/*` |
|      - |  345 | ` * float floor(float $arg )` |
|      - |  346 | ` *  Round fractions down.` |
|      - |  347 | ` * Parameter` |
|      - |  348 | ` *  The number to process.` |
|      - |  349 | ` * Return` |
|      - |  350 | ` *  Returns the next lowest integer value by rounding down value if necessary.` |
|      - |  351 | ` */` |
|      4 |  352 | `static int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  353 |  |
|      - |  354 | `	double r,x;` |
|      5 |  355 | `	if( nArg < 1 ){` |
|      - |  356 | `		/* Missing argument,return 0 */` |
|      3 |  357 | `		ph7_result_int(pCtx,0);` |
|      3 |  358 | `		return PH7_OK;` |
|      - |  359 | `	}` |
|      3 |  360 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  361 | `	/* Perform the requested operation */` |
|      3 |  362 | `	r = floor(x);` |
|      - |  363 | `	/* store the result back */` |
|      3 |  364 | `	ph7_result_double(pCtx,r);` |
|      3 |  365 | `	return PH7_OK;` |
|      3 |  366 |  |
|      - |  367 | `/*` |
|      - |  368 | ` * float cos(float $arg )` |
|      - |  369 | ` *  Cosine.` |
|      - |  370 | ` * Parameter` |
|      - |  371 | ` *  The number to process.` |
|      - |  372 | ` * Return` |
|      - |  373 | ` *  The cosine of arg.` |
|      - |  374 | ` */` |
|      4 |  375 | `static int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  376 |  |
|      - |  377 | `	double r,x;` |
|      5 |  378 | `	if( nArg < 1 ){` |
|      - |  379 | `		/* Missing argument,return 0 */` |
|      3 |  380 | `		ph7_result_int(pCtx,0);` |
|      3 |  381 | `		return PH7_OK;` |
|      - |  382 | `	}` |
|      3 |  383 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  384 | `	/* Perform the requested operation */` |
|      3 |  385 | `	r = cos(x);` |
|      - |  386 | `	/* store the result back */` |
|      3 |  387 | `	ph7_result_double(pCtx,r);` |
|      3 |  388 | `	return PH7_OK;` |
|      3 |  389 |  |
|      - |  390 | `/*` |
|      - |  391 | ` * float acos(float $arg )` |
|      - |  392 | ` *  Arc cosine.` |
|      - |  393 | ` * Parameter` |
|      - |  394 | ` *  The number to process.` |
|      - |  395 | ` * Return` |
|      - |  396 | ` *  The arc cosine of arg.` |
|      - |  397 | ` */` |
|     22 |  398 | `static int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  399 |  |
|      - |  400 | `	double r, x;` |
|      - |  401 | `	/* PHP enforces exactly one argument and a floatable parameter. */` |
|     24 |  402 | `	if( nArg != 1 ){` |
|      4 |  403 | `		return PH7_VmThrowException(pCtx,` |
|      - |  404 | `			"ArgumentCountError",` |
|      - |  405 | `			"acos() expects exactly 1 argument, %d given",` |
|      1 |  406 | `			nArg` |
|      - |  407 | `			);` |
|      - |  408 | `	}` |
|      - |  409 | `	/* Type checking: reject non-numeric values (arrays, objects, resources, strings)` |
|      - |  410 | `	 * PHP8 reports a TypeError for wrong types.  Numeric strings are allowed but` |
|      - |  411 | `	 * the float conversion will handle them. */` |
|     22 |  412 | `	if( !ph7_value_is_numeric(apArg[0]) ){` |
|      7 |  413 | `		return PH7_VmThrowException(pCtx,` |
|      - |  414 | `			"TypeError",` |
|      - |  415 | `			"acos(): Argument #1 ($num) must be of type float, %s given",` |
|      2 |  416 | `			ph7_type_name(apArg[0])` |
|      - |  417 | `			);` |
|      - |  418 | `	}` |
|      - |  419 | `	/* Convert to double now that we know it's numeric. */` |
|     17 |  420 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  421 | `	/* Handle domain error ourselves.  PHP returns NaN for \|x\|>1. */` |
|     17 |  422 | `	if( x < -1.0 \|\| x > 1.0 ){` |
|      5 |  423 | `		r = PH7_NAN_VALUE();` |
|      3 |  424 | `	}else{` |
|     13 |  425 | `		r = acos(x);` |
|      - |  426 | `	}` |
|      - |  427 | `	/* store the result back */` |
|     17 |  428 | `	ph7_result_double(pCtx,r);` |
|     17 |  429 | `	return PH7_OK;` |
|     13 |  430 |  |
|      - |  431 | `/*` |
|      - |  432 | ` * float cosh(float $arg )` |
|      - |  433 | ` *  Hyperbolic cosine.` |
|      - |  434 | ` * Parameter` |
|      - |  435 | ` *  The number to process.` |
|      - |  436 | ` * Return` |
|      - |  437 | ` *  The hyperbolic cosine of arg.` |
|      - |  438 | ` */` |
|     18 |  439 | `static int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  440 |  |
|      - |  441 | `	double r,x;` |
|     19 |  442 | `	if( nArg < 1 ){` |
|      - |  443 | `		/* Missing argument,return 0 */` |
|      3 |  444 | `		ph7_result_int(pCtx,0);` |
|      3 |  445 | `		return PH7_OK;` |
|      - |  446 | `	}` |
|     17 |  447 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  448 | `	/* Perform the requested operation */` |
|     17 |  449 | `	r = cosh(x);` |
|      - |  450 | `	/* store the result back */` |
|     17 |  451 | `	ph7_result_double(pCtx,r);` |
|     17 |  452 | `	return PH7_OK;` |
|     10 |  453 |  |
|      - |  454 | `/*` |
|      - |  455 | ` * float sin(float $arg )` |
|      - |  456 | ` *  Sine.` |
|      - |  457 | ` * Parameter` |
|      - |  458 | ` *  The number to process.` |
|      - |  459 | ` * Return` |
|      - |  460 | ` *  The sine of arg.` |
|      - |  461 | ` */` |
|      8 |  462 | `static int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  463 |  |
|      - |  464 | `	double r,x;` |
|      9 |  465 | `	if( nArg < 1 ){` |
|      - |  466 | `		/* Missing argument,return 0 */` |
|      7 |  467 | `		ph7_result_int(pCtx,0);` |
|      7 |  468 | `		return PH7_OK;` |
|      - |  469 | `	}` |
|      3 |  470 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  471 | `	/* Perform the requested operation */` |
|      3 |  472 | `	r = sin(x);` |
|      - |  473 | `	/* store the result back */` |
|      3 |  474 | `	ph7_result_double(pCtx,r);` |
|      3 |  475 | `	return PH7_OK;` |
|      5 |  476 |  |
|      - |  477 | `/*` |
|      - |  478 | ` * float asin(float $arg )` |
|      - |  479 | ` *  Arc sine.` |
|      - |  480 | ` * Parameter` |
|      - |  481 | ` *  The number to process.` |
|      - |  482 | ` * Return` |
|      - |  483 | ` *  The arc sine of arg.` |
|      - |  484 | ` */` |
|     14 |  485 | `static int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  486 |  |
|      - |  487 | `	double r,x;` |
|     15 |  488 | `	if( nArg < 1 ){` |
|      - |  489 | `		/* Missing argument,return 0 */` |
|      3 |  490 | `		ph7_result_int(pCtx,0);` |
|      3 |  491 | `		return PH7_OK;` |
|      - |  492 | `	}` |
|     13 |  493 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  494 | `	/* Perform the requested operation */` |
|     13 |  495 | `	r = asin(x);` |
|      - |  496 | `	/* store the result back */` |
|     13 |  497 | `	ph7_result_double(pCtx,r);` |
|     13 |  498 | `	return PH7_OK;` |
|      8 |  499 |  |
|      - |  500 | `/*` |
|      - |  501 | ` * float sinh(float $arg )` |
|      - |  502 | ` *  Hyperbolic sine.` |
|      - |  503 | ` * Parameter` |
|      - |  504 | ` *  The number to process.` |
|      - |  505 | ` * Return` |
|      - |  506 | ` *  The hyperbolic sine of arg.` |
|      - |  507 | ` */` |
|     20 |  508 | `static int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  509 |  |
|      - |  510 | `	double r,x;` |
|     21 |  511 | `	if( nArg < 1 ){` |
|      - |  512 | `		/* Missing argument,return 0 */` |
|      3 |  513 | `		ph7_result_int(pCtx,0);` |
|      3 |  514 | `		return PH7_OK;` |
|      - |  515 | `	}` |
|     19 |  516 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  517 | `	/* Perform the requested operation */` |
|     19 |  518 | `	r = sinh(x);` |
|      - |  519 | `	/* store the result back */` |
|     19 |  520 | `	ph7_result_double(pCtx,r);` |
|     19 |  521 | `	return PH7_OK;` |
|     11 |  522 |  |
|      - |  523 | `/*` |
|      - |  524 | ` * float ceil(float $arg )` |
|      - |  525 | ` *  Round fractions up.` |
|      - |  526 | ` * Parameter` |
|      - |  527 | ` *  The number to process.` |
|      - |  528 | ` * Return` |
|      - |  529 | ` *  The next highest integer value by rounding up value if necessary.` |
|      - |  530 | ` */` |
|      6 |  531 | `static int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  532 |  |
|      - |  533 | `	double r,x;` |
|      7 |  534 | `	if( nArg < 1 ){` |
|      - |  535 | `		/* Missing argument,return 0 */` |
|      5 |  536 | `		ph7_result_int(pCtx,0);` |
|      5 |  537 | `		return PH7_OK;` |
|      - |  538 | `	}` |
|      3 |  539 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  540 | `	/* Perform the requested operation */` |
|      3 |  541 | `	r = ceil(x);` |
|      - |  542 | `	/* store the result back */` |
|      3 |  543 | `	ph7_result_double(pCtx,r);` |
|      3 |  544 | `	return PH7_OK;` |
|      4 |  545 |  |
|      - |  546 | `/*` |
|      - |  547 | ` * float tan(float $arg )` |
|      - |  548 | ` *  Tangent.` |
|      - |  549 | ` * Parameter` |
|      - |  550 | ` *  The number to process.` |
|      - |  551 | ` * Return` |
|      - |  552 | ` *  The tangent of arg.` |
|      - |  553 | ` */` |
|      6 |  554 | `static int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  555 |  |
|      - |  556 | `	double r,x;` |
|      7 |  557 | `	if( nArg < 1 ){` |
|      - |  558 | `		/* Missing argument,return 0 */` |
|      3 |  559 | `		ph7_result_int(pCtx,0);` |
|      3 |  560 | `		return PH7_OK;` |
|      - |  561 | `	}` |
|      5 |  562 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  563 | `	/* Perform the requested operation */` |
|      5 |  564 | `	r = tan(x);` |
|      - |  565 | `	/* store the result back */` |
|      5 |  566 | `	ph7_result_double(pCtx,r);` |
|      5 |  567 | `	return PH7_OK;` |
|      4 |  568 |  |
|      - |  569 | `/*` |
|      - |  570 | ` * float atan(float $arg )` |
|      - |  571 | ` *  Arc tangent.` |
|      - |  572 | ` * Parameter` |
|      - |  573 | ` *  The number to process.` |
|      - |  574 | ` * Return` |
|      - |  575 | ` *  The arc tangent of arg.` |
|      - |  576 | ` */` |
|     16 |  577 | `static int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  578 |  |
|      - |  579 | `	double r,x;` |
|     17 |  580 | `	if( nArg < 1 ){` |
|      - |  581 | `		/* Missing argument,return 0 */` |
|      5 |  582 | `		ph7_result_int(pCtx,0);` |
|      5 |  583 | `		return PH7_OK;` |
|      - |  584 | `	}` |
|     13 |  585 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  586 | `	/* Perform the requested operation */` |
|     13 |  587 | `	r = atan(x);` |
|      - |  588 | `	/* store the result back */` |
|     13 |  589 | `	ph7_result_double(pCtx,r);` |
|     13 |  590 | `	return PH7_OK;` |
|      9 |  591 |  |
|      - |  592 | `/*` |
|      - |  593 | ` * float tanh(float $arg )` |
|      - |  594 | ` *  Hyperbolic tangent.` |
|      - |  595 | ` * Parameter` |
|      - |  596 | ` *  The number to process.` |
|      - |  597 | ` * Return` |
|      - |  598 | ` *  The Hyperbolic tangent of arg.` |
|      - |  599 | ` */` |
|     20 |  600 | `static int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  601 |  |
|      - |  602 | `	double r,x;` |
|     21 |  603 | `	if( nArg < 1 ){` |
|      - |  604 | `		/* Missing argument,return 0 */` |
|      3 |  605 | `		ph7_result_int(pCtx,0);` |
|      3 |  606 | `		return PH7_OK;` |
|      - |  607 | `	}` |
|     19 |  608 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  609 | `	/* Perform the requested operation */` |
|     19 |  610 | `	r = tanh(x);` |
|      - |  611 | `	/* store the result back */` |
|     19 |  612 | `	ph7_result_double(pCtx,r);` |
|     19 |  613 | `	return PH7_OK;` |
|     11 |  614 |  |
|      - |  615 | `/*` |
|      - |  616 | ` * float atan2(float $y,float $x)` |
|      - |  617 | ` *  Arc tangent of two variable.` |
|      - |  618 | ` * Parameter` |
|      - |  619 | ` *  $y = Dividend parameter.` |
|      - |  620 | ` *  $x = Divisor parameter.` |
|      - |  621 | ` * Return` |
|      - |  622 | ` *  The arc tangent of y/x in radian.` |
|      - |  623 | ` */` |
|     10 |  624 | `static int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  625 |  |
|      - |  626 | `	double r,x,y;` |
|     11 |  627 | `	if( nArg < 2 ){` |
|      - |  628 | `		/* Missing arguments,return 0 */` |
|      5 |  629 | `		ph7_result_int(pCtx,0);` |
|      5 |  630 | `		return PH7_OK;` |
|      - |  631 | `	}` |
|      7 |  632 | `	y = ph7_value_to_double(apArg[0]);` |
|      7 |  633 | `	x = ph7_value_to_double(apArg[1]);` |
|      - |  634 | `	/* Perform the requested operation */` |
|      7 |  635 | `	r = atan2(y,x);` |
|      - |  636 | `	/* store the result back */` |
|      7 |  637 | `	ph7_result_double(pCtx,r);` |
|      7 |  638 | `	return PH7_OK;` |
|      6 |  639 |  |
|      - |  640 | `/*` |
|      - |  641 | ` * float/int64 abs(float/int64 $arg )` |
|      - |  642 | ` *  Absolute value.` |
|      - |  643 | ` * Parameter` |
|      - |  644 | ` *  The number to process.` |
|      - |  645 | ` * Return` |
|      - |  646 | ` *  The absolute value of number.` |
|      - |  647 | ` */` |
|    100 |  648 | `static int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  649 |  |
|      - |  650 | `	int is_float;` |
|      - |  651 | `	/* PHP requires exactly one argument. */` |
|    102 |  652 | `	if( nArg != 1 ){` |
|     11 |  653 | `		return PH7_VmThrowException(pCtx,` |
|      - |  654 | `			"ArgumentCountError",` |
|      - |  655 | `			"abs() expects exactly 1 argument, %d given",` |
|      3 |  656 | `			nArg` |
|      - |  657 | `			);` |
|      - |  658 | `	}` |
|      - |  659 |  |
|      - |  660 | `	/* Numeric strings with decimal/exponent are treated as real values. */` |
|     96 |  661 | `	is_float = ph7_value_is_float(apArg[0]);` |
|     96 |  662 | `	if( !is_float && ph7_value_is_string(apArg[0]) ){` |
|      - |  663 | `		int len;` |
|     10 |  664 | `		sxu8 bReal = FALSE;` |
|     10 |  665 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|      - |  666 | `		sxi32 rcNum;` |
|     10 |  667 | `		rcNum = SyStrIsNumeric(zStr, len, &bReal, 0);` |
|     10 |  668 | `		if( rcNum != SXRET_OK ){` |
|      3 |  669 | `			return PH7_VmThrowException(pCtx,` |
|      - |  670 | `				"TypeError",` |
|      - |  671 | `				"abs(): Argument #1 ($num) must be of type int\|float, string given"` |
|      - |  672 | `				);` |
|      - |  673 | `		}` |
|      7 |  674 | `		if( bReal ){` |
|      5 |  675 | `			is_float = 1;` |
|      2 |  676 | `		}` |
|      3 |  677 | `	}` |
|     94 |  678 | `	if( is_float ){` |
|      - |  679 | `		double r,x;` |
|     77 |  680 | `		x = ph7_value_to_double(apArg[0]);` |
|      - |  681 | `		/* Perform the requested operation */` |
|     77 |  682 | `		r = fabs(x);` |
|     77 |  683 | `		ph7_result_double(pCtx,r);` |
|     39 |  684 | `	}else{` |
|      - |  685 | `		int r,x;` |
|     18 |  686 | `		x = ph7_value_to_int(apArg[0]);` |
|      - |  687 | `		/* Perform the requested operation */` |
|     18 |  688 | `		r = abs(x);` |
|     18 |  689 | `		ph7_result_int(pCtx,r);` |
|      - |  690 | `	}` |
|     94 |  691 | `	return PH7_OK;` |
|     52 |  692 |  |
|      - |  693 | `/*` |
|      - |  694 | ` * float log(float $arg,[int/float $base])` |
|      - |  695 | ` *  Natural logarithm.` |
|      - |  696 | ` * Parameter` |
|      - |  697 | ` *  $arg: The number to process.` |
|      - |  698 | ` *  $base: The optional logarithmic base to use. (only base-10 is supported)` |
|      - |  699 | ` * Return` |
|      - |  700 | ` *  The logarithm of arg to base, if given, or the natural logarithm.` |
|      - |  701 | ` * Note:` |
|      - |  702 | ` *  only Natural log and base-10 log are supported.` |
|      - |  703 | ` */` |
|     14 |  704 | `static int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  705 |  |
|      - |  706 | `	double r,x;` |
|     15 |  707 | `	if( nArg < 1 ){` |
|      - |  708 | `		/* Missing argument,return 0 */` |
|      3 |  709 | `		ph7_result_int(pCtx,0);` |
|      3 |  710 | `		return PH7_OK;` |
|      - |  711 | `	}` |
|     13 |  712 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  713 | `	/* Perform the requested operation */` |
|     13 |  714 | `	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){` |
|      - |  715 | `		/* Base-10 log */` |
|      5 |  716 | `		r = log10(x);` |
|      3 |  717 | `	}else{` |
|      9 |  718 | `		r = log(x);` |
|      - |  719 | `	}` |
|      - |  720 | `	/* store the result back */` |
|     13 |  721 | `	ph7_result_double(pCtx,r);` |
|     13 |  722 | `	return PH7_OK;` |
|      8 |  723 |  |
|      - |  724 | `/*` |
|      - |  725 | ` * float log10(float $arg )` |
|      - |  726 | ` *  Base-10 logarithm.` |
|      - |  727 | ` * Parameter` |
|      - |  728 | ` *  The number to process.` |
|      - |  729 | ` * Return` |
|      - |  730 | ` *  The Base-10 logarithm of the given number.` |
|      - |  731 | ` */` |
|     16 |  732 | `static int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  733 |  |
|      - |  734 | `	double r,x;` |
|     17 |  735 | `	if( nArg < 1 ){` |
|      - |  736 | `		/* Missing argument,return 0 */` |
|      3 |  737 | `		ph7_result_int(pCtx,0);` |
|      3 |  738 | `		return PH7_OK;` |
|      - |  739 | `	}` |
|     15 |  740 | `	x = ph7_value_to_double(apArg[0]);` |
|      - |  741 | `	/* Perform the requested operation */` |
|     15 |  742 | `	r = log10(x);` |
|      - |  743 | `	/* store the result back */` |
|     15 |  744 | `	ph7_result_double(pCtx,r);` |
|     15 |  745 | `	return PH7_OK;` |
|      9 |  746 |  |
|      - |  747 | `/*` |
|      - |  748 | ` * number pow(number $base,number $exp)` |
|      - |  749 | ` *  Exponential expression.` |
|      - |  750 | ` * Parameter` |
|      - |  751 | ` *  base` |
|      - |  752 | ` *  The base to use.` |
|      - |  753 | ` * exp` |
|      - |  754 | ` *  The exponent.` |
|      - |  755 | ` * Return` |
|      - |  756 | ` *  base raised to the power of exp.` |
|      - |  757 | ` *  If the result can be represented as integer it will be returned` |
|      - |  758 | ` *  as type integer, else it will be returned as type float.` |
|      - |  759 | ` */` |
|      8 |  760 | `static int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  761 |  |
|      - |  762 | `	double r,x,y;` |
|      9 |  763 | `	if( nArg < 1 ){` |
|      - |  764 | `		/* Missing argument,return 0 */` |
|      5 |  765 | `		ph7_result_int(pCtx,0);` |
|      5 |  766 | `		return PH7_OK;` |
|      - |  767 | `	}` |
|      5 |  768 | `	x = ph7_value_to_double(apArg[0]);` |
|      5 |  769 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  770 | `	/* Perform the requested operation */` |
|      5 |  771 | `	r = pow(x,y);` |
|      5 |  772 | `	ph7_result_double(pCtx,r);` |
|      5 |  773 | `	return PH7_OK;` |
|      5 |  774 |  |
|      - |  775 | `/*` |
|      - |  776 | ` * float pi(void)` |
|      - |  777 | ` *  Returns an approximation of pi.` |
|      - |  778 | ` * Note` |
|      - |  779 | ` *  you can use the M_PI constant which yields identical results to pi().` |
|      - |  780 | ` * Return` |
|      - |  781 | ` *  The value of pi as float.` |
|      - |  782 | ` */` |
|      2 |  783 | `static int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  784 |  |
|      1 |  785 | `	SXUNUSED(nArg); /* cc warning */` |
|      1 |  786 | `	SXUNUSED(apArg);` |
|      3 |  787 | `	ph7_result_double(pCtx,PH7_PI);` |
|      3 |  788 | `	return PH7_OK;` |
|      1 |  789 |  |
|      - |  790 | `/*` |
|      - |  791 | ` * float fmod(float $x,float $y)` |
|      - |  792 | ` *  Returns the floating point remainder (modulo) of the division of the arguments.` |
|      - |  793 | ` * Parameters` |
|      - |  794 | ` * $x` |
|      - |  795 | ` *  The dividend` |
|      - |  796 | ` * $y` |
|      - |  797 | ` *  The divisor` |
|      - |  798 | ` * Return` |
|      - |  799 | ` *  The floating point remainder of x/y.` |
|      - |  800 | ` */` |
|      8 |  801 | `static int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  802 |  |
|      - |  803 | `	double x,y,r;` |
|      9 |  804 | `	if( nArg < 2 ){` |
|      - |  805 | `		/* Missing arguments */` |
|      7 |  806 | `		ph7_result_double(pCtx,0);` |
|      7 |  807 | `		return PH7_OK;` |
|      - |  808 | `	}` |
|      - |  809 | `	/* Extract given arguments */` |
|      3 |  810 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  811 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  812 | `	/* Perform the requested operation */` |
|      3 |  813 | `	r = fmod(x,y);` |
|      - |  814 | `	/* Processing result */` |
|      3 |  815 | `	ph7_result_double(pCtx,r);` |
|      3 |  816 | `	return PH7_OK;` |
|      5 |  817 |  |
|      - |  818 | `/*` |
|      - |  819 | ` * float hypot(float $x,float $y)` |
|      - |  820 | ` *  Calculate the length of the hypotenuse of a right-angle triangle .` |
|      - |  821 | ` * Parameters` |
|      - |  822 | ` * $x` |
|      - |  823 | ` *  Length of first side` |
|      - |  824 | ` * $y` |
|      - |  825 | ` *  Length of first side` |
|      - |  826 | ` * Return` |
|      - |  827 | ` *  Calculated length of the hypotenuse.` |
|      - |  828 | ` */` |
|      6 |  829 | `static int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  830 |  |
|      - |  831 | `	double x,y,r;` |
|      7 |  832 | `	if( nArg < 2 ){` |
|      - |  833 | `		/* Missing arguments */` |
|      5 |  834 | `		ph7_result_double(pCtx,0);` |
|      5 |  835 | `		return PH7_OK;` |
|      - |  836 | `	}` |
|      - |  837 | `	/* Extract given arguments */` |
|      3 |  838 | `	x = ph7_value_to_double(apArg[0]);` |
|      3 |  839 | `	y = ph7_value_to_double(apArg[1]);` |
|      - |  840 | `	/* Perform the requested operation */` |
|      3 |  841 | `	r = hypot(x,y);` |
|      - |  842 | `	/* Processing result */` |
|      3 |  843 | `	ph7_result_double(pCtx,r);` |
|      3 |  844 | `	return PH7_OK;` |
|      4 |  845 |  |
|      - |  846 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  847 | `/*` |
|      - |  848 | ` * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )` |
|      - |  849 | ` *  Exponential expression.` |
|      - |  850 | ` * Parameter` |
|      - |  851 | ` *  $val` |
|      - |  852 | ` *   The value to round.` |
|      - |  853 | ` * $precision` |
|      - |  854 | ` *   The optional number of decimal digits to round to.` |
|      - |  855 | ` * $mode` |
|      - |  856 | ` *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.` |
|      - |  857 | ` *   (not supported).` |
|      - |  858 | ` * Return` |
|      - |  859 | ` *  The rounded value.` |
|      - |  860 | ` */` |
|     20 |  861 | `static int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  862 |  |
|     21 |  863 | `	int n = 0;` |
|      - |  864 | `	double r;` |
|     21 |  865 | `	if( nArg < 1 ){` |
|      - |  866 | `		/* Missing argument,return 0 */` |
|      5 |  867 | `		ph7_result_int(pCtx,0);` |
|      5 |  868 | `		return PH7_OK;` |
|      - |  869 | `	}` |
|      - |  870 | `	/* Extract the precision if available */` |
|     17 |  871 | `	if( nArg > 1 ){` |
|      5 |  872 | `		n = ph7_value_to_int(apArg[1]);` |
|      5 |  873 | `		if( n>30 ){` |
|      3 |  874 | `			n = 30;` |
|      1 |  875 | `		}` |
|      5 |  876 | `		if( n<0 ){` |
|      3 |  877 | `			n = 0;` |
|      1 |  878 | `		}` |
|      2 |  879 | `	}` |
|     17 |  880 | `	r = ph7_value_to_double(apArg[0]);` |
|      - |  881 | `	/* If Y==0 and X will fit in a 64-bit int,` |
|      - |  882 | `     * handle the rounding directly.Otherwise` |
|      - |  883 | `	 * use our own cutsom printf [i.e:SyBufferFormat()].` |
|      - |  884 | `     */` |
|     17 |  885 | `	if( n==0 && r>=0 && r < (double)(LARGEST_INT64-1) ){` |
|     13 |  886 | `    r = (double)((ph7_int64)(r+0.5));` |
|     11 |  887 | `	}else if( n==0 && r<0 && (-r) < (double)(LARGEST_INT64-1) ){` |
|      3 |  888 | `    r = -(double)((ph7_int64)((-r)+0.5));` |
|      2 |  889 | `  }else{` |
|      - |  890 | `	  char zBuf[256];` |
|      - |  891 | `	  sxu32 nLen;` |
|      3 |  892 | `	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);` |
|      - |  893 | `	  /* Convert the string to real number */` |
|      3 |  894 | `	  SyStrToReal(zBuf,nLen,(void *)&r,0);` |
|      - |  895 | `  }` |
|      - |  896 | `  /* Return thr rounded value */` |
|     17 |  897 | `  ph7_result_double(pCtx,r);` |
|     17 |  898 | `  return PH7_OK;` |
|     11 |  899 |  |
|      - |  900 | `/*` |
|      - |  901 | ` * string dechex(int $number)` |
|      - |  902 | ` *  Decimal to hexadecimal.` |
|      - |  903 | ` * Parameters` |
|      - |  904 | ` *  $number` |
|      - |  905 | ` *   Decimal value to convert` |
|      - |  906 | ` * Return` |
|      - |  907 | ` *  Hexadecimal string representation of number` |
|      - |  908 | ` */` |
|      6 |  909 | `static int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  910 |  |
|      - |  911 | `	int iVal;` |
|      7 |  912 | `	if( nArg < 1 ){` |
|      - |  913 | `		/* Missing arguments,return null */` |
|      5 |  914 | `		ph7_result_null(pCtx);` |
|      5 |  915 | `		return PH7_OK;` |
|      - |  916 | `	}` |
|      - |  917 | `	/* Extract the given number */` |
|      3 |  918 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  919 | `	/* Format */` |
|      3 |  920 | `	ph7_result_string_format(pCtx,"%x",iVal);` |
|      3 |  921 | `	return PH7_OK;` |
|      4 |  922 |  |
|      - |  923 | `/*` |
|      - |  924 | ` * string decoct(int $number)` |
|      - |  925 | ` *  Decimal to Octal.` |
|      - |  926 | ` * Parameters` |
|      - |  927 | ` *  $number` |
|      - |  928 | ` *   Decimal value to convert` |
|      - |  929 | ` * Return` |
|      - |  930 | ` *  Octal string representation of number` |
|      - |  931 | ` */` |
|      8 |  932 | `static int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  933 |  |
|      - |  934 | `	int iVal;` |
|      9 |  935 | `	if( nArg < 1 ){` |
|      - |  936 | `		/* Missing arguments,return null */` |
|      3 |  937 | `		ph7_result_null(pCtx);` |
|      3 |  938 | `		return PH7_OK;` |
|      - |  939 | `	}` |
|      - |  940 | `	/* Extract the given number */` |
|      7 |  941 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  942 | `	/* Format */` |
|      7 |  943 | `	ph7_result_string_format(pCtx,"%o",iVal);` |
|      7 |  944 | `	return PH7_OK;` |
|      5 |  945 |  |
|      - |  946 | `/*` |
|      - |  947 | ` * string decbin(int $number)` |
|      - |  948 | ` *  Decimal to binary.` |
|      - |  949 | ` * Parameters` |
|      - |  950 | ` *  $number` |
|      - |  951 | ` *   Decimal value to convert` |
|      - |  952 | ` * Return` |
|      - |  953 | ` *  Binary string representation of number` |
|      - |  954 | ` */` |
|      4 |  955 | `static int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  956 |  |
|      - |  957 | `	int iVal;` |
|      5 |  958 | `	if( nArg < 1 ){` |
|      - |  959 | `		/* Missing arguments,return null */` |
|      3 |  960 | `		ph7_result_null(pCtx);` |
|      3 |  961 | `		return PH7_OK;` |
|      - |  962 | `	}` |
|      - |  963 | `	/* Extract the given number */` |
|      3 |  964 | `	iVal = ph7_value_to_int(apArg[0]);` |
|      - |  965 | `	/* Format */` |
|      3 |  966 | `	ph7_result_string_format(pCtx,"%B",iVal);` |
|      3 |  967 | `	return PH7_OK;` |
|      3 |  968 |  |
|      - |  969 | `/*` |
|      - |  970 | ` * int64 hexdec(string $hex_string)` |
|      - |  971 | ` *  Hexadecimal to decimal.` |
|      - |  972 | ` * Parameters` |
|      - |  973 | ` *  $hex_string` |
|      - |  974 | ` *   The hexadecimal string to convert` |
|      - |  975 | ` * Return` |
|      - |  976 | ` *  The decimal representation of hex_string` |
|      - |  977 | ` */` |
|     24 |  978 | `static int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  979 |  |
|      - |  980 | `	const char *zString,*zEnd;` |
|      - |  981 | `	ph7_int64 iVal;` |
|      - |  982 | `	int nLen;` |
|     25 |  983 | `	if( nArg < 1 ){` |
|      - |  984 | `		/* Missing arguments,return -1 */` |
|      5 |  985 | `		ph7_result_int(pCtx,-1);` |
|      5 |  986 | `		return PH7_OK;` |
|      - |  987 | `	}` |
|     21 |  988 | `	iVal = 0;` |
|     21 |  989 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - |  990 | `		/* Extract the given string */` |
|     15 |  991 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  992 | `		/* Delimit the string */` |
|     15 |  993 | `		zEnd = &zString[nLen];` |
|      - |  994 | `		/* Ignore non hex-stream */` |
|     21 |  995 | `		while( zString < zEnd ){` |
|     21 |  996 | `			if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - |  997 | `				/* UTF-8 stream */` |
|      5 |  998 | `				zString++;` |
|      9 |  999 | `				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){` |
|      5 | 1000 | `					zString++;` |
|      1 | 1001 | `				}` |
|      3 | 1002 | `			}else{` |
|     17 | 1003 | `				if( SyisHex(zString[0]) ){` |
|     15 | 1004 | `					break;` |
|      - | 1005 | `				}` |
|      - | 1006 | `				/* Ignore */` |
|      3 | 1007 | `				zString++;` |
|      - | 1008 | `			}` |
|      1 | 1009 | `		}` |
|     15 | 1010 | `		if( zString < zEnd ){` |
|      - | 1011 | `			/* Cast */` |
|     15 | 1012 | `			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);` |
|      7 | 1013 | `		}` |
|      8 | 1014 | `	}else{` |
|      - | 1015 | `		/* Extract as a 64-bit integer */` |
|      7 | 1016 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1017 | `	}` |
|      - | 1018 | `	/* Return the number */` |
|     21 | 1019 | `	ph7_result_int64(pCtx,iVal);` |
|     21 | 1020 | `	return PH7_OK;` |
|     13 | 1021 |  |
|      - | 1022 | `/*` |
|      - | 1023 | ` * int64 bindec(string $bin_string)` |
|      - | 1024 | ` *  Binary to decimal.` |
|      - | 1025 | ` * Parameters` |
|      - | 1026 | ` *  $bin_string` |
|      - | 1027 | ` *   The binary string to convert` |
|      - | 1028 | ` * Return` |
|      - | 1029 | ` *  Returns the decimal equivalent of the binary number represented by the binary_string argument.` |
|      - | 1030 | ` */` |
|     12 | 1031 | `static int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1032 |  |
|      - | 1033 | `	const char *zString;` |
|      - | 1034 | `	ph7_int64 iVal;` |
|      - | 1035 | `	int nLen;` |
|     13 | 1036 | `	if( nArg < 1 ){` |
|      - | 1037 | `		/* Missing arguments,return -1 */` |
|      5 | 1038 | `		ph7_result_int(pCtx,-1);` |
|      5 | 1039 | `		return PH7_OK;` |
|      - | 1040 | `	}` |
|      9 | 1041 | `	iVal = 0;` |
|      9 | 1042 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1043 | `		/* Extract the given string */` |
|      7 | 1044 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1045 | `		if( nLen > 0 ){` |
|      - | 1046 | `			/* Perform a binary cast */` |
|      5 | 1047 | `			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      2 | 1048 | `		}` |
|      4 | 1049 | `	}else{` |
|      - | 1050 | `		/* Extract as a 64-bit integer */` |
|      3 | 1051 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1052 | `	}` |
|      - | 1053 | `	/* Return the number */` |
|      9 | 1054 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 1055 | `	return PH7_OK;` |
|      7 | 1056 |  |
|      - | 1057 | `/*` |
|      - | 1058 | ` * int64 octdec(string $oct_string)` |
|      - | 1059 | ` *  Octal to decimal.` |
|      - | 1060 | ` * Parameters` |
|      - | 1061 | ` *  $oct_string` |
|      - | 1062 | ` *   The octal string to convert` |
|      - | 1063 | ` * Return` |
|      - | 1064 | ` *  Returns the decimal equivalent of the octal number represented by the octal_string argument.` |
|      - | 1065 | ` */` |
|      6 | 1066 | `static int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1067 |  |
|      - | 1068 | `	const char *zString;` |
|      - | 1069 | `	ph7_int64 iVal;` |
|      - | 1070 | `	int nLen;` |
|      7 | 1071 | `	if( nArg < 1 ){` |
|      - | 1072 | `		/* Missing arguments,return -1 */` |
|      3 | 1073 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1074 | `		return PH7_OK;` |
|      - | 1075 | `	}` |
|      5 | 1076 | `	iVal = 0;` |
|      5 | 1077 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1078 | `		/* Extract the given string */` |
|      3 | 1079 | `		zString = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 1080 | `		if( nLen > 0 ){` |
|      - | 1081 | `			/* Perform the cast */` |
|      3 | 1082 | `			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);` |
|      1 | 1083 | `		}` |
|      2 | 1084 | `	}else{` |
|      - | 1085 | `		/* Extract as a 64-bit integer */` |
|      3 | 1086 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|      - | 1087 | `	}` |
|      - | 1088 | `	/* Return the number */` |
|      5 | 1089 | `	ph7_result_int64(pCtx,iVal);` |
|      5 | 1090 | `	return PH7_OK;` |
|      4 | 1091 |  |
|      - | 1092 | `/*` |
|      - | 1093 | ` * srand([int $seed])` |
|      - | 1094 | ` * mt_srand([int $seed])` |
|      - | 1095 | ` *  Seed the random number generator.` |
|      - | 1096 | ` * Parameters` |
|      - | 1097 | ` * $seed` |
|      - | 1098 | ` *  Optional seed value` |
|      - | 1099 | ` * Return` |
|      - | 1100 | ` *  null.` |
|      - | 1101 | ` * Note:` |
|      - | 1102 | ` *  THIS FUNCTION IS A NO-OP.` |
|      - | 1103 | ` *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.` |
|      - | 1104 | ` */` |
|     20 | 1105 | `static int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1106 |  |
|     10 | 1107 | `	SXUNUSED(nArg);` |
|     10 | 1108 | `	SXUNUSED(apArg);` |
|     21 | 1109 | `	ph7_result_null(pCtx);` |
|     21 | 1110 | `	return PH7_OK;` |
|      1 | 1111 |  |
|      - | 1112 | `/*` |
|      - | 1113 | ` * string base_convert(string $number,int $frombase,int $tobase)` |
|      - | 1114 | ` *  Convert a number between arbitrary bases.` |
|      - | 1115 | ` * Parameters` |
|      - | 1116 | ` * $number` |
|      - | 1117 | ` *  The number to convert` |
|      - | 1118 | ` * $frombase` |
|      - | 1119 | ` *  The base number is in` |
|      - | 1120 | ` * $tobase` |
|      - | 1121 | ` *  The base to convert number to` |
|      - | 1122 | ` * Return` |
|      - | 1123 | ` *  Number converted to base tobase` |
|      - | 1124 | ` */` |
|      - | 1125 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 1126 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     48 | 1127 | `static int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - | 1128 |  |
|      - | 1129 |  |
|      1 | 1130 |  |
|      - | 1131 | `	int nLen,iFbase,iTobase;` |
|      - | 1132 | `	const char *zNum;` |
|      - | 1133 | `	ph7_int64 iNum;` |
|     49 | 1134 | `	if( nArg < 3 ){` |
|      - | 1135 | `		/* Return the empty string*/` |
|     13 | 1136 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 1137 | `		return PH7_OK;` |
|      - | 1138 | `	}` |
|      - | 1139 | `	/* Base numbers */` |
|     37 | 1140 | `	iFbase  = ph7_value_to_int(apArg[1]);` |
|     37 | 1141 | `	iTobase = ph7_value_to_int(apArg[2]);` |
|     37 | 1142 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 1143 | `		/* Extract the target number */` |
|     33 | 1144 | `		zNum = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 1145 | `		if( nLen < 1 ){` |
|      - | 1146 | `			/* Return the empty string*/` |
|      5 | 1147 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1148 | `			return PH7_OK;` |
|      - | 1149 | `		}` |
|      - | 1150 | `		/* Base conversion */` |
|     29 | 1151 | `		switch(iFbase){` |
|      5 | 1152 | `		case 16:` |
|      - | 1153 | `			/* Hex */` |
|     11 | 1154 | `			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|     11 | 1155 | `			break;` |
|      3 | 1156 | `		case 8:` |
|      - | 1157 | `			/* Octal */` |
|      7 | 1158 | `			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      7 | 1159 | `			break;` |
|      2 | 1160 | `		case 2:` |
|      - | 1161 | `			/* Binary */` |
|      5 | 1162 | `			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      5 | 1163 | `			break;` |
|      4 | 1164 | `		default:` |
|      - | 1165 | `			/* Decimal */` |
|      9 | 1166 | `			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);` |
|      8 | 1167 | `			break;` |
|      - | 1168 | `		}` |
|     15 | 1169 | `	}else{` |
|      5 | 1170 | `		iNum = ph7_value_to_int64(apArg[0]);` |
|      - | 1171 | `	}` |
|     33 | 1172 | `	switch(iTobase){` |
|      3 | 1173 | `	case 16:` |
|      - | 1174 | `		/* Hex */` |
|      7 | 1175 | `		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */` |
|      7 | 1176 | `		break;` |
|      1 | 1177 | `	case 8:` |
|      - | 1178 | `		/* Octal */` |
|      3 | 1179 | `		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */` |
|      3 | 1180 | `		break;` |
|      1 | 1181 | `	case 2:` |
|      - | 1182 | `		/* Binary */` |
|      3 | 1183 | `		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */` |
|      3 | 1184 | `		break;` |
|     11 | 1185 | `	default:` |
|      - | 1186 | `		/* Decimal */` |
|     23 | 1187 | `		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */` |
|     22 | 1188 | `		break;` |
|      - | 1189 | `	}` |
|     33 | 1190 | `	return PH7_OK;` |
|     25 | 1191 |  |
|      - | 1192 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 1193 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 1194 | `/*` |
|      - | 1195 | ` * Section:` |
|      - | 1196 | ` *    String handling Functions.` |
|      - | 1197 | ` * Status:` |
|      - | 1198 | ` *    Stable.` |
|      - | 1199 | ` */` |
|      - | 1200 | `/*` |
|      - | 1201 | ` * string substr(string $string,int $start[, int $length ])` |
|      - | 1202 | ` *  Return part of a string.` |
|      - | 1203 | ` * Parameters` |
|      - | 1204 | ` *  $string` |
|      - | 1205 | ` *   The input string. Must be one character or longer.` |
|      - | 1206 | ` * $start` |
|      - | 1207 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - | 1208 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - | 1209 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 1210 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - | 1211 | ` *   from the end of string.` |
|      - | 1212 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - | 1213 | ` * $length` |
|      - | 1214 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - | 1215 | ` *   characters beginning from start (depending on the length of string).` |
|      - | 1216 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - | 1217 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - | 1218 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - | 1219 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - | 1220 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - | 1221 | ` *   will be returned.` |
|      - | 1222 | ` * Return` |
|      - | 1223 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - | 1224 | ` */` |
| 109830 | 1225 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1226 |  |
|      - | 1227 | `	const char *zSource,*zOfft;` |
|      - | 1228 | `	int nOfft,nLen,nSrcLen;` |
| 109832 | 1229 | `	if( nArg < 2 ){` |
|      - | 1230 | `		/* return FALSE */` |
|      5 | 1231 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1232 | `		return PH7_OK;` |
|      - | 1233 | `	}` |
|      - | 1234 | `	/* Extract the target string */` |
| 109828 | 1235 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 109828 | 1236 | `	if( nSrcLen < 1 ){` |
|      - | 1237 | `		/* Empty string,return FALSE */` |
|   6958 | 1238 | `		ph7_result_bool(pCtx,0);` |
|   6958 | 1239 | `		return PH7_OK;` |
|      - | 1240 | `	}` |
| 102872 | 1241 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1242 | `	/* Extract the offset */` |
| 102872 | 1243 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 102872 | 1244 | `	if( nOfft < 0 ){` |
|  17000 | 1245 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  17000 | 1246 | `		if( zOfft < zSource ){` |
|      - | 1247 | `			/* Invalid offset */` |
|      5 | 1248 | `			ph7_result_bool(pCtx,0);` |
|      5 | 1249 | `			return PH7_OK;` |
|      - | 1250 | `		}` |
|  16996 | 1251 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  16996 | 1252 | `		nOfft = (int)(zOfft-zSource);` |
|  94371 | 1253 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1254 | `		/* Invalid offset */` |
|      7 | 1255 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1256 | `		return PH7_OK;` |
|    ! 0 | 1257 | `	}else{` |
|  85868 | 1258 | `		zOfft = &zSource[nOfft];` |
|  85868 | 1259 | `		nLen = nSrcLen - nOfft;` |
|      - | 1260 | `	}` |
| 102862 | 1261 | `	if( nArg > 2 ){` |
|      - | 1262 | `		/* Extract the length */` |
|  85866 | 1263 | `		nLen = ph7_value_to_int(apArg[2]);` |
|  85866 | 1264 | `		if( nLen == 0 ){` |
|      - | 1265 | `			/* Invalid length,return an empty string */` |
|      5 | 1266 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 1267 | `			return PH7_OK;` |
|  85862 | 1268 | `		}else if( nLen < 0 ){` |
|  16998 | 1269 | `			nLen = nSrcLen + nLen - nOfft;` |
|  16998 | 1270 | `			if( nLen < 1 ){` |
|      - | 1271 | `				/* Invalid  length */` |
|      3 | 1272 | `				nLen = nSrcLen - nOfft;` |
|      1 | 1273 | `			}` |
|   8498 | 1274 | `		}` |
|  85862 | 1275 | `		if( nLen + nOfft > nSrcLen ){` |
|      - | 1276 | `			/* Invalid length */` |
|   2160 | 1277 | `			nLen = nSrcLen - nOfft;` |
|   1079 | 1278 | `		}` |
|  42930 | 1279 | `	}` |
|      - | 1280 | `	/* Return the substring */` |
| 102858 | 1281 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 102858 | 1282 | `	return PH7_OK;` |
|  54917 | 1283 |  |
|      - | 1284 | `/*` |
|      - | 1285 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - | 1286 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - | 1287 | ` * Parameters` |
|      - | 1288 | ` *  $main_str` |
|      - | 1289 | ` *  The main string being compared.` |
|      - | 1290 | ` *  $str` |
|      - | 1291 | ` *   The secondary string being compared.` |
|      - | 1292 | ` * $offset` |
|      - | 1293 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - | 1294 | ` *  the end of the string.` |
|      - | 1295 | ` * $length` |
|      - | 1296 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - | 1297 | ` *  of the str compared to the length of main_str less the offset.` |
|      - | 1298 | ` * $case_insensitivity` |
|      - | 1299 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - | 1300 | ` * Return` |
|      - | 1301 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - | 1302 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - | 1303 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - | 1304 | ` */` |
|     26 | 1305 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1306 |  |
|      - | 1307 | `	const char *zSource,*zOfft,*zSub;` |
|      - | 1308 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 | 1309 | `	int iCase = 0;` |
|      - | 1310 | `	int rc;` |
|     27 | 1311 | `	if( nArg < 3 ){` |
|      - | 1312 | `		/* Missing arguments,return FALSE */` |
|      5 | 1313 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1314 | `		return PH7_OK;` |
|      - | 1315 | `	}` |
|      - | 1316 | `	/* Extract the target string */` |
|     23 | 1317 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 | 1318 | `	if( nSrcLen < 1 ){` |
|      - | 1319 | `		/* Empty string,return FALSE */` |
|      3 | 1320 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1321 | `		return PH7_OK;` |
|      - | 1322 | `	}` |
|     21 | 1323 | `	nLen = nSrcLen; /* cc warning */` |
|      - | 1324 | `	/* Extract the substring */` |
|     21 | 1325 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 | 1326 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - | 1327 | `		/* Empty string,return FALSE */` |
|      3 | 1328 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1329 | `		return PH7_OK;` |
|      - | 1330 | `	}` |
|      - | 1331 | `	/* Extract the offset */` |
|     19 | 1332 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 | 1333 | `	if( nOfft < 0 ){` |
|      5 | 1334 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 | 1335 | `		if( zOfft < zSource ){` |
|      - | 1336 | `			/* Invalid offset */` |
|      3 | 1337 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1338 | `			return PH7_OK;` |
|      - | 1339 | `		}` |
|      3 | 1340 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 | 1341 | `		nOfft = (int)(zOfft-zSource);` |
|     16 | 1342 | `	}else if( nOfft >= nSrcLen ){` |
|      - | 1343 | `		/* Invalid offset */` |
|      3 | 1344 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1345 | `		return PH7_OK;` |
|    ! 0 | 1346 | `	}else{` |
|     13 | 1347 | `		zOfft = &zSource[nOfft];` |
|     13 | 1348 | `		nLen = nSrcLen - nOfft;` |
|      - | 1349 | `	}` |
|     15 | 1350 | `	if( nArg > 3 ){` |
|      - | 1351 | `		/* Extract the length */` |
|     13 | 1352 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1353 | `		if( nLen < 1 ){` |
|      - | 1354 | `			/* Invalid  length */` |
|      5 | 1355 | `			ph7_result_int(pCtx,1);` |
|      5 | 1356 | `			return PH7_OK;` |
|      9 | 1357 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - | 1358 | `			/* Invalid length */` |
|      3 | 1359 | `			nLen = nSrcLen - nOfft;` |
|      1 | 1360 | `		}` |
|      9 | 1361 | `		if( nArg > 4 ){` |
|      - | 1362 | `			/* Case-sensitive or not */` |
|      5 | 1363 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 | 1364 | `		}` |
|      4 | 1365 | `	}` |
|      - | 1366 | `	/* Perform the comparison */` |
|     11 | 1367 | `	if( iCase ){` |
|      3 | 1368 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 | 1369 | `	}else{` |
|      9 | 1370 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - | 1371 | `	}` |
|      - | 1372 | `	/* Comparison result */` |
|     11 | 1373 | `	ph7_result_int(pCtx,rc);` |
|     11 | 1374 | `	return PH7_OK;` |
|     14 | 1375 |  |
|      - | 1376 | `/*` |
|      - | 1377 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - | 1378 | ` *  Count the number of substring occurrences.` |
|      - | 1379 | ` * Parameters` |
|      - | 1380 | ` * $haystack` |
|      - | 1381 | ` *   The string to search in` |
|      - | 1382 | ` * $needle` |
|      - | 1383 | ` *   The substring to search for` |
|      - | 1384 | ` * $offset` |
|      - | 1385 | ` *  The offset where to start counting` |
|      - | 1386 | ` * $length (NOT USED)` |
|      - | 1387 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - | 1388 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - | 1389 | ` * Return` |
|      - | 1390 | ` *  Toral number of substring occurrences.` |
|      - | 1391 | ` */` |
|     24 | 1392 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1393 |  |
|      - | 1394 | `	const char *zText,*zPattern,*zEnd;` |
|      - | 1395 | `	int nTextlen,nPatlen;` |
|     25 | 1396 | `	int iCount = 0;` |
|      - | 1397 | `	sxu32 nOfft;` |
|      - | 1398 | `	sxi32 rc;` |
|     25 | 1399 | `	if( nArg < 2 ){` |
|      - | 1400 | `		/* Missing arguments */` |
|      5 | 1401 | `		ph7_result_int(pCtx,0);` |
|      5 | 1402 | `		return PH7_OK;` |
|      - | 1403 | `	}` |
|      - | 1404 | `	/* Point to the haystack */` |
|     21 | 1405 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - | 1406 | `	/* Point to the neddle */` |
|     21 | 1407 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 | 1408 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - | 1409 | `		/* NOOP,return zero */` |
|      3 | 1410 | `		ph7_result_int(pCtx,0);` |
|      3 | 1411 | `		return PH7_OK;` |
|      - | 1412 | `	}` |
|     19 | 1413 | `	if( nArg > 2 ){` |
|      - | 1414 | `		int iOfft;` |
|      - | 1415 | `		/* Extract the offset */` |
|     15 | 1416 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 | 1417 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - | 1418 | `			/* Invalid offset,return zero */` |
|      3 | 1419 | `			ph7_result_int(pCtx,0);` |
|      3 | 1420 | `			return PH7_OK;` |
|      - | 1421 | `		}` |
|      - | 1422 | `		/* Point to the desired offset */` |
|     13 | 1423 | `		zText = &zText[iOfft];` |
|      - | 1424 | `		/* Adjust length */` |
|     13 | 1425 | `		nTextlen -= iOfft;` |
|      6 | 1426 | `	}` |
|      - | 1427 | `	/* Point to the end of the string */` |
|     17 | 1428 | `	zEnd = &zText[nTextlen];` |
|     17 | 1429 | `	if( nArg > 3 ){` |
|      - | 1430 | `		int nLen;` |
|      - | 1431 | `		/* Extract the length */` |
|     13 | 1432 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 | 1433 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - | 1434 | `			/* Invalid length,return 0 */` |
|      7 | 1435 | `			ph7_result_int(pCtx,0);` |
|      7 | 1436 | `			return PH7_OK;` |
|      - | 1437 | `		}` |
|      - | 1438 | `		/* Adjust pointer */` |
|      7 | 1439 | `		nTextlen = nLen;` |
|      7 | 1440 | `		zEnd = &zText[nTextlen];` |
|      3 | 1441 | `	}` |
|      - | 1442 | `	/* Perform the search */` |
|     12 | 1443 | `	for(;;){` |
|     25 | 1444 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 | 1445 | `		if( rc != SXRET_OK ){` |
|      - | 1446 | `			/* Pattern not found,break immediately */` |
|      9 | 1447 | `			break;` |
|      - | 1448 | `		}` |
|      - | 1449 | `		/* Increment counter and update the offset */` |
|     17 | 1450 | `		iCount++;` |
|     17 | 1451 | `		zText += nOfft + nPatlen;` |
|     17 | 1452 | `		if( zText >= zEnd ){` |
|      3 | 1453 | `			break;` |
|      - | 1454 | `		}` |
|      1 | 1455 | `	}` |
|      - | 1456 | `	/* Pattern count */` |
|     11 | 1457 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 1458 | `	return PH7_OK;` |
|     13 | 1459 |  |
|      - | 1460 | `/*` |
|      - | 1461 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1462 | ` *   Split a string into smaller chunks.` |
|      - | 1463 | ` * Parameters` |
|      - | 1464 | ` *  $body` |
|      - | 1465 | ` *   The string to be chunked.` |
|      - | 1466 | ` * $chunklen` |
|      - | 1467 | ` *   The chunk length.` |
|      - | 1468 | ` * $end` |
|      - | 1469 | ` *   The line ending sequence.` |
|      - | 1470 | ` * Return` |
|      - | 1471 | ` *  The chunked string or NULL on failure.` |
|      - | 1472 | ` */` |
|     16 | 1473 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1474 |  |
|     17 | 1475 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1476 | `	int nSepLen,nChunkLen,nLen;` |
|     17 | 1477 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1478 | `		/* Nothing to split,return null */` |
|      5 | 1479 | `		ph7_result_null(pCtx);` |
|      5 | 1480 | `		return PH7_OK;` |
|      - | 1481 | `	}` |
|      - | 1482 | `	/* initialize/Extract arguments */` |
|     13 | 1483 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1484 | `	nChunkLen = 76;` |
|     13 | 1485 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1486 | `	zEnd = &zIn[nLen];` |
|     13 | 1487 | `	if( nArg > 1 ){` |
|      - | 1488 | `		/* Chunk length */` |
|     13 | 1489 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1490 | `		if( nChunkLen < 1 ){` |
|      - | 1491 | `			/* Switch back to the default length */` |
|      3 | 1492 | `			nChunkLen = 76;` |
|      1 | 1493 | `		}` |
|     13 | 1494 | `		if( nArg > 2 ){` |
|      - | 1495 | `			/* Separator */` |
|      9 | 1496 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1497 | `			if( nSepLen < 1 ){` |
|      - | 1498 | `				/* Switch back to the default separator */` |
|      3 | 1499 | `				zSep = "\r\n";` |
|      3 | 1500 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1501 | `			}` |
|      4 | 1502 | `		}` |
|      6 | 1503 | `	}` |
|      - | 1504 | `	/* Perform the requested operation */` |
|     13 | 1505 | `	if( nChunkLen > nLen ){` |
|      - | 1506 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1507 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1508 | `		return PH7_OK;` |
|      - | 1509 | `	}` |
|     17 | 1510 | `	while( zIn < zEnd ){` |
|     13 | 1511 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1512 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1513 | `		}` |
|      - | 1514 | `		/* Append the chunk and the separator */` |
|     13 | 1515 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1516 | `		/* Point beyond the chunk */` |
|     13 | 1517 | `		zIn += nChunkLen;` |
|      1 | 1518 | `	}` |
|      5 | 1519 | `	return PH7_OK;` |
|      9 | 1520 |  |
|      - | 1521 | `/*` |
|      - | 1522 | ` * string addslashes(string $str)` |
|      - | 1523 | ` *  Quote string with slashes.` |
|      - | 1524 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1525 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1526 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1527 | ` * Parameter` |
|      - | 1528 | ` *  str: The string to be escaped.` |
|      - | 1529 | ` * Return` |
|      - | 1530 | ` *  Returns the escaped string` |
|      - | 1531 | ` */` |
|     24 | 1532 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1533 |  |
|      - | 1534 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1535 | `	int nLen;` |
|      - | 1536 | `	/* PHP enforces exactly one argument. */` |
|     26 | 1537 | `	if( nArg != 1 ){` |
|      7 | 1538 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1539 | `			"ArgumentCountError",` |
|      - | 1540 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 | 1541 | `			nArg` |
|      - | 1542 | `			);` |
|      - | 1543 | `	}` |
|      - | 1544 | `	/* Reject explicit NULL right away.  With the compiler fix above,` |
|      - | 1545 | `	 * string literals (including the empty string) are represented as real` |
|      - | 1546 | `	 * strings, so this check won’t fire for "" anymore. */` |
|     22 | 1547 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      4 | 1548 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1549 | `			"TypeError",` |
|      - | 1550 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1551 | `			ph7_type_name(apArg[0])` |
|      - | 1552 | `			);` |
|      - | 1553 | `	}` |
|      - | 1554 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     34 | 1555 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     26 | 1556 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     16 | 1557 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1558 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1559 | `			"TypeError",` |
|      - | 1560 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1561 | `			ph7_type_name(apArg[0])` |
|      - | 1562 | `			);` |
|      - | 1563 | `	}` |
|      - | 1564 | `	/* Convert to string representation first and obtain length. */` |
|     17 | 1565 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 1566 | `	if( nLen < 1 ){` |
|      - | 1567 | `		/* Return the empty string */` |
|      3 | 1568 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1569 | `		return PH7_OK;` |
|      - | 1570 | `	}` |
|     15 | 1571 | `	zEnd = &zIn[nLen];` |
|     15 | 1572 | `	zCur = 0; /* cc warning */` |
|     20 | 1573 | `	for(;;){` |
|     41 | 1574 | `		if( zIn >= zEnd ){` |
|      - | 1575 | `			/* No more input */` |
|     15 | 1576 | `			break;` |
|      - | 1577 | `		}` |
|     27 | 1578 | `		zCur = zIn;` |
|      - | 1579 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1580 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1581 | `			zIn++;` |
|      1 | 1582 | `		}` |
|     27 | 1583 | `		if( zIn > zCur ){` |
|      - | 1584 | `			/* Append raw contents */` |
|     23 | 1585 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1586 | `		}` |
|     27 | 1587 | `		if( zIn < zEnd ){` |
|     17 | 1588 | `			int c = zIn[0];` |
|     17 | 1589 | `			if( c == '\0' ){` |
|      - | 1590 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1591 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1592 | `			}else{` |
|     15 | 1593 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1594 | `			}` |
|      8 | 1595 | `		}` |
|     27 | 1596 | `		zIn++;` |
|      1 | 1597 | `	}` |
|     15 | 1598 | `	return PH7_OK;` |
|     14 | 1599 |  |
|      - | 1600 | `/*` |
|      - | 1601 | ` * Check if the given character is present in the given mask.` |
|      - | 1602 | ` * Return TRUE if present. FALSE otherwise.` |
|      - | 1603 | ` */` |
|     76 | 1604 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 | 1605 |  |
|     77 | 1606 | `	const char *zEnd = &zMask[nLen];` |
|    495 | 1607 | `	while( zMask < zEnd ){` |
|    449 | 1608 | `		if( zMask[0] == c ){` |
|      - | 1609 | `			/* Character present,return TRUE */` |
|     31 | 1610 | `			return 1;` |
|      - | 1611 | `		}` |
|      - | 1612 | `		/* Advance the pointer */` |
|    419 | 1613 | `		zMask++;` |
|      1 | 1614 | `	}` |
|      - | 1615 | `	/* Not present */` |
|     47 | 1616 | `	return 0;` |
|     39 | 1617 |  |
|      - | 1618 | `/*` |
|      - | 1619 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1620 | ` *  Quote string with slashes in a C style.` |
|      - | 1621 | ` * Parameter` |
|      - | 1622 | ` *  $str:` |
|      - | 1623 | ` *    The string to be escaped.` |
|      - | 1624 | ` *  $charlist:` |
|      - | 1625 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1626 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1627 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1628 | ` * Return` |
|      - | 1629 | ` *  Returns the escaped string.` |
|      - | 1630 | ` * Note:` |
|      - | 1631 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - | 1632 | ` */` |
|     12 | 1633 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1634 |  |
|      - | 1635 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1636 | `	int nLen,nMask;` |
|     13 | 1637 | `	if( nArg < 1 ){` |
|      - | 1638 | `		/* Nothing to process,retun NULL */` |
|      3 | 1639 | `		ph7_result_null(pCtx);` |
|      3 | 1640 | `		return PH7_OK;` |
|      - | 1641 | `	}` |
|      - | 1642 | `	/* Extract the string to process */` |
|     11 | 1643 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1644 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 1645 | `		/* Return the string untouched */` |
|      5 | 1646 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1647 | `		return PH7_OK;` |
|      - | 1648 | `	}` |
|      - | 1649 | `	/* Extract the desired mask */` |
|      7 | 1650 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|      7 | 1651 | `	zEnd = &zIn[nLen];` |
|      7 | 1652 | `	zCur = 0; /* cc warning */` |
|      8 | 1653 | `	for(;;){` |
|     17 | 1654 | `		if( zIn >= zEnd ){` |
|      - | 1655 | `			/* No more input */` |
|      7 | 1656 | `			break;` |
|      - | 1657 | `		}` |
|     11 | 1658 | `		zCur = zIn;` |
|     31 | 1659 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     21 | 1660 | `			zIn++;` |
|      1 | 1661 | `		}` |
|     11 | 1662 | `		if( zIn > zCur ){` |
|      - | 1663 | `			/* Append raw contents */` |
|     11 | 1664 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1665 | `		}` |
|     11 | 1666 | `		if( zIn < zEnd ){` |
|      5 | 1667 | `			int c = zIn[0];` |
|      5 | 1668 | `			if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1669 | `				/* Convert to octal */` |
|      3 | 1670 | `				ph7_result_string_format(pCtx,"\\%o",c);` |
|      2 | 1671 | `			}else{` |
|      3 | 1672 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1673 | `			}` |
|      2 | 1674 | `		}` |
|     11 | 1675 | `		zIn++;` |
|      1 | 1676 | `	}` |
|      7 | 1677 | `	return PH7_OK;` |
|      7 | 1678 |  |
|      - | 1679 | `/*` |
|      - | 1680 | ` * string quotemeta(string $str)` |
|      - | 1681 | ` *  Quote meta characters.` |
|      - | 1682 | ` * Parameter` |
|      - | 1683 | ` *  $str:` |
|      - | 1684 | ` *    The string to be escaped.` |
|      - | 1685 | ` * Return` |
|      - | 1686 | ` *  Returns the escaped string.` |
|      - | 1687 | `*/` |
|     10 | 1688 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1689 |  |
|      - | 1690 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1691 | `	int nLen;` |
|     11 | 1692 | `	if( nArg < 1 ){` |
|      - | 1693 | `		/* Nothing to process,retun NULL */` |
|      3 | 1694 | `		ph7_result_null(pCtx);` |
|      3 | 1695 | `		return PH7_OK;` |
|      - | 1696 | `	}` |
|      - | 1697 | `	/* Extract the string to process */` |
|      9 | 1698 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 1699 | `	if( nLen < 1 ){` |
|      - | 1700 | `		/* Return the empty string */` |
|      3 | 1701 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1702 | `		return PH7_OK;` |
|      - | 1703 | `	}` |
|      7 | 1704 | `	zEnd = &zIn[nLen];` |
|      7 | 1705 | `	zCur = 0; /* cc warning */` |
|     17 | 1706 | `	for(;;){` |
|     35 | 1707 | `		if( zIn >= zEnd ){` |
|      - | 1708 | `			/* No more input */` |
|      7 | 1709 | `			break;` |
|      - | 1710 | `		}` |
|     29 | 1711 | `		zCur = zIn;` |
|     55 | 1712 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 | 1713 | `			zIn++;` |
|      1 | 1714 | `		}` |
|     29 | 1715 | `		if( zIn > zCur ){` |
|      - | 1716 | `			/* Append raw contents */` |
|     11 | 1717 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 | 1718 | `		}` |
|     29 | 1719 | `		if( zIn < zEnd ){` |
|     27 | 1720 | `			int c = zIn[0];` |
|     27 | 1721 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 | 1722 | `		}` |
|     29 | 1723 | `		zIn++;` |
|      1 | 1724 | `	}` |
|      7 | 1725 | `	return PH7_OK;` |
|      6 | 1726 |  |
|      - | 1727 | `/*` |
|      - | 1728 | ` * string stripslashes(string $str)` |
|      - | 1729 | ` *  Un-quotes a quoted string.` |
|      - | 1730 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1731 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1732 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1733 | ` * Parameter` |
|      - | 1734 | ` *  $str` |
|      - | 1735 | ` *   The input string.` |
|      - | 1736 | ` * Return` |
|      - | 1737 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1738 | ` */` |
|      8 | 1739 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1740 |  |
|      - | 1741 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1742 | `	int nLen;` |
|      9 | 1743 | `	if( nArg < 1 ){` |
|      - | 1744 | `		/* Nothing to process,retun NULL */` |
|      3 | 1745 | `		ph7_result_null(pCtx);` |
|      3 | 1746 | `		return PH7_OK;` |
|      - | 1747 | `	}` |
|      - | 1748 | `	/* Extract the string to process */` |
|      7 | 1749 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1750 | `	if( zIn == 0 ){` |
|    ! 0 | 1751 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1752 | `		return PH7_OK;` |
|      - | 1753 | `	}` |
|      7 | 1754 | `	zEnd = &zIn[nLen];` |
|      7 | 1755 | `	zCur = 0; /* cc warning */` |
|      - | 1756 | `	/* Encode the string */` |
|      4 | 1757 | `	for(;;){` |
|      9 | 1758 | `		if( zIn >= zEnd ){` |
|      - | 1759 | `			/* No more input */` |
|      5 | 1760 | `			break;` |
|      - | 1761 | `		}` |
|      5 | 1762 | `		zCur = zIn;` |
|     17 | 1763 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1764 | `			zIn++;` |
|      1 | 1765 | `		}` |
|      5 | 1766 | `		if( zIn > zCur ){` |
|      - | 1767 | `			/* Append raw contents */` |
|      5 | 1768 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1769 | `		}` |
|      5 | 1770 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1771 | `			int c = zIn[1];` |
|      3 | 1772 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1773 | `				/* Ignore the backslash */` |
|      3 | 1774 | `				zIn++;` |
|      1 | 1775 | `			}` |
|      2 | 1776 | `		}else{` |
|      3 | 1777 | `			break;` |
|      - | 1778 | `		}` |
|      1 | 1779 | `	}` |
|      7 | 1780 | `	return PH7_OK;` |
|      5 | 1781 |  |
|      - | 1782 | `/*` |
|      - | 1783 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1784 | ` *  HTML escaping of special characters.` |
|      - | 1785 | ` *  The translations performed are:` |
|      - | 1786 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1787 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1788 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1789 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1790 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1791 | ` * Parameters` |
|      - | 1792 | ` *  $string` |
|      - | 1793 | ` *   The string being converted.` |
|      - | 1794 | ` * $flags` |
|      - | 1795 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1796 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1797 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1798 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1799 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1800 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1801 | ` * $charset` |
|      - | 1802 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1803 | ` * Return` |
|      - | 1804 | ` *  The escaped string or NULL on failure.` |
|      - | 1805 | ` */` |
|     20 | 1806 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1807 |  |
|      - | 1808 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1809 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1810 | `	int nLen,c;` |
|     21 | 1811 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1812 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1813 | `		ph7_result_null(pCtx);` |
|      9 | 1814 | `		return PH7_OK;` |
|      - | 1815 | `	}` |
|      - | 1816 | `	/* Extract the target string */` |
|     13 | 1817 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1818 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1819 | `	if( nLen == 0 ){` |
|      3 | 1820 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1821 | `		return PH7_OK;` |
|      - | 1822 | `	}` |
|     11 | 1823 | `	zEnd = &zIn[nLen];` |
|      - | 1824 | `	/* Extract the flags if available */` |
|     11 | 1825 | `	if( nArg > 1 ){` |
|      9 | 1826 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1827 | `		if( iFlags < 0 ){` |
|      3 | 1828 | `			iFlags = 0x01\|0x40;` |
|      1 | 1829 | `		}` |
|      4 | 1830 | `	}` |
|      - | 1831 | `	/* Perform the requested operation */` |
|     23 | 1832 | `	for(;;){` |
|     47 | 1833 | `		if( zIn >= zEnd ){` |
|      9 | 1834 | `			break;` |
|      - | 1835 | `		}` |
|     39 | 1836 | `		zCur = zIn;` |
|     83 | 1837 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1838 | `			zIn++;` |
|      1 | 1839 | `		}` |
|     39 | 1840 | `		if( zCur < zIn ){` |
|      - | 1841 | `			/* Append the raw string verbatim */` |
|     17 | 1842 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1843 | `		}` |
|     39 | 1844 | `		if( zIn >= zEnd ){` |
|      3 | 1845 | `			break;` |
|      - | 1846 | `		}` |
|     37 | 1847 | `		c = zIn[0];` |
|     37 | 1848 | `		if( c == '&' ){` |
|      - | 1849 | `			/* Expand '&amp;' */` |
|      9 | 1850 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1851 | `		}else if( c == '<' ){` |
|      - | 1852 | `			/* Expand '&lt;' */` |
|      7 | 1853 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1854 | `		}else if( c == '>' ){` |
|      - | 1855 | `			/* Expand '&gt;' */` |
|      9 | 1856 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1857 | `		}else if( c == '\'' ){` |
|      5 | 1858 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1859 | `				/* Expand '&#039;' */` |
|      5 | 1860 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1861 | `			}else{` |
|      - | 1862 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1863 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1864 | `			}` |
|     13 | 1865 | `		}else if( c == '"' ){` |
|     11 | 1866 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1867 | `				/* Expand '&quot;' */` |
|      7 | 1868 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1869 | `			}else{` |
|      - | 1870 | `				/* Leave the double quote untouched */` |
|      5 | 1871 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1872 | `			}` |
|      5 | 1873 | `		}` |
|      - | 1874 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1875 | `		zIn++;` |
|      1 | 1876 | `	}` |
|     11 | 1877 | `	return PH7_OK;` |
|     11 | 1878 |  |
|      - | 1879 | `/*` |
|      - | 1880 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1881 | ` *  Unescape HTML entities.` |
|      - | 1882 | ` * Parameters` |
|      - | 1883 | ` *  $string` |
|      - | 1884 | ` *   The string to decode` |
|      - | 1885 | ` *  $quote_style` |
|      - | 1886 | ` *    The quote style. One of the following constants:` |
|      - | 1887 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1888 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1889 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1890 | ` * Return` |
|      - | 1891 | ` *  The unescaped string or NULL on failure.` |
|      - | 1892 | ` */` |
|     16 | 1893 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1894 |  |
|      - | 1895 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1896 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1897 | `	int nLen,nJump;` |
|     17 | 1898 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1899 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1900 | `		ph7_result_null(pCtx);` |
|      7 | 1901 | `		return PH7_OK;` |
|      - | 1902 | `	}` |
|      - | 1903 | `	/* Extract the target string */` |
|     11 | 1904 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1905 | `	zEnd = &zIn[nLen];` |
|      - | 1906 | `	/* Extract the flags if available */` |
|     11 | 1907 | `	if( nArg > 1 ){` |
|      7 | 1908 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1909 | `		if( iFlags < 0 ){` |
|      3 | 1910 | `			iFlags = 0x01;` |
|      1 | 1911 | `		}` |
|      3 | 1912 | `	}` |
|      - | 1913 | `	/* Perform the requested operation */` |
|     15 | 1914 | `	for(;;){` |
|     31 | 1915 | `		if( zIn >= zEnd ){` |
|     11 | 1916 | `			break;` |
|      - | 1917 | `		}` |
|     21 | 1918 | `		zCur = zIn;` |
|     51 | 1919 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1920 | `			zIn++;` |
|      1 | 1921 | `		}` |
|     21 | 1922 | `		if( zCur < zIn ){` |
|      - | 1923 | `			/* Append the raw string verbatim */` |
|      9 | 1924 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1925 | `		}` |
|     21 | 1926 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1927 | `		nJump = (int)sizeof(char);` |
|     21 | 1928 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1929 | `			/* &amp; ==> '&' */` |
|      3 | 1930 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1931 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1932 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1933 | `			/* &lt; ==> < */` |
|      3 | 1934 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1935 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1936 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1937 | `			/* &gt; ==> '>' */` |
|      3 | 1938 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1939 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1940 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1941 | `			/* &quot; ==> '"' */` |
|     13 | 1942 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1943 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1944 | `			}else{` |
|      - | 1945 | `				/* Leave untouched */` |
|      5 | 1946 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1947 | `			}` |
|     13 | 1948 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1949 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1950 | `			/* &#039; ==> ''' */` |
|      3 | 1951 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1952 | `				/* Expand ''' */` |
|      3 | 1953 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1954 | `			}else{` |
|      - | 1955 | `				/* Leave untouched */` |
|    ! 0 | 1956 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1957 | `			}` |
|      3 | 1958 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1959 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1960 | `			/* expand '&' */` |
|    ! 0 | 1961 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1962 | `		}else{` |
|      - | 1963 | `			/* No more input to process */` |
|    ! 0 | 1964 | `			break;` |
|      - | 1965 | `		}` |
|     21 | 1966 | `		zIn += nJump;` |
|      1 | 1967 | `	}` |
|     11 | 1968 | `	return PH7_OK;` |
|      9 | 1969 |  |
|      - | 1970 | `/* HTML encoding/Decoding table` |
|      - | 1971 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1972 | ` */` |
|      - | 1973 | `static const char *azHtmlEscape[] = {` |
|      - | 1974 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1975 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1976 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1977 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1978 | ` };` |
|      - | 1979 | `/*` |
|      - | 1980 | ` * array get_html_translation_table(void)` |
|      - | 1981 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1982 | ` * Parameters` |
|      - | 1983 | ` *  None` |
|      - | 1984 | ` * Return` |
|      - | 1985 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1986 | ` */` |
|      4 | 1987 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1988 |  |
|      - | 1989 | `	ph7_value *pArray,*pValue;` |
|      - | 1990 | `	sxu32 n;` |
|      - | 1991 | `	/* Element value */` |
|      5 | 1992 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1993 | `	if( pValue == 0 ){` |
|    ! 0 | 1994 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1995 | `		SXUNUSED(apArg);` |
|      - | 1996 | `		/* Return NULL */` |
|    ! 0 | 1997 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1998 | `		return PH7_OK;` |
|      - | 1999 | `	}` |
|      - | 2000 | `	/* Create a new array */` |
|      5 | 2001 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 2002 | `	if( pArray == 0 ){` |
|      - | 2003 | `		/* Return NULL */` |
|    ! 0 | 2004 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2005 | `		return PH7_OK;` |
|      - | 2006 | `	}` |
|      - | 2007 | `	/* Make the table */` |
|     85 | 2008 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 2009 | `		/* Prepare the value */` |
|     81 | 2010 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 2011 | `		/* Insert the value */` |
|     81 | 2012 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 2013 | `		/* Reset the string cursor */` |
|     81 | 2014 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 2015 | `	}` |
|      - | 2016 | `	/*` |
|      - | 2017 | `	 * Return the array.` |
|      - | 2018 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 2019 | `	 * released upon we return from this function.` |
|      - | 2020 | `	 */` |
|      5 | 2021 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 2022 | `	return PH7_OK;` |
|      3 | 2023 |  |
|      - | 2024 | `/*` |
|      - | 2025 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 2026 | ` *   Convert all applicable characters to HTML entities` |
|      - | 2027 | ` * Parameters` |
|      - | 2028 | ` * $string` |
|      - | 2029 | ` *   The input string.` |
|      - | 2030 | ` * $flags` |
|      - | 2031 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 2032 | ` * Return` |
|      - | 2033 | ` * The encoded string.` |
|      - | 2034 | ` */` |
|     10 | 2035 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2036 |  |
|     11 | 2037 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 2038 | `	const char *zIn,*zEnd;` |
|      - | 2039 | `	int nLen,c;` |
|      - | 2040 | `	sxu32 n;` |
|     11 | 2041 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2042 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2043 | `		ph7_result_null(pCtx);` |
|      5 | 2044 | `		return PH7_OK;` |
|      - | 2045 | `	}` |
|      - | 2046 | `	/* Extract the target string */` |
|      7 | 2047 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2048 | `	/* Handle empty string up front */` |
|      7 | 2049 | `	if( nLen == 0 ){` |
|      3 | 2050 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2051 | `		return PH7_OK;` |
|      - | 2052 | `	}` |
|      5 | 2053 | `	zEnd = &zIn[nLen];` |
|      - | 2054 | `	/* Extract the flags if available */` |
|      5 | 2055 | `	if( nArg > 1 ){` |
|      3 | 2056 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 2057 | `		if( iFlags < 0 ){` |
|      3 | 2058 | `			iFlags = 0x01;` |
|      1 | 2059 | `		}` |
|      1 | 2060 | `	}` |
|      - | 2061 | `	/* Perform the requested operation */` |
|     11 | 2062 | `	for(;;){` |
|     23 | 2063 | `		if( zIn >= zEnd ){` |
|      - | 2064 | `			/* No more input to process */` |
|      5 | 2065 | `			break;` |
|      - | 2066 | `		}` |
|     19 | 2067 | `		c = zIn[0];` |
|      - | 2068 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 2069 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 2070 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 2071 | `				/* Got one */` |
|      9 | 2072 | `				break;` |
|      - | 2073 | `			}` |
|    108 | 2074 | `		}` |
|     19 | 2075 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 2076 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 2077 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2078 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 2079 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 2080 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 2081 | `				/* expand single quote verbatim */` |
|    ! 0 | 2082 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 2083 | `			}else{` |
|      9 | 2084 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 2085 | `			}` |
|      5 | 2086 | `		}else{` |
|      - | 2087 | `			/* Output character verbatim */` |
|     11 | 2088 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2089 | `		}` |
|     19 | 2090 | `		zIn++;` |
|      1 | 2091 | `	}` |
|      5 | 2092 | `	return PH7_OK;` |
|      6 | 2093 |  |
|      - | 2094 | `/*` |
|      - | 2095 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 2096 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 2097 | ` * Parameters` |
|      - | 2098 | ` * $string` |
|      - | 2099 | ` *   The input string.` |
|      - | 2100 | ` * $flags` |
|      - | 2101 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 2102 | ` * Return` |
|      - | 2103 | ` * The decoded string.` |
|      - | 2104 | ` */` |
|     28 | 2105 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2106 |  |
|      - | 2107 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 2108 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 2109 | `	int nLen;` |
|      - | 2110 | `	sxu32 n;` |
|     29 | 2111 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2112 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 2113 | `		ph7_result_null(pCtx);` |
|      5 | 2114 | `		return PH7_OK;` |
|      - | 2115 | `	}` |
|      - | 2116 | `	/* Extract the target string */` |
|     25 | 2117 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2118 | `	zEnd = &zIn[nLen];` |
|      - | 2119 | `	/* Extract the flags if available */` |
|     25 | 2120 | `	if( nArg > 1 ){` |
|     15 | 2121 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 2122 | `		if( iFlags < 0 ){` |
|      3 | 2123 | `			iFlags = 0x01;` |
|      1 | 2124 | `		}` |
|      7 | 2125 | `	}` |
|      - | 2126 | `	/* Perform the requested operation */` |
|     27 | 2127 | `	for(;;){` |
|     55 | 2128 | `		if( zIn >= zEnd ){` |
|      - | 2129 | `			/* No more input to process */` |
|     13 | 2130 | `			break;` |
|      - | 2131 | `		}` |
|     43 | 2132 | `		zCur = zIn;` |
|    173 | 2133 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 2134 | `			zIn++;` |
|      1 | 2135 | `		}` |
|     43 | 2136 | `		if( zCur < zIn ){` |
|      - | 2137 | `			/* Append raw string verbatim */` |
|     27 | 2138 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 2139 | `		}` |
|     43 | 2140 | `		if( zIn >= zEnd ){` |
|     13 | 2141 | `			break;` |
|      - | 2142 | `		}` |
|     31 | 2143 | `		nLen = (int)(zEnd-zIn);` |
|      - | 2144 | `		/* Find an encoded sequence */` |
|    113 | 2145 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 2146 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 2147 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 2148 | `				/* Got one */` |
|     31 | 2149 | `				zIn += iLen;` |
|     31 | 2150 | `				break;` |
|      - | 2151 | `			}` |
|     42 | 2152 | `		}` |
|     31 | 2153 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 2154 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 2155 | `			/* Output the decoded character */` |
|     31 | 2156 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 2157 | `				/* Do not process single quotes */` |
|      9 | 2158 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 2159 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 2160 | `				/* Do not process double quotes */` |
|      5 | 2161 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 2162 | `			}else{` |
|     19 | 2163 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 2164 | `			}` |
|     16 | 2165 | `		}else{` |
|      - | 2166 | `			/* Append '&' */` |
|    ! 0 | 2167 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 2168 | `			zIn++;` |
|      - | 2169 | `		}` |
|      1 | 2170 | `	}` |
|     25 | 2171 | `	return PH7_OK;` |
|     15 | 2172 |  |
|      - | 2173 | `/*` |
|      - | 2174 | ` * int strlen($string)` |
|      - | 2175 | ` *  return the length of the given string.` |
|      - | 2176 | ` * Parameter` |
|      - | 2177 | ` *  string: The string being measured for length.` |
|      - | 2178 | ` * Return` |
|      - | 2179 | ` *  length of the given string.` |
|      - | 2180 | ` */` |
|   1536 | 2181 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2182 |  |
|   1538 | 2183 | `	int iLen = 0;` |
|   1538 | 2184 | `	if( nArg > 0 ){` |
|   1536 | 2185 | `		ph7_value_to_string(apArg[0],&iLen);` |
|    767 | 2186 | `	}` |
|      - | 2187 | `	/* String length */` |
|   1538 | 2188 | `	ph7_result_int(pCtx,iLen);` |
|   1538 | 2189 | `	return PH7_OK;` |
|      2 | 2190 |  |
|      - | 2191 | `/*` |
|      - | 2192 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2193 | ` *  Perform a binary safe string comparison.` |
|      - | 2194 | ` * Parameter` |
|      - | 2195 | ` *  str1: The first string` |
|      - | 2196 | ` *  str2: The second string` |
|      - | 2197 | ` * Return` |
|      - | 2198 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2199 | ` *  than str2, and 0 if they are equal.` |
|      - | 2200 | ` */` |
|     50 | 2201 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2202 |  |
|      - | 2203 | `	const char *z1,*z2;` |
|      - | 2204 | `	int n1,n2;` |
|      - | 2205 | `	int res;` |
|     51 | 2206 | `	if( nArg < 2 ){` |
|      5 | 2207 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 2208 | `		ph7_result_int(pCtx,res);` |
|      5 | 2209 | `		return PH7_OK;` |
|      - | 2210 | `	}` |
|      - | 2211 | `	/* Perform the comparison */` |
|     47 | 2212 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     47 | 2213 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     47 | 2214 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2215 | `	/* Comparison result */` |
|     47 | 2216 | `	ph7_result_int(pCtx,res);` |
|     47 | 2217 | `	return PH7_OK;` |
|     26 | 2218 |  |
|      - | 2219 | `/*` |
|      - | 2220 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2221 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2222 | ` * Parameter` |
|      - | 2223 | ` *  str1: The first string` |
|      - | 2224 | ` *  str2: The second string` |
|      - | 2225 | ` * Return` |
|      - | 2226 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2227 | ` *  than str2, and 0 if they are equal.` |
|      - | 2228 | ` */` |
|     20 | 2229 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2230 |  |
|      - | 2231 | `	const char *z1,*z2;` |
|      - | 2232 | `	int res;` |
|      - | 2233 | `	int n;` |
|     21 | 2234 | `	if( nArg < 3 ){` |
|      - | 2235 | `		/* Perform a standard comparison */` |
|      5 | 2236 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2237 | `	}` |
|      - | 2238 | `	/* Desired comparison length */` |
|     17 | 2239 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 2240 | `	if( n < 0 ){` |
|      - | 2241 | `		/* Invalid length */` |
|      3 | 2242 | `		ph7_result_int(pCtx,-1);` |
|      3 | 2243 | `		return PH7_OK;` |
|      - | 2244 | `	}` |
|      - | 2245 | `	/* Perform the comparison */` |
|     15 | 2246 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 2247 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 2248 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2249 | `	/* Comparison result */` |
|     15 | 2250 | `	ph7_result_int(pCtx,res);` |
|     15 | 2251 | `	return PH7_OK;` |
|     11 | 2252 |  |
|      - | 2253 | `/*` |
|      - | 2254 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2255 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2256 | ` * Parameter` |
|      - | 2257 | ` *  str1: The first string` |
|      - | 2258 | ` *  str2: The second string` |
|      - | 2259 | ` * Return` |
|      - | 2260 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2261 | ` *  than str2, and 0 if they are equal.` |
|      - | 2262 | ` */` |
|     18 | 2263 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2264 |  |
|      - | 2265 | `	const char *z1,*z2;` |
|      - | 2266 | `	int n1,n2;` |
|      - | 2267 | `	int res;` |
|     19 | 2268 | `	if( nArg < 2 ){` |
|      9 | 2269 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 2270 | `		ph7_result_int(pCtx,res);` |
|      9 | 2271 | `		return PH7_OK;` |
|      - | 2272 | `	}` |
|      - | 2273 | `	/* Perform the comparison */` |
|     11 | 2274 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     11 | 2275 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     11 | 2276 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2277 | `	/* Comparison result */` |
|     11 | 2278 | `	ph7_result_int(pCtx,res);` |
|     11 | 2279 | `	return PH7_OK;` |
|     10 | 2280 |  |
|      - | 2281 | `/*` |
|      - | 2282 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2283 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2284 | ` * Parameter` |
|      - | 2285 | ` *  $str1: The first string` |
|      - | 2286 | ` *  $str2: The second string` |
|      - | 2287 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2288 | ` * Return` |
|      - | 2289 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2290 | ` *  than str2, and 0 if they are equal.` |
|      - | 2291 | ` */` |
|      8 | 2292 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2293 |  |
|      - | 2294 | `	const char *z1,*z2;` |
|      - | 2295 | `	int res;` |
|      - | 2296 | `	int n;` |
|      9 | 2297 | `	if( nArg < 3 ){` |
|      - | 2298 | `		/* Perform a standard comparison */` |
|      5 | 2299 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2300 | `	}` |
|      - | 2301 | `	/* Desired comparison length */` |
|      5 | 2302 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 2303 | `	if( n < 0 ){` |
|      - | 2304 | `		/* Invalid length */` |
|    ! 0 | 2305 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 2306 | `		return PH7_OK;` |
|      - | 2307 | `	}` |
|      - | 2308 | `	/* Perform the comparison */` |
|      5 | 2309 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 2310 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 2311 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2312 | `	/* Comparison result */` |
|      5 | 2313 | `	ph7_result_int(pCtx,res);` |
|      5 | 2314 | `	return PH7_OK;` |
|      5 | 2315 |  |
|      - | 2316 | `/*` |
|      - | 2317 | ` * Implode context [i.e: it's private data].` |
|      - | 2318 | ` * A pointer to the following structure is forwarded` |
|      - | 2319 | ` * verbatim to the array walker callback defined below.` |
|      - | 2320 | ` */` |
|      - | 2321 | `struct implode_data {` |
|      - | 2322 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2323 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2324 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2325 | `	int nSeplen;          /* Separator length */` |
|      - | 2326 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2327 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2328 | `};` |
|      - | 2329 | `/*` |
|      - | 2330 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2331 | ` * The following routine is invoked for each array entry passed` |
|      - | 2332 | ` * to the implode() function.` |
|      - | 2333 | ` */` |
|  79178 | 2334 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 2335 |  |
|  39589 | 2336 | `	SXUNUSED(pKey);` |
|  79180 | 2337 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2338 | `	const char *zData;` |
|      - | 2339 | `	int nLen;` |
|  79180 | 2340 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2341 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2342 | `			if( !pData->bFirst ){` |
|      - | 2343 | `				/* append the separator first */` |
|      3 | 2344 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 2345 | `			}else{` |
|    ! 0 | 2346 | `				pData->bFirst = 0;` |
|      - | 2347 | `			}` |
|      1 | 2348 | `		}` |
|      - | 2349 | `		/* Recurse */` |
|      3 | 2350 | `		pData->bFirst = 1;` |
|      3 | 2351 | `		pData->nRecCount++;` |
|      3 | 2352 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2353 | `		pData->nRecCount--;` |
|      3 | 2354 | `		return PH7_OK;` |
|      - | 2355 | `	}` |
|      - | 2356 | `	/* Extract the string representation of the entry value */` |
|  79178 | 2357 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2358 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  79178 | 2359 | `	if( pData->bFirst ){` |
|  17112 | 2360 | `		pData->bFirst = 0;` |
|  70623 | 2361 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2362 | `		/* append the separator first */` |
|  62056 | 2363 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  31027 | 2364 | `	}` |
|      - | 2365 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  79178 | 2366 | `	if( nLen > 0 ){` |
|  72222 | 2367 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  36110 | 2368 | `	}` |
|  79178 | 2369 | `	return PH7_OK;` |
|  39591 | 2370 |  |
|      - | 2371 | `/*` |
|      - | 2372 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2373 | ` * string implode(array $pieces,...)` |
|      - | 2374 | ` *  Join array elements with a string.` |
|      - | 2375 | ` * $glue` |
|      - | 2376 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2377 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2378 | ` * $pieces` |
|      - | 2379 | ` *   The array of strings to implode.` |
|      - | 2380 | ` * Return` |
|      - | 2381 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2382 | ` *  order, with the glue string between each element.` |
|      - | 2383 | ` */` |
|  17136 | 2384 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2385 |  |
|      - | 2386 | `	struct implode_data imp_data;` |
|  17138 | 2387 | `	int i = 1;` |
|  17138 | 2388 | `	if( nArg < 1 ){` |
|      - | 2389 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2390 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2391 | `		return PH7_OK;` |
|      - | 2392 | `	}` |
|      - | 2393 | `	/* Prepare the implode context */` |
|  17138 | 2394 | `	imp_data.pCtx = pCtx;` |
|  17138 | 2395 | `	imp_data.bRecursive = 0;` |
|  17138 | 2396 | `	imp_data.bFirst = 1;` |
|  17138 | 2397 | `	imp_data.nRecCount = 0;` |
|  17138 | 2398 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  17136 | 2399 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   8569 | 2400 | `	}else{` |
|      3 | 2401 | `		imp_data.zSep = 0;` |
|      3 | 2402 | `		imp_data.nSeplen = 0;` |
|      3 | 2403 | `		i = 0;` |
|      - | 2404 | `	}` |
|  17138 | 2405 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2406 | `	/* Start the 'join' process */` |
|  34274 | 2407 | `	while( i < nArg ){` |
|  17138 | 2408 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2409 | `			/* Iterate throw array entries */` |
|  17138 | 2410 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|   8570 | 2411 | `		}else{` |
|      - | 2412 | `			const char *zData;` |
|      - | 2413 | `			int nLen;` |
|      - | 2414 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2415 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2416 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2417 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2418 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2419 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2420 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2421 | `			}` |
|      - | 2422 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2423 | `			if( nLen > 0 ){` |
|    ! 0 | 2424 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 2425 | `			}` |
|      - | 2426 | `		}` |
|  17138 | 2427 | `		i++;` |
|      2 | 2428 | `	}` |
|  17138 | 2429 | `	return PH7_OK;` |
|   8570 | 2430 |  |
|      - | 2431 | `/*` |
|      - | 2432 | ` * Symisc eXtension:` |
|      - | 2433 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2434 | ` * Purpose` |
|      - | 2435 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2436 | ` * Example:` |
|      - | 2437 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2438 | ` *   echo implode_recursive("/",$a);` |
|      - | 2439 | ` *   Will output` |
|      - | 2440 | ` *     usr/home/dean.` |
|      - | 2441 | ` *   While the standard implode would produce.` |
|      - | 2442 | ` *    usr/Array.` |
|      - | 2443 | ` * Parameter` |
|      - | 2444 | ` *  Refer to implode().` |
|      - | 2445 | ` * Return` |
|      - | 2446 | ` *  Refer to implode().` |
|      - | 2447 | ` */` |
|     12 | 2448 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2449 |  |
|      - | 2450 | `	struct implode_data imp_data;` |
|     13 | 2451 | `	int i = 1;` |
|     13 | 2452 | `	if( nArg < 1 ){` |
|      - | 2453 | `		/* Missing argument,return NULL */` |
|      3 | 2454 | `		ph7_result_null(pCtx);` |
|      3 | 2455 | `		return PH7_OK;` |
|      - | 2456 | `	}` |
|      - | 2457 | `	/* Prepare the implode context */` |
|     11 | 2458 | `	imp_data.pCtx = pCtx;` |
|     11 | 2459 | `	imp_data.bRecursive = 1;` |
|     11 | 2460 | `	imp_data.bFirst = 1;` |
|     11 | 2461 | `	imp_data.nRecCount = 0;` |
|     11 | 2462 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2463 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2464 | `	}else{` |
|    ! 0 | 2465 | `		imp_data.zSep = 0;` |
|    ! 0 | 2466 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2467 | `		i = 0;` |
|      - | 2468 | `	}` |
|     11 | 2469 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 2470 | `	/* Start the 'join' process */` |
|     21 | 2471 | `	while( i < nArg ){` |
|     11 | 2472 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2473 | `			/* Iterate throw array entries */` |
|      3 | 2474 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 2475 | `		}else{` |
|      - | 2476 | `			const char *zData;` |
|      - | 2477 | `			int nLen;` |
|      - | 2478 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2479 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2480 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2481 | `			if( imp_data.bFirst ){` |
|      9 | 2482 | `				imp_data.bFirst = 0;` |
|      4 | 2483 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2484 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 2485 | `			}` |
|      - | 2486 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2487 | `			if( nLen > 0 ){` |
|      9 | 2488 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 2489 | `			}` |
|      - | 2490 | `		}` |
|     11 | 2491 | `		i++;` |
|      1 | 2492 | `	}` |
|     11 | 2493 | `	return PH7_OK;` |
|      7 | 2494 |  |
|      - | 2495 | `/*` |
|      - | 2496 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2497 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2498 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2499 | ` * Parameters` |
|      - | 2500 | ` *  $delimiter` |
|      - | 2501 | ` *   The boundary string.` |
|      - | 2502 | ` * $string` |
|      - | 2503 | ` *   The input string.` |
|      - | 2504 | ` * $limit` |
|      - | 2505 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2506 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2507 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2508 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2509 | ` * Returns` |
|      - | 2510 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2511 | ` *  on boundaries formed by the delimiter.` |
|      - | 2512 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2513 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2514 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2515 | ` *  will be returned.` |
|      - | 2516 | ` * NOTE:` |
|      - | 2517 | ` *  Negative limit is not supported.` |
|      - | 2518 | ` */` |
|   3102 | 2519 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2520 |  |
|      - | 2521 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2522 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2523 | `	ph7_value *pArray;` |
|      - | 2524 | `	ph7_value *pValue;` |
|      - | 2525 | `	sxu32 nOfft;` |
|      - | 2526 | `	sxi32 rc;` |
|   3104 | 2527 | `	if( nArg < 2 ){` |
|      - | 2528 | `		/* Missing arguments,return FALSE */` |
|      9 | 2529 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2530 | `		return PH7_OK;` |
|      - | 2531 | `	}` |
|      - | 2532 | `	/* Extract the delimiter */` |
|   3096 | 2533 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3096 | 2534 | `	if( nDelim < 1 ){` |
|      - | 2535 | `		/* Empty delimiter,return FALSE */` |
|      3 | 2536 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2537 | `		return PH7_OK;` |
|      - | 2538 | `	}` |
|      - | 2539 | `	/* Extract the string */` |
|   3094 | 2540 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3094 | 2541 | `	if( nStrlen < 1 ){` |
|      - | 2542 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 2543 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 2544 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 2545 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 2546 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2547 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2548 | `			return PH7_OK;` |
|      - | 2549 | `		}` |
|      3 | 2550 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 2551 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 2552 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 2553 | `		return PH7_OK;` |
|      - | 2554 | `	}` |
|      - | 2555 | `	/* Point to the end of the string */` |
|   3092 | 2556 | `	zEnd = &zString[nStrlen];` |
|      - | 2557 | `	/* Create the array */` |
|   3092 | 2558 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3092 | 2559 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3092 | 2560 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2561 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2562 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2563 | `		return PH7_OK;` |
|      - | 2564 | `	}` |
|      - | 2565 | `	/* Set a defualt limit */` |
|   3092 | 2566 | `	iLimit = SXI32_HIGH;` |
|   3092 | 2567 | `	if( nArg > 2 ){` |
|      9 | 2568 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      9 | 2569 | `		 if( iLimit < 0 ){` |
|      3 | 2570 | `			iLimit = -iLimit;` |
|      1 | 2571 | `		}` |
|      9 | 2572 | `		if( iLimit == 0 ){` |
|      3 | 2573 | `			iLimit = 1;` |
|      1 | 2574 | `		}` |
|      9 | 2575 | `		iLimit--;` |
|      4 | 2576 | `	}` |
|      - | 2577 | `	/* Start exploding */` |
|  37931 | 2578 | `	for(;;){` |
|  75864 | 2579 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  75864 | 2580 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2581 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3092 | 2582 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3092 | 2583 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3092 | 2584 | `			break;` |
|      - | 2585 | `		}` |
|      - | 2586 | `		/* Point to the desired offset */` |
|  72774 | 2587 | `		zCur = &zString[nOfft];` |
|      - | 2588 | `		/* Perform the store operation (may be empty) */` |
|  72774 | 2589 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  72774 | 2590 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 2591 | `		/* Point beyond the delimiter */` |
|  72774 | 2592 | `		zString = &zCur[nDelim];` |
|      - | 2593 | `		/* Reset the cursor */` |
|  72774 | 2594 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 2595 | `	}` |
|      - | 2596 | `	/* Return the freshly created array */` |
|   3092 | 2597 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2598 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2599 | `	 * released as soon we return from this foregin function.` |
|      - | 2600 | `	 */` |
|   3092 | 2601 | `	return PH7_OK;` |
|   1553 | 2602 |  |
|      - | 2603 | `/*` |
|      - | 2604 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2605 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2606 | ` * Parameters` |
|      - | 2607 | ` *  $str` |
|      - | 2608 | ` *   The string that will be trimmed.` |
|      - | 2609 | ` * $charlist` |
|      - | 2610 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2611 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2612 | ` *   With .. you can specify a range of characters.` |
|      - | 2613 | ` * Returns.` |
|      - | 2614 | ` *  Thr processed string.` |
|      - | 2615 | ` * NOTE:` |
|      - | 2616 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2617 | ` */` |
|   7786 | 2618 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2619 |  |
|      - | 2620 | `	const char *zString;` |
|      - | 2621 | `	int nLen;` |
|   7788 | 2622 | `	if( nArg < 1 ){` |
|      - | 2623 | `		/* Missing arguments,return null */` |
|      3 | 2624 | `		ph7_result_null(pCtx);` |
|      3 | 2625 | `		return PH7_OK;` |
|      - | 2626 | `	}` |
|      - | 2627 | `	/* Extract the target string */` |
|   7786 | 2628 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   7786 | 2629 | `	if( nLen < 1 ){` |
|      - | 2630 | `		/* Empty string,return */` |
|   1640 | 2631 | `		ph7_result_string(pCtx,"",0);` |
|   1640 | 2632 | `		return PH7_OK;` |
|      - | 2633 | `	}` |
|      - | 2634 | `	/* Start the trim process */` |
|   6148 | 2635 | `	if( nArg < 2 ){` |
|      - | 2636 | `		SyString sStr;` |
|      - | 2637 | `		/* Remove white spaces and NUL bytes */` |
|   6144 | 2638 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  14904 | 2639 | `		SyStringFullTrimSafe(&sStr);` |
|   6144 | 2640 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3073 | 2641 | `	}else{` |
|      - | 2642 | `		/* Char list */` |
|      - | 2643 | `		const char *zList;` |
|      - | 2644 | `		int nListlen;` |
|      5 | 2645 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2646 | `		if( nListlen < 1 ){` |
|      - | 2647 | `			/* Return the string unchanged */` |
|      3 | 2648 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2649 | `		}else{` |
|      3 | 2650 | `			const char *zEnd = &zString[nLen];` |
|      3 | 2651 | `			const char *zCur = zString;` |
|      - | 2652 | `			const char *zPtr;` |
|      - | 2653 | `			int i;` |
|      - | 2654 | `			/* Left trim */` |
|      4 | 2655 | `			for(;;){` |
|      9 | 2656 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2657 | `					break;` |
|      - | 2658 | `				}` |
|      9 | 2659 | `				zPtr = zCur;` |
|     17 | 2660 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2661 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 2662 | `						zCur++;` |
|      3 | 2663 | `					}` |
|      5 | 2664 | `				}` |
|      9 | 2665 | `				if( zCur == zPtr ){` |
|      - | 2666 | `					/* No match,break immediately */` |
|      3 | 2667 | `					break;` |
|      - | 2668 | `				}` |
|      1 | 2669 | `			}` |
|      - | 2670 | `			/* Right trim */` |
|      3 | 2671 | `			zEnd--;` |
|      4 | 2672 | `			for(;;){` |
|      9 | 2673 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2674 | `					break;` |
|      - | 2675 | `				}` |
|      9 | 2676 | `				zPtr = zEnd;` |
|     17 | 2677 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 2678 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 2679 | `						zEnd--;` |
|      3 | 2680 | `					}` |
|      5 | 2681 | `				}` |
|      9 | 2682 | `				if( zEnd == zPtr ){` |
|      3 | 2683 | `					break;` |
|      - | 2684 | `				}` |
|      1 | 2685 | `			}` |
|      3 | 2686 | `			if( zCur >= zEnd ){` |
|      - | 2687 | `				/* Return the empty string */` |
|    ! 0 | 2688 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2689 | `			}else{` |
|      3 | 2690 | `				zEnd++;` |
|      3 | 2691 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2692 | `			}` |
|      - | 2693 | `		}` |
|      - | 2694 | `	}` |
|   6148 | 2695 | `	return PH7_OK;` |
|   3895 | 2696 |  |
|      - | 2697 | `/*` |
|      - | 2698 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2699 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2700 | ` * Parameters` |
|      - | 2701 | ` *  $str` |
|      - | 2702 | ` *   The string that will be trimmed.` |
|      - | 2703 | ` * $charlist` |
|      - | 2704 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2705 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2706 | ` *   With .. you can specify a range of characters.` |
|      - | 2707 | ` * Returns.` |
|      - | 2708 | ` *  Thr processed string.` |
|      - | 2709 | ` * NOTE:` |
|      - | 2710 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2711 | ` */` |
|     26 | 2712 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2713 |  |
|      - | 2714 | `	const char *zString;` |
|      - | 2715 | `	int nLen;` |
|     27 | 2716 | `	if( nArg < 1 ){` |
|      - | 2717 | `		/* Missing arguments,return null */` |
|      3 | 2718 | `		ph7_result_null(pCtx);` |
|      3 | 2719 | `		return PH7_OK;` |
|      - | 2720 | `	}` |
|      - | 2721 | `	/* Extract the target string */` |
|     25 | 2722 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 2723 | `	if( nLen < 1 ){` |
|      - | 2724 | `		/* Empty string,return */` |
|      5 | 2725 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2726 | `		return PH7_OK;` |
|      - | 2727 | `	}` |
|      - | 2728 | `	/* Start the trim process */` |
|     21 | 2729 | `	if( nArg < 2 ){` |
|      - | 2730 | `		SyString sStr;` |
|      - | 2731 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2732 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2733 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2734 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2735 | `	}else{` |
|      - | 2736 | `		/* Char list */` |
|      - | 2737 | `		const char *zList;` |
|      - | 2738 | `		int nListlen;` |
|      5 | 2739 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2740 | `		if( nListlen < 1 ){` |
|      - | 2741 | `			/* Return the string unchanged */` |
|    ! 0 | 2742 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2743 | `		}else{` |
|      5 | 2744 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 2745 | `			const char *zCur = zString;` |
|      - | 2746 | `			const char *zPtr;` |
|      - | 2747 | `			int i;` |
|      - | 2748 | `			/* Right trim */` |
|      6 | 2749 | `			for(;;){` |
|     13 | 2750 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2751 | `					break;` |
|      - | 2752 | `				}` |
|     13 | 2753 | `				zPtr = zEnd;` |
|     25 | 2754 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 2755 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 2756 | `						zEnd--;` |
|      4 | 2757 | `					}` |
|      7 | 2758 | `				}` |
|     13 | 2759 | `				if( zEnd == zPtr ){` |
|      5 | 2760 | `					break;` |
|      - | 2761 | `				}` |
|      1 | 2762 | `			}` |
|      5 | 2763 | `			if( zEnd <= zCur ){` |
|      - | 2764 | `				/* Return the empty string */` |
|    ! 0 | 2765 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2766 | `			}else{` |
|      5 | 2767 | `				zEnd++;` |
|      5 | 2768 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2769 | `			}` |
|      - | 2770 | `		}` |
|      - | 2771 | `	}` |
|     21 | 2772 | `	return PH7_OK;` |
|     14 | 2773 |  |
|      - | 2774 | `/*` |
|      - | 2775 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2776 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2777 | ` * Parameters` |
|      - | 2778 | ` *  $str` |
|      - | 2779 | ` *   The string that will be trimmed.` |
|      - | 2780 | ` * $charlist` |
|      - | 2781 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2782 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2783 | ` *   With .. you can specify a range of characters.` |
|      - | 2784 | ` * Returns.` |
|      - | 2785 | ` *  Thr processed string.` |
|      - | 2786 | ` * NOTE:` |
|      - | 2787 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2788 | ` */` |
|     12 | 2789 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2790 |  |
|      - | 2791 | `	const char *zString;` |
|      - | 2792 | `	int nLen;` |
|     13 | 2793 | `	if( nArg < 1 ){` |
|      - | 2794 | `		/* Missing arguments,return null */` |
|      3 | 2795 | `		ph7_result_null(pCtx);` |
|      3 | 2796 | `		return PH7_OK;` |
|      - | 2797 | `	}` |
|      - | 2798 | `	/* Extract the target string */` |
|     11 | 2799 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2800 | `	if( nLen < 1 ){` |
|      - | 2801 | `		/* Empty string,return */` |
|    ! 0 | 2802 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2803 | `		return PH7_OK;` |
|      - | 2804 | `	}` |
|      - | 2805 | `	/* Start the trim process */` |
|     11 | 2806 | `	if( nArg < 2 ){` |
|      - | 2807 | `		SyString sStr;` |
|      - | 2808 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2809 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2810 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2811 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2812 | `	}else{` |
|      - | 2813 | `		/* Char list */` |
|      - | 2814 | `		const char *zList;` |
|      - | 2815 | `		int nListlen;` |
|      9 | 2816 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2817 | `		if( nListlen < 1 ){` |
|      - | 2818 | `			/* Return the string unchanged */` |
|      3 | 2819 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2820 | `		}else{` |
|      7 | 2821 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2822 | `			const char *zCur = zString;` |
|      - | 2823 | `			const char *zPtr;` |
|      - | 2824 | `			int i;` |
|      - | 2825 | `			/* Left trim */` |
|      7 | 2826 | `			for(;;){` |
|     15 | 2827 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2828 | `					break;` |
|      - | 2829 | `				}` |
|     15 | 2830 | `				zPtr = zCur;` |
|     41 | 2831 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2832 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2833 | `						zCur++;` |
|      6 | 2834 | `					}` |
|     14 | 2835 | `				}` |
|     15 | 2836 | `				if( zCur == zPtr ){` |
|      - | 2837 | `					/* No match,break immediately */` |
|      7 | 2838 | `					break;` |
|      - | 2839 | `				}` |
|      1 | 2840 | `			}` |
|      7 | 2841 | `			if( zCur >= zEnd ){` |
|      - | 2842 | `				/* Return the empty string */` |
|    ! 0 | 2843 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2844 | `			}else{` |
|      7 | 2845 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2846 | `			}` |
|      - | 2847 | `		}` |
|      - | 2848 | `	}` |
|     11 | 2849 | `	return PH7_OK;` |
|      7 | 2850 |  |
|      - | 2851 | `/*` |
|      - | 2852 | ` * string strtolower(string $str)` |
|      - | 2853 | ` *  Make a string lowercase.` |
|      - | 2854 | ` * Parameters` |
|      - | 2855 | ` *  $str` |
|      - | 2856 | ` *   The input string.` |
|      - | 2857 | ` * Returns.` |
|      - | 2858 | ` *  The lowercased string.` |
|      - | 2859 | ` */` |
|  16998 | 2860 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2861 |  |
|      - | 2862 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2863 | `	int nLen;` |
|  17000 | 2864 | `	if( nArg < 1 ){` |
|      - | 2865 | `		/* Missing arguments,return null */` |
|      3 | 2866 | `		ph7_result_null(pCtx);` |
|      3 | 2867 | `		return PH7_OK;` |
|      - | 2868 | `	}` |
|      - | 2869 | `	/* Extract the target string */` |
|  16998 | 2870 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  16998 | 2871 | `	if( nLen < 1 ){` |
|      - | 2872 | `		/* Empty string,return */` |
|      3 | 2873 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2874 | `		return PH7_OK;` |
|      - | 2875 | `	}` |
|      - | 2876 | `	/* Perform the requested operation */` |
|  16996 | 2877 | `	zEnd = &zString[nLen];` |
|  53733 | 2878 | `	for(;;){` |
| 107468 | 2879 | `		if( zString >= zEnd ){` |
|      - | 2880 | `			/* No more input,break immediately */` |
|  16996 | 2881 | `			break;` |
|      - | 2882 | `		}` |
|  90474 | 2883 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2884 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2885 | `			zCur = zString;` |
|    ! 0 | 2886 | `			zString++;` |
|    ! 0 | 2887 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2888 | `				zString++;` |
|    ! 0 | 2889 | `			}` |
|      - | 2890 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2891 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2892 | `		}else{` |
|  90474 | 2893 | `			int c = zString[0];` |
|  90474 | 2894 | `			if( SyisUpper(c) ){` |
|  90472 | 2895 | `				c = SyToLower(zString[0]);` |
|  45235 | 2896 | `			}` |
|      - | 2897 | `			/* Append character */` |
|  90474 | 2898 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2899 | `			/* Advance the cursor */` |
|  90474 | 2900 | `			zString++;` |
|      - | 2901 | `		}` |
|      2 | 2902 | `	}` |
|  16996 | 2903 | `	return PH7_OK;` |
|   8501 | 2904 |  |
|      - | 2905 | `/*` |
|      - | 2906 | ` * string strtolower(string $str)` |
|      - | 2907 | ` *  Make a string uppercase.` |
|      - | 2908 | ` * Parameters` |
|      - | 2909 | ` *  $str` |
|      - | 2910 | ` *   The input string.` |
|      - | 2911 | ` * Returns.` |
|      - | 2912 | ` *  The uppercased string.` |
|      - | 2913 | ` */` |
|     10 | 2914 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2915 |  |
|      - | 2916 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2917 | `	int nLen;` |
|     11 | 2918 | `	if( nArg < 1 ){` |
|      - | 2919 | `		/* Missing arguments,return null */` |
|      3 | 2920 | `		ph7_result_null(pCtx);` |
|      3 | 2921 | `		return PH7_OK;` |
|      - | 2922 | `	}` |
|      - | 2923 | `	/* Extract the target string */` |
|      9 | 2924 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 2925 | `	if( nLen < 1 ){` |
|      - | 2926 | `		/* Empty string,return */` |
|      3 | 2927 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2928 | `		return PH7_OK;` |
|      - | 2929 | `	}` |
|      - | 2930 | `	/* Perform the requested operation */` |
|      7 | 2931 | `	zEnd = &zString[nLen];` |
|     19 | 2932 | `	for(;;){` |
|     39 | 2933 | `		if( zString >= zEnd ){` |
|      - | 2934 | `			/* No more input,break immediately */` |
|      7 | 2935 | `			break;` |
|      - | 2936 | `		}` |
|     33 | 2937 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2938 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2939 | `			zCur = zString;` |
|    ! 0 | 2940 | `			zString++;` |
|    ! 0 | 2941 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2942 | `				zString++;` |
|    ! 0 | 2943 | `			}` |
|      - | 2944 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2945 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2946 | `		}else{` |
|     33 | 2947 | `			int c = zString[0];` |
|     33 | 2948 | `			if( SyisLower(c) ){` |
|     27 | 2949 | `				c = SyToUpper(zString[0]);` |
|     13 | 2950 | `			}` |
|      - | 2951 | `			/* Append character */` |
|     33 | 2952 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2953 | `			/* Advance the cursor */` |
|     33 | 2954 | `			zString++;` |
|      - | 2955 | `		}` |
|      1 | 2956 | `	}` |
|      7 | 2957 | `	return PH7_OK;` |
|      6 | 2958 |  |
|      - | 2959 | `/*` |
|      - | 2960 | ` * string ucfirst(string $str)` |
|      - | 2961 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2962 | ` *  character is alphabetic.` |
|      - | 2963 | ` * Parameters` |
|      - | 2964 | ` *  $str` |
|      - | 2965 | ` *   The input string.` |
|      - | 2966 | ` * Returns.` |
|      - | 2967 | ` *  The processed string.` |
|      - | 2968 | ` */` |
|      6 | 2969 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2970 |  |
|      - | 2971 | `	const char *zString,*zEnd;` |
|      - | 2972 | `	int nLen,c;` |
|      7 | 2973 | `	if( nArg < 1 ){` |
|      - | 2974 | `		/* Missing arguments,return null */` |
|      3 | 2975 | `		ph7_result_null(pCtx);` |
|      3 | 2976 | `		return PH7_OK;` |
|      - | 2977 | `	}` |
|      - | 2978 | `	/* Extract the target string */` |
|      5 | 2979 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2980 | `	if( nLen < 1 ){` |
|      - | 2981 | `		/* Empty string,return */` |
|      3 | 2982 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2983 | `		return PH7_OK;` |
|      - | 2984 | `	}` |
|      - | 2985 | `	/* Perform the requested operation */` |
|      3 | 2986 | `	zEnd = &zString[nLen];` |
|      3 | 2987 | `	c = zString[0];` |
|      3 | 2988 | `	if( SyisLower(c) ){` |
|      3 | 2989 | `		c = SyToUpper(c);` |
|      1 | 2990 | `	}` |
|      - | 2991 | `	/* Append the first character */` |
|      3 | 2992 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2993 | `	zString++;` |
|      3 | 2994 | `	if( zString < zEnd ){` |
|      - | 2995 | `		/* Append the rest of the input verbatim */` |
|      3 | 2996 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2997 | `	}` |
|      3 | 2998 | `	return PH7_OK;` |
|      4 | 2999 |  |
|      - | 3000 | `/*` |
|      - | 3001 | ` * string lcfirst(string $str)` |
|      - | 3002 | ` *  Make a string's first character lowercase.` |
|      - | 3003 | ` * Parameters` |
|      - | 3004 | ` *  $str` |
|      - | 3005 | ` *   The input string.` |
|      - | 3006 | ` * Returns.` |
|      - | 3007 | ` *  The processed string.` |
|      - | 3008 | ` */` |
|      6 | 3009 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3010 |  |
|      - | 3011 | `	const char *zString,*zEnd;` |
|      - | 3012 | `	int nLen,c;` |
|      7 | 3013 | `	if( nArg < 1 ){` |
|      - | 3014 | `		/* Missing arguments,return null */` |
|      3 | 3015 | `		ph7_result_null(pCtx);` |
|      3 | 3016 | `		return PH7_OK;` |
|      - | 3017 | `	}` |
|      - | 3018 | `	/* Extract the target string */` |
|      5 | 3019 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3020 | `	if( nLen < 1 ){` |
|      - | 3021 | `		/* Empty string,return */` |
|      3 | 3022 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3023 | `		return PH7_OK;` |
|      - | 3024 | `	}` |
|      - | 3025 | `	/* Perform the requested operation */` |
|      3 | 3026 | `	zEnd = &zString[nLen];` |
|      3 | 3027 | `	c = zString[0];` |
|      3 | 3028 | `	if( SyisUpper(c) ){` |
|      3 | 3029 | `		c = SyToLower(c);` |
|      1 | 3030 | `	}` |
|      - | 3031 | `	/* Append the first character */` |
|      3 | 3032 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 3033 | `	zString++;` |
|      3 | 3034 | `	if( zString < zEnd ){` |
|      - | 3035 | `		/* Append the rest of the input verbatim */` |
|      3 | 3036 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 3037 | `	}` |
|      3 | 3038 | `	return PH7_OK;` |
|      4 | 3039 |  |
|      - | 3040 | `/*` |
|      - | 3041 | ` * int ord(string $string)` |
|      - | 3042 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3043 | ` * Parameters` |
|      - | 3044 | ` *  $str` |
|      - | 3045 | ` *   The input string.` |
|      - | 3046 | ` * Returns.` |
|      - | 3047 | ` *  The ASCII value as an integer.` |
|      - | 3048 | ` */` |
|     32 | 3049 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3050 |  |
|      - | 3051 | `	const char *zString;` |
|      - | 3052 | `	int nLen,c;` |
|     33 | 3053 | `	if( nArg < 1 ){` |
|      - | 3054 | `		/* Missing arguments,return -1 */` |
|      3 | 3055 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3056 | `		return PH7_OK;` |
|      - | 3057 | `	}` |
|      - | 3058 | `	/* Extract the target string */` |
|     31 | 3059 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3060 | `	if( nLen < 1 ){` |
|      - | 3061 | `		/* Empty string,return -1 */` |
|      3 | 3062 | `		ph7_result_int(pCtx,-1);` |
|      3 | 3063 | `		return PH7_OK;` |
|      - | 3064 | `	}` |
|      - | 3065 | `	/* Extract the ASCII value of the first character */` |
|     29 | 3066 | `	c = zString[0];` |
|      - | 3067 | `	/* Return that value */` |
|     29 | 3068 | `	ph7_result_int(pCtx,c);` |
|     29 | 3069 | `	return PH7_OK;` |
|     17 | 3070 |  |
|      - | 3071 | `/*` |
|      - | 3072 | ` * string chr(int $ascii)` |
|      - | 3073 | ` *  Returns a one-character string containing the character specified by ascii.` |
|      - | 3074 | ` * Parameters` |
|      - | 3075 | ` *  $ascii` |
|      - | 3076 | ` *   The ascii code.` |
|      - | 3077 | ` * Returns.` |
|      - | 3078 | ` *  The specified character.` |
|      - | 3079 | ` */` |
|     28 | 3080 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3081 |  |
|      - | 3082 | `	int c;` |
|     29 | 3083 | `	if( nArg < 1 ){` |
|      - | 3084 | `		/* Missing arguments,return null */` |
|      3 | 3085 | `		ph7_result_null(pCtx);` |
|      3 | 3086 | `		return PH7_OK;` |
|      - | 3087 | `	}` |
|      - | 3088 | `	/* Extract the ASCII value */` |
|     27 | 3089 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3090 | `	/* Return the specified character */` |
|     27 | 3091 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     27 | 3092 | `	return PH7_OK;` |
|     15 | 3093 |  |
|      - | 3094 | `/*` |
|      - | 3095 | ` * Binary to hex consumer callback.` |
|      - | 3096 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3097 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3098 | ` */` |
|    226 | 3099 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 3100 |  |
|      - | 3101 | `	/* Append hex chunk verbatim */` |
|    227 | 3102 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 3103 | `	return SXRET_OK;` |
|      1 | 3104 |  |
|      - | 3105 |  |
|      - | 3106 | `/*` |
|      - | 3107 | ` * string bin2hex(string $str)` |
|      - | 3108 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3109 | ` * Parameters` |
|      - | 3110 | ` *  $str` |
|      - | 3111 | ` *   The input string.` |
|      - | 3112 | ` * Returns.` |
|      - | 3113 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3114 | ` */` |
|     12 | 3115 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3116 |  |
|      - | 3117 | `	const char *zString;` |
|      - | 3118 | `	int nLen;` |
|     13 | 3119 | `	if( nArg < 1 ){` |
|      - | 3120 | `		/* Missing arguments,return null */` |
|      3 | 3121 | `		ph7_result_null(pCtx);` |
|      3 | 3122 | `		return PH7_OK;` |
|      - | 3123 | `	}` |
|      - | 3124 | `	/* Extract the target string */` |
|     11 | 3125 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3126 | `	if( nLen < 1 ){` |
|      - | 3127 | `		/* Empty string,return */` |
|      3 | 3128 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3129 | `		return PH7_OK;` |
|      - | 3130 | `	}` |
|      - | 3131 | `	/* Perform the requested operation */` |
|      9 | 3132 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 3133 | `	return PH7_OK;` |
|      7 | 3134 |  |
|      - | 3135 |  |
|      - | 3136 | `/* Search callback signature */` |
|      - | 3137 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3138 | `/*` |
|      - | 3139 | ` * Case-insensitive pattern match.` |
|      - | 3140 | ` * Brute force is the default search method used here.` |
|      - | 3141 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3142 | ` * well for short/medium texts on modern hardware.` |
|      - | 3143 | ` */` |
|    118 | 3144 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3145 |  |
|    119 | 3146 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 3147 | `	const char *zIn = (const char *)pText;` |
|    119 | 3148 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 3149 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3150 | `	const char *zPtr,*zPtr2;` |
|      - | 3151 | `	int c,d;` |
|    119 | 3152 | `	if( iPatLen > nLen ){` |
|      - | 3153 | `		/* Don't bother processing */` |
|     33 | 3154 | `		return SXERR_NOTFOUND;` |
|      - | 3155 | `	}` |
|    244 | 3156 | `	for(;;){` |
|    489 | 3157 | `		if( zIn >= zEnd ){` |
|     47 | 3158 | `			break;` |
|      - | 3159 | `		}` |
|    443 | 3160 | `		c = SyToLower(zIn[0]);` |
|    443 | 3161 | `		d = SyToLower(zpIn[0]);` |
|    443 | 3162 | `		if( c == d ){` |
|     41 | 3163 | `			zPtr   = &zIn[1];` |
|     41 | 3164 | `			zPtr2  = &zpIn[1];` |
|     71 | 3165 | `			for(;;){` |
|    143 | 3166 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3167 | `					/* Pattern found */` |
|     41 | 3168 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3169 | `					return SXRET_OK;` |
|      - | 3170 | `				}` |
|    103 | 3171 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3172 | `					break;` |
|      - | 3173 | `				}` |
|    103 | 3174 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3175 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3176 | `				if( c != d ){` |
|    ! 0 | 3177 | `					break;` |
|      - | 3178 | `				}` |
|    103 | 3179 | `				zPtr++; zPtr2++;` |
|      1 | 3180 | `			}` |
|    ! 0 | 3181 | `		}` |
|    403 | 3182 | `		zIn++;` |
|      1 | 3183 | `	}` |
|      - | 3184 | `	/* Pattern not found */` |
|     47 | 3185 | `	return SXERR_NOTFOUND;` |
|     60 | 3186 |  |
|      - | 3187 | `/*` |
|      - | 3188 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3189 | ` *  Find the first occurrence of a string.` |
|      - | 3190 | ` * Parameters` |
|      - | 3191 | ` *  $haystack` |
|      - | 3192 | ` *   The input string.` |
|      - | 3193 | ` * $needle` |
|      - | 3194 | ` *   Search pattern (must be a string).` |
|      - | 3195 | ` * $before_needle` |
|      - | 3196 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3197 | ` *   of the needle (excluding the needle).` |
|      - | 3198 | ` * Return` |
|      - | 3199 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3200 | ` */` |
|     10 | 3201 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3202 |  |
|     11 | 3203 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3204 | `	const char *zBlob,*zPattern;` |
|      - | 3205 | `	int nLen,nPatLen;` |
|      - | 3206 | `	sxu32 nOfft;` |
|      - | 3207 | `	sxi32 rc;` |
|     11 | 3208 | `	if( nArg < 2 ){` |
|      - | 3209 | `		/* Missing arguments,return FALSE */` |
|      5 | 3210 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3211 | `		return PH7_OK;` |
|      - | 3212 | `	}` |
|      - | 3213 | `	/* Extract the needle and the haystack */` |
|      7 | 3214 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3215 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3216 | `	nOfft = 0; /* cc warning */` |
|      9 | 3217 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3218 | `		int before = 0;` |
|      - | 3219 | `		/* Perform the lookup */` |
|      5 | 3220 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3221 | `		if( rc != SXRET_OK ){` |
|      - | 3222 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3223 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3224 | `			return PH7_OK;` |
|      - | 3225 | `		}` |
|      - | 3226 | `		/* Return the portion of the string */` |
|      5 | 3227 | `		if( nArg > 2 ){` |
|      3 | 3228 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3229 | `		}` |
|      5 | 3230 | `		if( before ){` |
|      3 | 3231 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3232 | `		}else{` |
|      3 | 3233 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3234 | `		}` |
|      3 | 3235 | `	}else{` |
|      3 | 3236 | `		ph7_result_bool(pCtx,0);` |
|      - | 3237 | `	}` |
|      7 | 3238 | `	return PH7_OK;` |
|      6 | 3239 |  |
|      - | 3240 | `/*` |
|      - | 3241 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3242 | ` *  Case-insensitive strstr().` |
|      - | 3243 | ` * Parameters` |
|      - | 3244 | ` *  $haystack` |
|      - | 3245 | ` *   The input string.` |
|      - | 3246 | ` * $needle` |
|      - | 3247 | ` *   Search pattern (must be a string).` |
|      - | 3248 | ` * $before_needle` |
|      - | 3249 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3250 | ` *   of the needle (excluding the needle).` |
|      - | 3251 | ` * Return` |
|      - | 3252 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3253 | ` */` |
|      6 | 3254 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3255 |  |
|      7 | 3256 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3257 | `	const char *zBlob,*zPattern;` |
|      - | 3258 | `	int nLen,nPatLen;` |
|      - | 3259 | `	sxu32 nOfft;` |
|      - | 3260 | `	sxi32 rc;` |
|      7 | 3261 | `	if( nArg < 2 ){` |
|      - | 3262 | `		/* Missing arguments,return FALSE */` |
|      3 | 3263 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3264 | `		return PH7_OK;` |
|      - | 3265 | `	}` |
|      - | 3266 | `	/* Extract the needle and the haystack */` |
|      5 | 3267 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3268 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3269 | `	nOfft = 0; /* cc warning */` |
|      7 | 3270 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3271 | `		int before = 0;` |
|      - | 3272 | `		/* Perform the lookup */` |
|      5 | 3273 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3274 | `		if( rc != SXRET_OK ){` |
|      - | 3275 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3276 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3277 | `			return PH7_OK;` |
|      - | 3278 | `		}` |
|      - | 3279 | `		/* Return the portion of the string */` |
|      5 | 3280 | `		if( nArg > 2 ){` |
|      3 | 3281 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3282 | `		}` |
|      5 | 3283 | `		if( before ){` |
|      3 | 3284 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3285 | `		}else{` |
|      3 | 3286 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3287 | `		}` |
|      3 | 3288 | `	}else{` |
|    ! 0 | 3289 | `		ph7_result_bool(pCtx,0);` |
|      - | 3290 | `	}` |
|      5 | 3291 | `	return PH7_OK;` |
|      4 | 3292 |  |
|      - | 3293 | `/*` |
|      - | 3294 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3295 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3296 | ` * Parameters` |
|      - | 3297 | ` *  $haystack` |
|      - | 3298 | ` *   The input string.` |
|      - | 3299 | ` * $needle` |
|      - | 3300 | ` *   Search pattern (must be a string).` |
|      - | 3301 | ` * $offset` |
|      - | 3302 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3303 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3304 | ` *   of haystack.` |
|      - | 3305 | ` * Return` |
|      - | 3306 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3307 | ` */` |
|     80 | 3308 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3309 |  |
|     82 | 3310 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3311 | `	const char *zBlob,*zPattern;` |
|      - | 3312 | `	int nLen,nPatLen,nStart;` |
|      - | 3313 | `	sxu32 nOfft;` |
|      - | 3314 | `	sxi32 rc;` |
|     82 | 3315 | `	if( nArg < 2 ){` |
|      - | 3316 | `		/* Missing arguments,return FALSE */` |
|      7 | 3317 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3318 | `		return PH7_OK;` |
|      - | 3319 | `	}` |
|      - | 3320 | `	/* Extract the needle and the haystack */` |
|     76 | 3321 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 3322 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 3323 | `	nOfft = 0; /* cc warning */` |
|     76 | 3324 | `	nStart = 0;` |
|      - | 3325 | `	/* Peek the starting offset if available */` |
|     76 | 3326 | `	if( nArg > 2 ){` |
|    ! 0 | 3327 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3328 | `		if( nStart < 0 ){` |
|    ! 0 | 3329 | `			nStart = -nStart;` |
|    ! 0 | 3330 | `		}` |
|    ! 0 | 3331 | `		if( nStart >= nLen ){` |
|      - | 3332 | `			/* Invalid offset */` |
|    ! 0 | 3333 | `			nStart = 0;` |
|    ! 0 | 3334 | `		}else{` |
|    ! 0 | 3335 | `			zBlob += nStart;` |
|    ! 0 | 3336 | `			nLen -= nStart;` |
|      - | 3337 | `		}` |
|    ! 0 | 3338 | `	}` |
|     76 | 3339 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3340 | `		/* Perform the lookup */` |
|     74 | 3341 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 3342 | `		if( rc != SXRET_OK ){` |
|      - | 3343 | `			/* Pattern not found,return FALSE */` |
|      3 | 3344 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3345 | `			return PH7_OK;` |
|      - | 3346 | `		}` |
|      - | 3347 | `		/* Return the pattern position */` |
|     72 | 3348 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     37 | 3349 | `	}else{` |
|      3 | 3350 | `		ph7_result_bool(pCtx,0);` |
|      - | 3351 | `	}` |
|     74 | 3352 | `	return PH7_OK;` |
|     42 | 3353 |  |
|      - | 3354 | `/*` |
|      - | 3355 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3356 | ` *  Case-insensitive strpos.` |
|      - | 3357 | ` * Parameters` |
|      - | 3358 | ` *  $haystack` |
|      - | 3359 | ` *   The input string.` |
|      - | 3360 | ` * $needle` |
|      - | 3361 | ` *   Search pattern (must be a string).` |
|      - | 3362 | ` * $offset` |
|      - | 3363 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3364 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3365 | ` *   of haystack.` |
|      - | 3366 | ` * Return` |
|      - | 3367 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3368 | ` */` |
|     18 | 3369 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3370 |  |
|     19 | 3371 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3372 | `	const char *zBlob,*zPattern;` |
|      - | 3373 | `	int nLen,nPatLen,nStart;` |
|      - | 3374 | `	sxu32 nOfft;` |
|      - | 3375 | `	sxi32 rc;` |
|     19 | 3376 | `	if( nArg < 2 ){` |
|      - | 3377 | `		/* Missing arguments,return FALSE */` |
|      3 | 3378 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3379 | `		return PH7_OK;` |
|      - | 3380 | `	}` |
|      - | 3381 | `	/* Extract the needle and the haystack */` |
|     17 | 3382 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 3383 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 3384 | `	nOfft = 0; /* cc warning */` |
|     17 | 3385 | `	nStart = 0;` |
|      - | 3386 | `	/* Peek the starting offset if available */` |
|     17 | 3387 | `	if( nArg > 2 ){` |
|      5 | 3388 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3389 | `		if( nStart < 0 ){` |
|      3 | 3390 | `			nStart = -nStart;` |
|      1 | 3391 | `		}` |
|      5 | 3392 | `		if( nStart >= nLen ){` |
|      - | 3393 | `			/* Invalid offset */` |
|    ! 0 | 3394 | `			nStart = 0;` |
|    ! 0 | 3395 | `		}else{` |
|      5 | 3396 | `			zBlob += nStart;` |
|      5 | 3397 | `			nLen -= nStart;` |
|      - | 3398 | `		}` |
|      2 | 3399 | `	}` |
|     17 | 3400 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3401 | `		/* Perform the lookup */` |
|     17 | 3402 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 3403 | `		if( rc != SXRET_OK ){` |
|      - | 3404 | `			/* Pattern not found,return FALSE */` |
|      3 | 3405 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3406 | `			return PH7_OK;` |
|      - | 3407 | `		}` |
|      - | 3408 | `		/* Return the pattern position */` |
|     15 | 3409 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3410 | `	}else{` |
|    ! 0 | 3411 | `		ph7_result_bool(pCtx,0);` |
|      - | 3412 | `	}` |
|     15 | 3413 | `	return PH7_OK;` |
|     10 | 3414 |  |
|      - | 3415 | `/*` |
|      - | 3416 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3417 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3418 | ` * Parameters` |
|      - | 3419 | ` *  $haystack` |
|      - | 3420 | ` *   The input string.` |
|      - | 3421 | ` * $needle` |
|      - | 3422 | ` *   Search pattern (must be a string).` |
|      - | 3423 | ` * $offset` |
|      - | 3424 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3425 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3426 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3427 | ` * Return` |
|      - | 3428 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3429 | ` */` |
|     32 | 3430 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3431 |  |
|      - | 3432 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 3433 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3434 | `	int nLen,nPatLen;` |
|      - | 3435 | `	sxu32 nOfft;` |
|      - | 3436 | `	sxi32 rc;` |
|     33 | 3437 | `	if( nArg < 2 ){` |
|      - | 3438 | `		/* Missing arguments,return FALSE */` |
|      3 | 3439 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3440 | `		return PH7_OK;` |
|      - | 3441 | `	}` |
|      - | 3442 | `	/* Extract the needle and the haystack */` |
|     31 | 3443 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 3444 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3445 | `	/* Point to the end of the pattern */` |
|     31 | 3446 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 3447 | `	zEnd = &zBlob[nLen];` |
|      - | 3448 | `	/* Save the starting posistion */` |
|     31 | 3449 | `	zStart = zBlob;` |
|     31 | 3450 | `	nOfft = 0; /* cc warning */` |
|      - | 3451 | `	/* Peek the starting offset if available */` |
|     31 | 3452 | `	if( nArg > 2 ){` |
|      - | 3453 | `		int nStart;` |
|     21 | 3454 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3455 | `		if( nStart < 0 ){` |
|     11 | 3456 | `			nStart = -nStart;` |
|     11 | 3457 | `			if( nStart >= nLen ){` |
|      - | 3458 | `				/* Invalid offset */` |
|      3 | 3459 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3460 | `				return PH7_OK;` |
|    ! 0 | 3461 | `			}else{` |
|      9 | 3462 | `				nLen -= nStart;` |
|      9 | 3463 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3464 | `				zEnd = &zBlob[nLen];` |
|      - | 3465 | `			}` |
|      5 | 3466 | `		}else{` |
|     11 | 3467 | `			if( nStart >= nLen ){` |
|      - | 3468 | `				/* Invalid offset */` |
|      5 | 3469 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3470 | `				return PH7_OK;` |
|    ! 0 | 3471 | `			}else{` |
|      7 | 3472 | `				zBlob += nStart;` |
|      7 | 3473 | `				nLen -= nStart;` |
|      - | 3474 | `			}` |
|      - | 3475 | `		}` |
|      7 | 3476 | `	}` |
|     25 | 3477 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3478 | `		/* Perform the lookup */` |
|     57 | 3479 | `		for(;;){` |
|    115 | 3480 | `			if( zBlob >= zPtr ){` |
|     11 | 3481 | `				break;` |
|      - | 3482 | `			}` |
|    105 | 3483 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3484 | `			if( rc == SXRET_OK ){` |
|      - | 3485 | `				/* Pattern found,return it's position */` |
|     13 | 3486 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3487 | `				return PH7_OK;` |
|      - | 3488 | `			}` |
|     93 | 3489 | `			zPtr--;` |
|      1 | 3490 | `		}` |
|      - | 3491 | `		/* Pattern not found,return FALSE */` |
|     11 | 3492 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3493 | `	}else{` |
|      3 | 3494 | `		ph7_result_bool(pCtx,0);` |
|      - | 3495 | `	}` |
|     13 | 3496 | `	return PH7_OK;` |
|     17 | 3497 |  |
|      - | 3498 | `/*` |
|      - | 3499 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3500 | ` *  Case-insensitive strrpos.` |
|      - | 3501 | ` * Parameters` |
|      - | 3502 | ` *  $haystack` |
|      - | 3503 | ` *   The input string.` |
|      - | 3504 | ` * $needle` |
|      - | 3505 | ` *   Search pattern (must be a string).` |
|      - | 3506 | ` * $offset` |
|      - | 3507 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3508 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3509 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3510 | ` * Return` |
|      - | 3511 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3512 | ` */` |
|     28 | 3513 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3514 |  |
|      - | 3515 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3516 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3517 | `	int nLen,nPatLen;` |
|      - | 3518 | `	sxu32 nOfft;` |
|      - | 3519 | `	sxi32 rc;` |
|     29 | 3520 | `	if( nArg < 2 ){` |
|      - | 3521 | `		/* Missing arguments,return FALSE */` |
|      3 | 3522 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3523 | `		return PH7_OK;` |
|      - | 3524 | `	}` |
|      - | 3525 | `	/* Extract the needle and the haystack */` |
|     27 | 3526 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3527 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3528 | `	/* Point to the end of the pattern */` |
|     27 | 3529 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3530 | `	zEnd = &zBlob[nLen];` |
|      - | 3531 | `	/* Save the starting posistion */` |
|     27 | 3532 | `	zStart = zBlob;` |
|     27 | 3533 | `	nOfft = 0; /* cc warning */` |
|      - | 3534 | `	/* Peek the starting offset if available */` |
|     27 | 3535 | `	if( nArg > 2 ){` |
|      - | 3536 | `		int nStart;` |
|     15 | 3537 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3538 | `		if( nStart < 0 ){` |
|      7 | 3539 | `			nStart = -nStart;` |
|      7 | 3540 | `			if( nStart >= nLen ){` |
|      - | 3541 | `				/* Invalid offset */` |
|      3 | 3542 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3543 | `				return PH7_OK;` |
|    ! 0 | 3544 | `			}else{` |
|      5 | 3545 | `				nLen -= nStart;` |
|      5 | 3546 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3547 | `				zEnd = &zBlob[nLen];` |
|      - | 3548 | `			}` |
|      3 | 3549 | `		}else{` |
|      9 | 3550 | `			if( nStart >= nLen ){` |
|      - | 3551 | `				/* Invalid offset */` |
|      5 | 3552 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3553 | `				return PH7_OK;` |
|    ! 0 | 3554 | `			}else{` |
|      5 | 3555 | `				zBlob += nStart;` |
|      5 | 3556 | `				nLen -= nStart;` |
|      - | 3557 | `			}` |
|      - | 3558 | `		}` |
|      4 | 3559 | `	}` |
|     21 | 3560 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3561 | `		/* Perform the lookup */` |
|     44 | 3562 | `		for(;;){` |
|     89 | 3563 | `			if( zBlob >= zPtr ){` |
|      9 | 3564 | `				break;` |
|      - | 3565 | `			}` |
|     81 | 3566 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3567 | `			if( rc == SXRET_OK ){` |
|      - | 3568 | `				/* Pattern found,return it's position */` |
|     11 | 3569 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3570 | `				return PH7_OK;` |
|      - | 3571 | `			}` |
|     71 | 3572 | `			zPtr--;` |
|      1 | 3573 | `		}` |
|      - | 3574 | `		/* Pattern not found,return FALSE */` |
|      9 | 3575 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3576 | `	}else{` |
|      3 | 3577 | `		ph7_result_bool(pCtx,0);` |
|      - | 3578 | `	}` |
|     11 | 3579 | `	return PH7_OK;` |
|     15 | 3580 |  |
|      - | 3581 | `/*` |
|      - | 3582 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3583 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3584 | ` * Parameters` |
|      - | 3585 | ` *  $haystack` |
|      - | 3586 | ` *   The input string.` |
|      - | 3587 | ` * $needle` |
|      - | 3588 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3589 | ` *  This behavior is different from that of strstr().` |
|      - | 3590 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3591 | ` *  as the ordinal value of a character.` |
|      - | 3592 | ` * Return` |
|      - | 3593 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3594 | ` */` |
|     24 | 3595 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3596 |  |
|      - | 3597 | `	const char *zBlob;` |
|      - | 3598 | `	int nLen,c;` |
|     25 | 3599 | `	if( nArg < 2 ){` |
|      - | 3600 | `		/* Missing arguments,return FALSE */` |
|      3 | 3601 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3602 | `		return PH7_OK;` |
|      - | 3603 | `	}` |
|      - | 3604 | `	/* Extract the haystack */` |
|     23 | 3605 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3606 | `	c = 0; /* cc warning */` |
|     23 | 3607 | `	if( nLen > 0 ){` |
|      - | 3608 | `		sxu32 nOfft;` |
|      - | 3609 | `		sxi32 rc;` |
|     21 | 3610 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3611 | `			const char *zPattern;` |
|     11 | 3612 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3613 | `														 * for NULL pointer.` |
|      - | 3614 | `														 */` |
|     11 | 3615 | `			c = zPattern[0];` |
|      6 | 3616 | `		}else{` |
|      - | 3617 | `			/* Int cast */` |
|     11 | 3618 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3619 | `		}` |
|      - | 3620 | `		/* Perform the lookup */` |
|     21 | 3621 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3622 | `		if( rc != SXRET_OK ){` |
|      - | 3623 | `			/* No such entry,return FALSE */` |
|      7 | 3624 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3625 | `			return PH7_OK;` |
|      - | 3626 | `		}` |
|      - | 3627 | `		/* Return the string portion */` |
|     15 | 3628 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3629 | `	}else{` |
|      3 | 3630 | `		ph7_result_bool(pCtx,0);` |
|      - | 3631 | `	}` |
|     17 | 3632 | `	return PH7_OK;` |
|     13 | 3633 |  |
|      - | 3634 | `/*` |
|      - | 3635 | ` * string strrev(string $string)` |
|      - | 3636 | ` *  Reverse a string.` |
|      - | 3637 | ` * Parameters` |
|      - | 3638 | ` *  $string` |
|      - | 3639 | ` *   String to be reversed.` |
|      - | 3640 | ` * Return` |
|      - | 3641 | ` *  The reversed string.` |
|      - | 3642 | ` */` |
|      4 | 3643 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3644 |  |
|      - | 3645 | `	const char *zIn,*zEnd;` |
|      - | 3646 | `	int nLen,c;` |
|      5 | 3647 | `	if( nArg < 1 ){` |
|      - | 3648 | `		/* Missing arguments,return NULL */` |
|      3 | 3649 | `		ph7_result_null(pCtx);` |
|      3 | 3650 | `		return PH7_OK;` |
|      - | 3651 | `	}` |
|      - | 3652 | `	/* Extract the target string */` |
|      3 | 3653 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3654 | `	if( nLen < 1 ){` |
|      - | 3655 | `		/* Empty string Return null */` |
|    ! 0 | 3656 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3657 | `		return PH7_OK;` |
|      - | 3658 | `	}` |
|      - | 3659 | `	/* Perform the requested operation */` |
|      3 | 3660 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3661 | `	for(;;){` |
|      9 | 3662 | `		if( zEnd < zIn ){` |
|      - | 3663 | `			/* No more input to process */` |
|      3 | 3664 | `			break;` |
|      - | 3665 | `		}` |
|      - | 3666 | `		/* Append current character */` |
|      7 | 3667 | `		c = zEnd[0];` |
|      7 | 3668 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3669 | `		zEnd--;` |
|      1 | 3670 | `	}` |
|      3 | 3671 | `	return PH7_OK;` |
|      3 | 3672 |  |
|      - | 3673 | `/*` |
|      - | 3674 | ` * string ucwords(string $string)` |
|      - | 3675 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3676 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3677 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3678 | ` * Parameters` |
|      - | 3679 | ` *  $string` |
|      - | 3680 | ` *   The input string.` |
|      - | 3681 | ` * Return` |
|      - | 3682 | ` *  The modified string..` |
|      - | 3683 | ` */` |
|     14 | 3684 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3685 |  |
|      - | 3686 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3687 | `	int nLen,c;` |
|     15 | 3688 | `	if( nArg < 1 ){` |
|      - | 3689 | `		/* Missing arguments,return NULL */` |
|      3 | 3690 | `		ph7_result_null(pCtx);` |
|      3 | 3691 | `		return PH7_OK;` |
|      - | 3692 | `	}` |
|      - | 3693 | `	/* Extract the target string */` |
|     13 | 3694 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3695 | `	if( nLen < 1 ){` |
|      - | 3696 | `		/* Empty string – match PHP semantics */` |
|      3 | 3697 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3698 | `		return PH7_OK;` |
|      - | 3699 | `	}` |
|      - | 3700 | `	/* Perform the requested operation */` |
|     11 | 3701 | `	zEnd = &zIn[nLen];` |
|     21 | 3702 | `	for(;;){` |
|      - | 3703 | `		/* Jump leading white spaces */` |
|     43 | 3704 | `		zCur = zIn;` |
|     65 | 3705 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3706 | `			zIn++;` |
|      1 | 3707 | `		}` |
|     43 | 3708 | `		if( zCur < zIn ){` |
|      - | 3709 | `			/* Append white space stream */` |
|     23 | 3710 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3711 | `		}` |
|     43 | 3712 | `		if( zIn >= zEnd ){` |
|      - | 3713 | `			/* No more input to process */` |
|     11 | 3714 | `			break;` |
|      - | 3715 | `		}` |
|     33 | 3716 | `		c = zIn[0];` |
|     33 | 3717 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3718 | `			c = SyToUpper(c);` |
|     14 | 3719 | `		}` |
|      - | 3720 | `		/* Append the upper-cased character */` |
|     33 | 3721 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3722 | `		zIn++;` |
|     33 | 3723 | `		zCur = zIn;` |
|      - | 3724 | `		/* Append the word varbatim */` |
|    149 | 3725 | `		while( zIn < zEnd ){` |
|    139 | 3726 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3727 | `				/* UTF-8 stream */` |
|    ! 0 | 3728 | `				zIn++;` |
|    ! 0 | 3729 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3730 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3731 | `				zIn++;` |
|     59 | 3732 | `			}else{` |
|     23 | 3733 | `				break;` |
|      - | 3734 | `			}` |
|      1 | 3735 | `		}` |
|     33 | 3736 | `		if( zCur < zIn ){` |
|     33 | 3737 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3738 | `		}` |
|      1 | 3739 | `	}` |
|     11 | 3740 | `	return PH7_OK;` |
|      8 | 3741 |  |
|      - | 3742 | `/*` |
|      - | 3743 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3744 | ` *  Returns input repeated multiplier times.` |
|      - | 3745 | ` * Parameters` |
|      - | 3746 | ` *  $string` |
|      - | 3747 | ` *   String to be repeated.` |
|      - | 3748 | ` * $multiplier` |
|      - | 3749 | ` *  Number of time the input string should be repeated.` |
|      - | 3750 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3751 | ` *  to 0, the function will return an empty string.` |
|      - | 3752 | ` * Return` |
|      - | 3753 | ` *  The repeated string.` |
|      - | 3754 | ` */` |
|  20212 | 3755 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3756 |  |
|      - | 3757 | `	const char *zIn;` |
|      - | 3758 | `	int nLen,nMul;` |
|      - | 3759 | `	int rc;` |
|  20213 | 3760 | `	if( nArg < 2 ){` |
|      - | 3761 | `		/* Missing arguments,return NULL */` |
|      3 | 3762 | `		ph7_result_null(pCtx);` |
|      3 | 3763 | `		return PH7_OK;` |
|      - | 3764 | `	}` |
|      - | 3765 | `	/* Extract the target string */` |
|  20211 | 3766 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 3767 | `	if( nLen < 1 ){` |
|      - | 3768 | `		/* Empty string.Return null */` |
|    ! 0 | 3769 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3770 | `		return PH7_OK;` |
|      - | 3771 | `	}` |
|      - | 3772 | `	/* Extract the multiplier */` |
|  20211 | 3773 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 3774 | `	if( nMul < 1 ){` |
|      - | 3775 | `		/* Return the empty string */` |
|      3 | 3776 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3777 | `		return PH7_OK;` |
|      - | 3778 | `	}` |
|      - | 3779 | `	/* Perform the requested operation */` |
| 120220 | 3780 | `	for(;;){` |
| 240441 | 3781 | `		if( !nMul ){` |
|  20209 | 3782 | `			break;` |
|      - | 3783 | `		}` |
|      - | 3784 | `		/* Append the copy */` |
| 220233 | 3785 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3786 | `		if( rc != PH7_OK ){` |
|      - | 3787 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3788 | `			break;` |
|      - | 3789 | `		}` |
| 220233 | 3790 | `		nMul--;` |
|      1 | 3791 | `	}` |
|  20209 | 3792 | `	return PH7_OK;` |
|  10107 | 3793 |  |
|      - | 3794 | `/*` |
|      - | 3795 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3796 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3797 | ` * Parameters` |
|      - | 3798 | ` *  $string` |
|      - | 3799 | ` *   The input string.` |
|      - | 3800 | ` * $is_xhtml` |
|      - | 3801 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3802 | ` * Return` |
|      - | 3803 | ` *  The processed string.` |
|      - | 3804 | ` */` |
|      6 | 3805 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3806 |  |
|      - | 3807 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3808 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3809 | `	int nLen;` |
|      7 | 3810 | `	if( nArg < 1 ){` |
|      - | 3811 | `		/* Missing arguments,return the empty string */` |
|      3 | 3812 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3813 | `		return PH7_OK;` |
|      - | 3814 | `	}` |
|      - | 3815 | `	/* Extract the target string */` |
|      5 | 3816 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3817 | `	if( nLen < 1 ){` |
|      - | 3818 | `		/* Empty string,return null */` |
|    ! 0 | 3819 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3820 | `		return PH7_OK;` |
|      - | 3821 | `	}` |
|      5 | 3822 | `	if( nArg > 1 ){` |
|      3 | 3823 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3824 | `	}` |
|      5 | 3825 | `	zEnd = &zIn[nLen];` |
|      - | 3826 | `	/* Perform the requested operation */` |
|      4 | 3827 | `	for(;;){` |
|      9 | 3828 | `		zCur = zIn;` |
|      - | 3829 | `		/* Delimit the string */` |
|     21 | 3830 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3831 | `			zIn++;` |
|      1 | 3832 | `		}` |
|      9 | 3833 | `		if( zCur < zIn ){` |
|      - | 3834 | `			/* Output chunk verbatim */` |
|      9 | 3835 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3836 | `		}` |
|      9 | 3837 | `		if( zIn >= zEnd ){` |
|      - | 3838 | `			/* No more input to process */` |
|      5 | 3839 | `			break;` |
|      - | 3840 | `		}` |
|      - | 3841 | `		/* Output the HTML line break */` |
|      - | 3842 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3843 | `		if( is_xhtml ){` |
|      3 | 3844 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3845 | `		}else{` |
|      3 | 3846 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3847 | `		}` |
|      5 | 3848 | `		zCur = zIn;` |
|      - | 3849 | `		/* Append trailing line */` |
|     11 | 3850 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3851 | `			zIn++;` |
|      1 | 3852 | `		}` |
|      5 | 3853 | `		if( zCur < zIn ){` |
|      - | 3854 | `			/* Output chunk verbatim */` |
|      5 | 3855 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3856 | `		}` |
|      1 | 3857 | `	}` |
|      5 | 3858 | `	return PH7_OK;` |
|      4 | 3859 |  |
|      - | 3860 | `/*` |
|      - | 3861 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3862 | ` *  According to the PHP reference manual.` |
|      - | 3863 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3864 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3865 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3866 | ` * This applies to both sprintf() and printf().` |
|      - | 3867 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3868 | ` * or more of these elements, in order:` |
|      - | 3869 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3870 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3871 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3872 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3873 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3874 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3875 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3876 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3877 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3878 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3879 | ` *   should result in.` |
|      - | 3880 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3881 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3882 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3883 | ` *   limit to the string.` |
|      - | 3884 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3885 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3886 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3887 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3888 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3889 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3890 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3891 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3892 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3893 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3894 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3895 | ` *       g - shorter of %e and %f.` |
|      - | 3896 | ` *       G - shorter of %E and %f.` |
|      - | 3897 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3898 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3899 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3900 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3901 | ` */` |
|      - | 3902 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3903 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3904 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3905 | `/*` |
|      - | 3906 | `** Conversion types fall into various categories as defined by the` |
|      - | 3907 | `** following enumeration.` |
|      - | 3908 | `*/` |
|      - | 3909 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3910 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3911 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3912 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3913 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3914 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3915 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3916 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3917 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3918 |  |
|      - | 3919 | `/*` |
|      - | 3920 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3921 | `*/` |
|      - | 3922 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3923 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3924 | `/*` |
|      - | 3925 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3926 | `** by an instance of the following structure` |
|      - | 3927 | `*/` |
|      - | 3928 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3929 | `struct ph7_fmt_info` |
|      - | 3930 |  |
|      - | 3931 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3932 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3933 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3934 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3935 | `  char *charset; /* The character set for conversion */` |
|      - | 3936 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3937 | `};` |
|      - | 3938 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3939 | `/*` |
|      - | 3940 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3941 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3942 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3943 | `**` |
|      - | 3944 | `** Example:` |
|      - | 3945 | `**     input:     *val = 3.14159` |
|      - | 3946 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3947 | `**` |
|      - | 3948 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3949 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3950 | `** always returned.` |
|      - | 3951 | `*/` |
|    404 | 3952 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3953 |  |
|      - | 3954 | `  sxlongreal d;` |
|      - | 3955 | `  int digit;` |
|      - | 3956 |  |
|    405 | 3957 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3958 | `	  return '0';` |
|      - | 3959 | `  }` |
|    405 | 3960 | `  digit = (int)*val;` |
|    405 | 3961 | `  d = digit;` |
|    405 | 3962 | `   *val = (*val - d)*10.0;` |
|    405 | 3963 | `  return digit + '0' ;` |
|    203 | 3964 |  |
|      - | 3965 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3966 | `/*` |
|      - | 3967 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3968 | ` * used conversion types first.` |
|      - | 3969 | ` */` |
|      - | 3970 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3971 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3972 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3973 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3974 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3975 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3976 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3977 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3978 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3979 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3980 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3981 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3982 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3983 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3984 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3985 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3986 | `};` |
|      - | 3987 | `/*` |
|      - | 3988 | ` * Format a given string.` |
|      - | 3989 | ` * The root program.  All variations call this core.` |
|      - | 3990 | ` * INPUTS:` |
|      - | 3991 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3992 | ` *            1. A pointer to the call context.` |
|      - | 3993 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3994 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3995 | ` *            3. An integer number of characters to be output.` |
|      - | 3996 | ` *               (Note: This number might be zero.)` |
|      - | 3997 | ` *            4. Upper layer private data.` |
|      - | 3998 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3999 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4000 | ` */` |
|    120 | 4001 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4002 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4003 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4004 | `	const char *zIn,    /* Format string */` |
|      - | 4005 | `	int nByte,          /* Format string length */` |
|      - | 4006 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4007 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4008 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4009 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4010 | `	)` |
|      1 | 4011 |  |
|    121 | 4012 | `	char spaces[] = "                                                  ";` |
|      - | 4013 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 4014 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4015 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4016 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4017 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4018 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4019 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4020 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4021 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4022 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4023 | `	ph7_int64 iVal;` |
|      - | 4024 | `	int precision;           /* Precision of the current field */` |
|      - | 4025 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4026 | `	int c,rc,n;` |
|      - | 4027 | `	int length;              /* Length of the field */` |
|      - | 4028 | `	int prefix;` |
|      - | 4029 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4030 | `	int width;               /* Width of the current field */` |
|      - | 4031 | `	int idx;` |
|    121 | 4032 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4033 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4034 | `	/* Start the format process */` |
|    123 | 4035 | `	for(;;){` |
|    247 | 4036 | `		zCur = zIn;` |
|    697 | 4037 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 4038 | `			zIn++;` |
|      1 | 4039 | `		}` |
|    247 | 4040 | `		if( zCur < zIn ){` |
|      - | 4041 | `			/* Consume chunk verbatim */` |
|     95 | 4042 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 4043 | `			if( rc == SXERR_ABORT ){` |
|      - | 4044 | `				/* Callback request an operation abort */` |
|    ! 0 | 4045 | `				break;` |
|      - | 4046 | `			}` |
|     47 | 4047 | `		}` |
|    247 | 4048 | `		if( zIn >= zEnd ){` |
|      - | 4049 | `			/* No more input to process,break immediately */` |
|    119 | 4050 | `			break;` |
|      - | 4051 | `		}` |
|      - | 4052 | `		/* Find out what flags are present */` |
|    129 | 4053 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 4054 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 4055 | `		zIn++; /* Jump the precent sign */` |
|     64 | 4056 | `		do{` |
|    157 | 4057 | `			c = zIn[0];` |
|    157 | 4058 | `			switch( c ){` |
|      9 | 4059 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 4060 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4061 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 4062 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 4063 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4064 | `			case '\'':` |
|    ! 0 | 4065 | `				zIn++;` |
|    ! 0 | 4066 | `				if( zIn < zEnd ){` |
|      - | 4067 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4068 | `					c = zIn[0];` |
|    ! 0 | 4069 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4070 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4071 | `					}` |
|    ! 0 | 4072 | `					c = 0;` |
|    ! 0 | 4073 | `				}` |
|    ! 0 | 4074 | `				break;` |
|    128 | 4075 | `			default:                                       break;` |
|      - | 4076 | `			}` |
|    157 | 4077 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4078 | `		/* Get the field width */` |
|    129 | 4079 | `		width = 0;` |
|    223 | 4080 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 4081 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 4082 | `			zIn++;` |
|      1 | 4083 | `		}` |
|    129 | 4084 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4085 | `			/* Position specifer */` |
|    ! 0 | 4086 | `			if( width > 0 ){` |
|    ! 0 | 4087 | `				n = width;` |
|    ! 0 | 4088 | `				if( vf && n > 0 ){` |
|    ! 0 | 4089 | `					n--;` |
|    ! 0 | 4090 | `				}` |
|    ! 0 | 4091 | `			}` |
|    ! 0 | 4092 | `			zIn++;` |
|    ! 0 | 4093 | `			width = 0;` |
|    ! 0 | 4094 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 4095 | `				flag_zeropad = 1;` |
|    ! 0 | 4096 | `				zIn++;` |
|    ! 0 | 4097 | `			}` |
|    ! 0 | 4098 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4099 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4100 | `				zIn++;` |
|    ! 0 | 4101 | `			}` |
|    ! 0 | 4102 | `		}` |
|    129 | 4103 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4104 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4105 | `		}` |
|      - | 4106 | `		/* Get the precision */` |
|    129 | 4107 | `		precision = -1;` |
|    129 | 4108 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 4109 | `			precision = 0;` |
|     57 | 4110 | `			zIn++;` |
|    145 | 4111 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 4112 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 4113 | `				zIn++;` |
|      1 | 4114 | `			}` |
|     28 | 4115 | `		}` |
|    129 | 4116 | `		if( zIn >= zEnd ){` |
|      - | 4117 | `			/* No more input */` |
|      3 | 4118 | `			break;` |
|      - | 4119 | `		}` |
|      - | 4120 | `		/* Fetch the info entry for the field */` |
|    127 | 4121 | `		pInfo = 0;` |
|    127 | 4122 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 4123 | `		c = zIn[0];` |
|    127 | 4124 | `		zIn++; /* Jump the format specifer */` |
|    699 | 4125 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 4126 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 4127 | `				pInfo = &aFmt[idx];` |
|    125 | 4128 | `				xtype = pInfo->type;` |
|    125 | 4129 | `				break;` |
|      - | 4130 | `			}` |
|    287 | 4131 | `		}` |
|    127 | 4132 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 4133 | `		length = 0;` |
|      - | 4134 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4135 | `		 /*` |
|      - | 4136 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4137 | `		  **` |
|      - | 4138 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4139 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4140 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4141 | `		  **                               field width was negative.` |
|      - | 4142 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4143 | `		  **                               the conversion character.` |
|      - | 4144 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4145 | `		  **   width                       The specified field width.  This is` |
|      - | 4146 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4147 | `		  **   precision                   The specified precision.  The default` |
|      - | 4148 | `		  **                               is -1.` |
|      - | 4149 | `		  */` |
|    127 | 4150 | `		switch(xtype){` |
|    ! 0 | 4151 | `		case PH7_FMT_PERCENT:` |
|      - | 4152 | `			/* A literal percent character */` |
|    ! 0 | 4153 | `			zWorker[0] = '%';` |
|    ! 0 | 4154 | `			length = (int)sizeof(char);` |
|    ! 0 | 4155 | `			break;` |
|      3 | 4156 | `		case PH7_FMT_CHARX:` |
|      - | 4157 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4158 | `			 * with that ASCII value` |
|      - | 4159 | `			 */` |
|      7 | 4160 | `			pArg = NEXT_ARG;` |
|      7 | 4161 | `			if( pArg == 0 ){` |
|      3 | 4162 | `				c = 0;` |
|      2 | 4163 | `			}else{` |
|      5 | 4164 | `				c = ph7_value_to_int(pArg);` |
|      - | 4165 | `			}` |
|      - | 4166 | `			/* NUL byte is an acceptable value */` |
|      7 | 4167 | `			zWorker[0] = (char)c;` |
|      7 | 4168 | `			length = (int)sizeof(char);` |
|      7 | 4169 | `			break;` |
|     12 | 4170 | `		case PH7_FMT_STRING:` |
|      - | 4171 | `			/* the argument is treated as and presented as a string */` |
|     25 | 4172 | `			pArg = NEXT_ARG;` |
|     25 | 4173 | `			if( pArg == 0 ){` |
|    ! 0 | 4174 | `				length = 0;` |
|    ! 0 | 4175 | `			}else{` |
|     25 | 4176 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4177 | `			}` |
|     25 | 4178 | `			if( length < 1 ){` |
|    ! 0 | 4179 | `				zBuf = " ";` |
|    ! 0 | 4180 | `				length = (int)sizeof(char);` |
|    ! 0 | 4181 | `			}` |
|     25 | 4182 | `			if( precision>=0 && precision<length ){` |
|      3 | 4183 | `				length = precision;` |
|      1 | 4184 | `			}` |
|     25 | 4185 | `			if( flag_zeropad ){` |
|      - | 4186 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4187 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4188 | `					spaces[idx] = '0';` |
|    ! 0 | 4189 | `				}` |
|    ! 0 | 4190 | `			}` |
|     25 | 4191 | `			break;` |
|     20 | 4192 | `		case PH7_FMT_RADIX:` |
|     41 | 4193 | `			pArg = NEXT_ARG;` |
|     41 | 4194 | `			if( pArg == 0 ){` |
|    ! 0 | 4195 | `				iVal = 0;` |
|    ! 0 | 4196 | `			}else{` |
|     41 | 4197 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4198 | `			}` |
|      - | 4199 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 4200 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4201 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4202 | `			}` |
|      - | 4203 | `#if 1` |
|      - | 4204 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4205 | `        ** I think this is stupid.*/` |
|     41 | 4206 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4207 | `#else` |
|      - | 4208 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4209 | `        ** but leave the prefix for hex.*/` |
|      - | 4210 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4211 | `#endif` |
|     41 | 4212 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 4213 | `          if( iVal<0 ){` |
|      3 | 4214 | `            iVal = -iVal;` |
|      - | 4215 | `			/* Ticket 1433-003 */` |
|      3 | 4216 | `			if( iVal < 0 ){` |
|      - | 4217 | `				/* Overflow */` |
|    ! 0 | 4218 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4219 | `			}` |
|      3 | 4220 | `            prefix = '-';` |
|     22 | 4221 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 4222 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 4223 | `          else                       prefix = 0;` |
|     12 | 4224 | `        }else{` |
|     19 | 4225 | `			if( iVal<0 ){` |
|    ! 0 | 4226 | `				iVal = -iVal;` |
|      - | 4227 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4228 | `				if( iVal < 0 ){` |
|      - | 4229 | `					/* Overflow */` |
|    ! 0 | 4230 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4231 | `				}` |
|    ! 0 | 4232 | `			}` |
|     19 | 4233 | `			prefix = 0;` |
|      - | 4234 | `		}` |
|     41 | 4235 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 4236 | `          precision = width-(prefix!=0);` |
|      1 | 4237 | `        }` |
|     41 | 4238 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4239 | `        {` |
|      - | 4240 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4241 | `          register int base;` |
|     41 | 4242 | `          cset = pInfo->charset;` |
|     41 | 4243 | `          base = pInfo->base;` |
|     20 | 4244 | `          do{                                           /* Convert to ascii */` |
|     79 | 4245 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 4246 | `            iVal = iVal/base;` |
|     79 | 4247 | `          }while( iVal>0 );` |
|      - | 4248 | `        }` |
|     41 | 4249 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 4250 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 4251 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 4252 | `        }` |
|     41 | 4253 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 4254 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4255 | `          char *pre, x;` |
|      9 | 4256 | `          pre = pInfo->prefix;` |
|      9 | 4257 | `          if( *zBuf!=pre[0] ){` |
|     23 | 4258 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 4259 | `          }` |
|      4 | 4260 | `        }` |
|     41 | 4261 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 4262 | `		break;` |
|     27 | 4263 | `		case PH7_FMT_FLOAT:` |
|      - | 4264 | `		case PH7_FMT_EXP:` |
|      - | 4265 | `		case PH7_FMT_GENERIC:{` |
|      - | 4266 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4267 | `		long double realvalue;` |
|      - | 4268 | `		int  exp;                /* exponent of real numbers */` |
|      - | 4269 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 4270 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 4271 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 4272 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 4273 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 4274 | `		pArg = NEXT_ARG;` |
|     55 | 4275 | `		if( pArg == 0 ){` |
|    ! 0 | 4276 | `			realvalue = 0;` |
|    ! 0 | 4277 | `		}else{` |
|     55 | 4278 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4279 | `		}` |
|      - | 4280 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 4281 | `		 * below assumes a finite positive realvalue. */` |
|     55 | 4282 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 4283 | `			zBuf = "NAN";` |
|    ! 0 | 4284 | `			length = 3;` |
|    ! 0 | 4285 | `			break;` |
|      - | 4286 | `		}` |
|     55 | 4287 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 4288 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 4289 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 4290 | `				zBuf = "-INF";` |
|    ! 0 | 4291 | `				length = 4;` |
|    ! 0 | 4292 | `			}else{` |
|    ! 0 | 4293 | `				zBuf = "INF";` |
|    ! 0 | 4294 | `				length = 3;` |
|      - | 4295 | `			}` |
|    ! 0 | 4296 | `			break;` |
|      - | 4297 | `		}` |
|     55 | 4298 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 4299 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 4300 | `        if( realvalue<0.0 ){` |
|    ! 0 | 4301 | `          realvalue = -realvalue;` |
|    ! 0 | 4302 | `          prefix = '-';` |
|    ! 0 | 4303 | `        }else{` |
|     55 | 4304 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 4305 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 4306 | `          else                         prefix = 0;` |
|      - | 4307 | `        }` |
|     55 | 4308 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 4309 | `        rounder = 0.0;` |
|      - | 4310 | `#if 0` |
|      - | 4311 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 4312 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 4313 | `#else` |
|      - | 4314 | `        /* It makes more sense to use 0.5 */` |
|    387 | 4315 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 4316 | `#endif` |
|     55 | 4317 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 4318 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 4319 | `        exp = 0;` |
|     55 | 4320 | `        if( realvalue>0.0 ){` |
|     59 | 4321 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 4322 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 4323 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 4324 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 4325 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 4326 | `            zBuf = "NaN";` |
|    ! 0 | 4327 | `            length = 3;` |
|    ! 0 | 4328 | `            break;` |
|      - | 4329 | `          }` |
|     27 | 4330 | `        }` |
|     55 | 4331 | `        zBuf = zWorker;` |
|      - | 4332 | `        /*` |
|      - | 4333 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 4334 | `        ** or etFLOAT, as appropriate.` |
|      - | 4335 | `        */` |
|     55 | 4336 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 4337 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 4338 | `          realvalue += rounder;` |
|    ! 0 | 4339 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 4340 | `        }` |
|     55 | 4341 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 4342 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 4343 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 4344 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 4345 | `          }else{` |
|    ! 0 | 4346 | `            precision = precision - exp;` |
|    ! 0 | 4347 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 4348 | `          }` |
|    ! 0 | 4349 | `        }else{` |
|     55 | 4350 | `          flag_rtz = 0;` |
|      - | 4351 | `        }` |
|      - | 4352 | `        /*` |
|      - | 4353 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 4354 | `        ** the precision is too large to fit in buf[].` |
|      - | 4355 | `        */` |
|     55 | 4356 | `        nsd = 0;` |
|     55 | 4357 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 4358 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 4359 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 4360 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 4361 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4362 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 4363 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 4364 | `            *(zBuf++) = '0';` |
|     17 | 4365 | `          }` |
|    355 | 4366 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 4367 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 4368 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 4369 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4370 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4371 | `          }` |
|     55 | 4372 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 4373 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 4374 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 4375 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 4376 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 4377 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 4378 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 4379 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 4380 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 4381 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 4382 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 4383 | `          }` |
|    ! 0 | 4384 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 4385 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 4386 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 4387 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 4388 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 4389 | `            if( exp>=100 ){` |
|    ! 0 | 4390 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 4391 | `              exp %= 100;` |
|    ! 0 | 4392 | `            }` |
|    ! 0 | 4393 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 4394 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 4395 | `          }` |
|      - | 4396 | `        }` |
|      - | 4397 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 4398 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 4399 | `        ** integer conversions.*/` |
|     55 | 4400 | `        length = (int)(zBuf-zWorker);` |
|     55 | 4401 | `        zBuf = zWorker;` |
|      - | 4402 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4403 | `        ** set and we are not left justified */` |
|     55 | 4404 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4405 | `          int i;` |
|      3 | 4406 | `          int nPad = width - length;` |
|     13 | 4407 | `          for(i=width; i>=nPad; i--){` |
|     11 | 4408 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 4409 | `          }` |
|      3 | 4410 | `          i = prefix!=0;` |
|      5 | 4411 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 4412 | `          length = width;` |
|      1 | 4413 | `        }` |
|      - | 4414 | `#else` |
|      - | 4415 | `         zBuf = " ";` |
|      - | 4416 | `		 length = (int)sizeof(char);` |
|      - | 4417 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 4418 | `		 break;` |
|      - | 4419 | `							 }` |
|      1 | 4420 | `		default:` |
|      - | 4421 | `			/* Invalid format specifer */` |
|      3 | 4422 | `			zWorker[0] = '?';` |
|      3 | 4423 | `			length = (int)sizeof(char);` |
|      2 | 4424 | `			break;` |
|      - | 4425 | `		}` |
|      - | 4426 | `		 /*` |
|      - | 4427 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4428 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4429 | `		 ** the output.` |
|      - | 4430 | `		 */` |
|    127 | 4431 | `    if( !flag_leftjustify ){` |
|      - | 4432 | `      register int nspace;` |
|    119 | 4433 | `      nspace = width-length;` |
|    119 | 4434 | `      if( nspace>0 ){` |
|      5 | 4435 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4436 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4437 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4438 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4439 | `			}` |
|    ! 0 | 4440 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4441 | `        }` |
|      5 | 4442 | `        if( nspace>0 ){` |
|      5 | 4443 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 4444 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4445 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4446 | `			}` |
|      2 | 4447 | `		}` |
|      2 | 4448 | `      }` |
|     59 | 4449 | `    }` |
|    127 | 4450 | `    if( length>0 ){` |
|    127 | 4451 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 4452 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4453 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4454 | `		}` |
|     63 | 4455 | `    }` |
|    127 | 4456 | `    if( flag_leftjustify ){` |
|      - | 4457 | `      register int nspace;` |
|      9 | 4458 | `      nspace = width-length;` |
|      9 | 4459 | `      if( nspace>0 ){` |
|      9 | 4460 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4461 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4462 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4463 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4464 | `			}` |
|    ! 0 | 4465 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4466 | `        }` |
|      9 | 4467 | `        if( nspace>0 ){` |
|      9 | 4468 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4469 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4470 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4471 | `			}` |
|      4 | 4472 | `		}` |
|      4 | 4473 | `      }` |
|      4 | 4474 | `    }` |
|      1 | 4475 | ` }/* for(;;) */` |
|    121 | 4476 | `	return SXRET_OK;` |
|     61 | 4477 |  |
|      - | 4478 | `/*` |
|      - | 4479 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4480 | ` */` |
|     84 | 4481 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4482 |  |
|      - | 4483 | `	/* Consume directly */` |
|     85 | 4484 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 4485 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 4486 | `	return PH7_OK;` |
|      1 | 4487 |  |
|      - | 4488 | `/*` |
|      - | 4489 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4490 | ` *  Return a formatted string.` |
|      - | 4491 | ` * Parameters` |
|      - | 4492 | ` *  $format` |
|      - | 4493 | ` *    The format string (see block comment above)` |
|      - | 4494 | ` * Return` |
|      - | 4495 | ` *  A string produced according to the formatting string format.` |
|      - | 4496 | ` */` |
|     56 | 4497 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4498 |  |
|      - | 4499 | `	const char *zFormat;` |
|      - | 4500 | `	int nLen;` |
|     57 | 4501 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4502 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4503 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4504 | `		return PH7_OK;` |
|      - | 4505 | `	}` |
|      - | 4506 | `	/* Extract the string format */` |
|     55 | 4507 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 4508 | `	if( nLen < 1 ){` |
|      - | 4509 | `		/* Empty string */` |
|    ! 0 | 4510 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4511 | `		return PH7_OK;` |
|      - | 4512 | `	}` |
|      - | 4513 | `	/* Format the string */` |
|     55 | 4514 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 4515 | `	return PH7_OK;` |
|     29 | 4516 |  |
|      - | 4517 | `/*` |
|      - | 4518 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4519 | ` */` |
|    110 | 4520 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4521 |  |
|    111 | 4522 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4523 | `	/* Call the VM output consumer directly */` |
|    111 | 4524 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4525 | `	/* Increment counter */` |
|    111 | 4526 | `	*pCounter += nLen;` |
|    111 | 4527 | `	return PH7_OK;` |
|      1 | 4528 |  |
|      - | 4529 | `/*` |
|      - | 4530 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4531 | ` *  Output a formatted string.` |
|      - | 4532 | ` * Parameters` |
|      - | 4533 | ` *  $format` |
|      - | 4534 | ` *   See sprintf() for a description of format.` |
|      - | 4535 | ` * Return` |
|      - | 4536 | ` *  The length of the outputted string.` |
|      - | 4537 | ` */` |
|     42 | 4538 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4539 |  |
|     43 | 4540 | `	ph7_int64 nCounter = 0;` |
|      - | 4541 | `	const char *zFormat;` |
|      - | 4542 | `	int nLen;` |
|     43 | 4543 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4544 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4545 | `		ph7_result_int(pCtx,0);` |
|      3 | 4546 | `		return PH7_OK;` |
|      - | 4547 | `	}` |
|      - | 4548 | `	/* Extract the string format */` |
|     41 | 4549 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 4550 | `	if( nLen < 1 ){` |
|      - | 4551 | `		/* Empty string */` |
|    ! 0 | 4552 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4553 | `		return PH7_OK;` |
|      - | 4554 | `	}` |
|      - | 4555 | `	/* Format the string */` |
|     41 | 4556 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4557 | `	/* Return the length of the outputted string */` |
|     41 | 4558 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 4559 | `	return PH7_OK;` |
|     22 | 4560 |  |
|      - | 4561 | `/*` |
|      - | 4562 | ` * int vprintf(string $format,array $args)` |
|      - | 4563 | ` *  Output a formatted string.` |
|      - | 4564 | ` * Parameters` |
|      - | 4565 | ` *  $format` |
|      - | 4566 | ` *   See sprintf() for a description of format.` |
|      - | 4567 | ` * Return` |
|      - | 4568 | ` *  The length of the outputted string.` |
|      - | 4569 | ` */` |
|      2 | 4570 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4571 |  |
|      3 | 4572 | `	ph7_int64 nCounter = 0;` |
|      - | 4573 | `	const char *zFormat;` |
|      - | 4574 | `	ph7_hashmap *pMap;` |
|      - | 4575 | `	SySet sArg;` |
|      - | 4576 | `	int nLen,n;` |
|      3 | 4577 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4578 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4579 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4580 | `		return PH7_OK;` |
|      - | 4581 | `	}` |
|      - | 4582 | `	/* Extract the string format */` |
|      3 | 4583 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4584 | `	if( nLen < 1 ){` |
|      - | 4585 | `		/* Empty string */` |
|    ! 0 | 4586 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4587 | `		return PH7_OK;` |
|      - | 4588 | `	}` |
|      - | 4589 | `	/* Point to the hashmap */` |
|      3 | 4590 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4591 | `	/* Extract arguments from the hashmap */` |
|      3 | 4592 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4593 | `	/* Format the string */` |
|      3 | 4594 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4595 | `	/* Return the length of the outputted string */` |
|      3 | 4596 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4597 | `	/* Release the container */` |
|      3 | 4598 | `	SySetRelease(&sArg);` |
|      3 | 4599 | `	return PH7_OK;` |
|      2 | 4600 |  |
|      - | 4601 | `/*` |
|      - | 4602 | ` * int vsprintf(string $format,array $args)` |
|      - | 4603 | ` *  Output a formatted string.` |
|      - | 4604 | ` * Parameters` |
|      - | 4605 | ` *  $format` |
|      - | 4606 | ` *   See sprintf() for a description of format.` |
|      - | 4607 | ` * Return` |
|      - | 4608 | ` *  A string produced according to the formatting string format.` |
|      - | 4609 | ` */` |
|     10 | 4610 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4611 |  |
|      - | 4612 | `	const char *zFormat;` |
|      - | 4613 | `	ph7_hashmap *pMap;` |
|      - | 4614 | `	SySet sArg;` |
|      - | 4615 | `	int nLen,n;` |
|     11 | 4616 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4617 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4618 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4619 | `		return PH7_OK;` |
|      - | 4620 | `	}` |
|      - | 4621 | `	/* Extract the string format */` |
|      7 | 4622 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4623 | `	if( nLen < 1 ){` |
|      - | 4624 | `		/* Empty string */` |
|    ! 0 | 4625 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4626 | `		return PH7_OK;` |
|      - | 4627 | `	}` |
|      - | 4628 | `	/* Point to hashmap */` |
|      7 | 4629 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4630 | `	/* Extract arguments from the hashmap */` |
|      7 | 4631 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4632 | `	/* Format the string */` |
|      7 | 4633 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4634 | `	/* Release the container */` |
|      7 | 4635 | `	SySetRelease(&sArg);` |
|      7 | 4636 | `	return PH7_OK;` |
|      6 | 4637 |  |
|      - | 4638 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4639 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4640 | `/*` |
|      - | 4641 | ` * Symisc eXtension.` |
|      - | 4642 | ` * string size_format(int64 $size)` |
|      - | 4643 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4644 | ` *  Example:` |
|      - | 4645 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4646 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4647 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4648 | ` * Parameter` |
|      - | 4649 | ` *  $size` |
|      - | 4650 | ` *    Entity size in bytes.` |
|      - | 4651 | ` * Return` |
|      - | 4652 | ` *   Formatted string representation of the given size.` |
|      - | 4653 | ` */` |
|     24 | 4654 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4655 |  |
|      - | 4656 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4657 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4658 | `	sxi32 nRest,i_32;` |
|      - | 4659 | `	ph7_int64 iSize;` |
|     25 | 4660 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4661 |  |
|     25 | 4662 | `	if( nArg < 1 ){` |
|      - | 4663 | `		/* Missing argument,return the empty string */` |
|      3 | 4664 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4665 | `		return PH7_OK;` |
|      - | 4666 | `	}` |
|      - | 4667 | `	/* Extract the given size */` |
|     23 | 4668 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4669 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4670 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4671 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4672 | `		return PH7_OK;` |
|      - | 4673 | `	}` |
|     19 | 4674 | `	for(;;){` |
|     39 | 4675 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4676 | `		iSize >>= 10;` |
|     39 | 4677 | `		c++;` |
|     39 | 4678 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4679 | `			break;` |
|      - | 4680 | `		}` |
|      1 | 4681 | `	}` |
|     19 | 4682 | `	nRest /= 100;` |
|     19 | 4683 | `	if( nRest > 9 ){` |
|    ! 0 | 4684 | `		nRest = 9;` |
|    ! 0 | 4685 | `	}` |
|     19 | 4686 | `	if( iSize > 999 ){` |
|    ! 0 | 4687 | `		c++;` |
|    ! 0 | 4688 | `		nRest = 9;` |
|    ! 0 | 4689 | `		iSize = 0;` |
|    ! 0 | 4690 | `	}` |
|     19 | 4691 | `	i_32 = (sxi32)iSize;` |
|      - | 4692 | `	/* Format */` |
|     19 | 4693 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4694 | `	return PH7_OK;` |
|     13 | 4695 |  |
|      - | 4696 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4697 | `/*` |
|      - | 4698 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4699 | ` *   Calculate the md5 hash of a string.` |
|      - | 4700 | ` * Parameter` |
|      - | 4701 | ` *  $str` |
|      - | 4702 | ` *   Input string` |
|      - | 4703 | ` * $raw_output` |
|      - | 4704 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4705 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4706 | ` * Return` |
|      - | 4707 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4708 | ` */` |
|     10 | 4709 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4710 |  |
|      - | 4711 | `	unsigned char zDigest[16];` |
|     11 | 4712 | `	int raw_output = FALSE;` |
|      - | 4713 | `	const void *pIn;` |
|      - | 4714 | `	int nLen;` |
|     11 | 4715 | `	if( nArg < 1 ){` |
|      - | 4716 | `		/* Missing arguments,return the empty string */` |
|      3 | 4717 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4718 | `		return PH7_OK;` |
|      - | 4719 | `	}` |
|      - | 4720 | `	/* Extract the input string */` |
|      9 | 4721 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4722 | `	if( nLen < 1 ){` |
|      - | 4723 | `		/* Empty string */` |
|    ! 0 | 4724 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4725 | `		return PH7_OK;` |
|      - | 4726 | `	}` |
|      9 | 4727 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4728 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4729 | `	}` |
|      - | 4730 | `	/* Compute the MD5 digest */` |
|      9 | 4731 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4732 | `	if( raw_output ){` |
|      - | 4733 | `		/* Output raw digest */` |
|      3 | 4734 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4735 | `	}else{` |
|      - | 4736 | `		/* Perform a binary to hex conversion */` |
|      7 | 4737 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4738 | `	}` |
|      9 | 4739 | `	return PH7_OK;` |
|      6 | 4740 |  |
|      - | 4741 | `/*` |
|      - | 4742 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4743 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4744 | ` * Parameter` |
|      - | 4745 | ` *  $str` |
|      - | 4746 | ` *   Input string` |
|      - | 4747 | ` * $raw_output` |
|      - | 4748 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4749 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4750 | ` * Return` |
|      - | 4751 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4752 | ` */` |
|      8 | 4753 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4754 |  |
|      - | 4755 | `	unsigned char zDigest[20];` |
|      9 | 4756 | `	int raw_output = FALSE;` |
|      - | 4757 | `	const void *pIn;` |
|      - | 4758 | `	int nLen;` |
|      9 | 4759 | `	if( nArg < 1 ){` |
|      - | 4760 | `		/* Missing arguments,return the empty string */` |
|      3 | 4761 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4762 | `		return PH7_OK;` |
|      - | 4763 | `	}` |
|      - | 4764 | `	/* Extract the input string */` |
|      7 | 4765 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4766 | `	if( nLen < 1 ){` |
|      - | 4767 | `		/* Empty string */` |
|    ! 0 | 4768 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4769 | `		return PH7_OK;` |
|      - | 4770 | `	}` |
|      7 | 4771 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4772 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4773 | `	}` |
|      - | 4774 | `	/* Compute the SHA1 digest */` |
|      7 | 4775 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4776 | `	if( raw_output ){` |
|      - | 4777 | `		/* Output raw digest */` |
|      3 | 4778 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4779 | `	}else{` |
|      - | 4780 | `		/* Perform a binary to hex conversion */` |
|      5 | 4781 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4782 | `	}` |
|      7 | 4783 | `	return PH7_OK;` |
|      5 | 4784 |  |
|      - | 4785 | `/*` |
|      - | 4786 | ` * int64 crc32(string $str)` |
|      - | 4787 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4788 | ` * Parameter` |
|      - | 4789 | ` *  $str` |
|      - | 4790 | ` *   Input string` |
|      - | 4791 | ` * Return` |
|      - | 4792 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4793 | ` */` |
|      4 | 4794 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4795 |  |
|      - | 4796 | `	const void *pIn;` |
|      - | 4797 | `	sxu32 nCRC;` |
|      - | 4798 | `	int nLen;` |
|      5 | 4799 | `	if( nArg < 1 ){` |
|      - | 4800 | `		/* Missing arguments,return 0 */` |
|      3 | 4801 | `		ph7_result_int(pCtx,0);` |
|      3 | 4802 | `		return PH7_OK;` |
|      - | 4803 | `	}` |
|      - | 4804 | `	/* Extract the input string */` |
|      3 | 4805 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4806 | `	if( nLen < 1 ){` |
|      - | 4807 | `		/* Empty string */` |
|    ! 0 | 4808 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4809 | `		return PH7_OK;` |
|      - | 4810 | `	}` |
|      - | 4811 | `	/* Calculate the sum */` |
|      3 | 4812 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4813 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4814 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4815 | `	return PH7_OK;` |
|      3 | 4816 |  |
|      - | 4817 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4818 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4819 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4820 | `/*` |
|      - | 4821 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4822 |  |
|      - | 4823 | ` */` |
|      4 | 4824 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4825 | `	const char *zInput, /* Raw input */` |
|      - | 4826 | `	int nByte,  /* Input length */` |
|      - | 4827 | `	int delim,  /* Delimiter */` |
|      - | 4828 | `	int encl,   /* Enclosure */` |
|      - | 4829 | `	int escape,  /* Escape character */` |
|      - | 4830 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4831 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4832 | `	)` |
|      1 | 4833 |  |
|      5 | 4834 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4835 | `	const char *zIn = zInput;` |
|      - | 4836 | `	const char *zPtr;` |
|      - | 4837 | `	int isEnc;` |
|      - | 4838 | `	/* Start processing */` |
|      8 | 4839 | `	for(;;){` |
|     17 | 4840 | `		if( zIn >= zEnd ){` |
|      - | 4841 | `			/* No more input to process */` |
|      5 | 4842 | `			break;` |
|      - | 4843 | `		}` |
|     13 | 4844 | `		isEnc = 0;` |
|     13 | 4845 | `		zPtr = zIn;` |
|      - | 4846 | `		/* Find the first delimiter */` |
|     27 | 4847 | `		while( zIn < zEnd ){` |
|     23 | 4848 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4849 | `				/* Delimiter found,break imediately */` |
|      5 | 4850 | `				break;` |
|     15 | 4851 | `			}else if( zIn[0] == encl ){` |
|      - | 4852 | `				/* Inside enclosure? */` |
|    ! 0 | 4853 | `				isEnc = !isEnc;` |
|     15 | 4854 | `			}else if( zIn[0] == escape ){` |
|      - | 4855 | `				/* Escape sequence */` |
|    ! 0 | 4856 | `				zIn++;` |
|    ! 0 | 4857 | `			}` |
|      - | 4858 | `			/* Advance the cursor */` |
|     15 | 4859 | `			zIn++;` |
|      1 | 4860 | `		}` |
|     13 | 4861 | `		if( zIn > zPtr ){` |
|     13 | 4862 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4863 | `			sxi32 rc;` |
|      - | 4864 | `			/* Invoke the supllied callback */` |
|     13 | 4865 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4866 | `				zPtr++;` |
|    ! 0 | 4867 | `				nByteChunk-=2;` |
|    ! 0 | 4868 | `			}` |
|     13 | 4869 | `			if( nByteChunk > 0 ){` |
|     13 | 4870 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4871 | `				if( rc == SXERR_ABORT ){` |
|      - | 4872 | `					/* User callback request an operation abort */` |
|    ! 0 | 4873 | `					break;` |
|      - | 4874 | `				}` |
|      6 | 4875 | `			}` |
|      6 | 4876 | `		}` |
|      - | 4877 | `		/* Ignore trailing delimiter */` |
|     21 | 4878 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4879 | `			zIn++;` |
|      1 | 4880 | `		}` |
|      1 | 4881 | `	}` |
|      5 | 4882 | `	return SXRET_OK;` |
|      1 | 4883 |  |
|      - | 4884 | `/*` |
|      - | 4885 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4886 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4887 | ` * argument to this callback.` |
|      - | 4888 | ` */` |
|     12 | 4889 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4890 |  |
|     13 | 4891 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4892 | `	ph7_value sEntry;` |
|      - | 4893 | `	SyString sToken;` |
|      - | 4894 | `	/* Insert the token in the given array */` |
|     13 | 4895 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4896 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4897 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4898 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4899 | `		return SXRET_OK;` |
|      - | 4900 | `	}` |
|     13 | 4901 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4902 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4903 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4904 | `	return SXRET_OK;` |
|      7 | 4905 |  |
|      - | 4906 | `/*` |
|      - | 4907 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4908 | ` *  Parse a CSV string into an array.` |
|      - | 4909 | ` * Parameters` |
|      - | 4910 | ` *  $input` |
|      - | 4911 | ` *   The string to parse.` |
|      - | 4912 | ` *  $delimiter` |
|      - | 4913 | ` *   Set the field delimiter (one character only).` |
|      - | 4914 | ` *  $enclosure` |
|      - | 4915 | ` *   Set the field enclosure character (one character only).` |
|      - | 4916 | ` *  $escape` |
|      - | 4917 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4918 | ` * Return` |
|      - | 4919 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4920 | ` */` |
|      4 | 4921 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4922 |  |
|      - | 4923 | `	const char *zInput,*zPtr;` |
|      - | 4924 | `	ph7_value *pArray;` |
|      5 | 4925 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4926 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4927 | `	int escape = '\\';  /* Escape character */` |
|      - | 4928 | `	int nLen;` |
|      5 | 4929 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4930 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4931 | `		ph7_result_null(pCtx);` |
|      3 | 4932 | `		return PH7_OK;` |
|      - | 4933 | `	}` |
|      - | 4934 | `	/* Extract the raw input */` |
|      3 | 4935 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4936 | `	if( nArg > 1 ){` |
|      - | 4937 | `		int i;` |
|      3 | 4938 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4939 | `			/* Extract the delimiter */` |
|      3 | 4940 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4941 | `			if( i > 0 ){` |
|      3 | 4942 | `				delim = zPtr[0];` |
|      1 | 4943 | `			}` |
|      1 | 4944 | `		}` |
|      3 | 4945 | `		if( nArg > 2 ){` |
|      3 | 4946 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4947 | `				/* Extract the enclosure */` |
|      3 | 4948 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4949 | `				if( i > 0 ){` |
|      3 | 4950 | `					encl = zPtr[0];` |
|      1 | 4951 | `				}` |
|      1 | 4952 | `			}` |
|      3 | 4953 | `			if( nArg > 3 ){` |
|      3 | 4954 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4955 | `					/* Extract the escape character */` |
|      3 | 4956 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4957 | `					if( i > 0 ){` |
|      3 | 4958 | `						escape = zPtr[0];` |
|      1 | 4959 | `					}` |
|      1 | 4960 | `				}` |
|      1 | 4961 | `			}` |
|      1 | 4962 | `		}` |
|      1 | 4963 | `	}` |
|      - | 4964 | `	/* Create our array */` |
|      3 | 4965 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4966 | `	if( pArray == 0 ){` |
|    ! 0 | 4967 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4968 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4969 | `		return PH7_OK;` |
|      - | 4970 | `	}` |
|      - | 4971 | `	/* Parse the raw input */` |
|      3 | 4972 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4973 | `	/* Return the freshly created array */` |
|      3 | 4974 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4975 | `	return PH7_OK;` |
|      3 | 4976 |  |
|      - | 4977 | `/*` |
|      - | 4978 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4979 | ` * container.` |
|      - | 4980 | ` * Refer to [strip_tags()].` |
|      - | 4981 | ` */` |
|     10 | 4982 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4983 |  |
|     11 | 4984 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4985 | `	const char *zPtr;` |
|      - | 4986 | `	SyString sEntry;` |
|      - | 4987 | `	/* Strip tags */` |
|     10 | 4988 | `	for(;;){` |
|     45 | 4989 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4990 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4991 | `				zTag++;` |
|      1 | 4992 | `		}` |
|     21 | 4993 | `		if( zTag >= zEnd ){` |
|     11 | 4994 | `			break;` |
|      - | 4995 | `		}` |
|     11 | 4996 | `		zPtr = zTag;` |
|      - | 4997 | `		/* Delimit the tag */` |
|     25 | 4998 | `		while(zTag < zEnd ){` |
|     25 | 4999 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5000 | `				/* UTF-8 stream */` |
|      3 | 5001 | `				zTag++;` |
|      5 | 5002 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5003 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5004 | `				break;` |
|    ! 0 | 5005 | `			}else{` |
|     13 | 5006 | `				zTag++;` |
|      - | 5007 | `			}` |
|      1 | 5008 | `		}` |
|     11 | 5009 | `		if( zTag > zPtr ){` |
|      - | 5010 | `			/* Perform the insertion */` |
|     11 | 5011 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5012 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5013 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5014 | `		}` |
|      - | 5015 | `		/* Jump the trailing '>' */` |
|     11 | 5016 | `		zTag++;` |
|      1 | 5017 | `	}` |
|     11 | 5018 | `	return SXRET_OK;` |
|      1 | 5019 |  |
|      - | 5020 | `/*` |
|      - | 5021 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5022 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5023 | ` * Refer to [strip_tags()].` |
|      - | 5024 | ` */` |
|     36 | 5025 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5026 |  |
|     37 | 5027 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5028 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5029 | `		SyString sTag;` |
|     85 | 5030 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5031 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5032 | `			zTag++;` |
|      1 | 5033 | `		}` |
|      - | 5034 | `		/* Delimit the tag */` |
|     25 | 5035 | `		zCur = zTag;` |
|     77 | 5036 | `		while(zTag < zEnd ){` |
|     77 | 5037 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5038 | `				/* UTF-8 stream */` |
|      5 | 5039 | `				zTag++;` |
|      9 | 5040 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5041 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5042 | `				break;` |
|    ! 0 | 5043 | `			}else{` |
|     49 | 5044 | `				zTag++;` |
|      - | 5045 | `			}` |
|      1 | 5046 | `		}` |
|     25 | 5047 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5048 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5049 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5050 | `		if( sTag.nByte > 0 ){` |
|      - | 5051 | `			SyString *aEntry,*pEntry;` |
|      - | 5052 | `			sxi32 rc;` |
|      - | 5053 | `			sxu32 n;` |
|      - | 5054 | `			/* Perform the lookup */` |
|     25 | 5055 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5056 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5057 | `				pEntry = &aEntry[n];` |
|      - | 5058 | `				/* Do the comparison */` |
|     25 | 5059 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5060 | `				if( !rc ){` |
|     21 | 5061 | `					return SXRET_OK;` |
|      - | 5062 | `				}` |
|      3 | 5063 | `			}` |
|      2 | 5064 | `		}` |
|      2 | 5065 | `	}` |
|      - | 5066 | `	/* No such tag */` |
|     17 | 5067 | `	return SXERR_NOTFOUND;` |
|     19 | 5068 |  |
|      - | 5069 | `/*` |
|      - | 5070 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5071 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5072 | ` * Refer to [strip_tags()].` |
|      - | 5073 | ` */` |
|     16 | 5074 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5075 |  |
|     17 | 5076 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5077 | `	const char *zPtr,*zTag;` |
|      - | 5078 | `	SySet sSet;` |
|      - | 5079 | `	/* initialize the set of allowed tags */` |
|     17 | 5080 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5081 | `	if( nTaglen > 0 ){` |
|      - | 5082 | `		/* Set of allowed tags */` |
|     11 | 5083 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5084 | `	}` |
|      - | 5085 | `	/* Set the empty string */` |
|     17 | 5086 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5087 | `	/* Start processing */` |
|     26 | 5088 | `	for(;;){` |
|     53 | 5089 | `		if(zIn >= zEnd){` |
|      - | 5090 | `			/* No more input to process */` |
|     15 | 5091 | `			break;` |
|      - | 5092 | `		}` |
|     39 | 5093 | `		zPtr = zIn;` |
|      - | 5094 | `		/* Find a tag */` |
|    133 | 5095 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5096 | `			zIn++;` |
|      1 | 5097 | `		}` |
|     39 | 5098 | `		if( zIn > zPtr ){` |
|      - | 5099 | `			/* Consume raw input */` |
|     21 | 5100 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5101 | `		}` |
|      - | 5102 | `		/* Ignore trailing null bytes */` |
|     39 | 5103 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5104 | `			zIn++;` |
|    ! 0 | 5105 | `		}` |
|     39 | 5106 | `		if(zIn >= zEnd){` |
|      - | 5107 | `			/* No more input to process */` |
|      3 | 5108 | `			break;` |
|      - | 5109 | `		}` |
|     37 | 5110 | `		if( zIn[0] == '<' ){` |
|      - | 5111 | `			sxi32 rc;` |
|     37 | 5112 | `			zTag = zIn++;` |
|      - | 5113 | `			/* Delimit the tag */` |
|    127 | 5114 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5115 | `				zIn++;` |
|      1 | 5116 | `			}` |
|     37 | 5117 | `			if( zIn < zEnd ){` |
|     37 | 5118 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5119 | `			}` |
|      - | 5120 | `			/* Query the set */` |
|     37 | 5121 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5122 | `			if( rc == SXRET_OK ){` |
|      - | 5123 | `				/* Keep the tag */` |
|     21 | 5124 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5125 | `			}` |
|     18 | 5126 | `		}` |
|      1 | 5127 | `	}` |
|      - | 5128 | `	/* Cleanup */` |
|     17 | 5129 | `	SySetRelease(&sSet);` |
|     17 | 5130 | `	return SXRET_OK;` |
|      1 | 5131 |  |
|      - | 5132 | `/*` |
|      - | 5133 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5134 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5135 | ` * Parameters` |
|      - | 5136 | ` *  $str` |
|      - | 5137 | ` *  The input string.` |
|      - | 5138 | ` * $allowable_tags` |
|      - | 5139 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5140 | ` * Return` |
|      - | 5141 | ` *  Returns the stripped string.` |
|      - | 5142 | ` */` |
|     16 | 5143 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5144 |  |
|     17 | 5145 | `	const char *zTaglist = 0;` |
|      - | 5146 | `	const char *zString;` |
|     17 | 5147 | `	int nTaglen = 0;` |
|      - | 5148 | `	int nLen;` |
|     17 | 5149 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5150 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5151 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5152 | `		return PH7_OK;` |
|      - | 5153 | `	}` |
|      - | 5154 | `	/* Point to the raw string */` |
|     15 | 5155 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5156 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5157 | `		/* Allowed tag */` |
|     11 | 5158 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5159 | `	}` |
|      - | 5160 | `	/* Process input */` |
|     15 | 5161 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5162 | `	return PH7_OK;` |
|      9 | 5163 |  |
|      - | 5164 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5165 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5166 | `/*` |
|      - | 5167 | ` * string str_shuffle(string $str)` |
|      - | 5168 |  |
|      - | 5169 | ` *  Randomly shuffles a string.` |
|      - | 5170 | ` * Parameters` |
|      - | 5171 | ` *  $str` |
|      - | 5172 | ` *   The input string.` |
|      - | 5173 | ` * Return` |
|      - | 5174 | ` *  Returns the shuffled string.` |
|      - | 5175 | ` */` |
|     12 | 5176 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5177 |  |
|      - | 5178 | `	const char *zString;` |
|      - | 5179 | `	int nLen,i,c;` |
|      - | 5180 | `	sxu32 iR;` |
|     13 | 5181 | `	if( nArg < 1 ){` |
|      - | 5182 | `		/* Missing arguments,return the empty string */` |
|      3 | 5183 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5184 | `		return PH7_OK;` |
|      - | 5185 | `	}` |
|      - | 5186 | `	/* Extract the target string */` |
|     11 | 5187 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5188 | `	if( nLen < 1 ){` |
|      - | 5189 | `		/* Nothing to shuffle */` |
|      3 | 5190 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5191 | `		return PH7_OK;` |
|      - | 5192 | `	}` |
|      - | 5193 | `	/* Shuffle the string */` |
|     43 | 5194 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5195 | `		/* Generate a random number first */` |
|     35 | 5196 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5197 | `		/* Extract a random offset */` |
|     35 | 5198 | `		c = zString[iR % nLen];` |
|      - | 5199 | `		/* Append it */` |
|     35 | 5200 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5201 | `	}` |
|      9 | 5202 | `	return PH7_OK;` |
|      7 | 5203 |  |
|      - | 5204 | `/*` |
|      - | 5205 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5206 | ` *  Convert a string to an array.` |
|      - | 5207 | ` * Parameters` |
|      - | 5208 | ` * $str` |
|      - | 5209 | ` *  The input string.` |
|      - | 5210 | ` * $split_length` |
|      - | 5211 | ` *  Maximum length of the chunk.` |
|      - | 5212 | ` * Return` |
|      - | 5213 | ` *  If the optional split_length parameter is specified, the returned array` |
|      - | 5214 | ` *  will be broken down into chunks with each being split_length in length, otherwise` |
|      - | 5215 | ` *  each chunk will be one character in length. FALSE is returned if split_length is less than 1.` |
|      - | 5216 | ` *  If the split_length length exceeds the length of string, the entire string is returned` |
|      - | 5217 | ` *  as the first (and only) array element.` |
|      - | 5218 | ` */` |
|      8 | 5219 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5220 |  |
|      - | 5221 | `	const char *zString,*zEnd;` |
|      - | 5222 | `	ph7_value *pArray,*pValue;` |
|      - | 5223 | `	int split_len;` |
|      - | 5224 | `	int nLen;` |
|      9 | 5225 | `	if( nArg < 1 ){` |
|      - | 5226 | `		/* Missing arguments,return FALSE */` |
|      5 | 5227 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5228 | `		return PH7_OK;` |
|      - | 5229 | `	}` |
|      - | 5230 | `	/* Point to the target string */` |
|      5 | 5231 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 5232 | `	if( nLen < 1 ){` |
|      - | 5233 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5234 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5235 | `		return PH7_OK;` |
|      - | 5236 | `	}` |
|      5 | 5237 | `	split_len = (int)sizeof(char);` |
|      5 | 5238 | `	if( nArg > 1 ){` |
|      - | 5239 | `		/* Split length */` |
|      5 | 5240 | `		split_len = ph7_value_to_int(apArg[1]);` |
|      5 | 5241 | `		if( split_len < 1 ){` |
|      - | 5242 | `			/* Invalid length,return FALSE */` |
|      3 | 5243 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5244 | `			return PH7_OK;` |
|      - | 5245 | `		}` |
|      3 | 5246 | `		if( split_len > nLen ){` |
|    ! 0 | 5247 | `			split_len = nLen;` |
|    ! 0 | 5248 | `		}` |
|      1 | 5249 | `	}` |
|      - | 5250 | `	/* Create the array and the scalar value */` |
|      3 | 5251 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5252 | `	/*Chunk value */` |
|      3 | 5253 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5254 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5255 | `		/* Return FALSE */` |
|    ! 0 | 5256 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5257 | `		return PH7_OK;` |
|      - | 5258 | `	}` |
|      - | 5259 | `	/* Point to the end of the string */` |
|      3 | 5260 | `	zEnd = &zString[nLen];` |
|      - | 5261 | `	/* Perform the requested operation */` |
|      7 | 5262 | `	for(;;){` |
|      - | 5263 | `		int nMax;` |
|      9 | 5264 | `		if( zString >= zEnd ){` |
|      - | 5265 | `			/* No more input to process */` |
|      3 | 5266 | `			break;` |
|      - | 5267 | `		}` |
|      7 | 5268 | `		nMax = (int)(zEnd-zString);` |
|      7 | 5269 | `		if( nMax < split_len ){` |
|    ! 0 | 5270 | `			split_len = nMax;` |
|    ! 0 | 5271 | `		}` |
|      - | 5272 | `		/* Copy the current chunk */` |
|      7 | 5273 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5274 | `		/* Insert it */` |
|      7 | 5275 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5276 | `		/* reset the string cursor */` |
|      7 | 5277 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5278 | `		/* Update position */` |
|      7 | 5279 | `		zString += split_len;` |
|      1 | 5280 | `	}` |
|      - | 5281 | `	/*` |
|      - | 5282 | `	 * Return the array.` |
|      - | 5283 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5284 | `	 * upon we return from this function.` |
|      - | 5285 | `	 */` |
|      3 | 5286 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5287 | `	return PH7_OK;` |
|      5 | 5288 |  |
|      - | 5289 | `/*` |
|      - | 5290 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5291 | ` * Refer to [strspn()].` |
|      - | 5292 | ` */` |
|     28 | 5293 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5294 |  |
|     29 | 5295 | `	const char *zIn = *pzIn;` |
|      - | 5296 | `	const char *zPtr;` |
|      - | 5297 | `	/* Ignore leading white spaces */` |
|     29 | 5298 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5299 | `		zIn++;` |
|    ! 0 | 5300 | `	}` |
|     29 | 5301 | `	if( zIn >= zEnd ){` |
|      - | 5302 | `		/* End of input */` |
|    ! 0 | 5303 | `		return SXERR_EOF;` |
|      - | 5304 | `	}` |
|     29 | 5305 | `	zPtr = zIn;` |
|      - | 5306 | `	/* Extract the token */` |
|    201 | 5307 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5308 | `		zIn++;` |
|      1 | 5309 | `	}` |
|     29 | 5310 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5311 | `	/* Synchronize pointers */` |
|     29 | 5312 | `	*pzIn = zIn;` |
|      - | 5313 | `	/* Return to the caller */` |
|     29 | 5314 | `	return SXRET_OK;` |
|     15 | 5315 |  |
|      - | 5316 | `/*` |
|      - | 5317 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5318 | ` * return the longest match.` |
|      - | 5319 | ` * Refer to [strspn()].` |
|      - | 5320 | ` */` |
|     18 | 5321 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5322 |  |
|     19 | 5323 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5324 | `	const char *zIn = zString;` |
|      - | 5325 | `	int i,c;` |
|     45 | 5326 | `	for(;;){` |
|     91 | 5327 | `		if( zString >= zEnd ){` |
|      7 | 5328 | `			break;` |
|      - | 5329 | `		}` |
|      - | 5330 | `		/* Extract current character */` |
|     85 | 5331 | `		c = zString[0];` |
|      - | 5332 | `		/* Perform the lookup */` |
|    383 | 5333 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5334 | `			if( c == zMask[i] ){` |
|      - | 5335 | `				/* Character found */` |
|     73 | 5336 | `				break;` |
|      - | 5337 | `			}` |
|    150 | 5338 | `		}` |
|     85 | 5339 | `		if( i >= nMaskLen ){` |
|      - | 5340 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5341 | `			break;` |
|      - | 5342 | `		}` |
|      - | 5343 | `		/* Advance cursor */` |
|     73 | 5344 | `		zString++;` |
|      1 | 5345 | `	}` |
|      - | 5346 | `	/* Longest match */` |
|     19 | 5347 | `	return (int)(zString-zIn);` |
|      1 | 5348 |  |
|      - | 5349 | `/*` |
|      - | 5350 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5351 | ` * Refer to [strcspn()].` |
|      - | 5352 | ` */` |
|     10 | 5353 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5354 |  |
|     11 | 5355 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5356 | `	const char *zIn = zString;` |
|      - | 5357 | `	int i,c;` |
|     12 | 5358 | `	for(;;){` |
|     25 | 5359 | `		if( zString >= zEnd ){` |
|      3 | 5360 | `			break;` |
|      - | 5361 | `		}` |
|      - | 5362 | `		/* Extract current character */` |
|     23 | 5363 | `		c = zString[0];` |
|      - | 5364 | `		/* Perform the lookup */` |
|     51 | 5365 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5366 | `			if( c == zMask[i] ){` |
|      9 | 5367 | `				break;` |
|      - | 5368 | `			}` |
|     15 | 5369 | `		}` |
|     23 | 5370 | `		if( i < nMaskLen ){` |
|      - | 5371 | `			/* Character in the current mask,break immediately */` |
|      9 | 5372 | `			break;` |
|      - | 5373 | `		}` |
|      - | 5374 | `		/* Advance cursor */` |
|     15 | 5375 | `		zString++;` |
|      1 | 5376 | `	}` |
|      - | 5377 | `	/* Longest match */` |
|     11 | 5378 | `	return (int)(zString-zIn);` |
|      1 | 5379 |  |
|      - | 5380 | `/*` |
|      - | 5381 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5382 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5383 | ` *  of characters contained within a given mask.` |
|      - | 5384 | ` * Parameters` |
|      - | 5385 | ` * $str` |
|      - | 5386 | ` *  The input string.` |
|      - | 5387 | ` * $mask` |
|      - | 5388 | ` *  The list of allowable characters.` |
|      - | 5389 | ` * $start` |
|      - | 5390 | ` *  The position in subject to start searching.` |
|      - | 5391 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5392 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5393 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5394 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5395 | ` *  start'th position from the end of subject.` |
|      - | 5396 | ` * $length` |
|      - | 5397 | ` *  The length of the segment from subject to examine.` |
|      - | 5398 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5399 | ` *  characters after the starting position.` |
|      - | 5400 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5401 | ` *  position up to length characters from the end of subject.` |
|      - | 5402 | ` * Return` |
|      - | 5403 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5404 | ` * in mask.` |
|      - | 5405 | ` */` |
|     26 | 5406 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5407 |  |
|      - | 5408 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5409 | `	int iMasklen,iLen;` |
|      - | 5410 | `	SyString sToken;` |
|     27 | 5411 | `	int iCount = 0;` |
|      - | 5412 | `	int rc;` |
|     27 | 5413 | `	if( nArg < 2 ){` |
|      - | 5414 | `		/* Missing agruments,return zero */` |
|      3 | 5415 | `		ph7_result_int(pCtx,0);` |
|      3 | 5416 | `		return PH7_OK;` |
|      - | 5417 | `	}` |
|      - | 5418 | `	/* Extract the target string */` |
|     25 | 5419 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5420 | `	/* Extract the mask */` |
|     25 | 5421 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5422 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5423 | `		/* Nothing to process,return zero */` |
|      7 | 5424 | `		ph7_result_int(pCtx,0);` |
|      7 | 5425 | `		return PH7_OK;` |
|      - | 5426 | `	}` |
|     19 | 5427 | `	if( nArg > 2 ){` |
|      - | 5428 | `		int nOfft;` |
|      - | 5429 | `		/* Extract the offset */` |
|      9 | 5430 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5431 | `		if( nOfft < 0 ){` |
|    ! 0 | 5432 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5433 | `			if( zBase > zString ){` |
|    ! 0 | 5434 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5435 | `				zString = zBase;` |
|    ! 0 | 5436 | `			}else{` |
|      - | 5437 | `				/* Invalid offset */` |
|    ! 0 | 5438 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5439 | `				return PH7_OK;` |
|      - | 5440 | `			}` |
|    ! 0 | 5441 | `		}else{` |
|      9 | 5442 | `			if( nOfft >= iLen ){` |
|      - | 5443 | `				/* Invalid offset */` |
|    ! 0 | 5444 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5445 | `				return PH7_OK;` |
|    ! 0 | 5446 | `			}else{` |
|      - | 5447 | `				/* Update offset */` |
|      9 | 5448 | `				zString += nOfft;` |
|      9 | 5449 | `				iLen -= nOfft;` |
|      - | 5450 | `			}` |
|      - | 5451 | `		}` |
|      9 | 5452 | `		if( nArg > 3 ){` |
|      - | 5453 | `			int iUserlen;` |
|      - | 5454 | `			/* Extract the desired length */` |
|      9 | 5455 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5456 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5457 | `				iLen = iUserlen;` |
|      2 | 5458 | `			}` |
|      4 | 5459 | `		}` |
|      4 | 5460 | `	}` |
|      - | 5461 | `	/* Point to the end of the string */` |
|     19 | 5462 | `	zEnd = &zString[iLen];` |
|      - | 5463 | `	/* Extract the first non-space token */` |
|     19 | 5464 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5465 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5466 | `		/* Compare against the current mask */` |
|     19 | 5467 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5468 | `	}` |
|      - | 5469 | `	/* Longest match */` |
|     19 | 5470 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5471 | `	return PH7_OK;` |
|     14 | 5472 |  |
|      - | 5473 | `/*` |
|      - | 5474 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5475 | ` *  Find length of initial segment not matching mask.` |
|      - | 5476 | ` * Parameters` |
|      - | 5477 | ` * $str` |
|      - | 5478 | ` *  The input string.` |
|      - | 5479 | ` * $mask` |
|      - | 5480 | ` *  The list of not allowed characters.` |
|      - | 5481 | ` * $start` |
|      - | 5482 | ` *  The position in subject to start searching.` |
|      - | 5483 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5484 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5485 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5486 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5487 | ` *  start'th position from the end of subject.` |
|      - | 5488 | ` * $length` |
|      - | 5489 | ` *  The length of the segment from subject to examine.` |
|      - | 5490 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5491 | ` *  characters after the starting position.` |
|      - | 5492 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5493 | ` *  position up to length characters from the end of subject.` |
|      - | 5494 | ` * Return` |
|      - | 5495 | ` *  Returns the length of the segment as an integer.` |
|      - | 5496 | ` */` |
|     16 | 5497 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5498 |  |
|      - | 5499 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5500 | `	int iMasklen,iLen;` |
|      - | 5501 | `	SyString sToken;` |
|     17 | 5502 | `	int iCount = 0;` |
|      - | 5503 | `	int rc;` |
|     17 | 5504 | `	if( nArg < 2 ){` |
|      - | 5505 | `		/* Missing agruments,return zero */` |
|      3 | 5506 | `		ph7_result_int(pCtx,0);` |
|      3 | 5507 | `		return PH7_OK;` |
|      - | 5508 | `	}` |
|      - | 5509 | `	/* Extract the target string */` |
|     15 | 5510 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5511 | `	/* Extract the mask */` |
|     15 | 5512 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5513 | `	if( iLen < 1 ){` |
|      - | 5514 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5515 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5516 | `		return PH7_OK;` |
|      - | 5517 | `	}` |
|     15 | 5518 | `	if( iMasklen < 1 ){` |
|      - | 5519 | `		/* No given mask,return the string length */` |
|      3 | 5520 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5521 | `		return PH7_OK;` |
|      - | 5522 | `	}` |
|     13 | 5523 | `	if( nArg > 2 ){` |
|      - | 5524 | `		int nOfft;` |
|      - | 5525 | `		/* Extract the offset */` |
|     11 | 5526 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5527 | `		if( nOfft < 0 ){` |
|    ! 0 | 5528 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5529 | `			if( zBase > zString ){` |
|    ! 0 | 5530 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5531 | `				zString = zBase;` |
|    ! 0 | 5532 | `			}else{` |
|      - | 5533 | `				/* Invalid offset */` |
|    ! 0 | 5534 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5535 | `				return PH7_OK;` |
|      - | 5536 | `			}` |
|    ! 0 | 5537 | `		}else{` |
|     11 | 5538 | `			if( nOfft >= iLen ){` |
|      - | 5539 | `				/* Invalid offset */` |
|      3 | 5540 | `				ph7_result_int(pCtx,0);` |
|      3 | 5541 | `				return PH7_OK;` |
|    ! 0 | 5542 | `			}else{` |
|      - | 5543 | `				/* Update offset */` |
|      9 | 5544 | `				zString += nOfft;` |
|      9 | 5545 | `				iLen -= nOfft;` |
|      - | 5546 | `			}` |
|      - | 5547 | `		}` |
|      9 | 5548 | `		if( nArg > 3 ){` |
|      - | 5549 | `			int iUserlen;` |
|      - | 5550 | `			/* Extract the desired length */` |
|    ! 0 | 5551 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5552 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5553 | `				iLen = iUserlen;` |
|    ! 0 | 5554 | `			}` |
|    ! 0 | 5555 | `		}` |
|      4 | 5556 | `	}` |
|      - | 5557 | `	/* Point to the end of the string */` |
|     11 | 5558 | `	zEnd = &zString[iLen];` |
|      - | 5559 | `	/* Extract the first non-space token */` |
|     11 | 5560 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5561 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5562 | `		/* Compare against the current mask */` |
|     11 | 5563 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5564 | `	}` |
|      - | 5565 | `	/* Longest match */` |
|     11 | 5566 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5567 | `	return PH7_OK;` |
|      9 | 5568 |  |
|      - | 5569 | `/*` |
|      - | 5570 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5571 | ` *  Search a string for any of a set of characters.` |
|      - | 5572 | ` * Parameters` |
|      - | 5573 | ` *  $haystack` |
|      - | 5574 | ` *   The string where char_list is looked for.` |
|      - | 5575 | ` *  $char_list` |
|      - | 5576 | ` *   This parameter is case sensitive.` |
|      - | 5577 | ` * Return` |
|      - | 5578 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5579 | ` */` |
|      6 | 5580 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5581 |  |
|      - | 5582 | `	const char *zString,*zList,*zEnd;` |
|      - | 5583 | `	int iLen,iListLen,i,c;` |
|      - | 5584 | `	sxu32 nOfft,nMax;` |
|      - | 5585 | `	sxi32 rc;` |
|      7 | 5586 | `	if( nArg < 2 ){` |
|      - | 5587 | `		/* Missing arguments,return FALSE */` |
|      3 | 5588 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5589 | `		return PH7_OK;` |
|      - | 5590 | `	}` |
|      - | 5591 | `	/* Extract the haystack and the char list */` |
|      5 | 5592 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5593 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5594 | `	if( iLen < 1 ){` |
|      - | 5595 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5596 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5597 | `		return PH7_OK;` |
|      - | 5598 | `	}` |
|      - | 5599 | `	/* Point to the end of the string */` |
|      5 | 5600 | `	zEnd = &zString[iLen];` |
|      5 | 5601 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5602 | `	/* perform the requested operation */` |
|     15 | 5603 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5604 | `		c = zList[i];` |
|     11 | 5605 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5606 | `		if( rc == SXRET_OK ){` |
|      5 | 5607 | `			if( nMax < nOfft ){` |
|      3 | 5608 | `				nOfft = nMax;` |
|      1 | 5609 | `			}` |
|      2 | 5610 | `		}` |
|      6 | 5611 | `	}` |
|      5 | 5612 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5613 | `		/* No such substring,return FALSE */` |
|      3 | 5614 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5615 | `	}else{` |
|      - | 5616 | `		/* Return the substring */` |
|      3 | 5617 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5618 | `	}` |
|      5 | 5619 | `	return PH7_OK;` |
|      4 | 5620 |  |
|      - | 5621 | `/*` |
|      - | 5622 | ` * string soundex(string $str)` |
|      - | 5623 | ` *  Calculate the soundex key of a string.` |
|      - | 5624 | ` * Parameters` |
|      - | 5625 | ` *  $str` |
|      - | 5626 | ` *   The input string.` |
|      - | 5627 | ` * Return` |
|      - | 5628 | ` *  Returns the soundex key as a string.` |
|      - | 5629 | ` * Note:` |
|      - | 5630 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5631 | ` * source tree.` |
|      - | 5632 | ` */` |
|     20 | 5633 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5634 |  |
|      - | 5635 | `	const unsigned char *zIn;` |
|      - | 5636 | `	char zResult[8];` |
|      - | 5637 | `	int i, j;` |
|      - | 5638 | `	static const unsigned char iCode[] = {` |
|      - | 5639 |  |
|      - | 5640 |  |
|      - | 5641 |  |
|      - | 5642 |  |
|      - | 5643 |  |
|      - | 5644 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5645 |  |
|      - | 5646 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5647 | `	};` |
|     21 | 5648 | `	if( nArg < 1 ){` |
|      - | 5649 | `		/* Missing arguments,return the empty string */` |
|      3 | 5650 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5651 | `		return PH7_OK;` |
|      - | 5652 | `	}` |
|     19 | 5653 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5654 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5655 | `	if( zIn[i] ){` |
|     17 | 5656 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5657 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5658 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5659 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5660 | `			if( code>0 ){` |
|     45 | 5661 | `				if( code!=prevcode ){` |
|     33 | 5662 | `					prevcode = (unsigned char)code;` |
|     33 | 5663 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5664 | `				}` |
|     23 | 5665 | `			}else{` |
|     49 | 5666 | `				prevcode = 0;` |
|      - | 5667 | `			}` |
|     47 | 5668 | `		}` |
|     33 | 5669 | `		while( j<4 ){` |
|     17 | 5670 | `			zResult[j++] = '0';` |
|      1 | 5671 | `		}` |
|     17 | 5672 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5673 | `	}else{` |
|      3 | 5674 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5675 | `	}` |
|     19 | 5676 | `	return PH7_OK;` |
|     11 | 5677 |  |
|      - | 5678 | `/*` |
|      - | 5679 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5680 | ` *  Wraps a string to a given number of characters.` |
|      - | 5681 | ` * Parameters` |
|      - | 5682 | ` *  $str` |
|      - | 5683 | ` *   The input string.` |
|      - | 5684 | ` * $width` |
|      - | 5685 | ` *  The column width.` |
|      - | 5686 | ` * $break` |
|      - | 5687 | ` *  The line is broken using the optional break parameter.` |
|      - | 5688 | ` * Return` |
|      - | 5689 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5690 | ` */` |
|     14 | 5691 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5692 |  |
|      - | 5693 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5694 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5695 | `	if( nArg < 1 ){` |
|      - | 5696 | `		/* Missing arguments,return the empty string */` |
|      3 | 5697 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5698 | `		return PH7_OK;` |
|      - | 5699 | `	}` |
|      - | 5700 | `	/* Extract the input string */` |
|     13 | 5701 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5702 | `	if( iLen < 1 ){` |
|      - | 5703 | `		/* Nothing to process,return the empty string */` |
|      3 | 5704 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5705 | `		return PH7_OK;` |
|      - | 5706 | `	}` |
|      - | 5707 | `	/* Chunk length */` |
|     11 | 5708 | `	iChunk = 75;` |
|     11 | 5709 | `	iBreaklen = 0;` |
|     11 | 5710 | `	zBreak = ""; /* cc warning */` |
|     11 | 5711 | `	if( nArg > 1 ){` |
|     11 | 5712 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5713 | `		if( iChunk < 1 ){` |
|    ! 0 | 5714 | `			iChunk = 75;` |
|    ! 0 | 5715 | `		}` |
|     11 | 5716 | `		if( nArg > 2 ){` |
|      3 | 5717 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5718 | `		}` |
|      5 | 5719 | `	}` |
|     11 | 5720 | `	if( iBreaklen < 1 ){` |
|      - | 5721 | `		/* Set a default column break */` |
|      - | 5722 | `#ifdef __WINNT__` |
|      1 | 5723 | `		zBreak = "\r\n";` |
|      1 | 5724 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5725 | `#else` |
|      8 | 5726 | `		zBreak = "\n";` |
|      8 | 5727 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5728 | `#endif` |
|      4 | 5729 | `	}` |
|      - | 5730 | `	/* Perform the requested operation */` |
|     11 | 5731 | `	zEnd = &zIn[iLen];` |
|     41 | 5732 | `	for(;;){` |
|      - | 5733 | `		int nMax;` |
|     47 | 5734 | `		if( zIn >= zEnd ){` |
|      - | 5735 | `			/* No more input to process */` |
|     11 | 5736 | `			break;` |
|      - | 5737 | `		}` |
|     37 | 5738 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5739 | `		if( iChunk > nMax ){` |
|     11 | 5740 | `			iChunk = nMax;` |
|      5 | 5741 | `		}` |
|      - | 5742 | `		/* Append the column first */` |
|     37 | 5743 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5744 | `		/* Advance the cursor */` |
|     37 | 5745 | `		zIn += iChunk;` |
|     37 | 5746 | `		if( zIn < zEnd ){` |
|      - | 5747 | `			/* Append the line break */` |
|     27 | 5748 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5749 | `		}` |
|      1 | 5750 | `	}` |
|     11 | 5751 | `	return PH7_OK;` |
|      8 | 5752 |  |
|      - | 5753 | `/*` |
|      - | 5754 | ` * Check if the given character is a member of the given mask.` |
|      - | 5755 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5756 | ` * Refer to [strtok()].` |
|      - | 5757 | ` */` |
|     30 | 5758 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5759 |  |
|      - | 5760 | `	int i;` |
|     57 | 5761 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5762 | `		if( c == zMask[i] ){` |
|     13 | 5763 | `			if( pOfft ){` |
|      5 | 5764 | `				*pOfft = i;` |
|      2 | 5765 | `			}` |
|     13 | 5766 | `			return TRUE;` |
|      - | 5767 | `		}` |
|     14 | 5768 | `	}` |
|     19 | 5769 | `	return FALSE;` |
|     16 | 5770 |  |
|      - | 5771 | `/*` |
|      - | 5772 | ` * Extract a single token from the input stream.` |
|      - | 5773 | ` * Refer to [strtok()].` |
|      - | 5774 | ` */` |
|      6 | 5775 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5776 |  |
|      7 | 5777 | `	const char *zIn = *pzIn;` |
|      - | 5778 | `	const char *zPtr;` |
|      - | 5779 | `	/* Ignore leading delimiter */` |
|     11 | 5780 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5781 | `		zIn++;` |
|      1 | 5782 | `	}` |
|      7 | 5783 | `	if( zIn >= zEnd ){` |
|      - | 5784 | `		/* End of input */` |
|    ! 0 | 5785 | `		return SXERR_EOF;` |
|      - | 5786 | `	}` |
|      7 | 5787 | `	zPtr = zIn;` |
|      - | 5788 | `	/* Extract the token */` |
|     13 | 5789 | `	while( zIn < zEnd ){` |
|     11 | 5790 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5791 | `			/* UTF-8 stream */` |
|    ! 0 | 5792 | `			zIn++;` |
|    ! 0 | 5793 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5794 | `		}else{` |
|     11 | 5795 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5796 | `				break;` |
|      - | 5797 | `			}` |
|      7 | 5798 | `			zIn++;` |
|      - | 5799 | `		}` |
|      1 | 5800 | `	}` |
|      7 | 5801 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5802 | `	/* Update the cursor */` |
|      7 | 5803 | `	*pzIn = zIn;` |
|      - | 5804 | `	/* Return to the caller */` |
|      7 | 5805 | `	return SXRET_OK;` |
|      4 | 5806 |  |
|      - | 5807 | `/* strtok auxiliary private data */` |
|      - | 5808 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5809 | `struct strtok_aux_data` |
|      - | 5810 |  |
|      - | 5811 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5812 | `	const char *zIn;   /* Current input stream */` |
|      - | 5813 | `	const char *zEnd;  /* End of input */` |
|      - | 5814 | `};` |
|      - | 5815 | `/*` |
|      - | 5816 | ` * string strtok(string $str,string $token)` |
|      - | 5817 | ` * string strtok(string $token)` |
|      - | 5818 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5819 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5820 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5821 | ` *  words by using the space character as the token.` |
|      - | 5822 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5823 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5824 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5825 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5826 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5827 | ` *  the argument are found.` |
|      - | 5828 | ` * Parameters` |
|      - | 5829 | ` *  $str` |
|      - | 5830 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5831 | ` * $token` |
|      - | 5832 | ` *  The delimiter used when splitting up str.` |
|      - | 5833 | ` * Return` |
|      - | 5834 | ` *   Current token or FALSE on EOF.` |
|      - | 5835 | ` */` |
|      8 | 5836 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5837 |  |
|      - | 5838 | `	strtok_aux_data *pAux;` |
|      - | 5839 | `	const char *zMask;` |
|      - | 5840 | `	SyString sToken;` |
|      - | 5841 | `	int nMasklen;` |
|      - | 5842 | `	sxi32 rc;` |
|      9 | 5843 | `	if( nArg < 2 ){` |
|      - | 5844 | `		/* Extract top aux data */` |
|      7 | 5845 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5846 | `		if( pAux == 0 ){` |
|      - | 5847 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5848 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5849 | `			return PH7_OK;` |
|      - | 5850 | `		}` |
|      7 | 5851 | `		nMasklen = 0;` |
|      7 | 5852 | `		zMask = ""; /* cc warning */` |
|      7 | 5853 | `		if( nArg > 0 ){` |
|      - | 5854 | `			/* Extract the mask */` |
|      5 | 5855 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5856 | `		}` |
|      7 | 5857 | `		if( nMasklen < 1 ){` |
|      - | 5858 | `			/* Invalid mask,return FALSE */` |
|      3 | 5859 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5860 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5861 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5862 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5863 | `			return PH7_OK;` |
|      - | 5864 | `		}` |
|      - | 5865 | `		/* Extract the token */` |
|      5 | 5866 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5867 | `		if( rc != SXRET_OK ){` |
|      - | 5868 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5869 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5870 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5871 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5872 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5873 | `		}else{` |
|      - | 5874 | `			/* Return the extracted token */` |
|      5 | 5875 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5876 | `		}` |
|      3 | 5877 | `	}else{` |
|      - | 5878 | `		const char *zInput,*zCur;` |
|      - | 5879 | `		char *zDup;` |
|      - | 5880 | `		int nLen;` |
|      - | 5881 | `		/* Extract the raw input */` |
|      3 | 5882 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5883 | `		if( nLen < 1 ){` |
|      - | 5884 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5885 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5886 | `			return PH7_OK;` |
|      - | 5887 | `		}` |
|      - | 5888 | `		/* Extract the mask */` |
|      3 | 5889 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5890 | `		if( nMasklen < 1 ){` |
|      - | 5891 | `			/* Set a default mask */` |
|      - | 5892 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5893 | `			zMask = TOK_MASK;` |
|    ! 0 | 5894 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5895 | `#undef TOK_MASK` |
|    ! 0 | 5896 | `		}` |
|      - | 5897 | `		/* Extract a single token */` |
|      3 | 5898 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5899 | `		if( rc != SXRET_OK ){` |
|      - | 5900 | `			/* Empty input */` |
|    ! 0 | 5901 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5902 | `			return PH7_OK;` |
|    ! 0 | 5903 | `		}else{` |
|      - | 5904 | `			/* Return the extracted token */` |
|      3 | 5905 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5906 | `		}` |
|      - | 5907 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5908 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5909 | `		if( pAux ){` |
|      3 | 5910 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5911 | `			if( nLen < 1 ){` |
|    ! 0 | 5912 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5913 | `				return PH7_OK;` |
|      - | 5914 | `			}` |
|      - | 5915 | `			/* Duplicate input */` |
|      3 | 5916 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5917 | `			if( zDup  ){` |
|      3 | 5918 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5919 | `				/* Register the aux data */` |
|      3 | 5920 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5921 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5922 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5923 | `			}` |
|      1 | 5924 | `		}` |
|      - | 5925 | `	}` |
|      7 | 5926 | `	return PH7_OK;` |
|      5 | 5927 |  |
|      - | 5928 | `/*` |
|      - | 5929 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5930 | ` *  Pad a string to a certain length with another string` |
|      - | 5931 | ` * Parameters` |
|      - | 5932 | ` *  $input` |
|      - | 5933 | ` *   The input string.` |
|      - | 5934 | ` * $pad_length` |
|      - | 5935 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5936 | ` *   string, no padding takes place.` |
|      - | 5937 | ` * $pad_string` |
|      - | 5938 | ` *   Note:` |
|      - | 5939 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5940 | ` *    divided by the pad_string's length.` |
|      - | 5941 | ` * $pad_type` |
|      - | 5942 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5943 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5944 | ` * Return` |
|      - | 5945 | ` *  The padded string.` |
|      - | 5946 | ` */` |
|     10 | 5947 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5948 |  |
|      - | 5949 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5950 | `	const char *zIn,*zPad;` |
|     11 | 5951 | `	if( nArg < 2 ){` |
|      - | 5952 | `		/* Missing arguments,return the empty string */` |
|      5 | 5953 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5954 | `		return PH7_OK;` |
|      - | 5955 | `	}` |
|      - | 5956 | `	/* Extract the target string */` |
|      7 | 5957 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5958 | `	/* Padding length */` |
|      7 | 5959 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5960 | `	if( iPadlen > 0 ){` |
|      5 | 5961 | `		iPadlen -= iLen;` |
|      2 | 5962 | `	}` |
|      7 | 5963 | `	if( iPadlen < 1  ){` |
|      - | 5964 | `		/* Return the string verbatim */` |
|      3 | 5965 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5966 | `		return PH7_OK;` |
|      - | 5967 | `	}` |
|      5 | 5968 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5969 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5970 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5971 | `	if( nArg > 2 ){` |
|      - | 5972 | `		/* Padding string */` |
|      5 | 5973 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5974 | `		if( iStrpad < 1 ){` |
|      - | 5975 | `			/* Empty string */` |
|    ! 0 | 5976 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5977 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5978 | `		}` |
|      5 | 5979 | `		if( nArg > 3 ){` |
|      - | 5980 | `			/* Padd type */` |
|      5 | 5981 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5982 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5983 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5984 | `			}` |
|      2 | 5985 | `		}` |
|      2 | 5986 | `	}` |
|      5 | 5987 | `	iDiv = 1;` |
|      5 | 5988 | `	if( iType == 2 ){` |
|    ! 0 | 5989 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5990 | `	}` |
|      - | 5991 | `	/* Perform the requested operation */` |
|      5 | 5992 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5993 | `		jPad = iStrpad;` |
|      5 | 5994 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5995 | `			/* Padding */` |
|      5 | 5996 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5997 | `				break;` |
|      - | 5998 | `			}` |
|      3 | 5999 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 6000 | `		}` |
|      3 | 6001 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6002 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6003 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6004 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6005 | `					jPad = iStrpad;` |
|    ! 0 | 6006 | `				}` |
|      3 | 6007 | `				if( jPad < 1){` |
|    ! 0 | 6008 | `					break;` |
|      - | 6009 | `				}` |
|      3 | 6010 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6011 | `			}` |
|      1 | 6012 | `		}` |
|      1 | 6013 | `	}` |
|      5 | 6014 | `	if( iLen > 0 ){` |
|      - | 6015 | `		/* Append the input string */` |
|      5 | 6016 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 6017 | `	}` |
|      5 | 6018 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6019 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6020 | `			/* Padding */` |
|      5 | 6021 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6022 | `				break;` |
|      - | 6023 | `			}` |
|      3 | 6024 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 6025 | `		}` |
|      5 | 6026 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6027 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6028 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6029 | `				jPad = iStrpad;` |
|    ! 0 | 6030 | `			}` |
|      3 | 6031 | `			if( jPad < 1){` |
|    ! 0 | 6032 | `				break;` |
|      - | 6033 | `			}` |
|      3 | 6034 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6035 | `		}` |
|      1 | 6036 | `	}` |
|      5 | 6037 | `	return PH7_OK;` |
|      6 | 6038 |  |
|      - | 6039 | `/*` |
|      - | 6040 | ` * String replacement private data.` |
|      - | 6041 | ` */` |
|      - | 6042 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6043 | `struct str_replace_data` |
|      - | 6044 |  |
|      - | 6045 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6046 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6047 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6048 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6049 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6050 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6051 | `};` |
|      - | 6052 | `/*` |
|      - | 6053 | ` * Remove a substring.` |
|      - | 6054 | ` */` |
|      - | 6055 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6056 | `	for(;;){\` |
|      - | 6057 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6058 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6059 | `		++OFFT;\` |
|      - | 6060 | `	}\` |
|      - | 6061 |  |
|      - | 6062 | `/*` |
|      - | 6063 | ` * Shift right and insert algorithm.` |
|      - | 6064 | ` */` |
|      - | 6065 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6066 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6067 | `		for(;;){\` |
|      - | 6068 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6069 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6070 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6071 | `			--INLEN; \` |
|      - | 6072 | `		}\` |
|      - | 6073 | `		for(;;){\` |
|      - | 6074 | `				if(ELEN < 1) { break; }\` |
|      - | 6075 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6076 | `				OFFT++;\` |
|      - | 6077 | `				ENTRY++;\` |
|      - | 6078 | `				--ELEN;\` |
|      - | 6079 | `		}\` |
|      - | 6080 |  |
|      - | 6081 | `/*` |
|      - | 6082 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6083 | ` * replacement string [i.e: zReplace].` |
|      - | 6084 | ` */` |
|     38 | 6085 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6086 |  |
|     39 | 6087 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6088 | `	sxu32 n,m;` |
|     39 | 6089 | `	n = SyBlobLength(pWorker);` |
|     39 | 6090 | `	m = nOfft;` |
|      - | 6091 | `	/* Delete the old entry */` |
|    475 | 6092 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6093 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6094 | `	if( nReplen > 0 ){` |
|     33 | 6095 | `		sxi32 iRep = nReplen;` |
|      - | 6096 | `		sxi32 rc;` |
|      - | 6097 | `		/*` |
|      - | 6098 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6099 | `		 * string.` |
|      - | 6100 | `		 */` |
|     33 | 6101 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6102 | `		if( rc != SXRET_OK ){` |
|      - | 6103 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 6104 | `			return SXRET_OK;` |
|      - | 6105 | `		}` |
|      - | 6106 | `		/* Perform the insertion now */` |
|     33 | 6107 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6108 | `		n = SyBlobLength(pWorker);` |
|    163 | 6109 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6110 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6111 | `	}` |
|     39 | 6112 | `	return SXRET_OK;` |
|     20 | 6113 |  |
|      - | 6114 | `/*` |
|      - | 6115 | ` * String replacement walker callback.` |
|      - | 6116 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6117 | ` * the replace string.` |
|      - | 6118 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6119 | ` */` |
|      8 | 6120 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6121 |  |
|      9 | 6122 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6123 | `	const char *zTarget,*zReplace;` |
|      - | 6124 | `	SyBlob *pWorker;` |
|      - | 6125 | `	int tLen,nLen;` |
|      - | 6126 | `	sxu32 nOfft;` |
|      - | 6127 | `	sxi32 rc;` |
|      - | 6128 | `	/* Point to the working buffer */` |
|      9 | 6129 | `	pWorker = pRepData->pWorker;` |
|      9 | 6130 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6131 | `		/* Target and replace must be a string */` |
|      3 | 6132 | `		return PH7_OK;` |
|      - | 6133 | `	}` |
|      - | 6134 | `	/* Extract the target and the replace */` |
|      7 | 6135 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6136 | `	if( tLen < 1 ){` |
|      - | 6137 | `		/* Empty target,return immediately */` |
|    ! 0 | 6138 | `		return PH7_OK;` |
|      - | 6139 | `	}` |
|      - | 6140 | `	/* Perform a pattern search */` |
|      7 | 6141 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6142 | `	if( rc != SXRET_OK ){` |
|      - | 6143 | `		/* Pattern not found */` |
|    ! 0 | 6144 | `		return PH7_OK;` |
|      - | 6145 | `	}` |
|      - | 6146 | `	/* Extract the replace string */` |
|      7 | 6147 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6148 | `	/* Perform the replace process */` |
|      7 | 6149 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 6150 | `	/* All done */` |
|      7 | 6151 | `	return PH7_OK;` |
|      5 | 6152 |  |
|      - | 6153 | `/*` |
|      - | 6154 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6155 | ` * to collect search/replace string.` |
|      - | 6156 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6157 | ` */` |
|     26 | 6158 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6159 |  |
|     27 | 6160 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6161 | `	SyString sWorker;` |
|      - | 6162 | `	const char *zIn;` |
|      - | 6163 | `	int nByte;` |
|      - | 6164 | `	/* Extract a string representation of the given argument */` |
|     27 | 6165 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6166 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6167 | `	if( nByte > 0 ){` |
|      - | 6168 | `		char *zDup;` |
|      - | 6169 | `		/* Duplicate the chunk */` |
|     25 | 6170 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6171 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6172 | `			);` |
|     25 | 6173 | `		if( zDup == 0 ){` |
|      - | 6174 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 6175 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 6176 | `			return PH7_OK;` |
|      - | 6177 | `		}` |
|     25 | 6178 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6179 | `		/* Save the chunk */` |
|     25 | 6180 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6181 | `	}` |
|      - | 6182 | `	/* Save for later processing */` |
|     27 | 6183 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6184 | `	/* All done */` |
|     13 | 6185 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6186 | `	return PH7_OK;` |
|     14 | 6187 |  |
|      - | 6188 | `/*` |
|      - | 6189 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6190 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6191 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6192 | ` * Parameters` |
|      - | 6193 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6194 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6195 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6196 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6197 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6198 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6199 | ` * $search` |
|      - | 6200 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6201 | ` *  to designate multiple needles.` |
|      - | 6202 | ` * $replace` |
|      - | 6203 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6204 | ` *  to designate multiple replacements.` |
|      - | 6205 | ` * $subject` |
|      - | 6206 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6207 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6208 | ` *  of subject, and the return value is an array as well.` |
|      - | 6209 | ` * $count (Not used)` |
|      - | 6210 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6211 | ` * Return` |
|      - | 6212 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6213 | ` */` |
|  12258 | 6214 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6215 |  |
|      - | 6216 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6217 | `	ProcStringMatch xMatch;` |
|      - | 6218 | `	const char *zIn,*zFunc;` |
|      - | 6219 | `	str_replace_data sRep;` |
|      - | 6220 | `	SyBlob sWorker;` |
|      - | 6221 | `	SySet sReplace;` |
|      - | 6222 | `	SySet sSearch;` |
|      - | 6223 | `	int rep_str;` |
|      - | 6224 | `	int nByte;` |
|      - | 6225 | `	sxi32 rc;` |
|  12260 | 6226 | `	if( nArg < 3 ){` |
|      - | 6227 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6228 | `		ph7_result_null(pCtx);` |
|      7 | 6229 | `		return PH7_OK;` |
|      - | 6230 | `	}` |
|      - | 6231 | `	/* Initialize fields */` |
|  12254 | 6232 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12254 | 6233 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  12254 | 6234 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  12254 | 6235 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  12254 | 6236 | `	sRep.pCtx = pCtx;` |
|  12254 | 6237 | `	sRep.pCollector = &sSearch;` |
|  12254 | 6238 | `	rep_str = 0;` |
|      - | 6239 | `	/* Extract the subject */` |
|  12254 | 6240 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  12254 | 6241 | `	if( nByte < 1 ){` |
|      - | 6242 | `		/* Nothing to replace,return the empty string */` |
|     38 | 6243 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 6244 | `		return PH7_OK;` |
|      - | 6245 | `	}` |
|      - | 6246 | `	/* Copy the subject */` |
|  12218 | 6247 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6248 | `	/* Search string */` |
|  12218 | 6249 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6250 | `		/* Collect search string */` |
|      9 | 6251 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6252 | `	}else{` |
|      - | 6253 | `		/* Single pattern */` |
|  12210 | 6254 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  12210 | 6255 | `		if( nByte < 1 ){` |
|      - | 6256 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6257 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6258 | `			return PH7_OK;` |
|      - | 6259 | `		}` |
|  12206 | 6260 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6261 | `		/* Save for later processing */` |
|  12206 | 6262 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6263 | `	}` |
|      - | 6264 | `	/* Replace string */` |
|  12214 | 6265 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6266 | `		/* Collect replace string */` |
|      7 | 6267 | `		sRep.pCollector = &sReplace;` |
|      7 | 6268 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6269 | `	}else{` |
|      - | 6270 | `		/* Single needle */` |
|  12208 | 6271 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  12208 | 6272 | `		rep_str = 1;` |
|  12208 | 6273 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6274 | `		/* Save for later processing */` |
|  12208 | 6275 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6276 | `	}` |
|      - | 6277 | `	/* Reset loop cursors */` |
|  12214 | 6278 | `	SySetResetCursor(&sSearch);` |
|  12214 | 6279 | `	SySetResetCursor(&sReplace);` |
|  12214 | 6280 | `	pReplace = pSearch = 0; /* cc warning */` |
|  12214 | 6281 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6282 | `	/* Extract function name */` |
|  12214 | 6283 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6284 | `	/* Set the default pattern match routine */` |
|  12214 | 6285 | `	xMatch = SyBlobSearch;` |
|  12214 | 6286 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6287 | `		/* Case insensitive pattern match */` |
|     11 | 6288 | `		xMatch = iPatternMatch;` |
|      5 | 6289 | `	}` |
|      - | 6290 | `	/* Start the replace process */` |
|  24434 | 6291 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6292 | `		sxu32 nCount,nOfft;` |
|  12222 | 6293 | `		if( pSearch->nByte <  1 ){` |
|      - | 6294 | `			/* Empty string,ignore */` |
|      3 | 6295 | `			continue;` |
|      - | 6296 | `		}` |
|      - | 6297 | `		/* Extract the replace string */` |
|  12220 | 6298 | `		if( rep_str ){` |
|  12210 | 6299 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   6106 | 6300 | `		}else{` |
|     11 | 6301 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6302 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6303 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6304 | `				 */` |
|      3 | 6305 | `				pReplace = 0;` |
|      1 | 6306 | `			}` |
|      - | 6307 | `		}` |
|  12220 | 6308 | `		if( pReplace == 0 ){` |
|      - | 6309 | `			/* Use an empty string instead */` |
|      3 | 6310 | `			pReplace = &sTemp;` |
|      1 | 6311 | `		}` |
|  12220 | 6312 | `		nOfft = nCount = 0;` |
|   6125 | 6313 | `		for(;;){` |
|  12252 | 6314 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6315 | `				break;` |
|      - | 6316 | `			}` |
|      - | 6317 | `			/* Perform a pattern lookup */` |
|  18359 | 6318 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  12238 | 6319 | `				pSearch->nByte,&nOfft);` |
|  12240 | 6320 | `			if( rc != SXRET_OK ){` |
|      - | 6321 | `				/* Pattern not found */` |
|  12208 | 6322 | `				break;` |
|      - | 6323 | `			}` |
|      - | 6324 | `			/* Perform the replace operation */` |
|     33 | 6325 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 6326 | `			/* Increment offset counter */` |
|     33 | 6327 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6328 | `		}` |
|      2 | 6329 | `	}` |
|      - | 6330 | `	/* All done,clean-up the mess left behind */` |
|  12214 | 6331 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  12214 | 6332 | `	SySetRelease(&sSearch);` |
|  12214 | 6333 | `	SySetRelease(&sReplace);` |
|  12214 | 6334 | `	SyBlobRelease(&sWorker);` |
|  12214 | 6335 | `	return PH7_OK;` |
|   6131 | 6336 |  |
|      - | 6337 | `/*` |
|      - | 6338 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6339 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6340 | ` *  Translate characters or replace substrings.` |
|      - | 6341 | ` * Parameters` |
|      - | 6342 | ` *  $str` |
|      - | 6343 | ` *  The string being translated.` |
|      - | 6344 | ` * $from` |
|      - | 6345 | ` *  The string being translated to to.` |
|      - | 6346 | ` * $to` |
|      - | 6347 | ` *  The string replacing from.` |
|      - | 6348 | ` * $replace_pairs` |
|      - | 6349 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6350 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6351 | ` * Return` |
|      - | 6352 | ` *  The translated string.` |
|      - | 6353 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6354 | ` */` |
|     12 | 6355 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6356 |  |
|      - | 6357 | `	const char *zIn;` |
|      - | 6358 | `	int nLen;` |
|     13 | 6359 | `	if( nArg < 1 ){` |
|      - | 6360 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6361 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6362 | `		return PH7_OK;` |
|      - | 6363 | `	}` |
|      7 | 6364 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6365 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6366 | `		/* Invalid arguments */` |
|    ! 0 | 6367 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6368 | `		return PH7_OK;` |
|      - | 6369 | `	}` |
|      9 | 6370 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6371 | `		str_replace_data sRepData;` |
|      - | 6372 | `		SyBlob sWorker;` |
|      - | 6373 | `		/* Initilaize the working buffer */` |
|      5 | 6374 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6375 | `		/* Copy raw string */` |
|      5 | 6376 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6377 | `		/* Init our replace data instance */` |
|      5 | 6378 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6379 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 6380 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6381 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 6382 | `		/* All done, return the result string */` |
|      7 | 6383 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6384 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6385 | `		/* Clean-up */` |
|      5 | 6386 | `		SyBlobRelease(&sWorker);` |
|      3 | 6387 | `	}else{` |
|      - | 6388 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6389 | `		const char *zFrom,*zTo;` |
|      3 | 6390 | `		if( nArg < 3 ){` |
|      - | 6391 | `			/* Nothing to replace */` |
|    ! 0 | 6392 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6393 | `			return PH7_OK;` |
|      - | 6394 | `		}` |
|      - | 6395 | `		/* Extract given arguments */` |
|      3 | 6396 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6397 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6398 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6399 | `			/* Nothing to replace */` |
|    ! 0 | 6400 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6401 | `			return PH7_OK;` |
|      - | 6402 | `		}` |
|      - | 6403 | `		/* Start the replace process */` |
|     13 | 6404 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6405 | `			c = zIn[i];` |
|     11 | 6406 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6407 | `				if ( iOfft < tlen ){` |
|      5 | 6408 | `					c = zTo[iOfft];` |
|      2 | 6409 | `				}` |
|      2 | 6410 | `			}` |
|     11 | 6411 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6412 |  |
|      6 | 6413 | `		}` |
|      - | 6414 | `	}` |
|      7 | 6415 | `	return PH7_OK;` |
|      7 | 6416 |  |
|      - | 6417 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6418 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6419 | `/*` |
|      - | 6420 | ` * Parse an INI string.` |
|      - | 6421 |  |
|      - | 6422 | ` * According to wikipedia` |
|      - | 6423 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6424 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6425 | ` *  Format` |
|      - | 6426 | `*    Properties` |
|      - | 6427 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6428 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6429 | `*     Example:` |
|      - | 6430 | `*      name=value` |
|      - | 6431 | `*    Sections` |
|      - | 6432 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6433 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6434 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6435 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6436 | `*     Example:` |
|      - | 6437 | `*      [section]` |
|      - | 6438 | `*   Comments` |
|      - | 6439 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6440 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6441 | `*/` |
|     12 | 6442 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6443 |  |
|      - | 6444 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6445 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6446 | `	SyHashEntry *pEntry;` |
|      - | 6447 | `	SyString sEntry;` |
|      - | 6448 | `	SyHash sHash;` |
|      - | 6449 | `	int c;` |
|      - | 6450 | `	/* Create an empty array and worker variables */` |
|     13 | 6451 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6452 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6453 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6454 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6455 | `		/* Out of memory */` |
|    ! 0 | 6456 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 6457 | `		/* Return FALSE */` |
|    ! 0 | 6458 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6459 | `		return PH7_OK;` |
|      - | 6460 | `	}` |
|     13 | 6461 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6462 | `	pCur = pArray;` |
|      - | 6463 | `	/* Start the parse process */` |
|     21 | 6464 | `	for(;;){` |
|      - | 6465 | `		/* Ignore leading white spaces */` |
|     69 | 6466 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6467 | `			zIn++;` |
|      1 | 6468 | `		}` |
|     43 | 6469 | `		if( zIn >= zEnd ){` |
|      - | 6470 | `			/* No more input to process */` |
|     13 | 6471 | `			break;` |
|      - | 6472 | `		}` |
|     31 | 6473 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6474 | `			/* Comment til the end of line */` |
|    ! 0 | 6475 | `			zIn++;` |
|    ! 0 | 6476 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6477 | `				zIn++;` |
|    ! 0 | 6478 | `			}` |
|    ! 0 | 6479 | `			continue;` |
|      - | 6480 | `		}` |
|      - | 6481 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6482 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6483 | `		if( zIn[0] == '[' ){` |
|      - | 6484 | `			/* Section: Extract the section name */` |
|      9 | 6485 | `			zIn++;` |
|      9 | 6486 | `			zCur = zIn;` |
|     73 | 6487 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6488 | `				zIn++;` |
|      1 | 6489 | `			}` |
|      9 | 6490 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6491 | `				/* Save the section name */` |
|      5 | 6492 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6493 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6494 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6495 | `				if( sEntry.nByte > 0 ){` |
|      - | 6496 | `					/* Associate an array with the section */` |
|      5 | 6497 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6498 | `					if( pSection ){` |
|      5 | 6499 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6500 | `						pCur = pSection;` |
|      2 | 6501 | `					}` |
|      2 | 6502 | `				}` |
|      2 | 6503 | `			}` |
|      9 | 6504 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6505 | `		}else{` |
|      - | 6506 | `			ph7_value *pOldCur;` |
|      - | 6507 | `			int is_array;` |
|      - | 6508 | `			int iLen;` |
|      - | 6509 | `			/* Properties */` |
|     23 | 6510 | `			is_array = 0;` |
|     23 | 6511 | `			zCur = zIn;` |
|     23 | 6512 | `			iLen = 0; /* cc warning */` |
|     23 | 6513 | `			pOldCur = pCur;` |
|    155 | 6514 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6515 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6516 | `					/* Array */` |
|    ! 0 | 6517 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6518 | `					is_array = 1;` |
|    ! 0 | 6519 | `					if( iLen > 0 ){` |
|    ! 0 | 6520 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6521 | `						/* Query the hashtable */` |
|    ! 0 | 6522 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6523 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6524 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6525 | `						if( pEntry ){` |
|    ! 0 | 6526 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6527 | `						}else{` |
|      - | 6528 | `							/* Create an empty array */` |
|    ! 0 | 6529 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6530 | `							if( pvArr ){` |
|      - | 6531 | `								/* Save the entry */` |
|    ! 0 | 6532 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6533 | `								/* Insert the entry */` |
|    ! 0 | 6534 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6535 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6536 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6537 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6538 | `							}` |
|      - | 6539 | `						}` |
|    ! 0 | 6540 | `						if( pvArr ){` |
|    ! 0 | 6541 | `							pCur = pvArr;` |
|    ! 0 | 6542 | `						}` |
|    ! 0 | 6543 | `					}` |
|    ! 0 | 6544 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6545 | `						zIn++;` |
|    ! 0 | 6546 | `					}` |
|    ! 0 | 6547 | `				}` |
|    133 | 6548 | `				zIn++;` |
|      1 | 6549 | `			}` |
|     23 | 6550 | `			if( !is_array ){` |
|     23 | 6551 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6552 | `			}` |
|      - | 6553 | `			/* Trim the key */` |
|     23 | 6554 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6555 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6556 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6557 | `				if( !is_array ){` |
|      - | 6558 | `					/* Save the key name */` |
|     23 | 6559 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6560 | `				}` |
|      - | 6561 | `				/* extract key value */` |
|     23 | 6562 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6563 | `				zIn++; /* '=' */` |
|     39 | 6564 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6565 | `					zIn++;` |
|      1 | 6566 | `				}` |
|     23 | 6567 | `				if( zIn < zEnd ){` |
|     21 | 6568 | `					zCur = zIn;` |
|     21 | 6569 | `					c = zIn[0];` |
|     21 | 6570 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6571 | `						zIn++;` |
|      - | 6572 | `						/* Delimit the value */` |
|    ! 0 | 6573 | `						while( zIn < zEnd ){` |
|    ! 0 | 6574 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6575 | `								break;` |
|      - | 6576 | `							}` |
|    ! 0 | 6577 | `							zIn++;` |
|    ! 0 | 6578 | `						}` |
|    ! 0 | 6579 | `						if( zIn < zEnd ){` |
|    ! 0 | 6580 | `							zIn++;` |
|    ! 0 | 6581 | `						}` |
|    ! 0 | 6582 | `					}else{` |
|    125 | 6583 | `						while( zIn < zEnd ){` |
|    123 | 6584 | `							if( zIn[0] == '\n' ){` |
|     19 | 6585 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6586 | `									break;` |
|    ! 0 | 6587 | `								}` |
|    105 | 6588 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6589 | `								/* Inline comments */` |
|    ! 0 | 6590 | `								break;` |
|      - | 6591 | `							}` |
|    105 | 6592 | `							zIn++;` |
|      1 | 6593 | `						}` |
|      - | 6594 | `					}` |
|      - | 6595 | `					/* Trim the value */` |
|     21 | 6596 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6597 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6598 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6599 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6600 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6601 | `					}` |
|     21 | 6602 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6603 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6604 | `					}` |
|      - | 6605 | `					/* Insert the key and it's value */` |
|     21 | 6606 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6607 | `				}` |
|     12 | 6608 | `			}else{` |
|    ! 0 | 6609 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6610 | `					zIn++;` |
|    ! 0 | 6611 | `				}` |
|      - | 6612 | `			}` |
|     23 | 6613 | `			pCur = pOldCur;` |
|      - | 6614 | `		}` |
|      1 | 6615 | `	}` |
|     13 | 6616 | `	SyHashRelease(&sHash);` |
|      - | 6617 | `	/* Return the parse of the INI string */` |
|     13 | 6618 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6619 | `	return SXRET_OK;` |
|      7 | 6620 |  |
|      - | 6621 | `/*` |
|      - | 6622 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6623 | ` *  Parse a configuration string.` |
|      - | 6624 | ` * Parameters` |
|      - | 6625 | ` *  $ini` |
|      - | 6626 | ` *   The contents of the ini file being parsed.` |
|      - | 6627 | ` *  $process_sections` |
|      - | 6628 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6629 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6630 | ` *  $scanner_mode (Not used)` |
|      - | 6631 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6632 | ` *   then option values will not be parsed.` |
|      - | 6633 | ` * Return` |
|      - | 6634 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6635 | ` */` |
|     10 | 6636 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6637 |  |
|      - | 6638 | `	const char *zIni;` |
|      - | 6639 | `	int nByte;` |
|     11 | 6640 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6641 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6642 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6643 | `		return PH7_OK;` |
|      - | 6644 | `	}` |
|      - | 6645 | `	/* Extract the raw INI buffer */` |
|     11 | 6646 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6647 | `	/* Process the INI buffer*/` |
|     11 | 6648 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 6649 | `	return PH7_OK;` |
|      6 | 6650 |  |
|      - | 6651 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6652 |  |
|      - | 6653 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6654 |  |
|      - | 6655 | `/*` |
|      - | 6656 | ` * Ctype Functions.` |
|      - | 6657 | ` * Status:` |
|      - | 6658 | ` *    Stable.` |
|      - | 6659 | ` */` |
|      - | 6660 | `/*` |
|      - | 6661 | ` * bool ctype_alnum(string $text)` |
|      - | 6662 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6663 | ` * Parameters` |
|      - | 6664 | ` *  $text` |
|      - | 6665 | ` *   The tested string.` |
|      - | 6666 | ` * Return` |
|      - | 6667 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6668 | ` */` |
|     16 | 6669 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6670 |  |
|      - | 6671 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6672 | `	int nLen;` |
|     17 | 6673 | `	if( nArg < 1 ){` |
|      - | 6674 | `		/* Missing arguments,return FALSE */` |
|      3 | 6675 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6676 | `		return PH7_OK;` |
|      - | 6677 | `	}` |
|      - | 6678 | `	/* Extract the target string */` |
|     15 | 6679 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6680 | `	zEnd = &zIn[nLen];` |
|     15 | 6681 | `	if( nLen < 1 ){` |
|      - | 6682 | `		/* Empty string,return FALSE */` |
|      3 | 6683 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6684 | `		return PH7_OK;` |
|      - | 6685 | `	}` |
|      - | 6686 | `	/* Perform the requested operation */` |
|     32 | 6687 | `	for(;;){` |
|     65 | 6688 | `		if( zIn >= zEnd ){` |
|      - | 6689 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6690 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6691 | `			return PH7_OK;` |
|      - | 6692 | `		}` |
|     57 | 6693 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6694 | `			break;` |
|      - | 6695 | `		}` |
|      - | 6696 | `		/* Point to the next character */` |
|     53 | 6697 | `		zIn++;` |
|      1 | 6698 | `	}` |
|      - | 6699 | `	/* The test failed,return FALSE */` |
|      5 | 6700 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6701 | `	return PH7_OK;` |
|      9 | 6702 |  |
|      - | 6703 | `/*` |
|      - | 6704 | ` * bool ctype_alpha(string $text)` |
|      - | 6705 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6706 | ` * Parameters` |
|      - | 6707 | ` *  $text` |
|      - | 6708 | ` *   The tested string.` |
|      - | 6709 | ` * Return` |
|      - | 6710 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6711 | ` */` |
|     18 | 6712 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6713 |  |
|      - | 6714 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6715 | `	int nLen;` |
|     19 | 6716 | `	if( nArg < 1 ){` |
|      - | 6717 | `		/* Missing arguments,return FALSE */` |
|      3 | 6718 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6719 | `		return PH7_OK;` |
|      - | 6720 | `	}` |
|      - | 6721 | `	/* Extract the target string */` |
|     17 | 6722 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6723 | `	zEnd = &zIn[nLen];` |
|     17 | 6724 | `	if( nLen < 1 ){` |
|      - | 6725 | `		/* Empty string,return FALSE */` |
|      3 | 6726 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6727 | `		return PH7_OK;` |
|      - | 6728 | `	}` |
|      - | 6729 | `	/* Perform the requested operation */` |
|     42 | 6730 | `	for(;;){` |
|     85 | 6731 | `		if( zIn >= zEnd ){` |
|      - | 6732 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6733 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6734 | `			return PH7_OK;` |
|      - | 6735 | `		}` |
|     77 | 6736 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6737 | `			break;` |
|      - | 6738 | `		}` |
|      - | 6739 | `		/* Point to the next character */` |
|     71 | 6740 | `		zIn++;` |
|      1 | 6741 | `	}` |
|      - | 6742 | `	/* The test failed,return FALSE */` |
|      7 | 6743 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6744 | `	return PH7_OK;` |
|     10 | 6745 |  |
|      - | 6746 | `/*` |
|      - | 6747 | ` * bool ctype_cntrl(string $text)` |
|      - | 6748 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6749 | ` * Parameters` |
|      - | 6750 | ` *  $text` |
|      - | 6751 | ` *   The tested string.` |
|      - | 6752 | ` * Return` |
|      - | 6753 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6754 | ` */` |
|     18 | 6755 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6756 |  |
|      - | 6757 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6758 | `	int nLen;` |
|     19 | 6759 | `	if( nArg < 1 ){` |
|      - | 6760 | `		/* Missing arguments,return FALSE */` |
|      3 | 6761 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6762 | `		return PH7_OK;` |
|      - | 6763 | `	}` |
|      - | 6764 | `	/* Extract the target string */` |
|     17 | 6765 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6766 | `	zEnd = &zIn[nLen];` |
|     17 | 6767 | `	if( nLen < 1 ){` |
|      - | 6768 | `		/* Empty string,return FALSE */` |
|      3 | 6769 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6770 | `		return PH7_OK;` |
|      - | 6771 | `	}` |
|      - | 6772 | `	/* Perform the requested operation */` |
|     14 | 6773 | `	for(;;){` |
|     29 | 6774 | `		if( zIn >= zEnd ){` |
|      - | 6775 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6776 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6777 | `			return PH7_OK;` |
|      - | 6778 | `		}` |
|     21 | 6779 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6780 | `			/* UTF-8 stream  */` |
|    ! 0 | 6781 | `			break;` |
|      - | 6782 | `		}` |
|     21 | 6783 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6784 | `			break;` |
|      - | 6785 | `		}` |
|      - | 6786 | `		/* Point to the next character */` |
|     15 | 6787 | `		zIn++;` |
|      1 | 6788 | `	}` |
|      - | 6789 | `	/* The test failed,return FALSE */` |
|      7 | 6790 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6791 | `	return PH7_OK;` |
|     10 | 6792 |  |
|      - | 6793 | `/*` |
|      - | 6794 | ` * bool ctype_digit(string $text)` |
|      - | 6795 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6796 | ` * Parameters` |
|      - | 6797 | ` *  $text` |
|      - | 6798 | ` *   The tested string.` |
|      - | 6799 | ` * Return` |
|      - | 6800 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6801 | ` */` |
|   1500 | 6802 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6803 |  |
|      - | 6804 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6805 | `	int nLen;` |
|   1502 | 6806 | `	if( nArg < 1 ){` |
|      - | 6807 | `		/* Missing arguments,return FALSE */` |
|      3 | 6808 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6809 | `		return PH7_OK;` |
|      - | 6810 | `	}` |
|      - | 6811 | `	/* Extract the target string */` |
|   1500 | 6812 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1500 | 6813 | `	zEnd = &zIn[nLen];` |
|   1500 | 6814 | `	if( nLen < 1 ){` |
|      - | 6815 | `		/* Empty string,return FALSE */` |
|      3 | 6816 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6817 | `		return PH7_OK;` |
|      - | 6818 | `	}` |
|      - | 6819 | `	/* Perform the requested operation */` |
|   1404 | 6820 | `	for(;;){` |
|   2810 | 6821 | `		if( zIn >= zEnd ){` |
|      - | 6822 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1282 | 6823 | `			ph7_result_bool(pCtx,1);` |
|   1282 | 6824 | `			return PH7_OK;` |
|      - | 6825 | `		}` |
|   1530 | 6826 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6827 | `			/* UTF-8 stream  */` |
|    ! 0 | 6828 | `			break;` |
|      - | 6829 | `		}` |
|   1530 | 6830 | `		if( !SyisDigit(zIn[0]) ){` |
|    218 | 6831 | `			break;` |
|      - | 6832 | `		}` |
|      - | 6833 | `		/* Point to the next character */` |
|   1314 | 6834 | `		zIn++;` |
|      2 | 6835 | `	}` |
|      - | 6836 | `	/* The test failed,return FALSE */` |
|    218 | 6837 | `	ph7_result_bool(pCtx,0);` |
|    218 | 6838 | `	return PH7_OK;` |
|    752 | 6839 |  |
|      - | 6840 | `/*` |
|      - | 6841 | ` * bool ctype_xdigit(string $text)` |
|      - | 6842 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6843 | ` * Parameters` |
|      - | 6844 | ` *  $text` |
|      - | 6845 | ` *   The tested string.` |
|      - | 6846 | ` * Return` |
|      - | 6847 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6848 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6849 | ` */` |
|     20 | 6850 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6851 |  |
|      - | 6852 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6853 | `	int nLen;` |
|     21 | 6854 | `	if( nArg < 1 ){` |
|      - | 6855 | `		/* Missing arguments,return FALSE */` |
|      3 | 6856 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6857 | `		return PH7_OK;` |
|      - | 6858 | `	}` |
|      - | 6859 | `	/* Extract the target string */` |
|     19 | 6860 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6861 | `	zEnd = &zIn[nLen];` |
|     19 | 6862 | `	if( nLen < 1 ){` |
|      - | 6863 | `		/* Empty string,return FALSE */` |
|      3 | 6864 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6865 | `		return PH7_OK;` |
|      - | 6866 | `	}` |
|      - | 6867 | `	/* Perform the requested operation */` |
|     46 | 6868 | `	for(;;){` |
|     93 | 6869 | `		if( zIn >= zEnd ){` |
|      - | 6870 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6871 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6872 | `			return PH7_OK;` |
|      - | 6873 | `		}` |
|     83 | 6874 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6875 | `			/* UTF-8 stream  */` |
|    ! 0 | 6876 | `			break;` |
|      - | 6877 | `		}` |
|     83 | 6878 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6879 | `			break;` |
|      - | 6880 | `		}` |
|      - | 6881 | `		/* Point to the next character */` |
|     77 | 6882 | `		zIn++;` |
|      1 | 6883 | `	}` |
|      - | 6884 | `	/* The test failed,return FALSE */` |
|      7 | 6885 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6886 | `	return PH7_OK;` |
|     11 | 6887 |  |
|      - | 6888 | `/*` |
|      - | 6889 | ` * bool ctype_graph(string $text)` |
|      - | 6890 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6891 | ` * Parameters` |
|      - | 6892 | ` *  $text` |
|      - | 6893 | ` *   The tested string.` |
|      - | 6894 | ` * Return` |
|      - | 6895 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6896 | ` * (no white space), FALSE otherwise.` |
|      - | 6897 | ` */` |
|     18 | 6898 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6899 |  |
|      - | 6900 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6901 | `	int nLen;` |
|     19 | 6902 | `	if( nArg < 1 ){` |
|      - | 6903 | `		/* Missing arguments,return FALSE */` |
|      3 | 6904 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6905 | `		return PH7_OK;` |
|      - | 6906 | `	}` |
|      - | 6907 | `	/* Extract the target string */` |
|     17 | 6908 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6909 | `	zEnd = &zIn[nLen];` |
|     17 | 6910 | `	if( nLen < 1 ){` |
|      - | 6911 | `		/* Empty string,return FALSE */` |
|      3 | 6912 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6913 | `		return PH7_OK;` |
|      - | 6914 | `	}` |
|      - | 6915 | `	/* Perform the requested operation */` |
|     57 | 6916 | `	for(;;){` |
|    115 | 6917 | `		if( zIn >= zEnd ){` |
|      - | 6918 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6919 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6920 | `			return PH7_OK;` |
|      - | 6921 | `		}` |
|    107 | 6922 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6923 | `			/* UTF-8 stream  */` |
|    ! 0 | 6924 | `			break;` |
|      - | 6925 | `		}` |
|    107 | 6926 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6927 | `			break;` |
|      - | 6928 | `		}` |
|      - | 6929 | `		/* Point to the next character */` |
|    101 | 6930 | `		zIn++;` |
|      1 | 6931 | `	}` |
|      - | 6932 | `	/* The test failed,return FALSE */` |
|      7 | 6933 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6934 | `	return PH7_OK;` |
|     10 | 6935 |  |
|      - | 6936 | `/*` |
|      - | 6937 | ` * bool ctype_print(string $text)` |
|      - | 6938 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6939 | ` * Parameters` |
|      - | 6940 | ` *  $text` |
|      - | 6941 | ` *   The tested string.` |
|      - | 6942 | ` * Return` |
|      - | 6943 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6944 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6945 | ` *  or control function at all.` |
|      - | 6946 | ` */` |
|     18 | 6947 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6948 |  |
|      - | 6949 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6950 | `	int nLen;` |
|     19 | 6951 | `	if( nArg < 1 ){` |
|      - | 6952 | `		/* Missing arguments,return FALSE */` |
|      3 | 6953 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6954 | `		return PH7_OK;` |
|      - | 6955 | `	}` |
|      - | 6956 | `	/* Extract the target string */` |
|     17 | 6957 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6958 | `	zEnd = &zIn[nLen];` |
|     17 | 6959 | `	if( nLen < 1 ){` |
|      - | 6960 | `		/* Empty string,return FALSE */` |
|      3 | 6961 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6962 | `		return PH7_OK;` |
|      - | 6963 | `	}` |
|      - | 6964 | `	/* Perform the requested operation */` |
|     63 | 6965 | `	for(;;){` |
|    127 | 6966 | `		if( zIn >= zEnd ){` |
|      - | 6967 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6968 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6969 | `			return PH7_OK;` |
|      - | 6970 | `		}` |
|    119 | 6971 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6972 | `			/* UTF-8 stream  */` |
|    ! 0 | 6973 | `			break;` |
|      - | 6974 | `		}` |
|    119 | 6975 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6976 | `			break;` |
|      - | 6977 | `		}` |
|      - | 6978 | `		/* Point to the next character */` |
|    113 | 6979 | `		zIn++;` |
|      1 | 6980 | `	}` |
|      - | 6981 | `	/* The test failed,return FALSE */` |
|      7 | 6982 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6983 | `	return PH7_OK;` |
|     10 | 6984 |  |
|      - | 6985 | `/*` |
|      - | 6986 | ` * bool ctype_punct(string $text)` |
|      - | 6987 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6988 | ` * Parameters` |
|      - | 6989 | ` *  $text` |
|      - | 6990 | ` *   The tested string.` |
|      - | 6991 | ` * Return` |
|      - | 6992 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6993 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6994 | ` */` |
|     20 | 6995 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6996 |  |
|      - | 6997 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6998 | `	int nLen;` |
|     21 | 6999 | `	if( nArg < 1 ){` |
|      - | 7000 | `		/* Missing arguments,return FALSE */` |
|      3 | 7001 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7002 | `		return PH7_OK;` |
|      - | 7003 | `	}` |
|      - | 7004 | `	/* Extract the target string */` |
|     19 | 7005 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7006 | `	zEnd = &zIn[nLen];` |
|     19 | 7007 | `	if( nLen < 1 ){` |
|      - | 7008 | `		/* Empty string,return FALSE */` |
|      3 | 7009 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7010 | `		return PH7_OK;` |
|      - | 7011 | `	}` |
|      - | 7012 | `	/* Perform the requested operation */` |
|     38 | 7013 | `	for(;;){` |
|     77 | 7014 | `		if( zIn >= zEnd ){` |
|      - | 7015 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7016 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7017 | `			return PH7_OK;` |
|      - | 7018 | `		}` |
|     69 | 7019 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7020 | `			/* UTF-8 stream  */` |
|    ! 0 | 7021 | `			break;` |
|      - | 7022 | `		}` |
|     69 | 7023 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7024 | `			break;` |
|      - | 7025 | `		}` |
|      - | 7026 | `		/* Point to the next character */` |
|     61 | 7027 | `		zIn++;` |
|      1 | 7028 | `	}` |
|      - | 7029 | `	/* The test failed,return FALSE */` |
|      9 | 7030 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7031 | `	return PH7_OK;` |
|     11 | 7032 |  |
|      - | 7033 | `/*` |
|      - | 7034 | ` * bool ctype_space(string $text)` |
|      - | 7035 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7036 | ` * Parameters` |
|      - | 7037 | ` *  $text` |
|      - | 7038 | ` *   The tested string.` |
|      - | 7039 | ` * Return` |
|      - | 7040 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7041 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7042 | ` *  and form feed characters.` |
|      - | 7043 | ` */` |
|  36634 | 7044 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7045 |  |
|      - | 7046 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7047 | `	int nLen;` |
|  36636 | 7048 | `	if( nArg < 1 ){` |
|      - | 7049 | `		/* Missing arguments,return FALSE */` |
|      3 | 7050 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7051 | `		return PH7_OK;` |
|      - | 7052 | `	}` |
|      - | 7053 | `	/* Extract the target string */` |
|  36634 | 7054 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  36634 | 7055 | `	zEnd = &zIn[nLen];` |
|  36634 | 7056 | `	if( nLen < 1 ){` |
|      - | 7057 | `		/* Empty string,return FALSE */` |
|      3 | 7058 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7059 | `		return PH7_OK;` |
|      - | 7060 | `	}` |
|      - | 7061 | `	/* Perform the requested operation */` |
|  18647 | 7062 | `	for(;;){` |
|  37252 | 7063 | `		if( zIn >= zEnd ){` |
|      - | 7064 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    598 | 7065 | `			ph7_result_bool(pCtx,1);` |
|    598 | 7066 | `			return PH7_OK;` |
|      - | 7067 | `		}` |
|  36656 | 7068 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7069 | `			/* UTF-8 stream  */` |
|    ! 0 | 7070 | `			break;` |
|      - | 7071 | `		}` |
|  36656 | 7072 | `		if( !SyisSpace(zIn[0]) ){` |
|  36036 | 7073 | `			break;` |
|      - | 7074 | `		}` |
|      - | 7075 | `		/* Point to the next character */` |
|    622 | 7076 | `		zIn++;` |
|      2 | 7077 | `	}` |
|      - | 7078 | `	/* The test failed,return FALSE */` |
|  36036 | 7079 | `	ph7_result_bool(pCtx,0);` |
|  36036 | 7080 | `	return PH7_OK;` |
|  18341 | 7081 |  |
|      - | 7082 | `/*` |
|      - | 7083 | ` * bool ctype_lower(string $text)` |
|      - | 7084 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7085 | ` * Parameters` |
|      - | 7086 | ` *  $text` |
|      - | 7087 | ` *   The tested string.` |
|      - | 7088 | ` * Return` |
|      - | 7089 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7090 | ` */` |
|     18 | 7091 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7092 |  |
|      - | 7093 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7094 | `	int nLen;` |
|     19 | 7095 | `	if( nArg < 1 ){` |
|      - | 7096 | `		/* Missing arguments,return FALSE */` |
|      3 | 7097 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7098 | `		return PH7_OK;` |
|      - | 7099 | `	}` |
|      - | 7100 | `	/* Extract the target string */` |
|     17 | 7101 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7102 | `	zEnd = &zIn[nLen];` |
|     17 | 7103 | `	if( nLen < 1 ){` |
|      - | 7104 | `		/* Empty string,return FALSE */` |
|      3 | 7105 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7106 | `		return PH7_OK;` |
|      - | 7107 | `	}` |
|      - | 7108 | `	/* Perform the requested operation */` |
|     27 | 7109 | `	for(;;){` |
|     55 | 7110 | `		if( zIn >= zEnd ){` |
|      - | 7111 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7112 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7113 | `			return PH7_OK;` |
|      - | 7114 | `		}` |
|     51 | 7115 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7116 | `			break;` |
|      - | 7117 | `		}` |
|      - | 7118 | `		/* Point to the next character */` |
|     41 | 7119 | `		zIn++;` |
|      1 | 7120 | `	}` |
|      - | 7121 | `	/* The test failed,return FALSE */` |
|     11 | 7122 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7123 | `	return PH7_OK;` |
|     10 | 7124 |  |
|      - | 7125 | `/*` |
|      - | 7126 | ` * bool ctype_upper(string $text)` |
|      - | 7127 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7128 | ` * Parameters` |
|      - | 7129 | ` *  $text` |
|      - | 7130 | ` *   The tested string.` |
|      - | 7131 | ` * Return` |
|      - | 7132 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7133 | ` */` |
|     18 | 7134 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7135 |  |
|      - | 7136 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7137 | `	int nLen;` |
|     19 | 7138 | `	if( nArg < 1 ){` |
|      - | 7139 | `		/* Missing arguments,return FALSE */` |
|      3 | 7140 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7141 | `		return PH7_OK;` |
|      - | 7142 | `	}` |
|      - | 7143 | `	/* Extract the target string */` |
|     17 | 7144 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7145 | `	zEnd = &zIn[nLen];` |
|     17 | 7146 | `	if( nLen < 1 ){` |
|      - | 7147 | `		/* Empty string,return FALSE */` |
|      3 | 7148 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7149 | `		return PH7_OK;` |
|      - | 7150 | `	}` |
|      - | 7151 | `	/* Perform the requested operation */` |
|     28 | 7152 | `	for(;;){` |
|     57 | 7153 | `		if( zIn >= zEnd ){` |
|      - | 7154 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7155 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7156 | `			return PH7_OK;` |
|      - | 7157 | `		}` |
|     53 | 7158 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7159 | `			break;` |
|      - | 7160 | `		}` |
|      - | 7161 | `		/* Point to the next character */` |
|     43 | 7162 | `		zIn++;` |
|      1 | 7163 | `	}` |
|      - | 7164 | `	/* The test failed,return FALSE */` |
|     11 | 7165 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7166 | `	return PH7_OK;` |
|     10 | 7167 |  |
|      - | 7168 | `/*` |
|      - | 7169 | ` * Date/Time functions` |
|      - | 7170 | ` * Status:` |
|      - | 7171 | ` *    Devel.` |
|      - | 7172 | ` */` |
|      - | 7173 | `#include <time.h>` |
|      - | 7174 | `#ifdef __WINNT__` |
|      - | 7175 | `/* GetSystemTime() */` |
|      - | 7176 | `#include <Windows.h>` |
|      - | 7177 | `#ifdef _WIN32_WCE` |
|      - | 7178 | `/*` |
|      - | 7179 | `** WindowsCE does not have a localtime() function.  So create a` |
|      - | 7180 | `** substitute.` |
|      - | 7181 | `** Taken from the SQLite3 source tree.` |
|      - | 7182 | `** Status: Public domain` |
|      - | 7183 | `*/` |
|      - | 7184 | `struct tm *__cdecl localtime(const time_t *t)` |
|      - | 7185 |  |
|      - | 7186 | `  static struct tm y;` |
|      - | 7187 | `  FILETIME uTm, lTm;` |
|      - | 7188 | `  SYSTEMTIME pTm;` |
|      - | 7189 | `  ph7_int64 t64;` |
|      - | 7190 | `  t64 = *t;` |
|      - | 7191 | `  t64 = (t64 + 11644473600)*10000000;` |
|      - | 7192 | `  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);` |
|      - | 7193 | `  uTm.dwHighDateTime= (DWORD)(t64 >> 32);` |
|      - | 7194 | `  FileTimeToLocalFileTime(&uTm,&lTm);` |
|      - | 7195 | `  FileTimeToSystemTime(&lTm,&pTm);` |
|      - | 7196 | `  y.tm_year = pTm.wYear - 1900;` |
|      - | 7197 | `  y.tm_mon = pTm.wMonth - 1;` |
|      - | 7198 | `  y.tm_wday = pTm.wDayOfWeek;` |
|      - | 7199 | `  y.tm_mday = pTm.wDay;` |
|      - | 7200 | `  y.tm_hour = pTm.wHour;` |
|      - | 7201 | `  y.tm_min = pTm.wMinute;` |
|      - | 7202 | `  y.tm_sec = pTm.wSecond;` |
|      - | 7203 | `  return &y;` |
|      - | 7204 |  |
|      - | 7205 | `#endif /*_WIN32_WCE */` |
|      - | 7206 | `#elif defined(__UNIXES__)` |
|      - | 7207 | `#include <sys/time.h>` |
|      - | 7208 | `#endif /* __WINNT__*/` |
|      - | 7209 | ` /*` |
|      - | 7210 | `  * int64 time(void)` |
|      - | 7211 | `  *  Current Unix timestamp` |
|      - | 7212 | `  * Parameters` |
|      - | 7213 | `  *  None.` |
|      - | 7214 | `  * Return` |
|      - | 7215 | `  *  Returns the current time measured in the number of seconds` |
|      - | 7216 | `  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).` |
|      - | 7217 | `  */` |
|      8 | 7218 | `static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7219 |  |
|      - | 7220 | `	time_t tt;` |
|      4 | 7221 | `	SXUNUSED(nArg); /* cc warning */` |
|      4 | 7222 | `	SXUNUSED(apArg);` |
|      - | 7223 | `	/* Extract the current time */` |
|      9 | 7224 | `	time(&tt);` |
|      - | 7225 | `	/* Return as 64-bit integer */` |
|      9 | 7226 | `	ph7_result_int64(pCtx,(ph7_int64)tt);` |
|      9 | 7227 | `	return  PH7_OK;` |
|      1 | 7228 |  |
|      - | 7229 | `/*` |
|      - | 7230 | `  * string/float microtime([ bool $get_as_float = false ])` |
|      - | 7231 | `  *  microtime() returns the current Unix timestamp with microseconds.` |
|      - | 7232 | `  * Parameters` |
|      - | 7233 | `  *  $get_as_float` |
|      - | 7234 | `  *   If used and set to TRUE, microtime() will return a float instead of a string` |
|      - | 7235 | `  *   as described in the return values section below.` |
|      - | 7236 | `  * Return` |
|      - | 7237 | `  *  By default, microtime() returns a string in the form "msec sec", where sec` |
|      - | 7238 | `  *  is the current time measured in the number of seconds since the Unix` |
|      - | 7239 | `  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds` |
|      - | 7240 | `  *  that have elapsed since sec expressed in seconds.` |
|      - | 7241 | `  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents` |
|      - | 7242 | `  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond.` |
|      - | 7243 | `  */` |
|     20 | 7244 | `static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7245 |  |
|     21 | 7246 | `	int bFloat = 0;` |
|      - | 7247 | `	sytime sTime;` |
|      - | 7248 | `#if defined(__UNIXES__)` |
|      - | 7249 | `	struct timeval tv;` |
|     20 | 7250 | `	gettimeofday(&tv,0);` |
|     20 | 7251 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|     20 | 7252 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7253 | `#else` |
|      - | 7254 | `	time_t tt;` |
|      1 | 7255 | `	time(&tt);` |
|      1 | 7256 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7257 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7258 | `#endif /* __UNIXES__ */` |
|     21 | 7259 | `	if( nArg > 0 ){` |
|     17 | 7260 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      8 | 7261 | `	}` |
|     21 | 7262 | `	if( bFloat ){` |
|      - | 7263 | `		/* Return as float */` |
|     17 | 7264 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      9 | 7265 | `	}else{` |
|      - | 7266 | `		/* Return as string */` |
|      5 | 7267 | `		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);` |
|      - | 7268 | `	}` |
|     21 | 7269 | `	return PH7_OK;` |
|      1 | 7270 |  |
|      - | 7271 | `/*` |
|      - | 7272 | ` * array getdate ([ int $timestamp = time() ])` |
|      - | 7273 | ` *  Get date/time information.` |
|      - | 7274 | ` * Parameter` |
|      - | 7275 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7276 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 7277 | ` *     In other words, it defaults to the value of time().` |
|      - | 7278 | ` * Returns` |
|      - | 7279 | ` *  Returns an associative array of information related to the timestamp.` |
|      - | 7280 | ` *  Elements from the returned associative array are as follows:` |
|      - | 7281 | ` *   KEY                                                         VALUE` |
|      - | 7282 | ` * ---------                                                    -------` |
|      - | 7283 | ` * "seconds" 	Numeric representation of seconds 	            0 to 59` |
|      - | 7284 | ` * "minutes" 	Numeric representation of minutes 	            0 to 59` |
|      - | 7285 | ` * "hours" 	    Numeric representation of hours 	            0 to 23` |
|      - | 7286 | ` * "mday" 	    Numeric representation of the day of the month 	1 to 31` |
|      - | 7287 | ` * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)` |
|      - | 7288 | ` * "mon" 	    Numeric representation of a month 	            1 through 12` |
|      - | 7289 | ` * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003` |
|      - | 7290 | ` * "yday" 	    Numeric representation of the day of the year   0 through 365` |
|      - | 7291 | ` * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday` |
|      - | 7292 | ` * "month" 	    A full textual representation of a month, such as January or March 	January through December` |
|      - | 7293 | ` * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date().` |
|      - | 7294 | ` * NOTE:` |
|      - | 7295 | ` *   NULL is returned on failure.` |
|      - | 7296 | ` */` |
|      8 | 7297 | `static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7298 |  |
|      - | 7299 | `	ph7_value *pValue,*pArray;` |
|      - | 7300 | `	Sytm sTm;` |
|      9 | 7301 | `	if( nArg < 1 ){` |
|      - | 7302 | `#ifdef __WINNT__` |
|      - | 7303 | `		SYSTEMTIME sOS;` |
|      1 | 7304 | `		GetSystemTime(&sOS);` |
|      1 | 7305 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7306 | `#else` |
|      - | 7307 | `		struct tm *pTm;` |
|      - | 7308 | `		time_t t;` |
|      4 | 7309 | `		time(&t);` |
|      4 | 7310 | `		pTm = localtime(&t);` |
|      4 | 7311 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7312 | `#endif` |
|      3 | 7313 | `	}else{` |
|      - | 7314 | `		/* Use the given timestamp */` |
|      - | 7315 | `		time_t t;` |
|      - | 7316 | `		struct tm *pTm;` |
|      - | 7317 | `#ifdef __WINNT__` |
|      - | 7318 | `#ifdef _MSC_VER` |
|      - | 7319 | `#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */` |
|      - | 7320 | `#pragma warning(disable:4996) /* _CRT_SECURE...*/` |
|      - | 7321 | `#endif` |
|      - | 7322 | `#endif` |
|      - | 7323 | `#endif` |
|      5 | 7324 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 7325 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 7326 | `			pTm = localtime(&t);` |
|      5 | 7327 | `			if( pTm == 0 ){` |
|    ! 0 | 7328 | `				time(&t);` |
|    ! 0 | 7329 | `			}` |
|      3 | 7330 | `		}else{` |
|    ! 0 | 7331 | `			time(&t);` |
|      - | 7332 | `		}` |
|      5 | 7333 | `		pTm = localtime(&t);` |
|      5 | 7334 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7335 | `	}` |
|      - | 7336 | `	/* Element value */` |
|      9 | 7337 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 7338 | `	if( pValue == 0 ){` |
|      - | 7339 | `		/* Return NULL */` |
|    ! 0 | 7340 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7341 | `		return PH7_OK;` |
|      - | 7342 | `	}` |
|      - | 7343 | `	/* Create a new array */` |
|      9 | 7344 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 7345 | `	if( pArray == 0 ){` |
|      - | 7346 | `		/* Return NULL */` |
|    ! 0 | 7347 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7348 | `		return PH7_OK;` |
|      - | 7349 | `	}` |
|      - | 7350 | `	/* Fill the array */` |
|      - | 7351 | `	/* Seconds */` |
|      9 | 7352 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 7353 | `	ph7_array_add_strkey_elem(pArray,"seconds",pValue);` |
|      - | 7354 | `	/* Minutes */` |
|      9 | 7355 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 7356 | `	ph7_array_add_strkey_elem(pArray,"minutes",pValue);` |
|      - | 7357 | `	/* Hours */` |
|      9 | 7358 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 7359 | `	ph7_array_add_strkey_elem(pArray,"hours",pValue);` |
|      - | 7360 | `	/* mday */` |
|      9 | 7361 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 7362 | `	ph7_array_add_strkey_elem(pArray,"mday",pValue);` |
|      - | 7363 | `	/* wday */` |
|      9 | 7364 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 7365 | `	ph7_array_add_strkey_elem(pArray,"wday",pValue);` |
|      - | 7366 | `	/* mon */` |
|      9 | 7367 | `	ph7_value_int(pValue,sTm.tm_mon+1);` |
|      9 | 7368 | `	ph7_array_add_strkey_elem(pArray,"mon",pValue);` |
|      - | 7369 | `	/* year */` |
|      9 | 7370 | `	ph7_value_int(pValue,sTm.tm_year);` |
|      9 | 7371 | `	ph7_array_add_strkey_elem(pArray,"year",pValue);` |
|      - | 7372 | `	/* yday */` |
|      9 | 7373 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 7374 | `	ph7_array_add_strkey_elem(pArray,"yday",pValue);` |
|      - | 7375 | `	/* Weekday */` |
|      9 | 7376 | `	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);` |
|      9 | 7377 | `	ph7_array_add_strkey_elem(pArray,"weekday",pValue);` |
|      - | 7378 | `	/* Month */` |
|      9 | 7379 | `	ph7_value_reset_string_cursor(pValue);` |
|      9 | 7380 | `	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);` |
|      9 | 7381 | `	ph7_array_add_strkey_elem(pArray,"month",pValue);` |
|      - | 7382 | `	/* Seconds since the epoch */` |
|      9 | 7383 | `	ph7_value_int64(pValue,(ph7_int64)time(0));` |
|      9 | 7384 | `	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);` |
|      - | 7385 | `	/* Return the freshly created array */` |
|      9 | 7386 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 7387 | `	return PH7_OK;` |
|      5 | 7388 |  |
|      - | 7389 | `/*` |
|      - | 7390 | ` * mixed gettimeofday([ bool $return_float = false ] )` |
|      - | 7391 | ` *   Returns an associative array containing the data returned from the system call.` |
|      - | 7392 | ` * Parameters` |
|      - | 7393 | ` *  $return_float` |
|      - | 7394 | ` *   When set to TRUE, a float instead of an array is returned.` |
|      - | 7395 | ` * Return` |
|      - | 7396 | ` *   By default an array is returned. If return_float is set, then` |
|      - | 7397 | ` *   a float is returned.` |
|      - | 7398 | ` */` |
|      4 | 7399 | `static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7400 |  |
|      5 | 7401 | `	int bFloat = 0;` |
|      - | 7402 | `	sytime sTime;` |
|      - | 7403 | `#if defined(__UNIXES__)` |
|      - | 7404 | `	struct timeval tv;` |
|      4 | 7405 | `	gettimeofday(&tv,0);` |
|      4 | 7406 | `	sTime.tm_sec  = (long)tv.tv_sec;` |
|      4 | 7407 | `	sTime.tm_usec = (long)tv.tv_usec;` |
|      - | 7408 | `#else` |
|      - | 7409 | `	time_t tt;` |
|      1 | 7410 | `	time(&tt);` |
|      1 | 7411 | `	sTime.tm_sec  = (long)tt;` |
|      1 | 7412 | `	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);` |
|      - | 7413 | `#endif /* __UNIXES__ */` |
|      5 | 7414 | `	if( nArg > 0 ){` |
|      5 | 7415 | `		bFloat = ph7_value_to_bool(apArg[0]);` |
|      2 | 7416 | `	}` |
|      5 | 7417 | `	if( bFloat ){` |
|      - | 7418 | `		/* Return as float */` |
|      3 | 7419 | `		ph7_result_double(pCtx,(double)sTime.tm_sec);` |
|      2 | 7420 | `	}else{` |
|      - | 7421 | `		/* Return an associative array */` |
|      - | 7422 | `		ph7_value *pValue,*pArray;` |
|      - | 7423 | `		/* Create a new array */` |
|      3 | 7424 | `		pArray = ph7_context_new_array(pCtx);` |
|      - | 7425 | `		/* Element value */` |
|      3 | 7426 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 7427 | `		if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7428 | `			/* Return NULL */` |
|    ! 0 | 7429 | `			ph7_result_null(pCtx);` |
|    ! 0 | 7430 | `			return PH7_OK;` |
|      - | 7431 | `		}` |
|      - | 7432 | `		/* Fill the array */` |
|      - | 7433 | `		/* sec */` |
|      3 | 7434 | `		ph7_value_int64(pValue,sTime.tm_sec);` |
|      3 | 7435 | `		ph7_array_add_strkey_elem(pArray,"sec",pValue);` |
|      - | 7436 | `		/* usec */` |
|      3 | 7437 | `		ph7_value_int64(pValue,sTime.tm_usec);` |
|      3 | 7438 | `		ph7_array_add_strkey_elem(pArray,"usec",pValue);` |
|      - | 7439 | `		/* Return the array */` |
|      3 | 7440 | `		ph7_result_value(pCtx,pArray);` |
|      - | 7441 | `	}` |
|      5 | 7442 | `	return PH7_OK;` |
|      3 | 7443 |  |
|      - | 7444 | `/* Check if the given year is leap or not */` |
|      - | 7445 | `#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)` |
|      - | 7446 | `/* ISO-8601 numeric representation of the day of the week */` |
|      - | 7447 | `static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      - | 7448 | `/*` |
|      - | 7449 | ` * Format a given date string.` |
|      - | 7450 | ` * Supported format: (Taken from PHP online docs)` |
|      - | 7451 | ` * character 	Description` |
|      - | 7452 | ` * d          Day of the month` |
|      - | 7453 | ` * D          A textual representation of a days` |
|      - | 7454 | ` * j          Day of the month without leading zeros` |
|      - | 7455 | ` * l          A full textual representation of the day of the week` |
|      - | 7456 | ` * N          ISO-8601 numeric representation of the day of the week` |
|      - | 7457 | ` * w          Numeric representation of the day of the week` |
|      - | 7458 | ` * z          The day of the year (starting from 0)` |
|      - | 7459 | ` * F          A full textual representation of a month, such as January or March` |
|      - | 7460 | ` * m          Numeric representation of a month, with leading zeros 	01 through 12` |
|      - | 7461 | ` * M          A short textual representation of a month, three letters 	Jan through Dec` |
|      - | 7462 | ` * n          Numeric representation of a month, without leading zeros 	1 through 12` |
|      - | 7463 | ` * t          Number of days in the given month 	28 through 31` |
|      - | 7464 | ` * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.` |
|      - | 7465 | ` * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number` |
|      - | 7466 | ` *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003` |
|      - | 7467 | ` * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003` |
|      - | 7468 | ` * y          A two digit representation of a year 	Examples: 99 or 03` |
|      - | 7469 | ` * a          Lowercase Ante meridiem and Post meridiem 	am or pm` |
|      - | 7470 | ` * A          Uppercase Ante meridiem and Post meridiem 	AM or PM` |
|      - | 7471 | ` * g          12-hour format of an hour without leading zeros 	1 through 12` |
|      - | 7472 | ` * G          24-hour format of an hour without leading zeros 	0 through 23` |
|      - | 7473 | ` * h          12-hour format of an hour with leading zeros 	01 through 12` |
|      - | 7474 | ` * H          24-hour format of an hour with leading zeros 	00 through 23` |
|      - | 7475 | ` * i          Minutes with leading zeros 	00 to 59` |
|      - | 7476 | ` * s          Seconds, with leading zeros 	00 through 59` |
|      - | 7477 | ` * u          Microseconds Example: 654321` |
|      - | 7478 | ` * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores` |
|      - | 7479 | ` * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.` |
|      - | 7480 | ` * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200` |
|      - | 7481 | ` * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)` |
|      - | 7482 | ` * S          English ordinal suffix for the day of the month, 2 characters` |
|      - | 7483 | ` * O          Difference to Greenwich time (GMT) in hours` |
|      - | 7484 | ` * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those` |
|      - | 7485 | ` *            east of UTC is always positive.` |
|      - | 7486 | ` * c         ISO 8601 date` |
|      - | 7487 | ` */` |
|     46 | 7488 | `static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)` |
|      1 | 7489 |  |
|     47 | 7490 | `	const char *zEnd = &zIn[nLen];` |
|      - | 7491 | `	const char *zCur;` |
|      - | 7492 | `	/* Start the format process */` |
|     78 | 7493 | `	for(;;){` |
|    157 | 7494 | `		if( zIn >= zEnd ){` |
|      - | 7495 | `			/* No more input to process */` |
|     47 | 7496 | `			break;` |
|      - | 7497 | `		}` |
|    111 | 7498 | `		switch(zIn[0]){` |
|      7 | 7499 | `		case 'd':` |
|      - | 7500 | `			/* Day of the month, 2 digits with leading zeros */` |
|     15 | 7501 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);` |
|     15 | 7502 | `			break;` |
|    ! 0 | 7503 | `		case 'D':` |
|      - | 7504 | `			/*A textual representation of a day, three letters*/` |
|    ! 0 | 7505 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|    ! 0 | 7506 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7507 | `			break;` |
|    ! 0 | 7508 | `		case 'j':` |
|      - | 7509 | `			/*	Day of the month without leading zeros */` |
|    ! 0 | 7510 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);` |
|    ! 0 | 7511 | `			break;` |
|      2 | 7512 | `		case 'l':` |
|      - | 7513 | `			/* A full textual representation of the day of the week */` |
|      5 | 7514 | `			zCur = SyTimeGetDay(pTm->tm_wday);` |
|      5 | 7515 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7516 | `			break;` |
|    ! 0 | 7517 | `		case 'N':{` |
|      - | 7518 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7519 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7520 | `			break;` |
|      - | 7521 | `				 }` |
|    ! 0 | 7522 | `		case 'w':` |
|      - | 7523 | `			/*Numeric representation of the day of the week*/` |
|    ! 0 | 7524 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7525 | `			break;` |
|    ! 0 | 7526 | `		case 'z':` |
|      - | 7527 | `			/*The day of the year*/` |
|    ! 0 | 7528 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);` |
|    ! 0 | 7529 | `			break;` |
|      2 | 7530 | `		case 'F':` |
|      - | 7531 | `			/*A full textual representation of a month, such as January or March*/` |
|      5 | 7532 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|      5 | 7533 | `			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);` |
|      5 | 7534 | `			break;` |
|      7 | 7535 | `		case 'm':` |
|      - | 7536 | `			/*Numeric representation of a month, with leading zeros*/` |
|     15 | 7537 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|     15 | 7538 | `			break;` |
|    ! 0 | 7539 | `		case 'M':` |
|      - | 7540 | `			/*A short textual representation of a month, three letters*/` |
|    ! 0 | 7541 | `			zCur = SyTimeGetMonth(pTm->tm_mon);` |
|    ! 0 | 7542 | `			ph7_result_string(pCtx,zCur,3);` |
|    ! 0 | 7543 | `			break;` |
|    ! 0 | 7544 | `		case 'n':` |
|      - | 7545 | `			/*Numeric representation of a month, without leading zeros*/` |
|    ! 0 | 7546 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);` |
|    ! 0 | 7547 | `			break;` |
|    ! 0 | 7548 | `		case 't':{` |
|      - | 7549 | `			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|    ! 0 | 7550 | `			int nDays = aMonDays[pTm->tm_mon % 12 ];` |
|    ! 0 | 7551 | `			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){` |
|    ! 0 | 7552 | `				nDays = 28;` |
|    ! 0 | 7553 | `			}` |
|      - | 7554 | `			/*Number of days in the given month*/` |
|    ! 0 | 7555 | `			ph7_result_string_format(pCtx,"%d",nDays);` |
|    ! 0 | 7556 | `			break;` |
|      - | 7557 | `				 }` |
|    ! 0 | 7558 | `		case 'L':{` |
|    ! 0 | 7559 | `			int isLeap = IS_LEAP_YEAR(pTm->tm_year);` |
|      - | 7560 | `			/* Whether it's a leap year */` |
|    ! 0 | 7561 | `			ph7_result_string_format(pCtx,"%d",isLeap);` |
|    ! 0 | 7562 | `			break;` |
|      - | 7563 | `				 }` |
|    ! 0 | 7564 | `		case 'o':` |
|      - | 7565 | `			/* ISO-8601 year number.*/` |
|    ! 0 | 7566 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|    ! 0 | 7567 | `			break;` |
|      9 | 7568 | `		case 'Y':` |
|      - | 7569 | `			/*	A full numeric representation of a year, 4 digits */` |
|     19 | 7570 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|     19 | 7571 | `			break;` |
|    ! 0 | 7572 | `		case 'y':` |
|      - | 7573 | `			/*A two digit representation of a year*/` |
|    ! 0 | 7574 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);` |
|    ! 0 | 7575 | `			break;` |
|    ! 0 | 7576 | `		case 'a':` |
|      - | 7577 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7578 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);` |
|    ! 0 | 7579 | `			break;` |
|    ! 0 | 7580 | `		case 'A':` |
|      - | 7581 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7582 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);` |
|    ! 0 | 7583 | `			break;` |
|    ! 0 | 7584 | `		case 'g':` |
|      - | 7585 | `			/*	12-hour format of an hour without leading zeros*/` |
|    ! 0 | 7586 | `			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7587 | `			break;` |
|    ! 0 | 7588 | `		case 'G':` |
|      - | 7589 | `			/* 24-hour format of an hour without leading zeros */` |
|    ! 0 | 7590 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);` |
|    ! 0 | 7591 | `			break;` |
|    ! 0 | 7592 | `		case 'h':` |
|      - | 7593 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7594 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7595 | `			break;` |
|      3 | 7596 | `		case 'H':` |
|      - | 7597 | `			/*	24-hour format of an hour with leading zeros */` |
|      7 | 7598 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      7 | 7599 | `			break;` |
|      3 | 7600 | `		case 'i':` |
|      - | 7601 | `			/* 	Minutes with leading zeros */` |
|      7 | 7602 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      7 | 7603 | `			break;` |
|      3 | 7604 | `		case 's':` |
|      - | 7605 | `			/* 	second with leading zeros */` |
|      7 | 7606 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|      7 | 7607 | `			break;` |
|    ! 0 | 7608 | `		case 'u':` |
|      - | 7609 | `			/* 	Microseconds */` |
|    ! 0 | 7610 | `			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);` |
|    ! 0 | 7611 | `			break;` |
|    ! 0 | 7612 | `		case 'S':{` |
|      - | 7613 | `			/* English ordinal suffix for the day of the month, 2 characters */` |
|      - | 7614 | `			static const char zSuffix[] = "thstndrdthththththth";` |
|    ! 0 | 7615 | `			int v = pTm->tm_mday;` |
|    ! 0 | 7616 | `			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);` |
|    ! 0 | 7617 | `			break;` |
|      - | 7618 | `				 }` |
|    ! 0 | 7619 | `		case 'e':` |
|      - | 7620 | `			/* 	Timezone identifier */` |
|    ! 0 | 7621 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7622 | `			if( zCur == 0 ){` |
|      - | 7623 | `				/* Assume GMT */` |
|    ! 0 | 7624 | `				zCur = "GMT";` |
|    ! 0 | 7625 | `			}` |
|    ! 0 | 7626 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7627 | `			break;` |
|    ! 0 | 7628 | `		case 'I':` |
|      - | 7629 | `			/* Whether or not the date is in daylight saving time */` |
|      - | 7630 | `#ifdef __WINNT__` |
|      - | 7631 | `#ifdef _MSC_VER` |
|      - | 7632 | `#ifndef _WIN32_WCE` |
|    ! 0 | 7633 | `			_get_daylight(&pTm->tm_isdst);` |
|      - | 7634 | `#endif` |
|      - | 7635 | `#endif` |
|      - | 7636 | `#endif` |
|    ! 0 | 7637 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);` |
|    ! 0 | 7638 | `			break;` |
|    ! 0 | 7639 | `		case 'r':` |
|      - | 7640 | `			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */` |
|    ! 0 | 7641 | `			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",` |
|    ! 0 | 7642 | `				SyTimeGetDay(pTm->tm_wday),` |
|    ! 0 | 7643 | `				pTm->tm_mday,` |
|    ! 0 | 7644 | `				SyTimeGetMonth(pTm->tm_mon),` |
|    ! 0 | 7645 | `				pTm->tm_year,` |
|    ! 0 | 7646 | `				pTm->tm_hour,` |
|    ! 0 | 7647 | `				pTm->tm_min,` |
|    ! 0 | 7648 | `				pTm->tm_sec` |
|      - | 7649 | `				);` |
|    ! 0 | 7650 | `			break;` |
|    ! 0 | 7651 | `		case 'U':{` |
|      - | 7652 | `			time_t tt;` |
|      - | 7653 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7654 | `			time(&tt);` |
|    ! 0 | 7655 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7656 | `			break;` |
|      - | 7657 | `				 }` |
|    ! 0 | 7658 | `		case 'O':` |
|      - | 7659 | `		case 'P':` |
|      - | 7660 | `			/* Difference to Greenwich time (GMT) in hours */` |
|    ! 0 | 7661 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7662 | `			break;` |
|    ! 0 | 7663 | `		case 'Z':` |
|      - | 7664 | `			/* Timezone offset in seconds. The offset for timezones west of UTC` |
|      - | 7665 | `			 * is always negative, and for those east of UTC is always positive.` |
|      - | 7666 | `			 */` |
|    ! 0 | 7667 | `			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);` |
|    ! 0 | 7668 | `			break;` |
|      1 | 7669 | `		case 'c':` |
|      - | 7670 | `			/* 	ISO 8601 date */` |
|      4 | 7671 | `			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",` |
|      1 | 7672 | `				pTm->tm_year,` |
|      2 | 7673 | `				pTm->tm_mon+1,` |
|      1 | 7674 | `				pTm->tm_mday,` |
|      1 | 7675 | `				pTm->tm_hour,` |
|      1 | 7676 | `				pTm->tm_min,` |
|      1 | 7677 | `				pTm->tm_sec,` |
|      1 | 7678 | `				pTm->tm_gmtoff` |
|      - | 7679 | `				);` |
|      3 | 7680 | `			break;` |
|      1 | 7681 | `		case '\\':` |
|      3 | 7682 | `			zIn++;` |
|      - | 7683 | `			/* Expand verbatim */` |
|      3 | 7684 | `			if( zIn < zEnd ){` |
|      3 | 7685 | `				ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|      1 | 7686 | `			}` |
|      3 | 7687 | `			break;` |
|     17 | 7688 | `		default:` |
|      - | 7689 | `			/* Unknown format specifer,expand verbatim */` |
|     35 | 7690 | `			ph7_result_string(pCtx,zIn,(int)sizeof(char));` |
|     34 | 7691 | `			break;` |
|      - | 7692 | `		}` |
|      - | 7693 | `		/* Point to the next character */` |
|    111 | 7694 | `		zIn++;` |
|      1 | 7695 | `	}` |
|     47 | 7696 | `	return SXRET_OK;` |
|      1 | 7697 |  |
|      - | 7698 | `/*` |
|      - | 7699 | ` * PH7 implementation of the strftime() function.` |
|      - | 7700 | ` * The following formats are supported:` |
|      - | 7701 | ` * %a 	An abbreviated textual representation of the day` |
|      - | 7702 | ` * %A 	A full textual representation of the day` |
|      - | 7703 | ` * %d 	Two-digit day of the month (with leading zeros)` |
|      - | 7704 | ` * %e 	Day of the month, with a space preceding single digits.` |
|      - | 7705 | ` * %j 	Day of the year, 3 digits with leading zeros` |
|      - | 7706 | ` * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)` |
|      - | 7707 | ` * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)` |
|      - | 7708 | ` * %U 	Week number of the given year, starting with the first Sunday as the first week` |
|      - | 7709 | ` * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least` |
|      - | 7710 | ` *   4 weekdays, with Monday being the start of the week.` |
|      - | 7711 | ` * %W 	A numeric representation of the week of the year` |
|      - | 7712 | ` * %b 	Abbreviated month name, based on the locale` |
|      - | 7713 | ` * %B 	Full month name, based on the locale` |
|      - | 7714 | ` * %h 	Abbreviated month name, based on the locale (an alias of %b)` |
|      - | 7715 | ` * %m 	Two digit representation of the month` |
|      - | 7716 | ` * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)` |
|      - | 7717 | ` * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)` |
|      - | 7718 | ` * %G 	The full four-digit version of %g` |
|      - | 7719 | ` * %y 	Two digit representation of the year` |
|      - | 7720 | ` * %Y 	Four digit representation for the year` |
|      - | 7721 | ` * %H 	Two digit representation of the hour in 24-hour format` |
|      - | 7722 | ` * %I 	Two digit representation of the hour in 12-hour format` |
|      - | 7723 | ` * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits` |
|      - | 7724 | ` * %M 	Two digit representation of the minute` |
|      - | 7725 | ` * %p 	UPPER-CASE 'AM' or 'PM' based on the given time` |
|      - | 7726 | ` * %P 	lower-case 'am' or 'pm' based on the given time` |
|      - | 7727 | ` * %r 	Same as "%I:%M:%S %p"` |
|      - | 7728 | ` * %R 	Same as "%H:%M"` |
|      - | 7729 | ` * %S 	Two digit representation of the second` |
|      - | 7730 | ` * %T 	Same as "%H:%M:%S"` |
|      - | 7731 | ` * %X 	Preferred time representation based on locale, without the date` |
|      - | 7732 | ` * %z 	Either the time zone offset from UTC or the abbreviation` |
|      - | 7733 | ` * %Z 	The time zone offset/abbreviation option NOT given by %z` |
|      - | 7734 | ` * %c 	Preferred date and time stamp based on local` |
|      - | 7735 | ` * %D 	Same as "%m/%d/%y"` |
|      - | 7736 | ` * %F 	Same as "%Y-%m-%d"` |
|      - | 7737 | ` * %s 	Unix Epoch Time timestamp (same as the time() function)` |
|      - | 7738 | ` * %x 	Preferred date representation based on locale, without the time` |
|      - | 7739 | ` * %n 	A newline character ("\n")` |
|      - | 7740 | ` * %t 	A Tab character ("\t")` |
|      - | 7741 | ` * %% 	A literal percentage character ("%")` |
|      - | 7742 | ` */` |
|     16 | 7743 | `static int PH7_Strftime(` |
|      - | 7744 | `	ph7_context *pCtx,  /* Call context */` |
|      - | 7745 | `	const char *zIn,    /* Input string */` |
|      - | 7746 | `	int nLen,           /* Input length */` |
|      - | 7747 | `	Sytm *pTm           /* Parse of the given time */` |
|      - | 7748 | `	)` |
|      1 | 7749 |  |
|     17 | 7750 | `	const char *zCur,*zEnd = &zIn[nLen];` |
|      - | 7751 | `	int c;` |
|      - | 7752 | `	/* Start the format process */` |
|     18 | 7753 | `	for(;;){` |
|     37 | 7754 | `		zCur = zIn;` |
|     41 | 7755 | `		while(zIn < zEnd && zIn[0] != '%' ){` |
|      5 | 7756 | `			zIn++;` |
|      1 | 7757 | `		}` |
|     37 | 7758 | `		if( zIn > zCur ){` |
|      - | 7759 | `			/* Consume input verbatim */` |
|      5 | 7760 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 7761 | `		}` |
|     37 | 7762 | `		zIn++; /* Jump the percent sign */` |
|     37 | 7763 | `		if( zIn >= zEnd ){` |
|      - | 7764 | `			/* No more input to process */` |
|     17 | 7765 | `			break;` |
|      - | 7766 | `		}` |
|     21 | 7767 | `		c = zIn[0];` |
|      - | 7768 | `		/* Act according to the current specifer */` |
|     21 | 7769 | `		switch(c){` |
|    ! 0 | 7770 | `		case '%':` |
|      - | 7771 | `			/* A literal percentage character ("%") */` |
|    ! 0 | 7772 | `			ph7_result_string(pCtx,"%",(int)sizeof(char));` |
|    ! 0 | 7773 | `			break;` |
|    ! 0 | 7774 | `		case 't':` |
|      - | 7775 | `			/* A Tab character */` |
|    ! 0 | 7776 | `			ph7_result_string(pCtx,"\t",(int)sizeof(char));` |
|    ! 0 | 7777 | `			break;` |
|    ! 0 | 7778 | `		case 'n':` |
|      - | 7779 | `			/* A newline character */` |
|    ! 0 | 7780 | `			ph7_result_string(pCtx,"\n",(int)sizeof(char));` |
|    ! 0 | 7781 | `			break;` |
|      1 | 7782 | `		case 'a':` |
|      - | 7783 | `			/* An abbreviated textual representation of the day */` |
|      3 | 7784 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);` |
|      3 | 7785 | `			break;` |
|    ! 0 | 7786 | `		case 'A':` |
|      - | 7787 | `			/* A full textual representation of the day */` |
|    ! 0 | 7788 | `			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);` |
|    ! 0 | 7789 | `			break;` |
|    ! 0 | 7790 | `		case 'e':` |
|      - | 7791 | `			/* Day of the month, 2 digits with leading space for single digit*/` |
|    ! 0 | 7792 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);` |
|    ! 0 | 7793 | `			break;` |
|      2 | 7794 | `		case 'd':` |
|      - | 7795 | `			/* Two-digit day of the month (with leading zeros) */` |
|      5 | 7796 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);` |
|      5 | 7797 | `			break;` |
|    ! 0 | 7798 | `		case 'j':` |
|      - | 7799 | `			/*The day of the year,3 digits with leading zeros*/` |
|    ! 0 | 7800 | `			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);` |
|    ! 0 | 7801 | `			break;` |
|    ! 0 | 7802 | `		case 'u':` |
|      - | 7803 | `			/* ISO-8601 numeric representation of the day of the week */` |
|    ! 0 | 7804 | `			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);` |
|    ! 0 | 7805 | `			break;` |
|    ! 0 | 7806 | `		case 'w':` |
|      - | 7807 | `			/* Numeric representation of the day of the week */` |
|    ! 0 | 7808 | `			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);` |
|    ! 0 | 7809 | `			break;` |
|    ! 0 | 7810 | `		case 'b':` |
|      - | 7811 | `		case 'h':` |
|      - | 7812 | `			/*A short textual representation of a month, three letters (Not based on locale)*/` |
|    ! 0 | 7813 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);` |
|    ! 0 | 7814 | `			break;` |
|    ! 0 | 7815 | `		case 'B':` |
|      - | 7816 | `			/* Full month name (Not based on locale) */` |
|    ! 0 | 7817 | `			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);` |
|    ! 0 | 7818 | `			break;` |
|      2 | 7819 | `		case 'm':` |
|      - | 7820 | `			/*Numeric representation of a month, with leading zeros*/` |
|      5 | 7821 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);` |
|      5 | 7822 | `			break;` |
|    ! 0 | 7823 | `		case 'C':` |
|      - | 7824 | `			/* Two digit representation of the century */` |
|    ! 0 | 7825 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);` |
|    ! 0 | 7826 | `			break;` |
|    ! 0 | 7827 | `		case 'y':` |
|      - | 7828 | `		case 'g':` |
|      - | 7829 | `			/* Two digit representation of the year */` |
|    ! 0 | 7830 | `			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);` |
|    ! 0 | 7831 | `			break;` |
|      2 | 7832 | `		case 'Y':` |
|      - | 7833 | `		case 'G':` |
|      - | 7834 | `			/* Four digit representation of the year */` |
|      5 | 7835 | `			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);` |
|      5 | 7836 | `			break;` |
|    ! 0 | 7837 | `		case 'I':` |
|      - | 7838 | `			/* 12-hour format of an hour with leading zeros */` |
|    ! 0 | 7839 | `			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7840 | `			break;` |
|    ! 0 | 7841 | `		case 'l':` |
|      - | 7842 | `			/* 12-hour format of an hour with leading space */` |
|    ! 0 | 7843 | `			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));` |
|    ! 0 | 7844 | `			break;` |
|      1 | 7845 | `		case 'H':` |
|      - | 7846 | `			/* 24-hour format of an hour with leading zeros */` |
|      3 | 7847 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);` |
|      3 | 7848 | `			break;` |
|      1 | 7849 | `		case 'M':` |
|      - | 7850 | `			/* Minutes with leading zeros */` |
|      3 | 7851 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);` |
|      3 | 7852 | `			break;` |
|    ! 0 | 7853 | `		case 'S':` |
|      - | 7854 | `			/* Seconds with leading zeros */` |
|    ! 0 | 7855 | `			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);` |
|    ! 0 | 7856 | `			break;` |
|    ! 0 | 7857 | `		case 'z':` |
|      - | 7858 | `		case 'Z':` |
|      - | 7859 | `			/* 	Timezone identifier */` |
|    ! 0 | 7860 | `			zCur = pTm->tm_zone;` |
|    ! 0 | 7861 | `			if( zCur == 0 ){` |
|      - | 7862 | `				/* Assume GMT */` |
|    ! 0 | 7863 | `				zCur = "GMT";` |
|    ! 0 | 7864 | `			}` |
|    ! 0 | 7865 | `			ph7_result_string(pCtx,zCur,-1);` |
|    ! 0 | 7866 | `			break;` |
|    ! 0 | 7867 | `		case 'T':` |
|      - | 7868 | `		case 'X':` |
|      - | 7869 | `			/* Same as "%H:%M:%S" */` |
|    ! 0 | 7870 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);` |
|    ! 0 | 7871 | `			break;` |
|    ! 0 | 7872 | `		case 'R':` |
|      - | 7873 | `			/* Same as "%H:%M" */` |
|    ! 0 | 7874 | `			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);` |
|    ! 0 | 7875 | `			break;` |
|    ! 0 | 7876 | `		case 'P':` |
|      - | 7877 | `			/*	Lowercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7878 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);` |
|    ! 0 | 7879 | `			break;` |
|    ! 0 | 7880 | `		case 'p':` |
|      - | 7881 | `			/*	Uppercase Ante meridiem and Post meridiem */` |
|    ! 0 | 7882 | `			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);` |
|    ! 0 | 7883 | `			break;` |
|    ! 0 | 7884 | `		case 'r':` |
|      - | 7885 | `			/* Same as "%I:%M:%S %p" */` |
|    ! 0 | 7886 | `			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",` |
|    ! 0 | 7887 | `				1+(pTm->tm_hour%12),` |
|    ! 0 | 7888 | `				pTm->tm_min,` |
|    ! 0 | 7889 | `				pTm->tm_sec,` |
|    ! 0 | 7890 | `				pTm->tm_hour > 12 ? "PM" : "AM"` |
|      - | 7891 | `				);` |
|    ! 0 | 7892 | `			break;` |
|      1 | 7893 | `		case 'D':` |
|      - | 7894 | `		case 'x':` |
|      - | 7895 | `			/* Same as "%m/%d/%y" */` |
|      4 | 7896 | `			ph7_result_string_format(pCtx,"%02d/%02d/%02d",` |
|      2 | 7897 | `				pTm->tm_mon+1,` |
|      1 | 7898 | `				pTm->tm_mday,` |
|      2 | 7899 | `				pTm->tm_year%100` |
|      - | 7900 | `				);` |
|      3 | 7901 | `			break;` |
|    ! 0 | 7902 | `		case 'F':` |
|      - | 7903 | `			/* Same as "%Y-%m-%d" */` |
|    ! 0 | 7904 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d",` |
|    ! 0 | 7905 | `				pTm->tm_year,` |
|    ! 0 | 7906 | `				pTm->tm_mon+1,` |
|    ! 0 | 7907 | `				pTm->tm_mday` |
|      - | 7908 | `				);` |
|    ! 0 | 7909 | `			break;` |
|    ! 0 | 7910 | `		case 'c':` |
|    ! 0 | 7911 | `			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",` |
|    ! 0 | 7912 | `				pTm->tm_year,` |
|    ! 0 | 7913 | `				pTm->tm_mon+1,` |
|    ! 0 | 7914 | `				pTm->tm_mday,` |
|    ! 0 | 7915 | `				pTm->tm_hour,` |
|    ! 0 | 7916 | `				pTm->tm_min,` |
|    ! 0 | 7917 | `				pTm->tm_sec` |
|      - | 7918 | `				);` |
|    ! 0 | 7919 | `			break;` |
|    ! 0 | 7920 | `		case 's':{` |
|      - | 7921 | `			time_t tt;` |
|      - | 7922 | `			/* Seconds since the Unix Epoch */` |
|    ! 0 | 7923 | `			time(&tt);` |
|    ! 0 | 7924 | `			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);` |
|    ! 0 | 7925 | `			break;` |
|      - | 7926 | `				 }` |
|    ! 0 | 7927 | `		default:` |
|      - | 7928 | `			/* unknown specifer,simply ignore*/` |
|    ! 0 | 7929 | `			break;` |
|      - | 7930 | `		}` |
|      - | 7931 | `		/* Advance the cursor */` |
|     21 | 7932 | `		zIn++;` |
|      1 | 7933 | `	}` |
|     17 | 7934 | `	return SXRET_OK;` |
|      1 | 7935 |  |
|      - | 7936 | `/*` |
|      - | 7937 | ` * string date(string $format [, int $timestamp = time() ] )` |
|      - | 7938 | ` *  Returns a string formatted according to the given format string using` |
|      - | 7939 | ` *  the given integer timestamp or the current time if no timestamp is given.` |
|      - | 7940 | ` *  In other words, timestamp is optional and defaults to the value of time().` |
|      - | 7941 | ` * Parameters` |
|      - | 7942 | ` *  $format` |
|      - | 7943 | ` *   The format of the outputted date string (See code above)` |
|      - | 7944 | ` * $timestamp` |
|      - | 7945 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 7946 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 7947 | ` *   In other words, it defaults to the value of time().` |
|      - | 7948 | ` * Return` |
|      - | 7949 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 7950 | ` */` |
|     36 | 7951 | `static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7952 |  |
|      - | 7953 | `	const char *zFormat;` |
|      - | 7954 | `	int nLen;` |
|      - | 7955 | `	Sytm sTm;` |
|     37 | 7956 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7957 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 7958 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7959 | `		return PH7_OK;` |
|      - | 7960 | `	}` |
|     33 | 7961 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 7962 | `	if( nLen < 1 ){` |
|      - | 7963 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 7964 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7965 | `	}` |
|     33 | 7966 | `	if( nArg < 2 ){` |
|      - | 7967 | `#ifdef __WINNT__` |
|      - | 7968 | `		SYSTEMTIME sOS;` |
|      1 | 7969 | `		GetSystemTime(&sOS);` |
|      1 | 7970 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 7971 | `#else` |
|      - | 7972 | `		struct tm *pTm;` |
|      - | 7973 | `		time_t t;` |
|     30 | 7974 | `		time(&t);` |
|     30 | 7975 | `		pTm = localtime(&t);` |
|     30 | 7976 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7977 | `#endif` |
|     16 | 7978 | `	}else{` |
|      - | 7979 | `		/* Use the given timestamp */` |
|      - | 7980 | `		time_t t;` |
|      - | 7981 | `		struct tm *pTm;` |
|      3 | 7982 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 7983 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 7984 | `			pTm = localtime(&t);` |
|      3 | 7985 | `			if( pTm == 0 ){` |
|    ! 0 | 7986 | `				time(&t);` |
|    ! 0 | 7987 | `			}` |
|      2 | 7988 | `		}else{` |
|    ! 0 | 7989 | `			time(&t);` |
|      - | 7990 | `		}` |
|      3 | 7991 | `		pTm = localtime(&t);` |
|      3 | 7992 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 7993 | `	}` |
|      - | 7994 | `	/* Format the given string */` |
|     33 | 7995 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     33 | 7996 | `	return PH7_OK;` |
|     19 | 7997 |  |
|      - | 7998 | `/*` |
|      - | 7999 | ` * string strftime(string $format [, int $timestamp = time() ] )` |
|      - | 8000 | ` *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)` |
|      - | 8001 | ` * Parameters` |
|      - | 8002 | ` *  $format` |
|      - | 8003 | ` *   The format of the outputted date string (See code above)` |
|      - | 8004 | ` * $timestamp` |
|      - | 8005 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8006 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8007 | ` *   In other words, it defaults to the value of time().` |
|      - | 8008 | ` * Return` |
|      - | 8009 | ` * Returns a string formatted according format using the given timestamp` |
|      - | 8010 | ` * or the current local time if no timestamp is given.` |
|      - | 8011 | ` */` |
|     20 | 8012 | `static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8013 |  |
|      - | 8014 | `	const char *zFormat;` |
|      - | 8015 | `	int nLen;` |
|      - | 8016 | `	Sytm sTm;` |
|     21 | 8017 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8018 | `		/* Missing/Invalid argument,return FALSE */` |
|      5 | 8019 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8020 | `		return PH7_OK;` |
|      - | 8021 | `	}` |
|     17 | 8022 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8023 | `	if( nLen < 1 ){` |
|      - | 8024 | `		/* Don't bother processing return FALSE */` |
|    ! 0 | 8025 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8026 | `	}` |
|     17 | 8027 | `	if( nArg < 2 ){` |
|      - | 8028 | `#ifdef __WINNT__` |
|      - | 8029 | `		SYSTEMTIME sOS;` |
|      1 | 8030 | `		GetSystemTime(&sOS);` |
|      1 | 8031 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8032 | `#else` |
|      - | 8033 | `		struct tm *pTm;` |
|      - | 8034 | `		time_t t;` |
|     14 | 8035 | `		time(&t);` |
|     14 | 8036 | `		pTm = localtime(&t);` |
|     14 | 8037 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8038 | `#endif` |
|      8 | 8039 | `	}else{` |
|      - | 8040 | `		/* Use the given timestamp */` |
|      - | 8041 | `		time_t t;` |
|      - | 8042 | `		struct tm *pTm;` |
|      3 | 8043 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8044 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8045 | `			pTm = localtime(&t);` |
|      3 | 8046 | `			if( pTm == 0 ){` |
|    ! 0 | 8047 | `				time(&t);` |
|    ! 0 | 8048 | `			}` |
|      2 | 8049 | `		}else{` |
|    ! 0 | 8050 | `			time(&t);` |
|      - | 8051 | `		}` |
|      3 | 8052 | `		pTm = localtime(&t);` |
|      3 | 8053 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8054 | `	}` |
|      - | 8055 | `	/* Format the given string */` |
|     17 | 8056 | `	PH7_Strftime(pCtx,zFormat,nLen,&sTm);` |
|     17 | 8057 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|      - | 8058 | `		/* Nothing was formatted,return FALSE */` |
|    ! 0 | 8059 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8060 | `	}` |
|     17 | 8061 | `	return PH7_OK;` |
|     11 | 8062 |  |
|      - | 8063 | `/*` |
|      - | 8064 | ` * string gmdate(string $format [, int $timestamp = time() ] )` |
|      - | 8065 | ` *  Identical to the date() function except that the time returned` |
|      - | 8066 | ` *  is Greenwich Mean Time (GMT).` |
|      - | 8067 | ` * Parameters` |
|      - | 8068 | ` *  $format` |
|      - | 8069 | ` *  The format of the outputted date string (See code above)` |
|      - | 8070 | ` *  $timestamp` |
|      - | 8071 | ` *   The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8072 | ` *   that defaults to the current local time if a timestamp is not given.` |
|      - | 8073 | ` *   In other words, it defaults to the value of time().` |
|      - | 8074 | ` * Return` |
|      - | 8075 | ` *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.` |
|      - | 8076 | ` */` |
|     16 | 8077 | `static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8078 |  |
|      - | 8079 | `	const char *zFormat;` |
|      - | 8080 | `	int nLen;` |
|      - | 8081 | `	Sytm sTm;` |
|     17 | 8082 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8083 | `		/* Missing/Invalid argument,return FALSE */` |
|      3 | 8084 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8085 | `		return PH7_OK;` |
|      - | 8086 | `	}` |
|     15 | 8087 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8088 | `	if( nLen < 1 ){` |
|      - | 8089 | `		/* Don't bother processing return the empty string */` |
|    ! 0 | 8090 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8091 | `	}` |
|     15 | 8092 | `	if( nArg < 2 ){` |
|      - | 8093 | `#ifdef __WINNT__` |
|      - | 8094 | `		SYSTEMTIME sOS;` |
|      1 | 8095 | `		GetSystemTime(&sOS);` |
|      1 | 8096 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8097 | `#else` |
|      - | 8098 | `		struct tm *pTm;` |
|      - | 8099 | `		time_t t;` |
|     12 | 8100 | `		time(&t);` |
|     12 | 8101 | `		pTm = gmtime(&t);` |
|     12 | 8102 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8103 | `#endif` |
|      7 | 8104 | `	}else{` |
|      - | 8105 | `		/* Use the given timestamp */` |
|      - | 8106 | `		time_t t;` |
|      - | 8107 | `		struct tm *pTm;` |
|      3 | 8108 | `		if( ph7_value_is_int(apArg[1]) ){` |
|      3 | 8109 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|      3 | 8110 | `			pTm = gmtime(&t);` |
|      3 | 8111 | `			if( pTm == 0 ){` |
|    ! 0 | 8112 | `				time(&t);` |
|    ! 0 | 8113 | `			}` |
|      2 | 8114 | `		}else{` |
|    ! 0 | 8115 | `			time(&t);` |
|      - | 8116 | `		}` |
|      3 | 8117 | `		pTm = gmtime(&t);` |
|      3 | 8118 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8119 | `	}` |
|      - | 8120 | `	/* Format the given string */` |
|     15 | 8121 | `	DateFormat(pCtx,zFormat,nLen,&sTm);` |
|     15 | 8122 | `	return PH7_OK;` |
|      9 | 8123 |  |
|      - | 8124 | `/*` |
|      - | 8125 | ` * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])` |
|      - | 8126 | ` *  Return the local time.` |
|      - | 8127 | ` * Parameter` |
|      - | 8128 | ` *  $timestamp: The optional timestamp parameter is an integer Unix timestamp` |
|      - | 8129 | ` *     that defaults to the current local time if a timestamp is not given.` |
|      - | 8130 | ` *     In other words, it defaults to the value of time().` |
|      - | 8131 | ` * $is_associative` |
|      - | 8132 | ` *   If set to FALSE or not supplied then the array is returned as a regular, numerically` |
|      - | 8133 | ` *   indexed array. If the argument is set to TRUE then localtime() returns an associative` |
|      - | 8134 | ` *   array containing all the different elements of the structure returned by the C function` |
|      - | 8135 | ` *   call to localtime. The names of the different keys of the associative array are as follows:` |
|      - | 8136 | ` *      "tm_sec" - seconds, 0 to 59` |
|      - | 8137 | ` *      "tm_min" - minutes, 0 to 59` |
|      - | 8138 | ` *      "tm_hour" - hours, 0 to 23` |
|      - | 8139 | ` *      "tm_mday" - day of the month, 1 to 31` |
|      - | 8140 | ` *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)` |
|      - | 8141 | ` *      "tm_year" - years since 1900` |
|      - | 8142 | ` *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)` |
|      - | 8143 | ` *      "tm_yday" - day of the year, 0 to 365` |
|      - | 8144 | ` *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.` |
|      - | 8145 | ` * Returns` |
|      - | 8146 | ` *  An associative array of information related to the timestamp.` |
|      - | 8147 | ` */` |
|      8 | 8148 | `static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8149 |  |
|      - | 8150 | `	ph7_value *pValue,*pArray;` |
|      9 | 8151 | `	int isAssoc = 0;` |
|      - | 8152 | `	Sytm sTm;` |
|      9 | 8153 | `	if( nArg < 1 ){` |
|      - | 8154 | `#ifdef __WINNT__` |
|      - | 8155 | `		SYSTEMTIME sOS;` |
|      1 | 8156 | `		GetSystemTime(&sOS); /* TODO(chems): GMT not local */` |
|      1 | 8157 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8158 | `#else` |
|      - | 8159 | `		struct tm *pTm;` |
|      - | 8160 | `		time_t t;` |
|      4 | 8161 | `		time(&t);` |
|      4 | 8162 | `		pTm = localtime(&t);` |
|      4 | 8163 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8164 | `#endif` |
|      3 | 8165 | `	}else{` |
|      - | 8166 | `		/* Use the given timestamp */` |
|      - | 8167 | `		time_t t;` |
|      - | 8168 | `		struct tm *pTm;` |
|      5 | 8169 | `		if( ph7_value_is_int(apArg[0]) ){` |
|      5 | 8170 | `			t = (time_t)ph7_value_to_int64(apArg[0]);` |
|      5 | 8171 | `			pTm = localtime(&t);` |
|      5 | 8172 | `			if( pTm == 0 ){` |
|    ! 0 | 8173 | `				time(&t);` |
|    ! 0 | 8174 | `			}` |
|      3 | 8175 | `		}else{` |
|    ! 0 | 8176 | `			time(&t);` |
|      - | 8177 | `		}` |
|      5 | 8178 | `		pTm = localtime(&t);` |
|      5 | 8179 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8180 | `	}` |
|      - | 8181 | `	/* Element value */` |
|      9 | 8182 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      9 | 8183 | `	if( pValue == 0 ){` |
|      - | 8184 | `		/* Return NULL */` |
|    ! 0 | 8185 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8186 | `		return PH7_OK;` |
|      - | 8187 | `	}` |
|      - | 8188 | `	/* Create a new array */` |
|      9 | 8189 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 8190 | `	if( pArray == 0 ){` |
|      - | 8191 | `		/* Return NULL */` |
|    ! 0 | 8192 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8193 | `		return PH7_OK;` |
|      - | 8194 | `	}` |
|      9 | 8195 | `	if( nArg > 1 ){` |
|      3 | 8196 | `		isAssoc = ph7_value_to_bool(apArg[1]);` |
|      1 | 8197 | `	}` |
|      - | 8198 | `	/* Fill the array */` |
|      - | 8199 | `	/* Seconds */` |
|      9 | 8200 | `	ph7_value_int(pValue,sTm.tm_sec);` |
|      9 | 8201 | `	if( isAssoc ){` |
|      3 | 8202 | `		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);` |
|      2 | 8203 | `	}else{` |
|      7 | 8204 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8205 | `	}` |
|      - | 8206 | `	/* Minutes */` |
|      9 | 8207 | `	ph7_value_int(pValue,sTm.tm_min);` |
|      9 | 8208 | `	if( isAssoc ){` |
|      3 | 8209 | `		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);` |
|      2 | 8210 | `	}else{` |
|      7 | 8211 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8212 | `	}` |
|      - | 8213 | `	/* Hours */` |
|      9 | 8214 | `	ph7_value_int(pValue,sTm.tm_hour);` |
|      9 | 8215 | `	if( isAssoc ){` |
|      3 | 8216 | `		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);` |
|      2 | 8217 | `	}else{` |
|      7 | 8218 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8219 | `	}` |
|      - | 8220 | `	/* mday */` |
|      9 | 8221 | `	ph7_value_int(pValue,sTm.tm_mday);` |
|      9 | 8222 | `	if( isAssoc ){` |
|      3 | 8223 | `		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);` |
|      2 | 8224 | `	}else{` |
|      7 | 8225 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8226 | `	}` |
|      - | 8227 | `	/* mon */` |
|      9 | 8228 | `	ph7_value_int(pValue,sTm.tm_mon);` |
|      9 | 8229 | `	if( isAssoc ){` |
|      3 | 8230 | `		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);` |
|      2 | 8231 | `	}else{` |
|      7 | 8232 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8233 | `	}` |
|      - | 8234 | `	/* year since 1900 */` |
|      9 | 8235 | `	ph7_value_int(pValue,sTm.tm_year-1900);` |
|      9 | 8236 | `	if( isAssoc ){` |
|      3 | 8237 | `		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);` |
|      2 | 8238 | `	}else{` |
|      7 | 8239 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8240 | `	}` |
|      - | 8241 | `	/* wday */` |
|      9 | 8242 | `	ph7_value_int(pValue,sTm.tm_wday);` |
|      9 | 8243 | `	if( isAssoc ){` |
|      3 | 8244 | `		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);` |
|      2 | 8245 | `	}else{` |
|      7 | 8246 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8247 | `	}` |
|      - | 8248 | `	/* yday */` |
|      9 | 8249 | `	ph7_value_int(pValue,sTm.tm_yday);` |
|      9 | 8250 | `	if( isAssoc ){` |
|      3 | 8251 | `		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);` |
|      2 | 8252 | `	}else{` |
|      7 | 8253 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8254 | `	}` |
|      - | 8255 | `	/* isdst */` |
|      - | 8256 | `#ifdef __WINNT__` |
|      - | 8257 | `#ifdef _MSC_VER` |
|      - | 8258 | `#ifndef _WIN32_WCE` |
|      1 | 8259 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8260 | `#endif` |
|      - | 8261 | `#endif` |
|      - | 8262 | `#endif` |
|      9 | 8263 | `	ph7_value_int(pValue,sTm.tm_isdst);` |
|      9 | 8264 | `	if( isAssoc ){` |
|      3 | 8265 | `		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);` |
|      2 | 8266 | `	}else{` |
|      7 | 8267 | `		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);` |
|      - | 8268 | `	}` |
|      - | 8269 | `	/* Return the array */` |
|      9 | 8270 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 8271 | `	return PH7_OK;` |
|      5 | 8272 |  |
|      - | 8273 | `/*` |
|      - | 8274 | ` * int idate(string $format [, int $timestamp = time() ])` |
|      - | 8275 | ` *  Returns a number formatted according to the given format string` |
|      - | 8276 | ` *  using the given integer timestamp or the current local time if` |
|      - | 8277 | ` *  no timestamp is given. In other words, timestamp is optional and defaults` |
|      - | 8278 | ` *  to the value of time().` |
|      - | 8279 | ` *  Unlike the function date(), idate() accepts just one char in the format` |
|      - | 8280 | ` *  parameter.` |
|      - | 8281 | ` * $Parameters` |
|      - | 8282 | ` *  Supported format` |
|      - | 8283 | ` *   d 	Day of the month` |
|      - | 8284 | ` *   h 	Hour (12 hour format)` |
|      - | 8285 | ` *   H 	Hour (24 hour format)` |
|      - | 8286 | ` *   i 	Minutes` |
|      - | 8287 | ` *   I (uppercase i)1 if DST is activated, 0 otherwise` |
|      - | 8288 | ` *   L (uppercase l) returns 1 for leap year, 0 otherwise` |
|      - | 8289 | ` *   m 	Month number` |
|      - | 8290 | ` *   s 	Seconds` |
|      - | 8291 | ` *   t 	Days in current month` |
|      - | 8292 | ` *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()` |
|      - | 8293 | ` *   w 	Day of the week (0 on Sunday)` |
|      - | 8294 | ` *   W 	ISO-8601 week number of year, weeks starting on Monday` |
|      - | 8295 | ` *   y 	Year (1 or 2 digits - check note below)` |
|      - | 8296 | ` *   Y 	Year (4 digits)` |
|      - | 8297 | ` *   z 	Day of the year` |
|      - | 8298 | ` *   Z 	Timezone offset in seconds` |
|      - | 8299 | ` * $timestamp` |
|      - | 8300 | ` *  The optional timestamp parameter is an integer Unix timestamp that defaults` |
|      - | 8301 | ` *  to the current local time if a timestamp is not given. In other words, it defaults` |
|      - | 8302 | ` *  to the value of time().` |
|      - | 8303 | ` * Return` |
|      - | 8304 | ` *  An integer.` |
|      - | 8305 | ` */` |
|     40 | 8306 | `static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8307 |  |
|      - | 8308 | `	const char *zFormat;` |
|     42 | 8309 | `	ph7_int64 iVal = 0;` |
|      - | 8310 | `	int nLen;` |
|      - | 8311 | `	Sytm sTm;` |
|     42 | 8312 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8313 | `		/* Missing/Invalid argument,return -1 */` |
|      5 | 8314 | `		ph7_result_int(pCtx,-1);` |
|      5 | 8315 | `		return PH7_OK;` |
|      - | 8316 | `	}` |
|     42 | 8317 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     42 | 8318 | `	if( nLen < 1 ){` |
|      - | 8319 | `		/* Don't bother processing return -1*/` |
|    ! 0 | 8320 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 8321 | `	}` |
|     42 | 8322 | `	if( nArg < 2 ){` |
|      - | 8323 | `#ifdef __WINNT__` |
|      - | 8324 | `		SYSTEMTIME sOS;` |
|      2 | 8325 | `		GetSystemTime(&sOS);` |
|      2 | 8326 | `		SYSTEMTIME_TO_SYTM(&sOS,&sTm);` |
|      - | 8327 | `#else` |
|      - | 8328 | `		struct tm *pTm;` |
|      - | 8329 | `		time_t t;` |
|     30 | 8330 | `		time(&t);` |
|     30 | 8331 | `		pTm = localtime(&t);` |
|     30 | 8332 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8333 | `#endif` |
|     18 | 8334 | `	}else{` |
|      - | 8335 | `		/* Use the given timestamp */` |
|      - | 8336 | `		time_t t;` |
|      - | 8337 | `		struct tm *pTm;` |
|     11 | 8338 | `		if( ph7_value_is_int(apArg[1]) ){` |
|     11 | 8339 | `			t = (time_t)ph7_value_to_int64(apArg[1]);` |
|     11 | 8340 | `			pTm = localtime(&t);` |
|     11 | 8341 | `			if( pTm == 0 ){` |
|    ! 0 | 8342 | `				time(&t);` |
|    ! 0 | 8343 | `			}` |
|      6 | 8344 | `		}else{` |
|    ! 0 | 8345 | `			time(&t);` |
|      - | 8346 | `		}` |
|     11 | 8347 | `		pTm = localtime(&t);` |
|     11 | 8348 | `		STRUCT_TM_TO_SYTM(pTm,&sTm);` |
|      - | 8349 | `	}` |
|      - | 8350 | `	/* Perform the requested operation */` |
|     42 | 8351 | `	switch(zFormat[0]){` |
|      2 | 8352 | `	case 'd':` |
|      - | 8353 | `		/* Day of the month */` |
|      5 | 8354 | `		iVal = sTm.tm_mday;` |
|      5 | 8355 | `		break;` |
|    ! 0 | 8356 | `	case 'h':` |
|      - | 8357 | `		/*	Hour (12 hour format)*/` |
|    ! 0 | 8358 | `		iVal = 1 + (sTm.tm_hour % 12);` |
|    ! 0 | 8359 | `		break;` |
|      1 | 8360 | `	case 'H':` |
|      - | 8361 | `		/* Hour (24 hour format)*/` |
|      3 | 8362 | `		iVal = sTm.tm_hour;` |
|      3 | 8363 | `		break;` |
|      1 | 8364 | `	case 'i':` |
|      - | 8365 | `		/*Minutes*/` |
|      3 | 8366 | `		iVal = sTm.tm_min;` |
|      3 | 8367 | `		break;` |
|      1 | 8368 | `	case 'I':` |
|      - | 8369 | `		/*	returns 1 if DST is activated, 0 otherwise */` |
|      - | 8370 | `#ifdef __WINNT__` |
|      - | 8371 | `#ifdef _MSC_VER` |
|      - | 8372 | `#ifndef _WIN32_WCE` |
|      1 | 8373 | `			_get_daylight(&sTm.tm_isdst);` |
|      - | 8374 | `#endif` |
|      - | 8375 | `#endif` |
|      - | 8376 | `#endif` |
|      3 | 8377 | `		iVal = sTm.tm_isdst;` |
|      3 | 8378 | `		break;` |
|      1 | 8379 | `	case 'L':` |
|      - | 8380 | `		/* 	returns 1 for leap year, 0 otherwise */` |
|      3 | 8381 | `		iVal = IS_LEAP_YEAR(sTm.tm_year);` |
|      3 | 8382 | `		break;` |
|      2 | 8383 | `	case 'm':` |
|      - | 8384 | `		/* Month number*/` |
|      5 | 8385 | `		iVal = sTm.tm_mon;` |
|      5 | 8386 | `		break;` |
|      1 | 8387 | `	case 's':` |
|      - | 8388 | `		/*Seconds*/` |
|      3 | 8389 | `		iVal = sTm.tm_sec;` |
|      3 | 8390 | `		break;` |
|      1 | 8391 | `	case 't':{` |
|      - | 8392 | `		/*Days in current month*/` |
|      - | 8393 | `		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };` |
|      5 | 8394 | `		int nDays = aMonDays[sTm.tm_mon % 12 ];` |
|      5 | 8395 | `		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){` |
|      3 | 8396 | `			nDays = 28;` |
|      1 | 8397 | `		}` |
|      7 | 8398 | `		iVal = nDays;` |
|      7 | 8399 | `		break;` |
|      - | 8400 | `			 }` |
|      1 | 8401 | `	case 'U':` |
|      - | 8402 | `		/*Seconds since the Unix Epoch*/` |
|      3 | 8403 | `		iVal = (ph7_int64)time(0);` |
|      3 | 8404 | `		break;` |
|      1 | 8405 | `	case 'w':` |
|      - | 8406 | `		/*	Day of the week (0 on Sunday) */` |
|      3 | 8407 | `		iVal = sTm.tm_wday;` |
|      3 | 8408 | `		break;` |
|      1 | 8409 | `	case 'W': {` |
|      - | 8410 | `		/* ISO-8601 week number of year, weeks starting on Monday */` |
|      - | 8411 | `		static const int aISO8601_local[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };` |
|      3 | 8412 | `		iVal = aISO8601_local[sTm.tm_wday % 7 ];` |
|      3 | 8413 | `		break;` |
|      - | 8414 | `			  }` |
|    ! 0 | 8415 | `	case 'y':` |
|      - | 8416 | `		/* Year (2 digits) */` |
|    ! 0 | 8417 | `		iVal = sTm.tm_year % 100;` |
|    ! 0 | 8418 | `		break;` |
|      3 | 8419 | `	case 'Y':` |
|      - | 8420 | `		/* Year (4 digits) */` |
|      7 | 8421 | `		iVal = sTm.tm_year;` |
|      7 | 8422 | `		break;` |
|      1 | 8423 | `	case 'z':` |
|      - | 8424 | `		/* Day of the year */` |
|      3 | 8425 | `		iVal = sTm.tm_yday;` |
|      3 | 8426 | `		break;` |
|      1 | 8427 | `	case 'Z':` |
|      - | 8428 | `		/*Timezone offset in seconds*/` |
|      3 | 8429 | `		iVal = sTm.tm_gmtoff;` |
|      3 | 8430 | `		break;` |
|      1 | 8431 | `	default:` |
|      - | 8432 | `		/* unknown format,throw a warning */` |
|      3 | 8433 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");` |
|      2 | 8434 | `		break;` |
|      - | 8435 | `	}` |
|      - | 8436 | `	/* Return the time value */` |
|     40 | 8437 | `	ph7_result_int64(pCtx,iVal);` |
|     40 | 8438 | `	return PH7_OK;` |
|     23 | 8439 |  |
|      - | 8440 | `/*` |
|      - | 8441 | ` * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s")` |
|      - | 8442 | ` *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )` |
|      - | 8443 | ` *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer` |
|      - | 8444 | ` *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time` |
|      - | 8445 | ` *  specified.` |
|      - | 8446 | ` *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to` |
|      - | 8447 | ` *  the current value according to the local date and time.` |
|      - | 8448 | ` * Parameters` |
|      - | 8449 | ` * $hour` |
|      - | 8450 | ` *  The number of the hour relevant to the start of the day determined by month, day and year.` |
|      - | 8451 | ` *  Negative values reference the hour before midnight of the day in question. Values greater` |
|      - | 8452 | ` *  than 23 reference the appropriate hour in the following day(s).` |
|      - | 8453 | ` * $minute` |
|      - | 8454 | ` *  The number of the minute relevant to the start of the hour. Negative values reference` |
|      - | 8455 | ` *  the minute in the previous hour. Values greater than 59 reference the appropriate minute` |
|      - | 8456 | ` *  in the following hour(s).` |
|      - | 8457 | ` * $second` |
|      - | 8458 | ` *  The number of seconds relevant to the start of the minute. Negative values reference` |
|      - | 8459 | ` *  the second in the previous minute. Values greater than 59 reference the appropriate` |
|      - | 8460 | ` * second in the following minute(s).` |
|      - | 8461 | ` * $month` |
|      - | 8462 | ` *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference` |
|      - | 8463 | ` *  the normal calendar months of the year in question. Values less than 1 (including negative values)` |
|      - | 8464 | ` *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...` |
|      - | 8465 | ` * $day` |
|      - | 8466 | ` *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31` |
|      - | 8467 | ` *  (depending upon the month) reference the normal days in the relevant month. Values less than 1` |
|      - | 8468 | ` *  (including negative values) reference the days in the previous month, so 0 is the last day` |
|      - | 8469 | ` *  of the previous month, -1 is the day before that, etc. Values greater than the number of days` |
|      - | 8470 | ` *  in the relevant month reference the appropriate day in the following month(s).` |
|      - | 8471 | ` * $year` |
|      - | 8472 | ` *  The number of the year, may be a two or four digit value, with values between 0-69 mapping` |
|      - | 8473 | ` *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as` |
|      - | 8474 | ` *  most common today, the valid range for year is somewhere between 1901 and 2038.` |
|      - | 8475 | ` * $is_dst` |
|      - | 8476 | ` *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,` |
|      - | 8477 | ` *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not.` |
|      - | 8478 | ` * Return` |
|      - | 8479 | ` *   mktime() returns the Unix timestamp of the arguments given.` |
|      - | 8480 | ` *   If the arguments are invalid, the function returns FALSE` |
|      - | 8481 | ` */` |
|      8 | 8482 | `static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8483 |  |
|      - | 8484 | `	const char *zFunction;` |
|      9 | 8485 | `	ph7_int64 iVal = 0;` |
|      - | 8486 | `	struct tm *pTm;` |
|      - | 8487 | `	time_t t;` |
|      - | 8488 | `	/* Extract function name */` |
|      9 | 8489 | `	zFunction = ph7_function_name(pCtx);` |
|      - | 8490 | `	/* Get the current time */` |
|      9 | 8491 | `	time(&t);` |
|      9 | 8492 | `	if( zFunction[0] == 'g' /* gmmktime */ ){` |
|      3 | 8493 | `		pTm = gmtime(&t);` |
|      2 | 8494 | `	}else{` |
|      - | 8495 | `		/* localtime */` |
|      7 | 8496 | `		pTm = localtime(&t);` |
|      - | 8497 | `	}` |
|      9 | 8498 | `	if( nArg > 0 ){` |
|      - | 8499 | `		int iTmp;` |
|      - | 8500 | `		/* Hour */` |
|      9 | 8501 | `		iTmp = ph7_value_to_int(apArg[0]);` |
|      9 | 8502 | `		pTm->tm_hour = iTmp;` |
|      9 | 8503 | `		if( nArg > 1 ){` |
|      - | 8504 | `			/* Minutes */` |
|      9 | 8505 | `			iTmp = ph7_value_to_int(apArg[1]);` |
|      9 | 8506 | `			pTm->tm_min = iTmp;` |
|      9 | 8507 | `			if( nArg > 2 ){` |
|      - | 8508 | `				/* Seconds */` |
|      9 | 8509 | `				iTmp = ph7_value_to_int(apArg[2]);` |
|      9 | 8510 | `				pTm->tm_sec = iTmp;` |
|      9 | 8511 | `				if( nArg > 3 ){` |
|      - | 8512 | `					/* Month */` |
|      9 | 8513 | `					iTmp = ph7_value_to_int(apArg[3]);` |
|      9 | 8514 | `					pTm->tm_mon = iTmp - 1;` |
|      9 | 8515 | `					if( nArg > 4 ){` |
|      - | 8516 | `						/* mday */` |
|      9 | 8517 | `						iTmp = ph7_value_to_int(apArg[4]);` |
|      9 | 8518 | `						pTm->tm_mday = iTmp;` |
|      9 | 8519 | `						if( nArg > 5 ){` |
|      - | 8520 | `							/* Year */` |
|      9 | 8521 | `							iTmp = ph7_value_to_int(apArg[5]);` |
|      9 | 8522 | `							if( iTmp > 1900 ){` |
|      9 | 8523 | `								iTmp -= 1900;` |
|      4 | 8524 | `							}` |
|      9 | 8525 | `							pTm->tm_year = iTmp;` |
|      9 | 8526 | `							if( nArg > 6 ){` |
|      - | 8527 | `								/* is_dst */` |
|    ! 0 | 8528 | `								iTmp = ph7_value_to_bool(apArg[6]);` |
|    ! 0 | 8529 | `								pTm->tm_isdst = iTmp;` |
|    ! 0 | 8530 | `							}` |
|      4 | 8531 | `						}` |
|      4 | 8532 | `					}` |
|      4 | 8533 | `				}` |
|      4 | 8534 | `			}` |
|      4 | 8535 | `		}` |
|      4 | 8536 | `	}` |
|      - | 8537 | `	/* Make the time */` |
|      9 | 8538 | `	iVal = (ph7_int64)mktime(pTm);` |
|      - | 8539 | `	/* Return the timesatmp as a 64bit integer */` |
|      9 | 8540 | `	ph7_result_int64(pCtx,iVal);` |
|      9 | 8541 | `	return PH7_OK;` |
|      1 | 8542 |  |
|      - | 8543 | `/*` |
|      - | 8544 | ` * Section:` |
|      - | 8545 | ` *    URL handling Functions.` |
|      - | 8546 | ` * Status:` |
|      - | 8547 | ` *    Stable.` |
|      - | 8548 | ` */` |
|      - | 8549 | `/*` |
|      - | 8550 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8551 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8552 | ` */` |
|   1026 | 8553 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8554 |  |
|      - | 8555 | `	/* Store in the call context result buffer */` |
|   1028 | 8556 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8557 | `	return SXRET_OK;` |
|      2 | 8558 |  |
|      - | 8559 | `/*` |
|      - | 8560 | ` * string base64_encode(string $data)` |
|      - | 8561 | ` * string convert_uuencode(string $data)` |
|      - | 8562 | ` *  Encodes data with MIME base64` |
|      - | 8563 | ` * Parameter` |
|      - | 8564 | ` *  $data` |
|      - | 8565 | ` *    Data to encode` |
|      - | 8566 | ` * Return` |
|      - | 8567 | ` *  Encoded data or FALSE on failure.` |
|      - | 8568 | ` */` |
|     10 | 8569 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8570 |  |
|      - | 8571 | `	const char *zIn;` |
|      - | 8572 | `	int nLen;` |
|     11 | 8573 | `	if( nArg < 1 ){` |
|      - | 8574 | `		/* Missing arguments,return FALSE */` |
|      5 | 8575 | `		ph7_result_bool(pCtx,0);` |
|      5 | 8576 | `		return PH7_OK;` |
|      - | 8577 | `	}` |
|      - | 8578 | `	/* Extract the input string */` |
|      7 | 8579 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8580 | `	if( nLen < 1 ){` |
|      - | 8581 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8582 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8583 | `		return PH7_OK;` |
|      - | 8584 | `	}` |
|      - | 8585 | `	/* Perform the BASE64 encoding */` |
|      7 | 8586 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8587 | `	return PH7_OK;` |
|      6 | 8588 |  |
|      - | 8589 | `/*` |
|      - | 8590 | ` * string base64_decode(string $data)` |
|      - | 8591 | ` * string convert_uudecode(string $data)` |
|      - | 8592 | ` *  Decodes data encoded with MIME base64` |
|      - | 8593 | ` * Parameter` |
|      - | 8594 | ` *  $data` |
|      - | 8595 | ` *    Encoded data.` |
|      - | 8596 | ` * Return` |
|      - | 8597 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8598 | ` */` |
|     36 | 8599 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8600 |  |
|      - | 8601 | `	const char *zIn;` |
|      - | 8602 | `	int nLen;` |
|     38 | 8603 | `	if( nArg < 1 ){` |
|      - | 8604 | `		/* Missing arguments,return FALSE */` |
|      3 | 8605 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8606 | `		return PH7_OK;` |
|      - | 8607 | `	}` |
|      - | 8608 | `	/* Extract the input string */` |
|     36 | 8609 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8610 | `	if( nLen < 1 ){` |
|      - | 8611 | `		/* Nothing to process,return FALSE */` |
|      3 | 8612 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8613 | `		return PH7_OK;` |
|      - | 8614 | `	}` |
|      - | 8615 | `	/* Perform the BASE64 decoding */` |
|     34 | 8616 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8617 | `	return PH7_OK;` |
|     20 | 8618 |  |
|      - | 8619 | `/*` |
|      - | 8620 | ` * string urlencode(string $str)` |
|      - | 8621 | ` *  URL encoding` |
|      - | 8622 | ` * Parameter` |
|      - | 8623 | ` *  $data` |
|      - | 8624 | ` *   Input string.` |
|      - | 8625 | ` * Return` |
|      - | 8626 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8627 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8628 | ` *  encoded as plus (+) signs.` |
|      - | 8629 | ` */` |
|      6 | 8630 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8631 |  |
|      - | 8632 | `	const char *zIn;` |
|      - | 8633 | `	int nLen;` |
|      7 | 8634 | `	if( nArg < 1 ){` |
|      - | 8635 | `		/* Missing arguments,return FALSE */` |
|      3 | 8636 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8637 | `		return PH7_OK;` |
|      - | 8638 | `	}` |
|      - | 8639 | `	/* Extract the input string */` |
|      5 | 8640 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8641 | `	if( nLen < 1 ){` |
|      - | 8642 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8643 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8644 | `		return PH7_OK;` |
|      - | 8645 | `	}` |
|      - | 8646 | `	/* Perform the URL encoding */` |
|      5 | 8647 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8648 | `	return PH7_OK;` |
|      4 | 8649 |  |
|      - | 8650 | `/*` |
|      - | 8651 | ` * string urldecode(string $str)` |
|      - | 8652 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8653 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8654 | ` * Parameter` |
|      - | 8655 | ` *  $data` |
|      - | 8656 | ` *    Input string.` |
|      - | 8657 | ` * Return` |
|      - | 8658 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8659 | ` */` |
|      8 | 8660 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8661 |  |
|      - | 8662 | `	const char *zIn;` |
|      - | 8663 | `	int nLen;` |
|      9 | 8664 | `	if( nArg < 1 ){` |
|      - | 8665 | `		/* Missing arguments,return FALSE */` |
|      3 | 8666 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8667 | `		return PH7_OK;` |
|      - | 8668 | `	}` |
|      - | 8669 | `	/* Extract the input string */` |
|      7 | 8670 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8671 | `	if( nLen < 1 ){` |
|      - | 8672 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8673 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8674 | `		return PH7_OK;` |
|      - | 8675 | `	}` |
|      - | 8676 | `	/* Perform the URL decoding */` |
|      7 | 8677 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8678 | `	return PH7_OK;` |
|      5 | 8679 |  |
|      - | 8680 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8681 | `/* Table of the built-in functions */` |
|      - | 8682 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8683 | `	   /* Variable handling functions */` |
|      - | 8684 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8685 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8686 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8687 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8688 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8689 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8690 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8691 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8692 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8693 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8694 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8695 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8696 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8697 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8698 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8699 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8700 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8701 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8702 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8703 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8704 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8705 | `	   /* Math functions */` |
|      - | 8706 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8707 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8708 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8709 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8710 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8711 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8712 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8713 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8714 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8715 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8716 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8717 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8718 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8719 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8720 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8721 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8722 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8723 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8724 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8725 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8726 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8727 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8728 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8729 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8730 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8731 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8732 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8733 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8734 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8735 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8736 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8737 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8738 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8739 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8740 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8741 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8742 | `	   /* String handling functions */` |
|      - | 8743 |  |
|      - | 8744 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8745 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8746 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8747 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8748 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8749 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8750 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8751 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8752 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8753 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8754 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8755 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8756 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8757 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8758 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8759 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8760 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8761 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8762 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8763 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8764 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8765 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8766 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8767 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8768 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8769 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8770 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8771 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8772 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8773 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8774 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8775 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8776 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8777 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8778 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8779 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8780 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8781 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8782 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8783 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8784 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8785 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8786 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8787 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8788 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8789 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8790 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8791 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8792 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8793 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8794 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8795 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8796 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8797 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8798 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8799 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8800 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8801 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8802 |  |
|      - | 8803 |  |
|      - | 8804 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8805 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8806 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8807 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8808 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8809 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8810 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8811 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8812 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8813 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8814 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8815 |  |
|      - | 8816 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8817 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8818 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8819 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8820 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8821 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8822 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8823 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8824 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8825 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8826 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8827 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8828 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8829 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8830 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8831 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8832 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8833 |  |
|      - | 8834 | `	         /* Ctype functions */` |
|      - | 8835 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8836 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8837 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8838 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8839 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8840 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8841 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8842 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8843 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8844 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8845 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8846 | `	         /* Time functions */` |
|      - | 8847 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8848 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8849 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8850 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8851 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8852 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8853 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8854 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8855 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8856 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8857 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8858 | `	        /* URL functions */` |
|      - | 8859 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8860 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8861 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8862 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8863 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8864 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8865 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8866 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8867 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8868 | `};` |
|      - | 8869 | `/*` |
|      - | 8870 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8871 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8872 | ` */` |
|    974 | 8873 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 8874 |  |
|      - | 8875 | `	sxu32 n;` |
| 149024 | 8876 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 148050 | 8877 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
|  74026 | 8878 | `	}` |
|      - | 8879 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|    976 | 8880 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8881 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|    976 | 8882 | `	PH7_RegisterIORoutine(&(*pVm));` |
|    976 | 8883 |  |
|      - | 8884 |  |
