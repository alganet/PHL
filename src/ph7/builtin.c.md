# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3045/3455 lines (88.13%)

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
|     74 |   42 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   43 |  |
|     75 |   44 | `	int res = 0; /* Assume false by default */` |
|     75 |   45 | `	if( nArg > 0 ){` |
|     73 |   46 | `		res = ph7_value_is_float(apArg[0]);` |
|     36 |   47 | `	}` |
|      - |   48 | `	/* Query result */` |
|     75 |   49 | `	ph7_result_bool(pCtx,res);` |
|     75 |   50 | `	return PH7_OK;` |
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
|    464 |   62 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   63 |  |
|    466 |   64 | `	int res = 0; /* Assume false by default */` |
|    466 |   65 | `	if( nArg > 0 ){` |
|    464 |   66 | `		res = ph7_value_is_int(apArg[0]);` |
|    231 |   67 | `	}` |
|      - |   68 | `	/* Query result */` |
|    466 |   69 | `	ph7_result_bool(pCtx,res);` |
|    466 |   70 | `	return PH7_OK;` |
|      2 |   71 |  |
|      - |   72 | `/*` |
|      - |   73 | ` * bool is_string($var)` |
|      - |   74 | ` *  Finds out whether a variable is a string.` |
|      - |   75 | ` * Parameters` |
|      - |   76 | ` *   $var: The variable being evaluated.` |
|      - |   77 | ` * Return` |
|      - |   78 | ` *  TRUE if var is string. False otherwise.` |
|      - |   79 | ` */` |
|     94 |   80 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   81 |  |
|     95 |   82 | `	int res = 0; /* Assume false by default */` |
|     95 |   83 | `	if( nArg > 0 ){` |
|     93 |   84 | `		res = ph7_value_is_string(apArg[0]);` |
|     46 |   85 | `	}` |
|      - |   86 | `	/* Query result */` |
|     95 |   87 | `	ph7_result_bool(pCtx,res);` |
|     95 |   88 | `	return PH7_OK;` |
|      1 |   89 |  |
|      - |   90 | `/*` |
|      - |   91 | ` * bool is_null($var)` |
|      - |   92 | ` *  Finds out whether a variable is NULL.` |
|      - |   93 | ` * Parameters` |
|      - |   94 | ` *   $var: The variable being evaluated.` |
|      - |   95 | ` * Return` |
|      - |   96 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |   97 | ` */` |
|     84 |   98 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   99 |  |
|     86 |  100 | `	int res = 0; /* Assume false by default */` |
|     86 |  101 | `	if( nArg > 0 ){` |
|     84 |  102 | `		res = ph7_value_is_null(apArg[0]);` |
|     41 |  103 | `	}` |
|      - |  104 | `	/* Query result */` |
|     86 |  105 | `	ph7_result_bool(pCtx,res);` |
|     86 |  106 | `	return PH7_OK;` |
|      2 |  107 |  |
|      - |  108 | `/*` |
|      - |  109 | ` * bool is_numeric($var)` |
|      - |  110 | ` *  Find out whether a variable is NULL.` |
|      - |  111 | ` * Parameters` |
|      - |  112 | ` *  $var: The variable being evaluated.` |
|      - |  113 | ` * Return` |
|      - |  114 | ` *  True if var is numeric. False otherwise.` |
|      - |  115 | ` */` |
|     38 |  116 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  117 |  |
|     40 |  118 | `	int res = 0; /* Assume false by default */` |
|     40 |  119 | `	if( nArg > 0 ){` |
|     38 |  120 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     18 |  121 | `	}` |
|      - |  122 | `	/* Query result */` |
|     40 |  123 | `	ph7_result_bool(pCtx,res);` |
|     40 |  124 | `	return PH7_OK;` |
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
|    194 |  152 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  153 |  |
|    196 |  154 | `	int res = 0; /* Assume false by default */` |
|    196 |  155 | `	if( nArg > 0 ){` |
|    194 |  156 | `		res = ph7_value_is_array(apArg[0]);` |
|     96 |  157 | `	}` |
|      - |  158 | `	/* Query result */` |
|    196 |  159 | `	ph7_result_bool(pCtx,res);` |
|    196 |  160 | `	return PH7_OK;` |
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
|     26 |  226 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  227 |  |
|     27 |  228 | `	if( nArg < 1 ){` |
|      - |  229 | `		/* return 0 */` |
|      3 |  230 | `		ph7_result_int(pCtx,0);` |
|      2 |  231 | `	}else{` |
|      - |  232 | `		sxi64 iVal;` |
|      - |  233 | `		/* Perform the cast */` |
|     25 |  234 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     25 |  235 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  236 | `	}` |
|     27 |  237 | `	return PH7_OK;` |
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
|      - |  262 | ` * bool boolval($var)` |
|      - |  263 | ` *  Get the boolean value of a variable.` |
|      - |  264 | ` * Parameter` |
|      - |  265 | ` *  $var: The variable being processed.` |
|      - |  266 | ` * Return` |
|      - |  267 | ` *  the bool value of a variable.` |
|      - |  268 | ` */` |
|     16 |  269 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|      - |  271 | `	int bVal;` |
|     18 |  272 | `	if( nArg != 1 ){` |
|      4 |  273 | `		return PH7_VmThrowException(pCtx,` |
|      - |  274 | `			"ArgumentCountError",` |
|      - |  275 | `			"boolval() expects exactly 1 argument, %d given",` |
|      1 |  276 | `			nArg` |
|      - |  277 | `			);` |
|      - |  278 | `	}` |
|      - |  279 | `	/* Perform the cast */` |
|     15 |  280 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  281 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  282 | `	return PH7_OK;` |
|     10 |  283 |  |
|      - |  284 | `/*` |
|      - |  285 | ` * bool empty($var)` |
|      - |  286 | ` *  Determine whether a variable is empty.` |
|      - |  287 | ` * Parameters` |
|      - |  288 | ` *   $var: The variable being checked.` |
|      - |  289 | ` * Return` |
|      - |  290 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  291 | ` */` |
|  24556 |  292 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  293 |  |
|  24558 |  294 | `	int res = 1; /* Assume empty by default */` |
|  24558 |  295 | `	if( nArg > 0 ){` |
|  24556 |  296 | `		res = ph7_value_is_empty(apArg[0]);` |
|  12277 |  297 | `	}` |
|  24558 |  298 | `	ph7_result_bool(pCtx,res);` |
|  24558 |  299 | `	return PH7_OK;` |
|      - |  300 |  |
|      2 |  301 |  |
|      - |  302 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  303 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  304 | `#endif` |
|      - |  305 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  306 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  307 | `#endif` |
|      - |  308 |  |
|      - |  309 | `/* Math functions moved to builtin_math.c */` |
|      - |  310 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  311 | `/*` |
|      - |  312 | ` * Section:` |
|      - |  313 | ` *    String handling Functions.` |
|      - |  314 | ` * Status:` |
|      - |  315 | ` *    Stable.` |
|      - |  316 | ` */` |
|      - |  317 | `/*` |
|      - |  318 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  319 | ` *  Return part of a string.` |
|      - |  320 | ` * Parameters` |
|      - |  321 | ` *  $string` |
|      - |  322 | ` *   The input string. Must be one character or longer.` |
|      - |  323 | ` * $start` |
|      - |  324 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  325 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  326 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  327 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  328 | ` *   from the end of string.` |
|      - |  329 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  330 | ` * $length` |
|      - |  331 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  332 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  333 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  334 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  335 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  336 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  337 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  338 | ` *   will be returned.` |
|      - |  339 | ` * Return` |
|      - |  340 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  341 | ` */` |
| 179788 |  342 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  343 |  |
|      - |  344 | `	const char *zSource,*zOfft;` |
|      - |  345 | `	int nOfft,nLen,nSrcLen;` |
| 179790 |  346 | `	if( nArg < 2 ){` |
|      - |  347 | `		/* return FALSE */` |
|      5 |  348 | `		ph7_result_bool(pCtx,0);` |
|      5 |  349 | `		return PH7_OK;` |
|      - |  350 | `	}` |
|      - |  351 | `	/* Extract the target string */` |
| 179786 |  352 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 179786 |  353 | `	if( nSrcLen < 1 ){` |
|      - |  354 | `		/* Empty string,return FALSE */` |
|  10638 |  355 | `		ph7_result_bool(pCtx,0);` |
|  10638 |  356 | `		return PH7_OK;` |
|      - |  357 | `	}` |
| 169150 |  358 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  359 | `	/* Extract the offset */` |
| 169150 |  360 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 169150 |  361 | `	if( nOfft < 0 ){` |
|  28342 |  362 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  28342 |  363 | `		if( zOfft < zSource ){` |
|      - |  364 | `			/* Invalid offset */` |
|      5 |  365 | `			ph7_result_bool(pCtx,0);` |
|      5 |  366 | `			return PH7_OK;` |
|      - |  367 | `		}` |
|  28338 |  368 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  28338 |  369 | `		nOfft = (int)(zOfft-zSource);` |
| 154978 |  370 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  371 | `		/* Invalid offset */` |
|    106 |  372 | `		ph7_result_bool(pCtx,0);` |
|    106 |  373 | `		return PH7_OK;` |
|    ! 0 |  374 | `	}else{` |
| 140706 |  375 | `		zOfft = &zSource[nOfft];` |
| 140706 |  376 | `		nLen = nSrcLen - nOfft;` |
|      - |  377 | `	}` |
| 169042 |  378 | `	if( nArg > 2 ){` |
|      - |  379 | `		/* Extract the length */` |
| 139526 |  380 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 139526 |  381 | `		if( nLen == 0 ){` |
|      - |  382 | `			/* Invalid length,return an empty string */` |
|      5 |  383 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  384 | `			return PH7_OK;` |
| 139522 |  385 | `		}else if( nLen < 0 ){` |
|  28340 |  386 | `			nLen = nSrcLen + nLen - nOfft;` |
|  28340 |  387 | `			if( nLen < 1 ){` |
|      - |  388 | `				/* Invalid  length */` |
|      3 |  389 | `				nLen = nSrcLen - nOfft;` |
|      1 |  390 | `			}` |
|  14169 |  391 | `		}` |
| 139522 |  392 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  393 | `			/* Invalid length */` |
|   3904 |  394 | `			nLen = nSrcLen - nOfft;` |
|   1951 |  395 | `		}` |
|  69760 |  396 | `	}` |
|      - |  397 | `	/* Return the substring */` |
| 169038 |  398 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 169038 |  399 | `	return PH7_OK;` |
|  89896 |  400 |  |
|      - |  401 | `/*` |
|      - |  402 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  403 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  404 | ` * Parameters` |
|      - |  405 | ` *  $main_str` |
|      - |  406 | ` *  The main string being compared.` |
|      - |  407 | ` *  $str` |
|      - |  408 | ` *   The secondary string being compared.` |
|      - |  409 | ` * $offset` |
|      - |  410 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  411 | ` *  the end of the string.` |
|      - |  412 | ` * $length` |
|      - |  413 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  414 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  415 | ` * $case_insensitivity` |
|      - |  416 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  417 | ` * Return` |
|      - |  418 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  419 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  420 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  421 | ` */` |
|     26 |  422 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  423 |  |
|      - |  424 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  425 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 |  426 | `	int iCase = 0;` |
|      - |  427 | `	int rc;` |
|     27 |  428 | `	if( nArg < 3 ){` |
|      - |  429 | `		/* Missing arguments,return FALSE */` |
|      5 |  430 | `		ph7_result_bool(pCtx,0);` |
|      5 |  431 | `		return PH7_OK;` |
|      - |  432 | `	}` |
|      - |  433 | `	/* Extract the target string */` |
|     23 |  434 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  435 | `	if( nSrcLen < 1 ){` |
|      - |  436 | `		/* Empty string,return FALSE */` |
|      3 |  437 | `		ph7_result_bool(pCtx,0);` |
|      3 |  438 | `		return PH7_OK;` |
|      - |  439 | `	}` |
|     21 |  440 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  441 | `	/* Extract the substring */` |
|     21 |  442 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  443 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  444 | `		/* Empty string,return FALSE */` |
|      3 |  445 | `		ph7_result_bool(pCtx,0);` |
|      3 |  446 | `		return PH7_OK;` |
|      - |  447 | `	}` |
|      - |  448 | `	/* Extract the offset */` |
|     19 |  449 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  450 | `	if( nOfft < 0 ){` |
|      5 |  451 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  452 | `		if( zOfft < zSource ){` |
|      - |  453 | `			/* Invalid offset */` |
|      3 |  454 | `			ph7_result_bool(pCtx,0);` |
|      3 |  455 | `			return PH7_OK;` |
|      - |  456 | `		}` |
|      3 |  457 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  458 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  459 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  460 | `		/* Invalid offset */` |
|      3 |  461 | `		ph7_result_bool(pCtx,0);` |
|      3 |  462 | `		return PH7_OK;` |
|    ! 0 |  463 | `	}else{` |
|     13 |  464 | `		zOfft = &zSource[nOfft];` |
|     13 |  465 | `		nLen = nSrcLen - nOfft;` |
|      - |  466 | `	}` |
|     15 |  467 | `	if( nArg > 3 ){` |
|      - |  468 | `		/* Extract the length */` |
|     13 |  469 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  470 | `		if( nLen < 1 ){` |
|      - |  471 | `			/* Invalid  length */` |
|      5 |  472 | `			ph7_result_int(pCtx,1);` |
|      5 |  473 | `			return PH7_OK;` |
|      9 |  474 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  475 | `			/* Invalid length */` |
|      3 |  476 | `			nLen = nSrcLen - nOfft;` |
|      1 |  477 | `		}` |
|      9 |  478 | `		if( nArg > 4 ){` |
|      - |  479 | `			/* Case-sensitive or not */` |
|      5 |  480 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  481 | `		}` |
|      4 |  482 | `	}` |
|      - |  483 | `	/* Perform the comparison */` |
|     11 |  484 | `	if( iCase ){` |
|      3 |  485 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  486 | `	}else{` |
|      9 |  487 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  488 | `	}` |
|      - |  489 | `	/* Comparison result */` |
|     11 |  490 | `	ph7_result_int(pCtx,rc);` |
|     11 |  491 | `	return PH7_OK;` |
|     14 |  492 |  |
|      - |  493 | `/*` |
|      - |  494 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  495 | ` *  Count the number of substring occurrences.` |
|      - |  496 | ` * Parameters` |
|      - |  497 | ` * $haystack` |
|      - |  498 | ` *   The string to search in` |
|      - |  499 | ` * $needle` |
|      - |  500 | ` *   The substring to search for` |
|      - |  501 | ` * $offset` |
|      - |  502 | ` *  The offset where to start counting` |
|      - |  503 | ` * $length (NOT USED)` |
|      - |  504 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  505 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  506 | ` * Return` |
|      - |  507 | ` *  Toral number of substring occurrences.` |
|      - |  508 | ` */` |
|     24 |  509 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  510 |  |
|      - |  511 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  512 | `	int nTextlen,nPatlen;` |
|     25 |  513 | `	int iCount = 0;` |
|      - |  514 | `	sxu32 nOfft;` |
|      - |  515 | `	sxi32 rc;` |
|     25 |  516 | `	if( nArg < 2 ){` |
|      - |  517 | `		/* Missing arguments */` |
|      5 |  518 | `		ph7_result_int(pCtx,0);` |
|      5 |  519 | `		return PH7_OK;` |
|      - |  520 | `	}` |
|      - |  521 | `	/* Point to the haystack */` |
|     21 |  522 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  523 | `	/* Point to the neddle */` |
|     21 |  524 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 |  525 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  526 | `		/* NOOP,return zero */` |
|      3 |  527 | `		ph7_result_int(pCtx,0);` |
|      3 |  528 | `		return PH7_OK;` |
|      - |  529 | `	}` |
|     19 |  530 | `	if( nArg > 2 ){` |
|      - |  531 | `		int iOfft;` |
|      - |  532 | `		/* Extract the offset */` |
|     15 |  533 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 |  534 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - |  535 | `			/* Invalid offset,return zero */` |
|      3 |  536 | `			ph7_result_int(pCtx,0);` |
|      3 |  537 | `			return PH7_OK;` |
|      - |  538 | `		}` |
|      - |  539 | `		/* Point to the desired offset */` |
|     13 |  540 | `		zText = &zText[iOfft];` |
|      - |  541 | `		/* Adjust length */` |
|     13 |  542 | `		nTextlen -= iOfft;` |
|      6 |  543 | `	}` |
|      - |  544 | `	/* Point to the end of the string */` |
|     17 |  545 | `	zEnd = &zText[nTextlen];` |
|     17 |  546 | `	if( nArg > 3 ){` |
|      - |  547 | `		int nLen;` |
|      - |  548 | `		/* Extract the length */` |
|     13 |  549 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  550 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - |  551 | `			/* Invalid length,return 0 */` |
|      7 |  552 | `			ph7_result_int(pCtx,0);` |
|      7 |  553 | `			return PH7_OK;` |
|      - |  554 | `		}` |
|      - |  555 | `		/* Adjust pointer */` |
|      7 |  556 | `		nTextlen = nLen;` |
|      7 |  557 | `		zEnd = &zText[nTextlen];` |
|      3 |  558 | `	}` |
|      - |  559 | `	/* Perform the search */` |
|     12 |  560 | `	for(;;){` |
|     25 |  561 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 |  562 | `		if( rc != SXRET_OK ){` |
|      - |  563 | `			/* Pattern not found,break immediately */` |
|      9 |  564 | `			break;` |
|      - |  565 | `		}` |
|      - |  566 | `		/* Increment counter and update the offset */` |
|     17 |  567 | `		iCount++;` |
|     17 |  568 | `		zText += nOfft + nPatlen;` |
|     17 |  569 | `		if( zText >= zEnd ){` |
|      3 |  570 | `			break;` |
|      - |  571 | `		}` |
|      1 |  572 | `	}` |
|      - |  573 | `	/* Pattern count */` |
|     11 |  574 | `	ph7_result_int(pCtx,iCount);` |
|     11 |  575 | `	return PH7_OK;` |
|     13 |  576 |  |
|      - |  577 | `/*` |
|      - |  578 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - |  579 | ` *   Split a string into smaller chunks.` |
|      - |  580 | ` * Parameters` |
|      - |  581 | ` *  $body` |
|      - |  582 | ` *   The string to be chunked.` |
|      - |  583 | ` * $chunklen` |
|      - |  584 | ` *   The chunk length.` |
|      - |  585 | ` * $end` |
|      - |  586 | ` *   The line ending sequence.` |
|      - |  587 | ` * Return` |
|      - |  588 | ` *  The chunked string or NULL on failure.` |
|      - |  589 | ` */` |
|     16 |  590 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  591 |  |
|     17 |  592 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - |  593 | `	int nSepLen,nChunkLen,nLen;` |
|     17 |  594 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  595 | `		/* Nothing to split,return null */` |
|      5 |  596 | `		ph7_result_null(pCtx);` |
|      5 |  597 | `		return PH7_OK;` |
|      - |  598 | `	}` |
|      - |  599 | `	/* initialize/Extract arguments */` |
|     13 |  600 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 |  601 | `	nChunkLen = 76;` |
|     13 |  602 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 |  603 | `	zEnd = &zIn[nLen];` |
|     13 |  604 | `	if( nArg > 1 ){` |
|      - |  605 | `		/* Chunk length */` |
|     13 |  606 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 |  607 | `		if( nChunkLen < 1 ){` |
|      - |  608 | `			/* Switch back to the default length */` |
|      3 |  609 | `			nChunkLen = 76;` |
|      1 |  610 | `		}` |
|     13 |  611 | `		if( nArg > 2 ){` |
|      - |  612 | `			/* Separator */` |
|      9 |  613 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 |  614 | `			if( nSepLen < 1 ){` |
|      - |  615 | `				/* Switch back to the default separator */` |
|      3 |  616 | `				zSep = "\r\n";` |
|      3 |  617 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 |  618 | `			}` |
|      4 |  619 | `		}` |
|      6 |  620 | `	}` |
|      - |  621 | `	/* Perform the requested operation */` |
|     13 |  622 | `	if( nChunkLen > nLen ){` |
|      - |  623 | `		/* Nothing to split,return the string and the separator */` |
|      9 |  624 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 |  625 | `		return PH7_OK;` |
|      - |  626 | `	}` |
|     17 |  627 | `	while( zIn < zEnd ){` |
|     13 |  628 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 |  629 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 |  630 | `		}` |
|      - |  631 | `		/* Append the chunk and the separator */` |
|     13 |  632 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - |  633 | `		/* Point beyond the chunk */` |
|     13 |  634 | `		zIn += nChunkLen;` |
|      1 |  635 | `	}` |
|      5 |  636 | `	return PH7_OK;` |
|      9 |  637 |  |
|      - |  638 | `/*` |
|      - |  639 | ` * string addslashes(string $str)` |
|      - |  640 | ` *  Quote string with slashes.` |
|      - |  641 | ` *  Returns a string with backslashes before characters that need` |
|      - |  642 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  643 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  644 | ` * Parameter` |
|      - |  645 | ` *  str: The string to be escaped.` |
|      - |  646 | ` * Return` |
|      - |  647 | ` *  Returns the escaped string` |
|      - |  648 | ` */` |
|     24 |  649 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  650 |  |
|      - |  651 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  652 | `	int nLen;` |
|      - |  653 | `	/* PHP enforces exactly one argument. */` |
|     26 |  654 | `	if( nArg != 1 ){` |
|      7 |  655 | `		return PH7_VmThrowException(pCtx,` |
|      - |  656 | `			"ArgumentCountError",` |
|      - |  657 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 |  658 | `			nArg` |
|      - |  659 | `			);` |
|      - |  660 | `	}` |
|      - |  661 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - |  662 | `	 * types still produce a TypeError. */` |
|     22 |  663 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 |  664 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  665 | `			E_DEPRECATED,` |
|      - |  666 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  667 | `			);` |
|      - |  668 | `		/* fall through so conversion below yields empty string */` |
|      1 |  669 | `	}` |
|      - |  670 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 |  671 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 |  672 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 |  673 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 |  674 | `		return PH7_VmThrowException(pCtx,` |
|      - |  675 | `			"TypeError",` |
|      - |  676 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  677 | `			ph7_type_name(apArg[0])` |
|      - |  678 | `			);` |
|      - |  679 | `	}` |
|      - |  680 | `	/* Convert to string representation first and obtain length. */` |
|     19 |  681 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 |  682 | `	if( nLen < 1 ){` |
|      - |  683 | `		/* Return the empty string */` |
|      5 |  684 | `		ph7_result_string(pCtx,"",0);` |
|      5 |  685 | `		return PH7_OK;` |
|      - |  686 | `	}` |
|     15 |  687 | `	zEnd = &zIn[nLen];` |
|     15 |  688 | `	zCur = 0; /* cc warning */` |
|     20 |  689 | `	for(;;){` |
|     41 |  690 | `		if( zIn >= zEnd ){` |
|      - |  691 | `			/* No more input */` |
|     15 |  692 | `			break;` |
|      - |  693 | `		}` |
|     27 |  694 | `		zCur = zIn;` |
|      - |  695 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 |  696 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 |  697 | `			zIn++;` |
|      1 |  698 | `		}` |
|     27 |  699 | `		if( zIn > zCur ){` |
|      - |  700 | `			/* Append raw contents */` |
|     23 |  701 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 |  702 | `		}` |
|     27 |  703 | `		if( zIn < zEnd ){` |
|     17 |  704 | `			int c = zIn[0];` |
|     17 |  705 | `			if( c == '\0' ){` |
|      - |  706 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 |  707 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 |  708 | `			}else{` |
|     15 |  709 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  710 | `			}` |
|      8 |  711 | `		}` |
|     27 |  712 | `		zIn++;` |
|      1 |  713 | `	}` |
|     15 |  714 | `	return PH7_OK;` |
|     14 |  715 |  |
|      - |  716 | `/*` |
|      - |  717 | ` * Check if the given character is present in the given mask.` |
|      - |  718 | ` * Return TRUE if present. FALSE otherwise.` |
|      - |  719 | ` */` |
|    124 |  720 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 |  721 |  |
|    125 |  722 | `	const char *zEnd = &zMask[nLen];` |
|    555 |  723 | `	while( zMask < zEnd ){` |
|      - |  724 | `		/* Support range syntax A..Z where A and Z are literal bytes.  The` |
|      - |  725 | `		 * original PH7 implementation ignored ranges; tests rely on them so` |
|      - |  726 | `		 * provide a simple on-the-fly check here. */` |
|    475 |  727 | `		if( zMask + 3 < zEnd && zMask[1] == '.' && zMask[2] == '.' ){` |
|      3 |  728 | `			int lo = (unsigned char)zMask[0];` |
|      3 |  729 | `			int hi = (unsigned char)zMask[3];` |
|      3 |  730 | `			if( lo > hi ){` |
|    ! 0 |  731 | `				int tmp = lo; lo = hi; hi = tmp;` |
|    ! 0 |  732 | `			}` |
|      3 |  733 | `			if( c >= lo && c <= hi ){` |
|      3 |  734 | `				return 1;` |
|      - |  735 | `			}` |
|      - |  736 | `			/* consume the range specifier */` |
|    ! 0 |  737 | `			zMask += 4;` |
|    ! 0 |  738 | `			continue;` |
|      - |  739 | `		}` |
|    473 |  740 | `		if( zMask[0] == c ){` |
|      - |  741 | `			/* Character present,return TRUE */` |
|     43 |  742 | `			return 1;` |
|      - |  743 | `		}` |
|      - |  744 | `		/* Advance the pointer */` |
|    431 |  745 | `		zMask++;` |
|      1 |  746 | `	}` |
|      - |  747 | `	/* Not present */` |
|     81 |  748 | `	return 0;` |
|     63 |  749 |  |
|      - |  750 | `/*` |
|      - |  751 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  752 | ` *  Quote string with slashes in a C style.` |
|      - |  753 | ` * Parameter` |
|      - |  754 | ` *  $str:` |
|      - |  755 | ` *    The string to be escaped.` |
|      - |  756 | ` *  $charlist:` |
|      - |  757 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  758 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  759 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  760 | ` * Return` |
|      - |  761 | ` *  Returns the escaped string.` |
|      - |  762 | ` * Note:` |
|      - |  763 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - |  764 | ` */` |
|     34 |  765 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  766 |  |
|      - |  767 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  768 | `	int nLen,nMask;` |
|      - |  769 | `	/* PHP enforces exactly two arguments. */` |
|     36 |  770 | `	if( nArg != 2 ){` |
|      7 |  771 | `		return PH7_VmThrowException(pCtx,` |
|      - |  772 | `			"ArgumentCountError",` |
|      - |  773 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  774 | `			nArg` |
|      - |  775 | `			);` |
|      - |  776 | `	}` |
|      - |  777 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  778 | `	 * treated as the empty string (PHP 8.1). */` |
|     32 |  779 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  780 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  781 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  782 | `			E_DEPRECATED,` |
|      - |  783 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  784 | `			);` |
|      - |  785 | `		/* treat as empty string; fall through to conversion logic */` |
|     56 |  786 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     41 |  787 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     26 |  788 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  789 | `		return PH7_VmThrowException(pCtx,` |
|      - |  790 | `			"TypeError",` |
|      - |  791 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  792 | `			ph7_type_name(apArg[0])` |
|      - |  793 | `			);` |
|      - |  794 | `	}` |
|      - |  795 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  796 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  797 | `	 * trigger a TypeError. */` |
|     30 |  798 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  799 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  800 | `			E_DEPRECATED,` |
|      - |  801 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  802 | `			);` |
|      - |  803 | `		/* allow through so it becomes empty string below */` |
|     52 |  804 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     38 |  805 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     24 |  806 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  807 | `		return PH7_VmThrowException(pCtx,` |
|      - |  808 | `			"TypeError",` |
|      - |  809 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  810 | `			ph7_type_name(apArg[1])` |
|      - |  811 | `			);` |
|      - |  812 | `	}` |
|      - |  813 | `	/* Extract the string to process */` |
|     27 |  814 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  815 | `	/* NULL would never reach here due to the check above. */` |
|     27 |  816 | `	if( nLen < 1 ){` |
|      - |  817 | `		/* Empty string returns itself. */` |
|      5 |  818 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  819 | `		return PH7_OK;` |
|      - |  820 | `	}` |
|      - |  821 | `	/* Extract the desired mask */` |
|     23 |  822 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     23 |  823 | `	zEnd = &zIn[nLen];` |
|     23 |  824 | `	zCur = 0; /* cc warning */` |
|     29 |  825 | `	for(;;){` |
|     59 |  826 | `		if( zIn >= zEnd ){` |
|      - |  827 | `			/* No more input */` |
|     23 |  828 | `			break;` |
|      - |  829 | `		}` |
|     37 |  830 | `		zCur = zIn;` |
|     91 |  831 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     55 |  832 | `			zIn++;` |
|      1 |  833 | `		}` |
|     37 |  834 | `		if( zIn > zCur ){` |
|      - |  835 | `			/* Append raw contents */` |
|     33 |  836 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 |  837 | `		}` |
|     37 |  838 | `		if( zIn < zEnd ){` |
|      - |  839 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  840 | `			 * on platforms where char is signed. */` |
|     19 |  841 | `			int c = (unsigned char)zIn[0];` |
|      - |  842 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  843 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  844 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     19 |  845 | `			if( c == '\n' ){` |
|      3 |  846 | `				ph7_result_string(pCtx,"\\n",2);` |
|     18 |  847 | `			}else if( c == '\r' ){` |
|      3 |  848 | `				ph7_result_string(pCtx,"\\r",2);` |
|     16 |  849 | `			}else if( c == '\t' ){` |
|      3 |  850 | `				ph7_result_string(pCtx,"\\t",2);` |
|     14 |  851 | `			}else if( c == '\v' ){` |
|      3 |  852 | `				ph7_result_string(pCtx,"\\v",2);` |
|     12 |  853 | `			}else if( c == '\f' ){` |
|      3 |  854 | `				ph7_result_string(pCtx,"\\f",2);` |
|     10 |  855 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  856 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  857 | `				 * octal escapes (\001 not \1). */` |
|      7 |  858 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  859 | `			}else{` |
|      3 |  860 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  861 | `			}` |
|      9 |  862 | `		}` |
|     37 |  863 | `		zIn++;` |
|      1 |  864 | `	}` |
|     23 |  865 | `	return PH7_OK;` |
|     19 |  866 |  |
|      - |  867 | `/*` |
|      - |  868 | ` * string quotemeta(string $str)` |
|      - |  869 | ` *  Quote meta characters.` |
|      - |  870 | ` * Parameter` |
|      - |  871 | ` *  $str:` |
|      - |  872 | ` *    The string to be escaped.` |
|      - |  873 | ` * Return` |
|      - |  874 | ` *  Returns the escaped string.` |
|      - |  875 | `*/` |
|     10 |  876 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  877 |  |
|      - |  878 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  879 | `	int nLen;` |
|     11 |  880 | `	if( nArg < 1 ){` |
|      - |  881 | `		/* Nothing to process,retun NULL */` |
|      3 |  882 | `		ph7_result_null(pCtx);` |
|      3 |  883 | `		return PH7_OK;` |
|      - |  884 | `	}` |
|      - |  885 | `	/* Extract the string to process */` |
|      9 |  886 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 |  887 | `	if( nLen < 1 ){` |
|      - |  888 | `		/* Return the empty string */` |
|      3 |  889 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  890 | `		return PH7_OK;` |
|      - |  891 | `	}` |
|      7 |  892 | `	zEnd = &zIn[nLen];` |
|      7 |  893 | `	zCur = 0; /* cc warning */` |
|     17 |  894 | `	for(;;){` |
|     35 |  895 | `		if( zIn >= zEnd ){` |
|      - |  896 | `			/* No more input */` |
|      7 |  897 | `			break;` |
|      - |  898 | `		}` |
|     29 |  899 | `		zCur = zIn;` |
|     55 |  900 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 |  901 | `			zIn++;` |
|      1 |  902 | `		}` |
|     29 |  903 | `		if( zIn > zCur ){` |
|      - |  904 | `			/* Append raw contents */` |
|     11 |  905 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 |  906 | `		}` |
|     29 |  907 | `		if( zIn < zEnd ){` |
|     27 |  908 | `			int c = zIn[0];` |
|     27 |  909 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 |  910 | `		}` |
|     29 |  911 | `		zIn++;` |
|      1 |  912 | `	}` |
|      7 |  913 | `	return PH7_OK;` |
|      6 |  914 |  |
|      - |  915 | `/*` |
|      - |  916 | ` * string stripslashes(string $str)` |
|      - |  917 | ` *  Un-quotes a quoted string.` |
|      - |  918 | ` *  Returns a string with backslashes before characters that need` |
|      - |  919 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  920 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  921 | ` * Parameter` |
|      - |  922 | ` *  $str` |
|      - |  923 | ` *   The input string.` |
|      - |  924 | ` * Return` |
|      - |  925 | ` *  Returns a string with backslashes stripped off.` |
|      - |  926 | ` */` |
|      8 |  927 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  928 |  |
|      - |  929 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  930 | `	int nLen;` |
|      9 |  931 | `	if( nArg < 1 ){` |
|      - |  932 | `		/* Nothing to process,retun NULL */` |
|      3 |  933 | `		ph7_result_null(pCtx);` |
|      3 |  934 | `		return PH7_OK;` |
|      - |  935 | `	}` |
|      - |  936 | `	/* Extract the string to process */` |
|      7 |  937 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  938 | `	if( zIn == 0 ){` |
|    ! 0 |  939 | `		ph7_result_null(pCtx);` |
|    ! 0 |  940 | `		return PH7_OK;` |
|      - |  941 | `	}` |
|      7 |  942 | `	zEnd = &zIn[nLen];` |
|      7 |  943 | `	zCur = 0; /* cc warning */` |
|      - |  944 | `	/* Encode the string */` |
|      4 |  945 | `	for(;;){` |
|      9 |  946 | `		if( zIn >= zEnd ){` |
|      - |  947 | `			/* No more input */` |
|      5 |  948 | `			break;` |
|      - |  949 | `		}` |
|      5 |  950 | `		zCur = zIn;` |
|     17 |  951 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  952 | `			zIn++;` |
|      1 |  953 | `		}` |
|      5 |  954 | `		if( zIn > zCur ){` |
|      - |  955 | `			/* Append raw contents */` |
|      5 |  956 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 |  957 | `		}` |
|      5 |  958 | `		if( &zIn[1] < zEnd ){` |
|      3 |  959 | `			int c = zIn[1];` |
|      3 |  960 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - |  961 | `				/* Ignore the backslash */` |
|      3 |  962 | `				zIn++;` |
|      1 |  963 | `			}` |
|      2 |  964 | `		}else{` |
|      3 |  965 | `			break;` |
|      - |  966 | `		}` |
|      1 |  967 | `	}` |
|      7 |  968 | `	return PH7_OK;` |
|      5 |  969 |  |
|      - |  970 | `/*` |
|      - |  971 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - |  972 | ` *  HTML escaping of special characters.` |
|      - |  973 | ` *  The translations performed are:` |
|      - |  974 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - |  975 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - |  976 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - |  977 | ` *   '<' (less than) ==> '&lt;'` |
|      - |  978 | ` *   '>' (greater than) ==> '&gt;'` |
|      - |  979 | ` * Parameters` |
|      - |  980 | ` *  $string` |
|      - |  981 | ` *   The string being converted.` |
|      - |  982 | ` * $flags` |
|      - |  983 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - |  984 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - |  985 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - |  986 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - |  987 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - |  988 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - |  989 | ` * $charset` |
|      - |  990 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - |  991 | ` * Return` |
|      - |  992 | ` *  The escaped string or NULL on failure.` |
|      - |  993 | ` */` |
|     20 |  994 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  995 |  |
|      - |  996 | `	const char *zCur,*zIn,*zEnd;` |
|     21 |  997 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - |  998 | `	int nLen,c;` |
|     21 |  999 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1000 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1001 | `		ph7_result_null(pCtx);` |
|      9 | 1002 | `		return PH7_OK;` |
|      - | 1003 | `	}` |
|      - | 1004 | `	/* Extract the target string */` |
|     13 | 1005 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1006 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1007 | `	if( nLen == 0 ){` |
|      3 | 1008 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1009 | `		return PH7_OK;` |
|      - | 1010 | `	}` |
|     11 | 1011 | `	zEnd = &zIn[nLen];` |
|      - | 1012 | `	/* Extract the flags if available */` |
|     11 | 1013 | `	if( nArg > 1 ){` |
|      9 | 1014 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1015 | `		if( iFlags < 0 ){` |
|      3 | 1016 | `			iFlags = 0x01\|0x40;` |
|      1 | 1017 | `		}` |
|      4 | 1018 | `	}` |
|      - | 1019 | `	/* Perform the requested operation */` |
|     23 | 1020 | `	for(;;){` |
|     47 | 1021 | `		if( zIn >= zEnd ){` |
|      9 | 1022 | `			break;` |
|      - | 1023 | `		}` |
|     39 | 1024 | `		zCur = zIn;` |
|     83 | 1025 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1026 | `			zIn++;` |
|      1 | 1027 | `		}` |
|     39 | 1028 | `		if( zCur < zIn ){` |
|      - | 1029 | `			/* Append the raw string verbatim */` |
|     17 | 1030 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1031 | `		}` |
|     39 | 1032 | `		if( zIn >= zEnd ){` |
|      3 | 1033 | `			break;` |
|      - | 1034 | `		}` |
|     37 | 1035 | `		c = zIn[0];` |
|     37 | 1036 | `		if( c == '&' ){` |
|      - | 1037 | `			/* Expand '&amp;' */` |
|      9 | 1038 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1039 | `		}else if( c == '<' ){` |
|      - | 1040 | `			/* Expand '&lt;' */` |
|      7 | 1041 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1042 | `		}else if( c == '>' ){` |
|      - | 1043 | `			/* Expand '&gt;' */` |
|      9 | 1044 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1045 | `		}else if( c == '\'' ){` |
|      5 | 1046 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1047 | `				/* Expand '&#039;' */` |
|      5 | 1048 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1049 | `			}else{` |
|      - | 1050 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1051 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1052 | `			}` |
|     13 | 1053 | `		}else if( c == '"' ){` |
|     11 | 1054 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1055 | `				/* Expand '&quot;' */` |
|      7 | 1056 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1057 | `			}else{` |
|      - | 1058 | `				/* Leave the double quote untouched */` |
|      5 | 1059 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1060 | `			}` |
|      5 | 1061 | `		}` |
|      - | 1062 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1063 | `		zIn++;` |
|      1 | 1064 | `	}` |
|     11 | 1065 | `	return PH7_OK;` |
|     11 | 1066 |  |
|      - | 1067 | `/*` |
|      - | 1068 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1069 | ` *  Unescape HTML entities.` |
|      - | 1070 | ` * Parameters` |
|      - | 1071 | ` *  $string` |
|      - | 1072 | ` *   The string to decode` |
|      - | 1073 | ` *  $quote_style` |
|      - | 1074 | ` *    The quote style. One of the following constants:` |
|      - | 1075 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1076 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1077 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1078 | ` * Return` |
|      - | 1079 | ` *  The unescaped string or NULL on failure.` |
|      - | 1080 | ` */` |
|     16 | 1081 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1082 |  |
|      - | 1083 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1084 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1085 | `	int nLen,nJump;` |
|     17 | 1086 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1087 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1088 | `		ph7_result_null(pCtx);` |
|      7 | 1089 | `		return PH7_OK;` |
|      - | 1090 | `	}` |
|      - | 1091 | `	/* Extract the target string */` |
|     11 | 1092 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1093 | `	zEnd = &zIn[nLen];` |
|      - | 1094 | `	/* Extract the flags if available */` |
|     11 | 1095 | `	if( nArg > 1 ){` |
|      7 | 1096 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1097 | `		if( iFlags < 0 ){` |
|      3 | 1098 | `			iFlags = 0x01;` |
|      1 | 1099 | `		}` |
|      3 | 1100 | `	}` |
|      - | 1101 | `	/* Perform the requested operation */` |
|     15 | 1102 | `	for(;;){` |
|     31 | 1103 | `		if( zIn >= zEnd ){` |
|     11 | 1104 | `			break;` |
|      - | 1105 | `		}` |
|     21 | 1106 | `		zCur = zIn;` |
|     51 | 1107 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1108 | `			zIn++;` |
|      1 | 1109 | `		}` |
|     21 | 1110 | `		if( zCur < zIn ){` |
|      - | 1111 | `			/* Append the raw string verbatim */` |
|      9 | 1112 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1113 | `		}` |
|     21 | 1114 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1115 | `		nJump = (int)sizeof(char);` |
|     21 | 1116 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1117 | `			/* &amp; ==> '&' */` |
|      3 | 1118 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1119 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1120 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1121 | `			/* &lt; ==> < */` |
|      3 | 1122 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1123 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1124 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1125 | `			/* &gt; ==> '>' */` |
|      3 | 1126 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1127 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1128 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1129 | `			/* &quot; ==> '"' */` |
|     13 | 1130 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1131 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1132 | `			}else{` |
|      - | 1133 | `				/* Leave untouched */` |
|      5 | 1134 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1135 | `			}` |
|     13 | 1136 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1137 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1138 | `			/* &#039; ==> ''' */` |
|      3 | 1139 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1140 | `				/* Expand ''' */` |
|      3 | 1141 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1142 | `			}else{` |
|      - | 1143 | `				/* Leave untouched */` |
|    ! 0 | 1144 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1145 | `			}` |
|      3 | 1146 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1147 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1148 | `			/* expand '&' */` |
|    ! 0 | 1149 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1150 | `		}else{` |
|      - | 1151 | `			/* No more input to process */` |
|    ! 0 | 1152 | `			break;` |
|      - | 1153 | `		}` |
|     21 | 1154 | `		zIn += nJump;` |
|      1 | 1155 | `	}` |
|     11 | 1156 | `	return PH7_OK;` |
|      9 | 1157 |  |
|      - | 1158 | `/* HTML encoding/Decoding table` |
|      - | 1159 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1160 | ` */` |
|      - | 1161 | `static const char *azHtmlEscape[] = {` |
|      - | 1162 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1163 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1164 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1165 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1166 | ` };` |
|      - | 1167 | `/*` |
|      - | 1168 | ` * array get_html_translation_table(void)` |
|      - | 1169 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1170 | ` * Parameters` |
|      - | 1171 | ` *  None` |
|      - | 1172 | ` * Return` |
|      - | 1173 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1174 | ` */` |
|      4 | 1175 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1176 |  |
|      - | 1177 | `	ph7_value *pArray,*pValue;` |
|      - | 1178 | `	sxu32 n;` |
|      - | 1179 | `	/* Element value */` |
|      5 | 1180 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1181 | `	if( pValue == 0 ){` |
|    ! 0 | 1182 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1183 | `		SXUNUSED(apArg);` |
|      - | 1184 | `		/* Return NULL */` |
|    ! 0 | 1185 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1186 | `		return PH7_OK;` |
|      - | 1187 | `	}` |
|      - | 1188 | `	/* Create a new array */` |
|      5 | 1189 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1190 | `	if( pArray == 0 ){` |
|      - | 1191 | `		/* Return NULL */` |
|    ! 0 | 1192 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1193 | `		return PH7_OK;` |
|      - | 1194 | `	}` |
|      - | 1195 | `	/* Make the table */` |
|     85 | 1196 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1197 | `		/* Prepare the value */` |
|     81 | 1198 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1199 | `		/* Insert the value */` |
|     81 | 1200 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1201 | `		/* Reset the string cursor */` |
|     81 | 1202 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1203 | `	}` |
|      - | 1204 | `	/*` |
|      - | 1205 | `	 * Return the array.` |
|      - | 1206 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1207 | `	 * released upon we return from this function.` |
|      - | 1208 | `	 */` |
|      5 | 1209 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1210 | `	return PH7_OK;` |
|      3 | 1211 |  |
|      - | 1212 | `/*` |
|      - | 1213 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1214 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1215 | ` * Parameters` |
|      - | 1216 | ` * $string` |
|      - | 1217 | ` *   The input string.` |
|      - | 1218 | ` * $flags` |
|      - | 1219 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1220 | ` * Return` |
|      - | 1221 | ` * The encoded string.` |
|      - | 1222 | ` */` |
|     10 | 1223 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1224 |  |
|     11 | 1225 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1226 | `	const char *zIn,*zEnd;` |
|      - | 1227 | `	int nLen,c;` |
|      - | 1228 | `	sxu32 n;` |
|     11 | 1229 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1230 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1231 | `		ph7_result_null(pCtx);` |
|      5 | 1232 | `		return PH7_OK;` |
|      - | 1233 | `	}` |
|      - | 1234 | `	/* Extract the target string */` |
|      7 | 1235 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1236 | `	/* Handle empty string up front */` |
|      7 | 1237 | `	if( nLen == 0 ){` |
|      3 | 1238 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1239 | `		return PH7_OK;` |
|      - | 1240 | `	}` |
|      5 | 1241 | `	zEnd = &zIn[nLen];` |
|      - | 1242 | `	/* Extract the flags if available */` |
|      5 | 1243 | `	if( nArg > 1 ){` |
|      3 | 1244 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1245 | `		if( iFlags < 0 ){` |
|      3 | 1246 | `			iFlags = 0x01;` |
|      1 | 1247 | `		}` |
|      1 | 1248 | `	}` |
|      - | 1249 | `	/* Perform the requested operation */` |
|     11 | 1250 | `	for(;;){` |
|     23 | 1251 | `		if( zIn >= zEnd ){` |
|      - | 1252 | `			/* No more input to process */` |
|      5 | 1253 | `			break;` |
|      - | 1254 | `		}` |
|     19 | 1255 | `		c = zIn[0];` |
|      - | 1256 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1257 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1258 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1259 | `				/* Got one */` |
|      9 | 1260 | `				break;` |
|      - | 1261 | `			}` |
|    108 | 1262 | `		}` |
|     19 | 1263 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1264 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1265 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1266 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 1267 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 1268 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 1269 | `				/* expand single quote verbatim */` |
|    ! 0 | 1270 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 1271 | `			}else{` |
|      9 | 1272 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 1273 | `			}` |
|      5 | 1274 | `		}else{` |
|      - | 1275 | `			/* Output character verbatim */` |
|     11 | 1276 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1277 | `		}` |
|     19 | 1278 | `		zIn++;` |
|      1 | 1279 | `	}` |
|      5 | 1280 | `	return PH7_OK;` |
|      6 | 1281 |  |
|      - | 1282 | `/*` |
|      - | 1283 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 1284 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 1285 | ` * Parameters` |
|      - | 1286 | ` * $string` |
|      - | 1287 | ` *   The input string.` |
|      - | 1288 | ` * $flags` |
|      - | 1289 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 1290 | ` * Return` |
|      - | 1291 | ` * The decoded string.` |
|      - | 1292 | ` */` |
|     28 | 1293 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1294 |  |
|      - | 1295 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 1296 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 1297 | `	int nLen;` |
|      - | 1298 | `	sxu32 n;` |
|     29 | 1299 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1300 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1301 | `		ph7_result_null(pCtx);` |
|      5 | 1302 | `		return PH7_OK;` |
|      - | 1303 | `	}` |
|      - | 1304 | `	/* Extract the target string */` |
|     25 | 1305 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1306 | `	zEnd = &zIn[nLen];` |
|      - | 1307 | `	/* Extract the flags if available */` |
|     25 | 1308 | `	if( nArg > 1 ){` |
|     15 | 1309 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 1310 | `		if( iFlags < 0 ){` |
|      3 | 1311 | `			iFlags = 0x01;` |
|      1 | 1312 | `		}` |
|      7 | 1313 | `	}` |
|      - | 1314 | `	/* Perform the requested operation */` |
|     27 | 1315 | `	for(;;){` |
|     55 | 1316 | `		if( zIn >= zEnd ){` |
|      - | 1317 | `			/* No more input to process */` |
|     13 | 1318 | `			break;` |
|      - | 1319 | `		}` |
|     43 | 1320 | `		zCur = zIn;` |
|    173 | 1321 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 1322 | `			zIn++;` |
|      1 | 1323 | `		}` |
|     43 | 1324 | `		if( zCur < zIn ){` |
|      - | 1325 | `			/* Append raw string verbatim */` |
|     27 | 1326 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 1327 | `		}` |
|     43 | 1328 | `		if( zIn >= zEnd ){` |
|     13 | 1329 | `			break;` |
|      - | 1330 | `		}` |
|     31 | 1331 | `		nLen = (int)(zEnd-zIn);` |
|      - | 1332 | `		/* Find an encoded sequence */` |
|    113 | 1333 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 1334 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 1335 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 1336 | `				/* Got one */` |
|     31 | 1337 | `				zIn += iLen;` |
|     31 | 1338 | `				break;` |
|      - | 1339 | `			}` |
|     42 | 1340 | `		}` |
|     31 | 1341 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 1342 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 1343 | `			/* Output the decoded character */` |
|     31 | 1344 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 1345 | `				/* Do not process single quotes */` |
|      9 | 1346 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 1347 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1348 | `				/* Do not process double quotes */` |
|      5 | 1349 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 1350 | `			}else{` |
|     19 | 1351 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 1352 | `			}` |
|     16 | 1353 | `		}else{` |
|      - | 1354 | `			/* Append '&' */` |
|    ! 0 | 1355 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1356 | `			zIn++;` |
|      - | 1357 | `		}` |
|      1 | 1358 | `	}` |
|     25 | 1359 | `	return PH7_OK;` |
|     15 | 1360 |  |
|      - | 1361 | `/*` |
|      - | 1362 | ` * int strlen($string)` |
|      - | 1363 | ` *  return the length of the given string.` |
|      - | 1364 | ` * Parameter` |
|      - | 1365 | ` *  string: The string being measured for length.` |
|      - | 1366 | ` * Return` |
|      - | 1367 | ` *  length of the given string.` |
|      - | 1368 | ` */` |
|   5212 | 1369 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1370 |  |
|   5214 | 1371 | `	int iLen = 0;` |
|   5214 | 1372 | `	if( nArg > 0 ){` |
|   5212 | 1373 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   2605 | 1374 | `	}` |
|      - | 1375 | `	/* String length */` |
|   5214 | 1376 | `	ph7_result_int(pCtx,iLen);` |
|   5214 | 1377 | `	return PH7_OK;` |
|      2 | 1378 |  |
|      - | 1379 | `/*` |
|      - | 1380 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1381 | ` *  Perform a binary safe string comparison.` |
|      - | 1382 | ` * Parameter` |
|      - | 1383 | ` *  str1: The first string` |
|      - | 1384 | ` *  str2: The second string` |
|      - | 1385 | ` * Return` |
|      - | 1386 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1387 | ` *  than str2, and 0 if they are equal.` |
|      - | 1388 | ` */` |
|     80 | 1389 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1390 |  |
|      - | 1391 | `	const char *z1,*z2;` |
|      - | 1392 | `	int n1,n2;` |
|      - | 1393 | `	int res;` |
|     81 | 1394 | `	if( nArg < 2 ){` |
|      5 | 1395 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 1396 | `		ph7_result_int(pCtx,res);` |
|      5 | 1397 | `		return PH7_OK;` |
|      - | 1398 | `	}` |
|      - | 1399 | `	/* Perform the comparison */` |
|     77 | 1400 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     77 | 1401 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     77 | 1402 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1403 | `	/* Comparison result */` |
|     77 | 1404 | `	ph7_result_int(pCtx,res);` |
|     77 | 1405 | `	return PH7_OK;` |
|     41 | 1406 |  |
|      - | 1407 | `/*` |
|      - | 1408 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1409 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1410 | ` * Parameter` |
|      - | 1411 | ` *  str1: The first string` |
|      - | 1412 | ` *  str2: The second string` |
|      - | 1413 | ` * Return` |
|      - | 1414 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1415 | ` *  than str2, and 0 if they are equal.` |
|      - | 1416 | ` */` |
|     20 | 1417 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1418 |  |
|      - | 1419 | `	const char *z1,*z2;` |
|      - | 1420 | `	int res;` |
|      - | 1421 | `	int n;` |
|     21 | 1422 | `	if( nArg < 3 ){` |
|      - | 1423 | `		/* Perform a standard comparison */` |
|      5 | 1424 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1425 | `	}` |
|      - | 1426 | `	/* Desired comparison length */` |
|     17 | 1427 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1428 | `	if( n < 0 ){` |
|      - | 1429 | `		/* Invalid length */` |
|      3 | 1430 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1431 | `		return PH7_OK;` |
|      - | 1432 | `	}` |
|      - | 1433 | `	/* Perform the comparison */` |
|     15 | 1434 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1435 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1436 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1437 | `	/* Comparison result */` |
|     15 | 1438 | `	ph7_result_int(pCtx,res);` |
|     15 | 1439 | `	return PH7_OK;` |
|     11 | 1440 |  |
|      - | 1441 | `/*` |
|      - | 1442 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1443 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1444 | ` * Parameter` |
|      - | 1445 | ` *  str1: The first string` |
|      - | 1446 | ` *  str2: The second string` |
|      - | 1447 | ` * Return` |
|      - | 1448 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1449 | ` *  than str2, and 0 if they are equal.` |
|      - | 1450 | ` */` |
|     22 | 1451 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1452 |  |
|      - | 1453 | `	const char *z1,*z2;` |
|      - | 1454 | `	int n1,n2;` |
|      - | 1455 | `	int res;` |
|     23 | 1456 | `	if( nArg < 2 ){` |
|      9 | 1457 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 1458 | `		ph7_result_int(pCtx,res);` |
|      9 | 1459 | `		return PH7_OK;` |
|      - | 1460 | `	}` |
|      - | 1461 | `	/* Perform the comparison */` |
|     15 | 1462 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1463 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1464 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1465 | `	/* Comparison result */` |
|     15 | 1466 | `	ph7_result_int(pCtx,res);` |
|     15 | 1467 | `	return PH7_OK;` |
|     12 | 1468 |  |
|      - | 1469 | `/*` |
|      - | 1470 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1471 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1472 | ` * Parameter` |
|      - | 1473 | ` *  $str1: The first string` |
|      - | 1474 | ` *  $str2: The second string` |
|      - | 1475 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1476 | ` * Return` |
|      - | 1477 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1478 | ` *  than str2, and 0 if they are equal.` |
|      - | 1479 | ` */` |
|      8 | 1480 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1481 |  |
|      - | 1482 | `	const char *z1,*z2;` |
|      - | 1483 | `	int res;` |
|      - | 1484 | `	int n;` |
|      9 | 1485 | `	if( nArg < 3 ){` |
|      - | 1486 | `		/* Perform a standard comparison */` |
|      5 | 1487 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1488 | `	}` |
|      - | 1489 | `	/* Desired comparison length */` |
|      5 | 1490 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1491 | `	if( n < 0 ){` |
|      - | 1492 | `		/* Invalid length */` |
|    ! 0 | 1493 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1494 | `		return PH7_OK;` |
|      - | 1495 | `	}` |
|      - | 1496 | `	/* Perform the comparison */` |
|      5 | 1497 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1498 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1499 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1500 | `	/* Comparison result */` |
|      5 | 1501 | `	ph7_result_int(pCtx,res);` |
|      5 | 1502 | `	return PH7_OK;` |
|      5 | 1503 |  |
|      - | 1504 | `/*` |
|      - | 1505 | ` * Implode context [i.e: it's private data].` |
|      - | 1506 | ` * A pointer to the following structure is forwarded` |
|      - | 1507 | ` * verbatim to the array walker callback defined below.` |
|      - | 1508 | ` */` |
|      - | 1509 | `struct implode_data {` |
|      - | 1510 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1511 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1512 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1513 | `	int nSeplen;          /* Separator length */` |
|      - | 1514 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1515 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1516 | `};` |
|      - | 1517 | `/*` |
|      - | 1518 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1519 | ` * The following routine is invoked for each array entry passed` |
|      - | 1520 | ` * to the implode() function.` |
|      - | 1521 | ` */` |
| 114210 | 1522 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 1523 |  |
|  57105 | 1524 | `	SXUNUSED(pKey);` |
| 114212 | 1525 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1526 | `	const char *zData;` |
|      - | 1527 | `	int nLen;` |
| 114212 | 1528 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1529 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1530 | `			if( !pData->bFirst ){` |
|      - | 1531 | `				/* append the separator first */` |
|      3 | 1532 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 1533 | `			}else{` |
|    ! 0 | 1534 | `				pData->bFirst = 0;` |
|      - | 1535 | `			}` |
|      1 | 1536 | `		}` |
|      - | 1537 | `		/* Recurse */` |
|      3 | 1538 | `		pData->bFirst = 1;` |
|      3 | 1539 | `		pData->nRecCount++;` |
|      3 | 1540 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1541 | `		pData->nRecCount--;` |
|      3 | 1542 | `		return PH7_OK;` |
|      - | 1543 | `	}` |
|      - | 1544 | `	/* Extract the string representation of the entry value */` |
| 114210 | 1545 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1546 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 114210 | 1547 | `	if( pData->bFirst ){` |
|  28606 | 1548 | `		pData->bFirst = 0;` |
|  99908 | 1549 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1550 | `		/* append the separator first */` |
|  85594 | 1551 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  42796 | 1552 | `	}` |
|      - | 1553 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 114210 | 1554 | `	if( nLen > 0 ){` |
| 103574 | 1555 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  51786 | 1556 | `	}` |
| 114210 | 1557 | `	return PH7_OK;` |
|  57107 | 1558 |  |
|      - | 1559 | `/*` |
|      - | 1560 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1561 | ` * string implode(array $pieces,...)` |
|      - | 1562 | ` *  Join array elements with a string.` |
|      - | 1563 | ` * $glue` |
|      - | 1564 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1565 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1566 | ` * $pieces` |
|      - | 1567 | ` *   The array of strings to implode.` |
|      - | 1568 | ` * Return` |
|      - | 1569 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1570 | ` *  order, with the glue string between each element.` |
|      - | 1571 | ` */` |
|  28628 | 1572 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1573 |  |
|      - | 1574 | `	struct implode_data imp_data;` |
|  28630 | 1575 | `	int i = 1;` |
|  28630 | 1576 | `	if( nArg < 1 ){` |
|      - | 1577 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1578 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1579 | `		return PH7_OK;` |
|      - | 1580 | `	}` |
|      - | 1581 | `	/* Prepare the implode context */` |
|  28630 | 1582 | `	imp_data.pCtx = pCtx;` |
|  28630 | 1583 | `	imp_data.bRecursive = 0;` |
|  28630 | 1584 | `	imp_data.bFirst = 1;` |
|  28630 | 1585 | `	imp_data.nRecCount = 0;` |
|  28630 | 1586 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  28628 | 1587 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  14315 | 1588 | `	}else{` |
|      3 | 1589 | `		imp_data.zSep = 0;` |
|      3 | 1590 | `		imp_data.nSeplen = 0;` |
|      3 | 1591 | `		i = 0;` |
|      - | 1592 | `	}` |
|  28630 | 1593 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1594 | `	/* Start the 'join' process */` |
|  57258 | 1595 | `	while( i < nArg ){` |
|  28630 | 1596 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1597 | `			/* Iterate throw array entries */` |
|  28630 | 1598 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  14316 | 1599 | `		}else{` |
|      - | 1600 | `			const char *zData;` |
|      - | 1601 | `			int nLen;` |
|      - | 1602 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1603 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1604 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1605 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1606 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1607 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1608 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1609 | `			}` |
|      - | 1610 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1611 | `			if( nLen > 0 ){` |
|    ! 0 | 1612 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 1613 | `			}` |
|      - | 1614 | `		}` |
|  28630 | 1615 | `		i++;` |
|      2 | 1616 | `	}` |
|  28630 | 1617 | `	return PH7_OK;` |
|  14316 | 1618 |  |
|      - | 1619 | `/*` |
|      - | 1620 | ` * Symisc eXtension:` |
|      - | 1621 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1622 | ` * Purpose` |
|      - | 1623 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1624 | ` * Example:` |
|      - | 1625 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1626 | ` *   echo implode_recursive("/",$a);` |
|      - | 1627 | ` *   Will output` |
|      - | 1628 | ` *     usr/home/dean.` |
|      - | 1629 | ` *   While the standard implode would produce.` |
|      - | 1630 | ` *    usr/Array.` |
|      - | 1631 | ` * Parameter` |
|      - | 1632 | ` *  Refer to implode().` |
|      - | 1633 | ` * Return` |
|      - | 1634 | ` *  Refer to implode().` |
|      - | 1635 | ` */` |
|     12 | 1636 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1637 |  |
|      - | 1638 | `	struct implode_data imp_data;` |
|     13 | 1639 | `	int i = 1;` |
|     13 | 1640 | `	if( nArg < 1 ){` |
|      - | 1641 | `		/* Missing argument,return NULL */` |
|      3 | 1642 | `		ph7_result_null(pCtx);` |
|      3 | 1643 | `		return PH7_OK;` |
|      - | 1644 | `	}` |
|      - | 1645 | `	/* Prepare the implode context */` |
|     11 | 1646 | `	imp_data.pCtx = pCtx;` |
|     11 | 1647 | `	imp_data.bRecursive = 1;` |
|     11 | 1648 | `	imp_data.bFirst = 1;` |
|     11 | 1649 | `	imp_data.nRecCount = 0;` |
|     11 | 1650 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1651 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1652 | `	}else{` |
|    ! 0 | 1653 | `		imp_data.zSep = 0;` |
|    ! 0 | 1654 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1655 | `		i = 0;` |
|      - | 1656 | `	}` |
|     11 | 1657 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1658 | `	/* Start the 'join' process */` |
|     21 | 1659 | `	while( i < nArg ){` |
|     11 | 1660 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1661 | `			/* Iterate throw array entries */` |
|      3 | 1662 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 1663 | `		}else{` |
|      - | 1664 | `			const char *zData;` |
|      - | 1665 | `			int nLen;` |
|      - | 1666 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1667 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1668 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1669 | `			if( imp_data.bFirst ){` |
|      9 | 1670 | `				imp_data.bFirst = 0;` |
|      4 | 1671 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1672 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1673 | `			}` |
|      - | 1674 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1675 | `			if( nLen > 0 ){` |
|      9 | 1676 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 1677 | `			}` |
|      - | 1678 | `		}` |
|     11 | 1679 | `		i++;` |
|      1 | 1680 | `	}` |
|     11 | 1681 | `	return PH7_OK;` |
|      7 | 1682 |  |
|      - | 1683 | `/*` |
|      - | 1684 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1685 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1686 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1687 | ` * Parameters` |
|      - | 1688 | ` *  $delimiter` |
|      - | 1689 | ` *   The boundary string.` |
|      - | 1690 | ` * $string` |
|      - | 1691 | ` *   The input string.` |
|      - | 1692 | ` * $limit` |
|      - | 1693 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1694 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1695 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1696 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1697 | ` * Returns` |
|      - | 1698 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1699 | ` *  on boundaries formed by the delimiter.` |
|      - | 1700 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1701 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1702 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1703 | ` *  will be returned.` |
|      - | 1704 | ` * NOTE:` |
|      - | 1705 | ` *  Negative limit is not supported.` |
|      - | 1706 | ` */` |
|   5380 | 1707 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1708 |  |
|      - | 1709 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1710 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1711 | `	ph7_value *pArray;` |
|      - | 1712 | `	ph7_value *pValue;` |
|      - | 1713 | `	sxu32 nOfft;` |
|      - | 1714 | `	sxi32 rc;` |
|   5382 | 1715 | `	if( nArg < 2 ){` |
|      - | 1716 | `		/* Missing arguments,return FALSE */` |
|      9 | 1717 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1718 | `		return PH7_OK;` |
|      - | 1719 | `	}` |
|      - | 1720 | `	/* Extract the delimiter */` |
|   5374 | 1721 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   5374 | 1722 | `	if( nDelim < 1 ){` |
|      - | 1723 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1724 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1725 | `		return PH7_OK;` |
|      - | 1726 | `	}` |
|      - | 1727 | `	/* Extract the string */` |
|   5372 | 1728 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   5372 | 1729 | `	if( nStrlen < 1 ){` |
|      - | 1730 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 1731 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 1732 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 1733 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 1734 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1735 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1736 | `			return PH7_OK;` |
|      - | 1737 | `		}` |
|      3 | 1738 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 1739 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 1740 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 1741 | `		return PH7_OK;` |
|      - | 1742 | `	}` |
|      - | 1743 | `	/* Point to the end of the string */` |
|   5370 | 1744 | `	zEnd = &zString[nStrlen];` |
|      - | 1745 | `	/* Create the array */` |
|   5370 | 1746 | `	pArray =  ph7_context_new_array(pCtx);` |
|   5370 | 1747 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   5370 | 1748 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1749 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1750 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1751 | `		return PH7_OK;` |
|      - | 1752 | `	}` |
|      - | 1753 | `	/* Set a defualt limit */` |
|   5370 | 1754 | `	iLimit = SXI32_HIGH;` |
|   5370 | 1755 | `	if( nArg > 2 ){` |
|     11 | 1756 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     11 | 1757 | `		 if( iLimit < 0 ){` |
|      3 | 1758 | `			iLimit = -iLimit;` |
|      1 | 1759 | `		}` |
|     11 | 1760 | `		if( iLimit == 0 ){` |
|      3 | 1761 | `			iLimit = 1;` |
|      1 | 1762 | `		}` |
|     11 | 1763 | `		iLimit--;` |
|      5 | 1764 | `	}` |
|      - | 1765 | `	/* Start exploding */` |
|  60932 | 1766 | `	for(;;){` |
| 121866 | 1767 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 121866 | 1768 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1769 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   5370 | 1770 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   5370 | 1771 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   5370 | 1772 | `			break;` |
|      - | 1773 | `		}` |
|      - | 1774 | `		/* Point to the desired offset */` |
| 116498 | 1775 | `		zCur = &zString[nOfft];` |
|      - | 1776 | `		/* Perform the store operation (may be empty) */` |
| 116498 | 1777 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 116498 | 1778 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 1779 | `		/* Point beyond the delimiter */` |
| 116498 | 1780 | `		zString = &zCur[nDelim];` |
|      - | 1781 | `		/* Reset the cursor */` |
| 116498 | 1782 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 1783 | `	}` |
|      - | 1784 | `	/* Return the freshly created array */` |
|   5370 | 1785 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1786 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1787 | `	 * released as soon we return from this foregin function.` |
|      - | 1788 | `	 */` |
|   5370 | 1789 | `	return PH7_OK;` |
|   2692 | 1790 |  |
|      - | 1791 | `/*` |
|      - | 1792 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1793 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1794 | ` * Parameters` |
|      - | 1795 | ` *  $str` |
|      - | 1796 | ` *   The string that will be trimmed.` |
|      - | 1797 | ` * $charlist` |
|      - | 1798 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1799 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1800 | ` *   With .. you can specify a range of characters.` |
|      - | 1801 | ` * Returns.` |
|      - | 1802 | ` *  Thr processed string.` |
|      - | 1803 | ` * NOTE:` |
|      - | 1804 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1805 | ` */` |
|  12394 | 1806 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1807 |  |
|      - | 1808 | `	const char *zString;` |
|      - | 1809 | `	int nLen;` |
|  12396 | 1810 | `	if( nArg < 1 ){` |
|      - | 1811 | `		/* Missing arguments,return null */` |
|      3 | 1812 | `		ph7_result_null(pCtx);` |
|      3 | 1813 | `		return PH7_OK;` |
|      - | 1814 | `	}` |
|      - | 1815 | `	/* Extract the target string */` |
|  12394 | 1816 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  12394 | 1817 | `	if( nLen < 1 ){` |
|      - | 1818 | `		/* Empty string,return */` |
|   1646 | 1819 | `		ph7_result_string(pCtx,"",0);` |
|   1646 | 1820 | `		return PH7_OK;` |
|      - | 1821 | `	}` |
|      - | 1822 | `	/* Start the trim process */` |
|  10750 | 1823 | `	if( nArg < 2 ){` |
|      - | 1824 | `		SyString sStr;` |
|      - | 1825 | `		/* Remove white spaces and NUL bytes */` |
|  10746 | 1826 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  26156 | 1827 | `		SyStringFullTrimSafe(&sStr);` |
|  10746 | 1828 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   5374 | 1829 | `	}else{` |
|      - | 1830 | `		/* Char list */` |
|      - | 1831 | `		const char *zList;` |
|      - | 1832 | `		int nListlen;` |
|      5 | 1833 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1834 | `		if( nListlen < 1 ){` |
|      - | 1835 | `			/* Return the string unchanged */` |
|      3 | 1836 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1837 | `		}else{` |
|      3 | 1838 | `			const char *zEnd = &zString[nLen];` |
|      3 | 1839 | `			const char *zCur = zString;` |
|      - | 1840 | `			const char *zPtr;` |
|      - | 1841 | `			int i;` |
|      - | 1842 | `			/* Left trim */` |
|      4 | 1843 | `			for(;;){` |
|      9 | 1844 | `				if( zCur >= zEnd ){` |
|    ! 0 | 1845 | `					break;` |
|      - | 1846 | `				}` |
|      9 | 1847 | `				zPtr = zCur;` |
|     17 | 1848 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1849 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 1850 | `						zCur++;` |
|      3 | 1851 | `					}` |
|      5 | 1852 | `				}` |
|      9 | 1853 | `				if( zCur == zPtr ){` |
|      - | 1854 | `					/* No match,break immediately */` |
|      3 | 1855 | `					break;` |
|      - | 1856 | `				}` |
|      1 | 1857 | `			}` |
|      - | 1858 | `			/* Right trim */` |
|      3 | 1859 | `			zEnd--;` |
|      4 | 1860 | `			for(;;){` |
|      9 | 1861 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1862 | `					break;` |
|      - | 1863 | `				}` |
|      9 | 1864 | `				zPtr = zEnd;` |
|     17 | 1865 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1866 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 1867 | `						zEnd--;` |
|      3 | 1868 | `					}` |
|      5 | 1869 | `				}` |
|      9 | 1870 | `				if( zEnd == zPtr ){` |
|      3 | 1871 | `					break;` |
|      - | 1872 | `				}` |
|      1 | 1873 | `			}` |
|      3 | 1874 | `			if( zCur >= zEnd ){` |
|      - | 1875 | `				/* Return the empty string */` |
|    ! 0 | 1876 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1877 | `			}else{` |
|      3 | 1878 | `				zEnd++;` |
|      3 | 1879 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1880 | `			}` |
|      - | 1881 | `		}` |
|      - | 1882 | `	}` |
|  10750 | 1883 | `	return PH7_OK;` |
|   6199 | 1884 |  |
|      - | 1885 | `/*` |
|      - | 1886 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1887 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1888 | ` * Parameters` |
|      - | 1889 | ` *  $str` |
|      - | 1890 | ` *   The string that will be trimmed.` |
|      - | 1891 | ` * $charlist` |
|      - | 1892 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1893 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1894 | ` *   With .. you can specify a range of characters.` |
|      - | 1895 | ` * Returns.` |
|      - | 1896 | ` *  Thr processed string.` |
|      - | 1897 | ` * NOTE:` |
|      - | 1898 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1899 | ` */` |
|     26 | 1900 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1901 |  |
|      - | 1902 | `	const char *zString;` |
|      - | 1903 | `	int nLen;` |
|     27 | 1904 | `	if( nArg < 1 ){` |
|      - | 1905 | `		/* Missing arguments,return null */` |
|      3 | 1906 | `		ph7_result_null(pCtx);` |
|      3 | 1907 | `		return PH7_OK;` |
|      - | 1908 | `	}` |
|      - | 1909 | `	/* Extract the target string */` |
|     25 | 1910 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1911 | `	if( nLen < 1 ){` |
|      - | 1912 | `		/* Empty string,return */` |
|      5 | 1913 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1914 | `		return PH7_OK;` |
|      - | 1915 | `	}` |
|      - | 1916 | `	/* Start the trim process */` |
|     21 | 1917 | `	if( nArg < 2 ){` |
|      - | 1918 | `		SyString sStr;` |
|      - | 1919 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1920 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1921 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1922 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1923 | `	}else{` |
|      - | 1924 | `		/* Char list */` |
|      - | 1925 | `		const char *zList;` |
|      - | 1926 | `		int nListlen;` |
|      5 | 1927 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1928 | `		if( nListlen < 1 ){` |
|      - | 1929 | `			/* Return the string unchanged */` |
|    ! 0 | 1930 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1931 | `		}else{` |
|      5 | 1932 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 1933 | `			const char *zCur = zString;` |
|      - | 1934 | `			const char *zPtr;` |
|      - | 1935 | `			int i;` |
|      - | 1936 | `			/* Right trim */` |
|      6 | 1937 | `			for(;;){` |
|     13 | 1938 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1939 | `					break;` |
|      - | 1940 | `				}` |
|     13 | 1941 | `				zPtr = zEnd;` |
|     25 | 1942 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 1943 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 1944 | `						zEnd--;` |
|      4 | 1945 | `					}` |
|      7 | 1946 | `				}` |
|     13 | 1947 | `				if( zEnd == zPtr ){` |
|      5 | 1948 | `					break;` |
|      - | 1949 | `				}` |
|      1 | 1950 | `			}` |
|      5 | 1951 | `			if( zEnd <= zCur ){` |
|      - | 1952 | `				/* Return the empty string */` |
|    ! 0 | 1953 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1954 | `			}else{` |
|      5 | 1955 | `				zEnd++;` |
|      5 | 1956 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1957 | `			}` |
|      - | 1958 | `		}` |
|      - | 1959 | `	}` |
|     21 | 1960 | `	return PH7_OK;` |
|     14 | 1961 |  |
|      - | 1962 | `/*` |
|      - | 1963 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 1964 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1965 | ` * Parameters` |
|      - | 1966 | ` *  $str` |
|      - | 1967 | ` *   The string that will be trimmed.` |
|      - | 1968 | ` * $charlist` |
|      - | 1969 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1970 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1971 | ` *   With .. you can specify a range of characters.` |
|      - | 1972 | ` * Returns.` |
|      - | 1973 | ` *  Thr processed string.` |
|      - | 1974 | ` * NOTE:` |
|      - | 1975 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1976 | ` */` |
|     12 | 1977 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1978 |  |
|      - | 1979 | `	const char *zString;` |
|      - | 1980 | `	int nLen;` |
|     13 | 1981 | `	if( nArg < 1 ){` |
|      - | 1982 | `		/* Missing arguments,return null */` |
|      3 | 1983 | `		ph7_result_null(pCtx);` |
|      3 | 1984 | `		return PH7_OK;` |
|      - | 1985 | `	}` |
|      - | 1986 | `	/* Extract the target string */` |
|     11 | 1987 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1988 | `	if( nLen < 1 ){` |
|      - | 1989 | `		/* Empty string,return */` |
|    ! 0 | 1990 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1991 | `		return PH7_OK;` |
|      - | 1992 | `	}` |
|      - | 1993 | `	/* Start the trim process */` |
|     11 | 1994 | `	if( nArg < 2 ){` |
|      - | 1995 | `		SyString sStr;` |
|      - | 1996 | `		/* Remove white spaces and NUL byte */` |
|      3 | 1997 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 1998 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 1999 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2000 | `	}else{` |
|      - | 2001 | `		/* Char list */` |
|      - | 2002 | `		const char *zList;` |
|      - | 2003 | `		int nListlen;` |
|      9 | 2004 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 2005 | `		if( nListlen < 1 ){` |
|      - | 2006 | `			/* Return the string unchanged */` |
|      3 | 2007 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2008 | `		}else{` |
|      7 | 2009 | `			const char *zEnd = &zString[nLen];` |
|      7 | 2010 | `			const char *zCur = zString;` |
|      - | 2011 | `			const char *zPtr;` |
|      - | 2012 | `			int i;` |
|      - | 2013 | `			/* Left trim */` |
|      7 | 2014 | `			for(;;){` |
|     15 | 2015 | `				if( zCur >= zEnd ){` |
|    ! 0 | 2016 | `					break;` |
|      - | 2017 | `				}` |
|     15 | 2018 | `				zPtr = zCur;` |
|     41 | 2019 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 2020 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 2021 | `						zCur++;` |
|      6 | 2022 | `					}` |
|     14 | 2023 | `				}` |
|     15 | 2024 | `				if( zCur == zPtr ){` |
|      - | 2025 | `					/* No match,break immediately */` |
|      7 | 2026 | `					break;` |
|      - | 2027 | `				}` |
|      1 | 2028 | `			}` |
|      7 | 2029 | `			if( zCur >= zEnd ){` |
|      - | 2030 | `				/* Return the empty string */` |
|    ! 0 | 2031 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2032 | `			}else{` |
|      7 | 2033 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2034 | `			}` |
|      - | 2035 | `		}` |
|      - | 2036 | `	}` |
|     11 | 2037 | `	return PH7_OK;` |
|      7 | 2038 |  |
|      - | 2039 | `/*` |
|      - | 2040 | ` * string strtolower(string $str)` |
|      - | 2041 | ` *  Make a string lowercase.` |
|      - | 2042 | ` * Parameters` |
|      - | 2043 | ` *  $str` |
|      - | 2044 | ` *   The input string.` |
|      - | 2045 | ` * Returns.` |
|      - | 2046 | ` *  The lowercased string.` |
|      - | 2047 | ` */` |
|  28340 | 2048 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2049 |  |
|      - | 2050 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2051 | `	int nLen;` |
|  28342 | 2052 | `	if( nArg < 1 ){` |
|      - | 2053 | `		/* Missing arguments,return null */` |
|      3 | 2054 | `		ph7_result_null(pCtx);` |
|      3 | 2055 | `		return PH7_OK;` |
|      - | 2056 | `	}` |
|      - | 2057 | `	/* Extract the target string */` |
|  28340 | 2058 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  28340 | 2059 | `	if( nLen < 1 ){` |
|      - | 2060 | `		/* Empty string,return */` |
|      3 | 2061 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2062 | `		return PH7_OK;` |
|      - | 2063 | `	}` |
|      - | 2064 | `	/* Perform the requested operation */` |
|  28338 | 2065 | `	zEnd = &zString[nLen];` |
|  89258 | 2066 | `	for(;;){` |
| 178518 | 2067 | `		if( zString >= zEnd ){` |
|      - | 2068 | `			/* No more input,break immediately */` |
|  28338 | 2069 | `			break;` |
|      - | 2070 | `		}` |
| 150182 | 2071 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2072 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2073 | `			zCur = zString;` |
|    ! 0 | 2074 | `			zString++;` |
|    ! 0 | 2075 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2076 | `				zString++;` |
|    ! 0 | 2077 | `			}` |
|      - | 2078 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2079 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2080 | `		}else{` |
| 150182 | 2081 | `			int c = zString[0];` |
| 150182 | 2082 | `			if( SyisUpper(c) ){` |
| 150180 | 2083 | `				c = SyToLower(zString[0]);` |
|  75089 | 2084 | `			}` |
|      - | 2085 | `			/* Append character */` |
| 150182 | 2086 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2087 | `			/* Advance the cursor */` |
| 150182 | 2088 | `			zString++;` |
|      - | 2089 | `		}` |
|      2 | 2090 | `	}` |
|  28338 | 2091 | `	return PH7_OK;` |
|  14172 | 2092 |  |
|      - | 2093 | `/*` |
|      - | 2094 | ` * string strtolower(string $str)` |
|      - | 2095 | ` *  Make a string uppercase.` |
|      - | 2096 | ` * Parameters` |
|      - | 2097 | ` *  $str` |
|      - | 2098 | ` *   The input string.` |
|      - | 2099 | ` * Returns.` |
|      - | 2100 | ` *  The uppercased string.` |
|      - | 2101 | ` */` |
|     34 | 2102 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2103 |  |
|      - | 2104 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2105 | `	int nLen;` |
|     36 | 2106 | `	if( nArg < 1 ){` |
|      - | 2107 | `		/* Missing arguments,return null */` |
|      3 | 2108 | `		ph7_result_null(pCtx);` |
|      3 | 2109 | `		return PH7_OK;` |
|      - | 2110 | `	}` |
|      - | 2111 | `	/* Extract the target string */` |
|     34 | 2112 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     34 | 2113 | `	if( nLen < 1 ){` |
|      - | 2114 | `		/* Empty string,return */` |
|      3 | 2115 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2116 | `		return PH7_OK;` |
|      - | 2117 | `	}` |
|      - | 2118 | `	/* Perform the requested operation */` |
|     32 | 2119 | `	zEnd = &zString[nLen];` |
|     88 | 2120 | `	for(;;){` |
|    178 | 2121 | `		if( zString >= zEnd ){` |
|      - | 2122 | `			/* No more input,break immediately */` |
|     32 | 2123 | `			break;` |
|      - | 2124 | `		}` |
|    148 | 2125 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2126 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2127 | `			zCur = zString;` |
|    ! 0 | 2128 | `			zString++;` |
|    ! 0 | 2129 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2130 | `				zString++;` |
|    ! 0 | 2131 | `			}` |
|      - | 2132 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2133 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2134 | `		}else{` |
|    148 | 2135 | `			int c = zString[0];` |
|    148 | 2136 | `			if( SyisLower(c) ){` |
|    142 | 2137 | `				c = SyToUpper(zString[0]);` |
|     70 | 2138 | `			}` |
|      - | 2139 | `			/* Append character */` |
|    148 | 2140 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2141 | `			/* Advance the cursor */` |
|    148 | 2142 | `			zString++;` |
|      - | 2143 | `		}` |
|      2 | 2144 | `	}` |
|     32 | 2145 | `	return PH7_OK;` |
|     19 | 2146 |  |
|      - | 2147 | `/*` |
|      - | 2148 | ` * string ucfirst(string $str)` |
|      - | 2149 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2150 | ` *  character is alphabetic.` |
|      - | 2151 | ` * Parameters` |
|      - | 2152 | ` *  $str` |
|      - | 2153 | ` *   The input string.` |
|      - | 2154 | ` * Returns.` |
|      - | 2155 | ` *  The processed string.` |
|      - | 2156 | ` */` |
|      6 | 2157 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2158 |  |
|      - | 2159 | `	const char *zString,*zEnd;` |
|      - | 2160 | `	int nLen,c;` |
|      7 | 2161 | `	if( nArg < 1 ){` |
|      - | 2162 | `		/* Missing arguments,return null */` |
|      3 | 2163 | `		ph7_result_null(pCtx);` |
|      3 | 2164 | `		return PH7_OK;` |
|      - | 2165 | `	}` |
|      - | 2166 | `	/* Extract the target string */` |
|      5 | 2167 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2168 | `	if( nLen < 1 ){` |
|      - | 2169 | `		/* Empty string,return */` |
|      3 | 2170 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2171 | `		return PH7_OK;` |
|      - | 2172 | `	}` |
|      - | 2173 | `	/* Perform the requested operation */` |
|      3 | 2174 | `	zEnd = &zString[nLen];` |
|      3 | 2175 | `	c = zString[0];` |
|      3 | 2176 | `	if( SyisLower(c) ){` |
|      3 | 2177 | `		c = SyToUpper(c);` |
|      1 | 2178 | `	}` |
|      - | 2179 | `	/* Append the first character */` |
|      3 | 2180 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2181 | `	zString++;` |
|      3 | 2182 | `	if( zString < zEnd ){` |
|      - | 2183 | `		/* Append the rest of the input verbatim */` |
|      3 | 2184 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2185 | `	}` |
|      3 | 2186 | `	return PH7_OK;` |
|      4 | 2187 |  |
|      - | 2188 | `/*` |
|      - | 2189 | ` * string lcfirst(string $str)` |
|      - | 2190 | ` *  Make a string's first character lowercase.` |
|      - | 2191 | ` * Parameters` |
|      - | 2192 | ` *  $str` |
|      - | 2193 | ` *   The input string.` |
|      - | 2194 | ` * Returns.` |
|      - | 2195 | ` *  The processed string.` |
|      - | 2196 | ` */` |
|      6 | 2197 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2198 |  |
|      - | 2199 | `	const char *zString,*zEnd;` |
|      - | 2200 | `	int nLen,c;` |
|      7 | 2201 | `	if( nArg < 1 ){` |
|      - | 2202 | `		/* Missing arguments,return null */` |
|      3 | 2203 | `		ph7_result_null(pCtx);` |
|      3 | 2204 | `		return PH7_OK;` |
|      - | 2205 | `	}` |
|      - | 2206 | `	/* Extract the target string */` |
|      5 | 2207 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2208 | `	if( nLen < 1 ){` |
|      - | 2209 | `		/* Empty string,return */` |
|      3 | 2210 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2211 | `		return PH7_OK;` |
|      - | 2212 | `	}` |
|      - | 2213 | `	/* Perform the requested operation */` |
|      3 | 2214 | `	zEnd = &zString[nLen];` |
|      3 | 2215 | `	c = zString[0];` |
|      3 | 2216 | `	if( SyisUpper(c) ){` |
|      3 | 2217 | `		c = SyToLower(c);` |
|      1 | 2218 | `	}` |
|      - | 2219 | `	/* Append the first character */` |
|      3 | 2220 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2221 | `	zString++;` |
|      3 | 2222 | `	if( zString < zEnd ){` |
|      - | 2223 | `		/* Append the rest of the input verbatim */` |
|      3 | 2224 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2225 | `	}` |
|      3 | 2226 | `	return PH7_OK;` |
|      4 | 2227 |  |
|      - | 2228 | `/*` |
|      - | 2229 | ` * int ord(string $string)` |
|      - | 2230 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2231 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2232 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2233 | ` * Parameters` |
|      - | 2234 | ` *  $string` |
|      - | 2235 | ` *   The input string.` |
|      - | 2236 | ` * Returns` |
|      - | 2237 | ` *  The ASCII value as an integer.` |
|      - | 2238 | ` */` |
|     62 | 2239 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2240 |  |
|      - | 2241 | `	const char *zString;` |
|      - | 2242 | `	int nLen,c;` |
|      - | 2243 | `	/* PHP requires exactly one argument. */` |
|     64 | 2244 | `	if( nArg != 1 ){` |
|      7 | 2245 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2246 | `			"ArgumentCountError",` |
|      - | 2247 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2248 | `			nArg` |
|      - | 2249 | `			);` |
|      - | 2250 | `	}` |
|      - | 2251 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2252 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2253 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2254 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2255 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2256 | `			"of type string is deprecated"` |
|      - | 2257 | `			);` |
|      1 | 2258 | `	}` |
|      - | 2259 | `	/* Extract the target string */` |
|     59 | 2260 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2261 | `	if( nLen < 1 ){` |
|      - | 2262 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2263 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2264 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2265 | `			);` |
|      5 | 2266 | `		ph7_result_int(pCtx,0);` |
|      5 | 2267 | `		return PH7_OK;` |
|      - | 2268 | `	}` |
|      - | 2269 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2270 | `	if( nLen > 1 ){` |
|      7 | 2271 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2272 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2273 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2274 | `			);` |
|      3 | 2275 | `	}` |
|      - | 2276 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2277 | `	c = (unsigned char)zString[0];` |
|      - | 2278 | `	/* Return that value */` |
|     55 | 2279 | `	ph7_result_int(pCtx,c);` |
|     55 | 2280 | `	return PH7_OK;` |
|     33 | 2281 |  |
|      - | 2282 | `/*` |
|      - | 2283 | ` * string chr(int $codepoint)` |
|      - | 2284 | ` *  Returns a one-character string containing the character specified` |
|      - | 2285 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2286 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2287 | ` * Parameters` |
|      - | 2288 | ` *  $codepoint` |
|      - | 2289 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2290 | ` *   will be constrained to a single byte.` |
|      - | 2291 | ` * Returns` |
|      - | 2292 | ` *  A single-character string.` |
|      - | 2293 | ` */` |
|     44 | 2294 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2295 |  |
|      - | 2296 | `	int c;` |
|      - | 2297 | `	unsigned char ch;` |
|      - | 2298 | `	/* PHP requires exactly one argument. */` |
|     46 | 2299 | `	if( nArg != 1 ){` |
|      7 | 2300 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2301 | `			"ArgumentCountError",` |
|      - | 2302 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2303 | `			nArg` |
|      - | 2304 | `			);` |
|      - | 2305 | `	}` |
|      - | 2306 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2307 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2308 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2309 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     41 | 2310 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2311 | `		char zBuf[120];` |
|      4 | 2312 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2313 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2314 | `			ph7_value_to_double(apArg[0])` |
|      - | 2315 | `			);` |
|      3 | 2316 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2317 | `	}` |
|      - | 2318 | `	/* Extract the codepoint. */` |
|     41 | 2319 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2320 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2321 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2322 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2323 | `	 * name to avoid the API double-prefixing it. */` |
|     41 | 2324 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2325 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2326 | `			E_DEPRECATED,` |
|      - | 2327 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2328 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2329 | `			"The value used will be constrained using % 256"` |
|      - | 2330 | `			);` |
|      2 | 2331 | `	}` |
|      - | 2332 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2333 | `	 * when taking the address of a wider int. */` |
|     41 | 2334 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2335 | `	/* Return the specified character */` |
|     41 | 2336 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     41 | 2337 | `	return PH7_OK;` |
|     24 | 2338 |  |
|      - | 2339 | `/*` |
|      - | 2340 | ` * Binary to hex consumer callback.` |
|      - | 2341 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2342 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2343 | ` */` |
|    226 | 2344 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2345 |  |
|      - | 2346 | `	/* Append hex chunk verbatim */` |
|    227 | 2347 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 2348 | `	return SXRET_OK;` |
|      1 | 2349 |  |
|      - | 2350 |  |
|      - | 2351 | `/*` |
|      - | 2352 | ` * string bin2hex(string $str)` |
|      - | 2353 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2354 | ` * Parameters` |
|      - | 2355 | ` *  $str` |
|      - | 2356 | ` *   The input string.` |
|      - | 2357 | ` * Returns.` |
|      - | 2358 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2359 | ` */` |
|     20 | 2360 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2361 |  |
|      - | 2362 | `	const char *zString;` |
|      - | 2363 | `	int nLen;` |
|      - | 2364 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     22 | 2365 | `	if( nArg != 1 ){` |
|      7 | 2366 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2367 | `			"ArgumentCountError",` |
|      - | 2368 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2369 | `			nArg` |
|      - | 2370 | `			);` |
|      - | 2371 | `	}` |
|      - | 2372 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2373 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2374 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2375 | `	 */` |
|     25 | 2376 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     12 | 2377 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2378 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2379 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2380 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2381 | `		)` |
|      - | 2382 | `	){` |
|      7 | 2383 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      7 | 2384 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2385 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2386 | `			if( pInst && pInst->pClass ){` |
|      3 | 2387 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2388 | `			}` |
|      1 | 2389 | `		}` |
|     10 | 2390 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2391 | `			"TypeError",` |
|      - | 2392 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2393 | `			zType` |
|      - | 2394 | `			);` |
|      - | 2395 | `	}` |
|      - | 2396 | `	/* Extract the target string */` |
|     11 | 2397 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2398 | `	if( nLen < 1 ){` |
|      - | 2399 | `		/* Empty string,return */` |
|      3 | 2400 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2401 | `		return PH7_OK;` |
|      - | 2402 | `	}` |
|      - | 2403 | `	/* Perform the requested operation */` |
|      9 | 2404 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 2405 | `	return PH7_OK;` |
|     12 | 2406 |  |
|      - | 2407 |  |
|      - | 2408 | `/* Search callback signature */` |
|      - | 2409 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2410 | `/*` |
|      - | 2411 | ` * Case-insensitive pattern match.` |
|      - | 2412 | ` * Brute force is the default search method used here.` |
|      - | 2413 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2414 | ` * well for short/medium texts on modern hardware.` |
|      - | 2415 | ` */` |
|    118 | 2416 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2417 |  |
|    119 | 2418 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2419 | `	const char *zIn = (const char *)pText;` |
|    119 | 2420 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2421 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2422 | `	const char *zPtr,*zPtr2;` |
|      - | 2423 | `	int c,d;` |
|    119 | 2424 | `	if( iPatLen > nLen ){` |
|      - | 2425 | `		/* Don't bother processing */` |
|     33 | 2426 | `		return SXERR_NOTFOUND;` |
|      - | 2427 | `	}` |
|    244 | 2428 | `	for(;;){` |
|    489 | 2429 | `		if( zIn >= zEnd ){` |
|     47 | 2430 | `			break;` |
|      - | 2431 | `		}` |
|    443 | 2432 | `		c = SyToLower(zIn[0]);` |
|    443 | 2433 | `		d = SyToLower(zpIn[0]);` |
|    443 | 2434 | `		if( c == d ){` |
|     41 | 2435 | `			zPtr   = &zIn[1];` |
|     41 | 2436 | `			zPtr2  = &zpIn[1];` |
|     71 | 2437 | `			for(;;){` |
|    143 | 2438 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2439 | `					/* Pattern found */` |
|     41 | 2440 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2441 | `					return SXRET_OK;` |
|      - | 2442 | `				}` |
|    103 | 2443 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2444 | `					break;` |
|      - | 2445 | `				}` |
|    103 | 2446 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2447 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2448 | `				if( c != d ){` |
|    ! 0 | 2449 | `					break;` |
|      - | 2450 | `				}` |
|    103 | 2451 | `				zPtr++; zPtr2++;` |
|      1 | 2452 | `			}` |
|    ! 0 | 2453 | `		}` |
|    403 | 2454 | `		zIn++;` |
|      1 | 2455 | `	}` |
|      - | 2456 | `	/* Pattern not found */` |
|     47 | 2457 | `	return SXERR_NOTFOUND;` |
|     60 | 2458 |  |
|      - | 2459 | `/*` |
|      - | 2460 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2461 | ` *  Find the first occurrence of a string.` |
|      - | 2462 | ` * Parameters` |
|      - | 2463 | ` *  $haystack` |
|      - | 2464 | ` *   The input string.` |
|      - | 2465 | ` * $needle` |
|      - | 2466 | ` *   Search pattern (must be a string).` |
|      - | 2467 | ` * $before_needle` |
|      - | 2468 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2469 | ` *   of the needle (excluding the needle).` |
|      - | 2470 | ` * Return` |
|      - | 2471 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2472 | ` */` |
|     10 | 2473 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2474 |  |
|     11 | 2475 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2476 | `	const char *zBlob,*zPattern;` |
|      - | 2477 | `	int nLen,nPatLen;` |
|      - | 2478 | `	sxu32 nOfft;` |
|      - | 2479 | `	sxi32 rc;` |
|     11 | 2480 | `	if( nArg < 2 ){` |
|      - | 2481 | `		/* Missing arguments,return FALSE */` |
|      5 | 2482 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2483 | `		return PH7_OK;` |
|      - | 2484 | `	}` |
|      - | 2485 | `	/* Extract the needle and the haystack */` |
|      7 | 2486 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2487 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2488 | `	nOfft = 0; /* cc warning */` |
|      9 | 2489 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2490 | `		int before = 0;` |
|      - | 2491 | `		/* Perform the lookup */` |
|      5 | 2492 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2493 | `		if( rc != SXRET_OK ){` |
|      - | 2494 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2495 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2496 | `			return PH7_OK;` |
|      - | 2497 | `		}` |
|      - | 2498 | `		/* Return the portion of the string */` |
|      5 | 2499 | `		if( nArg > 2 ){` |
|      3 | 2500 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2501 | `		}` |
|      5 | 2502 | `		if( before ){` |
|      3 | 2503 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2504 | `		}else{` |
|      3 | 2505 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2506 | `		}` |
|      3 | 2507 | `	}else{` |
|      3 | 2508 | `		ph7_result_bool(pCtx,0);` |
|      - | 2509 | `	}` |
|      7 | 2510 | `	return PH7_OK;` |
|      6 | 2511 |  |
|      - | 2512 | `/*` |
|      - | 2513 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2514 | ` *  Case-insensitive strstr().` |
|      - | 2515 | ` * Parameters` |
|      - | 2516 | ` *  $haystack` |
|      - | 2517 | ` *   The input string.` |
|      - | 2518 | ` * $needle` |
|      - | 2519 | ` *   Search pattern (must be a string).` |
|      - | 2520 | ` * $before_needle` |
|      - | 2521 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2522 | ` *   of the needle (excluding the needle).` |
|      - | 2523 | ` * Return` |
|      - | 2524 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2525 | ` */` |
|      6 | 2526 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2527 |  |
|      7 | 2528 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2529 | `	const char *zBlob,*zPattern;` |
|      - | 2530 | `	int nLen,nPatLen;` |
|      - | 2531 | `	sxu32 nOfft;` |
|      - | 2532 | `	sxi32 rc;` |
|      7 | 2533 | `	if( nArg < 2 ){` |
|      - | 2534 | `		/* Missing arguments,return FALSE */` |
|      3 | 2535 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2536 | `		return PH7_OK;` |
|      - | 2537 | `	}` |
|      - | 2538 | `	/* Extract the needle and the haystack */` |
|      5 | 2539 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2540 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2541 | `	nOfft = 0; /* cc warning */` |
|      7 | 2542 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2543 | `		int before = 0;` |
|      - | 2544 | `		/* Perform the lookup */` |
|      5 | 2545 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2546 | `		if( rc != SXRET_OK ){` |
|      - | 2547 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2548 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2549 | `			return PH7_OK;` |
|      - | 2550 | `		}` |
|      - | 2551 | `		/* Return the portion of the string */` |
|      5 | 2552 | `		if( nArg > 2 ){` |
|      3 | 2553 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2554 | `		}` |
|      5 | 2555 | `		if( before ){` |
|      3 | 2556 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2557 | `		}else{` |
|      3 | 2558 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2559 | `		}` |
|      3 | 2560 | `	}else{` |
|    ! 0 | 2561 | `		ph7_result_bool(pCtx,0);` |
|      - | 2562 | `	}` |
|      5 | 2563 | `	return PH7_OK;` |
|      4 | 2564 |  |
|      - | 2565 | `/*` |
|      - | 2566 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2567 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2568 | ` * Parameters` |
|      - | 2569 | ` *  $haystack` |
|      - | 2570 | ` *   The input string.` |
|      - | 2571 | ` * $needle` |
|      - | 2572 | ` *   Search pattern (must be a string).` |
|      - | 2573 | ` * $offset` |
|      - | 2574 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2575 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2576 | ` *   of haystack.` |
|      - | 2577 | ` * Return` |
|      - | 2578 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2579 | ` */` |
|    120 | 2580 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2581 |  |
|    122 | 2582 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2583 | `	const char *zBlob,*zPattern;` |
|      - | 2584 | `	int nLen,nPatLen,nStart;` |
|      - | 2585 | `	sxu32 nOfft;` |
|      - | 2586 | `	sxi32 rc;` |
|    122 | 2587 | `	if( nArg < 2 ){` |
|      - | 2588 | `		/* Missing arguments,return FALSE */` |
|      7 | 2589 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2590 | `		return PH7_OK;` |
|      - | 2591 | `	}` |
|      - | 2592 | `	/* Extract the needle and the haystack */` |
|    116 | 2593 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    116 | 2594 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    116 | 2595 | `	nOfft = 0; /* cc warning */` |
|    116 | 2596 | `	nStart = 0;` |
|      - | 2597 | `	/* Peek the starting offset if available */` |
|    116 | 2598 | `	if( nArg > 2 ){` |
|    ! 0 | 2599 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2600 | `		if( nStart < 0 ){` |
|    ! 0 | 2601 | `			nStart = -nStart;` |
|    ! 0 | 2602 | `		}` |
|    ! 0 | 2603 | `		if( nStart >= nLen ){` |
|      - | 2604 | `			/* Invalid offset */` |
|    ! 0 | 2605 | `			nStart = 0;` |
|    ! 0 | 2606 | `		}else{` |
|    ! 0 | 2607 | `			zBlob += nStart;` |
|    ! 0 | 2608 | `			nLen -= nStart;` |
|      - | 2609 | `		}` |
|    ! 0 | 2610 | `	}` |
|    116 | 2611 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2612 | `		/* Perform the lookup */` |
|    114 | 2613 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    114 | 2614 | `		if( rc != SXRET_OK ){` |
|      - | 2615 | `			/* Pattern not found,return FALSE */` |
|     26 | 2616 | `			ph7_result_bool(pCtx,0);` |
|     26 | 2617 | `			return PH7_OK;` |
|      - | 2618 | `		}` |
|      - | 2619 | `		/* Return the pattern position */` |
|     90 | 2620 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     46 | 2621 | `	}else{` |
|      3 | 2622 | `		ph7_result_bool(pCtx,0);` |
|      - | 2623 | `	}` |
|     92 | 2624 | `	return PH7_OK;` |
|     62 | 2625 |  |
|      - | 2626 | `/*` |
|      - | 2627 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2628 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2629 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2630 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2631 | ` *` |
|      - | 2632 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2633 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2634 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2635 | ` *` |
|      - | 2636 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2637 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2638 | ` */` |
|    386 | 2639 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2640 | `	ph7_context *pCtx,` |
|      - | 2641 | `	ph7_value *pArg,` |
|      - | 2642 | `	const char *zFunc,` |
|      - | 2643 | `	int iArgNum,` |
|      - | 2644 | `	const char *zParamName,` |
|      - | 2645 | `	const char *zNullMsg,` |
|      - | 2646 | `	ph7_value *pTmp,` |
|      - | 2647 | `	const char **pzOut,` |
|      - | 2648 | `	int *pnOut` |
|      2 | 2649 | `){` |
|    388 | 2650 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2651 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2652 | `		*pzOut = "";` |
|     13 | 2653 | `		*pnOut = 0;` |
|     13 | 2654 | `		return PH7_OK;` |
|      - | 2655 | `	}` |
|    578 | 2656 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    356 | 2657 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2658 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2659 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2660 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2661 | `	    )` |
|      - | 2662 | `	){` |
|     32 | 2663 | `		const char *zType = ph7_type_name(pArg);` |
|     32 | 2664 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2665 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2666 | `			if( pInst && pInst->pClass ){` |
|     13 | 2667 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2668 | `			}` |
|      6 | 2669 | `		}` |
|     47 | 2670 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2671 | `			"TypeError",` |
|      - | 2672 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2673 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2674 | `			);` |
|      - | 2675 | `	}` |
|    345 | 2676 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2677 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2678 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2679 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2680 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2681 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2682 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2683 | `		return PH7_OK;` |
|      - | 2684 | `	}` |
|    309 | 2685 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    309 | 2686 | `	return PH7_OK;` |
|    195 | 2687 |  |
|      - | 2688 | `/*` |
|      - | 2689 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2690 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2691 | ` * Return` |
|      - | 2692 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2693 | ` */` |
|     76 | 2694 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2695 |  |
|      - | 2696 | `	const char *zHaystack,*zNeedle;` |
|      - | 2697 | `	int nHayLen,nNeedleLen;` |
|      - | 2698 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2699 | `	sxi32 rc;` |
|     78 | 2700 | `	if( nArg != 2 ){` |
|     17 | 2701 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2702 | `			"ArgumentCountError",` |
|      - | 2703 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2704 | `			nArg` |
|      - | 2705 | `			);` |
|      - | 2706 | `	}` |
|     68 | 2707 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     68 | 2708 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     68 | 2709 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2710 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2711 | `		"of type string is deprecated",` |
|      - | 2712 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     68 | 2713 | `	if( rc != PH7_OK ) goto out;` |
|     61 | 2714 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2715 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2716 | `		"of type string is deprecated",` |
|      - | 2717 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     61 | 2718 | `	if( rc != PH7_OK ) goto out;` |
|     57 | 2719 | `	if( nNeedleLen < 1 ){` |
|     13 | 2720 | `		ph7_result_bool(pCtx,1);` |
|     51 | 2721 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2722 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2723 | `	}else{` |
|     55 | 2724 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     18 | 2725 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     37 | 2726 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2727 | `	}` |
|     57 | 2728 | `	rc = PH7_OK;` |
|     33 | 2729 | `out:` |
|     68 | 2730 | `	PH7_MemObjRelease(&sHayTmp);` |
|     68 | 2731 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     68 | 2732 | `	return rc;` |
|     40 | 2733 |  |
|      - | 2734 | `/*` |
|      - | 2735 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2736 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2737 | ` * Return` |
|      - | 2738 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2739 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2740 | ` */` |
|     78 | 2741 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2742 |  |
|      - | 2743 | `	const char *zHaystack,*zNeedle;` |
|      - | 2744 | `	int nHayLen,nNeedleLen;` |
|      - | 2745 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2746 | `	sxi32 rc;` |
|     80 | 2747 | `	if( nArg != 2 ){` |
|     17 | 2748 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2749 | `			"ArgumentCountError",` |
|      - | 2750 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2751 | `			nArg` |
|      - | 2752 | `			);` |
|      - | 2753 | `	}` |
|     70 | 2754 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2755 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2756 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2757 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2758 | `		"of type string is deprecated",` |
|      - | 2759 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2760 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2761 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2762 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2763 | `		"of type string is deprecated",` |
|      - | 2764 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2765 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2766 | `	if( nNeedleLen < 1 ){` |
|     13 | 2767 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2768 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2769 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2770 | `	}else{` |
|     58 | 2771 | `		ph7_result_bool(pCtx,` |
|     38 | 2772 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2773 | `	}` |
|     59 | 2774 | `	rc = PH7_OK;` |
|     34 | 2775 | `out:` |
|     70 | 2776 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2777 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2778 | `	return rc;` |
|     41 | 2779 |  |
|      - | 2780 | `/*` |
|      - | 2781 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2782 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2783 | ` * Return` |
|      - | 2784 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2785 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2786 | ` */` |
|     78 | 2787 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2788 |  |
|      - | 2789 | `	const char *zHaystack,*zNeedle;` |
|      - | 2790 | `	int nHayLen,nNeedleLen;` |
|      - | 2791 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2792 | `	sxi32 rc;` |
|     80 | 2793 | `	if( nArg != 2 ){` |
|     17 | 2794 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2795 | `			"ArgumentCountError",` |
|      - | 2796 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2797 | `			nArg` |
|      - | 2798 | `			);` |
|      - | 2799 | `	}` |
|     70 | 2800 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2801 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2802 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2803 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2804 | `		"of type string is deprecated",` |
|      - | 2805 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2806 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2807 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2808 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2809 | `		"of type string is deprecated",` |
|      - | 2810 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2811 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2812 | `	if( nNeedleLen < 1 ){` |
|     13 | 2813 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2814 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2815 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2816 | `	}else{` |
|     58 | 2817 | `		ph7_result_bool(pCtx,` |
|     38 | 2818 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2819 | `	}` |
|     59 | 2820 | `	rc = PH7_OK;` |
|     34 | 2821 | `out:` |
|     70 | 2822 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2823 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2824 | `	return rc;` |
|     41 | 2825 |  |
|      - | 2826 | `/*` |
|      - | 2827 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2828 | ` *  Case-insensitive strpos.` |
|      - | 2829 | ` * Parameters` |
|      - | 2830 | ` *  $haystack` |
|      - | 2831 | ` *   The input string.` |
|      - | 2832 | ` * $needle` |
|      - | 2833 | ` *   Search pattern (must be a string).` |
|      - | 2834 | ` * $offset` |
|      - | 2835 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2836 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2837 | ` *   of haystack.` |
|      - | 2838 | ` * Return` |
|      - | 2839 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2840 | ` */` |
|     18 | 2841 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2842 |  |
|     19 | 2843 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2844 | `	const char *zBlob,*zPattern;` |
|      - | 2845 | `	int nLen,nPatLen,nStart;` |
|      - | 2846 | `	sxu32 nOfft;` |
|      - | 2847 | `	sxi32 rc;` |
|     19 | 2848 | `	if( nArg < 2 ){` |
|      - | 2849 | `		/* Missing arguments,return FALSE */` |
|      3 | 2850 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2851 | `		return PH7_OK;` |
|      - | 2852 | `	}` |
|      - | 2853 | `	/* Extract the needle and the haystack */` |
|     17 | 2854 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2855 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2856 | `	nOfft = 0; /* cc warning */` |
|     17 | 2857 | `	nStart = 0;` |
|      - | 2858 | `	/* Peek the starting offset if available */` |
|     17 | 2859 | `	if( nArg > 2 ){` |
|      5 | 2860 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2861 | `		if( nStart < 0 ){` |
|      3 | 2862 | `			nStart = -nStart;` |
|      1 | 2863 | `		}` |
|      5 | 2864 | `		if( nStart >= nLen ){` |
|      - | 2865 | `			/* Invalid offset */` |
|    ! 0 | 2866 | `			nStart = 0;` |
|    ! 0 | 2867 | `		}else{` |
|      5 | 2868 | `			zBlob += nStart;` |
|      5 | 2869 | `			nLen -= nStart;` |
|      - | 2870 | `		}` |
|      2 | 2871 | `	}` |
|     17 | 2872 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2873 | `		/* Perform the lookup */` |
|     17 | 2874 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2875 | `		if( rc != SXRET_OK ){` |
|      - | 2876 | `			/* Pattern not found,return FALSE */` |
|      3 | 2877 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2878 | `			return PH7_OK;` |
|      - | 2879 | `		}` |
|      - | 2880 | `		/* Return the pattern position */` |
|     15 | 2881 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2882 | `	}else{` |
|    ! 0 | 2883 | `		ph7_result_bool(pCtx,0);` |
|      - | 2884 | `	}` |
|     15 | 2885 | `	return PH7_OK;` |
|     10 | 2886 |  |
|      - | 2887 | `/*` |
|      - | 2888 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2889 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2890 | ` * Parameters` |
|      - | 2891 | ` *  $haystack` |
|      - | 2892 | ` *   The input string.` |
|      - | 2893 | ` * $needle` |
|      - | 2894 | ` *   Search pattern (must be a string).` |
|      - | 2895 | ` * $offset` |
|      - | 2896 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2897 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2898 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2899 | ` * Return` |
|      - | 2900 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2901 | ` */` |
|     32 | 2902 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2903 |  |
|      - | 2904 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2905 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2906 | `	int nLen,nPatLen;` |
|      - | 2907 | `	sxu32 nOfft;` |
|      - | 2908 | `	sxi32 rc;` |
|     33 | 2909 | `	if( nArg < 2 ){` |
|      - | 2910 | `		/* Missing arguments,return FALSE */` |
|      3 | 2911 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2912 | `		return PH7_OK;` |
|      - | 2913 | `	}` |
|      - | 2914 | `	/* Extract the needle and the haystack */` |
|     31 | 2915 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2916 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2917 | `	/* Point to the end of the pattern */` |
|     31 | 2918 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2919 | `	zEnd = &zBlob[nLen];` |
|      - | 2920 | `	/* Save the starting posistion */` |
|     31 | 2921 | `	zStart = zBlob;` |
|     31 | 2922 | `	nOfft = 0; /* cc warning */` |
|      - | 2923 | `	/* Peek the starting offset if available */` |
|     31 | 2924 | `	if( nArg > 2 ){` |
|      - | 2925 | `		int nStart;` |
|     21 | 2926 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2927 | `		if( nStart < 0 ){` |
|     11 | 2928 | `			nStart = -nStart;` |
|     11 | 2929 | `			if( nStart >= nLen ){` |
|      - | 2930 | `				/* Invalid offset */` |
|      3 | 2931 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2932 | `				return PH7_OK;` |
|    ! 0 | 2933 | `			}else{` |
|      9 | 2934 | `				nLen -= nStart;` |
|      9 | 2935 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2936 | `				zEnd = &zBlob[nLen];` |
|      - | 2937 | `			}` |
|      5 | 2938 | `		}else{` |
|     11 | 2939 | `			if( nStart >= nLen ){` |
|      - | 2940 | `				/* Invalid offset */` |
|      5 | 2941 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2942 | `				return PH7_OK;` |
|    ! 0 | 2943 | `			}else{` |
|      7 | 2944 | `				zBlob += nStart;` |
|      7 | 2945 | `				nLen -= nStart;` |
|      - | 2946 | `			}` |
|      - | 2947 | `		}` |
|      7 | 2948 | `	}` |
|     25 | 2949 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2950 | `		/* Perform the lookup */` |
|     57 | 2951 | `		for(;;){` |
|    115 | 2952 | `			if( zBlob >= zPtr ){` |
|     11 | 2953 | `				break;` |
|      - | 2954 | `			}` |
|    105 | 2955 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 2956 | `			if( rc == SXRET_OK ){` |
|      - | 2957 | `				/* Pattern found,return it's position */` |
|     13 | 2958 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 2959 | `				return PH7_OK;` |
|      - | 2960 | `			}` |
|     93 | 2961 | `			zPtr--;` |
|      1 | 2962 | `		}` |
|      - | 2963 | `		/* Pattern not found,return FALSE */` |
|     11 | 2964 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2965 | `	}else{` |
|      3 | 2966 | `		ph7_result_bool(pCtx,0);` |
|      - | 2967 | `	}` |
|     13 | 2968 | `	return PH7_OK;` |
|     17 | 2969 |  |
|      - | 2970 | `/*` |
|      - | 2971 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2972 | ` *  Case-insensitive strrpos.` |
|      - | 2973 | ` * Parameters` |
|      - | 2974 | ` *  $haystack` |
|      - | 2975 | ` *   The input string.` |
|      - | 2976 | ` * $needle` |
|      - | 2977 | ` *   Search pattern (must be a string).` |
|      - | 2978 | ` * $offset` |
|      - | 2979 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2980 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2981 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2982 | ` * Return` |
|      - | 2983 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2984 | ` */` |
|     28 | 2985 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2986 |  |
|      - | 2987 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 2988 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2989 | `	int nLen,nPatLen;` |
|      - | 2990 | `	sxu32 nOfft;` |
|      - | 2991 | `	sxi32 rc;` |
|     29 | 2992 | `	if( nArg < 2 ){` |
|      - | 2993 | `		/* Missing arguments,return FALSE */` |
|      3 | 2994 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2995 | `		return PH7_OK;` |
|      - | 2996 | `	}` |
|      - | 2997 | `	/* Extract the needle and the haystack */` |
|     27 | 2998 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 2999 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3000 | `	/* Point to the end of the pattern */` |
|     27 | 3001 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3002 | `	zEnd = &zBlob[nLen];` |
|      - | 3003 | `	/* Save the starting posistion */` |
|     27 | 3004 | `	zStart = zBlob;` |
|     27 | 3005 | `	nOfft = 0; /* cc warning */` |
|      - | 3006 | `	/* Peek the starting offset if available */` |
|     27 | 3007 | `	if( nArg > 2 ){` |
|      - | 3008 | `		int nStart;` |
|     15 | 3009 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3010 | `		if( nStart < 0 ){` |
|      7 | 3011 | `			nStart = -nStart;` |
|      7 | 3012 | `			if( nStart >= nLen ){` |
|      - | 3013 | `				/* Invalid offset */` |
|      3 | 3014 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3015 | `				return PH7_OK;` |
|    ! 0 | 3016 | `			}else{` |
|      5 | 3017 | `				nLen -= nStart;` |
|      5 | 3018 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3019 | `				zEnd = &zBlob[nLen];` |
|      - | 3020 | `			}` |
|      3 | 3021 | `		}else{` |
|      9 | 3022 | `			if( nStart >= nLen ){` |
|      - | 3023 | `				/* Invalid offset */` |
|      5 | 3024 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3025 | `				return PH7_OK;` |
|    ! 0 | 3026 | `			}else{` |
|      5 | 3027 | `				zBlob += nStart;` |
|      5 | 3028 | `				nLen -= nStart;` |
|      - | 3029 | `			}` |
|      - | 3030 | `		}` |
|      4 | 3031 | `	}` |
|     21 | 3032 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3033 | `		/* Perform the lookup */` |
|     44 | 3034 | `		for(;;){` |
|     89 | 3035 | `			if( zBlob >= zPtr ){` |
|      9 | 3036 | `				break;` |
|      - | 3037 | `			}` |
|     81 | 3038 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3039 | `			if( rc == SXRET_OK ){` |
|      - | 3040 | `				/* Pattern found,return it's position */` |
|     11 | 3041 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3042 | `				return PH7_OK;` |
|      - | 3043 | `			}` |
|     71 | 3044 | `			zPtr--;` |
|      1 | 3045 | `		}` |
|      - | 3046 | `		/* Pattern not found,return FALSE */` |
|      9 | 3047 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3048 | `	}else{` |
|      3 | 3049 | `		ph7_result_bool(pCtx,0);` |
|      - | 3050 | `	}` |
|     11 | 3051 | `	return PH7_OK;` |
|     15 | 3052 |  |
|      - | 3053 | `/*` |
|      - | 3054 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3055 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3056 | ` * Parameters` |
|      - | 3057 | ` *  $haystack` |
|      - | 3058 | ` *   The input string.` |
|      - | 3059 | ` * $needle` |
|      - | 3060 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3061 | ` *  This behavior is different from that of strstr().` |
|      - | 3062 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3063 | ` *  as the ordinal value of a character.` |
|      - | 3064 | ` * Return` |
|      - | 3065 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3066 | ` */` |
|     24 | 3067 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3068 |  |
|      - | 3069 | `	const char *zBlob;` |
|      - | 3070 | `	int nLen,c;` |
|     25 | 3071 | `	if( nArg < 2 ){` |
|      - | 3072 | `		/* Missing arguments,return FALSE */` |
|      3 | 3073 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3074 | `		return PH7_OK;` |
|      - | 3075 | `	}` |
|      - | 3076 | `	/* Extract the haystack */` |
|     23 | 3077 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3078 | `	c = 0; /* cc warning */` |
|     23 | 3079 | `	if( nLen > 0 ){` |
|      - | 3080 | `		sxu32 nOfft;` |
|      - | 3081 | `		sxi32 rc;` |
|     21 | 3082 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3083 | `			const char *zPattern;` |
|     11 | 3084 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3085 | `														 * for NULL pointer.` |
|      - | 3086 | `														 */` |
|     11 | 3087 | `			c = zPattern[0];` |
|      6 | 3088 | `		}else{` |
|      - | 3089 | `			/* Int cast */` |
|     11 | 3090 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3091 | `		}` |
|      - | 3092 | `		/* Perform the lookup */` |
|     21 | 3093 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3094 | `		if( rc != SXRET_OK ){` |
|      - | 3095 | `			/* No such entry,return FALSE */` |
|      7 | 3096 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3097 | `			return PH7_OK;` |
|      - | 3098 | `		}` |
|      - | 3099 | `		/* Return the string portion */` |
|     15 | 3100 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3101 | `	}else{` |
|      3 | 3102 | `		ph7_result_bool(pCtx,0);` |
|      - | 3103 | `	}` |
|     17 | 3104 | `	return PH7_OK;` |
|     13 | 3105 |  |
|      - | 3106 | `/*` |
|      - | 3107 | ` * string strrev(string $string)` |
|      - | 3108 | ` *  Reverse a string.` |
|      - | 3109 | ` * Parameters` |
|      - | 3110 | ` *  $string` |
|      - | 3111 | ` *   String to be reversed.` |
|      - | 3112 | ` * Return` |
|      - | 3113 | ` *  The reversed string.` |
|      - | 3114 | ` */` |
|      4 | 3115 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3116 |  |
|      - | 3117 | `	const char *zIn,*zEnd;` |
|      - | 3118 | `	int nLen,c;` |
|      5 | 3119 | `	if( nArg < 1 ){` |
|      - | 3120 | `		/* Missing arguments,return NULL */` |
|      3 | 3121 | `		ph7_result_null(pCtx);` |
|      3 | 3122 | `		return PH7_OK;` |
|      - | 3123 | `	}` |
|      - | 3124 | `	/* Extract the target string */` |
|      3 | 3125 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3126 | `	if( nLen < 1 ){` |
|      - | 3127 | `		/* Empty string Return null */` |
|    ! 0 | 3128 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3129 | `		return PH7_OK;` |
|      - | 3130 | `	}` |
|      - | 3131 | `	/* Perform the requested operation */` |
|      3 | 3132 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3133 | `	for(;;){` |
|      9 | 3134 | `		if( zEnd < zIn ){` |
|      - | 3135 | `			/* No more input to process */` |
|      3 | 3136 | `			break;` |
|      - | 3137 | `		}` |
|      - | 3138 | `		/* Append current character */` |
|      7 | 3139 | `		c = zEnd[0];` |
|      7 | 3140 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3141 | `		zEnd--;` |
|      1 | 3142 | `	}` |
|      3 | 3143 | `	return PH7_OK;` |
|      3 | 3144 |  |
|      - | 3145 | `/*` |
|      - | 3146 | ` * string ucwords(string $string)` |
|      - | 3147 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3148 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3149 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3150 | ` * Parameters` |
|      - | 3151 | ` *  $string` |
|      - | 3152 | ` *   The input string.` |
|      - | 3153 | ` * Return` |
|      - | 3154 | ` *  The modified string..` |
|      - | 3155 | ` */` |
|     14 | 3156 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3157 |  |
|      - | 3158 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3159 | `	int nLen,c;` |
|     15 | 3160 | `	if( nArg < 1 ){` |
|      - | 3161 | `		/* Missing arguments,return NULL */` |
|      3 | 3162 | `		ph7_result_null(pCtx);` |
|      3 | 3163 | `		return PH7_OK;` |
|      - | 3164 | `	}` |
|      - | 3165 | `	/* Extract the target string */` |
|     13 | 3166 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3167 | `	if( nLen < 1 ){` |
|      - | 3168 | `		/* Empty string – match PHP semantics */` |
|      3 | 3169 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3170 | `		return PH7_OK;` |
|      - | 3171 | `	}` |
|      - | 3172 | `	/* Perform the requested operation */` |
|     11 | 3173 | `	zEnd = &zIn[nLen];` |
|     21 | 3174 | `	for(;;){` |
|      - | 3175 | `		/* Jump leading white spaces */` |
|     43 | 3176 | `		zCur = zIn;` |
|     65 | 3177 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3178 | `			zIn++;` |
|      1 | 3179 | `		}` |
|     43 | 3180 | `		if( zCur < zIn ){` |
|      - | 3181 | `			/* Append white space stream */` |
|     23 | 3182 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3183 | `		}` |
|     43 | 3184 | `		if( zIn >= zEnd ){` |
|      - | 3185 | `			/* No more input to process */` |
|     11 | 3186 | `			break;` |
|      - | 3187 | `		}` |
|     33 | 3188 | `		c = zIn[0];` |
|     33 | 3189 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3190 | `			c = SyToUpper(c);` |
|     14 | 3191 | `		}` |
|      - | 3192 | `		/* Append the upper-cased character */` |
|     33 | 3193 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3194 | `		zIn++;` |
|     33 | 3195 | `		zCur = zIn;` |
|      - | 3196 | `		/* Append the word varbatim */` |
|    149 | 3197 | `		while( zIn < zEnd ){` |
|    139 | 3198 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3199 | `				/* UTF-8 stream */` |
|    ! 0 | 3200 | `				zIn++;` |
|    ! 0 | 3201 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3202 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3203 | `				zIn++;` |
|     59 | 3204 | `			}else{` |
|     23 | 3205 | `				break;` |
|      - | 3206 | `			}` |
|      1 | 3207 | `		}` |
|     33 | 3208 | `		if( zCur < zIn ){` |
|     33 | 3209 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3210 | `		}` |
|      1 | 3211 | `	}` |
|     11 | 3212 | `	return PH7_OK;` |
|      8 | 3213 |  |
|      - | 3214 | `/*` |
|      - | 3215 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3216 | ` *  Returns input repeated multiplier times.` |
|      - | 3217 | ` * Parameters` |
|      - | 3218 | ` *  $string` |
|      - | 3219 | ` *   String to be repeated.` |
|      - | 3220 | ` * $multiplier` |
|      - | 3221 | ` *  Number of time the input string should be repeated.` |
|      - | 3222 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3223 | ` *  to 0, the function will return an empty string.` |
|      - | 3224 | ` * Return` |
|      - | 3225 | ` *  The repeated string.` |
|      - | 3226 | ` */` |
|  20216 | 3227 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3228 |  |
|      - | 3229 | `	const char *zIn;` |
|      - | 3230 | `	int nLen,nMul;` |
|      - | 3231 | `	int rc;` |
|  20217 | 3232 | `	if( nArg < 2 ){` |
|      - | 3233 | `		/* Missing arguments,return NULL */` |
|      3 | 3234 | `		ph7_result_null(pCtx);` |
|      3 | 3235 | `		return PH7_OK;` |
|      - | 3236 | `	}` |
|      - | 3237 | `	/* Extract the target string */` |
|  20215 | 3238 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20215 | 3239 | `	if( nLen < 1 ){` |
|      - | 3240 | `		/* Empty string.Return null */` |
|    ! 0 | 3241 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3242 | `		return PH7_OK;` |
|      - | 3243 | `	}` |
|      - | 3244 | `	/* Extract the multiplier */` |
|  20215 | 3245 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20215 | 3246 | `	if( nMul < 1 ){` |
|      - | 3247 | `		/* Return the empty string */` |
|      3 | 3248 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3249 | `		return PH7_OK;` |
|      - | 3250 | `	}` |
|      - | 3251 | `	/* Perform the requested operation */` |
| 120289 | 3252 | `	for(;;){` |
| 240579 | 3253 | `		if( !nMul ){` |
|  20213 | 3254 | `			break;` |
|      - | 3255 | `		}` |
|      - | 3256 | `		/* Append the copy */` |
| 220367 | 3257 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220367 | 3258 | `		if( rc != PH7_OK ){` |
|      - | 3259 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3260 | `			break;` |
|      - | 3261 | `		}` |
| 220367 | 3262 | `		nMul--;` |
|      1 | 3263 | `	}` |
|  20213 | 3264 | `	return PH7_OK;` |
|  10109 | 3265 |  |
|      - | 3266 | `/*` |
|      - | 3267 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3268 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3269 | ` * Parameters` |
|      - | 3270 | ` *  $string` |
|      - | 3271 | ` *   The input string.` |
|      - | 3272 | ` * $is_xhtml` |
|      - | 3273 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3274 | ` * Return` |
|      - | 3275 | ` *  The processed string.` |
|      - | 3276 | ` */` |
|      6 | 3277 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3278 |  |
|      - | 3279 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3280 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3281 | `	int nLen;` |
|      7 | 3282 | `	if( nArg < 1 ){` |
|      - | 3283 | `		/* Missing arguments,return the empty string */` |
|      3 | 3284 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3285 | `		return PH7_OK;` |
|      - | 3286 | `	}` |
|      - | 3287 | `	/* Extract the target string */` |
|      5 | 3288 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3289 | `	if( nLen < 1 ){` |
|      - | 3290 | `		/* Empty string,return null */` |
|    ! 0 | 3291 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3292 | `		return PH7_OK;` |
|      - | 3293 | `	}` |
|      5 | 3294 | `	if( nArg > 1 ){` |
|      3 | 3295 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3296 | `	}` |
|      5 | 3297 | `	zEnd = &zIn[nLen];` |
|      - | 3298 | `	/* Perform the requested operation */` |
|      4 | 3299 | `	for(;;){` |
|      9 | 3300 | `		zCur = zIn;` |
|      - | 3301 | `		/* Delimit the string */` |
|     21 | 3302 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3303 | `			zIn++;` |
|      1 | 3304 | `		}` |
|      9 | 3305 | `		if( zCur < zIn ){` |
|      - | 3306 | `			/* Output chunk verbatim */` |
|      9 | 3307 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3308 | `		}` |
|      9 | 3309 | `		if( zIn >= zEnd ){` |
|      - | 3310 | `			/* No more input to process */` |
|      5 | 3311 | `			break;` |
|      - | 3312 | `		}` |
|      - | 3313 | `		/* Output the HTML line break */` |
|      - | 3314 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3315 | `		if( is_xhtml ){` |
|      3 | 3316 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3317 | `		}else{` |
|      3 | 3318 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3319 | `		}` |
|      5 | 3320 | `		zCur = zIn;` |
|      - | 3321 | `		/* Append trailing line */` |
|     11 | 3322 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3323 | `			zIn++;` |
|      1 | 3324 | `		}` |
|      5 | 3325 | `		if( zCur < zIn ){` |
|      - | 3326 | `			/* Output chunk verbatim */` |
|      5 | 3327 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3328 | `		}` |
|      1 | 3329 | `	}` |
|      5 | 3330 | `	return PH7_OK;` |
|      4 | 3331 |  |
|      - | 3332 | `/*` |
|      - | 3333 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3334 | ` *  According to the PHP reference manual.` |
|      - | 3335 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3336 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3337 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3338 | ` * This applies to both sprintf() and printf().` |
|      - | 3339 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3340 | ` * or more of these elements, in order:` |
|      - | 3341 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3342 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3343 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3344 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3345 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3346 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3347 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3348 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3349 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3350 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3351 | ` *   should result in.` |
|      - | 3352 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3353 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3354 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3355 | ` *   limit to the string.` |
|      - | 3356 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3357 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3358 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3359 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3360 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3361 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3362 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3363 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3364 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3365 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3366 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3367 | ` *       g - shorter of %e and %f.` |
|      - | 3368 | ` *       G - shorter of %E and %f.` |
|      - | 3369 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3370 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3371 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3372 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3373 | ` */` |
|      - | 3374 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3375 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3376 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3377 | `/*` |
|      - | 3378 | `** Conversion types fall into various categories as defined by the` |
|      - | 3379 | `** following enumeration.` |
|      - | 3380 | `*/` |
|      - | 3381 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3382 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3383 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3384 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3385 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3386 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3387 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3388 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3389 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3390 |  |
|      - | 3391 | `/*` |
|      - | 3392 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3393 | `*/` |
|      - | 3394 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3395 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3396 | `/*` |
|      - | 3397 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3398 | `** by an instance of the following structure` |
|      - | 3399 | `*/` |
|      - | 3400 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3401 | `struct ph7_fmt_info` |
|      - | 3402 |  |
|      - | 3403 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3404 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3405 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3406 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3407 | `  char *charset; /* The character set for conversion */` |
|      - | 3408 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3409 | `};` |
|      - | 3410 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3411 | `/*` |
|      - | 3412 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3413 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3414 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3415 | `**` |
|      - | 3416 | `** Example:` |
|      - | 3417 | `**     input:     *val = 3.14159` |
|      - | 3418 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3419 | `**` |
|      - | 3420 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3421 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3422 | `** always returned.` |
|      - | 3423 | `*/` |
|    422 | 3424 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3425 |  |
|      - | 3426 | `  sxlongreal d;` |
|      - | 3427 | `  int digit;` |
|      - | 3428 |  |
|    423 | 3429 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3430 | `	  return '0';` |
|      - | 3431 | `  }` |
|    423 | 3432 | `  digit = (int)*val;` |
|    423 | 3433 | `  d = digit;` |
|    423 | 3434 | `   *val = (*val - d)*10.0;` |
|    423 | 3435 | `  return digit + '0' ;` |
|    212 | 3436 |  |
|      - | 3437 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3438 | `/*` |
|      - | 3439 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3440 | ` * used conversion types first.` |
|      - | 3441 | ` */` |
|      - | 3442 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3443 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3444 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3445 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3446 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3447 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3448 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3449 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3450 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3451 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3452 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3453 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3454 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3455 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3456 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3457 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3458 | `};` |
|      - | 3459 | `/*` |
|      - | 3460 | ` * Format a given string.` |
|      - | 3461 | ` * The root program.  All variations call this core.` |
|      - | 3462 | ` * INPUTS:` |
|      - | 3463 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3464 | ` *            1. A pointer to the call context.` |
|      - | 3465 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3466 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3467 | ` *            3. An integer number of characters to be output.` |
|      - | 3468 | ` *               (Note: This number might be zero.)` |
|      - | 3469 | ` *            4. Upper layer private data.` |
|      - | 3470 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3471 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3472 | ` */` |
|    136 | 3473 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3474 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3475 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3476 | `	const char *zIn,    /* Format string */` |
|      - | 3477 | `	int nByte,          /* Format string length */` |
|      - | 3478 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3479 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3480 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3481 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3482 | `	)` |
|      1 | 3483 |  |
|    137 | 3484 | `	char spaces[] = "                                                  ";` |
|      - | 3485 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    137 | 3486 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3487 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3488 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3489 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3490 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3491 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3492 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3493 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3494 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3495 | `	ph7_int64 iVal;` |
|      - | 3496 | `	int precision;           /* Precision of the current field */` |
|      - | 3497 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3498 | `	int c,rc,n;` |
|      - | 3499 | `	int length;              /* Length of the field */` |
|      - | 3500 | `	int prefix;` |
|      - | 3501 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3502 | `	int width;               /* Width of the current field */` |
|      - | 3503 | `	int idx;` |
|    137 | 3504 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3505 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3506 | `	/* Start the format process */` |
|    139 | 3507 | `	for(;;){` |
|    279 | 3508 | `		zCur = zIn;` |
|    739 | 3509 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    461 | 3510 | `			zIn++;` |
|      1 | 3511 | `		}` |
|    279 | 3512 | `		if( zCur < zIn ){` |
|      - | 3513 | `			/* Consume chunk verbatim */` |
|    105 | 3514 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    105 | 3515 | `			if( rc == SXERR_ABORT ){` |
|      - | 3516 | `				/* Callback request an operation abort */` |
|    ! 0 | 3517 | `				break;` |
|      - | 3518 | `			}` |
|     52 | 3519 | `		}` |
|    279 | 3520 | `		if( zIn >= zEnd ){` |
|      - | 3521 | `			/* No more input to process,break immediately */` |
|    135 | 3522 | `			break;` |
|      - | 3523 | `		}` |
|      - | 3524 | `		/* Find out what flags are present */` |
|    145 | 3525 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    144 | 3526 | `			flag_alternateform = flag_zeropad = 0;` |
|    145 | 3527 | `		zIn++; /* Jump the precent sign */` |
|     72 | 3528 | `		do{` |
|    177 | 3529 | `			c = zIn[0];` |
|    177 | 3530 | `			switch( c ){` |
|      9 | 3531 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3532 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3533 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3534 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3535 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3536 | `			case '\'':` |
|    ! 0 | 3537 | `				zIn++;` |
|    ! 0 | 3538 | `				if( zIn < zEnd ){` |
|      - | 3539 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3540 | `					c = zIn[0];` |
|    ! 0 | 3541 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3542 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3543 | `					}` |
|    ! 0 | 3544 | `					c = 0;` |
|    ! 0 | 3545 | `				}` |
|    ! 0 | 3546 | `				break;` |
|    144 | 3547 | `			default:                                       break;` |
|      - | 3548 | `			}` |
|    177 | 3549 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3550 | `		/* Get the field width */` |
|    145 | 3551 | `		width = 0;` |
|    251 | 3552 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3553 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3554 | `			zIn++;` |
|      1 | 3555 | `		}` |
|    145 | 3556 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3557 | `			/* Position specifer */` |
|    ! 0 | 3558 | `			if( width > 0 ){` |
|    ! 0 | 3559 | `				n = width;` |
|    ! 0 | 3560 | `				if( vf && n > 0 ){` |
|    ! 0 | 3561 | `					n--;` |
|    ! 0 | 3562 | `				}` |
|    ! 0 | 3563 | `			}` |
|    ! 0 | 3564 | `			zIn++;` |
|    ! 0 | 3565 | `			width = 0;` |
|    ! 0 | 3566 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3567 | `				flag_zeropad = 1;` |
|    ! 0 | 3568 | `				zIn++;` |
|    ! 0 | 3569 | `			}` |
|    ! 0 | 3570 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3571 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3572 | `				zIn++;` |
|    ! 0 | 3573 | `			}` |
|    ! 0 | 3574 | `		}` |
|    145 | 3575 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3576 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3577 | `		}` |
|      - | 3578 | `		/* Get the precision */` |
|    145 | 3579 | `		precision = -1;` |
|    145 | 3580 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3581 | `			precision = 0;` |
|     59 | 3582 | `			zIn++;` |
|    150 | 3583 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3584 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3585 | `				zIn++;` |
|      1 | 3586 | `			}` |
|     29 | 3587 | `		}` |
|    145 | 3588 | `		if( zIn >= zEnd ){` |
|      - | 3589 | `			/* No more input */` |
|      3 | 3590 | `			break;` |
|      - | 3591 | `		}` |
|      - | 3592 | `		/* Fetch the info entry for the field */` |
|    143 | 3593 | `		pInfo = 0;` |
|    143 | 3594 | `		xtype = PH7_FMT_ERROR;` |
|    143 | 3595 | `		c = zIn[0];` |
|    143 | 3596 | `		zIn++; /* Jump the format specifer */` |
|    787 | 3597 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    785 | 3598 | `			if( c==aFmt[idx].fmttype ){` |
|    141 | 3599 | `				pInfo = &aFmt[idx];` |
|    141 | 3600 | `				xtype = pInfo->type;` |
|    141 | 3601 | `				break;` |
|      - | 3602 | `			}` |
|    323 | 3603 | `		}` |
|    143 | 3604 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    143 | 3605 | `		length = 0;` |
|      - | 3606 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3607 | `		 /*` |
|      - | 3608 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3609 | `		  **` |
|      - | 3610 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3611 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3612 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3613 | `		  **                               field width was negative.` |
|      - | 3614 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3615 | `		  **                               the conversion character.` |
|      - | 3616 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3617 | `		  **   width                       The specified field width.  This is` |
|      - | 3618 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3619 | `		  **   precision                   The specified precision.  The default` |
|      - | 3620 | `		  **                               is -1.` |
|      - | 3621 | `		  */` |
|    143 | 3622 | `		switch(xtype){` |
|    ! 0 | 3623 | `		case PH7_FMT_PERCENT:` |
|      - | 3624 | `			/* A literal percent character */` |
|    ! 0 | 3625 | `			zWorker[0] = '%';` |
|    ! 0 | 3626 | `			length = (int)sizeof(char);` |
|    ! 0 | 3627 | `			break;` |
|      3 | 3628 | `		case PH7_FMT_CHARX:` |
|      - | 3629 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3630 | `			 * with that ASCII value` |
|      - | 3631 | `			 */` |
|      7 | 3632 | `			pArg = NEXT_ARG;` |
|      7 | 3633 | `			if( pArg == 0 ){` |
|      3 | 3634 | `				c = 0;` |
|      2 | 3635 | `			}else{` |
|      5 | 3636 | `				c = ph7_value_to_int(pArg);` |
|      - | 3637 | `			}` |
|      - | 3638 | `			/* NUL byte is an acceptable value */` |
|      7 | 3639 | `			zWorker[0] = (char)c;` |
|      7 | 3640 | `			length = (int)sizeof(char);` |
|      7 | 3641 | `			break;` |
|     12 | 3642 | `		case PH7_FMT_STRING:` |
|      - | 3643 | `			/* the argument is treated as and presented as a string */` |
|     25 | 3644 | `			pArg = NEXT_ARG;` |
|     25 | 3645 | `			if( pArg == 0 ){` |
|    ! 0 | 3646 | `				length = 0;` |
|    ! 0 | 3647 | `			}else{` |
|     25 | 3648 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3649 | `			}` |
|     25 | 3650 | `			if( length < 1 ){` |
|    ! 0 | 3651 | `				zBuf = " ";` |
|    ! 0 | 3652 | `				length = (int)sizeof(char);` |
|    ! 0 | 3653 | `			}` |
|     25 | 3654 | `			if( precision>=0 && precision<length ){` |
|      3 | 3655 | `				length = precision;` |
|      1 | 3656 | `			}` |
|     25 | 3657 | `			if( flag_zeropad ){` |
|      - | 3658 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3659 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3660 | `					spaces[idx] = '0';` |
|    ! 0 | 3661 | `				}` |
|    ! 0 | 3662 | `			}` |
|     25 | 3663 | `			break;` |
|     27 | 3664 | `		case PH7_FMT_RADIX:` |
|     55 | 3665 | `			pArg = NEXT_ARG;` |
|     55 | 3666 | `			if( pArg == 0 ){` |
|    ! 0 | 3667 | `				iVal = 0;` |
|    ! 0 | 3668 | `			}else{` |
|     55 | 3669 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3670 | `			}` |
|      - | 3671 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     55 | 3672 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3673 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3674 | `			}` |
|      - | 3675 | `#if 1` |
|      - | 3676 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3677 | `        ** I think this is stupid.*/` |
|     55 | 3678 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3679 | `#else` |
|      - | 3680 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3681 | `        ** but leave the prefix for hex.*/` |
|      - | 3682 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3683 | `#endif` |
|     55 | 3684 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     25 | 3685 | `          if( iVal<0 ){` |
|      3 | 3686 | `            iVal = -iVal;` |
|      - | 3687 | `			/* Ticket 1433-003 */` |
|      3 | 3688 | `			if( iVal < 0 ){` |
|      - | 3689 | `				/* Overflow */` |
|    ! 0 | 3690 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3691 | `			}` |
|      3 | 3692 | `            prefix = '-';` |
|     24 | 3693 | `          }else if( flag_plussign )  prefix = '+';` |
|     21 | 3694 | `          else if( flag_blanksign )  prefix = ' ';` |
|     19 | 3695 | `          else                       prefix = 0;` |
|     13 | 3696 | `        }else{` |
|     31 | 3697 | `			if( iVal<0 ){` |
|    ! 0 | 3698 | `				iVal = -iVal;` |
|      - | 3699 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3700 | `				if( iVal < 0 ){` |
|      - | 3701 | `					/* Overflow */` |
|    ! 0 | 3702 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3703 | `				}` |
|    ! 0 | 3704 | `			}` |
|     31 | 3705 | `			prefix = 0;` |
|      - | 3706 | `		}` |
|     55 | 3707 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3708 | `          precision = width-(prefix!=0);` |
|      3 | 3709 | `        }` |
|     55 | 3710 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3711 | `        {` |
|      - | 3712 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3713 | `          register int base;` |
|     55 | 3714 | `          cset = pInfo->charset;` |
|     55 | 3715 | `          base = pInfo->base;` |
|     27 | 3716 | `          do{                                           /* Convert to ascii */` |
|    123 | 3717 | `            *(--zBuf) = cset[iVal%base];` |
|    123 | 3718 | `            iVal = iVal/base;` |
|    123 | 3719 | `          }while( iVal>0 );` |
|      - | 3720 | `        }` |
|     55 | 3721 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     77 | 3722 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3723 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3724 | `        }` |
|     55 | 3725 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     55 | 3726 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3727 | `          char *pre, x;` |
|      9 | 3728 | `          pre = pInfo->prefix;` |
|      9 | 3729 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3730 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3731 | `          }` |
|      4 | 3732 | `        }` |
|     55 | 3733 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 3734 | `		break;` |
|     28 | 3735 | `		case PH7_FMT_FLOAT:` |
|      - | 3736 | `		case PH7_FMT_EXP:` |
|      - | 3737 | `		case PH7_FMT_GENERIC:{` |
|      - | 3738 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3739 | `		long double realvalue;` |
|      - | 3740 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3741 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3742 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3743 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3744 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3745 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3746 | `		pArg = NEXT_ARG;` |
|     57 | 3747 | `		if( pArg == 0 ){` |
|    ! 0 | 3748 | `			realvalue = 0;` |
|    ! 0 | 3749 | `		}else{` |
|     57 | 3750 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3751 | `		}` |
|      - | 3752 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3753 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3754 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3755 | `			zBuf = "NAN";` |
|    ! 0 | 3756 | `			length = 3;` |
|    ! 0 | 3757 | `			break;` |
|      - | 3758 | `		}` |
|     57 | 3759 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3760 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3761 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3762 | `				zBuf = "-INF";` |
|    ! 0 | 3763 | `				length = 4;` |
|    ! 0 | 3764 | `			}else{` |
|    ! 0 | 3765 | `				zBuf = "INF";` |
|    ! 0 | 3766 | `				length = 3;` |
|      - | 3767 | `			}` |
|    ! 0 | 3768 | `			break;` |
|      - | 3769 | `		}` |
|     57 | 3770 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3771 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3772 | `        if( realvalue<0.0 ){` |
|      3 | 3773 | `          realvalue = -realvalue;` |
|      3 | 3774 | `          prefix = '-';` |
|      2 | 3775 | `        }else{` |
|     55 | 3776 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3777 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3778 | `          else                         prefix = 0;` |
|      - | 3779 | `        }` |
|     57 | 3780 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3781 | `        rounder = 0.0;` |
|      - | 3782 | `#if 0` |
|      - | 3783 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3784 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3785 | `#else` |
|      - | 3786 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3787 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3788 | `#endif` |
|     57 | 3789 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3790 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3791 | `        exp = 0;` |
|     57 | 3792 | `        if( realvalue>0.0 ){` |
|     61 | 3793 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3794 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3795 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3796 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3797 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3798 | `            zBuf = "NaN";` |
|    ! 0 | 3799 | `            length = 3;` |
|    ! 0 | 3800 | `            break;` |
|      - | 3801 | `          }` |
|     28 | 3802 | `        }` |
|     57 | 3803 | `        zBuf = zWorker;` |
|      - | 3804 | `        /*` |
|      - | 3805 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3806 | `        ** or etFLOAT, as appropriate.` |
|      - | 3807 | `        */` |
|     57 | 3808 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3809 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3810 | `          realvalue += rounder;` |
|    ! 0 | 3811 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3812 | `        }` |
|     57 | 3813 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3814 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3815 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3816 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3817 | `          }else{` |
|    ! 0 | 3818 | `            precision = precision - exp;` |
|    ! 0 | 3819 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3820 | `          }` |
|    ! 0 | 3821 | `        }else{` |
|     57 | 3822 | `          flag_rtz = 0;` |
|      - | 3823 | `        }` |
|      - | 3824 | `        /*` |
|      - | 3825 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3826 | `        ** the precision is too large to fit in buf[].` |
|      - | 3827 | `        */` |
|     57 | 3828 | `        nsd = 0;` |
|     57 | 3829 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3830 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3831 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3832 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3833 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3834 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3835 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3836 | `            *(zBuf++) = '0';` |
|     17 | 3837 | `          }` |
|    373 | 3838 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3839 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3840 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3841 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3842 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3843 | `          }` |
|     57 | 3844 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3845 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3846 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3847 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3848 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3849 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3850 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3851 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3852 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3853 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3854 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3855 | `          }` |
|    ! 0 | 3856 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3857 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3858 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3859 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3860 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3861 | `            if( exp>=100 ){` |
|    ! 0 | 3862 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3863 | `              exp %= 100;` |
|    ! 0 | 3864 | `            }` |
|    ! 0 | 3865 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3866 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3867 | `          }` |
|      - | 3868 | `        }` |
|      - | 3869 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3870 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3871 | `        ** integer conversions.*/` |
|     57 | 3872 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3873 | `        zBuf = zWorker;` |
|      - | 3874 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3875 | `        ** set and we are not left justified */` |
|     57 | 3876 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3877 | `          int i;` |
|      3 | 3878 | `          int nPad = width - length;` |
|     13 | 3879 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3880 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3881 | `          }` |
|      3 | 3882 | `          i = prefix!=0;` |
|      5 | 3883 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3884 | `          length = width;` |
|      1 | 3885 | `        }` |
|      - | 3886 | `#else` |
|      - | 3887 | `         zBuf = " ";` |
|      - | 3888 | `		 length = (int)sizeof(char);` |
|      - | 3889 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3890 | `		 break;` |
|      - | 3891 | `							 }` |
|      1 | 3892 | `		default:` |
|      - | 3893 | `			/* Invalid format specifer */` |
|      3 | 3894 | `			zWorker[0] = '?';` |
|      3 | 3895 | `			length = (int)sizeof(char);` |
|      2 | 3896 | `			break;` |
|      - | 3897 | `		}` |
|      - | 3898 | `		 /*` |
|      - | 3899 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3900 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3901 | `		 ** the output.` |
|      - | 3902 | `		 */` |
|    143 | 3903 | `    if( !flag_leftjustify ){` |
|      - | 3904 | `      register int nspace;` |
|    135 | 3905 | `      nspace = width-length;` |
|    135 | 3906 | `      if( nspace>0 ){` |
|      5 | 3907 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3908 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3909 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3910 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3911 | `			}` |
|    ! 0 | 3912 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3913 | `        }` |
|      5 | 3914 | `        if( nspace>0 ){` |
|      5 | 3915 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3916 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3917 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3918 | `			}` |
|      2 | 3919 | `		}` |
|      2 | 3920 | `      }` |
|     67 | 3921 | `    }` |
|    143 | 3922 | `    if( length>0 ){` |
|    143 | 3923 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    143 | 3924 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3925 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3926 | `		}` |
|     71 | 3927 | `    }` |
|    143 | 3928 | `    if( flag_leftjustify ){` |
|      - | 3929 | `      register int nspace;` |
|      9 | 3930 | `      nspace = width-length;` |
|      9 | 3931 | `      if( nspace>0 ){` |
|      9 | 3932 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3933 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3934 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3935 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3936 | `			}` |
|    ! 0 | 3937 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3938 | `        }` |
|      9 | 3939 | `        if( nspace>0 ){` |
|      9 | 3940 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3941 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3942 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3943 | `			}` |
|      4 | 3944 | `		}` |
|      4 | 3945 | `      }` |
|      4 | 3946 | `    }` |
|      1 | 3947 | ` }/* for(;;) */` |
|    137 | 3948 | `	return SXRET_OK;` |
|     69 | 3949 |  |
|      - | 3950 | `/*` |
|      - | 3951 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3952 | ` */` |
|     90 | 3953 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3954 |  |
|      - | 3955 | `	/* Consume directly */` |
|     91 | 3956 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     45 | 3957 | `	SXUNUSED(pUserData); /* cc warning */` |
|     91 | 3958 | `	return PH7_OK;` |
|      1 | 3959 |  |
|      - | 3960 | `/*` |
|      - | 3961 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3962 | ` *  Return a formatted string.` |
|      - | 3963 | ` * Parameters` |
|      - | 3964 | ` *  $format` |
|      - | 3965 | ` *    The format string (see block comment above)` |
|      - | 3966 | ` * Return` |
|      - | 3967 | ` *  A string produced according to the formatting string format.` |
|      - | 3968 | ` */` |
|     62 | 3969 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3970 |  |
|      - | 3971 | `	const char *zFormat;` |
|      - | 3972 | `	int nLen;` |
|     63 | 3973 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3974 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 3975 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3976 | `		return PH7_OK;` |
|      - | 3977 | `	}` |
|      - | 3978 | `	/* Extract the string format */` |
|     61 | 3979 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 3980 | `	if( nLen < 1 ){` |
|      - | 3981 | `		/* Empty string */` |
|    ! 0 | 3982 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3983 | `		return PH7_OK;` |
|      - | 3984 | `	}` |
|      - | 3985 | `	/* Format the string */` |
|     61 | 3986 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     61 | 3987 | `	return PH7_OK;` |
|     32 | 3988 |  |
|      - | 3989 | `/*` |
|      - | 3990 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3991 | ` */` |
|    130 | 3992 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3993 |  |
|    131 | 3994 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3995 | `	/* Call the VM output consumer directly */` |
|    131 | 3996 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 3997 | `	/* Increment counter */` |
|    131 | 3998 | `	*pCounter += nLen;` |
|    131 | 3999 | `	return PH7_OK;` |
|      1 | 4000 |  |
|      - | 4001 | `/*` |
|      - | 4002 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4003 | ` *  Output a formatted string.` |
|      - | 4004 | ` * Parameters` |
|      - | 4005 | ` *  $format` |
|      - | 4006 | ` *   See sprintf() for a description of format.` |
|      - | 4007 | ` * Return` |
|      - | 4008 | ` *  The length of the outputted string.` |
|      - | 4009 | ` */` |
|     52 | 4010 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4011 |  |
|     53 | 4012 | `	ph7_int64 nCounter = 0;` |
|      - | 4013 | `	const char *zFormat;` |
|      - | 4014 | `	int nLen;` |
|     53 | 4015 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4016 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4017 | `		ph7_result_int(pCtx,0);` |
|      3 | 4018 | `		return PH7_OK;` |
|      - | 4019 | `	}` |
|      - | 4020 | `	/* Extract the string format */` |
|     51 | 4021 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     51 | 4022 | `	if( nLen < 1 ){` |
|      - | 4023 | `		/* Empty string */` |
|    ! 0 | 4024 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4025 | `		return PH7_OK;` |
|      - | 4026 | `	}` |
|      - | 4027 | `	/* Format the string */` |
|     51 | 4028 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4029 | `	/* Return the length of the outputted string */` |
|     51 | 4030 | `	ph7_result_int64(pCtx,nCounter);` |
|     51 | 4031 | `	return PH7_OK;` |
|     27 | 4032 |  |
|      - | 4033 | `/*` |
|      - | 4034 | ` * int vprintf(string $format,array $args)` |
|      - | 4035 | ` *  Output a formatted string.` |
|      - | 4036 | ` * Parameters` |
|      - | 4037 | ` *  $format` |
|      - | 4038 | ` *   See sprintf() for a description of format.` |
|      - | 4039 | ` * Return` |
|      - | 4040 | ` *  The length of the outputted string.` |
|      - | 4041 | ` */` |
|      2 | 4042 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4043 |  |
|      3 | 4044 | `	ph7_int64 nCounter = 0;` |
|      - | 4045 | `	const char *zFormat;` |
|      - | 4046 | `	ph7_hashmap *pMap;` |
|      - | 4047 | `	SySet sArg;` |
|      - | 4048 | `	int nLen,n;` |
|      3 | 4049 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4050 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4051 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4052 | `		return PH7_OK;` |
|      - | 4053 | `	}` |
|      - | 4054 | `	/* Extract the string format */` |
|      3 | 4055 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4056 | `	if( nLen < 1 ){` |
|      - | 4057 | `		/* Empty string */` |
|    ! 0 | 4058 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4059 | `		return PH7_OK;` |
|      - | 4060 | `	}` |
|      - | 4061 | `	/* Point to the hashmap */` |
|      3 | 4062 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4063 | `	/* Extract arguments from the hashmap */` |
|      3 | 4064 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4065 | `	/* Format the string */` |
|      3 | 4066 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4067 | `	/* Return the length of the outputted string */` |
|      3 | 4068 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4069 | `	/* Release the container */` |
|      3 | 4070 | `	SySetRelease(&sArg);` |
|      3 | 4071 | `	return PH7_OK;` |
|      2 | 4072 |  |
|      - | 4073 | `/*` |
|      - | 4074 | ` * int vsprintf(string $format,array $args)` |
|      - | 4075 | ` *  Output a formatted string.` |
|      - | 4076 | ` * Parameters` |
|      - | 4077 | ` *  $format` |
|      - | 4078 | ` *   See sprintf() for a description of format.` |
|      - | 4079 | ` * Return` |
|      - | 4080 | ` *  A string produced according to the formatting string format.` |
|      - | 4081 | ` */` |
|     10 | 4082 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4083 |  |
|      - | 4084 | `	const char *zFormat;` |
|      - | 4085 | `	ph7_hashmap *pMap;` |
|      - | 4086 | `	SySet sArg;` |
|      - | 4087 | `	int nLen,n;` |
|     11 | 4088 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4089 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4090 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4091 | `		return PH7_OK;` |
|      - | 4092 | `	}` |
|      - | 4093 | `	/* Extract the string format */` |
|      7 | 4094 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4095 | `	if( nLen < 1 ){` |
|      - | 4096 | `		/* Empty string */` |
|    ! 0 | 4097 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4098 | `		return PH7_OK;` |
|      - | 4099 | `	}` |
|      - | 4100 | `	/* Point to hashmap */` |
|      7 | 4101 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4102 | `	/* Extract arguments from the hashmap */` |
|      7 | 4103 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4104 | `	/* Format the string */` |
|      7 | 4105 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 4106 | `	/* Release the container */` |
|      7 | 4107 | `	SySetRelease(&sArg);` |
|      7 | 4108 | `	return PH7_OK;` |
|      6 | 4109 |  |
|      - | 4110 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4111 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4112 | `/*` |
|      - | 4113 | ` * Symisc eXtension.` |
|      - | 4114 | ` * string size_format(int64 $size)` |
|      - | 4115 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4116 | ` *  Example:` |
|      - | 4117 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4118 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4119 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4120 | ` * Parameter` |
|      - | 4121 | ` *  $size` |
|      - | 4122 | ` *    Entity size in bytes.` |
|      - | 4123 | ` * Return` |
|      - | 4124 | ` *   Formatted string representation of the given size.` |
|      - | 4125 | ` */` |
|     24 | 4126 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4127 |  |
|      - | 4128 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4129 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4130 | `	sxi32 nRest,i_32;` |
|      - | 4131 | `	ph7_int64 iSize;` |
|     25 | 4132 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4133 |  |
|     25 | 4134 | `	if( nArg < 1 ){` |
|      - | 4135 | `		/* Missing argument,return the empty string */` |
|      3 | 4136 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4137 | `		return PH7_OK;` |
|      - | 4138 | `	}` |
|      - | 4139 | `	/* Extract the given size */` |
|     23 | 4140 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4141 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4142 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4143 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4144 | `		return PH7_OK;` |
|      - | 4145 | `	}` |
|     19 | 4146 | `	for(;;){` |
|     39 | 4147 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4148 | `		iSize >>= 10;` |
|     39 | 4149 | `		c++;` |
|     39 | 4150 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4151 | `			break;` |
|      - | 4152 | `		}` |
|      1 | 4153 | `	}` |
|     19 | 4154 | `	nRest /= 100;` |
|     19 | 4155 | `	if( nRest > 9 ){` |
|    ! 0 | 4156 | `		nRest = 9;` |
|    ! 0 | 4157 | `	}` |
|     19 | 4158 | `	if( iSize > 999 ){` |
|    ! 0 | 4159 | `		c++;` |
|    ! 0 | 4160 | `		nRest = 9;` |
|    ! 0 | 4161 | `		iSize = 0;` |
|    ! 0 | 4162 | `	}` |
|     19 | 4163 | `	i_32 = (sxi32)iSize;` |
|      - | 4164 | `	/* Format */` |
|     19 | 4165 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4166 | `	return PH7_OK;` |
|     13 | 4167 |  |
|      - | 4168 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4169 | `/*` |
|      - | 4170 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4171 | ` *   Calculate the md5 hash of a string.` |
|      - | 4172 | ` * Parameter` |
|      - | 4173 | ` *  $str` |
|      - | 4174 | ` *   Input string` |
|      - | 4175 | ` * $raw_output` |
|      - | 4176 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4177 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4178 | ` * Return` |
|      - | 4179 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4180 | ` */` |
|     10 | 4181 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4182 |  |
|      - | 4183 | `	unsigned char zDigest[16];` |
|     11 | 4184 | `	int raw_output = FALSE;` |
|      - | 4185 | `	const void *pIn;` |
|      - | 4186 | `	int nLen;` |
|     11 | 4187 | `	if( nArg < 1 ){` |
|      - | 4188 | `		/* Missing arguments,return the empty string */` |
|      3 | 4189 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4190 | `		return PH7_OK;` |
|      - | 4191 | `	}` |
|      - | 4192 | `	/* Extract the input string */` |
|      9 | 4193 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4194 | `	if( nLen < 1 ){` |
|      - | 4195 | `		/* Empty string */` |
|    ! 0 | 4196 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4197 | `		return PH7_OK;` |
|      - | 4198 | `	}` |
|      9 | 4199 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4200 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4201 | `	}` |
|      - | 4202 | `	/* Compute the MD5 digest */` |
|      9 | 4203 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 4204 | `	if( raw_output ){` |
|      - | 4205 | `		/* Output raw digest */` |
|      3 | 4206 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4207 | `	}else{` |
|      - | 4208 | `		/* Perform a binary to hex conversion */` |
|      7 | 4209 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4210 | `	}` |
|      9 | 4211 | `	return PH7_OK;` |
|      6 | 4212 |  |
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
|      8 | 4225 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4226 |  |
|      - | 4227 | `	unsigned char zDigest[20];` |
|      9 | 4228 | `	int raw_output = FALSE;` |
|      - | 4229 | `	const void *pIn;` |
|      - | 4230 | `	int nLen;` |
|      9 | 4231 | `	if( nArg < 1 ){` |
|      - | 4232 | `		/* Missing arguments,return the empty string */` |
|      3 | 4233 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4234 | `		return PH7_OK;` |
|      - | 4235 | `	}` |
|      - | 4236 | `	/* Extract the input string */` |
|      7 | 4237 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4238 | `	if( nLen < 1 ){` |
|      - | 4239 | `		/* Empty string */` |
|    ! 0 | 4240 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4241 | `		return PH7_OK;` |
|      - | 4242 | `	}` |
|      7 | 4243 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 4244 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 4245 | `	}` |
|      - | 4246 | `	/* Compute the SHA1 digest */` |
|      7 | 4247 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 4248 | `	if( raw_output ){` |
|      - | 4249 | `		/* Output raw digest */` |
|      3 | 4250 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4251 | `	}else{` |
|      - | 4252 | `		/* Perform a binary to hex conversion */` |
|      5 | 4253 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4254 | `	}` |
|      7 | 4255 | `	return PH7_OK;` |
|      5 | 4256 |  |
|      - | 4257 | `/*` |
|      - | 4258 | ` * int64 crc32(string $str)` |
|      - | 4259 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4260 | ` * Parameter` |
|      - | 4261 | ` *  $str` |
|      - | 4262 | ` *   Input string` |
|      - | 4263 | ` * Return` |
|      - | 4264 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4265 | ` */` |
|      4 | 4266 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4267 |  |
|      - | 4268 | `	const void *pIn;` |
|      - | 4269 | `	sxu32 nCRC;` |
|      - | 4270 | `	int nLen;` |
|      5 | 4271 | `	if( nArg < 1 ){` |
|      - | 4272 | `		/* Missing arguments,return 0 */` |
|      3 | 4273 | `		ph7_result_int(pCtx,0);` |
|      3 | 4274 | `		return PH7_OK;` |
|      - | 4275 | `	}` |
|      - | 4276 | `	/* Extract the input string */` |
|      3 | 4277 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4278 | `	if( nLen < 1 ){` |
|      - | 4279 | `		/* Empty string */` |
|    ! 0 | 4280 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4281 | `		return PH7_OK;` |
|      - | 4282 | `	}` |
|      - | 4283 | `	/* Calculate the sum */` |
|      3 | 4284 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4285 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4286 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4287 | `	return PH7_OK;` |
|      3 | 4288 |  |
|      - | 4289 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4290 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4291 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4292 | `/*` |
|      - | 4293 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4294 |  |
|      - | 4295 | ` */` |
|      4 | 4296 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4297 | `	const char *zInput, /* Raw input */` |
|      - | 4298 | `	int nByte,  /* Input length */` |
|      - | 4299 | `	int delim,  /* Delimiter */` |
|      - | 4300 | `	int encl,   /* Enclosure */` |
|      - | 4301 | `	int escape,  /* Escape character */` |
|      - | 4302 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4303 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4304 | `	)` |
|      1 | 4305 |  |
|      5 | 4306 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4307 | `	const char *zIn = zInput;` |
|      - | 4308 | `	const char *zPtr;` |
|      - | 4309 | `	int isEnc;` |
|      - | 4310 | `	/* Start processing */` |
|      8 | 4311 | `	for(;;){` |
|     17 | 4312 | `		if( zIn >= zEnd ){` |
|      - | 4313 | `			/* No more input to process */` |
|      5 | 4314 | `			break;` |
|      - | 4315 | `		}` |
|     13 | 4316 | `		isEnc = 0;` |
|     13 | 4317 | `		zPtr = zIn;` |
|      - | 4318 | `		/* Find the first delimiter */` |
|     27 | 4319 | `		while( zIn < zEnd ){` |
|     23 | 4320 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4321 | `				/* Delimiter found,break imediately */` |
|      5 | 4322 | `				break;` |
|     15 | 4323 | `			}else if( zIn[0] == encl ){` |
|      - | 4324 | `				/* Inside enclosure? */` |
|    ! 0 | 4325 | `				isEnc = !isEnc;` |
|     15 | 4326 | `			}else if( zIn[0] == escape ){` |
|      - | 4327 | `				/* Escape sequence */` |
|    ! 0 | 4328 | `				zIn++;` |
|    ! 0 | 4329 | `			}` |
|      - | 4330 | `			/* Advance the cursor */` |
|     15 | 4331 | `			zIn++;` |
|      1 | 4332 | `		}` |
|     13 | 4333 | `		if( zIn > zPtr ){` |
|     13 | 4334 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4335 | `			sxi32 rc;` |
|      - | 4336 | `			/* Invoke the supllied callback */` |
|     13 | 4337 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4338 | `				zPtr++;` |
|    ! 0 | 4339 | `				nByteChunk-=2;` |
|    ! 0 | 4340 | `			}` |
|     13 | 4341 | `			if( nByteChunk > 0 ){` |
|     13 | 4342 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4343 | `				if( rc == SXERR_ABORT ){` |
|      - | 4344 | `					/* User callback request an operation abort */` |
|    ! 0 | 4345 | `					break;` |
|      - | 4346 | `				}` |
|      6 | 4347 | `			}` |
|      6 | 4348 | `		}` |
|      - | 4349 | `		/* Ignore trailing delimiter */` |
|     21 | 4350 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4351 | `			zIn++;` |
|      1 | 4352 | `		}` |
|      1 | 4353 | `	}` |
|      5 | 4354 | `	return SXRET_OK;` |
|      1 | 4355 |  |
|      - | 4356 | `/*` |
|      - | 4357 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4358 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4359 | ` * argument to this callback.` |
|      - | 4360 | ` */` |
|     12 | 4361 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4362 |  |
|     13 | 4363 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4364 | `	ph7_value sEntry;` |
|      - | 4365 | `	SyString sToken;` |
|      - | 4366 | `	/* Insert the token in the given array */` |
|     13 | 4367 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4368 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4369 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4370 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4371 | `		return SXRET_OK;` |
|      - | 4372 | `	}` |
|     13 | 4373 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4374 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4375 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4376 | `	return SXRET_OK;` |
|      7 | 4377 |  |
|      - | 4378 | `/*` |
|      - | 4379 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4380 | ` *  Parse a CSV string into an array.` |
|      - | 4381 | ` * Parameters` |
|      - | 4382 | ` *  $input` |
|      - | 4383 | ` *   The string to parse.` |
|      - | 4384 | ` *  $delimiter` |
|      - | 4385 | ` *   Set the field delimiter (one character only).` |
|      - | 4386 | ` *  $enclosure` |
|      - | 4387 | ` *   Set the field enclosure character (one character only).` |
|      - | 4388 | ` *  $escape` |
|      - | 4389 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4390 | ` * Return` |
|      - | 4391 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4392 | ` */` |
|      4 | 4393 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4394 |  |
|      - | 4395 | `	const char *zInput,*zPtr;` |
|      - | 4396 | `	ph7_value *pArray;` |
|      5 | 4397 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4398 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4399 | `	int escape = '\\';  /* Escape character */` |
|      - | 4400 | `	int nLen;` |
|      5 | 4401 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4402 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4403 | `		ph7_result_null(pCtx);` |
|      3 | 4404 | `		return PH7_OK;` |
|      - | 4405 | `	}` |
|      - | 4406 | `	/* Extract the raw input */` |
|      3 | 4407 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4408 | `	if( nArg > 1 ){` |
|      - | 4409 | `		int i;` |
|      3 | 4410 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4411 | `			/* Extract the delimiter */` |
|      3 | 4412 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4413 | `			if( i > 0 ){` |
|      3 | 4414 | `				delim = zPtr[0];` |
|      1 | 4415 | `			}` |
|      1 | 4416 | `		}` |
|      3 | 4417 | `		if( nArg > 2 ){` |
|      3 | 4418 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4419 | `				/* Extract the enclosure */` |
|      3 | 4420 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4421 | `				if( i > 0 ){` |
|      3 | 4422 | `					encl = zPtr[0];` |
|      1 | 4423 | `				}` |
|      1 | 4424 | `			}` |
|      3 | 4425 | `			if( nArg > 3 ){` |
|      3 | 4426 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4427 | `					/* Extract the escape character */` |
|      3 | 4428 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4429 | `					if( i > 0 ){` |
|      3 | 4430 | `						escape = zPtr[0];` |
|      1 | 4431 | `					}` |
|      1 | 4432 | `				}` |
|      1 | 4433 | `			}` |
|      1 | 4434 | `		}` |
|      1 | 4435 | `	}` |
|      - | 4436 | `	/* Create our array */` |
|      3 | 4437 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4438 | `	if( pArray == 0 ){` |
|    ! 0 | 4439 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4440 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4441 | `		return PH7_OK;` |
|      - | 4442 | `	}` |
|      - | 4443 | `	/* Parse the raw input */` |
|      3 | 4444 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4445 | `	/* Return the freshly created array */` |
|      3 | 4446 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4447 | `	return PH7_OK;` |
|      3 | 4448 |  |
|      - | 4449 | `/*` |
|      - | 4450 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4451 | ` * container.` |
|      - | 4452 | ` * Refer to [strip_tags()].` |
|      - | 4453 | ` */` |
|     10 | 4454 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4455 |  |
|     11 | 4456 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4457 | `	const char *zPtr;` |
|      - | 4458 | `	SyString sEntry;` |
|      - | 4459 | `	/* Strip tags */` |
|     10 | 4460 | `	for(;;){` |
|     45 | 4461 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4462 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4463 | `				zTag++;` |
|      1 | 4464 | `		}` |
|     21 | 4465 | `		if( zTag >= zEnd ){` |
|     11 | 4466 | `			break;` |
|      - | 4467 | `		}` |
|     11 | 4468 | `		zPtr = zTag;` |
|      - | 4469 | `		/* Delimit the tag */` |
|     25 | 4470 | `		while(zTag < zEnd ){` |
|     25 | 4471 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4472 | `				/* UTF-8 stream */` |
|      3 | 4473 | `				zTag++;` |
|      5 | 4474 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4475 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4476 | `				break;` |
|    ! 0 | 4477 | `			}else{` |
|     13 | 4478 | `				zTag++;` |
|      - | 4479 | `			}` |
|      1 | 4480 | `		}` |
|     11 | 4481 | `		if( zTag > zPtr ){` |
|      - | 4482 | `			/* Perform the insertion */` |
|     11 | 4483 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4484 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4485 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4486 | `		}` |
|      - | 4487 | `		/* Jump the trailing '>' */` |
|     11 | 4488 | `		zTag++;` |
|      1 | 4489 | `	}` |
|     11 | 4490 | `	return SXRET_OK;` |
|      1 | 4491 |  |
|      - | 4492 | `/*` |
|      - | 4493 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4494 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4495 | ` * Refer to [strip_tags()].` |
|      - | 4496 | ` */` |
|     36 | 4497 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4498 |  |
|     37 | 4499 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4500 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4501 | `		SyString sTag;` |
|     85 | 4502 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4503 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4504 | `			zTag++;` |
|      1 | 4505 | `		}` |
|      - | 4506 | `		/* Delimit the tag */` |
|     25 | 4507 | `		zCur = zTag;` |
|     77 | 4508 | `		while(zTag < zEnd ){` |
|     77 | 4509 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4510 | `				/* UTF-8 stream */` |
|      5 | 4511 | `				zTag++;` |
|      9 | 4512 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 4513 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 4514 | `				break;` |
|    ! 0 | 4515 | `			}else{` |
|     49 | 4516 | `				zTag++;` |
|      - | 4517 | `			}` |
|      1 | 4518 | `		}` |
|     25 | 4519 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 4520 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 4521 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 4522 | `		if( sTag.nByte > 0 ){` |
|      - | 4523 | `			SyString *aEntry,*pEntry;` |
|      - | 4524 | `			sxi32 rc;` |
|      - | 4525 | `			sxu32 n;` |
|      - | 4526 | `			/* Perform the lookup */` |
|     25 | 4527 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 4528 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 4529 | `				pEntry = &aEntry[n];` |
|      - | 4530 | `				/* Do the comparison */` |
|     25 | 4531 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 4532 | `				if( !rc ){` |
|     21 | 4533 | `					return SXRET_OK;` |
|      - | 4534 | `				}` |
|      3 | 4535 | `			}` |
|      2 | 4536 | `		}` |
|      2 | 4537 | `	}` |
|      - | 4538 | `	/* No such tag */` |
|     17 | 4539 | `	return SXERR_NOTFOUND;` |
|     19 | 4540 |  |
|      - | 4541 | `/*` |
|      - | 4542 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 4543 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 4544 | ` * Refer to [strip_tags()].` |
|      - | 4545 | ` */` |
|     16 | 4546 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 4547 |  |
|     17 | 4548 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4549 | `	const char *zPtr,*zTag;` |
|      - | 4550 | `	SySet sSet;` |
|      - | 4551 | `	/* initialize the set of allowed tags */` |
|     17 | 4552 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 4553 | `	if( nTaglen > 0 ){` |
|      - | 4554 | `		/* Set of allowed tags */` |
|     11 | 4555 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 4556 | `	}` |
|      - | 4557 | `	/* Set the empty string */` |
|     17 | 4558 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4559 | `	/* Start processing */` |
|     26 | 4560 | `	for(;;){` |
|     53 | 4561 | `		if(zIn >= zEnd){` |
|      - | 4562 | `			/* No more input to process */` |
|     15 | 4563 | `			break;` |
|      - | 4564 | `		}` |
|     39 | 4565 | `		zPtr = zIn;` |
|      - | 4566 | `		/* Find a tag */` |
|    133 | 4567 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 4568 | `			zIn++;` |
|      1 | 4569 | `		}` |
|     39 | 4570 | `		if( zIn > zPtr ){` |
|      - | 4571 | `			/* Consume raw input */` |
|     21 | 4572 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 4573 | `		}` |
|      - | 4574 | `		/* Ignore trailing null bytes */` |
|     39 | 4575 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 4576 | `			zIn++;` |
|    ! 0 | 4577 | `		}` |
|     39 | 4578 | `		if(zIn >= zEnd){` |
|      - | 4579 | `			/* No more input to process */` |
|      3 | 4580 | `			break;` |
|      - | 4581 | `		}` |
|     37 | 4582 | `		if( zIn[0] == '<' ){` |
|      - | 4583 | `			sxi32 rc;` |
|     37 | 4584 | `			zTag = zIn++;` |
|      - | 4585 | `			/* Delimit the tag */` |
|    127 | 4586 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 4587 | `				zIn++;` |
|      1 | 4588 | `			}` |
|     37 | 4589 | `			if( zIn < zEnd ){` |
|     37 | 4590 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 4591 | `			}` |
|      - | 4592 | `			/* Query the set */` |
|     37 | 4593 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 4594 | `			if( rc == SXRET_OK ){` |
|      - | 4595 | `				/* Keep the tag */` |
|     21 | 4596 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 4597 | `			}` |
|     18 | 4598 | `		}` |
|      1 | 4599 | `	}` |
|      - | 4600 | `	/* Cleanup */` |
|     17 | 4601 | `	SySetRelease(&sSet);` |
|     17 | 4602 | `	return SXRET_OK;` |
|      1 | 4603 |  |
|      - | 4604 | `/*` |
|      - | 4605 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 4606 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 4607 | ` * Parameters` |
|      - | 4608 | ` *  $str` |
|      - | 4609 | ` *  The input string.` |
|      - | 4610 | ` * $allowable_tags` |
|      - | 4611 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 4612 | ` * Return` |
|      - | 4613 | ` *  Returns the stripped string.` |
|      - | 4614 | ` */` |
|     16 | 4615 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4616 |  |
|     17 | 4617 | `	const char *zTaglist = 0;` |
|      - | 4618 | `	const char *zString;` |
|     17 | 4619 | `	int nTaglen = 0;` |
|      - | 4620 | `	int nLen;` |
|     17 | 4621 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4622 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4623 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4624 | `		return PH7_OK;` |
|      - | 4625 | `	}` |
|      - | 4626 | `	/* Point to the raw string */` |
|     15 | 4627 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 4628 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 4629 | `		/* Allowed tag */` |
|     11 | 4630 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 4631 | `	}` |
|      - | 4632 | `	/* Process input */` |
|     15 | 4633 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 4634 | `	return PH7_OK;` |
|      9 | 4635 |  |
|      - | 4636 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4637 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4638 | `/*` |
|      - | 4639 | ` * string str_shuffle(string $str)` |
|      - | 4640 |  |
|      - | 4641 | ` *  Randomly shuffles a string.` |
|      - | 4642 | ` * Parameters` |
|      - | 4643 | ` *  $str` |
|      - | 4644 | ` *   The input string.` |
|      - | 4645 | ` * Return` |
|      - | 4646 | ` *  Returns the shuffled string.` |
|      - | 4647 | ` */` |
|     12 | 4648 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4649 |  |
|      - | 4650 | `	const char *zString;` |
|      - | 4651 | `	int nLen,i,c;` |
|      - | 4652 | `	sxu32 iR;` |
|     13 | 4653 | `	if( nArg < 1 ){` |
|      - | 4654 | `		/* Missing arguments,return the empty string */` |
|      3 | 4655 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4656 | `		return PH7_OK;` |
|      - | 4657 | `	}` |
|      - | 4658 | `	/* Extract the target string */` |
|     11 | 4659 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4660 | `	if( nLen < 1 ){` |
|      - | 4661 | `		/* Nothing to shuffle */` |
|      3 | 4662 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4663 | `		return PH7_OK;` |
|      - | 4664 | `	}` |
|      - | 4665 | `	/* Shuffle the string */` |
|     43 | 4666 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 4667 | `		/* Generate a random number first */` |
|     35 | 4668 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 4669 | `		/* Extract a random offset */` |
|     35 | 4670 | `		c = zString[iR % nLen];` |
|      - | 4671 | `		/* Append it */` |
|     35 | 4672 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 4673 | `	}` |
|      9 | 4674 | `	return PH7_OK;` |
|      7 | 4675 |  |
|      - | 4676 | `/*` |
|      - | 4677 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 4678 | ` *  Convert a string to an array.` |
|      - | 4679 | ` * Parameters` |
|      - | 4680 | ` * $string` |
|      - | 4681 | ` *  The input string.` |
|      - | 4682 | ` * $split_length` |
|      - | 4683 | ` *  Maximum length of the chunk.` |
|      - | 4684 | ` * Return` |
|      - | 4685 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 4686 | ` *  except possibly the last one which may be shorter.` |
|      - | 4687 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 4688 | ` *  as the first (and only) array element.` |
|      - | 4689 | ` *  An empty string returns an empty array.` |
|      - | 4690 | ` * Errors` |
|      - | 4691 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 4692 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 4693 | ` *  ValueError if $split_length is less than 1.` |
|      - | 4694 | ` */` |
|     28 | 4695 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4696 |  |
|      - | 4697 | `	const char *zString,*zEnd;` |
|      - | 4698 | `	ph7_value *pArray,*pValue;` |
|      - | 4699 | `	int split_len;` |
|      - | 4700 | `	int nLen;` |
|     30 | 4701 | `	if( nArg < 1 ){` |
|      4 | 4702 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4703 | `			"ArgumentCountError",` |
|      - | 4704 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 4705 | `			nArg` |
|      - | 4706 | `			);` |
|      - | 4707 | `	}` |
|      - | 4708 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 4709 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     38 | 4710 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 4711 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 4712 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4713 | `			"TypeError",` |
|      - | 4714 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 4715 | `			ph7_type_name(apArg[0])` |
|      - | 4716 | `			);` |
|      - | 4717 | `	}` |
|      - | 4718 | `	/* Point to the target string */` |
|     26 | 4719 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     26 | 4720 | `	split_len = (int)sizeof(char);` |
|     26 | 4721 | `	if( nArg > 1 ){` |
|      - | 4722 | `		/* Split length */` |
|     16 | 4723 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     16 | 4724 | `		if( split_len < 1 ){` |
|      5 | 4725 | `			return PH7_VmThrowException(pCtx,` |
|      - | 4726 | `				"ValueError",` |
|      - | 4727 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 4728 | `				);` |
|      - | 4729 | `		}` |
|     11 | 4730 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 4731 | `			split_len = nLen;` |
|      1 | 4732 | `		}` |
|      5 | 4733 | `	}` |
|      - | 4734 | `	/* Create the array and the scalar value */` |
|     21 | 4735 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 4736 | `	/*Chunk value */` |
|     21 | 4737 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 4738 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 4739 | `		/* Return FALSE */` |
|    ! 0 | 4740 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4741 | `		return PH7_OK;` |
|      - | 4742 | `	}` |
|      - | 4743 | `	/* Point to the end of the string */` |
|     21 | 4744 | `	zEnd = &zString[nLen];` |
|      - | 4745 | `	/* Perform the requested operation */` |
|     48 | 4746 | `	for(;;){` |
|      - | 4747 | `		int nMax;` |
|     59 | 4748 | `		if( zString >= zEnd ){` |
|      - | 4749 | `			/* No more input to process */` |
|     21 | 4750 | `			break;` |
|      - | 4751 | `		}` |
|     39 | 4752 | `		nMax = (int)(zEnd-zString);` |
|     39 | 4753 | `		if( nMax < split_len ){` |
|      3 | 4754 | `			split_len = nMax;` |
|      1 | 4755 | `		}` |
|      - | 4756 | `		/* Copy the current chunk */` |
|     39 | 4757 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 4758 | `		/* Insert it */` |
|     39 | 4759 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 4760 | `		/* reset the string cursor */` |
|     39 | 4761 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 4762 | `		/* Update position */` |
|     39 | 4763 | `		zString += split_len;` |
|      1 | 4764 | `	}` |
|      - | 4765 | `	/*` |
|      - | 4766 | `	 * Return the array.` |
|      - | 4767 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 4768 | `	 * upon we return from this function.` |
|      - | 4769 | `	 */` |
|     21 | 4770 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 4771 | `	return PH7_OK;` |
|     16 | 4772 |  |
|      - | 4773 | `/*` |
|      - | 4774 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 4775 | ` * Refer to [strspn()].` |
|      - | 4776 | ` */` |
|     28 | 4777 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 4778 |  |
|     29 | 4779 | `	const char *zIn = *pzIn;` |
|      - | 4780 | `	const char *zPtr;` |
|      - | 4781 | `	/* Ignore leading white spaces */` |
|     29 | 4782 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 4783 | `		zIn++;` |
|    ! 0 | 4784 | `	}` |
|     29 | 4785 | `	if( zIn >= zEnd ){` |
|      - | 4786 | `		/* End of input */` |
|    ! 0 | 4787 | `		return SXERR_EOF;` |
|      - | 4788 | `	}` |
|     29 | 4789 | `	zPtr = zIn;` |
|      - | 4790 | `	/* Extract the token */` |
|    201 | 4791 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 4792 | `		zIn++;` |
|      1 | 4793 | `	}` |
|     29 | 4794 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4795 | `	/* Synchronize pointers */` |
|     29 | 4796 | `	*pzIn = zIn;` |
|      - | 4797 | `	/* Return to the caller */` |
|     29 | 4798 | `	return SXRET_OK;` |
|     15 | 4799 |  |
|      - | 4800 | `/*` |
|      - | 4801 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 4802 | ` * return the longest match.` |
|      - | 4803 | ` * Refer to [strspn()].` |
|      - | 4804 | ` */` |
|     18 | 4805 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4806 |  |
|     19 | 4807 | `	const char *zEnd = &zString[nLen];` |
|     19 | 4808 | `	const char *zIn = zString;` |
|      - | 4809 | `	int i,c;` |
|     45 | 4810 | `	for(;;){` |
|     91 | 4811 | `		if( zString >= zEnd ){` |
|      7 | 4812 | `			break;` |
|      - | 4813 | `		}` |
|      - | 4814 | `		/* Extract current character */` |
|     85 | 4815 | `		c = zString[0];` |
|      - | 4816 | `		/* Perform the lookup */` |
|    383 | 4817 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 4818 | `			if( c == zMask[i] ){` |
|      - | 4819 | `				/* Character found */` |
|     73 | 4820 | `				break;` |
|      - | 4821 | `			}` |
|    150 | 4822 | `		}` |
|     85 | 4823 | `		if( i >= nMaskLen ){` |
|      - | 4824 | `			/* Character not in the current mask,break immediately */` |
|     13 | 4825 | `			break;` |
|      - | 4826 | `		}` |
|      - | 4827 | `		/* Advance cursor */` |
|     73 | 4828 | `		zString++;` |
|      1 | 4829 | `	}` |
|      - | 4830 | `	/* Longest match */` |
|     19 | 4831 | `	return (int)(zString-zIn);` |
|      1 | 4832 |  |
|      - | 4833 | `/*` |
|      - | 4834 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4835 | ` * Refer to [strcspn()].` |
|      - | 4836 | ` */` |
|     10 | 4837 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4838 |  |
|     11 | 4839 | `	const char *zEnd = &zString[nLen];` |
|     11 | 4840 | `	const char *zIn = zString;` |
|      - | 4841 | `	int i,c;` |
|     12 | 4842 | `	for(;;){` |
|     25 | 4843 | `		if( zString >= zEnd ){` |
|      3 | 4844 | `			break;` |
|      - | 4845 | `		}` |
|      - | 4846 | `		/* Extract current character */` |
|     23 | 4847 | `		c = zString[0];` |
|      - | 4848 | `		/* Perform the lookup */` |
|     51 | 4849 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 4850 | `			if( c == zMask[i] ){` |
|      9 | 4851 | `				break;` |
|      - | 4852 | `			}` |
|     15 | 4853 | `		}` |
|     23 | 4854 | `		if( i < nMaskLen ){` |
|      - | 4855 | `			/* Character in the current mask,break immediately */` |
|      9 | 4856 | `			break;` |
|      - | 4857 | `		}` |
|      - | 4858 | `		/* Advance cursor */` |
|     15 | 4859 | `		zString++;` |
|      1 | 4860 | `	}` |
|      - | 4861 | `	/* Longest match */` |
|     11 | 4862 | `	return (int)(zString-zIn);` |
|      1 | 4863 |  |
|      - | 4864 | `/*` |
|      - | 4865 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4866 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4867 | ` *  of characters contained within a given mask.` |
|      - | 4868 | ` * Parameters` |
|      - | 4869 | ` * $str` |
|      - | 4870 | ` *  The input string.` |
|      - | 4871 | ` * $mask` |
|      - | 4872 | ` *  The list of allowable characters.` |
|      - | 4873 | ` * $start` |
|      - | 4874 | ` *  The position in subject to start searching.` |
|      - | 4875 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4876 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4877 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4878 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4879 | ` *  start'th position from the end of subject.` |
|      - | 4880 | ` * $length` |
|      - | 4881 | ` *  The length of the segment from subject to examine.` |
|      - | 4882 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4883 | ` *  characters after the starting position.` |
|      - | 4884 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4885 | ` *  position up to length characters from the end of subject.` |
|      - | 4886 | ` * Return` |
|      - | 4887 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4888 | ` * in mask.` |
|      - | 4889 | ` */` |
|     26 | 4890 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4891 |  |
|      - | 4892 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4893 | `	int iMasklen,iLen;` |
|      - | 4894 | `	SyString sToken;` |
|     27 | 4895 | `	int iCount = 0;` |
|      - | 4896 | `	int rc;` |
|     27 | 4897 | `	if( nArg < 2 ){` |
|      - | 4898 | `		/* Missing agruments,return zero */` |
|      3 | 4899 | `		ph7_result_int(pCtx,0);` |
|      3 | 4900 | `		return PH7_OK;` |
|      - | 4901 | `	}` |
|      - | 4902 | `	/* Extract the target string */` |
|     25 | 4903 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4904 | `	/* Extract the mask */` |
|     25 | 4905 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 4906 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 4907 | `		/* Nothing to process,return zero */` |
|      7 | 4908 | `		ph7_result_int(pCtx,0);` |
|      7 | 4909 | `		return PH7_OK;` |
|      - | 4910 | `	}` |
|     19 | 4911 | `	if( nArg > 2 ){` |
|      - | 4912 | `		int nOfft;` |
|      - | 4913 | `		/* Extract the offset */` |
|      9 | 4914 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 4915 | `		if( nOfft < 0 ){` |
|    ! 0 | 4916 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4917 | `			if( zBase > zString ){` |
|    ! 0 | 4918 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4919 | `				zString = zBase;` |
|    ! 0 | 4920 | `			}else{` |
|      - | 4921 | `				/* Invalid offset */` |
|    ! 0 | 4922 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4923 | `				return PH7_OK;` |
|      - | 4924 | `			}` |
|    ! 0 | 4925 | `		}else{` |
|      9 | 4926 | `			if( nOfft >= iLen ){` |
|      - | 4927 | `				/* Invalid offset */` |
|    ! 0 | 4928 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4929 | `				return PH7_OK;` |
|    ! 0 | 4930 | `			}else{` |
|      - | 4931 | `				/* Update offset */` |
|      9 | 4932 | `				zString += nOfft;` |
|      9 | 4933 | `				iLen -= nOfft;` |
|      - | 4934 | `			}` |
|      - | 4935 | `		}` |
|      9 | 4936 | `		if( nArg > 3 ){` |
|      - | 4937 | `			int iUserlen;` |
|      - | 4938 | `			/* Extract the desired length */` |
|      9 | 4939 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 4940 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 4941 | `				iLen = iUserlen;` |
|      2 | 4942 | `			}` |
|      4 | 4943 | `		}` |
|      4 | 4944 | `	}` |
|      - | 4945 | `	/* Point to the end of the string */` |
|     19 | 4946 | `	zEnd = &zString[iLen];` |
|      - | 4947 | `	/* Extract the first non-space token */` |
|     19 | 4948 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 4949 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4950 | `		/* Compare against the current mask */` |
|     19 | 4951 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 4952 | `	}` |
|      - | 4953 | `	/* Longest match */` |
|     19 | 4954 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 4955 | `	return PH7_OK;` |
|     14 | 4956 |  |
|      - | 4957 | `/*` |
|      - | 4958 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4959 | ` *  Find length of initial segment not matching mask.` |
|      - | 4960 | ` * Parameters` |
|      - | 4961 | ` * $str` |
|      - | 4962 | ` *  The input string.` |
|      - | 4963 | ` * $mask` |
|      - | 4964 | ` *  The list of not allowed characters.` |
|      - | 4965 | ` * $start` |
|      - | 4966 | ` *  The position in subject to start searching.` |
|      - | 4967 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4968 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4969 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4970 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4971 | ` *  start'th position from the end of subject.` |
|      - | 4972 | ` * $length` |
|      - | 4973 | ` *  The length of the segment from subject to examine.` |
|      - | 4974 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4975 | ` *  characters after the starting position.` |
|      - | 4976 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4977 | ` *  position up to length characters from the end of subject.` |
|      - | 4978 | ` * Return` |
|      - | 4979 | ` *  Returns the length of the segment as an integer.` |
|      - | 4980 | ` */` |
|     16 | 4981 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4982 |  |
|      - | 4983 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4984 | `	int iMasklen,iLen;` |
|      - | 4985 | `	SyString sToken;` |
|     17 | 4986 | `	int iCount = 0;` |
|      - | 4987 | `	int rc;` |
|     17 | 4988 | `	if( nArg < 2 ){` |
|      - | 4989 | `		/* Missing agruments,return zero */` |
|      3 | 4990 | `		ph7_result_int(pCtx,0);` |
|      3 | 4991 | `		return PH7_OK;` |
|      - | 4992 | `	}` |
|      - | 4993 | `	/* Extract the target string */` |
|     15 | 4994 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4995 | `	/* Extract the mask */` |
|     15 | 4996 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 4997 | `	if( iLen < 1 ){` |
|      - | 4998 | `		/* Nothing to process,return zero */` |
|    ! 0 | 4999 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5000 | `		return PH7_OK;` |
|      - | 5001 | `	}` |
|     15 | 5002 | `	if( iMasklen < 1 ){` |
|      - | 5003 | `		/* No given mask,return the string length */` |
|      3 | 5004 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5005 | `		return PH7_OK;` |
|      - | 5006 | `	}` |
|     13 | 5007 | `	if( nArg > 2 ){` |
|      - | 5008 | `		int nOfft;` |
|      - | 5009 | `		/* Extract the offset */` |
|     11 | 5010 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5011 | `		if( nOfft < 0 ){` |
|    ! 0 | 5012 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5013 | `			if( zBase > zString ){` |
|    ! 0 | 5014 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5015 | `				zString = zBase;` |
|    ! 0 | 5016 | `			}else{` |
|      - | 5017 | `				/* Invalid offset */` |
|    ! 0 | 5018 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5019 | `				return PH7_OK;` |
|      - | 5020 | `			}` |
|    ! 0 | 5021 | `		}else{` |
|     11 | 5022 | `			if( nOfft >= iLen ){` |
|      - | 5023 | `				/* Invalid offset */` |
|      3 | 5024 | `				ph7_result_int(pCtx,0);` |
|      3 | 5025 | `				return PH7_OK;` |
|    ! 0 | 5026 | `			}else{` |
|      - | 5027 | `				/* Update offset */` |
|      9 | 5028 | `				zString += nOfft;` |
|      9 | 5029 | `				iLen -= nOfft;` |
|      - | 5030 | `			}` |
|      - | 5031 | `		}` |
|      9 | 5032 | `		if( nArg > 3 ){` |
|      - | 5033 | `			int iUserlen;` |
|      - | 5034 | `			/* Extract the desired length */` |
|    ! 0 | 5035 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5036 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5037 | `				iLen = iUserlen;` |
|    ! 0 | 5038 | `			}` |
|    ! 0 | 5039 | `		}` |
|      4 | 5040 | `	}` |
|      - | 5041 | `	/* Point to the end of the string */` |
|     11 | 5042 | `	zEnd = &zString[iLen];` |
|      - | 5043 | `	/* Extract the first non-space token */` |
|     11 | 5044 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5045 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5046 | `		/* Compare against the current mask */` |
|     11 | 5047 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5048 | `	}` |
|      - | 5049 | `	/* Longest match */` |
|     11 | 5050 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5051 | `	return PH7_OK;` |
|      9 | 5052 |  |
|      - | 5053 | `/*` |
|      - | 5054 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5055 | ` *  Search a string for any of a set of characters.` |
|      - | 5056 | ` * Parameters` |
|      - | 5057 | ` *  $haystack` |
|      - | 5058 | ` *   The string where char_list is looked for.` |
|      - | 5059 | ` *  $char_list` |
|      - | 5060 | ` *   This parameter is case sensitive.` |
|      - | 5061 | ` * Return` |
|      - | 5062 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5063 | ` */` |
|      6 | 5064 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5065 |  |
|      - | 5066 | `	const char *zString,*zList,*zEnd;` |
|      - | 5067 | `	int iLen,iListLen,i,c;` |
|      - | 5068 | `	sxu32 nOfft,nMax;` |
|      - | 5069 | `	sxi32 rc;` |
|      7 | 5070 | `	if( nArg < 2 ){` |
|      - | 5071 | `		/* Missing arguments,return FALSE */` |
|      3 | 5072 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5073 | `		return PH7_OK;` |
|      - | 5074 | `	}` |
|      - | 5075 | `	/* Extract the haystack and the char list */` |
|      5 | 5076 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 5077 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 5078 | `	if( iLen < 1 ){` |
|      - | 5079 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 5080 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5081 | `		return PH7_OK;` |
|      - | 5082 | `	}` |
|      - | 5083 | `	/* Point to the end of the string */` |
|      5 | 5084 | `	zEnd = &zString[iLen];` |
|      5 | 5085 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 5086 | `	/* perform the requested operation */` |
|     15 | 5087 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 5088 | `		c = zList[i];` |
|     11 | 5089 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 5090 | `		if( rc == SXRET_OK ){` |
|      5 | 5091 | `			if( nMax < nOfft ){` |
|      3 | 5092 | `				nOfft = nMax;` |
|      1 | 5093 | `			}` |
|      2 | 5094 | `		}` |
|      6 | 5095 | `	}` |
|      5 | 5096 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 5097 | `		/* No such substring,return FALSE */` |
|      3 | 5098 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5099 | `	}else{` |
|      - | 5100 | `		/* Return the substring */` |
|      3 | 5101 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 5102 | `	}` |
|      5 | 5103 | `	return PH7_OK;` |
|      4 | 5104 |  |
|      - | 5105 | `/*` |
|      - | 5106 | ` * string soundex(string $str)` |
|      - | 5107 | ` *  Calculate the soundex key of a string.` |
|      - | 5108 | ` * Parameters` |
|      - | 5109 | ` *  $str` |
|      - | 5110 | ` *   The input string.` |
|      - | 5111 | ` * Return` |
|      - | 5112 | ` *  Returns the soundex key as a string.` |
|      - | 5113 | ` * Note:` |
|      - | 5114 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 5115 | ` * source tree.` |
|      - | 5116 | ` */` |
|     20 | 5117 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5118 |  |
|      - | 5119 | `	const unsigned char *zIn;` |
|      - | 5120 | `	char zResult[8];` |
|      - | 5121 | `	int i, j;` |
|      - | 5122 | `	static const unsigned char iCode[] = {` |
|      - | 5123 |  |
|      - | 5124 |  |
|      - | 5125 |  |
|      - | 5126 |  |
|      - | 5127 |  |
|      - | 5128 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5129 |  |
|      - | 5130 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 5131 | `	};` |
|     21 | 5132 | `	if( nArg < 1 ){` |
|      - | 5133 | `		/* Missing arguments,return the empty string */` |
|      3 | 5134 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5135 | `		return PH7_OK;` |
|      - | 5136 | `	}` |
|     19 | 5137 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 5138 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 5139 | `	if( zIn[i] ){` |
|     17 | 5140 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 5141 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 5142 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 5143 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 5144 | `			if( code>0 ){` |
|     45 | 5145 | `				if( code!=prevcode ){` |
|     33 | 5146 | `					prevcode = (unsigned char)code;` |
|     33 | 5147 | `					zResult[j++] = (char)code + '0';` |
|     16 | 5148 | `				}` |
|     23 | 5149 | `			}else{` |
|     49 | 5150 | `				prevcode = 0;` |
|      - | 5151 | `			}` |
|     47 | 5152 | `		}` |
|     33 | 5153 | `		while( j<4 ){` |
|     17 | 5154 | `			zResult[j++] = '0';` |
|      1 | 5155 | `		}` |
|     17 | 5156 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 5157 | `	}else{` |
|      3 | 5158 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 5159 | `	}` |
|     19 | 5160 | `	return PH7_OK;` |
|     11 | 5161 |  |
|      - | 5162 | `/*` |
|      - | 5163 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5164 | ` *  Wraps a string to a given number of characters.` |
|      - | 5165 | ` * Parameters` |
|      - | 5166 | ` *  $str` |
|      - | 5167 | ` *   The input string.` |
|      - | 5168 | ` * $width` |
|      - | 5169 | ` *  The column width.` |
|      - | 5170 | ` * $break` |
|      - | 5171 | ` *  The line is broken using the optional break parameter.` |
|      - | 5172 | ` * Return` |
|      - | 5173 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5174 | ` */` |
|     14 | 5175 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5176 |  |
|      - | 5177 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 5178 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 5179 | `	if( nArg < 1 ){` |
|      - | 5180 | `		/* Missing arguments,return the empty string */` |
|      3 | 5181 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5182 | `		return PH7_OK;` |
|      - | 5183 | `	}` |
|      - | 5184 | `	/* Extract the input string */` |
|     13 | 5185 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 5186 | `	if( iLen < 1 ){` |
|      - | 5187 | `		/* Nothing to process,return the empty string */` |
|      3 | 5188 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5189 | `		return PH7_OK;` |
|      - | 5190 | `	}` |
|      - | 5191 | `	/* Chunk length */` |
|     11 | 5192 | `	iChunk = 75;` |
|     11 | 5193 | `	iBreaklen = 0;` |
|     11 | 5194 | `	zBreak = ""; /* cc warning */` |
|     11 | 5195 | `	if( nArg > 1 ){` |
|     11 | 5196 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 5197 | `		if( iChunk < 1 ){` |
|    ! 0 | 5198 | `			iChunk = 75;` |
|    ! 0 | 5199 | `		}` |
|     11 | 5200 | `		if( nArg > 2 ){` |
|      3 | 5201 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 5202 | `		}` |
|      5 | 5203 | `	}` |
|     11 | 5204 | `	if( iBreaklen < 1 ){` |
|      - | 5205 | `		/* Set a default column break */` |
|      - | 5206 | `#ifdef __WINNT__` |
|      1 | 5207 | `		zBreak = "\r\n";` |
|      1 | 5208 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 5209 | `#else` |
|      8 | 5210 | `		zBreak = "\n";` |
|      8 | 5211 | `		iBreaklen = (int)sizeof(char);` |
|      - | 5212 | `#endif` |
|      4 | 5213 | `	}` |
|      - | 5214 | `	/* Perform the requested operation */` |
|     11 | 5215 | `	zEnd = &zIn[iLen];` |
|     41 | 5216 | `	for(;;){` |
|      - | 5217 | `		int nMax;` |
|     47 | 5218 | `		if( zIn >= zEnd ){` |
|      - | 5219 | `			/* No more input to process */` |
|     11 | 5220 | `			break;` |
|      - | 5221 | `		}` |
|     37 | 5222 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 5223 | `		if( iChunk > nMax ){` |
|     11 | 5224 | `			iChunk = nMax;` |
|      5 | 5225 | `		}` |
|      - | 5226 | `		/* Append the column first */` |
|     37 | 5227 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 5228 | `		/* Advance the cursor */` |
|     37 | 5229 | `		zIn += iChunk;` |
|     37 | 5230 | `		if( zIn < zEnd ){` |
|      - | 5231 | `			/* Append the line break */` |
|     27 | 5232 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 5233 | `		}` |
|      1 | 5234 | `	}` |
|     11 | 5235 | `	return PH7_OK;` |
|      8 | 5236 |  |
|      - | 5237 | `/*` |
|      - | 5238 | ` * Check if the given character is a member of the given mask.` |
|      - | 5239 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5240 | ` * Refer to [strtok()].` |
|      - | 5241 | ` */` |
|     30 | 5242 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5243 |  |
|      - | 5244 | `	int i;` |
|     57 | 5245 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 5246 | `		if( c == zMask[i] ){` |
|     13 | 5247 | `			if( pOfft ){` |
|      5 | 5248 | `				*pOfft = i;` |
|      2 | 5249 | `			}` |
|     13 | 5250 | `			return TRUE;` |
|      - | 5251 | `		}` |
|     14 | 5252 | `	}` |
|     19 | 5253 | `	return FALSE;` |
|     16 | 5254 |  |
|      - | 5255 | `/*` |
|      - | 5256 | ` * Extract a single token from the input stream.` |
|      - | 5257 | ` * Refer to [strtok()].` |
|      - | 5258 | ` */` |
|      6 | 5259 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5260 |  |
|      7 | 5261 | `	const char *zIn = *pzIn;` |
|      - | 5262 | `	const char *zPtr;` |
|      - | 5263 | `	/* Ignore leading delimiter */` |
|     11 | 5264 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5265 | `		zIn++;` |
|      1 | 5266 | `	}` |
|      7 | 5267 | `	if( zIn >= zEnd ){` |
|      - | 5268 | `		/* End of input */` |
|    ! 0 | 5269 | `		return SXERR_EOF;` |
|      - | 5270 | `	}` |
|      7 | 5271 | `	zPtr = zIn;` |
|      - | 5272 | `	/* Extract the token */` |
|     13 | 5273 | `	while( zIn < zEnd ){` |
|     11 | 5274 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5275 | `			/* UTF-8 stream */` |
|    ! 0 | 5276 | `			zIn++;` |
|    ! 0 | 5277 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5278 | `		}else{` |
|     11 | 5279 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5280 | `				break;` |
|      - | 5281 | `			}` |
|      7 | 5282 | `			zIn++;` |
|      - | 5283 | `		}` |
|      1 | 5284 | `	}` |
|      7 | 5285 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5286 | `	/* Update the cursor */` |
|      7 | 5287 | `	*pzIn = zIn;` |
|      - | 5288 | `	/* Return to the caller */` |
|      7 | 5289 | `	return SXRET_OK;` |
|      4 | 5290 |  |
|      - | 5291 | `/* strtok auxiliary private data */` |
|      - | 5292 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5293 | `struct strtok_aux_data` |
|      - | 5294 |  |
|      - | 5295 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5296 | `	const char *zIn;   /* Current input stream */` |
|      - | 5297 | `	const char *zEnd;  /* End of input */` |
|      - | 5298 | `};` |
|      - | 5299 | `/*` |
|      - | 5300 | ` * string strtok(string $str,string $token)` |
|      - | 5301 | ` * string strtok(string $token)` |
|      - | 5302 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5303 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5304 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5305 | ` *  words by using the space character as the token.` |
|      - | 5306 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5307 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5308 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5309 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5310 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5311 | ` *  the argument are found.` |
|      - | 5312 | ` * Parameters` |
|      - | 5313 | ` *  $str` |
|      - | 5314 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5315 | ` * $token` |
|      - | 5316 | ` *  The delimiter used when splitting up str.` |
|      - | 5317 | ` * Return` |
|      - | 5318 | ` *   Current token or FALSE on EOF.` |
|      - | 5319 | ` */` |
|      8 | 5320 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5321 |  |
|      - | 5322 | `	strtok_aux_data *pAux;` |
|      - | 5323 | `	const char *zMask;` |
|      - | 5324 | `	SyString sToken;` |
|      - | 5325 | `	int nMasklen;` |
|      - | 5326 | `	sxi32 rc;` |
|      9 | 5327 | `	if( nArg < 2 ){` |
|      - | 5328 | `		/* Extract top aux data */` |
|      7 | 5329 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5330 | `		if( pAux == 0 ){` |
|      - | 5331 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5332 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5333 | `			return PH7_OK;` |
|      - | 5334 | `		}` |
|      7 | 5335 | `		nMasklen = 0;` |
|      7 | 5336 | `		zMask = ""; /* cc warning */` |
|      7 | 5337 | `		if( nArg > 0 ){` |
|      - | 5338 | `			/* Extract the mask */` |
|      5 | 5339 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5340 | `		}` |
|      7 | 5341 | `		if( nMasklen < 1 ){` |
|      - | 5342 | `			/* Invalid mask,return FALSE */` |
|      3 | 5343 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5344 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5345 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5346 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5347 | `			return PH7_OK;` |
|      - | 5348 | `		}` |
|      - | 5349 | `		/* Extract the token */` |
|      5 | 5350 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5351 | `		if( rc != SXRET_OK ){` |
|      - | 5352 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5353 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5354 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5355 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5356 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5357 | `		}else{` |
|      - | 5358 | `			/* Return the extracted token */` |
|      5 | 5359 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5360 | `		}` |
|      3 | 5361 | `	}else{` |
|      - | 5362 | `		const char *zInput,*zCur;` |
|      - | 5363 | `		char *zDup;` |
|      - | 5364 | `		int nLen;` |
|      - | 5365 | `		/* Extract the raw input */` |
|      3 | 5366 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5367 | `		if( nLen < 1 ){` |
|      - | 5368 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5369 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5370 | `			return PH7_OK;` |
|      - | 5371 | `		}` |
|      - | 5372 | `		/* Extract the mask */` |
|      3 | 5373 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5374 | `		if( nMasklen < 1 ){` |
|      - | 5375 | `			/* Set a default mask */` |
|      - | 5376 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5377 | `			zMask = TOK_MASK;` |
|    ! 0 | 5378 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5379 | `#undef TOK_MASK` |
|    ! 0 | 5380 | `		}` |
|      - | 5381 | `		/* Extract a single token */` |
|      3 | 5382 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5383 | `		if( rc != SXRET_OK ){` |
|      - | 5384 | `			/* Empty input */` |
|    ! 0 | 5385 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5386 | `			return PH7_OK;` |
|    ! 0 | 5387 | `		}else{` |
|      - | 5388 | `			/* Return the extracted token */` |
|      3 | 5389 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5390 | `		}` |
|      - | 5391 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5392 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5393 | `		if( pAux ){` |
|      3 | 5394 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5395 | `			if( nLen < 1 ){` |
|    ! 0 | 5396 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5397 | `				return PH7_OK;` |
|      - | 5398 | `			}` |
|      - | 5399 | `			/* Duplicate input */` |
|      3 | 5400 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5401 | `			if( zDup  ){` |
|      3 | 5402 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5403 | `				/* Register the aux data */` |
|      3 | 5404 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5405 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5406 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5407 | `			}` |
|      1 | 5408 | `		}` |
|      - | 5409 | `	}` |
|      7 | 5410 | `	return PH7_OK;` |
|      5 | 5411 |  |
|      - | 5412 | `/*` |
|      - | 5413 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5414 | ` *  Pad a string to a certain length with another string` |
|      - | 5415 | ` * Parameters` |
|      - | 5416 | ` *  $input` |
|      - | 5417 | ` *   The input string.` |
|      - | 5418 | ` * $pad_length` |
|      - | 5419 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5420 | ` *   string, no padding takes place.` |
|      - | 5421 | ` * $pad_string` |
|      - | 5422 | ` *   Note:` |
|      - | 5423 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5424 | ` *    divided by the pad_string's length.` |
|      - | 5425 | ` * $pad_type` |
|      - | 5426 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5427 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5428 | ` * Return` |
|      - | 5429 | ` *  The padded string.` |
|      - | 5430 | ` */` |
|     10 | 5431 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5432 |  |
|      - | 5433 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5434 | `	const char *zIn,*zPad;` |
|     11 | 5435 | `	if( nArg < 2 ){` |
|      - | 5436 | `		/* Missing arguments,return the empty string */` |
|      5 | 5437 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5438 | `		return PH7_OK;` |
|      - | 5439 | `	}` |
|      - | 5440 | `	/* Extract the target string */` |
|      7 | 5441 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5442 | `	/* Padding length */` |
|      7 | 5443 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5444 | `	if( iPadlen > 0 ){` |
|      5 | 5445 | `		iPadlen -= iLen;` |
|      2 | 5446 | `	}` |
|      7 | 5447 | `	if( iPadlen < 1  ){` |
|      - | 5448 | `		/* Return the string verbatim */` |
|      3 | 5449 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5450 | `		return PH7_OK;` |
|      - | 5451 | `	}` |
|      5 | 5452 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5453 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5454 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5455 | `	if( nArg > 2 ){` |
|      - | 5456 | `		/* Padding string */` |
|      5 | 5457 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5458 | `		if( iStrpad < 1 ){` |
|      - | 5459 | `			/* Empty string */` |
|    ! 0 | 5460 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5461 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5462 | `		}` |
|      5 | 5463 | `		if( nArg > 3 ){` |
|      - | 5464 | `			/* Padd type */` |
|      5 | 5465 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5466 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5467 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5468 | `			}` |
|      2 | 5469 | `		}` |
|      2 | 5470 | `	}` |
|      5 | 5471 | `	iDiv = 1;` |
|      5 | 5472 | `	if( iType == 2 ){` |
|    ! 0 | 5473 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5474 | `	}` |
|      - | 5475 | `	/* Perform the requested operation */` |
|      5 | 5476 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5477 | `		jPad = iStrpad;` |
|      5 | 5478 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5479 | `			/* Padding */` |
|      5 | 5480 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5481 | `				break;` |
|      - | 5482 | `			}` |
|      3 | 5483 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5484 | `		}` |
|      3 | 5485 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5486 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5487 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5488 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5489 | `					jPad = iStrpad;` |
|    ! 0 | 5490 | `				}` |
|      3 | 5491 | `				if( jPad < 1){` |
|    ! 0 | 5492 | `					break;` |
|      - | 5493 | `				}` |
|      3 | 5494 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5495 | `			}` |
|      1 | 5496 | `		}` |
|      1 | 5497 | `	}` |
|      5 | 5498 | `	if( iLen > 0 ){` |
|      - | 5499 | `		/* Append the input string */` |
|      5 | 5500 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5501 | `	}` |
|      5 | 5502 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5503 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5504 | `			/* Padding */` |
|      5 | 5505 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5506 | `				break;` |
|      - | 5507 | `			}` |
|      3 | 5508 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5509 | `		}` |
|      5 | 5510 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5511 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5512 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5513 | `				jPad = iStrpad;` |
|    ! 0 | 5514 | `			}` |
|      3 | 5515 | `			if( jPad < 1){` |
|    ! 0 | 5516 | `				break;` |
|      - | 5517 | `			}` |
|      3 | 5518 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5519 | `		}` |
|      1 | 5520 | `	}` |
|      5 | 5521 | `	return PH7_OK;` |
|      6 | 5522 |  |
|      - | 5523 | `/*` |
|      - | 5524 | ` * String replacement private data.` |
|      - | 5525 | ` */` |
|      - | 5526 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5527 | `struct str_replace_data` |
|      - | 5528 |  |
|      - | 5529 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5530 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5531 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5532 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5533 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5534 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5535 | `};` |
|      - | 5536 | `/*` |
|      - | 5537 | ` * Remove a substring.` |
|      - | 5538 | ` */` |
|      - | 5539 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5540 | `	for(;;){\` |
|      - | 5541 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5542 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5543 | `		++OFFT;\` |
|      - | 5544 | `	}\` |
|      - | 5545 |  |
|      - | 5546 | `/*` |
|      - | 5547 | ` * Shift right and insert algorithm.` |
|      - | 5548 | ` */` |
|      - | 5549 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5550 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5551 | `		for(;;){\` |
|      - | 5552 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5553 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5554 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5555 | `			--INLEN; \` |
|      - | 5556 | `		}\` |
|      - | 5557 | `		for(;;){\` |
|      - | 5558 | `				if(ELEN < 1) { break; }\` |
|      - | 5559 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5560 | `				OFFT++;\` |
|      - | 5561 | `				ENTRY++;\` |
|      - | 5562 | `				--ELEN;\` |
|      - | 5563 | `		}\` |
|      - | 5564 |  |
|      - | 5565 | `/*` |
|      - | 5566 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5567 | ` * replacement string [i.e: zReplace].` |
|      - | 5568 | ` */` |
|     38 | 5569 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 5570 |  |
|     39 | 5571 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5572 | `	sxu32 n,m;` |
|     39 | 5573 | `	n = SyBlobLength(pWorker);` |
|     39 | 5574 | `	m = nOfft;` |
|      - | 5575 | `	/* Delete the old entry */` |
|    475 | 5576 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 5577 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 5578 | `	if( nReplen > 0 ){` |
|     33 | 5579 | `		sxi32 iRep = nReplen;` |
|      - | 5580 | `		sxi32 rc;` |
|      - | 5581 | `		/*` |
|      - | 5582 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 5583 | `		 * string.` |
|      - | 5584 | `		 */` |
|     33 | 5585 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 5586 | `		if( rc != SXRET_OK ){` |
|      - | 5587 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 5588 | `			return SXRET_OK;` |
|      - | 5589 | `		}` |
|      - | 5590 | `		/* Perform the insertion now */` |
|     33 | 5591 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 5592 | `		n = SyBlobLength(pWorker);` |
|    163 | 5593 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 5594 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 5595 | `	}` |
|     39 | 5596 | `	return SXRET_OK;` |
|     20 | 5597 |  |
|      - | 5598 | `/*` |
|      - | 5599 | ` * String replacement walker callback.` |
|      - | 5600 | ` * The following callback is invoked for each array entry that hold` |
|      - | 5601 | ` * the replace string.` |
|      - | 5602 | ` * Refer to the strtr() implementation for more information.` |
|      - | 5603 | ` */` |
|      8 | 5604 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5605 |  |
|      9 | 5606 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 5607 | `	const char *zTarget,*zReplace;` |
|      - | 5608 | `	SyBlob *pWorker;` |
|      - | 5609 | `	int tLen,nLen;` |
|      - | 5610 | `	sxu32 nOfft;` |
|      - | 5611 | `	sxi32 rc;` |
|      - | 5612 | `	/* Point to the working buffer */` |
|      9 | 5613 | `	pWorker = pRepData->pWorker;` |
|      9 | 5614 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 5615 | `		/* Target and replace must be a string */` |
|      3 | 5616 | `		return PH7_OK;` |
|      - | 5617 | `	}` |
|      - | 5618 | `	/* Extract the target and the replace */` |
|      7 | 5619 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 5620 | `	if( tLen < 1 ){` |
|      - | 5621 | `		/* Empty target,return immediately */` |
|    ! 0 | 5622 | `		return PH7_OK;` |
|      - | 5623 | `	}` |
|      - | 5624 | `	/* Perform a pattern search */` |
|      7 | 5625 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 5626 | `	if( rc != SXRET_OK ){` |
|      - | 5627 | `		/* Pattern not found */` |
|    ! 0 | 5628 | `		return PH7_OK;` |
|      - | 5629 | `	}` |
|      - | 5630 | `	/* Extract the replace string */` |
|      7 | 5631 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 5632 | `	/* Perform the replace process */` |
|      7 | 5633 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 5634 | `	/* All done */` |
|      7 | 5635 | `	return PH7_OK;` |
|      5 | 5636 |  |
|      - | 5637 | `/*` |
|      - | 5638 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 5639 | ` * to collect search/replace string.` |
|      - | 5640 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 5641 | ` */` |
|     26 | 5642 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5643 |  |
|     27 | 5644 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 5645 | `	SyString sWorker;` |
|      - | 5646 | `	const char *zIn;` |
|      - | 5647 | `	int nByte;` |
|      - | 5648 | `	/* Extract a string representation of the given argument */` |
|     27 | 5649 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 5650 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 5651 | `	if( nByte > 0 ){` |
|      - | 5652 | `		char *zDup;` |
|      - | 5653 | `		/* Duplicate the chunk */` |
|     25 | 5654 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 5655 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 5656 | `			);` |
|     25 | 5657 | `		if( zDup == 0 ){` |
|      - | 5658 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 5659 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5660 | `			return PH7_OK;` |
|      - | 5661 | `		}` |
|     25 | 5662 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 5663 | `		/* Save the chunk */` |
|     25 | 5664 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 5665 | `	}` |
|      - | 5666 | `	/* Save for later processing */` |
|     27 | 5667 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 5668 | `	/* All done */` |
|     13 | 5669 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 5670 | `	return PH7_OK;` |
|     14 | 5671 |  |
|      - | 5672 | `/*` |
|      - | 5673 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5674 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5675 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 5676 | ` * Parameters` |
|      - | 5677 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 5678 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 5679 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 5680 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 5681 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 5682 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 5683 | ` * $search` |
|      - | 5684 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 5685 | ` *  to designate multiple needles.` |
|      - | 5686 | ` * $replace` |
|      - | 5687 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 5688 | ` *  to designate multiple replacements.` |
|      - | 5689 | ` * $subject` |
|      - | 5690 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 5691 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 5692 | ` *  of subject, and the return value is an array as well.` |
|      - | 5693 | ` * $count (Not used)` |
|      - | 5694 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 5695 | ` * Return` |
|      - | 5696 | ` * This function returns a string or an array with the replaced values.` |
|      - | 5697 | ` */` |
|  21362 | 5698 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5699 |  |
|      - | 5700 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 5701 | `	ProcStringMatch xMatch;` |
|      - | 5702 | `	const char *zIn,*zFunc;` |
|      - | 5703 | `	str_replace_data sRep;` |
|      - | 5704 | `	SyBlob sWorker;` |
|      - | 5705 | `	SySet sReplace;` |
|      - | 5706 | `	SySet sSearch;` |
|      - | 5707 | `	int rep_str;` |
|      - | 5708 | `	int nByte;` |
|      - | 5709 | `	sxi32 rc;` |
|  21364 | 5710 | `	if( nArg < 3 ){` |
|      - | 5711 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 5712 | `		ph7_result_null(pCtx);` |
|      7 | 5713 | `		return PH7_OK;` |
|      - | 5714 | `	}` |
|      - | 5715 | `	/* Initialize fields */` |
|  21358 | 5716 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  21358 | 5717 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  21358 | 5718 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  21358 | 5719 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  21358 | 5720 | `	sRep.pCtx = pCtx;` |
|  21358 | 5721 | `	sRep.pCollector = &sSearch;` |
|  21358 | 5722 | `	rep_str = 0;` |
|      - | 5723 | `	/* Extract the subject */` |
|  21358 | 5724 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  21358 | 5725 | `	if( nByte < 1 ){` |
|      - | 5726 | `		/* Nothing to replace,return the empty string */` |
|     29 | 5727 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 5728 | `		return PH7_OK;` |
|      - | 5729 | `	}` |
|      - | 5730 | `	/* Copy the subject */` |
|  21330 | 5731 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 5732 | `	/* Search string */` |
|  21330 | 5733 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 5734 | `		/* Collect search string */` |
|      9 | 5735 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 5736 | `	}else{` |
|      - | 5737 | `		/* Single pattern */` |
|  21322 | 5738 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  21322 | 5739 | `		if( nByte < 1 ){` |
|      - | 5740 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 5741 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 5742 | `			return PH7_OK;` |
|      - | 5743 | `		}` |
|  21318 | 5744 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5745 | `		/* Save for later processing */` |
|  21318 | 5746 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 5747 | `	}` |
|      - | 5748 | `	/* Replace string */` |
|  21326 | 5749 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 5750 | `		/* Collect replace string */` |
|      7 | 5751 | `		sRep.pCollector = &sReplace;` |
|      7 | 5752 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 5753 | `	}else{` |
|      - | 5754 | `		/* Single needle */` |
|  21320 | 5755 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  21320 | 5756 | `		rep_str = 1;` |
|  21320 | 5757 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5758 | `		/* Save for later processing */` |
|  21320 | 5759 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 5760 | `	}` |
|      - | 5761 | `	/* Reset loop cursors */` |
|  21326 | 5762 | `	SySetResetCursor(&sSearch);` |
|  21326 | 5763 | `	SySetResetCursor(&sReplace);` |
|  21326 | 5764 | `	pReplace = pSearch = 0; /* cc warning */` |
|  21326 | 5765 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 5766 | `	/* Extract function name */` |
|  21326 | 5767 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 5768 | `	/* Set the default pattern match routine */` |
|  21326 | 5769 | `	xMatch = SyBlobSearch;` |
|  21326 | 5770 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5771 | `		/* Case insensitive pattern match */` |
|     11 | 5772 | `		xMatch = iPatternMatch;` |
|      5 | 5773 | `	}` |
|      - | 5774 | `	/* Start the replace process */` |
|  42658 | 5775 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 5776 | `		sxu32 nCount,nOfft;` |
|  21334 | 5777 | `		if( pSearch->nByte <  1 ){` |
|      - | 5778 | `			/* Empty string,ignore */` |
|      3 | 5779 | `			continue;` |
|      - | 5780 | `		}` |
|      - | 5781 | `		/* Extract the replace string */` |
|  21332 | 5782 | `		if( rep_str ){` |
|  21322 | 5783 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  10662 | 5784 | `		}else{` |
|     11 | 5785 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 5786 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 5787 | `				 * An empty string is used for the rest of replacement values` |
|      - | 5788 | `				 */` |
|      3 | 5789 | `				pReplace = 0;` |
|      1 | 5790 | `			}` |
|      - | 5791 | `		}` |
|  21332 | 5792 | `		if( pReplace == 0 ){` |
|      - | 5793 | `			/* Use an empty string instead */` |
|      3 | 5794 | `			pReplace = &sTemp;` |
|      1 | 5795 | `		}` |
|  21332 | 5796 | `		nOfft = nCount = 0;` |
|  10681 | 5797 | `		for(;;){` |
|  21364 | 5798 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 5799 | `				break;` |
|      - | 5800 | `			}` |
|      - | 5801 | `			/* Perform a pattern lookup */` |
|  32027 | 5802 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  21350 | 5803 | `				pSearch->nByte,&nOfft);` |
|  21352 | 5804 | `			if( rc != SXRET_OK ){` |
|      - | 5805 | `				/* Pattern not found */` |
|  21320 | 5806 | `				break;` |
|      - | 5807 | `			}` |
|      - | 5808 | `			/* Perform the replace operation */` |
|     33 | 5809 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 5810 | `			/* Increment offset counter */` |
|     33 | 5811 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 5812 | `		}` |
|      2 | 5813 | `	}` |
|      - | 5814 | `	/* All done,clean-up the mess left behind */` |
|  21326 | 5815 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  21326 | 5816 | `	SySetRelease(&sSearch);` |
|  21326 | 5817 | `	SySetRelease(&sReplace);` |
|  21326 | 5818 | `	SyBlobRelease(&sWorker);` |
|  21326 | 5819 | `	return PH7_OK;` |
|  10683 | 5820 |  |
|      - | 5821 | `/*` |
|      - | 5822 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5823 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5824 | ` *  Translate characters or replace substrings.` |
|      - | 5825 | ` * Parameters` |
|      - | 5826 | ` *  $str` |
|      - | 5827 | ` *  The string being translated.` |
|      - | 5828 | ` * $from` |
|      - | 5829 | ` *  The string being translated to to.` |
|      - | 5830 | ` * $to` |
|      - | 5831 | ` *  The string replacing from.` |
|      - | 5832 | ` * $replace_pairs` |
|      - | 5833 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5834 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5835 | ` * Return` |
|      - | 5836 | ` *  The translated string.` |
|      - | 5837 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5838 | ` */` |
|     12 | 5839 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5840 |  |
|      - | 5841 | `	const char *zIn;` |
|      - | 5842 | `	int nLen;` |
|     13 | 5843 | `	if( nArg < 1 ){` |
|      - | 5844 | `		/* Nothing to replace,return FALSE */` |
|      7 | 5845 | `		ph7_result_bool(pCtx,0);` |
|      7 | 5846 | `		return PH7_OK;` |
|      - | 5847 | `	}` |
|      7 | 5848 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 5849 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5850 | `		/* Invalid arguments */` |
|    ! 0 | 5851 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5852 | `		return PH7_OK;` |
|      - | 5853 | `	}` |
|      9 | 5854 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5855 | `		str_replace_data sRepData;` |
|      - | 5856 | `		SyBlob sWorker;` |
|      - | 5857 | `		/* Initilaize the working buffer */` |
|      5 | 5858 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 5859 | `		/* Copy raw string */` |
|      5 | 5860 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 5861 | `		/* Init our replace data instance */` |
|      5 | 5862 | `		sRepData.pWorker = &sWorker;` |
|      5 | 5863 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 5864 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 5865 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 5866 | `		/* All done, return the result string */` |
|      7 | 5867 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 5868 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5869 | `		/* Clean-up */` |
|      5 | 5870 | `		SyBlobRelease(&sWorker);` |
|      3 | 5871 | `	}else{` |
|      - | 5872 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5873 | `		const char *zFrom,*zTo;` |
|      3 | 5874 | `		if( nArg < 3 ){` |
|      - | 5875 | `			/* Nothing to replace */` |
|    ! 0 | 5876 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5877 | `			return PH7_OK;` |
|      - | 5878 | `		}` |
|      - | 5879 | `		/* Extract given arguments */` |
|      3 | 5880 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 5881 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 5882 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5883 | `			/* Nothing to replace */` |
|    ! 0 | 5884 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5885 | `			return PH7_OK;` |
|      - | 5886 | `		}` |
|      - | 5887 | `		/* Start the replace process */` |
|     13 | 5888 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 5889 | `			c = zIn[i];` |
|     11 | 5890 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 5891 | `				if ( iOfft < tlen ){` |
|      5 | 5892 | `					c = zTo[iOfft];` |
|      2 | 5893 | `				}` |
|      2 | 5894 | `			}` |
|     11 | 5895 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5896 |  |
|      6 | 5897 | `		}` |
|      - | 5898 | `	}` |
|      7 | 5899 | `	return PH7_OK;` |
|      7 | 5900 |  |
|      - | 5901 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5902 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5903 | `/*` |
|      - | 5904 | ` * Parse an INI string.` |
|      - | 5905 |  |
|      - | 5906 | ` * According to wikipedia` |
|      - | 5907 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 5908 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 5909 | ` *  Format` |
|      - | 5910 | `*    Properties` |
|      - | 5911 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 5912 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 5913 | `*     Example:` |
|      - | 5914 | `*      name=value` |
|      - | 5915 | `*    Sections` |
|      - | 5916 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 5917 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 5918 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 5919 | `*     or the end of the file. Sections may not be nested.` |
|      - | 5920 | `*     Example:` |
|      - | 5921 | `*      [section]` |
|      - | 5922 | `*   Comments` |
|      - | 5923 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 5924 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 5925 | `*/` |
|     12 | 5926 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 5927 |  |
|      - | 5928 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 5929 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 5930 | `	SyHashEntry *pEntry;` |
|      - | 5931 | `	SyString sEntry;` |
|      - | 5932 | `	SyHash sHash;` |
|      - | 5933 | `	int c;` |
|      - | 5934 | `	/* Create an empty array and worker variables */` |
|     13 | 5935 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5936 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 5937 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5938 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 5939 | `		/* Out of memory */` |
|    ! 0 | 5940 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 5941 | `		/* Return FALSE */` |
|    ! 0 | 5942 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5943 | `		return PH7_OK;` |
|      - | 5944 | `	}` |
|     13 | 5945 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 5946 | `	pCur = pArray;` |
|      - | 5947 | `	/* Start the parse process */` |
|     21 | 5948 | `	for(;;){` |
|      - | 5949 | `		/* Ignore leading white spaces */` |
|     69 | 5950 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 5951 | `			zIn++;` |
|      1 | 5952 | `		}` |
|     43 | 5953 | `		if( zIn >= zEnd ){` |
|      - | 5954 | `			/* No more input to process */` |
|     13 | 5955 | `			break;` |
|      - | 5956 | `		}` |
|     31 | 5957 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 5958 | `			/* Comment til the end of line */` |
|    ! 0 | 5959 | `			zIn++;` |
|    ! 0 | 5960 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 5961 | `				zIn++;` |
|    ! 0 | 5962 | `			}` |
|    ! 0 | 5963 | `			continue;` |
|      - | 5964 | `		}` |
|      - | 5965 | `		/* Reset the string cursor of the working variable */` |
|     31 | 5966 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 5967 | `		if( zIn[0] == '[' ){` |
|      - | 5968 | `			/* Section: Extract the section name */` |
|      9 | 5969 | `			zIn++;` |
|      9 | 5970 | `			zCur = zIn;` |
|     73 | 5971 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 5972 | `				zIn++;` |
|      1 | 5973 | `			}` |
|      9 | 5974 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 5975 | `				/* Save the section name */` |
|      5 | 5976 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 5977 | `				SyStringFullTrim(&sEntry);` |
|      5 | 5978 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 5979 | `				if( sEntry.nByte > 0 ){` |
|      - | 5980 | `					/* Associate an array with the section */` |
|      5 | 5981 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 5982 | `					if( pSection ){` |
|      5 | 5983 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 5984 | `						pCur = pSection;` |
|      2 | 5985 | `					}` |
|      2 | 5986 | `				}` |
|      2 | 5987 | `			}` |
|      9 | 5988 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 5989 | `		}else{` |
|      - | 5990 | `			ph7_value *pOldCur;` |
|      - | 5991 | `			int is_array;` |
|      - | 5992 | `			int iLen;` |
|      - | 5993 | `			/* Properties */` |
|     23 | 5994 | `			is_array = 0;` |
|     23 | 5995 | `			zCur = zIn;` |
|     23 | 5996 | `			iLen = 0; /* cc warning */` |
|     23 | 5997 | `			pOldCur = pCur;` |
|    155 | 5998 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 5999 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6000 | `					/* Array */` |
|    ! 0 | 6001 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6002 | `					is_array = 1;` |
|    ! 0 | 6003 | `					if( iLen > 0 ){` |
|    ! 0 | 6004 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6005 | `						/* Query the hashtable */` |
|    ! 0 | 6006 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6007 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6008 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6009 | `						if( pEntry ){` |
|    ! 0 | 6010 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6011 | `						}else{` |
|      - | 6012 | `							/* Create an empty array */` |
|    ! 0 | 6013 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6014 | `							if( pvArr ){` |
|      - | 6015 | `								/* Save the entry */` |
|    ! 0 | 6016 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6017 | `								/* Insert the entry */` |
|    ! 0 | 6018 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6019 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6020 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6021 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6022 | `							}` |
|      - | 6023 | `						}` |
|    ! 0 | 6024 | `						if( pvArr ){` |
|    ! 0 | 6025 | `							pCur = pvArr;` |
|    ! 0 | 6026 | `						}` |
|    ! 0 | 6027 | `					}` |
|    ! 0 | 6028 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6029 | `						zIn++;` |
|    ! 0 | 6030 | `					}` |
|    ! 0 | 6031 | `				}` |
|    133 | 6032 | `				zIn++;` |
|      1 | 6033 | `			}` |
|     23 | 6034 | `			if( !is_array ){` |
|     23 | 6035 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6036 | `			}` |
|      - | 6037 | `			/* Trim the key */` |
|     23 | 6038 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 6039 | `			SyStringFullTrim(&sEntry);` |
|     23 | 6040 | `			if( sEntry.nByte > 0 ){` |
|     23 | 6041 | `				if( !is_array ){` |
|      - | 6042 | `					/* Save the key name */` |
|     23 | 6043 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 6044 | `				}` |
|      - | 6045 | `				/* extract key value */` |
|     23 | 6046 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 6047 | `				zIn++; /* '=' */` |
|     39 | 6048 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 6049 | `					zIn++;` |
|      1 | 6050 | `				}` |
|     23 | 6051 | `				if( zIn < zEnd ){` |
|     21 | 6052 | `					zCur = zIn;` |
|     21 | 6053 | `					c = zIn[0];` |
|     21 | 6054 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6055 | `						zIn++;` |
|      - | 6056 | `						/* Delimit the value */` |
|    ! 0 | 6057 | `						while( zIn < zEnd ){` |
|    ! 0 | 6058 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 6059 | `								break;` |
|      - | 6060 | `							}` |
|    ! 0 | 6061 | `							zIn++;` |
|    ! 0 | 6062 | `						}` |
|    ! 0 | 6063 | `						if( zIn < zEnd ){` |
|    ! 0 | 6064 | `							zIn++;` |
|    ! 0 | 6065 | `						}` |
|    ! 0 | 6066 | `					}else{` |
|    125 | 6067 | `						while( zIn < zEnd ){` |
|    123 | 6068 | `							if( zIn[0] == '\n' ){` |
|     19 | 6069 | `								if( zIn[-1] != '\\' ){` |
|     19 | 6070 | `									break;` |
|    ! 0 | 6071 | `								}` |
|    105 | 6072 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6073 | `								/* Inline comments */` |
|    ! 0 | 6074 | `								break;` |
|      - | 6075 | `							}` |
|    105 | 6076 | `							zIn++;` |
|      1 | 6077 | `						}` |
|      - | 6078 | `					}` |
|      - | 6079 | `					/* Trim the value */` |
|     21 | 6080 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 6081 | `					SyStringFullTrim(&sEntry);` |
|     21 | 6082 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 6083 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 6084 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 6085 | `					}` |
|     21 | 6086 | `					if( sEntry.nByte > 0 ){` |
|     21 | 6087 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 6088 | `					}` |
|      - | 6089 | `					/* Insert the key and it's value */` |
|     21 | 6090 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 6091 | `				}` |
|     12 | 6092 | `			}else{` |
|    ! 0 | 6093 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 6094 | `					zIn++;` |
|    ! 0 | 6095 | `				}` |
|      - | 6096 | `			}` |
|     23 | 6097 | `			pCur = pOldCur;` |
|      - | 6098 | `		}` |
|      1 | 6099 | `	}` |
|     13 | 6100 | `	SyHashRelease(&sHash);` |
|      - | 6101 | `	/* Return the parse of the INI string */` |
|     13 | 6102 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 6103 | `	return SXRET_OK;` |
|      7 | 6104 |  |
|      - | 6105 | `/*` |
|      - | 6106 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 6107 | ` *  Parse a configuration string.` |
|      - | 6108 | ` * Parameters` |
|      - | 6109 | ` *  $ini` |
|      - | 6110 | ` *   The contents of the ini file being parsed.` |
|      - | 6111 | ` *  $process_sections` |
|      - | 6112 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 6113 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 6114 | ` *  $scanner_mode (Not used)` |
|      - | 6115 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 6116 | ` *   then option values will not be parsed.` |
|      - | 6117 | ` * Return` |
|      - | 6118 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 6119 | ` */` |
|     10 | 6120 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6121 |  |
|      - | 6122 | `	const char *zIni;` |
|      - | 6123 | `	int nByte;` |
|     11 | 6124 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6125 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 6126 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6127 | `		return PH7_OK;` |
|      - | 6128 | `	}` |
|      - | 6129 | `	/* Extract the raw INI buffer */` |
|     11 | 6130 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 6131 | `	/* Process the INI buffer*/` |
|     11 | 6132 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 6133 | `	return PH7_OK;` |
|      6 | 6134 |  |
|      - | 6135 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6136 |  |
|      - | 6137 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6138 |  |
|      - | 6139 | `/*` |
|      - | 6140 | ` * Ctype Functions.` |
|      - | 6141 | ` * Status:` |
|      - | 6142 | ` *    Stable.` |
|      - | 6143 | ` */` |
|      - | 6144 | `/*` |
|      - | 6145 | ` * bool ctype_alnum(string $text)` |
|      - | 6146 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 6147 | ` * Parameters` |
|      - | 6148 | ` *  $text` |
|      - | 6149 | ` *   The tested string.` |
|      - | 6150 | ` * Return` |
|      - | 6151 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 6152 | ` */` |
|     16 | 6153 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6154 |  |
|      - | 6155 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6156 | `	int nLen;` |
|     17 | 6157 | `	if( nArg < 1 ){` |
|      - | 6158 | `		/* Missing arguments,return FALSE */` |
|      3 | 6159 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6160 | `		return PH7_OK;` |
|      - | 6161 | `	}` |
|      - | 6162 | `	/* Extract the target string */` |
|     15 | 6163 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6164 | `	zEnd = &zIn[nLen];` |
|     15 | 6165 | `	if( nLen < 1 ){` |
|      - | 6166 | `		/* Empty string,return FALSE */` |
|      3 | 6167 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6168 | `		return PH7_OK;` |
|      - | 6169 | `	}` |
|      - | 6170 | `	/* Perform the requested operation */` |
|     32 | 6171 | `	for(;;){` |
|     65 | 6172 | `		if( zIn >= zEnd ){` |
|      - | 6173 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6174 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6175 | `			return PH7_OK;` |
|      - | 6176 | `		}` |
|     57 | 6177 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 6178 | `			break;` |
|      - | 6179 | `		}` |
|      - | 6180 | `		/* Point to the next character */` |
|     53 | 6181 | `		zIn++;` |
|      1 | 6182 | `	}` |
|      - | 6183 | `	/* The test failed,return FALSE */` |
|      5 | 6184 | `	ph7_result_bool(pCtx,0);` |
|      5 | 6185 | `	return PH7_OK;` |
|      9 | 6186 |  |
|      - | 6187 | `/*` |
|      - | 6188 | ` * bool ctype_alpha(string $text)` |
|      - | 6189 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 6190 | ` * Parameters` |
|      - | 6191 | ` *  $text` |
|      - | 6192 | ` *   The tested string.` |
|      - | 6193 | ` * Return` |
|      - | 6194 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 6195 | ` */` |
|     18 | 6196 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6197 |  |
|      - | 6198 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6199 | `	int nLen;` |
|     19 | 6200 | `	if( nArg < 1 ){` |
|      - | 6201 | `		/* Missing arguments,return FALSE */` |
|      3 | 6202 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6203 | `		return PH7_OK;` |
|      - | 6204 | `	}` |
|      - | 6205 | `	/* Extract the target string */` |
|     17 | 6206 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6207 | `	zEnd = &zIn[nLen];` |
|     17 | 6208 | `	if( nLen < 1 ){` |
|      - | 6209 | `		/* Empty string,return FALSE */` |
|      3 | 6210 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6211 | `		return PH7_OK;` |
|      - | 6212 | `	}` |
|      - | 6213 | `	/* Perform the requested operation */` |
|     42 | 6214 | `	for(;;){` |
|     85 | 6215 | `		if( zIn >= zEnd ){` |
|      - | 6216 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6217 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6218 | `			return PH7_OK;` |
|      - | 6219 | `		}` |
|     77 | 6220 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 6221 | `			break;` |
|      - | 6222 | `		}` |
|      - | 6223 | `		/* Point to the next character */` |
|     71 | 6224 | `		zIn++;` |
|      1 | 6225 | `	}` |
|      - | 6226 | `	/* The test failed,return FALSE */` |
|      7 | 6227 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6228 | `	return PH7_OK;` |
|     10 | 6229 |  |
|      - | 6230 | `/*` |
|      - | 6231 | ` * bool ctype_cntrl(string $text)` |
|      - | 6232 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 6233 | ` * Parameters` |
|      - | 6234 | ` *  $text` |
|      - | 6235 | ` *   The tested string.` |
|      - | 6236 | ` * Return` |
|      - | 6237 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 6238 | ` */` |
|     18 | 6239 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6240 |  |
|      - | 6241 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6242 | `	int nLen;` |
|     19 | 6243 | `	if( nArg < 1 ){` |
|      - | 6244 | `		/* Missing arguments,return FALSE */` |
|      3 | 6245 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6246 | `		return PH7_OK;` |
|      - | 6247 | `	}` |
|      - | 6248 | `	/* Extract the target string */` |
|     17 | 6249 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6250 | `	zEnd = &zIn[nLen];` |
|     17 | 6251 | `	if( nLen < 1 ){` |
|      - | 6252 | `		/* Empty string,return FALSE */` |
|      3 | 6253 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6254 | `		return PH7_OK;` |
|      - | 6255 | `	}` |
|      - | 6256 | `	/* Perform the requested operation */` |
|     14 | 6257 | `	for(;;){` |
|     29 | 6258 | `		if( zIn >= zEnd ){` |
|      - | 6259 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6260 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6261 | `			return PH7_OK;` |
|      - | 6262 | `		}` |
|     21 | 6263 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6264 | `			/* UTF-8 stream  */` |
|    ! 0 | 6265 | `			break;` |
|      - | 6266 | `		}` |
|     21 | 6267 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6268 | `			break;` |
|      - | 6269 | `		}` |
|      - | 6270 | `		/* Point to the next character */` |
|     15 | 6271 | `		zIn++;` |
|      1 | 6272 | `	}` |
|      - | 6273 | `	/* The test failed,return FALSE */` |
|      7 | 6274 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6275 | `	return PH7_OK;` |
|     10 | 6276 |  |
|      - | 6277 | `/*` |
|      - | 6278 | ` * bool ctype_digit(string $text)` |
|      - | 6279 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6280 | ` * Parameters` |
|      - | 6281 | ` *  $text` |
|      - | 6282 | ` *   The tested string.` |
|      - | 6283 | ` * Return` |
|      - | 6284 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6285 | ` */` |
|   1546 | 6286 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6287 |  |
|      - | 6288 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6289 | `	int nLen;` |
|   1548 | 6290 | `	if( nArg < 1 ){` |
|      - | 6291 | `		/* Missing arguments,return FALSE */` |
|      3 | 6292 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6293 | `		return PH7_OK;` |
|      - | 6294 | `	}` |
|      - | 6295 | `	/* Extract the target string */` |
|   1546 | 6296 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1546 | 6297 | `	zEnd = &zIn[nLen];` |
|   1546 | 6298 | `	if( nLen < 1 ){` |
|      - | 6299 | `		/* Empty string,return FALSE */` |
|      3 | 6300 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6301 | `		return PH7_OK;` |
|      - | 6302 | `	}` |
|      - | 6303 | `	/* Perform the requested operation */` |
|   1448 | 6304 | `	for(;;){` |
|   2898 | 6305 | `		if( zIn >= zEnd ){` |
|      - | 6306 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1324 | 6307 | `			ph7_result_bool(pCtx,1);` |
|   1324 | 6308 | `			return PH7_OK;` |
|      - | 6309 | `		}` |
|   1576 | 6310 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6311 | `			/* UTF-8 stream  */` |
|    ! 0 | 6312 | `			break;` |
|      - | 6313 | `		}` |
|   1576 | 6314 | `		if( !SyisDigit(zIn[0]) ){` |
|    222 | 6315 | `			break;` |
|      - | 6316 | `		}` |
|      - | 6317 | `		/* Point to the next character */` |
|   1356 | 6318 | `		zIn++;` |
|      2 | 6319 | `	}` |
|      - | 6320 | `	/* The test failed,return FALSE */` |
|    222 | 6321 | `	ph7_result_bool(pCtx,0);` |
|    222 | 6322 | `	return PH7_OK;` |
|    775 | 6323 |  |
|      - | 6324 | `/*` |
|      - | 6325 | ` * bool ctype_xdigit(string $text)` |
|      - | 6326 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6327 | ` * Parameters` |
|      - | 6328 | ` *  $text` |
|      - | 6329 | ` *   The tested string.` |
|      - | 6330 | ` * Return` |
|      - | 6331 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6332 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6333 | ` */` |
|     20 | 6334 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6335 |  |
|      - | 6336 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6337 | `	int nLen;` |
|     21 | 6338 | `	if( nArg < 1 ){` |
|      - | 6339 | `		/* Missing arguments,return FALSE */` |
|      3 | 6340 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6341 | `		return PH7_OK;` |
|      - | 6342 | `	}` |
|      - | 6343 | `	/* Extract the target string */` |
|     19 | 6344 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6345 | `	zEnd = &zIn[nLen];` |
|     19 | 6346 | `	if( nLen < 1 ){` |
|      - | 6347 | `		/* Empty string,return FALSE */` |
|      3 | 6348 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6349 | `		return PH7_OK;` |
|      - | 6350 | `	}` |
|      - | 6351 | `	/* Perform the requested operation */` |
|     46 | 6352 | `	for(;;){` |
|     93 | 6353 | `		if( zIn >= zEnd ){` |
|      - | 6354 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6355 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6356 | `			return PH7_OK;` |
|      - | 6357 | `		}` |
|     83 | 6358 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6359 | `			/* UTF-8 stream  */` |
|    ! 0 | 6360 | `			break;` |
|      - | 6361 | `		}` |
|     83 | 6362 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6363 | `			break;` |
|      - | 6364 | `		}` |
|      - | 6365 | `		/* Point to the next character */` |
|     77 | 6366 | `		zIn++;` |
|      1 | 6367 | `	}` |
|      - | 6368 | `	/* The test failed,return FALSE */` |
|      7 | 6369 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6370 | `	return PH7_OK;` |
|     11 | 6371 |  |
|      - | 6372 | `/*` |
|      - | 6373 | ` * bool ctype_graph(string $text)` |
|      - | 6374 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6375 | ` * Parameters` |
|      - | 6376 | ` *  $text` |
|      - | 6377 | ` *   The tested string.` |
|      - | 6378 | ` * Return` |
|      - | 6379 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6380 | ` * (no white space), FALSE otherwise.` |
|      - | 6381 | ` */` |
|     18 | 6382 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6383 |  |
|      - | 6384 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6385 | `	int nLen;` |
|     19 | 6386 | `	if( nArg < 1 ){` |
|      - | 6387 | `		/* Missing arguments,return FALSE */` |
|      3 | 6388 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6389 | `		return PH7_OK;` |
|      - | 6390 | `	}` |
|      - | 6391 | `	/* Extract the target string */` |
|     17 | 6392 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6393 | `	zEnd = &zIn[nLen];` |
|     17 | 6394 | `	if( nLen < 1 ){` |
|      - | 6395 | `		/* Empty string,return FALSE */` |
|      3 | 6396 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6397 | `		return PH7_OK;` |
|      - | 6398 | `	}` |
|      - | 6399 | `	/* Perform the requested operation */` |
|     57 | 6400 | `	for(;;){` |
|    115 | 6401 | `		if( zIn >= zEnd ){` |
|      - | 6402 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6403 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6404 | `			return PH7_OK;` |
|      - | 6405 | `		}` |
|    107 | 6406 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6407 | `			/* UTF-8 stream  */` |
|    ! 0 | 6408 | `			break;` |
|      - | 6409 | `		}` |
|    107 | 6410 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6411 | `			break;` |
|      - | 6412 | `		}` |
|      - | 6413 | `		/* Point to the next character */` |
|    101 | 6414 | `		zIn++;` |
|      1 | 6415 | `	}` |
|      - | 6416 | `	/* The test failed,return FALSE */` |
|      7 | 6417 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6418 | `	return PH7_OK;` |
|     10 | 6419 |  |
|      - | 6420 | `/*` |
|      - | 6421 | ` * bool ctype_print(string $text)` |
|      - | 6422 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6423 | ` * Parameters` |
|      - | 6424 | ` *  $text` |
|      - | 6425 | ` *   The tested string.` |
|      - | 6426 | ` * Return` |
|      - | 6427 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6428 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6429 | ` *  or control function at all.` |
|      - | 6430 | ` */` |
|     18 | 6431 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6432 |  |
|      - | 6433 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6434 | `	int nLen;` |
|     19 | 6435 | `	if( nArg < 1 ){` |
|      - | 6436 | `		/* Missing arguments,return FALSE */` |
|      3 | 6437 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6438 | `		return PH7_OK;` |
|      - | 6439 | `	}` |
|      - | 6440 | `	/* Extract the target string */` |
|     17 | 6441 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6442 | `	zEnd = &zIn[nLen];` |
|     17 | 6443 | `	if( nLen < 1 ){` |
|      - | 6444 | `		/* Empty string,return FALSE */` |
|      3 | 6445 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6446 | `		return PH7_OK;` |
|      - | 6447 | `	}` |
|      - | 6448 | `	/* Perform the requested operation */` |
|     63 | 6449 | `	for(;;){` |
|    127 | 6450 | `		if( zIn >= zEnd ){` |
|      - | 6451 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6452 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6453 | `			return PH7_OK;` |
|      - | 6454 | `		}` |
|    119 | 6455 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6456 | `			/* UTF-8 stream  */` |
|    ! 0 | 6457 | `			break;` |
|      - | 6458 | `		}` |
|    119 | 6459 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6460 | `			break;` |
|      - | 6461 | `		}` |
|      - | 6462 | `		/* Point to the next character */` |
|    113 | 6463 | `		zIn++;` |
|      1 | 6464 | `	}` |
|      - | 6465 | `	/* The test failed,return FALSE */` |
|      7 | 6466 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6467 | `	return PH7_OK;` |
|     10 | 6468 |  |
|      - | 6469 | `/*` |
|      - | 6470 | ` * bool ctype_punct(string $text)` |
|      - | 6471 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6472 | ` * Parameters` |
|      - | 6473 | ` *  $text` |
|      - | 6474 | ` *   The tested string.` |
|      - | 6475 | ` * Return` |
|      - | 6476 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6477 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6478 | ` */` |
|     20 | 6479 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6480 |  |
|      - | 6481 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6482 | `	int nLen;` |
|     21 | 6483 | `	if( nArg < 1 ){` |
|      - | 6484 | `		/* Missing arguments,return FALSE */` |
|      3 | 6485 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6486 | `		return PH7_OK;` |
|      - | 6487 | `	}` |
|      - | 6488 | `	/* Extract the target string */` |
|     19 | 6489 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6490 | `	zEnd = &zIn[nLen];` |
|     19 | 6491 | `	if( nLen < 1 ){` |
|      - | 6492 | `		/* Empty string,return FALSE */` |
|      3 | 6493 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6494 | `		return PH7_OK;` |
|      - | 6495 | `	}` |
|      - | 6496 | `	/* Perform the requested operation */` |
|     38 | 6497 | `	for(;;){` |
|     77 | 6498 | `		if( zIn >= zEnd ){` |
|      - | 6499 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6500 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6501 | `			return PH7_OK;` |
|      - | 6502 | `		}` |
|     69 | 6503 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6504 | `			/* UTF-8 stream  */` |
|    ! 0 | 6505 | `			break;` |
|      - | 6506 | `		}` |
|     69 | 6507 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6508 | `			break;` |
|      - | 6509 | `		}` |
|      - | 6510 | `		/* Point to the next character */` |
|     61 | 6511 | `		zIn++;` |
|      1 | 6512 | `	}` |
|      - | 6513 | `	/* The test failed,return FALSE */` |
|      9 | 6514 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6515 | `	return PH7_OK;` |
|     11 | 6516 |  |
|      - | 6517 | `/*` |
|      - | 6518 | ` * bool ctype_space(string $text)` |
|      - | 6519 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6520 | ` * Parameters` |
|      - | 6521 | ` *  $text` |
|      - | 6522 | ` *   The tested string.` |
|      - | 6523 | ` * Return` |
|      - | 6524 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6525 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6526 | ` *  and form feed characters.` |
|      - | 6527 | ` */` |
|  59184 | 6528 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6529 |  |
|      - | 6530 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6531 | `	int nLen;` |
|  59186 | 6532 | `	if( nArg < 1 ){` |
|      - | 6533 | `		/* Missing arguments,return FALSE */` |
|      3 | 6534 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6535 | `		return PH7_OK;` |
|      - | 6536 | `	}` |
|      - | 6537 | `	/* Extract the target string */` |
|  59184 | 6538 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  59184 | 6539 | `	zEnd = &zIn[nLen];` |
|  59184 | 6540 | `	if( nLen < 1 ){` |
|      - | 6541 | `		/* Empty string,return FALSE */` |
|      3 | 6542 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6543 | `		return PH7_OK;` |
|      - | 6544 | `	}` |
|      - | 6545 | `	/* Perform the requested operation */` |
|  30615 | 6546 | `	for(;;){` |
|  61188 | 6547 | `		if( zIn >= zEnd ){` |
|      - | 6548 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1984 | 6549 | `			ph7_result_bool(pCtx,1);` |
|   1984 | 6550 | `			return PH7_OK;` |
|      - | 6551 | `		}` |
|  59206 | 6552 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6553 | `			/* UTF-8 stream  */` |
|    ! 0 | 6554 | `			break;` |
|      - | 6555 | `		}` |
|  59206 | 6556 | `		if( !SyisSpace(zIn[0]) ){` |
|  57200 | 6557 | `			break;` |
|      - | 6558 | `		}` |
|      - | 6559 | `		/* Point to the next character */` |
|   2008 | 6560 | `		zIn++;` |
|      2 | 6561 | `	}` |
|      - | 6562 | `	/* The test failed,return FALSE */` |
|  57200 | 6563 | `	ph7_result_bool(pCtx,0);` |
|  57200 | 6564 | `	return PH7_OK;` |
|  29616 | 6565 |  |
|      - | 6566 | `/*` |
|      - | 6567 | ` * bool ctype_lower(string $text)` |
|      - | 6568 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 6569 | ` * Parameters` |
|      - | 6570 | ` *  $text` |
|      - | 6571 | ` *   The tested string.` |
|      - | 6572 | ` * Return` |
|      - | 6573 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 6574 | ` */` |
|     18 | 6575 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6576 |  |
|      - | 6577 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6578 | `	int nLen;` |
|     19 | 6579 | `	if( nArg < 1 ){` |
|      - | 6580 | `		/* Missing arguments,return FALSE */` |
|      3 | 6581 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6582 | `		return PH7_OK;` |
|      - | 6583 | `	}` |
|      - | 6584 | `	/* Extract the target string */` |
|     17 | 6585 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6586 | `	zEnd = &zIn[nLen];` |
|     17 | 6587 | `	if( nLen < 1 ){` |
|      - | 6588 | `		/* Empty string,return FALSE */` |
|      3 | 6589 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6590 | `		return PH7_OK;` |
|      - | 6591 | `	}` |
|      - | 6592 | `	/* Perform the requested operation */` |
|     27 | 6593 | `	for(;;){` |
|     55 | 6594 | `		if( zIn >= zEnd ){` |
|      - | 6595 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6596 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6597 | `			return PH7_OK;` |
|      - | 6598 | `		}` |
|     51 | 6599 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 6600 | `			break;` |
|      - | 6601 | `		}` |
|      - | 6602 | `		/* Point to the next character */` |
|     41 | 6603 | `		zIn++;` |
|      1 | 6604 | `	}` |
|      - | 6605 | `	/* The test failed,return FALSE */` |
|     11 | 6606 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6607 | `	return PH7_OK;` |
|     10 | 6608 |  |
|      - | 6609 | `/*` |
|      - | 6610 | ` * bool ctype_upper(string $text)` |
|      - | 6611 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 6612 | ` * Parameters` |
|      - | 6613 | ` *  $text` |
|      - | 6614 | ` *   The tested string.` |
|      - | 6615 | ` * Return` |
|      - | 6616 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 6617 | ` */` |
|     18 | 6618 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6619 |  |
|      - | 6620 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6621 | `	int nLen;` |
|     19 | 6622 | `	if( nArg < 1 ){` |
|      - | 6623 | `		/* Missing arguments,return FALSE */` |
|      3 | 6624 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6625 | `		return PH7_OK;` |
|      - | 6626 | `	}` |
|      - | 6627 | `	/* Extract the target string */` |
|     17 | 6628 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6629 | `	zEnd = &zIn[nLen];` |
|     17 | 6630 | `	if( nLen < 1 ){` |
|      - | 6631 | `		/* Empty string,return FALSE */` |
|      3 | 6632 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6633 | `		return PH7_OK;` |
|      - | 6634 | `	}` |
|      - | 6635 | `	/* Perform the requested operation */` |
|     28 | 6636 | `	for(;;){` |
|     57 | 6637 | `		if( zIn >= zEnd ){` |
|      - | 6638 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6639 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6640 | `			return PH7_OK;` |
|      - | 6641 | `		}` |
|     53 | 6642 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 6643 | `			break;` |
|      - | 6644 | `		}` |
|      - | 6645 | `		/* Point to the next character */` |
|     43 | 6646 | `		zIn++;` |
|      1 | 6647 | `	}` |
|      - | 6648 | `	/* The test failed,return FALSE */` |
|     11 | 6649 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6650 | `	return PH7_OK;` |
|     10 | 6651 |  |
|      - | 6652 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 6653 | `/*` |
|      - | 6654 | ` * Section:` |
|      - | 6655 | ` *    URL handling Functions.` |
|      - | 6656 | ` * Status:` |
|      - | 6657 | ` *    Stable.` |
|      - | 6658 | ` */` |
|      - | 6659 | `/*` |
|      - | 6660 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 6661 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 6662 | ` */` |
|   1026 | 6663 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 6664 |  |
|      - | 6665 | `	/* Store in the call context result buffer */` |
|   1028 | 6666 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 6667 | `	return SXRET_OK;` |
|      2 | 6668 |  |
|      - | 6669 | `/*` |
|      - | 6670 | ` * string base64_encode(string $data)` |
|      - | 6671 | ` * string convert_uuencode(string $data)` |
|      - | 6672 | ` *  Encodes data with MIME base64` |
|      - | 6673 | ` * Parameter` |
|      - | 6674 | ` *  $data` |
|      - | 6675 | ` *    Data to encode` |
|      - | 6676 | ` * Return` |
|      - | 6677 | ` *  Encoded data or FALSE on failure.` |
|      - | 6678 | ` */` |
|     10 | 6679 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6680 |  |
|      - | 6681 | `	const char *zIn;` |
|      - | 6682 | `	int nLen;` |
|     11 | 6683 | `	if( nArg < 1 ){` |
|      - | 6684 | `		/* Missing arguments,return FALSE */` |
|      5 | 6685 | `		ph7_result_bool(pCtx,0);` |
|      5 | 6686 | `		return PH7_OK;` |
|      - | 6687 | `	}` |
|      - | 6688 | `	/* Extract the input string */` |
|      7 | 6689 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6690 | `	if( nLen < 1 ){` |
|      - | 6691 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6692 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6693 | `		return PH7_OK;` |
|      - | 6694 | `	}` |
|      - | 6695 | `	/* Perform the BASE64 encoding */` |
|      7 | 6696 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 6697 | `	return PH7_OK;` |
|      6 | 6698 |  |
|      - | 6699 | `/*` |
|      - | 6700 | ` * string base64_decode(string $data)` |
|      - | 6701 | ` * string convert_uudecode(string $data)` |
|      - | 6702 | ` *  Decodes data encoded with MIME base64` |
|      - | 6703 | ` * Parameter` |
|      - | 6704 | ` *  $data` |
|      - | 6705 | ` *    Encoded data.` |
|      - | 6706 | ` * Return` |
|      - | 6707 | ` *  Returns the original data or FALSE on failure.` |
|      - | 6708 | ` */` |
|     36 | 6709 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6710 |  |
|      - | 6711 | `	const char *zIn;` |
|      - | 6712 | `	int nLen;` |
|     38 | 6713 | `	if( nArg < 1 ){` |
|      - | 6714 | `		/* Missing arguments,return FALSE */` |
|      3 | 6715 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6716 | `		return PH7_OK;` |
|      - | 6717 | `	}` |
|      - | 6718 | `	/* Extract the input string */` |
|     36 | 6719 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 6720 | `	if( nLen < 1 ){` |
|      - | 6721 | `		/* Nothing to process,return FALSE */` |
|      3 | 6722 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6723 | `		return PH7_OK;` |
|      - | 6724 | `	}` |
|      - | 6725 | `	/* Perform the BASE64 decoding */` |
|     34 | 6726 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 6727 | `	return PH7_OK;` |
|     20 | 6728 |  |
|      - | 6729 | `/*` |
|      - | 6730 | ` * string urlencode(string $str)` |
|      - | 6731 | ` *  URL encoding` |
|      - | 6732 | ` * Parameter` |
|      - | 6733 | ` *  $data` |
|      - | 6734 | ` *   Input string.` |
|      - | 6735 | ` * Return` |
|      - | 6736 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 6737 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 6738 | ` *  encoded as plus (+) signs.` |
|      - | 6739 | ` */` |
|      6 | 6740 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6741 |  |
|      - | 6742 | `	const char *zIn;` |
|      - | 6743 | `	int nLen;` |
|      7 | 6744 | `	if( nArg < 1 ){` |
|      - | 6745 | `		/* Missing arguments,return FALSE */` |
|      3 | 6746 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6747 | `		return PH7_OK;` |
|      - | 6748 | `	}` |
|      - | 6749 | `	/* Extract the input string */` |
|      5 | 6750 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 6751 | `	if( nLen < 1 ){` |
|      - | 6752 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6753 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6754 | `		return PH7_OK;` |
|      - | 6755 | `	}` |
|      - | 6756 | `	/* Perform the URL encoding */` |
|      5 | 6757 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 6758 | `	return PH7_OK;` |
|      4 | 6759 |  |
|      - | 6760 | `/*` |
|      - | 6761 | ` * string urldecode(string $str)` |
|      - | 6762 | ` *  Decodes any %## encoding in the given string.` |
|      - | 6763 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 6764 | ` * Parameter` |
|      - | 6765 | ` *  $data` |
|      - | 6766 | ` *    Input string.` |
|      - | 6767 | ` * Return` |
|      - | 6768 | ` *  Decoded URL or FALSE on failure.` |
|      - | 6769 | ` */` |
|      8 | 6770 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6771 |  |
|      - | 6772 | `	const char *zIn;` |
|      - | 6773 | `	int nLen;` |
|      9 | 6774 | `	if( nArg < 1 ){` |
|      - | 6775 | `		/* Missing arguments,return FALSE */` |
|      3 | 6776 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6777 | `		return PH7_OK;` |
|      - | 6778 | `	}` |
|      - | 6779 | `	/* Extract the input string */` |
|      7 | 6780 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6781 | `	if( nLen < 1 ){` |
|      - | 6782 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6783 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6784 | `		return PH7_OK;` |
|      - | 6785 | `	}` |
|      - | 6786 | `	/* Perform the URL decoding */` |
|      7 | 6787 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 6788 | `	return PH7_OK;` |
|      5 | 6789 |  |
|      - | 6790 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6791 | `/* Table of the built-in functions */` |
|      - | 6792 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 6793 | `	   /* Variable handling functions */` |
|      - | 6794 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 6795 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 6796 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 6797 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 6798 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 6799 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 6800 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 6801 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 6802 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 6803 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 6804 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 6805 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 6806 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 6807 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 6808 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 6809 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 6810 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 6811 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 6812 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 6813 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 6814 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6815 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 6816 | `	   /* Math functions */` |
|      - | 6817 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 6818 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 6819 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 6820 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 6821 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 6822 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 6823 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 6824 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 6825 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 6826 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 6827 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 6828 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 6829 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 6830 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 6831 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 6832 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 6833 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 6834 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 6835 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 6836 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 6837 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 6838 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 6839 | `	{ "round",    PH7_builtin_round        },` |
|      - | 6840 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 6841 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 6842 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 6843 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 6844 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 6845 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 6846 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 6847 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 6848 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 6849 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6850 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6851 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 6852 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6853 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6854 | `	   /* String handling functions */` |
|      - | 6855 |  |
|      - | 6856 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 6857 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 6858 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 6859 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 6860 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 6861 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 6862 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 6863 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 6864 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 6865 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 6866 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 6867 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 6868 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 6869 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 6870 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 6871 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 6872 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 6873 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 6874 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 6875 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 6876 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 6877 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 6878 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 6879 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 6880 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 6881 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 6882 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 6883 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 6884 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 6885 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 6886 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 6887 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 6888 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 6889 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 6890 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 6891 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 6892 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 6893 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 6894 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 6895 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 6896 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 6897 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 6898 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 6899 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 6900 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 6901 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 6902 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 6903 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 6904 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 6905 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 6906 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 6907 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 6908 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6909 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6910 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 6911 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 6912 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 6913 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 6914 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6915 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6916 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 6917 |  |
|      - | 6918 |  |
|      - | 6919 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 6920 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 6921 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 6922 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 6923 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6924 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6925 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6926 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 6927 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 6928 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6929 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6930 |  |
|      - | 6931 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 6932 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 6933 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 6934 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 6935 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 6936 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 6937 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 6938 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 6939 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 6940 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 6941 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 6942 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 6943 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6944 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6945 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 6946 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6947 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6948 |  |
|      - | 6949 | `	         /* Ctype functions */` |
|      - | 6950 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 6951 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 6952 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 6953 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 6954 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 6955 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 6956 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 6957 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 6958 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 6959 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 6960 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 6961 | `	         /* Time functions */` |
|      - | 6962 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 6963 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 6964 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 6965 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 6966 | `	{ "date",        PH7_builtin_date         },` |
|      - | 6967 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 6968 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 6969 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 6970 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 6971 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 6972 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 6973 | `	        /* URL functions */` |
|      - | 6974 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 6975 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 6976 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 6977 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 6978 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 6979 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 6980 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 6981 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 6982 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6983 | `};` |
|      - | 6984 | `/*` |
|      - | 6985 | ` * Register the built-in functions defined above,the array functions` |
|      - | 6986 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 6987 | ` */` |
|   2678 | 6988 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 6989 |  |
|      - | 6990 | `	sxu32 n;` |
| 423126 | 6991 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 420448 | 6992 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 210225 | 6993 | `	}` |
|      - | 6994 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   2680 | 6995 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 6996 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   2680 | 6997 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   2680 | 6998 |  |
|      - | 6999 |  |
