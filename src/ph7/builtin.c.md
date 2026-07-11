# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3743/4359 lines (85.87%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* filter_var(FILTER_VALIDATE_FLOAT) parses with libc strtod for overflow/underflow` |
|      - |    8 | ` * detection + a correctly-rounded value; SyStrToReal clamps the exponent and saturates,` |
|      - |    9 | ` * so it cannot reject out-of-range magnitudes. Same rationale as builtin_math.c's round(). */` |
|      - |   10 | `#include <stdlib.h>  /* strtod */` |
|      - |   11 | `#include <math.h>    /* HUGE_VAL */` |
|      - |   12 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|      - |   13 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |   14 | `/*` |
|      - |   15 | ` * Section:` |
|      - |   16 | ` *    Variable handling Functions.` |
|      - |   17 | ` * Status:` |
|      - |   18 | ` *    Stable.` |
|      - |   19 | ` */` |
|      - |   20 | `/*` |
|      - |   21 | ` * bool is_bool($var)` |
|      - |   22 | ` *  Finds out whether a variable is a boolean.` |
|      - |   23 | ` * Parameters` |
|      - |   24 | ` *   $var: The variable being evaluated.` |
|      - |   25 | ` * Return` |
|      - |   26 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |   27 | ` */` |
|     28 |   28 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   29 | `{` |
|     29 |   30 | `	int res = 0; /* Assume false by default */` |
|     29 |   31 | `	if( nArg > 0 ){` |
|     29 |   32 | `		res = ph7_value_is_bool(apArg[0]);` |
|     14 |   33 | `	}` |
|      - |   34 | `	/* Query result */` |
|     29 |   35 | `	ph7_result_bool(pCtx,res);` |
|     29 |   36 | `	return PH7_OK;` |
|      1 |   37 | `}` |
|      - |   38 | `/*` |
|      - |   39 | ` * bool is_float($var)` |
|      - |   40 | ` * bool is_real($var)` |
|      - |   41 | ` * bool is_double($var)` |
|      - |   42 | ` *  Finds out whether a variable is a float.` |
|      - |   43 | ` * Parameters` |
|      - |   44 | ` *   $var: The variable being evaluated.` |
|      - |   45 | ` * Return` |
|      - |   46 | ` *  TRUE if var is a float. False otherwise.` |
|      - |   47 | ` */` |
|    204 |   48 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   49 | `{` |
|    205 |   50 | `	int res = 0; /* Assume false by default */` |
|    205 |   51 | `	if( nArg > 0 ){` |
|    205 |   52 | `		res = ph7_value_is_float(apArg[0]);` |
|    102 |   53 | `	}` |
|      - |   54 | `	/* Query result */` |
|    205 |   55 | `	ph7_result_bool(pCtx,res);` |
|    205 |   56 | `	return PH7_OK;` |
|      1 |   57 | `}` |
|      - |   58 | `/*` |
|      - |   59 | ` * bool is_int($var)` |
|      - |   60 | ` * bool is_integer($var)` |
|      - |   61 | ` * bool is_long($var)` |
|      - |   62 | ` *  Finds out whether a variable is an integer.` |
|      - |   63 | ` * Parameters` |
|      - |   64 | ` *   $var: The variable being evaluated.` |
|      - |   65 | ` * Return` |
|      - |   66 | ` *  TRUE if var is an integer. False otherwise.` |
|      - |   67 | ` */` |
|    630 |   68 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |   69 | `{` |
|    633 |   70 | `	int res = 0; /* Assume false by default */` |
|    633 |   71 | `	if( nArg > 0 ){` |
|      - |   72 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   73 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   74 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    633 |   75 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    315 |   76 | `	}` |
|      - |   77 | `	/* Query result */` |
|    633 |   78 | `	ph7_result_bool(pCtx,res);` |
|    633 |   79 | `	return PH7_OK;` |
|      3 |   80 | `}` |
|      - |   81 | `/*` |
|      - |   82 | ` * bool is_string($var)` |
|      - |   83 | ` *  Finds out whether a variable is a string.` |
|      - |   84 | ` * Parameters` |
|      - |   85 | ` *   $var: The variable being evaluated.` |
|      - |   86 | ` * Return` |
|      - |   87 | ` *  TRUE if var is string. False otherwise.` |
|      - |   88 | ` */` |
|    124 |   89 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   90 | `{` |
|    125 |   91 | `	int res = 0; /* Assume false by default */` |
|    125 |   92 | `	if( nArg > 0 ){` |
|    125 |   93 | `		res = ph7_value_is_string(apArg[0]);` |
|     62 |   94 | `	}` |
|      - |   95 | `	/* Query result */` |
|    125 |   96 | `	ph7_result_bool(pCtx,res);` |
|    125 |   97 | `	return PH7_OK;` |
|      1 |   98 | `}` |
|      - |   99 | `/*` |
|      - |  100 | ` * bool is_null($var)` |
|      - |  101 | ` *  Finds out whether a variable is NULL.` |
|      - |  102 | ` * Parameters` |
|      - |  103 | ` *   $var: The variable being evaluated.` |
|      - |  104 | ` * Return` |
|      - |  105 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  106 | ` */` |
|     84 |  107 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  108 | `{` |
|     87 |  109 | `	int res = 0; /* Assume false by default */` |
|     87 |  110 | `	if( nArg > 0 ){` |
|     87 |  111 | `		res = ph7_value_is_null(apArg[0]);` |
|     42 |  112 | `	}` |
|      - |  113 | `	/* Query result */` |
|     87 |  114 | `	ph7_result_bool(pCtx,res);` |
|     87 |  115 | `	return PH7_OK;` |
|      3 |  116 | `}` |
|      - |  117 | `/*` |
|      - |  118 | ` * bool is_numeric($var)` |
|      - |  119 | ` *  Find out whether a variable is NULL.` |
|      - |  120 | ` * Parameters` |
|      - |  121 | ` *  $var: The variable being evaluated.` |
|      - |  122 | ` * Return` |
|      - |  123 | ` *  True if var is numeric. False otherwise.` |
|      - |  124 | ` */` |
|     36 |  125 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  126 | `{` |
|     41 |  127 | `	int res = 0; /* Assume false by default */` |
|     41 |  128 | `	if( nArg > 0 ){` |
|     41 |  129 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     18 |  130 | `	}` |
|      - |  131 | `	/* Query result */` |
|     41 |  132 | `	ph7_result_bool(pCtx,res);` |
|     41 |  133 | `	return PH7_OK;` |
|      5 |  134 | `}` |
|      - |  135 | `/*` |
|      - |  136 | ` * bool is_scalar($var)` |
|      - |  137 | ` *  Find out whether a variable is a scalar.` |
|      - |  138 | ` * Parameters` |
|      - |  139 | ` *  $var: The variable being evaluated.` |
|      - |  140 | ` * Return` |
|      - |  141 | ` *  True if var is scalar. False otherwise.` |
|      - |  142 | ` */` |
|     12 |  143 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  144 | `{` |
|     13 |  145 | `	int res = 0; /* Assume false by default */` |
|     13 |  146 | `	if( nArg > 0 ){` |
|     13 |  147 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  148 | `	}` |
|      - |  149 | `	/* Query result */` |
|     13 |  150 | `	ph7_result_bool(pCtx,res);` |
|     13 |  151 | `	return PH7_OK;` |
|      1 |  152 | `}` |
|      - |  153 | `/*` |
|      - |  154 | ` * bool is_array($var)` |
|      - |  155 | ` *  Find out whether a variable is an array.` |
|      - |  156 | ` * Parameters` |
|      - |  157 | ` *  $var: The variable being evaluated.` |
|      - |  158 | ` * Return` |
|      - |  159 | ` *  True if var is an array. False otherwise.` |
|      - |  160 | ` */` |
|    238 |  161 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  162 | `{` |
|    243 |  163 | `	int res = 0; /* Assume false by default */` |
|    243 |  164 | `	if( nArg > 0 ){` |
|    243 |  165 | `		res = ph7_value_is_array(apArg[0]);` |
|    119 |  166 | `	}` |
|      - |  167 | `	/* Query result */` |
|    243 |  168 | `	ph7_result_bool(pCtx,res);` |
|    243 |  169 | `	return PH7_OK;` |
|      5 |  170 | `}` |
|      - |  171 | `/*` |
|      - |  172 | ` * bool is_object($var)` |
|      - |  173 | ` *  Find out whether a variable is an object.` |
|      - |  174 | ` * Parameters` |
|      - |  175 | ` *  $var: The variable being evaluated.` |
|      - |  176 | ` * Return` |
|      - |  177 | ` *  True if var is an object. False otherwise.` |
|      - |  178 | ` */` |
|     20 |  179 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  180 | `{` |
|     21 |  181 | `	int res = 0; /* Assume false by default */` |
|     21 |  182 | `	if( nArg > 0 ){` |
|     21 |  183 | `		res = ph7_value_is_object(apArg[0]);` |
|     10 |  184 | `	}` |
|      - |  185 | `	/* Query result */` |
|     21 |  186 | `	ph7_result_bool(pCtx,res);` |
|     21 |  187 | `	return PH7_OK;` |
|      1 |  188 | `}` |
|      - |  189 | `/*` |
|      - |  190 | ` * bool is_resource($var)` |
|      - |  191 | ` *  Find out whether a variable is a resource.` |
|      - |  192 | ` * Parameters` |
|      - |  193 | ` *  $var: The variable being evaluated.` |
|      - |  194 | ` * Return` |
|      - |  195 | ` *  True if a resource. False otherwise.` |
|      - |  196 | ` */` |
|     58 |  197 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  198 | `{` |
|     61 |  199 | `	int res = 0; /* Assume false by default */` |
|     61 |  200 | `	if( nArg > 0 ){` |
|     61 |  201 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  202 | `	}` |
|     61 |  203 | `	ph7_result_bool(pCtx,res);` |
|     61 |  204 | `	return PH7_OK;` |
|      3 |  205 | `}` |
|      - |  206 | `/*` |
|      - |  207 | ` * float floatval($var)` |
|      - |  208 | ` *  Get float value of a variable.` |
|      - |  209 | ` * Parameter` |
|      - |  210 | ` *  $var: The variable being processed.` |
|      - |  211 | ` * Return` |
|      - |  212 | ` *  the float value of a variable.` |
|      - |  213 | ` */` |
|      4 |  214 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  215 | `{` |
|      5 |  216 | `	if( nArg < 1 ){` |
|      - |  217 | `		/* return 0.0 */` |
|    ! 0 |  218 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  219 | `	}else{` |
|      - |  220 | `		double dval;` |
|      - |  221 | `		/* Perform the cast */` |
|      5 |  222 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  223 | `		ph7_result_double(pCtx,dval);` |
|      - |  224 | `	}` |
|      5 |  225 | `	return PH7_OK;` |
|      1 |  226 | `}` |
|      - |  227 | `/*` |
|      - |  228 | ` * int intval($var)` |
|      - |  229 | ` *  Get integer value of a variable.` |
|      - |  230 | ` * Parameter` |
|      - |  231 | ` *  $var: The variable being processed.` |
|      - |  232 | ` * Return` |
|      - |  233 | ` *  the int value of a variable.` |
|      - |  234 | ` */` |
|     24 |  235 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  236 | `{` |
|     25 |  237 | `	if( nArg < 1 ){` |
|      - |  238 | `		/* return 0 */` |
|    ! 0 |  239 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  240 | `	}else{` |
|      - |  241 | `		sxi64 iVal;` |
|      - |  242 | `		/* Perform the cast */` |
|     25 |  243 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     25 |  244 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  245 | `	}` |
|     25 |  246 | `	return PH7_OK;` |
|      1 |  247 | `}` |
|      - |  248 | `/*` |
|      - |  249 | ` * string strval($var)` |
|      - |  250 | ` *  Get the string representation of a variable.` |
|      - |  251 | ` * Parameter` |
|      - |  252 | ` *  $var: The variable being processed.` |
|      - |  253 | ` * Return` |
|      - |  254 | ` *  the string value of a variable.` |
|      - |  255 | ` */` |
|      2 |  256 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  257 | `{` |
|      3 |  258 | `	if( nArg < 1 ){` |
|      - |  259 | `		/* return NULL */` |
|    ! 0 |  260 | `		ph7_result_null(pCtx);` |
|    ! 0 |  261 | `	}else{` |
|      - |  262 | `		const char *zVal;` |
|      3 |  263 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  264 | `		/* Perform the cast */` |
|      3 |  265 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  266 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  267 | `	}` |
|      3 |  268 | `	return PH7_OK;` |
|      1 |  269 | `}` |
|      - |  270 | `/*` |
|      - |  271 | ` * bool boolval($var)` |
|      - |  272 | ` *  Get the boolean value of a variable.` |
|      - |  273 | ` * Parameter` |
|      - |  274 | ` *  $var: The variable being processed.` |
|      - |  275 | ` * Return` |
|      - |  276 | ` *  the bool value of a variable.` |
|      - |  277 | ` */` |
|     16 |  278 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  279 | `{` |
|      - |  280 | `	int bVal;` |
|     18 |  281 | `	if( nArg != 1 ){` |
|      4 |  282 | `		return PH7_VmThrowException(pCtx,` |
|      - |  283 | `			"ArgumentCountError",` |
|      - |  284 | `			"boolval() expects exactly 1 argument, %d given",` |
|      1 |  285 | `			nArg` |
|      - |  286 | `			);` |
|      - |  287 | `	}` |
|      - |  288 | `	/* Perform the cast */` |
|     15 |  289 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  290 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  291 | `	return PH7_OK;` |
|     10 |  292 | `}` |
|      - |  293 | `/*` |
|      - |  294 | ` * bool empty($var)` |
|      - |  295 | ` *  Determine whether a variable is empty.` |
|      - |  296 | ` * Parameters` |
|      - |  297 | ` *   $var: The variable being checked.` |
|      - |  298 | ` * Return` |
|      - |  299 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  300 | ` */` |
|  33102 |  301 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  302 | `{` |
|  33107 |  303 | `	int res = 1; /* Assume empty by default */` |
|  33107 |  304 | `	if( nArg > 0 ){` |
|  33105 |  305 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16550 |  306 | `	}` |
|  33107 |  307 | `	ph7_result_bool(pCtx,res);` |
|  33107 |  308 | `	return PH7_OK;` |
|      - |  309 |  |
|      5 |  310 | `}` |
|      - |  311 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  312 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  313 | `#endif` |
|      - |  314 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  315 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  316 | `#endif` |
|      - |  317 |  |
|      - |  318 | `/* Math functions moved to builtin_math.c */` |
|      - |  319 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  320 | `/*` |
|      - |  321 | ` * Section:` |
|      - |  322 | ` *    String handling Functions.` |
|      - |  323 | ` * Status:` |
|      - |  324 | ` *    Stable.` |
|      - |  325 | ` */` |
|      - |  326 | `/*` |
|      - |  327 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  328 | ` *  Return part of a string.` |
|      - |  329 | ` * Parameters` |
|      - |  330 | ` *  $string` |
|      - |  331 | ` *   The input string. Must be one character or longer.` |
|      - |  332 | ` * $start` |
|      - |  333 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  334 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  335 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  336 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  337 | ` *   from the end of string.` |
|      - |  338 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  339 | ` * $length` |
|      - |  340 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  341 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  342 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  343 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  344 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  345 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  346 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  347 | ` *   will be returned.` |
|      - |  348 | ` * Return` |
|      - |  349 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  350 | ` */` |
| 211456 |  351 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  352 | `{` |
|      - |  353 | `	const char *zSource,*zOfft;` |
|      - |  354 | `	int nOfft,nLen,nSrcLen;` |
| 211461 |  355 | `	if( nArg < 2 ){` |
|      - |  356 | `		/* return FALSE */` |
|    ! 0 |  357 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  358 | `		return PH7_OK;` |
|      - |  359 | `	}` |
|      - |  360 | `	/* Extract the target string */` |
| 211461 |  361 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 211461 |  362 | `	if( nSrcLen < 1 ){` |
|      - |  363 | `		/* Empty string,return FALSE */` |
|  11693 |  364 | `		ph7_result_bool(pCtx,0);` |
|  11693 |  365 | `		return PH7_OK;` |
|      - |  366 | `	}` |
| 199773 |  367 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  368 | `	/* Extract the offset */` |
| 199773 |  369 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 199773 |  370 | `	if( nOfft < 0 ){` |
|  31877 |  371 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  31877 |  372 | `		if( zOfft < zSource ){` |
|      - |  373 | `			/* Invalid offset */` |
|      5 |  374 | `			ph7_result_bool(pCtx,0);` |
|      5 |  375 | `			return PH7_OK;` |
|      - |  376 | `		}` |
|  31873 |  377 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  31873 |  378 | `		nOfft = (int)(zOfft-zSource);` |
| 183835 |  379 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  380 | `		/* Invalid offset */` |
|    195 |  381 | `		ph7_result_bool(pCtx,0);` |
|    195 |  382 | `		return PH7_OK;` |
|    ! 0 |  383 | `	}else{` |
| 167711 |  384 | `		zOfft = &zSource[nOfft];` |
| 167711 |  385 | `		nLen = nSrcLen - nOfft;` |
|      - |  386 | `	}` |
| 199579 |  387 | `	if( nArg > 2 ){` |
|      - |  388 | `		/* Extract the length */` |
| 164393 |  389 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 164393 |  390 | `		if( nLen == 0 ){` |
|      - |  391 | `			/* Invalid length,return an empty string */` |
|      5 |  392 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  393 | `			return PH7_OK;` |
| 164389 |  394 | `		}else if( nLen < 0 ){` |
|  31865 |  395 | `			nLen = nSrcLen + nLen - nOfft;` |
|  31865 |  396 | `			if( nLen < 1 ){` |
|      - |  397 | `				/* Invalid  length */` |
|      3 |  398 | `				nLen = nSrcLen - nOfft;` |
|      1 |  399 | `			}` |
|  15930 |  400 | `		}` |
| 164389 |  401 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  402 | `			/* Invalid length */` |
|   5059 |  403 | `			nLen = nSrcLen - nOfft;` |
|   2527 |  404 | `		}` |
|  82192 |  405 | `	}` |
|      - |  406 | `	/* Return the substring */` |
| 199575 |  407 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 199575 |  408 | `	return PH7_OK;` |
| 105733 |  409 | `}` |
|      - |  410 | `/*` |
|      - |  411 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  412 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  413 | ` * Parameters` |
|      - |  414 | ` *  $main_str` |
|      - |  415 | ` *  The main string being compared.` |
|      - |  416 | ` *  $str` |
|      - |  417 | ` *   The secondary string being compared.` |
|      - |  418 | ` * $offset` |
|      - |  419 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  420 | ` *  the end of the string.` |
|      - |  421 | ` * $length` |
|      - |  422 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  423 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  424 | ` * $case_insensitivity` |
|      - |  425 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  426 | ` * Return` |
|      - |  427 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  428 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  429 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  430 | ` */` |
|     22 |  431 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  432 | `{` |
|      - |  433 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  434 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     23 |  435 | `	int iCase = 0;` |
|      - |  436 | `	int rc;` |
|     23 |  437 | `	if( nArg < 3 ){` |
|      - |  438 | `		/* Missing arguments,return FALSE */` |
|    ! 0 |  439 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  440 | `		return PH7_OK;` |
|      - |  441 | `	}` |
|      - |  442 | `	/* Extract the target string */` |
|     23 |  443 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  444 | `	if( nSrcLen < 1 ){` |
|      - |  445 | `		/* Empty string,return FALSE */` |
|      3 |  446 | `		ph7_result_bool(pCtx,0);` |
|      3 |  447 | `		return PH7_OK;` |
|      - |  448 | `	}` |
|     21 |  449 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  450 | `	/* Extract the substring */` |
|     21 |  451 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  452 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  453 | `		/* Empty string,return FALSE */` |
|      3 |  454 | `		ph7_result_bool(pCtx,0);` |
|      3 |  455 | `		return PH7_OK;` |
|      - |  456 | `	}` |
|      - |  457 | `	/* Extract the offset */` |
|     19 |  458 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  459 | `	if( nOfft < 0 ){` |
|      5 |  460 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  461 | `		if( zOfft < zSource ){` |
|      - |  462 | `			/* Invalid offset */` |
|      3 |  463 | `			ph7_result_bool(pCtx,0);` |
|      3 |  464 | `			return PH7_OK;` |
|      - |  465 | `		}` |
|      3 |  466 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  467 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  468 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  469 | `		/* Invalid offset */` |
|      3 |  470 | `		ph7_result_bool(pCtx,0);` |
|      3 |  471 | `		return PH7_OK;` |
|    ! 0 |  472 | `	}else{` |
|     13 |  473 | `		zOfft = &zSource[nOfft];` |
|     13 |  474 | `		nLen = nSrcLen - nOfft;` |
|      - |  475 | `	}` |
|     15 |  476 | `	if( nArg > 3 ){` |
|      - |  477 | `		/* Extract the length */` |
|     13 |  478 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  479 | `		if( nLen < 1 ){` |
|      - |  480 | `			/* Invalid  length */` |
|      5 |  481 | `			ph7_result_int(pCtx,1);` |
|      5 |  482 | `			return PH7_OK;` |
|      9 |  483 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  484 | `			/* Invalid length */` |
|      3 |  485 | `			nLen = nSrcLen - nOfft;` |
|      1 |  486 | `		}` |
|      9 |  487 | `		if( nArg > 4 ){` |
|      - |  488 | `			/* Case-sensitive or not */` |
|      5 |  489 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  490 | `		}` |
|      4 |  491 | `	}` |
|      - |  492 | `	/* Perform the comparison */` |
|     11 |  493 | `	if( iCase ){` |
|      3 |  494 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  495 | `	}else{` |
|      9 |  496 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Comparison result */` |
|     11 |  499 | `	ph7_result_int(pCtx,rc);` |
|     11 |  500 | `	return PH7_OK;` |
|     12 |  501 | `}` |
|      - |  502 | `/*` |
|      - |  503 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  504 | ` *  Count the number of substring occurrences.` |
|      - |  505 | ` * Parameters` |
|      - |  506 | ` * $haystack` |
|      - |  507 | ` *   The string to search in` |
|      - |  508 | ` * $needle` |
|      - |  509 | ` *   The substring to search for` |
|      - |  510 | ` * $offset` |
|      - |  511 | ` *  The offset where to start counting` |
|      - |  512 | ` * $length (NOT USED)` |
|      - |  513 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  514 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  515 | ` * Return` |
|      - |  516 | ` *  Toral number of substring occurrences.` |
|      - |  517 | ` */` |
|     26 |  518 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  519 | `{` |
|      - |  520 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  521 | `	int nTextlen,nPatlen;` |
|     27 |  522 | `	int iCount = 0;` |
|      - |  523 | `	sxu32 nOfft;` |
|      - |  524 | `	sxi32 rc;` |
|     27 |  525 | `	if( nArg < 2 ){` |
|      - |  526 | `		/* Missing arguments */` |
|    ! 0 |  527 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  528 | `		return PH7_OK;` |
|      - |  529 | `	}` |
|      - |  530 | `	/* Point to the haystack */` |
|     27 |  531 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  532 | `	/* Point to the neddle */` |
|     27 |  533 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     27 |  534 | `	if( nPatlen < 1 ){` |
|      - |  535 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  536 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  537 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  538 | `	}` |
|      - |  539 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  540 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  541 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  542 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     25 |  543 | `	if( nArg > 2 ){` |
|     19 |  544 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|     19 |  545 | `		if( iOfft < 0 ){` |
|      5 |  546 | `			iOfft += nTextlen;` |
|      2 |  547 | `		}` |
|     19 |  548 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      3 |  549 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  550 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  551 | `		}` |
|      - |  552 | `		/* Point to the desired offset and shrink the remaining region */` |
|     17 |  553 | `		zText = &zText[iOfft];` |
|     17 |  554 | `		nTextlen -= (int)iOfft;` |
|      8 |  555 | `	}` |
|     23 |  556 | `	if( nArg > 3 ){` |
|     15 |  557 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|     15 |  558 | `		if( nLen < 0 ){` |
|      - |  559 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|      5 |  560 | `			nLen += nTextlen;` |
|      2 |  561 | `		}` |
|     15 |  562 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      5 |  563 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  564 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|      - |  565 | `		}` |
|     11 |  566 | `		nTextlen = (int)nLen;` |
|      5 |  567 | `	}` |
|     19 |  568 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  569 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  570 | `		ph7_result_int(pCtx,0);` |
|      3 |  571 | `		return PH7_OK;` |
|      - |  572 | `	}` |
|      - |  573 | `	/* Point to the end of the windowed haystack */` |
|     17 |  574 | `	zEnd = &zText[nTextlen];` |
|      - |  575 | `	/* Perform the search */` |
|     17 |  576 | `	for(;;){` |
|     35 |  577 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     35 |  578 | `		if( rc != SXRET_OK ){` |
|      - |  579 | `			/* Pattern not found,break immediately */` |
|     13 |  580 | `			break;` |
|      - |  581 | `		}` |
|      - |  582 | `		/* Increment counter and update the offset */` |
|     23 |  583 | `		iCount++;` |
|     23 |  584 | `		zText += nOfft + nPatlen;` |
|     23 |  585 | `		if( zText >= zEnd ){` |
|      5 |  586 | `			break;` |
|      - |  587 | `		}` |
|      1 |  588 | `	}` |
|      - |  589 | `	/* Pattern count */` |
|     17 |  590 | `	ph7_result_int(pCtx,iCount);` |
|     17 |  591 | `	return PH7_OK;` |
|     14 |  592 | `}` |
|      - |  593 | `/*` |
|      - |  594 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - |  595 | ` *   Split a string into smaller chunks.` |
|      - |  596 | ` * Parameters` |
|      - |  597 | ` *  $body` |
|      - |  598 | ` *   The string to be chunked.` |
|      - |  599 | ` * $chunklen` |
|      - |  600 | ` *   The chunk length.` |
|      - |  601 | ` * $end` |
|      - |  602 | ` *   The line ending sequence.` |
|      - |  603 | ` * Return` |
|      - |  604 | ` *  The chunked string or NULL on failure.` |
|      - |  605 | ` */` |
|     14 |  606 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  607 | `{` |
|     15 |  608 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - |  609 | `	int nSepLen,nChunkLen,nLen;` |
|     15 |  610 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  611 | `		/* Nothing to split,return null */` |
|      3 |  612 | `		ph7_result_null(pCtx);` |
|      3 |  613 | `		return PH7_OK;` |
|      - |  614 | `	}` |
|      - |  615 | `	/* initialize/Extract arguments */` |
|     13 |  616 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 |  617 | `	nChunkLen = 76;` |
|     13 |  618 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 |  619 | `	zEnd = &zIn[nLen];` |
|     13 |  620 | `	if( nArg > 1 ){` |
|      - |  621 | `		/* Chunk length */` |
|     13 |  622 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 |  623 | `		if( nChunkLen < 1 ){` |
|      - |  624 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 |  625 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  626 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - |  627 | `		}` |
|     11 |  628 | `		if( nArg > 2 ){` |
|      - |  629 | `			/* Separator */` |
|      9 |  630 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 |  631 | `			if( nSepLen < 1 ){` |
|      - |  632 | `				/* Switch back to the default separator */` |
|      3 |  633 | `				zSep = "\r\n";` |
|      3 |  634 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 |  635 | `			}` |
|      4 |  636 | `		}` |
|      5 |  637 | `	}` |
|      - |  638 | `	/* Perform the requested operation */` |
|     11 |  639 | `	if( nChunkLen > nLen ){` |
|      - |  640 | `		/* Nothing to split,return the string and the separator */` |
|      7 |  641 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      7 |  642 | `		return PH7_OK;` |
|      - |  643 | `	}` |
|     17 |  644 | `	while( zIn < zEnd ){` |
|     13 |  645 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 |  646 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 |  647 | `		}` |
|      - |  648 | `		/* Append the chunk and the separator */` |
|     13 |  649 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - |  650 | `		/* Point beyond the chunk */` |
|     13 |  651 | `		zIn += nChunkLen;` |
|      1 |  652 | `	}` |
|      5 |  653 | `	return PH7_OK;` |
|      8 |  654 | `}` |
|      - |  655 | `/*` |
|      - |  656 | ` * string addslashes(string $str)` |
|      - |  657 | ` *  Quote string with slashes.` |
|      - |  658 | ` *  Returns a string with backslashes before characters that need` |
|      - |  659 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  660 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  661 | ` * Parameter` |
|      - |  662 | ` *  str: The string to be escaped.` |
|      - |  663 | ` * Return` |
|      - |  664 | ` *  Returns the escaped string` |
|      - |  665 | ` */` |
|     24 |  666 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  667 | `{` |
|      - |  668 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  669 | `	int nLen;` |
|      - |  670 | `	/* PHP enforces exactly one argument. */` |
|     28 |  671 | `	if( nArg != 1 ){` |
|      8 |  672 | `		return PH7_VmThrowException(pCtx,` |
|      - |  673 | `			"ArgumentCountError",` |
|      - |  674 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 |  675 | `			nArg` |
|      - |  676 | `			);` |
|      - |  677 | `	}` |
|      - |  678 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - |  679 | `	 * types still produce a TypeError. */` |
|     22 |  680 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 |  681 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  682 | `			E_DEPRECATED,` |
|      - |  683 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  684 | `			);` |
|      - |  685 | `		/* fall through so conversion below yields empty string */` |
|      1 |  686 | `	}` |
|      - |  687 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     29 |  688 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 |  689 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 |  690 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 |  691 | `		return PH7_VmThrowException(pCtx,` |
|      - |  692 | `			"TypeError",` |
|      - |  693 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  694 | `			ph7_type_name(apArg[0])` |
|      - |  695 | `			);` |
|      - |  696 | `	}` |
|      - |  697 | `	/* Convert to string representation first and obtain length. */` |
|     19 |  698 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 |  699 | `	if( nLen < 1 ){` |
|      - |  700 | `		/* Return the empty string */` |
|      5 |  701 | `		ph7_result_string(pCtx,"",0);` |
|      5 |  702 | `		return PH7_OK;` |
|      - |  703 | `	}` |
|     15 |  704 | `	zEnd = &zIn[nLen];` |
|     15 |  705 | `	zCur = 0; /* cc warning */` |
|     20 |  706 | `	for(;;){` |
|     41 |  707 | `		if( zIn >= zEnd ){` |
|      - |  708 | `			/* No more input */` |
|     15 |  709 | `			break;` |
|      - |  710 | `		}` |
|     27 |  711 | `		zCur = zIn;` |
|      - |  712 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 |  713 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 |  714 | `			zIn++;` |
|      1 |  715 | `		}` |
|     27 |  716 | `		if( zIn > zCur ){` |
|      - |  717 | `			/* Append raw contents */` |
|     23 |  718 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 |  719 | `		}` |
|     27 |  720 | `		if( zIn < zEnd ){` |
|     17 |  721 | `			int c = zIn[0];` |
|     17 |  722 | `			if( c == '\0' ){` |
|      - |  723 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 |  724 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 |  725 | `			}else{` |
|     15 |  726 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  727 | `			}` |
|      8 |  728 | `		}` |
|     27 |  729 | `		zIn++;` |
|      1 |  730 | `	}` |
|     15 |  731 | `	return PH7_OK;` |
|     16 |  732 | `}` |
|      - |  733 | `/*` |
|      - |  734 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - |  735 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - |  736 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - |  737 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - |  738 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - |  739 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - |  740 | ` * [zList, zList+nLen).` |
|      - |  741 | ` *` |
|      - |  742 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - |  743 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - |  744 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - |  745 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - |  746 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - |  747 | ` */` |
|     78 |  748 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      3 |  749 | `{` |
|     81 |  750 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|     81 |  751 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|     81 |  752 | `	SyZero(aMask,256);` |
|    291 |  753 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    213 |  754 | `		int c = zIn[0];` |
|    213 |  755 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - |  756 | `			/* Valid incrementing range c..zIn[3] */` |
|     20 |  757 | `			int hi = zIn[3],k;` |
|    364 |  758 | `			for( k = c ; k <= hi ; k++ ){` |
|    346 |  759 | `				aMask[k] = 1;` |
|    174 |  760 | `			}` |
|     20 |  761 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    213 |  762 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - |  763 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - |  764 | `			const char *zMsg;` |
|     20 |  765 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 |  766 | `				zMsg = "no character to the left of '..'";` |
|     18 |  767 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 |  768 | `				zMsg = "no character to the right of '..'";` |
|     14 |  769 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 |  770 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 |  771 | `			}else{` |
|    ! 0 |  772 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - |  773 | `			}` |
|     20 |  774 | `			if( zMsg ){` |
|     29 |  775 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 |  776 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 |  777 | `			}else{` |
|    ! 0 |  778 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  779 | `					"Invalid '..'-range");` |
|      - |  780 | `			}` |
|      - |  781 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - |  782 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 |  783 | `		}else{` |
|    177 |  784 | `			aMask[c] = 1;` |
|      - |  785 | `		}` |
|    108 |  786 | `	}` |
|     81 |  787 | `}` |
|      - |  788 | `/*` |
|      - |  789 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  790 | ` *  Quote string with slashes in a C style.` |
|      - |  791 | ` * Parameter` |
|      - |  792 | ` *  $str:` |
|      - |  793 | ` *    The string to be escaped.` |
|      - |  794 | ` *  $charlist:` |
|      - |  795 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  796 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  797 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  798 | ` * Return` |
|      - |  799 | ` *  Returns the escaped string.` |
|      - |  800 | ` * Note:` |
|      - |  801 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - |  802 | ` */` |
|     40 |  803 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  804 | `{` |
|      - |  805 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  806 | `	char aMask[256];` |
|      - |  807 | `	int nLen,nMask;` |
|      - |  808 | `	/* PHP enforces exactly two arguments. */` |
|     45 |  809 | `	if( nArg != 2 ){` |
|      8 |  810 | `		return PH7_VmThrowException(pCtx,` |
|      - |  811 | `			"ArgumentCountError",` |
|      - |  812 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  813 | `			nArg` |
|      - |  814 | `			);` |
|      - |  815 | `	}` |
|      - |  816 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  817 | `	 * treated as the empty string (PHP 8.1). */` |
|     40 |  818 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  819 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  820 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  821 | `			E_DEPRECATED,` |
|      - |  822 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  823 | `			);` |
|      - |  824 | `		/* treat as empty string; fall through to conversion logic */` |
|     52 |  825 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     52 |  826 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     32 |  827 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  828 | `		return PH7_VmThrowException(pCtx,` |
|      - |  829 | `			"TypeError",` |
|      - |  830 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  831 | `			ph7_type_name(apArg[0])` |
|      - |  832 | `			);` |
|      - |  833 | `	}` |
|      - |  834 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  835 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  836 | `	 * trigger a TypeError. */` |
|     37 |  837 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  838 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  839 | `			E_DEPRECATED,` |
|      - |  840 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  841 | `			);` |
|      - |  842 | `		/* allow through so it becomes empty string below */` |
|     49 |  843 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 |  844 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 |  845 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  846 | `		return PH7_VmThrowException(pCtx,` |
|      - |  847 | `			"TypeError",` |
|      - |  848 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  849 | `			ph7_type_name(apArg[1])` |
|      - |  850 | `			);` |
|      - |  851 | `	}` |
|      - |  852 | `	/* Extract the string to process */` |
|     35 |  853 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  854 | `	/* NULL would never reach here due to the check above. */` |
|     35 |  855 | `	if( nLen < 1 ){` |
|      - |  856 | `		/* Empty string returns itself. */` |
|      5 |  857 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  858 | `		return PH7_OK;` |
|      - |  859 | `	}` |
|      - |  860 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 |  861 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 |  862 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 |  863 | `	zEnd = &zIn[nLen];` |
|     31 |  864 | `	zCur = 0; /* cc warning */` |
|     37 |  865 | `	for(;;){` |
|     77 |  866 | `		if( zIn >= zEnd ){` |
|      - |  867 | `			/* No more input */` |
|     31 |  868 | `			break;` |
|      - |  869 | `		}` |
|     49 |  870 | `		zCur = zIn;` |
|    125 |  871 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 |  872 | `			zIn++;` |
|      3 |  873 | `		}` |
|     49 |  874 | `		if( zIn > zCur ){` |
|      - |  875 | `			/* Append raw contents */` |
|     43 |  876 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 |  877 | `		}` |
|     49 |  878 | `		if( zIn < zEnd ){` |
|      - |  879 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  880 | `			 * on platforms where char is signed. */` |
|     29 |  881 | `			int c = (unsigned char)zIn[0];` |
|      - |  882 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  883 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  884 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 |  885 | `			if( c == '\n' ){` |
|      3 |  886 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 |  887 | `			}else if( c == '\r' ){` |
|      3 |  888 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 |  889 | `			}else if( c == '\t' ){` |
|      3 |  890 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 |  891 | `			}else if( c == '\v' ){` |
|      3 |  892 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 |  893 | `			}else if( c == '\f' ){` |
|      3 |  894 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 |  895 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  896 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  897 | `				 * octal escapes (\001 not \1). */` |
|      7 |  898 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  899 | `			}else{` |
|     13 |  900 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  901 | `			}` |
|     13 |  902 | `		}` |
|     49 |  903 | `		zIn++;` |
|      3 |  904 | `	}` |
|     31 |  905 | `	return PH7_OK;` |
|     25 |  906 | `}` |
|      - |  907 | `/*` |
|      - |  908 | ` * string quotemeta(string $str)` |
|      - |  909 | ` *  Quote meta characters.` |
|      - |  910 | ` * Parameter` |
|      - |  911 | ` *  $str:` |
|      - |  912 | ` *    The string to be escaped.` |
|      - |  913 | ` * Return` |
|      - |  914 | ` *  Returns the escaped string.` |
|      - |  915 | `*/` |
|     10 |  916 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  917 | `{` |
|      - |  918 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  919 | `	char aMask[256];` |
|      - |  920 | `	int nLen;` |
|     12 |  921 | `	if( nArg < 1 ){` |
|      - |  922 | `		/* Nothing to process,retun NULL */` |
|    ! 0 |  923 | `		ph7_result_null(pCtx);` |
|    ! 0 |  924 | `		return PH7_OK;` |
|      - |  925 | `	}` |
|      - |  926 | `	/* Extract the string to process */` |
|     12 |  927 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 |  928 | `	if( nLen < 1 ){` |
|      - |  929 | `		/* Return the empty string */` |
|      3 |  930 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  931 | `		return PH7_OK;` |
|      - |  932 | `	}` |
|      - |  933 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 |  934 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 |  935 | `	zEnd = &zIn[nLen];` |
|     10 |  936 | `	zCur = 0; /* cc warning */` |
|     22 |  937 | `	for(;;){` |
|     46 |  938 | `		if( zIn >= zEnd ){` |
|      - |  939 | `			/* No more input */` |
|     10 |  940 | `			break;` |
|      - |  941 | `		}` |
|     38 |  942 | `		zCur = zIn;` |
|     76 |  943 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 |  944 | `			zIn++;` |
|      2 |  945 | `		}` |
|     38 |  946 | `		if( zIn > zCur ){` |
|      - |  947 | `			/* Append raw contents */` |
|     20 |  948 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 |  949 | `		}` |
|     38 |  950 | `		if( zIn < zEnd ){` |
|     36 |  951 | `			int c = zIn[0];` |
|     36 |  952 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 |  953 | `		}` |
|     38 |  954 | `		zIn++;` |
|      2 |  955 | `	}` |
|     10 |  956 | `	return PH7_OK;` |
|      7 |  957 | `}` |
|      - |  958 | `/*` |
|      - |  959 | ` * string stripslashes(string $str)` |
|      - |  960 | ` *  Un-quotes a quoted string.` |
|      - |  961 | ` *  Returns a string with backslashes before characters that need` |
|      - |  962 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  963 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  964 | ` * Parameter` |
|      - |  965 | ` *  $str` |
|      - |  966 | ` *   The input string.` |
|      - |  967 | ` * Return` |
|      - |  968 | ` *  Returns a string with backslashes stripped off.` |
|      - |  969 | ` */` |
|      6 |  970 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  971 | `{` |
|      - |  972 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  973 | `	int nLen;` |
|      7 |  974 | `	if( nArg < 1 ){` |
|      - |  975 | `		/* Nothing to process,retun NULL */` |
|    ! 0 |  976 | `		ph7_result_null(pCtx);` |
|    ! 0 |  977 | `		return PH7_OK;` |
|      - |  978 | `	}` |
|      - |  979 | `	/* Extract the string to process */` |
|      7 |  980 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  981 | `	if( zIn == 0 ){` |
|    ! 0 |  982 | `		ph7_result_null(pCtx);` |
|    ! 0 |  983 | `		return PH7_OK;` |
|      - |  984 | `	}` |
|      7 |  985 | `	zEnd = &zIn[nLen];` |
|      7 |  986 | `	zCur = 0; /* cc warning */` |
|      - |  987 | `	/* Encode the string */` |
|      4 |  988 | `	for(;;){` |
|      9 |  989 | `		if( zIn >= zEnd ){` |
|      - |  990 | `			/* No more input */` |
|      5 |  991 | `			break;` |
|      - |  992 | `		}` |
|      5 |  993 | `		zCur = zIn;` |
|     17 |  994 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  995 | `			zIn++;` |
|      1 |  996 | `		}` |
|      5 |  997 | `		if( zIn > zCur ){` |
|      - |  998 | `			/* Append raw contents */` |
|      5 |  999 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1000 | `		}` |
|      5 | 1001 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1002 | `			int c = zIn[1];` |
|      3 | 1003 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1004 | `				/* Ignore the backslash */` |
|      3 | 1005 | `				zIn++;` |
|      1 | 1006 | `			}` |
|      2 | 1007 | `		}else{` |
|      3 | 1008 | `			break;` |
|      - | 1009 | `		}` |
|      1 | 1010 | `	}` |
|      7 | 1011 | `	return PH7_OK;` |
|      4 | 1012 | `}` |
|      - | 1013 | `/*` |
|      - | 1014 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1015 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1016 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1017 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1018 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1019 | ` * (PLAN.md §6) so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1020 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1021 | ` *` |
|      - | 1022 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1023 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1024 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1025 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1026 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1027 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1028 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1029 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1030 | ` */` |
|      - | 1031 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1032 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1033 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1034 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1035 | `/*` |
|      - | 1036 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1037 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1038 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1039 | ` * Return` |
|      - | 1040 | ` *  The escaped string or NULL on failure.` |
|      - | 1041 | ` */` |
|     42 | 1042 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1043 | `{` |
|     43 | 1044 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1045 | `	const char *zIn;` |
|     43 | 1046 | `	int nLen,bDouble = 1;` |
|     43 | 1047 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1048 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1049 | `		ph7_result_null(pCtx);` |
|      3 | 1050 | `		return PH7_OK;` |
|      - | 1051 | `	}` |
|      - | 1052 | `	/* Extract the target string */` |
|     41 | 1053 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 1054 | `	if( nArg > 1 ){` |
|     35 | 1055 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1056 | `	}` |
|     41 | 1057 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     41 | 1058 | `	if( nArg > 3 ){` |
|      7 | 1059 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1060 | `	}` |
|     41 | 1061 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     41 | 1062 | `	return PH7_OK;` |
|     22 | 1063 | `}` |
|      - | 1064 | `/*` |
|      - | 1065 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1066 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1067 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1068 | ` * Return` |
|      - | 1069 | ` *  The unescaped string or NULL on failure.` |
|      - | 1070 | ` */` |
|     22 | 1071 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1072 | `{` |
|     23 | 1073 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1074 | `	const char *zIn;` |
|      - | 1075 | `	int nLen;` |
|     23 | 1076 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1077 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1078 | `		ph7_result_null(pCtx);` |
|      3 | 1079 | `		return PH7_OK;` |
|      - | 1080 | `	}` |
|      - | 1081 | `	/* Extract the target string */` |
|     21 | 1082 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     21 | 1083 | `	if( nArg > 1 ){` |
|      9 | 1084 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1085 | `	}` |
|     21 | 1086 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     21 | 1087 | `	return PH7_OK;` |
|     12 | 1088 | `}` |
|      - | 1089 | `/*` |
|      - | 1090 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1091 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1092 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1093 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1094 | ` * Return` |
|      - | 1095 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1096 | ` */` |
|     12 | 1097 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1098 | `{` |
|     13 | 1099 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1100 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1101 | `	if( nArg > 0 ){` |
|     11 | 1102 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1103 | `	}` |
|     13 | 1104 | `	if( nArg > 1 ){` |
|      9 | 1105 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1106 | `	}` |
|     13 | 1107 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1108 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1109 | `	return PH7_OK;` |
|      1 | 1110 | `}` |
|      - | 1111 | `/*` |
|      - | 1112 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1113 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1114 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1115 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1116 | ` * Return` |
|      - | 1117 | ` *  The encoded string or NULL on failure.` |
|      - | 1118 | ` */` |
|     30 | 1119 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1120 | `{` |
|     31 | 1121 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1122 | `	const char *zIn;` |
|     31 | 1123 | `	int nLen,bDouble = 1;` |
|     31 | 1124 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1125 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1126 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1127 | `		return PH7_OK;` |
|      - | 1128 | `	}` |
|      - | 1129 | `	/* Extract the target string */` |
|     31 | 1130 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1131 | `	if( nArg > 1 ){` |
|     19 | 1132 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1133 | `	}` |
|     31 | 1134 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1135 | `	if( nArg > 3 ){` |
|      3 | 1136 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1137 | `	}` |
|     31 | 1138 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1139 | `	return PH7_OK;` |
|     16 | 1140 | `}` |
|      - | 1141 | `/*` |
|      - | 1142 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1143 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1144 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1145 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1146 | ` * Return` |
|      - | 1147 | ` *  The decoded string or NULL on failure.` |
|      - | 1148 | ` */` |
|     58 | 1149 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1150 | `{` |
|     59 | 1151 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1152 | `	const char *zIn;` |
|      - | 1153 | `	int nLen;` |
|     59 | 1154 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1155 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1156 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1157 | `		return PH7_OK;` |
|      - | 1158 | `	}` |
|      - | 1159 | `	/* Extract the target string */` |
|     59 | 1160 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1161 | `	if( nArg > 1 ){` |
|     27 | 1162 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1163 | `	}` |
|     59 | 1164 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1165 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 1166 | `	return PH7_OK;` |
|     30 | 1167 | `}` |
|      - | 1168 | `/*` |
|      - | 1169 | ` * int strlen($string)` |
|      - | 1170 | ` *  return the length of the given string.` |
|      - | 1171 | ` * Parameter` |
|      - | 1172 | ` *  string: The string being measured for length.` |
|      - | 1173 | ` * Return` |
|      - | 1174 | ` *  length of the given string.` |
|      - | 1175 | ` */` |
|   9640 | 1176 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1177 | `{` |
|   9645 | 1178 | `	int iLen = 0;` |
|   9645 | 1179 | `	if( nArg > 0 ){` |
|   9645 | 1180 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   4820 | 1181 | `	}` |
|      - | 1182 | `	/* String length */` |
|   9645 | 1183 | `	ph7_result_int(pCtx,iLen);` |
|   9645 | 1184 | `	return PH7_OK;` |
|      5 | 1185 | `}` |
|      - | 1186 | `/*` |
|      - | 1187 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1188 | ` *  Perform a binary safe string comparison.` |
|      - | 1189 | ` * Parameter` |
|      - | 1190 | ` *  str1: The first string` |
|      - | 1191 | ` *  str2: The second string` |
|      - | 1192 | ` * Return` |
|      - | 1193 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1194 | ` *  than str2, and 0 if they are equal.` |
|      - | 1195 | ` */` |
|     72 | 1196 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1197 | `{` |
|      - | 1198 | `	const char *z1,*z2;` |
|      - | 1199 | `	int n1,n2;` |
|      - | 1200 | `	int res;` |
|     73 | 1201 | `	if( nArg < 2 ){` |
|    ! 0 | 1202 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1203 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1204 | `		return PH7_OK;` |
|      - | 1205 | `	}` |
|      - | 1206 | `	/* Perform the comparison */` |
|     73 | 1207 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 1208 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 1209 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1210 | `	/* Comparison result */` |
|     73 | 1211 | `	ph7_result_int(pCtx,res);` |
|     73 | 1212 | `	return PH7_OK;` |
|     37 | 1213 | `}` |
|      - | 1214 | `/*` |
|      - | 1215 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1216 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1217 | ` * Parameter` |
|      - | 1218 | ` *  str1: The first string` |
|      - | 1219 | ` *  str2: The second string` |
|      - | 1220 | ` * Return` |
|      - | 1221 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1222 | ` *  than str2, and 0 if they are equal.` |
|      - | 1223 | ` */` |
|     16 | 1224 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1225 | `{` |
|      - | 1226 | `	const char *z1,*z2;` |
|      - | 1227 | `	int res;` |
|      - | 1228 | `	int n;` |
|     17 | 1229 | `	if( nArg < 3 ){` |
|      - | 1230 | `		/* Perform a standard comparison */` |
|    ! 0 | 1231 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1232 | `	}` |
|      - | 1233 | `	/* Desired comparison length */` |
|     17 | 1234 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1235 | `	if( n < 0 ){` |
|      - | 1236 | `		/* Invalid length */` |
|      3 | 1237 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1238 | `		return PH7_OK;` |
|      - | 1239 | `	}` |
|      - | 1240 | `	/* Perform the comparison */` |
|     15 | 1241 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1242 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1243 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1244 | `	/* Comparison result */` |
|     15 | 1245 | `	ph7_result_int(pCtx,res);` |
|     15 | 1246 | `	return PH7_OK;` |
|      9 | 1247 | `}` |
|      - | 1248 | `/*` |
|      - | 1249 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1250 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1251 | ` * Parameter` |
|      - | 1252 | ` *  str1: The first string` |
|      - | 1253 | ` *  str2: The second string` |
|      - | 1254 | ` * Return` |
|      - | 1255 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1256 | ` *  than str2, and 0 if they are equal.` |
|      - | 1257 | ` */` |
|     14 | 1258 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1259 | `{` |
|      - | 1260 | `	const char *z1,*z2;` |
|      - | 1261 | `	int n1,n2;` |
|      - | 1262 | `	int res;` |
|     15 | 1263 | `	if( nArg < 2 ){` |
|    ! 0 | 1264 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1265 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1266 | `		return PH7_OK;` |
|      - | 1267 | `	}` |
|      - | 1268 | `	/* Perform the comparison */` |
|     15 | 1269 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1270 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1271 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1272 | `	/* Comparison result */` |
|     15 | 1273 | `	ph7_result_int(pCtx,res);` |
|     15 | 1274 | `	return PH7_OK;` |
|      8 | 1275 | `}` |
|      - | 1276 | `/*` |
|      - | 1277 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1278 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1279 | ` * Parameter` |
|      - | 1280 | ` *  $str1: The first string` |
|      - | 1281 | ` *  $str2: The second string` |
|      - | 1282 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1283 | ` * Return` |
|      - | 1284 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1285 | ` *  than str2, and 0 if they are equal.` |
|      - | 1286 | ` */` |
|      4 | 1287 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1288 | `{` |
|      - | 1289 | `	const char *z1,*z2;` |
|      - | 1290 | `	int res;` |
|      - | 1291 | `	int n;` |
|      5 | 1292 | `	if( nArg < 3 ){` |
|      - | 1293 | `		/* Perform a standard comparison */` |
|    ! 0 | 1294 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1295 | `	}` |
|      - | 1296 | `	/* Desired comparison length */` |
|      5 | 1297 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1298 | `	if( n < 0 ){` |
|      - | 1299 | `		/* Invalid length */` |
|    ! 0 | 1300 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1301 | `		return PH7_OK;` |
|      - | 1302 | `	}` |
|      - | 1303 | `	/* Perform the comparison */` |
|      5 | 1304 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1305 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1306 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1307 | `	/* Comparison result */` |
|      5 | 1308 | `	ph7_result_int(pCtx,res);` |
|      5 | 1309 | `	return PH7_OK;` |
|      3 | 1310 | `}` |
|      - | 1311 | `/*` |
|      - | 1312 | ` * Implode context [i.e: it's private data].` |
|      - | 1313 | ` * A pointer to the following structure is forwarded` |
|      - | 1314 | ` * verbatim to the array walker callback defined below.` |
|      - | 1315 | ` */` |
|      - | 1316 | `struct implode_data {` |
|      - | 1317 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1318 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1319 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1320 | `	int nSeplen;          /* Separator length */` |
|      - | 1321 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1322 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1323 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1324 | `};` |
|      - | 1325 | `/*` |
|      - | 1326 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1327 | ` * The following routine is invoked for each array entry passed` |
|      - | 1328 | ` * to the implode() function.` |
|      - | 1329 | ` */` |
| 133282 | 1330 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1331 | `{` |
|  66641 | 1332 | `	SXUNUSED(pKey);` |
| 133287 | 1333 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1334 | `	const char *zData;` |
|      - | 1335 | `	int nLen;` |
| 133287 | 1336 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1337 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1338 | `			if( !pData->bFirst ){` |
|      - | 1339 | `				/* append the separator first */` |
|      3 | 1340 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1341 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1342 | `					return PH7_ABORT;` |
|      - | 1343 | `				}` |
|      2 | 1344 | `			}else{` |
|    ! 0 | 1345 | `				pData->bFirst = 0;` |
|      - | 1346 | `			}` |
|      1 | 1347 | `		}` |
|      - | 1348 | `		/* Recurse */` |
|      3 | 1349 | `		pData->bFirst = 1;` |
|      3 | 1350 | `		pData->nRecCount++;` |
|      3 | 1351 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1352 | `		pData->nRecCount--;` |
|      - | 1353 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1354 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1355 | `			return PH7_ABORT;` |
|      - | 1356 | `		}` |
|      3 | 1357 | `		return PH7_OK;` |
|      - | 1358 | `	}` |
|      - | 1359 | `	/* Extract the string representation of the entry value */` |
| 133285 | 1360 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1361 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 133285 | 1362 | `	if( pData->bFirst ){` |
|  32217 | 1363 | `		pData->bFirst = 0;` |
| 117179 | 1364 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1365 | `		/* append the separator first */` |
| 101061 | 1366 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1367 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1368 | `			return PH7_ABORT;` |
|      - | 1369 | `		}` |
|  50528 | 1370 | `	}` |
|      - | 1371 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 133285 | 1372 | `	if( nLen > 0 ){` |
| 121597 | 1373 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1374 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1375 | `			return PH7_ABORT;` |
|      - | 1376 | `		}` |
|  60796 | 1377 | `	}` |
| 133285 | 1378 | `	return PH7_OK;` |
|  66646 | 1379 | `}` |
|      - | 1380 | `/*` |
|      - | 1381 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1382 | ` * string implode(array $pieces,...)` |
|      - | 1383 | ` *  Join array elements with a string.` |
|      - | 1384 | ` * $glue` |
|      - | 1385 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1386 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1387 | ` * $pieces` |
|      - | 1388 | ` *   The array of strings to implode.` |
|      - | 1389 | ` * Return` |
|      - | 1390 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1391 | ` *  order, with the glue string between each element.` |
|      - | 1392 | ` */` |
|  32234 | 1393 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1394 | `{` |
|      - | 1395 | `	struct implode_data imp_data;` |
|  32239 | 1396 | `	int i = 1;` |
|  32239 | 1397 | `	if( nArg < 1 ){` |
|      - | 1398 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1399 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1400 | `		return PH7_OK;` |
|      - | 1401 | `	}` |
|      - | 1402 | `	/* Prepare the implode context */` |
|  32239 | 1403 | `	imp_data.pCtx = pCtx;` |
|  32239 | 1404 | `	imp_data.bRecursive = 0;` |
|  32239 | 1405 | `	imp_data.bFirst = 1;` |
|  32239 | 1406 | `	imp_data.nRecCount = 0;` |
|  32239 | 1407 | `	imp_data.rc = SXRET_OK;` |
|  32239 | 1408 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  32237 | 1409 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16121 | 1410 | `	}else{` |
|      3 | 1411 | `		imp_data.zSep = 0;` |
|      3 | 1412 | `		imp_data.nSeplen = 0;` |
|      3 | 1413 | `		i = 0;` |
|      - | 1414 | `	}` |
|  32239 | 1415 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1416 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1417 | `	}` |
|      - | 1418 | `	/* Start the 'join' process */` |
|  64473 | 1419 | `	while( i < nArg ){` |
|  32239 | 1420 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1421 | `			/* Iterate throw array entries */` |
|  32239 | 1422 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1423 | `			/* Surface a callback allocation failure as a fatal */` |
|  32239 | 1424 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1425 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1426 | `			}` |
|  16122 | 1427 | `		}else{` |
|      - | 1428 | `			const char *zData;` |
|      - | 1429 | `			int nLen;` |
|      - | 1430 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1431 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1432 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1433 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1434 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1435 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1436 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1437 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1438 | `				}` |
|    ! 0 | 1439 | `			}` |
|      - | 1440 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1441 | `			if( nLen > 0 ){` |
|    ! 0 | 1442 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1443 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1444 | `				}` |
|    ! 0 | 1445 | `			}` |
|      - | 1446 | `		}` |
|  32239 | 1447 | `		i++;` |
|      5 | 1448 | `	}` |
|  32239 | 1449 | `	return PH7_OK;` |
|  16122 | 1450 | `}` |
|      - | 1451 | `/*` |
|      - | 1452 | ` * Symisc eXtension:` |
|      - | 1453 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1454 | ` * Purpose` |
|      - | 1455 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1456 | ` * Example:` |
|      - | 1457 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1458 | ` *   echo implode_recursive("/",$a);` |
|      - | 1459 | ` *   Will output` |
|      - | 1460 | ` *     usr/home/dean.` |
|      - | 1461 | ` *   While the standard implode would produce.` |
|      - | 1462 | ` *    usr/Array.` |
|      - | 1463 | ` * Parameter` |
|      - | 1464 | ` *  Refer to implode().` |
|      - | 1465 | ` * Return` |
|      - | 1466 | ` *  Refer to implode().` |
|      - | 1467 | ` */` |
|     12 | 1468 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1469 | `{` |
|      - | 1470 | `	struct implode_data imp_data;` |
|     13 | 1471 | `	int i = 1;` |
|     13 | 1472 | `	if( nArg < 1 ){` |
|      - | 1473 | `		/* Missing argument,return NULL */` |
|      3 | 1474 | `		ph7_result_null(pCtx);` |
|      3 | 1475 | `		return PH7_OK;` |
|      - | 1476 | `	}` |
|      - | 1477 | `	/* Prepare the implode context */` |
|     11 | 1478 | `	imp_data.pCtx = pCtx;` |
|     11 | 1479 | `	imp_data.bRecursive = 1;` |
|     11 | 1480 | `	imp_data.bFirst = 1;` |
|     11 | 1481 | `	imp_data.nRecCount = 0;` |
|     11 | 1482 | `	imp_data.rc = SXRET_OK;` |
|     11 | 1483 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1484 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1485 | `	}else{` |
|    ! 0 | 1486 | `		imp_data.zSep = 0;` |
|    ! 0 | 1487 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1488 | `		i = 0;` |
|      - | 1489 | `	}` |
|     11 | 1490 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1491 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1492 | `	}` |
|      - | 1493 | `	/* Start the 'join' process */` |
|     21 | 1494 | `	while( i < nArg ){` |
|     11 | 1495 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1496 | `			/* Iterate throw array entries */` |
|      3 | 1497 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1498 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 1499 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1500 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1501 | `			}` |
|      2 | 1502 | `		}else{` |
|      - | 1503 | `			const char *zData;` |
|      - | 1504 | `			int nLen;` |
|      - | 1505 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1506 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1507 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1508 | `			if( imp_data.bFirst ){` |
|      9 | 1509 | `				imp_data.bFirst = 0;` |
|      4 | 1510 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1511 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1512 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1513 | `				}` |
|    ! 0 | 1514 | `			}` |
|      - | 1515 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1516 | `			if( nLen > 0 ){` |
|      9 | 1517 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1518 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1519 | `				}` |
|      4 | 1520 | `			}` |
|      - | 1521 | `		}` |
|     11 | 1522 | `		i++;` |
|      1 | 1523 | `	}` |
|     11 | 1524 | `	return PH7_OK;` |
|      7 | 1525 | `}` |
|      - | 1526 | `/*` |
|      - | 1527 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1528 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1529 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1530 | ` * Parameters` |
|      - | 1531 | ` *  $delimiter` |
|      - | 1532 | ` *   The boundary string.` |
|      - | 1533 | ` * $string` |
|      - | 1534 | ` *   The input string.` |
|      - | 1535 | ` * $limit` |
|      - | 1536 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1537 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1538 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1539 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1540 | ` * Returns` |
|      - | 1541 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1542 | ` *  on boundaries formed by the delimiter.` |
|      - | 1543 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1544 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1545 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1546 | ` *  will be returned.` |
|      - | 1547 | ` * NOTE:` |
|      - | 1548 | ` *  Negative limit is not supported.` |
|      - | 1549 | ` */` |
|   6244 | 1550 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1551 | `{` |
|      - | 1552 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1553 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1554 | `	ph7_value *pArray;` |
|      - | 1555 | `	ph7_value *pValue;` |
|      - | 1556 | `	sxu32 nOfft;` |
|      - | 1557 | `	sxi32 rc;` |
|   6249 | 1558 | `	if( nArg < 2 ){` |
|      - | 1559 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 1560 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1561 | `		return PH7_OK;` |
|      - | 1562 | `	}` |
|      - | 1563 | `	/* Extract the delimiter */` |
|   6249 | 1564 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6249 | 1565 | `	if( nDelim < 1 ){` |
|      - | 1566 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      3 | 1567 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1568 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 1569 | `	}` |
|      - | 1570 | `	/* Extract the string */` |
|   6247 | 1571 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6247 | 1572 | `	if( nStrlen < 1 ){` |
|      - | 1573 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 1574 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 1575 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 1576 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 1577 | `		if( pArrayTmp == 0 ){` |
|      - | 1578 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1579 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1580 | `			return PH7_OK;` |
|      - | 1581 | `		}` |
|      7 | 1582 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 1583 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 1584 | `			if( pValueTmp == 0 ){` |
|      - | 1585 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 1586 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 1587 | `				return PH7_OK;` |
|      - | 1588 | `			}` |
|      5 | 1589 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 1590 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 1591 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1592 | `			}` |
|      2 | 1593 | `		}` |
|      7 | 1594 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 1595 | `		return PH7_OK;` |
|      - | 1596 | `	}` |
|      - | 1597 | `	/* Point to the end of the string */` |
|   6241 | 1598 | `	zEnd = &zString[nStrlen];` |
|      - | 1599 | `	/* Create the array */` |
|   6241 | 1600 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6241 | 1601 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6241 | 1602 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1603 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1604 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1605 | `		return PH7_OK;` |
|      - | 1606 | `	}` |
|      - | 1607 | `	/* Set a defualt limit */` |
|   6241 | 1608 | `	iLimit = SXI32_HIGH;` |
|   6241 | 1609 | `	if( nArg > 2 ){` |
|     38 | 1610 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 1611 | `		if( iLimit < 0 ){` |
|      - | 1612 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 1613 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 1614 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 1615 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 1616 | `			int nTotal = 1,nKeep;` |
|     17 | 1617 | `			const char *zScan = zString;` |
|      - | 1618 | `			sxu32 nScanOfft;` |
|     57 | 1619 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 1620 | `				nTotal++;` |
|     41 | 1621 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 1622 | `			}` |
|     17 | 1623 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 1624 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 1625 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 1626 | `				/* Emit the next clean component */` |
|     23 | 1627 | `				zCur = &zString[nOfft];` |
|     23 | 1628 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 1629 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1630 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1631 | `				}` |
|     23 | 1632 | `				zString = &zCur[nDelim];` |
|     23 | 1633 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 1634 | `			}` |
|     17 | 1635 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 1636 | `			return PH7_OK;` |
|      - | 1637 | `		}` |
|     22 | 1638 | `		if( iLimit == 0 ){` |
|      5 | 1639 | `			iLimit = 1;` |
|      2 | 1640 | `		}` |
|     22 | 1641 | `		iLimit--;` |
|      9 | 1642 | `	}` |
|      - | 1643 | `	/* Start exploding */` |
|  72137 | 1644 | `	for(;;){` |
| 144279 | 1645 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 144279 | 1646 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1647 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6225 | 1648 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6225 | 1649 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1650 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1651 | `			}` |
|   6225 | 1652 | `			break;` |
|      - | 1653 | `		}` |
|      - | 1654 | `		/* Point to the desired offset */` |
| 138059 | 1655 | `		zCur = &zString[nOfft];` |
|      - | 1656 | `		/* Perform the store operation (may be empty) */` |
| 138059 | 1657 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 138059 | 1658 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1659 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1660 | `		}` |
|      - | 1661 | `		/* Point beyond the delimiter */` |
| 138059 | 1662 | `		zString = &zCur[nDelim];` |
|      - | 1663 | `		/* Reset the cursor */` |
| 138059 | 1664 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1665 | `	}` |
|      - | 1666 | `	/* Return the freshly created array */` |
|   6225 | 1667 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1668 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1669 | `	 * released as soon we return from this foregin function.` |
|      - | 1670 | `	 */` |
|   6225 | 1671 | `	return PH7_OK;` |
|   3127 | 1672 | `}` |
|      - | 1673 | `/*` |
|      - | 1674 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1675 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1676 | ` * Parameters` |
|      - | 1677 | ` *  $str` |
|      - | 1678 | ` *   The string that will be trimmed.` |
|      - | 1679 | ` * $charlist` |
|      - | 1680 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1681 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1682 | ` *   With .. you can specify a range of characters.` |
|      - | 1683 | ` * Returns.` |
|      - | 1684 | ` *  Thr processed string.` |
|      - | 1685 | ` * NOTE:` |
|      - | 1686 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1687 | ` */` |
|  13814 | 1688 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1689 | `{` |
|      - | 1690 | `	const char *zString;` |
|      - | 1691 | `	int nLen;` |
|  13819 | 1692 | `	if( nArg < 1 ){` |
|      - | 1693 | `		/* Missing arguments,return null */` |
|    ! 0 | 1694 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1695 | `		return PH7_OK;` |
|      - | 1696 | `	}` |
|      - | 1697 | `	/* Extract the target string */` |
|  13819 | 1698 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  13819 | 1699 | `	if( nLen < 1 ){` |
|      - | 1700 | `		/* Empty string,return */` |
|   1347 | 1701 | `		ph7_result_string(pCtx,"",0);` |
|   1347 | 1702 | `		return PH7_OK;` |
|      - | 1703 | `	}` |
|      - | 1704 | `	/* Start the trim process */` |
|  12477 | 1705 | `	if( nArg < 2 ){` |
|      - | 1706 | `		SyString sStr;` |
|      - | 1707 | `		/* Remove white spaces and NUL bytes */` |
|  12447 | 1708 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  31071 | 1709 | `		SyStringFullTrimSafe(&sStr);` |
|  12447 | 1710 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6226 | 1711 | `	}else{` |
|      - | 1712 | `		/* Char list */` |
|      - | 1713 | `		const char *zList;` |
|      - | 1714 | `		int nListlen;` |
|     33 | 1715 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 1716 | `		if( nListlen < 1 ){` |
|      - | 1717 | `			/* Return the string unchanged */` |
|      6 | 1718 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 1719 | `		}else{` |
|      - | 1720 | `			char aMask[256];` |
|     29 | 1721 | `			const char *zEnd = &zString[nLen];` |
|     29 | 1722 | `			const char *zCur = zString;` |
|     29 | 1723 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1724 | `			/* Left trim */` |
|     79 | 1725 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 1726 | `				zCur++;` |
|      3 | 1727 | `			}` |
|      - | 1728 | `			/* Right trim */` |
|     79 | 1729 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 1730 | `				zEnd--;` |
|      3 | 1731 | `			}` |
|     29 | 1732 | `			if( zCur >= zEnd ){` |
|      - | 1733 | `				/* Return the empty string */` |
|    ! 0 | 1734 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1735 | `			}else{` |
|     29 | 1736 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1737 | `			}` |
|      - | 1738 | `		}` |
|      - | 1739 | `	}` |
|  12477 | 1740 | `	return PH7_OK;` |
|   6912 | 1741 | `}` |
|      - | 1742 | `/*` |
|      - | 1743 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1744 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1745 | ` * Parameters` |
|      - | 1746 | ` *  $str` |
|      - | 1747 | ` *   The string that will be trimmed.` |
|      - | 1748 | ` * $charlist` |
|      - | 1749 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1750 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1751 | ` *   With .. you can specify a range of characters.` |
|      - | 1752 | ` * Returns.` |
|      - | 1753 | ` *  Thr processed string.` |
|      - | 1754 | ` * NOTE:` |
|      - | 1755 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1756 | ` */` |
|     28 | 1757 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1758 | `{` |
|      - | 1759 | `	const char *zString;` |
|      - | 1760 | `	int nLen;` |
|     31 | 1761 | `	if( nArg < 1 ){` |
|      - | 1762 | `		/* Missing arguments,return null */` |
|    ! 0 | 1763 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1764 | `		return PH7_OK;` |
|      - | 1765 | `	}` |
|      - | 1766 | `	/* Extract the target string */` |
|     31 | 1767 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1768 | `	if( nLen < 1 ){` |
|      - | 1769 | `		/* Empty string,return */` |
|      5 | 1770 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1771 | `		return PH7_OK;` |
|      - | 1772 | `	}` |
|      - | 1773 | `	/* Start the trim process */` |
|     27 | 1774 | `	if( nArg < 2 ){` |
|      - | 1775 | `		SyString sStr;` |
|      - | 1776 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1777 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1778 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1779 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1780 | `	}else{` |
|      - | 1781 | `		/* Char list */` |
|      - | 1782 | `		const char *zList;` |
|      - | 1783 | `		int nListlen;` |
|     11 | 1784 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     11 | 1785 | `		if( nListlen < 1 ){` |
|      - | 1786 | `			/* Return the string unchanged */` |
|    ! 0 | 1787 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1788 | `		}else{` |
|      - | 1789 | `			char aMask[256];` |
|     11 | 1790 | `			const char *zEnd = &zString[nLen];` |
|     11 | 1791 | `			const char *zCur = zString;` |
|     11 | 1792 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1793 | `			/* Right trim */` |
|     29 | 1794 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 1795 | `				zEnd--;` |
|      2 | 1796 | `			}` |
|     11 | 1797 | `			if( zEnd <= zCur ){` |
|      - | 1798 | `				/* Return the empty string */` |
|    ! 0 | 1799 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1800 | `			}else{` |
|     11 | 1801 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1802 | `			}` |
|      - | 1803 | `		}` |
|      - | 1804 | `	}` |
|     27 | 1805 | `	return PH7_OK;` |
|     17 | 1806 | `}` |
|      - | 1807 | `/*` |
|      - | 1808 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 1809 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1810 | ` * Parameters` |
|      - | 1811 | ` *  $str` |
|      - | 1812 | ` *   The string that will be trimmed.` |
|      - | 1813 | ` * $charlist` |
|      - | 1814 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1815 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1816 | ` *   With .. you can specify a range of characters.` |
|      - | 1817 | ` * Returns.` |
|      - | 1818 | ` *  Thr processed string.` |
|      - | 1819 | ` * NOTE:` |
|      - | 1820 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1821 | ` */` |
|     12 | 1822 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1823 | `{` |
|      - | 1824 | `	const char *zString;` |
|      - | 1825 | `	int nLen;` |
|     14 | 1826 | `	if( nArg < 1 ){` |
|      - | 1827 | `		/* Missing arguments,return null */` |
|    ! 0 | 1828 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1829 | `		return PH7_OK;` |
|      - | 1830 | `	}` |
|      - | 1831 | `	/* Extract the target string */` |
|     14 | 1832 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     14 | 1833 | `	if( nLen < 1 ){` |
|      - | 1834 | `		/* Empty string,return */` |
|    ! 0 | 1835 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1836 | `		return PH7_OK;` |
|      - | 1837 | `	}` |
|      - | 1838 | `	/* Start the trim process */` |
|     14 | 1839 | `	if( nArg < 2 ){` |
|      - | 1840 | `		SyString sStr;` |
|      - | 1841 | `		/* Remove white spaces and NUL byte */` |
|      3 | 1842 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 1843 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 1844 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 1845 | `	}else{` |
|      - | 1846 | `		/* Char list */` |
|      - | 1847 | `		const char *zList;` |
|      - | 1848 | `		int nListlen;` |
|     12 | 1849 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     12 | 1850 | `		if( nListlen < 1 ){` |
|      - | 1851 | `			/* Return the string unchanged */` |
|      3 | 1852 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1853 | `		}else{` |
|      - | 1854 | `			char aMask[256];` |
|     10 | 1855 | `			const char *zEnd = &zString[nLen];` |
|     10 | 1856 | `			const char *zCur = zString;` |
|     10 | 1857 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1858 | `			/* Left trim */` |
|     28 | 1859 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     20 | 1860 | `				zCur++;` |
|      2 | 1861 | `			}` |
|     10 | 1862 | `			if( zCur >= zEnd ){` |
|      - | 1863 | `				/* Return the empty string */` |
|    ! 0 | 1864 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1865 | `			}else{` |
|     10 | 1866 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1867 | `			}` |
|      - | 1868 | `		}` |
|      - | 1869 | `	}` |
|     14 | 1870 | `	return PH7_OK;` |
|      8 | 1871 | `}` |
|      - | 1872 | `/*` |
|      - | 1873 | ` * string strtolower(string $str)` |
|      - | 1874 | ` *  Make a string lowercase.` |
|      - | 1875 | ` * Parameters` |
|      - | 1876 | ` *  $str` |
|      - | 1877 | ` *   The input string.` |
|      - | 1878 | ` * Returns.` |
|      - | 1879 | ` *  The lowercased string.` |
|      - | 1880 | ` */` |
|  31860 | 1881 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1882 | `{` |
|      - | 1883 | `	const char *zString,*zCur,*zEnd;` |
|      - | 1884 | `	int nLen;` |
|  31865 | 1885 | `	if( nArg < 1 ){` |
|      - | 1886 | `		/* Missing arguments,return null */` |
|    ! 0 | 1887 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1888 | `		return PH7_OK;` |
|      - | 1889 | `	}` |
|      - | 1890 | `	/* Extract the target string */` |
|  31865 | 1891 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  31865 | 1892 | `	if( nLen < 1 ){` |
|      - | 1893 | `		/* Empty string,return */` |
|      3 | 1894 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1895 | `		return PH7_OK;` |
|      - | 1896 | `	}` |
|      - | 1897 | `	/* Perform the requested operation */` |
|  31863 | 1898 | `	zEnd = &zString[nLen];` |
| 100284 | 1899 | `	for(;;){` |
| 200573 | 1900 | `		if( zString >= zEnd ){` |
|      - | 1901 | `			/* No more input,break immediately */` |
|  31863 | 1902 | `			break;` |
|      - | 1903 | `		}` |
| 168715 | 1904 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1905 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 1906 | `			zCur = zString;` |
|    ! 0 | 1907 | `			zString++;` |
|    ! 0 | 1908 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 1909 | `				zString++;` |
|    ! 0 | 1910 | `			}` |
|      - | 1911 | `			/* Append UTF-8 stream */` |
|    ! 0 | 1912 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 1913 | `		}else{` |
| 168715 | 1914 | `			int c = zString[0];` |
| 168715 | 1915 | `			if( SyisUpper(c) ){` |
| 168713 | 1916 | `				c = SyToLower(zString[0]);` |
|  84354 | 1917 | `			}` |
|      - | 1918 | `			/* Append character */` |
| 168715 | 1919 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1920 | `			/* Advance the cursor */` |
| 168715 | 1921 | `			zString++;` |
|      - | 1922 | `		}` |
|      5 | 1923 | `	}` |
|  31863 | 1924 | `	return PH7_OK;` |
|  15935 | 1925 | `}` |
|      - | 1926 | `/*` |
|      - | 1927 | ` * string strtolower(string $str)` |
|      - | 1928 | ` *  Make a string uppercase.` |
|      - | 1929 | ` * Parameters` |
|      - | 1930 | ` *  $str` |
|      - | 1931 | ` *   The input string.` |
|      - | 1932 | ` * Returns.` |
|      - | 1933 | ` *  The uppercased string.` |
|      - | 1934 | ` */` |
|     40 | 1935 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1936 | `{` |
|      - | 1937 | `	const char *zString,*zCur,*zEnd;` |
|      - | 1938 | `	int nLen;` |
|     44 | 1939 | `	if( nArg < 1 ){` |
|      - | 1940 | `		/* Missing arguments,return null */` |
|    ! 0 | 1941 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1942 | `		return PH7_OK;` |
|      - | 1943 | `	}` |
|      - | 1944 | `	/* Extract the target string */` |
|     44 | 1945 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     44 | 1946 | `	if( nLen < 1 ){` |
|      - | 1947 | `		/* Empty string,return */` |
|      3 | 1948 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1949 | `		return PH7_OK;` |
|      - | 1950 | `	}` |
|      - | 1951 | `	/* Perform the requested operation */` |
|     42 | 1952 | `	zEnd = &zString[nLen];` |
|     98 | 1953 | `	for(;;){` |
|    200 | 1954 | `		if( zString >= zEnd ){` |
|      - | 1955 | `			/* No more input,break immediately */` |
|     42 | 1956 | `			break;` |
|      - | 1957 | `		}` |
|    162 | 1958 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1959 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 1960 | `			zCur = zString;` |
|    ! 0 | 1961 | `			zString++;` |
|    ! 0 | 1962 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 1963 | `				zString++;` |
|    ! 0 | 1964 | `			}` |
|      - | 1965 | `			/* Append UTF-8 stream */` |
|    ! 0 | 1966 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 1967 | `		}else{` |
|    162 | 1968 | `			int c = zString[0];` |
|    162 | 1969 | `			if( SyisLower(c) ){` |
|    156 | 1970 | `				c = SyToUpper(zString[0]);` |
|     76 | 1971 | `			}` |
|      - | 1972 | `			/* Append character */` |
|    162 | 1973 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1974 | `			/* Advance the cursor */` |
|    162 | 1975 | `			zString++;` |
|      - | 1976 | `		}` |
|      4 | 1977 | `	}` |
|     42 | 1978 | `	return PH7_OK;` |
|     24 | 1979 | `}` |
|      - | 1980 | `/*` |
|      - | 1981 | ` * string ucfirst(string $str)` |
|      - | 1982 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 1983 | ` *  character is alphabetic.` |
|      - | 1984 | ` * Parameters` |
|      - | 1985 | ` *  $str` |
|      - | 1986 | ` *   The input string.` |
|      - | 1987 | ` * Returns.` |
|      - | 1988 | ` *  The processed string.` |
|      - | 1989 | ` */` |
|      4 | 1990 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1991 | `{` |
|      - | 1992 | `	const char *zString,*zEnd;` |
|      - | 1993 | `	int nLen,c;` |
|      5 | 1994 | `	if( nArg < 1 ){` |
|      - | 1995 | `		/* Missing arguments,return null */` |
|    ! 0 | 1996 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1997 | `		return PH7_OK;` |
|      - | 1998 | `	}` |
|      - | 1999 | `	/* Extract the target string */` |
|      5 | 2000 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2001 | `	if( nLen < 1 ){` |
|      - | 2002 | `		/* Empty string,return */` |
|      3 | 2003 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2004 | `		return PH7_OK;` |
|      - | 2005 | `	}` |
|      - | 2006 | `	/* Perform the requested operation */` |
|      3 | 2007 | `	zEnd = &zString[nLen];` |
|      3 | 2008 | `	c = zString[0];` |
|      3 | 2009 | `	if( SyisLower(c) ){` |
|      3 | 2010 | `		c = SyToUpper(c);` |
|      1 | 2011 | `	}` |
|      - | 2012 | `	/* Append the first character */` |
|      3 | 2013 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2014 | `	zString++;` |
|      3 | 2015 | `	if( zString < zEnd ){` |
|      - | 2016 | `		/* Append the rest of the input verbatim */` |
|      3 | 2017 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2018 | `	}` |
|      3 | 2019 | `	return PH7_OK;` |
|      3 | 2020 | `}` |
|      - | 2021 | `/*` |
|      - | 2022 | ` * string lcfirst(string $str)` |
|      - | 2023 | ` *  Make a string's first character lowercase.` |
|      - | 2024 | ` * Parameters` |
|      - | 2025 | ` *  $str` |
|      - | 2026 | ` *   The input string.` |
|      - | 2027 | ` * Returns.` |
|      - | 2028 | ` *  The processed string.` |
|      - | 2029 | ` */` |
|      4 | 2030 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2031 | `{` |
|      - | 2032 | `	const char *zString,*zEnd;` |
|      - | 2033 | `	int nLen,c;` |
|      5 | 2034 | `	if( nArg < 1 ){` |
|      - | 2035 | `		/* Missing arguments,return null */` |
|    ! 0 | 2036 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2037 | `		return PH7_OK;` |
|      - | 2038 | `	}` |
|      - | 2039 | `	/* Extract the target string */` |
|      5 | 2040 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2041 | `	if( nLen < 1 ){` |
|      - | 2042 | `		/* Empty string,return */` |
|      3 | 2043 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2044 | `		return PH7_OK;` |
|      - | 2045 | `	}` |
|      - | 2046 | `	/* Perform the requested operation */` |
|      3 | 2047 | `	zEnd = &zString[nLen];` |
|      3 | 2048 | `	c = zString[0];` |
|      3 | 2049 | `	if( SyisUpper(c) ){` |
|      3 | 2050 | `		c = SyToLower(c);` |
|      1 | 2051 | `	}` |
|      - | 2052 | `	/* Append the first character */` |
|      3 | 2053 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2054 | `	zString++;` |
|      3 | 2055 | `	if( zString < zEnd ){` |
|      - | 2056 | `		/* Append the rest of the input verbatim */` |
|      3 | 2057 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2058 | `	}` |
|      3 | 2059 | `	return PH7_OK;` |
|      3 | 2060 | `}` |
|      - | 2061 | `/*` |
|      - | 2062 | ` * int ord(string $string)` |
|      - | 2063 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2064 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2065 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2066 | ` * Parameters` |
|      - | 2067 | ` *  $string` |
|      - | 2068 | ` *   The input string.` |
|      - | 2069 | ` * Returns` |
|      - | 2070 | ` *  The ASCII value as an integer.` |
|      - | 2071 | ` */` |
|     56 | 2072 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2073 | `{` |
|      - | 2074 | `	const char *zString;` |
|      - | 2075 | `	int nLen,c;` |
|      - | 2076 | `	/* PHP requires exactly one argument. */` |
|     59 | 2077 | `	if( nArg != 1 ){` |
|      8 | 2078 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2079 | `			"ArgumentCountError",` |
|      - | 2080 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2081 | `			nArg` |
|      - | 2082 | `			);` |
|      - | 2083 | `	}` |
|      - | 2084 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2085 | `	 * the empty-string deprecation, so we check null first. */` |
|     53 | 2086 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2087 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2088 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2089 | `			"of type string is deprecated"` |
|      - | 2090 | `			);` |
|      1 | 2091 | `	}` |
|      - | 2092 | `	/* Extract the target string */` |
|     53 | 2093 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     53 | 2094 | `	if( nLen < 1 ){` |
|      - | 2095 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2096 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2097 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2098 | `			);` |
|      5 | 2099 | `		ph7_result_int(pCtx,0);` |
|      5 | 2100 | `		return PH7_OK;` |
|      - | 2101 | `	}` |
|      - | 2102 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     49 | 2103 | `	if( nLen > 1 ){` |
|      7 | 2104 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2105 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2106 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2107 | `			);` |
|      3 | 2108 | `	}` |
|      - | 2109 | `	/* Extract the ASCII value of the first character */` |
|     49 | 2110 | `	c = (unsigned char)zString[0];` |
|      - | 2111 | `	/* Return that value */` |
|     49 | 2112 | `	ph7_result_int(pCtx,c);` |
|     49 | 2113 | `	return PH7_OK;` |
|     31 | 2114 | `}` |
|      - | 2115 | `/*` |
|      - | 2116 | ` * string chr(int $codepoint)` |
|      - | 2117 | ` *  Returns a one-character string containing the character specified` |
|      - | 2118 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2119 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2120 | ` * Parameters` |
|      - | 2121 | ` *  $codepoint` |
|      - | 2122 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2123 | ` *   will be constrained to a single byte.` |
|      - | 2124 | ` * Returns` |
|      - | 2125 | ` *  A single-character string.` |
|      - | 2126 | ` */` |
|     48 | 2127 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2128 | `{` |
|      - | 2129 | `	int c;` |
|      - | 2130 | `	unsigned char ch;` |
|      - | 2131 | `	/* PHP requires exactly one argument. */` |
|     51 | 2132 | `	if( nArg != 1 ){` |
|      8 | 2133 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2134 | `			"ArgumentCountError",` |
|      - | 2135 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2136 | `			nArg` |
|      - | 2137 | `			);` |
|      - | 2138 | `	}` |
|      - | 2139 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2140 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2141 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2142 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     45 | 2143 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2144 | `		char zBuf[120];` |
|      4 | 2145 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2146 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2147 | `			ph7_value_to_double(apArg[0])` |
|      - | 2148 | `			);` |
|      3 | 2149 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2150 | `	}` |
|      - | 2151 | `	/* Extract the codepoint. */` |
|     45 | 2152 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2153 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2154 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2155 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2156 | `	 * name to avoid the API double-prefixing it. */` |
|     45 | 2157 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2158 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2159 | `			E_DEPRECATED,` |
|      - | 2160 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2161 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2162 | `			"The value used will be constrained using % 256"` |
|      - | 2163 | `			);` |
|      2 | 2164 | `	}` |
|      - | 2165 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2166 | `	 * when taking the address of a wider int. */` |
|     45 | 2167 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2168 | `	/* Return the specified character */` |
|     45 | 2169 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     45 | 2170 | `	return PH7_OK;` |
|     27 | 2171 | `}` |
|      - | 2172 | `/*` |
|      - | 2173 | ` * Binary to hex consumer callback.` |
|      - | 2174 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2175 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2176 | ` */` |
|   3118 | 2177 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 2178 | `{` |
|      - | 2179 | `	/* Append hex chunk verbatim */` |
|   3120 | 2180 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 2181 | `	return SXRET_OK;` |
|      2 | 2182 | `}` |
|      - | 2183 |  |
|      - | 2184 | `/*` |
|      - | 2185 | ` * string bin2hex(string $str)` |
|      - | 2186 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2187 | ` * Parameters` |
|      - | 2188 | ` *  $str` |
|      - | 2189 | ` *   The input string.` |
|      - | 2190 | ` * Returns.` |
|      - | 2191 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2192 | ` */` |
|    138 | 2193 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2194 | `{` |
|      - | 2195 | `	const char *zString;` |
|      - | 2196 | `	int nLen;` |
|      - | 2197 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    143 | 2198 | `	if( nArg != 1 ){` |
|      8 | 2199 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2200 | `			"ArgumentCountError",` |
|      - | 2201 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2202 | `			nArg` |
|      - | 2203 | `			);` |
|      - | 2204 | `	}` |
|      - | 2205 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2206 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2207 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2208 | `	 */` |
|    204 | 2209 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    130 | 2210 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2211 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2212 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2213 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2214 | `		)` |
|      - | 2215 | `	){` |
|      9 | 2216 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2217 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2218 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2219 | `			if( pInst && pInst->pClass ){` |
|      3 | 2220 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2221 | `			}` |
|      1 | 2222 | `		}` |
|     12 | 2223 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2224 | `			"TypeError",` |
|      - | 2225 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2226 | `			zType` |
|      - | 2227 | `			);` |
|      - | 2228 | `	}` |
|      - | 2229 | `	/* Extract the target string */` |
|    130 | 2230 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 2231 | `	if( nLen < 1 ){` |
|      - | 2232 | `		/* Empty string,return */` |
|     13 | 2233 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 2234 | `		return PH7_OK;` |
|      - | 2235 | `	}` |
|      - | 2236 | `	/* Perform the requested operation */` |
|    118 | 2237 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 2238 | `	return PH7_OK;` |
|     74 | 2239 | `}` |
|      - | 2240 |  |
|      - | 2241 | `/* Search callback signature */` |
|      - | 2242 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2243 | `/*` |
|      - | 2244 | ` * Case-insensitive pattern match.` |
|      - | 2245 | ` * Brute force is the default search method used here.` |
|      - | 2246 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2247 | ` * well for short/medium texts on modern hardware.` |
|      - | 2248 | ` */` |
|    118 | 2249 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2250 | `{` |
|    119 | 2251 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2252 | `	const char *zIn = (const char *)pText;` |
|    119 | 2253 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2254 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2255 | `	const char *zPtr,*zPtr2;` |
|      - | 2256 | `	int c,d;` |
|    119 | 2257 | `	if( iPatLen > nLen ){` |
|      - | 2258 | `		/* Don't bother processing */` |
|     33 | 2259 | `		return SXERR_NOTFOUND;` |
|      - | 2260 | `	}` |
|    242 | 2261 | `	for(;;){` |
|    485 | 2262 | `		if( zIn >= zEnd ){` |
|     47 | 2263 | `			break;` |
|      - | 2264 | `		}` |
|    439 | 2265 | `		c = SyToLower(zIn[0]);` |
|    439 | 2266 | `		d = SyToLower(zpIn[0]);` |
|    439 | 2267 | `		if( c == d ){` |
|     41 | 2268 | `			zPtr   = &zIn[1];` |
|     41 | 2269 | `			zPtr2  = &zpIn[1];` |
|     71 | 2270 | `			for(;;){` |
|    143 | 2271 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2272 | `					/* Pattern found */` |
|     41 | 2273 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2274 | `					return SXRET_OK;` |
|      - | 2275 | `				}` |
|    103 | 2276 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2277 | `					break;` |
|      - | 2278 | `				}` |
|    103 | 2279 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2280 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2281 | `				if( c != d ){` |
|    ! 0 | 2282 | `					break;` |
|      - | 2283 | `				}` |
|    103 | 2284 | `				zPtr++; zPtr2++;` |
|      1 | 2285 | `			}` |
|    ! 0 | 2286 | `		}` |
|    399 | 2287 | `		zIn++;` |
|      1 | 2288 | `	}` |
|      - | 2289 | `	/* Pattern not found */` |
|     47 | 2290 | `	return SXERR_NOTFOUND;` |
|     60 | 2291 | `}` |
|      - | 2292 | `/*` |
|      - | 2293 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2294 | ` *  Find the first occurrence of a string.` |
|      - | 2295 | ` * Parameters` |
|      - | 2296 | ` *  $haystack` |
|      - | 2297 | ` *   The input string.` |
|      - | 2298 | ` * $needle` |
|      - | 2299 | ` *   Search pattern (must be a string).` |
|      - | 2300 | ` * $before_needle` |
|      - | 2301 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2302 | ` *   of the needle (excluding the needle).` |
|      - | 2303 | ` * Return` |
|      - | 2304 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2305 | ` */` |
|      6 | 2306 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2307 | `{` |
|      7 | 2308 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2309 | `	const char *zBlob,*zPattern;` |
|      - | 2310 | `	int nLen,nPatLen;` |
|      - | 2311 | `	sxu32 nOfft;` |
|      - | 2312 | `	sxi32 rc;` |
|      7 | 2313 | `	if( nArg < 2 ){` |
|      - | 2314 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2315 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2316 | `		return PH7_OK;` |
|      - | 2317 | `	}` |
|      - | 2318 | `	/* Extract the needle and the haystack */` |
|      7 | 2319 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2320 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2321 | `	nOfft = 0; /* cc warning */` |
|      9 | 2322 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2323 | `		int before = 0;` |
|      - | 2324 | `		/* Perform the lookup */` |
|      5 | 2325 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2326 | `		if( rc != SXRET_OK ){` |
|      - | 2327 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2328 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2329 | `			return PH7_OK;` |
|      - | 2330 | `		}` |
|      - | 2331 | `		/* Return the portion of the string */` |
|      5 | 2332 | `		if( nArg > 2 ){` |
|      3 | 2333 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2334 | `		}` |
|      5 | 2335 | `		if( before ){` |
|      3 | 2336 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2337 | `		}else{` |
|      3 | 2338 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2339 | `		}` |
|      3 | 2340 | `	}else{` |
|      3 | 2341 | `		ph7_result_bool(pCtx,0);` |
|      - | 2342 | `	}` |
|      7 | 2343 | `	return PH7_OK;` |
|      4 | 2344 | `}` |
|      - | 2345 | `/*` |
|      - | 2346 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2347 | ` *  Case-insensitive strstr().` |
|      - | 2348 | ` * Parameters` |
|      - | 2349 | ` *  $haystack` |
|      - | 2350 | ` *   The input string.` |
|      - | 2351 | ` * $needle` |
|      - | 2352 | ` *   Search pattern (must be a string).` |
|      - | 2353 | ` * $before_needle` |
|      - | 2354 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2355 | ` *   of the needle (excluding the needle).` |
|      - | 2356 | ` * Return` |
|      - | 2357 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2358 | ` */` |
|      4 | 2359 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2360 | `{` |
|      5 | 2361 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2362 | `	const char *zBlob,*zPattern;` |
|      - | 2363 | `	int nLen,nPatLen;` |
|      - | 2364 | `	sxu32 nOfft;` |
|      - | 2365 | `	sxi32 rc;` |
|      5 | 2366 | `	if( nArg < 2 ){` |
|      - | 2367 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2368 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2369 | `		return PH7_OK;` |
|      - | 2370 | `	}` |
|      - | 2371 | `	/* Extract the needle and the haystack */` |
|      5 | 2372 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2373 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2374 | `	nOfft = 0; /* cc warning */` |
|      7 | 2375 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2376 | `		int before = 0;` |
|      - | 2377 | `		/* Perform the lookup */` |
|      5 | 2378 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2379 | `		if( rc != SXRET_OK ){` |
|      - | 2380 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2381 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2382 | `			return PH7_OK;` |
|      - | 2383 | `		}` |
|      - | 2384 | `		/* Return the portion of the string */` |
|      5 | 2385 | `		if( nArg > 2 ){` |
|      3 | 2386 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2387 | `		}` |
|      5 | 2388 | `		if( before ){` |
|      3 | 2389 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2390 | `		}else{` |
|      3 | 2391 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2392 | `		}` |
|      3 | 2393 | `	}else{` |
|    ! 0 | 2394 | `		ph7_result_bool(pCtx,0);` |
|      - | 2395 | `	}` |
|      5 | 2396 | `	return PH7_OK;` |
|      3 | 2397 | `}` |
|      - | 2398 | `/*` |
|      - | 2399 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2400 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2401 | ` * Parameters` |
|      - | 2402 | ` *  $haystack` |
|      - | 2403 | ` *   The input string.` |
|      - | 2404 | ` * $needle` |
|      - | 2405 | ` *   Search pattern (must be a string).` |
|      - | 2406 | ` * $offset` |
|      - | 2407 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2408 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2409 | ` *   of haystack.` |
|      - | 2410 | ` * Return` |
|      - | 2411 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2412 | ` */` |
|    124 | 2413 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2414 | `{` |
|    129 | 2415 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2416 | `	const char *zBlob,*zPattern;` |
|      - | 2417 | `	int nLen,nPatLen,nStart;` |
|      - | 2418 | `	sxu32 nOfft;` |
|      - | 2419 | `	sxi32 rc;` |
|    129 | 2420 | `	if( nArg < 2 ){` |
|      - | 2421 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2422 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2423 | `		return PH7_OK;` |
|      - | 2424 | `	}` |
|      - | 2425 | `	/* Extract the needle and the haystack */` |
|    129 | 2426 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    129 | 2427 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    129 | 2428 | `	nOfft = 0; /* cc warning */` |
|    129 | 2429 | `	nStart = 0;` |
|      - | 2430 | `	/* Peek the starting offset if available */` |
|    129 | 2431 | `	if( nArg > 2 ){` |
|    ! 0 | 2432 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2433 | `		if( nStart < 0 ){` |
|    ! 0 | 2434 | `			nStart = -nStart;` |
|    ! 0 | 2435 | `		}` |
|    ! 0 | 2436 | `		if( nStart >= nLen ){` |
|      - | 2437 | `			/* Invalid offset */` |
|    ! 0 | 2438 | `			nStart = 0;` |
|    ! 0 | 2439 | `		}else{` |
|    ! 0 | 2440 | `			zBlob += nStart;` |
|    ! 0 | 2441 | `			nLen -= nStart;` |
|      - | 2442 | `		}` |
|    ! 0 | 2443 | `	}` |
|    129 | 2444 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2445 | `		/* Perform the lookup */` |
|    127 | 2446 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    127 | 2447 | `		if( rc != SXRET_OK ){` |
|      - | 2448 | `			/* Pattern not found,return FALSE */` |
|     33 | 2449 | `			ph7_result_bool(pCtx,0);` |
|     33 | 2450 | `			return PH7_OK;` |
|      - | 2451 | `		}` |
|      - | 2452 | `		/* Return the pattern position */` |
|     99 | 2453 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     52 | 2454 | `	}else{` |
|      3 | 2455 | `		ph7_result_bool(pCtx,0);` |
|      - | 2456 | `	}` |
|    101 | 2457 | `	return PH7_OK;` |
|     67 | 2458 | `}` |
|      - | 2459 | `/*` |
|      - | 2460 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2461 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2462 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2463 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2464 | ` *` |
|      - | 2465 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2466 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2467 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2468 | ` *` |
|      - | 2469 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2470 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2471 | ` */` |
|    426 | 2472 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2473 | `	ph7_context *pCtx,` |
|      - | 2474 | `	ph7_value *pArg,` |
|      - | 2475 | `	const char *zFunc,` |
|      - | 2476 | `	int iArgNum,` |
|      - | 2477 | `	const char *zParamName,` |
|      - | 2478 | `	const char *zNullMsg,` |
|      - | 2479 | `	ph7_value *pTmp,` |
|      - | 2480 | `	const char **pzOut,` |
|      - | 2481 | `	int *pnOut` |
|      4 | 2482 | `){` |
|    430 | 2483 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2484 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2485 | `		*pzOut = "";` |
|     13 | 2486 | `		*pnOut = 0;` |
|     13 | 2487 | `		return PH7_OK;` |
|      - | 2488 | `	}` |
|    640 | 2489 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    396 | 2490 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2491 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2492 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2493 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2494 | `	    )` |
|      - | 2495 | `	){` |
|     34 | 2496 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2497 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2498 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2499 | `			if( pInst && pInst->pClass ){` |
|     13 | 2500 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2501 | `			}` |
|      6 | 2502 | `		}` |
|     49 | 2503 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2504 | `			"TypeError",` |
|      - | 2505 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2506 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2507 | `			);` |
|      - | 2508 | `	}` |
|    385 | 2509 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2510 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2511 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2512 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2513 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2514 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2515 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2516 | `		return PH7_OK;` |
|      - | 2517 | `	}` |
|    349 | 2518 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    349 | 2519 | `	return PH7_OK;` |
|    217 | 2520 | `}` |
|      - | 2521 | `/*` |
|      - | 2522 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2523 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2524 | ` * Return` |
|      - | 2525 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2526 | ` */` |
|     96 | 2527 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2528 | `{` |
|      - | 2529 | `	const char *zHaystack,*zNeedle;` |
|      - | 2530 | `	int nHayLen,nNeedleLen;` |
|      - | 2531 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2532 | `	sxi32 rc;` |
|    100 | 2533 | `	if( nArg != 2 ){` |
|     18 | 2534 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2535 | `			"ArgumentCountError",` |
|      - | 2536 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2537 | `			nArg` |
|      - | 2538 | `			);` |
|      - | 2539 | `	}` |
|     88 | 2540 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     88 | 2541 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     88 | 2542 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2543 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2544 | `		"of type string is deprecated",` |
|      - | 2545 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     88 | 2546 | `	if( rc != PH7_OK ) goto out;` |
|     81 | 2547 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2548 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2549 | `		"of type string is deprecated",` |
|      - | 2550 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     81 | 2551 | `	if( rc != PH7_OK ) goto out;` |
|     77 | 2552 | `	if( nNeedleLen < 1 ){` |
|     13 | 2553 | `		ph7_result_bool(pCtx,1);` |
|     71 | 2554 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2555 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2556 | `	}else{` |
|     85 | 2557 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     28 | 2558 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     57 | 2559 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2560 | `	}` |
|     77 | 2561 | `	rc = PH7_OK;` |
|     43 | 2562 | `out:` |
|     88 | 2563 | `	PH7_MemObjRelease(&sHayTmp);` |
|     88 | 2564 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     88 | 2565 | `	return rc;` |
|     52 | 2566 | `}` |
|      - | 2567 | `/*` |
|      - | 2568 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2569 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2570 | ` * Return` |
|      - | 2571 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2572 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2573 | ` */` |
|     78 | 2574 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2575 | `{` |
|      - | 2576 | `	const char *zHaystack,*zNeedle;` |
|      - | 2577 | `	int nHayLen,nNeedleLen;` |
|      - | 2578 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2579 | `	sxi32 rc;` |
|     82 | 2580 | `	if( nArg != 2 ){` |
|     18 | 2581 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2582 | `			"ArgumentCountError",` |
|      - | 2583 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2584 | `			nArg` |
|      - | 2585 | `			);` |
|      - | 2586 | `	}` |
|     70 | 2587 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2588 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2589 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2590 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2591 | `		"of type string is deprecated",` |
|      - | 2592 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2593 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2594 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2595 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2596 | `		"of type string is deprecated",` |
|      - | 2597 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2598 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2599 | `	if( nNeedleLen < 1 ){` |
|     13 | 2600 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2601 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2602 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2603 | `	}else{` |
|     58 | 2604 | `		ph7_result_bool(pCtx,` |
|     38 | 2605 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2606 | `	}` |
|     59 | 2607 | `	rc = PH7_OK;` |
|     34 | 2608 | `out:` |
|     70 | 2609 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2610 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2611 | `	return rc;` |
|     43 | 2612 | `}` |
|      - | 2613 | `/*` |
|      - | 2614 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2615 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2616 | ` * Return` |
|      - | 2617 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2618 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2619 | ` */` |
|     78 | 2620 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2621 | `{` |
|      - | 2622 | `	const char *zHaystack,*zNeedle;` |
|      - | 2623 | `	int nHayLen,nNeedleLen;` |
|      - | 2624 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2625 | `	sxi32 rc;` |
|     82 | 2626 | `	if( nArg != 2 ){` |
|     18 | 2627 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2628 | `			"ArgumentCountError",` |
|      - | 2629 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2630 | `			nArg` |
|      - | 2631 | `			);` |
|      - | 2632 | `	}` |
|     70 | 2633 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2634 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2635 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2636 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2637 | `		"of type string is deprecated",` |
|      - | 2638 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2639 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2640 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2641 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2642 | `		"of type string is deprecated",` |
|      - | 2643 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2644 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2645 | `	if( nNeedleLen < 1 ){` |
|     13 | 2646 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2647 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2648 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2649 | `	}else{` |
|     58 | 2650 | `		ph7_result_bool(pCtx,` |
|     38 | 2651 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2652 | `	}` |
|     59 | 2653 | `	rc = PH7_OK;` |
|     34 | 2654 | `out:` |
|     70 | 2655 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2656 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2657 | `	return rc;` |
|     43 | 2658 | `}` |
|      - | 2659 | `/*` |
|      - | 2660 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2661 | ` *  Case-insensitive strpos.` |
|      - | 2662 | ` * Parameters` |
|      - | 2663 | ` *  $haystack` |
|      - | 2664 | ` *   The input string.` |
|      - | 2665 | ` * $needle` |
|      - | 2666 | ` *   Search pattern (must be a string).` |
|      - | 2667 | ` * $offset` |
|      - | 2668 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2669 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2670 | ` *   of haystack.` |
|      - | 2671 | ` * Return` |
|      - | 2672 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2673 | ` */` |
|     16 | 2674 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2675 | `{` |
|     17 | 2676 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2677 | `	const char *zBlob,*zPattern;` |
|      - | 2678 | `	int nLen,nPatLen,nStart;` |
|      - | 2679 | `	sxu32 nOfft;` |
|      - | 2680 | `	sxi32 rc;` |
|     17 | 2681 | `	if( nArg < 2 ){` |
|      - | 2682 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2683 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2684 | `		return PH7_OK;` |
|      - | 2685 | `	}` |
|      - | 2686 | `	/* Extract the needle and the haystack */` |
|     17 | 2687 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2688 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2689 | `	nOfft = 0; /* cc warning */` |
|     17 | 2690 | `	nStart = 0;` |
|      - | 2691 | `	/* Peek the starting offset if available */` |
|     17 | 2692 | `	if( nArg > 2 ){` |
|      5 | 2693 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2694 | `		if( nStart < 0 ){` |
|      3 | 2695 | `			nStart = -nStart;` |
|      1 | 2696 | `		}` |
|      5 | 2697 | `		if( nStart >= nLen ){` |
|      - | 2698 | `			/* Invalid offset */` |
|    ! 0 | 2699 | `			nStart = 0;` |
|    ! 0 | 2700 | `		}else{` |
|      5 | 2701 | `			zBlob += nStart;` |
|      5 | 2702 | `			nLen -= nStart;` |
|      - | 2703 | `		}` |
|      2 | 2704 | `	}` |
|     17 | 2705 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2706 | `		/* Perform the lookup */` |
|     17 | 2707 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2708 | `		if( rc != SXRET_OK ){` |
|      - | 2709 | `			/* Pattern not found,return FALSE */` |
|      3 | 2710 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2711 | `			return PH7_OK;` |
|      - | 2712 | `		}` |
|      - | 2713 | `		/* Return the pattern position */` |
|     15 | 2714 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2715 | `	}else{` |
|    ! 0 | 2716 | `		ph7_result_bool(pCtx,0);` |
|      - | 2717 | `	}` |
|     15 | 2718 | `	return PH7_OK;` |
|      9 | 2719 | `}` |
|      - | 2720 | `/*` |
|      - | 2721 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2722 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2723 | ` * Parameters` |
|      - | 2724 | ` *  $haystack` |
|      - | 2725 | ` *   The input string.` |
|      - | 2726 | ` * $needle` |
|      - | 2727 | ` *   Search pattern (must be a string).` |
|      - | 2728 | ` * $offset` |
|      - | 2729 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2730 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2731 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2732 | ` * Return` |
|      - | 2733 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2734 | ` */` |
|     30 | 2735 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2736 | `{` |
|      - | 2737 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     31 | 2738 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2739 | `	int nLen,nPatLen;` |
|      - | 2740 | `	sxu32 nOfft;` |
|      - | 2741 | `	sxi32 rc;` |
|     31 | 2742 | `	if( nArg < 2 ){` |
|      - | 2743 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2744 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2745 | `		return PH7_OK;` |
|      - | 2746 | `	}` |
|      - | 2747 | `	/* Extract the needle and the haystack */` |
|     31 | 2748 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2749 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2750 | `	/* Point to the end of the pattern */` |
|     31 | 2751 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2752 | `	zEnd = &zBlob[nLen];` |
|      - | 2753 | `	/* Save the starting posistion */` |
|     31 | 2754 | `	zStart = zBlob;` |
|     31 | 2755 | `	nOfft = 0; /* cc warning */` |
|      - | 2756 | `	/* Peek the starting offset if available */` |
|     31 | 2757 | `	if( nArg > 2 ){` |
|      - | 2758 | `		int nStart;` |
|     21 | 2759 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2760 | `		if( nStart < 0 ){` |
|     11 | 2761 | `			nStart = -nStart;` |
|     11 | 2762 | `			if( nStart >= nLen ){` |
|      - | 2763 | `				/* Invalid offset */` |
|      3 | 2764 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2765 | `				return PH7_OK;` |
|    ! 0 | 2766 | `			}else{` |
|      9 | 2767 | `				nLen -= nStart;` |
|      9 | 2768 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2769 | `				zEnd = &zBlob[nLen];` |
|      - | 2770 | `			}` |
|      5 | 2771 | `		}else{` |
|     11 | 2772 | `			if( nStart >= nLen ){` |
|      - | 2773 | `				/* Invalid offset */` |
|      5 | 2774 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2775 | `				return PH7_OK;` |
|    ! 0 | 2776 | `			}else{` |
|      7 | 2777 | `				zBlob += nStart;` |
|      7 | 2778 | `				nLen -= nStart;` |
|      - | 2779 | `			}` |
|      - | 2780 | `		}` |
|      7 | 2781 | `	}` |
|     25 | 2782 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2783 | `		/* Perform the lookup */` |
|     57 | 2784 | `		for(;;){` |
|    115 | 2785 | `			if( zBlob >= zPtr ){` |
|     11 | 2786 | `				break;` |
|      - | 2787 | `			}` |
|    105 | 2788 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 2789 | `			if( rc == SXRET_OK ){` |
|      - | 2790 | `				/* Pattern found,return it's position */` |
|     13 | 2791 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 2792 | `				return PH7_OK;` |
|      - | 2793 | `			}` |
|     93 | 2794 | `			zPtr--;` |
|      1 | 2795 | `		}` |
|      - | 2796 | `		/* Pattern not found,return FALSE */` |
|     11 | 2797 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2798 | `	}else{` |
|      3 | 2799 | `		ph7_result_bool(pCtx,0);` |
|      - | 2800 | `	}` |
|     13 | 2801 | `	return PH7_OK;` |
|     16 | 2802 | `}` |
|      - | 2803 | `/*` |
|      - | 2804 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2805 | ` *  Case-insensitive strrpos.` |
|      - | 2806 | ` * Parameters` |
|      - | 2807 | ` *  $haystack` |
|      - | 2808 | ` *   The input string.` |
|      - | 2809 | ` * $needle` |
|      - | 2810 | ` *   Search pattern (must be a string).` |
|      - | 2811 | ` * $offset` |
|      - | 2812 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2813 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2814 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2815 | ` * Return` |
|      - | 2816 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2817 | ` */` |
|     26 | 2818 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2819 | `{` |
|      - | 2820 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 2821 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2822 | `	int nLen,nPatLen;` |
|      - | 2823 | `	sxu32 nOfft;` |
|      - | 2824 | `	sxi32 rc;` |
|     27 | 2825 | `	if( nArg < 2 ){` |
|      - | 2826 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2827 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2828 | `		return PH7_OK;` |
|      - | 2829 | `	}` |
|      - | 2830 | `	/* Extract the needle and the haystack */` |
|     27 | 2831 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 2832 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2833 | `	/* Point to the end of the pattern */` |
|     27 | 2834 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 2835 | `	zEnd = &zBlob[nLen];` |
|      - | 2836 | `	/* Save the starting posistion */` |
|     27 | 2837 | `	zStart = zBlob;` |
|     27 | 2838 | `	nOfft = 0; /* cc warning */` |
|      - | 2839 | `	/* Peek the starting offset if available */` |
|     27 | 2840 | `	if( nArg > 2 ){` |
|      - | 2841 | `		int nStart;` |
|     15 | 2842 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 2843 | `		if( nStart < 0 ){` |
|      7 | 2844 | `			nStart = -nStart;` |
|      7 | 2845 | `			if( nStart >= nLen ){` |
|      - | 2846 | `				/* Invalid offset */` |
|      3 | 2847 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2848 | `				return PH7_OK;` |
|    ! 0 | 2849 | `			}else{` |
|      5 | 2850 | `				nLen -= nStart;` |
|      5 | 2851 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 2852 | `				zEnd = &zBlob[nLen];` |
|      - | 2853 | `			}` |
|      3 | 2854 | `		}else{` |
|      9 | 2855 | `			if( nStart >= nLen ){` |
|      - | 2856 | `				/* Invalid offset */` |
|      5 | 2857 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2858 | `				return PH7_OK;` |
|    ! 0 | 2859 | `			}else{` |
|      5 | 2860 | `				zBlob += nStart;` |
|      5 | 2861 | `				nLen -= nStart;` |
|      - | 2862 | `			}` |
|      - | 2863 | `		}` |
|      4 | 2864 | `	}` |
|     21 | 2865 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2866 | `		/* Perform the lookup */` |
|     44 | 2867 | `		for(;;){` |
|     89 | 2868 | `			if( zBlob >= zPtr ){` |
|      9 | 2869 | `				break;` |
|      - | 2870 | `			}` |
|     81 | 2871 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 2872 | `			if( rc == SXRET_OK ){` |
|      - | 2873 | `				/* Pattern found,return it's position */` |
|     11 | 2874 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 2875 | `				return PH7_OK;` |
|      - | 2876 | `			}` |
|     71 | 2877 | `			zPtr--;` |
|      1 | 2878 | `		}` |
|      - | 2879 | `		/* Pattern not found,return FALSE */` |
|      9 | 2880 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2881 | `	}else{` |
|      3 | 2882 | `		ph7_result_bool(pCtx,0);` |
|      - | 2883 | `	}` |
|     11 | 2884 | `	return PH7_OK;` |
|     14 | 2885 | `}` |
|      - | 2886 | `/*` |
|      - | 2887 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 2888 | ` *  Find the last occurrence of a character in a string.` |
|      - | 2889 | ` * Parameters` |
|      - | 2890 | ` *  $haystack` |
|      - | 2891 | ` *   The input string.` |
|      - | 2892 | ` * $needle` |
|      - | 2893 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 2894 | ` *  This behavior is different from that of strstr().` |
|      - | 2895 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 2896 | ` *  as the ordinal value of a character.` |
|      - | 2897 | ` * Return` |
|      - | 2898 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 2899 | ` */` |
|     22 | 2900 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2901 | `{` |
|      - | 2902 | `	const char *zBlob;` |
|      - | 2903 | `	int nLen,c;` |
|     23 | 2904 | `	if( nArg < 2 ){` |
|      - | 2905 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2906 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2907 | `		return PH7_OK;` |
|      - | 2908 | `	}` |
|      - | 2909 | `	/* Extract the haystack */` |
|     23 | 2910 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 2911 | `	c = 0; /* cc warning */` |
|     23 | 2912 | `	if( nLen > 0 ){` |
|      - | 2913 | `		sxu32 nOfft;` |
|      - | 2914 | `		sxi32 rc;` |
|     21 | 2915 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 2916 | `			const char *zPattern;` |
|     11 | 2917 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 2918 | `														 * for NULL pointer.` |
|      - | 2919 | `														 */` |
|     11 | 2920 | `			c = zPattern[0];` |
|      6 | 2921 | `		}else{` |
|      - | 2922 | `			/* Int cast */` |
|     11 | 2923 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 2924 | `		}` |
|      - | 2925 | `		/* Perform the lookup */` |
|     21 | 2926 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 2927 | `		if( rc != SXRET_OK ){` |
|      - | 2928 | `			/* No such entry,return FALSE */` |
|      7 | 2929 | `			ph7_result_bool(pCtx,0);` |
|      7 | 2930 | `			return PH7_OK;` |
|      - | 2931 | `		}` |
|      - | 2932 | `		/* Return the string portion */` |
|     15 | 2933 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 2934 | `	}else{` |
|      3 | 2935 | `		ph7_result_bool(pCtx,0);` |
|      - | 2936 | `	}` |
|     17 | 2937 | `	return PH7_OK;` |
|     12 | 2938 | `}` |
|      - | 2939 | `/*` |
|      - | 2940 | ` * string strrev(string $string)` |
|      - | 2941 | ` *  Reverse a string.` |
|      - | 2942 | ` * Parameters` |
|      - | 2943 | ` *  $string` |
|      - | 2944 | ` *   String to be reversed.` |
|      - | 2945 | ` * Return` |
|      - | 2946 | ` *  The reversed string.` |
|      - | 2947 | ` */` |
|      2 | 2948 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2949 | `{` |
|      - | 2950 | `	const char *zIn,*zEnd;` |
|      - | 2951 | `	int nLen,c;` |
|      3 | 2952 | `	if( nArg < 1 ){` |
|      - | 2953 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 2954 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2955 | `		return PH7_OK;` |
|      - | 2956 | `	}` |
|      - | 2957 | `	/* Extract the target string */` |
|      3 | 2958 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 2959 | `	if( nLen < 1 ){` |
|      - | 2960 | `		/* Empty string Return null */` |
|    ! 0 | 2961 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2962 | `		return PH7_OK;` |
|      - | 2963 | `	}` |
|      - | 2964 | `	/* Perform the requested operation */` |
|      3 | 2965 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 2966 | `	for(;;){` |
|      9 | 2967 | `		if( zEnd < zIn ){` |
|      - | 2968 | `			/* No more input to process */` |
|      3 | 2969 | `			break;` |
|      - | 2970 | `		}` |
|      - | 2971 | `		/* Append current character */` |
|      7 | 2972 | `		c = zEnd[0];` |
|      7 | 2973 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 2974 | `		zEnd--;` |
|      1 | 2975 | `	}` |
|      3 | 2976 | `	return PH7_OK;` |
|      2 | 2977 | `}` |
|      - | 2978 | `/*` |
|      - | 2979 | ` * string ucwords(string $string)` |
|      - | 2980 | ` *  Uppercase the first character of each word in a string.` |
|      - | 2981 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 2982 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 2983 | ` * Parameters` |
|      - | 2984 | ` *  $string` |
|      - | 2985 | ` *   The input string.` |
|      - | 2986 | ` * Return` |
|      - | 2987 | ` *  The modified string..` |
|      - | 2988 | ` */` |
|     12 | 2989 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2990 | `{` |
|      - | 2991 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 2992 | `	int nLen,c;` |
|     13 | 2993 | `	if( nArg < 1 ){` |
|      - | 2994 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 2995 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2996 | `		return PH7_OK;` |
|      - | 2997 | `	}` |
|      - | 2998 | `	/* Extract the target string */` |
|     13 | 2999 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3000 | `	if( nLen < 1 ){` |
|      - | 3001 | `		/* Empty string – match PHP semantics */` |
|      3 | 3002 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3003 | `		return PH7_OK;` |
|      - | 3004 | `	}` |
|      - | 3005 | `	/* Perform the requested operation */` |
|     11 | 3006 | `	zEnd = &zIn[nLen];` |
|     21 | 3007 | `	for(;;){` |
|      - | 3008 | `		/* Jump leading white spaces */` |
|     43 | 3009 | `		zCur = zIn;` |
|     65 | 3010 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3011 | `			zIn++;` |
|      1 | 3012 | `		}` |
|     43 | 3013 | `		if( zCur < zIn ){` |
|      - | 3014 | `			/* Append white space stream */` |
|     23 | 3015 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3016 | `		}` |
|     43 | 3017 | `		if( zIn >= zEnd ){` |
|      - | 3018 | `			/* No more input to process */` |
|     11 | 3019 | `			break;` |
|      - | 3020 | `		}` |
|     33 | 3021 | `		c = zIn[0];` |
|     33 | 3022 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3023 | `			c = SyToUpper(c);` |
|     14 | 3024 | `		}` |
|      - | 3025 | `		/* Append the upper-cased character */` |
|     33 | 3026 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3027 | `		zIn++;` |
|     33 | 3028 | `		zCur = zIn;` |
|      - | 3029 | `		/* Append the word varbatim */` |
|    149 | 3030 | `		while( zIn < zEnd ){` |
|    139 | 3031 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3032 | `				/* UTF-8 stream */` |
|    ! 0 | 3033 | `				zIn++;` |
|    ! 0 | 3034 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3035 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3036 | `				zIn++;` |
|     59 | 3037 | `			}else{` |
|     23 | 3038 | `				break;` |
|      - | 3039 | `			}` |
|      1 | 3040 | `		}` |
|     33 | 3041 | `		if( zCur < zIn ){` |
|     33 | 3042 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3043 | `		}` |
|      1 | 3044 | `	}` |
|     11 | 3045 | `	return PH7_OK;` |
|      7 | 3046 | `}` |
|      - | 3047 | `/*` |
|      - | 3048 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3049 | ` *  Returns input repeated multiplier times.` |
|      - | 3050 | ` * Parameters` |
|      - | 3051 | ` *  $string` |
|      - | 3052 | ` *   String to be repeated.` |
|      - | 3053 | ` * $multiplier` |
|      - | 3054 | ` *  Number of time the input string should be repeated.` |
|      - | 3055 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3056 | ` *  to 0, the function will return an empty string.` |
|      - | 3057 | ` * Return` |
|      - | 3058 | ` *  The repeated string.` |
|      - | 3059 | ` */` |
|  20424 | 3060 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3061 | `{` |
|      - | 3062 | `	const char *zIn;` |
|      - | 3063 | `	int nLen;` |
|      - | 3064 | `	ph7_int64 nMul;` |
|      - | 3065 | `	int rc;` |
|  20426 | 3066 | `	if( nArg < 2 ){` |
|      - | 3067 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3068 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3069 | `		return PH7_OK;` |
|      - | 3070 | `	}` |
|      - | 3071 | `	/* Extract the target string */` |
|  20426 | 3072 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3073 | `	/* Extract the multiplier as a 64-bit value (a 32-bit read would wrap a large` |
|      - | 3074 | `	 * positive $times into a negative one and trip a spurious ValueError). PHP` |
|      - | 3075 | `	 * validates $times regardless of the string contents: a negative count throws` |
|      - | 3076 | `	 * a catchable ValueError. */` |
|  20426 | 3077 | `	nMul = ph7_value_to_int64(apArg[1]);` |
|  20426 | 3078 | `	if( nMul < 0 ){` |
|      3 | 3079 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3080 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3081 | `	}` |
|  20424 | 3082 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3083 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3084 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3085 | `		return PH7_OK;` |
|      - | 3086 | `	}` |
|      - | 3087 | `	/* Perform the requested operation */` |
| 220978 | 3088 | `	for(;;){` |
| 441958 | 3089 | `		if( !nMul ){` |
|  20424 | 3090 | `			break;` |
|      - | 3091 | `		}` |
|      - | 3092 | `		/* Append the copy */` |
| 421536 | 3093 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 421536 | 3094 | `		if( rc != PH7_OK ){` |
|      - | 3095 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3096 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3097 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3098 | `		}` |
| 421536 | 3099 | `		nMul--;` |
|      2 | 3100 | `	}` |
|  20424 | 3101 | `	return PH7_OK;` |
|  10214 | 3102 | `}` |
|      - | 3103 | `/*` |
|      - | 3104 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3105 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3106 | ` * Parameters` |
|      - | 3107 | ` *  $string` |
|      - | 3108 | ` *   The input string.` |
|      - | 3109 | ` * $is_xhtml` |
|      - | 3110 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3111 | ` * Return` |
|      - | 3112 | ` *  The processed string.` |
|      - | 3113 | ` */` |
|      4 | 3114 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3115 | `{` |
|      - | 3116 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3117 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3118 | `	int nLen;` |
|      5 | 3119 | `	if( nArg < 1 ){` |
|      - | 3120 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3121 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3122 | `		return PH7_OK;` |
|      - | 3123 | `	}` |
|      - | 3124 | `	/* Extract the target string */` |
|      5 | 3125 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3126 | `	if( nLen < 1 ){` |
|      - | 3127 | `		/* Empty string,return null */` |
|    ! 0 | 3128 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3129 | `		return PH7_OK;` |
|      - | 3130 | `	}` |
|      5 | 3131 | `	if( nArg > 1 ){` |
|      3 | 3132 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3133 | `	}` |
|      5 | 3134 | `	zEnd = &zIn[nLen];` |
|      - | 3135 | `	/* Perform the requested operation */` |
|      4 | 3136 | `	for(;;){` |
|      9 | 3137 | `		zCur = zIn;` |
|      - | 3138 | `		/* Delimit the string */` |
|     21 | 3139 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3140 | `			zIn++;` |
|      1 | 3141 | `		}` |
|      9 | 3142 | `		if( zCur < zIn ){` |
|      - | 3143 | `			/* Output chunk verbatim */` |
|      9 | 3144 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3145 | `		}` |
|      9 | 3146 | `		if( zIn >= zEnd ){` |
|      - | 3147 | `			/* No more input to process */` |
|      5 | 3148 | `			break;` |
|      - | 3149 | `		}` |
|      - | 3150 | `		/* Output the HTML line break */` |
|      - | 3151 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3152 | `		if( is_xhtml ){` |
|      3 | 3153 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3154 | `		}else{` |
|      3 | 3155 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3156 | `		}` |
|      5 | 3157 | `		zCur = zIn;` |
|      - | 3158 | `		/* Append trailing line */` |
|     11 | 3159 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3160 | `			zIn++;` |
|      1 | 3161 | `		}` |
|      5 | 3162 | `		if( zCur < zIn ){` |
|      - | 3163 | `			/* Output chunk verbatim */` |
|      5 | 3164 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3165 | `		}` |
|      1 | 3166 | `	}` |
|      5 | 3167 | `	return PH7_OK;` |
|      3 | 3168 | `}` |
|      - | 3169 | `/*` |
|      - | 3170 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3171 | ` *  According to the PHP reference manual.` |
|      - | 3172 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3173 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3174 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3175 | ` * This applies to both sprintf() and printf().` |
|      - | 3176 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3177 | ` * or more of these elements, in order:` |
|      - | 3178 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3179 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3180 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3181 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3182 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3183 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3184 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3185 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3186 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3187 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3188 | ` *   should result in.` |
|      - | 3189 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3190 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3191 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3192 | ` *   limit to the string.` |
|      - | 3193 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3194 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3195 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3196 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3197 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3198 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3199 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3200 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3201 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3202 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3203 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3204 | ` *       g - shorter of %e and %f.` |
|      - | 3205 | ` *       G - shorter of %E and %f.` |
|      - | 3206 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3207 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3208 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3209 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3210 | ` */` |
|      - | 3211 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3212 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3213 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3214 | `/*` |
|      - | 3215 | `** Conversion types fall into various categories as defined by the` |
|      - | 3216 | `** following enumeration.` |
|      - | 3217 | `*/` |
|      - | 3218 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3219 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3220 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3221 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3222 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3223 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3224 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3225 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3226 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3227 |  |
|      - | 3228 | `/*` |
|      - | 3229 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3230 | `*/` |
|      - | 3231 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3232 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3233 | `/*` |
|      - | 3234 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3235 | `** by an instance of the following structure` |
|      - | 3236 | `*/` |
|      - | 3237 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3238 | `struct ph7_fmt_info` |
|      - | 3239 | `{` |
|      - | 3240 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3241 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3242 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3243 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3244 | `  char *charset; /* The character set for conversion */` |
|      - | 3245 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3246 | `};` |
|      - | 3247 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3248 | `/*` |
|      - | 3249 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3250 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3251 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3252 | `**` |
|      - | 3253 | `** Example:` |
|      - | 3254 | `**     input:     *val = 3.14159` |
|      - | 3255 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3256 | `**` |
|      - | 3257 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3258 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3259 | `** always returned.` |
|      - | 3260 | `*/` |
|    422 | 3261 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3262 | `{` |
|      - | 3263 | `  sxlongreal d;` |
|      - | 3264 | `  int digit;` |
|      - | 3265 |  |
|    423 | 3266 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3267 | `	  return '0';` |
|      - | 3268 | `  }` |
|    423 | 3269 | `  digit = (int)*val;` |
|    423 | 3270 | `  d = digit;` |
|    423 | 3271 | `   *val = (*val - d)*10.0;` |
|    423 | 3272 | `  return digit + '0' ;` |
|    212 | 3273 | `}` |
|      - | 3274 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3275 | `/*` |
|      - | 3276 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3277 | ` * used conversion types first.` |
|      - | 3278 | ` */` |
|      - | 3279 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3280 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3281 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3282 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3283 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3284 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3285 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3286 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3287 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3288 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3289 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3290 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3291 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3292 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3293 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3294 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3295 | `};` |
|      - | 3296 | `/*` |
|      - | 3297 | ` * Format a given string.` |
|      - | 3298 | ` * The root program.  All variations call this core.` |
|      - | 3299 | ` * INPUTS:` |
|      - | 3300 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3301 | ` *            1. A pointer to the call context.` |
|      - | 3302 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3303 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3304 | ` *            3. An integer number of characters to be output.` |
|      - | 3305 | ` *               (Note: This number might be zero.)` |
|      - | 3306 | ` *            4. Upper layer private data.` |
|      - | 3307 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3308 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3309 | ` */` |
|    260 | 3310 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3311 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3312 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3313 | `	const char *zIn,    /* Format string */` |
|      - | 3314 | `	int nByte,          /* Format string length */` |
|      - | 3315 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3316 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3317 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3318 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3319 | `	)` |
|      1 | 3320 | `{` |
|    261 | 3321 | `	char spaces[] = "                                                  ";` |
|      - | 3322 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    261 | 3323 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3324 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3325 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3326 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3327 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3328 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3329 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3330 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3331 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3332 | `	ph7_int64 iVal;` |
|      - | 3333 | `	int precision;           /* Precision of the current field */` |
|      - | 3334 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3335 | `	int c,rc,n;` |
|      - | 3336 | `	int length;              /* Length of the field */` |
|      - | 3337 | `	int prefix;` |
|      - | 3338 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3339 | `	int width;               /* Width of the current field */` |
|      - | 3340 | `	int idx;` |
|    261 | 3341 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3342 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3343 | `	/* Start the format process */` |
|    380 | 3344 | `	for(;;){` |
|    761 | 3345 | `		zCur = zIn;` |
|   2785 | 3346 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2025 | 3347 | `			zIn++;` |
|      1 | 3348 | `		}` |
|    761 | 3349 | `		if( zCur < zIn ){` |
|      - | 3350 | `			/* Consume chunk verbatim */` |
|    539 | 3351 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    539 | 3352 | `			if( rc != SXRET_OK ){` |
|      - | 3353 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3354 | `				break;` |
|      - | 3355 | `			}` |
|    269 | 3356 | `		}` |
|    761 | 3357 | `		if( zIn >= zEnd ){` |
|      - | 3358 | `			/* No more input to process,break immediately */` |
|    259 | 3359 | `			break;` |
|      - | 3360 | `		}` |
|      - | 3361 | `		/* Find out what flags are present */` |
|    503 | 3362 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    502 | 3363 | `			flag_alternateform = flag_zeropad = 0;` |
|    503 | 3364 | `		zIn++; /* Jump the precent sign */` |
|    251 | 3365 | `		do{` |
|    535 | 3366 | `			c = zIn[0];` |
|    535 | 3367 | `			switch( c ){` |
|      9 | 3368 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3369 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3370 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3371 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3372 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3373 | `			case '\'':` |
|    ! 0 | 3374 | `				zIn++;` |
|    ! 0 | 3375 | `				if( zIn < zEnd ){` |
|      - | 3376 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3377 | `					c = zIn[0];` |
|    ! 0 | 3378 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3379 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3380 | `					}` |
|    ! 0 | 3381 | `					c = 0;` |
|    ! 0 | 3382 | `				}` |
|    ! 0 | 3383 | `				break;` |
|    502 | 3384 | `			default:                                       break;` |
|      - | 3385 | `			}` |
|    535 | 3386 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3387 | `		/* Get the field width */` |
|    503 | 3388 | `		width = 0;` |
|    788 | 3389 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3390 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3391 | `			zIn++;` |
|      1 | 3392 | `		}` |
|    503 | 3393 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3394 | `			/* Position specifer */` |
|    ! 0 | 3395 | `			if( width > 0 ){` |
|    ! 0 | 3396 | `				n = width;` |
|    ! 0 | 3397 | `				if( vf && n > 0 ){` |
|    ! 0 | 3398 | `					n--;` |
|    ! 0 | 3399 | `				}` |
|    ! 0 | 3400 | `			}` |
|    ! 0 | 3401 | `			zIn++;` |
|    ! 0 | 3402 | `			width = 0;` |
|    ! 0 | 3403 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3404 | `				flag_zeropad = 1;` |
|    ! 0 | 3405 | `				zIn++;` |
|    ! 0 | 3406 | `			}` |
|    ! 0 | 3407 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3408 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3409 | `				zIn++;` |
|    ! 0 | 3410 | `			}` |
|    ! 0 | 3411 | `		}` |
|    503 | 3412 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3413 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3414 | `		}` |
|      - | 3415 | `		/* Get the precision */` |
|    503 | 3416 | `		precision = -1;` |
|    503 | 3417 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3418 | `			precision = 0;` |
|     59 | 3419 | `			zIn++;` |
|    150 | 3420 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3421 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3422 | `				zIn++;` |
|      1 | 3423 | `			}` |
|     29 | 3424 | `		}` |
|    503 | 3425 | `		if( zIn >= zEnd ){` |
|      - | 3426 | `			/* No more input */` |
|      3 | 3427 | `			break;` |
|      - | 3428 | `		}` |
|      - | 3429 | `		/* Fetch the info entry for the field */` |
|    501 | 3430 | `		pInfo = 0;` |
|    501 | 3431 | `		xtype = PH7_FMT_ERROR;` |
|    501 | 3432 | `		c = zIn[0];` |
|    501 | 3433 | `		zIn++; /* Jump the format specifer */` |
|   1439 | 3434 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   1437 | 3435 | `			if( c==aFmt[idx].fmttype ){` |
|    499 | 3436 | `				pInfo = &aFmt[idx];` |
|    499 | 3437 | `				xtype = pInfo->type;` |
|    499 | 3438 | `				break;` |
|      - | 3439 | `			}` |
|    470 | 3440 | `		}` |
|    501 | 3441 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    501 | 3442 | `		length = 0;` |
|      - | 3443 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3444 | `		 /*` |
|      - | 3445 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3446 | `		  **` |
|      - | 3447 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3448 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3449 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3450 | `		  **                               field width was negative.` |
|      - | 3451 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3452 | `		  **                               the conversion character.` |
|      - | 3453 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3454 | `		  **   width                       The specified field width.  This is` |
|      - | 3455 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3456 | `		  **   precision                   The specified precision.  The default` |
|      - | 3457 | `		  **                               is -1.` |
|      - | 3458 | `		  */` |
|    501 | 3459 | `		switch(xtype){` |
|    ! 0 | 3460 | `		case PH7_FMT_PERCENT:` |
|      - | 3461 | `			/* A literal percent character */` |
|    ! 0 | 3462 | `			zWorker[0] = '%';` |
|    ! 0 | 3463 | `			length = (int)sizeof(char);` |
|    ! 0 | 3464 | `			break;` |
|      3 | 3465 | `		case PH7_FMT_CHARX:` |
|      - | 3466 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3467 | `			 * with that ASCII value` |
|      - | 3468 | `			 */` |
|      7 | 3469 | `			pArg = NEXT_ARG;` |
|      7 | 3470 | `			if( pArg == 0 ){` |
|      3 | 3471 | `				c = 0;` |
|      2 | 3472 | `			}else{` |
|      5 | 3473 | `				c = ph7_value_to_int(pArg);` |
|      - | 3474 | `			}` |
|      - | 3475 | `			/* NUL byte is an acceptable value */` |
|      7 | 3476 | `			zWorker[0] = (char)c;` |
|      7 | 3477 | `			length = (int)sizeof(char);` |
|      7 | 3478 | `			break;` |
|    159 | 3479 | `		case PH7_FMT_STRING:` |
|      - | 3480 | `			/* the argument is treated as and presented as a string */` |
|    319 | 3481 | `			pArg = NEXT_ARG;` |
|    319 | 3482 | `			if( pArg == 0 ){` |
|    ! 0 | 3483 | `				length = 0;` |
|    ! 0 | 3484 | `			}else{` |
|    319 | 3485 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3486 | `			}` |
|    319 | 3487 | `			if( length < 1 ){` |
|    ! 0 | 3488 | `				zBuf = " ";` |
|    ! 0 | 3489 | `				length = (int)sizeof(char);` |
|    ! 0 | 3490 | `			}` |
|    319 | 3491 | `			if( precision>=0 && precision<length ){` |
|      3 | 3492 | `				length = precision;` |
|      1 | 3493 | `			}` |
|    319 | 3494 | `			if( flag_zeropad ){` |
|      - | 3495 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3496 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3497 | `					spaces[idx] = '0';` |
|    ! 0 | 3498 | `				}` |
|    ! 0 | 3499 | `			}` |
|    319 | 3500 | `			break;` |
|     59 | 3501 | `		case PH7_FMT_RADIX:` |
|    119 | 3502 | `			pArg = NEXT_ARG;` |
|    119 | 3503 | `			if( pArg == 0 ){` |
|    ! 0 | 3504 | `				iVal = 0;` |
|    ! 0 | 3505 | `			}else{` |
|    119 | 3506 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3507 | `			}` |
|      - | 3508 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 3509 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3510 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3511 | `			}` |
|      - | 3512 | `#if 1` |
|      - | 3513 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3514 | `        ** I think this is stupid.*/` |
|    119 | 3515 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3516 | `#else` |
|      - | 3517 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3518 | `        ** but leave the prefix for hex.*/` |
|      - | 3519 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3520 | `#endif` |
|    119 | 3521 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     89 | 3522 | `          if( iVal<0 ){` |
|     25 | 3523 | `            iVal = -iVal;` |
|      - | 3524 | `			/* Ticket 1433-003 */` |
|     25 | 3525 | `			if( iVal < 0 ){` |
|      - | 3526 | `				/* Overflow */` |
|    ! 0 | 3527 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3528 | `			}` |
|     25 | 3529 | `            prefix = '-';` |
|     77 | 3530 | `          }else if( flag_plussign )  prefix = '+';` |
|     63 | 3531 | `          else if( flag_blanksign )  prefix = ' ';` |
|     61 | 3532 | `          else                       prefix = 0;` |
|     45 | 3533 | `        }else{` |
|     31 | 3534 | `			if( iVal<0 ){` |
|    ! 0 | 3535 | `				iVal = -iVal;` |
|      - | 3536 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3537 | `				if( iVal < 0 ){` |
|      - | 3538 | `					/* Overflow */` |
|    ! 0 | 3539 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3540 | `				}` |
|    ! 0 | 3541 | `			}` |
|     31 | 3542 | `			prefix = 0;` |
|      - | 3543 | `		}` |
|    119 | 3544 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3545 | `          precision = width-(prefix!=0);` |
|      3 | 3546 | `        }` |
|    119 | 3547 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3548 | `        {` |
|      - | 3549 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3550 | `          register int base;` |
|    119 | 3551 | `          cset = pInfo->charset;` |
|    119 | 3552 | `          base = pInfo->base;` |
|     59 | 3553 | `          do{                                           /* Convert to ascii */` |
|    187 | 3554 | `            *(--zBuf) = cset[iVal%base];` |
|    187 | 3555 | `            iVal = iVal/base;` |
|    187 | 3556 | `          }while( iVal>0 );` |
|      - | 3557 | `        }` |
|    119 | 3558 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 3559 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3560 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3561 | `        }` |
|    119 | 3562 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 3563 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3564 | `          char *pre, x;` |
|      9 | 3565 | `          pre = pInfo->prefix;` |
|      9 | 3566 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3567 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3568 | `          }` |
|      4 | 3569 | `        }` |
|    119 | 3570 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 3571 | `		break;` |
|     28 | 3572 | `		case PH7_FMT_FLOAT:` |
|      - | 3573 | `		case PH7_FMT_EXP:` |
|      - | 3574 | `		case PH7_FMT_GENERIC:{` |
|      - | 3575 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3576 | `		long double realvalue;` |
|      - | 3577 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3578 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3579 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3580 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3581 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3582 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3583 | `		pArg = NEXT_ARG;` |
|     57 | 3584 | `		if( pArg == 0 ){` |
|    ! 0 | 3585 | `			realvalue = 0;` |
|    ! 0 | 3586 | `		}else{` |
|     57 | 3587 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3588 | `		}` |
|      - | 3589 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3590 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3591 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3592 | `			zBuf = "NAN";` |
|    ! 0 | 3593 | `			length = 3;` |
|    ! 0 | 3594 | `			break;` |
|      - | 3595 | `		}` |
|     57 | 3596 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3597 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3598 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3599 | `				zBuf = "-INF";` |
|    ! 0 | 3600 | `				length = 4;` |
|    ! 0 | 3601 | `			}else{` |
|    ! 0 | 3602 | `				zBuf = "INF";` |
|    ! 0 | 3603 | `				length = 3;` |
|      - | 3604 | `			}` |
|    ! 0 | 3605 | `			break;` |
|      - | 3606 | `		}` |
|     57 | 3607 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3608 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3609 | `        if( realvalue<0.0 ){` |
|      3 | 3610 | `          realvalue = -realvalue;` |
|      3 | 3611 | `          prefix = '-';` |
|      2 | 3612 | `        }else{` |
|     55 | 3613 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3614 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3615 | `          else                         prefix = 0;` |
|      - | 3616 | `        }` |
|     57 | 3617 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3618 | `        rounder = 0.0;` |
|      - | 3619 | `#if 0` |
|      - | 3620 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3621 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3622 | `#else` |
|      - | 3623 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3624 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3625 | `#endif` |
|     57 | 3626 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3627 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3628 | `        exp = 0;` |
|     57 | 3629 | `        if( realvalue>0.0 ){` |
|     61 | 3630 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3631 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3632 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3633 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3634 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3635 | `            zBuf = "NaN";` |
|    ! 0 | 3636 | `            length = 3;` |
|    ! 0 | 3637 | `            break;` |
|      - | 3638 | `          }` |
|     28 | 3639 | `        }` |
|     57 | 3640 | `        zBuf = zWorker;` |
|      - | 3641 | `        /*` |
|      - | 3642 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3643 | `        ** or etFLOAT, as appropriate.` |
|      - | 3644 | `        */` |
|     57 | 3645 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3646 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3647 | `          realvalue += rounder;` |
|    ! 0 | 3648 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3649 | `        }` |
|     57 | 3650 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3651 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3652 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3653 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3654 | `          }else{` |
|    ! 0 | 3655 | `            precision = precision - exp;` |
|    ! 0 | 3656 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3657 | `          }` |
|    ! 0 | 3658 | `        }else{` |
|     57 | 3659 | `          flag_rtz = 0;` |
|      - | 3660 | `        }` |
|      - | 3661 | `        /*` |
|      - | 3662 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3663 | `        ** the precision is too large to fit in buf[].` |
|      - | 3664 | `        */` |
|     57 | 3665 | `        nsd = 0;` |
|     57 | 3666 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3667 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3668 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3669 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3670 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3671 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3672 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3673 | `            *(zBuf++) = '0';` |
|     17 | 3674 | `          }` |
|    373 | 3675 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3676 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3677 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3678 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3679 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3680 | `          }` |
|     57 | 3681 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3682 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3683 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3684 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3685 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3686 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3687 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3688 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3689 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3690 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3691 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3692 | `          }` |
|    ! 0 | 3693 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3694 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3695 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3696 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3697 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3698 | `            if( exp>=100 ){` |
|    ! 0 | 3699 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3700 | `              exp %= 100;` |
|    ! 0 | 3701 | `            }` |
|    ! 0 | 3702 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3703 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3704 | `          }` |
|      - | 3705 | `        }` |
|      - | 3706 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3707 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3708 | `        ** integer conversions.*/` |
|     57 | 3709 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3710 | `        zBuf = zWorker;` |
|      - | 3711 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3712 | `        ** set and we are not left justified */` |
|     57 | 3713 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3714 | `          int i;` |
|      3 | 3715 | `          int nPad = width - length;` |
|     13 | 3716 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3717 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3718 | `          }` |
|      3 | 3719 | `          i = prefix!=0;` |
|      5 | 3720 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3721 | `          length = width;` |
|      1 | 3722 | `        }` |
|      - | 3723 | `#else` |
|      - | 3724 | `         zBuf = " ";` |
|      - | 3725 | `		 length = (int)sizeof(char);` |
|      - | 3726 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3727 | `		 break;` |
|      - | 3728 | `							 }` |
|      1 | 3729 | `		default:` |
|      - | 3730 | `			/* Invalid format specifer */` |
|      3 | 3731 | `			zWorker[0] = '?';` |
|      3 | 3732 | `			length = (int)sizeof(char);` |
|      2 | 3733 | `			break;` |
|      - | 3734 | `		}` |
|      - | 3735 | `		 /*` |
|      - | 3736 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3737 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3738 | `		 ** the output.` |
|      - | 3739 | `		 */` |
|    501 | 3740 | `    if( !flag_leftjustify ){` |
|      - | 3741 | `      register int nspace;` |
|    493 | 3742 | `      nspace = width-length;` |
|    493 | 3743 | `      if( nspace>0 ){` |
|      5 | 3744 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3745 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3746 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3747 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3748 | `			}` |
|    ! 0 | 3749 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3750 | `        }` |
|      5 | 3751 | `        if( nspace>0 ){` |
|      5 | 3752 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3753 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3754 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3755 | `			}` |
|      2 | 3756 | `		}` |
|      2 | 3757 | `      }` |
|    246 | 3758 | `    }` |
|    501 | 3759 | `    if( length>0 ){` |
|    501 | 3760 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    501 | 3761 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3762 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3763 | `		}` |
|    250 | 3764 | `    }` |
|    501 | 3765 | `    if( flag_leftjustify ){` |
|      - | 3766 | `      register int nspace;` |
|      9 | 3767 | `      nspace = width-length;` |
|      9 | 3768 | `      if( nspace>0 ){` |
|      9 | 3769 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3770 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3771 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3772 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3773 | `			}` |
|    ! 0 | 3774 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3775 | `        }` |
|      9 | 3776 | `        if( nspace>0 ){` |
|      9 | 3777 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3778 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3779 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3780 | `			}` |
|      4 | 3781 | `		}` |
|      4 | 3782 | `      }` |
|      4 | 3783 | `    }` |
|      1 | 3784 | ` }/* for(;;) */` |
|    261 | 3785 | `	return SXRET_OK;` |
|    131 | 3786 | `}` |
|      - | 3787 | `/*` |
|      - | 3788 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3789 | ` */` |
|     90 | 3790 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3791 | `{` |
|      - | 3792 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 3793 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 3794 | `	 * non-OK rc also stops the format loop. */` |
|     91 | 3795 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|     91 | 3796 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|     91 | 3797 | `	return *pRc;` |
|      1 | 3798 | `}` |
|      - | 3799 | `/*` |
|      - | 3800 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3801 | ` *  Return a formatted string.` |
|      - | 3802 | ` * Parameters` |
|      - | 3803 | ` *  $format` |
|      - | 3804 | ` *    The format string (see block comment above)` |
|      - | 3805 | ` * Return` |
|      - | 3806 | ` *  A string produced according to the formatting string format.` |
|      - | 3807 | ` */` |
|     60 | 3808 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3809 | `{` |
|      - | 3810 | `	const char *zFormat;` |
|     61 | 3811 | `	sxi32 rc = SXRET_OK;` |
|      - | 3812 | `	int nLen;` |
|     61 | 3813 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3814 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 3815 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3816 | `		return PH7_OK;` |
|      - | 3817 | `	}` |
|      - | 3818 | `	/* Extract the string format */` |
|     61 | 3819 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 3820 | `	if( nLen < 1 ){` |
|      - | 3821 | `		/* Empty string */` |
|    ! 0 | 3822 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3823 | `		return PH7_OK;` |
|      - | 3824 | `	}` |
|      - | 3825 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     61 | 3826 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     61 | 3827 | `	if( rc != SXRET_OK ){` |
|      - | 3828 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 3829 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 3830 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3831 | `	}` |
|     61 | 3832 | `	return PH7_OK;` |
|     31 | 3833 | `}` |
|      - | 3834 | `/*` |
|      - | 3835 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3836 | ` */` |
|    922 | 3837 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3838 | `{` |
|    923 | 3839 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3840 | `	/* Call the VM output consumer directly */` |
|    923 | 3841 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 3842 | `	/* Increment counter */` |
|    923 | 3843 | `	*pCounter += nLen;` |
|    923 | 3844 | `	return PH7_OK;` |
|      1 | 3845 | `}` |
|      - | 3846 | `/*` |
|      - | 3847 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 3848 | ` *  Output a formatted string.` |
|      - | 3849 | ` * Parameters` |
|      - | 3850 | ` *  $format` |
|      - | 3851 | ` *   See sprintf() for a description of format.` |
|      - | 3852 | ` * Return` |
|      - | 3853 | ` *  The length of the outputted string.` |
|      - | 3854 | ` */` |
|    174 | 3855 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3856 | `{` |
|    175 | 3857 | `	ph7_int64 nCounter = 0;` |
|      - | 3858 | `	const char *zFormat;` |
|      - | 3859 | `	int nLen;` |
|    175 | 3860 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3861 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 3862 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3863 | `		return PH7_OK;` |
|      - | 3864 | `	}` |
|      - | 3865 | `	/* Extract the string format */` |
|    175 | 3866 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    175 | 3867 | `	if( nLen < 1 ){` |
|      - | 3868 | `		/* Empty string */` |
|    ! 0 | 3869 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3870 | `		return PH7_OK;` |
|      - | 3871 | `	}` |
|      - | 3872 | `	/* Format the string */` |
|    175 | 3873 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 3874 | `	/* Return the length of the outputted string */` |
|    175 | 3875 | `	ph7_result_int64(pCtx,nCounter);` |
|    175 | 3876 | `	return PH7_OK;` |
|     88 | 3877 | `}` |
|      - | 3878 | `/*` |
|      - | 3879 | ` * int vprintf(string $format,array $args)` |
|      - | 3880 | ` *  Output a formatted string.` |
|      - | 3881 | ` * Parameters` |
|      - | 3882 | ` *  $format` |
|      - | 3883 | ` *   See sprintf() for a description of format.` |
|      - | 3884 | ` * Return` |
|      - | 3885 | ` *  The length of the outputted string.` |
|      - | 3886 | ` */` |
|      2 | 3887 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3888 | `{` |
|      3 | 3889 | `	ph7_int64 nCounter = 0;` |
|      - | 3890 | `	const char *zFormat;` |
|      - | 3891 | `	ph7_hashmap *pMap;` |
|      - | 3892 | `	SySet sArg;` |
|      - | 3893 | `	int nLen,n;` |
|      3 | 3894 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3895 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 3896 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3897 | `		return PH7_OK;` |
|      - | 3898 | `	}` |
|      - | 3899 | `	/* Extract the string format */` |
|      3 | 3900 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3901 | `	if( nLen < 1 ){` |
|      - | 3902 | `		/* Empty string */` |
|    ! 0 | 3903 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3904 | `		return PH7_OK;` |
|      - | 3905 | `	}` |
|      - | 3906 | `	/* Point to the hashmap */` |
|      3 | 3907 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3908 | `	/* Extract arguments from the hashmap */` |
|      3 | 3909 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3910 | `	/* Format the string */` |
|      3 | 3911 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 3912 | `	/* Return the length of the outputted string */` |
|      3 | 3913 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 3914 | `	/* Release the container */` |
|      3 | 3915 | `	SySetRelease(&sArg);` |
|      3 | 3916 | `	return PH7_OK;` |
|      2 | 3917 | `}` |
|      - | 3918 | `/*` |
|      - | 3919 | ` * int vsprintf(string $format,array $args)` |
|      - | 3920 | ` *  Output a formatted string.` |
|      - | 3921 | ` * Parameters` |
|      - | 3922 | ` *  $format` |
|      - | 3923 | ` *   See sprintf() for a description of format.` |
|      - | 3924 | ` * Return` |
|      - | 3925 | ` *  A string produced according to the formatting string format.` |
|      - | 3926 | ` */` |
|      6 | 3927 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3928 | `{` |
|      - | 3929 | `	const char *zFormat;` |
|      - | 3930 | `	ph7_hashmap *pMap;` |
|      - | 3931 | `	SySet sArg;` |
|      7 | 3932 | `	sxi32 rc = SXRET_OK;` |
|      - | 3933 | `	int nLen,n;` |
|      7 | 3934 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3935 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 3936 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3937 | `		return PH7_OK;` |
|      - | 3938 | `	}` |
|      - | 3939 | `	/* Extract the string format */` |
|      7 | 3940 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3941 | `	if( nLen < 1 ){` |
|      - | 3942 | `		/* Empty string */` |
|    ! 0 | 3943 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3944 | `		return PH7_OK;` |
|      - | 3945 | `	}` |
|      - | 3946 | `	/* Point to hashmap */` |
|      7 | 3947 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3948 | `	/* Extract arguments from the hashmap */` |
|      7 | 3949 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3950 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      7 | 3951 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 3952 | `	/* Release the container */` |
|      7 | 3953 | `	SySetRelease(&sArg);` |
|      7 | 3954 | `	if( rc != SXRET_OK ){` |
|      - | 3955 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 3956 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3957 | `	}` |
|      7 | 3958 | `	return PH7_OK;` |
|      4 | 3959 | `}` |
|      - | 3960 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 3961 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3962 | `/*` |
|      - | 3963 | ` * Symisc eXtension.` |
|      - | 3964 | ` * string size_format(int64 $size)` |
|      - | 3965 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3966 | ` *  Example:` |
|      - | 3967 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3968 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3969 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3970 | ` * Parameter` |
|      - | 3971 | ` *  $size` |
|      - | 3972 | ` *    Entity size in bytes.` |
|      - | 3973 | ` * Return` |
|      - | 3974 | ` *   Formatted string representation of the given size.` |
|      - | 3975 | ` */` |
|     24 | 3976 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3977 | `{` |
|      - | 3978 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3979 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3980 | `	sxi32 nRest,i_32;` |
|      - | 3981 | `	ph7_int64 iSize;` |
|     25 | 3982 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3983 |  |
|     25 | 3984 | `	if( nArg < 1 ){` |
|      - | 3985 | `		/* Missing argument,return the empty string */` |
|      3 | 3986 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3987 | `		return PH7_OK;` |
|      - | 3988 | `	}` |
|      - | 3989 | `	/* Extract the given size */` |
|     23 | 3990 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3991 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3992 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3993 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3994 | `		return PH7_OK;` |
|      - | 3995 | `	}` |
|     19 | 3996 | `	for(;;){` |
|     39 | 3997 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3998 | `		iSize >>= 10;` |
|     39 | 3999 | `		c++;` |
|     39 | 4000 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4001 | `			break;` |
|      - | 4002 | `		}` |
|      1 | 4003 | `	}` |
|     19 | 4004 | `	nRest /= 100;` |
|     19 | 4005 | `	if( nRest > 9 ){` |
|    ! 0 | 4006 | `		nRest = 9;` |
|    ! 0 | 4007 | `	}` |
|     19 | 4008 | `	if( iSize > 999 ){` |
|    ! 0 | 4009 | `		c++;` |
|    ! 0 | 4010 | `		nRest = 9;` |
|    ! 0 | 4011 | `		iSize = 0;` |
|    ! 0 | 4012 | `	}` |
|     19 | 4013 | `	i_32 = (sxi32)iSize;` |
|      - | 4014 | `	/* Format */` |
|     19 | 4015 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4016 | `	return PH7_OK;` |
|     13 | 4017 | `}` |
|      - | 4018 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4019 | `/*` |
|      - | 4020 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4021 | ` *   Calculate the md5 hash of a string.` |
|      - | 4022 | ` * Parameter` |
|      - | 4023 | ` *  $str` |
|      - | 4024 | ` *   Input string` |
|      - | 4025 | ` * $raw_output` |
|      - | 4026 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4027 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4028 | ` * Return` |
|      - | 4029 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4030 | ` */` |
|     12 | 4031 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4032 | `{` |
|      - | 4033 | `	unsigned char zDigest[16];` |
|     13 | 4034 | `	int raw_output = FALSE;` |
|      - | 4035 | `	const void *pIn;` |
|      - | 4036 | `	int nLen;` |
|     13 | 4037 | `	if( nArg < 1 ){` |
|      - | 4038 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4039 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4040 | `		return PH7_OK;` |
|      - | 4041 | `	}` |
|      - | 4042 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4043 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4044 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4045 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4046 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4047 | `	}` |
|      - | 4048 | `	/* Compute the MD5 digest */` |
|     13 | 4049 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4050 | `	if( raw_output ){` |
|      - | 4051 | `		/* Output raw digest */` |
|      5 | 4052 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4053 | `	}else{` |
|      - | 4054 | `		/* Perform a binary to hex conversion */` |
|      9 | 4055 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4056 | `	}` |
|     13 | 4057 | `	return PH7_OK;` |
|      7 | 4058 | `}` |
|      - | 4059 | `/*` |
|      - | 4060 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4061 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4062 | ` * Parameter` |
|      - | 4063 | ` *  $str` |
|      - | 4064 | ` *   Input string` |
|      - | 4065 | ` * $raw_output` |
|      - | 4066 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4067 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4068 | ` * Return` |
|      - | 4069 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4070 | ` */` |
|     10 | 4071 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4072 | `{` |
|      - | 4073 | `	unsigned char zDigest[20];` |
|     11 | 4074 | `	int raw_output = FALSE;` |
|      - | 4075 | `	const void *pIn;` |
|      - | 4076 | `	int nLen;` |
|     11 | 4077 | `	if( nArg < 1 ){` |
|      - | 4078 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4079 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4080 | `		return PH7_OK;` |
|      - | 4081 | `	}` |
|      - | 4082 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4083 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4084 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4085 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4086 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4087 | `	}` |
|      - | 4088 | `	/* Compute the SHA1 digest */` |
|     11 | 4089 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4090 | `	if( raw_output ){` |
|      - | 4091 | `		/* Output raw digest */` |
|      5 | 4092 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4093 | `	}else{` |
|      - | 4094 | `		/* Perform a binary to hex conversion */` |
|      7 | 4095 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4096 | `	}` |
|     11 | 4097 | `	return PH7_OK;` |
|      6 | 4098 | `}` |
|      - | 4099 | `/*` |
|      - | 4100 | ` * int64 crc32(string $str)` |
|      - | 4101 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4102 | ` * Parameter` |
|      - | 4103 | ` *  $str` |
|      - | 4104 | ` *   Input string` |
|      - | 4105 | ` * Return` |
|      - | 4106 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4107 | ` */` |
|      2 | 4108 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4109 | `{` |
|      - | 4110 | `	const void *pIn;` |
|      - | 4111 | `	sxu32 nCRC;` |
|      - | 4112 | `	int nLen;` |
|      3 | 4113 | `	if( nArg < 1 ){` |
|      - | 4114 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4115 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4116 | `		return PH7_OK;` |
|      - | 4117 | `	}` |
|      - | 4118 | `	/* Extract the input string */` |
|      3 | 4119 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4120 | `	if( nLen < 1 ){` |
|      - | 4121 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4122 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4123 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4124 | `		return PH7_OK;` |
|      - | 4125 | `	}` |
|      - | 4126 | `	/* Calculate the sum */` |
|      3 | 4127 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4128 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4129 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4130 | `	return PH7_OK;` |
|      2 | 4131 | `}` |
|      - | 4132 | `/*` |
|      - | 4133 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4134 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4135 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4136 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4137 | ` */` |
|     11 | 4138 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4139 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4140 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4141 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4142 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4143 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4144 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4145 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4146 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4147 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4148 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4149 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4150 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4151 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4152 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4153 | `struct HashAlgo {` |
|      - | 4154 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4155 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4156 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4157 | `	void (*xInit)(HashCtx *);` |
|      - | 4158 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4159 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4160 | `};` |
|      - | 4161 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4162 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4163 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4164 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4165 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4166 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4167 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4168 | `};` |
|      - | 4169 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4170 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4171 | `	sxu32 i;` |
|    279 | 4172 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4173 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4174 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4175 | `			return &aHashAlgo[i];` |
|      - | 4176 | `		}` |
|    106 | 4177 | `	}` |
|      6 | 4178 | `	return 0;` |
|     38 | 4179 | `}` |
|      - | 4180 | `/*` |
|      - | 4181 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4182 | ` *   Generate a hash value (message digest).` |
|      - | 4183 | ` */` |
|     54 | 4184 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4185 | `{` |
|      - | 4186 | `	const HashAlgo *pAlgo;` |
|      - | 4187 | `	const char *zAlgo,*zData;` |
|     56 | 4188 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4189 | `	HashCtx sCtx;` |
|      - | 4190 | `	unsigned char zDigest[64];` |
|     56 | 4191 | `	if( nArg < 2 ){` |
|    ! 0 | 4192 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4193 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4194 | `	}` |
|     56 | 4195 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4196 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4197 | `	if( pAlgo == 0 ){` |
|      3 | 4198 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4199 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4200 | `	}` |
|     53 | 4201 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4202 | `	if( nArg > 2 ){` |
|      9 | 4203 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4204 | `	}` |
|     53 | 4205 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4206 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4207 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4208 | `	if( raw_output ){` |
|      9 | 4209 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4210 | `	}else{` |
|     45 | 4211 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4212 | `	}` |
|     53 | 4213 | `	return PH7_OK;` |
|     29 | 4214 | `}` |
|      - | 4215 | `/*` |
|      - | 4216 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4217 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4218 | ` */` |
|     16 | 4219 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4220 | `{` |
|      - | 4221 | `	const HashAlgo *pAlgo;` |
|      - | 4222 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4223 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4224 | `	HashCtx sCtx;` |
|      - | 4225 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4226 | `	int i,nBlock,nDigest;` |
|     18 | 4227 | `	if( nArg < 3 ){` |
|    ! 0 | 4228 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4229 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4230 | `	}` |
|     18 | 4231 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4232 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4233 | `	if( pAlgo == 0 ){` |
|      3 | 4234 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4235 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4236 | `	}` |
|     15 | 4237 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4238 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4239 | `	if( nArg > 3 ){` |
|      3 | 4240 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4241 | `	}` |
|     15 | 4242 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4243 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4244 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4245 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4246 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4247 | `	if( nKeyLen > nBlock ){` |
|      3 | 4248 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4249 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4250 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4251 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4252 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4253 | `	}` |
|   1039 | 4254 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4255 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4256 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4257 | `	}` |
|      - | 4258 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4259 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4260 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4261 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4262 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4263 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4264 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4265 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4266 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4267 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4268 | `	if( raw_output ){` |
|      3 | 4269 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4270 | `	}else{` |
|     13 | 4271 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4272 | `	}` |
|     15 | 4273 | `	return PH7_OK;` |
|     10 | 4274 | `}` |
|      - | 4275 | `/*` |
|      - | 4276 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4277 | ` *   Timing-attack-safe string comparison.` |
|      - | 4278 | ` */` |
|     14 | 4279 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4280 | `{` |
|      - | 4281 | `	const char *zKnown,*zUser;` |
|      - | 4282 | `	int nKnown,nUser,i;` |
|     17 | 4283 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4284 | `	if( nArg < 2 ){` |
|    ! 0 | 4285 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4286 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4287 | `	}` |
|     17 | 4288 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4289 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4290 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4291 | `			ph7_type_name(apArg[0]));` |
|      - | 4292 | `	}` |
|     14 | 4293 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4294 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4295 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4296 | `			ph7_type_name(apArg[1]));` |
|      - | 4297 | `	}` |
|     11 | 4298 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4299 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4300 | `	if( nKnown != nUser ){` |
|      5 | 4301 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4302 | `		return PH7_OK;` |
|      - | 4303 | `	}` |
|      - | 4304 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4305 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4306 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4307 | `	}` |
|      7 | 4308 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4309 | `	return PH7_OK;` |
|     10 | 4310 | `}` |
|      - | 4311 | `/*` |
|      - | 4312 | ` * array hash_algos(void)` |
|      - | 4313 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4314 | ` */` |
|      2 | 4315 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4316 | `{` |
|      - | 4317 | `	ph7_value *pArray,*pValue;` |
|      - | 4318 | `	sxu32 i;` |
|      1 | 4319 | `	SXUNUSED(nArg);` |
|      1 | 4320 | `	SXUNUSED(apArg);` |
|      3 | 4321 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4322 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4323 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4324 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4325 | `		return PH7_OK;` |
|      - | 4326 | `	}` |
|     15 | 4327 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4328 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4329 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4330 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4331 | `	}` |
|      3 | 4332 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4333 | `	return PH7_OK;` |
|      2 | 4334 | `}` |
|      - | 4335 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4336 | `/*` |
|      - | 4337 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4338 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4339 | ` */` |
|      - | 4340 | `/*` |
|      - | 4341 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4342 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4343 | ` */` |
|     40 | 4344 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4345 | `{` |
|      - | 4346 | `	int iCost;` |
|     40 | 4347 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4348 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4349 | `		return FALSE;` |
|      - | 4350 | `	}` |
|     29 | 4351 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4352 | `		return FALSE;` |
|      - | 4353 | `	}` |
|     29 | 4354 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4355 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4356 | `		return FALSE;` |
|      - | 4357 | `	}` |
|     27 | 4358 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4359 | `	return TRUE;` |
|     21 | 4360 | `}` |
|      - | 4361 | `/*` |
|      - | 4362 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4363 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4364 | ` */` |
|     20 | 4365 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4366 | `{` |
|     23 | 4367 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4368 | `		return TRUE;` |
|      - | 4369 | `	}` |
|     23 | 4370 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4371 | `		int nAlgo;` |
|     23 | 4372 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4373 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4374 | `	}` |
|    ! 0 | 4375 | `	return FALSE;` |
|     13 | 4376 | `}` |
|      - | 4377 | `/*` |
|      - | 4378 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4379 | ` *  Create a bcrypt hash of the password.` |
|      - | 4380 | ` */` |
|     16 | 4381 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4382 | `{` |
|      - | 4383 | `	const char *zPwd;` |
|     19 | 4384 | `	int nPwd,iCost = 12;` |
|      - | 4385 | `	unsigned char aSalt[16];` |
|      - | 4386 | `	char zHash[60];` |
|     19 | 4387 | `	if( nArg < 2 ){` |
|    ! 0 | 4388 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4389 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4390 | `	}` |
|     19 | 4391 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4392 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4393 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4394 | `	}` |
|      - | 4395 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4396 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4397 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4398 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4399 | `	}` |
|     16 | 4400 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4401 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4402 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4403 | `	}` |
|     13 | 4404 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4405 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4406 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4407 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4408 | `	}` |
|     13 | 4409 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4410 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4411 | `		return PH7_OK;` |
|      - | 4412 | `	}` |
|     13 | 4413 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4414 | `	return PH7_OK;` |
|     11 | 4415 | `}` |
|      - | 4416 | `/*` |
|      - | 4417 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4418 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4419 | ` */` |
|     28 | 4420 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4421 | `{` |
|      - | 4422 | `	const char *zPwd,*zHash;` |
|      - | 4423 | `	int nPwd,nHash,iCost,i;` |
|      - | 4424 | `	unsigned char aSalt[16];` |
|      - | 4425 | `	char zComputed[60];` |
|     29 | 4426 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4427 | `	if( nArg < 2 ){` |
|    ! 0 | 4428 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4429 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4430 | `	}` |
|     29 | 4431 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4432 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4433 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4434 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4435 | `		return PH7_OK;` |
|      - | 4436 | `	}` |
|      - | 4437 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4438 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4439 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4440 | `		return PH7_OK;` |
|      - | 4441 | `	}` |
|     19 | 4442 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4443 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4444 | `		return PH7_OK;` |
|      - | 4445 | `	}` |
|      - | 4446 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4447 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4448 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4449 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4450 | `	}` |
|     19 | 4451 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4452 | `	return PH7_OK;` |
|     15 | 4453 | `}` |
|      - | 4454 | `/*` |
|      - | 4455 | ` * array password_get_info(string $hash)` |
|      - | 4456 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4457 | ` */` |
|      6 | 4458 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4459 | `{` |
|      7 | 4460 | `	const char *zHash = "";` |
|      7 | 4461 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4462 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4463 | `	if( nArg > 0 ){` |
|      7 | 4464 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4465 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4466 | `	}` |
|      7 | 4467 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4468 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4469 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4470 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4471 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4472 | `		return PH7_OK;` |
|      - | 4473 | `	}` |
|      7 | 4474 | `	if( bBcrypt ){` |
|      5 | 4475 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4476 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4477 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4478 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4479 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4480 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4481 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4482 | `	}else{` |
|      3 | 4483 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4484 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4485 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4486 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4487 | `	}` |
|      7 | 4488 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4489 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4490 | `	return PH7_OK;` |
|      4 | 4491 | `}` |
|      - | 4492 | `/*` |
|      - | 4493 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4494 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4495 | ` */` |
|      6 | 4496 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4497 | `{` |
|      - | 4498 | `	const char *zHash;` |
|      7 | 4499 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4500 | `	if( nArg < 2 ){` |
|    ! 0 | 4501 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4502 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4503 | `	}` |
|      7 | 4504 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4505 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4506 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4507 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4508 | `		return PH7_OK;` |
|      - | 4509 | `	}` |
|      5 | 4510 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4511 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4512 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4513 | `	}` |
|      5 | 4514 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4515 | `	return PH7_OK;` |
|      4 | 4516 | `}` |
|      - | 4517 | `/*` |
|      - | 4518 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4519 | ` *` |
|      - | 4520 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4521 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4522 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4523 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4524 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4525 | ` */` |
|      - | 4526 | `#define FV_VALIDATE_INT     257` |
|      - | 4527 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4528 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4529 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4530 | `#define FV_VALIDATE_URL     273` |
|      - | 4531 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4532 | `#define FV_VALIDATE_IP      275` |
|      - | 4533 | `#define FV_VALIDATE_MAC     276` |
|      - | 4534 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4535 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4536 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4537 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4538 | `#define FV_SANITIZE_URL     518` |
|      - | 4539 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4540 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4541 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4542 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4543 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4544 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 4545 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 4546 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 4547 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 4548 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 4549 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 4550 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 4551 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4552 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4553 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4554 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4555 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4556 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4557 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 4558 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 4559 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 4560 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 4561 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 4562 |  |
|      - | 4563 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4564 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 4565 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 4566 | `	const char *z = *pz;` |
|    153 | 4567 | `	int n = *pn;` |
|    157 | 4568 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 4569 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 4570 | `	*pz = z; *pn = n;` |
|    153 | 4571 | `}` |
|      - | 4572 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4573 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4574 | `	int neg = 0, i;` |
|     57 | 4575 | `	sxu64 u = 0;` |
|     57 | 4576 | `	FvTrim(&z,&n);` |
|     57 | 4577 | `	if( n==0 ){ return 0; }` |
|     51 | 4578 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4579 | `	if( n==0 ){ return 0; }` |
|     49 | 4580 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4581 | `		z += 2; n -= 2;` |
|      3 | 4582 | `		if( n==0 ){ return 0; }` |
|      7 | 4583 | `		for( i=0; i<n; i++ ){` |
|      5 | 4584 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4585 | `			if( h<0 ){ return 0; }` |
|      5 | 4586 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4587 | `			u = u*16 + (sxu64)h;` |
|      3 | 4588 | `		}` |
|     48 | 4589 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4590 | `		for( i=0; i<n; i++ ){` |
|      7 | 4591 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4592 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4593 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4594 | `		}` |
|      2 | 4595 | `	}else{` |
|     45 | 4596 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4597 | `		for( i=0; i<n; i++ ){` |
|    173 | 4598 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4599 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4600 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4601 | `		}` |
|      - | 4602 | `	}` |
|     33 | 4603 | `	if( neg ){` |
|      5 | 4604 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4605 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4606 | `	}else{` |
|     29 | 4607 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4608 | `		*pOut = (ph7_int64)u;` |
|      - | 4609 | `	}` |
|     31 | 4610 | `	return 1;` |
|     29 | 4611 | `}` |
|      - | 4612 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 4613 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4614 | `	char zBuf[512];` |
|     69 | 4615 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 4616 | `	const char *zv; int nv; double d = 0;` |
|     69 | 4617 | `	FvTrim(&z,&n);` |
|      - | 4618 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4619 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 4620 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 4621 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4622 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4623 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4624 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4625 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 4626 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 4627 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 4628 | `		intEnd = s;` |
|    167 | 4629 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 4630 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 4631 | `			intEnd++;` |
|      1 | 4632 | `		}` |
|     25 | 4633 | `		if( hasComma ){` |
|     25 | 4634 | `			segStart = s; segIdx = 0;` |
|    165 | 4635 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 4636 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 4637 | `					int segLen = i - segStart, k;` |
|     49 | 4638 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 4639 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 4640 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 4641 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 4642 | `						zBuf[m++] = z[k];` |
|     41 | 4643 | `					}` |
|     39 | 4644 | `					segStart = i+1; segIdx++;` |
|     19 | 4645 | `				}` |
|     71 | 4646 | `			}` |
|      8 | 4647 | `		}else{` |
|    ! 0 | 4648 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4649 | `		}` |
|     27 | 4650 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 4651 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 4652 | `			zBuf[m++] = z[i];` |
|      7 | 4653 | `		}` |
|     15 | 4654 | `		zv = zBuf; nv = m;` |
|      8 | 4655 | `	}else{` |
|     45 | 4656 | `		zv = z; nv = n;` |
|      - | 4657 | `	}` |
|     59 | 4658 | `	i = 0;` |
|     59 | 4659 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 4660 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 4661 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 4662 | `		i++;` |
|     39 | 4663 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 4664 | `	}` |
|     59 | 4665 | `	if( !seenDigit ){ return 0; }` |
|     57 | 4666 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 4667 | `		i++;` |
|     29 | 4668 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 4669 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 4670 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 4671 | `	}` |
|     57 | 4672 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4673 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 4674 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 4675 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 4676 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 4677 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 4678 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 4679 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 4680 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 4681 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 4682 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 4683 | `	zBuf[nv] = 0;` |
|     53 | 4684 | `	errno = 0;` |
|     53 | 4685 | `	d = strtod(zBuf,0);` |
|     53 | 4686 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 4687 | `		return 0;` |
|      - | 4688 | `	}` |
|     39 | 4689 | `	*pOut = d;` |
|     39 | 4690 | `	return 1;` |
|     35 | 4691 | `}` |
|      - | 4692 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4693 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4694 | ` * false, NOT failures. */` |
|     33 | 4695 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4696 | `	FvTrim(&z,&n);` |
|     32 | 4697 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4698 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4699 | `		*pBool = 1; return 1;` |
|      - | 4700 | `	}` |
|     22 | 4701 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4702 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4703 | `		*pBool = 0; return 1;` |
|      - | 4704 | `	}` |
|      9 | 4705 | `	return 0;` |
|     15 | 4706 | `}` |
|      - | 4707 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4708 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4709 | `	int i = 0, parts = 0;` |
|     77 | 4710 | `	while( i<n ){` |
|     65 | 4711 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4712 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4713 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4714 | `			if( val>255 ){ return 0; }` |
|     79 | 4715 | `			digits++; i++;` |
|      1 | 4716 | `		}` |
|     59 | 4717 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4718 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4719 | `		parts++;` |
|     45 | 4720 | `		if( parts>4 ){ return 0; }` |
|     45 | 4721 | `		if( i<n ){` |
|     33 | 4722 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4723 | `			i++;` |
|     33 | 4724 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4725 | `		}` |
|      1 | 4726 | `	}` |
|     13 | 4727 | `	return parts==4;` |
|     17 | 4728 | `}` |
|      - | 4729 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4730 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4731 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4732 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4733 | `	if( n==0 ){ return 0; }` |
|    145 | 4734 | `	while( i<=n ){` |
|    133 | 4735 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4736 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4737 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4738 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4739 | `			if( isV4 ){` |
|     11 | 4740 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4741 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4742 | `				groups += 2;` |
|      3 | 4743 | `			}else{` |
|     13 | 4744 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4745 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4746 | `				groups++;` |
|      - | 4747 | `			}` |
|     17 | 4748 | `			segStart = i+1;` |
|      8 | 4749 | `		}` |
|    127 | 4750 | `		i++;` |
|      1 | 4751 | `	}` |
|     13 | 4752 | `	return groups;` |
|     10 | 4753 | `}` |
|      - | 4754 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4755 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4756 | `	const char *zDbl = 0;` |
|      - | 4757 | `	int i, ga, gb;` |
|    139 | 4758 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4759 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4760 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4761 | `			zDbl = z+i;` |
|      5 | 4762 | `		}` |
|     61 | 4763 | `	}` |
|     17 | 4764 | `	if( zDbl==0 ){` |
|      9 | 4765 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4766 | `	}else{` |
|      9 | 4767 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4768 | `		int lenB = n - lenA - 2;` |
|      9 | 4769 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4770 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4771 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4772 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4773 | `	}` |
|     10 | 4774 | `}` |
|     25 | 4775 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4776 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4777 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4778 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4779 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4780 | `	return 0;` |
|     13 | 4781 | `}` |
|      - | 4782 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 4783 | `static int FvValidateMac(const char *z,int n){` |
|      - | 4784 | `	char sep;` |
|      - | 4785 | `	int i;` |
|     11 | 4786 | `	if( n!=17 ){ return 0; }` |
|      7 | 4787 | `	sep = z[2];` |
|      7 | 4788 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 4789 | `	for( i=0; i<17; i++ ){` |
|    101 | 4790 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 4791 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 4792 | `	}` |
|      5 | 4793 | `	return 1;` |
|      6 | 4794 | `}` |
|      - | 4795 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 4796 | ` * parts or IP-literal domains). */` |
|     28 | 4797 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 4798 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 4799 | `	const char *zDom;` |
|     28 | 4800 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 4801 | `	for( i=0; i<n; i++ ){` |
|    181 | 4802 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 4803 | `	}` |
|     21 | 4804 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 4805 | `	localLen = at;` |
|     21 | 4806 | `	zDom = z + at + 1;` |
|     21 | 4807 | `	domLen = n - at - 1;` |
|     21 | 4808 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 4809 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 4810 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 4811 | `		if( c<=' ' ){ return 0; }` |
|     41 | 4812 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 4813 | `	}` |
|     15 | 4814 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 4815 | `	labelStart = 0;` |
|     85 | 4816 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 4817 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 4818 | `			int ll = i - labelStart;` |
|     25 | 4819 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 4820 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 4821 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 4822 | `			labelStart = i+1;` |
|     12 | 4823 | `		}else{` |
|     51 | 4824 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 4825 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 4826 | `		}` |
|     37 | 4827 | `	}` |
|     11 | 4828 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 4829 | `	return 1;` |
|     15 | 4830 | `}` |
|      - | 4831 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 4832 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 4833 | `	int i;` |
|     11 | 4834 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 4835 | `	for( i=0; i<n; i++ ){` |
|     75 | 4836 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 4837 | `		if( c<=' ' ){ return 0; }` |
|     75 | 4838 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 4839 | `	}` |
|      7 | 4840 | `	return 1;` |
|      6 | 4841 | `}` |
|      - | 4842 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 4843 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 4844 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 4845 | `	SyhttpUri sUri;` |
|     15 | 4846 | `	if( n==0 ){ return 0; }` |
|     15 | 4847 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 4848 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 4849 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 4850 | `}` |
|      - | 4851 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 4852 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 4853 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 4854 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 4855 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 4856 | `	int i, runStart = 0;` |
|     37 | 4857 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 4858 | `	for( i=0; i<n; i++ ){` |
|     91 | 4859 | `		char c = z[i];` |
|     91 | 4860 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 4861 | `		if( !keep && isFloat ){` |
|     38 | 4862 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 4863 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 4864 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 4865 | `		}` |
|     61 | 4866 | `		if( !keep ){` |
|     33 | 4867 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 4868 | `			runStart = i+1;` |
|     16 | 4869 | `		}` |
|     31 | 4870 | `	}` |
|      7 | 4871 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 4872 | `}` |
|      - | 4873 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 4874 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 4875 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 4876 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 4877 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 4878 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 4879 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 4880 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 4881 | `	return 0;` |
|    144 | 4882 | `}` |
|      - | 4883 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 4884 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 4885 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 4886 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 4887 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 4888 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 4889 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 4890 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 4891 | `	int i, runStart = 0;` |
|     25 | 4892 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 4893 | `	for( i=0; i<n; i++ ){` |
|    179 | 4894 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 4895 | `		if( FvStripByte(c,flags) ){` |
|     13 | 4896 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 4897 | `			runStart = i+1;` |
|     13 | 4898 | `			continue;` |
|      - | 4899 | `		}` |
|    167 | 4900 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 4901 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 4902 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 4903 | `			runStart = i+1;` |
|    166 | 4904 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 4905 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 4906 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 4907 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 4908 | `			runStart = i+1;` |
|      4 | 4909 | `		}` |
|     79 | 4910 | `	}` |
|     15 | 4911 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 4912 | `}` |
|      - | 4913 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 4914 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 4915 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 4916 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 4917 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 4918 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 4919 | `	int i, runStart = 0;` |
|      - | 4920 | `	const char *zEnt;` |
|     13 | 4921 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 4922 | `	for( i=0; i<n; i++ ){` |
|    119 | 4923 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 4924 | `		if( FvStripByte(c,flags) ){` |
|      9 | 4925 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 4926 | `			runStart = i+1;` |
|      9 | 4927 | `			continue;` |
|      - | 4928 | `		}` |
|    111 | 4929 | `		switch( c ){` |
|      3 | 4930 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 4931 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 4932 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 4933 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 4934 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 4935 | `		default:` |
|      - | 4936 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 4937 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 4938 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 4939 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 4940 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 4941 | `				runStart = i+1;` |
|      8 | 4942 | `			}` |
|     93 | 4943 | `			continue; /* keep in the current run */` |
|      - | 4944 | `		}` |
|     19 | 4945 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 4946 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 4947 | `		runStart = i+1;` |
|     10 | 4948 | `	}` |
|     13 | 4949 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 4950 | `}` |
|      - | 4951 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 4952 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 4953 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 4954 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 4955 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 4956 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 4957 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 4958 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 4959 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 4960 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 4961 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 4962 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 4963 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 4964 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 4965 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 4966 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 4967 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 4968 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 4969 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 4970 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 4971 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 4972 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 4973 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 4974 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 4975 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 4976 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 4977 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 4978 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 4979 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 4980 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 4981 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 4982 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 4983 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 4984 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 4985 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 4986 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 4987 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 4988 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 4989 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 4990 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 4991 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 4992 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 4993 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 4994 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 4995 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 4996 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 4997 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 4998 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 4999 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 5000 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 5001 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 5002 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 5003 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 5004 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 5005 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 5006 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 5007 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 5008 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 5009 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 5010 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 5011 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 5012 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 5013 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 5014 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 5015 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 5016 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 5017 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 5018 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 5019 | `};` |
|      - | 5020 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 5021 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 5022 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 5023 | `	while( lo <= hi ){` |
|    309 | 5024 | `		int mid = (lo + hi) / 2;` |
|    309 | 5025 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 5026 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 5027 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 5028 | `	}` |
|     15 | 5029 | `	return 0;` |
|     21 | 5030 | `}` |
|      - | 5031 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 5032 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 5033 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 5034 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 5035 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 5036 | `	unsigned char c = p[0];` |
|    101 | 5037 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 5038 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 5039 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 5040 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 5041 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 5042 | `		return 2;` |
|      - | 5043 | `	}` |
|     53 | 5044 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 5045 | `		sxu32 cp;` |
|     47 | 5046 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 5047 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 5048 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 5049 | `		*pCp = cp;` |
|     29 | 5050 | `		return 3;` |
|      - | 5051 | `	}` |
|      7 | 5052 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 5053 | `		sxu32 cp;` |
|      5 | 5054 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 5055 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 5056 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 5057 | `		*pCp = cp;` |
|      5 | 5058 | `		return 4;` |
|      - | 5059 | `	}` |
|      3 | 5060 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 5061 | `}` |
|      - | 5062 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 5063 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 5064 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 5065 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 5066 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 5067 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 5068 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 5069 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 5070 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 5071 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 5072 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5073 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 5074 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 5075 | `}` |
|      - | 5076 | `/* ---------------------------------------------------------------------------` |
|      - | 5077 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 5078 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 5079 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 5080 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 5081 | ` * ------------------------------------------------------------------------ */` |
|      - | 5082 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 5083 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 5084 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 5085 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 5086 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 5087 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 5088 | `}` |
|      - | 5089 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 5090 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 5091 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 5092 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 5093 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 5094 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 5095 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 5096 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 5097 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 5098 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 5099 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 5100 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 5101 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 5102 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 5103 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 5104 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 5105 | `	}` |
|     71 | 5106 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 5107 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 5108 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 5109 | `	}` |
|     71 | 5110 | `	return 1;` |
|     46 | 5111 | `}` |
|      - | 5112 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 5113 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 5114 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 5115 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 5116 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 5117 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 5118 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 5119 | `}` |
|      - | 5120 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 5121 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 5122 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 5123 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 5124 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 5125 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 5126 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 5127 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 5128 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 5129 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 5130 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 5131 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 5132 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 5133 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 5134 | `	return 1;` |
|      5 | 5135 | `}` |
|      - | 5136 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 5137 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 5138 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 5139 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 5140 | ` * start a new sequence is left for the next round. */` |
|      5 | 5141 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 5142 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 5143 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 5144 | `	unsigned char c = p[0];` |
|     15 | 5145 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 5146 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 5147 | `	if( c < 0xE0 ){` |
|      3 | 5148 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 5149 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 5150 | `	}` |
|     11 | 5151 | `	if( c < 0xF0 ){` |
|     11 | 5152 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 5153 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 5154 | `		}` |
|      9 | 5155 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5156 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5157 | `		return 3;` |
|      - | 5158 | `	}` |
|    ! 0 | 5159 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 5160 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 5161 | `	}` |
|    ! 0 | 5162 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5163 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5164 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 5165 | `	return 4;` |
|      8 | 5166 | `}` |
|      - | 5167 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 5168 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 5169 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 5170 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 5171 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 5172 | `};` |
|      - | 5173 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 5174 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 5175 | ` * HTML 4.01 table (documented divergence, PLAN.md §3.9). */` |
|     63 | 5176 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 5177 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 5178 | `}` |
|      - | 5179 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 5180 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 5181 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 5182 | ` * whichever function the requested table belongs to. */` |
|     29 | 5183 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 5184 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 5185 | `		return "&#039;";` |
|      - | 5186 | `	}` |
|      9 | 5187 | `	return "&apos;";` |
|     15 | 5188 | `}` |
|      - | 5189 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 5190 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 5191 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 5192 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 5193 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 5194 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 5195 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 5196 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 5197 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 5198 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 5199 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 5200 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 5201 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 5202 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 5203 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 5204 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5205 | `	sxu32 n;` |
|    173 | 5206 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 5207 | `	if( z[1] == '#' ){` |
|      - | 5208 | `		/* Numeric reference */` |
|     89 | 5209 | `		sxu32 cp = 0;` |
|     89 | 5210 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 5211 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 5212 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 5213 | `			int v;` |
|    221 | 5214 | `			unsigned char c = z[i];` |
|    221 | 5215 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 5216 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 5217 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 5218 | `			else { return 0; }` |
|      - | 5219 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 5220 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 5221 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 5222 | `			nDig++;` |
|    111 | 5223 | `		}` |
|     97 | 5224 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 5225 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 5226 | `		if( !bFull ){` |
|      - | 5227 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 5228 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 5229 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 5230 | `		}` |
|     75 | 5231 | `		*pCp = cp;` |
|     75 | 5232 | `		*pnConsumed = i + 1;` |
|     75 | 5233 | `		return 1;` |
|      - | 5234 | `	}` |
|      - | 5235 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 5236 | `	 * else can bail out before touching the tables. */` |
|     81 | 5237 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 5238 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 5239 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 5240 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 5241 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 5242 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 5243 | `			return 1;` |
|      - | 5244 | `		}` |
|     96 | 5245 | `	}` |
|     23 | 5246 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5247 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 5248 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 5249 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 5250 | `		 * for ~96% of rows. */` |
|   3369 | 5251 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 5252 | `			sxu32 nEnt;` |
|   3357 | 5253 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 5254 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 5255 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 5256 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 5257 | `				*pnConsumed = (int)nEnt;` |
|      7 | 5258 | `				return 1;` |
|      - | 5259 | `			}` |
|     58 | 5260 | `		}` |
|      6 | 5261 | `	}` |
|     17 | 5262 | `	return 0;` |
|     88 | 5263 | `}` |
|      - | 5264 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 5265 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 5266 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 5267 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 5268 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 5269 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5270 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 5271 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 5272 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 5273 | `	const unsigned char *runStart;` |
|     95 | 5274 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5275 | `	sxu32 cp;` |
|     95 | 5276 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 5277 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 5278 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 5279 | `		while( p < zEnd ){` |
|      - | 5280 | `			int len;` |
|    323 | 5281 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 5282 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 5283 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 5284 | `			p += len;` |
|      1 | 5285 | `		}` |
|     59 | 5286 | `		p = (const unsigned char *)zIn;` |
|     29 | 5287 | `	}` |
|     85 | 5288 | `	runStart = p;` |
|     85 | 5289 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 5290 | `	while( p < zEnd ){` |
|    371 | 5291 | `		const char *zEnt = 0;` |
|      - | 5292 | `		int len;` |
|    371 | 5293 | `		if( *p < 0x80 ){` |
|    307 | 5294 | `			len = 1;` |
|    307 | 5295 | `			switch( *p ){` |
|     25 | 5296 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 5297 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 5298 | `			case '&':` |
|     37 | 5299 | `				zEnt = "&amp;";` |
|     37 | 5300 | `				if( !bDoubleEncode ){` |
|      - | 5301 | `					sxu32 eCp; int nEat;` |
|     25 | 5302 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 5303 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 5304 | `						zEnt = 0;` |
|     13 | 5305 | `						len = nEat;` |
|      6 | 5306 | `					}` |
|     12 | 5307 | `				}` |
|     37 | 5308 | `				break;` |
|     10 | 5309 | `			case '"':` |
|     21 | 5310 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 5311 | `				break;` |
|     12 | 5312 | `			case '\'':` |
|     25 | 5313 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 5314 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 5315 | `				}` |
|     25 | 5316 | `				break;` |
|     89 | 5317 | `			default:` |
|    179 | 5318 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 5319 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5320 | `				}` |
|    178 | 5321 | `				break;` |
|      - | 5322 | `			}` |
|    154 | 5323 | `		}else{` |
|     65 | 5324 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 5325 | `			if( len == 0 ){` |
|      - | 5326 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 5327 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 5328 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 5329 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 5330 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 5331 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 5332 | `				runStart = p;` |
|     15 | 5333 | `				continue;` |
|      - | 5334 | `			}` |
|     51 | 5335 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 5336 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 5337 | `			}` |
|     51 | 5338 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 5339 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5340 | `			}` |
|      - | 5341 | `		}` |
|    357 | 5342 | `		if( zEnt ){` |
|    135 | 5343 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 5344 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 5345 | `			runStart = p + len;` |
|     67 | 5346 | `		}` |
|    357 | 5347 | `		p += len;` |
|      1 | 5348 | `	}` |
|     85 | 5349 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 5350 | `}` |
|      - | 5351 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 5352 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 5353 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 5354 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 5355 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 5356 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5357 | `                         int iFlags,int bFull){` |
|     83 | 5358 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 5359 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 5360 | `	const unsigned char *runStart = p;` |
|     83 | 5361 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 5362 | `	while( p < zEnd ){` |
|      - | 5363 | `		sxu32 cp;` |
|      - | 5364 | `		int nEat;` |
|    510 | 5365 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 5366 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 5367 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 5368 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 5369 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 5370 | `			p += nEat;` |
|     37 | 5371 | `			continue;` |
|      - | 5372 | `		}` |
|     89 | 5373 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 5374 | `		{` |
|      - | 5375 | `			char zBuf[4];` |
|     89 | 5376 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 5377 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 5378 | `		}` |
|     89 | 5379 | `		p += nEat;` |
|     89 | 5380 | `		runStart = p;` |
|      1 | 5381 | `	}` |
|     79 | 5382 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 5383 | `}` |
|      - | 5384 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 5385 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 5386 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only per` |
|      - | 5387 | ` * PLAN.md §6 — raises PHP's unsupported-charset warning and is treated as` |
|      - | 5388 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 5389 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 5390 | `	const char *zCs;` |
|      - | 5391 | `	int nCs;` |
|    148 | 5392 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 5393 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 5394 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 5395 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 5396 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 5397 | `	}` |
|    ! 0 | 5398 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5399 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 5400 | `}` |
|      - | 5401 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 5402 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 5403 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 5404 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 5405 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 5406 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 5407 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 5408 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 5409 | `}` |
|     13 | 5410 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 5411 | `	ph7_value *pArray,*pValue;` |
|     13 | 5412 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5413 | `	sxu32 n;` |
|     13 | 5414 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5415 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5416 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 5417 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5418 | `		return;` |
|      - | 5419 | `	}` |
|     13 | 5420 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 5421 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 5422 | `	}` |
|     13 | 5423 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 5424 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 5425 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 5426 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 5427 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 5428 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 5429 | `	}` |
|     13 | 5430 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 5431 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 5432 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5433 | `		char zKey[8];` |
|    499 | 5434 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 5435 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 5436 | `			zKey[nK] = 0;` |
|    497 | 5437 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 5438 | `		}` |
|      1 | 5439 | `	}` |
|     13 | 5440 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5441 | `}` |
|     25 | 5442 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5443 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5444 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5445 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5446 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5447 | `}` |
|     23 | 5448 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5449 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5450 | `}` |
|      - | 5451 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5452 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5453 | `	int i, runStart = 0;` |
|      5 | 5454 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5455 | `	for( i=0; i<n; i++ ){` |
|     47 | 5456 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5457 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5458 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5459 | `			runStart = i+1;` |
|      5 | 5460 | `		}` |
|     24 | 5461 | `	}` |
|      5 | 5462 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5463 | `}` |
|      - | 5464 | `/*` |
|      - | 5465 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 5466 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 5467 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 5468 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 5469 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 5470 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 5471 | ` */` |
|    316 | 5472 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 5473 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 5474 | `                         ph7_value *pDefault)` |
|      3 | 5475 | `{` |
|    319 | 5476 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5477 | `	const char *zVal; int nVal;` |
|      - | 5478 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 5479 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 5480 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 5481 | `	switch( iFilter ){` |
|     28 | 5482 | `	case FV_VALIDATE_INT: {` |
|      - | 5483 | `		ph7_int64 v;` |
|     58 | 5484 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5485 | `		if( pOpts ){` |
|      7 | 5486 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5487 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5488 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5489 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5490 | `		}` |
|     29 | 5491 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5492 | `		return PH7_OK;` |
|      - | 5493 | `	}` |
|     34 | 5494 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5495 | `		double d;` |
|     69 | 5496 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 5497 | `		ph7_result_double(pCtx,d);` |
|     39 | 5498 | `		return PH7_OK;` |
|      - | 5499 | `	}` |
|     14 | 5500 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5501 | `		int b;` |
|     29 | 5502 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5503 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5504 | `		return PH7_OK;` |
|      - | 5505 | `	}` |
|     25 | 5506 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5507 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 5508 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5509 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5510 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5511 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5512 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5513 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5514 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5515 | `		if( pRe==0 ){` |
|      3 | 5516 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5517 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5518 | `		}` |
|      5 | 5519 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5520 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5521 | `		goto pass;` |
|      - | 5522 | `#else` |
|      - | 5523 | `		goto fail;` |
|      - | 5524 | `#endif` |
|      - | 5525 | `	}` |
|      3 | 5526 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5527 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 5528 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 5529 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 5530 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5531 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 5532 | `	case FV_DEFAULT:` |
|      - | 5533 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 5534 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 5535 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 5536 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 5537 | `			return PH7_OK;` |
|      - | 5538 | `		}` |
|     14 | 5539 | `		goto pass;` |
|    ! 0 | 5540 | `	default:` |
|    ! 0 | 5541 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5542 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5543 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5544 | `	}` |
|     58 | 5545 | `fail:` |
|    118 | 5546 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 5547 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 5548 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 5549 | `	return PH7_OK;` |
|     26 | 5550 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 5551 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 5552 | `	return PH7_OK;` |
|    161 | 5553 | `}` |
|      - | 5554 | `/*` |
|      - | 5555 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 5556 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 5557 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 5558 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 5559 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 5560 | ` */` |
|    328 | 5561 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 5562 | `                              int *piFilter,int *piFlags,` |
|      - | 5563 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 5564 | `{` |
|    331 | 5565 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 5566 | `	if( nArg>iBase+1 ){` |
|     88 | 5567 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 5568 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 5569 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 5570 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 5571 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 5572 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 5573 | `		}else{` |
|     48 | 5574 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 5575 | `		}` |
|     43 | 5576 | `	}` |
|    331 | 5577 | `}` |
|      - | 5578 | `/*` |
|      - | 5579 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5580 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 5581 | ` */` |
|    306 | 5582 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5583 | `{` |
|    308 | 5584 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 5585 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 5586 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 5587 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 5588 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 5589 | `}` |
|      - | 5590 | `/*` |
|      - | 5591 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5592 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 5593 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 5594 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 5595 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 5596 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 5597 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 5598 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 5599 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 5600 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 5601 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 5602 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 5603 | ` *  php's snapshot.` |
|      - | 5604 | ` */` |
|     28 | 5605 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5606 | `{` |
|     30 | 5607 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 5608 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 5609 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 5610 | `	if( nArg<2 ){` |
|      7 | 5611 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 5612 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 5613 | `	}` |
|     26 | 5614 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 5615 | `	switch( iType ){` |
|      3 | 5616 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 5617 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 5618 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 5619 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 5620 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 5621 | `	default:` |
|      3 | 5622 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5623 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 5624 | `	}` |
|     23 | 5625 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 5626 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 5627 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 5628 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 5629 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 5630 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 5631 | `	if( pElem==0 ){` |
|      - | 5632 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 5633 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 5634 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 5635 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 5636 | `		else { ph7_result_null(pCtx); }` |
|     13 | 5637 | `		return PH7_OK;` |
|      - | 5638 | `	}` |
|     11 | 5639 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 5640 | `}` |
|      - | 5641 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5642 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5643 | `/*` |
|      - | 5644 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5645 |  |
|      - | 5646 | ` */` |
|      4 | 5647 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5648 | `	const char *zInput, /* Raw input */` |
|      - | 5649 | `	int nByte,  /* Input length */` |
|      - | 5650 | `	int delim,  /* Delimiter */` |
|      - | 5651 | `	int encl,   /* Enclosure */` |
|      - | 5652 | `	int escape,  /* Escape character */` |
|      - | 5653 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5654 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5655 | `	)` |
|      1 | 5656 | `{` |
|      5 | 5657 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5658 | `	const char *zIn = zInput;` |
|      - | 5659 | `	const char *zPtr;` |
|      - | 5660 | `	int isEnc;` |
|      - | 5661 | `	/* Start processing */` |
|      8 | 5662 | `	for(;;){` |
|     17 | 5663 | `		if( zIn >= zEnd ){` |
|      - | 5664 | `			/* No more input to process */` |
|      5 | 5665 | `			break;` |
|      - | 5666 | `		}` |
|     13 | 5667 | `		isEnc = 0;` |
|     13 | 5668 | `		zPtr = zIn;` |
|      - | 5669 | `		/* Find the first delimiter */` |
|     27 | 5670 | `		while( zIn < zEnd ){` |
|     23 | 5671 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5672 | `				/* Delimiter found,break imediately */` |
|      5 | 5673 | `				break;` |
|     15 | 5674 | `			}else if( zIn[0] == encl ){` |
|      - | 5675 | `				/* Inside enclosure? */` |
|    ! 0 | 5676 | `				isEnc = !isEnc;` |
|     15 | 5677 | `			}else if( zIn[0] == escape ){` |
|      - | 5678 | `				/* Escape sequence */` |
|    ! 0 | 5679 | `				zIn++;` |
|    ! 0 | 5680 | `			}` |
|      - | 5681 | `			/* Advance the cursor */` |
|     15 | 5682 | `			zIn++;` |
|      1 | 5683 | `		}` |
|     13 | 5684 | `		if( zIn > zPtr ){` |
|     13 | 5685 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5686 | `			sxi32 rc;` |
|      - | 5687 | `			/* Invoke the supllied callback */` |
|     13 | 5688 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5689 | `				zPtr++;` |
|    ! 0 | 5690 | `				nByteChunk-=2;` |
|    ! 0 | 5691 | `			}` |
|     13 | 5692 | `			if( nByteChunk > 0 ){` |
|     13 | 5693 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5694 | `				if( rc == SXERR_ABORT ){` |
|      - | 5695 | `					/* User callback request an operation abort */` |
|    ! 0 | 5696 | `					break;` |
|      - | 5697 | `				}` |
|      6 | 5698 | `			}` |
|      6 | 5699 | `		}` |
|      - | 5700 | `		/* Ignore trailing delimiter */` |
|     21 | 5701 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5702 | `			zIn++;` |
|      1 | 5703 | `		}` |
|      1 | 5704 | `	}` |
|      5 | 5705 | `	return SXRET_OK;` |
|      1 | 5706 | `}` |
|      - | 5707 | `/*` |
|      - | 5708 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5709 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5710 | ` * argument to this callback.` |
|      - | 5711 | ` */` |
|     12 | 5712 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5713 | `{` |
|     13 | 5714 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5715 | `	ph7_value sEntry;` |
|      - | 5716 | `	SyString sToken;` |
|      - | 5717 | `	/* Insert the token in the given array */` |
|     13 | 5718 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5719 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5720 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5721 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5722 | `		return SXRET_OK;` |
|      - | 5723 | `	}` |
|     13 | 5724 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5725 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5726 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5727 | `	return SXRET_OK;` |
|      7 | 5728 | `}` |
|      - | 5729 | `/*` |
|      - | 5730 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5731 | ` *  Parse a CSV string into an array.` |
|      - | 5732 | ` * Parameters` |
|      - | 5733 | ` *  $input` |
|      - | 5734 | ` *   The string to parse.` |
|      - | 5735 | ` *  $delimiter` |
|      - | 5736 | ` *   Set the field delimiter (one character only).` |
|      - | 5737 | ` *  $enclosure` |
|      - | 5738 | ` *   Set the field enclosure character (one character only).` |
|      - | 5739 | ` *  $escape` |
|      - | 5740 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5741 | ` * Return` |
|      - | 5742 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5743 | ` */` |
|      2 | 5744 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5745 | `{` |
|      - | 5746 | `	const char *zInput,*zPtr;` |
|      - | 5747 | `	ph7_value *pArray;` |
|      3 | 5748 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 5749 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 5750 | `	int escape = '\\';  /* Escape character */` |
|      - | 5751 | `	int nLen;` |
|      3 | 5752 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5753 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 5754 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5755 | `		return PH7_OK;` |
|      - | 5756 | `	}` |
|      - | 5757 | `	/* Extract the raw input */` |
|      3 | 5758 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5759 | `	if( nArg > 1 ){` |
|      - | 5760 | `		int i;` |
|      3 | 5761 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5762 | `			/* Extract the delimiter */` |
|      3 | 5763 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5764 | `			if( i > 0 ){` |
|      3 | 5765 | `				delim = zPtr[0];` |
|      1 | 5766 | `			}` |
|      1 | 5767 | `		}` |
|      3 | 5768 | `		if( nArg > 2 ){` |
|      3 | 5769 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5770 | `				/* Extract the enclosure */` |
|      3 | 5771 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5772 | `				if( i > 0 ){` |
|      3 | 5773 | `					encl = zPtr[0];` |
|      1 | 5774 | `				}` |
|      1 | 5775 | `			}` |
|      3 | 5776 | `			if( nArg > 3 ){` |
|      3 | 5777 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5778 | `					/* Extract the escape character */` |
|      3 | 5779 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5780 | `					if( i > 0 ){` |
|      3 | 5781 | `						escape = zPtr[0];` |
|      1 | 5782 | `					}` |
|      1 | 5783 | `				}` |
|      1 | 5784 | `			}` |
|      1 | 5785 | `		}` |
|      1 | 5786 | `	}` |
|      - | 5787 | `	/* Create our array */` |
|      3 | 5788 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5789 | `	if( pArray == 0 ){` |
|      - | 5790 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5791 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5792 | `	}` |
|      - | 5793 | `	/* Parse the raw input */` |
|      3 | 5794 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5795 | `	/* Return the freshly created array */` |
|      3 | 5796 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5797 | `	return PH7_OK;` |
|      2 | 5798 | `}` |
|      - | 5799 | `/*` |
|      - | 5800 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5801 | ` * container.` |
|      - | 5802 | ` * Refer to [strip_tags()].` |
|      - | 5803 | ` */` |
|     10 | 5804 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5805 | `{` |
|     11 | 5806 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5807 | `	const char *zPtr;` |
|      - | 5808 | `	SyString sEntry;` |
|      - | 5809 | `	/* Strip tags */` |
|     10 | 5810 | `	for(;;){` |
|     45 | 5811 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5812 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5813 | `				zTag++;` |
|      1 | 5814 | `		}` |
|     21 | 5815 | `		if( zTag >= zEnd ){` |
|     11 | 5816 | `			break;` |
|      - | 5817 | `		}` |
|     11 | 5818 | `		zPtr = zTag;` |
|      - | 5819 | `		/* Delimit the tag */` |
|     25 | 5820 | `		while(zTag < zEnd ){` |
|     25 | 5821 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5822 | `				/* UTF-8 stream */` |
|      3 | 5823 | `				zTag++;` |
|      5 | 5824 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5825 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5826 | `				break;` |
|    ! 0 | 5827 | `			}else{` |
|     13 | 5828 | `				zTag++;` |
|      - | 5829 | `			}` |
|      1 | 5830 | `		}` |
|     11 | 5831 | `		if( zTag > zPtr ){` |
|      - | 5832 | `			/* Perform the insertion */` |
|     11 | 5833 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5834 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5835 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5836 | `		}` |
|      - | 5837 | `		/* Jump the trailing '>' */` |
|     11 | 5838 | `		zTag++;` |
|      1 | 5839 | `	}` |
|     11 | 5840 | `	return SXRET_OK;` |
|      1 | 5841 | `}` |
|      - | 5842 | `/*` |
|      - | 5843 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5844 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5845 | ` * Refer to [strip_tags()].` |
|      - | 5846 | ` */` |
|     36 | 5847 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5848 | `{` |
|     37 | 5849 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5850 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5851 | `		SyString sTag;` |
|     85 | 5852 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5853 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5854 | `			zTag++;` |
|      1 | 5855 | `		}` |
|      - | 5856 | `		/* Delimit the tag */` |
|     25 | 5857 | `		zCur = zTag;` |
|     77 | 5858 | `		while(zTag < zEnd ){` |
|     77 | 5859 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5860 | `				/* UTF-8 stream */` |
|      5 | 5861 | `				zTag++;` |
|      9 | 5862 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5863 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5864 | `				break;` |
|    ! 0 | 5865 | `			}else{` |
|     49 | 5866 | `				zTag++;` |
|      - | 5867 | `			}` |
|      1 | 5868 | `		}` |
|     25 | 5869 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5870 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5871 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5872 | `		if( sTag.nByte > 0 ){` |
|      - | 5873 | `			SyString *aEntry,*pEntry;` |
|      - | 5874 | `			sxi32 rc;` |
|      - | 5875 | `			sxu32 n;` |
|      - | 5876 | `			/* Perform the lookup */` |
|     25 | 5877 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5878 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5879 | `				pEntry = &aEntry[n];` |
|      - | 5880 | `				/* Do the comparison */` |
|     25 | 5881 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5882 | `				if( !rc ){` |
|     21 | 5883 | `					return SXRET_OK;` |
|      - | 5884 | `				}` |
|      3 | 5885 | `			}` |
|      2 | 5886 | `		}` |
|      2 | 5887 | `	}` |
|      - | 5888 | `	/* No such tag */` |
|     17 | 5889 | `	return SXERR_NOTFOUND;` |
|     19 | 5890 | `}` |
|      - | 5891 | `/*` |
|      - | 5892 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5893 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5894 | ` * Refer to [strip_tags()].` |
|      - | 5895 | ` */` |
|     16 | 5896 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5897 | `{` |
|     17 | 5898 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5899 | `	const char *zPtr,*zTag;` |
|      - | 5900 | `	SySet sSet;` |
|      - | 5901 | `	/* initialize the set of allowed tags */` |
|     17 | 5902 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5903 | `	if( nTaglen > 0 ){` |
|      - | 5904 | `		/* Set of allowed tags */` |
|     11 | 5905 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5906 | `	}` |
|      - | 5907 | `	/* Set the empty string */` |
|     17 | 5908 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5909 | `	/* Start processing */` |
|     26 | 5910 | `	for(;;){` |
|     53 | 5911 | `		if(zIn >= zEnd){` |
|      - | 5912 | `			/* No more input to process */` |
|     15 | 5913 | `			break;` |
|      - | 5914 | `		}` |
|     39 | 5915 | `		zPtr = zIn;` |
|      - | 5916 | `		/* Find a tag */` |
|    133 | 5917 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5918 | `			zIn++;` |
|      1 | 5919 | `		}` |
|     39 | 5920 | `		if( zIn > zPtr ){` |
|      - | 5921 | `			/* Consume raw input */` |
|     21 | 5922 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5923 | `		}` |
|      - | 5924 | `		/* Ignore trailing null bytes */` |
|     39 | 5925 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5926 | `			zIn++;` |
|    ! 0 | 5927 | `		}` |
|     39 | 5928 | `		if(zIn >= zEnd){` |
|      - | 5929 | `			/* No more input to process */` |
|      3 | 5930 | `			break;` |
|      - | 5931 | `		}` |
|     37 | 5932 | `		if( zIn[0] == '<' ){` |
|      - | 5933 | `			sxi32 rc;` |
|     37 | 5934 | `			zTag = zIn++;` |
|      - | 5935 | `			/* Delimit the tag */` |
|    127 | 5936 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5937 | `				zIn++;` |
|      1 | 5938 | `			}` |
|     37 | 5939 | `			if( zIn < zEnd ){` |
|     37 | 5940 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5941 | `			}` |
|      - | 5942 | `			/* Query the set */` |
|     37 | 5943 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5944 | `			if( rc == SXRET_OK ){` |
|      - | 5945 | `				/* Keep the tag */` |
|     21 | 5946 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5947 | `			}` |
|     18 | 5948 | `		}` |
|      1 | 5949 | `	}` |
|      - | 5950 | `	/* Cleanup */` |
|     17 | 5951 | `	SySetRelease(&sSet);` |
|     17 | 5952 | `	return SXRET_OK;` |
|      1 | 5953 | `}` |
|      - | 5954 | `/*` |
|      - | 5955 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5956 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5957 | ` * Parameters` |
|      - | 5958 | ` *  $str` |
|      - | 5959 | ` *  The input string.` |
|      - | 5960 | ` * $allowable_tags` |
|      - | 5961 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5962 | ` * Return` |
|      - | 5963 | ` *  Returns the stripped string.` |
|      - | 5964 | ` */` |
|     14 | 5965 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5966 | `{` |
|     15 | 5967 | `	const char *zTaglist = 0;` |
|      - | 5968 | `	const char *zString;` |
|     15 | 5969 | `	int nTaglen = 0;` |
|      - | 5970 | `	int nLen;` |
|     15 | 5971 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5972 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 5973 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5974 | `		return PH7_OK;` |
|      - | 5975 | `	}` |
|      - | 5976 | `	/* Point to the raw string */` |
|     15 | 5977 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5978 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5979 | `		/* Allowed tag */` |
|     11 | 5980 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5981 | `	}` |
|      - | 5982 | `	/* Process input */` |
|     15 | 5983 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5984 | `	return PH7_OK;` |
|      8 | 5985 | `}` |
|      - | 5986 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5987 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5988 | `/*` |
|      - | 5989 | ` * string str_shuffle(string $str)` |
|      - | 5990 |  |
|      - | 5991 | ` *  Randomly shuffles a string.` |
|      - | 5992 | ` * Parameters` |
|      - | 5993 | ` *  $str` |
|      - | 5994 | ` *   The input string.` |
|      - | 5995 | ` * Return` |
|      - | 5996 | ` *  Returns the shuffled string.` |
|      - | 5997 | ` */` |
|     10 | 5998 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5999 | `{` |
|      - | 6000 | `	const char *zString;` |
|      - | 6001 | `	int nLen,i,c;` |
|      - | 6002 | `	sxu32 iR;` |
|     11 | 6003 | `	if( nArg < 1 ){` |
|      - | 6004 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6005 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6006 | `		return PH7_OK;` |
|      - | 6007 | `	}` |
|      - | 6008 | `	/* Extract the target string */` |
|     11 | 6009 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 6010 | `	if( nLen < 1 ){` |
|      - | 6011 | `		/* Nothing to shuffle */` |
|      3 | 6012 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6013 | `		return PH7_OK;` |
|      - | 6014 | `	}` |
|      - | 6015 | `	/* Shuffle the string */` |
|     43 | 6016 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 6017 | `		/* Generate a random number first */` |
|     35 | 6018 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 6019 | `		/* Extract a random offset */` |
|     35 | 6020 | `		c = zString[iR % nLen];` |
|      - | 6021 | `		/* Append it */` |
|     35 | 6022 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 6023 | `	}` |
|      9 | 6024 | `	return PH7_OK;` |
|      6 | 6025 | `}` |
|      - | 6026 | `/*` |
|      - | 6027 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 6028 | ` *  Convert a string to an array.` |
|      - | 6029 | ` * Parameters` |
|      - | 6030 | ` * $string` |
|      - | 6031 | ` *  The input string.` |
|      - | 6032 | ` * $split_length` |
|      - | 6033 | ` *  Maximum length of the chunk.` |
|      - | 6034 | ` * Return` |
|      - | 6035 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 6036 | ` *  except possibly the last one which may be shorter.` |
|      - | 6037 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 6038 | ` *  as the first (and only) array element.` |
|      - | 6039 | ` *  An empty string returns an empty array.` |
|      - | 6040 | ` * Errors` |
|      - | 6041 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 6042 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 6043 | ` *  ValueError if $split_length is less than 1.` |
|      - | 6044 | ` */` |
|     28 | 6045 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6046 | `{` |
|      - | 6047 | `	const char *zString,*zEnd;` |
|      - | 6048 | `	ph7_value *pArray,*pValue;` |
|      - | 6049 | `	int split_len;` |
|      - | 6050 | `	int nLen;` |
|     33 | 6051 | `	if( nArg < 1 ){` |
|      4 | 6052 | `		return PH7_VmThrowException(pCtx,` |
|      - | 6053 | `			"ArgumentCountError",` |
|      - | 6054 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 6055 | `			nArg` |
|      - | 6056 | `			);` |
|      - | 6057 | `	}` |
|      - | 6058 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 6059 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 6060 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 6061 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 6062 | `		return PH7_VmThrowException(pCtx,` |
|      - | 6063 | `			"TypeError",` |
|      - | 6064 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 6065 | `			ph7_type_name(apArg[0])` |
|      - | 6066 | `			);` |
|      - | 6067 | `	}` |
|      - | 6068 | `	/* Point to the target string */` |
|     27 | 6069 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 6070 | `	split_len = (int)sizeof(char);` |
|     27 | 6071 | `	if( nArg > 1 ){` |
|      - | 6072 | `		/* Split length */` |
|     17 | 6073 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 6074 | `		if( split_len < 1 ){` |
|      6 | 6075 | `			return PH7_VmThrowException(pCtx,` |
|      - | 6076 | `				"ValueError",` |
|      - | 6077 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 6078 | `				);` |
|      - | 6079 | `		}` |
|     11 | 6080 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 6081 | `			split_len = nLen;` |
|      1 | 6082 | `		}` |
|      5 | 6083 | `	}` |
|      - | 6084 | `	/* Create the array and the scalar value */` |
|     21 | 6085 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 6086 | `	/*Chunk value */` |
|     21 | 6087 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 6088 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 6089 | `		/* Return FALSE */` |
|    ! 0 | 6090 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6091 | `		return PH7_OK;` |
|      - | 6092 | `	}` |
|      - | 6093 | `	/* Point to the end of the string */` |
|     21 | 6094 | `	zEnd = &zString[nLen];` |
|      - | 6095 | `	/* Perform the requested operation */` |
|     48 | 6096 | `	for(;;){` |
|      - | 6097 | `		int nMax;` |
|     59 | 6098 | `		if( zString >= zEnd ){` |
|      - | 6099 | `			/* No more input to process */` |
|     21 | 6100 | `			break;` |
|      - | 6101 | `		}` |
|     39 | 6102 | `		nMax = (int)(zEnd-zString);` |
|     39 | 6103 | `		if( nMax < split_len ){` |
|      3 | 6104 | `			split_len = nMax;` |
|      1 | 6105 | `		}` |
|      - | 6106 | `		/* Copy the current chunk */` |
|     39 | 6107 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 6108 | `		/* Insert it */` |
|     39 | 6109 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 6110 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6111 | `		}` |
|      - | 6112 | `		/* reset the string cursor */` |
|     39 | 6113 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 6114 | `		/* Update position */` |
|     39 | 6115 | `		zString += split_len;` |
|      1 | 6116 | `	}` |
|      - | 6117 | `	/*` |
|      - | 6118 | `	 * Return the array.` |
|      - | 6119 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 6120 | `	 * upon we return from this function.` |
|      - | 6121 | `	 */` |
|     21 | 6122 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 6123 | `	return PH7_OK;` |
|     19 | 6124 | `}` |
|      - | 6125 | `/*` |
|      - | 6126 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 6127 | ` * Refer to [strspn()].` |
|      - | 6128 | ` */` |
|     28 | 6129 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 6130 | `{` |
|     29 | 6131 | `	const char *zIn = *pzIn;` |
|      - | 6132 | `	const char *zPtr;` |
|      - | 6133 | `	/* Ignore leading white spaces */` |
|     29 | 6134 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 6135 | `		zIn++;` |
|    ! 0 | 6136 | `	}` |
|     29 | 6137 | `	if( zIn >= zEnd ){` |
|      - | 6138 | `		/* End of input */` |
|    ! 0 | 6139 | `		return SXERR_EOF;` |
|      - | 6140 | `	}` |
|     29 | 6141 | `	zPtr = zIn;` |
|      - | 6142 | `	/* Extract the token */` |
|    201 | 6143 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 6144 | `		zIn++;` |
|      1 | 6145 | `	}` |
|     29 | 6146 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6147 | `	/* Synchronize pointers */` |
|     29 | 6148 | `	*pzIn = zIn;` |
|      - | 6149 | `	/* Return to the caller */` |
|     29 | 6150 | `	return SXRET_OK;` |
|     15 | 6151 | `}` |
|      - | 6152 | `/*` |
|      - | 6153 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 6154 | ` * return the longest match.` |
|      - | 6155 | ` * Refer to [strspn()].` |
|      - | 6156 | ` */` |
|     18 | 6157 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6158 | `{` |
|     19 | 6159 | `	const char *zEnd = &zString[nLen];` |
|     19 | 6160 | `	const char *zIn = zString;` |
|      - | 6161 | `	int i,c;` |
|     45 | 6162 | `	for(;;){` |
|     91 | 6163 | `		if( zString >= zEnd ){` |
|      7 | 6164 | `			break;` |
|      - | 6165 | `		}` |
|      - | 6166 | `		/* Extract current character */` |
|     85 | 6167 | `		c = zString[0];` |
|      - | 6168 | `		/* Perform the lookup */` |
|    383 | 6169 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 6170 | `			if( c == zMask[i] ){` |
|      - | 6171 | `				/* Character found */` |
|     73 | 6172 | `				break;` |
|      - | 6173 | `			}` |
|    150 | 6174 | `		}` |
|     85 | 6175 | `		if( i >= nMaskLen ){` |
|      - | 6176 | `			/* Character not in the current mask,break immediately */` |
|     13 | 6177 | `			break;` |
|      - | 6178 | `		}` |
|      - | 6179 | `		/* Advance cursor */` |
|     73 | 6180 | `		zString++;` |
|      1 | 6181 | `	}` |
|      - | 6182 | `	/* Longest match */` |
|     19 | 6183 | `	return (int)(zString-zIn);` |
|      1 | 6184 | `}` |
|      - | 6185 | `/*` |
|      - | 6186 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 6187 | ` * Refer to [strcspn()].` |
|      - | 6188 | ` */` |
|     10 | 6189 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6190 | `{` |
|     11 | 6191 | `	const char *zEnd = &zString[nLen];` |
|     11 | 6192 | `	const char *zIn = zString;` |
|      - | 6193 | `	int i,c;` |
|     12 | 6194 | `	for(;;){` |
|     25 | 6195 | `		if( zString >= zEnd ){` |
|      3 | 6196 | `			break;` |
|      - | 6197 | `		}` |
|      - | 6198 | `		/* Extract current character */` |
|     23 | 6199 | `		c = zString[0];` |
|      - | 6200 | `		/* Perform the lookup */` |
|     51 | 6201 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 6202 | `			if( c == zMask[i] ){` |
|      9 | 6203 | `				break;` |
|      - | 6204 | `			}` |
|     15 | 6205 | `		}` |
|     23 | 6206 | `		if( i < nMaskLen ){` |
|      - | 6207 | `			/* Character in the current mask,break immediately */` |
|      9 | 6208 | `			break;` |
|      - | 6209 | `		}` |
|      - | 6210 | `		/* Advance cursor */` |
|     15 | 6211 | `		zString++;` |
|      1 | 6212 | `	}` |
|      - | 6213 | `	/* Longest match */` |
|     11 | 6214 | `	return (int)(zString-zIn);` |
|      1 | 6215 | `}` |
|      - | 6216 | `/*` |
|      - | 6217 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6218 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 6219 | ` *  of characters contained within a given mask.` |
|      - | 6220 | ` * Parameters` |
|      - | 6221 | ` * $str` |
|      - | 6222 | ` *  The input string.` |
|      - | 6223 | ` * $mask` |
|      - | 6224 | ` *  The list of allowable characters.` |
|      - | 6225 | ` * $start` |
|      - | 6226 | ` *  The position in subject to start searching.` |
|      - | 6227 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6228 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6229 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6230 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6231 | ` *  start'th position from the end of subject.` |
|      - | 6232 | ` * $length` |
|      - | 6233 | ` *  The length of the segment from subject to examine.` |
|      - | 6234 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6235 | ` *  characters after the starting position.` |
|      - | 6236 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6237 | ` *  position up to length characters from the end of subject.` |
|      - | 6238 | ` * Return` |
|      - | 6239 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 6240 | ` * in mask.` |
|      - | 6241 | ` */` |
|     24 | 6242 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6243 | `{` |
|      - | 6244 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6245 | `	int iMasklen,iLen;` |
|      - | 6246 | `	SyString sToken;` |
|     25 | 6247 | `	int iCount = 0;` |
|      - | 6248 | `	int rc;` |
|     25 | 6249 | `	if( nArg < 2 ){` |
|      - | 6250 | `		/* Missing agruments,return zero */` |
|    ! 0 | 6251 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6252 | `		return PH7_OK;` |
|      - | 6253 | `	}` |
|      - | 6254 | `	/* Extract the target string */` |
|     25 | 6255 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6256 | `	/* Extract the mask */` |
|     25 | 6257 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 6258 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 6259 | `		/* Nothing to process,return zero */` |
|      7 | 6260 | `		ph7_result_int(pCtx,0);` |
|      7 | 6261 | `		return PH7_OK;` |
|      - | 6262 | `	}` |
|     19 | 6263 | `	if( nArg > 2 ){` |
|      - | 6264 | `		int nOfft;` |
|      - | 6265 | `		/* Extract the offset */` |
|      9 | 6266 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 6267 | `		if( nOfft < 0 ){` |
|    ! 0 | 6268 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6269 | `			if( zBase > zString ){` |
|    ! 0 | 6270 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6271 | `				zString = zBase;` |
|    ! 0 | 6272 | `			}else{` |
|      - | 6273 | `				/* Invalid offset */` |
|    ! 0 | 6274 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6275 | `				return PH7_OK;` |
|      - | 6276 | `			}` |
|    ! 0 | 6277 | `		}else{` |
|      9 | 6278 | `			if( nOfft >= iLen ){` |
|      - | 6279 | `				/* Invalid offset */` |
|    ! 0 | 6280 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6281 | `				return PH7_OK;` |
|    ! 0 | 6282 | `			}else{` |
|      - | 6283 | `				/* Update offset */` |
|      9 | 6284 | `				zString += nOfft;` |
|      9 | 6285 | `				iLen -= nOfft;` |
|      - | 6286 | `			}` |
|      - | 6287 | `		}` |
|      9 | 6288 | `		if( nArg > 3 ){` |
|      - | 6289 | `			int iUserlen;` |
|      - | 6290 | `			/* Extract the desired length */` |
|      9 | 6291 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 6292 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 6293 | `				iLen = iUserlen;` |
|      2 | 6294 | `			}` |
|      4 | 6295 | `		}` |
|      4 | 6296 | `	}` |
|      - | 6297 | `	/* Point to the end of the string */` |
|     19 | 6298 | `	zEnd = &zString[iLen];` |
|      - | 6299 | `	/* Extract the first non-space token */` |
|     19 | 6300 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 6301 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6302 | `		/* Compare against the current mask */` |
|     19 | 6303 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 6304 | `	}` |
|      - | 6305 | `	/* Longest match */` |
|     19 | 6306 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 6307 | `	return PH7_OK;` |
|     13 | 6308 | `}` |
|      - | 6309 | `/*` |
|      - | 6310 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6311 | ` *  Find length of initial segment not matching mask.` |
|      - | 6312 | ` * Parameters` |
|      - | 6313 | ` * $str` |
|      - | 6314 | ` *  The input string.` |
|      - | 6315 | ` * $mask` |
|      - | 6316 | ` *  The list of not allowed characters.` |
|      - | 6317 | ` * $start` |
|      - | 6318 | ` *  The position in subject to start searching.` |
|      - | 6319 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6320 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6321 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6322 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6323 | ` *  start'th position from the end of subject.` |
|      - | 6324 | ` * $length` |
|      - | 6325 | ` *  The length of the segment from subject to examine.` |
|      - | 6326 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6327 | ` *  characters after the starting position.` |
|      - | 6328 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6329 | ` *  position up to length characters from the end of subject.` |
|      - | 6330 | ` * Return` |
|      - | 6331 | ` *  Returns the length of the segment as an integer.` |
|      - | 6332 | ` */` |
|     14 | 6333 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6334 | `{` |
|      - | 6335 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6336 | `	int iMasklen,iLen;` |
|      - | 6337 | `	SyString sToken;` |
|     15 | 6338 | `	int iCount = 0;` |
|      - | 6339 | `	int rc;` |
|     15 | 6340 | `	if( nArg < 2 ){` |
|      - | 6341 | `		/* Missing agruments,return zero */` |
|    ! 0 | 6342 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6343 | `		return PH7_OK;` |
|      - | 6344 | `	}` |
|      - | 6345 | `	/* Extract the target string */` |
|     15 | 6346 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6347 | `	/* Extract the mask */` |
|     15 | 6348 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 6349 | `	if( iLen < 1 ){` |
|      - | 6350 | `		/* Nothing to process,return zero */` |
|    ! 0 | 6351 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6352 | `		return PH7_OK;` |
|      - | 6353 | `	}` |
|     15 | 6354 | `	if( iMasklen < 1 ){` |
|      - | 6355 | `		/* No given mask,return the string length */` |
|      3 | 6356 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 6357 | `		return PH7_OK;` |
|      - | 6358 | `	}` |
|     13 | 6359 | `	if( nArg > 2 ){` |
|      - | 6360 | `		int nOfft;` |
|      - | 6361 | `		/* Extract the offset */` |
|     11 | 6362 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 6363 | `		if( nOfft < 0 ){` |
|    ! 0 | 6364 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6365 | `			if( zBase > zString ){` |
|    ! 0 | 6366 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6367 | `				zString = zBase;` |
|    ! 0 | 6368 | `			}else{` |
|      - | 6369 | `				/* Invalid offset */` |
|    ! 0 | 6370 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6371 | `				return PH7_OK;` |
|      - | 6372 | `			}` |
|    ! 0 | 6373 | `		}else{` |
|     11 | 6374 | `			if( nOfft >= iLen ){` |
|      - | 6375 | `				/* Invalid offset */` |
|      3 | 6376 | `				ph7_result_int(pCtx,0);` |
|      3 | 6377 | `				return PH7_OK;` |
|    ! 0 | 6378 | `			}else{` |
|      - | 6379 | `				/* Update offset */` |
|      9 | 6380 | `				zString += nOfft;` |
|      9 | 6381 | `				iLen -= nOfft;` |
|      - | 6382 | `			}` |
|      - | 6383 | `		}` |
|      9 | 6384 | `		if( nArg > 3 ){` |
|      - | 6385 | `			int iUserlen;` |
|      - | 6386 | `			/* Extract the desired length */` |
|    ! 0 | 6387 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 6388 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 6389 | `				iLen = iUserlen;` |
|    ! 0 | 6390 | `			}` |
|    ! 0 | 6391 | `		}` |
|      4 | 6392 | `	}` |
|      - | 6393 | `	/* Point to the end of the string */` |
|     11 | 6394 | `	zEnd = &zString[iLen];` |
|      - | 6395 | `	/* Extract the first non-space token */` |
|     11 | 6396 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 6397 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6398 | `		/* Compare against the current mask */` |
|     11 | 6399 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 6400 | `	}` |
|      - | 6401 | `	/* Longest match */` |
|     11 | 6402 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 6403 | `	return PH7_OK;` |
|      8 | 6404 | `}` |
|      - | 6405 | `/*` |
|      - | 6406 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 6407 | ` *  Search a string for any of a set of characters.` |
|      - | 6408 | ` * Parameters` |
|      - | 6409 | ` *  $haystack` |
|      - | 6410 | ` *   The string where char_list is looked for.` |
|      - | 6411 | ` *  $char_list` |
|      - | 6412 | ` *   This parameter is case sensitive.` |
|      - | 6413 | ` * Return` |
|      - | 6414 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 6415 | ` */` |
|      4 | 6416 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6417 | `{` |
|      - | 6418 | `	const char *zString,*zList,*zEnd;` |
|      - | 6419 | `	int iLen,iListLen,i,c;` |
|      - | 6420 | `	sxu32 nOfft,nMax;` |
|      - | 6421 | `	sxi32 rc;` |
|      5 | 6422 | `	if( nArg < 2 ){` |
|      - | 6423 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 6424 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6425 | `		return PH7_OK;` |
|      - | 6426 | `	}` |
|      - | 6427 | `	/* Extract the haystack and the char list */` |
|      5 | 6428 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 6429 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 6430 | `	if( iLen < 1 ){` |
|      - | 6431 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6432 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6433 | `		return PH7_OK;` |
|      - | 6434 | `	}` |
|      - | 6435 | `	/* Point to the end of the string */` |
|      5 | 6436 | `	zEnd = &zString[iLen];` |
|      5 | 6437 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6438 | `	/* perform the requested operation */` |
|     15 | 6439 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6440 | `		c = zList[i];` |
|     11 | 6441 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6442 | `		if( rc == SXRET_OK ){` |
|      5 | 6443 | `			if( nMax < nOfft ){` |
|      3 | 6444 | `				nOfft = nMax;` |
|      1 | 6445 | `			}` |
|      2 | 6446 | `		}` |
|      6 | 6447 | `	}` |
|      5 | 6448 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6449 | `		/* No such substring,return FALSE */` |
|      3 | 6450 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6451 | `	}else{` |
|      - | 6452 | `		/* Return the substring */` |
|      3 | 6453 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6454 | `	}` |
|      5 | 6455 | `	return PH7_OK;` |
|      3 | 6456 | `}` |
|      - | 6457 | `/* SPDX-SnippetBegin */` |
|      - | 6458 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6459 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6460 | `/*` |
|      - | 6461 | ` * string soundex(string $str)` |
|      - | 6462 | ` *  Calculate the soundex key of a string.` |
|      - | 6463 | ` * Parameters` |
|      - | 6464 | ` *  $str` |
|      - | 6465 | ` *   The input string.` |
|      - | 6466 | ` * Return` |
|      - | 6467 | ` *  Returns the soundex key as a string.` |
|      - | 6468 | ` * Note:` |
|      - | 6469 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6470 | ` * source tree.` |
|      - | 6471 | ` */` |
|     22 | 6472 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6473 | `{` |
|      - | 6474 | `	const unsigned char *zIn;` |
|      - | 6475 | `	char zResult[8];` |
|      - | 6476 | `	int i, j;` |
|      - | 6477 | `	static const unsigned char iCode[] = {` |
|      - | 6478 |  |
|      - | 6479 |  |
|      - | 6480 |  |
|      - | 6481 |  |
|      - | 6482 |  |
|      - | 6483 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6484 |  |
|      - | 6485 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6486 | `	};` |
|     23 | 6487 | `	if( nArg < 1 ){` |
|      - | 6488 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6489 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6490 | `		return PH7_OK;` |
|      - | 6491 | `	}` |
|     23 | 6492 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 6493 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 6494 | `	if( zIn[i] ){` |
|     17 | 6495 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6496 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6497 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6498 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6499 | `			if( code>0 ){` |
|     45 | 6500 | `				if( code!=prevcode ){` |
|     33 | 6501 | `					prevcode = (unsigned char)code;` |
|     33 | 6502 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6503 | `				}` |
|     23 | 6504 | `			}else{` |
|     49 | 6505 | `				prevcode = 0;` |
|      - | 6506 | `			}` |
|     47 | 6507 | `		}` |
|     33 | 6508 | `		while( j<4 ){` |
|     17 | 6509 | `			zResult[j++] = '0';` |
|      1 | 6510 | `		}` |
|     17 | 6511 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6512 | `	}else{` |
|      - | 6513 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 6514 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 6515 | `	}` |
|     23 | 6516 | `	return PH7_OK;` |
|     12 | 6517 | `}` |
|      - | 6518 | `/* SPDX-SnippetEnd */` |
|      - | 6519 | `/*` |
|      - | 6520 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6521 | ` *  Wraps a string to a given number of characters.` |
|      - | 6522 | ` * Parameters` |
|      - | 6523 | ` *  $str` |
|      - | 6524 | ` *   The input string.` |
|      - | 6525 | ` * $width` |
|      - | 6526 | ` *  The column width.` |
|      - | 6527 | ` * $break` |
|      - | 6528 | ` *  The line is broken using the optional break parameter.` |
|      - | 6529 | ` * Return` |
|      - | 6530 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6531 | ` */` |
|     12 | 6532 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6533 | `{` |
|      - | 6534 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 6535 | `	int iLen,iBreaklen,iChunk;` |
|     13 | 6536 | `	if( nArg < 1 ){` |
|      - | 6537 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6538 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6539 | `		return PH7_OK;` |
|      - | 6540 | `	}` |
|      - | 6541 | `	/* Extract the input string */` |
|     13 | 6542 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 6543 | `	if( iLen < 1 ){` |
|      - | 6544 | `		/* Nothing to process,return the empty string */` |
|      3 | 6545 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6546 | `		return PH7_OK;` |
|      - | 6547 | `	}` |
|      - | 6548 | `	/* Chunk length */` |
|     11 | 6549 | `	iChunk = 75;` |
|     11 | 6550 | `	iBreaklen = 0;` |
|     11 | 6551 | `	zBreak = ""; /* cc warning */` |
|     11 | 6552 | `	if( nArg > 1 ){` |
|     11 | 6553 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 6554 | `		if( iChunk < 1 ){` |
|    ! 0 | 6555 | `			iChunk = 75;` |
|    ! 0 | 6556 | `		}` |
|     11 | 6557 | `		if( nArg > 2 ){` |
|      3 | 6558 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 6559 | `		}` |
|      5 | 6560 | `	}` |
|     11 | 6561 | `	if( iBreaklen < 1 ){` |
|      - | 6562 | `		/* Set a default column break */` |
|      - | 6563 | `#ifdef __WINNT__` |
|      1 | 6564 | `		zBreak = "\r\n";` |
|      1 | 6565 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 6566 | `#else` |
|      8 | 6567 | `		zBreak = "\n";` |
|      8 | 6568 | `		iBreaklen = (int)sizeof(char);` |
|      - | 6569 | `#endif` |
|      4 | 6570 | `	}` |
|      - | 6571 | `	/* Perform the requested operation */` |
|     11 | 6572 | `	zEnd = &zIn[iLen];` |
|     41 | 6573 | `	for(;;){` |
|      - | 6574 | `		int nMax;` |
|     47 | 6575 | `		if( zIn >= zEnd ){` |
|      - | 6576 | `			/* No more input to process */` |
|     11 | 6577 | `			break;` |
|      - | 6578 | `		}` |
|     37 | 6579 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 6580 | `		if( iChunk > nMax ){` |
|     11 | 6581 | `			iChunk = nMax;` |
|      5 | 6582 | `		}` |
|      - | 6583 | `		/* Append the column first */` |
|     37 | 6584 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 6585 | `		/* Advance the cursor */` |
|     37 | 6586 | `		zIn += iChunk;` |
|     37 | 6587 | `		if( zIn < zEnd ){` |
|      - | 6588 | `			/* Append the line break */` |
|     27 | 6589 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 6590 | `		}` |
|      1 | 6591 | `	}` |
|     11 | 6592 | `	return PH7_OK;` |
|      7 | 6593 | `}` |
|      - | 6594 | `/*` |
|      - | 6595 | ` * Check if the given character is a member of the given mask.` |
|      - | 6596 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6597 | ` * Refer to [strtok()].` |
|      - | 6598 | ` */` |
|     30 | 6599 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6600 | `{` |
|      - | 6601 | `	int i;` |
|     57 | 6602 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6603 | `		if( c == zMask[i] ){` |
|     13 | 6604 | `			if( pOfft ){` |
|      5 | 6605 | `				*pOfft = i;` |
|      2 | 6606 | `			}` |
|     13 | 6607 | `			return TRUE;` |
|      - | 6608 | `		}` |
|     14 | 6609 | `	}` |
|     19 | 6610 | `	return FALSE;` |
|     16 | 6611 | `}` |
|      - | 6612 | `/*` |
|      - | 6613 | ` * Extract a single token from the input stream.` |
|      - | 6614 | ` * Refer to [strtok()].` |
|      - | 6615 | ` */` |
|      6 | 6616 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6617 | `{` |
|      7 | 6618 | `	const char *zIn = *pzIn;` |
|      - | 6619 | `	const char *zPtr;` |
|      - | 6620 | `	/* Ignore leading delimiter */` |
|     11 | 6621 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6622 | `		zIn++;` |
|      1 | 6623 | `	}` |
|      7 | 6624 | `	if( zIn >= zEnd ){` |
|      - | 6625 | `		/* End of input */` |
|    ! 0 | 6626 | `		return SXERR_EOF;` |
|      - | 6627 | `	}` |
|      7 | 6628 | `	zPtr = zIn;` |
|      - | 6629 | `	/* Extract the token */` |
|     13 | 6630 | `	while( zIn < zEnd ){` |
|     11 | 6631 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6632 | `			/* UTF-8 stream */` |
|    ! 0 | 6633 | `			zIn++;` |
|    ! 0 | 6634 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6635 | `		}else{` |
|     11 | 6636 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6637 | `				break;` |
|      - | 6638 | `			}` |
|      7 | 6639 | `			zIn++;` |
|      - | 6640 | `		}` |
|      1 | 6641 | `	}` |
|      7 | 6642 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6643 | `	/* Update the cursor */` |
|      7 | 6644 | `	*pzIn = zIn;` |
|      - | 6645 | `	/* Return to the caller */` |
|      7 | 6646 | `	return SXRET_OK;` |
|      4 | 6647 | `}` |
|      - | 6648 | `/* strtok auxiliary private data */` |
|      - | 6649 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6650 | `struct strtok_aux_data` |
|      - | 6651 | `{` |
|      - | 6652 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6653 | `	const char *zIn;   /* Current input stream */` |
|      - | 6654 | `	const char *zEnd;  /* End of input */` |
|      - | 6655 | `};` |
|      - | 6656 | `/*` |
|      - | 6657 | ` * string strtok(string $str,string $token)` |
|      - | 6658 | ` * string strtok(string $token)` |
|      - | 6659 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6660 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6661 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6662 | ` *  words by using the space character as the token.` |
|      - | 6663 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6664 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6665 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6666 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6667 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6668 | ` *  the argument are found.` |
|      - | 6669 | ` * Parameters` |
|      - | 6670 | ` *  $str` |
|      - | 6671 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6672 | ` * $token` |
|      - | 6673 | ` *  The delimiter used when splitting up str.` |
|      - | 6674 | ` * Return` |
|      - | 6675 | ` *   Current token or FALSE on EOF.` |
|      - | 6676 | ` */` |
|      6 | 6677 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6678 | `{` |
|      - | 6679 | `	strtok_aux_data *pAux;` |
|      - | 6680 | `	const char *zMask;` |
|      - | 6681 | `	SyString sToken;` |
|      - | 6682 | `	int nMasklen;` |
|      - | 6683 | `	sxi32 rc;` |
|      7 | 6684 | `	if( nArg < 2 ){` |
|      - | 6685 | `		/* Extract top aux data */` |
|      5 | 6686 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 6687 | `		if( pAux == 0 ){` |
|      - | 6688 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6689 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6690 | `			return PH7_OK;` |
|      - | 6691 | `		}` |
|      5 | 6692 | `		nMasklen = 0;` |
|      5 | 6693 | `		zMask = ""; /* cc warning */` |
|      5 | 6694 | `		if( nArg > 0 ){` |
|      - | 6695 | `			/* Extract the mask */` |
|      5 | 6696 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6697 | `		}` |
|      5 | 6698 | `		if( nMasklen < 1 ){` |
|      - | 6699 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 6700 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6701 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6702 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6703 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6704 | `			return PH7_OK;` |
|      - | 6705 | `		}` |
|      - | 6706 | `		/* Extract the token */` |
|      5 | 6707 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6708 | `		if( rc != SXRET_OK ){` |
|      - | 6709 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6710 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6711 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6712 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6713 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6714 | `		}else{` |
|      - | 6715 | `			/* Return the extracted token */` |
|      5 | 6716 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6717 | `		}` |
|      3 | 6718 | `	}else{` |
|      - | 6719 | `		const char *zInput,*zCur;` |
|      - | 6720 | `		char *zDup;` |
|      - | 6721 | `		int nLen;` |
|      - | 6722 | `		/* Extract the raw input */` |
|      3 | 6723 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6724 | `		if( nLen < 1 ){` |
|      - | 6725 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6726 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6727 | `			return PH7_OK;` |
|      - | 6728 | `		}` |
|      - | 6729 | `		/* Extract the mask */` |
|      3 | 6730 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6731 | `		if( nMasklen < 1 ){` |
|      - | 6732 | `			/* Set a default mask */` |
|      - | 6733 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6734 | `			zMask = TOK_MASK;` |
|    ! 0 | 6735 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6736 | `#undef TOK_MASK` |
|    ! 0 | 6737 | `		}` |
|      - | 6738 | `		/* Extract a single token */` |
|      3 | 6739 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6740 | `		if( rc != SXRET_OK ){` |
|      - | 6741 | `			/* Empty input */` |
|    ! 0 | 6742 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6743 | `			return PH7_OK;` |
|    ! 0 | 6744 | `		}else{` |
|      - | 6745 | `			/* Return the extracted token */` |
|      3 | 6746 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6747 | `		}` |
|      - | 6748 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6749 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6750 | `		if( pAux ){` |
|      3 | 6751 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6752 | `			if( nLen < 1 ){` |
|    ! 0 | 6753 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6754 | `				return PH7_OK;` |
|      - | 6755 | `			}` |
|      - | 6756 | `			/* Duplicate input */` |
|      3 | 6757 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6758 | `			if( zDup  ){` |
|      3 | 6759 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6760 | `				/* Register the aux data */` |
|      3 | 6761 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6762 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6763 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6764 | `			}` |
|      1 | 6765 | `		}` |
|      - | 6766 | `	}` |
|      7 | 6767 | `	return PH7_OK;` |
|      4 | 6768 | `}` |
|      - | 6769 | `/*` |
|      - | 6770 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6771 | ` *  Pad a string to a certain length with another string` |
|      - | 6772 | ` * Parameters` |
|      - | 6773 | ` *  $input` |
|      - | 6774 | ` *   The input string.` |
|      - | 6775 | ` * $pad_length` |
|      - | 6776 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6777 | ` *   string, no padding takes place.` |
|      - | 6778 | ` * $pad_string` |
|      - | 6779 | ` *   Note:` |
|      - | 6780 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6781 | ` *    divided by the pad_string's length.` |
|      - | 6782 | ` * $pad_type` |
|      - | 6783 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6784 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6785 | ` * Return` |
|      - | 6786 | ` *  The padded string.` |
|      - | 6787 | ` */` |
|     10 | 6788 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6789 | `{` |
|      - | 6790 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6791 | `	const char *zIn,*zPad;` |
|     11 | 6792 | `	if( nArg < 2 ){` |
|      - | 6793 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6794 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6795 | `		return PH7_OK;` |
|      - | 6796 | `	}` |
|      - | 6797 | `	/* Extract the target string */` |
|     11 | 6798 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6799 | `	/* Padding length */` |
|     11 | 6800 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|     11 | 6801 | `	if( iPadlen > 0 ){` |
|      9 | 6802 | `		iPadlen -= iLen;` |
|      4 | 6803 | `	}` |
|     11 | 6804 | `	if( iPadlen < 1  ){` |
|      - | 6805 | `		/* Return the string verbatim */` |
|      5 | 6806 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 6807 | `		return PH7_OK;` |
|      - | 6808 | `	}` |
|      7 | 6809 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 6810 | `	iStrpad = (int)sizeof(char);` |
|      7 | 6811 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 6812 | `	if( nArg > 2 ){` |
|      - | 6813 | `		/* Padding string */` |
|      7 | 6814 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 6815 | `		if( iStrpad < 1 ){` |
|      - | 6816 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 6817 | `			 * (only reached once padding is actually required). */` |
|      3 | 6818 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6819 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 6820 | `		}` |
|      5 | 6821 | `		if( nArg > 3 ){` |
|      - | 6822 | `			/* Padd type */` |
|      5 | 6823 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6824 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6825 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6826 | `			}` |
|      2 | 6827 | `		}` |
|      2 | 6828 | `	}` |
|      5 | 6829 | `	iDiv = 1;` |
|      5 | 6830 | `	if( iType == 2 ){` |
|    ! 0 | 6831 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6832 | `	}` |
|      - | 6833 | `	/* Perform the requested operation */` |
|      5 | 6834 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6835 | `		jPad = iStrpad;` |
|      5 | 6836 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6837 | `			/* Padding */` |
|      5 | 6838 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6839 | `				break;` |
|      - | 6840 | `			}` |
|      3 | 6841 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6842 | `		}` |
|      3 | 6843 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6844 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6845 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6846 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6847 | `					jPad = iStrpad;` |
|    ! 0 | 6848 | `				}` |
|      3 | 6849 | `				if( jPad < 1){` |
|    ! 0 | 6850 | `					break;` |
|      - | 6851 | `				}` |
|      3 | 6852 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6853 | `			}` |
|      1 | 6854 | `		}` |
|      1 | 6855 | `	}` |
|      5 | 6856 | `	if( iLen > 0 ){` |
|      - | 6857 | `		/* Append the input string */` |
|      5 | 6858 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6859 | `	}` |
|      5 | 6860 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6861 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6862 | `			/* Padding */` |
|      5 | 6863 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6864 | `				break;` |
|      - | 6865 | `			}` |
|      3 | 6866 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6867 | `		}` |
|      5 | 6868 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6869 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6870 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6871 | `				jPad = iStrpad;` |
|    ! 0 | 6872 | `			}` |
|      3 | 6873 | `			if( jPad < 1){` |
|    ! 0 | 6874 | `				break;` |
|      - | 6875 | `			}` |
|      3 | 6876 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6877 | `		}` |
|      1 | 6878 | `	}` |
|      5 | 6879 | `	return PH7_OK;` |
|      6 | 6880 | `}` |
|      - | 6881 | `/*` |
|      - | 6882 | ` * String replacement private data.` |
|      - | 6883 | ` */` |
|      - | 6884 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6885 | `struct str_replace_data` |
|      - | 6886 | `{` |
|      - | 6887 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 6888 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6889 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6890 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 6891 | `};` |
|      - | 6892 | `/*` |
|      - | 6893 | ` * Remove a substring.` |
|      - | 6894 | ` */` |
|      - | 6895 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6896 | `	for(;;){\` |
|      - | 6897 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6898 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6899 | `		++OFFT;\` |
|      - | 6900 | `	}\` |
|      - | 6901 | `}` |
|      - | 6902 | `/*` |
|      - | 6903 | ` * Shift right and insert algorithm.` |
|      - | 6904 | ` */` |
|      - | 6905 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6906 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6907 | `		for(;;){\` |
|      - | 6908 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6909 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6910 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6911 | `			--INLEN; \` |
|      - | 6912 | `		}\` |
|      - | 6913 | `		for(;;){\` |
|      - | 6914 | `				if(ELEN < 1) { break; }\` |
|      - | 6915 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6916 | `				OFFT++;\` |
|      - | 6917 | `				ENTRY++;\` |
|      - | 6918 | `				--ELEN;\` |
|      - | 6919 | `		}\` |
|      - | 6920 | `}` |
|      - | 6921 | `/*` |
|      - | 6922 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6923 | ` * replacement string [i.e: zReplace].` |
|      - | 6924 | ` */` |
|     32 | 6925 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6926 | `{` |
|     33 | 6927 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6928 | `	sxu32 n,m;` |
|     33 | 6929 | `	n = SyBlobLength(pWorker);` |
|     33 | 6930 | `	m = nOfft;` |
|      - | 6931 | `	/* Delete the old entry */` |
|    429 | 6932 | `	STRDEL(zInput,n,m,nLen);` |
|     33 | 6933 | `	SyBlobLength(pWorker) -= nLen;` |
|     33 | 6934 | `	if( nReplen > 0 ){` |
|     27 | 6935 | `		sxi32 iRep = nReplen;` |
|      - | 6936 | `		sxi32 rc;` |
|      - | 6937 | `		/*` |
|      - | 6938 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6939 | `		 * string.` |
|      - | 6940 | `		 */` |
|     27 | 6941 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     27 | 6942 | `		if( rc != SXRET_OK ){` |
|      - | 6943 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 6944 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 6945 | `			return rc;` |
|      - | 6946 | `		}` |
|      - | 6947 | `		/* Perform the insertion now */` |
|     27 | 6948 | `		zInput = (char *)SyBlobData(pWorker);` |
|     27 | 6949 | `		n = SyBlobLength(pWorker);` |
|    129 | 6950 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     27 | 6951 | `		SyBlobLength(pWorker) += nReplen;` |
|     13 | 6952 | `	}` |
|     33 | 6953 | `	return SXRET_OK;` |
|     17 | 6954 | `}` |
|      - | 6955 | `/*` |
|      - | 6956 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6957 | ` * to collect search/replace string.` |
|      - | 6958 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6959 | ` */` |
|     26 | 6960 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6961 | `{` |
|     27 | 6962 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6963 | `	SyString sWorker;` |
|      - | 6964 | `	const char *zIn;` |
|      - | 6965 | `	int nByte;` |
|      - | 6966 | `	/* Extract a string representation of the given argument */` |
|     27 | 6967 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6968 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6969 | `	if( nByte > 0 ){` |
|      - | 6970 | `		char *zDup;` |
|      - | 6971 | `		/* Duplicate the chunk */` |
|     25 | 6972 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6973 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6974 | `			);` |
|     25 | 6975 | `		if( zDup == 0 ){` |
|      - | 6976 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 6977 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 6978 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 6979 | `			return SXERR_MEM;` |
|      - | 6980 | `		}` |
|     25 | 6981 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6982 | `		/* Save the chunk */` |
|     25 | 6983 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6984 | `	}` |
|      - | 6985 | `	/* Save for later processing */` |
|     27 | 6986 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6987 | `	/* All done */` |
|     13 | 6988 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6989 | `	return PH7_OK;` |
|     14 | 6990 | `}` |
|      - | 6991 | `/*` |
|      - | 6992 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6993 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6994 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6995 | ` * Parameters` |
|      - | 6996 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6997 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6998 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6999 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 7000 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 7001 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 7002 | ` * $search` |
|      - | 7003 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 7004 | ` *  to designate multiple needles.` |
|      - | 7005 | ` * $replace` |
|      - | 7006 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 7007 | ` *  to designate multiple replacements.` |
|      - | 7008 | ` * $subject` |
|      - | 7009 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 7010 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 7011 | ` *  of subject, and the return value is an array as well.` |
|      - | 7012 | ` * $count (Not used)` |
|      - | 7013 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 7014 | ` * Return` |
|      - | 7015 | ` * This function returns a string or an array with the replaced values.` |
|      - | 7016 | ` */` |
|  28500 | 7017 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7018 | `{` |
|      - | 7019 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 7020 | `	ProcStringMatch xMatch;` |
|      - | 7021 | `	const char *zIn,*zFunc;` |
|      - | 7022 | `	str_replace_data sRep;` |
|      - | 7023 | `	SyBlob sWorker;` |
|      - | 7024 | `	SySet sReplace;` |
|      - | 7025 | `	SySet sSearch;` |
|      - | 7026 | `	int rep_str;` |
|      - | 7027 | `	int nByte;` |
|      - | 7028 | `	sxi32 rc;` |
|  28505 | 7029 | `	if( nArg < 3 ){` |
|      - | 7030 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 7031 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7032 | `		return PH7_OK;` |
|      - | 7033 | `	}` |
|      - | 7034 | `	/* Initialize fields */` |
|  28505 | 7035 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  28505 | 7036 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  28505 | 7037 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  28505 | 7038 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  28505 | 7039 | `	sRep.pCtx = pCtx;` |
|  28505 | 7040 | `	sRep.pCollector = &sSearch;` |
|  28505 | 7041 | `	rep_str = 0;` |
|      - | 7042 | `	/* Extract the subject */` |
|  28505 | 7043 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  28505 | 7044 | `	if( nByte < 1 ){` |
|      - | 7045 | `		/* Nothing to replace,return the empty string */` |
|     29 | 7046 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 7047 | `		return PH7_OK;` |
|      - | 7048 | `	}` |
|      - | 7049 | `	/* Copy the subject */` |
|  28477 | 7050 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 7051 | `	/* Search string */` |
|  28477 | 7052 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 7053 | `		/* Collect search string */` |
|      9 | 7054 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 7055 | `	}else{` |
|      - | 7056 | `		/* Single pattern */` |
|  28469 | 7057 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  28469 | 7058 | `		if( nByte < 1 ){` |
|      - | 7059 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 7060 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 7061 | `			return PH7_OK;` |
|      - | 7062 | `		}` |
|  28465 | 7063 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7064 | `		/* Save for later processing */` |
|  28465 | 7065 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 7066 | `	}` |
|      - | 7067 | `	/* Replace string */` |
|  28473 | 7068 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 7069 | `		/* Collect replace string */` |
|      7 | 7070 | `		sRep.pCollector = &sReplace;` |
|      7 | 7071 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 7072 | `	}else{` |
|      - | 7073 | `		/* Single needle */` |
|  28467 | 7074 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  28467 | 7075 | `		rep_str = 1;` |
|  28467 | 7076 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7077 | `		/* Save for later processing */` |
|  28467 | 7078 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 7079 | `	}` |
|      - | 7080 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  28473 | 7081 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 7082 | `		SySetRelease(&sSearch);` |
|    ! 0 | 7083 | `		SySetRelease(&sReplace);` |
|    ! 0 | 7084 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 7085 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7086 | `	}` |
|      - | 7087 | `	/* Reset loop cursors */` |
|  28473 | 7088 | `	SySetResetCursor(&sSearch);` |
|  28473 | 7089 | `	SySetResetCursor(&sReplace);` |
|  28473 | 7090 | `	pReplace = pSearch = 0; /* cc warning */` |
|  28473 | 7091 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 7092 | `	/* Extract function name */` |
|  28473 | 7093 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 7094 | `	/* Set the default pattern match routine */` |
|  28473 | 7095 | `	xMatch = SyBlobSearch;` |
|  28473 | 7096 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 7097 | `		/* Case insensitive pattern match */` |
|     11 | 7098 | `		xMatch = iPatternMatch;` |
|      5 | 7099 | `	}` |
|      - | 7100 | `	/* Start the replace process */` |
|  56949 | 7101 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 7102 | `		sxu32 nCount,nOfft;` |
|  28481 | 7103 | `		if( pSearch->nByte <  1 ){` |
|      - | 7104 | `			/* Empty string,ignore */` |
|      3 | 7105 | `			continue;` |
|      - | 7106 | `		}` |
|      - | 7107 | `		/* Extract the replace string */` |
|  28479 | 7108 | `		if( rep_str ){` |
|  28469 | 7109 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14237 | 7110 | `		}else{` |
|     11 | 7111 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 7112 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 7113 | `				 * An empty string is used for the rest of replacement values` |
|      - | 7114 | `				 */` |
|      3 | 7115 | `				pReplace = 0;` |
|      1 | 7116 | `			}` |
|      - | 7117 | `		}` |
|  28479 | 7118 | `		if( pReplace == 0 ){` |
|      - | 7119 | `			/* Use an empty string instead */` |
|      3 | 7120 | `			pReplace = &sTemp;` |
|      1 | 7121 | `		}` |
|  28479 | 7122 | `		nOfft = nCount = 0;` |
|  14253 | 7123 | `		for(;;){` |
|  28511 | 7124 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 7125 | `				break;` |
|      - | 7126 | `			}` |
|      - | 7127 | `			/* Perform a pattern lookup */` |
|  42746 | 7128 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  28494 | 7129 | `				pSearch->nByte,&nOfft);` |
|  28499 | 7130 | `			if( rc != SXRET_OK ){` |
|      - | 7131 | `				/* Pattern not found */` |
|  28467 | 7132 | `				break;` |
|      - | 7133 | `			}` |
|      - | 7134 | `			/* Perform the replace operation */` |
|     33 | 7135 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 7136 | `			if( rc != SXRET_OK ){` |
|      - | 7137 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 7138 | `				SySetRelease(&sSearch);` |
|    ! 0 | 7139 | `				SySetRelease(&sReplace);` |
|    ! 0 | 7140 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7141 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7142 | `			}` |
|      - | 7143 | `			/* Increment offset counter */` |
|     33 | 7144 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 7145 | `		}` |
|      5 | 7146 | `	}` |
|      - | 7147 | `	/* All done,clean-up the mess left behind */` |
|  28473 | 7148 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  28473 | 7149 | `	SySetRelease(&sSearch);` |
|  28473 | 7150 | `	SySetRelease(&sReplace);` |
|  28473 | 7151 | `	SyBlobRelease(&sWorker);` |
|  28473 | 7152 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7153 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7154 | `	}` |
|  28473 | 7155 | `	return PH7_OK;` |
|  14255 | 7156 | `}` |
|      - | 7157 | `/*` |
|      - | 7158 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 7159 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 7160 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 7161 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 7162 | ` */` |
|      - | 7163 | `typedef struct strtr_entry strtr_entry;` |
|      - | 7164 | `struct strtr_entry` |
|      - | 7165 | `{` |
|      - | 7166 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 7167 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 7168 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 7169 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 7170 | `};` |
|      - | 7171 | `typedef struct strtr_collect strtr_collect;` |
|      - | 7172 | `struct strtr_collect` |
|      - | 7173 | `{` |
|      - | 7174 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 7175 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 7176 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 7177 | `};` |
|      - | 7178 | `/*` |
|      - | 7179 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 7180 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 7181 | ` * decimal form) and ignores an empty-string key.` |
|      - | 7182 | ` */` |
|     20 | 7183 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 7184 | `{` |
|     21 | 7185 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 7186 | `	const char *zKey,*zVal;` |
|      - | 7187 | `	strtr_entry sEnt;` |
|      - | 7188 | `	int nKey,nVal;` |
|     21 | 7189 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 7190 | `	if( nKey < 1 ){` |
|      - | 7191 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 7192 | `		return PH7_OK;` |
|      - | 7193 | `	}` |
|     21 | 7194 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 7195 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 7196 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 7197 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 7198 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7199 | `		return SXERR_ABORT;` |
|      - | 7200 | `	}` |
|     21 | 7201 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 7202 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 7203 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 7204 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7205 | `		return SXERR_ABORT;` |
|      - | 7206 | `	}` |
|     21 | 7207 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 7208 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7209 | `		return SXERR_ABORT;` |
|      - | 7210 | `	}` |
|     21 | 7211 | `	return PH7_OK;` |
|     11 | 7212 | `}` |
|      - | 7213 | `/*` |
|      - | 7214 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 7215 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 7216 | ` *  Translate characters or replace substrings.` |
|      - | 7217 | ` * Parameters` |
|      - | 7218 | ` *  $str` |
|      - | 7219 | ` *  The string being translated.` |
|      - | 7220 | ` * $from` |
|      - | 7221 | ` *  The string being translated to to.` |
|      - | 7222 | ` * $to` |
|      - | 7223 | ` *  The string replacing from.` |
|      - | 7224 | ` * $replace_pairs` |
|      - | 7225 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 7226 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 7227 | ` * Return` |
|      - | 7228 | ` *  The translated string.` |
|      - | 7229 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 7230 | ` */` |
|     12 | 7231 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7232 | `{` |
|      - | 7233 | `	const char *zIn;` |
|      - | 7234 | `	int nLen;` |
|     13 | 7235 | `	if( nArg < 1 ){` |
|      - | 7236 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 7237 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7238 | `		return PH7_OK;` |
|      - | 7239 | `	}` |
|     13 | 7240 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 7241 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 7242 | `		/* Invalid arguments */` |
|    ! 0 | 7243 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7244 | `		return PH7_OK;` |
|      - | 7245 | `	}` |
|     18 | 7246 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 7247 | `		strtr_collect sCol;` |
|      - | 7248 | `		SyBlob sPool,sWorker;` |
|      - | 7249 | `		SySet sTable;` |
|      - | 7250 | `		const char *zPool;` |
|      - | 7251 | `		strtr_entry *pEnt;` |
|      - | 7252 | `		sxi32 rc;` |
|      - | 7253 | `		int i;` |
|      - | 7254 | `		/*` |
|      - | 7255 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 7256 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 7257 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 7258 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 7259 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 7260 | `		 */` |
|     11 | 7261 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 7262 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 7263 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 7264 | `		sCol.pPool  = &sPool;` |
|     11 | 7265 | `		sCol.pTable = &sTable;` |
|     11 | 7266 | `		sCol.rc     = SXRET_OK;` |
|     11 | 7267 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 7268 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 7269 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 7270 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 7271 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 7272 | `			SySetRelease(&sTable);` |
|    ! 0 | 7273 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7274 | `		}` |
|      - | 7275 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 7276 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 7277 | `		rc = SXRET_OK;` |
|     43 | 7278 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 7279 | `			strtr_entry *pBest = 0;` |
|     33 | 7280 | `			sxu32 nBest = 0;` |
|      - | 7281 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 7282 | `			SySetResetCursor(&sTable);` |
|     97 | 7283 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 7284 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 7285 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 7286 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 7287 | `					nBest = pEnt->nKeyLen;` |
|     29 | 7288 | `					pBest = pEnt;` |
|     14 | 7289 | `				}` |
|      1 | 7290 | `			}` |
|     33 | 7291 | `			if( pBest ){` |
|     25 | 7292 | `				if( pBest->nValLen > 0 ){` |
|     25 | 7293 | `					rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 7294 | `				}` |
|     25 | 7295 | `				i += (int)pBest->nKeyLen;` |
|     13 | 7296 | `			}else{` |
|      9 | 7297 | `				rc = SyBlobAppend(&sWorker,(const void *)&zIn[i],(sxu32)sizeof(char));` |
|      9 | 7298 | `				i++;` |
|      - | 7299 | `			}` |
|     33 | 7300 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 7301 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 7302 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7303 | `				SySetRelease(&sTable);` |
|    ! 0 | 7304 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7305 | `			}` |
|      1 | 7306 | `		}` |
|      - | 7307 | `		/* All done, return the result string */` |
|     16 | 7308 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 7309 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 7310 | `		/* Clean-up */` |
|     11 | 7311 | `		SyBlobRelease(&sPool);` |
|     11 | 7312 | `		SyBlobRelease(&sWorker);` |
|     11 | 7313 | `		SySetRelease(&sTable);` |
|     11 | 7314 | `		if( rc != PH7_OK ){` |
|    ! 0 | 7315 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7316 | `		}` |
|      6 | 7317 | `	}else{` |
|      - | 7318 | `		int i,flen,tlen,c,iOfft;` |
|      - | 7319 | `		const char *zFrom,*zTo;` |
|      3 | 7320 | `		if( nArg < 3 ){` |
|      - | 7321 | `			/* Nothing to replace */` |
|    ! 0 | 7322 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7323 | `			return PH7_OK;` |
|      - | 7324 | `		}` |
|      - | 7325 | `		/* Extract given arguments */` |
|      3 | 7326 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 7327 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 7328 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 7329 | `			/* Nothing to replace */` |
|    ! 0 | 7330 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7331 | `			return PH7_OK;` |
|      - | 7332 | `		}` |
|      - | 7333 | `		/* Start the replace process */` |
|     13 | 7334 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 7335 | `			c = zIn[i];` |
|     11 | 7336 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 7337 | `				if ( iOfft < tlen ){` |
|      5 | 7338 | `					c = zTo[iOfft];` |
|      2 | 7339 | `				}` |
|      2 | 7340 | `			}` |
|     11 | 7341 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 7342 |  |
|      6 | 7343 | `		}` |
|      - | 7344 | `	}` |
|     13 | 7345 | `	return PH7_OK;` |
|      7 | 7346 | `}` |
|      - | 7347 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7348 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7349 | `/*` |
|      - | 7350 | ` * Parse an INI string.` |
|      - | 7351 |  |
|      - | 7352 | ` * According to wikipedia` |
|      - | 7353 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 7354 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 7355 | ` *  Format` |
|      - | 7356 | `*    Properties` |
|      - | 7357 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 7358 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 7359 | `*     Example:` |
|      - | 7360 | `*      name=value` |
|      - | 7361 | `*    Sections` |
|      - | 7362 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 7363 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 7364 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 7365 | `*     or the end of the file. Sections may not be nested.` |
|      - | 7366 | `*     Example:` |
|      - | 7367 | `*      [section]` |
|      - | 7368 | `*   Comments` |
|      - | 7369 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 7370 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 7371 | `*/` |
|     12 | 7372 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 7373 | `{` |
|      - | 7374 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 7375 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 7376 | `	SyHashEntry *pEntry;` |
|      - | 7377 | `	SyString sEntry;` |
|      - | 7378 | `	SyHash sHash;` |
|      - | 7379 | `	int c;` |
|      - | 7380 | `	/* Create an empty array and worker variables */` |
|     13 | 7381 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 7382 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 7383 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 7384 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 7385 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 7386 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7387 | `	}` |
|     13 | 7388 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 7389 | `	pCur = pArray;` |
|      - | 7390 | `	/* Start the parse process */` |
|     21 | 7391 | `	for(;;){` |
|      - | 7392 | `		/* Ignore leading white spaces */` |
|     69 | 7393 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 7394 | `			zIn++;` |
|      1 | 7395 | `		}` |
|     43 | 7396 | `		if( zIn >= zEnd ){` |
|      - | 7397 | `			/* No more input to process */` |
|     13 | 7398 | `			break;` |
|      - | 7399 | `		}` |
|     31 | 7400 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7401 | `			/* Comment til the end of line */` |
|    ! 0 | 7402 | `			zIn++;` |
|    ! 0 | 7403 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 7404 | `				zIn++;` |
|    ! 0 | 7405 | `			}` |
|    ! 0 | 7406 | `			continue;` |
|      - | 7407 | `		}` |
|      - | 7408 | `		/* Reset the string cursor of the working variable */` |
|     31 | 7409 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 7410 | `		if( zIn[0] == '[' ){` |
|      - | 7411 | `			/* Section: Extract the section name */` |
|      9 | 7412 | `			zIn++;` |
|      9 | 7413 | `			zCur = zIn;` |
|     73 | 7414 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 7415 | `				zIn++;` |
|      1 | 7416 | `			}` |
|      9 | 7417 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 7418 | `				/* Save the section name */` |
|      5 | 7419 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 7420 | `				SyStringFullTrim(&sEntry);` |
|      5 | 7421 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 7422 | `				if( sEntry.nByte > 0 ){` |
|      - | 7423 | `					/* Associate an array with the section */` |
|      5 | 7424 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 7425 | `					if( pSection ){` |
|      5 | 7426 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 7427 | `						pCur = pSection;` |
|      2 | 7428 | `					}` |
|      2 | 7429 | `				}` |
|      2 | 7430 | `			}` |
|      9 | 7431 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 7432 | `		}else{` |
|      - | 7433 | `			ph7_value *pOldCur;` |
|      - | 7434 | `			int is_array;` |
|      - | 7435 | `			int iLen;` |
|      - | 7436 | `			/* Properties */` |
|     23 | 7437 | `			is_array = 0;` |
|     23 | 7438 | `			zCur = zIn;` |
|     23 | 7439 | `			iLen = 0; /* cc warning */` |
|     23 | 7440 | `			pOldCur = pCur;` |
|    155 | 7441 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 7442 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 7443 | `					/* Array */` |
|    ! 0 | 7444 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 7445 | `					is_array = 1;` |
|    ! 0 | 7446 | `					if( iLen > 0 ){` |
|    ! 0 | 7447 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 7448 | `						/* Query the hashtable */` |
|    ! 0 | 7449 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 7450 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 7451 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 7452 | `						if( pEntry ){` |
|    ! 0 | 7453 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 7454 | `						}else{` |
|      - | 7455 | `							/* Create an empty array */` |
|    ! 0 | 7456 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 7457 | `							if( pvArr ){` |
|      - | 7458 | `								/* Save the entry */` |
|    ! 0 | 7459 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 7460 | `								/* Insert the entry */` |
|    ! 0 | 7461 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7462 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 7463 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 7464 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7465 | `							}` |
|      - | 7466 | `						}` |
|    ! 0 | 7467 | `						if( pvArr ){` |
|    ! 0 | 7468 | `							pCur = pvArr;` |
|    ! 0 | 7469 | `						}` |
|    ! 0 | 7470 | `					}` |
|    ! 0 | 7471 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 7472 | `						zIn++;` |
|    ! 0 | 7473 | `					}` |
|    ! 0 | 7474 | `				}` |
|    133 | 7475 | `				zIn++;` |
|      1 | 7476 | `			}` |
|     23 | 7477 | `			if( !is_array ){` |
|     23 | 7478 | `				iLen = (int)(zIn-zCur);` |
|     11 | 7479 | `			}` |
|      - | 7480 | `			/* Trim the key */` |
|     23 | 7481 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 7482 | `			SyStringFullTrim(&sEntry);` |
|     23 | 7483 | `			if( sEntry.nByte > 0 ){` |
|     23 | 7484 | `				if( !is_array ){` |
|      - | 7485 | `					/* Save the key name */` |
|     23 | 7486 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 7487 | `				}` |
|      - | 7488 | `				/* extract key value */` |
|     23 | 7489 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7490 | `				zIn++; /* '=' */` |
|     39 | 7491 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7492 | `					zIn++;` |
|      1 | 7493 | `				}` |
|     23 | 7494 | `				if( zIn < zEnd ){` |
|     21 | 7495 | `					zCur = zIn;` |
|     21 | 7496 | `					c = zIn[0];` |
|     21 | 7497 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7498 | `						zIn++;` |
|      - | 7499 | `						/* Delimit the value */` |
|    ! 0 | 7500 | `						while( zIn < zEnd ){` |
|    ! 0 | 7501 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7502 | `								break;` |
|      - | 7503 | `							}` |
|    ! 0 | 7504 | `							zIn++;` |
|    ! 0 | 7505 | `						}` |
|    ! 0 | 7506 | `						if( zIn < zEnd ){` |
|    ! 0 | 7507 | `							zIn++;` |
|    ! 0 | 7508 | `						}` |
|    ! 0 | 7509 | `					}else{` |
|    125 | 7510 | `						while( zIn < zEnd ){` |
|    123 | 7511 | `							if( zIn[0] == '\n' ){` |
|     19 | 7512 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7513 | `									break;` |
|    ! 0 | 7514 | `								}` |
|    105 | 7515 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7516 | `								/* Inline comments */` |
|    ! 0 | 7517 | `								break;` |
|      - | 7518 | `							}` |
|    105 | 7519 | `							zIn++;` |
|      1 | 7520 | `						}` |
|      - | 7521 | `					}` |
|      - | 7522 | `					/* Trim the value */` |
|     21 | 7523 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7524 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7525 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7526 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7527 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7528 | `					}` |
|     21 | 7529 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7530 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7531 | `					}` |
|      - | 7532 | `					/* Insert the key and it's value */` |
|     21 | 7533 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7534 | `				}` |
|     12 | 7535 | `			}else{` |
|    ! 0 | 7536 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7537 | `					zIn++;` |
|    ! 0 | 7538 | `				}` |
|      - | 7539 | `			}` |
|     23 | 7540 | `			pCur = pOldCur;` |
|      - | 7541 | `		}` |
|      1 | 7542 | `	}` |
|     13 | 7543 | `	SyHashRelease(&sHash);` |
|      - | 7544 | `	/* Return the parse of the INI string */` |
|     13 | 7545 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7546 | `	return SXRET_OK;` |
|      7 | 7547 | `}` |
|      - | 7548 | `/*` |
|      - | 7549 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7550 | ` *  Parse a configuration string.` |
|      - | 7551 | ` * Parameters` |
|      - | 7552 | ` *  $ini` |
|      - | 7553 | ` *   The contents of the ini file being parsed.` |
|      - | 7554 | ` *  $process_sections` |
|      - | 7555 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7556 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7557 | ` *  $scanner_mode (Not used)` |
|      - | 7558 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7559 | ` *   then option values will not be parsed.` |
|      - | 7560 | ` * Return` |
|      - | 7561 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7562 | ` */` |
|     10 | 7563 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7564 | `{` |
|      - | 7565 | `	const char *zIni;` |
|      - | 7566 | `	int nByte;` |
|     11 | 7567 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7568 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7569 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7570 | `		return PH7_OK;` |
|      - | 7571 | `	}` |
|      - | 7572 | `	/* Extract the raw INI buffer */` |
|     11 | 7573 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7574 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7575 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7576 | `}` |
|      - | 7577 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7578 |  |
|      - | 7579 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7580 |  |
|      - | 7581 | `/*` |
|      - | 7582 | ` * Ctype Functions.` |
|      - | 7583 | ` * Status:` |
|      - | 7584 | ` *    Stable.` |
|      - | 7585 | ` */` |
|      - | 7586 | `/*` |
|      - | 7587 | ` * bool ctype_alnum(string $text)` |
|      - | 7588 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7589 | ` * Parameters` |
|      - | 7590 | ` *  $text` |
|      - | 7591 | ` *   The tested string.` |
|      - | 7592 | ` * Return` |
|      - | 7593 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7594 | ` */` |
|     14 | 7595 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7596 | `{` |
|      - | 7597 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7598 | `	int nLen;` |
|     15 | 7599 | `	if( nArg < 1 ){` |
|      - | 7600 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7601 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7602 | `		return PH7_OK;` |
|      - | 7603 | `	}` |
|      - | 7604 | `	/* Extract the target string */` |
|     15 | 7605 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7606 | `	zEnd = &zIn[nLen];` |
|     15 | 7607 | `	if( nLen < 1 ){` |
|      - | 7608 | `		/* Empty string,return FALSE */` |
|      3 | 7609 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7610 | `		return PH7_OK;` |
|      - | 7611 | `	}` |
|      - | 7612 | `	/* Perform the requested operation */` |
|     32 | 7613 | `	for(;;){` |
|     65 | 7614 | `		if( zIn >= zEnd ){` |
|      - | 7615 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7616 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7617 | `			return PH7_OK;` |
|      - | 7618 | `		}` |
|     57 | 7619 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7620 | `			break;` |
|      - | 7621 | `		}` |
|      - | 7622 | `		/* Point to the next character */` |
|     53 | 7623 | `		zIn++;` |
|      1 | 7624 | `	}` |
|      - | 7625 | `	/* The test failed,return FALSE */` |
|      5 | 7626 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7627 | `	return PH7_OK;` |
|      8 | 7628 | `}` |
|      - | 7629 | `/*` |
|      - | 7630 | ` * bool ctype_alpha(string $text)` |
|      - | 7631 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7632 | ` * Parameters` |
|      - | 7633 | ` *  $text` |
|      - | 7634 | ` *   The tested string.` |
|      - | 7635 | ` * Return` |
|      - | 7636 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7637 | ` */` |
|     16 | 7638 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7639 | `{` |
|      - | 7640 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7641 | `	int nLen;` |
|     17 | 7642 | `	if( nArg < 1 ){` |
|      - | 7643 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7644 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7645 | `		return PH7_OK;` |
|      - | 7646 | `	}` |
|      - | 7647 | `	/* Extract the target string */` |
|     17 | 7648 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7649 | `	zEnd = &zIn[nLen];` |
|     17 | 7650 | `	if( nLen < 1 ){` |
|      - | 7651 | `		/* Empty string,return FALSE */` |
|      3 | 7652 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7653 | `		return PH7_OK;` |
|      - | 7654 | `	}` |
|      - | 7655 | `	/* Perform the requested operation */` |
|     42 | 7656 | `	for(;;){` |
|     85 | 7657 | `		if( zIn >= zEnd ){` |
|      - | 7658 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7659 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7660 | `			return PH7_OK;` |
|      - | 7661 | `		}` |
|     77 | 7662 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7663 | `			break;` |
|      - | 7664 | `		}` |
|      - | 7665 | `		/* Point to the next character */` |
|     71 | 7666 | `		zIn++;` |
|      1 | 7667 | `	}` |
|      - | 7668 | `	/* The test failed,return FALSE */` |
|      7 | 7669 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7670 | `	return PH7_OK;` |
|      9 | 7671 | `}` |
|      - | 7672 | `/*` |
|      - | 7673 | ` * bool ctype_cntrl(string $text)` |
|      - | 7674 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7675 | ` * Parameters` |
|      - | 7676 | ` *  $text` |
|      - | 7677 | ` *   The tested string.` |
|      - | 7678 | ` * Return` |
|      - | 7679 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7680 | ` */` |
|     16 | 7681 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7682 | `{` |
|      - | 7683 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7684 | `	int nLen;` |
|     17 | 7685 | `	if( nArg < 1 ){` |
|      - | 7686 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7687 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7688 | `		return PH7_OK;` |
|      - | 7689 | `	}` |
|      - | 7690 | `	/* Extract the target string */` |
|     17 | 7691 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7692 | `	zEnd = &zIn[nLen];` |
|     17 | 7693 | `	if( nLen < 1 ){` |
|      - | 7694 | `		/* Empty string,return FALSE */` |
|      3 | 7695 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7696 | `		return PH7_OK;` |
|      - | 7697 | `	}` |
|      - | 7698 | `	/* Perform the requested operation */` |
|     14 | 7699 | `	for(;;){` |
|     29 | 7700 | `		if( zIn >= zEnd ){` |
|      - | 7701 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7702 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7703 | `			return PH7_OK;` |
|      - | 7704 | `		}` |
|     21 | 7705 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7706 | `			/* UTF-8 stream  */` |
|    ! 0 | 7707 | `			break;` |
|      - | 7708 | `		}` |
|     21 | 7709 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7710 | `			break;` |
|      - | 7711 | `		}` |
|      - | 7712 | `		/* Point to the next character */` |
|     15 | 7713 | `		zIn++;` |
|      1 | 7714 | `	}` |
|      - | 7715 | `	/* The test failed,return FALSE */` |
|      7 | 7716 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7717 | `	return PH7_OK;` |
|      9 | 7718 | `}` |
|      - | 7719 | `/*` |
|      - | 7720 | ` * bool ctype_digit(string $text)` |
|      - | 7721 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7722 | ` * Parameters` |
|      - | 7723 | ` *  $text` |
|      - | 7724 | ` *   The tested string.` |
|      - | 7725 | ` * Return` |
|      - | 7726 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7727 | ` */` |
|   1622 | 7728 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7729 | `{` |
|      - | 7730 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7731 | `	int nLen;` |
|   1627 | 7732 | `	if( nArg < 1 ){` |
|      - | 7733 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7734 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7735 | `		return PH7_OK;` |
|      - | 7736 | `	}` |
|      - | 7737 | `	/* Extract the target string */` |
|   1627 | 7738 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1627 | 7739 | `	zEnd = &zIn[nLen];` |
|   1627 | 7740 | `	if( nLen < 1 ){` |
|      - | 7741 | `		/* Empty string,return FALSE */` |
|      3 | 7742 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7743 | `		return PH7_OK;` |
|      - | 7744 | `	}` |
|      - | 7745 | `	/* Perform the requested operation */` |
|   1523 | 7746 | `	for(;;){` |
|   3051 | 7747 | `		if( zIn >= zEnd ){` |
|      - | 7748 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1381 | 7749 | `			ph7_result_bool(pCtx,1);` |
|   1381 | 7750 | `			return PH7_OK;` |
|      - | 7751 | `		}` |
|   1675 | 7752 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7753 | `			/* UTF-8 stream  */` |
|    ! 0 | 7754 | `			break;` |
|      - | 7755 | `		}` |
|   1675 | 7756 | `		if( !SyisDigit(zIn[0]) ){` |
|    249 | 7757 | `			break;` |
|      - | 7758 | `		}` |
|      - | 7759 | `		/* Point to the next character */` |
|   1431 | 7760 | `		zIn++;` |
|      5 | 7761 | `	}` |
|      - | 7762 | `	/* The test failed,return FALSE */` |
|    249 | 7763 | `	ph7_result_bool(pCtx,0);` |
|    249 | 7764 | `	return PH7_OK;` |
|    816 | 7765 | `}` |
|      - | 7766 | `/*` |
|      - | 7767 | ` * bool ctype_xdigit(string $text)` |
|      - | 7768 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7769 | ` * Parameters` |
|      - | 7770 | ` *  $text` |
|      - | 7771 | ` *   The tested string.` |
|      - | 7772 | ` * Return` |
|      - | 7773 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7774 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7775 | ` */` |
|     18 | 7776 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7777 | `{` |
|      - | 7778 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7779 | `	int nLen;` |
|     19 | 7780 | `	if( nArg < 1 ){` |
|      - | 7781 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7782 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7783 | `		return PH7_OK;` |
|      - | 7784 | `	}` |
|      - | 7785 | `	/* Extract the target string */` |
|     19 | 7786 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7787 | `	zEnd = &zIn[nLen];` |
|     19 | 7788 | `	if( nLen < 1 ){` |
|      - | 7789 | `		/* Empty string,return FALSE */` |
|      3 | 7790 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7791 | `		return PH7_OK;` |
|      - | 7792 | `	}` |
|      - | 7793 | `	/* Perform the requested operation */` |
|     46 | 7794 | `	for(;;){` |
|     93 | 7795 | `		if( zIn >= zEnd ){` |
|      - | 7796 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7797 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7798 | `			return PH7_OK;` |
|      - | 7799 | `		}` |
|     83 | 7800 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7801 | `			/* UTF-8 stream  */` |
|    ! 0 | 7802 | `			break;` |
|      - | 7803 | `		}` |
|     83 | 7804 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7805 | `			break;` |
|      - | 7806 | `		}` |
|      - | 7807 | `		/* Point to the next character */` |
|     77 | 7808 | `		zIn++;` |
|      1 | 7809 | `	}` |
|      - | 7810 | `	/* The test failed,return FALSE */` |
|      7 | 7811 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7812 | `	return PH7_OK;` |
|     10 | 7813 | `}` |
|      - | 7814 | `/*` |
|      - | 7815 | ` * bool ctype_graph(string $text)` |
|      - | 7816 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7817 | ` * Parameters` |
|      - | 7818 | ` *  $text` |
|      - | 7819 | ` *   The tested string.` |
|      - | 7820 | ` * Return` |
|      - | 7821 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7822 | ` * (no white space), FALSE otherwise.` |
|      - | 7823 | ` */` |
|     16 | 7824 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7825 | `{` |
|      - | 7826 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7827 | `	int nLen;` |
|     17 | 7828 | `	if( nArg < 1 ){` |
|      - | 7829 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7830 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7831 | `		return PH7_OK;` |
|      - | 7832 | `	}` |
|      - | 7833 | `	/* Extract the target string */` |
|     17 | 7834 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7835 | `	zEnd = &zIn[nLen];` |
|     17 | 7836 | `	if( nLen < 1 ){` |
|      - | 7837 | `		/* Empty string,return FALSE */` |
|      3 | 7838 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7839 | `		return PH7_OK;` |
|      - | 7840 | `	}` |
|      - | 7841 | `	/* Perform the requested operation */` |
|     57 | 7842 | `	for(;;){` |
|    115 | 7843 | `		if( zIn >= zEnd ){` |
|      - | 7844 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7845 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7846 | `			return PH7_OK;` |
|      - | 7847 | `		}` |
|    107 | 7848 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7849 | `			/* UTF-8 stream  */` |
|    ! 0 | 7850 | `			break;` |
|      - | 7851 | `		}` |
|    107 | 7852 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7853 | `			break;` |
|      - | 7854 | `		}` |
|      - | 7855 | `		/* Point to the next character */` |
|    101 | 7856 | `		zIn++;` |
|      1 | 7857 | `	}` |
|      - | 7858 | `	/* The test failed,return FALSE */` |
|      7 | 7859 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7860 | `	return PH7_OK;` |
|      9 | 7861 | `}` |
|      - | 7862 | `/*` |
|      - | 7863 | ` * bool ctype_print(string $text)` |
|      - | 7864 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7865 | ` * Parameters` |
|      - | 7866 | ` *  $text` |
|      - | 7867 | ` *   The tested string.` |
|      - | 7868 | ` * Return` |
|      - | 7869 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7870 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7871 | ` *  or control function at all.` |
|      - | 7872 | ` */` |
|     16 | 7873 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7874 | `{` |
|      - | 7875 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7876 | `	int nLen;` |
|     17 | 7877 | `	if( nArg < 1 ){` |
|      - | 7878 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7879 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7880 | `		return PH7_OK;` |
|      - | 7881 | `	}` |
|      - | 7882 | `	/* Extract the target string */` |
|     17 | 7883 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7884 | `	zEnd = &zIn[nLen];` |
|     17 | 7885 | `	if( nLen < 1 ){` |
|      - | 7886 | `		/* Empty string,return FALSE */` |
|      3 | 7887 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7888 | `		return PH7_OK;` |
|      - | 7889 | `	}` |
|      - | 7890 | `	/* Perform the requested operation */` |
|     63 | 7891 | `	for(;;){` |
|    127 | 7892 | `		if( zIn >= zEnd ){` |
|      - | 7893 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7894 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7895 | `			return PH7_OK;` |
|      - | 7896 | `		}` |
|    119 | 7897 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7898 | `			/* UTF-8 stream  */` |
|    ! 0 | 7899 | `			break;` |
|      - | 7900 | `		}` |
|    119 | 7901 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7902 | `			break;` |
|      - | 7903 | `		}` |
|      - | 7904 | `		/* Point to the next character */` |
|    113 | 7905 | `		zIn++;` |
|      1 | 7906 | `	}` |
|      - | 7907 | `	/* The test failed,return FALSE */` |
|      7 | 7908 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7909 | `	return PH7_OK;` |
|      9 | 7910 | `}` |
|      - | 7911 | `/*` |
|      - | 7912 | ` * bool ctype_punct(string $text)` |
|      - | 7913 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7914 | ` * Parameters` |
|      - | 7915 | ` *  $text` |
|      - | 7916 | ` *   The tested string.` |
|      - | 7917 | ` * Return` |
|      - | 7918 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7919 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7920 | ` */` |
|     18 | 7921 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7922 | `{` |
|      - | 7923 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7924 | `	int nLen;` |
|     19 | 7925 | `	if( nArg < 1 ){` |
|      - | 7926 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7927 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7928 | `		return PH7_OK;` |
|      - | 7929 | `	}` |
|      - | 7930 | `	/* Extract the target string */` |
|     19 | 7931 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7932 | `	zEnd = &zIn[nLen];` |
|     19 | 7933 | `	if( nLen < 1 ){` |
|      - | 7934 | `		/* Empty string,return FALSE */` |
|      3 | 7935 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7936 | `		return PH7_OK;` |
|      - | 7937 | `	}` |
|      - | 7938 | `	/* Perform the requested operation */` |
|     38 | 7939 | `	for(;;){` |
|     77 | 7940 | `		if( zIn >= zEnd ){` |
|      - | 7941 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7942 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7943 | `			return PH7_OK;` |
|      - | 7944 | `		}` |
|     69 | 7945 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7946 | `			/* UTF-8 stream  */` |
|    ! 0 | 7947 | `			break;` |
|      - | 7948 | `		}` |
|     69 | 7949 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7950 | `			break;` |
|      - | 7951 | `		}` |
|      - | 7952 | `		/* Point to the next character */` |
|     61 | 7953 | `		zIn++;` |
|      1 | 7954 | `	}` |
|      - | 7955 | `	/* The test failed,return FALSE */` |
|      9 | 7956 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7957 | `	return PH7_OK;` |
|     10 | 7958 | `}` |
|      - | 7959 | `/*` |
|      - | 7960 | ` * bool ctype_space(string $text)` |
|      - | 7961 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7962 | ` * Parameters` |
|      - | 7963 | ` *  $text` |
|      - | 7964 | ` *   The tested string.` |
|      - | 7965 | ` * Return` |
|      - | 7966 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7967 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7968 | ` *  and form feed characters.` |
|      - | 7969 | ` */` |
|  62579 | 7970 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7971 | `{` |
|      - | 7972 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7973 | `	int nLen;` |
|  62584 | 7974 | `	if( nArg < 1 ){` |
|      - | 7975 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7976 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7977 | `		return PH7_OK;` |
|      - | 7978 | `	}` |
|      - | 7979 | `	/* Extract the target string */` |
|  62584 | 7980 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  62584 | 7981 | `	zEnd = &zIn[nLen];` |
|  62584 | 7982 | `	if( nLen < 1 ){` |
|      - | 7983 | `		/* Empty string,return FALSE */` |
|      3 | 7984 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7985 | `		return PH7_OK;` |
|      - | 7986 | `	}` |
|      - | 7987 | `	/* Perform the requested operation */` |
|  32400 | 7988 | `	for(;;){` |
|  64720 | 7989 | `		if( zIn >= zEnd ){` |
|      - | 7990 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2119 | 7991 | `			ph7_result_bool(pCtx,1);` |
|   2119 | 7992 | `			return PH7_OK;` |
|      - | 7993 | `		}` |
|  62606 | 7994 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7995 | `			/* UTF-8 stream  */` |
|    ! 0 | 7996 | `			break;` |
|      - | 7997 | `		}` |
|  62606 | 7998 | `		if( !SyisSpace(zIn[0]) ){` |
|  60468 | 7999 | `			break;` |
|      - | 8000 | `		}` |
|      - | 8001 | `		/* Point to the next character */` |
|   2143 | 8002 | `		zIn++;` |
|      5 | 8003 | `	}` |
|      - | 8004 | `	/* The test failed,return FALSE */` |
|  60468 | 8005 | `	ph7_result_bool(pCtx,0);` |
|  60468 | 8006 | `	return PH7_OK;` |
|  31337 | 8007 | `}` |
|      - | 8008 | `/*` |
|      - | 8009 | ` * bool ctype_lower(string $text)` |
|      - | 8010 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 8011 | ` * Parameters` |
|      - | 8012 | ` *  $text` |
|      - | 8013 | ` *   The tested string.` |
|      - | 8014 | ` * Return` |
|      - | 8015 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 8016 | ` */` |
|     16 | 8017 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8018 | `{` |
|      - | 8019 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8020 | `	int nLen;` |
|     17 | 8021 | `	if( nArg < 1 ){` |
|      - | 8022 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8023 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8024 | `		return PH7_OK;` |
|      - | 8025 | `	}` |
|      - | 8026 | `	/* Extract the target string */` |
|     17 | 8027 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8028 | `	zEnd = &zIn[nLen];` |
|     17 | 8029 | `	if( nLen < 1 ){` |
|      - | 8030 | `		/* Empty string,return FALSE */` |
|      3 | 8031 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8032 | `		return PH7_OK;` |
|      - | 8033 | `	}` |
|      - | 8034 | `	/* Perform the requested operation */` |
|     27 | 8035 | `	for(;;){` |
|     55 | 8036 | `		if( zIn >= zEnd ){` |
|      - | 8037 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8038 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8039 | `			return PH7_OK;` |
|      - | 8040 | `		}` |
|     51 | 8041 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 8042 | `			break;` |
|      - | 8043 | `		}` |
|      - | 8044 | `		/* Point to the next character */` |
|     41 | 8045 | `		zIn++;` |
|      1 | 8046 | `	}` |
|      - | 8047 | `	/* The test failed,return FALSE */` |
|     11 | 8048 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8049 | `	return PH7_OK;` |
|      9 | 8050 | `}` |
|      - | 8051 | `/*` |
|      - | 8052 | ` * bool ctype_upper(string $text)` |
|      - | 8053 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 8054 | ` * Parameters` |
|      - | 8055 | ` *  $text` |
|      - | 8056 | ` *   The tested string.` |
|      - | 8057 | ` * Return` |
|      - | 8058 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 8059 | ` */` |
|     16 | 8060 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8061 | `{` |
|      - | 8062 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8063 | `	int nLen;` |
|     17 | 8064 | `	if( nArg < 1 ){` |
|      - | 8065 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8066 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8067 | `		return PH7_OK;` |
|      - | 8068 | `	}` |
|      - | 8069 | `	/* Extract the target string */` |
|     17 | 8070 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8071 | `	zEnd = &zIn[nLen];` |
|     17 | 8072 | `	if( nLen < 1 ){` |
|      - | 8073 | `		/* Empty string,return FALSE */` |
|      3 | 8074 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8075 | `		return PH7_OK;` |
|      - | 8076 | `	}` |
|      - | 8077 | `	/* Perform the requested operation */` |
|     28 | 8078 | `	for(;;){` |
|     57 | 8079 | `		if( zIn >= zEnd ){` |
|      - | 8080 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8081 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8082 | `			return PH7_OK;` |
|      - | 8083 | `		}` |
|     53 | 8084 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 8085 | `			break;` |
|      - | 8086 | `		}` |
|      - | 8087 | `		/* Point to the next character */` |
|     43 | 8088 | `		zIn++;` |
|      1 | 8089 | `	}` |
|      - | 8090 | `	/* The test failed,return FALSE */` |
|     11 | 8091 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8092 | `	return PH7_OK;` |
|      9 | 8093 | `}` |
|      - | 8094 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 8095 | `/*` |
|      - | 8096 | ` * Section:` |
|      - | 8097 | ` *    URL handling Functions.` |
|      - | 8098 | ` * Status:` |
|      - | 8099 | ` *    Stable.` |
|      - | 8100 | ` */` |
|      - | 8101 | `/*` |
|      - | 8102 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8103 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8104 | ` */` |
|   1026 | 8105 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8106 | `{` |
|      - | 8107 | `	/* Store in the call context result buffer */` |
|   1028 | 8108 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8109 | `	return SXRET_OK;` |
|      2 | 8110 | `}` |
|      - | 8111 | `/*` |
|      - | 8112 | ` * string base64_encode(string $data)` |
|      - | 8113 | ` * string convert_uuencode(string $data)` |
|      - | 8114 | ` *  Encodes data with MIME base64` |
|      - | 8115 | ` * Parameter` |
|      - | 8116 | ` *  $data` |
|      - | 8117 | ` *    Data to encode` |
|      - | 8118 | ` * Return` |
|      - | 8119 | ` *  Encoded data or FALSE on failure.` |
|      - | 8120 | ` */` |
|      6 | 8121 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8122 | `{` |
|      - | 8123 | `	const char *zIn;` |
|      - | 8124 | `	int nLen;` |
|      7 | 8125 | `	if( nArg < 1 ){` |
|      - | 8126 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8127 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8128 | `		return PH7_OK;` |
|      - | 8129 | `	}` |
|      - | 8130 | `	/* Extract the input string */` |
|      7 | 8131 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8132 | `	if( nLen < 1 ){` |
|      - | 8133 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8134 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8135 | `		return PH7_OK;` |
|      - | 8136 | `	}` |
|      - | 8137 | `	/* Perform the BASE64 encoding */` |
|      7 | 8138 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8139 | `	return PH7_OK;` |
|      4 | 8140 | `}` |
|      - | 8141 | `/*` |
|      - | 8142 | ` * string base64_decode(string $data)` |
|      - | 8143 | ` * string convert_uudecode(string $data)` |
|      - | 8144 | ` *  Decodes data encoded with MIME base64` |
|      - | 8145 | ` * Parameter` |
|      - | 8146 | ` *  $data` |
|      - | 8147 | ` *    Encoded data.` |
|      - | 8148 | ` * Return` |
|      - | 8149 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8150 | ` */` |
|     34 | 8151 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8152 | `{` |
|      - | 8153 | `	const char *zIn;` |
|      - | 8154 | `	int nLen;` |
|     36 | 8155 | `	if( nArg < 1 ){` |
|      - | 8156 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8157 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8158 | `		return PH7_OK;` |
|      - | 8159 | `	}` |
|      - | 8160 | `	/* Extract the input string */` |
|     36 | 8161 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8162 | `	if( nLen < 1 ){` |
|      - | 8163 | `		/* Nothing to process,return FALSE */` |
|      3 | 8164 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8165 | `		return PH7_OK;` |
|      - | 8166 | `	}` |
|      - | 8167 | `	/* Perform the BASE64 decoding */` |
|     34 | 8168 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8169 | `	return PH7_OK;` |
|     19 | 8170 | `}` |
|      - | 8171 | `/*` |
|      - | 8172 | ` * string urlencode(string $str)` |
|      - | 8173 | ` *  URL encoding` |
|      - | 8174 | ` * Parameter` |
|      - | 8175 | ` *  $data` |
|      - | 8176 | ` *   Input string.` |
|      - | 8177 | ` * Return` |
|      - | 8178 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8179 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8180 | ` *  encoded as plus (+) signs.` |
|      - | 8181 | ` */` |
|      4 | 8182 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8183 | `{` |
|      - | 8184 | `	const char *zIn;` |
|      - | 8185 | `	int nLen;` |
|      5 | 8186 | `	if( nArg < 1 ){` |
|      - | 8187 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8189 | `		return PH7_OK;` |
|      - | 8190 | `	}` |
|      - | 8191 | `	/* Extract the input string */` |
|      5 | 8192 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8193 | `	if( nLen < 1 ){` |
|      - | 8194 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8195 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8196 | `		return PH7_OK;` |
|      - | 8197 | `	}` |
|      - | 8198 | `	/* Perform the URL encoding */` |
|      5 | 8199 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8200 | `	return PH7_OK;` |
|      3 | 8201 | `}` |
|      - | 8202 | `/*` |
|      - | 8203 | ` * string urldecode(string $str)` |
|      - | 8204 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8205 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8206 | ` * Parameter` |
|      - | 8207 | ` *  $data` |
|      - | 8208 | ` *    Input string.` |
|      - | 8209 | ` * Return` |
|      - | 8210 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8211 | ` */` |
|      6 | 8212 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8213 | `{` |
|      - | 8214 | `	const char *zIn;` |
|      - | 8215 | `	int nLen;` |
|      7 | 8216 | `	if( nArg < 1 ){` |
|      - | 8217 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8218 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8219 | `		return PH7_OK;` |
|      - | 8220 | `	}` |
|      - | 8221 | `	/* Extract the input string */` |
|      7 | 8222 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8223 | `	if( nLen < 1 ){` |
|      - | 8224 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8225 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8226 | `		return PH7_OK;` |
|      - | 8227 | `	}` |
|      - | 8228 | `	/* Perform the URL decoding */` |
|      7 | 8229 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8230 | `	return PH7_OK;` |
|      4 | 8231 | `}` |
|      - | 8232 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8233 | `/* Table of the built-in functions */` |
|      - | 8234 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8235 | `	   /* Variable handling functions */` |
|      - | 8236 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8237 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8238 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8239 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8240 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8241 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8242 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8243 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8244 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8245 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8246 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8247 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8248 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8249 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8250 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8251 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8252 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8253 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8254 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 8255 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8256 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8257 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8258 | `	   /* Math functions */` |
|      - | 8259 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8260 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8261 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8262 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8263 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8264 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8265 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8266 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8267 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8268 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8269 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8270 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8271 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8272 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8273 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8274 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8275 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8276 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8277 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8278 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8279 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8280 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8281 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8282 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 8283 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8284 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8285 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8286 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8287 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8288 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8289 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8290 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8291 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8292 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8293 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8294 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8295 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8296 | `	   /* String handling functions */` |
|      - | 8297 |  |
|      - | 8298 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8299 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8300 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8301 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8302 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8303 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8304 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8305 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8306 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8307 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8308 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8309 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8310 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8311 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8312 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8313 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8314 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8315 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8316 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8317 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8318 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8319 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8320 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8321 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8322 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8323 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8324 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8325 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8326 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8327 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8328 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8329 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8330 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8331 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8332 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8333 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8334 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8335 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8336 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8337 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8338 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8339 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8340 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8341 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8342 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8343 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8344 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8345 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8346 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 8347 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 8348 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 8349 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8350 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8351 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8352 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8353 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8354 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8355 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8356 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8357 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8358 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8359 |  |
|      - | 8360 |  |
|      - | 8361 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8362 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8363 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8364 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8365 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 8366 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 8367 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 8368 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 8369 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8370 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 8371 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 8372 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 8373 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 8374 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 8375 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 8376 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8377 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8378 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8379 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8380 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8381 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8382 |  |
|      - | 8383 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8384 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8385 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8386 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8387 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8388 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8389 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8390 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8391 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8392 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8393 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8394 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8395 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8396 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8397 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8398 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8399 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8400 |  |
|      - | 8401 | `	         /* Ctype functions */` |
|      - | 8402 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8403 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8404 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8405 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8406 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8407 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8408 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8409 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8410 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8411 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8412 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8413 | `	         /* Time functions */` |
|      - | 8414 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8415 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8416 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8417 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8418 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8419 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8420 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8421 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8422 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8423 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8424 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8425 | `	        /* URL functions */` |
|      - | 8426 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8427 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8428 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8429 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8430 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8431 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8432 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8433 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8434 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8435 | `};` |
|      - | 8436 | `/*` |
|      - | 8437 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8438 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8439 | ` */` |
|   3458 | 8440 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 8441 | `{` |
|      - | 8442 | `	sxu32 n;` |
| 580949 | 8443 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 577491 | 8444 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 288748 | 8445 | `	}` |
|      - | 8446 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3463 | 8447 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8448 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3463 | 8449 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3463 | 8450 | `}` |
|      - | 8451 |  |
