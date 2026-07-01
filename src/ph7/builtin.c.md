# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3639/4106 lines (88.63%)

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
|      1 |   23 | `{` |
|     33 |   24 | `	int res = 0; /* Assume false by default */` |
|     33 |   25 | `	if( nArg > 0 ){` |
|     29 |   26 | `		res = ph7_value_is_bool(apArg[0]);` |
|     14 |   27 | `	}` |
|      - |   28 | `	/* Query result */` |
|     33 |   29 | `	ph7_result_bool(pCtx,res);` |
|     33 |   30 | `	return PH7_OK;` |
|      1 |   31 | `}` |
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
|      1 |   43 | `{` |
|    197 |   44 | `	int res = 0; /* Assume false by default */` |
|    197 |   45 | `	if( nArg > 0 ){` |
|    195 |   46 | `		res = ph7_value_is_float(apArg[0]);` |
|     97 |   47 | `	}` |
|      - |   48 | `	/* Query result */` |
|    197 |   49 | `	ph7_result_bool(pCtx,res);` |
|    197 |   50 | `	return PH7_OK;` |
|      1 |   51 | `}` |
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
|      2 |   63 | `{` |
|    634 |   64 | `	int res = 0; /* Assume false by default */` |
|    634 |   65 | `	if( nArg > 0 ){` |
|      - |   66 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   67 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   68 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    632 |   69 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    315 |   70 | `	}` |
|      - |   71 | `	/* Query result */` |
|    634 |   72 | `	ph7_result_bool(pCtx,res);` |
|    634 |   73 | `	return PH7_OK;` |
|      2 |   74 | `}` |
|      - |   75 | `/*` |
|      - |   76 | ` * bool is_string($var)` |
|      - |   77 | ` *  Finds out whether a variable is a string.` |
|      - |   78 | ` * Parameters` |
|      - |   79 | ` *   $var: The variable being evaluated.` |
|      - |   80 | ` * Return` |
|      - |   81 | ` *  TRUE if var is string. False otherwise.` |
|      - |   82 | ` */` |
|    126 |   83 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   84 | `{` |
|    127 |   85 | `	int res = 0; /* Assume false by default */` |
|    127 |   86 | `	if( nArg > 0 ){` |
|    125 |   87 | `		res = ph7_value_is_string(apArg[0]);` |
|     62 |   88 | `	}` |
|      - |   89 | `	/* Query result */` |
|    127 |   90 | `	ph7_result_bool(pCtx,res);` |
|    127 |   91 | `	return PH7_OK;` |
|      1 |   92 | `}` |
|      - |   93 | `/*` |
|      - |   94 | ` * bool is_null($var)` |
|      - |   95 | ` *  Finds out whether a variable is NULL.` |
|      - |   96 | ` * Parameters` |
|      - |   97 | ` *   $var: The variable being evaluated.` |
|      - |   98 | ` * Return` |
|      - |   99 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  100 | ` */` |
|     92 |  101 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  102 | `{` |
|     96 |  103 | `	int res = 0; /* Assume false by default */` |
|     96 |  104 | `	if( nArg > 0 ){` |
|     94 |  105 | `		res = ph7_value_is_null(apArg[0]);` |
|     45 |  106 | `	}` |
|      - |  107 | `	/* Query result */` |
|     96 |  108 | `	ph7_result_bool(pCtx,res);` |
|     96 |  109 | `	return PH7_OK;` |
|      4 |  110 | `}` |
|      - |  111 | `/*` |
|      - |  112 | ` * bool is_numeric($var)` |
|      - |  113 | ` *  Find out whether a variable is NULL.` |
|      - |  114 | ` * Parameters` |
|      - |  115 | ` *  $var: The variable being evaluated.` |
|      - |  116 | ` * Return` |
|      - |  117 | ` *  True if var is numeric. False otherwise.` |
|      - |  118 | ` */` |
|     38 |  119 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  120 | `{` |
|     43 |  121 | `	int res = 0; /* Assume false by default */` |
|     43 |  122 | `	if( nArg > 0 ){` |
|     41 |  123 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     18 |  124 | `	}` |
|      - |  125 | `	/* Query result */` |
|     43 |  126 | `	ph7_result_bool(pCtx,res);` |
|     43 |  127 | `	return PH7_OK;` |
|      5 |  128 | `}` |
|      - |  129 | `/*` |
|      - |  130 | ` * bool is_scalar($var)` |
|      - |  131 | ` *  Find out whether a variable is a scalar.` |
|      - |  132 | ` * Parameters` |
|      - |  133 | ` *  $var: The variable being evaluated.` |
|      - |  134 | ` * Return` |
|      - |  135 | ` *  True if var is scalar. False otherwise.` |
|      - |  136 | ` */` |
|     14 |  137 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  138 | `{` |
|     15 |  139 | `	int res = 0; /* Assume false by default */` |
|     15 |  140 | `	if( nArg > 0 ){` |
|     13 |  141 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  142 | `	}` |
|      - |  143 | `	/* Query result */` |
|     15 |  144 | `	ph7_result_bool(pCtx,res);` |
|     15 |  145 | `	return PH7_OK;` |
|      1 |  146 | `}` |
|      - |  147 | `/*` |
|      - |  148 | ` * bool is_array($var)` |
|      - |  149 | ` *  Find out whether a variable is an array.` |
|      - |  150 | ` * Parameters` |
|      - |  151 | ` *  $var: The variable being evaluated.` |
|      - |  152 | ` * Return` |
|      - |  153 | ` *  True if var is an array. False otherwise.` |
|      - |  154 | ` */` |
|    242 |  155 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  156 | `{` |
|    246 |  157 | `	int res = 0; /* Assume false by default */` |
|    246 |  158 | `	if( nArg > 0 ){` |
|    244 |  159 | `		res = ph7_value_is_array(apArg[0]);` |
|    120 |  160 | `	}` |
|      - |  161 | `	/* Query result */` |
|    246 |  162 | `	ph7_result_bool(pCtx,res);` |
|    246 |  163 | `	return PH7_OK;` |
|      4 |  164 | `}` |
|      - |  165 | `/*` |
|      - |  166 | ` * bool is_object($var)` |
|      - |  167 | ` *  Find out whether a variable is an object.` |
|      - |  168 | ` * Parameters` |
|      - |  169 | ` *  $var: The variable being evaluated.` |
|      - |  170 | ` * Return` |
|      - |  171 | ` *  True if var is an object. False otherwise.` |
|      - |  172 | ` */` |
|     22 |  173 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  174 | `{` |
|     23 |  175 | `	int res = 0; /* Assume false by default */` |
|     23 |  176 | `	if( nArg > 0 ){` |
|     21 |  177 | `		res = ph7_value_is_object(apArg[0]);` |
|     10 |  178 | `	}` |
|      - |  179 | `	/* Query result */` |
|     23 |  180 | `	ph7_result_bool(pCtx,res);` |
|     23 |  181 | `	return PH7_OK;` |
|      1 |  182 | `}` |
|      - |  183 | `/*` |
|      - |  184 | ` * bool is_resource($var)` |
|      - |  185 | ` *  Find out whether a variable is a resource.` |
|      - |  186 | ` * Parameters` |
|      - |  187 | ` *  $var: The variable being evaluated.` |
|      - |  188 | ` * Return` |
|      - |  189 | ` *  True if a resource. False otherwise.` |
|      - |  190 | ` */` |
|     60 |  191 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  192 | `{` |
|     64 |  193 | `	int res = 0; /* Assume false by default */` |
|     64 |  194 | `	if( nArg > 0 ){` |
|     62 |  195 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  196 | `	}` |
|     64 |  197 | `	ph7_result_bool(pCtx,res);` |
|     64 |  198 | `	return PH7_OK;` |
|      4 |  199 | `}` |
|      - |  200 | `/*` |
|      - |  201 | ` * float floatval($var)` |
|      - |  202 | ` *  Get float value of a variable.` |
|      - |  203 | ` * Parameter` |
|      - |  204 | ` *  $var: The variable being processed.` |
|      - |  205 | ` * Return` |
|      - |  206 | ` *  the float value of a variable.` |
|      - |  207 | ` */` |
|      6 |  208 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  209 | `{` |
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
|      1 |  220 | `}` |
|      - |  221 | `/*` |
|      - |  222 | ` * int intval($var)` |
|      - |  223 | ` *  Get integer value of a variable.` |
|      - |  224 | ` * Parameter` |
|      - |  225 | ` *  $var: The variable being processed.` |
|      - |  226 | ` * Return` |
|      - |  227 | ` *  the int value of a variable.` |
|      - |  228 | ` */` |
|     26 |  229 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  230 | `{` |
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
|      1 |  241 | `}` |
|      - |  242 | `/*` |
|      - |  243 | ` * string strval($var)` |
|      - |  244 | ` *  Get the string representation of a variable.` |
|      - |  245 | ` * Parameter` |
|      - |  246 | ` *  $var: The variable being processed.` |
|      - |  247 | ` * Return` |
|      - |  248 | ` *  the string value of a variable.` |
|      - |  249 | ` */` |
|      4 |  250 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  251 | `{` |
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
|      1 |  263 | `}` |
|      - |  264 | `/*` |
|      - |  265 | ` * bool boolval($var)` |
|      - |  266 | ` *  Get the boolean value of a variable.` |
|      - |  267 | ` * Parameter` |
|      - |  268 | ` *  $var: The variable being processed.` |
|      - |  269 | ` * Return` |
|      - |  270 | ` *  the bool value of a variable.` |
|      - |  271 | ` */` |
|     16 |  272 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  273 | `{` |
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
|     10 |  286 | `}` |
|      - |  287 | `/*` |
|      - |  288 | ` * bool empty($var)` |
|      - |  289 | ` *  Determine whether a variable is empty.` |
|      - |  290 | ` * Parameters` |
|      - |  291 | ` *   $var: The variable being checked.` |
|      - |  292 | ` * Return` |
|      - |  293 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  294 | ` */` |
|  27582 |  295 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  296 | `{` |
|  27587 |  297 | `	int res = 1; /* Assume empty by default */` |
|  27587 |  298 | `	if( nArg > 0 ){` |
|  27585 |  299 | `		res = ph7_value_is_empty(apArg[0]);` |
|  13790 |  300 | `	}` |
|  27587 |  301 | `	ph7_result_bool(pCtx,res);` |
|  27587 |  302 | `	return PH7_OK;` |
|      - |  303 |  |
|      5 |  304 | `}` |
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
| 209288 |  345 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  346 | `{` |
|      - |  347 | `	const char *zSource,*zOfft;` |
|      - |  348 | `	int nOfft,nLen,nSrcLen;` |
| 209293 |  349 | `	if( nArg < 2 ){` |
|      - |  350 | `		/* return FALSE */` |
|      5 |  351 | `		ph7_result_bool(pCtx,0);` |
|      5 |  352 | `		return PH7_OK;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Extract the target string */` |
| 209289 |  355 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 209289 |  356 | `	if( nSrcLen < 1 ){` |
|      - |  357 | `		/* Empty string,return FALSE */` |
|  11817 |  358 | `		ph7_result_bool(pCtx,0);` |
|  11817 |  359 | `		return PH7_OK;` |
|      - |  360 | `	}` |
| 197477 |  361 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  362 | `	/* Extract the offset */` |
| 197477 |  363 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 197477 |  364 | `	if( nOfft < 0 ){` |
|  32053 |  365 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  32053 |  366 | `		if( zOfft < zSource ){` |
|      - |  367 | `			/* Invalid offset */` |
|      5 |  368 | `			ph7_result_bool(pCtx,0);` |
|      5 |  369 | `			return PH7_OK;` |
|      - |  370 | `		}` |
|  32049 |  371 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  32049 |  372 | `		nOfft = (int)(zOfft-zSource);` |
| 181451 |  373 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  374 | `		/* Invalid offset */` |
|    187 |  375 | `		ph7_result_bool(pCtx,0);` |
|    187 |  376 | `		return PH7_OK;` |
|    ! 0 |  377 | `	}else{` |
| 165247 |  378 | `		zOfft = &zSource[nOfft];` |
| 165247 |  379 | `		nLen = nSrcLen - nOfft;` |
|      - |  380 | `	}` |
| 197291 |  381 | `	if( nArg > 2 ){` |
|      - |  382 | `		/* Extract the length */` |
| 162509 |  383 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 162509 |  384 | `		if( nLen == 0 ){` |
|      - |  385 | `			/* Invalid length,return an empty string */` |
|      5 |  386 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  387 | `			return PH7_OK;` |
| 162505 |  388 | `		}else if( nLen < 0 ){` |
|  32041 |  389 | `			nLen = nSrcLen + nLen - nOfft;` |
|  32041 |  390 | `			if( nLen < 1 ){` |
|      - |  391 | `				/* Invalid  length */` |
|      3 |  392 | `				nLen = nSrcLen - nOfft;` |
|      1 |  393 | `			}` |
|  16018 |  394 | `		}` |
| 162505 |  395 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  396 | `			/* Invalid length */` |
|   4845 |  397 | `			nLen = nSrcLen - nOfft;` |
|   2420 |  398 | `		}` |
|  81250 |  399 | `	}` |
|      - |  400 | `	/* Return the substring */` |
| 197287 |  401 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 197287 |  402 | `	return PH7_OK;` |
| 104649 |  403 | `}` |
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
|      1 |  426 | `{` |
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
|     14 |  495 | `}` |
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
|      1 |  513 | `{` |
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
|     13 |  579 | `}` |
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
|      1 |  594 | `{` |
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
|      9 |  640 | `}` |
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
|      4 |  653 | `{` |
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
|     16 |  718 | `}` |
|      - |  719 | `/*` |
|      - |  720 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - |  721 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - |  722 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - |  723 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - |  724 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - |  725 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - |  726 | ` * [zList, zList+nLen).` |
|      - |  727 | ` *` |
|      - |  728 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - |  729 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - |  730 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - |  731 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - |  732 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - |  733 | ` */` |
|     78 |  734 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      3 |  735 | `{` |
|     81 |  736 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|     81 |  737 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|     81 |  738 | `	SyZero(aMask,256);` |
|    291 |  739 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    213 |  740 | `		int c = zIn[0];` |
|    213 |  741 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - |  742 | `			/* Valid incrementing range c..zIn[3] */` |
|     20 |  743 | `			int hi = zIn[3],k;` |
|    364 |  744 | `			for( k = c ; k <= hi ; k++ ){` |
|    346 |  745 | `				aMask[k] = 1;` |
|    174 |  746 | `			}` |
|     20 |  747 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    213 |  748 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - |  749 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - |  750 | `			const char *zMsg;` |
|     20 |  751 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 |  752 | `				zMsg = "no character to the left of '..'";` |
|     18 |  753 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 |  754 | `				zMsg = "no character to the right of '..'";` |
|     14 |  755 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 |  756 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 |  757 | `			}else{` |
|    ! 0 |  758 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - |  759 | `			}` |
|     20 |  760 | `			if( zMsg ){` |
|     29 |  761 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 |  762 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 |  763 | `			}else{` |
|    ! 0 |  764 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  765 | `					"Invalid '..'-range");` |
|      - |  766 | `			}` |
|      - |  767 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - |  768 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 |  769 | `		}else{` |
|    177 |  770 | `			aMask[c] = 1;` |
|      - |  771 | `		}` |
|    108 |  772 | `	}` |
|     81 |  773 | `}` |
|      - |  774 | `/*` |
|      - |  775 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  776 | ` *  Quote string with slashes in a C style.` |
|      - |  777 | ` * Parameter` |
|      - |  778 | ` *  $str:` |
|      - |  779 | ` *    The string to be escaped.` |
|      - |  780 | ` *  $charlist:` |
|      - |  781 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  782 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  783 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  784 | ` * Return` |
|      - |  785 | ` *  Returns the escaped string.` |
|      - |  786 | ` * Note:` |
|      - |  787 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - |  788 | ` */` |
|     40 |  789 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  790 | `{` |
|      - |  791 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  792 | `	char aMask[256];` |
|      - |  793 | `	int nLen,nMask;` |
|      - |  794 | `	/* PHP enforces exactly two arguments. */` |
|     45 |  795 | `	if( nArg != 2 ){` |
|      8 |  796 | `		return PH7_VmThrowException(pCtx,` |
|      - |  797 | `			"ArgumentCountError",` |
|      - |  798 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  799 | `			nArg` |
|      - |  800 | `			);` |
|      - |  801 | `	}` |
|      - |  802 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  803 | `	 * treated as the empty string (PHP 8.1). */` |
|     40 |  804 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  805 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  806 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  807 | `			E_DEPRECATED,` |
|      - |  808 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  809 | `			);` |
|      - |  810 | `		/* treat as empty string; fall through to conversion logic */` |
|     68 |  811 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     52 |  812 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     32 |  813 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  814 | `		return PH7_VmThrowException(pCtx,` |
|      - |  815 | `			"TypeError",` |
|      - |  816 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  817 | `			ph7_type_name(apArg[0])` |
|      - |  818 | `			);` |
|      - |  819 | `	}` |
|      - |  820 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  821 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  822 | `	 * trigger a TypeError. */` |
|     37 |  823 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  824 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  825 | `			E_DEPRECATED,` |
|      - |  826 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  827 | `			);` |
|      - |  828 | `		/* allow through so it becomes empty string below */` |
|     64 |  829 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 |  830 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 |  831 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  832 | `		return PH7_VmThrowException(pCtx,` |
|      - |  833 | `			"TypeError",` |
|      - |  834 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  835 | `			ph7_type_name(apArg[1])` |
|      - |  836 | `			);` |
|      - |  837 | `	}` |
|      - |  838 | `	/* Extract the string to process */` |
|     35 |  839 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  840 | `	/* NULL would never reach here due to the check above. */` |
|     35 |  841 | `	if( nLen < 1 ){` |
|      - |  842 | `		/* Empty string returns itself. */` |
|      5 |  843 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  844 | `		return PH7_OK;` |
|      - |  845 | `	}` |
|      - |  846 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 |  847 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 |  848 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 |  849 | `	zEnd = &zIn[nLen];` |
|     31 |  850 | `	zCur = 0; /* cc warning */` |
|     37 |  851 | `	for(;;){` |
|     77 |  852 | `		if( zIn >= zEnd ){` |
|      - |  853 | `			/* No more input */` |
|     31 |  854 | `			break;` |
|      - |  855 | `		}` |
|     49 |  856 | `		zCur = zIn;` |
|    125 |  857 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 |  858 | `			zIn++;` |
|      3 |  859 | `		}` |
|     49 |  860 | `		if( zIn > zCur ){` |
|      - |  861 | `			/* Append raw contents */` |
|     43 |  862 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 |  863 | `		}` |
|     49 |  864 | `		if( zIn < zEnd ){` |
|      - |  865 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  866 | `			 * on platforms where char is signed. */` |
|     29 |  867 | `			int c = (unsigned char)zIn[0];` |
|      - |  868 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  869 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  870 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 |  871 | `			if( c == '\n' ){` |
|      3 |  872 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 |  873 | `			}else if( c == '\r' ){` |
|      3 |  874 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 |  875 | `			}else if( c == '\t' ){` |
|      3 |  876 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 |  877 | `			}else if( c == '\v' ){` |
|      3 |  878 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 |  879 | `			}else if( c == '\f' ){` |
|      3 |  880 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 |  881 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  882 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  883 | `				 * octal escapes (\001 not \1). */` |
|      7 |  884 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  885 | `			}else{` |
|     13 |  886 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  887 | `			}` |
|     13 |  888 | `		}` |
|     49 |  889 | `		zIn++;` |
|      3 |  890 | `	}` |
|     31 |  891 | `	return PH7_OK;` |
|     25 |  892 | `}` |
|      - |  893 | `/*` |
|      - |  894 | ` * string quotemeta(string $str)` |
|      - |  895 | ` *  Quote meta characters.` |
|      - |  896 | ` * Parameter` |
|      - |  897 | ` *  $str:` |
|      - |  898 | ` *    The string to be escaped.` |
|      - |  899 | ` * Return` |
|      - |  900 | ` *  Returns the escaped string.` |
|      - |  901 | `*/` |
|     12 |  902 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  903 | `{` |
|      - |  904 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  905 | `	char aMask[256];` |
|      - |  906 | `	int nLen;` |
|     14 |  907 | `	if( nArg < 1 ){` |
|      - |  908 | `		/* Nothing to process,retun NULL */` |
|      3 |  909 | `		ph7_result_null(pCtx);` |
|      3 |  910 | `		return PH7_OK;` |
|      - |  911 | `	}` |
|      - |  912 | `	/* Extract the string to process */` |
|     12 |  913 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 |  914 | `	if( nLen < 1 ){` |
|      - |  915 | `		/* Return the empty string */` |
|      3 |  916 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  917 | `		return PH7_OK;` |
|      - |  918 | `	}` |
|      - |  919 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 |  920 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 |  921 | `	zEnd = &zIn[nLen];` |
|     10 |  922 | `	zCur = 0; /* cc warning */` |
|     22 |  923 | `	for(;;){` |
|     46 |  924 | `		if( zIn >= zEnd ){` |
|      - |  925 | `			/* No more input */` |
|     10 |  926 | `			break;` |
|      - |  927 | `		}` |
|     38 |  928 | `		zCur = zIn;` |
|     76 |  929 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 |  930 | `			zIn++;` |
|      2 |  931 | `		}` |
|     38 |  932 | `		if( zIn > zCur ){` |
|      - |  933 | `			/* Append raw contents */` |
|     20 |  934 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 |  935 | `		}` |
|     38 |  936 | `		if( zIn < zEnd ){` |
|     36 |  937 | `			int c = zIn[0];` |
|     36 |  938 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 |  939 | `		}` |
|     38 |  940 | `		zIn++;` |
|      2 |  941 | `	}` |
|     10 |  942 | `	return PH7_OK;` |
|      8 |  943 | `}` |
|      - |  944 | `/*` |
|      - |  945 | ` * string stripslashes(string $str)` |
|      - |  946 | ` *  Un-quotes a quoted string.` |
|      - |  947 | ` *  Returns a string with backslashes before characters that need` |
|      - |  948 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  949 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  950 | ` * Parameter` |
|      - |  951 | ` *  $str` |
|      - |  952 | ` *   The input string.` |
|      - |  953 | ` * Return` |
|      - |  954 | ` *  Returns a string with backslashes stripped off.` |
|      - |  955 | ` */` |
|      8 |  956 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  957 | `{` |
|      - |  958 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  959 | `	int nLen;` |
|      9 |  960 | `	if( nArg < 1 ){` |
|      - |  961 | `		/* Nothing to process,retun NULL */` |
|      3 |  962 | `		ph7_result_null(pCtx);` |
|      3 |  963 | `		return PH7_OK;` |
|      - |  964 | `	}` |
|      - |  965 | `	/* Extract the string to process */` |
|      7 |  966 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  967 | `	if( zIn == 0 ){` |
|    ! 0 |  968 | `		ph7_result_null(pCtx);` |
|    ! 0 |  969 | `		return PH7_OK;` |
|      - |  970 | `	}` |
|      7 |  971 | `	zEnd = &zIn[nLen];` |
|      7 |  972 | `	zCur = 0; /* cc warning */` |
|      - |  973 | `	/* Encode the string */` |
|      4 |  974 | `	for(;;){` |
|      9 |  975 | `		if( zIn >= zEnd ){` |
|      - |  976 | `			/* No more input */` |
|      5 |  977 | `			break;` |
|      - |  978 | `		}` |
|      5 |  979 | `		zCur = zIn;` |
|     17 |  980 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  981 | `			zIn++;` |
|      1 |  982 | `		}` |
|      5 |  983 | `		if( zIn > zCur ){` |
|      - |  984 | `			/* Append raw contents */` |
|      5 |  985 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 |  986 | `		}` |
|      5 |  987 | `		if( &zIn[1] < zEnd ){` |
|      3 |  988 | `			int c = zIn[1];` |
|      3 |  989 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - |  990 | `				/* Ignore the backslash */` |
|      3 |  991 | `				zIn++;` |
|      1 |  992 | `			}` |
|      2 |  993 | `		}else{` |
|      3 |  994 | `			break;` |
|      - |  995 | `		}` |
|      1 |  996 | `	}` |
|      7 |  997 | `	return PH7_OK;` |
|      5 |  998 | `}` |
|      - |  999 | `/*` |
|      - | 1000 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - | 1001 | ` *  HTML escaping of special characters.` |
|      - | 1002 | ` *  The translations performed are:` |
|      - | 1003 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - | 1004 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - | 1005 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - | 1006 | ` *   '<' (less than) ==> '&lt;'` |
|      - | 1007 | ` *   '>' (greater than) ==> '&gt;'` |
|      - | 1008 | ` * Parameters` |
|      - | 1009 | ` *  $string` |
|      - | 1010 | ` *   The string being converted.` |
|      - | 1011 | ` * $flags` |
|      - | 1012 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - | 1013 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - | 1014 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - | 1015 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - | 1016 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - | 1017 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - | 1018 | ` * $charset` |
|      - | 1019 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - | 1020 | ` * Return` |
|      - | 1021 | ` *  The escaped string or NULL on failure.` |
|      - | 1022 | ` */` |
|     20 | 1023 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1024 | `{` |
|      - | 1025 | `	const char *zCur,*zIn,*zEnd;` |
|     21 | 1026 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - | 1027 | `	int nLen,c;` |
|     21 | 1028 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1029 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 | 1030 | `		ph7_result_null(pCtx);` |
|      9 | 1031 | `		return PH7_OK;` |
|      - | 1032 | `	}` |
|      - | 1033 | `	/* Extract the target string */` |
|     13 | 1034 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1035 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 | 1036 | `	if( nLen == 0 ){` |
|      3 | 1037 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1038 | `		return PH7_OK;` |
|      - | 1039 | `	}` |
|     11 | 1040 | `	zEnd = &zIn[nLen];` |
|      - | 1041 | `	/* Extract the flags if available */` |
|     11 | 1042 | `	if( nArg > 1 ){` |
|      9 | 1043 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1044 | `		if( iFlags < 0 ){` |
|      3 | 1045 | `			iFlags = 0x01\|0x40;` |
|      1 | 1046 | `		}` |
|      4 | 1047 | `	}` |
|      - | 1048 | `	/* Perform the requested operation */` |
|     23 | 1049 | `	for(;;){` |
|     47 | 1050 | `		if( zIn >= zEnd ){` |
|      9 | 1051 | `			break;` |
|      - | 1052 | `		}` |
|     39 | 1053 | `		zCur = zIn;` |
|     83 | 1054 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1055 | `			zIn++;` |
|      1 | 1056 | `		}` |
|     39 | 1057 | `		if( zCur < zIn ){` |
|      - | 1058 | `			/* Append the raw string verbatim */` |
|     17 | 1059 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1060 | `		}` |
|     39 | 1061 | `		if( zIn >= zEnd ){` |
|      3 | 1062 | `			break;` |
|      - | 1063 | `		}` |
|     37 | 1064 | `		c = zIn[0];` |
|     37 | 1065 | `		if( c == '&' ){` |
|      - | 1066 | `			/* Expand '&amp;' */` |
|      9 | 1067 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1068 | `		}else if( c == '<' ){` |
|      - | 1069 | `			/* Expand '&lt;' */` |
|      7 | 1070 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1071 | `		}else if( c == '>' ){` |
|      - | 1072 | `			/* Expand '&gt;' */` |
|      9 | 1073 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1074 | `		}else if( c == '\'' ){` |
|      5 | 1075 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1076 | `				/* Expand '&#039;' */` |
|      5 | 1077 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1078 | `			}else{` |
|      - | 1079 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1080 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1081 | `			}` |
|     13 | 1082 | `		}else if( c == '"' ){` |
|     11 | 1083 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1084 | `				/* Expand '&quot;' */` |
|      7 | 1085 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1086 | `			}else{` |
|      - | 1087 | `				/* Leave the double quote untouched */` |
|      5 | 1088 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1089 | `			}` |
|      5 | 1090 | `		}` |
|      - | 1091 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1092 | `		zIn++;` |
|      1 | 1093 | `	}` |
|     11 | 1094 | `	return PH7_OK;` |
|     11 | 1095 | `}` |
|      - | 1096 | `/*` |
|      - | 1097 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1098 | ` *  Unescape HTML entities.` |
|      - | 1099 | ` * Parameters` |
|      - | 1100 | ` *  $string` |
|      - | 1101 | ` *   The string to decode` |
|      - | 1102 | ` *  $quote_style` |
|      - | 1103 | ` *    The quote style. One of the following constants:` |
|      - | 1104 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1105 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1106 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1107 | ` * Return` |
|      - | 1108 | ` *  The unescaped string or NULL on failure.` |
|      - | 1109 | ` */` |
|     16 | 1110 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1111 | `{` |
|      - | 1112 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1113 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1114 | `	int nLen,nJump;` |
|     17 | 1115 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1116 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1117 | `		ph7_result_null(pCtx);` |
|      7 | 1118 | `		return PH7_OK;` |
|      - | 1119 | `	}` |
|      - | 1120 | `	/* Extract the target string */` |
|     11 | 1121 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1122 | `	zEnd = &zIn[nLen];` |
|      - | 1123 | `	/* Extract the flags if available */` |
|     11 | 1124 | `	if( nArg > 1 ){` |
|      7 | 1125 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1126 | `		if( iFlags < 0 ){` |
|      3 | 1127 | `			iFlags = 0x01;` |
|      1 | 1128 | `		}` |
|      3 | 1129 | `	}` |
|      - | 1130 | `	/* Perform the requested operation */` |
|     15 | 1131 | `	for(;;){` |
|     31 | 1132 | `		if( zIn >= zEnd ){` |
|     11 | 1133 | `			break;` |
|      - | 1134 | `		}` |
|     21 | 1135 | `		zCur = zIn;` |
|     51 | 1136 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1137 | `			zIn++;` |
|      1 | 1138 | `		}` |
|     21 | 1139 | `		if( zCur < zIn ){` |
|      - | 1140 | `			/* Append the raw string verbatim */` |
|      9 | 1141 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1142 | `		}` |
|     21 | 1143 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1144 | `		nJump = (int)sizeof(char);` |
|     21 | 1145 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1146 | `			/* &amp; ==> '&' */` |
|      3 | 1147 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1148 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1149 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1150 | `			/* &lt; ==> < */` |
|      3 | 1151 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1152 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1153 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1154 | `			/* &gt; ==> '>' */` |
|      3 | 1155 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1156 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1157 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1158 | `			/* &quot; ==> '"' */` |
|     13 | 1159 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1160 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1161 | `			}else{` |
|      - | 1162 | `				/* Leave untouched */` |
|      5 | 1163 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1164 | `			}` |
|     13 | 1165 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1166 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1167 | `			/* &#039; ==> ''' */` |
|      3 | 1168 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1169 | `				/* Expand ''' */` |
|      3 | 1170 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1171 | `			}else{` |
|      - | 1172 | `				/* Leave untouched */` |
|    ! 0 | 1173 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1174 | `			}` |
|      3 | 1175 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1176 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1177 | `			/* expand '&' */` |
|    ! 0 | 1178 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1179 | `		}else{` |
|      - | 1180 | `			/* No more input to process */` |
|    ! 0 | 1181 | `			break;` |
|      - | 1182 | `		}` |
|     21 | 1183 | `		zIn += nJump;` |
|      1 | 1184 | `	}` |
|     11 | 1185 | `	return PH7_OK;` |
|      9 | 1186 | `}` |
|      - | 1187 | `/* HTML encoding/Decoding table` |
|      - | 1188 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1189 | ` */` |
|      - | 1190 | `static const char *azHtmlEscape[] = {` |
|      - | 1191 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1192 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1193 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1194 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1195 | ` };` |
|      - | 1196 | `/*` |
|      - | 1197 | ` * array get_html_translation_table(void)` |
|      - | 1198 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1199 | ` * Parameters` |
|      - | 1200 | ` *  None` |
|      - | 1201 | ` * Return` |
|      - | 1202 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1203 | ` */` |
|      4 | 1204 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1205 | `{` |
|      - | 1206 | `	ph7_value *pArray,*pValue;` |
|      - | 1207 | `	sxu32 n;` |
|      - | 1208 | `	/* Element value */` |
|      5 | 1209 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1210 | `	if( pValue == 0 ){` |
|    ! 0 | 1211 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1212 | `		SXUNUSED(apArg);` |
|      - | 1213 | `		/* Return NULL */` |
|    ! 0 | 1214 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1215 | `		return PH7_OK;` |
|      - | 1216 | `	}` |
|      - | 1217 | `	/* Create a new array */` |
|      5 | 1218 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1219 | `	if( pArray == 0 ){` |
|      - | 1220 | `		/* Return NULL */` |
|    ! 0 | 1221 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1222 | `		return PH7_OK;` |
|      - | 1223 | `	}` |
|      - | 1224 | `	/* Make the table */` |
|     85 | 1225 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1226 | `		/* Prepare the value */` |
|     81 | 1227 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1228 | `		/* Insert the value */` |
|     81 | 1229 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1230 | `		/* Reset the string cursor */` |
|     81 | 1231 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1232 | `	}` |
|      - | 1233 | `	/*` |
|      - | 1234 | `	 * Return the array.` |
|      - | 1235 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1236 | `	 * released upon we return from this function.` |
|      - | 1237 | `	 */` |
|      5 | 1238 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1239 | `	return PH7_OK;` |
|      3 | 1240 | `}` |
|      - | 1241 | `/*` |
|      - | 1242 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1243 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1244 | ` * Parameters` |
|      - | 1245 | ` * $string` |
|      - | 1246 | ` *   The input string.` |
|      - | 1247 | ` * $flags` |
|      - | 1248 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1249 | ` * Return` |
|      - | 1250 | ` * The encoded string.` |
|      - | 1251 | ` */` |
|     10 | 1252 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1253 | `{` |
|     11 | 1254 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1255 | `	const char *zIn,*zEnd;` |
|      - | 1256 | `	int nLen,c;` |
|      - | 1257 | `	sxu32 n;` |
|     11 | 1258 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1259 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1260 | `		ph7_result_null(pCtx);` |
|      5 | 1261 | `		return PH7_OK;` |
|      - | 1262 | `	}` |
|      - | 1263 | `	/* Extract the target string */` |
|      7 | 1264 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1265 | `	/* Handle empty string up front */` |
|      7 | 1266 | `	if( nLen == 0 ){` |
|      3 | 1267 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1268 | `		return PH7_OK;` |
|      - | 1269 | `	}` |
|      5 | 1270 | `	zEnd = &zIn[nLen];` |
|      - | 1271 | `	/* Extract the flags if available */` |
|      5 | 1272 | `	if( nArg > 1 ){` |
|      3 | 1273 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1274 | `		if( iFlags < 0 ){` |
|      3 | 1275 | `			iFlags = 0x01;` |
|      1 | 1276 | `		}` |
|      1 | 1277 | `	}` |
|      - | 1278 | `	/* Perform the requested operation */` |
|     11 | 1279 | `	for(;;){` |
|     23 | 1280 | `		if( zIn >= zEnd ){` |
|      - | 1281 | `			/* No more input to process */` |
|      5 | 1282 | `			break;` |
|      - | 1283 | `		}` |
|     19 | 1284 | `		c = zIn[0];` |
|      - | 1285 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1286 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1287 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1288 | `				/* Got one */` |
|      9 | 1289 | `				break;` |
|      - | 1290 | `			}` |
|    108 | 1291 | `		}` |
|     19 | 1292 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1293 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1294 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1295 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 1296 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 1297 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 1298 | `				/* expand single quote verbatim */` |
|    ! 0 | 1299 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 1300 | `			}else{` |
|      9 | 1301 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 1302 | `			}` |
|      5 | 1303 | `		}else{` |
|      - | 1304 | `			/* Output character verbatim */` |
|     11 | 1305 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1306 | `		}` |
|     19 | 1307 | `		zIn++;` |
|      1 | 1308 | `	}` |
|      5 | 1309 | `	return PH7_OK;` |
|      6 | 1310 | `}` |
|      - | 1311 | `/*` |
|      - | 1312 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 1313 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 1314 | ` * Parameters` |
|      - | 1315 | ` * $string` |
|      - | 1316 | ` *   The input string.` |
|      - | 1317 | ` * $flags` |
|      - | 1318 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 1319 | ` * Return` |
|      - | 1320 | ` * The decoded string.` |
|      - | 1321 | ` */` |
|     28 | 1322 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1323 | `{` |
|      - | 1324 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 1325 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 1326 | `	int nLen;` |
|      - | 1327 | `	sxu32 n;` |
|     29 | 1328 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1329 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1330 | `		ph7_result_null(pCtx);` |
|      5 | 1331 | `		return PH7_OK;` |
|      - | 1332 | `	}` |
|      - | 1333 | `	/* Extract the target string */` |
|     25 | 1334 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1335 | `	zEnd = &zIn[nLen];` |
|      - | 1336 | `	/* Extract the flags if available */` |
|     25 | 1337 | `	if( nArg > 1 ){` |
|     15 | 1338 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 1339 | `		if( iFlags < 0 ){` |
|      3 | 1340 | `			iFlags = 0x01;` |
|      1 | 1341 | `		}` |
|      7 | 1342 | `	}` |
|      - | 1343 | `	/* Perform the requested operation */` |
|     27 | 1344 | `	for(;;){` |
|     55 | 1345 | `		if( zIn >= zEnd ){` |
|      - | 1346 | `			/* No more input to process */` |
|     13 | 1347 | `			break;` |
|      - | 1348 | `		}` |
|     43 | 1349 | `		zCur = zIn;` |
|    173 | 1350 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 1351 | `			zIn++;` |
|      1 | 1352 | `		}` |
|     43 | 1353 | `		if( zCur < zIn ){` |
|      - | 1354 | `			/* Append raw string verbatim */` |
|     27 | 1355 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 1356 | `		}` |
|     43 | 1357 | `		if( zIn >= zEnd ){` |
|     13 | 1358 | `			break;` |
|      - | 1359 | `		}` |
|     31 | 1360 | `		nLen = (int)(zEnd-zIn);` |
|      - | 1361 | `		/* Find an encoded sequence */` |
|    113 | 1362 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 1363 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 1364 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 1365 | `				/* Got one */` |
|     31 | 1366 | `				zIn += iLen;` |
|     31 | 1367 | `				break;` |
|      - | 1368 | `			}` |
|     42 | 1369 | `		}` |
|     31 | 1370 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 1371 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 1372 | `			/* Output the decoded character */` |
|     31 | 1373 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 1374 | `				/* Do not process single quotes */` |
|      9 | 1375 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 1376 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1377 | `				/* Do not process double quotes */` |
|      5 | 1378 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 1379 | `			}else{` |
|     19 | 1380 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 1381 | `			}` |
|     16 | 1382 | `		}else{` |
|      - | 1383 | `			/* Append '&' */` |
|    ! 0 | 1384 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1385 | `			zIn++;` |
|      - | 1386 | `		}` |
|      1 | 1387 | `	}` |
|     25 | 1388 | `	return PH7_OK;` |
|     15 | 1389 | `}` |
|      - | 1390 | `/*` |
|      - | 1391 | ` * int strlen($string)` |
|      - | 1392 | ` *  return the length of the given string.` |
|      - | 1393 | ` * Parameter` |
|      - | 1394 | ` *  string: The string being measured for length.` |
|      - | 1395 | ` * Return` |
|      - | 1396 | ` *  length of the given string.` |
|      - | 1397 | ` */` |
|   8534 | 1398 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1399 | `{` |
|   8539 | 1400 | `	int iLen = 0;` |
|   8539 | 1401 | `	if( nArg > 0 ){` |
|   8537 | 1402 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   4266 | 1403 | `	}` |
|      - | 1404 | `	/* String length */` |
|   8539 | 1405 | `	ph7_result_int(pCtx,iLen);` |
|   8539 | 1406 | `	return PH7_OK;` |
|      5 | 1407 | `}` |
|      - | 1408 | `/*` |
|      - | 1409 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1410 | ` *  Perform a binary safe string comparison.` |
|      - | 1411 | ` * Parameter` |
|      - | 1412 | ` *  str1: The first string` |
|      - | 1413 | ` *  str2: The second string` |
|      - | 1414 | ` * Return` |
|      - | 1415 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1416 | ` *  than str2, and 0 if they are equal.` |
|      - | 1417 | ` */` |
|     80 | 1418 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1419 | `{` |
|      - | 1420 | `	const char *z1,*z2;` |
|      - | 1421 | `	int n1,n2;` |
|      - | 1422 | `	int res;` |
|     81 | 1423 | `	if( nArg < 2 ){` |
|      5 | 1424 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 1425 | `		ph7_result_int(pCtx,res);` |
|      5 | 1426 | `		return PH7_OK;` |
|      - | 1427 | `	}` |
|      - | 1428 | `	/* Perform the comparison */` |
|     77 | 1429 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     77 | 1430 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     77 | 1431 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1432 | `	/* Comparison result */` |
|     77 | 1433 | `	ph7_result_int(pCtx,res);` |
|     77 | 1434 | `	return PH7_OK;` |
|     41 | 1435 | `}` |
|      - | 1436 | `/*` |
|      - | 1437 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1438 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1439 | ` * Parameter` |
|      - | 1440 | ` *  str1: The first string` |
|      - | 1441 | ` *  str2: The second string` |
|      - | 1442 | ` * Return` |
|      - | 1443 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1444 | ` *  than str2, and 0 if they are equal.` |
|      - | 1445 | ` */` |
|     20 | 1446 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1447 | `{` |
|      - | 1448 | `	const char *z1,*z2;` |
|      - | 1449 | `	int res;` |
|      - | 1450 | `	int n;` |
|     21 | 1451 | `	if( nArg < 3 ){` |
|      - | 1452 | `		/* Perform a standard comparison */` |
|      5 | 1453 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1454 | `	}` |
|      - | 1455 | `	/* Desired comparison length */` |
|     17 | 1456 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1457 | `	if( n < 0 ){` |
|      - | 1458 | `		/* Invalid length */` |
|      3 | 1459 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1460 | `		return PH7_OK;` |
|      - | 1461 | `	}` |
|      - | 1462 | `	/* Perform the comparison */` |
|     15 | 1463 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1464 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1465 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1466 | `	/* Comparison result */` |
|     15 | 1467 | `	ph7_result_int(pCtx,res);` |
|     15 | 1468 | `	return PH7_OK;` |
|     11 | 1469 | `}` |
|      - | 1470 | `/*` |
|      - | 1471 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1472 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1473 | ` * Parameter` |
|      - | 1474 | ` *  str1: The first string` |
|      - | 1475 | ` *  str2: The second string` |
|      - | 1476 | ` * Return` |
|      - | 1477 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1478 | ` *  than str2, and 0 if they are equal.` |
|      - | 1479 | ` */` |
|     22 | 1480 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1481 | `{` |
|      - | 1482 | `	const char *z1,*z2;` |
|      - | 1483 | `	int n1,n2;` |
|      - | 1484 | `	int res;` |
|     23 | 1485 | `	if( nArg < 2 ){` |
|      9 | 1486 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 1487 | `		ph7_result_int(pCtx,res);` |
|      9 | 1488 | `		return PH7_OK;` |
|      - | 1489 | `	}` |
|      - | 1490 | `	/* Perform the comparison */` |
|     15 | 1491 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1492 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1493 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1494 | `	/* Comparison result */` |
|     15 | 1495 | `	ph7_result_int(pCtx,res);` |
|     15 | 1496 | `	return PH7_OK;` |
|     12 | 1497 | `}` |
|      - | 1498 | `/*` |
|      - | 1499 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1500 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1501 | ` * Parameter` |
|      - | 1502 | ` *  $str1: The first string` |
|      - | 1503 | ` *  $str2: The second string` |
|      - | 1504 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1505 | ` * Return` |
|      - | 1506 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1507 | ` *  than str2, and 0 if they are equal.` |
|      - | 1508 | ` */` |
|      8 | 1509 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1510 | `{` |
|      - | 1511 | `	const char *z1,*z2;` |
|      - | 1512 | `	int res;` |
|      - | 1513 | `	int n;` |
|      9 | 1514 | `	if( nArg < 3 ){` |
|      - | 1515 | `		/* Perform a standard comparison */` |
|      5 | 1516 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1517 | `	}` |
|      - | 1518 | `	/* Desired comparison length */` |
|      5 | 1519 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1520 | `	if( n < 0 ){` |
|      - | 1521 | `		/* Invalid length */` |
|    ! 0 | 1522 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1523 | `		return PH7_OK;` |
|      - | 1524 | `	}` |
|      - | 1525 | `	/* Perform the comparison */` |
|      5 | 1526 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1527 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1528 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1529 | `	/* Comparison result */` |
|      5 | 1530 | `	ph7_result_int(pCtx,res);` |
|      5 | 1531 | `	return PH7_OK;` |
|      5 | 1532 | `}` |
|      - | 1533 | `/*` |
|      - | 1534 | ` * Implode context [i.e: it's private data].` |
|      - | 1535 | ` * A pointer to the following structure is forwarded` |
|      - | 1536 | ` * verbatim to the array walker callback defined below.` |
|      - | 1537 | ` */` |
|      - | 1538 | `struct implode_data {` |
|      - | 1539 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1540 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1541 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1542 | `	int nSeplen;          /* Separator length */` |
|      - | 1543 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1544 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1545 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1546 | `};` |
|      - | 1547 | `/*` |
|      - | 1548 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1549 | ` * The following routine is invoked for each array entry passed` |
|      - | 1550 | ` * to the implode() function.` |
|      - | 1551 | ` */` |
| 131164 | 1552 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1553 | `{` |
|  65582 | 1554 | `	SXUNUSED(pKey);` |
| 131169 | 1555 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1556 | `	const char *zData;` |
|      - | 1557 | `	int nLen;` |
| 131169 | 1558 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1559 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1560 | `			if( !pData->bFirst ){` |
|      - | 1561 | `				/* append the separator first */` |
|      3 | 1562 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1563 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1564 | `					return PH7_ABORT;` |
|      - | 1565 | `				}` |
|      2 | 1566 | `			}else{` |
|    ! 0 | 1567 | `				pData->bFirst = 0;` |
|      - | 1568 | `			}` |
|      1 | 1569 | `		}` |
|      - | 1570 | `		/* Recurse */` |
|      3 | 1571 | `		pData->bFirst = 1;` |
|      3 | 1572 | `		pData->nRecCount++;` |
|      3 | 1573 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1574 | `		pData->nRecCount--;` |
|      - | 1575 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1576 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1577 | `			return PH7_ABORT;` |
|      - | 1578 | `		}` |
|      3 | 1579 | `		return PH7_OK;` |
|      - | 1580 | `	}` |
|      - | 1581 | `	/* Extract the string representation of the entry value */` |
| 131167 | 1582 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1583 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 131167 | 1584 | `	if( pData->bFirst ){` |
|  32385 | 1585 | `		pData->bFirst = 0;` |
| 114977 | 1586 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1587 | `		/* append the separator first */` |
|  98775 | 1588 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1589 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1590 | `			return PH7_ABORT;` |
|      - | 1591 | `		}` |
|  49385 | 1592 | `	}` |
|      - | 1593 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 131167 | 1594 | `	if( nLen > 0 ){` |
| 119355 | 1595 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1596 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1597 | `			return PH7_ABORT;` |
|      - | 1598 | `		}` |
|  59675 | 1599 | `	}` |
| 131167 | 1600 | `	return PH7_OK;` |
|  65587 | 1601 | `}` |
|      - | 1602 | `/*` |
|      - | 1603 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1604 | ` * string implode(array $pieces,...)` |
|      - | 1605 | ` *  Join array elements with a string.` |
|      - | 1606 | ` * $glue` |
|      - | 1607 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1608 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1609 | ` * $pieces` |
|      - | 1610 | ` *   The array of strings to implode.` |
|      - | 1611 | ` * Return` |
|      - | 1612 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1613 | ` *  order, with the glue string between each element.` |
|      - | 1614 | ` */` |
|  32406 | 1615 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1616 | `{` |
|      - | 1617 | `	struct implode_data imp_data;` |
|  32411 | 1618 | `	int i = 1;` |
|  32411 | 1619 | `	if( nArg < 1 ){` |
|      - | 1620 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1621 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1622 | `		return PH7_OK;` |
|      - | 1623 | `	}` |
|      - | 1624 | `	/* Prepare the implode context */` |
|  32411 | 1625 | `	imp_data.pCtx = pCtx;` |
|  32411 | 1626 | `	imp_data.bRecursive = 0;` |
|  32411 | 1627 | `	imp_data.bFirst = 1;` |
|  32411 | 1628 | `	imp_data.nRecCount = 0;` |
|  32411 | 1629 | `	imp_data.rc = SXRET_OK;` |
|  32411 | 1630 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  32409 | 1631 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16207 | 1632 | `	}else{` |
|      3 | 1633 | `		imp_data.zSep = 0;` |
|      3 | 1634 | `		imp_data.nSeplen = 0;` |
|      3 | 1635 | `		i = 0;` |
|      - | 1636 | `	}` |
|  32411 | 1637 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1638 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1639 | `	}` |
|      - | 1640 | `	/* Start the 'join' process */` |
|  64817 | 1641 | `	while( i < nArg ){` |
|  32411 | 1642 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1643 | `			/* Iterate throw array entries */` |
|  32411 | 1644 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1645 | `			/* Surface a callback allocation failure as a fatal */` |
|  32411 | 1646 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1647 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1648 | `			}` |
|  16208 | 1649 | `		}else{` |
|      - | 1650 | `			const char *zData;` |
|      - | 1651 | `			int nLen;` |
|      - | 1652 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1653 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1654 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1655 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1656 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1657 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1658 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1659 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1660 | `				}` |
|    ! 0 | 1661 | `			}` |
|      - | 1662 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1663 | `			if( nLen > 0 ){` |
|    ! 0 | 1664 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1665 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1666 | `				}` |
|    ! 0 | 1667 | `			}` |
|      - | 1668 | `		}` |
|  32411 | 1669 | `		i++;` |
|      5 | 1670 | `	}` |
|  32411 | 1671 | `	return PH7_OK;` |
|  16208 | 1672 | `}` |
|      - | 1673 | `/*` |
|      - | 1674 | ` * Symisc eXtension:` |
|      - | 1675 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1676 | ` * Purpose` |
|      - | 1677 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1678 | ` * Example:` |
|      - | 1679 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1680 | ` *   echo implode_recursive("/",$a);` |
|      - | 1681 | ` *   Will output` |
|      - | 1682 | ` *     usr/home/dean.` |
|      - | 1683 | ` *   While the standard implode would produce.` |
|      - | 1684 | ` *    usr/Array.` |
|      - | 1685 | ` * Parameter` |
|      - | 1686 | ` *  Refer to implode().` |
|      - | 1687 | ` * Return` |
|      - | 1688 | ` *  Refer to implode().` |
|      - | 1689 | ` */` |
|     12 | 1690 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1691 | `{` |
|      - | 1692 | `	struct implode_data imp_data;` |
|     13 | 1693 | `	int i = 1;` |
|     13 | 1694 | `	if( nArg < 1 ){` |
|      - | 1695 | `		/* Missing argument,return NULL */` |
|      3 | 1696 | `		ph7_result_null(pCtx);` |
|      3 | 1697 | `		return PH7_OK;` |
|      - | 1698 | `	}` |
|      - | 1699 | `	/* Prepare the implode context */` |
|     11 | 1700 | `	imp_data.pCtx = pCtx;` |
|     11 | 1701 | `	imp_data.bRecursive = 1;` |
|     11 | 1702 | `	imp_data.bFirst = 1;` |
|     11 | 1703 | `	imp_data.nRecCount = 0;` |
|     11 | 1704 | `	imp_data.rc = SXRET_OK;` |
|     11 | 1705 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1706 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1707 | `	}else{` |
|    ! 0 | 1708 | `		imp_data.zSep = 0;` |
|    ! 0 | 1709 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1710 | `		i = 0;` |
|      - | 1711 | `	}` |
|     11 | 1712 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1713 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1714 | `	}` |
|      - | 1715 | `	/* Start the 'join' process */` |
|     21 | 1716 | `	while( i < nArg ){` |
|     11 | 1717 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1718 | `			/* Iterate throw array entries */` |
|      3 | 1719 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1720 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 1721 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1722 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1723 | `			}` |
|      2 | 1724 | `		}else{` |
|      - | 1725 | `			const char *zData;` |
|      - | 1726 | `			int nLen;` |
|      - | 1727 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1728 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1729 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1730 | `			if( imp_data.bFirst ){` |
|      9 | 1731 | `				imp_data.bFirst = 0;` |
|      4 | 1732 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1733 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1734 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1735 | `				}` |
|    ! 0 | 1736 | `			}` |
|      - | 1737 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1738 | `			if( nLen > 0 ){` |
|      9 | 1739 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1740 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1741 | `				}` |
|      4 | 1742 | `			}` |
|      - | 1743 | `		}` |
|     11 | 1744 | `		i++;` |
|      1 | 1745 | `	}` |
|     11 | 1746 | `	return PH7_OK;` |
|      7 | 1747 | `}` |
|      - | 1748 | `/*` |
|      - | 1749 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1750 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1751 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1752 | ` * Parameters` |
|      - | 1753 | ` *  $delimiter` |
|      - | 1754 | ` *   The boundary string.` |
|      - | 1755 | ` * $string` |
|      - | 1756 | ` *   The input string.` |
|      - | 1757 | ` * $limit` |
|      - | 1758 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1759 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1760 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1761 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1762 | ` * Returns` |
|      - | 1763 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1764 | ` *  on boundaries formed by the delimiter.` |
|      - | 1765 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1766 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1767 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1768 | ` *  will be returned.` |
|      - | 1769 | ` * NOTE:` |
|      - | 1770 | ` *  Negative limit is not supported.` |
|      - | 1771 | ` */` |
|   6138 | 1772 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1773 | `{` |
|      - | 1774 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1775 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1776 | `	ph7_value *pArray;` |
|      - | 1777 | `	ph7_value *pValue;` |
|      - | 1778 | `	sxu32 nOfft;` |
|      - | 1779 | `	sxi32 rc;` |
|   6143 | 1780 | `	if( nArg < 2 ){` |
|      - | 1781 | `		/* Missing arguments,return FALSE */` |
|      9 | 1782 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1783 | `		return PH7_OK;` |
|      - | 1784 | `	}` |
|      - | 1785 | `	/* Extract the delimiter */` |
|   6135 | 1786 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6135 | 1787 | `	if( nDelim < 1 ){` |
|      - | 1788 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1789 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1790 | `		return PH7_OK;` |
|      - | 1791 | `	}` |
|      - | 1792 | `	/* Extract the string */` |
|   6133 | 1793 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6133 | 1794 | `	if( nStrlen < 1 ){` |
|      - | 1795 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 1796 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 1797 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 1798 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 1799 | `		if( pArrayTmp == 0 ){` |
|      - | 1800 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1801 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1802 | `			return PH7_OK;` |
|      - | 1803 | `		}` |
|      7 | 1804 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 1805 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 1806 | `			if( pValueTmp == 0 ){` |
|      - | 1807 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 1808 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 1809 | `				return PH7_OK;` |
|      - | 1810 | `			}` |
|      5 | 1811 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 1812 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 1813 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1814 | `			}` |
|      2 | 1815 | `		}` |
|      7 | 1816 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 1817 | `		return PH7_OK;` |
|      - | 1818 | `	}` |
|      - | 1819 | `	/* Point to the end of the string */` |
|   6127 | 1820 | `	zEnd = &zString[nStrlen];` |
|      - | 1821 | `	/* Create the array */` |
|   6127 | 1822 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6127 | 1823 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6127 | 1824 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1825 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1826 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1827 | `		return PH7_OK;` |
|      - | 1828 | `	}` |
|      - | 1829 | `	/* Set a defualt limit */` |
|   6127 | 1830 | `	iLimit = SXI32_HIGH;` |
|   6127 | 1831 | `	if( nArg > 2 ){` |
|     29 | 1832 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     29 | 1833 | `		if( iLimit < 0 ){` |
|      - | 1834 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 1835 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 1836 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 1837 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 1838 | `			int nTotal = 1,nKeep;` |
|     17 | 1839 | `			const char *zScan = zString;` |
|      - | 1840 | `			sxu32 nScanOfft;` |
|     57 | 1841 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 1842 | `				nTotal++;` |
|     41 | 1843 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 1844 | `			}` |
|     17 | 1845 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 1846 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 1847 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 1848 | `				/* Emit the next clean component */` |
|     23 | 1849 | `				zCur = &zString[nOfft];` |
|     23 | 1850 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 1851 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1852 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1853 | `				}` |
|     23 | 1854 | `				zString = &zCur[nDelim];` |
|     23 | 1855 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 1856 | `			}` |
|     17 | 1857 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 1858 | `			return PH7_OK;` |
|      - | 1859 | `		}` |
|     13 | 1860 | `		if( iLimit == 0 ){` |
|      5 | 1861 | `			iLimit = 1;` |
|      2 | 1862 | `		}` |
|     13 | 1863 | `		iLimit--;` |
|      6 | 1864 | `	}` |
|      - | 1865 | `	/* Start exploding */` |
|  71161 | 1866 | `	for(;;){` |
| 142327 | 1867 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 142327 | 1868 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1869 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6111 | 1870 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6111 | 1871 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1872 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1873 | `			}` |
|   6111 | 1874 | `			break;` |
|      - | 1875 | `		}` |
|      - | 1876 | `		/* Point to the desired offset */` |
| 136221 | 1877 | `		zCur = &zString[nOfft];` |
|      - | 1878 | `		/* Perform the store operation (may be empty) */` |
| 136221 | 1879 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 136221 | 1880 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1881 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1882 | `		}` |
|      - | 1883 | `		/* Point beyond the delimiter */` |
| 136221 | 1884 | `		zString = &zCur[nDelim];` |
|      - | 1885 | `		/* Reset the cursor */` |
| 136221 | 1886 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1887 | `	}` |
|      - | 1888 | `	/* Return the freshly created array */` |
|   6111 | 1889 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1890 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1891 | `	 * released as soon we return from this foregin function.` |
|      - | 1892 | `	 */` |
|   6111 | 1893 | `	return PH7_OK;` |
|   3074 | 1894 | `}` |
|      - | 1895 | `/*` |
|      - | 1896 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1897 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1898 | ` * Parameters` |
|      - | 1899 | ` *  $str` |
|      - | 1900 | ` *   The string that will be trimmed.` |
|      - | 1901 | ` * $charlist` |
|      - | 1902 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1903 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1904 | ` *   With .. you can specify a range of characters.` |
|      - | 1905 | ` * Returns.` |
|      - | 1906 | ` *  Thr processed string.` |
|      - | 1907 | ` * NOTE:` |
|      - | 1908 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1909 | ` */` |
|  13978 | 1910 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1911 | `{` |
|      - | 1912 | `	const char *zString;` |
|      - | 1913 | `	int nLen;` |
|  13983 | 1914 | `	if( nArg < 1 ){` |
|      - | 1915 | `		/* Missing arguments,return null */` |
|      3 | 1916 | `		ph7_result_null(pCtx);` |
|      3 | 1917 | `		return PH7_OK;` |
|      - | 1918 | `	}` |
|      - | 1919 | `	/* Extract the target string */` |
|  13981 | 1920 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  13981 | 1921 | `	if( nLen < 1 ){` |
|      - | 1922 | `		/* Empty string,return */` |
|   1737 | 1923 | `		ph7_result_string(pCtx,"",0);` |
|   1737 | 1924 | `		return PH7_OK;` |
|      - | 1925 | `	}` |
|      - | 1926 | `	/* Start the trim process */` |
|  12249 | 1927 | `	if( nArg < 2 ){` |
|      - | 1928 | `		SyString sStr;` |
|      - | 1929 | `		/* Remove white spaces and NUL bytes */` |
|  12219 | 1930 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  29941 | 1931 | `		SyStringFullTrimSafe(&sStr);` |
|  12219 | 1932 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6112 | 1933 | `	}else{` |
|      - | 1934 | `		/* Char list */` |
|      - | 1935 | `		const char *zList;` |
|      - | 1936 | `		int nListlen;` |
|     33 | 1937 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 1938 | `		if( nListlen < 1 ){` |
|      - | 1939 | `			/* Return the string unchanged */` |
|      6 | 1940 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 1941 | `		}else{` |
|      - | 1942 | `			char aMask[256];` |
|     29 | 1943 | `			const char *zEnd = &zString[nLen];` |
|     29 | 1944 | `			const char *zCur = zString;` |
|     29 | 1945 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1946 | `			/* Left trim */` |
|     79 | 1947 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 1948 | `				zCur++;` |
|      3 | 1949 | `			}` |
|      - | 1950 | `			/* Right trim */` |
|     79 | 1951 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 1952 | `				zEnd--;` |
|      3 | 1953 | `			}` |
|     29 | 1954 | `			if( zCur >= zEnd ){` |
|      - | 1955 | `				/* Return the empty string */` |
|    ! 0 | 1956 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1957 | `			}else{` |
|     29 | 1958 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1959 | `			}` |
|      - | 1960 | `		}` |
|      - | 1961 | `	}` |
|  12249 | 1962 | `	return PH7_OK;` |
|   6994 | 1963 | `}` |
|      - | 1964 | `/*` |
|      - | 1965 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1966 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1967 | ` * Parameters` |
|      - | 1968 | ` *  $str` |
|      - | 1969 | ` *   The string that will be trimmed.` |
|      - | 1970 | ` * $charlist` |
|      - | 1971 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1972 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1973 | ` *   With .. you can specify a range of characters.` |
|      - | 1974 | ` * Returns.` |
|      - | 1975 | ` *  Thr processed string.` |
|      - | 1976 | ` * NOTE:` |
|      - | 1977 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1978 | ` */` |
|     30 | 1979 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1980 | `{` |
|      - | 1981 | `	const char *zString;` |
|      - | 1982 | `	int nLen;` |
|     33 | 1983 | `	if( nArg < 1 ){` |
|      - | 1984 | `		/* Missing arguments,return null */` |
|      3 | 1985 | `		ph7_result_null(pCtx);` |
|      3 | 1986 | `		return PH7_OK;` |
|      - | 1987 | `	}` |
|      - | 1988 | `	/* Extract the target string */` |
|     31 | 1989 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1990 | `	if( nLen < 1 ){` |
|      - | 1991 | `		/* Empty string,return */` |
|      5 | 1992 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1993 | `		return PH7_OK;` |
|      - | 1994 | `	}` |
|      - | 1995 | `	/* Start the trim process */` |
|     27 | 1996 | `	if( nArg < 2 ){` |
|      - | 1997 | `		SyString sStr;` |
|      - | 1998 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1999 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2000 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2001 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2002 | `	}else{` |
|      - | 2003 | `		/* Char list */` |
|      - | 2004 | `		const char *zList;` |
|      - | 2005 | `		int nListlen;` |
|     11 | 2006 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     11 | 2007 | `		if( nListlen < 1 ){` |
|      - | 2008 | `			/* Return the string unchanged */` |
|    ! 0 | 2009 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2010 | `		}else{` |
|      - | 2011 | `			char aMask[256];` |
|     11 | 2012 | `			const char *zEnd = &zString[nLen];` |
|     11 | 2013 | `			const char *zCur = zString;` |
|     11 | 2014 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2015 | `			/* Right trim */` |
|     29 | 2016 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 2017 | `				zEnd--;` |
|      2 | 2018 | `			}` |
|     11 | 2019 | `			if( zEnd <= zCur ){` |
|      - | 2020 | `				/* Return the empty string */` |
|    ! 0 | 2021 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2022 | `			}else{` |
|     11 | 2023 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2024 | `			}` |
|      - | 2025 | `		}` |
|      - | 2026 | `	}` |
|     27 | 2027 | `	return PH7_OK;` |
|     18 | 2028 | `}` |
|      - | 2029 | `/*` |
|      - | 2030 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2031 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2032 | ` * Parameters` |
|      - | 2033 | ` *  $str` |
|      - | 2034 | ` *   The string that will be trimmed.` |
|      - | 2035 | ` * $charlist` |
|      - | 2036 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2037 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2038 | ` *   With .. you can specify a range of characters.` |
|      - | 2039 | ` * Returns.` |
|      - | 2040 | ` *  Thr processed string.` |
|      - | 2041 | ` * NOTE:` |
|      - | 2042 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2043 | ` */` |
|     14 | 2044 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2045 | `{` |
|      - | 2046 | `	const char *zString;` |
|      - | 2047 | `	int nLen;` |
|     16 | 2048 | `	if( nArg < 1 ){` |
|      - | 2049 | `		/* Missing arguments,return null */` |
|      3 | 2050 | `		ph7_result_null(pCtx);` |
|      3 | 2051 | `		return PH7_OK;` |
|      - | 2052 | `	}` |
|      - | 2053 | `	/* Extract the target string */` |
|     14 | 2054 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     14 | 2055 | `	if( nLen < 1 ){` |
|      - | 2056 | `		/* Empty string,return */` |
|    ! 0 | 2057 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2058 | `		return PH7_OK;` |
|      - | 2059 | `	}` |
|      - | 2060 | `	/* Start the trim process */` |
|     14 | 2061 | `	if( nArg < 2 ){` |
|      - | 2062 | `		SyString sStr;` |
|      - | 2063 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2064 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2065 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2066 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2067 | `	}else{` |
|      - | 2068 | `		/* Char list */` |
|      - | 2069 | `		const char *zList;` |
|      - | 2070 | `		int nListlen;` |
|     12 | 2071 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     12 | 2072 | `		if( nListlen < 1 ){` |
|      - | 2073 | `			/* Return the string unchanged */` |
|      3 | 2074 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2075 | `		}else{` |
|      - | 2076 | `			char aMask[256];` |
|     10 | 2077 | `			const char *zEnd = &zString[nLen];` |
|     10 | 2078 | `			const char *zCur = zString;` |
|     10 | 2079 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2080 | `			/* Left trim */` |
|     28 | 2081 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     20 | 2082 | `				zCur++;` |
|      2 | 2083 | `			}` |
|     10 | 2084 | `			if( zCur >= zEnd ){` |
|      - | 2085 | `				/* Return the empty string */` |
|    ! 0 | 2086 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2087 | `			}else{` |
|     10 | 2088 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2089 | `			}` |
|      - | 2090 | `		}` |
|      - | 2091 | `	}` |
|     14 | 2092 | `	return PH7_OK;` |
|      9 | 2093 | `}` |
|      - | 2094 | `/*` |
|      - | 2095 | ` * string strtolower(string $str)` |
|      - | 2096 | ` *  Make a string lowercase.` |
|      - | 2097 | ` * Parameters` |
|      - | 2098 | ` *  $str` |
|      - | 2099 | ` *   The input string.` |
|      - | 2100 | ` * Returns.` |
|      - | 2101 | ` *  The lowercased string.` |
|      - | 2102 | ` */` |
|  32038 | 2103 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2104 | `{` |
|      - | 2105 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2106 | `	int nLen;` |
|  32043 | 2107 | `	if( nArg < 1 ){` |
|      - | 2108 | `		/* Missing arguments,return null */` |
|      3 | 2109 | `		ph7_result_null(pCtx);` |
|      3 | 2110 | `		return PH7_OK;` |
|      - | 2111 | `	}` |
|      - | 2112 | `	/* Extract the target string */` |
|  32041 | 2113 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  32041 | 2114 | `	if( nLen < 1 ){` |
|      - | 2115 | `		/* Empty string,return */` |
|      3 | 2116 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2117 | `		return PH7_OK;` |
|      - | 2118 | `	}` |
|      - | 2119 | `	/* Perform the requested operation */` |
|  32039 | 2120 | `	zEnd = &zString[nLen];` |
| 100938 | 2121 | `	for(;;){` |
| 201881 | 2122 | `		if( zString >= zEnd ){` |
|      - | 2123 | `			/* No more input,break immediately */` |
|  32039 | 2124 | `			break;` |
|      - | 2125 | `		}` |
| 169847 | 2126 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2127 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2128 | `			zCur = zString;` |
|    ! 0 | 2129 | `			zString++;` |
|    ! 0 | 2130 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2131 | `				zString++;` |
|    ! 0 | 2132 | `			}` |
|      - | 2133 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2134 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2135 | `		}else{` |
| 169847 | 2136 | `			int c = zString[0];` |
| 169847 | 2137 | `			if( SyisUpper(c) ){` |
| 169845 | 2138 | `				c = SyToLower(zString[0]);` |
|  84920 | 2139 | `			}` |
|      - | 2140 | `			/* Append character */` |
| 169847 | 2141 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2142 | `			/* Advance the cursor */` |
| 169847 | 2143 | `			zString++;` |
|      - | 2144 | `		}` |
|      5 | 2145 | `	}` |
|  32039 | 2146 | `	return PH7_OK;` |
|  16024 | 2147 | `}` |
|      - | 2148 | `/*` |
|      - | 2149 | ` * string strtolower(string $str)` |
|      - | 2150 | ` *  Make a string uppercase.` |
|      - | 2151 | ` * Parameters` |
|      - | 2152 | ` *  $str` |
|      - | 2153 | ` *   The input string.` |
|      - | 2154 | ` * Returns.` |
|      - | 2155 | ` *  The uppercased string.` |
|      - | 2156 | ` */` |
|     42 | 2157 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2158 | `{` |
|      - | 2159 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2160 | `	int nLen;` |
|     47 | 2161 | `	if( nArg < 1 ){` |
|      - | 2162 | `		/* Missing arguments,return null */` |
|      3 | 2163 | `		ph7_result_null(pCtx);` |
|      3 | 2164 | `		return PH7_OK;` |
|      - | 2165 | `	}` |
|      - | 2166 | `	/* Extract the target string */` |
|     45 | 2167 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     45 | 2168 | `	if( nLen < 1 ){` |
|      - | 2169 | `		/* Empty string,return */` |
|      3 | 2170 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2171 | `		return PH7_OK;` |
|      - | 2172 | `	}` |
|      - | 2173 | `	/* Perform the requested operation */` |
|     43 | 2174 | `	zEnd = &zString[nLen];` |
|     98 | 2175 | `	for(;;){` |
|    201 | 2176 | `		if( zString >= zEnd ){` |
|      - | 2177 | `			/* No more input,break immediately */` |
|     43 | 2178 | `			break;` |
|      - | 2179 | `		}` |
|    163 | 2180 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2181 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2182 | `			zCur = zString;` |
|    ! 0 | 2183 | `			zString++;` |
|    ! 0 | 2184 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2185 | `				zString++;` |
|    ! 0 | 2186 | `			}` |
|      - | 2187 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2188 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2189 | `		}else{` |
|    163 | 2190 | `			int c = zString[0];` |
|    163 | 2191 | `			if( SyisLower(c) ){` |
|    157 | 2192 | `				c = SyToUpper(zString[0]);` |
|     76 | 2193 | `			}` |
|      - | 2194 | `			/* Append character */` |
|    163 | 2195 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2196 | `			/* Advance the cursor */` |
|    163 | 2197 | `			zString++;` |
|      - | 2198 | `		}` |
|      5 | 2199 | `	}` |
|     43 | 2200 | `	return PH7_OK;` |
|     26 | 2201 | `}` |
|      - | 2202 | `/*` |
|      - | 2203 | ` * string ucfirst(string $str)` |
|      - | 2204 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2205 | ` *  character is alphabetic.` |
|      - | 2206 | ` * Parameters` |
|      - | 2207 | ` *  $str` |
|      - | 2208 | ` *   The input string.` |
|      - | 2209 | ` * Returns.` |
|      - | 2210 | ` *  The processed string.` |
|      - | 2211 | ` */` |
|      6 | 2212 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2213 | `{` |
|      - | 2214 | `	const char *zString,*zEnd;` |
|      - | 2215 | `	int nLen,c;` |
|      7 | 2216 | `	if( nArg < 1 ){` |
|      - | 2217 | `		/* Missing arguments,return null */` |
|      3 | 2218 | `		ph7_result_null(pCtx);` |
|      3 | 2219 | `		return PH7_OK;` |
|      - | 2220 | `	}` |
|      - | 2221 | `	/* Extract the target string */` |
|      5 | 2222 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2223 | `	if( nLen < 1 ){` |
|      - | 2224 | `		/* Empty string,return */` |
|      3 | 2225 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2226 | `		return PH7_OK;` |
|      - | 2227 | `	}` |
|      - | 2228 | `	/* Perform the requested operation */` |
|      3 | 2229 | `	zEnd = &zString[nLen];` |
|      3 | 2230 | `	c = zString[0];` |
|      3 | 2231 | `	if( SyisLower(c) ){` |
|      3 | 2232 | `		c = SyToUpper(c);` |
|      1 | 2233 | `	}` |
|      - | 2234 | `	/* Append the first character */` |
|      3 | 2235 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2236 | `	zString++;` |
|      3 | 2237 | `	if( zString < zEnd ){` |
|      - | 2238 | `		/* Append the rest of the input verbatim */` |
|      3 | 2239 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2240 | `	}` |
|      3 | 2241 | `	return PH7_OK;` |
|      4 | 2242 | `}` |
|      - | 2243 | `/*` |
|      - | 2244 | ` * string lcfirst(string $str)` |
|      - | 2245 | ` *  Make a string's first character lowercase.` |
|      - | 2246 | ` * Parameters` |
|      - | 2247 | ` *  $str` |
|      - | 2248 | ` *   The input string.` |
|      - | 2249 | ` * Returns.` |
|      - | 2250 | ` *  The processed string.` |
|      - | 2251 | ` */` |
|      6 | 2252 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2253 | `{` |
|      - | 2254 | `	const char *zString,*zEnd;` |
|      - | 2255 | `	int nLen,c;` |
|      7 | 2256 | `	if( nArg < 1 ){` |
|      - | 2257 | `		/* Missing arguments,return null */` |
|      3 | 2258 | `		ph7_result_null(pCtx);` |
|      3 | 2259 | `		return PH7_OK;` |
|      - | 2260 | `	}` |
|      - | 2261 | `	/* Extract the target string */` |
|      5 | 2262 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2263 | `	if( nLen < 1 ){` |
|      - | 2264 | `		/* Empty string,return */` |
|      3 | 2265 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2266 | `		return PH7_OK;` |
|      - | 2267 | `	}` |
|      - | 2268 | `	/* Perform the requested operation */` |
|      3 | 2269 | `	zEnd = &zString[nLen];` |
|      3 | 2270 | `	c = zString[0];` |
|      3 | 2271 | `	if( SyisUpper(c) ){` |
|      3 | 2272 | `		c = SyToLower(c);` |
|      1 | 2273 | `	}` |
|      - | 2274 | `	/* Append the first character */` |
|      3 | 2275 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2276 | `	zString++;` |
|      3 | 2277 | `	if( zString < zEnd ){` |
|      - | 2278 | `		/* Append the rest of the input verbatim */` |
|      3 | 2279 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2280 | `	}` |
|      3 | 2281 | `	return PH7_OK;` |
|      4 | 2282 | `}` |
|      - | 2283 | `/*` |
|      - | 2284 | ` * int ord(string $string)` |
|      - | 2285 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2286 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2287 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2288 | ` * Parameters` |
|      - | 2289 | ` *  $string` |
|      - | 2290 | ` *   The input string.` |
|      - | 2291 | ` * Returns` |
|      - | 2292 | ` *  The ASCII value as an integer.` |
|      - | 2293 | ` */` |
|     62 | 2294 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2295 | `{` |
|      - | 2296 | `	const char *zString;` |
|      - | 2297 | `	int nLen,c;` |
|      - | 2298 | `	/* PHP requires exactly one argument. */` |
|     65 | 2299 | `	if( nArg != 1 ){` |
|      8 | 2300 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2301 | `			"ArgumentCountError",` |
|      - | 2302 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2303 | `			nArg` |
|      - | 2304 | `			);` |
|      - | 2305 | `	}` |
|      - | 2306 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2307 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2308 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2309 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2310 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2311 | `			"of type string is deprecated"` |
|      - | 2312 | `			);` |
|      1 | 2313 | `	}` |
|      - | 2314 | `	/* Extract the target string */` |
|     59 | 2315 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2316 | `	if( nLen < 1 ){` |
|      - | 2317 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2318 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2319 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2320 | `			);` |
|      5 | 2321 | `		ph7_result_int(pCtx,0);` |
|      5 | 2322 | `		return PH7_OK;` |
|      - | 2323 | `	}` |
|      - | 2324 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2325 | `	if( nLen > 1 ){` |
|      7 | 2326 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2327 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2328 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2329 | `			);` |
|      3 | 2330 | `	}` |
|      - | 2331 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2332 | `	c = (unsigned char)zString[0];` |
|      - | 2333 | `	/* Return that value */` |
|     55 | 2334 | `	ph7_result_int(pCtx,c);` |
|     55 | 2335 | `	return PH7_OK;` |
|     34 | 2336 | `}` |
|      - | 2337 | `/*` |
|      - | 2338 | ` * string chr(int $codepoint)` |
|      - | 2339 | ` *  Returns a one-character string containing the character specified` |
|      - | 2340 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2341 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2342 | ` * Parameters` |
|      - | 2343 | ` *  $codepoint` |
|      - | 2344 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2345 | ` *   will be constrained to a single byte.` |
|      - | 2346 | ` * Returns` |
|      - | 2347 | ` *  A single-character string.` |
|      - | 2348 | ` */` |
|     48 | 2349 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2350 | `{` |
|      - | 2351 | `	int c;` |
|      - | 2352 | `	unsigned char ch;` |
|      - | 2353 | `	/* PHP requires exactly one argument. */` |
|     51 | 2354 | `	if( nArg != 1 ){` |
|      8 | 2355 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2356 | `			"ArgumentCountError",` |
|      - | 2357 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2358 | `			nArg` |
|      - | 2359 | `			);` |
|      - | 2360 | `	}` |
|      - | 2361 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2362 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2363 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2364 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     45 | 2365 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2366 | `		char zBuf[120];` |
|      4 | 2367 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2368 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2369 | `			ph7_value_to_double(apArg[0])` |
|      - | 2370 | `			);` |
|      3 | 2371 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2372 | `	}` |
|      - | 2373 | `	/* Extract the codepoint. */` |
|     45 | 2374 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2375 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2376 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2377 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2378 | `	 * name to avoid the API double-prefixing it. */` |
|     45 | 2379 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2380 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2381 | `			E_DEPRECATED,` |
|      - | 2382 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2383 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2384 | `			"The value used will be constrained using % 256"` |
|      - | 2385 | `			);` |
|      2 | 2386 | `	}` |
|      - | 2387 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2388 | `	 * when taking the address of a wider int. */` |
|     45 | 2389 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2390 | `	/* Return the specified character */` |
|     45 | 2391 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     45 | 2392 | `	return PH7_OK;` |
|     27 | 2393 | `}` |
|      - | 2394 | `/*` |
|      - | 2395 | ` * Binary to hex consumer callback.` |
|      - | 2396 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2397 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2398 | ` */` |
|   2330 | 2399 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2400 | `{` |
|      - | 2401 | `	/* Append hex chunk verbatim */` |
|   2331 | 2402 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   2331 | 2403 | `	return SXRET_OK;` |
|      1 | 2404 | `}` |
|      - | 2405 |  |
|      - | 2406 | `/*` |
|      - | 2407 | ` * string bin2hex(string $str)` |
|      - | 2408 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2409 | ` * Parameters` |
|      - | 2410 | ` *  $str` |
|      - | 2411 | ` *   The input string.` |
|      - | 2412 | ` * Returns.` |
|      - | 2413 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2414 | ` */` |
|     24 | 2415 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2416 | `{` |
|      - | 2417 | `	const char *zString;` |
|      - | 2418 | `	int nLen;` |
|      - | 2419 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|     29 | 2420 | `	if( nArg != 1 ){` |
|      8 | 2421 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2422 | `			"ArgumentCountError",` |
|      - | 2423 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2424 | `			nArg` |
|      - | 2425 | `			);` |
|      - | 2426 | `	}` |
|      - | 2427 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2428 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2429 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2430 | `	 */` |
|     33 | 2431 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     16 | 2432 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2433 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2434 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2435 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2436 | `		)` |
|      - | 2437 | `	){` |
|      9 | 2438 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2439 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2440 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2441 | `			if( pInst && pInst->pClass ){` |
|      3 | 2442 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2443 | `			}` |
|      1 | 2444 | `		}` |
|     12 | 2445 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2446 | `			"TypeError",` |
|      - | 2447 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2448 | `			zType` |
|      - | 2449 | `			);` |
|      - | 2450 | `	}` |
|      - | 2451 | `	/* Extract the target string */` |
|     15 | 2452 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 2453 | `	if( nLen < 1 ){` |
|      - | 2454 | `		/* Empty string,return */` |
|      3 | 2455 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2456 | `		return PH7_OK;` |
|      - | 2457 | `	}` |
|      - | 2458 | `	/* Perform the requested operation */` |
|     13 | 2459 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|     13 | 2460 | `	return PH7_OK;` |
|     17 | 2461 | `}` |
|      - | 2462 |  |
|      - | 2463 | `/* Search callback signature */` |
|      - | 2464 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2465 | `/*` |
|      - | 2466 | ` * Case-insensitive pattern match.` |
|      - | 2467 | ` * Brute force is the default search method used here.` |
|      - | 2468 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2469 | ` * well for short/medium texts on modern hardware.` |
|      - | 2470 | ` */` |
|    118 | 2471 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2472 | `{` |
|    119 | 2473 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2474 | `	const char *zIn = (const char *)pText;` |
|    119 | 2475 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2476 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2477 | `	const char *zPtr,*zPtr2;` |
|      - | 2478 | `	int c,d;` |
|    119 | 2479 | `	if( iPatLen > nLen ){` |
|      - | 2480 | `		/* Don't bother processing */` |
|     33 | 2481 | `		return SXERR_NOTFOUND;` |
|      - | 2482 | `	}` |
|    242 | 2483 | `	for(;;){` |
|    485 | 2484 | `		if( zIn >= zEnd ){` |
|     47 | 2485 | `			break;` |
|      - | 2486 | `		}` |
|    439 | 2487 | `		c = SyToLower(zIn[0]);` |
|    439 | 2488 | `		d = SyToLower(zpIn[0]);` |
|    439 | 2489 | `		if( c == d ){` |
|     41 | 2490 | `			zPtr   = &zIn[1];` |
|     41 | 2491 | `			zPtr2  = &zpIn[1];` |
|     71 | 2492 | `			for(;;){` |
|    143 | 2493 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2494 | `					/* Pattern found */` |
|     41 | 2495 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2496 | `					return SXRET_OK;` |
|      - | 2497 | `				}` |
|    103 | 2498 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2499 | `					break;` |
|      - | 2500 | `				}` |
|    103 | 2501 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2502 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2503 | `				if( c != d ){` |
|    ! 0 | 2504 | `					break;` |
|      - | 2505 | `				}` |
|    103 | 2506 | `				zPtr++; zPtr2++;` |
|      1 | 2507 | `			}` |
|    ! 0 | 2508 | `		}` |
|    399 | 2509 | `		zIn++;` |
|      1 | 2510 | `	}` |
|      - | 2511 | `	/* Pattern not found */` |
|     47 | 2512 | `	return SXERR_NOTFOUND;` |
|     60 | 2513 | `}` |
|      - | 2514 | `/*` |
|      - | 2515 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2516 | ` *  Find the first occurrence of a string.` |
|      - | 2517 | ` * Parameters` |
|      - | 2518 | ` *  $haystack` |
|      - | 2519 | ` *   The input string.` |
|      - | 2520 | ` * $needle` |
|      - | 2521 | ` *   Search pattern (must be a string).` |
|      - | 2522 | ` * $before_needle` |
|      - | 2523 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2524 | ` *   of the needle (excluding the needle).` |
|      - | 2525 | ` * Return` |
|      - | 2526 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2527 | ` */` |
|     10 | 2528 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2529 | `{` |
|     11 | 2530 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2531 | `	const char *zBlob,*zPattern;` |
|      - | 2532 | `	int nLen,nPatLen;` |
|      - | 2533 | `	sxu32 nOfft;` |
|      - | 2534 | `	sxi32 rc;` |
|     11 | 2535 | `	if( nArg < 2 ){` |
|      - | 2536 | `		/* Missing arguments,return FALSE */` |
|      5 | 2537 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2538 | `		return PH7_OK;` |
|      - | 2539 | `	}` |
|      - | 2540 | `	/* Extract the needle and the haystack */` |
|      7 | 2541 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2542 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2543 | `	nOfft = 0; /* cc warning */` |
|      9 | 2544 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2545 | `		int before = 0;` |
|      - | 2546 | `		/* Perform the lookup */` |
|      5 | 2547 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2548 | `		if( rc != SXRET_OK ){` |
|      - | 2549 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2550 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2551 | `			return PH7_OK;` |
|      - | 2552 | `		}` |
|      - | 2553 | `		/* Return the portion of the string */` |
|      5 | 2554 | `		if( nArg > 2 ){` |
|      3 | 2555 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2556 | `		}` |
|      5 | 2557 | `		if( before ){` |
|      3 | 2558 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2559 | `		}else{` |
|      3 | 2560 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2561 | `		}` |
|      3 | 2562 | `	}else{` |
|      3 | 2563 | `		ph7_result_bool(pCtx,0);` |
|      - | 2564 | `	}` |
|      7 | 2565 | `	return PH7_OK;` |
|      6 | 2566 | `}` |
|      - | 2567 | `/*` |
|      - | 2568 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2569 | ` *  Case-insensitive strstr().` |
|      - | 2570 | ` * Parameters` |
|      - | 2571 | ` *  $haystack` |
|      - | 2572 | ` *   The input string.` |
|      - | 2573 | ` * $needle` |
|      - | 2574 | ` *   Search pattern (must be a string).` |
|      - | 2575 | ` * $before_needle` |
|      - | 2576 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2577 | ` *   of the needle (excluding the needle).` |
|      - | 2578 | ` * Return` |
|      - | 2579 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2580 | ` */` |
|      6 | 2581 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2582 | `{` |
|      7 | 2583 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2584 | `	const char *zBlob,*zPattern;` |
|      - | 2585 | `	int nLen,nPatLen;` |
|      - | 2586 | `	sxu32 nOfft;` |
|      - | 2587 | `	sxi32 rc;` |
|      7 | 2588 | `	if( nArg < 2 ){` |
|      - | 2589 | `		/* Missing arguments,return FALSE */` |
|      3 | 2590 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2591 | `		return PH7_OK;` |
|      - | 2592 | `	}` |
|      - | 2593 | `	/* Extract the needle and the haystack */` |
|      5 | 2594 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2595 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2596 | `	nOfft = 0; /* cc warning */` |
|      7 | 2597 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2598 | `		int before = 0;` |
|      - | 2599 | `		/* Perform the lookup */` |
|      5 | 2600 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2601 | `		if( rc != SXRET_OK ){` |
|      - | 2602 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2603 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2604 | `			return PH7_OK;` |
|      - | 2605 | `		}` |
|      - | 2606 | `		/* Return the portion of the string */` |
|      5 | 2607 | `		if( nArg > 2 ){` |
|      3 | 2608 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2609 | `		}` |
|      5 | 2610 | `		if( before ){` |
|      3 | 2611 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2612 | `		}else{` |
|      3 | 2613 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2614 | `		}` |
|      3 | 2615 | `	}else{` |
|    ! 0 | 2616 | `		ph7_result_bool(pCtx,0);` |
|      - | 2617 | `	}` |
|      5 | 2618 | `	return PH7_OK;` |
|      4 | 2619 | `}` |
|      - | 2620 | `/*` |
|      - | 2621 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2622 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2623 | ` * Parameters` |
|      - | 2624 | ` *  $haystack` |
|      - | 2625 | ` *   The input string.` |
|      - | 2626 | ` * $needle` |
|      - | 2627 | ` *   Search pattern (must be a string).` |
|      - | 2628 | ` * $offset` |
|      - | 2629 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2630 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2631 | ` *   of haystack.` |
|      - | 2632 | ` * Return` |
|      - | 2633 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2634 | ` */` |
|    124 | 2635 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2636 | `{` |
|    129 | 2637 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2638 | `	const char *zBlob,*zPattern;` |
|      - | 2639 | `	int nLen,nPatLen,nStart;` |
|      - | 2640 | `	sxu32 nOfft;` |
|      - | 2641 | `	sxi32 rc;` |
|    129 | 2642 | `	if( nArg < 2 ){` |
|      - | 2643 | `		/* Missing arguments,return FALSE */` |
|      7 | 2644 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2645 | `		return PH7_OK;` |
|      - | 2646 | `	}` |
|      - | 2647 | `	/* Extract the needle and the haystack */` |
|    123 | 2648 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    123 | 2649 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    123 | 2650 | `	nOfft = 0; /* cc warning */` |
|    123 | 2651 | `	nStart = 0;` |
|      - | 2652 | `	/* Peek the starting offset if available */` |
|    123 | 2653 | `	if( nArg > 2 ){` |
|    ! 0 | 2654 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2655 | `		if( nStart < 0 ){` |
|    ! 0 | 2656 | `			nStart = -nStart;` |
|    ! 0 | 2657 | `		}` |
|    ! 0 | 2658 | `		if( nStart >= nLen ){` |
|      - | 2659 | `			/* Invalid offset */` |
|    ! 0 | 2660 | `			nStart = 0;` |
|    ! 0 | 2661 | `		}else{` |
|    ! 0 | 2662 | `			zBlob += nStart;` |
|    ! 0 | 2663 | `			nLen -= nStart;` |
|      - | 2664 | `		}` |
|    ! 0 | 2665 | `	}` |
|    123 | 2666 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2667 | `		/* Perform the lookup */` |
|    121 | 2668 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    121 | 2669 | `		if( rc != SXRET_OK ){` |
|      - | 2670 | `			/* Pattern not found,return FALSE */` |
|     33 | 2671 | `			ph7_result_bool(pCtx,0);` |
|     33 | 2672 | `			return PH7_OK;` |
|      - | 2673 | `		}` |
|      - | 2674 | `		/* Return the pattern position */` |
|     90 | 2675 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     46 | 2676 | `	}else{` |
|      3 | 2677 | `		ph7_result_bool(pCtx,0);` |
|      - | 2678 | `	}` |
|     92 | 2679 | `	return PH7_OK;` |
|     67 | 2680 | `}` |
|      - | 2681 | `/*` |
|      - | 2682 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2683 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2684 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2685 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2686 | ` *` |
|      - | 2687 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2688 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2689 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2690 | ` *` |
|      - | 2691 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2692 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2693 | ` */` |
|    418 | 2694 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2695 | `	ph7_context *pCtx,` |
|      - | 2696 | `	ph7_value *pArg,` |
|      - | 2697 | `	const char *zFunc,` |
|      - | 2698 | `	int iArgNum,` |
|      - | 2699 | `	const char *zParamName,` |
|      - | 2700 | `	const char *zNullMsg,` |
|      - | 2701 | `	ph7_value *pTmp,` |
|      - | 2702 | `	const char **pzOut,` |
|      - | 2703 | `	int *pnOut` |
|      4 | 2704 | `){` |
|    422 | 2705 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2706 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2707 | `		*pzOut = "";` |
|     13 | 2708 | `		*pnOut = 0;` |
|     13 | 2709 | `		return PH7_OK;` |
|      - | 2710 | `	}` |
|    628 | 2711 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    388 | 2712 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2713 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2714 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2715 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2716 | `	    )` |
|      - | 2717 | `	){` |
|     34 | 2718 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2719 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2720 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2721 | `			if( pInst && pInst->pClass ){` |
|     13 | 2722 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2723 | `			}` |
|      6 | 2724 | `		}` |
|     49 | 2725 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2726 | `			"TypeError",` |
|      - | 2727 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2728 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2729 | `			);` |
|      - | 2730 | `	}` |
|    377 | 2731 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2732 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2733 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2734 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2735 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2736 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2737 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2738 | `		return PH7_OK;` |
|      - | 2739 | `	}` |
|    341 | 2740 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    341 | 2741 | `	return PH7_OK;` |
|    213 | 2742 | `}` |
|      - | 2743 | `/*` |
|      - | 2744 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2745 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2746 | ` * Return` |
|      - | 2747 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2748 | ` */` |
|     92 | 2749 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2750 | `{` |
|      - | 2751 | `	const char *zHaystack,*zNeedle;` |
|      - | 2752 | `	int nHayLen,nNeedleLen;` |
|      - | 2753 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2754 | `	sxi32 rc;` |
|     96 | 2755 | `	if( nArg != 2 ){` |
|     18 | 2756 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2757 | `			"ArgumentCountError",` |
|      - | 2758 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2759 | `			nArg` |
|      - | 2760 | `			);` |
|      - | 2761 | `	}` |
|     84 | 2762 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     84 | 2763 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     84 | 2764 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2765 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2766 | `		"of type string is deprecated",` |
|      - | 2767 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     84 | 2768 | `	if( rc != PH7_OK ) goto out;` |
|     77 | 2769 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2770 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2771 | `		"of type string is deprecated",` |
|      - | 2772 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     77 | 2773 | `	if( rc != PH7_OK ) goto out;` |
|     73 | 2774 | `	if( nNeedleLen < 1 ){` |
|     13 | 2775 | `		ph7_result_bool(pCtx,1);` |
|     67 | 2776 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2777 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2778 | `	}else{` |
|     79 | 2779 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     26 | 2780 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     53 | 2781 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2782 | `	}` |
|     73 | 2783 | `	rc = PH7_OK;` |
|     41 | 2784 | `out:` |
|     84 | 2785 | `	PH7_MemObjRelease(&sHayTmp);` |
|     84 | 2786 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     84 | 2787 | `	return rc;` |
|     50 | 2788 | `}` |
|      - | 2789 | `/*` |
|      - | 2790 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2791 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2792 | ` * Return` |
|      - | 2793 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2794 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2795 | ` */` |
|     78 | 2796 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2797 | `{` |
|      - | 2798 | `	const char *zHaystack,*zNeedle;` |
|      - | 2799 | `	int nHayLen,nNeedleLen;` |
|      - | 2800 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2801 | `	sxi32 rc;` |
|     82 | 2802 | `	if( nArg != 2 ){` |
|     18 | 2803 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2804 | `			"ArgumentCountError",` |
|      - | 2805 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2806 | `			nArg` |
|      - | 2807 | `			);` |
|      - | 2808 | `	}` |
|     70 | 2809 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2810 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2811 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2812 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2813 | `		"of type string is deprecated",` |
|      - | 2814 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2815 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2816 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2817 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2818 | `		"of type string is deprecated",` |
|      - | 2819 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2820 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2821 | `	if( nNeedleLen < 1 ){` |
|     13 | 2822 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2823 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2824 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2825 | `	}else{` |
|     58 | 2826 | `		ph7_result_bool(pCtx,` |
|     38 | 2827 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2828 | `	}` |
|     59 | 2829 | `	rc = PH7_OK;` |
|     34 | 2830 | `out:` |
|     70 | 2831 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2832 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2833 | `	return rc;` |
|     43 | 2834 | `}` |
|      - | 2835 | `/*` |
|      - | 2836 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2837 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2838 | ` * Return` |
|      - | 2839 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2840 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2841 | ` */` |
|     78 | 2842 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2843 | `{` |
|      - | 2844 | `	const char *zHaystack,*zNeedle;` |
|      - | 2845 | `	int nHayLen,nNeedleLen;` |
|      - | 2846 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2847 | `	sxi32 rc;` |
|     82 | 2848 | `	if( nArg != 2 ){` |
|     18 | 2849 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2850 | `			"ArgumentCountError",` |
|      - | 2851 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2852 | `			nArg` |
|      - | 2853 | `			);` |
|      - | 2854 | `	}` |
|     70 | 2855 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2856 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2857 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2858 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2859 | `		"of type string is deprecated",` |
|      - | 2860 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2861 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2862 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2863 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2864 | `		"of type string is deprecated",` |
|      - | 2865 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2866 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2867 | `	if( nNeedleLen < 1 ){` |
|     13 | 2868 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2869 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2870 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2871 | `	}else{` |
|     58 | 2872 | `		ph7_result_bool(pCtx,` |
|     38 | 2873 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2874 | `	}` |
|     59 | 2875 | `	rc = PH7_OK;` |
|     34 | 2876 | `out:` |
|     70 | 2877 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2878 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2879 | `	return rc;` |
|     43 | 2880 | `}` |
|      - | 2881 | `/*` |
|      - | 2882 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2883 | ` *  Case-insensitive strpos.` |
|      - | 2884 | ` * Parameters` |
|      - | 2885 | ` *  $haystack` |
|      - | 2886 | ` *   The input string.` |
|      - | 2887 | ` * $needle` |
|      - | 2888 | ` *   Search pattern (must be a string).` |
|      - | 2889 | ` * $offset` |
|      - | 2890 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2891 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2892 | ` *   of haystack.` |
|      - | 2893 | ` * Return` |
|      - | 2894 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2895 | ` */` |
|     18 | 2896 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2897 | `{` |
|     19 | 2898 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2899 | `	const char *zBlob,*zPattern;` |
|      - | 2900 | `	int nLen,nPatLen,nStart;` |
|      - | 2901 | `	sxu32 nOfft;` |
|      - | 2902 | `	sxi32 rc;` |
|     19 | 2903 | `	if( nArg < 2 ){` |
|      - | 2904 | `		/* Missing arguments,return FALSE */` |
|      3 | 2905 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2906 | `		return PH7_OK;` |
|      - | 2907 | `	}` |
|      - | 2908 | `	/* Extract the needle and the haystack */` |
|     17 | 2909 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2910 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2911 | `	nOfft = 0; /* cc warning */` |
|     17 | 2912 | `	nStart = 0;` |
|      - | 2913 | `	/* Peek the starting offset if available */` |
|     17 | 2914 | `	if( nArg > 2 ){` |
|      5 | 2915 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2916 | `		if( nStart < 0 ){` |
|      3 | 2917 | `			nStart = -nStart;` |
|      1 | 2918 | `		}` |
|      5 | 2919 | `		if( nStart >= nLen ){` |
|      - | 2920 | `			/* Invalid offset */` |
|    ! 0 | 2921 | `			nStart = 0;` |
|    ! 0 | 2922 | `		}else{` |
|      5 | 2923 | `			zBlob += nStart;` |
|      5 | 2924 | `			nLen -= nStart;` |
|      - | 2925 | `		}` |
|      2 | 2926 | `	}` |
|     17 | 2927 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2928 | `		/* Perform the lookup */` |
|     17 | 2929 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2930 | `		if( rc != SXRET_OK ){` |
|      - | 2931 | `			/* Pattern not found,return FALSE */` |
|      3 | 2932 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2933 | `			return PH7_OK;` |
|      - | 2934 | `		}` |
|      - | 2935 | `		/* Return the pattern position */` |
|     15 | 2936 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2937 | `	}else{` |
|    ! 0 | 2938 | `		ph7_result_bool(pCtx,0);` |
|      - | 2939 | `	}` |
|     15 | 2940 | `	return PH7_OK;` |
|     10 | 2941 | `}` |
|      - | 2942 | `/*` |
|      - | 2943 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2944 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2945 | ` * Parameters` |
|      - | 2946 | ` *  $haystack` |
|      - | 2947 | ` *   The input string.` |
|      - | 2948 | ` * $needle` |
|      - | 2949 | ` *   Search pattern (must be a string).` |
|      - | 2950 | ` * $offset` |
|      - | 2951 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2952 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2953 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2954 | ` * Return` |
|      - | 2955 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2956 | ` */` |
|     32 | 2957 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2958 | `{` |
|      - | 2959 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2960 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2961 | `	int nLen,nPatLen;` |
|      - | 2962 | `	sxu32 nOfft;` |
|      - | 2963 | `	sxi32 rc;` |
|     33 | 2964 | `	if( nArg < 2 ){` |
|      - | 2965 | `		/* Missing arguments,return FALSE */` |
|      3 | 2966 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2967 | `		return PH7_OK;` |
|      - | 2968 | `	}` |
|      - | 2969 | `	/* Extract the needle and the haystack */` |
|     31 | 2970 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2971 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2972 | `	/* Point to the end of the pattern */` |
|     31 | 2973 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2974 | `	zEnd = &zBlob[nLen];` |
|      - | 2975 | `	/* Save the starting posistion */` |
|     31 | 2976 | `	zStart = zBlob;` |
|     31 | 2977 | `	nOfft = 0; /* cc warning */` |
|      - | 2978 | `	/* Peek the starting offset if available */` |
|     31 | 2979 | `	if( nArg > 2 ){` |
|      - | 2980 | `		int nStart;` |
|     21 | 2981 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2982 | `		if( nStart < 0 ){` |
|     11 | 2983 | `			nStart = -nStart;` |
|     11 | 2984 | `			if( nStart >= nLen ){` |
|      - | 2985 | `				/* Invalid offset */` |
|      3 | 2986 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2987 | `				return PH7_OK;` |
|    ! 0 | 2988 | `			}else{` |
|      9 | 2989 | `				nLen -= nStart;` |
|      9 | 2990 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2991 | `				zEnd = &zBlob[nLen];` |
|      - | 2992 | `			}` |
|      5 | 2993 | `		}else{` |
|     11 | 2994 | `			if( nStart >= nLen ){` |
|      - | 2995 | `				/* Invalid offset */` |
|      5 | 2996 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2997 | `				return PH7_OK;` |
|    ! 0 | 2998 | `			}else{` |
|      7 | 2999 | `				zBlob += nStart;` |
|      7 | 3000 | `				nLen -= nStart;` |
|      - | 3001 | `			}` |
|      - | 3002 | `		}` |
|      7 | 3003 | `	}` |
|     25 | 3004 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3005 | `		/* Perform the lookup */` |
|     57 | 3006 | `		for(;;){` |
|    115 | 3007 | `			if( zBlob >= zPtr ){` |
|     11 | 3008 | `				break;` |
|      - | 3009 | `			}` |
|    105 | 3010 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 3011 | `			if( rc == SXRET_OK ){` |
|      - | 3012 | `				/* Pattern found,return it's position */` |
|     13 | 3013 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3014 | `				return PH7_OK;` |
|      - | 3015 | `			}` |
|     93 | 3016 | `			zPtr--;` |
|      1 | 3017 | `		}` |
|      - | 3018 | `		/* Pattern not found,return FALSE */` |
|     11 | 3019 | `		ph7_result_bool(pCtx,0);` |
|      6 | 3020 | `	}else{` |
|      3 | 3021 | `		ph7_result_bool(pCtx,0);` |
|      - | 3022 | `	}` |
|     13 | 3023 | `	return PH7_OK;` |
|     17 | 3024 | `}` |
|      - | 3025 | `/*` |
|      - | 3026 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3027 | ` *  Case-insensitive strrpos.` |
|      - | 3028 | ` * Parameters` |
|      - | 3029 | ` *  $haystack` |
|      - | 3030 | ` *   The input string.` |
|      - | 3031 | ` * $needle` |
|      - | 3032 | ` *   Search pattern (must be a string).` |
|      - | 3033 | ` * $offset` |
|      - | 3034 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3035 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3036 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3037 | ` * Return` |
|      - | 3038 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3039 | ` */` |
|     28 | 3040 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3041 | `{` |
|      - | 3042 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 3043 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3044 | `	int nLen,nPatLen;` |
|      - | 3045 | `	sxu32 nOfft;` |
|      - | 3046 | `	sxi32 rc;` |
|     29 | 3047 | `	if( nArg < 2 ){` |
|      - | 3048 | `		/* Missing arguments,return FALSE */` |
|      3 | 3049 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3050 | `		return PH7_OK;` |
|      - | 3051 | `	}` |
|      - | 3052 | `	/* Extract the needle and the haystack */` |
|     27 | 3053 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3054 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3055 | `	/* Point to the end of the pattern */` |
|     27 | 3056 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3057 | `	zEnd = &zBlob[nLen];` |
|      - | 3058 | `	/* Save the starting posistion */` |
|     27 | 3059 | `	zStart = zBlob;` |
|     27 | 3060 | `	nOfft = 0; /* cc warning */` |
|      - | 3061 | `	/* Peek the starting offset if available */` |
|     27 | 3062 | `	if( nArg > 2 ){` |
|      - | 3063 | `		int nStart;` |
|     15 | 3064 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3065 | `		if( nStart < 0 ){` |
|      7 | 3066 | `			nStart = -nStart;` |
|      7 | 3067 | `			if( nStart >= nLen ){` |
|      - | 3068 | `				/* Invalid offset */` |
|      3 | 3069 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3070 | `				return PH7_OK;` |
|    ! 0 | 3071 | `			}else{` |
|      5 | 3072 | `				nLen -= nStart;` |
|      5 | 3073 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3074 | `				zEnd = &zBlob[nLen];` |
|      - | 3075 | `			}` |
|      3 | 3076 | `		}else{` |
|      9 | 3077 | `			if( nStart >= nLen ){` |
|      - | 3078 | `				/* Invalid offset */` |
|      5 | 3079 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3080 | `				return PH7_OK;` |
|    ! 0 | 3081 | `			}else{` |
|      5 | 3082 | `				zBlob += nStart;` |
|      5 | 3083 | `				nLen -= nStart;` |
|      - | 3084 | `			}` |
|      - | 3085 | `		}` |
|      4 | 3086 | `	}` |
|     21 | 3087 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3088 | `		/* Perform the lookup */` |
|     44 | 3089 | `		for(;;){` |
|     89 | 3090 | `			if( zBlob >= zPtr ){` |
|      9 | 3091 | `				break;` |
|      - | 3092 | `			}` |
|     81 | 3093 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3094 | `			if( rc == SXRET_OK ){` |
|      - | 3095 | `				/* Pattern found,return it's position */` |
|     11 | 3096 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3097 | `				return PH7_OK;` |
|      - | 3098 | `			}` |
|     71 | 3099 | `			zPtr--;` |
|      1 | 3100 | `		}` |
|      - | 3101 | `		/* Pattern not found,return FALSE */` |
|      9 | 3102 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3103 | `	}else{` |
|      3 | 3104 | `		ph7_result_bool(pCtx,0);` |
|      - | 3105 | `	}` |
|     11 | 3106 | `	return PH7_OK;` |
|     15 | 3107 | `}` |
|      - | 3108 | `/*` |
|      - | 3109 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3110 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3111 | ` * Parameters` |
|      - | 3112 | ` *  $haystack` |
|      - | 3113 | ` *   The input string.` |
|      - | 3114 | ` * $needle` |
|      - | 3115 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3116 | ` *  This behavior is different from that of strstr().` |
|      - | 3117 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3118 | ` *  as the ordinal value of a character.` |
|      - | 3119 | ` * Return` |
|      - | 3120 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3121 | ` */` |
|     24 | 3122 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3123 | `{` |
|      - | 3124 | `	const char *zBlob;` |
|      - | 3125 | `	int nLen,c;` |
|     25 | 3126 | `	if( nArg < 2 ){` |
|      - | 3127 | `		/* Missing arguments,return FALSE */` |
|      3 | 3128 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3129 | `		return PH7_OK;` |
|      - | 3130 | `	}` |
|      - | 3131 | `	/* Extract the haystack */` |
|     23 | 3132 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3133 | `	c = 0; /* cc warning */` |
|     23 | 3134 | `	if( nLen > 0 ){` |
|      - | 3135 | `		sxu32 nOfft;` |
|      - | 3136 | `		sxi32 rc;` |
|     21 | 3137 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3138 | `			const char *zPattern;` |
|     11 | 3139 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3140 | `														 * for NULL pointer.` |
|      - | 3141 | `														 */` |
|     11 | 3142 | `			c = zPattern[0];` |
|      6 | 3143 | `		}else{` |
|      - | 3144 | `			/* Int cast */` |
|     11 | 3145 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3146 | `		}` |
|      - | 3147 | `		/* Perform the lookup */` |
|     21 | 3148 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3149 | `		if( rc != SXRET_OK ){` |
|      - | 3150 | `			/* No such entry,return FALSE */` |
|      7 | 3151 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3152 | `			return PH7_OK;` |
|      - | 3153 | `		}` |
|      - | 3154 | `		/* Return the string portion */` |
|     15 | 3155 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3156 | `	}else{` |
|      3 | 3157 | `		ph7_result_bool(pCtx,0);` |
|      - | 3158 | `	}` |
|     17 | 3159 | `	return PH7_OK;` |
|     13 | 3160 | `}` |
|      - | 3161 | `/*` |
|      - | 3162 | ` * string strrev(string $string)` |
|      - | 3163 | ` *  Reverse a string.` |
|      - | 3164 | ` * Parameters` |
|      - | 3165 | ` *  $string` |
|      - | 3166 | ` *   String to be reversed.` |
|      - | 3167 | ` * Return` |
|      - | 3168 | ` *  The reversed string.` |
|      - | 3169 | ` */` |
|      4 | 3170 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3171 | `{` |
|      - | 3172 | `	const char *zIn,*zEnd;` |
|      - | 3173 | `	int nLen,c;` |
|      5 | 3174 | `	if( nArg < 1 ){` |
|      - | 3175 | `		/* Missing arguments,return NULL */` |
|      3 | 3176 | `		ph7_result_null(pCtx);` |
|      3 | 3177 | `		return PH7_OK;` |
|      - | 3178 | `	}` |
|      - | 3179 | `	/* Extract the target string */` |
|      3 | 3180 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3181 | `	if( nLen < 1 ){` |
|      - | 3182 | `		/* Empty string Return null */` |
|    ! 0 | 3183 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3184 | `		return PH7_OK;` |
|      - | 3185 | `	}` |
|      - | 3186 | `	/* Perform the requested operation */` |
|      3 | 3187 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3188 | `	for(;;){` |
|      9 | 3189 | `		if( zEnd < zIn ){` |
|      - | 3190 | `			/* No more input to process */` |
|      3 | 3191 | `			break;` |
|      - | 3192 | `		}` |
|      - | 3193 | `		/* Append current character */` |
|      7 | 3194 | `		c = zEnd[0];` |
|      7 | 3195 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3196 | `		zEnd--;` |
|      1 | 3197 | `	}` |
|      3 | 3198 | `	return PH7_OK;` |
|      3 | 3199 | `}` |
|      - | 3200 | `/*` |
|      - | 3201 | ` * string ucwords(string $string)` |
|      - | 3202 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3203 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 3204 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 3205 | ` * Parameters` |
|      - | 3206 | ` *  $string` |
|      - | 3207 | ` *   The input string.` |
|      - | 3208 | ` * Return` |
|      - | 3209 | ` *  The modified string..` |
|      - | 3210 | ` */` |
|     14 | 3211 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3212 | `{` |
|      - | 3213 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 3214 | `	int nLen,c;` |
|     15 | 3215 | `	if( nArg < 1 ){` |
|      - | 3216 | `		/* Missing arguments,return NULL */` |
|      3 | 3217 | `		ph7_result_null(pCtx);` |
|      3 | 3218 | `		return PH7_OK;` |
|      - | 3219 | `	}` |
|      - | 3220 | `	/* Extract the target string */` |
|     13 | 3221 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3222 | `	if( nLen < 1 ){` |
|      - | 3223 | `		/* Empty string – match PHP semantics */` |
|      3 | 3224 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3225 | `		return PH7_OK;` |
|      - | 3226 | `	}` |
|      - | 3227 | `	/* Perform the requested operation */` |
|     11 | 3228 | `	zEnd = &zIn[nLen];` |
|     21 | 3229 | `	for(;;){` |
|      - | 3230 | `		/* Jump leading white spaces */` |
|     43 | 3231 | `		zCur = zIn;` |
|     65 | 3232 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3233 | `			zIn++;` |
|      1 | 3234 | `		}` |
|     43 | 3235 | `		if( zCur < zIn ){` |
|      - | 3236 | `			/* Append white space stream */` |
|     23 | 3237 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3238 | `		}` |
|     43 | 3239 | `		if( zIn >= zEnd ){` |
|      - | 3240 | `			/* No more input to process */` |
|     11 | 3241 | `			break;` |
|      - | 3242 | `		}` |
|     33 | 3243 | `		c = zIn[0];` |
|     33 | 3244 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3245 | `			c = SyToUpper(c);` |
|     14 | 3246 | `		}` |
|      - | 3247 | `		/* Append the upper-cased character */` |
|     33 | 3248 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3249 | `		zIn++;` |
|     33 | 3250 | `		zCur = zIn;` |
|      - | 3251 | `		/* Append the word varbatim */` |
|    149 | 3252 | `		while( zIn < zEnd ){` |
|    139 | 3253 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3254 | `				/* UTF-8 stream */` |
|    ! 0 | 3255 | `				zIn++;` |
|    ! 0 | 3256 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3257 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3258 | `				zIn++;` |
|     59 | 3259 | `			}else{` |
|     23 | 3260 | `				break;` |
|      - | 3261 | `			}` |
|      1 | 3262 | `		}` |
|     33 | 3263 | `		if( zCur < zIn ){` |
|     33 | 3264 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3265 | `		}` |
|      1 | 3266 | `	}` |
|     11 | 3267 | `	return PH7_OK;` |
|      8 | 3268 | `}` |
|      - | 3269 | `/*` |
|      - | 3270 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3271 | ` *  Returns input repeated multiplier times.` |
|      - | 3272 | ` * Parameters` |
|      - | 3273 | ` *  $string` |
|      - | 3274 | ` *   String to be repeated.` |
|      - | 3275 | ` * $multiplier` |
|      - | 3276 | ` *  Number of time the input string should be repeated.` |
|      - | 3277 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3278 | ` *  to 0, the function will return an empty string.` |
|      - | 3279 | ` * Return` |
|      - | 3280 | ` *  The repeated string.` |
|      - | 3281 | ` */` |
|  20226 | 3282 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3283 | `{` |
|      - | 3284 | `	const char *zIn;` |
|      - | 3285 | `	int nLen,nMul;` |
|      - | 3286 | `	int rc;` |
|  20227 | 3287 | `	if( nArg < 2 ){` |
|      - | 3288 | `		/* Missing arguments,return NULL */` |
|      3 | 3289 | `		ph7_result_null(pCtx);` |
|      3 | 3290 | `		return PH7_OK;` |
|      - | 3291 | `	}` |
|      - | 3292 | `	/* Extract the target string */` |
|  20225 | 3293 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20225 | 3294 | `	if( nLen < 1 ){` |
|      - | 3295 | `		/* Empty string.Return null */` |
|    ! 0 | 3296 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3297 | `		return PH7_OK;` |
|      - | 3298 | `	}` |
|      - | 3299 | `	/* Extract the multiplier */` |
|  20225 | 3300 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20225 | 3301 | `	if( nMul < 1 ){` |
|      - | 3302 | `		/* Return the empty string */` |
|      3 | 3303 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3304 | `		return PH7_OK;` |
|      - | 3305 | `	}` |
|      - | 3306 | `	/* Perform the requested operation */` |
| 120878 | 3307 | `	for(;;){` |
| 241757 | 3308 | `		if( !nMul ){` |
|  20223 | 3309 | `			break;` |
|      - | 3310 | `		}` |
|      - | 3311 | `		/* Append the copy */` |
| 221535 | 3312 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 221535 | 3313 | `		if( rc != PH7_OK ){` |
|      - | 3314 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3315 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3316 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3317 | `		}` |
| 221535 | 3318 | `		nMul--;` |
|      1 | 3319 | `	}` |
|  20223 | 3320 | `	return PH7_OK;` |
|  10114 | 3321 | `}` |
|      - | 3322 | `/*` |
|      - | 3323 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3324 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3325 | ` * Parameters` |
|      - | 3326 | ` *  $string` |
|      - | 3327 | ` *   The input string.` |
|      - | 3328 | ` * $is_xhtml` |
|      - | 3329 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3330 | ` * Return` |
|      - | 3331 | ` *  The processed string.` |
|      - | 3332 | ` */` |
|      6 | 3333 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3334 | `{` |
|      - | 3335 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3336 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3337 | `	int nLen;` |
|      7 | 3338 | `	if( nArg < 1 ){` |
|      - | 3339 | `		/* Missing arguments,return the empty string */` |
|      3 | 3340 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3341 | `		return PH7_OK;` |
|      - | 3342 | `	}` |
|      - | 3343 | `	/* Extract the target string */` |
|      5 | 3344 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3345 | `	if( nLen < 1 ){` |
|      - | 3346 | `		/* Empty string,return null */` |
|    ! 0 | 3347 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3348 | `		return PH7_OK;` |
|      - | 3349 | `	}` |
|      5 | 3350 | `	if( nArg > 1 ){` |
|      3 | 3351 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3352 | `	}` |
|      5 | 3353 | `	zEnd = &zIn[nLen];` |
|      - | 3354 | `	/* Perform the requested operation */` |
|      4 | 3355 | `	for(;;){` |
|      9 | 3356 | `		zCur = zIn;` |
|      - | 3357 | `		/* Delimit the string */` |
|     21 | 3358 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3359 | `			zIn++;` |
|      1 | 3360 | `		}` |
|      9 | 3361 | `		if( zCur < zIn ){` |
|      - | 3362 | `			/* Output chunk verbatim */` |
|      9 | 3363 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3364 | `		}` |
|      9 | 3365 | `		if( zIn >= zEnd ){` |
|      - | 3366 | `			/* No more input to process */` |
|      5 | 3367 | `			break;` |
|      - | 3368 | `		}` |
|      - | 3369 | `		/* Output the HTML line break */` |
|      - | 3370 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3371 | `		if( is_xhtml ){` |
|      3 | 3372 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3373 | `		}else{` |
|      3 | 3374 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3375 | `		}` |
|      5 | 3376 | `		zCur = zIn;` |
|      - | 3377 | `		/* Append trailing line */` |
|     11 | 3378 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3379 | `			zIn++;` |
|      1 | 3380 | `		}` |
|      5 | 3381 | `		if( zCur < zIn ){` |
|      - | 3382 | `			/* Output chunk verbatim */` |
|      5 | 3383 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3384 | `		}` |
|      1 | 3385 | `	}` |
|      5 | 3386 | `	return PH7_OK;` |
|      4 | 3387 | `}` |
|      - | 3388 | `/*` |
|      - | 3389 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3390 | ` *  According to the PHP reference manual.` |
|      - | 3391 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3392 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3393 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3394 | ` * This applies to both sprintf() and printf().` |
|      - | 3395 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3396 | ` * or more of these elements, in order:` |
|      - | 3397 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3398 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3399 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3400 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3401 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3402 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3403 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3404 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3405 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3406 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3407 | ` *   should result in.` |
|      - | 3408 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3409 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3410 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3411 | ` *   limit to the string.` |
|      - | 3412 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3413 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3414 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3415 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3416 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3417 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3418 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3419 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3420 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3421 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3422 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3423 | ` *       g - shorter of %e and %f.` |
|      - | 3424 | ` *       G - shorter of %E and %f.` |
|      - | 3425 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3426 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3427 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3428 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3429 | ` */` |
|      - | 3430 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3431 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3432 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3433 | `/*` |
|      - | 3434 | `** Conversion types fall into various categories as defined by the` |
|      - | 3435 | `** following enumeration.` |
|      - | 3436 | `*/` |
|      - | 3437 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3438 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3439 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3440 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3441 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3442 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3443 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3444 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3445 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3446 |  |
|      - | 3447 | `/*` |
|      - | 3448 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3449 | `*/` |
|      - | 3450 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3451 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3452 | `/*` |
|      - | 3453 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3454 | `** by an instance of the following structure` |
|      - | 3455 | `*/` |
|      - | 3456 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3457 | `struct ph7_fmt_info` |
|      - | 3458 | `{` |
|      - | 3459 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3460 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3461 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3462 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3463 | `  char *charset; /* The character set for conversion */` |
|      - | 3464 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3465 | `};` |
|      - | 3466 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3467 | `/*` |
|      - | 3468 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3469 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3470 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3471 | `**` |
|      - | 3472 | `** Example:` |
|      - | 3473 | `**     input:     *val = 3.14159` |
|      - | 3474 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3475 | `**` |
|      - | 3476 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3477 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3478 | `** always returned.` |
|      - | 3479 | `*/` |
|    422 | 3480 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3481 | `{` |
|      - | 3482 | `  sxlongreal d;` |
|      - | 3483 | `  int digit;` |
|      - | 3484 |  |
|    423 | 3485 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3486 | `	  return '0';` |
|      - | 3487 | `  }` |
|    423 | 3488 | `  digit = (int)*val;` |
|    423 | 3489 | `  d = digit;` |
|    423 | 3490 | `   *val = (*val - d)*10.0;` |
|    423 | 3491 | `  return digit + '0' ;` |
|    212 | 3492 | `}` |
|      - | 3493 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3494 | `/*` |
|      - | 3495 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3496 | ` * used conversion types first.` |
|      - | 3497 | ` */` |
|      - | 3498 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3499 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3500 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3501 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3502 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3503 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3504 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3505 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3506 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3507 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3508 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3509 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3510 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3511 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3512 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3513 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3514 | `};` |
|      - | 3515 | `/*` |
|      - | 3516 | ` * Format a given string.` |
|      - | 3517 | ` * The root program.  All variations call this core.` |
|      - | 3518 | ` * INPUTS:` |
|      - | 3519 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3520 | ` *            1. A pointer to the call context.` |
|      - | 3521 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3522 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3523 | ` *            3. An integer number of characters to be output.` |
|      - | 3524 | ` *               (Note: This number might be zero.)` |
|      - | 3525 | ` *            4. Upper layer private data.` |
|      - | 3526 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3527 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3528 | ` */` |
|    260 | 3529 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3530 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3531 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3532 | `	const char *zIn,    /* Format string */` |
|      - | 3533 | `	int nByte,          /* Format string length */` |
|      - | 3534 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3535 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3536 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3537 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3538 | `	)` |
|      1 | 3539 | `{` |
|    261 | 3540 | `	char spaces[] = "                                                  ";` |
|      - | 3541 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    261 | 3542 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3543 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3544 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3545 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3546 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3547 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3548 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3549 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3550 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3551 | `	ph7_int64 iVal;` |
|      - | 3552 | `	int precision;           /* Precision of the current field */` |
|      - | 3553 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3554 | `	int c,rc,n;` |
|      - | 3555 | `	int length;              /* Length of the field */` |
|      - | 3556 | `	int prefix;` |
|      - | 3557 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3558 | `	int width;               /* Width of the current field */` |
|      - | 3559 | `	int idx;` |
|    261 | 3560 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3561 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3562 | `	/* Start the format process */` |
|    380 | 3563 | `	for(;;){` |
|    761 | 3564 | `		zCur = zIn;` |
|   2785 | 3565 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2025 | 3566 | `			zIn++;` |
|      1 | 3567 | `		}` |
|    761 | 3568 | `		if( zCur < zIn ){` |
|      - | 3569 | `			/* Consume chunk verbatim */` |
|    539 | 3570 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    539 | 3571 | `			if( rc != SXRET_OK ){` |
|      - | 3572 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3573 | `				break;` |
|      - | 3574 | `			}` |
|    269 | 3575 | `		}` |
|    761 | 3576 | `		if( zIn >= zEnd ){` |
|      - | 3577 | `			/* No more input to process,break immediately */` |
|    259 | 3578 | `			break;` |
|      - | 3579 | `		}` |
|      - | 3580 | `		/* Find out what flags are present */` |
|    503 | 3581 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    502 | 3582 | `			flag_alternateform = flag_zeropad = 0;` |
|    503 | 3583 | `		zIn++; /* Jump the precent sign */` |
|    251 | 3584 | `		do{` |
|    535 | 3585 | `			c = zIn[0];` |
|    535 | 3586 | `			switch( c ){` |
|      9 | 3587 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3588 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3589 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3590 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      9 | 3591 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3592 | `			case '\'':` |
|    ! 0 | 3593 | `				zIn++;` |
|    ! 0 | 3594 | `				if( zIn < zEnd ){` |
|      - | 3595 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3596 | `					c = zIn[0];` |
|    ! 0 | 3597 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3598 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3599 | `					}` |
|    ! 0 | 3600 | `					c = 0;` |
|    ! 0 | 3601 | `				}` |
|    ! 0 | 3602 | `				break;` |
|    502 | 3603 | `			default:                                       break;` |
|      - | 3604 | `			}` |
|    535 | 3605 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3606 | `		/* Get the field width */` |
|    503 | 3607 | `		width = 0;` |
|    788 | 3608 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     35 | 3609 | `			width = width*10 + (zIn[0] - '0');` |
|     35 | 3610 | `			zIn++;` |
|      1 | 3611 | `		}` |
|    503 | 3612 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3613 | `			/* Position specifer */` |
|    ! 0 | 3614 | `			if( width > 0 ){` |
|    ! 0 | 3615 | `				n = width;` |
|    ! 0 | 3616 | `				if( vf && n > 0 ){` |
|    ! 0 | 3617 | `					n--;` |
|    ! 0 | 3618 | `				}` |
|    ! 0 | 3619 | `			}` |
|    ! 0 | 3620 | `			zIn++;` |
|    ! 0 | 3621 | `			width = 0;` |
|    ! 0 | 3622 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3623 | `				flag_zeropad = 1;` |
|    ! 0 | 3624 | `				zIn++;` |
|    ! 0 | 3625 | `			}` |
|    ! 0 | 3626 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3627 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3628 | `				zIn++;` |
|    ! 0 | 3629 | `			}` |
|    ! 0 | 3630 | `		}` |
|    503 | 3631 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3632 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3633 | `		}` |
|      - | 3634 | `		/* Get the precision */` |
|    503 | 3635 | `		precision = -1;` |
|    503 | 3636 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     59 | 3637 | `			precision = 0;` |
|     59 | 3638 | `			zIn++;` |
|    150 | 3639 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     63 | 3640 | `				precision = precision*10 + (zIn[0] - '0');` |
|     63 | 3641 | `				zIn++;` |
|      1 | 3642 | `			}` |
|     29 | 3643 | `		}` |
|    503 | 3644 | `		if( zIn >= zEnd ){` |
|      - | 3645 | `			/* No more input */` |
|      3 | 3646 | `			break;` |
|      - | 3647 | `		}` |
|      - | 3648 | `		/* Fetch the info entry for the field */` |
|    501 | 3649 | `		pInfo = 0;` |
|    501 | 3650 | `		xtype = PH7_FMT_ERROR;` |
|    501 | 3651 | `		c = zIn[0];` |
|    501 | 3652 | `		zIn++; /* Jump the format specifer */` |
|   1439 | 3653 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   1437 | 3654 | `			if( c==aFmt[idx].fmttype ){` |
|    499 | 3655 | `				pInfo = &aFmt[idx];` |
|    499 | 3656 | `				xtype = pInfo->type;` |
|    499 | 3657 | `				break;` |
|      - | 3658 | `			}` |
|    470 | 3659 | `		}` |
|    501 | 3660 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    501 | 3661 | `		length = 0;` |
|      - | 3662 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3663 | `		 /*` |
|      - | 3664 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3665 | `		  **` |
|      - | 3666 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3667 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3668 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3669 | `		  **                               field width was negative.` |
|      - | 3670 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3671 | `		  **                               the conversion character.` |
|      - | 3672 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3673 | `		  **   width                       The specified field width.  This is` |
|      - | 3674 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3675 | `		  **   precision                   The specified precision.  The default` |
|      - | 3676 | `		  **                               is -1.` |
|      - | 3677 | `		  */` |
|    501 | 3678 | `		switch(xtype){` |
|    ! 0 | 3679 | `		case PH7_FMT_PERCENT:` |
|      - | 3680 | `			/* A literal percent character */` |
|    ! 0 | 3681 | `			zWorker[0] = '%';` |
|    ! 0 | 3682 | `			length = (int)sizeof(char);` |
|    ! 0 | 3683 | `			break;` |
|      3 | 3684 | `		case PH7_FMT_CHARX:` |
|      - | 3685 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3686 | `			 * with that ASCII value` |
|      - | 3687 | `			 */` |
|      7 | 3688 | `			pArg = NEXT_ARG;` |
|      7 | 3689 | `			if( pArg == 0 ){` |
|      3 | 3690 | `				c = 0;` |
|      2 | 3691 | `			}else{` |
|      5 | 3692 | `				c = ph7_value_to_int(pArg);` |
|      - | 3693 | `			}` |
|      - | 3694 | `			/* NUL byte is an acceptable value */` |
|      7 | 3695 | `			zWorker[0] = (char)c;` |
|      7 | 3696 | `			length = (int)sizeof(char);` |
|      7 | 3697 | `			break;` |
|    159 | 3698 | `		case PH7_FMT_STRING:` |
|      - | 3699 | `			/* the argument is treated as and presented as a string */` |
|    319 | 3700 | `			pArg = NEXT_ARG;` |
|    319 | 3701 | `			if( pArg == 0 ){` |
|    ! 0 | 3702 | `				length = 0;` |
|    ! 0 | 3703 | `			}else{` |
|    319 | 3704 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3705 | `			}` |
|    319 | 3706 | `			if( length < 1 ){` |
|    ! 0 | 3707 | `				zBuf = " ";` |
|    ! 0 | 3708 | `				length = (int)sizeof(char);` |
|    ! 0 | 3709 | `			}` |
|    319 | 3710 | `			if( precision>=0 && precision<length ){` |
|      3 | 3711 | `				length = precision;` |
|      1 | 3712 | `			}` |
|    319 | 3713 | `			if( flag_zeropad ){` |
|      - | 3714 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3715 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3716 | `					spaces[idx] = '0';` |
|    ! 0 | 3717 | `				}` |
|    ! 0 | 3718 | `			}` |
|    319 | 3719 | `			break;` |
|     59 | 3720 | `		case PH7_FMT_RADIX:` |
|    119 | 3721 | `			pArg = NEXT_ARG;` |
|    119 | 3722 | `			if( pArg == 0 ){` |
|    ! 0 | 3723 | `				iVal = 0;` |
|    ! 0 | 3724 | `			}else{` |
|    119 | 3725 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3726 | `			}` |
|      - | 3727 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 3728 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3729 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3730 | `			}` |
|      - | 3731 | `#if 1` |
|      - | 3732 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3733 | `        ** I think this is stupid.*/` |
|    119 | 3734 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3735 | `#else` |
|      - | 3736 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3737 | `        ** but leave the prefix for hex.*/` |
|      - | 3738 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3739 | `#endif` |
|    119 | 3740 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     89 | 3741 | `          if( iVal<0 ){` |
|     25 | 3742 | `            iVal = -iVal;` |
|      - | 3743 | `			/* Ticket 1433-003 */` |
|     25 | 3744 | `			if( iVal < 0 ){` |
|      - | 3745 | `				/* Overflow */` |
|    ! 0 | 3746 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3747 | `			}` |
|     25 | 3748 | `            prefix = '-';` |
|     77 | 3749 | `          }else if( flag_plussign )  prefix = '+';` |
|     63 | 3750 | `          else if( flag_blanksign )  prefix = ' ';` |
|     61 | 3751 | `          else                       prefix = 0;` |
|     45 | 3752 | `        }else{` |
|     31 | 3753 | `			if( iVal<0 ){` |
|    ! 0 | 3754 | `				iVal = -iVal;` |
|      - | 3755 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3756 | `				if( iVal < 0 ){` |
|      - | 3757 | `					/* Overflow */` |
|    ! 0 | 3758 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3759 | `				}` |
|    ! 0 | 3760 | `			}` |
|     31 | 3761 | `			prefix = 0;` |
|      - | 3762 | `		}` |
|    119 | 3763 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3764 | `          precision = width-(prefix!=0);` |
|      3 | 3765 | `        }` |
|    119 | 3766 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3767 | `        {` |
|      - | 3768 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3769 | `          register int base;` |
|    119 | 3770 | `          cset = pInfo->charset;` |
|    119 | 3771 | `          base = pInfo->base;` |
|     59 | 3772 | `          do{                                           /* Convert to ascii */` |
|    187 | 3773 | `            *(--zBuf) = cset[iVal%base];` |
|    187 | 3774 | `            iVal = iVal/base;` |
|    187 | 3775 | `          }while( iVal>0 );` |
|      - | 3776 | `        }` |
|    119 | 3777 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 3778 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3779 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3780 | `        }` |
|    119 | 3781 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 3782 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3783 | `          char *pre, x;` |
|      9 | 3784 | `          pre = pInfo->prefix;` |
|      9 | 3785 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3786 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3787 | `          }` |
|      4 | 3788 | `        }` |
|    119 | 3789 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 3790 | `		break;` |
|     28 | 3791 | `		case PH7_FMT_FLOAT:` |
|      - | 3792 | `		case PH7_FMT_EXP:` |
|      - | 3793 | `		case PH7_FMT_GENERIC:{` |
|      - | 3794 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3795 | `		long double realvalue;` |
|      - | 3796 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3797 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3798 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3799 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3800 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3801 | `		int nsd;                 /* Number of significant digits returned */` |
|     57 | 3802 | `		pArg = NEXT_ARG;` |
|     57 | 3803 | `		if( pArg == 0 ){` |
|    ! 0 | 3804 | `			realvalue = 0;` |
|    ! 0 | 3805 | `		}else{` |
|     57 | 3806 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3807 | `		}` |
|      - | 3808 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3809 | `		 * below assumes a finite positive realvalue. */` |
|     57 | 3810 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3811 | `			zBuf = "NAN";` |
|    ! 0 | 3812 | `			length = 3;` |
|    ! 0 | 3813 | `			break;` |
|      - | 3814 | `		}` |
|     57 | 3815 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3816 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3817 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3818 | `				zBuf = "-INF";` |
|    ! 0 | 3819 | `				length = 4;` |
|    ! 0 | 3820 | `			}else{` |
|    ! 0 | 3821 | `				zBuf = "INF";` |
|    ! 0 | 3822 | `				length = 3;` |
|      - | 3823 | `			}` |
|    ! 0 | 3824 | `			break;` |
|      - | 3825 | `		}` |
|     57 | 3826 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     57 | 3827 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     57 | 3828 | `        if( realvalue<0.0 ){` |
|      3 | 3829 | `          realvalue = -realvalue;` |
|      3 | 3830 | `          prefix = '-';` |
|      2 | 3831 | `        }else{` |
|     55 | 3832 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3833 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3834 | `          else                         prefix = 0;` |
|      - | 3835 | `        }` |
|     57 | 3836 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     57 | 3837 | `        rounder = 0.0;` |
|      - | 3838 | `#if 0` |
|      - | 3839 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3840 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3841 | `#else` |
|      - | 3842 | `        /* It makes more sense to use 0.5 */` |
|    405 | 3843 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3844 | `#endif` |
|     57 | 3845 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3846 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     57 | 3847 | `        exp = 0;` |
|     57 | 3848 | `        if( realvalue>0.0 ){` |
|     61 | 3849 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     89 | 3850 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     61 | 3851 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     71 | 3852 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     57 | 3853 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3854 | `            zBuf = "NaN";` |
|    ! 0 | 3855 | `            length = 3;` |
|    ! 0 | 3856 | `            break;` |
|      - | 3857 | `          }` |
|     28 | 3858 | `        }` |
|     57 | 3859 | `        zBuf = zWorker;` |
|      - | 3860 | `        /*` |
|      - | 3861 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3862 | `        ** or etFLOAT, as appropriate.` |
|      - | 3863 | `        */` |
|     57 | 3864 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     57 | 3865 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3866 | `          realvalue += rounder;` |
|    ! 0 | 3867 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3868 | `        }` |
|     57 | 3869 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3870 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3871 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3872 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3873 | `          }else{` |
|    ! 0 | 3874 | `            precision = precision - exp;` |
|    ! 0 | 3875 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3876 | `          }` |
|    ! 0 | 3877 | `        }else{` |
|     57 | 3878 | `          flag_rtz = 0;` |
|      - | 3879 | `        }` |
|      - | 3880 | `        /*` |
|      - | 3881 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3882 | `        ** the precision is too large to fit in buf[].` |
|      - | 3883 | `        */` |
|     57 | 3884 | `        nsd = 0;` |
|     57 | 3885 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     57 | 3886 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     57 | 3887 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     57 | 3888 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    149 | 3889 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3890 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     89 | 3891 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3892 | `            *(zBuf++) = '0';` |
|     17 | 3893 | `          }` |
|    373 | 3894 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     57 | 3895 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     57 | 3896 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3897 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3898 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3899 | `          }` |
|     57 | 3900 | `          zBuf++;                            /* point to next free slot */` |
|     29 | 3901 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3902 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3903 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3904 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3905 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3906 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3907 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3908 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3909 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3910 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3911 | `          }` |
|    ! 0 | 3912 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3913 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3914 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3915 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3916 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3917 | `            if( exp>=100 ){` |
|    ! 0 | 3918 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3919 | `              exp %= 100;` |
|    ! 0 | 3920 | `            }` |
|    ! 0 | 3921 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3922 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3923 | `          }` |
|      - | 3924 | `        }` |
|      - | 3925 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3926 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3927 | `        ** integer conversions.*/` |
|     57 | 3928 | `        length = (int)(zBuf-zWorker);` |
|     57 | 3929 | `        zBuf = zWorker;` |
|      - | 3930 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3931 | `        ** set and we are not left justified */` |
|     57 | 3932 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3933 | `          int i;` |
|      3 | 3934 | `          int nPad = width - length;` |
|     13 | 3935 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3936 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3937 | `          }` |
|      3 | 3938 | `          i = prefix!=0;` |
|      5 | 3939 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3940 | `          length = width;` |
|      1 | 3941 | `        }` |
|      - | 3942 | `#else` |
|      - | 3943 | `         zBuf = " ";` |
|      - | 3944 | `		 length = (int)sizeof(char);` |
|      - | 3945 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     57 | 3946 | `		 break;` |
|      - | 3947 | `							 }` |
|      1 | 3948 | `		default:` |
|      - | 3949 | `			/* Invalid format specifer */` |
|      3 | 3950 | `			zWorker[0] = '?';` |
|      3 | 3951 | `			length = (int)sizeof(char);` |
|      2 | 3952 | `			break;` |
|      - | 3953 | `		}` |
|      - | 3954 | `		 /*` |
|      - | 3955 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3956 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3957 | `		 ** the output.` |
|      - | 3958 | `		 */` |
|    501 | 3959 | `    if( !flag_leftjustify ){` |
|      - | 3960 | `      register int nspace;` |
|    493 | 3961 | `      nspace = width-length;` |
|    493 | 3962 | `      if( nspace>0 ){` |
|      5 | 3963 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3964 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3965 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3966 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3967 | `			}` |
|    ! 0 | 3968 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3969 | `        }` |
|      5 | 3970 | `        if( nspace>0 ){` |
|      5 | 3971 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3972 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3973 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3974 | `			}` |
|      2 | 3975 | `		}` |
|      2 | 3976 | `      }` |
|    246 | 3977 | `    }` |
|    501 | 3978 | `    if( length>0 ){` |
|    501 | 3979 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    501 | 3980 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3981 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3982 | `		}` |
|    250 | 3983 | `    }` |
|    501 | 3984 | `    if( flag_leftjustify ){` |
|      - | 3985 | `      register int nspace;` |
|      9 | 3986 | `      nspace = width-length;` |
|      9 | 3987 | `      if( nspace>0 ){` |
|      9 | 3988 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3989 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3990 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3991 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3992 | `			}` |
|    ! 0 | 3993 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3994 | `        }` |
|      9 | 3995 | `        if( nspace>0 ){` |
|      9 | 3996 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3997 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3998 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3999 | `			}` |
|      4 | 4000 | `		}` |
|      4 | 4001 | `      }` |
|      4 | 4002 | `    }` |
|      1 | 4003 | ` }/* for(;;) */` |
|    261 | 4004 | `	return SXRET_OK;` |
|    131 | 4005 | `}` |
|      - | 4006 | `/*` |
|      - | 4007 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4008 | ` */` |
|     90 | 4009 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4010 | `{` |
|      - | 4011 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4012 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4013 | `	 * non-OK rc also stops the format loop. */` |
|     91 | 4014 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|     91 | 4015 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|     91 | 4016 | `	return *pRc;` |
|      1 | 4017 | `}` |
|      - | 4018 | `/*` |
|      - | 4019 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4020 | ` *  Return a formatted string.` |
|      - | 4021 | ` * Parameters` |
|      - | 4022 | ` *  $format` |
|      - | 4023 | ` *    The format string (see block comment above)` |
|      - | 4024 | ` * Return` |
|      - | 4025 | ` *  A string produced according to the formatting string format.` |
|      - | 4026 | ` */` |
|     62 | 4027 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4028 | `{` |
|      - | 4029 | `	const char *zFormat;` |
|     63 | 4030 | `	sxi32 rc = SXRET_OK;` |
|      - | 4031 | `	int nLen;` |
|     63 | 4032 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4033 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4034 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4035 | `		return PH7_OK;` |
|      - | 4036 | `	}` |
|      - | 4037 | `	/* Extract the string format */` |
|     61 | 4038 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 4039 | `	if( nLen < 1 ){` |
|      - | 4040 | `		/* Empty string */` |
|    ! 0 | 4041 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4042 | `		return PH7_OK;` |
|      - | 4043 | `	}` |
|      - | 4044 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     61 | 4045 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     61 | 4046 | `	if( rc != SXRET_OK ){` |
|      - | 4047 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4048 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4049 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4050 | `	}` |
|     61 | 4051 | `	return PH7_OK;` |
|     32 | 4052 | `}` |
|      - | 4053 | `/*` |
|      - | 4054 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4055 | ` */` |
|    922 | 4056 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4057 | `{` |
|    923 | 4058 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4059 | `	/* Call the VM output consumer directly */` |
|    923 | 4060 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4061 | `	/* Increment counter */` |
|    923 | 4062 | `	*pCounter += nLen;` |
|    923 | 4063 | `	return PH7_OK;` |
|      1 | 4064 | `}` |
|      - | 4065 | `/*` |
|      - | 4066 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4067 | ` *  Output a formatted string.` |
|      - | 4068 | ` * Parameters` |
|      - | 4069 | ` *  $format` |
|      - | 4070 | ` *   See sprintf() for a description of format.` |
|      - | 4071 | ` * Return` |
|      - | 4072 | ` *  The length of the outputted string.` |
|      - | 4073 | ` */` |
|    176 | 4074 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4075 | `{` |
|    177 | 4076 | `	ph7_int64 nCounter = 0;` |
|      - | 4077 | `	const char *zFormat;` |
|      - | 4078 | `	int nLen;` |
|    177 | 4079 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4080 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 4081 | `		ph7_result_int(pCtx,0);` |
|      3 | 4082 | `		return PH7_OK;` |
|      - | 4083 | `	}` |
|      - | 4084 | `	/* Extract the string format */` |
|    175 | 4085 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    175 | 4086 | `	if( nLen < 1 ){` |
|      - | 4087 | `		/* Empty string */` |
|    ! 0 | 4088 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4089 | `		return PH7_OK;` |
|      - | 4090 | `	}` |
|      - | 4091 | `	/* Format the string */` |
|    175 | 4092 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4093 | `	/* Return the length of the outputted string */` |
|    175 | 4094 | `	ph7_result_int64(pCtx,nCounter);` |
|    175 | 4095 | `	return PH7_OK;` |
|     89 | 4096 | `}` |
|      - | 4097 | `/*` |
|      - | 4098 | ` * int vprintf(string $format,array $args)` |
|      - | 4099 | ` *  Output a formatted string.` |
|      - | 4100 | ` * Parameters` |
|      - | 4101 | ` *  $format` |
|      - | 4102 | ` *   See sprintf() for a description of format.` |
|      - | 4103 | ` * Return` |
|      - | 4104 | ` *  The length of the outputted string.` |
|      - | 4105 | ` */` |
|      2 | 4106 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4107 | `{` |
|      3 | 4108 | `	ph7_int64 nCounter = 0;` |
|      - | 4109 | `	const char *zFormat;` |
|      - | 4110 | `	ph7_hashmap *pMap;` |
|      - | 4111 | `	SySet sArg;` |
|      - | 4112 | `	int nLen,n;` |
|      3 | 4113 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4114 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 4115 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4116 | `		return PH7_OK;` |
|      - | 4117 | `	}` |
|      - | 4118 | `	/* Extract the string format */` |
|      3 | 4119 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4120 | `	if( nLen < 1 ){` |
|      - | 4121 | `		/* Empty string */` |
|    ! 0 | 4122 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4123 | `		return PH7_OK;` |
|      - | 4124 | `	}` |
|      - | 4125 | `	/* Point to the hashmap */` |
|      3 | 4126 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4127 | `	/* Extract arguments from the hashmap */` |
|      3 | 4128 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4129 | `	/* Format the string */` |
|      3 | 4130 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4131 | `	/* Return the length of the outputted string */` |
|      3 | 4132 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 4133 | `	/* Release the container */` |
|      3 | 4134 | `	SySetRelease(&sArg);` |
|      3 | 4135 | `	return PH7_OK;` |
|      2 | 4136 | `}` |
|      - | 4137 | `/*` |
|      - | 4138 | ` * int vsprintf(string $format,array $args)` |
|      - | 4139 | ` *  Output a formatted string.` |
|      - | 4140 | ` * Parameters` |
|      - | 4141 | ` *  $format` |
|      - | 4142 | ` *   See sprintf() for a description of format.` |
|      - | 4143 | ` * Return` |
|      - | 4144 | ` *  A string produced according to the formatting string format.` |
|      - | 4145 | ` */` |
|     10 | 4146 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4147 | `{` |
|      - | 4148 | `	const char *zFormat;` |
|      - | 4149 | `	ph7_hashmap *pMap;` |
|      - | 4150 | `	SySet sArg;` |
|     11 | 4151 | `	sxi32 rc = SXRET_OK;` |
|      - | 4152 | `	int nLen,n;` |
|     11 | 4153 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4154 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 4155 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4156 | `		return PH7_OK;` |
|      - | 4157 | `	}` |
|      - | 4158 | `	/* Extract the string format */` |
|      7 | 4159 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 4160 | `	if( nLen < 1 ){` |
|      - | 4161 | `		/* Empty string */` |
|    ! 0 | 4162 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4163 | `		return PH7_OK;` |
|      - | 4164 | `	}` |
|      - | 4165 | `	/* Point to hashmap */` |
|      7 | 4166 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4167 | `	/* Extract arguments from the hashmap */` |
|      7 | 4168 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4169 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      7 | 4170 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4171 | `	/* Release the container */` |
|      7 | 4172 | `	SySetRelease(&sArg);` |
|      7 | 4173 | `	if( rc != SXRET_OK ){` |
|      - | 4174 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4175 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4176 | `	}` |
|      7 | 4177 | `	return PH7_OK;` |
|      6 | 4178 | `}` |
|      - | 4179 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4180 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4181 | `/*` |
|      - | 4182 | ` * Symisc eXtension.` |
|      - | 4183 | ` * string size_format(int64 $size)` |
|      - | 4184 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4185 | ` *  Example:` |
|      - | 4186 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4187 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4188 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4189 | ` * Parameter` |
|      - | 4190 | ` *  $size` |
|      - | 4191 | ` *    Entity size in bytes.` |
|      - | 4192 | ` * Return` |
|      - | 4193 | ` *   Formatted string representation of the given size.` |
|      - | 4194 | ` */` |
|     24 | 4195 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4196 | `{` |
|      - | 4197 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4198 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4199 | `	sxi32 nRest,i_32;` |
|      - | 4200 | `	ph7_int64 iSize;` |
|     25 | 4201 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4202 |  |
|     25 | 4203 | `	if( nArg < 1 ){` |
|      - | 4204 | `		/* Missing argument,return the empty string */` |
|      3 | 4205 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4206 | `		return PH7_OK;` |
|      - | 4207 | `	}` |
|      - | 4208 | `	/* Extract the given size */` |
|     23 | 4209 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4210 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4211 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4212 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4213 | `		return PH7_OK;` |
|      - | 4214 | `	}` |
|     19 | 4215 | `	for(;;){` |
|     39 | 4216 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4217 | `		iSize >>= 10;` |
|     39 | 4218 | `		c++;` |
|     39 | 4219 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4220 | `			break;` |
|      - | 4221 | `		}` |
|      1 | 4222 | `	}` |
|     19 | 4223 | `	nRest /= 100;` |
|     19 | 4224 | `	if( nRest > 9 ){` |
|    ! 0 | 4225 | `		nRest = 9;` |
|    ! 0 | 4226 | `	}` |
|     19 | 4227 | `	if( iSize > 999 ){` |
|    ! 0 | 4228 | `		c++;` |
|    ! 0 | 4229 | `		nRest = 9;` |
|    ! 0 | 4230 | `		iSize = 0;` |
|    ! 0 | 4231 | `	}` |
|     19 | 4232 | `	i_32 = (sxi32)iSize;` |
|      - | 4233 | `	/* Format */` |
|     19 | 4234 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4235 | `	return PH7_OK;` |
|     13 | 4236 | `}` |
|      - | 4237 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4238 | `/*` |
|      - | 4239 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4240 | ` *   Calculate the md5 hash of a string.` |
|      - | 4241 | ` * Parameter` |
|      - | 4242 | ` *  $str` |
|      - | 4243 | ` *   Input string` |
|      - | 4244 | ` * $raw_output` |
|      - | 4245 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4246 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4247 | ` * Return` |
|      - | 4248 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4249 | ` */` |
|     14 | 4250 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4251 | `{` |
|      - | 4252 | `	unsigned char zDigest[16];` |
|     15 | 4253 | `	int raw_output = FALSE;` |
|      - | 4254 | `	const void *pIn;` |
|      - | 4255 | `	int nLen;` |
|     15 | 4256 | `	if( nArg < 1 ){` |
|      - | 4257 | `		/* Missing arguments,return the empty string */` |
|      3 | 4258 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4259 | `		return PH7_OK;` |
|      - | 4260 | `	}` |
|      - | 4261 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4262 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4263 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4264 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4265 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4266 | `	}` |
|      - | 4267 | `	/* Compute the MD5 digest */` |
|     13 | 4268 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4269 | `	if( raw_output ){` |
|      - | 4270 | `		/* Output raw digest */` |
|      5 | 4271 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4272 | `	}else{` |
|      - | 4273 | `		/* Perform a binary to hex conversion */` |
|      9 | 4274 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4275 | `	}` |
|     13 | 4276 | `	return PH7_OK;` |
|      8 | 4277 | `}` |
|      - | 4278 | `/*` |
|      - | 4279 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4280 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4281 | ` * Parameter` |
|      - | 4282 | ` *  $str` |
|      - | 4283 | ` *   Input string` |
|      - | 4284 | ` * $raw_output` |
|      - | 4285 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4286 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4287 | ` * Return` |
|      - | 4288 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4289 | ` */` |
|     12 | 4290 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4291 | `{` |
|      - | 4292 | `	unsigned char zDigest[20];` |
|     13 | 4293 | `	int raw_output = FALSE;` |
|      - | 4294 | `	const void *pIn;` |
|      - | 4295 | `	int nLen;` |
|     13 | 4296 | `	if( nArg < 1 ){` |
|      - | 4297 | `		/* Missing arguments,return the empty string */` |
|      3 | 4298 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4299 | `		return PH7_OK;` |
|      - | 4300 | `	}` |
|      - | 4301 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4302 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4303 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4304 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4305 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4306 | `	}` |
|      - | 4307 | `	/* Compute the SHA1 digest */` |
|     11 | 4308 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4309 | `	if( raw_output ){` |
|      - | 4310 | `		/* Output raw digest */` |
|      5 | 4311 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4312 | `	}else{` |
|      - | 4313 | `		/* Perform a binary to hex conversion */` |
|      7 | 4314 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4315 | `	}` |
|     11 | 4316 | `	return PH7_OK;` |
|      7 | 4317 | `}` |
|      - | 4318 | `/*` |
|      - | 4319 | ` * int64 crc32(string $str)` |
|      - | 4320 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4321 | ` * Parameter` |
|      - | 4322 | ` *  $str` |
|      - | 4323 | ` *   Input string` |
|      - | 4324 | ` * Return` |
|      - | 4325 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4326 | ` */` |
|      4 | 4327 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4328 | `{` |
|      - | 4329 | `	const void *pIn;` |
|      - | 4330 | `	sxu32 nCRC;` |
|      - | 4331 | `	int nLen;` |
|      5 | 4332 | `	if( nArg < 1 ){` |
|      - | 4333 | `		/* Missing arguments,return 0 */` |
|      3 | 4334 | `		ph7_result_int(pCtx,0);` |
|      3 | 4335 | `		return PH7_OK;` |
|      - | 4336 | `	}` |
|      - | 4337 | `	/* Extract the input string */` |
|      3 | 4338 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4339 | `	if( nLen < 1 ){` |
|      - | 4340 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4341 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4342 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4343 | `		return PH7_OK;` |
|      - | 4344 | `	}` |
|      - | 4345 | `	/* Calculate the sum */` |
|      3 | 4346 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4347 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4348 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4349 | `	return PH7_OK;` |
|      3 | 4350 | `}` |
|      - | 4351 | `/*` |
|      - | 4352 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4353 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4354 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4355 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4356 | ` */` |
|     11 | 4357 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4358 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4359 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4360 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4361 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4362 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4363 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4364 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4365 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4366 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4367 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4368 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4369 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4370 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4371 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4372 | `struct HashAlgo {` |
|      - | 4373 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4374 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4375 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4376 | `	void (*xInit)(HashCtx *);` |
|      - | 4377 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4378 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4379 | `};` |
|      - | 4380 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4381 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4382 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4383 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4384 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4385 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4386 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4387 | `};` |
|      - | 4388 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4389 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4390 | `	sxu32 i;` |
|    279 | 4391 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4392 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4393 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4394 | `			return &aHashAlgo[i];` |
|      - | 4395 | `		}` |
|    106 | 4396 | `	}` |
|      6 | 4397 | `	return 0;` |
|     38 | 4398 | `}` |
|      - | 4399 | `/*` |
|      - | 4400 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4401 | ` *   Generate a hash value (message digest).` |
|      - | 4402 | ` */` |
|     54 | 4403 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4404 | `{` |
|      - | 4405 | `	const HashAlgo *pAlgo;` |
|      - | 4406 | `	const char *zAlgo,*zData;` |
|     56 | 4407 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4408 | `	HashCtx sCtx;` |
|      - | 4409 | `	unsigned char zDigest[64];` |
|     56 | 4410 | `	if( nArg < 2 ){` |
|    ! 0 | 4411 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4412 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4413 | `	}` |
|     56 | 4414 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4415 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4416 | `	if( pAlgo == 0 ){` |
|      3 | 4417 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4418 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4419 | `	}` |
|     53 | 4420 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4421 | `	if( nArg > 2 ){` |
|      9 | 4422 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4423 | `	}` |
|     53 | 4424 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4425 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4426 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4427 | `	if( raw_output ){` |
|      9 | 4428 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4429 | `	}else{` |
|     45 | 4430 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4431 | `	}` |
|     53 | 4432 | `	return PH7_OK;` |
|     29 | 4433 | `}` |
|      - | 4434 | `/*` |
|      - | 4435 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4436 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4437 | ` */` |
|     16 | 4438 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4439 | `{` |
|      - | 4440 | `	const HashAlgo *pAlgo;` |
|      - | 4441 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4442 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4443 | `	HashCtx sCtx;` |
|      - | 4444 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4445 | `	int i,nBlock,nDigest;` |
|     18 | 4446 | `	if( nArg < 3 ){` |
|    ! 0 | 4447 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4448 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4449 | `	}` |
|     18 | 4450 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4451 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4452 | `	if( pAlgo == 0 ){` |
|      3 | 4453 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4454 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4455 | `	}` |
|     15 | 4456 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4457 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4458 | `	if( nArg > 3 ){` |
|      3 | 4459 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4460 | `	}` |
|     15 | 4461 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4462 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4463 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4464 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4465 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4466 | `	if( nKeyLen > nBlock ){` |
|      3 | 4467 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4468 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4469 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4470 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4471 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4472 | `	}` |
|   1039 | 4473 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4474 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4475 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4476 | `	}` |
|      - | 4477 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4478 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4479 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4480 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4481 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4482 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4483 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4484 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4485 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4486 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4487 | `	if( raw_output ){` |
|      3 | 4488 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4489 | `	}else{` |
|     13 | 4490 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4491 | `	}` |
|     15 | 4492 | `	return PH7_OK;` |
|     10 | 4493 | `}` |
|      - | 4494 | `/*` |
|      - | 4495 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4496 | ` *   Timing-attack-safe string comparison.` |
|      - | 4497 | ` */` |
|     14 | 4498 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4499 | `{` |
|      - | 4500 | `	const char *zKnown,*zUser;` |
|      - | 4501 | `	int nKnown,nUser,i;` |
|     17 | 4502 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4503 | `	if( nArg < 2 ){` |
|    ! 0 | 4504 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4505 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4506 | `	}` |
|     17 | 4507 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4508 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4509 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4510 | `			ph7_type_name(apArg[0]));` |
|      - | 4511 | `	}` |
|     14 | 4512 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4513 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4514 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4515 | `			ph7_type_name(apArg[1]));` |
|      - | 4516 | `	}` |
|     11 | 4517 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4518 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4519 | `	if( nKnown != nUser ){` |
|      5 | 4520 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4521 | `		return PH7_OK;` |
|      - | 4522 | `	}` |
|      - | 4523 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4524 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4525 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4526 | `	}` |
|      7 | 4527 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4528 | `	return PH7_OK;` |
|     10 | 4529 | `}` |
|      - | 4530 | `/*` |
|      - | 4531 | ` * array hash_algos(void)` |
|      - | 4532 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4533 | ` */` |
|      2 | 4534 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4535 | `{` |
|      - | 4536 | `	ph7_value *pArray,*pValue;` |
|      - | 4537 | `	sxu32 i;` |
|      1 | 4538 | `	SXUNUSED(nArg);` |
|      1 | 4539 | `	SXUNUSED(apArg);` |
|      3 | 4540 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4541 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4542 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4543 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4544 | `		return PH7_OK;` |
|      - | 4545 | `	}` |
|     15 | 4546 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4547 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4548 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4549 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4550 | `	}` |
|      3 | 4551 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4552 | `	return PH7_OK;` |
|      2 | 4553 | `}` |
|      - | 4554 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4555 | `/*` |
|      - | 4556 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4557 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4558 | ` */` |
|      - | 4559 | `/*` |
|      - | 4560 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4561 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4562 | ` */` |
|     40 | 4563 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4564 | `{` |
|      - | 4565 | `	int iCost;` |
|     51 | 4566 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4567 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4568 | `		return FALSE;` |
|      - | 4569 | `	}` |
|     29 | 4570 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4571 | `		return FALSE;` |
|      - | 4572 | `	}` |
|     29 | 4573 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4574 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4575 | `		return FALSE;` |
|      - | 4576 | `	}` |
|     27 | 4577 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4578 | `	return TRUE;` |
|     21 | 4579 | `}` |
|      - | 4580 | `/*` |
|      - | 4581 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4582 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4583 | ` */` |
|     20 | 4584 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4585 | `{` |
|     23 | 4586 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4587 | `		return TRUE;` |
|      - | 4588 | `	}` |
|     23 | 4589 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4590 | `		int nAlgo;` |
|     23 | 4591 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4592 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4593 | `	}` |
|    ! 0 | 4594 | `	return FALSE;` |
|     13 | 4595 | `}` |
|      - | 4596 | `/*` |
|      - | 4597 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4598 | ` *  Create a bcrypt hash of the password.` |
|      - | 4599 | ` */` |
|     16 | 4600 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4601 | `{` |
|      - | 4602 | `	const char *zPwd;` |
|     19 | 4603 | `	int nPwd,iCost = 12;` |
|      - | 4604 | `	unsigned char aSalt[16];` |
|      - | 4605 | `	char zHash[60];` |
|     19 | 4606 | `	if( nArg < 2 ){` |
|    ! 0 | 4607 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4608 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4609 | `	}` |
|     19 | 4610 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4611 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4612 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4613 | `	}` |
|      - | 4614 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4615 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4616 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4617 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4618 | `	}` |
|     16 | 4619 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4620 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4621 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4622 | `	}` |
|     13 | 4623 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4624 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4625 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4626 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4627 | `	}` |
|     13 | 4628 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4629 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4630 | `		return PH7_OK;` |
|      - | 4631 | `	}` |
|     13 | 4632 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4633 | `	return PH7_OK;` |
|     11 | 4634 | `}` |
|      - | 4635 | `/*` |
|      - | 4636 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4637 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4638 | ` */` |
|     28 | 4639 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4640 | `{` |
|      - | 4641 | `	const char *zPwd,*zHash;` |
|      - | 4642 | `	int nPwd,nHash,iCost,i;` |
|      - | 4643 | `	unsigned char aSalt[16];` |
|      - | 4644 | `	char zComputed[60];` |
|     29 | 4645 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4646 | `	if( nArg < 2 ){` |
|    ! 0 | 4647 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4648 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4649 | `	}` |
|     29 | 4650 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4651 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4652 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4653 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4654 | `		return PH7_OK;` |
|      - | 4655 | `	}` |
|      - | 4656 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4657 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4658 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4659 | `		return PH7_OK;` |
|      - | 4660 | `	}` |
|     19 | 4661 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4662 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4663 | `		return PH7_OK;` |
|      - | 4664 | `	}` |
|      - | 4665 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4666 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4667 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4668 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4669 | `	}` |
|     19 | 4670 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4671 | `	return PH7_OK;` |
|     15 | 4672 | `}` |
|      - | 4673 | `/*` |
|      - | 4674 | ` * array password_get_info(string $hash)` |
|      - | 4675 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4676 | ` */` |
|      6 | 4677 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4678 | `{` |
|      7 | 4679 | `	const char *zHash = "";` |
|      7 | 4680 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4681 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4682 | `	if( nArg > 0 ){` |
|      7 | 4683 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4684 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4685 | `	}` |
|      7 | 4686 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4687 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4688 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4689 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4690 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4691 | `		return PH7_OK;` |
|      - | 4692 | `	}` |
|      7 | 4693 | `	if( bBcrypt ){` |
|      5 | 4694 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4695 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4696 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4697 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4698 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4699 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4700 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4701 | `	}else{` |
|      3 | 4702 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4703 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4704 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4705 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4706 | `	}` |
|      7 | 4707 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4708 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4709 | `	return PH7_OK;` |
|      4 | 4710 | `}` |
|      - | 4711 | `/*` |
|      - | 4712 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4713 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4714 | ` */` |
|      6 | 4715 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4716 | `{` |
|      - | 4717 | `	const char *zHash;` |
|      7 | 4718 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4719 | `	if( nArg < 2 ){` |
|    ! 0 | 4720 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4721 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4722 | `	}` |
|      7 | 4723 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4724 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4725 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4726 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4727 | `		return PH7_OK;` |
|      - | 4728 | `	}` |
|      5 | 4729 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4730 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4731 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4732 | `	}` |
|      5 | 4733 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4734 | `	return PH7_OK;` |
|      4 | 4735 | `}` |
|      - | 4736 | `/*` |
|      - | 4737 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4738 | ` *` |
|      - | 4739 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4740 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4741 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4742 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4743 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4744 | ` */` |
|      - | 4745 | `#define FV_VALIDATE_INT     257` |
|      - | 4746 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4747 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4748 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4749 | `#define FV_VALIDATE_URL     273` |
|      - | 4750 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4751 | `#define FV_VALIDATE_IP      275` |
|      - | 4752 | `#define FV_VALIDATE_MAC     276` |
|      - | 4753 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4754 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4755 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4756 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4757 | `#define FV_SANITIZE_URL     518` |
|      - | 4758 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4759 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4760 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4761 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4762 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4763 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4764 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4765 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4766 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4767 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4768 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4769 |  |
|      - | 4770 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4771 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    125 | 4772 | `static void FvTrim(const char **pz,int *pn){` |
|    125 | 4773 | `	const char *z = *pz;` |
|    125 | 4774 | `	int n = *pn;` |
|    129 | 4775 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    133 | 4776 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    125 | 4777 | `	*pz = z; *pn = n;` |
|    125 | 4778 | `}` |
|      - | 4779 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4780 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4781 | `	int neg = 0, i;` |
|     57 | 4782 | `	sxu64 u = 0;` |
|     57 | 4783 | `	FvTrim(&z,&n);` |
|     57 | 4784 | `	if( n==0 ){ return 0; }` |
|     51 | 4785 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4786 | `	if( n==0 ){ return 0; }` |
|     49 | 4787 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4788 | `		z += 2; n -= 2;` |
|      3 | 4789 | `		if( n==0 ){ return 0; }` |
|      7 | 4790 | `		for( i=0; i<n; i++ ){` |
|      5 | 4791 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4792 | `			if( h<0 ){ return 0; }` |
|      5 | 4793 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4794 | `			u = u*16 + (sxu64)h;` |
|      3 | 4795 | `		}` |
|     48 | 4796 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4797 | `		for( i=0; i<n; i++ ){` |
|      7 | 4798 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4799 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4800 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4801 | `		}` |
|      2 | 4802 | `	}else{` |
|     45 | 4803 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4804 | `		for( i=0; i<n; i++ ){` |
|    173 | 4805 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4806 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4807 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4808 | `		}` |
|      - | 4809 | `	}` |
|     33 | 4810 | `	if( neg ){` |
|      5 | 4811 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4812 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4813 | `	}else{` |
|     29 | 4814 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4815 | `		*pOut = (ph7_int64)u;` |
|      - | 4816 | `	}` |
|     31 | 4817 | `	return 1;` |
|     29 | 4818 | `}` |
|      - | 4819 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     41 | 4820 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4821 | `	char zBuf[512];` |
|     41 | 4822 | `	int i, m = 0, seenDigit = 0;` |
|     41 | 4823 | `	const char *zv; int nv; double d = 0; const char *zRest = 0;` |
|     41 | 4824 | `	FvTrim(&z,&n);` |
|      - | 4825 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4826 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     41 | 4827 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     41 | 4828 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4829 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4830 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4831 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4832 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     23 | 4833 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     23 | 4834 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     23 | 4835 | `		intEnd = s;` |
|    155 | 4836 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    133 | 4837 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    133 | 4838 | `			intEnd++;` |
|      1 | 4839 | `		}` |
|     23 | 4840 | `		if( hasComma ){` |
|     23 | 4841 | `			segStart = s; segIdx = 0;` |
|    151 | 4842 | `			for( i=s; i<=intEnd; i++ ){` |
|    139 | 4843 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     45 | 4844 | `					int segLen = i - segStart, k;` |
|     45 | 4845 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     23 | 4846 | `					else if( segLen!=3 ){ return 0; }` |
|    107 | 4847 | `					for( k=segStart; k<i; k++ ){` |
|     73 | 4848 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     73 | 4849 | `						zBuf[m++] = z[k];` |
|     37 | 4850 | `					}` |
|     35 | 4851 | `					segStart = i+1; segIdx++;` |
|     17 | 4852 | `				}` |
|     65 | 4853 | `			}` |
|      7 | 4854 | `		}else{` |
|    ! 0 | 4855 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4856 | `		}` |
|     17 | 4857 | `		for( i=intEnd; i<n; i++ ){` |
|      5 | 4858 | `			if( z[i]==',' ){ return 0; }` |
|      5 | 4859 | `			zBuf[m++] = z[i];` |
|      3 | 4860 | `		}` |
|     13 | 4861 | `		zv = zBuf; nv = m;` |
|      7 | 4862 | `	}else{` |
|     19 | 4863 | `		zv = z; nv = n;` |
|      - | 4864 | `	}` |
|     31 | 4865 | `	i = 0;` |
|     31 | 4866 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    105 | 4867 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     31 | 4868 | `	if( i<nv && zv[i]=='.' ){` |
|     13 | 4869 | `		i++;` |
|     23 | 4870 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|      6 | 4871 | `	}` |
|     31 | 4872 | `	if( !seenDigit ){ return 0; }` |
|     29 | 4873 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|      5 | 4874 | `		i++;` |
|      5 | 4875 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|      5 | 4876 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|      9 | 4877 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|      2 | 4878 | `	}` |
|     29 | 4879 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4880 | `	/* Divergence: PHP rejects magnitudes beyond the double range ("1e400" ->` |
|      - | 4881 | `	 * false), but SyStrToReal (the engine-wide float parser, also behind` |
|      - | 4882 | `	 * floatval/(float)) saturates them to a finite value, so they validate here. */` |
|     25 | 4883 | `	SyStrToReal(zv,(sxu32)nv,(void *)&d,&zRest);` |
|     25 | 4884 | `	*pOut = d;` |
|     25 | 4885 | `	return 1;` |
|     21 | 4886 | `}` |
|      - | 4887 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4888 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4889 | ` * false, NOT failures. */` |
|     33 | 4890 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4891 | `	FvTrim(&z,&n);` |
|     35 | 4892 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4893 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4894 | `		*pBool = 1; return 1;` |
|      - | 4895 | `	}` |
|     23 | 4896 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4897 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4898 | `		*pBool = 0; return 1;` |
|      - | 4899 | `	}` |
|      9 | 4900 | `	return 0;` |
|     15 | 4901 | `}` |
|      - | 4902 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4903 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4904 | `	int i = 0, parts = 0;` |
|     77 | 4905 | `	while( i<n ){` |
|     65 | 4906 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4907 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4908 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4909 | `			if( val>255 ){ return 0; }` |
|     79 | 4910 | `			digits++; i++;` |
|      1 | 4911 | `		}` |
|     59 | 4912 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4913 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4914 | `		parts++;` |
|     45 | 4915 | `		if( parts>4 ){ return 0; }` |
|     45 | 4916 | `		if( i<n ){` |
|     33 | 4917 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4918 | `			i++;` |
|     33 | 4919 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4920 | `		}` |
|      1 | 4921 | `	}` |
|     13 | 4922 | `	return parts==4;` |
|     17 | 4923 | `}` |
|      - | 4924 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4925 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4926 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4927 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4928 | `	if( n==0 ){ return 0; }` |
|    145 | 4929 | `	while( i<=n ){` |
|    133 | 4930 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4931 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4932 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4933 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4934 | `			if( isV4 ){` |
|     11 | 4935 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4936 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4937 | `				groups += 2;` |
|      3 | 4938 | `			}else{` |
|     13 | 4939 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4940 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4941 | `				groups++;` |
|      - | 4942 | `			}` |
|     17 | 4943 | `			segStart = i+1;` |
|      8 | 4944 | `		}` |
|    127 | 4945 | `		i++;` |
|      1 | 4946 | `	}` |
|     13 | 4947 | `	return groups;` |
|     10 | 4948 | `}` |
|      - | 4949 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4950 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4951 | `	const char *zDbl = 0;` |
|      - | 4952 | `	int i, ga, gb;` |
|    139 | 4953 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4954 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4955 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4956 | `			zDbl = z+i;` |
|      5 | 4957 | `		}` |
|     61 | 4958 | `	}` |
|     17 | 4959 | `	if( zDbl==0 ){` |
|      9 | 4960 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4961 | `	}else{` |
|      9 | 4962 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4963 | `		int lenB = n - lenA - 2;` |
|      9 | 4964 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4965 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4966 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4967 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4968 | `	}` |
|     10 | 4969 | `}` |
|     25 | 4970 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4971 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4972 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4973 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4974 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4975 | `	return 0;` |
|     13 | 4976 | `}` |
|      - | 4977 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 4978 | `static int FvValidateMac(const char *z,int n){` |
|      - | 4979 | `	char sep;` |
|      - | 4980 | `	int i;` |
|     11 | 4981 | `	if( n!=17 ){ return 0; }` |
|      7 | 4982 | `	sep = z[2];` |
|      7 | 4983 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 4984 | `	for( i=0; i<17; i++ ){` |
|    101 | 4985 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 4986 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 4987 | `	}` |
|      5 | 4988 | `	return 1;` |
|      6 | 4989 | `}` |
|      - | 4990 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 4991 | ` * parts or IP-literal domains). */` |
|     21 | 4992 | `static int FvValidateEmail(const char *z,int n){` |
|     21 | 4993 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 4994 | `	const char *zDom;` |
|     21 | 4995 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 4996 | `	for( i=0; i<n; i++ ){` |
|    181 | 4997 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 4998 | `	}` |
|     21 | 4999 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5000 | `	localLen = at;` |
|     21 | 5001 | `	zDom = z + at + 1;` |
|     21 | 5002 | `	domLen = n - at - 1;` |
|     21 | 5003 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5004 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5005 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5006 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5007 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5008 | `	}` |
|     15 | 5009 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5010 | `	labelStart = 0;` |
|     85 | 5011 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5012 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5013 | `			int ll = i - labelStart;` |
|     25 | 5014 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5015 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5016 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5017 | `			labelStart = i+1;` |
|     12 | 5018 | `		}else{` |
|     51 | 5019 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5020 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5021 | `		}` |
|     37 | 5022 | `	}` |
|     11 | 5023 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5024 | `	return 1;` |
|     11 | 5025 | `}` |
|      - | 5026 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5027 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5028 | `	int i;` |
|     11 | 5029 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5030 | `	for( i=0; i<n; i++ ){` |
|     75 | 5031 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5032 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5033 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5034 | `	}` |
|      7 | 5035 | `	return 1;` |
|      6 | 5036 | `}` |
|      - | 5037 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5038 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5039 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5040 | `	SyhttpUri sUri;` |
|     15 | 5041 | `	if( n==0 ){ return 0; }` |
|     15 | 5042 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5043 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5044 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5045 | `}` |
|      - | 5046 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5047 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5048 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5049 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5050 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5051 | `	int i, runStart = 0;` |
|     37 | 5052 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5053 | `	for( i=0; i<n; i++ ){` |
|     91 | 5054 | `		char c = z[i];` |
|     91 | 5055 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5056 | `		if( !keep && isFloat ){` |
|     38 | 5057 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5058 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5059 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5060 | `		}` |
|     61 | 5061 | `		if( !keep ){` |
|     33 | 5062 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5063 | `			runStart = i+1;` |
|     16 | 5064 | `		}` |
|     31 | 5065 | `	}` |
|      7 | 5066 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5067 | `}` |
|      - | 5068 | `/* SANITIZE_SPECIAL_CHARS (full=0, numeric entities; also encodes control bytes` |
|      - | 5069 | ` * <32 as &#N;) / FULL_SPECIAL_CHARS (full=1, named entities for <>&"').` |
|      - | 5070 | ` * Divergence on bytes >=128: PHP's FULL filter is UTF-8-aware — it named-entity` |
|      - | 5071 | ` * encodes valid sequences ("\xC3\xA9" -> "&eacute;") and drops invalid ones; we` |
|      - | 5072 | ` * pass every byte >=128 through verbatim (the engine has no UTF-8 entity table,` |
|      - | 5073 | ` * and PH7_builtin_htmlspecialchars behaves the same way). Bytes 0-127 match. */` |
|      7 | 5074 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int full){` |
|      7 | 5075 | `	int i, runStart = 0;` |
|      - | 5076 | `	const char *zEnt;` |
|      7 | 5077 | `	ph7_result_string(pCtx,"",0);` |
|     43 | 5078 | `	for( i=0; i<n; i++ ){` |
|     37 | 5079 | `		unsigned char c = (unsigned char)z[i];` |
|     37 | 5080 | `		switch( c ){` |
|      5 | 5081 | `		case '<':  zEnt = full?"&lt;":"&#60;";   break;` |
|      5 | 5082 | `		case '>':  zEnt = full?"&gt;":"&#62;";   break;` |
|      5 | 5083 | `		case '&':  zEnt = full?"&amp;":"&#38;";  break;` |
|      5 | 5084 | `		case '"':  zEnt = full?"&quot;":"&#34;"; break;` |
|      5 | 5085 | `		case '\'': zEnt = full?"&#039;":"&#39;"; break;` |
|      8 | 5086 | `		default:` |
|     17 | 5087 | `			if( full \|\| c>=32 ){ continue; } /* keep in the current run */` |
|      - | 5088 | `			/* SPECIAL_CHARS encodes a control byte as a numeric entity. */` |
|      5 | 5089 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      5 | 5090 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      5 | 5091 | `			runStart = i+1;` |
|      5 | 5092 | `			continue;` |
|      - | 5093 | `		}` |
|     21 | 5094 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     21 | 5095 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     21 | 5096 | `		runStart = i+1;` |
|     11 | 5097 | `	}` |
|      7 | 5098 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5099 | `}` |
|     25 | 5100 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5101 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5102 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5103 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5104 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5105 | `}` |
|     23 | 5106 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5107 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5108 | `}` |
|      - | 5109 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5110 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5111 | `	int i, runStart = 0;` |
|      5 | 5112 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5113 | `	for( i=0; i<n; i++ ){` |
|     47 | 5114 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5115 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5116 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5117 | `			runStart = i+1;` |
|      5 | 5118 | `		}` |
|     24 | 5119 | `	}` |
|      5 | 5120 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5121 | `}` |
|      - | 5122 | `/*` |
|      - | 5123 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5124 | ` *  Validate or sanitize a value. The scalar input is coerced to a string and the` |
|      - | 5125 | ` *  selected filter applied; on validation failure the 'default' option (if any)` |
|      - | 5126 | ` *  is returned, else null when FILTER_NULL_ON_FAILURE is set, else false.` |
|      - | 5127 | ` */` |
|    230 | 5128 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5129 | `{` |
|    232 | 5130 | `	int iFilter = FV_DEFAULT, iFlags = 0, bNull;` |
|    232 | 5131 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|      - | 5132 | `	const char *zVal; int nVal;` |
|    232 | 5133 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    232 | 5134 | `	if( nArg>1 ){ iFilter = ph7_value_to_int(apArg[1]); }` |
|    232 | 5135 | `	if( nArg>2 ){` |
|     53 | 5136 | `		if( ph7_value_is_array(apArg[2]) ){` |
|     13 | 5137 | `			ph7_value *pF = ph7_array_fetch(apArg[2],"flags",(int)sizeof("flags")-1);` |
|     13 | 5138 | `			if( pF ){ iFlags = ph7_value_to_int(pF); }` |
|     13 | 5139 | `			pOpts = ph7_array_fetch(apArg[2],"options",(int)sizeof("options")-1);` |
|     13 | 5140 | `			if( pOpts && !ph7_value_is_array(pOpts) ){ pOpts = 0; }` |
|     13 | 5141 | `			if( pOpts ){ pDefault = ph7_array_fetch(pOpts,"default",(int)sizeof("default")-1); }` |
|      7 | 5142 | `		}else{` |
|     41 | 5143 | `			iFlags = ph7_value_to_int(apArg[2]);` |
|      - | 5144 | `		}` |
|     26 | 5145 | `	}` |
|    232 | 5146 | `	bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5147 | `	/* An array/object input fails every scalar filter. */` |
|    232 | 5148 | `	if( ph7_value_is_array(apArg[0]) ){ goto fail; }` |
|    230 | 5149 | `	zVal = ph7_value_to_string(apArg[0],&nVal);` |
|    230 | 5150 | `	switch( iFilter ){` |
|     28 | 5151 | `	case FV_VALIDATE_INT: {` |
|      - | 5152 | `		ph7_int64 v;` |
|     58 | 5153 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5154 | `		if( pOpts ){` |
|      7 | 5155 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5156 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5157 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5158 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5159 | `		}` |
|     29 | 5160 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5161 | `		return PH7_OK;` |
|      - | 5162 | `	}` |
|     20 | 5163 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5164 | `		double d;` |
|     41 | 5165 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     25 | 5166 | `		ph7_result_double(pCtx,d);` |
|     25 | 5167 | `		return PH7_OK;` |
|      - | 5168 | `	}` |
|     14 | 5169 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5170 | `		int b;` |
|     29 | 5171 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5172 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5173 | `		return PH7_OK;` |
|      - | 5174 | `	}` |
|     25 | 5175 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5176 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     21 | 5177 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5178 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5179 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5180 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5181 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5182 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5183 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5184 | `		if( pRe==0 ){` |
|      3 | 5185 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5186 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5187 | `		}` |
|      5 | 5188 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5189 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5190 | `		goto pass;` |
|      - | 5191 | `#else` |
|      - | 5192 | `		goto fail;` |
|      - | 5193 | `#endif` |
|      - | 5194 | `	}` |
|      3 | 5195 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5196 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|      5 | 5197 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5198 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeSpecial(pCtx,zVal,nVal,1); return PH7_OK;` |
|      3 | 5199 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5200 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|      5 | 5201 | `	case FV_DEFAULT: goto pass; /* FILTER_UNSAFE_RAW: pass through unchanged */` |
|    ! 0 | 5202 | `	default:` |
|    ! 0 | 5203 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5204 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5205 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5206 | `	}` |
|     48 | 5207 | `fail:` |
|     97 | 5208 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|     95 | 5209 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|     91 | 5210 | `	else { ph7_result_bool(pCtx,0); }` |
|     97 | 5211 | `	return PH7_OK;` |
|     22 | 5212 | `pass: /* validation passed: return the (string) input unchanged */` |
|     45 | 5213 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     45 | 5214 | `	return PH7_OK;` |
|    117 | 5215 | `}` |
|      - | 5216 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5217 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5218 | `/*` |
|      - | 5219 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5220 |  |
|      - | 5221 | ` */` |
|      4 | 5222 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5223 | `	const char *zInput, /* Raw input */` |
|      - | 5224 | `	int nByte,  /* Input length */` |
|      - | 5225 | `	int delim,  /* Delimiter */` |
|      - | 5226 | `	int encl,   /* Enclosure */` |
|      - | 5227 | `	int escape,  /* Escape character */` |
|      - | 5228 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5229 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5230 | `	)` |
|      1 | 5231 | `{` |
|      5 | 5232 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5233 | `	const char *zIn = zInput;` |
|      - | 5234 | `	const char *zPtr;` |
|      - | 5235 | `	int isEnc;` |
|      - | 5236 | `	/* Start processing */` |
|      8 | 5237 | `	for(;;){` |
|     17 | 5238 | `		if( zIn >= zEnd ){` |
|      - | 5239 | `			/* No more input to process */` |
|      5 | 5240 | `			break;` |
|      - | 5241 | `		}` |
|     13 | 5242 | `		isEnc = 0;` |
|     13 | 5243 | `		zPtr = zIn;` |
|      - | 5244 | `		/* Find the first delimiter */` |
|     27 | 5245 | `		while( zIn < zEnd ){` |
|     23 | 5246 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5247 | `				/* Delimiter found,break imediately */` |
|      5 | 5248 | `				break;` |
|     15 | 5249 | `			}else if( zIn[0] == encl ){` |
|      - | 5250 | `				/* Inside enclosure? */` |
|    ! 0 | 5251 | `				isEnc = !isEnc;` |
|     15 | 5252 | `			}else if( zIn[0] == escape ){` |
|      - | 5253 | `				/* Escape sequence */` |
|    ! 0 | 5254 | `				zIn++;` |
|    ! 0 | 5255 | `			}` |
|      - | 5256 | `			/* Advance the cursor */` |
|     15 | 5257 | `			zIn++;` |
|      1 | 5258 | `		}` |
|     13 | 5259 | `		if( zIn > zPtr ){` |
|     13 | 5260 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5261 | `			sxi32 rc;` |
|      - | 5262 | `			/* Invoke the supllied callback */` |
|     13 | 5263 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5264 | `				zPtr++;` |
|    ! 0 | 5265 | `				nByteChunk-=2;` |
|    ! 0 | 5266 | `			}` |
|     13 | 5267 | `			if( nByteChunk > 0 ){` |
|     13 | 5268 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5269 | `				if( rc == SXERR_ABORT ){` |
|      - | 5270 | `					/* User callback request an operation abort */` |
|    ! 0 | 5271 | `					break;` |
|      - | 5272 | `				}` |
|      6 | 5273 | `			}` |
|      6 | 5274 | `		}` |
|      - | 5275 | `		/* Ignore trailing delimiter */` |
|     21 | 5276 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5277 | `			zIn++;` |
|      1 | 5278 | `		}` |
|      1 | 5279 | `	}` |
|      5 | 5280 | `	return SXRET_OK;` |
|      1 | 5281 | `}` |
|      - | 5282 | `/*` |
|      - | 5283 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5284 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5285 | ` * argument to this callback.` |
|      - | 5286 | ` */` |
|     12 | 5287 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5288 | `{` |
|     13 | 5289 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5290 | `	ph7_value sEntry;` |
|      - | 5291 | `	SyString sToken;` |
|      - | 5292 | `	/* Insert the token in the given array */` |
|     13 | 5293 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5294 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5295 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5296 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5297 | `		return SXRET_OK;` |
|      - | 5298 | `	}` |
|     13 | 5299 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5300 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5301 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5302 | `	return SXRET_OK;` |
|      7 | 5303 | `}` |
|      - | 5304 | `/*` |
|      - | 5305 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5306 | ` *  Parse a CSV string into an array.` |
|      - | 5307 | ` * Parameters` |
|      - | 5308 | ` *  $input` |
|      - | 5309 | ` *   The string to parse.` |
|      - | 5310 | ` *  $delimiter` |
|      - | 5311 | ` *   Set the field delimiter (one character only).` |
|      - | 5312 | ` *  $enclosure` |
|      - | 5313 | ` *   Set the field enclosure character (one character only).` |
|      - | 5314 | ` *  $escape` |
|      - | 5315 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5316 | ` * Return` |
|      - | 5317 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5318 | ` */` |
|      4 | 5319 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5320 | `{` |
|      - | 5321 | `	const char *zInput,*zPtr;` |
|      - | 5322 | `	ph7_value *pArray;` |
|      5 | 5323 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 5324 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 5325 | `	int escape = '\\';  /* Escape character */` |
|      - | 5326 | `	int nLen;` |
|      5 | 5327 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5328 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 5329 | `		ph7_result_null(pCtx);` |
|      3 | 5330 | `		return PH7_OK;` |
|      - | 5331 | `	}` |
|      - | 5332 | `	/* Extract the raw input */` |
|      3 | 5333 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5334 | `	if( nArg > 1 ){` |
|      - | 5335 | `		int i;` |
|      3 | 5336 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5337 | `			/* Extract the delimiter */` |
|      3 | 5338 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5339 | `			if( i > 0 ){` |
|      3 | 5340 | `				delim = zPtr[0];` |
|      1 | 5341 | `			}` |
|      1 | 5342 | `		}` |
|      3 | 5343 | `		if( nArg > 2 ){` |
|      3 | 5344 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5345 | `				/* Extract the enclosure */` |
|      3 | 5346 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5347 | `				if( i > 0 ){` |
|      3 | 5348 | `					encl = zPtr[0];` |
|      1 | 5349 | `				}` |
|      1 | 5350 | `			}` |
|      3 | 5351 | `			if( nArg > 3 ){` |
|      3 | 5352 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5353 | `					/* Extract the escape character */` |
|      3 | 5354 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5355 | `					if( i > 0 ){` |
|      3 | 5356 | `						escape = zPtr[0];` |
|      1 | 5357 | `					}` |
|      1 | 5358 | `				}` |
|      1 | 5359 | `			}` |
|      1 | 5360 | `		}` |
|      1 | 5361 | `	}` |
|      - | 5362 | `	/* Create our array */` |
|      3 | 5363 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5364 | `	if( pArray == 0 ){` |
|      - | 5365 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5366 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5367 | `	}` |
|      - | 5368 | `	/* Parse the raw input */` |
|      3 | 5369 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5370 | `	/* Return the freshly created array */` |
|      3 | 5371 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5372 | `	return PH7_OK;` |
|      3 | 5373 | `}` |
|      - | 5374 | `/*` |
|      - | 5375 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5376 | ` * container.` |
|      - | 5377 | ` * Refer to [strip_tags()].` |
|      - | 5378 | ` */` |
|     10 | 5379 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5380 | `{` |
|     11 | 5381 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5382 | `	const char *zPtr;` |
|      - | 5383 | `	SyString sEntry;` |
|      - | 5384 | `	/* Strip tags */` |
|     10 | 5385 | `	for(;;){` |
|     45 | 5386 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5387 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5388 | `				zTag++;` |
|      1 | 5389 | `		}` |
|     21 | 5390 | `		if( zTag >= zEnd ){` |
|     11 | 5391 | `			break;` |
|      - | 5392 | `		}` |
|     11 | 5393 | `		zPtr = zTag;` |
|      - | 5394 | `		/* Delimit the tag */` |
|     25 | 5395 | `		while(zTag < zEnd ){` |
|     25 | 5396 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5397 | `				/* UTF-8 stream */` |
|      3 | 5398 | `				zTag++;` |
|      5 | 5399 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5400 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5401 | `				break;` |
|    ! 0 | 5402 | `			}else{` |
|     13 | 5403 | `				zTag++;` |
|      - | 5404 | `			}` |
|      1 | 5405 | `		}` |
|     11 | 5406 | `		if( zTag > zPtr ){` |
|      - | 5407 | `			/* Perform the insertion */` |
|     11 | 5408 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5409 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5410 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5411 | `		}` |
|      - | 5412 | `		/* Jump the trailing '>' */` |
|     11 | 5413 | `		zTag++;` |
|      1 | 5414 | `	}` |
|     11 | 5415 | `	return SXRET_OK;` |
|      1 | 5416 | `}` |
|      - | 5417 | `/*` |
|      - | 5418 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5419 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5420 | ` * Refer to [strip_tags()].` |
|      - | 5421 | ` */` |
|     36 | 5422 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5423 | `{` |
|     37 | 5424 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5425 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5426 | `		SyString sTag;` |
|     85 | 5427 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5428 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5429 | `			zTag++;` |
|      1 | 5430 | `		}` |
|      - | 5431 | `		/* Delimit the tag */` |
|     25 | 5432 | `		zCur = zTag;` |
|     77 | 5433 | `		while(zTag < zEnd ){` |
|     77 | 5434 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5435 | `				/* UTF-8 stream */` |
|      5 | 5436 | `				zTag++;` |
|      9 | 5437 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5438 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5439 | `				break;` |
|    ! 0 | 5440 | `			}else{` |
|     49 | 5441 | `				zTag++;` |
|      - | 5442 | `			}` |
|      1 | 5443 | `		}` |
|     25 | 5444 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5445 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5446 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5447 | `		if( sTag.nByte > 0 ){` |
|      - | 5448 | `			SyString *aEntry,*pEntry;` |
|      - | 5449 | `			sxi32 rc;` |
|      - | 5450 | `			sxu32 n;` |
|      - | 5451 | `			/* Perform the lookup */` |
|     25 | 5452 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5453 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5454 | `				pEntry = &aEntry[n];` |
|      - | 5455 | `				/* Do the comparison */` |
|     25 | 5456 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5457 | `				if( !rc ){` |
|     21 | 5458 | `					return SXRET_OK;` |
|      - | 5459 | `				}` |
|      3 | 5460 | `			}` |
|      2 | 5461 | `		}` |
|      2 | 5462 | `	}` |
|      - | 5463 | `	/* No such tag */` |
|     17 | 5464 | `	return SXERR_NOTFOUND;` |
|     19 | 5465 | `}` |
|      - | 5466 | `/*` |
|      - | 5467 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5468 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5469 | ` * Refer to [strip_tags()].` |
|      - | 5470 | ` */` |
|     16 | 5471 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5472 | `{` |
|     17 | 5473 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5474 | `	const char *zPtr,*zTag;` |
|      - | 5475 | `	SySet sSet;` |
|      - | 5476 | `	/* initialize the set of allowed tags */` |
|     17 | 5477 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5478 | `	if( nTaglen > 0 ){` |
|      - | 5479 | `		/* Set of allowed tags */` |
|     11 | 5480 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5481 | `	}` |
|      - | 5482 | `	/* Set the empty string */` |
|     17 | 5483 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5484 | `	/* Start processing */` |
|     26 | 5485 | `	for(;;){` |
|     53 | 5486 | `		if(zIn >= zEnd){` |
|      - | 5487 | `			/* No more input to process */` |
|     15 | 5488 | `			break;` |
|      - | 5489 | `		}` |
|     39 | 5490 | `		zPtr = zIn;` |
|      - | 5491 | `		/* Find a tag */` |
|    133 | 5492 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5493 | `			zIn++;` |
|      1 | 5494 | `		}` |
|     39 | 5495 | `		if( zIn > zPtr ){` |
|      - | 5496 | `			/* Consume raw input */` |
|     21 | 5497 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5498 | `		}` |
|      - | 5499 | `		/* Ignore trailing null bytes */` |
|     39 | 5500 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5501 | `			zIn++;` |
|    ! 0 | 5502 | `		}` |
|     39 | 5503 | `		if(zIn >= zEnd){` |
|      - | 5504 | `			/* No more input to process */` |
|      3 | 5505 | `			break;` |
|      - | 5506 | `		}` |
|     37 | 5507 | `		if( zIn[0] == '<' ){` |
|      - | 5508 | `			sxi32 rc;` |
|     37 | 5509 | `			zTag = zIn++;` |
|      - | 5510 | `			/* Delimit the tag */` |
|    127 | 5511 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5512 | `				zIn++;` |
|      1 | 5513 | `			}` |
|     37 | 5514 | `			if( zIn < zEnd ){` |
|     37 | 5515 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5516 | `			}` |
|      - | 5517 | `			/* Query the set */` |
|     37 | 5518 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5519 | `			if( rc == SXRET_OK ){` |
|      - | 5520 | `				/* Keep the tag */` |
|     21 | 5521 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5522 | `			}` |
|     18 | 5523 | `		}` |
|      1 | 5524 | `	}` |
|      - | 5525 | `	/* Cleanup */` |
|     17 | 5526 | `	SySetRelease(&sSet);` |
|     17 | 5527 | `	return SXRET_OK;` |
|      1 | 5528 | `}` |
|      - | 5529 | `/*` |
|      - | 5530 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5531 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5532 | ` * Parameters` |
|      - | 5533 | ` *  $str` |
|      - | 5534 | ` *  The input string.` |
|      - | 5535 | ` * $allowable_tags` |
|      - | 5536 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5537 | ` * Return` |
|      - | 5538 | ` *  Returns the stripped string.` |
|      - | 5539 | ` */` |
|     16 | 5540 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5541 | `{` |
|     17 | 5542 | `	const char *zTaglist = 0;` |
|      - | 5543 | `	const char *zString;` |
|     17 | 5544 | `	int nTaglen = 0;` |
|      - | 5545 | `	int nLen;` |
|     17 | 5546 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5547 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 5548 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5549 | `		return PH7_OK;` |
|      - | 5550 | `	}` |
|      - | 5551 | `	/* Point to the raw string */` |
|     15 | 5552 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5553 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5554 | `		/* Allowed tag */` |
|     11 | 5555 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5556 | `	}` |
|      - | 5557 | `	/* Process input */` |
|     15 | 5558 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5559 | `	return PH7_OK;` |
|      9 | 5560 | `}` |
|      - | 5561 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5562 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5563 | `/*` |
|      - | 5564 | ` * string str_shuffle(string $str)` |
|      - | 5565 |  |
|      - | 5566 | ` *  Randomly shuffles a string.` |
|      - | 5567 | ` * Parameters` |
|      - | 5568 | ` *  $str` |
|      - | 5569 | ` *   The input string.` |
|      - | 5570 | ` * Return` |
|      - | 5571 | ` *  Returns the shuffled string.` |
|      - | 5572 | ` */` |
|     12 | 5573 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5574 | `{` |
|      - | 5575 | `	const char *zString;` |
|      - | 5576 | `	int nLen,i,c;` |
|      - | 5577 | `	sxu32 iR;` |
|     13 | 5578 | `	if( nArg < 1 ){` |
|      - | 5579 | `		/* Missing arguments,return the empty string */` |
|      3 | 5580 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5581 | `		return PH7_OK;` |
|      - | 5582 | `	}` |
|      - | 5583 | `	/* Extract the target string */` |
|     11 | 5584 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5585 | `	if( nLen < 1 ){` |
|      - | 5586 | `		/* Nothing to shuffle */` |
|      3 | 5587 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5588 | `		return PH7_OK;` |
|      - | 5589 | `	}` |
|      - | 5590 | `	/* Shuffle the string */` |
|     43 | 5591 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5592 | `		/* Generate a random number first */` |
|     35 | 5593 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5594 | `		/* Extract a random offset */` |
|     35 | 5595 | `		c = zString[iR % nLen];` |
|      - | 5596 | `		/* Append it */` |
|     35 | 5597 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5598 | `	}` |
|      9 | 5599 | `	return PH7_OK;` |
|      7 | 5600 | `}` |
|      - | 5601 | `/*` |
|      - | 5602 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5603 | ` *  Convert a string to an array.` |
|      - | 5604 | ` * Parameters` |
|      - | 5605 | ` * $string` |
|      - | 5606 | ` *  The input string.` |
|      - | 5607 | ` * $split_length` |
|      - | 5608 | ` *  Maximum length of the chunk.` |
|      - | 5609 | ` * Return` |
|      - | 5610 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5611 | ` *  except possibly the last one which may be shorter.` |
|      - | 5612 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5613 | ` *  as the first (and only) array element.` |
|      - | 5614 | ` *  An empty string returns an empty array.` |
|      - | 5615 | ` * Errors` |
|      - | 5616 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5617 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5618 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5619 | ` */` |
|     28 | 5620 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5621 | `{` |
|      - | 5622 | `	const char *zString,*zEnd;` |
|      - | 5623 | `	ph7_value *pArray,*pValue;` |
|      - | 5624 | `	int split_len;` |
|      - | 5625 | `	int nLen;` |
|     33 | 5626 | `	if( nArg < 1 ){` |
|      4 | 5627 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5628 | `			"ArgumentCountError",` |
|      - | 5629 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5630 | `			nArg` |
|      - | 5631 | `			);` |
|      - | 5632 | `	}` |
|      - | 5633 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 5634 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 5635 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5636 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5637 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5638 | `			"TypeError",` |
|      - | 5639 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5640 | `			ph7_type_name(apArg[0])` |
|      - | 5641 | `			);` |
|      - | 5642 | `	}` |
|      - | 5643 | `	/* Point to the target string */` |
|     27 | 5644 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 5645 | `	split_len = (int)sizeof(char);` |
|     27 | 5646 | `	if( nArg > 1 ){` |
|      - | 5647 | `		/* Split length */` |
|     17 | 5648 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 5649 | `		if( split_len < 1 ){` |
|      6 | 5650 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5651 | `				"ValueError",` |
|      - | 5652 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5653 | `				);` |
|      - | 5654 | `		}` |
|     11 | 5655 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5656 | `			split_len = nLen;` |
|      1 | 5657 | `		}` |
|      5 | 5658 | `	}` |
|      - | 5659 | `	/* Create the array and the scalar value */` |
|     21 | 5660 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 5661 | `	/*Chunk value */` |
|     21 | 5662 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 5663 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 5664 | `		/* Return FALSE */` |
|    ! 0 | 5665 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5666 | `		return PH7_OK;` |
|      - | 5667 | `	}` |
|      - | 5668 | `	/* Point to the end of the string */` |
|     21 | 5669 | `	zEnd = &zString[nLen];` |
|      - | 5670 | `	/* Perform the requested operation */` |
|     48 | 5671 | `	for(;;){` |
|      - | 5672 | `		int nMax;` |
|     59 | 5673 | `		if( zString >= zEnd ){` |
|      - | 5674 | `			/* No more input to process */` |
|     21 | 5675 | `			break;` |
|      - | 5676 | `		}` |
|     39 | 5677 | `		nMax = (int)(zEnd-zString);` |
|     39 | 5678 | `		if( nMax < split_len ){` |
|      3 | 5679 | `			split_len = nMax;` |
|      1 | 5680 | `		}` |
|      - | 5681 | `		/* Copy the current chunk */` |
|     39 | 5682 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 5683 | `		/* Insert it */` |
|     39 | 5684 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 5685 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5686 | `		}` |
|      - | 5687 | `		/* reset the string cursor */` |
|     39 | 5688 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 5689 | `		/* Update position */` |
|     39 | 5690 | `		zString += split_len;` |
|      1 | 5691 | `	}` |
|      - | 5692 | `	/*` |
|      - | 5693 | `	 * Return the array.` |
|      - | 5694 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 5695 | `	 * upon we return from this function.` |
|      - | 5696 | `	 */` |
|     21 | 5697 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 5698 | `	return PH7_OK;` |
|     19 | 5699 | `}` |
|      - | 5700 | `/*` |
|      - | 5701 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 5702 | ` * Refer to [strspn()].` |
|      - | 5703 | ` */` |
|     28 | 5704 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 5705 | `{` |
|     29 | 5706 | `	const char *zIn = *pzIn;` |
|      - | 5707 | `	const char *zPtr;` |
|      - | 5708 | `	/* Ignore leading white spaces */` |
|     29 | 5709 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 5710 | `		zIn++;` |
|    ! 0 | 5711 | `	}` |
|     29 | 5712 | `	if( zIn >= zEnd ){` |
|      - | 5713 | `		/* End of input */` |
|    ! 0 | 5714 | `		return SXERR_EOF;` |
|      - | 5715 | `	}` |
|     29 | 5716 | `	zPtr = zIn;` |
|      - | 5717 | `	/* Extract the token */` |
|    201 | 5718 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 5719 | `		zIn++;` |
|      1 | 5720 | `	}` |
|     29 | 5721 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5722 | `	/* Synchronize pointers */` |
|     29 | 5723 | `	*pzIn = zIn;` |
|      - | 5724 | `	/* Return to the caller */` |
|     29 | 5725 | `	return SXRET_OK;` |
|     15 | 5726 | `}` |
|      - | 5727 | `/*` |
|      - | 5728 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 5729 | ` * return the longest match.` |
|      - | 5730 | ` * Refer to [strspn()].` |
|      - | 5731 | ` */` |
|     18 | 5732 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5733 | `{` |
|     19 | 5734 | `	const char *zEnd = &zString[nLen];` |
|     19 | 5735 | `	const char *zIn = zString;` |
|      - | 5736 | `	int i,c;` |
|     45 | 5737 | `	for(;;){` |
|     91 | 5738 | `		if( zString >= zEnd ){` |
|      7 | 5739 | `			break;` |
|      - | 5740 | `		}` |
|      - | 5741 | `		/* Extract current character */` |
|     85 | 5742 | `		c = zString[0];` |
|      - | 5743 | `		/* Perform the lookup */` |
|    383 | 5744 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 5745 | `			if( c == zMask[i] ){` |
|      - | 5746 | `				/* Character found */` |
|     73 | 5747 | `				break;` |
|      - | 5748 | `			}` |
|    150 | 5749 | `		}` |
|     85 | 5750 | `		if( i >= nMaskLen ){` |
|      - | 5751 | `			/* Character not in the current mask,break immediately */` |
|     13 | 5752 | `			break;` |
|      - | 5753 | `		}` |
|      - | 5754 | `		/* Advance cursor */` |
|     73 | 5755 | `		zString++;` |
|      1 | 5756 | `	}` |
|      - | 5757 | `	/* Longest match */` |
|     19 | 5758 | `	return (int)(zString-zIn);` |
|      1 | 5759 | `}` |
|      - | 5760 | `/*` |
|      - | 5761 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 5762 | ` * Refer to [strcspn()].` |
|      - | 5763 | ` */` |
|     10 | 5764 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 5765 | `{` |
|     11 | 5766 | `	const char *zEnd = &zString[nLen];` |
|     11 | 5767 | `	const char *zIn = zString;` |
|      - | 5768 | `	int i,c;` |
|     12 | 5769 | `	for(;;){` |
|     25 | 5770 | `		if( zString >= zEnd ){` |
|      3 | 5771 | `			break;` |
|      - | 5772 | `		}` |
|      - | 5773 | `		/* Extract current character */` |
|     23 | 5774 | `		c = zString[0];` |
|      - | 5775 | `		/* Perform the lookup */` |
|     51 | 5776 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 5777 | `			if( c == zMask[i] ){` |
|      9 | 5778 | `				break;` |
|      - | 5779 | `			}` |
|     15 | 5780 | `		}` |
|     23 | 5781 | `		if( i < nMaskLen ){` |
|      - | 5782 | `			/* Character in the current mask,break immediately */` |
|      9 | 5783 | `			break;` |
|      - | 5784 | `		}` |
|      - | 5785 | `		/* Advance cursor */` |
|     15 | 5786 | `		zString++;` |
|      1 | 5787 | `	}` |
|      - | 5788 | `	/* Longest match */` |
|     11 | 5789 | `	return (int)(zString-zIn);` |
|      1 | 5790 | `}` |
|      - | 5791 | `/*` |
|      - | 5792 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5793 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 5794 | ` *  of characters contained within a given mask.` |
|      - | 5795 | ` * Parameters` |
|      - | 5796 | ` * $str` |
|      - | 5797 | ` *  The input string.` |
|      - | 5798 | ` * $mask` |
|      - | 5799 | ` *  The list of allowable characters.` |
|      - | 5800 | ` * $start` |
|      - | 5801 | ` *  The position in subject to start searching.` |
|      - | 5802 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5803 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5804 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5805 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5806 | ` *  start'th position from the end of subject.` |
|      - | 5807 | ` * $length` |
|      - | 5808 | ` *  The length of the segment from subject to examine.` |
|      - | 5809 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5810 | ` *  characters after the starting position.` |
|      - | 5811 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5812 | ` *  position up to length characters from the end of subject.` |
|      - | 5813 | ` * Return` |
|      - | 5814 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 5815 | ` * in mask.` |
|      - | 5816 | ` */` |
|     26 | 5817 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5818 | `{` |
|      - | 5819 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5820 | `	int iMasklen,iLen;` |
|      - | 5821 | `	SyString sToken;` |
|     27 | 5822 | `	int iCount = 0;` |
|      - | 5823 | `	int rc;` |
|     27 | 5824 | `	if( nArg < 2 ){` |
|      - | 5825 | `		/* Missing agruments,return zero */` |
|      3 | 5826 | `		ph7_result_int(pCtx,0);` |
|      3 | 5827 | `		return PH7_OK;` |
|      - | 5828 | `	}` |
|      - | 5829 | `	/* Extract the target string */` |
|     25 | 5830 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5831 | `	/* Extract the mask */` |
|     25 | 5832 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 5833 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 5834 | `		/* Nothing to process,return zero */` |
|      7 | 5835 | `		ph7_result_int(pCtx,0);` |
|      7 | 5836 | `		return PH7_OK;` |
|      - | 5837 | `	}` |
|     19 | 5838 | `	if( nArg > 2 ){` |
|      - | 5839 | `		int nOfft;` |
|      - | 5840 | `		/* Extract the offset */` |
|      9 | 5841 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 5842 | `		if( nOfft < 0 ){` |
|    ! 0 | 5843 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5844 | `			if( zBase > zString ){` |
|    ! 0 | 5845 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5846 | `				zString = zBase;` |
|    ! 0 | 5847 | `			}else{` |
|      - | 5848 | `				/* Invalid offset */` |
|    ! 0 | 5849 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5850 | `				return PH7_OK;` |
|      - | 5851 | `			}` |
|    ! 0 | 5852 | `		}else{` |
|      9 | 5853 | `			if( nOfft >= iLen ){` |
|      - | 5854 | `				/* Invalid offset */` |
|    ! 0 | 5855 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5856 | `				return PH7_OK;` |
|    ! 0 | 5857 | `			}else{` |
|      - | 5858 | `				/* Update offset */` |
|      9 | 5859 | `				zString += nOfft;` |
|      9 | 5860 | `				iLen -= nOfft;` |
|      - | 5861 | `			}` |
|      - | 5862 | `		}` |
|      9 | 5863 | `		if( nArg > 3 ){` |
|      - | 5864 | `			int iUserlen;` |
|      - | 5865 | `			/* Extract the desired length */` |
|      9 | 5866 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 5867 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 5868 | `				iLen = iUserlen;` |
|      2 | 5869 | `			}` |
|      4 | 5870 | `		}` |
|      4 | 5871 | `	}` |
|      - | 5872 | `	/* Point to the end of the string */` |
|     19 | 5873 | `	zEnd = &zString[iLen];` |
|      - | 5874 | `	/* Extract the first non-space token */` |
|     19 | 5875 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 5876 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5877 | `		/* Compare against the current mask */` |
|     19 | 5878 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 5879 | `	}` |
|      - | 5880 | `	/* Longest match */` |
|     19 | 5881 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 5882 | `	return PH7_OK;` |
|     14 | 5883 | `}` |
|      - | 5884 | `/*` |
|      - | 5885 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 5886 | ` *  Find length of initial segment not matching mask.` |
|      - | 5887 | ` * Parameters` |
|      - | 5888 | ` * $str` |
|      - | 5889 | ` *  The input string.` |
|      - | 5890 | ` * $mask` |
|      - | 5891 | ` *  The list of not allowed characters.` |
|      - | 5892 | ` * $start` |
|      - | 5893 | ` *  The position in subject to start searching.` |
|      - | 5894 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 5895 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 5896 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 5897 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 5898 | ` *  start'th position from the end of subject.` |
|      - | 5899 | ` * $length` |
|      - | 5900 | ` *  The length of the segment from subject to examine.` |
|      - | 5901 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 5902 | ` *  characters after the starting position.` |
|      - | 5903 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 5904 | ` *  position up to length characters from the end of subject.` |
|      - | 5905 | ` * Return` |
|      - | 5906 | ` *  Returns the length of the segment as an integer.` |
|      - | 5907 | ` */` |
|     16 | 5908 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5909 | `{` |
|      - | 5910 | `	const char *zString,*zMask,*zEnd;` |
|      - | 5911 | `	int iMasklen,iLen;` |
|      - | 5912 | `	SyString sToken;` |
|     17 | 5913 | `	int iCount = 0;` |
|      - | 5914 | `	int rc;` |
|     17 | 5915 | `	if( nArg < 2 ){` |
|      - | 5916 | `		/* Missing agruments,return zero */` |
|      3 | 5917 | `		ph7_result_int(pCtx,0);` |
|      3 | 5918 | `		return PH7_OK;` |
|      - | 5919 | `	}` |
|      - | 5920 | `	/* Extract the target string */` |
|     15 | 5921 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5922 | `	/* Extract the mask */` |
|     15 | 5923 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 5924 | `	if( iLen < 1 ){` |
|      - | 5925 | `		/* Nothing to process,return zero */` |
|    ! 0 | 5926 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5927 | `		return PH7_OK;` |
|      - | 5928 | `	}` |
|     15 | 5929 | `	if( iMasklen < 1 ){` |
|      - | 5930 | `		/* No given mask,return the string length */` |
|      3 | 5931 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 5932 | `		return PH7_OK;` |
|      - | 5933 | `	}` |
|     13 | 5934 | `	if( nArg > 2 ){` |
|      - | 5935 | `		int nOfft;` |
|      - | 5936 | `		/* Extract the offset */` |
|     11 | 5937 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 5938 | `		if( nOfft < 0 ){` |
|    ! 0 | 5939 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 5940 | `			if( zBase > zString ){` |
|    ! 0 | 5941 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 5942 | `				zString = zBase;` |
|    ! 0 | 5943 | `			}else{` |
|      - | 5944 | `				/* Invalid offset */` |
|    ! 0 | 5945 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 5946 | `				return PH7_OK;` |
|      - | 5947 | `			}` |
|    ! 0 | 5948 | `		}else{` |
|     11 | 5949 | `			if( nOfft >= iLen ){` |
|      - | 5950 | `				/* Invalid offset */` |
|      3 | 5951 | `				ph7_result_int(pCtx,0);` |
|      3 | 5952 | `				return PH7_OK;` |
|    ! 0 | 5953 | `			}else{` |
|      - | 5954 | `				/* Update offset */` |
|      9 | 5955 | `				zString += nOfft;` |
|      9 | 5956 | `				iLen -= nOfft;` |
|      - | 5957 | `			}` |
|      - | 5958 | `		}` |
|      9 | 5959 | `		if( nArg > 3 ){` |
|      - | 5960 | `			int iUserlen;` |
|      - | 5961 | `			/* Extract the desired length */` |
|    ! 0 | 5962 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 5963 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 5964 | `				iLen = iUserlen;` |
|    ! 0 | 5965 | `			}` |
|    ! 0 | 5966 | `		}` |
|      4 | 5967 | `	}` |
|      - | 5968 | `	/* Point to the end of the string */` |
|     11 | 5969 | `	zEnd = &zString[iLen];` |
|      - | 5970 | `	/* Extract the first non-space token */` |
|     11 | 5971 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 5972 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 5973 | `		/* Compare against the current mask */` |
|     11 | 5974 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 5975 | `	}` |
|      - | 5976 | `	/* Longest match */` |
|     11 | 5977 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 5978 | `	return PH7_OK;` |
|      9 | 5979 | `}` |
|      - | 5980 | `/*` |
|      - | 5981 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 5982 | ` *  Search a string for any of a set of characters.` |
|      - | 5983 | ` * Parameters` |
|      - | 5984 | ` *  $haystack` |
|      - | 5985 | ` *   The string where char_list is looked for.` |
|      - | 5986 | ` *  $char_list` |
|      - | 5987 | ` *   This parameter is case sensitive.` |
|      - | 5988 | ` * Return` |
|      - | 5989 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 5990 | ` */` |
|      6 | 5991 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5992 | `{` |
|      - | 5993 | `	const char *zString,*zList,*zEnd;` |
|      - | 5994 | `	int iLen,iListLen,i,c;` |
|      - | 5995 | `	sxu32 nOfft,nMax;` |
|      - | 5996 | `	sxi32 rc;` |
|      7 | 5997 | `	if( nArg < 2 ){` |
|      - | 5998 | `		/* Missing arguments,return FALSE */` |
|      3 | 5999 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6000 | `		return PH7_OK;` |
|      - | 6001 | `	}` |
|      - | 6002 | `	/* Extract the haystack and the char list */` |
|      5 | 6003 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 6004 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 6005 | `	if( iLen < 1 ){` |
|      - | 6006 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6007 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6008 | `		return PH7_OK;` |
|      - | 6009 | `	}` |
|      - | 6010 | `	/* Point to the end of the string */` |
|      5 | 6011 | `	zEnd = &zString[iLen];` |
|      5 | 6012 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6013 | `	/* perform the requested operation */` |
|     15 | 6014 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6015 | `		c = zList[i];` |
|     11 | 6016 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6017 | `		if( rc == SXRET_OK ){` |
|      5 | 6018 | `			if( nMax < nOfft ){` |
|      3 | 6019 | `				nOfft = nMax;` |
|      1 | 6020 | `			}` |
|      2 | 6021 | `		}` |
|      6 | 6022 | `	}` |
|      5 | 6023 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6024 | `		/* No such substring,return FALSE */` |
|      3 | 6025 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6026 | `	}else{` |
|      - | 6027 | `		/* Return the substring */` |
|      3 | 6028 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6029 | `	}` |
|      5 | 6030 | `	return PH7_OK;` |
|      4 | 6031 | `}` |
|      - | 6032 | `/* SPDX-SnippetBegin */` |
|      - | 6033 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6034 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6035 | `/*` |
|      - | 6036 | ` * string soundex(string $str)` |
|      - | 6037 | ` *  Calculate the soundex key of a string.` |
|      - | 6038 | ` * Parameters` |
|      - | 6039 | ` *  $str` |
|      - | 6040 | ` *   The input string.` |
|      - | 6041 | ` * Return` |
|      - | 6042 | ` *  Returns the soundex key as a string.` |
|      - | 6043 | ` * Note:` |
|      - | 6044 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6045 | ` * source tree.` |
|      - | 6046 | ` */` |
|     20 | 6047 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6048 | `{` |
|      - | 6049 | `	const unsigned char *zIn;` |
|      - | 6050 | `	char zResult[8];` |
|      - | 6051 | `	int i, j;` |
|      - | 6052 | `	static const unsigned char iCode[] = {` |
|      - | 6053 |  |
|      - | 6054 |  |
|      - | 6055 |  |
|      - | 6056 |  |
|      - | 6057 |  |
|      - | 6058 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6059 |  |
|      - | 6060 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6061 | `	};` |
|     21 | 6062 | `	if( nArg < 1 ){` |
|      - | 6063 | `		/* Missing arguments,return the empty string */` |
|      3 | 6064 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6065 | `		return PH7_OK;` |
|      - | 6066 | `	}` |
|     19 | 6067 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 6068 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 6069 | `	if( zIn[i] ){` |
|     17 | 6070 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6071 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6072 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6073 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6074 | `			if( code>0 ){` |
|     45 | 6075 | `				if( code!=prevcode ){` |
|     33 | 6076 | `					prevcode = (unsigned char)code;` |
|     33 | 6077 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6078 | `				}` |
|     23 | 6079 | `			}else{` |
|     49 | 6080 | `				prevcode = 0;` |
|      - | 6081 | `			}` |
|     47 | 6082 | `		}` |
|     33 | 6083 | `		while( j<4 ){` |
|     17 | 6084 | `			zResult[j++] = '0';` |
|      1 | 6085 | `		}` |
|     17 | 6086 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6087 | `	}else{` |
|      3 | 6088 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 6089 | `	}` |
|     19 | 6090 | `	return PH7_OK;` |
|     11 | 6091 | `}` |
|      - | 6092 | `/* SPDX-SnippetEnd */` |
|      - | 6093 | `/*` |
|      - | 6094 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6095 | ` *  Wraps a string to a given number of characters.` |
|      - | 6096 | ` * Parameters` |
|      - | 6097 | ` *  $str` |
|      - | 6098 | ` *   The input string.` |
|      - | 6099 | ` * $width` |
|      - | 6100 | ` *  The column width.` |
|      - | 6101 | ` * $break` |
|      - | 6102 | ` *  The line is broken using the optional break parameter.` |
|      - | 6103 | ` * Return` |
|      - | 6104 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6105 | ` */` |
|     14 | 6106 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6107 | `{` |
|      - | 6108 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 6109 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 6110 | `	if( nArg < 1 ){` |
|      - | 6111 | `		/* Missing arguments,return the empty string */` |
|      3 | 6112 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6113 | `		return PH7_OK;` |
|      - | 6114 | `	}` |
|      - | 6115 | `	/* Extract the input string */` |
|     13 | 6116 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 6117 | `	if( iLen < 1 ){` |
|      - | 6118 | `		/* Nothing to process,return the empty string */` |
|      3 | 6119 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6120 | `		return PH7_OK;` |
|      - | 6121 | `	}` |
|      - | 6122 | `	/* Chunk length */` |
|     11 | 6123 | `	iChunk = 75;` |
|     11 | 6124 | `	iBreaklen = 0;` |
|     11 | 6125 | `	zBreak = ""; /* cc warning */` |
|     11 | 6126 | `	if( nArg > 1 ){` |
|     11 | 6127 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 6128 | `		if( iChunk < 1 ){` |
|    ! 0 | 6129 | `			iChunk = 75;` |
|    ! 0 | 6130 | `		}` |
|     11 | 6131 | `		if( nArg > 2 ){` |
|      3 | 6132 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 6133 | `		}` |
|      5 | 6134 | `	}` |
|     11 | 6135 | `	if( iBreaklen < 1 ){` |
|      - | 6136 | `		/* Set a default column break */` |
|      - | 6137 | `#ifdef __WINNT__` |
|      1 | 6138 | `		zBreak = "\r\n";` |
|      1 | 6139 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 6140 | `#else` |
|      8 | 6141 | `		zBreak = "\n";` |
|      8 | 6142 | `		iBreaklen = (int)sizeof(char);` |
|      - | 6143 | `#endif` |
|      4 | 6144 | `	}` |
|      - | 6145 | `	/* Perform the requested operation */` |
|     11 | 6146 | `	zEnd = &zIn[iLen];` |
|     41 | 6147 | `	for(;;){` |
|      - | 6148 | `		int nMax;` |
|     47 | 6149 | `		if( zIn >= zEnd ){` |
|      - | 6150 | `			/* No more input to process */` |
|     11 | 6151 | `			break;` |
|      - | 6152 | `		}` |
|     37 | 6153 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 6154 | `		if( iChunk > nMax ){` |
|     11 | 6155 | `			iChunk = nMax;` |
|      5 | 6156 | `		}` |
|      - | 6157 | `		/* Append the column first */` |
|     37 | 6158 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 6159 | `		/* Advance the cursor */` |
|     37 | 6160 | `		zIn += iChunk;` |
|     37 | 6161 | `		if( zIn < zEnd ){` |
|      - | 6162 | `			/* Append the line break */` |
|     27 | 6163 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 6164 | `		}` |
|      1 | 6165 | `	}` |
|     11 | 6166 | `	return PH7_OK;` |
|      8 | 6167 | `}` |
|      - | 6168 | `/*` |
|      - | 6169 | ` * Check if the given character is a member of the given mask.` |
|      - | 6170 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6171 | ` * Refer to [strtok()].` |
|      - | 6172 | ` */` |
|     30 | 6173 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6174 | `{` |
|      - | 6175 | `	int i;` |
|     57 | 6176 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6177 | `		if( c == zMask[i] ){` |
|     13 | 6178 | `			if( pOfft ){` |
|      5 | 6179 | `				*pOfft = i;` |
|      2 | 6180 | `			}` |
|     13 | 6181 | `			return TRUE;` |
|      - | 6182 | `		}` |
|     14 | 6183 | `	}` |
|     19 | 6184 | `	return FALSE;` |
|     16 | 6185 | `}` |
|      - | 6186 | `/*` |
|      - | 6187 | ` * Extract a single token from the input stream.` |
|      - | 6188 | ` * Refer to [strtok()].` |
|      - | 6189 | ` */` |
|      6 | 6190 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6191 | `{` |
|      7 | 6192 | `	const char *zIn = *pzIn;` |
|      - | 6193 | `	const char *zPtr;` |
|      - | 6194 | `	/* Ignore leading delimiter */` |
|     11 | 6195 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6196 | `		zIn++;` |
|      1 | 6197 | `	}` |
|      7 | 6198 | `	if( zIn >= zEnd ){` |
|      - | 6199 | `		/* End of input */` |
|    ! 0 | 6200 | `		return SXERR_EOF;` |
|      - | 6201 | `	}` |
|      7 | 6202 | `	zPtr = zIn;` |
|      - | 6203 | `	/* Extract the token */` |
|     13 | 6204 | `	while( zIn < zEnd ){` |
|     11 | 6205 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6206 | `			/* UTF-8 stream */` |
|    ! 0 | 6207 | `			zIn++;` |
|    ! 0 | 6208 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6209 | `		}else{` |
|     11 | 6210 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6211 | `				break;` |
|      - | 6212 | `			}` |
|      7 | 6213 | `			zIn++;` |
|      - | 6214 | `		}` |
|      1 | 6215 | `	}` |
|      7 | 6216 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6217 | `	/* Update the cursor */` |
|      7 | 6218 | `	*pzIn = zIn;` |
|      - | 6219 | `	/* Return to the caller */` |
|      7 | 6220 | `	return SXRET_OK;` |
|      4 | 6221 | `}` |
|      - | 6222 | `/* strtok auxiliary private data */` |
|      - | 6223 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6224 | `struct strtok_aux_data` |
|      - | 6225 | `{` |
|      - | 6226 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6227 | `	const char *zIn;   /* Current input stream */` |
|      - | 6228 | `	const char *zEnd;  /* End of input */` |
|      - | 6229 | `};` |
|      - | 6230 | `/*` |
|      - | 6231 | ` * string strtok(string $str,string $token)` |
|      - | 6232 | ` * string strtok(string $token)` |
|      - | 6233 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6234 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6235 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6236 | ` *  words by using the space character as the token.` |
|      - | 6237 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6238 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6239 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6240 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6241 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6242 | ` *  the argument are found.` |
|      - | 6243 | ` * Parameters` |
|      - | 6244 | ` *  $str` |
|      - | 6245 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6246 | ` * $token` |
|      - | 6247 | ` *  The delimiter used when splitting up str.` |
|      - | 6248 | ` * Return` |
|      - | 6249 | ` *   Current token or FALSE on EOF.` |
|      - | 6250 | ` */` |
|      8 | 6251 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6252 | `{` |
|      - | 6253 | `	strtok_aux_data *pAux;` |
|      - | 6254 | `	const char *zMask;` |
|      - | 6255 | `	SyString sToken;` |
|      - | 6256 | `	int nMasklen;` |
|      - | 6257 | `	sxi32 rc;` |
|      9 | 6258 | `	if( nArg < 2 ){` |
|      - | 6259 | `		/* Extract top aux data */` |
|      7 | 6260 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 6261 | `		if( pAux == 0 ){` |
|      - | 6262 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6263 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6264 | `			return PH7_OK;` |
|      - | 6265 | `		}` |
|      7 | 6266 | `		nMasklen = 0;` |
|      7 | 6267 | `		zMask = ""; /* cc warning */` |
|      7 | 6268 | `		if( nArg > 0 ){` |
|      - | 6269 | `			/* Extract the mask */` |
|      5 | 6270 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6271 | `		}` |
|      7 | 6272 | `		if( nMasklen < 1 ){` |
|      - | 6273 | `			/* Invalid mask,return FALSE */` |
|      3 | 6274 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 6275 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 6276 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 6277 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6278 | `			return PH7_OK;` |
|      - | 6279 | `		}` |
|      - | 6280 | `		/* Extract the token */` |
|      5 | 6281 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6282 | `		if( rc != SXRET_OK ){` |
|      - | 6283 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6284 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6285 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6286 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6287 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6288 | `		}else{` |
|      - | 6289 | `			/* Return the extracted token */` |
|      5 | 6290 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6291 | `		}` |
|      3 | 6292 | `	}else{` |
|      - | 6293 | `		const char *zInput,*zCur;` |
|      - | 6294 | `		char *zDup;` |
|      - | 6295 | `		int nLen;` |
|      - | 6296 | `		/* Extract the raw input */` |
|      3 | 6297 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6298 | `		if( nLen < 1 ){` |
|      - | 6299 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6300 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6301 | `			return PH7_OK;` |
|      - | 6302 | `		}` |
|      - | 6303 | `		/* Extract the mask */` |
|      3 | 6304 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6305 | `		if( nMasklen < 1 ){` |
|      - | 6306 | `			/* Set a default mask */` |
|      - | 6307 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6308 | `			zMask = TOK_MASK;` |
|    ! 0 | 6309 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6310 | `#undef TOK_MASK` |
|    ! 0 | 6311 | `		}` |
|      - | 6312 | `		/* Extract a single token */` |
|      3 | 6313 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6314 | `		if( rc != SXRET_OK ){` |
|      - | 6315 | `			/* Empty input */` |
|    ! 0 | 6316 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6317 | `			return PH7_OK;` |
|    ! 0 | 6318 | `		}else{` |
|      - | 6319 | `			/* Return the extracted token */` |
|      3 | 6320 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6321 | `		}` |
|      - | 6322 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6323 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6324 | `		if( pAux ){` |
|      3 | 6325 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6326 | `			if( nLen < 1 ){` |
|    ! 0 | 6327 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6328 | `				return PH7_OK;` |
|      - | 6329 | `			}` |
|      - | 6330 | `			/* Duplicate input */` |
|      3 | 6331 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6332 | `			if( zDup  ){` |
|      3 | 6333 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6334 | `				/* Register the aux data */` |
|      3 | 6335 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6336 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6337 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6338 | `			}` |
|      1 | 6339 | `		}` |
|      - | 6340 | `	}` |
|      7 | 6341 | `	return PH7_OK;` |
|      5 | 6342 | `}` |
|      - | 6343 | `/*` |
|      - | 6344 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6345 | ` *  Pad a string to a certain length with another string` |
|      - | 6346 | ` * Parameters` |
|      - | 6347 | ` *  $input` |
|      - | 6348 | ` *   The input string.` |
|      - | 6349 | ` * $pad_length` |
|      - | 6350 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6351 | ` *   string, no padding takes place.` |
|      - | 6352 | ` * $pad_string` |
|      - | 6353 | ` *   Note:` |
|      - | 6354 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6355 | ` *    divided by the pad_string's length.` |
|      - | 6356 | ` * $pad_type` |
|      - | 6357 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6358 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6359 | ` * Return` |
|      - | 6360 | ` *  The padded string.` |
|      - | 6361 | ` */` |
|     10 | 6362 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6363 | `{` |
|      - | 6364 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6365 | `	const char *zIn,*zPad;` |
|     11 | 6366 | `	if( nArg < 2 ){` |
|      - | 6367 | `		/* Missing arguments,return the empty string */` |
|      5 | 6368 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6369 | `		return PH7_OK;` |
|      - | 6370 | `	}` |
|      - | 6371 | `	/* Extract the target string */` |
|      7 | 6372 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6373 | `	/* Padding length */` |
|      7 | 6374 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 6375 | `	if( iPadlen > 0 ){` |
|      5 | 6376 | `		iPadlen -= iLen;` |
|      2 | 6377 | `	}` |
|      7 | 6378 | `	if( iPadlen < 1  ){` |
|      - | 6379 | `		/* Return the string verbatim */` |
|      3 | 6380 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      3 | 6381 | `		return PH7_OK;` |
|      - | 6382 | `	}` |
|      5 | 6383 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 6384 | `	iStrpad = (int)sizeof(char);` |
|      5 | 6385 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 6386 | `	if( nArg > 2 ){` |
|      - | 6387 | `		/* Padding string */` |
|      5 | 6388 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 6389 | `		if( iStrpad < 1 ){` |
|      - | 6390 | `			/* Empty string */` |
|    ! 0 | 6391 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 6392 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 6393 | `		}` |
|      5 | 6394 | `		if( nArg > 3 ){` |
|      - | 6395 | `			/* Padd type */` |
|      5 | 6396 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6397 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6398 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6399 | `			}` |
|      2 | 6400 | `		}` |
|      2 | 6401 | `	}` |
|      5 | 6402 | `	iDiv = 1;` |
|      5 | 6403 | `	if( iType == 2 ){` |
|    ! 0 | 6404 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6405 | `	}` |
|      - | 6406 | `	/* Perform the requested operation */` |
|      5 | 6407 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6408 | `		jPad = iStrpad;` |
|      5 | 6409 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6410 | `			/* Padding */` |
|      5 | 6411 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6412 | `				break;` |
|      - | 6413 | `			}` |
|      3 | 6414 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6415 | `		}` |
|      3 | 6416 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6417 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6418 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6419 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6420 | `					jPad = iStrpad;` |
|    ! 0 | 6421 | `				}` |
|      3 | 6422 | `				if( jPad < 1){` |
|    ! 0 | 6423 | `					break;` |
|      - | 6424 | `				}` |
|      3 | 6425 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6426 | `			}` |
|      1 | 6427 | `		}` |
|      1 | 6428 | `	}` |
|      5 | 6429 | `	if( iLen > 0 ){` |
|      - | 6430 | `		/* Append the input string */` |
|      5 | 6431 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6432 | `	}` |
|      5 | 6433 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6434 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6435 | `			/* Padding */` |
|      5 | 6436 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6437 | `				break;` |
|      - | 6438 | `			}` |
|      3 | 6439 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6440 | `		}` |
|      5 | 6441 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6442 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6443 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6444 | `				jPad = iStrpad;` |
|    ! 0 | 6445 | `			}` |
|      3 | 6446 | `			if( jPad < 1){` |
|    ! 0 | 6447 | `				break;` |
|      - | 6448 | `			}` |
|      3 | 6449 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6450 | `		}` |
|      1 | 6451 | `	}` |
|      5 | 6452 | `	return PH7_OK;` |
|      6 | 6453 | `}` |
|      - | 6454 | `/*` |
|      - | 6455 | ` * String replacement private data.` |
|      - | 6456 | ` */` |
|      - | 6457 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6458 | `struct str_replace_data` |
|      - | 6459 | `{` |
|      - | 6460 | `	/* The following two fields are only used by the strtr function */` |
|      - | 6461 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 6462 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 6463 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 6464 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6465 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6466 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 6467 | `};` |
|      - | 6468 | `/*` |
|      - | 6469 | ` * Remove a substring.` |
|      - | 6470 | ` */` |
|      - | 6471 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6472 | `	for(;;){\` |
|      - | 6473 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6474 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6475 | `		++OFFT;\` |
|      - | 6476 | `	}\` |
|      - | 6477 | `}` |
|      - | 6478 | `/*` |
|      - | 6479 | ` * Shift right and insert algorithm.` |
|      - | 6480 | ` */` |
|      - | 6481 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6482 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6483 | `		for(;;){\` |
|      - | 6484 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6485 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6486 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6487 | `			--INLEN; \` |
|      - | 6488 | `		}\` |
|      - | 6489 | `		for(;;){\` |
|      - | 6490 | `				if(ELEN < 1) { break; }\` |
|      - | 6491 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6492 | `				OFFT++;\` |
|      - | 6493 | `				ENTRY++;\` |
|      - | 6494 | `				--ELEN;\` |
|      - | 6495 | `		}\` |
|      - | 6496 | `}` |
|      - | 6497 | `/*` |
|      - | 6498 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6499 | ` * replacement string [i.e: zReplace].` |
|      - | 6500 | ` */` |
|     38 | 6501 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6502 | `{` |
|     39 | 6503 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6504 | `	sxu32 n,m;` |
|     39 | 6505 | `	n = SyBlobLength(pWorker);` |
|     39 | 6506 | `	m = nOfft;` |
|      - | 6507 | `	/* Delete the old entry */` |
|    475 | 6508 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 6509 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 6510 | `	if( nReplen > 0 ){` |
|     33 | 6511 | `		sxi32 iRep = nReplen;` |
|      - | 6512 | `		sxi32 rc;` |
|      - | 6513 | `		/*` |
|      - | 6514 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6515 | `		 * string.` |
|      - | 6516 | `		 */` |
|     33 | 6517 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 6518 | `		if( rc != SXRET_OK ){` |
|      - | 6519 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 6520 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 6521 | `			return rc;` |
|      - | 6522 | `		}` |
|      - | 6523 | `		/* Perform the insertion now */` |
|     33 | 6524 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 6525 | `		n = SyBlobLength(pWorker);` |
|    163 | 6526 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 6527 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 6528 | `	}` |
|     39 | 6529 | `	return SXRET_OK;` |
|     20 | 6530 | `}` |
|      - | 6531 | `/*` |
|      - | 6532 | ` * String replacement walker callback.` |
|      - | 6533 | ` * The following callback is invoked for each array entry that hold` |
|      - | 6534 | ` * the replace string.` |
|      - | 6535 | ` * Refer to the strtr() implementation for more information.` |
|      - | 6536 | ` */` |
|      8 | 6537 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6538 | `{` |
|      9 | 6539 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 6540 | `	const char *zTarget,*zReplace;` |
|      - | 6541 | `	SyBlob *pWorker;` |
|      - | 6542 | `	int tLen,nLen;` |
|      - | 6543 | `	sxu32 nOfft;` |
|      - | 6544 | `	sxi32 rc;` |
|      - | 6545 | `	/* Point to the working buffer */` |
|      9 | 6546 | `	pWorker = pRepData->pWorker;` |
|      9 | 6547 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 6548 | `		/* Target and replace must be a string */` |
|      3 | 6549 | `		return PH7_OK;` |
|      - | 6550 | `	}` |
|      - | 6551 | `	/* Extract the target and the replace */` |
|      7 | 6552 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 6553 | `	if( tLen < 1 ){` |
|      - | 6554 | `		/* Empty target,return immediately */` |
|    ! 0 | 6555 | `		return PH7_OK;` |
|      - | 6556 | `	}` |
|      - | 6557 | `	/* Perform a pattern search */` |
|      7 | 6558 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 6559 | `	if( rc != SXRET_OK ){` |
|      - | 6560 | `		/* Pattern not found */` |
|    ! 0 | 6561 | `		return PH7_OK;` |
|      - | 6562 | `	}` |
|      - | 6563 | `	/* Extract the replace string */` |
|      7 | 6564 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 6565 | `	/* Perform the replace process */` |
|      7 | 6566 | `	rc = StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      7 | 6567 | `	if( rc != SXRET_OK ){` |
|      - | 6568 | `		/* Allocation failure: carry it out and stop the walk */` |
|    ! 0 | 6569 | `		pRepData->rc = rc;` |
|    ! 0 | 6570 | `		return rc;` |
|      - | 6571 | `	}` |
|      - | 6572 | `	/* All done */` |
|      7 | 6573 | `	return PH7_OK;` |
|      5 | 6574 | `}` |
|      - | 6575 | `/*` |
|      - | 6576 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6577 | ` * to collect search/replace string.` |
|      - | 6578 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6579 | ` */` |
|     26 | 6580 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6581 | `{` |
|     27 | 6582 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6583 | `	SyString sWorker;` |
|      - | 6584 | `	const char *zIn;` |
|      - | 6585 | `	int nByte;` |
|      - | 6586 | `	/* Extract a string representation of the given argument */` |
|     27 | 6587 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6588 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6589 | `	if( nByte > 0 ){` |
|      - | 6590 | `		char *zDup;` |
|      - | 6591 | `		/* Duplicate the chunk */` |
|     25 | 6592 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6593 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6594 | `			);` |
|     25 | 6595 | `		if( zDup == 0 ){` |
|      - | 6596 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 6597 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 6598 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 6599 | `			return SXERR_MEM;` |
|      - | 6600 | `		}` |
|     25 | 6601 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6602 | `		/* Save the chunk */` |
|     25 | 6603 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6604 | `	}` |
|      - | 6605 | `	/* Save for later processing */` |
|     27 | 6606 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6607 | `	/* All done */` |
|     13 | 6608 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6609 | `	return PH7_OK;` |
|     14 | 6610 | `}` |
|      - | 6611 | `/*` |
|      - | 6612 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6613 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6614 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6615 | ` * Parameters` |
|      - | 6616 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6617 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6618 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6619 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6620 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6621 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6622 | ` * $search` |
|      - | 6623 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6624 | ` *  to designate multiple needles.` |
|      - | 6625 | ` * $replace` |
|      - | 6626 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6627 | ` *  to designate multiple replacements.` |
|      - | 6628 | ` * $subject` |
|      - | 6629 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6630 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6631 | ` *  of subject, and the return value is an array as well.` |
|      - | 6632 | ` * $count (Not used)` |
|      - | 6633 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6634 | ` * Return` |
|      - | 6635 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6636 | ` */` |
|  24290 | 6637 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6638 | `{` |
|      - | 6639 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6640 | `	ProcStringMatch xMatch;` |
|      - | 6641 | `	const char *zIn,*zFunc;` |
|      - | 6642 | `	str_replace_data sRep;` |
|      - | 6643 | `	SyBlob sWorker;` |
|      - | 6644 | `	SySet sReplace;` |
|      - | 6645 | `	SySet sSearch;` |
|      - | 6646 | `	int rep_str;` |
|      - | 6647 | `	int nByte;` |
|      - | 6648 | `	sxi32 rc;` |
|  24295 | 6649 | `	if( nArg < 3 ){` |
|      - | 6650 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 6651 | `		ph7_result_null(pCtx);` |
|      7 | 6652 | `		return PH7_OK;` |
|      - | 6653 | `	}` |
|      - | 6654 | `	/* Initialize fields */` |
|  24289 | 6655 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  24289 | 6656 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  24289 | 6657 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  24289 | 6658 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  24289 | 6659 | `	sRep.pCtx = pCtx;` |
|  24289 | 6660 | `	sRep.pCollector = &sSearch;` |
|  24289 | 6661 | `	rep_str = 0;` |
|      - | 6662 | `	/* Extract the subject */` |
|  24289 | 6663 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  24289 | 6664 | `	if( nByte < 1 ){` |
|      - | 6665 | `		/* Nothing to replace,return the empty string */` |
|     29 | 6666 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 6667 | `		return PH7_OK;` |
|      - | 6668 | `	}` |
|      - | 6669 | `	/* Copy the subject */` |
|  24261 | 6670 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 6671 | `	/* Search string */` |
|  24261 | 6672 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 6673 | `		/* Collect search string */` |
|      9 | 6674 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 6675 | `	}else{` |
|      - | 6676 | `		/* Single pattern */` |
|  24253 | 6677 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  24253 | 6678 | `		if( nByte < 1 ){` |
|      - | 6679 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 6680 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 6681 | `			return PH7_OK;` |
|      - | 6682 | `		}` |
|  24249 | 6683 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6684 | `		/* Save for later processing */` |
|  24249 | 6685 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 6686 | `	}` |
|      - | 6687 | `	/* Replace string */` |
|  24257 | 6688 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 6689 | `		/* Collect replace string */` |
|      7 | 6690 | `		sRep.pCollector = &sReplace;` |
|      7 | 6691 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 6692 | `	}else{` |
|      - | 6693 | `		/* Single needle */` |
|  24251 | 6694 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  24251 | 6695 | `		rep_str = 1;` |
|  24251 | 6696 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 6697 | `		/* Save for later processing */` |
|  24251 | 6698 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 6699 | `	}` |
|      - | 6700 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  24257 | 6701 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 6702 | `		SySetRelease(&sSearch);` |
|    ! 0 | 6703 | `		SySetRelease(&sReplace);` |
|    ! 0 | 6704 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 6705 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6706 | `	}` |
|      - | 6707 | `	/* Reset loop cursors */` |
|  24257 | 6708 | `	SySetResetCursor(&sSearch);` |
|  24257 | 6709 | `	SySetResetCursor(&sReplace);` |
|  24257 | 6710 | `	pReplace = pSearch = 0; /* cc warning */` |
|  24257 | 6711 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 6712 | `	/* Extract function name */` |
|  24257 | 6713 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 6714 | `	/* Set the default pattern match routine */` |
|  24257 | 6715 | `	xMatch = SyBlobSearch;` |
|  24257 | 6716 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 6717 | `		/* Case insensitive pattern match */` |
|     11 | 6718 | `		xMatch = iPatternMatch;` |
|      5 | 6719 | `	}` |
|      - | 6720 | `	/* Start the replace process */` |
|  48517 | 6721 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 6722 | `		sxu32 nCount,nOfft;` |
|  24265 | 6723 | `		if( pSearch->nByte <  1 ){` |
|      - | 6724 | `			/* Empty string,ignore */` |
|      3 | 6725 | `			continue;` |
|      - | 6726 | `		}` |
|      - | 6727 | `		/* Extract the replace string */` |
|  24263 | 6728 | `		if( rep_str ){` |
|  24253 | 6729 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  12129 | 6730 | `		}else{` |
|     11 | 6731 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 6732 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 6733 | `				 * An empty string is used for the rest of replacement values` |
|      - | 6734 | `				 */` |
|      3 | 6735 | `				pReplace = 0;` |
|      1 | 6736 | `			}` |
|      - | 6737 | `		}` |
|  24263 | 6738 | `		if( pReplace == 0 ){` |
|      - | 6739 | `			/* Use an empty string instead */` |
|      3 | 6740 | `			pReplace = &sTemp;` |
|      1 | 6741 | `		}` |
|  24263 | 6742 | `		nOfft = nCount = 0;` |
|  12145 | 6743 | `		for(;;){` |
|  24295 | 6744 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 6745 | `				break;` |
|      - | 6746 | `			}` |
|      - | 6747 | `			/* Perform a pattern lookup */` |
|  36422 | 6748 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  24278 | 6749 | `				pSearch->nByte,&nOfft);` |
|  24283 | 6750 | `			if( rc != SXRET_OK ){` |
|      - | 6751 | `				/* Pattern not found */` |
|  24251 | 6752 | `				break;` |
|      - | 6753 | `			}` |
|      - | 6754 | `			/* Perform the replace operation */` |
|     33 | 6755 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 6756 | `			if( rc != SXRET_OK ){` |
|      - | 6757 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 6758 | `				SySetRelease(&sSearch);` |
|    ! 0 | 6759 | `				SySetRelease(&sReplace);` |
|    ! 0 | 6760 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 6761 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 6762 | `			}` |
|      - | 6763 | `			/* Increment offset counter */` |
|     33 | 6764 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 6765 | `		}` |
|      5 | 6766 | `	}` |
|      - | 6767 | `	/* All done,clean-up the mess left behind */` |
|  24257 | 6768 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  24257 | 6769 | `	SySetRelease(&sSearch);` |
|  24257 | 6770 | `	SySetRelease(&sReplace);` |
|  24257 | 6771 | `	SyBlobRelease(&sWorker);` |
|  24257 | 6772 | `	if( rc != PH7_OK ){` |
|    ! 0 | 6773 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6774 | `	}` |
|  24257 | 6775 | `	return PH7_OK;` |
|  12150 | 6776 | `}` |
|      - | 6777 | `/*` |
|      - | 6778 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 6779 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 6780 | ` *  Translate characters or replace substrings.` |
|      - | 6781 | ` * Parameters` |
|      - | 6782 | ` *  $str` |
|      - | 6783 | ` *  The string being translated.` |
|      - | 6784 | ` * $from` |
|      - | 6785 | ` *  The string being translated to to.` |
|      - | 6786 | ` * $to` |
|      - | 6787 | ` *  The string replacing from.` |
|      - | 6788 | ` * $replace_pairs` |
|      - | 6789 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 6790 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 6791 | ` * Return` |
|      - | 6792 | ` *  The translated string.` |
|      - | 6793 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 6794 | ` */` |
|     12 | 6795 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6796 | `{` |
|      - | 6797 | `	const char *zIn;` |
|      - | 6798 | `	int nLen;` |
|     13 | 6799 | `	if( nArg < 1 ){` |
|      - | 6800 | `		/* Nothing to replace,return FALSE */` |
|      7 | 6801 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6802 | `		return PH7_OK;` |
|      - | 6803 | `	}` |
|      7 | 6804 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6805 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6806 | `		/* Invalid arguments */` |
|    ! 0 | 6807 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6808 | `		return PH7_OK;` |
|      - | 6809 | `	}` |
|      9 | 6810 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6811 | `		str_replace_data sRepData;` |
|      - | 6812 | `		SyBlob sWorker;` |
|      - | 6813 | `		sxi32 rc;` |
|      - | 6814 | `		/* Initilaize the working buffer */` |
|      5 | 6815 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 6816 | `		/* Copy raw string */` |
|      5 | 6817 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 6818 | `		/* Init our replace data instance */` |
|      5 | 6819 | `		sRepData.pWorker = &sWorker;` |
|      5 | 6820 | `		sRepData.xMatch = SyBlobSearch;` |
|      5 | 6821 | `		sRepData.rc = SXRET_OK;` |
|      - | 6822 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 6823 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      5 | 6824 | `		if( sRepData.rc != SXRET_OK ){` |
|      - | 6825 | `			/* Allocation failure during replacement: surface a fatal */` |
|    ! 0 | 6826 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 6827 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6828 | `		}` |
|      - | 6829 | `		/* All done, return the result string */` |
|      7 | 6830 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 6831 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6832 | `		/* Clean-up */` |
|      5 | 6833 | `		SyBlobRelease(&sWorker);` |
|      5 | 6834 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6835 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6836 | `		}` |
|      3 | 6837 | `	}else{` |
|      - | 6838 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6839 | `		const char *zFrom,*zTo;` |
|      3 | 6840 | `		if( nArg < 3 ){` |
|      - | 6841 | `			/* Nothing to replace */` |
|    ! 0 | 6842 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6843 | `			return PH7_OK;` |
|      - | 6844 | `		}` |
|      - | 6845 | `		/* Extract given arguments */` |
|      3 | 6846 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 6847 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 6848 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6849 | `			/* Nothing to replace */` |
|    ! 0 | 6850 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6851 | `			return PH7_OK;` |
|      - | 6852 | `		}` |
|      - | 6853 | `		/* Start the replace process */` |
|     13 | 6854 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 6855 | `			c = zIn[i];` |
|     11 | 6856 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 6857 | `				if ( iOfft < tlen ){` |
|      5 | 6858 | `					c = zTo[iOfft];` |
|      2 | 6859 | `				}` |
|      2 | 6860 | `			}` |
|     11 | 6861 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6862 |  |
|      6 | 6863 | `		}` |
|      - | 6864 | `	}` |
|      7 | 6865 | `	return PH7_OK;` |
|      7 | 6866 | `}` |
|      - | 6867 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6868 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6869 | `/*` |
|      - | 6870 | ` * Parse an INI string.` |
|      - | 6871 |  |
|      - | 6872 | ` * According to wikipedia` |
|      - | 6873 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 6874 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 6875 | ` *  Format` |
|      - | 6876 | `*    Properties` |
|      - | 6877 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 6878 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 6879 | `*     Example:` |
|      - | 6880 | `*      name=value` |
|      - | 6881 | `*    Sections` |
|      - | 6882 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 6883 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 6884 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 6885 | `*     or the end of the file. Sections may not be nested.` |
|      - | 6886 | `*     Example:` |
|      - | 6887 | `*      [section]` |
|      - | 6888 | `*   Comments` |
|      - | 6889 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 6890 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 6891 | `*/` |
|     12 | 6892 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 6893 | `{` |
|      - | 6894 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 6895 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 6896 | `	SyHashEntry *pEntry;` |
|      - | 6897 | `	SyString sEntry;` |
|      - | 6898 | `	SyHash sHash;` |
|      - | 6899 | `	int c;` |
|      - | 6900 | `	/* Create an empty array and worker variables */` |
|     13 | 6901 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6902 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 6903 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6904 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 6905 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 6906 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6907 | `	}` |
|     13 | 6908 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 6909 | `	pCur = pArray;` |
|      - | 6910 | `	/* Start the parse process */` |
|     21 | 6911 | `	for(;;){` |
|      - | 6912 | `		/* Ignore leading white spaces */` |
|     69 | 6913 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 6914 | `			zIn++;` |
|      1 | 6915 | `		}` |
|     43 | 6916 | `		if( zIn >= zEnd ){` |
|      - | 6917 | `			/* No more input to process */` |
|     13 | 6918 | `			break;` |
|      - | 6919 | `		}` |
|     31 | 6920 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 6921 | `			/* Comment til the end of line */` |
|    ! 0 | 6922 | `			zIn++;` |
|    ! 0 | 6923 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 6924 | `				zIn++;` |
|    ! 0 | 6925 | `			}` |
|    ! 0 | 6926 | `			continue;` |
|      - | 6927 | `		}` |
|      - | 6928 | `		/* Reset the string cursor of the working variable */` |
|     31 | 6929 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 6930 | `		if( zIn[0] == '[' ){` |
|      - | 6931 | `			/* Section: Extract the section name */` |
|      9 | 6932 | `			zIn++;` |
|      9 | 6933 | `			zCur = zIn;` |
|     73 | 6934 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 6935 | `				zIn++;` |
|      1 | 6936 | `			}` |
|      9 | 6937 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 6938 | `				/* Save the section name */` |
|      5 | 6939 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 6940 | `				SyStringFullTrim(&sEntry);` |
|      5 | 6941 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 6942 | `				if( sEntry.nByte > 0 ){` |
|      - | 6943 | `					/* Associate an array with the section */` |
|      5 | 6944 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 6945 | `					if( pSection ){` |
|      5 | 6946 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 6947 | `						pCur = pSection;` |
|      2 | 6948 | `					}` |
|      2 | 6949 | `				}` |
|      2 | 6950 | `			}` |
|      9 | 6951 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 6952 | `		}else{` |
|      - | 6953 | `			ph7_value *pOldCur;` |
|      - | 6954 | `			int is_array;` |
|      - | 6955 | `			int iLen;` |
|      - | 6956 | `			/* Properties */` |
|     23 | 6957 | `			is_array = 0;` |
|     23 | 6958 | `			zCur = zIn;` |
|     23 | 6959 | `			iLen = 0; /* cc warning */` |
|     23 | 6960 | `			pOldCur = pCur;` |
|    155 | 6961 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 6962 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 6963 | `					/* Array */` |
|    ! 0 | 6964 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 6965 | `					is_array = 1;` |
|    ! 0 | 6966 | `					if( iLen > 0 ){` |
|    ! 0 | 6967 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 6968 | `						/* Query the hashtable */` |
|    ! 0 | 6969 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 6970 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 6971 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 6972 | `						if( pEntry ){` |
|    ! 0 | 6973 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 6974 | `						}else{` |
|      - | 6975 | `							/* Create an empty array */` |
|    ! 0 | 6976 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 6977 | `							if( pvArr ){` |
|      - | 6978 | `								/* Save the entry */` |
|    ! 0 | 6979 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 6980 | `								/* Insert the entry */` |
|    ! 0 | 6981 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6982 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 6983 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 6984 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 6985 | `							}` |
|      - | 6986 | `						}` |
|    ! 0 | 6987 | `						if( pvArr ){` |
|    ! 0 | 6988 | `							pCur = pvArr;` |
|    ! 0 | 6989 | `						}` |
|    ! 0 | 6990 | `					}` |
|    ! 0 | 6991 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 6992 | `						zIn++;` |
|    ! 0 | 6993 | `					}` |
|    ! 0 | 6994 | `				}` |
|    133 | 6995 | `				zIn++;` |
|      1 | 6996 | `			}` |
|     23 | 6997 | `			if( !is_array ){` |
|     23 | 6998 | `				iLen = (int)(zIn-zCur);` |
|     11 | 6999 | `			}` |
|      - | 7000 | `			/* Trim the key */` |
|     23 | 7001 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 7002 | `			SyStringFullTrim(&sEntry);` |
|     23 | 7003 | `			if( sEntry.nByte > 0 ){` |
|     23 | 7004 | `				if( !is_array ){` |
|      - | 7005 | `					/* Save the key name */` |
|     23 | 7006 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 7007 | `				}` |
|      - | 7008 | `				/* extract key value */` |
|     23 | 7009 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7010 | `				zIn++; /* '=' */` |
|     39 | 7011 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7012 | `					zIn++;` |
|      1 | 7013 | `				}` |
|     23 | 7014 | `				if( zIn < zEnd ){` |
|     21 | 7015 | `					zCur = zIn;` |
|     21 | 7016 | `					c = zIn[0];` |
|     21 | 7017 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7018 | `						zIn++;` |
|      - | 7019 | `						/* Delimit the value */` |
|    ! 0 | 7020 | `						while( zIn < zEnd ){` |
|    ! 0 | 7021 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7022 | `								break;` |
|      - | 7023 | `							}` |
|    ! 0 | 7024 | `							zIn++;` |
|    ! 0 | 7025 | `						}` |
|    ! 0 | 7026 | `						if( zIn < zEnd ){` |
|    ! 0 | 7027 | `							zIn++;` |
|    ! 0 | 7028 | `						}` |
|    ! 0 | 7029 | `					}else{` |
|    125 | 7030 | `						while( zIn < zEnd ){` |
|    123 | 7031 | `							if( zIn[0] == '\n' ){` |
|     19 | 7032 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7033 | `									break;` |
|    ! 0 | 7034 | `								}` |
|    105 | 7035 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7036 | `								/* Inline comments */` |
|    ! 0 | 7037 | `								break;` |
|      - | 7038 | `							}` |
|    105 | 7039 | `							zIn++;` |
|      1 | 7040 | `						}` |
|      - | 7041 | `					}` |
|      - | 7042 | `					/* Trim the value */` |
|     21 | 7043 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7044 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7045 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7046 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7047 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7048 | `					}` |
|     21 | 7049 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7050 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7051 | `					}` |
|      - | 7052 | `					/* Insert the key and it's value */` |
|     21 | 7053 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7054 | `				}` |
|     12 | 7055 | `			}else{` |
|    ! 0 | 7056 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7057 | `					zIn++;` |
|    ! 0 | 7058 | `				}` |
|      - | 7059 | `			}` |
|     23 | 7060 | `			pCur = pOldCur;` |
|      - | 7061 | `		}` |
|      1 | 7062 | `	}` |
|     13 | 7063 | `	SyHashRelease(&sHash);` |
|      - | 7064 | `	/* Return the parse of the INI string */` |
|     13 | 7065 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7066 | `	return SXRET_OK;` |
|      7 | 7067 | `}` |
|      - | 7068 | `/*` |
|      - | 7069 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7070 | ` *  Parse a configuration string.` |
|      - | 7071 | ` * Parameters` |
|      - | 7072 | ` *  $ini` |
|      - | 7073 | ` *   The contents of the ini file being parsed.` |
|      - | 7074 | ` *  $process_sections` |
|      - | 7075 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7076 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7077 | ` *  $scanner_mode (Not used)` |
|      - | 7078 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7079 | ` *   then option values will not be parsed.` |
|      - | 7080 | ` * Return` |
|      - | 7081 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7082 | ` */` |
|     10 | 7083 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7084 | `{` |
|      - | 7085 | `	const char *zIni;` |
|      - | 7086 | `	int nByte;` |
|     11 | 7087 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7088 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7089 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7090 | `		return PH7_OK;` |
|      - | 7091 | `	}` |
|      - | 7092 | `	/* Extract the raw INI buffer */` |
|     11 | 7093 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7094 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7095 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7096 | `}` |
|      - | 7097 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7098 |  |
|      - | 7099 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7100 |  |
|      - | 7101 | `/*` |
|      - | 7102 | ` * Ctype Functions.` |
|      - | 7103 | ` * Status:` |
|      - | 7104 | ` *    Stable.` |
|      - | 7105 | ` */` |
|      - | 7106 | `/*` |
|      - | 7107 | ` * bool ctype_alnum(string $text)` |
|      - | 7108 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7109 | ` * Parameters` |
|      - | 7110 | ` *  $text` |
|      - | 7111 | ` *   The tested string.` |
|      - | 7112 | ` * Return` |
|      - | 7113 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7114 | ` */` |
|     16 | 7115 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7116 | `{` |
|      - | 7117 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7118 | `	int nLen;` |
|     17 | 7119 | `	if( nArg < 1 ){` |
|      - | 7120 | `		/* Missing arguments,return FALSE */` |
|      3 | 7121 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7122 | `		return PH7_OK;` |
|      - | 7123 | `	}` |
|      - | 7124 | `	/* Extract the target string */` |
|     15 | 7125 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7126 | `	zEnd = &zIn[nLen];` |
|     15 | 7127 | `	if( nLen < 1 ){` |
|      - | 7128 | `		/* Empty string,return FALSE */` |
|      3 | 7129 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7130 | `		return PH7_OK;` |
|      - | 7131 | `	}` |
|      - | 7132 | `	/* Perform the requested operation */` |
|     32 | 7133 | `	for(;;){` |
|     65 | 7134 | `		if( zIn >= zEnd ){` |
|      - | 7135 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7136 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7137 | `			return PH7_OK;` |
|      - | 7138 | `		}` |
|     57 | 7139 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7140 | `			break;` |
|      - | 7141 | `		}` |
|      - | 7142 | `		/* Point to the next character */` |
|     53 | 7143 | `		zIn++;` |
|      1 | 7144 | `	}` |
|      - | 7145 | `	/* The test failed,return FALSE */` |
|      5 | 7146 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7147 | `	return PH7_OK;` |
|      9 | 7148 | `}` |
|      - | 7149 | `/*` |
|      - | 7150 | ` * bool ctype_alpha(string $text)` |
|      - | 7151 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7152 | ` * Parameters` |
|      - | 7153 | ` *  $text` |
|      - | 7154 | ` *   The tested string.` |
|      - | 7155 | ` * Return` |
|      - | 7156 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7157 | ` */` |
|     18 | 7158 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7159 | `{` |
|      - | 7160 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7161 | `	int nLen;` |
|     19 | 7162 | `	if( nArg < 1 ){` |
|      - | 7163 | `		/* Missing arguments,return FALSE */` |
|      3 | 7164 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7165 | `		return PH7_OK;` |
|      - | 7166 | `	}` |
|      - | 7167 | `	/* Extract the target string */` |
|     17 | 7168 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7169 | `	zEnd = &zIn[nLen];` |
|     17 | 7170 | `	if( nLen < 1 ){` |
|      - | 7171 | `		/* Empty string,return FALSE */` |
|      3 | 7172 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7173 | `		return PH7_OK;` |
|      - | 7174 | `	}` |
|      - | 7175 | `	/* Perform the requested operation */` |
|     42 | 7176 | `	for(;;){` |
|     85 | 7177 | `		if( zIn >= zEnd ){` |
|      - | 7178 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7179 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7180 | `			return PH7_OK;` |
|      - | 7181 | `		}` |
|     77 | 7182 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7183 | `			break;` |
|      - | 7184 | `		}` |
|      - | 7185 | `		/* Point to the next character */` |
|     71 | 7186 | `		zIn++;` |
|      1 | 7187 | `	}` |
|      - | 7188 | `	/* The test failed,return FALSE */` |
|      7 | 7189 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7190 | `	return PH7_OK;` |
|     10 | 7191 | `}` |
|      - | 7192 | `/*` |
|      - | 7193 | ` * bool ctype_cntrl(string $text)` |
|      - | 7194 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7195 | ` * Parameters` |
|      - | 7196 | ` *  $text` |
|      - | 7197 | ` *   The tested string.` |
|      - | 7198 | ` * Return` |
|      - | 7199 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7200 | ` */` |
|     18 | 7201 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7202 | `{` |
|      - | 7203 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7204 | `	int nLen;` |
|     19 | 7205 | `	if( nArg < 1 ){` |
|      - | 7206 | `		/* Missing arguments,return FALSE */` |
|      3 | 7207 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7208 | `		return PH7_OK;` |
|      - | 7209 | `	}` |
|      - | 7210 | `	/* Extract the target string */` |
|     17 | 7211 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7212 | `	zEnd = &zIn[nLen];` |
|     17 | 7213 | `	if( nLen < 1 ){` |
|      - | 7214 | `		/* Empty string,return FALSE */` |
|      3 | 7215 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7216 | `		return PH7_OK;` |
|      - | 7217 | `	}` |
|      - | 7218 | `	/* Perform the requested operation */` |
|     14 | 7219 | `	for(;;){` |
|     29 | 7220 | `		if( zIn >= zEnd ){` |
|      - | 7221 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7222 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7223 | `			return PH7_OK;` |
|      - | 7224 | `		}` |
|     21 | 7225 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7226 | `			/* UTF-8 stream  */` |
|    ! 0 | 7227 | `			break;` |
|      - | 7228 | `		}` |
|     21 | 7229 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7230 | `			break;` |
|      - | 7231 | `		}` |
|      - | 7232 | `		/* Point to the next character */` |
|     15 | 7233 | `		zIn++;` |
|      1 | 7234 | `	}` |
|      - | 7235 | `	/* The test failed,return FALSE */` |
|      7 | 7236 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7237 | `	return PH7_OK;` |
|     10 | 7238 | `}` |
|      - | 7239 | `/*` |
|      - | 7240 | ` * bool ctype_digit(string $text)` |
|      - | 7241 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7242 | ` * Parameters` |
|      - | 7243 | ` *  $text` |
|      - | 7244 | ` *   The tested string.` |
|      - | 7245 | ` * Return` |
|      - | 7246 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7247 | ` */` |
|   1639 | 7248 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7249 | `{` |
|      - | 7250 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7251 | `	int nLen;` |
|   1644 | 7252 | `	if( nArg < 1 ){` |
|      - | 7253 | `		/* Missing arguments,return FALSE */` |
|      3 | 7254 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7255 | `		return PH7_OK;` |
|      - | 7256 | `	}` |
|      - | 7257 | `	/* Extract the target string */` |
|   1642 | 7258 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1642 | 7259 | `	zEnd = &zIn[nLen];` |
|   1642 | 7260 | `	if( nLen < 1 ){` |
|      - | 7261 | `		/* Empty string,return FALSE */` |
|      3 | 7262 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7263 | `		return PH7_OK;` |
|      - | 7264 | `	}` |
|      - | 7265 | `	/* Perform the requested operation */` |
|   1540 | 7266 | `	for(;;){` |
|   3083 | 7267 | `		if( zIn >= zEnd ){` |
|      - | 7268 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1398 | 7269 | `			ph7_result_bool(pCtx,1);` |
|   1398 | 7270 | `			return PH7_OK;` |
|      - | 7271 | `		}` |
|   1690 | 7272 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7273 | `			/* UTF-8 stream  */` |
|    ! 0 | 7274 | `			break;` |
|      - | 7275 | `		}` |
|   1690 | 7276 | `		if( !SyisDigit(zIn[0]) ){` |
|    247 | 7277 | `			break;` |
|      - | 7278 | `		}` |
|      - | 7279 | `		/* Point to the next character */` |
|   1448 | 7280 | `		zIn++;` |
|      5 | 7281 | `	}` |
|      - | 7282 | `	/* The test failed,return FALSE */` |
|    247 | 7283 | `	ph7_result_bool(pCtx,0);` |
|    247 | 7284 | `	return PH7_OK;` |
|    825 | 7285 | `}` |
|      - | 7286 | `/*` |
|      - | 7287 | ` * bool ctype_xdigit(string $text)` |
|      - | 7288 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7289 | ` * Parameters` |
|      - | 7290 | ` *  $text` |
|      - | 7291 | ` *   The tested string.` |
|      - | 7292 | ` * Return` |
|      - | 7293 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7294 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7295 | ` */` |
|     20 | 7296 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7297 | `{` |
|      - | 7298 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7299 | `	int nLen;` |
|     21 | 7300 | `	if( nArg < 1 ){` |
|      - | 7301 | `		/* Missing arguments,return FALSE */` |
|      3 | 7302 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7303 | `		return PH7_OK;` |
|      - | 7304 | `	}` |
|      - | 7305 | `	/* Extract the target string */` |
|     19 | 7306 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7307 | `	zEnd = &zIn[nLen];` |
|     19 | 7308 | `	if( nLen < 1 ){` |
|      - | 7309 | `		/* Empty string,return FALSE */` |
|      3 | 7310 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7311 | `		return PH7_OK;` |
|      - | 7312 | `	}` |
|      - | 7313 | `	/* Perform the requested operation */` |
|     46 | 7314 | `	for(;;){` |
|     93 | 7315 | `		if( zIn >= zEnd ){` |
|      - | 7316 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7317 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7318 | `			return PH7_OK;` |
|      - | 7319 | `		}` |
|     83 | 7320 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7321 | `			/* UTF-8 stream  */` |
|    ! 0 | 7322 | `			break;` |
|      - | 7323 | `		}` |
|     83 | 7324 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7325 | `			break;` |
|      - | 7326 | `		}` |
|      - | 7327 | `		/* Point to the next character */` |
|     77 | 7328 | `		zIn++;` |
|      1 | 7329 | `	}` |
|      - | 7330 | `	/* The test failed,return FALSE */` |
|      7 | 7331 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7332 | `	return PH7_OK;` |
|     11 | 7333 | `}` |
|      - | 7334 | `/*` |
|      - | 7335 | ` * bool ctype_graph(string $text)` |
|      - | 7336 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7337 | ` * Parameters` |
|      - | 7338 | ` *  $text` |
|      - | 7339 | ` *   The tested string.` |
|      - | 7340 | ` * Return` |
|      - | 7341 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7342 | ` * (no white space), FALSE otherwise.` |
|      - | 7343 | ` */` |
|     18 | 7344 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7345 | `{` |
|      - | 7346 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7347 | `	int nLen;` |
|     19 | 7348 | `	if( nArg < 1 ){` |
|      - | 7349 | `		/* Missing arguments,return FALSE */` |
|      3 | 7350 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7351 | `		return PH7_OK;` |
|      - | 7352 | `	}` |
|      - | 7353 | `	/* Extract the target string */` |
|     17 | 7354 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7355 | `	zEnd = &zIn[nLen];` |
|     17 | 7356 | `	if( nLen < 1 ){` |
|      - | 7357 | `		/* Empty string,return FALSE */` |
|      3 | 7358 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7359 | `		return PH7_OK;` |
|      - | 7360 | `	}` |
|      - | 7361 | `	/* Perform the requested operation */` |
|     57 | 7362 | `	for(;;){` |
|    115 | 7363 | `		if( zIn >= zEnd ){` |
|      - | 7364 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7365 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7366 | `			return PH7_OK;` |
|      - | 7367 | `		}` |
|    107 | 7368 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7369 | `			/* UTF-8 stream  */` |
|    ! 0 | 7370 | `			break;` |
|      - | 7371 | `		}` |
|    107 | 7372 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7373 | `			break;` |
|      - | 7374 | `		}` |
|      - | 7375 | `		/* Point to the next character */` |
|    101 | 7376 | `		zIn++;` |
|      1 | 7377 | `	}` |
|      - | 7378 | `	/* The test failed,return FALSE */` |
|      7 | 7379 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7380 | `	return PH7_OK;` |
|     10 | 7381 | `}` |
|      - | 7382 | `/*` |
|      - | 7383 | ` * bool ctype_print(string $text)` |
|      - | 7384 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7385 | ` * Parameters` |
|      - | 7386 | ` *  $text` |
|      - | 7387 | ` *   The tested string.` |
|      - | 7388 | ` * Return` |
|      - | 7389 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7390 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7391 | ` *  or control function at all.` |
|      - | 7392 | ` */` |
|     18 | 7393 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7394 | `{` |
|      - | 7395 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7396 | `	int nLen;` |
|     19 | 7397 | `	if( nArg < 1 ){` |
|      - | 7398 | `		/* Missing arguments,return FALSE */` |
|      3 | 7399 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7400 | `		return PH7_OK;` |
|      - | 7401 | `	}` |
|      - | 7402 | `	/* Extract the target string */` |
|     17 | 7403 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7404 | `	zEnd = &zIn[nLen];` |
|     17 | 7405 | `	if( nLen < 1 ){` |
|      - | 7406 | `		/* Empty string,return FALSE */` |
|      3 | 7407 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7408 | `		return PH7_OK;` |
|      - | 7409 | `	}` |
|      - | 7410 | `	/* Perform the requested operation */` |
|     63 | 7411 | `	for(;;){` |
|    127 | 7412 | `		if( zIn >= zEnd ){` |
|      - | 7413 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7414 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7415 | `			return PH7_OK;` |
|      - | 7416 | `		}` |
|    119 | 7417 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7418 | `			/* UTF-8 stream  */` |
|    ! 0 | 7419 | `			break;` |
|      - | 7420 | `		}` |
|    119 | 7421 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7422 | `			break;` |
|      - | 7423 | `		}` |
|      - | 7424 | `		/* Point to the next character */` |
|    113 | 7425 | `		zIn++;` |
|      1 | 7426 | `	}` |
|      - | 7427 | `	/* The test failed,return FALSE */` |
|      7 | 7428 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7429 | `	return PH7_OK;` |
|     10 | 7430 | `}` |
|      - | 7431 | `/*` |
|      - | 7432 | ` * bool ctype_punct(string $text)` |
|      - | 7433 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7434 | ` * Parameters` |
|      - | 7435 | ` *  $text` |
|      - | 7436 | ` *   The tested string.` |
|      - | 7437 | ` * Return` |
|      - | 7438 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7439 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7440 | ` */` |
|     20 | 7441 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7442 | `{` |
|      - | 7443 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7444 | `	int nLen;` |
|     21 | 7445 | `	if( nArg < 1 ){` |
|      - | 7446 | `		/* Missing arguments,return FALSE */` |
|      3 | 7447 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7448 | `		return PH7_OK;` |
|      - | 7449 | `	}` |
|      - | 7450 | `	/* Extract the target string */` |
|     19 | 7451 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7452 | `	zEnd = &zIn[nLen];` |
|     19 | 7453 | `	if( nLen < 1 ){` |
|      - | 7454 | `		/* Empty string,return FALSE */` |
|      3 | 7455 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7456 | `		return PH7_OK;` |
|      - | 7457 | `	}` |
|      - | 7458 | `	/* Perform the requested operation */` |
|     38 | 7459 | `	for(;;){` |
|     77 | 7460 | `		if( zIn >= zEnd ){` |
|      - | 7461 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7462 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7463 | `			return PH7_OK;` |
|      - | 7464 | `		}` |
|     69 | 7465 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7466 | `			/* UTF-8 stream  */` |
|    ! 0 | 7467 | `			break;` |
|      - | 7468 | `		}` |
|     69 | 7469 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7470 | `			break;` |
|      - | 7471 | `		}` |
|      - | 7472 | `		/* Point to the next character */` |
|     61 | 7473 | `		zIn++;` |
|      1 | 7474 | `	}` |
|      - | 7475 | `	/* The test failed,return FALSE */` |
|      9 | 7476 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7477 | `	return PH7_OK;` |
|     11 | 7478 | `}` |
|      - | 7479 | `/*` |
|      - | 7480 | ` * bool ctype_space(string $text)` |
|      - | 7481 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7482 | ` * Parameters` |
|      - | 7483 | ` *  $text` |
|      - | 7484 | ` *   The tested string.` |
|      - | 7485 | ` * Return` |
|      - | 7486 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7487 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7488 | ` *  and form feed characters.` |
|      - | 7489 | ` */` |
|  62046 | 7490 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7491 | `{` |
|      - | 7492 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7493 | `	int nLen;` |
|  62051 | 7494 | `	if( nArg < 1 ){` |
|      - | 7495 | `		/* Missing arguments,return FALSE */` |
|      3 | 7496 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7497 | `		return PH7_OK;` |
|      - | 7498 | `	}` |
|      - | 7499 | `	/* Extract the target string */` |
|  62049 | 7500 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  62049 | 7501 | `	zEnd = &zIn[nLen];` |
|  62049 | 7502 | `	if( nLen < 1 ){` |
|      - | 7503 | `		/* Empty string,return FALSE */` |
|      3 | 7504 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7505 | `		return PH7_OK;` |
|      - | 7506 | `	}` |
|      - | 7507 | `	/* Perform the requested operation */` |
|  32129 | 7508 | `	for(;;){` |
|  64177 | 7509 | `		if( zIn >= zEnd ){` |
|      - | 7510 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2111 | 7511 | `			ph7_result_bool(pCtx,1);` |
|   2111 | 7512 | `			return PH7_OK;` |
|      - | 7513 | `		}` |
|  62071 | 7514 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7515 | `			/* UTF-8 stream  */` |
|    ! 0 | 7516 | `			break;` |
|      - | 7517 | `		}` |
|  62071 | 7518 | `		if( !SyisSpace(zIn[0]) ){` |
|  59941 | 7519 | `			break;` |
|      - | 7520 | `		}` |
|      - | 7521 | `		/* Point to the next character */` |
|   2135 | 7522 | `		zIn++;` |
|      5 | 7523 | `	}` |
|      - | 7524 | `	/* The test failed,return FALSE */` |
|  59941 | 7525 | `	ph7_result_bool(pCtx,0);` |
|  59941 | 7526 | `	return PH7_OK;` |
|  31071 | 7527 | `}` |
|      - | 7528 | `/*` |
|      - | 7529 | ` * bool ctype_lower(string $text)` |
|      - | 7530 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7531 | ` * Parameters` |
|      - | 7532 | ` *  $text` |
|      - | 7533 | ` *   The tested string.` |
|      - | 7534 | ` * Return` |
|      - | 7535 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7536 | ` */` |
|     18 | 7537 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7538 | `{` |
|      - | 7539 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7540 | `	int nLen;` |
|     19 | 7541 | `	if( nArg < 1 ){` |
|      - | 7542 | `		/* Missing arguments,return FALSE */` |
|      3 | 7543 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7544 | `		return PH7_OK;` |
|      - | 7545 | `	}` |
|      - | 7546 | `	/* Extract the target string */` |
|     17 | 7547 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7548 | `	zEnd = &zIn[nLen];` |
|     17 | 7549 | `	if( nLen < 1 ){` |
|      - | 7550 | `		/* Empty string,return FALSE */` |
|      3 | 7551 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7552 | `		return PH7_OK;` |
|      - | 7553 | `	}` |
|      - | 7554 | `	/* Perform the requested operation */` |
|     27 | 7555 | `	for(;;){` |
|     55 | 7556 | `		if( zIn >= zEnd ){` |
|      - | 7557 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7558 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7559 | `			return PH7_OK;` |
|      - | 7560 | `		}` |
|     51 | 7561 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 7562 | `			break;` |
|      - | 7563 | `		}` |
|      - | 7564 | `		/* Point to the next character */` |
|     41 | 7565 | `		zIn++;` |
|      1 | 7566 | `	}` |
|      - | 7567 | `	/* The test failed,return FALSE */` |
|     11 | 7568 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7569 | `	return PH7_OK;` |
|     10 | 7570 | `}` |
|      - | 7571 | `/*` |
|      - | 7572 | ` * bool ctype_upper(string $text)` |
|      - | 7573 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 7574 | ` * Parameters` |
|      - | 7575 | ` *  $text` |
|      - | 7576 | ` *   The tested string.` |
|      - | 7577 | ` * Return` |
|      - | 7578 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 7579 | ` */` |
|     18 | 7580 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7581 | `{` |
|      - | 7582 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7583 | `	int nLen;` |
|     19 | 7584 | `	if( nArg < 1 ){` |
|      - | 7585 | `		/* Missing arguments,return FALSE */` |
|      3 | 7586 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7587 | `		return PH7_OK;` |
|      - | 7588 | `	}` |
|      - | 7589 | `	/* Extract the target string */` |
|     17 | 7590 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7591 | `	zEnd = &zIn[nLen];` |
|     17 | 7592 | `	if( nLen < 1 ){` |
|      - | 7593 | `		/* Empty string,return FALSE */` |
|      3 | 7594 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7595 | `		return PH7_OK;` |
|      - | 7596 | `	}` |
|      - | 7597 | `	/* Perform the requested operation */` |
|     28 | 7598 | `	for(;;){` |
|     57 | 7599 | `		if( zIn >= zEnd ){` |
|      - | 7600 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 7601 | `			ph7_result_bool(pCtx,1);` |
|      5 | 7602 | `			return PH7_OK;` |
|      - | 7603 | `		}` |
|     53 | 7604 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 7605 | `			break;` |
|      - | 7606 | `		}` |
|      - | 7607 | `		/* Point to the next character */` |
|     43 | 7608 | `		zIn++;` |
|      1 | 7609 | `	}` |
|      - | 7610 | `	/* The test failed,return FALSE */` |
|     11 | 7611 | `	ph7_result_bool(pCtx,0);` |
|     11 | 7612 | `	return PH7_OK;` |
|     10 | 7613 | `}` |
|      - | 7614 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 7615 | `/*` |
|      - | 7616 | ` * Section:` |
|      - | 7617 | ` *    URL handling Functions.` |
|      - | 7618 | ` * Status:` |
|      - | 7619 | ` *    Stable.` |
|      - | 7620 | ` */` |
|      - | 7621 | `/*` |
|      - | 7622 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 7623 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 7624 | ` */` |
|   1026 | 7625 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7626 | `{` |
|      - | 7627 | `	/* Store in the call context result buffer */` |
|   1028 | 7628 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 7629 | `	return SXRET_OK;` |
|      2 | 7630 | `}` |
|      - | 7631 | `/*` |
|      - | 7632 | ` * string base64_encode(string $data)` |
|      - | 7633 | ` * string convert_uuencode(string $data)` |
|      - | 7634 | ` *  Encodes data with MIME base64` |
|      - | 7635 | ` * Parameter` |
|      - | 7636 | ` *  $data` |
|      - | 7637 | ` *    Data to encode` |
|      - | 7638 | ` * Return` |
|      - | 7639 | ` *  Encoded data or FALSE on failure.` |
|      - | 7640 | ` */` |
|     10 | 7641 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7642 | `{` |
|      - | 7643 | `	const char *zIn;` |
|      - | 7644 | `	int nLen;` |
|     11 | 7645 | `	if( nArg < 1 ){` |
|      - | 7646 | `		/* Missing arguments,return FALSE */` |
|      5 | 7647 | `		ph7_result_bool(pCtx,0);` |
|      5 | 7648 | `		return PH7_OK;` |
|      - | 7649 | `	}` |
|      - | 7650 | `	/* Extract the input string */` |
|      7 | 7651 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7652 | `	if( nLen < 1 ){` |
|      - | 7653 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7654 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7655 | `		return PH7_OK;` |
|      - | 7656 | `	}` |
|      - | 7657 | `	/* Perform the BASE64 encoding */` |
|      7 | 7658 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 7659 | `	return PH7_OK;` |
|      6 | 7660 | `}` |
|      - | 7661 | `/*` |
|      - | 7662 | ` * string base64_decode(string $data)` |
|      - | 7663 | ` * string convert_uudecode(string $data)` |
|      - | 7664 | ` *  Decodes data encoded with MIME base64` |
|      - | 7665 | ` * Parameter` |
|      - | 7666 | ` *  $data` |
|      - | 7667 | ` *    Encoded data.` |
|      - | 7668 | ` * Return` |
|      - | 7669 | ` *  Returns the original data or FALSE on failure.` |
|      - | 7670 | ` */` |
|     36 | 7671 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7672 | `{` |
|      - | 7673 | `	const char *zIn;` |
|      - | 7674 | `	int nLen;` |
|     38 | 7675 | `	if( nArg < 1 ){` |
|      - | 7676 | `		/* Missing arguments,return FALSE */` |
|      3 | 7677 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7678 | `		return PH7_OK;` |
|      - | 7679 | `	}` |
|      - | 7680 | `	/* Extract the input string */` |
|     36 | 7681 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 7682 | `	if( nLen < 1 ){` |
|      - | 7683 | `		/* Nothing to process,return FALSE */` |
|      3 | 7684 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7685 | `		return PH7_OK;` |
|      - | 7686 | `	}` |
|      - | 7687 | `	/* Perform the BASE64 decoding */` |
|     34 | 7688 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 7689 | `	return PH7_OK;` |
|     20 | 7690 | `}` |
|      - | 7691 | `/*` |
|      - | 7692 | ` * string urlencode(string $str)` |
|      - | 7693 | ` *  URL encoding` |
|      - | 7694 | ` * Parameter` |
|      - | 7695 | ` *  $data` |
|      - | 7696 | ` *   Input string.` |
|      - | 7697 | ` * Return` |
|      - | 7698 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 7699 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 7700 | ` *  encoded as plus (+) signs.` |
|      - | 7701 | ` */` |
|      6 | 7702 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7703 | `{` |
|      - | 7704 | `	const char *zIn;` |
|      - | 7705 | `	int nLen;` |
|      7 | 7706 | `	if( nArg < 1 ){` |
|      - | 7707 | `		/* Missing arguments,return FALSE */` |
|      3 | 7708 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7709 | `		return PH7_OK;` |
|      - | 7710 | `	}` |
|      - | 7711 | `	/* Extract the input string */` |
|      5 | 7712 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 7713 | `	if( nLen < 1 ){` |
|      - | 7714 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7715 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7716 | `		return PH7_OK;` |
|      - | 7717 | `	}` |
|      - | 7718 | `	/* Perform the URL encoding */` |
|      5 | 7719 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 7720 | `	return PH7_OK;` |
|      4 | 7721 | `}` |
|      - | 7722 | `/*` |
|      - | 7723 | ` * string urldecode(string $str)` |
|      - | 7724 | ` *  Decodes any %## encoding in the given string.` |
|      - | 7725 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 7726 | ` * Parameter` |
|      - | 7727 | ` *  $data` |
|      - | 7728 | ` *    Input string.` |
|      - | 7729 | ` * Return` |
|      - | 7730 | ` *  Decoded URL or FALSE on failure.` |
|      - | 7731 | ` */` |
|      8 | 7732 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7733 | `{` |
|      - | 7734 | `	const char *zIn;` |
|      - | 7735 | `	int nLen;` |
|      9 | 7736 | `	if( nArg < 1 ){` |
|      - | 7737 | `		/* Missing arguments,return FALSE */` |
|      3 | 7738 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7739 | `		return PH7_OK;` |
|      - | 7740 | `	}` |
|      - | 7741 | `	/* Extract the input string */` |
|      7 | 7742 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 7743 | `	if( nLen < 1 ){` |
|      - | 7744 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7745 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7746 | `		return PH7_OK;` |
|      - | 7747 | `	}` |
|      - | 7748 | `	/* Perform the URL decoding */` |
|      7 | 7749 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 7750 | `	return PH7_OK;` |
|      5 | 7751 | `}` |
|      - | 7752 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7753 | `/* Table of the built-in functions */` |
|      - | 7754 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 7755 | `	   /* Variable handling functions */` |
|      - | 7756 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 7757 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 7758 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 7759 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 7760 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 7761 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 7762 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 7763 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 7764 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 7765 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 7766 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 7767 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 7768 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 7769 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 7770 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 7771 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 7772 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 7773 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 7774 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 7775 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 7776 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7777 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 7778 | `	   /* Math functions */` |
|      - | 7779 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 7780 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 7781 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 7782 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 7783 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 7784 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 7785 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 7786 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 7787 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 7788 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 7789 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 7790 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 7791 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 7792 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 7793 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 7794 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 7795 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 7796 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 7797 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 7798 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 7799 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 7800 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 7801 | `	{ "round",    PH7_builtin_round        },` |
|      - | 7802 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 7803 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 7804 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 7805 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 7806 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 7807 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 7808 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 7809 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 7810 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 7811 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7812 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7813 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 7814 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7815 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7816 | `	   /* String handling functions */` |
|      - | 7817 |  |
|      - | 7818 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 7819 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 7820 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 7821 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 7822 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 7823 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 7824 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 7825 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 7826 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 7827 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 7828 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 7829 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 7830 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 7831 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 7832 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 7833 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 7834 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 7835 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 7836 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 7837 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 7838 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 7839 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 7840 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 7841 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 7842 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 7843 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 7844 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 7845 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 7846 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 7847 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 7848 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 7849 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 7850 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 7851 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 7852 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 7853 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 7854 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 7855 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 7856 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 7857 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 7858 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 7859 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 7860 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 7861 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 7862 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 7863 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 7864 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 7865 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 7866 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 7867 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 7868 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 7869 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 7870 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7871 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7872 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 7873 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 7874 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 7875 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 7876 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7877 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7878 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 7879 |  |
|      - | 7880 |  |
|      - | 7881 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 7882 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 7883 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 7884 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 7885 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 7886 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 7887 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 7888 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 7889 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7890 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 7891 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 7892 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 7893 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 7894 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 7895 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7896 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7897 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 7898 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 7899 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7900 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7901 |  |
|      - | 7902 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 7903 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 7904 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 7905 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 7906 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 7907 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 7908 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 7909 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 7910 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 7911 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 7912 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 7913 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 7914 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7915 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7916 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 7917 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7918 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7919 |  |
|      - | 7920 | `	         /* Ctype functions */` |
|      - | 7921 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 7922 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 7923 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 7924 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 7925 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 7926 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 7927 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 7928 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 7929 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 7930 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 7931 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 7932 | `	         /* Time functions */` |
|      - | 7933 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 7934 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 7935 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 7936 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 7937 | `	{ "date",        PH7_builtin_date         },` |
|      - | 7938 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 7939 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 7940 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 7941 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 7942 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 7943 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 7944 | `	        /* URL functions */` |
|      - | 7945 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 7946 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 7947 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 7948 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 7949 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 7950 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 7951 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 7952 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 7953 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7954 | `};` |
|      - | 7955 | `/*` |
|      - | 7956 | ` * Register the built-in functions defined above,the array functions` |
|      - | 7957 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 7958 | ` */` |
|   3268 | 7959 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 7960 | `{` |
|      - | 7961 | `	sxu32 n;` |
| 545761 | 7962 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 542493 | 7963 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 271249 | 7964 | `	}` |
|      - | 7965 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3273 | 7966 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 7967 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3273 | 7968 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3273 | 7969 | `}` |
|      - | 7970 |  |
