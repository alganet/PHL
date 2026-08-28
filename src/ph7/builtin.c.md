# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4350/5027 lines (86.53%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* filter_var(FILTER_VALIDATE_FLOAT) parses with libc strtod directly because it` |
|      - |    8 | ` * needs errno==ERANGE to reject out-of-range magnitudes; SyStrToReal (also` |
|      - |    9 | ` * strtod-backed nowadays) exposes no range-error signal. */` |
|      - |   10 | `#include <stdlib.h>  /* strtod */` |
|      - |   11 | `#include <math.h>    /* HUGE_VAL */` |
|      - |   12 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|      - |   13 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|      - |   14 | `                      * rounded digits like php's zend_dtoa; see PH7_InputFormat) */` |
|      - |   15 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |   16 | `/* Forward decl: null-to-string ZPP deprecation notice (defined near the ZPP` |
|      - |   17 | ` * helpers; both live inside the same DISABLE_BUILTIN_FUNC region as every` |
|      - |   18 | ` * caller — the tiny build compiles none of them). */` |
|      - |   19 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName);` |
|      - |   20 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - |   21 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |   22 | `/*` |
|      - |   23 | ` * Section:` |
|      - |   24 | ` *    Variable handling Functions.` |
|      - |   25 | ` * Status:` |
|      - |   26 | ` *    Stable.` |
|      - |   27 | ` */` |
|      - |   28 | `/*` |
|      - |   29 | ` * bool is_bool($var)` |
|      - |   30 | ` *  Finds out whether a variable is a boolean.` |
|      - |   31 | ` * Parameters` |
|      - |   32 | ` *   $var: The variable being evaluated.` |
|      - |   33 | ` * Return` |
|      - |   34 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |   35 | ` */` |
|     72 |   36 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   37 | `{` |
|     74 |   38 | `	int res = 0; /* Assume false by default */` |
|     74 |   39 | `	if( nArg > 0 ){` |
|     74 |   40 | `		res = ph7_value_is_bool(apArg[0]);` |
|     36 |   41 | `	}` |
|      - |   42 | `	/* Query result */` |
|     74 |   43 | `	ph7_result_bool(pCtx,res);` |
|     74 |   44 | `	return PH7_OK;` |
|      2 |   45 | `}` |
|      - |   46 | `/*` |
|      - |   47 | ` * bool is_float($var)` |
|      - |   48 | ` * bool is_real($var)` |
|      - |   49 | ` * bool is_double($var)` |
|      - |   50 | ` *  Finds out whether a variable is a float.` |
|      - |   51 | ` * Parameters` |
|      - |   52 | ` *   $var: The variable being evaluated.` |
|      - |   53 | ` * Return` |
|      - |   54 | ` *  TRUE if var is a float. False otherwise.` |
|      - |   55 | ` */` |
|    308 |   56 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   57 | `{` |
|    309 |   58 | `	int res = 0; /* Assume false by default */` |
|    309 |   59 | `	if( nArg > 0 ){` |
|    309 |   60 | `		res = ph7_value_is_float(apArg[0]);` |
|    154 |   61 | `	}` |
|      - |   62 | `	/* Query result */` |
|    309 |   63 | `	ph7_result_bool(pCtx,res);` |
|    309 |   64 | `	return PH7_OK;` |
|      1 |   65 | `}` |
|      - |   66 | `/*` |
|      - |   67 | ` * bool is_int($var)` |
|      - |   68 | ` * bool is_integer($var)` |
|      - |   69 | ` * bool is_long($var)` |
|      - |   70 | ` *  Finds out whether a variable is an integer.` |
|      - |   71 | ` * Parameters` |
|      - |   72 | ` *   $var: The variable being evaluated.` |
|      - |   73 | ` * Return` |
|      - |   74 | ` *  TRUE if var is an integer. False otherwise.` |
|      - |   75 | ` */` |
|    868 |   76 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |   77 | `{` |
|    872 |   78 | `	int res = 0; /* Assume false by default */` |
|    872 |   79 | `	if( nArg > 0 ){` |
|      - |   80 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   81 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   82 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    872 |   83 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    434 |   84 | `	}` |
|      - |   85 | `	/* Query result */` |
|    872 |   86 | `	ph7_result_bool(pCtx,res);` |
|    872 |   87 | `	return PH7_OK;` |
|      4 |   88 | `}` |
|      - |   89 | `/*` |
|      - |   90 | ` * bool is_string($var)` |
|      - |   91 | ` *  Finds out whether a variable is a string.` |
|      - |   92 | ` * Parameters` |
|      - |   93 | ` *   $var: The variable being evaluated.` |
|      - |   94 | ` * Return` |
|      - |   95 | ` *  TRUE if var is string. False otherwise.` |
|      - |   96 | ` */` |
|    756 |   97 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |   98 | `{` |
|    759 |   99 | `	int res = 0; /* Assume false by default */` |
|    759 |  100 | `	if( nArg > 0 ){` |
|    759 |  101 | `		res = ph7_value_is_string(apArg[0]);` |
|    378 |  102 | `	}` |
|      - |  103 | `	/* Query result */` |
|    759 |  104 | `	ph7_result_bool(pCtx,res);` |
|    759 |  105 | `	return PH7_OK;` |
|      3 |  106 | `}` |
|      - |  107 | `/*` |
|      - |  108 | ` * bool is_null($var)` |
|      - |  109 | ` *  Finds out whether a variable is NULL.` |
|      - |  110 | ` * Parameters` |
|      - |  111 | ` *   $var: The variable being evaluated.` |
|      - |  112 | ` * Return` |
|      - |  113 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  114 | ` */` |
|     86 |  115 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  116 | `{` |
|     89 |  117 | `	int res = 0; /* Assume false by default */` |
|     89 |  118 | `	if( nArg > 0 ){` |
|     89 |  119 | `		res = ph7_value_is_null(apArg[0]);` |
|     43 |  120 | `	}` |
|      - |  121 | `	/* Query result */` |
|     89 |  122 | `	ph7_result_bool(pCtx,res);` |
|     89 |  123 | `	return PH7_OK;` |
|      3 |  124 | `}` |
|      - |  125 | `/*` |
|      - |  126 | ` * bool is_numeric($var)` |
|      - |  127 | ` *  Find out whether a variable is NULL.` |
|      - |  128 | ` * Parameters` |
|      - |  129 | ` *  $var: The variable being evaluated.` |
|      - |  130 | ` * Return` |
|      - |  131 | ` *  True if var is numeric. False otherwise.` |
|      - |  132 | ` */` |
|     60 |  133 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  134 | `{` |
|     65 |  135 | `	int res = 0; /* Assume false by default */` |
|     65 |  136 | `	if( nArg > 0 ){` |
|     65 |  137 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     30 |  138 | `	}` |
|      - |  139 | `	/* Query result */` |
|     65 |  140 | `	ph7_result_bool(pCtx,res);` |
|     65 |  141 | `	return PH7_OK;` |
|      5 |  142 | `}` |
|      - |  143 | `/*` |
|      - |  144 | ` * bool is_scalar($var)` |
|      - |  145 | ` *  Find out whether a variable is a scalar.` |
|      - |  146 | ` * Parameters` |
|      - |  147 | ` *  $var: The variable being evaluated.` |
|      - |  148 | ` * Return` |
|      - |  149 | ` *  True if var is scalar. False otherwise.` |
|      - |  150 | ` */` |
|     12 |  151 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  152 | `{` |
|     13 |  153 | `	int res = 0; /* Assume false by default */` |
|     13 |  154 | `	if( nArg > 0 ){` |
|     13 |  155 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  156 | `	}` |
|      - |  157 | `	/* Query result */` |
|     13 |  158 | `	ph7_result_bool(pCtx,res);` |
|     13 |  159 | `	return PH7_OK;` |
|      1 |  160 | `}` |
|      - |  161 | `/*` |
|      - |  162 | ` * bool is_array($var)` |
|      - |  163 | ` *  Find out whether a variable is an array.` |
|      - |  164 | ` * Parameters` |
|      - |  165 | ` *  $var: The variable being evaluated.` |
|      - |  166 | ` * Return` |
|      - |  167 | ` *  True if var is an array. False otherwise.` |
|      - |  168 | ` */` |
|    650 |  169 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  170 | `{` |
|    654 |  171 | `	int res = 0; /* Assume false by default */` |
|    654 |  172 | `	if( nArg > 0 ){` |
|    654 |  173 | `		res = ph7_value_is_array(apArg[0]);` |
|    325 |  174 | `	}` |
|      - |  175 | `	/* Query result */` |
|    654 |  176 | `	ph7_result_bool(pCtx,res);` |
|    654 |  177 | `	return PH7_OK;` |
|      4 |  178 | `}` |
|      - |  179 | `/*` |
|      - |  180 | ` * bool is_object($var)` |
|      - |  181 | ` *  Find out whether a variable is an object.` |
|      - |  182 | ` * Parameters` |
|      - |  183 | ` *  $var: The variable being evaluated.` |
|      - |  184 | ` * Return` |
|      - |  185 | ` *  True if var is an object. False otherwise.` |
|      - |  186 | ` */` |
|    440 |  187 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  188 | `{` |
|    442 |  189 | `	int res = 0; /* Assume false by default */` |
|    442 |  190 | `	if( nArg > 0 ){` |
|    442 |  191 | `		res = ph7_value_is_object(apArg[0]);` |
|    220 |  192 | `	}` |
|      - |  193 | `	/* Query result */` |
|    442 |  194 | `	ph7_result_bool(pCtx,res);` |
|    442 |  195 | `	return PH7_OK;` |
|      2 |  196 | `}` |
|      - |  197 | `/*` |
|      - |  198 | ` * bool is_resource($var)` |
|      - |  199 | ` *  Find out whether a variable is a resource.` |
|      - |  200 | ` * Parameters` |
|      - |  201 | ` *  $var: The variable being evaluated.` |
|      - |  202 | ` * Return` |
|      - |  203 | ` *  True if a resource. False otherwise.` |
|      - |  204 | ` */` |
|     58 |  205 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  206 | `{` |
|     62 |  207 | `	int res = 0; /* Assume false by default */` |
|     62 |  208 | `	if( nArg > 0 ){` |
|     62 |  209 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  210 | `	}` |
|     62 |  211 | `	ph7_result_bool(pCtx,res);` |
|     62 |  212 | `	return PH7_OK;` |
|      4 |  213 | `}` |
|      - |  214 | `/*` |
|      - |  215 | ` * float floatval($var)` |
|      - |  216 | ` *  Get float value of a variable.` |
|      - |  217 | ` * Parameter` |
|      - |  218 | ` *  $var: The variable being processed.` |
|      - |  219 | ` * Return` |
|      - |  220 | ` *  the float value of a variable.` |
|      - |  221 | ` */` |
|      4 |  222 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  223 | `{` |
|      5 |  224 | `	if( nArg < 1 ){` |
|      - |  225 | `		/* return 0.0 */` |
|    ! 0 |  226 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  227 | `	}else{` |
|      - |  228 | `		double dval;` |
|      - |  229 | `		/* Perform the cast */` |
|      5 |  230 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  231 | `		ph7_result_double(pCtx,dval);` |
|      - |  232 | `	}` |
|      5 |  233 | `	return PH7_OK;` |
|      1 |  234 | `}` |
|      - |  235 | `/*` |
|      - |  236 | ` * int intval($var)` |
|      - |  237 | ` *  Get integer value of a variable.` |
|      - |  238 | ` * Parameter` |
|      - |  239 | ` *  $var: The variable being processed.` |
|      - |  240 | ` * Return` |
|      - |  241 | ` *  the int value of a variable.` |
|      - |  242 | ` */` |
|     46 |  243 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  244 | `{` |
|     47 |  245 | `	if( nArg < 1 ){` |
|      - |  246 | `		/* return 0 */` |
|    ! 0 |  247 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  248 | `	}else{` |
|      - |  249 | `		sxi64 iVal;` |
|      - |  250 | `		/* Perform the cast */` |
|     47 |  251 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     47 |  252 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  253 | `	}` |
|     47 |  254 | `	return PH7_OK;` |
|      1 |  255 | `}` |
|      - |  256 | `/*` |
|      - |  257 | ` * string strval($var)` |
|      - |  258 | ` *  Get the string representation of a variable.` |
|      - |  259 | ` * Parameter` |
|      - |  260 | ` *  $var: The variable being processed.` |
|      - |  261 | ` * Return` |
|      - |  262 | ` *  the string value of a variable.` |
|      - |  263 | ` */` |
|      2 |  264 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  265 | `{` |
|      3 |  266 | `	if( nArg < 1 ){` |
|      - |  267 | `		/* return NULL */` |
|    ! 0 |  268 | `		ph7_result_null(pCtx);` |
|    ! 0 |  269 | `	}else{` |
|      - |  270 | `		const char *zVal;` |
|      3 |  271 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  272 | `		/* Perform the cast */` |
|      3 |  273 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  274 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  275 | `	}` |
|      3 |  276 | `	return PH7_OK;` |
|      1 |  277 | `}` |
|      - |  278 | `/*` |
|      - |  279 | ` * bool boolval($var)` |
|      - |  280 | ` *  Get the boolean value of a variable.` |
|      - |  281 | ` * Parameter` |
|      - |  282 | ` *  $var: The variable being processed.` |
|      - |  283 | ` * Return` |
|      - |  284 | ` *  the bool value of a variable.` |
|      - |  285 | ` */` |
|     16 |  286 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  287 | `{` |
|      - |  288 | `	int bVal;` |
|     18 |  289 | `	if( nArg != 1 ){` |
|      4 |  290 | `		return PH7_VmThrowException(pCtx,` |
|      - |  291 | `			"ArgumentCountError",` |
|      - |  292 | `			"boolval() expects exactly 1 argument, %d given",` |
|      1 |  293 | `			nArg` |
|      - |  294 | `			);` |
|      - |  295 | `	}` |
|      - |  296 | `	/* Perform the cast */` |
|     15 |  297 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  298 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  299 | `	return PH7_OK;` |
|     10 |  300 | `}` |
|      - |  301 | `/*` |
|      - |  302 | ` * bool empty($var)` |
|      - |  303 | ` *  Determine whether a variable is empty.` |
|      - |  304 | ` * Parameters` |
|      - |  305 | ` *   $var: The variable being checked.` |
|      - |  306 | ` * Return` |
|      - |  307 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  308 | ` */` |
|  34402 |  309 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  310 | `{` |
|  34407 |  311 | `	int res = 1; /* Assume empty by default */` |
|  34407 |  312 | `	if( nArg > 0 ){` |
|  34405 |  313 | `		res = ph7_value_is_empty(apArg[0]);` |
|  17200 |  314 | `	}` |
|  34407 |  315 | `	ph7_result_bool(pCtx,res);` |
|  34407 |  316 | `	return PH7_OK;` |
|      - |  317 |  |
|      5 |  318 | `}` |
|      - |  319 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  320 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  321 | `#endif` |
|      - |  322 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  323 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  324 | `#endif` |
|      - |  325 |  |
|      - |  326 | `/* Math functions moved to builtin_math.c */` |
|      - |  327 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  328 | `/*` |
|      - |  329 | ` * Section:` |
|      - |  330 | ` *    String handling Functions.` |
|      - |  331 | ` * Status:` |
|      - |  332 | ` *    Stable.` |
|      - |  333 | ` */` |
|      - |  334 | `/*` |
|      - |  335 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  336 | ` *  Return part of a string.` |
|      - |  337 | ` * Parameters` |
|      - |  338 | ` *  $string` |
|      - |  339 | ` *   The input string. Must be one character or longer.` |
|      - |  340 | ` * $start` |
|      - |  341 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  342 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  343 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  344 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  345 | ` *   from the end of string.` |
|      - |  346 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  347 | ` * $length` |
|      - |  348 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  349 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  350 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  351 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  352 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  353 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  354 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  355 | ` *   will be returned.` |
|      - |  356 | ` * Return` |
|      - |  357 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  358 | ` */` |
| 233584 |  359 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  360 | `{` |
| 233589 |  361 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
|      - |  362 | `	const char *zSource,*zOfft;` |
|      - |  363 | `	int nOfft,nLen,nSrcLen;` |
| 233589 |  364 | `	if( nArg < 2 ){` |
|      - |  365 | `		/* return FALSE */` |
|    ! 0 |  366 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  367 | `		return PH7_OK;` |
|      - |  368 | `	}` |
|      - |  369 | `	/* Extract the target string */` |
| 233589 |  370 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 233589 |  371 | `	if( nSrcLen < 1 ){` |
|      - |  372 | `		/* Empty string,return FALSE */` |
|  12417 |  373 | `		ph7_result_bool(pCtx,0);` |
|  12417 |  374 | `		return PH7_OK;` |
|      - |  375 | `	}` |
| 221177 |  376 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  377 | `	/* Extract the offset */` |
| 221177 |  378 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 221177 |  379 | `	if( nOfft < 0 ){` |
|  33291 |  380 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  33291 |  381 | `		if( zOfft < zSource ){` |
|      - |  382 | `			/* Invalid offset */` |
|      5 |  383 | `			ph7_result_bool(pCtx,0);` |
|      5 |  384 | `			return PH7_OK;` |
|      - |  385 | `		}` |
|  33287 |  386 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  33287 |  387 | `		nOfft = (int)(zOfft-zSource);` |
| 204532 |  388 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  389 | `		/* Invalid offset */` |
|    241 |  390 | `		ph7_result_bool(pCtx,0);` |
|    241 |  391 | `		return PH7_OK;` |
|    ! 0 |  392 | `	}else{` |
| 187655 |  393 | `		zOfft = &zSource[nOfft];` |
| 187655 |  394 | `		nLen = nSrcLen - nOfft;` |
|      - |  395 | `	}` |
| 220937 |  396 | `	if( nArg > 2 ){` |
|      - |  397 | `		/* Extract the length */` |
| 182005 |  398 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 182005 |  399 | `		if( nLen == 0 ){` |
|      - |  400 | `			/* Invalid length,return an empty string */` |
|      5 |  401 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  402 | `			return PH7_OK;` |
| 182001 |  403 | `		}else if( nLen < 0 ){` |
|  33221 |  404 | `			nLen = nSrcLen + nLen - nOfft;` |
|  33221 |  405 | `			if( nLen < 1 ){` |
|      - |  406 | `				/* Invalid  length */` |
|      3 |  407 | `				nLen = nSrcLen - nOfft;` |
|      1 |  408 | `			}` |
|  16608 |  409 | `		}` |
| 182001 |  410 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  411 | `			/* Invalid length */` |
|   6109 |  412 | `			nLen = nSrcLen - nOfft;` |
|   3052 |  413 | `		}` |
|  90998 |  414 | `	}` |
|      - |  415 | `	/* Return the substring */` |
| 220933 |  416 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 220933 |  417 | `	return PH7_OK;` |
| 116797 |  418 | `}` |
|      - |  419 | `/*` |
|      - |  420 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  421 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  422 | ` * Parameters` |
|      - |  423 | ` *  $main_str` |
|      - |  424 | ` *  The main string being compared.` |
|      - |  425 | ` *  $str` |
|      - |  426 | ` *   The secondary string being compared.` |
|      - |  427 | ` * $offset` |
|      - |  428 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  429 | ` *  the end of the string.` |
|      - |  430 | ` * $length` |
|      - |  431 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  432 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  433 | ` * $case_insensitivity` |
|      - |  434 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  435 | ` * Return` |
|      - |  436 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  437 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  438 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  439 | ` */` |
|     22 |  440 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  441 | `{` |
|      - |  442 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  443 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     23 |  444 | `	int iCase = 0;` |
|      - |  445 | `	int rc;` |
|     23 |  446 | `	if( nArg < 3 ){` |
|      - |  447 | `		/* Missing arguments,return FALSE */` |
|    ! 0 |  448 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  449 | `		return PH7_OK;` |
|      - |  450 | `	}` |
|      - |  451 | `	/* Extract the target string */` |
|     23 |  452 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  453 | `	if( nSrcLen < 1 ){` |
|      - |  454 | `		/* Empty string,return FALSE */` |
|      3 |  455 | `		ph7_result_bool(pCtx,0);` |
|      3 |  456 | `		return PH7_OK;` |
|      - |  457 | `	}` |
|     21 |  458 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  459 | `	/* Extract the substring */` |
|     21 |  460 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  461 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  462 | `		/* Empty string,return FALSE */` |
|      3 |  463 | `		ph7_result_bool(pCtx,0);` |
|      3 |  464 | `		return PH7_OK;` |
|      - |  465 | `	}` |
|      - |  466 | `	/* Extract the offset */` |
|     19 |  467 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  468 | `	if( nOfft < 0 ){` |
|      5 |  469 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  470 | `		if( zOfft < zSource ){` |
|      - |  471 | `			/* Invalid offset */` |
|      3 |  472 | `			ph7_result_bool(pCtx,0);` |
|      3 |  473 | `			return PH7_OK;` |
|      - |  474 | `		}` |
|      3 |  475 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  476 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  477 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  478 | `		/* Invalid offset */` |
|      3 |  479 | `		ph7_result_bool(pCtx,0);` |
|      3 |  480 | `		return PH7_OK;` |
|    ! 0 |  481 | `	}else{` |
|     13 |  482 | `		zOfft = &zSource[nOfft];` |
|     13 |  483 | `		nLen = nSrcLen - nOfft;` |
|      - |  484 | `	}` |
|     15 |  485 | `	if( nArg > 3 ){` |
|      - |  486 | `		/* Extract the length */` |
|     13 |  487 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  488 | `		if( nLen < 1 ){` |
|      - |  489 | `			/* Invalid  length */` |
|      5 |  490 | `			ph7_result_int(pCtx,1);` |
|      5 |  491 | `			return PH7_OK;` |
|      9 |  492 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  493 | `			/* Invalid length */` |
|      3 |  494 | `			nLen = nSrcLen - nOfft;` |
|      1 |  495 | `		}` |
|      9 |  496 | `		if( nArg > 4 ){` |
|      - |  497 | `			/* Case-sensitive or not */` |
|      5 |  498 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  499 | `		}` |
|      4 |  500 | `	}` |
|      - |  501 | `	/* Perform the comparison */` |
|     11 |  502 | `	if( iCase ){` |
|      3 |  503 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  504 | `	}else{` |
|      9 |  505 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  506 | `	}` |
|      - |  507 | `	/* Comparison result */` |
|     11 |  508 | `	ph7_result_int(pCtx,rc);` |
|     11 |  509 | `	return PH7_OK;` |
|     12 |  510 | `}` |
|      - |  511 | `/*` |
|      - |  512 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  513 | ` *  Count the number of substring occurrences.` |
|      - |  514 | ` * Parameters` |
|      - |  515 | ` * $haystack` |
|      - |  516 | ` *   The string to search in` |
|      - |  517 | ` * $needle` |
|      - |  518 | ` *   The substring to search for` |
|      - |  519 | ` * $offset` |
|      - |  520 | ` *  The offset where to start counting` |
|      - |  521 | ` * $length (NOT USED)` |
|      - |  522 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  523 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  524 | ` * Return` |
|      - |  525 | ` *  Toral number of substring occurrences.` |
|      - |  526 | ` */` |
|     26 |  527 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  528 | `{` |
|      - |  529 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  530 | `	int nTextlen,nPatlen;` |
|     27 |  531 | `	int iCount = 0;` |
|      - |  532 | `	sxu32 nOfft;` |
|      - |  533 | `	sxi32 rc;` |
|     27 |  534 | `	if( nArg < 2 ){` |
|      - |  535 | `		/* Missing arguments */` |
|    ! 0 |  536 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  537 | `		return PH7_OK;` |
|      - |  538 | `	}` |
|      - |  539 | `	/* Point to the haystack */` |
|     27 |  540 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  541 | `	/* Point to the neddle */` |
|     27 |  542 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     27 |  543 | `	if( nPatlen < 1 ){` |
|      - |  544 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  545 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  546 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  547 | `	}` |
|      - |  548 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  549 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  550 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  551 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     25 |  552 | `	if( nArg > 2 ){` |
|     19 |  553 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|     19 |  554 | `		if( iOfft < 0 ){` |
|      5 |  555 | `			iOfft += nTextlen;` |
|      2 |  556 | `		}` |
|     19 |  557 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      3 |  558 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  559 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  560 | `		}` |
|      - |  561 | `		/* Point to the desired offset and shrink the remaining region */` |
|     17 |  562 | `		zText = &zText[iOfft];` |
|     17 |  563 | `		nTextlen -= (int)iOfft;` |
|      8 |  564 | `	}` |
|     23 |  565 | `	if( nArg > 3 ){` |
|     15 |  566 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|     15 |  567 | `		if( nLen < 0 ){` |
|      - |  568 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|      5 |  569 | `			nLen += nTextlen;` |
|      2 |  570 | `		}` |
|     15 |  571 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      5 |  572 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  573 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|      - |  574 | `		}` |
|     11 |  575 | `		nTextlen = (int)nLen;` |
|      5 |  576 | `	}` |
|     19 |  577 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  578 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  579 | `		ph7_result_int(pCtx,0);` |
|      3 |  580 | `		return PH7_OK;` |
|      - |  581 | `	}` |
|      - |  582 | `	/* Point to the end of the windowed haystack */` |
|     17 |  583 | `	zEnd = &zText[nTextlen];` |
|      - |  584 | `	/* Perform the search */` |
|     17 |  585 | `	for(;;){` |
|     35 |  586 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     35 |  587 | `		if( rc != SXRET_OK ){` |
|      - |  588 | `			/* Pattern not found,break immediately */` |
|     13 |  589 | `			break;` |
|      - |  590 | `		}` |
|      - |  591 | `		/* Increment counter and update the offset */` |
|     23 |  592 | `		iCount++;` |
|     23 |  593 | `		zText += nOfft + nPatlen;` |
|     23 |  594 | `		if( zText >= zEnd ){` |
|      5 |  595 | `			break;` |
|      - |  596 | `		}` |
|      1 |  597 | `	}` |
|      - |  598 | `	/* Pattern count */` |
|     17 |  599 | `	ph7_result_int(pCtx,iCount);` |
|     17 |  600 | `	return PH7_OK;` |
|     14 |  601 | `}` |
|      - |  602 | `/* Forward declarations: defined with the trim/addcslashes and str_contains` |
|      - |  603 | ` * families below. */` |
|      - |  604 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);` |
|      - |  605 | `/*` |
|      - |  606 | ` * php 8.1 null-to-non-nullable ZPP deprecation, notice-only form for the` |
|      - |  607 | ` * legacy string builtins that still coerce null to "" themselves: emit` |
|      - |  608 | ``  * `f(): Passing null to parameter #N ($name) of type string is deprecated` `` |
|      - |  609 | ` * when the arg is an actual null, leaving the resolution unchanged.` |
|      - |  610 | ` */` |
| 299354 |  611 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  612 | `{` |
| 299359 |  613 | `	if( ph7_value_is_null(pArg) ){` |
|     25 |  614 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  615 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      8 |  616 | `			zFunc,iArgNum,zParamName);` |
|      8 |  617 | `	}` |
| 299359 |  618 | `}` |
|      - |  619 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|      - |  620 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|      - |  621 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|      - |  622 | `/*` |
|      - |  623 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  624 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  625 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  626 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  627 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  628 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  629 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  630 | ` */` |
|    150 |  631 | `static sxi32 IntArgResolve(` |
|      - |  632 | `	ph7_context *pCtx,` |
|      - |  633 | `	ph7_value *pArg,` |
|      - |  634 | `	const char *zFunc,` |
|      - |  635 | `	int iArgNum,` |
|      - |  636 | `	const char *zParamName,` |
|      - |  637 | `	const char *zTypeStr,` |
|      - |  638 | `	sxi64 *pOut` |
|      1 |  639 | `){` |
|    151 |  640 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |  641 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  642 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |  643 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  644 | `			);` |
|    ! 0 |  645 | `		*pOut = 0;` |
|    ! 0 |  646 | `		return PH7_OK;` |
|      - |  647 | `	}` |
|    151 |  648 | `	if( ph7_value_is_float(pArg) ){` |
|      5 |  649 | `		double dVal = ph7_value_to_double(pArg);` |
|      - |  650 | `		sxi64 iVal;` |
|      - |  651 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|      5 |  652 | `		if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|      7 |  653 | `			return PH7_VmThrowException(pCtx,` |
|      - |  654 | `				"TypeError",` |
|      - |  655 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|      2 |  656 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  657 | `				);` |
|      - |  658 | `		}` |
|    ! 0 |  659 | `		iVal = (sxi64)dVal;` |
|    ! 0 |  660 | `		if( (double)iVal != dVal ){` |
|    ! 0 |  661 | `			PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  662 | `				"Implicit conversion from float %s to int loses precision",` |
|    ! 0 |  663 | `				ph7_value_to_string(pArg,0)` |
|      - |  664 | `				);` |
|    ! 0 |  665 | `		}` |
|    ! 0 |  666 | `		*pOut = iVal;` |
|    ! 0 |  667 | `		return PH7_OK;` |
|      - |  668 | `	}` |
|    147 |  669 | `	if( ph7_value_is_string(pArg) ){` |
|      - |  670 | `		const char *zNum;` |
|      - |  671 | `		int nSlen;` |
|     15 |  672 | `		int i,bFloat = 0;` |
|     15 |  673 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|     16 |  674 | `			return PH7_VmThrowException(pCtx,` |
|      - |  675 | `				"TypeError",` |
|      - |  676 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      5 |  677 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  678 | `				);` |
|      - |  679 | `		}` |
|      5 |  680 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|      9 |  681 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|      5 |  682 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|    ! 0 |  683 | `				bFloat = 1;` |
|    ! 0 |  684 | `				break;` |
|      - |  685 | `			}` |
|      3 |  686 | `		}` |
|      5 |  687 | `		if( bFloat ){` |
|    ! 0 |  688 | `			double dVal = 0;` |
|      - |  689 | `			sxi64 iVal;` |
|    ! 0 |  690 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|    ! 0 |  691 | `			if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|    ! 0 |  692 | `				return PH7_VmThrowException(pCtx,` |
|      - |  693 | `					"TypeError",` |
|      - |  694 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|    ! 0 |  695 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  696 | `					);` |
|      - |  697 | `			}` |
|    ! 0 |  698 | `			iVal = (sxi64)dVal;` |
|    ! 0 |  699 | `			if( (double)iVal != dVal ){` |
|    ! 0 |  700 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  701 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|    ! 0 |  702 | `					zNum` |
|      - |  703 | `					);` |
|    ! 0 |  704 | `			}` |
|    ! 0 |  705 | `			*pOut = iVal;` |
|    ! 0 |  706 | `			return PH7_OK;` |
|      - |  707 | `		}` |
|      5 |  708 | `		*pOut = ph7_value_to_int64(pArg);` |
|      5 |  709 | `		return PH7_OK;` |
|      - |  710 | `	}` |
|    133 |  711 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|      - |  712 | `		/* Arrays, resources and objects: php names the class for objects */` |
|      5 |  713 | `		const char *zType = ph7_type_name(pArg);` |
|      5 |  714 | `		if( ph7_value_is_object(pArg) ){` |
|      3 |  715 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|      3 |  716 | `			if( pInst && pInst->pClass ){` |
|      3 |  717 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 |  718 | `			}` |
|      1 |  719 | `		}` |
|      7 |  720 | `		return PH7_VmThrowException(pCtx,` |
|      - |  721 | `			"TypeError",` |
|      - |  722 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|      2 |  723 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|      - |  724 | `			);` |
|      - |  725 | `	}` |
|    129 |  726 | `	*pOut = ph7_value_to_int64(pArg);` |
|    129 |  727 | `	return PH7_OK;` |
|     76 |  728 | `}` |
|      - |  729 | `/*` |
|      - |  730 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  731 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  732 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  733 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  734 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  735 | ` * INT64_MAX length cannot overflow.` |
|      - |  736 | ` */` |
|     60 |  737 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  738 | `{` |
|     61 |  739 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  740 | `	if( f < 0 ){` |
|      9 |  741 | `		f += nStrLen;` |
|      9 |  742 | `		if( f < 0 ){` |
|      5 |  743 | `			f = 0;` |
|      3 |  744 | `		}` |
|     57 |  745 | `	}else if( f > nStrLen ){` |
|      5 |  746 | `		f = nStrLen;` |
|      2 |  747 | `	}` |
|     61 |  748 | `	if( l < 0 ){` |
|      7 |  749 | `		l += nStrLen - f;` |
|      7 |  750 | `		if( l < 0 ){` |
|      5 |  751 | `			l = 0;` |
|      2 |  752 | `		}` |
|      3 |  753 | `	}` |
|     61 |  754 | `	if( l > nStrLen - f ){` |
|     25 |  755 | `		l = nStrLen - f;` |
|     12 |  756 | `	}` |
|     61 |  757 | `	*pF = f;` |
|     61 |  758 | `	*pL = l;` |
|     61 |  759 | `}` |
|      - |  760 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  761 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  762 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  763 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  764 | `struct substr_repl_item` |
|      - |  765 | `{` |
|      - |  766 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  767 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  768 | `};` |
|      - |  769 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  770 | `struct substr_replace_collect` |
|      - |  771 | `{` |
|      - |  772 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  773 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  774 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  775 | `};` |
|      - |  776 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  777 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  778 | `{` |
|      7 |  779 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  780 | `	substr_repl_item sItem;` |
|      - |  781 | `	const char *zStr;` |
|      - |  782 | `	int nLen;` |
|      3 |  783 | `	SXUNUSED(pKey);` |
|      7 |  784 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  785 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  786 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  787 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  788 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  789 | `		return SXERR_ABORT;` |
|      - |  790 | `	}` |
|      7 |  791 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  792 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  793 | `		return SXERR_ABORT;` |
|      - |  794 | `	}` |
|      7 |  795 | `	return PH7_OK;` |
|      4 |  796 | `}` |
|      - |  797 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  798 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  799 | `{` |
|     13 |  800 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  801 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  802 | `	SXUNUSED(pKey);` |
|     13 |  803 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  804 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  805 | `		return SXERR_ABORT;` |
|      - |  806 | `	}` |
|     13 |  807 | `	return PH7_OK;` |
|      7 |  808 | `}` |
|      - |  809 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  810 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  811 | `struct substr_replace_ctx` |
|      - |  812 | `{` |
|      - |  813 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  814 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  815 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  816 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  817 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  818 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  819 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  820 | `	sxu32 iFromCur;` |
|      - |  821 | `	sxu32 iLenCur;` |
|      - |  822 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  823 | `	int nRepl;` |
|      - |  824 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  825 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  826 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  827 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  828 | `};` |
|      - |  829 | `/*` |
|      - |  830 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  831 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  832 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  833 | ` * falls back to ""/0/element-length respectively.` |
|      - |  834 | ` */` |
|     24 |  835 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  836 | `{` |
|     25 |  837 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  838 | `	const char *zStr,*zRepl;` |
|      - |  839 | `	sxi64 f,l;` |
|      - |  840 | `	int nLen,nRepl;` |
|     25 |  841 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  842 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  843 | `	if( pRep->pRepl ){` |
|     11 |  844 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  845 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  846 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  847 | `			nRepl = (int)pItem->nLen;` |
|      4 |  848 | `		}else{` |
|      5 |  849 | `			zRepl = "";` |
|      5 |  850 | `			nRepl = 0;` |
|      - |  851 | `		}` |
|      6 |  852 | `	}else{` |
|     15 |  853 | `		zRepl = pRep->zRepl;` |
|     15 |  854 | `		nRepl = pRep->nRepl;` |
|      - |  855 | `	}` |
|      - |  856 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  857 | `	if( pRep->pFrom ){` |
|     13 |  858 | `		sxi64 *pVal = 0;` |
|     13 |  859 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  860 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  861 | `		}` |
|     13 |  862 | `		f = pVal ? *pVal : 0;` |
|      7 |  863 | `	}else{` |
|     13 |  864 | `		f = pRep->iFrom;` |
|      - |  865 | `	}` |
|      - |  866 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  867 | `	if( pRep->pLen ){` |
|      7 |  868 | `		sxi64 *pVal = 0;` |
|      7 |  869 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  870 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  871 | `		}` |
|      7 |  872 | `		l = pVal ? *pVal : nLen;` |
|      4 |  873 | `	}else{` |
|     19 |  874 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  875 | `	}` |
|     25 |  876 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  877 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  878 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  879 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  880 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  881 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  882 | `		pRep->rc = SXERR_MEM;` |
|     30 |  883 | `		return SXERR_ABORT;` |
|      - |  884 | `	}` |
|     25 |  885 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  886 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  887 | `		return SXERR_ABORT;` |
|      - |  888 | `	}` |
|     25 |  889 | `	return PH7_OK;` |
|     43 |  890 | `}` |
|      - |  891 | `/*` |
|      - |  892 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  893 | ` *  Replace text within a portion of a string.` |
|      - |  894 | ` * Parameters` |
|      - |  895 | ` *  $string` |
|      - |  896 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  897 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  898 | ` *  $replace` |
|      - |  899 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  900 | ` *   only its first element is used (PHP quirk).` |
|      - |  901 | ` *  $offset` |
|      - |  902 | ` *   Window start; negative counts from the end of the string.` |
|      - |  903 | ` *  $length` |
|      - |  904 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  905 | ` *   means "to the end of the string".` |
|      - |  906 | ` * Return` |
|      - |  907 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  908 | ` * Errors` |
|      - |  909 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  910 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  911 | ` */` |
|     68 |  912 | `static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  913 | `{` |
|      - |  914 | `	ph7_value sStrTmp,sReplTmp;` |
|     69 |  915 | `	const char *zStr = 0,*zRepl = 0;` |
|     69 |  916 | `	int nLen = 0,nRepl = 0;` |
|      - |  917 | `	int bLenGiven;` |
|     69 |  918 | `	sxi64 f = 0,l = 0;` |
|      - |  919 | `	sxi32 rc;` |
|     69 |  920 | `	if( nArg < 3 ){` |
|      7 |  921 | `		return PH7_VmThrowException(pCtx,` |
|      - |  922 | `			"ArgumentCountError",` |
|      - |  923 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|      2 |  924 | `			nArg` |
|      - |  925 | `			);` |
|      - |  926 | `	}` |
|      - |  927 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     65 |  928 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  929 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  930 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  931 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     65 |  932 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     65 |  933 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     65 |  934 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     49 |  935 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  936 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  937 | `			"of type array\|string is deprecated",` |
|      - |  938 | `			&sStrTmp,&zStr,&nLen);` |
|     49 |  939 | `		if( rc != PH7_OK ) goto out;` |
|     23 |  940 | `	}` |
|     63 |  941 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     55 |  942 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  943 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  944 | `			"of type array\|string is deprecated",` |
|      - |  945 | `			&sReplTmp,&zRepl,&nRepl);` |
|     55 |  946 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  947 | `	}` |
|     59 |  948 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  949 | `		rc = IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  950 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  951 | `	}` |
|     57 |  952 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  953 | `		rc = IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  954 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  955 | `	}` |
|     55 |  956 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  957 | `		/* Array form: process each element, preserving keys */` |
|      - |  958 | `		substr_replace_ctx sRep;` |
|      - |  959 | `		substr_replace_collect sCol;` |
|      - |  960 | `		SyBlob sReplPool;` |
|      - |  961 | `		SySet sRepl,sFrom,sLen;` |
|      - |  962 | `		ph7_value *pResult,*pScratch;` |
|     15 |  963 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  964 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  965 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  966 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  967 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  968 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  969 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  970 | `		sCol.rc = SXRET_OK;` |
|      - |  971 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  972 | `		 * scalar forms were already resolved above. */` |
|     15 |  973 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  974 | `			sCol.pPool = &sReplPool;` |
|      5 |  975 | `			sCol.pSet = &sRepl;` |
|      5 |  976 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  977 | `			sRep.pRepl = &sRepl;` |
|      5 |  978 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  979 | `		}else{` |
|     11 |  980 | `			sRep.zRepl = zRepl;` |
|     11 |  981 | `			sRep.nRepl = nRepl;` |
|      - |  982 | `		}` |
|     15 |  983 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 |  984 | `			sCol.pSet = &sFrom;` |
|      7 |  985 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 |  986 | `			sRep.pFrom = &sFrom;` |
|      4 |  987 | `		}else{` |
|      9 |  988 | `			sRep.iFrom = f;` |
|      - |  989 | `		}` |
|     15 |  990 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 |  991 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 |  992 | `				sCol.pSet = &sLen;` |
|      5 |  993 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 |  994 | `				sRep.pLen = &sLen;` |
|      3 |  995 | `			}else{` |
|      5 |  996 | `				sRep.iLen = l;` |
|      - |  997 | `			}` |
|      4 |  998 | `		}` |
|     15 |  999 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 | 1000 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 | 1001 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 | 1002 | `			rcWalk = SXERR_MEM;` |
|    ! 0 | 1003 | `		}else{` |
|     15 | 1004 | `			sRep.pResult = pResult;` |
|     15 | 1005 | `			sRep.pScratch = pScratch;` |
|     15 | 1006 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 | 1007 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 | 1008 | `			rcWalk = sRep.rc;` |
|      - | 1009 | `		}` |
|     15 | 1010 | `		SyBlobRelease(&sReplPool);` |
|     15 | 1011 | `		SySetRelease(&sRepl);` |
|     15 | 1012 | `		SySetRelease(&sFrom);` |
|     15 | 1013 | `		SySetRelease(&sLen);` |
|     15 | 1014 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 | 1015 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1016 | `			goto out;` |
|      - | 1017 | `		}` |
|     15 | 1018 | `		ph7_result_value(pCtx,pResult);` |
|     15 | 1019 | `		rc = PH7_OK;` |
|     15 | 1020 | `		goto out;` |
|      - | 1021 | `	}` |
|      - | 1022 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - | 1023 | `	 * degrades to its first element (php quirk). */` |
|     41 | 1024 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 | 1025 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1026 | `			"TypeError",` |
|      - | 1027 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - | 1028 | `			);` |
|      3 | 1029 | `		goto out;` |
|      - | 1030 | `	}` |
|     39 | 1031 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 | 1032 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1033 | `			"TypeError",` |
|      - | 1034 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - | 1035 | `			);` |
|      3 | 1036 | `		goto out;` |
|      - | 1037 | `	}` |
|     37 | 1038 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 1039 | `		/* First element of the replace array, or "" when empty */` |
|      5 | 1040 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 1041 | `		zRepl = "";` |
|      5 | 1042 | `		nRepl = 0;` |
|      5 | 1043 | `		if( pMap->pFirst ){` |
|      3 | 1044 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 | 1045 | `			if( pVal ){` |
|      3 | 1046 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 | 1047 | `			}` |
|      1 | 1048 | `		}` |
|      2 | 1049 | `	}` |
|     37 | 1050 | `	if( !bLenGiven ){` |
|     15 | 1051 | `		l = nLen;` |
|      7 | 1052 | `	}` |
|     37 | 1053 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - | 1054 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - | 1055 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 | 1056 | `	rc = SXRET_OK;` |
|     37 | 1057 | `	if( f > 0 ){` |
|     29 | 1058 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 | 1059 | `	}` |
|     37 | 1060 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 | 1061 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 | 1062 | `	}` |
|     37 | 1063 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 | 1064 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 | 1065 | `	}` |
|     37 | 1066 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1067 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1068 | `		goto out;` |
|      - | 1069 | `	}` |
|      - | 1070 | `	/* Force a string result even when all three segments are empty */` |
|     37 | 1071 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 | 1072 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1073 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1074 | `		goto out;` |
|      - | 1075 | `	}` |
|     37 | 1076 | `	rc = PH7_OK;` |
|     32 | 1077 | `out:` |
|     65 | 1078 | `	PH7_MemObjRelease(&sStrTmp);` |
|     65 | 1079 | `	PH7_MemObjRelease(&sReplTmp);` |
|     65 | 1080 | `	return rc;` |
|     35 | 1081 | `}` |
|      - | 1082 | `/*` |
|      - | 1083 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - | 1084 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - | 1085 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - | 1086 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - | 1087 | ` * Return` |
|      - | 1088 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - | 1089 | ` *  $string2.` |
|      - | 1090 | ` */` |
|     42 | 1091 | `static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1092 | `{` |
|      - | 1093 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - | 1094 | `	const char *zStr1,*zStr2;` |
|     43 | 1095 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - | 1096 | `	sxi64 *p1,*p2,*pTmp;` |
|      - | 1097 | `	sxi64 c0,c1,c2;` |
|      - | 1098 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1099 | `	int nLen1,nLen2;` |
|      - | 1100 | `	int i1,i2;` |
|      - | 1101 | `	sxi32 rc;` |
|      - | 1102 | `	int i;` |
|     43 | 1103 | `	if( nArg < 2 ){` |
|      4 | 1104 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1105 | `			"ArgumentCountError",` |
|      - | 1106 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|      1 | 1107 | `			nArg` |
|      - | 1108 | `			);` |
|      - | 1109 | `	}` |
|      - | 1110 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - | 1111 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     41 | 1112 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     41 | 1113 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     41 | 1114 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - | 1115 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - | 1116 | `		"of type string is deprecated",` |
|      - | 1117 | `		&sTmp1,&zStr1,&nLen1);` |
|     41 | 1118 | `	if( rc != PH7_OK ) goto out;` |
|     39 | 1119 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - | 1120 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - | 1121 | `		"of type string is deprecated",` |
|      - | 1122 | `		&sTmp2,&zStr2,&nLen2);` |
|     39 | 1123 | `	if( rc != PH7_OK ) goto out;` |
|      - | 1124 | `	/* Optional integer costs */` |
|     63 | 1125 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - | 1126 | `		sxi64 iVal;` |
|     37 | 1127 | `		rc = IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     37 | 1128 | `		if( rc != PH7_OK ) goto out;` |
|     25 | 1129 | `		if( i == 2 ){` |
|     13 | 1130 | `			iCostIns = iVal;` |
|     19 | 1131 | `		}else if( i == 3 ){` |
|      7 | 1132 | `			iCostRep = iVal;` |
|      4 | 1133 | `		}else{` |
|      7 | 1134 | `			iCostDel = iVal;` |
|      - | 1135 | `		}` |
|     13 | 1136 | `	}` |
|     27 | 1137 | `	if( nLen1 == 0 ){` |
|      3 | 1138 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 | 1139 | `		rc = PH7_OK;` |
|      3 | 1140 | `		goto out;` |
|      - | 1141 | `	}` |
|     25 | 1142 | `	if( nLen2 == 0 ){` |
|      3 | 1143 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 | 1144 | `		rc = PH7_OK;` |
|      3 | 1145 | `		goto out;` |
|      - | 1146 | `	}` |
|      - | 1147 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - | 1148 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 | 1149 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 | 1150 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1151 | `		goto out;` |
|      - | 1152 | `	}` |
|     23 | 1153 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1154 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1155 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 | 1156 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1157 | `		goto out;` |
|      - | 1158 | `	}` |
|    733 | 1159 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 | 1160 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 | 1161 | `	}` |
|    707 | 1162 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 | 1163 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 | 1164 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 | 1165 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 | 1166 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 | 1167 | `			if( c1 < c0 ){` |
|  45393 | 1168 | `				c0 = c1;` |
|  22696 | 1169 | `			}` |
| 180427 | 1170 | `			c2 = p2[i2] + iCostIns;` |
| 180427 | 1171 | `			if( c2 < c0 ){` |
|  44809 | 1172 | `				c0 = c2;` |
|  22404 | 1173 | `			}` |
| 180427 | 1174 | `			p2[i2 + 1] = c0;` |
|  90214 | 1175 | `		}` |
|    685 | 1176 | `		pTmp = p1;` |
|    685 | 1177 | `		p1 = p2;` |
|    685 | 1178 | `		p2 = pTmp;` |
|    343 | 1179 | `	}` |
|     23 | 1180 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 | 1181 | `	rc = PH7_OK;` |
|     20 | 1182 | `out:` |
|     41 | 1183 | `	PH7_MemObjRelease(&sTmp1);` |
|     41 | 1184 | `	PH7_MemObjRelease(&sTmp2);` |
|     41 | 1185 | `	return rc;` |
|     22 | 1186 | `}` |
|      - | 1187 | `/*` |
|      - | 1188 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - | 1189 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - | 1190 | ` */` |
|     26 | 1191 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - | 1192 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 | 1193 | `{` |
|      - | 1194 | `	const char *p,*q;` |
|     27 | 1195 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 | 1196 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - | 1197 | `	int l;` |
|     27 | 1198 | `	*pMax = 0;` |
|     27 | 1199 | `	*pCount = 0;` |
|    143 | 1200 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 | 1201 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 | 1202 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 | 1203 | `			if( l > *pMax ){` |
|     25 | 1204 | `				*pMax = l;` |
|     25 | 1205 | `				*pCount += 1;` |
|     25 | 1206 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 | 1207 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 | 1208 | `			}` |
|    364 | 1209 | `		}` |
|     59 | 1210 | `	}` |
|     27 | 1211 | `}` |
|      - | 1212 | `/*` |
|      - | 1213 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - | 1214 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - | 1215 | ` * left-side recursion.` |
|      - | 1216 | ` */` |
|     26 | 1217 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 | 1218 | `{` |
|      - | 1219 | `	int nSum;` |
|     27 | 1220 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 | 1221 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 | 1222 | `	if( (nSum = nMax) != 0 ){` |
|     25 | 1223 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 | 1224 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 | 1225 | `		}` |
|     25 | 1226 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 | 1227 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 | 1228 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 | 1229 | `		}` |
|     12 | 1230 | `	}` |
|     27 | 1231 | `	return nSum;` |
|      1 | 1232 | `}` |
|      - | 1233 | `/*` |
|      - | 1234 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - | 1235 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - | 1236 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - | 1237 | ` *  When $percent is given it receives the similarity in percent:` |
|      - | 1238 | ` *  matching * 200 / (len1 + len2).` |
|      - | 1239 | ` * Return` |
|      - | 1240 | ` *  The number of matching characters in both strings.` |
|      - | 1241 | ` */` |
|     28 | 1242 | `static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1243 | `{` |
|      - | 1244 | `	const char *zStr1,*zStr2;` |
|      - | 1245 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1246 | `	int nLen1,nLen2;` |
|      - | 1247 | `	int nSim;` |
|      - | 1248 | `	sxi32 rc;` |
|     29 | 1249 | `	if( nArg < 2 ){` |
|      4 | 1250 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1251 | `			"ArgumentCountError",` |
|      - | 1252 | `			"similar_text() expects at least 2 arguments, %d given",` |
|      1 | 1253 | `			nArg` |
|      - | 1254 | `			);` |
|      - | 1255 | `	}` |
|     27 | 1256 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     27 | 1257 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     27 | 1258 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - | 1259 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - | 1260 | `		"of type string is deprecated",` |
|      - | 1261 | `		&sTmp1,&zStr1,&nLen1);` |
|     27 | 1262 | `	if( rc != PH7_OK ) goto out;` |
|     25 | 1263 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - | 1264 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - | 1265 | `		"of type string is deprecated",` |
|      - | 1266 | `		&sTmp2,&zStr2,&nLen2);` |
|     25 | 1267 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1268 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 | 1269 | `		nSim = 0;` |
|      3 | 1270 | `	}else{` |
|     19 | 1271 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - | 1272 | `	}` |
|     23 | 1273 | `	if( nArg > 2 ){` |
|      - | 1274 | `		/* Write the percentage through the by-ref out-param */` |
|      7 | 1275 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 | 1276 | `		if( pPercent == 0 ){` |
|    ! 0 | 1277 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1278 | `			goto out;` |
|    ! 0 | 1279 | `		}else{` |
|      7 | 1280 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 | 1281 | `			ph7_value_double(pPercent,dPct);` |
|      7 | 1282 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - | 1283 | `		}` |
|      3 | 1284 | `	}` |
|     23 | 1285 | `	ph7_result_int(pCtx,nSim);` |
|     23 | 1286 | `	rc = PH7_OK;` |
|     13 | 1287 | `out:` |
|     27 | 1288 | `	PH7_MemObjRelease(&sTmp1);` |
|     27 | 1289 | `	PH7_MemObjRelease(&sTmp2);` |
|     27 | 1290 | `	return rc;` |
|     15 | 1291 | `}` |
|      - | 1292 | `/*` |
|      - | 1293 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - | 1294 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - | 1295 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - | 1296 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - | 1297 | ` *  in PHP's php_charmask).` |
|      - | 1298 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - | 1299 | ` *  by their byte position in $string.` |
|      - | 1300 | ` * Errors` |
|      - | 1301 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - | 1302 | ` */` |
|     52 | 1303 | `static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1304 | `{` |
|      - | 1305 | `	const char *zIn,*zEnd,*zPtr;` |
|     53 | 1306 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - | 1307 | `	ph7_value sTmp,sListTmp;` |
|      - | 1308 | `	char aMask[256];` |
|     53 | 1309 | `	int bMask = 0;` |
|     53 | 1310 | `	int iFormat = 0;` |
|     53 | 1311 | `	int nCount = 0;` |
|      - | 1312 | `	int nLen;` |
|      - | 1313 | `	sxi32 rc;` |
|     53 | 1314 | `	if( nArg < 1 ){` |
|      4 | 1315 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1316 | `			"ArgumentCountError",` |
|      - | 1317 | `			"str_word_count() expects at least 1 argument, %d given",` |
|      1 | 1318 | `			nArg` |
|      - | 1319 | `			);` |
|      - | 1320 | `	}` |
|     51 | 1321 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     51 | 1322 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     51 | 1323 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - | 1324 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - | 1325 | `		"of type string is deprecated",` |
|      - | 1326 | `		&sTmp,&zIn,&nLen);` |
|     51 | 1327 | `	if( rc != PH7_OK ) goto out;` |
|     49 | 1328 | `	if( nArg > 1 ){` |
|      - | 1329 | `		sxi64 iVal;` |
|     35 | 1330 | `		rc = IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     37 | 1331 | `		if( rc != PH7_OK ) goto out;` |
|     33 | 1332 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 | 1333 | `			rc = PH7_VmThrowException(pCtx,` |
|      - | 1334 | `				"ValueError",` |
|      - | 1335 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - | 1336 | `				);` |
|      5 | 1337 | `			goto out;` |
|      - | 1338 | `		}` |
|     29 | 1339 | `		iFormat = (int)iVal;` |
|     14 | 1340 | `	}` |
|     43 | 1341 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1342 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - | 1343 | `		 * default word set, no deprecation. */` |
|      - | 1344 | `		const char *zList;` |
|      - | 1345 | `		int nList;` |
|     17 | 1346 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - | 1347 | `			"" /* unreachable: null never gets here */,` |
|      - | 1348 | `			&sListTmp,&zList,&nList);` |
|     17 | 1349 | `		if( rc != PH7_OK ) goto out;` |
|     13 | 1350 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 | 1351 | `		bMask = 1;` |
|      6 | 1352 | `	}` |
|     39 | 1353 | `	if( iFormat != 0 ){` |
|     25 | 1354 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 | 1355 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 | 1356 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1357 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1358 | `			goto out;` |
|      - | 1359 | `		}` |
|     12 | 1360 | `	}` |
|     39 | 1361 | `	zPtr = zIn;` |
|     39 | 1362 | `	zEnd = &zIn[nLen];` |
|     39 | 1363 | `	if( nLen > 0 ){` |
|      - | 1364 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - | 1365 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 | 1366 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 | 1367 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 | 1368 | `			zPtr++;` |
|      4 | 1369 | `		}` |
|     33 | 1370 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 | 1371 | `			zEnd--;` |
|      4 | 1372 | `		}` |
|     16 | 1373 | `	}` |
|    135 | 1374 | `	while( zPtr < zEnd ){` |
|     91 | 1375 | `		const char *zStart = zPtr;` |
|    477 | 1376 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 | 1377 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 | 1378 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 | 1379 | `			zPtr++;` |
|      1 | 1380 | `		}` |
|     97 | 1381 | `		if( zPtr > zStart ){` |
|     91 | 1382 | `			if( iFormat == 0 ){` |
|     19 | 1383 | `				nCount++;` |
|     10 | 1384 | `			}else{` |
|     73 | 1385 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 | 1386 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 | 1387 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1388 | `					goto out;` |
|      - | 1389 | `				}` |
|     73 | 1390 | `				if( iFormat == 1 ){` |
|     59 | 1391 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1392 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1393 | `						goto out;` |
|      - | 1394 | `					}` |
|     30 | 1395 | `				}else{` |
|     15 | 1396 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1397 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1398 | `						goto out;` |
|      - | 1399 | `					}` |
|      - | 1400 | `				}` |
|      - | 1401 | `			}` |
|     45 | 1402 | `		}` |
|     97 | 1403 | `		zPtr++;` |
|      1 | 1404 | `	}` |
|     37 | 1405 | `	if( iFormat == 0 ){` |
|     13 | 1406 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1407 | `	}else{` |
|     25 | 1408 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1409 | `	}` |
|     37 | 1410 | `	rc = PH7_OK;` |
|     24 | 1411 | `out:` |
|     49 | 1412 | `	PH7_MemObjRelease(&sTmp);` |
|     49 | 1413 | `	PH7_MemObjRelease(&sListTmp);` |
|     49 | 1414 | `	return rc;` |
|     26 | 1415 | `}` |
|      - | 1416 | `/*` |
|      - | 1417 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1418 | ` *   Split a string into smaller chunks.` |
|      - | 1419 | ` * Parameters` |
|      - | 1420 | ` *  $body` |
|      - | 1421 | ` *   The string to be chunked.` |
|      - | 1422 | ` * $chunklen` |
|      - | 1423 | ` *   The chunk length.` |
|      - | 1424 | ` * $end` |
|      - | 1425 | ` *   The line ending sequence.` |
|      - | 1426 | ` * Return` |
|      - | 1427 | ` *  The chunked string or NULL on failure.` |
|      - | 1428 | ` */` |
|     14 | 1429 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1430 | `{` |
|     15 | 1431 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1432 | `	int nSepLen,nChunkLen,nLen;` |
|     15 | 1433 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1434 | `		/* Nothing to split,return null */` |
|      3 | 1435 | `		ph7_result_null(pCtx);` |
|      3 | 1436 | `		return PH7_OK;` |
|      - | 1437 | `	}` |
|      - | 1438 | `	/* initialize/Extract arguments */` |
|     13 | 1439 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1440 | `	nChunkLen = 76;` |
|     13 | 1441 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1442 | `	zEnd = &zIn[nLen];` |
|     13 | 1443 | `	if( nArg > 1 ){` |
|      - | 1444 | `		/* Chunk length */` |
|     13 | 1445 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1446 | `		if( nChunkLen < 1 ){` |
|      - | 1447 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1448 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1449 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1450 | `		}` |
|     11 | 1451 | `		if( nArg > 2 ){` |
|      - | 1452 | `			/* Separator */` |
|      9 | 1453 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1454 | `			if( nSepLen < 1 ){` |
|      - | 1455 | `				/* Switch back to the default separator */` |
|      3 | 1456 | `				zSep = "\r\n";` |
|      3 | 1457 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1458 | `			}` |
|      4 | 1459 | `		}` |
|      5 | 1460 | `	}` |
|      - | 1461 | `	/* Perform the requested operation */` |
|     11 | 1462 | `	if( nChunkLen > nLen ){` |
|      - | 1463 | `		/* Nothing to split,return the string and the separator */` |
|      7 | 1464 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      7 | 1465 | `		return PH7_OK;` |
|      - | 1466 | `	}` |
|     17 | 1467 | `	while( zIn < zEnd ){` |
|     13 | 1468 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1469 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1470 | `		}` |
|      - | 1471 | `		/* Append the chunk and the separator */` |
|     13 | 1472 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1473 | `		/* Point beyond the chunk */` |
|     13 | 1474 | `		zIn += nChunkLen;` |
|      1 | 1475 | `	}` |
|      5 | 1476 | `	return PH7_OK;` |
|      8 | 1477 | `}` |
|      - | 1478 | `/*` |
|      - | 1479 | ` * string addslashes(string $str)` |
|      - | 1480 | ` *  Quote string with slashes.` |
|      - | 1481 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1482 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1483 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1484 | ` * Parameter` |
|      - | 1485 | ` *  str: The string to be escaped.` |
|      - | 1486 | ` * Return` |
|      - | 1487 | ` *  Returns the escaped string` |
|      - | 1488 | ` */` |
|     24 | 1489 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1490 | `{` |
|      - | 1491 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1492 | `	int nLen;` |
|      - | 1493 | `	/* PHP enforces exactly one argument. */` |
|     28 | 1494 | `	if( nArg != 1 ){` |
|      8 | 1495 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1496 | `			"ArgumentCountError",` |
|      - | 1497 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 | 1498 | `			nArg` |
|      - | 1499 | `			);` |
|      - | 1500 | `	}` |
|      - | 1501 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1502 | `	 * types still produce a TypeError. */` |
|     22 | 1503 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1504 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1505 | `			E_DEPRECATED,` |
|      - | 1506 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1507 | `			);` |
|      - | 1508 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1509 | `	}` |
|      - | 1510 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     29 | 1511 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 | 1512 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1513 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1514 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1515 | `			"TypeError",` |
|      - | 1516 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1517 | `			ph7_type_name(apArg[0])` |
|      - | 1518 | `			);` |
|      - | 1519 | `	}` |
|      - | 1520 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1521 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1522 | `	if( nLen < 1 ){` |
|      - | 1523 | `		/* Return the empty string */` |
|      5 | 1524 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1525 | `		return PH7_OK;` |
|      - | 1526 | `	}` |
|     15 | 1527 | `	zEnd = &zIn[nLen];` |
|     15 | 1528 | `	zCur = 0; /* cc warning */` |
|     20 | 1529 | `	for(;;){` |
|     41 | 1530 | `		if( zIn >= zEnd ){` |
|      - | 1531 | `			/* No more input */` |
|     15 | 1532 | `			break;` |
|      - | 1533 | `		}` |
|     27 | 1534 | `		zCur = zIn;` |
|      - | 1535 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1536 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1537 | `			zIn++;` |
|      1 | 1538 | `		}` |
|     27 | 1539 | `		if( zIn > zCur ){` |
|      - | 1540 | `			/* Append raw contents */` |
|     23 | 1541 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1542 | `		}` |
|     27 | 1543 | `		if( zIn < zEnd ){` |
|     17 | 1544 | `			int c = zIn[0];` |
|     17 | 1545 | `			if( c == '\0' ){` |
|      - | 1546 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1547 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1548 | `			}else{` |
|     15 | 1549 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1550 | `			}` |
|      8 | 1551 | `		}` |
|     27 | 1552 | `		zIn++;` |
|      1 | 1553 | `	}` |
|     15 | 1554 | `	return PH7_OK;` |
|     16 | 1555 | `}` |
|      - | 1556 | `/*` |
|      - | 1557 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1558 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1559 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1560 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1561 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1562 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1563 | ` * [zList, zList+nLen).` |
|      - | 1564 | ` *` |
|      - | 1565 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1566 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1567 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1568 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1569 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1570 | ` */` |
|    106 | 1571 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1572 | `{` |
|    111 | 1573 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    111 | 1574 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    111 | 1575 | `	SyZero(aMask,256);` |
|    379 | 1576 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    273 | 1577 | `		int c = zIn[0];` |
|    273 | 1578 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1579 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1580 | `			int hi = zIn[3],k;` |
|    386 | 1581 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1582 | `				aMask[k] = 1;` |
|    184 | 1583 | `			}` |
|     22 | 1584 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    272 | 1585 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1586 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1587 | `			const char *zMsg;` |
|     20 | 1588 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1589 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1590 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1591 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1592 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1593 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1594 | `			}else{` |
|    ! 0 | 1595 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1596 | `			}` |
|     20 | 1597 | `			if( zMsg ){` |
|     29 | 1598 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1599 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1600 | `			}else{` |
|    ! 0 | 1601 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1602 | `					"Invalid '..'-range");` |
|      - | 1603 | `			}` |
|      - | 1604 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1605 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1606 | `		}else{` |
|    235 | 1607 | `			aMask[c] = 1;` |
|      - | 1608 | `		}` |
|    139 | 1609 | `	}` |
|    111 | 1610 | `}` |
|      - | 1611 | `/*` |
|      - | 1612 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1613 | ` *  Quote string with slashes in a C style.` |
|      - | 1614 | ` * Parameter` |
|      - | 1615 | ` *  $str:` |
|      - | 1616 | ` *    The string to be escaped.` |
|      - | 1617 | ` *  $charlist:` |
|      - | 1618 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1619 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1620 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1621 | ` * Return` |
|      - | 1622 | ` *  Returns the escaped string.` |
|      - | 1623 | ` * Note:` |
|      - | 1624 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1625 | ` */` |
|     40 | 1626 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1627 | `{` |
|      - | 1628 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1629 | `	char aMask[256];` |
|      - | 1630 | `	int nLen,nMask;` |
|      - | 1631 | `	/* PHP enforces exactly two arguments. */` |
|     45 | 1632 | `	if( nArg != 2 ){` |
|      8 | 1633 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1634 | `			"ArgumentCountError",` |
|      - | 1635 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 | 1636 | `			nArg` |
|      - | 1637 | `			);` |
|      - | 1638 | `	}` |
|      - | 1639 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1640 | `	 * treated as the empty string (PHP 8.1). */` |
|     41 | 1641 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1642 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1643 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1644 | `			E_DEPRECATED,` |
|      - | 1645 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1646 | `			);` |
|      - | 1647 | `		/* treat as empty string; fall through to conversion logic */` |
|     52 | 1648 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     53 | 1649 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     32 | 1650 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1651 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1652 | `			"TypeError",` |
|      - | 1653 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1654 | `			ph7_type_name(apArg[0])` |
|      - | 1655 | `			);` |
|      - | 1656 | `	}` |
|      - | 1657 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1658 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1659 | `	 * trigger a TypeError. */` |
|     38 | 1660 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1661 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1662 | `			E_DEPRECATED,` |
|      - | 1663 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1664 | `			);` |
|      - | 1665 | `		/* allow through so it becomes empty string below */` |
|     49 | 1666 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     49 | 1667 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 | 1668 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 | 1669 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1670 | `			"TypeError",` |
|      - | 1671 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 | 1672 | `			ph7_type_name(apArg[1])` |
|      - | 1673 | `			);` |
|      - | 1674 | `	}` |
|      - | 1675 | `	/* Extract the string to process */` |
|     35 | 1676 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1677 | `	/* NULL would never reach here due to the check above. */` |
|     35 | 1678 | `	if( nLen < 1 ){` |
|      - | 1679 | `		/* Empty string returns itself. */` |
|      5 | 1680 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1681 | `		return PH7_OK;` |
|      - | 1682 | `	}` |
|      - | 1683 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 | 1684 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 | 1685 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 | 1686 | `	zEnd = &zIn[nLen];` |
|     31 | 1687 | `	zCur = 0; /* cc warning */` |
|     37 | 1688 | `	for(;;){` |
|     77 | 1689 | `		if( zIn >= zEnd ){` |
|      - | 1690 | `			/* No more input */` |
|     31 | 1691 | `			break;` |
|      - | 1692 | `		}` |
|     49 | 1693 | `		zCur = zIn;` |
|    125 | 1694 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 | 1695 | `			zIn++;` |
|      3 | 1696 | `		}` |
|     49 | 1697 | `		if( zIn > zCur ){` |
|      - | 1698 | `			/* Append raw contents */` |
|     43 | 1699 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 | 1700 | `		}` |
|     49 | 1701 | `		if( zIn < zEnd ){` |
|      - | 1702 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1703 | `			 * on platforms where char is signed. */` |
|     29 | 1704 | `			int c = (unsigned char)zIn[0];` |
|      - | 1705 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1706 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1707 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1708 | `			if( c == '\n' ){` |
|      3 | 1709 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1710 | `			}else if( c == '\r' ){` |
|      3 | 1711 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1712 | `			}else if( c == '\t' ){` |
|      3 | 1713 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1714 | `			}else if( c == '\v' ){` |
|      3 | 1715 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1716 | `			}else if( c == '\f' ){` |
|      3 | 1717 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1718 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1719 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1720 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1721 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1722 | `			}else{` |
|     13 | 1723 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1724 | `			}` |
|     13 | 1725 | `		}` |
|     49 | 1726 | `		zIn++;` |
|      3 | 1727 | `	}` |
|     31 | 1728 | `	return PH7_OK;` |
|     25 | 1729 | `}` |
|      - | 1730 | `/*` |
|      - | 1731 | ` * string quotemeta(string $str)` |
|      - | 1732 | ` *  Quote meta characters.` |
|      - | 1733 | ` * Parameter` |
|      - | 1734 | ` *  $str:` |
|      - | 1735 | ` *    The string to be escaped.` |
|      - | 1736 | ` * Return` |
|      - | 1737 | ` *  Returns the escaped string.` |
|      - | 1738 | `*/` |
|     10 | 1739 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1740 | `{` |
|      - | 1741 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1742 | `	char aMask[256];` |
|      - | 1743 | `	int nLen;` |
|     12 | 1744 | `	if( nArg < 1 ){` |
|      - | 1745 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1746 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1747 | `		return PH7_OK;` |
|      - | 1748 | `	}` |
|      - | 1749 | `	/* Extract the string to process */` |
|     12 | 1750 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1751 | `	if( nLen < 1 ){` |
|      - | 1752 | `		/* Return the empty string */` |
|      3 | 1753 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1754 | `		return PH7_OK;` |
|      - | 1755 | `	}` |
|      - | 1756 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1757 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1758 | `	zEnd = &zIn[nLen];` |
|     10 | 1759 | `	zCur = 0; /* cc warning */` |
|     22 | 1760 | `	for(;;){` |
|     46 | 1761 | `		if( zIn >= zEnd ){` |
|      - | 1762 | `			/* No more input */` |
|     10 | 1763 | `			break;` |
|      - | 1764 | `		}` |
|     38 | 1765 | `		zCur = zIn;` |
|     76 | 1766 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1767 | `			zIn++;` |
|      2 | 1768 | `		}` |
|     38 | 1769 | `		if( zIn > zCur ){` |
|      - | 1770 | `			/* Append raw contents */` |
|     20 | 1771 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1772 | `		}` |
|     38 | 1773 | `		if( zIn < zEnd ){` |
|     36 | 1774 | `			int c = zIn[0];` |
|     36 | 1775 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1776 | `		}` |
|     38 | 1777 | `		zIn++;` |
|      2 | 1778 | `	}` |
|     10 | 1779 | `	return PH7_OK;` |
|      7 | 1780 | `}` |
|      - | 1781 | `/*` |
|      - | 1782 | ` * string stripslashes(string $str)` |
|      - | 1783 | ` *  Un-quotes a quoted string.` |
|      - | 1784 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1785 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1786 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1787 | ` * Parameter` |
|      - | 1788 | ` *  $str` |
|      - | 1789 | ` *   The input string.` |
|      - | 1790 | ` * Return` |
|      - | 1791 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1792 | ` */` |
|      6 | 1793 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1794 | `{` |
|      - | 1795 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1796 | `	int nLen;` |
|      7 | 1797 | `	if( nArg < 1 ){` |
|      - | 1798 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1799 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1800 | `		return PH7_OK;` |
|      - | 1801 | `	}` |
|      - | 1802 | `	/* Extract the string to process */` |
|      7 | 1803 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1804 | `	if( zIn == 0 ){` |
|    ! 0 | 1805 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1806 | `		return PH7_OK;` |
|      - | 1807 | `	}` |
|      7 | 1808 | `	zEnd = &zIn[nLen];` |
|      7 | 1809 | `	zCur = 0; /* cc warning */` |
|      - | 1810 | `	/* Encode the string */` |
|      4 | 1811 | `	for(;;){` |
|      9 | 1812 | `		if( zIn >= zEnd ){` |
|      - | 1813 | `			/* No more input */` |
|      5 | 1814 | `			break;` |
|      - | 1815 | `		}` |
|      5 | 1816 | `		zCur = zIn;` |
|     17 | 1817 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1818 | `			zIn++;` |
|      1 | 1819 | `		}` |
|      5 | 1820 | `		if( zIn > zCur ){` |
|      - | 1821 | `			/* Append raw contents */` |
|      5 | 1822 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1823 | `		}` |
|      5 | 1824 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1825 | `			int c = zIn[1];` |
|      3 | 1826 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1827 | `				/* Ignore the backslash */` |
|      3 | 1828 | `				zIn++;` |
|      1 | 1829 | `			}` |
|      2 | 1830 | `		}else{` |
|      3 | 1831 | `			break;` |
|      - | 1832 | `		}` |
|      1 | 1833 | `	}` |
|      7 | 1834 | `	return PH7_OK;` |
|      4 | 1835 | `}` |
|      - | 1836 | `/*` |
|      - | 1837 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1838 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1839 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1840 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1841 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1842 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1843 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1844 | ` *` |
|      - | 1845 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1846 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1847 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1848 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1849 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1850 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1851 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1852 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1853 | ` */` |
|      - | 1854 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1855 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1856 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1857 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1858 | `/*` |
|      - | 1859 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1860 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1861 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1862 | ` * Return` |
|      - | 1863 | ` *  The escaped string or NULL on failure.` |
|      - | 1864 | ` */` |
|     42 | 1865 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1866 | `{` |
|     43 | 1867 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1868 | `	const char *zIn;` |
|     43 | 1869 | `	int nLen,bDouble = 1;` |
|     43 | 1870 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1871 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1872 | `		ph7_result_null(pCtx);` |
|      3 | 1873 | `		return PH7_OK;` |
|      - | 1874 | `	}` |
|      - | 1875 | `	/* Extract the target string */` |
|     41 | 1876 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 1877 | `	if( nArg > 1 ){` |
|     35 | 1878 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1879 | `	}` |
|     41 | 1880 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     41 | 1881 | `	if( nArg > 3 ){` |
|      7 | 1882 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1883 | `	}` |
|     41 | 1884 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     41 | 1885 | `	return PH7_OK;` |
|     22 | 1886 | `}` |
|      - | 1887 | `/*` |
|      - | 1888 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1889 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1890 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1891 | ` * Return` |
|      - | 1892 | ` *  The unescaped string or NULL on failure.` |
|      - | 1893 | ` */` |
|     22 | 1894 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1895 | `{` |
|     23 | 1896 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1897 | `	const char *zIn;` |
|      - | 1898 | `	int nLen;` |
|     23 | 1899 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1900 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1901 | `		ph7_result_null(pCtx);` |
|      3 | 1902 | `		return PH7_OK;` |
|      - | 1903 | `	}` |
|      - | 1904 | `	/* Extract the target string */` |
|     21 | 1905 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     21 | 1906 | `	if( nArg > 1 ){` |
|      9 | 1907 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1908 | `	}` |
|     21 | 1909 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     21 | 1910 | `	return PH7_OK;` |
|     12 | 1911 | `}` |
|      - | 1912 | `/*` |
|      - | 1913 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1914 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1915 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1916 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1917 | ` * Return` |
|      - | 1918 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1919 | ` */` |
|     12 | 1920 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1921 | `{` |
|     13 | 1922 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1923 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1924 | `	if( nArg > 0 ){` |
|     11 | 1925 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1926 | `	}` |
|     13 | 1927 | `	if( nArg > 1 ){` |
|      9 | 1928 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1929 | `	}` |
|     13 | 1930 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1931 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1932 | `	return PH7_OK;` |
|      1 | 1933 | `}` |
|      - | 1934 | `/*` |
|      - | 1935 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1936 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1937 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1938 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1939 | ` * Return` |
|      - | 1940 | ` *  The encoded string or NULL on failure.` |
|      - | 1941 | ` */` |
|     30 | 1942 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1943 | `{` |
|     31 | 1944 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1945 | `	const char *zIn;` |
|     31 | 1946 | `	int nLen,bDouble = 1;` |
|     31 | 1947 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1948 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1949 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1950 | `		return PH7_OK;` |
|      - | 1951 | `	}` |
|      - | 1952 | `	/* Extract the target string */` |
|     31 | 1953 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1954 | `	if( nArg > 1 ){` |
|     19 | 1955 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1956 | `	}` |
|     31 | 1957 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1958 | `	if( nArg > 3 ){` |
|      3 | 1959 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1960 | `	}` |
|     31 | 1961 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1962 | `	return PH7_OK;` |
|     16 | 1963 | `}` |
|      - | 1964 | `/*` |
|      - | 1965 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1966 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1967 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1968 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1969 | ` * Return` |
|      - | 1970 | ` *  The decoded string or NULL on failure.` |
|      - | 1971 | ` */` |
|     58 | 1972 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1973 | `{` |
|     59 | 1974 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1975 | `	const char *zIn;` |
|      - | 1976 | `	int nLen;` |
|     59 | 1977 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1978 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1979 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1980 | `		return PH7_OK;` |
|      - | 1981 | `	}` |
|      - | 1982 | `	/* Extract the target string */` |
|     59 | 1983 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1984 | `	if( nArg > 1 ){` |
|     27 | 1985 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1986 | `	}` |
|     59 | 1987 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1988 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 1989 | `	return PH7_OK;` |
|     30 | 1990 | `}` |
|      - | 1991 | `/*` |
|      - | 1992 | ` * int strlen($string)` |
|      - | 1993 | ` *  return the length of the given string.` |
|      - | 1994 | ` * Parameter` |
|      - | 1995 | ` *  string: The string being measured for length.` |
|      - | 1996 | ` * Return` |
|      - | 1997 | ` *  length of the given string.` |
|      - | 1998 | ` */` |
|  14152 | 1999 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2000 | `{` |
|  14157 | 2001 | `	int iLen = 0;` |
|  14157 | 2002 | `	if( nArg > 0 ){` |
|  14157 | 2003 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  14157 | 2004 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   7076 | 2005 | `	}` |
|      - | 2006 | `	/* String length */` |
|  14157 | 2007 | `	ph7_result_int(pCtx,iLen);` |
|  14157 | 2008 | `	return PH7_OK;` |
|      5 | 2009 | `}` |
|      - | 2010 | `/*` |
|      - | 2011 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2012 | ` *  Perform a binary safe string comparison.` |
|      - | 2013 | ` * Parameter` |
|      - | 2014 | ` *  str1: The first string` |
|      - | 2015 | ` *  str2: The second string` |
|      - | 2016 | ` * Return` |
|      - | 2017 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2018 | ` *  than str2, and 0 if they are equal.` |
|      - | 2019 | ` */` |
|     72 | 2020 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2021 | `{` |
|      - | 2022 | `	const char *z1,*z2;` |
|      - | 2023 | `	int n1,n2;` |
|      - | 2024 | `	int res;` |
|     73 | 2025 | `	if( nArg < 2 ){` |
|    ! 0 | 2026 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2027 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2028 | `		return PH7_OK;` |
|      - | 2029 | `	}` |
|      - | 2030 | `	/* Perform the comparison */` |
|     73 | 2031 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 2032 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 2033 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2034 | `	/* Comparison result */` |
|     73 | 2035 | `	ph7_result_int(pCtx,res);` |
|     73 | 2036 | `	return PH7_OK;` |
|     37 | 2037 | `}` |
|      - | 2038 | `/*` |
|      - | 2039 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 2040 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 2041 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 2042 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 2043 | ` */` |
|     16 | 2044 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2045 | `{` |
|     17 | 2046 | `	int bias = 0;` |
|     30 | 2047 | `	for(;;){` |
|     39 | 2048 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     39 | 2049 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     39 | 2050 | `		if( !da && !db ){ return bias; }` |
|     31 | 2051 | `		if( !da ){ return -1; }` |
|     25 | 2052 | `		if( !db ){ return 1; }` |
|     23 | 2053 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     21 | 2054 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     23 | 2055 | `		(*pa)++;` |
|     23 | 2056 | `		(*pb)++;` |
|      1 | 2057 | `	}` |
|      9 | 2058 | `}` |
|      2 | 2059 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2060 | `{` |
|      1 | 2061 | `	for(;;){` |
|      3 | 2062 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      3 | 2063 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      3 | 2064 | `		if( !da && !db ){ return 0; }` |
|      3 | 2065 | `		if( !da ){ return -1; }` |
|      3 | 2066 | `		if( !db ){ return 1; }` |
|      3 | 2067 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 2068 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 2069 | `		(*pa)++;` |
|    ! 0 | 2070 | `		(*pb)++;` |
|    ! 0 | 2071 | `	}` |
|      2 | 2072 | `}` |
|     20 | 2073 | `static int StrNatCmpCore(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 2074 | `{` |
|     21 | 2075 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     21 | 2076 | `	const char *b = zB,*bEnd = &zB[nB];` |
|     59 | 2077 | `	for(;;){` |
|      - | 2078 | `		int ca,cb;` |
|     73 | 2079 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|     71 | 2080 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|     71 | 2081 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|     71 | 2082 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|     71 | 2083 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     18 | 2084 | `			int r = (ca == '0' \|\| cb == '0')` |
|      2 | 2085 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     25 | 2086 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     19 | 2087 | `			if( r ){ return r; }` |
|      3 | 2088 | `			continue;` |
|      - | 2089 | `		}` |
|     53 | 2090 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|     49 | 2091 | `		if( bFold ){` |
|     49 | 2092 | `			ca = SyToLower(ca);` |
|     49 | 2093 | `			cb = SyToLower(cb);` |
|     24 | 2094 | `		}` |
|     49 | 2095 | `		if( ca < cb ){ return -1; }` |
|     49 | 2096 | `		if( ca > cb ){ return 1; }` |
|     49 | 2097 | `		a++;` |
|     49 | 2098 | `		b++;` |
|      1 | 2099 | `	}` |
|     11 | 2100 | `}` |
|      - | 2101 | `/*` |
|      - | 2102 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 2103 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 2104 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 2105 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 2106 | ` */` |
|     20 | 2107 | `static int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2108 | `{` |
|      - | 2109 | `	const char *z1,*z2,*zFunc;` |
|      - | 2110 | `	int n1,n2,bFold;` |
|     21 | 2111 | `	if( nArg < 2 ){` |
|    ! 0 | 2112 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2113 | `		return PH7_OK;` |
|      - | 2114 | `	}` |
|     21 | 2115 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 2116 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 2117 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 2118 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 2119 | `	ph7_result_int(pCtx,StrNatCmpCore(z1,n1,z2,n2,bFold));` |
|     21 | 2120 | `	return PH7_OK;` |
|     11 | 2121 | `}` |
|      - | 2122 | `/*` |
|      - | 2123 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2124 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2125 | ` * Parameter` |
|      - | 2126 | ` *  str1: The first string` |
|      - | 2127 | ` *  str2: The second string` |
|      - | 2128 | ` * Return` |
|      - | 2129 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2130 | ` *  than str2, and 0 if they are equal.` |
|      - | 2131 | ` */` |
|     66 | 2132 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2133 | `{` |
|      - | 2134 | `	const char *z1,*z2;` |
|      - | 2135 | `	int res;` |
|      - | 2136 | `	int n;` |
|     68 | 2137 | `	if( nArg < 3 ){` |
|      - | 2138 | `		/* Perform a standard comparison */` |
|    ! 0 | 2139 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2140 | `	}` |
|      - | 2141 | `	/* Desired comparison length */` |
|     68 | 2142 | `	n  = ph7_value_to_int(apArg[2]);` |
|     68 | 2143 | `	if( n < 0 ){` |
|      - | 2144 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2145 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2146 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2147 | `			ph7_function_name(pCtx));` |
|      - | 2148 | `	}` |
|      - | 2149 | `	/* Perform the comparison */` |
|     66 | 2150 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     66 | 2151 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     66 | 2152 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2153 | `	/* Comparison result */` |
|     66 | 2154 | `	ph7_result_int(pCtx,res);` |
|     66 | 2155 | `	return PH7_OK;` |
|     35 | 2156 | `}` |
|      - | 2157 | `/*` |
|      - | 2158 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2159 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2160 | ` * Parameter` |
|      - | 2161 | ` *  str1: The first string` |
|      - | 2162 | ` *  str2: The second string` |
|      - | 2163 | ` * Return` |
|      - | 2164 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2165 | ` *  than str2, and 0 if they are equal.` |
|      - | 2166 | ` */` |
|    140 | 2167 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2168 | `{` |
|      - | 2169 | `	const char *z1,*z2;` |
|      - | 2170 | `	int n1,n2;` |
|      - | 2171 | `	int res;` |
|    141 | 2172 | `	if( nArg < 2 ){` |
|    ! 0 | 2173 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2174 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2175 | `		return PH7_OK;` |
|      - | 2176 | `	}` |
|      - | 2177 | `	/* Perform the comparison */` |
|    141 | 2178 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    141 | 2179 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    141 | 2180 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2181 | `	/* Comparison result */` |
|    141 | 2182 | `	ph7_result_int(pCtx,res);` |
|    141 | 2183 | `	return PH7_OK;` |
|     71 | 2184 | `}` |
|      - | 2185 | `/*` |
|      - | 2186 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2187 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2188 | ` * Parameter` |
|      - | 2189 | ` *  $str1: The first string` |
|      - | 2190 | ` *  $str2: The second string` |
|      - | 2191 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2192 | ` * Return` |
|      - | 2193 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2194 | ` *  than str2, and 0 if they are equal.` |
|      - | 2195 | ` */` |
|     42 | 2196 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2197 | `{` |
|      - | 2198 | `	const char *z1,*z2;` |
|      - | 2199 | `	int res;` |
|      - | 2200 | `	int n;` |
|     47 | 2201 | `	if( nArg < 3 ){` |
|      - | 2202 | `		/* Perform a standard comparison */` |
|    ! 0 | 2203 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2204 | `	}` |
|      - | 2205 | `	/* Desired comparison length */` |
|     47 | 2206 | `	n  = ph7_value_to_int(apArg[2]);` |
|     47 | 2207 | `	if( n < 0 ){` |
|      - | 2208 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2209 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2210 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2211 | `			ph7_function_name(pCtx));` |
|      - | 2212 | `	}` |
|      - | 2213 | `	/* Perform the comparison */` |
|     45 | 2214 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     45 | 2215 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     45 | 2216 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2217 | `	/* Comparison result */` |
|     45 | 2218 | `	ph7_result_int(pCtx,res);` |
|     45 | 2219 | `	return PH7_OK;` |
|     26 | 2220 | `}` |
|      - | 2221 | `/*` |
|      - | 2222 | ` * Implode context [i.e: it's private data].` |
|      - | 2223 | ` * A pointer to the following structure is forwarded` |
|      - | 2224 | ` * verbatim to the array walker callback defined below.` |
|      - | 2225 | ` */` |
|      - | 2226 | `struct implode_data {` |
|      - | 2227 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2228 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2229 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2230 | `	int nSeplen;          /* Separator length */` |
|      - | 2231 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2232 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2233 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2234 | `};` |
|      - | 2235 | `/*` |
|      - | 2236 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2237 | ` * The following routine is invoked for each array entry passed` |
|      - | 2238 | ` * to the implode() function.` |
|      - | 2239 | ` */` |
| 149022 | 2240 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2241 | `{` |
|  74511 | 2242 | `	SXUNUSED(pKey);` |
| 149027 | 2243 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2244 | `	const char *zData;` |
|      - | 2245 | `	int nLen;` |
| 149027 | 2246 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2247 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2248 | `			if( !pData->bFirst ){` |
|      - | 2249 | `				/* append the separator first */` |
|      3 | 2250 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2251 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2252 | `					return PH7_ABORT;` |
|      - | 2253 | `				}` |
|      2 | 2254 | `			}else{` |
|    ! 0 | 2255 | `				pData->bFirst = 0;` |
|      - | 2256 | `			}` |
|      1 | 2257 | `		}` |
|      - | 2258 | `		/* Recurse */` |
|      3 | 2259 | `		pData->bFirst = 1;` |
|      3 | 2260 | `		pData->nRecCount++;` |
|      3 | 2261 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2262 | `		pData->nRecCount--;` |
|      - | 2263 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2264 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2265 | `			return PH7_ABORT;` |
|      - | 2266 | `		}` |
|      3 | 2267 | `		return PH7_OK;` |
|      - | 2268 | `	}` |
|      - | 2269 | `	/* Extract the string representation of the entry value */` |
| 149025 | 2270 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2271 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 149025 | 2272 | `	if( pData->bFirst ){` |
|  33693 | 2273 | `		pData->bFirst = 0;` |
| 132181 | 2274 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2275 | `		/* append the separator first */` |
| 115321 | 2276 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2277 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2278 | `			return PH7_ABORT;` |
|      - | 2279 | `		}` |
|  57658 | 2280 | `	}` |
|      - | 2281 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 149025 | 2282 | `	if( nLen > 0 ){` |
| 136615 | 2283 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2284 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2285 | `			return PH7_ABORT;` |
|      - | 2286 | `		}` |
|  68305 | 2287 | `	}` |
| 149025 | 2288 | `	return PH7_OK;` |
|  74516 | 2289 | `}` |
|      - | 2290 | `/*` |
|      - | 2291 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2292 | ` * string implode(array $pieces,...)` |
|      - | 2293 | ` *  Join array elements with a string.` |
|      - | 2294 | ` * $glue` |
|      - | 2295 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2296 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2297 | ` * $pieces` |
|      - | 2298 | ` *   The array of strings to implode.` |
|      - | 2299 | ` * Return` |
|      - | 2300 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2301 | ` *  order, with the glue string between each element.` |
|      - | 2302 | ` */` |
|  33714 | 2303 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2304 | `{` |
|      - | 2305 | `	struct implode_data imp_data;` |
|  33719 | 2306 | `	int i = 1;` |
|  33719 | 2307 | `	if( nArg < 1 ){` |
|      - | 2308 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2309 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2310 | `		return PH7_OK;` |
|      - | 2311 | `	}` |
|      - | 2312 | `	/* Prepare the implode context */` |
|  33719 | 2313 | `	imp_data.pCtx = pCtx;` |
|  33719 | 2314 | `	imp_data.bRecursive = 0;` |
|  33719 | 2315 | `	imp_data.bFirst = 1;` |
|  33719 | 2316 | `	imp_data.nRecCount = 0;` |
|  33719 | 2317 | `	imp_data.rc = SXRET_OK;` |
|  33719 | 2318 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33717 | 2319 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16861 | 2320 | `	}else{` |
|      3 | 2321 | `		imp_data.zSep = 0;` |
|      3 | 2322 | `		imp_data.nSeplen = 0;` |
|      3 | 2323 | `		i = 0;` |
|      - | 2324 | `	}` |
|  33719 | 2325 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2326 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2327 | `	}` |
|      - | 2328 | `	/* Start the 'join' process */` |
|  67433 | 2329 | `	while( i < nArg ){` |
|  33719 | 2330 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2331 | `			/* Iterate throw array entries */` |
|  33719 | 2332 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2333 | `			/* Surface a callback allocation failure as a fatal */` |
|  33719 | 2334 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2335 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2336 | `			}` |
|  16862 | 2337 | `		}else{` |
|      - | 2338 | `			const char *zData;` |
|      - | 2339 | `			int nLen;` |
|      - | 2340 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2341 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2342 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2343 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2344 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2345 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2346 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2347 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2348 | `				}` |
|    ! 0 | 2349 | `			}` |
|      - | 2350 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2351 | `			if( nLen > 0 ){` |
|    ! 0 | 2352 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2353 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2354 | `				}` |
|    ! 0 | 2355 | `			}` |
|      - | 2356 | `		}` |
|  33719 | 2357 | `		i++;` |
|      5 | 2358 | `	}` |
|  33719 | 2359 | `	return PH7_OK;` |
|  16862 | 2360 | `}` |
|      - | 2361 | `/*` |
|      - | 2362 | ` * Symisc eXtension:` |
|      - | 2363 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2364 | ` * Purpose` |
|      - | 2365 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2366 | ` * Example:` |
|      - | 2367 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2368 | ` *   echo implode_recursive("/",$a);` |
|      - | 2369 | ` *   Will output` |
|      - | 2370 | ` *     usr/home/dean.` |
|      - | 2371 | ` *   While the standard implode would produce.` |
|      - | 2372 | ` *    usr/Array.` |
|      - | 2373 | ` * Parameter` |
|      - | 2374 | ` *  Refer to implode().` |
|      - | 2375 | ` * Return` |
|      - | 2376 | ` *  Refer to implode().` |
|      - | 2377 | ` */` |
|     12 | 2378 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2379 | `{` |
|      - | 2380 | `	struct implode_data imp_data;` |
|     13 | 2381 | `	int i = 1;` |
|     13 | 2382 | `	if( nArg < 1 ){` |
|      - | 2383 | `		/* Missing argument,return NULL */` |
|      3 | 2384 | `		ph7_result_null(pCtx);` |
|      3 | 2385 | `		return PH7_OK;` |
|      - | 2386 | `	}` |
|      - | 2387 | `	/* Prepare the implode context */` |
|     11 | 2388 | `	imp_data.pCtx = pCtx;` |
|     11 | 2389 | `	imp_data.bRecursive = 1;` |
|     11 | 2390 | `	imp_data.bFirst = 1;` |
|     11 | 2391 | `	imp_data.nRecCount = 0;` |
|     11 | 2392 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2393 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2394 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2395 | `	}else{` |
|    ! 0 | 2396 | `		imp_data.zSep = 0;` |
|    ! 0 | 2397 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2398 | `		i = 0;` |
|      - | 2399 | `	}` |
|     11 | 2400 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2401 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2402 | `	}` |
|      - | 2403 | `	/* Start the 'join' process */` |
|     21 | 2404 | `	while( i < nArg ){` |
|     11 | 2405 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2406 | `			/* Iterate throw array entries */` |
|      3 | 2407 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2408 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2409 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2410 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2411 | `			}` |
|      2 | 2412 | `		}else{` |
|      - | 2413 | `			const char *zData;` |
|      - | 2414 | `			int nLen;` |
|      - | 2415 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2416 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2417 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2418 | `			if( imp_data.bFirst ){` |
|      9 | 2419 | `				imp_data.bFirst = 0;` |
|      4 | 2420 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2421 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2422 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2423 | `				}` |
|    ! 0 | 2424 | `			}` |
|      - | 2425 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2426 | `			if( nLen > 0 ){` |
|      9 | 2427 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2428 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2429 | `				}` |
|      4 | 2430 | `			}` |
|      - | 2431 | `		}` |
|     11 | 2432 | `		i++;` |
|      1 | 2433 | `	}` |
|     11 | 2434 | `	return PH7_OK;` |
|      7 | 2435 | `}` |
|      - | 2436 | `/*` |
|      - | 2437 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2438 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2439 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2440 | ` * Parameters` |
|      - | 2441 | ` *  $delimiter` |
|      - | 2442 | ` *   The boundary string.` |
|      - | 2443 | ` * $string` |
|      - | 2444 | ` *   The input string.` |
|      - | 2445 | ` * $limit` |
|      - | 2446 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2447 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2448 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2449 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2450 | ` * Returns` |
|      - | 2451 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2452 | ` *  on boundaries formed by the delimiter.` |
|      - | 2453 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2454 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2455 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2456 | ` *  will be returned.` |
|      - | 2457 | ` * NOTE:` |
|      - | 2458 | ` *  Negative limit is not supported.` |
|      - | 2459 | ` */` |
|   6578 | 2460 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2461 | `{` |
|      - | 2462 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2463 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2464 | `	ph7_value *pArray;` |
|      - | 2465 | `	ph7_value *pValue;` |
|      - | 2466 | `	sxu32 nOfft;` |
|      - | 2467 | `	sxi32 rc;` |
|   6583 | 2468 | `	if( nArg < 2 ){` |
|      - | 2469 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2470 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2471 | `		return PH7_OK;` |
|      - | 2472 | `	}` |
|      - | 2473 | `	/* Extract the delimiter */` |
|   6583 | 2474 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6583 | 2475 | `	if( nDelim < 1 ){` |
|      - | 2476 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2477 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2478 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2479 | `	}` |
|      - | 2480 | `	/* Extract the string */` |
|   6579 | 2481 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6579 | 2482 | `	if( nStrlen < 1 ){` |
|      - | 2483 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2484 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2485 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 2486 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 2487 | `		if( pArrayTmp == 0 ){` |
|      - | 2488 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2489 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2490 | `			return PH7_OK;` |
|      - | 2491 | `		}` |
|      7 | 2492 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 2493 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 2494 | `			if( pValueTmp == 0 ){` |
|      - | 2495 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2496 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2497 | `				return PH7_OK;` |
|      - | 2498 | `			}` |
|      5 | 2499 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 2500 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2501 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2502 | `			}` |
|      2 | 2503 | `		}` |
|      7 | 2504 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 2505 | `		return PH7_OK;` |
|      - | 2506 | `	}` |
|      - | 2507 | `	/* Point to the end of the string */` |
|   6573 | 2508 | `	zEnd = &zString[nStrlen];` |
|      - | 2509 | `	/* Create the array */` |
|   6573 | 2510 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6573 | 2511 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6573 | 2512 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2513 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2514 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2515 | `		return PH7_OK;` |
|      - | 2516 | `	}` |
|      - | 2517 | `	/* Set a defualt limit */` |
|   6573 | 2518 | `	iLimit = SXI32_HIGH;` |
|   6573 | 2519 | `	if( nArg > 2 ){` |
|     38 | 2520 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 2521 | `		if( iLimit < 0 ){` |
|      - | 2522 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2523 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2524 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2525 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2526 | `			int nTotal = 1,nKeep;` |
|     17 | 2527 | `			const char *zScan = zString;` |
|      - | 2528 | `			sxu32 nScanOfft;` |
|     57 | 2529 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2530 | `				nTotal++;` |
|     41 | 2531 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2532 | `			}` |
|     17 | 2533 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2534 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2535 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2536 | `				/* Emit the next clean component */` |
|     23 | 2537 | `				zCur = &zString[nOfft];` |
|     23 | 2538 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2539 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2540 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2541 | `				}` |
|     23 | 2542 | `				zString = &zCur[nDelim];` |
|     23 | 2543 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2544 | `			}` |
|     17 | 2545 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2546 | `			return PH7_OK;` |
|      - | 2547 | `		}` |
|     22 | 2548 | `		if( iLimit == 0 ){` |
|      5 | 2549 | `			iLimit = 1;` |
|      2 | 2550 | `		}` |
|     22 | 2551 | `		iLimit--;` |
|      9 | 2552 | `	}` |
|      - | 2553 | `	/* Start exploding */` |
|  80518 | 2554 | `	for(;;){` |
| 161041 | 2555 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 161041 | 2556 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2557 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6557 | 2558 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6557 | 2559 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2560 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2561 | `			}` |
|   6557 | 2562 | `			break;` |
|      - | 2563 | `		}` |
|      - | 2564 | `		/* Point to the desired offset */` |
| 154489 | 2565 | `		zCur = &zString[nOfft];` |
|      - | 2566 | `		/* Perform the store operation (may be empty) */` |
| 154489 | 2567 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 154489 | 2568 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2569 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2570 | `		}` |
|      - | 2571 | `		/* Point beyond the delimiter */` |
| 154489 | 2572 | `		zString = &zCur[nDelim];` |
|      - | 2573 | `		/* Reset the cursor */` |
| 154489 | 2574 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2575 | `	}` |
|      - | 2576 | `	/* Return the freshly created array */` |
|   6557 | 2577 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2578 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2579 | `	 * released as soon we return from this foregin function.` |
|      - | 2580 | `	 */` |
|   6557 | 2581 | `	return PH7_OK;` |
|   3294 | 2582 | `}` |
|      - | 2583 | `/*` |
|      - | 2584 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2585 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2586 | ` * Parameters` |
|      - | 2587 | ` *  $str` |
|      - | 2588 | ` *   The string that will be trimmed.` |
|      - | 2589 | ` * $charlist` |
|      - | 2590 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2591 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2592 | ` *   With .. you can specify a range of characters.` |
|      - | 2593 | ` * Returns.` |
|      - | 2594 | ` *  Thr processed string.` |
|      - | 2595 | ` * NOTE:` |
|      - | 2596 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2597 | ` */` |
|  14844 | 2598 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2599 | `{` |
|  14849 | 2600 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2601 | `	const char *zString;` |
|      - | 2602 | `	int nLen;` |
|  14849 | 2603 | `	if( nArg < 1 ){` |
|      - | 2604 | `		/* Missing arguments,return null */` |
|    ! 0 | 2605 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2606 | `		return PH7_OK;` |
|      - | 2607 | `	}` |
|      - | 2608 | `	/* Extract the target string */` |
|  14849 | 2609 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14849 | 2610 | `	if( nLen < 1 ){` |
|      - | 2611 | `		/* Empty string,return */` |
|   1291 | 2612 | `		ph7_result_string(pCtx,"",0);` |
|   1291 | 2613 | `		return PH7_OK;` |
|      - | 2614 | `	}` |
|      - | 2615 | `	/* Start the trim process */` |
|  13563 | 2616 | `	if( nArg < 2 ){` |
|      - | 2617 | `		SyString sStr;` |
|      - | 2618 | `		/* Remove white spaces and NUL bytes */` |
|  13533 | 2619 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  33939 | 2620 | `		SyStringFullTrimSafe(&sStr);` |
|  13533 | 2621 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6769 | 2622 | `	}else{` |
|      - | 2623 | `		/* Char list */` |
|      - | 2624 | `		const char *zList;` |
|      - | 2625 | `		int nListlen;` |
|     33 | 2626 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2627 | `		if( nListlen < 1 ){` |
|      - | 2628 | `			/* Return the string unchanged */` |
|      6 | 2629 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2630 | `		}else{` |
|      - | 2631 | `			char aMask[256];` |
|     29 | 2632 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2633 | `			const char *zCur = zString;` |
|     29 | 2634 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2635 | `			/* Left trim */` |
|     79 | 2636 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2637 | `				zCur++;` |
|      3 | 2638 | `			}` |
|      - | 2639 | `			/* Right trim */` |
|     79 | 2640 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2641 | `				zEnd--;` |
|      3 | 2642 | `			}` |
|     29 | 2643 | `			if( zCur >= zEnd ){` |
|      - | 2644 | `				/* Return the empty string */` |
|    ! 0 | 2645 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2646 | `			}else{` |
|     29 | 2647 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2648 | `			}` |
|      - | 2649 | `		}` |
|      - | 2650 | `	}` |
|  13563 | 2651 | `	return PH7_OK;` |
|   7427 | 2652 | `}` |
|      - | 2653 | `/*` |
|      - | 2654 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2655 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2656 | ` * Parameters` |
|      - | 2657 | ` *  $str` |
|      - | 2658 | ` *   The string that will be trimmed.` |
|      - | 2659 | ` * $charlist` |
|      - | 2660 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2661 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2662 | ` *   With .. you can specify a range of characters.` |
|      - | 2663 | ` * Returns.` |
|      - | 2664 | ` *  Thr processed string.` |
|      - | 2665 | ` * NOTE:` |
|      - | 2666 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2667 | ` */` |
|     36 | 2668 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2669 | `{` |
|     40 | 2670 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2671 | `	const char *zString;` |
|      - | 2672 | `	int nLen;` |
|     40 | 2673 | `	if( nArg < 1 ){` |
|      - | 2674 | `		/* Missing arguments,return null */` |
|    ! 0 | 2675 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2676 | `		return PH7_OK;` |
|      - | 2677 | `	}` |
|      - | 2678 | `	/* Extract the target string */` |
|     40 | 2679 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 2680 | `	if( nLen < 1 ){` |
|      - | 2681 | `		/* Empty string,return */` |
|      7 | 2682 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2683 | `		return PH7_OK;` |
|      - | 2684 | `	}` |
|      - | 2685 | `	/* Start the trim process */` |
|     34 | 2686 | `	if( nArg < 2 ){` |
|      - | 2687 | `		SyString sStr;` |
|      - | 2688 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2689 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2690 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2691 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2692 | `	}else{` |
|      - | 2693 | `		/* Char list */` |
|      - | 2694 | `		const char *zList;` |
|      - | 2695 | `		int nListlen;` |
|     18 | 2696 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     18 | 2697 | `		if( nListlen < 1 ){` |
|      - | 2698 | `			/* Return the string unchanged */` |
|    ! 0 | 2699 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2700 | `		}else{` |
|      - | 2701 | `			char aMask[256];` |
|     18 | 2702 | `			const char *zEnd = &zString[nLen];` |
|     18 | 2703 | `			const char *zCur = zString;` |
|     18 | 2704 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2705 | `			/* Right trim */` |
|     38 | 2706 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     22 | 2707 | `				zEnd--;` |
|      2 | 2708 | `			}` |
|     18 | 2709 | `			if( zEnd <= zCur ){` |
|      - | 2710 | `				/* Return the empty string */` |
|    ! 0 | 2711 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2712 | `			}else{` |
|     18 | 2713 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2714 | `			}` |
|      - | 2715 | `		}` |
|      - | 2716 | `	}` |
|     34 | 2717 | `	return PH7_OK;` |
|     22 | 2718 | `}` |
|      - | 2719 | `/*` |
|      - | 2720 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2721 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2722 | ` * Parameters` |
|      - | 2723 | ` *  $str` |
|      - | 2724 | ` *   The string that will be trimmed.` |
|      - | 2725 | ` * $charlist` |
|      - | 2726 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2727 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2728 | ` *   With .. you can specify a range of characters.` |
|      - | 2729 | ` * Returns.` |
|      - | 2730 | ` *  Thr processed string.` |
|      - | 2731 | ` * NOTE:` |
|      - | 2732 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2733 | ` */` |
|     48 | 2734 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2735 | `{` |
|     53 | 2736 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2737 | `	const char *zString;` |
|      - | 2738 | `	int nLen;` |
|     53 | 2739 | `	if( nArg < 1 ){` |
|      - | 2740 | `		/* Missing arguments,return null */` |
|    ! 0 | 2741 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2742 | `		return PH7_OK;` |
|      - | 2743 | `	}` |
|      - | 2744 | `	/* Extract the target string */` |
|     53 | 2745 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     53 | 2746 | `	if( nLen < 1 ){` |
|      - | 2747 | `		/* Empty string,return */` |
|     29 | 2748 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 2749 | `		return PH7_OK;` |
|      - | 2750 | `	}` |
|      - | 2751 | `	/* Start the trim process */` |
|     28 | 2752 | `	if( nArg < 2 ){` |
|      - | 2753 | `		SyString sStr;` |
|      - | 2754 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2755 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2756 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2757 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2758 | `	}else{` |
|      - | 2759 | `		/* Char list */` |
|      - | 2760 | `		const char *zList;` |
|      - | 2761 | `		int nListlen;` |
|     24 | 2762 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     24 | 2763 | `		if( nListlen < 1 ){` |
|      - | 2764 | `			/* Return the string unchanged */` |
|      3 | 2765 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2766 | `		}else{` |
|      - | 2767 | `			char aMask[256];` |
|     22 | 2768 | `			const char *zEnd = &zString[nLen];` |
|     22 | 2769 | `			const char *zCur = zString;` |
|     22 | 2770 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2771 | `			/* Left trim */` |
|     56 | 2772 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     38 | 2773 | `				zCur++;` |
|      4 | 2774 | `			}` |
|     22 | 2775 | `			if( zCur >= zEnd ){` |
|      - | 2776 | `				/* Return the empty string */` |
|    ! 0 | 2777 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2778 | `			}else{` |
|     22 | 2779 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2780 | `			}` |
|      - | 2781 | `		}` |
|      - | 2782 | `	}` |
|     28 | 2783 | `	return PH7_OK;` |
|     29 | 2784 | `}` |
|      - | 2785 | `/*` |
|      - | 2786 | ` * string strtolower(string $str)` |
|      - | 2787 | ` *  Make a string lowercase.` |
|      - | 2788 | ` * Parameters` |
|      - | 2789 | ` *  $str` |
|      - | 2790 | ` *   The input string.` |
|      - | 2791 | ` * Returns.` |
|      - | 2792 | ` *  The lowercased string.` |
|      - | 2793 | ` */` |
|  33686 | 2794 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2795 | `{` |
|  33691 | 2796 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2797 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2798 | `	int nLen;` |
|  33691 | 2799 | `	if( nArg < 1 ){` |
|      - | 2800 | `		/* Missing arguments,return null */` |
|    ! 0 | 2801 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2802 | `		return PH7_OK;` |
|      - | 2803 | `	}` |
|      - | 2804 | `	/* Extract the target string */` |
|  33691 | 2805 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33691 | 2806 | `	if( nLen < 1 ){` |
|      - | 2807 | `		/* Empty string,return */` |
|      5 | 2808 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2809 | `		return PH7_OK;` |
|      - | 2810 | `	}` |
|      - | 2811 | `	/* Perform the requested operation */` |
|  33687 | 2812 | `	zEnd = &zString[nLen];` |
| 106316 | 2813 | `	for(;;){` |
| 212637 | 2814 | `		if( zString >= zEnd ){` |
|      - | 2815 | `			/* No more input,break immediately */` |
|  33687 | 2816 | `			break;` |
|      - | 2817 | `		}` |
| 178955 | 2818 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2819 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2820 | `			zCur = zString;` |
|    ! 0 | 2821 | `			zString++;` |
|    ! 0 | 2822 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2823 | `				zString++;` |
|    ! 0 | 2824 | `			}` |
|      - | 2825 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2826 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2827 | `		}else{` |
| 178955 | 2828 | `			int c = zString[0];` |
| 178955 | 2829 | `			if( SyisUpper(c) ){` |
| 176399 | 2830 | `				c = SyToLower(zString[0]);` |
|  88197 | 2831 | `			}` |
|      - | 2832 | `			/* Append character */` |
| 178955 | 2833 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2834 | `			/* Advance the cursor */` |
| 178955 | 2835 | `			zString++;` |
|      - | 2836 | `		}` |
|      5 | 2837 | `	}` |
|  33687 | 2838 | `	return PH7_OK;` |
|  16848 | 2839 | `}` |
|      - | 2840 | `/*` |
|      - | 2841 | ` * string strtolower(string $str)` |
|      - | 2842 | ` *  Make a string uppercase.` |
|      - | 2843 | ` * Parameters` |
|      - | 2844 | ` *  $str` |
|      - | 2845 | ` *   The input string.` |
|      - | 2846 | ` * Returns.` |
|      - | 2847 | ` *  The uppercased string.` |
|      - | 2848 | ` */` |
|     68 | 2849 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2850 | `{` |
|     72 | 2851 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2852 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2853 | `	int nLen;` |
|     72 | 2854 | `	if( nArg < 1 ){` |
|      - | 2855 | `		/* Missing arguments,return null */` |
|    ! 0 | 2856 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2857 | `		return PH7_OK;` |
|      - | 2858 | `	}` |
|      - | 2859 | `	/* Extract the target string */` |
|     72 | 2860 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     72 | 2861 | `	if( nLen < 1 ){` |
|      - | 2862 | `		/* Empty string,return */` |
|      5 | 2863 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2864 | `		return PH7_OK;` |
|      - | 2865 | `	}` |
|      - | 2866 | `	/* Perform the requested operation */` |
|     68 | 2867 | `	zEnd = &zString[nLen];` |
|    135 | 2868 | `	for(;;){` |
|    274 | 2869 | `		if( zString >= zEnd ){` |
|      - | 2870 | `			/* No more input,break immediately */` |
|     68 | 2871 | `			break;` |
|      - | 2872 | `		}` |
|    210 | 2873 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2874 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2875 | `			zCur = zString;` |
|    ! 0 | 2876 | `			zString++;` |
|    ! 0 | 2877 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2878 | `				zString++;` |
|    ! 0 | 2879 | `			}` |
|      - | 2880 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2881 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2882 | `		}else{` |
|    210 | 2883 | `			int c = zString[0];` |
|    210 | 2884 | `			if( SyisLower(c) ){` |
|    200 | 2885 | `				c = SyToUpper(zString[0]);` |
|     98 | 2886 | `			}` |
|      - | 2887 | `			/* Append character */` |
|    210 | 2888 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2889 | `			/* Advance the cursor */` |
|    210 | 2890 | `			zString++;` |
|      - | 2891 | `		}` |
|      4 | 2892 | `	}` |
|     68 | 2893 | `	return PH7_OK;` |
|     38 | 2894 | `}` |
|      - | 2895 | `/*` |
|      - | 2896 | ` * string ucfirst(string $str)` |
|      - | 2897 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2898 | ` *  character is alphabetic.` |
|      - | 2899 | ` * Parameters` |
|      - | 2900 | ` *  $str` |
|      - | 2901 | ` *   The input string.` |
|      - | 2902 | ` * Returns.` |
|      - | 2903 | ` *  The processed string.` |
|      - | 2904 | ` */` |
|      4 | 2905 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2906 | `{` |
|      - | 2907 | `	const char *zString,*zEnd;` |
|      - | 2908 | `	int nLen,c;` |
|      5 | 2909 | `	if( nArg < 1 ){` |
|      - | 2910 | `		/* Missing arguments,return null */` |
|    ! 0 | 2911 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2912 | `		return PH7_OK;` |
|      - | 2913 | `	}` |
|      - | 2914 | `	/* Extract the target string */` |
|      5 | 2915 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2916 | `	if( nLen < 1 ){` |
|      - | 2917 | `		/* Empty string,return */` |
|      3 | 2918 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2919 | `		return PH7_OK;` |
|      - | 2920 | `	}` |
|      - | 2921 | `	/* Perform the requested operation */` |
|      3 | 2922 | `	zEnd = &zString[nLen];` |
|      3 | 2923 | `	c = zString[0];` |
|      3 | 2924 | `	if( SyisLower(c) ){` |
|      3 | 2925 | `		c = SyToUpper(c);` |
|      1 | 2926 | `	}` |
|      - | 2927 | `	/* Append the first character */` |
|      3 | 2928 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2929 | `	zString++;` |
|      3 | 2930 | `	if( zString < zEnd ){` |
|      - | 2931 | `		/* Append the rest of the input verbatim */` |
|      3 | 2932 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2933 | `	}` |
|      3 | 2934 | `	return PH7_OK;` |
|      3 | 2935 | `}` |
|      - | 2936 | `/*` |
|      - | 2937 | ` * string lcfirst(string $str)` |
|      - | 2938 | ` *  Make a string's first character lowercase.` |
|      - | 2939 | ` * Parameters` |
|      - | 2940 | ` *  $str` |
|      - | 2941 | ` *   The input string.` |
|      - | 2942 | ` * Returns.` |
|      - | 2943 | ` *  The processed string.` |
|      - | 2944 | ` */` |
|      4 | 2945 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2946 | `{` |
|      - | 2947 | `	const char *zString,*zEnd;` |
|      - | 2948 | `	int nLen,c;` |
|      5 | 2949 | `	if( nArg < 1 ){` |
|      - | 2950 | `		/* Missing arguments,return null */` |
|    ! 0 | 2951 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2952 | `		return PH7_OK;` |
|      - | 2953 | `	}` |
|      - | 2954 | `	/* Extract the target string */` |
|      5 | 2955 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2956 | `	if( nLen < 1 ){` |
|      - | 2957 | `		/* Empty string,return */` |
|      3 | 2958 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2959 | `		return PH7_OK;` |
|      - | 2960 | `	}` |
|      - | 2961 | `	/* Perform the requested operation */` |
|      3 | 2962 | `	zEnd = &zString[nLen];` |
|      3 | 2963 | `	c = zString[0];` |
|      3 | 2964 | `	if( SyisUpper(c) ){` |
|      3 | 2965 | `		c = SyToLower(c);` |
|      1 | 2966 | `	}` |
|      - | 2967 | `	/* Append the first character */` |
|      3 | 2968 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2969 | `	zString++;` |
|      3 | 2970 | `	if( zString < zEnd ){` |
|      - | 2971 | `		/* Append the rest of the input verbatim */` |
|      3 | 2972 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2973 | `	}` |
|      3 | 2974 | `	return PH7_OK;` |
|      3 | 2975 | `}` |
|      - | 2976 | `/*` |
|      - | 2977 | ` * int ord(string $string)` |
|      - | 2978 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2979 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2980 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2981 | ` * Parameters` |
|      - | 2982 | ` *  $string` |
|      - | 2983 | ` *   The input string.` |
|      - | 2984 | ` * Returns` |
|      - | 2985 | ` *  The ASCII value as an integer.` |
|      - | 2986 | ` */` |
|    184 | 2987 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2988 | `{` |
|      - | 2989 | `	const char *zString;` |
|      - | 2990 | `	int nLen,c;` |
|      - | 2991 | `	/* PHP requires exactly one argument. */` |
|    188 | 2992 | `	if( nArg != 1 ){` |
|      8 | 2993 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2994 | `			"ArgumentCountError",` |
|      - | 2995 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2996 | `			nArg` |
|      - | 2997 | `			);` |
|      - | 2998 | `	}` |
|      - | 2999 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 3000 | `	 * the empty-string deprecation, so we check null first. */` |
|    182 | 3001 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 3002 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3003 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 3004 | `			"of type string is deprecated"` |
|      - | 3005 | `			);` |
|      1 | 3006 | `	}` |
|      - | 3007 | `	/* Extract the target string */` |
|    182 | 3008 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    182 | 3009 | `	if( nLen < 1 ){` |
|      - | 3010 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 3011 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3012 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 3013 | `			);` |
|      5 | 3014 | `		ph7_result_int(pCtx,0);` |
|      5 | 3015 | `		return PH7_OK;` |
|      - | 3016 | `	}` |
|      - | 3017 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|    178 | 3018 | `	if( nLen > 1 ){` |
|      7 | 3019 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3020 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 3021 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 3022 | `			);` |
|      3 | 3023 | `	}` |
|      - | 3024 | `	/* Extract the ASCII value of the first character */` |
|    178 | 3025 | `	c = (unsigned char)zString[0];` |
|      - | 3026 | `	/* Return that value */` |
|    178 | 3027 | `	ph7_result_int(pCtx,c);` |
|    178 | 3028 | `	return PH7_OK;` |
|     96 | 3029 | `}` |
|      - | 3030 | `/*` |
|      - | 3031 | ` * string chr(int $codepoint)` |
|      - | 3032 | ` *  Returns a one-character string containing the character specified` |
|      - | 3033 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3034 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3035 | ` * Parameters` |
|      - | 3036 | ` *  $codepoint` |
|      - | 3037 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3038 | ` *   will be constrained to a single byte.` |
|      - | 3039 | ` * Returns` |
|      - | 3040 | ` *  A single-character string.` |
|      - | 3041 | ` */` |
|   7116 | 3042 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3043 | `{` |
|      - | 3044 | `	int c;` |
|      - | 3045 | `	unsigned char ch;` |
|      - | 3046 | `	/* PHP requires exactly one argument. */` |
|   7119 | 3047 | `	if( nArg != 1 ){` |
|      8 | 3048 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3049 | `			"ArgumentCountError",` |
|      - | 3050 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 3051 | `			nArg` |
|      - | 3052 | `			);` |
|      - | 3053 | `	}` |
|      - | 3054 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3055 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3056 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3057 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7114 | 3058 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3059 | `		char zBuf[120];` |
|      4 | 3060 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3061 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3062 | `			ph7_value_to_double(apArg[0])` |
|      - | 3063 | `			);` |
|      3 | 3064 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3065 | `	}` |
|      - | 3066 | `	/* Extract the codepoint. */` |
|   7114 | 3067 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3068 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3069 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3070 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3071 | `	 * name to avoid the API double-prefixing it. */` |
|   7114 | 3072 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3073 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3074 | `			E_DEPRECATED,` |
|      - | 3075 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3076 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3077 | `			"The value used will be constrained using % 256"` |
|      - | 3078 | `			);` |
|      2 | 3079 | `	}` |
|      - | 3080 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3081 | `	 * when taking the address of a wider int. */` |
|   7114 | 3082 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3083 | `	/* Return the specified character */` |
|   7114 | 3084 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7114 | 3085 | `	return PH7_OK;` |
|   3561 | 3086 | `}` |
|      - | 3087 | `/*` |
|      - | 3088 | ` * Binary to hex consumer callback.` |
|      - | 3089 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3090 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3091 | ` */` |
|   3118 | 3092 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 3093 | `{` |
|      - | 3094 | `	/* Append hex chunk verbatim */` |
|   3120 | 3095 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 3096 | `	return SXRET_OK;` |
|      2 | 3097 | `}` |
|      - | 3098 |  |
|      - | 3099 | `/*` |
|      - | 3100 | ` * string bin2hex(string $str)` |
|      - | 3101 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3102 | ` * Parameters` |
|      - | 3103 | ` *  $str` |
|      - | 3104 | ` *   The input string.` |
|      - | 3105 | ` * Returns.` |
|      - | 3106 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3107 | ` */` |
|    138 | 3108 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3109 | `{` |
|      - | 3110 | `	const char *zString;` |
|      - | 3111 | `	int nLen;` |
|      - | 3112 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    143 | 3113 | `	if( nArg != 1 ){` |
|      8 | 3114 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3115 | `			"ArgumentCountError",` |
|      - | 3116 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 3117 | `			nArg` |
|      - | 3118 | `			);` |
|      - | 3119 | `	}` |
|      - | 3120 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3121 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3122 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3123 | `	 */` |
|    204 | 3124 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    130 | 3125 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 3126 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 3127 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 3128 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3129 | `		)` |
|      - | 3130 | `	){` |
|      9 | 3131 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 3132 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 3133 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 3134 | `			if( pInst && pInst->pClass ){` |
|      3 | 3135 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 3136 | `			}` |
|      1 | 3137 | `		}` |
|     12 | 3138 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3139 | `			"TypeError",` |
|      - | 3140 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 3141 | `			zType` |
|      - | 3142 | `			);` |
|      - | 3143 | `	}` |
|      - | 3144 | `	/* Extract the target string */` |
|    130 | 3145 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 3146 | `	if( nLen < 1 ){` |
|      - | 3147 | `		/* Empty string,return */` |
|     13 | 3148 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3149 | `		return PH7_OK;` |
|      - | 3150 | `	}` |
|      - | 3151 | `	/* Perform the requested operation */` |
|    118 | 3152 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 3153 | `	return PH7_OK;` |
|     74 | 3154 | `}` |
|      - | 3155 |  |
|      - | 3156 | `/* Search callback signature */` |
|      - | 3157 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3158 | `/*` |
|      - | 3159 | ` * Case-insensitive pattern match.` |
|      - | 3160 | ` * Brute force is the default search method used here.` |
|      - | 3161 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3162 | ` * well for short/medium texts on modern hardware.` |
|      - | 3163 | ` */` |
|    298 | 3164 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 3165 | `{` |
|    300 | 3166 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 3167 | `	const char *zIn = (const char *)pText;` |
|    300 | 3168 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 3169 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3170 | `	const char *zPtr,*zPtr2;` |
|      - | 3171 | `	int c,d;` |
|    300 | 3172 | `	if( iPatLen > nLen ){` |
|      - | 3173 | `		/* Don't bother processing */` |
|     67 | 3174 | `		return SXERR_NOTFOUND;` |
|      - | 3175 | `	}` |
|    860 | 3176 | `	for(;;){` |
|   1722 | 3177 | `		if( zIn >= zEnd ){` |
|    194 | 3178 | `			break;` |
|      - | 3179 | `		}` |
|   1530 | 3180 | `		c = SyToLower(zIn[0]);` |
|   1530 | 3181 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 3182 | `		if( c == d ){` |
|    182 | 3183 | `			zPtr   = &zIn[1];` |
|    182 | 3184 | `			zPtr2  = &zpIn[1];` |
|    141 | 3185 | `			for(;;){` |
|    284 | 3186 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3187 | `					/* Pattern found */` |
|     41 | 3188 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3189 | `					return SXRET_OK;` |
|      - | 3190 | `				}` |
|    244 | 3191 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3192 | `					break;` |
|      - | 3193 | `				}` |
|    244 | 3194 | `				c = SyToLower(zPtr[0]);` |
|    244 | 3195 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 3196 | `				if( c != d ){` |
|    142 | 3197 | `					break;` |
|      - | 3198 | `				}` |
|    103 | 3199 | `				zPtr++; zPtr2++;` |
|      1 | 3200 | `			}` |
|     70 | 3201 | `		}` |
|   1490 | 3202 | `		zIn++;` |
|      2 | 3203 | `	}` |
|      - | 3204 | `	/* Pattern not found */` |
|    194 | 3205 | `	return SXERR_NOTFOUND;` |
|    151 | 3206 | `}` |
|      - | 3207 | `/*` |
|      - | 3208 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3209 | ` *  Find the first occurrence of a string.` |
|      - | 3210 | ` * Parameters` |
|      - | 3211 | ` *  $haystack` |
|      - | 3212 | ` *   The input string.` |
|      - | 3213 | ` * $needle` |
|      - | 3214 | ` *   Search pattern (must be a string).` |
|      - | 3215 | ` * $before_needle` |
|      - | 3216 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3217 | ` *   of the needle (excluding the needle).` |
|      - | 3218 | ` * Return` |
|      - | 3219 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3220 | ` */` |
|      6 | 3221 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3222 | `{` |
|      7 | 3223 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3224 | `	const char *zBlob,*zPattern;` |
|      - | 3225 | `	int nLen,nPatLen;` |
|      - | 3226 | `	sxu32 nOfft;` |
|      - | 3227 | `	sxi32 rc;` |
|      7 | 3228 | `	if( nArg < 2 ){` |
|      - | 3229 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3230 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3231 | `		return PH7_OK;` |
|      - | 3232 | `	}` |
|      - | 3233 | `	/* Extract the needle and the haystack */` |
|      7 | 3234 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3235 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3236 | `	nOfft = 0; /* cc warning */` |
|      9 | 3237 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3238 | `		int before = 0;` |
|      - | 3239 | `		/* Perform the lookup */` |
|      5 | 3240 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3241 | `		if( rc != SXRET_OK ){` |
|      - | 3242 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3243 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3244 | `			return PH7_OK;` |
|      - | 3245 | `		}` |
|      - | 3246 | `		/* Return the portion of the string */` |
|      5 | 3247 | `		if( nArg > 2 ){` |
|      3 | 3248 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3249 | `		}` |
|      5 | 3250 | `		if( before ){` |
|      3 | 3251 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3252 | `		}else{` |
|      3 | 3253 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3254 | `		}` |
|      3 | 3255 | `	}else{` |
|      3 | 3256 | `		ph7_result_bool(pCtx,0);` |
|      - | 3257 | `	}` |
|      7 | 3258 | `	return PH7_OK;` |
|      4 | 3259 | `}` |
|      - | 3260 | `/*` |
|      - | 3261 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3262 | ` *  Case-insensitive strstr().` |
|      - | 3263 | ` * Parameters` |
|      - | 3264 | ` *  $haystack` |
|      - | 3265 | ` *   The input string.` |
|      - | 3266 | ` * $needle` |
|      - | 3267 | ` *   Search pattern (must be a string).` |
|      - | 3268 | ` * $before_needle` |
|      - | 3269 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3270 | ` *   of the needle (excluding the needle).` |
|      - | 3271 | ` * Return` |
|      - | 3272 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3273 | ` */` |
|      4 | 3274 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3275 | `{` |
|      5 | 3276 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3277 | `	const char *zBlob,*zPattern;` |
|      - | 3278 | `	int nLen,nPatLen;` |
|      - | 3279 | `	sxu32 nOfft;` |
|      - | 3280 | `	sxi32 rc;` |
|      5 | 3281 | `	if( nArg < 2 ){` |
|      - | 3282 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3283 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3284 | `		return PH7_OK;` |
|      - | 3285 | `	}` |
|      - | 3286 | `	/* Extract the needle and the haystack */` |
|      5 | 3287 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3288 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3289 | `	nOfft = 0; /* cc warning */` |
|      7 | 3290 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3291 | `		int before = 0;` |
|      - | 3292 | `		/* Perform the lookup */` |
|      5 | 3293 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3294 | `		if( rc != SXRET_OK ){` |
|      - | 3295 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3296 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3297 | `			return PH7_OK;` |
|      - | 3298 | `		}` |
|      - | 3299 | `		/* Return the portion of the string */` |
|      5 | 3300 | `		if( nArg > 2 ){` |
|      3 | 3301 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3302 | `		}` |
|      5 | 3303 | `		if( before ){` |
|      3 | 3304 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3305 | `		}else{` |
|      3 | 3306 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3307 | `		}` |
|      3 | 3308 | `	}else{` |
|    ! 0 | 3309 | `		ph7_result_bool(pCtx,0);` |
|      - | 3310 | `	}` |
|      5 | 3311 | `	return PH7_OK;` |
|      3 | 3312 | `}` |
|      - | 3313 | `/*` |
|      - | 3314 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3315 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3316 | ` * Parameters` |
|      - | 3317 | ` *  $haystack` |
|      - | 3318 | ` *   The input string.` |
|      - | 3319 | ` * $needle` |
|      - | 3320 | ` *   Search pattern (must be a string).` |
|      - | 3321 | ` * $offset` |
|      - | 3322 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3323 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3324 | ` *   of haystack.` |
|      - | 3325 | ` * Return` |
|      - | 3326 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3327 | ` */` |
|   1468 | 3328 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3329 | `{` |
|   1473 | 3330 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1473 | 3331 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1473 | 3332 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3333 | `	const char *zBlob,*zPattern;` |
|      - | 3334 | `	int nLen,nPatLen,nStart;` |
|      - | 3335 | `	sxu32 nOfft;` |
|      - | 3336 | `	sxi32 rc;` |
|   1473 | 3337 | `	if( nArg < 2 ){` |
|      - | 3338 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3339 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3340 | `		return PH7_OK;` |
|      - | 3341 | `	}` |
|      - | 3342 | `	/* Extract the needle and the haystack */` |
|   1473 | 3343 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1473 | 3344 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1473 | 3345 | `	nOfft = 0; /* cc warning */` |
|   1473 | 3346 | `	nStart = 0;` |
|      - | 3347 | `	/* Peek the starting offset if available */` |
|   1473 | 3348 | `	if( nArg > 2 ){` |
|     15 | 3349 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3350 | `		if( nStart < 0 ){` |
|    ! 0 | 3351 | `			nStart = -nStart;` |
|    ! 0 | 3352 | `		}` |
|     15 | 3353 | `		if( nStart >= nLen ){` |
|      - | 3354 | `			/* Invalid offset */` |
|    ! 0 | 3355 | `			nStart = 0;` |
|    ! 0 | 3356 | `		}else{` |
|     15 | 3357 | `			zBlob += nStart;` |
|     15 | 3358 | `			nLen -= nStart;` |
|      - | 3359 | `		}` |
|      7 | 3360 | `	}` |
|   1473 | 3361 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3362 | `		/* Perform the lookup */` |
|   1471 | 3363 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1471 | 3364 | `		if( rc != SXRET_OK ){` |
|      - | 3365 | `			/* Pattern not found,return FALSE */` |
|    779 | 3366 | `			ph7_result_bool(pCtx,0);` |
|    779 | 3367 | `			return PH7_OK;` |
|      - | 3368 | `		}` |
|      - | 3369 | `		/* Return the pattern position */` |
|    697 | 3370 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    351 | 3371 | `	}else{` |
|      3 | 3372 | `		ph7_result_bool(pCtx,0);` |
|      - | 3373 | `	}` |
|    699 | 3374 | `	return PH7_OK;` |
|    739 | 3375 | `}` |
|      - | 3376 | `/*` |
|      - | 3377 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3378 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3379 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3380 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3381 | ` *` |
|      - | 3382 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3383 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3384 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3385 | ` *` |
|      - | 3386 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3387 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3388 | ` */` |
|    724 | 3389 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3390 | `	ph7_context *pCtx,` |
|      - | 3391 | `	ph7_value *pArg,` |
|      - | 3392 | `	const char *zFunc,` |
|      - | 3393 | `	int iArgNum,` |
|      - | 3394 | `	const char *zParamName,` |
|      - | 3395 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3396 | `	const char *zNullMsg,` |
|      - | 3397 | `	ph7_value *pTmp,` |
|      - | 3398 | `	const char **pzOut,` |
|      - | 3399 | `	int *pnOut` |
|      4 | 3400 | `){` |
|    728 | 3401 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3402 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3403 | `		*pzOut = "";` |
|     13 | 3404 | `		*pnOut = 0;` |
|     13 | 3405 | `		return PH7_OK;` |
|      - | 3406 | `	}` |
|   1094 | 3407 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    686 | 3408 | `	    ( ph7_value_is_object(pArg) &&` |
|    105 | 3409 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     70 | 3410 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     35 | 3411 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3412 | `	    )` |
|      - | 3413 | `	){` |
|     52 | 3414 | `		const char *zType = ph7_type_name(pArg);` |
|     52 | 3415 | `		if( ph7_value_is_object(pArg) ){` |
|     23 | 3416 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     23 | 3417 | `			if( pInst && pInst->pClass ){` |
|     23 | 3418 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     11 | 3419 | `			}` |
|     11 | 3420 | `		}` |
|     76 | 3421 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3422 | `			"TypeError",` |
|      - | 3423 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|     24 | 3424 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3425 | `			);` |
|      - | 3426 | `	}` |
|    665 | 3427 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3428 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3429 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3430 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3431 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3432 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3433 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3434 | `		return PH7_OK;` |
|      - | 3435 | `	}` |
|    617 | 3436 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    617 | 3437 | `	return PH7_OK;` |
|    366 | 3438 | `}` |
|      - | 3439 | `/*` |
|      - | 3440 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3441 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3442 | ` * Return` |
|      - | 3443 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3444 | ` */` |
|     98 | 3445 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3446 | `{` |
|      - | 3447 | `	const char *zHaystack,*zNeedle;` |
|      - | 3448 | `	int nHayLen,nNeedleLen;` |
|      - | 3449 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3450 | `	sxi32 rc;` |
|    102 | 3451 | `	if( nArg != 2 ){` |
|     18 | 3452 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3453 | `			"ArgumentCountError",` |
|      - | 3454 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 3455 | `			nArg` |
|      - | 3456 | `			);` |
|      - | 3457 | `	}` |
|     90 | 3458 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     90 | 3459 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     90 | 3460 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3461 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3462 | `		"of type string is deprecated",` |
|      - | 3463 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     90 | 3464 | `	if( rc != PH7_OK ) goto out;` |
|     83 | 3465 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3466 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3467 | `		"of type string is deprecated",` |
|      - | 3468 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     83 | 3469 | `	if( rc != PH7_OK ) goto out;` |
|     79 | 3470 | `	if( nNeedleLen < 1 ){` |
|     13 | 3471 | `		ph7_result_bool(pCtx,1);` |
|     73 | 3472 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3473 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3474 | `	}else{` |
|     88 | 3475 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     29 | 3476 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     59 | 3477 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3478 | `	}` |
|     79 | 3479 | `	rc = PH7_OK;` |
|     44 | 3480 | `out:` |
|     90 | 3481 | `	PH7_MemObjRelease(&sHayTmp);` |
|     90 | 3482 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     90 | 3483 | `	return rc;` |
|     53 | 3484 | `}` |
|      - | 3485 | `/*` |
|      - | 3486 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3487 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3488 | ` * Return` |
|      - | 3489 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3490 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3491 | ` */` |
|     78 | 3492 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3493 | `{` |
|      - | 3494 | `	const char *zHaystack,*zNeedle;` |
|      - | 3495 | `	int nHayLen,nNeedleLen;` |
|      - | 3496 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3497 | `	sxi32 rc;` |
|     82 | 3498 | `	if( nArg != 2 ){` |
|     18 | 3499 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3500 | `			"ArgumentCountError",` |
|      - | 3501 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 3502 | `			nArg` |
|      - | 3503 | `			);` |
|      - | 3504 | `	}` |
|     70 | 3505 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 3506 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 3507 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3508 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3509 | `		"of type string is deprecated",` |
|      - | 3510 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 3511 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 3512 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3513 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3514 | `		"of type string is deprecated",` |
|      - | 3515 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 3516 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3517 | `	if( nNeedleLen < 1 ){` |
|     13 | 3518 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3519 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3520 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3521 | `	}else{` |
|     58 | 3522 | `		ph7_result_bool(pCtx,` |
|     38 | 3523 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3524 | `	}` |
|     59 | 3525 | `	rc = PH7_OK;` |
|     34 | 3526 | `out:` |
|     70 | 3527 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 3528 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 3529 | `	return rc;` |
|     43 | 3530 | `}` |
|      - | 3531 | `/*` |
|      - | 3532 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3533 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3534 | ` * Return` |
|      - | 3535 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3536 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3537 | ` */` |
|     78 | 3538 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3539 | `{` |
|      - | 3540 | `	const char *zHaystack,*zNeedle;` |
|      - | 3541 | `	int nHayLen,nNeedleLen;` |
|      - | 3542 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3543 | `	sxi32 rc;` |
|     82 | 3544 | `	if( nArg != 2 ){` |
|     18 | 3545 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3546 | `			"ArgumentCountError",` |
|      - | 3547 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 3548 | `			nArg` |
|      - | 3549 | `			);` |
|      - | 3550 | `	}` |
|     70 | 3551 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 3552 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 3553 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3554 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3555 | `		"of type string is deprecated",` |
|      - | 3556 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 3557 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 3558 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3559 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3560 | `		"of type string is deprecated",` |
|      - | 3561 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 3562 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3563 | `	if( nNeedleLen < 1 ){` |
|     13 | 3564 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3565 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3566 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3567 | `	}else{` |
|     58 | 3568 | `		ph7_result_bool(pCtx,` |
|     38 | 3569 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3570 | `	}` |
|     59 | 3571 | `	rc = PH7_OK;` |
|     34 | 3572 | `out:` |
|     70 | 3573 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 3574 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 3575 | `	return rc;` |
|     43 | 3576 | `}` |
|      - | 3577 | `/*` |
|      - | 3578 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3579 | ` *  Case-insensitive strpos.` |
|      - | 3580 | ` * Parameters` |
|      - | 3581 | ` *  $haystack` |
|      - | 3582 | ` *   The input string.` |
|      - | 3583 | ` * $needle` |
|      - | 3584 | ` *   Search pattern (must be a string).` |
|      - | 3585 | ` * $offset` |
|      - | 3586 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3587 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3588 | ` *   of haystack.` |
|      - | 3589 | ` * Return` |
|      - | 3590 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3591 | ` */` |
|    196 | 3592 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3593 | `{` |
|    198 | 3594 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3595 | `	const char *zBlob,*zPattern;` |
|      - | 3596 | `	int nLen,nPatLen,nStart;` |
|      - | 3597 | `	sxu32 nOfft;` |
|      - | 3598 | `	sxi32 rc;` |
|    198 | 3599 | `	if( nArg < 2 ){` |
|      - | 3600 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3601 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3602 | `		return PH7_OK;` |
|      - | 3603 | `	}` |
|      - | 3604 | `	/* Extract the needle and the haystack */` |
|    198 | 3605 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3606 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3607 | `	nOfft = 0; /* cc warning */` |
|    198 | 3608 | `	nStart = 0;` |
|      - | 3609 | `	/* Peek the starting offset if available */` |
|    198 | 3610 | `	if( nArg > 2 ){` |
|      5 | 3611 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3612 | `		if( nStart < 0 ){` |
|      3 | 3613 | `			nStart = -nStart;` |
|      1 | 3614 | `		}` |
|      5 | 3615 | `		if( nStart >= nLen ){` |
|      - | 3616 | `			/* Invalid offset */` |
|    ! 0 | 3617 | `			nStart = 0;` |
|    ! 0 | 3618 | `		}else{` |
|      5 | 3619 | `			zBlob += nStart;` |
|      5 | 3620 | `			nLen -= nStart;` |
|      - | 3621 | `		}` |
|      2 | 3622 | `	}` |
|    198 | 3623 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3624 | `		/* Perform the lookup */` |
|    198 | 3625 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3626 | `		if( rc != SXRET_OK ){` |
|      - | 3627 | `			/* Pattern not found,return FALSE */` |
|    184 | 3628 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3629 | `			return PH7_OK;` |
|      - | 3630 | `		}` |
|      - | 3631 | `		/* Return the pattern position */` |
|     15 | 3632 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3633 | `	}else{` |
|    ! 0 | 3634 | `		ph7_result_bool(pCtx,0);` |
|      - | 3635 | `	}` |
|     15 | 3636 | `	return PH7_OK;` |
|    100 | 3637 | `}` |
|      - | 3638 | `/*` |
|      - | 3639 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3640 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3641 | ` * Parameters` |
|      - | 3642 | ` *  $haystack` |
|      - | 3643 | ` *   The input string.` |
|      - | 3644 | ` * $needle` |
|      - | 3645 | ` *   Search pattern (must be a string).` |
|      - | 3646 | ` * $offset` |
|      - | 3647 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3648 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3649 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3650 | ` * Return` |
|      - | 3651 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3652 | ` */` |
|     40 | 3653 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3654 | `{` |
|      - | 3655 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     41 | 3656 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3657 | `	int nLen,nPatLen;` |
|      - | 3658 | `	sxu32 nOfft;` |
|      - | 3659 | `	sxi32 rc;` |
|     41 | 3660 | `	if( nArg < 2 ){` |
|      - | 3661 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3662 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3663 | `		return PH7_OK;` |
|      - | 3664 | `	}` |
|      - | 3665 | `	/* Extract the needle and the haystack */` |
|     41 | 3666 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3667 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3668 | `	/* Point to the end of the pattern */` |
|     41 | 3669 | `	zPtr = &zBlob[nLen - 1];` |
|     41 | 3670 | `	zEnd = &zBlob[nLen];` |
|      - | 3671 | `	/* Save the starting posistion */` |
|     41 | 3672 | `	zStart = zBlob;` |
|     41 | 3673 | `	nOfft = 0; /* cc warning */` |
|      - | 3674 | `	/* Peek the starting offset if available */` |
|     41 | 3675 | `	if( nArg > 2 ){` |
|      - | 3676 | `		int nStart;` |
|     21 | 3677 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3678 | `		if( nStart < 0 ){` |
|     11 | 3679 | `			nStart = -nStart;` |
|     11 | 3680 | `			if( nStart >= nLen ){` |
|      - | 3681 | `				/* Invalid offset */` |
|      3 | 3682 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3683 | `				return PH7_OK;` |
|    ! 0 | 3684 | `			}else{` |
|      9 | 3685 | `				nLen -= nStart;` |
|      9 | 3686 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3687 | `				zEnd = &zBlob[nLen];` |
|      - | 3688 | `			}` |
|      5 | 3689 | `		}else{` |
|     11 | 3690 | `			if( nStart >= nLen ){` |
|      - | 3691 | `				/* Invalid offset */` |
|      5 | 3692 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3693 | `				return PH7_OK;` |
|    ! 0 | 3694 | `			}else{` |
|      7 | 3695 | `				zBlob += nStart;` |
|      7 | 3696 | `				nLen -= nStart;` |
|      - | 3697 | `			}` |
|      - | 3698 | `		}` |
|      7 | 3699 | `	}` |
|     35 | 3700 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3701 | `		/* Perform the lookup */` |
|    121 | 3702 | `		for(;;){` |
|    243 | 3703 | `			if( zBlob >= zPtr ){` |
|     21 | 3704 | `				break;` |
|      - | 3705 | `			}` |
|    223 | 3706 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    223 | 3707 | `			if( rc == SXRET_OK ){` |
|      - | 3708 | `				/* Pattern found,return it's position */` |
|     13 | 3709 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3710 | `				return PH7_OK;` |
|      - | 3711 | `			}` |
|    211 | 3712 | `			zPtr--;` |
|      1 | 3713 | `		}` |
|      - | 3714 | `		/* Pattern not found,return FALSE */` |
|     21 | 3715 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3716 | `	}else{` |
|      3 | 3717 | `		ph7_result_bool(pCtx,0);` |
|      - | 3718 | `	}` |
|     23 | 3719 | `	return PH7_OK;` |
|     21 | 3720 | `}` |
|      - | 3721 | `/*` |
|      - | 3722 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3723 | ` *  Case-insensitive strrpos.` |
|      - | 3724 | ` * Parameters` |
|      - | 3725 | ` *  $haystack` |
|      - | 3726 | ` *   The input string.` |
|      - | 3727 | ` * $needle` |
|      - | 3728 | ` *   Search pattern (must be a string).` |
|      - | 3729 | ` * $offset` |
|      - | 3730 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3731 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3732 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3733 | ` * Return` |
|      - | 3734 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3735 | ` */` |
|     26 | 3736 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3737 | `{` |
|      - | 3738 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3739 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3740 | `	int nLen,nPatLen;` |
|      - | 3741 | `	sxu32 nOfft;` |
|      - | 3742 | `	sxi32 rc;` |
|     27 | 3743 | `	if( nArg < 2 ){` |
|      - | 3744 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3745 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3746 | `		return PH7_OK;` |
|      - | 3747 | `	}` |
|      - | 3748 | `	/* Extract the needle and the haystack */` |
|     27 | 3749 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3750 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3751 | `	/* Point to the end of the pattern */` |
|     27 | 3752 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3753 | `	zEnd = &zBlob[nLen];` |
|      - | 3754 | `	/* Save the starting posistion */` |
|     27 | 3755 | `	zStart = zBlob;` |
|     27 | 3756 | `	nOfft = 0; /* cc warning */` |
|      - | 3757 | `	/* Peek the starting offset if available */` |
|     27 | 3758 | `	if( nArg > 2 ){` |
|      - | 3759 | `		int nStart;` |
|     15 | 3760 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3761 | `		if( nStart < 0 ){` |
|      7 | 3762 | `			nStart = -nStart;` |
|      7 | 3763 | `			if( nStart >= nLen ){` |
|      - | 3764 | `				/* Invalid offset */` |
|      3 | 3765 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3766 | `				return PH7_OK;` |
|    ! 0 | 3767 | `			}else{` |
|      5 | 3768 | `				nLen -= nStart;` |
|      5 | 3769 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3770 | `				zEnd = &zBlob[nLen];` |
|      - | 3771 | `			}` |
|      3 | 3772 | `		}else{` |
|      9 | 3773 | `			if( nStart >= nLen ){` |
|      - | 3774 | `				/* Invalid offset */` |
|      5 | 3775 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3776 | `				return PH7_OK;` |
|    ! 0 | 3777 | `			}else{` |
|      5 | 3778 | `				zBlob += nStart;` |
|      5 | 3779 | `				nLen -= nStart;` |
|      - | 3780 | `			}` |
|      - | 3781 | `		}` |
|      4 | 3782 | `	}` |
|     21 | 3783 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3784 | `		/* Perform the lookup */` |
|     44 | 3785 | `		for(;;){` |
|     89 | 3786 | `			if( zBlob >= zPtr ){` |
|      9 | 3787 | `				break;` |
|      - | 3788 | `			}` |
|     81 | 3789 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3790 | `			if( rc == SXRET_OK ){` |
|      - | 3791 | `				/* Pattern found,return it's position */` |
|     11 | 3792 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3793 | `				return PH7_OK;` |
|      - | 3794 | `			}` |
|     71 | 3795 | `			zPtr--;` |
|      1 | 3796 | `		}` |
|      - | 3797 | `		/* Pattern not found,return FALSE */` |
|      9 | 3798 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3799 | `	}else{` |
|      3 | 3800 | `		ph7_result_bool(pCtx,0);` |
|      - | 3801 | `	}` |
|     11 | 3802 | `	return PH7_OK;` |
|     14 | 3803 | `}` |
|      - | 3804 | `/*` |
|      - | 3805 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3806 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3807 | ` * Parameters` |
|      - | 3808 | ` *  $haystack` |
|      - | 3809 | ` *   The input string.` |
|      - | 3810 | ` * $needle` |
|      - | 3811 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3812 | ` *  This behavior is different from that of strstr().` |
|      - | 3813 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3814 | ` *  as the ordinal value of a character.` |
|      - | 3815 | ` * Return` |
|      - | 3816 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3817 | ` */` |
|     22 | 3818 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3819 | `{` |
|      - | 3820 | `	const char *zBlob;` |
|      - | 3821 | `	int nLen,c;` |
|     23 | 3822 | `	if( nArg < 2 ){` |
|      - | 3823 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3824 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3825 | `		return PH7_OK;` |
|      - | 3826 | `	}` |
|      - | 3827 | `	/* Extract the haystack */` |
|     23 | 3828 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3829 | `	c = 0; /* cc warning */` |
|     23 | 3830 | `	if( nLen > 0 ){` |
|      - | 3831 | `		sxu32 nOfft;` |
|      - | 3832 | `		sxi32 rc;` |
|     21 | 3833 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3834 | `			const char *zPattern;` |
|     11 | 3835 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3836 | `														 * for NULL pointer.` |
|      - | 3837 | `														 */` |
|     11 | 3838 | `			c = zPattern[0];` |
|      6 | 3839 | `		}else{` |
|      - | 3840 | `			/* Int cast */` |
|     11 | 3841 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3842 | `		}` |
|      - | 3843 | `		/* Perform the lookup */` |
|     21 | 3844 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3845 | `		if( rc != SXRET_OK ){` |
|      - | 3846 | `			/* No such entry,return FALSE */` |
|      7 | 3847 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3848 | `			return PH7_OK;` |
|      - | 3849 | `		}` |
|      - | 3850 | `		/* Return the string portion */` |
|     15 | 3851 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3852 | `	}else{` |
|      3 | 3853 | `		ph7_result_bool(pCtx,0);` |
|      - | 3854 | `	}` |
|     17 | 3855 | `	return PH7_OK;` |
|     12 | 3856 | `}` |
|      - | 3857 | `/*` |
|      - | 3858 | ` * string strrev(string $string)` |
|      - | 3859 | ` *  Reverse a string.` |
|      - | 3860 | ` * Parameters` |
|      - | 3861 | ` *  $string` |
|      - | 3862 | ` *   String to be reversed.` |
|      - | 3863 | ` * Return` |
|      - | 3864 | ` *  The reversed string.` |
|      - | 3865 | ` */` |
|      2 | 3866 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3867 | `{` |
|      - | 3868 | `	const char *zIn,*zEnd;` |
|      - | 3869 | `	int nLen,c;` |
|      3 | 3870 | `	if( nArg < 1 ){` |
|      - | 3871 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3872 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3873 | `		return PH7_OK;` |
|      - | 3874 | `	}` |
|      - | 3875 | `	/* Extract the target string */` |
|      3 | 3876 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3877 | `	if( nLen < 1 ){` |
|      - | 3878 | `		/* Empty string Return null */` |
|    ! 0 | 3879 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3880 | `		return PH7_OK;` |
|      - | 3881 | `	}` |
|      - | 3882 | `	/* Perform the requested operation */` |
|      3 | 3883 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3884 | `	for(;;){` |
|      9 | 3885 | `		if( zEnd < zIn ){` |
|      - | 3886 | `			/* No more input to process */` |
|      3 | 3887 | `			break;` |
|      - | 3888 | `		}` |
|      - | 3889 | `		/* Append current character */` |
|      7 | 3890 | `		c = zEnd[0];` |
|      7 | 3891 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3892 | `		zEnd--;` |
|      1 | 3893 | `	}` |
|      3 | 3894 | `	return PH7_OK;` |
|      2 | 3895 | `}` |
|      - | 3896 | `/*` |
|      - | 3897 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3898 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3899 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3900 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3901 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3902 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3903 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3904 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3905 | ` * Parameters` |
|      - | 3906 | ` *  $string` |
|      - | 3907 | ` *   The input string.` |
|      - | 3908 | ` *  $separators` |
|      - | 3909 | ` *   The optional word-boundary characters.` |
|      - | 3910 | ` * Return` |
|      - | 3911 | ` *  The modified string.` |
|      - | 3912 | ` */` |
|     22 | 3913 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3914 | `{` |
|      - | 3915 | `	const char *zIn;` |
|      - | 3916 | `	int nLen,i,iStart;` |
|      - | 3917 | `	char aDelim[256];` |
|     23 | 3918 | `	if( nArg < 1 ){` |
|      - | 3919 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3920 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3921 | `		return PH7_OK;` |
|      - | 3922 | `	}` |
|      - | 3923 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3924 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3925 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3926 | `	if( nArg > 1 ){` |
|      - | 3927 | `		int nDelim;` |
|      9 | 3928 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3929 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3930 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3931 | `		}` |
|      5 | 3932 | `	}else{` |
|     15 | 3933 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3934 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3935 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3936 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3937 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3938 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3939 | `	}` |
|      - | 3940 | `	/* Extract the target string */` |
|     23 | 3941 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3942 | `	if( nLen < 1 ){` |
|      - | 3943 | `		/* Empty string – match PHP semantics */` |
|      3 | 3944 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3945 | `		return PH7_OK;` |
|      - | 3946 | `	}` |
|      - | 3947 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3948 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3949 | `	iStart = 0;` |
|    309 | 3950 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3951 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3952 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3953 | `			char up = (char)SyToUpper(c);` |
|     53 | 3954 | `			if( i > iStart ){` |
|     35 | 3955 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3956 | `			}` |
|     53 | 3957 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3958 | `			iStart = i + 1;` |
|     26 | 3959 | `		}` |
|    145 | 3960 | `	}` |
|     21 | 3961 | `	if( nLen > iStart ){` |
|     21 | 3962 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3963 | `	}` |
|     21 | 3964 | `	return PH7_OK;` |
|     12 | 3965 | `}` |
|      - | 3966 | `/*` |
|      - | 3967 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3968 | ` *  Returns input repeated multiplier times.` |
|      - | 3969 | ` * Parameters` |
|      - | 3970 | ` *  $string` |
|      - | 3971 | ` *   String to be repeated.` |
|      - | 3972 | ` * $multiplier` |
|      - | 3973 | ` *  Number of time the input string should be repeated.` |
|      - | 3974 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3975 | ` *  to 0, the function will return an empty string.` |
|      - | 3976 | ` * Return` |
|      - | 3977 | ` *  The repeated string.` |
|      - | 3978 | ` */` |
|  20434 | 3979 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3980 | `{` |
|      - | 3981 | `	const char *zIn;` |
|      - | 3982 | `	int nLen;` |
|      - | 3983 | `	ph7_int64 nMul;` |
|      - | 3984 | `	int rc;` |
|  20436 | 3985 | `	if( nArg < 2 ){` |
|      - | 3986 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3987 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3988 | `		return PH7_OK;` |
|      - | 3989 | `	}` |
|      - | 3990 | `	/* Extract the target string */` |
|  20436 | 3991 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3992 | `	/* Extract the multiplier as a 64-bit value (a 32-bit read would wrap a large` |
|      - | 3993 | `	 * positive $times into a negative one and trip a spurious ValueError). PHP` |
|      - | 3994 | `	 * validates $times regardless of the string contents: a negative count throws` |
|      - | 3995 | `	 * a catchable ValueError. */` |
|  20436 | 3996 | `	nMul = ph7_value_to_int64(apArg[1]);` |
|  20436 | 3997 | `	if( nMul < 0 ){` |
|      3 | 3998 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3999 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 4000 | `	}` |
|  20434 | 4001 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 4002 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 4003 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4004 | `		return PH7_OK;` |
|      - | 4005 | `	}` |
|      - | 4006 | `	/* Perform the requested operation */` |
| 221930 | 4007 | `	for(;;){` |
| 443862 | 4008 | `		if( !nMul ){` |
|  20434 | 4009 | `			break;` |
|      - | 4010 | `		}` |
|      - | 4011 | `		/* Append the copy */` |
| 423430 | 4012 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 423430 | 4013 | `		if( rc != PH7_OK ){` |
|      - | 4014 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 4015 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 4016 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4017 | `		}` |
| 423430 | 4018 | `		nMul--;` |
|      2 | 4019 | `	}` |
|  20434 | 4020 | `	return PH7_OK;` |
|  10219 | 4021 | `}` |
|      - | 4022 | `/*` |
|      - | 4023 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 4024 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 4025 | ` * Parameters` |
|      - | 4026 | ` *  $string` |
|      - | 4027 | ` *   The input string.` |
|      - | 4028 | ` * $is_xhtml` |
|      - | 4029 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 4030 | ` * Return` |
|      - | 4031 | ` *  The processed string.` |
|      - | 4032 | ` */` |
|      4 | 4033 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4034 | `{` |
|      - | 4035 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 4036 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 4037 | `	int nLen;` |
|      5 | 4038 | `	if( nArg < 1 ){` |
|      - | 4039 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4040 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4041 | `		return PH7_OK;` |
|      - | 4042 | `	}` |
|      - | 4043 | `	/* Extract the target string */` |
|      5 | 4044 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 4045 | `	if( nLen < 1 ){` |
|      - | 4046 | `		/* Empty string,return null */` |
|    ! 0 | 4047 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4048 | `		return PH7_OK;` |
|      - | 4049 | `	}` |
|      5 | 4050 | `	if( nArg > 1 ){` |
|      3 | 4051 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4052 | `	}` |
|      5 | 4053 | `	zEnd = &zIn[nLen];` |
|      - | 4054 | `	/* Perform the requested operation */` |
|      4 | 4055 | `	for(;;){` |
|      9 | 4056 | `		zCur = zIn;` |
|      - | 4057 | `		/* Delimit the string */` |
|     21 | 4058 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 4059 | `			zIn++;` |
|      1 | 4060 | `		}` |
|      9 | 4061 | `		if( zCur < zIn ){` |
|      - | 4062 | `			/* Output chunk verbatim */` |
|      9 | 4063 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4064 | `		}` |
|      9 | 4065 | `		if( zIn >= zEnd ){` |
|      - | 4066 | `			/* No more input to process */` |
|      5 | 4067 | `			break;` |
|      - | 4068 | `		}` |
|      - | 4069 | `		/* Output the HTML line break */` |
|      - | 4070 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 4071 | `		if( is_xhtml ){` |
|      3 | 4072 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 4073 | `		}else{` |
|      3 | 4074 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4075 | `		}` |
|      5 | 4076 | `		zCur = zIn;` |
|      - | 4077 | `		/* Append trailing line */` |
|     11 | 4078 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4079 | `			zIn++;` |
|      1 | 4080 | `		}` |
|      5 | 4081 | `		if( zCur < zIn ){` |
|      - | 4082 | `			/* Output chunk verbatim */` |
|      5 | 4083 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4084 | `		}` |
|      1 | 4085 | `	}` |
|      5 | 4086 | `	return PH7_OK;` |
|      3 | 4087 | `}` |
|      - | 4088 | `/*` |
|      - | 4089 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4090 | ` *  According to the PHP reference manual.` |
|      - | 4091 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4092 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4093 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4094 | ` * This applies to both sprintf() and printf().` |
|      - | 4095 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4096 | ` * or more of these elements, in order:` |
|      - | 4097 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4098 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4099 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4100 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4101 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4102 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4103 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4104 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4105 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4106 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4107 | ` *   should result in.` |
|      - | 4108 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4109 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4110 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4111 | ` *   limit to the string.` |
|      - | 4112 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4113 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4114 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4115 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4116 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4117 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4118 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4119 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4120 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4121 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4122 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4123 | ` *       g - shorter of %e and %f.` |
|      - | 4124 | ` *       G - shorter of %E and %f.` |
|      - | 4125 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4126 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4127 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4128 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4129 | ` */` |
|      - | 4130 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4131 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4132 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4133 | `/*` |
|      - | 4134 | `** Conversion types fall into various categories as defined by the` |
|      - | 4135 | `** following enumeration.` |
|      - | 4136 | `*/` |
|      - | 4137 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4138 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4139 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4140 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4141 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4142 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4143 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4144 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4145 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4146 |  |
|      - | 4147 | `/*` |
|      - | 4148 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4149 | `*/` |
|      - | 4150 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4151 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4152 | `/*` |
|      - | 4153 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4154 | `** by an instance of the following structure` |
|      - | 4155 | `*/` |
|      - | 4156 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4157 | `struct ph7_fmt_info` |
|      - | 4158 | `{` |
|      - | 4159 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4160 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4161 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4162 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4163 | `  char *charset; /* The character set for conversion */` |
|      - | 4164 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4165 | `};` |
|      - | 4166 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4167 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4168 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4169 | `/*` |
|      - | 4170 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4171 | ` * used conversion types first.` |
|      - | 4172 | ` */` |
|      - | 4173 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4174 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4175 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4176 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4177 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4178 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4179 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4180 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4181 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4182 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4183 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4184 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4185 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4186 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4187 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4188 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4189 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4190 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4191 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4192 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4193 | `};` |
|      - | 4194 | `/*` |
|      - | 4195 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4196 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4197 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4198 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4199 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4200 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4201 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4202 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4203 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4204 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4205 | ` * "all valid".)` |
|      - | 4206 | ` */` |
|    412 | 4207 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 4208 | `{` |
|    413 | 4209 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4210 | `	int c,idx;` |
|   3449 | 4211 | `	while( zIn < zEnd ){` |
|   3057 | 4212 | `		if( zIn[0] != '%' ){` |
|   2265 | 4213 | `			zIn++;` |
|   2265 | 4214 | `			continue;` |
|      - | 4215 | `		}` |
|    793 | 4216 | `		zIn++; /* jump the percent sign */` |
|      - | 4217 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4218 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4219 | `		 * unknown specifier, matching php. */` |
|    977 | 4220 | `		while( zIn < zEnd ){` |
|    975 | 4221 | `			c = zIn[0];` |
|    975 | 4222 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    185 | 4223 | `				zIn++;` |
|    185 | 4224 | `				continue;` |
|      - | 4225 | `			}` |
|    791 | 4226 | `			if( c=='\'' ){` |
|    ! 0 | 4227 | `				zIn++;` |
|    ! 0 | 4228 | `				if( zIn < zEnd ){` |
|    ! 0 | 4229 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 4230 | `				}` |
|    ! 0 | 4231 | `				continue;` |
|      - | 4232 | `			}` |
|    791 | 4233 | `			break;` |
|    ! 0 | 4234 | `		}` |
|      - | 4235 | `		/* field width */` |
|   1009 | 4236 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    217 | 4237 | `			zIn++;` |
|      1 | 4238 | `		}` |
|      - | 4239 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4240 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    793 | 4241 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 4242 | `			zIn++;` |
|    ! 0 | 4243 | `			while( zIn < zEnd ){` |
|    ! 0 | 4244 | `				c = zIn[0];` |
|    ! 0 | 4245 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4246 | `					zIn++;` |
|    ! 0 | 4247 | `					continue;` |
|      - | 4248 | `				}` |
|    ! 0 | 4249 | `				if( c=='\'' ){` |
|    ! 0 | 4250 | `					zIn++;` |
|    ! 0 | 4251 | `					if( zIn < zEnd ){` |
|    ! 0 | 4252 | `						zIn++;` |
|    ! 0 | 4253 | `					}` |
|    ! 0 | 4254 | `					continue;` |
|      - | 4255 | `				}` |
|    ! 0 | 4256 | `				break;` |
|    ! 0 | 4257 | `			}` |
|    ! 0 | 4258 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 4259 | `				zIn++;` |
|    ! 0 | 4260 | `			}` |
|    ! 0 | 4261 | `		}` |
|      - | 4262 | `		/* precision */` |
|    793 | 4263 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 4264 | `			zIn++;` |
|    183 | 4265 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 4266 | `				zIn++;` |
|      1 | 4267 | `			}` |
|     43 | 4268 | `		}` |
|      - | 4269 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    793 | 4270 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4271 | `			zIn++;` |
|      5 | 4272 | `		}` |
|    793 | 4273 | `		if( zIn >= zEnd ){` |
|      - | 4274 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4275 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4276 | `			break;` |
|      - | 4277 | `		}` |
|    791 | 4278 | `		c = zIn[0];` |
|    791 | 4279 | `		zIn++; /* jump the conversion specifier */` |
|   3333 | 4280 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3315 | 4281 | `			if( c == aFmt[idx].fmttype ){` |
|    773 | 4282 | `				break;` |
|      - | 4283 | `			}` |
|   1272 | 4284 | `		}` |
|    791 | 4285 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4286 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4287 | `			return TRUE;` |
|      - | 4288 | `		}` |
|      1 | 4289 | `	}` |
|    395 | 4290 | `	return FALSE;` |
|    207 | 4291 | `}` |
|      - | 4292 | `/*` |
|      - | 4293 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4294 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4295 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4296 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4297 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4298 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4299 | ` */` |
|    412 | 4300 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 4301 | `{` |
|    413 | 4302 | `	int badSpec = 0;` |
|    413 | 4303 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4304 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4305 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4306 | `	}` |
|    395 | 4307 | `	return PH7_OK;` |
|    207 | 4308 | `}` |
|      - | 4309 | `/*` |
|      - | 4310 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4311 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4312 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4313 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4314 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4315 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4316 | ` */` |
|    432 | 4317 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 4318 | `{` |
|    433 | 4319 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4320 | `		char zBuf[64];` |
|     13 | 4321 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4322 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|      4 | 4323 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4324 | `	}` |
|    425 | 4325 | `	return PH7_OK;` |
|    217 | 4326 | `}` |
|      - | 4327 | `/*` |
|      - | 4328 | ` * Format a given string.` |
|      - | 4329 | ` * The root program.  All variations call this core.` |
|      - | 4330 | ` * INPUTS:` |
|      - | 4331 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4332 | ` *            1. A pointer to the call context.` |
|      - | 4333 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4334 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4335 | ` *            3. An integer number of characters to be output.` |
|      - | 4336 | ` *               (Note: This number might be zero.)` |
|      - | 4337 | ` *            4. Upper layer private data.` |
|      - | 4338 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4339 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4340 | ` */` |
|    394 | 4341 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4342 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4343 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4344 | `	const char *zIn,    /* Format string */` |
|      - | 4345 | `	int nByte,          /* Format string length */` |
|      - | 4346 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4347 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4348 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4349 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4350 | `	)` |
|      1 | 4351 | `{` |
|    395 | 4352 | `	char spaces[] = "                                                  ";` |
|      - | 4353 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    395 | 4354 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4355 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4356 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4357 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4358 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4359 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4360 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4361 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4362 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4363 | `	ph7_int64 iVal;` |
|      - | 4364 | `	int precision;           /* Precision of the current field */` |
|      - | 4365 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4366 | `	int c,rc,n;` |
|      - | 4367 | `	int length;              /* Length of the field */` |
|      - | 4368 | `	int prefix;` |
|      - | 4369 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4370 | `	int width;               /* Width of the current field */` |
|      - | 4371 | `	int idx;` |
|    395 | 4372 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4373 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4374 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4375 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4376 | `	 * seen here is always valid. */` |
|      - | 4377 | `	/* Start the format process */` |
|    583 | 4378 | `	for(;;){` |
|   1167 | 4379 | `		zCur = zIn;` |
|   3417 | 4380 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2251 | 4381 | `			zIn++;` |
|      1 | 4382 | `		}` |
|   1167 | 4383 | `		if( zCur < zIn ){` |
|      - | 4384 | `			/* Consume chunk verbatim */` |
|    725 | 4385 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    725 | 4386 | `			if( rc != SXRET_OK ){` |
|      - | 4387 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4388 | `				break;` |
|      - | 4389 | `			}` |
|    362 | 4390 | `		}` |
|   1167 | 4391 | `		if( zIn >= zEnd ){` |
|      - | 4392 | `			/* No more input to process,break immediately */` |
|    393 | 4393 | `			break;` |
|      - | 4394 | `		}` |
|      - | 4395 | `		/* Find out what flags are present */` |
|    775 | 4396 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    774 | 4397 | `			flag_alternateform = flag_zeropad = 0;` |
|    775 | 4398 | `		zIn++; /* Jump the precent sign */` |
|    387 | 4399 | `		do{` |
|    959 | 4400 | `			c = zIn[0];` |
|    959 | 4401 | `			switch( c ){` |
|     15 | 4402 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4403 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4404 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    159 | 4405 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4406 | `			case '\'':` |
|    ! 0 | 4407 | `				zIn++;` |
|    ! 0 | 4408 | `				if( zIn < zEnd ){` |
|      - | 4409 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4410 | `					c = zIn[0];` |
|    ! 0 | 4411 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4412 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4413 | `					}` |
|    ! 0 | 4414 | `					c = 0;` |
|    ! 0 | 4415 | `				}` |
|    ! 0 | 4416 | `				break;` |
|    774 | 4417 | `			default:                                       break;` |
|      - | 4418 | `			}` |
|    959 | 4419 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4420 | `		/* Get the field width */` |
|    775 | 4421 | `		width = 0;` |
|   1378 | 4422 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    217 | 4423 | `			width = width*10 + (zIn[0] - '0');` |
|    217 | 4424 | `			zIn++;` |
|      1 | 4425 | `		}` |
|    775 | 4426 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4427 | `			/* Position specifer */` |
|    ! 0 | 4428 | `			if( width > 0 ){` |
|    ! 0 | 4429 | `				n = width;` |
|    ! 0 | 4430 | `				if( vf && n > 0 ){` |
|    ! 0 | 4431 | `					n--;` |
|    ! 0 | 4432 | `				}` |
|    ! 0 | 4433 | `			}` |
|    ! 0 | 4434 | `			zIn++;` |
|    ! 0 | 4435 | `			width = 0;` |
|      - | 4436 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4437 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4438 | `			 * not just zero-padding. */` |
|    ! 0 | 4439 | `			do{` |
|    ! 0 | 4440 | `				c = zIn[0];` |
|    ! 0 | 4441 | `				switch( c ){` |
|    ! 0 | 4442 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4443 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4444 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4445 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4446 | `				case '\'':` |
|    ! 0 | 4447 | `					zIn++;` |
|    ! 0 | 4448 | `					if( zIn < zEnd ){` |
|    ! 0 | 4449 | `						c = zIn[0];` |
|    ! 0 | 4450 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4451 | `							spaces[idx] = (char)c;` |
|    ! 0 | 4452 | `						}` |
|    ! 0 | 4453 | `						c = 0;` |
|    ! 0 | 4454 | `					}` |
|    ! 0 | 4455 | `					break;` |
|    ! 0 | 4456 | `				default:                                       break;` |
|      - | 4457 | `				}` |
|    ! 0 | 4458 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 4459 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4460 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4461 | `				zIn++;` |
|    ! 0 | 4462 | `			}` |
|    ! 0 | 4463 | `		}` |
|    775 | 4464 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4465 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4466 | `		}` |
|      - | 4467 | `		/* Get the precision */` |
|    775 | 4468 | `		precision = -1;` |
|    775 | 4469 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 4470 | `			precision = 0;` |
|     87 | 4471 | `			zIn++;` |
|    226 | 4472 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 4473 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 4474 | `				zIn++;` |
|      1 | 4475 | `			}` |
|     43 | 4476 | `		}` |
|      - | 4477 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4478 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4479 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    775 | 4480 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4481 | `			zIn++;` |
|      4 | 4482 | `		}` |
|    775 | 4483 | `		if( zIn >= zEnd ){` |
|      - | 4484 | `			/* No more input */` |
|      3 | 4485 | `			break;` |
|      - | 4486 | `		}` |
|      - | 4487 | `		/* Fetch the info entry for the field */` |
|    773 | 4488 | `		pInfo = 0;` |
|    773 | 4489 | `		xtype = PH7_FMT_ERROR;` |
|    773 | 4490 | `		c = zIn[0];` |
|    773 | 4491 | `		zIn++; /* Jump the format specifer */` |
|   3009 | 4492 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3009 | 4493 | `			if( c==aFmt[idx].fmttype ){` |
|    773 | 4494 | `				pInfo = &aFmt[idx];` |
|    773 | 4495 | `				xtype = pInfo->type;` |
|    773 | 4496 | `				break;` |
|      - | 4497 | `			}` |
|   1119 | 4498 | `		}` |
|    773 | 4499 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    773 | 4500 | `		length = 0;` |
|      - | 4501 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4502 | `		 /*` |
|      - | 4503 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4504 | `		  **` |
|      - | 4505 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4506 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4507 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4508 | `		  **                               field width was negative.` |
|      - | 4509 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4510 | `		  **                               the conversion character.` |
|      - | 4511 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4512 | `		  **   width                       The specified field width.  This is` |
|      - | 4513 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4514 | `		  **   precision                   The specified precision.  The default` |
|      - | 4515 | `		  **                               is -1.` |
|      - | 4516 | `		  */` |
|    773 | 4517 | `		switch(xtype){` |
|      3 | 4518 | `		case PH7_FMT_PERCENT:` |
|      - | 4519 | `			/* A literal percent character */` |
|      7 | 4520 | `			zWorker[0] = '%';` |
|      7 | 4521 | `			length = (int)sizeof(char);` |
|      7 | 4522 | `			break;` |
|      3 | 4523 | `		case PH7_FMT_CHARX:` |
|      - | 4524 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4525 | `			 * with that ASCII value` |
|      - | 4526 | `			 */` |
|      7 | 4527 | `			pArg = NEXT_ARG;` |
|      7 | 4528 | `			if( pArg == 0 ){` |
|      3 | 4529 | `				c = 0;` |
|      2 | 4530 | `			}else{` |
|      5 | 4531 | `				c = ph7_value_to_int(pArg);` |
|      - | 4532 | `			}` |
|      - | 4533 | `			/* NUL byte is an acceptable value */` |
|      7 | 4534 | `			zWorker[0] = (char)c;` |
|      7 | 4535 | `			length = (int)sizeof(char);` |
|      7 | 4536 | `			break;` |
|    162 | 4537 | `		case PH7_FMT_STRING:` |
|      - | 4538 | `			/* the argument is treated as and presented as a string */` |
|    325 | 4539 | `			pArg = NEXT_ARG;` |
|    325 | 4540 | `			if( pArg == 0 ){` |
|    ! 0 | 4541 | `				length = 0;` |
|    ! 0 | 4542 | `			}else{` |
|    325 | 4543 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4544 | `			}` |
|    325 | 4545 | `			if( length < 1 ){` |
|    ! 0 | 4546 | `				zBuf = " ";` |
|    ! 0 | 4547 | `				length = (int)sizeof(char);` |
|    ! 0 | 4548 | `			}` |
|    325 | 4549 | `			if( precision>=0 && precision<length ){` |
|      3 | 4550 | `				length = precision;` |
|      1 | 4551 | `			}` |
|    325 | 4552 | `			if( flag_zeropad ){` |
|      - | 4553 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4554 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4555 | `					spaces[idx] = '0';` |
|    ! 0 | 4556 | `				}` |
|    ! 0 | 4557 | `			}` |
|    325 | 4558 | `			break;` |
|    130 | 4559 | `		case PH7_FMT_RADIX:` |
|    261 | 4560 | `			pArg = NEXT_ARG;` |
|    261 | 4561 | `			if( pArg == 0 ){` |
|    ! 0 | 4562 | `				iVal = 0;` |
|    ! 0 | 4563 | `			}else{` |
|    261 | 4564 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4565 | `			}` |
|      - | 4566 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    261 | 4567 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4568 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4569 | `			}` |
|      - | 4570 | `#if 1` |
|      - | 4571 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4572 | `        ** I think this is stupid.*/` |
|    261 | 4573 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4574 | `#else` |
|      - | 4575 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4576 | `        ** but leave the prefix for hex.*/` |
|      - | 4577 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4578 | `#endif` |
|    261 | 4579 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    237 | 4580 | `          if( iVal<0 ){` |
|     25 | 4581 | `            iVal = -iVal;` |
|      - | 4582 | `			/* Ticket 1433-003 */` |
|     25 | 4583 | `			if( iVal < 0 ){` |
|      - | 4584 | `				/* Overflow */` |
|    ! 0 | 4585 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4586 | `			}` |
|     25 | 4587 | `            prefix = '-';` |
|    225 | 4588 | `          }else if( flag_plussign )  prefix = '+';` |
|    211 | 4589 | `          else if( flag_blanksign )  prefix = ' ';` |
|    209 | 4590 | `          else                       prefix = 0;` |
|    119 | 4591 | `        }else{` |
|     25 | 4592 | `			if( iVal<0 ){` |
|    ! 0 | 4593 | `				iVal = -iVal;` |
|      - | 4594 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4595 | `				if( iVal < 0 ){` |
|      - | 4596 | `					/* Overflow */` |
|    ! 0 | 4597 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4598 | `				}` |
|    ! 0 | 4599 | `			}` |
|     25 | 4600 | `			prefix = 0;` |
|      - | 4601 | `		}` |
|    261 | 4602 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    149 | 4603 | `          precision = width-(prefix!=0);` |
|     74 | 4604 | `        }` |
|    261 | 4605 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4606 | `        {` |
|      - | 4607 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4608 | `          register int base;` |
|    261 | 4609 | `          cset = pInfo->charset;` |
|    261 | 4610 | `          base = pInfo->base;` |
|    130 | 4611 | `          do{                                           /* Convert to ascii */` |
|    333 | 4612 | `            *(--zBuf) = cset[iVal%base];` |
|    333 | 4613 | `            iVal = iVal/base;` |
|    333 | 4614 | `          }while( iVal>0 );` |
|      - | 4615 | `        }` |
|    261 | 4616 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    427 | 4617 | `        for(idx=precision-length; idx>0; idx--){` |
|    167 | 4618 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     84 | 4619 | `        }` |
|    261 | 4620 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    261 | 4621 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4622 | `          char *pre, x;` |
|    ! 0 | 4623 | `          pre = pInfo->prefix;` |
|    ! 0 | 4624 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4625 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4626 | `          }` |
|    ! 0 | 4627 | `        }` |
|    261 | 4628 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    261 | 4629 | `		break;` |
|     88 | 4630 | `		case PH7_FMT_FLOAT:` |
|      - | 4631 | `		case PH7_FMT_EXP:` |
|      - | 4632 | `		case PH7_FMT_GENERIC:{` |
|      - | 4633 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4634 | `		double realvalue;` |
|      - | 4635 | `		char zFmt[8];` |
|      - | 4636 | `		int nOut, nFmt;` |
|    177 | 4637 | `		pArg = NEXT_ARG;` |
|    177 | 4638 | `		if( pArg == 0 ){` |
|    ! 0 | 4639 | `			realvalue = 0;` |
|    ! 0 | 4640 | `		}else{` |
|    177 | 4641 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4642 | `		}` |
|      - | 4643 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4644 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 4645 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4646 | `			zBuf = "NaN";` |
|     21 | 4647 | `			length = 3;` |
|     21 | 4648 | `			width = 0;` |
|     21 | 4649 | `			break;` |
|      - | 4650 | `		}` |
|    157 | 4651 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4652 | `			if( realvalue < 0.0 ){` |
|     15 | 4653 | `				zBuf = "-INF";` |
|     15 | 4654 | `				length = 4;` |
|      8 | 4655 | `			}else{` |
|     23 | 4656 | `				zBuf = "INF";` |
|     23 | 4657 | `				length = 3;` |
|      - | 4658 | `			}` |
|     37 | 4659 | `			width = 0;` |
|     37 | 4660 | `			break;` |
|      - | 4661 | `		}` |
|    121 | 4662 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 4663 | `		if( precision > 53 ){` |
|      - | 4664 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4665 | `			 * (message prefixed with the active function's name, like` |
|      - | 4666 | `			 * php_error_docref). */` |
|      - | 4667 | `			char zMsg[160];` |
|      4 | 4668 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4669 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4670 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4671 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4672 | `			precision = 53;` |
|      1 | 4673 | `		}` |
|      - | 4674 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4675 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 4676 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4677 | `			realvalue = 0.0;` |
|      4 | 4678 | `		}` |
|      - | 4679 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4680 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4681 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4682 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4683 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 4684 | `		nFmt = 0;` |
|    121 | 4685 | `		zFmt[nFmt++] = '%';` |
|    121 | 4686 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4687 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4688 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 4689 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 4690 | `		zFmt[nFmt++] = '.';` |
|    121 | 4691 | `		zFmt[nFmt++] = '*';` |
|    165 | 4692 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 4693 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 4694 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 4695 | `		zFmt[nFmt] = 0;` |
|    121 | 4696 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 4697 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4698 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4699 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4700 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4701 | `		}` |
|    121 | 4702 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 4703 | `		zBuf = zWorker;` |
|    121 | 4704 | `		length = nOut;` |
|      - | 4705 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4706 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 4707 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4708 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4709 | `        ** set and we are not left justified */` |
|    121 | 4710 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4711 | `          int i;` |
|      7 | 4712 | `          int nPad = width - length;` |
|     51 | 4713 | `          for(i=width; i>=nPad; i--){` |
|     45 | 4714 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 4715 | `          }` |
|      7 | 4716 | `          i = prefix!=0;` |
|     29 | 4717 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 4718 | `          length = width;` |
|      3 | 4719 | `        }` |
|      - | 4720 | `#else` |
|      - | 4721 | `         zBuf = " ";` |
|      - | 4722 | `		 length = (int)sizeof(char);` |
|      - | 4723 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 4724 | `		 break;` |
|      - | 4725 | `							 }` |
|    ! 0 | 4726 | `		default:` |
|      - | 4727 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4728 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4729 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4730 | `			length = 0;` |
|    ! 0 | 4731 | `			break;` |
|      - | 4732 | `		}` |
|      - | 4733 | `		 /*` |
|      - | 4734 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4735 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4736 | `		 ** the output.` |
|      - | 4737 | `		 */` |
|    773 | 4738 | `    if( !flag_leftjustify ){` |
|      - | 4739 | `      register int nspace;` |
|    759 | 4740 | `      nspace = width-length;` |
|    759 | 4741 | `      if( nspace>0 ){` |
|      7 | 4742 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4743 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4744 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4745 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4746 | `			}` |
|    ! 0 | 4747 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4748 | `        }` |
|      7 | 4749 | `        if( nspace>0 ){` |
|      7 | 4750 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 4751 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4752 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4753 | `			}` |
|      3 | 4754 | `		}` |
|      3 | 4755 | `      }` |
|    379 | 4756 | `    }` |
|    773 | 4757 | `    if( length>0 ){` |
|    773 | 4758 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    773 | 4759 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4760 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4761 | `		}` |
|    386 | 4762 | `    }` |
|    773 | 4763 | `    if( flag_leftjustify ){` |
|      - | 4764 | `      register int nspace;` |
|     15 | 4765 | `      nspace = width-length;` |
|     15 | 4766 | `      if( nspace>0 ){` |
|     11 | 4767 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4768 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4769 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4770 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4771 | `			}` |
|    ! 0 | 4772 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4773 | `        }` |
|     11 | 4774 | `        if( nspace>0 ){` |
|     11 | 4775 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 4776 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4777 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4778 | `			}` |
|      5 | 4779 | `		}` |
|      5 | 4780 | `      }` |
|      7 | 4781 | `    }` |
|      1 | 4782 | ` }/* for(;;) */` |
|    395 | 4783 | `	return SXRET_OK;` |
|    198 | 4784 | `}` |
|      - | 4785 | `/*` |
|      - | 4786 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4787 | ` */` |
|    352 | 4788 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4789 | `{` |
|      - | 4790 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4791 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4792 | `	 * non-OK rc also stops the format loop. */` |
|    353 | 4793 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    353 | 4794 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    353 | 4795 | `	return *pRc;` |
|      1 | 4796 | `}` |
|      - | 4797 | `/*` |
|      - | 4798 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4799 | ` *  Return a formatted string.` |
|      - | 4800 | ` * Parameters` |
|      - | 4801 | ` *  $format` |
|      - | 4802 | ` *    The format string (see block comment above)` |
|      - | 4803 | ` * Return` |
|      - | 4804 | ` *  A string produced according to the formatting string format.` |
|      - | 4805 | ` */` |
|    188 | 4806 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4807 | `{` |
|      - | 4808 | `	const char *zFormat;` |
|    189 | 4809 | `	sxi32 rc = SXRET_OK;` |
|      - | 4810 | `	int nLen;` |
|    189 | 4811 | `	if( nArg < 1 ){` |
|      - | 4812 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4813 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4814 | `		return PH7_OK;` |
|      - | 4815 | `	}` |
|      - | 4816 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    189 | 4817 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    189 | 4818 | `	if( rc != PH7_OK ){` |
|      5 | 4819 | `		return rc;` |
|      - | 4820 | `	}` |
|      - | 4821 | `	/* Extract the string format (scalars/null coerce). */` |
|    185 | 4822 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    185 | 4823 | `	if( nLen < 1 ){` |
|      - | 4824 | `		/* Empty string */` |
|    ! 0 | 4825 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4826 | `		return PH7_OK;` |
|      - | 4827 | `	}` |
|      - | 4828 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4829 | `	 * output; propagate the throw status verbatim. */` |
|    185 | 4830 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    185 | 4831 | `	if( rc != PH7_OK ){` |
|     17 | 4832 | `		return rc;` |
|      - | 4833 | `	}` |
|      - | 4834 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    169 | 4835 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    169 | 4836 | `	if( rc != SXRET_OK ){` |
|      - | 4837 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4838 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4839 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4840 | `	}` |
|    169 | 4841 | `	return PH7_OK;` |
|     95 | 4842 | `}` |
|      - | 4843 | `/*` |
|      - | 4844 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4845 | ` */` |
|   1130 | 4846 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4847 | `{` |
|   1131 | 4848 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4849 | `	/* Call the VM output consumer directly */` |
|   1131 | 4850 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4851 | `	/* Increment counter */` |
|   1131 | 4852 | `	*pCounter += nLen;` |
|   1131 | 4853 | `	return PH7_OK;` |
|      1 | 4854 | `}` |
|      - | 4855 | `/*` |
|      - | 4856 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4857 | ` *  Output a formatted string.` |
|      - | 4858 | ` * Parameters` |
|      - | 4859 | ` *  $format` |
|      - | 4860 | ` *   See sprintf() for a description of format.` |
|      - | 4861 | ` * Return` |
|      - | 4862 | ` *  The length of the outputted string.` |
|      - | 4863 | ` */` |
|    200 | 4864 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4865 | `{` |
|    201 | 4866 | `	ph7_int64 nCounter = 0;` |
|      - | 4867 | `	const char *zFormat;` |
|      - | 4868 | `	int nLen;` |
|    201 | 4869 | `	if( nArg < 1 ){` |
|      - | 4870 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4871 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4872 | `		return PH7_OK;` |
|      - | 4873 | `	}` |
|      - | 4874 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 4875 | `	{` |
|    201 | 4876 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    201 | 4877 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4878 | `			return rcf;` |
|      - | 4879 | `		}` |
|      - | 4880 | `	}` |
|      - | 4881 | `	/* Extract the string format (scalars/null coerce). */` |
|    201 | 4882 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 4883 | `	if( nLen < 1 ){` |
|      - | 4884 | `		/* Empty string */` |
|    ! 0 | 4885 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4886 | `		return PH7_OK;` |
|      - | 4887 | `	}` |
|      - | 4888 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4889 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4890 | `	{` |
|    201 | 4891 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    201 | 4892 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4893 | `			return rcv;` |
|      - | 4894 | `		}` |
|      - | 4895 | `	}` |
|      - | 4896 | `	/* Format the string */` |
|    201 | 4897 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4898 | `	/* Return the length of the outputted string */` |
|    201 | 4899 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 4900 | `	return PH7_OK;` |
|    101 | 4901 | `}` |
|      - | 4902 | `/*` |
|      - | 4903 | ` * int vprintf(string $format,array $args)` |
|      - | 4904 | ` *  Output a formatted string.` |
|      - | 4905 | ` * Parameters` |
|      - | 4906 | ` *  $format` |
|      - | 4907 | ` *   See sprintf() for a description of format.` |
|      - | 4908 | ` * Return` |
|      - | 4909 | ` *  The length of the outputted string.` |
|      - | 4910 | ` */` |
|      4 | 4911 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4912 | `{` |
|      5 | 4913 | `	ph7_int64 nCounter = 0;` |
|      - | 4914 | `	const char *zFormat;` |
|      - | 4915 | `	ph7_hashmap *pMap;` |
|      - | 4916 | `	SySet sArg;` |
|      - | 4917 | `	int nLen,n;` |
|      - | 4918 | `	sxi32 rcFmt;` |
|      5 | 4919 | `	if( nArg < 2 ){` |
|      - | 4920 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4921 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4922 | `		return PH7_OK;` |
|      - | 4923 | `	}` |
|      - | 4924 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4925 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4926 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4927 | `		return rcFmt;` |
|      - | 4928 | `	}` |
|      5 | 4929 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4930 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4931 | `		char zBuf[64];` |
|      4 | 4932 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4933 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4934 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4935 | `	}` |
|      - | 4936 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4937 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4938 | `	if( nLen < 1 ){` |
|      - | 4939 | `		/* Empty string */` |
|    ! 0 | 4940 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4941 | `		return PH7_OK;` |
|      - | 4942 | `	}` |
|      - | 4943 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4944 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4945 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4946 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4947 | `		return rcFmt;` |
|      - | 4948 | `	}` |
|      - | 4949 | `	/* Point to the hashmap */` |
|      3 | 4950 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4951 | `	/* Extract arguments from the hashmap */` |
|      3 | 4952 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4953 | `	/* Format the string */` |
|      3 | 4954 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4955 | `	/* Release the container */` |
|      3 | 4956 | `	SySetRelease(&sArg);` |
|      - | 4957 | `	/* Return the length of the outputted string */` |
|      3 | 4958 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4959 | `	return PH7_OK;` |
|      3 | 4960 | `}` |
|      - | 4961 | `/*` |
|      - | 4962 | ` * int vsprintf(string $format,array $args)` |
|      - | 4963 | ` *  Output a formatted string.` |
|      - | 4964 | ` * Parameters` |
|      - | 4965 | ` *  $format` |
|      - | 4966 | ` *   See sprintf() for a description of format.` |
|      - | 4967 | ` * Return` |
|      - | 4968 | ` *  A string produced according to the formatting string format.` |
|      - | 4969 | ` */` |
|     22 | 4970 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4971 | `{` |
|      - | 4972 | `	const char *zFormat;` |
|      - | 4973 | `	ph7_hashmap *pMap;` |
|      - | 4974 | `	SySet sArg;` |
|     23 | 4975 | `	sxi32 rc = SXRET_OK;` |
|      - | 4976 | `	sxi32 rcFmt;` |
|      - | 4977 | `	int nLen,n;` |
|     23 | 4978 | `	if( nArg < 2 ){` |
|      - | 4979 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4980 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4981 | `		return PH7_OK;` |
|      - | 4982 | `	}` |
|      - | 4983 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     23 | 4984 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     23 | 4985 | `	if( rc != PH7_OK ){` |
|      5 | 4986 | `		return rc;` |
|      - | 4987 | `	}` |
|     19 | 4988 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4989 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4990 | `		char zBuf[64];` |
|     16 | 4991 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4992 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 4993 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4994 | `	}` |
|      - | 4995 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 4996 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4997 | `	if( nLen < 1 ){` |
|      - | 4998 | `		/* Empty string */` |
|    ! 0 | 4999 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5000 | `		return PH7_OK;` |
|      - | 5001 | `	}` |
|      - | 5002 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5003 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 5004 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 5005 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5006 | `		return rcFmt;` |
|      - | 5007 | `	}` |
|      - | 5008 | `	/* Point to hashmap */` |
|      9 | 5009 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5010 | `	/* Extract arguments from the hashmap */` |
|      9 | 5011 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5012 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 5013 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5014 | `	/* Release the container */` |
|      9 | 5015 | `	SySetRelease(&sArg);` |
|      9 | 5016 | `	if( rc != SXRET_OK ){` |
|      - | 5017 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5018 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5019 | `	}` |
|      9 | 5020 | `	return PH7_OK;` |
|     12 | 5021 | `}` |
|      - | 5022 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5023 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5024 | `/*` |
|      - | 5025 | ` * Symisc eXtension.` |
|      - | 5026 | ` * string size_format(int64 $size)` |
|      - | 5027 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5028 | ` *  Example:` |
|      - | 5029 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5030 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5031 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5032 | ` * Parameter` |
|      - | 5033 | ` *  $size` |
|      - | 5034 | ` *    Entity size in bytes.` |
|      - | 5035 | ` * Return` |
|      - | 5036 | ` *   Formatted string representation of the given size.` |
|      - | 5037 | ` */` |
|     24 | 5038 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5039 | `{` |
|      - | 5040 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5041 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5042 | `	sxi32 nRest,i_32;` |
|      - | 5043 | `	ph7_int64 iSize;` |
|     25 | 5044 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5045 |  |
|     25 | 5046 | `	if( nArg < 1 ){` |
|      - | 5047 | `		/* Missing argument,return the empty string */` |
|      3 | 5048 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5049 | `		return PH7_OK;` |
|      - | 5050 | `	}` |
|      - | 5051 | `	/* Extract the given size */` |
|     23 | 5052 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5053 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5054 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5055 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5056 | `		return PH7_OK;` |
|      - | 5057 | `	}` |
|     19 | 5058 | `	for(;;){` |
|     39 | 5059 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5060 | `		iSize >>= 10;` |
|     39 | 5061 | `		c++;` |
|     39 | 5062 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5063 | `			break;` |
|      - | 5064 | `		}` |
|      1 | 5065 | `	}` |
|     19 | 5066 | `	nRest /= 100;` |
|     19 | 5067 | `	if( nRest > 9 ){` |
|    ! 0 | 5068 | `		nRest = 9;` |
|    ! 0 | 5069 | `	}` |
|     19 | 5070 | `	if( iSize > 999 ){` |
|    ! 0 | 5071 | `		c++;` |
|    ! 0 | 5072 | `		nRest = 9;` |
|    ! 0 | 5073 | `		iSize = 0;` |
|    ! 0 | 5074 | `	}` |
|     19 | 5075 | `	i_32 = (sxi32)iSize;` |
|      - | 5076 | `	/* Format */` |
|     19 | 5077 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5078 | `	return PH7_OK;` |
|     13 | 5079 | `}` |
|      - | 5080 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5081 | `/*` |
|      - | 5082 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5083 | ` *   Calculate the md5 hash of a string.` |
|      - | 5084 | ` * Parameter` |
|      - | 5085 | ` *  $str` |
|      - | 5086 | ` *   Input string` |
|      - | 5087 | ` * $raw_output` |
|      - | 5088 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5089 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5090 | ` * Return` |
|      - | 5091 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5092 | ` */` |
|     12 | 5093 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5094 | `{` |
|      - | 5095 | `	unsigned char zDigest[16];` |
|     13 | 5096 | `	int raw_output = FALSE;` |
|      - | 5097 | `	const void *pIn;` |
|      - | 5098 | `	int nLen;` |
|     13 | 5099 | `	if( nArg < 1 ){` |
|      - | 5100 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5101 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5102 | `		return PH7_OK;` |
|      - | 5103 | `	}` |
|      - | 5104 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5105 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5106 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5107 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5108 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5109 | `	}` |
|      - | 5110 | `	/* Compute the MD5 digest */` |
|     13 | 5111 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5112 | `	if( raw_output ){` |
|      - | 5113 | `		/* Output raw digest */` |
|      5 | 5114 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5115 | `	}else{` |
|      - | 5116 | `		/* Perform a binary to hex conversion */` |
|      9 | 5117 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5118 | `	}` |
|     13 | 5119 | `	return PH7_OK;` |
|      7 | 5120 | `}` |
|      - | 5121 | `/*` |
|      - | 5122 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5123 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5124 | ` * Parameter` |
|      - | 5125 | ` *  $str` |
|      - | 5126 | ` *   Input string` |
|      - | 5127 | ` * $raw_output` |
|      - | 5128 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5129 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5130 | ` * Return` |
|      - | 5131 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5132 | ` */` |
|     10 | 5133 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5134 | `{` |
|      - | 5135 | `	unsigned char zDigest[20];` |
|     11 | 5136 | `	int raw_output = FALSE;` |
|      - | 5137 | `	const void *pIn;` |
|      - | 5138 | `	int nLen;` |
|     11 | 5139 | `	if( nArg < 1 ){` |
|      - | 5140 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5141 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5142 | `		return PH7_OK;` |
|      - | 5143 | `	}` |
|      - | 5144 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5145 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5146 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5147 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5148 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5149 | `	}` |
|      - | 5150 | `	/* Compute the SHA1 digest */` |
|     11 | 5151 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5152 | `	if( raw_output ){` |
|      - | 5153 | `		/* Output raw digest */` |
|      5 | 5154 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5155 | `	}else{` |
|      - | 5156 | `		/* Perform a binary to hex conversion */` |
|      7 | 5157 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5158 | `	}` |
|     11 | 5159 | `	return PH7_OK;` |
|      6 | 5160 | `}` |
|      - | 5161 | `/*` |
|      - | 5162 | ` * int64 crc32(string $str)` |
|      - | 5163 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5164 | ` * Parameter` |
|      - | 5165 | ` *  $str` |
|      - | 5166 | ` *   Input string` |
|      - | 5167 | ` * Return` |
|      - | 5168 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5169 | ` */` |
|      2 | 5170 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5171 | `{` |
|      - | 5172 | `	const void *pIn;` |
|      - | 5173 | `	sxu32 nCRC;` |
|      - | 5174 | `	int nLen;` |
|      3 | 5175 | `	if( nArg < 1 ){` |
|      - | 5176 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5177 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5178 | `		return PH7_OK;` |
|      - | 5179 | `	}` |
|      - | 5180 | `	/* Extract the input string */` |
|      3 | 5181 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5182 | `	if( nLen < 1 ){` |
|      - | 5183 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5184 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5185 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5186 | `		return PH7_OK;` |
|      - | 5187 | `	}` |
|      - | 5188 | `	/* Calculate the sum */` |
|      3 | 5189 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5190 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5191 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5192 | `	return PH7_OK;` |
|      2 | 5193 | `}` |
|      - | 5194 | `/*` |
|      - | 5195 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5196 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5197 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5198 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5199 | ` */` |
|     11 | 5200 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5201 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5202 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5203 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5204 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5205 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5206 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5207 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5208 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5209 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5210 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5211 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5212 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5213 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5214 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5215 | `struct HashAlgo {` |
|      - | 5216 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5217 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5218 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5219 | `	void (*xInit)(HashCtx *);` |
|      - | 5220 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5221 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5222 | `};` |
|      - | 5223 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5224 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5225 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5226 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5227 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5228 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5229 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5230 | `};` |
|      - | 5231 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5232 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5233 | `	sxu32 i;` |
|    279 | 5234 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5235 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5236 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5237 | `			return &aHashAlgo[i];` |
|      - | 5238 | `		}` |
|    106 | 5239 | `	}` |
|      6 | 5240 | `	return 0;` |
|     38 | 5241 | `}` |
|      - | 5242 | `/*` |
|      - | 5243 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5244 | ` *   Generate a hash value (message digest).` |
|      - | 5245 | ` */` |
|     54 | 5246 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5247 | `{` |
|      - | 5248 | `	const HashAlgo *pAlgo;` |
|      - | 5249 | `	const char *zAlgo,*zData;` |
|     56 | 5250 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5251 | `	HashCtx sCtx;` |
|      - | 5252 | `	unsigned char zDigest[64];` |
|     56 | 5253 | `	if( nArg < 2 ){` |
|    ! 0 | 5254 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5255 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5256 | `	}` |
|     56 | 5257 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5258 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5259 | `	if( pAlgo == 0 ){` |
|      3 | 5260 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5261 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5262 | `	}` |
|     53 | 5263 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5264 | `	if( nArg > 2 ){` |
|      9 | 5265 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5266 | `	}` |
|     53 | 5267 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5268 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5269 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5270 | `	if( raw_output ){` |
|      9 | 5271 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5272 | `	}else{` |
|     45 | 5273 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5274 | `	}` |
|     53 | 5275 | `	return PH7_OK;` |
|     29 | 5276 | `}` |
|      - | 5277 | `/*` |
|      - | 5278 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5279 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5280 | ` */` |
|     16 | 5281 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5282 | `{` |
|      - | 5283 | `	const HashAlgo *pAlgo;` |
|      - | 5284 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5285 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5286 | `	HashCtx sCtx;` |
|      - | 5287 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5288 | `	int i,nBlock,nDigest;` |
|     18 | 5289 | `	if( nArg < 3 ){` |
|    ! 0 | 5290 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5291 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5292 | `	}` |
|     18 | 5293 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5294 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5295 | `	if( pAlgo == 0 ){` |
|      3 | 5296 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5297 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5298 | `	}` |
|     15 | 5299 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5300 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5301 | `	if( nArg > 3 ){` |
|      3 | 5302 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5303 | `	}` |
|     15 | 5304 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5305 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5306 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5307 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5308 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5309 | `	if( nKeyLen > nBlock ){` |
|      3 | 5310 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5311 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5312 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5313 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5314 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5315 | `	}` |
|   1039 | 5316 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5317 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5318 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5319 | `	}` |
|      - | 5320 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5321 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5322 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5323 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5324 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5325 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5326 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5327 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5328 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5329 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5330 | `	if( raw_output ){` |
|      3 | 5331 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5332 | `	}else{` |
|     13 | 5333 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5334 | `	}` |
|     15 | 5335 | `	return PH7_OK;` |
|     10 | 5336 | `}` |
|      - | 5337 | `/*` |
|      - | 5338 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5339 | ` *   Timing-attack-safe string comparison.` |
|      - | 5340 | ` */` |
|     14 | 5341 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5342 | `{` |
|      - | 5343 | `	const char *zKnown,*zUser;` |
|      - | 5344 | `	int nKnown,nUser,i;` |
|     17 | 5345 | `	volatile unsigned char vDiff = 0;` |
|     17 | 5346 | `	if( nArg < 2 ){` |
|    ! 0 | 5347 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5348 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5349 | `	}` |
|     17 | 5350 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5351 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5352 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5353 | `			ph7_type_name(apArg[0]));` |
|      - | 5354 | `	}` |
|     14 | 5355 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 5356 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5357 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 5358 | `			ph7_type_name(apArg[1]));` |
|      - | 5359 | `	}` |
|     11 | 5360 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5361 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5362 | `	if( nKnown != nUser ){` |
|      5 | 5363 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5364 | `		return PH7_OK;` |
|      - | 5365 | `	}` |
|      - | 5366 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5367 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5368 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5369 | `	}` |
|      7 | 5370 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5371 | `	return PH7_OK;` |
|     10 | 5372 | `}` |
|      - | 5373 | `/*` |
|      - | 5374 | ` * array hash_algos(void)` |
|      - | 5375 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5376 | ` */` |
|      2 | 5377 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5378 | `{` |
|      - | 5379 | `	ph7_value *pArray,*pValue;` |
|      - | 5380 | `	sxu32 i;` |
|      1 | 5381 | `	SXUNUSED(nArg);` |
|      1 | 5382 | `	SXUNUSED(apArg);` |
|      3 | 5383 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5384 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5385 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5386 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5387 | `		return PH7_OK;` |
|      - | 5388 | `	}` |
|     15 | 5389 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5390 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5391 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5392 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5393 | `	}` |
|      3 | 5394 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5395 | `	return PH7_OK;` |
|      2 | 5396 | `}` |
|      - | 5397 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5398 | `/*` |
|      - | 5399 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5400 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5401 | ` */` |
|      - | 5402 | `/*` |
|      - | 5403 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5404 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5405 | ` */` |
|     40 | 5406 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5407 | `{` |
|      - | 5408 | `	int iCost;` |
|     40 | 5409 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5410 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5411 | `		return FALSE;` |
|      - | 5412 | `	}` |
|     29 | 5413 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5414 | `		return FALSE;` |
|      - | 5415 | `	}` |
|     29 | 5416 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5417 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5418 | `		return FALSE;` |
|      - | 5419 | `	}` |
|     27 | 5420 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5421 | `	return TRUE;` |
|     21 | 5422 | `}` |
|      - | 5423 | `/*` |
|      - | 5424 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5425 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5426 | ` */` |
|     20 | 5427 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5428 | `{` |
|     23 | 5429 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5430 | `		return TRUE;` |
|      - | 5431 | `	}` |
|     23 | 5432 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5433 | `		int nAlgo;` |
|     23 | 5434 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5435 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5436 | `	}` |
|    ! 0 | 5437 | `	return FALSE;` |
|     13 | 5438 | `}` |
|      - | 5439 | `/*` |
|      - | 5440 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5441 | ` *  Create a bcrypt hash of the password.` |
|      - | 5442 | ` */` |
|     16 | 5443 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5444 | `{` |
|      - | 5445 | `	const char *zPwd;` |
|     19 | 5446 | `	int nPwd,iCost = 12;` |
|      - | 5447 | `	unsigned char aSalt[16];` |
|      - | 5448 | `	char zHash[60];` |
|     19 | 5449 | `	if( nArg < 2 ){` |
|    ! 0 | 5450 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5451 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5452 | `	}` |
|     19 | 5453 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5454 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5455 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5456 | `	}` |
|      - | 5457 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5458 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5459 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5460 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5461 | `	}` |
|     16 | 5462 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5463 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5464 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5465 | `	}` |
|     13 | 5466 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5467 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5468 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5469 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5470 | `	}` |
|     13 | 5471 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5472 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5473 | `		return PH7_OK;` |
|      - | 5474 | `	}` |
|     13 | 5475 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5476 | `	return PH7_OK;` |
|     11 | 5477 | `}` |
|      - | 5478 | `/*` |
|      - | 5479 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5480 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5481 | ` */` |
|     28 | 5482 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5483 | `{` |
|      - | 5484 | `	const char *zPwd,*zHash;` |
|      - | 5485 | `	int nPwd,nHash,iCost,i;` |
|      - | 5486 | `	unsigned char aSalt[16];` |
|      - | 5487 | `	char zComputed[60];` |
|     29 | 5488 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5489 | `	if( nArg < 2 ){` |
|    ! 0 | 5490 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5491 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5492 | `	}` |
|     29 | 5493 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5494 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5495 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5496 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5497 | `		return PH7_OK;` |
|      - | 5498 | `	}` |
|      - | 5499 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5500 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5501 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5502 | `		return PH7_OK;` |
|      - | 5503 | `	}` |
|     19 | 5504 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5505 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5506 | `		return PH7_OK;` |
|      - | 5507 | `	}` |
|      - | 5508 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5509 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5510 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5511 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5512 | `	}` |
|     19 | 5513 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5514 | `	return PH7_OK;` |
|     15 | 5515 | `}` |
|      - | 5516 | `/*` |
|      - | 5517 | ` * array password_get_info(string $hash)` |
|      - | 5518 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5519 | ` */` |
|      6 | 5520 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5521 | `{` |
|      7 | 5522 | `	const char *zHash = "";` |
|      7 | 5523 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5524 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5525 | `	if( nArg > 0 ){` |
|      7 | 5526 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5527 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5528 | `	}` |
|      7 | 5529 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5530 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5531 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5532 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5533 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5534 | `		return PH7_OK;` |
|      - | 5535 | `	}` |
|      7 | 5536 | `	if( bBcrypt ){` |
|      5 | 5537 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5538 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5539 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5540 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5541 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5542 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5543 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5544 | `	}else{` |
|      3 | 5545 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5546 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5547 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5548 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5549 | `	}` |
|      7 | 5550 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5551 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5552 | `	return PH7_OK;` |
|      4 | 5553 | `}` |
|      - | 5554 | `/*` |
|      - | 5555 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5556 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5557 | ` */` |
|      6 | 5558 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5559 | `{` |
|      - | 5560 | `	const char *zHash;` |
|      7 | 5561 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5562 | `	if( nArg < 2 ){` |
|    ! 0 | 5563 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5564 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5565 | `	}` |
|      7 | 5566 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5567 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5568 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5569 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5570 | `		return PH7_OK;` |
|      - | 5571 | `	}` |
|      5 | 5572 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5573 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5574 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5575 | `	}` |
|      5 | 5576 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5577 | `	return PH7_OK;` |
|      4 | 5578 | `}` |
|      - | 5579 | `/*` |
|      - | 5580 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5581 | ` *` |
|      - | 5582 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5583 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5584 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5585 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5586 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5587 | ` */` |
|      - | 5588 | `#define FV_VALIDATE_INT     257` |
|      - | 5589 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5590 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5591 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5592 | `#define FV_VALIDATE_URL     273` |
|      - | 5593 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5594 | `#define FV_VALIDATE_IP      275` |
|      - | 5595 | `#define FV_VALIDATE_MAC     276` |
|      - | 5596 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5597 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5598 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5599 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5600 | `#define FV_SANITIZE_URL     518` |
|      - | 5601 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5602 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5603 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5604 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5605 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5606 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5607 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5608 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5609 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5610 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5611 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5612 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5613 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5614 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5615 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5616 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5617 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5618 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5619 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5620 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5621 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5622 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5623 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5624 |  |
|      - | 5625 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5626 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5627 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5628 | `	const char *z = *pz;` |
|    153 | 5629 | `	int n = *pn;` |
|    157 | 5630 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5631 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5632 | `	*pz = z; *pn = n;` |
|    153 | 5633 | `}` |
|      - | 5634 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5635 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5636 | `	int neg = 0, i;` |
|     57 | 5637 | `	sxu64 u = 0;` |
|     57 | 5638 | `	FvTrim(&z,&n);` |
|     57 | 5639 | `	if( n==0 ){ return 0; }` |
|     51 | 5640 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5641 | `	if( n==0 ){ return 0; }` |
|     49 | 5642 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5643 | `		z += 2; n -= 2;` |
|      3 | 5644 | `		if( n==0 ){ return 0; }` |
|      7 | 5645 | `		for( i=0; i<n; i++ ){` |
|      5 | 5646 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5647 | `			if( h<0 ){ return 0; }` |
|      5 | 5648 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5649 | `			u = u*16 + (sxu64)h;` |
|      3 | 5650 | `		}` |
|     48 | 5651 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5652 | `		for( i=0; i<n; i++ ){` |
|      7 | 5653 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5654 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5655 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5656 | `		}` |
|      2 | 5657 | `	}else{` |
|     45 | 5658 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5659 | `		for( i=0; i<n; i++ ){` |
|    173 | 5660 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5661 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5662 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5663 | `		}` |
|      - | 5664 | `	}` |
|     33 | 5665 | `	if( neg ){` |
|      5 | 5666 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5667 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5668 | `	}else{` |
|     29 | 5669 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5670 | `		*pOut = (ph7_int64)u;` |
|      - | 5671 | `	}` |
|     31 | 5672 | `	return 1;` |
|     29 | 5673 | `}` |
|      - | 5674 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5675 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5676 | `	char zBuf[512];` |
|     69 | 5677 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5678 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5679 | `	FvTrim(&z,&n);` |
|      - | 5680 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5681 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5682 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5683 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5684 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5685 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5686 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5687 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5688 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5689 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5690 | `		intEnd = s;` |
|    167 | 5691 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5692 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5693 | `			intEnd++;` |
|      1 | 5694 | `		}` |
|     25 | 5695 | `		if( hasComma ){` |
|     25 | 5696 | `			segStart = s; segIdx = 0;` |
|    165 | 5697 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5698 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5699 | `					int segLen = i - segStart, k;` |
|     49 | 5700 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5701 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5702 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5703 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5704 | `						zBuf[m++] = z[k];` |
|     41 | 5705 | `					}` |
|     39 | 5706 | `					segStart = i+1; segIdx++;` |
|     19 | 5707 | `				}` |
|     71 | 5708 | `			}` |
|      8 | 5709 | `		}else{` |
|    ! 0 | 5710 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5711 | `		}` |
|     27 | 5712 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5713 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5714 | `			zBuf[m++] = z[i];` |
|      7 | 5715 | `		}` |
|     15 | 5716 | `		zv = zBuf; nv = m;` |
|      8 | 5717 | `	}else{` |
|     45 | 5718 | `		zv = z; nv = n;` |
|      - | 5719 | `	}` |
|     59 | 5720 | `	i = 0;` |
|     59 | 5721 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5722 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5723 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5724 | `		i++;` |
|     39 | 5725 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5726 | `	}` |
|     59 | 5727 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5728 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5729 | `		i++;` |
|     29 | 5730 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5731 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5732 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5733 | `	}` |
|     57 | 5734 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5735 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5736 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5737 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5738 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5739 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5740 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5741 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5742 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5743 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5744 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5745 | `	zBuf[nv] = 0;` |
|     53 | 5746 | `	errno = 0;` |
|     53 | 5747 | `	d = strtod(zBuf,0);` |
|     53 | 5748 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5749 | `		return 0;` |
|      - | 5750 | `	}` |
|     39 | 5751 | `	*pOut = d;` |
|     39 | 5752 | `	return 1;` |
|     35 | 5753 | `}` |
|      - | 5754 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5755 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5756 | ` * false, NOT failures. */` |
|     33 | 5757 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5758 | `	FvTrim(&z,&n);` |
|     32 | 5759 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5760 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5761 | `		*pBool = 1; return 1;` |
|      - | 5762 | `	}` |
|     22 | 5763 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5764 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5765 | `		*pBool = 0; return 1;` |
|      - | 5766 | `	}` |
|      9 | 5767 | `	return 0;` |
|     15 | 5768 | `}` |
|      - | 5769 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5770 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5771 | `	int i = 0, parts = 0;` |
|     77 | 5772 | `	while( i<n ){` |
|     65 | 5773 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5774 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5775 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5776 | `			if( val>255 ){ return 0; }` |
|     79 | 5777 | `			digits++; i++;` |
|      1 | 5778 | `		}` |
|     59 | 5779 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5780 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5781 | `		parts++;` |
|     45 | 5782 | `		if( parts>4 ){ return 0; }` |
|     45 | 5783 | `		if( i<n ){` |
|     33 | 5784 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5785 | `			i++;` |
|     33 | 5786 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5787 | `		}` |
|      1 | 5788 | `	}` |
|     13 | 5789 | `	return parts==4;` |
|     17 | 5790 | `}` |
|      - | 5791 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5792 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5793 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5794 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5795 | `	if( n==0 ){ return 0; }` |
|    145 | 5796 | `	while( i<=n ){` |
|    133 | 5797 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5798 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5799 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5800 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5801 | `			if( isV4 ){` |
|     11 | 5802 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5803 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5804 | `				groups += 2;` |
|      3 | 5805 | `			}else{` |
|     13 | 5806 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5807 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5808 | `				groups++;` |
|      - | 5809 | `			}` |
|     17 | 5810 | `			segStart = i+1;` |
|      8 | 5811 | `		}` |
|    127 | 5812 | `		i++;` |
|      1 | 5813 | `	}` |
|     13 | 5814 | `	return groups;` |
|     10 | 5815 | `}` |
|      - | 5816 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5817 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5818 | `	const char *zDbl = 0;` |
|      - | 5819 | `	int i, ga, gb;` |
|    139 | 5820 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5821 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5822 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5823 | `			zDbl = z+i;` |
|      5 | 5824 | `		}` |
|     61 | 5825 | `	}` |
|     17 | 5826 | `	if( zDbl==0 ){` |
|      9 | 5827 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5828 | `	}else{` |
|      9 | 5829 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5830 | `		int lenB = n - lenA - 2;` |
|      9 | 5831 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5832 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5833 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5834 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5835 | `	}` |
|     10 | 5836 | `}` |
|     25 | 5837 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5838 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5839 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5840 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5841 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5842 | `	return 0;` |
|     13 | 5843 | `}` |
|      - | 5844 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5845 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5846 | `	char sep;` |
|      - | 5847 | `	int i;` |
|     11 | 5848 | `	if( n!=17 ){ return 0; }` |
|      7 | 5849 | `	sep = z[2];` |
|      7 | 5850 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5851 | `	for( i=0; i<17; i++ ){` |
|    101 | 5852 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5853 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5854 | `	}` |
|      5 | 5855 | `	return 1;` |
|      6 | 5856 | `}` |
|      - | 5857 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5858 | ` * parts or IP-literal domains). */` |
|     28 | 5859 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5860 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5861 | `	const char *zDom;` |
|     28 | 5862 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5863 | `	for( i=0; i<n; i++ ){` |
|    181 | 5864 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5865 | `	}` |
|     21 | 5866 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5867 | `	localLen = at;` |
|     21 | 5868 | `	zDom = z + at + 1;` |
|     21 | 5869 | `	domLen = n - at - 1;` |
|     21 | 5870 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5871 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5872 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5873 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5874 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5875 | `	}` |
|     15 | 5876 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5877 | `	labelStart = 0;` |
|     85 | 5878 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5879 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5880 | `			int ll = i - labelStart;` |
|     25 | 5881 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5882 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5883 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5884 | `			labelStart = i+1;` |
|     12 | 5885 | `		}else{` |
|     51 | 5886 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5887 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5888 | `		}` |
|     37 | 5889 | `	}` |
|     11 | 5890 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5891 | `	return 1;` |
|     15 | 5892 | `}` |
|      - | 5893 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5894 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5895 | `	int i;` |
|     11 | 5896 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5897 | `	for( i=0; i<n; i++ ){` |
|     75 | 5898 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5899 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5900 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5901 | `	}` |
|      7 | 5902 | `	return 1;` |
|      6 | 5903 | `}` |
|      - | 5904 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5905 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5906 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5907 | `	SyhttpUri sUri;` |
|     15 | 5908 | `	if( n==0 ){ return 0; }` |
|     15 | 5909 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5910 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5911 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5912 | `}` |
|      - | 5913 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5914 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5915 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5916 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5917 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5918 | `	int i, runStart = 0;` |
|     37 | 5919 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5920 | `	for( i=0; i<n; i++ ){` |
|     91 | 5921 | `		char c = z[i];` |
|     91 | 5922 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5923 | `		if( !keep && isFloat ){` |
|     38 | 5924 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5925 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5926 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5927 | `		}` |
|     61 | 5928 | `		if( !keep ){` |
|     33 | 5929 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5930 | `			runStart = i+1;` |
|     16 | 5931 | `		}` |
|     31 | 5932 | `	}` |
|      7 | 5933 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5934 | `}` |
|      - | 5935 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5936 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5937 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5938 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5939 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5940 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5941 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5942 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5943 | `	return 0;` |
|    144 | 5944 | `}` |
|      - | 5945 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5946 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5947 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5948 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5949 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5950 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5951 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5952 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5953 | `	int i, runStart = 0;` |
|     25 | 5954 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5955 | `	for( i=0; i<n; i++ ){` |
|    179 | 5956 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5957 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5958 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5959 | `			runStart = i+1;` |
|     13 | 5960 | `			continue;` |
|      - | 5961 | `		}` |
|    167 | 5962 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5963 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5964 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5965 | `			runStart = i+1;` |
|    166 | 5966 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5967 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5968 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5969 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5970 | `			runStart = i+1;` |
|      4 | 5971 | `		}` |
|     79 | 5972 | `	}` |
|     15 | 5973 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5974 | `}` |
|      - | 5975 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 5976 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 5977 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 5978 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 5979 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 5980 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 5981 | `	int i, runStart = 0;` |
|      - | 5982 | `	const char *zEnt;` |
|     13 | 5983 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 5984 | `	for( i=0; i<n; i++ ){` |
|    119 | 5985 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 5986 | `		if( FvStripByte(c,flags) ){` |
|      9 | 5987 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5988 | `			runStart = i+1;` |
|      9 | 5989 | `			continue;` |
|      - | 5990 | `		}` |
|    111 | 5991 | `		switch( c ){` |
|      3 | 5992 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 5993 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 5994 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 5995 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 5996 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 5997 | `		default:` |
|      - | 5998 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 5999 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6000 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6001 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6002 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6003 | `				runStart = i+1;` |
|      8 | 6004 | `			}` |
|     93 | 6005 | `			continue; /* keep in the current run */` |
|      - | 6006 | `		}` |
|     19 | 6007 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6008 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6009 | `		runStart = i+1;` |
|     10 | 6010 | `	}` |
|     13 | 6011 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6012 | `}` |
|      - | 6013 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6014 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6015 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6016 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6017 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6018 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6019 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6020 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6021 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6022 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6023 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6024 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6025 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6026 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6027 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6028 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6029 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6030 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6031 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6032 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6033 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6034 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6035 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6036 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6037 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6038 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6039 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6040 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6041 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6042 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6043 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6044 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6045 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6046 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6047 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6048 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6049 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6050 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6051 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6052 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6053 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6054 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6055 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6056 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6057 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6058 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6059 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6060 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6061 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6062 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6063 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6064 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6065 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6066 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6067 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6068 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6069 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6070 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6071 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6072 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6073 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6074 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6075 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6076 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6077 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6078 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6079 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6080 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6081 | `};` |
|      - | 6082 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6083 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6084 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6085 | `	while( lo <= hi ){` |
|    309 | 6086 | `		int mid = (lo + hi) / 2;` |
|    309 | 6087 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6088 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6089 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6090 | `	}` |
|     15 | 6091 | `	return 0;` |
|     21 | 6092 | `}` |
|      - | 6093 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6094 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6095 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6096 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6097 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6098 | `	unsigned char c = p[0];` |
|    101 | 6099 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6100 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6101 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6102 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6103 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6104 | `		return 2;` |
|      - | 6105 | `	}` |
|     53 | 6106 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6107 | `		sxu32 cp;` |
|     47 | 6108 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6109 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6110 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6111 | `		*pCp = cp;` |
|     29 | 6112 | `		return 3;` |
|      - | 6113 | `	}` |
|      7 | 6114 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6115 | `		sxu32 cp;` |
|      5 | 6116 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6117 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6118 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6119 | `		*pCp = cp;` |
|      5 | 6120 | `		return 4;` |
|      - | 6121 | `	}` |
|      3 | 6122 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6123 | `}` |
|      - | 6124 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6125 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6126 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6127 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6128 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6129 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6130 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6131 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6132 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6133 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6134 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6135 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6136 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6137 | `}` |
|      - | 6138 | `/* ---------------------------------------------------------------------------` |
|      - | 6139 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6140 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6141 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6142 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6143 | ` * ------------------------------------------------------------------------ */` |
|      - | 6144 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6145 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6146 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6147 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6148 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6149 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6150 | `}` |
|      - | 6151 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6152 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6153 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6154 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6155 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6156 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6157 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6158 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6159 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6160 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6161 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6162 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6163 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6164 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6165 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6166 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6167 | `	}` |
|     71 | 6168 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6169 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6170 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6171 | `	}` |
|     71 | 6172 | `	return 1;` |
|     46 | 6173 | `}` |
|      - | 6174 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6175 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6176 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6177 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6178 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6179 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6180 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6181 | `}` |
|      - | 6182 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6183 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6184 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6185 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6186 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6187 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6188 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6189 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6190 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6191 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6192 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6193 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6194 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6195 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6196 | `	return 1;` |
|      5 | 6197 | `}` |
|      - | 6198 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6199 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6200 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6201 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6202 | ` * start a new sequence is left for the next round. */` |
|      5 | 6203 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6204 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6205 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6206 | `	unsigned char c = p[0];` |
|     15 | 6207 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6208 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6209 | `	if( c < 0xE0 ){` |
|      3 | 6210 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6211 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6212 | `	}` |
|     11 | 6213 | `	if( c < 0xF0 ){` |
|     11 | 6214 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6215 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6216 | `		}` |
|      9 | 6217 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6218 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6219 | `		return 3;` |
|      - | 6220 | `	}` |
|    ! 0 | 6221 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6222 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6223 | `	}` |
|    ! 0 | 6224 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6225 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6226 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6227 | `	return 4;` |
|      8 | 6228 | `}` |
|      - | 6229 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6230 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6231 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6232 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6233 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6234 | `};` |
|      - | 6235 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6236 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6237 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6238 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6239 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6240 | `}` |
|      - | 6241 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6242 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6243 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6244 | ` * whichever function the requested table belongs to. */` |
|     29 | 6245 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6246 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6247 | `		return "&#039;";` |
|      - | 6248 | `	}` |
|      9 | 6249 | `	return "&apos;";` |
|     15 | 6250 | `}` |
|      - | 6251 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6252 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6253 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6254 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6255 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6256 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6257 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6258 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6259 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6260 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6261 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6262 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6263 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6264 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6265 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6266 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6267 | `	sxu32 n;` |
|    173 | 6268 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6269 | `	if( z[1] == '#' ){` |
|      - | 6270 | `		/* Numeric reference */` |
|     89 | 6271 | `		sxu32 cp = 0;` |
|     89 | 6272 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6273 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6274 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6275 | `			int v;` |
|    221 | 6276 | `			unsigned char c = z[i];` |
|    221 | 6277 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6278 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6279 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6280 | `			else { return 0; }` |
|      - | 6281 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6282 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6283 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6284 | `			nDig++;` |
|    111 | 6285 | `		}` |
|     97 | 6286 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6287 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6288 | `		if( !bFull ){` |
|      - | 6289 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6290 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6291 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6292 | `		}` |
|     75 | 6293 | `		*pCp = cp;` |
|     75 | 6294 | `		*pnConsumed = i + 1;` |
|     75 | 6295 | `		return 1;` |
|      - | 6296 | `	}` |
|      - | 6297 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6298 | `	 * else can bail out before touching the tables. */` |
|     81 | 6299 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6300 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6301 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6302 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6303 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6304 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6305 | `			return 1;` |
|      - | 6306 | `		}` |
|     96 | 6307 | `	}` |
|     23 | 6308 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6309 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6310 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6311 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6312 | `		 * for ~96% of rows. */` |
|   3369 | 6313 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6314 | `			sxu32 nEnt;` |
|   3357 | 6315 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6316 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6317 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6318 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6319 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6320 | `				return 1;` |
|      - | 6321 | `			}` |
|     58 | 6322 | `		}` |
|      6 | 6323 | `	}` |
|     17 | 6324 | `	return 0;` |
|     88 | 6325 | `}` |
|      - | 6326 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6327 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6328 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6329 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6330 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 6331 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6332 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 6333 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 6334 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6335 | `	const unsigned char *runStart;` |
|     95 | 6336 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6337 | `	sxu32 cp;` |
|     95 | 6338 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6339 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6340 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6341 | `		while( p < zEnd ){` |
|      - | 6342 | `			int len;` |
|    323 | 6343 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6344 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6345 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6346 | `			p += len;` |
|      1 | 6347 | `		}` |
|     59 | 6348 | `		p = (const unsigned char *)zIn;` |
|     29 | 6349 | `	}` |
|     85 | 6350 | `	runStart = p;` |
|     85 | 6351 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 6352 | `	while( p < zEnd ){` |
|    371 | 6353 | `		const char *zEnt = 0;` |
|      - | 6354 | `		int len;` |
|    371 | 6355 | `		if( *p < 0x80 ){` |
|    307 | 6356 | `			len = 1;` |
|    307 | 6357 | `			switch( *p ){` |
|     25 | 6358 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6359 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6360 | `			case '&':` |
|     37 | 6361 | `				zEnt = "&amp;";` |
|     37 | 6362 | `				if( !bDoubleEncode ){` |
|      - | 6363 | `					sxu32 eCp; int nEat;` |
|     25 | 6364 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6365 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6366 | `						zEnt = 0;` |
|     13 | 6367 | `						len = nEat;` |
|      6 | 6368 | `					}` |
|     12 | 6369 | `				}` |
|     37 | 6370 | `				break;` |
|     10 | 6371 | `			case '"':` |
|     21 | 6372 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6373 | `				break;` |
|     12 | 6374 | `			case '\'':` |
|     25 | 6375 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6376 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6377 | `				}` |
|     25 | 6378 | `				break;` |
|     89 | 6379 | `			default:` |
|    179 | 6380 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6381 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6382 | `				}` |
|    178 | 6383 | `				break;` |
|      - | 6384 | `			}` |
|    154 | 6385 | `		}else{` |
|     65 | 6386 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6387 | `			if( len == 0 ){` |
|      - | 6388 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6389 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6390 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6391 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6392 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6393 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6394 | `				runStart = p;` |
|     15 | 6395 | `				continue;` |
|      - | 6396 | `			}` |
|     51 | 6397 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6398 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6399 | `			}` |
|     51 | 6400 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6401 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6402 | `			}` |
|      - | 6403 | `		}` |
|    357 | 6404 | `		if( zEnt ){` |
|    135 | 6405 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6406 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6407 | `			runStart = p + len;` |
|     67 | 6408 | `		}` |
|    357 | 6409 | `		p += len;` |
|      1 | 6410 | `	}` |
|     85 | 6411 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 6412 | `}` |
|      - | 6413 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6414 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6415 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6416 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6417 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 6418 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6419 | `                         int iFlags,int bFull){` |
|     83 | 6420 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 6421 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 6422 | `	const unsigned char *runStart = p;` |
|     83 | 6423 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 6424 | `	while( p < zEnd ){` |
|      - | 6425 | `		sxu32 cp;` |
|      - | 6426 | `		int nEat;` |
|    510 | 6427 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6428 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6429 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6430 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6431 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6432 | `			p += nEat;` |
|     37 | 6433 | `			continue;` |
|      - | 6434 | `		}` |
|     89 | 6435 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6436 | `		{` |
|      - | 6437 | `			char zBuf[4];` |
|     89 | 6438 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6439 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6440 | `		}` |
|     89 | 6441 | `		p += nEat;` |
|     89 | 6442 | `		runStart = p;` |
|      1 | 6443 | `	}` |
|     79 | 6444 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 6445 | `}` |
|      - | 6446 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6447 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6448 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6449 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6450 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 6451 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6452 | `	const char *zCs;` |
|      - | 6453 | `	int nCs;` |
|    148 | 6454 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6455 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6456 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6457 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6458 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6459 | `	}` |
|    ! 0 | 6460 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6461 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 6462 | `}` |
|      - | 6463 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6464 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6465 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6466 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6467 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6468 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6469 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6470 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6471 | `}` |
|     13 | 6472 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6473 | `	ph7_value *pArray,*pValue;` |
|     13 | 6474 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6475 | `	sxu32 n;` |
|     13 | 6476 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6477 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6478 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6479 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6480 | `		return;` |
|      - | 6481 | `	}` |
|     13 | 6482 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6483 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6484 | `	}` |
|     13 | 6485 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6486 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6487 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6488 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6489 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6490 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6491 | `	}` |
|     13 | 6492 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6493 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6494 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6495 | `		char zKey[8];` |
|    499 | 6496 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6497 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6498 | `			zKey[nK] = 0;` |
|    497 | 6499 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6500 | `		}` |
|      1 | 6501 | `	}` |
|     13 | 6502 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6503 | `}` |
|     25 | 6504 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6505 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6506 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6507 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6508 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6509 | `}` |
|     23 | 6510 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6511 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6512 | `}` |
|      - | 6513 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6514 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6515 | `	int i, runStart = 0;` |
|      5 | 6516 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6517 | `	for( i=0; i<n; i++ ){` |
|     47 | 6518 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6519 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6520 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6521 | `			runStart = i+1;` |
|      5 | 6522 | `		}` |
|     24 | 6523 | `	}` |
|      5 | 6524 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6525 | `}` |
|      - | 6526 | `/*` |
|      - | 6527 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6528 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6529 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6530 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6531 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6532 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6533 | ` */` |
|    316 | 6534 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6535 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6536 | `                         ph7_value *pDefault)` |
|      3 | 6537 | `{` |
|    319 | 6538 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6539 | `	const char *zVal; int nVal;` |
|      - | 6540 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6541 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6542 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6543 | `	switch( iFilter ){` |
|     28 | 6544 | `	case FV_VALIDATE_INT: {` |
|      - | 6545 | `		ph7_int64 v;` |
|     58 | 6546 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6547 | `		if( pOpts ){` |
|      7 | 6548 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6549 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6550 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6551 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6552 | `		}` |
|     29 | 6553 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6554 | `		return PH7_OK;` |
|      - | 6555 | `	}` |
|     34 | 6556 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6557 | `		double d;` |
|     69 | 6558 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6559 | `		ph7_result_double(pCtx,d);` |
|     39 | 6560 | `		return PH7_OK;` |
|      - | 6561 | `	}` |
|     14 | 6562 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6563 | `		int b;` |
|     29 | 6564 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6565 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6566 | `		return PH7_OK;` |
|      - | 6567 | `	}` |
|     25 | 6568 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6569 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6570 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6571 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6572 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6573 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6574 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6575 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6576 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6577 | `		if( pRe==0 ){` |
|      3 | 6578 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6579 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6580 | `		}` |
|      5 | 6581 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6582 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6583 | `		goto pass;` |
|      - | 6584 | `#else` |
|      - | 6585 | `		goto fail;` |
|      - | 6586 | `#endif` |
|      - | 6587 | `	}` |
|      3 | 6588 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6589 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6590 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6591 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6592 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6593 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6594 | `	case FV_DEFAULT:` |
|      - | 6595 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6596 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6597 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6598 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6599 | `			return PH7_OK;` |
|      - | 6600 | `		}` |
|     14 | 6601 | `		goto pass;` |
|    ! 0 | 6602 | `	default:` |
|    ! 0 | 6603 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6604 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6605 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6606 | `	}` |
|     58 | 6607 | `fail:` |
|    118 | 6608 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6609 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6610 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6611 | `	return PH7_OK;` |
|     26 | 6612 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6613 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6614 | `	return PH7_OK;` |
|    161 | 6615 | `}` |
|      - | 6616 | `/*` |
|      - | 6617 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6618 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6619 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6620 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6621 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6622 | ` */` |
|    328 | 6623 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6624 | `                              int *piFilter,int *piFlags,` |
|      - | 6625 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6626 | `{` |
|    331 | 6627 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6628 | `	if( nArg>iBase+1 ){` |
|     88 | 6629 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6630 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6631 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6632 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6633 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6634 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6635 | `		}else{` |
|     48 | 6636 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6637 | `		}` |
|     43 | 6638 | `	}` |
|    331 | 6639 | `}` |
|      - | 6640 | `/*` |
|      - | 6641 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6642 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6643 | ` */` |
|    306 | 6644 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6645 | `{` |
|    308 | 6646 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6647 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6648 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6649 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6650 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6651 | `}` |
|      - | 6652 | `/*` |
|      - | 6653 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6654 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6655 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6656 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6657 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6658 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6659 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6660 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6661 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6662 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6663 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6664 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6665 | ` *  php's snapshot.` |
|      - | 6666 | ` */` |
|     28 | 6667 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6668 | `{` |
|     30 | 6669 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 6670 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6671 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 6672 | `	if( nArg<2 ){` |
|      7 | 6673 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 6674 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6675 | `	}` |
|     26 | 6676 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6677 | `	switch( iType ){` |
|      3 | 6678 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6679 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6680 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6681 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6682 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6683 | `	default:` |
|      3 | 6684 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6685 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6686 | `	}` |
|     23 | 6687 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6688 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6689 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6690 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6691 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6692 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6693 | `	if( pElem==0 ){` |
|      - | 6694 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6695 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6696 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6697 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6698 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6699 | `		return PH7_OK;` |
|      - | 6700 | `	}` |
|     11 | 6701 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 6702 | `}` |
|      - | 6703 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6704 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6705 | `/*` |
|      - | 6706 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6707 |  |
|      - | 6708 | ` */` |
|      4 | 6709 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6710 | `	const char *zInput, /* Raw input */` |
|      - | 6711 | `	int nByte,  /* Input length */` |
|      - | 6712 | `	int delim,  /* Delimiter */` |
|      - | 6713 | `	int encl,   /* Enclosure */` |
|      - | 6714 | `	int escape,  /* Escape character */` |
|      - | 6715 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6716 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6717 | `	)` |
|      1 | 6718 | `{` |
|      5 | 6719 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6720 | `	const char *zIn = zInput;` |
|      - | 6721 | `	const char *zPtr;` |
|      - | 6722 | `	int isEnc;` |
|      - | 6723 | `	/* Start processing */` |
|      8 | 6724 | `	for(;;){` |
|     17 | 6725 | `		if( zIn >= zEnd ){` |
|      - | 6726 | `			/* No more input to process */` |
|      5 | 6727 | `			break;` |
|      - | 6728 | `		}` |
|     13 | 6729 | `		isEnc = 0;` |
|     13 | 6730 | `		zPtr = zIn;` |
|      - | 6731 | `		/* Find the first delimiter */` |
|     27 | 6732 | `		while( zIn < zEnd ){` |
|     23 | 6733 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6734 | `				/* Delimiter found,break imediately */` |
|      5 | 6735 | `				break;` |
|     15 | 6736 | `			}else if( zIn[0] == encl ){` |
|      - | 6737 | `				/* Inside enclosure? */` |
|    ! 0 | 6738 | `				isEnc = !isEnc;` |
|     15 | 6739 | `			}else if( zIn[0] == escape ){` |
|      - | 6740 | `				/* Escape sequence */` |
|    ! 0 | 6741 | `				zIn++;` |
|    ! 0 | 6742 | `			}` |
|      - | 6743 | `			/* Advance the cursor */` |
|     15 | 6744 | `			zIn++;` |
|      1 | 6745 | `		}` |
|     13 | 6746 | `		if( zIn > zPtr ){` |
|     13 | 6747 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6748 | `			sxi32 rc;` |
|      - | 6749 | `			/* Invoke the supllied callback */` |
|     13 | 6750 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6751 | `				zPtr++;` |
|    ! 0 | 6752 | `				nByteChunk-=2;` |
|    ! 0 | 6753 | `			}` |
|     13 | 6754 | `			if( nByteChunk > 0 ){` |
|     13 | 6755 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6756 | `				if( rc == SXERR_ABORT ){` |
|      - | 6757 | `					/* User callback request an operation abort */` |
|    ! 0 | 6758 | `					break;` |
|      - | 6759 | `				}` |
|      6 | 6760 | `			}` |
|      6 | 6761 | `		}` |
|      - | 6762 | `		/* Ignore trailing delimiter */` |
|     21 | 6763 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6764 | `			zIn++;` |
|      1 | 6765 | `		}` |
|      1 | 6766 | `	}` |
|      5 | 6767 | `	return SXRET_OK;` |
|      1 | 6768 | `}` |
|      - | 6769 | `/*` |
|      - | 6770 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6771 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6772 | ` * argument to this callback.` |
|      - | 6773 | ` */` |
|     12 | 6774 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6775 | `{` |
|     13 | 6776 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6777 | `	ph7_value sEntry;` |
|      - | 6778 | `	SyString sToken;` |
|      - | 6779 | `	/* Insert the token in the given array */` |
|     13 | 6780 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6781 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6782 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6783 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6784 | `		return SXRET_OK;` |
|      - | 6785 | `	}` |
|     13 | 6786 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6787 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6788 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6789 | `	return SXRET_OK;` |
|      7 | 6790 | `}` |
|      - | 6791 | `/*` |
|      - | 6792 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6793 | ` *  Parse a CSV string into an array.` |
|      - | 6794 | ` * Parameters` |
|      - | 6795 | ` *  $input` |
|      - | 6796 | ` *   The string to parse.` |
|      - | 6797 | ` *  $delimiter` |
|      - | 6798 | ` *   Set the field delimiter (one character only).` |
|      - | 6799 | ` *  $enclosure` |
|      - | 6800 | ` *   Set the field enclosure character (one character only).` |
|      - | 6801 | ` *  $escape` |
|      - | 6802 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6803 | ` * Return` |
|      - | 6804 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6805 | ` */` |
|      2 | 6806 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6807 | `{` |
|      - | 6808 | `	const char *zInput,*zPtr;` |
|      - | 6809 | `	ph7_value *pArray;` |
|      3 | 6810 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6811 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6812 | `	int escape = '\\';  /* Escape character */` |
|      - | 6813 | `	int nLen;` |
|      3 | 6814 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6815 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6816 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6817 | `		return PH7_OK;` |
|      - | 6818 | `	}` |
|      - | 6819 | `	/* Extract the raw input */` |
|      3 | 6820 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6821 | `	if( nArg > 1 ){` |
|      - | 6822 | `		int i;` |
|      3 | 6823 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6824 | `			/* Extract the delimiter */` |
|      3 | 6825 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6826 | `			if( i > 0 ){` |
|      3 | 6827 | `				delim = zPtr[0];` |
|      1 | 6828 | `			}` |
|      1 | 6829 | `		}` |
|      3 | 6830 | `		if( nArg > 2 ){` |
|      3 | 6831 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6832 | `				/* Extract the enclosure */` |
|      3 | 6833 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6834 | `				if( i > 0 ){` |
|      3 | 6835 | `					encl = zPtr[0];` |
|      1 | 6836 | `				}` |
|      1 | 6837 | `			}` |
|      3 | 6838 | `			if( nArg > 3 ){` |
|      3 | 6839 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6840 | `					/* Extract the escape character */` |
|      3 | 6841 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6842 | `					if( i > 0 ){` |
|      3 | 6843 | `						escape = zPtr[0];` |
|      1 | 6844 | `					}` |
|      1 | 6845 | `				}` |
|      1 | 6846 | `			}` |
|      1 | 6847 | `		}` |
|      1 | 6848 | `	}` |
|      - | 6849 | `	/* Create our array */` |
|      3 | 6850 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 6851 | `	if( pArray == 0 ){` |
|      - | 6852 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 6853 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6854 | `	}` |
|      - | 6855 | `	/* Parse the raw input */` |
|      3 | 6856 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 6857 | `	/* Return the freshly created array */` |
|      3 | 6858 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 6859 | `	return PH7_OK;` |
|      2 | 6860 | `}` |
|      - | 6861 | `/*` |
|      - | 6862 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 6863 | ` * container.` |
|      - | 6864 | ` * Refer to [strip_tags()].` |
|      - | 6865 | ` */` |
|     10 | 6866 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6867 | `{` |
|     11 | 6868 | `	const char *zEnd = &zTag[nByte];` |
|      - | 6869 | `	const char *zPtr;` |
|      - | 6870 | `	SyString sEntry;` |
|      - | 6871 | `	/* Strip tags */` |
|     10 | 6872 | `	for(;;){` |
|     45 | 6873 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 6874 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 6875 | `				zTag++;` |
|      1 | 6876 | `		}` |
|     21 | 6877 | `		if( zTag >= zEnd ){` |
|     11 | 6878 | `			break;` |
|      - | 6879 | `		}` |
|     11 | 6880 | `		zPtr = zTag;` |
|      - | 6881 | `		/* Delimit the tag */` |
|     25 | 6882 | `		while(zTag < zEnd ){` |
|     25 | 6883 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6884 | `				/* UTF-8 stream */` |
|      3 | 6885 | `				zTag++;` |
|      5 | 6886 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 6887 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 6888 | `				break;` |
|    ! 0 | 6889 | `			}else{` |
|     13 | 6890 | `				zTag++;` |
|      - | 6891 | `			}` |
|      1 | 6892 | `		}` |
|     11 | 6893 | `		if( zTag > zPtr ){` |
|      - | 6894 | `			/* Perform the insertion */` |
|     11 | 6895 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 6896 | `			SyStringFullTrim(&sEntry);` |
|     11 | 6897 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 6898 | `		}` |
|      - | 6899 | `		/* Jump the trailing '>' */` |
|     11 | 6900 | `		zTag++;` |
|      1 | 6901 | `	}` |
|     11 | 6902 | `	return SXRET_OK;` |
|      1 | 6903 | `}` |
|      - | 6904 | `/*` |
|      - | 6905 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 6906 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 6907 | ` * Refer to [strip_tags()].` |
|      - | 6908 | ` */` |
|     36 | 6909 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6910 | `{` |
|     37 | 6911 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 6912 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 6913 | `		SyString sTag;` |
|     85 | 6914 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6915 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6916 | `			zTag++;` |
|      1 | 6917 | `		}` |
|      - | 6918 | `		/* Delimit the tag */` |
|     25 | 6919 | `		zCur = zTag;` |
|     77 | 6920 | `		while(zTag < zEnd ){` |
|     77 | 6921 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6922 | `				/* UTF-8 stream */` |
|      5 | 6923 | `				zTag++;` |
|      9 | 6924 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6925 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6926 | `				break;` |
|    ! 0 | 6927 | `			}else{` |
|     49 | 6928 | `				zTag++;` |
|      - | 6929 | `			}` |
|      1 | 6930 | `		}` |
|     25 | 6931 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6932 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6933 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6934 | `		if( sTag.nByte > 0 ){` |
|      - | 6935 | `			SyString *aEntry,*pEntry;` |
|      - | 6936 | `			sxi32 rc;` |
|      - | 6937 | `			sxu32 n;` |
|      - | 6938 | `			/* Perform the lookup */` |
|     25 | 6939 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6940 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6941 | `				pEntry = &aEntry[n];` |
|      - | 6942 | `				/* Do the comparison */` |
|     25 | 6943 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6944 | `				if( !rc ){` |
|     21 | 6945 | `					return SXRET_OK;` |
|      - | 6946 | `				}` |
|      3 | 6947 | `			}` |
|      2 | 6948 | `		}` |
|      2 | 6949 | `	}` |
|      - | 6950 | `	/* No such tag */` |
|     17 | 6951 | `	return SXERR_NOTFOUND;` |
|     19 | 6952 | `}` |
|      - | 6953 | `/*` |
|      - | 6954 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6955 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6956 | ` * Refer to [strip_tags()].` |
|      - | 6957 | ` */` |
|     16 | 6958 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6959 | `{` |
|     17 | 6960 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6961 | `	const char *zPtr,*zTag;` |
|      - | 6962 | `	SySet sSet;` |
|      - | 6963 | `	/* initialize the set of allowed tags */` |
|     17 | 6964 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 6965 | `	if( nTaglen > 0 ){` |
|      - | 6966 | `		/* Set of allowed tags */` |
|     11 | 6967 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 6968 | `	}` |
|      - | 6969 | `	/* Set the empty string */` |
|     17 | 6970 | `	ph7_result_string(pCtx,"",0);` |
|      - | 6971 | `	/* Start processing */` |
|     26 | 6972 | `	for(;;){` |
|     53 | 6973 | `		if(zIn >= zEnd){` |
|      - | 6974 | `			/* No more input to process */` |
|     15 | 6975 | `			break;` |
|      - | 6976 | `		}` |
|     39 | 6977 | `		zPtr = zIn;` |
|      - | 6978 | `		/* Find a tag */` |
|    133 | 6979 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 6980 | `			zIn++;` |
|      1 | 6981 | `		}` |
|     39 | 6982 | `		if( zIn > zPtr ){` |
|      - | 6983 | `			/* Consume raw input */` |
|     21 | 6984 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 6985 | `		}` |
|      - | 6986 | `		/* Ignore trailing null bytes */` |
|     39 | 6987 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 6988 | `			zIn++;` |
|    ! 0 | 6989 | `		}` |
|     39 | 6990 | `		if(zIn >= zEnd){` |
|      - | 6991 | `			/* No more input to process */` |
|      3 | 6992 | `			break;` |
|      - | 6993 | `		}` |
|     37 | 6994 | `		if( zIn[0] == '<' ){` |
|      - | 6995 | `			sxi32 rc;` |
|     37 | 6996 | `			zTag = zIn++;` |
|      - | 6997 | `			/* Delimit the tag */` |
|    127 | 6998 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 6999 | `				zIn++;` |
|      1 | 7000 | `			}` |
|     37 | 7001 | `			if( zIn < zEnd ){` |
|     37 | 7002 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7003 | `			}` |
|      - | 7004 | `			/* Query the set */` |
|     37 | 7005 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7006 | `			if( rc == SXRET_OK ){` |
|      - | 7007 | `				/* Keep the tag */` |
|     21 | 7008 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7009 | `			}` |
|     18 | 7010 | `		}` |
|      1 | 7011 | `	}` |
|      - | 7012 | `	/* Cleanup */` |
|     17 | 7013 | `	SySetRelease(&sSet);` |
|     17 | 7014 | `	return SXRET_OK;` |
|      1 | 7015 | `}` |
|      - | 7016 | `/*` |
|      - | 7017 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7018 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7019 | ` * Parameters` |
|      - | 7020 | ` *  $str` |
|      - | 7021 | ` *  The input string.` |
|      - | 7022 | ` * $allowable_tags` |
|      - | 7023 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7024 | ` * Return` |
|      - | 7025 | ` *  Returns the stripped string.` |
|      - | 7026 | ` */` |
|     14 | 7027 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7028 | `{` |
|     15 | 7029 | `	const char *zTaglist = 0;` |
|      - | 7030 | `	const char *zString;` |
|     15 | 7031 | `	int nTaglen = 0;` |
|      - | 7032 | `	int nLen;` |
|     15 | 7033 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7034 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7035 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7036 | `		return PH7_OK;` |
|      - | 7037 | `	}` |
|      - | 7038 | `	/* Point to the raw string */` |
|     15 | 7039 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7040 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7041 | `		/* Allowed tag */` |
|     11 | 7042 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7043 | `	}` |
|      - | 7044 | `	/* Process input */` |
|     15 | 7045 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7046 | `	return PH7_OK;` |
|      8 | 7047 | `}` |
|      - | 7048 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7049 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7050 | `/*` |
|      - | 7051 | ` * string str_shuffle(string $str)` |
|      - | 7052 |  |
|      - | 7053 | ` *  Randomly shuffles a string.` |
|      - | 7054 | ` * Parameters` |
|      - | 7055 | ` *  $str` |
|      - | 7056 | ` *   The input string.` |
|      - | 7057 | ` * Return` |
|      - | 7058 | ` *  Returns the shuffled string.` |
|      - | 7059 | ` */` |
|     10 | 7060 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7061 | `{` |
|      - | 7062 | `	const char *zString;` |
|      - | 7063 | `	int nLen,i,c;` |
|      - | 7064 | `	sxu32 iR;` |
|     11 | 7065 | `	if( nArg < 1 ){` |
|      - | 7066 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7067 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7068 | `		return PH7_OK;` |
|      - | 7069 | `	}` |
|      - | 7070 | `	/* Extract the target string */` |
|     11 | 7071 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7072 | `	if( nLen < 1 ){` |
|      - | 7073 | `		/* Nothing to shuffle */` |
|      3 | 7074 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7075 | `		return PH7_OK;` |
|      - | 7076 | `	}` |
|      - | 7077 | `	/* Shuffle the string */` |
|     43 | 7078 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7079 | `		/* Generate a random number first */` |
|     35 | 7080 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7081 | `		/* Extract a random offset */` |
|     35 | 7082 | `		c = zString[iR % nLen];` |
|      - | 7083 | `		/* Append it */` |
|     35 | 7084 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7085 | `	}` |
|      9 | 7086 | `	return PH7_OK;` |
|      6 | 7087 | `}` |
|      - | 7088 | `/*` |
|      - | 7089 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7090 | ` *  Convert a string to an array.` |
|      - | 7091 | ` * Parameters` |
|      - | 7092 | ` * $string` |
|      - | 7093 | ` *  The input string.` |
|      - | 7094 | ` * $split_length` |
|      - | 7095 | ` *  Maximum length of the chunk.` |
|      - | 7096 | ` * Return` |
|      - | 7097 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7098 | ` *  except possibly the last one which may be shorter.` |
|      - | 7099 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7100 | ` *  as the first (and only) array element.` |
|      - | 7101 | ` *  An empty string returns an empty array.` |
|      - | 7102 | ` * Errors` |
|      - | 7103 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7104 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7105 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7106 | ` */` |
|     28 | 7107 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7108 | `{` |
|      - | 7109 | `	const char *zString,*zEnd;` |
|      - | 7110 | `	ph7_value *pArray,*pValue;` |
|      - | 7111 | `	int split_len;` |
|      - | 7112 | `	int nLen;` |
|     33 | 7113 | `	if( nArg < 1 ){` |
|      4 | 7114 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7115 | `			"ArgumentCountError",` |
|      - | 7116 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 7117 | `			nArg` |
|      - | 7118 | `			);` |
|      - | 7119 | `	}` |
|      - | 7120 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 7121 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 7122 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7123 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 7124 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7125 | `			"TypeError",` |
|      - | 7126 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 7127 | `			ph7_type_name(apArg[0])` |
|      - | 7128 | `			);` |
|      - | 7129 | `	}` |
|      - | 7130 | `	/* Point to the target string */` |
|     27 | 7131 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7132 | `	split_len = (int)sizeof(char);` |
|     27 | 7133 | `	if( nArg > 1 ){` |
|      - | 7134 | `		/* Split length */` |
|     17 | 7135 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7136 | `		if( split_len < 1 ){` |
|      6 | 7137 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7138 | `				"ValueError",` |
|      - | 7139 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7140 | `				);` |
|      - | 7141 | `		}` |
|     11 | 7142 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7143 | `			split_len = nLen;` |
|      1 | 7144 | `		}` |
|      5 | 7145 | `	}` |
|      - | 7146 | `	/* Create the array and the scalar value */` |
|     21 | 7147 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7148 | `	/*Chunk value */` |
|     21 | 7149 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7150 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7151 | `		/* Return FALSE */` |
|    ! 0 | 7152 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7153 | `		return PH7_OK;` |
|      - | 7154 | `	}` |
|      - | 7155 | `	/* Point to the end of the string */` |
|     21 | 7156 | `	zEnd = &zString[nLen];` |
|      - | 7157 | `	/* Perform the requested operation */` |
|     48 | 7158 | `	for(;;){` |
|      - | 7159 | `		int nMax;` |
|     59 | 7160 | `		if( zString >= zEnd ){` |
|      - | 7161 | `			/* No more input to process */` |
|     21 | 7162 | `			break;` |
|      - | 7163 | `		}` |
|     39 | 7164 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7165 | `		if( nMax < split_len ){` |
|      3 | 7166 | `			split_len = nMax;` |
|      1 | 7167 | `		}` |
|      - | 7168 | `		/* Copy the current chunk */` |
|     39 | 7169 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7170 | `		/* Insert it */` |
|     39 | 7171 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7172 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7173 | `		}` |
|      - | 7174 | `		/* reset the string cursor */` |
|     39 | 7175 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7176 | `		/* Update position */` |
|     39 | 7177 | `		zString += split_len;` |
|      1 | 7178 | `	}` |
|      - | 7179 | `	/*` |
|      - | 7180 | `	 * Return the array.` |
|      - | 7181 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7182 | `	 * upon we return from this function.` |
|      - | 7183 | `	 */` |
|     21 | 7184 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7185 | `	return PH7_OK;` |
|     19 | 7186 | `}` |
|      - | 7187 | `/*` |
|      - | 7188 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7189 | ` * Refer to [strspn()].` |
|      - | 7190 | ` */` |
|     28 | 7191 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7192 | `{` |
|     29 | 7193 | `	const char *zIn = *pzIn;` |
|      - | 7194 | `	const char *zPtr;` |
|      - | 7195 | `	/* Ignore leading white spaces */` |
|     29 | 7196 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7197 | `		zIn++;` |
|    ! 0 | 7198 | `	}` |
|     29 | 7199 | `	if( zIn >= zEnd ){` |
|      - | 7200 | `		/* End of input */` |
|    ! 0 | 7201 | `		return SXERR_EOF;` |
|      - | 7202 | `	}` |
|     29 | 7203 | `	zPtr = zIn;` |
|      - | 7204 | `	/* Extract the token */` |
|    201 | 7205 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7206 | `		zIn++;` |
|      1 | 7207 | `	}` |
|     29 | 7208 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7209 | `	/* Synchronize pointers */` |
|     29 | 7210 | `	*pzIn = zIn;` |
|      - | 7211 | `	/* Return to the caller */` |
|     29 | 7212 | `	return SXRET_OK;` |
|     15 | 7213 | `}` |
|      - | 7214 | `/*` |
|      - | 7215 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7216 | ` * return the longest match.` |
|      - | 7217 | ` * Refer to [strspn()].` |
|      - | 7218 | ` */` |
|     18 | 7219 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7220 | `{` |
|     19 | 7221 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7222 | `	const char *zIn = zString;` |
|      - | 7223 | `	int i,c;` |
|     45 | 7224 | `	for(;;){` |
|     91 | 7225 | `		if( zString >= zEnd ){` |
|      7 | 7226 | `			break;` |
|      - | 7227 | `		}` |
|      - | 7228 | `		/* Extract current character */` |
|     85 | 7229 | `		c = zString[0];` |
|      - | 7230 | `		/* Perform the lookup */` |
|    383 | 7231 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7232 | `			if( c == zMask[i] ){` |
|      - | 7233 | `				/* Character found */` |
|     73 | 7234 | `				break;` |
|      - | 7235 | `			}` |
|    150 | 7236 | `		}` |
|     85 | 7237 | `		if( i >= nMaskLen ){` |
|      - | 7238 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7239 | `			break;` |
|      - | 7240 | `		}` |
|      - | 7241 | `		/* Advance cursor */` |
|     73 | 7242 | `		zString++;` |
|      1 | 7243 | `	}` |
|      - | 7244 | `	/* Longest match */` |
|     19 | 7245 | `	return (int)(zString-zIn);` |
|      1 | 7246 | `}` |
|      - | 7247 | `/*` |
|      - | 7248 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7249 | ` * Refer to [strcspn()].` |
|      - | 7250 | ` */` |
|     10 | 7251 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7252 | `{` |
|     11 | 7253 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7254 | `	const char *zIn = zString;` |
|      - | 7255 | `	int i,c;` |
|     12 | 7256 | `	for(;;){` |
|     25 | 7257 | `		if( zString >= zEnd ){` |
|      3 | 7258 | `			break;` |
|      - | 7259 | `		}` |
|      - | 7260 | `		/* Extract current character */` |
|     23 | 7261 | `		c = zString[0];` |
|      - | 7262 | `		/* Perform the lookup */` |
|     51 | 7263 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7264 | `			if( c == zMask[i] ){` |
|      9 | 7265 | `				break;` |
|      - | 7266 | `			}` |
|     15 | 7267 | `		}` |
|     23 | 7268 | `		if( i < nMaskLen ){` |
|      - | 7269 | `			/* Character in the current mask,break immediately */` |
|      9 | 7270 | `			break;` |
|      - | 7271 | `		}` |
|      - | 7272 | `		/* Advance cursor */` |
|     15 | 7273 | `		zString++;` |
|      1 | 7274 | `	}` |
|      - | 7275 | `	/* Longest match */` |
|     11 | 7276 | `	return (int)(zString-zIn);` |
|      1 | 7277 | `}` |
|      - | 7278 | `/*` |
|      - | 7279 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7280 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7281 | ` *  of characters contained within a given mask.` |
|      - | 7282 | ` * Parameters` |
|      - | 7283 | ` * $str` |
|      - | 7284 | ` *  The input string.` |
|      - | 7285 | ` * $mask` |
|      - | 7286 | ` *  The list of allowable characters.` |
|      - | 7287 | ` * $start` |
|      - | 7288 | ` *  The position in subject to start searching.` |
|      - | 7289 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7290 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7291 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7292 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7293 | ` *  start'th position from the end of subject.` |
|      - | 7294 | ` * $length` |
|      - | 7295 | ` *  The length of the segment from subject to examine.` |
|      - | 7296 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7297 | ` *  characters after the starting position.` |
|      - | 7298 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7299 | ` *  position up to length characters from the end of subject.` |
|      - | 7300 | ` * Return` |
|      - | 7301 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7302 | ` * in mask.` |
|      - | 7303 | ` */` |
|     24 | 7304 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7305 | `{` |
|      - | 7306 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7307 | `	int iMasklen,iLen;` |
|      - | 7308 | `	SyString sToken;` |
|     25 | 7309 | `	int iCount = 0;` |
|      - | 7310 | `	int rc;` |
|     25 | 7311 | `	if( nArg < 2 ){` |
|      - | 7312 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7313 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7314 | `		return PH7_OK;` |
|      - | 7315 | `	}` |
|      - | 7316 | `	/* Extract the target string */` |
|     25 | 7317 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7318 | `	/* Extract the mask */` |
|     25 | 7319 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7320 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7321 | `		/* Nothing to process,return zero */` |
|      7 | 7322 | `		ph7_result_int(pCtx,0);` |
|      7 | 7323 | `		return PH7_OK;` |
|      - | 7324 | `	}` |
|     19 | 7325 | `	if( nArg > 2 ){` |
|      - | 7326 | `		int nOfft;` |
|      - | 7327 | `		/* Extract the offset */` |
|      9 | 7328 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7329 | `		if( nOfft < 0 ){` |
|    ! 0 | 7330 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7331 | `			if( zBase > zString ){` |
|    ! 0 | 7332 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7333 | `				zString = zBase;` |
|    ! 0 | 7334 | `			}else{` |
|      - | 7335 | `				/* Invalid offset */` |
|    ! 0 | 7336 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7337 | `				return PH7_OK;` |
|      - | 7338 | `			}` |
|    ! 0 | 7339 | `		}else{` |
|      9 | 7340 | `			if( nOfft >= iLen ){` |
|      - | 7341 | `				/* Invalid offset */` |
|    ! 0 | 7342 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7343 | `				return PH7_OK;` |
|    ! 0 | 7344 | `			}else{` |
|      - | 7345 | `				/* Update offset */` |
|      9 | 7346 | `				zString += nOfft;` |
|      9 | 7347 | `				iLen -= nOfft;` |
|      - | 7348 | `			}` |
|      - | 7349 | `		}` |
|      9 | 7350 | `		if( nArg > 3 ){` |
|      - | 7351 | `			int iUserlen;` |
|      - | 7352 | `			/* Extract the desired length */` |
|      9 | 7353 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7354 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7355 | `				iLen = iUserlen;` |
|      2 | 7356 | `			}` |
|      4 | 7357 | `		}` |
|      4 | 7358 | `	}` |
|      - | 7359 | `	/* Point to the end of the string */` |
|     19 | 7360 | `	zEnd = &zString[iLen];` |
|      - | 7361 | `	/* Extract the first non-space token */` |
|     19 | 7362 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7363 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7364 | `		/* Compare against the current mask */` |
|     19 | 7365 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7366 | `	}` |
|      - | 7367 | `	/* Longest match */` |
|     19 | 7368 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7369 | `	return PH7_OK;` |
|     13 | 7370 | `}` |
|      - | 7371 | `/*` |
|      - | 7372 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7373 | ` *  Find length of initial segment not matching mask.` |
|      - | 7374 | ` * Parameters` |
|      - | 7375 | ` * $str` |
|      - | 7376 | ` *  The input string.` |
|      - | 7377 | ` * $mask` |
|      - | 7378 | ` *  The list of not allowed characters.` |
|      - | 7379 | ` * $start` |
|      - | 7380 | ` *  The position in subject to start searching.` |
|      - | 7381 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7382 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7383 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7384 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7385 | ` *  start'th position from the end of subject.` |
|      - | 7386 | ` * $length` |
|      - | 7387 | ` *  The length of the segment from subject to examine.` |
|      - | 7388 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7389 | ` *  characters after the starting position.` |
|      - | 7390 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7391 | ` *  position up to length characters from the end of subject.` |
|      - | 7392 | ` * Return` |
|      - | 7393 | ` *  Returns the length of the segment as an integer.` |
|      - | 7394 | ` */` |
|     14 | 7395 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7396 | `{` |
|      - | 7397 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7398 | `	int iMasklen,iLen;` |
|      - | 7399 | `	SyString sToken;` |
|     15 | 7400 | `	int iCount = 0;` |
|      - | 7401 | `	int rc;` |
|     15 | 7402 | `	if( nArg < 2 ){` |
|      - | 7403 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7404 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7405 | `		return PH7_OK;` |
|      - | 7406 | `	}` |
|      - | 7407 | `	/* Extract the target string */` |
|     15 | 7408 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7409 | `	/* Extract the mask */` |
|     15 | 7410 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7411 | `	if( iLen < 1 ){` |
|      - | 7412 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7413 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7414 | `		return PH7_OK;` |
|      - | 7415 | `	}` |
|     15 | 7416 | `	if( iMasklen < 1 ){` |
|      - | 7417 | `		/* No given mask,return the string length */` |
|      3 | 7418 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7419 | `		return PH7_OK;` |
|      - | 7420 | `	}` |
|     13 | 7421 | `	if( nArg > 2 ){` |
|      - | 7422 | `		int nOfft;` |
|      - | 7423 | `		/* Extract the offset */` |
|     11 | 7424 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7425 | `		if( nOfft < 0 ){` |
|    ! 0 | 7426 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7427 | `			if( zBase > zString ){` |
|    ! 0 | 7428 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7429 | `				zString = zBase;` |
|    ! 0 | 7430 | `			}else{` |
|      - | 7431 | `				/* Invalid offset */` |
|    ! 0 | 7432 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7433 | `				return PH7_OK;` |
|      - | 7434 | `			}` |
|    ! 0 | 7435 | `		}else{` |
|     11 | 7436 | `			if( nOfft >= iLen ){` |
|      - | 7437 | `				/* Invalid offset */` |
|      3 | 7438 | `				ph7_result_int(pCtx,0);` |
|      3 | 7439 | `				return PH7_OK;` |
|    ! 0 | 7440 | `			}else{` |
|      - | 7441 | `				/* Update offset */` |
|      9 | 7442 | `				zString += nOfft;` |
|      9 | 7443 | `				iLen -= nOfft;` |
|      - | 7444 | `			}` |
|      - | 7445 | `		}` |
|      9 | 7446 | `		if( nArg > 3 ){` |
|      - | 7447 | `			int iUserlen;` |
|      - | 7448 | `			/* Extract the desired length */` |
|    ! 0 | 7449 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7450 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7451 | `				iLen = iUserlen;` |
|    ! 0 | 7452 | `			}` |
|    ! 0 | 7453 | `		}` |
|      4 | 7454 | `	}` |
|      - | 7455 | `	/* Point to the end of the string */` |
|     11 | 7456 | `	zEnd = &zString[iLen];` |
|      - | 7457 | `	/* Extract the first non-space token */` |
|     11 | 7458 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7459 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7460 | `		/* Compare against the current mask */` |
|     11 | 7461 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7462 | `	}` |
|      - | 7463 | `	/* Longest match */` |
|     11 | 7464 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7465 | `	return PH7_OK;` |
|      8 | 7466 | `}` |
|      - | 7467 | `/*` |
|      - | 7468 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7469 | ` *  Search a string for any of a set of characters.` |
|      - | 7470 | ` * Parameters` |
|      - | 7471 | ` *  $haystack` |
|      - | 7472 | ` *   The string where char_list is looked for.` |
|      - | 7473 | ` *  $char_list` |
|      - | 7474 | ` *   This parameter is case sensitive.` |
|      - | 7475 | ` * Return` |
|      - | 7476 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7477 | ` */` |
|      4 | 7478 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7479 | `{` |
|      - | 7480 | `	const char *zString,*zList,*zEnd;` |
|      - | 7481 | `	int iLen,iListLen,i,c;` |
|      - | 7482 | `	sxu32 nOfft,nMax;` |
|      - | 7483 | `	sxi32 rc;` |
|      5 | 7484 | `	if( nArg < 2 ){` |
|      - | 7485 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7486 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7487 | `		return PH7_OK;` |
|      - | 7488 | `	}` |
|      - | 7489 | `	/* Extract the haystack and the char list */` |
|      5 | 7490 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7491 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7492 | `	if( iLen < 1 ){` |
|      - | 7493 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7494 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7495 | `		return PH7_OK;` |
|      - | 7496 | `	}` |
|      - | 7497 | `	/* Point to the end of the string */` |
|      5 | 7498 | `	zEnd = &zString[iLen];` |
|      5 | 7499 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7500 | `	/* perform the requested operation */` |
|     15 | 7501 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7502 | `		c = zList[i];` |
|     11 | 7503 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7504 | `		if( rc == SXRET_OK ){` |
|      5 | 7505 | `			if( nMax < nOfft ){` |
|      3 | 7506 | `				nOfft = nMax;` |
|      1 | 7507 | `			}` |
|      2 | 7508 | `		}` |
|      6 | 7509 | `	}` |
|      5 | 7510 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7511 | `		/* No such substring,return FALSE */` |
|      3 | 7512 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7513 | `	}else{` |
|      - | 7514 | `		/* Return the substring */` |
|      3 | 7515 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7516 | `	}` |
|      5 | 7517 | `	return PH7_OK;` |
|      3 | 7518 | `}` |
|      - | 7519 | `/* SPDX-SnippetBegin */` |
|      - | 7520 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7521 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7522 | `/*` |
|      - | 7523 | ` * string soundex(string $str)` |
|      - | 7524 | ` *  Calculate the soundex key of a string.` |
|      - | 7525 | ` * Parameters` |
|      - | 7526 | ` *  $str` |
|      - | 7527 | ` *   The input string.` |
|      - | 7528 | ` * Return` |
|      - | 7529 | ` *  Returns the soundex key as a string.` |
|      - | 7530 | ` * Note:` |
|      - | 7531 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7532 | ` * source tree.` |
|      - | 7533 | ` */` |
|     22 | 7534 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7535 | `{` |
|      - | 7536 | `	const unsigned char *zIn;` |
|      - | 7537 | `	char zResult[8];` |
|      - | 7538 | `	int i, j;` |
|      - | 7539 | `	static const unsigned char iCode[] = {` |
|      - | 7540 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7541 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7542 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7543 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7544 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7545 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7546 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7547 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7548 | `	};` |
|     23 | 7549 | `	if( nArg < 1 ){` |
|      - | 7550 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7551 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7552 | `		return PH7_OK;` |
|      - | 7553 | `	}` |
|     23 | 7554 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7555 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7556 | `	if( zIn[i] ){` |
|     17 | 7557 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7558 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7559 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7560 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7561 | `			if( code>0 ){` |
|     45 | 7562 | `				if( code!=prevcode ){` |
|     33 | 7563 | `					prevcode = (unsigned char)code;` |
|     33 | 7564 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7565 | `				}` |
|     23 | 7566 | `			}else{` |
|     49 | 7567 | `				prevcode = 0;` |
|      - | 7568 | `			}` |
|     47 | 7569 | `		}` |
|     33 | 7570 | `		while( j<4 ){` |
|     17 | 7571 | `			zResult[j++] = '0';` |
|      1 | 7572 | `		}` |
|     17 | 7573 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7574 | `	}else{` |
|      - | 7575 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7576 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7577 | `	}` |
|     23 | 7578 | `	return PH7_OK;` |
|     12 | 7579 | `}` |
|      - | 7580 | `/* SPDX-SnippetEnd */` |
|      - | 7581 | `/*` |
|      - | 7582 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7583 | ` *  Wraps a string to a given number of characters.` |
|      - | 7584 | ` * Parameters` |
|      - | 7585 | ` *  $str` |
|      - | 7586 | ` *   The input string.` |
|      - | 7587 | ` * $width` |
|      - | 7588 | ` *  The column width.` |
|      - | 7589 | ` * $break` |
|      - | 7590 | ` *  The line is broken using the optional break parameter.` |
|      - | 7591 | ` * Return` |
|      - | 7592 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7593 | ` */` |
|     26 | 7594 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7595 | `{` |
|      - | 7596 | `	const char *zIn,*zBreak;` |
|      - | 7597 | `	SyBlob sWorker;` |
|      - | 7598 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7599 | `	sxi32 rc;` |
|     27 | 7600 | `	if( nArg < 1 ){` |
|      - | 7601 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7602 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7603 | `		return PH7_OK;` |
|      - | 7604 | `	}` |
|      - | 7605 | `	/* Extract the input string */` |
|     27 | 7606 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7607 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7608 | `	iWidth = 75;` |
|     27 | 7609 | `	if( nArg > 1 ){` |
|     27 | 7610 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7611 | `	}` |
|      - | 7612 | `	/* Break string (default "\n"). */` |
|     27 | 7613 | `	zBreak = "\n";` |
|     27 | 7614 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7615 | `	if( nArg > 2 ){` |
|     13 | 7616 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7617 | `	}` |
|      - | 7618 | `	/* Cut long words? (default false). */` |
|     27 | 7619 | `	iCut = 0;` |
|     27 | 7620 | `	if( nArg > 3 ){` |
|      7 | 7621 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7622 | `	}` |
|     27 | 7623 | `	if( iLen < 1 ){` |
|      - | 7624 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7625 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7626 | `		return PH7_OK;` |
|      - | 7627 | `	}` |
|      - | 7628 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7629 | `	if( iBreaklen < 1 ){` |
|      3 | 7630 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7631 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7632 | `	}` |
|     21 | 7633 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7634 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7635 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7636 | `	}` |
|      - | 7637 | `	/*` |
|      - | 7638 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7639 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7640 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7641 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7642 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7643 | `	 */` |
|     19 | 7644 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7645 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7646 | `	rc = SXRET_OK;` |
|    551 | 7647 | `	while( iCur < iLen ){` |
|    533 | 7648 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7649 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7650 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7651 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7652 | `			iCur += iBreaklen;` |
|    ! 0 | 7653 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7654 | `			continue;` |
|    533 | 7655 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7656 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7657 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7658 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7659 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7660 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7661 | `				iStart = iCur + 1;` |
|      6 | 7662 | `			}` |
|     67 | 7663 | `			iSpace = iCur;` |
|    500 | 7664 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7665 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7666 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7667 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7668 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7669 | `			iStart = iSpace = iCur;` |
|    464 | 7670 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7671 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7672 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7673 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7674 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7675 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7676 | `		}` |
|    533 | 7677 | `		iCur++;` |
|      1 | 7678 | `	}` |
|      - | 7679 | `	/* Emit the trailing chunk. */` |
|     19 | 7680 | `	if( iStart < iCur ){` |
|     19 | 7681 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7682 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7683 | `	}` |
|     19 | 7684 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7685 | `	SyBlobRelease(&sWorker);` |
|     19 | 7686 | `	return PH7_OK;` |
|    ! 0 | 7687 | `oom:` |
|    ! 0 | 7688 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7689 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7690 | `}` |
|      - | 7691 | `/*` |
|      - | 7692 | ` * Check if the given character is a member of the given mask.` |
|      - | 7693 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7694 | ` * Refer to [strtok()].` |
|      - | 7695 | ` */` |
|     30 | 7696 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7697 | `{` |
|      - | 7698 | `	int i;` |
|     57 | 7699 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7700 | `		if( c == zMask[i] ){` |
|     13 | 7701 | `			if( pOfft ){` |
|      5 | 7702 | `				*pOfft = i;` |
|      2 | 7703 | `			}` |
|     13 | 7704 | `			return TRUE;` |
|      - | 7705 | `		}` |
|     14 | 7706 | `	}` |
|     19 | 7707 | `	return FALSE;` |
|     16 | 7708 | `}` |
|      - | 7709 | `/*` |
|      - | 7710 | ` * Extract a single token from the input stream.` |
|      - | 7711 | ` * Refer to [strtok()].` |
|      - | 7712 | ` */` |
|      6 | 7713 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7714 | `{` |
|      7 | 7715 | `	const char *zIn = *pzIn;` |
|      - | 7716 | `	const char *zPtr;` |
|      - | 7717 | `	/* Ignore leading delimiter */` |
|     11 | 7718 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7719 | `		zIn++;` |
|      1 | 7720 | `	}` |
|      7 | 7721 | `	if( zIn >= zEnd ){` |
|      - | 7722 | `		/* End of input */` |
|    ! 0 | 7723 | `		return SXERR_EOF;` |
|      - | 7724 | `	}` |
|      7 | 7725 | `	zPtr = zIn;` |
|      - | 7726 | `	/* Extract the token */` |
|     13 | 7727 | `	while( zIn < zEnd ){` |
|     11 | 7728 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7729 | `			/* UTF-8 stream */` |
|    ! 0 | 7730 | `			zIn++;` |
|    ! 0 | 7731 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7732 | `		}else{` |
|     11 | 7733 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7734 | `				break;` |
|      - | 7735 | `			}` |
|      7 | 7736 | `			zIn++;` |
|      - | 7737 | `		}` |
|      1 | 7738 | `	}` |
|      7 | 7739 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7740 | `	/* Update the cursor */` |
|      7 | 7741 | `	*pzIn = zIn;` |
|      - | 7742 | `	/* Return to the caller */` |
|      7 | 7743 | `	return SXRET_OK;` |
|      4 | 7744 | `}` |
|      - | 7745 | `/* strtok auxiliary private data */` |
|      - | 7746 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7747 | `struct strtok_aux_data` |
|      - | 7748 | `{` |
|      - | 7749 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7750 | `	const char *zIn;   /* Current input stream */` |
|      - | 7751 | `	const char *zEnd;  /* End of input */` |
|      - | 7752 | `};` |
|      - | 7753 | `/*` |
|      - | 7754 | ` * string strtok(string $str,string $token)` |
|      - | 7755 | ` * string strtok(string $token)` |
|      - | 7756 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7757 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7758 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7759 | ` *  words by using the space character as the token.` |
|      - | 7760 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7761 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7762 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7763 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7764 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7765 | ` *  the argument are found.` |
|      - | 7766 | ` * Parameters` |
|      - | 7767 | ` *  $str` |
|      - | 7768 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7769 | ` * $token` |
|      - | 7770 | ` *  The delimiter used when splitting up str.` |
|      - | 7771 | ` * Return` |
|      - | 7772 | ` *   Current token or FALSE on EOF.` |
|      - | 7773 | ` */` |
|      6 | 7774 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7775 | `{` |
|      - | 7776 | `	strtok_aux_data *pAux;` |
|      - | 7777 | `	const char *zMask;` |
|      - | 7778 | `	SyString sToken;` |
|      - | 7779 | `	int nMasklen;` |
|      - | 7780 | `	sxi32 rc;` |
|      7 | 7781 | `	if( nArg < 2 ){` |
|      - | 7782 | `		/* Extract top aux data */` |
|      5 | 7783 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7784 | `		if( pAux == 0 ){` |
|      - | 7785 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7786 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7787 | `			return PH7_OK;` |
|      - | 7788 | `		}` |
|      5 | 7789 | `		nMasklen = 0;` |
|      5 | 7790 | `		zMask = ""; /* cc warning */` |
|      5 | 7791 | `		if( nArg > 0 ){` |
|      - | 7792 | `			/* Extract the mask */` |
|      5 | 7793 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7794 | `		}` |
|      5 | 7795 | `		if( nMasklen < 1 ){` |
|      - | 7796 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7797 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7798 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7799 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7800 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7801 | `			return PH7_OK;` |
|      - | 7802 | `		}` |
|      - | 7803 | `		/* Extract the token */` |
|      5 | 7804 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7805 | `		if( rc != SXRET_OK ){` |
|      - | 7806 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7807 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7808 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7809 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7810 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7811 | `		}else{` |
|      - | 7812 | `			/* Return the extracted token */` |
|      5 | 7813 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7814 | `		}` |
|      3 | 7815 | `	}else{` |
|      - | 7816 | `		const char *zInput,*zCur;` |
|      - | 7817 | `		char *zDup;` |
|      - | 7818 | `		int nLen;` |
|      - | 7819 | `		/* Extract the raw input */` |
|      3 | 7820 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7821 | `		if( nLen < 1 ){` |
|      - | 7822 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7823 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7824 | `			return PH7_OK;` |
|      - | 7825 | `		}` |
|      - | 7826 | `		/* Extract the mask */` |
|      3 | 7827 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7828 | `		if( nMasklen < 1 ){` |
|      - | 7829 | `			/* Set a default mask */` |
|      - | 7830 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7831 | `			zMask = TOK_MASK;` |
|    ! 0 | 7832 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7833 | `#undef TOK_MASK` |
|    ! 0 | 7834 | `		}` |
|      - | 7835 | `		/* Extract a single token */` |
|      3 | 7836 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7837 | `		if( rc != SXRET_OK ){` |
|      - | 7838 | `			/* Empty input */` |
|    ! 0 | 7839 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7840 | `			return PH7_OK;` |
|    ! 0 | 7841 | `		}else{` |
|      - | 7842 | `			/* Return the extracted token */` |
|      3 | 7843 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7844 | `		}` |
|      - | 7845 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 7846 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 7847 | `		if( pAux ){` |
|      3 | 7848 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 7849 | `			if( nLen < 1 ){` |
|    ! 0 | 7850 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7851 | `				return PH7_OK;` |
|      - | 7852 | `			}` |
|      - | 7853 | `			/* Duplicate input */` |
|      3 | 7854 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 7855 | `			if( zDup  ){` |
|      3 | 7856 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 7857 | `				/* Register the aux data */` |
|      3 | 7858 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 7859 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 7860 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 7861 | `			}` |
|      1 | 7862 | `		}` |
|      - | 7863 | `	}` |
|      7 | 7864 | `	return PH7_OK;` |
|      4 | 7865 | `}` |
|      - | 7866 | `/*` |
|      - | 7867 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 7868 | ` *  Pad a string to a certain length with another string` |
|      - | 7869 | ` * Parameters` |
|      - | 7870 | ` *  $input` |
|      - | 7871 | ` *   The input string.` |
|      - | 7872 | ` * $pad_length` |
|      - | 7873 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 7874 | ` *   string, no padding takes place.` |
|      - | 7875 | ` * $pad_string` |
|      - | 7876 | ` *   Note:` |
|      - | 7877 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 7878 | ` *    divided by the pad_string's length.` |
|      - | 7879 | ` * $pad_type` |
|      - | 7880 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 7881 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 7882 | ` * Return` |
|      - | 7883 | ` *  The padded string.` |
|      - | 7884 | ` */` |
|     10 | 7885 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7886 | `{` |
|      - | 7887 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 7888 | `	const char *zIn,*zPad;` |
|     11 | 7889 | `	if( nArg < 2 ){` |
|      - | 7890 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7891 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7892 | `		return PH7_OK;` |
|      - | 7893 | `	}` |
|      - | 7894 | `	/* Extract the target string */` |
|     11 | 7895 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7896 | `	/* Padding length */` |
|     11 | 7897 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|     11 | 7898 | `	if( iPadlen > 0 ){` |
|      9 | 7899 | `		iPadlen -= iLen;` |
|      4 | 7900 | `	}` |
|     11 | 7901 | `	if( iPadlen < 1  ){` |
|      - | 7902 | `		/* Return the string verbatim */` |
|      5 | 7903 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 7904 | `		return PH7_OK;` |
|      - | 7905 | `	}` |
|      7 | 7906 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 7907 | `	iStrpad = (int)sizeof(char);` |
|      7 | 7908 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 7909 | `	if( nArg > 2 ){` |
|      - | 7910 | `		/* Padding string */` |
|      7 | 7911 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 7912 | `		if( iStrpad < 1 ){` |
|      - | 7913 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7914 | `			 * (only reached once padding is actually required). */` |
|      3 | 7915 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7916 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7917 | `		}` |
|      5 | 7918 | `		if( nArg > 3 ){` |
|      - | 7919 | `			/* Padd type */` |
|      5 | 7920 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7921 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7922 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7923 | `			}` |
|      2 | 7924 | `		}` |
|      2 | 7925 | `	}` |
|      5 | 7926 | `	iDiv = 1;` |
|      5 | 7927 | `	if( iType == 2 ){` |
|    ! 0 | 7928 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7929 | `	}` |
|      - | 7930 | `	/* Perform the requested operation */` |
|      5 | 7931 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7932 | `		jPad = iStrpad;` |
|      5 | 7933 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7934 | `			/* Padding */` |
|      5 | 7935 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7936 | `				break;` |
|      - | 7937 | `			}` |
|      3 | 7938 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7939 | `		}` |
|      3 | 7940 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7941 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7942 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7943 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7944 | `					jPad = iStrpad;` |
|    ! 0 | 7945 | `				}` |
|      3 | 7946 | `				if( jPad < 1){` |
|    ! 0 | 7947 | `					break;` |
|      - | 7948 | `				}` |
|      3 | 7949 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7950 | `			}` |
|      1 | 7951 | `		}` |
|      1 | 7952 | `	}` |
|      5 | 7953 | `	if( iLen > 0 ){` |
|      - | 7954 | `		/* Append the input string */` |
|      5 | 7955 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7956 | `	}` |
|      5 | 7957 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 7958 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 7959 | `			/* Padding */` |
|      5 | 7960 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 7961 | `				break;` |
|      - | 7962 | `			}` |
|      3 | 7963 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7964 | `		}` |
|      5 | 7965 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 7966 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 7967 | `			if( jPad > iStrpad ){` |
|    ! 0 | 7968 | `				jPad = iStrpad;` |
|    ! 0 | 7969 | `			}` |
|      3 | 7970 | `			if( jPad < 1){` |
|    ! 0 | 7971 | `				break;` |
|      - | 7972 | `			}` |
|      3 | 7973 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7974 | `		}` |
|      1 | 7975 | `	}` |
|      5 | 7976 | `	return PH7_OK;` |
|      6 | 7977 | `}` |
|      - | 7978 | `/*` |
|      - | 7979 | ` * String replacement private data.` |
|      - | 7980 | ` */` |
|      - | 7981 | `typedef struct str_replace_data str_replace_data;` |
|      - | 7982 | `struct str_replace_data` |
|      - | 7983 | `{` |
|      - | 7984 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 7985 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 7986 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 7987 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 7988 | `};` |
|      - | 7989 | `/*` |
|      - | 7990 | ` * Remove a substring.` |
|      - | 7991 | ` */` |
|      - | 7992 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 7993 | `	for(;;){\` |
|      - | 7994 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 7995 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 7996 | `		++OFFT;\` |
|      - | 7997 | `	}\` |
|      - | 7998 | `}` |
|      - | 7999 | `/*` |
|      - | 8000 | ` * Shift right and insert algorithm.` |
|      - | 8001 | ` */` |
|      - | 8002 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8003 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8004 | `		for(;;){\` |
|      - | 8005 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8006 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8007 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8008 | `			--INLEN; \` |
|      - | 8009 | `		}\` |
|      - | 8010 | `		for(;;){\` |
|      - | 8011 | `				if(ELEN < 1) { break; }\` |
|      - | 8012 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8013 | `				OFFT++;\` |
|      - | 8014 | `				ENTRY++;\` |
|      - | 8015 | `				--ELEN;\` |
|      - | 8016 | `		}\` |
|      - | 8017 | `}` |
|      - | 8018 | `/*` |
|      - | 8019 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8020 | ` * replacement string [i.e: zReplace].` |
|      - | 8021 | ` */` |
|     54 | 8022 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8023 | `{` |
|     59 | 8024 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8025 | `	sxu32 n,m;` |
|     59 | 8026 | `	n = SyBlobLength(pWorker);` |
|     59 | 8027 | `	m = nOfft;` |
|      - | 8028 | `	/* Delete the old entry */` |
|   6689 | 8029 | `	STRDEL(zInput,n,m,nLen);` |
|     59 | 8030 | `	SyBlobLength(pWorker) -= nLen;` |
|     59 | 8031 | `	if( nReplen > 0 ){` |
|     53 | 8032 | `		sxi32 iRep = nReplen;` |
|      - | 8033 | `		sxi32 rc;` |
|      - | 8034 | `		/*` |
|      - | 8035 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8036 | `		 * string.` |
|      - | 8037 | `		 */` |
|     53 | 8038 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     53 | 8039 | `		if( rc != SXRET_OK ){` |
|      - | 8040 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8041 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8042 | `			return rc;` |
|      - | 8043 | `		}` |
|      - | 8044 | `		/* Perform the insertion now */` |
|     53 | 8045 | `		zInput = (char *)SyBlobData(pWorker);` |
|     53 | 8046 | `		n = SyBlobLength(pWorker);` |
|   6481 | 8047 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     53 | 8048 | `		SyBlobLength(pWorker) += nReplen;` |
|     24 | 8049 | `	}` |
|     59 | 8050 | `	return SXRET_OK;` |
|     32 | 8051 | `}` |
|      - | 8052 | `/*` |
|      - | 8053 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8054 | ` * to collect search/replace string.` |
|      - | 8055 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8056 | ` */` |
|     94 | 8057 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8058 | `{` |
|     99 | 8059 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8060 | `	SyString sWorker;` |
|      - | 8061 | `	const char *zIn;` |
|      - | 8062 | `	int nByte;` |
|      - | 8063 | `	/* Extract a string representation of the given argument */` |
|     99 | 8064 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     99 | 8065 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     99 | 8066 | `	if( nByte > 0 ){` |
|      - | 8067 | `		char *zDup;` |
|      - | 8068 | `		/* Duplicate the chunk */` |
|     97 | 8069 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8070 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8071 | `			);` |
|     97 | 8072 | `		if( zDup == 0 ){` |
|      - | 8073 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8074 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8075 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8076 | `			return SXERR_MEM;` |
|      - | 8077 | `		}` |
|     97 | 8078 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8079 | `		/* Save the chunk */` |
|     97 | 8080 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     46 | 8081 | `	}` |
|      - | 8082 | `	/* Save for later processing */` |
|     99 | 8083 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8084 | `	/* All done */` |
|     47 | 8085 | `	SXUNUSED(pKey); /* cc warning */` |
|     99 | 8086 | `	return PH7_OK;` |
|     52 | 8087 | `}` |
|      - | 8088 | `/*` |
|      - | 8089 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8090 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8091 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8092 | ` * Parameters` |
|      - | 8093 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8094 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8095 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8096 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8097 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8098 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8099 | ` * $search` |
|      - | 8100 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8101 | ` *  to designate multiple needles.` |
|      - | 8102 | ` * $replace` |
|      - | 8103 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8104 | ` *  to designate multiple replacements.` |
|      - | 8105 | ` * $subject` |
|      - | 8106 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8107 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8108 | ` *  of subject, and the return value is an array as well.` |
|      - | 8109 | ` * $count (Not used)` |
|      - | 8110 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8111 | ` * Return` |
|      - | 8112 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8113 | ` */` |
|  29858 | 8114 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8115 | `{` |
|      - | 8116 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8117 | `	ProcStringMatch xMatch;` |
|      - | 8118 | `	const char *zIn,*zFunc;` |
|      - | 8119 | `	str_replace_data sRep;` |
|      - | 8120 | `	SyBlob sWorker;` |
|      - | 8121 | `	SySet sReplace;` |
|      - | 8122 | `	SySet sSearch;` |
|      - | 8123 | `	int rep_str;` |
|      - | 8124 | `	int nByte;` |
|      - | 8125 | `	sxi32 rc;` |
|  29863 | 8126 | `	if( nArg < 3 ){` |
|      - | 8127 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8128 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8129 | `		return PH7_OK;` |
|      - | 8130 | `	}` |
|      - | 8131 | `	/* Initialize fields */` |
|  29863 | 8132 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29863 | 8133 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29863 | 8134 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29863 | 8135 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29863 | 8136 | `	sRep.pCtx = pCtx;` |
|  29863 | 8137 | `	sRep.pCollector = &sSearch;` |
|  29863 | 8138 | `	rep_str = 0;` |
|      - | 8139 | `	/* Extract the subject */` |
|  29863 | 8140 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29863 | 8141 | `	if( nByte < 1 ){` |
|      - | 8142 | `		/* Nothing to replace,return the empty string */` |
|     29 | 8143 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 8144 | `		return PH7_OK;` |
|      - | 8145 | `	}` |
|      - | 8146 | `	/* Copy the subject */` |
|  29835 | 8147 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8148 | `	/* Search string */` |
|  29835 | 8149 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8150 | `		/* Collect search string */` |
|     47 | 8151 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     26 | 8152 | `	}else{` |
|      - | 8153 | `		/* Single pattern */` |
|  29793 | 8154 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29793 | 8155 | `		if( nByte < 1 ){` |
|      - | 8156 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8157 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8158 | `			return PH7_OK;` |
|      - | 8159 | `		}` |
|  29789 | 8160 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8161 | `		/* Save for later processing */` |
|  29789 | 8162 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8163 | `	}` |
|      - | 8164 | `	/* Replace string */` |
|  29831 | 8165 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8166 | `		/* Collect replace string */` |
|      7 | 8167 | `		sRep.pCollector = &sReplace;` |
|      7 | 8168 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8169 | `	}else{` |
|      - | 8170 | `		/* Single needle */` |
|  29825 | 8171 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29825 | 8172 | `		rep_str = 1;` |
|  29825 | 8173 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8174 | `		/* Save for later processing */` |
|  29825 | 8175 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8176 | `	}` |
|      - | 8177 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29831 | 8178 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8179 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8180 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8181 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8182 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8183 | `	}` |
|      - | 8184 | `	/* Reset loop cursors */` |
|  29831 | 8185 | `	SySetResetCursor(&sSearch);` |
|  29831 | 8186 | `	SySetResetCursor(&sReplace);` |
|  29831 | 8187 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29831 | 8188 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8189 | `	/* Extract function name */` |
|  29831 | 8190 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8191 | `	/* Set the default pattern match routine */` |
|  29831 | 8192 | `	xMatch = SyBlobSearch;` |
|  29831 | 8193 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8194 | `		/* Case insensitive pattern match */` |
|     11 | 8195 | `		xMatch = iPatternMatch;` |
|      5 | 8196 | `	}` |
|      - | 8197 | `	/* Start the replace process */` |
|  59699 | 8198 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8199 | `		sxu32 nCount,nOfft;` |
|  29873 | 8200 | `		if( pSearch->nByte <  1 ){` |
|      - | 8201 | `			/* Empty string,ignore */` |
|      3 | 8202 | `			continue;` |
|      - | 8203 | `		}` |
|      - | 8204 | `		/* Extract the replace string */` |
|  29871 | 8205 | `		if( rep_str ){` |
|  29861 | 8206 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14933 | 8207 | `		}else{` |
|     11 | 8208 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8209 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8210 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8211 | `				 */` |
|      3 | 8212 | `				pReplace = 0;` |
|      1 | 8213 | `			}` |
|      - | 8214 | `		}` |
|  29871 | 8215 | `		if( pReplace == 0 ){` |
|      - | 8216 | `			/* Use an empty string instead */` |
|      3 | 8217 | `			pReplace = &sTemp;` |
|      1 | 8218 | `		}` |
|  29871 | 8219 | `		nOfft = nCount = 0;` |
|  14960 | 8220 | `		for(;;){` |
|  29925 | 8221 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8222 | `				break;` |
|      - | 8223 | `			}` |
|      - | 8224 | `			/* Perform a pattern lookup */` |
|  44867 | 8225 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  29908 | 8226 | `				pSearch->nByte,&nOfft);` |
|  29913 | 8227 | `			if( rc != SXRET_OK ){` |
|      - | 8228 | `				/* Pattern not found */` |
|  29859 | 8229 | `				break;` |
|      - | 8230 | `			}` |
|      - | 8231 | `			/* Perform the replace operation */` |
|     59 | 8232 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     59 | 8233 | `			if( rc != SXRET_OK ){` |
|      - | 8234 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8235 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8236 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8237 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8238 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8239 | `			}` |
|      - | 8240 | `			/* Increment offset counter */` |
|     59 | 8241 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8242 | `		}` |
|      5 | 8243 | `	}` |
|      - | 8244 | `	/* All done,clean-up the mess left behind */` |
|  29831 | 8245 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29831 | 8246 | `	SySetRelease(&sSearch);` |
|  29831 | 8247 | `	SySetRelease(&sReplace);` |
|  29831 | 8248 | `	SyBlobRelease(&sWorker);` |
|  29831 | 8249 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8250 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8251 | `	}` |
|  29831 | 8252 | `	return PH7_OK;` |
|  14934 | 8253 | `}` |
|      - | 8254 | `/*` |
|      - | 8255 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8256 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8257 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8258 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8259 | ` */` |
|      - | 8260 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8261 | `struct strtr_entry` |
|      - | 8262 | `{` |
|      - | 8263 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8264 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8265 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8266 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8267 | `};` |
|      - | 8268 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8269 | `struct strtr_collect` |
|      - | 8270 | `{` |
|      - | 8271 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8272 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8273 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8274 | `};` |
|      - | 8275 | `/*` |
|      - | 8276 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8277 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8278 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8279 | ` */` |
|     20 | 8280 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8281 | `{` |
|     21 | 8282 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8283 | `	const char *zKey,*zVal;` |
|      - | 8284 | `	strtr_entry sEnt;` |
|      - | 8285 | `	int nKey,nVal;` |
|     21 | 8286 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8287 | `	if( nKey < 1 ){` |
|      - | 8288 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 8289 | `		return PH7_OK;` |
|      - | 8290 | `	}` |
|     21 | 8291 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 8292 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8293 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 8294 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8295 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8296 | `		return SXERR_ABORT;` |
|      - | 8297 | `	}` |
|     21 | 8298 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8299 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 8300 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8301 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8302 | `		return SXERR_ABORT;` |
|      - | 8303 | `	}` |
|     21 | 8304 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8305 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8306 | `		return SXERR_ABORT;` |
|      - | 8307 | `	}` |
|     21 | 8308 | `	return PH7_OK;` |
|     11 | 8309 | `}` |
|      - | 8310 | `/*` |
|      - | 8311 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8312 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8313 | ` *  Translate characters or replace substrings.` |
|      - | 8314 | ` * Parameters` |
|      - | 8315 | ` *  $str` |
|      - | 8316 | ` *  The string being translated.` |
|      - | 8317 | ` * $from` |
|      - | 8318 | ` *  The string being translated to to.` |
|      - | 8319 | ` * $to` |
|      - | 8320 | ` *  The string replacing from.` |
|      - | 8321 | ` * $replace_pairs` |
|      - | 8322 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8323 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8324 | ` * Return` |
|      - | 8325 | ` *  The translated string.` |
|      - | 8326 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8327 | ` */` |
|     12 | 8328 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8329 | `{` |
|      - | 8330 | `	const char *zIn;` |
|      - | 8331 | `	int nLen;` |
|     13 | 8332 | `	if( nArg < 1 ){` |
|      - | 8333 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8334 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8335 | `		return PH7_OK;` |
|      - | 8336 | `	}` |
|     13 | 8337 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8338 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8339 | `		/* Invalid arguments */` |
|    ! 0 | 8340 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8341 | `		return PH7_OK;` |
|      - | 8342 | `	}` |
|     18 | 8343 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8344 | `		strtr_collect sCol;` |
|      - | 8345 | `		SyBlob sPool,sWorker;` |
|      - | 8346 | `		SySet sTable;` |
|      - | 8347 | `		const char *zPool;` |
|      - | 8348 | `		strtr_entry *pEnt;` |
|      - | 8349 | `		sxi32 rc;` |
|      - | 8350 | `		int i,iRun;` |
|      - | 8351 | `		/*` |
|      - | 8352 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8353 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8354 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8355 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8356 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8357 | `		 */` |
|     11 | 8358 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8359 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8360 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8361 | `		sCol.pPool  = &sPool;` |
|     11 | 8362 | `		sCol.pTable = &sTable;` |
|     11 | 8363 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8364 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8365 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8366 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8367 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8368 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8369 | `			SySetRelease(&sTable);` |
|    ! 0 | 8370 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8371 | `		}` |
|      - | 8372 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8373 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8374 | `		rc = SXRET_OK;` |
|     11 | 8375 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8376 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8377 | `			strtr_entry *pBest = 0;` |
|     33 | 8378 | `			sxu32 nBest = 0;` |
|      - | 8379 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8380 | `			SySetResetCursor(&sTable);` |
|     97 | 8381 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 8382 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 8383 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 8384 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8385 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8386 | `					pBest = pEnt;` |
|     14 | 8387 | `				}` |
|      1 | 8388 | `			}` |
|     33 | 8389 | `			if( pBest == 0 ){` |
|      - | 8390 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8391 | `				i++;` |
|      9 | 8392 | `				continue;` |
|      - | 8393 | `			}` |
|      - | 8394 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8395 | `			if( i > iRun ){` |
|      5 | 8396 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8397 | `			}` |
|     25 | 8398 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8399 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8400 | `			}` |
|     25 | 8401 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8402 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8403 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8404 | `				SySetRelease(&sTable);` |
|    ! 0 | 8405 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8406 | `			}` |
|     25 | 8407 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8408 | `			iRun = i;` |
|      1 | 8409 | `		}` |
|      - | 8410 | `		/* Flush the trailing literal run. */` |
|     11 | 8411 | `		if( nLen > iRun ){` |
|      3 | 8412 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8413 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8414 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8415 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8416 | `				SySetRelease(&sTable);` |
|    ! 0 | 8417 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8418 | `			}` |
|      1 | 8419 | `		}` |
|      - | 8420 | `		/* All done, return the result string */` |
|     16 | 8421 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8422 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8423 | `		/* Clean-up */` |
|     11 | 8424 | `		SyBlobRelease(&sPool);` |
|     11 | 8425 | `		SyBlobRelease(&sWorker);` |
|     11 | 8426 | `		SySetRelease(&sTable);` |
|     11 | 8427 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8428 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8429 | `		}` |
|      6 | 8430 | `	}else{` |
|      - | 8431 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8432 | `		const char *zFrom,*zTo;` |
|      3 | 8433 | `		if( nArg < 3 ){` |
|      - | 8434 | `			/* Nothing to replace */` |
|    ! 0 | 8435 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8436 | `			return PH7_OK;` |
|      - | 8437 | `		}` |
|      - | 8438 | `		/* Extract given arguments */` |
|      3 | 8439 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8440 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8441 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8442 | `			/* Nothing to replace */` |
|    ! 0 | 8443 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8444 | `			return PH7_OK;` |
|      - | 8445 | `		}` |
|      - | 8446 | `		/* Start the replace process */` |
|     13 | 8447 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8448 | `			c = zIn[i];` |
|     11 | 8449 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8450 | `				if ( iOfft < tlen ){` |
|      5 | 8451 | `					c = zTo[iOfft];` |
|      2 | 8452 | `				}` |
|      2 | 8453 | `			}` |
|     11 | 8454 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8455 |  |
|      6 | 8456 | `		}` |
|      - | 8457 | `	}` |
|     13 | 8458 | `	return PH7_OK;` |
|      7 | 8459 | `}` |
|      - | 8460 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8461 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8462 | `/*` |
|      - | 8463 | ` * Parse an INI string.` |
|      - | 8464 |  |
|      - | 8465 | ` * According to wikipedia` |
|      - | 8466 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8467 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8468 | ` *  Format` |
|      - | 8469 | `*    Properties` |
|      - | 8470 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8471 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8472 | `*     Example:` |
|      - | 8473 | `*      name=value` |
|      - | 8474 | `*    Sections` |
|      - | 8475 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8476 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8477 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8478 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8479 | `*     Example:` |
|      - | 8480 | `*      [section]` |
|      - | 8481 | `*   Comments` |
|      - | 8482 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8483 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8484 | `*/` |
|     12 | 8485 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8486 | `{` |
|      - | 8487 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8488 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8489 | `	SyHashEntry *pEntry;` |
|      - | 8490 | `	SyString sEntry;` |
|      - | 8491 | `	SyHash sHash;` |
|      - | 8492 | `	int c;` |
|      - | 8493 | `	/* Create an empty array and worker variables */` |
|     13 | 8494 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8495 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8496 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8497 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8498 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8499 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8500 | `	}` |
|     13 | 8501 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8502 | `	pCur = pArray;` |
|      - | 8503 | `	/* Start the parse process */` |
|     21 | 8504 | `	for(;;){` |
|      - | 8505 | `		/* Ignore leading white spaces */` |
|     69 | 8506 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8507 | `			zIn++;` |
|      1 | 8508 | `		}` |
|     43 | 8509 | `		if( zIn >= zEnd ){` |
|      - | 8510 | `			/* No more input to process */` |
|     13 | 8511 | `			break;` |
|      - | 8512 | `		}` |
|     31 | 8513 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8514 | `			/* Comment til the end of line */` |
|    ! 0 | 8515 | `			zIn++;` |
|    ! 0 | 8516 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8517 | `				zIn++;` |
|    ! 0 | 8518 | `			}` |
|    ! 0 | 8519 | `			continue;` |
|      - | 8520 | `		}` |
|      - | 8521 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8522 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8523 | `		if( zIn[0] == '[' ){` |
|      - | 8524 | `			/* Section: Extract the section name */` |
|      9 | 8525 | `			zIn++;` |
|      9 | 8526 | `			zCur = zIn;` |
|     73 | 8527 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8528 | `				zIn++;` |
|      1 | 8529 | `			}` |
|      9 | 8530 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8531 | `				/* Save the section name */` |
|      5 | 8532 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8533 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8534 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8535 | `				if( sEntry.nByte > 0 ){` |
|      - | 8536 | `					/* Associate an array with the section */` |
|      5 | 8537 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8538 | `					if( pSection ){` |
|      5 | 8539 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8540 | `						pCur = pSection;` |
|      2 | 8541 | `					}` |
|      2 | 8542 | `				}` |
|      2 | 8543 | `			}` |
|      9 | 8544 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8545 | `		}else{` |
|      - | 8546 | `			ph7_value *pOldCur;` |
|      - | 8547 | `			int is_array;` |
|      - | 8548 | `			int iLen;` |
|      - | 8549 | `			/* Properties */` |
|     23 | 8550 | `			is_array = 0;` |
|     23 | 8551 | `			zCur = zIn;` |
|     23 | 8552 | `			iLen = 0; /* cc warning */` |
|     23 | 8553 | `			pOldCur = pCur;` |
|    155 | 8554 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8555 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8556 | `					/* Array */` |
|    ! 0 | 8557 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8558 | `					is_array = 1;` |
|    ! 0 | 8559 | `					if( iLen > 0 ){` |
|    ! 0 | 8560 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8561 | `						/* Query the hashtable */` |
|    ! 0 | 8562 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8563 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8564 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8565 | `						if( pEntry ){` |
|    ! 0 | 8566 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8567 | `						}else{` |
|      - | 8568 | `							/* Create an empty array */` |
|    ! 0 | 8569 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8570 | `							if( pvArr ){` |
|      - | 8571 | `								/* Save the entry */` |
|    ! 0 | 8572 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8573 | `								/* Insert the entry */` |
|    ! 0 | 8574 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8575 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8576 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8577 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8578 | `							}` |
|      - | 8579 | `						}` |
|    ! 0 | 8580 | `						if( pvArr ){` |
|    ! 0 | 8581 | `							pCur = pvArr;` |
|    ! 0 | 8582 | `						}` |
|    ! 0 | 8583 | `					}` |
|    ! 0 | 8584 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8585 | `						zIn++;` |
|    ! 0 | 8586 | `					}` |
|    ! 0 | 8587 | `				}` |
|    133 | 8588 | `				zIn++;` |
|      1 | 8589 | `			}` |
|     23 | 8590 | `			if( !is_array ){` |
|     23 | 8591 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8592 | `			}` |
|      - | 8593 | `			/* Trim the key */` |
|     23 | 8594 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8595 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8596 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8597 | `				if( !is_array ){` |
|      - | 8598 | `					/* Save the key name */` |
|     23 | 8599 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8600 | `				}` |
|      - | 8601 | `				/* extract key value */` |
|     23 | 8602 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8603 | `				zIn++; /* '=' */` |
|     39 | 8604 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8605 | `					zIn++;` |
|      1 | 8606 | `				}` |
|     23 | 8607 | `				if( zIn < zEnd ){` |
|     21 | 8608 | `					zCur = zIn;` |
|     21 | 8609 | `					c = zIn[0];` |
|     21 | 8610 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8611 | `						zIn++;` |
|      - | 8612 | `						/* Delimit the value */` |
|    ! 0 | 8613 | `						while( zIn < zEnd ){` |
|    ! 0 | 8614 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8615 | `								break;` |
|      - | 8616 | `							}` |
|    ! 0 | 8617 | `							zIn++;` |
|    ! 0 | 8618 | `						}` |
|    ! 0 | 8619 | `						if( zIn < zEnd ){` |
|    ! 0 | 8620 | `							zIn++;` |
|    ! 0 | 8621 | `						}` |
|    ! 0 | 8622 | `					}else{` |
|    125 | 8623 | `						while( zIn < zEnd ){` |
|    123 | 8624 | `							if( zIn[0] == '\n' ){` |
|     19 | 8625 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8626 | `									break;` |
|    ! 0 | 8627 | `								}` |
|    105 | 8628 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8629 | `								/* Inline comments */` |
|    ! 0 | 8630 | `								break;` |
|      - | 8631 | `							}` |
|    105 | 8632 | `							zIn++;` |
|      1 | 8633 | `						}` |
|      - | 8634 | `					}` |
|      - | 8635 | `					/* Trim the value */` |
|     21 | 8636 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8637 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8638 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8639 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8640 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8641 | `					}` |
|     21 | 8642 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8643 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8644 | `					}` |
|      - | 8645 | `					/* Insert the key and it's value */` |
|     21 | 8646 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8647 | `				}` |
|     12 | 8648 | `			}else{` |
|    ! 0 | 8649 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8650 | `					zIn++;` |
|    ! 0 | 8651 | `				}` |
|      - | 8652 | `			}` |
|     23 | 8653 | `			pCur = pOldCur;` |
|      - | 8654 | `		}` |
|      1 | 8655 | `	}` |
|     13 | 8656 | `	SyHashRelease(&sHash);` |
|      - | 8657 | `	/* Return the parse of the INI string */` |
|     13 | 8658 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8659 | `	return SXRET_OK;` |
|      7 | 8660 | `}` |
|      - | 8661 | `/*` |
|      - | 8662 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8663 | ` *  Parse a configuration string.` |
|      - | 8664 | ` * Parameters` |
|      - | 8665 | ` *  $ini` |
|      - | 8666 | ` *   The contents of the ini file being parsed.` |
|      - | 8667 | ` *  $process_sections` |
|      - | 8668 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8669 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8670 | ` *  $scanner_mode (Not used)` |
|      - | 8671 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8672 | ` *   then option values will not be parsed.` |
|      - | 8673 | ` * Return` |
|      - | 8674 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8675 | ` */` |
|     10 | 8676 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8677 | `{` |
|      - | 8678 | `	const char *zIni;` |
|      - | 8679 | `	int nByte;` |
|     11 | 8680 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8681 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8682 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8683 | `		return PH7_OK;` |
|      - | 8684 | `	}` |
|      - | 8685 | `	/* Extract the raw INI buffer */` |
|     11 | 8686 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8687 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8688 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8689 | `}` |
|      - | 8690 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8691 |  |
|      - | 8692 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8693 |  |
|      - | 8694 | `/*` |
|      - | 8695 | ` * Ctype Functions.` |
|      - | 8696 | ` * Status:` |
|      - | 8697 | ` *    Stable.` |
|      - | 8698 | ` */` |
|      - | 8699 | `/*` |
|      - | 8700 | ` * bool ctype_alnum(string $text)` |
|      - | 8701 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8702 | ` * Parameters` |
|      - | 8703 | ` *  $text` |
|      - | 8704 | ` *   The tested string.` |
|      - | 8705 | ` * Return` |
|      - | 8706 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8707 | ` */` |
|     14 | 8708 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8709 | `{` |
|      - | 8710 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8711 | `	int nLen;` |
|     15 | 8712 | `	if( nArg < 1 ){` |
|      - | 8713 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8714 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8715 | `		return PH7_OK;` |
|      - | 8716 | `	}` |
|      - | 8717 | `	/* Extract the target string */` |
|     15 | 8718 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8719 | `	zEnd = &zIn[nLen];` |
|     15 | 8720 | `	if( nLen < 1 ){` |
|      - | 8721 | `		/* Empty string,return FALSE */` |
|      3 | 8722 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8723 | `		return PH7_OK;` |
|      - | 8724 | `	}` |
|      - | 8725 | `	/* Perform the requested operation */` |
|     32 | 8726 | `	for(;;){` |
|     65 | 8727 | `		if( zIn >= zEnd ){` |
|      - | 8728 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8729 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8730 | `			return PH7_OK;` |
|      - | 8731 | `		}` |
|     57 | 8732 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 8733 | `			break;` |
|      - | 8734 | `		}` |
|      - | 8735 | `		/* Point to the next character */` |
|     53 | 8736 | `		zIn++;` |
|      1 | 8737 | `	}` |
|      - | 8738 | `	/* The test failed,return FALSE */` |
|      5 | 8739 | `	ph7_result_bool(pCtx,0);` |
|      5 | 8740 | `	return PH7_OK;` |
|      8 | 8741 | `}` |
|      - | 8742 | `/*` |
|      - | 8743 | ` * bool ctype_alpha(string $text)` |
|      - | 8744 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8745 | ` * Parameters` |
|      - | 8746 | ` *  $text` |
|      - | 8747 | ` *   The tested string.` |
|      - | 8748 | ` * Return` |
|      - | 8749 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8750 | ` */` |
|     16 | 8751 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8752 | `{` |
|      - | 8753 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8754 | `	int nLen;` |
|     17 | 8755 | `	if( nArg < 1 ){` |
|      - | 8756 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8757 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8758 | `		return PH7_OK;` |
|      - | 8759 | `	}` |
|      - | 8760 | `	/* Extract the target string */` |
|     17 | 8761 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8762 | `	zEnd = &zIn[nLen];` |
|     17 | 8763 | `	if( nLen < 1 ){` |
|      - | 8764 | `		/* Empty string,return FALSE */` |
|      3 | 8765 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8766 | `		return PH7_OK;` |
|      - | 8767 | `	}` |
|      - | 8768 | `	/* Perform the requested operation */` |
|     42 | 8769 | `	for(;;){` |
|     85 | 8770 | `		if( zIn >= zEnd ){` |
|      - | 8771 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8772 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8773 | `			return PH7_OK;` |
|      - | 8774 | `		}` |
|     77 | 8775 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8776 | `			break;` |
|      - | 8777 | `		}` |
|      - | 8778 | `		/* Point to the next character */` |
|     71 | 8779 | `		zIn++;` |
|      1 | 8780 | `	}` |
|      - | 8781 | `	/* The test failed,return FALSE */` |
|      7 | 8782 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8783 | `	return PH7_OK;` |
|      9 | 8784 | `}` |
|      - | 8785 | `/*` |
|      - | 8786 | ` * bool ctype_cntrl(string $text)` |
|      - | 8787 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8788 | ` * Parameters` |
|      - | 8789 | ` *  $text` |
|      - | 8790 | ` *   The tested string.` |
|      - | 8791 | ` * Return` |
|      - | 8792 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8793 | ` */` |
|     16 | 8794 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8795 | `{` |
|      - | 8796 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8797 | `	int nLen;` |
|     17 | 8798 | `	if( nArg < 1 ){` |
|      - | 8799 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8800 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8801 | `		return PH7_OK;` |
|      - | 8802 | `	}` |
|      - | 8803 | `	/* Extract the target string */` |
|     17 | 8804 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8805 | `	zEnd = &zIn[nLen];` |
|     17 | 8806 | `	if( nLen < 1 ){` |
|      - | 8807 | `		/* Empty string,return FALSE */` |
|      3 | 8808 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8809 | `		return PH7_OK;` |
|      - | 8810 | `	}` |
|      - | 8811 | `	/* Perform the requested operation */` |
|     14 | 8812 | `	for(;;){` |
|     29 | 8813 | `		if( zIn >= zEnd ){` |
|      - | 8814 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8815 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8816 | `			return PH7_OK;` |
|      - | 8817 | `		}` |
|     21 | 8818 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8819 | `			/* UTF-8 stream  */` |
|    ! 0 | 8820 | `			break;` |
|      - | 8821 | `		}` |
|     21 | 8822 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8823 | `			break;` |
|      - | 8824 | `		}` |
|      - | 8825 | `		/* Point to the next character */` |
|     15 | 8826 | `		zIn++;` |
|      1 | 8827 | `	}` |
|      - | 8828 | `	/* The test failed,return FALSE */` |
|      7 | 8829 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8830 | `	return PH7_OK;` |
|      9 | 8831 | `}` |
|      - | 8832 | `/*` |
|      - | 8833 | ` * bool ctype_digit(string $text)` |
|      - | 8834 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8835 | ` * Parameters` |
|      - | 8836 | ` *  $text` |
|      - | 8837 | ` *   The tested string.` |
|      - | 8838 | ` * Return` |
|      - | 8839 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 8840 | ` */` |
|   1855 | 8841 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8842 | `{` |
|      - | 8843 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8844 | `	int nLen;` |
|   1860 | 8845 | `	if( nArg < 1 ){` |
|      - | 8846 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8847 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8848 | `		return PH7_OK;` |
|      - | 8849 | `	}` |
|      - | 8850 | `	/* Extract the target string */` |
|   1860 | 8851 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1860 | 8852 | `	zEnd = &zIn[nLen];` |
|   1860 | 8853 | `	if( nLen < 1 ){` |
|      - | 8854 | `		/* Empty string,return FALSE */` |
|      3 | 8855 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8856 | `		return PH7_OK;` |
|      - | 8857 | `	}` |
|      - | 8858 | `	/* Perform the requested operation */` |
|   1722 | 8859 | `	for(;;){` |
|   3447 | 8860 | `		if( zIn >= zEnd ){` |
|      - | 8861 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1544 | 8862 | `			ph7_result_bool(pCtx,1);` |
|   1544 | 8863 | `			return PH7_OK;` |
|      - | 8864 | `		}` |
|   1908 | 8865 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8866 | `			/* UTF-8 stream  */` |
|    ! 0 | 8867 | `			break;` |
|      - | 8868 | `		}` |
|   1908 | 8869 | `		if( !SyisDigit(zIn[0]) ){` |
|    319 | 8870 | `			break;` |
|      - | 8871 | `		}` |
|      - | 8872 | `		/* Point to the next character */` |
|   1594 | 8873 | `		zIn++;` |
|      5 | 8874 | `	}` |
|      - | 8875 | `	/* The test failed,return FALSE */` |
|    319 | 8876 | `	ph7_result_bool(pCtx,0);` |
|    319 | 8877 | `	return PH7_OK;` |
|    933 | 8878 | `}` |
|      - | 8879 | `/*` |
|      - | 8880 | ` * bool ctype_xdigit(string $text)` |
|      - | 8881 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 8882 | ` * Parameters` |
|      - | 8883 | ` *  $text` |
|      - | 8884 | ` *   The tested string.` |
|      - | 8885 | ` * Return` |
|      - | 8886 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 8887 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 8888 | ` */` |
|     18 | 8889 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8890 | `{` |
|      - | 8891 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8892 | `	int nLen;` |
|     19 | 8893 | `	if( nArg < 1 ){` |
|      - | 8894 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8895 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8896 | `		return PH7_OK;` |
|      - | 8897 | `	}` |
|      - | 8898 | `	/* Extract the target string */` |
|     19 | 8899 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8900 | `	zEnd = &zIn[nLen];` |
|     19 | 8901 | `	if( nLen < 1 ){` |
|      - | 8902 | `		/* Empty string,return FALSE */` |
|      3 | 8903 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8904 | `		return PH7_OK;` |
|      - | 8905 | `	}` |
|      - | 8906 | `	/* Perform the requested operation */` |
|     46 | 8907 | `	for(;;){` |
|     93 | 8908 | `		if( zIn >= zEnd ){` |
|      - | 8909 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 8910 | `			ph7_result_bool(pCtx,1);` |
|     11 | 8911 | `			return PH7_OK;` |
|      - | 8912 | `		}` |
|     83 | 8913 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8914 | `			/* UTF-8 stream  */` |
|    ! 0 | 8915 | `			break;` |
|      - | 8916 | `		}` |
|     83 | 8917 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8918 | `			break;` |
|      - | 8919 | `		}` |
|      - | 8920 | `		/* Point to the next character */` |
|     77 | 8921 | `		zIn++;` |
|      1 | 8922 | `	}` |
|      - | 8923 | `	/* The test failed,return FALSE */` |
|      7 | 8924 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8925 | `	return PH7_OK;` |
|     10 | 8926 | `}` |
|      - | 8927 | `/*` |
|      - | 8928 | ` * bool ctype_graph(string $text)` |
|      - | 8929 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8930 | ` * Parameters` |
|      - | 8931 | ` *  $text` |
|      - | 8932 | ` *   The tested string.` |
|      - | 8933 | ` * Return` |
|      - | 8934 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8935 | ` * (no white space), FALSE otherwise.` |
|      - | 8936 | ` */` |
|     16 | 8937 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8938 | `{` |
|      - | 8939 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8940 | `	int nLen;` |
|     17 | 8941 | `	if( nArg < 1 ){` |
|      - | 8942 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8943 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8944 | `		return PH7_OK;` |
|      - | 8945 | `	}` |
|      - | 8946 | `	/* Extract the target string */` |
|     17 | 8947 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8948 | `	zEnd = &zIn[nLen];` |
|     17 | 8949 | `	if( nLen < 1 ){` |
|      - | 8950 | `		/* Empty string,return FALSE */` |
|      3 | 8951 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8952 | `		return PH7_OK;` |
|      - | 8953 | `	}` |
|      - | 8954 | `	/* Perform the requested operation */` |
|     57 | 8955 | `	for(;;){` |
|    115 | 8956 | `		if( zIn >= zEnd ){` |
|      - | 8957 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8958 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8959 | `			return PH7_OK;` |
|      - | 8960 | `		}` |
|    107 | 8961 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8962 | `			/* UTF-8 stream  */` |
|    ! 0 | 8963 | `			break;` |
|      - | 8964 | `		}` |
|    107 | 8965 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 8966 | `			break;` |
|      - | 8967 | `		}` |
|      - | 8968 | `		/* Point to the next character */` |
|    101 | 8969 | `		zIn++;` |
|      1 | 8970 | `	}` |
|      - | 8971 | `	/* The test failed,return FALSE */` |
|      7 | 8972 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8973 | `	return PH7_OK;` |
|      9 | 8974 | `}` |
|      - | 8975 | `/*` |
|      - | 8976 | ` * bool ctype_print(string $text)` |
|      - | 8977 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 8978 | ` * Parameters` |
|      - | 8979 | ` *  $text` |
|      - | 8980 | ` *   The tested string.` |
|      - | 8981 | ` * Return` |
|      - | 8982 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 8983 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 8984 | ` *  or control function at all.` |
|      - | 8985 | ` */` |
|     16 | 8986 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8987 | `{` |
|      - | 8988 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8989 | `	int nLen;` |
|     17 | 8990 | `	if( nArg < 1 ){` |
|      - | 8991 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8992 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8993 | `		return PH7_OK;` |
|      - | 8994 | `	}` |
|      - | 8995 | `	/* Extract the target string */` |
|     17 | 8996 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8997 | `	zEnd = &zIn[nLen];` |
|     17 | 8998 | `	if( nLen < 1 ){` |
|      - | 8999 | `		/* Empty string,return FALSE */` |
|      3 | 9000 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9001 | `		return PH7_OK;` |
|      - | 9002 | `	}` |
|      - | 9003 | `	/* Perform the requested operation */` |
|     63 | 9004 | `	for(;;){` |
|    127 | 9005 | `		if( zIn >= zEnd ){` |
|      - | 9006 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9007 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9008 | `			return PH7_OK;` |
|      - | 9009 | `		}` |
|    119 | 9010 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9011 | `			/* UTF-8 stream  */` |
|    ! 0 | 9012 | `			break;` |
|      - | 9013 | `		}` |
|    119 | 9014 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9015 | `			break;` |
|      - | 9016 | `		}` |
|      - | 9017 | `		/* Point to the next character */` |
|    113 | 9018 | `		zIn++;` |
|      1 | 9019 | `	}` |
|      - | 9020 | `	/* The test failed,return FALSE */` |
|      7 | 9021 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9022 | `	return PH7_OK;` |
|      9 | 9023 | `}` |
|      - | 9024 | `/*` |
|      - | 9025 | ` * bool ctype_punct(string $text)` |
|      - | 9026 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9027 | ` * Parameters` |
|      - | 9028 | ` *  $text` |
|      - | 9029 | ` *   The tested string.` |
|      - | 9030 | ` * Return` |
|      - | 9031 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9032 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9033 | ` */` |
|     18 | 9034 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9035 | `{` |
|      - | 9036 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9037 | `	int nLen;` |
|     19 | 9038 | `	if( nArg < 1 ){` |
|      - | 9039 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9040 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9041 | `		return PH7_OK;` |
|      - | 9042 | `	}` |
|      - | 9043 | `	/* Extract the target string */` |
|     19 | 9044 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9045 | `	zEnd = &zIn[nLen];` |
|     19 | 9046 | `	if( nLen < 1 ){` |
|      - | 9047 | `		/* Empty string,return FALSE */` |
|      3 | 9048 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9049 | `		return PH7_OK;` |
|      - | 9050 | `	}` |
|      - | 9051 | `	/* Perform the requested operation */` |
|     38 | 9052 | `	for(;;){` |
|     77 | 9053 | `		if( zIn >= zEnd ){` |
|      - | 9054 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9055 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9056 | `			return PH7_OK;` |
|      - | 9057 | `		}` |
|     69 | 9058 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9059 | `			/* UTF-8 stream  */` |
|    ! 0 | 9060 | `			break;` |
|      - | 9061 | `		}` |
|     69 | 9062 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9063 | `			break;` |
|      - | 9064 | `		}` |
|      - | 9065 | `		/* Point to the next character */` |
|     61 | 9066 | `		zIn++;` |
|      1 | 9067 | `	}` |
|      - | 9068 | `	/* The test failed,return FALSE */` |
|      9 | 9069 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9070 | `	return PH7_OK;` |
|     10 | 9071 | `}` |
|      - | 9072 | `/*` |
|      - | 9073 | ` * bool ctype_space(string $text)` |
|      - | 9074 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9075 | ` * Parameters` |
|      - | 9076 | ` *  $text` |
|      - | 9077 | ` *   The tested string.` |
|      - | 9078 | ` * Return` |
|      - | 9079 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9080 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9081 | ` *  and form feed characters.` |
|      - | 9082 | ` */` |
|  62922 | 9083 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9084 | `{` |
|      - | 9085 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9086 | `	int nLen;` |
|  62927 | 9087 | `	if( nArg < 1 ){` |
|      - | 9088 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9089 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9090 | `		return PH7_OK;` |
|      - | 9091 | `	}` |
|      - | 9092 | `	/* Extract the target string */` |
|  62927 | 9093 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  62927 | 9094 | `	zEnd = &zIn[nLen];` |
|  62927 | 9095 | `	if( nLen < 1 ){` |
|      - | 9096 | `		/* Empty string,return FALSE */` |
|      3 | 9097 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9098 | `		return PH7_OK;` |
|      - | 9099 | `	}` |
|      - | 9100 | `	/* Perform the requested operation */` |
|  32578 | 9101 | `	for(;;){` |
|  65075 | 9102 | `		if( zIn >= zEnd ){` |
|      - | 9103 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2131 | 9104 | `			ph7_result_bool(pCtx,1);` |
|   2131 | 9105 | `			return PH7_OK;` |
|      - | 9106 | `		}` |
|  62949 | 9107 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9108 | `			/* UTF-8 stream  */` |
|    ! 0 | 9109 | `			break;` |
|      - | 9110 | `		}` |
|  62949 | 9111 | `		if( !SyisSpace(zIn[0]) ){` |
|  60799 | 9112 | `			break;` |
|      - | 9113 | `		}` |
|      - | 9114 | `		/* Point to the next character */` |
|   2155 | 9115 | `		zIn++;` |
|      5 | 9116 | `	}` |
|      - | 9117 | `	/* The test failed,return FALSE */` |
|  60799 | 9118 | `	ph7_result_bool(pCtx,0);` |
|  60799 | 9119 | `	return PH7_OK;` |
|  31509 | 9120 | `}` |
|      - | 9121 | `/*` |
|      - | 9122 | ` * bool ctype_lower(string $text)` |
|      - | 9123 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9124 | ` * Parameters` |
|      - | 9125 | ` *  $text` |
|      - | 9126 | ` *   The tested string.` |
|      - | 9127 | ` * Return` |
|      - | 9128 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9129 | ` */` |
|     16 | 9130 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9131 | `{` |
|      - | 9132 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9133 | `	int nLen;` |
|     17 | 9134 | `	if( nArg < 1 ){` |
|      - | 9135 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9136 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9137 | `		return PH7_OK;` |
|      - | 9138 | `	}` |
|      - | 9139 | `	/* Extract the target string */` |
|     17 | 9140 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9141 | `	zEnd = &zIn[nLen];` |
|     17 | 9142 | `	if( nLen < 1 ){` |
|      - | 9143 | `		/* Empty string,return FALSE */` |
|      3 | 9144 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9145 | `		return PH7_OK;` |
|      - | 9146 | `	}` |
|      - | 9147 | `	/* Perform the requested operation */` |
|     27 | 9148 | `	for(;;){` |
|     55 | 9149 | `		if( zIn >= zEnd ){` |
|      - | 9150 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9151 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9152 | `			return PH7_OK;` |
|      - | 9153 | `		}` |
|     51 | 9154 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9155 | `			break;` |
|      - | 9156 | `		}` |
|      - | 9157 | `		/* Point to the next character */` |
|     41 | 9158 | `		zIn++;` |
|      1 | 9159 | `	}` |
|      - | 9160 | `	/* The test failed,return FALSE */` |
|     11 | 9161 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9162 | `	return PH7_OK;` |
|      9 | 9163 | `}` |
|      - | 9164 | `/*` |
|      - | 9165 | ` * bool ctype_upper(string $text)` |
|      - | 9166 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9167 | ` * Parameters` |
|      - | 9168 | ` *  $text` |
|      - | 9169 | ` *   The tested string.` |
|      - | 9170 | ` * Return` |
|      - | 9171 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9172 | ` */` |
|     16 | 9173 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9174 | `{` |
|      - | 9175 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9176 | `	int nLen;` |
|     17 | 9177 | `	if( nArg < 1 ){` |
|      - | 9178 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9179 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9180 | `		return PH7_OK;` |
|      - | 9181 | `	}` |
|      - | 9182 | `	/* Extract the target string */` |
|     17 | 9183 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9184 | `	zEnd = &zIn[nLen];` |
|     17 | 9185 | `	if( nLen < 1 ){` |
|      - | 9186 | `		/* Empty string,return FALSE */` |
|      3 | 9187 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9188 | `		return PH7_OK;` |
|      - | 9189 | `	}` |
|      - | 9190 | `	/* Perform the requested operation */` |
|     28 | 9191 | `	for(;;){` |
|     57 | 9192 | `		if( zIn >= zEnd ){` |
|      - | 9193 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9194 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9195 | `			return PH7_OK;` |
|      - | 9196 | `		}` |
|     53 | 9197 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9198 | `			break;` |
|      - | 9199 | `		}` |
|      - | 9200 | `		/* Point to the next character */` |
|     43 | 9201 | `		zIn++;` |
|      1 | 9202 | `	}` |
|      - | 9203 | `	/* The test failed,return FALSE */` |
|     11 | 9204 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9205 | `	return PH7_OK;` |
|      9 | 9206 | `}` |
|      - | 9207 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9208 | `/*` |
|      - | 9209 | ` * Section:` |
|      - | 9210 | ` *    URL handling Functions.` |
|      - | 9211 | ` * Status:` |
|      - | 9212 | ` *    Stable.` |
|      - | 9213 | ` */` |
|      - | 9214 | `/*` |
|      - | 9215 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9216 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9217 | ` */` |
|   1026 | 9218 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9219 | `{` |
|      - | 9220 | `	/* Store in the call context result buffer */` |
|   1028 | 9221 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 9222 | `	return SXRET_OK;` |
|      2 | 9223 | `}` |
|      - | 9224 | `/*` |
|      - | 9225 | ` * string base64_encode(string $data)` |
|      - | 9226 | ` * string convert_uuencode(string $data)` |
|      - | 9227 | ` *  Encodes data with MIME base64` |
|      - | 9228 | ` * Parameter` |
|      - | 9229 | ` *  $data` |
|      - | 9230 | ` *    Data to encode` |
|      - | 9231 | ` * Return` |
|      - | 9232 | ` *  Encoded data or FALSE on failure.` |
|      - | 9233 | ` */` |
|      6 | 9234 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9235 | `{` |
|      - | 9236 | `	const char *zIn;` |
|      - | 9237 | `	int nLen;` |
|      7 | 9238 | `	if( nArg < 1 ){` |
|      - | 9239 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9240 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9241 | `		return PH7_OK;` |
|      - | 9242 | `	}` |
|      - | 9243 | `	/* Extract the input string */` |
|      7 | 9244 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9245 | `	if( nLen < 1 ){` |
|      - | 9246 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9247 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9248 | `		return PH7_OK;` |
|      - | 9249 | `	}` |
|      - | 9250 | `	/* Perform the BASE64 encoding */` |
|      7 | 9251 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9252 | `	return PH7_OK;` |
|      4 | 9253 | `}` |
|      - | 9254 | `/*` |
|      - | 9255 | ` * string base64_decode(string $data)` |
|      - | 9256 | ` * string convert_uudecode(string $data)` |
|      - | 9257 | ` *  Decodes data encoded with MIME base64` |
|      - | 9258 | ` * Parameter` |
|      - | 9259 | ` *  $data` |
|      - | 9260 | ` *    Encoded data.` |
|      - | 9261 | ` * Return` |
|      - | 9262 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9263 | ` */` |
|     34 | 9264 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9265 | `{` |
|      - | 9266 | `	const char *zIn;` |
|      - | 9267 | `	int nLen;` |
|     36 | 9268 | `	if( nArg < 1 ){` |
|      - | 9269 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9270 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9271 | `		return PH7_OK;` |
|      - | 9272 | `	}` |
|      - | 9273 | `	/* Extract the input string */` |
|     36 | 9274 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9275 | `	if( nLen < 1 ){` |
|      - | 9276 | `		/* Nothing to process,return FALSE */` |
|      3 | 9277 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9278 | `		return PH7_OK;` |
|      - | 9279 | `	}` |
|      - | 9280 | `	/* Perform the BASE64 decoding */` |
|     34 | 9281 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9282 | `	return PH7_OK;` |
|     19 | 9283 | `}` |
|      - | 9284 | `/*` |
|      - | 9285 | ` * string urlencode(string $str)` |
|      - | 9286 | ` *  URL encoding` |
|      - | 9287 | ` * Parameter` |
|      - | 9288 | ` *  $data` |
|      - | 9289 | ` *   Input string.` |
|      - | 9290 | ` * Return` |
|      - | 9291 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9292 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9293 | ` *  encoded as plus (+) signs.` |
|      - | 9294 | ` */` |
|      4 | 9295 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9296 | `{` |
|      - | 9297 | `	const char *zIn;` |
|      - | 9298 | `	int nLen;` |
|      5 | 9299 | `	if( nArg < 1 ){` |
|      - | 9300 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9301 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9302 | `		return PH7_OK;` |
|      - | 9303 | `	}` |
|      - | 9304 | `	/* Extract the input string */` |
|      5 | 9305 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 9306 | `	if( nLen < 1 ){` |
|      - | 9307 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9308 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9309 | `		return PH7_OK;` |
|      - | 9310 | `	}` |
|      - | 9311 | `	/* Perform the URL encoding */` |
|      5 | 9312 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 9313 | `	return PH7_OK;` |
|      3 | 9314 | `}` |
|      - | 9315 | `/*` |
|      - | 9316 | ` * string urldecode(string $str)` |
|      - | 9317 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9318 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9319 | ` * Parameter` |
|      - | 9320 | ` *  $data` |
|      - | 9321 | ` *    Input string.` |
|      - | 9322 | ` * Return` |
|      - | 9323 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9324 | ` */` |
|      6 | 9325 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9326 | `{` |
|      - | 9327 | `	const char *zIn;` |
|      - | 9328 | `	int nLen;` |
|      7 | 9329 | `	if( nArg < 1 ){` |
|      - | 9330 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9331 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9332 | `		return PH7_OK;` |
|      - | 9333 | `	}` |
|      - | 9334 | `	/* Extract the input string */` |
|      7 | 9335 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9336 | `	if( nLen < 1 ){` |
|      - | 9337 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9338 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9339 | `		return PH7_OK;` |
|      - | 9340 | `	}` |
|      - | 9341 | `	/* Perform the URL decoding */` |
|      7 | 9342 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 9343 | `	return PH7_OK;` |
|      4 | 9344 | `}` |
|      - | 9345 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9346 | `/* Table of the built-in functions */` |
|      - | 9347 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9348 | `	   /* Variable handling functions */` |
|      - | 9349 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9350 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9351 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9352 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9353 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9354 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9355 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9356 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9357 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9358 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9359 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9360 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9361 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9362 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9363 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9364 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9365 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9366 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9367 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9368 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9369 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9370 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9371 | `	   /* Math functions */` |
|      - | 9372 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9373 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9374 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9375 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9376 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9377 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9378 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9379 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9380 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9381 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9382 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9383 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9384 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9385 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9386 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9387 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9388 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9389 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9390 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9391 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9392 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9393 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9394 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9395 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9396 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9397 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9398 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9399 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9400 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9401 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9402 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9403 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9404 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9405 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9406 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9407 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9408 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9409 | `	   /* String handling functions */` |
|      - | 9410 |  |
|      - | 9411 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9412 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9413 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9414 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9415 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9416 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9417 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9418 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9419 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9420 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9421 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9422 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9423 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9424 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9425 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9426 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9427 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9428 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9429 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9430 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9431 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9432 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9433 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9434 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9435 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9436 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9437 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9438 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9439 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9440 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9441 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9442 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9443 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9444 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9445 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9446 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 9447 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9448 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 9449 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9450 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9451 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9452 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9453 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9454 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9455 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9456 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9457 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9458 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9459 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9460 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9461 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9462 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9463 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9464 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9465 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9466 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9467 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9468 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9469 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9470 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9471 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9472 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9473 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9474 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9475 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9476 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9477 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9478 |  |
|      - | 9479 |  |
|      - | 9480 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9481 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9482 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9483 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9484 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9485 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9486 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9487 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9488 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9489 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9490 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9491 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9492 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9493 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9494 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9495 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9496 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9497 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9498 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9499 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9500 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9501 |  |
|      - | 9502 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9503 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9504 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9505 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9506 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9507 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9508 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9509 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9510 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9511 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9512 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9513 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9514 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9515 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9516 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9517 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9518 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9519 |  |
|      - | 9520 | `	         /* Ctype functions */` |
|      - | 9521 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9522 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9523 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9524 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9525 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9526 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9527 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9528 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9529 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9530 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9531 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9532 | `	         /* Time functions */` |
|      - | 9533 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9534 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9535 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9536 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9537 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9538 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9539 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9540 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9541 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9542 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9543 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9544 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9545 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9546 | `	        /* URL functions */` |
|      - | 9547 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9548 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9549 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9550 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9551 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9552 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9553 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9554 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9555 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9556 | `};` |
|      - | 9557 | `/*` |
|      - | 9558 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9559 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9560 | ` */` |
|   3528 | 9561 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9562 | `{` |
|      - | 9563 | `	sxu32 n;` |
| 620933 | 9564 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 617405 | 9565 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 308705 | 9566 | `	}` |
|      - | 9567 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3533 | 9568 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9569 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3533 | 9570 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3533 | 9571 | `}` |
|      - | 9572 |  |
