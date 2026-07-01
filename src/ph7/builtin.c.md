# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3656/4129 lines (88.54%)

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
|    196 |   42 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   43 |  |
|    197 |   44 | `	int res = 0; /* Assume false by default */` |
|    197 |   45 | `	if( nArg > 0 ){` |
|    195 |   46 | `		res = ph7_value_is_float(apArg[0]);` |
|     97 |   47 | `	}` |
|      - |   48 | `	/* Query result */` |
|    197 |   49 | `	ph7_result_bool(pCtx,res);` |
|    197 |   50 | `	return PH7_OK;` |
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
|    632 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |   63 |  |
|    635 |   64 | `	int res = 0; /* Assume false by default */` |
|    635 |   65 | `	if( nArg > 0 ){` |
|      - |   66 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   67 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   68 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    633 |   69 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    315 |   70 | `	}` |
|      - |   71 | `	/* Query result */` |
|    635 |   72 | `	ph7_result_bool(pCtx,res);` |
|    635 |   73 | `	return PH7_OK;` |
|      3 |   74 |  |
|      - |   75 | `/*` |
|      - |   76 | ` * bool is_string($var)` |
|      - |   77 | ` *  Finds out whether a variable is a string.` |
|      - |   78 | ` * Parameters` |
|      - |   79 | ` *   $var: The variable being evaluated.` |
|      - |   80 | ` * Return` |
|      - |   81 | ` *  TRUE if var is string. False otherwise.` |
|      - |   82 | ` */` |
|    126 |   83 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   84 |  |
|    127 |   85 | `	int res = 0; /* Assume false by default */` |
|    127 |   86 | `	if( nArg > 0 ){` |
|    125 |   87 | `		res = ph7_value_is_string(apArg[0]);` |
|     62 |   88 | `	}` |
|      - |   89 | `	/* Query result */` |
|    127 |   90 | `	ph7_result_bool(pCtx,res);` |
|    127 |   91 | `	return PH7_OK;` |
|      1 |   92 |  |
|      - |   93 | `/*` |
|      - |   94 | ` * bool is_null($var)` |
|      - |   95 | ` *  Finds out whether a variable is NULL.` |
|      - |   96 | ` * Parameters` |
|      - |   97 | ` *   $var: The variable being evaluated.` |
|      - |   98 | ` * Return` |
|      - |   99 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  100 | ` */` |
|     92 |  101 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  102 |  |
|     96 |  103 | `	int res = 0; /* Assume false by default */` |
|     96 |  104 | `	if( nArg > 0 ){` |
|     94 |  105 | `		res = ph7_value_is_null(apArg[0]);` |
|     45 |  106 | `	}` |
|      - |  107 | `	/* Query result */` |
|     96 |  108 | `	ph7_result_bool(pCtx,res);` |
|     96 |  109 | `	return PH7_OK;` |
|      4 |  110 |  |
|      - |  111 | `/*` |
|      - |  112 | ` * bool is_numeric($var)` |
|      - |  113 | ` *  Find out whether a variable is NULL.` |
|      - |  114 | ` * Parameters` |
|      - |  115 | ` *  $var: The variable being evaluated.` |
|      - |  116 | ` * Return` |
|      - |  117 | ` *  True if var is numeric. False otherwise.` |
|      - |  118 | ` */` |
|     38 |  119 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  120 |  |
|     43 |  121 | `	int res = 0; /* Assume false by default */` |
|     43 |  122 | `	if( nArg > 0 ){` |
|     41 |  123 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     18 |  124 | `	}` |
|      - |  125 | `	/* Query result */` |
|     43 |  126 | `	ph7_result_bool(pCtx,res);` |
|     43 |  127 | `	return PH7_OK;` |
|      5 |  128 |  |
|      - |  129 | `/*` |
|      - |  130 | ` * bool is_scalar($var)` |
|      - |  131 | ` *  Find out whether a variable is a scalar.` |
|      - |  132 | ` * Parameters` |
|      - |  133 | ` *  $var: The variable being evaluated.` |
|      - |  134 | ` * Return` |
|      - |  135 | ` *  True if var is scalar. False otherwise.` |
|      - |  136 | ` */` |
|     14 |  137 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  138 |  |
|     15 |  139 | `	int res = 0; /* Assume false by default */` |
|     15 |  140 | `	if( nArg > 0 ){` |
|     13 |  141 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  142 | `	}` |
|      - |  143 | `	/* Query result */` |
|     15 |  144 | `	ph7_result_bool(pCtx,res);` |
|     15 |  145 | `	return PH7_OK;` |
|      1 |  146 |  |
|      - |  147 | `/*` |
|      - |  148 | ` * bool is_array($var)` |
|      - |  149 | ` *  Find out whether a variable is an array.` |
|      - |  150 | ` * Parameters` |
|      - |  151 | ` *  $var: The variable being evaluated.` |
|      - |  152 | ` * Return` |
|      - |  153 | ` *  True if var is an array. False otherwise.` |
|      - |  154 | ` */` |
|    242 |  155 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  156 |  |
|    246 |  157 | `	int res = 0; /* Assume false by default */` |
|    246 |  158 | `	if( nArg > 0 ){` |
|    244 |  159 | `		res = ph7_value_is_array(apArg[0]);` |
|    120 |  160 | `	}` |
|      - |  161 | `	/* Query result */` |
|    246 |  162 | `	ph7_result_bool(pCtx,res);` |
|    246 |  163 | `	return PH7_OK;` |
|      4 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * bool is_object($var)` |
|      - |  167 | ` *  Find out whether a variable is an object.` |
|      - |  168 | ` * Parameters` |
|      - |  169 | ` *  $var: The variable being evaluated.` |
|      - |  170 | ` * Return` |
|      - |  171 | ` *  True if var is an object. False otherwise.` |
|      - |  172 | ` */` |
|     22 |  173 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  174 |  |
|     23 |  175 | `	int res = 0; /* Assume false by default */` |
|     23 |  176 | `	if( nArg > 0 ){` |
|     21 |  177 | `		res = ph7_value_is_object(apArg[0]);` |
|     10 |  178 | `	}` |
|      - |  179 | `	/* Query result */` |
|     23 |  180 | `	ph7_result_bool(pCtx,res);` |
|     23 |  181 | `	return PH7_OK;` |
|      1 |  182 |  |
|      - |  183 | `/*` |
|      - |  184 | ` * bool is_resource($var)` |
|      - |  185 | ` *  Find out whether a variable is a resource.` |
|      - |  186 | ` * Parameters` |
|      - |  187 | ` *  $var: The variable being evaluated.` |
|      - |  188 | ` * Return` |
|      - |  189 | ` *  True if a resource. False otherwise.` |
|      - |  190 | ` */` |
|     60 |  191 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  192 |  |
|     64 |  193 | `	int res = 0; /* Assume false by default */` |
|     64 |  194 | `	if( nArg > 0 ){` |
|     62 |  195 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  196 | `	}` |
|     64 |  197 | `	ph7_result_bool(pCtx,res);` |
|     64 |  198 | `	return PH7_OK;` |
|      4 |  199 |  |
|      - |  200 | `/*` |
|      - |  201 | ` * float floatval($var)` |
|      - |  202 | ` *  Get float value of a variable.` |
|      - |  203 | ` * Parameter` |
|      - |  204 | ` *  $var: The variable being processed.` |
|      - |  205 | ` * Return` |
|      - |  206 | ` *  the float value of a variable.` |
|      - |  207 | ` */` |
|      6 |  208 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  209 |  |
|      7 |  210 | `	if( nArg < 1 ){` |
|      - |  211 | `		/* return 0.0 */` |
|      3 |  212 | `		ph7_result_double(pCtx,0);` |
|      2 |  213 | `	}else{` |
|      - |  214 | `		double dval;` |
|      - |  215 | `		/* Perform the cast */` |
|      5 |  216 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  217 | `		ph7_result_double(pCtx,dval);` |
|      - |  218 | `	}` |
|      7 |  219 | `	return PH7_OK;` |
|      1 |  220 |  |
|      - |  221 | `/*` |
|      - |  222 | ` * int intval($var)` |
|      - |  223 | ` *  Get integer value of a variable.` |
|      - |  224 | ` * Parameter` |
|      - |  225 | ` *  $var: The variable being processed.` |
|      - |  226 | ` * Return` |
|      - |  227 | ` *  the int value of a variable.` |
|      - |  228 | ` */` |
|     26 |  229 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  230 |  |
|     27 |  231 | `	if( nArg < 1 ){` |
|      - |  232 | `		/* return 0 */` |
|      3 |  233 | `		ph7_result_int(pCtx,0);` |
|      2 |  234 | `	}else{` |
|      - |  235 | `		sxi64 iVal;` |
|      - |  236 | `		/* Perform the cast */` |
|     25 |  237 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     25 |  238 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  239 | `	}` |
|     27 |  240 | `	return PH7_OK;` |
|      1 |  241 |  |
|      - |  242 | `/*` |
|      - |  243 | ` * string strval($var)` |
|      - |  244 | ` *  Get the string representation of a variable.` |
|      - |  245 | ` * Parameter` |
|      - |  246 | ` *  $var: The variable being processed.` |
|      - |  247 | ` * Return` |
|      - |  248 | ` *  the string value of a variable.` |
|      - |  249 | ` */` |
|      4 |  250 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  251 |  |
|      5 |  252 | `	if( nArg < 1 ){` |
|      - |  253 | `		/* return NULL */` |
|      3 |  254 | `		ph7_result_null(pCtx);` |
|      2 |  255 | `	}else{` |
|      - |  256 | `		const char *zVal;` |
|      3 |  257 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  258 | `		/* Perform the cast */` |
|      3 |  259 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  260 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  261 | `	}` |
|      5 |  262 | `	return PH7_OK;` |
|      1 |  263 |  |
|      - |  264 | `/*` |
|      - |  265 | ` * bool boolval($var)` |
|      - |  266 | ` *  Get the boolean value of a variable.` |
|      - |  267 | ` * Parameter` |
|      - |  268 | ` *  $var: The variable being processed.` |
|      - |  269 | ` * Return` |
|      - |  270 | ` *  the bool value of a variable.` |
|      - |  271 | ` */` |
|     16 |  272 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  273 |  |
|      - |  274 | `	int bVal;` |
|     18 |  275 | `	if( nArg != 1 ){` |
|      4 |  276 | `		return PH7_VmThrowException(pCtx,` |
|      - |  277 | `			"ArgumentCountError",` |
|      - |  278 | `			"boolval() expects exactly 1 argument, %d given",` |
|      1 |  279 | `			nArg` |
|      - |  280 | `			);` |
|      - |  281 | `	}` |
|      - |  282 | `	/* Perform the cast */` |
|     15 |  283 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  284 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  285 | `	return PH7_OK;` |
|     10 |  286 |  |
|      - |  287 | `/*` |
|      - |  288 | ` * bool empty($var)` |
|      - |  289 | ` *  Determine whether a variable is empty.` |
|      - |  290 | ` * Parameters` |
|      - |  291 | ` *   $var: The variable being checked.` |
|      - |  292 | ` * Return` |
|      - |  293 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  294 | ` */` |
|  27356 |  295 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  296 |  |
|  27361 |  297 | `	int res = 1; /* Assume empty by default */` |
|  27361 |  298 | `	if( nArg > 0 ){` |
|  27359 |  299 | `		res = ph7_value_is_empty(apArg[0]);` |
|  13677 |  300 | `	}` |
|  27361 |  301 | `	ph7_result_bool(pCtx,res);` |
|  27361 |  302 | `	return PH7_OK;` |
|      - |  303 |  |
|      5 |  304 |  |
|      - |  305 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  306 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  307 | `#endif` |
|      - |  308 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  309 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  310 | `#endif` |
|      - |  311 |  |
|      - |  312 | `/* Math functions moved to builtin_math.c */` |
|      - |  313 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  314 | `/*` |
|      - |  315 | ` * Section:` |
|      - |  316 | ` *    String handling Functions.` |
|      - |  317 | ` * Status:` |
|      - |  318 | ` *    Stable.` |
|      - |  319 | ` */` |
|      - |  320 | `/*` |
|      - |  321 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  322 | ` *  Return part of a string.` |
|      - |  323 | ` * Parameters` |
|      - |  324 | ` *  $string` |
|      - |  325 | ` *   The input string. Must be one character or longer.` |
|      - |  326 | ` * $start` |
|      - |  327 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  328 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  329 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  330 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  331 | ` *   from the end of string.` |
|      - |  332 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  333 | ` * $length` |
|      - |  334 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  335 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  336 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  337 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  338 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  339 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  340 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  341 | ` *   will be returned.` |
|      - |  342 | ` * Return` |
|      - |  343 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  344 | ` */` |
| 207032 |  345 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  346 |  |
|      - |  347 | `	const char *zSource,*zOfft;` |
|      - |  348 | `	int nOfft,nLen,nSrcLen;` |
| 207037 |  349 | `	if( nArg < 2 ){` |
|      - |  350 | `		/* return FALSE */` |
|      5 |  351 | `		ph7_result_bool(pCtx,0);` |
|      5 |  352 | `		return PH7_OK;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Extract the target string */` |
| 207033 |  355 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 207033 |  356 | `	if( nSrcLen < 1 ){` |
|      - |  357 | `		/* Empty string,return FALSE */` |
|  11731 |  358 | `		ph7_result_bool(pCtx,0);` |
|  11731 |  359 | `		return PH7_OK;` |
|      - |  360 | `	}` |
| 195307 |  361 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  362 | `	/* Extract the offset */` |
| 195307 |  363 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 195307 |  364 | `	if( nOfft < 0 ){` |
|  31781 |  365 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  31781 |  366 | `		if( zOfft < zSource ){` |
|      - |  367 | `			/* Invalid offset */` |
|      5 |  368 | `			ph7_result_bool(pCtx,0);` |
|      5 |  369 | `			return PH7_OK;` |
|      - |  370 | `		}` |
|  31777 |  371 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  31777 |  372 | `		nOfft = (int)(zOfft-zSource);` |
| 179417 |  373 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  374 | `		/* Invalid offset */` |
|    179 |  375 | `		ph7_result_bool(pCtx,0);` |
|    179 |  376 | `		return PH7_OK;` |
|    ! 0 |  377 | `	}else{` |
| 163357 |  378 | `		zOfft = &zSource[nOfft];` |
| 163357 |  379 | `		nLen = nSrcLen - nOfft;` |
|      - |  380 | `	}` |
| 195129 |  381 | `	if( nArg > 2 ){` |
|      - |  382 | `		/* Extract the length */` |
| 160627 |  383 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 160627 |  384 | `		if( nLen == 0 ){` |
|      - |  385 | `			/* Invalid length,return an empty string */` |
|      5 |  386 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  387 | `			return PH7_OK;` |
| 160623 |  388 | `		}else if( nLen < 0 ){` |
|  31769 |  389 | `			nLen = nSrcLen + nLen - nOfft;` |
|  31769 |  390 | `			if( nLen < 1 ){` |
|      - |  391 | `				/* Invalid  length */` |
|      3 |  392 | `				nLen = nSrcLen - nOfft;` |
|      1 |  393 | `			}` |
|  15882 |  394 | `		}` |
| 160623 |  395 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  396 | `			/* Invalid length */` |
|   4781 |  397 | `			nLen = nSrcLen - nOfft;` |
|   2388 |  398 | `		}` |
|  80309 |  399 | `	}` |
|      - |  400 | `	/* Return the substring */` |
| 195125 |  401 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 195125 |  402 | `	return PH7_OK;` |
| 103521 |  403 |  |
|      - |  404 | `/*` |
|      - |  405 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  406 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  407 | ` * Parameters` |
|      - |  408 | ` *  $main_str` |
|      - |  409 | ` *  The main string being compared.` |
|      - |  410 | ` *  $str` |
|      - |  411 | ` *   The secondary string being compared.` |
|      - |  412 | ` * $offset` |
|      - |  413 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  414 | ` *  the end of the string.` |
|      - |  415 | ` * $length` |
|      - |  416 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  417 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  418 | ` * $case_insensitivity` |
|      - |  419 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  420 | ` * Return` |
|      - |  421 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  422 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  423 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  424 | ` */` |
|     26 |  425 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  426 |  |
|      - |  427 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  428 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 |  429 | `	int iCase = 0;` |
|      - |  430 | `	int rc;` |
|     27 |  431 | `	if( nArg < 3 ){` |
|      - |  432 | `		/* Missing arguments,return FALSE */` |
|      5 |  433 | `		ph7_result_bool(pCtx,0);` |
|      5 |  434 | `		return PH7_OK;` |
|      - |  435 | `	}` |
|      - |  436 | `	/* Extract the target string */` |
|     23 |  437 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  438 | `	if( nSrcLen < 1 ){` |
|      - |  439 | `		/* Empty string,return FALSE */` |
|      3 |  440 | `		ph7_result_bool(pCtx,0);` |
|      3 |  441 | `		return PH7_OK;` |
|      - |  442 | `	}` |
|     21 |  443 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  444 | `	/* Extract the substring */` |
|     21 |  445 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  446 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  447 | `		/* Empty string,return FALSE */` |
|      3 |  448 | `		ph7_result_bool(pCtx,0);` |
|      3 |  449 | `		return PH7_OK;` |
|      - |  450 | `	}` |
|      - |  451 | `	/* Extract the offset */` |
|     19 |  452 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  453 | `	if( nOfft < 0 ){` |
|      5 |  454 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  455 | `		if( zOfft < zSource ){` |
|      - |  456 | `			/* Invalid offset */` |
|      3 |  457 | `			ph7_result_bool(pCtx,0);` |
|      3 |  458 | `			return PH7_OK;` |
|      - |  459 | `		}` |
|      3 |  460 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  461 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  462 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  463 | `		/* Invalid offset */` |
|      3 |  464 | `		ph7_result_bool(pCtx,0);` |
|      3 |  465 | `		return PH7_OK;` |
|    ! 0 |  466 | `	}else{` |
|     13 |  467 | `		zOfft = &zSource[nOfft];` |
|     13 |  468 | `		nLen = nSrcLen - nOfft;` |
|      - |  469 | `	}` |
|     15 |  470 | `	if( nArg > 3 ){` |
|      - |  471 | `		/* Extract the length */` |
|     13 |  472 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  473 | `		if( nLen < 1 ){` |
|      - |  474 | `			/* Invalid  length */` |
|      5 |  475 | `			ph7_result_int(pCtx,1);` |
|      5 |  476 | `			return PH7_OK;` |
|      9 |  477 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  478 | `			/* Invalid length */` |
|      3 |  479 | `			nLen = nSrcLen - nOfft;` |
|      1 |  480 | `		}` |
|      9 |  481 | `		if( nArg > 4 ){` |
|      - |  482 | `			/* Case-sensitive or not */` |
|      5 |  483 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  484 | `		}` |
|      4 |  485 | `	}` |
|      - |  486 | `	/* Perform the comparison */` |
|     11 |  487 | `	if( iCase ){` |
|      3 |  488 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  489 | `	}else{` |
|      9 |  490 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  491 | `	}` |
|      - |  492 | `	/* Comparison result */` |
|     11 |  493 | `	ph7_result_int(pCtx,rc);` |
|     11 |  494 | `	return PH7_OK;` |
|     14 |  495 |  |
|      - |  496 | `/*` |
|      - |  497 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  498 | ` *  Count the number of substring occurrences.` |
|      - |  499 | ` * Parameters` |
|      - |  500 | ` * $haystack` |
|      - |  501 | ` *   The string to search in` |
|      - |  502 | ` * $needle` |
|      - |  503 | ` *   The substring to search for` |
|      - |  504 | ` * $offset` |
|      - |  505 | ` *  The offset where to start counting` |
|      - |  506 | ` * $length (NOT USED)` |
|      - |  507 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  508 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  509 | ` * Return` |
|      - |  510 | ` *  Toral number of substring occurrences.` |
|      - |  511 | ` */` |
|     24 |  512 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  513 |  |
|      - |  514 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  515 | `	int nTextlen,nPatlen;` |
|     25 |  516 | `	int iCount = 0;` |
|      - |  517 | `	sxu32 nOfft;` |
|      - |  518 | `	sxi32 rc;` |
|     25 |  519 | `	if( nArg < 2 ){` |
|      - |  520 | `		/* Missing arguments */` |
|      5 |  521 | `		ph7_result_int(pCtx,0);` |
|      5 |  522 | `		return PH7_OK;` |
|      - |  523 | `	}` |
|      - |  524 | `	/* Point to the haystack */` |
|     21 |  525 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  526 | `	/* Point to the neddle */` |
|     21 |  527 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 |  528 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  529 | `		/* NOOP,return zero */` |
|      3 |  530 | `		ph7_result_int(pCtx,0);` |
|      3 |  531 | `		return PH7_OK;` |
|      - |  532 | `	}` |
|     19 |  533 | `	if( nArg > 2 ){` |
|      - |  534 | `		int iOfft;` |
|      - |  535 | `		/* Extract the offset */` |
|     15 |  536 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 |  537 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - |  538 | `			/* Invalid offset,return zero */` |
|      3 |  539 | `			ph7_result_int(pCtx,0);` |
|      3 |  540 | `			return PH7_OK;` |
|      - |  541 | `		}` |
|      - |  542 | `		/* Point to the desired offset */` |
|     13 |  543 | `		zText = &zText[iOfft];` |
|      - |  544 | `		/* Adjust length */` |
|     13 |  545 | `		nTextlen -= iOfft;` |
|      6 |  546 | `	}` |
|      - |  547 | `	/* Point to the end of the string */` |
|     17 |  548 | `	zEnd = &zText[nTextlen];` |
|     17 |  549 | `	if( nArg > 3 ){` |
|      - |  550 | `		int nLen;` |
|      - |  551 | `		/* Extract the length */` |
|     13 |  552 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  553 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - |  554 | `			/* Invalid length,return 0 */` |
|      7 |  555 | `			ph7_result_int(pCtx,0);` |
|      7 |  556 | `			return PH7_OK;` |
|      - |  557 | `		}` |
|      - |  558 | `		/* Adjust pointer */` |
|      7 |  559 | `		nTextlen = nLen;` |
|      7 |  560 | `		zEnd = &zText[nTextlen];` |
|      3 |  561 | `	}` |
|      - |  562 | `	/* Perform the search */` |
|     12 |  563 | `	for(;;){` |
|     25 |  564 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 |  565 | `		if( rc != SXRET_OK ){` |
|      - |  566 | `			/* Pattern not found,break immediately */` |
|      9 |  567 | `			break;` |
|      - |  568 | `		}` |
|      - |  569 | `		/* Increment counter and update the offset */` |
|     17 |  570 | `		iCount++;` |
|     17 |  571 | `		zText += nOfft + nPatlen;` |
|     17 |  572 | `		if( zText >= zEnd ){` |
|      3 |  573 | `			break;` |
|      - |  574 | `		}` |
|      1 |  575 | `	}` |
|      - |  576 | `	/* Pattern count */` |
|     11 |  577 | `	ph7_result_int(pCtx,iCount);` |
|     11 |  578 | `	return PH7_OK;` |
|     13 |  579 |  |
|      - |  580 | `/*` |
|      - |  581 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - |  582 | ` *   Split a string into smaller chunks.` |
|      - |  583 | ` * Parameters` |
|      - |  584 | ` *  $body` |
|      - |  585 | ` *   The string to be chunked.` |
|      - |  586 | ` * $chunklen` |
|      - |  587 | ` *   The chunk length.` |
|      - |  588 | ` * $end` |
|      - |  589 | ` *   The line ending sequence.` |
|      - |  590 | ` * Return` |
|      - |  591 | ` *  The chunked string or NULL on failure.` |
|      - |  592 | ` */` |
|     16 |  593 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  594 |  |
|     17 |  595 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - |  596 | `	int nSepLen,nChunkLen,nLen;` |
|     17 |  597 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  598 | `		/* Nothing to split,return null */` |
|      5 |  599 | `		ph7_result_null(pCtx);` |
|      5 |  600 | `		return PH7_OK;` |
|      - |  601 | `	}` |
|      - |  602 | `	/* initialize/Extract arguments */` |
|     13 |  603 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 |  604 | `	nChunkLen = 76;` |
|     13 |  605 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 |  606 | `	zEnd = &zIn[nLen];` |
|     13 |  607 | `	if( nArg > 1 ){` |
|      - |  608 | `		/* Chunk length */` |
|     13 |  609 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 |  610 | `		if( nChunkLen < 1 ){` |
|      - |  611 | `			/* Switch back to the default length */` |
|      3 |  612 | `			nChunkLen = 76;` |
|      1 |  613 | `		}` |
|     13 |  614 | `		if( nArg > 2 ){` |
|      - |  615 | `			/* Separator */` |
|      9 |  616 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 |  617 | `			if( nSepLen < 1 ){` |
|      - |  618 | `				/* Switch back to the default separator */` |
|      3 |  619 | `				zSep = "\r\n";` |
|      3 |  620 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 |  621 | `			}` |
|      4 |  622 | `		}` |
|      6 |  623 | `	}` |
|      - |  624 | `	/* Perform the requested operation */` |
|     13 |  625 | `	if( nChunkLen > nLen ){` |
|      - |  626 | `		/* Nothing to split,return the string and the separator */` |
|      9 |  627 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 |  628 | `		return PH7_OK;` |
|      - |  629 | `	}` |
|     17 |  630 | `	while( zIn < zEnd ){` |
|     13 |  631 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 |  632 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 |  633 | `		}` |
|      - |  634 | `		/* Append the chunk and the separator */` |
|     13 |  635 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - |  636 | `		/* Point beyond the chunk */` |
|     13 |  637 | `		zIn += nChunkLen;` |
|      1 |  638 | `	}` |
|      5 |  639 | `	return PH7_OK;` |
|      9 |  640 |  |
|      - |  641 | `/*` |
|      - |  642 | ` * string addslashes(string $str)` |
|      - |  643 | ` *  Quote string with slashes.` |
|      - |  644 | ` *  Returns a string with backslashes before characters that need` |
|      - |  645 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  646 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  647 | ` * Parameter` |
|      - |  648 | ` *  str: The string to be escaped.` |
|      - |  649 | ` * Return` |
|      - |  650 | ` *  Returns the escaped string` |
|      - |  651 | ` */` |
|     24 |  652 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  653 |  |
|      - |  654 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  655 | `	int nLen;` |
|      - |  656 | `	/* PHP enforces exactly one argument. */` |
|     28 |  657 | `	if( nArg != 1 ){` |
|      8 |  658 | `		return PH7_VmThrowException(pCtx,` |
|      - |  659 | `			"ArgumentCountError",` |
|      - |  660 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 |  661 | `			nArg` |
|      - |  662 | `			);` |
|      - |  663 | `	}` |
|      - |  664 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - |  665 | `	 * types still produce a TypeError. */` |
|     22 |  666 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 |  667 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  668 | `			E_DEPRECATED,` |
|      - |  669 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  670 | `			);` |
|      - |  671 | `		/* fall through so conversion below yields empty string */` |
|      1 |  672 | `	}` |
|      - |  673 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 |  674 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 |  675 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 |  676 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 |  677 | `		return PH7_VmThrowException(pCtx,` |
|      - |  678 | `			"TypeError",` |
|      - |  679 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  680 | `			ph7_type_name(apArg[0])` |
|      - |  681 | `			);` |
|      - |  682 | `	}` |
|      - |  683 | `	/* Convert to string representation first and obtain length. */` |
|     19 |  684 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 |  685 | `	if( nLen < 1 ){` |
|      - |  686 | `		/* Return the empty string */` |
|      5 |  687 | `		ph7_result_string(pCtx,"",0);` |
|      5 |  688 | `		return PH7_OK;` |
|      - |  689 | `	}` |
|     15 |  690 | `	zEnd = &zIn[nLen];` |
|     15 |  691 | `	zCur = 0; /* cc warning */` |
|     20 |  692 | `	for(;;){` |
|     41 |  693 | `		if( zIn >= zEnd ){` |
|      - |  694 | `			/* No more input */` |
|     15 |  695 | `			break;` |
|      - |  696 | `		}` |
|     27 |  697 | `		zCur = zIn;` |
|      - |  698 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 |  699 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 |  700 | `			zIn++;` |
|      1 |  701 | `		}` |
|     27 |  702 | `		if( zIn > zCur ){` |
|      - |  703 | `			/* Append raw contents */` |
|     23 |  704 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 |  705 | `		}` |
|     27 |  706 | `		if( zIn < zEnd ){` |
|     17 |  707 | `			int c = zIn[0];` |
|     17 |  708 | `			if( c == '\0' ){` |
|      - |  709 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 |  710 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 |  711 | `			}else{` |
|     15 |  712 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  713 | `			}` |
|      8 |  714 | `		}` |
|     27 |  715 | `		zIn++;` |
|      1 |  716 | `	}` |
|     15 |  717 | `	return PH7_OK;` |
|     16 |  718 |  |
|      - |  719 | `/*` |
|      - |  720 | ` * Check if the given character is present in the given mask.` |
|      - |  721 | ` * Return TRUE if present. FALSE otherwise.` |
|      - |  722 | ` */` |
|    124 |  723 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 |  724 |  |
|    125 |  725 | `	const char *zEnd = &zMask[nLen];` |
|    555 |  726 | `	while( zMask < zEnd ){` |
|      - |  727 | `		/* Support range syntax A..Z where A and Z are literal bytes.  The` |
|      - |  728 | `		 * original PH7 implementation ignored ranges; tests rely on them so` |
|      - |  729 | `		 * provide a simple on-the-fly check here. */` |
|    475 |  730 | `		if( zMask + 3 < zEnd && zMask[1] == '.' && zMask[2] == '.' ){` |
|      3 |  731 | `			int lo = (unsigned char)zMask[0];` |
|      3 |  732 | `			int hi = (unsigned char)zMask[3];` |
|      3 |  733 | `			if( lo > hi ){` |
|    ! 0 |  734 | `				int tmp = lo; lo = hi; hi = tmp;` |
|    ! 0 |  735 | `			}` |
|      3 |  736 | `			if( c >= lo && c <= hi ){` |
|      3 |  737 | `				return 1;` |
|      - |  738 | `			}` |
|      - |  739 | `			/* consume the range specifier */` |
|    ! 0 |  740 | `			zMask += 4;` |
|    ! 0 |  741 | `			continue;` |
|      - |  742 | `		}` |
|    473 |  743 | `		if( zMask[0] == c ){` |
|      - |  744 | `			/* Character present,return TRUE */` |
|     43 |  745 | `			return 1;` |
|      - |  746 | `		}` |
|      - |  747 | `		/* Advance the pointer */` |
|    431 |  748 | `		zMask++;` |
|      1 |  749 | `	}` |
|      - |  750 | `	/* Not present */` |
|     81 |  751 | `	return 0;` |
|     63 |  752 |  |
|      - |  753 | `/*` |
|      - |  754 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  755 | ` *  Quote string with slashes in a C style.` |
|      - |  756 | ` * Parameter` |
|      - |  757 | ` *  $str:` |
|      - |  758 | ` *    The string to be escaped.` |
|      - |  759 | ` *  $charlist:` |
|      - |  760 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  761 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  762 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  763 | ` * Return` |
|      - |  764 | ` *  Returns the escaped string.` |
|      - |  765 | ` * Note:` |
|      - |  766 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - |  767 | ` */` |
|     34 |  768 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  769 |  |
|      - |  770 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  771 | `	int nLen,nMask;` |
|      - |  772 | `	/* PHP enforces exactly two arguments. */` |
|     39 |  773 | `	if( nArg != 2 ){` |
|      8 |  774 | `		return PH7_VmThrowException(pCtx,` |
|      - |  775 | `			"ArgumentCountError",` |
|      - |  776 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  777 | `			nArg` |
|      - |  778 | `			);` |
|      - |  779 | `	}` |
|      - |  780 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  781 | `	 * treated as the empty string (PHP 8.1). */` |
|     33 |  782 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  783 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  784 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  785 | `			E_DEPRECATED,` |
|      - |  786 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  787 | `			);` |
|      - |  788 | `		/* treat as empty string; fall through to conversion logic */` |
|     56 |  789 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 |  790 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     26 |  791 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  792 | `		return PH7_VmThrowException(pCtx,` |
|      - |  793 | `			"TypeError",` |
|      - |  794 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  795 | `			ph7_type_name(apArg[0])` |
|      - |  796 | `			);` |
|      - |  797 | `	}` |
|      - |  798 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  799 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  800 | `	 * trigger a TypeError. */` |
|     30 |  801 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  802 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  803 | `			E_DEPRECATED,` |
|      - |  804 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  805 | `			);` |
|      - |  806 | `		/* allow through so it becomes empty string below */` |
|     52 |  807 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     38 |  808 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     24 |  809 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  810 | `		return PH7_VmThrowException(pCtx,` |
|      - |  811 | `			"TypeError",` |
|      - |  812 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  813 | `			ph7_type_name(apArg[1])` |
|      - |  814 | `			);` |
|      - |  815 | `	}` |
|      - |  816 | `	/* Extract the string to process */` |
|     27 |  817 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  818 | `	/* NULL would never reach here due to the check above. */` |
|     27 |  819 | `	if( nLen < 1 ){` |
|      - |  820 | `		/* Empty string returns itself. */` |
|      5 |  821 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  822 | `		return PH7_OK;` |
|      - |  823 | `	}` |
|      - |  824 | `	/* Extract the desired mask */` |
|     23 |  825 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     23 |  826 | `	zEnd = &zIn[nLen];` |
|     23 |  827 | `	zCur = 0; /* cc warning */` |
|     29 |  828 | `	for(;;){` |
|     59 |  829 | `		if( zIn >= zEnd ){` |
|      - |  830 | `			/* No more input */` |
|     23 |  831 | `			break;` |
|      - |  832 | `		}` |
|     37 |  833 | `		zCur = zIn;` |
|     91 |  834 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     55 |  835 | `			zIn++;` |
|      1 |  836 | `		}` |
|     37 |  837 | `		if( zIn > zCur ){` |
|      - |  838 | `			/* Append raw contents */` |
|     33 |  839 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 |  840 | `		}` |
|     37 |  841 | `		if( zIn < zEnd ){` |
|      - |  842 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  843 | `			 * on platforms where char is signed. */` |
|     19 |  844 | `			int c = (unsigned char)zIn[0];` |
|      - |  845 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  846 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  847 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     19 |  848 | `			if( c == '\n' ){` |
|      3 |  849 | `				ph7_result_string(pCtx,"\\n",2);` |
|     18 |  850 | `			}else if( c == '\r' ){` |
|      3 |  851 | `				ph7_result_string(pCtx,"\\r",2);` |
|     16 |  852 | `			}else if( c == '\t' ){` |
|      3 |  853 | `				ph7_result_string(pCtx,"\\t",2);` |
|     14 |  854 | `			}else if( c == '\v' ){` |
|      3 |  855 | `				ph7_result_string(pCtx,"\\v",2);` |
|     12 |  856 | `			}else if( c == '\f' ){` |
|      3 |  857 | `				ph7_result_string(pCtx,"\\f",2);` |
|     10 |  858 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  859 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  860 | `				 * octal escapes (\001 not \1). */` |
|      7 |  861 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  862 | `			}else{` |
|      3 |  863 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  864 | `			}` |
|      9 |  865 | `		}` |
|     37 |  866 | `		zIn++;` |
|      1 |  867 | `	}` |
|     23 |  868 | `	return PH7_OK;` |
|     22 |  869 |  |
|      - |  870 | `/*` |
|      - |  871 | ` * string quotemeta(string $str)` |
|      - |  872 | ` *  Quote meta characters.` |
|      - |  873 | ` * Parameter` |
|      - |  874 | ` *  $str:` |
|      - |  875 | ` *    The string to be escaped.` |
|      - |  876 | ` * Return` |
|      - |  877 | ` *  Returns the escaped string.` |
|      - |  878 | `*/` |
|     10 |  879 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  880 |  |
|      - |  881 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  882 | `	int nLen;` |
|     11 |  883 | `	if( nArg < 1 ){` |
|      - |  884 | `		/* Nothing to process,retun NULL */` |
|      3 |  885 | `		ph7_result_null(pCtx);` |
|      3 |  886 | `		return PH7_OK;` |
|      - |  887 | `	}` |
|      - |  888 | `	/* Extract the string to process */` |
|      9 |  889 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 |  890 | `	if( nLen < 1 ){` |
|      - |  891 | `		/* Return the empty string */` |
|      3 |  892 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  893 | `		return PH7_OK;` |
|      - |  894 | `	}` |
|      7 |  895 | `	zEnd = &zIn[nLen];` |
|      7 |  896 | `	zCur = 0; /* cc warning */` |
|     17 |  897 | `	for(;;){` |
|     35 |  898 | `		if( zIn >= zEnd ){` |
|      - |  899 | `			/* No more input */` |
|      7 |  900 | `			break;` |
|      - |  901 | `		}` |
|     29 |  902 | `		zCur = zIn;` |
|     55 |  903 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 |  904 | `			zIn++;` |
|      1 |  905 | `		}` |
|     29 |  906 | `		if( zIn > zCur ){` |
|      - |  907 | `			/* Append raw contents */` |
|     11 |  908 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 |  909 | `		}` |
|     29 |  910 | `		if( zIn < zEnd ){` |
|     27 |  911 | `			int c = zIn[0];` |
|     27 |  912 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 |  913 | `		}` |
|     29 |  914 | `		zIn++;` |
|      1 |  915 | `	}` |
|      7 |  916 | `	return PH7_OK;` |
|      6 |  917 |  |
|      - |  918 | `/*` |
|      - |  919 | ` * string stripslashes(string $str)` |
|      - |  920 | ` *  Un-quotes a quoted string.` |
|      - |  921 | ` *  Returns a string with backslashes before characters that need` |
|      - |  922 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  923 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  924 | ` * Parameter` |
|      - |  925 | ` *  $str` |
|      - |  926 | ` *   The input string.` |
|      - |  927 | ` * Return` |
|      - |  928 | ` *  Returns a string with backslashes stripped off.` |
|      - |  929 | ` */` |
|      8 |  930 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  931 |  |
|      - |  932 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  933 | `	int nLen;` |
|      9 |  934 | `	if( nArg < 1 ){` |
|      - |  935 | `		/* Nothing to process,retun NULL */` |
|      3 |  936 | `		ph7_result_null(pCtx);` |
|      3 |  937 | `		return PH7_OK;` |
|      - |  938 | `	}` |
|      - |  939 | `	/* Extract the string to process */` |
|      7 |  940 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  941 | `	if( zIn == 0 ){` |
|    ! 0 |  942 | `		ph7_result_null(pCtx);` |
|    ! 0 |  943 | `		return PH7_OK;` |
|      - |  944 | `	}` |
|      7 |  945 | `	zEnd = &zIn[nLen];` |
|      7 |  946 | `	zCur = 0; /* cc warning */` |
|      - |  947 | `	/* Encode the string */` |
|      4 |  948 | `	for(;;){` |
|      9 |  949 | `		if( zIn >= zEnd ){` |
|      - |  950 | `			/* No more input */` |
|      5 |  951 | `			break;` |
|      - |  952 | `		}` |
|      5 |  953 | `		zCur = zIn;` |
|     17 |  954 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  955 | `			zIn++;` |
|      1 |  956 | `		}` |
|      5 |  957 | `		if( zIn > zCur ){` |
|      - |  958 | `			/* Append raw contents */` |
|      5 |  959 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 |  960 | `		}` |
|      5 |  961 | `		if( &zIn[1] < zEnd ){` |
|      3 |  962 | `			int c = zIn[1];` |
|      3 |  963 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - |  964 | `				/* Ignore the backslash */` |
|      3 |  965 | `				zIn++;` |
|      1 |  966 | `			}` |
|      2 |  967 | `		}else{` |
|      3 |  968 | `			break;` |
|      - |  969 | `		}` |
|      1 |  970 | `	}` |
|      7 |  971 | `	return PH7_OK;` |
|      5 |  972 |  |
|      - |  973 | `/*` |
|      - |  974 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - |  975 | ` *  HTML escaping of special characters.` |
|      - |  976 | ` *  The translations performed are:` |
|      - |  977 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - |  978 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - |  979 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - |  980 | ` *   '<' (less than) ==> '&lt;'` |
|      - |  981 | ` *   '>' (greater than) ==> '&gt;'` |
|      - |  982 | ` * Parameters` |
|      - |  983 | ` *  $string` |
|      - |  984 | ` *   The string being converted.` |
|      - |  985 | ` * $flags` |
|      - |  986 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - |  987 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - |  988 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - |  989 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - |  990 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - |  991 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - |  992 | ` * $charset` |
|      - |  993 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - |  994 | ` * Return` |
|      - |  995 | ` *  The escaped string or NULL on failure.` |
|      - |  996 | ` */` |
|     20 |  997 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  998 |  |
|      - |  999 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1000 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1001 | `	int nLen,c;` |
|     21 | 1002 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1003 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1004 | `		ph7_result_null(pCtx);` |
|      9 | 1005 | `		return PH7_OK;` |
|      - | 1006 | `	}` |
|      - | 1007 | `	/* Extract the target string */` |
|     13 | 1008 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1009 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1010 | `	if( nLen == 0 ){` |
|      3 | 1011 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1012 | `		return PH7_OK;` |
|      - | 1013 | `	}` |
|     11 | 1014 | `	zEnd = &zIn[nLen];` |
|      - | 1015 | `	/* Extract the flags if available */` |
|     11 | 1016 | `	if( nArg > 1 ){` |
|      9 | 1017 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1018 | `		if( iFlags < 0 ){` |
|      3 | 1019 | `			iFlags = 0x01\|0x40;` |
|      1 | 1020 | `		}` |
|      4 | 1021 | `	}` |
|      - | 1022 | `	/* Perform the requested operation */` |
|     23 | 1023 | `	for(;;){` |
|     47 | 1024 | `		if( zIn >= zEnd ){` |
|      9 | 1025 | `			break;` |
|      - | 1026 | `		}` |
|     39 | 1027 | `		zCur = zIn;` |
|     83 | 1028 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1029 | `			zIn++;` |
|      1 | 1030 | `		}` |
|     39 | 1031 | `		if( zCur < zIn ){` |
|      - | 1032 | `			/* Append the raw string verbatim */` |
|     17 | 1033 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1034 | `		}` |
|     39 | 1035 | `		if( zIn >= zEnd ){` |
|      3 | 1036 | `			break;` |
|      - | 1037 | `		}` |
|     37 | 1038 | `		c = zIn[0];` |
|     37 | 1039 | `		if( c == '&' ){` |
|      - | 1040 | `			/* Expand '&amp;' */` |
|      9 | 1041 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1042 | `		}else if( c == '<' ){` |
|      - | 1043 | `			/* Expand '&lt;' */` |
|      7 | 1044 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1045 | `		}else if( c == '>' ){` |
|      - | 1046 | `			/* Expand '&gt;' */` |
|      9 | 1047 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1048 | `		}else if( c == '\'' ){` |
|      5 | 1049 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1050 | `				/* Expand '&#039;' */` |
|      5 | 1051 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1052 | `			}else{` |
|      - | 1053 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1054 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1055 | `			}` |
|     13 | 1056 | `		}else if( c == '"' ){` |
|     11 | 1057 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1058 | `				/* Expand '&quot;' */` |
|      7 | 1059 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1060 | `			}else{` |
|      - | 1061 | `				/* Leave the double quote untouched */` |
|      5 | 1062 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1063 | `			}` |
|      5 | 1064 | `		}` |
|      - | 1065 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1066 | `		zIn++;` |
|      1 | 1067 | `	}` |
|     11 | 1068 | `	return PH7_OK;` |
|     11 | 1069 |  |
|      - | 1070 | `/*` |
|      - | 1071 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1072 | ` *  Unescape HTML entities.` |
|      - | 1073 | ` * Parameters` |
|      - | 1074 | ` *  $string` |
|      - | 1075 | ` *   The string to decode` |
|      - | 1076 | ` *  $quote_style` |
|      - | 1077 | ` *    The quote style. One of the following constants:` |
|      - | 1078 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1079 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1080 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1081 | ` * Return` |
|      - | 1082 | ` *  The unescaped string or NULL on failure.` |
|      - | 1083 | ` */` |
|     16 | 1084 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1085 |  |
|      - | 1086 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1087 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1088 | `	int nLen,nJump;` |
|     17 | 1089 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1090 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1091 | `		ph7_result_null(pCtx);` |
|      7 | 1092 | `		return PH7_OK;` |
|      - | 1093 | `	}` |
|      - | 1094 | `	/* Extract the target string */` |
|     11 | 1095 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1096 | `	zEnd = &zIn[nLen];` |
|      - | 1097 | `	/* Extract the flags if available */` |
|     11 | 1098 | `	if( nArg > 1 ){` |
|      7 | 1099 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1100 | `		if( iFlags < 0 ){` |
|      3 | 1101 | `			iFlags = 0x01;` |
|      1 | 1102 | `		}` |
|      3 | 1103 | `	}` |
|      - | 1104 | `	/* Perform the requested operation */` |
|     15 | 1105 | `	for(;;){` |
|     31 | 1106 | `		if( zIn >= zEnd ){` |
|     11 | 1107 | `			break;` |
|      - | 1108 | `		}` |
|     21 | 1109 | `		zCur = zIn;` |
|     51 | 1110 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1111 | `			zIn++;` |
|      1 | 1112 | `		}` |
|     21 | 1113 | `		if( zCur < zIn ){` |
|      - | 1114 | `			/* Append the raw string verbatim */` |
|      9 | 1115 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1116 | `		}` |
|     21 | 1117 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1118 | `		nJump = (int)sizeof(char);` |
|     21 | 1119 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1120 | `			/* &amp; ==> '&' */` |
|      3 | 1121 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1122 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1123 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1124 | `			/* &lt; ==> < */` |
|      3 | 1125 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1126 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1127 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1128 | `			/* &gt; ==> '>' */` |
|      3 | 1129 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1130 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1131 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1132 | `			/* &quot; ==> '"' */` |
|     13 | 1133 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1134 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1135 | `			}else{` |
|      - | 1136 | `				/* Leave untouched */` |
|      5 | 1137 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1138 | `			}` |
|     13 | 1139 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1140 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1141 | `			/* &#039; ==> ''' */` |
|      3 | 1142 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1143 | `				/* Expand ''' */` |
|      3 | 1144 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1145 | `			}else{` |
|      - | 1146 | `				/* Leave untouched */` |
|    ! 0 | 1147 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1148 | `			}` |
|      3 | 1149 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1150 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1151 | `			/* expand '&' */` |
|    ! 0 | 1152 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1153 | `		}else{` |
|      - | 1154 | `			/* No more input to process */` |
|    ! 0 | 1155 | `			break;` |
|      - | 1156 | `		}` |
|     21 | 1157 | `		zIn += nJump;` |
|      1 | 1158 | `	}` |
|     11 | 1159 | `	return PH7_OK;` |
|      9 | 1160 |  |
|      - | 1161 | `/* HTML encoding/Decoding table` |
|      - | 1162 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1163 | ` */` |
|      - | 1164 | `static const char *azHtmlEscape[] = {` |
|      - | 1165 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1166 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1167 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1168 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1169 | ` };` |
|      - | 1170 | `/*` |
|      - | 1171 | ` * array get_html_translation_table(void)` |
|      - | 1172 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1173 | ` * Parameters` |
|      - | 1174 | ` *  None` |
|      - | 1175 | ` * Return` |
|      - | 1176 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1177 | ` */` |
|      4 | 1178 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1179 |  |
|      - | 1180 | `	ph7_value *pArray,*pValue;` |
|      - | 1181 | `	sxu32 n;` |
|      - | 1182 | `	/* Element value */` |
|      5 | 1183 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1184 | `	if( pValue == 0 ){` |
|    ! 0 | 1185 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1186 | `		SXUNUSED(apArg);` |
|      - | 1187 | `		/* Return NULL */` |
|    ! 0 | 1188 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1189 | `		return PH7_OK;` |
|      - | 1190 | `	}` |
|      - | 1191 | `	/* Create a new array */` |
|      5 | 1192 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1193 | `	if( pArray == 0 ){` |
|      - | 1194 | `		/* Return NULL */` |
|    ! 0 | 1195 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1196 | `		return PH7_OK;` |
|      - | 1197 | `	}` |
|      - | 1198 | `	/* Make the table */` |
|     85 | 1199 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1200 | `		/* Prepare the value */` |
|     81 | 1201 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1202 | `		/* Insert the value */` |
|     81 | 1203 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1204 | `		/* Reset the string cursor */` |
|     81 | 1205 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1206 | `	}` |
|      - | 1207 | `	/*` |
|      - | 1208 | `	 * Return the array.` |
|      - | 1209 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1210 | `	 * released upon we return from this function.` |
|      - | 1211 | `	 */` |
|      5 | 1212 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1213 | `	return PH7_OK;` |
|      3 | 1214 |  |
|      - | 1215 | `/*` |
|      - | 1216 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1217 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1218 | ` * Parameters` |
|      - | 1219 | ` * $string` |
|      - | 1220 | ` *   The input string.` |
|      - | 1221 | ` * $flags` |
|      - | 1222 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1223 | ` * Return` |
|      - | 1224 | ` * The encoded string.` |
|      - | 1225 | ` */` |
|     10 | 1226 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1227 |  |
|     11 | 1228 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1229 | `	const char *zIn,*zEnd;` |
|      - | 1230 | `	int nLen,c;` |
|      - | 1231 | `	sxu32 n;` |
|     11 | 1232 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1233 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1234 | `		ph7_result_null(pCtx);` |
|      5 | 1235 | `		return PH7_OK;` |
|      - | 1236 | `	}` |
|      - | 1237 | `	/* Extract the target string */` |
|      7 | 1238 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1239 | `	/* Handle empty string up front */` |
|      7 | 1240 | `	if( nLen == 0 ){` |
|      3 | 1241 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1242 | `		return PH7_OK;` |
|      - | 1243 | `	}` |
|      5 | 1244 | `	zEnd = &zIn[nLen];` |
|      - | 1245 | `	/* Extract the flags if available */` |
|      5 | 1246 | `	if( nArg > 1 ){` |
|      3 | 1247 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1248 | `		if( iFlags < 0 ){` |
|      3 | 1249 | `			iFlags = 0x01;` |
|      1 | 1250 | `		}` |
|      1 | 1251 | `	}` |
|      - | 1252 | `	/* Perform the requested operation */` |
|     11 | 1253 | `	for(;;){` |
|     23 | 1254 | `		if( zIn >= zEnd ){` |
|      - | 1255 | `			/* No more input to process */` |
|      5 | 1256 | `			break;` |
|      - | 1257 | `		}` |
|     19 | 1258 | `		c = zIn[0];` |
|      - | 1259 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1260 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1261 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1262 | `				/* Got one */` |
|      9 | 1263 | `				break;` |
|      - | 1264 | `			}` |
|    108 | 1265 | `		}` |
|     19 | 1266 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1267 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1268 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1269 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 1270 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 1271 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 1272 | `				/* expand single quote verbatim */` |
|    ! 0 | 1273 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 1274 | `			}else{` |
|      9 | 1275 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 1276 | `			}` |
|      5 | 1277 | `		}else{` |
|      - | 1278 | `			/* Output character verbatim */` |
|     11 | 1279 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1280 | `		}` |
|     19 | 1281 | `		zIn++;` |
|      1 | 1282 | `	}` |
|      5 | 1283 | `	return PH7_OK;` |
|      6 | 1284 |  |
|      - | 1285 | `/*` |
|      - | 1286 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 1287 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 1288 | ` * Parameters` |
|      - | 1289 | ` * $string` |
|      - | 1290 | ` *   The input string.` |
|      - | 1291 | ` * $flags` |
|      - | 1292 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 1293 | ` * Return` |
|      - | 1294 | ` * The decoded string.` |
|      - | 1295 | ` */` |
|     28 | 1296 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1297 |  |
|      - | 1298 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 1299 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 1300 | `	int nLen;` |
|      - | 1301 | `	sxu32 n;` |
|     29 | 1302 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1303 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1304 | `		ph7_result_null(pCtx);` |
|      5 | 1305 | `		return PH7_OK;` |
|      - | 1306 | `	}` |
|      - | 1307 | `	/* Extract the target string */` |
|     25 | 1308 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1309 | `	zEnd = &zIn[nLen];` |
|      - | 1310 | `	/* Extract the flags if available */` |
|     25 | 1311 | `	if( nArg > 1 ){` |
|     15 | 1312 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 1313 | `		if( iFlags < 0 ){` |
|      3 | 1314 | `			iFlags = 0x01;` |
|      1 | 1315 | `		}` |
|      7 | 1316 | `	}` |
|      - | 1317 | `	/* Perform the requested operation */` |
|     27 | 1318 | `	for(;;){` |
|     55 | 1319 | `		if( zIn >= zEnd ){` |
|      - | 1320 | `			/* No more input to process */` |
|     13 | 1321 | `			break;` |
|      - | 1322 | `		}` |
|     43 | 1323 | `		zCur = zIn;` |
|    173 | 1324 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 1325 | `			zIn++;` |
|      1 | 1326 | `		}` |
|     43 | 1327 | `		if( zCur < zIn ){` |
|      - | 1328 | `			/* Append raw string verbatim */` |
|     27 | 1329 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 1330 | `		}` |
|     43 | 1331 | `		if( zIn >= zEnd ){` |
|     13 | 1332 | `			break;` |
|      - | 1333 | `		}` |
|     31 | 1334 | `		nLen = (int)(zEnd-zIn);` |
|      - | 1335 | `		/* Find an encoded sequence */` |
|    113 | 1336 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 1337 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 1338 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 1339 | `				/* Got one */` |
|     31 | 1340 | `				zIn += iLen;` |
|     31 | 1341 | `				break;` |
|      - | 1342 | `			}` |
|     42 | 1343 | `		}` |
|     31 | 1344 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 1345 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 1346 | `			/* Output the decoded character */` |
|     31 | 1347 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 1348 | `				/* Do not process single quotes */` |
|      9 | 1349 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 1350 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1351 | `				/* Do not process double quotes */` |
|      5 | 1352 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 1353 | `			}else{` |
|     19 | 1354 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 1355 | `			}` |
|     16 | 1356 | `		}else{` |
|      - | 1357 | `			/* Append '&' */` |
|    ! 0 | 1358 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1359 | `			zIn++;` |
|      - | 1360 | `		}` |
|      1 | 1361 | `	}` |
|     25 | 1362 | `	return PH7_OK;` |
|     15 | 1363 |  |
|      - | 1364 | `/*` |
|      - | 1365 | ` * int strlen($string)` |
|      - | 1366 | ` *  return the length of the given string.` |
|      - | 1367 | ` * Parameter` |
|      - | 1368 | ` *  string: The string being measured for length.` |
|      - | 1369 | ` * Return` |
|      - | 1370 | ` *  length of the given string.` |
|      - | 1371 | ` */` |
|   8478 | 1372 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1373 |  |
|   8483 | 1374 | `	int iLen = 0;` |
|   8483 | 1375 | `	if( nArg > 0 ){` |
|   8481 | 1376 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   4238 | 1377 | `	}` |
|      - | 1378 | `	/* String length */` |
|   8483 | 1379 | `	ph7_result_int(pCtx,iLen);` |
|   8483 | 1380 | `	return PH7_OK;` |
|      5 | 1381 |  |
|      - | 1382 | `/*` |
|      - | 1383 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1384 | ` *  Perform a binary safe string comparison.` |
|      - | 1385 | ` * Parameter` |
|      - | 1386 | ` *  str1: The first string` |
|      - | 1387 | ` *  str2: The second string` |
|      - | 1388 | ` * Return` |
|      - | 1389 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1390 | ` *  than str2, and 0 if they are equal.` |
|      - | 1391 | ` */` |
|     80 | 1392 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1393 |  |
|      - | 1394 | `	const char *z1,*z2;` |
|      - | 1395 | `	int n1,n2;` |
|      - | 1396 | `	int res;` |
|     81 | 1397 | `	if( nArg < 2 ){` |
|      5 | 1398 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 1399 | `		ph7_result_int(pCtx,res);` |
|      5 | 1400 | `		return PH7_OK;` |
|      - | 1401 | `	}` |
|      - | 1402 | `	/* Perform the comparison */` |
|     77 | 1403 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     77 | 1404 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     77 | 1405 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1406 | `	/* Comparison result */` |
|     77 | 1407 | `	ph7_result_int(pCtx,res);` |
|     77 | 1408 | `	return PH7_OK;` |
|     41 | 1409 |  |
|      - | 1410 | `/*` |
|      - | 1411 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1412 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1413 | ` * Parameter` |
|      - | 1414 | ` *  str1: The first string` |
|      - | 1415 | ` *  str2: The second string` |
|      - | 1416 | ` * Return` |
|      - | 1417 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1418 | ` *  than str2, and 0 if they are equal.` |
|      - | 1419 | ` */` |
|     20 | 1420 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1421 |  |
|      - | 1422 | `	const char *z1,*z2;` |
|      - | 1423 | `	int res;` |
|      - | 1424 | `	int n;` |
|     21 | 1425 | `	if( nArg < 3 ){` |
|      - | 1426 | `		/* Perform a standard comparison */` |
|      5 | 1427 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1428 | `	}` |
|      - | 1429 | `	/* Desired comparison length */` |
|     17 | 1430 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1431 | `	if( n < 0 ){` |
|      - | 1432 | `		/* Invalid length */` |
|      3 | 1433 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1434 | `		return PH7_OK;` |
|      - | 1435 | `	}` |
|      - | 1436 | `	/* Perform the comparison */` |
|     15 | 1437 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1438 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1439 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1440 | `	/* Comparison result */` |
|     15 | 1441 | `	ph7_result_int(pCtx,res);` |
|     15 | 1442 | `	return PH7_OK;` |
|     11 | 1443 |  |
|      - | 1444 | `/*` |
|      - | 1445 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1446 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1447 | ` * Parameter` |
|      - | 1448 | ` *  str1: The first string` |
|      - | 1449 | ` *  str2: The second string` |
|      - | 1450 | ` * Return` |
|      - | 1451 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1452 | ` *  than str2, and 0 if they are equal.` |
|      - | 1453 | ` */` |
|     22 | 1454 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1455 |  |
|      - | 1456 | `	const char *z1,*z2;` |
|      - | 1457 | `	int n1,n2;` |
|      - | 1458 | `	int res;` |
|     23 | 1459 | `	if( nArg < 2 ){` |
|      9 | 1460 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 1461 | `		ph7_result_int(pCtx,res);` |
|      9 | 1462 | `		return PH7_OK;` |
|      - | 1463 | `	}` |
|      - | 1464 | `	/* Perform the comparison */` |
|     15 | 1465 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1466 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1467 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1468 | `	/* Comparison result */` |
|     15 | 1469 | `	ph7_result_int(pCtx,res);` |
|     15 | 1470 | `	return PH7_OK;` |
|     12 | 1471 |  |
|      - | 1472 | `/*` |
|      - | 1473 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1474 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1475 | ` * Parameter` |
|      - | 1476 | ` *  $str1: The first string` |
|      - | 1477 | ` *  $str2: The second string` |
|      - | 1478 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1479 | ` * Return` |
|      - | 1480 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1481 | ` *  than str2, and 0 if they are equal.` |
|      - | 1482 | ` */` |
|      8 | 1483 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1484 |  |
|      - | 1485 | `	const char *z1,*z2;` |
|      - | 1486 | `	int res;` |
|      - | 1487 | `	int n;` |
|      9 | 1488 | `	if( nArg < 3 ){` |
|      - | 1489 | `		/* Perform a standard comparison */` |
|      5 | 1490 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1491 | `	}` |
|      - | 1492 | `	/* Desired comparison length */` |
|      5 | 1493 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1494 | `	if( n < 0 ){` |
|      - | 1495 | `		/* Invalid length */` |
|    ! 0 | 1496 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1497 | `		return PH7_OK;` |
|      - | 1498 | `	}` |
|      - | 1499 | `	/* Perform the comparison */` |
|      5 | 1500 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1501 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1502 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1503 | `	/* Comparison result */` |
|      5 | 1504 | `	ph7_result_int(pCtx,res);` |
|      5 | 1505 | `	return PH7_OK;` |
|      5 | 1506 |  |
|      - | 1507 | `/*` |
|      - | 1508 | ` * Implode context [i.e: it's private data].` |
|      - | 1509 | ` * A pointer to the following structure is forwarded` |
|      - | 1510 | ` * verbatim to the array walker callback defined below.` |
|      - | 1511 | ` */` |
|      - | 1512 | `struct implode_data {` |
|      - | 1513 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1514 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1515 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1516 | `	int nSeplen;          /* Separator length */` |
|      - | 1517 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1518 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1519 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1520 | `};` |
|      - | 1521 | `/*` |
|      - | 1522 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1523 | ` * The following routine is invoked for each array entry passed` |
|      - | 1524 | ` * to the implode() function.` |
|      - | 1525 | ` */` |
| 129740 | 1526 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1527 |  |
|  64870 | 1528 | `	SXUNUSED(pKey);` |
| 129745 | 1529 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1530 | `	const char *zData;` |
|      - | 1531 | `	int nLen;` |
| 129745 | 1532 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1533 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1534 | `			if( !pData->bFirst ){` |
|      - | 1535 | `				/* append the separator first */` |
|      3 | 1536 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1537 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1538 | `					return PH7_ABORT;` |
|      - | 1539 | `				}` |
|      2 | 1540 | `			}else{` |
|    ! 0 | 1541 | `				pData->bFirst = 0;` |
|      - | 1542 | `			}` |
|      1 | 1543 | `		}` |
|      - | 1544 | `		/* Recurse */` |
|      3 | 1545 | `		pData->bFirst = 1;` |
|      3 | 1546 | `		pData->nRecCount++;` |
|      3 | 1547 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1548 | `		pData->nRecCount--;` |
|      - | 1549 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1550 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1551 | `			return PH7_ABORT;` |
|      - | 1552 | `		}` |
|      3 | 1553 | `		return PH7_OK;` |
|      - | 1554 | `	}` |
|      - | 1555 | `	/* Extract the string representation of the entry value */` |
| 129743 | 1556 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1557 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 129743 | 1558 | `	if( pData->bFirst ){` |
|  32113 | 1559 | `		pData->bFirst = 0;` |
| 113689 | 1560 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1561 | `		/* append the separator first */` |
|  97623 | 1562 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1563 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1564 | `			return PH7_ABORT;` |
|      - | 1565 | `		}` |
|  48809 | 1566 | `	}` |
|      - | 1567 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 129743 | 1568 | `	if( nLen > 0 ){` |
| 118017 | 1569 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1570 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1571 | `			return PH7_ABORT;` |
|      - | 1572 | `		}` |
|  59006 | 1573 | `	}` |
| 129743 | 1574 | `	return PH7_OK;` |
|  64875 | 1575 |  |
|      - | 1576 | `/*` |
|      - | 1577 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1578 | ` * string implode(array $pieces,...)` |
|      - | 1579 | ` *  Join array elements with a string.` |
|      - | 1580 | ` * $glue` |
|      - | 1581 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1582 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1583 | ` * $pieces` |
|      - | 1584 | ` *   The array of strings to implode.` |
|      - | 1585 | ` * Return` |
|      - | 1586 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1587 | ` *  order, with the glue string between each element.` |
|      - | 1588 | ` */` |
|  32134 | 1589 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1590 |  |
|      - | 1591 | `	struct implode_data imp_data;` |
|  32139 | 1592 | `	int i = 1;` |
|  32139 | 1593 | `	if( nArg < 1 ){` |
|      - | 1594 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1595 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1596 | `		return PH7_OK;` |
|      - | 1597 | `	}` |
|      - | 1598 | `	/* Prepare the implode context */` |
|  32139 | 1599 | `	imp_data.pCtx = pCtx;` |
|  32139 | 1600 | `	imp_data.bRecursive = 0;` |
|  32139 | 1601 | `	imp_data.bFirst = 1;` |
|  32139 | 1602 | `	imp_data.nRecCount = 0;` |
|  32139 | 1603 | `	imp_data.rc = SXRET_OK;` |
|  32139 | 1604 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  32137 | 1605 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16071 | 1606 | `	}else{` |
|      3 | 1607 | `		imp_data.zSep = 0;` |
|      3 | 1608 | `		imp_data.nSeplen = 0;` |
|      3 | 1609 | `		i = 0;` |
|      - | 1610 | `	}` |
|  32139 | 1611 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1612 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1613 | `	}` |
|      - | 1614 | `	/* Start the 'join' process */` |
|  64273 | 1615 | `	while( i < nArg ){` |
|  32139 | 1616 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1617 | `			/* Iterate throw array entries */` |
|  32139 | 1618 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1619 | `			/* Surface a callback allocation failure as a fatal */` |
|  32139 | 1620 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1621 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1622 | `			}` |
|  16072 | 1623 | `		}else{` |
|      - | 1624 | `			const char *zData;` |
|      - | 1625 | `			int nLen;` |
|      - | 1626 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1627 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1628 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1629 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1630 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1631 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1632 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1633 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1634 | `				}` |
|    ! 0 | 1635 | `			}` |
|      - | 1636 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1637 | `			if( nLen > 0 ){` |
|    ! 0 | 1638 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1639 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1640 | `				}` |
|    ! 0 | 1641 | `			}` |
|      - | 1642 | `		}` |
|  32139 | 1643 | `		i++;` |
|      5 | 1644 | `	}` |
|  32139 | 1645 | `	return PH7_OK;` |
|  16072 | 1646 |  |
|      - | 1647 | `/*` |
|      - | 1648 | ` * Symisc eXtension:` |
|      - | 1649 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1650 | ` * Purpose` |
|      - | 1651 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1652 | ` * Example:` |
|      - | 1653 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1654 | ` *   echo implode_recursive("/",$a);` |
|      - | 1655 | ` *   Will output` |
|      - | 1656 | ` *     usr/home/dean.` |
|      - | 1657 | ` *   While the standard implode would produce.` |
|      - | 1658 | ` *    usr/Array.` |
|      - | 1659 | ` * Parameter` |
|      - | 1660 | ` *  Refer to implode().` |
|      - | 1661 | ` * Return` |
|      - | 1662 | ` *  Refer to implode().` |
|      - | 1663 | ` */` |
|     12 | 1664 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1665 |  |
|      - | 1666 | `	struct implode_data imp_data;` |
|     13 | 1667 | `	int i = 1;` |
|     13 | 1668 | `	if( nArg < 1 ){` |
|      - | 1669 | `		/* Missing argument,return NULL */` |
|      3 | 1670 | `		ph7_result_null(pCtx);` |
|      3 | 1671 | `		return PH7_OK;` |
|      - | 1672 | `	}` |
|      - | 1673 | `	/* Prepare the implode context */` |
|     11 | 1674 | `	imp_data.pCtx = pCtx;` |
|     11 | 1675 | `	imp_data.bRecursive = 1;` |
|     11 | 1676 | `	imp_data.bFirst = 1;` |
|     11 | 1677 | `	imp_data.nRecCount = 0;` |
|     11 | 1678 | `	imp_data.rc = SXRET_OK;` |
|     11 | 1679 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1680 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1681 | `	}else{` |
|    ! 0 | 1682 | `		imp_data.zSep = 0;` |
|    ! 0 | 1683 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1684 | `		i = 0;` |
|      - | 1685 | `	}` |
|     11 | 1686 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1687 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1688 | `	}` |
|      - | 1689 | `	/* Start the 'join' process */` |
|     21 | 1690 | `	while( i < nArg ){` |
|     11 | 1691 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1692 | `			/* Iterate throw array entries */` |
|      3 | 1693 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1694 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 1695 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1696 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1697 | `			}` |
|      2 | 1698 | `		}else{` |
|      - | 1699 | `			const char *zData;` |
|      - | 1700 | `			int nLen;` |
|      - | 1701 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1702 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1703 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1704 | `			if( imp_data.bFirst ){` |
|      9 | 1705 | `				imp_data.bFirst = 0;` |
|      4 | 1706 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1707 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1708 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1709 | `				}` |
|    ! 0 | 1710 | `			}` |
|      - | 1711 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1712 | `			if( nLen > 0 ){` |
|      9 | 1713 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1714 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1715 | `				}` |
|      4 | 1716 | `			}` |
|      - | 1717 | `		}` |
|     11 | 1718 | `		i++;` |
|      1 | 1719 | `	}` |
|     11 | 1720 | `	return PH7_OK;` |
|      7 | 1721 |  |
|      - | 1722 | `/*` |
|      - | 1723 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1724 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1725 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1726 | ` * Parameters` |
|      - | 1727 | ` *  $delimiter` |
|      - | 1728 | ` *   The boundary string.` |
|      - | 1729 | ` * $string` |
|      - | 1730 | ` *   The input string.` |
|      - | 1731 | ` * $limit` |
|      - | 1732 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1733 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1734 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1735 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1736 | ` * Returns` |
|      - | 1737 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1738 | ` *  on boundaries formed by the delimiter.` |
|      - | 1739 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1740 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1741 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1742 | ` *  will be returned.` |
|      - | 1743 | ` * NOTE:` |
|      - | 1744 | ` *  Negative limit is not supported.` |
|      - | 1745 | ` */` |
|   6080 | 1746 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1747 |  |
|      - | 1748 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1749 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1750 | `	ph7_value *pArray;` |
|      - | 1751 | `	ph7_value *pValue;` |
|      - | 1752 | `	sxu32 nOfft;` |
|      - | 1753 | `	sxi32 rc;` |
|   6085 | 1754 | `	if( nArg < 2 ){` |
|      - | 1755 | `		/* Missing arguments,return FALSE */` |
|      9 | 1756 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1757 | `		return PH7_OK;` |
|      - | 1758 | `	}` |
|      - | 1759 | `	/* Extract the delimiter */` |
|   6077 | 1760 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6077 | 1761 | `	if( nDelim < 1 ){` |
|      - | 1762 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1763 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1764 | `		return PH7_OK;` |
|      - | 1765 | `	}` |
|      - | 1766 | `	/* Extract the string */` |
|   6075 | 1767 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6075 | 1768 | `	if( nStrlen < 1 ){` |
|      - | 1769 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 1770 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 1771 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 1772 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 1773 | `		if( pArrayTmp == 0 ){` |
|      - | 1774 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1775 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1776 | `			return PH7_OK;` |
|      - | 1777 | `		}` |
|      7 | 1778 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 1779 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 1780 | `			if( pValueTmp == 0 ){` |
|      - | 1781 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 1782 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 1783 | `				return PH7_OK;` |
|      - | 1784 | `			}` |
|      5 | 1785 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 1786 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 1787 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1788 | `			}` |
|      2 | 1789 | `		}` |
|      7 | 1790 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 1791 | `		return PH7_OK;` |
|      - | 1792 | `	}` |
|      - | 1793 | `	/* Point to the end of the string */` |
|   6069 | 1794 | `	zEnd = &zString[nStrlen];` |
|      - | 1795 | `	/* Create the array */` |
|   6069 | 1796 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6069 | 1797 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6069 | 1798 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1799 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1800 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1801 | `		return PH7_OK;` |
|      - | 1802 | `	}` |
|      - | 1803 | `	/* Set a defualt limit */` |
|   6069 | 1804 | `	iLimit = SXI32_HIGH;` |
|   6069 | 1805 | `	if( nArg > 2 ){` |
|     29 | 1806 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     29 | 1807 | `		if( iLimit < 0 ){` |
|      - | 1808 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 1809 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 1810 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 1811 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 1812 | `			int nTotal = 1,nKeep;` |
|     17 | 1813 | `			const char *zScan = zString;` |
|      - | 1814 | `			sxu32 nScanOfft;` |
|     57 | 1815 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 1816 | `				nTotal++;` |
|     41 | 1817 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 1818 | `			}` |
|     17 | 1819 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 1820 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 1821 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 1822 | `				/* Emit the next clean component */` |
|     23 | 1823 | `				zCur = &zString[nOfft];` |
|     23 | 1824 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 1825 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1826 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1827 | `				}` |
|     23 | 1828 | `				zString = &zCur[nDelim];` |
|     23 | 1829 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 1830 | `			}` |
|     17 | 1831 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 1832 | `			return PH7_OK;` |
|      - | 1833 | `		}` |
|     13 | 1834 | `		if( iLimit == 0 ){` |
|      5 | 1835 | `			iLimit = 1;` |
|      2 | 1836 | `		}` |
|     13 | 1837 | `		iLimit--;` |
|      6 | 1838 | `	}` |
|      - | 1839 | `	/* Start exploding */` |
|  70313 | 1840 | `	for(;;){` |
| 140631 | 1841 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 140631 | 1842 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1843 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6053 | 1844 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6053 | 1845 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1846 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1847 | `			}` |
|   6053 | 1848 | `			break;` |
|      - | 1849 | `		}` |
|      - | 1850 | `		/* Point to the desired offset */` |
| 134583 | 1851 | `		zCur = &zString[nOfft];` |
|      - | 1852 | `		/* Perform the store operation (may be empty) */` |
| 134583 | 1853 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 134583 | 1854 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1855 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1856 | `		}` |
|      - | 1857 | `		/* Point beyond the delimiter */` |
| 134583 | 1858 | `		zString = &zCur[nDelim];` |
|      - | 1859 | `		/* Reset the cursor */` |
| 134583 | 1860 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1861 | `	}` |
|      - | 1862 | `	/* Return the freshly created array */` |
|   6053 | 1863 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1864 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1865 | `	 * released as soon we return from this foregin function.` |
|      - | 1866 | `	 */` |
|   6053 | 1867 | `	return PH7_OK;` |
|   3045 | 1868 |  |
|      - | 1869 | `/*` |
|      - | 1870 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1871 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1872 | ` * Parameters` |
|      - | 1873 | ` *  $str` |
|      - | 1874 | ` *   The string that will be trimmed.` |
|      - | 1875 | ` * $charlist` |
|      - | 1876 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1877 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1878 | ` *   With .. you can specify a range of characters.` |
|      - | 1879 | ` * Returns.` |
|      - | 1880 | ` *  Thr processed string.` |
|      - | 1881 | ` * NOTE:` |
|      - | 1882 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1883 | ` */` |
|  13822 | 1884 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1885 |  |
|      - | 1886 | `	const char *zString;` |
|      - | 1887 | `	int nLen;` |
|  13827 | 1888 | `	if( nArg < 1 ){` |
|      - | 1889 | `		/* Missing arguments,return null */` |
|      3 | 1890 | `		ph7_result_null(pCtx);` |
|      3 | 1891 | `		return PH7_OK;` |
|      - | 1892 | `	}` |
|      - | 1893 | `	/* Extract the target string */` |
|  13825 | 1894 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  13825 | 1895 | `	if( nLen < 1 ){` |
|      - | 1896 | `		/* Empty string,return */` |
|   1725 | 1897 | `		ph7_result_string(pCtx,"",0);` |
|   1725 | 1898 | `		return PH7_OK;` |
|      - | 1899 | `	}` |
|      - | 1900 | `	/* Start the trim process */` |
|  12105 | 1901 | `	if( nArg < 2 ){` |
|      - | 1902 | `		SyString sStr;` |
|      - | 1903 | `		/* Remove white spaces and NUL bytes */` |
|  12101 | 1904 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  29609 | 1905 | `		SyStringFullTrimSafe(&sStr);` |
|  12101 | 1906 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6053 | 1907 | `	}else{` |
|      - | 1908 | `		/* Char list */` |
|      - | 1909 | `		const char *zList;` |
|      - | 1910 | `		int nListlen;` |
|      5 | 1911 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1912 | `		if( nListlen < 1 ){` |
|      - | 1913 | `			/* Return the string unchanged */` |
|      3 | 1914 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1915 | `		}else{` |
|      3 | 1916 | `			const char *zEnd = &zString[nLen];` |
|      3 | 1917 | `			const char *zCur = zString;` |
|      - | 1918 | `			const char *zPtr;` |
|      - | 1919 | `			int i;` |
|      - | 1920 | `			/* Left trim */` |
|      4 | 1921 | `			for(;;){` |
|      9 | 1922 | `				if( zCur >= zEnd ){` |
|    ! 0 | 1923 | `					break;` |
|      - | 1924 | `				}` |
|      9 | 1925 | `				zPtr = zCur;` |
|     17 | 1926 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1927 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 1928 | `						zCur++;` |
|      3 | 1929 | `					}` |
|      5 | 1930 | `				}` |
|      9 | 1931 | `				if( zCur == zPtr ){` |
|      - | 1932 | `					/* No match,break immediately */` |
|      3 | 1933 | `					break;` |
|      - | 1934 | `				}` |
|      1 | 1935 | `			}` |
|      - | 1936 | `			/* Right trim */` |
|      3 | 1937 | `			zEnd--;` |
|      4 | 1938 | `			for(;;){` |
|      9 | 1939 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1940 | `					break;` |
|      - | 1941 | `				}` |
|      9 | 1942 | `				zPtr = zEnd;` |
|     17 | 1943 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1944 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 1945 | `						zEnd--;` |
|      3 | 1946 | `					}` |
|      5 | 1947 | `				}` |
|      9 | 1948 | `				if( zEnd == zPtr ){` |
|      3 | 1949 | `					break;` |
|      - | 1950 | `				}` |
|      1 | 1951 | `			}` |
|      3 | 1952 | `			if( zCur >= zEnd ){` |
|      - | 1953 | `				/* Return the empty string */` |
|    ! 0 | 1954 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1955 | `			}else{` |
|      3 | 1956 | `				zEnd++;` |
|      3 | 1957 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1958 | `			}` |
|      - | 1959 | `		}` |
|      - | 1960 | `	}` |
|  12105 | 1961 | `	return PH7_OK;` |
|   6916 | 1962 |  |
|      - | 1963 | `/*` |
|      - | 1964 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1965 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1966 | ` * Parameters` |
|      - | 1967 | ` *  $str` |
|      - | 1968 | ` *   The string that will be trimmed.` |
|      - | 1969 | ` * $charlist` |
|      - | 1970 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1971 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1972 | ` *   With .. you can specify a range of characters.` |
|      - | 1973 | ` * Returns.` |
|      - | 1974 | ` *  Thr processed string.` |
|      - | 1975 | ` * NOTE:` |
|      - | 1976 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1977 | ` */` |
|     26 | 1978 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1979 |  |
|      - | 1980 | `	const char *zString;` |
|      - | 1981 | `	int nLen;` |
|     27 | 1982 | `	if( nArg < 1 ){` |
|      - | 1983 | `		/* Missing arguments,return null */` |
|      3 | 1984 | `		ph7_result_null(pCtx);` |
|      3 | 1985 | `		return PH7_OK;` |
|      - | 1986 | `	}` |
|      - | 1987 | `	/* Extract the target string */` |
|     25 | 1988 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1989 | `	if( nLen < 1 ){` |
|      - | 1990 | `		/* Empty string,return */` |
|      5 | 1991 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1992 | `		return PH7_OK;` |
|      - | 1993 | `	}` |
|      - | 1994 | `	/* Start the trim process */` |
|     21 | 1995 | `	if( nArg < 2 ){` |
|      - | 1996 | `		SyString sStr;` |
|      - | 1997 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1998 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1999 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2000 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2001 | `	}else{` |
|      - | 2002 | `		/* Char list */` |
|      - | 2003 | `		const char *zList;` |
|      - | 2004 | `		int nListlen;` |
|      5 | 2005 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 2006 | `		if( nListlen < 1 ){` |
|      - | 2007 | `			/* Return the string unchanged */` |
|    ! 0 | 2008 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2009 | `		}else{` |
|      5 | 2010 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 2011 | `			const char *zCur = zString;` |
|      - | 2012 | `			const char *zPtr;` |
|      - | 2013 | `			int i;` |
|      - | 2014 | `			/* Right trim */` |
|      6 | 2015 | `			for(;;){` |
|     13 | 2016 | `				if( zEnd <= zCur ){` |
|    ! 0 | 2017 | `					break;` |
|      - | 2018 | `				}` |
|     13 | 2019 | `				zPtr = zEnd;` |
|     25 | 2020 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 2021 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 2022 | `						zEnd--;` |
|      4 | 2023 | `					}` |
|      7 | 2024 | `				}` |
|     13 | 2025 | `				if( zEnd == zPtr ){` |
|      5 | 2026 | `					break;` |
|      - | 2027 | `				}` |
|      1 | 2028 | `			}` |
|      5 | 2029 | `			if( zEnd <= zCur ){` |
|      - | 2030 | `				/* Return the empty string */` |
|    ! 0 | 2031 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2032 | `			}else{` |
|      5 | 2033 | `				zEnd++;` |
|      5 | 2034 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2035 | `			}` |
|      - | 2036 | `		}` |
|      - | 2037 | `	}` |
|     21 | 2038 | `	return PH7_OK;` |
|     14 | 2039 |  |
|      - | 2040 | `/*` |
|      - | 2041 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2042 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2043 | ` * Parameters` |
|      - | 2044 | ` *  $str` |
|      - | 2045 | ` *   The string that will be trimmed.` |
|      - | 2046 | ` * $charlist` |
|      - | 2047 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2048 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2049 | ` *   With .. you can specify a range of characters.` |
|      - | 2050 | ` * Returns.` |
|      - | 2051 | ` *  Thr processed string.` |
|      - | 2052 | ` * NOTE:` |
|      - | 2053 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 2054 | ` */` |
|     12 | 2055 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2056 |  |
|      - | 2057 | `	const char *zString;` |
|      - | 2058 | `	int nLen;` |
|     13 | 2059 | `	if( nArg < 1 ){` |
|      - | 2060 | `		/* Missing arguments,return null */` |
|      3 | 2061 | `		ph7_result_null(pCtx);` |
|      3 | 2062 | `		return PH7_OK;` |
|      - | 2063 | `	}` |
|      - | 2064 | `	/* Extract the target string */` |
|     11 | 2065 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2066 | `	if( nLen < 1 ){` |
|      - | 2067 | `		/* Empty string,return */` |
|    ! 0 | 2068 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2069 | `		return PH7_OK;` |
|      - | 2070 | `	}` |
|      - | 2071 | `	/* Start the trim process */` |
|     11 | 2072 | `	if( nArg < 2 ){` |
|      - | 2073 | `		SyString sStr;` |
|      - | 2074 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2075 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2076 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2077 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2078 | `	}else{` |
|      - | 2079 | `		/* Char list */` |
|      - | 2080 | `		const char *zList;` |
|      - | 2081 | `		int nListlen;` |
|      9 | 2082 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2083 | `		if( nListlen < 1 ){` |
|      - | 2084 | `			/* Return the string unchanged */` |
|      3 | 2085 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2086 | `		}else{` |
|      7 | 2087 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2088 | `			const char *zCur = zString;` |
|      - | 2089 | `			const char *zPtr;` |
|      - | 2090 | `			int i;` |
|      - | 2091 | `			/* Left trim */` |
|      7 | 2092 | `			for(;;){` |
|     15 | 2093 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2094 | `					break;` |
|      - | 2095 | `				}` |
|     15 | 2096 | `				zPtr = zCur;` |
|     41 | 2097 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2098 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2099 | `						zCur++;` |
|      6 | 2100 | `					}` |
|     14 | 2101 | `				}` |
|     15 | 2102 | `				if( zCur == zPtr ){` |
|      - | 2103 | `					/* No match,break immediately */` |
|      7 | 2104 | `					break;` |
|      - | 2105 | `				}` |
|      1 | 2106 | `			}` |
|      7 | 2107 | `			if( zCur >= zEnd ){` |
|      - | 2108 | `				/* Return the empty string */` |
|    ! 0 | 2109 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2110 | `			}else{` |
|      7 | 2111 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2112 | `			}` |
|      - | 2113 | `		}` |
|      - | 2114 | `	}` |
|     11 | 2115 | `	return PH7_OK;` |
|      7 | 2116 |  |
|      - | 2117 | `/*` |
|      - | 2118 | ` * string strtolower(string $str)` |
|      - | 2119 | ` *  Make a string lowercase.` |
|      - | 2120 | ` * Parameters` |
|      - | 2121 | ` *  $str` |
|      - | 2122 | ` *   The input string.` |
|      - | 2123 | ` * Returns.` |
|      - | 2124 | ` *  The lowercased string.` |
|      - | 2125 | ` */` |
|  31766 | 2126 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2127 |  |
|      - | 2128 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2129 | `	int nLen;` |
|  31771 | 2130 | `	if( nArg < 1 ){` |
|      - | 2131 | `		/* Missing arguments,return null */` |
|      3 | 2132 | `		ph7_result_null(pCtx);` |
|      3 | 2133 | `		return PH7_OK;` |
|      - | 2134 | `	}` |
|      - | 2135 | `	/* Extract the target string */` |
|  31769 | 2136 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  31769 | 2137 | `	if( nLen < 1 ){` |
|      - | 2138 | `		/* Empty string,return */` |
|      3 | 2139 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2140 | `		return PH7_OK;` |
|      - | 2141 | `	}` |
|      - | 2142 | `	/* Perform the requested operation */` |
|  31767 | 2143 | `	zEnd = &zString[nLen];` |
| 100077 | 2144 | `	for(;;){` |
| 200159 | 2145 | `		if( zString >= zEnd ){` |
|      - | 2146 | `			/* No more input,break immediately */` |
|  31767 | 2147 | `			break;` |
|      - | 2148 | `		}` |
| 168397 | 2149 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2150 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2151 | `			zCur = zString;` |
|    ! 0 | 2152 | `			zString++;` |
|    ! 0 | 2153 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2154 | `				zString++;` |
|    ! 0 | 2155 | `			}` |
|      - | 2156 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2157 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2158 | `		}else{` |
| 168397 | 2159 | `			int c = zString[0];` |
| 168397 | 2160 | `			if( SyisUpper(c) ){` |
| 168395 | 2161 | `				c = SyToLower(zString[0]);` |
|  84195 | 2162 | `			}` |
|      - | 2163 | `			/* Append character */` |
| 168397 | 2164 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2165 | `			/* Advance the cursor */` |
| 168397 | 2166 | `			zString++;` |
|      - | 2167 | `		}` |
|      5 | 2168 | `	}` |
|  31767 | 2169 | `	return PH7_OK;` |
|  15888 | 2170 |  |
|      - | 2171 | `/*` |
|      - | 2172 | ` * string strtolower(string $str)` |
|      - | 2173 | ` *  Make a string uppercase.` |
|      - | 2174 | ` * Parameters` |
|      - | 2175 | ` *  $str` |
|      - | 2176 | ` *   The input string.` |
|      - | 2177 | ` * Returns.` |
|      - | 2178 | ` *  The uppercased string.` |
|      - | 2179 | ` */` |
|     42 | 2180 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2181 |  |
|      - | 2182 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2183 | `	int nLen;` |
|     47 | 2184 | `	if( nArg < 1 ){` |
|      - | 2185 | `		/* Missing arguments,return null */` |
|      3 | 2186 | `		ph7_result_null(pCtx);` |
|      3 | 2187 | `		return PH7_OK;` |
|      - | 2188 | `	}` |
|      - | 2189 | `	/* Extract the target string */` |
|     45 | 2190 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     45 | 2191 | `	if( nLen < 1 ){` |
|      - | 2192 | `		/* Empty string,return */` |
|      3 | 2193 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2194 | `		return PH7_OK;` |
|      - | 2195 | `	}` |
|      - | 2196 | `	/* Perform the requested operation */` |
|     43 | 2197 | `	zEnd = &zString[nLen];` |
|     98 | 2198 | `	for(;;){` |
|    201 | 2199 | `		if( zString >= zEnd ){` |
|      - | 2200 | `			/* No more input,break immediately */` |
|     43 | 2201 | `			break;` |
|      - | 2202 | `		}` |
|    163 | 2203 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2204 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2205 | `			zCur = zString;` |
|    ! 0 | 2206 | `			zString++;` |
|    ! 0 | 2207 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2208 | `				zString++;` |
|    ! 0 | 2209 | `			}` |
|      - | 2210 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2211 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2212 | `		}else{` |
|    163 | 2213 | `			int c = zString[0];` |
|    163 | 2214 | `			if( SyisLower(c) ){` |
|    157 | 2215 | `				c = SyToUpper(zString[0]);` |
|     76 | 2216 | `			}` |
|      - | 2217 | `			/* Append character */` |
|    163 | 2218 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2219 | `			/* Advance the cursor */` |
|    163 | 2220 | `			zString++;` |
|      - | 2221 | `		}` |
|      5 | 2222 | `	}` |
|     43 | 2223 | `	return PH7_OK;` |
|     26 | 2224 |  |
|      - | 2225 | `/*` |
|      - | 2226 | ` * string ucfirst(string $str)` |
|      - | 2227 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2228 | ` *  character is alphabetic.` |
|      - | 2229 | ` * Parameters` |
|      - | 2230 | ` *  $str` |
|      - | 2231 | ` *   The input string.` |
|      - | 2232 | ` * Returns.` |
|      - | 2233 | ` *  The processed string.` |
|      - | 2234 | ` */` |
|      6 | 2235 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2236 |  |
|      - | 2237 | `	const char *zString,*zEnd;` |
|      - | 2238 | `	int nLen,c;` |
|      7 | 2239 | `	if( nArg < 1 ){` |
|      - | 2240 | `		/* Missing arguments,return null */` |
|      3 | 2241 | `		ph7_result_null(pCtx);` |
|      3 | 2242 | `		return PH7_OK;` |
|      - | 2243 | `	}` |
|      - | 2244 | `	/* Extract the target string */` |
|      5 | 2245 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2246 | `	if( nLen < 1 ){` |
|      - | 2247 | `		/* Empty string,return */` |
|      3 | 2248 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2249 | `		return PH7_OK;` |
|      - | 2250 | `	}` |
|      - | 2251 | `	/* Perform the requested operation */` |
|      3 | 2252 | `	zEnd = &zString[nLen];` |
|      3 | 2253 | `	c = zString[0];` |
|      3 | 2254 | `	if( SyisLower(c) ){` |
|      3 | 2255 | `		c = SyToUpper(c);` |
|      1 | 2256 | `	}` |
|      - | 2257 | `	/* Append the first character */` |
|      3 | 2258 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2259 | `	zString++;` |
|      3 | 2260 | `	if( zString < zEnd ){` |
|      - | 2261 | `		/* Append the rest of the input verbatim */` |
|      3 | 2262 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2263 | `	}` |
|      3 | 2264 | `	return PH7_OK;` |
|      4 | 2265 |  |
|      - | 2266 | `/*` |
|      - | 2267 | ` * string lcfirst(string $str)` |
|      - | 2268 | ` *  Make a string's first character lowercase.` |
|      - | 2269 | ` * Parameters` |
|      - | 2270 | ` *  $str` |
|      - | 2271 | ` *   The input string.` |
|      - | 2272 | ` * Returns.` |
|      - | 2273 | ` *  The processed string.` |
|      - | 2274 | ` */` |
|      6 | 2275 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2276 |  |
|      - | 2277 | `	const char *zString,*zEnd;` |
|      - | 2278 | `	int nLen,c;` |
|      7 | 2279 | `	if( nArg < 1 ){` |
|      - | 2280 | `		/* Missing arguments,return null */` |
|      3 | 2281 | `		ph7_result_null(pCtx);` |
|      3 | 2282 | `		return PH7_OK;` |
|      - | 2283 | `	}` |
|      - | 2284 | `	/* Extract the target string */` |
|      5 | 2285 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2286 | `	if( nLen < 1 ){` |
|      - | 2287 | `		/* Empty string,return */` |
|      3 | 2288 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2289 | `		return PH7_OK;` |
|      - | 2290 | `	}` |
|      - | 2291 | `	/* Perform the requested operation */` |
|      3 | 2292 | `	zEnd = &zString[nLen];` |
|      3 | 2293 | `	c = zString[0];` |
|      3 | 2294 | `	if( SyisUpper(c) ){` |
|      3 | 2295 | `		c = SyToLower(c);` |
|      1 | 2296 | `	}` |
|      - | 2297 | `	/* Append the first character */` |
|      3 | 2298 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2299 | `	zString++;` |
|      3 | 2300 | `	if( zString < zEnd ){` |
|      - | 2301 | `		/* Append the rest of the input verbatim */` |
|      3 | 2302 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2303 | `	}` |
|      3 | 2304 | `	return PH7_OK;` |
|      4 | 2305 |  |
|      - | 2306 | `/*` |
|      - | 2307 | ` * int ord(string $string)` |
|      - | 2308 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2309 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2310 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2311 | ` * Parameters` |
|      - | 2312 | ` *  $string` |
|      - | 2313 | ` *   The input string.` |
|      - | 2314 | ` * Returns` |
|      - | 2315 | ` *  The ASCII value as an integer.` |
|      - | 2316 | ` */` |
|     62 | 2317 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2318 |  |
|      - | 2319 | `	const char *zString;` |
|      - | 2320 | `	int nLen,c;` |
|      - | 2321 | `	/* PHP requires exactly one argument. */` |
|     65 | 2322 | `	if( nArg != 1 ){` |
|      8 | 2323 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2324 | `			"ArgumentCountError",` |
|      - | 2325 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2326 | `			nArg` |
|      - | 2327 | `			);` |
|      - | 2328 | `	}` |
|      - | 2329 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2330 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2331 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2332 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2333 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2334 | `			"of type string is deprecated"` |
|      - | 2335 | `			);` |
|      1 | 2336 | `	}` |
|      - | 2337 | `	/* Extract the target string */` |
|     59 | 2338 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2339 | `	if( nLen < 1 ){` |
|      - | 2340 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2341 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2342 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2343 | `			);` |
|      5 | 2344 | `		ph7_result_int(pCtx,0);` |
|      5 | 2345 | `		return PH7_OK;` |
|      - | 2346 | `	}` |
|      - | 2347 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2348 | `	if( nLen > 1 ){` |
|      7 | 2349 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2350 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2351 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2352 | `			);` |
|      3 | 2353 | `	}` |
|      - | 2354 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2355 | `	c = (unsigned char)zString[0];` |
|      - | 2356 | `	/* Return that value */` |
|     55 | 2357 | `	ph7_result_int(pCtx,c);` |
|     55 | 2358 | `	return PH7_OK;` |
|     34 | 2359 |  |
|      - | 2360 | `/*` |
|      - | 2361 | ` * string chr(int $codepoint)` |
|      - | 2362 | ` *  Returns a one-character string containing the character specified` |
|      - | 2363 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2364 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2365 | ` * Parameters` |
|      - | 2366 | ` *  $codepoint` |
|      - | 2367 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2368 | ` *   will be constrained to a single byte.` |
|      - | 2369 | ` * Returns` |
|      - | 2370 | ` *  A single-character string.` |
|      - | 2371 | ` */` |
|     48 | 2372 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2373 |  |
|      - | 2374 | `	int c;` |
|      - | 2375 | `	unsigned char ch;` |
|      - | 2376 | `	/* PHP requires exactly one argument. */` |
|     51 | 2377 | `	if( nArg != 1 ){` |
|      8 | 2378 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2379 | `			"ArgumentCountError",` |
|      - | 2380 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2381 | `			nArg` |
|      - | 2382 | `			);` |
|      - | 2383 | `	}` |
|      - | 2384 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2385 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2386 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2387 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     45 | 2388 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2389 | `		char zBuf[120];` |
|      4 | 2390 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2391 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2392 | `			ph7_value_to_double(apArg[0])` |
|      - | 2393 | `			);` |
|      3 | 2394 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2395 | `	}` |
|      - | 2396 | `	/* Extract the codepoint. */` |
|     45 | 2397 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2398 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2399 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2400 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2401 | `	 * name to avoid the API double-prefixing it. */` |
|     45 | 2402 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2403 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2404 | `			E_DEPRECATED,` |
|      - | 2405 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2406 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2407 | `			"The value used will be constrained using % 256"` |
|      - | 2408 | `			);` |
|      2 | 2409 | `	}` |
|      - | 2410 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2411 | `	 * when taking the address of a wider int. */` |
|     45 | 2412 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2413 | `	/* Return the specified character */` |
|     45 | 2414 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     45 | 2415 | `	return PH7_OK;` |
|     27 | 2416 |  |
|      - | 2417 | `/*` |
|      - | 2418 | ` * Binary to hex consumer callback.` |
|      - | 2419 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2420 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2421 | ` */` |
|   2330 | 2422 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2423 |  |
|      - | 2424 | `	/* Append hex chunk verbatim */` |
|   2331 | 2425 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   2331 | 2426 | `	return SXRET_OK;` |
|      1 | 2427 |  |
|      - | 2428 |  |
|      - | 2429 | `/*` |
|      - | 2430 | ` * string bin2hex(string $str)` |
|      - | 2431 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2432 | ` * Parameters` |
|      - | 2433 | ` *  $str` |
|      - | 2434 | ` *   The input string.` |
|      - | 2435 | ` * Returns.` |
|      - | 2436 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2437 | ` */` |
|     24 | 2438 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2439 |  |
|      - | 2440 | `	const char *zString;` |
|      - | 2441 | `	int nLen;` |
|      - | 2442 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     29 | 2443 | `	if( nArg != 1 ){` |
|      8 | 2444 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2445 | `			"ArgumentCountError",` |
|      - | 2446 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2447 | `			nArg` |
|      - | 2448 | `			);` |
|      - | 2449 | `	}` |
|      - | 2450 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2451 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2452 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2453 | `	 */` |
|     33 | 2454 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     16 | 2455 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2456 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2457 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2458 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2459 | `		)` |
|      - | 2460 | `	){` |
|      9 | 2461 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2462 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2463 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2464 | `			if( pInst && pInst->pClass ){` |
|      3 | 2465 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2466 | `			}` |
|      1 | 2467 | `		}` |
|     12 | 2468 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2469 | `			"TypeError",` |
|      - | 2470 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2471 | `			zType` |
|      - | 2472 | `			);` |
|      - | 2473 | `	}` |
|      - | 2474 | `	/* Extract the target string */` |
|     15 | 2475 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 2476 | `	if( nLen < 1 ){` |
|      - | 2477 | `		/* Empty string,return */` |
|      3 | 2478 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2479 | `		return PH7_OK;` |
|      - | 2480 | `	}` |
|      - | 2481 | `	/* Perform the requested operation */` |
|     13 | 2482 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|     13 | 2483 | `	return PH7_OK;` |
|     17 | 2484 |  |
|      - | 2485 |  |
|      - | 2486 | `/* Search callback signature */` |
|      - | 2487 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2488 | `/*` |
|      - | 2489 | ` * Case-insensitive pattern match.` |
|      - | 2490 | ` * Brute force is the default search method used here.` |
|      - | 2491 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2492 | ` * well for short/medium texts on modern hardware.` |
|      - | 2493 | ` */` |
|    118 | 2494 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2495 |  |
|    119 | 2496 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2497 | `	const char *zIn = (const char *)pText;` |
|    119 | 2498 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2499 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2500 | `	const char *zPtr,*zPtr2;` |
|      - | 2501 | `	int c,d;` |
|    119 | 2502 | `	if( iPatLen > nLen ){` |
|      - | 2503 | `		/* Don't bother processing */` |
|     33 | 2504 | `		return SXERR_NOTFOUND;` |
|      - | 2505 | `	}` |
|    242 | 2506 | `	for(;;){` |
|    485 | 2507 | `		if( zIn >= zEnd ){` |
|     47 | 2508 | `			break;` |
|      - | 2509 | `		}` |
|    439 | 2510 | `		c = SyToLower(zIn[0]);` |
|    439 | 2511 | `		d = SyToLower(zpIn[0]);` |
|    439 | 2512 | `		if( c == d ){` |
|     41 | 2513 | `			zPtr   = &zIn[1];` |
|     41 | 2514 | `			zPtr2  = &zpIn[1];` |
|     71 | 2515 | `			for(;;){` |
|    143 | 2516 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2517 | `					/* Pattern found */` |
|     41 | 2518 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2519 | `					return SXRET_OK;` |
|      - | 2520 | `				}` |
|    103 | 2521 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2522 | `					break;` |
|      - | 2523 | `				}` |
|    103 | 2524 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2525 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2526 | `				if( c != d ){` |
|    ! 0 | 2527 | `					break;` |
|      - | 2528 | `				}` |
|    103 | 2529 | `				zPtr++; zPtr2++;` |
|      1 | 2530 | `			}` |
|    ! 0 | 2531 | `		}` |
|    399 | 2532 | `		zIn++;` |
|      1 | 2533 | `	}` |
|      - | 2534 | `	/* Pattern not found */` |
|     47 | 2535 | `	return SXERR_NOTFOUND;` |
|     60 | 2536 |  |
|      - | 2537 | `/*` |
|      - | 2538 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2539 | ` *  Find the first occurrence of a string.` |
|      - | 2540 | ` * Parameters` |
|      - | 2541 | ` *  $haystack` |
|      - | 2542 | ` *   The input string.` |
|      - | 2543 | ` * $needle` |
|      - | 2544 | ` *   Search pattern (must be a string).` |
|      - | 2545 | ` * $before_needle` |
|      - | 2546 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2547 | ` *   of the needle (excluding the needle).` |
|      - | 2548 | ` * Return` |
|      - | 2549 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2550 | ` */` |
|     10 | 2551 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2552 |  |
|     11 | 2553 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2554 | `	const char *zBlob,*zPattern;` |
|      - | 2555 | `	int nLen,nPatLen;` |
|      - | 2556 | `	sxu32 nOfft;` |
|      - | 2557 | `	sxi32 rc;` |
|     11 | 2558 | `	if( nArg < 2 ){` |
|      - | 2559 | `		/* Missing arguments,return FALSE */` |
|      5 | 2560 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2561 | `		return PH7_OK;` |
|      - | 2562 | `	}` |
|      - | 2563 | `	/* Extract the needle and the haystack */` |
|      7 | 2564 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2565 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2566 | `	nOfft = 0; /* cc warning */` |
|      9 | 2567 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2568 | `		int before = 0;` |
|      - | 2569 | `		/* Perform the lookup */` |
|      5 | 2570 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2571 | `		if( rc != SXRET_OK ){` |
|      - | 2572 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2573 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2574 | `			return PH7_OK;` |
|      - | 2575 | `		}` |
|      - | 2576 | `		/* Return the portion of the string */` |
|      5 | 2577 | `		if( nArg > 2 ){` |
|      3 | 2578 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2579 | `		}` |
|      5 | 2580 | `		if( before ){` |
|      3 | 2581 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2582 | `		}else{` |
|      3 | 2583 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2584 | `		}` |
|      3 | 2585 | `	}else{` |
|      3 | 2586 | `		ph7_result_bool(pCtx,0);` |
|      - | 2587 | `	}` |
|      7 | 2588 | `	return PH7_OK;` |
|      6 | 2589 |  |
|      - | 2590 | `/*` |
|      - | 2591 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2592 | ` *  Case-insensitive strstr().` |
|      - | 2593 | ` * Parameters` |
|      - | 2594 | ` *  $haystack` |
|      - | 2595 | ` *   The input string.` |
|      - | 2596 | ` * $needle` |
|      - | 2597 | ` *   Search pattern (must be a string).` |
|      - | 2598 | ` * $before_needle` |
|      - | 2599 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2600 | ` *   of the needle (excluding the needle).` |
|      - | 2601 | ` * Return` |
|      - | 2602 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2603 | ` */` |
|      6 | 2604 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2605 |  |
|      7 | 2606 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2607 | `	const char *zBlob,*zPattern;` |
|      - | 2608 | `	int nLen,nPatLen;` |
|      - | 2609 | `	sxu32 nOfft;` |
|      - | 2610 | `	sxi32 rc;` |
|      7 | 2611 | `	if( nArg < 2 ){` |
|      - | 2612 | `		/* Missing arguments,return FALSE */` |
|      3 | 2613 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2614 | `		return PH7_OK;` |
|      - | 2615 | `	}` |
|      - | 2616 | `	/* Extract the needle and the haystack */` |
|      5 | 2617 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2618 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2619 | `	nOfft = 0; /* cc warning */` |
|      7 | 2620 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2621 | `		int before = 0;` |
|      - | 2622 | `		/* Perform the lookup */` |
|      5 | 2623 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2624 | `		if( rc != SXRET_OK ){` |
|      - | 2625 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2626 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2627 | `			return PH7_OK;` |
|      - | 2628 | `		}` |
|      - | 2629 | `		/* Return the portion of the string */` |
|      5 | 2630 | `		if( nArg > 2 ){` |
|      3 | 2631 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2632 | `		}` |
|      5 | 2633 | `		if( before ){` |
|      3 | 2634 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2635 | `		}else{` |
|      3 | 2636 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2637 | `		}` |
|      3 | 2638 | `	}else{` |
|    ! 0 | 2639 | `		ph7_result_bool(pCtx,0);` |
|      - | 2640 | `	}` |
|      5 | 2641 | `	return PH7_OK;` |
|      4 | 2642 |  |
|      - | 2643 | `/*` |
|      - | 2644 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2645 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2646 | ` * Parameters` |
|      - | 2647 | ` *  $haystack` |
|      - | 2648 | ` *   The input string.` |
|      - | 2649 | ` * $needle` |
|      - | 2650 | ` *   Search pattern (must be a string).` |
|      - | 2651 | ` * $offset` |
|      - | 2652 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2653 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2654 | ` *   of haystack.` |
|      - | 2655 | ` * Return` |
|      - | 2656 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2657 | ` */` |
|    124 | 2658 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2659 |  |
|    129 | 2660 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2661 | `	const char *zBlob,*zPattern;` |
|      - | 2662 | `	int nLen,nPatLen,nStart;` |
|      - | 2663 | `	sxu32 nOfft;` |
|      - | 2664 | `	sxi32 rc;` |
|    129 | 2665 | `	if( nArg < 2 ){` |
|      - | 2666 | `		/* Missing arguments,return FALSE */` |
|      7 | 2667 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2668 | `		return PH7_OK;` |
|      - | 2669 | `	}` |
|      - | 2670 | `	/* Extract the needle and the haystack */` |
|    123 | 2671 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    123 | 2672 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    123 | 2673 | `	nOfft = 0; /* cc warning */` |
|    123 | 2674 | `	nStart = 0;` |
|      - | 2675 | `	/* Peek the starting offset if available */` |
|    123 | 2676 | `	if( nArg > 2 ){` |
|    ! 0 | 2677 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2678 | `		if( nStart < 0 ){` |
|    ! 0 | 2679 | `			nStart = -nStart;` |
|    ! 0 | 2680 | `		}` |
|    ! 0 | 2681 | `		if( nStart >= nLen ){` |
|      - | 2682 | `			/* Invalid offset */` |
|    ! 0 | 2683 | `			nStart = 0;` |
|    ! 0 | 2684 | `		}else{` |
|    ! 0 | 2685 | `			zBlob += nStart;` |
|    ! 0 | 2686 | `			nLen -= nStart;` |
|      - | 2687 | `		}` |
|    ! 0 | 2688 | `	}` |
|    123 | 2689 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2690 | `		/* Perform the lookup */` |
|    121 | 2691 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    121 | 2692 | `		if( rc != SXRET_OK ){` |
|      - | 2693 | `			/* Pattern not found,return FALSE */` |
|     33 | 2694 | `			ph7_result_bool(pCtx,0);` |
|     33 | 2695 | `			return PH7_OK;` |
|      - | 2696 | `		}` |
|      - | 2697 | `		/* Return the pattern position */` |
|     90 | 2698 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     46 | 2699 | `	}else{` |
|      3 | 2700 | `		ph7_result_bool(pCtx,0);` |
|      - | 2701 | `	}` |
|     92 | 2702 | `	return PH7_OK;` |
|     67 | 2703 |  |
|      - | 2704 | `/*` |
|      - | 2705 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2706 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2707 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2708 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2709 | ` *` |
|      - | 2710 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2711 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2712 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2713 | ` *` |
|      - | 2714 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2715 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2716 | ` */` |
|    418 | 2717 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2718 | `	ph7_context *pCtx,` |
|      - | 2719 | `	ph7_value *pArg,` |
|      - | 2720 | `	const char *zFunc,` |
|      - | 2721 | `	int iArgNum,` |
|      - | 2722 | `	const char *zParamName,` |
|      - | 2723 | `	const char *zNullMsg,` |
|      - | 2724 | `	ph7_value *pTmp,` |
|      - | 2725 | `	const char **pzOut,` |
|      - | 2726 | `	int *pnOut` |
|      4 | 2727 | `){` |
|    422 | 2728 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2729 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2730 | `		*pzOut = "";` |
|     13 | 2731 | `		*pnOut = 0;` |
|     13 | 2732 | `		return PH7_OK;` |
|      - | 2733 | `	}` |
|    628 | 2734 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    388 | 2735 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2736 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2737 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2738 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2739 | `	    )` |
|      - | 2740 | `	){` |
|     34 | 2741 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2742 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2743 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2744 | `			if( pInst && pInst->pClass ){` |
|     13 | 2745 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2746 | `			}` |
|      6 | 2747 | `		}` |
|     49 | 2748 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2749 | `			"TypeError",` |
|      - | 2750 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2751 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2752 | `			);` |
|      - | 2753 | `	}` |
|    377 | 2754 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2755 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2756 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2757 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2758 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2759 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2760 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2761 | `		return PH7_OK;` |
|      - | 2762 | `	}` |
|    341 | 2763 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    341 | 2764 | `	return PH7_OK;` |
|    213 | 2765 |  |
|      - | 2766 | `/*` |
|      - | 2767 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2768 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2769 | ` * Return` |
|      - | 2770 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2771 | ` */` |
|     92 | 2772 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2773 |  |
|      - | 2774 | `	const char *zHaystack,*zNeedle;` |
|      - | 2775 | `	int nHayLen,nNeedleLen;` |
|      - | 2776 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2777 | `	sxi32 rc;` |
|     96 | 2778 | `	if( nArg != 2 ){` |
|     18 | 2779 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2780 | `			"ArgumentCountError",` |
|      - | 2781 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2782 | `			nArg` |
|      - | 2783 | `			);` |
|      - | 2784 | `	}` |
|     84 | 2785 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     84 | 2786 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     84 | 2787 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2788 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2789 | `		"of type string is deprecated",` |
|      - | 2790 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     84 | 2791 | `	if( rc != PH7_OK ) goto out;` |
|     77 | 2792 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2793 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2794 | `		"of type string is deprecated",` |
|      - | 2795 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     77 | 2796 | `	if( rc != PH7_OK ) goto out;` |
|     73 | 2797 | `	if( nNeedleLen < 1 ){` |
|     13 | 2798 | `		ph7_result_bool(pCtx,1);` |
|     67 | 2799 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2800 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2801 | `	}else{` |
|     79 | 2802 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     26 | 2803 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     53 | 2804 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2805 | `	}` |
|     73 | 2806 | `	rc = PH7_OK;` |
|     41 | 2807 | `out:` |
|     84 | 2808 | `	PH7_MemObjRelease(&sHayTmp);` |
|     84 | 2809 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     84 | 2810 | `	return rc;` |
|     50 | 2811 |  |
|      - | 2812 | `/*` |
|      - | 2813 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2814 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2815 | ` * Return` |
|      - | 2816 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2817 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2818 | ` */` |
|     78 | 2819 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2820 |  |
|      - | 2821 | `	const char *zHaystack,*zNeedle;` |
|      - | 2822 | `	int nHayLen,nNeedleLen;` |
|      - | 2823 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2824 | `	sxi32 rc;` |
|     82 | 2825 | `	if( nArg != 2 ){` |
|     18 | 2826 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2827 | `			"ArgumentCountError",` |
|      - | 2828 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2829 | `			nArg` |
|      - | 2830 | `			);` |
|      - | 2831 | `	}` |
|     70 | 2832 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2833 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2834 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2835 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2836 | `		"of type string is deprecated",` |
|      - | 2837 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2838 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2839 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2840 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2841 | `		"of type string is deprecated",` |
|      - | 2842 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2843 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2844 | `	if( nNeedleLen < 1 ){` |
|     13 | 2845 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2846 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2847 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2848 | `	}else{` |
|     58 | 2849 | `		ph7_result_bool(pCtx,` |
|     38 | 2850 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2851 | `	}` |
|     59 | 2852 | `	rc = PH7_OK;` |
|     34 | 2853 | `out:` |
|     70 | 2854 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2855 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2856 | `	return rc;` |
|     43 | 2857 |  |
|      - | 2858 | `/*` |
|      - | 2859 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2860 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2861 | ` * Return` |
|      - | 2862 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2863 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2864 | ` */` |
|     78 | 2865 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2866 |  |
|      - | 2867 | `	const char *zHaystack,*zNeedle;` |
|      - | 2868 | `	int nHayLen,nNeedleLen;` |
|      - | 2869 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2870 | `	sxi32 rc;` |
|     82 | 2871 | `	if( nArg != 2 ){` |
|     18 | 2872 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2873 | `			"ArgumentCountError",` |
|      - | 2874 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2875 | `			nArg` |
|      - | 2876 | `			);` |
|      - | 2877 | `	}` |
|     70 | 2878 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2879 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2880 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2881 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2882 | `		"of type string is deprecated",` |
|      - | 2883 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2884 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2885 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2886 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2887 | `		"of type string is deprecated",` |
|      - | 2888 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2889 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2890 | `	if( nNeedleLen < 1 ){` |
|     13 | 2891 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2892 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2893 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2894 | `	}else{` |
|     58 | 2895 | `		ph7_result_bool(pCtx,` |
|     38 | 2896 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2897 | `	}` |
|     59 | 2898 | `	rc = PH7_OK;` |
|     34 | 2899 | `out:` |
|     70 | 2900 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2901 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2902 | `	return rc;` |
|     43 | 2903 |  |
|      - | 2904 | `/*` |
|      - | 2905 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2906 | ` *  Case-insensitive strpos.` |
|      - | 2907 | ` * Parameters` |
|      - | 2908 | ` *  $haystack` |
|      - | 2909 | ` *   The input string.` |
|      - | 2910 | ` * $needle` |
|      - | 2911 | ` *   Search pattern (must be a string).` |
|      - | 2912 | ` * $offset` |
|      - | 2913 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2914 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2915 | ` *   of haystack.` |
|      - | 2916 | ` * Return` |
|      - | 2917 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2918 | ` */` |
|     18 | 2919 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2920 |  |
|     19 | 2921 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2922 | `	const char *zBlob,*zPattern;` |
|      - | 2923 | `	int nLen,nPatLen,nStart;` |
|      - | 2924 | `	sxu32 nOfft;` |
|      - | 2925 | `	sxi32 rc;` |
|     19 | 2926 | `	if( nArg < 2 ){` |
|      - | 2927 | `		/* Missing arguments,return FALSE */` |
|      3 | 2928 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2929 | `		return PH7_OK;` |
|      - | 2930 | `	}` |
|      - | 2931 | `	/* Extract the needle and the haystack */` |
|     17 | 2932 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2933 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2934 | `	nOfft = 0; /* cc warning */` |
|     17 | 2935 | `	nStart = 0;` |
|      - | 2936 | `	/* Peek the starting offset if available */` |
|     17 | 2937 | `	if( nArg > 2 ){` |
|      5 | 2938 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2939 | `		if( nStart < 0 ){` |
|      3 | 2940 | `			nStart = -nStart;` |
|      1 | 2941 | `		}` |
|      5 | 2942 | `		if( nStart >= nLen ){` |
|      - | 2943 | `			/* Invalid offset */` |
|    ! 0 | 2944 | `			nStart = 0;` |
|    ! 0 | 2945 | `		}else{` |
|      5 | 2946 | `			zBlob += nStart;` |
|      5 | 2947 | `			nLen -= nStart;` |
|      - | 2948 | `		}` |
|      2 | 2949 | `	}` |
|     17 | 2950 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2951 | `		/* Perform the lookup */` |
|     17 | 2952 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2953 | `		if( rc != SXRET_OK ){` |
|      - | 2954 | `			/* Pattern not found,return FALSE */` |
|      3 | 2955 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2956 | `			return PH7_OK;` |
|      - | 2957 | `		}` |
|      - | 2958 | `		/* Return the pattern position */` |
|     15 | 2959 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2960 | `	}else{` |
|    ! 0 | 2961 | `		ph7_result_bool(pCtx,0);` |
|      - | 2962 | `	}` |
|     15 | 2963 | `	return PH7_OK;` |
|     10 | 2964 |  |
|      - | 2965 | `/*` |
|      - | 2966 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2967 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2968 | ` * Parameters` |
|      - | 2969 | ` *  $haystack` |
|      - | 2970 | ` *   The input string.` |
|      - | 2971 | ` * $needle` |
|      - | 2972 | ` *   Search pattern (must be a string).` |
|      - | 2973 | ` * $offset` |
|      - | 2974 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2975 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2976 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2977 | ` * Return` |
|      - | 2978 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2979 | ` */` |
|     32 | 2980 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2981 |  |
|      - | 2982 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2983 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2984 | `	int nLen,nPatLen;` |
|      - | 2985 | `	sxu32 nOfft;` |
|      - | 2986 | `	sxi32 rc;` |
|     33 | 2987 | `	if( nArg < 2 ){` |
|      - | 2988 | `		/* Missing arguments,return FALSE */` |
|      3 | 2989 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2990 | `		return PH7_OK;` |
|      - | 2991 | `	}` |
|      - | 2992 | `	/* Extract the needle and the haystack */` |
|     31 | 2993 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2994 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2995 | `	/* Point to the end of the pattern */` |
|     31 | 2996 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2997 | `	zEnd = &zBlob[nLen];` |
|      - | 2998 | `	/* Save the starting posistion */` |
|     31 | 2999 | `	zStart = zBlob;` |
|     31 | 3000 | `	nOfft = 0; /* cc warning */` |
|      - | 3001 | `	/* Peek the starting offset if available */` |
|     31 | 3002 | `	if( nArg > 2 ){` |
|      - | 3003 | `		int nStart;` |
|     21 | 3004 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3005 | `		if( nStart < 0 ){` |
|     11 | 3006 | `			nStart = -nStart;` |
|     11 | 3007 | `			if( nStart >= nLen ){` |
|      - | 3008 | `				/* Invalid offset */` |
|      3 | 3009 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3010 | `				return PH7_OK;` |
|    ! 0 | 3011 | `			}else{` |
|      9 | 3012 | `				nLen -= nStart;` |
|      9 | 3013 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3014 | `				zEnd = &zBlob[nLen];` |
|      - | 3015 | `			}` |
|      5 | 3016 | `		}else{` |
|     11 | 3017 | `			if( nStart >= nLen ){` |
|      - | 3018 | `				/* Invalid offset */` |
|      5 | 3019 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3020 | `				return PH7_OK;` |
|    ! 0 | 3021 | `			}else{` |
|      7 | 3022 | `				zBlob += nStart;` |
|      7 | 3023 | `				nLen -= nStart;` |
|      - | 3024 | `			}` |
|      - | 3025 | `		}` |
|      7 | 3026 | `	}` |
|     25 | 3027 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3028 | `		/* Perform the lookup */` |
|     57 | 3029 | `		for(;;){` |
|    115 | 3030 | `			if( zBlob >= zPtr ){` |
|     11 | 3031 | `				break;` |
|      - | 3032 | `			}` |
|    105 | 3033 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3034 | `			if( rc == SXRET_OK ){` |
|      - | 3035 | `				/* Pattern found,return it's position */` |
|     13 | 3036 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3037 | `				return PH7_OK;` |
|      - | 3038 | `			}` |
|     93 | 3039 | `			zPtr--;` |
|      1 | 3040 | `		}` |
|      - | 3041 | `		/* Pattern not found,return FALSE */` |
|     11 | 3042 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3043 | `	}else{` |
|      3 | 3044 | `		ph7_result_bool(pCtx,0);` |
|      - | 3045 | `	}` |
|     13 | 3046 | `	return PH7_OK;` |
|     17 | 3047 |  |
|      - | 3048 | `/*` |
|      - | 3049 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3050 | ` *  Case-insensitive strrpos.` |
|      - | 3051 | ` * Parameters` |
|      - | 3052 | ` *  $haystack` |
|      - | 3053 | ` *   The input string.` |
|      - | 3054 | ` * $needle` |
|      - | 3055 | ` *   Search pattern (must be a string).` |
|      - | 3056 | ` * $offset` |
|      - | 3057 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3058 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3059 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3060 | ` * Return` |
|      - | 3061 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3062 | ` */` |
|     28 | 3063 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3064 |  |
|      - | 3065 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3066 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3067 | `	int nLen,nPatLen;` |
|      - | 3068 | `	sxu32 nOfft;` |
|      - | 3069 | `	sxi32 rc;` |
|     29 | 3070 | `	if( nArg < 2 ){` |
|      - | 3071 | `		/* Missing arguments,return FALSE */` |
|      3 | 3072 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3073 | `		return PH7_OK;` |
|      - | 3074 | `	}` |
|      - | 3075 | `	/* Extract the needle and the haystack */` |
|     27 | 3076 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3077 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3078 | `	/* Point to the end of the pattern */` |
|     27 | 3079 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3080 | `	zEnd = &zBlob[nLen];` |
|      - | 3081 | `	/* Save the starting posistion */` |
|     27 | 3082 | `	zStart = zBlob;` |
|     27 | 3083 | `	nOfft = 0; /* cc warning */` |
|      - | 3084 | `	/* Peek the starting offset if available */` |
|     27 | 3085 | `	if( nArg > 2 ){` |
|      - | 3086 | `		int nStart;` |
|     15 | 3087 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3088 | `		if( nStart < 0 ){` |
|      7 | 3089 | `			nStart = -nStart;` |
|      7 | 3090 | `			if( nStart >= nLen ){` |
|      - | 3091 | `				/* Invalid offset */` |
|      3 | 3092 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3093 | `				return PH7_OK;` |
|    ! 0 | 3094 | `			}else{` |
|      5 | 3095 | `				nLen -= nStart;` |
|      5 | 3096 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3097 | `				zEnd = &zBlob[nLen];` |
|      - | 3098 | `			}` |
|      3 | 3099 | `		}else{` |
|      9 | 3100 | `			if( nStart >= nLen ){` |
|      - | 3101 | `				/* Invalid offset */` |
|      5 | 3102 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3103 | `				return PH7_OK;` |
|    ! 0 | 3104 | `			}else{` |
|      5 | 3105 | `				zBlob += nStart;` |
|      5 | 3106 | `				nLen -= nStart;` |
|      - | 3107 | `			}` |
|      - | 3108 | `		}` |
|      4 | 3109 | `	}` |
|     21 | 3110 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3111 | `		/* Perform the lookup */` |
|     44 | 3112 | `		for(;;){` |
|     89 | 3113 | `			if( zBlob >= zPtr ){` |
|      9 | 3114 | `				break;` |
|      - | 3115 | `			}` |
|     81 | 3116 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3117 | `			if( rc == SXRET_OK ){` |
|      - | 3118 | `				/* Pattern found,return it's position */` |
|     11 | 3119 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3120 | `				return PH7_OK;` |
|      - | 3121 | `			}` |
|     71 | 3122 | `			zPtr--;` |
|      1 | 3123 | `		}` |
|      - | 3124 | `		/* Pattern not found,return FALSE */` |
|      9 | 3125 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3126 | `	}else{` |
|      3 | 3127 | `		ph7_result_bool(pCtx,0);` |
|      - | 3128 | `	}` |
|     11 | 3129 | `	return PH7_OK;` |
|     15 | 3130 |  |
|      - | 3131 | `/*` |
|      - | 3132 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3133 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3134 | ` * Parameters` |
|      - | 3135 | ` *  $haystack` |
|      - | 3136 | ` *   The input string.` |
|      - | 3137 | ` * $needle` |
|      - | 3138 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3139 | ` *  This behavior is different from that of strstr().` |
|      - | 3140 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3141 | ` *  as the ordinal value of a character.` |
|      - | 3142 | ` * Return` |
|      - | 3143 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3144 | ` */` |
|     24 | 3145 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3146 |  |
|      - | 3147 | `	const char *zBlob;` |
|      - | 3148 | `	int nLen,c;` |
|     25 | 3149 | `	if( nArg < 2 ){` |
|      - | 3150 | `		/* Missing arguments,return FALSE */` |
|      3 | 3151 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3152 | `		return PH7_OK;` |
|      - | 3153 | `	}` |
|      - | 3154 | `	/* Extract the haystack */` |
|     23 | 3155 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3156 | `	c = 0; /* cc warning */` |
|     23 | 3157 | `	if( nLen > 0 ){` |
|      - | 3158 | `		sxu32 nOfft;` |
|      - | 3159 | `		sxi32 rc;` |
|     21 | 3160 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3161 | `			const char *zPattern;` |
|     11 | 3162 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3163 | `														 * for NULL pointer.` |
|      - | 3164 | `														 */` |
|     11 | 3165 | `			c = zPattern[0];` |
|      6 | 3166 | `		}else{` |
|      - | 3167 | `			/* Int cast */` |
|     11 | 3168 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3169 | `		}` |
|      - | 3170 | `		/* Perform the lookup */` |
|     21 | 3171 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3172 | `		if( rc != SXRET_OK ){` |
|      - | 3173 | `			/* No such entry,return FALSE */` |
|      7 | 3174 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3175 | `			return PH7_OK;` |
|      - | 3176 | `		}` |
|      - | 3177 | `		/* Return the string portion */` |
|     15 | 3178 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3179 | `	}else{` |
|      3 | 3180 | `		ph7_result_bool(pCtx,0);` |
|      - | 3181 | `	}` |
|     17 | 3182 | `	return PH7_OK;` |
|     13 | 3183 |  |
|      - | 3184 | `/*` |
|      - | 3185 | ` * string strrev(string $string)` |
|      - | 3186 | ` *  Reverse a string.` |
|      - | 3187 | ` * Parameters` |
|      - | 3188 | ` *  $string` |
|      - | 3189 | ` *   String to be reversed.` |
|      - | 3190 | ` * Return` |
|      - | 3191 | ` *  The reversed string.` |
|      - | 3192 | ` */` |
|      4 | 3193 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3194 |  |
|      - | 3195 | `	const char *zIn,*zEnd;` |
|      - | 3196 | `	int nLen,c;` |
|      5 | 3197 | `	if( nArg < 1 ){` |
|      - | 3198 | `		/* Missing arguments,return NULL */` |
|      3 | 3199 | `		ph7_result_null(pCtx);` |
|      3 | 3200 | `		return PH7_OK;` |
|      - | 3201 | `	}` |
|      - | 3202 | `	/* Extract the target string */` |
|      3 | 3203 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3204 | `	if( nLen < 1 ){` |
|      - | 3205 | `		/* Empty string Return null */` |
|    ! 0 | 3206 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3207 | `		return PH7_OK;` |
|      - | 3208 | `	}` |
|      - | 3209 | `	/* Perform the requested operation */` |
|      3 | 3210 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3211 | `	for(;;){` |
|      9 | 3212 | `		if( zEnd < zIn ){` |
|      - | 3213 | `			/* No more input to process */` |
|      3 | 3214 | `			break;` |
|      - | 3215 | `		}` |
|      - | 3216 | `		/* Append current character */` |
|      7 | 3217 | `		c = zEnd[0];` |
|      7 | 3218 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3219 | `		zEnd--;` |
|      1 | 3220 | `	}` |
|      3 | 3221 | `	return PH7_OK;` |
|      3 | 3222 |  |
|      - | 3223 | `/*` |
|      - | 3224 | ` * string ucwords(string $string)` |
|      - | 3225 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3226 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3227 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3228 | ` * Parameters` |
|      - | 3229 | ` *  $string` |
|      - | 3230 | ` *   The input string.` |
|      - | 3231 | ` * Return` |
|      - | 3232 | ` *  The modified string..` |
|      - | 3233 | ` */` |
|     14 | 3234 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3235 |  |
|      - | 3236 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3237 | `	int nLen,c;` |
|     15 | 3238 | `	if( nArg < 1 ){` |
|      - | 3239 | `		/* Missing arguments,return NULL */` |
|      3 | 3240 | `		ph7_result_null(pCtx);` |
|      3 | 3241 | `		return PH7_OK;` |
|      - | 3242 | `	}` |
|      - | 3243 | `	/* Extract the target string */` |
|     13 | 3244 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3245 | `	if( nLen < 1 ){` |
|      - | 3246 | `		/* Empty string – match PHP semantics */` |
|      3 | 3247 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3248 | `		return PH7_OK;` |
|      - | 3249 | `	}` |
|      - | 3250 | `	/* Perform the requested operation */` |
|     11 | 3251 | `	zEnd = &zIn[nLen];` |
|     21 | 3252 | `	for(;;){` |
|      - | 3253 | `		/* Jump leading white spaces */` |
|     43 | 3254 | `		zCur = zIn;` |
|     65 | 3255 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3256 | `			zIn++;` |
|      1 | 3257 | `		}` |
|     43 | 3258 | `		if( zCur < zIn ){` |
|      - | 3259 | `			/* Append white space stream */` |
|     23 | 3260 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3261 | `		}` |
|     43 | 3262 | `		if( zIn >= zEnd ){` |
|      - | 3263 | `			/* No more input to process */` |
|     11 | 3264 | `			break;` |
|      - | 3265 | `		}` |
|     33 | 3266 | `		c = zIn[0];` |
|     33 | 3267 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3268 | `			c = SyToUpper(c);` |
|     14 | 3269 | `		}` |
|      - | 3270 | `		/* Append the upper-cased character */` |
|     33 | 3271 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3272 | `		zIn++;` |
|     33 | 3273 | `		zCur = zIn;` |
|      - | 3274 | `		/* Append the word varbatim */` |
|    149 | 3275 | `		while( zIn < zEnd ){` |
|    139 | 3276 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3277 | `				/* UTF-8 stream */` |
|    ! 0 | 3278 | `				zIn++;` |
|    ! 0 | 3279 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3280 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3281 | `				zIn++;` |
|     59 | 3282 | `			}else{` |
|     23 | 3283 | `				break;` |
|      - | 3284 | `			}` |
|      1 | 3285 | `		}` |
|     33 | 3286 | `		if( zCur < zIn ){` |
|     33 | 3287 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3288 | `		}` |
|      1 | 3289 | `	}` |
|     11 | 3290 | `	return PH7_OK;` |
|      8 | 3291 |  |
|      - | 3292 | `/*` |
|      - | 3293 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3294 | ` *  Returns input repeated multiplier times.` |
|      - | 3295 | ` * Parameters` |
|      - | 3296 | ` *  $string` |
|      - | 3297 | ` *   String to be repeated.` |
|      - | 3298 | ` * $multiplier` |
|      - | 3299 | ` *  Number of time the input string should be repeated.` |
|      - | 3300 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3301 | ` *  to 0, the function will return an empty string.` |
|      - | 3302 | ` * Return` |
|      - | 3303 | ` *  The repeated string.` |
|      - | 3304 | ` */` |
|  20226 | 3305 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3306 |  |
|      - | 3307 | `	const char *zIn;` |
|      - | 3308 | `	int nLen,nMul;` |
|      - | 3309 | `	int rc;` |
|  20227 | 3310 | `	if( nArg < 2 ){` |
|      - | 3311 | `		/* Missing arguments,return NULL */` |
|      3 | 3312 | `		ph7_result_null(pCtx);` |
|      3 | 3313 | `		return PH7_OK;` |
|      - | 3314 | `	}` |
|      - | 3315 | `	/* Extract the target string */` |
|  20225 | 3316 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20225 | 3317 | `	if( nLen < 1 ){` |
|      - | 3318 | `		/* Empty string.Return null */` |
|    ! 0 | 3319 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3320 | `		return PH7_OK;` |
|      - | 3321 | `	}` |
|      - | 3322 | `	/* Extract the multiplier */` |
|  20225 | 3323 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20225 | 3324 | `	if( nMul < 1 ){` |
|      - | 3325 | `		/* Return the empty string */` |
|      3 | 3326 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3327 | `		return PH7_OK;` |
|      - | 3328 | `	}` |
|      - | 3329 | `	/* Perform the requested operation */` |
| 120878 | 3330 | `	for(;;){` |
| 241757 | 3331 | `		if( !nMul ){` |
|  20223 | 3332 | `			break;` |
|      - | 3333 | `		}` |
|      - | 3334 | `		/* Append the copy */` |
| 221535 | 3335 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 221535 | 3336 | `		if( rc != PH7_OK ){` |
|      - | 3337 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3338 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3339 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3340 | `		}` |
| 221535 | 3341 | `		nMul--;` |
|      1 | 3342 | `	}` |
|  20223 | 3343 | `	return PH7_OK;` |
|  10114 | 3344 |  |
|      - | 3345 | `/*` |
|      - | 3346 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3347 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3348 | ` * Parameters` |
|      - | 3349 | ` *  $string` |
|      - | 3350 | ` *   The input string.` |
|      - | 3351 | ` * $is_xhtml` |
|      - | 3352 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3353 | ` * Return` |
|      - | 3354 | ` *  The processed string.` |
|      - | 3355 | ` */` |
|      6 | 3356 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3357 |  |
|      - | 3358 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3359 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3360 | `	int nLen;` |
|      7 | 3361 | `	if( nArg < 1 ){` |
|      - | 3362 | `		/* Missing arguments,return the empty string */` |
|      3 | 3363 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3364 | `		return PH7_OK;` |
|      - | 3365 | `	}` |
|      - | 3366 | `	/* Extract the target string */` |
|      5 | 3367 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3368 | `	if( nLen < 1 ){` |
|      - | 3369 | `		/* Empty string,return null */` |
|    ! 0 | 3370 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3371 | `		return PH7_OK;` |
|      - | 3372 | `	}` |
|      5 | 3373 | `	if( nArg > 1 ){` |
|      3 | 3374 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3375 | `	}` |
|      5 | 3376 | `	zEnd = &zIn[nLen];` |
|      - | 3377 | `	/* Perform the requested operation */` |
|      4 | 3378 | `	for(;;){` |
|      9 | 3379 | `		zCur = zIn;` |
|      - | 3380 | `		/* Delimit the string */` |
|     21 | 3381 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3382 | `			zIn++;` |
|      1 | 3383 | `		}` |
|      9 | 3384 | `		if( zCur < zIn ){` |
|      - | 3385 | `			/* Output chunk verbatim */` |
|      9 | 3386 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3387 | `		}` |
|      9 | 3388 | `		if( zIn >= zEnd ){` |
|      - | 3389 | `			/* No more input to process */` |
|      5 | 3390 | `			break;` |
|      - | 3391 | `		}` |
|      - | 3392 | `		/* Output the HTML line break */` |
|      - | 3393 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3394 | `		if( is_xhtml ){` |
|      3 | 3395 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3396 | `		}else{` |
|      3 | 3397 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3398 | `		}` |
|      5 | 3399 | `		zCur = zIn;` |
|      - | 3400 | `		/* Append trailing line */` |
|     11 | 3401 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3402 | `			zIn++;` |
|      1 | 3403 | `		}` |
|      5 | 3404 | `		if( zCur < zIn ){` |
|      - | 3405 | `			/* Output chunk verbatim */` |
|      5 | 3406 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3407 | `		}` |
|      1 | 3408 | `	}` |
|      5 | 3409 | `	return PH7_OK;` |
|      4 | 3410 |  |
|      - | 3411 | `/*` |
|      - | 3412 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3413 | ` *  According to the PHP reference manual.` |
|      - | 3414 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3415 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3416 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3417 | ` * This applies to both sprintf() and printf().` |
|      - | 3418 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3419 | ` * or more of these elements, in order:` |
|      - | 3420 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3421 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3422 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3423 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3424 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3425 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3426 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3427 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3428 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3429 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3430 | ` *   should result in.` |
|      - | 3431 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3432 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3433 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3434 | ` *   limit to the string.` |
|      - | 3435 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3436 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3437 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3438 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3439 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3440 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3441 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3442 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3443 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3444 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3445 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3446 | ` *       g - shorter of %e and %f.` |
|      - | 3447 | ` *       G - shorter of %E and %f.` |
|      - | 3448 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3449 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3450 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3451 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3452 | ` */` |
|      - | 3453 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3454 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3455 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3456 | `/*` |
|      - | 3457 | `** Conversion types fall into various categories as defined by the` |
|      - | 3458 | `** following enumeration.` |
|      - | 3459 | `*/` |
|      - | 3460 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3461 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3462 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3463 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3464 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3465 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3466 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3467 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3468 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3469 |  |
|      - | 3470 | `/*` |
|      - | 3471 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3472 | `*/` |
|      - | 3473 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3474 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3475 | `/*` |
|      - | 3476 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3477 | `** by an instance of the following structure` |
|      - | 3478 | `*/` |
|      - | 3479 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3480 | `struct ph7_fmt_info` |
|      - | 3481 |  |
|      - | 3482 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3483 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3484 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3485 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3486 | `  char *charset; /* The character set for conversion */` |
|      - | 3487 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3488 | `};` |
|      - | 3489 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3490 | `/*` |
|      - | 3491 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3492 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3493 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3494 | `**` |
|      - | 3495 | `** Example:` |
|      - | 3496 | `**     input:     *val = 3.14159` |
|      - | 3497 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3498 | `**` |
|      - | 3499 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3500 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3501 | `** always returned.` |
|      - | 3502 | `*/` |
|    422 | 3503 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3504 |  |
|      - | 3505 | `  sxlongreal d;` |
|      - | 3506 | `  int digit;` |
|      - | 3507 |  |
|    423 | 3508 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3509 | `	  return '0';` |
|      - | 3510 | `  }` |
|    423 | 3511 | `  digit = (int)*val;` |
|    423 | 3512 | `  d = digit;` |
|    423 | 3513 | `   *val = (*val - d)*10.0;` |
|    423 | 3514 | `  return digit + '0' ;` |
|    212 | 3515 |  |
|      - | 3516 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3517 | `/*` |
|      - | 3518 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3519 | ` * used conversion types first.` |
|      - | 3520 | ` */` |
|      - | 3521 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3522 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3523 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3524 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3525 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3526 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3527 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3528 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3529 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3530 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3531 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3532 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3533 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3534 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3535 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3536 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3537 | `};` |
|      - | 3538 | `/*` |
|      - | 3539 | ` * Format a given string.` |
|      - | 3540 | ` * The root program.  All variations call this core.` |
|      - | 3541 | ` * INPUTS:` |
|      - | 3542 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3543 | ` *            1. A pointer to the call context.` |
|      - | 3544 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3545 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3546 | ` *            3. An integer number of characters to be output.` |
|      - | 3547 | ` *               (Note: This number might be zero.)` |
|      - | 3548 | ` *            4. Upper layer private data.` |
|      - | 3549 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3550 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3551 | ` */` |
|    136 | 3552 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3553 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3554 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3555 | `	const char *zIn,    /* Format string */` |
|      - | 3556 | `	int nByte,          /* Format string length */` |
|      - | 3557 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3558 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3559 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3560 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3561 | `	)` |
|      1 | 3562 |  |
|    137 | 3563 | `	char spaces[] = "                                                  ";` |
|      - | 3564 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    137 | 3565 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3566 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3567 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3568 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3569 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3570 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3571 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3572 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3573 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3574 | `	ph7_int64 iVal;` |
|      - | 3575 | `	int precision;           /* Precision of the current field */` |
|      - | 3576 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3577 | `	int c,rc,n;` |
|      - | 3578 | `	int length;              /* Length of the field */` |
|      - | 3579 | `	int prefix;` |
|      - | 3580 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3581 | `	int width;               /* Width of the current field */` |
|      - | 3582 | `	int idx;` |
|    137 | 3583 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3584 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3585 | `	/* Start the format process */` |
|    139 | 3586 | `	for(;;){` |
|    279 | 3587 | `		zCur = zIn;` |
|    739 | 3588 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    461 | 3589 | `			zIn++;` |
|      1 | 3590 | `		}` |
|    279 | 3591 | `		if( zCur < zIn ){` |
|      - | 3592 | `			/* Consume chunk verbatim */` |
|    105 | 3593 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    105 | 3594 | `			if( rc != SXRET_OK ){` |
|      - | 3595 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3596 | `				break;` |
|      - | 3597 | `			}` |
|     52 | 3598 | `		}` |
|    279 | 3599 | `		if( zIn >= zEnd ){` |
|      - | 3600 | `			/* No more input to process,break immediately */` |
|    135 | 3601 | `			break;` |
|      - | 3602 | `		}` |
|      - | 3603 | `		/* Find out what flags are present */` |
|    145 | 3604 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    144 | 3605 | `			flag_alternateform = flag_zeropad = 0;` |
|    145 | 3606 | `		zIn++; /* Jump the precent sign */` |
|     72 | 3607 | `		do{` |
|    177 | 3608 | `			c = zIn[0];` |
|    177 | 3609 | `			switch( c ){` |
|      9 | 3610 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3611 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3612 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3613 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3614 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3615 | `			case '\'':` |
|    ! 0 | 3616 | `				zIn++;` |
|    ! 0 | 3617 | `				if( zIn < zEnd ){` |
|      - | 3618 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3619 | `					c = zIn[0];` |
|    ! 0 | 3620 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3621 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3622 | `					}` |
|    ! 0 | 3623 | `					c = 0;` |
|    ! 0 | 3624 | `				}` |
|    ! 0 | 3625 | `				break;` |
|    144 | 3626 | `			default:                                       break;` |
|      - | 3627 | `			}` |
|    177 | 3628 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3629 | `		/* Get the field width */` |
|    145 | 3630 | `		width = 0;` |
|    251 | 3631 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3632 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3633 | `			zIn++;` |
|      1 | 3634 | `		}` |
|    145 | 3635 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3636 | `			/* Position specifer */` |
|    ! 0 | 3637 | `			if( width > 0 ){` |
|    ! 0 | 3638 | `				n = width;` |
|    ! 0 | 3639 | `				if( vf && n > 0 ){` |
|    ! 0 | 3640 | `					n--;` |
|    ! 0 | 3641 | `				}` |
|    ! 0 | 3642 | `			}` |
|    ! 0 | 3643 | `			zIn++;` |
|    ! 0 | 3644 | `			width = 0;` |
|    ! 0 | 3645 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3646 | `				flag_zeropad = 1;` |
|    ! 0 | 3647 | `				zIn++;` |
|    ! 0 | 3648 | `			}` |
|    ! 0 | 3649 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3650 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3651 | `				zIn++;` |
|    ! 0 | 3652 | `			}` |
|    ! 0 | 3653 | `		}` |
|    145 | 3654 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3655 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3656 | `		}` |
|      - | 3657 | `		/* Get the precision */` |
|    145 | 3658 | `		precision = -1;` |
|    145 | 3659 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3660 | `			precision = 0;` |
|     59 | 3661 | `			zIn++;` |
|    150 | 3662 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3663 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3664 | `				zIn++;` |
|      1 | 3665 | `			}` |
|     29 | 3666 | `		}` |
|    145 | 3667 | `		if( zIn >= zEnd ){` |
|      - | 3668 | `			/* No more input */` |
|      3 | 3669 | `			break;` |
|      - | 3670 | `		}` |
|      - | 3671 | `		/* Fetch the info entry for the field */` |
|    143 | 3672 | `		pInfo = 0;` |
|    143 | 3673 | `		xtype = PH7_FMT_ERROR;` |
|    143 | 3674 | `		c = zIn[0];` |
|    143 | 3675 | `		zIn++; /* Jump the format specifer */` |
|    787 | 3676 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    785 | 3677 | `			if( c==aFmt[idx].fmttype ){` |
|    141 | 3678 | `				pInfo = &aFmt[idx];` |
|    141 | 3679 | `				xtype = pInfo->type;` |
|    141 | 3680 | `				break;` |
|      - | 3681 | `			}` |
|    323 | 3682 | `		}` |
|    143 | 3683 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    143 | 3684 | `		length = 0;` |
|      - | 3685 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3686 | `		 /*` |
|      - | 3687 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3688 | `		  **` |
|      - | 3689 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3690 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3691 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3692 | `		  **                               field width was negative.` |
|      - | 3693 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3694 | `		  **                               the conversion character.` |
|      - | 3695 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3696 | `		  **   width                       The specified field width.  This is` |
|      - | 3697 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3698 | `		  **   precision                   The specified precision.  The default` |
|      - | 3699 | `		  **                               is -1.` |
|      - | 3700 | `		  */` |
|    143 | 3701 | `		switch(xtype){` |
|    ! 0 | 3702 | `		case PH7_FMT_PERCENT:` |
|      - | 3703 | `			/* A literal percent character */` |
|    ! 0 | 3704 | `			zWorker[0] = '%';` |
|    ! 0 | 3705 | `			length = (int)sizeof(char);` |
|    ! 0 | 3706 | `			break;` |
|      3 | 3707 | `		case PH7_FMT_CHARX:` |
|      - | 3708 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3709 | `			 * with that ASCII value` |
|      - | 3710 | `			 */` |
|      7 | 3711 | `			pArg = NEXT_ARG;` |
|      7 | 3712 | `			if( pArg == 0 ){` |
|      3 | 3713 | `				c = 0;` |
|      2 | 3714 | `			}else{` |
|      5 | 3715 | `				c = ph7_value_to_int(pArg);` |
|      - | 3716 | `			}` |
|      - | 3717 | `			/* NUL byte is an acceptable value */` |
|      7 | 3718 | `			zWorker[0] = (char)c;` |
|      7 | 3719 | `			length = (int)sizeof(char);` |
|      7 | 3720 | `			break;` |
|     12 | 3721 | `		case PH7_FMT_STRING:` |
|      - | 3722 | `			/* the argument is treated as and presented as a string */` |
|     25 | 3723 | `			pArg = NEXT_ARG;` |
|     25 | 3724 | `			if( pArg == 0 ){` |
|    ! 0 | 3725 | `				length = 0;` |
|    ! 0 | 3726 | `			}else{` |
|     25 | 3727 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3728 | `			}` |
|     25 | 3729 | `			if( length < 1 ){` |
|    ! 0 | 3730 | `				zBuf = " ";` |
|    ! 0 | 3731 | `				length = (int)sizeof(char);` |
|    ! 0 | 3732 | `			}` |
|     25 | 3733 | `			if( precision>=0 && precision<length ){` |
|      3 | 3734 | `				length = precision;` |
|      1 | 3735 | `			}` |
|     25 | 3736 | `			if( flag_zeropad ){` |
|      - | 3737 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3738 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3739 | `					spaces[idx] = '0';` |
|    ! 0 | 3740 | `				}` |
|    ! 0 | 3741 | `			}` |
|     25 | 3742 | `			break;` |
|     27 | 3743 | `		case PH7_FMT_RADIX:` |
|     55 | 3744 | `			pArg = NEXT_ARG;` |
|     55 | 3745 | `			if( pArg == 0 ){` |
|    ! 0 | 3746 | `				iVal = 0;` |
|    ! 0 | 3747 | `			}else{` |
|     55 | 3748 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3749 | `			}` |
|      - | 3750 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     55 | 3751 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3752 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3753 | `			}` |
|      - | 3754 | `#if 1` |
|      - | 3755 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3756 | `        ** I think this is stupid.*/` |
|     55 | 3757 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3758 | `#else` |
|      - | 3759 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3760 | `        ** but leave the prefix for hex.*/` |
|      - | 3761 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3762 | `#endif` |
|     55 | 3763 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     25 | 3764 | `          if( iVal<0 ){` |
|      3 | 3765 | `            iVal = -iVal;` |
|      - | 3766 | `			/* Ticket 1433-003 */` |
|      3 | 3767 | `			if( iVal < 0 ){` |
|      - | 3768 | `				/* Overflow */` |
|    ! 0 | 3769 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3770 | `			}` |
|      3 | 3771 | `            prefix = '-';` |
|     24 | 3772 | `          }else if( flag_plussign )  prefix = '+';` |
|     21 | 3773 | `          else if( flag_blanksign )  prefix = ' ';` |
|     19 | 3774 | `          else                       prefix = 0;` |
|     13 | 3775 | `        }else{` |
|     31 | 3776 | `			if( iVal<0 ){` |
|    ! 0 | 3777 | `				iVal = -iVal;` |
|      - | 3778 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3779 | `				if( iVal < 0 ){` |
|      - | 3780 | `					/* Overflow */` |
|    ! 0 | 3781 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3782 | `				}` |
|    ! 0 | 3783 | `			}` |
|     31 | 3784 | `			prefix = 0;` |
|      - | 3785 | `		}` |
|     55 | 3786 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3787 | `          precision = width-(prefix!=0);` |
|      3 | 3788 | `        }` |
|     55 | 3789 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3790 | `        {` |
|      - | 3791 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3792 | `          register int base;` |
|     55 | 3793 | `          cset = pInfo->charset;` |
|     55 | 3794 | `          base = pInfo->base;` |
|     27 | 3795 | `          do{                                           /* Convert to ascii */` |
|    123 | 3796 | `            *(--zBuf) = cset[iVal%base];` |
|    123 | 3797 | `            iVal = iVal/base;` |
|    123 | 3798 | `          }while( iVal>0 );` |
|      - | 3799 | `        }` |
|     55 | 3800 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     77 | 3801 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3802 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3803 | `        }` |
|     55 | 3804 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     55 | 3805 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3806 | `          char *pre, x;` |
|      9 | 3807 | `          pre = pInfo->prefix;` |
|      9 | 3808 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3809 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3810 | `          }` |
|      4 | 3811 | `        }` |
|     55 | 3812 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 3813 | `		break;` |
|     28 | 3814 | `		case PH7_FMT_FLOAT:` |
|      - | 3815 | `		case PH7_FMT_EXP:` |
|      - | 3816 | `		case PH7_FMT_GENERIC:{` |
|      - | 3817 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3818 | `		long double realvalue;` |
|      - | 3819 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3820 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3821 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3822 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3823 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3824 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3825 | `		pArg = NEXT_ARG;` |
|     57 | 3826 | `		if( pArg == 0 ){` |
|    ! 0 | 3827 | `			realvalue = 0;` |
|    ! 0 | 3828 | `		}else{` |
|     57 | 3829 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3830 | `		}` |
|      - | 3831 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3832 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3833 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3834 | `			zBuf = "NAN";` |
|    ! 0 | 3835 | `			length = 3;` |
|    ! 0 | 3836 | `			break;` |
|      - | 3837 | `		}` |
|     57 | 3838 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3839 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3840 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3841 | `				zBuf = "-INF";` |
|    ! 0 | 3842 | `				length = 4;` |
|    ! 0 | 3843 | `			}else{` |
|    ! 0 | 3844 | `				zBuf = "INF";` |
|    ! 0 | 3845 | `				length = 3;` |
|      - | 3846 | `			}` |
|    ! 0 | 3847 | `			break;` |
|      - | 3848 | `		}` |
|     57 | 3849 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3850 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3851 | `        if( realvalue<0.0 ){` |
|      3 | 3852 | `          realvalue = -realvalue;` |
|      3 | 3853 | `          prefix = '-';` |
|      2 | 3854 | `        }else{` |
|     55 | 3855 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3856 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3857 | `          else                         prefix = 0;` |
|      - | 3858 | `        }` |
|     57 | 3859 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3860 | `        rounder = 0.0;` |
|      - | 3861 | `#if 0` |
|      - | 3862 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3863 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3864 | `#else` |
|      - | 3865 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3866 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3867 | `#endif` |
|     57 | 3868 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3869 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3870 | `        exp = 0;` |
|     57 | 3871 | `        if( realvalue>0.0 ){` |
|     61 | 3872 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3873 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3874 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3875 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3876 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3877 | `            zBuf = "NaN";` |
|    ! 0 | 3878 | `            length = 3;` |
|    ! 0 | 3879 | `            break;` |
|      - | 3880 | `          }` |
|     28 | 3881 | `        }` |
|     57 | 3882 | `        zBuf = zWorker;` |
|      - | 3883 | `        /*` |
|      - | 3884 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3885 | `        ** or etFLOAT, as appropriate.` |
|      - | 3886 | `        */` |
|     57 | 3887 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3888 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3889 | `          realvalue += rounder;` |
|    ! 0 | 3890 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3891 | `        }` |
|     57 | 3892 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3893 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3894 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3895 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3896 | `          }else{` |
|    ! 0 | 3897 | `            precision = precision - exp;` |
|    ! 0 | 3898 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3899 | `          }` |
|    ! 0 | 3900 | `        }else{` |
|     57 | 3901 | `          flag_rtz = 0;` |
|      - | 3902 | `        }` |
|      - | 3903 | `        /*` |
|      - | 3904 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3905 | `        ** the precision is too large to fit in buf[].` |
|      - | 3906 | `        */` |
|     57 | 3907 | `        nsd = 0;` |
|     57 | 3908 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3909 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3910 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3911 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3912 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3913 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3914 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3915 | `            *(zBuf++) = '0';` |
|     17 | 3916 | `          }` |
|    373 | 3917 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3918 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3919 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3920 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3921 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3922 | `          }` |
|     57 | 3923 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3924 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3925 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3926 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3927 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3928 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3929 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3930 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3931 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3932 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3933 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3934 | `          }` |
|    ! 0 | 3935 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3936 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3937 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3938 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3939 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3940 | `            if( exp>=100 ){` |
|    ! 0 | 3941 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3942 | `              exp %= 100;` |
|    ! 0 | 3943 | `            }` |
|    ! 0 | 3944 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3945 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3946 | `          }` |
|      - | 3947 | `        }` |
|      - | 3948 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3949 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3950 | `        ** integer conversions.*/` |
|     57 | 3951 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3952 | `        zBuf = zWorker;` |
|      - | 3953 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3954 | `        ** set and we are not left justified */` |
|     57 | 3955 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3956 | `          int i;` |
|      3 | 3957 | `          int nPad = width - length;` |
|     13 | 3958 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3959 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3960 | `          }` |
|      3 | 3961 | `          i = prefix!=0;` |
|      5 | 3962 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3963 | `          length = width;` |
|      1 | 3964 | `        }` |
|      - | 3965 | `#else` |
|      - | 3966 | `         zBuf = " ";` |
|      - | 3967 | `		 length = (int)sizeof(char);` |
|      - | 3968 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3969 | `		 break;` |
|      - | 3970 | `							 }` |
|      1 | 3971 | `		default:` |
|      - | 3972 | `			/* Invalid format specifer */` |
|      3 | 3973 | `			zWorker[0] = '?';` |
|      3 | 3974 | `			length = (int)sizeof(char);` |
|      2 | 3975 | `			break;` |
|      - | 3976 | `		}` |
|      - | 3977 | `		 /*` |
|      - | 3978 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3979 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3980 | `		 ** the output.` |
|      - | 3981 | `		 */` |
|    143 | 3982 | `    if( !flag_leftjustify ){` |
|      - | 3983 | `      register int nspace;` |
|    135 | 3984 | `      nspace = width-length;` |
|    135 | 3985 | `      if( nspace>0 ){` |
|      5 | 3986 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3987 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3988 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3989 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3990 | `			}` |
|    ! 0 | 3991 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3992 | `        }` |
|      5 | 3993 | `        if( nspace>0 ){` |
|      5 | 3994 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3995 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3996 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3997 | `			}` |
|      2 | 3998 | `		}` |
|      2 | 3999 | `      }` |
|     67 | 4000 | `    }` |
|    143 | 4001 | `    if( length>0 ){` |
|    143 | 4002 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    143 | 4003 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4004 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4005 | `		}` |
|     71 | 4006 | `    }` |
|    143 | 4007 | `    if( flag_leftjustify ){` |
|      - | 4008 | `      register int nspace;` |
|      9 | 4009 | `      nspace = width-length;` |
|      9 | 4010 | `      if( nspace>0 ){` |
|      9 | 4011 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4012 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4013 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4014 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4015 | `			}` |
|    ! 0 | 4016 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4017 | `        }` |
|      9 | 4018 | `        if( nspace>0 ){` |
|      9 | 4019 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 4020 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4021 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4022 | `			}` |
|      4 | 4023 | `		}` |
|      4 | 4024 | `      }` |
|      4 | 4025 | `    }` |
|      1 | 4026 | ` }/* for(;;) */` |
|    137 | 4027 | `	return SXRET_OK;` |
|     69 | 4028 |  |
|      - | 4029 | `/*` |
|      - | 4030 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4031 | ` */` |
|     90 | 4032 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4033 |  |
|      - | 4034 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4035 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4036 | `	 * non-OK rc also stops the format loop. */` |
|     91 | 4037 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|     91 | 4038 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|     91 | 4039 | `	return *pRc;` |
|      1 | 4040 |  |
|      - | 4041 | `/*` |
|      - | 4042 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4043 | ` *  Return a formatted string.` |
|      - | 4044 | ` * Parameters` |
|      - | 4045 | ` *  $format` |
|      - | 4046 | ` *    The format string (see block comment above)` |
|      - | 4047 | ` * Return` |
|      - | 4048 | ` *  A string produced according to the formatting string format.` |
|      - | 4049 | ` */` |
|     62 | 4050 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4051 |  |
|      - | 4052 | `	const char *zFormat;` |
|     63 | 4053 | `	sxi32 rc = SXRET_OK;` |
|      - | 4054 | `	int nLen;` |
|     63 | 4055 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4056 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4057 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4058 | `		return PH7_OK;` |
|      - | 4059 | `	}` |
|      - | 4060 | `	/* Extract the string format */` |
|     61 | 4061 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 4062 | `	if( nLen < 1 ){` |
|      - | 4063 | `		/* Empty string */` |
|    ! 0 | 4064 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4065 | `		return PH7_OK;` |
|      - | 4066 | `	}` |
|      - | 4067 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     61 | 4068 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     61 | 4069 | `	if( rc != SXRET_OK ){` |
|      - | 4070 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4071 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4072 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4073 | `	}` |
|     61 | 4074 | `	return PH7_OK;` |
|     32 | 4075 |  |
|      - | 4076 | `/*` |
|      - | 4077 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4078 | ` */` |
|    130 | 4079 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4080 |  |
|    131 | 4081 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4082 | `	/* Call the VM output consumer directly */` |
|    131 | 4083 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4084 | `	/* Increment counter */` |
|    131 | 4085 | `	*pCounter += nLen;` |
|    131 | 4086 | `	return PH7_OK;` |
|      1 | 4087 |  |
|      - | 4088 | `/*` |
|      - | 4089 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4090 | ` *  Output a formatted string.` |
|      - | 4091 | ` * Parameters` |
|      - | 4092 | ` *  $format` |
|      - | 4093 | ` *   See sprintf() for a description of format.` |
|      - | 4094 | ` * Return` |
|      - | 4095 | ` *  The length of the outputted string.` |
|      - | 4096 | ` */` |
|     52 | 4097 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4098 |  |
|     53 | 4099 | `	ph7_int64 nCounter = 0;` |
|      - | 4100 | `	const char *zFormat;` |
|      - | 4101 | `	int nLen;` |
|     53 | 4102 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4103 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4104 | `		ph7_result_int(pCtx,0);` |
|      3 | 4105 | `		return PH7_OK;` |
|      - | 4106 | `	}` |
|      - | 4107 | `	/* Extract the string format */` |
|     51 | 4108 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     51 | 4109 | `	if( nLen < 1 ){` |
|      - | 4110 | `		/* Empty string */` |
|    ! 0 | 4111 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4112 | `		return PH7_OK;` |
|      - | 4113 | `	}` |
|      - | 4114 | `	/* Format the string */` |
|     51 | 4115 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4116 | `	/* Return the length of the outputted string */` |
|     51 | 4117 | `	ph7_result_int64(pCtx,nCounter);` |
|     51 | 4118 | `	return PH7_OK;` |
|     27 | 4119 |  |
|      - | 4120 | `/*` |
|      - | 4121 | ` * int vprintf(string $format,array $args)` |
|      - | 4122 | ` *  Output a formatted string.` |
|      - | 4123 | ` * Parameters` |
|      - | 4124 | ` *  $format` |
|      - | 4125 | ` *   See sprintf() for a description of format.` |
|      - | 4126 | ` * Return` |
|      - | 4127 | ` *  The length of the outputted string.` |
|      - | 4128 | ` */` |
|      2 | 4129 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4130 |  |
|      3 | 4131 | `	ph7_int64 nCounter = 0;` |
|      - | 4132 | `	const char *zFormat;` |
|      - | 4133 | `	ph7_hashmap *pMap;` |
|      - | 4134 | `	SySet sArg;` |
|      - | 4135 | `	int nLen,n;` |
|      3 | 4136 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4137 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4138 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4139 | `		return PH7_OK;` |
|      - | 4140 | `	}` |
|      - | 4141 | `	/* Extract the string format */` |
|      3 | 4142 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4143 | `	if( nLen < 1 ){` |
|      - | 4144 | `		/* Empty string */` |
|    ! 0 | 4145 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4146 | `		return PH7_OK;` |
|      - | 4147 | `	}` |
|      - | 4148 | `	/* Point to the hashmap */` |
|      3 | 4149 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4150 | `	/* Extract arguments from the hashmap */` |
|      3 | 4151 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4152 | `	/* Format the string */` |
|      3 | 4153 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4154 | `	/* Return the length of the outputted string */` |
|      3 | 4155 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4156 | `	/* Release the container */` |
|      3 | 4157 | `	SySetRelease(&sArg);` |
|      3 | 4158 | `	return PH7_OK;` |
|      2 | 4159 |  |
|      - | 4160 | `/*` |
|      - | 4161 | ` * int vsprintf(string $format,array $args)` |
|      - | 4162 | ` *  Output a formatted string.` |
|      - | 4163 | ` * Parameters` |
|      - | 4164 | ` *  $format` |
|      - | 4165 | ` *   See sprintf() for a description of format.` |
|      - | 4166 | ` * Return` |
|      - | 4167 | ` *  A string produced according to the formatting string format.` |
|      - | 4168 | ` */` |
|     10 | 4169 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4170 |  |
|      - | 4171 | `	const char *zFormat;` |
|      - | 4172 | `	ph7_hashmap *pMap;` |
|      - | 4173 | `	SySet sArg;` |
|     11 | 4174 | `	sxi32 rc = SXRET_OK;` |
|      - | 4175 | `	int nLen,n;` |
|     11 | 4176 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4177 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4178 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4179 | `		return PH7_OK;` |
|      - | 4180 | `	}` |
|      - | 4181 | `	/* Extract the string format */` |
|      7 | 4182 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4183 | `	if( nLen < 1 ){` |
|      - | 4184 | `		/* Empty string */` |
|    ! 0 | 4185 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4186 | `		return PH7_OK;` |
|      - | 4187 | `	}` |
|      - | 4188 | `	/* Point to hashmap */` |
|      7 | 4189 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4190 | `	/* Extract arguments from the hashmap */` |
|      7 | 4191 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4192 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      7 | 4193 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4194 | `	/* Release the container */` |
|      7 | 4195 | `	SySetRelease(&sArg);` |
|      7 | 4196 | `	if( rc != SXRET_OK ){` |
|      - | 4197 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4198 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4199 | `	}` |
|      7 | 4200 | `	return PH7_OK;` |
|      6 | 4201 |  |
|      - | 4202 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4203 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4204 | `/*` |
|      - | 4205 | ` * Symisc eXtension.` |
|      - | 4206 | ` * string size_format(int64 $size)` |
|      - | 4207 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4208 | ` *  Example:` |
|      - | 4209 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4210 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4211 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4212 | ` * Parameter` |
|      - | 4213 | ` *  $size` |
|      - | 4214 | ` *    Entity size in bytes.` |
|      - | 4215 | ` * Return` |
|      - | 4216 | ` *   Formatted string representation of the given size.` |
|      - | 4217 | ` */` |
|     24 | 4218 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4219 |  |
|      - | 4220 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4221 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4222 | `	sxi32 nRest,i_32;` |
|      - | 4223 | `	ph7_int64 iSize;` |
|     25 | 4224 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4225 |  |
|     25 | 4226 | `	if( nArg < 1 ){` |
|      - | 4227 | `		/* Missing argument,return the empty string */` |
|      3 | 4228 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4229 | `		return PH7_OK;` |
|      - | 4230 | `	}` |
|      - | 4231 | `	/* Extract the given size */` |
|     23 | 4232 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4233 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4234 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4235 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4236 | `		return PH7_OK;` |
|      - | 4237 | `	}` |
|     19 | 4238 | `	for(;;){` |
|     39 | 4239 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4240 | `		iSize >>= 10;` |
|     39 | 4241 | `		c++;` |
|     39 | 4242 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4243 | `			break;` |
|      - | 4244 | `		}` |
|      1 | 4245 | `	}` |
|     19 | 4246 | `	nRest /= 100;` |
|     19 | 4247 | `	if( nRest > 9 ){` |
|    ! 0 | 4248 | `		nRest = 9;` |
|    ! 0 | 4249 | `	}` |
|     19 | 4250 | `	if( iSize > 999 ){` |
|    ! 0 | 4251 | `		c++;` |
|    ! 0 | 4252 | `		nRest = 9;` |
|    ! 0 | 4253 | `		iSize = 0;` |
|    ! 0 | 4254 | `	}` |
|     19 | 4255 | `	i_32 = (sxi32)iSize;` |
|      - | 4256 | `	/* Format */` |
|     19 | 4257 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4258 | `	return PH7_OK;` |
|     13 | 4259 |  |
|      - | 4260 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4261 | `/*` |
|      - | 4262 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4263 | ` *   Calculate the md5 hash of a string.` |
|      - | 4264 | ` * Parameter` |
|      - | 4265 | ` *  $str` |
|      - | 4266 | ` *   Input string` |
|      - | 4267 | ` * $raw_output` |
|      - | 4268 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4269 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4270 | ` * Return` |
|      - | 4271 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4272 | ` */` |
|     14 | 4273 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4274 |  |
|      - | 4275 | `	unsigned char zDigest[16];` |
|     15 | 4276 | `	int raw_output = FALSE;` |
|      - | 4277 | `	const void *pIn;` |
|      - | 4278 | `	int nLen;` |
|     15 | 4279 | `	if( nArg < 1 ){` |
|      - | 4280 | `		/* Missing arguments,return the empty string */` |
|      3 | 4281 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4282 | `		return PH7_OK;` |
|      - | 4283 | `	}` |
|      - | 4284 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4285 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4286 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4287 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4288 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4289 | `	}` |
|      - | 4290 | `	/* Compute the MD5 digest */` |
|     13 | 4291 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4292 | `	if( raw_output ){` |
|      - | 4293 | `		/* Output raw digest */` |
|      5 | 4294 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4295 | `	}else{` |
|      - | 4296 | `		/* Perform a binary to hex conversion */` |
|      9 | 4297 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4298 | `	}` |
|     13 | 4299 | `	return PH7_OK;` |
|      8 | 4300 |  |
|      - | 4301 | `/*` |
|      - | 4302 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4303 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4304 | ` * Parameter` |
|      - | 4305 | ` *  $str` |
|      - | 4306 | ` *   Input string` |
|      - | 4307 | ` * $raw_output` |
|      - | 4308 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4309 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4310 | ` * Return` |
|      - | 4311 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4312 | ` */` |
|     12 | 4313 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4314 |  |
|      - | 4315 | `	unsigned char zDigest[20];` |
|     13 | 4316 | `	int raw_output = FALSE;` |
|      - | 4317 | `	const void *pIn;` |
|      - | 4318 | `	int nLen;` |
|     13 | 4319 | `	if( nArg < 1 ){` |
|      - | 4320 | `		/* Missing arguments,return the empty string */` |
|      3 | 4321 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4322 | `		return PH7_OK;` |
|      - | 4323 | `	}` |
|      - | 4324 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4325 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4326 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4327 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4328 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4329 | `	}` |
|      - | 4330 | `	/* Compute the SHA1 digest */` |
|     11 | 4331 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4332 | `	if( raw_output ){` |
|      - | 4333 | `		/* Output raw digest */` |
|      5 | 4334 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4335 | `	}else{` |
|      - | 4336 | `		/* Perform a binary to hex conversion */` |
|      7 | 4337 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4338 | `	}` |
|     11 | 4339 | `	return PH7_OK;` |
|      7 | 4340 |  |
|      - | 4341 | `/*` |
|      - | 4342 | ` * int64 crc32(string $str)` |
|      - | 4343 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4344 | ` * Parameter` |
|      - | 4345 | ` *  $str` |
|      - | 4346 | ` *   Input string` |
|      - | 4347 | ` * Return` |
|      - | 4348 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4349 | ` */` |
|      4 | 4350 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4351 |  |
|      - | 4352 | `	const void *pIn;` |
|      - | 4353 | `	sxu32 nCRC;` |
|      - | 4354 | `	int nLen;` |
|      5 | 4355 | `	if( nArg < 1 ){` |
|      - | 4356 | `		/* Missing arguments,return 0 */` |
|      3 | 4357 | `		ph7_result_int(pCtx,0);` |
|      3 | 4358 | `		return PH7_OK;` |
|      - | 4359 | `	}` |
|      - | 4360 | `	/* Extract the input string */` |
|      3 | 4361 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4362 | `	if( nLen < 1 ){` |
|      - | 4363 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4364 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4365 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4366 | `		return PH7_OK;` |
|      - | 4367 | `	}` |
|      - | 4368 | `	/* Calculate the sum */` |
|      3 | 4369 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4370 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4371 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4372 | `	return PH7_OK;` |
|      3 | 4373 |  |
|      - | 4374 | `/*` |
|      - | 4375 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4376 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4377 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4378 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4379 | ` */` |
|     11 | 4380 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4381 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4382 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4383 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4384 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4385 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4386 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4387 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4388 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4389 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4390 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4391 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4392 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4393 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4394 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4395 | `struct HashAlgo {` |
|      - | 4396 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4397 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4398 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4399 | `	void (*xInit)(HashCtx *);` |
|      - | 4400 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4401 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4402 | `};` |
|      - | 4403 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4404 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4405 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4406 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4407 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4408 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4409 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4410 | `};` |
|      - | 4411 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4412 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4413 | `	sxu32 i;` |
|    279 | 4414 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4415 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4416 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4417 | `			return &aHashAlgo[i];` |
|      - | 4418 | `		}` |
|    106 | 4419 | `	}` |
|      6 | 4420 | `	return 0;` |
|     38 | 4421 |  |
|      - | 4422 | `/*` |
|      - | 4423 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4424 | ` *   Generate a hash value (message digest).` |
|      - | 4425 | ` */` |
|     54 | 4426 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4427 |  |
|      - | 4428 | `	const HashAlgo *pAlgo;` |
|      - | 4429 | `	const char *zAlgo,*zData;` |
|     56 | 4430 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4431 | `	HashCtx sCtx;` |
|      - | 4432 | `	unsigned char zDigest[64];` |
|     56 | 4433 | `	if( nArg < 2 ){` |
|    ! 0 | 4434 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4435 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4436 | `	}` |
|     56 | 4437 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4438 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4439 | `	if( pAlgo == 0 ){` |
|      3 | 4440 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4441 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4442 | `	}` |
|     53 | 4443 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4444 | `	if( nArg > 2 ){` |
|      9 | 4445 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4446 | `	}` |
|     53 | 4447 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4448 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4449 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4450 | `	if( raw_output ){` |
|      9 | 4451 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4452 | `	}else{` |
|     45 | 4453 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4454 | `	}` |
|     53 | 4455 | `	return PH7_OK;` |
|     29 | 4456 |  |
|      - | 4457 | `/*` |
|      - | 4458 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4459 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4460 | ` */` |
|     16 | 4461 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4462 |  |
|      - | 4463 | `	const HashAlgo *pAlgo;` |
|      - | 4464 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4465 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4466 | `	HashCtx sCtx;` |
|      - | 4467 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4468 | `	int i,nBlock,nDigest;` |
|     18 | 4469 | `	if( nArg < 3 ){` |
|    ! 0 | 4470 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4471 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4472 | `	}` |
|     18 | 4473 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4474 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4475 | `	if( pAlgo == 0 ){` |
|      3 | 4476 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4477 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4478 | `	}` |
|     15 | 4479 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4480 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4481 | `	if( nArg > 3 ){` |
|      3 | 4482 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4483 | `	}` |
|     15 | 4484 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4485 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4486 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4487 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4488 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4489 | `	if( nKeyLen > nBlock ){` |
|      3 | 4490 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4491 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4492 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4493 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4494 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4495 | `	}` |
|   1039 | 4496 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4497 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4498 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4499 | `	}` |
|      - | 4500 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4501 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4502 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4503 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4504 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4505 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4506 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4507 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4508 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4509 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4510 | `	if( raw_output ){` |
|      3 | 4511 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4512 | `	}else{` |
|     13 | 4513 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4514 | `	}` |
|     15 | 4515 | `	return PH7_OK;` |
|     10 | 4516 |  |
|      - | 4517 | `/*` |
|      - | 4518 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4519 | ` *   Timing-attack-safe string comparison.` |
|      - | 4520 | ` */` |
|     14 | 4521 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4522 |  |
|      - | 4523 | `	const char *zKnown,*zUser;` |
|      - | 4524 | `	int nKnown,nUser,i;` |
|     17 | 4525 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4526 | `	if( nArg < 2 ){` |
|    ! 0 | 4527 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4528 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4529 | `	}` |
|     17 | 4530 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4531 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4532 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4533 | `			ph7_type_name(apArg[0]));` |
|      - | 4534 | `	}` |
|     14 | 4535 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4536 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4537 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4538 | `			ph7_type_name(apArg[1]));` |
|      - | 4539 | `	}` |
|     11 | 4540 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4541 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4542 | `	if( nKnown != nUser ){` |
|      5 | 4543 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4544 | `		return PH7_OK;` |
|      - | 4545 | `	}` |
|      - | 4546 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4547 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4548 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4549 | `	}` |
|      7 | 4550 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4551 | `	return PH7_OK;` |
|     10 | 4552 |  |
|      - | 4553 | `/*` |
|      - | 4554 | ` * array hash_algos(void)` |
|      - | 4555 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4556 | ` */` |
|      2 | 4557 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4558 |  |
|      - | 4559 | `	ph7_value *pArray,*pValue;` |
|      - | 4560 | `	sxu32 i;` |
|      1 | 4561 | `	SXUNUSED(nArg);` |
|      1 | 4562 | `	SXUNUSED(apArg);` |
|      3 | 4563 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4564 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4565 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4566 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4567 | `		return PH7_OK;` |
|      - | 4568 | `	}` |
|     15 | 4569 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4570 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4571 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4572 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4573 | `	}` |
|      3 | 4574 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4575 | `	return PH7_OK;` |
|      2 | 4576 |  |
|      - | 4577 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4578 | `/*` |
|      - | 4579 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4580 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4581 | ` */` |
|      - | 4582 | `/*` |
|      - | 4583 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4584 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4585 | ` */` |
|     40 | 4586 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4587 |  |
|      - | 4588 | `	int iCost;` |
|     51 | 4589 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4590 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4591 | `		return FALSE;` |
|      - | 4592 | `	}` |
|     29 | 4593 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4594 | `		return FALSE;` |
|      - | 4595 | `	}` |
|     29 | 4596 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4597 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4598 | `		return FALSE;` |
|      - | 4599 | `	}` |
|     27 | 4600 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4601 | `	return TRUE;` |
|     21 | 4602 |  |
|      - | 4603 | `/*` |
|      - | 4604 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4605 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4606 | ` */` |
|     20 | 4607 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4608 |  |
|     23 | 4609 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4610 | `		return TRUE;` |
|      - | 4611 | `	}` |
|     23 | 4612 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4613 | `		int nAlgo;` |
|     23 | 4614 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4615 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4616 | `	}` |
|    ! 0 | 4617 | `	return FALSE;` |
|     13 | 4618 |  |
|      - | 4619 | `/*` |
|      - | 4620 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4621 | ` *  Create a bcrypt hash of the password.` |
|      - | 4622 | ` */` |
|     16 | 4623 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4624 |  |
|      - | 4625 | `	const char *zPwd;` |
|     19 | 4626 | `	int nPwd,iCost = 12;` |
|      - | 4627 | `	unsigned char aSalt[16];` |
|      - | 4628 | `	char zHash[60];` |
|     19 | 4629 | `	if( nArg < 2 ){` |
|    ! 0 | 4630 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4631 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4632 | `	}` |
|     19 | 4633 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4634 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4635 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4636 | `	}` |
|      - | 4637 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4638 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4639 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4640 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4641 | `	}` |
|     16 | 4642 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4643 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4644 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4645 | `	}` |
|     13 | 4646 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4647 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4648 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4649 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4650 | `	}` |
|     13 | 4651 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4652 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4653 | `		return PH7_OK;` |
|      - | 4654 | `	}` |
|     13 | 4655 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4656 | `	return PH7_OK;` |
|     11 | 4657 |  |
|      - | 4658 | `/*` |
|      - | 4659 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4660 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4661 | ` */` |
|     28 | 4662 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4663 |  |
|      - | 4664 | `	const char *zPwd,*zHash;` |
|      - | 4665 | `	int nPwd,nHash,iCost,i;` |
|      - | 4666 | `	unsigned char aSalt[16];` |
|      - | 4667 | `	char zComputed[60];` |
|     29 | 4668 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4669 | `	if( nArg < 2 ){` |
|    ! 0 | 4670 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4671 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4672 | `	}` |
|     29 | 4673 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4674 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4675 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4676 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4677 | `		return PH7_OK;` |
|      - | 4678 | `	}` |
|      - | 4679 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4680 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4681 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4682 | `		return PH7_OK;` |
|      - | 4683 | `	}` |
|     19 | 4684 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4685 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4686 | `		return PH7_OK;` |
|      - | 4687 | `	}` |
|      - | 4688 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4689 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4690 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4691 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4692 | `	}` |
|     19 | 4693 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4694 | `	return PH7_OK;` |
|     15 | 4695 |  |
|      - | 4696 | `/*` |
|      - | 4697 | ` * array password_get_info(string $hash)` |
|      - | 4698 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4699 | ` */` |
|      6 | 4700 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4701 |  |
|      7 | 4702 | `	const char *zHash = "";` |
|      7 | 4703 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4704 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4705 | `	if( nArg > 0 ){` |
|      7 | 4706 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4707 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4708 | `	}` |
|      7 | 4709 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4710 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4711 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4712 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4713 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4714 | `		return PH7_OK;` |
|      - | 4715 | `	}` |
|      7 | 4716 | `	if( bBcrypt ){` |
|      5 | 4717 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4718 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4719 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4720 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4721 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4722 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4723 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4724 | `	}else{` |
|      3 | 4725 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4726 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4727 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4728 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4729 | `	}` |
|      7 | 4730 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4731 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4732 | `	return PH7_OK;` |
|      4 | 4733 |  |
|      - | 4734 | `/*` |
|      - | 4735 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4736 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4737 | ` */` |
|      6 | 4738 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4739 |  |
|      - | 4740 | `	const char *zHash;` |
|      7 | 4741 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4742 | `	if( nArg < 2 ){` |
|    ! 0 | 4743 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4744 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4745 | `	}` |
|      7 | 4746 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4747 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4748 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4749 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4750 | `		return PH7_OK;` |
|      - | 4751 | `	}` |
|      5 | 4752 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4753 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4754 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4755 | `	}` |
|      5 | 4756 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4757 | `	return PH7_OK;` |
|      4 | 4758 |  |
|      - | 4759 | `/*` |
|      - | 4760 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4761 | ` *` |
|      - | 4762 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4763 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4764 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4765 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4766 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4767 | ` */` |
|      - | 4768 | `#define FV_VALIDATE_INT     257` |
|      - | 4769 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4770 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4771 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4772 | `#define FV_VALIDATE_URL     273` |
|      - | 4773 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4774 | `#define FV_VALIDATE_IP      275` |
|      - | 4775 | `#define FV_VALIDATE_MAC     276` |
|      - | 4776 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4777 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4778 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4779 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4780 | `#define FV_SANITIZE_URL     518` |
|      - | 4781 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4782 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4783 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4784 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4785 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4786 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4787 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4788 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4789 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4790 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4791 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4792 |  |
|      - | 4793 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4794 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    125 | 4795 | `static void FvTrim(const char **pz,int *pn){` |
|    125 | 4796 | `	const char *z = *pz;` |
|    125 | 4797 | `	int n = *pn;` |
|    129 | 4798 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    133 | 4799 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    125 | 4800 | `	*pz = z; *pn = n;` |
|    125 | 4801 |  |
|      - | 4802 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4803 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4804 | `	int neg = 0, i;` |
|     57 | 4805 | `	sxu64 u = 0;` |
|     57 | 4806 | `	FvTrim(&z,&n);` |
|     57 | 4807 | `	if( n==0 ){ return 0; }` |
|     51 | 4808 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4809 | `	if( n==0 ){ return 0; }` |
|     49 | 4810 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4811 | `		z += 2; n -= 2;` |
|      3 | 4812 | `		if( n==0 ){ return 0; }` |
|      7 | 4813 | `		for( i=0; i<n; i++ ){` |
|      5 | 4814 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4815 | `			if( h<0 ){ return 0; }` |
|      5 | 4816 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4817 | `			u = u*16 + (sxu64)h;` |
|      3 | 4818 | `		}` |
|     48 | 4819 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4820 | `		for( i=0; i<n; i++ ){` |
|      7 | 4821 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4822 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4823 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4824 | `		}` |
|      2 | 4825 | `	}else{` |
|     45 | 4826 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4827 | `		for( i=0; i<n; i++ ){` |
|    173 | 4828 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4829 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4830 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4831 | `		}` |
|      - | 4832 | `	}` |
|     33 | 4833 | `	if( neg ){` |
|      5 | 4834 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4835 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4836 | `	}else{` |
|     29 | 4837 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4838 | `		*pOut = (ph7_int64)u;` |
|      - | 4839 | `	}` |
|     31 | 4840 | `	return 1;` |
|     29 | 4841 |  |
|      - | 4842 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     41 | 4843 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4844 | `	char zBuf[512];` |
|     41 | 4845 | `	int i, m = 0, seenDigit = 0;` |
|     41 | 4846 | `	const char *zv; int nv; double d = 0; const char *zRest = 0;` |
|     41 | 4847 | `	FvTrim(&z,&n);` |
|      - | 4848 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4849 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     41 | 4850 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     41 | 4851 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4852 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4853 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4854 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4855 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     23 | 4856 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     23 | 4857 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     23 | 4858 | `		intEnd = s;` |
|    155 | 4859 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    133 | 4860 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    133 | 4861 | `			intEnd++;` |
|      1 | 4862 | `		}` |
|     23 | 4863 | `		if( hasComma ){` |
|     23 | 4864 | `			segStart = s; segIdx = 0;` |
|    151 | 4865 | `			for( i=s; i<=intEnd; i++ ){` |
|    139 | 4866 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     45 | 4867 | `					int segLen = i - segStart, k;` |
|     45 | 4868 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     23 | 4869 | `					else if( segLen!=3 ){ return 0; }` |
|    107 | 4870 | `					for( k=segStart; k<i; k++ ){` |
|     73 | 4871 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     73 | 4872 | `						zBuf[m++] = z[k];` |
|     37 | 4873 | `					}` |
|     35 | 4874 | `					segStart = i+1; segIdx++;` |
|     17 | 4875 | `				}` |
|     65 | 4876 | `			}` |
|      7 | 4877 | `		}else{` |
|    ! 0 | 4878 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4879 | `		}` |
|     17 | 4880 | `		for( i=intEnd; i<n; i++ ){` |
|      5 | 4881 | `			if( z[i]==',' ){ return 0; }` |
|      5 | 4882 | `			zBuf[m++] = z[i];` |
|      3 | 4883 | `		}` |
|     13 | 4884 | `		zv = zBuf; nv = m;` |
|      7 | 4885 | `	}else{` |
|     19 | 4886 | `		zv = z; nv = n;` |
|      - | 4887 | `	}` |
|     31 | 4888 | `	i = 0;` |
|     31 | 4889 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    105 | 4890 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     31 | 4891 | `	if( i<nv && zv[i]=='.' ){` |
|     13 | 4892 | `		i++;` |
|     23 | 4893 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|      6 | 4894 | `	}` |
|     31 | 4895 | `	if( !seenDigit ){ return 0; }` |
|     29 | 4896 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|      5 | 4897 | `		i++;` |
|      5 | 4898 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|      5 | 4899 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|      9 | 4900 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|      2 | 4901 | `	}` |
|     29 | 4902 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4903 | `	/* Divergence: PHP rejects magnitudes beyond the double range ("1e400" ->` |
|      - | 4904 | `	 * false), but SyStrToReal (the engine-wide float parser, also behind` |
|      - | 4905 | `	 * floatval/(float)) saturates them to a finite value, so they validate here. */` |
|     25 | 4906 | `	SyStrToReal(zv,(sxu32)nv,(void *)&d,&zRest);` |
|     25 | 4907 | `	*pOut = d;` |
|     25 | 4908 | `	return 1;` |
|     21 | 4909 |  |
|      - | 4910 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4911 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4912 | ` * false, NOT failures. */` |
|     33 | 4913 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4914 | `	FvTrim(&z,&n);` |
|     35 | 4915 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4916 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4917 | `		*pBool = 1; return 1;` |
|      - | 4918 | `	}` |
|     23 | 4919 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4920 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4921 | `		*pBool = 0; return 1;` |
|      - | 4922 | `	}` |
|      9 | 4923 | `	return 0;` |
|     15 | 4924 |  |
|      - | 4925 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4926 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4927 | `	int i = 0, parts = 0;` |
|     77 | 4928 | `	while( i<n ){` |
|     65 | 4929 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4930 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4931 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4932 | `			if( val>255 ){ return 0; }` |
|     79 | 4933 | `			digits++; i++;` |
|      1 | 4934 | `		}` |
|     59 | 4935 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4936 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4937 | `		parts++;` |
|     45 | 4938 | `		if( parts>4 ){ return 0; }` |
|     45 | 4939 | `		if( i<n ){` |
|     33 | 4940 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4941 | `			i++;` |
|     33 | 4942 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4943 | `		}` |
|      1 | 4944 | `	}` |
|     13 | 4945 | `	return parts==4;` |
|     17 | 4946 |  |
|      - | 4947 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4948 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4949 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4950 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4951 | `	if( n==0 ){ return 0; }` |
|    145 | 4952 | `	while( i<=n ){` |
|    133 | 4953 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4954 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4955 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4956 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4957 | `			if( isV4 ){` |
|     11 | 4958 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4959 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4960 | `				groups += 2;` |
|      3 | 4961 | `			}else{` |
|     13 | 4962 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4963 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4964 | `				groups++;` |
|      - | 4965 | `			}` |
|     17 | 4966 | `			segStart = i+1;` |
|      8 | 4967 | `		}` |
|    127 | 4968 | `		i++;` |
|      1 | 4969 | `	}` |
|     13 | 4970 | `	return groups;` |
|     10 | 4971 |  |
|      - | 4972 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4973 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4974 | `	const char *zDbl = 0;` |
|      - | 4975 | `	int i, ga, gb;` |
|    139 | 4976 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4977 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4978 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4979 | `			zDbl = z+i;` |
|      5 | 4980 | `		}` |
|     61 | 4981 | `	}` |
|     17 | 4982 | `	if( zDbl==0 ){` |
|      9 | 4983 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4984 | `	}else{` |
|      9 | 4985 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4986 | `		int lenB = n - lenA - 2;` |
|      9 | 4987 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4988 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4989 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4990 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4991 | `	}` |
|     10 | 4992 |  |
|     25 | 4993 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4994 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4995 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4996 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4997 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4998 | `	return 0;` |
|     13 | 4999 |  |
|      - | 5000 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5001 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5002 | `	char sep;` |
|      - | 5003 | `	int i;` |
|     11 | 5004 | `	if( n!=17 ){ return 0; }` |
|      7 | 5005 | `	sep = z[2];` |
|      7 | 5006 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5007 | `	for( i=0; i<17; i++ ){` |
|    101 | 5008 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5009 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5010 | `	}` |
|      5 | 5011 | `	return 1;` |
|      6 | 5012 |  |
|      - | 5013 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5014 | ` * parts or IP-literal domains). */` |
|     21 | 5015 | `static int FvValidateEmail(const char *z,int n){` |
|     21 | 5016 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5017 | `	const char *zDom;` |
|     21 | 5018 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5019 | `	for( i=0; i<n; i++ ){` |
|    181 | 5020 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5021 | `	}` |
|     21 | 5022 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5023 | `	localLen = at;` |
|     21 | 5024 | `	zDom = z + at + 1;` |
|     21 | 5025 | `	domLen = n - at - 1;` |
|     21 | 5026 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5027 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5028 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5029 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5030 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5031 | `	}` |
|     15 | 5032 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5033 | `	labelStart = 0;` |
|     85 | 5034 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5035 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5036 | `			int ll = i - labelStart;` |
|     25 | 5037 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5038 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5039 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5040 | `			labelStart = i+1;` |
|     12 | 5041 | `		}else{` |
|     51 | 5042 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5043 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5044 | `		}` |
|     37 | 5045 | `	}` |
|     11 | 5046 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5047 | `	return 1;` |
|     11 | 5048 |  |
|      - | 5049 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5050 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5051 | `	int i;` |
|     11 | 5052 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5053 | `	for( i=0; i<n; i++ ){` |
|     75 | 5054 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5055 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5056 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5057 | `	}` |
|      7 | 5058 | `	return 1;` |
|      6 | 5059 |  |
|      - | 5060 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5061 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5062 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5063 | `	SyhttpUri sUri;` |
|     15 | 5064 | `	if( n==0 ){ return 0; }` |
|     15 | 5065 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5066 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5067 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5068 |  |
|      - | 5069 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5070 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5071 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5072 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5073 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5074 | `	int i, runStart = 0;` |
|     37 | 5075 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5076 | `	for( i=0; i<n; i++ ){` |
|     91 | 5077 | `		char c = z[i];` |
|     91 | 5078 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5079 | `		if( !keep && isFloat ){` |
|     38 | 5080 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5081 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5082 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5083 | `		}` |
|     61 | 5084 | `		if( !keep ){` |
|     33 | 5085 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5086 | `			runStart = i+1;` |
|     16 | 5087 | `		}` |
|     31 | 5088 | `	}` |
|      7 | 5089 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5090 |  |
|      - | 5091 | `/* SANITIZE_SPECIAL_CHARS (full=0, numeric entities; also encodes control bytes` |
|      - | 5092 | ` * <32 as &#N;) / FULL_SPECIAL_CHARS (full=1, named entities for <>&"').` |
|      - | 5093 | ` * Divergence on bytes >=128: PHP's FULL filter is UTF-8-aware — it named-entity` |
|      - | 5094 | ` * encodes valid sequences ("\xC3\xA9" -> "&eacute;") and drops invalid ones; we` |
|      - | 5095 | ` * pass every byte >=128 through verbatim (the engine has no UTF-8 entity table,` |
|      - | 5096 | ` * and PH7_builtin_htmlspecialchars behaves the same way). Bytes 0-127 match. */` |
|      7 | 5097 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int full){` |
|      7 | 5098 | `	int i, runStart = 0;` |
|      - | 5099 | `	const char *zEnt;` |
|      7 | 5100 | `	ph7_result_string(pCtx,"",0);` |
|     43 | 5101 | `	for( i=0; i<n; i++ ){` |
|     37 | 5102 | `		unsigned char c = (unsigned char)z[i];` |
|     37 | 5103 | `		switch( c ){` |
|      5 | 5104 | `		case '<':  zEnt = full?"&lt;":"&#60;";   break;` |
|      5 | 5105 | `		case '>':  zEnt = full?"&gt;":"&#62;";   break;` |
|      5 | 5106 | `		case '&':  zEnt = full?"&amp;":"&#38;";  break;` |
|      5 | 5107 | `		case '"':  zEnt = full?"&quot;":"&#34;"; break;` |
|      5 | 5108 | `		case '\'': zEnt = full?"&#039;":"&#39;"; break;` |
|      8 | 5109 | `		default:` |
|     17 | 5110 | `			if( full \|\| c>=32 ){ continue; } /* keep in the current run */` |
|      - | 5111 | `			/* SPECIAL_CHARS encodes a control byte as a numeric entity. */` |
|      5 | 5112 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      5 | 5113 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      5 | 5114 | `			runStart = i+1;` |
|      5 | 5115 | `			continue;` |
|      - | 5116 | `		}` |
|     21 | 5117 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     21 | 5118 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     21 | 5119 | `		runStart = i+1;` |
|     11 | 5120 | `	}` |
|      7 | 5121 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5122 |  |
|     25 | 5123 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5124 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5125 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5126 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5127 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5128 |  |
|     23 | 5129 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5130 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5131 |  |
|      - | 5132 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5133 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5134 | `	int i, runStart = 0;` |
|      5 | 5135 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5136 | `	for( i=0; i<n; i++ ){` |
|     47 | 5137 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5138 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5139 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5140 | `			runStart = i+1;` |
|      5 | 5141 | `		}` |
|     24 | 5142 | `	}` |
|      5 | 5143 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5144 |  |
|      - | 5145 | `/*` |
|      - | 5146 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5147 | ` *  Validate or sanitize a value. The scalar input is coerced to a string and the` |
|      - | 5148 | ` *  selected filter applied; on validation failure the 'default' option (if any)` |
|      - | 5149 | ` *  is returned, else null when FILTER_NULL_ON_FAILURE is set, else false.` |
|      - | 5150 | ` */` |
|    230 | 5151 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5152 |  |
|    232 | 5153 | `	int iFilter = FV_DEFAULT, iFlags = 0, bNull;` |
|    232 | 5154 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|      - | 5155 | `	const char *zVal; int nVal;` |
|    232 | 5156 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    232 | 5157 | `	if( nArg>1 ){ iFilter = ph7_value_to_int(apArg[1]); }` |
|    232 | 5158 | `	if( nArg>2 ){` |
|     53 | 5159 | `		if( ph7_value_is_array(apArg[2]) ){` |
|     13 | 5160 | `			ph7_value *pF = ph7_array_fetch(apArg[2],"flags",(int)sizeof("flags")-1);` |
|     13 | 5161 | `			if( pF ){ iFlags = ph7_value_to_int(pF); }` |
|     13 | 5162 | `			pOpts = ph7_array_fetch(apArg[2],"options",(int)sizeof("options")-1);` |
|     13 | 5163 | `			if( pOpts && !ph7_value_is_array(pOpts) ){ pOpts = 0; }` |
|     13 | 5164 | `			if( pOpts ){ pDefault = ph7_array_fetch(pOpts,"default",(int)sizeof("default")-1); }` |
|      7 | 5165 | `		}else{` |
|     41 | 5166 | `			iFlags = ph7_value_to_int(apArg[2]);` |
|      - | 5167 | `		}` |
|     26 | 5168 | `	}` |
|    232 | 5169 | `	bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5170 | `	/* An array/object input fails every scalar filter. */` |
|    232 | 5171 | `	if( ph7_value_is_array(apArg[0]) ){ goto fail; }` |
|    230 | 5172 | `	zVal = ph7_value_to_string(apArg[0],&nVal);` |
|    230 | 5173 | `	switch( iFilter ){` |
|     28 | 5174 | `	case FV_VALIDATE_INT: {` |
|      - | 5175 | `		ph7_int64 v;` |
|     58 | 5176 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5177 | `		if( pOpts ){` |
|      7 | 5178 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5179 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5180 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5181 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5182 | `		}` |
|     29 | 5183 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5184 | `		return PH7_OK;` |
|      - | 5185 | `	}` |
|     20 | 5186 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5187 | `		double d;` |
|     41 | 5188 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     25 | 5189 | `		ph7_result_double(pCtx,d);` |
|     25 | 5190 | `		return PH7_OK;` |
|      - | 5191 | `	}` |
|     14 | 5192 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5193 | `		int b;` |
|     29 | 5194 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5195 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5196 | `		return PH7_OK;` |
|      - | 5197 | `	}` |
|     25 | 5198 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5199 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     21 | 5200 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5201 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5202 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5203 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5204 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5205 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5206 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5207 | `		if( pRe==0 ){` |
|      3 | 5208 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5209 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5210 | `		}` |
|      5 | 5211 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5212 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5213 | `		goto pass;` |
|      - | 5214 | `#else` |
|      - | 5215 | `		goto fail;` |
|      - | 5216 | `#endif` |
|      - | 5217 | `	}` |
|      3 | 5218 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5219 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|      5 | 5220 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5221 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeSpecial(pCtx,zVal,nVal,1); return PH7_OK;` |
|      3 | 5222 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5223 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|      5 | 5224 | `	case FV_DEFAULT: goto pass; /* FILTER_UNSAFE_RAW: pass through unchanged */` |
|    ! 0 | 5225 | `	default:` |
|    ! 0 | 5226 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5227 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5228 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5229 | `	}` |
|     48 | 5230 | `fail:` |
|     97 | 5231 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|     95 | 5232 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|     91 | 5233 | `	else { ph7_result_bool(pCtx,0); }` |
|     97 | 5234 | `	return PH7_OK;` |
|     22 | 5235 | `pass: /* validation passed: return the (string) input unchanged */` |
|     45 | 5236 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     45 | 5237 | `	return PH7_OK;` |
|    117 | 5238 |  |
|      - | 5239 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5240 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5241 | `/*` |
|      - | 5242 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5243 |  |
|      - | 5244 | ` */` |
|      4 | 5245 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5246 | `	const char *zInput, /* Raw input */` |
|      - | 5247 | `	int nByte,  /* Input length */` |
|      - | 5248 | `	int delim,  /* Delimiter */` |
|      - | 5249 | `	int encl,   /* Enclosure */` |
|      - | 5250 | `	int escape,  /* Escape character */` |
|      - | 5251 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5252 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5253 | `	)` |
|      1 | 5254 |  |
|      5 | 5255 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5256 | `	const char *zIn = zInput;` |
|      - | 5257 | `	const char *zPtr;` |
|      - | 5258 | `	int isEnc;` |
|      - | 5259 | `	/* Start processing */` |
|      8 | 5260 | `	for(;;){` |
|     17 | 5261 | `		if( zIn >= zEnd ){` |
|      - | 5262 | `			/* No more input to process */` |
|      5 | 5263 | `			break;` |
|      - | 5264 | `		}` |
|     13 | 5265 | `		isEnc = 0;` |
|     13 | 5266 | `		zPtr = zIn;` |
|      - | 5267 | `		/* Find the first delimiter */` |
|     27 | 5268 | `		while( zIn < zEnd ){` |
|     23 | 5269 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5270 | `				/* Delimiter found,break imediately */` |
|      5 | 5271 | `				break;` |
|     15 | 5272 | `			}else if( zIn[0] == encl ){` |
|      - | 5273 | `				/* Inside enclosure? */` |
|    ! 0 | 5274 | `				isEnc = !isEnc;` |
|     15 | 5275 | `			}else if( zIn[0] == escape ){` |
|      - | 5276 | `				/* Escape sequence */` |
|    ! 0 | 5277 | `				zIn++;` |
|    ! 0 | 5278 | `			}` |
|      - | 5279 | `			/* Advance the cursor */` |
|     15 | 5280 | `			zIn++;` |
|      1 | 5281 | `		}` |
|     13 | 5282 | `		if( zIn > zPtr ){` |
|     13 | 5283 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5284 | `			sxi32 rc;` |
|      - | 5285 | `			/* Invoke the supllied callback */` |
|     13 | 5286 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5287 | `				zPtr++;` |
|    ! 0 | 5288 | `				nByteChunk-=2;` |
|    ! 0 | 5289 | `			}` |
|     13 | 5290 | `			if( nByteChunk > 0 ){` |
|     13 | 5291 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5292 | `				if( rc == SXERR_ABORT ){` |
|      - | 5293 | `					/* User callback request an operation abort */` |
|    ! 0 | 5294 | `					break;` |
|      - | 5295 | `				}` |
|      6 | 5296 | `			}` |
|      6 | 5297 | `		}` |
|      - | 5298 | `		/* Ignore trailing delimiter */` |
|     21 | 5299 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5300 | `			zIn++;` |
|      1 | 5301 | `		}` |
|      1 | 5302 | `	}` |
|      5 | 5303 | `	return SXRET_OK;` |
|      1 | 5304 |  |
|      - | 5305 | `/*` |
|      - | 5306 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5307 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5308 | ` * argument to this callback.` |
|      - | 5309 | ` */` |
|     12 | 5310 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5311 |  |
|     13 | 5312 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5313 | `	ph7_value sEntry;` |
|      - | 5314 | `	SyString sToken;` |
|      - | 5315 | `	/* Insert the token in the given array */` |
|     13 | 5316 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5317 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5318 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5319 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5320 | `		return SXRET_OK;` |
|      - | 5321 | `	}` |
|     13 | 5322 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5323 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5324 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5325 | `	return SXRET_OK;` |
|      7 | 5326 |  |
|      - | 5327 | `/*` |
|      - | 5328 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5329 | ` *  Parse a CSV string into an array.` |
|      - | 5330 | ` * Parameters` |
|      - | 5331 | ` *  $input` |
|      - | 5332 | ` *   The string to parse.` |
|      - | 5333 | ` *  $delimiter` |
|      - | 5334 | ` *   Set the field delimiter (one character only).` |
|      - | 5335 | ` *  $enclosure` |
|      - | 5336 | ` *   Set the field enclosure character (one character only).` |
|      - | 5337 | ` *  $escape` |
|      - | 5338 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5339 | ` * Return` |
|      - | 5340 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5341 | ` */` |
|      4 | 5342 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5343 |  |
|      - | 5344 | `	const char *zInput,*zPtr;` |
|      - | 5345 | `	ph7_value *pArray;` |
|      5 | 5346 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5347 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5348 | `	int escape = '\\';  /* Escape character */` |
|      - | 5349 | `	int nLen;` |
|      5 | 5350 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5351 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5352 | `		ph7_result_null(pCtx);` |
|      3 | 5353 | `		return PH7_OK;` |
|      - | 5354 | `	}` |
|      - | 5355 | `	/* Extract the raw input */` |
|      3 | 5356 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5357 | `	if( nArg > 1 ){` |
|      - | 5358 | `		int i;` |
|      3 | 5359 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5360 | `			/* Extract the delimiter */` |
|      3 | 5361 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5362 | `			if( i > 0 ){` |
|      3 | 5363 | `				delim = zPtr[0];` |
|      1 | 5364 | `			}` |
|      1 | 5365 | `		}` |
|      3 | 5366 | `		if( nArg > 2 ){` |
|      3 | 5367 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5368 | `				/* Extract the enclosure */` |
|      3 | 5369 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5370 | `				if( i > 0 ){` |
|      3 | 5371 | `					encl = zPtr[0];` |
|      1 | 5372 | `				}` |
|      1 | 5373 | `			}` |
|      3 | 5374 | `			if( nArg > 3 ){` |
|      3 | 5375 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5376 | `					/* Extract the escape character */` |
|      3 | 5377 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5378 | `					if( i > 0 ){` |
|      3 | 5379 | `						escape = zPtr[0];` |
|      1 | 5380 | `					}` |
|      1 | 5381 | `				}` |
|      1 | 5382 | `			}` |
|      1 | 5383 | `		}` |
|      1 | 5384 | `	}` |
|      - | 5385 | `	/* Create our array */` |
|      3 | 5386 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5387 | `	if( pArray == 0 ){` |
|      - | 5388 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5389 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5390 | `	}` |
|      - | 5391 | `	/* Parse the raw input */` |
|      3 | 5392 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5393 | `	/* Return the freshly created array */` |
|      3 | 5394 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5395 | `	return PH7_OK;` |
|      3 | 5396 |  |
|      - | 5397 | `/*` |
|      - | 5398 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5399 | ` * container.` |
|      - | 5400 | ` * Refer to [strip_tags()].` |
|      - | 5401 | ` */` |
|     10 | 5402 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5403 |  |
|     11 | 5404 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5405 | `	const char *zPtr;` |
|      - | 5406 | `	SyString sEntry;` |
|      - | 5407 | `	/* Strip tags */` |
|     10 | 5408 | `	for(;;){` |
|     45 | 5409 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5410 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5411 | `				zTag++;` |
|      1 | 5412 | `		}` |
|     21 | 5413 | `		if( zTag >= zEnd ){` |
|     11 | 5414 | `			break;` |
|      - | 5415 | `		}` |
|     11 | 5416 | `		zPtr = zTag;` |
|      - | 5417 | `		/* Delimit the tag */` |
|     25 | 5418 | `		while(zTag < zEnd ){` |
|     25 | 5419 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5420 | `				/* UTF-8 stream */` |
|      3 | 5421 | `				zTag++;` |
|      5 | 5422 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5423 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5424 | `				break;` |
|    ! 0 | 5425 | `			}else{` |
|     13 | 5426 | `				zTag++;` |
|      - | 5427 | `			}` |
|      1 | 5428 | `		}` |
|     11 | 5429 | `		if( zTag > zPtr ){` |
|      - | 5430 | `			/* Perform the insertion */` |
|     11 | 5431 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5432 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5433 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5434 | `		}` |
|      - | 5435 | `		/* Jump the trailing '>' */` |
|     11 | 5436 | `		zTag++;` |
|      1 | 5437 | `	}` |
|     11 | 5438 | `	return SXRET_OK;` |
|      1 | 5439 |  |
|      - | 5440 | `/*` |
|      - | 5441 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5442 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5443 | ` * Refer to [strip_tags()].` |
|      - | 5444 | ` */` |
|     36 | 5445 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5446 |  |
|     37 | 5447 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5448 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5449 | `		SyString sTag;` |
|     85 | 5450 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5451 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5452 | `			zTag++;` |
|      1 | 5453 | `		}` |
|      - | 5454 | `		/* Delimit the tag */` |
|     25 | 5455 | `		zCur = zTag;` |
|     77 | 5456 | `		while(zTag < zEnd ){` |
|     77 | 5457 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5458 | `				/* UTF-8 stream */` |
|      5 | 5459 | `				zTag++;` |
|      9 | 5460 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5461 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5462 | `				break;` |
|    ! 0 | 5463 | `			}else{` |
|     49 | 5464 | `				zTag++;` |
|      - | 5465 | `			}` |
|      1 | 5466 | `		}` |
|     25 | 5467 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5468 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5469 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5470 | `		if( sTag.nByte > 0 ){` |
|      - | 5471 | `			SyString *aEntry,*pEntry;` |
|      - | 5472 | `			sxi32 rc;` |
|      - | 5473 | `			sxu32 n;` |
|      - | 5474 | `			/* Perform the lookup */` |
|     25 | 5475 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5476 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5477 | `				pEntry = &aEntry[n];` |
|      - | 5478 | `				/* Do the comparison */` |
|     25 | 5479 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5480 | `				if( !rc ){` |
|     21 | 5481 | `					return SXRET_OK;` |
|      - | 5482 | `				}` |
|      3 | 5483 | `			}` |
|      2 | 5484 | `		}` |
|      2 | 5485 | `	}` |
|      - | 5486 | `	/* No such tag */` |
|     17 | 5487 | `	return SXERR_NOTFOUND;` |
|     19 | 5488 |  |
|      - | 5489 | `/*` |
|      - | 5490 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5491 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5492 | ` * Refer to [strip_tags()].` |
|      - | 5493 | ` */` |
|     16 | 5494 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5495 |  |
|     17 | 5496 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5497 | `	const char *zPtr,*zTag;` |
|      - | 5498 | `	SySet sSet;` |
|      - | 5499 | `	/* initialize the set of allowed tags */` |
|     17 | 5500 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5501 | `	if( nTaglen > 0 ){` |
|      - | 5502 | `		/* Set of allowed tags */` |
|     11 | 5503 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5504 | `	}` |
|      - | 5505 | `	/* Set the empty string */` |
|     17 | 5506 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5507 | `	/* Start processing */` |
|     26 | 5508 | `	for(;;){` |
|     53 | 5509 | `		if(zIn >= zEnd){` |
|      - | 5510 | `			/* No more input to process */` |
|     15 | 5511 | `			break;` |
|      - | 5512 | `		}` |
|     39 | 5513 | `		zPtr = zIn;` |
|      - | 5514 | `		/* Find a tag */` |
|    133 | 5515 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5516 | `			zIn++;` |
|      1 | 5517 | `		}` |
|     39 | 5518 | `		if( zIn > zPtr ){` |
|      - | 5519 | `			/* Consume raw input */` |
|     21 | 5520 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5521 | `		}` |
|      - | 5522 | `		/* Ignore trailing null bytes */` |
|     39 | 5523 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5524 | `			zIn++;` |
|    ! 0 | 5525 | `		}` |
|     39 | 5526 | `		if(zIn >= zEnd){` |
|      - | 5527 | `			/* No more input to process */` |
|      3 | 5528 | `			break;` |
|      - | 5529 | `		}` |
|     37 | 5530 | `		if( zIn[0] == '<' ){` |
|      - | 5531 | `			sxi32 rc;` |
|     37 | 5532 | `			zTag = zIn++;` |
|      - | 5533 | `			/* Delimit the tag */` |
|    127 | 5534 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5535 | `				zIn++;` |
|      1 | 5536 | `			}` |
|     37 | 5537 | `			if( zIn < zEnd ){` |
|     37 | 5538 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5539 | `			}` |
|      - | 5540 | `			/* Query the set */` |
|     37 | 5541 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5542 | `			if( rc == SXRET_OK ){` |
|      - | 5543 | `				/* Keep the tag */` |
|     21 | 5544 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5545 | `			}` |
|     18 | 5546 | `		}` |
|      1 | 5547 | `	}` |
|      - | 5548 | `	/* Cleanup */` |
|     17 | 5549 | `	SySetRelease(&sSet);` |
|     17 | 5550 | `	return SXRET_OK;` |
|      1 | 5551 |  |
|      - | 5552 | `/*` |
|      - | 5553 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5554 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5555 | ` * Parameters` |
|      - | 5556 | ` *  $str` |
|      - | 5557 | ` *  The input string.` |
|      - | 5558 | ` * $allowable_tags` |
|      - | 5559 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5560 | ` * Return` |
|      - | 5561 | ` *  Returns the stripped string.` |
|      - | 5562 | ` */` |
|     16 | 5563 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5564 |  |
|     17 | 5565 | `	const char *zTaglist = 0;` |
|      - | 5566 | `	const char *zString;` |
|     17 | 5567 | `	int nTaglen = 0;` |
|      - | 5568 | `	int nLen;` |
|     17 | 5569 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5570 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5571 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5572 | `		return PH7_OK;` |
|      - | 5573 | `	}` |
|      - | 5574 | `	/* Point to the raw string */` |
|     15 | 5575 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5576 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5577 | `		/* Allowed tag */` |
|     11 | 5578 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5579 | `	}` |
|      - | 5580 | `	/* Process input */` |
|     15 | 5581 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5582 | `	return PH7_OK;` |
|      9 | 5583 |  |
|      - | 5584 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5585 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5586 | `/*` |
|      - | 5587 | ` * string str_shuffle(string $str)` |
|      - | 5588 |  |
|      - | 5589 | ` *  Randomly shuffles a string.` |
|      - | 5590 | ` * Parameters` |
|      - | 5591 | ` *  $str` |
|      - | 5592 | ` *   The input string.` |
|      - | 5593 | ` * Return` |
|      - | 5594 | ` *  Returns the shuffled string.` |
|      - | 5595 | ` */` |
|     12 | 5596 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5597 |  |
|      - | 5598 | `	const char *zString;` |
|      - | 5599 | `	int nLen,i,c;` |
|      - | 5600 | `	sxu32 iR;` |
|     13 | 5601 | `	if( nArg < 1 ){` |
|      - | 5602 | `		/* Missing arguments,return the empty string */` |
|      3 | 5603 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5604 | `		return PH7_OK;` |
|      - | 5605 | `	}` |
|      - | 5606 | `	/* Extract the target string */` |
|     11 | 5607 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5608 | `	if( nLen < 1 ){` |
|      - | 5609 | `		/* Nothing to shuffle */` |
|      3 | 5610 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5611 | `		return PH7_OK;` |
|      - | 5612 | `	}` |
|      - | 5613 | `	/* Shuffle the string */` |
|     43 | 5614 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5615 | `		/* Generate a random number first */` |
|     35 | 5616 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5617 | `		/* Extract a random offset */` |
|     35 | 5618 | `		c = zString[iR % nLen];` |
|      - | 5619 | `		/* Append it */` |
|     35 | 5620 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5621 | `	}` |
|      9 | 5622 | `	return PH7_OK;` |
|      7 | 5623 |  |
|      - | 5624 | `/*` |
|      - | 5625 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5626 | ` *  Convert a string to an array.` |
|      - | 5627 | ` * Parameters` |
|      - | 5628 | ` * $string` |
|      - | 5629 | ` *  The input string.` |
|      - | 5630 | ` * $split_length` |
|      - | 5631 | ` *  Maximum length of the chunk.` |
|      - | 5632 | ` * Return` |
|      - | 5633 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5634 | ` *  except possibly the last one which may be shorter.` |
|      - | 5635 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5636 | ` *  as the first (and only) array element.` |
|      - | 5637 | ` *  An empty string returns an empty array.` |
|      - | 5638 | ` * Errors` |
|      - | 5639 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5640 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5641 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5642 | ` */` |
|     28 | 5643 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5644 |  |
|      - | 5645 | `	const char *zString,*zEnd;` |
|      - | 5646 | `	ph7_value *pArray,*pValue;` |
|      - | 5647 | `	int split_len;` |
|      - | 5648 | `	int nLen;` |
|     33 | 5649 | `	if( nArg < 1 ){` |
|      4 | 5650 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5651 | `			"ArgumentCountError",` |
|      - | 5652 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5653 | `			nArg` |
|      - | 5654 | `			);` |
|      - | 5655 | `	}` |
|      - | 5656 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 5657 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 5658 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5659 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5660 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5661 | `			"TypeError",` |
|      - | 5662 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5663 | `			ph7_type_name(apArg[0])` |
|      - | 5664 | `			);` |
|      - | 5665 | `	}` |
|      - | 5666 | `	/* Point to the target string */` |
|     27 | 5667 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 5668 | `	split_len = (int)sizeof(char);` |
|     27 | 5669 | `	if( nArg > 1 ){` |
|      - | 5670 | `		/* Split length */` |
|     17 | 5671 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 5672 | `		if( split_len < 1 ){` |
|      6 | 5673 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5674 | `				"ValueError",` |
|      - | 5675 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5676 | `				);` |
|      - | 5677 | `		}` |
|     11 | 5678 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5679 | `			split_len = nLen;` |
|      1 | 5680 | `		}` |
|      5 | 5681 | `	}` |
|      - | 5682 | `	/* Create the array and the scalar value */` |
|     21 | 5683 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5684 | `	/*Chunk value */` |
|     21 | 5685 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 5686 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5687 | `		/* Return FALSE */` |
|    ! 0 | 5688 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5689 | `		return PH7_OK;` |
|      - | 5690 | `	}` |
|      - | 5691 | `	/* Point to the end of the string */` |
|     21 | 5692 | `	zEnd = &zString[nLen];` |
|      - | 5693 | `	/* Perform the requested operation */` |
|     48 | 5694 | `	for(;;){` |
|      - | 5695 | `		int nMax;` |
|     59 | 5696 | `		if( zString >= zEnd ){` |
|      - | 5697 | `			/* No more input to process */` |
|     21 | 5698 | `			break;` |
|      - | 5699 | `		}` |
|     39 | 5700 | `		nMax = (int)(zEnd-zString);` |
|     39 | 5701 | `		if( nMax < split_len ){` |
|      3 | 5702 | `			split_len = nMax;` |
|      1 | 5703 | `		}` |
|      - | 5704 | `		/* Copy the current chunk */` |
|     39 | 5705 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5706 | `		/* Insert it */` |
|     39 | 5707 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 5708 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5709 | `		}` |
|      - | 5710 | `		/* reset the string cursor */` |
|     39 | 5711 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5712 | `		/* Update position */` |
|     39 | 5713 | `		zString += split_len;` |
|      1 | 5714 | `	}` |
|      - | 5715 | `	/*` |
|      - | 5716 | `	 * Return the array.` |
|      - | 5717 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5718 | `	 * upon we return from this function.` |
|      - | 5719 | `	 */` |
|     21 | 5720 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 5721 | `	return PH7_OK;` |
|     19 | 5722 |  |
|      - | 5723 | `/*` |
|      - | 5724 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5725 | ` * Refer to [strspn()].` |
|      - | 5726 | ` */` |
|     28 | 5727 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5728 |  |
|     29 | 5729 | `	const char *zIn = *pzIn;` |
|      - | 5730 | `	const char *zPtr;` |
|      - | 5731 | `	/* Ignore leading white spaces */` |
|     29 | 5732 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5733 | `		zIn++;` |
|    ! 0 | 5734 | `	}` |
|     29 | 5735 | `	if( zIn >= zEnd ){` |
|      - | 5736 | `		/* End of input */` |
|    ! 0 | 5737 | `		return SXERR_EOF;` |
|      - | 5738 | `	}` |
|     29 | 5739 | `	zPtr = zIn;` |
|      - | 5740 | `	/* Extract the token */` |
|    201 | 5741 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5742 | `		zIn++;` |
|      1 | 5743 | `	}` |
|     29 | 5744 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5745 | `	/* Synchronize pointers */` |
|     29 | 5746 | `	*pzIn = zIn;` |
|      - | 5747 | `	/* Return to the caller */` |
|     29 | 5748 | `	return SXRET_OK;` |
|     15 | 5749 |  |
|      - | 5750 | `/*` |
|      - | 5751 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5752 | ` * return the longest match.` |
|      - | 5753 | ` * Refer to [strspn()].` |
|      - | 5754 | ` */` |
|     18 | 5755 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5756 |  |
|     19 | 5757 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5758 | `	const char *zIn = zString;` |
|      - | 5759 | `	int i,c;` |
|     45 | 5760 | `	for(;;){` |
|     91 | 5761 | `		if( zString >= zEnd ){` |
|      7 | 5762 | `			break;` |
|      - | 5763 | `		}` |
|      - | 5764 | `		/* Extract current character */` |
|     85 | 5765 | `		c = zString[0];` |
|      - | 5766 | `		/* Perform the lookup */` |
|    383 | 5767 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5768 | `			if( c == zMask[i] ){` |
|      - | 5769 | `				/* Character found */` |
|     73 | 5770 | `				break;` |
|      - | 5771 | `			}` |
|    150 | 5772 | `		}` |
|     85 | 5773 | `		if( i >= nMaskLen ){` |
|      - | 5774 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5775 | `			break;` |
|      - | 5776 | `		}` |
|      - | 5777 | `		/* Advance cursor */` |
|     73 | 5778 | `		zString++;` |
|      1 | 5779 | `	}` |
|      - | 5780 | `	/* Longest match */` |
|     19 | 5781 | `	return (int)(zString-zIn);` |
|      1 | 5782 |  |
|      - | 5783 | `/*` |
|      - | 5784 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5785 | ` * Refer to [strcspn()].` |
|      - | 5786 | ` */` |
|     10 | 5787 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5788 |  |
|     11 | 5789 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5790 | `	const char *zIn = zString;` |
|      - | 5791 | `	int i,c;` |
|     12 | 5792 | `	for(;;){` |
|     25 | 5793 | `		if( zString >= zEnd ){` |
|      3 | 5794 | `			break;` |
|      - | 5795 | `		}` |
|      - | 5796 | `		/* Extract current character */` |
|     23 | 5797 | `		c = zString[0];` |
|      - | 5798 | `		/* Perform the lookup */` |
|     51 | 5799 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5800 | `			if( c == zMask[i] ){` |
|      9 | 5801 | `				break;` |
|      - | 5802 | `			}` |
|     15 | 5803 | `		}` |
|     23 | 5804 | `		if( i < nMaskLen ){` |
|      - | 5805 | `			/* Character in the current mask,break immediately */` |
|      9 | 5806 | `			break;` |
|      - | 5807 | `		}` |
|      - | 5808 | `		/* Advance cursor */` |
|     15 | 5809 | `		zString++;` |
|      1 | 5810 | `	}` |
|      - | 5811 | `	/* Longest match */` |
|     11 | 5812 | `	return (int)(zString-zIn);` |
|      1 | 5813 |  |
|      - | 5814 | `/*` |
|      - | 5815 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5816 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5817 | ` *  of characters contained within a given mask.` |
|      - | 5818 | ` * Parameters` |
|      - | 5819 | ` * $str` |
|      - | 5820 | ` *  The input string.` |
|      - | 5821 | ` * $mask` |
|      - | 5822 | ` *  The list of allowable characters.` |
|      - | 5823 | ` * $start` |
|      - | 5824 | ` *  The position in subject to start searching.` |
|      - | 5825 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5826 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5827 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5828 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5829 | ` *  start'th position from the end of subject.` |
|      - | 5830 | ` * $length` |
|      - | 5831 | ` *  The length of the segment from subject to examine.` |
|      - | 5832 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5833 | ` *  characters after the starting position.` |
|      - | 5834 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5835 | ` *  position up to length characters from the end of subject.` |
|      - | 5836 | ` * Return` |
|      - | 5837 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5838 | ` * in mask.` |
|      - | 5839 | ` */` |
|     26 | 5840 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5841 |  |
|      - | 5842 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5843 | `	int iMasklen,iLen;` |
|      - | 5844 | `	SyString sToken;` |
|     27 | 5845 | `	int iCount = 0;` |
|      - | 5846 | `	int rc;` |
|     27 | 5847 | `	if( nArg < 2 ){` |
|      - | 5848 | `		/* Missing agruments,return zero */` |
|      3 | 5849 | `		ph7_result_int(pCtx,0);` |
|      3 | 5850 | `		return PH7_OK;` |
|      - | 5851 | `	}` |
|      - | 5852 | `	/* Extract the target string */` |
|     25 | 5853 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5854 | `	/* Extract the mask */` |
|     25 | 5855 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5856 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5857 | `		/* Nothing to process,return zero */` |
|      7 | 5858 | `		ph7_result_int(pCtx,0);` |
|      7 | 5859 | `		return PH7_OK;` |
|      - | 5860 | `	}` |
|     19 | 5861 | `	if( nArg > 2 ){` |
|      - | 5862 | `		int nOfft;` |
|      - | 5863 | `		/* Extract the offset */` |
|      9 | 5864 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5865 | `		if( nOfft < 0 ){` |
|    ! 0 | 5866 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5867 | `			if( zBase > zString ){` |
|    ! 0 | 5868 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5869 | `				zString = zBase;` |
|    ! 0 | 5870 | `			}else{` |
|      - | 5871 | `				/* Invalid offset */` |
|    ! 0 | 5872 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5873 | `				return PH7_OK;` |
|      - | 5874 | `			}` |
|    ! 0 | 5875 | `		}else{` |
|      9 | 5876 | `			if( nOfft >= iLen ){` |
|      - | 5877 | `				/* Invalid offset */` |
|    ! 0 | 5878 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5879 | `				return PH7_OK;` |
|    ! 0 | 5880 | `			}else{` |
|      - | 5881 | `				/* Update offset */` |
|      9 | 5882 | `				zString += nOfft;` |
|      9 | 5883 | `				iLen -= nOfft;` |
|      - | 5884 | `			}` |
|      - | 5885 | `		}` |
|      9 | 5886 | `		if( nArg > 3 ){` |
|      - | 5887 | `			int iUserlen;` |
|      - | 5888 | `			/* Extract the desired length */` |
|      9 | 5889 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5890 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5891 | `				iLen = iUserlen;` |
|      2 | 5892 | `			}` |
|      4 | 5893 | `		}` |
|      4 | 5894 | `	}` |
|      - | 5895 | `	/* Point to the end of the string */` |
|     19 | 5896 | `	zEnd = &zString[iLen];` |
|      - | 5897 | `	/* Extract the first non-space token */` |
|     19 | 5898 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5899 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5900 | `		/* Compare against the current mask */` |
|     19 | 5901 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5902 | `	}` |
|      - | 5903 | `	/* Longest match */` |
|     19 | 5904 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5905 | `	return PH7_OK;` |
|     14 | 5906 |  |
|      - | 5907 | `/*` |
|      - | 5908 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5909 | ` *  Find length of initial segment not matching mask.` |
|      - | 5910 | ` * Parameters` |
|      - | 5911 | ` * $str` |
|      - | 5912 | ` *  The input string.` |
|      - | 5913 | ` * $mask` |
|      - | 5914 | ` *  The list of not allowed characters.` |
|      - | 5915 | ` * $start` |
|      - | 5916 | ` *  The position in subject to start searching.` |
|      - | 5917 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5918 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5919 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5920 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5921 | ` *  start'th position from the end of subject.` |
|      - | 5922 | ` * $length` |
|      - | 5923 | ` *  The length of the segment from subject to examine.` |
|      - | 5924 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5925 | ` *  characters after the starting position.` |
|      - | 5926 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5927 | ` *  position up to length characters from the end of subject.` |
|      - | 5928 | ` * Return` |
|      - | 5929 | ` *  Returns the length of the segment as an integer.` |
|      - | 5930 | ` */` |
|     16 | 5931 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5932 |  |
|      - | 5933 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5934 | `	int iMasklen,iLen;` |
|      - | 5935 | `	SyString sToken;` |
|     17 | 5936 | `	int iCount = 0;` |
|      - | 5937 | `	int rc;` |
|     17 | 5938 | `	if( nArg < 2 ){` |
|      - | 5939 | `		/* Missing agruments,return zero */` |
|      3 | 5940 | `		ph7_result_int(pCtx,0);` |
|      3 | 5941 | `		return PH7_OK;` |
|      - | 5942 | `	}` |
|      - | 5943 | `	/* Extract the target string */` |
|     15 | 5944 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5945 | `	/* Extract the mask */` |
|     15 | 5946 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5947 | `	if( iLen < 1 ){` |
|      - | 5948 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5949 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5950 | `		return PH7_OK;` |
|      - | 5951 | `	}` |
|     15 | 5952 | `	if( iMasklen < 1 ){` |
|      - | 5953 | `		/* No given mask,return the string length */` |
|      3 | 5954 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5955 | `		return PH7_OK;` |
|      - | 5956 | `	}` |
|     13 | 5957 | `	if( nArg > 2 ){` |
|      - | 5958 | `		int nOfft;` |
|      - | 5959 | `		/* Extract the offset */` |
|     11 | 5960 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5961 | `		if( nOfft < 0 ){` |
|    ! 0 | 5962 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5963 | `			if( zBase > zString ){` |
|    ! 0 | 5964 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5965 | `				zString = zBase;` |
|    ! 0 | 5966 | `			}else{` |
|      - | 5967 | `				/* Invalid offset */` |
|    ! 0 | 5968 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5969 | `				return PH7_OK;` |
|      - | 5970 | `			}` |
|    ! 0 | 5971 | `		}else{` |
|     11 | 5972 | `			if( nOfft >= iLen ){` |
|      - | 5973 | `				/* Invalid offset */` |
|      3 | 5974 | `				ph7_result_int(pCtx,0);` |
|      3 | 5975 | `				return PH7_OK;` |
|    ! 0 | 5976 | `			}else{` |
|      - | 5977 | `				/* Update offset */` |
|      9 | 5978 | `				zString += nOfft;` |
|      9 | 5979 | `				iLen -= nOfft;` |
|      - | 5980 | `			}` |
|      - | 5981 | `		}` |
|      9 | 5982 | `		if( nArg > 3 ){` |
|      - | 5983 | `			int iUserlen;` |
|      - | 5984 | `			/* Extract the desired length */` |
|    ! 0 | 5985 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5986 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5987 | `				iLen = iUserlen;` |
|    ! 0 | 5988 | `			}` |
|    ! 0 | 5989 | `		}` |
|      4 | 5990 | `	}` |
|      - | 5991 | `	/* Point to the end of the string */` |
|     11 | 5992 | `	zEnd = &zString[iLen];` |
|      - | 5993 | `	/* Extract the first non-space token */` |
|     11 | 5994 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5995 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5996 | `		/* Compare against the current mask */` |
|     11 | 5997 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5998 | `	}` |
|      - | 5999 | `	/* Longest match */` |
|     11 | 6000 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 6001 | `	return PH7_OK;` |
|      9 | 6002 |  |
|      - | 6003 | `/*` |
|      - | 6004 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 6005 | ` *  Search a string for any of a set of characters.` |
|      - | 6006 | ` * Parameters` |
|      - | 6007 | ` *  $haystack` |
|      - | 6008 | ` *   The string where char_list is looked for.` |
|      - | 6009 | ` *  $char_list` |
|      - | 6010 | ` *   This parameter is case sensitive.` |
|      - | 6011 | ` * Return` |
|      - | 6012 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 6013 | ` */` |
|      6 | 6014 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6015 |  |
|      - | 6016 | `	const char *zString,*zList,*zEnd;` |
|      - | 6017 | `	int iLen,iListLen,i,c;` |
|      - | 6018 | `	sxu32 nOfft,nMax;` |
|      - | 6019 | `	sxi32 rc;` |
|      7 | 6020 | `	if( nArg < 2 ){` |
|      - | 6021 | `		/* Missing arguments,return FALSE */` |
|      3 | 6022 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6023 | `		return PH7_OK;` |
|      - | 6024 | `	}` |
|      - | 6025 | `	/* Extract the haystack and the char list */` |
|      5 | 6026 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 6027 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 6028 | `	if( iLen < 1 ){` |
|      - | 6029 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6030 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6031 | `		return PH7_OK;` |
|      - | 6032 | `	}` |
|      - | 6033 | `	/* Point to the end of the string */` |
|      5 | 6034 | `	zEnd = &zString[iLen];` |
|      5 | 6035 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6036 | `	/* perform the requested operation */` |
|     15 | 6037 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6038 | `		c = zList[i];` |
|     11 | 6039 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6040 | `		if( rc == SXRET_OK ){` |
|      5 | 6041 | `			if( nMax < nOfft ){` |
|      3 | 6042 | `				nOfft = nMax;` |
|      1 | 6043 | `			}` |
|      2 | 6044 | `		}` |
|      6 | 6045 | `	}` |
|      5 | 6046 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6047 | `		/* No such substring,return FALSE */` |
|      3 | 6048 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6049 | `	}else{` |
|      - | 6050 | `		/* Return the substring */` |
|      3 | 6051 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6052 | `	}` |
|      5 | 6053 | `	return PH7_OK;` |
|      4 | 6054 |  |
|      - | 6055 | `/* SPDX-SnippetBegin */` |
|      - | 6056 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6057 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6058 | `/*` |
|      - | 6059 | ` * string soundex(string $str)` |
|      - | 6060 | ` *  Calculate the soundex key of a string.` |
|      - | 6061 | ` * Parameters` |
|      - | 6062 | ` *  $str` |
|      - | 6063 | ` *   The input string.` |
|      - | 6064 | ` * Return` |
|      - | 6065 | ` *  Returns the soundex key as a string.` |
|      - | 6066 | ` * Note:` |
|      - | 6067 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6068 | ` * source tree.` |
|      - | 6069 | ` */` |
|     20 | 6070 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6071 |  |
|      - | 6072 | `	const unsigned char *zIn;` |
|      - | 6073 | `	char zResult[8];` |
|      - | 6074 | `	int i, j;` |
|      - | 6075 | `	static const unsigned char iCode[] = {` |
|      - | 6076 |  |
|      - | 6077 |  |
|      - | 6078 |  |
|      - | 6079 |  |
|      - | 6080 |  |
|      - | 6081 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6082 |  |
|      - | 6083 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6084 | `	};` |
|     21 | 6085 | `	if( nArg < 1 ){` |
|      - | 6086 | `		/* Missing arguments,return the empty string */` |
|      3 | 6087 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6088 | `		return PH7_OK;` |
|      - | 6089 | `	}` |
|     19 | 6090 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 6091 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 6092 | `	if( zIn[i] ){` |
|     17 | 6093 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6094 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6095 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6096 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6097 | `			if( code>0 ){` |
|     45 | 6098 | `				if( code!=prevcode ){` |
|     33 | 6099 | `					prevcode = (unsigned char)code;` |
|     33 | 6100 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6101 | `				}` |
|     23 | 6102 | `			}else{` |
|     49 | 6103 | `				prevcode = 0;` |
|      - | 6104 | `			}` |
|     47 | 6105 | `		}` |
|     33 | 6106 | `		while( j<4 ){` |
|     17 | 6107 | `			zResult[j++] = '0';` |
|      1 | 6108 | `		}` |
|     17 | 6109 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6110 | `	}else{` |
|      3 | 6111 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 6112 | `	}` |
|     19 | 6113 | `	return PH7_OK;` |
|     11 | 6114 |  |
|      - | 6115 | `/* SPDX-SnippetEnd */` |
|      - | 6116 | `/*` |
|      - | 6117 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6118 | ` *  Wraps a string to a given number of characters.` |
|      - | 6119 | ` * Parameters` |
|      - | 6120 | ` *  $str` |
|      - | 6121 | ` *   The input string.` |
|      - | 6122 | ` * $width` |
|      - | 6123 | ` *  The column width.` |
|      - | 6124 | ` * $break` |
|      - | 6125 | ` *  The line is broken using the optional break parameter.` |
|      - | 6126 | ` * Return` |
|      - | 6127 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6128 | ` */` |
|     14 | 6129 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6130 |  |
|      - | 6131 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 6132 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 6133 | `	if( nArg < 1 ){` |
|      - | 6134 | `		/* Missing arguments,return the empty string */` |
|      3 | 6135 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6136 | `		return PH7_OK;` |
|      - | 6137 | `	}` |
|      - | 6138 | `	/* Extract the input string */` |
|     13 | 6139 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 6140 | `	if( iLen < 1 ){` |
|      - | 6141 | `		/* Nothing to process,return the empty string */` |
|      3 | 6142 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6143 | `		return PH7_OK;` |
|      - | 6144 | `	}` |
|      - | 6145 | `	/* Chunk length */` |
|     11 | 6146 | `	iChunk = 75;` |
|     11 | 6147 | `	iBreaklen = 0;` |
|     11 | 6148 | `	zBreak = ""; /* cc warning */` |
|     11 | 6149 | `	if( nArg > 1 ){` |
|     11 | 6150 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 6151 | `		if( iChunk < 1 ){` |
|    ! 0 | 6152 | `			iChunk = 75;` |
|    ! 0 | 6153 | `		}` |
|     11 | 6154 | `		if( nArg > 2 ){` |
|      3 | 6155 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 6156 | `		}` |
|      5 | 6157 | `	}` |
|     11 | 6158 | `	if( iBreaklen < 1 ){` |
|      - | 6159 | `		/* Set a default column break */` |
|      - | 6160 | `#ifdef __WINNT__` |
|      1 | 6161 | `		zBreak = "\r\n";` |
|      1 | 6162 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 6163 | `#else` |
|      8 | 6164 | `		zBreak = "\n";` |
|      8 | 6165 | `		iBreaklen = (int)sizeof(char);` |
|      - | 6166 | `#endif` |
|      4 | 6167 | `	}` |
|      - | 6168 | `	/* Perform the requested operation */` |
|     11 | 6169 | `	zEnd = &zIn[iLen];` |
|     41 | 6170 | `	for(;;){` |
|      - | 6171 | `		int nMax;` |
|     47 | 6172 | `		if( zIn >= zEnd ){` |
|      - | 6173 | `			/* No more input to process */` |
|     11 | 6174 | `			break;` |
|      - | 6175 | `		}` |
|     37 | 6176 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 6177 | `		if( iChunk > nMax ){` |
|     11 | 6178 | `			iChunk = nMax;` |
|      5 | 6179 | `		}` |
|      - | 6180 | `		/* Append the column first */` |
|     37 | 6181 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 6182 | `		/* Advance the cursor */` |
|     37 | 6183 | `		zIn += iChunk;` |
|     37 | 6184 | `		if( zIn < zEnd ){` |
|      - | 6185 | `			/* Append the line break */` |
|     27 | 6186 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 6187 | `		}` |
|      1 | 6188 | `	}` |
|     11 | 6189 | `	return PH7_OK;` |
|      8 | 6190 |  |
|      - | 6191 | `/*` |
|      - | 6192 | ` * Check if the given character is a member of the given mask.` |
|      - | 6193 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6194 | ` * Refer to [strtok()].` |
|      - | 6195 | ` */` |
|     30 | 6196 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6197 |  |
|      - | 6198 | `	int i;` |
|     57 | 6199 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6200 | `		if( c == zMask[i] ){` |
|     13 | 6201 | `			if( pOfft ){` |
|      5 | 6202 | `				*pOfft = i;` |
|      2 | 6203 | `			}` |
|     13 | 6204 | `			return TRUE;` |
|      - | 6205 | `		}` |
|     14 | 6206 | `	}` |
|     19 | 6207 | `	return FALSE;` |
|     16 | 6208 |  |
|      - | 6209 | `/*` |
|      - | 6210 | ` * Extract a single token from the input stream.` |
|      - | 6211 | ` * Refer to [strtok()].` |
|      - | 6212 | ` */` |
|      6 | 6213 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6214 |  |
|      7 | 6215 | `	const char *zIn = *pzIn;` |
|      - | 6216 | `	const char *zPtr;` |
|      - | 6217 | `	/* Ignore leading delimiter */` |
|     11 | 6218 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6219 | `		zIn++;` |
|      1 | 6220 | `	}` |
|      7 | 6221 | `	if( zIn >= zEnd ){` |
|      - | 6222 | `		/* End of input */` |
|    ! 0 | 6223 | `		return SXERR_EOF;` |
|      - | 6224 | `	}` |
|      7 | 6225 | `	zPtr = zIn;` |
|      - | 6226 | `	/* Extract the token */` |
|     13 | 6227 | `	while( zIn < zEnd ){` |
|     11 | 6228 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6229 | `			/* UTF-8 stream */` |
|    ! 0 | 6230 | `			zIn++;` |
|    ! 0 | 6231 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6232 | `		}else{` |
|     11 | 6233 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6234 | `				break;` |
|      - | 6235 | `			}` |
|      7 | 6236 | `			zIn++;` |
|      - | 6237 | `		}` |
|      1 | 6238 | `	}` |
|      7 | 6239 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6240 | `	/* Update the cursor */` |
|      7 | 6241 | `	*pzIn = zIn;` |
|      - | 6242 | `	/* Return to the caller */` |
|      7 | 6243 | `	return SXRET_OK;` |
|      4 | 6244 |  |
|      - | 6245 | `/* strtok auxiliary private data */` |
|      - | 6246 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6247 | `struct strtok_aux_data` |
|      - | 6248 |  |
|      - | 6249 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6250 | `	const char *zIn;   /* Current input stream */` |
|      - | 6251 | `	const char *zEnd;  /* End of input */` |
|      - | 6252 | `};` |
|      - | 6253 | `/*` |
|      - | 6254 | ` * string strtok(string $str,string $token)` |
|      - | 6255 | ` * string strtok(string $token)` |
|      - | 6256 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6257 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6258 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6259 | ` *  words by using the space character as the token.` |
|      - | 6260 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6261 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6262 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6263 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6264 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6265 | ` *  the argument are found.` |
|      - | 6266 | ` * Parameters` |
|      - | 6267 | ` *  $str` |
|      - | 6268 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6269 | ` * $token` |
|      - | 6270 | ` *  The delimiter used when splitting up str.` |
|      - | 6271 | ` * Return` |
|      - | 6272 | ` *   Current token or FALSE on EOF.` |
|      - | 6273 | ` */` |
|      8 | 6274 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6275 |  |
|      - | 6276 | `	strtok_aux_data *pAux;` |
|      - | 6277 | `	const char *zMask;` |
|      - | 6278 | `	SyString sToken;` |
|      - | 6279 | `	int nMasklen;` |
|      - | 6280 | `	sxi32 rc;` |
|      9 | 6281 | `	if( nArg < 2 ){` |
|      - | 6282 | `		/* Extract top aux data */` |
|      7 | 6283 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 6284 | `		if( pAux == 0 ){` |
|      - | 6285 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6286 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6287 | `			return PH7_OK;` |
|      - | 6288 | `		}` |
|      7 | 6289 | `		nMasklen = 0;` |
|      7 | 6290 | `		zMask = ""; /* cc warning */` |
|      7 | 6291 | `		if( nArg > 0 ){` |
|      - | 6292 | `			/* Extract the mask */` |
|      5 | 6293 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6294 | `		}` |
|      7 | 6295 | `		if( nMasklen < 1 ){` |
|      - | 6296 | `			/* Invalid mask,return FALSE */` |
|      3 | 6297 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 6298 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 6299 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 6300 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6301 | `			return PH7_OK;` |
|      - | 6302 | `		}` |
|      - | 6303 | `		/* Extract the token */` |
|      5 | 6304 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6305 | `		if( rc != SXRET_OK ){` |
|      - | 6306 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6307 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6308 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6309 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6310 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6311 | `		}else{` |
|      - | 6312 | `			/* Return the extracted token */` |
|      5 | 6313 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6314 | `		}` |
|      3 | 6315 | `	}else{` |
|      - | 6316 | `		const char *zInput,*zCur;` |
|      - | 6317 | `		char *zDup;` |
|      - | 6318 | `		int nLen;` |
|      - | 6319 | `		/* Extract the raw input */` |
|      3 | 6320 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6321 | `		if( nLen < 1 ){` |
|      - | 6322 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6323 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6324 | `			return PH7_OK;` |
|      - | 6325 | `		}` |
|      - | 6326 | `		/* Extract the mask */` |
|      3 | 6327 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6328 | `		if( nMasklen < 1 ){` |
|      - | 6329 | `			/* Set a default mask */` |
|      - | 6330 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6331 | `			zMask = TOK_MASK;` |
|    ! 0 | 6332 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6333 | `#undef TOK_MASK` |
|    ! 0 | 6334 | `		}` |
|      - | 6335 | `		/* Extract a single token */` |
|      3 | 6336 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6337 | `		if( rc != SXRET_OK ){` |
|      - | 6338 | `			/* Empty input */` |
|    ! 0 | 6339 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6340 | `			return PH7_OK;` |
|    ! 0 | 6341 | `		}else{` |
|      - | 6342 | `			/* Return the extracted token */` |
|      3 | 6343 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6344 | `		}` |
|      - | 6345 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6346 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6347 | `		if( pAux ){` |
|      3 | 6348 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6349 | `			if( nLen < 1 ){` |
|    ! 0 | 6350 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6351 | `				return PH7_OK;` |
|      - | 6352 | `			}` |
|      - | 6353 | `			/* Duplicate input */` |
|      3 | 6354 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6355 | `			if( zDup  ){` |
|      3 | 6356 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6357 | `				/* Register the aux data */` |
|      3 | 6358 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6359 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6360 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6361 | `			}` |
|      1 | 6362 | `		}` |
|      - | 6363 | `	}` |
|      7 | 6364 | `	return PH7_OK;` |
|      5 | 6365 |  |
|      - | 6366 | `/*` |
|      - | 6367 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6368 | ` *  Pad a string to a certain length with another string` |
|      - | 6369 | ` * Parameters` |
|      - | 6370 | ` *  $input` |
|      - | 6371 | ` *   The input string.` |
|      - | 6372 | ` * $pad_length` |
|      - | 6373 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6374 | ` *   string, no padding takes place.` |
|      - | 6375 | ` * $pad_string` |
|      - | 6376 | ` *   Note:` |
|      - | 6377 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6378 | ` *    divided by the pad_string's length.` |
|      - | 6379 | ` * $pad_type` |
|      - | 6380 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6381 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6382 | ` * Return` |
|      - | 6383 | ` *  The padded string.` |
|      - | 6384 | ` */` |
|     10 | 6385 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6386 |  |
|      - | 6387 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6388 | `	const char *zIn,*zPad;` |
|     11 | 6389 | `	if( nArg < 2 ){` |
|      - | 6390 | `		/* Missing arguments,return the empty string */` |
|      5 | 6391 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6392 | `		return PH7_OK;` |
|      - | 6393 | `	}` |
|      - | 6394 | `	/* Extract the target string */` |
|      7 | 6395 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6396 | `	/* Padding length */` |
|      7 | 6397 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6398 | `	if( iPadlen > 0 ){` |
|      5 | 6399 | `		iPadlen -= iLen;` |
|      2 | 6400 | `	}` |
|      7 | 6401 | `	if( iPadlen < 1  ){` |
|      - | 6402 | `		/* Return the string verbatim */` |
|      3 | 6403 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      3 | 6404 | `		return PH7_OK;` |
|      - | 6405 | `	}` |
|      5 | 6406 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6407 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6408 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6409 | `	if( nArg > 2 ){` |
|      - | 6410 | `		/* Padding string */` |
|      5 | 6411 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6412 | `		if( iStrpad < 1 ){` |
|      - | 6413 | `			/* Empty string */` |
|    ! 0 | 6414 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6415 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6416 | `		}` |
|      5 | 6417 | `		if( nArg > 3 ){` |
|      - | 6418 | `			/* Padd type */` |
|      5 | 6419 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6420 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6421 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6422 | `			}` |
|      2 | 6423 | `		}` |
|      2 | 6424 | `	}` |
|      5 | 6425 | `	iDiv = 1;` |
|      5 | 6426 | `	if( iType == 2 ){` |
|    ! 0 | 6427 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6428 | `	}` |
|      - | 6429 | `	/* Perform the requested operation */` |
|      5 | 6430 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6431 | `		jPad = iStrpad;` |
|      5 | 6432 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6433 | `			/* Padding */` |
|      5 | 6434 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6435 | `				break;` |
|      - | 6436 | `			}` |
|      3 | 6437 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6438 | `		}` |
|      3 | 6439 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6440 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6441 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6442 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6443 | `					jPad = iStrpad;` |
|    ! 0 | 6444 | `				}` |
|      3 | 6445 | `				if( jPad < 1){` |
|    ! 0 | 6446 | `					break;` |
|      - | 6447 | `				}` |
|      3 | 6448 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6449 | `			}` |
|      1 | 6450 | `		}` |
|      1 | 6451 | `	}` |
|      5 | 6452 | `	if( iLen > 0 ){` |
|      - | 6453 | `		/* Append the input string */` |
|      5 | 6454 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6455 | `	}` |
|      5 | 6456 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6457 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6458 | `			/* Padding */` |
|      5 | 6459 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6460 | `				break;` |
|      - | 6461 | `			}` |
|      3 | 6462 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6463 | `		}` |
|      5 | 6464 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6465 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6466 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6467 | `				jPad = iStrpad;` |
|    ! 0 | 6468 | `			}` |
|      3 | 6469 | `			if( jPad < 1){` |
|    ! 0 | 6470 | `				break;` |
|      - | 6471 | `			}` |
|      3 | 6472 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6473 | `		}` |
|      1 | 6474 | `	}` |
|      5 | 6475 | `	return PH7_OK;` |
|      6 | 6476 |  |
|      - | 6477 | `/*` |
|      - | 6478 | ` * String replacement private data.` |
|      - | 6479 | ` */` |
|      - | 6480 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6481 | `struct str_replace_data` |
|      - | 6482 |  |
|      - | 6483 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6484 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6485 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6486 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6487 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6488 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6489 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 6490 | `};` |
|      - | 6491 | `/*` |
|      - | 6492 | ` * Remove a substring.` |
|      - | 6493 | ` */` |
|      - | 6494 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6495 | `	for(;;){\` |
|      - | 6496 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6497 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6498 | `		++OFFT;\` |
|      - | 6499 | `	}\` |
|      - | 6500 |  |
|      - | 6501 | `/*` |
|      - | 6502 | ` * Shift right and insert algorithm.` |
|      - | 6503 | ` */` |
|      - | 6504 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6505 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6506 | `		for(;;){\` |
|      - | 6507 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6508 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6509 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6510 | `			--INLEN; \` |
|      - | 6511 | `		}\` |
|      - | 6512 | `		for(;;){\` |
|      - | 6513 | `				if(ELEN < 1) { break; }\` |
|      - | 6514 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6515 | `				OFFT++;\` |
|      - | 6516 | `				ENTRY++;\` |
|      - | 6517 | `				--ELEN;\` |
|      - | 6518 | `		}\` |
|      - | 6519 |  |
|      - | 6520 | `/*` |
|      - | 6521 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6522 | ` * replacement string [i.e: zReplace].` |
|      - | 6523 | ` */` |
|     38 | 6524 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6525 |  |
|     39 | 6526 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6527 | `	sxu32 n,m;` |
|     39 | 6528 | `	n = SyBlobLength(pWorker);` |
|     39 | 6529 | `	m = nOfft;` |
|      - | 6530 | `	/* Delete the old entry */` |
|    475 | 6531 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6532 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6533 | `	if( nReplen > 0 ){` |
|     33 | 6534 | `		sxi32 iRep = nReplen;` |
|      - | 6535 | `		sxi32 rc;` |
|      - | 6536 | `		/*` |
|      - | 6537 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6538 | `		 * string.` |
|      - | 6539 | `		 */` |
|     33 | 6540 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6541 | `		if( rc != SXRET_OK ){` |
|      - | 6542 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 6543 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 6544 | `			return rc;` |
|      - | 6545 | `		}` |
|      - | 6546 | `		/* Perform the insertion now */` |
|     33 | 6547 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6548 | `		n = SyBlobLength(pWorker);` |
|    163 | 6549 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6550 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6551 | `	}` |
|     39 | 6552 | `	return SXRET_OK;` |
|     20 | 6553 |  |
|      - | 6554 | `/*` |
|      - | 6555 | ` * String replacement walker callback.` |
|      - | 6556 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6557 | ` * the replace string.` |
|      - | 6558 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6559 | ` */` |
|      8 | 6560 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6561 |  |
|      9 | 6562 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6563 | `	const char *zTarget,*zReplace;` |
|      - | 6564 | `	SyBlob *pWorker;` |
|      - | 6565 | `	int tLen,nLen;` |
|      - | 6566 | `	sxu32 nOfft;` |
|      - | 6567 | `	sxi32 rc;` |
|      - | 6568 | `	/* Point to the working buffer */` |
|      9 | 6569 | `	pWorker = pRepData->pWorker;` |
|      9 | 6570 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6571 | `		/* Target and replace must be a string */` |
|      3 | 6572 | `		return PH7_OK;` |
|      - | 6573 | `	}` |
|      - | 6574 | `	/* Extract the target and the replace */` |
|      7 | 6575 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6576 | `	if( tLen < 1 ){` |
|      - | 6577 | `		/* Empty target,return immediately */` |
|    ! 0 | 6578 | `		return PH7_OK;` |
|      - | 6579 | `	}` |
|      - | 6580 | `	/* Perform a pattern search */` |
|      7 | 6581 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6582 | `	if( rc != SXRET_OK ){` |
|      - | 6583 | `		/* Pattern not found */` |
|    ! 0 | 6584 | `		return PH7_OK;` |
|      - | 6585 | `	}` |
|      - | 6586 | `	/* Extract the replace string */` |
|      7 | 6587 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6588 | `	/* Perform the replace process */` |
|      7 | 6589 | `	rc = StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      7 | 6590 | `	if( rc != SXRET_OK ){` |
|      - | 6591 | `		/* Allocation failure: carry it out and stop the walk */` |
|    ! 0 | 6592 | `		pRepData->rc = rc;` |
|    ! 0 | 6593 | `		return rc;` |
|      - | 6594 | `	}` |
|      - | 6595 | `	/* All done */` |
|      7 | 6596 | `	return PH7_OK;` |
|      5 | 6597 |  |
|      - | 6598 | `/*` |
|      - | 6599 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6600 | ` * to collect search/replace string.` |
|      - | 6601 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6602 | ` */` |
|     26 | 6603 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6604 |  |
|     27 | 6605 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6606 | `	SyString sWorker;` |
|      - | 6607 | `	const char *zIn;` |
|      - | 6608 | `	int nByte;` |
|      - | 6609 | `	/* Extract a string representation of the given argument */` |
|     27 | 6610 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6611 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6612 | `	if( nByte > 0 ){` |
|      - | 6613 | `		char *zDup;` |
|      - | 6614 | `		/* Duplicate the chunk */` |
|     25 | 6615 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6616 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6617 | `			);` |
|     25 | 6618 | `		if( zDup == 0 ){` |
|      - | 6619 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 6620 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 6621 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 6622 | `			return SXERR_MEM;` |
|      - | 6623 | `		}` |
|     25 | 6624 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6625 | `		/* Save the chunk */` |
|     25 | 6626 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6627 | `	}` |
|      - | 6628 | `	/* Save for later processing */` |
|     27 | 6629 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6630 | `	/* All done */` |
|     13 | 6631 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6632 | `	return PH7_OK;` |
|     14 | 6633 |  |
|      - | 6634 | `/*` |
|      - | 6635 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6636 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6637 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6638 | ` * Parameters` |
|      - | 6639 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6640 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6641 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6642 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6643 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6644 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6645 | ` * $search` |
|      - | 6646 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6647 | ` *  to designate multiple needles.` |
|      - | 6648 | ` * $replace` |
|      - | 6649 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6650 | ` *  to designate multiple replacements.` |
|      - | 6651 | ` * $subject` |
|      - | 6652 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6653 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6654 | ` *  of subject, and the return value is an array as well.` |
|      - | 6655 | ` * $count (Not used)` |
|      - | 6656 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6657 | ` * Return` |
|      - | 6658 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6659 | ` */` |
|  24058 | 6660 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6661 |  |
|      - | 6662 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6663 | `	ProcStringMatch xMatch;` |
|      - | 6664 | `	const char *zIn,*zFunc;` |
|      - | 6665 | `	str_replace_data sRep;` |
|      - | 6666 | `	SyBlob sWorker;` |
|      - | 6667 | `	SySet sReplace;` |
|      - | 6668 | `	SySet sSearch;` |
|      - | 6669 | `	int rep_str;` |
|      - | 6670 | `	int nByte;` |
|      - | 6671 | `	sxi32 rc;` |
|  24063 | 6672 | `	if( nArg < 3 ){` |
|      - | 6673 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6674 | `		ph7_result_null(pCtx);` |
|      7 | 6675 | `		return PH7_OK;` |
|      - | 6676 | `	}` |
|      - | 6677 | `	/* Initialize fields */` |
|  24057 | 6678 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  24057 | 6679 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  24057 | 6680 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  24057 | 6681 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  24057 | 6682 | `	sRep.pCtx = pCtx;` |
|  24057 | 6683 | `	sRep.pCollector = &sSearch;` |
|  24057 | 6684 | `	rep_str = 0;` |
|      - | 6685 | `	/* Extract the subject */` |
|  24057 | 6686 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  24057 | 6687 | `	if( nByte < 1 ){` |
|      - | 6688 | `		/* Nothing to replace,return the empty string */` |
|     29 | 6689 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 6690 | `		return PH7_OK;` |
|      - | 6691 | `	}` |
|      - | 6692 | `	/* Copy the subject */` |
|  24029 | 6693 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6694 | `	/* Search string */` |
|  24029 | 6695 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6696 | `		/* Collect search string */` |
|      9 | 6697 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6698 | `	}else{` |
|      - | 6699 | `		/* Single pattern */` |
|  24021 | 6700 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  24021 | 6701 | `		if( nByte < 1 ){` |
|      - | 6702 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6703 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6704 | `			return PH7_OK;` |
|      - | 6705 | `		}` |
|  24017 | 6706 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6707 | `		/* Save for later processing */` |
|  24017 | 6708 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6709 | `	}` |
|      - | 6710 | `	/* Replace string */` |
|  24025 | 6711 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6712 | `		/* Collect replace string */` |
|      7 | 6713 | `		sRep.pCollector = &sReplace;` |
|      7 | 6714 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6715 | `	}else{` |
|      - | 6716 | `		/* Single needle */` |
|  24019 | 6717 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  24019 | 6718 | `		rep_str = 1;` |
|  24019 | 6719 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6720 | `		/* Save for later processing */` |
|  24019 | 6721 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6722 | `	}` |
|      - | 6723 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  24025 | 6724 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 6725 | `		SySetRelease(&sSearch);` |
|    ! 0 | 6726 | `		SySetRelease(&sReplace);` |
|    ! 0 | 6727 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 6728 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6729 | `	}` |
|      - | 6730 | `	/* Reset loop cursors */` |
|  24025 | 6731 | `	SySetResetCursor(&sSearch);` |
|  24025 | 6732 | `	SySetResetCursor(&sReplace);` |
|  24025 | 6733 | `	pReplace = pSearch = 0; /* cc warning */` |
|  24025 | 6734 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6735 | `	/* Extract function name */` |
|  24025 | 6736 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6737 | `	/* Set the default pattern match routine */` |
|  24025 | 6738 | `	xMatch = SyBlobSearch;` |
|  24025 | 6739 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6740 | `		/* Case insensitive pattern match */` |
|     11 | 6741 | `		xMatch = iPatternMatch;` |
|      5 | 6742 | `	}` |
|      - | 6743 | `	/* Start the replace process */` |
|  48053 | 6744 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6745 | `		sxu32 nCount,nOfft;` |
|  24033 | 6746 | `		if( pSearch->nByte <  1 ){` |
|      - | 6747 | `			/* Empty string,ignore */` |
|      3 | 6748 | `			continue;` |
|      - | 6749 | `		}` |
|      - | 6750 | `		/* Extract the replace string */` |
|  24031 | 6751 | `		if( rep_str ){` |
|  24021 | 6752 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  12013 | 6753 | `		}else{` |
|     11 | 6754 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6755 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6756 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6757 | `				 */` |
|      3 | 6758 | `				pReplace = 0;` |
|      1 | 6759 | `			}` |
|      - | 6760 | `		}` |
|  24031 | 6761 | `		if( pReplace == 0 ){` |
|      - | 6762 | `			/* Use an empty string instead */` |
|      3 | 6763 | `			pReplace = &sTemp;` |
|      1 | 6764 | `		}` |
|  24031 | 6765 | `		nOfft = nCount = 0;` |
|  12029 | 6766 | `		for(;;){` |
|  24063 | 6767 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6768 | `				break;` |
|      - | 6769 | `			}` |
|      - | 6770 | `			/* Perform a pattern lookup */` |
|  36074 | 6771 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  24046 | 6772 | `				pSearch->nByte,&nOfft);` |
|  24051 | 6773 | `			if( rc != SXRET_OK ){` |
|      - | 6774 | `				/* Pattern not found */` |
|  24019 | 6775 | `				break;` |
|      - | 6776 | `			}` |
|      - | 6777 | `			/* Perform the replace operation */` |
|     33 | 6778 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 6779 | `			if( rc != SXRET_OK ){` |
|      - | 6780 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 6781 | `				SySetRelease(&sSearch);` |
|    ! 0 | 6782 | `				SySetRelease(&sReplace);` |
|    ! 0 | 6783 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 6784 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 6785 | `			}` |
|      - | 6786 | `			/* Increment offset counter */` |
|     33 | 6787 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6788 | `		}` |
|      5 | 6789 | `	}` |
|      - | 6790 | `	/* All done,clean-up the mess left behind */` |
|  24025 | 6791 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  24025 | 6792 | `	SySetRelease(&sSearch);` |
|  24025 | 6793 | `	SySetRelease(&sReplace);` |
|  24025 | 6794 | `	SyBlobRelease(&sWorker);` |
|  24025 | 6795 | `	if( rc != PH7_OK ){` |
|    ! 0 | 6796 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6797 | `	}` |
|  24025 | 6798 | `	return PH7_OK;` |
|  12034 | 6799 |  |
|      - | 6800 | `/*` |
|      - | 6801 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6802 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6803 | ` *  Translate characters or replace substrings.` |
|      - | 6804 | ` * Parameters` |
|      - | 6805 | ` *  $str` |
|      - | 6806 | ` *  The string being translated.` |
|      - | 6807 | ` * $from` |
|      - | 6808 | ` *  The string being translated to to.` |
|      - | 6809 | ` * $to` |
|      - | 6810 | ` *  The string replacing from.` |
|      - | 6811 | ` * $replace_pairs` |
|      - | 6812 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6813 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6814 | ` * Return` |
|      - | 6815 | ` *  The translated string.` |
|      - | 6816 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6817 | ` */` |
|     12 | 6818 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6819 |  |
|      - | 6820 | `	const char *zIn;` |
|      - | 6821 | `	int nLen;` |
|     13 | 6822 | `	if( nArg < 1 ){` |
|      - | 6823 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6824 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6825 | `		return PH7_OK;` |
|      - | 6826 | `	}` |
|      7 | 6827 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6828 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6829 | `		/* Invalid arguments */` |
|    ! 0 | 6830 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6831 | `		return PH7_OK;` |
|      - | 6832 | `	}` |
|      9 | 6833 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6834 | `		str_replace_data sRepData;` |
|      - | 6835 | `		SyBlob sWorker;` |
|      - | 6836 | `		sxi32 rc;` |
|      - | 6837 | `		/* Initilaize the working buffer */` |
|      5 | 6838 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6839 | `		/* Copy raw string */` |
|      5 | 6840 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6841 | `		/* Init our replace data instance */` |
|      5 | 6842 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6843 | `		sRepData.xMatch = SyBlobSearch;` |
|      5 | 6844 | `		sRepData.rc = SXRET_OK;` |
|      - | 6845 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6846 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      5 | 6847 | `		if( sRepData.rc != SXRET_OK ){` |
|      - | 6848 | `			/* Allocation failure during replacement: surface a fatal */` |
|    ! 0 | 6849 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 6850 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6851 | `		}` |
|      - | 6852 | `		/* All done, return the result string */` |
|      7 | 6853 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6854 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6855 | `		/* Clean-up */` |
|      5 | 6856 | `		SyBlobRelease(&sWorker);` |
|      5 | 6857 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6858 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6859 | `		}` |
|      3 | 6860 | `	}else{` |
|      - | 6861 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6862 | `		const char *zFrom,*zTo;` |
|      3 | 6863 | `		if( nArg < 3 ){` |
|      - | 6864 | `			/* Nothing to replace */` |
|    ! 0 | 6865 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6866 | `			return PH7_OK;` |
|      - | 6867 | `		}` |
|      - | 6868 | `		/* Extract given arguments */` |
|      3 | 6869 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6870 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6871 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6872 | `			/* Nothing to replace */` |
|    ! 0 | 6873 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6874 | `			return PH7_OK;` |
|      - | 6875 | `		}` |
|      - | 6876 | `		/* Start the replace process */` |
|     13 | 6877 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6878 | `			c = zIn[i];` |
|     11 | 6879 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6880 | `				if ( iOfft < tlen ){` |
|      5 | 6881 | `					c = zTo[iOfft];` |
|      2 | 6882 | `				}` |
|      2 | 6883 | `			}` |
|     11 | 6884 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6885 |  |
|      6 | 6886 | `		}` |
|      - | 6887 | `	}` |
|      7 | 6888 | `	return PH7_OK;` |
|      7 | 6889 |  |
|      - | 6890 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6891 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6892 | `/*` |
|      - | 6893 | ` * Parse an INI string.` |
|      - | 6894 |  |
|      - | 6895 | ` * According to wikipedia` |
|      - | 6896 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6897 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6898 | ` *  Format` |
|      - | 6899 | `*    Properties` |
|      - | 6900 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6901 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6902 | `*     Example:` |
|      - | 6903 | `*      name=value` |
|      - | 6904 | `*    Sections` |
|      - | 6905 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6906 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6907 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6908 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6909 | `*     Example:` |
|      - | 6910 | `*      [section]` |
|      - | 6911 | `*   Comments` |
|      - | 6912 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6913 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6914 | `*/` |
|     12 | 6915 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6916 |  |
|      - | 6917 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6918 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6919 | `	SyHashEntry *pEntry;` |
|      - | 6920 | `	SyString sEntry;` |
|      - | 6921 | `	SyHash sHash;` |
|      - | 6922 | `	int c;` |
|      - | 6923 | `	/* Create an empty array and worker variables */` |
|     13 | 6924 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6925 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6926 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6927 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6928 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 6929 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6930 | `	}` |
|     13 | 6931 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6932 | `	pCur = pArray;` |
|      - | 6933 | `	/* Start the parse process */` |
|     21 | 6934 | `	for(;;){` |
|      - | 6935 | `		/* Ignore leading white spaces */` |
|     69 | 6936 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6937 | `			zIn++;` |
|      1 | 6938 | `		}` |
|     43 | 6939 | `		if( zIn >= zEnd ){` |
|      - | 6940 | `			/* No more input to process */` |
|     13 | 6941 | `			break;` |
|      - | 6942 | `		}` |
|     31 | 6943 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6944 | `			/* Comment til the end of line */` |
|    ! 0 | 6945 | `			zIn++;` |
|    ! 0 | 6946 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6947 | `				zIn++;` |
|    ! 0 | 6948 | `			}` |
|    ! 0 | 6949 | `			continue;` |
|      - | 6950 | `		}` |
|      - | 6951 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6952 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6953 | `		if( zIn[0] == '[' ){` |
|      - | 6954 | `			/* Section: Extract the section name */` |
|      9 | 6955 | `			zIn++;` |
|      9 | 6956 | `			zCur = zIn;` |
|     73 | 6957 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6958 | `				zIn++;` |
|      1 | 6959 | `			}` |
|      9 | 6960 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6961 | `				/* Save the section name */` |
|      5 | 6962 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6963 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6964 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6965 | `				if( sEntry.nByte > 0 ){` |
|      - | 6966 | `					/* Associate an array with the section */` |
|      5 | 6967 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6968 | `					if( pSection ){` |
|      5 | 6969 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6970 | `						pCur = pSection;` |
|      2 | 6971 | `					}` |
|      2 | 6972 | `				}` |
|      2 | 6973 | `			}` |
|      9 | 6974 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6975 | `		}else{` |
|      - | 6976 | `			ph7_value *pOldCur;` |
|      - | 6977 | `			int is_array;` |
|      - | 6978 | `			int iLen;` |
|      - | 6979 | `			/* Properties */` |
|     23 | 6980 | `			is_array = 0;` |
|     23 | 6981 | `			zCur = zIn;` |
|     23 | 6982 | `			iLen = 0; /* cc warning */` |
|     23 | 6983 | `			pOldCur = pCur;` |
|    155 | 6984 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6985 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6986 | `					/* Array */` |
|    ! 0 | 6987 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6988 | `					is_array = 1;` |
|    ! 0 | 6989 | `					if( iLen > 0 ){` |
|    ! 0 | 6990 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6991 | `						/* Query the hashtable */` |
|    ! 0 | 6992 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6993 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6994 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6995 | `						if( pEntry ){` |
|    ! 0 | 6996 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6997 | `						}else{` |
|      - | 6998 | `							/* Create an empty array */` |
|    ! 0 | 6999 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 7000 | `							if( pvArr ){` |
|      - | 7001 | `								/* Save the entry */` |
|    ! 0 | 7002 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 7003 | `								/* Insert the entry */` |
|    ! 0 | 7004 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7005 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 7006 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 7007 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7008 | `							}` |
|      - | 7009 | `						}` |
|    ! 0 | 7010 | `						if( pvArr ){` |
|    ! 0 | 7011 | `							pCur = pvArr;` |
|    ! 0 | 7012 | `						}` |
|    ! 0 | 7013 | `					}` |
|    ! 0 | 7014 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 7015 | `						zIn++;` |
|    ! 0 | 7016 | `					}` |
|    ! 0 | 7017 | `				}` |
|    133 | 7018 | `				zIn++;` |
|      1 | 7019 | `			}` |
|     23 | 7020 | `			if( !is_array ){` |
|     23 | 7021 | `				iLen = (int)(zIn-zCur);` |
|     11 | 7022 | `			}` |
|      - | 7023 | `			/* Trim the key */` |
|     23 | 7024 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 7025 | `			SyStringFullTrim(&sEntry);` |
|     23 | 7026 | `			if( sEntry.nByte > 0 ){` |
|     23 | 7027 | `				if( !is_array ){` |
|      - | 7028 | `					/* Save the key name */` |
|     23 | 7029 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 7030 | `				}` |
|      - | 7031 | `				/* extract key value */` |
|     23 | 7032 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7033 | `				zIn++; /* '=' */` |
|     39 | 7034 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7035 | `					zIn++;` |
|      1 | 7036 | `				}` |
|     23 | 7037 | `				if( zIn < zEnd ){` |
|     21 | 7038 | `					zCur = zIn;` |
|     21 | 7039 | `					c = zIn[0];` |
|     21 | 7040 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7041 | `						zIn++;` |
|      - | 7042 | `						/* Delimit the value */` |
|    ! 0 | 7043 | `						while( zIn < zEnd ){` |
|    ! 0 | 7044 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7045 | `								break;` |
|      - | 7046 | `							}` |
|    ! 0 | 7047 | `							zIn++;` |
|    ! 0 | 7048 | `						}` |
|    ! 0 | 7049 | `						if( zIn < zEnd ){` |
|    ! 0 | 7050 | `							zIn++;` |
|    ! 0 | 7051 | `						}` |
|    ! 0 | 7052 | `					}else{` |
|    125 | 7053 | `						while( zIn < zEnd ){` |
|    123 | 7054 | `							if( zIn[0] == '\n' ){` |
|     19 | 7055 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7056 | `									break;` |
|    ! 0 | 7057 | `								}` |
|    105 | 7058 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7059 | `								/* Inline comments */` |
|    ! 0 | 7060 | `								break;` |
|      - | 7061 | `							}` |
|    105 | 7062 | `							zIn++;` |
|      1 | 7063 | `						}` |
|      - | 7064 | `					}` |
|      - | 7065 | `					/* Trim the value */` |
|     21 | 7066 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7067 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7068 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7069 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7070 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7071 | `					}` |
|     21 | 7072 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7073 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7074 | `					}` |
|      - | 7075 | `					/* Insert the key and it's value */` |
|     21 | 7076 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7077 | `				}` |
|     12 | 7078 | `			}else{` |
|    ! 0 | 7079 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7080 | `					zIn++;` |
|    ! 0 | 7081 | `				}` |
|      - | 7082 | `			}` |
|     23 | 7083 | `			pCur = pOldCur;` |
|      - | 7084 | `		}` |
|      1 | 7085 | `	}` |
|     13 | 7086 | `	SyHashRelease(&sHash);` |
|      - | 7087 | `	/* Return the parse of the INI string */` |
|     13 | 7088 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7089 | `	return SXRET_OK;` |
|      7 | 7090 |  |
|      - | 7091 | `/*` |
|      - | 7092 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7093 | ` *  Parse a configuration string.` |
|      - | 7094 | ` * Parameters` |
|      - | 7095 | ` *  $ini` |
|      - | 7096 | ` *   The contents of the ini file being parsed.` |
|      - | 7097 | ` *  $process_sections` |
|      - | 7098 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7099 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7100 | ` *  $scanner_mode (Not used)` |
|      - | 7101 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7102 | ` *   then option values will not be parsed.` |
|      - | 7103 | ` * Return` |
|      - | 7104 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7105 | ` */` |
|     10 | 7106 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7107 |  |
|      - | 7108 | `	const char *zIni;` |
|      - | 7109 | `	int nByte;` |
|     11 | 7110 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7111 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7112 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7113 | `		return PH7_OK;` |
|      - | 7114 | `	}` |
|      - | 7115 | `	/* Extract the raw INI buffer */` |
|     11 | 7116 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7117 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7118 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7119 |  |
|      - | 7120 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7121 |  |
|      - | 7122 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7123 |  |
|      - | 7124 | `/*` |
|      - | 7125 | ` * Ctype Functions.` |
|      - | 7126 | ` * Status:` |
|      - | 7127 | ` *    Stable.` |
|      - | 7128 | ` */` |
|      - | 7129 | `/*` |
|      - | 7130 | ` * bool ctype_alnum(string $text)` |
|      - | 7131 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7132 | ` * Parameters` |
|      - | 7133 | ` *  $text` |
|      - | 7134 | ` *   The tested string.` |
|      - | 7135 | ` * Return` |
|      - | 7136 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7137 | ` */` |
|     16 | 7138 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7139 |  |
|      - | 7140 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7141 | `	int nLen;` |
|     17 | 7142 | `	if( nArg < 1 ){` |
|      - | 7143 | `		/* Missing arguments,return FALSE */` |
|      3 | 7144 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7145 | `		return PH7_OK;` |
|      - | 7146 | `	}` |
|      - | 7147 | `	/* Extract the target string */` |
|     15 | 7148 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7149 | `	zEnd = &zIn[nLen];` |
|     15 | 7150 | `	if( nLen < 1 ){` |
|      - | 7151 | `		/* Empty string,return FALSE */` |
|      3 | 7152 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7153 | `		return PH7_OK;` |
|      - | 7154 | `	}` |
|      - | 7155 | `	/* Perform the requested operation */` |
|     32 | 7156 | `	for(;;){` |
|     65 | 7157 | `		if( zIn >= zEnd ){` |
|      - | 7158 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7159 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7160 | `			return PH7_OK;` |
|      - | 7161 | `		}` |
|     57 | 7162 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7163 | `			break;` |
|      - | 7164 | `		}` |
|      - | 7165 | `		/* Point to the next character */` |
|     53 | 7166 | `		zIn++;` |
|      1 | 7167 | `	}` |
|      - | 7168 | `	/* The test failed,return FALSE */` |
|      5 | 7169 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7170 | `	return PH7_OK;` |
|      9 | 7171 |  |
|      - | 7172 | `/*` |
|      - | 7173 | ` * bool ctype_alpha(string $text)` |
|      - | 7174 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7175 | ` * Parameters` |
|      - | 7176 | ` *  $text` |
|      - | 7177 | ` *   The tested string.` |
|      - | 7178 | ` * Return` |
|      - | 7179 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7180 | ` */` |
|     18 | 7181 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7182 |  |
|      - | 7183 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7184 | `	int nLen;` |
|     19 | 7185 | `	if( nArg < 1 ){` |
|      - | 7186 | `		/* Missing arguments,return FALSE */` |
|      3 | 7187 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7188 | `		return PH7_OK;` |
|      - | 7189 | `	}` |
|      - | 7190 | `	/* Extract the target string */` |
|     17 | 7191 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7192 | `	zEnd = &zIn[nLen];` |
|     17 | 7193 | `	if( nLen < 1 ){` |
|      - | 7194 | `		/* Empty string,return FALSE */` |
|      3 | 7195 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7196 | `		return PH7_OK;` |
|      - | 7197 | `	}` |
|      - | 7198 | `	/* Perform the requested operation */` |
|     42 | 7199 | `	for(;;){` |
|     85 | 7200 | `		if( zIn >= zEnd ){` |
|      - | 7201 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7202 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7203 | `			return PH7_OK;` |
|      - | 7204 | `		}` |
|     77 | 7205 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7206 | `			break;` |
|      - | 7207 | `		}` |
|      - | 7208 | `		/* Point to the next character */` |
|     71 | 7209 | `		zIn++;` |
|      1 | 7210 | `	}` |
|      - | 7211 | `	/* The test failed,return FALSE */` |
|      7 | 7212 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7213 | `	return PH7_OK;` |
|     10 | 7214 |  |
|      - | 7215 | `/*` |
|      - | 7216 | ` * bool ctype_cntrl(string $text)` |
|      - | 7217 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7218 | ` * Parameters` |
|      - | 7219 | ` *  $text` |
|      - | 7220 | ` *   The tested string.` |
|      - | 7221 | ` * Return` |
|      - | 7222 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7223 | ` */` |
|     18 | 7224 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7225 |  |
|      - | 7226 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7227 | `	int nLen;` |
|     19 | 7228 | `	if( nArg < 1 ){` |
|      - | 7229 | `		/* Missing arguments,return FALSE */` |
|      3 | 7230 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7231 | `		return PH7_OK;` |
|      - | 7232 | `	}` |
|      - | 7233 | `	/* Extract the target string */` |
|     17 | 7234 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7235 | `	zEnd = &zIn[nLen];` |
|     17 | 7236 | `	if( nLen < 1 ){` |
|      - | 7237 | `		/* Empty string,return FALSE */` |
|      3 | 7238 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7239 | `		return PH7_OK;` |
|      - | 7240 | `	}` |
|      - | 7241 | `	/* Perform the requested operation */` |
|     14 | 7242 | `	for(;;){` |
|     29 | 7243 | `		if( zIn >= zEnd ){` |
|      - | 7244 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7245 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7246 | `			return PH7_OK;` |
|      - | 7247 | `		}` |
|     21 | 7248 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7249 | `			/* UTF-8 stream  */` |
|    ! 0 | 7250 | `			break;` |
|      - | 7251 | `		}` |
|     21 | 7252 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7253 | `			break;` |
|      - | 7254 | `		}` |
|      - | 7255 | `		/* Point to the next character */` |
|     15 | 7256 | `		zIn++;` |
|      1 | 7257 | `	}` |
|      - | 7258 | `	/* The test failed,return FALSE */` |
|      7 | 7259 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7260 | `	return PH7_OK;` |
|     10 | 7261 |  |
|      - | 7262 | `/*` |
|      - | 7263 | ` * bool ctype_digit(string $text)` |
|      - | 7264 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7265 | ` * Parameters` |
|      - | 7266 | ` *  $text` |
|      - | 7267 | ` *   The tested string.` |
|      - | 7268 | ` * Return` |
|      - | 7269 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7270 | ` */` |
|   1638 | 7271 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7272 |  |
|      - | 7273 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7274 | `	int nLen;` |
|   1643 | 7275 | `	if( nArg < 1 ){` |
|      - | 7276 | `		/* Missing arguments,return FALSE */` |
|      3 | 7277 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7278 | `		return PH7_OK;` |
|      - | 7279 | `	}` |
|      - | 7280 | `	/* Extract the target string */` |
|   1641 | 7281 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1641 | 7282 | `	zEnd = &zIn[nLen];` |
|   1641 | 7283 | `	if( nLen < 1 ){` |
|      - | 7284 | `		/* Empty string,return FALSE */` |
|      3 | 7285 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7286 | `		return PH7_OK;` |
|      - | 7287 | `	}` |
|      - | 7288 | `	/* Perform the requested operation */` |
|   1538 | 7289 | `	for(;;){` |
|   3081 | 7290 | `		if( zIn >= zEnd ){` |
|      - | 7291 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1397 | 7292 | `			ph7_result_bool(pCtx,1);` |
|   1397 | 7293 | `			return PH7_OK;` |
|      - | 7294 | `		}` |
|   1689 | 7295 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7296 | `			/* UTF-8 stream  */` |
|    ! 0 | 7297 | `			break;` |
|      - | 7298 | `		}` |
|   1689 | 7299 | `		if( !SyisDigit(zIn[0]) ){` |
|    247 | 7300 | `			break;` |
|      - | 7301 | `		}` |
|      - | 7302 | `		/* Point to the next character */` |
|   1447 | 7303 | `		zIn++;` |
|      5 | 7304 | `	}` |
|      - | 7305 | `	/* The test failed,return FALSE */` |
|    247 | 7306 | `	ph7_result_bool(pCtx,0);` |
|    247 | 7307 | `	return PH7_OK;` |
|    824 | 7308 |  |
|      - | 7309 | `/*` |
|      - | 7310 | ` * bool ctype_xdigit(string $text)` |
|      - | 7311 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7312 | ` * Parameters` |
|      - | 7313 | ` *  $text` |
|      - | 7314 | ` *   The tested string.` |
|      - | 7315 | ` * Return` |
|      - | 7316 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7317 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7318 | ` */` |
|     20 | 7319 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7320 |  |
|      - | 7321 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7322 | `	int nLen;` |
|     21 | 7323 | `	if( nArg < 1 ){` |
|      - | 7324 | `		/* Missing arguments,return FALSE */` |
|      3 | 7325 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7326 | `		return PH7_OK;` |
|      - | 7327 | `	}` |
|      - | 7328 | `	/* Extract the target string */` |
|     19 | 7329 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7330 | `	zEnd = &zIn[nLen];` |
|     19 | 7331 | `	if( nLen < 1 ){` |
|      - | 7332 | `		/* Empty string,return FALSE */` |
|      3 | 7333 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7334 | `		return PH7_OK;` |
|      - | 7335 | `	}` |
|      - | 7336 | `	/* Perform the requested operation */` |
|     46 | 7337 | `	for(;;){` |
|     93 | 7338 | `		if( zIn >= zEnd ){` |
|      - | 7339 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7340 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7341 | `			return PH7_OK;` |
|      - | 7342 | `		}` |
|     83 | 7343 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7344 | `			/* UTF-8 stream  */` |
|    ! 0 | 7345 | `			break;` |
|      - | 7346 | `		}` |
|     83 | 7347 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7348 | `			break;` |
|      - | 7349 | `		}` |
|      - | 7350 | `		/* Point to the next character */` |
|     77 | 7351 | `		zIn++;` |
|      1 | 7352 | `	}` |
|      - | 7353 | `	/* The test failed,return FALSE */` |
|      7 | 7354 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7355 | `	return PH7_OK;` |
|     11 | 7356 |  |
|      - | 7357 | `/*` |
|      - | 7358 | ` * bool ctype_graph(string $text)` |
|      - | 7359 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7360 | ` * Parameters` |
|      - | 7361 | ` *  $text` |
|      - | 7362 | ` *   The tested string.` |
|      - | 7363 | ` * Return` |
|      - | 7364 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7365 | ` * (no white space), FALSE otherwise.` |
|      - | 7366 | ` */` |
|     18 | 7367 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7368 |  |
|      - | 7369 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7370 | `	int nLen;` |
|     19 | 7371 | `	if( nArg < 1 ){` |
|      - | 7372 | `		/* Missing arguments,return FALSE */` |
|      3 | 7373 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7374 | `		return PH7_OK;` |
|      - | 7375 | `	}` |
|      - | 7376 | `	/* Extract the target string */` |
|     17 | 7377 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7378 | `	zEnd = &zIn[nLen];` |
|     17 | 7379 | `	if( nLen < 1 ){` |
|      - | 7380 | `		/* Empty string,return FALSE */` |
|      3 | 7381 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7382 | `		return PH7_OK;` |
|      - | 7383 | `	}` |
|      - | 7384 | `	/* Perform the requested operation */` |
|     57 | 7385 | `	for(;;){` |
|    115 | 7386 | `		if( zIn >= zEnd ){` |
|      - | 7387 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7388 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7389 | `			return PH7_OK;` |
|      - | 7390 | `		}` |
|    107 | 7391 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7392 | `			/* UTF-8 stream  */` |
|    ! 0 | 7393 | `			break;` |
|      - | 7394 | `		}` |
|    107 | 7395 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7396 | `			break;` |
|      - | 7397 | `		}` |
|      - | 7398 | `		/* Point to the next character */` |
|    101 | 7399 | `		zIn++;` |
|      1 | 7400 | `	}` |
|      - | 7401 | `	/* The test failed,return FALSE */` |
|      7 | 7402 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7403 | `	return PH7_OK;` |
|     10 | 7404 |  |
|      - | 7405 | `/*` |
|      - | 7406 | ` * bool ctype_print(string $text)` |
|      - | 7407 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7408 | ` * Parameters` |
|      - | 7409 | ` *  $text` |
|      - | 7410 | ` *   The tested string.` |
|      - | 7411 | ` * Return` |
|      - | 7412 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7413 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7414 | ` *  or control function at all.` |
|      - | 7415 | ` */` |
|     18 | 7416 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7417 |  |
|      - | 7418 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7419 | `	int nLen;` |
|     19 | 7420 | `	if( nArg < 1 ){` |
|      - | 7421 | `		/* Missing arguments,return FALSE */` |
|      3 | 7422 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7423 | `		return PH7_OK;` |
|      - | 7424 | `	}` |
|      - | 7425 | `	/* Extract the target string */` |
|     17 | 7426 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7427 | `	zEnd = &zIn[nLen];` |
|     17 | 7428 | `	if( nLen < 1 ){` |
|      - | 7429 | `		/* Empty string,return FALSE */` |
|      3 | 7430 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7431 | `		return PH7_OK;` |
|      - | 7432 | `	}` |
|      - | 7433 | `	/* Perform the requested operation */` |
|     63 | 7434 | `	for(;;){` |
|    127 | 7435 | `		if( zIn >= zEnd ){` |
|      - | 7436 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7437 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7438 | `			return PH7_OK;` |
|      - | 7439 | `		}` |
|    119 | 7440 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7441 | `			/* UTF-8 stream  */` |
|    ! 0 | 7442 | `			break;` |
|      - | 7443 | `		}` |
|    119 | 7444 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7445 | `			break;` |
|      - | 7446 | `		}` |
|      - | 7447 | `		/* Point to the next character */` |
|    113 | 7448 | `		zIn++;` |
|      1 | 7449 | `	}` |
|      - | 7450 | `	/* The test failed,return FALSE */` |
|      7 | 7451 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7452 | `	return PH7_OK;` |
|     10 | 7453 |  |
|      - | 7454 | `/*` |
|      - | 7455 | ` * bool ctype_punct(string $text)` |
|      - | 7456 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7457 | ` * Parameters` |
|      - | 7458 | ` *  $text` |
|      - | 7459 | ` *   The tested string.` |
|      - | 7460 | ` * Return` |
|      - | 7461 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7462 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7463 | ` */` |
|     20 | 7464 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7465 |  |
|      - | 7466 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7467 | `	int nLen;` |
|     21 | 7468 | `	if( nArg < 1 ){` |
|      - | 7469 | `		/* Missing arguments,return FALSE */` |
|      3 | 7470 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7471 | `		return PH7_OK;` |
|      - | 7472 | `	}` |
|      - | 7473 | `	/* Extract the target string */` |
|     19 | 7474 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7475 | `	zEnd = &zIn[nLen];` |
|     19 | 7476 | `	if( nLen < 1 ){` |
|      - | 7477 | `		/* Empty string,return FALSE */` |
|      3 | 7478 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7479 | `		return PH7_OK;` |
|      - | 7480 | `	}` |
|      - | 7481 | `	/* Perform the requested operation */` |
|     38 | 7482 | `	for(;;){` |
|     77 | 7483 | `		if( zIn >= zEnd ){` |
|      - | 7484 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7485 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7486 | `			return PH7_OK;` |
|      - | 7487 | `		}` |
|     69 | 7488 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7489 | `			/* UTF-8 stream  */` |
|    ! 0 | 7490 | `			break;` |
|      - | 7491 | `		}` |
|     69 | 7492 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7493 | `			break;` |
|      - | 7494 | `		}` |
|      - | 7495 | `		/* Point to the next character */` |
|     61 | 7496 | `		zIn++;` |
|      1 | 7497 | `	}` |
|      - | 7498 | `	/* The test failed,return FALSE */` |
|      9 | 7499 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7500 | `	return PH7_OK;` |
|     11 | 7501 |  |
|      - | 7502 | `/*` |
|      - | 7503 | ` * bool ctype_space(string $text)` |
|      - | 7504 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7505 | ` * Parameters` |
|      - | 7506 | ` *  $text` |
|      - | 7507 | ` *   The tested string.` |
|      - | 7508 | ` * Return` |
|      - | 7509 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7510 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7511 | ` *  and form feed characters.` |
|      - | 7512 | ` */` |
|  60787 | 7513 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7514 |  |
|      - | 7515 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7516 | `	int nLen;` |
|  60792 | 7517 | `	if( nArg < 1 ){` |
|      - | 7518 | `		/* Missing arguments,return FALSE */` |
|      3 | 7519 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7520 | `		return PH7_OK;` |
|      - | 7521 | `	}` |
|      - | 7522 | `	/* Extract the target string */` |
|  60790 | 7523 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  60790 | 7524 | `	zEnd = &zIn[nLen];` |
|  60790 | 7525 | `	if( nLen < 1 ){` |
|      - | 7526 | `		/* Empty string,return FALSE */` |
|      3 | 7527 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7528 | `		return PH7_OK;` |
|      - | 7529 | `	}` |
|      - | 7530 | `	/* Perform the requested operation */` |
|  31480 | 7531 | `	for(;;){` |
|  62880 | 7532 | `		if( zIn >= zEnd ){` |
|      - | 7533 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2073 | 7534 | `			ph7_result_bool(pCtx,1);` |
|   2073 | 7535 | `			return PH7_OK;` |
|      - | 7536 | `		}` |
|  60812 | 7537 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7538 | `			/* UTF-8 stream  */` |
|    ! 0 | 7539 | `			break;` |
|      - | 7540 | `		}` |
|  60812 | 7541 | `		if( !SyisSpace(zIn[0]) ){` |
|  58720 | 7542 | `			break;` |
|      - | 7543 | `		}` |
|      - | 7544 | `		/* Point to the next character */` |
|   2097 | 7545 | `		zIn++;` |
|      5 | 7546 | `	}` |
|      - | 7547 | `	/* The test failed,return FALSE */` |
|  58720 | 7548 | `	ph7_result_bool(pCtx,0);` |
|  58720 | 7549 | `	return PH7_OK;` |
|  30441 | 7550 |  |
|      - | 7551 | `/*` |
|      - | 7552 | ` * bool ctype_lower(string $text)` |
|      - | 7553 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7554 | ` * Parameters` |
|      - | 7555 | ` *  $text` |
|      - | 7556 | ` *   The tested string.` |
|      - | 7557 | ` * Return` |
|      - | 7558 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7559 | ` */` |
|     18 | 7560 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7561 |  |
|      - | 7562 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7563 | `	int nLen;` |
|     19 | 7564 | `	if( nArg < 1 ){` |
|      - | 7565 | `		/* Missing arguments,return FALSE */` |
|      3 | 7566 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7567 | `		return PH7_OK;` |
|      - | 7568 | `	}` |
|      - | 7569 | `	/* Extract the target string */` |
|     17 | 7570 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7571 | `	zEnd = &zIn[nLen];` |
|     17 | 7572 | `	if( nLen < 1 ){` |
|      - | 7573 | `		/* Empty string,return FALSE */` |
|      3 | 7574 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7575 | `		return PH7_OK;` |
|      - | 7576 | `	}` |
|      - | 7577 | `	/* Perform the requested operation */` |
|     27 | 7578 | `	for(;;){` |
|     55 | 7579 | `		if( zIn >= zEnd ){` |
|      - | 7580 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7581 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7582 | `			return PH7_OK;` |
|      - | 7583 | `		}` |
|     51 | 7584 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7585 | `			break;` |
|      - | 7586 | `		}` |
|      - | 7587 | `		/* Point to the next character */` |
|     41 | 7588 | `		zIn++;` |
|      1 | 7589 | `	}` |
|      - | 7590 | `	/* The test failed,return FALSE */` |
|     11 | 7591 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7592 | `	return PH7_OK;` |
|     10 | 7593 |  |
|      - | 7594 | `/*` |
|      - | 7595 | ` * bool ctype_upper(string $text)` |
|      - | 7596 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7597 | ` * Parameters` |
|      - | 7598 | ` *  $text` |
|      - | 7599 | ` *   The tested string.` |
|      - | 7600 | ` * Return` |
|      - | 7601 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7602 | ` */` |
|     18 | 7603 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7604 |  |
|      - | 7605 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7606 | `	int nLen;` |
|     19 | 7607 | `	if( nArg < 1 ){` |
|      - | 7608 | `		/* Missing arguments,return FALSE */` |
|      3 | 7609 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7610 | `		return PH7_OK;` |
|      - | 7611 | `	}` |
|      - | 7612 | `	/* Extract the target string */` |
|     17 | 7613 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7614 | `	zEnd = &zIn[nLen];` |
|     17 | 7615 | `	if( nLen < 1 ){` |
|      - | 7616 | `		/* Empty string,return FALSE */` |
|      3 | 7617 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7618 | `		return PH7_OK;` |
|      - | 7619 | `	}` |
|      - | 7620 | `	/* Perform the requested operation */` |
|     28 | 7621 | `	for(;;){` |
|     57 | 7622 | `		if( zIn >= zEnd ){` |
|      - | 7623 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7624 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7625 | `			return PH7_OK;` |
|      - | 7626 | `		}` |
|     53 | 7627 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7628 | `			break;` |
|      - | 7629 | `		}` |
|      - | 7630 | `		/* Point to the next character */` |
|     43 | 7631 | `		zIn++;` |
|      1 | 7632 | `	}` |
|      - | 7633 | `	/* The test failed,return FALSE */` |
|     11 | 7634 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7635 | `	return PH7_OK;` |
|     10 | 7636 |  |
|      - | 7637 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 7638 | `/*` |
|      - | 7639 | ` * Section:` |
|      - | 7640 | ` *    URL handling Functions.` |
|      - | 7641 | ` * Status:` |
|      - | 7642 | ` *    Stable.` |
|      - | 7643 | ` */` |
|      - | 7644 | `/*` |
|      - | 7645 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 7646 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 7647 | ` */` |
|   1026 | 7648 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7649 |  |
|      - | 7650 | `	/* Store in the call context result buffer */` |
|   1028 | 7651 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 7652 | `	return SXRET_OK;` |
|      2 | 7653 |  |
|      - | 7654 | `/*` |
|      - | 7655 | ` * string base64_encode(string $data)` |
|      - | 7656 | ` * string convert_uuencode(string $data)` |
|      - | 7657 | ` *  Encodes data with MIME base64` |
|      - | 7658 | ` * Parameter` |
|      - | 7659 | ` *  $data` |
|      - | 7660 | ` *    Data to encode` |
|      - | 7661 | ` * Return` |
|      - | 7662 | ` *  Encoded data or FALSE on failure.` |
|      - | 7663 | ` */` |
|     10 | 7664 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7665 |  |
|      - | 7666 | `	const char *zIn;` |
|      - | 7667 | `	int nLen;` |
|     11 | 7668 | `	if( nArg < 1 ){` |
|      - | 7669 | `		/* Missing arguments,return FALSE */` |
|      5 | 7670 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7671 | `		return PH7_OK;` |
|      - | 7672 | `	}` |
|      - | 7673 | `	/* Extract the input string */` |
|      7 | 7674 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7675 | `	if( nLen < 1 ){` |
|      - | 7676 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7677 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7678 | `		return PH7_OK;` |
|      - | 7679 | `	}` |
|      - | 7680 | `	/* Perform the BASE64 encoding */` |
|      7 | 7681 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 7682 | `	return PH7_OK;` |
|      6 | 7683 |  |
|      - | 7684 | `/*` |
|      - | 7685 | ` * string base64_decode(string $data)` |
|      - | 7686 | ` * string convert_uudecode(string $data)` |
|      - | 7687 | ` *  Decodes data encoded with MIME base64` |
|      - | 7688 | ` * Parameter` |
|      - | 7689 | ` *  $data` |
|      - | 7690 | ` *    Encoded data.` |
|      - | 7691 | ` * Return` |
|      - | 7692 | ` *  Returns the original data or FALSE on failure.` |
|      - | 7693 | ` */` |
|     36 | 7694 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7695 |  |
|      - | 7696 | `	const char *zIn;` |
|      - | 7697 | `	int nLen;` |
|     38 | 7698 | `	if( nArg < 1 ){` |
|      - | 7699 | `		/* Missing arguments,return FALSE */` |
|      3 | 7700 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7701 | `		return PH7_OK;` |
|      - | 7702 | `	}` |
|      - | 7703 | `	/* Extract the input string */` |
|     36 | 7704 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 7705 | `	if( nLen < 1 ){` |
|      - | 7706 | `		/* Nothing to process,return FALSE */` |
|      3 | 7707 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7708 | `		return PH7_OK;` |
|      - | 7709 | `	}` |
|      - | 7710 | `	/* Perform the BASE64 decoding */` |
|     34 | 7711 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 7712 | `	return PH7_OK;` |
|     20 | 7713 |  |
|      - | 7714 | `/*` |
|      - | 7715 | ` * string urlencode(string $str)` |
|      - | 7716 | ` *  URL encoding` |
|      - | 7717 | ` * Parameter` |
|      - | 7718 | ` *  $data` |
|      - | 7719 | ` *   Input string.` |
|      - | 7720 | ` * Return` |
|      - | 7721 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 7722 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 7723 | ` *  encoded as plus (+) signs.` |
|      - | 7724 | ` */` |
|      6 | 7725 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7726 |  |
|      - | 7727 | `	const char *zIn;` |
|      - | 7728 | `	int nLen;` |
|      7 | 7729 | `	if( nArg < 1 ){` |
|      - | 7730 | `		/* Missing arguments,return FALSE */` |
|      3 | 7731 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7732 | `		return PH7_OK;` |
|      - | 7733 | `	}` |
|      - | 7734 | `	/* Extract the input string */` |
|      5 | 7735 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 7736 | `	if( nLen < 1 ){` |
|      - | 7737 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7738 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7739 | `		return PH7_OK;` |
|      - | 7740 | `	}` |
|      - | 7741 | `	/* Perform the URL encoding */` |
|      5 | 7742 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 7743 | `	return PH7_OK;` |
|      4 | 7744 |  |
|      - | 7745 | `/*` |
|      - | 7746 | ` * string urldecode(string $str)` |
|      - | 7747 | ` *  Decodes any %## encoding in the given string.` |
|      - | 7748 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 7749 | ` * Parameter` |
|      - | 7750 | ` *  $data` |
|      - | 7751 | ` *    Input string.` |
|      - | 7752 | ` * Return` |
|      - | 7753 | ` *  Decoded URL or FALSE on failure.` |
|      - | 7754 | ` */` |
|      8 | 7755 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7756 |  |
|      - | 7757 | `	const char *zIn;` |
|      - | 7758 | `	int nLen;` |
|      9 | 7759 | `	if( nArg < 1 ){` |
|      - | 7760 | `		/* Missing arguments,return FALSE */` |
|      3 | 7761 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7762 | `		return PH7_OK;` |
|      - | 7763 | `	}` |
|      - | 7764 | `	/* Extract the input string */` |
|      7 | 7765 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7766 | `	if( nLen < 1 ){` |
|      - | 7767 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7768 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7769 | `		return PH7_OK;` |
|      - | 7770 | `	}` |
|      - | 7771 | `	/* Perform the URL decoding */` |
|      7 | 7772 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 7773 | `	return PH7_OK;` |
|      5 | 7774 |  |
|      - | 7775 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7776 | `/* Table of the built-in functions */` |
|      - | 7777 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 7778 | `	   /* Variable handling functions */` |
|      - | 7779 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 7780 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 7781 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 7782 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 7783 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 7784 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 7785 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 7786 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 7787 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 7788 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 7789 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 7790 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 7791 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 7792 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 7793 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 7794 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 7795 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 7796 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 7797 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 7798 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 7799 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7800 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 7801 | `	   /* Math functions */` |
|      - | 7802 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 7803 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 7804 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 7805 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 7806 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 7807 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 7808 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 7809 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 7810 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 7811 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 7812 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 7813 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 7814 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 7815 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 7816 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 7817 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 7818 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 7819 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 7820 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 7821 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 7822 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 7823 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 7824 | `	{ "round",    PH7_builtin_round        },` |
|      - | 7825 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 7826 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 7827 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 7828 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 7829 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 7830 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 7831 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 7832 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 7833 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 7834 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7835 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7836 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 7837 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7838 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7839 | `	   /* String handling functions */` |
|      - | 7840 |  |
|      - | 7841 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 7842 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 7843 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 7844 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 7845 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 7846 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 7847 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 7848 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 7849 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 7850 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 7851 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 7852 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 7853 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 7854 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 7855 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 7856 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 7857 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 7858 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 7859 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 7860 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 7861 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 7862 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 7863 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 7864 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 7865 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 7866 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 7867 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 7868 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 7869 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 7870 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 7871 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 7872 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 7873 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 7874 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 7875 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 7876 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 7877 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 7878 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 7879 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 7880 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 7881 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 7882 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 7883 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 7884 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 7885 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 7886 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 7887 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 7888 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 7889 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 7890 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 7891 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 7892 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 7893 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7894 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7895 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 7896 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 7897 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 7898 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 7899 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7900 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7901 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 7902 |  |
|      - | 7903 |  |
|      - | 7904 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 7905 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 7906 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 7907 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 7908 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 7909 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 7910 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 7911 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 7912 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7913 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 7914 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 7915 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 7916 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 7917 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 7918 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7919 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7920 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 7921 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 7922 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7923 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7924 |  |
|      - | 7925 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 7926 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 7927 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 7928 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 7929 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 7930 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 7931 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 7932 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 7933 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 7934 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 7935 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 7936 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 7937 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7938 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7939 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 7940 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7941 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7942 |  |
|      - | 7943 | `	         /* Ctype functions */` |
|      - | 7944 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 7945 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 7946 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 7947 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 7948 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 7949 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 7950 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 7951 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 7952 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 7953 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 7954 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 7955 | `	         /* Time functions */` |
|      - | 7956 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 7957 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 7958 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 7959 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 7960 | `	{ "date",        PH7_builtin_date         },` |
|      - | 7961 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 7962 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 7963 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 7964 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 7965 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 7966 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 7967 | `	        /* URL functions */` |
|      - | 7968 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 7969 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 7970 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 7971 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 7972 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 7973 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 7974 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 7975 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 7976 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7977 | `};` |
|      - | 7978 | `/*` |
|      - | 7979 | ` * Register the built-in functions defined above,the array functions` |
|      - | 7980 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 7981 | ` */` |
|   3210 | 7982 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 7983 |  |
|      - | 7984 | `	sxu32 n;` |
| 536075 | 7985 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 532865 | 7986 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 266435 | 7987 | `	}` |
|      - | 7988 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3215 | 7989 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 7990 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3215 | 7991 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3215 | 7992 |  |
|      - | 7993 |  |
