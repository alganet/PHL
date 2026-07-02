# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3678/4147 lines (88.69%)

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
|     32 |   28 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   29 | `{` |
|     33 |   30 | `	int res = 0; /* Assume false by default */` |
|     33 |   31 | `	if( nArg > 0 ){` |
|     29 |   32 | `		res = ph7_value_is_bool(apArg[0]);` |
|     14 |   33 | `	}` |
|      - |   34 | `	/* Query result */` |
|     33 |   35 | `	ph7_result_bool(pCtx,res);` |
|     33 |   36 | `	return PH7_OK;` |
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
|    210 |   48 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   49 | `{` |
|    211 |   50 | `	int res = 0; /* Assume false by default */` |
|    211 |   51 | `	if( nArg > 0 ){` |
|    209 |   52 | `		res = ph7_value_is_float(apArg[0]);` |
|    104 |   53 | `	}` |
|      - |   54 | `	/* Query result */` |
|    211 |   55 | `	ph7_result_bool(pCtx,res);` |
|    211 |   56 | `	return PH7_OK;` |
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
|    632 |   68 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   69 | `{` |
|    634 |   70 | `	int res = 0; /* Assume false by default */` |
|    634 |   71 | `	if( nArg > 0 ){` |
|      - |   72 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   73 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   74 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    632 |   75 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    315 |   76 | `	}` |
|      - |   77 | `	/* Query result */` |
|    634 |   78 | `	ph7_result_bool(pCtx,res);` |
|    634 |   79 | `	return PH7_OK;` |
|      2 |   80 | `}` |
|      - |   81 | `/*` |
|      - |   82 | ` * bool is_string($var)` |
|      - |   83 | ` *  Finds out whether a variable is a string.` |
|      - |   84 | ` * Parameters` |
|      - |   85 | ` *   $var: The variable being evaluated.` |
|      - |   86 | ` * Return` |
|      - |   87 | ` *  TRUE if var is string. False otherwise.` |
|      - |   88 | ` */` |
|    126 |   89 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   90 | `{` |
|    127 |   91 | `	int res = 0; /* Assume false by default */` |
|    127 |   92 | `	if( nArg > 0 ){` |
|    125 |   93 | `		res = ph7_value_is_string(apArg[0]);` |
|     62 |   94 | `	}` |
|      - |   95 | `	/* Query result */` |
|    127 |   96 | `	ph7_result_bool(pCtx,res);` |
|    127 |   97 | `	return PH7_OK;` |
|      1 |   98 | `}` |
|      - |   99 | `/*` |
|      - |  100 | ` * bool is_null($var)` |
|      - |  101 | ` *  Finds out whether a variable is NULL.` |
|      - |  102 | ` * Parameters` |
|      - |  103 | ` *   $var: The variable being evaluated.` |
|      - |  104 | ` * Return` |
|      - |  105 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  106 | ` */` |
|     92 |  107 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  108 | `{` |
|     96 |  109 | `	int res = 0; /* Assume false by default */` |
|     96 |  110 | `	if( nArg > 0 ){` |
|     94 |  111 | `		res = ph7_value_is_null(apArg[0]);` |
|     45 |  112 | `	}` |
|      - |  113 | `	/* Query result */` |
|     96 |  114 | `	ph7_result_bool(pCtx,res);` |
|     96 |  115 | `	return PH7_OK;` |
|      4 |  116 | `}` |
|      - |  117 | `/*` |
|      - |  118 | ` * bool is_numeric($var)` |
|      - |  119 | ` *  Find out whether a variable is NULL.` |
|      - |  120 | ` * Parameters` |
|      - |  121 | ` *  $var: The variable being evaluated.` |
|      - |  122 | ` * Return` |
|      - |  123 | ` *  True if var is numeric. False otherwise.` |
|      - |  124 | ` */` |
|     38 |  125 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  126 | `{` |
|     43 |  127 | `	int res = 0; /* Assume false by default */` |
|     43 |  128 | `	if( nArg > 0 ){` |
|     41 |  129 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     18 |  130 | `	}` |
|      - |  131 | `	/* Query result */` |
|     43 |  132 | `	ph7_result_bool(pCtx,res);` |
|     43 |  133 | `	return PH7_OK;` |
|      5 |  134 | `}` |
|      - |  135 | `/*` |
|      - |  136 | ` * bool is_scalar($var)` |
|      - |  137 | ` *  Find out whether a variable is a scalar.` |
|      - |  138 | ` * Parameters` |
|      - |  139 | ` *  $var: The variable being evaluated.` |
|      - |  140 | ` * Return` |
|      - |  141 | ` *  True if var is scalar. False otherwise.` |
|      - |  142 | ` */` |
|     14 |  143 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  144 | `{` |
|     15 |  145 | `	int res = 0; /* Assume false by default */` |
|     15 |  146 | `	if( nArg > 0 ){` |
|     13 |  147 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  148 | `	}` |
|      - |  149 | `	/* Query result */` |
|     15 |  150 | `	ph7_result_bool(pCtx,res);` |
|     15 |  151 | `	return PH7_OK;` |
|      1 |  152 | `}` |
|      - |  153 | `/*` |
|      - |  154 | ` * bool is_array($var)` |
|      - |  155 | ` *  Find out whether a variable is an array.` |
|      - |  156 | ` * Parameters` |
|      - |  157 | ` *  $var: The variable being evaluated.` |
|      - |  158 | ` * Return` |
|      - |  159 | ` *  True if var is an array. False otherwise.` |
|      - |  160 | ` */` |
|    242 |  161 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  162 | `{` |
|    247 |  163 | `	int res = 0; /* Assume false by default */` |
|    247 |  164 | `	if( nArg > 0 ){` |
|    245 |  165 | `		res = ph7_value_is_array(apArg[0]);` |
|    120 |  166 | `	}` |
|      - |  167 | `	/* Query result */` |
|    247 |  168 | `	ph7_result_bool(pCtx,res);` |
|    247 |  169 | `	return PH7_OK;` |
|      5 |  170 | `}` |
|      - |  171 | `/*` |
|      - |  172 | ` * bool is_object($var)` |
|      - |  173 | ` *  Find out whether a variable is an object.` |
|      - |  174 | ` * Parameters` |
|      - |  175 | ` *  $var: The variable being evaluated.` |
|      - |  176 | ` * Return` |
|      - |  177 | ` *  True if var is an object. False otherwise.` |
|      - |  178 | ` */` |
|     22 |  179 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  180 | `{` |
|     23 |  181 | `	int res = 0; /* Assume false by default */` |
|     23 |  182 | `	if( nArg > 0 ){` |
|     21 |  183 | `		res = ph7_value_is_object(apArg[0]);` |
|     10 |  184 | `	}` |
|      - |  185 | `	/* Query result */` |
|     23 |  186 | `	ph7_result_bool(pCtx,res);` |
|     23 |  187 | `	return PH7_OK;` |
|      1 |  188 | `}` |
|      - |  189 | `/*` |
|      - |  190 | ` * bool is_resource($var)` |
|      - |  191 | ` *  Find out whether a variable is a resource.` |
|      - |  192 | ` * Parameters` |
|      - |  193 | ` *  $var: The variable being evaluated.` |
|      - |  194 | ` * Return` |
|      - |  195 | ` *  True if a resource. False otherwise.` |
|      - |  196 | ` */` |
|     60 |  197 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  198 | `{` |
|     63 |  199 | `	int res = 0; /* Assume false by default */` |
|     63 |  200 | `	if( nArg > 0 ){` |
|     61 |  201 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  202 | `	}` |
|     63 |  203 | `	ph7_result_bool(pCtx,res);` |
|     63 |  204 | `	return PH7_OK;` |
|      3 |  205 | `}` |
|      - |  206 | `/*` |
|      - |  207 | ` * float floatval($var)` |
|      - |  208 | ` *  Get float value of a variable.` |
|      - |  209 | ` * Parameter` |
|      - |  210 | ` *  $var: The variable being processed.` |
|      - |  211 | ` * Return` |
|      - |  212 | ` *  the float value of a variable.` |
|      - |  213 | ` */` |
|      6 |  214 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  215 | `{` |
|      7 |  216 | `	if( nArg < 1 ){` |
|      - |  217 | `		/* return 0.0 */` |
|      3 |  218 | `		ph7_result_double(pCtx,0);` |
|      2 |  219 | `	}else{` |
|      - |  220 | `		double dval;` |
|      - |  221 | `		/* Perform the cast */` |
|      5 |  222 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  223 | `		ph7_result_double(pCtx,dval);` |
|      - |  224 | `	}` |
|      7 |  225 | `	return PH7_OK;` |
|      1 |  226 | `}` |
|      - |  227 | `/*` |
|      - |  228 | ` * int intval($var)` |
|      - |  229 | ` *  Get integer value of a variable.` |
|      - |  230 | ` * Parameter` |
|      - |  231 | ` *  $var: The variable being processed.` |
|      - |  232 | ` * Return` |
|      - |  233 | ` *  the int value of a variable.` |
|      - |  234 | ` */` |
|     26 |  235 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  236 | `{` |
|     27 |  237 | `	if( nArg < 1 ){` |
|      - |  238 | `		/* return 0 */` |
|      3 |  239 | `		ph7_result_int(pCtx,0);` |
|      2 |  240 | `	}else{` |
|      - |  241 | `		sxi64 iVal;` |
|      - |  242 | `		/* Perform the cast */` |
|     25 |  243 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     25 |  244 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  245 | `	}` |
|     27 |  246 | `	return PH7_OK;` |
|      1 |  247 | `}` |
|      - |  248 | `/*` |
|      - |  249 | ` * string strval($var)` |
|      - |  250 | ` *  Get the string representation of a variable.` |
|      - |  251 | ` * Parameter` |
|      - |  252 | ` *  $var: The variable being processed.` |
|      - |  253 | ` * Return` |
|      - |  254 | ` *  the string value of a variable.` |
|      - |  255 | ` */` |
|      4 |  256 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  257 | `{` |
|      5 |  258 | `	if( nArg < 1 ){` |
|      - |  259 | `		/* return NULL */` |
|      3 |  260 | `		ph7_result_null(pCtx);` |
|      2 |  261 | `	}else{` |
|      - |  262 | `		const char *zVal;` |
|      3 |  263 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  264 | `		/* Perform the cast */` |
|      3 |  265 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  266 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  267 | `	}` |
|      5 |  268 | `	return PH7_OK;` |
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
|  27680 |  301 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  302 | `{` |
|  27685 |  303 | `	int res = 1; /* Assume empty by default */` |
|  27685 |  304 | `	if( nArg > 0 ){` |
|  27683 |  305 | `		res = ph7_value_is_empty(apArg[0]);` |
|  13839 |  306 | `	}` |
|  27685 |  307 | `	ph7_result_bool(pCtx,res);` |
|  27685 |  308 | `	return PH7_OK;` |
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
| 210256 |  351 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  352 | `{` |
|      - |  353 | `	const char *zSource,*zOfft;` |
|      - |  354 | `	int nOfft,nLen,nSrcLen;` |
| 210261 |  355 | `	if( nArg < 2 ){` |
|      - |  356 | `		/* return FALSE */` |
|      5 |  357 | `		ph7_result_bool(pCtx,0);` |
|      5 |  358 | `		return PH7_OK;` |
|      - |  359 | `	}` |
|      - |  360 | `	/* Extract the target string */` |
| 210257 |  361 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 210257 |  362 | `	if( nSrcLen < 1 ){` |
|      - |  363 | `		/* Empty string,return FALSE */` |
|  11861 |  364 | `		ph7_result_bool(pCtx,0);` |
|  11861 |  365 | `		return PH7_OK;` |
|      - |  366 | `	}` |
| 198401 |  367 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  368 | `	/* Extract the offset */` |
| 198401 |  369 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 198401 |  370 | `	if( nOfft < 0 ){` |
|  32171 |  371 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  32171 |  372 | `		if( zOfft < zSource ){` |
|      - |  373 | `			/* Invalid offset */` |
|      5 |  374 | `			ph7_result_bool(pCtx,0);` |
|      5 |  375 | `			return PH7_OK;` |
|      - |  376 | `		}` |
|  32167 |  377 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  32167 |  378 | `		nOfft = (int)(zOfft-zSource);` |
| 182316 |  379 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  380 | `		/* Invalid offset */` |
|    187 |  381 | `		ph7_result_bool(pCtx,0);` |
|    187 |  382 | `		return PH7_OK;` |
|    ! 0 |  383 | `	}else{` |
| 166053 |  384 | `		zOfft = &zSource[nOfft];` |
| 166053 |  385 | `		nLen = nSrcLen - nOfft;` |
|      - |  386 | `	}` |
| 198215 |  387 | `	if( nArg > 2 ){` |
|      - |  388 | `		/* Extract the length */` |
| 163315 |  389 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 163315 |  390 | `		if( nLen == 0 ){` |
|      - |  391 | `			/* Invalid length,return an empty string */` |
|      5 |  392 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  393 | `			return PH7_OK;` |
| 163311 |  394 | `		}else if( nLen < 0 ){` |
|  32159 |  395 | `			nLen = nSrcLen + nLen - nOfft;` |
|  32159 |  396 | `			if( nLen < 1 ){` |
|      - |  397 | `				/* Invalid  length */` |
|      3 |  398 | `				nLen = nSrcLen - nOfft;` |
|      1 |  399 | `			}` |
|  16077 |  400 | `		}` |
| 163311 |  401 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  402 | `			/* Invalid length */` |
|   4873 |  403 | `			nLen = nSrcLen - nOfft;` |
|   2434 |  404 | `		}` |
|  81653 |  405 | `	}` |
|      - |  406 | `	/* Return the substring */` |
| 198211 |  407 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 198211 |  408 | `	return PH7_OK;` |
| 105133 |  409 | `}` |
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
|     26 |  431 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  432 | `{` |
|      - |  433 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  434 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 |  435 | `	int iCase = 0;` |
|      - |  436 | `	int rc;` |
|     27 |  437 | `	if( nArg < 3 ){` |
|      - |  438 | `		/* Missing arguments,return FALSE */` |
|      5 |  439 | `		ph7_result_bool(pCtx,0);` |
|      5 |  440 | `		return PH7_OK;` |
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
|     14 |  501 | `}` |
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
|     24 |  518 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  519 | `{` |
|      - |  520 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  521 | `	int nTextlen,nPatlen;` |
|     25 |  522 | `	int iCount = 0;` |
|      - |  523 | `	sxu32 nOfft;` |
|      - |  524 | `	sxi32 rc;` |
|     25 |  525 | `	if( nArg < 2 ){` |
|      - |  526 | `		/* Missing arguments */` |
|      5 |  527 | `		ph7_result_int(pCtx,0);` |
|      5 |  528 | `		return PH7_OK;` |
|      - |  529 | `	}` |
|      - |  530 | `	/* Point to the haystack */` |
|     21 |  531 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  532 | `	/* Point to the neddle */` |
|     21 |  533 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 |  534 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  535 | `		/* NOOP,return zero */` |
|      3 |  536 | `		ph7_result_int(pCtx,0);` |
|      3 |  537 | `		return PH7_OK;` |
|      - |  538 | `	}` |
|     19 |  539 | `	if( nArg > 2 ){` |
|      - |  540 | `		int iOfft;` |
|      - |  541 | `		/* Extract the offset */` |
|     15 |  542 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 |  543 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - |  544 | `			/* Invalid offset,return zero */` |
|      3 |  545 | `			ph7_result_int(pCtx,0);` |
|      3 |  546 | `			return PH7_OK;` |
|      - |  547 | `		}` |
|      - |  548 | `		/* Point to the desired offset */` |
|     13 |  549 | `		zText = &zText[iOfft];` |
|      - |  550 | `		/* Adjust length */` |
|     13 |  551 | `		nTextlen -= iOfft;` |
|      6 |  552 | `	}` |
|      - |  553 | `	/* Point to the end of the string */` |
|     17 |  554 | `	zEnd = &zText[nTextlen];` |
|     17 |  555 | `	if( nArg > 3 ){` |
|      - |  556 | `		int nLen;` |
|      - |  557 | `		/* Extract the length */` |
|     13 |  558 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  559 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - |  560 | `			/* Invalid length,return 0 */` |
|      7 |  561 | `			ph7_result_int(pCtx,0);` |
|      7 |  562 | `			return PH7_OK;` |
|      - |  563 | `		}` |
|      - |  564 | `		/* Adjust pointer */` |
|      7 |  565 | `		nTextlen = nLen;` |
|      7 |  566 | `		zEnd = &zText[nTextlen];` |
|      3 |  567 | `	}` |
|      - |  568 | `	/* Perform the search */` |
|     12 |  569 | `	for(;;){` |
|     25 |  570 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 |  571 | `		if( rc != SXRET_OK ){` |
|      - |  572 | `			/* Pattern not found,break immediately */` |
|      9 |  573 | `			break;` |
|      - |  574 | `		}` |
|      - |  575 | `		/* Increment counter and update the offset */` |
|     17 |  576 | `		iCount++;` |
|     17 |  577 | `		zText += nOfft + nPatlen;` |
|     17 |  578 | `		if( zText >= zEnd ){` |
|      3 |  579 | `			break;` |
|      - |  580 | `		}` |
|      1 |  581 | `	}` |
|      - |  582 | `	/* Pattern count */` |
|     11 |  583 | `	ph7_result_int(pCtx,iCount);` |
|     11 |  584 | `	return PH7_OK;` |
|     13 |  585 | `}` |
|      - |  586 | `/*` |
|      - |  587 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - |  588 | ` *   Split a string into smaller chunks.` |
|      - |  589 | ` * Parameters` |
|      - |  590 | ` *  $body` |
|      - |  591 | ` *   The string to be chunked.` |
|      - |  592 | ` * $chunklen` |
|      - |  593 | ` *   The chunk length.` |
|      - |  594 | ` * $end` |
|      - |  595 | ` *   The line ending sequence.` |
|      - |  596 | ` * Return` |
|      - |  597 | ` *  The chunked string or NULL on failure.` |
|      - |  598 | ` */` |
|     16 |  599 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  600 | `{` |
|     17 |  601 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - |  602 | `	int nSepLen,nChunkLen,nLen;` |
|     17 |  603 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  604 | `		/* Nothing to split,return null */` |
|      5 |  605 | `		ph7_result_null(pCtx);` |
|      5 |  606 | `		return PH7_OK;` |
|      - |  607 | `	}` |
|      - |  608 | `	/* initialize/Extract arguments */` |
|     13 |  609 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 |  610 | `	nChunkLen = 76;` |
|     13 |  611 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 |  612 | `	zEnd = &zIn[nLen];` |
|     13 |  613 | `	if( nArg > 1 ){` |
|      - |  614 | `		/* Chunk length */` |
|     13 |  615 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 |  616 | `		if( nChunkLen < 1 ){` |
|      - |  617 | `			/* Switch back to the default length */` |
|      3 |  618 | `			nChunkLen = 76;` |
|      1 |  619 | `		}` |
|     13 |  620 | `		if( nArg > 2 ){` |
|      - |  621 | `			/* Separator */` |
|      9 |  622 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 |  623 | `			if( nSepLen < 1 ){` |
|      - |  624 | `				/* Switch back to the default separator */` |
|      3 |  625 | `				zSep = "\r\n";` |
|      3 |  626 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 |  627 | `			}` |
|      4 |  628 | `		}` |
|      6 |  629 | `	}` |
|      - |  630 | `	/* Perform the requested operation */` |
|     13 |  631 | `	if( nChunkLen > nLen ){` |
|      - |  632 | `		/* Nothing to split,return the string and the separator */` |
|      9 |  633 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 |  634 | `		return PH7_OK;` |
|      - |  635 | `	}` |
|     17 |  636 | `	while( zIn < zEnd ){` |
|     13 |  637 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 |  638 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 |  639 | `		}` |
|      - |  640 | `		/* Append the chunk and the separator */` |
|     13 |  641 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - |  642 | `		/* Point beyond the chunk */` |
|     13 |  643 | `		zIn += nChunkLen;` |
|      1 |  644 | `	}` |
|      5 |  645 | `	return PH7_OK;` |
|      9 |  646 | `}` |
|      - |  647 | `/*` |
|      - |  648 | ` * string addslashes(string $str)` |
|      - |  649 | ` *  Quote string with slashes.` |
|      - |  650 | ` *  Returns a string with backslashes before characters that need` |
|      - |  651 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  652 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  653 | ` * Parameter` |
|      - |  654 | ` *  str: The string to be escaped.` |
|      - |  655 | ` * Return` |
|      - |  656 | ` *  Returns the escaped string` |
|      - |  657 | ` */` |
|     24 |  658 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  659 | `{` |
|      - |  660 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  661 | `	int nLen;` |
|      - |  662 | `	/* PHP enforces exactly one argument. */` |
|     28 |  663 | `	if( nArg != 1 ){` |
|      8 |  664 | `		return PH7_VmThrowException(pCtx,` |
|      - |  665 | `			"ArgumentCountError",` |
|      - |  666 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 |  667 | `			nArg` |
|      - |  668 | `			);` |
|      - |  669 | `	}` |
|      - |  670 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - |  671 | `	 * types still produce a TypeError. */` |
|     22 |  672 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 |  673 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  674 | `			E_DEPRECATED,` |
|      - |  675 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  676 | `			);` |
|      - |  677 | `		/* fall through so conversion below yields empty string */` |
|      1 |  678 | `	}` |
|      - |  679 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     29 |  680 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 |  681 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 |  682 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 |  683 | `		return PH7_VmThrowException(pCtx,` |
|      - |  684 | `			"TypeError",` |
|      - |  685 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  686 | `			ph7_type_name(apArg[0])` |
|      - |  687 | `			);` |
|      - |  688 | `	}` |
|      - |  689 | `	/* Convert to string representation first and obtain length. */` |
|     19 |  690 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 |  691 | `	if( nLen < 1 ){` |
|      - |  692 | `		/* Return the empty string */` |
|      5 |  693 | `		ph7_result_string(pCtx,"",0);` |
|      5 |  694 | `		return PH7_OK;` |
|      - |  695 | `	}` |
|     15 |  696 | `	zEnd = &zIn[nLen];` |
|     15 |  697 | `	zCur = 0; /* cc warning */` |
|     20 |  698 | `	for(;;){` |
|     41 |  699 | `		if( zIn >= zEnd ){` |
|      - |  700 | `			/* No more input */` |
|     15 |  701 | `			break;` |
|      - |  702 | `		}` |
|     27 |  703 | `		zCur = zIn;` |
|      - |  704 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 |  705 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 |  706 | `			zIn++;` |
|      1 |  707 | `		}` |
|     27 |  708 | `		if( zIn > zCur ){` |
|      - |  709 | `			/* Append raw contents */` |
|     23 |  710 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 |  711 | `		}` |
|     27 |  712 | `		if( zIn < zEnd ){` |
|     17 |  713 | `			int c = zIn[0];` |
|     17 |  714 | `			if( c == '\0' ){` |
|      - |  715 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 |  716 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 |  717 | `			}else{` |
|     15 |  718 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  719 | `			}` |
|      8 |  720 | `		}` |
|     27 |  721 | `		zIn++;` |
|      1 |  722 | `	}` |
|     15 |  723 | `	return PH7_OK;` |
|     16 |  724 | `}` |
|      - |  725 | `/*` |
|      - |  726 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - |  727 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - |  728 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - |  729 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - |  730 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - |  731 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - |  732 | ` * [zList, zList+nLen).` |
|      - |  733 | ` *` |
|      - |  734 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - |  735 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - |  736 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - |  737 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - |  738 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - |  739 | ` */` |
|     78 |  740 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      3 |  741 | `{` |
|     81 |  742 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|     81 |  743 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|     81 |  744 | `	SyZero(aMask,256);` |
|    291 |  745 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    213 |  746 | `		int c = zIn[0];` |
|    213 |  747 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - |  748 | `			/* Valid incrementing range c..zIn[3] */` |
|     20 |  749 | `			int hi = zIn[3],k;` |
|    364 |  750 | `			for( k = c ; k <= hi ; k++ ){` |
|    346 |  751 | `				aMask[k] = 1;` |
|    174 |  752 | `			}` |
|     20 |  753 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    213 |  754 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - |  755 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - |  756 | `			const char *zMsg;` |
|     20 |  757 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 |  758 | `				zMsg = "no character to the left of '..'";` |
|     18 |  759 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 |  760 | `				zMsg = "no character to the right of '..'";` |
|     14 |  761 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 |  762 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 |  763 | `			}else{` |
|    ! 0 |  764 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - |  765 | `			}` |
|     20 |  766 | `			if( zMsg ){` |
|     29 |  767 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 |  768 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 |  769 | `			}else{` |
|    ! 0 |  770 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  771 | `					"Invalid '..'-range");` |
|      - |  772 | `			}` |
|      - |  773 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - |  774 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 |  775 | `		}else{` |
|    177 |  776 | `			aMask[c] = 1;` |
|      - |  777 | `		}` |
|    108 |  778 | `	}` |
|     81 |  779 | `}` |
|      - |  780 | `/*` |
|      - |  781 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  782 | ` *  Quote string with slashes in a C style.` |
|      - |  783 | ` * Parameter` |
|      - |  784 | ` *  $str:` |
|      - |  785 | ` *    The string to be escaped.` |
|      - |  786 | ` *  $charlist:` |
|      - |  787 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  788 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  789 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  790 | ` * Return` |
|      - |  791 | ` *  Returns the escaped string.` |
|      - |  792 | ` * Note:` |
|      - |  793 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - |  794 | ` */` |
|     40 |  795 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  796 | `{` |
|      - |  797 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  798 | `	char aMask[256];` |
|      - |  799 | `	int nLen,nMask;` |
|      - |  800 | `	/* PHP enforces exactly two arguments. */` |
|     45 |  801 | `	if( nArg != 2 ){` |
|      8 |  802 | `		return PH7_VmThrowException(pCtx,` |
|      - |  803 | `			"ArgumentCountError",` |
|      - |  804 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  805 | `			nArg` |
|      - |  806 | `			);` |
|      - |  807 | `	}` |
|      - |  808 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  809 | `	 * treated as the empty string (PHP 8.1). */` |
|     40 |  810 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  811 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  812 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  813 | `			E_DEPRECATED,` |
|      - |  814 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  815 | `			);` |
|      - |  816 | `		/* treat as empty string; fall through to conversion logic */` |
|     52 |  817 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     52 |  818 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     32 |  819 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  820 | `		return PH7_VmThrowException(pCtx,` |
|      - |  821 | `			"TypeError",` |
|      - |  822 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  823 | `			ph7_type_name(apArg[0])` |
|      - |  824 | `			);` |
|      - |  825 | `	}` |
|      - |  826 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  827 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  828 | `	 * trigger a TypeError. */` |
|     37 |  829 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  830 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  831 | `			E_DEPRECATED,` |
|      - |  832 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  833 | `			);` |
|      - |  834 | `		/* allow through so it becomes empty string below */` |
|     49 |  835 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 |  836 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 |  837 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  838 | `		return PH7_VmThrowException(pCtx,` |
|      - |  839 | `			"TypeError",` |
|      - |  840 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  841 | `			ph7_type_name(apArg[1])` |
|      - |  842 | `			);` |
|      - |  843 | `	}` |
|      - |  844 | `	/* Extract the string to process */` |
|     35 |  845 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  846 | `	/* NULL would never reach here due to the check above. */` |
|     35 |  847 | `	if( nLen < 1 ){` |
|      - |  848 | `		/* Empty string returns itself. */` |
|      5 |  849 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  850 | `		return PH7_OK;` |
|      - |  851 | `	}` |
|      - |  852 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 |  853 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 |  854 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 |  855 | `	zEnd = &zIn[nLen];` |
|     31 |  856 | `	zCur = 0; /* cc warning */` |
|     37 |  857 | `	for(;;){` |
|     77 |  858 | `		if( zIn >= zEnd ){` |
|      - |  859 | `			/* No more input */` |
|     31 |  860 | `			break;` |
|      - |  861 | `		}` |
|     49 |  862 | `		zCur = zIn;` |
|    125 |  863 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 |  864 | `			zIn++;` |
|      3 |  865 | `		}` |
|     49 |  866 | `		if( zIn > zCur ){` |
|      - |  867 | `			/* Append raw contents */` |
|     43 |  868 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 |  869 | `		}` |
|     49 |  870 | `		if( zIn < zEnd ){` |
|      - |  871 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  872 | `			 * on platforms where char is signed. */` |
|     29 |  873 | `			int c = (unsigned char)zIn[0];` |
|      - |  874 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  875 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  876 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 |  877 | `			if( c == '\n' ){` |
|      3 |  878 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 |  879 | `			}else if( c == '\r' ){` |
|      3 |  880 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 |  881 | `			}else if( c == '\t' ){` |
|      3 |  882 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 |  883 | `			}else if( c == '\v' ){` |
|      3 |  884 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 |  885 | `			}else if( c == '\f' ){` |
|      3 |  886 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 |  887 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  888 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  889 | `				 * octal escapes (\001 not \1). */` |
|      7 |  890 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  891 | `			}else{` |
|     13 |  892 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  893 | `			}` |
|     13 |  894 | `		}` |
|     49 |  895 | `		zIn++;` |
|      3 |  896 | `	}` |
|     31 |  897 | `	return PH7_OK;` |
|     25 |  898 | `}` |
|      - |  899 | `/*` |
|      - |  900 | ` * string quotemeta(string $str)` |
|      - |  901 | ` *  Quote meta characters.` |
|      - |  902 | ` * Parameter` |
|      - |  903 | ` *  $str:` |
|      - |  904 | ` *    The string to be escaped.` |
|      - |  905 | ` * Return` |
|      - |  906 | ` *  Returns the escaped string.` |
|      - |  907 | `*/` |
|     12 |  908 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  909 | `{` |
|      - |  910 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  911 | `	char aMask[256];` |
|      - |  912 | `	int nLen;` |
|     14 |  913 | `	if( nArg < 1 ){` |
|      - |  914 | `		/* Nothing to process,retun NULL */` |
|      3 |  915 | `		ph7_result_null(pCtx);` |
|      3 |  916 | `		return PH7_OK;` |
|      - |  917 | `	}` |
|      - |  918 | `	/* Extract the string to process */` |
|     12 |  919 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 |  920 | `	if( nLen < 1 ){` |
|      - |  921 | `		/* Return the empty string */` |
|      3 |  922 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  923 | `		return PH7_OK;` |
|      - |  924 | `	}` |
|      - |  925 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 |  926 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 |  927 | `	zEnd = &zIn[nLen];` |
|     10 |  928 | `	zCur = 0; /* cc warning */` |
|     22 |  929 | `	for(;;){` |
|     46 |  930 | `		if( zIn >= zEnd ){` |
|      - |  931 | `			/* No more input */` |
|     10 |  932 | `			break;` |
|      - |  933 | `		}` |
|     38 |  934 | `		zCur = zIn;` |
|     76 |  935 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 |  936 | `			zIn++;` |
|      2 |  937 | `		}` |
|     38 |  938 | `		if( zIn > zCur ){` |
|      - |  939 | `			/* Append raw contents */` |
|     20 |  940 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 |  941 | `		}` |
|     38 |  942 | `		if( zIn < zEnd ){` |
|     36 |  943 | `			int c = zIn[0];` |
|     36 |  944 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 |  945 | `		}` |
|     38 |  946 | `		zIn++;` |
|      2 |  947 | `	}` |
|     10 |  948 | `	return PH7_OK;` |
|      8 |  949 | `}` |
|      - |  950 | `/*` |
|      - |  951 | ` * string stripslashes(string $str)` |
|      - |  952 | ` *  Un-quotes a quoted string.` |
|      - |  953 | ` *  Returns a string with backslashes before characters that need` |
|      - |  954 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  955 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  956 | ` * Parameter` |
|      - |  957 | ` *  $str` |
|      - |  958 | ` *   The input string.` |
|      - |  959 | ` * Return` |
|      - |  960 | ` *  Returns a string with backslashes stripped off.` |
|      - |  961 | ` */` |
|      8 |  962 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  963 | `{` |
|      - |  964 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  965 | `	int nLen;` |
|      9 |  966 | `	if( nArg < 1 ){` |
|      - |  967 | `		/* Nothing to process,retun NULL */` |
|      3 |  968 | `		ph7_result_null(pCtx);` |
|      3 |  969 | `		return PH7_OK;` |
|      - |  970 | `	}` |
|      - |  971 | `	/* Extract the string to process */` |
|      7 |  972 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  973 | `	if( zIn == 0 ){` |
|    ! 0 |  974 | `		ph7_result_null(pCtx);` |
|    ! 0 |  975 | `		return PH7_OK;` |
|      - |  976 | `	}` |
|      7 |  977 | `	zEnd = &zIn[nLen];` |
|      7 |  978 | `	zCur = 0; /* cc warning */` |
|      - |  979 | `	/* Encode the string */` |
|      4 |  980 | `	for(;;){` |
|      9 |  981 | `		if( zIn >= zEnd ){` |
|      - |  982 | `			/* No more input */` |
|      5 |  983 | `			break;` |
|      - |  984 | `		}` |
|      5 |  985 | `		zCur = zIn;` |
|     17 |  986 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  987 | `			zIn++;` |
|      1 |  988 | `		}` |
|      5 |  989 | `		if( zIn > zCur ){` |
|      - |  990 | `			/* Append raw contents */` |
|      5 |  991 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 |  992 | `		}` |
|      5 |  993 | `		if( &zIn[1] < zEnd ){` |
|      3 |  994 | `			int c = zIn[1];` |
|      3 |  995 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - |  996 | `				/* Ignore the backslash */` |
|      3 |  997 | `				zIn++;` |
|      1 |  998 | `			}` |
|      2 |  999 | `		}else{` |
|      3 | 1000 | `			break;` |
|      - | 1001 | `		}` |
|      1 | 1002 | `	}` |
|      7 | 1003 | `	return PH7_OK;` |
|      5 | 1004 | `}` |
|      - | 1005 | `/*` |
|      - | 1006 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1007 | ` *  HTML escaping of special characters.` |
|      - | 1008 | ` *  The translations performed are:` |
|      - | 1009 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1010 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1011 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1012 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1013 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1014 | ` * Parameters` |
|      - | 1015 | ` *  $string` |
|      - | 1016 | ` *   The string being converted.` |
|      - | 1017 | ` * $flags` |
|      - | 1018 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1019 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1020 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1021 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1022 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1023 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1024 | ` * $charset` |
|      - | 1025 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1026 | ` * Return` |
|      - | 1027 | ` *  The escaped string or NULL on failure.` |
|      - | 1028 | ` */` |
|     20 | 1029 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1030 | `{` |
|      - | 1031 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1032 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1033 | `	int nLen,c;` |
|     21 | 1034 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1035 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1036 | `		ph7_result_null(pCtx);` |
|      9 | 1037 | `		return PH7_OK;` |
|      - | 1038 | `	}` |
|      - | 1039 | `	/* Extract the target string */` |
|     13 | 1040 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1041 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1042 | `	if( nLen == 0 ){` |
|      3 | 1043 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1044 | `		return PH7_OK;` |
|      - | 1045 | `	}` |
|     11 | 1046 | `	zEnd = &zIn[nLen];` |
|      - | 1047 | `	/* Extract the flags if available */` |
|     11 | 1048 | `	if( nArg > 1 ){` |
|      9 | 1049 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1050 | `		if( iFlags < 0 ){` |
|      3 | 1051 | `			iFlags = 0x01\|0x40;` |
|      1 | 1052 | `		}` |
|      4 | 1053 | `	}` |
|      - | 1054 | `	/* Perform the requested operation */` |
|     23 | 1055 | `	for(;;){` |
|     47 | 1056 | `		if( zIn >= zEnd ){` |
|      9 | 1057 | `			break;` |
|      - | 1058 | `		}` |
|     39 | 1059 | `		zCur = zIn;` |
|     83 | 1060 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1061 | `			zIn++;` |
|      1 | 1062 | `		}` |
|     39 | 1063 | `		if( zCur < zIn ){` |
|      - | 1064 | `			/* Append the raw string verbatim */` |
|     17 | 1065 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1066 | `		}` |
|     39 | 1067 | `		if( zIn >= zEnd ){` |
|      3 | 1068 | `			break;` |
|      - | 1069 | `		}` |
|     37 | 1070 | `		c = zIn[0];` |
|     37 | 1071 | `		if( c == '&' ){` |
|      - | 1072 | `			/* Expand '&amp;' */` |
|      9 | 1073 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1074 | `		}else if( c == '<' ){` |
|      - | 1075 | `			/* Expand '&lt;' */` |
|      7 | 1076 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1077 | `		}else if( c == '>' ){` |
|      - | 1078 | `			/* Expand '&gt;' */` |
|      9 | 1079 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1080 | `		}else if( c == '\'' ){` |
|      5 | 1081 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1082 | `				/* Expand '&#039;' */` |
|      5 | 1083 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1084 | `			}else{` |
|      - | 1085 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1086 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1087 | `			}` |
|     13 | 1088 | `		}else if( c == '"' ){` |
|     11 | 1089 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1090 | `				/* Expand '&quot;' */` |
|      7 | 1091 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1092 | `			}else{` |
|      - | 1093 | `				/* Leave the double quote untouched */` |
|      5 | 1094 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1095 | `			}` |
|      5 | 1096 | `		}` |
|      - | 1097 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1098 | `		zIn++;` |
|      1 | 1099 | `	}` |
|     11 | 1100 | `	return PH7_OK;` |
|     11 | 1101 | `}` |
|      - | 1102 | `/*` |
|      - | 1103 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1104 | ` *  Unescape HTML entities.` |
|      - | 1105 | ` * Parameters` |
|      - | 1106 | ` *  $string` |
|      - | 1107 | ` *   The string to decode` |
|      - | 1108 | ` *  $quote_style` |
|      - | 1109 | ` *    The quote style. One of the following constants:` |
|      - | 1110 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1111 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1112 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1113 | ` * Return` |
|      - | 1114 | ` *  The unescaped string or NULL on failure.` |
|      - | 1115 | ` */` |
|     16 | 1116 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1117 | `{` |
|      - | 1118 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1119 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1120 | `	int nLen,nJump;` |
|     17 | 1121 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1122 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1123 | `		ph7_result_null(pCtx);` |
|      7 | 1124 | `		return PH7_OK;` |
|      - | 1125 | `	}` |
|      - | 1126 | `	/* Extract the target string */` |
|     11 | 1127 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1128 | `	zEnd = &zIn[nLen];` |
|      - | 1129 | `	/* Extract the flags if available */` |
|     11 | 1130 | `	if( nArg > 1 ){` |
|      7 | 1131 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1132 | `		if( iFlags < 0 ){` |
|      3 | 1133 | `			iFlags = 0x01;` |
|      1 | 1134 | `		}` |
|      3 | 1135 | `	}` |
|      - | 1136 | `	/* Perform the requested operation */` |
|     15 | 1137 | `	for(;;){` |
|     31 | 1138 | `		if( zIn >= zEnd ){` |
|     11 | 1139 | `			break;` |
|      - | 1140 | `		}` |
|     21 | 1141 | `		zCur = zIn;` |
|     51 | 1142 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1143 | `			zIn++;` |
|      1 | 1144 | `		}` |
|     21 | 1145 | `		if( zCur < zIn ){` |
|      - | 1146 | `			/* Append the raw string verbatim */` |
|      9 | 1147 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1148 | `		}` |
|     21 | 1149 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1150 | `		nJump = (int)sizeof(char);` |
|     21 | 1151 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1152 | `			/* &amp; ==> '&' */` |
|      3 | 1153 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1154 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1155 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1156 | `			/* &lt; ==> < */` |
|      3 | 1157 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1158 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1159 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1160 | `			/* &gt; ==> '>' */` |
|      3 | 1161 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1162 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1163 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1164 | `			/* &quot; ==> '"' */` |
|     13 | 1165 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1166 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1167 | `			}else{` |
|      - | 1168 | `				/* Leave untouched */` |
|      5 | 1169 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1170 | `			}` |
|     13 | 1171 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1172 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1173 | `			/* &#039; ==> ''' */` |
|      3 | 1174 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1175 | `				/* Expand ''' */` |
|      3 | 1176 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1177 | `			}else{` |
|      - | 1178 | `				/* Leave untouched */` |
|    ! 0 | 1179 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1180 | `			}` |
|      3 | 1181 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1182 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1183 | `			/* expand '&' */` |
|    ! 0 | 1184 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1185 | `		}else{` |
|      - | 1186 | `			/* No more input to process */` |
|    ! 0 | 1187 | `			break;` |
|      - | 1188 | `		}` |
|     21 | 1189 | `		zIn += nJump;` |
|      1 | 1190 | `	}` |
|     11 | 1191 | `	return PH7_OK;` |
|      9 | 1192 | `}` |
|      - | 1193 | `/* HTML encoding/Decoding table` |
|      - | 1194 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1195 | ` */` |
|      - | 1196 | `static const char *azHtmlEscape[] = {` |
|      - | 1197 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1198 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1199 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1200 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1201 | ` };` |
|      - | 1202 | `/*` |
|      - | 1203 | ` * array get_html_translation_table(void)` |
|      - | 1204 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1205 | ` * Parameters` |
|      - | 1206 | ` *  None` |
|      - | 1207 | ` * Return` |
|      - | 1208 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1209 | ` */` |
|      4 | 1210 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1211 | `{` |
|      - | 1212 | `	ph7_value *pArray,*pValue;` |
|      - | 1213 | `	sxu32 n;` |
|      - | 1214 | `	/* Element value */` |
|      5 | 1215 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1216 | `	if( pValue == 0 ){` |
|    ! 0 | 1217 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1218 | `		SXUNUSED(apArg);` |
|      - | 1219 | `		/* Return NULL */` |
|    ! 0 | 1220 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1221 | `		return PH7_OK;` |
|      - | 1222 | `	}` |
|      - | 1223 | `	/* Create a new array */` |
|      5 | 1224 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1225 | `	if( pArray == 0 ){` |
|      - | 1226 | `		/* Return NULL */` |
|    ! 0 | 1227 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1228 | `		return PH7_OK;` |
|      - | 1229 | `	}` |
|      - | 1230 | `	/* Make the table */` |
|     85 | 1231 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1232 | `		/* Prepare the value */` |
|     81 | 1233 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1234 | `		/* Insert the value */` |
|     81 | 1235 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1236 | `		/* Reset the string cursor */` |
|     81 | 1237 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1238 | `	}` |
|      - | 1239 | `	/*` |
|      - | 1240 | `	 * Return the array.` |
|      - | 1241 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1242 | `	 * released upon we return from this function.` |
|      - | 1243 | `	 */` |
|      5 | 1244 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1245 | `	return PH7_OK;` |
|      3 | 1246 | `}` |
|      - | 1247 | `/*` |
|      - | 1248 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1249 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1250 | ` * Parameters` |
|      - | 1251 | ` * $string` |
|      - | 1252 | ` *   The input string.` |
|      - | 1253 | ` * $flags` |
|      - | 1254 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1255 | ` * Return` |
|      - | 1256 | ` * The encoded string.` |
|      - | 1257 | ` */` |
|     10 | 1258 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1259 | `{` |
|     11 | 1260 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1261 | `	const char *zIn,*zEnd;` |
|      - | 1262 | `	int nLen,c;` |
|      - | 1263 | `	sxu32 n;` |
|     11 | 1264 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1265 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1266 | `		ph7_result_null(pCtx);` |
|      5 | 1267 | `		return PH7_OK;` |
|      - | 1268 | `	}` |
|      - | 1269 | `	/* Extract the target string */` |
|      7 | 1270 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1271 | `	/* Handle empty string up front */` |
|      7 | 1272 | `	if( nLen == 0 ){` |
|      3 | 1273 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1274 | `		return PH7_OK;` |
|      - | 1275 | `	}` |
|      5 | 1276 | `	zEnd = &zIn[nLen];` |
|      - | 1277 | `	/* Extract the flags if available */` |
|      5 | 1278 | `	if( nArg > 1 ){` |
|      3 | 1279 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1280 | `		if( iFlags < 0 ){` |
|      3 | 1281 | `			iFlags = 0x01;` |
|      1 | 1282 | `		}` |
|      1 | 1283 | `	}` |
|      - | 1284 | `	/* Perform the requested operation */` |
|     11 | 1285 | `	for(;;){` |
|     23 | 1286 | `		if( zIn >= zEnd ){` |
|      - | 1287 | `			/* No more input to process */` |
|      5 | 1288 | `			break;` |
|      - | 1289 | `		}` |
|     19 | 1290 | `		c = zIn[0];` |
|      - | 1291 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1292 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1293 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1294 | `				/* Got one */` |
|      9 | 1295 | `				break;` |
|      - | 1296 | `			}` |
|    108 | 1297 | `		}` |
|     19 | 1298 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1299 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1300 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1301 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 1302 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 1303 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 1304 | `				/* expand single quote verbatim */` |
|    ! 0 | 1305 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 1306 | `			}else{` |
|      9 | 1307 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 1308 | `			}` |
|      5 | 1309 | `		}else{` |
|      - | 1310 | `			/* Output character verbatim */` |
|     11 | 1311 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1312 | `		}` |
|     19 | 1313 | `		zIn++;` |
|      1 | 1314 | `	}` |
|      5 | 1315 | `	return PH7_OK;` |
|      6 | 1316 | `}` |
|      - | 1317 | `/*` |
|      - | 1318 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 1319 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 1320 | ` * Parameters` |
|      - | 1321 | ` * $string` |
|      - | 1322 | ` *   The input string.` |
|      - | 1323 | ` * $flags` |
|      - | 1324 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 1325 | ` * Return` |
|      - | 1326 | ` * The decoded string.` |
|      - | 1327 | ` */` |
|     28 | 1328 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1329 | `{` |
|      - | 1330 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 1331 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 1332 | `	int nLen;` |
|      - | 1333 | `	sxu32 n;` |
|     29 | 1334 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1335 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1336 | `		ph7_result_null(pCtx);` |
|      5 | 1337 | `		return PH7_OK;` |
|      - | 1338 | `	}` |
|      - | 1339 | `	/* Extract the target string */` |
|     25 | 1340 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1341 | `	zEnd = &zIn[nLen];` |
|      - | 1342 | `	/* Extract the flags if available */` |
|     25 | 1343 | `	if( nArg > 1 ){` |
|     15 | 1344 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 1345 | `		if( iFlags < 0 ){` |
|      3 | 1346 | `			iFlags = 0x01;` |
|      1 | 1347 | `		}` |
|      7 | 1348 | `	}` |
|      - | 1349 | `	/* Perform the requested operation */` |
|     27 | 1350 | `	for(;;){` |
|     55 | 1351 | `		if( zIn >= zEnd ){` |
|      - | 1352 | `			/* No more input to process */` |
|     13 | 1353 | `			break;` |
|      - | 1354 | `		}` |
|     43 | 1355 | `		zCur = zIn;` |
|    173 | 1356 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 1357 | `			zIn++;` |
|      1 | 1358 | `		}` |
|     43 | 1359 | `		if( zCur < zIn ){` |
|      - | 1360 | `			/* Append raw string verbatim */` |
|     27 | 1361 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 1362 | `		}` |
|     43 | 1363 | `		if( zIn >= zEnd ){` |
|     13 | 1364 | `			break;` |
|      - | 1365 | `		}` |
|     31 | 1366 | `		nLen = (int)(zEnd-zIn);` |
|      - | 1367 | `		/* Find an encoded sequence */` |
|    113 | 1368 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 1369 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 1370 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 1371 | `				/* Got one */` |
|     31 | 1372 | `				zIn += iLen;` |
|     31 | 1373 | `				break;` |
|      - | 1374 | `			}` |
|     42 | 1375 | `		}` |
|     31 | 1376 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 1377 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 1378 | `			/* Output the decoded character */` |
|     31 | 1379 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 1380 | `				/* Do not process single quotes */` |
|      9 | 1381 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 1382 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1383 | `				/* Do not process double quotes */` |
|      5 | 1384 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 1385 | `			}else{` |
|     19 | 1386 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 1387 | `			}` |
|     16 | 1388 | `		}else{` |
|      - | 1389 | `			/* Append '&' */` |
|    ! 0 | 1390 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1391 | `			zIn++;` |
|      - | 1392 | `		}` |
|      1 | 1393 | `	}` |
|     25 | 1394 | `	return PH7_OK;` |
|     15 | 1395 | `}` |
|      - | 1396 | `/*` |
|      - | 1397 | ` * int strlen($string)` |
|      - | 1398 | ` *  return the length of the given string.` |
|      - | 1399 | ` * Parameter` |
|      - | 1400 | ` *  string: The string being measured for length.` |
|      - | 1401 | ` * Return` |
|      - | 1402 | ` *  length of the given string.` |
|      - | 1403 | ` */` |
|   8534 | 1404 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1405 | `{` |
|   8539 | 1406 | `	int iLen = 0;` |
|   8539 | 1407 | `	if( nArg > 0 ){` |
|   8537 | 1408 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   4266 | 1409 | `	}` |
|      - | 1410 | `	/* String length */` |
|   8539 | 1411 | `	ph7_result_int(pCtx,iLen);` |
|   8539 | 1412 | `	return PH7_OK;` |
|      5 | 1413 | `}` |
|      - | 1414 | `/*` |
|      - | 1415 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1416 | ` *  Perform a binary safe string comparison.` |
|      - | 1417 | ` * Parameter` |
|      - | 1418 | ` *  str1: The first string` |
|      - | 1419 | ` *  str2: The second string` |
|      - | 1420 | ` * Return` |
|      - | 1421 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1422 | ` *  than str2, and 0 if they are equal.` |
|      - | 1423 | ` */` |
|     80 | 1424 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1425 | `{` |
|      - | 1426 | `	const char *z1,*z2;` |
|      - | 1427 | `	int n1,n2;` |
|      - | 1428 | `	int res;` |
|     81 | 1429 | `	if( nArg < 2 ){` |
|      5 | 1430 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 1431 | `		ph7_result_int(pCtx,res);` |
|      5 | 1432 | `		return PH7_OK;` |
|      - | 1433 | `	}` |
|      - | 1434 | `	/* Perform the comparison */` |
|     77 | 1435 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     77 | 1436 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     77 | 1437 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1438 | `	/* Comparison result */` |
|     77 | 1439 | `	ph7_result_int(pCtx,res);` |
|     77 | 1440 | `	return PH7_OK;` |
|     41 | 1441 | `}` |
|      - | 1442 | `/*` |
|      - | 1443 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1444 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1445 | ` * Parameter` |
|      - | 1446 | ` *  str1: The first string` |
|      - | 1447 | ` *  str2: The second string` |
|      - | 1448 | ` * Return` |
|      - | 1449 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1450 | ` *  than str2, and 0 if they are equal.` |
|      - | 1451 | ` */` |
|     20 | 1452 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1453 | `{` |
|      - | 1454 | `	const char *z1,*z2;` |
|      - | 1455 | `	int res;` |
|      - | 1456 | `	int n;` |
|     21 | 1457 | `	if( nArg < 3 ){` |
|      - | 1458 | `		/* Perform a standard comparison */` |
|      5 | 1459 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1460 | `	}` |
|      - | 1461 | `	/* Desired comparison length */` |
|     17 | 1462 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1463 | `	if( n < 0 ){` |
|      - | 1464 | `		/* Invalid length */` |
|      3 | 1465 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1466 | `		return PH7_OK;` |
|      - | 1467 | `	}` |
|      - | 1468 | `	/* Perform the comparison */` |
|     15 | 1469 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1470 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1471 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1472 | `	/* Comparison result */` |
|     15 | 1473 | `	ph7_result_int(pCtx,res);` |
|     15 | 1474 | `	return PH7_OK;` |
|     11 | 1475 | `}` |
|      - | 1476 | `/*` |
|      - | 1477 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1478 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1479 | ` * Parameter` |
|      - | 1480 | ` *  str1: The first string` |
|      - | 1481 | ` *  str2: The second string` |
|      - | 1482 | ` * Return` |
|      - | 1483 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1484 | ` *  than str2, and 0 if they are equal.` |
|      - | 1485 | ` */` |
|     22 | 1486 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1487 | `{` |
|      - | 1488 | `	const char *z1,*z2;` |
|      - | 1489 | `	int n1,n2;` |
|      - | 1490 | `	int res;` |
|     23 | 1491 | `	if( nArg < 2 ){` |
|      9 | 1492 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 1493 | `		ph7_result_int(pCtx,res);` |
|      9 | 1494 | `		return PH7_OK;` |
|      - | 1495 | `	}` |
|      - | 1496 | `	/* Perform the comparison */` |
|     15 | 1497 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1498 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1499 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1500 | `	/* Comparison result */` |
|     15 | 1501 | `	ph7_result_int(pCtx,res);` |
|     15 | 1502 | `	return PH7_OK;` |
|     12 | 1503 | `}` |
|      - | 1504 | `/*` |
|      - | 1505 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1506 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1507 | ` * Parameter` |
|      - | 1508 | ` *  $str1: The first string` |
|      - | 1509 | ` *  $str2: The second string` |
|      - | 1510 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1511 | ` * Return` |
|      - | 1512 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1513 | ` *  than str2, and 0 if they are equal.` |
|      - | 1514 | ` */` |
|      8 | 1515 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1516 | `{` |
|      - | 1517 | `	const char *z1,*z2;` |
|      - | 1518 | `	int res;` |
|      - | 1519 | `	int n;` |
|      9 | 1520 | `	if( nArg < 3 ){` |
|      - | 1521 | `		/* Perform a standard comparison */` |
|      5 | 1522 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1523 | `	}` |
|      - | 1524 | `	/* Desired comparison length */` |
|      5 | 1525 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1526 | `	if( n < 0 ){` |
|      - | 1527 | `		/* Invalid length */` |
|    ! 0 | 1528 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1529 | `		return PH7_OK;` |
|      - | 1530 | `	}` |
|      - | 1531 | `	/* Perform the comparison */` |
|      5 | 1532 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1533 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1534 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1535 | `	/* Comparison result */` |
|      5 | 1536 | `	ph7_result_int(pCtx,res);` |
|      5 | 1537 | `	return PH7_OK;` |
|      5 | 1538 | `}` |
|      - | 1539 | `/*` |
|      - | 1540 | ` * Implode context [i.e: it's private data].` |
|      - | 1541 | ` * A pointer to the following structure is forwarded` |
|      - | 1542 | ` * verbatim to the array walker callback defined below.` |
|      - | 1543 | ` */` |
|      - | 1544 | `struct implode_data {` |
|      - | 1545 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1546 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1547 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1548 | `	int nSeplen;          /* Separator length */` |
|      - | 1549 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1550 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1551 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1552 | `};` |
|      - | 1553 | `/*` |
|      - | 1554 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1555 | ` * The following routine is invoked for each array entry passed` |
|      - | 1556 | ` * to the implode() function.` |
|      - | 1557 | ` */` |
| 131778 | 1558 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1559 | `{` |
|  65889 | 1560 | `	SXUNUSED(pKey);` |
| 131783 | 1561 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1562 | `	const char *zData;` |
|      - | 1563 | `	int nLen;` |
| 131783 | 1564 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1565 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1566 | `			if( !pData->bFirst ){` |
|      - | 1567 | `				/* append the separator first */` |
|      3 | 1568 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1569 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1570 | `					return PH7_ABORT;` |
|      - | 1571 | `				}` |
|      2 | 1572 | `			}else{` |
|    ! 0 | 1573 | `				pData->bFirst = 0;` |
|      - | 1574 | `			}` |
|      1 | 1575 | `		}` |
|      - | 1576 | `		/* Recurse */` |
|      3 | 1577 | `		pData->bFirst = 1;` |
|      3 | 1578 | `		pData->nRecCount++;` |
|      3 | 1579 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1580 | `		pData->nRecCount--;` |
|      - | 1581 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1582 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1583 | `			return PH7_ABORT;` |
|      - | 1584 | `		}` |
|      3 | 1585 | `		return PH7_OK;` |
|      - | 1586 | `	}` |
|      - | 1587 | `	/* Extract the string representation of the entry value */` |
| 131781 | 1588 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1589 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 131781 | 1590 | `	if( pData->bFirst ){` |
|  32503 | 1591 | `		pData->bFirst = 0;` |
| 115532 | 1592 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1593 | `		/* append the separator first */` |
|  99271 | 1594 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1595 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1596 | `			return PH7_ABORT;` |
|      - | 1597 | `		}` |
|  49633 | 1598 | `	}` |
|      - | 1599 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 131781 | 1600 | `	if( nLen > 0 ){` |
| 119925 | 1601 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1602 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1603 | `			return PH7_ABORT;` |
|      - | 1604 | `		}` |
|  59960 | 1605 | `	}` |
| 131781 | 1606 | `	return PH7_OK;` |
|  65894 | 1607 | `}` |
|      - | 1608 | `/*` |
|      - | 1609 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1610 | ` * string implode(array $pieces,...)` |
|      - | 1611 | ` *  Join array elements with a string.` |
|      - | 1612 | ` * $glue` |
|      - | 1613 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1614 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1615 | ` * $pieces` |
|      - | 1616 | ` *   The array of strings to implode.` |
|      - | 1617 | ` * Return` |
|      - | 1618 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1619 | ` *  order, with the glue string between each element.` |
|      - | 1620 | ` */` |
|  32524 | 1621 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1622 | `{` |
|      - | 1623 | `	struct implode_data imp_data;` |
|  32529 | 1624 | `	int i = 1;` |
|  32529 | 1625 | `	if( nArg < 1 ){` |
|      - | 1626 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1627 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1628 | `		return PH7_OK;` |
|      - | 1629 | `	}` |
|      - | 1630 | `	/* Prepare the implode context */` |
|  32529 | 1631 | `	imp_data.pCtx = pCtx;` |
|  32529 | 1632 | `	imp_data.bRecursive = 0;` |
|  32529 | 1633 | `	imp_data.bFirst = 1;` |
|  32529 | 1634 | `	imp_data.nRecCount = 0;` |
|  32529 | 1635 | `	imp_data.rc = SXRET_OK;` |
|  32529 | 1636 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  32527 | 1637 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16266 | 1638 | `	}else{` |
|      3 | 1639 | `		imp_data.zSep = 0;` |
|      3 | 1640 | `		imp_data.nSeplen = 0;` |
|      3 | 1641 | `		i = 0;` |
|      - | 1642 | `	}` |
|  32529 | 1643 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1644 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1645 | `	}` |
|      - | 1646 | `	/* Start the 'join' process */` |
|  65053 | 1647 | `	while( i < nArg ){` |
|  32529 | 1648 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1649 | `			/* Iterate throw array entries */` |
|  32529 | 1650 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1651 | `			/* Surface a callback allocation failure as a fatal */` |
|  32529 | 1652 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1653 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1654 | `			}` |
|  16267 | 1655 | `		}else{` |
|      - | 1656 | `			const char *zData;` |
|      - | 1657 | `			int nLen;` |
|      - | 1658 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1659 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1660 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1661 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1662 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1663 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1664 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1665 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1666 | `				}` |
|    ! 0 | 1667 | `			}` |
|      - | 1668 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1669 | `			if( nLen > 0 ){` |
|    ! 0 | 1670 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1671 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1672 | `				}` |
|    ! 0 | 1673 | `			}` |
|      - | 1674 | `		}` |
|  32529 | 1675 | `		i++;` |
|      5 | 1676 | `	}` |
|  32529 | 1677 | `	return PH7_OK;` |
|  16267 | 1678 | `}` |
|      - | 1679 | `/*` |
|      - | 1680 | ` * Symisc eXtension:` |
|      - | 1681 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1682 | ` * Purpose` |
|      - | 1683 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1684 | ` * Example:` |
|      - | 1685 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1686 | ` *   echo implode_recursive("/",$a);` |
|      - | 1687 | ` *   Will output` |
|      - | 1688 | ` *     usr/home/dean.` |
|      - | 1689 | ` *   While the standard implode would produce.` |
|      - | 1690 | ` *    usr/Array.` |
|      - | 1691 | ` * Parameter` |
|      - | 1692 | ` *  Refer to implode().` |
|      - | 1693 | ` * Return` |
|      - | 1694 | ` *  Refer to implode().` |
|      - | 1695 | ` */` |
|     12 | 1696 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1697 | `{` |
|      - | 1698 | `	struct implode_data imp_data;` |
|     13 | 1699 | `	int i = 1;` |
|     13 | 1700 | `	if( nArg < 1 ){` |
|      - | 1701 | `		/* Missing argument,return NULL */` |
|      3 | 1702 | `		ph7_result_null(pCtx);` |
|      3 | 1703 | `		return PH7_OK;` |
|      - | 1704 | `	}` |
|      - | 1705 | `	/* Prepare the implode context */` |
|     11 | 1706 | `	imp_data.pCtx = pCtx;` |
|     11 | 1707 | `	imp_data.bRecursive = 1;` |
|     11 | 1708 | `	imp_data.bFirst = 1;` |
|     11 | 1709 | `	imp_data.nRecCount = 0;` |
|     11 | 1710 | `	imp_data.rc = SXRET_OK;` |
|     11 | 1711 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1712 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1713 | `	}else{` |
|    ! 0 | 1714 | `		imp_data.zSep = 0;` |
|    ! 0 | 1715 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1716 | `		i = 0;` |
|      - | 1717 | `	}` |
|     11 | 1718 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1719 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1720 | `	}` |
|      - | 1721 | `	/* Start the 'join' process */` |
|     21 | 1722 | `	while( i < nArg ){` |
|     11 | 1723 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1724 | `			/* Iterate throw array entries */` |
|      3 | 1725 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1726 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 1727 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1728 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1729 | `			}` |
|      2 | 1730 | `		}else{` |
|      - | 1731 | `			const char *zData;` |
|      - | 1732 | `			int nLen;` |
|      - | 1733 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1734 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1735 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1736 | `			if( imp_data.bFirst ){` |
|      9 | 1737 | `				imp_data.bFirst = 0;` |
|      4 | 1738 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1739 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1740 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1741 | `				}` |
|    ! 0 | 1742 | `			}` |
|      - | 1743 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1744 | `			if( nLen > 0 ){` |
|      9 | 1745 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1746 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1747 | `				}` |
|      4 | 1748 | `			}` |
|      - | 1749 | `		}` |
|     11 | 1750 | `		i++;` |
|      1 | 1751 | `	}` |
|     11 | 1752 | `	return PH7_OK;` |
|      7 | 1753 | `}` |
|      - | 1754 | `/*` |
|      - | 1755 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1756 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1757 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1758 | ` * Parameters` |
|      - | 1759 | ` *  $delimiter` |
|      - | 1760 | ` *   The boundary string.` |
|      - | 1761 | ` * $string` |
|      - | 1762 | ` *   The input string.` |
|      - | 1763 | ` * $limit` |
|      - | 1764 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1765 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1766 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1767 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1768 | ` * Returns` |
|      - | 1769 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1770 | ` *  on boundaries formed by the delimiter.` |
|      - | 1771 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1772 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1773 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1774 | ` *  will be returned.` |
|      - | 1775 | ` * NOTE:` |
|      - | 1776 | ` *  Negative limit is not supported.` |
|      - | 1777 | ` */` |
|   6164 | 1778 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1779 | `{` |
|      - | 1780 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1781 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1782 | `	ph7_value *pArray;` |
|      - | 1783 | `	ph7_value *pValue;` |
|      - | 1784 | `	sxu32 nOfft;` |
|      - | 1785 | `	sxi32 rc;` |
|   6169 | 1786 | `	if( nArg < 2 ){` |
|      - | 1787 | `		/* Missing arguments,return FALSE */` |
|      9 | 1788 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1789 | `		return PH7_OK;` |
|      - | 1790 | `	}` |
|      - | 1791 | `	/* Extract the delimiter */` |
|   6161 | 1792 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6161 | 1793 | `	if( nDelim < 1 ){` |
|      - | 1794 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1795 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1796 | `		return PH7_OK;` |
|      - | 1797 | `	}` |
|      - | 1798 | `	/* Extract the string */` |
|   6159 | 1799 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6159 | 1800 | `	if( nStrlen < 1 ){` |
|      - | 1801 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 1802 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 1803 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 1804 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 1805 | `		if( pArrayTmp == 0 ){` |
|      - | 1806 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1807 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1808 | `			return PH7_OK;` |
|      - | 1809 | `		}` |
|      7 | 1810 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 1811 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 1812 | `			if( pValueTmp == 0 ){` |
|      - | 1813 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 1814 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 1815 | `				return PH7_OK;` |
|      - | 1816 | `			}` |
|      5 | 1817 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 1818 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 1819 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1820 | `			}` |
|      2 | 1821 | `		}` |
|      7 | 1822 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 1823 | `		return PH7_OK;` |
|      - | 1824 | `	}` |
|      - | 1825 | `	/* Point to the end of the string */` |
|   6153 | 1826 | `	zEnd = &zString[nStrlen];` |
|      - | 1827 | `	/* Create the array */` |
|   6153 | 1828 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6153 | 1829 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6153 | 1830 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1831 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1832 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1833 | `		return PH7_OK;` |
|      - | 1834 | `	}` |
|      - | 1835 | `	/* Set a defualt limit */` |
|   6153 | 1836 | `	iLimit = SXI32_HIGH;` |
|   6153 | 1837 | `	if( nArg > 2 ){` |
|     29 | 1838 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     29 | 1839 | `		if( iLimit < 0 ){` |
|      - | 1840 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 1841 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 1842 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 1843 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 1844 | `			int nTotal = 1,nKeep;` |
|     17 | 1845 | `			const char *zScan = zString;` |
|      - | 1846 | `			sxu32 nScanOfft;` |
|     57 | 1847 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 1848 | `				nTotal++;` |
|     41 | 1849 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 1850 | `			}` |
|     17 | 1851 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 1852 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 1853 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 1854 | `				/* Emit the next clean component */` |
|     23 | 1855 | `				zCur = &zString[nOfft];` |
|     23 | 1856 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 1857 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1858 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1859 | `				}` |
|     23 | 1860 | `				zString = &zCur[nDelim];` |
|     23 | 1861 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 1862 | `			}` |
|     17 | 1863 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 1864 | `			return PH7_OK;` |
|      - | 1865 | `		}` |
|     13 | 1866 | `		if( iLimit == 0 ){` |
|      5 | 1867 | `			iLimit = 1;` |
|      2 | 1868 | `		}` |
|     13 | 1869 | `		iLimit--;` |
|      6 | 1870 | `	}` |
|      - | 1871 | `	/* Start exploding */` |
|  71527 | 1872 | `	for(;;){` |
| 143059 | 1873 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 143059 | 1874 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1875 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6137 | 1876 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6137 | 1877 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1878 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1879 | `			}` |
|   6137 | 1880 | `			break;` |
|      - | 1881 | `		}` |
|      - | 1882 | `		/* Point to the desired offset */` |
| 136927 | 1883 | `		zCur = &zString[nOfft];` |
|      - | 1884 | `		/* Perform the store operation (may be empty) */` |
| 136927 | 1885 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 136927 | 1886 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1887 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1888 | `		}` |
|      - | 1889 | `		/* Point beyond the delimiter */` |
| 136927 | 1890 | `		zString = &zCur[nDelim];` |
|      - | 1891 | `		/* Reset the cursor */` |
| 136927 | 1892 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1893 | `	}` |
|      - | 1894 | `	/* Return the freshly created array */` |
|   6137 | 1895 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1896 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1897 | `	 * released as soon we return from this foregin function.` |
|      - | 1898 | `	 */` |
|   6137 | 1899 | `	return PH7_OK;` |
|   3087 | 1900 | `}` |
|      - | 1901 | `/*` |
|      - | 1902 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1903 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1904 | ` * Parameters` |
|      - | 1905 | ` *  $str` |
|      - | 1906 | ` *   The string that will be trimmed.` |
|      - | 1907 | ` * $charlist` |
|      - | 1908 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1909 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1910 | ` *   With .. you can specify a range of characters.` |
|      - | 1911 | ` * Returns.` |
|      - | 1912 | ` *  Thr processed string.` |
|      - | 1913 | ` * NOTE:` |
|      - | 1914 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1915 | ` */` |
|  14032 | 1916 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1917 | `{` |
|      - | 1918 | `	const char *zString;` |
|      - | 1919 | `	int nLen;` |
|  14037 | 1920 | `	if( nArg < 1 ){` |
|      - | 1921 | `		/* Missing arguments,return null */` |
|      3 | 1922 | `		ph7_result_null(pCtx);` |
|      3 | 1923 | `		return PH7_OK;` |
|      - | 1924 | `	}` |
|      - | 1925 | `	/* Extract the target string */` |
|  14035 | 1926 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14035 | 1927 | `	if( nLen < 1 ){` |
|      - | 1928 | `		/* Empty string,return */` |
|   1739 | 1929 | `		ph7_result_string(pCtx,"",0);` |
|   1739 | 1930 | `		return PH7_OK;` |
|      - | 1931 | `	}` |
|      - | 1932 | `	/* Start the trim process */` |
|  12301 | 1933 | `	if( nArg < 2 ){` |
|      - | 1934 | `		SyString sStr;` |
|      - | 1935 | `		/* Remove white spaces and NUL bytes */` |
|  12271 | 1936 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  30081 | 1937 | `		SyStringFullTrimSafe(&sStr);` |
|  12271 | 1938 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6138 | 1939 | `	}else{` |
|      - | 1940 | `		/* Char list */` |
|      - | 1941 | `		const char *zList;` |
|      - | 1942 | `		int nListlen;` |
|     33 | 1943 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 1944 | `		if( nListlen < 1 ){` |
|      - | 1945 | `			/* Return the string unchanged */` |
|      6 | 1946 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 1947 | `		}else{` |
|      - | 1948 | `			char aMask[256];` |
|     29 | 1949 | `			const char *zEnd = &zString[nLen];` |
|     29 | 1950 | `			const char *zCur = zString;` |
|     29 | 1951 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1952 | `			/* Left trim */` |
|     79 | 1953 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 1954 | `				zCur++;` |
|      3 | 1955 | `			}` |
|      - | 1956 | `			/* Right trim */` |
|     79 | 1957 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 1958 | `				zEnd--;` |
|      3 | 1959 | `			}` |
|     29 | 1960 | `			if( zCur >= zEnd ){` |
|      - | 1961 | `				/* Return the empty string */` |
|    ! 0 | 1962 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1963 | `			}else{` |
|     29 | 1964 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1965 | `			}` |
|      - | 1966 | `		}` |
|      - | 1967 | `	}` |
|  12301 | 1968 | `	return PH7_OK;` |
|   7021 | 1969 | `}` |
|      - | 1970 | `/*` |
|      - | 1971 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1972 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1973 | ` * Parameters` |
|      - | 1974 | ` *  $str` |
|      - | 1975 | ` *   The string that will be trimmed.` |
|      - | 1976 | ` * $charlist` |
|      - | 1977 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1978 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1979 | ` *   With .. you can specify a range of characters.` |
|      - | 1980 | ` * Returns.` |
|      - | 1981 | ` *  Thr processed string.` |
|      - | 1982 | ` * NOTE:` |
|      - | 1983 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1984 | ` */` |
|     30 | 1985 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1986 | `{` |
|      - | 1987 | `	const char *zString;` |
|      - | 1988 | `	int nLen;` |
|     33 | 1989 | `	if( nArg < 1 ){` |
|      - | 1990 | `		/* Missing arguments,return null */` |
|      3 | 1991 | `		ph7_result_null(pCtx);` |
|      3 | 1992 | `		return PH7_OK;` |
|      - | 1993 | `	}` |
|      - | 1994 | `	/* Extract the target string */` |
|     31 | 1995 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1996 | `	if( nLen < 1 ){` |
|      - | 1997 | `		/* Empty string,return */` |
|      5 | 1998 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1999 | `		return PH7_OK;` |
|      - | 2000 | `	}` |
|      - | 2001 | `	/* Start the trim process */` |
|     27 | 2002 | `	if( nArg < 2 ){` |
|      - | 2003 | `		SyString sStr;` |
|      - | 2004 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2005 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2006 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2007 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2008 | `	}else{` |
|      - | 2009 | `		/* Char list */` |
|      - | 2010 | `		const char *zList;` |
|      - | 2011 | `		int nListlen;` |
|     11 | 2012 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     11 | 2013 | `		if( nListlen < 1 ){` |
|      - | 2014 | `			/* Return the string unchanged */` |
|    ! 0 | 2015 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2016 | `		}else{` |
|      - | 2017 | `			char aMask[256];` |
|     11 | 2018 | `			const char *zEnd = &zString[nLen];` |
|     11 | 2019 | `			const char *zCur = zString;` |
|     11 | 2020 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2021 | `			/* Right trim */` |
|     29 | 2022 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 2023 | `				zEnd--;` |
|      2 | 2024 | `			}` |
|     11 | 2025 | `			if( zEnd <= zCur ){` |
|      - | 2026 | `				/* Return the empty string */` |
|    ! 0 | 2027 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2028 | `			}else{` |
|     11 | 2029 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2030 | `			}` |
|      - | 2031 | `		}` |
|      - | 2032 | `	}` |
|     27 | 2033 | `	return PH7_OK;` |
|     18 | 2034 | `}` |
|      - | 2035 | `/*` |
|      - | 2036 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2037 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2038 | ` * Parameters` |
|      - | 2039 | ` *  $str` |
|      - | 2040 | ` *   The string that will be trimmed.` |
|      - | 2041 | ` * $charlist` |
|      - | 2042 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2043 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2044 | ` *   With .. you can specify a range of characters.` |
|      - | 2045 | ` * Returns.` |
|      - | 2046 | ` *  Thr processed string.` |
|      - | 2047 | ` * NOTE:` |
|      - | 2048 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2049 | ` */` |
|     14 | 2050 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2051 | `{` |
|      - | 2052 | `	const char *zString;` |
|      - | 2053 | `	int nLen;` |
|     16 | 2054 | `	if( nArg < 1 ){` |
|      - | 2055 | `		/* Missing arguments,return null */` |
|      3 | 2056 | `		ph7_result_null(pCtx);` |
|      3 | 2057 | `		return PH7_OK;` |
|      - | 2058 | `	}` |
|      - | 2059 | `	/* Extract the target string */` |
|     14 | 2060 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     14 | 2061 | `	if( nLen < 1 ){` |
|      - | 2062 | `		/* Empty string,return */` |
|    ! 0 | 2063 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2064 | `		return PH7_OK;` |
|      - | 2065 | `	}` |
|      - | 2066 | `	/* Start the trim process */` |
|     14 | 2067 | `	if( nArg < 2 ){` |
|      - | 2068 | `		SyString sStr;` |
|      - | 2069 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2070 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2071 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2072 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2073 | `	}else{` |
|      - | 2074 | `		/* Char list */` |
|      - | 2075 | `		const char *zList;` |
|      - | 2076 | `		int nListlen;` |
|     12 | 2077 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     12 | 2078 | `		if( nListlen < 1 ){` |
|      - | 2079 | `			/* Return the string unchanged */` |
|      3 | 2080 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2081 | `		}else{` |
|      - | 2082 | `			char aMask[256];` |
|     10 | 2083 | `			const char *zEnd = &zString[nLen];` |
|     10 | 2084 | `			const char *zCur = zString;` |
|     10 | 2085 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2086 | `			/* Left trim */` |
|     28 | 2087 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     20 | 2088 | `				zCur++;` |
|      2 | 2089 | `			}` |
|     10 | 2090 | `			if( zCur >= zEnd ){` |
|      - | 2091 | `				/* Return the empty string */` |
|    ! 0 | 2092 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2093 | `			}else{` |
|     10 | 2094 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2095 | `			}` |
|      - | 2096 | `		}` |
|      - | 2097 | `	}` |
|     14 | 2098 | `	return PH7_OK;` |
|      9 | 2099 | `}` |
|      - | 2100 | `/*` |
|      - | 2101 | ` * string strtolower(string $str)` |
|      - | 2102 | ` *  Make a string lowercase.` |
|      - | 2103 | ` * Parameters` |
|      - | 2104 | ` *  $str` |
|      - | 2105 | ` *   The input string.` |
|      - | 2106 | ` * Returns.` |
|      - | 2107 | ` *  The lowercased string.` |
|      - | 2108 | ` */` |
|  32156 | 2109 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2110 | `{` |
|      - | 2111 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2112 | `	int nLen;` |
|  32161 | 2113 | `	if( nArg < 1 ){` |
|      - | 2114 | `		/* Missing arguments,return null */` |
|      3 | 2115 | `		ph7_result_null(pCtx);` |
|      3 | 2116 | `		return PH7_OK;` |
|      - | 2117 | `	}` |
|      - | 2118 | `	/* Extract the target string */` |
|  32159 | 2119 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  32159 | 2120 | `	if( nLen < 1 ){` |
|      - | 2121 | `		/* Empty string,return */` |
|      3 | 2122 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2123 | `		return PH7_OK;` |
|      - | 2124 | `	}` |
|      - | 2125 | `	/* Perform the requested operation */` |
|  32157 | 2126 | `	zEnd = &zString[nLen];` |
| 101302 | 2127 | `	for(;;){` |
| 202609 | 2128 | `		if( zString >= zEnd ){` |
|      - | 2129 | `			/* No more input,break immediately */` |
|  32157 | 2130 | `			break;` |
|      - | 2131 | `		}` |
| 170457 | 2132 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2133 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2134 | `			zCur = zString;` |
|    ! 0 | 2135 | `			zString++;` |
|    ! 0 | 2136 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2137 | `				zString++;` |
|    ! 0 | 2138 | `			}` |
|      - | 2139 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2140 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2141 | `		}else{` |
| 170457 | 2142 | `			int c = zString[0];` |
| 170457 | 2143 | `			if( SyisUpper(c) ){` |
| 170455 | 2144 | `				c = SyToLower(zString[0]);` |
|  85225 | 2145 | `			}` |
|      - | 2146 | `			/* Append character */` |
| 170457 | 2147 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2148 | `			/* Advance the cursor */` |
| 170457 | 2149 | `			zString++;` |
|      - | 2150 | `		}` |
|      5 | 2151 | `	}` |
|  32157 | 2152 | `	return PH7_OK;` |
|  16083 | 2153 | `}` |
|      - | 2154 | `/*` |
|      - | 2155 | ` * string strtolower(string $str)` |
|      - | 2156 | ` *  Make a string uppercase.` |
|      - | 2157 | ` * Parameters` |
|      - | 2158 | ` *  $str` |
|      - | 2159 | ` *   The input string.` |
|      - | 2160 | ` * Returns.` |
|      - | 2161 | ` *  The uppercased string.` |
|      - | 2162 | ` */` |
|     42 | 2163 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2164 | `{` |
|      - | 2165 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2166 | `	int nLen;` |
|     47 | 2167 | `	if( nArg < 1 ){` |
|      - | 2168 | `		/* Missing arguments,return null */` |
|      3 | 2169 | `		ph7_result_null(pCtx);` |
|      3 | 2170 | `		return PH7_OK;` |
|      - | 2171 | `	}` |
|      - | 2172 | `	/* Extract the target string */` |
|     45 | 2173 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     45 | 2174 | `	if( nLen < 1 ){` |
|      - | 2175 | `		/* Empty string,return */` |
|      3 | 2176 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2177 | `		return PH7_OK;` |
|      - | 2178 | `	}` |
|      - | 2179 | `	/* Perform the requested operation */` |
|     43 | 2180 | `	zEnd = &zString[nLen];` |
|     98 | 2181 | `	for(;;){` |
|    201 | 2182 | `		if( zString >= zEnd ){` |
|      - | 2183 | `			/* No more input,break immediately */` |
|     43 | 2184 | `			break;` |
|      - | 2185 | `		}` |
|    163 | 2186 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2187 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2188 | `			zCur = zString;` |
|    ! 0 | 2189 | `			zString++;` |
|    ! 0 | 2190 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2191 | `				zString++;` |
|    ! 0 | 2192 | `			}` |
|      - | 2193 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2194 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2195 | `		}else{` |
|    163 | 2196 | `			int c = zString[0];` |
|    163 | 2197 | `			if( SyisLower(c) ){` |
|    157 | 2198 | `				c = SyToUpper(zString[0]);` |
|     76 | 2199 | `			}` |
|      - | 2200 | `			/* Append character */` |
|    163 | 2201 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2202 | `			/* Advance the cursor */` |
|    163 | 2203 | `			zString++;` |
|      - | 2204 | `		}` |
|      5 | 2205 | `	}` |
|     43 | 2206 | `	return PH7_OK;` |
|     26 | 2207 | `}` |
|      - | 2208 | `/*` |
|      - | 2209 | ` * string ucfirst(string $str)` |
|      - | 2210 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2211 | ` *  character is alphabetic.` |
|      - | 2212 | ` * Parameters` |
|      - | 2213 | ` *  $str` |
|      - | 2214 | ` *   The input string.` |
|      - | 2215 | ` * Returns.` |
|      - | 2216 | ` *  The processed string.` |
|      - | 2217 | ` */` |
|      6 | 2218 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2219 | `{` |
|      - | 2220 | `	const char *zString,*zEnd;` |
|      - | 2221 | `	int nLen,c;` |
|      7 | 2222 | `	if( nArg < 1 ){` |
|      - | 2223 | `		/* Missing arguments,return null */` |
|      3 | 2224 | `		ph7_result_null(pCtx);` |
|      3 | 2225 | `		return PH7_OK;` |
|      - | 2226 | `	}` |
|      - | 2227 | `	/* Extract the target string */` |
|      5 | 2228 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2229 | `	if( nLen < 1 ){` |
|      - | 2230 | `		/* Empty string,return */` |
|      3 | 2231 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2232 | `		return PH7_OK;` |
|      - | 2233 | `	}` |
|      - | 2234 | `	/* Perform the requested operation */` |
|      3 | 2235 | `	zEnd = &zString[nLen];` |
|      3 | 2236 | `	c = zString[0];` |
|      3 | 2237 | `	if( SyisLower(c) ){` |
|      3 | 2238 | `		c = SyToUpper(c);` |
|      1 | 2239 | `	}` |
|      - | 2240 | `	/* Append the first character */` |
|      3 | 2241 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2242 | `	zString++;` |
|      3 | 2243 | `	if( zString < zEnd ){` |
|      - | 2244 | `		/* Append the rest of the input verbatim */` |
|      3 | 2245 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2246 | `	}` |
|      3 | 2247 | `	return PH7_OK;` |
|      4 | 2248 | `}` |
|      - | 2249 | `/*` |
|      - | 2250 | ` * string lcfirst(string $str)` |
|      - | 2251 | ` *  Make a string's first character lowercase.` |
|      - | 2252 | ` * Parameters` |
|      - | 2253 | ` *  $str` |
|      - | 2254 | ` *   The input string.` |
|      - | 2255 | ` * Returns.` |
|      - | 2256 | ` *  The processed string.` |
|      - | 2257 | ` */` |
|      6 | 2258 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2259 | `{` |
|      - | 2260 | `	const char *zString,*zEnd;` |
|      - | 2261 | `	int nLen,c;` |
|      7 | 2262 | `	if( nArg < 1 ){` |
|      - | 2263 | `		/* Missing arguments,return null */` |
|      3 | 2264 | `		ph7_result_null(pCtx);` |
|      3 | 2265 | `		return PH7_OK;` |
|      - | 2266 | `	}` |
|      - | 2267 | `	/* Extract the target string */` |
|      5 | 2268 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2269 | `	if( nLen < 1 ){` |
|      - | 2270 | `		/* Empty string,return */` |
|      3 | 2271 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2272 | `		return PH7_OK;` |
|      - | 2273 | `	}` |
|      - | 2274 | `	/* Perform the requested operation */` |
|      3 | 2275 | `	zEnd = &zString[nLen];` |
|      3 | 2276 | `	c = zString[0];` |
|      3 | 2277 | `	if( SyisUpper(c) ){` |
|      3 | 2278 | `		c = SyToLower(c);` |
|      1 | 2279 | `	}` |
|      - | 2280 | `	/* Append the first character */` |
|      3 | 2281 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2282 | `	zString++;` |
|      3 | 2283 | `	if( zString < zEnd ){` |
|      - | 2284 | `		/* Append the rest of the input verbatim */` |
|      3 | 2285 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2286 | `	}` |
|      3 | 2287 | `	return PH7_OK;` |
|      4 | 2288 | `}` |
|      - | 2289 | `/*` |
|      - | 2290 | ` * int ord(string $string)` |
|      - | 2291 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2292 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2293 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2294 | ` * Parameters` |
|      - | 2295 | ` *  $string` |
|      - | 2296 | ` *   The input string.` |
|      - | 2297 | ` * Returns` |
|      - | 2298 | ` *  The ASCII value as an integer.` |
|      - | 2299 | ` */` |
|     62 | 2300 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2301 | `{` |
|      - | 2302 | `	const char *zString;` |
|      - | 2303 | `	int nLen,c;` |
|      - | 2304 | `	/* PHP requires exactly one argument. */` |
|     65 | 2305 | `	if( nArg != 1 ){` |
|      8 | 2306 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2307 | `			"ArgumentCountError",` |
|      - | 2308 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2309 | `			nArg` |
|      - | 2310 | `			);` |
|      - | 2311 | `	}` |
|      - | 2312 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2313 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2314 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2315 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2316 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2317 | `			"of type string is deprecated"` |
|      - | 2318 | `			);` |
|      1 | 2319 | `	}` |
|      - | 2320 | `	/* Extract the target string */` |
|     59 | 2321 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2322 | `	if( nLen < 1 ){` |
|      - | 2323 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2324 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2325 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2326 | `			);` |
|      5 | 2327 | `		ph7_result_int(pCtx,0);` |
|      5 | 2328 | `		return PH7_OK;` |
|      - | 2329 | `	}` |
|      - | 2330 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2331 | `	if( nLen > 1 ){` |
|      7 | 2332 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2333 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2334 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2335 | `			);` |
|      3 | 2336 | `	}` |
|      - | 2337 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2338 | `	c = (unsigned char)zString[0];` |
|      - | 2339 | `	/* Return that value */` |
|     55 | 2340 | `	ph7_result_int(pCtx,c);` |
|     55 | 2341 | `	return PH7_OK;` |
|     34 | 2342 | `}` |
|      - | 2343 | `/*` |
|      - | 2344 | ` * string chr(int $codepoint)` |
|      - | 2345 | ` *  Returns a one-character string containing the character specified` |
|      - | 2346 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2347 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2348 | ` * Parameters` |
|      - | 2349 | ` *  $codepoint` |
|      - | 2350 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2351 | ` *   will be constrained to a single byte.` |
|      - | 2352 | ` * Returns` |
|      - | 2353 | ` *  A single-character string.` |
|      - | 2354 | ` */` |
|     48 | 2355 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2356 | `{` |
|      - | 2357 | `	int c;` |
|      - | 2358 | `	unsigned char ch;` |
|      - | 2359 | `	/* PHP requires exactly one argument. */` |
|     51 | 2360 | `	if( nArg != 1 ){` |
|      8 | 2361 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2362 | `			"ArgumentCountError",` |
|      - | 2363 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2364 | `			nArg` |
|      - | 2365 | `			);` |
|      - | 2366 | `	}` |
|      - | 2367 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2368 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2369 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2370 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     45 | 2371 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2372 | `		char zBuf[120];` |
|      4 | 2373 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2374 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2375 | `			ph7_value_to_double(apArg[0])` |
|      - | 2376 | `			);` |
|      3 | 2377 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2378 | `	}` |
|      - | 2379 | `	/* Extract the codepoint. */` |
|     45 | 2380 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2381 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2382 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2383 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2384 | `	 * name to avoid the API double-prefixing it. */` |
|     45 | 2385 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2386 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2387 | `			E_DEPRECATED,` |
|      - | 2388 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2389 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2390 | `			"The value used will be constrained using % 256"` |
|      - | 2391 | `			);` |
|      2 | 2392 | `	}` |
|      - | 2393 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2394 | `	 * when taking the address of a wider int. */` |
|     45 | 2395 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2396 | `	/* Return the specified character */` |
|     45 | 2397 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     45 | 2398 | `	return PH7_OK;` |
|     27 | 2399 | `}` |
|      - | 2400 | `/*` |
|      - | 2401 | ` * Binary to hex consumer callback.` |
|      - | 2402 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2403 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2404 | ` */` |
|   2330 | 2405 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2406 | `{` |
|      - | 2407 | `	/* Append hex chunk verbatim */` |
|   2331 | 2408 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   2331 | 2409 | `	return SXRET_OK;` |
|      1 | 2410 | `}` |
|      - | 2411 |  |
|      - | 2412 | `/*` |
|      - | 2413 | ` * string bin2hex(string $str)` |
|      - | 2414 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2415 | ` * Parameters` |
|      - | 2416 | ` *  $str` |
|      - | 2417 | ` *   The input string.` |
|      - | 2418 | ` * Returns.` |
|      - | 2419 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2420 | ` */` |
|     24 | 2421 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2422 | `{` |
|      - | 2423 | `	const char *zString;` |
|      - | 2424 | `	int nLen;` |
|      - | 2425 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     29 | 2426 | `	if( nArg != 1 ){` |
|      8 | 2427 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2428 | `			"ArgumentCountError",` |
|      - | 2429 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2430 | `			nArg` |
|      - | 2431 | `			);` |
|      - | 2432 | `	}` |
|      - | 2433 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2434 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2435 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2436 | `	 */` |
|     33 | 2437 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     16 | 2438 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2439 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2440 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2441 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2442 | `		)` |
|      - | 2443 | `	){` |
|      9 | 2444 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2445 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2446 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2447 | `			if( pInst && pInst->pClass ){` |
|      3 | 2448 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2449 | `			}` |
|      1 | 2450 | `		}` |
|     12 | 2451 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2452 | `			"TypeError",` |
|      - | 2453 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2454 | `			zType` |
|      - | 2455 | `			);` |
|      - | 2456 | `	}` |
|      - | 2457 | `	/* Extract the target string */` |
|     15 | 2458 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 2459 | `	if( nLen < 1 ){` |
|      - | 2460 | `		/* Empty string,return */` |
|      3 | 2461 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2462 | `		return PH7_OK;` |
|      - | 2463 | `	}` |
|      - | 2464 | `	/* Perform the requested operation */` |
|     13 | 2465 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|     13 | 2466 | `	return PH7_OK;` |
|     17 | 2467 | `}` |
|      - | 2468 |  |
|      - | 2469 | `/* Search callback signature */` |
|      - | 2470 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2471 | `/*` |
|      - | 2472 | ` * Case-insensitive pattern match.` |
|      - | 2473 | ` * Brute force is the default search method used here.` |
|      - | 2474 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2475 | ` * well for short/medium texts on modern hardware.` |
|      - | 2476 | ` */` |
|    118 | 2477 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2478 | `{` |
|    119 | 2479 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2480 | `	const char *zIn = (const char *)pText;` |
|    119 | 2481 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2482 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2483 | `	const char *zPtr,*zPtr2;` |
|      - | 2484 | `	int c,d;` |
|    119 | 2485 | `	if( iPatLen > nLen ){` |
|      - | 2486 | `		/* Don't bother processing */` |
|     33 | 2487 | `		return SXERR_NOTFOUND;` |
|      - | 2488 | `	}` |
|    242 | 2489 | `	for(;;){` |
|    485 | 2490 | `		if( zIn >= zEnd ){` |
|     47 | 2491 | `			break;` |
|      - | 2492 | `		}` |
|    439 | 2493 | `		c = SyToLower(zIn[0]);` |
|    439 | 2494 | `		d = SyToLower(zpIn[0]);` |
|    439 | 2495 | `		if( c == d ){` |
|     41 | 2496 | `			zPtr   = &zIn[1];` |
|     41 | 2497 | `			zPtr2  = &zpIn[1];` |
|     71 | 2498 | `			for(;;){` |
|    143 | 2499 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2500 | `					/* Pattern found */` |
|     41 | 2501 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2502 | `					return SXRET_OK;` |
|      - | 2503 | `				}` |
|    103 | 2504 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2505 | `					break;` |
|      - | 2506 | `				}` |
|    103 | 2507 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2508 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2509 | `				if( c != d ){` |
|    ! 0 | 2510 | `					break;` |
|      - | 2511 | `				}` |
|    103 | 2512 | `				zPtr++; zPtr2++;` |
|      1 | 2513 | `			}` |
|    ! 0 | 2514 | `		}` |
|    399 | 2515 | `		zIn++;` |
|      1 | 2516 | `	}` |
|      - | 2517 | `	/* Pattern not found */` |
|     47 | 2518 | `	return SXERR_NOTFOUND;` |
|     60 | 2519 | `}` |
|      - | 2520 | `/*` |
|      - | 2521 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2522 | ` *  Find the first occurrence of a string.` |
|      - | 2523 | ` * Parameters` |
|      - | 2524 | ` *  $haystack` |
|      - | 2525 | ` *   The input string.` |
|      - | 2526 | ` * $needle` |
|      - | 2527 | ` *   Search pattern (must be a string).` |
|      - | 2528 | ` * $before_needle` |
|      - | 2529 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2530 | ` *   of the needle (excluding the needle).` |
|      - | 2531 | ` * Return` |
|      - | 2532 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2533 | ` */` |
|     10 | 2534 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2535 | `{` |
|     11 | 2536 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2537 | `	const char *zBlob,*zPattern;` |
|      - | 2538 | `	int nLen,nPatLen;` |
|      - | 2539 | `	sxu32 nOfft;` |
|      - | 2540 | `	sxi32 rc;` |
|     11 | 2541 | `	if( nArg < 2 ){` |
|      - | 2542 | `		/* Missing arguments,return FALSE */` |
|      5 | 2543 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2544 | `		return PH7_OK;` |
|      - | 2545 | `	}` |
|      - | 2546 | `	/* Extract the needle and the haystack */` |
|      7 | 2547 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2548 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2549 | `	nOfft = 0; /* cc warning */` |
|      9 | 2550 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2551 | `		int before = 0;` |
|      - | 2552 | `		/* Perform the lookup */` |
|      5 | 2553 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2554 | `		if( rc != SXRET_OK ){` |
|      - | 2555 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2556 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2557 | `			return PH7_OK;` |
|      - | 2558 | `		}` |
|      - | 2559 | `		/* Return the portion of the string */` |
|      5 | 2560 | `		if( nArg > 2 ){` |
|      3 | 2561 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2562 | `		}` |
|      5 | 2563 | `		if( before ){` |
|      3 | 2564 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2565 | `		}else{` |
|      3 | 2566 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2567 | `		}` |
|      3 | 2568 | `	}else{` |
|      3 | 2569 | `		ph7_result_bool(pCtx,0);` |
|      - | 2570 | `	}` |
|      7 | 2571 | `	return PH7_OK;` |
|      6 | 2572 | `}` |
|      - | 2573 | `/*` |
|      - | 2574 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2575 | ` *  Case-insensitive strstr().` |
|      - | 2576 | ` * Parameters` |
|      - | 2577 | ` *  $haystack` |
|      - | 2578 | ` *   The input string.` |
|      - | 2579 | ` * $needle` |
|      - | 2580 | ` *   Search pattern (must be a string).` |
|      - | 2581 | ` * $before_needle` |
|      - | 2582 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2583 | ` *   of the needle (excluding the needle).` |
|      - | 2584 | ` * Return` |
|      - | 2585 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2586 | ` */` |
|      6 | 2587 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2588 | `{` |
|      7 | 2589 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2590 | `	const char *zBlob,*zPattern;` |
|      - | 2591 | `	int nLen,nPatLen;` |
|      - | 2592 | `	sxu32 nOfft;` |
|      - | 2593 | `	sxi32 rc;` |
|      7 | 2594 | `	if( nArg < 2 ){` |
|      - | 2595 | `		/* Missing arguments,return FALSE */` |
|      3 | 2596 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2597 | `		return PH7_OK;` |
|      - | 2598 | `	}` |
|      - | 2599 | `	/* Extract the needle and the haystack */` |
|      5 | 2600 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2601 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2602 | `	nOfft = 0; /* cc warning */` |
|      7 | 2603 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2604 | `		int before = 0;` |
|      - | 2605 | `		/* Perform the lookup */` |
|      5 | 2606 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2607 | `		if( rc != SXRET_OK ){` |
|      - | 2608 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2609 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2610 | `			return PH7_OK;` |
|      - | 2611 | `		}` |
|      - | 2612 | `		/* Return the portion of the string */` |
|      5 | 2613 | `		if( nArg > 2 ){` |
|      3 | 2614 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2615 | `		}` |
|      5 | 2616 | `		if( before ){` |
|      3 | 2617 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2618 | `		}else{` |
|      3 | 2619 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2620 | `		}` |
|      3 | 2621 | `	}else{` |
|    ! 0 | 2622 | `		ph7_result_bool(pCtx,0);` |
|      - | 2623 | `	}` |
|      5 | 2624 | `	return PH7_OK;` |
|      4 | 2625 | `}` |
|      - | 2626 | `/*` |
|      - | 2627 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2628 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2629 | ` * Parameters` |
|      - | 2630 | ` *  $haystack` |
|      - | 2631 | ` *   The input string.` |
|      - | 2632 | ` * $needle` |
|      - | 2633 | ` *   Search pattern (must be a string).` |
|      - | 2634 | ` * $offset` |
|      - | 2635 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2636 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2637 | ` *   of haystack.` |
|      - | 2638 | ` * Return` |
|      - | 2639 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2640 | ` */` |
|    124 | 2641 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2642 | `{` |
|    129 | 2643 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2644 | `	const char *zBlob,*zPattern;` |
|      - | 2645 | `	int nLen,nPatLen,nStart;` |
|      - | 2646 | `	sxu32 nOfft;` |
|      - | 2647 | `	sxi32 rc;` |
|    129 | 2648 | `	if( nArg < 2 ){` |
|      - | 2649 | `		/* Missing arguments,return FALSE */` |
|      7 | 2650 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2651 | `		return PH7_OK;` |
|      - | 2652 | `	}` |
|      - | 2653 | `	/* Extract the needle and the haystack */` |
|    123 | 2654 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    123 | 2655 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    123 | 2656 | `	nOfft = 0; /* cc warning */` |
|    123 | 2657 | `	nStart = 0;` |
|      - | 2658 | `	/* Peek the starting offset if available */` |
|    123 | 2659 | `	if( nArg > 2 ){` |
|    ! 0 | 2660 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2661 | `		if( nStart < 0 ){` |
|    ! 0 | 2662 | `			nStart = -nStart;` |
|    ! 0 | 2663 | `		}` |
|    ! 0 | 2664 | `		if( nStart >= nLen ){` |
|      - | 2665 | `			/* Invalid offset */` |
|    ! 0 | 2666 | `			nStart = 0;` |
|    ! 0 | 2667 | `		}else{` |
|    ! 0 | 2668 | `			zBlob += nStart;` |
|    ! 0 | 2669 | `			nLen -= nStart;` |
|      - | 2670 | `		}` |
|    ! 0 | 2671 | `	}` |
|    123 | 2672 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2673 | `		/* Perform the lookup */` |
|    121 | 2674 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    121 | 2675 | `		if( rc != SXRET_OK ){` |
|      - | 2676 | `			/* Pattern not found,return FALSE */` |
|     33 | 2677 | `			ph7_result_bool(pCtx,0);` |
|     33 | 2678 | `			return PH7_OK;` |
|      - | 2679 | `		}` |
|      - | 2680 | `		/* Return the pattern position */` |
|     90 | 2681 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     46 | 2682 | `	}else{` |
|      3 | 2683 | `		ph7_result_bool(pCtx,0);` |
|      - | 2684 | `	}` |
|     92 | 2685 | `	return PH7_OK;` |
|     67 | 2686 | `}` |
|      - | 2687 | `/*` |
|      - | 2688 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2689 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2690 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2691 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2692 | ` *` |
|      - | 2693 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2694 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2695 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2696 | ` *` |
|      - | 2697 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2698 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2699 | ` */` |
|    418 | 2700 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2701 | `	ph7_context *pCtx,` |
|      - | 2702 | `	ph7_value *pArg,` |
|      - | 2703 | `	const char *zFunc,` |
|      - | 2704 | `	int iArgNum,` |
|      - | 2705 | `	const char *zParamName,` |
|      - | 2706 | `	const char *zNullMsg,` |
|      - | 2707 | `	ph7_value *pTmp,` |
|      - | 2708 | `	const char **pzOut,` |
|      - | 2709 | `	int *pnOut` |
|      4 | 2710 | `){` |
|    422 | 2711 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2712 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2713 | `		*pzOut = "";` |
|     13 | 2714 | `		*pnOut = 0;` |
|     13 | 2715 | `		return PH7_OK;` |
|      - | 2716 | `	}` |
|    628 | 2717 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    388 | 2718 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2719 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2720 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2721 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2722 | `	    )` |
|      - | 2723 | `	){` |
|     34 | 2724 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2725 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2726 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2727 | `			if( pInst && pInst->pClass ){` |
|     13 | 2728 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2729 | `			}` |
|      6 | 2730 | `		}` |
|     49 | 2731 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2732 | `			"TypeError",` |
|      - | 2733 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2734 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2735 | `			);` |
|      - | 2736 | `	}` |
|    377 | 2737 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2738 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2739 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2740 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2741 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2742 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2743 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2744 | `		return PH7_OK;` |
|      - | 2745 | `	}` |
|    341 | 2746 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    341 | 2747 | `	return PH7_OK;` |
|    213 | 2748 | `}` |
|      - | 2749 | `/*` |
|      - | 2750 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2751 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2752 | ` * Return` |
|      - | 2753 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2754 | ` */` |
|     92 | 2755 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2756 | `{` |
|      - | 2757 | `	const char *zHaystack,*zNeedle;` |
|      - | 2758 | `	int nHayLen,nNeedleLen;` |
|      - | 2759 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2760 | `	sxi32 rc;` |
|     96 | 2761 | `	if( nArg != 2 ){` |
|     18 | 2762 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2763 | `			"ArgumentCountError",` |
|      - | 2764 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2765 | `			nArg` |
|      - | 2766 | `			);` |
|      - | 2767 | `	}` |
|     84 | 2768 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     84 | 2769 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     84 | 2770 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2771 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2772 | `		"of type string is deprecated",` |
|      - | 2773 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     84 | 2774 | `	if( rc != PH7_OK ) goto out;` |
|     77 | 2775 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2776 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2777 | `		"of type string is deprecated",` |
|      - | 2778 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     77 | 2779 | `	if( rc != PH7_OK ) goto out;` |
|     73 | 2780 | `	if( nNeedleLen < 1 ){` |
|     13 | 2781 | `		ph7_result_bool(pCtx,1);` |
|     67 | 2782 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2783 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2784 | `	}else{` |
|     79 | 2785 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     26 | 2786 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     53 | 2787 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2788 | `	}` |
|     73 | 2789 | `	rc = PH7_OK;` |
|     41 | 2790 | `out:` |
|     84 | 2791 | `	PH7_MemObjRelease(&sHayTmp);` |
|     84 | 2792 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     84 | 2793 | `	return rc;` |
|     50 | 2794 | `}` |
|      - | 2795 | `/*` |
|      - | 2796 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2797 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2798 | ` * Return` |
|      - | 2799 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2800 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2801 | ` */` |
|     78 | 2802 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2803 | `{` |
|      - | 2804 | `	const char *zHaystack,*zNeedle;` |
|      - | 2805 | `	int nHayLen,nNeedleLen;` |
|      - | 2806 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2807 | `	sxi32 rc;` |
|     82 | 2808 | `	if( nArg != 2 ){` |
|     18 | 2809 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2810 | `			"ArgumentCountError",` |
|      - | 2811 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2812 | `			nArg` |
|      - | 2813 | `			);` |
|      - | 2814 | `	}` |
|     70 | 2815 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2816 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2817 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2818 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2819 | `		"of type string is deprecated",` |
|      - | 2820 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2821 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2822 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2823 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2824 | `		"of type string is deprecated",` |
|      - | 2825 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2826 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2827 | `	if( nNeedleLen < 1 ){` |
|     13 | 2828 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2829 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2830 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2831 | `	}else{` |
|     58 | 2832 | `		ph7_result_bool(pCtx,` |
|     38 | 2833 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2834 | `	}` |
|     59 | 2835 | `	rc = PH7_OK;` |
|     34 | 2836 | `out:` |
|     70 | 2837 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2838 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2839 | `	return rc;` |
|     43 | 2840 | `}` |
|      - | 2841 | `/*` |
|      - | 2842 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2843 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2844 | ` * Return` |
|      - | 2845 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2846 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2847 | ` */` |
|     78 | 2848 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2849 | `{` |
|      - | 2850 | `	const char *zHaystack,*zNeedle;` |
|      - | 2851 | `	int nHayLen,nNeedleLen;` |
|      - | 2852 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2853 | `	sxi32 rc;` |
|     82 | 2854 | `	if( nArg != 2 ){` |
|     18 | 2855 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2856 | `			"ArgumentCountError",` |
|      - | 2857 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2858 | `			nArg` |
|      - | 2859 | `			);` |
|      - | 2860 | `	}` |
|     70 | 2861 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2862 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2863 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2864 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2865 | `		"of type string is deprecated",` |
|      - | 2866 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2867 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2868 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2869 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2870 | `		"of type string is deprecated",` |
|      - | 2871 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2872 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2873 | `	if( nNeedleLen < 1 ){` |
|     13 | 2874 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2875 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2876 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2877 | `	}else{` |
|     58 | 2878 | `		ph7_result_bool(pCtx,` |
|     38 | 2879 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2880 | `	}` |
|     59 | 2881 | `	rc = PH7_OK;` |
|     34 | 2882 | `out:` |
|     70 | 2883 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2884 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2885 | `	return rc;` |
|     43 | 2886 | `}` |
|      - | 2887 | `/*` |
|      - | 2888 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2889 | ` *  Case-insensitive strpos.` |
|      - | 2890 | ` * Parameters` |
|      - | 2891 | ` *  $haystack` |
|      - | 2892 | ` *   The input string.` |
|      - | 2893 | ` * $needle` |
|      - | 2894 | ` *   Search pattern (must be a string).` |
|      - | 2895 | ` * $offset` |
|      - | 2896 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2897 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2898 | ` *   of haystack.` |
|      - | 2899 | ` * Return` |
|      - | 2900 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2901 | ` */` |
|     18 | 2902 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2903 | `{` |
|     19 | 2904 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2905 | `	const char *zBlob,*zPattern;` |
|      - | 2906 | `	int nLen,nPatLen,nStart;` |
|      - | 2907 | `	sxu32 nOfft;` |
|      - | 2908 | `	sxi32 rc;` |
|     19 | 2909 | `	if( nArg < 2 ){` |
|      - | 2910 | `		/* Missing arguments,return FALSE */` |
|      3 | 2911 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2912 | `		return PH7_OK;` |
|      - | 2913 | `	}` |
|      - | 2914 | `	/* Extract the needle and the haystack */` |
|     17 | 2915 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2916 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2917 | `	nOfft = 0; /* cc warning */` |
|     17 | 2918 | `	nStart = 0;` |
|      - | 2919 | `	/* Peek the starting offset if available */` |
|     17 | 2920 | `	if( nArg > 2 ){` |
|      5 | 2921 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2922 | `		if( nStart < 0 ){` |
|      3 | 2923 | `			nStart = -nStart;` |
|      1 | 2924 | `		}` |
|      5 | 2925 | `		if( nStart >= nLen ){` |
|      - | 2926 | `			/* Invalid offset */` |
|    ! 0 | 2927 | `			nStart = 0;` |
|    ! 0 | 2928 | `		}else{` |
|      5 | 2929 | `			zBlob += nStart;` |
|      5 | 2930 | `			nLen -= nStart;` |
|      - | 2931 | `		}` |
|      2 | 2932 | `	}` |
|     17 | 2933 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2934 | `		/* Perform the lookup */` |
|     17 | 2935 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2936 | `		if( rc != SXRET_OK ){` |
|      - | 2937 | `			/* Pattern not found,return FALSE */` |
|      3 | 2938 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2939 | `			return PH7_OK;` |
|      - | 2940 | `		}` |
|      - | 2941 | `		/* Return the pattern position */` |
|     15 | 2942 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2943 | `	}else{` |
|    ! 0 | 2944 | `		ph7_result_bool(pCtx,0);` |
|      - | 2945 | `	}` |
|     15 | 2946 | `	return PH7_OK;` |
|     10 | 2947 | `}` |
|      - | 2948 | `/*` |
|      - | 2949 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2950 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2951 | ` * Parameters` |
|      - | 2952 | ` *  $haystack` |
|      - | 2953 | ` *   The input string.` |
|      - | 2954 | ` * $needle` |
|      - | 2955 | ` *   Search pattern (must be a string).` |
|      - | 2956 | ` * $offset` |
|      - | 2957 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2958 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2959 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2960 | ` * Return` |
|      - | 2961 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2962 | ` */` |
|     32 | 2963 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2964 | `{` |
|      - | 2965 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2966 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2967 | `	int nLen,nPatLen;` |
|      - | 2968 | `	sxu32 nOfft;` |
|      - | 2969 | `	sxi32 rc;` |
|     33 | 2970 | `	if( nArg < 2 ){` |
|      - | 2971 | `		/* Missing arguments,return FALSE */` |
|      3 | 2972 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2973 | `		return PH7_OK;` |
|      - | 2974 | `	}` |
|      - | 2975 | `	/* Extract the needle and the haystack */` |
|     31 | 2976 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2977 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2978 | `	/* Point to the end of the pattern */` |
|     31 | 2979 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2980 | `	zEnd = &zBlob[nLen];` |
|      - | 2981 | `	/* Save the starting posistion */` |
|     31 | 2982 | `	zStart = zBlob;` |
|     31 | 2983 | `	nOfft = 0; /* cc warning */` |
|      - | 2984 | `	/* Peek the starting offset if available */` |
|     31 | 2985 | `	if( nArg > 2 ){` |
|      - | 2986 | `		int nStart;` |
|     21 | 2987 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2988 | `		if( nStart < 0 ){` |
|     11 | 2989 | `			nStart = -nStart;` |
|     11 | 2990 | `			if( nStart >= nLen ){` |
|      - | 2991 | `				/* Invalid offset */` |
|      3 | 2992 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2993 | `				return PH7_OK;` |
|    ! 0 | 2994 | `			}else{` |
|      9 | 2995 | `				nLen -= nStart;` |
|      9 | 2996 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2997 | `				zEnd = &zBlob[nLen];` |
|      - | 2998 | `			}` |
|      5 | 2999 | `		}else{` |
|     11 | 3000 | `			if( nStart >= nLen ){` |
|      - | 3001 | `				/* Invalid offset */` |
|      5 | 3002 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3003 | `				return PH7_OK;` |
|    ! 0 | 3004 | `			}else{` |
|      7 | 3005 | `				zBlob += nStart;` |
|      7 | 3006 | `				nLen -= nStart;` |
|      - | 3007 | `			}` |
|      - | 3008 | `		}` |
|      7 | 3009 | `	}` |
|     25 | 3010 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3011 | `		/* Perform the lookup */` |
|     57 | 3012 | `		for(;;){` |
|    115 | 3013 | `			if( zBlob >= zPtr ){` |
|     11 | 3014 | `				break;` |
|      - | 3015 | `			}` |
|    105 | 3016 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3017 | `			if( rc == SXRET_OK ){` |
|      - | 3018 | `				/* Pattern found,return it's position */` |
|     13 | 3019 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3020 | `				return PH7_OK;` |
|      - | 3021 | `			}` |
|     93 | 3022 | `			zPtr--;` |
|      1 | 3023 | `		}` |
|      - | 3024 | `		/* Pattern not found,return FALSE */` |
|     11 | 3025 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3026 | `	}else{` |
|      3 | 3027 | `		ph7_result_bool(pCtx,0);` |
|      - | 3028 | `	}` |
|     13 | 3029 | `	return PH7_OK;` |
|     17 | 3030 | `}` |
|      - | 3031 | `/*` |
|      - | 3032 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3033 | ` *  Case-insensitive strrpos.` |
|      - | 3034 | ` * Parameters` |
|      - | 3035 | ` *  $haystack` |
|      - | 3036 | ` *   The input string.` |
|      - | 3037 | ` * $needle` |
|      - | 3038 | ` *   Search pattern (must be a string).` |
|      - | 3039 | ` * $offset` |
|      - | 3040 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3041 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3042 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3043 | ` * Return` |
|      - | 3044 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3045 | ` */` |
|     28 | 3046 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3047 | `{` |
|      - | 3048 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3049 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3050 | `	int nLen,nPatLen;` |
|      - | 3051 | `	sxu32 nOfft;` |
|      - | 3052 | `	sxi32 rc;` |
|     29 | 3053 | `	if( nArg < 2 ){` |
|      - | 3054 | `		/* Missing arguments,return FALSE */` |
|      3 | 3055 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3056 | `		return PH7_OK;` |
|      - | 3057 | `	}` |
|      - | 3058 | `	/* Extract the needle and the haystack */` |
|     27 | 3059 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3060 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3061 | `	/* Point to the end of the pattern */` |
|     27 | 3062 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3063 | `	zEnd = &zBlob[nLen];` |
|      - | 3064 | `	/* Save the starting posistion */` |
|     27 | 3065 | `	zStart = zBlob;` |
|     27 | 3066 | `	nOfft = 0; /* cc warning */` |
|      - | 3067 | `	/* Peek the starting offset if available */` |
|     27 | 3068 | `	if( nArg > 2 ){` |
|      - | 3069 | `		int nStart;` |
|     15 | 3070 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3071 | `		if( nStart < 0 ){` |
|      7 | 3072 | `			nStart = -nStart;` |
|      7 | 3073 | `			if( nStart >= nLen ){` |
|      - | 3074 | `				/* Invalid offset */` |
|      3 | 3075 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3076 | `				return PH7_OK;` |
|    ! 0 | 3077 | `			}else{` |
|      5 | 3078 | `				nLen -= nStart;` |
|      5 | 3079 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3080 | `				zEnd = &zBlob[nLen];` |
|      - | 3081 | `			}` |
|      3 | 3082 | `		}else{` |
|      9 | 3083 | `			if( nStart >= nLen ){` |
|      - | 3084 | `				/* Invalid offset */` |
|      5 | 3085 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3086 | `				return PH7_OK;` |
|    ! 0 | 3087 | `			}else{` |
|      5 | 3088 | `				zBlob += nStart;` |
|      5 | 3089 | `				nLen -= nStart;` |
|      - | 3090 | `			}` |
|      - | 3091 | `		}` |
|      4 | 3092 | `	}` |
|     21 | 3093 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3094 | `		/* Perform the lookup */` |
|     44 | 3095 | `		for(;;){` |
|     89 | 3096 | `			if( zBlob >= zPtr ){` |
|      9 | 3097 | `				break;` |
|      - | 3098 | `			}` |
|     81 | 3099 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3100 | `			if( rc == SXRET_OK ){` |
|      - | 3101 | `				/* Pattern found,return it's position */` |
|     11 | 3102 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3103 | `				return PH7_OK;` |
|      - | 3104 | `			}` |
|     71 | 3105 | `			zPtr--;` |
|      1 | 3106 | `		}` |
|      - | 3107 | `		/* Pattern not found,return FALSE */` |
|      9 | 3108 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3109 | `	}else{` |
|      3 | 3110 | `		ph7_result_bool(pCtx,0);` |
|      - | 3111 | `	}` |
|     11 | 3112 | `	return PH7_OK;` |
|     15 | 3113 | `}` |
|      - | 3114 | `/*` |
|      - | 3115 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3116 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3117 | ` * Parameters` |
|      - | 3118 | ` *  $haystack` |
|      - | 3119 | ` *   The input string.` |
|      - | 3120 | ` * $needle` |
|      - | 3121 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3122 | ` *  This behavior is different from that of strstr().` |
|      - | 3123 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3124 | ` *  as the ordinal value of a character.` |
|      - | 3125 | ` * Return` |
|      - | 3126 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3127 | ` */` |
|     24 | 3128 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3129 | `{` |
|      - | 3130 | `	const char *zBlob;` |
|      - | 3131 | `	int nLen,c;` |
|     25 | 3132 | `	if( nArg < 2 ){` |
|      - | 3133 | `		/* Missing arguments,return FALSE */` |
|      3 | 3134 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3135 | `		return PH7_OK;` |
|      - | 3136 | `	}` |
|      - | 3137 | `	/* Extract the haystack */` |
|     23 | 3138 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3139 | `	c = 0; /* cc warning */` |
|     23 | 3140 | `	if( nLen > 0 ){` |
|      - | 3141 | `		sxu32 nOfft;` |
|      - | 3142 | `		sxi32 rc;` |
|     21 | 3143 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3144 | `			const char *zPattern;` |
|     11 | 3145 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3146 | `														 * for NULL pointer.` |
|      - | 3147 | `														 */` |
|     11 | 3148 | `			c = zPattern[0];` |
|      6 | 3149 | `		}else{` |
|      - | 3150 | `			/* Int cast */` |
|     11 | 3151 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3152 | `		}` |
|      - | 3153 | `		/* Perform the lookup */` |
|     21 | 3154 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3155 | `		if( rc != SXRET_OK ){` |
|      - | 3156 | `			/* No such entry,return FALSE */` |
|      7 | 3157 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3158 | `			return PH7_OK;` |
|      - | 3159 | `		}` |
|      - | 3160 | `		/* Return the string portion */` |
|     15 | 3161 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3162 | `	}else{` |
|      3 | 3163 | `		ph7_result_bool(pCtx,0);` |
|      - | 3164 | `	}` |
|     17 | 3165 | `	return PH7_OK;` |
|     13 | 3166 | `}` |
|      - | 3167 | `/*` |
|      - | 3168 | ` * string strrev(string $string)` |
|      - | 3169 | ` *  Reverse a string.` |
|      - | 3170 | ` * Parameters` |
|      - | 3171 | ` *  $string` |
|      - | 3172 | ` *   String to be reversed.` |
|      - | 3173 | ` * Return` |
|      - | 3174 | ` *  The reversed string.` |
|      - | 3175 | ` */` |
|      4 | 3176 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3177 | `{` |
|      - | 3178 | `	const char *zIn,*zEnd;` |
|      - | 3179 | `	int nLen,c;` |
|      5 | 3180 | `	if( nArg < 1 ){` |
|      - | 3181 | `		/* Missing arguments,return NULL */` |
|      3 | 3182 | `		ph7_result_null(pCtx);` |
|      3 | 3183 | `		return PH7_OK;` |
|      - | 3184 | `	}` |
|      - | 3185 | `	/* Extract the target string */` |
|      3 | 3186 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3187 | `	if( nLen < 1 ){` |
|      - | 3188 | `		/* Empty string Return null */` |
|    ! 0 | 3189 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3190 | `		return PH7_OK;` |
|      - | 3191 | `	}` |
|      - | 3192 | `	/* Perform the requested operation */` |
|      3 | 3193 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3194 | `	for(;;){` |
|      9 | 3195 | `		if( zEnd < zIn ){` |
|      - | 3196 | `			/* No more input to process */` |
|      3 | 3197 | `			break;` |
|      - | 3198 | `		}` |
|      - | 3199 | `		/* Append current character */` |
|      7 | 3200 | `		c = zEnd[0];` |
|      7 | 3201 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3202 | `		zEnd--;` |
|      1 | 3203 | `	}` |
|      3 | 3204 | `	return PH7_OK;` |
|      3 | 3205 | `}` |
|      - | 3206 | `/*` |
|      - | 3207 | ` * string ucwords(string $string)` |
|      - | 3208 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3209 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3210 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3211 | ` * Parameters` |
|      - | 3212 | ` *  $string` |
|      - | 3213 | ` *   The input string.` |
|      - | 3214 | ` * Return` |
|      - | 3215 | ` *  The modified string..` |
|      - | 3216 | ` */` |
|     14 | 3217 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3218 | `{` |
|      - | 3219 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3220 | `	int nLen,c;` |
|     15 | 3221 | `	if( nArg < 1 ){` |
|      - | 3222 | `		/* Missing arguments,return NULL */` |
|      3 | 3223 | `		ph7_result_null(pCtx);` |
|      3 | 3224 | `		return PH7_OK;` |
|      - | 3225 | `	}` |
|      - | 3226 | `	/* Extract the target string */` |
|     13 | 3227 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3228 | `	if( nLen < 1 ){` |
|      - | 3229 | `		/* Empty string – match PHP semantics */` |
|      3 | 3230 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3231 | `		return PH7_OK;` |
|      - | 3232 | `	}` |
|      - | 3233 | `	/* Perform the requested operation */` |
|     11 | 3234 | `	zEnd = &zIn[nLen];` |
|     21 | 3235 | `	for(;;){` |
|      - | 3236 | `		/* Jump leading white spaces */` |
|     43 | 3237 | `		zCur = zIn;` |
|     65 | 3238 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3239 | `			zIn++;` |
|      1 | 3240 | `		}` |
|     43 | 3241 | `		if( zCur < zIn ){` |
|      - | 3242 | `			/* Append white space stream */` |
|     23 | 3243 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3244 | `		}` |
|     43 | 3245 | `		if( zIn >= zEnd ){` |
|      - | 3246 | `			/* No more input to process */` |
|     11 | 3247 | `			break;` |
|      - | 3248 | `		}` |
|     33 | 3249 | `		c = zIn[0];` |
|     33 | 3250 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3251 | `			c = SyToUpper(c);` |
|     14 | 3252 | `		}` |
|      - | 3253 | `		/* Append the upper-cased character */` |
|     33 | 3254 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3255 | `		zIn++;` |
|     33 | 3256 | `		zCur = zIn;` |
|      - | 3257 | `		/* Append the word varbatim */` |
|    149 | 3258 | `		while( zIn < zEnd ){` |
|    139 | 3259 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3260 | `				/* UTF-8 stream */` |
|    ! 0 | 3261 | `				zIn++;` |
|    ! 0 | 3262 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3263 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3264 | `				zIn++;` |
|     59 | 3265 | `			}else{` |
|     23 | 3266 | `				break;` |
|      - | 3267 | `			}` |
|      1 | 3268 | `		}` |
|     33 | 3269 | `		if( zCur < zIn ){` |
|     33 | 3270 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3271 | `		}` |
|      1 | 3272 | `	}` |
|     11 | 3273 | `	return PH7_OK;` |
|      8 | 3274 | `}` |
|      - | 3275 | `/*` |
|      - | 3276 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3277 | ` *  Returns input repeated multiplier times.` |
|      - | 3278 | ` * Parameters` |
|      - | 3279 | ` *  $string` |
|      - | 3280 | ` *   String to be repeated.` |
|      - | 3281 | ` * $multiplier` |
|      - | 3282 | ` *  Number of time the input string should be repeated.` |
|      - | 3283 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3284 | ` *  to 0, the function will return an empty string.` |
|      - | 3285 | ` * Return` |
|      - | 3286 | ` *  The repeated string.` |
|      - | 3287 | ` */` |
|  20226 | 3288 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3289 | `{` |
|      - | 3290 | `	const char *zIn;` |
|      - | 3291 | `	int nLen,nMul;` |
|      - | 3292 | `	int rc;` |
|  20227 | 3293 | `	if( nArg < 2 ){` |
|      - | 3294 | `		/* Missing arguments,return NULL */` |
|      3 | 3295 | `		ph7_result_null(pCtx);` |
|      3 | 3296 | `		return PH7_OK;` |
|      - | 3297 | `	}` |
|      - | 3298 | `	/* Extract the target string */` |
|  20225 | 3299 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20225 | 3300 | `	if( nLen < 1 ){` |
|      - | 3301 | `		/* Empty string.Return null */` |
|    ! 0 | 3302 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3303 | `		return PH7_OK;` |
|      - | 3304 | `	}` |
|      - | 3305 | `	/* Extract the multiplier */` |
|  20225 | 3306 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20225 | 3307 | `	if( nMul < 1 ){` |
|      - | 3308 | `		/* Return the empty string */` |
|      3 | 3309 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3310 | `		return PH7_OK;` |
|      - | 3311 | `	}` |
|      - | 3312 | `	/* Perform the requested operation */` |
| 120878 | 3313 | `	for(;;){` |
| 241757 | 3314 | `		if( !nMul ){` |
|  20223 | 3315 | `			break;` |
|      - | 3316 | `		}` |
|      - | 3317 | `		/* Append the copy */` |
| 221535 | 3318 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 221535 | 3319 | `		if( rc != PH7_OK ){` |
|      - | 3320 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3321 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3322 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3323 | `		}` |
| 221535 | 3324 | `		nMul--;` |
|      1 | 3325 | `	}` |
|  20223 | 3326 | `	return PH7_OK;` |
|  10114 | 3327 | `}` |
|      - | 3328 | `/*` |
|      - | 3329 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3330 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3331 | ` * Parameters` |
|      - | 3332 | ` *  $string` |
|      - | 3333 | ` *   The input string.` |
|      - | 3334 | ` * $is_xhtml` |
|      - | 3335 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3336 | ` * Return` |
|      - | 3337 | ` *  The processed string.` |
|      - | 3338 | ` */` |
|      6 | 3339 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3340 | `{` |
|      - | 3341 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3342 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3343 | `	int nLen;` |
|      7 | 3344 | `	if( nArg < 1 ){` |
|      - | 3345 | `		/* Missing arguments,return the empty string */` |
|      3 | 3346 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3347 | `		return PH7_OK;` |
|      - | 3348 | `	}` |
|      - | 3349 | `	/* Extract the target string */` |
|      5 | 3350 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3351 | `	if( nLen < 1 ){` |
|      - | 3352 | `		/* Empty string,return null */` |
|    ! 0 | 3353 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3354 | `		return PH7_OK;` |
|      - | 3355 | `	}` |
|      5 | 3356 | `	if( nArg > 1 ){` |
|      3 | 3357 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3358 | `	}` |
|      5 | 3359 | `	zEnd = &zIn[nLen];` |
|      - | 3360 | `	/* Perform the requested operation */` |
|      4 | 3361 | `	for(;;){` |
|      9 | 3362 | `		zCur = zIn;` |
|      - | 3363 | `		/* Delimit the string */` |
|     21 | 3364 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3365 | `			zIn++;` |
|      1 | 3366 | `		}` |
|      9 | 3367 | `		if( zCur < zIn ){` |
|      - | 3368 | `			/* Output chunk verbatim */` |
|      9 | 3369 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3370 | `		}` |
|      9 | 3371 | `		if( zIn >= zEnd ){` |
|      - | 3372 | `			/* No more input to process */` |
|      5 | 3373 | `			break;` |
|      - | 3374 | `		}` |
|      - | 3375 | `		/* Output the HTML line break */` |
|      - | 3376 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3377 | `		if( is_xhtml ){` |
|      3 | 3378 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3379 | `		}else{` |
|      3 | 3380 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3381 | `		}` |
|      5 | 3382 | `		zCur = zIn;` |
|      - | 3383 | `		/* Append trailing line */` |
|     11 | 3384 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3385 | `			zIn++;` |
|      1 | 3386 | `		}` |
|      5 | 3387 | `		if( zCur < zIn ){` |
|      - | 3388 | `			/* Output chunk verbatim */` |
|      5 | 3389 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3390 | `		}` |
|      1 | 3391 | `	}` |
|      5 | 3392 | `	return PH7_OK;` |
|      4 | 3393 | `}` |
|      - | 3394 | `/*` |
|      - | 3395 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3396 | ` *  According to the PHP reference manual.` |
|      - | 3397 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3398 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3399 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3400 | ` * This applies to both sprintf() and printf().` |
|      - | 3401 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3402 | ` * or more of these elements, in order:` |
|      - | 3403 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3404 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3405 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3406 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3407 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3408 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3409 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3410 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3411 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3412 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3413 | ` *   should result in.` |
|      - | 3414 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3415 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3416 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3417 | ` *   limit to the string.` |
|      - | 3418 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3419 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3420 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3421 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3422 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3423 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3424 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3425 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3426 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3427 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3428 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3429 | ` *       g - shorter of %e and %f.` |
|      - | 3430 | ` *       G - shorter of %E and %f.` |
|      - | 3431 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3432 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3433 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3434 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3435 | ` */` |
|      - | 3436 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3437 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3438 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3439 | `/*` |
|      - | 3440 | `** Conversion types fall into various categories as defined by the` |
|      - | 3441 | `** following enumeration.` |
|      - | 3442 | `*/` |
|      - | 3443 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3444 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3445 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3446 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3447 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3448 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3449 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3450 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3451 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3452 |  |
|      - | 3453 | `/*` |
|      - | 3454 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3455 | `*/` |
|      - | 3456 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3457 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3458 | `/*` |
|      - | 3459 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3460 | `** by an instance of the following structure` |
|      - | 3461 | `*/` |
|      - | 3462 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3463 | `struct ph7_fmt_info` |
|      - | 3464 | `{` |
|      - | 3465 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3466 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3467 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3468 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3469 | `  char *charset; /* The character set for conversion */` |
|      - | 3470 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3471 | `};` |
|      - | 3472 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3473 | `/*` |
|      - | 3474 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3475 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3476 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3477 | `**` |
|      - | 3478 | `** Example:` |
|      - | 3479 | `**     input:     *val = 3.14159` |
|      - | 3480 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3481 | `**` |
|      - | 3482 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3483 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3484 | `** always returned.` |
|      - | 3485 | `*/` |
|    422 | 3486 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3487 | `{` |
|      - | 3488 | `  sxlongreal d;` |
|      - | 3489 | `  int digit;` |
|      - | 3490 |  |
|    423 | 3491 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3492 | `	  return '0';` |
|      - | 3493 | `  }` |
|    423 | 3494 | `  digit = (int)*val;` |
|    423 | 3495 | `  d = digit;` |
|    423 | 3496 | `   *val = (*val - d)*10.0;` |
|    423 | 3497 | `  return digit + '0' ;` |
|    212 | 3498 | `}` |
|      - | 3499 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3500 | `/*` |
|      - | 3501 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3502 | ` * used conversion types first.` |
|      - | 3503 | ` */` |
|      - | 3504 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3505 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3506 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3507 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3508 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3509 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3510 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3511 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3512 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3513 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3514 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3515 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3516 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3517 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3518 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3519 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3520 | `};` |
|      - | 3521 | `/*` |
|      - | 3522 | ` * Format a given string.` |
|      - | 3523 | ` * The root program.  All variations call this core.` |
|      - | 3524 | ` * INPUTS:` |
|      - | 3525 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3526 | ` *            1. A pointer to the call context.` |
|      - | 3527 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3528 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3529 | ` *            3. An integer number of characters to be output.` |
|      - | 3530 | ` *               (Note: This number might be zero.)` |
|      - | 3531 | ` *            4. Upper layer private data.` |
|      - | 3532 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3533 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3534 | ` */` |
|    260 | 3535 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3536 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3537 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3538 | `	const char *zIn,    /* Format string */` |
|      - | 3539 | `	int nByte,          /* Format string length */` |
|      - | 3540 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3541 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3542 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3543 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3544 | `	)` |
|      1 | 3545 | `{` |
|    261 | 3546 | `	char spaces[] = "                                                  ";` |
|      - | 3547 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    261 | 3548 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3549 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3550 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3551 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3552 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3553 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3554 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3555 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3556 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3557 | `	ph7_int64 iVal;` |
|      - | 3558 | `	int precision;           /* Precision of the current field */` |
|      - | 3559 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3560 | `	int c,rc,n;` |
|      - | 3561 | `	int length;              /* Length of the field */` |
|      - | 3562 | `	int prefix;` |
|      - | 3563 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3564 | `	int width;               /* Width of the current field */` |
|      - | 3565 | `	int idx;` |
|    261 | 3566 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3567 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3568 | `	/* Start the format process */` |
|    380 | 3569 | `	for(;;){` |
|    761 | 3570 | `		zCur = zIn;` |
|   2785 | 3571 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2025 | 3572 | `			zIn++;` |
|      1 | 3573 | `		}` |
|    761 | 3574 | `		if( zCur < zIn ){` |
|      - | 3575 | `			/* Consume chunk verbatim */` |
|    539 | 3576 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    539 | 3577 | `			if( rc != SXRET_OK ){` |
|      - | 3578 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3579 | `				break;` |
|      - | 3580 | `			}` |
|    269 | 3581 | `		}` |
|    761 | 3582 | `		if( zIn >= zEnd ){` |
|      - | 3583 | `			/* No more input to process,break immediately */` |
|    259 | 3584 | `			break;` |
|      - | 3585 | `		}` |
|      - | 3586 | `		/* Find out what flags are present */` |
|    503 | 3587 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    502 | 3588 | `			flag_alternateform = flag_zeropad = 0;` |
|    503 | 3589 | `		zIn++; /* Jump the precent sign */` |
|    251 | 3590 | `		do{` |
|    535 | 3591 | `			c = zIn[0];` |
|    535 | 3592 | `			switch( c ){` |
|      9 | 3593 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3594 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3595 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3596 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3597 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3598 | `			case '\'':` |
|    ! 0 | 3599 | `				zIn++;` |
|    ! 0 | 3600 | `				if( zIn < zEnd ){` |
|      - | 3601 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3602 | `					c = zIn[0];` |
|    ! 0 | 3603 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3604 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3605 | `					}` |
|    ! 0 | 3606 | `					c = 0;` |
|    ! 0 | 3607 | `				}` |
|    ! 0 | 3608 | `				break;` |
|    502 | 3609 | `			default:                                       break;` |
|      - | 3610 | `			}` |
|    535 | 3611 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3612 | `		/* Get the field width */` |
|    503 | 3613 | `		width = 0;` |
|    788 | 3614 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3615 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3616 | `			zIn++;` |
|      1 | 3617 | `		}` |
|    503 | 3618 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3619 | `			/* Position specifer */` |
|    ! 0 | 3620 | `			if( width > 0 ){` |
|    ! 0 | 3621 | `				n = width;` |
|    ! 0 | 3622 | `				if( vf && n > 0 ){` |
|    ! 0 | 3623 | `					n--;` |
|    ! 0 | 3624 | `				}` |
|    ! 0 | 3625 | `			}` |
|    ! 0 | 3626 | `			zIn++;` |
|    ! 0 | 3627 | `			width = 0;` |
|    ! 0 | 3628 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3629 | `				flag_zeropad = 1;` |
|    ! 0 | 3630 | `				zIn++;` |
|    ! 0 | 3631 | `			}` |
|    ! 0 | 3632 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3633 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3634 | `				zIn++;` |
|    ! 0 | 3635 | `			}` |
|    ! 0 | 3636 | `		}` |
|    503 | 3637 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3638 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3639 | `		}` |
|      - | 3640 | `		/* Get the precision */` |
|    503 | 3641 | `		precision = -1;` |
|    503 | 3642 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3643 | `			precision = 0;` |
|     59 | 3644 | `			zIn++;` |
|    150 | 3645 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3646 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3647 | `				zIn++;` |
|      1 | 3648 | `			}` |
|     29 | 3649 | `		}` |
|    503 | 3650 | `		if( zIn >= zEnd ){` |
|      - | 3651 | `			/* No more input */` |
|      3 | 3652 | `			break;` |
|      - | 3653 | `		}` |
|      - | 3654 | `		/* Fetch the info entry for the field */` |
|    501 | 3655 | `		pInfo = 0;` |
|    501 | 3656 | `		xtype = PH7_FMT_ERROR;` |
|    501 | 3657 | `		c = zIn[0];` |
|    501 | 3658 | `		zIn++; /* Jump the format specifer */` |
|   1439 | 3659 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   1437 | 3660 | `			if( c==aFmt[idx].fmttype ){` |
|    499 | 3661 | `				pInfo = &aFmt[idx];` |
|    499 | 3662 | `				xtype = pInfo->type;` |
|    499 | 3663 | `				break;` |
|      - | 3664 | `			}` |
|    470 | 3665 | `		}` |
|    501 | 3666 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    501 | 3667 | `		length = 0;` |
|      - | 3668 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3669 | `		 /*` |
|      - | 3670 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3671 | `		  **` |
|      - | 3672 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3673 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3674 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3675 | `		  **                               field width was negative.` |
|      - | 3676 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3677 | `		  **                               the conversion character.` |
|      - | 3678 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3679 | `		  **   width                       The specified field width.  This is` |
|      - | 3680 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3681 | `		  **   precision                   The specified precision.  The default` |
|      - | 3682 | `		  **                               is -1.` |
|      - | 3683 | `		  */` |
|    501 | 3684 | `		switch(xtype){` |
|    ! 0 | 3685 | `		case PH7_FMT_PERCENT:` |
|      - | 3686 | `			/* A literal percent character */` |
|    ! 0 | 3687 | `			zWorker[0] = '%';` |
|    ! 0 | 3688 | `			length = (int)sizeof(char);` |
|    ! 0 | 3689 | `			break;` |
|      3 | 3690 | `		case PH7_FMT_CHARX:` |
|      - | 3691 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3692 | `			 * with that ASCII value` |
|      - | 3693 | `			 */` |
|      7 | 3694 | `			pArg = NEXT_ARG;` |
|      7 | 3695 | `			if( pArg == 0 ){` |
|      3 | 3696 | `				c = 0;` |
|      2 | 3697 | `			}else{` |
|      5 | 3698 | `				c = ph7_value_to_int(pArg);` |
|      - | 3699 | `			}` |
|      - | 3700 | `			/* NUL byte is an acceptable value */` |
|      7 | 3701 | `			zWorker[0] = (char)c;` |
|      7 | 3702 | `			length = (int)sizeof(char);` |
|      7 | 3703 | `			break;` |
|    159 | 3704 | `		case PH7_FMT_STRING:` |
|      - | 3705 | `			/* the argument is treated as and presented as a string */` |
|    319 | 3706 | `			pArg = NEXT_ARG;` |
|    319 | 3707 | `			if( pArg == 0 ){` |
|    ! 0 | 3708 | `				length = 0;` |
|    ! 0 | 3709 | `			}else{` |
|    319 | 3710 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3711 | `			}` |
|    319 | 3712 | `			if( length < 1 ){` |
|    ! 0 | 3713 | `				zBuf = " ";` |
|    ! 0 | 3714 | `				length = (int)sizeof(char);` |
|    ! 0 | 3715 | `			}` |
|    319 | 3716 | `			if( precision>=0 && precision<length ){` |
|      3 | 3717 | `				length = precision;` |
|      1 | 3718 | `			}` |
|    319 | 3719 | `			if( flag_zeropad ){` |
|      - | 3720 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3721 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3722 | `					spaces[idx] = '0';` |
|    ! 0 | 3723 | `				}` |
|    ! 0 | 3724 | `			}` |
|    319 | 3725 | `			break;` |
|     59 | 3726 | `		case PH7_FMT_RADIX:` |
|    119 | 3727 | `			pArg = NEXT_ARG;` |
|    119 | 3728 | `			if( pArg == 0 ){` |
|    ! 0 | 3729 | `				iVal = 0;` |
|    ! 0 | 3730 | `			}else{` |
|    119 | 3731 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3732 | `			}` |
|      - | 3733 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 3734 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3735 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3736 | `			}` |
|      - | 3737 | `#if 1` |
|      - | 3738 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3739 | `        ** I think this is stupid.*/` |
|    119 | 3740 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3741 | `#else` |
|      - | 3742 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3743 | `        ** but leave the prefix for hex.*/` |
|      - | 3744 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3745 | `#endif` |
|    119 | 3746 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     89 | 3747 | `          if( iVal<0 ){` |
|     25 | 3748 | `            iVal = -iVal;` |
|      - | 3749 | `			/* Ticket 1433-003 */` |
|     25 | 3750 | `			if( iVal < 0 ){` |
|      - | 3751 | `				/* Overflow */` |
|    ! 0 | 3752 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3753 | `			}` |
|     25 | 3754 | `            prefix = '-';` |
|     77 | 3755 | `          }else if( flag_plussign )  prefix = '+';` |
|     63 | 3756 | `          else if( flag_blanksign )  prefix = ' ';` |
|     61 | 3757 | `          else                       prefix = 0;` |
|     45 | 3758 | `        }else{` |
|     31 | 3759 | `			if( iVal<0 ){` |
|    ! 0 | 3760 | `				iVal = -iVal;` |
|      - | 3761 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3762 | `				if( iVal < 0 ){` |
|      - | 3763 | `					/* Overflow */` |
|    ! 0 | 3764 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3765 | `				}` |
|    ! 0 | 3766 | `			}` |
|     31 | 3767 | `			prefix = 0;` |
|      - | 3768 | `		}` |
|    119 | 3769 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3770 | `          precision = width-(prefix!=0);` |
|      3 | 3771 | `        }` |
|    119 | 3772 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3773 | `        {` |
|      - | 3774 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3775 | `          register int base;` |
|    119 | 3776 | `          cset = pInfo->charset;` |
|    119 | 3777 | `          base = pInfo->base;` |
|     59 | 3778 | `          do{                                           /* Convert to ascii */` |
|    187 | 3779 | `            *(--zBuf) = cset[iVal%base];` |
|    187 | 3780 | `            iVal = iVal/base;` |
|    187 | 3781 | `          }while( iVal>0 );` |
|      - | 3782 | `        }` |
|    119 | 3783 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 3784 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3785 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3786 | `        }` |
|    119 | 3787 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 3788 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3789 | `          char *pre, x;` |
|      9 | 3790 | `          pre = pInfo->prefix;` |
|      9 | 3791 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3792 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3793 | `          }` |
|      4 | 3794 | `        }` |
|    119 | 3795 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 3796 | `		break;` |
|     28 | 3797 | `		case PH7_FMT_FLOAT:` |
|      - | 3798 | `		case PH7_FMT_EXP:` |
|      - | 3799 | `		case PH7_FMT_GENERIC:{` |
|      - | 3800 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3801 | `		long double realvalue;` |
|      - | 3802 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3803 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3804 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3805 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3806 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3807 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3808 | `		pArg = NEXT_ARG;` |
|     57 | 3809 | `		if( pArg == 0 ){` |
|    ! 0 | 3810 | `			realvalue = 0;` |
|    ! 0 | 3811 | `		}else{` |
|     57 | 3812 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3813 | `		}` |
|      - | 3814 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3815 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3816 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3817 | `			zBuf = "NAN";` |
|    ! 0 | 3818 | `			length = 3;` |
|    ! 0 | 3819 | `			break;` |
|      - | 3820 | `		}` |
|     57 | 3821 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3822 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3823 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3824 | `				zBuf = "-INF";` |
|    ! 0 | 3825 | `				length = 4;` |
|    ! 0 | 3826 | `			}else{` |
|    ! 0 | 3827 | `				zBuf = "INF";` |
|    ! 0 | 3828 | `				length = 3;` |
|      - | 3829 | `			}` |
|    ! 0 | 3830 | `			break;` |
|      - | 3831 | `		}` |
|     57 | 3832 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3833 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3834 | `        if( realvalue<0.0 ){` |
|      3 | 3835 | `          realvalue = -realvalue;` |
|      3 | 3836 | `          prefix = '-';` |
|      2 | 3837 | `        }else{` |
|     55 | 3838 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3839 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3840 | `          else                         prefix = 0;` |
|      - | 3841 | `        }` |
|     57 | 3842 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3843 | `        rounder = 0.0;` |
|      - | 3844 | `#if 0` |
|      - | 3845 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3846 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3847 | `#else` |
|      - | 3848 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3849 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3850 | `#endif` |
|     57 | 3851 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3852 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3853 | `        exp = 0;` |
|     57 | 3854 | `        if( realvalue>0.0 ){` |
|     61 | 3855 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3856 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3857 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3858 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3859 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3860 | `            zBuf = "NaN";` |
|    ! 0 | 3861 | `            length = 3;` |
|    ! 0 | 3862 | `            break;` |
|      - | 3863 | `          }` |
|     28 | 3864 | `        }` |
|     57 | 3865 | `        zBuf = zWorker;` |
|      - | 3866 | `        /*` |
|      - | 3867 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3868 | `        ** or etFLOAT, as appropriate.` |
|      - | 3869 | `        */` |
|     57 | 3870 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3871 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3872 | `          realvalue += rounder;` |
|    ! 0 | 3873 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3874 | `        }` |
|     57 | 3875 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3876 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3877 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3878 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3879 | `          }else{` |
|    ! 0 | 3880 | `            precision = precision - exp;` |
|    ! 0 | 3881 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3882 | `          }` |
|    ! 0 | 3883 | `        }else{` |
|     57 | 3884 | `          flag_rtz = 0;` |
|      - | 3885 | `        }` |
|      - | 3886 | `        /*` |
|      - | 3887 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3888 | `        ** the precision is too large to fit in buf[].` |
|      - | 3889 | `        */` |
|     57 | 3890 | `        nsd = 0;` |
|     57 | 3891 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3892 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3893 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3894 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3895 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3896 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3897 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3898 | `            *(zBuf++) = '0';` |
|     17 | 3899 | `          }` |
|    373 | 3900 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3901 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3902 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3903 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3904 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3905 | `          }` |
|     57 | 3906 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3907 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3908 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3909 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3910 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3911 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3912 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3913 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3914 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3915 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3916 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3917 | `          }` |
|    ! 0 | 3918 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3919 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3920 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3921 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3922 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3923 | `            if( exp>=100 ){` |
|    ! 0 | 3924 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3925 | `              exp %= 100;` |
|    ! 0 | 3926 | `            }` |
|    ! 0 | 3927 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3928 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3929 | `          }` |
|      - | 3930 | `        }` |
|      - | 3931 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3932 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3933 | `        ** integer conversions.*/` |
|     57 | 3934 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3935 | `        zBuf = zWorker;` |
|      - | 3936 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3937 | `        ** set and we are not left justified */` |
|     57 | 3938 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3939 | `          int i;` |
|      3 | 3940 | `          int nPad = width - length;` |
|     13 | 3941 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3942 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3943 | `          }` |
|      3 | 3944 | `          i = prefix!=0;` |
|      5 | 3945 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3946 | `          length = width;` |
|      1 | 3947 | `        }` |
|      - | 3948 | `#else` |
|      - | 3949 | `         zBuf = " ";` |
|      - | 3950 | `		 length = (int)sizeof(char);` |
|      - | 3951 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3952 | `		 break;` |
|      - | 3953 | `							 }` |
|      1 | 3954 | `		default:` |
|      - | 3955 | `			/* Invalid format specifer */` |
|      3 | 3956 | `			zWorker[0] = '?';` |
|      3 | 3957 | `			length = (int)sizeof(char);` |
|      2 | 3958 | `			break;` |
|      - | 3959 | `		}` |
|      - | 3960 | `		 /*` |
|      - | 3961 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3962 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3963 | `		 ** the output.` |
|      - | 3964 | `		 */` |
|    501 | 3965 | `    if( !flag_leftjustify ){` |
|      - | 3966 | `      register int nspace;` |
|    493 | 3967 | `      nspace = width-length;` |
|    493 | 3968 | `      if( nspace>0 ){` |
|      5 | 3969 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3970 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3971 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3972 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3973 | `			}` |
|    ! 0 | 3974 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3975 | `        }` |
|      5 | 3976 | `        if( nspace>0 ){` |
|      5 | 3977 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3978 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3979 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3980 | `			}` |
|      2 | 3981 | `		}` |
|      2 | 3982 | `      }` |
|    246 | 3983 | `    }` |
|    501 | 3984 | `    if( length>0 ){` |
|    501 | 3985 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    501 | 3986 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3987 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3988 | `		}` |
|    250 | 3989 | `    }` |
|    501 | 3990 | `    if( flag_leftjustify ){` |
|      - | 3991 | `      register int nspace;` |
|      9 | 3992 | `      nspace = width-length;` |
|      9 | 3993 | `      if( nspace>0 ){` |
|      9 | 3994 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3995 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3996 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3997 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3998 | `			}` |
|    ! 0 | 3999 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4000 | `        }` |
|      9 | 4001 | `        if( nspace>0 ){` |
|      9 | 4002 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4003 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4004 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4005 | `			}` |
|      4 | 4006 | `		}` |
|      4 | 4007 | `      }` |
|      4 | 4008 | `    }` |
|      1 | 4009 | ` }/* for(;;) */` |
|    261 | 4010 | `	return SXRET_OK;` |
|    131 | 4011 | `}` |
|      - | 4012 | `/*` |
|      - | 4013 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4014 | ` */` |
|     90 | 4015 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4016 | `{` |
|      - | 4017 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4018 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4019 | `	 * non-OK rc also stops the format loop. */` |
|     91 | 4020 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|     91 | 4021 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|     91 | 4022 | `	return *pRc;` |
|      1 | 4023 | `}` |
|      - | 4024 | `/*` |
|      - | 4025 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4026 | ` *  Return a formatted string.` |
|      - | 4027 | ` * Parameters` |
|      - | 4028 | ` *  $format` |
|      - | 4029 | ` *    The format string (see block comment above)` |
|      - | 4030 | ` * Return` |
|      - | 4031 | ` *  A string produced according to the formatting string format.` |
|      - | 4032 | ` */` |
|     62 | 4033 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4034 | `{` |
|      - | 4035 | `	const char *zFormat;` |
|     63 | 4036 | `	sxi32 rc = SXRET_OK;` |
|      - | 4037 | `	int nLen;` |
|     63 | 4038 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4039 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4040 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4041 | `		return PH7_OK;` |
|      - | 4042 | `	}` |
|      - | 4043 | `	/* Extract the string format */` |
|     61 | 4044 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 4045 | `	if( nLen < 1 ){` |
|      - | 4046 | `		/* Empty string */` |
|    ! 0 | 4047 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4048 | `		return PH7_OK;` |
|      - | 4049 | `	}` |
|      - | 4050 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     61 | 4051 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     61 | 4052 | `	if( rc != SXRET_OK ){` |
|      - | 4053 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4054 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4055 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4056 | `	}` |
|     61 | 4057 | `	return PH7_OK;` |
|     32 | 4058 | `}` |
|      - | 4059 | `/*` |
|      - | 4060 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4061 | ` */` |
|    922 | 4062 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4063 | `{` |
|    923 | 4064 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4065 | `	/* Call the VM output consumer directly */` |
|    923 | 4066 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4067 | `	/* Increment counter */` |
|    923 | 4068 | `	*pCounter += nLen;` |
|    923 | 4069 | `	return PH7_OK;` |
|      1 | 4070 | `}` |
|      - | 4071 | `/*` |
|      - | 4072 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4073 | ` *  Output a formatted string.` |
|      - | 4074 | ` * Parameters` |
|      - | 4075 | ` *  $format` |
|      - | 4076 | ` *   See sprintf() for a description of format.` |
|      - | 4077 | ` * Return` |
|      - | 4078 | ` *  The length of the outputted string.` |
|      - | 4079 | ` */` |
|    176 | 4080 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4081 | `{` |
|    177 | 4082 | `	ph7_int64 nCounter = 0;` |
|      - | 4083 | `	const char *zFormat;` |
|      - | 4084 | `	int nLen;` |
|    177 | 4085 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4086 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4087 | `		ph7_result_int(pCtx,0);` |
|      3 | 4088 | `		return PH7_OK;` |
|      - | 4089 | `	}` |
|      - | 4090 | `	/* Extract the string format */` |
|    175 | 4091 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    175 | 4092 | `	if( nLen < 1 ){` |
|      - | 4093 | `		/* Empty string */` |
|    ! 0 | 4094 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4095 | `		return PH7_OK;` |
|      - | 4096 | `	}` |
|      - | 4097 | `	/* Format the string */` |
|    175 | 4098 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4099 | `	/* Return the length of the outputted string */` |
|    175 | 4100 | `	ph7_result_int64(pCtx,nCounter);` |
|    175 | 4101 | `	return PH7_OK;` |
|     89 | 4102 | `}` |
|      - | 4103 | `/*` |
|      - | 4104 | ` * int vprintf(string $format,array $args)` |
|      - | 4105 | ` *  Output a formatted string.` |
|      - | 4106 | ` * Parameters` |
|      - | 4107 | ` *  $format` |
|      - | 4108 | ` *   See sprintf() for a description of format.` |
|      - | 4109 | ` * Return` |
|      - | 4110 | ` *  The length of the outputted string.` |
|      - | 4111 | ` */` |
|      2 | 4112 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4113 | `{` |
|      3 | 4114 | `	ph7_int64 nCounter = 0;` |
|      - | 4115 | `	const char *zFormat;` |
|      - | 4116 | `	ph7_hashmap *pMap;` |
|      - | 4117 | `	SySet sArg;` |
|      - | 4118 | `	int nLen,n;` |
|      3 | 4119 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4120 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4121 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4122 | `		return PH7_OK;` |
|      - | 4123 | `	}` |
|      - | 4124 | `	/* Extract the string format */` |
|      3 | 4125 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4126 | `	if( nLen < 1 ){` |
|      - | 4127 | `		/* Empty string */` |
|    ! 0 | 4128 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4129 | `		return PH7_OK;` |
|      - | 4130 | `	}` |
|      - | 4131 | `	/* Point to the hashmap */` |
|      3 | 4132 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4133 | `	/* Extract arguments from the hashmap */` |
|      3 | 4134 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4135 | `	/* Format the string */` |
|      3 | 4136 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4137 | `	/* Return the length of the outputted string */` |
|      3 | 4138 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4139 | `	/* Release the container */` |
|      3 | 4140 | `	SySetRelease(&sArg);` |
|      3 | 4141 | `	return PH7_OK;` |
|      2 | 4142 | `}` |
|      - | 4143 | `/*` |
|      - | 4144 | ` * int vsprintf(string $format,array $args)` |
|      - | 4145 | ` *  Output a formatted string.` |
|      - | 4146 | ` * Parameters` |
|      - | 4147 | ` *  $format` |
|      - | 4148 | ` *   See sprintf() for a description of format.` |
|      - | 4149 | ` * Return` |
|      - | 4150 | ` *  A string produced according to the formatting string format.` |
|      - | 4151 | ` */` |
|     10 | 4152 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4153 | `{` |
|      - | 4154 | `	const char *zFormat;` |
|      - | 4155 | `	ph7_hashmap *pMap;` |
|      - | 4156 | `	SySet sArg;` |
|     11 | 4157 | `	sxi32 rc = SXRET_OK;` |
|      - | 4158 | `	int nLen,n;` |
|     11 | 4159 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4160 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4161 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4162 | `		return PH7_OK;` |
|      - | 4163 | `	}` |
|      - | 4164 | `	/* Extract the string format */` |
|      7 | 4165 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4166 | `	if( nLen < 1 ){` |
|      - | 4167 | `		/* Empty string */` |
|    ! 0 | 4168 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4169 | `		return PH7_OK;` |
|      - | 4170 | `	}` |
|      - | 4171 | `	/* Point to hashmap */` |
|      7 | 4172 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4173 | `	/* Extract arguments from the hashmap */` |
|      7 | 4174 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4175 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      7 | 4176 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4177 | `	/* Release the container */` |
|      7 | 4178 | `	SySetRelease(&sArg);` |
|      7 | 4179 | `	if( rc != SXRET_OK ){` |
|      - | 4180 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4181 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4182 | `	}` |
|      7 | 4183 | `	return PH7_OK;` |
|      6 | 4184 | `}` |
|      - | 4185 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4186 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4187 | `/*` |
|      - | 4188 | ` * Symisc eXtension.` |
|      - | 4189 | ` * string size_format(int64 $size)` |
|      - | 4190 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4191 | ` *  Example:` |
|      - | 4192 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4193 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4194 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4195 | ` * Parameter` |
|      - | 4196 | ` *  $size` |
|      - | 4197 | ` *    Entity size in bytes.` |
|      - | 4198 | ` * Return` |
|      - | 4199 | ` *   Formatted string representation of the given size.` |
|      - | 4200 | ` */` |
|     24 | 4201 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4202 | `{` |
|      - | 4203 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4204 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4205 | `	sxi32 nRest,i_32;` |
|      - | 4206 | `	ph7_int64 iSize;` |
|     25 | 4207 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4208 |  |
|     25 | 4209 | `	if( nArg < 1 ){` |
|      - | 4210 | `		/* Missing argument,return the empty string */` |
|      3 | 4211 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4212 | `		return PH7_OK;` |
|      - | 4213 | `	}` |
|      - | 4214 | `	/* Extract the given size */` |
|     23 | 4215 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4216 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4217 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4218 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4219 | `		return PH7_OK;` |
|      - | 4220 | `	}` |
|     19 | 4221 | `	for(;;){` |
|     39 | 4222 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4223 | `		iSize >>= 10;` |
|     39 | 4224 | `		c++;` |
|     39 | 4225 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4226 | `			break;` |
|      - | 4227 | `		}` |
|      1 | 4228 | `	}` |
|     19 | 4229 | `	nRest /= 100;` |
|     19 | 4230 | `	if( nRest > 9 ){` |
|    ! 0 | 4231 | `		nRest = 9;` |
|    ! 0 | 4232 | `	}` |
|     19 | 4233 | `	if( iSize > 999 ){` |
|    ! 0 | 4234 | `		c++;` |
|    ! 0 | 4235 | `		nRest = 9;` |
|    ! 0 | 4236 | `		iSize = 0;` |
|    ! 0 | 4237 | `	}` |
|     19 | 4238 | `	i_32 = (sxi32)iSize;` |
|      - | 4239 | `	/* Format */` |
|     19 | 4240 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4241 | `	return PH7_OK;` |
|     13 | 4242 | `}` |
|      - | 4243 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4244 | `/*` |
|      - | 4245 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4246 | ` *   Calculate the md5 hash of a string.` |
|      - | 4247 | ` * Parameter` |
|      - | 4248 | ` *  $str` |
|      - | 4249 | ` *   Input string` |
|      - | 4250 | ` * $raw_output` |
|      - | 4251 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4252 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4253 | ` * Return` |
|      - | 4254 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4255 | ` */` |
|     14 | 4256 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4257 | `{` |
|      - | 4258 | `	unsigned char zDigest[16];` |
|     15 | 4259 | `	int raw_output = FALSE;` |
|      - | 4260 | `	const void *pIn;` |
|      - | 4261 | `	int nLen;` |
|     15 | 4262 | `	if( nArg < 1 ){` |
|      - | 4263 | `		/* Missing arguments,return the empty string */` |
|      3 | 4264 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4265 | `		return PH7_OK;` |
|      - | 4266 | `	}` |
|      - | 4267 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4268 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4269 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4270 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4271 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4272 | `	}` |
|      - | 4273 | `	/* Compute the MD5 digest */` |
|     13 | 4274 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4275 | `	if( raw_output ){` |
|      - | 4276 | `		/* Output raw digest */` |
|      5 | 4277 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4278 | `	}else{` |
|      - | 4279 | `		/* Perform a binary to hex conversion */` |
|      9 | 4280 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4281 | `	}` |
|     13 | 4282 | `	return PH7_OK;` |
|      8 | 4283 | `}` |
|      - | 4284 | `/*` |
|      - | 4285 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4286 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4287 | ` * Parameter` |
|      - | 4288 | ` *  $str` |
|      - | 4289 | ` *   Input string` |
|      - | 4290 | ` * $raw_output` |
|      - | 4291 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4292 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4293 | ` * Return` |
|      - | 4294 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4295 | ` */` |
|     12 | 4296 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4297 | `{` |
|      - | 4298 | `	unsigned char zDigest[20];` |
|     13 | 4299 | `	int raw_output = FALSE;` |
|      - | 4300 | `	const void *pIn;` |
|      - | 4301 | `	int nLen;` |
|     13 | 4302 | `	if( nArg < 1 ){` |
|      - | 4303 | `		/* Missing arguments,return the empty string */` |
|      3 | 4304 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4305 | `		return PH7_OK;` |
|      - | 4306 | `	}` |
|      - | 4307 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4308 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4309 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4310 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4311 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4312 | `	}` |
|      - | 4313 | `	/* Compute the SHA1 digest */` |
|     11 | 4314 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4315 | `	if( raw_output ){` |
|      - | 4316 | `		/* Output raw digest */` |
|      5 | 4317 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4318 | `	}else{` |
|      - | 4319 | `		/* Perform a binary to hex conversion */` |
|      7 | 4320 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4321 | `	}` |
|     11 | 4322 | `	return PH7_OK;` |
|      7 | 4323 | `}` |
|      - | 4324 | `/*` |
|      - | 4325 | ` * int64 crc32(string $str)` |
|      - | 4326 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4327 | ` * Parameter` |
|      - | 4328 | ` *  $str` |
|      - | 4329 | ` *   Input string` |
|      - | 4330 | ` * Return` |
|      - | 4331 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4332 | ` */` |
|      4 | 4333 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4334 | `{` |
|      - | 4335 | `	const void *pIn;` |
|      - | 4336 | `	sxu32 nCRC;` |
|      - | 4337 | `	int nLen;` |
|      5 | 4338 | `	if( nArg < 1 ){` |
|      - | 4339 | `		/* Missing arguments,return 0 */` |
|      3 | 4340 | `		ph7_result_int(pCtx,0);` |
|      3 | 4341 | `		return PH7_OK;` |
|      - | 4342 | `	}` |
|      - | 4343 | `	/* Extract the input string */` |
|      3 | 4344 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4345 | `	if( nLen < 1 ){` |
|      - | 4346 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4347 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4348 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4349 | `		return PH7_OK;` |
|      - | 4350 | `	}` |
|      - | 4351 | `	/* Calculate the sum */` |
|      3 | 4352 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4353 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4354 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4355 | `	return PH7_OK;` |
|      3 | 4356 | `}` |
|      - | 4357 | `/*` |
|      - | 4358 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4359 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4360 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4361 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4362 | ` */` |
|     11 | 4363 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4364 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4365 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4366 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4367 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4368 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4369 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4370 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4371 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4372 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4373 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4374 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4375 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4376 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4377 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4378 | `struct HashAlgo {` |
|      - | 4379 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4380 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4381 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4382 | `	void (*xInit)(HashCtx *);` |
|      - | 4383 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4384 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4385 | `};` |
|      - | 4386 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4387 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4388 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4389 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4390 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4391 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4392 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4393 | `};` |
|      - | 4394 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4395 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4396 | `	sxu32 i;` |
|    279 | 4397 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4398 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4399 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4400 | `			return &aHashAlgo[i];` |
|      - | 4401 | `		}` |
|    106 | 4402 | `	}` |
|      6 | 4403 | `	return 0;` |
|     38 | 4404 | `}` |
|      - | 4405 | `/*` |
|      - | 4406 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4407 | ` *   Generate a hash value (message digest).` |
|      - | 4408 | ` */` |
|     54 | 4409 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4410 | `{` |
|      - | 4411 | `	const HashAlgo *pAlgo;` |
|      - | 4412 | `	const char *zAlgo,*zData;` |
|     56 | 4413 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4414 | `	HashCtx sCtx;` |
|      - | 4415 | `	unsigned char zDigest[64];` |
|     56 | 4416 | `	if( nArg < 2 ){` |
|    ! 0 | 4417 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4418 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4419 | `	}` |
|     56 | 4420 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4421 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4422 | `	if( pAlgo == 0 ){` |
|      3 | 4423 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4424 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4425 | `	}` |
|     53 | 4426 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4427 | `	if( nArg > 2 ){` |
|      9 | 4428 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4429 | `	}` |
|     53 | 4430 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4431 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4432 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4433 | `	if( raw_output ){` |
|      9 | 4434 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4435 | `	}else{` |
|     45 | 4436 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4437 | `	}` |
|     53 | 4438 | `	return PH7_OK;` |
|     29 | 4439 | `}` |
|      - | 4440 | `/*` |
|      - | 4441 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4442 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4443 | ` */` |
|     16 | 4444 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4445 | `{` |
|      - | 4446 | `	const HashAlgo *pAlgo;` |
|      - | 4447 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4448 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4449 | `	HashCtx sCtx;` |
|      - | 4450 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4451 | `	int i,nBlock,nDigest;` |
|     18 | 4452 | `	if( nArg < 3 ){` |
|    ! 0 | 4453 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4454 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4455 | `	}` |
|     18 | 4456 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4457 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4458 | `	if( pAlgo == 0 ){` |
|      3 | 4459 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4460 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4461 | `	}` |
|     15 | 4462 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4463 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4464 | `	if( nArg > 3 ){` |
|      3 | 4465 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4466 | `	}` |
|     15 | 4467 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4468 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4469 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4470 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4471 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4472 | `	if( nKeyLen > nBlock ){` |
|      3 | 4473 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4474 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4475 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4476 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4477 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4478 | `	}` |
|   1039 | 4479 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4480 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4481 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4482 | `	}` |
|      - | 4483 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4484 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4485 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4486 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4487 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4488 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4489 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4490 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4491 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4492 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4493 | `	if( raw_output ){` |
|      3 | 4494 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4495 | `	}else{` |
|     13 | 4496 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4497 | `	}` |
|     15 | 4498 | `	return PH7_OK;` |
|     10 | 4499 | `}` |
|      - | 4500 | `/*` |
|      - | 4501 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4502 | ` *   Timing-attack-safe string comparison.` |
|      - | 4503 | ` */` |
|     14 | 4504 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4505 | `{` |
|      - | 4506 | `	const char *zKnown,*zUser;` |
|      - | 4507 | `	int nKnown,nUser,i;` |
|     17 | 4508 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4509 | `	if( nArg < 2 ){` |
|    ! 0 | 4510 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4511 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4512 | `	}` |
|     17 | 4513 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4514 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4515 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4516 | `			ph7_type_name(apArg[0]));` |
|      - | 4517 | `	}` |
|     14 | 4518 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4519 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4520 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4521 | `			ph7_type_name(apArg[1]));` |
|      - | 4522 | `	}` |
|     11 | 4523 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4524 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4525 | `	if( nKnown != nUser ){` |
|      5 | 4526 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4527 | `		return PH7_OK;` |
|      - | 4528 | `	}` |
|      - | 4529 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4530 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4531 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4532 | `	}` |
|      7 | 4533 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4534 | `	return PH7_OK;` |
|     10 | 4535 | `}` |
|      - | 4536 | `/*` |
|      - | 4537 | ` * array hash_algos(void)` |
|      - | 4538 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4539 | ` */` |
|      2 | 4540 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4541 | `{` |
|      - | 4542 | `	ph7_value *pArray,*pValue;` |
|      - | 4543 | `	sxu32 i;` |
|      1 | 4544 | `	SXUNUSED(nArg);` |
|      1 | 4545 | `	SXUNUSED(apArg);` |
|      3 | 4546 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4547 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4548 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4549 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4550 | `		return PH7_OK;` |
|      - | 4551 | `	}` |
|     15 | 4552 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4553 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4554 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4555 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4556 | `	}` |
|      3 | 4557 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4558 | `	return PH7_OK;` |
|      2 | 4559 | `}` |
|      - | 4560 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4561 | `/*` |
|      - | 4562 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4563 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4564 | ` */` |
|      - | 4565 | `/*` |
|      - | 4566 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4567 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4568 | ` */` |
|     40 | 4569 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4570 | `{` |
|      - | 4571 | `	int iCost;` |
|     40 | 4572 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4573 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4574 | `		return FALSE;` |
|      - | 4575 | `	}` |
|     29 | 4576 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4577 | `		return FALSE;` |
|      - | 4578 | `	}` |
|     29 | 4579 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4580 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4581 | `		return FALSE;` |
|      - | 4582 | `	}` |
|     27 | 4583 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4584 | `	return TRUE;` |
|     21 | 4585 | `}` |
|      - | 4586 | `/*` |
|      - | 4587 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4588 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4589 | ` */` |
|     20 | 4590 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4591 | `{` |
|     23 | 4592 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4593 | `		return TRUE;` |
|      - | 4594 | `	}` |
|     23 | 4595 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4596 | `		int nAlgo;` |
|     23 | 4597 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4598 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4599 | `	}` |
|    ! 0 | 4600 | `	return FALSE;` |
|     13 | 4601 | `}` |
|      - | 4602 | `/*` |
|      - | 4603 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4604 | ` *  Create a bcrypt hash of the password.` |
|      - | 4605 | ` */` |
|     16 | 4606 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4607 | `{` |
|      - | 4608 | `	const char *zPwd;` |
|     19 | 4609 | `	int nPwd,iCost = 12;` |
|      - | 4610 | `	unsigned char aSalt[16];` |
|      - | 4611 | `	char zHash[60];` |
|     19 | 4612 | `	if( nArg < 2 ){` |
|    ! 0 | 4613 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4614 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4615 | `	}` |
|     19 | 4616 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4617 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4618 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4619 | `	}` |
|      - | 4620 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4621 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4622 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4623 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4624 | `	}` |
|     16 | 4625 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4626 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4627 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4628 | `	}` |
|     13 | 4629 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4630 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4631 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4632 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4633 | `	}` |
|     13 | 4634 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4635 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4636 | `		return PH7_OK;` |
|      - | 4637 | `	}` |
|     13 | 4638 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4639 | `	return PH7_OK;` |
|     11 | 4640 | `}` |
|      - | 4641 | `/*` |
|      - | 4642 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4643 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4644 | ` */` |
|     28 | 4645 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4646 | `{` |
|      - | 4647 | `	const char *zPwd,*zHash;` |
|      - | 4648 | `	int nPwd,nHash,iCost,i;` |
|      - | 4649 | `	unsigned char aSalt[16];` |
|      - | 4650 | `	char zComputed[60];` |
|     29 | 4651 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4652 | `	if( nArg < 2 ){` |
|    ! 0 | 4653 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4654 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4655 | `	}` |
|     29 | 4656 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4657 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4658 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4659 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4660 | `		return PH7_OK;` |
|      - | 4661 | `	}` |
|      - | 4662 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4663 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4664 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4665 | `		return PH7_OK;` |
|      - | 4666 | `	}` |
|     19 | 4667 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4668 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4669 | `		return PH7_OK;` |
|      - | 4670 | `	}` |
|      - | 4671 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4672 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4673 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4674 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4675 | `	}` |
|     19 | 4676 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4677 | `	return PH7_OK;` |
|     15 | 4678 | `}` |
|      - | 4679 | `/*` |
|      - | 4680 | ` * array password_get_info(string $hash)` |
|      - | 4681 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4682 | ` */` |
|      6 | 4683 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4684 | `{` |
|      7 | 4685 | `	const char *zHash = "";` |
|      7 | 4686 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4687 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4688 | `	if( nArg > 0 ){` |
|      7 | 4689 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4690 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4691 | `	}` |
|      7 | 4692 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4693 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4694 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4695 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4696 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4697 | `		return PH7_OK;` |
|      - | 4698 | `	}` |
|      7 | 4699 | `	if( bBcrypt ){` |
|      5 | 4700 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4701 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4702 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4703 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4704 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4705 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4706 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4707 | `	}else{` |
|      3 | 4708 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4709 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4710 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4711 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4712 | `	}` |
|      7 | 4713 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4714 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4715 | `	return PH7_OK;` |
|      4 | 4716 | `}` |
|      - | 4717 | `/*` |
|      - | 4718 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4719 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4720 | ` */` |
|      6 | 4721 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4722 | `{` |
|      - | 4723 | `	const char *zHash;` |
|      7 | 4724 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4725 | `	if( nArg < 2 ){` |
|    ! 0 | 4726 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4727 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4728 | `	}` |
|      7 | 4729 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4730 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4731 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4732 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4733 | `		return PH7_OK;` |
|      - | 4734 | `	}` |
|      5 | 4735 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4736 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4737 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4738 | `	}` |
|      5 | 4739 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4740 | `	return PH7_OK;` |
|      4 | 4741 | `}` |
|      - | 4742 | `/*` |
|      - | 4743 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4744 | ` *` |
|      - | 4745 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4746 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4747 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4748 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4749 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4750 | ` */` |
|      - | 4751 | `#define FV_VALIDATE_INT     257` |
|      - | 4752 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4753 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4754 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4755 | `#define FV_VALIDATE_URL     273` |
|      - | 4756 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4757 | `#define FV_VALIDATE_IP      275` |
|      - | 4758 | `#define FV_VALIDATE_MAC     276` |
|      - | 4759 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4760 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4761 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4762 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4763 | `#define FV_SANITIZE_URL     518` |
|      - | 4764 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4765 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4766 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4767 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4768 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4769 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4770 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4771 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4772 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4773 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4774 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4775 |  |
|      - | 4776 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4777 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 4778 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 4779 | `	const char *z = *pz;` |
|    153 | 4780 | `	int n = *pn;` |
|    157 | 4781 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 4782 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 4783 | `	*pz = z; *pn = n;` |
|    153 | 4784 | `}` |
|      - | 4785 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4786 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4787 | `	int neg = 0, i;` |
|     57 | 4788 | `	sxu64 u = 0;` |
|     57 | 4789 | `	FvTrim(&z,&n);` |
|     57 | 4790 | `	if( n==0 ){ return 0; }` |
|     51 | 4791 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4792 | `	if( n==0 ){ return 0; }` |
|     49 | 4793 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4794 | `		z += 2; n -= 2;` |
|      3 | 4795 | `		if( n==0 ){ return 0; }` |
|      7 | 4796 | `		for( i=0; i<n; i++ ){` |
|      5 | 4797 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4798 | `			if( h<0 ){ return 0; }` |
|      5 | 4799 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4800 | `			u = u*16 + (sxu64)h;` |
|      3 | 4801 | `		}` |
|     48 | 4802 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4803 | `		for( i=0; i<n; i++ ){` |
|      7 | 4804 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4805 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4806 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4807 | `		}` |
|      2 | 4808 | `	}else{` |
|     45 | 4809 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4810 | `		for( i=0; i<n; i++ ){` |
|    173 | 4811 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4812 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4813 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4814 | `		}` |
|      - | 4815 | `	}` |
|     33 | 4816 | `	if( neg ){` |
|      5 | 4817 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4818 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4819 | `	}else{` |
|     29 | 4820 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4821 | `		*pOut = (ph7_int64)u;` |
|      - | 4822 | `	}` |
|     31 | 4823 | `	return 1;` |
|     29 | 4824 | `}` |
|      - | 4825 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 4826 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4827 | `	char zBuf[512];` |
|     69 | 4828 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 4829 | `	const char *zv; int nv; double d = 0;` |
|     69 | 4830 | `	FvTrim(&z,&n);` |
|      - | 4831 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4832 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 4833 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 4834 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4835 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4836 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4837 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4838 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 4839 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 4840 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 4841 | `		intEnd = s;` |
|    167 | 4842 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 4843 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 4844 | `			intEnd++;` |
|      1 | 4845 | `		}` |
|     25 | 4846 | `		if( hasComma ){` |
|     25 | 4847 | `			segStart = s; segIdx = 0;` |
|    165 | 4848 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 4849 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 4850 | `					int segLen = i - segStart, k;` |
|     49 | 4851 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 4852 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 4853 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 4854 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 4855 | `						zBuf[m++] = z[k];` |
|     41 | 4856 | `					}` |
|     39 | 4857 | `					segStart = i+1; segIdx++;` |
|     19 | 4858 | `				}` |
|     71 | 4859 | `			}` |
|      8 | 4860 | `		}else{` |
|    ! 0 | 4861 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4862 | `		}` |
|     27 | 4863 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 4864 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 4865 | `			zBuf[m++] = z[i];` |
|      7 | 4866 | `		}` |
|     15 | 4867 | `		zv = zBuf; nv = m;` |
|      8 | 4868 | `	}else{` |
|     45 | 4869 | `		zv = z; nv = n;` |
|      - | 4870 | `	}` |
|     59 | 4871 | `	i = 0;` |
|     59 | 4872 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 4873 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 4874 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 4875 | `		i++;` |
|     39 | 4876 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 4877 | `	}` |
|     59 | 4878 | `	if( !seenDigit ){ return 0; }` |
|     57 | 4879 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 4880 | `		i++;` |
|     29 | 4881 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 4882 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 4883 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 4884 | `	}` |
|     57 | 4885 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4886 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 4887 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 4888 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 4889 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 4890 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 4891 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 4892 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 4893 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 4894 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 4895 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 4896 | `	zBuf[nv] = 0;` |
|     53 | 4897 | `	errno = 0;` |
|     53 | 4898 | `	d = strtod(zBuf,0);` |
|     53 | 4899 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 4900 | `		return 0;` |
|      - | 4901 | `	}` |
|     39 | 4902 | `	*pOut = d;` |
|     39 | 4903 | `	return 1;` |
|     35 | 4904 | `}` |
|      - | 4905 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4906 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4907 | ` * false, NOT failures. */` |
|     33 | 4908 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4909 | `	FvTrim(&z,&n);` |
|     32 | 4910 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4911 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4912 | `		*pBool = 1; return 1;` |
|      - | 4913 | `	}` |
|     22 | 4914 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4915 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4916 | `		*pBool = 0; return 1;` |
|      - | 4917 | `	}` |
|      9 | 4918 | `	return 0;` |
|     15 | 4919 | `}` |
|      - | 4920 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4921 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4922 | `	int i = 0, parts = 0;` |
|     77 | 4923 | `	while( i<n ){` |
|     65 | 4924 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4925 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4926 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4927 | `			if( val>255 ){ return 0; }` |
|     79 | 4928 | `			digits++; i++;` |
|      1 | 4929 | `		}` |
|     59 | 4930 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4931 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4932 | `		parts++;` |
|     45 | 4933 | `		if( parts>4 ){ return 0; }` |
|     45 | 4934 | `		if( i<n ){` |
|     33 | 4935 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4936 | `			i++;` |
|     33 | 4937 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4938 | `		}` |
|      1 | 4939 | `	}` |
|     13 | 4940 | `	return parts==4;` |
|     17 | 4941 | `}` |
|      - | 4942 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4943 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4944 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4945 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4946 | `	if( n==0 ){ return 0; }` |
|    145 | 4947 | `	while( i<=n ){` |
|    133 | 4948 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4949 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4950 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4951 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4952 | `			if( isV4 ){` |
|     11 | 4953 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4954 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4955 | `				groups += 2;` |
|      3 | 4956 | `			}else{` |
|     13 | 4957 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4958 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4959 | `				groups++;` |
|      - | 4960 | `			}` |
|     17 | 4961 | `			segStart = i+1;` |
|      8 | 4962 | `		}` |
|    127 | 4963 | `		i++;` |
|      1 | 4964 | `	}` |
|     13 | 4965 | `	return groups;` |
|     10 | 4966 | `}` |
|      - | 4967 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4968 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4969 | `	const char *zDbl = 0;` |
|      - | 4970 | `	int i, ga, gb;` |
|    139 | 4971 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4972 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4973 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4974 | `			zDbl = z+i;` |
|      5 | 4975 | `		}` |
|     61 | 4976 | `	}` |
|     17 | 4977 | `	if( zDbl==0 ){` |
|      9 | 4978 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4979 | `	}else{` |
|      9 | 4980 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4981 | `		int lenB = n - lenA - 2;` |
|      9 | 4982 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4983 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4984 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4985 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4986 | `	}` |
|     10 | 4987 | `}` |
|     25 | 4988 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4989 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4990 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4991 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4992 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4993 | `	return 0;` |
|     13 | 4994 | `}` |
|      - | 4995 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 4996 | `static int FvValidateMac(const char *z,int n){` |
|      - | 4997 | `	char sep;` |
|      - | 4998 | `	int i;` |
|     11 | 4999 | `	if( n!=17 ){ return 0; }` |
|      7 | 5000 | `	sep = z[2];` |
|      7 | 5001 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5002 | `	for( i=0; i<17; i++ ){` |
|    101 | 5003 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5004 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5005 | `	}` |
|      5 | 5006 | `	return 1;` |
|      6 | 5007 | `}` |
|      - | 5008 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5009 | ` * parts or IP-literal domains). */` |
|     28 | 5010 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5011 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5012 | `	const char *zDom;` |
|     28 | 5013 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5014 | `	for( i=0; i<n; i++ ){` |
|    181 | 5015 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5016 | `	}` |
|     21 | 5017 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5018 | `	localLen = at;` |
|     21 | 5019 | `	zDom = z + at + 1;` |
|     21 | 5020 | `	domLen = n - at - 1;` |
|     21 | 5021 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5022 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5023 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5024 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5025 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5026 | `	}` |
|     15 | 5027 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5028 | `	labelStart = 0;` |
|     85 | 5029 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5030 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5031 | `			int ll = i - labelStart;` |
|     25 | 5032 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5033 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5034 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5035 | `			labelStart = i+1;` |
|     12 | 5036 | `		}else{` |
|     51 | 5037 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5038 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5039 | `		}` |
|     37 | 5040 | `	}` |
|     11 | 5041 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5042 | `	return 1;` |
|     15 | 5043 | `}` |
|      - | 5044 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5045 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5046 | `	int i;` |
|     11 | 5047 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5048 | `	for( i=0; i<n; i++ ){` |
|     75 | 5049 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5050 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5051 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5052 | `	}` |
|      7 | 5053 | `	return 1;` |
|      6 | 5054 | `}` |
|      - | 5055 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5056 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5057 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5058 | `	SyhttpUri sUri;` |
|     15 | 5059 | `	if( n==0 ){ return 0; }` |
|     15 | 5060 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5061 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5062 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5063 | `}` |
|      - | 5064 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5065 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5066 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5067 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5068 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5069 | `	int i, runStart = 0;` |
|     37 | 5070 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5071 | `	for( i=0; i<n; i++ ){` |
|     91 | 5072 | `		char c = z[i];` |
|     91 | 5073 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5074 | `		if( !keep && isFloat ){` |
|     38 | 5075 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5076 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5077 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5078 | `		}` |
|     61 | 5079 | `		if( !keep ){` |
|     33 | 5080 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5081 | `			runStart = i+1;` |
|     16 | 5082 | `		}` |
|     31 | 5083 | `	}` |
|      7 | 5084 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5085 | `}` |
|      - | 5086 | `/* SANITIZE_SPECIAL_CHARS (full=0, numeric entities; also encodes control bytes` |
|      - | 5087 | ` * <32 as &#N;) / FULL_SPECIAL_CHARS (full=1, named entities for <>&"').` |
|      - | 5088 | ` * Divergence on bytes >=128: PHP's FULL filter is UTF-8-aware — it named-entity` |
|      - | 5089 | ` * encodes valid sequences ("\xC3\xA9" -> "&eacute;") and drops invalid ones; we` |
|      - | 5090 | ` * pass every byte >=128 through verbatim (the engine has no UTF-8 entity table,` |
|      - | 5091 | ` * and PH7_builtin_htmlspecialchars behaves the same way). Bytes 0-127 match. */` |
|      7 | 5092 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int full){` |
|      7 | 5093 | `	int i, runStart = 0;` |
|      - | 5094 | `	const char *zEnt;` |
|      7 | 5095 | `	ph7_result_string(pCtx,"",0);` |
|     43 | 5096 | `	for( i=0; i<n; i++ ){` |
|     37 | 5097 | `		unsigned char c = (unsigned char)z[i];` |
|     37 | 5098 | `		switch( c ){` |
|      5 | 5099 | `		case '<':  zEnt = full?"&lt;":"&#60;";   break;` |
|      5 | 5100 | `		case '>':  zEnt = full?"&gt;":"&#62;";   break;` |
|      5 | 5101 | `		case '&':  zEnt = full?"&amp;":"&#38;";  break;` |
|      5 | 5102 | `		case '"':  zEnt = full?"&quot;":"&#34;"; break;` |
|      5 | 5103 | `		case '\'': zEnt = full?"&#039;":"&#39;"; break;` |
|      8 | 5104 | `		default:` |
|     17 | 5105 | `			if( full \|\| c>=32 ){ continue; } /* keep in the current run */` |
|      - | 5106 | `			/* SPECIAL_CHARS encodes a control byte as a numeric entity. */` |
|      5 | 5107 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      5 | 5108 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      5 | 5109 | `			runStart = i+1;` |
|      5 | 5110 | `			continue;` |
|      - | 5111 | `		}` |
|     21 | 5112 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     21 | 5113 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     21 | 5114 | `		runStart = i+1;` |
|     11 | 5115 | `	}` |
|      7 | 5116 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5117 | `}` |
|     25 | 5118 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5119 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5120 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5121 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5122 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5123 | `}` |
|     23 | 5124 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5125 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5126 | `}` |
|      - | 5127 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5128 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5129 | `	int i, runStart = 0;` |
|      5 | 5130 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5131 | `	for( i=0; i<n; i++ ){` |
|     47 | 5132 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5133 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5134 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5135 | `			runStart = i+1;` |
|      5 | 5136 | `		}` |
|     24 | 5137 | `	}` |
|      5 | 5138 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5139 | `}` |
|      - | 5140 | `/*` |
|      - | 5141 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 5142 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 5143 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 5144 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 5145 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 5146 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 5147 | ` */` |
|    268 | 5148 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 5149 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 5150 | `                         ph7_value *pDefault)` |
|      3 | 5151 | `{` |
|    271 | 5152 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5153 | `	const char *zVal; int nVal;` |
|      - | 5154 | `	/* An array/object input fails every scalar filter. */` |
|    271 | 5155 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    269 | 5156 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    269 | 5157 | `	switch( iFilter ){` |
|     28 | 5158 | `	case FV_VALIDATE_INT: {` |
|      - | 5159 | `		ph7_int64 v;` |
|     58 | 5160 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5161 | `		if( pOpts ){` |
|      7 | 5162 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5163 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5164 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5165 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5166 | `		}` |
|     29 | 5167 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5168 | `		return PH7_OK;` |
|      - | 5169 | `	}` |
|     34 | 5170 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5171 | `		double d;` |
|     69 | 5172 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 5173 | `		ph7_result_double(pCtx,d);` |
|     39 | 5174 | `		return PH7_OK;` |
|      - | 5175 | `	}` |
|     14 | 5176 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5177 | `		int b;` |
|     29 | 5178 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5179 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5180 | `		return PH7_OK;` |
|      - | 5181 | `	}` |
|     25 | 5182 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5183 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 5184 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5185 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5186 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5187 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5188 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5189 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5190 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5191 | `		if( pRe==0 ){` |
|      3 | 5192 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5193 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5194 | `		}` |
|      5 | 5195 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5196 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5197 | `		goto pass;` |
|      - | 5198 | `#else` |
|      - | 5199 | `		goto fail;` |
|      - | 5200 | `#endif` |
|      - | 5201 | `	}` |
|      3 | 5202 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5203 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|      5 | 5204 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5205 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeSpecial(pCtx,zVal,nVal,1); return PH7_OK;` |
|      3 | 5206 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5207 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     10 | 5208 | `	case FV_DEFAULT: goto pass; /* FILTER_UNSAFE_RAW: pass through unchanged */` |
|    ! 0 | 5209 | `	default:` |
|    ! 0 | 5210 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5211 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5212 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5213 | `	}` |
|     58 | 5214 | `fail:` |
|    118 | 5215 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 5216 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 5217 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 5218 | `	return PH7_OK;` |
|     24 | 5219 | `pass: /* validation passed: return the (string) input unchanged */` |
|     50 | 5220 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     50 | 5221 | `	return PH7_OK;` |
|    137 | 5222 | `}` |
|      - | 5223 | `/*` |
|      - | 5224 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 5225 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 5226 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 5227 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 5228 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 5229 | ` */` |
|    280 | 5230 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 5231 | `                              int *piFilter,int *piFlags,` |
|      - | 5232 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 5233 | `{` |
|    283 | 5234 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    283 | 5235 | `	if( nArg>iBase+1 ){` |
|     66 | 5236 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     20 | 5237 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     20 | 5238 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     20 | 5239 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     20 | 5240 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     20 | 5241 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     11 | 5242 | `		}else{` |
|     48 | 5243 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 5244 | `		}` |
|     32 | 5245 | `	}` |
|    283 | 5246 | `}` |
|      - | 5247 | `/*` |
|      - | 5248 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5249 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 5250 | ` */` |
|    258 | 5251 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5252 | `{` |
|    260 | 5253 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    260 | 5254 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    260 | 5255 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    260 | 5256 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    260 | 5257 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    131 | 5258 | `}` |
|      - | 5259 | `/*` |
|      - | 5260 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5261 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 5262 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 5263 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 5264 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 5265 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 5266 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 5267 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 5268 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 5269 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 5270 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 5271 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 5272 | ` *  php's snapshot.` |
|      - | 5273 | ` */` |
|     28 | 5274 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5275 | `{` |
|     30 | 5276 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 5277 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 5278 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 5279 | `	if( nArg<2 ){` |
|      7 | 5280 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 5281 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 5282 | `	}` |
|     26 | 5283 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 5284 | `	switch( iType ){` |
|      3 | 5285 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 5286 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 5287 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 5288 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 5289 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 5290 | `	default:` |
|      3 | 5291 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5292 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 5293 | `	}` |
|     23 | 5294 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 5295 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 5296 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 5297 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 5298 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 5299 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 5300 | `	if( pElem==0 ){` |
|      - | 5301 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 5302 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 5303 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 5304 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 5305 | `		else { ph7_result_null(pCtx); }` |
|     13 | 5306 | `		return PH7_OK;` |
|      - | 5307 | `	}` |
|     11 | 5308 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 5309 | `}` |
|      - | 5310 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5311 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5312 | `/*` |
|      - | 5313 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5314 |  |
|      - | 5315 | ` */` |
|      4 | 5316 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5317 | `	const char *zInput, /* Raw input */` |
|      - | 5318 | `	int nByte,  /* Input length */` |
|      - | 5319 | `	int delim,  /* Delimiter */` |
|      - | 5320 | `	int encl,   /* Enclosure */` |
|      - | 5321 | `	int escape,  /* Escape character */` |
|      - | 5322 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5323 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5324 | `	)` |
|      1 | 5325 | `{` |
|      5 | 5326 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5327 | `	const char *zIn = zInput;` |
|      - | 5328 | `	const char *zPtr;` |
|      - | 5329 | `	int isEnc;` |
|      - | 5330 | `	/* Start processing */` |
|      8 | 5331 | `	for(;;){` |
|     17 | 5332 | `		if( zIn >= zEnd ){` |
|      - | 5333 | `			/* No more input to process */` |
|      5 | 5334 | `			break;` |
|      - | 5335 | `		}` |
|     13 | 5336 | `		isEnc = 0;` |
|     13 | 5337 | `		zPtr = zIn;` |
|      - | 5338 | `		/* Find the first delimiter */` |
|     27 | 5339 | `		while( zIn < zEnd ){` |
|     23 | 5340 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5341 | `				/* Delimiter found,break imediately */` |
|      5 | 5342 | `				break;` |
|     15 | 5343 | `			}else if( zIn[0] == encl ){` |
|      - | 5344 | `				/* Inside enclosure? */` |
|    ! 0 | 5345 | `				isEnc = !isEnc;` |
|     15 | 5346 | `			}else if( zIn[0] == escape ){` |
|      - | 5347 | `				/* Escape sequence */` |
|    ! 0 | 5348 | `				zIn++;` |
|    ! 0 | 5349 | `			}` |
|      - | 5350 | `			/* Advance the cursor */` |
|     15 | 5351 | `			zIn++;` |
|      1 | 5352 | `		}` |
|     13 | 5353 | `		if( zIn > zPtr ){` |
|     13 | 5354 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5355 | `			sxi32 rc;` |
|      - | 5356 | `			/* Invoke the supllied callback */` |
|     13 | 5357 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5358 | `				zPtr++;` |
|    ! 0 | 5359 | `				nByteChunk-=2;` |
|    ! 0 | 5360 | `			}` |
|     13 | 5361 | `			if( nByteChunk > 0 ){` |
|     13 | 5362 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5363 | `				if( rc == SXERR_ABORT ){` |
|      - | 5364 | `					/* User callback request an operation abort */` |
|    ! 0 | 5365 | `					break;` |
|      - | 5366 | `				}` |
|      6 | 5367 | `			}` |
|      6 | 5368 | `		}` |
|      - | 5369 | `		/* Ignore trailing delimiter */` |
|     21 | 5370 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5371 | `			zIn++;` |
|      1 | 5372 | `		}` |
|      1 | 5373 | `	}` |
|      5 | 5374 | `	return SXRET_OK;` |
|      1 | 5375 | `}` |
|      - | 5376 | `/*` |
|      - | 5377 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5378 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5379 | ` * argument to this callback.` |
|      - | 5380 | ` */` |
|     12 | 5381 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5382 | `{` |
|     13 | 5383 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5384 | `	ph7_value sEntry;` |
|      - | 5385 | `	SyString sToken;` |
|      - | 5386 | `	/* Insert the token in the given array */` |
|     13 | 5387 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5388 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5389 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5390 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5391 | `		return SXRET_OK;` |
|      - | 5392 | `	}` |
|     13 | 5393 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5394 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5395 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5396 | `	return SXRET_OK;` |
|      7 | 5397 | `}` |
|      - | 5398 | `/*` |
|      - | 5399 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5400 | ` *  Parse a CSV string into an array.` |
|      - | 5401 | ` * Parameters` |
|      - | 5402 | ` *  $input` |
|      - | 5403 | ` *   The string to parse.` |
|      - | 5404 | ` *  $delimiter` |
|      - | 5405 | ` *   Set the field delimiter (one character only).` |
|      - | 5406 | ` *  $enclosure` |
|      - | 5407 | ` *   Set the field enclosure character (one character only).` |
|      - | 5408 | ` *  $escape` |
|      - | 5409 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5410 | ` * Return` |
|      - | 5411 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5412 | ` */` |
|      4 | 5413 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5414 | `{` |
|      - | 5415 | `	const char *zInput,*zPtr;` |
|      - | 5416 | `	ph7_value *pArray;` |
|      5 | 5417 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5418 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5419 | `	int escape = '\\';  /* Escape character */` |
|      - | 5420 | `	int nLen;` |
|      5 | 5421 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5422 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5423 | `		ph7_result_null(pCtx);` |
|      3 | 5424 | `		return PH7_OK;` |
|      - | 5425 | `	}` |
|      - | 5426 | `	/* Extract the raw input */` |
|      3 | 5427 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5428 | `	if( nArg > 1 ){` |
|      - | 5429 | `		int i;` |
|      3 | 5430 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5431 | `			/* Extract the delimiter */` |
|      3 | 5432 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5433 | `			if( i > 0 ){` |
|      3 | 5434 | `				delim = zPtr[0];` |
|      1 | 5435 | `			}` |
|      1 | 5436 | `		}` |
|      3 | 5437 | `		if( nArg > 2 ){` |
|      3 | 5438 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5439 | `				/* Extract the enclosure */` |
|      3 | 5440 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5441 | `				if( i > 0 ){` |
|      3 | 5442 | `					encl = zPtr[0];` |
|      1 | 5443 | `				}` |
|      1 | 5444 | `			}` |
|      3 | 5445 | `			if( nArg > 3 ){` |
|      3 | 5446 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5447 | `					/* Extract the escape character */` |
|      3 | 5448 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5449 | `					if( i > 0 ){` |
|      3 | 5450 | `						escape = zPtr[0];` |
|      1 | 5451 | `					}` |
|      1 | 5452 | `				}` |
|      1 | 5453 | `			}` |
|      1 | 5454 | `		}` |
|      1 | 5455 | `	}` |
|      - | 5456 | `	/* Create our array */` |
|      3 | 5457 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5458 | `	if( pArray == 0 ){` |
|      - | 5459 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5460 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5461 | `	}` |
|      - | 5462 | `	/* Parse the raw input */` |
|      3 | 5463 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5464 | `	/* Return the freshly created array */` |
|      3 | 5465 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5466 | `	return PH7_OK;` |
|      3 | 5467 | `}` |
|      - | 5468 | `/*` |
|      - | 5469 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5470 | ` * container.` |
|      - | 5471 | ` * Refer to [strip_tags()].` |
|      - | 5472 | ` */` |
|     10 | 5473 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5474 | `{` |
|     11 | 5475 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5476 | `	const char *zPtr;` |
|      - | 5477 | `	SyString sEntry;` |
|      - | 5478 | `	/* Strip tags */` |
|     10 | 5479 | `	for(;;){` |
|     45 | 5480 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5481 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5482 | `				zTag++;` |
|      1 | 5483 | `		}` |
|     21 | 5484 | `		if( zTag >= zEnd ){` |
|     11 | 5485 | `			break;` |
|      - | 5486 | `		}` |
|     11 | 5487 | `		zPtr = zTag;` |
|      - | 5488 | `		/* Delimit the tag */` |
|     25 | 5489 | `		while(zTag < zEnd ){` |
|     25 | 5490 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5491 | `				/* UTF-8 stream */` |
|      3 | 5492 | `				zTag++;` |
|      5 | 5493 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5494 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5495 | `				break;` |
|    ! 0 | 5496 | `			}else{` |
|     13 | 5497 | `				zTag++;` |
|      - | 5498 | `			}` |
|      1 | 5499 | `		}` |
|     11 | 5500 | `		if( zTag > zPtr ){` |
|      - | 5501 | `			/* Perform the insertion */` |
|     11 | 5502 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5503 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5504 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5505 | `		}` |
|      - | 5506 | `		/* Jump the trailing '>' */` |
|     11 | 5507 | `		zTag++;` |
|      1 | 5508 | `	}` |
|     11 | 5509 | `	return SXRET_OK;` |
|      1 | 5510 | `}` |
|      - | 5511 | `/*` |
|      - | 5512 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5513 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5514 | ` * Refer to [strip_tags()].` |
|      - | 5515 | ` */` |
|     36 | 5516 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5517 | `{` |
|     37 | 5518 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5519 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5520 | `		SyString sTag;` |
|     85 | 5521 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5522 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5523 | `			zTag++;` |
|      1 | 5524 | `		}` |
|      - | 5525 | `		/* Delimit the tag */` |
|     25 | 5526 | `		zCur = zTag;` |
|     77 | 5527 | `		while(zTag < zEnd ){` |
|     77 | 5528 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5529 | `				/* UTF-8 stream */` |
|      5 | 5530 | `				zTag++;` |
|      9 | 5531 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5532 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5533 | `				break;` |
|    ! 0 | 5534 | `			}else{` |
|     49 | 5535 | `				zTag++;` |
|      - | 5536 | `			}` |
|      1 | 5537 | `		}` |
|     25 | 5538 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5539 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5540 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5541 | `		if( sTag.nByte > 0 ){` |
|      - | 5542 | `			SyString *aEntry,*pEntry;` |
|      - | 5543 | `			sxi32 rc;` |
|      - | 5544 | `			sxu32 n;` |
|      - | 5545 | `			/* Perform the lookup */` |
|     25 | 5546 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5547 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5548 | `				pEntry = &aEntry[n];` |
|      - | 5549 | `				/* Do the comparison */` |
|     25 | 5550 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5551 | `				if( !rc ){` |
|     21 | 5552 | `					return SXRET_OK;` |
|      - | 5553 | `				}` |
|      3 | 5554 | `			}` |
|      2 | 5555 | `		}` |
|      2 | 5556 | `	}` |
|      - | 5557 | `	/* No such tag */` |
|     17 | 5558 | `	return SXERR_NOTFOUND;` |
|     19 | 5559 | `}` |
|      - | 5560 | `/*` |
|      - | 5561 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5562 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5563 | ` * Refer to [strip_tags()].` |
|      - | 5564 | ` */` |
|     16 | 5565 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5566 | `{` |
|     17 | 5567 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5568 | `	const char *zPtr,*zTag;` |
|      - | 5569 | `	SySet sSet;` |
|      - | 5570 | `	/* initialize the set of allowed tags */` |
|     17 | 5571 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5572 | `	if( nTaglen > 0 ){` |
|      - | 5573 | `		/* Set of allowed tags */` |
|     11 | 5574 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5575 | `	}` |
|      - | 5576 | `	/* Set the empty string */` |
|     17 | 5577 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5578 | `	/* Start processing */` |
|     26 | 5579 | `	for(;;){` |
|     53 | 5580 | `		if(zIn >= zEnd){` |
|      - | 5581 | `			/* No more input to process */` |
|     15 | 5582 | `			break;` |
|      - | 5583 | `		}` |
|     39 | 5584 | `		zPtr = zIn;` |
|      - | 5585 | `		/* Find a tag */` |
|    133 | 5586 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5587 | `			zIn++;` |
|      1 | 5588 | `		}` |
|     39 | 5589 | `		if( zIn > zPtr ){` |
|      - | 5590 | `			/* Consume raw input */` |
|     21 | 5591 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5592 | `		}` |
|      - | 5593 | `		/* Ignore trailing null bytes */` |
|     39 | 5594 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5595 | `			zIn++;` |
|    ! 0 | 5596 | `		}` |
|     39 | 5597 | `		if(zIn >= zEnd){` |
|      - | 5598 | `			/* No more input to process */` |
|      3 | 5599 | `			break;` |
|      - | 5600 | `		}` |
|     37 | 5601 | `		if( zIn[0] == '<' ){` |
|      - | 5602 | `			sxi32 rc;` |
|     37 | 5603 | `			zTag = zIn++;` |
|      - | 5604 | `			/* Delimit the tag */` |
|    127 | 5605 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5606 | `				zIn++;` |
|      1 | 5607 | `			}` |
|     37 | 5608 | `			if( zIn < zEnd ){` |
|     37 | 5609 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5610 | `			}` |
|      - | 5611 | `			/* Query the set */` |
|     37 | 5612 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5613 | `			if( rc == SXRET_OK ){` |
|      - | 5614 | `				/* Keep the tag */` |
|     21 | 5615 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5616 | `			}` |
|     18 | 5617 | `		}` |
|      1 | 5618 | `	}` |
|      - | 5619 | `	/* Cleanup */` |
|     17 | 5620 | `	SySetRelease(&sSet);` |
|     17 | 5621 | `	return SXRET_OK;` |
|      1 | 5622 | `}` |
|      - | 5623 | `/*` |
|      - | 5624 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5625 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5626 | ` * Parameters` |
|      - | 5627 | ` *  $str` |
|      - | 5628 | ` *  The input string.` |
|      - | 5629 | ` * $allowable_tags` |
|      - | 5630 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5631 | ` * Return` |
|      - | 5632 | ` *  Returns the stripped string.` |
|      - | 5633 | ` */` |
|     16 | 5634 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5635 | `{` |
|     17 | 5636 | `	const char *zTaglist = 0;` |
|      - | 5637 | `	const char *zString;` |
|     17 | 5638 | `	int nTaglen = 0;` |
|      - | 5639 | `	int nLen;` |
|     17 | 5640 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5641 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5642 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5643 | `		return PH7_OK;` |
|      - | 5644 | `	}` |
|      - | 5645 | `	/* Point to the raw string */` |
|     15 | 5646 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5647 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5648 | `		/* Allowed tag */` |
|     11 | 5649 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5650 | `	}` |
|      - | 5651 | `	/* Process input */` |
|     15 | 5652 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5653 | `	return PH7_OK;` |
|      9 | 5654 | `}` |
|      - | 5655 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5656 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5657 | `/*` |
|      - | 5658 | ` * string str_shuffle(string $str)` |
|      - | 5659 |  |
|      - | 5660 | ` *  Randomly shuffles a string.` |
|      - | 5661 | ` * Parameters` |
|      - | 5662 | ` *  $str` |
|      - | 5663 | ` *   The input string.` |
|      - | 5664 | ` * Return` |
|      - | 5665 | ` *  Returns the shuffled string.` |
|      - | 5666 | ` */` |
|     12 | 5667 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5668 | `{` |
|      - | 5669 | `	const char *zString;` |
|      - | 5670 | `	int nLen,i,c;` |
|      - | 5671 | `	sxu32 iR;` |
|     13 | 5672 | `	if( nArg < 1 ){` |
|      - | 5673 | `		/* Missing arguments,return the empty string */` |
|      3 | 5674 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5675 | `		return PH7_OK;` |
|      - | 5676 | `	}` |
|      - | 5677 | `	/* Extract the target string */` |
|     11 | 5678 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5679 | `	if( nLen < 1 ){` |
|      - | 5680 | `		/* Nothing to shuffle */` |
|      3 | 5681 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5682 | `		return PH7_OK;` |
|      - | 5683 | `	}` |
|      - | 5684 | `	/* Shuffle the string */` |
|     43 | 5685 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5686 | `		/* Generate a random number first */` |
|     35 | 5687 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5688 | `		/* Extract a random offset */` |
|     35 | 5689 | `		c = zString[iR % nLen];` |
|      - | 5690 | `		/* Append it */` |
|     35 | 5691 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5692 | `	}` |
|      9 | 5693 | `	return PH7_OK;` |
|      7 | 5694 | `}` |
|      - | 5695 | `/*` |
|      - | 5696 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5697 | ` *  Convert a string to an array.` |
|      - | 5698 | ` * Parameters` |
|      - | 5699 | ` * $string` |
|      - | 5700 | ` *  The input string.` |
|      - | 5701 | ` * $split_length` |
|      - | 5702 | ` *  Maximum length of the chunk.` |
|      - | 5703 | ` * Return` |
|      - | 5704 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5705 | ` *  except possibly the last one which may be shorter.` |
|      - | 5706 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5707 | ` *  as the first (and only) array element.` |
|      - | 5708 | ` *  An empty string returns an empty array.` |
|      - | 5709 | ` * Errors` |
|      - | 5710 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5711 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5712 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5713 | ` */` |
|     28 | 5714 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5715 | `{` |
|      - | 5716 | `	const char *zString,*zEnd;` |
|      - | 5717 | `	ph7_value *pArray,*pValue;` |
|      - | 5718 | `	int split_len;` |
|      - | 5719 | `	int nLen;` |
|     33 | 5720 | `	if( nArg < 1 ){` |
|      4 | 5721 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5722 | `			"ArgumentCountError",` |
|      - | 5723 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5724 | `			nArg` |
|      - | 5725 | `			);` |
|      - | 5726 | `	}` |
|      - | 5727 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 5728 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 5729 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5730 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5731 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5732 | `			"TypeError",` |
|      - | 5733 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5734 | `			ph7_type_name(apArg[0])` |
|      - | 5735 | `			);` |
|      - | 5736 | `	}` |
|      - | 5737 | `	/* Point to the target string */` |
|     27 | 5738 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 5739 | `	split_len = (int)sizeof(char);` |
|     27 | 5740 | `	if( nArg > 1 ){` |
|      - | 5741 | `		/* Split length */` |
|     17 | 5742 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 5743 | `		if( split_len < 1 ){` |
|      6 | 5744 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5745 | `				"ValueError",` |
|      - | 5746 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5747 | `				);` |
|      - | 5748 | `		}` |
|     11 | 5749 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5750 | `			split_len = nLen;` |
|      1 | 5751 | `		}` |
|      5 | 5752 | `	}` |
|      - | 5753 | `	/* Create the array and the scalar value */` |
|     21 | 5754 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5755 | `	/*Chunk value */` |
|     21 | 5756 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 5757 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5758 | `		/* Return FALSE */` |
|    ! 0 | 5759 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5760 | `		return PH7_OK;` |
|      - | 5761 | `	}` |
|      - | 5762 | `	/* Point to the end of the string */` |
|     21 | 5763 | `	zEnd = &zString[nLen];` |
|      - | 5764 | `	/* Perform the requested operation */` |
|     48 | 5765 | `	for(;;){` |
|      - | 5766 | `		int nMax;` |
|     59 | 5767 | `		if( zString >= zEnd ){` |
|      - | 5768 | `			/* No more input to process */` |
|     21 | 5769 | `			break;` |
|      - | 5770 | `		}` |
|     39 | 5771 | `		nMax = (int)(zEnd-zString);` |
|     39 | 5772 | `		if( nMax < split_len ){` |
|      3 | 5773 | `			split_len = nMax;` |
|      1 | 5774 | `		}` |
|      - | 5775 | `		/* Copy the current chunk */` |
|     39 | 5776 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5777 | `		/* Insert it */` |
|     39 | 5778 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 5779 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5780 | `		}` |
|      - | 5781 | `		/* reset the string cursor */` |
|     39 | 5782 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5783 | `		/* Update position */` |
|     39 | 5784 | `		zString += split_len;` |
|      1 | 5785 | `	}` |
|      - | 5786 | `	/*` |
|      - | 5787 | `	 * Return the array.` |
|      - | 5788 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5789 | `	 * upon we return from this function.` |
|      - | 5790 | `	 */` |
|     21 | 5791 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 5792 | `	return PH7_OK;` |
|     19 | 5793 | `}` |
|      - | 5794 | `/*` |
|      - | 5795 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5796 | ` * Refer to [strspn()].` |
|      - | 5797 | ` */` |
|     28 | 5798 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5799 | `{` |
|     29 | 5800 | `	const char *zIn = *pzIn;` |
|      - | 5801 | `	const char *zPtr;` |
|      - | 5802 | `	/* Ignore leading white spaces */` |
|     29 | 5803 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5804 | `		zIn++;` |
|    ! 0 | 5805 | `	}` |
|     29 | 5806 | `	if( zIn >= zEnd ){` |
|      - | 5807 | `		/* End of input */` |
|    ! 0 | 5808 | `		return SXERR_EOF;` |
|      - | 5809 | `	}` |
|     29 | 5810 | `	zPtr = zIn;` |
|      - | 5811 | `	/* Extract the token */` |
|    201 | 5812 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5813 | `		zIn++;` |
|      1 | 5814 | `	}` |
|     29 | 5815 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5816 | `	/* Synchronize pointers */` |
|     29 | 5817 | `	*pzIn = zIn;` |
|      - | 5818 | `	/* Return to the caller */` |
|     29 | 5819 | `	return SXRET_OK;` |
|     15 | 5820 | `}` |
|      - | 5821 | `/*` |
|      - | 5822 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5823 | ` * return the longest match.` |
|      - | 5824 | ` * Refer to [strspn()].` |
|      - | 5825 | ` */` |
|     18 | 5826 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5827 | `{` |
|     19 | 5828 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5829 | `	const char *zIn = zString;` |
|      - | 5830 | `	int i,c;` |
|     45 | 5831 | `	for(;;){` |
|     91 | 5832 | `		if( zString >= zEnd ){` |
|      7 | 5833 | `			break;` |
|      - | 5834 | `		}` |
|      - | 5835 | `		/* Extract current character */` |
|     85 | 5836 | `		c = zString[0];` |
|      - | 5837 | `		/* Perform the lookup */` |
|    383 | 5838 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5839 | `			if( c == zMask[i] ){` |
|      - | 5840 | `				/* Character found */` |
|     73 | 5841 | `				break;` |
|      - | 5842 | `			}` |
|    150 | 5843 | `		}` |
|     85 | 5844 | `		if( i >= nMaskLen ){` |
|      - | 5845 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5846 | `			break;` |
|      - | 5847 | `		}` |
|      - | 5848 | `		/* Advance cursor */` |
|     73 | 5849 | `		zString++;` |
|      1 | 5850 | `	}` |
|      - | 5851 | `	/* Longest match */` |
|     19 | 5852 | `	return (int)(zString-zIn);` |
|      1 | 5853 | `}` |
|      - | 5854 | `/*` |
|      - | 5855 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5856 | ` * Refer to [strcspn()].` |
|      - | 5857 | ` */` |
|     10 | 5858 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5859 | `{` |
|     11 | 5860 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5861 | `	const char *zIn = zString;` |
|      - | 5862 | `	int i,c;` |
|     12 | 5863 | `	for(;;){` |
|     25 | 5864 | `		if( zString >= zEnd ){` |
|      3 | 5865 | `			break;` |
|      - | 5866 | `		}` |
|      - | 5867 | `		/* Extract current character */` |
|     23 | 5868 | `		c = zString[0];` |
|      - | 5869 | `		/* Perform the lookup */` |
|     51 | 5870 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5871 | `			if( c == zMask[i] ){` |
|      9 | 5872 | `				break;` |
|      - | 5873 | `			}` |
|     15 | 5874 | `		}` |
|     23 | 5875 | `		if( i < nMaskLen ){` |
|      - | 5876 | `			/* Character in the current mask,break immediately */` |
|      9 | 5877 | `			break;` |
|      - | 5878 | `		}` |
|      - | 5879 | `		/* Advance cursor */` |
|     15 | 5880 | `		zString++;` |
|      1 | 5881 | `	}` |
|      - | 5882 | `	/* Longest match */` |
|     11 | 5883 | `	return (int)(zString-zIn);` |
|      1 | 5884 | `}` |
|      - | 5885 | `/*` |
|      - | 5886 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5887 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5888 | ` *  of characters contained within a given mask.` |
|      - | 5889 | ` * Parameters` |
|      - | 5890 | ` * $str` |
|      - | 5891 | ` *  The input string.` |
|      - | 5892 | ` * $mask` |
|      - | 5893 | ` *  The list of allowable characters.` |
|      - | 5894 | ` * $start` |
|      - | 5895 | ` *  The position in subject to start searching.` |
|      - | 5896 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5897 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5898 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5899 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5900 | ` *  start'th position from the end of subject.` |
|      - | 5901 | ` * $length` |
|      - | 5902 | ` *  The length of the segment from subject to examine.` |
|      - | 5903 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5904 | ` *  characters after the starting position.` |
|      - | 5905 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5906 | ` *  position up to length characters from the end of subject.` |
|      - | 5907 | ` * Return` |
|      - | 5908 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5909 | ` * in mask.` |
|      - | 5910 | ` */` |
|     26 | 5911 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5912 | `{` |
|      - | 5913 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5914 | `	int iMasklen,iLen;` |
|      - | 5915 | `	SyString sToken;` |
|     27 | 5916 | `	int iCount = 0;` |
|      - | 5917 | `	int rc;` |
|     27 | 5918 | `	if( nArg < 2 ){` |
|      - | 5919 | `		/* Missing agruments,return zero */` |
|      3 | 5920 | `		ph7_result_int(pCtx,0);` |
|      3 | 5921 | `		return PH7_OK;` |
|      - | 5922 | `	}` |
|      - | 5923 | `	/* Extract the target string */` |
|     25 | 5924 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5925 | `	/* Extract the mask */` |
|     25 | 5926 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5927 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5928 | `		/* Nothing to process,return zero */` |
|      7 | 5929 | `		ph7_result_int(pCtx,0);` |
|      7 | 5930 | `		return PH7_OK;` |
|      - | 5931 | `	}` |
|     19 | 5932 | `	if( nArg > 2 ){` |
|      - | 5933 | `		int nOfft;` |
|      - | 5934 | `		/* Extract the offset */` |
|      9 | 5935 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5936 | `		if( nOfft < 0 ){` |
|    ! 0 | 5937 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5938 | `			if( zBase > zString ){` |
|    ! 0 | 5939 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5940 | `				zString = zBase;` |
|    ! 0 | 5941 | `			}else{` |
|      - | 5942 | `				/* Invalid offset */` |
|    ! 0 | 5943 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5944 | `				return PH7_OK;` |
|      - | 5945 | `			}` |
|    ! 0 | 5946 | `		}else{` |
|      9 | 5947 | `			if( nOfft >= iLen ){` |
|      - | 5948 | `				/* Invalid offset */` |
|    ! 0 | 5949 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5950 | `				return PH7_OK;` |
|    ! 0 | 5951 | `			}else{` |
|      - | 5952 | `				/* Update offset */` |
|      9 | 5953 | `				zString += nOfft;` |
|      9 | 5954 | `				iLen -= nOfft;` |
|      - | 5955 | `			}` |
|      - | 5956 | `		}` |
|      9 | 5957 | `		if( nArg > 3 ){` |
|      - | 5958 | `			int iUserlen;` |
|      - | 5959 | `			/* Extract the desired length */` |
|      9 | 5960 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5961 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5962 | `				iLen = iUserlen;` |
|      2 | 5963 | `			}` |
|      4 | 5964 | `		}` |
|      4 | 5965 | `	}` |
|      - | 5966 | `	/* Point to the end of the string */` |
|     19 | 5967 | `	zEnd = &zString[iLen];` |
|      - | 5968 | `	/* Extract the first non-space token */` |
|     19 | 5969 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5970 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5971 | `		/* Compare against the current mask */` |
|     19 | 5972 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5973 | `	}` |
|      - | 5974 | `	/* Longest match */` |
|     19 | 5975 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5976 | `	return PH7_OK;` |
|     14 | 5977 | `}` |
|      - | 5978 | `/*` |
|      - | 5979 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5980 | ` *  Find length of initial segment not matching mask.` |
|      - | 5981 | ` * Parameters` |
|      - | 5982 | ` * $str` |
|      - | 5983 | ` *  The input string.` |
|      - | 5984 | ` * $mask` |
|      - | 5985 | ` *  The list of not allowed characters.` |
|      - | 5986 | ` * $start` |
|      - | 5987 | ` *  The position in subject to start searching.` |
|      - | 5988 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5989 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5990 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5991 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5992 | ` *  start'th position from the end of subject.` |
|      - | 5993 | ` * $length` |
|      - | 5994 | ` *  The length of the segment from subject to examine.` |
|      - | 5995 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5996 | ` *  characters after the starting position.` |
|      - | 5997 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5998 | ` *  position up to length characters from the end of subject.` |
|      - | 5999 | ` * Return` |
|      - | 6000 | ` *  Returns the length of the segment as an integer.` |
|      - | 6001 | ` */` |
|     16 | 6002 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6003 | `{` |
|      - | 6004 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6005 | `	int iMasklen,iLen;` |
|      - | 6006 | `	SyString sToken;` |
|     17 | 6007 | `	int iCount = 0;` |
|      - | 6008 | `	int rc;` |
|     17 | 6009 | `	if( nArg < 2 ){` |
|      - | 6010 | `		/* Missing agruments,return zero */` |
|      3 | 6011 | `		ph7_result_int(pCtx,0);` |
|      3 | 6012 | `		return PH7_OK;` |
|      - | 6013 | `	}` |
|      - | 6014 | `	/* Extract the target string */` |
|     15 | 6015 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6016 | `	/* Extract the mask */` |
|     15 | 6017 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 6018 | `	if( iLen < 1 ){` |
|      - | 6019 | `		/* Nothing to process,return zero */` |
|    ! 0 | 6020 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6021 | `		return PH7_OK;` |
|      - | 6022 | `	}` |
|     15 | 6023 | `	if( iMasklen < 1 ){` |
|      - | 6024 | `		/* No given mask,return the string length */` |
|      3 | 6025 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 6026 | `		return PH7_OK;` |
|      - | 6027 | `	}` |
|     13 | 6028 | `	if( nArg > 2 ){` |
|      - | 6029 | `		int nOfft;` |
|      - | 6030 | `		/* Extract the offset */` |
|     11 | 6031 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 6032 | `		if( nOfft < 0 ){` |
|    ! 0 | 6033 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6034 | `			if( zBase > zString ){` |
|    ! 0 | 6035 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6036 | `				zString = zBase;` |
|    ! 0 | 6037 | `			}else{` |
|      - | 6038 | `				/* Invalid offset */` |
|    ! 0 | 6039 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6040 | `				return PH7_OK;` |
|      - | 6041 | `			}` |
|    ! 0 | 6042 | `		}else{` |
|     11 | 6043 | `			if( nOfft >= iLen ){` |
|      - | 6044 | `				/* Invalid offset */` |
|      3 | 6045 | `				ph7_result_int(pCtx,0);` |
|      3 | 6046 | `				return PH7_OK;` |
|    ! 0 | 6047 | `			}else{` |
|      - | 6048 | `				/* Update offset */` |
|      9 | 6049 | `				zString += nOfft;` |
|      9 | 6050 | `				iLen -= nOfft;` |
|      - | 6051 | `			}` |
|      - | 6052 | `		}` |
|      9 | 6053 | `		if( nArg > 3 ){` |
|      - | 6054 | `			int iUserlen;` |
|      - | 6055 | `			/* Extract the desired length */` |
|    ! 0 | 6056 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 6057 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 6058 | `				iLen = iUserlen;` |
|    ! 0 | 6059 | `			}` |
|    ! 0 | 6060 | `		}` |
|      4 | 6061 | `	}` |
|      - | 6062 | `	/* Point to the end of the string */` |
|     11 | 6063 | `	zEnd = &zString[iLen];` |
|      - | 6064 | `	/* Extract the first non-space token */` |
|     11 | 6065 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 6066 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6067 | `		/* Compare against the current mask */` |
|     11 | 6068 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 6069 | `	}` |
|      - | 6070 | `	/* Longest match */` |
|     11 | 6071 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 6072 | `	return PH7_OK;` |
|      9 | 6073 | `}` |
|      - | 6074 | `/*` |
|      - | 6075 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 6076 | ` *  Search a string for any of a set of characters.` |
|      - | 6077 | ` * Parameters` |
|      - | 6078 | ` *  $haystack` |
|      - | 6079 | ` *   The string where char_list is looked for.` |
|      - | 6080 | ` *  $char_list` |
|      - | 6081 | ` *   This parameter is case sensitive.` |
|      - | 6082 | ` * Return` |
|      - | 6083 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 6084 | ` */` |
|      6 | 6085 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6086 | `{` |
|      - | 6087 | `	const char *zString,*zList,*zEnd;` |
|      - | 6088 | `	int iLen,iListLen,i,c;` |
|      - | 6089 | `	sxu32 nOfft,nMax;` |
|      - | 6090 | `	sxi32 rc;` |
|      7 | 6091 | `	if( nArg < 2 ){` |
|      - | 6092 | `		/* Missing arguments,return FALSE */` |
|      3 | 6093 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6094 | `		return PH7_OK;` |
|      - | 6095 | `	}` |
|      - | 6096 | `	/* Extract the haystack and the char list */` |
|      5 | 6097 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 6098 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 6099 | `	if( iLen < 1 ){` |
|      - | 6100 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6101 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6102 | `		return PH7_OK;` |
|      - | 6103 | `	}` |
|      - | 6104 | `	/* Point to the end of the string */` |
|      5 | 6105 | `	zEnd = &zString[iLen];` |
|      5 | 6106 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6107 | `	/* perform the requested operation */` |
|     15 | 6108 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6109 | `		c = zList[i];` |
|     11 | 6110 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6111 | `		if( rc == SXRET_OK ){` |
|      5 | 6112 | `			if( nMax < nOfft ){` |
|      3 | 6113 | `				nOfft = nMax;` |
|      1 | 6114 | `			}` |
|      2 | 6115 | `		}` |
|      6 | 6116 | `	}` |
|      5 | 6117 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6118 | `		/* No such substring,return FALSE */` |
|      3 | 6119 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6120 | `	}else{` |
|      - | 6121 | `		/* Return the substring */` |
|      3 | 6122 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6123 | `	}` |
|      5 | 6124 | `	return PH7_OK;` |
|      4 | 6125 | `}` |
|      - | 6126 | `/* SPDX-SnippetBegin */` |
|      - | 6127 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6128 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6129 | `/*` |
|      - | 6130 | ` * string soundex(string $str)` |
|      - | 6131 | ` *  Calculate the soundex key of a string.` |
|      - | 6132 | ` * Parameters` |
|      - | 6133 | ` *  $str` |
|      - | 6134 | ` *   The input string.` |
|      - | 6135 | ` * Return` |
|      - | 6136 | ` *  Returns the soundex key as a string.` |
|      - | 6137 | ` * Note:` |
|      - | 6138 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6139 | ` * source tree.` |
|      - | 6140 | ` */` |
|     20 | 6141 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6142 | `{` |
|      - | 6143 | `	const unsigned char *zIn;` |
|      - | 6144 | `	char zResult[8];` |
|      - | 6145 | `	int i, j;` |
|      - | 6146 | `	static const unsigned char iCode[] = {` |
|      - | 6147 |  |
|      - | 6148 |  |
|      - | 6149 |  |
|      - | 6150 |  |
|      - | 6151 |  |
|      - | 6152 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6153 |  |
|      - | 6154 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6155 | `	};` |
|     21 | 6156 | `	if( nArg < 1 ){` |
|      - | 6157 | `		/* Missing arguments,return the empty string */` |
|      3 | 6158 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6159 | `		return PH7_OK;` |
|      - | 6160 | `	}` |
|     19 | 6161 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 6162 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 6163 | `	if( zIn[i] ){` |
|     17 | 6164 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6165 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6166 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6167 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6168 | `			if( code>0 ){` |
|     45 | 6169 | `				if( code!=prevcode ){` |
|     33 | 6170 | `					prevcode = (unsigned char)code;` |
|     33 | 6171 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6172 | `				}` |
|     23 | 6173 | `			}else{` |
|     49 | 6174 | `				prevcode = 0;` |
|      - | 6175 | `			}` |
|     47 | 6176 | `		}` |
|     33 | 6177 | `		while( j<4 ){` |
|     17 | 6178 | `			zResult[j++] = '0';` |
|      1 | 6179 | `		}` |
|     17 | 6180 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6181 | `	}else{` |
|      3 | 6182 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 6183 | `	}` |
|     19 | 6184 | `	return PH7_OK;` |
|     11 | 6185 | `}` |
|      - | 6186 | `/* SPDX-SnippetEnd */` |
|      - | 6187 | `/*` |
|      - | 6188 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6189 | ` *  Wraps a string to a given number of characters.` |
|      - | 6190 | ` * Parameters` |
|      - | 6191 | ` *  $str` |
|      - | 6192 | ` *   The input string.` |
|      - | 6193 | ` * $width` |
|      - | 6194 | ` *  The column width.` |
|      - | 6195 | ` * $break` |
|      - | 6196 | ` *  The line is broken using the optional break parameter.` |
|      - | 6197 | ` * Return` |
|      - | 6198 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6199 | ` */` |
|     14 | 6200 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6201 | `{` |
|      - | 6202 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 6203 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 6204 | `	if( nArg < 1 ){` |
|      - | 6205 | `		/* Missing arguments,return the empty string */` |
|      3 | 6206 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6207 | `		return PH7_OK;` |
|      - | 6208 | `	}` |
|      - | 6209 | `	/* Extract the input string */` |
|     13 | 6210 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 6211 | `	if( iLen < 1 ){` |
|      - | 6212 | `		/* Nothing to process,return the empty string */` |
|      3 | 6213 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6214 | `		return PH7_OK;` |
|      - | 6215 | `	}` |
|      - | 6216 | `	/* Chunk length */` |
|     11 | 6217 | `	iChunk = 75;` |
|     11 | 6218 | `	iBreaklen = 0;` |
|     11 | 6219 | `	zBreak = ""; /* cc warning */` |
|     11 | 6220 | `	if( nArg > 1 ){` |
|     11 | 6221 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 6222 | `		if( iChunk < 1 ){` |
|    ! 0 | 6223 | `			iChunk = 75;` |
|    ! 0 | 6224 | `		}` |
|     11 | 6225 | `		if( nArg > 2 ){` |
|      3 | 6226 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 6227 | `		}` |
|      5 | 6228 | `	}` |
|     11 | 6229 | `	if( iBreaklen < 1 ){` |
|      - | 6230 | `		/* Set a default column break */` |
|      - | 6231 | `#ifdef __WINNT__` |
|      1 | 6232 | `		zBreak = "\r\n";` |
|      1 | 6233 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 6234 | `#else` |
|      8 | 6235 | `		zBreak = "\n";` |
|      8 | 6236 | `		iBreaklen = (int)sizeof(char);` |
|      - | 6237 | `#endif` |
|      4 | 6238 | `	}` |
|      - | 6239 | `	/* Perform the requested operation */` |
|     11 | 6240 | `	zEnd = &zIn[iLen];` |
|     41 | 6241 | `	for(;;){` |
|      - | 6242 | `		int nMax;` |
|     47 | 6243 | `		if( zIn >= zEnd ){` |
|      - | 6244 | `			/* No more input to process */` |
|     11 | 6245 | `			break;` |
|      - | 6246 | `		}` |
|     37 | 6247 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 6248 | `		if( iChunk > nMax ){` |
|     11 | 6249 | `			iChunk = nMax;` |
|      5 | 6250 | `		}` |
|      - | 6251 | `		/* Append the column first */` |
|     37 | 6252 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 6253 | `		/* Advance the cursor */` |
|     37 | 6254 | `		zIn += iChunk;` |
|     37 | 6255 | `		if( zIn < zEnd ){` |
|      - | 6256 | `			/* Append the line break */` |
|     27 | 6257 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 6258 | `		}` |
|      1 | 6259 | `	}` |
|     11 | 6260 | `	return PH7_OK;` |
|      8 | 6261 | `}` |
|      - | 6262 | `/*` |
|      - | 6263 | ` * Check if the given character is a member of the given mask.` |
|      - | 6264 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6265 | ` * Refer to [strtok()].` |
|      - | 6266 | ` */` |
|     30 | 6267 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6268 | `{` |
|      - | 6269 | `	int i;` |
|     57 | 6270 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6271 | `		if( c == zMask[i] ){` |
|     13 | 6272 | `			if( pOfft ){` |
|      5 | 6273 | `				*pOfft = i;` |
|      2 | 6274 | `			}` |
|     13 | 6275 | `			return TRUE;` |
|      - | 6276 | `		}` |
|     14 | 6277 | `	}` |
|     19 | 6278 | `	return FALSE;` |
|     16 | 6279 | `}` |
|      - | 6280 | `/*` |
|      - | 6281 | ` * Extract a single token from the input stream.` |
|      - | 6282 | ` * Refer to [strtok()].` |
|      - | 6283 | ` */` |
|      6 | 6284 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6285 | `{` |
|      7 | 6286 | `	const char *zIn = *pzIn;` |
|      - | 6287 | `	const char *zPtr;` |
|      - | 6288 | `	/* Ignore leading delimiter */` |
|     11 | 6289 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6290 | `		zIn++;` |
|      1 | 6291 | `	}` |
|      7 | 6292 | `	if( zIn >= zEnd ){` |
|      - | 6293 | `		/* End of input */` |
|    ! 0 | 6294 | `		return SXERR_EOF;` |
|      - | 6295 | `	}` |
|      7 | 6296 | `	zPtr = zIn;` |
|      - | 6297 | `	/* Extract the token */` |
|     13 | 6298 | `	while( zIn < zEnd ){` |
|     11 | 6299 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6300 | `			/* UTF-8 stream */` |
|    ! 0 | 6301 | `			zIn++;` |
|    ! 0 | 6302 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6303 | `		}else{` |
|     11 | 6304 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6305 | `				break;` |
|      - | 6306 | `			}` |
|      7 | 6307 | `			zIn++;` |
|      - | 6308 | `		}` |
|      1 | 6309 | `	}` |
|      7 | 6310 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6311 | `	/* Update the cursor */` |
|      7 | 6312 | `	*pzIn = zIn;` |
|      - | 6313 | `	/* Return to the caller */` |
|      7 | 6314 | `	return SXRET_OK;` |
|      4 | 6315 | `}` |
|      - | 6316 | `/* strtok auxiliary private data */` |
|      - | 6317 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6318 | `struct strtok_aux_data` |
|      - | 6319 | `{` |
|      - | 6320 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6321 | `	const char *zIn;   /* Current input stream */` |
|      - | 6322 | `	const char *zEnd;  /* End of input */` |
|      - | 6323 | `};` |
|      - | 6324 | `/*` |
|      - | 6325 | ` * string strtok(string $str,string $token)` |
|      - | 6326 | ` * string strtok(string $token)` |
|      - | 6327 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6328 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6329 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6330 | ` *  words by using the space character as the token.` |
|      - | 6331 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6332 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6333 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6334 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6335 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6336 | ` *  the argument are found.` |
|      - | 6337 | ` * Parameters` |
|      - | 6338 | ` *  $str` |
|      - | 6339 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6340 | ` * $token` |
|      - | 6341 | ` *  The delimiter used when splitting up str.` |
|      - | 6342 | ` * Return` |
|      - | 6343 | ` *   Current token or FALSE on EOF.` |
|      - | 6344 | ` */` |
|      8 | 6345 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6346 | `{` |
|      - | 6347 | `	strtok_aux_data *pAux;` |
|      - | 6348 | `	const char *zMask;` |
|      - | 6349 | `	SyString sToken;` |
|      - | 6350 | `	int nMasklen;` |
|      - | 6351 | `	sxi32 rc;` |
|      9 | 6352 | `	if( nArg < 2 ){` |
|      - | 6353 | `		/* Extract top aux data */` |
|      7 | 6354 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 6355 | `		if( pAux == 0 ){` |
|      - | 6356 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6357 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6358 | `			return PH7_OK;` |
|      - | 6359 | `		}` |
|      7 | 6360 | `		nMasklen = 0;` |
|      7 | 6361 | `		zMask = ""; /* cc warning */` |
|      7 | 6362 | `		if( nArg > 0 ){` |
|      - | 6363 | `			/* Extract the mask */` |
|      5 | 6364 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6365 | `		}` |
|      7 | 6366 | `		if( nMasklen < 1 ){` |
|      - | 6367 | `			/* Invalid mask,return FALSE */` |
|      3 | 6368 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 6369 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 6370 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 6371 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6372 | `			return PH7_OK;` |
|      - | 6373 | `		}` |
|      - | 6374 | `		/* Extract the token */` |
|      5 | 6375 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6376 | `		if( rc != SXRET_OK ){` |
|      - | 6377 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6378 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6379 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6380 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6381 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6382 | `		}else{` |
|      - | 6383 | `			/* Return the extracted token */` |
|      5 | 6384 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6385 | `		}` |
|      3 | 6386 | `	}else{` |
|      - | 6387 | `		const char *zInput,*zCur;` |
|      - | 6388 | `		char *zDup;` |
|      - | 6389 | `		int nLen;` |
|      - | 6390 | `		/* Extract the raw input */` |
|      3 | 6391 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6392 | `		if( nLen < 1 ){` |
|      - | 6393 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6394 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6395 | `			return PH7_OK;` |
|      - | 6396 | `		}` |
|      - | 6397 | `		/* Extract the mask */` |
|      3 | 6398 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6399 | `		if( nMasklen < 1 ){` |
|      - | 6400 | `			/* Set a default mask */` |
|      - | 6401 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6402 | `			zMask = TOK_MASK;` |
|    ! 0 | 6403 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6404 | `#undef TOK_MASK` |
|    ! 0 | 6405 | `		}` |
|      - | 6406 | `		/* Extract a single token */` |
|      3 | 6407 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6408 | `		if( rc != SXRET_OK ){` |
|      - | 6409 | `			/* Empty input */` |
|    ! 0 | 6410 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6411 | `			return PH7_OK;` |
|    ! 0 | 6412 | `		}else{` |
|      - | 6413 | `			/* Return the extracted token */` |
|      3 | 6414 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6415 | `		}` |
|      - | 6416 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6417 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6418 | `		if( pAux ){` |
|      3 | 6419 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6420 | `			if( nLen < 1 ){` |
|    ! 0 | 6421 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6422 | `				return PH7_OK;` |
|      - | 6423 | `			}` |
|      - | 6424 | `			/* Duplicate input */` |
|      3 | 6425 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6426 | `			if( zDup  ){` |
|      3 | 6427 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6428 | `				/* Register the aux data */` |
|      3 | 6429 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6430 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6431 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6432 | `			}` |
|      1 | 6433 | `		}` |
|      - | 6434 | `	}` |
|      7 | 6435 | `	return PH7_OK;` |
|      5 | 6436 | `}` |
|      - | 6437 | `/*` |
|      - | 6438 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6439 | ` *  Pad a string to a certain length with another string` |
|      - | 6440 | ` * Parameters` |
|      - | 6441 | ` *  $input` |
|      - | 6442 | ` *   The input string.` |
|      - | 6443 | ` * $pad_length` |
|      - | 6444 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6445 | ` *   string, no padding takes place.` |
|      - | 6446 | ` * $pad_string` |
|      - | 6447 | ` *   Note:` |
|      - | 6448 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6449 | ` *    divided by the pad_string's length.` |
|      - | 6450 | ` * $pad_type` |
|      - | 6451 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6452 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6453 | ` * Return` |
|      - | 6454 | ` *  The padded string.` |
|      - | 6455 | ` */` |
|     10 | 6456 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6457 | `{` |
|      - | 6458 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6459 | `	const char *zIn,*zPad;` |
|     11 | 6460 | `	if( nArg < 2 ){` |
|      - | 6461 | `		/* Missing arguments,return the empty string */` |
|      5 | 6462 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6463 | `		return PH7_OK;` |
|      - | 6464 | `	}` |
|      - | 6465 | `	/* Extract the target string */` |
|      7 | 6466 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6467 | `	/* Padding length */` |
|      7 | 6468 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6469 | `	if( iPadlen > 0 ){` |
|      5 | 6470 | `		iPadlen -= iLen;` |
|      2 | 6471 | `	}` |
|      7 | 6472 | `	if( iPadlen < 1  ){` |
|      - | 6473 | `		/* Return the string verbatim */` |
|      3 | 6474 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      3 | 6475 | `		return PH7_OK;` |
|      - | 6476 | `	}` |
|      5 | 6477 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6478 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6479 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6480 | `	if( nArg > 2 ){` |
|      - | 6481 | `		/* Padding string */` |
|      5 | 6482 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6483 | `		if( iStrpad < 1 ){` |
|      - | 6484 | `			/* Empty string */` |
|    ! 0 | 6485 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6486 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6487 | `		}` |
|      5 | 6488 | `		if( nArg > 3 ){` |
|      - | 6489 | `			/* Padd type */` |
|      5 | 6490 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6491 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6492 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6493 | `			}` |
|      2 | 6494 | `		}` |
|      2 | 6495 | `	}` |
|      5 | 6496 | `	iDiv = 1;` |
|      5 | 6497 | `	if( iType == 2 ){` |
|    ! 0 | 6498 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6499 | `	}` |
|      - | 6500 | `	/* Perform the requested operation */` |
|      5 | 6501 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6502 | `		jPad = iStrpad;` |
|      5 | 6503 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6504 | `			/* Padding */` |
|      5 | 6505 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6506 | `				break;` |
|      - | 6507 | `			}` |
|      3 | 6508 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6509 | `		}` |
|      3 | 6510 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6511 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6512 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6513 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6514 | `					jPad = iStrpad;` |
|    ! 0 | 6515 | `				}` |
|      3 | 6516 | `				if( jPad < 1){` |
|    ! 0 | 6517 | `					break;` |
|      - | 6518 | `				}` |
|      3 | 6519 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6520 | `			}` |
|      1 | 6521 | `		}` |
|      1 | 6522 | `	}` |
|      5 | 6523 | `	if( iLen > 0 ){` |
|      - | 6524 | `		/* Append the input string */` |
|      5 | 6525 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6526 | `	}` |
|      5 | 6527 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6528 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6529 | `			/* Padding */` |
|      5 | 6530 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6531 | `				break;` |
|      - | 6532 | `			}` |
|      3 | 6533 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6534 | `		}` |
|      5 | 6535 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6536 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6537 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6538 | `				jPad = iStrpad;` |
|    ! 0 | 6539 | `			}` |
|      3 | 6540 | `			if( jPad < 1){` |
|    ! 0 | 6541 | `				break;` |
|      - | 6542 | `			}` |
|      3 | 6543 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6544 | `		}` |
|      1 | 6545 | `	}` |
|      5 | 6546 | `	return PH7_OK;` |
|      6 | 6547 | `}` |
|      - | 6548 | `/*` |
|      - | 6549 | ` * String replacement private data.` |
|      - | 6550 | ` */` |
|      - | 6551 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6552 | `struct str_replace_data` |
|      - | 6553 | `{` |
|      - | 6554 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6555 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6556 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6557 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6558 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6559 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6560 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 6561 | `};` |
|      - | 6562 | `/*` |
|      - | 6563 | ` * Remove a substring.` |
|      - | 6564 | ` */` |
|      - | 6565 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6566 | `	for(;;){\` |
|      - | 6567 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6568 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6569 | `		++OFFT;\` |
|      - | 6570 | `	}\` |
|      - | 6571 | `}` |
|      - | 6572 | `/*` |
|      - | 6573 | ` * Shift right and insert algorithm.` |
|      - | 6574 | ` */` |
|      - | 6575 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6576 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6577 | `		for(;;){\` |
|      - | 6578 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6579 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6580 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6581 | `			--INLEN; \` |
|      - | 6582 | `		}\` |
|      - | 6583 | `		for(;;){\` |
|      - | 6584 | `				if(ELEN < 1) { break; }\` |
|      - | 6585 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6586 | `				OFFT++;\` |
|      - | 6587 | `				ENTRY++;\` |
|      - | 6588 | `				--ELEN;\` |
|      - | 6589 | `		}\` |
|      - | 6590 | `}` |
|      - | 6591 | `/*` |
|      - | 6592 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6593 | ` * replacement string [i.e: zReplace].` |
|      - | 6594 | ` */` |
|     38 | 6595 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6596 | `{` |
|     39 | 6597 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6598 | `	sxu32 n,m;` |
|     39 | 6599 | `	n = SyBlobLength(pWorker);` |
|     39 | 6600 | `	m = nOfft;` |
|      - | 6601 | `	/* Delete the old entry */` |
|    475 | 6602 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6603 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6604 | `	if( nReplen > 0 ){` |
|     33 | 6605 | `		sxi32 iRep = nReplen;` |
|      - | 6606 | `		sxi32 rc;` |
|      - | 6607 | `		/*` |
|      - | 6608 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6609 | `		 * string.` |
|      - | 6610 | `		 */` |
|     33 | 6611 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6612 | `		if( rc != SXRET_OK ){` |
|      - | 6613 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 6614 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 6615 | `			return rc;` |
|      - | 6616 | `		}` |
|      - | 6617 | `		/* Perform the insertion now */` |
|     33 | 6618 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6619 | `		n = SyBlobLength(pWorker);` |
|    163 | 6620 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6621 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6622 | `	}` |
|     39 | 6623 | `	return SXRET_OK;` |
|     20 | 6624 | `}` |
|      - | 6625 | `/*` |
|      - | 6626 | ` * String replacement walker callback.` |
|      - | 6627 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6628 | ` * the replace string.` |
|      - | 6629 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6630 | ` */` |
|      8 | 6631 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6632 | `{` |
|      9 | 6633 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6634 | `	const char *zTarget,*zReplace;` |
|      - | 6635 | `	SyBlob *pWorker;` |
|      - | 6636 | `	int tLen,nLen;` |
|      - | 6637 | `	sxu32 nOfft;` |
|      - | 6638 | `	sxi32 rc;` |
|      - | 6639 | `	/* Point to the working buffer */` |
|      9 | 6640 | `	pWorker = pRepData->pWorker;` |
|      9 | 6641 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6642 | `		/* Target and replace must be a string */` |
|      3 | 6643 | `		return PH7_OK;` |
|      - | 6644 | `	}` |
|      - | 6645 | `	/* Extract the target and the replace */` |
|      7 | 6646 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6647 | `	if( tLen < 1 ){` |
|      - | 6648 | `		/* Empty target,return immediately */` |
|    ! 0 | 6649 | `		return PH7_OK;` |
|      - | 6650 | `	}` |
|      - | 6651 | `	/* Perform a pattern search */` |
|      7 | 6652 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6653 | `	if( rc != SXRET_OK ){` |
|      - | 6654 | `		/* Pattern not found */` |
|    ! 0 | 6655 | `		return PH7_OK;` |
|      - | 6656 | `	}` |
|      - | 6657 | `	/* Extract the replace string */` |
|      7 | 6658 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6659 | `	/* Perform the replace process */` |
|      7 | 6660 | `	rc = StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      7 | 6661 | `	if( rc != SXRET_OK ){` |
|      - | 6662 | `		/* Allocation failure: carry it out and stop the walk */` |
|    ! 0 | 6663 | `		pRepData->rc = rc;` |
|    ! 0 | 6664 | `		return rc;` |
|      - | 6665 | `	}` |
|      - | 6666 | `	/* All done */` |
|      7 | 6667 | `	return PH7_OK;` |
|      5 | 6668 | `}` |
|      - | 6669 | `/*` |
|      - | 6670 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6671 | ` * to collect search/replace string.` |
|      - | 6672 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6673 | ` */` |
|     26 | 6674 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6675 | `{` |
|     27 | 6676 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6677 | `	SyString sWorker;` |
|      - | 6678 | `	const char *zIn;` |
|      - | 6679 | `	int nByte;` |
|      - | 6680 | `	/* Extract a string representation of the given argument */` |
|     27 | 6681 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6682 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6683 | `	if( nByte > 0 ){` |
|      - | 6684 | `		char *zDup;` |
|      - | 6685 | `		/* Duplicate the chunk */` |
|     25 | 6686 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6687 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6688 | `			);` |
|     25 | 6689 | `		if( zDup == 0 ){` |
|      - | 6690 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 6691 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 6692 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 6693 | `			return SXERR_MEM;` |
|      - | 6694 | `		}` |
|     25 | 6695 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6696 | `		/* Save the chunk */` |
|     25 | 6697 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6698 | `	}` |
|      - | 6699 | `	/* Save for later processing */` |
|     27 | 6700 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6701 | `	/* All done */` |
|     13 | 6702 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6703 | `	return PH7_OK;` |
|     14 | 6704 | `}` |
|      - | 6705 | `/*` |
|      - | 6706 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6707 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6708 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6709 | ` * Parameters` |
|      - | 6710 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6711 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6712 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6713 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6714 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6715 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6716 | ` * $search` |
|      - | 6717 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6718 | ` *  to designate multiple needles.` |
|      - | 6719 | ` * $replace` |
|      - | 6720 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6721 | ` *  to designate multiple replacements.` |
|      - | 6722 | ` * $subject` |
|      - | 6723 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6724 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6725 | ` *  of subject, and the return value is an array as well.` |
|      - | 6726 | ` * $count (Not used)` |
|      - | 6727 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6728 | ` * Return` |
|      - | 6729 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6730 | ` */` |
|  24394 | 6731 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6732 | `{` |
|      - | 6733 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6734 | `	ProcStringMatch xMatch;` |
|      - | 6735 | `	const char *zIn,*zFunc;` |
|      - | 6736 | `	str_replace_data sRep;` |
|      - | 6737 | `	SyBlob sWorker;` |
|      - | 6738 | `	SySet sReplace;` |
|      - | 6739 | `	SySet sSearch;` |
|      - | 6740 | `	int rep_str;` |
|      - | 6741 | `	int nByte;` |
|      - | 6742 | `	sxi32 rc;` |
|  24399 | 6743 | `	if( nArg < 3 ){` |
|      - | 6744 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6745 | `		ph7_result_null(pCtx);` |
|      7 | 6746 | `		return PH7_OK;` |
|      - | 6747 | `	}` |
|      - | 6748 | `	/* Initialize fields */` |
|  24393 | 6749 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  24393 | 6750 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  24393 | 6751 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  24393 | 6752 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  24393 | 6753 | `	sRep.pCtx = pCtx;` |
|  24393 | 6754 | `	sRep.pCollector = &sSearch;` |
|  24393 | 6755 | `	rep_str = 0;` |
|      - | 6756 | `	/* Extract the subject */` |
|  24393 | 6757 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  24393 | 6758 | `	if( nByte < 1 ){` |
|      - | 6759 | `		/* Nothing to replace,return the empty string */` |
|     29 | 6760 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 6761 | `		return PH7_OK;` |
|      - | 6762 | `	}` |
|      - | 6763 | `	/* Copy the subject */` |
|  24365 | 6764 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6765 | `	/* Search string */` |
|  24365 | 6766 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6767 | `		/* Collect search string */` |
|      9 | 6768 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6769 | `	}else{` |
|      - | 6770 | `		/* Single pattern */` |
|  24357 | 6771 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  24357 | 6772 | `		if( nByte < 1 ){` |
|      - | 6773 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6774 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6775 | `			return PH7_OK;` |
|      - | 6776 | `		}` |
|  24353 | 6777 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6778 | `		/* Save for later processing */` |
|  24353 | 6779 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6780 | `	}` |
|      - | 6781 | `	/* Replace string */` |
|  24361 | 6782 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6783 | `		/* Collect replace string */` |
|      7 | 6784 | `		sRep.pCollector = &sReplace;` |
|      7 | 6785 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6786 | `	}else{` |
|      - | 6787 | `		/* Single needle */` |
|  24355 | 6788 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  24355 | 6789 | `		rep_str = 1;` |
|  24355 | 6790 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6791 | `		/* Save for later processing */` |
|  24355 | 6792 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6793 | `	}` |
|      - | 6794 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  24361 | 6795 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 6796 | `		SySetRelease(&sSearch);` |
|    ! 0 | 6797 | `		SySetRelease(&sReplace);` |
|    ! 0 | 6798 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 6799 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6800 | `	}` |
|      - | 6801 | `	/* Reset loop cursors */` |
|  24361 | 6802 | `	SySetResetCursor(&sSearch);` |
|  24361 | 6803 | `	SySetResetCursor(&sReplace);` |
|  24361 | 6804 | `	pReplace = pSearch = 0; /* cc warning */` |
|  24361 | 6805 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6806 | `	/* Extract function name */` |
|  24361 | 6807 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6808 | `	/* Set the default pattern match routine */` |
|  24361 | 6809 | `	xMatch = SyBlobSearch;` |
|  24361 | 6810 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6811 | `		/* Case insensitive pattern match */` |
|     11 | 6812 | `		xMatch = iPatternMatch;` |
|      5 | 6813 | `	}` |
|      - | 6814 | `	/* Start the replace process */` |
|  48725 | 6815 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6816 | `		sxu32 nCount,nOfft;` |
|  24369 | 6817 | `		if( pSearch->nByte <  1 ){` |
|      - | 6818 | `			/* Empty string,ignore */` |
|      3 | 6819 | `			continue;` |
|      - | 6820 | `		}` |
|      - | 6821 | `		/* Extract the replace string */` |
|  24367 | 6822 | `		if( rep_str ){` |
|  24357 | 6823 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  12181 | 6824 | `		}else{` |
|     11 | 6825 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6826 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6827 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6828 | `				 */` |
|      3 | 6829 | `				pReplace = 0;` |
|      1 | 6830 | `			}` |
|      - | 6831 | `		}` |
|  24367 | 6832 | `		if( pReplace == 0 ){` |
|      - | 6833 | `			/* Use an empty string instead */` |
|      3 | 6834 | `			pReplace = &sTemp;` |
|      1 | 6835 | `		}` |
|  24367 | 6836 | `		nOfft = nCount = 0;` |
|  12197 | 6837 | `		for(;;){` |
|  24399 | 6838 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6839 | `				break;` |
|      - | 6840 | `			}` |
|      - | 6841 | `			/* Perform a pattern lookup */` |
|  36578 | 6842 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  24382 | 6843 | `				pSearch->nByte,&nOfft);` |
|  24387 | 6844 | `			if( rc != SXRET_OK ){` |
|      - | 6845 | `				/* Pattern not found */` |
|  24355 | 6846 | `				break;` |
|      - | 6847 | `			}` |
|      - | 6848 | `			/* Perform the replace operation */` |
|     33 | 6849 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 6850 | `			if( rc != SXRET_OK ){` |
|      - | 6851 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 6852 | `				SySetRelease(&sSearch);` |
|    ! 0 | 6853 | `				SySetRelease(&sReplace);` |
|    ! 0 | 6854 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 6855 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 6856 | `			}` |
|      - | 6857 | `			/* Increment offset counter */` |
|     33 | 6858 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6859 | `		}` |
|      5 | 6860 | `	}` |
|      - | 6861 | `	/* All done,clean-up the mess left behind */` |
|  24361 | 6862 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  24361 | 6863 | `	SySetRelease(&sSearch);` |
|  24361 | 6864 | `	SySetRelease(&sReplace);` |
|  24361 | 6865 | `	SyBlobRelease(&sWorker);` |
|  24361 | 6866 | `	if( rc != PH7_OK ){` |
|    ! 0 | 6867 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6868 | `	}` |
|  24361 | 6869 | `	return PH7_OK;` |
|  12202 | 6870 | `}` |
|      - | 6871 | `/*` |
|      - | 6872 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6873 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6874 | ` *  Translate characters or replace substrings.` |
|      - | 6875 | ` * Parameters` |
|      - | 6876 | ` *  $str` |
|      - | 6877 | ` *  The string being translated.` |
|      - | 6878 | ` * $from` |
|      - | 6879 | ` *  The string being translated to to.` |
|      - | 6880 | ` * $to` |
|      - | 6881 | ` *  The string replacing from.` |
|      - | 6882 | ` * $replace_pairs` |
|      - | 6883 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6884 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6885 | ` * Return` |
|      - | 6886 | ` *  The translated string.` |
|      - | 6887 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6888 | ` */` |
|     12 | 6889 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6890 | `{` |
|      - | 6891 | `	const char *zIn;` |
|      - | 6892 | `	int nLen;` |
|     13 | 6893 | `	if( nArg < 1 ){` |
|      - | 6894 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6895 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6896 | `		return PH7_OK;` |
|      - | 6897 | `	}` |
|      7 | 6898 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6899 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6900 | `		/* Invalid arguments */` |
|    ! 0 | 6901 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6902 | `		return PH7_OK;` |
|      - | 6903 | `	}` |
|      9 | 6904 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6905 | `		str_replace_data sRepData;` |
|      - | 6906 | `		SyBlob sWorker;` |
|      - | 6907 | `		sxi32 rc;` |
|      - | 6908 | `		/* Initilaize the working buffer */` |
|      5 | 6909 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6910 | `		/* Copy raw string */` |
|      5 | 6911 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6912 | `		/* Init our replace data instance */` |
|      5 | 6913 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6914 | `		sRepData.xMatch = SyBlobSearch;` |
|      5 | 6915 | `		sRepData.rc = SXRET_OK;` |
|      - | 6916 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6917 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      5 | 6918 | `		if( sRepData.rc != SXRET_OK ){` |
|      - | 6919 | `			/* Allocation failure during replacement: surface a fatal */` |
|    ! 0 | 6920 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 6921 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6922 | `		}` |
|      - | 6923 | `		/* All done, return the result string */` |
|      7 | 6924 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6925 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6926 | `		/* Clean-up */` |
|      5 | 6927 | `		SyBlobRelease(&sWorker);` |
|      5 | 6928 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6929 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6930 | `		}` |
|      3 | 6931 | `	}else{` |
|      - | 6932 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6933 | `		const char *zFrom,*zTo;` |
|      3 | 6934 | `		if( nArg < 3 ){` |
|      - | 6935 | `			/* Nothing to replace */` |
|    ! 0 | 6936 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6937 | `			return PH7_OK;` |
|      - | 6938 | `		}` |
|      - | 6939 | `		/* Extract given arguments */` |
|      3 | 6940 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6941 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6942 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6943 | `			/* Nothing to replace */` |
|    ! 0 | 6944 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6945 | `			return PH7_OK;` |
|      - | 6946 | `		}` |
|      - | 6947 | `		/* Start the replace process */` |
|     13 | 6948 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6949 | `			c = zIn[i];` |
|     11 | 6950 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6951 | `				if ( iOfft < tlen ){` |
|      5 | 6952 | `					c = zTo[iOfft];` |
|      2 | 6953 | `				}` |
|      2 | 6954 | `			}` |
|     11 | 6955 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6956 |  |
|      6 | 6957 | `		}` |
|      - | 6958 | `	}` |
|      7 | 6959 | `	return PH7_OK;` |
|      7 | 6960 | `}` |
|      - | 6961 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6962 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6963 | `/*` |
|      - | 6964 | ` * Parse an INI string.` |
|      - | 6965 |  |
|      - | 6966 | ` * According to wikipedia` |
|      - | 6967 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6968 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6969 | ` *  Format` |
|      - | 6970 | `*    Properties` |
|      - | 6971 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6972 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6973 | `*     Example:` |
|      - | 6974 | `*      name=value` |
|      - | 6975 | `*    Sections` |
|      - | 6976 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6977 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6978 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6979 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6980 | `*     Example:` |
|      - | 6981 | `*      [section]` |
|      - | 6982 | `*   Comments` |
|      - | 6983 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6984 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6985 | `*/` |
|     12 | 6986 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6987 | `{` |
|      - | 6988 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6989 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6990 | `	SyHashEntry *pEntry;` |
|      - | 6991 | `	SyString sEntry;` |
|      - | 6992 | `	SyHash sHash;` |
|      - | 6993 | `	int c;` |
|      - | 6994 | `	/* Create an empty array and worker variables */` |
|     13 | 6995 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6996 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6997 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6998 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6999 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 7000 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7001 | `	}` |
|     13 | 7002 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 7003 | `	pCur = pArray;` |
|      - | 7004 | `	/* Start the parse process */` |
|     21 | 7005 | `	for(;;){` |
|      - | 7006 | `		/* Ignore leading white spaces */` |
|     69 | 7007 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 7008 | `			zIn++;` |
|      1 | 7009 | `		}` |
|     43 | 7010 | `		if( zIn >= zEnd ){` |
|      - | 7011 | `			/* No more input to process */` |
|     13 | 7012 | `			break;` |
|      - | 7013 | `		}` |
|     31 | 7014 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7015 | `			/* Comment til the end of line */` |
|    ! 0 | 7016 | `			zIn++;` |
|    ! 0 | 7017 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 7018 | `				zIn++;` |
|    ! 0 | 7019 | `			}` |
|    ! 0 | 7020 | `			continue;` |
|      - | 7021 | `		}` |
|      - | 7022 | `		/* Reset the string cursor of the working variable */` |
|     31 | 7023 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 7024 | `		if( zIn[0] == '[' ){` |
|      - | 7025 | `			/* Section: Extract the section name */` |
|      9 | 7026 | `			zIn++;` |
|      9 | 7027 | `			zCur = zIn;` |
|     73 | 7028 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 7029 | `				zIn++;` |
|      1 | 7030 | `			}` |
|      9 | 7031 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 7032 | `				/* Save the section name */` |
|      5 | 7033 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 7034 | `				SyStringFullTrim(&sEntry);` |
|      5 | 7035 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 7036 | `				if( sEntry.nByte > 0 ){` |
|      - | 7037 | `					/* Associate an array with the section */` |
|      5 | 7038 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 7039 | `					if( pSection ){` |
|      5 | 7040 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 7041 | `						pCur = pSection;` |
|      2 | 7042 | `					}` |
|      2 | 7043 | `				}` |
|      2 | 7044 | `			}` |
|      9 | 7045 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 7046 | `		}else{` |
|      - | 7047 | `			ph7_value *pOldCur;` |
|      - | 7048 | `			int is_array;` |
|      - | 7049 | `			int iLen;` |
|      - | 7050 | `			/* Properties */` |
|     23 | 7051 | `			is_array = 0;` |
|     23 | 7052 | `			zCur = zIn;` |
|     23 | 7053 | `			iLen = 0; /* cc warning */` |
|     23 | 7054 | `			pOldCur = pCur;` |
|    155 | 7055 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 7056 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 7057 | `					/* Array */` |
|    ! 0 | 7058 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 7059 | `					is_array = 1;` |
|    ! 0 | 7060 | `					if( iLen > 0 ){` |
|    ! 0 | 7061 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 7062 | `						/* Query the hashtable */` |
|    ! 0 | 7063 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 7064 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 7065 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 7066 | `						if( pEntry ){` |
|    ! 0 | 7067 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 7068 | `						}else{` |
|      - | 7069 | `							/* Create an empty array */` |
|    ! 0 | 7070 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 7071 | `							if( pvArr ){` |
|      - | 7072 | `								/* Save the entry */` |
|    ! 0 | 7073 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 7074 | `								/* Insert the entry */` |
|    ! 0 | 7075 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7076 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 7077 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 7078 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7079 | `							}` |
|      - | 7080 | `						}` |
|    ! 0 | 7081 | `						if( pvArr ){` |
|    ! 0 | 7082 | `							pCur = pvArr;` |
|    ! 0 | 7083 | `						}` |
|    ! 0 | 7084 | `					}` |
|    ! 0 | 7085 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 7086 | `						zIn++;` |
|    ! 0 | 7087 | `					}` |
|    ! 0 | 7088 | `				}` |
|    133 | 7089 | `				zIn++;` |
|      1 | 7090 | `			}` |
|     23 | 7091 | `			if( !is_array ){` |
|     23 | 7092 | `				iLen = (int)(zIn-zCur);` |
|     11 | 7093 | `			}` |
|      - | 7094 | `			/* Trim the key */` |
|     23 | 7095 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 7096 | `			SyStringFullTrim(&sEntry);` |
|     23 | 7097 | `			if( sEntry.nByte > 0 ){` |
|     23 | 7098 | `				if( !is_array ){` |
|      - | 7099 | `					/* Save the key name */` |
|     23 | 7100 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 7101 | `				}` |
|      - | 7102 | `				/* extract key value */` |
|     23 | 7103 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7104 | `				zIn++; /* '=' */` |
|     39 | 7105 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7106 | `					zIn++;` |
|      1 | 7107 | `				}` |
|     23 | 7108 | `				if( zIn < zEnd ){` |
|     21 | 7109 | `					zCur = zIn;` |
|     21 | 7110 | `					c = zIn[0];` |
|     21 | 7111 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7112 | `						zIn++;` |
|      - | 7113 | `						/* Delimit the value */` |
|    ! 0 | 7114 | `						while( zIn < zEnd ){` |
|    ! 0 | 7115 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7116 | `								break;` |
|      - | 7117 | `							}` |
|    ! 0 | 7118 | `							zIn++;` |
|    ! 0 | 7119 | `						}` |
|    ! 0 | 7120 | `						if( zIn < zEnd ){` |
|    ! 0 | 7121 | `							zIn++;` |
|    ! 0 | 7122 | `						}` |
|    ! 0 | 7123 | `					}else{` |
|    125 | 7124 | `						while( zIn < zEnd ){` |
|    123 | 7125 | `							if( zIn[0] == '\n' ){` |
|     19 | 7126 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7127 | `									break;` |
|    ! 0 | 7128 | `								}` |
|    105 | 7129 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7130 | `								/* Inline comments */` |
|    ! 0 | 7131 | `								break;` |
|      - | 7132 | `							}` |
|    105 | 7133 | `							zIn++;` |
|      1 | 7134 | `						}` |
|      - | 7135 | `					}` |
|      - | 7136 | `					/* Trim the value */` |
|     21 | 7137 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7138 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7139 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7140 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7141 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7142 | `					}` |
|     21 | 7143 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7144 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7145 | `					}` |
|      - | 7146 | `					/* Insert the key and it's value */` |
|     21 | 7147 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7148 | `				}` |
|     12 | 7149 | `			}else{` |
|    ! 0 | 7150 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7151 | `					zIn++;` |
|    ! 0 | 7152 | `				}` |
|      - | 7153 | `			}` |
|     23 | 7154 | `			pCur = pOldCur;` |
|      - | 7155 | `		}` |
|      1 | 7156 | `	}` |
|     13 | 7157 | `	SyHashRelease(&sHash);` |
|      - | 7158 | `	/* Return the parse of the INI string */` |
|     13 | 7159 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7160 | `	return SXRET_OK;` |
|      7 | 7161 | `}` |
|      - | 7162 | `/*` |
|      - | 7163 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7164 | ` *  Parse a configuration string.` |
|      - | 7165 | ` * Parameters` |
|      - | 7166 | ` *  $ini` |
|      - | 7167 | ` *   The contents of the ini file being parsed.` |
|      - | 7168 | ` *  $process_sections` |
|      - | 7169 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7170 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7171 | ` *  $scanner_mode (Not used)` |
|      - | 7172 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7173 | ` *   then option values will not be parsed.` |
|      - | 7174 | ` * Return` |
|      - | 7175 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7176 | ` */` |
|     10 | 7177 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7178 | `{` |
|      - | 7179 | `	const char *zIni;` |
|      - | 7180 | `	int nByte;` |
|     11 | 7181 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7182 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7183 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7184 | `		return PH7_OK;` |
|      - | 7185 | `	}` |
|      - | 7186 | `	/* Extract the raw INI buffer */` |
|     11 | 7187 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7188 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7189 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7190 | `}` |
|      - | 7191 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7192 |  |
|      - | 7193 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7194 |  |
|      - | 7195 | `/*` |
|      - | 7196 | ` * Ctype Functions.` |
|      - | 7197 | ` * Status:` |
|      - | 7198 | ` *    Stable.` |
|      - | 7199 | ` */` |
|      - | 7200 | `/*` |
|      - | 7201 | ` * bool ctype_alnum(string $text)` |
|      - | 7202 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7203 | ` * Parameters` |
|      - | 7204 | ` *  $text` |
|      - | 7205 | ` *   The tested string.` |
|      - | 7206 | ` * Return` |
|      - | 7207 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7208 | ` */` |
|     16 | 7209 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7210 | `{` |
|      - | 7211 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7212 | `	int nLen;` |
|     17 | 7213 | `	if( nArg < 1 ){` |
|      - | 7214 | `		/* Missing arguments,return FALSE */` |
|      3 | 7215 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7216 | `		return PH7_OK;` |
|      - | 7217 | `	}` |
|      - | 7218 | `	/* Extract the target string */` |
|     15 | 7219 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7220 | `	zEnd = &zIn[nLen];` |
|     15 | 7221 | `	if( nLen < 1 ){` |
|      - | 7222 | `		/* Empty string,return FALSE */` |
|      3 | 7223 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7224 | `		return PH7_OK;` |
|      - | 7225 | `	}` |
|      - | 7226 | `	/* Perform the requested operation */` |
|     32 | 7227 | `	for(;;){` |
|     65 | 7228 | `		if( zIn >= zEnd ){` |
|      - | 7229 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7230 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7231 | `			return PH7_OK;` |
|      - | 7232 | `		}` |
|     57 | 7233 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7234 | `			break;` |
|      - | 7235 | `		}` |
|      - | 7236 | `		/* Point to the next character */` |
|     53 | 7237 | `		zIn++;` |
|      1 | 7238 | `	}` |
|      - | 7239 | `	/* The test failed,return FALSE */` |
|      5 | 7240 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7241 | `	return PH7_OK;` |
|      9 | 7242 | `}` |
|      - | 7243 | `/*` |
|      - | 7244 | ` * bool ctype_alpha(string $text)` |
|      - | 7245 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7246 | ` * Parameters` |
|      - | 7247 | ` *  $text` |
|      - | 7248 | ` *   The tested string.` |
|      - | 7249 | ` * Return` |
|      - | 7250 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7251 | ` */` |
|     18 | 7252 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7253 | `{` |
|      - | 7254 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7255 | `	int nLen;` |
|     19 | 7256 | `	if( nArg < 1 ){` |
|      - | 7257 | `		/* Missing arguments,return FALSE */` |
|      3 | 7258 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7259 | `		return PH7_OK;` |
|      - | 7260 | `	}` |
|      - | 7261 | `	/* Extract the target string */` |
|     17 | 7262 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7263 | `	zEnd = &zIn[nLen];` |
|     17 | 7264 | `	if( nLen < 1 ){` |
|      - | 7265 | `		/* Empty string,return FALSE */` |
|      3 | 7266 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7267 | `		return PH7_OK;` |
|      - | 7268 | `	}` |
|      - | 7269 | `	/* Perform the requested operation */` |
|     42 | 7270 | `	for(;;){` |
|     85 | 7271 | `		if( zIn >= zEnd ){` |
|      - | 7272 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7273 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7274 | `			return PH7_OK;` |
|      - | 7275 | `		}` |
|     77 | 7276 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7277 | `			break;` |
|      - | 7278 | `		}` |
|      - | 7279 | `		/* Point to the next character */` |
|     71 | 7280 | `		zIn++;` |
|      1 | 7281 | `	}` |
|      - | 7282 | `	/* The test failed,return FALSE */` |
|      7 | 7283 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7284 | `	return PH7_OK;` |
|     10 | 7285 | `}` |
|      - | 7286 | `/*` |
|      - | 7287 | ` * bool ctype_cntrl(string $text)` |
|      - | 7288 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7289 | ` * Parameters` |
|      - | 7290 | ` *  $text` |
|      - | 7291 | ` *   The tested string.` |
|      - | 7292 | ` * Return` |
|      - | 7293 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7294 | ` */` |
|     18 | 7295 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7296 | `{` |
|      - | 7297 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7298 | `	int nLen;` |
|     19 | 7299 | `	if( nArg < 1 ){` |
|      - | 7300 | `		/* Missing arguments,return FALSE */` |
|      3 | 7301 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7302 | `		return PH7_OK;` |
|      - | 7303 | `	}` |
|      - | 7304 | `	/* Extract the target string */` |
|     17 | 7305 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7306 | `	zEnd = &zIn[nLen];` |
|     17 | 7307 | `	if( nLen < 1 ){` |
|      - | 7308 | `		/* Empty string,return FALSE */` |
|      3 | 7309 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7310 | `		return PH7_OK;` |
|      - | 7311 | `	}` |
|      - | 7312 | `	/* Perform the requested operation */` |
|     14 | 7313 | `	for(;;){` |
|     29 | 7314 | `		if( zIn >= zEnd ){` |
|      - | 7315 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7316 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7317 | `			return PH7_OK;` |
|      - | 7318 | `		}` |
|     21 | 7319 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7320 | `			/* UTF-8 stream  */` |
|    ! 0 | 7321 | `			break;` |
|      - | 7322 | `		}` |
|     21 | 7323 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7324 | `			break;` |
|      - | 7325 | `		}` |
|      - | 7326 | `		/* Point to the next character */` |
|     15 | 7327 | `		zIn++;` |
|      1 | 7328 | `	}` |
|      - | 7329 | `	/* The test failed,return FALSE */` |
|      7 | 7330 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7331 | `	return PH7_OK;` |
|     10 | 7332 | `}` |
|      - | 7333 | `/*` |
|      - | 7334 | ` * bool ctype_digit(string $text)` |
|      - | 7335 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7336 | ` * Parameters` |
|      - | 7337 | ` *  $text` |
|      - | 7338 | ` *   The tested string.` |
|      - | 7339 | ` * Return` |
|      - | 7340 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7341 | ` */` |
|   1638 | 7342 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7343 | `{` |
|      - | 7344 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7345 | `	int nLen;` |
|   1643 | 7346 | `	if( nArg < 1 ){` |
|      - | 7347 | `		/* Missing arguments,return FALSE */` |
|      3 | 7348 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7349 | `		return PH7_OK;` |
|      - | 7350 | `	}` |
|      - | 7351 | `	/* Extract the target string */` |
|   1641 | 7352 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1641 | 7353 | `	zEnd = &zIn[nLen];` |
|   1641 | 7354 | `	if( nLen < 1 ){` |
|      - | 7355 | `		/* Empty string,return FALSE */` |
|      3 | 7356 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7357 | `		return PH7_OK;` |
|      - | 7358 | `	}` |
|      - | 7359 | `	/* Perform the requested operation */` |
|   1538 | 7360 | `	for(;;){` |
|   3081 | 7361 | `		if( zIn >= zEnd ){` |
|      - | 7362 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1397 | 7363 | `			ph7_result_bool(pCtx,1);` |
|   1397 | 7364 | `			return PH7_OK;` |
|      - | 7365 | `		}` |
|   1689 | 7366 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7367 | `			/* UTF-8 stream  */` |
|    ! 0 | 7368 | `			break;` |
|      - | 7369 | `		}` |
|   1689 | 7370 | `		if( !SyisDigit(zIn[0]) ){` |
|    247 | 7371 | `			break;` |
|      - | 7372 | `		}` |
|      - | 7373 | `		/* Point to the next character */` |
|   1447 | 7374 | `		zIn++;` |
|      5 | 7375 | `	}` |
|      - | 7376 | `	/* The test failed,return FALSE */` |
|    247 | 7377 | `	ph7_result_bool(pCtx,0);` |
|    247 | 7378 | `	return PH7_OK;` |
|    824 | 7379 | `}` |
|      - | 7380 | `/*` |
|      - | 7381 | ` * bool ctype_xdigit(string $text)` |
|      - | 7382 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7383 | ` * Parameters` |
|      - | 7384 | ` *  $text` |
|      - | 7385 | ` *   The tested string.` |
|      - | 7386 | ` * Return` |
|      - | 7387 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7388 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7389 | ` */` |
|     20 | 7390 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7391 | `{` |
|      - | 7392 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7393 | `	int nLen;` |
|     21 | 7394 | `	if( nArg < 1 ){` |
|      - | 7395 | `		/* Missing arguments,return FALSE */` |
|      3 | 7396 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7397 | `		return PH7_OK;` |
|      - | 7398 | `	}` |
|      - | 7399 | `	/* Extract the target string */` |
|     19 | 7400 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7401 | `	zEnd = &zIn[nLen];` |
|     19 | 7402 | `	if( nLen < 1 ){` |
|      - | 7403 | `		/* Empty string,return FALSE */` |
|      3 | 7404 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7405 | `		return PH7_OK;` |
|      - | 7406 | `	}` |
|      - | 7407 | `	/* Perform the requested operation */` |
|     46 | 7408 | `	for(;;){` |
|     93 | 7409 | `		if( zIn >= zEnd ){` |
|      - | 7410 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7411 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7412 | `			return PH7_OK;` |
|      - | 7413 | `		}` |
|     83 | 7414 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7415 | `			/* UTF-8 stream  */` |
|    ! 0 | 7416 | `			break;` |
|      - | 7417 | `		}` |
|     83 | 7418 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7419 | `			break;` |
|      - | 7420 | `		}` |
|      - | 7421 | `		/* Point to the next character */` |
|     77 | 7422 | `		zIn++;` |
|      1 | 7423 | `	}` |
|      - | 7424 | `	/* The test failed,return FALSE */` |
|      7 | 7425 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7426 | `	return PH7_OK;` |
|     11 | 7427 | `}` |
|      - | 7428 | `/*` |
|      - | 7429 | ` * bool ctype_graph(string $text)` |
|      - | 7430 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7431 | ` * Parameters` |
|      - | 7432 | ` *  $text` |
|      - | 7433 | ` *   The tested string.` |
|      - | 7434 | ` * Return` |
|      - | 7435 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7436 | ` * (no white space), FALSE otherwise.` |
|      - | 7437 | ` */` |
|     18 | 7438 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7439 | `{` |
|      - | 7440 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7441 | `	int nLen;` |
|     19 | 7442 | `	if( nArg < 1 ){` |
|      - | 7443 | `		/* Missing arguments,return FALSE */` |
|      3 | 7444 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7445 | `		return PH7_OK;` |
|      - | 7446 | `	}` |
|      - | 7447 | `	/* Extract the target string */` |
|     17 | 7448 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7449 | `	zEnd = &zIn[nLen];` |
|     17 | 7450 | `	if( nLen < 1 ){` |
|      - | 7451 | `		/* Empty string,return FALSE */` |
|      3 | 7452 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7453 | `		return PH7_OK;` |
|      - | 7454 | `	}` |
|      - | 7455 | `	/* Perform the requested operation */` |
|     57 | 7456 | `	for(;;){` |
|    115 | 7457 | `		if( zIn >= zEnd ){` |
|      - | 7458 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7459 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7460 | `			return PH7_OK;` |
|      - | 7461 | `		}` |
|    107 | 7462 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7463 | `			/* UTF-8 stream  */` |
|    ! 0 | 7464 | `			break;` |
|      - | 7465 | `		}` |
|    107 | 7466 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7467 | `			break;` |
|      - | 7468 | `		}` |
|      - | 7469 | `		/* Point to the next character */` |
|    101 | 7470 | `		zIn++;` |
|      1 | 7471 | `	}` |
|      - | 7472 | `	/* The test failed,return FALSE */` |
|      7 | 7473 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7474 | `	return PH7_OK;` |
|     10 | 7475 | `}` |
|      - | 7476 | `/*` |
|      - | 7477 | ` * bool ctype_print(string $text)` |
|      - | 7478 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7479 | ` * Parameters` |
|      - | 7480 | ` *  $text` |
|      - | 7481 | ` *   The tested string.` |
|      - | 7482 | ` * Return` |
|      - | 7483 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7484 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7485 | ` *  or control function at all.` |
|      - | 7486 | ` */` |
|     18 | 7487 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7488 | `{` |
|      - | 7489 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7490 | `	int nLen;` |
|     19 | 7491 | `	if( nArg < 1 ){` |
|      - | 7492 | `		/* Missing arguments,return FALSE */` |
|      3 | 7493 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7494 | `		return PH7_OK;` |
|      - | 7495 | `	}` |
|      - | 7496 | `	/* Extract the target string */` |
|     17 | 7497 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7498 | `	zEnd = &zIn[nLen];` |
|     17 | 7499 | `	if( nLen < 1 ){` |
|      - | 7500 | `		/* Empty string,return FALSE */` |
|      3 | 7501 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7502 | `		return PH7_OK;` |
|      - | 7503 | `	}` |
|      - | 7504 | `	/* Perform the requested operation */` |
|     63 | 7505 | `	for(;;){` |
|    127 | 7506 | `		if( zIn >= zEnd ){` |
|      - | 7507 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7508 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7509 | `			return PH7_OK;` |
|      - | 7510 | `		}` |
|    119 | 7511 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7512 | `			/* UTF-8 stream  */` |
|    ! 0 | 7513 | `			break;` |
|      - | 7514 | `		}` |
|    119 | 7515 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7516 | `			break;` |
|      - | 7517 | `		}` |
|      - | 7518 | `		/* Point to the next character */` |
|    113 | 7519 | `		zIn++;` |
|      1 | 7520 | `	}` |
|      - | 7521 | `	/* The test failed,return FALSE */` |
|      7 | 7522 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7523 | `	return PH7_OK;` |
|     10 | 7524 | `}` |
|      - | 7525 | `/*` |
|      - | 7526 | ` * bool ctype_punct(string $text)` |
|      - | 7527 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7528 | ` * Parameters` |
|      - | 7529 | ` *  $text` |
|      - | 7530 | ` *   The tested string.` |
|      - | 7531 | ` * Return` |
|      - | 7532 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7533 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7534 | ` */` |
|     20 | 7535 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7536 | `{` |
|      - | 7537 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7538 | `	int nLen;` |
|     21 | 7539 | `	if( nArg < 1 ){` |
|      - | 7540 | `		/* Missing arguments,return FALSE */` |
|      3 | 7541 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7542 | `		return PH7_OK;` |
|      - | 7543 | `	}` |
|      - | 7544 | `	/* Extract the target string */` |
|     19 | 7545 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7546 | `	zEnd = &zIn[nLen];` |
|     19 | 7547 | `	if( nLen < 1 ){` |
|      - | 7548 | `		/* Empty string,return FALSE */` |
|      3 | 7549 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7550 | `		return PH7_OK;` |
|      - | 7551 | `	}` |
|      - | 7552 | `	/* Perform the requested operation */` |
|     38 | 7553 | `	for(;;){` |
|     77 | 7554 | `		if( zIn >= zEnd ){` |
|      - | 7555 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7556 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7557 | `			return PH7_OK;` |
|      - | 7558 | `		}` |
|     69 | 7559 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7560 | `			/* UTF-8 stream  */` |
|    ! 0 | 7561 | `			break;` |
|      - | 7562 | `		}` |
|     69 | 7563 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7564 | `			break;` |
|      - | 7565 | `		}` |
|      - | 7566 | `		/* Point to the next character */` |
|     61 | 7567 | `		zIn++;` |
|      1 | 7568 | `	}` |
|      - | 7569 | `	/* The test failed,return FALSE */` |
|      9 | 7570 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7571 | `	return PH7_OK;` |
|     11 | 7572 | `}` |
|      - | 7573 | `/*` |
|      - | 7574 | ` * bool ctype_space(string $text)` |
|      - | 7575 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7576 | ` * Parameters` |
|      - | 7577 | ` *  $text` |
|      - | 7578 | ` *   The tested string.` |
|      - | 7579 | ` * Return` |
|      - | 7580 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7581 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7582 | ` *  and form feed characters.` |
|      - | 7583 | ` */` |
|  62045 | 7584 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7585 | `{` |
|      - | 7586 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7587 | `	int nLen;` |
|  62050 | 7588 | `	if( nArg < 1 ){` |
|      - | 7589 | `		/* Missing arguments,return FALSE */` |
|      3 | 7590 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7591 | `		return PH7_OK;` |
|      - | 7592 | `	}` |
|      - | 7593 | `	/* Extract the target string */` |
|  62048 | 7594 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  62048 | 7595 | `	zEnd = &zIn[nLen];` |
|  62048 | 7596 | `	if( nLen < 1 ){` |
|      - | 7597 | `		/* Empty string,return FALSE */` |
|      3 | 7598 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7599 | `		return PH7_OK;` |
|      - | 7600 | `	}` |
|      - | 7601 | `	/* Perform the requested operation */` |
|  32128 | 7602 | `	for(;;){` |
|  64176 | 7603 | `		if( zIn >= zEnd ){` |
|      - | 7604 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2111 | 7605 | `			ph7_result_bool(pCtx,1);` |
|   2111 | 7606 | `			return PH7_OK;` |
|      - | 7607 | `		}` |
|  62070 | 7608 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7609 | `			/* UTF-8 stream  */` |
|    ! 0 | 7610 | `			break;` |
|      - | 7611 | `		}` |
|  62070 | 7612 | `		if( !SyisSpace(zIn[0]) ){` |
|  59940 | 7613 | `			break;` |
|      - | 7614 | `		}` |
|      - | 7615 | `		/* Point to the next character */` |
|   2135 | 7616 | `		zIn++;` |
|      5 | 7617 | `	}` |
|      - | 7618 | `	/* The test failed,return FALSE */` |
|  59940 | 7619 | `	ph7_result_bool(pCtx,0);` |
|  59940 | 7620 | `	return PH7_OK;` |
|  31070 | 7621 | `}` |
|      - | 7622 | `/*` |
|      - | 7623 | ` * bool ctype_lower(string $text)` |
|      - | 7624 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7625 | ` * Parameters` |
|      - | 7626 | ` *  $text` |
|      - | 7627 | ` *   The tested string.` |
|      - | 7628 | ` * Return` |
|      - | 7629 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7630 | ` */` |
|     18 | 7631 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7632 | `{` |
|      - | 7633 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7634 | `	int nLen;` |
|     19 | 7635 | `	if( nArg < 1 ){` |
|      - | 7636 | `		/* Missing arguments,return FALSE */` |
|      3 | 7637 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7638 | `		return PH7_OK;` |
|      - | 7639 | `	}` |
|      - | 7640 | `	/* Extract the target string */` |
|     17 | 7641 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7642 | `	zEnd = &zIn[nLen];` |
|     17 | 7643 | `	if( nLen < 1 ){` |
|      - | 7644 | `		/* Empty string,return FALSE */` |
|      3 | 7645 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7646 | `		return PH7_OK;` |
|      - | 7647 | `	}` |
|      - | 7648 | `	/* Perform the requested operation */` |
|     27 | 7649 | `	for(;;){` |
|     55 | 7650 | `		if( zIn >= zEnd ){` |
|      - | 7651 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7652 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7653 | `			return PH7_OK;` |
|      - | 7654 | `		}` |
|     51 | 7655 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7656 | `			break;` |
|      - | 7657 | `		}` |
|      - | 7658 | `		/* Point to the next character */` |
|     41 | 7659 | `		zIn++;` |
|      1 | 7660 | `	}` |
|      - | 7661 | `	/* The test failed,return FALSE */` |
|     11 | 7662 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7663 | `	return PH7_OK;` |
|     10 | 7664 | `}` |
|      - | 7665 | `/*` |
|      - | 7666 | ` * bool ctype_upper(string $text)` |
|      - | 7667 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7668 | ` * Parameters` |
|      - | 7669 | ` *  $text` |
|      - | 7670 | ` *   The tested string.` |
|      - | 7671 | ` * Return` |
|      - | 7672 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7673 | ` */` |
|     18 | 7674 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7675 | `{` |
|      - | 7676 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7677 | `	int nLen;` |
|     19 | 7678 | `	if( nArg < 1 ){` |
|      - | 7679 | `		/* Missing arguments,return FALSE */` |
|      3 | 7680 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7681 | `		return PH7_OK;` |
|      - | 7682 | `	}` |
|      - | 7683 | `	/* Extract the target string */` |
|     17 | 7684 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7685 | `	zEnd = &zIn[nLen];` |
|     17 | 7686 | `	if( nLen < 1 ){` |
|      - | 7687 | `		/* Empty string,return FALSE */` |
|      3 | 7688 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7689 | `		return PH7_OK;` |
|      - | 7690 | `	}` |
|      - | 7691 | `	/* Perform the requested operation */` |
|     28 | 7692 | `	for(;;){` |
|     57 | 7693 | `		if( zIn >= zEnd ){` |
|      - | 7694 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7695 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7696 | `			return PH7_OK;` |
|      - | 7697 | `		}` |
|     53 | 7698 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7699 | `			break;` |
|      - | 7700 | `		}` |
|      - | 7701 | `		/* Point to the next character */` |
|     43 | 7702 | `		zIn++;` |
|      1 | 7703 | `	}` |
|      - | 7704 | `	/* The test failed,return FALSE */` |
|     11 | 7705 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7706 | `	return PH7_OK;` |
|     10 | 7707 | `}` |
|      - | 7708 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 7709 | `/*` |
|      - | 7710 | ` * Section:` |
|      - | 7711 | ` *    URL handling Functions.` |
|      - | 7712 | ` * Status:` |
|      - | 7713 | ` *    Stable.` |
|      - | 7714 | ` */` |
|      - | 7715 | `/*` |
|      - | 7716 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 7717 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 7718 | ` */` |
|   1026 | 7719 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7720 | `{` |
|      - | 7721 | `	/* Store in the call context result buffer */` |
|   1028 | 7722 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 7723 | `	return SXRET_OK;` |
|      2 | 7724 | `}` |
|      - | 7725 | `/*` |
|      - | 7726 | ` * string base64_encode(string $data)` |
|      - | 7727 | ` * string convert_uuencode(string $data)` |
|      - | 7728 | ` *  Encodes data with MIME base64` |
|      - | 7729 | ` * Parameter` |
|      - | 7730 | ` *  $data` |
|      - | 7731 | ` *    Data to encode` |
|      - | 7732 | ` * Return` |
|      - | 7733 | ` *  Encoded data or FALSE on failure.` |
|      - | 7734 | ` */` |
|     10 | 7735 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7736 | `{` |
|      - | 7737 | `	const char *zIn;` |
|      - | 7738 | `	int nLen;` |
|     11 | 7739 | `	if( nArg < 1 ){` |
|      - | 7740 | `		/* Missing arguments,return FALSE */` |
|      5 | 7741 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7742 | `		return PH7_OK;` |
|      - | 7743 | `	}` |
|      - | 7744 | `	/* Extract the input string */` |
|      7 | 7745 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7746 | `	if( nLen < 1 ){` |
|      - | 7747 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7748 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7749 | `		return PH7_OK;` |
|      - | 7750 | `	}` |
|      - | 7751 | `	/* Perform the BASE64 encoding */` |
|      7 | 7752 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 7753 | `	return PH7_OK;` |
|      6 | 7754 | `}` |
|      - | 7755 | `/*` |
|      - | 7756 | ` * string base64_decode(string $data)` |
|      - | 7757 | ` * string convert_uudecode(string $data)` |
|      - | 7758 | ` *  Decodes data encoded with MIME base64` |
|      - | 7759 | ` * Parameter` |
|      - | 7760 | ` *  $data` |
|      - | 7761 | ` *    Encoded data.` |
|      - | 7762 | ` * Return` |
|      - | 7763 | ` *  Returns the original data or FALSE on failure.` |
|      - | 7764 | ` */` |
|     36 | 7765 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7766 | `{` |
|      - | 7767 | `	const char *zIn;` |
|      - | 7768 | `	int nLen;` |
|     38 | 7769 | `	if( nArg < 1 ){` |
|      - | 7770 | `		/* Missing arguments,return FALSE */` |
|      3 | 7771 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7772 | `		return PH7_OK;` |
|      - | 7773 | `	}` |
|      - | 7774 | `	/* Extract the input string */` |
|     36 | 7775 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 7776 | `	if( nLen < 1 ){` |
|      - | 7777 | `		/* Nothing to process,return FALSE */` |
|      3 | 7778 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7779 | `		return PH7_OK;` |
|      - | 7780 | `	}` |
|      - | 7781 | `	/* Perform the BASE64 decoding */` |
|     34 | 7782 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 7783 | `	return PH7_OK;` |
|     20 | 7784 | `}` |
|      - | 7785 | `/*` |
|      - | 7786 | ` * string urlencode(string $str)` |
|      - | 7787 | ` *  URL encoding` |
|      - | 7788 | ` * Parameter` |
|      - | 7789 | ` *  $data` |
|      - | 7790 | ` *   Input string.` |
|      - | 7791 | ` * Return` |
|      - | 7792 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 7793 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 7794 | ` *  encoded as plus (+) signs.` |
|      - | 7795 | ` */` |
|      6 | 7796 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7797 | `{` |
|      - | 7798 | `	const char *zIn;` |
|      - | 7799 | `	int nLen;` |
|      7 | 7800 | `	if( nArg < 1 ){` |
|      - | 7801 | `		/* Missing arguments,return FALSE */` |
|      3 | 7802 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7803 | `		return PH7_OK;` |
|      - | 7804 | `	}` |
|      - | 7805 | `	/* Extract the input string */` |
|      5 | 7806 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 7807 | `	if( nLen < 1 ){` |
|      - | 7808 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7809 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7810 | `		return PH7_OK;` |
|      - | 7811 | `	}` |
|      - | 7812 | `	/* Perform the URL encoding */` |
|      5 | 7813 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 7814 | `	return PH7_OK;` |
|      4 | 7815 | `}` |
|      - | 7816 | `/*` |
|      - | 7817 | ` * string urldecode(string $str)` |
|      - | 7818 | ` *  Decodes any %## encoding in the given string.` |
|      - | 7819 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 7820 | ` * Parameter` |
|      - | 7821 | ` *  $data` |
|      - | 7822 | ` *    Input string.` |
|      - | 7823 | ` * Return` |
|      - | 7824 | ` *  Decoded URL or FALSE on failure.` |
|      - | 7825 | ` */` |
|      8 | 7826 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7827 | `{` |
|      - | 7828 | `	const char *zIn;` |
|      - | 7829 | `	int nLen;` |
|      9 | 7830 | `	if( nArg < 1 ){` |
|      - | 7831 | `		/* Missing arguments,return FALSE */` |
|      3 | 7832 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7833 | `		return PH7_OK;` |
|      - | 7834 | `	}` |
|      - | 7835 | `	/* Extract the input string */` |
|      7 | 7836 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7837 | `	if( nLen < 1 ){` |
|      - | 7838 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7839 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7840 | `		return PH7_OK;` |
|      - | 7841 | `	}` |
|      - | 7842 | `	/* Perform the URL decoding */` |
|      7 | 7843 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 7844 | `	return PH7_OK;` |
|      5 | 7845 | `}` |
|      - | 7846 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7847 | `/* Table of the built-in functions */` |
|      - | 7848 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 7849 | `	   /* Variable handling functions */` |
|      - | 7850 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 7851 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 7852 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 7853 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 7854 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 7855 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 7856 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 7857 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 7858 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 7859 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 7860 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 7861 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 7862 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 7863 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 7864 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 7865 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 7866 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 7867 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 7868 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 7869 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 7870 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7871 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 7872 | `	   /* Math functions */` |
|      - | 7873 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 7874 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 7875 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 7876 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 7877 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 7878 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 7879 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 7880 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 7881 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 7882 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 7883 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 7884 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 7885 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 7886 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 7887 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 7888 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 7889 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 7890 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 7891 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 7892 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 7893 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 7894 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 7895 | `	{ "round",    PH7_builtin_round        },` |
|      - | 7896 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 7897 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 7898 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 7899 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 7900 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 7901 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 7902 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 7903 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 7904 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 7905 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7906 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7907 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 7908 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7909 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7910 | `	   /* String handling functions */` |
|      - | 7911 |  |
|      - | 7912 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 7913 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 7914 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 7915 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 7916 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 7917 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 7918 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 7919 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 7920 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 7921 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 7922 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 7923 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 7924 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 7925 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 7926 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 7927 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 7928 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 7929 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 7930 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 7931 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 7932 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 7933 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 7934 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 7935 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 7936 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 7937 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 7938 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 7939 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 7940 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 7941 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 7942 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 7943 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 7944 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 7945 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 7946 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 7947 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 7948 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 7949 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 7950 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 7951 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 7952 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 7953 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 7954 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 7955 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 7956 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 7957 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 7958 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 7959 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 7960 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 7961 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 7962 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 7963 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 7964 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7965 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7966 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 7967 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 7968 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 7969 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 7970 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7971 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7972 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 7973 |  |
|      - | 7974 |  |
|      - | 7975 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 7976 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 7977 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 7978 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 7979 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 7980 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 7981 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 7982 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 7983 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7984 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 7985 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 7986 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 7987 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 7988 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 7989 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 7990 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7991 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7992 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 7993 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 7994 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7995 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7996 |  |
|      - | 7997 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 7998 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 7999 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8000 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8001 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8002 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8003 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8004 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8005 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8006 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8007 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8008 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8009 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8010 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8011 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8012 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8013 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8014 |  |
|      - | 8015 | `	         /* Ctype functions */` |
|      - | 8016 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8017 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8018 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8019 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8020 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8021 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8022 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8023 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8024 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8025 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8026 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8027 | `	         /* Time functions */` |
|      - | 8028 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8029 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8030 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8031 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8032 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8033 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8034 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8035 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8036 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8037 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8038 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8039 | `	        /* URL functions */` |
|      - | 8040 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8041 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8042 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8043 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8044 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8045 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8046 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8047 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8048 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8049 | `};` |
|      - | 8050 | `/*` |
|      - | 8051 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8052 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8053 | ` */` |
|   3310 | 8054 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 8055 | `{` |
|      - | 8056 | `	sxu32 n;` |
| 556085 | 8057 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 552775 | 8058 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 276390 | 8059 | `	}` |
|      - | 8060 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3315 | 8061 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8062 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3315 | 8063 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3315 | 8064 | `}` |
|      - | 8065 |  |
