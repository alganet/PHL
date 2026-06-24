# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3629/4078 lines (88.99%)

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
|    628 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   63 |  |
|    630 |   64 | `	int res = 0; /* Assume false by default */` |
|    630 |   65 | `	if( nArg > 0 ){` |
|      - |   66 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   67 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   68 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    628 |   69 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    313 |   70 | `	}` |
|      - |   71 | `	/* Query result */` |
|    630 |   72 | `	ph7_result_bool(pCtx,res);` |
|    630 |   73 | `	return PH7_OK;` |
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
|  26508 |  295 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  296 |  |
|  26513 |  297 | `	int res = 1; /* Assume empty by default */` |
|  26513 |  298 | `	if( nArg > 0 ){` |
|  26511 |  299 | `		res = ph7_value_is_empty(apArg[0]);` |
|  13253 |  300 | `	}` |
|  26513 |  301 | `	ph7_result_bool(pCtx,res);` |
|  26513 |  302 | `	return PH7_OK;` |
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
| 197616 |  345 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  346 |  |
|      - |  347 | `	const char *zSource,*zOfft;` |
|      - |  348 | `	int nOfft,nLen,nSrcLen;` |
| 197621 |  349 | `	if( nArg < 2 ){` |
|      - |  350 | `		/* return FALSE */` |
|      5 |  351 | `		ph7_result_bool(pCtx,0);` |
|      5 |  352 | `		return PH7_OK;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Extract the target string */` |
| 197617 |  355 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 197617 |  356 | `	if( nSrcLen < 1 ){` |
|      - |  357 | `		/* Empty string,return FALSE */` |
|  11347 |  358 | `		ph7_result_bool(pCtx,0);` |
|  11347 |  359 | `		return PH7_OK;` |
|      - |  360 | `	}` |
| 186275 |  361 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  362 | `	/* Extract the offset */` |
| 186275 |  363 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 186275 |  364 | `	if( nOfft < 0 ){` |
|  30775 |  365 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  30775 |  366 | `		if( zOfft < zSource ){` |
|      - |  367 | `			/* Invalid offset */` |
|      5 |  368 | `			ph7_result_bool(pCtx,0);` |
|      5 |  369 | `			return PH7_OK;` |
|      - |  370 | `		}` |
|  30771 |  371 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  30771 |  372 | `		nOfft = (int)(zOfft-zSource);` |
| 170888 |  373 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  374 | `		/* Invalid offset */` |
|    169 |  375 | `		ph7_result_bool(pCtx,0);` |
|    169 |  376 | `		return PH7_OK;` |
|    ! 0 |  377 | `	}else{` |
| 155341 |  378 | `		zOfft = &zSource[nOfft];` |
| 155341 |  379 | `		nLen = nSrcLen - nOfft;` |
|      - |  380 | `	}` |
| 186107 |  381 | `	if( nArg > 2 ){` |
|      - |  382 | `		/* Extract the length */` |
| 153833 |  383 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 153833 |  384 | `		if( nLen == 0 ){` |
|      - |  385 | `			/* Invalid length,return an empty string */` |
|      5 |  386 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  387 | `			return PH7_OK;` |
| 153829 |  388 | `		}else if( nLen < 0 ){` |
|  30763 |  389 | `			nLen = nSrcLen + nLen - nOfft;` |
|  30763 |  390 | `			if( nLen < 1 ){` |
|      - |  391 | `				/* Invalid  length */` |
|      3 |  392 | `				nLen = nSrcLen - nOfft;` |
|      1 |  393 | `			}` |
|  15379 |  394 | `		}` |
| 153829 |  395 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  396 | `			/* Invalid length */` |
|   4545 |  397 | `			nLen = nSrcLen - nOfft;` |
|   2270 |  398 | `		}` |
|  76912 |  399 | `	}` |
|      - |  400 | `	/* Return the substring */` |
| 186103 |  401 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 186103 |  402 | `	return PH7_OK;` |
|  98813 |  403 |  |
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
|     29 |  674 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
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
|     43 |  789 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
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
|     40 |  807 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
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
|   6046 | 1372 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1373 |  |
|   6051 | 1374 | `	int iLen = 0;` |
|   6051 | 1375 | `	if( nArg > 0 ){` |
|   6049 | 1376 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   3022 | 1377 | `	}` |
|      - | 1378 | `	/* String length */` |
|   6051 | 1379 | `	ph7_result_int(pCtx,iLen);` |
|   6051 | 1380 | `	return PH7_OK;` |
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
| 124480 | 1525 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1526 |  |
|  62240 | 1527 | `	SXUNUSED(pKey);` |
| 124485 | 1528 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1529 | `	const char *zData;` |
|      - | 1530 | `	int nLen;` |
| 124485 | 1531 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
| 124483 | 1548 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1549 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 124483 | 1550 | `	if( pData->bFirst ){` |
|  31073 | 1551 | `		pData->bFirst = 0;` |
| 108949 | 1552 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1553 | `		/* append the separator first */` |
|  93403 | 1554 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  46699 | 1555 | `	}` |
|      - | 1556 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 124483 | 1557 | `	if( nLen > 0 ){` |
| 113141 | 1558 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  56568 | 1559 | `	}` |
| 124483 | 1560 | `	return PH7_OK;` |
|  62245 | 1561 |  |
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
|  31094 | 1575 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1576 |  |
|      - | 1577 | `	struct implode_data imp_data;` |
|  31099 | 1578 | `	int i = 1;` |
|  31099 | 1579 | `	if( nArg < 1 ){` |
|      - | 1580 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1581 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1582 | `		return PH7_OK;` |
|      - | 1583 | `	}` |
|      - | 1584 | `	/* Prepare the implode context */` |
|  31099 | 1585 | `	imp_data.pCtx = pCtx;` |
|  31099 | 1586 | `	imp_data.bRecursive = 0;` |
|  31099 | 1587 | `	imp_data.bFirst = 1;` |
|  31099 | 1588 | `	imp_data.nRecCount = 0;` |
|  31099 | 1589 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  31097 | 1590 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  15551 | 1591 | `	}else{` |
|      3 | 1592 | `		imp_data.zSep = 0;` |
|      3 | 1593 | `		imp_data.nSeplen = 0;` |
|      3 | 1594 | `		i = 0;` |
|      - | 1595 | `	}` |
|  31099 | 1596 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1597 | `	/* Start the 'join' process */` |
|  62193 | 1598 | `	while( i < nArg ){` |
|  31099 | 1599 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1600 | `			/* Iterate throw array entries */` |
|  31099 | 1601 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  15552 | 1602 | `		}else{` |
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
|  31099 | 1618 | `		i++;` |
|      5 | 1619 | `	}` |
|  31099 | 1620 | `	return PH7_OK;` |
|  15552 | 1621 |  |
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
|   5864 | 1710 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1711 |  |
|      - | 1712 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1713 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1714 | `	ph7_value *pArray;` |
|      - | 1715 | `	ph7_value *pValue;` |
|      - | 1716 | `	sxu32 nOfft;` |
|      - | 1717 | `	sxi32 rc;` |
|   5869 | 1718 | `	if( nArg < 2 ){` |
|      - | 1719 | `		/* Missing arguments,return FALSE */` |
|      9 | 1720 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1721 | `		return PH7_OK;` |
|      - | 1722 | `	}` |
|      - | 1723 | `	/* Extract the delimiter */` |
|   5861 | 1724 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   5861 | 1725 | `	if( nDelim < 1 ){` |
|      - | 1726 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1727 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1728 | `		return PH7_OK;` |
|      - | 1729 | `	}` |
|      - | 1730 | `	/* Extract the string */` |
|   5859 | 1731 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   5859 | 1732 | `	if( nStrlen < 1 ){` |
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
|   5857 | 1747 | `	zEnd = &zString[nStrlen];` |
|      - | 1748 | `	/* Create the array */` |
|   5857 | 1749 | `	pArray =  ph7_context_new_array(pCtx);` |
|   5857 | 1750 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   5857 | 1751 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1752 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1754 | `		return PH7_OK;` |
|      - | 1755 | `	}` |
|      - | 1756 | `	/* Set a defualt limit */` |
|   5857 | 1757 | `	iLimit = SXI32_HIGH;` |
|   5857 | 1758 | `	if( nArg > 2 ){` |
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
|  67227 | 1769 | `	for(;;){` |
| 134459 | 1770 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 134459 | 1771 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1772 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   5857 | 1773 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   5857 | 1774 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   5857 | 1775 | `			break;` |
|      - | 1776 | `		}` |
|      - | 1777 | `		/* Point to the desired offset */` |
| 128607 | 1778 | `		zCur = &zString[nOfft];` |
|      - | 1779 | `		/* Perform the store operation (may be empty) */` |
| 128607 | 1780 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 128607 | 1781 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 1782 | `		/* Point beyond the delimiter */` |
| 128607 | 1783 | `		zString = &zCur[nDelim];` |
|      - | 1784 | `		/* Reset the cursor */` |
| 128607 | 1785 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1786 | `	}` |
|      - | 1787 | `	/* Return the freshly created array */` |
|   5857 | 1788 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1789 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1790 | `	 * released as soon we return from this foregin function.` |
|      - | 1791 | `	 */` |
|   5857 | 1792 | `	return PH7_OK;` |
|   2937 | 1793 |  |
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
|  13400 | 1809 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1810 |  |
|      - | 1811 | `	const char *zString;` |
|      - | 1812 | `	int nLen;` |
|  13405 | 1813 | `	if( nArg < 1 ){` |
|      - | 1814 | `		/* Missing arguments,return null */` |
|      3 | 1815 | `		ph7_result_null(pCtx);` |
|      3 | 1816 | `		return PH7_OK;` |
|      - | 1817 | `	}` |
|      - | 1818 | `	/* Extract the target string */` |
|  13403 | 1819 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  13403 | 1820 | `	if( nLen < 1 ){` |
|      - | 1821 | `		/* Empty string,return */` |
|   1691 | 1822 | `		ph7_result_string(pCtx,"",0);` |
|   1691 | 1823 | `		return PH7_OK;` |
|      - | 1824 | `	}` |
|      - | 1825 | `	/* Start the trim process */` |
|  11717 | 1826 | `	if( nArg < 2 ){` |
|      - | 1827 | `		SyString sStr;` |
|      - | 1828 | `		/* Remove white spaces and NUL bytes */` |
|  11713 | 1829 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  28635 | 1830 | `		SyStringFullTrimSafe(&sStr);` |
|  11713 | 1831 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   5859 | 1832 | `	}else{` |
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
|  11717 | 1886 | `	return PH7_OK;` |
|   6705 | 1887 |  |
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
|  30760 | 2051 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2052 |  |
|      - | 2053 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2054 | `	int nLen;` |
|  30765 | 2055 | `	if( nArg < 1 ){` |
|      - | 2056 | `		/* Missing arguments,return null */` |
|      3 | 2057 | `		ph7_result_null(pCtx);` |
|      3 | 2058 | `		return PH7_OK;` |
|      - | 2059 | `	}` |
|      - | 2060 | `	/* Extract the target string */` |
|  30763 | 2061 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  30763 | 2062 | `	if( nLen < 1 ){` |
|      - | 2063 | `		/* Empty string,return */` |
|      3 | 2064 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2065 | `		return PH7_OK;` |
|      - | 2066 | `	}` |
|      - | 2067 | `	/* Perform the requested operation */` |
|  30761 | 2068 | `	zEnd = &zString[nLen];` |
|  96921 | 2069 | `	for(;;){` |
| 193847 | 2070 | `		if( zString >= zEnd ){` |
|      - | 2071 | `			/* No more input,break immediately */` |
|  30761 | 2072 | `			break;` |
|      - | 2073 | `		}` |
| 163091 | 2074 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2075 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2076 | `			zCur = zString;` |
|    ! 0 | 2077 | `			zString++;` |
|    ! 0 | 2078 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2079 | `				zString++;` |
|    ! 0 | 2080 | `			}` |
|      - | 2081 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2082 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2083 | `		}else{` |
| 163091 | 2084 | `			int c = zString[0];` |
| 163091 | 2085 | `			if( SyisUpper(c) ){` |
| 163089 | 2086 | `				c = SyToLower(zString[0]);` |
|  81542 | 2087 | `			}` |
|      - | 2088 | `			/* Append character */` |
| 163091 | 2089 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2090 | `			/* Advance the cursor */` |
| 163091 | 2091 | `			zString++;` |
|      - | 2092 | `		}` |
|      5 | 2093 | `	}` |
|  30761 | 2094 | `	return PH7_OK;` |
|  15385 | 2095 |  |
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
|     48 | 2297 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2298 |  |
|      - | 2299 | `	int c;` |
|      - | 2300 | `	unsigned char ch;` |
|      - | 2301 | `	/* PHP requires exactly one argument. */` |
|     51 | 2302 | `	if( nArg != 1 ){` |
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
|     45 | 2313 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2314 | `		char zBuf[120];` |
|      4 | 2315 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2316 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2317 | `			ph7_value_to_double(apArg[0])` |
|      - | 2318 | `			);` |
|      3 | 2319 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2320 | `	}` |
|      - | 2321 | `	/* Extract the codepoint. */` |
|     45 | 2322 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2323 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2324 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2325 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2326 | `	 * name to avoid the API double-prefixing it. */` |
|     45 | 2327 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2328 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2329 | `			E_DEPRECATED,` |
|      - | 2330 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2331 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2332 | `			"The value used will be constrained using % 256"` |
|      - | 2333 | `			);` |
|      2 | 2334 | `	}` |
|      - | 2335 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2336 | `	 * when taking the address of a wider int. */` |
|     45 | 2337 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2338 | `	/* Return the specified character */` |
|     45 | 2339 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     45 | 2340 | `	return PH7_OK;` |
|     27 | 2341 |  |
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
|    242 | 2431 | `	for(;;){` |
|    485 | 2432 | `		if( zIn >= zEnd ){` |
|     47 | 2433 | `			break;` |
|      - | 2434 | `		}` |
|    439 | 2435 | `		c = SyToLower(zIn[0]);` |
|    439 | 2436 | `		d = SyToLower(zpIn[0]);` |
|    439 | 2437 | `		if( c == d ){` |
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
|    399 | 2457 | `		zIn++;` |
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
|  20226 | 3230 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3231 |  |
|      - | 3232 | `	const char *zIn;` |
|      - | 3233 | `	int nLen,nMul;` |
|      - | 3234 | `	int rc;` |
|  20227 | 3235 | `	if( nArg < 2 ){` |
|      - | 3236 | `		/* Missing arguments,return NULL */` |
|      3 | 3237 | `		ph7_result_null(pCtx);` |
|      3 | 3238 | `		return PH7_OK;` |
|      - | 3239 | `	}` |
|      - | 3240 | `	/* Extract the target string */` |
|  20225 | 3241 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20225 | 3242 | `	if( nLen < 1 ){` |
|      - | 3243 | `		/* Empty string.Return null */` |
|    ! 0 | 3244 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3245 | `		return PH7_OK;` |
|      - | 3246 | `	}` |
|      - | 3247 | `	/* Extract the multiplier */` |
|  20225 | 3248 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20225 | 3249 | `	if( nMul < 1 ){` |
|      - | 3250 | `		/* Return the empty string */` |
|      3 | 3251 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3252 | `		return PH7_OK;` |
|      - | 3253 | `	}` |
|      - | 3254 | `	/* Perform the requested operation */` |
| 120878 | 3255 | `	for(;;){` |
| 241757 | 3256 | `		if( !nMul ){` |
|  20223 | 3257 | `			break;` |
|      - | 3258 | `		}` |
|      - | 3259 | `		/* Append the copy */` |
| 221535 | 3260 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 221535 | 3261 | `		if( rc != PH7_OK ){` |
|      - | 3262 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3263 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3264 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3265 | `		}` |
| 221535 | 3266 | `		nMul--;` |
|      1 | 3267 | `	}` |
|  20223 | 3268 | `	return PH7_OK;` |
|  10114 | 3269 |  |
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
|      - | 4490 | `/*` |
|      - | 4491 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4492 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4493 | ` */` |
|      - | 4494 | `/*` |
|      - | 4495 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4496 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4497 | ` */` |
|     40 | 4498 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4499 |  |
|      - | 4500 | `	int iCost;` |
|     40 | 4501 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4502 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4503 | `		return FALSE;` |
|      - | 4504 | `	}` |
|     29 | 4505 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4506 | `		return FALSE;` |
|      - | 4507 | `	}` |
|     29 | 4508 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4509 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4510 | `		return FALSE;` |
|      - | 4511 | `	}` |
|     27 | 4512 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4513 | `	return TRUE;` |
|     21 | 4514 |  |
|      - | 4515 | `/*` |
|      - | 4516 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4517 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4518 | ` */` |
|     20 | 4519 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4520 |  |
|     23 | 4521 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4522 | `		return TRUE;` |
|      - | 4523 | `	}` |
|     23 | 4524 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4525 | `		int nAlgo;` |
|     23 | 4526 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4527 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4528 | `	}` |
|    ! 0 | 4529 | `	return FALSE;` |
|     13 | 4530 |  |
|      - | 4531 | `/*` |
|      - | 4532 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4533 | ` *  Create a bcrypt hash of the password.` |
|      - | 4534 | ` */` |
|     16 | 4535 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4536 |  |
|      - | 4537 | `	const char *zPwd;` |
|     19 | 4538 | `	int nPwd,iCost = 12;` |
|      - | 4539 | `	unsigned char aSalt[16];` |
|      - | 4540 | `	char zHash[60];` |
|     19 | 4541 | `	if( nArg < 2 ){` |
|    ! 0 | 4542 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4543 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4544 | `	}` |
|     19 | 4545 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4546 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4547 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4548 | `	}` |
|      - | 4549 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4550 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4551 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4552 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4553 | `	}` |
|     16 | 4554 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4555 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4556 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4557 | `	}` |
|     13 | 4558 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4559 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4560 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4561 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4562 | `	}` |
|     13 | 4563 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4564 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4565 | `		return PH7_OK;` |
|      - | 4566 | `	}` |
|     13 | 4567 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4568 | `	return PH7_OK;` |
|     11 | 4569 |  |
|      - | 4570 | `/*` |
|      - | 4571 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4572 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4573 | ` */` |
|     28 | 4574 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4575 |  |
|      - | 4576 | `	const char *zPwd,*zHash;` |
|      - | 4577 | `	int nPwd,nHash,iCost,i;` |
|      - | 4578 | `	unsigned char aSalt[16];` |
|      - | 4579 | `	char zComputed[60];` |
|     29 | 4580 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4581 | `	if( nArg < 2 ){` |
|    ! 0 | 4582 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4583 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4584 | `	}` |
|     29 | 4585 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4586 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4587 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4588 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4589 | `		return PH7_OK;` |
|      - | 4590 | `	}` |
|      - | 4591 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4592 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4593 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4594 | `		return PH7_OK;` |
|      - | 4595 | `	}` |
|     19 | 4596 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4597 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4598 | `		return PH7_OK;` |
|      - | 4599 | `	}` |
|      - | 4600 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4601 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4602 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4603 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4604 | `	}` |
|     19 | 4605 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4606 | `	return PH7_OK;` |
|     15 | 4607 |  |
|      - | 4608 | `/*` |
|      - | 4609 | ` * array password_get_info(string $hash)` |
|      - | 4610 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4611 | ` */` |
|      6 | 4612 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4613 |  |
|      7 | 4614 | `	const char *zHash = "";` |
|      7 | 4615 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4616 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4617 | `	if( nArg > 0 ){` |
|      7 | 4618 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4619 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4620 | `	}` |
|      7 | 4621 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4622 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4623 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4624 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4625 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4626 | `		return PH7_OK;` |
|      - | 4627 | `	}` |
|      7 | 4628 | `	if( bBcrypt ){` |
|      5 | 4629 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4630 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4631 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4632 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4633 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4634 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4635 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4636 | `	}else{` |
|      3 | 4637 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4638 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4639 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4640 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4641 | `	}` |
|      7 | 4642 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4643 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4644 | `	return PH7_OK;` |
|      4 | 4645 |  |
|      - | 4646 | `/*` |
|      - | 4647 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4648 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4649 | ` */` |
|      6 | 4650 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4651 |  |
|      - | 4652 | `	const char *zHash;` |
|      7 | 4653 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4654 | `	if( nArg < 2 ){` |
|    ! 0 | 4655 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4656 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4657 | `	}` |
|      7 | 4658 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4659 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4660 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4661 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4662 | `		return PH7_OK;` |
|      - | 4663 | `	}` |
|      5 | 4664 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4665 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4666 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4667 | `	}` |
|      5 | 4668 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4669 | `	return PH7_OK;` |
|      4 | 4670 |  |
|      - | 4671 | `/*` |
|      - | 4672 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4673 | ` *` |
|      - | 4674 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4675 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4676 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4677 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4678 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4679 | ` */` |
|      - | 4680 | `#define FV_VALIDATE_INT     257` |
|      - | 4681 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4682 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4683 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4684 | `#define FV_VALIDATE_URL     273` |
|      - | 4685 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4686 | `#define FV_VALIDATE_IP      275` |
|      - | 4687 | `#define FV_VALIDATE_MAC     276` |
|      - | 4688 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4689 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4690 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4691 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4692 | `#define FV_SANITIZE_URL     518` |
|      - | 4693 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4694 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4695 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4696 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4697 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4698 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4699 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4700 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4701 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4702 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4703 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4704 |  |
|      - | 4705 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4706 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    125 | 4707 | `static void FvTrim(const char **pz,int *pn){` |
|    125 | 4708 | `	const char *z = *pz;` |
|    125 | 4709 | `	int n = *pn;` |
|    129 | 4710 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    133 | 4711 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    125 | 4712 | `	*pz = z; *pn = n;` |
|    125 | 4713 |  |
|      - | 4714 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4715 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4716 | `	int neg = 0, i;` |
|     57 | 4717 | `	sxu64 u = 0;` |
|     57 | 4718 | `	FvTrim(&z,&n);` |
|     57 | 4719 | `	if( n==0 ){ return 0; }` |
|     51 | 4720 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4721 | `	if( n==0 ){ return 0; }` |
|     49 | 4722 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4723 | `		z += 2; n -= 2;` |
|      3 | 4724 | `		if( n==0 ){ return 0; }` |
|      7 | 4725 | `		for( i=0; i<n; i++ ){` |
|      5 | 4726 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4727 | `			if( h<0 ){ return 0; }` |
|      5 | 4728 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4729 | `			u = u*16 + (sxu64)h;` |
|      3 | 4730 | `		}` |
|     48 | 4731 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4732 | `		for( i=0; i<n; i++ ){` |
|      7 | 4733 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4734 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4735 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4736 | `		}` |
|      2 | 4737 | `	}else{` |
|     45 | 4738 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4739 | `		for( i=0; i<n; i++ ){` |
|    173 | 4740 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4741 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4742 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4743 | `		}` |
|      - | 4744 | `	}` |
|     33 | 4745 | `	if( neg ){` |
|      5 | 4746 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4747 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4748 | `	}else{` |
|     29 | 4749 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4750 | `		*pOut = (ph7_int64)u;` |
|      - | 4751 | `	}` |
|     31 | 4752 | `	return 1;` |
|     29 | 4753 |  |
|      - | 4754 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     41 | 4755 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4756 | `	char zBuf[512];` |
|     41 | 4757 | `	int i, m = 0, seenDigit = 0;` |
|     41 | 4758 | `	const char *zv; int nv; double d = 0; const char *zRest = 0;` |
|     41 | 4759 | `	FvTrim(&z,&n);` |
|      - | 4760 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4761 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     41 | 4762 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     41 | 4763 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4764 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4765 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4766 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4767 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     23 | 4768 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     23 | 4769 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     23 | 4770 | `		intEnd = s;` |
|    155 | 4771 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    133 | 4772 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    133 | 4773 | `			intEnd++;` |
|      1 | 4774 | `		}` |
|     23 | 4775 | `		if( hasComma ){` |
|     23 | 4776 | `			segStart = s; segIdx = 0;` |
|    151 | 4777 | `			for( i=s; i<=intEnd; i++ ){` |
|    139 | 4778 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     45 | 4779 | `					int segLen = i - segStart, k;` |
|     45 | 4780 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     23 | 4781 | `					else if( segLen!=3 ){ return 0; }` |
|    107 | 4782 | `					for( k=segStart; k<i; k++ ){` |
|     73 | 4783 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     73 | 4784 | `						zBuf[m++] = z[k];` |
|     37 | 4785 | `					}` |
|     35 | 4786 | `					segStart = i+1; segIdx++;` |
|     17 | 4787 | `				}` |
|     65 | 4788 | `			}` |
|      7 | 4789 | `		}else{` |
|    ! 0 | 4790 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4791 | `		}` |
|     17 | 4792 | `		for( i=intEnd; i<n; i++ ){` |
|      5 | 4793 | `			if( z[i]==',' ){ return 0; }` |
|      5 | 4794 | `			zBuf[m++] = z[i];` |
|      3 | 4795 | `		}` |
|     13 | 4796 | `		zv = zBuf; nv = m;` |
|      7 | 4797 | `	}else{` |
|     19 | 4798 | `		zv = z; nv = n;` |
|      - | 4799 | `	}` |
|     31 | 4800 | `	i = 0;` |
|     31 | 4801 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    105 | 4802 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     31 | 4803 | `	if( i<nv && zv[i]=='.' ){` |
|     13 | 4804 | `		i++;` |
|     23 | 4805 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|      6 | 4806 | `	}` |
|     31 | 4807 | `	if( !seenDigit ){ return 0; }` |
|     29 | 4808 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|      5 | 4809 | `		i++;` |
|      5 | 4810 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|      5 | 4811 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|      9 | 4812 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|      2 | 4813 | `	}` |
|     29 | 4814 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4815 | `	/* Divergence: PHP rejects magnitudes beyond the double range ("1e400" ->` |
|      - | 4816 | `	 * false), but SyStrToReal (the engine-wide float parser, also behind` |
|      - | 4817 | `	 * floatval/(float)) saturates them to a finite value, so they validate here. */` |
|     25 | 4818 | `	SyStrToReal(zv,(sxu32)nv,(void *)&d,&zRest);` |
|     25 | 4819 | `	*pOut = d;` |
|     25 | 4820 | `	return 1;` |
|     21 | 4821 |  |
|      - | 4822 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4823 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4824 | ` * false, NOT failures. */` |
|     33 | 4825 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4826 | `	FvTrim(&z,&n);` |
|     32 | 4827 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4828 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4829 | `		*pBool = 1; return 1;` |
|      - | 4830 | `	}` |
|     22 | 4831 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4832 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4833 | `		*pBool = 0; return 1;` |
|      - | 4834 | `	}` |
|      9 | 4835 | `	return 0;` |
|     15 | 4836 |  |
|      - | 4837 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4838 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4839 | `	int i = 0, parts = 0;` |
|     77 | 4840 | `	while( i<n ){` |
|     65 | 4841 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4842 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4843 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4844 | `			if( val>255 ){ return 0; }` |
|     79 | 4845 | `			digits++; i++;` |
|      1 | 4846 | `		}` |
|     59 | 4847 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4848 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4849 | `		parts++;` |
|     45 | 4850 | `		if( parts>4 ){ return 0; }` |
|     45 | 4851 | `		if( i<n ){` |
|     33 | 4852 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4853 | `			i++;` |
|     33 | 4854 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4855 | `		}` |
|      1 | 4856 | `	}` |
|     13 | 4857 | `	return parts==4;` |
|     17 | 4858 |  |
|      - | 4859 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4860 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4861 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4862 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4863 | `	if( n==0 ){ return 0; }` |
|    145 | 4864 | `	while( i<=n ){` |
|    133 | 4865 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4866 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4867 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4868 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4869 | `			if( isV4 ){` |
|     11 | 4870 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4871 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4872 | `				groups += 2;` |
|      3 | 4873 | `			}else{` |
|     13 | 4874 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4875 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4876 | `				groups++;` |
|      - | 4877 | `			}` |
|     17 | 4878 | `			segStart = i+1;` |
|      8 | 4879 | `		}` |
|    127 | 4880 | `		i++;` |
|      1 | 4881 | `	}` |
|     13 | 4882 | `	return groups;` |
|     10 | 4883 |  |
|      - | 4884 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4885 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4886 | `	const char *zDbl = 0;` |
|      - | 4887 | `	int i, ga, gb;` |
|    139 | 4888 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4889 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4890 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4891 | `			zDbl = z+i;` |
|      5 | 4892 | `		}` |
|     61 | 4893 | `	}` |
|     17 | 4894 | `	if( zDbl==0 ){` |
|      9 | 4895 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4896 | `	}else{` |
|      9 | 4897 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4898 | `		int lenB = n - lenA - 2;` |
|      9 | 4899 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4900 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4901 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4902 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4903 | `	}` |
|     10 | 4904 |  |
|     25 | 4905 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4906 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4907 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4908 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4909 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4910 | `	return 0;` |
|     13 | 4911 |  |
|      - | 4912 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 4913 | `static int FvValidateMac(const char *z,int n){` |
|      - | 4914 | `	char sep;` |
|      - | 4915 | `	int i;` |
|     11 | 4916 | `	if( n!=17 ){ return 0; }` |
|      7 | 4917 | `	sep = z[2];` |
|      7 | 4918 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 4919 | `	for( i=0; i<17; i++ ){` |
|    101 | 4920 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 4921 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 4922 | `	}` |
|      5 | 4923 | `	return 1;` |
|      6 | 4924 |  |
|      - | 4925 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 4926 | ` * parts or IP-literal domains). */` |
|     21 | 4927 | `static int FvValidateEmail(const char *z,int n){` |
|     21 | 4928 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 4929 | `	const char *zDom;` |
|     21 | 4930 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 4931 | `	for( i=0; i<n; i++ ){` |
|    181 | 4932 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 4933 | `	}` |
|     21 | 4934 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 4935 | `	localLen = at;` |
|     21 | 4936 | `	zDom = z + at + 1;` |
|     21 | 4937 | `	domLen = n - at - 1;` |
|     21 | 4938 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 4939 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 4940 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 4941 | `		if( c<=' ' ){ return 0; }` |
|     41 | 4942 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 4943 | `	}` |
|     15 | 4944 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 4945 | `	labelStart = 0;` |
|     85 | 4946 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 4947 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 4948 | `			int ll = i - labelStart;` |
|     25 | 4949 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 4950 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 4951 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 4952 | `			labelStart = i+1;` |
|     12 | 4953 | `		}else{` |
|     51 | 4954 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 4955 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 4956 | `		}` |
|     37 | 4957 | `	}` |
|     11 | 4958 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 4959 | `	return 1;` |
|     11 | 4960 |  |
|      - | 4961 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 4962 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 4963 | `	int i;` |
|     11 | 4964 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 4965 | `	for( i=0; i<n; i++ ){` |
|     75 | 4966 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 4967 | `		if( c<=' ' ){ return 0; }` |
|     75 | 4968 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 4969 | `	}` |
|      7 | 4970 | `	return 1;` |
|      6 | 4971 |  |
|      - | 4972 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 4973 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 4974 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 4975 | `	SyhttpUri sUri;` |
|     15 | 4976 | `	if( n==0 ){ return 0; }` |
|     15 | 4977 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 4978 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 4979 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 4980 |  |
|      - | 4981 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 4982 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 4983 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 4984 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 4985 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 4986 | `	int i, runStart = 0;` |
|     37 | 4987 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 4988 | `	for( i=0; i<n; i++ ){` |
|     91 | 4989 | `		char c = z[i];` |
|     91 | 4990 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 4991 | `		if( !keep && isFloat ){` |
|     38 | 4992 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 4993 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 4994 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 4995 | `		}` |
|     61 | 4996 | `		if( !keep ){` |
|     33 | 4997 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 4998 | `			runStart = i+1;` |
|     16 | 4999 | `		}` |
|     31 | 5000 | `	}` |
|      7 | 5001 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5002 |  |
|      - | 5003 | `/* SANITIZE_SPECIAL_CHARS (full=0, numeric entities; also encodes control bytes` |
|      - | 5004 | ` * <32 as &#N;) / FULL_SPECIAL_CHARS (full=1, named entities for <>&"').` |
|      - | 5005 | ` * Divergence on bytes >=128: PHP's FULL filter is UTF-8-aware — it named-entity` |
|      - | 5006 | ` * encodes valid sequences ("\xC3\xA9" -> "&eacute;") and drops invalid ones; we` |
|      - | 5007 | ` * pass every byte >=128 through verbatim (the engine has no UTF-8 entity table,` |
|      - | 5008 | ` * and PH7_builtin_htmlspecialchars behaves the same way). Bytes 0-127 match. */` |
|      7 | 5009 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int full){` |
|      7 | 5010 | `	int i, runStart = 0;` |
|      - | 5011 | `	const char *zEnt;` |
|      7 | 5012 | `	ph7_result_string(pCtx,"",0);` |
|     43 | 5013 | `	for( i=0; i<n; i++ ){` |
|     37 | 5014 | `		unsigned char c = (unsigned char)z[i];` |
|     37 | 5015 | `		switch( c ){` |
|      5 | 5016 | `		case '<':  zEnt = full?"&lt;":"&#60;";   break;` |
|      5 | 5017 | `		case '>':  zEnt = full?"&gt;":"&#62;";   break;` |
|      5 | 5018 | `		case '&':  zEnt = full?"&amp;":"&#38;";  break;` |
|      5 | 5019 | `		case '"':  zEnt = full?"&quot;":"&#34;"; break;` |
|      5 | 5020 | `		case '\'': zEnt = full?"&#039;":"&#39;"; break;` |
|      8 | 5021 | `		default:` |
|     17 | 5022 | `			if( full \|\| c>=32 ){ continue; } /* keep in the current run */` |
|      - | 5023 | `			/* SPECIAL_CHARS encodes a control byte as a numeric entity. */` |
|      5 | 5024 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      5 | 5025 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      5 | 5026 | `			runStart = i+1;` |
|      5 | 5027 | `			continue;` |
|      - | 5028 | `		}` |
|     21 | 5029 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     21 | 5030 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     21 | 5031 | `		runStart = i+1;` |
|     11 | 5032 | `	}` |
|      7 | 5033 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5034 |  |
|     25 | 5035 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5036 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5037 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5038 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5039 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5040 |  |
|     23 | 5041 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5042 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5043 |  |
|      - | 5044 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5045 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5046 | `	int i, runStart = 0;` |
|      5 | 5047 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5048 | `	for( i=0; i<n; i++ ){` |
|     47 | 5049 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5050 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5051 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5052 | `			runStart = i+1;` |
|      5 | 5053 | `		}` |
|     24 | 5054 | `	}` |
|      5 | 5055 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5056 |  |
|      - | 5057 | `/*` |
|      - | 5058 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5059 | ` *  Validate or sanitize a value. The scalar input is coerced to a string and the` |
|      - | 5060 | ` *  selected filter applied; on validation failure the 'default' option (if any)` |
|      - | 5061 | ` *  is returned, else null when FILTER_NULL_ON_FAILURE is set, else false.` |
|      - | 5062 | ` */` |
|    230 | 5063 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5064 |  |
|    232 | 5065 | `	int iFilter = FV_DEFAULT, iFlags = 0, bNull;` |
|    232 | 5066 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|      - | 5067 | `	const char *zVal; int nVal;` |
|    232 | 5068 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    232 | 5069 | `	if( nArg>1 ){ iFilter = ph7_value_to_int(apArg[1]); }` |
|    232 | 5070 | `	if( nArg>2 ){` |
|     53 | 5071 | `		if( ph7_value_is_array(apArg[2]) ){` |
|     13 | 5072 | `			ph7_value *pF = ph7_array_fetch(apArg[2],"flags",(int)sizeof("flags")-1);` |
|     13 | 5073 | `			if( pF ){ iFlags = ph7_value_to_int(pF); }` |
|     13 | 5074 | `			pOpts = ph7_array_fetch(apArg[2],"options",(int)sizeof("options")-1);` |
|     13 | 5075 | `			if( pOpts && !ph7_value_is_array(pOpts) ){ pOpts = 0; }` |
|     13 | 5076 | `			if( pOpts ){ pDefault = ph7_array_fetch(pOpts,"default",(int)sizeof("default")-1); }` |
|      7 | 5077 | `		}else{` |
|     41 | 5078 | `			iFlags = ph7_value_to_int(apArg[2]);` |
|      - | 5079 | `		}` |
|     26 | 5080 | `	}` |
|    232 | 5081 | `	bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5082 | `	/* An array/object input fails every scalar filter. */` |
|    232 | 5083 | `	if( ph7_value_is_array(apArg[0]) ){ goto fail; }` |
|    230 | 5084 | `	zVal = ph7_value_to_string(apArg[0],&nVal);` |
|    230 | 5085 | `	switch( iFilter ){` |
|     28 | 5086 | `	case FV_VALIDATE_INT: {` |
|      - | 5087 | `		ph7_int64 v;` |
|     58 | 5088 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5089 | `		if( pOpts ){` |
|      7 | 5090 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5091 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5092 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5093 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5094 | `		}` |
|     29 | 5095 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5096 | `		return PH7_OK;` |
|      - | 5097 | `	}` |
|     20 | 5098 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5099 | `		double d;` |
|     41 | 5100 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     25 | 5101 | `		ph7_result_double(pCtx,d);` |
|     25 | 5102 | `		return PH7_OK;` |
|      - | 5103 | `	}` |
|     14 | 5104 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5105 | `		int b;` |
|     29 | 5106 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5107 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5108 | `		return PH7_OK;` |
|      - | 5109 | `	}` |
|     25 | 5110 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5111 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     21 | 5112 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5113 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5114 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5115 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5116 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5117 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5118 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5119 | `		if( pRe==0 ){` |
|      3 | 5120 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5121 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5122 | `		}` |
|      5 | 5123 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5124 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5125 | `		goto pass;` |
|      - | 5126 | `#else` |
|      - | 5127 | `		goto fail;` |
|      - | 5128 | `#endif` |
|      - | 5129 | `	}` |
|      3 | 5130 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5131 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|      5 | 5132 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5133 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeSpecial(pCtx,zVal,nVal,1); return PH7_OK;` |
|      3 | 5134 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5135 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|      5 | 5136 | `	case FV_DEFAULT: goto pass; /* FILTER_UNSAFE_RAW: pass through unchanged */` |
|    ! 0 | 5137 | `	default:` |
|    ! 0 | 5138 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5139 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5140 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5141 | `	}` |
|     48 | 5142 | `fail:` |
|     97 | 5143 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|     95 | 5144 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|     91 | 5145 | `	else { ph7_result_bool(pCtx,0); }` |
|     97 | 5146 | `	return PH7_OK;` |
|     22 | 5147 | `pass: /* validation passed: return the (string) input unchanged */` |
|     45 | 5148 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     45 | 5149 | `	return PH7_OK;` |
|    117 | 5150 |  |
|      - | 5151 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5152 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5153 | `/*` |
|      - | 5154 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5155 |  |
|      - | 5156 | ` */` |
|      4 | 5157 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5158 | `	const char *zInput, /* Raw input */` |
|      - | 5159 | `	int nByte,  /* Input length */` |
|      - | 5160 | `	int delim,  /* Delimiter */` |
|      - | 5161 | `	int encl,   /* Enclosure */` |
|      - | 5162 | `	int escape,  /* Escape character */` |
|      - | 5163 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5164 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5165 | `	)` |
|      1 | 5166 |  |
|      5 | 5167 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5168 | `	const char *zIn = zInput;` |
|      - | 5169 | `	const char *zPtr;` |
|      - | 5170 | `	int isEnc;` |
|      - | 5171 | `	/* Start processing */` |
|      8 | 5172 | `	for(;;){` |
|     17 | 5173 | `		if( zIn >= zEnd ){` |
|      - | 5174 | `			/* No more input to process */` |
|      5 | 5175 | `			break;` |
|      - | 5176 | `		}` |
|     13 | 5177 | `		isEnc = 0;` |
|     13 | 5178 | `		zPtr = zIn;` |
|      - | 5179 | `		/* Find the first delimiter */` |
|     27 | 5180 | `		while( zIn < zEnd ){` |
|     23 | 5181 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5182 | `				/* Delimiter found,break imediately */` |
|      5 | 5183 | `				break;` |
|     15 | 5184 | `			}else if( zIn[0] == encl ){` |
|      - | 5185 | `				/* Inside enclosure? */` |
|    ! 0 | 5186 | `				isEnc = !isEnc;` |
|     15 | 5187 | `			}else if( zIn[0] == escape ){` |
|      - | 5188 | `				/* Escape sequence */` |
|    ! 0 | 5189 | `				zIn++;` |
|    ! 0 | 5190 | `			}` |
|      - | 5191 | `			/* Advance the cursor */` |
|     15 | 5192 | `			zIn++;` |
|      1 | 5193 | `		}` |
|     13 | 5194 | `		if( zIn > zPtr ){` |
|     13 | 5195 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5196 | `			sxi32 rc;` |
|      - | 5197 | `			/* Invoke the supllied callback */` |
|     13 | 5198 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5199 | `				zPtr++;` |
|    ! 0 | 5200 | `				nByteChunk-=2;` |
|    ! 0 | 5201 | `			}` |
|     13 | 5202 | `			if( nByteChunk > 0 ){` |
|     13 | 5203 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5204 | `				if( rc == SXERR_ABORT ){` |
|      - | 5205 | `					/* User callback request an operation abort */` |
|    ! 0 | 5206 | `					break;` |
|      - | 5207 | `				}` |
|      6 | 5208 | `			}` |
|      6 | 5209 | `		}` |
|      - | 5210 | `		/* Ignore trailing delimiter */` |
|     21 | 5211 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5212 | `			zIn++;` |
|      1 | 5213 | `		}` |
|      1 | 5214 | `	}` |
|      5 | 5215 | `	return SXRET_OK;` |
|      1 | 5216 |  |
|      - | 5217 | `/*` |
|      - | 5218 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5219 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5220 | ` * argument to this callback.` |
|      - | 5221 | ` */` |
|     12 | 5222 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5223 |  |
|     13 | 5224 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5225 | `	ph7_value sEntry;` |
|      - | 5226 | `	SyString sToken;` |
|      - | 5227 | `	/* Insert the token in the given array */` |
|     13 | 5228 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5229 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5230 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5231 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5232 | `		return SXRET_OK;` |
|      - | 5233 | `	}` |
|     13 | 5234 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5235 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5236 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5237 | `	return SXRET_OK;` |
|      7 | 5238 |  |
|      - | 5239 | `/*` |
|      - | 5240 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5241 | ` *  Parse a CSV string into an array.` |
|      - | 5242 | ` * Parameters` |
|      - | 5243 | ` *  $input` |
|      - | 5244 | ` *   The string to parse.` |
|      - | 5245 | ` *  $delimiter` |
|      - | 5246 | ` *   Set the field delimiter (one character only).` |
|      - | 5247 | ` *  $enclosure` |
|      - | 5248 | ` *   Set the field enclosure character (one character only).` |
|      - | 5249 | ` *  $escape` |
|      - | 5250 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5251 | ` * Return` |
|      - | 5252 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5253 | ` */` |
|      4 | 5254 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5255 |  |
|      - | 5256 | `	const char *zInput,*zPtr;` |
|      - | 5257 | `	ph7_value *pArray;` |
|      5 | 5258 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5259 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5260 | `	int escape = '\\';  /* Escape character */` |
|      - | 5261 | `	int nLen;` |
|      5 | 5262 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5263 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5264 | `		ph7_result_null(pCtx);` |
|      3 | 5265 | `		return PH7_OK;` |
|      - | 5266 | `	}` |
|      - | 5267 | `	/* Extract the raw input */` |
|      3 | 5268 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5269 | `	if( nArg > 1 ){` |
|      - | 5270 | `		int i;` |
|      3 | 5271 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5272 | `			/* Extract the delimiter */` |
|      3 | 5273 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5274 | `			if( i > 0 ){` |
|      3 | 5275 | `				delim = zPtr[0];` |
|      1 | 5276 | `			}` |
|      1 | 5277 | `		}` |
|      3 | 5278 | `		if( nArg > 2 ){` |
|      3 | 5279 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5280 | `				/* Extract the enclosure */` |
|      3 | 5281 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5282 | `				if( i > 0 ){` |
|      3 | 5283 | `					encl = zPtr[0];` |
|      1 | 5284 | `				}` |
|      1 | 5285 | `			}` |
|      3 | 5286 | `			if( nArg > 3 ){` |
|      3 | 5287 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5288 | `					/* Extract the escape character */` |
|      3 | 5289 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5290 | `					if( i > 0 ){` |
|      3 | 5291 | `						escape = zPtr[0];` |
|      1 | 5292 | `					}` |
|      1 | 5293 | `				}` |
|      1 | 5294 | `			}` |
|      1 | 5295 | `		}` |
|      1 | 5296 | `	}` |
|      - | 5297 | `	/* Create our array */` |
|      3 | 5298 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5299 | `	if( pArray == 0 ){` |
|      - | 5300 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5301 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5302 | `	}` |
|      - | 5303 | `	/* Parse the raw input */` |
|      3 | 5304 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5305 | `	/* Return the freshly created array */` |
|      3 | 5306 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5307 | `	return PH7_OK;` |
|      3 | 5308 |  |
|      - | 5309 | `/*` |
|      - | 5310 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5311 | ` * container.` |
|      - | 5312 | ` * Refer to [strip_tags()].` |
|      - | 5313 | ` */` |
|     10 | 5314 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5315 |  |
|     11 | 5316 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5317 | `	const char *zPtr;` |
|      - | 5318 | `	SyString sEntry;` |
|      - | 5319 | `	/* Strip tags */` |
|     10 | 5320 | `	for(;;){` |
|     45 | 5321 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5322 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5323 | `				zTag++;` |
|      1 | 5324 | `		}` |
|     21 | 5325 | `		if( zTag >= zEnd ){` |
|     11 | 5326 | `			break;` |
|      - | 5327 | `		}` |
|     11 | 5328 | `		zPtr = zTag;` |
|      - | 5329 | `		/* Delimit the tag */` |
|     25 | 5330 | `		while(zTag < zEnd ){` |
|     25 | 5331 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5332 | `				/* UTF-8 stream */` |
|      3 | 5333 | `				zTag++;` |
|      5 | 5334 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5335 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5336 | `				break;` |
|    ! 0 | 5337 | `			}else{` |
|     13 | 5338 | `				zTag++;` |
|      - | 5339 | `			}` |
|      1 | 5340 | `		}` |
|     11 | 5341 | `		if( zTag > zPtr ){` |
|      - | 5342 | `			/* Perform the insertion */` |
|     11 | 5343 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5344 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5345 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5346 | `		}` |
|      - | 5347 | `		/* Jump the trailing '>' */` |
|     11 | 5348 | `		zTag++;` |
|      1 | 5349 | `	}` |
|     11 | 5350 | `	return SXRET_OK;` |
|      1 | 5351 |  |
|      - | 5352 | `/*` |
|      - | 5353 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5354 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5355 | ` * Refer to [strip_tags()].` |
|      - | 5356 | ` */` |
|     36 | 5357 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5358 |  |
|     37 | 5359 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5360 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5361 | `		SyString sTag;` |
|     85 | 5362 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5363 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5364 | `			zTag++;` |
|      1 | 5365 | `		}` |
|      - | 5366 | `		/* Delimit the tag */` |
|     25 | 5367 | `		zCur = zTag;` |
|     77 | 5368 | `		while(zTag < zEnd ){` |
|     77 | 5369 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5370 | `				/* UTF-8 stream */` |
|      5 | 5371 | `				zTag++;` |
|      9 | 5372 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5373 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5374 | `				break;` |
|    ! 0 | 5375 | `			}else{` |
|     49 | 5376 | `				zTag++;` |
|      - | 5377 | `			}` |
|      1 | 5378 | `		}` |
|     25 | 5379 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5380 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5381 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5382 | `		if( sTag.nByte > 0 ){` |
|      - | 5383 | `			SyString *aEntry,*pEntry;` |
|      - | 5384 | `			sxi32 rc;` |
|      - | 5385 | `			sxu32 n;` |
|      - | 5386 | `			/* Perform the lookup */` |
|     25 | 5387 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5388 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5389 | `				pEntry = &aEntry[n];` |
|      - | 5390 | `				/* Do the comparison */` |
|     25 | 5391 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5392 | `				if( !rc ){` |
|     21 | 5393 | `					return SXRET_OK;` |
|      - | 5394 | `				}` |
|      3 | 5395 | `			}` |
|      2 | 5396 | `		}` |
|      2 | 5397 | `	}` |
|      - | 5398 | `	/* No such tag */` |
|     17 | 5399 | `	return SXERR_NOTFOUND;` |
|     19 | 5400 |  |
|      - | 5401 | `/*` |
|      - | 5402 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5403 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5404 | ` * Refer to [strip_tags()].` |
|      - | 5405 | ` */` |
|     16 | 5406 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5407 |  |
|     17 | 5408 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5409 | `	const char *zPtr,*zTag;` |
|      - | 5410 | `	SySet sSet;` |
|      - | 5411 | `	/* initialize the set of allowed tags */` |
|     17 | 5412 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5413 | `	if( nTaglen > 0 ){` |
|      - | 5414 | `		/* Set of allowed tags */` |
|     11 | 5415 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5416 | `	}` |
|      - | 5417 | `	/* Set the empty string */` |
|     17 | 5418 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5419 | `	/* Start processing */` |
|     26 | 5420 | `	for(;;){` |
|     53 | 5421 | `		if(zIn >= zEnd){` |
|      - | 5422 | `			/* No more input to process */` |
|     15 | 5423 | `			break;` |
|      - | 5424 | `		}` |
|     39 | 5425 | `		zPtr = zIn;` |
|      - | 5426 | `		/* Find a tag */` |
|    133 | 5427 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5428 | `			zIn++;` |
|      1 | 5429 | `		}` |
|     39 | 5430 | `		if( zIn > zPtr ){` |
|      - | 5431 | `			/* Consume raw input */` |
|     21 | 5432 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5433 | `		}` |
|      - | 5434 | `		/* Ignore trailing null bytes */` |
|     39 | 5435 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5436 | `			zIn++;` |
|    ! 0 | 5437 | `		}` |
|     39 | 5438 | `		if(zIn >= zEnd){` |
|      - | 5439 | `			/* No more input to process */` |
|      3 | 5440 | `			break;` |
|      - | 5441 | `		}` |
|     37 | 5442 | `		if( zIn[0] == '<' ){` |
|      - | 5443 | `			sxi32 rc;` |
|     37 | 5444 | `			zTag = zIn++;` |
|      - | 5445 | `			/* Delimit the tag */` |
|    127 | 5446 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5447 | `				zIn++;` |
|      1 | 5448 | `			}` |
|     37 | 5449 | `			if( zIn < zEnd ){` |
|     37 | 5450 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5451 | `			}` |
|      - | 5452 | `			/* Query the set */` |
|     37 | 5453 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5454 | `			if( rc == SXRET_OK ){` |
|      - | 5455 | `				/* Keep the tag */` |
|     21 | 5456 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5457 | `			}` |
|     18 | 5458 | `		}` |
|      1 | 5459 | `	}` |
|      - | 5460 | `	/* Cleanup */` |
|     17 | 5461 | `	SySetRelease(&sSet);` |
|     17 | 5462 | `	return SXRET_OK;` |
|      1 | 5463 |  |
|      - | 5464 | `/*` |
|      - | 5465 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5466 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5467 | ` * Parameters` |
|      - | 5468 | ` *  $str` |
|      - | 5469 | ` *  The input string.` |
|      - | 5470 | ` * $allowable_tags` |
|      - | 5471 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5472 | ` * Return` |
|      - | 5473 | ` *  Returns the stripped string.` |
|      - | 5474 | ` */` |
|     16 | 5475 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5476 |  |
|     17 | 5477 | `	const char *zTaglist = 0;` |
|      - | 5478 | `	const char *zString;` |
|     17 | 5479 | `	int nTaglen = 0;` |
|      - | 5480 | `	int nLen;` |
|     17 | 5481 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5482 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5483 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5484 | `		return PH7_OK;` |
|      - | 5485 | `	}` |
|      - | 5486 | `	/* Point to the raw string */` |
|     15 | 5487 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5488 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5489 | `		/* Allowed tag */` |
|     11 | 5490 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5491 | `	}` |
|      - | 5492 | `	/* Process input */` |
|     15 | 5493 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5494 | `	return PH7_OK;` |
|      9 | 5495 |  |
|      - | 5496 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5497 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5498 | `/*` |
|      - | 5499 | ` * string str_shuffle(string $str)` |
|      - | 5500 |  |
|      - | 5501 | ` *  Randomly shuffles a string.` |
|      - | 5502 | ` * Parameters` |
|      - | 5503 | ` *  $str` |
|      - | 5504 | ` *   The input string.` |
|      - | 5505 | ` * Return` |
|      - | 5506 | ` *  Returns the shuffled string.` |
|      - | 5507 | ` */` |
|     12 | 5508 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5509 |  |
|      - | 5510 | `	const char *zString;` |
|      - | 5511 | `	int nLen,i,c;` |
|      - | 5512 | `	sxu32 iR;` |
|     13 | 5513 | `	if( nArg < 1 ){` |
|      - | 5514 | `		/* Missing arguments,return the empty string */` |
|      3 | 5515 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5516 | `		return PH7_OK;` |
|      - | 5517 | `	}` |
|      - | 5518 | `	/* Extract the target string */` |
|     11 | 5519 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5520 | `	if( nLen < 1 ){` |
|      - | 5521 | `		/* Nothing to shuffle */` |
|      3 | 5522 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5523 | `		return PH7_OK;` |
|      - | 5524 | `	}` |
|      - | 5525 | `	/* Shuffle the string */` |
|     43 | 5526 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5527 | `		/* Generate a random number first */` |
|     35 | 5528 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5529 | `		/* Extract a random offset */` |
|     35 | 5530 | `		c = zString[iR % nLen];` |
|      - | 5531 | `		/* Append it */` |
|     35 | 5532 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5533 | `	}` |
|      9 | 5534 | `	return PH7_OK;` |
|      7 | 5535 |  |
|      - | 5536 | `/*` |
|      - | 5537 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5538 | ` *  Convert a string to an array.` |
|      - | 5539 | ` * Parameters` |
|      - | 5540 | ` * $string` |
|      - | 5541 | ` *  The input string.` |
|      - | 5542 | ` * $split_length` |
|      - | 5543 | ` *  Maximum length of the chunk.` |
|      - | 5544 | ` * Return` |
|      - | 5545 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5546 | ` *  except possibly the last one which may be shorter.` |
|      - | 5547 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5548 | ` *  as the first (and only) array element.` |
|      - | 5549 | ` *  An empty string returns an empty array.` |
|      - | 5550 | ` * Errors` |
|      - | 5551 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5552 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5553 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5554 | ` */` |
|     28 | 5555 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5556 |  |
|      - | 5557 | `	const char *zString,*zEnd;` |
|      - | 5558 | `	ph7_value *pArray,*pValue;` |
|      - | 5559 | `	int split_len;` |
|      - | 5560 | `	int nLen;` |
|     33 | 5561 | `	if( nArg < 1 ){` |
|      4 | 5562 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5563 | `			"ArgumentCountError",` |
|      - | 5564 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5565 | `			nArg` |
|      - | 5566 | `			);` |
|      - | 5567 | `	}` |
|      - | 5568 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 5569 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 5570 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5571 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5572 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5573 | `			"TypeError",` |
|      - | 5574 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5575 | `			ph7_type_name(apArg[0])` |
|      - | 5576 | `			);` |
|      - | 5577 | `	}` |
|      - | 5578 | `	/* Point to the target string */` |
|     27 | 5579 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 5580 | `	split_len = (int)sizeof(char);` |
|     27 | 5581 | `	if( nArg > 1 ){` |
|      - | 5582 | `		/* Split length */` |
|     17 | 5583 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 5584 | `		if( split_len < 1 ){` |
|      6 | 5585 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5586 | `				"ValueError",` |
|      - | 5587 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5588 | `				);` |
|      - | 5589 | `		}` |
|     11 | 5590 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5591 | `			split_len = nLen;` |
|      1 | 5592 | `		}` |
|      5 | 5593 | `	}` |
|      - | 5594 | `	/* Create the array and the scalar value */` |
|     21 | 5595 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5596 | `	/*Chunk value */` |
|     21 | 5597 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 5598 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5599 | `		/* Return FALSE */` |
|    ! 0 | 5600 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5601 | `		return PH7_OK;` |
|      - | 5602 | `	}` |
|      - | 5603 | `	/* Point to the end of the string */` |
|     21 | 5604 | `	zEnd = &zString[nLen];` |
|      - | 5605 | `	/* Perform the requested operation */` |
|     48 | 5606 | `	for(;;){` |
|      - | 5607 | `		int nMax;` |
|     59 | 5608 | `		if( zString >= zEnd ){` |
|      - | 5609 | `			/* No more input to process */` |
|     21 | 5610 | `			break;` |
|      - | 5611 | `		}` |
|     39 | 5612 | `		nMax = (int)(zEnd-zString);` |
|     39 | 5613 | `		if( nMax < split_len ){` |
|      3 | 5614 | `			split_len = nMax;` |
|      1 | 5615 | `		}` |
|      - | 5616 | `		/* Copy the current chunk */` |
|     39 | 5617 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5618 | `		/* Insert it */` |
|     39 | 5619 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 5620 | `		/* reset the string cursor */` |
|     39 | 5621 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5622 | `		/* Update position */` |
|     39 | 5623 | `		zString += split_len;` |
|      1 | 5624 | `	}` |
|      - | 5625 | `	/*` |
|      - | 5626 | `	 * Return the array.` |
|      - | 5627 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5628 | `	 * upon we return from this function.` |
|      - | 5629 | `	 */` |
|     21 | 5630 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 5631 | `	return PH7_OK;` |
|     19 | 5632 |  |
|      - | 5633 | `/*` |
|      - | 5634 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5635 | ` * Refer to [strspn()].` |
|      - | 5636 | ` */` |
|     28 | 5637 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5638 |  |
|     29 | 5639 | `	const char *zIn = *pzIn;` |
|      - | 5640 | `	const char *zPtr;` |
|      - | 5641 | `	/* Ignore leading white spaces */` |
|     29 | 5642 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5643 | `		zIn++;` |
|    ! 0 | 5644 | `	}` |
|     29 | 5645 | `	if( zIn >= zEnd ){` |
|      - | 5646 | `		/* End of input */` |
|    ! 0 | 5647 | `		return SXERR_EOF;` |
|      - | 5648 | `	}` |
|     29 | 5649 | `	zPtr = zIn;` |
|      - | 5650 | `	/* Extract the token */` |
|    201 | 5651 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5652 | `		zIn++;` |
|      1 | 5653 | `	}` |
|     29 | 5654 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5655 | `	/* Synchronize pointers */` |
|     29 | 5656 | `	*pzIn = zIn;` |
|      - | 5657 | `	/* Return to the caller */` |
|     29 | 5658 | `	return SXRET_OK;` |
|     15 | 5659 |  |
|      - | 5660 | `/*` |
|      - | 5661 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5662 | ` * return the longest match.` |
|      - | 5663 | ` * Refer to [strspn()].` |
|      - | 5664 | ` */` |
|     18 | 5665 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5666 |  |
|     19 | 5667 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5668 | `	const char *zIn = zString;` |
|      - | 5669 | `	int i,c;` |
|     45 | 5670 | `	for(;;){` |
|     91 | 5671 | `		if( zString >= zEnd ){` |
|      7 | 5672 | `			break;` |
|      - | 5673 | `		}` |
|      - | 5674 | `		/* Extract current character */` |
|     85 | 5675 | `		c = zString[0];` |
|      - | 5676 | `		/* Perform the lookup */` |
|    383 | 5677 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5678 | `			if( c == zMask[i] ){` |
|      - | 5679 | `				/* Character found */` |
|     73 | 5680 | `				break;` |
|      - | 5681 | `			}` |
|    150 | 5682 | `		}` |
|     85 | 5683 | `		if( i >= nMaskLen ){` |
|      - | 5684 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5685 | `			break;` |
|      - | 5686 | `		}` |
|      - | 5687 | `		/* Advance cursor */` |
|     73 | 5688 | `		zString++;` |
|      1 | 5689 | `	}` |
|      - | 5690 | `	/* Longest match */` |
|     19 | 5691 | `	return (int)(zString-zIn);` |
|      1 | 5692 |  |
|      - | 5693 | `/*` |
|      - | 5694 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5695 | ` * Refer to [strcspn()].` |
|      - | 5696 | ` */` |
|     10 | 5697 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5698 |  |
|     11 | 5699 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5700 | `	const char *zIn = zString;` |
|      - | 5701 | `	int i,c;` |
|     12 | 5702 | `	for(;;){` |
|     25 | 5703 | `		if( zString >= zEnd ){` |
|      3 | 5704 | `			break;` |
|      - | 5705 | `		}` |
|      - | 5706 | `		/* Extract current character */` |
|     23 | 5707 | `		c = zString[0];` |
|      - | 5708 | `		/* Perform the lookup */` |
|     51 | 5709 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5710 | `			if( c == zMask[i] ){` |
|      9 | 5711 | `				break;` |
|      - | 5712 | `			}` |
|     15 | 5713 | `		}` |
|     23 | 5714 | `		if( i < nMaskLen ){` |
|      - | 5715 | `			/* Character in the current mask,break immediately */` |
|      9 | 5716 | `			break;` |
|      - | 5717 | `		}` |
|      - | 5718 | `		/* Advance cursor */` |
|     15 | 5719 | `		zString++;` |
|      1 | 5720 | `	}` |
|      - | 5721 | `	/* Longest match */` |
|     11 | 5722 | `	return (int)(zString-zIn);` |
|      1 | 5723 |  |
|      - | 5724 | `/*` |
|      - | 5725 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5726 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5727 | ` *  of characters contained within a given mask.` |
|      - | 5728 | ` * Parameters` |
|      - | 5729 | ` * $str` |
|      - | 5730 | ` *  The input string.` |
|      - | 5731 | ` * $mask` |
|      - | 5732 | ` *  The list of allowable characters.` |
|      - | 5733 | ` * $start` |
|      - | 5734 | ` *  The position in subject to start searching.` |
|      - | 5735 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5736 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5737 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5738 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5739 | ` *  start'th position from the end of subject.` |
|      - | 5740 | ` * $length` |
|      - | 5741 | ` *  The length of the segment from subject to examine.` |
|      - | 5742 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5743 | ` *  characters after the starting position.` |
|      - | 5744 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5745 | ` *  position up to length characters from the end of subject.` |
|      - | 5746 | ` * Return` |
|      - | 5747 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5748 | ` * in mask.` |
|      - | 5749 | ` */` |
|     26 | 5750 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5751 |  |
|      - | 5752 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5753 | `	int iMasklen,iLen;` |
|      - | 5754 | `	SyString sToken;` |
|     27 | 5755 | `	int iCount = 0;` |
|      - | 5756 | `	int rc;` |
|     27 | 5757 | `	if( nArg < 2 ){` |
|      - | 5758 | `		/* Missing agruments,return zero */` |
|      3 | 5759 | `		ph7_result_int(pCtx,0);` |
|      3 | 5760 | `		return PH7_OK;` |
|      - | 5761 | `	}` |
|      - | 5762 | `	/* Extract the target string */` |
|     25 | 5763 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5764 | `	/* Extract the mask */` |
|     25 | 5765 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5766 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5767 | `		/* Nothing to process,return zero */` |
|      7 | 5768 | `		ph7_result_int(pCtx,0);` |
|      7 | 5769 | `		return PH7_OK;` |
|      - | 5770 | `	}` |
|     19 | 5771 | `	if( nArg > 2 ){` |
|      - | 5772 | `		int nOfft;` |
|      - | 5773 | `		/* Extract the offset */` |
|      9 | 5774 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5775 | `		if( nOfft < 0 ){` |
|    ! 0 | 5776 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5777 | `			if( zBase > zString ){` |
|    ! 0 | 5778 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5779 | `				zString = zBase;` |
|    ! 0 | 5780 | `			}else{` |
|      - | 5781 | `				/* Invalid offset */` |
|    ! 0 | 5782 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5783 | `				return PH7_OK;` |
|      - | 5784 | `			}` |
|    ! 0 | 5785 | `		}else{` |
|      9 | 5786 | `			if( nOfft >= iLen ){` |
|      - | 5787 | `				/* Invalid offset */` |
|    ! 0 | 5788 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5789 | `				return PH7_OK;` |
|    ! 0 | 5790 | `			}else{` |
|      - | 5791 | `				/* Update offset */` |
|      9 | 5792 | `				zString += nOfft;` |
|      9 | 5793 | `				iLen -= nOfft;` |
|      - | 5794 | `			}` |
|      - | 5795 | `		}` |
|      9 | 5796 | `		if( nArg > 3 ){` |
|      - | 5797 | `			int iUserlen;` |
|      - | 5798 | `			/* Extract the desired length */` |
|      9 | 5799 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5800 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5801 | `				iLen = iUserlen;` |
|      2 | 5802 | `			}` |
|      4 | 5803 | `		}` |
|      4 | 5804 | `	}` |
|      - | 5805 | `	/* Point to the end of the string */` |
|     19 | 5806 | `	zEnd = &zString[iLen];` |
|      - | 5807 | `	/* Extract the first non-space token */` |
|     19 | 5808 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5809 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5810 | `		/* Compare against the current mask */` |
|     19 | 5811 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5812 | `	}` |
|      - | 5813 | `	/* Longest match */` |
|     19 | 5814 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5815 | `	return PH7_OK;` |
|     14 | 5816 |  |
|      - | 5817 | `/*` |
|      - | 5818 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5819 | ` *  Find length of initial segment not matching mask.` |
|      - | 5820 | ` * Parameters` |
|      - | 5821 | ` * $str` |
|      - | 5822 | ` *  The input string.` |
|      - | 5823 | ` * $mask` |
|      - | 5824 | ` *  The list of not allowed characters.` |
|      - | 5825 | ` * $start` |
|      - | 5826 | ` *  The position in subject to start searching.` |
|      - | 5827 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5828 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5829 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5830 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5831 | ` *  start'th position from the end of subject.` |
|      - | 5832 | ` * $length` |
|      - | 5833 | ` *  The length of the segment from subject to examine.` |
|      - | 5834 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5835 | ` *  characters after the starting position.` |
|      - | 5836 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5837 | ` *  position up to length characters from the end of subject.` |
|      - | 5838 | ` * Return` |
|      - | 5839 | ` *  Returns the length of the segment as an integer.` |
|      - | 5840 | ` */` |
|     16 | 5841 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5842 |  |
|      - | 5843 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5844 | `	int iMasklen,iLen;` |
|      - | 5845 | `	SyString sToken;` |
|     17 | 5846 | `	int iCount = 0;` |
|      - | 5847 | `	int rc;` |
|     17 | 5848 | `	if( nArg < 2 ){` |
|      - | 5849 | `		/* Missing agruments,return zero */` |
|      3 | 5850 | `		ph7_result_int(pCtx,0);` |
|      3 | 5851 | `		return PH7_OK;` |
|      - | 5852 | `	}` |
|      - | 5853 | `	/* Extract the target string */` |
|     15 | 5854 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5855 | `	/* Extract the mask */` |
|     15 | 5856 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5857 | `	if( iLen < 1 ){` |
|      - | 5858 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5859 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5860 | `		return PH7_OK;` |
|      - | 5861 | `	}` |
|     15 | 5862 | `	if( iMasklen < 1 ){` |
|      - | 5863 | `		/* No given mask,return the string length */` |
|      3 | 5864 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5865 | `		return PH7_OK;` |
|      - | 5866 | `	}` |
|     13 | 5867 | `	if( nArg > 2 ){` |
|      - | 5868 | `		int nOfft;` |
|      - | 5869 | `		/* Extract the offset */` |
|     11 | 5870 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5871 | `		if( nOfft < 0 ){` |
|    ! 0 | 5872 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5873 | `			if( zBase > zString ){` |
|    ! 0 | 5874 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5875 | `				zString = zBase;` |
|    ! 0 | 5876 | `			}else{` |
|      - | 5877 | `				/* Invalid offset */` |
|    ! 0 | 5878 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5879 | `				return PH7_OK;` |
|      - | 5880 | `			}` |
|    ! 0 | 5881 | `		}else{` |
|     11 | 5882 | `			if( nOfft >= iLen ){` |
|      - | 5883 | `				/* Invalid offset */` |
|      3 | 5884 | `				ph7_result_int(pCtx,0);` |
|      3 | 5885 | `				return PH7_OK;` |
|    ! 0 | 5886 | `			}else{` |
|      - | 5887 | `				/* Update offset */` |
|      9 | 5888 | `				zString += nOfft;` |
|      9 | 5889 | `				iLen -= nOfft;` |
|      - | 5890 | `			}` |
|      - | 5891 | `		}` |
|      9 | 5892 | `		if( nArg > 3 ){` |
|      - | 5893 | `			int iUserlen;` |
|      - | 5894 | `			/* Extract the desired length */` |
|    ! 0 | 5895 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5896 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5897 | `				iLen = iUserlen;` |
|    ! 0 | 5898 | `			}` |
|    ! 0 | 5899 | `		}` |
|      4 | 5900 | `	}` |
|      - | 5901 | `	/* Point to the end of the string */` |
|     11 | 5902 | `	zEnd = &zString[iLen];` |
|      - | 5903 | `	/* Extract the first non-space token */` |
|     11 | 5904 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5905 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5906 | `		/* Compare against the current mask */` |
|     11 | 5907 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5908 | `	}` |
|      - | 5909 | `	/* Longest match */` |
|     11 | 5910 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5911 | `	return PH7_OK;` |
|      9 | 5912 |  |
|      - | 5913 | `/*` |
|      - | 5914 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5915 | ` *  Search a string for any of a set of characters.` |
|      - | 5916 | ` * Parameters` |
|      - | 5917 | ` *  $haystack` |
|      - | 5918 | ` *   The string where char_list is looked for.` |
|      - | 5919 | ` *  $char_list` |
|      - | 5920 | ` *   This parameter is case sensitive.` |
|      - | 5921 | ` * Return` |
|      - | 5922 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5923 | ` */` |
|      6 | 5924 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5925 |  |
|      - | 5926 | `	const char *zString,*zList,*zEnd;` |
|      - | 5927 | `	int iLen,iListLen,i,c;` |
|      - | 5928 | `	sxu32 nOfft,nMax;` |
|      - | 5929 | `	sxi32 rc;` |
|      7 | 5930 | `	if( nArg < 2 ){` |
|      - | 5931 | `		/* Missing arguments,return FALSE */` |
|      3 | 5932 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5933 | `		return PH7_OK;` |
|      - | 5934 | `	}` |
|      - | 5935 | `	/* Extract the haystack and the char list */` |
|      5 | 5936 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5937 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5938 | `	if( iLen < 1 ){` |
|      - | 5939 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5940 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5941 | `		return PH7_OK;` |
|      - | 5942 | `	}` |
|      - | 5943 | `	/* Point to the end of the string */` |
|      5 | 5944 | `	zEnd = &zString[iLen];` |
|      5 | 5945 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5946 | `	/* perform the requested operation */` |
|     15 | 5947 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5948 | `		c = zList[i];` |
|     11 | 5949 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5950 | `		if( rc == SXRET_OK ){` |
|      5 | 5951 | `			if( nMax < nOfft ){` |
|      3 | 5952 | `				nOfft = nMax;` |
|      1 | 5953 | `			}` |
|      2 | 5954 | `		}` |
|      6 | 5955 | `	}` |
|      5 | 5956 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5957 | `		/* No such substring,return FALSE */` |
|      3 | 5958 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5959 | `	}else{` |
|      - | 5960 | `		/* Return the substring */` |
|      3 | 5961 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5962 | `	}` |
|      5 | 5963 | `	return PH7_OK;` |
|      4 | 5964 |  |
|      - | 5965 | `/* SPDX-SnippetBegin */` |
|      - | 5966 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 5967 | `/* SPDX-License-Identifier: blessing */` |
|      - | 5968 | `/*` |
|      - | 5969 | ` * string soundex(string $str)` |
|      - | 5970 | ` *  Calculate the soundex key of a string.` |
|      - | 5971 | ` * Parameters` |
|      - | 5972 | ` *  $str` |
|      - | 5973 | ` *   The input string.` |
|      - | 5974 | ` * Return` |
|      - | 5975 | ` *  Returns the soundex key as a string.` |
|      - | 5976 | ` * Note:` |
|      - | 5977 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5978 | ` * source tree.` |
|      - | 5979 | ` */` |
|     20 | 5980 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5981 |  |
|      - | 5982 | `	const unsigned char *zIn;` |
|      - | 5983 | `	char zResult[8];` |
|      - | 5984 | `	int i, j;` |
|      - | 5985 | `	static const unsigned char iCode[] = {` |
|      - | 5986 |  |
|      - | 5987 |  |
|      - | 5988 |  |
|      - | 5989 |  |
|      - | 5990 |  |
|      - | 5991 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5992 |  |
|      - | 5993 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5994 | `	};` |
|     21 | 5995 | `	if( nArg < 1 ){` |
|      - | 5996 | `		/* Missing arguments,return the empty string */` |
|      3 | 5997 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5998 | `		return PH7_OK;` |
|      - | 5999 | `	}` |
|     19 | 6000 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 6001 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 6002 | `	if( zIn[i] ){` |
|     17 | 6003 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6004 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6005 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6006 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6007 | `			if( code>0 ){` |
|     45 | 6008 | `				if( code!=prevcode ){` |
|     33 | 6009 | `					prevcode = (unsigned char)code;` |
|     33 | 6010 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6011 | `				}` |
|     23 | 6012 | `			}else{` |
|     49 | 6013 | `				prevcode = 0;` |
|      - | 6014 | `			}` |
|     47 | 6015 | `		}` |
|     33 | 6016 | `		while( j<4 ){` |
|     17 | 6017 | `			zResult[j++] = '0';` |
|      1 | 6018 | `		}` |
|     17 | 6019 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6020 | `	}else{` |
|      3 | 6021 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 6022 | `	}` |
|     19 | 6023 | `	return PH7_OK;` |
|     11 | 6024 |  |
|      - | 6025 | `/* SPDX-SnippetEnd */` |
|      - | 6026 | `/*` |
|      - | 6027 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6028 | ` *  Wraps a string to a given number of characters.` |
|      - | 6029 | ` * Parameters` |
|      - | 6030 | ` *  $str` |
|      - | 6031 | ` *   The input string.` |
|      - | 6032 | ` * $width` |
|      - | 6033 | ` *  The column width.` |
|      - | 6034 | ` * $break` |
|      - | 6035 | ` *  The line is broken using the optional break parameter.` |
|      - | 6036 | ` * Return` |
|      - | 6037 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6038 | ` */` |
|     14 | 6039 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6040 |  |
|      - | 6041 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 6042 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 6043 | `	if( nArg < 1 ){` |
|      - | 6044 | `		/* Missing arguments,return the empty string */` |
|      3 | 6045 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6046 | `		return PH7_OK;` |
|      - | 6047 | `	}` |
|      - | 6048 | `	/* Extract the input string */` |
|     13 | 6049 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 6050 | `	if( iLen < 1 ){` |
|      - | 6051 | `		/* Nothing to process,return the empty string */` |
|      3 | 6052 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6053 | `		return PH7_OK;` |
|      - | 6054 | `	}` |
|      - | 6055 | `	/* Chunk length */` |
|     11 | 6056 | `	iChunk = 75;` |
|     11 | 6057 | `	iBreaklen = 0;` |
|     11 | 6058 | `	zBreak = ""; /* cc warning */` |
|     11 | 6059 | `	if( nArg > 1 ){` |
|     11 | 6060 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 6061 | `		if( iChunk < 1 ){` |
|    ! 0 | 6062 | `			iChunk = 75;` |
|    ! 0 | 6063 | `		}` |
|     11 | 6064 | `		if( nArg > 2 ){` |
|      3 | 6065 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 6066 | `		}` |
|      5 | 6067 | `	}` |
|     11 | 6068 | `	if( iBreaklen < 1 ){` |
|      - | 6069 | `		/* Set a default column break */` |
|      - | 6070 | `#ifdef __WINNT__` |
|      1 | 6071 | `		zBreak = "\r\n";` |
|      1 | 6072 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 6073 | `#else` |
|      8 | 6074 | `		zBreak = "\n";` |
|      8 | 6075 | `		iBreaklen = (int)sizeof(char);` |
|      - | 6076 | `#endif` |
|      4 | 6077 | `	}` |
|      - | 6078 | `	/* Perform the requested operation */` |
|     11 | 6079 | `	zEnd = &zIn[iLen];` |
|     41 | 6080 | `	for(;;){` |
|      - | 6081 | `		int nMax;` |
|     47 | 6082 | `		if( zIn >= zEnd ){` |
|      - | 6083 | `			/* No more input to process */` |
|     11 | 6084 | `			break;` |
|      - | 6085 | `		}` |
|     37 | 6086 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 6087 | `		if( iChunk > nMax ){` |
|     11 | 6088 | `			iChunk = nMax;` |
|      5 | 6089 | `		}` |
|      - | 6090 | `		/* Append the column first */` |
|     37 | 6091 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 6092 | `		/* Advance the cursor */` |
|     37 | 6093 | `		zIn += iChunk;` |
|     37 | 6094 | `		if( zIn < zEnd ){` |
|      - | 6095 | `			/* Append the line break */` |
|     27 | 6096 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 6097 | `		}` |
|      1 | 6098 | `	}` |
|     11 | 6099 | `	return PH7_OK;` |
|      8 | 6100 |  |
|      - | 6101 | `/*` |
|      - | 6102 | ` * Check if the given character is a member of the given mask.` |
|      - | 6103 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6104 | ` * Refer to [strtok()].` |
|      - | 6105 | ` */` |
|     30 | 6106 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6107 |  |
|      - | 6108 | `	int i;` |
|     57 | 6109 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6110 | `		if( c == zMask[i] ){` |
|     13 | 6111 | `			if( pOfft ){` |
|      5 | 6112 | `				*pOfft = i;` |
|      2 | 6113 | `			}` |
|     13 | 6114 | `			return TRUE;` |
|      - | 6115 | `		}` |
|     14 | 6116 | `	}` |
|     19 | 6117 | `	return FALSE;` |
|     16 | 6118 |  |
|      - | 6119 | `/*` |
|      - | 6120 | ` * Extract a single token from the input stream.` |
|      - | 6121 | ` * Refer to [strtok()].` |
|      - | 6122 | ` */` |
|      6 | 6123 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6124 |  |
|      7 | 6125 | `	const char *zIn = *pzIn;` |
|      - | 6126 | `	const char *zPtr;` |
|      - | 6127 | `	/* Ignore leading delimiter */` |
|     11 | 6128 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6129 | `		zIn++;` |
|      1 | 6130 | `	}` |
|      7 | 6131 | `	if( zIn >= zEnd ){` |
|      - | 6132 | `		/* End of input */` |
|    ! 0 | 6133 | `		return SXERR_EOF;` |
|      - | 6134 | `	}` |
|      7 | 6135 | `	zPtr = zIn;` |
|      - | 6136 | `	/* Extract the token */` |
|     13 | 6137 | `	while( zIn < zEnd ){` |
|     11 | 6138 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6139 | `			/* UTF-8 stream */` |
|    ! 0 | 6140 | `			zIn++;` |
|    ! 0 | 6141 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6142 | `		}else{` |
|     11 | 6143 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6144 | `				break;` |
|      - | 6145 | `			}` |
|      7 | 6146 | `			zIn++;` |
|      - | 6147 | `		}` |
|      1 | 6148 | `	}` |
|      7 | 6149 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6150 | `	/* Update the cursor */` |
|      7 | 6151 | `	*pzIn = zIn;` |
|      - | 6152 | `	/* Return to the caller */` |
|      7 | 6153 | `	return SXRET_OK;` |
|      4 | 6154 |  |
|      - | 6155 | `/* strtok auxiliary private data */` |
|      - | 6156 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6157 | `struct strtok_aux_data` |
|      - | 6158 |  |
|      - | 6159 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6160 | `	const char *zIn;   /* Current input stream */` |
|      - | 6161 | `	const char *zEnd;  /* End of input */` |
|      - | 6162 | `};` |
|      - | 6163 | `/*` |
|      - | 6164 | ` * string strtok(string $str,string $token)` |
|      - | 6165 | ` * string strtok(string $token)` |
|      - | 6166 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6167 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6168 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6169 | ` *  words by using the space character as the token.` |
|      - | 6170 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6171 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6172 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6173 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6174 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6175 | ` *  the argument are found.` |
|      - | 6176 | ` * Parameters` |
|      - | 6177 | ` *  $str` |
|      - | 6178 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6179 | ` * $token` |
|      - | 6180 | ` *  The delimiter used when splitting up str.` |
|      - | 6181 | ` * Return` |
|      - | 6182 | ` *   Current token or FALSE on EOF.` |
|      - | 6183 | ` */` |
|      8 | 6184 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6185 |  |
|      - | 6186 | `	strtok_aux_data *pAux;` |
|      - | 6187 | `	const char *zMask;` |
|      - | 6188 | `	SyString sToken;` |
|      - | 6189 | `	int nMasklen;` |
|      - | 6190 | `	sxi32 rc;` |
|      9 | 6191 | `	if( nArg < 2 ){` |
|      - | 6192 | `		/* Extract top aux data */` |
|      7 | 6193 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 6194 | `		if( pAux == 0 ){` |
|      - | 6195 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6196 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6197 | `			return PH7_OK;` |
|      - | 6198 | `		}` |
|      7 | 6199 | `		nMasklen = 0;` |
|      7 | 6200 | `		zMask = ""; /* cc warning */` |
|      7 | 6201 | `		if( nArg > 0 ){` |
|      - | 6202 | `			/* Extract the mask */` |
|      5 | 6203 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6204 | `		}` |
|      7 | 6205 | `		if( nMasklen < 1 ){` |
|      - | 6206 | `			/* Invalid mask,return FALSE */` |
|      3 | 6207 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 6208 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 6209 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 6210 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6211 | `			return PH7_OK;` |
|      - | 6212 | `		}` |
|      - | 6213 | `		/* Extract the token */` |
|      5 | 6214 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6215 | `		if( rc != SXRET_OK ){` |
|      - | 6216 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6217 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6218 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6219 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6220 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6221 | `		}else{` |
|      - | 6222 | `			/* Return the extracted token */` |
|      5 | 6223 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6224 | `		}` |
|      3 | 6225 | `	}else{` |
|      - | 6226 | `		const char *zInput,*zCur;` |
|      - | 6227 | `		char *zDup;` |
|      - | 6228 | `		int nLen;` |
|      - | 6229 | `		/* Extract the raw input */` |
|      3 | 6230 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6231 | `		if( nLen < 1 ){` |
|      - | 6232 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6233 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6234 | `			return PH7_OK;` |
|      - | 6235 | `		}` |
|      - | 6236 | `		/* Extract the mask */` |
|      3 | 6237 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6238 | `		if( nMasklen < 1 ){` |
|      - | 6239 | `			/* Set a default mask */` |
|      - | 6240 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6241 | `			zMask = TOK_MASK;` |
|    ! 0 | 6242 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6243 | `#undef TOK_MASK` |
|    ! 0 | 6244 | `		}` |
|      - | 6245 | `		/* Extract a single token */` |
|      3 | 6246 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6247 | `		if( rc != SXRET_OK ){` |
|      - | 6248 | `			/* Empty input */` |
|    ! 0 | 6249 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6250 | `			return PH7_OK;` |
|    ! 0 | 6251 | `		}else{` |
|      - | 6252 | `			/* Return the extracted token */` |
|      3 | 6253 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6254 | `		}` |
|      - | 6255 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6256 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6257 | `		if( pAux ){` |
|      3 | 6258 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6259 | `			if( nLen < 1 ){` |
|    ! 0 | 6260 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6261 | `				return PH7_OK;` |
|      - | 6262 | `			}` |
|      - | 6263 | `			/* Duplicate input */` |
|      3 | 6264 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6265 | `			if( zDup  ){` |
|      3 | 6266 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6267 | `				/* Register the aux data */` |
|      3 | 6268 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6269 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6270 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6271 | `			}` |
|      1 | 6272 | `		}` |
|      - | 6273 | `	}` |
|      7 | 6274 | `	return PH7_OK;` |
|      5 | 6275 |  |
|      - | 6276 | `/*` |
|      - | 6277 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6278 | ` *  Pad a string to a certain length with another string` |
|      - | 6279 | ` * Parameters` |
|      - | 6280 | ` *  $input` |
|      - | 6281 | ` *   The input string.` |
|      - | 6282 | ` * $pad_length` |
|      - | 6283 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6284 | ` *   string, no padding takes place.` |
|      - | 6285 | ` * $pad_string` |
|      - | 6286 | ` *   Note:` |
|      - | 6287 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6288 | ` *    divided by the pad_string's length.` |
|      - | 6289 | ` * $pad_type` |
|      - | 6290 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6291 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6292 | ` * Return` |
|      - | 6293 | ` *  The padded string.` |
|      - | 6294 | ` */` |
|     10 | 6295 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6296 |  |
|      - | 6297 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6298 | `	const char *zIn,*zPad;` |
|     11 | 6299 | `	if( nArg < 2 ){` |
|      - | 6300 | `		/* Missing arguments,return the empty string */` |
|      5 | 6301 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6302 | `		return PH7_OK;` |
|      - | 6303 | `	}` |
|      - | 6304 | `	/* Extract the target string */` |
|      7 | 6305 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6306 | `	/* Padding length */` |
|      7 | 6307 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6308 | `	if( iPadlen > 0 ){` |
|      5 | 6309 | `		iPadlen -= iLen;` |
|      2 | 6310 | `	}` |
|      7 | 6311 | `	if( iPadlen < 1  ){` |
|      - | 6312 | `		/* Return the string verbatim */` |
|      3 | 6313 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 6314 | `		return PH7_OK;` |
|      - | 6315 | `	}` |
|      5 | 6316 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6317 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6318 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6319 | `	if( nArg > 2 ){` |
|      - | 6320 | `		/* Padding string */` |
|      5 | 6321 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6322 | `		if( iStrpad < 1 ){` |
|      - | 6323 | `			/* Empty string */` |
|    ! 0 | 6324 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6325 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6326 | `		}` |
|      5 | 6327 | `		if( nArg > 3 ){` |
|      - | 6328 | `			/* Padd type */` |
|      5 | 6329 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6330 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6331 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6332 | `			}` |
|      2 | 6333 | `		}` |
|      2 | 6334 | `	}` |
|      5 | 6335 | `	iDiv = 1;` |
|      5 | 6336 | `	if( iType == 2 ){` |
|    ! 0 | 6337 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6338 | `	}` |
|      - | 6339 | `	/* Perform the requested operation */` |
|      5 | 6340 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6341 | `		jPad = iStrpad;` |
|      5 | 6342 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6343 | `			/* Padding */` |
|      5 | 6344 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6345 | `				break;` |
|      - | 6346 | `			}` |
|      3 | 6347 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 6348 | `		}` |
|      3 | 6349 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6350 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6351 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6352 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6353 | `					jPad = iStrpad;` |
|    ! 0 | 6354 | `				}` |
|      3 | 6355 | `				if( jPad < 1){` |
|    ! 0 | 6356 | `					break;` |
|      - | 6357 | `				}` |
|      3 | 6358 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6359 | `			}` |
|      1 | 6360 | `		}` |
|      1 | 6361 | `	}` |
|      5 | 6362 | `	if( iLen > 0 ){` |
|      - | 6363 | `		/* Append the input string */` |
|      5 | 6364 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 6365 | `	}` |
|      5 | 6366 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6367 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6368 | `			/* Padding */` |
|      5 | 6369 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6370 | `				break;` |
|      - | 6371 | `			}` |
|      3 | 6372 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 6373 | `		}` |
|      5 | 6374 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6375 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6376 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6377 | `				jPad = iStrpad;` |
|    ! 0 | 6378 | `			}` |
|      3 | 6379 | `			if( jPad < 1){` |
|    ! 0 | 6380 | `				break;` |
|      - | 6381 | `			}` |
|      3 | 6382 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 6383 | `		}` |
|      1 | 6384 | `	}` |
|      5 | 6385 | `	return PH7_OK;` |
|      6 | 6386 |  |
|      - | 6387 | `/*` |
|      - | 6388 | ` * String replacement private data.` |
|      - | 6389 | ` */` |
|      - | 6390 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6391 | `struct str_replace_data` |
|      - | 6392 |  |
|      - | 6393 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6394 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6395 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6396 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6397 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6398 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6399 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 6400 | `};` |
|      - | 6401 | `/*` |
|      - | 6402 | ` * Remove a substring.` |
|      - | 6403 | ` */` |
|      - | 6404 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6405 | `	for(;;){\` |
|      - | 6406 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6407 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6408 | `		++OFFT;\` |
|      - | 6409 | `	}\` |
|      - | 6410 |  |
|      - | 6411 | `/*` |
|      - | 6412 | ` * Shift right and insert algorithm.` |
|      - | 6413 | ` */` |
|      - | 6414 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6415 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6416 | `		for(;;){\` |
|      - | 6417 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6418 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6419 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6420 | `			--INLEN; \` |
|      - | 6421 | `		}\` |
|      - | 6422 | `		for(;;){\` |
|      - | 6423 | `				if(ELEN < 1) { break; }\` |
|      - | 6424 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6425 | `				OFFT++;\` |
|      - | 6426 | `				ENTRY++;\` |
|      - | 6427 | `				--ELEN;\` |
|      - | 6428 | `		}\` |
|      - | 6429 |  |
|      - | 6430 | `/*` |
|      - | 6431 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6432 | ` * replacement string [i.e: zReplace].` |
|      - | 6433 | ` */` |
|     38 | 6434 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6435 |  |
|     39 | 6436 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6437 | `	sxu32 n,m;` |
|     39 | 6438 | `	n = SyBlobLength(pWorker);` |
|     39 | 6439 | `	m = nOfft;` |
|      - | 6440 | `	/* Delete the old entry */` |
|    475 | 6441 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6442 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6443 | `	if( nReplen > 0 ){` |
|     33 | 6444 | `		sxi32 iRep = nReplen;` |
|      - | 6445 | `		sxi32 rc;` |
|      - | 6446 | `		/*` |
|      - | 6447 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6448 | `		 * string.` |
|      - | 6449 | `		 */` |
|     33 | 6450 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6451 | `		if( rc != SXRET_OK ){` |
|      - | 6452 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 6453 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 6454 | `			return rc;` |
|      - | 6455 | `		}` |
|      - | 6456 | `		/* Perform the insertion now */` |
|     33 | 6457 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6458 | `		n = SyBlobLength(pWorker);` |
|    163 | 6459 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6460 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6461 | `	}` |
|     39 | 6462 | `	return SXRET_OK;` |
|     20 | 6463 |  |
|      - | 6464 | `/*` |
|      - | 6465 | ` * String replacement walker callback.` |
|      - | 6466 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6467 | ` * the replace string.` |
|      - | 6468 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6469 | ` */` |
|      8 | 6470 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6471 |  |
|      9 | 6472 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6473 | `	const char *zTarget,*zReplace;` |
|      - | 6474 | `	SyBlob *pWorker;` |
|      - | 6475 | `	int tLen,nLen;` |
|      - | 6476 | `	sxu32 nOfft;` |
|      - | 6477 | `	sxi32 rc;` |
|      - | 6478 | `	/* Point to the working buffer */` |
|      9 | 6479 | `	pWorker = pRepData->pWorker;` |
|      9 | 6480 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6481 | `		/* Target and replace must be a string */` |
|      3 | 6482 | `		return PH7_OK;` |
|      - | 6483 | `	}` |
|      - | 6484 | `	/* Extract the target and the replace */` |
|      7 | 6485 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6486 | `	if( tLen < 1 ){` |
|      - | 6487 | `		/* Empty target,return immediately */` |
|    ! 0 | 6488 | `		return PH7_OK;` |
|      - | 6489 | `	}` |
|      - | 6490 | `	/* Perform a pattern search */` |
|      7 | 6491 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6492 | `	if( rc != SXRET_OK ){` |
|      - | 6493 | `		/* Pattern not found */` |
|    ! 0 | 6494 | `		return PH7_OK;` |
|      - | 6495 | `	}` |
|      - | 6496 | `	/* Extract the replace string */` |
|      7 | 6497 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6498 | `	/* Perform the replace process */` |
|      7 | 6499 | `	rc = StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      7 | 6500 | `	if( rc != SXRET_OK ){` |
|      - | 6501 | `		/* Allocation failure: carry it out and stop the walk */` |
|    ! 0 | 6502 | `		pRepData->rc = rc;` |
|    ! 0 | 6503 | `		return rc;` |
|      - | 6504 | `	}` |
|      - | 6505 | `	/* All done */` |
|      7 | 6506 | `	return PH7_OK;` |
|      5 | 6507 |  |
|      - | 6508 | `/*` |
|      - | 6509 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6510 | ` * to collect search/replace string.` |
|      - | 6511 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6512 | ` */` |
|     26 | 6513 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6514 |  |
|     27 | 6515 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6516 | `	SyString sWorker;` |
|      - | 6517 | `	const char *zIn;` |
|      - | 6518 | `	int nByte;` |
|      - | 6519 | `	/* Extract a string representation of the given argument */` |
|     27 | 6520 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6521 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6522 | `	if( nByte > 0 ){` |
|      - | 6523 | `		char *zDup;` |
|      - | 6524 | `		/* Duplicate the chunk */` |
|     25 | 6525 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6526 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6527 | `			);` |
|     25 | 6528 | `		if( zDup == 0 ){` |
|      - | 6529 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 6530 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 6531 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 6532 | `			return SXERR_MEM;` |
|      - | 6533 | `		}` |
|     25 | 6534 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6535 | `		/* Save the chunk */` |
|     25 | 6536 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6537 | `	}` |
|      - | 6538 | `	/* Save for later processing */` |
|     27 | 6539 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6540 | `	/* All done */` |
|     13 | 6541 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6542 | `	return PH7_OK;` |
|     14 | 6543 |  |
|      - | 6544 | `/*` |
|      - | 6545 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6546 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6547 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6548 | ` * Parameters` |
|      - | 6549 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6550 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6551 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6552 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6553 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6554 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6555 | ` * $search` |
|      - | 6556 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6557 | ` *  to designate multiple needles.` |
|      - | 6558 | ` * $replace` |
|      - | 6559 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6560 | ` *  to designate multiple replacements.` |
|      - | 6561 | ` * $subject` |
|      - | 6562 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6563 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6564 | ` *  of subject, and the return value is an array as well.` |
|      - | 6565 | ` * $count (Not used)` |
|      - | 6566 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6567 | ` * Return` |
|      - | 6568 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6569 | ` */` |
|  23282 | 6570 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6571 |  |
|      - | 6572 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6573 | `	ProcStringMatch xMatch;` |
|      - | 6574 | `	const char *zIn,*zFunc;` |
|      - | 6575 | `	str_replace_data sRep;` |
|      - | 6576 | `	SyBlob sWorker;` |
|      - | 6577 | `	SySet sReplace;` |
|      - | 6578 | `	SySet sSearch;` |
|      - | 6579 | `	int rep_str;` |
|      - | 6580 | `	int nByte;` |
|      - | 6581 | `	sxi32 rc;` |
|  23287 | 6582 | `	if( nArg < 3 ){` |
|      - | 6583 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6584 | `		ph7_result_null(pCtx);` |
|      7 | 6585 | `		return PH7_OK;` |
|      - | 6586 | `	}` |
|      - | 6587 | `	/* Initialize fields */` |
|  23281 | 6588 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  23281 | 6589 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  23281 | 6590 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  23281 | 6591 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  23281 | 6592 | `	sRep.pCtx = pCtx;` |
|  23281 | 6593 | `	sRep.pCollector = &sSearch;` |
|  23281 | 6594 | `	rep_str = 0;` |
|      - | 6595 | `	/* Extract the subject */` |
|  23281 | 6596 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  23281 | 6597 | `	if( nByte < 1 ){` |
|      - | 6598 | `		/* Nothing to replace,return the empty string */` |
|     29 | 6599 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 6600 | `		return PH7_OK;` |
|      - | 6601 | `	}` |
|      - | 6602 | `	/* Copy the subject */` |
|  23253 | 6603 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6604 | `	/* Search string */` |
|  23253 | 6605 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6606 | `		/* Collect search string */` |
|      9 | 6607 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6608 | `	}else{` |
|      - | 6609 | `		/* Single pattern */` |
|  23245 | 6610 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  23245 | 6611 | `		if( nByte < 1 ){` |
|      - | 6612 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6613 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6614 | `			return PH7_OK;` |
|      - | 6615 | `		}` |
|  23241 | 6616 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6617 | `		/* Save for later processing */` |
|  23241 | 6618 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6619 | `	}` |
|      - | 6620 | `	/* Replace string */` |
|  23249 | 6621 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6622 | `		/* Collect replace string */` |
|      7 | 6623 | `		sRep.pCollector = &sReplace;` |
|      7 | 6624 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6625 | `	}else{` |
|      - | 6626 | `		/* Single needle */` |
|  23243 | 6627 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  23243 | 6628 | `		rep_str = 1;` |
|  23243 | 6629 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6630 | `		/* Save for later processing */` |
|  23243 | 6631 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6632 | `	}` |
|      - | 6633 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  23249 | 6634 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 6635 | `		SySetRelease(&sSearch);` |
|    ! 0 | 6636 | `		SySetRelease(&sReplace);` |
|    ! 0 | 6637 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 6638 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6639 | `	}` |
|      - | 6640 | `	/* Reset loop cursors */` |
|  23249 | 6641 | `	SySetResetCursor(&sSearch);` |
|  23249 | 6642 | `	SySetResetCursor(&sReplace);` |
|  23249 | 6643 | `	pReplace = pSearch = 0; /* cc warning */` |
|  23249 | 6644 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6645 | `	/* Extract function name */` |
|  23249 | 6646 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6647 | `	/* Set the default pattern match routine */` |
|  23249 | 6648 | `	xMatch = SyBlobSearch;` |
|  23249 | 6649 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6650 | `		/* Case insensitive pattern match */` |
|     11 | 6651 | `		xMatch = iPatternMatch;` |
|      5 | 6652 | `	}` |
|      - | 6653 | `	/* Start the replace process */` |
|  46501 | 6654 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6655 | `		sxu32 nCount,nOfft;` |
|  23257 | 6656 | `		if( pSearch->nByte <  1 ){` |
|      - | 6657 | `			/* Empty string,ignore */` |
|      3 | 6658 | `			continue;` |
|      - | 6659 | `		}` |
|      - | 6660 | `		/* Extract the replace string */` |
|  23255 | 6661 | `		if( rep_str ){` |
|  23245 | 6662 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  11625 | 6663 | `		}else{` |
|     11 | 6664 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6665 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6666 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6667 | `				 */` |
|      3 | 6668 | `				pReplace = 0;` |
|      1 | 6669 | `			}` |
|      - | 6670 | `		}` |
|  23255 | 6671 | `		if( pReplace == 0 ){` |
|      - | 6672 | `			/* Use an empty string instead */` |
|      3 | 6673 | `			pReplace = &sTemp;` |
|      1 | 6674 | `		}` |
|  23255 | 6675 | `		nOfft = nCount = 0;` |
|  11641 | 6676 | `		for(;;){` |
|  23287 | 6677 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6678 | `				break;` |
|      - | 6679 | `			}` |
|      - | 6680 | `			/* Perform a pattern lookup */` |
|  34910 | 6681 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  23270 | 6682 | `				pSearch->nByte,&nOfft);` |
|  23275 | 6683 | `			if( rc != SXRET_OK ){` |
|      - | 6684 | `				/* Pattern not found */` |
|  23243 | 6685 | `				break;` |
|      - | 6686 | `			}` |
|      - | 6687 | `			/* Perform the replace operation */` |
|     33 | 6688 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 6689 | `			if( rc != SXRET_OK ){` |
|      - | 6690 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 6691 | `				SySetRelease(&sSearch);` |
|    ! 0 | 6692 | `				SySetRelease(&sReplace);` |
|    ! 0 | 6693 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 6694 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 6695 | `			}` |
|      - | 6696 | `			/* Increment offset counter */` |
|     33 | 6697 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6698 | `		}` |
|      5 | 6699 | `	}` |
|      - | 6700 | `	/* All done,clean-up the mess left behind */` |
|  23249 | 6701 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  23249 | 6702 | `	SySetRelease(&sSearch);` |
|  23249 | 6703 | `	SySetRelease(&sReplace);` |
|  23249 | 6704 | `	SyBlobRelease(&sWorker);` |
|  23249 | 6705 | `	if( rc != PH7_OK ){` |
|    ! 0 | 6706 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6707 | `	}` |
|  23249 | 6708 | `	return PH7_OK;` |
|  11646 | 6709 |  |
|      - | 6710 | `/*` |
|      - | 6711 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6712 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6713 | ` *  Translate characters or replace substrings.` |
|      - | 6714 | ` * Parameters` |
|      - | 6715 | ` *  $str` |
|      - | 6716 | ` *  The string being translated.` |
|      - | 6717 | ` * $from` |
|      - | 6718 | ` *  The string being translated to to.` |
|      - | 6719 | ` * $to` |
|      - | 6720 | ` *  The string replacing from.` |
|      - | 6721 | ` * $replace_pairs` |
|      - | 6722 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6723 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6724 | ` * Return` |
|      - | 6725 | ` *  The translated string.` |
|      - | 6726 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6727 | ` */` |
|     12 | 6728 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6729 |  |
|      - | 6730 | `	const char *zIn;` |
|      - | 6731 | `	int nLen;` |
|     13 | 6732 | `	if( nArg < 1 ){` |
|      - | 6733 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6734 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6735 | `		return PH7_OK;` |
|      - | 6736 | `	}` |
|      7 | 6737 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6738 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6739 | `		/* Invalid arguments */` |
|    ! 0 | 6740 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6741 | `		return PH7_OK;` |
|      - | 6742 | `	}` |
|      9 | 6743 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6744 | `		str_replace_data sRepData;` |
|      - | 6745 | `		SyBlob sWorker;` |
|      - | 6746 | `		sxi32 rc;` |
|      - | 6747 | `		/* Initilaize the working buffer */` |
|      5 | 6748 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6749 | `		/* Copy raw string */` |
|      5 | 6750 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6751 | `		/* Init our replace data instance */` |
|      5 | 6752 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6753 | `		sRepData.xMatch = SyBlobSearch;` |
|      5 | 6754 | `		sRepData.rc = SXRET_OK;` |
|      - | 6755 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6756 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      5 | 6757 | `		if( sRepData.rc != SXRET_OK ){` |
|      - | 6758 | `			/* Allocation failure during replacement: surface a fatal */` |
|    ! 0 | 6759 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 6760 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6761 | `		}` |
|      - | 6762 | `		/* All done, return the result string */` |
|      7 | 6763 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6764 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6765 | `		/* Clean-up */` |
|      5 | 6766 | `		SyBlobRelease(&sWorker);` |
|      5 | 6767 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6768 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6769 | `		}` |
|      3 | 6770 | `	}else{` |
|      - | 6771 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6772 | `		const char *zFrom,*zTo;` |
|      3 | 6773 | `		if( nArg < 3 ){` |
|      - | 6774 | `			/* Nothing to replace */` |
|    ! 0 | 6775 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6776 | `			return PH7_OK;` |
|      - | 6777 | `		}` |
|      - | 6778 | `		/* Extract given arguments */` |
|      3 | 6779 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6780 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6781 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6782 | `			/* Nothing to replace */` |
|    ! 0 | 6783 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6784 | `			return PH7_OK;` |
|      - | 6785 | `		}` |
|      - | 6786 | `		/* Start the replace process */` |
|     13 | 6787 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6788 | `			c = zIn[i];` |
|     11 | 6789 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6790 | `				if ( iOfft < tlen ){` |
|      5 | 6791 | `					c = zTo[iOfft];` |
|      2 | 6792 | `				}` |
|      2 | 6793 | `			}` |
|     11 | 6794 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6795 |  |
|      6 | 6796 | `		}` |
|      - | 6797 | `	}` |
|      7 | 6798 | `	return PH7_OK;` |
|      7 | 6799 |  |
|      - | 6800 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6801 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6802 | `/*` |
|      - | 6803 | ` * Parse an INI string.` |
|      - | 6804 |  |
|      - | 6805 | ` * According to wikipedia` |
|      - | 6806 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6807 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6808 | ` *  Format` |
|      - | 6809 | `*    Properties` |
|      - | 6810 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6811 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6812 | `*     Example:` |
|      - | 6813 | `*      name=value` |
|      - | 6814 | `*    Sections` |
|      - | 6815 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6816 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6817 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6818 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6819 | `*     Example:` |
|      - | 6820 | `*      [section]` |
|      - | 6821 | `*   Comments` |
|      - | 6822 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6823 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6824 | `*/` |
|     12 | 6825 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6826 |  |
|      - | 6827 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6828 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6829 | `	SyHashEntry *pEntry;` |
|      - | 6830 | `	SyString sEntry;` |
|      - | 6831 | `	SyHash sHash;` |
|      - | 6832 | `	int c;` |
|      - | 6833 | `	/* Create an empty array and worker variables */` |
|     13 | 6834 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6835 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6836 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6837 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6838 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 6839 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6840 | `	}` |
|     13 | 6841 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6842 | `	pCur = pArray;` |
|      - | 6843 | `	/* Start the parse process */` |
|     21 | 6844 | `	for(;;){` |
|      - | 6845 | `		/* Ignore leading white spaces */` |
|     69 | 6846 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6847 | `			zIn++;` |
|      1 | 6848 | `		}` |
|     43 | 6849 | `		if( zIn >= zEnd ){` |
|      - | 6850 | `			/* No more input to process */` |
|     13 | 6851 | `			break;` |
|      - | 6852 | `		}` |
|     31 | 6853 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6854 | `			/* Comment til the end of line */` |
|    ! 0 | 6855 | `			zIn++;` |
|    ! 0 | 6856 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6857 | `				zIn++;` |
|    ! 0 | 6858 | `			}` |
|    ! 0 | 6859 | `			continue;` |
|      - | 6860 | `		}` |
|      - | 6861 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6862 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6863 | `		if( zIn[0] == '[' ){` |
|      - | 6864 | `			/* Section: Extract the section name */` |
|      9 | 6865 | `			zIn++;` |
|      9 | 6866 | `			zCur = zIn;` |
|     73 | 6867 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6868 | `				zIn++;` |
|      1 | 6869 | `			}` |
|      9 | 6870 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6871 | `				/* Save the section name */` |
|      5 | 6872 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6873 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6874 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6875 | `				if( sEntry.nByte > 0 ){` |
|      - | 6876 | `					/* Associate an array with the section */` |
|      5 | 6877 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6878 | `					if( pSection ){` |
|      5 | 6879 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6880 | `						pCur = pSection;` |
|      2 | 6881 | `					}` |
|      2 | 6882 | `				}` |
|      2 | 6883 | `			}` |
|      9 | 6884 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6885 | `		}else{` |
|      - | 6886 | `			ph7_value *pOldCur;` |
|      - | 6887 | `			int is_array;` |
|      - | 6888 | `			int iLen;` |
|      - | 6889 | `			/* Properties */` |
|     23 | 6890 | `			is_array = 0;` |
|     23 | 6891 | `			zCur = zIn;` |
|     23 | 6892 | `			iLen = 0; /* cc warning */` |
|     23 | 6893 | `			pOldCur = pCur;` |
|    155 | 6894 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6895 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6896 | `					/* Array */` |
|    ! 0 | 6897 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6898 | `					is_array = 1;` |
|    ! 0 | 6899 | `					if( iLen > 0 ){` |
|    ! 0 | 6900 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6901 | `						/* Query the hashtable */` |
|    ! 0 | 6902 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6903 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6904 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6905 | `						if( pEntry ){` |
|    ! 0 | 6906 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6907 | `						}else{` |
|      - | 6908 | `							/* Create an empty array */` |
|    ! 0 | 6909 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6910 | `							if( pvArr ){` |
|      - | 6911 | `								/* Save the entry */` |
|    ! 0 | 6912 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6913 | `								/* Insert the entry */` |
|    ! 0 | 6914 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6915 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6916 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6917 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6918 | `							}` |
|      - | 6919 | `						}` |
|    ! 0 | 6920 | `						if( pvArr ){` |
|    ! 0 | 6921 | `							pCur = pvArr;` |
|    ! 0 | 6922 | `						}` |
|    ! 0 | 6923 | `					}` |
|    ! 0 | 6924 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6925 | `						zIn++;` |
|    ! 0 | 6926 | `					}` |
|    ! 0 | 6927 | `				}` |
|    133 | 6928 | `				zIn++;` |
|      1 | 6929 | `			}` |
|     23 | 6930 | `			if( !is_array ){` |
|     23 | 6931 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6932 | `			}` |
|      - | 6933 | `			/* Trim the key */` |
|     23 | 6934 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6935 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6936 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6937 | `				if( !is_array ){` |
|      - | 6938 | `					/* Save the key name */` |
|     23 | 6939 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6940 | `				}` |
|      - | 6941 | `				/* extract key value */` |
|     23 | 6942 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6943 | `				zIn++; /* '=' */` |
|     39 | 6944 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6945 | `					zIn++;` |
|      1 | 6946 | `				}` |
|     23 | 6947 | `				if( zIn < zEnd ){` |
|     21 | 6948 | `					zCur = zIn;` |
|     21 | 6949 | `					c = zIn[0];` |
|     21 | 6950 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6951 | `						zIn++;` |
|      - | 6952 | `						/* Delimit the value */` |
|    ! 0 | 6953 | `						while( zIn < zEnd ){` |
|    ! 0 | 6954 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6955 | `								break;` |
|      - | 6956 | `							}` |
|    ! 0 | 6957 | `							zIn++;` |
|    ! 0 | 6958 | `						}` |
|    ! 0 | 6959 | `						if( zIn < zEnd ){` |
|    ! 0 | 6960 | `							zIn++;` |
|    ! 0 | 6961 | `						}` |
|    ! 0 | 6962 | `					}else{` |
|    125 | 6963 | `						while( zIn < zEnd ){` |
|    123 | 6964 | `							if( zIn[0] == '\n' ){` |
|     19 | 6965 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6966 | `									break;` |
|    ! 0 | 6967 | `								}` |
|    105 | 6968 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6969 | `								/* Inline comments */` |
|    ! 0 | 6970 | `								break;` |
|      - | 6971 | `							}` |
|    105 | 6972 | `							zIn++;` |
|      1 | 6973 | `						}` |
|      - | 6974 | `					}` |
|      - | 6975 | `					/* Trim the value */` |
|     21 | 6976 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6977 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6978 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6979 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6980 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6981 | `					}` |
|     21 | 6982 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6983 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6984 | `					}` |
|      - | 6985 | `					/* Insert the key and it's value */` |
|     21 | 6986 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6987 | `				}` |
|     12 | 6988 | `			}else{` |
|    ! 0 | 6989 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6990 | `					zIn++;` |
|    ! 0 | 6991 | `				}` |
|      - | 6992 | `			}` |
|     23 | 6993 | `			pCur = pOldCur;` |
|      - | 6994 | `		}` |
|      1 | 6995 | `	}` |
|     13 | 6996 | `	SyHashRelease(&sHash);` |
|      - | 6997 | `	/* Return the parse of the INI string */` |
|     13 | 6998 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6999 | `	return SXRET_OK;` |
|      7 | 7000 |  |
|      - | 7001 | `/*` |
|      - | 7002 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7003 | ` *  Parse a configuration string.` |
|      - | 7004 | ` * Parameters` |
|      - | 7005 | ` *  $ini` |
|      - | 7006 | ` *   The contents of the ini file being parsed.` |
|      - | 7007 | ` *  $process_sections` |
|      - | 7008 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7009 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7010 | ` *  $scanner_mode (Not used)` |
|      - | 7011 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7012 | ` *   then option values will not be parsed.` |
|      - | 7013 | ` * Return` |
|      - | 7014 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7015 | ` */` |
|     10 | 7016 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7017 |  |
|      - | 7018 | `	const char *zIni;` |
|      - | 7019 | `	int nByte;` |
|     11 | 7020 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7021 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7022 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7023 | `		return PH7_OK;` |
|      - | 7024 | `	}` |
|      - | 7025 | `	/* Extract the raw INI buffer */` |
|     11 | 7026 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7027 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7028 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7029 |  |
|      - | 7030 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7031 |  |
|      - | 7032 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7033 |  |
|      - | 7034 | `/*` |
|      - | 7035 | ` * Ctype Functions.` |
|      - | 7036 | ` * Status:` |
|      - | 7037 | ` *    Stable.` |
|      - | 7038 | ` */` |
|      - | 7039 | `/*` |
|      - | 7040 | ` * bool ctype_alnum(string $text)` |
|      - | 7041 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7042 | ` * Parameters` |
|      - | 7043 | ` *  $text` |
|      - | 7044 | ` *   The tested string.` |
|      - | 7045 | ` * Return` |
|      - | 7046 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7047 | ` */` |
|     16 | 7048 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7049 |  |
|      - | 7050 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7051 | `	int nLen;` |
|     17 | 7052 | `	if( nArg < 1 ){` |
|      - | 7053 | `		/* Missing arguments,return FALSE */` |
|      3 | 7054 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7055 | `		return PH7_OK;` |
|      - | 7056 | `	}` |
|      - | 7057 | `	/* Extract the target string */` |
|     15 | 7058 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7059 | `	zEnd = &zIn[nLen];` |
|     15 | 7060 | `	if( nLen < 1 ){` |
|      - | 7061 | `		/* Empty string,return FALSE */` |
|      3 | 7062 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7063 | `		return PH7_OK;` |
|      - | 7064 | `	}` |
|      - | 7065 | `	/* Perform the requested operation */` |
|     32 | 7066 | `	for(;;){` |
|     65 | 7067 | `		if( zIn >= zEnd ){` |
|      - | 7068 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7069 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7070 | `			return PH7_OK;` |
|      - | 7071 | `		}` |
|     57 | 7072 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7073 | `			break;` |
|      - | 7074 | `		}` |
|      - | 7075 | `		/* Point to the next character */` |
|     53 | 7076 | `		zIn++;` |
|      1 | 7077 | `	}` |
|      - | 7078 | `	/* The test failed,return FALSE */` |
|      5 | 7079 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7080 | `	return PH7_OK;` |
|      9 | 7081 |  |
|      - | 7082 | `/*` |
|      - | 7083 | ` * bool ctype_alpha(string $text)` |
|      - | 7084 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7085 | ` * Parameters` |
|      - | 7086 | ` *  $text` |
|      - | 7087 | ` *   The tested string.` |
|      - | 7088 | ` * Return` |
|      - | 7089 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7090 | ` */` |
|     18 | 7091 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|     42 | 7109 | `	for(;;){` |
|     85 | 7110 | `		if( zIn >= zEnd ){` |
|      - | 7111 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7112 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7113 | `			return PH7_OK;` |
|      - | 7114 | `		}` |
|     77 | 7115 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7116 | `			break;` |
|      - | 7117 | `		}` |
|      - | 7118 | `		/* Point to the next character */` |
|     71 | 7119 | `		zIn++;` |
|      1 | 7120 | `	}` |
|      - | 7121 | `	/* The test failed,return FALSE */` |
|      7 | 7122 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7123 | `	return PH7_OK;` |
|     10 | 7124 |  |
|      - | 7125 | `/*` |
|      - | 7126 | ` * bool ctype_cntrl(string $text)` |
|      - | 7127 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7128 | ` * Parameters` |
|      - | 7129 | ` *  $text` |
|      - | 7130 | ` *   The tested string.` |
|      - | 7131 | ` * Return` |
|      - | 7132 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7133 | ` */` |
|     18 | 7134 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|     14 | 7152 | `	for(;;){` |
|     29 | 7153 | `		if( zIn >= zEnd ){` |
|      - | 7154 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7155 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7156 | `			return PH7_OK;` |
|      - | 7157 | `		}` |
|     21 | 7158 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7159 | `			/* UTF-8 stream  */` |
|    ! 0 | 7160 | `			break;` |
|      - | 7161 | `		}` |
|     21 | 7162 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7163 | `			break;` |
|      - | 7164 | `		}` |
|      - | 7165 | `		/* Point to the next character */` |
|     15 | 7166 | `		zIn++;` |
|      1 | 7167 | `	}` |
|      - | 7168 | `	/* The test failed,return FALSE */` |
|      7 | 7169 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7170 | `	return PH7_OK;` |
|     10 | 7171 |  |
|      - | 7172 | `/*` |
|      - | 7173 | ` * bool ctype_digit(string $text)` |
|      - | 7174 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7175 | ` * Parameters` |
|      - | 7176 | ` *  $text` |
|      - | 7177 | ` *   The tested string.` |
|      - | 7178 | ` * Return` |
|      - | 7179 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7180 | ` */` |
|   1621 | 7181 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7182 |  |
|      - | 7183 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7184 | `	int nLen;` |
|   1626 | 7185 | `	if( nArg < 1 ){` |
|      - | 7186 | `		/* Missing arguments,return FALSE */` |
|      3 | 7187 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7188 | `		return PH7_OK;` |
|      - | 7189 | `	}` |
|      - | 7190 | `	/* Extract the target string */` |
|   1624 | 7191 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1624 | 7192 | `	zEnd = &zIn[nLen];` |
|   1624 | 7193 | `	if( nLen < 1 ){` |
|      - | 7194 | `		/* Empty string,return FALSE */` |
|      3 | 7195 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7196 | `		return PH7_OK;` |
|      - | 7197 | `	}` |
|      - | 7198 | `	/* Perform the requested operation */` |
|   1523 | 7199 | `	for(;;){` |
|   3049 | 7200 | `		if( zIn >= zEnd ){` |
|      - | 7201 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1382 | 7202 | `			ph7_result_bool(pCtx,1);` |
|   1382 | 7203 | `			return PH7_OK;` |
|      - | 7204 | `		}` |
|   1672 | 7205 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7206 | `			/* UTF-8 stream  */` |
|    ! 0 | 7207 | `			break;` |
|      - | 7208 | `		}` |
|   1672 | 7209 | `		if( !SyisDigit(zIn[0]) ){` |
|    245 | 7210 | `			break;` |
|      - | 7211 | `		}` |
|      - | 7212 | `		/* Point to the next character */` |
|   1432 | 7213 | `		zIn++;` |
|      5 | 7214 | `	}` |
|      - | 7215 | `	/* The test failed,return FALSE */` |
|    245 | 7216 | `	ph7_result_bool(pCtx,0);` |
|    245 | 7217 | `	return PH7_OK;` |
|    816 | 7218 |  |
|      - | 7219 | `/*` |
|      - | 7220 | ` * bool ctype_xdigit(string $text)` |
|      - | 7221 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7222 | ` * Parameters` |
|      - | 7223 | ` *  $text` |
|      - | 7224 | ` *   The tested string.` |
|      - | 7225 | ` * Return` |
|      - | 7226 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7227 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7228 | ` */` |
|     20 | 7229 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7230 |  |
|      - | 7231 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7232 | `	int nLen;` |
|     21 | 7233 | `	if( nArg < 1 ){` |
|      - | 7234 | `		/* Missing arguments,return FALSE */` |
|      3 | 7235 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7236 | `		return PH7_OK;` |
|      - | 7237 | `	}` |
|      - | 7238 | `	/* Extract the target string */` |
|     19 | 7239 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7240 | `	zEnd = &zIn[nLen];` |
|     19 | 7241 | `	if( nLen < 1 ){` |
|      - | 7242 | `		/* Empty string,return FALSE */` |
|      3 | 7243 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7244 | `		return PH7_OK;` |
|      - | 7245 | `	}` |
|      - | 7246 | `	/* Perform the requested operation */` |
|     46 | 7247 | `	for(;;){` |
|     93 | 7248 | `		if( zIn >= zEnd ){` |
|      - | 7249 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7250 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7251 | `			return PH7_OK;` |
|      - | 7252 | `		}` |
|     83 | 7253 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7254 | `			/* UTF-8 stream  */` |
|    ! 0 | 7255 | `			break;` |
|      - | 7256 | `		}` |
|     83 | 7257 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7258 | `			break;` |
|      - | 7259 | `		}` |
|      - | 7260 | `		/* Point to the next character */` |
|     77 | 7261 | `		zIn++;` |
|      1 | 7262 | `	}` |
|      - | 7263 | `	/* The test failed,return FALSE */` |
|      7 | 7264 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7265 | `	return PH7_OK;` |
|     11 | 7266 |  |
|      - | 7267 | `/*` |
|      - | 7268 | ` * bool ctype_graph(string $text)` |
|      - | 7269 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7270 | ` * Parameters` |
|      - | 7271 | ` *  $text` |
|      - | 7272 | ` *   The tested string.` |
|      - | 7273 | ` * Return` |
|      - | 7274 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7275 | ` * (no white space), FALSE otherwise.` |
|      - | 7276 | ` */` |
|     18 | 7277 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7278 |  |
|      - | 7279 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7280 | `	int nLen;` |
|     19 | 7281 | `	if( nArg < 1 ){` |
|      - | 7282 | `		/* Missing arguments,return FALSE */` |
|      3 | 7283 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7284 | `		return PH7_OK;` |
|      - | 7285 | `	}` |
|      - | 7286 | `	/* Extract the target string */` |
|     17 | 7287 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7288 | `	zEnd = &zIn[nLen];` |
|     17 | 7289 | `	if( nLen < 1 ){` |
|      - | 7290 | `		/* Empty string,return FALSE */` |
|      3 | 7291 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7292 | `		return PH7_OK;` |
|      - | 7293 | `	}` |
|      - | 7294 | `	/* Perform the requested operation */` |
|     57 | 7295 | `	for(;;){` |
|    115 | 7296 | `		if( zIn >= zEnd ){` |
|      - | 7297 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7298 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7299 | `			return PH7_OK;` |
|      - | 7300 | `		}` |
|    107 | 7301 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7302 | `			/* UTF-8 stream  */` |
|    ! 0 | 7303 | `			break;` |
|      - | 7304 | `		}` |
|    107 | 7305 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7306 | `			break;` |
|      - | 7307 | `		}` |
|      - | 7308 | `		/* Point to the next character */` |
|    101 | 7309 | `		zIn++;` |
|      1 | 7310 | `	}` |
|      - | 7311 | `	/* The test failed,return FALSE */` |
|      7 | 7312 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7313 | `	return PH7_OK;` |
|     10 | 7314 |  |
|      - | 7315 | `/*` |
|      - | 7316 | ` * bool ctype_print(string $text)` |
|      - | 7317 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7318 | ` * Parameters` |
|      - | 7319 | ` *  $text` |
|      - | 7320 | ` *   The tested string.` |
|      - | 7321 | ` * Return` |
|      - | 7322 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7323 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7324 | ` *  or control function at all.` |
|      - | 7325 | ` */` |
|     18 | 7326 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7327 |  |
|      - | 7328 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7329 | `	int nLen;` |
|     19 | 7330 | `	if( nArg < 1 ){` |
|      - | 7331 | `		/* Missing arguments,return FALSE */` |
|      3 | 7332 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7333 | `		return PH7_OK;` |
|      - | 7334 | `	}` |
|      - | 7335 | `	/* Extract the target string */` |
|     17 | 7336 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7337 | `	zEnd = &zIn[nLen];` |
|     17 | 7338 | `	if( nLen < 1 ){` |
|      - | 7339 | `		/* Empty string,return FALSE */` |
|      3 | 7340 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7341 | `		return PH7_OK;` |
|      - | 7342 | `	}` |
|      - | 7343 | `	/* Perform the requested operation */` |
|     63 | 7344 | `	for(;;){` |
|    127 | 7345 | `		if( zIn >= zEnd ){` |
|      - | 7346 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7347 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7348 | `			return PH7_OK;` |
|      - | 7349 | `		}` |
|    119 | 7350 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7351 | `			/* UTF-8 stream  */` |
|    ! 0 | 7352 | `			break;` |
|      - | 7353 | `		}` |
|    119 | 7354 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7355 | `			break;` |
|      - | 7356 | `		}` |
|      - | 7357 | `		/* Point to the next character */` |
|    113 | 7358 | `		zIn++;` |
|      1 | 7359 | `	}` |
|      - | 7360 | `	/* The test failed,return FALSE */` |
|      7 | 7361 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7362 | `	return PH7_OK;` |
|     10 | 7363 |  |
|      - | 7364 | `/*` |
|      - | 7365 | ` * bool ctype_punct(string $text)` |
|      - | 7366 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7367 | ` * Parameters` |
|      - | 7368 | ` *  $text` |
|      - | 7369 | ` *   The tested string.` |
|      - | 7370 | ` * Return` |
|      - | 7371 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7372 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7373 | ` */` |
|     20 | 7374 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7375 |  |
|      - | 7376 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7377 | `	int nLen;` |
|     21 | 7378 | `	if( nArg < 1 ){` |
|      - | 7379 | `		/* Missing arguments,return FALSE */` |
|      3 | 7380 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7381 | `		return PH7_OK;` |
|      - | 7382 | `	}` |
|      - | 7383 | `	/* Extract the target string */` |
|     19 | 7384 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7385 | `	zEnd = &zIn[nLen];` |
|     19 | 7386 | `	if( nLen < 1 ){` |
|      - | 7387 | `		/* Empty string,return FALSE */` |
|      3 | 7388 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7389 | `		return PH7_OK;` |
|      - | 7390 | `	}` |
|      - | 7391 | `	/* Perform the requested operation */` |
|     38 | 7392 | `	for(;;){` |
|     77 | 7393 | `		if( zIn >= zEnd ){` |
|      - | 7394 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7395 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7396 | `			return PH7_OK;` |
|      - | 7397 | `		}` |
|     69 | 7398 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7399 | `			/* UTF-8 stream  */` |
|    ! 0 | 7400 | `			break;` |
|      - | 7401 | `		}` |
|     69 | 7402 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7403 | `			break;` |
|      - | 7404 | `		}` |
|      - | 7405 | `		/* Point to the next character */` |
|     61 | 7406 | `		zIn++;` |
|      1 | 7407 | `	}` |
|      - | 7408 | `	/* The test failed,return FALSE */` |
|      9 | 7409 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7410 | `	return PH7_OK;` |
|     11 | 7411 |  |
|      - | 7412 | `/*` |
|      - | 7413 | ` * bool ctype_space(string $text)` |
|      - | 7414 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7415 | ` * Parameters` |
|      - | 7416 | ` *  $text` |
|      - | 7417 | ` *   The tested string.` |
|      - | 7418 | ` * Return` |
|      - | 7419 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7420 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7421 | ` *  and form feed characters.` |
|      - | 7422 | ` */` |
|  60182 | 7423 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7424 |  |
|      - | 7425 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7426 | `	int nLen;` |
|  60187 | 7427 | `	if( nArg < 1 ){` |
|      - | 7428 | `		/* Missing arguments,return FALSE */` |
|      3 | 7429 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7430 | `		return PH7_OK;` |
|      - | 7431 | `	}` |
|      - | 7432 | `	/* Extract the target string */` |
|  60185 | 7433 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  60185 | 7434 | `	zEnd = &zIn[nLen];` |
|  60185 | 7435 | `	if( nLen < 1 ){` |
|      - | 7436 | `		/* Empty string,return FALSE */` |
|      3 | 7437 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7438 | `		return PH7_OK;` |
|      - | 7439 | `	}` |
|      - | 7440 | `	/* Perform the requested operation */` |
|  31172 | 7441 | `	for(;;){` |
|  62263 | 7442 | `		if( zIn >= zEnd ){` |
|      - | 7443 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2061 | 7444 | `			ph7_result_bool(pCtx,1);` |
|   2061 | 7445 | `			return PH7_OK;` |
|      - | 7446 | `		}` |
|  60207 | 7447 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7448 | `			/* UTF-8 stream  */` |
|    ! 0 | 7449 | `			break;` |
|      - | 7450 | `		}` |
|  60207 | 7451 | `		if( !SyisSpace(zIn[0]) ){` |
|  58127 | 7452 | `			break;` |
|      - | 7453 | `		}` |
|      - | 7454 | `		/* Point to the next character */` |
|   2085 | 7455 | `		zIn++;` |
|      5 | 7456 | `	}` |
|      - | 7457 | `	/* The test failed,return FALSE */` |
|  58127 | 7458 | `	ph7_result_bool(pCtx,0);` |
|  58127 | 7459 | `	return PH7_OK;` |
|  30139 | 7460 |  |
|      - | 7461 | `/*` |
|      - | 7462 | ` * bool ctype_lower(string $text)` |
|      - | 7463 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7464 | ` * Parameters` |
|      - | 7465 | ` *  $text` |
|      - | 7466 | ` *   The tested string.` |
|      - | 7467 | ` * Return` |
|      - | 7468 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7469 | ` */` |
|     18 | 7470 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7471 |  |
|      - | 7472 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7473 | `	int nLen;` |
|     19 | 7474 | `	if( nArg < 1 ){` |
|      - | 7475 | `		/* Missing arguments,return FALSE */` |
|      3 | 7476 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7477 | `		return PH7_OK;` |
|      - | 7478 | `	}` |
|      - | 7479 | `	/* Extract the target string */` |
|     17 | 7480 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7481 | `	zEnd = &zIn[nLen];` |
|     17 | 7482 | `	if( nLen < 1 ){` |
|      - | 7483 | `		/* Empty string,return FALSE */` |
|      3 | 7484 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7485 | `		return PH7_OK;` |
|      - | 7486 | `	}` |
|      - | 7487 | `	/* Perform the requested operation */` |
|     27 | 7488 | `	for(;;){` |
|     55 | 7489 | `		if( zIn >= zEnd ){` |
|      - | 7490 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7491 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7492 | `			return PH7_OK;` |
|      - | 7493 | `		}` |
|     51 | 7494 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7495 | `			break;` |
|      - | 7496 | `		}` |
|      - | 7497 | `		/* Point to the next character */` |
|     41 | 7498 | `		zIn++;` |
|      1 | 7499 | `	}` |
|      - | 7500 | `	/* The test failed,return FALSE */` |
|     11 | 7501 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7502 | `	return PH7_OK;` |
|     10 | 7503 |  |
|      - | 7504 | `/*` |
|      - | 7505 | ` * bool ctype_upper(string $text)` |
|      - | 7506 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7507 | ` * Parameters` |
|      - | 7508 | ` *  $text` |
|      - | 7509 | ` *   The tested string.` |
|      - | 7510 | ` * Return` |
|      - | 7511 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7512 | ` */` |
|     18 | 7513 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7514 |  |
|      - | 7515 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7516 | `	int nLen;` |
|     19 | 7517 | `	if( nArg < 1 ){` |
|      - | 7518 | `		/* Missing arguments,return FALSE */` |
|      3 | 7519 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7520 | `		return PH7_OK;` |
|      - | 7521 | `	}` |
|      - | 7522 | `	/* Extract the target string */` |
|     17 | 7523 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7524 | `	zEnd = &zIn[nLen];` |
|     17 | 7525 | `	if( nLen < 1 ){` |
|      - | 7526 | `		/* Empty string,return FALSE */` |
|      3 | 7527 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7528 | `		return PH7_OK;` |
|      - | 7529 | `	}` |
|      - | 7530 | `	/* Perform the requested operation */` |
|     28 | 7531 | `	for(;;){` |
|     57 | 7532 | `		if( zIn >= zEnd ){` |
|      - | 7533 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7534 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7535 | `			return PH7_OK;` |
|      - | 7536 | `		}` |
|     53 | 7537 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7538 | `			break;` |
|      - | 7539 | `		}` |
|      - | 7540 | `		/* Point to the next character */` |
|     43 | 7541 | `		zIn++;` |
|      1 | 7542 | `	}` |
|      - | 7543 | `	/* The test failed,return FALSE */` |
|     11 | 7544 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7545 | `	return PH7_OK;` |
|     10 | 7546 |  |
|      - | 7547 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 7548 | `/*` |
|      - | 7549 | ` * Section:` |
|      - | 7550 | ` *    URL handling Functions.` |
|      - | 7551 | ` * Status:` |
|      - | 7552 | ` *    Stable.` |
|      - | 7553 | ` */` |
|      - | 7554 | `/*` |
|      - | 7555 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 7556 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 7557 | ` */` |
|   1026 | 7558 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7559 |  |
|      - | 7560 | `	/* Store in the call context result buffer */` |
|   1028 | 7561 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 7562 | `	return SXRET_OK;` |
|      2 | 7563 |  |
|      - | 7564 | `/*` |
|      - | 7565 | ` * string base64_encode(string $data)` |
|      - | 7566 | ` * string convert_uuencode(string $data)` |
|      - | 7567 | ` *  Encodes data with MIME base64` |
|      - | 7568 | ` * Parameter` |
|      - | 7569 | ` *  $data` |
|      - | 7570 | ` *    Data to encode` |
|      - | 7571 | ` * Return` |
|      - | 7572 | ` *  Encoded data or FALSE on failure.` |
|      - | 7573 | ` */` |
|     10 | 7574 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7575 |  |
|      - | 7576 | `	const char *zIn;` |
|      - | 7577 | `	int nLen;` |
|     11 | 7578 | `	if( nArg < 1 ){` |
|      - | 7579 | `		/* Missing arguments,return FALSE */` |
|      5 | 7580 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7581 | `		return PH7_OK;` |
|      - | 7582 | `	}` |
|      - | 7583 | `	/* Extract the input string */` |
|      7 | 7584 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7585 | `	if( nLen < 1 ){` |
|      - | 7586 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7587 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7588 | `		return PH7_OK;` |
|      - | 7589 | `	}` |
|      - | 7590 | `	/* Perform the BASE64 encoding */` |
|      7 | 7591 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 7592 | `	return PH7_OK;` |
|      6 | 7593 |  |
|      - | 7594 | `/*` |
|      - | 7595 | ` * string base64_decode(string $data)` |
|      - | 7596 | ` * string convert_uudecode(string $data)` |
|      - | 7597 | ` *  Decodes data encoded with MIME base64` |
|      - | 7598 | ` * Parameter` |
|      - | 7599 | ` *  $data` |
|      - | 7600 | ` *    Encoded data.` |
|      - | 7601 | ` * Return` |
|      - | 7602 | ` *  Returns the original data or FALSE on failure.` |
|      - | 7603 | ` */` |
|     36 | 7604 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7605 |  |
|      - | 7606 | `	const char *zIn;` |
|      - | 7607 | `	int nLen;` |
|     38 | 7608 | `	if( nArg < 1 ){` |
|      - | 7609 | `		/* Missing arguments,return FALSE */` |
|      3 | 7610 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7611 | `		return PH7_OK;` |
|      - | 7612 | `	}` |
|      - | 7613 | `	/* Extract the input string */` |
|     36 | 7614 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 7615 | `	if( nLen < 1 ){` |
|      - | 7616 | `		/* Nothing to process,return FALSE */` |
|      3 | 7617 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7618 | `		return PH7_OK;` |
|      - | 7619 | `	}` |
|      - | 7620 | `	/* Perform the BASE64 decoding */` |
|     34 | 7621 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 7622 | `	return PH7_OK;` |
|     20 | 7623 |  |
|      - | 7624 | `/*` |
|      - | 7625 | ` * string urlencode(string $str)` |
|      - | 7626 | ` *  URL encoding` |
|      - | 7627 | ` * Parameter` |
|      - | 7628 | ` *  $data` |
|      - | 7629 | ` *   Input string.` |
|      - | 7630 | ` * Return` |
|      - | 7631 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 7632 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 7633 | ` *  encoded as plus (+) signs.` |
|      - | 7634 | ` */` |
|      6 | 7635 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7636 |  |
|      - | 7637 | `	const char *zIn;` |
|      - | 7638 | `	int nLen;` |
|      7 | 7639 | `	if( nArg < 1 ){` |
|      - | 7640 | `		/* Missing arguments,return FALSE */` |
|      3 | 7641 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7642 | `		return PH7_OK;` |
|      - | 7643 | `	}` |
|      - | 7644 | `	/* Extract the input string */` |
|      5 | 7645 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 7646 | `	if( nLen < 1 ){` |
|      - | 7647 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7648 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7649 | `		return PH7_OK;` |
|      - | 7650 | `	}` |
|      - | 7651 | `	/* Perform the URL encoding */` |
|      5 | 7652 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 7653 | `	return PH7_OK;` |
|      4 | 7654 |  |
|      - | 7655 | `/*` |
|      - | 7656 | ` * string urldecode(string $str)` |
|      - | 7657 | ` *  Decodes any %## encoding in the given string.` |
|      - | 7658 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 7659 | ` * Parameter` |
|      - | 7660 | ` *  $data` |
|      - | 7661 | ` *    Input string.` |
|      - | 7662 | ` * Return` |
|      - | 7663 | ` *  Decoded URL or FALSE on failure.` |
|      - | 7664 | ` */` |
|      8 | 7665 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7666 |  |
|      - | 7667 | `	const char *zIn;` |
|      - | 7668 | `	int nLen;` |
|      9 | 7669 | `	if( nArg < 1 ){` |
|      - | 7670 | `		/* Missing arguments,return FALSE */` |
|      3 | 7671 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7672 | `		return PH7_OK;` |
|      - | 7673 | `	}` |
|      - | 7674 | `	/* Extract the input string */` |
|      7 | 7675 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7676 | `	if( nLen < 1 ){` |
|      - | 7677 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7678 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7679 | `		return PH7_OK;` |
|      - | 7680 | `	}` |
|      - | 7681 | `	/* Perform the URL decoding */` |
|      7 | 7682 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 7683 | `	return PH7_OK;` |
|      5 | 7684 |  |
|      - | 7685 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7686 | `/* Table of the built-in functions */` |
|      - | 7687 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 7688 | `	   /* Variable handling functions */` |
|      - | 7689 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 7690 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 7691 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 7692 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 7693 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 7694 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 7695 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 7696 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 7697 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 7698 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 7699 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 7700 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 7701 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 7702 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 7703 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 7704 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 7705 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 7706 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 7707 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 7708 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 7709 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7710 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 7711 | `	   /* Math functions */` |
|      - | 7712 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 7713 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 7714 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 7715 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 7716 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 7717 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 7718 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 7719 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 7720 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 7721 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 7722 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 7723 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 7724 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 7725 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 7726 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 7727 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 7728 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 7729 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 7730 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 7731 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 7732 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 7733 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 7734 | `	{ "round",    PH7_builtin_round        },` |
|      - | 7735 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 7736 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 7737 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 7738 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 7739 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 7740 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 7741 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 7742 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 7743 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 7744 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7745 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7746 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 7747 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7748 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7749 | `	   /* String handling functions */` |
|      - | 7750 |  |
|      - | 7751 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 7752 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 7753 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 7754 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 7755 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 7756 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 7757 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 7758 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 7759 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 7760 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 7761 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 7762 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 7763 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 7764 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 7765 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 7766 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 7767 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 7768 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 7769 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 7770 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 7771 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 7772 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 7773 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 7774 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 7775 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 7776 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 7777 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 7778 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 7779 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 7780 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 7781 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 7782 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 7783 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 7784 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 7785 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 7786 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 7787 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 7788 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 7789 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 7790 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 7791 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 7792 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 7793 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 7794 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 7795 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 7796 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 7797 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 7798 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 7799 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 7800 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 7801 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 7802 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 7803 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7804 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7805 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 7806 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 7807 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 7808 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 7809 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7810 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7811 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 7812 |  |
|      - | 7813 |  |
|      - | 7814 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 7815 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 7816 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 7817 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 7818 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 7819 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 7820 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 7821 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 7822 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7823 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 7824 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 7825 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 7826 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 7827 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 7828 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7829 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7830 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 7831 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 7832 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7833 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7834 |  |
|      - | 7835 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 7836 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 7837 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 7838 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 7839 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 7840 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 7841 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 7842 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 7843 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 7844 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 7845 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 7846 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 7847 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7848 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7849 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 7850 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7851 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7852 |  |
|      - | 7853 | `	         /* Ctype functions */` |
|      - | 7854 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 7855 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 7856 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 7857 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 7858 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 7859 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 7860 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 7861 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 7862 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 7863 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 7864 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 7865 | `	         /* Time functions */` |
|      - | 7866 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 7867 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 7868 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 7869 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 7870 | `	{ "date",        PH7_builtin_date         },` |
|      - | 7871 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 7872 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 7873 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 7874 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 7875 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 7876 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 7877 | `	        /* URL functions */` |
|      - | 7878 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 7879 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 7880 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 7881 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 7882 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 7883 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 7884 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 7885 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 7886 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7887 | `};` |
|      - | 7888 | `/*` |
|      - | 7889 | ` * Register the built-in functions defined above,the array functions` |
|      - | 7890 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 7891 | ` */` |
|   2966 | 7892 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 7893 |  |
|      - | 7894 | `	sxu32 n;` |
| 495327 | 7895 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 492361 | 7896 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 246183 | 7897 | `	}` |
|      - | 7898 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   2971 | 7899 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 7900 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   2971 | 7901 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   2971 | 7902 |  |
|      - | 7903 |  |
