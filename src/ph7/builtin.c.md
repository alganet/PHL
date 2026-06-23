# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3169/3593 lines (88.20%)

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
|    112 |   42 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   43 |  |
|    113 |   44 | `	int res = 0; /* Assume false by default */` |
|    113 |   45 | `	if( nArg > 0 ){` |
|    111 |   46 | `		res = ph7_value_is_float(apArg[0]);` |
|     55 |   47 | `	}` |
|      - |   48 | `	/* Query result */` |
|    113 |   49 | `	ph7_result_bool(pCtx,res);` |
|    113 |   50 | `	return PH7_OK;` |
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
|    514 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   63 |  |
|    516 |   64 | `	int res = 0; /* Assume false by default */` |
|    516 |   65 | `	if( nArg > 0 ){` |
|      - |   66 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   67 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   68 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    514 |   69 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    256 |   70 | `	}` |
|      - |   71 | `	/* Query result */` |
|    516 |   72 | `	ph7_result_bool(pCtx,res);` |
|    516 |   73 | `	return PH7_OK;` |
|      2 |   74 |  |
|      - |   75 | `/*` |
|      - |   76 | ` * bool is_string($var)` |
|      - |   77 | ` *  Finds out whether a variable is a string.` |
|      - |   78 | ` * Parameters` |
|      - |   79 | ` *   $var: The variable being evaluated.` |
|      - |   80 | ` * Return` |
|      - |   81 | ` *  TRUE if var is string. False otherwise.` |
|      - |   82 | ` */` |
|    124 |   83 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   84 |  |
|    125 |   85 | `	int res = 0; /* Assume false by default */` |
|    125 |   86 | `	if( nArg > 0 ){` |
|    123 |   87 | `		res = ph7_value_is_string(apArg[0]);` |
|     61 |   88 | `	}` |
|      - |   89 | `	/* Query result */` |
|    125 |   90 | `	ph7_result_bool(pCtx,res);` |
|    125 |   91 | `	return PH7_OK;` |
|      1 |   92 |  |
|      - |   93 | `/*` |
|      - |   94 | ` * bool is_null($var)` |
|      - |   95 | ` *  Finds out whether a variable is NULL.` |
|      - |   96 | ` * Parameters` |
|      - |   97 | ` *   $var: The variable being evaluated.` |
|      - |   98 | ` * Return` |
|      - |   99 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  100 | ` */` |
|     86 |  101 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  102 |  |
|     89 |  103 | `	int res = 0; /* Assume false by default */` |
|     89 |  104 | `	if( nArg > 0 ){` |
|     87 |  105 | `		res = ph7_value_is_null(apArg[0]);` |
|     42 |  106 | `	}` |
|      - |  107 | `	/* Query result */` |
|     89 |  108 | `	ph7_result_bool(pCtx,res);` |
|     89 |  109 | `	return PH7_OK;` |
|      3 |  110 |  |
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
|    240 |  155 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  156 |  |
|    244 |  157 | `	int res = 0; /* Assume false by default */` |
|    244 |  158 | `	if( nArg > 0 ){` |
|    242 |  159 | `		res = ph7_value_is_array(apArg[0]);` |
|    119 |  160 | `	}` |
|      - |  161 | `	/* Query result */` |
|    244 |  162 | `	ph7_result_bool(pCtx,res);` |
|    244 |  163 | `	return PH7_OK;` |
|      4 |  164 |  |
|      - |  165 | `/*` |
|      - |  166 | ` * bool is_object($var)` |
|      - |  167 | ` *  Find out whether a variable is an object.` |
|      - |  168 | ` * Parameters` |
|      - |  169 | ` *  $var: The variable being evaluated.` |
|      - |  170 | ` * Return` |
|      - |  171 | ` *  True if var is an object. False otherwise.` |
|      - |  172 | ` */` |
|     20 |  173 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  174 |  |
|     21 |  175 | `	int res = 0; /* Assume false by default */` |
|     21 |  176 | `	if( nArg > 0 ){` |
|     19 |  177 | `		res = ph7_value_is_object(apArg[0]);` |
|      9 |  178 | `	}` |
|      - |  179 | `	/* Query result */` |
|     21 |  180 | `	ph7_result_bool(pCtx,res);` |
|     21 |  181 | `	return PH7_OK;` |
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
|  26166 |  295 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  296 |  |
|  26171 |  297 | `	int res = 1; /* Assume empty by default */` |
|  26171 |  298 | `	if( nArg > 0 ){` |
|  26169 |  299 | `		res = ph7_value_is_empty(apArg[0]);` |
|  13082 |  300 | `	}` |
|  26171 |  301 | `	ph7_result_bool(pCtx,res);` |
|  26171 |  302 | `	return PH7_OK;` |
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
| 194352 |  345 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  346 |  |
|      - |  347 | `	const char *zSource,*zOfft;` |
|      - |  348 | `	int nOfft,nLen,nSrcLen;` |
| 194357 |  349 | `	if( nArg < 2 ){` |
|      - |  350 | `		/* return FALSE */` |
|      5 |  351 | `		ph7_result_bool(pCtx,0);` |
|      5 |  352 | `		return PH7_OK;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Extract the target string */` |
| 194353 |  355 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 194353 |  356 | `	if( nSrcLen < 1 ){` |
|      - |  357 | `		/* Empty string,return FALSE */` |
|  11201 |  358 | `		ph7_result_bool(pCtx,0);` |
|  11201 |  359 | `		return PH7_OK;` |
|      - |  360 | `	}` |
| 183157 |  361 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  362 | `	/* Extract the offset */` |
| 183157 |  363 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 183157 |  364 | `	if( nOfft < 0 ){` |
|  30335 |  365 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  30335 |  366 | `		if( zOfft < zSource ){` |
|      - |  367 | `			/* Invalid offset */` |
|      5 |  368 | `			ph7_result_bool(pCtx,0);` |
|      5 |  369 | `			return PH7_OK;` |
|      - |  370 | `		}` |
|  30331 |  371 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  30331 |  372 | `		nOfft = (int)(zOfft-zSource);` |
| 167990 |  373 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  374 | `		/* Invalid offset */` |
|    165 |  375 | `		ph7_result_bool(pCtx,0);` |
|    165 |  376 | `		return PH7_OK;` |
|    ! 0 |  377 | `	}else{` |
| 152667 |  378 | `		zOfft = &zSource[nOfft];` |
| 152667 |  379 | `		nLen = nSrcLen - nOfft;` |
|      - |  380 | `	}` |
| 182993 |  381 | `	if( nArg > 2 ){` |
|      - |  382 | `		/* Extract the length */` |
| 151163 |  383 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 151163 |  384 | `		if( nLen == 0 ){` |
|      - |  385 | `			/* Invalid length,return an empty string */` |
|      5 |  386 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  387 | `			return PH7_OK;` |
| 151159 |  388 | `		}else if( nLen < 0 ){` |
|  30323 |  389 | `			nLen = nSrcLen + nLen - nOfft;` |
|  30323 |  390 | `			if( nLen < 1 ){` |
|      - |  391 | `				/* Invalid  length */` |
|      3 |  392 | `				nLen = nSrcLen - nOfft;` |
|      1 |  393 | `			}` |
|  15159 |  394 | `		}` |
| 151159 |  395 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  396 | `			/* Invalid length */` |
|   4365 |  397 | `			nLen = nSrcLen - nOfft;` |
|   2180 |  398 | `		}` |
|  75577 |  399 | `	}` |
|      - |  400 | `	/* Return the substring */` |
| 182989 |  401 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 182989 |  402 | `	return PH7_OK;` |
|  97181 |  403 |  |
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
|   6028 | 1372 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1373 |  |
|   6033 | 1374 | `	int iLen = 0;` |
|   6033 | 1375 | `	if( nArg > 0 ){` |
|   6031 | 1376 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   3013 | 1377 | `	}` |
|      - | 1378 | `	/* String length */` |
|   6033 | 1379 | `	ph7_result_int(pCtx,iLen);` |
|   6033 | 1380 | `	return PH7_OK;` |
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
|      - | 1519 | `};` |
|      - | 1520 | `/*` |
|      - | 1521 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1522 | ` * The following routine is invoked for each array entry passed` |
|      - | 1523 | ` * to the implode() function.` |
|      - | 1524 | ` */` |
| 122548 | 1525 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1526 |  |
|  61274 | 1527 | `	SXUNUSED(pKey);` |
| 122553 | 1528 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1529 | `	const char *zData;` |
|      - | 1530 | `	int nLen;` |
| 122553 | 1531 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1532 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1533 | `			if( !pData->bFirst ){` |
|      - | 1534 | `				/* append the separator first */` |
|      3 | 1535 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 1536 | `			}else{` |
|    ! 0 | 1537 | `				pData->bFirst = 0;` |
|      - | 1538 | `			}` |
|      1 | 1539 | `		}` |
|      - | 1540 | `		/* Recurse */` |
|      3 | 1541 | `		pData->bFirst = 1;` |
|      3 | 1542 | `		pData->nRecCount++;` |
|      3 | 1543 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1544 | `		pData->nRecCount--;` |
|      3 | 1545 | `		return PH7_OK;` |
|      - | 1546 | `	}` |
|      - | 1547 | `	/* Extract the string representation of the entry value */` |
| 122551 | 1548 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1549 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 122551 | 1550 | `	if( pData->bFirst ){` |
|  30633 | 1551 | `		pData->bFirst = 0;` |
| 107237 | 1552 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1553 | `		/* append the separator first */` |
|  91911 | 1554 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  45953 | 1555 | `	}` |
|      - | 1556 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 122551 | 1557 | `	if( nLen > 0 ){` |
| 111355 | 1558 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  55675 | 1559 | `	}` |
| 122551 | 1560 | `	return PH7_OK;` |
|  61279 | 1561 |  |
|      - | 1562 | `/*` |
|      - | 1563 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1564 | ` * string implode(array $pieces,...)` |
|      - | 1565 | ` *  Join array elements with a string.` |
|      - | 1566 | ` * $glue` |
|      - | 1567 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1568 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1569 | ` * $pieces` |
|      - | 1570 | ` *   The array of strings to implode.` |
|      - | 1571 | ` * Return` |
|      - | 1572 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1573 | ` *  order, with the glue string between each element.` |
|      - | 1574 | ` */` |
|  30654 | 1575 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1576 |  |
|      - | 1577 | `	struct implode_data imp_data;` |
|  30659 | 1578 | `	int i = 1;` |
|  30659 | 1579 | `	if( nArg < 1 ){` |
|      - | 1580 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1581 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1582 | `		return PH7_OK;` |
|      - | 1583 | `	}` |
|      - | 1584 | `	/* Prepare the implode context */` |
|  30659 | 1585 | `	imp_data.pCtx = pCtx;` |
|  30659 | 1586 | `	imp_data.bRecursive = 0;` |
|  30659 | 1587 | `	imp_data.bFirst = 1;` |
|  30659 | 1588 | `	imp_data.nRecCount = 0;` |
|  30659 | 1589 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  30657 | 1590 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  15331 | 1591 | `	}else{` |
|      3 | 1592 | `		imp_data.zSep = 0;` |
|      3 | 1593 | `		imp_data.nSeplen = 0;` |
|      3 | 1594 | `		i = 0;` |
|      - | 1595 | `	}` |
|  30659 | 1596 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1597 | `	/* Start the 'join' process */` |
|  61313 | 1598 | `	while( i < nArg ){` |
|  30659 | 1599 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1600 | `			/* Iterate throw array entries */` |
|  30659 | 1601 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  15332 | 1602 | `		}else{` |
|      - | 1603 | `			const char *zData;` |
|      - | 1604 | `			int nLen;` |
|      - | 1605 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1606 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1607 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1608 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1609 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1610 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1611 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1612 | `			}` |
|      - | 1613 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1614 | `			if( nLen > 0 ){` |
|    ! 0 | 1615 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 1616 | `			}` |
|      - | 1617 | `		}` |
|  30659 | 1618 | `		i++;` |
|      5 | 1619 | `	}` |
|  30659 | 1620 | `	return PH7_OK;` |
|  15332 | 1621 |  |
|      - | 1622 | `/*` |
|      - | 1623 | ` * Symisc eXtension:` |
|      - | 1624 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1625 | ` * Purpose` |
|      - | 1626 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1627 | ` * Example:` |
|      - | 1628 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1629 | ` *   echo implode_recursive("/",$a);` |
|      - | 1630 | ` *   Will output` |
|      - | 1631 | ` *     usr/home/dean.` |
|      - | 1632 | ` *   While the standard implode would produce.` |
|      - | 1633 | ` *    usr/Array.` |
|      - | 1634 | ` * Parameter` |
|      - | 1635 | ` *  Refer to implode().` |
|      - | 1636 | ` * Return` |
|      - | 1637 | ` *  Refer to implode().` |
|      - | 1638 | ` */` |
|     12 | 1639 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1640 |  |
|      - | 1641 | `	struct implode_data imp_data;` |
|     13 | 1642 | `	int i = 1;` |
|     13 | 1643 | `	if( nArg < 1 ){` |
|      - | 1644 | `		/* Missing argument,return NULL */` |
|      3 | 1645 | `		ph7_result_null(pCtx);` |
|      3 | 1646 | `		return PH7_OK;` |
|      - | 1647 | `	}` |
|      - | 1648 | `	/* Prepare the implode context */` |
|     11 | 1649 | `	imp_data.pCtx = pCtx;` |
|     11 | 1650 | `	imp_data.bRecursive = 1;` |
|     11 | 1651 | `	imp_data.bFirst = 1;` |
|     11 | 1652 | `	imp_data.nRecCount = 0;` |
|     11 | 1653 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1654 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1655 | `	}else{` |
|    ! 0 | 1656 | `		imp_data.zSep = 0;` |
|    ! 0 | 1657 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1658 | `		i = 0;` |
|      - | 1659 | `	}` |
|     11 | 1660 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1661 | `	/* Start the 'join' process */` |
|     21 | 1662 | `	while( i < nArg ){` |
|     11 | 1663 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1664 | `			/* Iterate throw array entries */` |
|      3 | 1665 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 1666 | `		}else{` |
|      - | 1667 | `			const char *zData;` |
|      - | 1668 | `			int nLen;` |
|      - | 1669 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1670 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1671 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1672 | `			if( imp_data.bFirst ){` |
|      9 | 1673 | `				imp_data.bFirst = 0;` |
|      4 | 1674 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1675 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1676 | `			}` |
|      - | 1677 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1678 | `			if( nLen > 0 ){` |
|      9 | 1679 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 1680 | `			}` |
|      - | 1681 | `		}` |
|     11 | 1682 | `		i++;` |
|      1 | 1683 | `	}` |
|     11 | 1684 | `	return PH7_OK;` |
|      7 | 1685 |  |
|      - | 1686 | `/*` |
|      - | 1687 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1688 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1689 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1690 | ` * Parameters` |
|      - | 1691 | ` *  $delimiter` |
|      - | 1692 | ` *   The boundary string.` |
|      - | 1693 | ` * $string` |
|      - | 1694 | ` *   The input string.` |
|      - | 1695 | ` * $limit` |
|      - | 1696 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1697 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1698 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1699 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1700 | ` * Returns` |
|      - | 1701 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1702 | ` *  on boundaries formed by the delimiter.` |
|      - | 1703 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1704 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1705 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1706 | ` *  will be returned.` |
|      - | 1707 | ` * NOTE:` |
|      - | 1708 | ` *  Negative limit is not supported.` |
|      - | 1709 | ` */` |
|   5766 | 1710 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1711 |  |
|      - | 1712 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1713 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1714 | `	ph7_value *pArray;` |
|      - | 1715 | `	ph7_value *pValue;` |
|      - | 1716 | `	sxu32 nOfft;` |
|      - | 1717 | `	sxi32 rc;` |
|   5771 | 1718 | `	if( nArg < 2 ){` |
|      - | 1719 | `		/* Missing arguments,return FALSE */` |
|      9 | 1720 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1721 | `		return PH7_OK;` |
|      - | 1722 | `	}` |
|      - | 1723 | `	/* Extract the delimiter */` |
|   5763 | 1724 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   5763 | 1725 | `	if( nDelim < 1 ){` |
|      - | 1726 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1727 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1728 | `		return PH7_OK;` |
|      - | 1729 | `	}` |
|      - | 1730 | `	/* Extract the string */` |
|   5761 | 1731 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   5761 | 1732 | `	if( nStrlen < 1 ){` |
|      - | 1733 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 1734 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 1735 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 1736 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 1737 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1738 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1739 | `			return PH7_OK;` |
|      - | 1740 | `		}` |
|      3 | 1741 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 1742 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 1743 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 1744 | `		return PH7_OK;` |
|      - | 1745 | `	}` |
|      - | 1746 | `	/* Point to the end of the string */` |
|   5759 | 1747 | `	zEnd = &zString[nStrlen];` |
|      - | 1748 | `	/* Create the array */` |
|   5759 | 1749 | `	pArray =  ph7_context_new_array(pCtx);` |
|   5759 | 1750 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   5759 | 1751 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1752 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1754 | `		return PH7_OK;` |
|      - | 1755 | `	}` |
|      - | 1756 | `	/* Set a defualt limit */` |
|   5759 | 1757 | `	iLimit = SXI32_HIGH;` |
|   5759 | 1758 | `	if( nArg > 2 ){` |
|     11 | 1759 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     11 | 1760 | `		 if( iLimit < 0 ){` |
|      3 | 1761 | `			iLimit = -iLimit;` |
|      1 | 1762 | `		}` |
|     11 | 1763 | `		if( iLimit == 0 ){` |
|      3 | 1764 | `			iLimit = 1;` |
|      1 | 1765 | `		}` |
|     11 | 1766 | `		iLimit--;` |
|      5 | 1767 | `	}` |
|      - | 1768 | `	/* Start exploding */` |
|  66041 | 1769 | `	for(;;){` |
| 132087 | 1770 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 132087 | 1771 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1772 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   5759 | 1773 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   5759 | 1774 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   5759 | 1775 | `			break;` |
|      - | 1776 | `		}` |
|      - | 1777 | `		/* Point to the desired offset */` |
| 126333 | 1778 | `		zCur = &zString[nOfft];` |
|      - | 1779 | `		/* Perform the store operation (may be empty) */` |
| 126333 | 1780 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 126333 | 1781 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 1782 | `		/* Point beyond the delimiter */` |
| 126333 | 1783 | `		zString = &zCur[nDelim];` |
|      - | 1784 | `		/* Reset the cursor */` |
| 126333 | 1785 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1786 | `	}` |
|      - | 1787 | `	/* Return the freshly created array */` |
|   5759 | 1788 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1789 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1790 | `	 * released as soon we return from this foregin function.` |
|      - | 1791 | `	 */` |
|   5759 | 1792 | `	return PH7_OK;` |
|   2888 | 1793 |  |
|      - | 1794 | `/*` |
|      - | 1795 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1796 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1797 | ` * Parameters` |
|      - | 1798 | ` *  $str` |
|      - | 1799 | ` *   The string that will be trimmed.` |
|      - | 1800 | ` * $charlist` |
|      - | 1801 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1802 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1803 | ` *   With .. you can specify a range of characters.` |
|      - | 1804 | ` * Returns.` |
|      - | 1805 | ` *  Thr processed string.` |
|      - | 1806 | ` * NOTE:` |
|      - | 1807 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1808 | ` */` |
|  13204 | 1809 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1810 |  |
|      - | 1811 | `	const char *zString;` |
|      - | 1812 | `	int nLen;` |
|  13209 | 1813 | `	if( nArg < 1 ){` |
|      - | 1814 | `		/* Missing arguments,return null */` |
|      3 | 1815 | `		ph7_result_null(pCtx);` |
|      3 | 1816 | `		return PH7_OK;` |
|      - | 1817 | `	}` |
|      - | 1818 | `	/* Extract the target string */` |
|  13207 | 1819 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  13207 | 1820 | `	if( nLen < 1 ){` |
|      - | 1821 | `		/* Empty string,return */` |
|   1691 | 1822 | `		ph7_result_string(pCtx,"",0);` |
|   1691 | 1823 | `		return PH7_OK;` |
|      - | 1824 | `	}` |
|      - | 1825 | `	/* Start the trim process */` |
|  11521 | 1826 | `	if( nArg < 2 ){` |
|      - | 1827 | `		SyString sStr;` |
|      - | 1828 | `		/* Remove white spaces and NUL bytes */` |
|  11517 | 1829 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  28095 | 1830 | `		SyStringFullTrimSafe(&sStr);` |
|  11517 | 1831 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   5761 | 1832 | `	}else{` |
|      - | 1833 | `		/* Char list */` |
|      - | 1834 | `		const char *zList;` |
|      - | 1835 | `		int nListlen;` |
|      5 | 1836 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1837 | `		if( nListlen < 1 ){` |
|      - | 1838 | `			/* Return the string unchanged */` |
|      3 | 1839 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1840 | `		}else{` |
|      3 | 1841 | `			const char *zEnd = &zString[nLen];` |
|      3 | 1842 | `			const char *zCur = zString;` |
|      - | 1843 | `			const char *zPtr;` |
|      - | 1844 | `			int i;` |
|      - | 1845 | `			/* Left trim */` |
|      4 | 1846 | `			for(;;){` |
|      9 | 1847 | `				if( zCur >= zEnd ){` |
|    ! 0 | 1848 | `					break;` |
|      - | 1849 | `				}` |
|      9 | 1850 | `				zPtr = zCur;` |
|     17 | 1851 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1852 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 1853 | `						zCur++;` |
|      3 | 1854 | `					}` |
|      5 | 1855 | `				}` |
|      9 | 1856 | `				if( zCur == zPtr ){` |
|      - | 1857 | `					/* No match,break immediately */` |
|      3 | 1858 | `					break;` |
|      - | 1859 | `				}` |
|      1 | 1860 | `			}` |
|      - | 1861 | `			/* Right trim */` |
|      3 | 1862 | `			zEnd--;` |
|      4 | 1863 | `			for(;;){` |
|      9 | 1864 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1865 | `					break;` |
|      - | 1866 | `				}` |
|      9 | 1867 | `				zPtr = zEnd;` |
|     17 | 1868 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1869 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 1870 | `						zEnd--;` |
|      3 | 1871 | `					}` |
|      5 | 1872 | `				}` |
|      9 | 1873 | `				if( zEnd == zPtr ){` |
|      3 | 1874 | `					break;` |
|      - | 1875 | `				}` |
|      1 | 1876 | `			}` |
|      3 | 1877 | `			if( zCur >= zEnd ){` |
|      - | 1878 | `				/* Return the empty string */` |
|    ! 0 | 1879 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1880 | `			}else{` |
|      3 | 1881 | `				zEnd++;` |
|      3 | 1882 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1883 | `			}` |
|      - | 1884 | `		}` |
|      - | 1885 | `	}` |
|  11521 | 1886 | `	return PH7_OK;` |
|   6607 | 1887 |  |
|      - | 1888 | `/*` |
|      - | 1889 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1890 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1891 | ` * Parameters` |
|      - | 1892 | ` *  $str` |
|      - | 1893 | ` *   The string that will be trimmed.` |
|      - | 1894 | ` * $charlist` |
|      - | 1895 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1896 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1897 | ` *   With .. you can specify a range of characters.` |
|      - | 1898 | ` * Returns.` |
|      - | 1899 | ` *  Thr processed string.` |
|      - | 1900 | ` * NOTE:` |
|      - | 1901 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1902 | ` */` |
|     26 | 1903 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1904 |  |
|      - | 1905 | `	const char *zString;` |
|      - | 1906 | `	int nLen;` |
|     27 | 1907 | `	if( nArg < 1 ){` |
|      - | 1908 | `		/* Missing arguments,return null */` |
|      3 | 1909 | `		ph7_result_null(pCtx);` |
|      3 | 1910 | `		return PH7_OK;` |
|      - | 1911 | `	}` |
|      - | 1912 | `	/* Extract the target string */` |
|     25 | 1913 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1914 | `	if( nLen < 1 ){` |
|      - | 1915 | `		/* Empty string,return */` |
|      5 | 1916 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1917 | `		return PH7_OK;` |
|      - | 1918 | `	}` |
|      - | 1919 | `	/* Start the trim process */` |
|     21 | 1920 | `	if( nArg < 2 ){` |
|      - | 1921 | `		SyString sStr;` |
|      - | 1922 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1923 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1924 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1925 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1926 | `	}else{` |
|      - | 1927 | `		/* Char list */` |
|      - | 1928 | `		const char *zList;` |
|      - | 1929 | `		int nListlen;` |
|      5 | 1930 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1931 | `		if( nListlen < 1 ){` |
|      - | 1932 | `			/* Return the string unchanged */` |
|    ! 0 | 1933 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1934 | `		}else{` |
|      5 | 1935 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 1936 | `			const char *zCur = zString;` |
|      - | 1937 | `			const char *zPtr;` |
|      - | 1938 | `			int i;` |
|      - | 1939 | `			/* Right trim */` |
|      6 | 1940 | `			for(;;){` |
|     13 | 1941 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1942 | `					break;` |
|      - | 1943 | `				}` |
|     13 | 1944 | `				zPtr = zEnd;` |
|     25 | 1945 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 1946 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 1947 | `						zEnd--;` |
|      4 | 1948 | `					}` |
|      7 | 1949 | `				}` |
|     13 | 1950 | `				if( zEnd == zPtr ){` |
|      5 | 1951 | `					break;` |
|      - | 1952 | `				}` |
|      1 | 1953 | `			}` |
|      5 | 1954 | `			if( zEnd <= zCur ){` |
|      - | 1955 | `				/* Return the empty string */` |
|    ! 0 | 1956 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1957 | `			}else{` |
|      5 | 1958 | `				zEnd++;` |
|      5 | 1959 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1960 | `			}` |
|      - | 1961 | `		}` |
|      - | 1962 | `	}` |
|     21 | 1963 | `	return PH7_OK;` |
|     14 | 1964 |  |
|      - | 1965 | `/*` |
|      - | 1966 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 1967 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1968 | ` * Parameters` |
|      - | 1969 | ` *  $str` |
|      - | 1970 | ` *   The string that will be trimmed.` |
|      - | 1971 | ` * $charlist` |
|      - | 1972 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1973 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1974 | ` *   With .. you can specify a range of characters.` |
|      - | 1975 | ` * Returns.` |
|      - | 1976 | ` *  Thr processed string.` |
|      - | 1977 | ` * NOTE:` |
|      - | 1978 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1979 | ` */` |
|     12 | 1980 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1981 |  |
|      - | 1982 | `	const char *zString;` |
|      - | 1983 | `	int nLen;` |
|     13 | 1984 | `	if( nArg < 1 ){` |
|      - | 1985 | `		/* Missing arguments,return null */` |
|      3 | 1986 | `		ph7_result_null(pCtx);` |
|      3 | 1987 | `		return PH7_OK;` |
|      - | 1988 | `	}` |
|      - | 1989 | `	/* Extract the target string */` |
|     11 | 1990 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1991 | `	if( nLen < 1 ){` |
|      - | 1992 | `		/* Empty string,return */` |
|    ! 0 | 1993 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1994 | `		return PH7_OK;` |
|      - | 1995 | `	}` |
|      - | 1996 | `	/* Start the trim process */` |
|     11 | 1997 | `	if( nArg < 2 ){` |
|      - | 1998 | `		SyString sStr;` |
|      - | 1999 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2000 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2001 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2002 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2003 | `	}else{` |
|      - | 2004 | `		/* Char list */` |
|      - | 2005 | `		const char *zList;` |
|      - | 2006 | `		int nListlen;` |
|      9 | 2007 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2008 | `		if( nListlen < 1 ){` |
|      - | 2009 | `			/* Return the string unchanged */` |
|      3 | 2010 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2011 | `		}else{` |
|      7 | 2012 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2013 | `			const char *zCur = zString;` |
|      - | 2014 | `			const char *zPtr;` |
|      - | 2015 | `			int i;` |
|      - | 2016 | `			/* Left trim */` |
|      7 | 2017 | `			for(;;){` |
|     15 | 2018 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2019 | `					break;` |
|      - | 2020 | `				}` |
|     15 | 2021 | `				zPtr = zCur;` |
|     41 | 2022 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2023 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2024 | `						zCur++;` |
|      6 | 2025 | `					}` |
|     14 | 2026 | `				}` |
|     15 | 2027 | `				if( zCur == zPtr ){` |
|      - | 2028 | `					/* No match,break immediately */` |
|      7 | 2029 | `					break;` |
|      - | 2030 | `				}` |
|      1 | 2031 | `			}` |
|      7 | 2032 | `			if( zCur >= zEnd ){` |
|      - | 2033 | `				/* Return the empty string */` |
|    ! 0 | 2034 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2035 | `			}else{` |
|      7 | 2036 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2037 | `			}` |
|      - | 2038 | `		}` |
|      - | 2039 | `	}` |
|     11 | 2040 | `	return PH7_OK;` |
|      7 | 2041 |  |
|      - | 2042 | `/*` |
|      - | 2043 | ` * string strtolower(string $str)` |
|      - | 2044 | ` *  Make a string lowercase.` |
|      - | 2045 | ` * Parameters` |
|      - | 2046 | ` *  $str` |
|      - | 2047 | ` *   The input string.` |
|      - | 2048 | ` * Returns.` |
|      - | 2049 | ` *  The lowercased string.` |
|      - | 2050 | ` */` |
|  30320 | 2051 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2052 |  |
|      - | 2053 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2054 | `	int nLen;` |
|  30325 | 2055 | `	if( nArg < 1 ){` |
|      - | 2056 | `		/* Missing arguments,return null */` |
|      3 | 2057 | `		ph7_result_null(pCtx);` |
|      3 | 2058 | `		return PH7_OK;` |
|      - | 2059 | `	}` |
|      - | 2060 | `	/* Extract the target string */` |
|  30323 | 2061 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  30323 | 2062 | `	if( nLen < 1 ){` |
|      - | 2063 | `		/* Empty string,return */` |
|      3 | 2064 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2065 | `		return PH7_OK;` |
|      - | 2066 | `	}` |
|      - | 2067 | `	/* Perform the requested operation */` |
|  30321 | 2068 | `	zEnd = &zString[nLen];` |
|  95550 | 2069 | `	for(;;){` |
| 191105 | 2070 | `		if( zString >= zEnd ){` |
|      - | 2071 | `			/* No more input,break immediately */` |
|  30321 | 2072 | `			break;` |
|      - | 2073 | `		}` |
| 160789 | 2074 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2075 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2076 | `			zCur = zString;` |
|    ! 0 | 2077 | `			zString++;` |
|    ! 0 | 2078 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2079 | `				zString++;` |
|    ! 0 | 2080 | `			}` |
|      - | 2081 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2082 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2083 | `		}else{` |
| 160789 | 2084 | `			int c = zString[0];` |
| 160789 | 2085 | `			if( SyisUpper(c) ){` |
| 160787 | 2086 | `				c = SyToLower(zString[0]);` |
|  80391 | 2087 | `			}` |
|      - | 2088 | `			/* Append character */` |
| 160789 | 2089 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2090 | `			/* Advance the cursor */` |
| 160789 | 2091 | `			zString++;` |
|      - | 2092 | `		}` |
|      5 | 2093 | `	}` |
|  30321 | 2094 | `	return PH7_OK;` |
|  15165 | 2095 |  |
|      - | 2096 | `/*` |
|      - | 2097 | ` * string strtolower(string $str)` |
|      - | 2098 | ` *  Make a string uppercase.` |
|      - | 2099 | ` * Parameters` |
|      - | 2100 | ` *  $str` |
|      - | 2101 | ` *   The input string.` |
|      - | 2102 | ` * Returns.` |
|      - | 2103 | ` *  The uppercased string.` |
|      - | 2104 | ` */` |
|     34 | 2105 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2106 |  |
|      - | 2107 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2108 | `	int nLen;` |
|     39 | 2109 | `	if( nArg < 1 ){` |
|      - | 2110 | `		/* Missing arguments,return null */` |
|      3 | 2111 | `		ph7_result_null(pCtx);` |
|      3 | 2112 | `		return PH7_OK;` |
|      - | 2113 | `	}` |
|      - | 2114 | `	/* Extract the target string */` |
|     37 | 2115 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     37 | 2116 | `	if( nLen < 1 ){` |
|      - | 2117 | `		/* Empty string,return */` |
|      3 | 2118 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2119 | `		return PH7_OK;` |
|      - | 2120 | `	}` |
|      - | 2121 | `	/* Perform the requested operation */` |
|     35 | 2122 | `	zEnd = &zString[nLen];` |
|     88 | 2123 | `	for(;;){` |
|    181 | 2124 | `		if( zString >= zEnd ){` |
|      - | 2125 | `			/* No more input,break immediately */` |
|     35 | 2126 | `			break;` |
|      - | 2127 | `		}` |
|    151 | 2128 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2129 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2130 | `			zCur = zString;` |
|    ! 0 | 2131 | `			zString++;` |
|    ! 0 | 2132 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2133 | `				zString++;` |
|    ! 0 | 2134 | `			}` |
|      - | 2135 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2136 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2137 | `		}else{` |
|    151 | 2138 | `			int c = zString[0];` |
|    151 | 2139 | `			if( SyisLower(c) ){` |
|    145 | 2140 | `				c = SyToUpper(zString[0]);` |
|     70 | 2141 | `			}` |
|      - | 2142 | `			/* Append character */` |
|    151 | 2143 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2144 | `			/* Advance the cursor */` |
|    151 | 2145 | `			zString++;` |
|      - | 2146 | `		}` |
|      5 | 2147 | `	}` |
|     35 | 2148 | `	return PH7_OK;` |
|     22 | 2149 |  |
|      - | 2150 | `/*` |
|      - | 2151 | ` * string ucfirst(string $str)` |
|      - | 2152 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2153 | ` *  character is alphabetic.` |
|      - | 2154 | ` * Parameters` |
|      - | 2155 | ` *  $str` |
|      - | 2156 | ` *   The input string.` |
|      - | 2157 | ` * Returns.` |
|      - | 2158 | ` *  The processed string.` |
|      - | 2159 | ` */` |
|      6 | 2160 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2161 |  |
|      - | 2162 | `	const char *zString,*zEnd;` |
|      - | 2163 | `	int nLen,c;` |
|      7 | 2164 | `	if( nArg < 1 ){` |
|      - | 2165 | `		/* Missing arguments,return null */` |
|      3 | 2166 | `		ph7_result_null(pCtx);` |
|      3 | 2167 | `		return PH7_OK;` |
|      - | 2168 | `	}` |
|      - | 2169 | `	/* Extract the target string */` |
|      5 | 2170 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2171 | `	if( nLen < 1 ){` |
|      - | 2172 | `		/* Empty string,return */` |
|      3 | 2173 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2174 | `		return PH7_OK;` |
|      - | 2175 | `	}` |
|      - | 2176 | `	/* Perform the requested operation */` |
|      3 | 2177 | `	zEnd = &zString[nLen];` |
|      3 | 2178 | `	c = zString[0];` |
|      3 | 2179 | `	if( SyisLower(c) ){` |
|      3 | 2180 | `		c = SyToUpper(c);` |
|      1 | 2181 | `	}` |
|      - | 2182 | `	/* Append the first character */` |
|      3 | 2183 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2184 | `	zString++;` |
|      3 | 2185 | `	if( zString < zEnd ){` |
|      - | 2186 | `		/* Append the rest of the input verbatim */` |
|      3 | 2187 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2188 | `	}` |
|      3 | 2189 | `	return PH7_OK;` |
|      4 | 2190 |  |
|      - | 2191 | `/*` |
|      - | 2192 | ` * string lcfirst(string $str)` |
|      - | 2193 | ` *  Make a string's first character lowercase.` |
|      - | 2194 | ` * Parameters` |
|      - | 2195 | ` *  $str` |
|      - | 2196 | ` *   The input string.` |
|      - | 2197 | ` * Returns.` |
|      - | 2198 | ` *  The processed string.` |
|      - | 2199 | ` */` |
|      6 | 2200 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2201 |  |
|      - | 2202 | `	const char *zString,*zEnd;` |
|      - | 2203 | `	int nLen,c;` |
|      7 | 2204 | `	if( nArg < 1 ){` |
|      - | 2205 | `		/* Missing arguments,return null */` |
|      3 | 2206 | `		ph7_result_null(pCtx);` |
|      3 | 2207 | `		return PH7_OK;` |
|      - | 2208 | `	}` |
|      - | 2209 | `	/* Extract the target string */` |
|      5 | 2210 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2211 | `	if( nLen < 1 ){` |
|      - | 2212 | `		/* Empty string,return */` |
|      3 | 2213 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2214 | `		return PH7_OK;` |
|      - | 2215 | `	}` |
|      - | 2216 | `	/* Perform the requested operation */` |
|      3 | 2217 | `	zEnd = &zString[nLen];` |
|      3 | 2218 | `	c = zString[0];` |
|      3 | 2219 | `	if( SyisUpper(c) ){` |
|      3 | 2220 | `		c = SyToLower(c);` |
|      1 | 2221 | `	}` |
|      - | 2222 | `	/* Append the first character */` |
|      3 | 2223 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2224 | `	zString++;` |
|      3 | 2225 | `	if( zString < zEnd ){` |
|      - | 2226 | `		/* Append the rest of the input verbatim */` |
|      3 | 2227 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2228 | `	}` |
|      3 | 2229 | `	return PH7_OK;` |
|      4 | 2230 |  |
|      - | 2231 | `/*` |
|      - | 2232 | ` * int ord(string $string)` |
|      - | 2233 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2234 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2235 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2236 | ` * Parameters` |
|      - | 2237 | ` *  $string` |
|      - | 2238 | ` *   The input string.` |
|      - | 2239 | ` * Returns` |
|      - | 2240 | ` *  The ASCII value as an integer.` |
|      - | 2241 | ` */` |
|     62 | 2242 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2243 |  |
|      - | 2244 | `	const char *zString;` |
|      - | 2245 | `	int nLen,c;` |
|      - | 2246 | `	/* PHP requires exactly one argument. */` |
|     65 | 2247 | `	if( nArg != 1 ){` |
|      8 | 2248 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2249 | `			"ArgumentCountError",` |
|      - | 2250 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2251 | `			nArg` |
|      - | 2252 | `			);` |
|      - | 2253 | `	}` |
|      - | 2254 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2255 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2256 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2257 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2258 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2259 | `			"of type string is deprecated"` |
|      - | 2260 | `			);` |
|      1 | 2261 | `	}` |
|      - | 2262 | `	/* Extract the target string */` |
|     59 | 2263 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2264 | `	if( nLen < 1 ){` |
|      - | 2265 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2266 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2267 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2268 | `			);` |
|      5 | 2269 | `		ph7_result_int(pCtx,0);` |
|      5 | 2270 | `		return PH7_OK;` |
|      - | 2271 | `	}` |
|      - | 2272 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2273 | `	if( nLen > 1 ){` |
|      7 | 2274 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2275 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2276 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2277 | `			);` |
|      3 | 2278 | `	}` |
|      - | 2279 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2280 | `	c = (unsigned char)zString[0];` |
|      - | 2281 | `	/* Return that value */` |
|     55 | 2282 | `	ph7_result_int(pCtx,c);` |
|     55 | 2283 | `	return PH7_OK;` |
|     34 | 2284 |  |
|      - | 2285 | `/*` |
|      - | 2286 | ` * string chr(int $codepoint)` |
|      - | 2287 | ` *  Returns a one-character string containing the character specified` |
|      - | 2288 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2289 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2290 | ` * Parameters` |
|      - | 2291 | ` *  $codepoint` |
|      - | 2292 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2293 | ` *   will be constrained to a single byte.` |
|      - | 2294 | ` * Returns` |
|      - | 2295 | ` *  A single-character string.` |
|      - | 2296 | ` */` |
|     44 | 2297 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2298 |  |
|      - | 2299 | `	int c;` |
|      - | 2300 | `	unsigned char ch;` |
|      - | 2301 | `	/* PHP requires exactly one argument. */` |
|     47 | 2302 | `	if( nArg != 1 ){` |
|      8 | 2303 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2304 | `			"ArgumentCountError",` |
|      - | 2305 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2306 | `			nArg` |
|      - | 2307 | `			);` |
|      - | 2308 | `	}` |
|      - | 2309 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2310 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2311 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2312 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     41 | 2313 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2314 | `		char zBuf[120];` |
|      4 | 2315 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2316 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2317 | `			ph7_value_to_double(apArg[0])` |
|      - | 2318 | `			);` |
|      3 | 2319 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2320 | `	}` |
|      - | 2321 | `	/* Extract the codepoint. */` |
|     41 | 2322 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2323 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2324 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2325 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2326 | `	 * name to avoid the API double-prefixing it. */` |
|     41 | 2327 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2328 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2329 | `			E_DEPRECATED,` |
|      - | 2330 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2331 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2332 | `			"The value used will be constrained using % 256"` |
|      - | 2333 | `			);` |
|      2 | 2334 | `	}` |
|      - | 2335 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2336 | `	 * when taking the address of a wider int. */` |
|     41 | 2337 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2338 | `	/* Return the specified character */` |
|     41 | 2339 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     41 | 2340 | `	return PH7_OK;` |
|     25 | 2341 |  |
|      - | 2342 | `/*` |
|      - | 2343 | ` * Binary to hex consumer callback.` |
|      - | 2344 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2345 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2346 | ` */` |
|   2330 | 2347 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2348 |  |
|      - | 2349 | `	/* Append hex chunk verbatim */` |
|   2331 | 2350 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   2331 | 2351 | `	return SXRET_OK;` |
|      1 | 2352 |  |
|      - | 2353 |  |
|      - | 2354 | `/*` |
|      - | 2355 | ` * string bin2hex(string $str)` |
|      - | 2356 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2357 | ` * Parameters` |
|      - | 2358 | ` *  $str` |
|      - | 2359 | ` *   The input string.` |
|      - | 2360 | ` * Returns.` |
|      - | 2361 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2362 | ` */` |
|     24 | 2363 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2364 |  |
|      - | 2365 | `	const char *zString;` |
|      - | 2366 | `	int nLen;` |
|      - | 2367 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     29 | 2368 | `	if( nArg != 1 ){` |
|      8 | 2369 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2370 | `			"ArgumentCountError",` |
|      - | 2371 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2372 | `			nArg` |
|      - | 2373 | `			);` |
|      - | 2374 | `	}` |
|      - | 2375 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2376 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2377 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2378 | `	 */` |
|     33 | 2379 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     16 | 2380 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2381 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2382 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2383 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2384 | `		)` |
|      - | 2385 | `	){` |
|      9 | 2386 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2387 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2388 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2389 | `			if( pInst && pInst->pClass ){` |
|      3 | 2390 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2391 | `			}` |
|      1 | 2392 | `		}` |
|     12 | 2393 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2394 | `			"TypeError",` |
|      - | 2395 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2396 | `			zType` |
|      - | 2397 | `			);` |
|      - | 2398 | `	}` |
|      - | 2399 | `	/* Extract the target string */` |
|     15 | 2400 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 2401 | `	if( nLen < 1 ){` |
|      - | 2402 | `		/* Empty string,return */` |
|      3 | 2403 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2404 | `		return PH7_OK;` |
|      - | 2405 | `	}` |
|      - | 2406 | `	/* Perform the requested operation */` |
|     13 | 2407 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|     13 | 2408 | `	return PH7_OK;` |
|     17 | 2409 |  |
|      - | 2410 |  |
|      - | 2411 | `/* Search callback signature */` |
|      - | 2412 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2413 | `/*` |
|      - | 2414 | ` * Case-insensitive pattern match.` |
|      - | 2415 | ` * Brute force is the default search method used here.` |
|      - | 2416 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2417 | ` * well for short/medium texts on modern hardware.` |
|      - | 2418 | ` */` |
|    118 | 2419 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2420 |  |
|    119 | 2421 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2422 | `	const char *zIn = (const char *)pText;` |
|    119 | 2423 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2424 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2425 | `	const char *zPtr,*zPtr2;` |
|      - | 2426 | `	int c,d;` |
|    119 | 2427 | `	if( iPatLen > nLen ){` |
|      - | 2428 | `		/* Don't bother processing */` |
|     33 | 2429 | `		return SXERR_NOTFOUND;` |
|      - | 2430 | `	}` |
|    244 | 2431 | `	for(;;){` |
|    489 | 2432 | `		if( zIn >= zEnd ){` |
|     47 | 2433 | `			break;` |
|      - | 2434 | `		}` |
|    443 | 2435 | `		c = SyToLower(zIn[0]);` |
|    443 | 2436 | `		d = SyToLower(zpIn[0]);` |
|    443 | 2437 | `		if( c == d ){` |
|     41 | 2438 | `			zPtr   = &zIn[1];` |
|     41 | 2439 | `			zPtr2  = &zpIn[1];` |
|     71 | 2440 | `			for(;;){` |
|    143 | 2441 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2442 | `					/* Pattern found */` |
|     41 | 2443 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2444 | `					return SXRET_OK;` |
|      - | 2445 | `				}` |
|    103 | 2446 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2447 | `					break;` |
|      - | 2448 | `				}` |
|    103 | 2449 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2450 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2451 | `				if( c != d ){` |
|    ! 0 | 2452 | `					break;` |
|      - | 2453 | `				}` |
|    103 | 2454 | `				zPtr++; zPtr2++;` |
|      1 | 2455 | `			}` |
|    ! 0 | 2456 | `		}` |
|    403 | 2457 | `		zIn++;` |
|      1 | 2458 | `	}` |
|      - | 2459 | `	/* Pattern not found */` |
|     47 | 2460 | `	return SXERR_NOTFOUND;` |
|     60 | 2461 |  |
|      - | 2462 | `/*` |
|      - | 2463 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2464 | ` *  Find the first occurrence of a string.` |
|      - | 2465 | ` * Parameters` |
|      - | 2466 | ` *  $haystack` |
|      - | 2467 | ` *   The input string.` |
|      - | 2468 | ` * $needle` |
|      - | 2469 | ` *   Search pattern (must be a string).` |
|      - | 2470 | ` * $before_needle` |
|      - | 2471 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2472 | ` *   of the needle (excluding the needle).` |
|      - | 2473 | ` * Return` |
|      - | 2474 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2475 | ` */` |
|     10 | 2476 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2477 |  |
|     11 | 2478 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2479 | `	const char *zBlob,*zPattern;` |
|      - | 2480 | `	int nLen,nPatLen;` |
|      - | 2481 | `	sxu32 nOfft;` |
|      - | 2482 | `	sxi32 rc;` |
|     11 | 2483 | `	if( nArg < 2 ){` |
|      - | 2484 | `		/* Missing arguments,return FALSE */` |
|      5 | 2485 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2486 | `		return PH7_OK;` |
|      - | 2487 | `	}` |
|      - | 2488 | `	/* Extract the needle and the haystack */` |
|      7 | 2489 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2490 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2491 | `	nOfft = 0; /* cc warning */` |
|      9 | 2492 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2493 | `		int before = 0;` |
|      - | 2494 | `		/* Perform the lookup */` |
|      5 | 2495 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2496 | `		if( rc != SXRET_OK ){` |
|      - | 2497 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2498 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2499 | `			return PH7_OK;` |
|      - | 2500 | `		}` |
|      - | 2501 | `		/* Return the portion of the string */` |
|      5 | 2502 | `		if( nArg > 2 ){` |
|      3 | 2503 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2504 | `		}` |
|      5 | 2505 | `		if( before ){` |
|      3 | 2506 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2507 | `		}else{` |
|      3 | 2508 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2509 | `		}` |
|      3 | 2510 | `	}else{` |
|      3 | 2511 | `		ph7_result_bool(pCtx,0);` |
|      - | 2512 | `	}` |
|      7 | 2513 | `	return PH7_OK;` |
|      6 | 2514 |  |
|      - | 2515 | `/*` |
|      - | 2516 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2517 | ` *  Case-insensitive strstr().` |
|      - | 2518 | ` * Parameters` |
|      - | 2519 | ` *  $haystack` |
|      - | 2520 | ` *   The input string.` |
|      - | 2521 | ` * $needle` |
|      - | 2522 | ` *   Search pattern (must be a string).` |
|      - | 2523 | ` * $before_needle` |
|      - | 2524 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2525 | ` *   of the needle (excluding the needle).` |
|      - | 2526 | ` * Return` |
|      - | 2527 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2528 | ` */` |
|      6 | 2529 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2530 |  |
|      7 | 2531 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2532 | `	const char *zBlob,*zPattern;` |
|      - | 2533 | `	int nLen,nPatLen;` |
|      - | 2534 | `	sxu32 nOfft;` |
|      - | 2535 | `	sxi32 rc;` |
|      7 | 2536 | `	if( nArg < 2 ){` |
|      - | 2537 | `		/* Missing arguments,return FALSE */` |
|      3 | 2538 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2539 | `		return PH7_OK;` |
|      - | 2540 | `	}` |
|      - | 2541 | `	/* Extract the needle and the haystack */` |
|      5 | 2542 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2543 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2544 | `	nOfft = 0; /* cc warning */` |
|      7 | 2545 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2546 | `		int before = 0;` |
|      - | 2547 | `		/* Perform the lookup */` |
|      5 | 2548 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2549 | `		if( rc != SXRET_OK ){` |
|      - | 2550 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2551 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2552 | `			return PH7_OK;` |
|      - | 2553 | `		}` |
|      - | 2554 | `		/* Return the portion of the string */` |
|      5 | 2555 | `		if( nArg > 2 ){` |
|      3 | 2556 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2557 | `		}` |
|      5 | 2558 | `		if( before ){` |
|      3 | 2559 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2560 | `		}else{` |
|      3 | 2561 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2562 | `		}` |
|      3 | 2563 | `	}else{` |
|    ! 0 | 2564 | `		ph7_result_bool(pCtx,0);` |
|      - | 2565 | `	}` |
|      5 | 2566 | `	return PH7_OK;` |
|      4 | 2567 |  |
|      - | 2568 | `/*` |
|      - | 2569 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2570 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2571 | ` * Parameters` |
|      - | 2572 | ` *  $haystack` |
|      - | 2573 | ` *   The input string.` |
|      - | 2574 | ` * $needle` |
|      - | 2575 | ` *   Search pattern (must be a string).` |
|      - | 2576 | ` * $offset` |
|      - | 2577 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2578 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2579 | ` *   of haystack.` |
|      - | 2580 | ` * Return` |
|      - | 2581 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2582 | ` */` |
|    122 | 2583 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2584 |  |
|    127 | 2585 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2586 | `	const char *zBlob,*zPattern;` |
|      - | 2587 | `	int nLen,nPatLen,nStart;` |
|      - | 2588 | `	sxu32 nOfft;` |
|      - | 2589 | `	sxi32 rc;` |
|    127 | 2590 | `	if( nArg < 2 ){` |
|      - | 2591 | `		/* Missing arguments,return FALSE */` |
|      7 | 2592 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2593 | `		return PH7_OK;` |
|      - | 2594 | `	}` |
|      - | 2595 | `	/* Extract the needle and the haystack */` |
|    121 | 2596 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    121 | 2597 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    121 | 2598 | `	nOfft = 0; /* cc warning */` |
|    121 | 2599 | `	nStart = 0;` |
|      - | 2600 | `	/* Peek the starting offset if available */` |
|    121 | 2601 | `	if( nArg > 2 ){` |
|    ! 0 | 2602 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2603 | `		if( nStart < 0 ){` |
|    ! 0 | 2604 | `			nStart = -nStart;` |
|    ! 0 | 2605 | `		}` |
|    ! 0 | 2606 | `		if( nStart >= nLen ){` |
|      - | 2607 | `			/* Invalid offset */` |
|    ! 0 | 2608 | `			nStart = 0;` |
|    ! 0 | 2609 | `		}else{` |
|    ! 0 | 2610 | `			zBlob += nStart;` |
|    ! 0 | 2611 | `			nLen -= nStart;` |
|      - | 2612 | `		}` |
|    ! 0 | 2613 | `	}` |
|    121 | 2614 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2615 | `		/* Perform the lookup */` |
|    119 | 2616 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    119 | 2617 | `		if( rc != SXRET_OK ){` |
|      - | 2618 | `			/* Pattern not found,return FALSE */` |
|     33 | 2619 | `			ph7_result_bool(pCtx,0);` |
|     33 | 2620 | `			return PH7_OK;` |
|      - | 2621 | `		}` |
|      - | 2622 | `		/* Return the pattern position */` |
|     88 | 2623 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     45 | 2624 | `	}else{` |
|      3 | 2625 | `		ph7_result_bool(pCtx,0);` |
|      - | 2626 | `	}` |
|     90 | 2627 | `	return PH7_OK;` |
|     66 | 2628 |  |
|      - | 2629 | `/*` |
|      - | 2630 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2631 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2632 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2633 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2634 | ` *` |
|      - | 2635 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2636 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2637 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2638 | ` *` |
|      - | 2639 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2640 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2641 | ` */` |
|    386 | 2642 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2643 | `	ph7_context *pCtx,` |
|      - | 2644 | `	ph7_value *pArg,` |
|      - | 2645 | `	const char *zFunc,` |
|      - | 2646 | `	int iArgNum,` |
|      - | 2647 | `	const char *zParamName,` |
|      - | 2648 | `	const char *zNullMsg,` |
|      - | 2649 | `	ph7_value *pTmp,` |
|      - | 2650 | `	const char **pzOut,` |
|      - | 2651 | `	int *pnOut` |
|      4 | 2652 | `){` |
|    390 | 2653 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2654 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2655 | `		*pzOut = "";` |
|     13 | 2656 | `		*pnOut = 0;` |
|     13 | 2657 | `		return PH7_OK;` |
|      - | 2658 | `	}` |
|    580 | 2659 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    356 | 2660 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2661 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2662 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2663 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2664 | `	    )` |
|      - | 2665 | `	){` |
|     34 | 2666 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2667 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2668 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2669 | `			if( pInst && pInst->pClass ){` |
|     13 | 2670 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2671 | `			}` |
|      6 | 2672 | `		}` |
|     49 | 2673 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2674 | `			"TypeError",` |
|      - | 2675 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2676 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2677 | `			);` |
|      - | 2678 | `	}` |
|    345 | 2679 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2680 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2681 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2682 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2683 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2684 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2685 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2686 | `		return PH7_OK;` |
|      - | 2687 | `	}` |
|    309 | 2688 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    309 | 2689 | `	return PH7_OK;` |
|    197 | 2690 |  |
|      - | 2691 | `/*` |
|      - | 2692 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2693 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2694 | ` * Return` |
|      - | 2695 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2696 | ` */` |
|     76 | 2697 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2698 |  |
|      - | 2699 | `	const char *zHaystack,*zNeedle;` |
|      - | 2700 | `	int nHayLen,nNeedleLen;` |
|      - | 2701 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2702 | `	sxi32 rc;` |
|     80 | 2703 | `	if( nArg != 2 ){` |
|     18 | 2704 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2705 | `			"ArgumentCountError",` |
|      - | 2706 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2707 | `			nArg` |
|      - | 2708 | `			);` |
|      - | 2709 | `	}` |
|     68 | 2710 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     68 | 2711 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     68 | 2712 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2713 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2714 | `		"of type string is deprecated",` |
|      - | 2715 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     68 | 2716 | `	if( rc != PH7_OK ) goto out;` |
|     61 | 2717 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2718 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2719 | `		"of type string is deprecated",` |
|      - | 2720 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     61 | 2721 | `	if( rc != PH7_OK ) goto out;` |
|     57 | 2722 | `	if( nNeedleLen < 1 ){` |
|     13 | 2723 | `		ph7_result_bool(pCtx,1);` |
|     51 | 2724 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2725 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2726 | `	}else{` |
|     55 | 2727 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     18 | 2728 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     37 | 2729 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2730 | `	}` |
|     57 | 2731 | `	rc = PH7_OK;` |
|     33 | 2732 | `out:` |
|     68 | 2733 | `	PH7_MemObjRelease(&sHayTmp);` |
|     68 | 2734 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     68 | 2735 | `	return rc;` |
|     42 | 2736 |  |
|      - | 2737 | `/*` |
|      - | 2738 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2739 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2740 | ` * Return` |
|      - | 2741 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2742 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2743 | ` */` |
|     78 | 2744 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2745 |  |
|      - | 2746 | `	const char *zHaystack,*zNeedle;` |
|      - | 2747 | `	int nHayLen,nNeedleLen;` |
|      - | 2748 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2749 | `	sxi32 rc;` |
|     82 | 2750 | `	if( nArg != 2 ){` |
|     18 | 2751 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2752 | `			"ArgumentCountError",` |
|      - | 2753 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2754 | `			nArg` |
|      - | 2755 | `			);` |
|      - | 2756 | `	}` |
|     70 | 2757 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2758 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2759 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2760 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2761 | `		"of type string is deprecated",` |
|      - | 2762 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2763 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2764 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2765 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2766 | `		"of type string is deprecated",` |
|      - | 2767 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2768 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2769 | `	if( nNeedleLen < 1 ){` |
|     13 | 2770 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2771 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2772 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2773 | `	}else{` |
|     58 | 2774 | `		ph7_result_bool(pCtx,` |
|     38 | 2775 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2776 | `	}` |
|     59 | 2777 | `	rc = PH7_OK;` |
|     34 | 2778 | `out:` |
|     70 | 2779 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2780 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2781 | `	return rc;` |
|     43 | 2782 |  |
|      - | 2783 | `/*` |
|      - | 2784 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2785 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2786 | ` * Return` |
|      - | 2787 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2788 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2789 | ` */` |
|     78 | 2790 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2791 |  |
|      - | 2792 | `	const char *zHaystack,*zNeedle;` |
|      - | 2793 | `	int nHayLen,nNeedleLen;` |
|      - | 2794 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2795 | `	sxi32 rc;` |
|     82 | 2796 | `	if( nArg != 2 ){` |
|     18 | 2797 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2798 | `			"ArgumentCountError",` |
|      - | 2799 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2800 | `			nArg` |
|      - | 2801 | `			);` |
|      - | 2802 | `	}` |
|     70 | 2803 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2804 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2805 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2806 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2807 | `		"of type string is deprecated",` |
|      - | 2808 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2809 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2810 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2811 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2812 | `		"of type string is deprecated",` |
|      - | 2813 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2814 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2815 | `	if( nNeedleLen < 1 ){` |
|     13 | 2816 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2817 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2818 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2819 | `	}else{` |
|     58 | 2820 | `		ph7_result_bool(pCtx,` |
|     38 | 2821 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2822 | `	}` |
|     59 | 2823 | `	rc = PH7_OK;` |
|     34 | 2824 | `out:` |
|     70 | 2825 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2826 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2827 | `	return rc;` |
|     43 | 2828 |  |
|      - | 2829 | `/*` |
|      - | 2830 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2831 | ` *  Case-insensitive strpos.` |
|      - | 2832 | ` * Parameters` |
|      - | 2833 | ` *  $haystack` |
|      - | 2834 | ` *   The input string.` |
|      - | 2835 | ` * $needle` |
|      - | 2836 | ` *   Search pattern (must be a string).` |
|      - | 2837 | ` * $offset` |
|      - | 2838 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2839 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2840 | ` *   of haystack.` |
|      - | 2841 | ` * Return` |
|      - | 2842 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2843 | ` */` |
|     18 | 2844 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2845 |  |
|     19 | 2846 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2847 | `	const char *zBlob,*zPattern;` |
|      - | 2848 | `	int nLen,nPatLen,nStart;` |
|      - | 2849 | `	sxu32 nOfft;` |
|      - | 2850 | `	sxi32 rc;` |
|     19 | 2851 | `	if( nArg < 2 ){` |
|      - | 2852 | `		/* Missing arguments,return FALSE */` |
|      3 | 2853 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2854 | `		return PH7_OK;` |
|      - | 2855 | `	}` |
|      - | 2856 | `	/* Extract the needle and the haystack */` |
|     17 | 2857 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2858 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2859 | `	nOfft = 0; /* cc warning */` |
|     17 | 2860 | `	nStart = 0;` |
|      - | 2861 | `	/* Peek the starting offset if available */` |
|     17 | 2862 | `	if( nArg > 2 ){` |
|      5 | 2863 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2864 | `		if( nStart < 0 ){` |
|      3 | 2865 | `			nStart = -nStart;` |
|      1 | 2866 | `		}` |
|      5 | 2867 | `		if( nStart >= nLen ){` |
|      - | 2868 | `			/* Invalid offset */` |
|    ! 0 | 2869 | `			nStart = 0;` |
|    ! 0 | 2870 | `		}else{` |
|      5 | 2871 | `			zBlob += nStart;` |
|      5 | 2872 | `			nLen -= nStart;` |
|      - | 2873 | `		}` |
|      2 | 2874 | `	}` |
|     17 | 2875 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2876 | `		/* Perform the lookup */` |
|     17 | 2877 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2878 | `		if( rc != SXRET_OK ){` |
|      - | 2879 | `			/* Pattern not found,return FALSE */` |
|      3 | 2880 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2881 | `			return PH7_OK;` |
|      - | 2882 | `		}` |
|      - | 2883 | `		/* Return the pattern position */` |
|     15 | 2884 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2885 | `	}else{` |
|    ! 0 | 2886 | `		ph7_result_bool(pCtx,0);` |
|      - | 2887 | `	}` |
|     15 | 2888 | `	return PH7_OK;` |
|     10 | 2889 |  |
|      - | 2890 | `/*` |
|      - | 2891 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2892 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2893 | ` * Parameters` |
|      - | 2894 | ` *  $haystack` |
|      - | 2895 | ` *   The input string.` |
|      - | 2896 | ` * $needle` |
|      - | 2897 | ` *   Search pattern (must be a string).` |
|      - | 2898 | ` * $offset` |
|      - | 2899 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2900 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2901 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2902 | ` * Return` |
|      - | 2903 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2904 | ` */` |
|     32 | 2905 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2906 |  |
|      - | 2907 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2908 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2909 | `	int nLen,nPatLen;` |
|      - | 2910 | `	sxu32 nOfft;` |
|      - | 2911 | `	sxi32 rc;` |
|     33 | 2912 | `	if( nArg < 2 ){` |
|      - | 2913 | `		/* Missing arguments,return FALSE */` |
|      3 | 2914 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2915 | `		return PH7_OK;` |
|      - | 2916 | `	}` |
|      - | 2917 | `	/* Extract the needle and the haystack */` |
|     31 | 2918 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2919 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2920 | `	/* Point to the end of the pattern */` |
|     31 | 2921 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2922 | `	zEnd = &zBlob[nLen];` |
|      - | 2923 | `	/* Save the starting posistion */` |
|     31 | 2924 | `	zStart = zBlob;` |
|     31 | 2925 | `	nOfft = 0; /* cc warning */` |
|      - | 2926 | `	/* Peek the starting offset if available */` |
|     31 | 2927 | `	if( nArg > 2 ){` |
|      - | 2928 | `		int nStart;` |
|     21 | 2929 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2930 | `		if( nStart < 0 ){` |
|     11 | 2931 | `			nStart = -nStart;` |
|     11 | 2932 | `			if( nStart >= nLen ){` |
|      - | 2933 | `				/* Invalid offset */` |
|      3 | 2934 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2935 | `				return PH7_OK;` |
|    ! 0 | 2936 | `			}else{` |
|      9 | 2937 | `				nLen -= nStart;` |
|      9 | 2938 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2939 | `				zEnd = &zBlob[nLen];` |
|      - | 2940 | `			}` |
|      5 | 2941 | `		}else{` |
|     11 | 2942 | `			if( nStart >= nLen ){` |
|      - | 2943 | `				/* Invalid offset */` |
|      5 | 2944 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2945 | `				return PH7_OK;` |
|    ! 0 | 2946 | `			}else{` |
|      7 | 2947 | `				zBlob += nStart;` |
|      7 | 2948 | `				nLen -= nStart;` |
|      - | 2949 | `			}` |
|      - | 2950 | `		}` |
|      7 | 2951 | `	}` |
|     25 | 2952 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2953 | `		/* Perform the lookup */` |
|     57 | 2954 | `		for(;;){` |
|    115 | 2955 | `			if( zBlob >= zPtr ){` |
|     11 | 2956 | `				break;` |
|      - | 2957 | `			}` |
|    105 | 2958 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 2959 | `			if( rc == SXRET_OK ){` |
|      - | 2960 | `				/* Pattern found,return it's position */` |
|     13 | 2961 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 2962 | `				return PH7_OK;` |
|      - | 2963 | `			}` |
|     93 | 2964 | `			zPtr--;` |
|      1 | 2965 | `		}` |
|      - | 2966 | `		/* Pattern not found,return FALSE */` |
|     11 | 2967 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2968 | `	}else{` |
|      3 | 2969 | `		ph7_result_bool(pCtx,0);` |
|      - | 2970 | `	}` |
|     13 | 2971 | `	return PH7_OK;` |
|     17 | 2972 |  |
|      - | 2973 | `/*` |
|      - | 2974 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2975 | ` *  Case-insensitive strrpos.` |
|      - | 2976 | ` * Parameters` |
|      - | 2977 | ` *  $haystack` |
|      - | 2978 | ` *   The input string.` |
|      - | 2979 | ` * $needle` |
|      - | 2980 | ` *   Search pattern (must be a string).` |
|      - | 2981 | ` * $offset` |
|      - | 2982 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2983 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2984 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2985 | ` * Return` |
|      - | 2986 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2987 | ` */` |
|     28 | 2988 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2989 |  |
|      - | 2990 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 2991 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2992 | `	int nLen,nPatLen;` |
|      - | 2993 | `	sxu32 nOfft;` |
|      - | 2994 | `	sxi32 rc;` |
|     29 | 2995 | `	if( nArg < 2 ){` |
|      - | 2996 | `		/* Missing arguments,return FALSE */` |
|      3 | 2997 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2998 | `		return PH7_OK;` |
|      - | 2999 | `	}` |
|      - | 3000 | `	/* Extract the needle and the haystack */` |
|     27 | 3001 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3002 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3003 | `	/* Point to the end of the pattern */` |
|     27 | 3004 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3005 | `	zEnd = &zBlob[nLen];` |
|      - | 3006 | `	/* Save the starting posistion */` |
|     27 | 3007 | `	zStart = zBlob;` |
|     27 | 3008 | `	nOfft = 0; /* cc warning */` |
|      - | 3009 | `	/* Peek the starting offset if available */` |
|     27 | 3010 | `	if( nArg > 2 ){` |
|      - | 3011 | `		int nStart;` |
|     15 | 3012 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3013 | `		if( nStart < 0 ){` |
|      7 | 3014 | `			nStart = -nStart;` |
|      7 | 3015 | `			if( nStart >= nLen ){` |
|      - | 3016 | `				/* Invalid offset */` |
|      3 | 3017 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3018 | `				return PH7_OK;` |
|    ! 0 | 3019 | `			}else{` |
|      5 | 3020 | `				nLen -= nStart;` |
|      5 | 3021 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3022 | `				zEnd = &zBlob[nLen];` |
|      - | 3023 | `			}` |
|      3 | 3024 | `		}else{` |
|      9 | 3025 | `			if( nStart >= nLen ){` |
|      - | 3026 | `				/* Invalid offset */` |
|      5 | 3027 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3028 | `				return PH7_OK;` |
|    ! 0 | 3029 | `			}else{` |
|      5 | 3030 | `				zBlob += nStart;` |
|      5 | 3031 | `				nLen -= nStart;` |
|      - | 3032 | `			}` |
|      - | 3033 | `		}` |
|      4 | 3034 | `	}` |
|     21 | 3035 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3036 | `		/* Perform the lookup */` |
|     44 | 3037 | `		for(;;){` |
|     89 | 3038 | `			if( zBlob >= zPtr ){` |
|      9 | 3039 | `				break;` |
|      - | 3040 | `			}` |
|     81 | 3041 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3042 | `			if( rc == SXRET_OK ){` |
|      - | 3043 | `				/* Pattern found,return it's position */` |
|     11 | 3044 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3045 | `				return PH7_OK;` |
|      - | 3046 | `			}` |
|     71 | 3047 | `			zPtr--;` |
|      1 | 3048 | `		}` |
|      - | 3049 | `		/* Pattern not found,return FALSE */` |
|      9 | 3050 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3051 | `	}else{` |
|      3 | 3052 | `		ph7_result_bool(pCtx,0);` |
|      - | 3053 | `	}` |
|     11 | 3054 | `	return PH7_OK;` |
|     15 | 3055 |  |
|      - | 3056 | `/*` |
|      - | 3057 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3058 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3059 | ` * Parameters` |
|      - | 3060 | ` *  $haystack` |
|      - | 3061 | ` *   The input string.` |
|      - | 3062 | ` * $needle` |
|      - | 3063 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3064 | ` *  This behavior is different from that of strstr().` |
|      - | 3065 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3066 | ` *  as the ordinal value of a character.` |
|      - | 3067 | ` * Return` |
|      - | 3068 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3069 | ` */` |
|     24 | 3070 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3071 |  |
|      - | 3072 | `	const char *zBlob;` |
|      - | 3073 | `	int nLen,c;` |
|     25 | 3074 | `	if( nArg < 2 ){` |
|      - | 3075 | `		/* Missing arguments,return FALSE */` |
|      3 | 3076 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3077 | `		return PH7_OK;` |
|      - | 3078 | `	}` |
|      - | 3079 | `	/* Extract the haystack */` |
|     23 | 3080 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3081 | `	c = 0; /* cc warning */` |
|     23 | 3082 | `	if( nLen > 0 ){` |
|      - | 3083 | `		sxu32 nOfft;` |
|      - | 3084 | `		sxi32 rc;` |
|     21 | 3085 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3086 | `			const char *zPattern;` |
|     11 | 3087 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3088 | `														 * for NULL pointer.` |
|      - | 3089 | `														 */` |
|     11 | 3090 | `			c = zPattern[0];` |
|      6 | 3091 | `		}else{` |
|      - | 3092 | `			/* Int cast */` |
|     11 | 3093 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3094 | `		}` |
|      - | 3095 | `		/* Perform the lookup */` |
|     21 | 3096 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3097 | `		if( rc != SXRET_OK ){` |
|      - | 3098 | `			/* No such entry,return FALSE */` |
|      7 | 3099 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3100 | `			return PH7_OK;` |
|      - | 3101 | `		}` |
|      - | 3102 | `		/* Return the string portion */` |
|     15 | 3103 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3104 | `	}else{` |
|      3 | 3105 | `		ph7_result_bool(pCtx,0);` |
|      - | 3106 | `	}` |
|     17 | 3107 | `	return PH7_OK;` |
|     13 | 3108 |  |
|      - | 3109 | `/*` |
|      - | 3110 | ` * string strrev(string $string)` |
|      - | 3111 | ` *  Reverse a string.` |
|      - | 3112 | ` * Parameters` |
|      - | 3113 | ` *  $string` |
|      - | 3114 | ` *   String to be reversed.` |
|      - | 3115 | ` * Return` |
|      - | 3116 | ` *  The reversed string.` |
|      - | 3117 | ` */` |
|      4 | 3118 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3119 |  |
|      - | 3120 | `	const char *zIn,*zEnd;` |
|      - | 3121 | `	int nLen,c;` |
|      5 | 3122 | `	if( nArg < 1 ){` |
|      - | 3123 | `		/* Missing arguments,return NULL */` |
|      3 | 3124 | `		ph7_result_null(pCtx);` |
|      3 | 3125 | `		return PH7_OK;` |
|      - | 3126 | `	}` |
|      - | 3127 | `	/* Extract the target string */` |
|      3 | 3128 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3129 | `	if( nLen < 1 ){` |
|      - | 3130 | `		/* Empty string Return null */` |
|    ! 0 | 3131 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3132 | `		return PH7_OK;` |
|      - | 3133 | `	}` |
|      - | 3134 | `	/* Perform the requested operation */` |
|      3 | 3135 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3136 | `	for(;;){` |
|      9 | 3137 | `		if( zEnd < zIn ){` |
|      - | 3138 | `			/* No more input to process */` |
|      3 | 3139 | `			break;` |
|      - | 3140 | `		}` |
|      - | 3141 | `		/* Append current character */` |
|      7 | 3142 | `		c = zEnd[0];` |
|      7 | 3143 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3144 | `		zEnd--;` |
|      1 | 3145 | `	}` |
|      3 | 3146 | `	return PH7_OK;` |
|      3 | 3147 |  |
|      - | 3148 | `/*` |
|      - | 3149 | ` * string ucwords(string $string)` |
|      - | 3150 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3151 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3152 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3153 | ` * Parameters` |
|      - | 3154 | ` *  $string` |
|      - | 3155 | ` *   The input string.` |
|      - | 3156 | ` * Return` |
|      - | 3157 | ` *  The modified string..` |
|      - | 3158 | ` */` |
|     14 | 3159 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3160 |  |
|      - | 3161 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3162 | `	int nLen,c;` |
|     15 | 3163 | `	if( nArg < 1 ){` |
|      - | 3164 | `		/* Missing arguments,return NULL */` |
|      3 | 3165 | `		ph7_result_null(pCtx);` |
|      3 | 3166 | `		return PH7_OK;` |
|      - | 3167 | `	}` |
|      - | 3168 | `	/* Extract the target string */` |
|     13 | 3169 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3170 | `	if( nLen < 1 ){` |
|      - | 3171 | `		/* Empty string – match PHP semantics */` |
|      3 | 3172 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3173 | `		return PH7_OK;` |
|      - | 3174 | `	}` |
|      - | 3175 | `	/* Perform the requested operation */` |
|     11 | 3176 | `	zEnd = &zIn[nLen];` |
|     21 | 3177 | `	for(;;){` |
|      - | 3178 | `		/* Jump leading white spaces */` |
|     43 | 3179 | `		zCur = zIn;` |
|     65 | 3180 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3181 | `			zIn++;` |
|      1 | 3182 | `		}` |
|     43 | 3183 | `		if( zCur < zIn ){` |
|      - | 3184 | `			/* Append white space stream */` |
|     23 | 3185 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3186 | `		}` |
|     43 | 3187 | `		if( zIn >= zEnd ){` |
|      - | 3188 | `			/* No more input to process */` |
|     11 | 3189 | `			break;` |
|      - | 3190 | `		}` |
|     33 | 3191 | `		c = zIn[0];` |
|     33 | 3192 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3193 | `			c = SyToUpper(c);` |
|     14 | 3194 | `		}` |
|      - | 3195 | `		/* Append the upper-cased character */` |
|     33 | 3196 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3197 | `		zIn++;` |
|     33 | 3198 | `		zCur = zIn;` |
|      - | 3199 | `		/* Append the word varbatim */` |
|    149 | 3200 | `		while( zIn < zEnd ){` |
|    139 | 3201 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3202 | `				/* UTF-8 stream */` |
|    ! 0 | 3203 | `				zIn++;` |
|    ! 0 | 3204 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3205 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3206 | `				zIn++;` |
|     59 | 3207 | `			}else{` |
|     23 | 3208 | `				break;` |
|      - | 3209 | `			}` |
|      1 | 3210 | `		}` |
|     33 | 3211 | `		if( zCur < zIn ){` |
|     33 | 3212 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3213 | `		}` |
|      1 | 3214 | `	}` |
|     11 | 3215 | `	return PH7_OK;` |
|      8 | 3216 |  |
|      - | 3217 | `/*` |
|      - | 3218 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3219 | ` *  Returns input repeated multiplier times.` |
|      - | 3220 | ` * Parameters` |
|      - | 3221 | ` *  $string` |
|      - | 3222 | ` *   String to be repeated.` |
|      - | 3223 | ` * $multiplier` |
|      - | 3224 | ` *  Number of time the input string should be repeated.` |
|      - | 3225 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3226 | ` *  to 0, the function will return an empty string.` |
|      - | 3227 | ` * Return` |
|      - | 3228 | ` *  The repeated string.` |
|      - | 3229 | ` */` |
|  20220 | 3230 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3231 |  |
|      - | 3232 | `	const char *zIn;` |
|      - | 3233 | `	int nLen,nMul;` |
|      - | 3234 | `	int rc;` |
|  20221 | 3235 | `	if( nArg < 2 ){` |
|      - | 3236 | `		/* Missing arguments,return NULL */` |
|      3 | 3237 | `		ph7_result_null(pCtx);` |
|      3 | 3238 | `		return PH7_OK;` |
|      - | 3239 | `	}` |
|      - | 3240 | `	/* Extract the target string */` |
|  20219 | 3241 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20219 | 3242 | `	if( nLen < 1 ){` |
|      - | 3243 | `		/* Empty string.Return null */` |
|    ! 0 | 3244 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3245 | `		return PH7_OK;` |
|      - | 3246 | `	}` |
|      - | 3247 | `	/* Extract the multiplier */` |
|  20219 | 3248 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20219 | 3249 | `	if( nMul < 1 ){` |
|      - | 3250 | `		/* Return the empty string */` |
|      3 | 3251 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3252 | `		return PH7_OK;` |
|      - | 3253 | `	}` |
|      - | 3254 | `	/* Perform the requested operation */` |
| 120691 | 3255 | `	for(;;){` |
| 241383 | 3256 | `		if( !nMul ){` |
|  20217 | 3257 | `			break;` |
|      - | 3258 | `		}` |
|      - | 3259 | `		/* Append the copy */` |
| 221167 | 3260 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 221167 | 3261 | `		if( rc != PH7_OK ){` |
|      - | 3262 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3263 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3264 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3265 | `		}` |
| 221167 | 3266 | `		nMul--;` |
|      1 | 3267 | `	}` |
|  20217 | 3268 | `	return PH7_OK;` |
|  10111 | 3269 |  |
|      - | 3270 | `/*` |
|      - | 3271 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3272 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3273 | ` * Parameters` |
|      - | 3274 | ` *  $string` |
|      - | 3275 | ` *   The input string.` |
|      - | 3276 | ` * $is_xhtml` |
|      - | 3277 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3278 | ` * Return` |
|      - | 3279 | ` *  The processed string.` |
|      - | 3280 | ` */` |
|      6 | 3281 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3282 |  |
|      - | 3283 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3284 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3285 | `	int nLen;` |
|      7 | 3286 | `	if( nArg < 1 ){` |
|      - | 3287 | `		/* Missing arguments,return the empty string */` |
|      3 | 3288 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3289 | `		return PH7_OK;` |
|      - | 3290 | `	}` |
|      - | 3291 | `	/* Extract the target string */` |
|      5 | 3292 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3293 | `	if( nLen < 1 ){` |
|      - | 3294 | `		/* Empty string,return null */` |
|    ! 0 | 3295 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3296 | `		return PH7_OK;` |
|      - | 3297 | `	}` |
|      5 | 3298 | `	if( nArg > 1 ){` |
|      3 | 3299 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3300 | `	}` |
|      5 | 3301 | `	zEnd = &zIn[nLen];` |
|      - | 3302 | `	/* Perform the requested operation */` |
|      4 | 3303 | `	for(;;){` |
|      9 | 3304 | `		zCur = zIn;` |
|      - | 3305 | `		/* Delimit the string */` |
|     21 | 3306 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3307 | `			zIn++;` |
|      1 | 3308 | `		}` |
|      9 | 3309 | `		if( zCur < zIn ){` |
|      - | 3310 | `			/* Output chunk verbatim */` |
|      9 | 3311 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3312 | `		}` |
|      9 | 3313 | `		if( zIn >= zEnd ){` |
|      - | 3314 | `			/* No more input to process */` |
|      5 | 3315 | `			break;` |
|      - | 3316 | `		}` |
|      - | 3317 | `		/* Output the HTML line break */` |
|      - | 3318 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3319 | `		if( is_xhtml ){` |
|      3 | 3320 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3321 | `		}else{` |
|      3 | 3322 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3323 | `		}` |
|      5 | 3324 | `		zCur = zIn;` |
|      - | 3325 | `		/* Append trailing line */` |
|     11 | 3326 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3327 | `			zIn++;` |
|      1 | 3328 | `		}` |
|      5 | 3329 | `		if( zCur < zIn ){` |
|      - | 3330 | `			/* Output chunk verbatim */` |
|      5 | 3331 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3332 | `		}` |
|      1 | 3333 | `	}` |
|      5 | 3334 | `	return PH7_OK;` |
|      4 | 3335 |  |
|      - | 3336 | `/*` |
|      - | 3337 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3338 | ` *  According to the PHP reference manual.` |
|      - | 3339 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3340 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3341 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3342 | ` * This applies to both sprintf() and printf().` |
|      - | 3343 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3344 | ` * or more of these elements, in order:` |
|      - | 3345 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3346 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3347 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3348 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3349 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3350 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3351 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3352 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3353 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3354 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3355 | ` *   should result in.` |
|      - | 3356 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3357 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3358 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3359 | ` *   limit to the string.` |
|      - | 3360 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3361 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3362 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3363 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3364 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3365 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3366 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3367 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3368 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3369 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3370 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3371 | ` *       g - shorter of %e and %f.` |
|      - | 3372 | ` *       G - shorter of %E and %f.` |
|      - | 3373 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3374 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3375 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3376 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3377 | ` */` |
|      - | 3378 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3379 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3380 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3381 | `/*` |
|      - | 3382 | `** Conversion types fall into various categories as defined by the` |
|      - | 3383 | `** following enumeration.` |
|      - | 3384 | `*/` |
|      - | 3385 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3386 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3387 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3388 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3389 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3390 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3391 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3392 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3393 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3394 |  |
|      - | 3395 | `/*` |
|      - | 3396 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3397 | `*/` |
|      - | 3398 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3399 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3400 | `/*` |
|      - | 3401 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3402 | `** by an instance of the following structure` |
|      - | 3403 | `*/` |
|      - | 3404 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3405 | `struct ph7_fmt_info` |
|      - | 3406 |  |
|      - | 3407 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3408 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3409 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3410 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3411 | `  char *charset; /* The character set for conversion */` |
|      - | 3412 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3413 | `};` |
|      - | 3414 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3415 | `/*` |
|      - | 3416 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3417 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3418 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3419 | `**` |
|      - | 3420 | `** Example:` |
|      - | 3421 | `**     input:     *val = 3.14159` |
|      - | 3422 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3423 | `**` |
|      - | 3424 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3425 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3426 | `** always returned.` |
|      - | 3427 | `*/` |
|    422 | 3428 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3429 |  |
|      - | 3430 | `  sxlongreal d;` |
|      - | 3431 | `  int digit;` |
|      - | 3432 |  |
|    423 | 3433 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3434 | `	  return '0';` |
|      - | 3435 | `  }` |
|    423 | 3436 | `  digit = (int)*val;` |
|    423 | 3437 | `  d = digit;` |
|    423 | 3438 | `   *val = (*val - d)*10.0;` |
|    423 | 3439 | `  return digit + '0' ;` |
|    212 | 3440 |  |
|      - | 3441 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3442 | `/*` |
|      - | 3443 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3444 | ` * used conversion types first.` |
|      - | 3445 | ` */` |
|      - | 3446 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3447 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3448 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3449 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3450 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3451 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3452 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3453 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3454 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3455 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3456 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3457 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3458 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3459 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3460 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3461 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3462 | `};` |
|      - | 3463 | `/*` |
|      - | 3464 | ` * Format a given string.` |
|      - | 3465 | ` * The root program.  All variations call this core.` |
|      - | 3466 | ` * INPUTS:` |
|      - | 3467 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3468 | ` *            1. A pointer to the call context.` |
|      - | 3469 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3470 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3471 | ` *            3. An integer number of characters to be output.` |
|      - | 3472 | ` *               (Note: This number might be zero.)` |
|      - | 3473 | ` *            4. Upper layer private data.` |
|      - | 3474 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3475 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3476 | ` */` |
|    136 | 3477 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3478 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3479 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3480 | `	const char *zIn,    /* Format string */` |
|      - | 3481 | `	int nByte,          /* Format string length */` |
|      - | 3482 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3483 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3484 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3485 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3486 | `	)` |
|      1 | 3487 |  |
|    137 | 3488 | `	char spaces[] = "                                                  ";` |
|      - | 3489 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    137 | 3490 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3491 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3492 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3493 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3494 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3495 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3496 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3497 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3498 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3499 | `	ph7_int64 iVal;` |
|      - | 3500 | `	int precision;           /* Precision of the current field */` |
|      - | 3501 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3502 | `	int c,rc,n;` |
|      - | 3503 | `	int length;              /* Length of the field */` |
|      - | 3504 | `	int prefix;` |
|      - | 3505 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3506 | `	int width;               /* Width of the current field */` |
|      - | 3507 | `	int idx;` |
|    137 | 3508 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3509 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3510 | `	/* Start the format process */` |
|    139 | 3511 | `	for(;;){` |
|    279 | 3512 | `		zCur = zIn;` |
|    739 | 3513 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    461 | 3514 | `			zIn++;` |
|      1 | 3515 | `		}` |
|    279 | 3516 | `		if( zCur < zIn ){` |
|      - | 3517 | `			/* Consume chunk verbatim */` |
|    105 | 3518 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    105 | 3519 | `			if( rc == SXERR_ABORT ){` |
|      - | 3520 | `				/* Callback request an operation abort */` |
|    ! 0 | 3521 | `				break;` |
|      - | 3522 | `			}` |
|     52 | 3523 | `		}` |
|    279 | 3524 | `		if( zIn >= zEnd ){` |
|      - | 3525 | `			/* No more input to process,break immediately */` |
|    135 | 3526 | `			break;` |
|      - | 3527 | `		}` |
|      - | 3528 | `		/* Find out what flags are present */` |
|    145 | 3529 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    144 | 3530 | `			flag_alternateform = flag_zeropad = 0;` |
|    145 | 3531 | `		zIn++; /* Jump the precent sign */` |
|     72 | 3532 | `		do{` |
|    177 | 3533 | `			c = zIn[0];` |
|    177 | 3534 | `			switch( c ){` |
|      9 | 3535 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3536 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3537 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3538 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3539 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3540 | `			case '\'':` |
|    ! 0 | 3541 | `				zIn++;` |
|    ! 0 | 3542 | `				if( zIn < zEnd ){` |
|      - | 3543 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3544 | `					c = zIn[0];` |
|    ! 0 | 3545 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3546 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3547 | `					}` |
|    ! 0 | 3548 | `					c = 0;` |
|    ! 0 | 3549 | `				}` |
|    ! 0 | 3550 | `				break;` |
|    144 | 3551 | `			default:                                       break;` |
|      - | 3552 | `			}` |
|    177 | 3553 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3554 | `		/* Get the field width */` |
|    145 | 3555 | `		width = 0;` |
|    251 | 3556 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3557 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3558 | `			zIn++;` |
|      1 | 3559 | `		}` |
|    145 | 3560 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3561 | `			/* Position specifer */` |
|    ! 0 | 3562 | `			if( width > 0 ){` |
|    ! 0 | 3563 | `				n = width;` |
|    ! 0 | 3564 | `				if( vf && n > 0 ){` |
|    ! 0 | 3565 | `					n--;` |
|    ! 0 | 3566 | `				}` |
|    ! 0 | 3567 | `			}` |
|    ! 0 | 3568 | `			zIn++;` |
|    ! 0 | 3569 | `			width = 0;` |
|    ! 0 | 3570 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3571 | `				flag_zeropad = 1;` |
|    ! 0 | 3572 | `				zIn++;` |
|    ! 0 | 3573 | `			}` |
|    ! 0 | 3574 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3575 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3576 | `				zIn++;` |
|    ! 0 | 3577 | `			}` |
|    ! 0 | 3578 | `		}` |
|    145 | 3579 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3580 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3581 | `		}` |
|      - | 3582 | `		/* Get the precision */` |
|    145 | 3583 | `		precision = -1;` |
|    145 | 3584 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3585 | `			precision = 0;` |
|     59 | 3586 | `			zIn++;` |
|    150 | 3587 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3588 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3589 | `				zIn++;` |
|      1 | 3590 | `			}` |
|     29 | 3591 | `		}` |
|    145 | 3592 | `		if( zIn >= zEnd ){` |
|      - | 3593 | `			/* No more input */` |
|      3 | 3594 | `			break;` |
|      - | 3595 | `		}` |
|      - | 3596 | `		/* Fetch the info entry for the field */` |
|    143 | 3597 | `		pInfo = 0;` |
|    143 | 3598 | `		xtype = PH7_FMT_ERROR;` |
|    143 | 3599 | `		c = zIn[0];` |
|    143 | 3600 | `		zIn++; /* Jump the format specifer */` |
|    787 | 3601 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    785 | 3602 | `			if( c==aFmt[idx].fmttype ){` |
|    141 | 3603 | `				pInfo = &aFmt[idx];` |
|    141 | 3604 | `				xtype = pInfo->type;` |
|    141 | 3605 | `				break;` |
|      - | 3606 | `			}` |
|    323 | 3607 | `		}` |
|    143 | 3608 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    143 | 3609 | `		length = 0;` |
|      - | 3610 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3611 | `		 /*` |
|      - | 3612 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3613 | `		  **` |
|      - | 3614 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3615 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3616 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3617 | `		  **                               field width was negative.` |
|      - | 3618 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3619 | `		  **                               the conversion character.` |
|      - | 3620 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3621 | `		  **   width                       The specified field width.  This is` |
|      - | 3622 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3623 | `		  **   precision                   The specified precision.  The default` |
|      - | 3624 | `		  **                               is -1.` |
|      - | 3625 | `		  */` |
|    143 | 3626 | `		switch(xtype){` |
|    ! 0 | 3627 | `		case PH7_FMT_PERCENT:` |
|      - | 3628 | `			/* A literal percent character */` |
|    ! 0 | 3629 | `			zWorker[0] = '%';` |
|    ! 0 | 3630 | `			length = (int)sizeof(char);` |
|    ! 0 | 3631 | `			break;` |
|      3 | 3632 | `		case PH7_FMT_CHARX:` |
|      - | 3633 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3634 | `			 * with that ASCII value` |
|      - | 3635 | `			 */` |
|      7 | 3636 | `			pArg = NEXT_ARG;` |
|      7 | 3637 | `			if( pArg == 0 ){` |
|      3 | 3638 | `				c = 0;` |
|      2 | 3639 | `			}else{` |
|      5 | 3640 | `				c = ph7_value_to_int(pArg);` |
|      - | 3641 | `			}` |
|      - | 3642 | `			/* NUL byte is an acceptable value */` |
|      7 | 3643 | `			zWorker[0] = (char)c;` |
|      7 | 3644 | `			length = (int)sizeof(char);` |
|      7 | 3645 | `			break;` |
|     12 | 3646 | `		case PH7_FMT_STRING:` |
|      - | 3647 | `			/* the argument is treated as and presented as a string */` |
|     25 | 3648 | `			pArg = NEXT_ARG;` |
|     25 | 3649 | `			if( pArg == 0 ){` |
|    ! 0 | 3650 | `				length = 0;` |
|    ! 0 | 3651 | `			}else{` |
|     25 | 3652 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3653 | `			}` |
|     25 | 3654 | `			if( length < 1 ){` |
|    ! 0 | 3655 | `				zBuf = " ";` |
|    ! 0 | 3656 | `				length = (int)sizeof(char);` |
|    ! 0 | 3657 | `			}` |
|     25 | 3658 | `			if( precision>=0 && precision<length ){` |
|      3 | 3659 | `				length = precision;` |
|      1 | 3660 | `			}` |
|     25 | 3661 | `			if( flag_zeropad ){` |
|      - | 3662 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3663 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3664 | `					spaces[idx] = '0';` |
|    ! 0 | 3665 | `				}` |
|    ! 0 | 3666 | `			}` |
|     25 | 3667 | `			break;` |
|     27 | 3668 | `		case PH7_FMT_RADIX:` |
|     55 | 3669 | `			pArg = NEXT_ARG;` |
|     55 | 3670 | `			if( pArg == 0 ){` |
|    ! 0 | 3671 | `				iVal = 0;` |
|    ! 0 | 3672 | `			}else{` |
|     55 | 3673 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3674 | `			}` |
|      - | 3675 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     55 | 3676 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3677 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3678 | `			}` |
|      - | 3679 | `#if 1` |
|      - | 3680 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3681 | `        ** I think this is stupid.*/` |
|     55 | 3682 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3683 | `#else` |
|      - | 3684 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3685 | `        ** but leave the prefix for hex.*/` |
|      - | 3686 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3687 | `#endif` |
|     55 | 3688 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     25 | 3689 | `          if( iVal<0 ){` |
|      3 | 3690 | `            iVal = -iVal;` |
|      - | 3691 | `			/* Ticket 1433-003 */` |
|      3 | 3692 | `			if( iVal < 0 ){` |
|      - | 3693 | `				/* Overflow */` |
|    ! 0 | 3694 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3695 | `			}` |
|      3 | 3696 | `            prefix = '-';` |
|     24 | 3697 | `          }else if( flag_plussign )  prefix = '+';` |
|     21 | 3698 | `          else if( flag_blanksign )  prefix = ' ';` |
|     19 | 3699 | `          else                       prefix = 0;` |
|     13 | 3700 | `        }else{` |
|     31 | 3701 | `			if( iVal<0 ){` |
|    ! 0 | 3702 | `				iVal = -iVal;` |
|      - | 3703 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3704 | `				if( iVal < 0 ){` |
|      - | 3705 | `					/* Overflow */` |
|    ! 0 | 3706 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3707 | `				}` |
|    ! 0 | 3708 | `			}` |
|     31 | 3709 | `			prefix = 0;` |
|      - | 3710 | `		}` |
|     55 | 3711 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3712 | `          precision = width-(prefix!=0);` |
|      3 | 3713 | `        }` |
|     55 | 3714 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3715 | `        {` |
|      - | 3716 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3717 | `          register int base;` |
|     55 | 3718 | `          cset = pInfo->charset;` |
|     55 | 3719 | `          base = pInfo->base;` |
|     27 | 3720 | `          do{                                           /* Convert to ascii */` |
|    123 | 3721 | `            *(--zBuf) = cset[iVal%base];` |
|    123 | 3722 | `            iVal = iVal/base;` |
|    123 | 3723 | `          }while( iVal>0 );` |
|      - | 3724 | `        }` |
|     55 | 3725 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     77 | 3726 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3727 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3728 | `        }` |
|     55 | 3729 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     55 | 3730 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3731 | `          char *pre, x;` |
|      9 | 3732 | `          pre = pInfo->prefix;` |
|      9 | 3733 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3734 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3735 | `          }` |
|      4 | 3736 | `        }` |
|     55 | 3737 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 3738 | `		break;` |
|     28 | 3739 | `		case PH7_FMT_FLOAT:` |
|      - | 3740 | `		case PH7_FMT_EXP:` |
|      - | 3741 | `		case PH7_FMT_GENERIC:{` |
|      - | 3742 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3743 | `		long double realvalue;` |
|      - | 3744 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3745 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3746 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3747 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3748 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3749 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3750 | `		pArg = NEXT_ARG;` |
|     57 | 3751 | `		if( pArg == 0 ){` |
|    ! 0 | 3752 | `			realvalue = 0;` |
|    ! 0 | 3753 | `		}else{` |
|     57 | 3754 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3755 | `		}` |
|      - | 3756 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3757 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3758 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3759 | `			zBuf = "NAN";` |
|    ! 0 | 3760 | `			length = 3;` |
|    ! 0 | 3761 | `			break;` |
|      - | 3762 | `		}` |
|     57 | 3763 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3764 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3765 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3766 | `				zBuf = "-INF";` |
|    ! 0 | 3767 | `				length = 4;` |
|    ! 0 | 3768 | `			}else{` |
|    ! 0 | 3769 | `				zBuf = "INF";` |
|    ! 0 | 3770 | `				length = 3;` |
|      - | 3771 | `			}` |
|    ! 0 | 3772 | `			break;` |
|      - | 3773 | `		}` |
|     57 | 3774 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3775 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3776 | `        if( realvalue<0.0 ){` |
|      3 | 3777 | `          realvalue = -realvalue;` |
|      3 | 3778 | `          prefix = '-';` |
|      2 | 3779 | `        }else{` |
|     55 | 3780 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3781 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3782 | `          else                         prefix = 0;` |
|      - | 3783 | `        }` |
|     57 | 3784 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3785 | `        rounder = 0.0;` |
|      - | 3786 | `#if 0` |
|      - | 3787 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3788 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3789 | `#else` |
|      - | 3790 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3791 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3792 | `#endif` |
|     57 | 3793 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3794 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3795 | `        exp = 0;` |
|     57 | 3796 | `        if( realvalue>0.0 ){` |
|     61 | 3797 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3798 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3799 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3800 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3801 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3802 | `            zBuf = "NaN";` |
|    ! 0 | 3803 | `            length = 3;` |
|    ! 0 | 3804 | `            break;` |
|      - | 3805 | `          }` |
|     28 | 3806 | `        }` |
|     57 | 3807 | `        zBuf = zWorker;` |
|      - | 3808 | `        /*` |
|      - | 3809 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3810 | `        ** or etFLOAT, as appropriate.` |
|      - | 3811 | `        */` |
|     57 | 3812 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3813 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3814 | `          realvalue += rounder;` |
|    ! 0 | 3815 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3816 | `        }` |
|     57 | 3817 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3818 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3819 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3820 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3821 | `          }else{` |
|    ! 0 | 3822 | `            precision = precision - exp;` |
|    ! 0 | 3823 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3824 | `          }` |
|    ! 0 | 3825 | `        }else{` |
|     57 | 3826 | `          flag_rtz = 0;` |
|      - | 3827 | `        }` |
|      - | 3828 | `        /*` |
|      - | 3829 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3830 | `        ** the precision is too large to fit in buf[].` |
|      - | 3831 | `        */` |
|     57 | 3832 | `        nsd = 0;` |
|     57 | 3833 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3834 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3835 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3836 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3837 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3838 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3839 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3840 | `            *(zBuf++) = '0';` |
|     17 | 3841 | `          }` |
|    373 | 3842 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3843 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3844 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3845 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3846 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3847 | `          }` |
|     57 | 3848 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3849 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3850 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3851 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3852 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3853 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3854 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3855 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3856 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3857 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3858 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3859 | `          }` |
|    ! 0 | 3860 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3861 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3862 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3863 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3864 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3865 | `            if( exp>=100 ){` |
|    ! 0 | 3866 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3867 | `              exp %= 100;` |
|    ! 0 | 3868 | `            }` |
|    ! 0 | 3869 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3870 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3871 | `          }` |
|      - | 3872 | `        }` |
|      - | 3873 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3874 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3875 | `        ** integer conversions.*/` |
|     57 | 3876 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3877 | `        zBuf = zWorker;` |
|      - | 3878 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3879 | `        ** set and we are not left justified */` |
|     57 | 3880 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3881 | `          int i;` |
|      3 | 3882 | `          int nPad = width - length;` |
|     13 | 3883 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3884 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3885 | `          }` |
|      3 | 3886 | `          i = prefix!=0;` |
|      5 | 3887 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3888 | `          length = width;` |
|      1 | 3889 | `        }` |
|      - | 3890 | `#else` |
|      - | 3891 | `         zBuf = " ";` |
|      - | 3892 | `		 length = (int)sizeof(char);` |
|      - | 3893 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3894 | `		 break;` |
|      - | 3895 | `							 }` |
|      1 | 3896 | `		default:` |
|      - | 3897 | `			/* Invalid format specifer */` |
|      3 | 3898 | `			zWorker[0] = '?';` |
|      3 | 3899 | `			length = (int)sizeof(char);` |
|      2 | 3900 | `			break;` |
|      - | 3901 | `		}` |
|      - | 3902 | `		 /*` |
|      - | 3903 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3904 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3905 | `		 ** the output.` |
|      - | 3906 | `		 */` |
|    143 | 3907 | `    if( !flag_leftjustify ){` |
|      - | 3908 | `      register int nspace;` |
|    135 | 3909 | `      nspace = width-length;` |
|    135 | 3910 | `      if( nspace>0 ){` |
|      5 | 3911 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3912 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3913 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3914 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3915 | `			}` |
|    ! 0 | 3916 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3917 | `        }` |
|      5 | 3918 | `        if( nspace>0 ){` |
|      5 | 3919 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3920 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3921 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3922 | `			}` |
|      2 | 3923 | `		}` |
|      2 | 3924 | `      }` |
|     67 | 3925 | `    }` |
|    143 | 3926 | `    if( length>0 ){` |
|    143 | 3927 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    143 | 3928 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3929 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3930 | `		}` |
|     71 | 3931 | `    }` |
|    143 | 3932 | `    if( flag_leftjustify ){` |
|      - | 3933 | `      register int nspace;` |
|      9 | 3934 | `      nspace = width-length;` |
|      9 | 3935 | `      if( nspace>0 ){` |
|      9 | 3936 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3937 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3938 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3939 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3940 | `			}` |
|    ! 0 | 3941 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3942 | `        }` |
|      9 | 3943 | `        if( nspace>0 ){` |
|      9 | 3944 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3945 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3946 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3947 | `			}` |
|      4 | 3948 | `		}` |
|      4 | 3949 | `      }` |
|      4 | 3950 | `    }` |
|      1 | 3951 | ` }/* for(;;) */` |
|    137 | 3952 | `	return SXRET_OK;` |
|     69 | 3953 |  |
|      - | 3954 | `/*` |
|      - | 3955 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3956 | ` */` |
|     90 | 3957 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3958 |  |
|      - | 3959 | `	/* Consume directly */` |
|     91 | 3960 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     45 | 3961 | `	SXUNUSED(pUserData); /* cc warning */` |
|     91 | 3962 | `	return PH7_OK;` |
|      1 | 3963 |  |
|      - | 3964 | `/*` |
|      - | 3965 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3966 | ` *  Return a formatted string.` |
|      - | 3967 | ` * Parameters` |
|      - | 3968 | ` *  $format` |
|      - | 3969 | ` *    The format string (see block comment above)` |
|      - | 3970 | ` * Return` |
|      - | 3971 | ` *  A string produced according to the formatting string format.` |
|      - | 3972 | ` */` |
|     62 | 3973 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3974 |  |
|      - | 3975 | `	const char *zFormat;` |
|      - | 3976 | `	int nLen;` |
|     63 | 3977 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3978 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 3979 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3980 | `		return PH7_OK;` |
|      - | 3981 | `	}` |
|      - | 3982 | `	/* Extract the string format */` |
|     61 | 3983 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 3984 | `	if( nLen < 1 ){` |
|      - | 3985 | `		/* Empty string */` |
|    ! 0 | 3986 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3987 | `		return PH7_OK;` |
|      - | 3988 | `	}` |
|      - | 3989 | `	/* Format the string */` |
|     61 | 3990 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     61 | 3991 | `	return PH7_OK;` |
|     32 | 3992 |  |
|      - | 3993 | `/*` |
|      - | 3994 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3995 | ` */` |
|    130 | 3996 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3997 |  |
|    131 | 3998 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3999 | `	/* Call the VM output consumer directly */` |
|    131 | 4000 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4001 | `	/* Increment counter */` |
|    131 | 4002 | `	*pCounter += nLen;` |
|    131 | 4003 | `	return PH7_OK;` |
|      1 | 4004 |  |
|      - | 4005 | `/*` |
|      - | 4006 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4007 | ` *  Output a formatted string.` |
|      - | 4008 | ` * Parameters` |
|      - | 4009 | ` *  $format` |
|      - | 4010 | ` *   See sprintf() for a description of format.` |
|      - | 4011 | ` * Return` |
|      - | 4012 | ` *  The length of the outputted string.` |
|      - | 4013 | ` */` |
|     52 | 4014 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4015 |  |
|     53 | 4016 | `	ph7_int64 nCounter = 0;` |
|      - | 4017 | `	const char *zFormat;` |
|      - | 4018 | `	int nLen;` |
|     53 | 4019 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4020 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4021 | `		ph7_result_int(pCtx,0);` |
|      3 | 4022 | `		return PH7_OK;` |
|      - | 4023 | `	}` |
|      - | 4024 | `	/* Extract the string format */` |
|     51 | 4025 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     51 | 4026 | `	if( nLen < 1 ){` |
|      - | 4027 | `		/* Empty string */` |
|    ! 0 | 4028 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4029 | `		return PH7_OK;` |
|      - | 4030 | `	}` |
|      - | 4031 | `	/* Format the string */` |
|     51 | 4032 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4033 | `	/* Return the length of the outputted string */` |
|     51 | 4034 | `	ph7_result_int64(pCtx,nCounter);` |
|     51 | 4035 | `	return PH7_OK;` |
|     27 | 4036 |  |
|      - | 4037 | `/*` |
|      - | 4038 | ` * int vprintf(string $format,array $args)` |
|      - | 4039 | ` *  Output a formatted string.` |
|      - | 4040 | ` * Parameters` |
|      - | 4041 | ` *  $format` |
|      - | 4042 | ` *   See sprintf() for a description of format.` |
|      - | 4043 | ` * Return` |
|      - | 4044 | ` *  The length of the outputted string.` |
|      - | 4045 | ` */` |
|      2 | 4046 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4047 |  |
|      3 | 4048 | `	ph7_int64 nCounter = 0;` |
|      - | 4049 | `	const char *zFormat;` |
|      - | 4050 | `	ph7_hashmap *pMap;` |
|      - | 4051 | `	SySet sArg;` |
|      - | 4052 | `	int nLen,n;` |
|      3 | 4053 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4054 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4055 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4056 | `		return PH7_OK;` |
|      - | 4057 | `	}` |
|      - | 4058 | `	/* Extract the string format */` |
|      3 | 4059 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4060 | `	if( nLen < 1 ){` |
|      - | 4061 | `		/* Empty string */` |
|    ! 0 | 4062 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4063 | `		return PH7_OK;` |
|      - | 4064 | `	}` |
|      - | 4065 | `	/* Point to the hashmap */` |
|      3 | 4066 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4067 | `	/* Extract arguments from the hashmap */` |
|      3 | 4068 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4069 | `	/* Format the string */` |
|      3 | 4070 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4071 | `	/* Return the length of the outputted string */` |
|      3 | 4072 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4073 | `	/* Release the container */` |
|      3 | 4074 | `	SySetRelease(&sArg);` |
|      3 | 4075 | `	return PH7_OK;` |
|      2 | 4076 |  |
|      - | 4077 | `/*` |
|      - | 4078 | ` * int vsprintf(string $format,array $args)` |
|      - | 4079 | ` *  Output a formatted string.` |
|      - | 4080 | ` * Parameters` |
|      - | 4081 | ` *  $format` |
|      - | 4082 | ` *   See sprintf() for a description of format.` |
|      - | 4083 | ` * Return` |
|      - | 4084 | ` *  A string produced according to the formatting string format.` |
|      - | 4085 | ` */` |
|     10 | 4086 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4087 |  |
|      - | 4088 | `	const char *zFormat;` |
|      - | 4089 | `	ph7_hashmap *pMap;` |
|      - | 4090 | `	SySet sArg;` |
|      - | 4091 | `	int nLen,n;` |
|     11 | 4092 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4093 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4094 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4095 | `		return PH7_OK;` |
|      - | 4096 | `	}` |
|      - | 4097 | `	/* Extract the string format */` |
|      7 | 4098 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4099 | `	if( nLen < 1 ){` |
|      - | 4100 | `		/* Empty string */` |
|    ! 0 | 4101 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4102 | `		return PH7_OK;` |
|      - | 4103 | `	}` |
|      - | 4104 | `	/* Point to hashmap */` |
|      7 | 4105 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4106 | `	/* Extract arguments from the hashmap */` |
|      7 | 4107 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4108 | `	/* Format the string */` |
|      7 | 4109 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4110 | `	/* Release the container */` |
|      7 | 4111 | `	SySetRelease(&sArg);` |
|      7 | 4112 | `	return PH7_OK;` |
|      6 | 4113 |  |
|      - | 4114 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4115 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4116 | `/*` |
|      - | 4117 | ` * Symisc eXtension.` |
|      - | 4118 | ` * string size_format(int64 $size)` |
|      - | 4119 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4120 | ` *  Example:` |
|      - | 4121 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4122 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4123 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4124 | ` * Parameter` |
|      - | 4125 | ` *  $size` |
|      - | 4126 | ` *    Entity size in bytes.` |
|      - | 4127 | ` * Return` |
|      - | 4128 | ` *   Formatted string representation of the given size.` |
|      - | 4129 | ` */` |
|     24 | 4130 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4131 |  |
|      - | 4132 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4133 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4134 | `	sxi32 nRest,i_32;` |
|      - | 4135 | `	ph7_int64 iSize;` |
|     25 | 4136 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4137 |  |
|     25 | 4138 | `	if( nArg < 1 ){` |
|      - | 4139 | `		/* Missing argument,return the empty string */` |
|      3 | 4140 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4141 | `		return PH7_OK;` |
|      - | 4142 | `	}` |
|      - | 4143 | `	/* Extract the given size */` |
|     23 | 4144 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4145 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4146 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4147 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4148 | `		return PH7_OK;` |
|      - | 4149 | `	}` |
|     19 | 4150 | `	for(;;){` |
|     39 | 4151 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4152 | `		iSize >>= 10;` |
|     39 | 4153 | `		c++;` |
|     39 | 4154 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4155 | `			break;` |
|      - | 4156 | `		}` |
|      1 | 4157 | `	}` |
|     19 | 4158 | `	nRest /= 100;` |
|     19 | 4159 | `	if( nRest > 9 ){` |
|    ! 0 | 4160 | `		nRest = 9;` |
|    ! 0 | 4161 | `	}` |
|     19 | 4162 | `	if( iSize > 999 ){` |
|    ! 0 | 4163 | `		c++;` |
|    ! 0 | 4164 | `		nRest = 9;` |
|    ! 0 | 4165 | `		iSize = 0;` |
|    ! 0 | 4166 | `	}` |
|     19 | 4167 | `	i_32 = (sxi32)iSize;` |
|      - | 4168 | `	/* Format */` |
|     19 | 4169 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4170 | `	return PH7_OK;` |
|     13 | 4171 |  |
|      - | 4172 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4173 | `/*` |
|      - | 4174 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4175 | ` *   Calculate the md5 hash of a string.` |
|      - | 4176 | ` * Parameter` |
|      - | 4177 | ` *  $str` |
|      - | 4178 | ` *   Input string` |
|      - | 4179 | ` * $raw_output` |
|      - | 4180 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4181 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4182 | ` * Return` |
|      - | 4183 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4184 | ` */` |
|     14 | 4185 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4186 |  |
|      - | 4187 | `	unsigned char zDigest[16];` |
|     15 | 4188 | `	int raw_output = FALSE;` |
|      - | 4189 | `	const void *pIn;` |
|      - | 4190 | `	int nLen;` |
|     15 | 4191 | `	if( nArg < 1 ){` |
|      - | 4192 | `		/* Missing arguments,return the empty string */` |
|      3 | 4193 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4194 | `		return PH7_OK;` |
|      - | 4195 | `	}` |
|      - | 4196 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4197 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4198 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4199 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4200 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4201 | `	}` |
|      - | 4202 | `	/* Compute the MD5 digest */` |
|     13 | 4203 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4204 | `	if( raw_output ){` |
|      - | 4205 | `		/* Output raw digest */` |
|      5 | 4206 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4207 | `	}else{` |
|      - | 4208 | `		/* Perform a binary to hex conversion */` |
|      9 | 4209 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4210 | `	}` |
|     13 | 4211 | `	return PH7_OK;` |
|      8 | 4212 |  |
|      - | 4213 | `/*` |
|      - | 4214 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4215 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4216 | ` * Parameter` |
|      - | 4217 | ` *  $str` |
|      - | 4218 | ` *   Input string` |
|      - | 4219 | ` * $raw_output` |
|      - | 4220 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4221 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4222 | ` * Return` |
|      - | 4223 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4224 | ` */` |
|     12 | 4225 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4226 |  |
|      - | 4227 | `	unsigned char zDigest[20];` |
|     13 | 4228 | `	int raw_output = FALSE;` |
|      - | 4229 | `	const void *pIn;` |
|      - | 4230 | `	int nLen;` |
|     13 | 4231 | `	if( nArg < 1 ){` |
|      - | 4232 | `		/* Missing arguments,return the empty string */` |
|      3 | 4233 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4234 | `		return PH7_OK;` |
|      - | 4235 | `	}` |
|      - | 4236 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4237 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4238 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4239 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4240 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4241 | `	}` |
|      - | 4242 | `	/* Compute the SHA1 digest */` |
|     11 | 4243 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4244 | `	if( raw_output ){` |
|      - | 4245 | `		/* Output raw digest */` |
|      5 | 4246 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4247 | `	}else{` |
|      - | 4248 | `		/* Perform a binary to hex conversion */` |
|      7 | 4249 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4250 | `	}` |
|     11 | 4251 | `	return PH7_OK;` |
|      7 | 4252 |  |
|      - | 4253 | `/*` |
|      - | 4254 | ` * int64 crc32(string $str)` |
|      - | 4255 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4256 | ` * Parameter` |
|      - | 4257 | ` *  $str` |
|      - | 4258 | ` *   Input string` |
|      - | 4259 | ` * Return` |
|      - | 4260 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4261 | ` */` |
|      4 | 4262 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4263 |  |
|      - | 4264 | `	const void *pIn;` |
|      - | 4265 | `	sxu32 nCRC;` |
|      - | 4266 | `	int nLen;` |
|      5 | 4267 | `	if( nArg < 1 ){` |
|      - | 4268 | `		/* Missing arguments,return 0 */` |
|      3 | 4269 | `		ph7_result_int(pCtx,0);` |
|      3 | 4270 | `		return PH7_OK;` |
|      - | 4271 | `	}` |
|      - | 4272 | `	/* Extract the input string */` |
|      3 | 4273 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4274 | `	if( nLen < 1 ){` |
|      - | 4275 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4276 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4277 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4278 | `		return PH7_OK;` |
|      - | 4279 | `	}` |
|      - | 4280 | `	/* Calculate the sum */` |
|      3 | 4281 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4282 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4283 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4284 | `	return PH7_OK;` |
|      3 | 4285 |  |
|      - | 4286 | `/*` |
|      - | 4287 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4288 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4289 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4290 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4291 | ` */` |
|     11 | 4292 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4293 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4294 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4295 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4296 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4297 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4298 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4299 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4300 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4301 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4302 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4303 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4304 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4305 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4306 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4307 | `struct HashAlgo {` |
|      - | 4308 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4309 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4310 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4311 | `	void (*xInit)(HashCtx *);` |
|      - | 4312 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4313 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4314 | `};` |
|      - | 4315 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4316 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4317 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4318 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4319 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4320 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4321 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4322 | `};` |
|      - | 4323 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4324 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4325 | `	sxu32 i;` |
|    279 | 4326 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4327 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4328 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4329 | `			return &aHashAlgo[i];` |
|      - | 4330 | `		}` |
|    106 | 4331 | `	}` |
|      6 | 4332 | `	return 0;` |
|     38 | 4333 |  |
|      - | 4334 | `/*` |
|      - | 4335 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4336 | ` *   Generate a hash value (message digest).` |
|      - | 4337 | ` */` |
|     54 | 4338 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4339 |  |
|      - | 4340 | `	const HashAlgo *pAlgo;` |
|      - | 4341 | `	const char *zAlgo,*zData;` |
|     56 | 4342 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4343 | `	HashCtx sCtx;` |
|      - | 4344 | `	unsigned char zDigest[64];` |
|     56 | 4345 | `	if( nArg < 2 ){` |
|    ! 0 | 4346 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4347 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4348 | `	}` |
|     56 | 4349 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4350 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4351 | `	if( pAlgo == 0 ){` |
|      3 | 4352 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4353 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4354 | `	}` |
|     53 | 4355 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4356 | `	if( nArg > 2 ){` |
|      9 | 4357 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4358 | `	}` |
|     53 | 4359 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4360 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4361 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4362 | `	if( raw_output ){` |
|      9 | 4363 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4364 | `	}else{` |
|     45 | 4365 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4366 | `	}` |
|     53 | 4367 | `	return PH7_OK;` |
|     29 | 4368 |  |
|      - | 4369 | `/*` |
|      - | 4370 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4371 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4372 | ` */` |
|     16 | 4373 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4374 |  |
|      - | 4375 | `	const HashAlgo *pAlgo;` |
|      - | 4376 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4377 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4378 | `	HashCtx sCtx;` |
|      - | 4379 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4380 | `	int i,nBlock,nDigest;` |
|     18 | 4381 | `	if( nArg < 3 ){` |
|    ! 0 | 4382 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4383 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4384 | `	}` |
|     18 | 4385 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4386 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4387 | `	if( pAlgo == 0 ){` |
|      3 | 4388 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4389 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4390 | `	}` |
|     15 | 4391 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4392 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4393 | `	if( nArg > 3 ){` |
|      3 | 4394 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4395 | `	}` |
|     15 | 4396 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4397 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4398 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4399 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4400 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4401 | `	if( nKeyLen > nBlock ){` |
|      3 | 4402 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4403 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4404 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4405 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4406 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4407 | `	}` |
|   1039 | 4408 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4409 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4410 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4411 | `	}` |
|      - | 4412 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4413 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4414 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4415 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4416 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4417 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4418 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4419 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4420 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4421 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4422 | `	if( raw_output ){` |
|      3 | 4423 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4424 | `	}else{` |
|     13 | 4425 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4426 | `	}` |
|     15 | 4427 | `	return PH7_OK;` |
|     10 | 4428 |  |
|      - | 4429 | `/*` |
|      - | 4430 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4431 | ` *   Timing-attack-safe string comparison.` |
|      - | 4432 | ` */` |
|     14 | 4433 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4434 |  |
|      - | 4435 | `	const char *zKnown,*zUser;` |
|      - | 4436 | `	int nKnown,nUser,i;` |
|     17 | 4437 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4438 | `	if( nArg < 2 ){` |
|    ! 0 | 4439 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4440 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4441 | `	}` |
|     17 | 4442 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4443 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4444 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4445 | `			ph7_type_name(apArg[0]));` |
|      - | 4446 | `	}` |
|     14 | 4447 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4448 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4449 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4450 | `			ph7_type_name(apArg[1]));` |
|      - | 4451 | `	}` |
|     11 | 4452 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4453 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4454 | `	if( nKnown != nUser ){` |
|      5 | 4455 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4456 | `		return PH7_OK;` |
|      - | 4457 | `	}` |
|      - | 4458 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4459 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4460 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4461 | `	}` |
|      7 | 4462 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4463 | `	return PH7_OK;` |
|     10 | 4464 |  |
|      - | 4465 | `/*` |
|      - | 4466 | ` * array hash_algos(void)` |
|      - | 4467 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4468 | ` */` |
|      2 | 4469 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4470 |  |
|      - | 4471 | `	ph7_value *pArray,*pValue;` |
|      - | 4472 | `	sxu32 i;` |
|      1 | 4473 | `	SXUNUSED(nArg);` |
|      1 | 4474 | `	SXUNUSED(apArg);` |
|      3 | 4475 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4476 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4477 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4478 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4479 | `		return PH7_OK;` |
|      - | 4480 | `	}` |
|     15 | 4481 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4482 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4483 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4484 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4485 | `	}` |
|      3 | 4486 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4487 | `	return PH7_OK;` |
|      2 | 4488 |  |
|      - | 4489 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4490 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4491 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4492 | `/*` |
|      - | 4493 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4494 |  |
|      - | 4495 | ` */` |
|      4 | 4496 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4497 | `	const char *zInput, /* Raw input */` |
|      - | 4498 | `	int nByte,  /* Input length */` |
|      - | 4499 | `	int delim,  /* Delimiter */` |
|      - | 4500 | `	int encl,   /* Enclosure */` |
|      - | 4501 | `	int escape,  /* Escape character */` |
|      - | 4502 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4503 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4504 | `	)` |
|      1 | 4505 |  |
|      5 | 4506 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4507 | `	const char *zIn = zInput;` |
|      - | 4508 | `	const char *zPtr;` |
|      - | 4509 | `	int isEnc;` |
|      - | 4510 | `	/* Start processing */` |
|      8 | 4511 | `	for(;;){` |
|     17 | 4512 | `		if( zIn >= zEnd ){` |
|      - | 4513 | `			/* No more input to process */` |
|      5 | 4514 | `			break;` |
|      - | 4515 | `		}` |
|     13 | 4516 | `		isEnc = 0;` |
|     13 | 4517 | `		zPtr = zIn;` |
|      - | 4518 | `		/* Find the first delimiter */` |
|     27 | 4519 | `		while( zIn < zEnd ){` |
|     23 | 4520 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4521 | `				/* Delimiter found,break imediately */` |
|      5 | 4522 | `				break;` |
|     15 | 4523 | `			}else if( zIn[0] == encl ){` |
|      - | 4524 | `				/* Inside enclosure? */` |
|    ! 0 | 4525 | `				isEnc = !isEnc;` |
|     15 | 4526 | `			}else if( zIn[0] == escape ){` |
|      - | 4527 | `				/* Escape sequence */` |
|    ! 0 | 4528 | `				zIn++;` |
|    ! 0 | 4529 | `			}` |
|      - | 4530 | `			/* Advance the cursor */` |
|     15 | 4531 | `			zIn++;` |
|      1 | 4532 | `		}` |
|     13 | 4533 | `		if( zIn > zPtr ){` |
|     13 | 4534 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4535 | `			sxi32 rc;` |
|      - | 4536 | `			/* Invoke the supllied callback */` |
|     13 | 4537 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4538 | `				zPtr++;` |
|    ! 0 | 4539 | `				nByteChunk-=2;` |
|    ! 0 | 4540 | `			}` |
|     13 | 4541 | `			if( nByteChunk > 0 ){` |
|     13 | 4542 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4543 | `				if( rc == SXERR_ABORT ){` |
|      - | 4544 | `					/* User callback request an operation abort */` |
|    ! 0 | 4545 | `					break;` |
|      - | 4546 | `				}` |
|      6 | 4547 | `			}` |
|      6 | 4548 | `		}` |
|      - | 4549 | `		/* Ignore trailing delimiter */` |
|     21 | 4550 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4551 | `			zIn++;` |
|      1 | 4552 | `		}` |
|      1 | 4553 | `	}` |
|      5 | 4554 | `	return SXRET_OK;` |
|      1 | 4555 |  |
|      - | 4556 | `/*` |
|      - | 4557 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4558 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4559 | ` * argument to this callback.` |
|      - | 4560 | ` */` |
|     12 | 4561 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4562 |  |
|     13 | 4563 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4564 | `	ph7_value sEntry;` |
|      - | 4565 | `	SyString sToken;` |
|      - | 4566 | `	/* Insert the token in the given array */` |
|     13 | 4567 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4568 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4569 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4570 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4571 | `		return SXRET_OK;` |
|      - | 4572 | `	}` |
|     13 | 4573 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4574 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4575 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4576 | `	return SXRET_OK;` |
|      7 | 4577 |  |
|      - | 4578 | `/*` |
|      - | 4579 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4580 | ` *  Parse a CSV string into an array.` |
|      - | 4581 | ` * Parameters` |
|      - | 4582 | ` *  $input` |
|      - | 4583 | ` *   The string to parse.` |
|      - | 4584 | ` *  $delimiter` |
|      - | 4585 | ` *   Set the field delimiter (one character only).` |
|      - | 4586 | ` *  $enclosure` |
|      - | 4587 | ` *   Set the field enclosure character (one character only).` |
|      - | 4588 | ` *  $escape` |
|      - | 4589 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4590 | ` * Return` |
|      - | 4591 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4592 | ` */` |
|      4 | 4593 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4594 |  |
|      - | 4595 | `	const char *zInput,*zPtr;` |
|      - | 4596 | `	ph7_value *pArray;` |
|      5 | 4597 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4598 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4599 | `	int escape = '\\';  /* Escape character */` |
|      - | 4600 | `	int nLen;` |
|      5 | 4601 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4602 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4603 | `		ph7_result_null(pCtx);` |
|      3 | 4604 | `		return PH7_OK;` |
|      - | 4605 | `	}` |
|      - | 4606 | `	/* Extract the raw input */` |
|      3 | 4607 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4608 | `	if( nArg > 1 ){` |
|      - | 4609 | `		int i;` |
|      3 | 4610 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4611 | `			/* Extract the delimiter */` |
|      3 | 4612 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4613 | `			if( i > 0 ){` |
|      3 | 4614 | `				delim = zPtr[0];` |
|      1 | 4615 | `			}` |
|      1 | 4616 | `		}` |
|      3 | 4617 | `		if( nArg > 2 ){` |
|      3 | 4618 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4619 | `				/* Extract the enclosure */` |
|      3 | 4620 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4621 | `				if( i > 0 ){` |
|      3 | 4622 | `					encl = zPtr[0];` |
|      1 | 4623 | `				}` |
|      1 | 4624 | `			}` |
|      3 | 4625 | `			if( nArg > 3 ){` |
|      3 | 4626 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4627 | `					/* Extract the escape character */` |
|      3 | 4628 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4629 | `					if( i > 0 ){` |
|      3 | 4630 | `						escape = zPtr[0];` |
|      1 | 4631 | `					}` |
|      1 | 4632 | `				}` |
|      1 | 4633 | `			}` |
|      1 | 4634 | `		}` |
|      1 | 4635 | `	}` |
|      - | 4636 | `	/* Create our array */` |
|      3 | 4637 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4638 | `	if( pArray == 0 ){` |
|      - | 4639 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 4640 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4641 | `	}` |
|      - | 4642 | `	/* Parse the raw input */` |
|      3 | 4643 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4644 | `	/* Return the freshly created array */` |
|      3 | 4645 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4646 | `	return PH7_OK;` |
|      3 | 4647 |  |
|      - | 4648 | `/*` |
|      - | 4649 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4650 | ` * container.` |
|      - | 4651 | ` * Refer to [strip_tags()].` |
|      - | 4652 | ` */` |
|     10 | 4653 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4654 |  |
|     11 | 4655 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4656 | `	const char *zPtr;` |
|      - | 4657 | `	SyString sEntry;` |
|      - | 4658 | `	/* Strip tags */` |
|     10 | 4659 | `	for(;;){` |
|     45 | 4660 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4661 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4662 | `				zTag++;` |
|      1 | 4663 | `		}` |
|     21 | 4664 | `		if( zTag >= zEnd ){` |
|     11 | 4665 | `			break;` |
|      - | 4666 | `		}` |
|     11 | 4667 | `		zPtr = zTag;` |
|      - | 4668 | `		/* Delimit the tag */` |
|     25 | 4669 | `		while(zTag < zEnd ){` |
|     25 | 4670 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4671 | `				/* UTF-8 stream */` |
|      3 | 4672 | `				zTag++;` |
|      5 | 4673 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4674 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4675 | `				break;` |
|    ! 0 | 4676 | `			}else{` |
|     13 | 4677 | `				zTag++;` |
|      - | 4678 | `			}` |
|      1 | 4679 | `		}` |
|     11 | 4680 | `		if( zTag > zPtr ){` |
|      - | 4681 | `			/* Perform the insertion */` |
|     11 | 4682 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4683 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4684 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4685 | `		}` |
|      - | 4686 | `		/* Jump the trailing '>' */` |
|     11 | 4687 | `		zTag++;` |
|      1 | 4688 | `	}` |
|     11 | 4689 | `	return SXRET_OK;` |
|      1 | 4690 |  |
|      - | 4691 | `/*` |
|      - | 4692 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4693 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4694 | ` * Refer to [strip_tags()].` |
|      - | 4695 | ` */` |
|     36 | 4696 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4697 |  |
|     37 | 4698 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4699 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4700 | `		SyString sTag;` |
|     85 | 4701 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4702 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4703 | `			zTag++;` |
|      1 | 4704 | `		}` |
|      - | 4705 | `		/* Delimit the tag */` |
|     25 | 4706 | `		zCur = zTag;` |
|     77 | 4707 | `		while(zTag < zEnd ){` |
|     77 | 4708 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4709 | `				/* UTF-8 stream */` |
|      5 | 4710 | `				zTag++;` |
|      9 | 4711 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 4712 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 4713 | `				break;` |
|    ! 0 | 4714 | `			}else{` |
|     49 | 4715 | `				zTag++;` |
|      - | 4716 | `			}` |
|      1 | 4717 | `		}` |
|     25 | 4718 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 4719 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 4720 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 4721 | `		if( sTag.nByte > 0 ){` |
|      - | 4722 | `			SyString *aEntry,*pEntry;` |
|      - | 4723 | `			sxi32 rc;` |
|      - | 4724 | `			sxu32 n;` |
|      - | 4725 | `			/* Perform the lookup */` |
|     25 | 4726 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 4727 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 4728 | `				pEntry = &aEntry[n];` |
|      - | 4729 | `				/* Do the comparison */` |
|     25 | 4730 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 4731 | `				if( !rc ){` |
|     21 | 4732 | `					return SXRET_OK;` |
|      - | 4733 | `				}` |
|      3 | 4734 | `			}` |
|      2 | 4735 | `		}` |
|      2 | 4736 | `	}` |
|      - | 4737 | `	/* No such tag */` |
|     17 | 4738 | `	return SXERR_NOTFOUND;` |
|     19 | 4739 |  |
|      - | 4740 | `/*` |
|      - | 4741 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 4742 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 4743 | ` * Refer to [strip_tags()].` |
|      - | 4744 | ` */` |
|     16 | 4745 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 4746 |  |
|     17 | 4747 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4748 | `	const char *zPtr,*zTag;` |
|      - | 4749 | `	SySet sSet;` |
|      - | 4750 | `	/* initialize the set of allowed tags */` |
|     17 | 4751 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 4752 | `	if( nTaglen > 0 ){` |
|      - | 4753 | `		/* Set of allowed tags */` |
|     11 | 4754 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 4755 | `	}` |
|      - | 4756 | `	/* Set the empty string */` |
|     17 | 4757 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4758 | `	/* Start processing */` |
|     26 | 4759 | `	for(;;){` |
|     53 | 4760 | `		if(zIn >= zEnd){` |
|      - | 4761 | `			/* No more input to process */` |
|     15 | 4762 | `			break;` |
|      - | 4763 | `		}` |
|     39 | 4764 | `		zPtr = zIn;` |
|      - | 4765 | `		/* Find a tag */` |
|    133 | 4766 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 4767 | `			zIn++;` |
|      1 | 4768 | `		}` |
|     39 | 4769 | `		if( zIn > zPtr ){` |
|      - | 4770 | `			/* Consume raw input */` |
|     21 | 4771 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 4772 | `		}` |
|      - | 4773 | `		/* Ignore trailing null bytes */` |
|     39 | 4774 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 4775 | `			zIn++;` |
|    ! 0 | 4776 | `		}` |
|     39 | 4777 | `		if(zIn >= zEnd){` |
|      - | 4778 | `			/* No more input to process */` |
|      3 | 4779 | `			break;` |
|      - | 4780 | `		}` |
|     37 | 4781 | `		if( zIn[0] == '<' ){` |
|      - | 4782 | `			sxi32 rc;` |
|     37 | 4783 | `			zTag = zIn++;` |
|      - | 4784 | `			/* Delimit the tag */` |
|    127 | 4785 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 4786 | `				zIn++;` |
|      1 | 4787 | `			}` |
|     37 | 4788 | `			if( zIn < zEnd ){` |
|     37 | 4789 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 4790 | `			}` |
|      - | 4791 | `			/* Query the set */` |
|     37 | 4792 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 4793 | `			if( rc == SXRET_OK ){` |
|      - | 4794 | `				/* Keep the tag */` |
|     21 | 4795 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 4796 | `			}` |
|     18 | 4797 | `		}` |
|      1 | 4798 | `	}` |
|      - | 4799 | `	/* Cleanup */` |
|     17 | 4800 | `	SySetRelease(&sSet);` |
|     17 | 4801 | `	return SXRET_OK;` |
|      1 | 4802 |  |
|      - | 4803 | `/*` |
|      - | 4804 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 4805 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 4806 | ` * Parameters` |
|      - | 4807 | ` *  $str` |
|      - | 4808 | ` *  The input string.` |
|      - | 4809 | ` * $allowable_tags` |
|      - | 4810 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 4811 | ` * Return` |
|      - | 4812 | ` *  Returns the stripped string.` |
|      - | 4813 | ` */` |
|     16 | 4814 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4815 |  |
|     17 | 4816 | `	const char *zTaglist = 0;` |
|      - | 4817 | `	const char *zString;` |
|     17 | 4818 | `	int nTaglen = 0;` |
|      - | 4819 | `	int nLen;` |
|     17 | 4820 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4821 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4822 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4823 | `		return PH7_OK;` |
|      - | 4824 | `	}` |
|      - | 4825 | `	/* Point to the raw string */` |
|     15 | 4826 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 4827 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 4828 | `		/* Allowed tag */` |
|     11 | 4829 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 4830 | `	}` |
|      - | 4831 | `	/* Process input */` |
|     15 | 4832 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 4833 | `	return PH7_OK;` |
|      9 | 4834 |  |
|      - | 4835 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4836 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4837 | `/*` |
|      - | 4838 | ` * string str_shuffle(string $str)` |
|      - | 4839 |  |
|      - | 4840 | ` *  Randomly shuffles a string.` |
|      - | 4841 | ` * Parameters` |
|      - | 4842 | ` *  $str` |
|      - | 4843 | ` *   The input string.` |
|      - | 4844 | ` * Return` |
|      - | 4845 | ` *  Returns the shuffled string.` |
|      - | 4846 | ` */` |
|     12 | 4847 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4848 |  |
|      - | 4849 | `	const char *zString;` |
|      - | 4850 | `	int nLen,i,c;` |
|      - | 4851 | `	sxu32 iR;` |
|     13 | 4852 | `	if( nArg < 1 ){` |
|      - | 4853 | `		/* Missing arguments,return the empty string */` |
|      3 | 4854 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4855 | `		return PH7_OK;` |
|      - | 4856 | `	}` |
|      - | 4857 | `	/* Extract the target string */` |
|     11 | 4858 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4859 | `	if( nLen < 1 ){` |
|      - | 4860 | `		/* Nothing to shuffle */` |
|      3 | 4861 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4862 | `		return PH7_OK;` |
|      - | 4863 | `	}` |
|      - | 4864 | `	/* Shuffle the string */` |
|     43 | 4865 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 4866 | `		/* Generate a random number first */` |
|     35 | 4867 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 4868 | `		/* Extract a random offset */` |
|     35 | 4869 | `		c = zString[iR % nLen];` |
|      - | 4870 | `		/* Append it */` |
|     35 | 4871 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 4872 | `	}` |
|      9 | 4873 | `	return PH7_OK;` |
|      7 | 4874 |  |
|      - | 4875 | `/*` |
|      - | 4876 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 4877 | ` *  Convert a string to an array.` |
|      - | 4878 | ` * Parameters` |
|      - | 4879 | ` * $string` |
|      - | 4880 | ` *  The input string.` |
|      - | 4881 | ` * $split_length` |
|      - | 4882 | ` *  Maximum length of the chunk.` |
|      - | 4883 | ` * Return` |
|      - | 4884 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 4885 | ` *  except possibly the last one which may be shorter.` |
|      - | 4886 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 4887 | ` *  as the first (and only) array element.` |
|      - | 4888 | ` *  An empty string returns an empty array.` |
|      - | 4889 | ` * Errors` |
|      - | 4890 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 4891 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 4892 | ` *  ValueError if $split_length is less than 1.` |
|      - | 4893 | ` */` |
|     28 | 4894 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4895 |  |
|      - | 4896 | `	const char *zString,*zEnd;` |
|      - | 4897 | `	ph7_value *pArray,*pValue;` |
|      - | 4898 | `	int split_len;` |
|      - | 4899 | `	int nLen;` |
|     33 | 4900 | `	if( nArg < 1 ){` |
|      4 | 4901 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4902 | `			"ArgumentCountError",` |
|      - | 4903 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 4904 | `			nArg` |
|      - | 4905 | `			);` |
|      - | 4906 | `	}` |
|      - | 4907 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 4908 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 4909 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 4910 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 4911 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4912 | `			"TypeError",` |
|      - | 4913 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 4914 | `			ph7_type_name(apArg[0])` |
|      - | 4915 | `			);` |
|      - | 4916 | `	}` |
|      - | 4917 | `	/* Point to the target string */` |
|     27 | 4918 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 4919 | `	split_len = (int)sizeof(char);` |
|     27 | 4920 | `	if( nArg > 1 ){` |
|      - | 4921 | `		/* Split length */` |
|     17 | 4922 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 4923 | `		if( split_len < 1 ){` |
|      6 | 4924 | `			return PH7_VmThrowException(pCtx,` |
|      - | 4925 | `				"ValueError",` |
|      - | 4926 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 4927 | `				);` |
|      - | 4928 | `		}` |
|     11 | 4929 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 4930 | `			split_len = nLen;` |
|      1 | 4931 | `		}` |
|      5 | 4932 | `	}` |
|      - | 4933 | `	/* Create the array and the scalar value */` |
|     21 | 4934 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 4935 | `	/*Chunk value */` |
|     21 | 4936 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 4937 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 4938 | `		/* Return FALSE */` |
|    ! 0 | 4939 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4940 | `		return PH7_OK;` |
|      - | 4941 | `	}` |
|      - | 4942 | `	/* Point to the end of the string */` |
|     21 | 4943 | `	zEnd = &zString[nLen];` |
|      - | 4944 | `	/* Perform the requested operation */` |
|     48 | 4945 | `	for(;;){` |
|      - | 4946 | `		int nMax;` |
|     59 | 4947 | `		if( zString >= zEnd ){` |
|      - | 4948 | `			/* No more input to process */` |
|     21 | 4949 | `			break;` |
|      - | 4950 | `		}` |
|     39 | 4951 | `		nMax = (int)(zEnd-zString);` |
|     39 | 4952 | `		if( nMax < split_len ){` |
|      3 | 4953 | `			split_len = nMax;` |
|      1 | 4954 | `		}` |
|      - | 4955 | `		/* Copy the current chunk */` |
|     39 | 4956 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 4957 | `		/* Insert it */` |
|     39 | 4958 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 4959 | `		/* reset the string cursor */` |
|     39 | 4960 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 4961 | `		/* Update position */` |
|     39 | 4962 | `		zString += split_len;` |
|      1 | 4963 | `	}` |
|      - | 4964 | `	/*` |
|      - | 4965 | `	 * Return the array.` |
|      - | 4966 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 4967 | `	 * upon we return from this function.` |
|      - | 4968 | `	 */` |
|     21 | 4969 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 4970 | `	return PH7_OK;` |
|     19 | 4971 |  |
|      - | 4972 | `/*` |
|      - | 4973 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 4974 | ` * Refer to [strspn()].` |
|      - | 4975 | ` */` |
|     28 | 4976 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 4977 |  |
|     29 | 4978 | `	const char *zIn = *pzIn;` |
|      - | 4979 | `	const char *zPtr;` |
|      - | 4980 | `	/* Ignore leading white spaces */` |
|     29 | 4981 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 4982 | `		zIn++;` |
|    ! 0 | 4983 | `	}` |
|     29 | 4984 | `	if( zIn >= zEnd ){` |
|      - | 4985 | `		/* End of input */` |
|    ! 0 | 4986 | `		return SXERR_EOF;` |
|      - | 4987 | `	}` |
|     29 | 4988 | `	zPtr = zIn;` |
|      - | 4989 | `	/* Extract the token */` |
|    201 | 4990 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 4991 | `		zIn++;` |
|      1 | 4992 | `	}` |
|     29 | 4993 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4994 | `	/* Synchronize pointers */` |
|     29 | 4995 | `	*pzIn = zIn;` |
|      - | 4996 | `	/* Return to the caller */` |
|     29 | 4997 | `	return SXRET_OK;` |
|     15 | 4998 |  |
|      - | 4999 | `/*` |
|      - | 5000 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5001 | ` * return the longest match.` |
|      - | 5002 | ` * Refer to [strspn()].` |
|      - | 5003 | ` */` |
|     18 | 5004 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5005 |  |
|     19 | 5006 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5007 | `	const char *zIn = zString;` |
|      - | 5008 | `	int i,c;` |
|     45 | 5009 | `	for(;;){` |
|     91 | 5010 | `		if( zString >= zEnd ){` |
|      7 | 5011 | `			break;` |
|      - | 5012 | `		}` |
|      - | 5013 | `		/* Extract current character */` |
|     85 | 5014 | `		c = zString[0];` |
|      - | 5015 | `		/* Perform the lookup */` |
|    383 | 5016 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5017 | `			if( c == zMask[i] ){` |
|      - | 5018 | `				/* Character found */` |
|     73 | 5019 | `				break;` |
|      - | 5020 | `			}` |
|    150 | 5021 | `		}` |
|     85 | 5022 | `		if( i >= nMaskLen ){` |
|      - | 5023 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5024 | `			break;` |
|      - | 5025 | `		}` |
|      - | 5026 | `		/* Advance cursor */` |
|     73 | 5027 | `		zString++;` |
|      1 | 5028 | `	}` |
|      - | 5029 | `	/* Longest match */` |
|     19 | 5030 | `	return (int)(zString-zIn);` |
|      1 | 5031 |  |
|      - | 5032 | `/*` |
|      - | 5033 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5034 | ` * Refer to [strcspn()].` |
|      - | 5035 | ` */` |
|     10 | 5036 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5037 |  |
|     11 | 5038 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5039 | `	const char *zIn = zString;` |
|      - | 5040 | `	int i,c;` |
|     12 | 5041 | `	for(;;){` |
|     25 | 5042 | `		if( zString >= zEnd ){` |
|      3 | 5043 | `			break;` |
|      - | 5044 | `		}` |
|      - | 5045 | `		/* Extract current character */` |
|     23 | 5046 | `		c = zString[0];` |
|      - | 5047 | `		/* Perform the lookup */` |
|     51 | 5048 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5049 | `			if( c == zMask[i] ){` |
|      9 | 5050 | `				break;` |
|      - | 5051 | `			}` |
|     15 | 5052 | `		}` |
|     23 | 5053 | `		if( i < nMaskLen ){` |
|      - | 5054 | `			/* Character in the current mask,break immediately */` |
|      9 | 5055 | `			break;` |
|      - | 5056 | `		}` |
|      - | 5057 | `		/* Advance cursor */` |
|     15 | 5058 | `		zString++;` |
|      1 | 5059 | `	}` |
|      - | 5060 | `	/* Longest match */` |
|     11 | 5061 | `	return (int)(zString-zIn);` |
|      1 | 5062 |  |
|      - | 5063 | `/*` |
|      - | 5064 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5065 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5066 | ` *  of characters contained within a given mask.` |
|      - | 5067 | ` * Parameters` |
|      - | 5068 | ` * $str` |
|      - | 5069 | ` *  The input string.` |
|      - | 5070 | ` * $mask` |
|      - | 5071 | ` *  The list of allowable characters.` |
|      - | 5072 | ` * $start` |
|      - | 5073 | ` *  The position in subject to start searching.` |
|      - | 5074 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5075 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5076 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5077 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5078 | ` *  start'th position from the end of subject.` |
|      - | 5079 | ` * $length` |
|      - | 5080 | ` *  The length of the segment from subject to examine.` |
|      - | 5081 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5082 | ` *  characters after the starting position.` |
|      - | 5083 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5084 | ` *  position up to length characters from the end of subject.` |
|      - | 5085 | ` * Return` |
|      - | 5086 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5087 | ` * in mask.` |
|      - | 5088 | ` */` |
|     26 | 5089 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5090 |  |
|      - | 5091 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5092 | `	int iMasklen,iLen;` |
|      - | 5093 | `	SyString sToken;` |
|     27 | 5094 | `	int iCount = 0;` |
|      - | 5095 | `	int rc;` |
|     27 | 5096 | `	if( nArg < 2 ){` |
|      - | 5097 | `		/* Missing agruments,return zero */` |
|      3 | 5098 | `		ph7_result_int(pCtx,0);` |
|      3 | 5099 | `		return PH7_OK;` |
|      - | 5100 | `	}` |
|      - | 5101 | `	/* Extract the target string */` |
|     25 | 5102 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5103 | `	/* Extract the mask */` |
|     25 | 5104 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5105 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5106 | `		/* Nothing to process,return zero */` |
|      7 | 5107 | `		ph7_result_int(pCtx,0);` |
|      7 | 5108 | `		return PH7_OK;` |
|      - | 5109 | `	}` |
|     19 | 5110 | `	if( nArg > 2 ){` |
|      - | 5111 | `		int nOfft;` |
|      - | 5112 | `		/* Extract the offset */` |
|      9 | 5113 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5114 | `		if( nOfft < 0 ){` |
|    ! 0 | 5115 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5116 | `			if( zBase > zString ){` |
|    ! 0 | 5117 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5118 | `				zString = zBase;` |
|    ! 0 | 5119 | `			}else{` |
|      - | 5120 | `				/* Invalid offset */` |
|    ! 0 | 5121 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5122 | `				return PH7_OK;` |
|      - | 5123 | `			}` |
|    ! 0 | 5124 | `		}else{` |
|      9 | 5125 | `			if( nOfft >= iLen ){` |
|      - | 5126 | `				/* Invalid offset */` |
|    ! 0 | 5127 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5128 | `				return PH7_OK;` |
|    ! 0 | 5129 | `			}else{` |
|      - | 5130 | `				/* Update offset */` |
|      9 | 5131 | `				zString += nOfft;` |
|      9 | 5132 | `				iLen -= nOfft;` |
|      - | 5133 | `			}` |
|      - | 5134 | `		}` |
|      9 | 5135 | `		if( nArg > 3 ){` |
|      - | 5136 | `			int iUserlen;` |
|      - | 5137 | `			/* Extract the desired length */` |
|      9 | 5138 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5139 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5140 | `				iLen = iUserlen;` |
|      2 | 5141 | `			}` |
|      4 | 5142 | `		}` |
|      4 | 5143 | `	}` |
|      - | 5144 | `	/* Point to the end of the string */` |
|     19 | 5145 | `	zEnd = &zString[iLen];` |
|      - | 5146 | `	/* Extract the first non-space token */` |
|     19 | 5147 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5148 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5149 | `		/* Compare against the current mask */` |
|     19 | 5150 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5151 | `	}` |
|      - | 5152 | `	/* Longest match */` |
|     19 | 5153 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5154 | `	return PH7_OK;` |
|     14 | 5155 |  |
|      - | 5156 | `/*` |
|      - | 5157 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5158 | ` *  Find length of initial segment not matching mask.` |
|      - | 5159 | ` * Parameters` |
|      - | 5160 | ` * $str` |
|      - | 5161 | ` *  The input string.` |
|      - | 5162 | ` * $mask` |
|      - | 5163 | ` *  The list of not allowed characters.` |
|      - | 5164 | ` * $start` |
|      - | 5165 | ` *  The position in subject to start searching.` |
|      - | 5166 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5167 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5168 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5169 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5170 | ` *  start'th position from the end of subject.` |
|      - | 5171 | ` * $length` |
|      - | 5172 | ` *  The length of the segment from subject to examine.` |
|      - | 5173 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5174 | ` *  characters after the starting position.` |
|      - | 5175 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5176 | ` *  position up to length characters from the end of subject.` |
|      - | 5177 | ` * Return` |
|      - | 5178 | ` *  Returns the length of the segment as an integer.` |
|      - | 5179 | ` */` |
|     16 | 5180 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5181 |  |
|      - | 5182 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5183 | `	int iMasklen,iLen;` |
|      - | 5184 | `	SyString sToken;` |
|     17 | 5185 | `	int iCount = 0;` |
|      - | 5186 | `	int rc;` |
|     17 | 5187 | `	if( nArg < 2 ){` |
|      - | 5188 | `		/* Missing agruments,return zero */` |
|      3 | 5189 | `		ph7_result_int(pCtx,0);` |
|      3 | 5190 | `		return PH7_OK;` |
|      - | 5191 | `	}` |
|      - | 5192 | `	/* Extract the target string */` |
|     15 | 5193 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5194 | `	/* Extract the mask */` |
|     15 | 5195 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5196 | `	if( iLen < 1 ){` |
|      - | 5197 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5198 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5199 | `		return PH7_OK;` |
|      - | 5200 | `	}` |
|     15 | 5201 | `	if( iMasklen < 1 ){` |
|      - | 5202 | `		/* No given mask,return the string length */` |
|      3 | 5203 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5204 | `		return PH7_OK;` |
|      - | 5205 | `	}` |
|     13 | 5206 | `	if( nArg > 2 ){` |
|      - | 5207 | `		int nOfft;` |
|      - | 5208 | `		/* Extract the offset */` |
|     11 | 5209 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5210 | `		if( nOfft < 0 ){` |
|    ! 0 | 5211 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5212 | `			if( zBase > zString ){` |
|    ! 0 | 5213 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5214 | `				zString = zBase;` |
|    ! 0 | 5215 | `			}else{` |
|      - | 5216 | `				/* Invalid offset */` |
|    ! 0 | 5217 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5218 | `				return PH7_OK;` |
|      - | 5219 | `			}` |
|    ! 0 | 5220 | `		}else{` |
|     11 | 5221 | `			if( nOfft >= iLen ){` |
|      - | 5222 | `				/* Invalid offset */` |
|      3 | 5223 | `				ph7_result_int(pCtx,0);` |
|      3 | 5224 | `				return PH7_OK;` |
|    ! 0 | 5225 | `			}else{` |
|      - | 5226 | `				/* Update offset */` |
|      9 | 5227 | `				zString += nOfft;` |
|      9 | 5228 | `				iLen -= nOfft;` |
|      - | 5229 | `			}` |
|      - | 5230 | `		}` |
|      9 | 5231 | `		if( nArg > 3 ){` |
|      - | 5232 | `			int iUserlen;` |
|      - | 5233 | `			/* Extract the desired length */` |
|    ! 0 | 5234 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5235 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5236 | `				iLen = iUserlen;` |
|    ! 0 | 5237 | `			}` |
|    ! 0 | 5238 | `		}` |
|      4 | 5239 | `	}` |
|      - | 5240 | `	/* Point to the end of the string */` |
|     11 | 5241 | `	zEnd = &zString[iLen];` |
|      - | 5242 | `	/* Extract the first non-space token */` |
|     11 | 5243 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5244 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5245 | `		/* Compare against the current mask */` |
|     11 | 5246 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5247 | `	}` |
|      - | 5248 | `	/* Longest match */` |
|     11 | 5249 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5250 | `	return PH7_OK;` |
|      9 | 5251 |  |
|      - | 5252 | `/*` |
|      - | 5253 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5254 | ` *  Search a string for any of a set of characters.` |
|      - | 5255 | ` * Parameters` |
|      - | 5256 | ` *  $haystack` |
|      - | 5257 | ` *   The string where char_list is looked for.` |
|      - | 5258 | ` *  $char_list` |
|      - | 5259 | ` *   This parameter is case sensitive.` |
|      - | 5260 | ` * Return` |
|      - | 5261 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5262 | ` */` |
|      6 | 5263 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5264 |  |
|      - | 5265 | `	const char *zString,*zList,*zEnd;` |
|      - | 5266 | `	int iLen,iListLen,i,c;` |
|      - | 5267 | `	sxu32 nOfft,nMax;` |
|      - | 5268 | `	sxi32 rc;` |
|      7 | 5269 | `	if( nArg < 2 ){` |
|      - | 5270 | `		/* Missing arguments,return FALSE */` |
|      3 | 5271 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5272 | `		return PH7_OK;` |
|      - | 5273 | `	}` |
|      - | 5274 | `	/* Extract the haystack and the char list */` |
|      5 | 5275 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5276 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5277 | `	if( iLen < 1 ){` |
|      - | 5278 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5279 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5280 | `		return PH7_OK;` |
|      - | 5281 | `	}` |
|      - | 5282 | `	/* Point to the end of the string */` |
|      5 | 5283 | `	zEnd = &zString[iLen];` |
|      5 | 5284 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5285 | `	/* perform the requested operation */` |
|     15 | 5286 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5287 | `		c = zList[i];` |
|     11 | 5288 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5289 | `		if( rc == SXRET_OK ){` |
|      5 | 5290 | `			if( nMax < nOfft ){` |
|      3 | 5291 | `				nOfft = nMax;` |
|      1 | 5292 | `			}` |
|      2 | 5293 | `		}` |
|      6 | 5294 | `	}` |
|      5 | 5295 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5296 | `		/* No such substring,return FALSE */` |
|      3 | 5297 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5298 | `	}else{` |
|      - | 5299 | `		/* Return the substring */` |
|      3 | 5300 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5301 | `	}` |
|      5 | 5302 | `	return PH7_OK;` |
|      4 | 5303 |  |
|      - | 5304 | `/* SPDX-SnippetBegin */` |
|      - | 5305 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 5306 | `/* SPDX-License-Identifier: blessing */` |
|      - | 5307 | `/*` |
|      - | 5308 | ` * string soundex(string $str)` |
|      - | 5309 | ` *  Calculate the soundex key of a string.` |
|      - | 5310 | ` * Parameters` |
|      - | 5311 | ` *  $str` |
|      - | 5312 | ` *   The input string.` |
|      - | 5313 | ` * Return` |
|      - | 5314 | ` *  Returns the soundex key as a string.` |
|      - | 5315 | ` * Note:` |
|      - | 5316 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5317 | ` * source tree.` |
|      - | 5318 | ` */` |
|     20 | 5319 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5320 |  |
|      - | 5321 | `	const unsigned char *zIn;` |
|      - | 5322 | `	char zResult[8];` |
|      - | 5323 | `	int i, j;` |
|      - | 5324 | `	static const unsigned char iCode[] = {` |
|      - | 5325 |  |
|      - | 5326 |  |
|      - | 5327 |  |
|      - | 5328 |  |
|      - | 5329 |  |
|      - | 5330 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5331 |  |
|      - | 5332 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5333 | `	};` |
|     21 | 5334 | `	if( nArg < 1 ){` |
|      - | 5335 | `		/* Missing arguments,return the empty string */` |
|      3 | 5336 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5337 | `		return PH7_OK;` |
|      - | 5338 | `	}` |
|     19 | 5339 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5340 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5341 | `	if( zIn[i] ){` |
|     17 | 5342 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5343 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5344 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5345 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5346 | `			if( code>0 ){` |
|     45 | 5347 | `				if( code!=prevcode ){` |
|     33 | 5348 | `					prevcode = (unsigned char)code;` |
|     33 | 5349 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5350 | `				}` |
|     23 | 5351 | `			}else{` |
|     49 | 5352 | `				prevcode = 0;` |
|      - | 5353 | `			}` |
|     47 | 5354 | `		}` |
|     33 | 5355 | `		while( j<4 ){` |
|     17 | 5356 | `			zResult[j++] = '0';` |
|      1 | 5357 | `		}` |
|     17 | 5358 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5359 | `	}else{` |
|      3 | 5360 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5361 | `	}` |
|     19 | 5362 | `	return PH7_OK;` |
|     11 | 5363 |  |
|      - | 5364 | `/* SPDX-SnippetEnd */` |
|      - | 5365 | `/*` |
|      - | 5366 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5367 | ` *  Wraps a string to a given number of characters.` |
|      - | 5368 | ` * Parameters` |
|      - | 5369 | ` *  $str` |
|      - | 5370 | ` *   The input string.` |
|      - | 5371 | ` * $width` |
|      - | 5372 | ` *  The column width.` |
|      - | 5373 | ` * $break` |
|      - | 5374 | ` *  The line is broken using the optional break parameter.` |
|      - | 5375 | ` * Return` |
|      - | 5376 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5377 | ` */` |
|     14 | 5378 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5379 |  |
|      - | 5380 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5381 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5382 | `	if( nArg < 1 ){` |
|      - | 5383 | `		/* Missing arguments,return the empty string */` |
|      3 | 5384 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5385 | `		return PH7_OK;` |
|      - | 5386 | `	}` |
|      - | 5387 | `	/* Extract the input string */` |
|     13 | 5388 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5389 | `	if( iLen < 1 ){` |
|      - | 5390 | `		/* Nothing to process,return the empty string */` |
|      3 | 5391 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5392 | `		return PH7_OK;` |
|      - | 5393 | `	}` |
|      - | 5394 | `	/* Chunk length */` |
|     11 | 5395 | `	iChunk = 75;` |
|     11 | 5396 | `	iBreaklen = 0;` |
|     11 | 5397 | `	zBreak = ""; /* cc warning */` |
|     11 | 5398 | `	if( nArg > 1 ){` |
|     11 | 5399 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5400 | `		if( iChunk < 1 ){` |
|    ! 0 | 5401 | `			iChunk = 75;` |
|    ! 0 | 5402 | `		}` |
|     11 | 5403 | `		if( nArg > 2 ){` |
|      3 | 5404 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5405 | `		}` |
|      5 | 5406 | `	}` |
|     11 | 5407 | `	if( iBreaklen < 1 ){` |
|      - | 5408 | `		/* Set a default column break */` |
|      - | 5409 | `#ifdef __WINNT__` |
|      1 | 5410 | `		zBreak = "\r\n";` |
|      1 | 5411 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5412 | `#else` |
|      8 | 5413 | `		zBreak = "\n";` |
|      8 | 5414 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5415 | `#endif` |
|      4 | 5416 | `	}` |
|      - | 5417 | `	/* Perform the requested operation */` |
|     11 | 5418 | `	zEnd = &zIn[iLen];` |
|     41 | 5419 | `	for(;;){` |
|      - | 5420 | `		int nMax;` |
|     47 | 5421 | `		if( zIn >= zEnd ){` |
|      - | 5422 | `			/* No more input to process */` |
|     11 | 5423 | `			break;` |
|      - | 5424 | `		}` |
|     37 | 5425 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5426 | `		if( iChunk > nMax ){` |
|     11 | 5427 | `			iChunk = nMax;` |
|      5 | 5428 | `		}` |
|      - | 5429 | `		/* Append the column first */` |
|     37 | 5430 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5431 | `		/* Advance the cursor */` |
|     37 | 5432 | `		zIn += iChunk;` |
|     37 | 5433 | `		if( zIn < zEnd ){` |
|      - | 5434 | `			/* Append the line break */` |
|     27 | 5435 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5436 | `		}` |
|      1 | 5437 | `	}` |
|     11 | 5438 | `	return PH7_OK;` |
|      8 | 5439 |  |
|      - | 5440 | `/*` |
|      - | 5441 | ` * Check if the given character is a member of the given mask.` |
|      - | 5442 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5443 | ` * Refer to [strtok()].` |
|      - | 5444 | ` */` |
|     30 | 5445 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5446 |  |
|      - | 5447 | `	int i;` |
|     57 | 5448 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5449 | `		if( c == zMask[i] ){` |
|     13 | 5450 | `			if( pOfft ){` |
|      5 | 5451 | `				*pOfft = i;` |
|      2 | 5452 | `			}` |
|     13 | 5453 | `			return TRUE;` |
|      - | 5454 | `		}` |
|     14 | 5455 | `	}` |
|     19 | 5456 | `	return FALSE;` |
|     16 | 5457 |  |
|      - | 5458 | `/*` |
|      - | 5459 | ` * Extract a single token from the input stream.` |
|      - | 5460 | ` * Refer to [strtok()].` |
|      - | 5461 | ` */` |
|      6 | 5462 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5463 |  |
|      7 | 5464 | `	const char *zIn = *pzIn;` |
|      - | 5465 | `	const char *zPtr;` |
|      - | 5466 | `	/* Ignore leading delimiter */` |
|     11 | 5467 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5468 | `		zIn++;` |
|      1 | 5469 | `	}` |
|      7 | 5470 | `	if( zIn >= zEnd ){` |
|      - | 5471 | `		/* End of input */` |
|    ! 0 | 5472 | `		return SXERR_EOF;` |
|      - | 5473 | `	}` |
|      7 | 5474 | `	zPtr = zIn;` |
|      - | 5475 | `	/* Extract the token */` |
|     13 | 5476 | `	while( zIn < zEnd ){` |
|     11 | 5477 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5478 | `			/* UTF-8 stream */` |
|    ! 0 | 5479 | `			zIn++;` |
|    ! 0 | 5480 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5481 | `		}else{` |
|     11 | 5482 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5483 | `				break;` |
|      - | 5484 | `			}` |
|      7 | 5485 | `			zIn++;` |
|      - | 5486 | `		}` |
|      1 | 5487 | `	}` |
|      7 | 5488 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5489 | `	/* Update the cursor */` |
|      7 | 5490 | `	*pzIn = zIn;` |
|      - | 5491 | `	/* Return to the caller */` |
|      7 | 5492 | `	return SXRET_OK;` |
|      4 | 5493 |  |
|      - | 5494 | `/* strtok auxiliary private data */` |
|      - | 5495 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5496 | `struct strtok_aux_data` |
|      - | 5497 |  |
|      - | 5498 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5499 | `	const char *zIn;   /* Current input stream */` |
|      - | 5500 | `	const char *zEnd;  /* End of input */` |
|      - | 5501 | `};` |
|      - | 5502 | `/*` |
|      - | 5503 | ` * string strtok(string $str,string $token)` |
|      - | 5504 | ` * string strtok(string $token)` |
|      - | 5505 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5506 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5507 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5508 | ` *  words by using the space character as the token.` |
|      - | 5509 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5510 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5511 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5512 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5513 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5514 | ` *  the argument are found.` |
|      - | 5515 | ` * Parameters` |
|      - | 5516 | ` *  $str` |
|      - | 5517 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5518 | ` * $token` |
|      - | 5519 | ` *  The delimiter used when splitting up str.` |
|      - | 5520 | ` * Return` |
|      - | 5521 | ` *   Current token or FALSE on EOF.` |
|      - | 5522 | ` */` |
|      8 | 5523 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5524 |  |
|      - | 5525 | `	strtok_aux_data *pAux;` |
|      - | 5526 | `	const char *zMask;` |
|      - | 5527 | `	SyString sToken;` |
|      - | 5528 | `	int nMasklen;` |
|      - | 5529 | `	sxi32 rc;` |
|      9 | 5530 | `	if( nArg < 2 ){` |
|      - | 5531 | `		/* Extract top aux data */` |
|      7 | 5532 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5533 | `		if( pAux == 0 ){` |
|      - | 5534 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5535 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5536 | `			return PH7_OK;` |
|      - | 5537 | `		}` |
|      7 | 5538 | `		nMasklen = 0;` |
|      7 | 5539 | `		zMask = ""; /* cc warning */` |
|      7 | 5540 | `		if( nArg > 0 ){` |
|      - | 5541 | `			/* Extract the mask */` |
|      5 | 5542 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5543 | `		}` |
|      7 | 5544 | `		if( nMasklen < 1 ){` |
|      - | 5545 | `			/* Invalid mask,return FALSE */` |
|      3 | 5546 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5547 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5548 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5549 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5550 | `			return PH7_OK;` |
|      - | 5551 | `		}` |
|      - | 5552 | `		/* Extract the token */` |
|      5 | 5553 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5554 | `		if( rc != SXRET_OK ){` |
|      - | 5555 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5556 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5557 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5558 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5559 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5560 | `		}else{` |
|      - | 5561 | `			/* Return the extracted token */` |
|      5 | 5562 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5563 | `		}` |
|      3 | 5564 | `	}else{` |
|      - | 5565 | `		const char *zInput,*zCur;` |
|      - | 5566 | `		char *zDup;` |
|      - | 5567 | `		int nLen;` |
|      - | 5568 | `		/* Extract the raw input */` |
|      3 | 5569 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5570 | `		if( nLen < 1 ){` |
|      - | 5571 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5572 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5573 | `			return PH7_OK;` |
|      - | 5574 | `		}` |
|      - | 5575 | `		/* Extract the mask */` |
|      3 | 5576 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5577 | `		if( nMasklen < 1 ){` |
|      - | 5578 | `			/* Set a default mask */` |
|      - | 5579 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5580 | `			zMask = TOK_MASK;` |
|    ! 0 | 5581 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5582 | `#undef TOK_MASK` |
|    ! 0 | 5583 | `		}` |
|      - | 5584 | `		/* Extract a single token */` |
|      3 | 5585 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5586 | `		if( rc != SXRET_OK ){` |
|      - | 5587 | `			/* Empty input */` |
|    ! 0 | 5588 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5589 | `			return PH7_OK;` |
|    ! 0 | 5590 | `		}else{` |
|      - | 5591 | `			/* Return the extracted token */` |
|      3 | 5592 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5593 | `		}` |
|      - | 5594 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5595 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5596 | `		if( pAux ){` |
|      3 | 5597 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5598 | `			if( nLen < 1 ){` |
|    ! 0 | 5599 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5600 | `				return PH7_OK;` |
|      - | 5601 | `			}` |
|      - | 5602 | `			/* Duplicate input */` |
|      3 | 5603 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5604 | `			if( zDup  ){` |
|      3 | 5605 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5606 | `				/* Register the aux data */` |
|      3 | 5607 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5608 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5609 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5610 | `			}` |
|      1 | 5611 | `		}` |
|      - | 5612 | `	}` |
|      7 | 5613 | `	return PH7_OK;` |
|      5 | 5614 |  |
|      - | 5615 | `/*` |
|      - | 5616 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5617 | ` *  Pad a string to a certain length with another string` |
|      - | 5618 | ` * Parameters` |
|      - | 5619 | ` *  $input` |
|      - | 5620 | ` *   The input string.` |
|      - | 5621 | ` * $pad_length` |
|      - | 5622 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5623 | ` *   string, no padding takes place.` |
|      - | 5624 | ` * $pad_string` |
|      - | 5625 | ` *   Note:` |
|      - | 5626 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5627 | ` *    divided by the pad_string's length.` |
|      - | 5628 | ` * $pad_type` |
|      - | 5629 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5630 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5631 | ` * Return` |
|      - | 5632 | ` *  The padded string.` |
|      - | 5633 | ` */` |
|     10 | 5634 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5635 |  |
|      - | 5636 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5637 | `	const char *zIn,*zPad;` |
|     11 | 5638 | `	if( nArg < 2 ){` |
|      - | 5639 | `		/* Missing arguments,return the empty string */` |
|      5 | 5640 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5641 | `		return PH7_OK;` |
|      - | 5642 | `	}` |
|      - | 5643 | `	/* Extract the target string */` |
|      7 | 5644 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5645 | `	/* Padding length */` |
|      7 | 5646 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5647 | `	if( iPadlen > 0 ){` |
|      5 | 5648 | `		iPadlen -= iLen;` |
|      2 | 5649 | `	}` |
|      7 | 5650 | `	if( iPadlen < 1  ){` |
|      - | 5651 | `		/* Return the string verbatim */` |
|      3 | 5652 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5653 | `		return PH7_OK;` |
|      - | 5654 | `	}` |
|      5 | 5655 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5656 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5657 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5658 | `	if( nArg > 2 ){` |
|      - | 5659 | `		/* Padding string */` |
|      5 | 5660 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5661 | `		if( iStrpad < 1 ){` |
|      - | 5662 | `			/* Empty string */` |
|    ! 0 | 5663 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5664 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5665 | `		}` |
|      5 | 5666 | `		if( nArg > 3 ){` |
|      - | 5667 | `			/* Padd type */` |
|      5 | 5668 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5669 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5670 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5671 | `			}` |
|      2 | 5672 | `		}` |
|      2 | 5673 | `	}` |
|      5 | 5674 | `	iDiv = 1;` |
|      5 | 5675 | `	if( iType == 2 ){` |
|    ! 0 | 5676 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5677 | `	}` |
|      - | 5678 | `	/* Perform the requested operation */` |
|      5 | 5679 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5680 | `		jPad = iStrpad;` |
|      5 | 5681 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5682 | `			/* Padding */` |
|      5 | 5683 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5684 | `				break;` |
|      - | 5685 | `			}` |
|      3 | 5686 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5687 | `		}` |
|      3 | 5688 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5689 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5690 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5691 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5692 | `					jPad = iStrpad;` |
|    ! 0 | 5693 | `				}` |
|      3 | 5694 | `				if( jPad < 1){` |
|    ! 0 | 5695 | `					break;` |
|      - | 5696 | `				}` |
|      3 | 5697 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5698 | `			}` |
|      1 | 5699 | `		}` |
|      1 | 5700 | `	}` |
|      5 | 5701 | `	if( iLen > 0 ){` |
|      - | 5702 | `		/* Append the input string */` |
|      5 | 5703 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5704 | `	}` |
|      5 | 5705 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5706 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5707 | `			/* Padding */` |
|      5 | 5708 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5709 | `				break;` |
|      - | 5710 | `			}` |
|      3 | 5711 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5712 | `		}` |
|      5 | 5713 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5714 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5715 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5716 | `				jPad = iStrpad;` |
|    ! 0 | 5717 | `			}` |
|      3 | 5718 | `			if( jPad < 1){` |
|    ! 0 | 5719 | `				break;` |
|      - | 5720 | `			}` |
|      3 | 5721 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5722 | `		}` |
|      1 | 5723 | `	}` |
|      5 | 5724 | `	return PH7_OK;` |
|      6 | 5725 |  |
|      - | 5726 | `/*` |
|      - | 5727 | ` * String replacement private data.` |
|      - | 5728 | ` */` |
|      - | 5729 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5730 | `struct str_replace_data` |
|      - | 5731 |  |
|      - | 5732 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5733 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5734 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5735 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5736 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5737 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5738 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 5739 | `};` |
|      - | 5740 | `/*` |
|      - | 5741 | ` * Remove a substring.` |
|      - | 5742 | ` */` |
|      - | 5743 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5744 | `	for(;;){\` |
|      - | 5745 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5746 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5747 | `		++OFFT;\` |
|      - | 5748 | `	}\` |
|      - | 5749 |  |
|      - | 5750 | `/*` |
|      - | 5751 | ` * Shift right and insert algorithm.` |
|      - | 5752 | ` */` |
|      - | 5753 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5754 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5755 | `		for(;;){\` |
|      - | 5756 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5757 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5758 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5759 | `			--INLEN; \` |
|      - | 5760 | `		}\` |
|      - | 5761 | `		for(;;){\` |
|      - | 5762 | `				if(ELEN < 1) { break; }\` |
|      - | 5763 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5764 | `				OFFT++;\` |
|      - | 5765 | `				ENTRY++;\` |
|      - | 5766 | `				--ELEN;\` |
|      - | 5767 | `		}\` |
|      - | 5768 |  |
|      - | 5769 | `/*` |
|      - | 5770 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5771 | ` * replacement string [i.e: zReplace].` |
|      - | 5772 | ` */` |
|     38 | 5773 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 5774 |  |
|     39 | 5775 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5776 | `	sxu32 n,m;` |
|     39 | 5777 | `	n = SyBlobLength(pWorker);` |
|     39 | 5778 | `	m = nOfft;` |
|      - | 5779 | `	/* Delete the old entry */` |
|    475 | 5780 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 5781 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 5782 | `	if( nReplen > 0 ){` |
|     33 | 5783 | `		sxi32 iRep = nReplen;` |
|      - | 5784 | `		sxi32 rc;` |
|      - | 5785 | `		/*` |
|      - | 5786 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 5787 | `		 * string.` |
|      - | 5788 | `		 */` |
|     33 | 5789 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 5790 | `		if( rc != SXRET_OK ){` |
|      - | 5791 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 5792 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 5793 | `			return rc;` |
|      - | 5794 | `		}` |
|      - | 5795 | `		/* Perform the insertion now */` |
|     33 | 5796 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 5797 | `		n = SyBlobLength(pWorker);` |
|    163 | 5798 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 5799 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 5800 | `	}` |
|     39 | 5801 | `	return SXRET_OK;` |
|     20 | 5802 |  |
|      - | 5803 | `/*` |
|      - | 5804 | ` * String replacement walker callback.` |
|      - | 5805 | ` * The following callback is invoked for each array entry that hold` |
|      - | 5806 | ` * the replace string.` |
|      - | 5807 | ` * Refer to the strtr() implementation for more information.` |
|      - | 5808 | ` */` |
|      8 | 5809 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5810 |  |
|      9 | 5811 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 5812 | `	const char *zTarget,*zReplace;` |
|      - | 5813 | `	SyBlob *pWorker;` |
|      - | 5814 | `	int tLen,nLen;` |
|      - | 5815 | `	sxu32 nOfft;` |
|      - | 5816 | `	sxi32 rc;` |
|      - | 5817 | `	/* Point to the working buffer */` |
|      9 | 5818 | `	pWorker = pRepData->pWorker;` |
|      9 | 5819 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 5820 | `		/* Target and replace must be a string */` |
|      3 | 5821 | `		return PH7_OK;` |
|      - | 5822 | `	}` |
|      - | 5823 | `	/* Extract the target and the replace */` |
|      7 | 5824 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 5825 | `	if( tLen < 1 ){` |
|      - | 5826 | `		/* Empty target,return immediately */` |
|    ! 0 | 5827 | `		return PH7_OK;` |
|      - | 5828 | `	}` |
|      - | 5829 | `	/* Perform a pattern search */` |
|      7 | 5830 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 5831 | `	if( rc != SXRET_OK ){` |
|      - | 5832 | `		/* Pattern not found */` |
|    ! 0 | 5833 | `		return PH7_OK;` |
|      - | 5834 | `	}` |
|      - | 5835 | `	/* Extract the replace string */` |
|      7 | 5836 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 5837 | `	/* Perform the replace process */` |
|      7 | 5838 | `	rc = StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      7 | 5839 | `	if( rc != SXRET_OK ){` |
|      - | 5840 | `		/* Allocation failure: carry it out and stop the walk */` |
|    ! 0 | 5841 | `		pRepData->rc = rc;` |
|    ! 0 | 5842 | `		return rc;` |
|      - | 5843 | `	}` |
|      - | 5844 | `	/* All done */` |
|      7 | 5845 | `	return PH7_OK;` |
|      5 | 5846 |  |
|      - | 5847 | `/*` |
|      - | 5848 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 5849 | ` * to collect search/replace string.` |
|      - | 5850 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 5851 | ` */` |
|     26 | 5852 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5853 |  |
|     27 | 5854 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 5855 | `	SyString sWorker;` |
|      - | 5856 | `	const char *zIn;` |
|      - | 5857 | `	int nByte;` |
|      - | 5858 | `	/* Extract a string representation of the given argument */` |
|     27 | 5859 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 5860 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 5861 | `	if( nByte > 0 ){` |
|      - | 5862 | `		char *zDup;` |
|      - | 5863 | `		/* Duplicate the chunk */` |
|     25 | 5864 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 5865 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 5866 | `			);` |
|     25 | 5867 | `		if( zDup == 0 ){` |
|      - | 5868 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 5869 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 5870 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 5871 | `			return SXERR_MEM;` |
|      - | 5872 | `		}` |
|     25 | 5873 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 5874 | `		/* Save the chunk */` |
|     25 | 5875 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 5876 | `	}` |
|      - | 5877 | `	/* Save for later processing */` |
|     27 | 5878 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 5879 | `	/* All done */` |
|     13 | 5880 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 5881 | `	return PH7_OK;` |
|     14 | 5882 |  |
|      - | 5883 | `/*` |
|      - | 5884 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5885 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5886 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 5887 | ` * Parameters` |
|      - | 5888 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 5889 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 5890 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 5891 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 5892 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 5893 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 5894 | ` * $search` |
|      - | 5895 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 5896 | ` *  to designate multiple needles.` |
|      - | 5897 | ` * $replace` |
|      - | 5898 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 5899 | ` *  to designate multiple replacements.` |
|      - | 5900 | ` * $subject` |
|      - | 5901 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 5902 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 5903 | ` *  of subject, and the return value is an array as well.` |
|      - | 5904 | ` * $count (Not used)` |
|      - | 5905 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 5906 | ` * Return` |
|      - | 5907 | ` * This function returns a string or an array with the replaced values.` |
|      - | 5908 | ` */` |
|  22890 | 5909 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5910 |  |
|      - | 5911 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 5912 | `	ProcStringMatch xMatch;` |
|      - | 5913 | `	const char *zIn,*zFunc;` |
|      - | 5914 | `	str_replace_data sRep;` |
|      - | 5915 | `	SyBlob sWorker;` |
|      - | 5916 | `	SySet sReplace;` |
|      - | 5917 | `	SySet sSearch;` |
|      - | 5918 | `	int rep_str;` |
|      - | 5919 | `	int nByte;` |
|      - | 5920 | `	sxi32 rc;` |
|  22895 | 5921 | `	if( nArg < 3 ){` |
|      - | 5922 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 5923 | `		ph7_result_null(pCtx);` |
|      7 | 5924 | `		return PH7_OK;` |
|      - | 5925 | `	}` |
|      - | 5926 | `	/* Initialize fields */` |
|  22889 | 5927 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  22889 | 5928 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  22889 | 5929 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  22889 | 5930 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  22889 | 5931 | `	sRep.pCtx = pCtx;` |
|  22889 | 5932 | `	sRep.pCollector = &sSearch;` |
|  22889 | 5933 | `	rep_str = 0;` |
|      - | 5934 | `	/* Extract the subject */` |
|  22889 | 5935 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  22889 | 5936 | `	if( nByte < 1 ){` |
|      - | 5937 | `		/* Nothing to replace,return the empty string */` |
|     29 | 5938 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 5939 | `		return PH7_OK;` |
|      - | 5940 | `	}` |
|      - | 5941 | `	/* Copy the subject */` |
|  22861 | 5942 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 5943 | `	/* Search string */` |
|  22861 | 5944 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 5945 | `		/* Collect search string */` |
|      9 | 5946 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 5947 | `	}else{` |
|      - | 5948 | `		/* Single pattern */` |
|  22853 | 5949 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  22853 | 5950 | `		if( nByte < 1 ){` |
|      - | 5951 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 5952 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 5953 | `			return PH7_OK;` |
|      - | 5954 | `		}` |
|  22849 | 5955 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5956 | `		/* Save for later processing */` |
|  22849 | 5957 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 5958 | `	}` |
|      - | 5959 | `	/* Replace string */` |
|  22857 | 5960 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 5961 | `		/* Collect replace string */` |
|      7 | 5962 | `		sRep.pCollector = &sReplace;` |
|      7 | 5963 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 5964 | `	}else{` |
|      - | 5965 | `		/* Single needle */` |
|  22851 | 5966 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  22851 | 5967 | `		rep_str = 1;` |
|  22851 | 5968 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5969 | `		/* Save for later processing */` |
|  22851 | 5970 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 5971 | `	}` |
|      - | 5972 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  22857 | 5973 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 5974 | `		SySetRelease(&sSearch);` |
|    ! 0 | 5975 | `		SySetRelease(&sReplace);` |
|    ! 0 | 5976 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 5977 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5978 | `	}` |
|      - | 5979 | `	/* Reset loop cursors */` |
|  22857 | 5980 | `	SySetResetCursor(&sSearch);` |
|  22857 | 5981 | `	SySetResetCursor(&sReplace);` |
|  22857 | 5982 | `	pReplace = pSearch = 0; /* cc warning */` |
|  22857 | 5983 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 5984 | `	/* Extract function name */` |
|  22857 | 5985 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 5986 | `	/* Set the default pattern match routine */` |
|  22857 | 5987 | `	xMatch = SyBlobSearch;` |
|  22857 | 5988 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5989 | `		/* Case insensitive pattern match */` |
|     11 | 5990 | `		xMatch = iPatternMatch;` |
|      5 | 5991 | `	}` |
|      - | 5992 | `	/* Start the replace process */` |
|  45717 | 5993 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 5994 | `		sxu32 nCount,nOfft;` |
|  22865 | 5995 | `		if( pSearch->nByte <  1 ){` |
|      - | 5996 | `			/* Empty string,ignore */` |
|      3 | 5997 | `			continue;` |
|      - | 5998 | `		}` |
|      - | 5999 | `		/* Extract the replace string */` |
|  22863 | 6000 | `		if( rep_str ){` |
|  22853 | 6001 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  11429 | 6002 | `		}else{` |
|     11 | 6003 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6004 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6005 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6006 | `				 */` |
|      3 | 6007 | `				pReplace = 0;` |
|      1 | 6008 | `			}` |
|      - | 6009 | `		}` |
|  22863 | 6010 | `		if( pReplace == 0 ){` |
|      - | 6011 | `			/* Use an empty string instead */` |
|      3 | 6012 | `			pReplace = &sTemp;` |
|      1 | 6013 | `		}` |
|  22863 | 6014 | `		nOfft = nCount = 0;` |
|  11445 | 6015 | `		for(;;){` |
|  22895 | 6016 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6017 | `				break;` |
|      - | 6018 | `			}` |
|      - | 6019 | `			/* Perform a pattern lookup */` |
|  34322 | 6020 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  22878 | 6021 | `				pSearch->nByte,&nOfft);` |
|  22883 | 6022 | `			if( rc != SXRET_OK ){` |
|      - | 6023 | `				/* Pattern not found */` |
|  22851 | 6024 | `				break;` |
|      - | 6025 | `			}` |
|      - | 6026 | `			/* Perform the replace operation */` |
|     33 | 6027 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 6028 | `			if( rc != SXRET_OK ){` |
|      - | 6029 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 6030 | `				SySetRelease(&sSearch);` |
|    ! 0 | 6031 | `				SySetRelease(&sReplace);` |
|    ! 0 | 6032 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 6033 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 6034 | `			}` |
|      - | 6035 | `			/* Increment offset counter */` |
|     33 | 6036 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6037 | `		}` |
|      5 | 6038 | `	}` |
|      - | 6039 | `	/* All done,clean-up the mess left behind */` |
|  22857 | 6040 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  22857 | 6041 | `	SySetRelease(&sSearch);` |
|  22857 | 6042 | `	SySetRelease(&sReplace);` |
|  22857 | 6043 | `	SyBlobRelease(&sWorker);` |
|  22857 | 6044 | `	if( rc != PH7_OK ){` |
|    ! 0 | 6045 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6046 | `	}` |
|  22857 | 6047 | `	return PH7_OK;` |
|  11450 | 6048 |  |
|      - | 6049 | `/*` |
|      - | 6050 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6051 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6052 | ` *  Translate characters or replace substrings.` |
|      - | 6053 | ` * Parameters` |
|      - | 6054 | ` *  $str` |
|      - | 6055 | ` *  The string being translated.` |
|      - | 6056 | ` * $from` |
|      - | 6057 | ` *  The string being translated to to.` |
|      - | 6058 | ` * $to` |
|      - | 6059 | ` *  The string replacing from.` |
|      - | 6060 | ` * $replace_pairs` |
|      - | 6061 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6062 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6063 | ` * Return` |
|      - | 6064 | ` *  The translated string.` |
|      - | 6065 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6066 | ` */` |
|     12 | 6067 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6068 |  |
|      - | 6069 | `	const char *zIn;` |
|      - | 6070 | `	int nLen;` |
|     13 | 6071 | `	if( nArg < 1 ){` |
|      - | 6072 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6073 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6074 | `		return PH7_OK;` |
|      - | 6075 | `	}` |
|      7 | 6076 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6077 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6078 | `		/* Invalid arguments */` |
|    ! 0 | 6079 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6080 | `		return PH7_OK;` |
|      - | 6081 | `	}` |
|      9 | 6082 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6083 | `		str_replace_data sRepData;` |
|      - | 6084 | `		SyBlob sWorker;` |
|      - | 6085 | `		sxi32 rc;` |
|      - | 6086 | `		/* Initilaize the working buffer */` |
|      5 | 6087 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6088 | `		/* Copy raw string */` |
|      5 | 6089 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6090 | `		/* Init our replace data instance */` |
|      5 | 6091 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6092 | `		sRepData.xMatch = SyBlobSearch;` |
|      5 | 6093 | `		sRepData.rc = SXRET_OK;` |
|      - | 6094 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6095 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      5 | 6096 | `		if( sRepData.rc != SXRET_OK ){` |
|      - | 6097 | `			/* Allocation failure during replacement: surface a fatal */` |
|    ! 0 | 6098 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 6099 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6100 | `		}` |
|      - | 6101 | `		/* All done, return the result string */` |
|      7 | 6102 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6103 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6104 | `		/* Clean-up */` |
|      5 | 6105 | `		SyBlobRelease(&sWorker);` |
|      5 | 6106 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6107 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6108 | `		}` |
|      3 | 6109 | `	}else{` |
|      - | 6110 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6111 | `		const char *zFrom,*zTo;` |
|      3 | 6112 | `		if( nArg < 3 ){` |
|      - | 6113 | `			/* Nothing to replace */` |
|    ! 0 | 6114 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6115 | `			return PH7_OK;` |
|      - | 6116 | `		}` |
|      - | 6117 | `		/* Extract given arguments */` |
|      3 | 6118 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6119 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6120 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6121 | `			/* Nothing to replace */` |
|    ! 0 | 6122 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6123 | `			return PH7_OK;` |
|      - | 6124 | `		}` |
|      - | 6125 | `		/* Start the replace process */` |
|     13 | 6126 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6127 | `			c = zIn[i];` |
|     11 | 6128 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6129 | `				if ( iOfft < tlen ){` |
|      5 | 6130 | `					c = zTo[iOfft];` |
|      2 | 6131 | `				}` |
|      2 | 6132 | `			}` |
|     11 | 6133 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6134 |  |
|      6 | 6135 | `		}` |
|      - | 6136 | `	}` |
|      7 | 6137 | `	return PH7_OK;` |
|      7 | 6138 |  |
|      - | 6139 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6140 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6141 | `/*` |
|      - | 6142 | ` * Parse an INI string.` |
|      - | 6143 |  |
|      - | 6144 | ` * According to wikipedia` |
|      - | 6145 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6146 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6147 | ` *  Format` |
|      - | 6148 | `*    Properties` |
|      - | 6149 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6150 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6151 | `*     Example:` |
|      - | 6152 | `*      name=value` |
|      - | 6153 | `*    Sections` |
|      - | 6154 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6155 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6156 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6157 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6158 | `*     Example:` |
|      - | 6159 | `*      [section]` |
|      - | 6160 | `*   Comments` |
|      - | 6161 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6162 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6163 | `*/` |
|     12 | 6164 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6165 |  |
|      - | 6166 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6167 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6168 | `	SyHashEntry *pEntry;` |
|      - | 6169 | `	SyString sEntry;` |
|      - | 6170 | `	SyHash sHash;` |
|      - | 6171 | `	int c;` |
|      - | 6172 | `	/* Create an empty array and worker variables */` |
|     13 | 6173 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6174 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6175 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6176 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6177 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 6178 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6179 | `	}` |
|     13 | 6180 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6181 | `	pCur = pArray;` |
|      - | 6182 | `	/* Start the parse process */` |
|     21 | 6183 | `	for(;;){` |
|      - | 6184 | `		/* Ignore leading white spaces */` |
|     69 | 6185 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6186 | `			zIn++;` |
|      1 | 6187 | `		}` |
|     43 | 6188 | `		if( zIn >= zEnd ){` |
|      - | 6189 | `			/* No more input to process */` |
|     13 | 6190 | `			break;` |
|      - | 6191 | `		}` |
|     31 | 6192 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6193 | `			/* Comment til the end of line */` |
|    ! 0 | 6194 | `			zIn++;` |
|    ! 0 | 6195 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6196 | `				zIn++;` |
|    ! 0 | 6197 | `			}` |
|    ! 0 | 6198 | `			continue;` |
|      - | 6199 | `		}` |
|      - | 6200 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6201 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6202 | `		if( zIn[0] == '[' ){` |
|      - | 6203 | `			/* Section: Extract the section name */` |
|      9 | 6204 | `			zIn++;` |
|      9 | 6205 | `			zCur = zIn;` |
|     73 | 6206 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6207 | `				zIn++;` |
|      1 | 6208 | `			}` |
|      9 | 6209 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6210 | `				/* Save the section name */` |
|      5 | 6211 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6212 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6213 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6214 | `				if( sEntry.nByte > 0 ){` |
|      - | 6215 | `					/* Associate an array with the section */` |
|      5 | 6216 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6217 | `					if( pSection ){` |
|      5 | 6218 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6219 | `						pCur = pSection;` |
|      2 | 6220 | `					}` |
|      2 | 6221 | `				}` |
|      2 | 6222 | `			}` |
|      9 | 6223 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6224 | `		}else{` |
|      - | 6225 | `			ph7_value *pOldCur;` |
|      - | 6226 | `			int is_array;` |
|      - | 6227 | `			int iLen;` |
|      - | 6228 | `			/* Properties */` |
|     23 | 6229 | `			is_array = 0;` |
|     23 | 6230 | `			zCur = zIn;` |
|     23 | 6231 | `			iLen = 0; /* cc warning */` |
|     23 | 6232 | `			pOldCur = pCur;` |
|    155 | 6233 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6234 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6235 | `					/* Array */` |
|    ! 0 | 6236 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6237 | `					is_array = 1;` |
|    ! 0 | 6238 | `					if( iLen > 0 ){` |
|    ! 0 | 6239 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6240 | `						/* Query the hashtable */` |
|    ! 0 | 6241 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6242 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6243 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6244 | `						if( pEntry ){` |
|    ! 0 | 6245 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6246 | `						}else{` |
|      - | 6247 | `							/* Create an empty array */` |
|    ! 0 | 6248 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6249 | `							if( pvArr ){` |
|      - | 6250 | `								/* Save the entry */` |
|    ! 0 | 6251 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6252 | `								/* Insert the entry */` |
|    ! 0 | 6253 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6254 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6255 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6256 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6257 | `							}` |
|      - | 6258 | `						}` |
|    ! 0 | 6259 | `						if( pvArr ){` |
|    ! 0 | 6260 | `							pCur = pvArr;` |
|    ! 0 | 6261 | `						}` |
|    ! 0 | 6262 | `					}` |
|    ! 0 | 6263 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6264 | `						zIn++;` |
|    ! 0 | 6265 | `					}` |
|    ! 0 | 6266 | `				}` |
|    133 | 6267 | `				zIn++;` |
|      1 | 6268 | `			}` |
|     23 | 6269 | `			if( !is_array ){` |
|     23 | 6270 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6271 | `			}` |
|      - | 6272 | `			/* Trim the key */` |
|     23 | 6273 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6274 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6275 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6276 | `				if( !is_array ){` |
|      - | 6277 | `					/* Save the key name */` |
|     23 | 6278 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6279 | `				}` |
|      - | 6280 | `				/* extract key value */` |
|     23 | 6281 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6282 | `				zIn++; /* '=' */` |
|     39 | 6283 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6284 | `					zIn++;` |
|      1 | 6285 | `				}` |
|     23 | 6286 | `				if( zIn < zEnd ){` |
|     21 | 6287 | `					zCur = zIn;` |
|     21 | 6288 | `					c = zIn[0];` |
|     21 | 6289 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6290 | `						zIn++;` |
|      - | 6291 | `						/* Delimit the value */` |
|    ! 0 | 6292 | `						while( zIn < zEnd ){` |
|    ! 0 | 6293 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6294 | `								break;` |
|      - | 6295 | `							}` |
|    ! 0 | 6296 | `							zIn++;` |
|    ! 0 | 6297 | `						}` |
|    ! 0 | 6298 | `						if( zIn < zEnd ){` |
|    ! 0 | 6299 | `							zIn++;` |
|    ! 0 | 6300 | `						}` |
|    ! 0 | 6301 | `					}else{` |
|    125 | 6302 | `						while( zIn < zEnd ){` |
|    123 | 6303 | `							if( zIn[0] == '\n' ){` |
|     19 | 6304 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6305 | `									break;` |
|    ! 0 | 6306 | `								}` |
|    105 | 6307 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6308 | `								/* Inline comments */` |
|    ! 0 | 6309 | `								break;` |
|      - | 6310 | `							}` |
|    105 | 6311 | `							zIn++;` |
|      1 | 6312 | `						}` |
|      - | 6313 | `					}` |
|      - | 6314 | `					/* Trim the value */` |
|     21 | 6315 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6316 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6317 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6318 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6319 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6320 | `					}` |
|     21 | 6321 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6322 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6323 | `					}` |
|      - | 6324 | `					/* Insert the key and it's value */` |
|     21 | 6325 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6326 | `				}` |
|     12 | 6327 | `			}else{` |
|    ! 0 | 6328 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6329 | `					zIn++;` |
|    ! 0 | 6330 | `				}` |
|      - | 6331 | `			}` |
|     23 | 6332 | `			pCur = pOldCur;` |
|      - | 6333 | `		}` |
|      1 | 6334 | `	}` |
|     13 | 6335 | `	SyHashRelease(&sHash);` |
|      - | 6336 | `	/* Return the parse of the INI string */` |
|     13 | 6337 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6338 | `	return SXRET_OK;` |
|      7 | 6339 |  |
|      - | 6340 | `/*` |
|      - | 6341 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6342 | ` *  Parse a configuration string.` |
|      - | 6343 | ` * Parameters` |
|      - | 6344 | ` *  $ini` |
|      - | 6345 | ` *   The contents of the ini file being parsed.` |
|      - | 6346 | ` *  $process_sections` |
|      - | 6347 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6348 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6349 | ` *  $scanner_mode (Not used)` |
|      - | 6350 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6351 | ` *   then option values will not be parsed.` |
|      - | 6352 | ` * Return` |
|      - | 6353 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6354 | ` */` |
|     10 | 6355 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6356 |  |
|      - | 6357 | `	const char *zIni;` |
|      - | 6358 | `	int nByte;` |
|     11 | 6359 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6360 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6361 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6362 | `		return PH7_OK;` |
|      - | 6363 | `	}` |
|      - | 6364 | `	/* Extract the raw INI buffer */` |
|     11 | 6365 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6366 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 6367 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 6368 |  |
|      - | 6369 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6370 |  |
|      - | 6371 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6372 |  |
|      - | 6373 | `/*` |
|      - | 6374 | ` * Ctype Functions.` |
|      - | 6375 | ` * Status:` |
|      - | 6376 | ` *    Stable.` |
|      - | 6377 | ` */` |
|      - | 6378 | `/*` |
|      - | 6379 | ` * bool ctype_alnum(string $text)` |
|      - | 6380 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6381 | ` * Parameters` |
|      - | 6382 | ` *  $text` |
|      - | 6383 | ` *   The tested string.` |
|      - | 6384 | ` * Return` |
|      - | 6385 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6386 | ` */` |
|     16 | 6387 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6388 |  |
|      - | 6389 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6390 | `	int nLen;` |
|     17 | 6391 | `	if( nArg < 1 ){` |
|      - | 6392 | `		/* Missing arguments,return FALSE */` |
|      3 | 6393 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6394 | `		return PH7_OK;` |
|      - | 6395 | `	}` |
|      - | 6396 | `	/* Extract the target string */` |
|     15 | 6397 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6398 | `	zEnd = &zIn[nLen];` |
|     15 | 6399 | `	if( nLen < 1 ){` |
|      - | 6400 | `		/* Empty string,return FALSE */` |
|      3 | 6401 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6402 | `		return PH7_OK;` |
|      - | 6403 | `	}` |
|      - | 6404 | `	/* Perform the requested operation */` |
|     32 | 6405 | `	for(;;){` |
|     65 | 6406 | `		if( zIn >= zEnd ){` |
|      - | 6407 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6408 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6409 | `			return PH7_OK;` |
|      - | 6410 | `		}` |
|     57 | 6411 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6412 | `			break;` |
|      - | 6413 | `		}` |
|      - | 6414 | `		/* Point to the next character */` |
|     53 | 6415 | `		zIn++;` |
|      1 | 6416 | `	}` |
|      - | 6417 | `	/* The test failed,return FALSE */` |
|      5 | 6418 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6419 | `	return PH7_OK;` |
|      9 | 6420 |  |
|      - | 6421 | `/*` |
|      - | 6422 | ` * bool ctype_alpha(string $text)` |
|      - | 6423 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6424 | ` * Parameters` |
|      - | 6425 | ` *  $text` |
|      - | 6426 | ` *   The tested string.` |
|      - | 6427 | ` * Return` |
|      - | 6428 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6429 | ` */` |
|     18 | 6430 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6431 |  |
|      - | 6432 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6433 | `	int nLen;` |
|     19 | 6434 | `	if( nArg < 1 ){` |
|      - | 6435 | `		/* Missing arguments,return FALSE */` |
|      3 | 6436 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6437 | `		return PH7_OK;` |
|      - | 6438 | `	}` |
|      - | 6439 | `	/* Extract the target string */` |
|     17 | 6440 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6441 | `	zEnd = &zIn[nLen];` |
|     17 | 6442 | `	if( nLen < 1 ){` |
|      - | 6443 | `		/* Empty string,return FALSE */` |
|      3 | 6444 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6445 | `		return PH7_OK;` |
|      - | 6446 | `	}` |
|      - | 6447 | `	/* Perform the requested operation */` |
|     42 | 6448 | `	for(;;){` |
|     85 | 6449 | `		if( zIn >= zEnd ){` |
|      - | 6450 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6451 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6452 | `			return PH7_OK;` |
|      - | 6453 | `		}` |
|     77 | 6454 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6455 | `			break;` |
|      - | 6456 | `		}` |
|      - | 6457 | `		/* Point to the next character */` |
|     71 | 6458 | `		zIn++;` |
|      1 | 6459 | `	}` |
|      - | 6460 | `	/* The test failed,return FALSE */` |
|      7 | 6461 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6462 | `	return PH7_OK;` |
|     10 | 6463 |  |
|      - | 6464 | `/*` |
|      - | 6465 | ` * bool ctype_cntrl(string $text)` |
|      - | 6466 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6467 | ` * Parameters` |
|      - | 6468 | ` *  $text` |
|      - | 6469 | ` *   The tested string.` |
|      - | 6470 | ` * Return` |
|      - | 6471 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6472 | ` */` |
|     18 | 6473 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6474 |  |
|      - | 6475 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6476 | `	int nLen;` |
|     19 | 6477 | `	if( nArg < 1 ){` |
|      - | 6478 | `		/* Missing arguments,return FALSE */` |
|      3 | 6479 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6480 | `		return PH7_OK;` |
|      - | 6481 | `	}` |
|      - | 6482 | `	/* Extract the target string */` |
|     17 | 6483 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6484 | `	zEnd = &zIn[nLen];` |
|     17 | 6485 | `	if( nLen < 1 ){` |
|      - | 6486 | `		/* Empty string,return FALSE */` |
|      3 | 6487 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6488 | `		return PH7_OK;` |
|      - | 6489 | `	}` |
|      - | 6490 | `	/* Perform the requested operation */` |
|     14 | 6491 | `	for(;;){` |
|     29 | 6492 | `		if( zIn >= zEnd ){` |
|      - | 6493 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6494 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6495 | `			return PH7_OK;` |
|      - | 6496 | `		}` |
|     21 | 6497 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6498 | `			/* UTF-8 stream  */` |
|    ! 0 | 6499 | `			break;` |
|      - | 6500 | `		}` |
|     21 | 6501 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6502 | `			break;` |
|      - | 6503 | `		}` |
|      - | 6504 | `		/* Point to the next character */` |
|     15 | 6505 | `		zIn++;` |
|      1 | 6506 | `	}` |
|      - | 6507 | `	/* The test failed,return FALSE */` |
|      7 | 6508 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6509 | `	return PH7_OK;` |
|     10 | 6510 |  |
|      - | 6511 | `/*` |
|      - | 6512 | ` * bool ctype_digit(string $text)` |
|      - | 6513 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6514 | ` * Parameters` |
|      - | 6515 | ` *  $text` |
|      - | 6516 | ` *   The tested string.` |
|      - | 6517 | ` * Return` |
|      - | 6518 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6519 | ` */` |
|   1620 | 6520 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6521 |  |
|      - | 6522 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6523 | `	int nLen;` |
|   1625 | 6524 | `	if( nArg < 1 ){` |
|      - | 6525 | `		/* Missing arguments,return FALSE */` |
|      3 | 6526 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6527 | `		return PH7_OK;` |
|      - | 6528 | `	}` |
|      - | 6529 | `	/* Extract the target string */` |
|   1623 | 6530 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1623 | 6531 | `	zEnd = &zIn[nLen];` |
|   1623 | 6532 | `	if( nLen < 1 ){` |
|      - | 6533 | `		/* Empty string,return FALSE */` |
|      3 | 6534 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6535 | `		return PH7_OK;` |
|      - | 6536 | `	}` |
|      - | 6537 | `	/* Perform the requested operation */` |
|   1521 | 6538 | `	for(;;){` |
|   3047 | 6539 | `		if( zIn >= zEnd ){` |
|      - | 6540 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1381 | 6541 | `			ph7_result_bool(pCtx,1);` |
|   1381 | 6542 | `			return PH7_OK;` |
|      - | 6543 | `		}` |
|   1671 | 6544 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6545 | `			/* UTF-8 stream  */` |
|    ! 0 | 6546 | `			break;` |
|      - | 6547 | `		}` |
|   1671 | 6548 | `		if( !SyisDigit(zIn[0]) ){` |
|    245 | 6549 | `			break;` |
|      - | 6550 | `		}` |
|      - | 6551 | `		/* Point to the next character */` |
|   1431 | 6552 | `		zIn++;` |
|      5 | 6553 | `	}` |
|      - | 6554 | `	/* The test failed,return FALSE */` |
|    245 | 6555 | `	ph7_result_bool(pCtx,0);` |
|    245 | 6556 | `	return PH7_OK;` |
|    815 | 6557 |  |
|      - | 6558 | `/*` |
|      - | 6559 | ` * bool ctype_xdigit(string $text)` |
|      - | 6560 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6561 | ` * Parameters` |
|      - | 6562 | ` *  $text` |
|      - | 6563 | ` *   The tested string.` |
|      - | 6564 | ` * Return` |
|      - | 6565 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6566 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6567 | ` */` |
|     20 | 6568 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6569 |  |
|      - | 6570 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6571 | `	int nLen;` |
|     21 | 6572 | `	if( nArg < 1 ){` |
|      - | 6573 | `		/* Missing arguments,return FALSE */` |
|      3 | 6574 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6575 | `		return PH7_OK;` |
|      - | 6576 | `	}` |
|      - | 6577 | `	/* Extract the target string */` |
|     19 | 6578 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6579 | `	zEnd = &zIn[nLen];` |
|     19 | 6580 | `	if( nLen < 1 ){` |
|      - | 6581 | `		/* Empty string,return FALSE */` |
|      3 | 6582 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6583 | `		return PH7_OK;` |
|      - | 6584 | `	}` |
|      - | 6585 | `	/* Perform the requested operation */` |
|     46 | 6586 | `	for(;;){` |
|     93 | 6587 | `		if( zIn >= zEnd ){` |
|      - | 6588 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6589 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6590 | `			return PH7_OK;` |
|      - | 6591 | `		}` |
|     83 | 6592 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6593 | `			/* UTF-8 stream  */` |
|    ! 0 | 6594 | `			break;` |
|      - | 6595 | `		}` |
|     83 | 6596 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6597 | `			break;` |
|      - | 6598 | `		}` |
|      - | 6599 | `		/* Point to the next character */` |
|     77 | 6600 | `		zIn++;` |
|      1 | 6601 | `	}` |
|      - | 6602 | `	/* The test failed,return FALSE */` |
|      7 | 6603 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6604 | `	return PH7_OK;` |
|     11 | 6605 |  |
|      - | 6606 | `/*` |
|      - | 6607 | ` * bool ctype_graph(string $text)` |
|      - | 6608 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6609 | ` * Parameters` |
|      - | 6610 | ` *  $text` |
|      - | 6611 | ` *   The tested string.` |
|      - | 6612 | ` * Return` |
|      - | 6613 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6614 | ` * (no white space), FALSE otherwise.` |
|      - | 6615 | ` */` |
|     18 | 6616 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6617 |  |
|      - | 6618 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6619 | `	int nLen;` |
|     19 | 6620 | `	if( nArg < 1 ){` |
|      - | 6621 | `		/* Missing arguments,return FALSE */` |
|      3 | 6622 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6623 | `		return PH7_OK;` |
|      - | 6624 | `	}` |
|      - | 6625 | `	/* Extract the target string */` |
|     17 | 6626 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6627 | `	zEnd = &zIn[nLen];` |
|     17 | 6628 | `	if( nLen < 1 ){` |
|      - | 6629 | `		/* Empty string,return FALSE */` |
|      3 | 6630 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6631 | `		return PH7_OK;` |
|      - | 6632 | `	}` |
|      - | 6633 | `	/* Perform the requested operation */` |
|     57 | 6634 | `	for(;;){` |
|    115 | 6635 | `		if( zIn >= zEnd ){` |
|      - | 6636 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6637 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6638 | `			return PH7_OK;` |
|      - | 6639 | `		}` |
|    107 | 6640 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6641 | `			/* UTF-8 stream  */` |
|    ! 0 | 6642 | `			break;` |
|      - | 6643 | `		}` |
|    107 | 6644 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6645 | `			break;` |
|      - | 6646 | `		}` |
|      - | 6647 | `		/* Point to the next character */` |
|    101 | 6648 | `		zIn++;` |
|      1 | 6649 | `	}` |
|      - | 6650 | `	/* The test failed,return FALSE */` |
|      7 | 6651 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6652 | `	return PH7_OK;` |
|     10 | 6653 |  |
|      - | 6654 | `/*` |
|      - | 6655 | ` * bool ctype_print(string $text)` |
|      - | 6656 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6657 | ` * Parameters` |
|      - | 6658 | ` *  $text` |
|      - | 6659 | ` *   The tested string.` |
|      - | 6660 | ` * Return` |
|      - | 6661 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6662 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6663 | ` *  or control function at all.` |
|      - | 6664 | ` */` |
|     18 | 6665 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6666 |  |
|      - | 6667 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6668 | `	int nLen;` |
|     19 | 6669 | `	if( nArg < 1 ){` |
|      - | 6670 | `		/* Missing arguments,return FALSE */` |
|      3 | 6671 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6672 | `		return PH7_OK;` |
|      - | 6673 | `	}` |
|      - | 6674 | `	/* Extract the target string */` |
|     17 | 6675 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6676 | `	zEnd = &zIn[nLen];` |
|     17 | 6677 | `	if( nLen < 1 ){` |
|      - | 6678 | `		/* Empty string,return FALSE */` |
|      3 | 6679 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6680 | `		return PH7_OK;` |
|      - | 6681 | `	}` |
|      - | 6682 | `	/* Perform the requested operation */` |
|     63 | 6683 | `	for(;;){` |
|    127 | 6684 | `		if( zIn >= zEnd ){` |
|      - | 6685 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6686 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6687 | `			return PH7_OK;` |
|      - | 6688 | `		}` |
|    119 | 6689 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6690 | `			/* UTF-8 stream  */` |
|    ! 0 | 6691 | `			break;` |
|      - | 6692 | `		}` |
|    119 | 6693 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6694 | `			break;` |
|      - | 6695 | `		}` |
|      - | 6696 | `		/* Point to the next character */` |
|    113 | 6697 | `		zIn++;` |
|      1 | 6698 | `	}` |
|      - | 6699 | `	/* The test failed,return FALSE */` |
|      7 | 6700 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6701 | `	return PH7_OK;` |
|     10 | 6702 |  |
|      - | 6703 | `/*` |
|      - | 6704 | ` * bool ctype_punct(string $text)` |
|      - | 6705 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6706 | ` * Parameters` |
|      - | 6707 | ` *  $text` |
|      - | 6708 | ` *   The tested string.` |
|      - | 6709 | ` * Return` |
|      - | 6710 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6711 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6712 | ` */` |
|     20 | 6713 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6714 |  |
|      - | 6715 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6716 | `	int nLen;` |
|     21 | 6717 | `	if( nArg < 1 ){` |
|      - | 6718 | `		/* Missing arguments,return FALSE */` |
|      3 | 6719 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6720 | `		return PH7_OK;` |
|      - | 6721 | `	}` |
|      - | 6722 | `	/* Extract the target string */` |
|     19 | 6723 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6724 | `	zEnd = &zIn[nLen];` |
|     19 | 6725 | `	if( nLen < 1 ){` |
|      - | 6726 | `		/* Empty string,return FALSE */` |
|      3 | 6727 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6728 | `		return PH7_OK;` |
|      - | 6729 | `	}` |
|      - | 6730 | `	/* Perform the requested operation */` |
|     38 | 6731 | `	for(;;){` |
|     77 | 6732 | `		if( zIn >= zEnd ){` |
|      - | 6733 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6734 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6735 | `			return PH7_OK;` |
|      - | 6736 | `		}` |
|     69 | 6737 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6738 | `			/* UTF-8 stream  */` |
|    ! 0 | 6739 | `			break;` |
|      - | 6740 | `		}` |
|     69 | 6741 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6742 | `			break;` |
|      - | 6743 | `		}` |
|      - | 6744 | `		/* Point to the next character */` |
|     61 | 6745 | `		zIn++;` |
|      1 | 6746 | `	}` |
|      - | 6747 | `	/* The test failed,return FALSE */` |
|      9 | 6748 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6749 | `	return PH7_OK;` |
|     11 | 6750 |  |
|      - | 6751 | `/*` |
|      - | 6752 | ` * bool ctype_space(string $text)` |
|      - | 6753 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6754 | ` * Parameters` |
|      - | 6755 | ` *  $text` |
|      - | 6756 | ` *   The tested string.` |
|      - | 6757 | ` * Return` |
|      - | 6758 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6759 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6760 | ` *  and form feed characters.` |
|      - | 6761 | ` */` |
|  60161 | 6762 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6763 |  |
|      - | 6764 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6765 | `	int nLen;` |
|  60166 | 6766 | `	if( nArg < 1 ){` |
|      - | 6767 | `		/* Missing arguments,return FALSE */` |
|      3 | 6768 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6769 | `		return PH7_OK;` |
|      - | 6770 | `	}` |
|      - | 6771 | `	/* Extract the target string */` |
|  60164 | 6772 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  60164 | 6773 | `	zEnd = &zIn[nLen];` |
|  60164 | 6774 | `	if( nLen < 1 ){` |
|      - | 6775 | `		/* Empty string,return FALSE */` |
|      3 | 6776 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6777 | `		return PH7_OK;` |
|      - | 6778 | `	}` |
|      - | 6779 | `	/* Perform the requested operation */` |
|  31159 | 6780 | `	for(;;){` |
|  62238 | 6781 | `		if( zIn >= zEnd ){` |
|      - | 6782 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2057 | 6783 | `			ph7_result_bool(pCtx,1);` |
|   2057 | 6784 | `			return PH7_OK;` |
|      - | 6785 | `		}` |
|  60186 | 6786 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6787 | `			/* UTF-8 stream  */` |
|    ! 0 | 6788 | `			break;` |
|      - | 6789 | `		}` |
|  60186 | 6790 | `		if( !SyisSpace(zIn[0]) ){` |
|  58110 | 6791 | `			break;` |
|      - | 6792 | `		}` |
|      - | 6793 | `		/* Point to the next character */` |
|   2081 | 6794 | `		zIn++;` |
|      5 | 6795 | `	}` |
|      - | 6796 | `	/* The test failed,return FALSE */` |
|  58110 | 6797 | `	ph7_result_bool(pCtx,0);` |
|  58110 | 6798 | `	return PH7_OK;` |
|  30128 | 6799 |  |
|      - | 6800 | `/*` |
|      - | 6801 | ` * bool ctype_lower(string $text)` |
|      - | 6802 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 6803 | ` * Parameters` |
|      - | 6804 | ` *  $text` |
|      - | 6805 | ` *   The tested string.` |
|      - | 6806 | ` * Return` |
|      - | 6807 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 6808 | ` */` |
|     18 | 6809 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6810 |  |
|      - | 6811 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6812 | `	int nLen;` |
|     19 | 6813 | `	if( nArg < 1 ){` |
|      - | 6814 | `		/* Missing arguments,return FALSE */` |
|      3 | 6815 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6816 | `		return PH7_OK;` |
|      - | 6817 | `	}` |
|      - | 6818 | `	/* Extract the target string */` |
|     17 | 6819 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6820 | `	zEnd = &zIn[nLen];` |
|     17 | 6821 | `	if( nLen < 1 ){` |
|      - | 6822 | `		/* Empty string,return FALSE */` |
|      3 | 6823 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6824 | `		return PH7_OK;` |
|      - | 6825 | `	}` |
|      - | 6826 | `	/* Perform the requested operation */` |
|     27 | 6827 | `	for(;;){` |
|     55 | 6828 | `		if( zIn >= zEnd ){` |
|      - | 6829 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6830 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6831 | `			return PH7_OK;` |
|      - | 6832 | `		}` |
|     51 | 6833 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 6834 | `			break;` |
|      - | 6835 | `		}` |
|      - | 6836 | `		/* Point to the next character */` |
|     41 | 6837 | `		zIn++;` |
|      1 | 6838 | `	}` |
|      - | 6839 | `	/* The test failed,return FALSE */` |
|     11 | 6840 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6841 | `	return PH7_OK;` |
|     10 | 6842 |  |
|      - | 6843 | `/*` |
|      - | 6844 | ` * bool ctype_upper(string $text)` |
|      - | 6845 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 6846 | ` * Parameters` |
|      - | 6847 | ` *  $text` |
|      - | 6848 | ` *   The tested string.` |
|      - | 6849 | ` * Return` |
|      - | 6850 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 6851 | ` */` |
|     18 | 6852 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6853 |  |
|      - | 6854 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6855 | `	int nLen;` |
|     19 | 6856 | `	if( nArg < 1 ){` |
|      - | 6857 | `		/* Missing arguments,return FALSE */` |
|      3 | 6858 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6859 | `		return PH7_OK;` |
|      - | 6860 | `	}` |
|      - | 6861 | `	/* Extract the target string */` |
|     17 | 6862 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6863 | `	zEnd = &zIn[nLen];` |
|     17 | 6864 | `	if( nLen < 1 ){` |
|      - | 6865 | `		/* Empty string,return FALSE */` |
|      3 | 6866 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6867 | `		return PH7_OK;` |
|      - | 6868 | `	}` |
|      - | 6869 | `	/* Perform the requested operation */` |
|     28 | 6870 | `	for(;;){` |
|     57 | 6871 | `		if( zIn >= zEnd ){` |
|      - | 6872 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6873 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6874 | `			return PH7_OK;` |
|      - | 6875 | `		}` |
|     53 | 6876 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 6877 | `			break;` |
|      - | 6878 | `		}` |
|      - | 6879 | `		/* Point to the next character */` |
|     43 | 6880 | `		zIn++;` |
|      1 | 6881 | `	}` |
|      - | 6882 | `	/* The test failed,return FALSE */` |
|     11 | 6883 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6884 | `	return PH7_OK;` |
|     10 | 6885 |  |
|      - | 6886 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 6887 | `/*` |
|      - | 6888 | ` * Section:` |
|      - | 6889 | ` *    URL handling Functions.` |
|      - | 6890 | ` * Status:` |
|      - | 6891 | ` *    Stable.` |
|      - | 6892 | ` */` |
|      - | 6893 | `/*` |
|      - | 6894 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 6895 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 6896 | ` */` |
|   1026 | 6897 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 6898 |  |
|      - | 6899 | `	/* Store in the call context result buffer */` |
|   1028 | 6900 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 6901 | `	return SXRET_OK;` |
|      2 | 6902 |  |
|      - | 6903 | `/*` |
|      - | 6904 | ` * string base64_encode(string $data)` |
|      - | 6905 | ` * string convert_uuencode(string $data)` |
|      - | 6906 | ` *  Encodes data with MIME base64` |
|      - | 6907 | ` * Parameter` |
|      - | 6908 | ` *  $data` |
|      - | 6909 | ` *    Data to encode` |
|      - | 6910 | ` * Return` |
|      - | 6911 | ` *  Encoded data or FALSE on failure.` |
|      - | 6912 | ` */` |
|     10 | 6913 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6914 |  |
|      - | 6915 | `	const char *zIn;` |
|      - | 6916 | `	int nLen;` |
|     11 | 6917 | `	if( nArg < 1 ){` |
|      - | 6918 | `		/* Missing arguments,return FALSE */` |
|      5 | 6919 | `		ph7_result_bool(pCtx,0);` |
|      5 | 6920 | `		return PH7_OK;` |
|      - | 6921 | `	}` |
|      - | 6922 | `	/* Extract the input string */` |
|      7 | 6923 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6924 | `	if( nLen < 1 ){` |
|      - | 6925 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6927 | `		return PH7_OK;` |
|      - | 6928 | `	}` |
|      - | 6929 | `	/* Perform the BASE64 encoding */` |
|      7 | 6930 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 6931 | `	return PH7_OK;` |
|      6 | 6932 |  |
|      - | 6933 | `/*` |
|      - | 6934 | ` * string base64_decode(string $data)` |
|      - | 6935 | ` * string convert_uudecode(string $data)` |
|      - | 6936 | ` *  Decodes data encoded with MIME base64` |
|      - | 6937 | ` * Parameter` |
|      - | 6938 | ` *  $data` |
|      - | 6939 | ` *    Encoded data.` |
|      - | 6940 | ` * Return` |
|      - | 6941 | ` *  Returns the original data or FALSE on failure.` |
|      - | 6942 | ` */` |
|     36 | 6943 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6944 |  |
|      - | 6945 | `	const char *zIn;` |
|      - | 6946 | `	int nLen;` |
|     38 | 6947 | `	if( nArg < 1 ){` |
|      - | 6948 | `		/* Missing arguments,return FALSE */` |
|      3 | 6949 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6950 | `		return PH7_OK;` |
|      - | 6951 | `	}` |
|      - | 6952 | `	/* Extract the input string */` |
|     36 | 6953 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 6954 | `	if( nLen < 1 ){` |
|      - | 6955 | `		/* Nothing to process,return FALSE */` |
|      3 | 6956 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6957 | `		return PH7_OK;` |
|      - | 6958 | `	}` |
|      - | 6959 | `	/* Perform the BASE64 decoding */` |
|     34 | 6960 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 6961 | `	return PH7_OK;` |
|     20 | 6962 |  |
|      - | 6963 | `/*` |
|      - | 6964 | ` * string urlencode(string $str)` |
|      - | 6965 | ` *  URL encoding` |
|      - | 6966 | ` * Parameter` |
|      - | 6967 | ` *  $data` |
|      - | 6968 | ` *   Input string.` |
|      - | 6969 | ` * Return` |
|      - | 6970 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 6971 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 6972 | ` *  encoded as plus (+) signs.` |
|      - | 6973 | ` */` |
|      6 | 6974 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6975 |  |
|      - | 6976 | `	const char *zIn;` |
|      - | 6977 | `	int nLen;` |
|      7 | 6978 | `	if( nArg < 1 ){` |
|      - | 6979 | `		/* Missing arguments,return FALSE */` |
|      3 | 6980 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6981 | `		return PH7_OK;` |
|      - | 6982 | `	}` |
|      - | 6983 | `	/* Extract the input string */` |
|      5 | 6984 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 6985 | `	if( nLen < 1 ){` |
|      - | 6986 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6987 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6988 | `		return PH7_OK;` |
|      - | 6989 | `	}` |
|      - | 6990 | `	/* Perform the URL encoding */` |
|      5 | 6991 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 6992 | `	return PH7_OK;` |
|      4 | 6993 |  |
|      - | 6994 | `/*` |
|      - | 6995 | ` * string urldecode(string $str)` |
|      - | 6996 | ` *  Decodes any %## encoding in the given string.` |
|      - | 6997 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 6998 | ` * Parameter` |
|      - | 6999 | ` *  $data` |
|      - | 7000 | ` *    Input string.` |
|      - | 7001 | ` * Return` |
|      - | 7002 | ` *  Decoded URL or FALSE on failure.` |
|      - | 7003 | ` */` |
|      8 | 7004 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7005 |  |
|      - | 7006 | `	const char *zIn;` |
|      - | 7007 | `	int nLen;` |
|      9 | 7008 | `	if( nArg < 1 ){` |
|      - | 7009 | `		/* Missing arguments,return FALSE */` |
|      3 | 7010 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7011 | `		return PH7_OK;` |
|      - | 7012 | `	}` |
|      - | 7013 | `	/* Extract the input string */` |
|      7 | 7014 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7015 | `	if( nLen < 1 ){` |
|      - | 7016 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7017 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7018 | `		return PH7_OK;` |
|      - | 7019 | `	}` |
|      - | 7020 | `	/* Perform the URL decoding */` |
|      7 | 7021 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 7022 | `	return PH7_OK;` |
|      5 | 7023 |  |
|      - | 7024 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7025 | `/* Table of the built-in functions */` |
|      - | 7026 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 7027 | `	   /* Variable handling functions */` |
|      - | 7028 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 7029 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 7030 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 7031 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 7032 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 7033 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 7034 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 7035 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 7036 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 7037 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 7038 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 7039 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 7040 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 7041 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 7042 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 7043 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 7044 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 7045 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 7046 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 7047 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 7048 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7049 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 7050 | `	   /* Math functions */` |
|      - | 7051 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 7052 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 7053 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 7054 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 7055 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 7056 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 7057 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 7058 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 7059 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 7060 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 7061 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 7062 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 7063 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 7064 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 7065 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 7066 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 7067 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 7068 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 7069 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 7070 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 7071 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 7072 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 7073 | `	{ "round",    PH7_builtin_round        },` |
|      - | 7074 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 7075 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 7076 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 7077 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 7078 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 7079 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 7080 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 7081 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 7082 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 7083 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7084 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7085 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 7086 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7087 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7088 | `	   /* String handling functions */` |
|      - | 7089 |  |
|      - | 7090 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 7091 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 7092 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 7093 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 7094 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 7095 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 7096 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 7097 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 7098 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 7099 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 7100 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 7101 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 7102 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 7103 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 7104 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 7105 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 7106 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 7107 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 7108 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 7109 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 7110 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 7111 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 7112 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 7113 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 7114 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 7115 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 7116 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 7117 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 7118 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 7119 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 7120 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 7121 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 7122 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 7123 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 7124 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 7125 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 7126 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 7127 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 7128 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 7129 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 7130 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 7131 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 7132 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 7133 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 7134 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 7135 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 7136 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 7137 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 7138 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 7139 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 7140 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 7141 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 7142 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7143 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7144 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 7145 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 7146 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 7147 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 7148 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7149 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7150 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 7151 |  |
|      - | 7152 |  |
|      - | 7153 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 7154 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 7155 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 7156 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 7157 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 7158 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 7159 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 7160 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 7161 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7162 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7163 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7164 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 7165 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 7166 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7167 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7168 |  |
|      - | 7169 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 7170 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 7171 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 7172 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 7173 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 7174 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 7175 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 7176 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 7177 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 7178 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 7179 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 7180 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 7181 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7182 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7183 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 7184 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7185 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7186 |  |
|      - | 7187 | `	         /* Ctype functions */` |
|      - | 7188 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 7189 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 7190 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 7191 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 7192 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 7193 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 7194 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 7195 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 7196 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 7197 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 7198 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 7199 | `	         /* Time functions */` |
|      - | 7200 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 7201 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 7202 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 7203 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 7204 | `	{ "date",        PH7_builtin_date         },` |
|      - | 7205 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 7206 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 7207 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 7208 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 7209 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 7210 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 7211 | `	        /* URL functions */` |
|      - | 7212 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 7213 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 7214 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 7215 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 7216 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 7217 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 7218 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 7219 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 7220 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7221 | `};` |
|      - | 7222 | `/*` |
|      - | 7223 | ` * Register the built-in functions defined above,the array functions` |
|      - | 7224 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 7225 | ` */` |
|   2956 | 7226 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 7227 |  |
|      - | 7228 | `	sxu32 n;` |
| 478877 | 7229 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 475921 | 7230 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 237963 | 7231 | `	}` |
|      - | 7232 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   2961 | 7233 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 7234 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   2961 | 7235 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   2961 | 7236 |  |
|      - | 7237 |  |
